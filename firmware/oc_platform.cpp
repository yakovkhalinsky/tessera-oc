/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The o_C port's platform layer: the module itself.
 *
 * A Teensy 4.0 in an o_C module: four CV outs (a DAC8565 over SPI, then an
 * OPA2172 buffer and the module's Vbias), the module's trigger inputs, the
 * encoders and the buttons, the OLED (the UI is Phase 4) and the module's
 * EEPROM, where its calibration lives.
 *
 * The engine (oc_engine.c, C) is driven from the module's 60 us tick, the same
 * rate the o_C's own firmware runs its core at: the tick advances the engine's
 * clock (the sample at FS, exactly) and the engine's notes go out as CV.
 *
 * The pin numbers are the o_C's own (oc-phazerville's OC_gpio.cpp, the T4.0's
 * static pinout; the T4.1's are detected at run time and differ).
 *
 * The CV out's mapping is the o_C's, not ours: 0 V is MIDI 36 (C2), the
 * T4.0's kOctaveZero = 3, an octave up is a volt more, and the DAC's codes
 * come from the module's own calibration (per channel, per octave), so the
 * port's CV outs are interchangeable with the module's.
 *
 * There are no digital outs: the o_C's TR1..TR4 are trigger *inputs* (the
 * schematic has no other digital out, and the o_C's own firmware reads them:
 * oc-phazerville's OC_digital_inputs.cpp). The port reads them and never
 * drives them - driving an input is how a module gets hurt.
 *
 * So the gate rides on the CV: a track's CV carries its pitch while the
 * track's out is asserted (the gate up, the trigger's pulse, the clock's
 * pulse) and rests at the module's 0 V reference otherwise. The rig reads the
 * gate off the CV (a comparator, an envelope follower) and the pitch off the
 * same wire. */

#include <Arduino.h>
#include <SPI.h>
#include <EEPROM.h>
#include <MIDIUSB.h>
#include "oc_port.h"
#include "oc_spi.h"
#include "oc_version.h"

/* The hardware variant: the o_C panel this port is built for. The VOR panel
 * is not the same hardware (its pin 9 is a button, and the DAC's reset wants
 * a pin this port does not have), and a build with no variant at all would
 * take the pins below on faith. The variant is named in platformio.ini. */
#if defined(OC_HW_UOC_VOR) || defined(OC_HW_VOR)
#error "VOR panel unsupported on Teensy 4.0 (pin 9 is a button; DAC RST needs pin 9)"
#elif !defined(OC_HW_UOC_A)
#error "no hardware variant: set -DOC_HW_UOC_A in platformio.ini"
#endif

/* --------------------------------------------------------- the o_C's pins --- */
/* The DAC and the OLED's own pins: the only outputs the module has. */
#define PIN_DAC_CS 10          /* the DAC8565's chip select (SPI0's) */
#define PIN_DAC_RST 9          /* its reset */
#define PIN_TR1 0              /* the trigger inputs: the rig's clock comes in
                                * on the TR1 jack, whose pin is oc_pins.tr1 */
#define PIN_TR2 1
#define PIN_TR3 2
#define PIN_TR4 3

/* The module's input pins: the trigger inputs, the buttons and the encoders.
 * The set is the same either way round (only the roles change with the flip),
 * and the tables that say the roles are in oc_port.h. */
static constexpr uint8_t OC_INPUT_PINS[] = {0, 1, 2, 3, 4, 5, 14, 15, 16, 21, 22, 23};

/* The module's output pins: the OLED's three (6, 7, 8), the DAC's two (9, 10),
 * the SPI's three (11, 12, 13) and the SPI's chip select (37). */
static constexpr uint8_t OC_OUTPUT_PINS[] = OC_OUTPUT_PIN_LIST;

/* The pin check: the module's inputs and its outputs must not meet. A control
 * on a pin the module drives is a control that does not work. */
static constexpr bool oc_pin_sets_disjoint(void)
{
    for (uint8_t a : OC_INPUT_PINS)
        for (uint8_t b : OC_OUTPUT_PINS)
            if (a == b)
                return false;
    return true;
}
static_assert(oc_pin_sets_disjoint(), "an input pin is an output pin");

struct oc_pinout oc_pins = OC_PINOUT_PLAIN;

