#include "Engine.h"

#include "Eval.h"
#include "MoveGenerator.h"
#include "threadsafe_io.h"
#include <atomic>
#include <chrono>
#include <cassert>
#include "StringTools.h"
#include "Timer.h"
#include "Board.h"
#include "MoveSorting.h"
#include "TranspositionTable.h"

/**
 * @brief the main search function
 */
void Worker::RootSearch(int max_depth, SearchUtils::PlyData* ply_data){
    if(max_depth == 0){
        //want just a qsearch score
        Evaluation qscore = Quiescence(Engine::QUIESCENCE_DEPTH, ply_data, Eval::START_NEGATIVE, -Eval::START_NEGATIVE);
        sync_cout << "info score " << qscore << std::endl;
        sync_cout << "bestmove " << StringTools::MoveToString(MoveUtils::NULL_MOVE) << std::endl;
        return;
    }

    Move safe_best_move = MoveUtils::NULL_MOVE;//this move is the latest safe move we have

    ply_data->ply_from_root=0;//currently at ply 0

    Evaluation alpha = Eval::START_NEGATIVE;
    Evaluation beta = -Eval::START_NEGATIVE;
    int curr_depth = 2;
    while(curr_depth <= max_depth){

        assert(alpha < beta);

        Evaluation current_iteration_eval = NegaMax<ROOT>(curr_depth, ply_data, alpha, beta, 0, true);
        Move current_iteration_move = ply_data->best_move;

        if(current_iteration_eval == Eval::NULL_EVAL && curr_depth >= 5){
            //move is forced (one possible reply)
            safe_best_move = current_iteration_move;
            break;
        }

        UpdateTimer();

        if(stop_condition.load()){//this must be right after the search to protect the rest of the program from the 0 returned when a search is forcibly stopped
                if(current_iteration_move != MoveUtils::NULL_MOVE){//new best move found
                    safe_best_move = current_iteration_move;//use last cached value
                }
                break;
            }

        if(current_iteration_eval <= alpha){
            sync_dbg << "aspiration window failed low: " << alpha << " to " << beta << " but got " << current_iteration_eval << std::endl;
            alpha = Eval::START_NEGATIVE;
            continue;//skip and redo search
        } else if (current_iteration_eval >= beta) {
            sync_dbg << "aspiration window failed high: " << alpha << " to " << beta << " but got " << current_iteration_eval << std::endl;
            beta = -Eval::START_NEGATIVE;
            continue;//skip and redo search
        }
        assert(current_iteration_eval < beta && current_iteration_eval > alpha);
        
        sync_cout << "info depth " << curr_depth << 
        " pv " << FindPV(ply_data) << //trailing space included!
        "score " << StringTools::ScoreToString(current_iteration_eval) << 
        " hashfull " << transposition_table.CalculatePerMilFull() <<
        " nodes " << leaf_nodes_searched << 
            std::endl;

        assert(current_iteration_eval != Eval::NULL_EVAL);
        //set aspiration window
        if(curr_depth >= 3){
            constexpr Evaluation window_diff = 50;
            alpha = std::max(current_iteration_eval - window_diff, Eval::START_NEGATIVE);
            beta = std::min(current_iteration_eval + window_diff, -Eval::START_NEGATIVE);
        }

        assert(alpha >= Eval::START_NEGATIVE);
        assert(beta <= -Eval::START_NEGATIVE);

        safe_best_move = current_iteration_move;//iteration complete, I can safely overwrite the previous result
        curr_depth += 1;//successful search, increase depth
    }
    stop_condition.store(true);
    sync_cout << "bestmove " << StringTools::MoveToString(safe_best_move) << std::endl;
}


