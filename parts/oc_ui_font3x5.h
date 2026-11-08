/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The 3 x 5 face: the footer's. Original, drawn for this port.
 *
 * Five rows of three pixels a glyph, four pixels a glyph on the screen (the
 * gap is the space between two letters). It is small enough to put a hint
 * along the bottom of the panel and still be read, and it is a third of the
 * 5 x 7 face's height, so the footer band costs five rows instead of seven.
 *
 * The face has what the footers need: the space, the ten digits, A to Z, and
 * + - . : / > ~. A lower-case letter is drawn as its capital, as the 5 x 7
 * face does. Anything else is drawn as a space.
 *
 * Each glyph is five bytes, one a row, and a row's three bits are the row:
 * bit 2 is the left column, bit 1 the middle, bit 0 the right.
 */
#ifndef OC_UI_FONT3X5_H_
#define OC_UI_FONT3X5_H_

#include <stdint.h>

/* the glyphs, in the order of the table below */
enum {
    UI_F3_SPACE,
    UI_F3_0, UI_F3_1, UI_F3_2, UI_F3_3, UI_F3_4,
    UI_F3_5, UI_F3_6, UI_F3_7, UI_F3_8, UI_F3_9,
    UI_F3_A, UI_F3_B, UI_F3_C, UI_F3_D, UI_F3_E,
    UI_F3_F, UI_F3_G, UI_F3_H, UI_F3_I, UI_F3_J,
    UI_F3_K, UI_F3_L, UI_F3_M, UI_F3_N, UI_F3_O,
    UI_F3_P, UI_F3_Q, UI_F3_R, UI_F3_S, UI_F3_T,
    UI_F3_U, UI_F3_V, UI_F3_W, UI_F3_X, UI_F3_Y,
    UI_F3_Z,
    UI_F3_PLUS, UI_F3_MINUS, UI_F3_DOT, UI_F3_COLON,
    UI_F3_SLASH, UI_F3_GT, UI_F3_TILDE,
    UI_F3_COUNT
};

/* the face itself: five bytes a glyph, a row a byte, bit 2 the left column */
static const uint8_t UI_FONT3X5[UI_F3_COUNT][5] = {
    {0, 0, 0, 0, 0},     /* the space */
    {2, 5, 5, 5, 2},     /* 0 */
    {2, 6, 2, 2, 7},     /* 1 */
    {7, 1, 7, 4, 7},     /* 2 */
    {7, 1, 3, 1, 7},     /* 3 */
    {5, 5, 7, 1, 1},     /* 4 */
    {7, 4, 7, 1, 7},     /* 5 */
    {3, 4, 7, 5, 7},     /* 6 */
    {7, 1, 1, 1, 1},     /* 7 */
    {7, 5, 7, 5, 7},     /* 8 */
    {7, 5, 7, 1, 3},     /* 9 */
    {2, 5, 7, 5, 5},     /* A */
    {6, 5, 6, 5, 6},     /* B */
    {3, 4, 4, 4, 3},     /* C */
    {6, 5, 5, 5, 6},     /* D */
    {7, 4, 6, 4, 7},     /* E */
    {7, 4, 6, 4, 4},     /* F */
    {3, 4, 5, 5, 3},     /* G */
    {5, 5, 7, 5, 5},     /* H */
    {7, 2, 2, 2, 7},     /* I */
    {1, 1, 1, 5, 2},     /* J */
    {5, 5, 6, 5, 5},     /* K */
    {4, 4, 4, 4, 7},     /* L */
    {5, 7, 5, 5, 5},     /* M */
    {5, 6, 5, 3, 5},     /* N */
    {7, 5, 5, 5, 7},     /* O */
    {6, 5, 6, 4, 4},     /* P */
    {2, 5, 5, 7, 3},     /* Q */
    {6, 5, 6, 5, 5},     /* R */
    {3, 4, 2, 1, 6},     /* S */
    {7, 2, 2, 2, 2},     /* T */
    {5, 5, 5, 5, 2},     /* U */
    {5, 5, 5, 2, 2},     /* V */
    {5, 5, 5, 7, 5},     /* W */
    {5, 5, 2, 5, 5},     /* X */
    {5, 5, 2, 2, 2},     /* Y */
    {7, 1, 2, 4, 7},     /* Z */
    {0, 2, 7, 2, 0},     /* + */
    {0, 0, 7, 0, 0},     /* - */
    {0, 0, 0, 0, 2},     /* . */
    {0, 2, 0, 2, 0},     /* : */
    {1, 1, 2, 4, 4},     /* / */
    {4, 2, 1, 2, 4},     /* > */
    {0, 3, 6, 0, 0},     /* ~ */
};

/* the glyph a character is drawn with, or the space if it has none */
static inline uint8_t ui_font3x5_index(uint8_t ch)
{
    if (ch >= 'a' && ch <= 'z')
        ch = (uint8_t)(ch - 32);
    if (ch >= '0' && ch <= '9')
        return (uint8_t)(UI_F3_0 + (ch - '0'));
    if (ch >= 'A' && ch <= 'Z')
        return (uint8_t)(UI_F3_A + (ch - 'A'));
    switch (ch) {
    case ' ':  return UI_F3_SPACE;
    case '+':  return UI_F3_PLUS;
    case '-':  return UI_F3_MINUS;
    case '.':  return UI_F3_DOT;
    case ':':  return UI_F3_COLON;
    case '/':  return UI_F3_SLASH;
    case '>':  return UI_F3_GT;
    case '~':  return UI_F3_TILDE;
    default:   return UI_F3_SPACE;
    }
}

#endif
