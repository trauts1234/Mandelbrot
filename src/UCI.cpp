#include "UCI.h"

#include "threadsafe_io.h"
#include "Util.h"
#include "Eval.h"
#include "StringHandling.h"
#include "Timer.h"
#include "Engine.h"
#include "Perft.h"

#include <unordered_map>
#include <vector>
#include <optional>
#include "StringTools.h"
#include <cassert>

void UCI::Init(){

}

void ParseGoCommand(const std::string &operand, UCI::Context& ctx){
    //these flags take priority over other settings
    bool perft = false;

    std::vector<std::string> searchmoves;
    int myside_time=INT_MAX, myside_increment=0, movetime=INT_MAX;
    int depth=INT_MAX, _nodes_limit=INT_MAX;

    std::istringstream stream(operand);
    std::string token;
    bool turn = ctx.worker.current_board.turn;

    while (stream >> token) {
        if (token == "searchmoves") {
            searchmoves.clear();
            while (stream >> token && StringTools::IsMove(token)) {
                searchmoves.push_back(token);
            }
            if (!stream.eof()) stream.putback(' ');  // Reset stream if a keyword is encountered
            stream.seekg(-static_cast<int>(token.size()), std::ios_base::cur);
        } 
        else if (token == "perft"){
            perft = true;
            stream >> depth;
            break;
        }
        //I love chatGPT for this mess:
        else if (token == "wtime" && turn) stream >> myside_time;
        else if (token == "btime" && !turn) stream >> myside_time;
        else if (token == "winc" && turn) stream >> myside_increment;
        else if (token == "binc" && !turn) stream >> myside_increment;
        else if (token == "depth" || token == "mate") stream >> depth;
        else if (token == "nodes") stream >> _nodes_limit;
        else if (token == "movetime") stream >> movetime;
    }
    ctx.operation = {};

    ctx.operation.search_depth = depth;

    if(perft){
        ctx.operation.search_type = PERFT;
        return;
    }

    ctx.operation.search_time_ms = std::min(
        movetime,
        myside_time/20 + myside_increment/2
    );
}

UCI::Command ParseCommand(const std::string& command){
    std::unordered_map<std::string, UCI::Command> command_mappings = {
        {"uci", UCI::UCI},
        {"quit", UCI::QUIT},
        {"debug", UCI::DEBUG},
        {"isready", UCI::ISREADY},
        {"setoption", UCI::SETOPTION},
        {"ucinewgame", UCI::UCINEWGAME},
        {"position", UCI::POSITION},
        {"go", UCI::GO},
        {"stop", UCI::STOP},
        {"ponderhit", UCI::PONDERHIT},
        {"static", UCI::STATIC_EVAL},
    };
    if(!command_mappings.contains(command)){
        return UCI::NO_COMMAND;
    }
    return command_mappings.at(command);
}

void ParsePositionCommand(const std::string& operand, UCI::Context& ctx){
    auto [pre_moves, moves_str] = StringHandling::SplitTwoParts(operand, " moves ");
    std::string position_fen;
    if(pre_moves == "startpos"){
        position_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    } else {
        //position fen ...
        position_fen = pre_moves.substr(4);
    }
    ctx.worker.current_ply_before_search = ctx.worker.current_board_search_stack;//reset pointer to start

    StringTools::ReadFEN(position_fen, ctx.worker.current_board, ctx.worker.current_ply_before_search);

    for(std::string move : StringHandling::SplitBySpace(moves_str)){
        Move to_play = StringTools::MoveFromString(ctx.worker.current_ply_before_search, move);
        //play the UCI move
        BoardUtils::MakeMove(ctx.worker.current_board, to_play, ctx.worker.current_ply_before_search);
        //ctx.current_data->current_move = to_play;//save the played move like it has been searched
        ctx.worker.current_ply_before_search++;//go to next move down in stack, as it has been filled by the make move code

        //Board copy;
        //bool copy_turn;//position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 moves e2e4 b8c6 d2d4 a8b8 g1f3 g8f6 b1c3 c6b4 f1c4 b4c2 d1c2 f6e4 c3e4 d7d5 c2a4 b7b5 a4a7 b5c4 a7b8 d5e4 b8b5 c8d7 b5c4 e4f3 g2f3 f7f6 c1f4 e7e5 d4e5 g7g5 f4g3 d8a8 e5f6 a8a5 b2b4 a5a8 f6f7 e8d8 a1c1
        //StringTools::ReadFEN(StringTools::ToFEN(ctx.current_board, ctx.next_to_move), copy, copy_turn);
        //sync_dbg << "board is equal to its fen string: " << BoardUtils::CompareBoards(ctx.current_board,ctx.next_to_move, copy, copy_turn) << std::endl;
    }
}