template<Worker::NodeType node_type>
Evaluation Worker::NegaMax(int depth, SearchUtils::PlyData *ply_data, Evaluation alpha, Evaluation beta, int previous_extensions, bool allow_null){
    assert(depth >= 0);
    assert(alpha < beta);
    assert(Engine::BoardIsOK(current_board, ply_data));

    if(depth <= 0){
        return Quiescence(Engine::QUIESCENCE_DEPTH, ply_data, alpha, beta);
    }
    ply_data->best_move = MoveUtils::NULL_MOVE;

    if(SearchUtils::IsDraw(ply_data) && node_type != NodeType::ROOT){//is this right?
        return 0;//draw by 50 move or repetition
    }

    TTEntry tt_result = transposition_table.ProbeAdjusted(ply_data->zobrist, depth, ply_data->ply_from_root, alpha, beta);
    if(tt_result.score != Eval::NULL_EVAL){
        assert(tt_result.score <= Eval::CHECKMATE_WIN && tt_result.score >= -Eval::CHECKMATE_WIN);
        ply_data->best_move = tt_result.best_move;
        return tt_result.score;
    }

    TTLookupType score_type = TTLookupType::UPPERBOUND;//result from this search starts as being an exact score

    Move move_list[MoveGenerator::MAX_MOVE_COUNT];
    int move_scores[MoveGenerator::MAX_MOVE_COUNT];
    Move* end = current_board.turn ? MoveGenerator::GenerateMain<true, false>(current_board, ply_data, move_list) : MoveGenerator::GenerateMain<false, false>(current_board, ply_data, move_list);
    int legal_move_count = end - move_list;

    if (node_type == NodeType::ROOT && legal_move_count == 1){
        //this move is forced, so instantly return it
        ply_data->best_move = move_list[0];
        return Eval::NULL_EVAL;
    }

    if(legal_move_count == 0){
        assert(tt_result.best_move == MoveUtils::NULL_MOVE);
        if(ply_data->in_check){
            assert(ply_data->ply_from_root >= 0);
            return Eval::MakeMatedEvaluation(ply_data->ply_from_root);//enemy checkmated me
        }
        return 0;//draw
    }

    if(leaf_nodes_searched % 2048 == 0){
        UpdateTimer();
    }

    Evaluation curr_score;

    //NMP
    constexpr int nmp_reduction = 2;
    bool can_nmp = allow_null && //no previous null moves tried
        !ply_data->in_check && //illegal position if skipping my move whilst in check
        BitboardUtils::PopCount(current_board.colour_bitboard[current_board.turn] & ~current_board.piece_bitboard[PieceUtils::PAWN]) >= 3 && //3 non-pawn pieces needed, so no zugzwang
        depth >= nmp_reduction+1;//don't jump straight into qsearch, do it at high depths only

    if(node_type != NodeType::ROOT && can_nmp){

        BoardUtils::MakeNullMove(current_board, ply_data);
        curr_score = -NegaMax<NodeType::NORMAL>(depth-1-nmp_reduction, ply_data+1, -beta, 1-beta, previous_extensions, false);
        BoardUtils::UnMakeNullMove(current_board);

        if(curr_score >= beta){
            return beta;//I skipped my move and still beat beta. must be great for me, so stop here
        }
    }

    MoveSorting::CalculateMoveValues(move_scores, move_list, legal_move_count, tt_result.best_move, current_board, ply_data, history_heuristic[current_board.turn]);

    for(int i=0;i<legal_move_count;i++){
        Move current_move = MoveSorting::SortNext(move_scores, move_list, legal_move_count, i);;
        assert(current_move != MoveUtils::NULL_MOVE);

        BoardUtils::MakeMove(current_board, current_move, ply_data);

        //extensions
        int extension = 0;
        if (node_type != NodeType::ROOT && previous_extensions < 6){
            if(legal_move_count == 1//one reply extension
                || ply_data->in_check)//check extension
            {
                extension++;
            }
        }

        const bool bad_idea_to_lmr = depth < 3;
        if(i > 4 && !bad_idea_to_lmr){
            constexpr int lmr_reduction=1;
            curr_score = -NegaMax<NodeType::NORMAL>(depth-1-lmr_reduction+extension, ply_data+1, -alpha - 1, -alpha, previous_extensions+extension, true);//reduced depth
            if(curr_score > alpha && curr_score < beta){
                curr_score = -NegaMax<NodeType::NORMAL>(depth-1+extension, ply_data+1, -beta, -alpha, previous_extensions+extension, true);//normal full width search
            }
        } else{
            curr_score = -NegaMax<NodeType::NORMAL>(depth-1+extension, ply_data+1, -beta, -alpha, previous_extensions+extension, true);//normal full width search
        }

        BoardUtils::UnMakeMove(current_board, current_move, ply_data);

        if(stop_condition.load(std::memory_order_relaxed)){
            return 0;//out of time
        }

        if(curr_score > alpha){
            score_type = TTLookupType::EXACT;
            alpha = curr_score;
            ply_data->best_move = current_move;//save best move
        }
        if(alpha >= beta){
            assert(alpha == curr_score);
            transposition_table.Set(TranspositionUtils::GenerateEntry(ply_data->zobrist, ply_data->best_move, depth, TTLookupType::LOWERBOUND, beta));
            if(PieceUtils::IsEmpty(ply_data->killed)){//maybe block promotions and enpessant?
                const int previous_history = history_heuristic[current_board.turn][MoveSorting::CalculateHistoryIndex(current_move)];
                history_heuristic[current_board.turn][MoveSorting::CalculateHistoryIndex(current_move)] = MoveSorting::CalculateNewHistory(previous_history, depth);

                ply_data->killer_move = current_move;
            }
            return beta;
        }
    }

    transposition_table.Set(TranspositionUtils::GenerateEntry(ply_data->zobrist, ply_data->best_move, depth, score_type, alpha));
    return alpha;
}

