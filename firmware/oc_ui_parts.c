/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The UI's pure-C parts (parts/), built into the firmware as one unit, the way
 * the engine is (firmware/oc_engine.c): the source directory is firmware/, and
 * the host tests build the same files on their own (tools/build_host.sh). */
#include "../parts/oc_ui_input.c"
#include "../parts/oc_ui_nav.c"
#include "../parts/oc_ui_gfx.c"
#include "../parts/oc_ui_screen.c"
#include "../parts/oc_ui_overview.c"
