/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* OVERVIEW's lanes (UI-PLAN.md §4.1, phase 3). */
#include <stdio.h>
#include <string.h>

#include "oc_ui_overview.h"

static const char *const NOTE_NAME[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

void ui_note_text(int16_t midi, char *out, uint32_t n)
{
    int32_t v, oct;
    if (!out || !n)
        return;
    out[0] = 0;
    if (midi < 0) {
        (void)snprintf(out, n, "--");
        return;
    }
    v = midi;
    if (v > 120)
        v = 120;
    oct = (v / 12) - 1;             /* MIDI 36 = C2 */
    (void)snprintf(out, n, "%s%d", NOTE_NAME[v % 12], (int)oct);
}

uint16_t ui_hist_shift(uint16_t hist, int on)
{
    return (uint16_t)((hist << 1) | (on ? 1u : 0u));
}

/* the header: the transport, the tempo or "---", the clock, the track,
 * and OVERVIEW, knocked out of the lit bar */
static void ui_ov_header(oc_gfx_t *g, const ui_overview_t *o)
{
    char title[32];
    unsigned tr = (unsigned)((o->sel % UI_LANES) + 1u);
    if (o->ext)
        snprintf(title, sizeof title, "%c --- EXT T%u OVERVIEW", o->playing ? '>' : ' ', tr);
    else
        snprintf(title, sizeof title, "%c %3d INT T%u OVERVIEW", o->playing ? '>' : ' ',
                 (int)o->tempo, tr);
    oc_gfx_text5x7(g, 2, UI_HDR_TEXT_Y, title);
    oc_gfx_invert_rect(g, 0, UI_HDR_Y, UI_FB_W, UI_HDR_H);
}

/* the ROLL flag: a wave five pixels wide and three tall (the 5 x 7 face has
 * no ~, and the 3 x 5 one is too small to read beside the 5 x 7 text) */
static void ui_ov_tilde(oc_gfx_t *g, int16_t x, int16_t y)
{
    oc_gfx_px(g, (int16_t)(x + 1), y, 1);
    oc_gfx_px(g, (int16_t)(x + 2), y, 1);
    oc_gfx_px(g, x, (int16_t)(y + 1), 1);
    oc_gfx_px(g, (int16_t)(x + 3), (int16_t)(y + 1), 1);
    oc_gfx_px(g, (int16_t)(x + 3), (int16_t)(y + 2), 1);
    oc_gfx_px(g, (int16_t)(x + 4), (int16_t)(y + 2), 1);
}

/* one lane: the track number, the generator, the note, M/~ and the strip */
static void ui_ov_lane(oc_gfx_t *g, uint32_t i, const ui_lane_t *l, int selected)
{
    int16_t y = (int16_t)(UI_BODY_Y + (int32_t)i * UI_LANE_H);
    int16_t ty = (int16_t)(y + 2);
    char num[2];
    uint32_t b, x;

    num[0] = (char)('1' + (int)i);
    num[1] = 0;
    oc_gfx_text5x7(g, 2, ty, num);
    oc_gfx_text5x7(g, 12, ty, l->gen[0] ? l->gen : "?");
    oc_gfx_text5x7(g, 52, ty, l->note[0] ? l->note : "--");
    if (l->muted)
        oc_gfx_text5x7(g, 72, ty, "M");
    else if (l->roll)
        ui_ov_tilde(g, 72, (int16_t)(ty + 2));

    /* 16 cells at the right: 2 pixels wide, 1 pixel between, 7 tall.
     * A sounding cell is filled; an empty one is a 2-pixel tick at the foot.
     * A muted lane keeps the cells empty. */
    x = 81u;
    for (b = 0; b < UI_HIST_BITS; b++) {
        int on = !l->muted && (l->hist & (uint16_t)(1u << (UI_HIST_BITS - 1u - b)));
        if (on)
            oc_gfx_fill_rect(g, (int16_t)x, ty, 2, UI_CH_H, 1);
        else
            oc_gfx_fill_rect(g, (int16_t)x, (int16_t)(ty + UI_CH_H - 1), 2, 1, 1);
        x += 3u;
    }
    if (selected)
        oc_gfx_invert_rect(g, 0, y, UI_FB_W, UI_LANE_H);
}

void ui_overview_draw(oc_gfx_t *g, const ui_overview_t *o)
{
    uint32_t i;
    oc_gfx_clear(g);
    if (!o)
        return;
    ui_ov_header(g, o);
    for (i = 0; i < UI_LANES; i++)
        ui_ov_lane(g, i, &o->lane[i], o->sel == i);
    if (o->footer)
        oc_gfx_text3x5(g, 2, UI_FTR_TEXT_Y, o->footer);
}
