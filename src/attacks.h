#pragma once

#include "types.h"
#include "bitboard.h"

#if defined(__BMI2__)
#include <immintrin.h>
#endif

extern Bitboard pawn_attacks[2][64];
extern Bitboard knight_attacks[64];
extern Bitboard king_attacks[64];

typedef struct {
    Bitboard mask;
    Bitboard magic;
    uint8_t shift;
} Magic;

extern Magic bishop_magics[64];
extern Magic rook_magics[64];

extern Bitboard bishop_table[64][512];
extern Bitboard rook_table[64][4096];

void attacks_init(void);

#if defined(__BMI2__)

static inline Bitboard bishop_attacks(uint8_t sq, Bitboard occ) {
    return bishop_table[sq][_pext_u64(occ, bishop_magics[sq].mask)];
}

static inline Bitboard rook_attacks(uint8_t sq, Bitboard occ) {
    return rook_table[sq][_pext_u64(occ, rook_magics[sq].mask)];
}

#else

static inline Bitboard bishop_attacks(uint8_t sq, Bitboard occ) {
    const Magic *m = &bishop_magics[sq];
    return bishop_table[sq][((occ & m->mask) * m->magic) >> m->shift];
}

static inline Bitboard rook_attacks(uint8_t sq, Bitboard occ) {
    const Magic *m = &rook_magics[sq];
    return rook_table[sq][((occ & m->mask) * m->magic) >> m->shift];
}

#endif

static inline Bitboard queen_attacks(uint8_t sq, Bitboard occ) {
    return bishop_attacks(sq, occ) | rook_attacks(sq, occ);
}
