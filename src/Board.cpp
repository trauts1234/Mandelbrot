#include "Board.h"

#include "Zobrist.h"
#include "Castling.h"
#include "threadsafe_io.h"
#include "StringTools.h"
#include <algorithm> // For std::equal
#include <cassert>

/**
 * @brief edits bitboards, mailbox, and zobrist hash to remove the specified piece
 * @warning piece must not be NULL
 */
inline void RemovePiece(Board &board, SearchUtils::PlyData* ply_data, Square sq, Piece p)
{
    assert(!PieceUtils::IsEmpty(p));
    ply_data->zobrist ^= Zobrist::GetPieceHash(sq, p);//remove piece from hash
    board.squares[sq] = PieceUtils::EMPTY;
    BitboardUtils::RemoveSquare(board.piece_bitboard[PieceUtils::BasePiece(p)], sq);
    BitboardUtils::RemoveSquare(board.colour_bitboard[PieceUtils::IsWhite(p)], sq);
    board.neural_net.RemovePiece(sq,p);
}
/**
 * @brief edits bitboards, mailbox, and zobrist hash to add the specified piece
 * @warning piece must not be NULL
 */
inline void AddPiece(Board &board, SearchUtils::PlyData* ply_data, Square sq, Piece p)
{
    assert(!PieceUtils::IsEmpty(p));
    ply_data->zobrist ^= Zobrist::GetPieceHash(sq, p);//add piece to hash
    board.squares[sq] = p;
    BitboardUtils::AddSquare(board.piece_bitboard[PieceUtils::BasePiece(p)], sq);
    BitboardUtils::AddSquare(board.colour_bitboard[PieceUtils::IsWhite(p)], sq);
    board.neural_net.AddPiece(sq, p);
}

inline void RemovePieceNoZobrist(Board &board, Square sq, Piece p){
    assert(!PieceUtils::IsEmpty(p));
    board.squares[sq] = PieceUtils::EMPTY;
    BitboardUtils::RemoveSquare(board.piece_bitboard[PieceUtils::BasePiece(p)], sq);
    BitboardUtils::RemoveSquare(board.colour_bitboard[PieceUtils::IsWhite(p)], sq);
    board.neural_net.RemovePiece(sq, p);
}
inline void AddPieceNoZobrist(Board &board, Square sq, Piece p){
    assert(!PieceUtils::IsEmpty(p));
    board.squares[sq] = p;
    BitboardUtils::AddSquare(board.piece_bitboard[PieceUtils::BasePiece(p)], sq);
    BitboardUtils::AddSquare(board.colour_bitboard[PieceUtils::IsWhite(p)], sq);
    board.neural_net.AddPiece(sq, p);
}


bool BoardUtils::CompareBoards(const Board &board1, const Board &board2)
{
    for (Square i = 0; i < 64; ++i) {
        if (board1.squares[i] != board2.squares[i]) {
            sync_dbg << "problem in squares comparison" << std::endl;
            return false;
        }
    }
    for (int i = 0; i < 2; ++i) {
        if (board1.colour_bitboard[i] != board2.colour_bitboard[i]) {
            sync_dbg << "problem in colour bitboard" << std::endl;
            return false;
        }
    }
    for (int i = 1; i < 6; ++i) {
        if (board1.piece_bitboard[i] != board2.piece_bitboard[i]) {
            sync_dbg << "problem in piece bitboard" << std::endl;
            return false;
        }
    }

    if (board1.turn != board2.turn) {
        sync_dbg << "problem with turn" << std::endl;
        return false;
    }

    if(!board1.neural_net.AmEqualToOtherNet(board2.neural_net)){
        sync_dbg << "problem with neural nets" << std::endl;
        return false;
    }

    return true;
}

Board BoardUtils::CloneBoard(const Board &original)
{
    auto result = Board();
    for(Square s=0;s<64;s++){
        result.squares[s] = original.squares[s];
    }
    for(int i=0;i < 6;i++){
        result.piece_bitboard[i] = original.piece_bitboard[i];
    }
    result.colour_bitboard[0] = original.colour_bitboard[0];
    result.colour_bitboard[1] = original.colour_bitboard[1];
    result.turn = original.turn;

    return result;
}

