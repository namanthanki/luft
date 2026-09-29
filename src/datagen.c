#include "datagen.h"
#include "position.h"
#include "movegen.h"
#include "makemove.h"
#include "search.h"
#include "eval.h"
#include "time_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
typedef HANDLE thread_t;
typedef CRITICAL_SECTION mutex_t;
#define mutex_init(m) InitializeCriticalSection(m)
#define mutex_lock(m) EnterCriticalSection(m)
#define mutex_unlock(m) LeaveCriticalSection(m)
#define mutex_destroy(m) DeleteCriticalSection(m)
#else
#include <pthread.h>
typedef pthread_t thread_t;
typedef pthread_mutex_t mutex_t;
#define mutex_init(m) pthread_mutex_init(m, NULL)
#define mutex_lock(m) pthread_mutex_lock(m)
#define mutex_unlock(m) pthread_mutex_unlock(m)
#define mutex_destroy(m) pthread_mutex_destroy(m)
#endif

#define MAX_GAME_PLIES 300

static inline uint64_t xorshift64(uint64_t *state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

typedef struct {
    char *buffer;
    char **fens;
    int count;
} OpeningBook;

static OpeningBook global_book = {0};

static void load_book(const char *path) {
    if (!path) return;
    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("[datagen] Warning: failed to open book '%s', using startpos\n", path);
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        return;
    }

    global_book.buffer = malloc((size_t)size + 1);
    if (!global_book.buffer) {
        fclose(f);
        return;
    }

    size_t read_bytes = fread(global_book.buffer, 1, (size_t)size, f);
    fclose(f);
    global_book.buffer[read_bytes] = '\0';

    int line_count = 0;
    for (size_t i = 0; i < read_bytes; i++) {
        if (global_book.buffer[i] == '\n') line_count++;
    }
    line_count++;

    global_book.fens = malloc(sizeof(char *) * (size_t)line_count);
    if (!global_book.fens) {
        free(global_book.buffer);
        global_book.buffer = NULL;
        return;
    }

    char *p = global_book.buffer;
    int count = 0;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (!*p) break;

        char *line_start = p;
        while (*p && *p != '\r' && *p != '\n') {
            if (*p == '[' || *p == ';') *p = ' ';
            p++;
        }
        if (*p) {
            *p = '\0';
            p++;
        }

        char *end = line_start + strlen(line_start) - 1;
        while (end >= line_start && (*end == ' ' || *end == '\t')) {
            *end = '\0';
            end--;
        }

        if (strlen(line_start) >= 10) {
            global_book.fens[count++] = line_start;
        }
    }

    global_book.count = count;
    printf("[datagen] Loaded %d opening positions from '%s'\n", count, path);
}

typedef struct {
    int thread_id;
    uint64_t num_games;
    uint64_t *shared_games_completed;
    uint64_t *shared_positions_recorded;
    FILE *fout;
    mutex_t *lock;
    int64_t start_time;
    uint64_t target_games;
} ThreadData;

static bool is_game_over(Position *pos, double *result, MoveGenMasks *out_masks) {
    if (pos->halfmove >= 100) {
        *result = 0.5;
        return true;
    }

    if (position_is_repetition(pos)) {
        *result = 0.5;
        return true;
    }

    Bitboard pawns = pos->pieces[WHITE][PAWN] | pos->pieces[BLACK][PAWN];
    Bitboard rooks = pos->pieces[WHITE][ROOK] | pos->pieces[BLACK][ROOK];
    Bitboard queens = pos->pieces[WHITE][QUEEN] | pos->pieces[BLACK][QUEEN];
    if (pawns == 0 && rooks == 0 && queens == 0) {
        int w_minors = bb_popcount(pos->pieces[WHITE][KNIGHT] | pos->pieces[WHITE][BISHOP]);
        int b_minors = bb_popcount(pos->pieces[BLACK][KNIGHT] | pos->pieces[BLACK][BISHOP]);
        if (w_minors <= 1 && b_minors <= 1) {
            *result = 0.5;
            return true;
        }
    }

    if (pos->game_ply >= 250) {
        *result = 0.5;
        return true;
    }

    MoveList ml;
    MoveGenMasks masks = generate_moves(pos, &ml);
    if (out_masks) *out_masks = masks;

    if (ml.count == 0) {
        if (masks.checkers != 0) {
            *result = (pos->side == WHITE) ? 0.0 : 1.0;
        } else {
            *result = 0.5;
        }
        return true;
    }

    return false;
}

static bool generate_opening(Position *pos, uint64_t *rng) {
    for (int attempt = 0; attempt < 32; attempt++) {
        if (global_book.count > 0) {
            int idx = (int)(xorshift64(rng) % (uint64_t)global_book.count);
            if (!position_set_fen(pos, global_book.fens[idx])) {
                position_set_fen(pos, START_FEN);
            }
        } else {
            position_set_fen(pos, START_FEN);
        }

        int random_plies = 4 + (int)(xorshift64(rng) % 5);
        bool failed = false;

        for (int p = 0; p < random_plies; p++) {
            MoveList ml;
            generate_moves(pos, &ml);
            if (ml.count == 0) {
                failed = true;
                break;
            }
            int move_idx = (int)(xorshift64(rng) % (uint64_t)ml.count);
            make_move(pos, ml.moves[move_idx]);
        }

        if (failed) continue;

        MoveList ml;
        MoveGenMasks masks = generate_moves(pos, &ml);
        if (ml.count == 0 || masks.checkers != 0) continue;

        int score = evaluate(pos);
        if (abs(score) > 200) continue;

        return true;
    }

    position_set_fen(pos, START_FEN);
    return true;
}

