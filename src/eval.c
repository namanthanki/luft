#include "eval.h"

int evaluate(const Position *pos) {
    int us   = (int)pos->side;
    int them = us ^ 1;
    return (int)bb_popcount(pos->occupancy[us])
         - (int)bb_popcount(pos->occupancy[them]);
}
