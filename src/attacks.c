#include "attacks.h"
#include "data.h"
#include <string.h>

Bitboard pawn_attacks[2][64];
Bitboard knight_attacks[64];
Bitboard king_attacks[64];

Magic bishop_magics[64];
Magic rook_magics[64];

Bitboard bishop_table[64][512];
Bitboard rook_table[64][4096];

static Bitboard rook_mask(uint8_t sq) {
    Bitboard mask = 0;
    int r = sq / 8;
    int f = sq % 8;
    for (int rr = 1; rr < 7; rr++) {
        if (rr != r) mask |= bb_bit((uint8_t)(rr * 8 + f));
    }
    for (int ff = 1; ff < 7; ff++) {
        if (ff != f) mask |= bb_bit((uint8_t)(r * 8 + ff));
    }
    return mask;
}

static Bitboard bishop_mask(uint8_t sq) {
    Bitboard mask = 0;
    int r = sq / 8;
    int f = sq % 8;
    for (int i = 1; r + i < 7 && f + i < 7; i++) mask |= bb_bit((uint8_t)((r + i) * 8 + (f + i)));
    for (int i = 1; r + i < 7 && f - i > 0; i++) mask |= bb_bit((uint8_t)((r + i) * 8 + (f - i)));
    for (int i = 1; r - i > 0 && f + i < 7; i++) mask |= bb_bit((uint8_t)((r - i) * 8 + (f + i)));
    for (int i = 1; r - i > 0 && f - i > 0; i++) mask |= bb_bit((uint8_t)((r - i) * 8 + (f - i)));
    return mask;
}

static Bitboard slide_attacks(uint8_t sq, Bitboard occ, const int dr[4], const int df[4]) {
    Bitboard atk = 0;
    int r = sq / 8;
    int f = sq % 8;
    for (int i = 0; i < 4; i++) {
        int nr = r + dr[i];
        int nf = f + df[i];
        while (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
            uint8_t s = (uint8_t)(nr * 8 + nf);
            atk |= bb_bit(s);
            if (occ & bb_bit(s)) break;
            nr += dr[i];
            nf += df[i];
        }
    }
    return atk;
}

static Bitboard rook_attacks_ref(uint8_t sq, Bitboard occ) {
    static const int dr[4] = { 1, -1, 0, 0 };
    static const int df[4] = { 0, 0, 1, -1 };
    return slide_attacks(sq, occ, dr, df);
}

static Bitboard bishop_attacks_ref(uint8_t sq, Bitboard occ) {
    static const int dr[4] = { 1, 1, -1, -1 };
    static const int df[4] = { 1, -1, 1, -1 };
    return slide_attacks(sq, occ, dr, df);
}

#if defined(USE_PEXT)

static void init_pext_table(bool is_bishop, uint8_t sq, Bitboard mask, Bitboard *table) {
    Bitboard sub = 0;
    do {
        Bitboard attacks = is_bishop ? bishop_attacks_ref(sq, sub) : rook_attacks_ref(sq, sub);
        size_t idx = (size_t)_pext_u64(sub, mask);
        table[idx] = attacks;
        sub = (sub - mask) & mask;
    } while (sub != 0);
}

#else

static bool try_fill(bool is_bishop, uint8_t sq, Bitboard magic, Bitboard mask, uint8_t shift, Bitboard *table, size_t size) {
    memset(table, 0, size * sizeof(Bitboard));
    Bitboard sub = 0;
    do {
        Bitboard attacks = is_bishop ? bishop_attacks_ref(sq, sub) : rook_attacks_ref(sq, sub);
        size_t idx = (size_t)((sub * magic) >> shift);
        if (table[idx] == 0) {
            table[idx] = attacks;
        } else if (table[idx] != attacks) {
            return false;
        }
        sub = (sub - mask) & mask;
    } while (sub != 0);
    return true;
}

#endif

void attacks_init(void) {
    for (int sq = 0; sq < 64; sq++) {
        Bitboard b = bb_bit((uint8_t)sq);
        pawn_attacks[WHITE][sq] = bb_north_east_of(b) | bb_north_west_of(b);
        pawn_attacks[BLACK][sq] = bb_south_east_of(b) | bb_south_west_of(b);
    }

    for (int sq = 0; sq < 64; sq++) {
        Bitboard b = bb_bit((uint8_t)sq);
        Bitboard north2 = bb_north_of(bb_north_of(b));
        Bitboard south2 = bb_south_of(bb_south_of(b));
        Bitboard east2 = bb_east_of(bb_east_of(b));
        Bitboard west2 = bb_west_of(bb_west_of(b));
        knight_attacks[sq] =
            bb_east_of(north2) | bb_west_of(north2) |
            bb_east_of(south2) | bb_west_of(south2) |
            bb_north_of(east2) | bb_south_of(east2) |
            bb_north_of(west2) | bb_south_of(west2);
    }

    for (int sq = 0; sq < 64; sq++) {
        Bitboard b = bb_bit((uint8_t)sq);
        king_attacks[sq] =
            bb_north_of(b) | bb_south_of(b) |
            bb_east_of(b) | bb_west_of(b) |
            bb_north_east_of(b) | bb_north_west_of(b) |
            bb_south_east_of(b) | bb_south_west_of(b);
    }

    for (int sq = 0; sq < 64; sq++) {
        uint8_t s = (uint8_t)sq;

        Bitboard b_mask = bishop_mask(s);
        uint8_t b_bits = bb_popcount(b_mask);
        uint8_t b_shift = (uint8_t)(64 - b_bits);
        bishop_magics[s].mask = b_mask;
        bishop_magics[s].magic = BISHOP_MAGICS[s];
        bishop_magics[s].shift = b_shift;

#if defined(USE_PEXT)
        init_pext_table(true, s, b_mask, bishop_table[s]);
#else
        bool b_ok = try_fill(true, s, BISHOP_MAGICS[s], b_mask, b_shift, bishop_table[s], (size_t)1 << b_bits);
        assert(b_ok);
        (void)b_ok;
#endif

        Bitboard r_mask = rook_mask(s);
        uint8_t r_bits = bb_popcount(r_mask);
        uint8_t r_shift = (uint8_t)(64 - r_bits);
        rook_magics[s].mask = r_mask;
        rook_magics[s].magic = ROOK_MAGICS[s];
        rook_magics[s].shift = r_shift;

#if defined(USE_PEXT)
        init_pext_table(false, s, r_mask, rook_table[s]);
#else
        bool r_ok = try_fill(false, s, ROOK_MAGICS[s], r_mask, r_shift, rook_table[s], (size_t)1 << r_bits);
        assert(r_ok);
        (void)r_ok;
#endif
    }
}
