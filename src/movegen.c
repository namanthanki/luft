#include "movegen.h"
#include <string.h>

static const Bitboard FULL_BOARD = ~0ULL;

static const Bitboard CASTLE_WK_EMPTY = (1ULL << SQ_F1) | (1ULL << SQ_G1);
static const Bitboard CASTLE_WQ_EMPTY = (1ULL << SQ_B1) | (1ULL << SQ_C1) | (1ULL << SQ_D1);
static const Bitboard CASTLE_BK_EMPTY = (1ULL << SQ_F8) | (1ULL << SQ_G8);
static const Bitboard CASTLE_BQ_EMPTY = (1ULL << SQ_B8) | (1ULL << SQ_C8) | (1ULL << SQ_D8);

static const Bitboard CASTLE_WK_SAFE  = (1ULL << SQ_E1) | (1ULL << SQ_F1) | (1ULL << SQ_G1);
static const Bitboard CASTLE_WQ_SAFE  = (1ULL << SQ_C1) | (1ULL << SQ_D1) | (1ULL << SQ_E1);
static const Bitboard CASTLE_BK_SAFE  = (1ULL << SQ_E8) | (1ULL << SQ_F8) | (1ULL << SQ_G8);
static const Bitboard CASTLE_BQ_SAFE  = (1ULL << SQ_C8) | (1ULL << SQ_D8) | (1ULL << SQ_E8);

bool is_in_check(const Position *pos, Color side) {
    Bitboard king_bb = pos->pieces[side][KING];
    if (king_bb == 0) return false;
    return is_square_attacked(pos, bb_lsb(king_bb), flip_color(side));
}

MoveGenMasks compute_masks(const Position *pos, Color side) {
    MoveGenMasks result;
    memset(&result, 0, sizeof(result));

    int us = (int)side;
    int them = (int)flip_color(side);

    Bitboard king_bb = pos->pieces[us][KING];
    if (king_bb == 0) return result;

    uint8_t king_sq = bb_lsb(king_bb);
    Bitboard occ = pos->occupancy[2];
    Bitboard occ_without_king = occ ^ king_bb;

    Bitboard checkers = 0;
    Bitboard danger = 0;
    Bitboard pinned = 0;

    checkers |= pawn_attacks[us][king_sq] & pos->pieces[them][PAWN];
    checkers |= knight_attacks[king_sq] & pos->pieces[them][KNIGHT];

    Bitboard opp_pawns = pos->pieces[them][PAWN];
    while (opp_pawns) danger |= pawn_attacks[them][bb_pop_lsb(&opp_pawns)];

    Bitboard opp_knights = pos->pieces[them][KNIGHT];
    while (opp_knights) danger |= knight_attacks[bb_pop_lsb(&opp_knights)];

    Bitboard opp_bq = pos->pieces[them][BISHOP] | pos->pieces[them][QUEEN];
    while (opp_bq) {
        uint8_t sq = bb_pop_lsb(&opp_bq);
        Bitboard atk = bishop_attacks(sq, occ_without_king);
        danger |= atk;
        if (atk & king_bb) checkers |= bb_bit(sq);
    }

    Bitboard opp_rq = pos->pieces[them][ROOK] | pos->pieces[them][QUEEN];
    while (opp_rq) {
        uint8_t sq = bb_pop_lsb(&opp_rq);
        Bitboard atk = rook_attacks(sq, occ_without_king);
        danger |= atk;
        if (atk & king_bb) checkers |= bb_bit(sq);
    }

    Bitboard opp_king = pos->pieces[them][KING];
    if (opp_king) danger |= king_attacks[bb_lsb(opp_king)];

    Bitboard check_mask;
    int n_checkers = bb_popcount(checkers);
    if (n_checkers == 0) {
        check_mask = FULL_BOARD;
    } else if (n_checkers == 1) {
        uint8_t checker_sq = bb_lsb(checkers);
        Piece checker_piece = position_piece_on(pos, (Square)checker_sq);
        Bitboard mask = bb_bit(checker_sq);
        if (checker_piece == BISHOP || checker_piece == ROOK || checker_piece == QUEEN) {
            bool is_diagonal = (bishop_attacks(checker_sq, 0) & king_bb) != 0;
            if (is_diagonal) {
                mask |= bishop_attacks(checker_sq, king_bb) & bishop_attacks(king_sq, bb_bit(checker_sq));
            } else {
                mask |= rook_attacks(checker_sq, king_bb) & rook_attacks(king_sq, bb_bit(checker_sq));
            }
        }
        check_mask = mask;
    } else {
        check_mask = 0;
    }

    Bitboard king_diag = bishop_attacks(king_sq, 0);
    Bitboard enemy_bq = pos->pieces[them][BISHOP] | pos->pieces[them][QUEEN];
    while (enemy_bq) {
        uint8_t sq = bb_pop_lsb(&enemy_bq);
        if (king_diag & bb_bit(sq)) {
            Bitboard ray = bishop_attacks(king_sq, bb_bit(sq)) & bishop_attacks(sq, king_bb);
            if (bb_popcount(ray & occ) == 1) {
                Bitboard blocker = ray & occ & pos->occupancy[us];
                if (blocker) pinned |= blocker;
            }
        }
    }

    Bitboard king_ortho = rook_attacks(king_sq, 0);
    Bitboard enemy_rq = pos->pieces[them][ROOK] | pos->pieces[them][QUEEN];
    while (enemy_rq) {
        uint8_t sq = bb_pop_lsb(&enemy_rq);
        if (king_ortho & bb_bit(sq)) {
            Bitboard ray = rook_attacks(king_sq, bb_bit(sq)) & rook_attacks(sq, king_bb);
            if (bb_popcount(ray & occ) == 1) {
                Bitboard blocker = ray & occ & pos->occupancy[us];
                if (blocker) pinned |= blocker;
            }
        }
    }

    result.checkers = checkers;
    result.pinned = pinned;
    result.check_mask = check_mask;
    result.danger = danger;
    result.king_sq = king_sq;
    return result;
}

