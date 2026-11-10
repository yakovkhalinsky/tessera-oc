/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* OVERVIEW's lanes (UI-PLAN.md §4.1, phase 3): the note name, the gate
 * history and the four-lane screen, pure C so the host can check them. */
#ifndef OC_UI_OVERVIEW_H_
#define OC_UI_OVERVIEW_H_

#include <stdint.h>
#include "oc_ui_gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_LANES 4u
#define UI_LANE_H 11                /* four lanes and three 0-pixel gaps fill the body */
#define UI_HIST_BITS 16u

/* a lane, as the shell describes it */
typedef struct {
    char gen[8];                    /* the engine's own name (oc_tx_desc, TX_MODE) */
    char note[6];                   /* "C2", "D#3", or "--" if there is none */
    uint16_t hist;                  /* 16 cells, bit 0 is the newest (the right) */
    uint8_t muted;                  /* 1: show M and an empty strip */
    uint8_t roll;                   /* 1: a ROLL is waiting for the next bar (~) */
} ui_lane_t;

/* the OVERVIEW screen: the header's transport and the four lanes */
typedef struct {
    uint8_t playing;
    uint8_t ext;                    /* 1: the clock follows TR1; the tempo is "---" */
    uint8_t sel;                    /* the selected track, 0..3 */
    int16_t tempo;
    ui_lane_t lane[UI_LANES];
    const char *footer;
} ui_overview_t;

/* the current note, as text: MIDI 36 is C2 (the CV's 0 V). -1 is "--".
 * Below 0 or above 120 is clamped (the CV's span is MIDI 0..120). */
void ui_note_text(int16_t midi, char *out, uint32_t n);

/* one frame of the history: the new bit goes in on the right, the oldest
 * falls off the left. */
uint16_t ui_hist_shift(uint16_t hist, int on);

/* the OVERVIEW screen, drawn on the grid: the header bar, four 11-pixel
 * lanes (the selected one inverted), the 16-cell strip, the footer. */
void ui_overview_draw(oc_gfx_t *g, const ui_overview_t *o);

#ifdef __cplusplus
}
#endif

#endif