uint64_t BoardUtils::CalculateZobristHash(const Board &board, const SearchUtils::PlyData* ply_data)
{
    uint64_t hash = 0;

    for(Square s=0;s<64;s++){
        if(PieceUtils::IsEmpty(board.squares[s])) {continue;}//skip blank squares
        hash ^= Zobrist::GetPieceHash(s, board.squares[s]);
    }

   hash ^= Zobrist::CASTLING[ply_data->castling_rights];

    if(SquareUtils::IsValid(ply_data->enpessant)){
        hash ^= Zobrist::ENPESSANT[SquareUtils::ToFile(ply_data->enpessant)];
    }

    if(board.turn){hash ^= Zobrist::WHITE_TURN;}

    return hash;
}

void BoardUtils::MakeMove(Board &board, Move m, SearchUtils::PlyData* ply_data)
{
    SearchUtils::PlyData* next_ply = ply_data+1;

    Square from = MoveUtils::FromSquare(m);
    Bitboard from_bb = BitboardUtils::MakeBitBoard(from);
    Square to = MoveUtils::ToSquare(m);
    Bitboard to_bb = BitboardUtils::MakeBitBoard(to);
    Piece moved_piece = board.squares[from];
    bool move_creates_enpessant_location = 
        PieceUtils::BasePiece(moved_piece) == PieceUtils::PAWN && 
        from_bb&BitboardUtils::DOUBLE_PUSH &&
        to_bb&BitboardUtils::AFTER_DOUBLE_PUSH;
    
    assert(PieceUtils::IsWhite(moved_piece) == board.turn);
    assert(SquareUtils::IsValid(from));
    assert(SquareUtils::IsValid(to));
    assert(!PieceUtils::IsEmpty(moved_piece));

    //set next ply data
    next_ply->fifty_move_rule = ply_data->fifty_move_rule + 1;
    next_ply->zobrist = ply_data->zobrist;//this gets edited slowly
    next_ply->castling_rights = ply_data->castling_rights;
    next_ply->ply_from_root = ply_data->ply_from_root+1;//TODO pass ply from root as a negamax param, and use it to index ply data?

    next_ply->zobrist ^= Zobrist::CASTLING[next_ply->castling_rights];//remove this, and add it after the castling has happened or not

    Piece us_king = PieceUtils::KING + (board.turn ? PieceUtils::WHITE_FLAG : 0);

    int my_kingside_castling = board.turn ? CastlingUtils::WK_CASTLE : CastlingUtils::BK_CASTLE;
    int my_queenside_castling = board.turn ? CastlingUtils::WQ_CASTLE : CastlingUtils::BQ_CASTLE;
    unsigned int my_castling_mask = CastlingUtils::GenerateCastleMask(my_kingside_castling) | CastlingUtils::GenerateCastleMask(my_queenside_castling);

    if(moved_piece == us_king){
        next_ply->castling_rights &= ~my_castling_mask;//if the king moves, then I can't castle at all this game
    }

    if(from == CastlingUtils::WK_ROOK_SQ || to == CastlingUtils::WK_ROOK_SQ){// bitwise or here generates one branch, which is better predicted?
        next_ply->castling_rights &= ~CastlingUtils::GenerateCastleMask(CastlingUtils::WK_CASTLE);
    }
    if(from == CastlingUtils::WQ_ROOK_SQ || to == CastlingUtils::WQ_ROOK_SQ){
        next_ply->castling_rights &= ~CastlingUtils::GenerateCastleMask(CastlingUtils::WQ_CASTLE);
    }

    if(from == CastlingUtils::BK_ROOK_SQ || to == CastlingUtils::BK_ROOK_SQ){
        next_ply->castling_rights &= ~CastlingUtils::GenerateCastleMask(CastlingUtils::BK_CASTLE);
    }
    if(from == CastlingUtils::BQ_ROOK_SQ || to == CastlingUtils::BQ_ROOK_SQ){
        next_ply->castling_rights &= ~CastlingUtils::GenerateCastleMask(CastlingUtils::BQ_CASTLE);
    }

    assert(next_ply->castling_rights <= ply_data->castling_rights);//simple check to catch instances where extra castling was added

    next_ply->zobrist ^= Zobrist::CASTLING[next_ply->castling_rights];//re-set the castling  after

    //remove piece you picked up to move
    RemovePiece(board, next_ply, from, moved_piece);

    Piece killed = board.squares[to];
    ply_data->killed = killed;//save undo move data

    //delete killed piece
    if(!PieceUtils::IsEmpty(killed)){
        RemovePiece(board, next_ply, to, killed);
        next_ply->fifty_move_rule = 0;//piece killed, reset fifty move counter
    }

    //manage enpessant capture
    if(PieceUtils::BasePiece(moved_piece) == PieceUtils::PAWN){
        next_ply->fifty_move_rule = 0;//pawn move, reset fifty move counter
        if(to == ply_data->enpessant){
            Piece killed_piece = PieceUtils::PAWN + (board.turn ? 0 : PieceUtils::WHITE_FLAG);
            Piece killed_ep = ply_data->enpessant + (board.turn ? -8 : 8);
            RemovePiece(board, next_ply, killed_ep, killed_piece);
        }
    }

    //manage enpessant square
    if(SquareUtils::IsValid(ply_data->enpessant)){//if there is a current enpessant move
        next_ply->zobrist ^= Zobrist::ENPESSANT[SquareUtils::ToFile(ply_data->enpessant)];//remove current enpessant
    }

    if(move_creates_enpessant_location){
        next_ply->enpessant = to + (board.turn ? -8 : 8);
    } else{
        next_ply->enpessant = SquareUtils::NULL_SQUARE;
    }

    if(SquareUtils::IsValid(next_ply->enpessant)){//if there is a new enpessant square
        next_ply->zobrist ^= Zobrist::ENPESSANT[SquareUtils::ToFile(next_ply->enpessant)];//add new enpessant square
    }

    //manage castling
    switch (MoveUtils::CastleType(m)){
        case CastlingUtils::NO_CASTLE:
            break;
        case CastlingUtils::WK_CASTLE:
            RemovePiece(board, next_ply, CastlingUtils::WK_ROOK_SQ, PieceUtils::ROOK+PieceUtils::WHITE_FLAG);
            AddPiece(board, next_ply, 5, PieceUtils::ROOK+PieceUtils::WHITE_FLAG);
            break;
        case CastlingUtils::WQ_CASTLE:
            RemovePiece(board, next_ply, CastlingUtils::WQ_ROOK_SQ, PieceUtils::ROOK+PieceUtils::WHITE_FLAG);
            AddPiece(board, next_ply, 3, PieceUtils::ROOK+PieceUtils::WHITE_FLAG);
            break;
        
        case CastlingUtils::BK_CASTLE:
            RemovePiece(board, next_ply, CastlingUtils::BK_ROOK_SQ, PieceUtils::ROOK);
            AddPiece(board, next_ply, 61, PieceUtils::ROOK);
            break;
        case CastlingUtils::BQ_CASTLE:
            RemovePiece(board, next_ply, CastlingUtils::BQ_ROOK_SQ, PieceUtils::ROOK);
            AddPiece(board, next_ply, 59, PieceUtils::ROOK);
            break;
    }

    //place my piece back down
    if(PieceUtils::IsEmpty(MoveUtils::PromotionBase(m))){
        AddPiece(board, next_ply, to, moved_piece);
    } else{
        Piece promotion = MoveUtils::PromotionBase(m) + (board.turn ? PieceUtils::WHITE_FLAG : 0);
        assert(PieceUtils::IsWhite(promotion) == board.turn);
        AddPiece(board, next_ply, to, promotion);
    }

    next_ply->zobrist ^= Zobrist::WHITE_TURN;//switch turn
    board.turn ^= true;
}

