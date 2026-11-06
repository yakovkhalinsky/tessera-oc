/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The o_C port's interface: the engine (C) and the module (C++).
 *
 * The engine is one translation unit (oc_engine.c) and knows nothing of the
 * module: it counts in the sample at FS, it hands the notes over, and the
 * platform writes them. The platform advances the clock and writes the CV out
 * and the digital outs.
 *
 * The two sides are linked with C linkage, so the engine stays C and the
 * platform can use the Arduino's C++ (SPI, the timers, the EEPROM). */
#ifndef OC_PORT_H_
#define OC_PORT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- the engine's side --- */

/* the boot: the defaults and the empty patterns */
void oc_engine_init(void);

/* the power-on: the generators on the tracks, and the transport running. The
 * port composes, so it does not wait to be played. */
void oc_power_on(void);

/* the clock: the platform advances it by the samples that passed. The engine
 * counts in the sample at FS, exactly as the FM-1 does, and runs the blocks. */
void oc_clock_advance(uint32_t samples);

/* the sample rate the engine counts at (FS) */
uint32_t oc_sample_rate(void);

/* the outputs' tick: a trigger's pulse ends on its own */
void oc_out_tick(void);

/* the transport: 1 = start, 2 = stop, 3 = restart from the top */
void oc_transport(uint8_t request);

/* is the transport running? */
uint8_t oc_playing(void);

/* the clock source: 0 = the port's own tempo, 2 = an external clock, which
 * arrives on the module's first digital input as clock pulses */
void oc_clock_source(uint8_t source);

/* a clock pulse from the external clock (the engine's own MIDI clock path:
 * the pulses are what it follows) */
void oc_clock_pulse(void);

/* a start from the external clock */
void oc_clock_start(void);

/* is the port following an external clock? (the platform knows) */
uint8_t oc_clock_following(void);

/* what the screen shows: the selected track, a track's generator, the tempo
 * and a track's octave on its CV out */
uint8_t oc_selected_track(void);
void oc_select_track(uint8_t track);
uint8_t oc_track_generator(uint32_t track);
int32_t oc_track_octave(uint32_t track);
int16_t oc_tempo(void);
void oc_set_tempo(int16_t bpm);

/* --- the platform's side --- */

/* the CV out: the note for the channel (a MIDI note number, 0..127). The
 * platform puts it at the module's own reference and turns it into the DAC's
 * code, through the module's calibration. */
void oc_cv_write(uint32_t ch, uint32_t note);

/* a digital out: high or low (a gate, a trigger or a clock) */
void oc_gate_write(uint32_t ch, uint32_t on);

#ifdef __cplusplus
}
#endif

#endif
