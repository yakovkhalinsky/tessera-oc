/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The port's storage: the module's own flash, as a little filesystem - and
 * read only.
 *
 * The o_C has no SD card on this revision (the schematic has none), so the
 * projects live in the Teensy's program flash through LittleFS - the same
 * drive the o_C's own firmware uses for its settings (PhzConfig.cpp).
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
 * So the drive mounts (the mount only: it is never formatted, not even a
 * blank one) and is read. A write is refused before it starts: the write,
 * the erase and the sync of the drive all return LFS_ERR_IO, and the port's
 * own save says so.
 *
 * The geometry is the framework's own program-flash filesystem on the T4.0
 * (framework-arduinoteensy 1.162.0, LittleFS.cpp:911-957): FLASH_SIZE
 * 0x1F0000, SECTOR_SIZE 32768, the base at 0x60000000 + 0x1F0000 - 512 KiB =
 * 0x60170000, the blocks 32768 bytes and 16 of them, the read, the program
 * and the cache 128, the block cycles 800. The o_C's own firmware mounts the
 * same drive the same way (PhzConfig.cpp:26: 1024 * 512), so the port reads
 * the filesystem the module's own firmware wrote.
 *
 * If the mount fails (the drive is blank, or it holds a filesystem the port
 * cannot read), the storage stays off: the module runs, nothing is written,
 * and the project page says that there is no save. */

#include <Arduino.h>
#include <LittleFS.h>
#include "oc_port.h"

/* the drive: the program's flash, the last 512 KiB of the T4.0's, in 32 KiB
 * blocks (the framework's own geometry, as above) */
#define OC_FLASH_BASE 0x60170000UL
#define OC_FLASH_BLOCK 32768u
#define OC_FLASH_BLOCKS 16u

static_assert(OC_FLASH_BLOCK * OC_FLASH_BLOCKS == 512u * 1024u, "the drive is 512 KiB");
static_assert(OC_FLASH_BASE == 0x60170000UL, "the drive is at the T4.0's flash end");

/* the length of the firmware in the flash: the drive must be behind it */
extern unsigned long _flashimagelen;

/* the drive: the module's flash, read only */
class OcFlashRO : public LittleFS {
public:
    bool begin(void)
    {
        configured = false;
        if ((uint32_t)&_flashimagelen >= OC_FLASH_BASE - 0x60000000UL)
            return false;                   /* the firmware reaches the drive */
        memset(&lfs, 0, sizeof(lfs));
        memset(&config, 0, sizeof(config));
        config.context = (void *)(uintptr_t)OC_FLASH_BASE;
        config.read = &static_read;   config.prog = &static_prog;
        config.erase = &static_erase; config.sync = &static_sync;
        config.read_size = config.prog_size = config.cache_size = config.lookahead_size = 128;
        config.block_size = OC_FLASH_BLOCK;    config.block_count = OC_FLASH_BLOCKS;
        config.block_cycles = 800;              config.name_max = LFS_NAME_MAX;
        configured = true;
        if (lfs_mount(&lfs, &config) < 0)
            return false;                   /* no format: the drive is read only */
        mounted = true;
        return true;
    }

private:
    /* the read: the flash is memory mapped, so it is a copy */
    static int static_read(const struct lfs_config *c, lfs_block_t block,
                           lfs_off_t offset, void *buffer, lfs_size_t size)
    { memcpy(buffer, (const uint8_t *)c->context + block * c->block_size + offset, size); return 0; }
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
bool oc_storage_ok(void) { return fs_ok; }

/* the project's bytes into the slot: the drive is read only, so there is no
 * save. The error is the answer, and nothing is touched. */
int oc_save(uint32_t slot) { (void)slot; return 1; }

/* The reads: fs.open(name), the default mode, and nothing else.
 *
 * The framework's FILE_READ is 0 and FILE_WRITE is 1 (cores/teensy4/FS.h:37-38),
 * and littlefs's LFS_O_RDONLY is 1 (LittleFS/src/littlefs/lfs.h:127) - so
 * open(name, LFS_O_RDONLY) opens for WRITE: it creates the file if it is not
 * there and seeks to its end (LittleFS/src/LittleFS.h:278-296). The drive is
 * read only, so the mode is the default one: FILE_READ. */
int oc_load(uint32_t slot)
{
    File f = fs.open(slot_name(slot));
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
    File f = fs.open(LAST_NAME);
    uint8_t slot = 0;
    if (!f)
        return 0;
    if (f.read(&slot, 1) != 1)
        slot = 0;
    f.close();
    return slot & 3u;
}

/* the slot the module was last using: the drive is read only, so it is not
 * written. */
void oc_set_last_slot(uint32_t slot) { (void)slot; }

/* is there anything in the slot? */
int oc_slot_used(uint32_t slot)
{
    File f = fs.open(slot_name(slot));
    if (!f)
        return 0;
    f.close();
    return 1;
}
