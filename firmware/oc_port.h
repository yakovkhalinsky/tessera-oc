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

/* a MIDI packet in, from the module's USB (the USB-MIDI four bytes: the cable,
 * the status, the two data bytes) */
void oc_midi_in(uint32_t pkt);

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
const uint8_t *oc_mod_rows(uint8_t *count);
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
bool oc_storage_ok(void);

/* --- the module's own --- */

/* The module's pins: the o_C's own T4.0 pinout, and the mirrored one for a
 * module built the other way round (the calibration's FLIPCONTROLS bit).
 *
 * The tables are the o_C's own firmware's (Phazerville PSv2.0.2 OC_gpio.cpp:
 * the default at :128-136, the flipped one at :82-94):
 *
 *   the default: but_top 5, but_bot 4, encL1 22, encL2 21, butL 23,
 *                encR1 16, encR2 15, butR 14, TR1..TR4 = 0, 1, 2, 3;
 *   the flipped:  but_top 4, but_bot 5, encL1 15, encL2 16, butL 14,
 *                encR1 21, encR2 22, butR 23, TR1..TR4 = 3, 2, 1, 0.
 *
 * The flip is the calibration's FLIPCONTROLS bit: the o_C's own
 * CALIBRATION_FLAG_FLIPCONTROLS is 1u << 3 (OC_calibration.h:130), and
 * Main.cpp:349 hands it to OC::SetFlipMode, which swaps the pins as above.
 *
 * The remap may move the module's inputs and nothing else: the OLED's three
 * pins (6, 7, 8), the DAC's two (9, 10) and the SPI's (11, 12, 13 and 25, 26,
 * 27) are the module's own. */
struct oc_pinout {
    uint8_t but_top, but_bot, but_l, but_r;
    uint8_t enc_l1, enc_l2, enc_r1, enc_r2;
    uint8_t tr1;             /* the trigger input the rig's clock comes in on */
};
static const struct oc_pinout OC_PINOUT_PLAIN = {5, 4, 23, 14, 22, 21, 16, 15, 0};
static const struct oc_pinout OC_PINOUT_FLIP = {4, 5, 14, 23, 15, 16, 21, 22, 3};
#define OC_PIN_MOVABLE(p) ((p) < 6u || ((p) > 13u && (p) < 25u) || (p) > 27u)

/* The module's output pins: the OLED's three (6, 7, 8), the DAC's two (9, 10),
 * the SPI's three (11, 12, 13) and the SPI's chip select (37). The remap may
 * not touch them, and the firmware drives nothing else. The firmware and the
 * host test have the same set, and the two must not meet. */
#define OC_OUTPUT_PIN_LIST {6, 7, 8, 9, 10, 11, 12, 13, 37}

extern struct oc_pinout oc_pins;

/* The calibration's flags, as the module's own page has them: the display's
 * offset (the SH1106's RAM is 132 wide) and the flags (the FLIPSCREEN and
 * the FLIPCONTROLS bits, the o_C's own CALIBRATION_FLAG_*: 1u << 2 and
 * 1u << 3, OC_calibration.h:129-130). */
#define OC_CAL_FLIPSCREEN 0x04u
#define OC_CAL_FLIPCONTROLS 0x08u
extern uint32_t oc_cal_flags;
extern uint8_t oc_cal_display_offset;

/* The encoders' quadrature: the o_C's own state table (the direction of a
 * turn and how far it went), and the row it moves. Portable on purpose: the
 * host test drives it (tests/oc_port_test.c).
 *
 * The direction is the sign of the steps: a turn backwards is as many steps as
 * a turn forwards. The old loop (while (steps--)) never ended for a negative
 * count, and a turn the other way hung the module. */
static inline int32_t oc_enc_step(uint8_t *state, int8_t *acc, uint8_t new_state)
{
    static const int8_t q[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
    int32_t steps;
    if (new_state == *state)
        return 0;
    *acc = (int8_t)(*acc + q[(*state << 2) | new_state]);
    *state = new_state;
    steps = *acc / 4;                     /* a detent is a whole cycle */
    *acc = (int8_t)(*acc - steps * 4);
    return steps;
}

static inline uint8_t oc_enc_row(uint8_t row, int32_t steps, uint32_t n)
{
    int32_t dir = steps < 0 ? -1 : 1;
    uint32_t k = steps < 0 ? (uint32_t)(-steps) : (uint32_t)steps;
    while (k-- && n)
        row = (uint8_t)(((int32_t)row + dir + (int32_t)n) % (int32_t)n);
    return row;
}

/* --- the diagnostics --- */
/* The stage word (the persistent one), the timing, the crash report on the
 * screen and the safe mode. */
void oc_diag_begin(void);
void oc_stage(uint32_t n);
void oc_diag_block(uint32_t cycles);
void oc_diag_loop(void);
uint32_t oc_diag_cyccnt(void);
void oc_diag_crash(void);
void oc_diag_safe(void);

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
