#include "position.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

void position_init(Position *pos) {
    memset(pos, 0, sizeof(Position));
    pos->ep = SQ_NONE;
    pos->fullmove = 1;
    for (int i = 0; i < 64; i++) {
        pos->mailbox[i] = PIECE_NONE;
    }
}

bool position_set_fen(Position *pos, const char *fen) {
    position_init(pos);

    if (!fen) return false;

    // 1. Piece placement
    const char *p = fen;
    uint8_t sq = 56; // start at a8
    while (*p && *p != ' ') {
        char ch = *p++;
        if (ch == '/') {
            if (sq < 16) return false;
            sq -= 16;
        } else if (ch >= '1' && ch <= '8') {
            sq += (uint8_t)(ch - '0');
        } else {
            Color color = isupper((unsigned char)ch) ? WHITE : BLACK;
            Piece piece;
            switch (tolower((unsigned char)ch)) {
                case 'p': piece = PAWN; break;
                case 'n': piece = KNIGHT; break;
                case 'b': piece = BISHOP; break;
                case 'r': piece = ROOK; break;
                case 'q': piece = QUEEN; break;
                case 'k': piece = KING; break;
                default: return false;
            }
            if (sq >= 64) return false;
            position_place_piece(pos, color, piece, (Square)sq);
            sq++;
        }
    }

    while (*p == ' ') p++;
    if (!*p) return false;

    // 2. Side to move
    if (*p == 'w') {
        pos->side = WHITE;
    } else if (*p == 'b') {
        pos->side = BLACK;
        pos->hash ^= zobrist_side_key;
    } else {
        return false;
    }
    p++;

    while (*p == ' ') p++;
    if (!*p) return false;

    // 3. Castling rights
    pos->castling = CASTLE_NONE;
    if (*p == '-') {
        p++;
    } else {
        while (*p && *p != ' ') {
            switch (*p) {
                case 'K': pos->castling |= CASTLE_WK; break;
                case 'Q': pos->castling |= CASTLE_WQ; break;
                case 'k': pos->castling |= CASTLE_BK; break;
                case 'q': pos->castling |= CASTLE_BQ; break;
                default: break;
            }
            p++;
        }
    }
    pos->hash ^= zobrist_castling_keys[pos->castling];

    while (*p == ' ') p++;
    if (!*p) return true; // FEN can end here

    // 4. En passant target
    if (*p != '-') {
        if (p[0] >= 'a' && p[0] <= 'h' && p[1] >= '1' && p[1] <= '8') {
            uint8_t file = (uint8_t)(p[0] - 'a');
            uint8_t rank = (uint8_t)(p[1] - '1');
            pos->ep = (Square)(rank * 8 + file);
            pos->hash ^= zobrist_ep_keys[file];
            p += 2;
        } else {
            p++;
        }
    } else {
        p++;
    }

    while (*p == ' ') p++;
    if (!*p) return true;

    // 5. Halfmove clock
    pos->halfmove = (uint16_t)atoi(p);
    while (*p && *p != ' ') p++;

    while (*p == ' ') p++;
    if (!*p) return true;

    // 6. Fullmove number
    pos->fullmove = (uint16_t)atoi(p);

    return true;
}

void position_display(const Position *pos) {
    printf("\n");
    for (int rank = 7; rank >= 0; rank--) {
        printf("  %d  ", rank + 1);
        for (int file = 0; file < 8; file++) {
            Square sq = (Square)(rank * 8 + file);
            Piece piece = position_piece_on(pos, sq);
            if (piece == PIECE_NONE) {
                printf(". ");
            } else {
                Color c = position_color_on(pos, sq);
                printf("%c ", piece_char(piece, c == WHITE));
            }
        }
        printf("\n");
    }
    printf("\n     a b c d e f g h\n\n");
    printf("  side:     %s\n", pos->side == WHITE ? "white" : "black");
    printf("  castling: %s%s%s%s\n",
           (pos->castling & CASTLE_WK) ? "K" : "",
           (pos->castling & CASTLE_WQ) ? "Q" : "",
           (pos->castling & CASTLE_BK) ? "k" : "",
           (pos->castling & CASTLE_BQ) ? "q" : "");
    if (pos->ep != SQ_NONE) {
        printf("  ep:       %c%c\n", 'a' + sq_file(pos->ep), '1' + sq_rank(pos->ep));
    } else {
        printf("  ep:       -\n");
    }
    printf("  halfmove: %d\n", pos->halfmove);
    printf("  fullmove: %d\n", pos->fullmove);
    printf("  hash:     %016llx\n\n", (unsigned long long)pos->hash);
}

void position_to_fen(const Position *pos, char *buf) {
    char *p = buf;
    for (int rank = 7; rank >= 0; rank--) {
        int empty_count = 0;
        for (int file = 0; file < 8; file++) {
            Square sq = (Square)(rank * 8 + file);
            Piece piece = position_piece_on(pos, sq);
            if (piece == PIECE_NONE) {
                empty_count++;
            } else {
                if (empty_count > 0) {
                    *p++ = (char)('0' + empty_count);
                    empty_count = 0;
                }
                Color c = position_color_on(pos, sq);
                *p++ = piece_char(piece, c == WHITE);
            }
        }
        if (empty_count > 0) {
            *p++ = (char)('0' + empty_count);
        }
        if (rank > 0) {
            *p++ = '/';
        }
    }

    *p++ = ' ';
    *p++ = (pos->side == WHITE) ? 'w' : 'b';
    *p++ = ' ';

    if (pos->castling == CASTLE_NONE) {
        *p++ = '-';
    } else {
        if (pos->castling & CASTLE_WK) *p++ = 'K';
        if (pos->castling & CASTLE_WQ) *p++ = 'Q';
        if (pos->castling & CASTLE_BK) *p++ = 'k';
        if (pos->castling & CASTLE_BQ) *p++ = 'q';
    }

    *p++ = ' ';
    if (pos->ep != SQ_NONE) {
        *p++ = (char)('a' + sq_file(pos->ep));
        *p++ = (char)('1' + sq_rank(pos->ep));
    } else {
        *p++ = '-';
    }

    p += sprintf(p, " %d %d", pos->halfmove, pos->fullmove);
    *p = '\0';
}
