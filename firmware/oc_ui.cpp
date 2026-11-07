/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The port's screen and its controls: the UI's shell (UI-PLAN.md §3.2).
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
 * Phase 1c, the shell: a 1 kHz sampler (an IntervalTimer of its own, on the
 * engine tick's interrupt) reads the six controls into a ring; the main loop drains
 * it into the input core (parts/oc_ui_input.c), whose events go to the screen
 * router (parts/oc_ui_nav.c), and the router's actions are applied here, to
 * the engine, through the port. The screens: OVERVIEW (the old HOME), TRACK
 * with the GEN, SCL, CV and MOD tabs, PERFORM (a stub until phase 5),
 * PROJECT (the slots; the drive is read only), INFO (the timings) and the
 * screen menu.
 *
 * The face is Tessera's 5 x 7 (the engine's mono_font.h). */

#include <Arduino.h>
#include "oc_port.h"
#include "oc_version.h"
#include "../parts/oc_ui_input.h"
#include "../parts/oc_ui_nav.h"

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
const param_desc_t *oc_engine_desc(uint32_t id);
void oc_param_format(const param_desc_t *d, int32_t v, char *val, const char **unit);
int32_t oc_tx_get(uint32_t tr, uint32_t tx);
void oc_tx_set(uint32_t tr, uint32_t tx, int32_t v);
int32_t oc_track_param_get(uint32_t tr, uint32_t id);
void oc_track_param_set(uint32_t tr, uint32_t id, int32_t v);
const uint8_t *oc_gen_rows(uint32_t gen, uint8_t *count);
const uint8_t *oc_scl_rows(uint8_t *count);
void oc_set_generator(uint32_t tr, int32_t mode);
int oc_save(uint32_t slot);
int oc_load(uint32_t slot);
int oc_slot_used(uint32_t slot);
uint32_t oc_diag_blk_max_us(void);

/* --- the screens' rows --- */
/* A row is a parameter of the selected track, or one of the port's own. */
enum { ROW_TX, ROW_P, ROW_E, ROW_TEMPO, ROW_TRACK, ROW_CLOCK, ROW_SLOT };
struct UiRow {
    uint8_t kind;
    uint8_t id;      /* the parameter's id */
    uint8_t arg;     /* the track, for a track row */
};


/* OVERVIEW (HOME): the tempo, the four tracks, the clock */
static const UiRow PAGE_HOME[] = {
    {ROW_TEMPO, 0, 0},
    {ROW_TRACK, 0, 0}, {ROW_TRACK, 0, 1}, {ROW_TRACK, 0, 2}, {ROW_TRACK, 0, 3},
    {ROW_CLOCK, 0, 0},
};

/* PROJECT: the four slots the project is kept in */
static const UiRow PAGE_PROJ[] = {
    {ROW_SLOT, 0, 0}, {ROW_SLOT, 0, 1}, {ROW_SLOT, 0, 2}, {ROW_SLOT, 0, 3},
};

#define UI_ROWS_MAX 16

static UiRow ui_rows[UI_ROWS_MAX];

static uint32_t ui_ids_rows(uint8_t kind, const uint8_t *ids, uint8_t count)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < count && n < UI_ROWS_MAX; i++) {
        ui_rows[n].kind = kind;
        ui_rows[n].id = ids[i];
        ui_rows[n].arg = 0;
        n++;
    }
    return n;
}

/* a screen's rows: TRACK's come from the tab, and GEN's from the generator,
 * so they are built each time */
static uint32_t ui_build_rows(uint32_t screen, uint32_t tab)
{
    const UiRow *rows = 0;
    uint32_t count = 0, n = 0;
    const uint8_t *ids;
    uint8_t c = 0;

    if (screen == UI_SCR_TRACK) {
        if (tab == UI_TAB_GEN) {
            ids = oc_gen_rows(oc_track_generator(oc_selected_track()), &c);
            return ui_ids_rows(ROW_TX, ids, c);
        }
        if (tab == UI_TAB_SCL) {
            ids = oc_scl_rows(&c);
            return ui_ids_rows(ROW_P, ids, c);
        }
        if (tab == UI_TAB_CV) {
            ids = oc_cv_rows(&c);
            return ui_ids_rows(ROW_E, ids, c);
        }
        ids = oc_mod_rows(&c);
        return ui_ids_rows(ROW_P, ids, c);
    }
    if (screen == UI_SCR_OVERVIEW) {
        rows = PAGE_HOME;
        count = sizeof PAGE_HOME / sizeof PAGE_HOME[0];
    } else if (screen == UI_SCR_PROJECT) {
        rows = PAGE_PROJ;
        count = sizeof PAGE_PROJ / sizeof PAGE_PROJ[0];
    }
    for (uint32_t i = 0; i < count && n < UI_ROWS_MAX; i++)
        ui_rows[n++] = rows[i];
    return n;
}

