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
#include <stdbool.h>

#include "param_desc.h"     /* the engine's parameter descriptor (core.h has it too) */

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

/* the parameters, as the screen shows them: the descriptor (the label, the
 * range, the format, the names), the name, the value and the value's text */
const param_desc_t *oc_tx_desc(uint32_t tx);
const param_desc_t *oc_track_desc(uint32_t id);
const param_desc_t *oc_engine_desc(uint32_t id);
const char *oc_param_name(uint32_t pid);
void oc_param_format(const param_desc_t *d, int32_t v, char *val, const char **unit);
int32_t oc_tx_get(uint32_t tr, uint32_t tx);
void oc_tx_set(uint32_t tr, uint32_t tx, int32_t v);
int32_t oc_track_param_get(uint32_t tr, uint32_t id);
void oc_track_param_set(uint32_t tr, uint32_t id, int32_t v);

/* the rows the screen shows: the generator's own knobs, and the track's key
 * and scale. The ids are the engine's: the port asks for the list. */
const uint8_t *oc_gen_rows(uint32_t gen, uint8_t *count);
const uint8_t *oc_scl_rows(uint8_t *count);
const uint8_t *oc_cv_rows(uint8_t *count);
uint32_t oc_cv_scale(uint32_t part);
uint32_t oc_out_mode(uint32_t part);

/* the track's generator */
void oc_set_generator(uint32_t tr, int32_t mode);

/* the project: the state, packed and unpacked (the engine's), and the slots
 * it is kept in (the platform's storage) */
uint32_t oc_project_pack(uint8_t *out, uint32_t max);
int oc_project_unpack(const uint8_t *in, uint32_t len);
int oc_save(uint32_t slot);
int oc_load(uint32_t slot);
int oc_slot_used(uint32_t slot);
int oc_last_slot(void);
void oc_set_last_slot(uint32_t slot);
bool oc_storage_init(void);

/* --- the platform's side --- */

/* the CV out: the pitch for the channel, in the 128th of a semitone, with 0
 * the note at 0 V (MIDI 36, C2). The platform turns it into the DAC's code,
 * through the module's calibration. */
void oc_cv_write(uint32_t ch, uint32_t pitch);

/* a digital out: high or low (a gate, a trigger or a clock) */
void oc_gate_write(uint32_t ch, uint32_t on);

#ifdef __cplusplus
}
#endif

#endif
