/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The UI's input core (oc_ui_input.h). UI-PLAN.md §3.2. */
#include "oc_ui_input.h"

/* bit i of the masks is button i: L, R, TOP, BOT */
static const uint8_t BTN_SRC[4] = {UI_SRC_PUSH_L, UI_SRC_PUSH_R, UI_SRC_TOP, UI_SRC_BOT};

/* and back: PUSH_L, PUSH_R, TOP as bits (a button's events carry its press-time shift) */
static const uint8_t SRC_BTN[3] = {UI_BTN_L, UI_BTN_R, UI_BTN_TOP};

/* a press that counts: stable, not latched */
static int live(const ui_input_t *s, uint8_t bit)
{
    return (s->stable & bit) && !(s->latched & bit);
}

static void push(ui_input_t *s, uint8_t src, uint8_t kind, int32_t n, uint32_t dt, uint8_t fine)
{
    ui_event_t *e;
    if ((uint8_t)(s->head - s->tail) >= UI_EVQ) {
        s->dropped++;
        return;
    }
    e = &s->q[s->head & (UI_EVQ - 1u)];
    e->src = src;
    e->kind = kind;
    e->n = (int16_t)n;
    e->dt = (uint16_t)(dt > 65535u ? 65535u : dt);
    e->fine = fine;
    if (src == UI_SRC_PUSH_L || src == UI_SRC_PUSH_R || src == UI_SRC_TOP)
        e->shift = (uint8_t)((s->shift_at_press & SRC_BTN[src - UI_SRC_PUSH_L]) ? 1u : 0u);
    else
        e->shift = (uint8_t)(src != UI_SRC_BOT && src != UI_SRC_CHORD_TB && live(s, UI_BTN_BOT));
    s->head++;
}

void ui_in_init(ui_input_t *s, uint8_t buttons_down, uint32_t now_ms)
{
    uint32_t i;
    uint8_t *p = (uint8_t *)s;
    for (i = 0; i < sizeof *s; i++)
        p[i] = 0;
    s->raw = s->stable = s->latched = (uint8_t)(buttons_down & 15u);
    for (i = 0; i < 4u; i++)
        s->down_ms[i] = now_ms;
}

static void on_press(ui_input_t *s, uint32_t i, uint32_t now)
{
    uint8_t bit = (uint8_t)(1u << i);
    s->down_ms[i] = now;
    s->used &= (uint8_t)~bit;
    s->long_done &= (uint8_t)~bit;
    s->hold_done &= (uint8_t)~bit;
    if (bit != UI_BTN_BOT && live(s, UI_BTN_BOT))
        s->shift_at_press |= bit;     /* latch the shift at the press */
    else
        s->shift_at_press &= (uint8_t)~bit;
    /* another control pressed while BOTTOM is down: BOTTOM was a shift */
    if (bit != UI_BTN_BOT && live(s, UI_BTN_BOT))
        s->used |= UI_BTN_BOT;
    if (bit == UI_BTN_TOP) {
#if UI_CHORD_MS > 0
        if (live(s, UI_BTN_BOT) && now - s->down_ms[3] < UI_CHORD_MS) {
            push(s, UI_SRC_CHORD_TB, UI_EV_CHORD, 0, 0, 0);
            s->used |= UI_BTN_TOP | UI_BTN_BOT;
        } else {
            s->top_pending = 1;
        }
#else
        push(s, UI_SRC_TOP, UI_EV_DOWN, 0, 0, 0);
#endif
        return;
    }
    if (bit == UI_BTN_BOT && s->top_pending) {
        s->top_pending = 0;
        push(s, UI_SRC_CHORD_TB, UI_EV_CHORD, 0, 0, 0);
        s->used |= UI_BTN_TOP | UI_BTN_BOT;
        return;
    }
    push(s, BTN_SRC[i], UI_EV_DOWN, 0, 0, 0);
}

static void on_release(ui_input_t *s, uint32_t i)
{
    uint8_t bit = (uint8_t)(1u << i);
    if (s->latched & bit) {           /* held through boot: released, silently */
        s->latched &= (uint8_t)~bit;
        return;
    }
    if (bit == UI_BTN_TOP && s->top_pending) {   /* a short TOP still plays/stops */
        s->top_pending = 0;
        push(s, UI_SRC_TOP, UI_EV_DOWN, 0, 0, 0);
    }
    if (s->hold_done & bit)
        push(s, BTN_SRC[i], UI_EV_HOLD_END, 0, 0, 0);
    /* HOLD_START fires at UI_HOLD_MS while held, so a press is a TAP or a HOLD */
    if (!((s->used | s->long_done | s->hold_done) & bit) && bit != UI_BTN_TOP)
        push(s, BTN_SRC[i], UI_EV_TAP, 0, 0, 0);
}

