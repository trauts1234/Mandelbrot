#include "TranspositionTable.h"

inline uint64_t CalculateEntryIndex(uint64_t zobrist_hash, uint64_t num_entries){return zobrist_hash % num_entries;}

TTEntry TranspositionUtils::GenerateEntry(uint64_t zobrist, Move best, int ply_to_leaves, TTLookupType score_type, Evaluation score)
{
    return TTEntry{
		zobrist,
		best,
		ply_to_leaves,
		score_type,
		score
	};
}

void TranspositionTable::Resize(int MB_size)
{
	constexpr uint64_t BYTES_IN_MB = 1024 * 1024;//1 MB is this many bytes
	constexpr uint64_t ENTRIES_IN_MB = BYTES_IN_MB / sizeof(TTEntry);//this many entries fit in 1MB

	number_of_entries = ENTRIES_IN_MB * MB_size;//set number of entries
	delete[] entries;//remove old entries
	entries = new TTEntry[number_of_entries];//allocate new entries
	ClearTable();
}

void TranspositionTable::Set(TTEntry entry)
{
	entries[CalculateEntryIndex(entry.zobrist_hash, number_of_entries)] = entry;
}

inline Evaluation AdjustEval(Evaluation eval, TTLookupType eval_type, int ply_from_root, Evaluation alpha, Evaluation beta)
{
    if(eval >= Eval::FURTHEST_MATE){
		//white is going to mate
		eval -= ply_from_root; // the mate score is mate - depth, and the stored result is corrected to (ply from leaves), so we further adjust by ply from root, to get mate in (total tree depth)
	} else if (eval <= -Eval::FURTHEST_MATE){
		//black is going to mate
		eval += ply_from_root;// ''
	}

	switch(eval_type){
		case TTLookupType::EXACT:
			return eval;
		case TTLookupType::UPPERBOUND:
			if(eval <= alpha) return alpha;
			break;
		case TTLookupType::LOWERBOUND:
			if(eval >= beta) return beta;
			break;
	}
	return Eval::NULL_EVAL;
}

TTEntry TranspositionTable::ProbeAdjusted(uint64_t zobrist, int requested_ply_to_leaves, int ply_from_root, Evaluation alpha, Evaluation beta) const
{
    TTEntry ans = entries[CalculateEntryIndex(zobrist, number_of_entries)];
	if(ans.zobrist_hash != zobrist || ans.score == Eval::NULL_EVAL){
		//lookup failed
		return TTEntry();//null
	}

	//adjust mate scores to be correct distance from root, and fix upper and lower bounds logic
	ans.score = AdjustEval(ans.score, ans.score_type, ply_from_root, alpha, beta);

	if(ans.subtree_depth < requested_ply_to_leaves){
		//not deep enough search
		ans.score = Eval::NULL_EVAL;
	}
	
	return ans;

}

TTEntry TranspositionTable::ProbeUnadjusted(uint64_t zobrist) const
{
    TTEntry ans = entries[CalculateEntryIndex(zobrist, number_of_entries)];
	if(ans.zobrist_hash != zobrist || ans.score == Eval::NULL_EVAL){
		//lookup failed
		return TTEntry();//null
	}
	
	return ans;
}

int TranspositionTable::CalculatePerMilFull() const
{
    int ans = 0;
	uint64_t step_size = number_of_entries/1000;
	for(int i=0;i<1000;i++){
		uint64_t index = step_size * i;
		if(entries[index].score != Eval::NULL_EVAL){
			ans++;
		}
	}
	return ans;
}

void TranspositionTable::ClearTable()
{
	for(uint64_t i=0;i<number_of_entries;i++){
		entries[i].score = Eval::NULL_EVAL;//set each element to null
	}
}
