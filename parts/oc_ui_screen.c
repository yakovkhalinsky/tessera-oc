/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The screen: the grid every screen obeys (UI-PLAN §4, §4.4).
 *
 * Pure C, so the firmware and the host test draw a screen the same way.
 *
 * The grid:
 *
 *   y 0..8    the header: a lit bar, the title knocked out of it
 *   y 9..55   the body: six rows, eight pixels a line; the cursor's row is
 *             an inverted band (lit, with the row's text knocked out)
 *   y 56..63  the footer: the screen's hints in the 3 x 5 face
 *
 * The bands are made by drawing the text and then flipping the band it sits
 * in: the title comes out dark on the lit bar, and the cursor's row comes out
 * as a lit band with the row's text dark in it.
 */

#include <stdint.h>
#include <string.h>

#include "oc_ui_screen.h"

/* the text's left margin, and the right margin of the value column */
#define UI_MARGIN_X 2

/* the gap between a row's label and its value */
#define UI_COL_GAP 6

/* the body: the rows, eight pixels a line */
static int16_t row_y(uint32_t row)
{
    return (int16_t)(UI_BODY_Y + (int32_t)row * UI_ROW_H);
}

/* the header: the title, knocked out of a lit bar */
static void draw_header(oc_gfx_t *g, const char *title)
{
    if (title)
        oc_gfx_text5x7(g, UI_MARGIN_X, UI_HDR_TEXT_Y, title);
    oc_gfx_invert_rect(g, 0, UI_HDR_Y, UI_FB_W, UI_HDR_H);
}

/* a row of the body: the label at the left, the value at the right */
static void draw_row(oc_gfx_t *g, uint32_t row, const oc_row_t *r, int cursor)
{
    int16_t y = row_y(row);
    int32_t right = UI_FB_W - UI_MARGIN_X - 1;
    int32_t room = right - UI_MARGIN_X + 1;

    if (r->value) {
        /* the value's first pixel, so the label knows where to stop */
        int32_t n = (int32_t)strlen(r->value);
        int32_t vx = right - (UI_CH_W * (n - 1) + 4);
        room = vx - UI_COL_GAP - UI_MARGIN_X;
    }

    if (r->label) {
        char lab[32];
        /* the label may not reach the value: it is cut, and a dot marks it */
        oc_gfx_ellipsize(lab, sizeof lab, r->label, room, UI_CH_W);
        oc_gfx_clip(g, UI_MARGIN_X, y, (int16_t)room, UI_CH_H);
        oc_gfx_text5x7(g, UI_MARGIN_X, y, lab);
        oc_gfx_clip_reset(g);
    }
    if (r->value)
        oc_gfx_text_right(g, right, y, r->value);
    if (cursor)
        oc_gfx_invert_rect(g, 0, y, UI_FB_W, UI_CH_H);
}

/* the footer: the screen's hints, in the 3 x 5 face */
static void draw_footer(oc_gfx_t *g, const char *hints)
{
    if (hints)
        oc_gfx_text3x5(g, UI_MARGIN_X, UI_FTR_TEXT_Y, hints);
}

void oc_screen_draw(oc_gfx_t *g, const oc_screen_t *s)
{
    uint32_t i;

    oc_gfx_clear(g);
    draw_header(g, s->title);
    for (i = 0; i < s->nrows && i < UI_ROWS; i++)
        draw_row(g, i, &s->rows[i], (int32_t)i == s->cursor);
    draw_footer(g, s->footer);
}
