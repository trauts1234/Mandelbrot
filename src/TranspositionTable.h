#pragma once
#include <cstdint>
#include "Util.h"
#include "Eval.h"
#include "threadsafe_io.h"

/**
 * @brief this enum stores what type of score a position has
 */
enum TTLookupType{
    UPPERBOUND = 1,//Alpha result
    LOWERBOUND = 2,//Beta result
    EXACT = UPPERBOUND | LOWERBOUND,//Exact result is both an upper and lower bound
};

/**
 * @brief this struct is an entry to the transposition table, containing data about the position
 * @warning TT entries may clash often. It is best to check the full zobrist hash of this entry with your current board
 */
struct TTEntry {
    uint64_t zobrist_hash = 0;
    /// @brief Is the bestmove calculated so far?
    /// @warning check this move is legal before trying to play it!
    Move best_move = MoveUtils::NULL_MOVE;
    //how far further did the creator of this entry search into the tree
    int subtree_depth = 0;
    
    //what type of score has been stored
    TTLookupType score_type;
    //the score calculated when this entry was inserted
    Evaluation score = Eval::NULL_EVAL;
};

class TranspositionTable {
    public:

    TranspositionTable(int size_mb){
        Resize(size_mb);
    }
    TranspositionTable(const TranspositionTable&) = delete;
    TranspositionTable& operator=(const TranspositionTable&) = delete;
    
    ~TranspositionTable(){
        delete[] entries;
    }

    void Resize(int MB_size);

    /**
     * @brief stores the entry in the transposition table.
     * The table uses a MOD based indexing method to overwrite and store entries
     * @param entry the entry that is to be put in
     */
    void Set(TTEntry entry);

    /**
     * @brief gets the evaluation, if applicable, of the zobrist score
     * @param zobrist the current zobrist hash, to search for
     * @param requested_ply_to_leaves how far from the leaf nodes do you want a result for
     * @param ply_from_root for adjusting mate scores, how far you are from the start of the search
     * @param alpha the alpha currently
     * @param beta the beta currently
     * @returns the entry with the evaluation: NULL_EVAL or an evaluation to return immediately, 
     * move will always be NULL or a good suggested move
     */
    TTEntry ProbeAdjusted(uint64_t zobrist, int requested_ply_to_leaves, int ply_from_root, Evaluation alpha, Evaluation beta) const;

    TTEntry ProbeUnadjusted(uint64_t zobrist) const;

    //getters for statistics:
    /**
     * @brief calculates an approximation of how full the table is out of 1000
     * @returns an integer between 0 and 1000 describing how full the table is
     */
    int CalculatePerMilFull() const;

    void ClearTable();
    
    private:

    uint64_t number_of_entries;
    TTEntry *entries;
};

namespace TranspositionUtils
{
    /**
     * @brief generates a transposition table entry based on data from the current search
     * @param zobrist the zobrist hash of the current position
     * @param best the best move in the current position
     * @param ply_to_leaves how many ply deeper was this search carried out to? (usually just the depth variable)
     * @param score_type is the evaluation result exact, an upper or lower bound?
     * @param score what score is this position
     */
    TTEntry GenerateEntry(uint64_t zobrist, Move best, int ply_to_leaves, TTLookupType score_type, Evaluation score);
} // namespace TranspositionUtils
