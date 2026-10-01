#include "zobrist.h"

uint64_t zobrist_piece_keys[2][6][64];
uint64_t zobrist_side_key;
uint64_t zobrist_castling_keys[16];
uint64_t zobrist_ep_keys[8];

static inline uint64_t xorshift64(uint64_t s) {
    uint64_t x = s;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return x;
}

static const uint64_t ZOBRIST_SEED = 0xAC7EE7ACCAFEBABEULL;

void zobrist_init(void) {
    uint64_t s = ZOBRIST_SEED;
    for (int color = 0; color < 2; color++) {
        for (int piece = 0; piece < 6; piece++) {
            for (int sq = 0; sq < 64; sq++) {
                s = xorshift64(s);
                zobrist_piece_keys[color][piece][sq] = s;
            }
        }
    }
    s = xorshift64(s);
    zobrist_side_key = s;
    for (int i = 0; i < 16; i++) {
        s = xorshift64(s);
        zobrist_castling_keys[i] = s;
    }
    for (int i = 0; i < 8; i++) {
        s = xorshift64(s);
        zobrist_ep_keys[i] = s;
    }
}