Evaluation Worker::Quiescence(int depth, SearchUtils::PlyData* ply_data, Evaluation alpha, Evaluation beta){
    assert(depth >= 0);
    assert(alpha < beta);

    const Evaluation stand_pat = Eval::EvaluateBoard(current_board);//if just chilling here leads to a good eval, assume I can just do it
    if(depth == 0){
        leaf_nodes_searched++;
        return stand_pat;
    }
    if(stand_pat >= beta){
        leaf_nodes_searched++;
        return beta;//prune
    }

    if(stand_pat > alpha){
        alpha = stand_pat;
    }

    Move move_list[MoveGenerator::MAX_CAPTURE_COUNT];
    Move* end = current_board.turn ? MoveGenerator::GenerateMain<true, true>(current_board, ply_data, move_list) : MoveGenerator::GenerateMain<false, true>(current_board, ply_data, move_list);//maybe test by & moves to enemy squares?
    int legal_move_count = end - move_list;

    MoveSorting::QSort(move_list, legal_move_count, MoveUtils::NULL_MOVE, current_board);

    for(int i=0;i<legal_move_count;i++){
        Move current_move = move_list[i];
        BoardUtils::MakeMove(current_board, current_move, ply_data);
        Evaluation curr_score = -Quiescence(depth-1, ply_data+1, -beta, -alpha);
        BoardUtils::UnMakeMove(current_board, current_move, ply_data);

        if(curr_score > alpha){
            alpha = curr_score;
        }
        if(curr_score >= beta){
            leaf_nodes_searched++;
            return beta;
        }
    }
    leaf_nodes_searched++;
    return alpha;
}

/**
 * @brief gets the pv moves, with a trailing space
 */
std::string Worker::FindPV(SearchUtils::PlyData* ply_data_for_board){
    std::vector<Move> pv_moves = {};
    std::string text_output = {};

    for(int i=0;i<20;i++){
        SearchUtils::PlyData* current_ply_data = ply_data_for_board + i;
        TTEntry entry = transposition_table.ProbeAdjusted(current_ply_data->zobrist, 0, current_ply_data->ply_from_root, Eval::START_NEGATIVE, -Eval::START_NEGATIVE);
        if(entry.score == Eval::NULL_EVAL || entry.best_move == MoveUtils::NULL_MOVE){
            break;//end of pv
        }

        pv_moves.push_back(entry.best_move);
        BoardUtils::MakeMove(current_board, entry.best_move, current_ply_data);//play out the pv
        text_output += StringTools::MoveToString(entry.best_move) + " ";
    }

    for(int i=pv_moves.size()-1; i >= 0; i--){
        SearchUtils::PlyData* current_ply_data = ply_data_for_board + i;
        BoardUtils::UnMakeMove(current_board, pv_moves[i], current_ply_data);//undo the pv we just checked
    }

    return text_output;
}

void Worker::UpdateTimer()
{
    if(end_time.NowIsPastTimePoint()){
        stop_condition.store(true);
    }
}

void Engine::StartSearch(Worker& w, int depth, uint64_t search_time_ms)
{
    for(int i=0; i<2; i++){
        for(int j=0; j<64*64; j++){
            w.history_heuristic[i][j] = 0;//reset all the history
        }
    }
    w.end_time = TimePoint(search_time_ms);
    w.RootSearch(depth, w.current_ply_before_search);
}

bool Engine::BoardIsOK(Board &board, const SearchUtils::PlyData *ply_data)
{
    std::string board_fen = StringTools::ToFEN(board, ply_data);
    Board copy_from_fen = BoardUtils::CloneBoard(board);//faster to clone current board, than create new board
    SearchUtils::PlyData copy_ply = SearchUtils::PlyData();
    StringTools::ReadFEN(board_fen, copy_from_fen, &copy_ply);

    Eval::EvaluateBoard(copy_from_fen);
    Eval::EvaluateBoard(board);

    if(!BoardUtils::CompareBoards(board, copy_from_fen)){
        return false;//to fen and then back from fen, and they are different!
    }
    return true;
}
