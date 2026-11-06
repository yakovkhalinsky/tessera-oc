/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The port's screen and its controls.
 *
 * The module has two encoders and four buttons and a 128 x 64 screen: the FM-1
 * had seven encoders, fourteen buttons, a 27-note keyboard and a 240 x 240
 * colour panel. So this is not the FM-1's UI with the knobs taken away, it is
 * a small one of its own: what is playing, on which CV, and the few things
 * that have to be reachable while it plays.
 *
 * The left encoder picks the track, the right one the tempo, the top button
 * starts and stops, the bottom one restarts from the top. The rest of the
 * parameter surface comes with the full UI (Phase 4), in the o_C's own idiom.
 *
 * The face is Tessera's 5 x 7 (the engine's mono_font.h). */

#include <Arduino.h>
#include "oc_port.h"

/* the module's encoders (the o_C's own pins) */
#define ENC_L1 22
#define ENC_L2 21
#define ENC_R1 16
#define ENC_R2 15

/* --- the display, the platform's side --- */
void oc_display_init(void);
void oc_display_clear(void);
void oc_display_text(int32_t x, int32_t y, const char *s);
void oc_display_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t on);
void oc_display_flush(void);

/* --- the engine, through the port --- */
uint8_t oc_selected_track(void);
void oc_select_track(uint8_t track);
uint8_t oc_track_generator(uint32_t track);
int16_t oc_tempo(void);
void oc_set_tempo(int16_t bpm);

/* --- the encoders --- */
/* The quadrature, from the state table: the direction of a turn and how far it
 * has gone. A detent is a whole cycle (four transitions), as the o_C's own
 * encoders count it, so a slow turn is one step and a fast one is still one. */
static const int8_t QUAD[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

struct Encoder {
    uint8_t state;
    int8_t acc;
    uint8_t pin_a, pin_b;
};

static Encoder enc_left = {0, 0, ENC_L1, ENC_L2};
static Encoder enc_right = {0, 0, ENC_R1, ENC_R2};

static int32_t encoder_step(Encoder *e)
{
    uint8_t state = (uint8_t)((digitalReadFast(e->pin_a) << 1) | digitalReadFast(e->pin_b));
    int32_t steps = 0;
    if (state != e->state) {
        int8_t delta = QUAD[(e->state << 2) | state];
        e->state = state;
        e->acc = (int8_t)(e->acc + delta);
        while (e->acc >= 4) {
            e->acc = (int8_t)(e->acc - 4);
            steps++;
        }
        while (e->acc <= -4) {
            e->acc = (int8_t)(e->acc + 4);
            steps--;
        }
    }
    return steps;
}

/* --- the screen --- */
static const char *const GEN_NAME[] = {"OFF", "TURING", "EUCLID", "MARKOV", "MORPH", "GRIDS",
                                       "BOUNCE", "AWAKE", "POLY", "QUAD", "CONSTEL", "SERIES",
                                       "CHORD", "ANSWER"};

static void draw_line(int32_t row, const char *s)
{
    oc_display_text(0, row * 8, s);
}

/* the screen: what is playing, on which CV out, and the controls */
void ui_draw(void)
{
    char line[24];
    uint32_t sel = oc_selected_track();
    int16_t bpm = oc_tempo();
    uint32_t i;

    oc_display_clear();

    snprintf(line, sizeof line, "TESSERA-OC %3d BPM", (int)bpm);
    draw_line(0, line);
    snprintf(line, sizeof line, "%s  CLK:%s", oc_playing() ? "PLAY" : "STOP",
             oc_clock_following() ? "EXT" : "INT");
    draw_line(1, line);

    for (i = 0; i < 4u; i++) {
        uint8_t gen = oc_track_generator(i);
        const char *name = gen < (uint8_t)(sizeof GEN_NAME / sizeof GEN_NAME[0]) ? GEN_NAME[gen] : "?";
        int32_t octave = oc_track_octave(i);
        snprintf(line, sizeof line, "%c%u %-7s CV%u %+d", i == sel ? '>' : ' ', i + 1u, name, i + 1u, octave);
        draw_line(3 + (int32_t)i, line);
    }
    draw_line(7, "L:TRACK R:TEMPO TOP:PLAY");
    oc_display_flush();
}

/* the controls: the encoders and the buttons */
void ui_input(void)
{
    int32_t steps = encoder_step(&enc_left);
    while (steps--) {
        uint8_t sel = (uint8_t)((oc_selected_track() + 1u) % 4u);
        oc_select_track(sel);
    }
    steps = encoder_step(&enc_right);
    if (steps)
        oc_set_tempo((int16_t)(oc_tempo() + steps));
}

void ui_init(void)
{
    pinMode(ENC_L1, INPUT_PULLUP);
    pinMode(ENC_L2, INPUT_PULLUP);
    pinMode(ENC_R1, INPUT_PULLUP);
    pinMode(ENC_R2, INPUT_PULLUP);
    enc_left.state = (uint8_t)((digitalReadFast(ENC_L1) << 1) | digitalReadFast(ENC_L2));
    enc_right.state = (uint8_t)((digitalReadFast(ENC_R1) << 1) | digitalReadFast(ENC_R2));
}
