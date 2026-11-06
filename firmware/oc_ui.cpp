/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The port's screen and its controls.
 *
 * The module has two encoders and four buttons and a 128 x 64 screen; the FM-1
 * had seven encoders, fourteen buttons, a 27-note keyboard and a 240 x 240
 * colour panel. So this is not the FM-1's UI with the knobs taken away, it is
 * a small one of its own, in the o_C's idiom: a list, the left encoder moving
 * down it, the right one turning the value.
 *
 * The list is the engine's own parameters: the descriptors (the label, the
 * range, the format, the names) and the values are the engine's, read through
 * the port's interface. So a generator's knobs are the same knobs the FM-1
 * shows, with the same ranges and the same names.
 *
 * The top and the bottom buttons change the page, the left encoder's button
 * starts and stops, the right one's is the row's own action.
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
uint8_t oc_clock_following(void);
const param_desc_t *oc_tx_desc(uint32_t tx);
const param_desc_t *oc_track_desc(uint32_t id);
void oc_param_format(const param_desc_t *d, int32_t v, char *val, const char **unit);
int32_t oc_tx_get(uint32_t tr, uint32_t tx);
void oc_tx_set(uint32_t tr, uint32_t tx, int32_t v);
int32_t oc_track_param_get(uint32_t tr, uint32_t id);
void oc_track_param_set(uint32_t tr, uint32_t id, int32_t v);
const uint8_t *oc_gen_rows(uint32_t gen, uint8_t *count);
const uint8_t *oc_scl_rows(uint8_t *count);
void oc_set_generator(uint32_t tr, int32_t mode);

/* --- the pages --- */
/* A row is a parameter of the selected track, or one of the port's own. */
enum { ROW_TX, ROW_P, ROW_TEMPO, ROW_TRACK, ROW_CLOCK };
struct UiRow {
    uint8_t kind;
    uint8_t id;      /* the parameter's id */
    uint8_t arg;     /* the track, for a track row */
};


/* HOME: the tempo, the four tracks, the clock */
static const UiRow PAGE_HOME[] = {
    {ROW_TEMPO, 0, 0},
    {ROW_TRACK, 0, 0}, {ROW_TRACK, 0, 1}, {ROW_TRACK, 0, 2}, {ROW_TRACK, 0, 3},
    {ROW_CLOCK, 0, 0},
};

struct UiPage {
    const char *name;
    const UiRow *rows;
    uint8_t count;
    uint8_t gen_page;      /* 1: the rows come from the generator */
};

static const UiPage PAGES[] = {
    {"HOME", PAGE_HOME, sizeof PAGE_HOME / sizeof PAGE_HOME[0], 0},
    {"GEN", 0, 0, 1},
    {"SCL", 0, 0, 2},
};

#define PAGE_COUNT (sizeof PAGES / sizeof PAGES[0])
#define UI_ROWS_MAX 16

static uint8_t ui_page, ui_row;
static UiRow ui_rows[UI_ROWS_MAX];

/* the page's rows: the generator's rows change with the generator, so they
 * are built here */
static uint32_t ui_build_rows(void)
{
    const UiPage *p = &PAGES[ui_page];
    uint32_t n = 0;

    if (p->gen_page) {
        const uint8_t *ids;
        uint8_t count = 0;
        if (p->gen_page == 1u) {
            ids = oc_gen_rows(oc_track_generator(oc_selected_track()), &count);
            for (uint32_t i = 0; i < count && n < UI_ROWS_MAX; i++) {
                ui_rows[n].kind = ROW_TX;
                ui_rows[n].id = ids[i];
                ui_rows[n].arg = 0;
                n++;
            }
        } else {
            ids = oc_scl_rows(&count);
            for (uint32_t i = 0; i < count && n < UI_ROWS_MAX; i++) {
                ui_rows[n].kind = ROW_P;
                ui_rows[n].id = ids[i];
                ui_rows[n].arg = 0;
                n++;
            }
        }
        return n;
    }
    for (uint32_t i = 0; i < p->count && n < UI_ROWS_MAX; i++)
        ui_rows[n++] = p->rows[i];
    return n;
}

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

/* a copy, bounded: the engine's own str_cpy is not the platform's */
static void copy(char *d, const char *s, uint32_t max)
{
    uint32_t i = 0;
    for (; i + 1u < max && s[i]; i++)
        d[i] = s[i];
    d[i] = 0;
}

