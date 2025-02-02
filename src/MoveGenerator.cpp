#include "MoveGenerator.h"

#include "MoveLookup.h"
#include "StringTools.h"
#include "threadsafe_io.h"
#include "Castling.h"

/**
 * @brief takes start_pos and the first set bit of move_destinations, and adds each of the 4 promotion moves to the movelist
 */
Move* AddPromotions(Bitboard move_destinations, Square start_pos, Move* movelist_next){
    Bitloop(move_destinations){
        Square destination = BitboardUtils::FindLSB(move_destinations);
        for(Piece promo : {PieceUtils::ROOK, PieceUtils::QUEEN, PieceUtils::BISHOP, PieceUtils::KNIGHT}){
            *movelist_next++ = MoveUtils::MakeMove(start_pos, destination, promo);
        }
    }
    return movelist_next;
}
/**
 * @brief takes start_pos and each bit individually of move_destinations and adds that move to the movelist
 */
inline Move* AddFromBB(Bitboard move_destinations, Square start_pos, Move* movelist_next){
    //this loop may be badly predicted
    Bitloop(move_destinations){
        *movelist_next++ = MoveUtils::MakeMove(start_pos, BitboardUtils::FindLSB(move_destinations));
    }

    return movelist_next;
}


template<Piece base_piece_type>
Move* GenerateSliderMoves(Bitboard pieces, Bitboard blockers, Bitboard valid_end_locations, Bitboard hv_pins, Bitboard diag_pins, Move* move_list){
    static_assert(
        base_piece_type == PieceUtils::ROOK ||
        base_piece_type == PieceUtils::BISHOP);

    Bitboard in_line_pinmask;//for rooks, this would be the horisontal and vertical pins, vice-versa for bishops
    Bitboard out_of_line_pinmask;//for rooks, this would be the diagonal pins, vice-versa for bishops
    if constexpr(base_piece_type == PieceUtils::ROOK){
        in_line_pinmask = hv_pins;
        out_of_line_pinmask = diag_pins;
    } else{
        in_line_pinmask = diag_pins;
        out_of_line_pinmask = hv_pins;
    }

    pieces &= ~out_of_line_pinmask;//remove pieces locked with no pieces i.e rook pinned diagonally

    Bitboard free_pieces = pieces & ~in_line_pinmask;//bitboard of the sliders that are free to move
    Bitboard pinned_pieces = pieces & in_line_pinmask;//bitboard of the sliders that can only slide along one line

    Bitloop(free_pieces){
        Square current_sq = BitboardUtils::FindLSB(free_pieces);
        Bitboard end_locations = MoveLookup::SliderLookup<base_piece_type>(current_sq, blockers) & valid_end_locations;

        move_list = AddFromBB(end_locations, current_sq, move_list);
    }
    Bitloop(pinned_pieces){
        Square current_sq = BitboardUtils::FindLSB(pinned_pieces);
        Bitboard end_locations = MoveLookup::SliderLookup<base_piece_type>(current_sq, blockers) & valid_end_locations & in_line_pinmask;

        move_list = AddFromBB(end_locations, current_sq, move_list);
    }

    return move_list;
}

Move* GenerateKnightMoves(Bitboard pieces, Bitboard valid_end_locations, Bitboard all_pins, Move* move_list){

    pieces &= ~all_pins;//knights can't be pinned at all, otherwise they are completely stuck

    Bitloop(pieces){
        Square current_sq = BitboardUtils::FindLSB(pieces);

        Bitboard end_locations = MoveLookup::KnightLookup(current_sq) & valid_end_locations;
        move_list = AddFromBB(end_locations, current_sq, move_list);
    }

    return move_list;
}

