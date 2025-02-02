#pragma once

#include <climits>
#include "Board.h"
#include "nn_lib.h"

#define Evaluation int

struct TunableParams {
    // x[0] is the middlegame params, [1] is endgame
    //evaluation features: (must be all same type for tuning - this struct is cast to a int*)
    Evaluation piece_values[6];//subtract 1 from base piece for piece index
    Evaluation Passed_Pawn_Bonuses[2][8];//[mg,eg][sqs from end]
    Evaluation Isolated_Pawn_Loss[2];//[mg,eg]
    Evaluation Pawn_Shield_Bonus;
    Evaluation rook_open_file_bonus;
    Evaluation rook_half_file_bonus;
    Evaluation minor_impact_on_phase;
    Evaluation rook_queen_impact_on_phase;
};

namespace Eval
{
    constexpr Evaluation NULL_EVAL = INT_MAX-1;
    constexpr Evaluation CHECKMATE_WIN = INT_MAX/4;
    constexpr Evaluation FURTHEST_MATE = CHECKMATE_WIN-100;
    constexpr Evaluation START_NEGATIVE = -CHECKMATE_WIN - 1;//below the worst, so I always find a move

    /**
     * @brief calculates the static evaluation of the board
     */
    Evaluation EvaluateBoard(Board& board);

    /**
     * @brief make a mate score
     * @param ply_from_root how many moves deep are you in the search?
     * @returns a negative mate score adjusted linearly by distance from root
     */
    Evaluation MakeMatedEvaluation(int ply_from_root);
} // namespace Eval