void BoardUtils::UnMakeMove(Board &board, Move m, const SearchUtils::PlyData* ply_data)
{
    board.turn ^= true;//this must be first
    Square from = MoveUtils::FromSquare(m);
    Square to = MoveUtils::ToSquare(m);
    Piece moved_piece;//based on whether this is a promoted piece or not, this is calculated

    assert(SquareUtils::IsValid(from));
    assert(SquareUtils::IsValid(to));

    // pick my piece back up
    if(PieceUtils::IsEmpty(MoveUtils::PromotionBase(m))){
        moved_piece = board.squares[to];
        RemovePieceNoZobrist(board, to, moved_piece);
    } else{
        Piece promotion = MoveUtils::PromotionBase(m) + (board.turn ? PieceUtils::WHITE_FLAG : 0);
        moved_piece = PieceUtils::PAWN + (board.turn ? PieceUtils::WHITE_FLAG : 0);
        RemovePieceNoZobrist(board, to, promotion);
    }

    // restore the killed piece
    if(!PieceUtils::IsEmpty(ply_data->killed)){//perhaps I can write this to be branchless, the compiler can't!
        AddPieceNoZobrist(board, to, ply_data->killed);
    }

    //manage enpessant capture
    if(to == ply_data->enpessant && PieceUtils::BasePiece(moved_piece) == PieceUtils::PAWN){
        Piece killed_piece = PieceUtils::PAWN + (board.turn ? 0 : PieceUtils::WHITE_FLAG);
        Piece killed_ep = ply_data->enpessant + (board.turn ? -8 : 8);
        AddPieceNoZobrist(board, killed_ep, killed_piece);
    }

    //mamage castling
    switch (MoveUtils::CastleType(m)){
        case CastlingUtils::NO_CASTLE:
            break;
        case CastlingUtils::WK_CASTLE:
            AddPieceNoZobrist(board, CastlingUtils::WK_ROOK_SQ, PieceUtils::ROOK+PieceUtils::WHITE_FLAG);
            RemovePieceNoZobrist(board, 5, PieceUtils::ROOK+PieceUtils::WHITE_FLAG);
            break;
        case CastlingUtils::WQ_CASTLE:
            AddPieceNoZobrist(board, CastlingUtils::WQ_ROOK_SQ, PieceUtils::ROOK+PieceUtils::WHITE_FLAG);
            RemovePieceNoZobrist(board, 3, PieceUtils::ROOK+PieceUtils::WHITE_FLAG);
            break;
        
        case CastlingUtils::BK_CASTLE:
            AddPieceNoZobrist(board, CastlingUtils::BK_ROOK_SQ, PieceUtils::ROOK);
            RemovePieceNoZobrist(board, 61, PieceUtils::ROOK);
            break;
        case CastlingUtils::BQ_CASTLE:
            AddPieceNoZobrist(board, CastlingUtils::BQ_ROOK_SQ, PieceUtils::ROOK);
            RemovePieceNoZobrist(board, 59, PieceUtils::ROOK);
            break;
    }

    // Move the piece back to the 'from' square
    AddPieceNoZobrist(board, from, moved_piece);
}

