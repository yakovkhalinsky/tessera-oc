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

/* The parameters the port's screen shows, seen from outside. The engine keeps
 * the descriptors (the label, the range, the format, the names) and the
 * values; the UI only reads and writes them. */
const param_desc_t *oc_tx_desc(uint32_t tx)
{
    return tx < TX_COUNT ? &TX_DESC[tx] : 0;
}

const param_desc_t *oc_track_desc(uint32_t id)
{
    return id < P_COUNT ? &TP[id] : 0;
}

const char *oc_param_name(uint32_t pid)
{
    const pid_entry_t *e = pid_find(pid);
    return e ? e->name : "";
}

void oc_param_format(const param_desc_t *d, int32_t v, char *val, const char **unit)
{
    if (!d) {
        str_cpy(val, "-", 8);
        *unit = "";
        return;
    }
    param_format(d, v, val, unit);
}

int32_t oc_tx_get(uint32_t tr, uint32_t tx)
{
    return tessera_param_get(tr, tx);
}

void oc_tx_set(uint32_t tr, uint32_t tx, int32_t v)
{
    tessera_param_set(tr, tx, v);
}

int32_t oc_track_param_get(uint32_t tr, uint32_t id)
{
    return tr < NTRK ? trk[tr].p[id] : 0;
}

void oc_track_param_set(uint32_t tr, uint32_t id, int32_t v)
{
    if (tr < NTRK)
        trk[tr].p[id] = (int16_t)clamp(v, TP[id].min, TP[id].max);
}

/* The rows the port's screen shows. The ids are the engine's, so the port does
 * not have to know them: it asks for the list and shows what comes back. */
static const uint8_t GEN_NONE[] = {TX_MODE};
static const uint8_t GEN_TURING[] = {TX_MODE, TX_TUR_LEN, TX_TUR_PROB, TX_TUR_SPAN};
static const uint8_t GEN_EUCLID[] = {TX_MODE, TX_EUC_STEPS, TX_EUC_PULSES, TX_EUC_ROT};
static const uint8_t GEN_MARKOV[] = {TX_MODE, TX_MKV_LEAP, TX_MKV_DENS, TX_MKV_SPAN};
static const uint8_t GEN_MORPH[] = {TX_MODE, TX_MRP_POS, TX_MRP_SNPA, TX_MRP_SNPB};
static const uint8_t GEN_GRIDS[] = {TX_MODE, TX_GRD_X, TX_GRD_Y, TX_GRD_DENS};
static const uint8_t GEN_BOUNCE[] = {TX_MODE, TX_BNC_SPEED, TX_BNC_ANGLE, TX_BNC_BALLS};
static const uint8_t GEN_AWAKE[] = {TX_MODE, TX_AWK_LENA, TX_AWK_LENB, TX_AWK_PROB};
static const uint8_t GEN_POLY[] = {TX_MODE, TX_POL_DIV1, TX_POL_DIV2, TX_POL_SPAN};
static const uint8_t GEN_QUAD[] = {TX_MODE, TX_QD_DIR, TX_QD_PROB, TX_QD_LINK};
static const uint8_t GEN_CONSTEL[] = {TX_MODE, TX_CS_Y, TX_CS_DENS, TX_CS_SIZE};
static const uint8_t GEN_SERIES[] = {TX_MODE, TX_SR_START, TX_SR_INTV, TX_SR_LOOP};
static const uint8_t GEN_CHORD[] = {TX_MODE, TX_CH_PROG, TX_CH_BARS, TX_CH_HIT};
static const uint8_t GEN_ANSWER[] = {TX_MODE, TX_AN_VARY, TX_AN_ECHO, TX_AN_FADE};

const uint8_t *oc_gen_rows(uint32_t gen, uint8_t *count)
{
    static const struct {
        const uint8_t *ids;
        uint8_t count;
    } ROWS[] = {
        {GEN_NONE, 1}, {GEN_TURING, 4}, {GEN_EUCLID, 4}, {GEN_MARKOV, 4}, {GEN_MORPH, 4},
        {GEN_GRIDS, 4}, {GEN_BOUNCE, 4}, {GEN_AWAKE, 4}, {GEN_POLY, 4}, {GEN_QUAD, 4},
        {GEN_CONSTEL, 4}, {GEN_SERIES, 4}, {GEN_CHORD, 4}, {GEN_ANSWER, 4},
    };
    if (gen >= NELEM(ROWS))
        gen = 0;
    *count = ROWS[gen].count;
    return ROWS[gen].ids;
}

/* the track's key, scale, transpose, quantize and its octave on the CV */
static const uint8_t SCL_ROWS[] = {P_ROOT, P_SCALE, P_TRANS, P_QUANT, P_E0};
const uint8_t *oc_scl_rows(uint8_t *count)
{
    *count = NELEM(SCL_ROWS);
    return SCL_ROWS;
}

/* the track's generator */
void oc_set_generator(uint32_t tr, int32_t mode)
{
    if (mode < 0)
        mode = 0;
    if (mode > 13)
        mode = 13;
    tessera_param_set(tr, TX_MODE, mode);
}
