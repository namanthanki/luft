#pragma once

#include "types.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

static const Bitboard FILE_A = 0x0101010101010101ULL;
static const Bitboard FILE_B = 0x0202020202020202ULL;
static const Bitboard FILE_G = 0x4040404040404040ULL;
static const Bitboard FILE_H = 0x8080808080808080ULL;

static const Bitboard RANK_1 = 0x00000000000000FFULL;
static const Bitboard RANK_2 = 0x000000000000FF00ULL;
static const Bitboard RANK_7 = 0x00FF000000000000ULL;
static const Bitboard RANK_8 = 0xFF00000000000000ULL;

static inline uint8_t bb_lsb(Bitboard bb) {
    assert(bb != 0);
#if defined(_MSC_VER)
    unsigned long idx;
    _BitScanForward64(&idx, bb);
    return (uint8_t)idx;
#else
    return (uint8_t)__builtin_ctzll(bb);
#endif
}

static inline uint8_t bb_msb(Bitboard bb) {
    assert(bb != 0);
#if defined(_MSC_VER)
    unsigned long idx;
    _BitScanReverse64(&idx, bb);
    return (uint8_t)idx;
#else
    return (uint8_t)(63 - __builtin_clzll(bb));
#endif
}

static inline uint8_t bb_pop_lsb(Bitboard *bb) {
    assert(*bb != 0);
    uint8_t sq = bb_lsb(*bb);
    *bb &= *bb - 1;
    return sq;
}

static inline uint8_t bb_popcount(Bitboard bb) {
#if defined(_MSC_VER)
    return (uint8_t)__popcnt64(bb);
#else
    return (uint8_t)__builtin_popcountll(bb);
#endif
}

static inline Bitboard bb_bit(uint8_t sq) {
    return 1ULL << sq;
}

static inline void bb_set_bit(Bitboard *bb, uint8_t sq) {
    *bb |= (1ULL << sq);
}

static inline void bb_clear_bit(Bitboard *bb, uint8_t sq) {
    *bb &= ~(1ULL << sq);
}

static inline bool bb_is_bit_set(Bitboard bb, uint8_t sq) {
    return ((bb >> sq) & 1ULL) != 0;
}

static inline Bitboard bb_north_of(Bitboard b) {
    return b << 8;
}

static inline Bitboard bb_south_of(Bitboard b) {
    return b >> 8;
}

static inline Bitboard bb_east_of(Bitboard b) {
    return (b << 1) & ~FILE_A;
}

static inline Bitboard bb_west_of(Bitboard b) {
    return (b >> 1) & ~FILE_H;
}

static inline Bitboard bb_north_east_of(Bitboard b) {
    return (b << 9) & ~FILE_A;
}

static inline Bitboard bb_north_west_of(Bitboard b) {
    return (b << 7) & ~FILE_H;
}

static inline Bitboard bb_south_east_of(Bitboard b) {
    return (b >> 7) & ~FILE_A;
}

static inline Bitboard bb_south_west_of(Bitboard b) {
    return (b >> 9) & ~FILE_H;
}

void bb_print(Bitboard bb);
