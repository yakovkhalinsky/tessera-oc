/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* The o_C port's engine: the Tessera sequencer, the generators, the harmony and
 * the cord matrix, with no audio path.
 *
 * The time base is the sample at FS, exactly as on the FM-1, but nothing
 * renders it: the module's tick advances it (oc_clock.c), and the notes that
 * come out go to the rig as CV and as MIDI. The block cadence is the FM-1's as
 * well - one block every CTL samples, 1378 a second - so the modulation
 * sources and the sequencer see the same clock they always did.
 *
 * This file is the unity unit the device build and the host test include; the
 * platform below it provides the CV, the gate and the MIDI port. */
#include <stdint.h>
#include <string.h>              /* memset, memcpy: the host's and the Teensy's own */
#include "melodee_tables.h"      /* FS, CTL and the tables (generated: tools/gen_tables.py) */

/* ------------------------------------------------------------ clock --- */
/* The sample at FS, advanced by the module's tick. Nothing renders it; it is
 * the sequencer's and the modulation sources' time, and the MIDI's. */
static uint32_t oc_samples;               /* the samples since power-on */
static uint32_t oc_ticks(void) { return oc_samples; }
static uint32_t oc_ms(void) { return oc_samples * 1000u / FS; }

/* the C library's own memset / memcpy / memcmp, as the FM-1 build has them
 * (the device's own libc.c; here the host's, remapped so the two do not clash) */
#define memset melodee_memset
#define memcpy melodee_memcpy
#define memcmp melodee_memcmp
#include "libc.c"
#undef memset
#undef memcpy
#undef memcmp

/* ------------------------------------------------------- base, engine --- */
#include "oc_port.h"
#include "core.h"
#include "oc_hal.h"               /* the platform names the shared engine calls (the tick, the critical section) */
#include "fm6_core.h"            /* the FM6 patch layout (the parameter registry names it) */
#include "eng_oracle.h"          /* the ORACLE patch layout (the same) */
#include "eng_drum.h"            /* the DRUM lanes (the sequencer names their notes) */
#include "resources.c"            /* the working-memory arena (the engines' state; the port's engine has none) */
#include "dsp.c"                  /* the fixed-point helpers (mulq15 and the rest) */
#ifdef OC_HOST
/* the host's MIDI clock is in milliseconds (the FM-1's host build, usb.c's
 * fallback: the same units, so the same MIDI timing) */
#define MIDI_TIME_NOW() fm1_ms
#define MIDI_TICKS_PER_MS 1u
#else
#define MIDI_TIME_NOW() oc_ticks()
#define MIDI_TICKS_PER_MS 1000u
#endif
#include "../parts/oc_state.c"             /* the input, the bend, the inert engine state */
#include "../parts/oc_midi.c"              /* the MIDI queue (the module's USB MIDI) */
#include "../parts/oc_engines.c"           /* the engine table (the port's own engine; the rest retired) */
#include "params.c"
#include "mod.c"                  /* the modulation matrix (the LFO's SLEW, the note's VEL / KEY / RAND) */
#include "seqtime.c"              /* the time base: a beat, a division */
#include "seq_lfo.c"              /* the track's LFO and the part's voice mode */
#include "../parts/oc_out.c"               /* the CV out, the gate and the MIDI out */
#include "../parts/oc_voice.c"             /* the note layer (no voices: the note goes to the rig) */

/* ------------------------------------------------- MIDI, sequencer --- */
#include "tessera.h"
#include "gen.c"
#include "song_chain.c"
#ifdef TESSERA_NOTE_HOOK                       /* (the host test's note log: the same notes the FM-1's hostsim logs:
                                               * the SUB's repeats and a generator's notes, and the notes played
                                               * after the sequencer) */
static void TESSERA_NOTE_HOOK(track_t *t, uint32_t note, uint32_t vel);
#define SUB_NOTE_ON(t, n, v) (TESSERA_NOTE_HOOK((t), (n), (v)), trk_note_on((t), (n), (v)))
#endif
#include "seq.c"
#ifdef TESSERA_NOTE_HOOK
#define trk_note_on(t, n, v) (TESSERA_NOTE_HOOK((t), (n), (v)), trk_note_on(t, n, v))
#endif
#include "tessera.c"
#include "pid.c"                  /* the central parameter ids (pid.h) */
#include "cord.c"                 /* cords: the signal bus (cord.h) */
#ifdef TESSERA_NOTE_HOOK
#undef trk_note_on
#endif

/* ------------------------------------------------------------ clock --- */
/* One block: the sequencer's events, the modulation sources and the tracks'
 * LFOs. The FM-1's mix_block does the same and then renders the voices; here
 * there is nothing to render. */
static void events_block(uint32_t n);     /* seq.c */
static void oc_block(uint32_t n)
{
    events_block(n);
    cord_tick();                          /* the organic sources, ENERGY, DRIFT's clock */
    for (uint32_t i = 0; i < NTRK; i++)
        track_lfo_tick(&trk[i]);           /* the track's LFO, once a block */
}

/* The port's boot: the defaults and the empty patterns (as melodee_init on the
 * FM-1: song.g and the tracks' own parameters take their table's defaults). */
void oc_engine_init(void)
{
    uint32_t i, k;
    pattern_init();
    for (i = 0; i < G_COUNT; i++) song.g[i] = GP[i].def;
    for (k = 0; k < NTRK; k++) {
        for (i = 0; i < P_E0; i++) trk[k].p[i] = TP[i].def;
        for (i = 0; i < NSTEP; i++) trk[k].step[i].time = ST_REST;
    }
    song.master_q12 = 4096;
}

/* Is the transport running? */
uint8_t oc_playing(void) { return song.playing ? 1u : 0u; }

/* The transport: the platform's buttons (the UI comes later). */
void oc_transport(uint8_t request)
{
    transport_req = request;
}

/* The clock: the samples that passed. The blocks keep the FM-1's cadence (one
 * every CTL samples), so the engine sees the same clock it always did. */
static uint32_t oc_block_acc;            /* the samples not yet a whole block */
void oc_clock_advance(uint32_t samples)
{
    oc_samples += samples;
    oc_block_acc += samples;
    while (oc_block_acc >= CTL) {
        oc_block_acc -= CTL;
        oc_block(CTL);
    }
}

/* the sample rate the engine counts at */
uint32_t oc_sample_rate(void) { return FS; }
