#include "perft.h"
#include "makemove.h"
#include "movegen.h"
#include "time_utils.h"


static inline void count_leaf(Move m, PerftStats *stats) {
    stats->nodes++;
    if (move_is_capture(m)) stats->captures++;
    if (move_flag(m) == FLAG_EN_PASSANT) stats->en_passant++;
    if (move_flag(m) == FLAG_CASTLE_K || move_flag(m) == FLAG_CASTLE_Q) stats->castles++;
    if (move_is_promo(m)) stats->promotions++;
}

uint64_t perft_node(Position *pos, int depth, PerftStats *stats) {
    if (depth == 0) return 1;

    MoveList list;
    generate_moves(pos, &list);

    if (depth == 1) {
        if (stats) {
            for (int i = 0; i < list.count; i++) {
                count_leaf(list.moves[i], stats);
            }
        }
        return (uint64_t)list.count;
    }

    uint64_t total = 0;
    for (int i = 0; i < list.count; i++) {
        make_move(pos, list.moves[i]);
        PerftStats sub = {0};
        total += perft_node(pos, depth - 1, stats ? &sub : NULL);
        if (stats) {
            stats->nodes += sub.nodes;
            stats->captures += sub.captures;
            stats->en_passant += sub.en_passant;
            stats->castles += sub.castles;
            stats->promotions += sub.promotions;
        }
        unmake_move(pos, list.moves[i]);
    }
    return total;
}

void perft_divide(Position *pos, int depth) {
    MoveList list;
    generate_moves(pos, &list);

    uint64_t t0 = get_time_ms();
    uint64_t total = 0;
    for (int i = 0; i < list.count; i++) {
        make_move(pos, list.moves[i]);
        PerftStats sub = {0};
        uint64_t count = (depth <= 1) ? 1 : perft_node(pos, depth - 1, &sub);
        total += count;
        char buf[6];
        move_to_uci(list.moves[i], buf);
        printf("%s: %llu\n", buf, (unsigned long long)count);
        unmake_move(pos, list.moves[i]);
    }
    uint64_t elapsed_ms = get_time_ms() - t0;
    uint64_t nps = (elapsed_ms > 0) ? (total * 1000 / elapsed_ms) : 0;
    printf("\nNodes searched: %llu\nTime: %llu ms\nNPS: %llu\n",
           (unsigned long long)total,
           (unsigned long long)elapsed_ms,
           (unsigned long long)nps);
    fflush(stdout);
}

typedef struct {
    const char *label;
    const char *fen;
    int depth;
    uint64_t expected;
} PerftCase;

#define KIWIPETE "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -"
#define POS3 "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"
#define POS4 "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"
#define POS5 "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"
#define POS6 "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10"

static const PerftCase SUITE[] = {
    { "start d1", START_FEN, 1, 20ULL },
    { "start d2", START_FEN, 2, 400ULL },
    { "start d3", START_FEN, 3, 8902ULL },
    { "start d4", START_FEN, 4, 197281ULL },
    { "start d5", START_FEN, 5, 4865609ULL },
    { "start d6", START_FEN, 6, 119060324ULL },

    { "kiwi d1",  KIWIPETE,  1, 48ULL },
    { "kiwi d2",  KIWIPETE,  2, 2039ULL },
    { "kiwi d3",  KIWIPETE,  3, 97862ULL },
    { "kiwi d4",  KIWIPETE,  4, 4085603ULL },
    { "kiwi d5",  KIWIPETE,  5, 193690690ULL },

    { "pos3 d1",  POS3,      1, 14ULL },
    { "pos3 d2",  POS3,      2, 191ULL },
    { "pos3 d3",  POS3,      3, 2812ULL },
    { "pos3 d4",  POS3,      4, 43238ULL },
    { "pos3 d5",  POS3,      5, 674624ULL },

    { "pos4 d1",  POS4,      1, 6ULL },
    { "pos4 d2",  POS4,      2, 264ULL },
    { "pos4 d3",  POS4,      3, 9467ULL },
    { "pos4 d4",  POS4,      4, 422333ULL },
    { "pos4 d5",  POS4,      5, 15833292ULL },

    { "pos5 d1",  POS5,      1, 44ULL },
    { "pos5 d2",  POS5,      2, 1486ULL },
    { "pos5 d3",  POS5,      3, 62379ULL },
    { "pos5 d4",  POS5,      4, 2103487ULL },

    { "pos6 d1",  POS6,      1, 46ULL },
    { "pos6 d2",  POS6,      2, 2079ULL },
    { "pos6 d3",  POS6,      3, 89890ULL },
    { "pos6 d4",  POS6,      4, 3894594ULL },
};

void perft_run_suite(FILE *out) {
    if (!out) out = stdout;

    fprintf(out, "\n===== PERFT SUITE =====\n");
    fprintf(out, "  %-10s  %-12s  %-12s  %s\n", "label", "result", "expected", "status");
    fprintf(out, "  ---------------------------------------------------\n");

    int num_cases = (int)(sizeof(SUITE) / sizeof(SUITE[0]));
    int passed = 0;
    int failed = 0;

    for (int i = 0; i < num_cases; i++) {
        Position pos;
        if (!position_set_fen(&pos, SUITE[i].fen)) {
            fprintf(out, "  %-10s  FEN parse error\n", SUITE[i].label);
            failed++;
            continue;
        }

        uint64_t t0 = get_time_ms();
        PerftStats stats = {0};
        uint64_t nodes = perft_node(&pos, SUITE[i].depth, &stats);
        uint64_t elapsed_ms = get_time_ms() - t0;
        uint64_t knps = (elapsed_ms > 0) ? (nodes / elapsed_ms) : 0;
        bool ok = (nodes == SUITE[i].expected);

        if (ok) passed++; else failed++;
        fprintf(out, "  %-10s  %-12llu  %-12llu  %s  (%llu knps)\n",
                SUITE[i].label,
                (unsigned long long)nodes,
                (unsigned long long)SUITE[i].expected,
                ok ? "PASS" : "FAIL",
                (unsigned long long)knps);
        fflush(out);
    }

    fprintf(out, "  ---------------------------------------------------\n");
    fprintf(out, "  %d passed, %d failed\n\n", passed, failed);
    fflush(out);
}
