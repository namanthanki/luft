#ifndef SEARCH_H
#define SEARCH_H

#include "position.h"
#include "eval.h"
#include <stdio.h>

#define MATE_BOUND (MATE_SCORE - 512)
#define MAX_SEARCH_PLY 64

static inline bool is_mate_score(int score) {
    int s = score < 0 ? -score : score;
    return s >= MATE_BOUND;
}

typedef struct {
    Move best_move;
    int score;
    int depth;
    uint64_t nodes;
    uint64_t time_ms;
    uint64_t nps;
} SearchResult;

typedef struct {
    int max_depth;
    int64_t time_limit_ms;
    uint64_t soft_nodes;
    uint64_t hard_nodes;
} SearchLimits;

typedef struct {
    int64_t start_time;
    int64_t time_limit_ms;
    uint64_t soft_nodes;
    uint64_t hard_nodes;
    uint64_t nodes;
    bool stopped;
    Move killers[2][MAX_SEARCH_PLY];
} SearchInfo;

SearchResult search_position(Position *pos, const SearchLimits *limits, FILE *out);
SearchResult iterative_deepening(Position *pos, int max_depth, int64_t time_limit_ms, FILE *out);

#endif // SEARCH_H
