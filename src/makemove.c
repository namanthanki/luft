#include "makemove.h"

static const uint8_t castling_mask[64] = {
    CASTLE_ALL & ~CASTLE_WQ, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL & ~(CASTLE_WK | CASTLE_WQ), CASTLE_ALL, CASTLE_ALL, CASTLE_ALL & ~CASTLE_WK,
    CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL,
    CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL,
    CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL,
    CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL,
    CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL,
    CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL,
    CASTLE_ALL & ~CASTLE_BQ, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL, CASTLE_ALL & ~(CASTLE_BK | CASTLE_BQ), CASTLE_ALL, CASTLE_ALL, CASTLE_ALL & ~CASTLE_BK
};

static inline void move_piece_on_board(Position *pos, Color color, Piece pc, Square from_sq, Square to_sq) {
    int c = (int)color;
    int p = (int)pc;
    uint8_t sf = (uint8_t)from_sq;
    uint8_t st = (uint8_t)to_sq;
    Bitboard both = bb_bit(sf) | bb_bit(st);
    pos->pieces[c][p] ^= both;
    pos->occupancy[c] ^= both;
    pos->occupancy[2] ^= both;
    pos->hash ^= zobrist_piece_keys[c][p][sf] ^ zobrist_piece_keys[c][p][st];
    pos->mailbox[sf] = PIECE_NONE;
    pos->mailbox[st] = pc;
}

void make_move(Position *pos, Move m) {
    Square from_sq = move_from(m);
    Square to_sq = move_to(m);
    Piece pc = move_piece(m);
    Piece captured = move_cap(m);
    Piece promotion = move_promo(m);
    Flag flag = move_flag(m);
    Color us = pos->side;
    Color them = flip_color(us);

    State *undo = &pos->history[pos->game_ply++];
    undo->captured = captured;
    undo->ep = pos->ep;
    undo->castling = pos->castling;
    undo->halfmove = pos->halfmove;
    undo->hash = pos->hash;
    undo->mg_score = pos->mg_score;
    undo->eg_score = pos->eg_score;
    undo->phase = pos->phase;

    if (pos->ep != SQ_NONE) {
        pos->hash ^= zobrist_ep_keys[sq_file(pos->ep)];
    }
    pos->hash ^= zobrist_castling_keys[pos->castling];

    pos->ep = SQ_NONE;
    pos->halfmove = (pc == PAWN || captured != PIECE_NONE) ? 0 : pos->halfmove + 1;

    if (captured != PIECE_NONE && flag != FLAG_EN_PASSANT) {
        position_remove_piece(pos, them, captured, to_sq);
    }

    switch (flag) {
        case FLAG_CASTLE_K:
            if (us == WHITE) {
                move_piece_on_board(pos, us, KING, SQ_E1, SQ_G1);
                move_piece_on_board(pos, us, ROOK, SQ_H1, SQ_F1);
            } else {
                move_piece_on_board(pos, us, KING, SQ_E8, SQ_G8);
                move_piece_on_board(pos, us, ROOK, SQ_H8, SQ_F8);
            }
            break;
        case FLAG_CASTLE_Q:
            if (us == WHITE) {
                move_piece_on_board(pos, us, KING, SQ_E1, SQ_C1);
                move_piece_on_board(pos, us, ROOK, SQ_A1, SQ_D1);
            } else {
                move_piece_on_board(pos, us, KING, SQ_E8, SQ_C8);
                move_piece_on_board(pos, us, ROOK, SQ_A8, SQ_D8);
            }
            break;
        case FLAG_EN_PASSANT: {
            move_piece_on_board(pos, us, PAWN, from_sq, to_sq);
            Square cap_sq = (Square)(us == WHITE ? (int)to_sq - 8 : (int)to_sq + 8);
            position_remove_piece(pos, them, PAWN, cap_sq);
            break;
        }
        case FLAG_DOUBLE_PUSH:
        case FLAG_QUIET:
            move_piece_on_board(pos, us, pc, from_sq, to_sq);
            break;
    }

    if (promotion != NO_PROMO) {
        position_remove_piece(pos, us, PAWN, to_sq);
        position_place_piece(pos, us, promotion, to_sq);
    }

    if (flag == FLAG_DOUBLE_PUSH) {
        pos->ep = (Square)(us == WHITE ? (int)to_sq - 8 : (int)to_sq + 8);
        pos->hash ^= zobrist_ep_keys[sq_file(pos->ep)];
    }

    pos->castling &= castling_mask[from_sq] & castling_mask[to_sq];
    pos->hash ^= zobrist_castling_keys[pos->castling];

    pos->side = them;
    pos->hash ^= zobrist_side_key;

    if (pos->side == WHITE) pos->fullmove++;
}

void unmake_move(Position *pos, Move m) {
    pos->side = flip_color(pos->side);
    if (pos->side == BLACK) pos->fullmove--;

    Square from_sq = move_from(m);
    Square to_sq = move_to(m);
    Piece pc = move_piece(m);
    Piece promotion = move_promo(m);
    Flag flag = move_flag(m);
    Color us = pos->side;
    Color them = flip_color(us);

    pos->game_ply--;
    State state = pos->history[pos->game_ply];
    Piece captured = state.captured;

    if (promotion != NO_PROMO) {
        position_remove_piece(pos, us, promotion, to_sq);
        position_place_piece(pos, us, PAWN, from_sq);
        if (captured != PIECE_NONE) {
            position_place_piece(pos, them, captured, to_sq);
        }
    } else {
        switch (flag) {
            case FLAG_CASTLE_K:
                if (us == WHITE) {
                    move_piece_on_board(pos, us, KING, SQ_G1, SQ_E1);
                    move_piece_on_board(pos, us, ROOK, SQ_F1, SQ_H1);
                } else {
                    move_piece_on_board(pos, us, KING, SQ_G8, SQ_E8);
                    move_piece_on_board(pos, us, ROOK, SQ_F8, SQ_H8);
                }
                break;
            case FLAG_CASTLE_Q:
                if (us == WHITE) {
                    move_piece_on_board(pos, us, KING, SQ_C1, SQ_E1);
                    move_piece_on_board(pos, us, ROOK, SQ_D1, SQ_A1);
                } else {
                    move_piece_on_board(pos, us, KING, SQ_C8, SQ_E8);
                    move_piece_on_board(pos, us, ROOK, SQ_D8, SQ_A8);
                }
                break;
            case FLAG_EN_PASSANT: {
                move_piece_on_board(pos, us, PAWN, to_sq, from_sq);
                Square cap_sq = (Square)(us == WHITE ? (int)to_sq - 8 : (int)to_sq + 8);
                position_place_piece(pos, them, PAWN, cap_sq);
                break;
            }
            case FLAG_DOUBLE_PUSH:
            case FLAG_QUIET:
                move_piece_on_board(pos, us, pc, to_sq, from_sq);
                if (captured != PIECE_NONE) {
                    position_place_piece(pos, them, captured, to_sq);
                }
                break;
        }
    }

    pos->ep = state.ep;
    pos->castling = state.castling;
    pos->halfmove = state.halfmove;
    pos->hash = state.hash;
    pos->mg_score = state.mg_score;
    pos->eg_score = state.eg_score;
    pos->phase = state.phase;
}