template<bool turn, bool has_enpessant>
Move* GeneratePawnMoves(Bitboard pawns, Bitboard blockers, Bitboard valid_end_locations, Bitboard capturable_pieces, Square enpessant, Bitboard hv_pinmask, Bitboard diag_pinmask,Square king_sq, Bitboard enemy_orthogonals, Move* move_list){
    Bitboard blockers_and_shadows = blockers | (turn ? blockers << 8 : blockers >> 8);//1 square behind the blockers, to prevent douple pushes from jumping over pieces

    Bitloop(pawns){
        Square current_sq = BitboardUtils::FindLSB(pawns);
        Bitboard current_pawn = BitboardUtils::MakeBitBoard(current_sq);

        Bitboard forward_slide = (BitboardUtils::Forwards<turn,1>(current_pawn) & ~blockers) |
                (BitboardUtils::Forwards<turn,2>(current_pawn&BitboardUtils::DOUBLE_PUSH) & ~blockers_and_shadows);

        Bitboard captures = BitboardUtils::GeneratePawnSetAttacks<turn>(current_pawn) & capturable_pieces;

        Bitboard enpessant_capture = 0;
        if constexpr(has_enpessant){
            Bitboard enpessant_bb = BitboardUtils::MakeBitBoard(enpessant);
            Bitboard enpessant_dead_position = BitboardUtils::Forwards<!turn, 1>(enpessant_bb);
            Bitboard blockers_after_enpessant = (blockers & ~(enpessant_dead_position|current_pawn)) | enpessant_bb;//remove from square and killed piece, add to square

            bool didnt_enpessant_checking_pawn = enpessant_dead_position&~valid_end_locations;//not valid end locations means that there is a check mask or ray that I am not dealing with
            bool rook_check_when_enpessant = MoveLookup::SliderLookup<PieceUtils::ROOK>(king_sq, blockers_after_enpessant) & enemy_orthogonals;//if removing the pawns leaves me in check(d6 in 1k6/8/8/K2pP2r/8/8/8/8 w - - 0 1)
            bool end_sq_is_valid = enpessant_bb&valid_end_locations;
            bool in_check_after_enpessant = 
                (didnt_enpessant_checking_pawn&&!end_sq_is_valid) ||//didn't kill enpessanting pawn or block the check
                rook_check_when_enpessant;//or moving me and killing ep would expose me to a rook

            
            if(in_check_after_enpessant){
                enpessant_capture = 0;
            } else{
                enpessant_capture = BitboardUtils::GeneratePawnSetAttacks<turn>(current_pawn) & enpessant_bb;
            }
            
        }

        if(current_pawn&hv_pinmask){
            captures = 0;//no captures possible when pinned either horisontally or vertically
            enpessant_capture = 0;//ditto for enpessant
            forward_slide &= hv_pinmask;//can only stay in my mask
        }
        if(current_pawn&diag_pinmask){
            forward_slide = 0;//no stepping forward
            captures &= diag_pinmask;//can only capture along my ray
            enpessant_capture &= diag_pinmask;
        }

        Bitboard end_locations = (forward_slide|captures) & valid_end_locations;
        end_locations |= enpessant_capture;//sometimes enpessant can go to an invalid location, as it kills a checking pawn, see d6 in 1k6/8/8/3pP3/2K5/8/8/8 w - d6 0 1
        move_list = AddFromBB(end_locations & BitboardUtils::NOT_PROMOTION, current_sq, move_list);
        move_list = AddPromotions(end_locations & BitboardUtils::PROMOTION, current_sq, move_list);
    }

    return move_list;
}

/**
 * @brief simpler pawn move generation for only pawn captures, excluding enpessant
 */
template<bool turn>//no enpessant to speed things up maybe?
Move* GeneratePawnCaptures(Bitboard pawns, Bitboard capturable_pieces, Bitboard hv_pinmask, Bitboard diag_pinmask, Move* move_list){
    //TODO don't loop pawns for captures, just do set captures, and subtract the jump made, but this can't work with pins etc.

    pawns &= ~hv_pinmask;//no captures possible when pinned either horisontally or vertically

    Bitloop(pawns){
        Square current_sq = BitboardUtils::FindLSB(pawns);
        Bitboard current_pawn = BitboardUtils::MakeBitBoard(current_sq);

        Bitboard captures = BitboardUtils::GeneratePawnSetAttacks<turn>(current_pawn) & capturable_pieces;

        if(current_pawn&diag_pinmask){
            captures &= diag_pinmask;//can only capture along my ray
        }

        move_list = AddFromBB(captures & BitboardUtils::NOT_PROMOTION, current_sq, move_list);
        move_list = AddPromotions(captures & BitboardUtils::PROMOTION, current_sq, move_list);
    }

    return move_list;
}

