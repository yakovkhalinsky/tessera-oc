/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The port's diagnostics: the stage word, the timing, the crash report on the
 * screen, the safe mode and the watchdog.
 *
 * A module that faults on a rack tells you nothing: there is no serial cable
 * on it and no log. So the module has to say what happened to itself, on its
 * own screen, and it has to say it across a reset.
 *
 * The stage word is that memory. It is a small struct in the AXI RAM (RAM2,
 * the framework's .bss.dma), which the startup does not clear, so it survives
 * the reset: the stage the module reached when it died is still there when it
 * comes back. Each stage write flushes the cache (the AXI RAM is write-back
 * cached, so without the flush the word would be lost at the reset), and the
 * stage is mirrored to the framework's crash breadcrumbs, which the crash
 * report prints.
 *
 * The stages are the boot's, in order: 10 setup, 11 the pins, 12 the SPI, 13
 * the DAC, 14 the calibration, 15 the display, 16 the splash, 17 the engine,
 * 18 the storage, 19 the project, 20 the UI, 21 the USB MIDI, 22 the timer,
 * 23 the setup is done. 30..34 are the loop's own steps (the buttons, the
 * MIDI, the UI's input, the UI's draw, the CV's service) for the first 100
 * iterations, 50 is the tick's first entry, and 99 is the module running.
 *
 * The timing is the DWT's cycle counter: the longest tick and the longest
 * block, in microseconds, on the crash screen and in the safe mode.
 *
 * The crash report goes on the screen: the framework's report is captured
 * into a buffer of our own (the framework's print clears the report as it
 * prints it, so the registers are read first), shown a page at a time, and
 * waited for: a module in a reboot loop stops on a screen that says what
 * happened, and the bottom button lets it go on.
 *
 * The safe mode is the bottom button held at power-on: the calibration, the
 * DAC at 0 V and the display, and nothing else - no storage, no engine, no
 * timer, no MIDI. It is for a module that does not boot: the screen says
 * what it knows and the module waits.
 *
 * The watchdog is armed at the end of the setup and fed in the loop: a module
 * that hangs resets itself instead of sitting there. */

#include <Arduino.h>
#include "oc_port.h"
#include "oc_version.h"

/* the display (the platform's side) */
void oc_display_clear(void);
void oc_display_text(int32_t x, int32_t y, const char *s);
void oc_display_flush(void);

/* ------------------------------------------------------- the stage word --- */

/* The persistent word. It lives in RAM2 (.bss.dma, the AXI bus), which the
 * startup does not clear - the DTCM's .bss is cleared, this is not. */
struct oc_persist_t {
    uint32_t magic, build, stage, prev_stage, isr_n, loop_n, isr_max_cyc, block_max_cyc, boots, sum;
};

DMAMEM volatile struct oc_persist_t oc_persist;

#define OC_PERSIST_MAGIC 0x32434F54u    /* 'TOC2' */
#define OC_PERSIST_WORDS (sizeof(struct oc_persist_t) / 4)

/* the build's own word: the version and the time of the build. A different
 * build starts a new word (the stages of two builds are not comparable). */
static uint32_t oc_build_word(void)
{
    static const char stamp[] = OC_VERSION_STRING " " __DATE__ " " __TIME__;
    uint32_t c = 0xFFFFFFFFu;
    for (const char *p = stamp; *p; p++) {
        c ^= (uint8_t)*p;
        for (uint32_t i = 0; i < 8; i++)
            c = (c >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(c & 1u)));
    }
    return ~c;
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
    static const char *const names[] = {"setup", "pins", "spi", "dac", "cal", "display", "splash",
                                        "engine", "storage", "project", "ui", "midi", "timer", "done"};
    if (n >= 10u && n <= 23u) return names[n - 10u];
    if (n >= 30u && n <= 34u) return "loop";
    if (n == 50u) return "isr";
    if (n == 99u) return "running";
    return "?";
}

/* the cycles in microseconds (the CPU's clock) */
static uint32_t oc_us(uint32_t cycles)
{
    return (uint32_t)(((uint64_t)cycles * 1000u) / (F_CPU / 1000u));
}

/* ---------------------------------------------------------- the timing --- */

/* The cycle counter: the DWT's. The framework's headers do not carry the
 * CMSIS ones (core_cm7.h needs cmsis_version.h, which is not in the include
 * path), so the three registers are named here. */
