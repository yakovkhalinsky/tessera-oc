/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* tools/render_screens: the module's screens, as PNG pictures for the guide
 * (docs/GUIDE.md).
 *
 * The screens are drawn by the firmware's own code (firmware/oc_display.cpp,
 * oc_ui.cpp, oc_diag.cpp) and the engine's own state (firmware/oc_engine.c),
 * built for the host against the shims beside this file (Arduino.h, SPI.h).
 * The display's bytes are caught as the SH1106 would take them, so a picture
 * is the panel's RAM after a flush, pixel for pixel: three times the size,
 * one grey level for a lit pixel, no smoothing.
 *
 *   tools/render_screens.sh [OUTDIR]     (default docs/img)
 *
 * The PNG is written here, by hand: stored (uncompressed) deflate blocks, the
 * CRC-32 and the Adler-32, so nothing outside the C++ library is needed. */
#include <Arduino.h>
#include <SPI.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "oc_port.h"
#include "../../parts/oc_ui_nav.h"

/* the screen code's entry points (declared in the firmware's own files) */
void oc_display_init(void);
void oc_display_splash(void);
void ui_init(void);
void ui_draw(void);
void ui_open(uint32_t screen, uint32_t tab);   /* oc_ui.cpp: the renderer's way in */

/* ---------------------------------------------- what the platform owns --- */
/* oc_platform.cpp's globals and functions that the screen code reads; the
 * values are the module's defaults (the plain pinout, the display offset 2) */
struct oc_pinout oc_pins = OC_PINOUT_PLAIN;
uint32_t oc_cal_flags;
uint8_t oc_cal_display_offset = 2;
volatile uint8_t oc_spi_busy, oc_spi_pending;
static uint8_t rs_clock_ext;
extern "C" uint8_t oc_clock_following(void) { return rs_clock_ext; }

extern "C" bool oc_storage_ok(void) { return true; }        /* the drive is there, read only */
extern "C" int oc_slot_used(uint32_t slot) { return slot == 0u; }
extern "C" int oc_save(uint32_t slot) { (void)slot; return -1; }
extern "C" int oc_load(uint32_t slot) { (void)slot; return -1; }

RsSerial Serial;
RsSPI SPI;
RsCrashReport CrashReport;

/* ----------------------------------------------------- the panel's RAM --- */
#define OLED_DC 6                 /* firmware/oc_display.cpp's D/C pin */
#define W 128
#define H 64
#define RAM_W 132                 /* the SH1106's RAM is 132 wide */
static uint8_t ram[8][RAM_W];
static uint8_t dc, page, col;
static int new_frame;             /* a flush has reached the last byte */

void rs_pin_write(uint8_t pin, uint8_t level)
{
    if (pin == OLED_DC)
        dc = level;
}

void rs_spi_byte(uint8_t b)
{
    if (!dc) {                    /* a command: the page or the column */
        if ((b & 0xF0u) == 0xB0u)
            page = b & 7u;
        else if ((b & 0xF0u) == 0x10u)
            col = (uint8_t)((col & 0x0Fu) | ((b & 0x0Fu) << 4));
        else if (b < 0x10u)
            col = (uint8_t)((col & 0xF0u) | b);
        return;
    }
    if (col < RAM_W)
        ram[page][col++] = b;
    if (page == 7u && col == oc_cal_display_offset + W)
        new_frame = 1;
}

static int lit(int x, int y)
{
    return (ram[y / 8][x + oc_cal_display_offset] >> (y & 7)) & 1;
}

/* ----------------------------------------------------------- the PNG --- */
#define SCALE 3
static uint32_t crc_table[256];

static void crc_init(void)
{
    for (uint32_t n = 0; n < 256u; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++)
            c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
}

