/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The UI's screen router: the global control map (UI-PLAN.md §2, §3.2).
 * Pure C, no Arduino, no engine: it takes the input core's events and keeps
 * the screen stack and the cursors; anything that touches the engine comes
 * back to the shell as an action. Host-tested in tests/oc_ui_nav_test.c. */
#ifndef OC_UI_NAV_H_
#define OC_UI_NAV_H_

#include <stdint.h>

#include "oc_ui_input.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { UI_SCR_OVERVIEW, UI_SCR_TRACK, UI_SCR_PERFORM, UI_SCR_PROJECT, UI_SCR_INFO, UI_SCR_MENU, UI_SCR_COUNT };
enum { UI_TAB_GEN, UI_TAB_SCL, UI_TAB_CV, UI_TAB_MOD, UI_TAB_COUNT };

/* what the shell must do for an event (the router changes no engine state) */
enum {
    UI_ACT_NONE,
    UI_ACT_PLAY_STOP,     /* TOP press (a TOP that carries the shift is still plain TOP) */
    UI_ACT_RESTART,       /* BOTTOM tap that was not a shift */
    UI_ACT_SELECT_TRACK,  /* BOTTOM + L turn, any screen: n tracks */
    UI_ACT_TEMPO,         /* BOTTOM + R turn, any screen: n BPM steps */
    UI_ACT_VALUE,         /* R turn: n detents, dt, fine (the shell applies ui_accel) */
    UI_ACT_ENTER,         /* R tap: open / toggle / confirm the row under the cursor */
    UI_ACT_CONTEXT,       /* R long: reset to default (TRACK), roll (PERFORM) */
    UI_ACT_SECONDARY      /* BOTTOM + R tap: the screen's secondary action */
};

typedef struct {
    uint8_t kind;
    int16_t n;
    uint16_t dt;
    uint8_t fine;
} ui_action_t;

#define UI_NAV_DEPTH 4u
#define UI_MENU_ITEMS 5u

typedef struct {
    uint8_t stack[UI_NAV_DEPTH];  /* stack[depth - 1] is the screen shown */
    uint8_t depth;
    uint8_t rows[UI_SCR_COUNT];   /* rows on each screen, set by the shell */
    uint8_t cursor[UI_SCR_COUNT]; /* the row cursor on each screen */
    uint8_t tab;                  /* TRACK's tab: GEN, SCL, CV, MOD */
} ui_nav_t;

/* OVERVIEW at the root, the menu's five items, every other screen empty */
void ui_nav_init(ui_nav_t *nav);
/* the shell says how many rows a screen has now (cursors are clamped) */
void ui_nav_rows(ui_nav_t *nav, uint32_t screen, uint32_t rows);
uint32_t ui_nav_screen(const ui_nav_t *nav);
/* one event in; the action for the shell out (UI_ACT_NONE: nothing to do) */
ui_action_t ui_nav_event(ui_nav_t *nav, const ui_event_t *ev);
/* the menu's items, in order: OVERVIEW, PERFORM, TRACK, PROJECT, INFO */
uint32_t ui_nav_menu_item(uint32_t index);

#ifdef __cplusplus
}
#endif

#endif