template<bool friendly_direction>
Bitboard EnemyAttacks(Bitboard blockers_minus_yourking, Bitboard enemy_orthogonals, Bitboard enemy_diagonals, Bitboard enemy_pawns, Bitboard enemy_knights, Square enemy_king){
    Bitboard result = 0;
    //add Q,B,R to result
    Bitloop(enemy_orthogonals){
        Square sq = BitboardUtils::FindLSB(enemy_orthogonals);
        result |= MoveLookup::SliderLookup<PieceUtils::ROOK>(sq, blockers_minus_yourking);
    }
    Bitloop(enemy_diagonals){
        Square sq = BitboardUtils::FindLSB(enemy_diagonals);
        result |= MoveLookup::SliderLookup<PieceUtils::BISHOP>(sq, blockers_minus_yourking);
    }
    //add P to result
    result |=  BitboardUtils::GeneratePawnSetAttacks<!friendly_direction>(enemy_pawns);
    //add N to result
    Bitloop(enemy_knights){
        result |= MoveLookup::KnightLookup(BitboardUtils::FindLSB(enemy_knights));
    }
    //add K to result
    result |= MoveLookup::KingLookup(enemy_king);

    return result;
}

template<Piece direction>
Bitboard CalculatePinsAndUpdateCheckMask(Bitboard enemy_samedirection_sliders, Square my_king, Bitboard all_pieces, Bitboard& check_mask){
    Bitboard king_to_enemies_mask = MoveLookup::SliderLookup<direction>(my_king, enemy_samedirection_sliders);
    Bitboard king_bb = BitboardUtils::MakeBitBoard(my_king);

    Bitboard result = 0;

    Bitboard potential_enemy_pinners = king_to_enemies_mask & enemy_samedirection_sliders;
    Bitloop(potential_enemy_pinners){
        Square enemy = BitboardUtils::FindLSB(potential_enemy_pinners);
        Bitboard enemy_bb = potential_enemy_pinners & -potential_enemy_pinners;
        //attack from attacker to king & attack from king to attacker is the line between them
        Bitboard squares_between = MoveLookup::SliderLookup<direction>(enemy, king_bb) & king_to_enemies_mask;
        unsigned int pieces_between_king_and_slider = BitboardUtils::PopCount(squares_between&all_pieces);

        if(pieces_between_king_and_slider == 1){//there must be only one piece here to be pinned
            //include the enemy as I can capture them om nom
            result |= squares_between | enemy_bb;//perhaps I can branchless-ify this?
        }
        if(pieces_between_king_and_slider == 0){
            check_mask &= squares_between | enemy_bb;//can only block or capture to stop the check
        }
    }
    return result;
}

