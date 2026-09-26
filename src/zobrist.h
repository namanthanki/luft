#ifndef ZOBRIST_H
#define ZOBRIST_H

#include <stdint.h>

extern uint64_t zobrist_piece_keys[2][6][64];
extern uint64_t zobrist_side_key;
extern uint64_t zobrist_castling_keys[16];
extern uint64_t zobrist_ep_keys[8];

void zobrist_init(void);

#endif // ZOBRIST_H