/* --- the encoders --- */
/* The quadrature is the port's own (oc_port.h): the state table, the
 * direction of a turn and how far it has gone. The sampler runs it. */
struct Encoder {
    uint8_t state;
    int8_t acc;
    uint8_t pin_a, pin_b;
};

static Encoder enc_left = {0, 0, 0, 0};
static Encoder enc_right = {0, 0, 0, 0};

static int32_t encoder_step(Encoder *e)
{
    uint8_t state = (uint8_t)((digitalReadFast(e->pin_a) << 1) | digitalReadFast(e->pin_b));
    int32_t steps = oc_enc_step(&e->state, &e->acc, state);
    /* physical 21/22 shaft; cf. Phazerville ENCODER_CONFIG_L_REVERSED / OC_ui.cpp:70-71.
     * Applied once, here, before the input core sees the detents. */
    if (OC_ENC_REV_PINS_21_22 && oc_enc_on_21_22(e->pin_a, e->pin_b))
        steps = -steps;
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
    case ROW_SLOT:
        snprintf(label, 12, "SLOT %u", (unsigned)(row->arg + 1u));
        copy(value, oc_slot_used(row->arg) ? "USED" : "-", 8);
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
    case ROW_E:
        d = oc_engine_desc(row->id);
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
    case ROW_E:
        d = row->kind == ROW_E ? oc_engine_desc(row->id) : oc_track_desc(row->id);
        v = oc_track_param_get(tr, row->id);
        oc_track_param_set(tr, row->id, v + steps);
        return;
    }
}

/* --- the sampler --- */
/* The six controls, read at 1 kHz by a timer of their own: the four buttons as a mask
 * (UI_BTN_*, pressed = LOW) and the two encoders' detents, into a ring the
 * main loop drains. The sampler starts only in normal mode (ui_start, from
 * setup, next to the tick): the safe mode and the crash pager read the
 * buttons themselves, as they always have. */
#define UI_SAMPLE_US 1000u
/* On the Teensy 4 every IntervalTimer shares the PIT's one interrupt, and it
 * runs at the highest priority any running channel asked for: the tick's 80.
 * 160 only says the sampler never raises it. The channels' callbacks run one
 * after the other in that one handler, the tick's channel (begun first) checked
 * first, so the sampler never preempts the tick; its body is about 1 us. The
 * priority is set before begin(), so the channel never starts at the default. */
#define UI_SAMPLE_PRIORITY 160

static IntervalTimer ui_timer;
static ui_ring_t ui_ring;
static ui_input_t ui_in;
static ui_nav_t ui_nav;

static uint8_t ui_buttons_now(void)
{
    uint8_t b = 0;
    if (!digitalReadFast(oc_pins.but_l)) b |= UI_BTN_L;
    if (!digitalReadFast(oc_pins.but_r)) b |= UI_BTN_R;
    if (!digitalReadFast(oc_pins.but_top)) b |= UI_BTN_TOP;
    if (!digitalReadFast(oc_pins.but_bot)) b |= UI_BTN_BOT;
    return b;
}

static void ui_sample(void)
{
    int32_t dl = encoder_step(&enc_left);
    int32_t dr = encoder_step(&enc_right);
    ui_ring_put(&ui_ring, millis(), ui_buttons_now(), dl, dr);
}

/* --- the timings, for INFO (plain RAM: they start again at each boot) --- */
static uint32_t ui_draw_us, ui_draw_max_us, ui_flush_us, ui_flush_max_us;
static uint32_t ui_loop_max_us, ui_loop_last;

