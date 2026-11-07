/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The UI's screen router (oc_ui_nav.h). UI-PLAN.md §2, §3.2. */
#include "oc_ui_nav.h"

static const uint8_t MENU[UI_MENU_ITEMS] = {
    UI_SCR_OVERVIEW, UI_SCR_PERFORM, UI_SCR_TRACK, UI_SCR_PROJECT, UI_SCR_INFO};

uint32_t ui_nav_menu_item(uint32_t index)
{
    return index < UI_MENU_ITEMS ? MENU[index] : UI_SCR_OVERVIEW;
}

void ui_nav_init(ui_nav_t *nav)
{
    uint32_t i;
    uint8_t *p = (uint8_t *)nav;
    for (i = 0; i < sizeof *nav; i++)
        p[i] = 0;
    nav->stack[0] = UI_SCR_OVERVIEW;
    nav->depth = 1;
    nav->rows[UI_SCR_MENU] = UI_MENU_ITEMS;
}

void ui_nav_rows(ui_nav_t *nav, uint32_t screen, uint32_t rows)
{
    if (screen >= UI_SCR_COUNT)
        return;
    nav->rows[screen] = (uint8_t)(rows > 255u ? 255u : rows);
    if (nav->cursor[screen] >= nav->rows[screen])
        nav->cursor[screen] = nav->rows[screen] ? (uint8_t)(nav->rows[screen] - 1u) : 0u;
}

uint32_t ui_nav_screen(const ui_nav_t *nav)
{
    return nav->stack[nav->depth - 1u];
}

/* a new screen on top; a full stack replaces its top (depth stays bounded) */
static void nav_open(ui_nav_t *nav, uint8_t screen)
{
    if (screen == UI_SCR_OVERVIEW) {      /* the root: back to depth 1 */
        nav->depth = 1;
        return;
    }
    if (ui_nav_screen(nav) == screen)
        return;
    if (nav->depth < UI_NAV_DEPTH)
        nav->depth++;
    nav->stack[nav->depth - 1u] = screen;
}

void ui_nav_open(ui_nav_t *nav, uint32_t screen)
{
    if (screen < UI_SCR_COUNT)
        nav_open(nav, (uint8_t)screen);
}

static void nav_back(ui_nav_t *nav)
{
    if (nav->depth > 1u)                  /* BACK at the root does nothing */
        nav->depth--;
}

/* the cursor wraps; a screen with no rows has nowhere to go */
static void nav_move(ui_nav_t *nav, uint32_t screen, int32_t n)
{
    int32_t rows = nav->rows[screen];
    int32_t c;
    if (rows == 0)
        return;
    c = ((int32_t)nav->cursor[screen] + n % rows + rows) % rows;
    nav->cursor[screen] = (uint8_t)c;
}

static ui_action_t act(uint8_t kind, const ui_event_t *ev)
{
    ui_action_t a;
    a.kind = kind;
    a.n = ev->n;
    a.dt = ev->dt;
    a.fine = ev->fine;
    return a;
}

ui_action_t ui_nav_event(ui_nav_t *nav, const ui_event_t *ev)
{
    uint32_t scr = ui_nav_screen(nav);
    switch (ev->src) {
    case UI_SRC_TOP:
        return act(ev->kind == UI_EV_DOWN ? UI_ACT_PLAY_STOP : UI_ACT_NONE, ev);
    case UI_SRC_BOT:
        /* BOTTOM acts only at its TAP; DOWN and HOLD_* mean nothing here */
        return act(ev->kind == UI_EV_TAP ? UI_ACT_RESTART : UI_ACT_NONE, ev);
    case UI_SRC_CHORD_TB:
        if (scr == UI_SCR_PERFORM)
            nav_back(nav);
        else
            nav_open(nav, UI_SCR_PERFORM);
        return act(UI_ACT_NONE, ev);
    case UI_SRC_ENC_L:
        if (ev->kind == UI_EV_SHIFT_TURN)
            return act(UI_ACT_SELECT_TRACK, ev);
        if (ev->kind == UI_EV_TURN)
            nav_move(nav, scr, ev->n);        /* never the track: BOTTOM + L does that */
        return act(UI_ACT_NONE, ev);
    case UI_SRC_ENC_R:
        if (ev->kind == UI_EV_SHIFT_TURN)
            return act(scr == UI_SCR_PERFORM ? UI_ACT_MACRO : UI_ACT_TEMPO, ev);   /* PERFORM: EVOLVE */
        if (ev->kind == UI_EV_TURN && scr != UI_SCR_MENU)
            return act(UI_ACT_VALUE, ev);
        return act(UI_ACT_NONE, ev);
    case UI_SRC_PUSH_L:
        if (ev->kind == UI_EV_LONG && scr != UI_SCR_MENU)
            nav_open(nav, UI_SCR_MENU);
        else if (ev->kind == UI_EV_TAP)
            nav_back(nav);
        return act(UI_ACT_NONE, ev);
    case UI_SRC_PUSH_R:
        if (scr == UI_SCR_MENU) {
            if (ev->kind == UI_EV_TAP) {
                nav_back(nav);
                nav_open(nav, MENU[nav->cursor[UI_SCR_MENU] % UI_MENU_ITEMS]);
            }
            return act(UI_ACT_NONE, ev);
        }
        if (ev->kind == UI_EV_TAP && ev->shift && scr == UI_SCR_TRACK) {
            nav->tab = (uint8_t)((nav->tab + 1u) % UI_TAB_COUNT);   /* next tab */
            nav->cursor[UI_SCR_TRACK] = 0;
            return act(UI_ACT_NONE, ev);
        }
        if (ev->kind == UI_EV_TAP)
            return act(ev->shift ? UI_ACT_SECONDARY : UI_ACT_ENTER, ev);
        if (ev->kind == UI_EV_LONG)
            return act(UI_ACT_CONTEXT, ev);
        return act(UI_ACT_NONE, ev);
    default:
        return act(UI_ACT_NONE, ev);
    }
}
