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
 */
#ifndef OC_UI_SCREEN_H_
#define OC_UI_SCREEN_H_

#include <stdint.h>

#include "oc_ui_gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* a row of the body: the label at the left, the value at the right */
typedef struct {
    const char *label;               /* the row's label, 0 for none */
    const char *value;               /* the row's value, 0 for none */
} oc_row_t;

/* a screen, as the shell describes it */
typedef struct {
    const char *title;                /* the header's title */
    oc_row_t rows[UI_ROWS];           /* the body's rows */
    uint32_t nrows;                   /* how many of them there are */
    int32_t cursor;                   /* the row that is inverted, or -1 */
    const char *footer;               /* the hints, in the 3 x 5 face */
} oc_screen_t;

/* the screen, drawn into the panel's pixels */
void oc_screen_draw(oc_gfx_t *g, const oc_screen_t *s);

#ifdef __cplusplus
}
#endif

#endif
