#pragma once

#include <cstdint> //force size of numbers
#include <immintrin.h> //vomit-inducing magic for speedy code
#include <bit>//            ''
#include <algorithm>

#define Square std::uint32_t
#define Piece std::uint32_t
#define Move std::uint32_t
#define Bitboard std::uint64_t
#define ALIGN_CACHE alignas(64)

namespace SquareUtils{

constexpr Square NULL_SQUARE = 64;

inline Square FromCoords(Square file, Square rank) {return file + 8*rank;}

/**
 * @brief Get the x coordinate of the square
 * @warning undefined behaviour for NULL_SQUARE
 */
inline Square ToFile(Square sq) {return sq % 8;}

/**
 * @brief Get the y coordinate of the square
 * @warning undefined behaviour for NULL_SQUARE
 */
inline Square ToRank(Square sq) {return sq / 8;}

/**
 * @brief folds all the squares to a range of 32
 * a1=a8=0, a2=a7=1, a3=a6=2, ..., d8=e8=31
 */
inline Square FoldSquares(Square sq) {
    Square file = SquareUtils::ToFile(sq);
    if(file >= 4){
        file = 7-file;//if on kingside, flip it back
    }
    return file + 4*ToRank(sq);
}

/// @brief detects whether the square is a NULL or a valid square
/// @param s the square to check
/// @return true if the value is not equal to NULL_SQUARE
inline bool IsValid(Square s) {return s != NULL_SQUARE;}

/**
 * @returns true if the x,y coordinate pair are in the chess board
 * data types are still int to house negative numbers
 */
bool InBounds(int file, int rank);

/**
 * @brief calculates the sum of dy + dx for the two squares
 */
inline Square ManhattanDistance(Square a, Square b)
{
    Square rank_diff = std::abs((int)SquareUtils::ToRank(a) - (int)SquareUtils::ToRank(b));
    Square file_diff = std::abs((int)SquareUtils::ToFile(a) - (int)SquareUtils::ToFile(b));
    return rank_diff + file_diff;
}

/**
 * @brief finds the number of king moves needed to go from one square to the other
 */
inline Square ChebyshevDistance(Square a, Square b)
{
    Square rank_diff = std::abs((int)SquareUtils::ToRank(a) - (int)SquareUtils::ToRank(b));
    Square file_diff = std::abs((int)SquareUtils::ToFile(a) - (int)SquareUtils::ToFile(b));
    return std::max(rank_diff, file_diff);
}

}//SquareUtils

namespace PieceUtils{

constexpr Piece KNIGHT = 0;
constexpr Piece BISHOP = 1;
constexpr Piece ROOK = 2;
constexpr Piece QUEEN = 3;
constexpr Piece PAWN = 4;
constexpr Piece KING = 5;

constexpr Piece WHITE_FLAG=6;
constexpr Piece EMPTY=16;
constexpr Piece MAX_NUM = WHITE_FLAG + KING;

/// @brief usage: bool x = IsWhite(mypiece);
/// @param p the piece to check
/// @warning null square is treated as white
/// @return true if the piece is white OR empty, otherwise returns false
inline bool IsWhite(Piece p) {return p >= WHITE_FLAG;}

/// @brief converts any white piece to black one
/// @warning BasePiece(EMPTY) is undefined behaviour
/// @warning relies on white flag being a separate bit
/// @param p the piece to process
/// @return any black piece
inline Piece BasePiece(Piece p) {return p % WHITE_FLAG;}

/// @brief tells whether the piece is a NULL value or not
/// @param p the piece to check
/// @return true if the piece is EMPTY, false otherwise
inline bool IsEmpty(Piece p){return p == EMPTY;}

/**
 * @returns a piece with the opposite colour of the one entered
 */
inline Piece FlippedColour(Piece p){return PieceUtils::IsWhite(p) ? p-WHITE_FLAG : p+WHITE_FLAG;}//hacky to remove a branch

}//PieceUtils

namespace MoveUtils{
constexpr Move NULL_MOVE = ~0;//11111... should cause something to crash, if it hasn't already
/**
 * the bytes of a Move are as follows:
 * MSB
 * xxxxxx unused
 * 3 bits: castling(none,wk,wq,bk,bq)
 * 8 bits: Promotion(uncoloured)
 * 8 bits: ToSquare
 * 8 bits: FromSquare
 */

/**
 * @brief Get the square a piece moved from
 * @param m the move to read
 * @returns the "from" square of the move
 */
inline Square FromSquare(Move m) {return m & 0xff;}

/**
 * @brief Get the square a piece will move to
 * @param m the move to read
 * @returns the "to" square of the move
 */
inline Square ToSquare(Move m) {return (m >> 8) & 0xff;}

/**
 * @brief find the promotion, if any, from a move
 * @param m the move to read
 * @returns null piece if no promotion will happen, or the base of the piece that will be promoted to
 * @warning if it is white's turn to promote a pawn, it will still return black pieces
 */
inline Piece PromotionBase(Move m) {return (m >> 16) & 0xff;}

/**
 * @brief find if the move is a castling move
 * @param m the move to read
 * @returns a castling constant from Castling.h to reflect the effect of this move, i.e BQ_CASTLE
 * @warning if no castle is planned for this move, it will return NO_CASTLE
 */
inline unsigned int CastleType(Move m) {return (m >> 24) & 0x7;}

/**
 * @brief generates a move from data about the move
 * @warning if it is a promotion or castle, remember to specify. the move maker will not work if these aren't set correctly
 */
Move MakeMove(Square from, Square to, Piece promotion_uncoloured = PieceUtils::EMPTY, unsigned int castle = 4);//4 is no castling

}//MoveUtils

