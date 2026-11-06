/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The port's version: the release it is, and the build it is (the board's own
 * suffix comes from the build flags: OC_VERSION_EXTRA is "_T40" on the Teensy
 * 4.0). The screen shows it on the splash, and the release notes carry it. */
#ifndef OC_VERSION_H_
#define OC_VERSION_H_

#define OC_VERSION "0.1.2"

#ifndef OC_VERSION_EXTRA
#define OC_VERSION_EXTRA ""
#endif

/* the version as the screen shows it: "0.1.2_T40" */
#define OC_VERSION_STRING OC_VERSION OC_VERSION_EXTRA

#endif