/* --- the screen --- */
static void draw_line(int32_t row, const char *s)
{
    oc_display_text(0, row * 8, s);
}

static const char *const TAB_NAME[UI_TAB_COUNT] = {"GEN", "SCL", "CV", "MOD"};
static const char *const MENU_NAME[UI_MENU_ITEMS] = {"OVERVIEW", "PERFORM", "TRACK", "PROJECT", "INFO"};

/* the footers: the controls each screen answers, in 21 characters or less
 * (L/R: the encoders, LP/RP: their pushes, LL: a long LP, RB: the RIGHT
 * button, BOTTOM in the code; LEFT and RIGHT are the panel's names) */
static const char *ui_footer(uint32_t screen)
{
    switch (screen) {
    case UI_SCR_OVERVIEW: return "L:ROW R:SET LL:MENU";
    case UI_SCR_TRACK:    return "L:ROW R:SET RB+RP:TAB";
    case UI_SCR_PERFORM:  return "LEFT+RIGHT:BACK";
    case UI_SCR_PROJECT:  return "RP:LOAD LP:BACK";
    case UI_SCR_MENU:     return "L:MOVE RP:OPEN";
    default:              return "LP:BACK";
    }
}

/* the list: a row per line, the cursor's row marked, scrolled so it stays on */
static void draw_rows(uint32_t n, uint32_t cur)
{
    char line[24], label[12], value[10];
    uint32_t first = 0;
    if (cur >= 6u)
        first = cur - 5u;
    if (first + 6u > n)
        first = n > 6u ? n - 6u : 0;
    for (uint32_t i = 0; i < 6u && first + i < n; i++) {
        uint32_t r = first + i;
        ui_row_text(&ui_rows[r], label, value);
        snprintf(line, sizeof line, "%c%-11s %s", r == cur ? '>' : ' ', label, value);
        draw_line(1 + (int32_t)i, line);
    }
}

static void draw_info(void)
{
    char line[24];
    snprintf(line, sizeof line, "v%s STORE %s", OC_VERSION_STRING, oc_storage_ok() ? "RO" : "OFF");
    draw_line(1, line);
    snprintf(line, sizeof line, "BLK MAX %lu US", (unsigned long)oc_diag_blk_max_us());
    draw_line(2, line);
    snprintf(line, sizeof line, "DRAW %lu/%lu US", (unsigned long)ui_draw_us, (unsigned long)ui_draw_max_us);
    draw_line(3, line);
    snprintf(line, sizeof line, "FLUSH %lu/%lu US", (unsigned long)ui_flush_us, (unsigned long)ui_flush_max_us);
    draw_line(4, line);
    snprintf(line, sizeof line, "LOOP MAX %lu US", (unsigned long)ui_loop_max_us);
    draw_line(5, line);
    /* the events the input core's queue dropped, and the samples the
     * sampler's ring had no room for (their detents wait in the carry) */
    snprintf(line, sizeof line, "DROP EV %u RING %lu", (unsigned)ui_in.dropped, (unsigned long)ui_ring.dropped);
    draw_line(6, line);
}

static void draw_perform(void)
{
    char line[24];
    for (uint32_t t = 0; t < 4u; t++) {
        uint8_t gen = oc_track_generator(t);
        snprintf(line, sizeof line, "T%u %s", (unsigned)(t + 1u), gen < 14u ? GEN_NAME[gen] : "?");
        draw_line(1 + (int32_t)t, line);
    }
    draw_line(6, "MORE IN PHASE 5");
}

static void draw_menu(void)
{
    char line[24];
    uint32_t cur = ui_nav.cursor[UI_SCR_MENU] % UI_MENU_ITEMS;
    for (uint32_t i = 0; i < UI_MENU_ITEMS; i++) {
        snprintf(line, sizeof line, "%c%s", i == cur ? '>' : ' ', MENU_NAME[i]);
        draw_line(1 + (int32_t)i, line);
    }
}

