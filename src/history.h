#pragma once

#include "types.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>

enum {
    MAX_HISTORY = 16384,
    MAX_BONUS   = 2000
};

typedef struct {
    int table[2][64][64];
} HistoryTable;

static inline void history_clear(HistoryTable *ht) {
    memset(ht->table, 0, sizeof(ht->table));
}

static inline int history_get(const HistoryTable *ht, Color side, Square from, Square to) {
    return ht->table[side][from][to];
}

static inline void history_update(HistoryTable *ht, Color side, Square from, Square to, int bonus) {
    int clamped = clamp_int(bonus, -MAX_BONUS, MAX_BONUS);
    int current = ht->table[side][from][to];
    int abs_bonus = abs(clamped);
    ht->table[side][from][to] = current + clamped - (current * abs_bonus) / MAX_HISTORY;
}

static inline int history_bonus(int depth) {
    return min_int(depth * depth, MAX_BONUS);
}
