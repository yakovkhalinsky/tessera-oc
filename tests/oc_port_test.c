/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The port's own host test: the module's pins, and the encoders' quadrature.
 *
 * Two things the module itself cannot check:
 *
 *   - the pin map. It is the o_C's own (Phazerville PSv2.0.2 OC_gpio.cpp),
 *     and the two tables - the default orientation and the flipped one - are
 *     what the o_C's own firmware does. A table that is the wrong way round
 *     puts the buttons and the encoders on each other's pins, and nothing on
 *     the module says so.
 *   - the encoders' quadrature. A turn the other way (a negative step count)
 *     must end, and must go the other way. The old loop (while (steps--))
 *     never ended for a negative count, and a turn the other way hung the
 *     module.
 *
 *   build/host/oc_port_test
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "oc_port.h"

static int checks, failures;

/* the two tables, as the firmware has them: the default one and the flipped
 * one */
static const struct oc_pinout tabs[2] = {OC_PINOUT_PLAIN, OC_PINOUT_FLIP};

static void check(const char *what, int ok)
{
    checks++;
    if (!ok)
        failures++;
    printf("oc_port: %-72s %s\n", what, ok ? "ok" : "FAIL");
}

/* the pins the module's own outputs own: the OLED's three (6, 7, 8), the
 * DAC's two (9, 10) and the SPI's (11, 12, 13 and 25, 26, 27) */
static int is_module_pin(uint8_t pin)
{
    return !OC_PIN_MOVABLE(pin);
}

/* the default orientation (Phazerville PSv2.0.2 OC_gpio.cpp:128-136) */
static void check_default(void)
{
    const struct oc_pinout p = OC_PINOUT_PLAIN;
    check("default: the top button is on pin 5", p.but_top == 5);
    check("default: the bottom button is on pin 4", p.but_bot == 4);
    check("default: the left encoder is on 22 and 21", p.enc_l1 == 22 && p.enc_l2 == 21);
    check("default: the left encoder's button is on 23", p.but_l == 23);
    check("default: the right encoder is on 16 and 15", p.enc_r1 == 16 && p.enc_r2 == 15);
    check("default: the right encoder's button is on 14", p.but_r == 14);
    check("default: the rig's clock is on the TR1 jack (pin 0)", p.tr1 == 0);
}

/* the flipped one (the same file, :82-94) */
static void check_flipped(void)
{
    const struct oc_pinout p = OC_PINOUT_FLIP;
    check("flipped: the top button is on pin 4", p.but_top == 4);
    check("flipped: the bottom button is on pin 5", p.but_bot == 5);
    check("flipped: the left encoder is on 15 and 16", p.enc_l1 == 15 && p.enc_l2 == 16);
    check("flipped: the left encoder's button is on 14", p.but_l == 14);
    check("flipped: the right encoder is on 21 and 22", p.enc_r1 == 21 && p.enc_r2 == 22);
    check("flipped: the right encoder's button is on 23", p.but_r == 23);
    check("flipped: the rig's clock is on the TR1 jack (pin 3)", p.tr1 == 3);
}

/* the remap may move the inputs and nothing else, and it may only change the
 * roles: the set of pins is the same either way */
static void check_remap(void)
{
    int t, i;
    for (t = 0; t < 2; t++) {
        const struct oc_pinout *p = &tabs[t];
        const uint8_t pins[9] = {p->but_top, p->but_bot, p->but_l, p->but_r,
                                 p->enc_l1, p->enc_l2, p->enc_r1, p->enc_r2, p->tr1};
        int bad = 0;
        for (i = 0; i < 9; i++)
            if (is_module_pin(pins[i]))
                bad++;
        check("the remap touches no output pin (6-13, 25-27)", bad == 0);
    }
    /* the same set of pins, the roles swapped: the top and the bottom button
     * trade places, and so do the two encoders and their buttons */
    check("the flip swaps the top and the bottom button",
          tabs[0].but_top == tabs[1].but_bot && tabs[0].but_bot == tabs[1].but_top);
    check("the flip swaps the encoders and their buttons",
          tabs[0].enc_l1 == tabs[1].enc_r2 && tabs[0].enc_l2 == tabs[1].enc_r1 &&
          tabs[0].but_l == tabs[1].but_r && tabs[0].but_r == tabs[1].but_l);
    check("the two tables are not the same one",
          memcmp(&tabs[0], &tabs[1], sizeof(struct oc_pinout)) != 0);
}

