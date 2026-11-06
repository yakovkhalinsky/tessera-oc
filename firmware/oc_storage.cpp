/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The port's storage: the module's own flash, as a little filesystem - and
 * read only.
 *
 * The o_C has no SD card on this revision (the schematic has none, and the
 * T4.0's own flash is what there is), so the projects live in the Teensy's
 * program flash through LittleFS - the same drive the o_C's own firmware uses
 * for its settings (PhzConfig.cpp).
 *
 * v0.1.2: the drive is READ ONLY. The firmware never writes the flash:
 *
 *   - the o_C's own firmware owns that flash (its settings and its presets
 *     live there), and a program flash that is written behind its back is a
 *     program flash that gets lost;
 *   - a write path is a write path: it is the one thing that can turn a
 *     firmware into a brick, and the port does not need it (the projects are
 *     in the engine's own format, and a save is a nicety, not a feature);
 *   - the security review (ZC, issuecomment-6677339063) asks for a fail
 *     closed storage: it must be read only, and a mount that fails must
 *     leave the module running, not formatting anything.
 *
 * So the drive mounts (the mount only: the drive is never formatted, not even
 * a blank one) and is read. A write is refused before it starts: the write,
 * the erase and the sync of the drive all return LFS_ERR_IO, and the port's
 * own save says so.
 *
 * The geometry is the o_C's own (the T4.0's program-flash drive, the same one
 * Phazerville uses): the base is the program's flash at
 * 0x60000000 + 0x1F0000 (the T4.0's flash size) less the 512 KiB the drive
 * takes, the blocks are 4 KiB, and there are 128 of them.
 *
 * If the mount fails (the drive is blank, or it holds a filesystem the port
 * cannot read), the storage stays off: the module runs, the screen says so,
 * and nothing is written. */

#include <Arduino.h>
#include <LittleFS.h>
#include "oc_port.h"

/* the drive: the program's flash, the last 512 KiB of the T4.0's (0x60000000
 * + 0x1F0000 - 512K), in 4 KiB blocks */
#define OC_FLASH_START 0x60000000UL
#define OC_FLASH_BASE 0x60170000UL
#define OC_FLASH_BLOCK 4096u
#define OC_FLASH_BLOCKS 128u

/* the length of the firmware in the flash: the drive must be behind it */
extern unsigned long _flashimagelen;

/* the drive: the module's flash, read only */
class OcFlashRO : public LittleFS {
public:
    bool begin(void)
    {
        configured = false;
        if ((uint32_t)&_flashimagelen >= OC_FLASH_BASE - OC_FLASH_START)
            return false;                   /* the firmware reaches the drive */
        memset(&lfs, 0, sizeof(lfs));
        memset(&config, 0, sizeof(config));
        config.context = (void *)(uintptr_t)OC_FLASH_BASE;
        config.read = &static_read;
        config.prog = &static_prog;
        config.erase = &static_erase;
        config.sync = &static_sync;
        config.read_size = config.prog_size = config.cache_size = config.lookahead_size = 128;
        config.block_size = OC_FLASH_BLOCK;
        config.block_count = OC_FLASH_BLOCKS;
        config.block_cycles = 800;
        config.name_max = LFS_NAME_MAX;
        configured = true;
        if (lfs_mount(&lfs, &config) < 0)
            return false;                   /* no format: the drive is read only */
        mounted = true;
        return true;
    }
    const char *getMediaName() { return (const char *)F("PROGRAM (RO)"); }

private:
    /* the read: the flash is memory mapped, so it is a copy */
    static int static_read(const struct lfs_config *c, lfs_block_t block,
                           lfs_off_t offset, void *buffer, lfs_size_t size)
    {
        const uint8_t *p = (const uint8_t *)c->context + block * c->block_size + offset;
        memcpy(buffer, p, size);
        return 0;
    }
    /* the writes: there are none. The drive is read only. */
    static int static_prog(const struct lfs_config *, lfs_block_t, lfs_off_t,
                           const void *, lfs_size_t) { return LFS_ERR_IO; }
    static int static_erase(const struct lfs_config *, lfs_block_t) { return LFS_ERR_IO; }
    static int static_sync(const struct lfs_config *) { return LFS_ERR_IO; }
};

static OcFlashRO fs;
static bool fs_ok;

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
    fs_ok = fs.begin();
    return fs_ok;
}

/* is the drive there? (the screen says so when it is not) */
bool oc_storage_ok(void)
{
    return fs_ok;
}

/* the project's bytes into the slot: the drive is read only, so there is no
 * save. The error is the answer, and nothing is touched. */
int oc_save(uint32_t slot)
{
    (void)slot;
    return 1;
}

/* the slot's bytes into the project */
int oc_load(uint32_t slot)
{
    File f;
    size_t len;
    if (!fs_ok)
        return 2;                           /* not found: the drive is off */
    f = fs.open(slot_name(slot), LFS_O_RDONLY);
    if (!f)
        return 2;
    len = f.read(project_buf, sizeof project_buf);
    f.close();
    if (!len || len > sizeof project_buf)
        return 2;
    return oc_project_unpack(project_buf, (uint32_t)len) ? 0 : 1;
}

/* the slot the module was last using: so it comes back as it was left */
static const char *LAST_NAME = "/last";

int oc_last_slot(void)
{
    File f;
    uint8_t slot = 0;
    if (!fs_ok)
        return 0;
    f = fs.open(LAST_NAME, LFS_O_RDONLY);
    if (!f)
        return 0;
    if (f.read(&slot, 1) != 1)
        slot = 0;
    f.close();
    return slot & 3u;
}

/* the slot the module was last using: the drive is read only, so it is not
 * written. The error is the answer. */
int oc_set_last_slot(uint32_t slot)
{
    (void)slot;
    return 1;
}

/* is there anything in the slot? */
int oc_slot_used(uint32_t slot)
{
    File f;
    if (!fs_ok)
        return 0;
    f = fs.open(slot_name(slot), LFS_O_RDONLY);
    if (!f)
        return 0;
    f.close();
    return 1;
}