void Stop(UCI::Context& ctx){
    ctx.stop_flag.store(true);
    //wait for threads to stop and clean up
    if(ctx.searcher_thread.has_value() && ctx.searcher_thread.value().joinable()){
        ctx.searcher_thread.value().join();
        ctx.searcher_thread.reset();
    }
    ctx.stop_flag.store(false);
}

/// @brief the main external function
void UCI::HandleMessage(const std::string& message, Context& ctx)
{
    sync_dbg << message << std::endl;
    auto [command, operand] = StringHandling::SplitTwoParts(message);

    switch (ParseCommand(command))
    {
    case UCI:
        sync_cout << "id name Mandelbrot\n" << "id author Stu\n"
        << "option name " << ctx.hash_size_mb.name << " type spin default " << ctx.hash_size_mb.default_value << " min " << ctx.hash_size_mb.min_value << " max " << ctx.hash_size_mb.max_value << "\n"
         << "uciok" << std::endl;
        return;

    case DEBUG:
        if(operand == "on"){

        } else{

        }
        return;

    case ISREADY:
        sync_cout << "readyok" << std::endl;
        return;

    case SETOPTION:
        {
            size_t value_pos = operand.find(" value ");

            std::string name = operand.substr(5, value_pos - 5);
            std::string new_value = operand.substr(value_pos + 7);

            if(name == ctx.hash_size_mb.name){
                ctx.hash_size_mb.current_value = std::stoi(new_value);
                ctx.worker.transposition_table.Resize(ctx.hash_size_mb.current_value);
            }
        }
        return;
    
    case UCINEWGAME:
        //Init();//no need to init here?
        ctx.worker.transposition_table.ClearTable();//reset all values in tt to null
        return;
    
    case POSITION:
        ParsePositionCommand(operand, ctx);
        return;

    case GO:
        Stop(ctx);//reset some stuff
        ParseGoCommand(operand, ctx);
        
        //ensure my pointers not messed up
        assert(ctx.worker.current_ply_before_search - ctx.worker.current_board_search_stack >= 0);
        assert(ctx.worker.current_ply_before_search - ctx.worker.current_board_search_stack < BoardUtils::MAX_GAME_LENGTH);

        switch (ctx.operation.search_type)
        {
        case DEFAULT:
            ctx.searcher_thread.emplace(std::thread(Engine::StartSearch, std::ref(ctx.worker), ctx.operation.search_depth, ctx.operation.search_time_ms));
            break;
        case PERFT:
            ctx.searcher_thread.emplace(std::thread(PerftEngine::StartPerft, std::ref(ctx.worker.current_board), ctx.operation.search_depth, std::ref(ctx.worker.current_ply_before_search), std::ref(ctx.stop_flag)));
            break;
        }

        return;
    
    case STOP:
        Stop(ctx);
        return;
    
    case PONDERHIT:
        //go to full search
        return;
    
    case STATIC_EVAL:
        sync_cout << Eval::EvaluateBoard(ctx.worker.current_board) << std::endl;
        return;

    case NO_COMMAND:
        sync_dbg << "invalid command:" << command << ", skipping it." << std::endl;
        return;
    
    case REGISTER:
        return;//this program is free
        
    case QUIT:
        Stop(ctx);
        return;
    }
}