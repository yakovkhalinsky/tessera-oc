/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The o_C port's platform layer: the module itself.
 *
 * A Teensy 4.0 in an o_C module: four CV outs (a DAC8565 over SPI, then an
 * OPA2172 buffer and the module's Vbias), four digital outs, the encoders and
 * the buttons, the OLED (the UI is Phase 4) and the module's EEPROM, where its
 * calibration lives.
 *
 * The engine (oc_engine.c, C) is driven from the module's 60 us tick, the same
 * rate the o_C's own firmware runs its core at: the tick advances the engine's
 * clock (the sample at FS, exactly) and the engine's notes go out as CV and as
 * gates.
 *
 * The pin numbers are the o_C's own (oc-phazerville's OC_gpio.cpp, the T4.0's
 * static pinout; the T4.1's are detected at run time and differ).
 *
 * The CV out's mapping is the o_C's, not ours: 0 V is MIDI 36 (C2), the
 * T4.0's kOctaveZero = 3, an octave up is a volt more, and the DAC's codes
 * come from the module's own calibration (per channel, per octave), so the
 * port's CV outs are interchangeable with the module's. */

#include <Arduino.h>
#include <SPI.h>
#include <EEPROM.h>
#include "oc_port.h"

/* --------------------------------------------------------- the o_C's pins --- */
#define PIN_DAC_CS 10          /* the DAC8565's chip select (SPI0's) */
#define PIN_DAC_RST 9          /* its reset */
#define PIN_TR1 0              /* the digital ins (an external clock comes in on TR1) */
#define PIN_TR2 1
#define PIN_TR3 2
#define PIN_TR4 3
#define PIN_BUT_TOP 5          /* the buttons: the top, the bottom, and the two encoders' */
#define PIN_BUT_BOT 4
#define PIN_BUT_L 23
#define PIN_BUT_R 14
#define PIN_ENC_L1 22          /* the left encoder (quadrature) */
#define PIN_ENC_L2 21
#define PIN_ENC_R1 16          /* the right encoder */
#define PIN_ENC_R2 15

/* the DAC8565: four channels, 16 bits, the output inverted */
#define DAC_CMD(ch) (0x10u + ((ch) & 3u) * 2u)     /* 0x10, 0x12, 0x14, 0x16 */
#define DAC_MAX 65535u
#define DAC_OCTAVES 11         /* the calibration's octaves: MIDI 0..120 */

/* the module's tick: the o_C's own core rate, 60 us */
#define OC_TICK_US 60u

/* ------------------------------------------------- the module's calibration --- */
/* The calibration lives in the module's EEPROM, in the o_C's own page layout:
 * a header (fourcc 'CAL1', a generation, a size, a checksum) and then the
 * data, whose first part is the DAC's table: four channels of eleven octaves,
 * the DAC's code at the bottom of each octave. */
#define EEPROM_CAL_BASE 0
#define EEPROM_CAL_END 128
struct oc_cal_page {
    uint32_t fourcc;
    uint32_t generation;
    uint16_t size;
    uint16_t checksum;
    uint16_t dac_octaves[4][DAC_OCTAVES];
};
#define OC_CAL_FOURCC 0x314C4143u     /* 'CAL1' */

static uint16_t oc_cal[DAC_OCTAVES + 1];    /* the current channel's table */
static bool oc_cal_valid;

/* the module's reference: 0 V is the note at kOctaveZero (the T4.0's 3) */
#define OC_OCTAVE_ZERO 3
#define OC_SEMITONE_UNITS 128              /* the pitch's unit: a 128th of a semitone */
#define OC_OCTAVE_UNITS (12 * OC_SEMITONE_UNITS)

static void oc_cal_load(void)
{
    oc_cal_page page;
    EEPROM.get(EEPROM_CAL_BASE, page);
    if (page.fourcc != OC_CAL_FOURCC) {
        oc_cal_valid = false;
        return;
    }
    memcpy(oc_cal, page.dac_octaves[0], sizeof oc_cal);
    oc_cal_valid = true;
}

/* a note as the DAC's code: the o_C's own mapping (its kOctaveZero, its
 * calibration table, the linear interpolation inside an octave) */
static uint32_t oc_note_to_code(uint32_t note)
{
    int32_t pitch = (int32_t)note * OC_SEMITONE_UNITS;
    int32_t octave, fractional;
    uint16_t lo, hi;
    if (!oc_cal_valid)
        return 0;
    pitch += OC_OCTAVE_ZERO * OC_OCTAVE_UNITS;
    if (pitch < 0) pitch = 0;
    if (pitch > DAC_OCTAVES * OC_OCTAVE_UNITS) pitch = DAC_OCTAVES * OC_OCTAVE_UNITS;
    octave = pitch / OC_OCTAVE_UNITS;
    fractional = pitch - octave * OC_OCTAVE_UNITS;
    lo = oc_cal[octave];
    hi = oc_cal[octave + 1 < DAC_OCTAVES + 1 ? octave + 1 : DAC_OCTAVES];
    return (uint32_t)lo + (uint32_t)((int32_t)(fractional * ((int32_t)hi - (int32_t)lo)) / OC_OCTAVE_UNITS);
}

/* the CV out: the note, turned into the DAC's code and written */
void oc_cv_write(uint32_t ch, uint32_t note)
{
    uint32_t code;
    if (ch > 3u)
        return;
    code = oc_note_to_code(note);
    code = DAC_MAX - code;                    /* the DAC's output is inverted */
    SPI.transfer16(DAC_CMD(ch));               /* the command */
    SPI.transfer16((uint16_t)code);            /* the data */
}

/* a digital out: a gate, a trigger or a clock */
static const uint8_t oc_digital_pin[4] = {PIN_TR1, PIN_TR2, PIN_TR3, PIN_TR4};
void oc_gate_write(uint32_t ch, uint32_t on)
{
    if (ch > 3u)
        return;
    digitalWrite(oc_digital_pin[ch], on ? HIGH : LOW);
}

/* ------------------------------------------------------------- the module --- */
static IntervalTimer oc_timer;

/* The module's tick to the engine's samples: the fraction is kept, so the rate
 * is exact whatever the tick's jitter is (60 us is 2.646 samples). */
static uint32_t oc_sample_acc_q16;

static void oc_tick(void)
{
    uint32_t n;
    oc_sample_acc_q16 += (uint32_t)(((uint64_t)oc_sample_rate() * OC_TICK_US << 16) / 1000000u);
    n = oc_sample_acc_q16 >> 16;
    oc_sample_acc_q16 &= 0xFFFFu;
    oc_clock_advance(n);
    oc_out_tick();
}

/* The buttons: the top one starts and stops the transport, the bottom one
 * restarts it from the top. The encoders and the display come with the UI
 * (Phase 4). */
#define BUT_TOP 1u
#define BUT_BOT 2u
#define BUT_L 4u
#define BUT_R 8u
static uint8_t oc_button_prev;

static uint8_t oc_buttons_edges(void)
{
    uint8_t now = 0, edges;
    if (!digitalRead(PIN_BUT_TOP)) now |= BUT_TOP;
    if (!digitalRead(PIN_BUT_BOT)) now |= BUT_BOT;
    if (!digitalRead(PIN_BUT_L)) now |= BUT_L;
    if (!digitalRead(PIN_BUT_R)) now |= BUT_R;
    edges = now & ~oc_button_prev;             /* only the edges */
    oc_button_prev = now;
    return edges;
}

void setup(void)
{
    pinMode(PIN_BUT_TOP, INPUT_PULLUP);
    pinMode(PIN_BUT_BOT, INPUT_PULLUP);
    pinMode(PIN_BUT_L, INPUT_PULLUP);
    pinMode(PIN_BUT_R, INPUT_PULLUP);
    for (uint32_t i = 0; i < 4; i++)
        pinMode(oc_digital_pin[i], OUTPUT);
    digitalWrite(PIN_DAC_RST, HIGH);
    pinMode(PIN_DAC_RST, OUTPUT);

    /* the DAC's chip select is the SPI0's own pin (the o_C remuxes it) */
    IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_00 = 3;
    SPI.begin();
    SPI.beginTransaction(SPISettings(12000000, MSBFIRST, SPI_MODE0));

    /* the DAC's reset: low, then high (the o_C's own sequence) */
    digitalWrite(PIN_DAC_RST, LOW);
    delay(5);
    digitalWrite(PIN_DAC_RST, HIGH);
    delay(5);

    oc_cal_load();
    oc_engine_init();

    oc_timer.begin(oc_tick, OC_TICK_US);
    oc_timer.priority(128);
}

void loop(void)
{
    uint8_t buttons = oc_buttons_edges();
    if (buttons & BUT_TOP)
        oc_transport(oc_playing ? 2u : 1u);    /* stop, or start */
    if (buttons & BUT_BOT)
        oc_transport(3u);                      /* restart from the top */
    if (buttons & (BUT_L | BUT_R))
        ;                                       /* the UI's business (Phase 4) */
}
