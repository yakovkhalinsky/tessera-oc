/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* The o_C port's platform names.
 *
 * The shared engine calls the FM-1's hardware layer by its own names (the
 * press statistics in seq.c, the critical sections around a step edit). Here
 * they are the module's own: the tick that advances the sequencer's clock, and
 * the module's critical section.
 *
 * The port's clock is the sample at FS (oc_engine.c): the same unit the FM-1's
 * sequencer counts in, so nothing in the engine changes. */
#ifndef OC_HAL_H_
#define OC_HAL_H_

/* the module's tick, in samples at FS since power-on */
#define fm1_ticks() oc_ticks()

/* the critical section: the port's sequencer runs in the main loop and the
 * module's tick advances the clock from the timer interrupt, so the two must
 * not read a half-updated state. The Teensy's own primitives (Phase 2). */
static inline void oc_irq_on(void) { }
static inline void oc_irq_off(void) { }
#define fm1_irq_on() oc_irq_on()
#define fm1_irq_off() oc_irq_off()

#endif
