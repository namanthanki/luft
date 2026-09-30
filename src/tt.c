#include "tt.h"
#include <stdlib.h>
#include <string.h>

TranspositionTable g_tt = {0};

void tt_init(TranspositionTable *tt, size_t mb) {
    tt->entries = NULL;
    tt->capacity = 0;
    tt->age = 0;
    tt_resize(tt, mb);
}

void tt_resize(TranspositionTable *tt, size_t mb) {
    if (tt->entries) {
        free(tt->entries);
        tt->entries = NULL;
    }

    if (mb == 0) mb = 16;
    size_t bytes = mb * 1024 * 1024;
    size_t count = bytes / sizeof(TTEntry);

    tt->capacity = count;
    tt->entries = (TTEntry *)calloc(tt->capacity, sizeof(TTEntry));
    tt->age = 0;
}

void tt_clear(TranspositionTable *tt) {
    if (tt->entries && tt->capacity > 0) {
        memset(tt->entries, 0, tt->capacity * sizeof(TTEntry));
    }
    tt->age = 0;
}

void tt_free(TranspositionTable *tt) {
    if (tt->entries) {
        free(tt->entries);
        tt->entries = NULL;
    }
    tt->capacity = 0;
    tt->age = 0;
}
