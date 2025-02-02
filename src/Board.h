#pragma once

#include "Util.h"
#include "Castling.h"
#include "nn_lib.h"

#include <string>
#include <stdint.h>

struct Board {
    Board();
    Board& operator=(Board&&) = default;//allow move assignment
    Board(Board&&) = default;//             ''
    Board(const Board&) = delete;
    Board& operator=(const Board &original) = delete;

    //index 0 is square A1
    //index 63 is square H8
    Piece squares[64] = {};

    Bitboard colour_bitboard[2] = {};//[0] is black, [1] is white
    Bitboard piece_bitboard[6] = {};

    NeuralNetwork::NeuralNetwork<64,8> neural_net;

    bool turn;
};

namespace BoardUtils
{
constexpr int MAX_GAME_LENGTH = 5898;
/**
 * @returns true if bitboards, squares and turn are all the same between both boards
 */
bool CompareBoards(const Board &board1, const Board &board2);

/**
 * @brief copies the original board to a new one, separately allocated in memory
 * @param original the chessboard to copy
 * @warning will not clone the neural network!!
 * @bug not updated for neural network addition
 */
Board CloneBoard(const Board &original);

/**
 * @brief Plays the move on the board, and updates data in ply_data
 * @param board the chessboard to modify
 * @param m the move to play
 * @param ply_data the current ply's data to modify with undo data and extra info
 * @warning ply_data+1 must be a ply data too, as that is also modified
 */
void MakeMove(Board &board, Move m, SearchUtils::PlyData* ply_data);

/**
 * @brief Undoes the move on the board
 * @param board the board to modify
 * @param m the move previously made
 * @param ply_data the ply data set when the move was made
 */
void UnMakeMove(Board &board, Move m, const SearchUtils::PlyData* ply_data);

void MakeNullMove(Board &board, SearchUtils::PlyData* ply_data);

void UnMakeNullMove(Board &board);

/**
 * @brief calculates the hash of the current chessboard
 * @param board the board to calculate the hash of
 * @param ply_data the data to support the board
 * @param turn who is to move on the board
 * @warning do not re-run this function as it can be slow. Make and unmake move should update the hash automatically
 */
uint64_t CalculateZobristHash(const Board& board, const SearchUtils::PlyData* ply_data);
} // namespace BoardUtils

