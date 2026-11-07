/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The port's engine on the host, for tools/render_screens: the same unit the
 * host tests build (as tests/oc_hostsim.c does), so the screens show the
 * engine's own defaults: its tempo, its generators, its names. */
#define OC_HOST 1
#define TESS_ENTROPY() 0x5EEDu

#include "../../firmware/oc_engine.c"

/* the platform's side, on the host: nothing is written anywhere */
void oc_cv_write(uint32_t ch, uint32_t note) { (void)ch; (void)note; }
void oc_gate_write(uint32_t ch, uint32_t on) { (void)ch; (void)on; }
