#pragma once

#include "Util.h"

#include <string>
#include "Board.h"
#include "Eval.h"

namespace StringTools{

/**
 * @brief detects whether some text could be interpreted as a chess move i.e e4c3. This test can result in false positives i.e n0z4F would count as a chess move
 * @param text the text to check
 * @return true if text could be read as a chess move, otherwise false
 */
bool IsMove(const std::string& text);

/// @brief parses a FEN string to a board position
/// @param fen the FEN string to parse
/// @param board the board to be overwritten
void ReadFEN(std::string fen, Board& board, SearchUtils::PlyData* ply_data);

Square FromString(std::string sq);

Move MoveFromString(const SearchUtils::PlyData* ply_data, std::string move);

/**
 * @brief creates a text representation of the square i.e e4, d5
 * @param sq the square to convert to string
 * @return a string representing the coordinates
 */
std::string SquareToString(Square sq);

/**
 * @brief converts rnbkqp RNBKQP notation to a piece enum
 * @param letter a single letter i.e 'n'
 * @return the piece that is assigned to that letter
 */
Piece FromLetter(char letter);

/**
 * @brief converts a piece to rnbkqp type notation
 * @param p the piece to convert
 * @return a string that represents the piece, or "" if piece is EMPTY
 */ 
std::string ToLetter(Piece p);

/**
 * @brief converts a move to a UCI format
 */
std::string MoveToString(Move m);

/**
 * @brief converts a score into UCI format, either x cp or mate x
 */
std::string ScoreToString(Evaluation eval);

/**
 * @brief pretty-prints a bitboard
 */
std::string BitboardToString(Bitboard b);

/// @brief converts a board to a FEN string
/// @param board the board to read
/// @return a FEN string that represents the board received
std::string ToFEN(const Board& board, const SearchUtils::PlyData* ply_data);

/**
 * @brief creates a comma separated string representing the vector input to the neural network
 */
std::string CreateNNVectorInput(const Board& board);

/**
 * @brief gets the path to where the executable is stored
 * @warning this may only work on linux
 */
std::string GetExecutablePath();

}//namespace StringTools