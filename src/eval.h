#ifndef EVAL_H
#define EVAL_H

#include "types.h"

#define MATE_SCORE 32000
#define INF        32001
#define DRAW_SCORE 0

#define TOTAL_PHASE 24

static const int PIECE_VALUES_MG[6] = {
    100,  // PAWN
    222,  // KNIGHT
    200,  // BISHOP
    267,  // ROOK
    700,  // QUEEN
    0     // KING
};

static const int PIECE_VALUES_EG[6] = {
    141,  // PAWN
    188,  // KNIGHT
    296,  // BISHOP
    403,  // ROOK
    807,  // QUEEN
    0     // KING
};

static const int PIECE_PHASE[6] = {
    0, // PAWN
    1, // KNIGHT
    1, // BISHOP
    2, // ROOK
    4, // QUEEN
    0  // KING
};

struct Position;
int evaluate(const struct Position *pos);

#endif // EVAL_H
