#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

typedef uint64_t Bitboard;
typedef uint32_t Move;

typedef enum {
    WHITE = 0,
    BLACK = 1
} Color;

static inline Color flip_color(Color c) {
    return (Color)(c ^ 1);
}

typedef enum {
    PAWN       = 0,
    KNIGHT     = 1,
    BISHOP     = 2,
    ROOK       = 3,
    QUEEN      = 4,
    KING       = 5,
    PIECE_NONE = 6,
    NO_PROMO   = 7
} Piece;

static const char PIECE_CHARS[]       = "pnbrqk";
static const char PIECE_CHARS_UPPER[] = "PNBRQK";

static inline char piece_char(Piece p, bool upper) {
    assert((int)p < 6);
    return upper ? PIECE_CHARS_UPPER[(int)p] : PIECE_CHARS[(int)p];
}

typedef enum {
    SQ_A1 = 0,  SQ_B1 = 1,  SQ_C1 = 2,  SQ_D1 = 3,  SQ_E1 = 4,  SQ_F1 = 5,  SQ_G1 = 6,  SQ_H1 = 7,
    SQ_A2 = 8,  SQ_B2 = 9,  SQ_C2 = 10, SQ_D2 = 11, SQ_E2 = 12, SQ_F2 = 13, SQ_G2 = 14, SQ_H2 = 15,
    SQ_A3 = 16, SQ_B3 = 17, SQ_C3 = 18, SQ_D3 = 19, SQ_E3 = 20, SQ_F3 = 21, SQ_G3 = 22, SQ_H3 = 23,
    SQ_A4 = 24, SQ_B4 = 25, SQ_C4 = 26, SQ_D4 = 27, SQ_E4 = 28, SQ_F4 = 29, SQ_G4 = 30, SQ_H4 = 31,
    SQ_A5 = 32, SQ_B5 = 33, SQ_C5 = 34, SQ_D5 = 35, SQ_E5 = 36, SQ_F5 = 37, SQ_G5 = 38, SQ_H5 = 39,
    SQ_A6 = 40, SQ_B6 = 41, SQ_C6 = 42, SQ_D6 = 43, SQ_E6 = 44, SQ_F6 = 45, SQ_G6 = 46, SQ_H6 = 47,
    SQ_A7 = 48, SQ_B7 = 49, SQ_C7 = 50, SQ_D7 = 51, SQ_E7 = 52, SQ_F7 = 53, SQ_G7 = 54, SQ_H7 = 55,
    SQ_A8 = 56, SQ_B8 = 57, SQ_C8 = 58, SQ_D8 = 59, SQ_E8 = 60, SQ_F8 = 61, SQ_G8 = 62, SQ_H8 = 63,
    SQ_NONE = 255
} Square;

static inline uint8_t sq_rank(Square sq) {
    return (uint8_t)(sq / 8);
}

static inline uint8_t sq_file(Square sq) {
    return (uint8_t)(sq % 8);
}

static inline Square sq_from_coords(uint8_t r, uint8_t f) {
    assert(r < 8 && f < 8);
    return (Square)(r * 8 + f);
}

typedef enum {
    CASTLE_NONE = 0,
    CASTLE_WK   = 1,
    CASTLE_WQ   = 2,
    CASTLE_BK   = 4,
    CASTLE_BQ   = 8,
    CASTLE_ALL  = 0xF
} Castling;

typedef enum {
    FLAG_QUIET       = 0,
    FLAG_DOUBLE_PUSH = 1,
    FLAG_CASTLE_K    = 2,
    FLAG_CASTLE_Q    = 3,
    FLAG_EN_PASSANT  = 4
} Flag;

enum {
    FROM_SHIFT  = 0,
    TO_SHIFT    = 6,
    PIECE_SHIFT = 12,
    CAP_SHIFT   = 16,
    PROMO_SHIFT = 20,
    FLAG_SHIFT  = 24,

    SQ_MASK     = 0x3F,
    NIBBLE_MASK = 0x0F,

    MOVE_NULL   = 0,
    MAX_MOVES   = 256
};

static inline Move move_encode(Square from, Square to, Piece piece, Piece cap, Piece promo, Flag flag) {
    return ((Move)from  << FROM_SHIFT)  |
           ((Move)to    << TO_SHIFT)    |
           ((Move)piece << PIECE_SHIFT) |
           ((Move)cap   << CAP_SHIFT)   |
           ((Move)promo << PROMO_SHIFT) |
           ((Move)flag  << FLAG_SHIFT);
}

static inline Square move_from(Move m) {
    return (Square)((m >> FROM_SHIFT) & SQ_MASK);
}

static inline Square move_to(Move m) {
    return (Square)((m >> TO_SHIFT) & SQ_MASK);
}

static inline Piece move_piece(Move m) {
    return (Piece)((m >> PIECE_SHIFT) & NIBBLE_MASK);
}

static inline Piece move_cap(Move m) {
    return (Piece)((m >> CAP_SHIFT) & NIBBLE_MASK);
}

static inline Piece move_promo(Move m) {
    return (Piece)((m >> PROMO_SHIFT) & NIBBLE_MASK);
}

static inline Flag move_flag(Move m) {
    return (Flag)((m >> FLAG_SHIFT) & NIBBLE_MASK);
}

static inline bool move_is_capture(Move m) {
    return move_cap(m) != PIECE_NONE;
}

static inline bool move_is_promo(Move m) {
    return move_promo(m) != NO_PROMO;
}

static inline bool move_is_quiet(Move m) {
    return ((m >> CAP_SHIFT) & 0xFF) == ((uint32_t)PIECE_NONE | ((uint32_t)NO_PROMO << 4));
}

static inline bool move_is_tactical(Move m) {
    return move_is_capture(m) || move_is_promo(m);
}

static inline uint16_t move_from_to(Move m) {
    return (uint16_t)(m & 0xFFF);
}

static inline void move_to_uci(Move m, char buf[6]) {
    Square f = move_from(m);
    Square t = move_to(m);
    buf[0] = (char)('a' + sq_file(f));
    buf[1] = (char)('1' + sq_rank(f));
    buf[2] = (char)('a' + sq_file(t));
    buf[3] = (char)('1' + sq_rank(t));
    if (move_is_promo(m)) {
        buf[4] = PIECE_CHARS[(int)move_promo(m)];
        buf[5] = '\0';
    } else {
        buf[4] = '\0';
    }
}

typedef struct {
    Move moves[MAX_MOVES];
    int count;
} MoveList;

static inline void move_list_push(MoveList *list, Move m) {
    assert(list->count < MAX_MOVES);
    list->moves[list->count++] = m;
}

static inline void move_list_reset(MoveList *list) {
    list->count = 0;
}
