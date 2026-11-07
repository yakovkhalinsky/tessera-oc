/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The host's SPI, for tools/render_screens: every byte the display code
 * sends goes to the renderer, which keeps the panel's RAM the way the SH1106
 * does (with D/C low the bytes set the page and the column, with D/C high
 * they fill it). */
#ifndef RS_SPI_H_
#define RS_SPI_H_

#include <stdint.h>

#define MSBFIRST 1
#define SPI_MODE0 0

struct SPISettings {
    SPISettings(uint32_t hz, uint8_t order, uint8_t mode) { (void)hz; (void)order; (void)mode; }
};

void rs_spi_byte(uint8_t b);

struct RsSPI {
    void begin() {}
    void setCS(uint8_t pin) { (void)pin; }
    void beginTransaction(const SPISettings &s) { (void)s; }
    void endTransaction() {}
    uint8_t transfer(uint8_t b) { rs_spi_byte(b); return 0; }
};
extern RsSPI SPI;

#endif
