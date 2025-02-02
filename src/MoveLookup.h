#pragma once

#include "Util.h"

namespace MoveLookup {
/**
 * @brief looks up the slider moves from position, going to and capturing any of blockers
 * @param base_piece_type constexpr of which piece type is used to search for moves i.e rook -> all horisontal and vertical moves, and must be the base piece(don't add WHITE_FLAG)
 */
template<Piece base_piece_type>
Bitboard SliderLookup(Square position, Bitboard blockers);

/**
 * @brief uses a lookup table to find the possible knight moves from position
 * @param position the square the knight is on
 * @returns a bitboard of all the squares the knight can go to
 */
Bitboard KnightLookup(Square position);

/**
 * @brief uses a lookup table to find the possible king moves from position
 * @param position the square the king is on
 * @returns a bitboard of all the squares the king can go to
 */
Bitboard KingLookup(Square position);
}