/* the DAC8565: four channels, 16 bits, the output inverted */
#define DAC_CMD(ch) (0x10u + ((ch) & 3u) * 2u)     /* 0x10, 0x12, 0x14, 0x16 */
#define DAC_MAX 65535u
#define DAC_OCTAVES 11         /* the calibration's table: the octaves 0..10 */
#define DAC_MAX_OCTAVE 10      /* the highest one: the module's range ends there */

/* The DAC8565's frame is 24 bits (the command byte and the 16 data bits), and
 * the chip latches on the rising edge of SCLK - the mode the o_C's own
 * firmware drives it in (oc-phazerville's OC_DAC.cpp: a 24-bit frame, the mode
 * the display on the same bus uses). The mode is one constant, so a module
 * that wants the other one is a one-line change. */
#define OC_DAC_SPI_MODE SPI_MODE0
#if OC_DAC_SPI_MODE == SPI_MODE1
#define OC_DAC_TCR_MODE LPSPI_TCR_CPHA
#else
#define OC_DAC_TCR_MODE 0u
#endif

/* the module's tick: the o_C's own core rate, 60 us */
#define OC_TICK_US 60u

/* ------------------------------------------------- the module's calibration --- */
/* The calibration lives in the module's EEPROM, in the o_C's own page layout
 * (oc-phazerville's util_pagestorage.h and OC_calibration.h): a header (the
 * fourcc 'CAL1', a generation, the size, a checksum) and then the data, whose
 * first part is the DAC's table (four channels of eleven octaves, the DAC's
 * code at the bottom of each octave). The pages lie one after another from
 * EEPROM_CALIBRATIONDATA_START, and the newest one (the highest generation)
 * that checksums is the one to read.
 *
 * On the T4.0 the calibration is 128 bytes (the header and 116 bytes of data)
 * and the EEPROM holds exactly one page of it.
 *
 * The port only reads the calibration: it never writes the EEPROM (the
 * module's own firmware owns the calibration, and a write would wear it). */
#define OC_CAL_BASE 0
#define OC_CAL_PAGE_SIZE 128u
#define OC_CAL_PAGES 1u
#define OC_CAL_DATA_SIZE 116u
#define OC_CAL_FOURCC 0x43414C01u     /* 'CAL1' */

/* The page's own layout (the o_C's CalibrationData, per Phazerville PSv2.0.2
 * OC_calibration.h:136-150): the DAC's table (four channels of eleven
 * octaves, 88 bytes), the ADC's (12 bytes), the display's column offset (a
 * byte), three bytes of padding, and the flags (a word). */
#define OC_CAL_OFF_DISPLAY 100u
#define OC_CAL_OFF_FLAGS 104u

/* the calibration's flags and the display's offset: the module's own */
uint32_t oc_cal_flags;
uint8_t oc_cal_display_offset = 2;    /* the SH1106's RAM is 132 wide */

struct oc_cal_page {
    uint32_t fourcc;
    uint32_t generation;
    uint16_t size;
    uint16_t checksum;
    uint8_t data[OC_CAL_DATA_SIZE];
};

/* the DAC's table, per channel: the module's own, as the calibration has it */
static uint16_t oc_cal_table[4][DAC_OCTAVES + 1];
static bool oc_cal_valid;

/* the module's reference: 0 V is the note at kOctaveZero (the T4.0's 3) */
#define OC_OCTAVE_ZERO 3
#define OC_SEMITONE_UNITS 128              /* the pitch's unit: a 128th of a semitone */
#define OC_OCTAVE_UNITS (12 * OC_SEMITONE_UNITS)
/* the o_C's own gate level (OC_DAC.h's kOctaveGateHigh): the level a CV gate
 * goes to, an octave below the top of the module's range */
#define OC_GATE_OCTAVE 9

/* The same range, mapped straight: the fallback for a module with no
 * calibration (or one the port cannot read). The o_C's own range: the Vbias
 * is asymmetric, so the output spans -3 V to +7 V, MIDI note 0 to 120 - and
 * the pitch is the port's own (0 is MIDI 36, the note at 0 V), so the line
 * runs from the pitch through 0 V, one volt an octave. */
#define OC_CV_MIN_MV (-3000)
#define OC_CV_MAX_MV (7000)
#define OC_GATE_MV 6000                    /* the gate's high level, in millivolts */

