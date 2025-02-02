#pragma once
#include "Board.h"
#include "MoveGenerator.h"

namespace MoveSorting{

    /**
     * @brief calculates a clamped new value for history heuristic tables
     * @param old_history what the previous history value was
     * @param depth distance from you to the leaves of the search tree
     * @returns the new history value to store
     */
    int CalculateNewHistory(int old_history, int depth);

    /**
     * @brief calculates the index for the history heuristic
     * @returns fromsquare + 64*tosquare
     */
    inline int CalculateHistoryIndex(Move m){return MoveUtils::FromSquare(m) + 64*MoveUtils::ToSquare(m);}

    /**
     * @brief scores but does not sort the moves on how promising they look
     * @param score_list the output of scores for each move
     * @param move_list a list of all moves playable on board
     * @param moves_count the number of used moves in move_list i.e number of legal moves
     * @param expected_best_move which move you expect to be a PV from here, often collected from transposition table etc.
     */
    void CalculateMoveValues(int *score_list, const Move move_list[MoveGenerator::MAX_MOVE_COUNT], int moves_count, Move expected_best_move, const Board &board, const SearchUtils::PlyData* ply_data, const int this_side_history[64*64]);

    /**
     * @brief sorts the next element in the move_list by score_list, using selection sort
     * @param score_list the scores of all moves
     * @param move_list the moves
     * @param moves_count the number of moves
     * @param num_already_sorted how many times this function has been called
     * @returns the next move to use
     */
    Move SortNext(int *score_list, Move *move_list, int moves_count, int num_already_sorted);

    /**
     * @brief sorts the captures from most promising to least
     * @param move_list a list of all moves playable on board
     * @param moves_count the number of used moves in move_list i.e number of legal moves
     */
    void QSort(Move move_list[MoveGenerator::MAX_CAPTURE_COUNT], int moves_count, Move expected_best_move, const Board &board);
}