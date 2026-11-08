/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The drawing kit: the panel's pixels, and nothing else (UI-PLAN §4.4).
 *
 * Pure C, no Arduino and no engine: everything draws into a framebuffer the
 * caller owns, so the whole kit can be checked on the host.
 *
 * The pixels are the panel's own layout - 128 x 64, one bit a pixel, eight
 * pages of sixteen bytes, the bit of the pixel (x, y) being 1 << (y & 7) of
 * the byte at page y / 8 - so the display's own buffer is handed over as it
 * is and nothing is converted.
 *
 * The kit draws with the two faces the module has: Tessera's own 5 x 7 (the
 * engine's mono_font.h, the FM-1's Mono look) and the port's own 3 x 5 (the
 * footer's, oc_ui_font3x5.h). A lower-case letter is drawn as its capital,
 * as the FM-1's Mono look does.
 */

#include <stdint.h>
#include <string.h>

#include "oc_ui_gfx.h"
#include "oc_ui_font3x5.h"
#include "mono_font.h"     /* Tessera's 5 x 7 face (the engine's) */

/* --- the context --- */

void oc_gfx_init(oc_gfx_t *g, uint8_t *fb)
{
    g->fb = fb;
    oc_gfx_clip_reset(g);
}

void oc_gfx_clip(oc_gfx_t *g, int16_t x, int16_t y, int16_t w, int16_t h)
{
    g->x0 = x;
    g->y0 = y;
    g->x1 = (int16_t)(x + w - 1);
    g->y1 = (int16_t)(y + h - 1);
}

void oc_gfx_clip_reset(oc_gfx_t *g)
{
    g->x0 = 0;
    g->y0 = 0;
    g->x1 = UI_FB_W - 1;
    g->y1 = UI_FB_H - 1;
}

/* --- the primitives --- */

/* is the pixel on the panel, and inside the clip? */
static int on_panel(const oc_gfx_t *g, int32_t x, int32_t y)
{
    if (x < 0 || x >= UI_FB_W || y < 0 || y >= UI_FB_H)
        return 0;
    return x >= g->x0 && x <= g->x1 && y >= g->y0 && y <= g->y1;
}

static uint8_t *pixel_at(const oc_gfx_t *g, int32_t x, int32_t y)
{
    return &g->fb[(y >> 3) * UI_FB_W + x];
}

void oc_gfx_clear(oc_gfx_t *g)
{
    memset(g->fb, 0, UI_FB_SIZE);
}

void oc_gfx_px(oc_gfx_t *g, int16_t x, int16_t y, uint8_t on)
{
    uint8_t *p;
    if (!on_panel(g, x, y))
        return;
    p = pixel_at(g, x, y);
    if (on)
        *p |= (uint8_t)(1u << (y & 7));
    else
        *p &= (uint8_t)~(1u << (y & 7));
}

void oc_gfx_hline(oc_gfx_t *g, int16_t x, int16_t y, int16_t w, uint8_t on)
{
    for (int16_t i = 0; i < w; i++)
        oc_gfx_px(g, (int16_t)(x + i), y, on);
}

void oc_gfx_vline(oc_gfx_t *g, int16_t x, int16_t y, int16_t h, uint8_t on)
{
    for (int16_t i = 0; i < h; i++)
        oc_gfx_px(g, x, (int16_t)(y + i), on);
}

void oc_gfx_fill_rect(oc_gfx_t *g, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t on)
{
    for (int16_t i = 0; i < h; i++)
        oc_gfx_hline(g, x, (int16_t)(y + i), w, on);
}

/* the rectangle, flipped: the bar over a title, and the cursor's row, are
 * made by drawing the text and then flipping the band it sits in */
void oc_gfx_invert_rect(oc_gfx_t *g, int16_t x, int16_t y, int16_t w, int16_t h)
{
    for (int16_t j = 0; j < h; j++)
        for (int16_t i = 0; i < w; i++) {
            int16_t px = (int16_t)(x + i), py = (int16_t)(y + j);
            if (!on_panel(g, px, py))
                continue;
            *pixel_at(g, px, py) ^= (uint8_t)(1u << (py & 7));
        }
}

/* --- the text --- */

/* the width a string takes: the advance, so two strings can be laid out side
 * by side */
int32_t oc_gfx_width5x7(const char *s)
{
    return (int32_t)strlen(s) * UI_CH_W;
}

/* the width a string takes in the 3 x 5 face */
int32_t oc_gfx_width3x5(const char *s)
{
    return (int32_t)strlen(s) * UI_CH3_W;
}

