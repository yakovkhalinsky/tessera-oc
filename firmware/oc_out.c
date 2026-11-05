/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* The o_C port's output: a note goes to the rig as CV (the pitch) and a gate,
 * and as MIDI (the FM-1's own MIDI out path, unchanged).
 *
 * The module's four CV outs (a DAC8565, then an OPA2172 buffer: 0..10 V, or
 * +-10 V with the biased Vbias) and its four trigger outs take one track each:
 * track 1..4 on CV 1..4 and on TRIG 1..4. The trigger outs are the module's
 * digital pins, 3.3 V logic, so a rig whose gates want +-5 V needs a level
 * shifter; the CV outs are the ones that carry the voltage.
 *
 * The pitch: 1 V an octave by default (Eurorack), the reference note at the
 * middle of the CV out's range, so the rig's own 0 V is the reference and the
 * scale is the one the sequencer already follows (the track's key and scale,
 * its transpose, its octave). 0.1 V an octave (V/oct systems) is the other
 * setting.
 *
 * The platform writes what comes out of here (oc_cv_write, oc_gate_write);
 * the MIDI bytes go out through the module's own MIDI port (midi_out_event). */

/* the CV out's scale, in millivolts a semitone, Q8 */
#define OC_MV_PER_SEMITONE_1VOCT 21333u   /* 1000/12 mV, Q8 */
#define OC_MV_PER_SEMITONE_01VOCT 2133u    /* 100/12 mV, Q8 */
/* the reference: the note at the middle of the CV out's range */
#define OC_CV_REFERENCE 60u                 /* C4 */
/* the CV out's range, in millivolts, and the DAC's codes over it */
#define OC_CV_MIN_MV (-5000)
#define OC_CV_MAX_MV (5000)
#define OC_CV_CODES 65535u

/* the platform's side: the DAC, the trigger outs, the MIDI port */
static void oc_cv_write(uint32_t ch, uint32_t code);
static void oc_gate_write(uint32_t ch, uint32_t on);
static void midi_out_event(uint32_t v);

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

/* a note on: its pitch on the track's CV out, its gate up */
static void oc_note_on(uint32_t part, uint32_t note, uint32_t vel)
{
    (void)vel;
    if (part >= NPART)
        return;
    oc_cv_write(part, oc_cv_code(part, note));
    oc_gate_write(part, 1);
}

/* the note's end: the gate down. The pitch stays where it was, so a rig that
 * holds the last pitch (a VCA without a gate, a filter) keeps it. */
static void oc_note_off(uint32_t part, uint32_t note)
{
    (void)note;
    if (part >= NPART)
        return;
    oc_gate_write(part, 0);
}
