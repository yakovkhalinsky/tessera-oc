/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
#include "oc_port.h"
/* The o_C port's output: a note goes to the rig as CV (the pitch) and as a gate
 * or a trigger. No MIDI: the module is a sequencer for the rig, not a voice in
 * it, and the rig reads CV and gates.
 *
 * The module's four CV outs (a DAC8565, then an OPA2172 buffer) and its four
 * digital outs take one track each: track 1..4 on CV 1..4 and on TRIG 1..4.
 * Tessera is a four-track instrument and the o_C is a four-channel one, so the
 * mapping is one to one and every track keeps its own key, scale, transpose
 * and cords; the harmony (the chord track, QNT CHRD) lands on the outputs as
 * it should.
 *
 * The digital outs are 3.3 V logic, where a Eurorack gate is +5 V: most gate
 * inputs take 3.3 V, the rest want a level shifter. Each one is a gate (up for
 * the note, down at its end) or a trigger (a short pulse at the note's onset),
 * the track's own setting. A gate is the default, because a gate is what a
 * voice wants; a trigger is what a drum voice wants, and a drum track does not
 * need its CV patched anywhere. A third mode, a clock, lets a track's out
 * carry the tempo instead, for a rig the module is the master of; the usual
 * way round is the other one: the rig's clock comes in on a digital input and
 * the module follows it.
 *
 * The gate's length is the sequencer's own gate (the step's GATE, the LENGTH
 * page), so it moves with the music and the cords can move it too. The
 * velocity has nowhere to go: there is no CV spare for it and no MIDI, so an
 * accent is heard in the timing, not in the level.
 *
 * The pitch: 1 V an octave, the o_C's own convention, so the port's CV outs
 * are interchangeable with the module's: 0 V is the note the o_C puts there
 * (MIDI 36, C2, with the T4.0's kOctaveZero = 3), an octave up is a volt
 * more, and the reference follows the sequencer (the track's key and scale,
 * its transpose, its octave). 0.1 V an octave (V/oct systems) is the other
 * setting.
 *
 * The CV out's range is the module's own: the Vbias is asymmetric, so the
 * output spans -3 V to +7 V, MIDI note 0 to 120. The DAC is not linear enough
 * to be trusted over that span, so the codes come from the module's own
 * calibration table (per channel, per octave, in its EEPROM) rather than from
 * a straight line.
 *
 * So the engine hands the platform a *note*, and the platform turns it into the
 * DAC's code the way the o_C does (its kOctaveZero, its calibration): the
 * engine knows the music, the platform knows the module. */

/* a trigger's pulse: wide enough for a slow gate input */
#define OC_TRIG_MS 5u
#define OC_TRIG_TICKS (FS * OC_TRIG_MS / 1000u)
/* what a track's digital out carries */
enum { OC_OUT_GATE, OC_OUT_TRIG, OC_OUT_CLOCK };
static uint32_t oc_trig_until[NTRK];       /* the tick its trigger pulse ends at */
static uint32_t oc_gate_until[NTRK];       /* the tick a gate falls at (the accent) */
static uint8_t oc_vel[NTRK];               /* the last note's velocity */
static int32_t oc_pit[NTRK];               /* the cords' pitch, in 1/16 semitones */

/* OVERVIEW's live values: one store in the tick, the UI reads them in loop() */
static volatile uint8_t oc_gate_on[NTRK];
static volatile uint8_t oc_gate_seen_flag[NTRK];
static volatile int16_t oc_last_note[NTRK] = {-1, -1, -1, -1};

static void oc_gate_out(uint32_t part, uint32_t on)
{
    if (part < NTRK)
        oc_gate_on[part] = on ? 1u : 0u;
    oc_gate_write(part, on);
}

int16_t oc_track_last_note(uint32_t tr)
{
    return tr < NTRK ? oc_last_note[tr] : (int16_t)-1;
}

uint8_t oc_track_gate_seen(uint32_t tr)
{
    uint8_t v;
    if (tr >= NTRK)
        return 0;
    v = (uint8_t)(oc_gate_seen_flag[tr] | oc_gate_on[tr]);
    oc_gate_seen_flag[tr] = 0;
    return v;
}

/* the mode is the track's own parameter (the port's engine's P_E4), so the
 * cords can move it like any other */
uint32_t oc_out_mode(uint32_t part)
{
    return part < NTRK ? (uint32_t)trk[part].p[P_E4] : 0u;
}

