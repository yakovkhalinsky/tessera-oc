/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The host's Arduino, for tools/render_screens: just enough of the Teensy's
 * framework for the screen code (firmware/oc_display.cpp, oc_ui.cpp,
 * oc_diag.cpp) to build and run on the host. Nothing here touches hardware:
 * the pins are a table, the delays return at once, and the SPI bytes go to
 * the renderer (SPI.h). */
#ifndef RS_ARDUINO_H_
#define RS_ARDUINO_H_

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <initializer_list>   /* the framework has it: a range-for over a brace list */

#define F_CPU 600000000u
#define LOW 0
#define HIGH 1
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

/* the pins: what a pin reads is the renderer's (rs_main.cpp) */
void rs_pin_write(uint8_t pin, uint8_t level);
uint8_t rs_pin_read(uint8_t pin);

static inline void pinMode(uint8_t pin, uint8_t mode) { (void)pin; (void)mode; }
static inline void digitalWriteFast(uint8_t pin, uint8_t v) { rs_pin_write(pin, v); }
static inline void digitalWrite(uint8_t pin, uint8_t v) { rs_pin_write(pin, v); }
static inline uint8_t digitalRead(uint8_t pin) { return rs_pin_read(pin); }
static inline uint8_t digitalReadFast(uint8_t pin) { return rs_pin_read(pin); }
static inline void delay(uint32_t ms) { (void)ms; }
static inline uint32_t millis(void) { return 0; }
static inline void arm_dcache_flush(void *p, uint32_t n) { (void)p; (void)n; }

/* Print, as the framework has it: the crash report prints into one */
class Print {
public:
    virtual size_t write(uint8_t c) = 0;
    virtual ~Print() {}
};

/* the USB serial: never connected on the host */
struct RsSerial {
    explicit operator bool() const { return false; }
    size_t write(const char *s, size_t n) { (void)s; return n; }
};
extern RsSerial Serial;

/* the framework's crash report: the renderer says whether there is one and
 * what it says */
struct RsCrashReport {
    explicit operator bool() const;
    void printTo(Print &p) const;
    void clear();
    void breadcrumb(unsigned n, unsigned v) { (void)n; (void)v; }
};
extern RsCrashReport CrashReport;

#endif