/* one detent: the four transitions of a whole cycle. The state is the
 * encoder's own A wire in the high bit and its B wire in the low one. */
static const uint8_t A_LEADS[4] = {2, 3, 1, 0};
static const uint8_t B_LEADS[4] = {1, 3, 2, 0};

/* the steps of a turn, from the resting state: the detent repeats (the
 * sequence is four transitions long) */
static int32_t turn(uint8_t *state, int8_t *acc, const uint8_t *seq, int n)
{
    int32_t steps = 0;
    int i;
    for (i = 0; i < n; i++)
        steps += oc_enc_step(state, acc, seq[i & 3]);
    return steps;
}

/* the same rotation, said in the pins' own terms: the higher pin of the pair
 * leads, or the lower one does. The encoder's A wire is the pin_a one, so the
 * table's swap of A and B shows up here as the opposite sign. */
static int32_t turn_pins(uint8_t *state, int8_t *acc, uint8_t pin_a, uint8_t pin_b, int hi_leads)
{
    static const uint8_t HI_LEADS[4][2] = {{1, 0}, {1, 1}, {0, 1}, {0, 0}};
    static const uint8_t LO_LEADS[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
    const uint8_t (*wave)[2] = hi_leads ? HI_LEADS : LO_LEADS;
    int a_is_hi = pin_a > pin_b;
    int32_t steps = 0;
    int i;
    for (i = 0; i < 4; i++) {
        uint8_t hi = wave[i][0], lo = wave[i][1];
        uint8_t a = a_is_hi ? hi : lo;
        uint8_t b = a_is_hi ? lo : hi;
        steps += oc_enc_step(state, acc, (uint8_t)((a << 1) | b));
    }
    return steps;
}

/* the encoders: a turn the other way ends, and goes the other way */
static void check_encoders(void)
{
    uint8_t state = 0;
    int8_t acc = 0;
    int32_t plain_left, plain_right, flip_left, flip_right;

    check("a clockwise detent is one step", turn(&state, &acc, A_LEADS, 4) == 1);
    check("an anticlockwise detent is minus one step", turn(&state, &acc, B_LEADS, 4) == -1);

    state = 0;
    acc = 0;
    check("a turn back does not run away (ten detents back)", turn(&state, &acc, B_LEADS, 40) == -10);

    /* both encoders, in both orientations: in each orientation they are wired
     * the same way round, so they count the same way, and the flip swaps A
     * and B, so the mirrored pair counts the other way */
    state = 0;
    acc = 0;
    plain_left = turn_pins(&state, &acc, tabs[0].enc_l1, tabs[0].enc_l2, 1);
    state = 0;
    acc = 0;
    plain_right = turn_pins(&state, &acc, tabs[0].enc_r1, tabs[0].enc_r2, 1);
    state = 0;
    acc = 0;
    flip_left = turn_pins(&state, &acc, tabs[1].enc_l1, tabs[1].enc_l2, 1);
    state = 0;
    acc = 0;
    flip_right = turn_pins(&state, &acc, tabs[1].enc_r1, tabs[1].enc_r2, 1);

    check("the default's two encoders count the same way", plain_left == plain_right);
    check("the flipped one's two encoders count the same way", flip_left == flip_right);
    check("the flip mirrors the encoders", flip_left == -plain_left);
    check("a detent is a step, either way round", plain_left == 1 && flip_left == -1);
}

/* the row the encoder moves: it ends, and it goes the way the encoder turned */
static void check_row(void)
{
    check("the row moves down", oc_enc_row(0, 1, 4) == 1);
    check("the row moves up", oc_enc_row(2, -1, 4) == 1);
    check("the row wraps at the top", oc_enc_row(0, -1, 4) == 3);
    check("the row wraps at the bottom", oc_enc_row(3, 1, 4) == 0);
    check("a turn back ends (twelve steps up)", oc_enc_row(0, -12, 4) == 0);
    check("a turn forward ends (twelve steps down)", oc_enc_row(0, 12, 4) == 0);
    check("no rows, no move", oc_enc_row(0, -3, 0) == 0);
}

int main(void)
{
    check_default();
    check_flipped();
    check_remap();
    check_encoders();
    check_row();

    printf("oc_port: %d checks, %d failing\n", checks, failures);
    return failures ? 1 : 0;
}
