#pragma once

#include "Board.h"
#include "StringTools.h"
#include "MoveGenerator.h"
#include "nn_lib_helpers.h"
#include "threadsafe_io.h"
#include <sstream>
#include <cmath>
#include <format>
#include <csignal>
#include <cstdlib>
#include <atomic>
#include <cctype>
#include <algorithm>
#include <random>

#include "nlohmann/json.hpp"

namespace QuietGen {
    //structs

    #pragma pack(push,1)//prevent compiler from messing with my struct alignment
    struct DataPoint {
        DataPoint(const Board& board, float tanh_input):
        tanh_score(tanh_input)
        {
            //store colour bitboards
            colours[0] = board.colour_bitboard[0];
            colours[1] = board.colour_bitboard[1];

            //store non king bitboards
            for(Piece p = 0; p < 5; p++){// N B R Q P
                non_king_pieces[p] = board.piece_bitboard[p];
            }

            //store king positions
            for(int i=0;i<2;i++){
                SetKingPos(board.piece_bitboard[PieceUtils::KING] & board.colour_bitboard[i], i);
            }
        }
        //TODO order these better, but don't mess up the training data
        float tanh_score;
        uint8_t king_xy[2];//[colour] upper nibble is x, lower nibble is y
        Bitboard colours[2];//[colour]
        Bitboard non_king_pieces[5];//bitboards for non king pieces, indexed [base piece]

        private:
        void SetKingPos(Bitboard king_bb, bool colour){
            assert(BitboardUtils::PopCount(king_bb) == 1);
            Square king_pos = BitboardUtils::FindLSB(king_bb);
            Square x = SquareUtils::ToFile(king_pos);
            assert(x < 8);
            Square y = SquareUtils::ToRank(king_pos);
            assert(y < 8);
            king_xy[colour] = (x<<4 | y);
        }
    };
    #pragma pack(pop)

    class ShufflingBufferedWriter {
        public:
        ShufflingBufferedWriter(size_t max_elements):
        out_file("/home/stuart/ChessTrainingData/nn_training_data.bin", std::iostream::binary | std::iostream::trunc),
        write_buffer(),
        can_save_training_data(true),
        max_elements(max_elements)
        {
            write_buffer.reserve(max_elements);
            assert(write_buffer.size() == 0);
            assert(write_buffer.capacity() == max_elements);
        }

        /**
         * @warning not thread-safe with other writes
         */
        void SaveEntry(const DataPoint& dp){
            write_buffer.push_back(dp);
            if(write_buffer.size() == max_elements){
                ShuffleWrite();
            }
        }

        bool CanWrite() const {return can_save_training_data.load();}

        /**
         * @note can be called from multi_threads
         */
        void Destruct(){
            if(can_save_training_data.load() == false){
                return;//already done
            }
            can_save_training_data = false;
            ShuffleWrite();

            out_file.close();
        }

        ~ShufflingBufferedWriter(){
            Destruct();
        }

        private:

        void ShuffleWrite(){
            sync_cout << "shuffling segment" << std::endl;
            auto rng = std::default_random_engine {};

            std::ranges::shuffle(write_buffer, rng);

            out_file.write((const char*)write_buffer.data(), write_buffer.size() * sizeof(DataPoint));//don't ask. it works on my machine...

            write_buffer.clear();

            sync_cout << "done shuffling" << std::endl;

        }

        std::ofstream out_file;
        std::vector<DataPoint> write_buffer;
        std::atomic<bool> can_save_training_data;
        size_t max_elements;
    };
    

    //global variables
    std::ifstream in_file("/home/stuart/ChessTrainingData/lichess_evals.json");
    ShufflingBufferedWriter out_writer = ShufflingBufferedWriter(8'064'516);//roughly every 500MB

    Board board;

    //functions

    using json = nlohmann::json;

    static std::optional<float> GetTanhScore(const json& pv){
        if(pv.contains("mate")){
            int mate_in = pv["mate"];
            if(std::abs(mate_in) <= 4){
                return std::nullopt;//my engine should mate find an 8 ply mate, so wouldn't make good training data
            }
            return (mate_in > 0) ? 1 : -1;//really?
        }

        float cp_score = pv["cp"];
        return std::tanh(cp_score/200);
    }

    inline bool HandleJsonEntry(std::string json_data){
        json data = json::parse(json_data);

        std::string fen = data["fen"];

        SearchUtils::PlyData ply_data[2];

        for(const json& eval : data["evals"]) {
            int depth = eval["depth"];
            if(depth < 8){continue;}//too low depth

            for(const json& pv : eval["pvs"]) {
                std::optional<float> opt_wanted_output = GetTanhScore(pv);//this score is only valid when the pv has been played, as it could be a second pv that is really terrible
                if(!opt_wanted_output){
                    continue;//mate in {small number}, skip this one
                }
                float wanted_output = opt_wanted_output.value();
                assert(std::abs(wanted_output) <= 1);
                StringTools::ReadFEN(fen, board, ply_data);//reset board

                std::string uci_moves = pv["line"];
                std::istringstream uci_move_stream(uci_moves);

                std::string curr_move_str;
                int curr_move_idx=-1;//increment to start at 0
                while(uci_move_stream >> curr_move_str){
                    Move move = StringTools::MoveFromString(ply_data, curr_move_str);
                    curr_move_idx++;
                    if(!MoveGenerator::VerifyMove(board, move) || move == MoveUtils::NULL_MOVE){
                        break;//possibly corupted stockfish hash?
                    }

                    BoardUtils::MakeMove(board, move, ply_data);//play the move, so that the score becomes true
                    int piece_count = BitboardUtils::PopCount(board.colour_bitboard[0] | board.colour_bitboard[1]);
                    const bool invalid_position_for_training = ply_data[0].in_check || !PieceUtils::IsEmpty(ply_data[0].killed) || piece_count <= 5;
                    ply_data[0] = ply_data[1];//shuffle along pretend-stack

                    if(invalid_position_for_training){
                        continue;//skip this move, as in check or move was a capture
                    }
                    if(depth - curr_move_idx < 8){
                        break;//ensure that I have at least an 8 move pv
                    }

                    DataPoint data = DataPoint(board, wanted_output);
                    assert(std::abs(data.tanh_score) <= 1);
                    if(!out_writer.CanWrite()){
                        return false;//cannot write to file, stop here
                    }
                    out_writer.SaveEntry(data);
                }

            }
        }
        return true;
    }

    void signalHandler(int signal) {
        if (signal == SIGINT) {
            std::cout << "\nCtrl+C detected. Exiting gracefully...\n";
            out_writer.Destruct();//prevent writing to file while I close it
            in_file.close();

            std::exit(0); // Exit the program
        }
    }
};