static uint32_t oc_mv_to_level(int32_t mv)
{
    if (mv < OC_CV_MIN_MV) mv = OC_CV_MIN_MV;
    if (mv > OC_CV_MAX_MV) mv = OC_CV_MAX_MV;
    return (uint32_t)((uint64_t)(mv - OC_CV_MIN_MV) * 65535u / (uint64_t)(OC_CV_MAX_MV - OC_CV_MIN_MV));
}

static uint32_t oc_note_to_code_linear(int32_t pitch, int32_t mv_per_semitone)
{
    return oc_mv_to_level((pitch * mv_per_semitone) / 128);
}

/* The DAC's channel the engine's channel goes to: a module with the controls
 * mirrored has its DAC channels mirrored too (A<->D, B<->C), and the
 * calibration is per physical channel, so it is read with the physical
 * channel as well. */
static uint32_t oc_dac_ch(uint32_t ch)
{
    return (oc_cal_flags & OC_CAL_FLIPCONTROLS) ? (3u - ch) : ch;
}

/* a pitch as the DAC's level: the o_C's own mapping (its kOctaveZero, its
 * calibration table, the linear interpolation inside an octave). The pitch is
 * in the 128th of a semitone, as the o_C's own MIDI quantizer counts it. */
static uint32_t oc_note_to_level(uint32_t ch, int32_t pitch)
{
    int32_t octave, fractional;
    uint16_t lo, hi;
    if (oc_cv_scale(ch))
        return oc_note_to_code_linear(pitch, 100);  /* 0.1 V an octave (V/oct systems) */
    if (!oc_cal_valid)
        return oc_note_to_code_linear(pitch, 1000); /* 1 V an octave, straight */
    pitch += OC_OCTAVE_ZERO * OC_OCTAVE_UNITS;
    if (pitch < 0) pitch = 0;
    if (pitch > DAC_MAX_OCTAVE * OC_OCTAVE_UNITS) pitch = DAC_MAX_OCTAVE * OC_OCTAVE_UNITS;
    octave = pitch / OC_OCTAVE_UNITS;
    fractional = pitch - octave * OC_OCTAVE_UNITS;
    ch = oc_dac_ch(ch);
    lo = oc_cal_table[ch][octave];
    hi = oc_cal_table[ch][octave + 1];
    return (uint32_t)lo + (uint32_t)((int32_t)(fractional * ((int32_t)hi - (int32_t)lo)) / OC_OCTAVE_UNITS);
}

/* ------------------------------------------------- the module's calibration --- */
/* The page's checksum: the sum of the data's bytes, inverted (the o_C's own
 * page storage: util_pagestorage.h). */
static uint16_t oc_cal_checksum(const uint8_t *data, uint32_t len)
{
    uint16_t c = 0;
    for (uint32_t i = 0; i < len; i++)
        c = (uint16_t)(c + data[i]);
    return (uint16_t)(c ^ 0xFFFFu);
}

/* The calibration: the newest page that checksums, read only. A module with no
 * calibration (or one the port cannot read) keeps the straight line. */
static void oc_cal_load(void)
{
    oc_cal_page page;
    uint32_t best_gen = 0, p, ch, oct;
    bool found = false;

    for (p = 0; p < OC_CAL_PAGES; p++) {
        EEPROM.get(OC_CAL_BASE + p * OC_CAL_PAGE_SIZE, page);
        if (page.fourcc != OC_CAL_FOURCC)
            continue;                               /* not a calibration page */
        if (page.size != OC_CAL_DATA_SIZE)
            continue;                               /* another layout */
        if (page.checksum != oc_cal_checksum(page.data, OC_CAL_DATA_SIZE))
            continue;                               /* a broken page */
        if (found && (int32_t)(page.generation - best_gen) <= 0)
            continue;                               /* an older one */
        best_gen = page.generation;
        /* the DAC's table is the data's first part: four channels of eleven
         * octaves (the o_C's own DAC::CalibrationData) */
        for (ch = 0; ch < 4u; ch++)
            for (oct = 0; oct < DAC_OCTAVES; oct++) {
                uint32_t at = (ch * DAC_OCTAVES + oct) * 2u;
                oc_cal_table[ch][oct] = (uint16_t)(page.data[at] | ((uint16_t)page.data[at + 1] << 8));
            }
        /* the display's offset and the flags (the rest of the page's data) */
        oc_cal_display_offset = page.data[OC_CAL_OFF_DISPLAY];
        oc_cal_flags = 0;
        for (uint32_t i = 0; i < 4u; i++)
            oc_cal_flags |= (uint32_t)page.data[OC_CAL_OFF_FLAGS + i] << (8u * i);
        found = true;
    }
    if (!found) {
        oc_cal_valid = false;
        return;
    }
    /* the table's last entry: the code holds above the top octave */
    for (ch = 0; ch < 4u; ch++)
        oc_cal_table[ch][DAC_OCTAVES] = oc_cal_table[ch][DAC_MAX_OCTAVE];
    oc_cal_valid = true;
}

