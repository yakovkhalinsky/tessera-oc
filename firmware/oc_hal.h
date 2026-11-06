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

#include <stdint.h>

/* the module's tick, in samples at FS since power-on */
#define fm1_ticks() oc_ticks()

/* The critical section: the port's sequencer runs in the main loop and the
 * module's tick advances the clock from the timer interrupt, so the two must
 * not read a half-updated state. The FM-1's own primitives are a plain
 * cli/sti (hal/fm1_irq.h), and so are these: the interrupts off, and back on
 * again.
 *
 * The engine calls these from the main loop (the knob handlers, the undo
 * levels, the cord adoption): the tick's own code does not, so the section is
 * never taken from inside the interrupt, where turning the interrupts back on
 * would let the tick nest in itself.
 *
 * The host build (the tests) has no interrupts at all: the section is empty
 * there, as the FM-1's own host build has it. */
#ifdef OC_HOST
static inline void oc_irq_off(void) { }
static inline void oc_irq_on(void) { }
#else
static inline void oc_irq_off(void) { __asm__ volatile("cpsid i" ::: "memory"); }
static inline void oc_irq_on(void) { __asm__ volatile("cpsie i" ::: "memory"); }
#endif
#define fm1_irq_on() oc_irq_on()
#define fm1_irq_off() oc_irq_off()

#endif
