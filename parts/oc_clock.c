/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The external clock.
 *
 * A modular rig's clock is a pulse train, not MIDI: the o_C takes it on its
 * first digital input and follows it. The engine already knows how to follow a
 * clock - the MIDI clock path (midi_clock.c) measures the tempo from the
 * pulses, keeps the musical phase exact (1024 units a pulse) and hands the
 * timing back to the port's own clock when the pulses stop.
 *
 * So the port does not need a clock of its own: it turns the rig's pulses into
 * the engine's MIDI clock messages (the same ones a TRS MIDI clock would
 * bring) and the engine does the rest. The source is the TRS one (2), so the
 * engine accepts them and ignores any other source. */
void oc_clock_source(uint8_t source)
{
    song.g[G_CLOCK] = (int16_t)source;
}

void oc_clock_pulse(void)
{
    midi_enqueue(0xF8u | 0xF8u << 8, 2u);        /* a clock pulse */
}

void oc_clock_start(void)
{
    midi_enqueue(0xFAu | 0xFAu << 8, 2u);        /* a start */
}
