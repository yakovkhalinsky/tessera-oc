/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* What the port's screen shows: the engine's state, seen from outside.
 *
 * The UI is the port's own (the o_C idiom: two encoders, four buttons, a small
 * screen), so it reads the engine through these and writes through the ones
 * beside them. */
uint8_t oc_selected_track(void)
{
    return song.sel;
}

void oc_select_track(uint8_t track)
{
    song.sel = track;
}

uint8_t oc_track_generator(uint32_t track)
{
    if (track >= TESSERA_TRACKS)
        return TESS_ENG_OFF;
    return tessera.mode[track];
}

int32_t oc_track_octave(uint32_t track)
{
    if (track >= NTRK)
        return 0;
    return trk[track].p[P_E0];
}

int16_t oc_tempo(void)
{
    return song.g[G_BPM];
}

void oc_set_tempo(int16_t bpm)
{
    song.g[G_BPM] = (int16_t)clamp(bpm, 40, 240);
}
