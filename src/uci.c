#include "uci.h"
#include "position.h"
#include "movegen.h"
#include "makemove.h"
#include "search.h"
#include "perft.h"
#include "eval.h"
#include "tt.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static bool parse_and_make_move(Position *pos, const char *tok) {
    if (strlen(tok) < 4) return false;

    int from_file = tok[0] - 'a';
    int from_rank = tok[1] - '1';
    int to_file = tok[2] - 'a';
    int to_rank = tok[3] - '1';

    if (from_file < 0 || from_file > 7 || from_rank < 0 || from_rank > 7 ||
        to_file < 0 || to_file > 7 || to_rank < 0 || to_rank > 7) {
        return false;
    }

    uint8_t from_sq = (uint8_t)(from_rank * 8 + from_file);
    uint8_t to_sq = (uint8_t)(to_rank * 8 + to_file);

    Piece promo = NO_PROMO;
    if (strlen(tok) >= 5) {
        switch (tok[4]) {
            case 'q': promo = QUEEN; break;
            case 'r': promo = ROOK; break;
            case 'b': promo = BISHOP; break;
            case 'n': promo = KNIGHT; break;
            default: break;
        }
    }

    MoveList list;
    generate_moves(pos, &list);

    for (int i = 0; i < list.count; i++) {
        Move m = list.moves[i];
        if (move_from(m) == from_sq && move_to(m) == to_sq) {
            if (move_is_promo(m)) {
                if (promo == NO_PROMO || move_promo(m) != promo) continue;
            }
            make_move(pos, m);
            return true;
        }
    }
    return false;
}

static void handle_position(Position *pos, char *line) {
    char *token = strtok(line, " \t\r\n"); // "position"
    token = strtok(NULL, " \t\r\n");
    if (!token) return;

    if (strcmp(token, "startpos") == 0) {
        position_set_fen(pos, START_FEN);
        token = strtok(NULL, " \t\r\n");
        if (!token || strcmp(token, "moves") != 0) return;
    } else if (strcmp(token, "fen") == 0) {
        char fen_buf[256];
        fen_buf[0] = '\0';
        bool saw_moves = false;

        while ((token = strtok(NULL, " \t\r\n")) != NULL) {
            if (strcmp(token, "moves") == 0) {
                saw_moves = true;
                break;
            }
            if (fen_buf[0] != '\0') {
                size_t cur = strlen(fen_buf);
                if (cur + 1 < sizeof(fen_buf)) {
                    fen_buf[cur] = ' ';
                    fen_buf[cur + 1] = '\0';
                }
            }
            size_t cur = strlen(fen_buf);
            if (cur < sizeof(fen_buf) - 1) {
                strncat(fen_buf, token, sizeof(fen_buf) - cur - 1);
            }
        }

        position_set_fen(pos, fen_buf);
        if (!saw_moves) return;
    } else {
        return;
    }

    while ((token = strtok(NULL, " \t\r\n")) != NULL) {
        if (pos->game_ply >= MAX_GAME_PLY - 1) break;
        parse_and_make_move(pos, token);
    }
}

static int64_t get_param(const char *line, const char *param) {
    size_t plen = strlen(param);
    const char *p = line;
    while ((p = strstr(p, param)) != NULL) {
        if (p == line || isspace((unsigned char)*(p - 1))) {
            char next = *(p + plen);
            if (isspace((unsigned char)next) || next == '\0') {
                p += plen;
                while (*p == ' ' || *p == '\t') p++;
                return atoll(p);
            }
        }
        p += plen;
    }
    return 0;
}