static uint32_t crc(uint32_t c, const uint8_t *p, size_t n)
{
    c ^= 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++)
        c = crc_table[(c ^ p[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static void be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}

static void chunk(FILE *f, const char *type, const uint8_t *data, uint32_t n)
{
    uint8_t len[4], sum[4];
    uint32_t c;
    be32(len, n);
    fwrite(len, 1, 4, f);
    fwrite(type, 1, 4, f);
    if (n)
        fwrite(data, 1, n, f);
    c = crc(0, (const uint8_t *)type, 4);
    c = crc(c, data, n);
    be32(sum, c);
    fwrite(sum, 1, 4, f);
}

/* the screen, as text and as a bitmap: the golden snapshot and the picture
 *
 * The golden is the screen the module shows, 64 lines of 128 characters, a
 * '#' for a lit pixel and a '.' for a dark one. It is what a reviewer reads:
 * the PNGs are pictures, and a picture cannot be diffed. The PBM is the same
 * pixels in a format every image tool reads, for the README's screenshots.
 *
 * The goldens live in tests/golden/ and are checked against a fresh render,
 * like the PNGs in docs/img/ are: a screen that changed without its golden
 * being updated is a change nobody looked at.
 *
 * The crash and safe-mode screens draw with the plain text of the 5 x 7 face
 * (the crash pager's own layout), so their goldens do not move when the grid
 * changes. That is the point of having them. */
static const char *golden_dir;        /* where the .txt goldens go, or 0 */
static const char *pbm_dir;           /* where the .pbm files go, or 0 */

static int save_golden(const char *name)
{
    char path[512], stem[256];
    uint8_t row[W / 8];
    FILE *f;
    size_t n = strlen(name);

    if (!golden_dir || !*golden_dir)
        return 0;
    if (n < 5 || strcmp(name + n - 4, ".png") != 0)
        return 1;
    snprintf(stem, sizeof stem, "%.*s", (int)(n - 4), name);

    snprintf(path, sizeof path, "%s/%s.txt", golden_dir, stem);
    f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "render_screens: cannot write %s\n", path);
        return 1;
    }
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++)
            fputc(lit(x, y) ? '#' : '.', f);
        fputc('\n', f);
    }
    fclose(f);

    if (pbm_dir && *pbm_dir) {
        snprintf(path, sizeof path, "%s/%s.pbm", pbm_dir, stem);
        f = fopen(path, "wb");
        if (!f) {
            fprintf(stderr, "render_screens: cannot write %s\n", path);
            return 1;
        }
        fprintf(f, "P4\n%d %d\n", W, H);
        for (int y = 0; y < H; y++) {
            memset(row, 0, sizeof row);
            for (int x = 0; x < W; x++)
                if (lit(x, y))
                    row[x / 8] |= (uint8_t)(0x80u >> (x % 8));
            fwrite(row, 1, sizeof row, f);
        }
        fclose(f);
    }
    return 0;
}