static inline Bitboard apply_pin(uint8_t from_sq, Bitboard targets, const MoveGenMasks *masks) {
    if ((masks->pinned & bb_bit(from_sq)) == 0) return targets;
    bool is_diagonal = (bishop_attacks(from_sq, 0) & bb_bit(masks->king_sq)) != 0;
    Bitboard ray = is_diagonal
        ? (bishop_attacks(masks->king_sq, 0) & bishop_attacks(from_sq, 0))
        : (rook_attacks(masks->king_sq, 0) & rook_attacks(from_sq, 0));
    return targets & ray;
}

static inline void add_quiet(MoveList *list, Square from_sq, Piece piece, Bitboard targets) {
    Bitboard t = targets;
    while (t) {
        Square to_sq = (Square)bb_pop_lsb(&t);
        move_list_push(list, move_encode(from_sq, to_sq, piece, PIECE_NONE, NO_PROMO, FLAG_QUIET));
    }
}

static inline void add_captures(MoveList *list, const Position *pos, Square from_sq, Piece piece, Bitboard targets) {
    Bitboard t = targets;
    while (t) {
        Square to_sq = (Square)bb_pop_lsb(&t);
        move_list_push(list, move_encode(from_sq, to_sq, piece, position_piece_on(pos, to_sq), NO_PROMO, FLAG_QUIET));
    }
}

static inline void add_promotions(MoveList *list, Square from_sq, Square to_sq, Piece captured) {
    static const Piece PROMOS[4] = { QUEEN, ROOK, BISHOP, KNIGHT };
    for (int i = 0; i < 4; i++) {
        move_list_push(list, move_encode(from_sq, to_sq, PAWN, captured, PROMOS[i], FLAG_QUIET));
    }
}

