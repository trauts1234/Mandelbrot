#include "StringTools.h"

#include <string>

#include "Util.h"
#include "Castling.h"
#include <unordered_map>
#include "threadsafe_io.h"
#include <limits.h>
#include <unistd.h>
#include <format>
#include "Eval.h"

void StringTools::ReadFEN(std::string fen, Board& board, SearchUtils::PlyData* ply_data)
{
    std::istringstream iss(fen);

    std::string piece_placement;
    std::string active_colour;
    std::string castling_rights;
    std::string enpessant_square;
    std::string fifty_move_rule;
    std::string full_moves;

    iss >> piece_placement >> active_colour >> castling_rights >> enpessant_square >> fifty_move_rule >> full_moves;

    // Reset the squares to empty
    for(Square i=0;i<64;i++){
        board.squares[i] = PieceUtils::EMPTY;
    }
    for(int i=0;i<2;i++){
        board.colour_bitboard[i] = 0;
    }
    for(int i=0;i < 6;i++){
        board.piece_bitboard[i] = 0;
    }
    board.neural_net.ResetEfficientUpdate();//clear all squares on the neural network
    

    // FEN strings start at A8
    Square file = 0;
    Square rank = 7;

    for (char c : piece_placement) {
        if (c == '/') {//next rank
            file=0;
            rank--;
            continue;
        }

        if (std::isdigit(c)) {
            // If it's a number, skip that many squares
            file += c - '0';
        } else {
            // Place the piece on the board and bitboard
            Piece curr_piece = FromLetter(c);
            Square curr_square = SquareUtils::FromCoords(file, rank);
            Bitboard curr_piece_bb = BitboardUtils::MakeBitBoard(curr_square);

            board.piece_bitboard[PieceUtils::BasePiece(curr_piece)] |= curr_piece_bb;
            board.colour_bitboard[PieceUtils::IsWhite(curr_piece)] |= curr_piece_bb;
            board.squares[curr_square] = curr_piece;
            board.neural_net.AddPiece(curr_square, curr_piece);
            file++;
        }
    }

    /*for(int i=0;i<4;i++){
        board.castling[i] = false;
    }*/
    ply_data->castling_rights = 0;

    for(char c : castling_rights){
        switch (c)
        {
        case 'K':
            //board.castling[CastlingUtils::WK_CASTLE] = true;
            ply_data->castling_rights |= CastlingUtils::GenerateCastleMask(CastlingUtils::WK_CASTLE);
            break;
        case 'Q':
            //board.castling[CastlingUtils::WQ_CASTLE] = true;
            ply_data->castling_rights |= CastlingUtils::GenerateCastleMask(CastlingUtils::WQ_CASTLE);
            break;
        case 'k':
            //board.castling[CastlingUtils::BK_CASTLE] = true;
            ply_data->castling_rights |= CastlingUtils::GenerateCastleMask(CastlingUtils::BK_CASTLE);
            break;
        case 'q':
            //board.castling[CastlingUtils::BQ_CASTLE] = true;
            ply_data->castling_rights |= CastlingUtils::GenerateCastleMask(CastlingUtils::BQ_CASTLE);
            break;
        
        default:
            break;
        }
    }

    board.turn = active_colour == "w";

    ply_data->enpessant = FromString(enpessant_square);

    if(!fifty_move_rule.empty()){
        ply_data->fifty_move_rule = std::stoi(fifty_move_rule);
    }

    ply_data->zobrist = BoardUtils::CalculateZobristHash(board, ply_data);
}

