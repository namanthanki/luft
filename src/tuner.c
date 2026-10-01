#include "tuner.h"
#include "position.h"
#include "bitboard.h"
#include "types.h"
#include "eval.h"
#include "movegen.h"
#include "time_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

typedef struct {
    int8_t f[5];
    uint8_t phase;
    float target;
} TunerEntry;

static inline double sigmoid(double z) {
    return 1.0 / (1.0 + exp(-z));
}

static void shuffle_dataset(TunerEntry *data, size_t n, uint64_t *rng) {
    for (size_t i = n - 1; i > 0; i--) {
        *rng ^= *rng << 13;
        *rng ^= *rng >> 7;
        *rng ^= *rng << 17;
        size_t j = (size_t)(*rng % (i + 1));
        TunerEntry tmp = data[i];
        data[i] = data[j];
        data[j] = tmp;
    }
}

void run_tuner(const char *data_file, int epochs) {
    if (epochs <= 0) epochs = 20;

    FILE *f = fopen(data_file, "r");
    if (!f) {
        fprintf(stderr, "[tuner] Error: could not open '%s'\n", data_file);
        return;
    }

    size_t capacity = 131072;
    size_t count = 0;
    TunerEntry *dataset = malloc(capacity * sizeof(TunerEntry));
    if (!dataset) {
        fprintf(stderr, "[tuner] Error: out of memory allocating dataset\n");
        fclose(f);
        return;
    }

    char line[512];
    Position pos;
    size_t skipped_check = 0;

    while (fgets(line, sizeof(line), f)) {
        char *bracket = strchr(line, '[');
        if (!bracket) continue;

        double result_w = atof(bracket + 1);

        char *fen_end = bracket;
        while (fen_end > line && (*(fen_end - 1) == ' ' || *(fen_end - 1) == '\t')) {
            fen_end--;
        }
        *fen_end = '\0';

        if (!position_set_fen(&pos, line)) continue;
        if (is_in_check(&pos, pos.side)) {
            skipped_check++;
            continue;
        }

        if (count >= capacity) {
            capacity *= 2;
            TunerEntry *new_dataset = realloc(dataset, capacity * sizeof(TunerEntry));
            if (!new_dataset) {
                fprintf(stderr, "[tuner] Error: failed to expand dataset to %zu entries\n", capacity);
                break;
            }
            dataset = new_dataset;
        }

        Color us = pos.side;
        Color them = flip_color(us);

        dataset[count].f[0] = (int8_t)(bb_popcount(pos.pieces[us][PAWN])   - bb_popcount(pos.pieces[them][PAWN]));
        dataset[count].f[1] = (int8_t)(bb_popcount(pos.pieces[us][KNIGHT]) - bb_popcount(pos.pieces[them][KNIGHT]));
        dataset[count].f[2] = (int8_t)(bb_popcount(pos.pieces[us][BISHOP]) - bb_popcount(pos.pieces[them][BISHOP]));
        dataset[count].f[3] = (int8_t)(bb_popcount(pos.pieces[us][ROOK])   - bb_popcount(pos.pieces[them][ROOK]));
        dataset[count].f[4] = (int8_t)(bb_popcount(pos.pieces[us][QUEEN])  - bb_popcount(pos.pieces[them][QUEEN]));

        dataset[count].phase = (pos.phase > 24) ? 24 : (uint8_t)pos.phase;
        dataset[count].target = (us == WHITE) ? (float)result_w : (1.0f - (float)result_w);
        count++;
    }
    fclose(f);

    printf("[tuner] Loaded %zu positions from '%s'\n", count, data_file);
    if (count == 0) {
        free(dataset);
        return;
    }

    static const double result_scale = 300.0;
    static const double initial_weight = 100.0 / 300.0;
    double w_mg[5] = { initial_weight, initial_weight, initial_weight, initial_weight, initial_weight };
    double w_eg[5] = { initial_weight, initial_weight, initial_weight, initial_weight, initial_weight };

    double momentum_mg[5] = {0};
    double momentum_eg[5] = {0};

    static const size_t batch_size = 64;
    static const double mu = 0.9;
    static const double lambda = 1e-4;
    static const double lr_max = 0.01;
    static const double lr_min = 0.001;
    static const double pi = 3.14159265358979323846;

    uint64_t rng = 0x9e3779b97f4a7c15ULL;
    int64_t tune_start = get_time_ms_signed();

    for (int epoch = 1; epoch <= epochs; epoch++) {
        shuffle_dataset(dataset, count, &rng);

        double progress = (double)(epoch - 1) / (double)epochs;
        double lr = lr_min + (lr_max - lr_min) * 0.5 * (1.0 + cos(pi * progress));

        double total_loss = 0.0;

        for (size_t b = 0; b < count; b += batch_size) {
            size_t b_end = b + batch_size;
            if (b_end > count) b_end = count;
            size_t b_size = b_end - b;

            double grad_mg[5] = {0};
            double grad_eg[5] = {0};

            for (size_t i = b; i < b_end; i++) {
                double dot_mg = w_mg[0] * dataset[i].f[0] + w_mg[1] * dataset[i].f[1] +
                                w_mg[2] * dataset[i].f[2] + w_mg[3] * dataset[i].f[3] +
                                w_mg[4] * dataset[i].f[4];
                double dot_eg = w_eg[0] * dataset[i].f[0] + w_eg[1] * dataset[i].f[1] +
                                w_eg[2] * dataset[i].f[2] + w_eg[3] * dataset[i].f[3] +
                                w_eg[4] * dataset[i].f[4];

                double phase = (double)dataset[i].phase / 24.0;
                double pred = sigmoid(phase * dot_mg + (1.0 - phase) * dot_eg);
                double err = pred - (double)dataset[i].target;
                total_loss += err * err;

                for (int j = 0; j < 5; j++) {
                    grad_mg[j] += err * phase * dataset[i].f[j];
                    grad_eg[j] += err * (1.0 - phase) * dataset[i].f[j];
                }
            }

            double inv_b = 1.0 / (double)b_size;

            for (int j = 0; j < 5; j++) {
                double avg_gmg = grad_mg[j] * inv_b + lambda * w_mg[j];
                double avg_geg = grad_eg[j] * inv_b + lambda * w_eg[j];

                momentum_mg[j] = mu * momentum_mg[j] + avg_gmg;
                momentum_eg[j] = mu * momentum_eg[j] + avg_geg;

                w_mg[j] -= lr * momentum_mg[j];
                w_eg[j] -= lr * momentum_eg[j];

                if (w_mg[j] < 0.05) w_mg[j] = 0.05;
                if (w_eg[j] < 0.05) w_eg[j] = 0.05;
            }
        }

        total_loss /= (double)count;
        double norm_scale = 100.0 / (w_mg[0] * result_scale);

        if (epoch % 20 == 0 || epoch == 1 || epoch == epochs) {
            printf("[tuner] epoch %3d/%d | mse: %.6f | mg: [%3d, %3d, %3d, %3d, %3d] | eg: [%3d, %3d, %3d, %3d, %3d]\n",
                   epoch, epochs, total_loss,
                   (int)round(w_mg[0] * result_scale * norm_scale),
                   (int)round(w_mg[1] * result_scale * norm_scale),
                   (int)round(w_mg[2] * result_scale * norm_scale),
                   (int)round(w_mg[3] * result_scale * norm_scale),
                   (int)round(w_mg[4] * result_scale * norm_scale),
                   (int)round(w_eg[0] * result_scale * norm_scale),
                   (int)round(w_eg[1] * result_scale * norm_scale),
                   (int)round(w_eg[2] * result_scale * norm_scale),
                   (int)round(w_eg[3] * result_scale * norm_scale),
                   (int)round(w_eg[4] * result_scale * norm_scale));
            fflush(stdout);
        }
    }

    int64_t tune_elapsed = get_time_ms_signed() - tune_start;
    double norm_scale = 100.0 / (w_mg[0] * result_scale);

    printf("[tuner] done in %.2fs\n\n", (double)tune_elapsed / 1000.0);
    printf("static const int PIECE_VALUES_MG[6] = {\n");
    printf("    %d,  // PAWN\n",   (int)round(w_mg[0] * result_scale * norm_scale));
    printf("    %d,  // KNIGHT\n", (int)round(w_mg[1] * result_scale * norm_scale));
    printf("    %d,  // BISHOP\n", (int)round(w_mg[2] * result_scale * norm_scale));
    printf("    %d,  // ROOK\n",   (int)round(w_mg[3] * result_scale * norm_scale));
    printf("    %d,  // QUEEN\n",  (int)round(w_mg[4] * result_scale * norm_scale));
    printf("    0    // KING\n");
    printf("};\n\n");

    printf("static const int PIECE_VALUES_EG[6] = {\n");
    printf("    %d,  // PAWN\n",   (int)round(w_eg[0] * result_scale * norm_scale));
    printf("    %d,  // KNIGHT\n", (int)round(w_eg[1] * result_scale * norm_scale));
    printf("    %d,  // BISHOP\n", (int)round(w_eg[2] * result_scale * norm_scale));
    printf("    %d,  // ROOK\n",   (int)round(w_eg[3] * result_scale * norm_scale));
    printf("    %d,  // QUEEN\n",  (int)round(w_eg[4] * result_scale * norm_scale));
    printf("    0    // KING\n");
    printf("};\n");

    free(dataset);
}
