#include "sliding_lookup_generation.h"

#include "Util.h"
#include "StringTools.h"
#include <vector>
#include <cmath>
#include <iostream>

/// @brief if the mask=101b, combinations returned are: 1b,100b,101b
/// @param mask only count combinations of pieces in this mask
/// @return returns a list of bitboards, one for every combination of miscellaneous chess pieces in the mask
std::vector<Bitboard> PermutationsInMask(Bitboard mask){
    unsigned int blocker_positions_count = BitboardUtils::PopCount(mask);// blockers could be in any one or many of the set bits of the mask
    Bitboard blocker_permutations_count = 1ull << blocker_positions_count;//2^positions count unique combinations of blockers

    std::vector<Bitboard> result = {};
    for(Bitboard i=0;i<blocker_permutations_count;i++){
        //now the lowest bits of i just need to be put in the right places to block the rook/bishop
        result.push_back(BitboardUtils::UncondenseBlockersWithMask(i, mask));
    }

    return result;
}

/**
 * @brief generates a bitboard showing a ray from pos using vector (dy,dx)
 */
Bitboard GenerateRay(int dy, int dx, Square pos, Bitboard blockers, bool include_last_square_in_slide){
    Bitboard result = 0;

    int file = SquareUtils::ToFile(pos);
    int rank = SquareUtils::ToRank(pos);

    while(1){
        file += dx;
        rank += dy;
        pos = SquareUtils::FromCoords(file, rank);

        Square check_x = include_last_square_in_slide ? file : file+dx;
        Square check_y = include_last_square_in_slide ? rank : rank+dy;
        if(!SquareUtils::InBounds(check_x, check_y)){
            return result;
        }

        result |= BitboardUtils::MakeBitBoard(pos);

        if(BitboardUtils::ContainsBit(blockers, pos)){
            //I have just added the capture of a piece
            return result;//stop here
        }
    }
}

Bitboard GenerateRookMoves(Square start_pos, Bitboard blockers, bool include_last_square_in_slide){
    Bitboard result = 0;

    int dys[] = {1,-1,0,0};
    int dxs[] = {0,0,1,-1};

    for(int i=0;i<4;i++){
        result |= GenerateRay(dys[i], dxs[i], start_pos, blockers, include_last_square_in_slide);
    }

    return result;
}

Bitboard GenerateBishopMoves(Square start_pos, Bitboard blockers, bool include_last_square_in_slide){
    Bitboard result = 0;

    int dys[] = {1,-1,1,-1};
    int dxs[] = {1,1,-1,-1};

    for(int i=0;i<4;i++){
        result |= GenerateRay(dys[i], dxs[i], start_pos, blockers, include_last_square_in_slide);
    }

    return result;
}

void GenerateData(std::vector<Bitboard>& slider_data, std::vector<Bitboard>& offsets, std::vector<Bitboard>& masks, Bitboard (*movegen)(Square,Bitboard, bool)){
    for(Square i=0;i<64;i++){
        Bitboard mask = movegen(i, 0, false);
        masks.push_back(mask);
        std::vector<Bitboard> blocker_combos = PermutationsInMask(mask);

        offsets.push_back(slider_data.size());//allows the move generator to know where the lookup starts for each square

        for(Bitboard combo : blocker_combos){
            Bitboard legal_moves = movegen(i, combo, true);
            slider_data.push_back(legal_moves);//add lookup - when blockers are condensed to an int, the entry index lines up and gives me legal_moves back
        }
    }
}

Bitboard GenerateOneStep(Square pos, std::vector<int>& dxs, std::vector<int>& dys){
    Bitboard ans = 0;

    int file = SquareUtils::ToFile(pos);
    int rank = SquareUtils::ToRank(pos);

    for(size_t i=0;i<dxs.size();i++){
        if(SquareUtils::InBounds(file+dxs[i], rank+dys[i])){
            ans |= BitboardUtils::MakeBitBoard(SquareUtils::FromCoords(file+dxs[i], rank+dys[i]));
        }
    }
    return ans;
}
int FindStepSize(int change){
    if(change == 0) return 0;
    if(change < 0) return -1;
    return 1;//if(change > 0) 
}
Bitboard GenerateAttackSquares(Square pos, Square blocker){
    int dy = FindStepSize(SquareUtils::ToRank(blocker) - SquareUtils::ToRank(pos));
    int dx = FindStepSize(SquareUtils::ToFile(blocker) - SquareUtils::ToFile(pos));
    return GenerateRay(dy, dx, pos, BitboardUtils::MakeBitBoard(blocker), true);
}

