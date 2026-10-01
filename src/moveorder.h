#pragma once

#include "types.h"
#include "eval.h"
#include "data.h"
#include "search.h"
#include "search_constants.h"
#include "util.h"

static inline int score_move(Move m, Color side, int ply, const SearchInfo *info) {
    if (move_is_promo(m)) {
        Piece promo = move_promo(m);
        int val = (promo < 6) ? PIECE_VALUES_MG[promo] : 0;
        return SCORE_PROMOTION_BASE + val;
    }
    if (move_is_capture(m)) {
        int victim = (int)move_cap(m);
        int attacker = (int)move_piece(m);
        return SCORE_CAPTURE_BASE + MVV_LVA[victim][attacker];
    }
    if (info && ply < MAX_SEARCH_PLY) {
        if (m == info->killers[0][ply]) return SCORE_KILLER_1;
        if (m == info->killers[1][ply]) return SCORE_KILLER_2;
    }
    if (info && info->history) {
        return history_get(info->history, side, move_from(m), move_to(m));
    }
    return 0;
}

static inline void score_moves(const MoveList *list, int scores[MAX_MOVES], Color side, int ply, const SearchInfo *info) {
    for (int i = 0; i < list->count; i++) {
        scores[i] = score_move(list->moves[i], side, ply, info);
    }
}

static inline void pick_next_move(MoveList *list, int scores[MAX_MOVES], int current_idx) {
    int best_idx = current_idx;
    int best_val = scores[current_idx];

    for (int j = current_idx + 1; j < list->count; j++) {
        if (scores[j] > best_val) {
            best_val = scores[j];
            best_idx = j;
        }
    }

    if (best_idx != current_idx) {
        swap_moves(&list->moves[current_idx], &list->moves[best_idx]);
        swap_ints(&scores[current_idx], &scores[best_idx]);
    }
}