int32_t oc_gfx_text5x7(oc_gfx_t *g, int16_t x, int16_t y, const char *s)
{
    int16_t x0 = x;
    for (; *s; s++) {
        uint8_t ch = (uint8_t)*s;
        int16_t col;
        if (ch >= 'a' && ch <= 'z')
            ch = (uint8_t)(ch - 32);
        if (ch < 32u || ch > 122u) {     /* the face's own range */
            x = (int16_t)(x + UI_CH_W);
            continue;
        }
        for (int16_t r = 0; r < UI_CH_H; r++)
            for (col = 0; col < 5; col++)
                if (MO_FONT[ch - 32u][r] & (16u >> col))
                    oc_gfx_px(g, (int16_t)(x + col), (int16_t)(y + r), 1);
        x = (int16_t)(x + UI_CH_W);
    }
    return (int32_t)(x - x0);
}

/* the same, twice the size: the big numbers of the CV readouts */
int32_t oc_gfx_text5x7_x2(oc_gfx_t *g, int16_t x, int16_t y, const char *s)
{
    int16_t x0 = x;
    for (; *s; s++) {
        uint8_t ch = (uint8_t)*s;
        int16_t col;
        if (ch >= 'a' && ch <= 'z')
            ch = (uint8_t)(ch - 32);
        if (ch < 32u || ch > 122u) {
            x = (int16_t)(x + UI_CH_W * 2);
            continue;
        }
        for (int16_t r = 0; r < UI_CH_H; r++)
            for (col = 0; col < 5; col++)
                if (MO_FONT[ch - 32u][r] & (16u >> col)) {
                    oc_gfx_px(g, (int16_t)(x + col * 2), (int16_t)(y + r * 2), 1);
                    oc_gfx_px(g, (int16_t)(x + col * 2 + 1), (int16_t)(y + r * 2), 1);
                    oc_gfx_px(g, (int16_t)(x + col * 2), (int16_t)(y + r * 2 + 1), 1);
                    oc_gfx_px(g, (int16_t)(x + col * 2 + 1), (int16_t)(y + r * 2 + 1), 1);
                }
        x = (int16_t)(x + UI_CH_W * 2);
    }
    return (int32_t)(x - x0);
}

int32_t oc_gfx_text3x5(oc_gfx_t *g, int16_t x, int16_t y, const char *s)
{
    int16_t x0 = x;
    for (; *s; s++) {
        const uint8_t *glyph = UI_FONT3X5[ui_font3x5_index((uint8_t)*s)];
        for (int16_t r = 0; r < UI_CH3_H; r++)
            for (int16_t col = 0; col < 3; col++)
                if (glyph[r] & (4u >> col))
                    oc_gfx_px(g, (int16_t)(x + col), (int16_t)(y + r), 1);
        x = (int16_t)(x + UI_CH3_W);
    }
    return (int32_t)(x - x0);
}

/* the text, ending at the column `right`: the last glyph's last pixel */
int32_t oc_gfx_text_right(oc_gfx_t *g, int16_t right, int16_t y, const char *s)
{
    int32_t n = (int32_t)strlen(s);
    int16_t x;
    if (n <= 0)
        return 0;
    x = (int16_t)(right - (UI_CH_W * (n - 1) + 4));
    oc_gfx_text5x7(g, x, y, s);
    return oc_gfx_width5x7(s);
}

/* the text, cut to fit a width and marked with a dot: how many characters
 * were written (the dot is one of them)
 *
 * A character takes `adv` pixels, so `maxw / adv` characters fit. If the
 * text is longer, the dot takes the place of the last character, and the
 * result still fits the width. */
uint32_t oc_gfx_ellipsize(char *dst, uint32_t dstsz, const char *src, int32_t maxw, int32_t adv)
{
    uint32_t n = (uint32_t)strlen(src);
    uint32_t fit, i;
    if (dstsz == 0)
        return 0;
    if (adv <= 0 || maxw < adv) {
        dst[0] = 0;
        return 0;
    }
    fit = (uint32_t)(maxw / adv);
    if (n <= fit) {                       /* it fits */
        for (i = 0; i < n && i + 1u < dstsz; i++)
            dst[i] = src[i];
        dst[i] = 0;
        return i;
    }
    /* it does not: the dot takes the place of the last character */
    for (i = 0; i + 2u < dstsz && i + 1u < fit; i++)
        dst[i] = src[i];
    dst[i++] = '.';
    dst[i] = 0;
    return i;
}
