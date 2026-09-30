#include "search.h"
#include "eval.h"
#include "movegen.h"
#include "makemove.h"
#include "moveorder.h"
#include "time_utils.h"


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
    if ((info->nodes & 2047) != 0) return;
    if (search_info_elapsed(info) >= info->time_limit_ms) {
        info->stopped = true;
    }
}

static int qsearch(Position *pos, int ply, int alpha, int beta, SearchInfo *info) {
    info->nodes++;
    check_time(info);
    if (info->stopped) return 0;

    if (pos->halfmove >= 100 || position_is_repetition(pos)) return DRAW_SCORE;

    bool in_check = is_in_check(pos, pos->side);
    if (ply >= MAX_SEARCH_PLY - 1 || pos->game_ply >= MAX_GAME_PLY - 1) {
        return in_check ? 0 : evaluate(pos);
    }

    int best_score = -INF;

    if (!in_check) {
        int stand_pat = evaluate(pos);
        if (stand_pat >= beta) return stand_pat;
        if (stand_pat > alpha) {
            alpha = stand_pat;
        }
        best_score = stand_pat;
    }

    MoveList list;
    if (in_check) {
        MoveGenMasks masks = generate_moves(pos, &list);
        (void)masks;
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
        }

        if (score >= beta) {
            return best_score;
        }

        if (score > alpha) {
            alpha = score;
        }
    }

    return best_score;
}

static int negamax(Position *pos, int depth, int ply, int alpha, int beta, SearchInfo *info) {
    info->nodes++;
    check_time(info);
    if (info->stopped) return 0;

    if (pos->halfmove >= 100 || position_is_repetition(pos)) return DRAW_SCORE;
    if (ply >= MAX_SEARCH_PLY - 1 || pos->game_ply >= MAX_GAME_PLY - 1) return evaluate(pos);
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
    score_moves(&list, scores, pos->side, ply, info);

    int best_score = -INF;
    for (int i = 0; i < list.count; i++) {
        pick_next_move(&list, scores, i);
        Move m = list.moves[i];

        make_move(pos, m);
        int score = -negamax(pos, depth - 1, ply + 1, -beta, -alpha, info);
        unmake_move(pos, m);

        if (info->stopped) return 0;

        if (score > best_score) {
            best_score = score;
            if (score > alpha) {
                alpha = score;
            }
        }

        if (score >= beta) {
            if (move_is_quiet(m)) {
                if (ply < MAX_SEARCH_PLY) {
                    if (m != info->killers[0][ply]) {
                        info->killers[1][ply] = info->killers[0][ply];
                        info->killers[0][ply] = m;
                    }
                }
                if (info->history) {
                    int bonus = history_bonus(depth);
                    history_update(info->history, pos->side, move_from(m), move_to(m), bonus);
                }
            }
            return best_score;
        }
    }
    return best_score;
}

SearchResult search_position(Position *pos, const SearchLimits *limits, FILE *out) {
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
    int max_d = limits->max_depth > 0 ? limits->max_depth : 64;

    for (int depth = 1; depth <= max_d; depth++) {
        int root_scores[MAX_MOVES];
        for (int i = 0; i < root_list.count; i++) {
            if (root_list.moves[i] == result.best_move && depth > 1) {
                root_scores[i] = 100000;
            } else {
                root_scores[i] = score_move(root_list.moves[i], pos->side, 0, &info);
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
            int score = -negamax(pos, depth - 1, 1, -beta, -alpha, &info);
            unmake_move(pos, m);

            if (info.stopped) break;

            if (score > best_score) {
                best_score = score;
                best_move = m;
                if (score > alpha) {
                    alpha = score;
                }
            }
        }

        if (info.stopped && depth > 1) break;

        result.best_move = best_move;
        result.score = best_score;
        result.depth = depth;
        result.nodes = info.nodes;
        int64_t elapsed = search_info_elapsed(&info);
        if (elapsed < 1) elapsed = 1;
        result.time_ms = (uint64_t)elapsed;
        result.nps = result.nodes * 1000 / (uint64_t)elapsed;

        if (out) {
            char pv_str[6];
            move_to_uci(best_move, pv_str);

            if (is_mate_score(best_score)) {
                int abs_s = best_score < 0 ? -best_score : best_score;
                int mate_in = (MATE_SCORE - abs_s + 1) / 2;
                const char *sign = (best_score < 0) ? "-" : "";
                fprintf(out, "info depth %d score mate %s%d nodes %llu nps %llu time %llu pv %s\n",
                        depth, sign, mate_in,
                        (unsigned long long)result.nodes,
                        (unsigned long long)result.nps,
                        (unsigned long long)result.time_ms,
                        pv_str);
            } else {
                fprintf(out, "info depth %d score cp %d nodes %llu nps %llu time %llu pv %s\n",
                        depth, best_score,
                        (unsigned long long)result.nodes,
                        (unsigned long long)result.nps,
                        (unsigned long long)result.time_ms,
                        pv_str);
            }
            fflush(out);
        }

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
