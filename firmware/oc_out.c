/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
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
 * a straight line - Phase 2 reads it.
 *
 * The platform writes what comes out of here (oc_cv_write, oc_gate_write). */

/* the CV out's scale, in millivolts a semitone, Q8 */
#define OC_MV_PER_SEMITONE_1VOCT 21333u   /* 1000/12 mV, Q8 */
#define OC_MV_PER_SEMITONE_01VOCT 2133u    /* 100/12 mV, Q8 */
/* the reference: the note at 0 V (the o_C's own: MIDI 36, C2, with the T4.0's
 * kOctaveZero = 3 - the module's MIDI CV() and NoteNumber() agree with it) */
#define OC_CV_REFERENCE 36u
/* a trigger's pulse: wide enough for a slow gate input */
#define OC_TRIG_MS 5u
#define OC_TRIG_TICKS (FS * OC_TRIG_MS / 1000u)
/* the CV out's range, in millivolts: the module's asymmetric Vbias, -3 V to +7 V */
#define OC_CV_MIN_MV (-3000)
#define OC_CV_MAX_MV (7000)
#define OC_CV_CODES 65535u

/* the platform's side: the DAC and the digital outs */
static void oc_cv_write(uint32_t ch, uint32_t code);
static void oc_gate_write(uint32_t ch, uint32_t on);

/* what a track's digital out carries */
enum { OC_OUT_GATE, OC_OUT_TRIG, OC_OUT_CLOCK };
static uint8_t oc_out_mode[NTRK];          /* the track's own setting */
static uint32_t oc_trig_until[NTRK];       /* the tick its trigger pulse ends at */

/* the CV out's scale: 0 = 1 V an octave, 1 = 0.1 V */
static uint32_t oc_cv_scale;               /* the project's setting */
static int32_t oc_cv_octave;               /* the project's octave offset */

/* a note as the CV out's code. The track's own octave (P_E0) and the project's
 * octave offset move it; the transpose (P_TRANS) is in semitones. */
static uint32_t oc_cv_code(uint32_t part, uint32_t note)
{
    int32_t semitones = (int32_t)note - (int32_t)OC_CV_REFERENCE;
    uint32_t q8 = oc_cv_scale ? OC_MV_PER_SEMITONE_01VOCT : OC_MV_PER_SEMITONE_1VOCT;
    int32_t mv;
    if (part < NPART)
        semitones += (int32_t)trk[part].p[P_E0] * 12;
    semitones += oc_cv_octave * 12;
    mv = (semitones * (int32_t)q8) >> 8;
    mv += OC_CV_MIN_MV;
    if (mv < 0) mv = 0;
    if (mv > OC_CV_MAX_MV - OC_CV_MIN_MV) mv = OC_CV_MAX_MV - OC_CV_MIN_MV;
    return (uint32_t)((uint64_t)mv * OC_CV_CODES / (uint64_t)(OC_CV_MAX_MV - OC_CV_MIN_MV));
}

/* a note on: its pitch on the track's CV out, and its gate or its trigger */
static void oc_note_on(uint32_t part, uint32_t note, uint32_t vel)
{
    (void)vel;
    if (part >= NPART)
        return;
    oc_cv_write(part, oc_cv_code(part, note));
    if (oc_out_mode[part] == OC_OUT_TRIG) {
        oc_trig_until[part] = oc_ticks() + OC_TRIG_TICKS;  /* a pulse at the note's onset */
        oc_gate_write(part, 1);
    } else if (oc_out_mode[part] != OC_OUT_CLOCK) {
        oc_gate_write(part, 1);
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
    if (oc_out_mode[part] != OC_OUT_TRIG)
        oc_gate_write(part, 0);
}

/* the module's tick: a trigger's pulse ends on its own, and a track whose out
 * carries the clock writes it (Phase 2). */
static void oc_out_tick(void)
{
    uint32_t now = oc_ticks(), i;
    for (i = 0; i < NTRK; i++)
        if (oc_out_mode[i] == OC_OUT_TRIG && oc_trig_until[i] && now >= oc_trig_until[i]) {
            oc_trig_until[i] = 0;
            oc_gate_write(i, 0);
        }
}
