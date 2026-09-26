#include "search.h"
#include "eval.h"
#include "movegen.h"
#include "makemove.h"
#include "time_utils.h"


typedef struct {
    int64_t start_time;
    int64_t time_limit_ms;
    uint64_t nodes;
    bool stopped;
} SearchInfo;

static inline int64_t search_info_elapsed(const SearchInfo *info) {
    return get_time_ms_signed() - info->start_time;
}

static inline void check_time(SearchInfo *info) {
    if (info->time_limit_ms <= 0) return;
    if ((info->nodes & 2047) != 0) return;
    if (search_info_elapsed(info) >= info->time_limit_ms) {
        info->stopped = true;
    }
}

static bool is_repetition(const Position *pos) {
    int ply = pos->game_ply;
    if (ply < 4 || pos->halfmove < 4) return false;
    int i = ply - 2;
    while (1) {
        if (pos->history[i].hash == pos->hash) return true;
        if (i < 2 || pos->history[i].halfmove == 0) break;
        i -= 2;
    }
    return false;
}

static int negamax(Position *pos, int depth, int ply, int alpha, int beta, SearchInfo *info) {
    info->nodes++;
    check_time(info);
    if (info->stopped) return 0;

    if (pos->halfmove >= 100 || is_repetition(pos)) return DRAW_SCORE;
    if (depth == 0) return evaluate(pos);

    MoveList list;
    MoveGenMasks masks = generate_moves(pos, &list);

    if (list.count == 0) {
        if (masks.checkers != 0) {
            return -(MATE_SCORE - ply);
        }
        return DRAW_SCORE;
    }

    int best_score = -INF;
    for (int i = 0; i < list.count; i++) {
        make_move(pos, list.moves[i]);
        int score = -negamax(pos, depth - 1, ply + 1, -beta, -alpha, info);
        unmake_move(pos, list.moves[i]);

        if (info->stopped) return 0;

        if (score > best_score) {
            best_score = score;
            if (score > alpha) {
                alpha = score;
            }
        }

        if (score >= beta) {
            return best_score;
        }
    }
    return best_score;
}

SearchResult iterative_deepening(Position *pos, int max_depth, int64_t time_limit_ms, FILE *out) {
    if (!out) out = stdout;

    SearchInfo info;
    info.start_time = get_time_ms_signed();
    info.time_limit_ms = time_limit_ms;
    info.nodes = 0;
    info.stopped = false;

    SearchResult result;
    memset(&result, 0, sizeof(result));

    MoveList root_list;
    generate_moves(pos, &root_list);
    if (root_list.count == 0) return result;
    result.best_move = root_list.moves[0];

    for (int depth = 1; depth <= max_depth; depth++) {
        int best_score = -INF;
        Move best_move = result.best_move;
        int alpha = -INF;
        int beta = INF;

        for (int i = 0; i < root_list.count; i++) {
            make_move(pos, root_list.moves[i]);
            int score = -negamax(pos, depth - 1, 1, -beta, -alpha, &info);
            unmake_move(pos, root_list.moves[i]);

            if (info.stopped) break;

            if (score > best_score) {
                best_score = score;
                best_move = root_list.moves[i];
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

        if (is_mate_score(best_score)) break;
        if (time_limit_ms > 0 && elapsed >= time_limit_ms / 2) break;
    }

    return result;
}
