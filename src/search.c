#include "search.h"
#include "eval.h"
#include "movegen.h"
#include "makemove.h"
#include "moveorder.h"
#include "time_utils.h"
#include "tt.h"
#include "search_constants.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static _Thread_local HistoryTable g_history;

void search_clear_history(void) {
    history_clear(&g_history);
}

static inline int64_t search_info_elapsed(const SearchInfo *info) {
    return get_time_ms_signed() - info->start_time;
}

static inline void check_time(SearchInfo *info) {
    if (info->hard_nodes > 0 && info->nodes >= info->hard_nodes) {
        info->stopped = true;
        return;
    }
    if (info->time_limit_ms <= 0) return;
    if ((info->nodes & TIME_CHECK_MASK) != 0) return;
    if (search_info_elapsed(info) >= info->time_limit_ms) {
        info->stopped = true;
    }
}

static inline void update_quiet_heuristics(SearchInfo *info, const Position *pos, Move m, int depth, int ply) {
    if (!move_is_quiet(m)) return;
    if (ply < MAX_SEARCH_PLY && m != info->killers[0][ply]) {
        info->killers[1][ply] = info->killers[0][ply];
        info->killers[0][ply] = m;
    }
    if (info->history) {
        history_update(info->history, pos->side, move_from(m), move_to(m), history_bonus(depth));
    }
}

static void print_search_info(FILE *out, int depth, int score, const SearchResult *result) {
    if (!out) return;

    char pv_str[6];
    move_to_uci(result->best_move, pv_str);

    char score_str[32];
    if (is_mate_score(score)) {
        int mate_in = (MATE_SCORE - abs(score) + 1) / 2;
        snprintf(score_str, sizeof(score_str), "mate %s%d", (score < 0) ? "-" : "", mate_in);
    } else {
        snprintf(score_str, sizeof(score_str), "cp %d", score);
    }

    fprintf(out, "info depth %d score %s nodes %llu nps %llu time %llu pv %s\n",
            depth, score_str,
            (unsigned long long)result->nodes,
            (unsigned long long)result->nps,
            (unsigned long long)result->time_ms,
            pv_str);
    fflush(out);
}

static int qsearch(Position *pos, int ply, int alpha, int beta, SearchInfo *info) {
    info->nodes++;
    check_time(info);
    if (info->stopped) return 0;

    if (pos->halfmove >= FIFTY_MOVE_LIMIT || position_is_repetition(pos)) return DRAW_SCORE;

    bool in_check = is_in_check(pos, pos->side);
    if (ply >= MAX_SEARCH_PLY - 1 || pos->game_ply >= MAX_GAME_PLY - 1) {
        return in_check ? 0 : evaluate(pos);
    }

    int best_score = -INF;

    if (!in_check) {
        int stand_pat = evaluate(pos);
        if (stand_pat >= beta) return stand_pat;
        if (stand_pat > alpha) alpha = stand_pat;
        best_score = stand_pat;
    }

    MoveList list;
    if (in_check) {
        generate_moves(pos, &list);
        if (list.count == 0) {
            return -(MATE_SCORE - ply);
        }
    } else {
        generate_captures(pos, &list);
    }
 
    if (list.count == 0) return best_score;

    int scores[MAX_MOVES];
    score_moves(&list, scores, pos->side, ply, info);

    for (int i = 0; i < list.count; i++) {
        pick_next_move(&list, scores, i);
        Move m = list.moves[i];

        make_move(pos, m);
        int score = -qsearch(pos, ply + 1, -beta, -alpha, info);
        unmake_move(pos, m);

        if (info->stopped) return 0;

        if (score > best_score) {
            best_score = score;
            if (score >= beta) return score;
            if (score > alpha) alpha = score;
        }
    }

    return best_score;
}

