/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The port's power-on.
 *
 * The FM-1's power-on leaves the sequencers empty: it is an instrument, and
 * someone plays it. The port is the other way round - it is a sequencer for the
 * rig, so it composes. So the tracks get their generators at power-on, one
 * each, and the transport starts.
 *
 * The generators are the engine's own (tessera.c): the mode knob picks the
 * track's generator, and the key link (on by default) gives them all one key
 * and scale, so the four CV outs carry lines that agree with each other.
 *
 * The tracks' octaves (P_E0, the port's own parameter: how high a track sits on
 * its CV out) space them out: a bass, a low line, a high line, a middle one.
 * On a rig four lines in one octave are mud; four lines an octave apart are a
 * band. */
void oc_power_on(void)
{
    static const uint8_t MODE[4] = {TESS_ENG_TURING, TESS_ENG_EUCLID, TESS_ENG_MARKOV, TESS_ENG_MORPH};
    static const int8_t OCT[4] = {-2, -1, 1, 0};
    uint32_t i;

    for (i = 0; i < 4u; i++) {
        song.sel = (uint8_t)i;
        tessera_knob(TX_MODE, MODE[i]);
        trk[i].p[P_E0] = OCT[i];
    }
    song.sel = 0;
    oc_transport(1);                  /* play */
}
