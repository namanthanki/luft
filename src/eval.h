#pragma once

#include "types.h"
#include "search_constants.h"

enum {
    TOTAL_PHASE = 24
};

static const int PIECE_VALUES_MG[6] = {
    100,  // PAWN
    396,  // KNIGHT
    439,  // BISHOP
    620,  // ROOK
    1253, // QUEEN
    0     // KING
};

static const int PIECE_VALUES_EG[6] = {
    235,  // PAWN
    206,  // KNIGHT
    255,  // BISHOP
    460,  // ROOK
    700,  // QUEEN
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
