#include "tuner.h"
#include "position.h"
#include "bitboard.h"
#include "types.h"
#include "eval.h"
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

static double compute_loss(const TunerEntry *dataset, size_t count, const double w_mg[5], const double w_eg[5], double K) {
    double loss = 0.0;
    for (size_t i = 0; i < count; i++) {
        double eval_mg = w_mg[0] * dataset[i].f[0]
                       + w_mg[1] * dataset[i].f[1]
                       + w_mg[2] * dataset[i].f[2]
                       + w_mg[3] * dataset[i].f[3]
                       + w_mg[4] * dataset[i].f[4];

        double eval_eg = w_eg[0] * dataset[i].f[0]
                       + w_eg[1] * dataset[i].f[1]
                       + w_eg[2] * dataset[i].f[2]
                       + w_eg[3] * dataset[i].f[3]
                       + w_eg[4] * dataset[i].f[4];

        double phase = (double)dataset[i].phase;
        double eval = (eval_mg * phase + eval_eg * (24.0 - phase)) / 24.0;

        double pred = sigmoid(K * eval);
        double err = pred - (double)dataset[i].target;
        loss += err * err;
    }
    return loss / (double)count;
}

static double find_optimal_k(const TunerEntry *dataset, size_t count, const double w_mg[5], const double w_eg[5]) {
    double a = 0.0005, b = 0.0300;
    double r = (sqrt(5.0) - 1.0) / 2.0;
    double c = b - r * (b - a);
    double d = a + r * (b - a);
    double fc = compute_loss(dataset, count, w_mg, w_eg, c);
    double fd = compute_loss(dataset, count, w_mg, w_eg, d);

    for (int iter = 0; iter < 24; iter++) {
        if (fc < fd) {
            b = d;
            d = c;
            fd = fc;
            c = b - r * (b - a);
            fc = compute_loss(dataset, count, w_mg, w_eg, c);
        } else {
            a = c;
            c = d;
            fc = fd;
            d = a + r * (b - a);
            fd = compute_loss(dataset, count, w_mg, w_eg, d);
        }
    }
    return (a + b) / 2.0;
}