void ui_in_buttons(ui_input_t *s, uint8_t buttons_down, uint32_t now)
{
    uint32_t i;
    buttons_down &= 15u;
#if UI_CHORD_MS > 0
    /* the window ends before this sample's edges: a BOTTOM at exactly
     * UI_CHORD_MS is late, the same as a TOP at UI_CHORD_MS after BOTTOM */
    if (s->top_pending && now - s->down_ms[2] >= UI_CHORD_MS) {
        s->top_pending = 0;
        push(s, UI_SRC_TOP, UI_EV_DOWN, 0, 0, 0);
    }
#endif
    for (i = 0; i < 4u; i++) {
        uint8_t bit = (uint8_t)(1u << i);
        if ((buttons_down & bit) != (s->raw & bit)) {
            s->raw ^= bit;            /* a new level: start counting again */
            s->cnt[i] = 1;
        } else if (s->cnt[i] < UI_DEBOUNCE_MS) {
            s->cnt[i]++;
        }
        if (s->cnt[i] >= UI_DEBOUNCE_MS && (s->raw & bit) != (s->stable & bit)) {
            s->stable ^= bit;
            if (s->stable & bit)
                on_press(s, i, now);
            else
                on_release(s, i);
        }
    }
    for (i = 0; i < 4u; i++) {
        uint8_t bit = (uint8_t)(1u << i);
        uint32_t held = now - s->down_ms[i];
        if (!live(s, bit) || bit == UI_BTN_TOP)
            continue;
        if (!(s->hold_done & bit) && !(s->used & bit) && held >= UI_HOLD_MS) {
            s->hold_done |= bit;
            push(s, BTN_SRC[i], UI_EV_HOLD_START, 0, 0, 0);
        }
        if (!(s->long_done & bit) && !(s->used & bit) && held >= UI_LONG_MS) {
            s->long_done |= bit;
            push(s, BTN_SRC[i], UI_EV_LONG, 0, 0, 0);
        }
    }
}

void ui_in_turn(ui_input_t *s, uint32_t enc, int32_t detents, uint32_t now)
{
    uint8_t push_bit = enc ? UI_BTN_R : UI_BTN_L;
    uint32_t dt;
    if (enc > 1u || detents == 0)
        return;
    dt = s->turned[enc] ? now - s->last_turn_ms[enc] : 65535u;
    s->turned[enc] = 1;
    s->last_turn_ms[enc] = now;
    if (detents > 32767) detents = 32767;
    if (detents < -32767) detents = -32767;
    if (live(s, push_bit))
        s->used |= push_bit;          /* turned while pushed: no tap, no long */
    if (live(s, UI_BTN_BOT)) {
        s->used |= UI_BTN_BOT;        /* BOTTOM was a shift: no restart */
        push(s, (uint8_t)(enc ? UI_SRC_ENC_R : UI_SRC_ENC_L), UI_EV_SHIFT_TURN, detents, dt, 0);
        return;
    }
    push(s, (uint8_t)(enc ? UI_SRC_ENC_R : UI_SRC_ENC_L), UI_EV_TURN, detents, dt,
         (uint8_t)(live(s, push_bit) ? 1u : 0u));
}

int ui_in_pop(ui_input_t *s, ui_event_t *ev)
{
    if (s->head == s->tail)
        return 0;
    *ev = s->q[s->tail & (UI_EVQ - 1u)];
    s->tail++;
    return 1;
}

int32_t ui_accel(uint32_t dt_ms, int32_t range, int is_enum, int fine)
{
    if (fine || is_enum || range <= 24)
        return 1;
    if (dt_ms < 20u) return 8;
    if (dt_ms < 50u) return 4;
    if (dt_ms < 100u) return 2;
    return 1;
}

/* the ring: the stores land before the index that publishes them */
#define UI_BARRIER() __asm__ volatile("" ::: "memory")

static int16_t take16(int32_t *carry)
{
    int32_t v = *carry;
    if (v > 32767) v = 32767;
    if (v < -32767) v = -32767;
    *carry -= v;
    return (int16_t)v;
}

void ui_ring_put(ui_ring_t *r, uint32_t ms, uint8_t buttons, int32_t dl, int32_t dr)
{
    uint8_t h = r->head;
    ui_sample_t *e;
    r->carry_l += dl;
    r->carry_r += dr;
    if ((uint8_t)(h - r->tail) >= UI_RING)
        return;                       /* full: the detents wait in the carry */
    e = &r->buf[h & (UI_RING - 1u)];
    e->ms = ms;
    e->buttons = buttons;
    e->dl = take16(&r->carry_l);
    e->dr = take16(&r->carry_r);
    UI_BARRIER();
    r->head = (uint8_t)(h + 1u);
}

int ui_ring_get(ui_ring_t *r, ui_sample_t *out)
{
    uint8_t t = r->tail;
    if (t == r->head)
        return 0;
    *out = r->buf[t & (UI_RING - 1u)];
    UI_BARRIER();
    r->tail = (uint8_t)(t + 1u);
    return 1;
}
