/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The module's display: an SH1106, 128 x 64, one bit a pixel, eight pages of
 * sixteen bytes, over the SPI the DAC shares.
 *
 * The two devices take turns on the bus and never see each other's bits: the
 * DAC's chip select is a plain pin (the library's own is on a spare one, so it
 * is never asserted) and the OLED's is a plain pin too, driven by hand around
 * each transfer.
 *
 * This file owns the panel's pixels and the bus. The drawing is the kit's
 * (parts/oc_ui_gfx.c, UI-PLAN §4.4): the pixels are the panel's own layout,
 * so the kit draws into this buffer as it is and nothing is converted. The
 * thin wrappers below are for the callers that draw before the UI exists (the
 * crash pager, the safe mode, the splash).
 *
 * The init sequence is the one the o_C's own firmware uses (u8glib's SSD1306
 * sequence, which the SH1106 takes), and the pages are sent the same way: the
 * column address is 2, because the SH1106's RAM is 132 wide. */

#include <Arduino.h>
#include <SPI.h>
#include <stdint.h>
#include <string.h>
#include "oc_port.h"
#include "oc_spi.h"
#include "oc_version.h"
#include "../parts/oc_ui_gfx.h"

#define OLED_DC 6
#define OLED_RST 7
#define OLED_CS 8

/* the panel's pixels: eight pages of 128 columns, one bit a pixel */
static uint8_t display_fb[UI_FB_SIZE];

/* the drawing context: the pixels, and the clip they are drawn through.
 * The clip is always the whole panel here: the crash pager and the safe
 * mode draw with the plain text of the 5 x 7 face, and they must come out
 * byte for byte as they did before the kit existed. */
static oc_gfx_t display_gfx;

/* the init sequence (the o_C's own: oc-phazerville's SH1106 driver, from
 * u8glib's SSD1306 sequence) */
static const uint8_t OLED_INIT[] = {
    0xae, 0xd5, 0x80, 0xa8, 0x3f, 0xd3, 0x00, 0x40, 0x8d, 0x14, 0x20, 0x00,
    0xa1, 0xc8, 0xda, 0x12, 0x81, 0xcf, 0xd9, 0xf1, 0xdb, 0x40, 0x2e, 0xa4,
    0xa6, 0xaf,
};

void oc_display_flush(void);

static void oled_select(void) { digitalWriteFast(OLED_CS, LOW); }
static void oled_release(void) { digitalWriteFast(OLED_CS, HIGH); }

static void oled_command(uint8_t c)
{
    digitalWriteFast(OLED_DC, LOW);
    oled_select();
    SPI.transfer(c);
    oled_release();
}

static void oled_data(const uint8_t *d, uint32_t n)
{
    digitalWriteFast(OLED_DC, HIGH);
    oled_select();
    for (uint32_t i = 0; i < n; i++)
        SPI.transfer(d[i]);
    oled_release();
}

void oc_display_init(void)
{
    /* the kit's context first, so the buffer is never a null pointer: the
     * crash pager and the safe mode draw through the wrappers below, and
     * they must not fault if the bus is the thing that failed */
    oc_gfx_init(&display_gfx, display_fb);

    pinMode(OLED_CS, OUTPUT);
    pinMode(OLED_RST, OUTPUT);
    pinMode(OLED_DC, OUTPUT);
    oled_release();

    digitalWriteFast(OLED_RST, HIGH);
    delay(1);
    digitalWriteFast(OLED_RST, LOW);
    delay(10);
    digitalWriteFast(OLED_RST, HIGH);
    delay(1);

    /* the init sequence, on the bus alone (the tick is not running yet, but
     * the lock is the rule: the DAC never writes over the display). The
     * segment and the COM are the module's own: a screen that is the other
     * way round (the calibration's FLIPSCREEN flag) takes 0xa0 and 0xc0. */
    oc_spi_lock();
    for (uint32_t i = 0; i < sizeof OLED_INIT; i++) {
        uint8_t c = OLED_INIT[i];
        if (oc_cal_flags & OC_CAL_FLIPSCREEN)
            c = (c == 0xa1) ? 0xa0 : (c == 0xc8) ? 0xc0 : c;    /* the segment, the COM */
        oled_command(c);
    }
    oc_spi_unlock();
    oc_gfx_clear(&display_gfx);
    oc_display_flush();
}

/* the panel's pixels: the kit draws into them */
uint8_t *oc_display_fb(void)
{
    return display_fb;
}

/* the thin wrappers, for the callers that draw before the UI exists */
void oc_display_clear(void)
{
    oc_gfx_clear(&display_gfx);
}

void oc_display_text(int32_t x, int32_t y, const char *s)
{
    oc_gfx_text5x7(&display_gfx, (int16_t)x, (int16_t)y, s);
}

/* the screen to the panel: a page at a time, the column address at 2
 *
 * The bus is the DAC's too, and the DAC is written from the module's tick
 * (the interrupt): so the flush takes the bus for as long as it is on it, and
 * the tick's DAC write waits for the main loop to be done (oc_cv_service). */
void oc_display_flush(void)
{
    /* the display's own settings: eight bits a byte, the mode the SH1106
     * wants. The DAC leaves the bus as it found it, but the flush sets it
     * anyway: the two devices share the peripheral. The bus is taken before
     * the settings, so the tick's DAC write cannot slip in between them. */
    oc_spi_lock();
    SPI.beginTransaction(SPISettings(12000000, MSBFIRST, SPI_MODE0));
    for (uint32_t page = 0; page < UI_FB_PAGES; page++) {
        digitalWriteFast(OLED_DC, LOW);
        oled_select();
        SPI.transfer(0x10);            /* the column's high bits */
        SPI.transfer(oc_cal_display_offset);  /* the column's low bits: the
                                                * calibration's (the SH1106's
                                                * RAM is 132 wide) */
        SPI.transfer(0xB0 | page);     /* the page */
        digitalWriteFast(OLED_DC, HIGH);
        for (uint32_t x = 0; x < UI_FB_W; x++)
            SPI.transfer(display_fb[page * UI_FB_W + x]);
        oled_release();
    }
    SPI.endTransaction();
    oc_spi_unlock();
}

/* the splash: the module's name and the firmware's version, so a module that
 * boots says so (and which firmware it runs) before the engine's own screen
 * takes over. */
void oc_display_splash(void)
{
    oc_gfx_clear(&display_gfx);
    oc_gfx_text5x7(&display_gfx, (UI_FB_W - 10 * UI_CH_W) / 2, 20, "TESSERA-OC");
    oc_gfx_text5x7(&display_gfx, (UI_FB_W - (int32_t)strlen(OC_VERSION_STRING) * UI_CH_W) / 2, 36, OC_VERSION_STRING);
    oc_display_flush();
}