void run_tuner(const char *data_file, int epochs) {
    if (epochs <= 0) epochs = 100;

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

    double w_mg[5] = { 100.0, 200.0, 285.0, 395.0, 820.0 };
    double w_eg[5] = { 100.0, 200.0, 285.0, 395.0, 820.0 };

    double K = find_optimal_k(dataset, count, w_mg, w_eg);
    double initial_loss = compute_loss(dataset, count, w_mg, w_eg, K);
    printf("[tuner] Initial K: %.6f | MSE: %.6f\n", K, initial_loss);

    double m_mg[5] = {0}, v_mg[5] = {0};
    double m_eg[5] = {0}, v_eg[5] = {0};
    const double alpha = 2.0;
    const double beta1 = 0.9;
    const double beta2 = 0.999;
    const double eps = 1e-8;

    double beta1_pow = 1.0;
    double beta2_pow = 1.0;

    int64_t tune_start = get_time_ms_signed();

    for (int epoch = 1; epoch <= epochs; epoch++) {
        if (epoch > 1 && epoch % 25 == 0) {
            K = find_optimal_k(dataset, count, w_mg, w_eg);
        }

        double loss = 0.0;
        double grad_mg[5] = {0};
        double grad_eg[5] = {0};

        for (size_t i = 0; i < count; i++) {
            double eval_mg = w_mg[0] * dataset[i].f[0]
                           + w_mg[1] * dataset[i].f[1]
                           + w_mg[2] * dataset[i].f[2]
                           + w_mg[3] * dataset[i].f[3]
                           + w_mg[4] * dataset[i].f[4];

            double eval_eg = w_eg[0] * dataset[i].f[0]
                           + w_eg[1] * dataset[i].f[1]
                           + w_eg[2] * dataset[i].f[2]
                           + w_eg[3] * dataset[i].f[3]
                           + w_eg[4] * dataset[i].f[4];

            double phase = (double)dataset[i].phase;
            double p_mg = phase / 24.0;
            double p_eg = (24.0 - phase) / 24.0;
            double eval = eval_mg * p_mg + eval_eg * p_eg;

            double pred = sigmoid(K * eval);
            double err = pred - (double)dataset[i].target;
            loss += err * err;

            double d_sig = err * pred * (1.0 - pred);
            double factor = 2.0 * d_sig * K;

            for (int j = 0; j < 5; j++) {
                grad_mg[j] += factor * p_mg * dataset[i].f[j];
                grad_eg[j] += factor * p_eg * dataset[i].f[j];
            }
        }

        double inv_n = 1.0 / (double)count;
        loss *= inv_n;
        for (int j = 0; j < 5; j++) {
            grad_mg[j] *= inv_n;
            grad_eg[j] *= inv_n;
        }

        beta1_pow *= beta1;
        beta2_pow *= beta2;

        for (int j = 1; j < 5; j++) {
            m_mg[j] = beta1 * m_mg[j] + (1.0 - beta1) * grad_mg[j];
            v_mg[j] = beta2 * v_mg[j] + (1.0 - beta2) * (grad_mg[j] * grad_mg[j]);

            double m_hat = m_mg[j] / (1.0 - beta1_pow);
            double v_hat = v_mg[j] / (1.0 - beta2_pow);

            w_mg[j] -= alpha * m_hat / (sqrt(v_hat) + eps);
            if (w_mg[j] < 50.0) w_mg[j] = 50.0;
        }

        for (int j = 0; j < 5; j++) {
            m_eg[j] = beta1 * m_eg[j] + (1.0 - beta1) * grad_eg[j];
            v_eg[j] = beta2 * v_eg[j] + (1.0 - beta2) * (grad_eg[j] * grad_eg[j]);

            double m_hat = m_eg[j] / (1.0 - beta1_pow);
            double v_hat = v_eg[j] / (1.0 - beta2_pow);

            w_eg[j] -= alpha * m_hat / (sqrt(v_hat) + eps);
            if (w_eg[j] < 50.0) w_eg[j] = 50.0;
        }

        if (epoch % 10 == 0 || epoch == 1 || epoch == epochs) {
            printf("[tuner] Epoch %3d/%d | MSE: %.6f | K: %.6f\n", epoch, epochs, loss, K);
            printf("  MG: [%3d, %3d, %3d, %3d, %3d]\n",
                   (int)round(w_mg[0]), (int)round(w_mg[1]), (int)round(w_mg[2]), (int)round(w_mg[3]), (int)round(w_mg[4]));
            printf("  EG: [%3d, %3d, %3d, %3d, %3d]\n",
                   (int)round(w_eg[0]), (int)round(w_eg[1]), (int)round(w_eg[2]), (int)round(w_eg[3]), (int)round(w_eg[4]));
            fflush(stdout);
        }
    }

    int64_t tune_elapsed = get_time_ms_signed() - tune_start;

    printf("\n[tuner] Finished in %.2fs\n\n", (double)tune_elapsed / 1000.0);
    printf("static const int PIECE_VALUES_MG[6] = {\n");
    printf("    %d,  // PAWN\n", (int)round(w_mg[0]));
    printf("    %d,  // KNIGHT\n", (int)round(w_mg[1]));
    printf("    %d,  // BISHOP\n", (int)round(w_mg[2]));
    printf("    %d,  // ROOK\n", (int)round(w_mg[3]));
    printf("    %d,  // QUEEN\n", (int)round(w_mg[4]));
    printf("    0    // KING\n");
    printf("};\n\n");

    printf("static const int PIECE_VALUES_EG[6] = {\n");
    printf("    %d,  // PAWN\n", (int)round(w_eg[0]));
    printf("    %d,  // KNIGHT\n", (int)round(w_eg[1]));
    printf("    %d,  // BISHOP\n", (int)round(w_eg[2]));
    printf("    %d,  // ROOK\n", (int)round(w_eg[3]));
    printf("    %d,  // QUEEN\n", (int)round(w_eg[4]));
    printf("    0    // KING\n");
    printf("};\n");

    free(dataset);
}