/* --- the rows' values --- */
static const char *const GEN_NAME[] = {"STEP", "TURING", "EUCLID", "MARKOV", "MORPH", "ATLAS", "BOUNCE", "AWAKE",
                                       "POLY", "QUAD", "CONSTL", "SERIES", "CHORD", "ANSWER"};

/* a row's label and its value, as text */
static void ui_row_text(const UiRow *row, char *label, char *value)
{
    uint32_t tr = oc_selected_track();
    const param_desc_t *d;
    const char *unit;
    int32_t v;

    label[0] = 0;
    value[0] = 0;
    switch (row->kind) {
    case ROW_TEMPO:
        copy(label, "TEMPO", 12);
        snprintf(value, 8, "%d", (int)oc_tempo());
        return;
    case ROW_TRACK: {
        uint8_t gen = oc_track_generator(row->arg);
        snprintf(label, 12, "TRACK %u", (unsigned)(row->arg + 1u));
        copy(value, gen < 14u ? GEN_NAME[gen] : "?", 8);
        return;
    }
    case ROW_CLOCK:
        copy(label, "CLOCK", 12);
        copy(value, oc_clock_following() ? "EXT" : "INT", 8);
        return;
    case ROW_TX:
        d = oc_tx_desc(row->id);
        v = oc_tx_get(tr, row->id);
        copy(label, d ? d->label : "?", 12);
        oc_param_format(d, v, value, &unit);
        return;
    case ROW_P:
        d = oc_track_desc(row->id);
        v = oc_track_param_get(tr, row->id);
        copy(label, d ? d->label : "?", 12);
        oc_param_format(d, v, value, &unit);
        return;
    }
}

/* a row's value, turned */
static void ui_row_turn(const UiRow *row, int32_t steps)
{
    uint32_t tr = oc_selected_track();
    const param_desc_t *d;
    int32_t v;

    switch (row->kind) {
    case ROW_TEMPO:
        oc_set_tempo((int16_t)(oc_tempo() + steps));
        return;
    case ROW_TRACK:
        oc_set_generator(row->arg, (int32_t)oc_track_generator(row->arg) + steps);
        return;
    case ROW_CLOCK:
        return;                         /* the clock's source is the rig's business */
    case ROW_TX:
        d = oc_tx_desc(row->id);
        v = oc_tx_get(tr, row->id);
        oc_tx_set(tr, row->id, v + steps);
        return;
    case ROW_P:
        d = oc_track_desc(row->id);
        v = oc_track_param_get(tr, row->id);
        oc_track_param_set(tr, row->id, v + steps);
        return;
    }
}

/* --- the screen --- */
static void draw_line(int32_t row, const char *s)
{
    oc_display_text(0, row * 8, s);
}

void ui_draw(void)
{
    char line[24], label[12], value[10];
    uint32_t n = ui_build_rows();
    uint32_t sel = oc_selected_track();
    int16_t bpm = oc_tempo();
    uint32_t first = 0;

    /* the list scrolls so the chosen row stays on the screen */
    if (ui_row >= 6u)
        first = ui_row - 5u;
    if (first + 6u > n)
        first = n > 6u ? n - 6u : 0;

    oc_display_clear();
    snprintf(line, sizeof line, "%-4s T%u %3d BPM", PAGES[ui_page].name, (unsigned)(sel + 1u), (int)bpm);
    draw_line(0, line);

    for (uint32_t i = 0; i < 6u && first + i < n; i++) {
        uint32_t r = first + i;
        ui_row_text(&ui_rows[r], label, value);
        snprintf(line, sizeof line, "%c%-11s %s", r == ui_row ? '>' : ' ', label, value);
        draw_line(1 + (int32_t)i, line);
    }
    draw_line(7, "L:ROW R:VALUE T:PAGE");
    oc_display_flush();
}

/* the controls: the encoders and the buttons */
void ui_input(void)
{
    uint32_t n = ui_build_rows();
    int32_t steps = encoder_step(&enc_left);
    while (steps-- && n) {
        ui_row = (uint8_t)((ui_row + 1u) % n);
        /* a track row chooses the track the other pages work on */
        if (ui_rows[ui_row].kind == ROW_TRACK)
            oc_select_track(ui_rows[ui_row].arg);
    }

    steps = encoder_step(&enc_right);
    while (steps-- && n)
        ui_row_turn(&ui_rows[ui_row], steps > 0 ? 1 : -1);
}

void ui_page_next(int32_t dir)
{
    ui_page = (uint8_t)((ui_page + (uint8_t)(dir < 0 ? PAGE_COUNT - 1u : 1u)) % PAGE_COUNT);
    ui_row = 0;
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
