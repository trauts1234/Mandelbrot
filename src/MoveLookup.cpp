#include "MoveLookup.h"

#include "lookuptables.h"

//eww... it stops a linker error though
template Bitboard MoveLookup::SliderLookup<PieceUtils::ROOK>(Square position, Bitboard blockers);
template Bitboard MoveLookup::SliderLookup<PieceUtils::BISHOP>(Square position, Bitboard blockers);

template<Piece base_piece_type>
Bitboard MoveLookup::SliderLookup(Square position, Bitboard blockers){
    static_assert(base_piece_type == PieceUtils::ROOK || base_piece_type == PieceUtils::BISHOP, "invalid piece passed to SliderLookup");

    if constexpr(base_piece_type == PieceUtils::ROOK){
        const Bitboard important_blockers_mask = ROOK_MASKS[position];
        return SLIDER_DATA[ROOK_OFFSETS[position] + BitboardUtils::CondenseBlockersWithMask(blockers, important_blockers_mask)];
    } else if constexpr(base_piece_type == PieceUtils::BISHOP){
        const Bitboard important_blockers_mask = BISHOP_MASKS[position];
        return SLIDER_DATA[BISHOP_OFFSETS[position] + BitboardUtils::CondenseBlockersWithMask(blockers, important_blockers_mask)];
    } else{
        static_assert(false);
    }


    //return 0;//stops a warning, I should never get here because this function is only for sliders
}

Bitboard MoveLookup::KnightLookup(Square position)
{
    return KNIGHT_MOVES[position];
}

Bitboard MoveLookup::KingLookup(Square position)
{
    return KING_MOVES[position];
}
