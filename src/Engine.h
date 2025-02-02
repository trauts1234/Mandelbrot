#pragma once

#include "Board.h"

#include <atomic>
#include <immintrin.h> //vomit-inducing magic for speedy code
#include <bit>//            ''
#include "Eval.h"
#include "TranspositionTable.h"
#include "Timer.h"

class Worker {
    public:

    enum NodeType{
        ROOT,
        NORMAL
    };
    
    Board current_board;
    std::atomic<bool> &stop_condition;
    TimePoint end_time;
    TranspositionTable transposition_table;
    int history_heuristic[2][64*64];//[for each turn][from square + to square*64]

    SearchUtils::PlyData current_board_search_stack[BoardUtils::MAX_GAME_LENGTH];
    SearchUtils::PlyData* current_ply_before_search;

    uint64_t leaf_nodes_searched = 0;

    Worker(std::atomic<bool> &stop_cond, int initial_hash_size): 
    current_board(), stop_condition(stop_cond), transposition_table(initial_hash_size), history_heuristic(), current_board_search_stack(), current_ply_before_search(current_board_search_stack) {}

    void RootSearch(int max_depth, SearchUtils::PlyData* ply_data);

    private:

    template<NodeType node_type>
    Evaluation NegaMax(int depth, SearchUtils::PlyData *ply_data, Evaluation alpha, Evaluation beta, int previous_extensions, bool allow_null);
    Evaluation Quiescence(int depth, SearchUtils::PlyData* ply_data, Evaluation alpha, Evaluation beta);
    std::string FindPV(SearchUtils::PlyData* ply_data_for_board);
    void UpdateTimer();
};

namespace Engine
{

constexpr int MAX_SEARCH_DEPTH = 30;
constexpr int QUIESCENCE_DEPTH = 6;

/**
 * @brief the main search function
 * @param worker the worker to call
 * @param depth the approximate depth to search to
 */
void StartSearch(Worker& w, int depth, uint64_t time_limit_ms);

bool BoardIsOK(Board& board, const SearchUtils::PlyData* ply_data);
} // namespace Engine
