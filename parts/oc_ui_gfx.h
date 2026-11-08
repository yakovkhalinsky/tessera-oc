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
 *
 * The context holds the pixels and the clip rectangle they are drawn
 * through, so nothing here is hidden state: two screens can be drawn at once,
 * on the host as on the module.
 */
#ifndef OC_UI_GFX_H_
#define OC_UI_GFX_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the panel: 128 x 64, one bit a pixel, eight pages of sixteen bytes */
#define UI_FB_W 128
#define UI_FB_H 64
#define UI_FB_PAGES (UI_FB_H / 8)
#define UI_FB_SIZE (UI_FB_W * UI_FB_PAGES)

/* the grid every screen obeys (UI-PLAN §4)
 *
 *   y 0..8    the header: a bar, the title knocked out of it
 *   y 9..55   the body: six rows, eight pixels a line
 *   y 56..63  the footer: the screen's hints in the 3 x 5 face
 */
#define UI_HDR_Y 0
#define UI_HDR_H 9               /* the header's band */
#define UI_HDR_TEXT_Y 1          /* the title, inside the band */
#define UI_BODY_Y 9
#define UI_BODY_H 47
#define UI_ROW_H 8               /* a line of the body */
#define UI_ROWS 6                /* the lines the body holds */
#define UI_FTR_Y 56
#define UI_FTR_H 8
#define UI_FTR_TEXT_Y 58         /* the hints, inside the band */
#define UI_COL_W 32              /* a column of the four-lane layout */

/* the faces */
#define UI_CH_W 6                /* the 5 x 7 face: six pixels a glyph */
#define UI_CH_H 7
#define UI_CH3_W 4               /* the 3 x 5 face: four pixels a glyph */
#define UI_CH3_H 5

/* the drawing context: the pixels and the clip they are drawn through */
typedef struct {
    uint8_t *fb;                /* the panel's RAM: UI_FB_SIZE bytes */
    int16_t x0, y0, x1, y1;      /* the clip rectangle, inclusive */
} oc_gfx_t;

/* the context, on the whole panel */
void oc_gfx_init(oc_gfx_t *g, uint8_t *fb);

/* the clip rectangle: the pixels outside it are not touched */
void oc_gfx_clip(oc_gfx_t *g, int16_t x, int16_t y, int16_t w, int16_t h);
void oc_gfx_clip_reset(oc_gfx_t *g);

/* the whole panel, cleared */
void oc_gfx_clear(oc_gfx_t *g);

/* one pixel, and the lines and rectangles built from it */
void oc_gfx_px(oc_gfx_t *g, int16_t x, int16_t y, uint8_t on);
void oc_gfx_hline(oc_gfx_t *g, int16_t x, int16_t y, int16_t w, uint8_t on);
void oc_gfx_vline(oc_gfx_t *g, int16_t x, int16_t y, int16_t h, uint8_t on);
void oc_gfx_fill_rect(oc_gfx_t *g, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t on);

/* the rectangle, flipped: the bar over a title, and the cursor's row, are
 * made by drawing the text and then flipping the band it sits in */
void oc_gfx_invert_rect(oc_gfx_t *g, int16_t x, int16_t y, int16_t w, int16_t h);

/* the text: the width it took, so a caller can lay it out */
int32_t oc_gfx_text5x7(oc_gfx_t *g, int16_t x, int16_t y, const char *s);
int32_t oc_gfx_text3x5(oc_gfx_t *g, int16_t x, int16_t y, const char *s);
int32_t oc_gfx_text5x7_x2(oc_gfx_t *g, int16_t x, int16_t y, const char *s);

/* the width a string takes: the advance, so two strings can be laid out side
 * by side */
int32_t oc_gfx_width5x7(const char *s);
int32_t oc_gfx_width3x5(const char *s);

/* the text, ending at the column `right`: the last glyph's last pixel */
int32_t oc_gfx_text_right(oc_gfx_t *g, int16_t right, int16_t y, const char *s);

/* the text, cut to fit a width and marked with a dot: how many characters
 * were written (the dot is one of them). `adv` is the face's advance. */
uint32_t oc_gfx_ellipsize(char *dst, uint32_t dstsz, const char *src, int32_t maxw, int32_t adv);

#ifdef __cplusplus
}
#endif

#endif