#ifdef _WIN32
static DWORD WINAPI worker_routine(LPVOID arg)
#else
static void *worker_routine(void *arg)
#endif
{
    ThreadData *data = (ThreadData *)arg;
    uint64_t rng = (uint64_t)time(NULL) ^ ((uint64_t)data->thread_id * 0x9e3779b97f4a7c15ULL);
    xorshift64(&rng);

    char (*fen_buffer)[96] = malloc(sizeof(char[96]) * MAX_GAME_PLIES);
    if (!fen_buffer) {
#ifdef _WIN32
        return 1;
#else
        return NULL;
#endif
    }

    const SearchLimits limits = {
        .max_depth = 64,
        .time_limit_ms = 0,
        .soft_nodes = 5000,
        .hard_nodes = 8000000
    };

    while (1) {
        mutex_lock(data->lock);
        if (*data->shared_games_completed >= data->target_games) {
            mutex_unlock(data->lock);
            break;
        }
        mutex_unlock(data->lock);

        Position pos;
        generate_opening(&pos, &rng);
        search_clear_history();

        int recorded_count = 0;
        double result = 0.5;

        for (int ply = 0; ply < MAX_GAME_PLIES; ply++) {
            MoveGenMasks masks = {0};
            if (is_game_over(&pos, &result, &masks)) {
                break;
            }

            if (masks.checkers == 0 && recorded_count < MAX_GAME_PLIES) {
                position_to_fen(&pos, fen_buffer[recorded_count]);
                recorded_count++;
            }

            SearchResult sres = search_position(&pos, &limits, NULL);
            if (sres.best_move == 0) {
                MoveList ml;
                generate_moves(&pos, &ml);
                if (ml.count > 0) sres.best_move = ml.moves[0];
                else break;
            }

            make_move(&pos, sres.best_move);
        }

        mutex_lock(data->lock);
        for (int i = 0; i < recorded_count; i++) {
            fprintf(data->fout, "%s [%.1f]\n", fen_buffer[i], result);
        }

        (*data->shared_games_completed)++;
        (*data->shared_positions_recorded) += (uint64_t)recorded_count;
        uint64_t current_games = *data->shared_games_completed;
        uint64_t current_pos = *data->shared_positions_recorded;

        uint64_t interval = data->target_games >= 1000 ? (data->target_games / 20) : 50;
        if (current_games % interval == 0 || current_games == data->target_games) {
            int64_t elapsed = get_time_ms_signed() - data->start_time;
            double sec = elapsed > 0 ? (double)elapsed / 1000.0 : 0.001;
            double gps = (double)current_games / sec;
            printf("[datagen] %5llu / %5llu games (%5.1f%%) | %7llu positions | %6.1f games/s\n",
                   (unsigned long long)current_games,
                   (unsigned long long)data->target_games,
                   (double)current_games * 100.0 / (double)data->target_games,
                   (unsigned long long)current_pos,
                   gps);
            fflush(stdout);
            fflush(data->fout);
        }
        mutex_unlock(data->lock);
    }

    free(fen_buffer);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

void run_datagen(uint64_t num_games, int num_threads, const char *output_file, const char *book_file) {
    if (num_threads < 1) num_threads = 1;
    if (num_threads > 64) num_threads = 64;

    printf("[datagen] Starting %llu games on %d threads -> '%s'\n",
           (unsigned long long)num_games, num_threads, output_file);

    if (book_file) {
        load_book(book_file);
    }

    FILE *fout = fopen(output_file, "a");
    if (!fout) {
        fprintf(stderr, "[datagen] Error opening output file '%s'\n", output_file);
        return;
    }

    mutex_t lock;
    mutex_init(&lock);

    uint64_t games_completed = 0;
    uint64_t positions_recorded = 0;
    int64_t start_time = get_time_ms_signed();

    thread_t *threads = malloc(sizeof(thread_t) * (size_t)num_threads);
    ThreadData *tdata = malloc(sizeof(ThreadData) * (size_t)num_threads);

    for (int i = 0; i < num_threads; i++) {
        tdata[i].thread_id = i;
        tdata[i].num_games = num_games;
        tdata[i].shared_games_completed = &games_completed;
        tdata[i].shared_positions_recorded = &positions_recorded;
        tdata[i].fout = fout;
        tdata[i].lock = &lock;
        tdata[i].start_time = start_time;
        tdata[i].target_games = num_games;

#ifdef _WIN32
        threads[i] = CreateThread(NULL, 0, worker_routine, &tdata[i], 0, NULL);
#else
        pthread_create(&threads[i], NULL, worker_routine, &tdata[i]);
#endif
    }

#ifdef _WIN32
    WaitForMultipleObjects((DWORD)num_threads, threads, TRUE, INFINITE);
    for (int i = 0; i < num_threads; i++) {
        CloseHandle(threads[i]);
    }
#else
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
#endif

    int64_t elapsed = get_time_ms_signed() - start_time;
    double sec = elapsed > 0 ? (double)elapsed / 1000.0 : 0.001;
    printf("[datagen] Done: %llu games (%llu positions) in %.2fs (%.1f games/s)\n",
           (unsigned long long)games_completed,
           (unsigned long long)positions_recorded,
           sec,
           (double)games_completed / sec);

    fclose(fout);
    mutex_destroy(&lock);
    free(threads);
    free(tdata);

    if (global_book.fens) {
        free(global_book.fens);
        free(global_book.buffer);
        global_book.fens = NULL;
        global_book.buffer = NULL;
        global_book.count = 0;
    }
}