/* the rows of every screen, told to the router (its cursors are clamped) */
static uint32_t ui_rows_for(uint32_t screen)
{
    uint32_t n = 0;
    if (screen == UI_SCR_OVERVIEW || screen == UI_SCR_TRACK || screen == UI_SCR_PROJECT)
        n = ui_build_rows(screen, ui_nav.tab);
    if (screen != UI_SCR_MENU)
        ui_nav_rows(&ui_nav, screen, n);
    return n;
}

void ui_draw(void)
{
    char line[24];
    uint32_t screen = ui_nav_screen(&ui_nav);
    uint32_t t0 = micros(), t1;
    uint32_t n = ui_rows_for(screen);
    uint32_t cur = ui_nav.cursor[screen];

    oc_display_clear();
    if (screen == UI_SCR_TRACK)
        snprintf(line, sizeof line, "TRACK %s T%u %3d BPM", TAB_NAME[ui_nav.tab % UI_TAB_COUNT],
                 (unsigned)(oc_selected_track() + 1u), (int)oc_tempo());
    else if (screen == UI_SCR_OVERVIEW)
        snprintf(line, sizeof line, "HOME T%u %3d BPM", (unsigned)(oc_selected_track() + 1u), (int)oc_tempo());
    else
        snprintf(line, sizeof line, "%s", screen == UI_SCR_PERFORM ? "PERFORM"
                                          : screen == UI_SCR_PROJECT ? "PROJECT"
                                          : screen == UI_SCR_INFO ? "INFO" : "MENU");
    draw_line(0, line);

    if (screen == UI_SCR_INFO)
        draw_info();
    else if (screen == UI_SCR_PERFORM)
        draw_perform();
    else if (screen == UI_SCR_MENU)
        draw_menu();
    else if (screen == UI_SCR_PROJECT && !oc_storage_ok()) {
        /* the project screen without a drive: no load, and the screen says so */
        snprintf(line, sizeof line, "SAVE OFF v%s", OC_VERSION);
        draw_line(1, line);
    } else
        draw_rows(n, cur);
    draw_line(7, ui_footer(screen));

    t1 = micros();
    oc_display_flush();
    ui_draw_us = t1 - t0;
    ui_flush_us = micros() - t1;
    if (ui_draw_us > ui_draw_max_us) ui_draw_max_us = ui_draw_us;
    if (ui_flush_us > ui_flush_max_us) ui_flush_max_us = ui_flush_us;
}

/* --- the actions --- */
/* a value turned: the turn's detents times the acceleration (a fine turn,
 * an enum or a short range is one step a detent) */
static int32_t ui_row_range(const UiRow *row, int *is_enum)
{
    const param_desc_t *d = 0;
    *is_enum = 0;
    switch (row->kind) {
    case ROW_TEMPO: return 200;
    case ROW_TRACK: *is_enum = 1; return 14;
    case ROW_TX: d = oc_tx_desc(row->id); break;
    case ROW_P: d = oc_track_desc(row->id); break;
    case ROW_E: d = oc_engine_desc(row->id); break;
    default: return 0;
    }
    if (!d)
        return 0;
    *is_enum = d->names != 0;
    return (int32_t)d->max - (int32_t)d->min;
}

static void ui_row_default(const UiRow *row)
{
    uint32_t tr = oc_selected_track();
    const param_desc_t *d;
    switch (row->kind) {
    case ROW_TX:
        d = oc_tx_desc(row->id);
        if (d) oc_tx_set(tr, row->id, d->def);
        return;
    case ROW_P:
    case ROW_E:
        d = row->kind == ROW_E ? oc_engine_desc(row->id) : oc_track_desc(row->id);
        if (d) oc_track_param_set(tr, row->id, d->def);
        return;
    default:
        return;
    }
}

