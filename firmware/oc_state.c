/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* The o_C port's state that the shared engine asks for.
 *
 * Three things live here:
 *
 * - The input. The FM-1 has 27 note keys and 14 buttons behind a matrix; the
 *   module has two encoders, four buttons and four digital inputs, which are
 *   read as the port's own keys and buttons (Phase 2 wires them). The engine
 *   reads the FM-1's names, so the state keeps them: fm1_in is the module's
 *   key and button state here.
 *
 * - The pitch bend, the MIDI's: the module's CV pitch follows it (oc_out.c).
 *
 * - The FM-1's FM6, ORACLE and DRUM state. The shared parameter registry still
 *   names their pages (params.c), and the port's own pages replace them in the
 *   UI phase; until then those pages are never shown and their state is inert.
 *   It is here so the shared engine compiles unchanged. */
static volatile struct {
    uint32_t notes;              /* bit n = the key n is down (the module's digital inputs as keys) */
    uint32_t buttons;            /* bit i = the button i is down */
    uint32_t pressed, released;  /* the button edges since the last read */
    uint32_t notes_pressed;      /* the key press edges since the last read */
    uint32_t note_t0[27];        /* the tick of the first scan that saw the key n down */
    uint32_t frames;
} fm1_in;

static volatile struct {
    uint32_t last, gap_max, gap_hist[8];
    uint32_t cost_sum, cost_max, ticks;
    uint32_t press_n, press_sum, press_max;
    uint32_t kb_n, kb_sum, kb_max, dac_sum, dac_max;
} fm1_in_stat;

/* the keyboard layer: which layer's keys are held (the module's digital inputs
 * can be layers), and the lock that keeps one open */
static volatile uint32_t kb_mask, kb_layer;
static volatile uint8_t kb_lock;

/* the MIDI pitch bend, Q8 semitones: the target and the value it moves toward
 * (the CV out follows it) */
static int32_t midi_bend_q8[NTRK], midi_bend_target[NTRK];

/* the FM-1's FM6 state (the shared parameter registry names its pages) */
static uint8_t fm6_patch[NTRK][FP_SIZE + 1u];
static uint8_t fm6_fn[NTRK][FM6_NFN];
static uint8_t fm6_on[NTRK];
static volatile uint8_t fm6_pgen[NTRK];

/* the FM-1's ORACLE state (the same) */
static uint8_t or_patch[NTRK][OR_N];

/* the DRUM engine's last event (the sequencer sets it when it auditions a lane
 * or records one; the engine reads it) */
static int32_t drum_event_pitch;
static uint32_t drum_event_length;

/* The FM-1's engines, called by the shared code (the MIDI CC that moves an
 * operator, the ORACLE pages). The port has none of them, so they do nothing:
 * the port's own pages replace the FM6's and the ORACLE's in the UI phase. */
static void fm6_fn_set(uint32_t tr, uint32_t k, int32_t v) { (void)tr; (void)k; (void)v; }
static void fm6_put_patch(uint32_t tr, const uint8_t *v, int load) { (void)tr; (void)v; (void)load; }
static uint8_t *or_patch_of(const track_t *t) { return or_patch[(uint32_t)(t - trk) % NTRK]; }
static uint32_t or_byte_max(uint32_t i) { (void)i; return 127u; }

/* the port's note clock: the notes that started, and the clock at each track's
 * last one (the FM-1's vage counts the voices that started) */
static uint32_t vage;
static uint32_t oc_note_at[NTRK];

/* the track's index (seq.c has trk_index, but the port's files come before it) */
static uint32_t oc_part(const track_t *t) { return (uint32_t)(t - trk); }