template<bool turn, bool captures_only>
Move* MoveGenerator::GenerateMain(const Board &board, SearchUtils::PlyData* ply_data, Move* move_list){
    const Bitboard my_pieces = board.colour_bitboard[turn];
    const Bitboard enemy_pieces = board.colour_bitboard[!turn];
    const Bitboard all_blockers = board.colour_bitboard[0] | board.colour_bitboard[1];
    const Bitboard blockers_minus_friendly_king = all_blockers ^ (board.piece_bitboard[PieceUtils::KING] & my_pieces);

    assert((my_pieces&enemy_pieces) == 0);

    const Square my_king = BitboardUtils::FindLSB(board.piece_bitboard[PieceUtils::KING] & my_pieces);
    const Bitboard orthogonal_sliders = (board.piece_bitboard[PieceUtils::ROOK] | board.piece_bitboard[PieceUtils::QUEEN]);
    const Bitboard diagonal_sliders = (board.piece_bitboard[PieceUtils::BISHOP] | board.piece_bitboard[PieceUtils::QUEEN]);
    const Bitboard knights = board.piece_bitboard[PieceUtils::KNIGHT];
    const Bitboard pawns = board.piece_bitboard[PieceUtils::PAWN];
    
    const Bitboard enemy_attack_squares = EnemyAttacks<turn>(
        blockers_minus_friendly_king,
        orthogonal_sliders&enemy_pieces,
        diagonal_sliders & enemy_pieces,
        pawns & enemy_pieces,
        knights & enemy_pieces,
        BitboardUtils::FindLSB(board.piece_bitboard[PieceUtils::KING] & enemy_pieces));

    ply_data->in_check = board.piece_bitboard[PieceUtils::KING] & my_pieces & enemy_attack_squares;//if enemy attack is my king square

    const Bitboard king_move_bb = MoveLookup::KingLookup(my_king) & ~my_pieces & ~enemy_attack_squares & (captures_only ? enemy_pieces : BitboardUtils::ALL_SQUARES);//conditionally screen for captures
    move_list = AddFromBB(king_move_bb, my_king, move_list);

    //this is the checkmask also
    Bitboard nonking_end_squares = ~my_pieces & (captures_only ? enemy_pieces : BitboardUtils::ALL_SQUARES);//I can go anywhere except on to my own pieces, and sometimes only captures too

    if constexpr(!captures_only){
        constexpr Bitboard my_kingside_checkmask = turn ? CastlingUtils::WK_CHECK_MASK : CastlingUtils::BK_CHECK_MASK;
        constexpr Bitboard my_kingside_piecemask = turn ? CastlingUtils::WK_PIECE_MASK : CastlingUtils::BK_PIECE_MASK;
        constexpr Castling my_kingside_castle_type    = turn ? CastlingUtils::WK_CASTLE : CastlingUtils::BK_CASTLE;

        constexpr Bitboard my_queenside_checkmask = turn ? CastlingUtils::WQ_CHECK_MASK : CastlingUtils::BQ_CHECK_MASK;
        constexpr Bitboard my_queenside_piecemask = turn ? CastlingUtils::WQ_PIECE_MASK : CastlingUtils::BQ_PIECE_MASK;
        constexpr Castling my_queenside_castle_type    = turn ? CastlingUtils::WQ_CASTLE : CastlingUtils::BQ_CASTLE;

        if(CastlingUtils::HasCastling<my_kingside_castle_type>(ply_data->castling_rights) && //has kingside castling old: board.castling[my_kingside_castle_type]
        (my_kingside_checkmask & enemy_attack_squares) == 0 && //ensure no important squares are under attack
        (my_kingside_piecemask & all_blockers) == 0)//ensure no pieces are blocking me
        {
            *move_list++ = MoveUtils::MakeMove(my_king, turn ? 6 : 62, PieceUtils::EMPTY, my_kingside_castle_type);//6,62 are where the king ends up after kingside castling
        }
        if(CastlingUtils::HasCastling<my_queenside_castle_type>(ply_data->castling_rights) && //has queenside castling old: board.castling[my_queenside_castle_type]
        (my_queenside_checkmask & enemy_attack_squares) == 0 && //ensure no important squares are under attack
        (my_queenside_piecemask & all_blockers) == 0)//ensure no pieces are blocking me
        {
            *move_list++ = MoveUtils::MakeMove(my_king, turn ? 2 : 58, PieceUtils::EMPTY, my_queenside_castle_type);//2,58 are where the king ends up after queenside castling
        }
    }

    const Bitboard hv_pinmask = CalculatePinsAndUpdateCheckMask<PieceUtils::ROOK>(orthogonal_sliders&enemy_pieces, my_king, all_blockers, nonking_end_squares);
    const Bitboard diag_pinmask = CalculatePinsAndUpdateCheckMask<PieceUtils::BISHOP>(diagonal_sliders&enemy_pieces, my_king, all_blockers, nonking_end_squares);
    
    const Bitboard checking_knights = MoveLookup::KnightLookup(my_king) & knights & enemy_pieces;
    if(checking_knights){//enemy knight is checking my king
        nonking_end_squares &= checking_knights;//there can only be one checking knight at a time, so only allow killing it
    }

    const Bitboard checking_pawns = BitboardUtils::GeneratePawnSetAttacks<turn>(BitboardUtils::MakeBitBoard(my_king)) & pawns & enemy_pieces;//pawn attacks from king to pawns = pawn attacks from enemy in opposite direction to king
    if(checking_pawns){
        nonking_end_squares &= checking_pawns;
    }

    if(nonking_end_squares == 0){
        return move_list;//no more possible moves, usually due to double check
    }
    move_list = GenerateSliderMoves<PieceUtils::ROOK>(orthogonal_sliders & my_pieces, 
        all_blockers, nonking_end_squares, hv_pinmask, diag_pinmask, move_list);

    move_list = GenerateSliderMoves<PieceUtils::BISHOP>(diagonal_sliders & my_pieces,
        all_blockers, nonking_end_squares, hv_pinmask, diag_pinmask, move_list);

    move_list = GenerateKnightMoves(knights&my_pieces, nonking_end_squares, hv_pinmask|diag_pinmask, move_list);

    if constexpr(!captures_only){
        //maybe remove branching entirely
        //this branch +20Mnps on branch inside GeneratePawnMoves
        if(SquareUtils::IsValid(ply_data->enpessant)){
            move_list = GeneratePawnMoves<turn, true>(pawns & my_pieces,
            all_blockers, nonking_end_squares, enemy_pieces,ply_data->enpessant, hv_pinmask, diag_pinmask,my_king, orthogonal_sliders&enemy_pieces, move_list);
        } else{
            move_list = GeneratePawnMoves<turn, false>(pawns & my_pieces,
            all_blockers, nonking_end_squares, enemy_pieces,ply_data->enpessant, hv_pinmask, diag_pinmask,my_king, orthogonal_sliders&enemy_pieces, move_list);
        }
    } else {
        move_list = GeneratePawnCaptures<turn>(pawns & my_pieces, nonking_end_squares, hv_pinmask, diag_pinmask, move_list);
    }

    return move_list;
}

