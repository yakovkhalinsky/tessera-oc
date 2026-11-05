/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* The o_C port's engine in the FM-1's generator test.
 *
 * The port (firmware/oc) runs the same sequencer, the same generators, the same
 * harmony and the same cord matrix as the FM-1, with no audio path: the notes
 * go out as CV and as MIDI. So the port must play exactly what the FM-1 plays.
 * This test is the FM-1's own (tests/tessera_test.c) with the engine swapped
 * for the port's: the same scenarios, the same note log, the same hash and the
 * same baseline (tests/tessera_baseline.txt).
 *
 *   build/host/oc_engine_test tests/tessera_baseline.txt [LOGDIR]
 *
 * A failure here means the port's engine has drifted from the FM-1's: the
 * baseline is the FM-1's, and it must not change. */
#define OC_ENGINE 1
#include "../vendor/tessera/tests/tessera_test.c"
