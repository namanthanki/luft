#ifndef DATAGEN_H
#define DATAGEN_H

#include <stdint.h>

void run_datagen(uint64_t num_games, int num_threads, const char *output_file, const char *book_file);

#endif // DATAGEN_H