static void gen_pawns(const Position *pos, MoveList *list, Color side, const MoveGenMasks *masks, bool captures_only) {
    int us = (int)side;
    int them = (int)flip_color(side);
    Bitboard our_pawns = pos->pieces[us][PAWN];
    Bitboard their_pieces = pos->occupancy[them];
    Bitboard occ = pos->occupancy[2];
    Bitboard empty = ~occ;

    Bitboard promo_rank = (side == WHITE) ? RANK_8 : RANK_1;
    Bitboard start_rank = (side == WHITE) ? RANK_2 : RANK_7;

    Bitboard single = (side == WHITE) ? (bb_north_of(our_pawns) & empty) : (bb_south_of(our_pawns) & empty);
    Bitboard single_legal = single & masks->check_mask;

    Bitboard promo_pushes = single_legal & promo_rank;
    while (promo_pushes) {
        uint8_t to_sq = bb_pop_lsb(&promo_pushes);
        Square to = (Square)to_sq;
        Square from = (Square)(side == WHITE ? to_sq - 8 : to_sq + 8);
        if (apply_pin((uint8_t)from, bb_bit(to_sq), masks)) {
            add_promotions(list, from, to, PIECE_NONE);
        }
    }

    if (!captures_only) {
        Bitboard sp = single_legal & ~promo_rank;
        while (sp) {
            uint8_t to_sq = bb_pop_lsb(&sp);
            Square to = (Square)to_sq;
            Square from = (Square)(side == WHITE ? to_sq - 8 : to_sq + 8);
            if (apply_pin((uint8_t)from, bb_bit(to_sq), masks)) {
                move_list_push(list, move_encode(from, to, PAWN, PIECE_NONE, NO_PROMO, FLAG_QUIET));
            }
        }

        Bitboard double_push = (side == WHITE)
            ? (bb_north_of(bb_north_of(our_pawns & start_rank) & empty) & empty)
            : (bb_south_of(bb_south_of(our_pawns & start_rank) & empty) & empty);

        Bitboard dp = double_push & masks->check_mask;
        while (dp) {
            uint8_t to_sq = bb_pop_lsb(&dp);
            Square to = (Square)to_sq;
            Square from = (Square)(side == WHITE ? to_sq - 16 : to_sq + 16);
            if (apply_pin((uint8_t)from, bb_bit(to_sq), masks)) {
                move_list_push(list, move_encode(from, to, PAWN, PIECE_NONE, NO_PROMO, FLAG_DOUBLE_PUSH));
            }
        }
    }

    Bitboard pawns = our_pawns;
    while (pawns) {
        uint8_t from_sq = bb_pop_lsb(&pawns);
        Square from = (Square)from_sq;
        Bitboard cap_targets = pawn_attacks[us][from_sq] & their_pieces & masks->check_mask;
        cap_targets = apply_pin(from_sq, cap_targets, masks);

        Bitboard pc = cap_targets & promo_rank;
        while (pc) {
            Square to = (Square)bb_pop_lsb(&pc);
            add_promotions(list, from, to, position_piece_on(pos, to));
        }

        Bitboard nc = cap_targets & ~promo_rank;
        while (nc) {
            Square to = (Square)bb_pop_lsb(&nc);
            move_list_push(list, move_encode(from, to, PAWN, position_piece_on(pos, to), NO_PROMO, FLAG_QUIET));
        }

        if (pos->ep != SQ_NONE) {
            uint8_t ep_sq = (uint8_t)pos->ep;
            uint8_t cap_sq = (side == WHITE) ? ep_sq - 8 : ep_sq + 8;

            if (pawn_attacks[us][from_sq] & bb_bit(ep_sq)) {
                Bitboard occ_after = (occ ^ bb_bit(from_sq) ^ bb_bit(cap_sq)) | bb_bit(ep_sq);
                if ((rook_attacks(masks->king_sq, occ_after) & (pos->pieces[them][ROOK] | pos->pieces[them][QUEEN])) == 0 &&
                    (bishop_attacks(masks->king_sq, occ_after) & (pos->pieces[them][BISHOP] | pos->pieces[them][QUEEN])) == 0)
                {
                    if (masks->check_mask == FULL_BOARD ||
                        (masks->check_mask & bb_bit(ep_sq)) != 0 ||
                        (masks->check_mask & bb_bit(cap_sq)) != 0)
                    {
                        move_list_push(list, move_encode(from, (Square)ep_sq, PAWN, PAWN, NO_PROMO, FLAG_EN_PASSANT));
                    }
                }
            }
        }
    }
}

