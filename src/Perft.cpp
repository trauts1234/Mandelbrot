#include "Perft.h"
#include "threadsafe_io.h"
#include "Timer.h"
#include "Board.h"
#include "Util.h"
#include "MoveGenerator.h"
#include "StringTools.h"
#include <chrono>
#include <atomic>

template<int depth, bool extra_logging>
unsigned long long Perft(Board &board, SearchUtils::PlyData* ply_data, const std::atomic<bool>& stop_condition)
{   
    static_assert(depth >= 0);
    assert(ply_data->zobrist == BoardUtils::CalculateZobristHash(board, ply_data));

    if(stop_condition.load()){ //reading this atomic actually makes the search faster?? Very confused. +10Mnps
        sync_dbg << "stopped perft test early" << std::endl;
        return 0;
    }

    Move move_list[MoveGenerator::MAX_MOVE_COUNT];
    Move* end = board.turn ? MoveGenerator::GenerateMain<true, false>(board,ply_data, move_list) : MoveGenerator::GenerateMain<false, false>(board,ply_data, move_list);
    int legal_move_count = end - move_list;

    if constexpr(depth==1){
        return legal_move_count;
    }

    unsigned long long total = 0;

    for(int move_num=0;move_num<legal_move_count;move_num++){
        Move current_move = move_list[move_num];
        #ifndef NDEBUG//hides a warning, as in release I don't compare this copy
        Board temp_copy = BoardUtils::CloneBoard(board);
        #endif
        BoardUtils::MakeMove(board, current_move, ply_data);

        unsigned long long inc=0;
        if constexpr(depth > 1){
            inc = Perft<depth-1, false>(board,ply_data+1, stop_condition);
        }
        total += inc;
        BoardUtils::UnMakeMove(board, current_move, ply_data);
        assert(BoardUtils::CompareBoards(board, temp_copy));
        assert(ply_data->zobrist == BoardUtils::CalculateZobristHash(board, ply_data));
        if constexpr(extra_logging){
            sync_cout << StringTools::MoveToString(current_move) << ": " << inc << std::endl;
            //sync_dbg << StringTools::ToFEN(board) << std::endl;
        }
    }

    return total;
}

unsigned long long SwitchDepthPerft(Board &board, int depth, SearchUtils::PlyData* ply_data, const std::atomic<bool> &stop_condition){
    assert(depth < 9);
    //shh... it makes it faster... +20Mnps
    switch(depth){
        case 1: return Perft<1, true>(board, ply_data, stop_condition);
        case 2: return Perft<2, true>(board, ply_data, stop_condition);
        case 3: return Perft<3, true>(board, ply_data, stop_condition);
        case 4: return Perft<4, true>(board, ply_data, stop_condition);
        case 5: return Perft<5, true>(board, ply_data, stop_condition);
        case 6: return Perft<6, true>(board, ply_data, stop_condition);
        case 7: return Perft<7, true>(board, ply_data, stop_condition);
        case 8: return Perft<8, true>(board, ply_data, stop_condition);
        default: throw std::invalid_argument("Unsupported depth");
    }
}

void PerftEngine::StartPerft(Board &board, int depth, SearchUtils::PlyData* ply_data, const std::atomic<bool> &stop_condition)
{
    assert(ply_data->zobrist == BoardUtils::CalculateZobristHash(board, ply_data));
    TimePoint start_time = TimePoint();

    unsigned long long nodes;

    nodes = SwitchDepthPerft(board, depth, ply_data, stop_condition);

    uint64_t time_taken = start_time.HowLongAgo();
    sync_cout << time_taken << "ms" << std::endl;

    int Mnps=0;
    if(time_taken != 0){
        Mnps = (nodes/time_taken)/1'000;
    }
    sync_cout << Mnps << "Mnps" << std::endl;

    sync_cout << "Nodes searched: " << nodes << std::endl;
}