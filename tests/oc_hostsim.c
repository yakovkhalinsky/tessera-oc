/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* The o_C port's engine on the host: the same environment tests/hostsim.c gives
 * the FM-1 build (the tables, the defaults, the block driver), but the port's
 * engine - no audio path, the notes go out as CV and MIDI.
 *
 * The port's engine is one unit (firmware/oc/oc_engine.c), so the shared
 * generator test can drive it unchanged: the block driver answers to the FM-1's
 * name (mix_block) and the note log's hook is called from the port's note
 * layer. */
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define OC_HOST 1                                /* the host build (the MIDI clock in milliseconds) */
#define TESS_ENTROPY() 0x5EEDu                     /* (a fresh Turing register: fixed on the host, repeatable tests) */

#include "../firmware/oc_engine.c"

/* the block driver, under the FM-1's name: the shared harness calls it once a
 * block, and the port's clock advances by the same 32 samples */
static void mix_block(int32_t *out, uint32_t n)
{
    (void)out;
    oc_clock_advance(n);
}

/* the platform's side on the host: the CV out and the gate are recorded, not
 * written to a DAC */
static uint32_t oc_cv_last[NPART];                   /* the last code on each CV out */
static uint32_t oc_gate_state[NPART];               /* the gate's state */
static uint32_t oc_gate_edges;                      /* the gate's changes, all the outs together */
void oc_cv_write(uint32_t ch, uint32_t note)
{
    if (ch < NPART) oc_cv_last[ch] = note;
}
void oc_gate_write(uint32_t ch, uint32_t on)
{
    if (ch < NPART && oc_gate_state[ch] != (on ? 1u : 0u)) {
        oc_gate_state[ch] = on ? 1u : 0u;
        oc_gate_edges++;
    }
}

/* the diagnostics: the host has no cycle counter, and the block's cost is
 * the host's business, not the module's */
uint32_t oc_diag_cyccnt(void) { return 0; }
void oc_diag_block(uint32_t cycles) { (void)cycles; }

/* a step written by the test */
static void put_step(track_t *t, uint32_t i, uint32_t n, const uint8_t *notes, uint32_t time, uint32_t flags)
{
    step_t *s = &t->step[i];
    uint32_t k;
    s->n = (uint8_t)n;
    for (k = 0; k < 4u; k++)
        s->note[k] = k < n ? notes[k] : 0;
    s->time = (uint8_t)time;
    s->flags = (uint8_t)flags;
    s->vel = n ? 100 : 0;
}

/* t's notes started since the note clock (vage) was a0 (the FM-1 counts the
 * voices that started; the port has none, only notes) */
static uint32_t started_since(const track_t *t, uint32_t a0)
{
    return oc_note_at[oc_part(t)] > a0;
}

/* the defaults and the empty patterns (as melodee_init on the FM-1) */
static void host_tracks_init(void)
{
    oc_engine_init();
}

/* the port has no demo renderer: the test's own main follows */
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return 0;
}
