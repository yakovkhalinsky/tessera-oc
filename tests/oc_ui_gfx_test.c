/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The drawing kit: the primitives, the two faces and the grid (UI-PLAN §4.4).
 *
 * The kit is pure C and draws into a framebuffer the caller owns, so all of it
 * can be checked here: a pixel is a pixel is a pixel, and the tests are the
 * kit's own picture of what it should draw.
 *
 *   build/host/oc_ui_gfx_test
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "oc_ui_gfx.h"

static int checks, failures;

static void check(const char *what, int ok)
{
    checks++;
    if (!ok)
        failures++;
    printf("oc_gfx: %-72s %s\n", what, ok ? "ok" : "FAIL");
}

/* the panel under test */
static uint8_t fb[UI_FB_SIZE];

/* is the pixel lit? */
static int lit(int32_t x, int32_t y)
{
    return (fb[(y >> 3) * UI_FB_W + x] >> (y & 7)) & 1;
}

/* how many pixels are lit? */
static int lit_count(void)
{
    int n = 0;
    for (uint32_t i = 0; i < UI_FB_SIZE; i++)
        for (int b = 0; b < 8; b++)
            if (fb[i] & (1u << b))
                n++;
    return n;
}

/* --- the primitives --- */

static void check_px(void)
{
    oc_gfx_t g;

    oc_gfx_init(&g, fb);
    oc_gfx_clear(&g);
    check("px: a cleared panel has no lit pixels", lit_count() == 0);

    oc_gfx_px(&g, 0, 0, 1);
    oc_gfx_px(&g, 127, 63, 1);
    check("px: the corners are lit", lit(0, 0) == 1 && lit(127, 63) == 1);
    check("px: only two pixels are lit", lit_count() == 2);

    oc_gfx_px(&g, 0, 0, 0);
    check("px: a pixel can be cleared", lit(0, 0) == 0 && lit_count() == 1);

    /* off the panel: nothing, and nothing broken */
    oc_gfx_px(&g, -1, 0, 1);
    oc_gfx_px(&g, 0, -1, 1);
    oc_gfx_px(&g, UI_FB_W, 0, 1);
    oc_gfx_px(&g, 0, UI_FB_H, 1);
    check("px: off the panel is ignored", lit_count() == 1);
}

static void check_lines(void)
{
    oc_gfx_t g;
    int n;

    oc_gfx_init(&g, fb);
    oc_gfx_clear(&g);
    oc_gfx_hline(&g, 10, 20, 5, 1);
    check("hline: five pixels", lit_count() == 5);
    for (n = 10; n < 15; n++)
        if (!lit(n, 20))
            break;
    check("hline: at the right place", n == 15);

    oc_gfx_clear(&g);
    oc_gfx_vline(&g, 10, 20, 5, 1);
    check("vline: five pixels", lit_count() == 5);

    oc_gfx_clear(&g);
    oc_gfx_fill_rect(&g, 4, 4, 8, 6, 1);
    check("fill_rect: the whole rectangle", lit_count() == 8 * 6);

    /* a rectangle that runs off the panel is cut, not wrapped */
    oc_gfx_clear(&g);
    oc_gfx_fill_rect(&g, 120, 60, 20, 20, 1);
    check("fill_rect: cut at the panel's edge", lit_count() == 8 * 4);
}

static void check_invert(void)
{
    oc_gfx_t g;

    oc_gfx_init(&g, fb);
    oc_gfx_clear(&g);
    oc_gfx_fill_rect(&g, 0, 0, 10, 10, 1);
    oc_gfx_invert_rect(&g, 0, 0, 10, 10);
    check("invert_rect: a lit rectangle becomes dark", lit_count() == 0);

    oc_gfx_invert_rect(&g, 0, 0, 10, 10);
    check("invert_rect: and back again", lit_count() == 100);

    /* the title's band: the bar is lit, the title is knocked out of it */
    oc_gfx_clear(&g);
    oc_gfx_text5x7(&g, 0, 0, "M");
    oc_gfx_invert_rect(&g, 0, 0, UI_FB_W, UI_CH_H);
    check("invert_rect: the band is lit around the text", lit(100, 3) == 1);
    check("invert_rect: the text is knocked out", lit(0, 3) == 0);
}

static void check_clip(void)
{
    oc_gfx_t g;

    oc_gfx_init(&g, fb);
    oc_gfx_clear(&g);
    oc_gfx_clip(&g, 10, 10, 20, 20);
    oc_gfx_fill_rect(&g, 0, 0, UI_FB_W, UI_FB_H, 1);
    check("clip: only the rectangle is drawn", lit_count() == 20 * 20);
    check("clip: the clip's corner is lit", lit(10, 10) == 1);
    check("clip: just outside is dark", lit(9, 10) == 0 && lit(10, 9) == 0);

    oc_gfx_clip_reset(&g);
    oc_gfx_fill_rect(&g, 0, 0, UI_FB_W, UI_FB_H, 1);
    check("clip: reset covers the panel again", lit_count() == UI_FB_SIZE * 8);
}

/* --- the text --- */

/* the 5 x 7 face: 'A' is
 *
 *   .###.
 *   #...#
 *   #...#
 *   #####
 *   #...#
 *   #...#
 *   #...#
 */
