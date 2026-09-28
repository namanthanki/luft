#include "eval.h"
#include "position.h"

int evaluate(const Position *pos) {
    int phase = pos->phase;
    if (phase > TOTAL_PHASE) phase = TOTAL_PHASE;

    int mg = (int)pos->mg_score;
    int eg = (int)pos->eg_score;

    int eval = (mg * phase + eg * (TOTAL_PHASE - phase)) / TOTAL_PHASE;

    return (pos->side == WHITE) ? eval : -eval;
}