/* ------------------------------------------------------------- the CV out --- */
/* What a track's digital out carries (the engine's own P_E4): the same three
 * modes the o_C's port had: a gate, a trigger, a clock. The engine's names
 * for them live in the C half (parts/oc_out.c), so they are repeated here. */
enum { OC_OUT_GATE, OC_OUT_TRIG, OC_OUT_CLOCK };

static uint32_t oc_cv_pitch_level[4];    /* the level of the last note */
static uint32_t oc_cv_level[4];          /* the level the DAC has */
static uint32_t oc_cv_rest[4];           /* the 0 V reference, per channel */
static uint32_t oc_cv_high[4];           /* the gate's high level, per channel */
static uint8_t oc_cv_on[4];              /* the track's out is asserted */

/* The DAC and the OLED share the SPI bus, and the DAC is written from the
 * module's tick (the interrupt) while the OLED is written from the main loop:
 * they must never be on the bus at the same time. The lock is a flag and not
 * a wait (the interrupt must never wait for the main loop: it cannot run while
 * the interrupt runs), so the interrupt leaves its write pending when the
 * display is on the bus, and the main loop applies it (oc_cv_service). */
volatile uint8_t oc_spi_busy;
volatile uint8_t oc_spi_pending;

/* the DAC8565's frame: 24 bits, the chip select around it */
static void oc_dac_write(uint32_t ch, uint32_t level)
{
    uint32_t tcr = LPSPI4_TCR;
    LPSPI4_CR = LPSPI4_CR | LPSPI_CR_RRF;   /* the display's leftovers go */
    digitalWriteFast(PIN_DAC_CS, LOW);
    LPSPI4_TCR = ((tcr & 0xF8000000u) & ~(LPSPI_TCR_CPHA | LPSPI_TCR_CPOL)) |
                 LPSPI_TCR_FRAMESZ(23) | OC_DAC_TCR_MODE;
    LPSPI4_TDR = ((uint32_t)DAC_CMD(oc_dac_ch(ch)) << 16) | (uint32_t)(DAC_MAX - level);
    while (LPSPI4_RSR & LPSPI_RSR_RXEMPTY) { }    /* the frame is out */
    (void)LPSPI4_RDR;                             /* and drained */
    LPSPI4_TCR = tcr;
    digitalWriteFast(PIN_DAC_CS, HIGH);
}

/* the level a channel's CV carries now */
static uint32_t oc_cv_want(uint32_t ch)
{
    if (oc_out_mode(ch) == OC_OUT_CLOCK)
        return oc_cv_on[ch] ? oc_cv_high[ch] : oc_cv_rest[ch];
    return oc_cv_on[ch] ? oc_cv_pitch_level[ch] : oc_cv_rest[ch];
}

static void oc_cv_put(uint32_t ch, uint32_t level)
{
    oc_dac_write(ch, level);
    oc_cv_level[ch] = level;
}

/* the level onto the DAC, if the bus is free; otherwise it waits for the main
 * loop (which applies it when the display is done) */
static void oc_cv_commit(uint32_t ch)
{
    uint32_t want = oc_cv_want(ch);
    if (want == oc_cv_level[ch])
        return;
    if (oc_spi_busy) {
        oc_spi_pending = 1;
        return;
    }
    oc_cv_put(ch, want);
}

/* The main loop's: the writes the tick had to leave pending. The interrupts
 * are off for it, so a write from the tick cannot land between the reading of
 * a level and its writing. */
void oc_cv_service(void)
{
    uint32_t ch;
    if (!oc_spi_pending)
        return;
    noInterrupts();
    oc_spi_pending = 0;
    for (ch = 0; ch < 4u; ch++) {
        uint32_t want = oc_cv_want(ch);
        if (want != oc_cv_level[ch])
            oc_cv_put(ch, want);
    }
    interrupts();
}

