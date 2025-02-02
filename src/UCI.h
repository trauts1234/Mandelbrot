#pragma once
#include <string>
#include <thread>
#include "Board.h"
#include "Timer.h"
#include <optional>
#include "TranspositionTable.h"
#include "Engine.h"

namespace UCI{

enum Command{
    NO_COMMAND,
    UCI,
    DEBUG,
    ISREADY,
    QUIT,
    SETOPTION,
    REGISTER,
    UCINEWGAME,
    POSITION,
    GO,
    STOP,
    PONDERHIT,
    STATIC_EVAL,
};

template<typename T>
struct UCISpinOption{
    UCISpinOption(std::string name, T max_val, T min_val, T default_val): name(name), max_value(max_val), min_value(min_val), default_value(default_val), current_value(default_val) {}

    const std::string name;
    const T max_value;
    const T min_value;
    const T default_value;
    T current_value;
};

struct Context{
    Context() = default;

    UCISpinOption<int> hash_size_mb = UCISpinOption<int>("Hash", INT_MAX, 1, 64);

    std::optional<std::thread> searcher_thread = std::nullopt;
    SearchLimits operation;
    std::atomic<bool> stop_flag = {true};

    Worker worker = Worker(stop_flag, hash_size_mb.current_value);
};

/// @brief handles the engine based on the inputted command. you need to stop the program yourself if the command is "quit" though
/// @param command the full message, with \r\n removed
void HandleMessage(const std::string& message, Context& ctx);

/**
 * @brief loads PSQT values
 */
void Init();

}