void BoardUtils::MakeNullMove(Board &board, SearchUtils::PlyData *ply_data)
{
    assert(!ply_data->in_check);

    SearchUtils::PlyData* next_ply = ply_data+1;

    //set next ply data
    next_ply->fifty_move_rule = 0;
    next_ply->castling_rights = ply_data->castling_rights;
    next_ply->ply_from_root = ply_data->ply_from_root+1;
    next_ply->zobrist = ply_data->zobrist;//updated later
    next_ply->enpessant = SquareUtils::NULL_SQUARE;//no enpessant after a null move


    //manage enpessant square
    if(SquareUtils::IsValid(ply_data->enpessant)){//if there is a current enpessant move
        next_ply->zobrist ^= Zobrist::ENPESSANT[SquareUtils::ToFile(ply_data->enpessant)];//update hash
    }

    next_ply->zobrist ^= Zobrist::WHITE_TURN;//switch turn
    board.turn ^= true;
}

void BoardUtils::UnMakeNullMove(Board &board)
{
    board.turn ^= true;
}

/**
 * @brief weird but cool function to read an embedded header file generated by xxd -i
 * @note xxd does not null terminate the char array, so be careful as you can't cast the array to char[]
 * @returns the text encoded in the network data file
 */
static std::string ReadNN(){
    #include "network_data.txt"

    std::string result = std::string();
    for(unsigned int i=0; i < weights_biases_txt_len; i++){
        result += (char)weights_biases_txt[i];
    }
    return result;
}

Board::Board():
neural_net(ReadNN())
{
}
