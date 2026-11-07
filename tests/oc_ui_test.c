/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The UI's input core: the host test (UI-PLAN §3.6).
 *
 * The core is pure C, so all of it can be checked here: the debounce, the
 * tap/long/hold boundaries, the shift that BOTTOM is, the TOP+BOTTOM chord
 * window, the boot latch, the acceleration table, and the sampler's ring.
 *
 * The timelines are in samples (1 ms each). A press held for N samples and
 * then released is held for N ms, measured between the debounced edges.
 *
 *   build/host/oc_ui_test
 */

#include <stdio.h>
#include <stdint.h>

#include "oc_ui_input.h"

static int checks, failures;

static void check(const char *what, int ok)
{
    checks++;
    if (!ok)
        failures++;
    printf("oc_ui: %-72s %s\n", what, ok ? "ok" : "FAIL");
}

/* the state under test: one core, and the clock it runs on */
static ui_input_t s;
static uint32_t T;

/* a fresh core: nothing held, nothing latched, nothing in the queue */
static void fresh(uint8_t latched)
{
    T = 1000;
    ui_in_init(&s, latched, T);
}

/* one sample a millisecond */
static void run(uint8_t buttons, uint32_t ms)
{
    uint32_t i;
    for (i = 0; i < ms; i++) {
        ui_in_buttons(&s, buttons, T);
        T++;
    }
}

/* what an event must be */
typedef struct {
    uint8_t src, kind;
    int16_t n;
    uint16_t dt;
    uint8_t fine, shift;
} exp_t;

#define E_(src_, kind_, n_, dt_, fine_, shift_) \
    {(uint8_t)(src_), (uint8_t)(kind_), (int16_t)(n_), (uint16_t)(dt_), \
     (uint8_t)(fine_), (uint8_t)(shift_)}
#define DN(s_, sh_) E_(UI_SRC_##s_, UI_EV_DOWN, 0, 0, 0, sh_)
#define TP(s_, sh_) E_(UI_SRC_##s_, UI_EV_TAP, 0, 0, 0, sh_)
#define HS(s_) E_(UI_SRC_##s_, UI_EV_HOLD_START, 0, 0, 0, 0)
#define HE(s_) E_(UI_SRC_##s_, UI_EV_HOLD_END, 0, 0, 0, 0)
#define LO(s_) E_(UI_SRC_##s_, UI_EV_LONG, 0, 0, 0, 0)
#define CH() E_(UI_SRC_CHORD_TB, UI_EV_CHORD, 0, 0, 0, 0)
#define TU(s_, n_, dt_, f_) E_(UI_SRC_##s_, UI_EV_TURN, n_, dt_, f_, 0)
#define ST(s_, n_, dt_) E_(UI_SRC_##s_, UI_EV_SHIFT_TURN, n_, dt_, 0, 1)
#define EX(...) ((const exp_t[]){__VA_ARGS__})

#define MAXEV 32
static ui_event_t got[MAXEV];

/* the queue must hold exactly this, in this order */
static int seq(const exp_t *exp, int n)
{
    int ng = 0, i, ok;
    ui_event_t e;
    while (ng < MAXEV && ui_in_pop(&s, &e))
        got[ng++] = e;
    ok = (ng == n);
    for (i = 0; ok && i < n; i++)
        ok = got[i].src == exp[i].src && got[i].kind == exp[i].kind &&
             got[i].n == exp[i].n && got[i].dt == exp[i].dt &&
             got[i].fine == exp[i].fine && got[i].shift == exp[i].shift;
    if (!ok) {
        printf("oc_ui:   expected %d events, got %d\n", n, ng);
        for (i = 0; i < ng; i++)
            printf("oc_ui:   got src=%u kind=%u n=%d dt=%u fine=%u shift=%u\n",
                   (unsigned)got[i].src, (unsigned)got[i].kind, (int)got[i].n,
                   (unsigned)got[i].dt, (unsigned)got[i].fine, (unsigned)got[i].shift);
    }
    return ok;
}

/* the debounce: five equal samples are a press */
static void check_debounce(void)
{
    fresh(0);
    run(UI_BTN_R, 2);
    run(0, 1);
    run(UI_BTN_R, 1);
    run(0, 20);
    check("debounce: a bounce shorter than 5 ms is not a press", seq(NULL, 0));

    fresh(0);
    run(UI_BTN_R, 4);
    run(0, 20);
    check("debounce: 4 ms is not a press", seq(NULL, 0));

    fresh(0);
    run(UI_BTN_R, 5);
    run(0, 20);
    check("debounce: 5 ms is a press, and then a tap",
          seq(EX(DN(PUSH_R, 0), TP(PUSH_R, 0)), 2));
}