/* The CV out: the note, turned into the DAC's level and written. */
void oc_cv_write(uint32_t ch, uint32_t pitch)
{
    if (ch > 3u)
        return;
    oc_cv_pitch_level[ch] = oc_note_to_level(ch, (int32_t)pitch);
    oc_cv_commit(ch);
}

/* A digital out: a gate, a trigger or a clock. The module has none, so it
 * rides on the CV: the level goes up (the pitch, or the clock's high level)
 * and comes back down to the 0 V reference. */
void oc_gate_write(uint32_t ch, uint32_t on)
{
    if (ch > 3u)
        return;
    oc_cv_on[ch] = on ? 1u : 0u;
    oc_cv_commit(ch);
}

/* the module's CV outs at rest: the 0 V reference, per channel, and the level
 * the DAC is put to at power-on (the chip comes out of its reset at the
 * midscale). */
static void oc_cv_rest_all(void)
{
    uint32_t ch;
    for (ch = 0; ch < 4u; ch++) {
        if (oc_cal_valid) {
            uint32_t phys = oc_dac_ch(ch);
            oc_cv_rest[ch] = oc_cal_table[phys][OC_OCTAVE_ZERO];
            oc_cv_high[ch] = oc_cal_table[phys][OC_GATE_OCTAVE];
        } else {
            oc_cv_rest[ch] = oc_mv_to_level(0);
            oc_cv_high[ch] = oc_mv_to_level(OC_GATE_MV);
        }
        oc_cv_pitch_level[ch] = oc_cv_rest[ch];
        oc_cv_level[ch] = 0xFFFFFFFFu;     /* not written yet: the first write goes out */
        oc_cv_on[ch] = 0;
        oc_cv_put(ch, oc_cv_rest[ch]);
    }
}

/* ------------------------------------------------------------- the module --- */
static IntervalTimer oc_timer;

/* The clock out: a track whose digital out is set to CLOCK carries the tempo
 * (24 pulses a beat, as MIDI clock does), so the rig can follow the module
 * instead of the other way round. It rides on the CV, like every gate. */
static uint32_t oc_clock_phase;                /* the samples since the last pulse */
static uint32_t oc_clock_pulse_ticks;         /* the ticks the pulse is high for */

static void oc_clock_out(uint32_t samples)
{
    int32_t bpm = oc_tempo();
    uint32_t period = bpm > 0 ? (uint32_t)(((uint64_t)oc_sample_rate() * 60u * 1000u) / ((uint32_t)bpm * 24u)) : 0;
    uint32_t i;
    if (!period)
        return;
    oc_clock_phase += samples;
    if (oc_clock_phase >= period) {
        oc_clock_phase -= period;
        oc_clock_pulse_ticks = 17;                     /* about a millisecond, at the 60 us tick */
        for (i = 0; i < 4u; i++)
            if (oc_out_mode(i) == (uint32_t)OC_OUT_CLOCK)
                oc_gate_write(i, 1);
    }
    if (oc_clock_pulse_ticks) {
        oc_clock_pulse_ticks--;
        if (oc_clock_pulse_ticks == 0)
            for (i = 0; i < 4u; i++)
                if (oc_out_mode(i) == (uint32_t)OC_OUT_CLOCK)
                    oc_gate_write(i, 0);
    }
}

/* The external clock: the rig's clock comes in on the first trigger input, as
 * a pulse train (a patch's clock is not MIDI). The port watches for the pulses
 * and, once they are there, hands them to the engine's clock path - the same
 * one a MIDI clock uses - and follows them.
 *
 * The pulses before the switch are counted, not used: the engine starts on the
 * first start it is sent, and the phase it keeps afterwards is the pulse grid's,
 * so the notes land on the rig's beats. The bar's downbeat may be a beat off
 * the rig's; on a rig that sends a clock, that is the clock's business.
 *
 * The input is active low, as the o_C's own hardware and firmware have it
 * (oc-phazerville's OC_digital_inputs.cpp: the pin is pulled up and a trigger
 * pulls it down, so the edge to watch is the falling one). An unpatched input
 * reads high and stays there. */