/* the project's octave offset: every track's CV with it */
static int32_t oc_cv_octave;

/* The CV out's pitch, in the module's own unit: a 128th of a semitone, with 0
 * the note at 0 V (MIDI 36, C2, the T4.0's kOctaveZero = 3). The platform
 * turns it into the DAC's code through the module's calibration, the o_C's own
 * way.
 *
 * Onto the note:
 * - the track's own octave (P_E0) and the project's octave offset;
 * - the track's cents offset (P_E1), which the cords can move;
 * - the cords' pitch (the matrix's PITCH destination: the LFO and the other
 *   per-block sources), in the engine's own 1/16 semitone unit;
 * - the LFO's own depth (P_LD_PIT, the vibrato), in the same unit;
 * - the MIDI bend.
 *
 * So the whole modulation matrix reaches the CV, not just the engine's
 * parameters: a cord to PITCH is a vibrato on the rig. */
static int32_t oc_cv_pitch(uint32_t part, uint32_t note)
{
    int32_t p = ((int32_t)note - 36) * 128;         /* the 128th of a semitone */
    int32_t lfo, bend;

    if (part < NPART) {
        p += (int32_t)trk[part].p[P_E0] * 12 * 128; /* the track's octave */
        p += (int32_t)trk[part].p[P_E1] * 128 / 100; /* the cents offset */
    }
    p += oc_cv_octave * 12 * 128;                    /* the project's octave */

    lfo = mulq15(trk[part].lfo_val, trk[part].lfo_fade);
    p += ((lfo * trk[part].p[P_LD_PIT] * 3) >> 15) * 8;   /* the LFO's depth: 1/16 semitones */
    p += oc_pit[part] * 8;                                /* the cords' pitch: 1/16 semitones */
    bend = midi_bend_q8[part] * 128 / 25;                 /* the bend: Q8 semitones */
    p += bend;
    return p;
}

/* a note on: its pitch on the track's CV out, and its gate or its trigger */
static void oc_note_on(uint32_t part, uint32_t note, uint32_t vel)
{
    (void)vel;
    if (part >= NPART)
        return;
    oc_last_note[part] = (int16_t)note;
    oc_gate_seen_flag[part] = 1;
    oc_cv_write(part, (uint32_t)oc_cv_pitch(part, note));
    if (oc_out_mode(part) == OC_OUT_TRIG) {
        oc_trig_until[part] = oc_ticks() + OC_TRIG_TICKS;  /* a pulse at the note's onset */
        oc_gate_out(part, 1);
    } else if (oc_out_mode(part) != OC_OUT_CLOCK) {
        oc_gate_out(part, 1);
    }
}

/* the note's end: the gate down. The pitch stays where it was, so a rig that
 * holds the last pitch (a VCA without a gate, a filter) keeps it. A trigger's
 * pulse ends on its own, so the note's end does not touch it. */
static void oc_note_off(uint32_t part, uint32_t note)
{
    (void)note;
    if (part >= NPART)
        return;
    if (oc_out_mode(part) == OC_OUT_TRIG)
        return;                                     /* a trigger's pulse ends on its own */
    if (oc_out_mode(part) == OC_OUT_CLOCK)
        return;
    {   /* the accent: a loud note holds its gate a little longer (the modular's
         * way: there is no level to speak of, only the gate's length) */
        uint32_t accent = part < NPART ? (uint32_t)trk[part].p[P_E2] : 0u;
        uint32_t extra = 0;
        if (accent && oc_vel[part] > 96u)
            extra = (uint32_t)(((uint64_t)(oc_vel[part] - 96u) * accent * 100u * FS) / (127u * 1000u));
        if (extra) {
            oc_gate_until[part] = oc_ticks() + extra;
            return;
        }
        oc_gate_out(part, 0);
    }
}

/* the module's tick: a trigger's pulse ends on its own, and a track whose out
 * carries the clock writes it (Phase 2). */
void oc_out_tick(void)
{
    uint32_t now = oc_ticks(), i;
    for (i = 0; i < NTRK; i++) {
        if (oc_out_mode(i) == OC_OUT_TRIG && oc_trig_until[i] && now >= oc_trig_until[i]) {
            oc_trig_until[i] = 0;
            oc_gate_out(i, 0);
        }
        if (oc_gate_until[i] && now >= oc_gate_until[i]) {   /* the accent's gate */
            oc_gate_until[i] = 0;
            oc_gate_out(i, 0);
        }
    }
}
