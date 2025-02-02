#pragma once
#include "Util.h"
#include "Board.h"

namespace MoveGenerator{

constexpr int MAX_MOVE_COUNT = 256;
constexpr int MAX_CAPTURE_COUNT = 74;

/**
 * @brief generates all legal moves
 * @param board the board whose legal moves are to be listed
 * @param move_list the start of an array of moves to which all legal moves will be written to (should be 256 elements long)
 * @returns a pointer to the last move added plus one
 */
template<bool turn, bool captures_only>
Move* GenerateMain(const Board &board, SearchUtils::PlyData* ply_data, Move* move_list);

/**
 * @brief performs some basic checks to ensure that a move is likely to be legal
 * @warning NULL_MOVE counts as legal
 * @returns false if the candidate is definitely not legal, true if it might be legal
 */
bool VerifyMove(const Board &board, Move candidate);

}