/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The port's storage: the module's own flash, as a little filesystem.
 *
 * The o_C has no SD card on this revision (the schematic has none, and the
 * T4.0's own flash is what there is), so the projects live in the Teensy's
 * program flash through LittleFS - the same drive the o_C's own firmware uses
 * for its settings (PhzConfig.cpp).
 *
 * One file a slot, written whole: the port's project is a few kilobytes, so a
 * file is written in one go and the little filesystem writes only the blocks
 * that changed.
 *
 * The write blocks for as long as the flash takes, so a save happens with the
 * transport stopped (the engine's own rule: a save silences the sequencer). */

#include <Arduino.h>
#include <LittleFS.h>
#include "oc_port.h"

/* the drive: the Teensy's program flash (the maximum is 960 blocks of 1024) */
static constexpr uint32_t DISK_SIZE = 1024 * 512;
static LittleFS_Program fs;

/* the project's own buffer: the engine packs into it */
static uint8_t project_buf[16384];

static const char *slot_name(uint32_t slot)
{
    static char name[16];
    snprintf(name, sizeof name, "/p%u.toc", (unsigned)(slot & 3u));
    return name;
}

bool oc_storage_init(void)
{
    return fs.begin(DISK_SIZE);
}

/* the project's bytes into the slot */
int oc_save(uint32_t slot)
{
    uint32_t len = oc_project_pack(project_buf, sizeof project_buf);
    File f;
    if (!len)
        return 1;
    f = fs.open(slot_name(slot), LFS_O_WRONLY | LFS_O_TRUNC | LFS_O_CREAT);
    if (!f)
        return 2;
    size_t written = f.write(project_buf, len);
    f.close();
    return written == len ? 0 : 2;
}

/* the slot's bytes into the project */
int oc_load(uint32_t slot)
{
    File f = fs.open(slot_name(slot), LFS_O_RDONLY);
    size_t len;
    int ok;
    if (!f)
        return 2;
    len = f.read(project_buf, sizeof project_buf);
    f.close();
    if (!len || len > sizeof project_buf)
        return 2;
    ok = oc_project_unpack(project_buf, (uint32_t)len);
    return ok ? 0 : 1;
}

/* the slot the module was last using: so it comes back as it was left */
static const char *LAST_NAME = "/last";

int oc_last_slot(void)
{
    File f = fs.open(LAST_NAME, LFS_O_RDONLY);
    uint8_t slot = 0;
    if (!f)
        return 0;
    if (f.read(&slot, 1) != 1)
        slot = 0;
    f.close();
    return slot & 3u;
}

void oc_set_last_slot(uint32_t slot)
{
    File f = fs.open(LAST_NAME, LFS_O_WRONLY | LFS_O_TRUNC | LFS_O_CREAT);
    uint8_t s = (uint8_t)(slot & 3u);
    if (!f)
        return;
    f.write(&s, 1);
    f.close();
}

/* is there anything in the slot? */
int oc_slot_used(uint32_t slot)
{
    File f = fs.open(slot_name(slot), LFS_O_RDONLY);
    if (!f)
        return 0;
    f.close();
    return 1;
}
