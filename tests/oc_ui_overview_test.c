/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* OVERVIEW's helpers: the note name and the gate history (UI-PLAN §4.1). */
#include <stdio.h>
#include <string.h>
#include "oc_ui_overview.h"

static int checks, failures;

static void check(const char *what, int ok)
{
    checks++;
    if (!ok)
        failures++;
    printf("oc_overview: %-72s %s\n", what, ok ? "ok" : "FAIL");
}

static void check_note(void)
{
    char b[8];
    ui_note_text(36, b, sizeof b);
    check("note: MIDI 36 is C2 (the CV's 0 V)", strcmp(b, "C2") == 0);
    ui_note_text(51, b, sizeof b);
    check("note: MIDI 51 is D#3", strcmp(b, "D#3") == 0);
    ui_note_text(0, b, sizeof b);
    check("note: MIDI 0 is C-1", strcmp(b, "C-1") == 0);
    ui_note_text(-1, b, sizeof b);
    check("note: none is --", strcmp(b, "--") == 0);
    ui_note_text(-20, b, sizeof b);
    check("note: below none is still --", strcmp(b, "--") == 0);
    ui_note_text(120, b, sizeof b);
    check("note: MIDI 120 is C9, the top of the CV", strcmp(b, "C9") == 0);
    ui_note_text(127, b, sizeof b);
    check("note: above 120 clamps to C9", strcmp(b, "C9") == 0);
    ui_note_text(60, b, 0);
    check("note: a zero buffer is left alone", 1);
}

static void check_hist(void)
{
    uint16_t h = 0;
    int i;
    h = ui_hist_shift(h, 1);
    check("hist: the first bit lands on the right", h == 1u);
    h = ui_hist_shift(h, 0);
    check("hist: a silent frame shifts a zero in", h == 2u);
    for (i = 0; i < 14; i++)
        h = ui_hist_shift(h, 1);
    check("hist: 16 cells, the oldest still there", h == 0xBFFFu);
    h = ui_hist_shift(h, 0);
    check("hist: the 17th frame drops the oldest bit", h == 0x7FFEu);
}

int main(void)
{
    check_note();
    check_hist();
    printf("oc_overview: %d checks, %d failing\n", checks, failures);
    return failures ? 1 : 0;
}
