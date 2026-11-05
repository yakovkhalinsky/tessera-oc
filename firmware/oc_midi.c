/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* The o_C port's MIDI layer.
 *
 * The queue is the FM-1's own shape (usb.c): four-byte packets, the source's
 * clock count kept with every packet so that a MIDI clock's tempo survives a
 * note burst that overflowed the queue, and an overflow flag that makes the
 * sequencer release the notes rather than leave them hanging.
 *
 * The module's MIDI out is USB (the o_C's DIN jacks are the T4.1's; the T4.0's
 * are free pins, so a DIN out is a wire away - Phase 2). The port's clock is
 * the sample at FS (oc_ticks), the same unit the sequencer counts in, so the
 * MIDI timing is the FM-1's. */
#define MQ 64u
static uint32_t midi_in_q[MQ], midi_out_q[MQ];
static uint32_t midi_in_ms[MQ];
static uint32_t midi_in_time[MQ];
static uint8_t midi_in_source[MQ];                    /* 1 USB, 2 TRS */
static uint16_t midi_in_pulse[MQ], midi_rx_pulses[2];
static volatile uint32_t mi_w, mi_r, mo_w, mo_r;
static volatile uint8_t midi_in_overflow;            /* the sequencer releases the notes */
static uint32_t midi_render_time;
static uint8_t midi_render_timed;                    /* the block's timeline (the port: none) */

/* the module's millisecond clock, since boot */
static volatile uint32_t fm1_ms;

/* a MIDI packet in: the status byte, the two data bytes and the source */
static int midi_enqueue(uint32_t pkt, uint32_t source)
{
    uint32_t at = mi_w % MQ;
    uint32_t clock_source = source == 2u;
    if (((pkt >> 8) & 0xFFu) == 0xF8u)
        midi_rx_pulses[clock_source]++;
    if (midi_in_overflow)
        return 0;
    if (mi_w - mi_r >= MQ) {
        midi_in_overflow = 1;
        return 0;
    }
    midi_in_q[at] = pkt;
    midi_in_ms[at] = fm1_ms;
    midi_in_time[at] = MIDI_TIME_NOW();
    midi_in_source[at] = (uint8_t)source;
    midi_in_pulse[at] = midi_rx_pulses[clock_source];
    RING_PUBLISH();
    mi_w++;
    return 1;
}

/* a MIDI packet out: the queue the platform drains */
static void midi_out_event(uint32_t pkt)
{
    if (mo_w - mo_r < MQ) {
        midi_out_q[mo_w % MQ] = pkt;
        RING_PUBLISH();
        mo_w++;
    }
}