static void gen_piece(const Position *pos, MoveList *list, Color side, Piece piece, const MoveGenMasks *masks, bool captures_only) {
    int us = (int)side;
    int them = (int)flip_color(side);
    Bitboard occ = pos->occupancy[2];
    Bitboard pieces = pos->pieces[us][piece];

    while (pieces) {
        uint8_t from_sq = bb_pop_lsb(&pieces);
        Square from = (Square)from_sq;

        Bitboard atk = 0;
        switch (piece) {
            case KNIGHT: atk = knight_attacks[from_sq]; break;
            case BISHOP: atk = bishop_attacks(from_sq, occ); break;
            case ROOK:   atk = rook_attacks(from_sq, occ); break;
            case QUEEN:  atk = queen_attacks(from_sq, occ); break;
            default: break;
        }
        atk &= masks->check_mask;
        atk = apply_pin(from_sq, atk, masks);

        add_captures(list, pos, from, piece, atk & pos->occupancy[them]);
        if (!captures_only) {
            add_quiet(list, from, piece, atk & ~occ);
        }
    }
}

static void gen_king(const Position *pos, MoveList *list, Color side, const MoveGenMasks *masks, bool captures_only) {
    int us = (int)side;
    int them = (int)flip_color(side);
    if (pos->pieces[us][KING] == 0) return;

    Square from = (Square)masks->king_sq;
    Bitboard own = pos->occupancy[us];
    Bitboard occ = pos->occupancy[2];

    Bitboard king_moves = king_attacks[masks->king_sq] & ~masks->danger;
    add_captures(list, pos, from, KING, king_moves & pos->occupancy[them] & ~own);
    if (captures_only) return;
    add_quiet(list, from, KING, king_moves & ~occ);

    if (masks->checkers != 0) return;

    if (side == WHITE) {
        if ((pos->castling & CASTLE_WK) &&
            !(occ & CASTLE_WK_EMPTY) &&
            !(masks->danger & CASTLE_WK_SAFE))
        {
            move_list_push(list, move_encode(SQ_E1, SQ_G1, KING, PIECE_NONE, NO_PROMO, FLAG_CASTLE_K));
        }
        if ((pos->castling & CASTLE_WQ) &&
            !(occ & CASTLE_WQ_EMPTY) &&
            !(masks->danger & CASTLE_WQ_SAFE))
        {
            move_list_push(list, move_encode(SQ_E1, SQ_C1, KING, PIECE_NONE, NO_PROMO, FLAG_CASTLE_Q));
        }
    } else {
        if ((pos->castling & CASTLE_BK) &&
            !(occ & CASTLE_BK_EMPTY) &&
            !(masks->danger & CASTLE_BK_SAFE))
        {
            move_list_push(list, move_encode(SQ_E8, SQ_G8, KING, PIECE_NONE, NO_PROMO, FLAG_CASTLE_K));
        }
        if ((pos->castling & CASTLE_BQ) &&
            !(occ & CASTLE_BQ_EMPTY) &&
            !(masks->danger & CASTLE_BQ_SAFE))
        {
            move_list_push(list, move_encode(SQ_E8, SQ_C8, KING, PIECE_NONE, NO_PROMO, FLAG_CASTLE_Q));
        }
    }
}

MoveGenMasks generate_moves(const Position *pos, MoveList *list) {
    move_list_reset(list);
    MoveGenMasks masks = compute_masks(pos, pos->side);
    gen_pawns(pos, list, pos->side, &masks, false);
    gen_piece(pos, list, pos->side, KNIGHT, &masks, false);
    gen_piece(pos, list, pos->side, BISHOP, &masks, false);
    gen_piece(pos, list, pos->side, ROOK, &masks, false);
    gen_piece(pos, list, pos->side, QUEEN, &masks, false);
    gen_king(pos, list, pos->side, &masks, false);
    return masks;
}

void generate_captures(const Position *pos, MoveList *list) {
    move_list_reset(list);
    MoveGenMasks masks = compute_masks(pos, pos->side);
    gen_pawns(pos, list, pos->side, &masks, true);
    gen_piece(pos, list, pos->side, KNIGHT, &masks, true);
    gen_piece(pos, list, pos->side, BISHOP, &masks, true);
    gen_piece(pos, list, pos->side, ROOK, &masks, true);
    gen_piece(pos, list, pos->side, QUEEN, &masks, true);
    gen_king(pos, list, pos->side, &masks, true);
}