#define OC_CLOCK_PULSES_TO_FOLLOW 24u    /* a beat, before the port follows */
static uint8_t oc_clock_level;
static uint32_t oc_clock_pulses;
static uint8_t oc_clock_on;                  /* the engine is following the rig */

uint8_t oc_clock_following(void) { return oc_clock_on; }

static void oc_clock_watch(void)
{
    uint8_t level = digitalReadFast(oc_pins.tr1);
    if (!level && oc_clock_level) {              /* a falling edge: the rig's pulse */
        oc_clock_pulses++;
        if (oc_clock_on)
            oc_clock_pulse();
        else if (oc_clock_pulses >= OC_CLOCK_PULSES_TO_FOLLOW) {
            oc_clock_on = 1;
            oc_clock_source(2);                  /* the engine takes the clock */
            oc_clock_start();                   /* and starts on it */
        }
    }
    oc_clock_level = level;
}

/* The module's tick to the engine's samples: the fraction is kept, so the rate
 * is exact whatever the tick's jitter is (60 us is 2.646 samples). */
static uint32_t oc_sample_acc_q16;

static void oc_tick(void)
{
    uint32_t n;
    oc_sample_acc_q16 += (uint32_t)(((uint64_t)oc_sample_rate() * OC_TICK_US << 16) / 1000000u);
    n = oc_sample_acc_q16 >> 16;
    oc_sample_acc_q16 &= 0xFFFFu;
    oc_clock_watch();
    oc_clock_advance(n);
    oc_out_tick();
    oc_clock_out(n);
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
    if (!digitalRead(oc_pins.but_top)) now |= BUT_TOP;
    if (!digitalRead(oc_pins.but_bot)) now |= BUT_BOT;
    if (!digitalRead(oc_pins.but_l)) now |= BUT_L;
    if (!digitalRead(oc_pins.but_r)) now |= BUT_R;
    edges = now & ~oc_button_prev;             /* only the edges */
    oc_button_prev = now;
    return edges;
}

/* the screen and its controls */
void oc_display_init(void);
void oc_display_splash(void);
void ui_init(void);
void ui_input(void);
void ui_draw(void);
void ui_page_next(int32_t dir);
void ui_button(int32_t which);

/* the safe mode: the bottom button held at power-on. Either of the two
 * buttons that can be the bottom one (pins 4 and 5) is accepted: the flag
 * that says which is which is in the calibration, read after the pins. */
static bool oc_safe_mode;

