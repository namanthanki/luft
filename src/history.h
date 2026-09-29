#ifndef HISTORY_H
#define HISTORY_H

#include "types.h"
#include <string.h>
#include <stdlib.h>

#define MAX_HISTORY 16384
#define MAX_BONUS   2000

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
    if (bonus > MAX_BONUS) bonus = MAX_BONUS;
    if (bonus < -MAX_BONUS) bonus = -MAX_BONUS;
    int current = ht->table[side][from][to];
    int abs_bonus = bonus < 0 ? -bonus : bonus;
    ht->table[side][from][to] = current + bonus - (current * abs_bonus) / MAX_HISTORY;
}

static inline int history_bonus(int depth) {
    int b = depth * depth;
    return b > MAX_BONUS ? MAX_BONUS : b;
}

#endif // HISTORY_H
