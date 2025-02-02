#include "Eval.h"
#include "threadsafe_io.h"
#include <cassert>
#include "StringTools.h"
#include "MoveLookup.h"
#include "nn_lib.h"
#include <algorithm>

Evaluation Eval::EvaluateBoard(Board &board)
{
    //sync_cout << StringTools::CreateNNVectorInput(board) << std::endl;
    Evaluation score = board.neural_net.FastForwardPass();
    return board.turn ? score : -score;
}

Evaluation Eval::MakeMatedEvaluation(int ply_from_root)
{
    assert(ply_from_root >= 0);
    Evaluation positive_score = Eval::CHECKMATE_WIN - ply_from_root;//further from root means slower mate and therefore worse
    assert(positive_score >= Eval::FURTHEST_MATE);
    return -positive_score;//checkmate is bad
}