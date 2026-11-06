/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The port's project: what is saved and what is loaded.
 *
 * The engine's own pack does the hard part - the generators' phrases, the
 * cords, the sections, the seeds (tessera_pack) - and the rest is small: the
 * song's settings, the tracks' parameters and their steps.
 *
 * The format:
 *   'T','O','C','1'   the magic
 *   version, flags    (version 1)
 *   length            the payload's bytes
 *   crc32             the payload's
 *   the payload: the Tessera state, the globals, the tracks' parameters, the steps
 *
 * The port's own format, not the FM-1's TSP1: that one carries the synths'
 * sounds and the project slots, and the port has neither. */
#include "step_snapshot.h"     /* the steps, compactly (the engine's) */

#define OC_PROJ_MAGIC 0x31434F54u          /* "TOC1" */
#define OC_PROJ_VERSION 1u
#define OC_PROJ_MAX 16384u

static uint32_t oc_crc32(const uint8_t *p, uint32_t n)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < n; i++) {
        crc ^= p[i];
        for (uint32_t b = 0; b < 8u; b++)
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1u)));
    }
    return ~crc;
}

/* the state, packed. The engine's own pack first (the generators, the cords,
 * the sections), then the globals, the tracks' parameters and the steps. */
uint32_t oc_project_pack(uint8_t *out, uint32_t max)
{
    uint32_t n = 0, crc;
    if (max < 16u)
        return 0;
    out[0] = 'T';
    out[1] = 'O';
    out[2] = 'C';
    out[3] = '1';
    out[4] = (uint8_t)OC_PROJ_VERSION;
    out[5] = 0;
    /* the payload */
    n = tessera_pack(out + 16);
    if (!n || 16u + n + sizeof song.g + NTRK * P_COUNT * 2 + NTRK * NSTEP * sizeof(step_snapshot_t) > max)
        return 0;
    memcpy(out + 16 + n, song.g, sizeof song.g);
    n += sizeof song.g;
    for (uint32_t t = 0; t < NTRK; t++) {
        memcpy(out + 16 + n, trk[t].p, P_COUNT * 2);
        n += P_COUNT * 2;
    }
    for (uint32_t t = 0; t < NTRK; t++) {
        for (uint32_t s = 0; s < NSTEP; s++)
            step_snapshot_save((step_snapshot_t *)(out + 16 + n) + s, &trk[t].step[s]);
        n += NSTEP * sizeof(step_snapshot_t);
    }
    crc = oc_crc32(out + 16, n);
    memcpy(out + 8, &n, 4);
    memcpy(out + 12, &crc, 4);
    return 16u + n;
}

/* the state, loaded. Nothing changes unless the whole thing is good. */
int oc_project_unpack(const uint8_t *in, uint32_t len)
{
    uint32_t n = 0, crc, want;
    uint32_t tess_n;
    if (len < 16u)
        return 0;
    if (in[0] != 'T' || in[1] != 'O' || in[2] != 'C' || in[3] != '1')
        return 0;
    if (in[4] != OC_PROJ_VERSION)
        return 0;
    memcpy(&n, in + 8, 4);
    memcpy(&crc, in + 12, 4);
    if (n + 16u != len)
        return 0;
    if (oc_crc32(in + 16, n) != crc)
        return 0;

    tess_n = TESSERA_PK_BYTES;
    if (n < tess_n)
        return 0;
    if (!tessera_unpack(in + 16))
        return 0;
    want = 16u;
    memcpy(song.g, in + want, sizeof song.g);
    want += sizeof song.g;
    for (uint32_t t = 0; t < NTRK; t++) {
        memcpy(trk[t].p, in + want, P_COUNT * 2);
        want += P_COUNT * 2;
    }
    for (uint32_t t = 0; t < NTRK; t++) {
        for (uint32_t s = 0; s < NSTEP; s++)
            trk[t].step[s] = step_snapshot_get((const step_snapshot_t *)(in + want) + s);
        want += NSTEP * sizeof(step_snapshot_t);
    }
    return 1;
}