static void ui_apply(const ui_action_t *a, uint32_t screen)
{
    uint32_t n = ui_rows_for(screen);
    uint32_t cur = ui_nav.cursor[screen];
    const UiRow *row = cur < n ? &ui_rows[cur] : 0;
    int is_enum;
    int32_t range;

    switch (a->kind) {
    case UI_ACT_PLAY_STOP:
        oc_transport(oc_playing() ? 2u : 1u);
        return;
    case UI_ACT_RESTART:
        oc_transport(3u);
        return;
    case UI_ACT_SELECT_TRACK:
        oc_select_track((uint8_t)(((int32_t)oc_selected_track() + a->n % 4 + 4) % 4));
        return;
    case UI_ACT_TEMPO:
        if (!oc_clock_following())     /* an external clock sets the tempo itself */
            oc_set_tempo((int16_t)(oc_tempo() + a->n * ui_accel(a->dt, 200, 0, a->fine)));
        return;
    case UI_ACT_VALUE:
        if (!row || screen == UI_SCR_PROJECT)
            return;
        range = ui_row_range(row, &is_enum);
        if (range > 0)
            ui_row_turn(row, a->n * ui_accel(a->dt, range, is_enum, a->fine));
        return;
    case UI_ACT_ENTER:
        if (screen == UI_SCR_OVERVIEW && row && row->kind == ROW_TRACK)
            ui_nav_open(&ui_nav, UI_SCR_TRACK);
        else if (screen == UI_SCR_PROJECT && row && row->kind == ROW_SLOT && oc_storage_ok())
            oc_load(row->arg);         /* the drive is read only: load, no save */
        return;
    case UI_ACT_CONTEXT:
        if (screen == UI_SCR_TRACK && row)
            ui_row_default(row);
        return;
    default:
        return;                        /* SECONDARY, MACRO: phase 5 */
    }
}

/* the main loop's side: the ring into the input core, its events into the
 * router, the router's actions onto the engine */
void ui_service(void)
{
    ui_sample_t s;
    ui_event_t ev;
    uint32_t now = micros();

    if (ui_loop_last && millis() > 2000u && now - ui_loop_last > ui_loop_max_us)
        ui_loop_max_us = now - ui_loop_last;
    ui_loop_last = now;

    while (ui_ring_get(&ui_ring, &s)) {
        ui_in_buttons(&ui_in, s.buttons, s.ms);
        ui_in_turn(&ui_in, 0, s.dl, s.ms);
        ui_in_turn(&ui_in, 1, s.dr, s.ms);
        while (ui_in_pop(&ui_in, &ev)) {
            uint32_t screen = ui_nav_screen(&ui_nav);
            ui_action_t a;
            ui_rows_for(screen);
            a = ui_nav_event(&ui_nav, &ev);
            ui_apply(&a, screen);
            /* OVERVIEW's cursor on a track row is the track the other screens edit */
            screen = ui_nav_screen(&ui_nav);
            if (screen == UI_SCR_OVERVIEW && ev.src == UI_SRC_ENC_L) {
                uint32_t n = ui_rows_for(screen), cur = ui_nav.cursor[screen];
                if (cur < n && ui_rows[cur].kind == ROW_TRACK)
                    oc_select_track(ui_rows[cur].arg);
            }
        }
    }
}

/* the renderer's way in (tools/render_screens): a screen and a tab to draw */
void ui_open(uint32_t screen, uint32_t tab)
{
    ui_nav_init(&ui_nav);
    ui_nav.tab = (uint8_t)(tab % UI_TAB_COUNT);
    ui_nav_open(&ui_nav, screen);
}

void ui_init(void)
{
    enc_left.pin_a = oc_pins.enc_l1;
    enc_left.pin_b = oc_pins.enc_l2;
    enc_right.pin_a = oc_pins.enc_r1;
    enc_right.pin_b = oc_pins.enc_r2;
    for (uint8_t p : {oc_pins.enc_l1, oc_pins.enc_l2, oc_pins.enc_r1, oc_pins.enc_r2})
        pinMode(p, INPUT_PULLUP);
    enc_left.state = (uint8_t)((digitalReadFast(oc_pins.enc_l1) << 1) | digitalReadFast(oc_pins.enc_l2));
    enc_right.state = (uint8_t)((digitalReadFast(oc_pins.enc_r1) << 1) | digitalReadFast(oc_pins.enc_r2));
    ui_nav_init(&ui_nav);
}

/* normal mode only (setup, next to the tick): the buttons held through the
 * boot are latched (they act only after a release), then the sampler runs */
void ui_start(void)
{
    ui_in_init(&ui_in, ui_buttons_now(), millis());
    ui_timer.priority(UI_SAMPLE_PRIORITY);
    ui_timer.begin(ui_sample, UI_SAMPLE_US);
}