std::string StringTools::ToFEN(const Board &board, const SearchUtils::PlyData* ply_data)
{
    std::ostringstream output;

    int empty_count = 0;
    for (int rank = 7; rank >= 0; --rank) {
        for (int file = 0; file < 8; ++file) {
            int index = SquareUtils::FromCoords(file, rank);
            Piece current_piece = board.squares[index];

            if (PieceUtils::IsEmpty(current_piece)) {
                empty_count++;//count for the number of empty squares
                continue;
            }

            if (empty_count > 0) {
                output << empty_count;
                empty_count = 0;
            }
            output << ToLetter(current_piece);
        }
        
        if (empty_count > 0) {
            output << empty_count;
            empty_count = 0;
        }
        if (rank > 0) {
            output << '/'; // Add rank separator
        }
    }

    // 2. Active color
    output << ' ' << (board.turn ? 'w' : 'b');

    // 3. CastlingUtils rights
    std::string castling;
    if (CastlingUtils::HasCastling<CastlingUtils::WK_CASTLE>(ply_data->castling_rights)) castling += 'K';
    if (CastlingUtils::HasCastling<CastlingUtils::WQ_CASTLE>(ply_data->castling_rights)) castling += 'Q';
    if (CastlingUtils::HasCastling<CastlingUtils::BK_CASTLE>(ply_data->castling_rights)) castling += 'k';
    if (CastlingUtils::HasCastling<CastlingUtils::BQ_CASTLE>(ply_data->castling_rights)) castling += 'q';
    output << ' ' << (castling.empty() ? "-" : castling);

    // 4. En passant target square
    output << ' ' << (SquareUtils::IsValid(ply_data->enpessant) ? SquareToString(ply_data->enpessant) : "-");

    // 5. fifty move rule
    output << ' ' << ply_data->fifty_move_rule;

    // 6. fullmove number is not counted
    output << ' ' << "0";

    return output.str();
}

std::string StringTools::CreateNNVectorInput(const Board &board)
{
    std::string result;

    for(Piece p=0;p<=PieceUtils::MAX_NUM;p++){
        for(Square sq=0;sq<64;sq++){
            if(board.squares[sq] == p){
                result += "1,";
            } else{
                result += "0,";
            }
        }
    }
    return result.substr(0,result.size()-1);//remove trailing ","
}

Piece StringTools::FromLetter(char letter)
{
    std::unordered_map<char, Piece> lowercase_mappings = {
        {'r', PieceUtils::ROOK}, {'n', PieceUtils::KNIGHT}, {'b', PieceUtils::BISHOP}, {'q', PieceUtils::QUEEN}, {'k', PieceUtils::KING}, {'p', PieceUtils::PAWN}
    };

    assert(lowercase_mappings.contains(tolower(letter)));

    Piece col = isupper(letter) ? PieceUtils::WHITE_FLAG : 0;
    Piece uncoloured_piece = lowercase_mappings.at(tolower(letter));

    return uncoloured_piece + col;
}

Square StringTools::FromString(std::string sq)
{
    if(sq == "-"){
        return SquareUtils::NULL_SQUARE;
    }

    Square file = sq[0] - 'a';

    Square rank = sq[1] - '1';

    assert(rank < 8);
    assert(file < 8);

    return SquareUtils::FromCoords(file, rank);
}

Move StringTools::MoveFromString(const SearchUtils::PlyData* ply_data, std::string move)
{
    //in case there is a promotion
    Piece promotion = PieceUtils::EMPTY;
    if(move.length() == 5){
        promotion = FromLetter(tolower(move[4]));
    } else {
        assert(move.length() == 4);
    }

    Square from = FromString(move.substr(0,2));
    Square to = FromString(move.substr(2,2));

    unsigned int castling = CastlingUtils::NO_CASTLE;
    if(CastlingUtils::W_START_SQ == from){
        if(to == CastlingUtils::WK_END_SQ && CastlingUtils::HasCastling<CastlingUtils::WK_CASTLE>(ply_data->castling_rights)){
            castling = CastlingUtils::WK_CASTLE;
        }
        if(to == CastlingUtils::WQ_END_SQ && CastlingUtils::HasCastling<CastlingUtils::WQ_CASTLE>(ply_data->castling_rights)){
            castling = CastlingUtils::WQ_CASTLE;
        }
    }
    if(CastlingUtils::B_START_SQ == from){
        if(to == CastlingUtils::BK_END_SQ && CastlingUtils::HasCastling<CastlingUtils::BK_CASTLE>(ply_data->castling_rights)){
            castling = CastlingUtils::BK_CASTLE;
        }
        if(to == CastlingUtils::BQ_END_SQ && CastlingUtils::HasCastling<CastlingUtils::BQ_CASTLE>(ply_data->castling_rights)){
            castling = CastlingUtils::BQ_CASTLE;
        }
    }
    return MoveUtils::MakeMove(from, to, promotion, castling);
}