#define OC_DWT_CTRL (*(volatile uint32_t *)0xE0001000u)    /* CYCCNTENA is bit 0 */
#define OC_DWT_CYCCNT (*(volatile uint32_t *)0xE0001004u)
#define OC_DEMCR (*(volatile uint32_t *)0xE000EDFCu)       /* TRCENA is bit 24 */
#define OC_DWT_LAR (*(volatile uint32_t *)0xE0001FB0u)

uint32_t oc_diag_cyccnt(void) { return OC_DWT_CYCCNT; }

/* the tick: the count and the longest tick */
void oc_diag_isr(uint32_t cycles)
{
    if (oc_persist.isr_n == 0u) oc_stage(50);      /* the tick's first entry */
    oc_persist.isr_n++;
    if (cycles > oc_persist.isr_max_cyc) oc_persist.isr_max_cyc = cycles;
    if ((oc_persist.isr_n & 63u) == 0u) {
        oc_persist_save();
        if (oc_persist.isr_n >= 1024u) oc_stage(99);       /* it is running */
    }
}

/* the engine's block: the longest one */
void oc_diag_block(uint32_t cycles)
{
    if (cycles > oc_persist.block_max_cyc) { oc_persist.block_max_cyc = cycles; oc_persist_save(); }
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
    CrashReport.breadcrumb(2, oc_persist.isr_n);
    CrashReport.breadcrumb(3, oc_persist.loop_n);
}

/* the boot: the word is read (or made), the boots are counted, and the cycle
 * counter is started */
void oc_diag_begin(void)
{
    volatile uint32_t *w = (volatile uint32_t *)&oc_persist;
    *(volatile uint32_t *)0xE0001FB0 = 0xC5ACCE55u;     /* the DWT's lock */
    *(volatile uint32_t *)0xE000EDFC |= (1u << 24);    /* the trace */
    OC_DWT_CYCCNT = 0;
    OC_DWT_CTRL |= 1u;                                 /* the counter */
    if (oc_persist.magic != OC_PERSIST_MAGIC || oc_persist.build != oc_build_word() ||
        oc_persist.sum != oc_persist_sum()) {
        for (uint32_t i = 0; i < OC_PERSIST_WORDS; i++) w[i] = 0;
        oc_persist.magic = OC_PERSIST_MAGIC;
        oc_persist.build = oc_build_word();
    } else {
        oc_persist.prev_stage = oc_persist.stage;
    }
    oc_persist.boots++;
    oc_persist.stage = 0;
    oc_persist_save();
}

/* --------------------------------------------------------- the screens --- */

/* the n-th line of a text (the lines are separated by newlines) */
static void oc_text_line(const char *text, uint32_t n, char *out, uint32_t max)
{
    uint32_t i = 0, line = 0, o = 0;
    out[0] = 0;
    while (text[i] && line < n) { if (text[i++] == '\n') line++; }
    while (text[i] && text[i] != '\n' && o + 1u < max) {
        char c = text[i++];
        out[o++] = (c == '\r') ? ' ' : c;
    }
    out[o] = 0;
}

/* the text's lines, from the first, drawn from y0 (eight pixels a line) */
static void oc_lines(const char *text, uint32_t first, int32_t y0)
{
    char line[24];
    for (uint32_t r = 0; r < 7u; r++) {
        oc_text_line(text, first + r, line, sizeof line);
        if (!line[0]) break;
        oc_display_text(0, y0 + (int32_t)r * 8, line);
    }
}

/* the whole screen: the text's lines */
void oc_diag_screen(const char *text)
{
    oc_display_clear();
    oc_lines(text, 0, 0);
    oc_display_flush();
}

/* ------------------------------------------------------- the crash report --- */

/* The framework's fault information, at the top of the AXI RAM (the same
 * address the framework's own crash report reads): the CFSR is the third
 * word, the MMFAR the fifth, the BFAR the sixth and the return address the
 * seventh. */
#define OC_FAULT_INFO ((volatile uint32_t *)0x2027FF80)

static char crash_text[1024];
static uint32_t crash_len;

/* the report's text, into a buffer of our own: the framework's print clears
 * the report as it prints it */
class OcTextBuf : public Print {
public:
    size_t write(uint8_t c) override {
        if (crash_len + 1u < sizeof crash_text) crash_text[crash_len++] = (char)c;
        return 1;
    }
};

