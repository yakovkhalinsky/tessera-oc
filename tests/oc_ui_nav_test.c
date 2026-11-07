/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The UI's screen router: the host test (UI-PLAN §3.6). The router is pure
 * C: the screen stack and its bound, the cursors, the menu, TRACK's tabs and
 * the global control map are all checked here, without the shell.
 *
 *   build/host/oc_ui_nav_test
 */

#include <stdio.h>
#include <stdint.h>

#include "oc_ui_nav.h"

static int checks, failures;

static void check(const char *what, int ok)
{
    checks++;
    if (!ok)
        failures++;
    printf("oc_nav: %-72s %s\n", what, ok ? "ok" : "FAIL");
}

/* the router under test */
static ui_nav_t nav;

/* one event in: dt 7, fine 0 */
static ui_action_t ev(uint8_t src, uint8_t kind, int16_t n, uint8_t shift)
{
    ui_event_t e = {src, kind, n, 7, 0, shift};
    return ui_nav_event(&nav, &e);
}

/* the menu's item i, opened from a fresh router */
static void to_screen(int i)
{
    ui_nav_init(&nav);
    ev(UI_SRC_PUSH_L, UI_EV_LONG, 0, 0);
    ev(UI_SRC_ENC_L, UI_EV_TURN, (int16_t)i, 0);
    ev(UI_SRC_PUSH_R, UI_EV_TAP, 0, 0);
}

/* the screens' names, for the labels */
static const char *const NAMES[UI_SCR_COUNT] = {
    "OVERVIEW", "TRACK", "PERFORM", "PROJECT", "INFO", "MENU"};

/* the screen shown, and how deep the stack is */
static int at(uint32_t screen, uint8_t depth)
{
    return nav.depth == depth && ui_nav_screen(&nav) == screen;
}

/* the root, and the menu that opens and closes */
static void check_menu(void)
{
    ui_action_t a;
    ui_nav_init(&nav);
    check("init: OVERVIEW, one deep, five menu items",
          at(UI_SCR_OVERVIEW, 1) && nav.rows[UI_SCR_MENU] == UI_MENU_ITEMS);
    a = ev(UI_SRC_PUSH_L, UI_EV_TAP, 0, 0);
    check("back: an L tap at the root stays at the root", a.kind == UI_ACT_NONE && at(UI_SCR_OVERVIEW, 1));
    a = ev(UI_SRC_PUSH_L, UI_EV_LONG, 0, 0);
    check("menu: an L long opens the menu", a.kind == UI_ACT_NONE && at(UI_SCR_MENU, 2));
    a = ev(UI_SRC_PUSH_L, UI_EV_LONG, 0, 0);
    check("menu: an L long in the menu does nothing", a.kind == UI_ACT_NONE && at(UI_SCR_MENU, 2));
    a = ev(UI_SRC_PUSH_L, UI_EV_TAP, 0, 0);
    check("menu: an L tap closes the menu", a.kind == UI_ACT_NONE && at(UI_SCR_OVERVIEW, 1));
}

/* the menu picks a screen */
static void check_menu_pick(void)
{
    ui_action_t a;
    to_screen(2);
    check("menu: the third item opens TRACK", at(UI_SCR_TRACK, 2));
    ui_nav_init(&nav);
    ev(UI_SRC_PUSH_L, UI_EV_LONG, 0, 0);
    ev(UI_SRC_ENC_L, UI_EV_TURN, -1, 0);
    check("menu: the cursor wraps backwards", nav.cursor[UI_SCR_MENU] == 4);
    a = ev(UI_SRC_PUSH_R, UI_EV_TAP, 0, 0);
    check("menu: the fifth item opens INFO", a.kind == UI_ACT_NONE && at(UI_SCR_INFO, 2));
    ev(UI_SRC_PUSH_L, UI_EV_LONG, 0, 0);
    ev(UI_SRC_ENC_L, UI_EV_TURN, 1, 0);
    check("menu: the cursor moves on", nav.cursor[UI_SCR_MENU] == 0);
    a = ev(UI_SRC_PUSH_R, UI_EV_TAP, 0, 0);
    check("menu: OVERVIEW is the root again", a.kind == UI_ACT_NONE && at(UI_SCR_OVERVIEW, 1));
}

