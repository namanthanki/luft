#ifndef POSITION_H
#define POSITION_H

#include "types.h"
#include "bitboard.h"
#include "zobrist.h"

#define MAX_GAME_PLY 2048
#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

typedef struct {
    Piece captured;
    Square ep;
    uint8_t castling;
    uint16_t halfmove;
    uint64_t hash;
} State;

typedef struct Position {
    Bitboard pieces[2][6];
    Bitboard occupancy[3];
    Color side;
    Square ep;
    uint8_t castling;
    uint16_t halfmove;
    uint16_t fullmove;
    uint64_t hash;
    Piece mailbox[64];
    State history[MAX_GAME_PLY];
    uint16_t game_ply;
} Position;

void position_init(Position *pos);
bool position_set_fen(Position *pos, const char *fen);
void position_display(const Position *pos);

static inline Piece position_piece_on(const Position *pos, Square sq) {
    return pos->mailbox[(uint8_t)sq];
}

static inline Color position_color_on(const Position *pos, Square sq) {
    Bitboard b = bb_bit((uint8_t)sq);
    return (pos->occupancy[WHITE] & b) ? WHITE : BLACK;
}

static inline void position_place_piece(Position *pos, Color color, Piece piece, Square sq) {
    int c = (int)color;
    int p = (int)piece;
    uint8_t s = (uint8_t)sq;
    bb_set_bit(&pos->pieces[c][p], s);
    bb_set_bit(&pos->occupancy[c], s);
    bb_set_bit(&pos->occupancy[2], s);
    pos->hash ^= zobrist_piece_keys[c][p][s];
    pos->mailbox[s] = piece;
}

static inline void position_remove_piece(Position *pos, Color color, Piece piece, Square sq) {
    int c = (int)color;
    int p = (int)piece;
    uint8_t s = (uint8_t)sq;
    bb_clear_bit(&pos->pieces[c][p], s);
    bb_clear_bit(&pos->occupancy[c], s);
    bb_clear_bit(&pos->occupancy[2], s);
    pos->hash ^= zobrist_piece_keys[c][p][s];
    pos->mailbox[s] = PIECE_NONE;
}

#endif // POSITION_H
