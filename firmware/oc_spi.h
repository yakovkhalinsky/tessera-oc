/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The SPI bus the DAC and the OLED share.
 *
 * The DAC is written from the module's tick (the timer interrupt), the OLED
 * from the main loop: they must never be on the bus at the same time, or the
 * display shows the DAC's bits and the DAC gets the display's.
 *
 * The lock is a flag and not a wait: the interrupt must never wait for the
 * main loop (the main loop cannot run while the interrupt runs - it would
 * deadlock). So the interrupt takes the bus when it is free and otherwise
 * leaves its write pending; the main loop applies what is pending when it is
 * done with the display (oc_cv_service, in oc_platform.cpp). */
#ifndef OC_SPI_H_
#define OC_SPI_H_

#include <stdint.h>

/* the display's transfer is on the bus */
extern volatile uint8_t oc_spi_busy;

/* the interrupt's write is waiting for the bus */
extern volatile uint8_t oc_spi_pending;

static inline void oc_spi_lock(void) { oc_spi_busy = 1; }
static inline void oc_spi_unlock(void) { oc_spi_busy = 0; }

#endif