void setup(void)
{
    /* The diagnostics first: the persistent stage word (what the last run
     * reached) and the cycle counter (the timing). */
    oc_diag_begin();
    oc_stage(10);

    /* The trigger inputs: the o_C's own, read and never driven. They are
     * pulled up (as the o_C's firmware has them) and a trigger pulls the pin
     * down. The buttons and the encoders are inputs too, and they are the
     * same pins either way round: only their roles change with the flip. */
    pinMode(PIN_TR1, INPUT_PULLUP);
    pinMode(PIN_TR2, INPUT_PULLUP);
    pinMode(PIN_TR3, INPUT_PULLUP);
    pinMode(PIN_TR4, INPUT_PULLUP);
    for (uint32_t i = 0; i < sizeof OC_INPUT_PINS / sizeof OC_INPUT_PINS[0]; i++) pinMode(OC_INPUT_PINS[i], INPUT_PULLUP);
    oc_stage(11);

    /* Safe mode: bottom button held at power-on. Cap on the line needs
     * settle time; require ALL samples LOW on the same pin (AND), not any. */
    delay(25);                              /* ≥25 ms settle after INPUT_PULLUP */
    {
        const uint8_t candidates[2] = {4, 5}; /* but_bot either orientation; cal not read yet */
        bool held[2] = {true, true};
        for (int s = 0; s < 3; s++) {       /* ≥3 samples, 10 ms apart */
            for (int c = 0; c < 2; c++)
                if (digitalRead(candidates[c])) held[c] = false; /* HIGH → not held */
            if (s + 1 < 3) delay(10);
        }
        oc_safe_mode = held[0] || held[1];  /* a pin LOW on every sample */
    }

    /* The DAC and the OLED share the SPI bus and must never see each other's
     * bits. So neither of them uses the library's chip select: the library's
     * goes to a spare pin (37), and the DAC's own select (10) and the OLED's
     * (8) are plain pins, driven by hand around each transfer. The library
     * would otherwise hold the DAC's select low for the display's traffic. */
    SPI.begin();
    SPI.setCS(37);
    IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_00 = 5;   /* pin 10 back to a plain pin: the DAC's select */
    pinMode(PIN_DAC_CS, OUTPUT);
    digitalWrite(PIN_DAC_CS, HIGH);
    SPI.beginTransaction(SPISettings(12000000, MSBFIRST, SPI_MODE0));
    oc_stage(12);

    /* The DAC's reset: high only, and no low pulse (the o_C's own firmware
     * does the same: the chip comes out of its reset on its own). */
    digitalWrite(PIN_DAC_RST, HIGH);
    pinMode(PIN_DAC_RST, OUTPUT);
    oc_stage(13);

    /* The USB serial: the crash report and the diagnostics. A module on the
     * other side of a rack has no other way to say what went wrong. */
    Serial.begin(115200);
    Serial.print(F("Tessera-OC "));
    Serial.println(OC_VERSION_STRING);

    /* The calibration: the module's own, read only. It says where the
     * display's columns start and how the module's controls are laid out. */
    oc_cal_load();
    oc_pins = (oc_cal_flags & OC_CAL_FLIPCONTROLS) ? OC_PINOUT_FLIP : OC_PINOUT_PLAIN;
    oc_stage(14);

    /* The CV outs at rest (0 V on all four) before anything else runs. */
    oc_cv_rest_all();

    /* The screen: the crash report first (a module in a reboot loop stops
     * here, and says what happened), then the splash. */
    oc_display_init();
    oc_stage(15);
    oc_diag_crash();
    oc_display_splash();
    oc_stage(16);

    /* The engine, and the storage: not in the safe mode. */
    if (!oc_safe_mode)
        oc_engine_init();
    oc_stage(17);
    if (!oc_safe_mode)
        oc_storage_init();
    oc_stage(18);

    /* The project: the last one, or the power-on. */
    if (!oc_safe_mode && oc_load((uint32_t)oc_last_slot()) != 0)
        oc_power_on();                    /* nothing saved: the module composes */
    oc_stage(19);

    ui_init();                           /* the screen's own drawing takes over the splash */
    oc_stage(20);

    /* The USB MIDI: not in the safe mode. */
    if (!oc_safe_mode)
        usbMIDI.begin();
    oc_stage(21);

    /* The tick: last of all, and at the priority the o_C's own firmware uses
     * (OC_config.h:29). */
    if (!oc_safe_mode) {
        oc_timer.begin(oc_tick, OC_TICK_US);
        oc_timer.priority(80);
    }
    oc_stage(22);
    oc_stage(23);
}

void loop(void)
{
    static uint32_t last_draw;
    static uint32_t loops;
    uint8_t buttons;
    bool diag = loops < 100u;               /* the first 100 iterations are
                                             * reported, step by step */

    /* The safe mode: the screen says what the module knows, and nothing
     * else runs (no engine, no MIDI, no tick). */
    if (oc_safe_mode) {
        if (millis() - last_draw >= 250u) { last_draw = millis(); oc_diag_safe(); }
        return;
    }

    buttons = oc_buttons_edges();
    if (buttons & BUT_TOP)
        oc_transport(oc_playing() ? 2u : 1u);    /* stop, or start */
    if (buttons & BUT_BOT)
        oc_transport(3u);                      /* restart from the top */

    /* the module's USB MIDI: the keys, the controllers and the clock. The
     * packet is the USB-MIDI one - the cable, the status, the two data bytes -
     * which is the shape the engine's MIDI in reads. */
    while (usbMIDI.read()) {
        uint8_t status = usbMIDI.getType();
        uint32_t pkt = (uint32_t)usbMIDI.getCable() | (uint32_t)status << 8;
        if (status < 0xF0u)
            pkt |= (uint32_t)usbMIDI.getData1() << 16 | (uint32_t)usbMIDI.getData2() << 24;
        oc_midi_in(pkt);
    }

    ui_input();
    if (millis() - last_draw >= 100u) {          /* ten screens a second */
        last_draw = millis();
        ui_draw();
    }
    oc_cv_service();                             /* the DAC's pending writes */

    if (diag) { loops++; oc_diag_loop(); oc_stage(99); }   /* the module runs */
}
