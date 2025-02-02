#pragma once

#include "Util.h"
#include <cstdint>
#include <cassert>

typedef std::uint32_t Castling;
typedef std::uint32_t CastlingMask;

namespace CastlingUtils
{

//where there can't be any pieces while castling
constexpr Bitboard WK_PIECE_MASK = 0x60;
constexpr Bitboard WQ_PIECE_MASK = 0x0e;
constexpr Bitboard BK_PIECE_MASK = WK_PIECE_MASK << (8*7);
constexpr Bitboard BQ_PIECE_MASK = WQ_PIECE_MASK << (8*7);

//where there can't be any checking while castling
constexpr Bitboard WK_CHECK_MASK = 0x70;
constexpr Bitboard WQ_CHECK_MASK = 0x1c;
constexpr Bitboard BK_CHECK_MASK = WK_CHECK_MASK << (8*7);
constexpr Bitboard BQ_CHECK_MASK = WQ_CHECK_MASK << (8*7);

//castling king positions
constexpr Square W_START_SQ = 4;
constexpr Square B_START_SQ = 60;

constexpr Square WK_END_SQ = W_START_SQ+2;
constexpr Square WQ_END_SQ = W_START_SQ-2;

constexpr Square BK_END_SQ = B_START_SQ+2;
constexpr Square BQ_END_SQ = B_START_SQ-2;

//castling rook positions
constexpr Square WK_ROOK_SQ = 7;
constexpr Square WQ_ROOK_SQ = 0;

constexpr Square BK_ROOK_SQ = WK_ROOK_SQ+(8*7);
constexpr Square BQ_ROOK_SQ = WQ_ROOK_SQ+(8*7);

constexpr Castling WK_CASTLE = 0; // White Kingside
constexpr Castling WQ_CASTLE = 1; // White Queenside
constexpr Castling BK_CASTLE = 2; // Black Kingside
constexpr Castling BQ_CASTLE = 3; // Black Queenside
constexpr Castling NO_CASTLE = 4;

/**
 * @brief generates a part of a castling flags mask based off the castling constant provided
 * @returns 1 << castle
 */
constexpr CastlingMask GenerateCastleMask(Castling castle) {
    assert(castle < 32);
    return 1 << castle;
}

/**
 * @brief calculates whether a castle flag is set
 * @returns true if the castle flag is set, else false
 */
template<Castling castle_type>
bool HasCastling(CastlingMask castle_mask){
    return GenerateCastleMask(castle_type) & castle_mask;
}

} // namespace CastlingUtils