static int negamax(Position *pos, int depth, int ply, int alpha, int beta, bool is_pv, SearchInfo *info) {
    info->nodes++;
    check_time(info);
    if (info->stopped) return 0;

    if (pos->halfmove >= FIFTY_MOVE_LIMIT || position_is_repetition(pos)) return DRAW_SCORE;
    if (ply >= MAX_SEARCH_PLY - 1 || pos->game_ply >= MAX_GAME_PLY - 1) return evaluate(pos);

    Move tt_move = MOVE_NULL;
    TTEntry *entry = tt_probe(&g_tt, pos->hash);
    if (entry) {
        tt_move = entry->best_move;
        int tt_score = score_from_tt(entry->score, ply);
        if (!is_pv && entry->depth >= depth && ply > 0) {
            uint8_t flag = tt_entry_flag(entry);
            if (flag == TT_EXACT) return tt_score;
            if (flag == TT_LOWERBOUND && tt_score >= beta) return tt_score;
            if (flag == TT_UPPERBOUND && tt_score <= alpha) return tt_score;
        }
    }

    if (depth <= 0) return qsearch(pos, ply, alpha, beta, info);

    MoveList list;
    MoveGenMasks masks = generate_moves(pos, &list);

    if (list.count == 0) {
        if (masks.checkers != 0) {
            return -(MATE_SCORE - ply);
        }
        return DRAW_SCORE;
    }

    int scores[MAX_MOVES];
    for (int i = 0; i < list.count; i++) {
        Move m = list.moves[i];
        scores[i] = (m == tt_move) ? SCORE_TT_MOVE : score_move(m, pos->side, ply, info);
    }

    const int orig_alpha = alpha;
    int best_score = -INF;
    Move best_move = MOVE_NULL;

    for (int i = 0; i < list.count; i++) {
        pick_next_move(&list, scores, i);
        Move m = list.moves[i];

        bool child_is_pv = is_pv && (i == 0);
        make_move(pos, m);
        int score = -negamax(pos, depth - 1, ply + 1, -beta, -alpha, child_is_pv, info);
        unmake_move(pos, m);

        if (info->stopped) return 0;

        if (score > best_score) {
            best_score = score;
        }

        if (score >= beta) {
            update_quiet_heuristics(info, pos, m, depth, ply);
            tt_store(&g_tt, pos->hash, depth, ply, score, TT_LOWERBOUND, m);
            return score;
        }

        if (score > alpha) {
            alpha = score;
            best_move = m;
        }
    }

    TTFlag flag = (best_score > orig_alpha) ? TT_EXACT : TT_UPPERBOUND;
    tt_store(&g_tt, pos->hash, depth, ply, best_score, flag, best_move);

    return best_score;
}

SearchResult search_position(Position *pos, const SearchLimits *limits, FILE *out) {
    g_tt.age++;
    SearchInfo info;
    info.start_time = get_time_ms_signed();
    info.time_limit_ms = limits->time_limit_ms;
    info.soft_nodes = limits->soft_nodes;
    info.hard_nodes = limits->hard_nodes;
    info.nodes = 0;
    info.stopped = false;
    memset(info.killers, 0, sizeof(info.killers));
    info.history = &g_history;

    SearchResult result;
    memset(&result, 0, sizeof(result));

    MoveList root_list;
    generate_moves(pos, &root_list);
    if (root_list.count == 0) return result;
    result.best_move = root_list.moves[0];
    int max_d = limits->max_depth > 0 ? limits->max_depth : DEFAULT_MAX_DEPTH;

    for (int depth = 1; depth <= max_d; depth++) {
        TTEntry *entry = tt_probe(&g_tt, pos->hash);
        Move root_tt_move = entry ? entry->best_move : MOVE_NULL;

        int root_scores[MAX_MOVES];
        for (int i = 0; i < root_list.count; i++) {
            Move m = root_list.moves[i];
            if (m == result.best_move && depth > 1) {
                root_scores[i] = SCORE_ROOT_PV;
            } else if (m == root_tt_move) {
                root_scores[i] = SCORE_TT_MOVE;
            } else {
                root_scores[i] = score_move(m, pos->side, 0, &info);
            }
        }
        int best_score = -INF;
        Move best_move = result.best_move;
        int alpha = -INF;
        int beta = INF;

        for (int i = 0; i < root_list.count; i++) {
            pick_next_move(&root_list, root_scores, i);
            Move m = root_list.moves[i];

            make_move(pos, m);
            bool is_pv = (i == 0);
            int score = -negamax(pos, depth - 1, 1, -beta, -alpha, is_pv, &info);
            unmake_move(pos, m);

            if (info.stopped) break;

            if (score > best_score) {
                best_score = score;
                best_move = m;
                alpha = score;
            }
        }

        if (info.stopped && depth > 1) break;

        result.best_move = best_move;
        result.score = best_score;
        result.depth = depth;
        result.nodes = info.nodes;
        int64_t elapsed = max_i64(search_info_elapsed(&info), 1);
        result.time_ms = (uint64_t)elapsed;
        result.nps = result.nodes * 1000 / (uint64_t)elapsed;

        print_search_info(out, depth, best_score, &result);

        if (is_mate_score(best_score)) break;
        if (limits->time_limit_ms > 0 && elapsed >= limits->time_limit_ms / 2) break;
        if (limits->soft_nodes > 0 && info.nodes >= limits->soft_nodes) break;
    }

    return result;
}

SearchResult iterative_deepening(Position *pos, int max_depth, int64_t time_limit_ms, FILE *out) {
    SearchLimits limits = {
        .max_depth = max_depth,
        .time_limit_ms = time_limit_ms,
        .soft_nodes = 0,
        .hard_nodes = 0
    };
    return search_position(pos, &limits, out);
}
