/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* The o_C port's note layer.
 *
 * There are no voices here: the module sends the note to the rig (CV and MIDI)
 * and the sequencer's own state is what it reads back. The FM-1's voice layer
 * gave the sequencer two things: the notes a track was sounding, and the
 * fresh-phrase rule (a track whose notes have all stopped starts a new
 * phrase, and the LFO starts with it unless TRIG NOTE is on). Both are kept:
 * the gate is the note's own, going up at the note and down at its end, with no
 * release after it - the module's CV gate has no tail.
 *
 * The MIDI out is the FM-1's own path, unchanged: it is what the rig hears
 * when it is driven over MIDI rather than CV (midi_out.c, included by seq.c). */

static void midi_out_on(const track_t *t, uint32_t note, uint32_t vel);   /* seq.c midi_out.c: MIDI OUT */
static void midi_out_off(const track_t *t, uint32_t note);
static void midi_out_flush(uint32_t k);

/* the notes each track still holds (the gate up): the fresh-phrase rule reads it */
#define OC_HELD_MAX 16u
static uint8_t oc_held[NTRK][OC_HELD_MAX];
static uint8_t oc_held_n[NTRK];

static void oc_hold_add(uint32_t k, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < oc_held_n[k]; i++)
        if (oc_held[k][i] == note)
            return;                                 /* sounding already: it starts again */
    if (oc_held_n[k] < OC_HELD_MAX)
        oc_held[k][oc_held_n[k]++] = (uint8_t)note;
}

static void oc_hold_del(uint32_t k, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < oc_held_n[k]; i++)
        if (oc_held[k][i] == note) {
            oc_held[k][i] = oc_held[k][--oc_held_n[k]];
            return;
        }
}

static int oc_holding(uint32_t k) { return oc_held_n[k] != 0u; }

/* the note goes to the rig: the CV out and its gate (oc_out.c) */
static void oc_note_on(uint32_t part, uint32_t note, uint32_t vel);
static void oc_note_off(uint32_t part, uint32_t note);

static void trk_note_on_work(track_t *t, uint32_t note, uint32_t vel)
{
    uint32_t k = oc_part(t);
    if (t->p[P_MUTE])
        return;
    mod_note(t, note, vel);                         /* the matrix's VEL / KEY / RAND */
    if (!oc_holding(k)) {                            /* fresh phrase: the LFO starts with it */
        if (!t->p[P_LTRIG])
            t->lfo_ph = (uint32_t)t->p[P_LPHASE] << 25;
        t->lfo_fade = 0;
    }
    oc_hold_add(k, note);
    vage++;
    oc_note_at[k] = vage;                           /* the note clock (the FM-1's vage) */
    oc_note_on(k, note, vel);
}

static void trk_note_on(track_t *t, uint32_t note, uint32_t vel)
{
    midi_out_on(t, note, vel);
    trk_note_on_work(t, note, vel);
}

static void trk_note_off(track_t *t, uint32_t note)
{
    uint32_t k = oc_part(t);
    midi_out_off(t, note);
    oc_hold_del(k, note);
    oc_note_off(k, note);
}

static void trk_all_off(track_t *t)
{
    uint32_t k = oc_part(t);
    midi_out_flush(k);
    oc_held_n[k] = 0;
    oc_note_off(k, 0);
}

/* engine switch, at each block start (events_block), before any note of the
 * block. The port has one engine: nothing switches, and nothing is queued. */
static void engine_block(track_t *t) { (void)t; }

/* a voice given up (the budget, an overload): there are no voices here */
static void voice_kill(voice_t *v) { (void)v; }
static uint32_t voice_kills, voice_kills_part[NPART];
