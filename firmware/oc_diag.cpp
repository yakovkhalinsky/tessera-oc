/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The port's diagnostics: the stage word, the crash report on the screen and
 * the safe mode.
 *
 * A module that faults on a rack tells you nothing: there is no serial cable
 * on it and no log. So the module says what happened to itself, on its own
 * screen, and it says it across a reset.
 *
 * The stage word is that memory: a small struct in its own section at the end
 * of the AXI RAM (.oc_persist, 0x2027FF40 - the start of the AXI RAM is wiped
 * by the boot ROM, and the DTCM's .bss is cleared by the startup), so the stage
 * the module reached when it died is still there when it comes back. Every
 * write flushes the cache (the AXI RAM is write-back cached) and is mirrored to
 * the framework's crash breadcrumbs.
 *
 * The stages are the boot's, in order: 10 setup, 11 the pins, 12 the SPI,
 * 13 the DAC, 14 the calibration, 15 the display, 16 the splash, 17 the
 * engine, 18 the storage, 19 the project, 20 the UI, 21 the USB MIDI, 22 the
 * timer, 23 done; and 99 is the module running.
 *
 * The crash report is the framework's own, captured into a buffer of our own
 * (the framework's print clears the report as it prints it) and shown a page
 * at a time; the safe mode is a button held at power-on: the calibration, the
 * DAC at 0 V and the display, and nothing else.
 *
 * The watchdog is not here: it is not cross-checked against the o_C's own
 * firmware yet, and it goes in v0.1.3. */

#include <Arduino.h>
#include "oc_port.h"
#include "oc_version.h"

/* the display (the platform's side) */
void oc_display_clear(void);
void oc_display_text(int32_t x, int32_t y, const char *s);
void oc_display_flush(void);

/* ------------------------------------------------------- the stage word --- */

/* The persistent word. It lives in its own section, .oc_persist, at the end of
 * the AXI RAM (0x2027FF40) - just below the framework's fault info, which the
 * startup writes at 0x2027FF80 (cores/teensy4/startup.c:635). The start of the
 * AXI RAM is not safe across a reset (the boot ROM and the HAB may wipe it) and
 * the DTCM's .bss is cleared by the startup; the end of the AXI RAM is neither,
 * which is why the framework keeps its fault info there. */
struct oc_persist_t { uint32_t magic, build, stage, prev_stage, loop_n, blk_max_cyc, boots, sum; };

__attribute__((section(".oc_persist"), used))
volatile struct oc_persist_t oc_persist;

/* Was the word there before this boot? A cold boot (the RAM's contents are
 * whatever they were) is not a reset in the middle of a run, and it must not
 * be reported as one. */
static bool oc_persist_warm;

#define OC_PERSIST_MAGIC 0x32434F54u    /* 'TOC2' */
#define OC_PERSIST_WORDS (sizeof(struct oc_persist_t) / 4)

/* the build's own word: the version and the time of the build. A different
 * build starts a new word (the stages of two builds are not comparable). */
static uint32_t oc_build_word(void)
{
    static const char stamp[] = OC_VERSION_STRING " " __DATE__ " " __TIME__;
    uint32_t c = 2166136261u;                 /* FNV */
    for (const char *p = stamp; *p; p++) c = (c ^ (uint8_t)*p) * 16777619u;
    return c;
}

/* the sum of the word's words (all but the sum itself) */
static uint32_t oc_persist_sum(void)
{
    const volatile uint32_t *w = &oc_persist.magic;
    uint32_t s = 0;
    for (uint32_t i = 0; i + 1 < OC_PERSIST_WORDS; i++) s += w[i];
    return s;
}

/* the word is written and flushed: the AXI RAM is write-back cached, and
 * without the flush the word would be lost at the reset */
static void oc_persist_save(void)
{
    oc_persist.sum = oc_persist_sum();
    arm_dcache_flush((void *)&oc_persist, sizeof oc_persist);
}

/* the stage names, for the screens */
static const char *oc_stage_name(uint32_t n)
{
    static const char *const names[] = {"setup", "pins", "spi", "dac", "cal", "display", "splash", "engine", "storage", "project", "ui", "midi", "timer", "done"};
    if (n >= 10u && n <= 23u) return names[n - 10u];
    return n == 99u ? "running" : "?";
}

/* the cycles in microseconds (the CPU's clock) */
static uint32_t oc_us(uint32_t cycles)
{
    return (uint32_t)(((uint64_t)cycles * 1000u) / (F_CPU / 1000u));
}

/* ---------------------------------------------------------- the timing --- */

/* The cycle counter: the DWT's. The framework's headers do not carry the
 * CMSIS ones (core_cm7.h needs cmsis_version.h, which is not in the include
 * path), so the two registers are named here. */
#define OC_DWT_CTRL (*(volatile uint32_t *)0xE0001000u)    /* CYCCNTENA is bit 0 */
#define OC_DWT_CYCCNT (*(volatile uint32_t *)0xE0001004u)

uint32_t oc_diag_cyccnt(void) { return OC_DWT_CYCCNT; }

/* the engine's block: the longest one */
void oc_diag_block(uint32_t cycles)
{
    if (cycles > oc_persist.blk_max_cyc) { oc_persist.blk_max_cyc = cycles; oc_persist_save(); }
}

/* the loop's own count */
void oc_diag_loop(void) { oc_persist.loop_n++; }

/* the stage: set, summed, flushed, and mirrored to the breadcrumbs (which
 * the crash report prints) */
void oc_stage(uint32_t n)
{
    oc_persist.stage = n;
    oc_persist_save();
    CrashReport.breadcrumb(1, n);
    CrashReport.breadcrumb(2, oc_persist.loop_n);
}

/* the boot: the word is read (or made), the boots are counted, and the cycle
 * counter is started */
void oc_diag_begin(void)
{
    volatile uint32_t *w = (volatile uint32_t *)&oc_persist;
    *(volatile uint32_t *)0xE0001FB0 = 0xC5ACCE55u;     /* the DWT's lock */
    *(volatile uint32_t *)0xE000EDFC |= (1u << 24);    /* the trace */    OC_DWT_CYCCNT = 0;
    OC_DWT_CTRL |= 1u;                                 /* the counter */
    if (oc_persist.magic != OC_PERSIST_MAGIC || oc_persist.build != oc_build_word() ||
        oc_persist.sum != oc_persist_sum()) {
        for (uint32_t i = 0; i < OC_PERSIST_WORDS; i++) w[i] = 0;
        oc_persist.magic = OC_PERSIST_MAGIC;
        oc_persist.build = oc_build_word();
        oc_persist_warm = false;              /* a cold boot: nothing to report */
    } else {
        oc_persist.prev_stage = oc_persist.stage;
        oc_persist_warm = true;               /* the word was there: a warm boot */
    }
    oc_persist.boots++;
    oc_persist.stage = 0;
    oc_persist_save();
}

/* --------------------------------------------------------- the screens --- */

/* the text's lines from the first, drawn from y0 (eight pixels a line) */
static void oc_lines(const char *text, uint32_t first, int32_t y0)
{
    char line[24];
    for (uint32_t r = 0; r < 7u; r++) {
        uint32_t i = 0, n = 0, o = 0;
        while (text[i] && n < first + r) if (text[i++] == '\n') n++;
        while (text[i] && text[i] != '\n' && o + 1u < sizeof line) { char c = text[i++]; line[o++] = (c == '\r') ? ' ' : c; }
        line[o] = 0;
        if (!line[0]) break;
        oc_display_text(0, y0 + (int32_t)r * 8, line);
    }
}

/* the whole screen: the text's lines */
static void oc_diag_screen(const char *text)
{
    oc_display_clear();
    oc_lines(text, 0, 0);
    oc_display_flush();
}

/* ------------------------------------------------------- the crash report --- */

static char crash_text[1024];
static uint32_t crash_len;

/* the report's text, into a buffer of our own: the framework's print clears
 * the report as it prints it */
class OcTextBuf : public Print {
public:
    size_t write(uint8_t c) override {
        if (crash_len + 1u < sizeof crash_text) crash_text[crash_len++] = (char)c;
        return 1; }
};

/* The crash report on the screen: the framework's own text, a page at a time.
 * The top button is the next page and the bottom one goes on. There is no
 * timeout on purpose: a module in a reboot loop must stop on a screen that
 * says what happened. */
void oc_diag_crash(void)
{
    OcTextBuf buf;
    char page[1200], head[24];
    uint32_t lines = 0, page_no = 0, pages;

    if (!CrashReport) {
        /* No crash report: but the last run did not get to the end (a reset in
         * the middle of the boot), so the screen says where it stopped. Only
         * a warm boot has a last run to speak of: on a cold boot the word was
         * made here and there is nothing to report. The stages that say the
         * run got to the end (23 done, 99 running) are not a reset either. */
        uint32_t ps = oc_persist.prev_stage;
        if (oc_persist_warm && ps != 0u && ps != 23u && ps != 99u) {
            snprintf(page, sizeof page, "RESET AT STAGE %u", (unsigned)ps);
            oc_diag_screen(page);
            delay(5000);
        }
        return;
    }

    crash_len = 0;
    CrashReport.printTo(buf);
    crash_text[crash_len] = 0;
    snprintf(page, sizeof page, "PREV STAGE %u %s\n%s", (unsigned)oc_persist.prev_stage,
             oc_stage_name(oc_persist.prev_stage), crash_text);
    for (uint32_t i = 0; page[i]; i++) if (page[i] == '\n') lines++;

    pages = (lines + 6u) / 7u;
    for (;;) {
        oc_display_clear();
        snprintf(head, sizeof head, "CRASH p/%u", (unsigned)(page_no + 1u));
        oc_display_text(0, 0, head);
        oc_lines(page, page_no * 7u, 8);
        oc_display_flush();
        /* the top button is the next page, the bottom one goes on. It waits
         * for the release, so a held button counts once. */
        int b = 0;
        while (!b) { b = !digitalRead(oc_pins.but_top) ? 1 : (!digitalRead(oc_pins.but_bot) ? 2 : 0); if (!b) delay(10); }
        delay(30);
        while (digitalRead(b == 1 ? oc_pins.but_top : oc_pins.but_bot)) delay(10);
        if (b != 1) break;                 /* the bottom button: go on */
        page_no = (page_no + 1u) % pages;
    }
    CrashReport.clear();
}

/* ------------------------------------------------------- the safe mode --- */

/* the safe mode's screen: what the module knows, and nothing else */
void oc_diag_safe(void)
{
    char text[192];
    snprintf(text, sizeof text, "SAFE MODE\nVER %s\nPREV STG %u %s\nBOOTS %u\nBLK %u US\nSTORAGE %s",
             OC_VERSION_STRING, (unsigned)oc_persist.prev_stage, oc_stage_name(oc_persist.prev_stage),
             (unsigned)oc_persist.boots, (unsigned)oc_us(oc_persist.blk_max_cyc),
             oc_storage_ok() ? "ON (RO)" : "OFF (RO)");
    oc_diag_screen(text);
}
