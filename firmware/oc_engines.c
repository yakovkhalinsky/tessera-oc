/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* The o_C port's engine layer.
 *
 * The module has no audio output: it sends CV and MIDI to the rig, it makes no
 * sound of its own. So there is no engine to render, and the table's entry 0 is
 * the port's own: a voice that makes no sound but keeps the state the sequencer
 * reads (the part's voice mode, the fresh-phrase rule). Its eight engine
 * parameters are the CV output's own (the octave, the level, the gate length,
 * the pitch scale); the rest of the table is retired, as in the FM-1 build
 * without its synths.
 *
 * The stored indices are the FM-1's (core.h: the engine numbers of the editor
 * protocol, append only), so a project saved on an FM-1 keeps its sounds'
 * indices; here they all name the port's engine. */
#define OC_EDIT(label, fmt, mn, mx, df) {label, fmt, mn, mx, df, 0, 0}
#define OC_EDIT_NONE {"-", F_INT, 0, 0, 0, 0, 0}

static void oc_engine_note_on(struct track *t, voice_t *v) { (void)t; (void)v; }
static void oc_engine_render(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    (void)t; (void)v; (void)out; (void)n; (void)m;
}

/* the port's engine: the CV and MIDI the track sends to the rig */
static const engine_t ENG_CV = {
    .name = "CV",
    .page_title = {"CV 1", "CV 2"},
    .edit = {
        OC_EDIT("OCT", F_OCT, -3, 3, 0),         /* P_E0: the track's octave on the CV out */
        OC_EDIT("LEVEL", F_INT, 0, 127, 100),    /* P_E1: how hard the CV out drives */
        OC_EDIT("GATE", F_INT, 0, 127, 64),      /* P_E2: the gate's length, as a share of the step */
        OC_EDIT("V/OCT", F_INT, 0, 1, 0),        /* P_E3: 0 = 1 V an octave (Eurorack), 1 = 0.1 V */
        OC_EDIT_NONE, OC_EDIT_NONE, OC_EDIT_NONE, OC_EDIT_NONE,
    },
    .npresets = 0,
    .knob = {P_LEVEL, P_TRANS, P_LRATE, P_LD_PIT},  /* HOME: level, transpose, the LFO's rate and depth */
    .poly = 0,                                    /* NPOLY: the cap of a POLY track */
    .oneshot = 0,
    .note_on = oc_engine_note_on,
    .render = oc_engine_render,
};

/* the retired engines the sequencer names: ENG_DRUM (a drum track is a drum
 * track, whatever it renders), ENG_FM6, ENG_SID and ENG_SLICE (the voice layer
 * compares them). Each is its own object, so a compare against one is false. */
#define OC_RETIRED(nm) \
    .name = nm, .page_title = {nm, nm}, \
    .edit = {OC_EDIT_NONE, OC_EDIT_NONE, OC_EDIT_NONE, OC_EDIT_NONE, \
             OC_EDIT_NONE, OC_EDIT_NONE, OC_EDIT_NONE, OC_EDIT_NONE}, \
    .npresets = 0, .knob = {P_LEVEL, P_TRANS, P_LRATE, P_LD_PIT}, \
    .poly = 0, .oneshot = 0, .note_on = oc_engine_note_on, .render = oc_engine_render

static const engine_t ENG_RETIRED = {OC_RETIRED("-")};
static const engine_t ENG_DRUM = {OC_RETIRED("DRUM")};
static const engine_t ENG_FM6 = {OC_RETIRED("FM6")};
static const engine_t ENG_SID = {OC_RETIRED("SID")};
static const engine_t ENG_SLICE = {OC_RETIRED("SLICE")};

/* the table (order = the PRESETS browsing order and the engine numbers of the
 * editor protocol): entry 0 is the port's own, the rest are retired */
static const engine_t *const ENGINES[NENGINES] = {
    &ENG_CV,                     /* 0: the port's engine (CV and MIDI out) */
    &ENG_RETIRED,                /* 1: retired DIGITAL */
    &ENG_RETIRED,                /* 2: retired PHASE */
    &ENG_SID,                    /* 3: SID (its stored indices are kept) */
    &ENG_RETIRED,                /* 4: retired SAMPLE */
    &ENG_RETIRED,                /* 5: retired VOICE */
    &ENG_RETIRED,                /* 6: retired TRIO */
    &ENG_RETIRED,                /* 7: retired WHEEL */
    &ENG_RETIRED,                /* 8: retired GRAIN */
    &ENG_RETIRED,                /* 9: retired PHYS */
    &ENG_DRUM,                   /* 10 (ENGI_DRUM) */
    &ENG_RETIRED,                /* 11: retired NOISE */
    &ENG_FM6,                    /* 12 (ENGI_FM6) */
    &ENG_SLICE,                  /* 13 (ENGI_SLICE) */
    &ENG_RETIRED,                /* 14: retired OBXF */
    &ENG_RETIRED,                /* 15: retired CZ-1 */
    &ENG_RETIRED, &ENG_RETIRED, &ENG_RETIRED, /* 16..18 reserved */
    &ENG_RETIRED,                /* 19: retired ORACLE */
    &ENG_RETIRED,                /* 20: reserved */
    &ENG_RETIRED,                /* 21: retired MACRO */
};

/* a track's engine number as an index (the audio paths: a compare, cheaper than % NENGINES; a bad number: 0) */
static inline uint32_t eng_idx(uint32_t e) { return e < NENGINES ? e : 0u; }

/* the engines one can pick: here the port's own only */
static int eng_ok(uint32_t e) { return e == 0u; }

/* Each track owns only its selected engine's working state. The port's engine
 * has none: the note goes straight to the CV and the MIDI out. */
static void eng_state_clear(uint32_t part)
{
    if (part < NPART) resource_release(RES_ENGINE0 + part);
}
static void eng_state_reset(void) { for (uint32_t k = 0; k < NPART; k++) eng_state_clear(k); }
static int eng_state_prepare(const track_t *t) { (void)t; return 1; }
