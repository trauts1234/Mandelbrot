#include "Util.h"

//SquareUtils

bool SquareUtils::InBounds(int file, int rank)
{
    if(file < 0 || file >= 8){return false;}
    if(rank < 0 || rank >= 8){return false;}
    return true;
}

Move MoveUtils::MakeMove(Square from, Square to, Piece promotion, unsigned int castle)
{
    return 
        from |
        (to << 8) |
        (promotion << 16) |
        (castle << 24);
}

Bitboard BitboardUtils::MakeBitBoard(Square sq)
{
    if(!SquareUtils::IsValid(sq)){return 0;}

    return 1ull << sq;
}

Square BitboardUtils::PopLSB(Bitboard &bits)
{
    Square ans = FindLSB(bits);
    bits &= bits - 1;
    return ans;
}

template<bool pawns_direction>
Bitboard BitboardUtils::GeneratePawnSetAttacks(Bitboard pawns){
    if constexpr(pawns_direction){
        return (pawns & ~BitboardUtils::FILE_A) << 7 | (pawns & ~BitboardUtils::FILE_H) << 9;
    } else{
        return (pawns & ~BitboardUtils::FILE_A) >> 9 | (pawns & ~BitboardUtils::FILE_H) >> 7;
    }
}

template <bool direction, int num_sq>
Bitboard BitboardUtils::Forwards(Bitboard original)
{
    constexpr int shift_count = 8*num_sq;
    if(direction){
        return original << shift_count;
    } else{
        return original >> shift_count;
    }
}

template <bool direction>
Bitboard BitboardUtils::SquaresInFront(Square square)
{
    if constexpr(direction){
        return (BitboardUtils::FILE_A << 8) << square;//shift to one square ahead of the square
    } else{
        return (BitboardUtils::FILE_H >> 8) >> (63-square);
    }
}

bool SearchUtils::IsDraw(PlyData* current_node){
    PlyData* previous = current_node-2;

    if(current_node->fifty_move_rule >= 100){
        return true;//repetition by fifty move rule
    }

    uint64_t current_hash = current_node->zobrist;

    for(unsigned int i=2;i<current_node->fifty_move_rule;i+=2){//might not work, as FENs with big fifty move counters could cause searches of positions before the FEN, which obviously don't exist
        if(current_hash == previous->zobrist){
            return true;
        }
        previous -= 2;//back 2 nodes
    }

    return false;
}

template Bitboard BitboardUtils::Forwards<true,1>(Bitboard original);
template Bitboard BitboardUtils::Forwards<false,1>(Bitboard original);
template Bitboard BitboardUtils::Forwards<true,2>(Bitboard original);
template Bitboard BitboardUtils::Forwards<false,2>(Bitboard original);

template Bitboard BitboardUtils::GeneratePawnSetAttacks<true>(Bitboard pawns);
template Bitboard BitboardUtils::GeneratePawnSetAttacks<false>(Bitboard pawns);

template Bitboard BitboardUtils::SquaresInFront<true>(Square square);
template Bitboard BitboardUtils::SquaresInFront<false>(Square square);
