#include "attacks.h"
#include "zobrist.h"
#include "uci.h"
#include "datagen.h"
#include "tuner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    attacks_init();
    zobrist_init();

    if (argc > 1) {
        if (strcmp(argv[1], "datagen") == 0) {
            uint64_t num_games = (argc > 2) ? strtoull(argv[2], NULL, 10) : 10000;
            int num_threads = (argc > 3) ? atoi(argv[3]) : 4;
            const char *output_file = (argc > 4) ? argv[4] : "data.txt";
            const char *book_file = (argc > 5) ? argv[5] : NULL;
            run_datagen(num_games, num_threads, output_file, book_file);
            return 0;
        }

        if (strcmp(argv[1], "tune") == 0) {
            const char *data_file = (argc > 2) ? argv[2] : "data.txt";
            int epochs = (argc > 3) ? atoi(argv[3]) : 100;
            run_tuner(data_file, epochs);
            return 0;
        }

        if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("Luft Chess Engine\n");
            printf("Usage:\n");
            printf("  luft                     Run UCI loop (default)\n");
            printf("  luft uci                 Run UCI loop\n");
            printf("  luft datagen [games] [threads] [out.txt] [book.epd]   Run self-play datagen\n");
            printf("  luft tune [data.txt] [epochs]                          Run Texel material tuner\n");
            return 0;
        }
    }

    uci_run();
    return 0;
}