/* the tap / hold / long boundaries */
static void check_tap_hold_long(void)
{
    fresh(0);
    run(0, 10);
    run(UI_BTN_R, 399);
    run(0, 10);
    check("tap: released after 399 ms is a tap, not a hold",
          seq(EX(DN(PUSH_R, 0), TP(PUSH_R, 0)), 2));

    fresh(0);
    run(UI_BTN_R, 400);
    run(0, 10);
    check("tap: released at exactly 400 ms is still a tap",
          seq(EX(DN(PUSH_R, 0), TP(PUSH_R, 0)), 2));

    fresh(0);
    run(UI_BTN_R, 401);
    run(0, 10);
    check("hold: released at 401 ms is a hold, and never a tap",
          seq(EX(DN(PUSH_R, 0), HS(PUSH_R), HE(PUSH_R)), 3));

    fresh(0);
    run(UI_BTN_R, 700);
    check("long: fires at 600 ms, while the button is still held",
          seq(EX(DN(PUSH_R, 0), HS(PUSH_R), LO(PUSH_R)), 3));
    run(UI_BTN_R, 2000);
    check("long: fires only once", seq(NULL, 0));
    run(0, 10);
    check("long: the release ends the hold, and is not a tap",
          seq(EX(HE(PUSH_R)), 1));

    fresh(0);
    run(UI_BTN_L, 700);
    check("long: the left push has a long too",
          seq(EX(DN(PUSH_L, 0), HS(PUSH_L), LO(PUSH_L)), 3));
}

/* BOTTOM as the shift: it is consumed, and never restarts */
static void check_shift(void)
{
    fresh(0);
    run(UI_BTN_BOT, 20);
    ui_in_turn(&s, 0, 3, T);
    run(UI_BTN_BOT, 20);
    run(0, 10);
    check("shift: a turn under BOTTOM consumes it (no tap, no restart)",
          seq(EX(DN(BOT, 0), ST(ENC_L, 3, 65535)), 2));

    fresh(0);
    run(UI_BTN_BOT, 20);
    run(UI_BTN_BOT | UI_BTN_L, 20);
    run(UI_BTN_BOT, 10);
    run(0, 10);
    check("shift: L under BOTTOM taps with the shift, and BOTTOM does not tap",
          seq(EX(DN(BOT, 0), DN(PUSH_L, 1), TP(PUSH_L, 1)), 3));

    fresh(0);
    run(UI_BTN_L, 20);
    run(0, 10);
    check("a plain L press taps, and is not a shift",
          seq(EX(DN(PUSH_L, 0), TP(PUSH_L, 0)), 2));

    fresh(0);
    run(UI_BTN_BOT, 50);
    run(0, 10);
    check("a plain BOTTOM press taps (that is the restart)",
          seq(EX(DN(BOT, 0), TP(BOT, 0)), 2));

    fresh(0);
    run(UI_BTN_BOT, 401);
    run(0, 10);
    check("a long BOTTOM is a hold, and restarts nothing",
          seq(EX(DN(BOT, 0), HS(BOT), HE(BOT)), 3));
}

/* TOP + BOTTOM within the window, either way round */
static void check_chord(void)
{
    fresh(0);
    run(UI_BTN_TOP, 39);
    run(UI_BTN_TOP | UI_BTN_BOT, 50);
    run(0, 10);
    check("chord: TOP then BOTTOM within 40 ms is a chord, and nothing else",
          seq(EX(CH()), 1));

    fresh(0);
    run(UI_BTN_TOP, 40);
    run(UI_BTN_TOP | UI_BTN_BOT, 50);
    run(UI_BTN_TOP, 10);
    run(0, 10);
    check("chord: BOTTOM at exactly 40 ms is late: TOP, then BOTTOM",
          seq(EX(DN(TOP, 0), DN(BOT, 0), TP(BOT, 0)), 3));

    fresh(0);
    run(UI_BTN_TOP, 41);
    run(UI_BTN_TOP | UI_BTN_BOT, 50);
    run(UI_BTN_TOP, 10);
    run(0, 10);
    check("chord: BOTTOM at 41 ms is late too",
          seq(EX(DN(TOP, 0), DN(BOT, 0), TP(BOT, 0)), 3));

    fresh(0);
    run(UI_BTN_BOT, 39);
    run(UI_BTN_BOT | UI_BTN_TOP, 50);
    run(0, 10);
    check("chord: BOTTOM then TOP within 40 ms is a chord",
          seq(EX(DN(BOT, 0), CH()), 2));

    fresh(0);
    run(UI_BTN_BOT, 40);
    run(UI_BTN_BOT | UI_BTN_TOP, 50);
    run(0, 10);
    check("chord: TOP at exactly 40 ms is late: TOP plays, and BOTTOM does not tap",
          seq(EX(DN(BOT, 0), DN(TOP, 1)), 2));

    fresh(0);
    run(UI_BTN_BOT, 20);
    run(UI_BTN_BOT | UI_BTN_TOP, 30);
    run(0, 10);
    check("chord: TOP 20 ms after BOTTOM is a chord",
          seq(EX(DN(BOT, 0), CH()), 2));

    fresh(0);
    run(UI_BTN_TOP, 10);
    run(0, 50);
    check("a short TOP plays once", seq(EX(DN(TOP, 0)), 1));

    fresh(0);
    run(UI_BTN_TOP, 1000);
    run(0, 10);
    check("a held TOP plays once, and never taps or holds",
          seq(EX(DN(TOP, 0)), 1));
}