/* TOP + BOTTOM opens and closes PERFORM; the stack is four deep */
static void check_stack(void)
{
    int i, over = 0;
    ui_action_t a;
    ui_nav_init(&nav);
    a = ev(UI_SRC_CHORD_TB, UI_EV_CHORD, 0, 0);
    check("chord: TOP + BOTTOM opens PERFORM", a.kind == UI_ACT_NONE && at(UI_SCR_PERFORM, 2));
    a = ev(UI_SRC_CHORD_TB, UI_EV_CHORD, 0, 0);
    check("chord: TOP + BOTTOM again closes it", a.kind == UI_ACT_NONE && at(UI_SCR_OVERVIEW, 1));
    for (i = 0; i < 8; i++) {
        ev(UI_SRC_CHORD_TB, UI_EV_CHORD, 0, 0);
        ev(UI_SRC_PUSH_L, UI_EV_LONG, 0, 0);
        if (nav.depth > UI_NAV_DEPTH)
            over = 1;
    }
    check("depth: the stack never grows past four", over == 0 && nav.depth == UI_NAV_DEPTH);
    to_screen(2);
    ev(UI_SRC_CHORD_TB, UI_EV_CHORD, 0, 0);
    ev(UI_SRC_PUSH_L, UI_EV_LONG, 0, 0);
    ev(UI_SRC_CHORD_TB, UI_EV_CHORD, 0, 0);
    check("depth: a full stack replaces its top",
          nav.depth == UI_NAV_DEPTH && at(UI_SCR_PERFORM, 4) && nav.stack[2] == UI_SCR_PERFORM);
    to_screen(2);
    ev(UI_SRC_CHORD_TB, UI_EV_CHORD, 0, 0);
    a = ev(UI_SRC_CHORD_TB, UI_EV_CHORD, 0, 0);
    check("chord: from TRACK, TOP + BOTTOM twice comes back to TRACK", a.kind == UI_ACT_NONE && at(UI_SCR_TRACK, 2));
    to_screen(2);
    ev(UI_SRC_CHORD_TB, UI_EV_CHORD, 0, 0);
    ev(UI_SRC_PUSH_L, UI_EV_LONG, 0, 0);
    ev(UI_SRC_ENC_L, UI_EV_TURN, 2, 0);
    ev(UI_SRC_PUSH_R, UI_EV_TAP, 0, 0);       /* OVERVIEW, TRACK, PERFORM, INFO: full */
    ev(UI_SRC_PUSH_L, UI_EV_LONG, 0, 0);
    over = at(UI_SCR_MENU, 4) && nav.stack[2] == UI_SCR_PERFORM;
    ev(UI_SRC_ENC_L, UI_EV_TURN, -1, 0);
    ev(UI_SRC_PUSH_R, UI_EV_TAP, 0, 0);
    check("depth: the menu on a full stack replaces the top; its pick lands at four",
          over && at(UI_SCR_PROJECT, 4) && nav.stack[2] == UI_SCR_PERFORM);
}

/* the global controls: the value, the track, the tempo, TOP and BOTTOM */
static void check_global(void)
{
    int i;
    char what[96];
    ui_action_t a;
    ui_nav_init(&nav);
    a = ev(UI_SRC_ENC_R, UI_EV_TURN, 3, 0);
    check("value: an R turn is a value", a.kind == UI_ACT_VALUE && a.n == 3 && a.dt == 7);
    {
        ui_event_t e = {UI_SRC_ENC_R, UI_EV_TURN, -4, 33, 1, 0};
        a = ui_nav_event(&nav, &e);
    }
    check("value: the R turn's dt and fine pass through", a.kind == UI_ACT_VALUE && a.n == -4 && a.dt == 33 && a.fine == 1);
    ui_nav_init(&nav);
    ev(UI_SRC_PUSH_L, UI_EV_LONG, 0, 0);
    ev(UI_SRC_ENC_L, UI_EV_TURN, 2, 0);
    a = ev(UI_SRC_ENC_R, UI_EV_TURN, 3, 0);
    check("value: in the menu an R turn does nothing", a.kind == UI_ACT_NONE && nav.cursor[UI_SCR_MENU] == 2);
    for (i = 0; i < 5; i++) {
        int ok;
        if (i == 0)
            ui_nav_init(&nav);
        else
            to_screen(i);
        a = ev(UI_SRC_ENC_L, UI_EV_SHIFT_TURN, -2, 1);
        ok = a.kind == UI_ACT_SELECT_TRACK && a.n == -2;
        a = ev(UI_SRC_ENC_R, UI_EV_SHIFT_TURN, 5, 1);
        ok = ok && a.n == 5 && (i == 1 || a.kind == UI_ACT_TEMPO);   /* PERFORM: below */
        a = ev(UI_SRC_TOP, UI_EV_DOWN, 0, 0);
        ok = ok && a.kind == UI_ACT_PLAY_STOP;
        a = ev(UI_SRC_TOP, UI_EV_DOWN, 0, 1);
        ok = ok && a.kind == UI_ACT_PLAY_STOP;
        a = ev(UI_SRC_BOT, UI_EV_TAP, 0, 0);
        ok = ok && a.kind == UI_ACT_RESTART;
        snprintf(what, sizeof what, "global: the track, the tempo, TOP and BOTTOM (%s)",
                 NAMES[ui_nav_screen(&nav)]);
        check(what, ok);
    }
    to_screen(1);
    a = ev(UI_SRC_ENC_R, UI_EV_SHIFT_TURN, 5, 1);
    check("perform: BOTTOM + R is the EVOLVE amount, not the tempo", a.kind == UI_ACT_MACRO && a.n == 5);
}