template<bool direction>
Bitboard GeneratePasserFrontMask(Square pos){
    Bitboard front_straight_squares = GenerateRay(direction?1:-1, 0, pos, 0, true);
    return front_straight_squares | BitboardUtils::GeneratePawnSetAttacks<direction>(front_straight_squares);
}

/**
 * @brief generates a bitboard of bars either side of the square, to detect
 */
Bitboard GenerateIsolatedPawnMask(Square pos){
    Square file = SquareUtils::ToFile(pos);
    Bitboard result = 0;
    if(file != 0){
        result |= BitboardUtils::FILE_A << (file-1);
    }
    if(file != 7){
        result |= BitboardUtils::FILE_A << (file+1);
    }
    return result;
}

void PrintTable(const std::vector<Bitboard>& table, const std::string& tableName) {
    std::cout << "constexpr Bitboard " << tableName << "[] = { ";
    for (size_t i = 0; i < table.size(); ++i) {
        std::cout << "0x" << std::hex << table[i] << "ull";
        if (i != table.size() - 1) {
            std::cout << ", ";
        }
    }
    std::cout << " };" << std::endl;
}

void GenerateMain()
{
    std::vector<Bitboard> slider_data = {};
    std::vector<Bitboard> rook_offsets = {};

    std::vector<Bitboard> rook_atks = {};
    std::vector<Bitboard> bishop_atks = {};

    GenerateData(slider_data, rook_offsets, rook_atks, &GenerateRookMoves);

    std::vector<Bitboard> bishop_offsets = {};

    GenerateData(slider_data, bishop_offsets, bishop_atks, &GenerateBishopMoves);

    std::vector<Bitboard> knight_moves = {};
    std::vector<Bitboard> king_moves = {};

    std::vector<Bitboard> w_passer_masks = {};
    std::vector<Bitboard> b_passer_masks = {};
    std::vector<Bitboard> w_attacking_king_zone = {};
    std::vector<Bitboard> b_attacking_king_zone = {};
    std::vector<Bitboard> isolated_masks = {};//only 8 long, index by file

    std::vector<int> knight_dx = {2, 1,-1,-2,  -2,-1, 1, 2};
    std::vector<int> knight_dy = {1, 2, 2, 1,  -1,-2,-2,-1};

    std::vector<int> king_dx = {1,1,0,-1,-1,-1, 0,1};
    std::vector<int> king_dy = {0,1,1, 1, 0,-1,-1,-1};

    for(Square i=0;i<8;i++){
        isolated_masks.push_back(GenerateIsolatedPawnMask(i));
    }

    for(Square i=0;i<64;i++){
        knight_moves.push_back(GenerateOneStep(i, knight_dx, knight_dy));
        king_moves.push_back(GenerateOneStep(i, king_dx, king_dy));
        w_passer_masks.push_back(GeneratePasserFrontMask<true>(i));
        b_passer_masks.push_back(GeneratePasserFrontMask<false>(i));

        Bitboard w_near_king = king_moves[i];//luckily king moves have just been generated
        Bitboard b_near_king = w_near_king;

        for(int fstep=0;fstep<3;fstep++){
            //add a square forwards from the king moves
            w_near_king |= BitboardUtils::Forwards<true,1>(w_near_king);
            b_near_king |= BitboardUtils::Forwards<false,1>(b_near_king);
        }
        w_attacking_king_zone.push_back(w_near_king);
        b_attacking_king_zone.push_back(b_near_king);

    }
    #define EVAL
    #undef SLIDER

    #ifdef SLIDER
    std::cout << "#pragma once\n#include \"Util.h\"" << std::endl;
    PrintTable(knight_moves, "KNIGHT_MOVES");
    PrintTable(king_moves, "KING_MOVES");
    PrintTable(slider_data, "SLIDER_DATA");
    PrintTable(rook_offsets, "ROOK_OFFSETS");
    PrintTable(bishop_offsets, "BISHOP_OFFSETS");
    PrintTable(rook_atks, "ROOK_MASKS");
    PrintTable(bishop_atks, "BISHOP_MASKS");
    #endif

    #ifdef EVAL

    PrintTable(isolated_masks, "ISOLATED_PAWN_SIDE_SQUARES");
    PrintTable(w_passer_masks, "W_PASSED_PAWN_FRONT_SQUARES");
    PrintTable(b_passer_masks, "B_PASSED_PAWN_FRONT_SQUARES");
    PrintTable(w_attacking_king_zone, "W_ATTACKING_KING_ZONE");
    PrintTable(b_attacking_king_zone, "B_ATTACKING_KING_ZONE");
    #endif


}