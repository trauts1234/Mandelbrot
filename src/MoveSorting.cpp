#include "MoveSorting.h"
#include "Board.h"
#include "MoveGenerator.h"
#include <cassert>
#include <algorithm>

//borrowed from rustic chess engine:
constexpr int MVV_LVA[5][6] = {
//N  B   R   Q   P   K (attacker)
{24, 23, 22, 21, 25, 20},//victim: Knight
{34, 33, 32, 31, 35, 30},//victim: Bishop
{44, 43, 42, 41, 45, 40},//victim: Rook
{54, 53, 52, 51, 55, 50},//victim: Queen
{14, 13, 12, 11, 15, 10},//victim: Pawn
};

int CalculateMoveValue(Move move, const Board& board){
    const Piece captured = board.squares[MoveUtils::ToSquare(move)];
    const Piece attacking = board.squares[MoveUtils::FromSquare(move)];

    assert(!PieceUtils::IsEmpty(captured));
    assert(!PieceUtils::IsEmpty(attacking));

    const Piece captured_base = PieceUtils::BasePiece(captured);
    const Piece attacking_base = PieceUtils::BasePiece(attacking);

    const int result = MVV_LVA[captured_base][attacking_base];
    return result;
}

int MoveSorting::CalculateNewHistory(int old_history, int depth)
{
    constexpr int MAX_HISTORY = 200'000'000;
    constexpr int MIN_HISTORY = -MAX_HISTORY;
    const int bonus = std::clamp(depth*depth, MIN_HISTORY, MAX_HISTORY);//nodes far from leaves less likely so need a bigger weight to counterbalance
    return old_history + bonus - (old_history * bonus) / MAX_HISTORY;//try this on desmos. It really clamps it between min and max history!
}

void MoveSorting::CalculateMoveValues(int *score_list, const Move move_list[MoveGenerator::MAX_MOVE_COUNT], int moves_count, Move expected_best_move, const Board &board, const SearchUtils::PlyData *ply_data, const int this_side_history[64*64])
{
    assert(moves_count <= MoveGenerator::MAX_MOVE_COUNT);
    constexpr int PV_BONUS =      900'000'000;
    constexpr int CAPTURE_BONUS = 800'000'000;
    constexpr int KILLER_BONUS =  700'000'000;

    //score moves
    for(int i=0;i<moves_count;i++){
        const Move current = move_list[i];
        if(current == expected_best_move){
            score_list[i] = PV_BONUS;//PV is very good
            continue;
        }

        const Piece captured = board.squares[MoveUtils::ToSquare(current)];
        if(!PieceUtils::IsEmpty(captured)){
            score_list[i] = CalculateMoveValue(move_list[i], board) + CAPTURE_BONUS;
            continue;
        }

        if(current == ply_data->killer_move){
            score_list[i] = KILLER_BONUS;
            continue;
        }

        score_list[i] = this_side_history[CalculateHistoryIndex(current)];
    }
}

Move MoveSorting::SortNext(int *score_list, Move *move_list, int moves_count, int next_to_sort)
{
    assert(moves_count <= MoveGenerator::MAX_CAPTURE_COUNT);
    int index_of_biggest = next_to_sort;
    int score_of_biggest = score_list[index_of_biggest];
    for (int j = next_to_sort + 1; j < moves_count; j++) {
        if (score_list[j] > score_of_biggest) {
            index_of_biggest = j;
            score_of_biggest = score_list[j];
        }
    }

    assert(index_of_biggest >= next_to_sort);
    // Swap the found minimum element with the first element of the unsorted part
    std::swap(score_list[next_to_sort], score_list[index_of_biggest]);
    std::swap(move_list[next_to_sort], move_list[index_of_biggest]);//swap moves also

    return move_list[next_to_sort];//return the now-sorted item
}

void MoveSorting::QSort(Move move_list[MoveGenerator::MAX_CAPTURE_COUNT], int moves_count, Move expected_best_move, const Board &board)
{
    assert(moves_count <= MoveGenerator::MAX_CAPTURE_COUNT);
    constexpr int PV_BONUS = 900'000'000;
    int move_scores[MoveGenerator::MAX_CAPTURE_COUNT];

    //score moves
    for(int i=0;i<moves_count;i++){
        Move current = move_list[i];
        if(current == expected_best_move){
            move_scores[i] = PV_BONUS;//PV is very good
            continue;
        }
        //Piece captured = board.squares[MoveUtils::ToSquare(move_list[i])];
        move_scores[i] = CalculateMoveValue(current, board);
    }

    //sort move_list by move_scores

    for (int next_to_sort = 0; next_to_sort < moves_count - 1; next_to_sort++) {//start at front and continue
        // Find the minimum element in the unsorted part
        int index_of_biggest = next_to_sort;
        for (int j = next_to_sort + 1; j < moves_count; j++) {
            if (move_scores[j] > move_scores[index_of_biggest]) {
                index_of_biggest = j;
            }
        }
        // Swap the found minimum element with the first element of the unsorted part
        if (index_of_biggest != next_to_sort) {
            std::swap(move_scores[next_to_sort], move_scores[index_of_biggest]);
            std::swap(move_list[next_to_sort], move_list[index_of_biggest]);//swap moves also
        }
    }
}