/* the panel as an 8-bit grey picture: lit = 255, dark = 0 */
static int save_png(const char *dir, const char *name)
{
    static const uint8_t SIG[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    const uint32_t pw = W * SCALE, ph = H * SCALE, stride = pw + 1u;
    const uint32_t raw_n = stride * ph;
    uint8_t *raw = (uint8_t *)malloc(raw_n);
    uint8_t *z = (uint8_t *)malloc(raw_n + raw_n / 65535u * 5u + 16u);
    uint8_t ihdr[13];
    uint32_t zn = 0, a = 1, b = 0;
    char path[512];
    FILE *f;

    if (!raw || !z) {
        free(raw); free(z);
        return 1;
    }
    for (uint32_t y = 0; y < ph; y++) {
        raw[y * stride] = 0;                          /* the filter: none */
        for (uint32_t x = 0; x < pw; x++)
            raw[y * stride + 1u + x] = lit((int)(x / SCALE), (int)(y / SCALE)) ? 255u : 0u;
    }
    z[zn++] = 0x78;                                   /* zlib: deflate, 32 K window */
    z[zn++] = 0x01;
    for (uint32_t off = 0; off < raw_n;) {
        uint32_t n = raw_n - off > 65535u ? 65535u : raw_n - off;
        z[zn++] = (uint8_t)(off + n == raw_n);        /* BFINAL, BTYPE 00: stored */
        z[zn++] = (uint8_t)n; z[zn++] = (uint8_t)(n >> 8);
        z[zn++] = (uint8_t)~n; z[zn++] = (uint8_t)(~n >> 8);
        memcpy(z + zn, raw + off, n);
        zn += n;
        off += n;
    }
    for (uint32_t i = 0; i < raw_n; i++) {            /* Adler-32 */
        a = (a + raw[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    be32(z + zn, (b << 16) | a);
    zn += 4;

    be32(ihdr, pw);
    be32(ihdr + 4, ph);
    ihdr[8] = 8;                                      /* 8 bits, grey */
    ihdr[9] = 0;
    ihdr[10] = ihdr[11] = ihdr[12] = 0;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    f = fopen(path, "wb");
    if (!f) {
        free(raw); free(z);
        return 1;
    }
    fwrite(SIG, 1, 8, f);
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", z, zn);
    chunk(f, "IEND", 0, 0);
    fclose(f);
    free(raw);
    free(z);
    printf("render_screens: %s\n", path);
    if (save_golden(name))
        return 1;
    return 0;
}

/* ------------------------------------------ the buttons and the report --- */
/* The crash pager (oc_diag_crash) waits for a press: the first read after a
 * flush saves the screen, and then the scripted button reads LOW once. */
static uint8_t level[64];
static const char *out_dir;
static const char *const *script_names;
static const uint8_t *script_pins;
static int script_len, script_at, pressed = -1, failed;

uint8_t rs_pin_read(uint8_t pin)
{
    if (new_frame && script_at < script_len) {
        new_frame = 0;
        failed |= save_png(out_dir, script_names[script_at]);
        pressed = script_pins[script_at++];
    }
    if ((int)pin == pressed) {
        pressed = -1;
        return LOW;
    }
    return level[pin & 63u];
}

static int crash_on;
static const char CRASH_TEXT[] =
    "CrashReport:\n"
    "  A problem occurred at (system time) 0:0:2\n"
    "  Code was executing from address 0x3F12\n"
    "  CFSR: 82\n"
    "\t(DACCVIOL) Data Access Violation\n"
    "\t(MMARVALID) Accessed Address: 0x200272A0\n"
    "  Temperature inside the chip was 41.27 C\n"
    "  Startup CPU clock speed is 600MHz\n"
    "  Reboot was caused by auto reboot after fault or bad interrupt detected\n";

RsCrashReport::operator bool() const { return crash_on != 0; }
void RsCrashReport::printTo(Print &p) const
{
    for (const char *s = CRASH_TEXT; *s; s++)
        p.write((uint8_t)*s);
}
void RsCrashReport::clear() { crash_on = 0; }

/* ----------------------------------------------------------- the main --- */
int main(int argc, char **argv)
{
    static const char *const CRASH_NAMES[2] = {"screen-crash-1.png", "screen-crash-2.png"};
    static const uint8_t CRASH_PINS[2] = {5, 4};      /* oc_pins.but_top, then .but_bot */

    /* The Cortex-M7's debug registers are plain memory on the host: the boot's
     * diagnostics (oc_diag_begin) start the cycle counter at 0xE0001000, its
     * lock at 0xE0001FB0 and the trace at 0xE000EDFC. */
    void *dbg = mmap((void *)0xE0000000ul, 0x10000, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (dbg != (void *)0xE0000000ul) {
        fprintf(stderr, "render_screens: cannot map the debug registers\n");
        return 1;
    }
    out_dir = argc > 1 ? argv[1] : "docs/img";
    golden_dir = argc > 2 ? argv[2] : 0;             /* the .txt goldens, or none */
    pbm_dir = argc > 3 ? argv[3] : 0;                /* the .pbm files, or none */
    crc_init();
    memset(level, HIGH, sizeof level);                /* every button released */

    /* the splash, as the module boots */
    oc_display_init();
    oc_display_splash();
    failed |= save_png(out_dir, "screen-splash.png");

    /* HOME: a power-on with nothing saved (the module composes: four
     * generators, playing). oc_transport only queues the start; one control
     * block latches song.playing so the header shows the play mark, without
     * waiting for a first note. */
    oc_engine_init();
    oc_power_on();
    oc_clock_advance(32);                 /* CTL, one events_block */
    ui_init();
    ui_draw();
    failed |= save_png(out_dir, "screen-home.png");

    /* OVERVIEW's other states (phase 3): track 2 selected, track 3 muted,
     * following TR1, and a ROLL waiting on track 4 */
    oc_select_track(1);
    ui_draw();
    failed |= save_png(out_dir, "screen-overview-t2.png");
    oc_select_track(2);
    oc_track_set_mute(2, 1);
    ui_draw();
    failed |= save_png(out_dir, "screen-overview-mute.png");
    oc_track_set_mute(2, 0);
    oc_select_track(0);
    rs_clock_ext = 1;
    ui_draw();
    failed |= save_png(out_dir, "screen-overview-ext.png");
    rs_clock_ext = 0;
    oc_select_track(3);
    oc_track_set_roll(3, 1);
    ui_draw();
    failed |= save_png(out_dir, "screen-overview-roll.png");
    oc_track_set_roll(3, 0);
    oc_select_track(0);

    /* the shell's other screens (phase 1c), opened as the router opens them:
     * the screen menu, TRACK on its GEN tab (track 1), PERFORM, PROJECT and
     * INFO (the timings read 0 on the host) */
    ui_open(UI_SCR_MENU, 0);
    ui_draw();
    failed |= save_png(out_dir, "screen-menu.png");
    ui_open(UI_SCR_TRACK, UI_TAB_GEN);
    ui_draw();
    failed |= save_png(out_dir, "screen-track.png");
    ui_open(UI_SCR_PERFORM, 0);
    ui_draw();
    failed |= save_png(out_dir, "screen-perform.png");
    ui_open(UI_SCR_PROJECT, 0);
    ui_draw();
    failed |= save_png(out_dir, "screen-project.png");
    ui_open(UI_SCR_INFO, 0);
    ui_draw();
    failed |= save_png(out_dir, "screen-info.png");

    /* the safe mode: a warm boot after a run that got to the end (stage 23) */
    oc_diag_begin();
    oc_stage(23);
    oc_diag_begin();
    oc_diag_safe();
    failed |= save_png(out_dir, "screen-safe-mode.png");

    /* a reset in the middle of the boot (stage 17, the engine), no report */
    oc_stage(17);
    oc_diag_begin();
    oc_diag_crash();
    failed |= save_png(out_dir, "screen-reset.png");

    /* the crash pager: page 1, TOP for page 2, BOTTOM to go on */
    oc_stage(17);
    oc_diag_begin();
    crash_on = 1;
    new_frame = 0;
    script_names = CRASH_NAMES;
    script_pins = CRASH_PINS;
    script_len = 2;
    script_at = 0;
    oc_diag_crash();
    if (script_at != 2)
        failed = 1;
    return failed;
}
