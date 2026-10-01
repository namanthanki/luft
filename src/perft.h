#pragma once

#include "position.h"
#include <stdio.h>

typedef struct {
    uint64_t nodes;
    uint64_t captures;
    uint64_t en_passant;
    uint64_t castles;
    uint64_t promotions;
} PerftStats;

uint64_t perft_node(Position *pos, int depth, PerftStats *stats);
void perft_divide(Position *pos, int depth);
void perft_run_suite(FILE *out);
