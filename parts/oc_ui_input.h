/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The UI's input core: the four buttons and the two encoders, turned into
 * events. Pure C, no Arduino: the shell (firmware/oc_ui.cpp) feeds it one
 * button sample a millisecond and the encoders' detents, and the host test
 * (tests/oc_ui_test.c) feeds it scripted timelines. UI-PLAN.md §3.2. */
#ifndef OC_UI_INPUT_H_
#define OC_UI_INPUT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TOP + BOTTOM chord window (UI-PLAN §8 Q1 default): the second press lands
 * < UI_CHORD_MS after the first, either order. 0 = TOP fires on press. */
#ifndef UI_CHORD_MS
#define UI_CHORD_MS 40u
#endif
#define UI_DEBOUNCE_MS 5u     /* 5 equal 1 ms samples */
#define UI_TAP_MS 400u        /* tap: released before a hold starts (held <= 400 ms) */
#define UI_HOLD_MS UI_TAP_MS /* hold start: where a tap ends (UI-PLAN says 300 ms; see the PR) */
#define UI_LONG_MS 600u       /* long: fires once, while still held */

/* the buttons, as bits: 1 = pressed (the shell inverts the active-low pins) */
#define UI_BTN_L 1u
#define UI_BTN_R 2u
#define UI_BTN_TOP 4u
#define UI_BTN_BOT 8u

enum { UI_SRC_ENC_L, UI_SRC_ENC_R, UI_SRC_PUSH_L, UI_SRC_PUSH_R, UI_SRC_TOP, UI_SRC_BOT, UI_SRC_CHORD_TB };
enum { UI_EV_TURN, UI_EV_DOWN, UI_EV_TAP, UI_EV_LONG, UI_EV_HOLD_START, UI_EV_HOLD_END, UI_EV_SHIFT_TURN, UI_EV_CHORD };

typedef struct {
    uint8_t src, kind;
    int16_t n;        /* TURN / SHIFT_TURN: detents, signed */
    uint16_t dt;      /* TURN: ms since this encoder's previous detent (65535 = long ago) */
    uint8_t fine;     /* TURN: that encoder's push is held (accel off) */
    uint8_t shift;    /* BOTTOM is held (0 on BOT and CHORD_TB events) */
} ui_event_t;

#define UI_EVQ 16u    /* a power of two */

typedef struct {
    uint8_t raw, stable, latched, used, long_done, hold_done;
    uint8_t cnt[4];               /* debounce: equal samples seen, per button */
    uint32_t down_ms[4];          /* when each button went down (stable) */
    uint32_t last_turn_ms[2];
    uint8_t turned[2];            /* an encoder has turned at least once */
    uint8_t top_pending;          /* TOP waits for the chord window */
    uint8_t head, tail;
    uint16_t dropped;
    ui_event_t q[UI_EVQ];
} ui_input_t;

/* The 1 kHz sampler's ring: single producer (the sampler interrupt), single
 * consumer (loop()). Never blocks; when it is full the detents wait in the
 * carry and go out with the next sample, so no detent is lost. */
typedef struct {
    uint32_t ms;
    uint8_t buttons;
    int16_t dl, dr;
} ui_sample_t;

#define UI_RING 64u   /* a power of two, <= 128 */

typedef struct {
    volatile uint8_t head, tail;
    int32_t carry_l, carry_r;
    ui_sample_t buf[UI_RING];
} ui_ring_t;

void ui_ring_put(ui_ring_t *r, uint32_t ms, uint8_t buttons, int32_t dl, int32_t dr);
int ui_ring_get(ui_ring_t *r, ui_sample_t *out);

/* buttons already down here are boot-latched: nothing until released */
void ui_in_init(ui_input_t *s, uint8_t buttons_down, uint32_t now_ms);
/* one 1 ms button sample */
void ui_in_buttons(ui_input_t *s, uint8_t buttons_down, uint32_t now_ms);
/* detents from one encoder (0 = L, 1 = R), after the 21/22 reverse */
void ui_in_turn(ui_input_t *s, uint32_t enc, int32_t detents, uint32_t now_ms);
/* the next event: 1, or 0 when there is none */
int ui_in_pop(ui_input_t *s, ui_event_t *ev);
/* the multiplier for one detent: x8 / x4 / x2 / x1 by the detent interval,
 * only for ranges over 24 steps, never for enums or when fine */
int32_t ui_accel(uint32_t dt_ms, int32_t range, int is_enum, int fine);

#ifdef __cplusplus
}
#endif

#endif