/* TRACK: the cursor, the tabs, enter and context */
static void check_track(void)
{
    ui_action_t a;
    to_screen(2);
    ui_nav_rows(&nav, UI_SCR_TRACK, 6);
    a = ev(UI_SRC_ENC_L, UI_EV_TURN, 1, 0);
    check("track: the L encoder moves the cursor, never the track",
          a.kind == UI_ACT_NONE && nav.cursor[UI_SCR_TRACK] == 1);
    a = ev(UI_SRC_ENC_L, UI_EV_TURN, -3, 0);
    check("track: the cursor wraps", a.kind == UI_ACT_NONE && nav.cursor[UI_SCR_TRACK] == 4);
    a = ev(UI_SRC_PUSH_R, UI_EV_TAP, 0, 1);
    check("track: a shifted R tap goes to the next tab",
          a.kind == UI_ACT_NONE && nav.tab == UI_TAB_SCL && nav.cursor[UI_SCR_TRACK] == 0);
    ev(UI_SRC_PUSH_R, UI_EV_TAP, 0, 1);
    ev(UI_SRC_PUSH_R, UI_EV_TAP, 0, 1);
    ev(UI_SRC_PUSH_R, UI_EV_TAP, 0, 1);
    check("track: the tabs wrap back to GEN", nav.tab == UI_TAB_GEN);
    a = ev(UI_SRC_PUSH_R, UI_EV_TAP, 0, 0);
    check("track: an R tap enters the row under the cursor", a.kind == UI_ACT_ENTER);
    a = ev(UI_SRC_PUSH_R, UI_EV_LONG, 0, 0);
    check("track: an R long is the context action", a.kind == UI_ACT_CONTEXT);
    a = ev(UI_SRC_PUSH_R, UI_EV_LONG, 0, 1);
    check("track: a shifted R long is still the context action", a.kind == UI_ACT_CONTEXT);
}

/* the rows the shell reports, and the cursor's clamp */
static void check_rows(void)
{
    ui_nav_init(&nav);
    ev(UI_SRC_ENC_L, UI_EV_TURN, 3, 0);
    check("rows: a screen with no rows has no cursor to move", nav.cursor[UI_SCR_OVERVIEW] == 0);
    ui_nav_rows(&nav, UI_SCR_OVERVIEW, 4);
    ev(UI_SRC_ENC_L, UI_EV_TURN, 3, 0);
    check("rows: the cursor moves over the rows", nav.cursor[UI_SCR_OVERVIEW] == 3);
    ui_nav_rows(&nav, UI_SCR_OVERVIEW, 2);
    check("rows: fewer rows clamp the cursor", nav.cursor[UI_SCR_OVERVIEW] == 1);
    ui_nav_rows(&nav, UI_SCR_COUNT, 9);
    check("rows: a screen that does not exist is ignored", nav.rows[UI_SCR_OVERVIEW] == 2 && nav.depth == 1);
}

/* the events that mean nothing, and the menu's items */
static void check_rest(void)
{
    static const uint8_t BOT_KINDS[4] = {UI_EV_DOWN, UI_EV_HOLD_START, UI_EV_HOLD_END, UI_EV_LONG};
    ui_action_t a;
    int i, ok = 1;
    ui_nav_init(&nav);
    for (i = 0; i < 4; i++) {
        a = ev(UI_SRC_BOT, BOT_KINDS[i], 0, 0);
        ok = ok && a.kind == UI_ACT_NONE && at(UI_SCR_OVERVIEW, 1);
    }
    check("bottom: down, the hold and the long mean nothing", ok);
    a = ev(UI_SRC_TOP, UI_EV_TAP, 0, 0);
    check("bottom: a TOP tap means nothing", a.kind == UI_ACT_NONE && at(UI_SCR_OVERVIEW, 1));
    a = ev(UI_SRC_PUSH_R, UI_EV_TAP, 0, 1);
    check("secondary: a shifted R tap is the secondary action", a.kind == UI_ACT_SECONDARY);
    a = ev(UI_SRC_PUSH_R, UI_EV_HOLD_START, 0, 0);
    check("misc: an R hold start means nothing", a.kind == UI_ACT_NONE && at(UI_SCR_OVERVIEW, 1));
    a = ev(UI_SRC_PUSH_L, UI_EV_DOWN, 0, 0);
    check("misc: an L down means nothing", a.kind == UI_ACT_NONE && at(UI_SCR_OVERVIEW, 1));
    check("menu items: OVERVIEW, PERFORM, TRACK, PROJECT, INFO",
          ui_nav_menu_item(0) == UI_SCR_OVERVIEW && ui_nav_menu_item(1) == UI_SCR_PERFORM &&
          ui_nav_menu_item(2) == UI_SCR_TRACK && ui_nav_menu_item(3) == UI_SCR_PROJECT &&
          ui_nav_menu_item(4) == UI_SCR_INFO && ui_nav_menu_item(9) == UI_SCR_OVERVIEW);
}

int main(void)
{
    check_menu();
    check_menu_pick();
    check_stack();
    check_global();
    check_track();
    check_rows();
    check_rest();

    printf("oc_nav: %d checks, %d failing\n", checks, failures);
    return failures ? 1 : 0;
}
