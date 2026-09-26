#include "bitboard.h"
#include <stdio.h>

void bb_print(Bitboard bb) {
    printf("\n");
    for (int i = 0; i < 8; i++) {
        int rank = 7 - i;
        printf("  %d  ", rank + 1);
        for (int file = 0; file < 8; file++) {
            uint8_t sq = (uint8_t)(rank * 8 + file);
            printf("%c ", bb_is_bit_set(bb, sq) ? '1' : '.');
        }
        printf("\n");
    }
    printf("     a b c d e f g h\n\n");
}
