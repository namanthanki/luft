#pragma once

#include <stdint.h>
#include <stdbool.h>

enum {
    // Score bounds
    MATE_SCORE           = 32000,
    MATE_BOUND           = MATE_SCORE - 512,
    INF                  = 32001,
    DRAW_SCORE           = 0,

    // Search limits & intervals
    MAX_SEARCH_PLY       = 128,
    FIFTY_MOVE_LIMIT     = 100,
    TIME_CHECK_MASK      = 2047,
    DEFAULT_MAX_DEPTH    = 64,
    DEFAULT_FIXED_DEPTH  = 5,

    // Move ordering score tiers
    SCORE_ROOT_PV        = 300000,
    SCORE_TT_MOVE        = 200000,
    SCORE_PROMOTION_BASE = 60000,
    SCORE_CAPTURE_BASE   = 40000,
    SCORE_KILLER_1       = 25000,
    SCORE_KILLER_2       = 20000,

    // UCI & Time allocation
    TIME_DIVISOR_BASE    = 20,
    TIME_DIVISOR_INC     = 2,
    DEFAULT_HASH_MB      = 16,
    MIN_HASH_MB          = 1,
    MAX_HASH_MB          = 2048
};

static inline bool is_mate_score(int score) {
    int s = score < 0 ? -score : score;
    return s >= MATE_BOUND;
}
