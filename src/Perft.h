#pragma once

#include "Board.h"
#include <atomic>

namespace PerftEngine
{
constexpr int MAX_SEARCH_DEPTH = 30;
constexpr int QUIESCENCE_DEPTH = 6;

void StartPerft(Board &board, int depth, SearchUtils::PlyData* ply_data, const std::atomic<bool> &stop_condition);

} // namespace Engine