bool MoveGenerator::VerifyMove(const Board &board, Move candidate)
{
    if(candidate == MoveUtils::NULL_MOVE) return true;
    if(MoveUtils::FromSquare(candidate) >= 64) return false;
    if(MoveUtils::ToSquare(candidate) >= 64) return false;
    const Bitboard start_bb = BitboardUtils::MakeBitBoard(MoveUtils::FromSquare(candidate));
    const Bitboard end_bb = BitboardUtils::MakeBitBoard(MoveUtils::ToSquare(candidate));
    const Bitboard my_pieces = board.colour_bitboard[board.turn];

    if((start_bb & my_pieces) == 0){
        //moved a piece that isn't mine
        return false;
    }
    if((end_bb & my_pieces) != 0){
        //captured friendly piece
        return false;
    }

    return true;//i am not entirely sure, but this move looks ok
}

template Move* MoveGenerator::GenerateMain<true, false>(const Board &board, SearchUtils::PlyData* ply_data, Move* move_list);
template Move* MoveGenerator::GenerateMain<false, false>(const Board &board, SearchUtils::PlyData* ply_data, Move* move_list);

template Move* MoveGenerator::GenerateMain<true, true>(const Board &board, SearchUtils::PlyData* ply_data, Move* move_list);
template Move* MoveGenerator::GenerateMain<false, true>(const Board &board, SearchUtils::PlyData* ply_data, Move* move_list);