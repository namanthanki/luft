#pragma once

#include "types.h"
#include "search_constants.h"

enum {
    TOTAL_PHASE = 24
};

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
