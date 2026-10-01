#pragma once

#include "types.h"
#include <stdint.h>
#include <stdbool.h>

static inline void swap_moves(Move *a, Move *b) {
    Move tmp = *a;
    *a = *b;
    *b = tmp;
}

static inline void swap_ints(int *a, int *b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

static inline int min_int(int a, int b) {
    return a < b ? a : b;
}

static inline int max_int(int a, int b) {
    return a > b ? a : b;
}

static inline int clamp_int(int val, int min_val, int max_val) {
    if (val < min_val) return min_val;
    if (val > max_val) return max_val;
    return val;
}

static inline int64_t max_i64(int64_t a, int64_t b) {
    return a > b ? a : b;
}