/* the crash report on the screen: a page at a time, the top button for the
 * next page and the bottom one to go on. There is no timeout on purpose: a
 * module in a reboot loop must stop on a screen that says what happened. */
void oc_diag_crash(void)
{
    volatile uint32_t *fault = OC_FAULT_INFO;
    OcTextBuf buf;
    char page[2048], head[24];
    uint32_t pages, page_no = 0, lines = 0;
    uint32_t cfsr, addr, pc, srsr;

    if (!CrashReport) {
        /* No crash report: but the last run did not get to the end (a reset
         * in the middle of the boot), so the screen says where it stopped. */
        if (oc_persist.prev_stage != 99u && oc_persist.prev_stage != 23u) {
            snprintf(page, sizeof page, "RESET AT STAGE %u", (unsigned)oc_persist.prev_stage);
            oc_diag_screen(page);
            delay(5000);
        }
        return;
    }

    /* the registers first: the framework's print clears the report as it
     * prints it (and that zeroes SRC_SRSR), so they are read here */
    cfsr = fault[2];
    addr = (cfsr & 0x80u) ? fault[4] : fault[5];
    pc = fault[6];
    srsr = SRC_SRSR;

    crash_len = 0;
    CrashReport.printTo(buf);
    crash_text[crash_len] = 0;

    /* the summary: where it was, what faulted, and how long the tick was.
     * It is the first page, and the report's own text is the rest. */
    snprintf(page, sizeof page,
             "PREV STG %u %s\nCFSR 0x%08X\n%s 0x%08X\nPC 0x%08X\nSRSR 0x%08X\nISR MAX %u US\n",
             (unsigned)oc_persist.prev_stage, oc_stage_name(oc_persist.prev_stage),
             (unsigned)cfsr, (cfsr & 0x80u) ? "MMFAR" : "BFAR", (unsigned)addr,
             (unsigned)pc, (unsigned)srsr, (unsigned)oc_us(oc_persist.isr_max_cyc));
    strncat(page, crash_text, sizeof page - strlen(page) - 1u);
    for (uint32_t i = 0; page[i]; i++) if (page[i] == '\n') lines++;

    /* and the serial: the same text, without waiting for it */
    Serial.println(F("Tessera-OC: the last run crashed:"));
    Serial.print(crash_text);

    pages = (lines + 7u) / 7u;
    for (;;) {
        oc_display_clear();
        snprintf(head, sizeof head, "CRASH p/%u", (unsigned)(page_no + 1u));
        oc_display_text(0, 0, head);
        oc_lines(page, page_no * 7u, 8);
        oc_display_flush();
        /* the top button is the next page, the bottom one goes on. It waits
         * for the release, so a held button counts once. */
        int b = 0;
        while (!b) {
            b = !digitalRead(oc_pins.but_top) ? 1 : (!digitalRead(oc_pins.but_bot) ? 2 : 0);
            if (!b) delay(10);
        }
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
    snprintf(text, sizeof text,
             "SAFE MODE\nVER %s\nPREV STG %u %s\nBOOTS %u\nISR %u US BLK %u US\nSTORAGE %s",
             OC_VERSION_STRING, (unsigned)oc_persist.prev_stage,
             oc_stage_name(oc_persist.prev_stage), (unsigned)oc_persist.boots,
             (unsigned)oc_us(oc_persist.isr_max_cyc),
             (unsigned)oc_us(oc_persist.block_max_cyc),
             oc_storage_ok() ? "ON (RO)" : "OFF (RO)");
    oc_diag_screen(text);
}

/* --------------------------------------------------------- the watchdog --- */

/* WDOG1: 16 s (the timeout is (WT+1) * 0.5 s, so WT is 31). It resets the
 * module when it is not fed, so a module that hangs comes back and says so
 * instead of sitting there. */
void oc_diag_watchdog(bool arm)
{
    if (arm) {
        CCM_CCGR3 = (CCM_CCGR3 & ~(CCM_CCGR3_WDOG1(3u))) | CCM_CCGR3_WDOG1(1u);
        WDOG1_WMCR = 0x5555;                /* the unlock */
        WDOG1_WMCR = 0xAAAA;
        WDOG1_WICR = 0;                     /* no interrupt: the reset is enough */
        WDOG1_WCR = WDOG_WCR_WT(31u) | WDOG_WCR_WDE | WDOG_WCR_SRE;
    }
    WDOG1_WSR = 0x5555;                     /* the service */
}