static void handle_go(Position *pos, const char *line) {
    bool is_infinite = (strstr(line, "infinite") != NULL);
    int64_t depth_raw = get_param(line, "depth");
    int64_t movetime_raw = get_param(line, "movetime");
    int64_t wtime_raw = get_param(line, "wtime");
    int64_t btime_raw = get_param(line, "btime");
    int64_t winc_raw = get_param(line, "winc");
    int64_t binc_raw = get_param(line, "binc");

    bool has_time = (movetime_raw > 0 || wtime_raw > 0 || btime_raw > 0);
    int max_depth;
    if (is_infinite) {
        max_depth = 64;
    } else if (depth_raw > 0 && depth_raw <= 64) {
        max_depth = (int)depth_raw;
    } else if (has_time) {
        max_depth = 64;
    } else {
        max_depth = 5;
    }

    int64_t time_limit_ms = 0;
    if (is_infinite) {
        time_limit_ms = 0;
    } else if (movetime_raw > 0) {
        time_limit_ms = movetime_raw;
    } else if (wtime_raw > 0 || btime_raw > 0) {
        int64_t my_time = (pos->side == WHITE) ? wtime_raw : btime_raw;
        int64_t my_inc = (pos->side == WHITE) ? winc_raw : binc_raw;
        time_limit_ms = my_time / 20 + my_inc / 2;
        if (time_limit_ms < 1) time_limit_ms = 1;
    }

    SearchResult res = iterative_deepening(pos, max_depth, time_limit_ms, stdout);
    char buf[6];
    if (res.best_move != MOVE_NULL && move_from(res.best_move) != SQ_NONE) {
        move_to_uci(res.best_move, buf);
        printf("bestmove %s\n", buf);
    } else {
        printf("bestmove 0000\n");
    }
    fflush(stdout);
}

static void handle_setoption(char *line) {
    char *name_token = strstr(line, "name");
    char *val_token = strstr(line, "value");
    if (!name_token || !val_token) return;

    name_token += 4;
    while (*name_token == ' ') name_token++;

    if (strncmp(name_token, "Hash", 4) == 0 || strncmp(name_token, "hash", 4) == 0) {
        val_token += 5;
        while (*val_token == ' ') val_token++;
        int mb = atoi(val_token);
        if (mb >= 1 && mb <= 2048) {
            tt_resize(&g_tt, (size_t)mb);
        }
    }
}

void uci_run(void) {
    Position pos;
    position_set_fen(&pos, START_FEN);

    char line[4096];
    while (fgets(line, sizeof(line), stdin)) {
        // Trim trailing newline / carriage return
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' ')) {
            line[--len] = '\0';
        }
        if (len == 0) continue;

        if (strcmp(line, "uci") == 0) {
            printf("id name %s\n", ENGINE_NAME);
            printf("id author %s\n", ENGINE_AUTHOR);
            printf("option name Hash type spin default 16 min 1 max 2048\n");
            printf("option name Threads type spin default 1 min 1 max 1\n");
            printf("uciok\n");
            fflush(stdout);
        } else if (strcmp(line, "isready") == 0) {
            printf("readyok\n");
            fflush(stdout);
        } else if (strcmp(line, "ucinewgame") == 0) {
            position_set_fen(&pos, START_FEN);
            search_clear_history();
            tt_clear(&g_tt);
        } else if (strncmp(line, "setoption", 9) == 0) {
            handle_setoption(line);
        } else if (strncmp(line, "position", 8) == 0) {
            handle_position(&pos, line);
        } else if (strncmp(line, "go", 2) == 0) {
            handle_go(&pos, line);
        } else if (strcmp(line, "stop") == 0) {
            printf("bestmove 0000\n");
            fflush(stdout);
        } else if (strncmp(line, "perft", 5) == 0) {
            char *p = line + 5;
            while (*p == ' ') p++;
            if (strncmp(p, "suite", 5) == 0) {
                perft_run_suite(stdout);
            } else {
                int depth = atoi(p);
                if (depth < 1) depth = 1;
                perft_divide(&pos, depth);
            }
        } else if (strcmp(line, "eval") == 0) {
            position_display(&pos);
            int score = evaluate(&pos);
            printf("  eval:     %d cp (%s)\n\n", score, pos.side == WHITE ? "white" : "black");
            fflush(stdout);
        } else if (strcmp(line, "d") == 0) {
            position_display(&pos);
            fflush(stdout);
        } else if (strcmp(line, "quit") == 0) {
            break;
        }
    }
}
