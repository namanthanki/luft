#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "position.h"
#include "attacks.h"

typedef struct {
    Bitboard checkers;
    Bitboard pinned;
    Bitboard check_mask;
    Bitboard danger;
    uint8_t king_sq;
} MoveGenMasks;

static inline bool is_square_attacked(const Position *pos, uint8_t sq, Color by_color) {
    int c = (int)by_color;
    Bitboard occ = pos->occupancy[2];
    if (pawn_attacks[c ^ 1][sq] & pos->pieces[c][PAWN]) return true;
    if (knight_attacks[sq] & pos->pieces[c][KNIGHT]) return true;
    if (king_attacks[sq] & pos->pieces[c][KING]) return true;
    if (bishop_attacks(sq, occ) & (pos->pieces[c][BISHOP] | pos->pieces[c][QUEEN])) return true;
    if (rook_attacks(sq, occ) & (pos->pieces[c][ROOK] | pos->pieces[c][QUEEN])) return true;
    return false;
}

bool is_in_check(const Position *pos, Color side);
MoveGenMasks compute_masks(const Position *pos, Color side);
MoveGenMasks generate_moves(const Position *pos, MoveList *list);
void generate_captures(const Position *pos, MoveList *list);

#endif // MOVEGEN_H
