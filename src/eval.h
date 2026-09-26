#ifndef EVAL_H
#define EVAL_H

#include "position.h"

#define MATE_SCORE 32000
#define INF        32001
#define DRAW_SCORE 0

int evaluate(const Position *pos);

#endif // EVAL_H
