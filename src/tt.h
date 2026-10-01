#pragma once

#include "types.h"
#include "search_constants.h"
#include "util.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    TT_NONE       = 0,
    TT_EXACT      = 1,
    TT_LOWERBOUND = 2,
    TT_UPPERBOUND = 3
} TTFlag;

typedef struct {
    uint64_t key;
    Move best_move;
    int16_t score;
    uint8_t depth;
    uint8_t flag_age; // bits 0..1: flag, bits 2..7: age
} TTEntry;

typedef struct {
    TTEntry *entries;
    size_t capacity;
    uint8_t age;
} TranspositionTable;

extern TranspositionTable g_tt;

void tt_init(TranspositionTable *tt, size_t mb);
void tt_resize(TranspositionTable *tt, size_t mb);
void tt_clear(TranspositionTable *tt);
void tt_free(TranspositionTable *tt);

static inline uint8_t tt_entry_flag(const TTEntry *e) {
    return e->flag_age & 0x03;
}

static inline uint8_t tt_entry_age(const TTEntry *e) {
    return e->flag_age >> 2;
}

static inline uint8_t tt_make_flag_age(uint8_t flag, uint8_t age) {
    return (uint8_t)((flag & 0x03) | ((age & 0x3F) << 2));
}

static inline int score_to_tt(int score, int ply) {
    if (score > MATE_BOUND) {
        return score + ply;
    } else if (score < -MATE_BOUND) {
        return score - ply;
    }
    return score;
}

static inline int score_from_tt(int16_t score, int ply) {
    int s = (int)score;
    if (s > MATE_BOUND) {
        return s - ply;
    } else if (s < -MATE_BOUND) {
        return s + ply;
    }
    return s;
}

static inline TTEntry *tt_probe(const TranspositionTable *tt, uint64_t key) {
    if (!tt->entries || tt->capacity == 0) return NULL;
    size_t idx = (size_t)(key % tt->capacity);
    TTEntry *entry = &tt->entries[idx];
    if (entry->key == key && tt_entry_flag(entry) != TT_NONE) {
        return entry;
    }
    return NULL;
}

static inline void tt_store(
    TranspositionTable *tt,
    uint64_t key,
    int depth,
    int ply,
    int score,
    TTFlag flag,
    Move best_move
) {
    if (!tt->entries || tt->capacity == 0) return;
    size_t idx = (size_t)(key % tt->capacity);
    TTEntry *entry = &tt->entries[idx];

    bool is_same_pos = (entry->key == key);
    bool is_older_age = (tt_entry_age(entry) != (tt->age & 0x3F));
    bool is_deeper = (depth >= (int)entry->depth);

    if (tt_entry_flag(entry) == TT_NONE || is_same_pos || is_older_age || is_deeper) {
        entry->key = key;
        if (best_move != MOVE_NULL || !is_same_pos) {
            entry->best_move = best_move;
        }
        entry->score = (int16_t)score_to_tt(score, ply);
        entry->depth = (uint8_t)clamp_int(depth, 0, 255);
        entry->flag_age = tt_make_flag_age((uint8_t)flag, tt->age);
    }
}