/* a button held through the boot does nothing until it is released */
static void check_boot_latch(void)
{
    fresh(UI_BTN_BOT);
    run(UI_BTN_BOT, 800);
    run(0, 10);
    check("boot-latch: a BOTTOM held at boot does nothing", seq(NULL, 0));
    run(UI_BTN_BOT, 50);
    run(0, 10);
    check("boot-latch: it works once it is released and pressed again",
          seq(EX(DN(BOT, 0), TP(BOT, 0)), 2));

    fresh(UI_BTN_R);
    run(UI_BTN_R, 10);
    ui_in_turn(&s, 1, 1, T);
    run(UI_BTN_R, 10);
    run(0, 10);
    check("boot-latch: a latched push is not held (the turn is a plain turn)",
          seq(EX(TU(ENC_R, 1, 65535, 0)), 1));
}

/* the encoders */
static void check_turn(void)
{
    fresh(0);
    ui_in_turn(&s, 1, 2, T);
    check("turn: the first detent has no interval yet",
          seq(EX(TU(ENC_R, 2, 65535, 0)), 1));
    run(0, 30);
    ui_in_turn(&s, 1, 2, T);
    check("turn: the next one knows the interval",
          seq(EX(TU(ENC_R, 2, 30, 0)), 1));

    fresh(0);
    run(UI_BTN_R, 10);
    ui_in_turn(&s, 1, 1, T);
    run(UI_BTN_R, 10);
    run(0, 10);
    check("turn: a turn under a held R is fine, and the R does not tap",
          seq(EX(DN(PUSH_R, 0), TU(ENC_R, 1, 65535, 1)), 2));

    fresh(0);
    ui_in_turn(&s, 0, -5, T);
    check("turn: a negative count turns the other way",
          seq(EX(TU(ENC_L, -5, 65535, 0)), 1));

    fresh(0);
    ui_in_turn(&s, 2, 1, T);
    ui_in_turn(&s, 1, 0, T);
    check("turn: no encoder, or no detents, is no event", seq(NULL, 0));
}

/* the acceleration table */
static void check_accel(void)
{
    check("accel: 10 ms apart is x8", ui_accel(10, 200, 0, 0) == 8);
    check("accel: 19 ms apart is x8", ui_accel(19, 200, 0, 0) == 8);
    check("accel: 20 ms apart is x4", ui_accel(20, 200, 0, 0) == 4);
    check("accel: 49 ms apart is x4", ui_accel(49, 200, 0, 0) == 4);
    check("accel: 50 ms apart is x2", ui_accel(50, 200, 0, 0) == 2);
    check("accel: 99 ms apart is x2", ui_accel(99, 200, 0, 0) == 2);
    check("accel: 100 ms apart is x1", ui_accel(100, 200, 0, 0) == 1);
    check("accel: a range of 24 steps is not accelerated", ui_accel(10, 24, 0, 0) == 1);
    check("accel: a range of 25 steps is", ui_accel(10, 25, 0, 0) == 8);
    check("accel: an enum is never accelerated", ui_accel(10, 200, 1, 0) == 1);
    check("accel: fine (the push held) is x1", ui_accel(10, 200, 0, 1) == 1);
}

/* the sampler's ring: a fast spin loses no detent */
static ui_ring_t ring1, ring2;

static void check_ring(void)
{
    int i, n = 0, dl = 0, dr = 0;
    ui_sample_t sm;

    for (i = 0; i < 20; i++)
        ui_ring_put(&ring1, 1000u + (uint32_t)i, 0, 1, 0);
    while (ui_ring_get(&ring1, &sm)) {
        n++;
        dl += sm.dl;
    }
    check("ring: 20 fast detents are 20 detents", n == 20 && dl == 20);

    /* the ring fills up: the samples that do not fit wait in the carry */
    n = 0;
    dl = 0;
    dr = 0;
    for (i = 0; i < 100; i++)
        ui_ring_put(&ring2, 2000u + (uint32_t)i, 0, 1, -1);
    while (ui_ring_get(&ring2, &sm)) {
        n++;
        dl += sm.dl;
        dr += sm.dr;
    }
    check("ring: a full ring holds 64 samples", n == 64);
    ui_ring_put(&ring2, 3000u, 0, 0, 0);
    while (ui_ring_get(&ring2, &sm)) {
        n++;
        dl += sm.dl;
        dr += sm.dr;
    }
    check("ring: the detents that did not fit are still there",
          dl == 100 && dr == -100);
}

/* the queue: it is small, and it says what it dropped */
static void check_queue(void)
{
    int i, n = 0;
    ui_event_t e;
    fresh(0);
    for (i = 0; i < 20; i++)
        ui_in_turn(&s, 1, 1, T);
    while (ui_in_pop(&s, &e))
        n++;
    check("queue: 16 events fit, and the other 4 are dropped",
          n == 16 && s.dropped == 4);
}

int main(void)
{
    check_debounce();
    check_tap_hold_long();
    check_shift();
    check_chord();
    check_boot_latch();
    check_turn();
    check_accel();
    check_ring();
    check_queue();

    printf("oc_ui: %d checks, %d failing\n", checks, failures);
    return failures ? 1 : 0;
}