namespace BitboardUtils{

constexpr Bitboard ALL_SQUARES = ~0;
constexpr Bitboard FILE_A = 0x0101010101010101ULL;
constexpr Bitboard FILE_H = FILE_A << 7;

constexpr Bitboard DOUBLE_PUSH =       0x00ff00000000ff00ULL;
constexpr Bitboard AFTER_DOUBLE_PUSH = 0x000000ffff000000ULL;

constexpr Bitboard PROMOTION =         0xff000000000000ffULL;
constexpr Bitboard NOT_PROMOTION = ~PROMOTION;

constexpr Bitboard WHITE_SIDE = 0xffffffffULL;
constexpr Bitboard BLACK_SIDE = ~WHITE_SIDE;

constexpr Bitboard BLACK_SQUARES = 0x55aa'55aa'55aa'55aaULL;
constexpr Bitboard WHITE_SQUARES = ~BLACK_SQUARES;

/// @brief creates a bitboard with the only set bit, and returns 0 if the square is invalid
/// @param sq the square that will have a 1
/// @return a bitboard representing the square
Bitboard MakeBitBoard(Square sq);

/// @brief determines if a square is in the bitboard, and works with invalid squares
/// @param b the bitboard that the square might be in
/// @param sq the square that will be checked
/// @return true if the square is in the squares represented by the bitboard
inline bool ContainsBit(Bitboard b, Square sq) {return (b & MakeBitBoard(sq)) != 0;}

/**
 * @brief
 * uses PEXT instructions to condense the blockers to an index
 * 
 * pseudocode software emulation example:
 * 
 * for each set bit in mask
 * 
 *   if bit is also set in blockers: set next bit to 1
 * 
 *   else: set next bit to 0
 * 
 * 
 * @param blockers where the pieces are on the board
 * @param mask where the current piece can move to on an empty board
 * @return a PEXT-condensed index for further lookup tables
 */
inline Bitboard CondenseBlockersWithMask(Bitboard blockers, Bitboard mask) {return _pext_u64(blockers, mask);}

/**
 * @brief inverse of CondenseBlockersWithMask
 */
inline Bitboard UncondenseBlockersWithMask(Bitboard condensed, Bitboard mask) {return _pdep_u64(condensed, mask);}

/**
 * @brief counts the number of set bits in a bitboard
 */
inline unsigned int PopCount(Bitboard bits){return std::popcount(bits);}

/**
 * @brief finds the Least Significant Bit of a bitboard
 * @returns the smallest value for Square that makes ContainsBit(bits, return_value) true, or 64 if bits equals 0
 */
inline Square FindLSB(Bitboard bits) {return _tzcnt_u64(bits);}

/**
 * @brief finds the LSB, then removes it from the bitboard
 * @returns the Square just removed from the bitboard
 */
Square PopLSB(Bitboard& bits);

/**
 * @brief creates a loop, popping each bit in x. use FindLSB to find the index of each bit
 */
#define Bitloop(X) for(;X; X = _blsr_u64(X))

/**
 * @brief changes bits so that ContainsBit(bits,sq) equals false
 * @todo maybe use _blsr_u64 instead of my current solution
 */
inline void RemoveSquare(Bitboard &bits, Square sq) {bits &= ~MakeBitBoard(sq);}

/**
 * @brief changes bits so that ContainsBit(bits,sq) equals true
 */
inline void AddSquare(Bitboard &bits, Square sq) {bits |= MakeBitBoard(sq);}

/**
 * @brief Generates a bitboard of all the attacks possible for a set of pawns
 * @param pawns_direction true if pawns will attack in the direction white pawns attack, and vice-versa for false and black
 */
template<bool pawns_direction>
Bitboard GeneratePawnSetAttacks(Bitboard pawns);

/**
 * @brief pushes all bits in the mask
 * @param direction which way to move all the bits (true is the direction white pawns move)
 * @param num_sq how many squares forward to go
 */
template <bool direction, int num_sq>
Bitboard Forwards(Bitboard original);

/**
 * @brief creates a bitboard of every square that is in front of the square, facing in {direction}
 * @returns a bitboard with a line from but not including square in {direction}
 */
template<bool direction>
Bitboard SquaresInFront(Square square);

}//BitboardUtils

namespace SearchUtils
{

/**
 * @brief List containing data about this position
 * subtract or add to the pointer (add is forwards) to read further or previous data
 */
struct ALIGN_CACHE PlyData{
    PlyData() = default;

    //current board data for the board
    uint64_t zobrist;
    Square enpessant;
    unsigned int castling_rights;
    unsigned int fifty_move_rule;

    int ply_from_root;

    Piece killed;//undo move data, for the most recently made move

    //move generation extra info
    bool in_check;

    Move best_move;

    Move killer_move = MoveUtils::NULL_MOVE;
};

bool IsDraw(PlyData* current_node);

} // namespace SearchUtils