static void check_text5x7(void)
{
    oc_gfx_t g;
    int32_t w;

    oc_gfx_init(&g, fb);
    oc_gfx_clear(&g);
    w = oc_gfx_text5x7(&g, 0, 0, "A");
    check("text5x7: the width is the advance", w == UI_CH_W);
    check("text5x7: the A's crossbar is lit", lit(2, 3) == 1);
    check("text5x7: the A's left leg is lit", lit(0, 1) == 1);
    check("text5x7: the A's right leg is lit", lit(4, 1) == 1);
    check("text5x7: the A's apex is lit", lit(2, 0) == 1);
    check("text5x7: below the A is dark", lit(2, 7) == 0);

    /* a lower-case letter is its capital */
    oc_gfx_clear(&g);
    oc_gfx_text5x7(&g, 0, 0, "a");
    check("text5x7: a lower-case letter is its capital", lit(2, 3) == 1 && lit(0, 1) == 1);

    /* the space draws nothing */
    oc_gfx_clear(&g);
    oc_gfx_text5x7(&g, 0, 0, " ");
    check("text5x7: a space draws nothing", lit_count() == 0);

    /* two glyphs are two advances apart */
    oc_gfx_clear(&g);
    w = oc_gfx_text5x7(&g, 0, 0, "AA");
    check("text5x7: two glyphs are two advances", w == 2 * UI_CH_W);
    check("text5x7: the second A is six pixels on", lit(UI_CH_W + 2, 3) == 1);

    /* the width of a string is its advance */
    check("width5x7: the advance", oc_gfx_width5x7("ABC") == 3 * UI_CH_W);
    check("width5x7: the empty string", oc_gfx_width5x7("") == 0);
}

/* the 3 x 5 face: 'A' is
 *
 *   .#.
 *   #.#
 *   ###
 *   #.#
 *   #.#
 */
static void check_text3x5(void)
{
    oc_gfx_t g;
    int32_t w;

    oc_gfx_init(&g, fb);
    oc_gfx_clear(&g);
    w = oc_gfx_text3x5(&g, 0, 0, "A");
    check("text3x5: the width is the advance", w == UI_CH3_W);
    check("text3x5: the A's crossbar is lit", lit(1, 2) == 1);
    check("text3x5: the A's left leg is lit", lit(0, 1) == 1);
    check("text3x5: the A's right leg is lit", lit(2, 1) == 1);
    check("text3x5: the A's apex is lit", lit(1, 0) == 1);
    check("text3x5: below the A is dark", lit(1, 5) == 0);

    /* a character the face does not have is a space */
    oc_gfx_clear(&g);
    oc_gfx_text3x5(&g, 0, 0, "(");
    check("text3x5: a character the face lacks is a space", lit_count() == 0);

    /* a lower-case letter is its capital */
    oc_gfx_clear(&g);
    oc_gfx_text3x5(&g, 0, 0, "a");
    check("text3x5: a lower-case letter is its capital", lit(1, 2) == 1);

    check("width3x5: the advance", oc_gfx_width3x5("ABC") == 3 * UI_CH3_W);
}

static void check_text_x2(void)
{
    oc_gfx_t g;

    oc_gfx_init(&g, fb);
    oc_gfx_clear(&g);
    oc_gfx_text5x7_x2(&g, 0, 0, "A");
    check("text5x7_x2: the A is twice as wide", lit(4, 6) == 1);
    check("text5x7_x2: the A is twice as tall", lit(0, 6) == 1);
    check("text5x7_x2: the A's crossbar is twice as wide", lit(4, 6) == 1 && lit(5, 6) == 1);
    check("text5x7_x2: to the right of the A is dark", lit(10, 6) == 0);
}

static void check_text_right(void)
{
    oc_gfx_t g;

    oc_gfx_init(&g, fb);
    oc_gfx_clear(&g);
    oc_gfx_text_right(&g, 100, 0, "A");
    check("text_right: the A ends at the column", lit(100, 1) == 1);
    check("text_right: and starts five on", lit(96, 1) == 1);
    check("text_right: nothing past the column", lit(101, 1) == 0);

    oc_gfx_clear(&g);
    oc_gfx_text_right(&g, 100, 0, "AB");
    check("text_right: two glyphs end at the column", lit(100, 1) == 1);
    check("text_right: the first is an advance on", lit(100 - UI_CH_W - 4, 1) == 1);
}

static void check_ellipsize(void)
{
    char buf[32];
    uint32_t n;

    /* it fits: it is copied whole */
    n = oc_gfx_ellipsize(buf, sizeof buf, "ABC", 3 * UI_CH_W, UI_CH_W);
    check("ellipsize: a string that fits is copied whole", n == 3 && strcmp(buf, "ABC") == 0);

    /* it does not: the last character is a dot */
    n = oc_gfx_ellipsize(buf, sizeof buf, "ABCDEFGH", 3 * UI_CH_W, UI_CH_W);
    check("ellipsize: a string that does not fit is cut", n == 3);
    check("ellipsize: the cut is marked with a dot", buf[2] == '.' && buf[1] == 'B');
    check("ellipsize: and it is terminated", buf[3] == 0);

    /* the dot takes the place of the last character, so it still fits */
    check("ellipsize: the result fits the width", n * UI_CH_W <= 3 * UI_CH_W);

    /* no room at all: nothing */
    n = oc_gfx_ellipsize(buf, sizeof buf, "ABC", 2, UI_CH_W);
    check("ellipsize: no room means nothing", n == 0 && buf[0] == 0);
}

int main(void)
{
    check_px();
    check_lines();
    check_invert();
    check_clip();
    check_text5x7();
    check_text3x5();
    check_text_x2();
    check_text_right();
    check_ellipsize();

    printf("oc_gfx: %d checks, %d failing\n", checks, failures);
    return failures ? 1 : 0;
}