bool StringTools::IsMove(const std::string &text)
{
    switch(text.length()){
        case 4:
            break;//all good
        case 5:
            if(!isalpha(text[4])){return false;}//ensure the last letter is a promotion piece
            break;
        default:
            return false;//wrong number of letters
    }

    //rank numbers need to be present
    if(!isdigit(text[0])){return false;}
    if(!isdigit(text[2])){return false;}
    //file letters need to be present
    if(!isalpha(text[1])){return false;}
    if(!isalpha(text[3])){return false;}
    return true;
}

std::string StringTools::ToLetter(Piece p)
{
    if(PieceUtils::IsEmpty(p)){
        return "";
    }
    std::unordered_map<Piece, char> lowercase_mappings = {
        {PieceUtils::ROOK, 'r'}, {PieceUtils::KNIGHT, 'n'}, {PieceUtils::BISHOP, 'b'}, {PieceUtils::QUEEN, 'q'}, {PieceUtils::KING, 'k'}, {PieceUtils::PAWN, 'p'}
    };

    assert(lowercase_mappings.contains(PieceUtils::BasePiece(p)));

    char ans = lowercase_mappings.at(PieceUtils::BasePiece(p));

    if(PieceUtils::IsWhite(p)){
        return std::string(1,toupper(ans));
    }
    return std::string(1,ans);
}

std::string StringTools::SquareToString(Square sq)
{
    if(!SquareUtils::IsValid(sq)){return "NULL";}

    char rank = '1' + SquareUtils::ToRank(sq);
    char file = 'a' + SquareUtils::ToFile(sq);
    return std::string() + file + rank;
}

std::string StringTools::MoveToString(Move m)
{
    if(m == MoveUtils::NULL_MOVE){
        return "NULL";
    }
    return 
        SquareToString(MoveUtils::FromSquare(m)) + 
        SquareToString(MoveUtils::ToSquare(m)) + 
        ToLetter(MoveUtils::PromotionBase(m));
}

std::string StringTools::ScoreToString(Evaluation eval)
{
    bool is_mate = std::abs(eval) >= Eval::FURTHEST_MATE;//score is so big, it must be a mate score

    if(is_mate){
        int mate_ply = Eval::CHECKMATE_WIN - std::abs(eval);//how far is the score from mate in 0? that is the mate ply
        int side_winning = eval > 0 ? 1 : -1;//switch sign based on who is winning (positive for me about to win)
        int adjusted_mate_moves = side_winning * (mate_ply+1)/2;//UCI demands moves, not ply (+1 helps with rounding errors)
        return std::format("mate {}", adjusted_mate_moves);
    } else {
        return std::format("cp {}", eval);
    }
}

std::string StringTools::BitboardToString(Bitboard b)
{
    std::string ans = "";
    for(int rank=7;rank >= 0;rank--){
        for(int file=0;file<8;file++){
            Square curr_sq = SquareUtils::FromCoords(file,rank);
            ans += BitboardUtils::ContainsBit(b, curr_sq) ? "1" : "0";
            ans += " ";
        }
        ans += "\n";
    }
    return ans;
}

std::string StringTools::GetExecutablePath() {
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len == -1) return std::string();
    
    buf[len] = '\0';
    std::string ans = std::string(buf);

    //get rid of executable name
    size_t pos = ans.rfind('/');
    if (pos != std::string::npos) {
        ans = ans.substr(0, pos);
    }

    return ans;
}