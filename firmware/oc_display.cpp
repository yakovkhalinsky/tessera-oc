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
 * The face is Tessera's own 5 x 7 font (the engine's mono_font.h), drawn a
 * pixel a bit: the module's screen is a third of the FM-1's area.
 *
 * The init sequence is the one the o_C's own firmware uses (u8glib's SSD1306
 * sequence, which the SH1106 takes), and the pages are sent the same way: the
 * column address is 2, because the SH1106's RAM is 132 wide. */

#include <Arduino.h>
#include <SPI.h>
#include <stdint.h>
#include "oc_port.h"
#include "mono_font.h"     /* Tessera's 5 x 7 face (the engine's) */

#define OLED_DC 6
#define OLED_RST 7
#define OLED_CS 8

#define DISPLAY_W 128
#define DISPLAY_H 64
#define DISPLAY_PAGES (DISPLAY_H / 8)

static uint8_t display_fb[DISPLAY_W * DISPLAY_PAGES];

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

    for (uint32_t i = 0; i < sizeof OLED_INIT; i++)
        oled_command(OLED_INIT[i]);
    memset(display_fb, 0, sizeof display_fb);
    oc_display_flush();
}

void oc_display_clear(void)
{
    memset(display_fb, 0, sizeof display_fb);
}

void oc_display_pixel(int32_t x, int32_t y, uint8_t on)
{
    uint8_t *p;
    if (x < 0 || x >= DISPLAY_W || y < 0 || y >= DISPLAY_H)
        return;
    p = &display_fb[(y / 8) * DISPLAY_W + x];
    if (on)
        *p |= (uint8_t)(1u << (y & 7));
    else
        *p &= (uint8_t)~(1u << (y & 7));
}

void oc_display_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t on)
{
    for (int32_t i = 0; i < w; i++) {
        oc_display_pixel(x + i, y, on);
        oc_display_pixel(x + i, y + h - 1, on);
    }
    for (int32_t i = 0; i < h; i++) {
        oc_display_pixel(x, y + i, on);
        oc_display_pixel(x + w - 1, y + i, on);
    }
}

/* a line of text: Tessera's 5 x 7 face, six pixels a glyph, eight pixels a
 * line. A lower-case letter is drawn as its capital, as the FM-1's Mono look
 * does. */
void oc_display_text(int32_t x, int32_t y, const char *s)
{
    for (; *s; s++) {
        uint8_t ch = (uint8_t)*s;
        int32_t col;
        if (ch >= 'a' && ch <= 'z')
            ch = (uint8_t)(ch - 32);
        if (ch < 32u || ch > 122u) {
            x += 6;
            continue;
        }
        for (int32_t r = 0; r < 7; r++)
            for (col = 0; col < 5; col++)
                if (MO_FONT[ch - 32u][r] & (16u >> col))
                    oc_display_pixel(x + col, y + r, 1);
        x += 6;
    }
}

/* the screen to the panel: a page at a time, the column address at 2 */
void oc_display_flush(void)
{
    for (uint32_t page = 0; page < DISPLAY_PAGES; page++) {
        digitalWriteFast(OLED_DC, LOW);
        oled_select();
        SPI.transfer(0x10);            /* the column's high bits */
        SPI.transfer(0x02);            /* the column's low bits: 2 (the SH1106's RAM is 132 wide) */
        SPI.transfer(0xB0 | page);     /* the page */
        digitalWriteFast(OLED_DC, HIGH);
        for (uint32_t x = 0; x < DISPLAY_W; x++)
            SPI.transfer(display_fb[page * DISPLAY_W + x]);
        oled_release();
    }
}
