/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Obsimo (OSR Records) */
/* OSR-1 app: input, pages, chord grids, the screen, settings. Part of the unity build (device/unity.c,
 * host/plat_host.c), after the platform: gfx.c and announce.c are in scope. The sound is dsp/engine.c.
 *
 * Controls: one action per control
 *   all 27 keys     chords of the grid (same note name, same chord; C5 = the grid's 8th chord),
 *                   white keys safe, black keys spices; below C4 an octave down, above C5 an octave up
 *   SELECT grid   ALGORITHM rhythm (OFF = the chord held)   PRESET sound
 *   K1..K4          the page's four settings, always shown at the bottom of the screen
 *   page buttons    HOME  cutoff, glide, resonance, sidechain      (HOME held: the help screen)
 *                   FX    delay, tremolo, tape, reverb
 *                   SCL   chords: key, voicing, richness, strum
 *                   ENV   groove: BPM, swing, density, gate
 *                   GLO   drums: pattern, kick, hi-hat, rim
 *                   SEQ   bass: sound, level, motif, filter
 *                   EDIT  decor: creature, eyes, background, mood
 *   ARP latch   PLAY drums on/off (kick, and its sidechain)   LFO (hold) variation   OCT- / OCT+ octave
 *   SAVE, REC free.  Settings save themselves a few seconds after the last change. */
#include "fastmath.h"
#include "engine.h"
#include "data.h"
#include "patterns.h"

/* ------------------------------------------------------------------ settings --- */
#define SET_MAGIC 0x4F535233u            /* "OSR3" (v3 layout: pages) */
typedef struct {
    uint32_t magic;
    uint8_t grid, key, rich, voicing;
    int8_t octave;
    uint8_t sound, rhythm, latch, drums_on, page;
    uint8_t cutoff, glide, reso, side;          /* HOME */
    uint8_t delay, trem, tape, reverb;          /* FX */
    uint8_t strum;                              /* chords (with key, voicing, richness) */
    uint8_t bpm, swing, dens, gate;             /* groove (density: 50 = as written) */
    uint8_t dpat, dkick, dhat, drim;            /* drums */
    uint8_t bsound, blevel, bmotif, bcut;       /* bass */
    uint8_t creature, eyes, decor, mood;        /* decor */
    uint8_t rsv[3];
} settings_t;
static settings_t set;
static uint8_t set_dirty;
static uint32_t dirty_at;             /* when the settings last changed (auto-save)*/

static void settings_defaults(void)
{
    memset(&set, 0, sizeof set);
    set.magic = SET_MAGIC;
    set.cutoff = 72; set.glide = 17; set.reso = 20; set.side = 55;
    set.delay = 0; set.trem = 0; set.tape = 0; set.reverb = 45;
    set.strum = 0;
    set.bpm = 118; set.swing = 0; set.dens = 50; set.gate = 50;
    set.dpat = 1; set.dkick = 75; set.dhat = 30; set.drim = 25;
    set.bsound = BASS_ROND; set.blevel = 60; set.bmotif = MOTIF_SUIT; set.bcut = 45;
    set.creature = 0; set.eyes = 0; set.decor = 0; set.mood = MOOD_AUTO;
}

static float glide_s(void) { return set.glide < 1u ? 0.0f : (float)set.glide * 0.015f; }
static float cutoff_hz(void) { return 70.0f * fm_powf(18000.0f / 70.0f, (float)set.cutoff * 0.01f); }
static float reso_q(void) { return 0.55f * fm_powf(30.0f, (float)set.reso * 0.01f); }

/* ------------------------------------------------------------------ chords --- */
typedef struct { uint8_t semi; char q; } chord_def_t;
static chord_def_t grid_w[NGRID][8], grid_b[NGRID][5];
static const char *const NOTE[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
static const char *const RICH_NAME[4] = {"TRIADE", "7E", "9E", "RICHE"};
static const char *const VOICING_NAME[3] = {"SERRE", "OUVERT", "LARGE"};
static const uint8_t WHITE_IDX[8] = {0, 2, 4, 5, 7, 9, 11, 12}, BLACK_IDX[5] = {1, 3, 6, 8, 10};

static int parse_chords(const char *s, chord_def_t *out, int max)
{
    int n = 0;
    while (*s && n < max) {
        int v = 0;
        while (*s == ' ') s++;
        while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
        out[n].semi = (uint8_t)v;
        out[n].q = *s ? *s++ : 'M';
        n++;
    }
    return n;
}

/* intervals for a quality at a richness; returns the count */
static int quality_ivs(char q, int rich, int8_t *iv)
{
    static const int8_t M[4][6] = {{0,4,7}, {0,4,7,11}, {0,4,7,11,14}, {0,4,7,11,14,21}};
    static const int8_t V[4][6] = {{0,4,7}, {0,4,7,10}, {0,4,7,10,14}, {0,4,7,10,14,21}};
    static const int8_t m[4][6] = {{0,3,7}, {0,3,7,10}, {0,3,7,10,14}, {0,3,7,10,14,17}};
    static const int8_t D[4][6] = {{0,4,7,10}, {0,4,7,10}, {0,4,7,10,14}, {0,4,7,10,14,21}};
    static const int8_t d[4][6] = {{0,3,6}, {0,3,6,10}, {0,3,6,10,14}, {0,3,6,10,14,17}};
    static const int8_t s4[4][6] = {{0,5,7}, {0,5,7,10}, {0,5,7,10,14}, {0,5,7,10,14,21}};
    static const int8_t s2[4][6] = {{0,2,7}, {0,2,7,10}, {0,2,7,10}, {0,2,7,10,21}};
    static const int8_t a[4][6] = {{0,4,8}, {0,4,8,11}, {0,4,8,11,14}, {0,4,8,11,14}};
    static const uint8_t NM[4] = {3, 4, 5, 6}, ND[4] = {4, 4, 5, 6}, NS2[4] = {3, 4, 4, 5}, NA[4] = {3, 4, 5, 5};
    const int8_t *src;
    int n, i;
    rich = clamp(rich, 0, 3);
    switch (q) {
    case 'V': src = V[rich]; n = NM[rich]; break;
    case 'm': src = m[rich]; n = NM[rich]; break;
    case 'D': src = D[rich]; n = ND[rich]; break;
    case 'd': src = d[rich]; n = NM[rich]; break;
    case 's': src = s4[rich]; n = NM[rich]; break;
    case 'S': src = s2[rich]; n = NS2[rich]; break;
    case 'a': src = a[rich]; n = NA[rich]; break;
    default: src = M[rich]; n = NM[rich]; break;
    }
    for (i = 0; i < n; i++) iv[i] = src[i];
    return n;
}

static char variation_of(char q)
{
    switch (q) {
    case 'M': case 'V': case 'D': return 's';
    case 'm': return 'S';
    case 'd': return 'm';
    case 's': return 'M';
    case 'S': return 'm';
    default: return 'M';
    }
}

typedef struct { int root; int8_t iv[6]; int n; int bass_off; int oct; int shift; } chord_t;
static uint8_t variation;                /* VARIATION held */

#define KEY_C4 7                         /* key index of C4 (key 0 is F3) */
static chord_t build_chord(int key)
{
    chord_t c;
    const chord_def_t *d;
    int rel = key - KEY_C4, pos = ((rel % 12) + 12) % 12, oct = rel < 0 ? -1 : rel / 12, i;
    d = &grid_w[set.grid][0];
    if (pos == 0 && oct >= 1) d = &grid_w[set.grid][7];            /* C5: the grid's 8th chord */
    else {
        for (i = 0; i < 7; i++) if (WHITE_IDX[i] == pos) d = &grid_w[set.grid][i];
        for (i = 0; i < 5; i++) if (BLACK_IDX[i] == pos) d = &grid_b[set.grid][i];
    }
    c.root = (set.key + d->semi) % 12;
    c.n = quality_ivs(variation ? variation_of(d->q) : d->q, set.rich, c.iv);
    c.bass_off = variation ? 7 : 0;
    c.oct = oct;
    c.shift = set.octave * 12;
    return c;
}

static int has(const chord_t *c, int pc)
{
    int i;
    for (i = 0; i < c->n; i++) if (c->iv[i] % 12 == pc) return 1;
    return 0;
}

static void text_cat(char *d, const char *s)
{
    while (*d) d++;
    while (*s) *d++ = *s++;
    *d = 0;
}

static void chord_name(const chord_t *c, char *b)
{
    int m = has(c, 3) && !has(c, 4), M = has(c, 4);
    int sus4 = !m && !M && has(c, 5), sus2 = !m && !M && !has(c, 5) && has(c, 2);
    int dim = m && has(c, 6) && !has(c, 7), aug = M && has(c, 8) && !has(c, 7);
    int maj7 = has(c, 11), min7 = has(c, 10), six = has(c, 9), add9 = has(c, 2) && !sus2;
    int eleven = has(c, 5) && (m || M);
    b[0] = 0;
    text_cat(b, NOTE[c->root]);
    if (dim) { text_cat(b, min7 ? "m7b5" : "dim"); return; }
    if (m) text_cat(b, "m");
    if (aug) text_cat(b, "+");
    if (maj7 || min7) {
        if (maj7) text_cat(b, "maj");
        text_cat(b, six ? "13" : eleven ? "11" : add9 ? "9" : "7");
    } else if (six && add9) text_cat(b, "6/9");
    else if (six) text_cat(b, "6");
    else if (add9) text_cat(b, "add9");
    if (sus4) text_cat(b, "sus4");
    if (sus2) text_cat(b, "sus2");
}

/* the 4-note voicing closest to the centre: the same key always sounds the same */
static void voice_chord(const chord_t *c, float *out)
{
    int pcs[6], np = 0, i, r, base, best_set = 0;
    int center = 64 + c->oct * 7 + c->shift;
    float best = 1e9f;
    for (i = 0; i < c->n; i++) {
        int p = (c->root + c->iv[i]) % 12, k, dup = 0;
        for (k = 0; k < np; k++) if (pcs[k] == p) dup = 1;
        if (!dup) pcs[np++] = p;
    }
    if (np > 4) {                        /* drop the fifth, then the root (the bass plays it) */
        int k, j = 0;
        for (k = 0; k < np; k++) if (pcs[k] != (c->root + 7) % 12) pcs[j++] = pcs[k];
        np = j;
    }
    if (np > 4) {
        int k, j = 0;
        for (k = 0; k < np; k++) if (pcs[k] != c->root) pcs[j++] = pcs[k];
        np = j;
    }
    if (np > 4) np = 4;
    for (r = 0; r < np; r++) {
        for (base = 50 + c->shift; base <= 72 + c->shift; base++) {
            int notes[4], k, cur = base, cnt = 0;
            float mean = 0.0f, cost;
            for (k = 0; k < np; k++) {
                int p = pcs[(r + k) % np];
                while (((cur % 12) + 12) % 12 != p) cur++;
                notes[cnt++] = cur++;
            }
            if (cnt == 3) notes[cnt++] = notes[0] + 12;
            if (notes[0] < 50 + c->shift || notes[cnt - 1] > 84 + c->shift) continue;
            for (k = 0; k < 4; k++) mean += (float)notes[k];
            mean *= 0.25f;
            cost = fm_fabsf(mean - (float)center);
            if (cost < best) {
                best = cost;
                for (k = 0; k < 4; k++) out[k] = (float)notes[k];
                best_set = 1;
            }
        }
    }
    if (!best_set) for (i = 0; i < 4; i++) out[i] = 60.0f + (float)i * 4.0f;
}

static void spread(float *n, int mode)   /* SERRE as is, OUVERT drop 2, LARGE drop 2 + top up an octave */
{
    int i, j;
    if (mode >= 1) n[2] -= 12.0f;
    if (mode >= 2) n[3] += 12.0f;
    for (i = 1; i < 4; i++)              /* sort */
        for (j = i; j > 0 && n[j - 1] > n[j]; j--) { float t = n[j]; n[j] = n[j - 1]; n[j - 1] = t; }
}

/* ------------------------------------------------------------------ engine links --- */
static void send_sound(void) { eng_sound(&SOUNDS[set.sound]); }
static void send_rhythm(void)
{
    rhythm_t r;
    int i, def = set.rhythm;
    const char *s = RHYTHM_DEF[def].s;
    memset(&r, 0, sizeof r);
    r.hold = s[0] == 0;
    r.gate = RHYTHM_DEF[def].gate;
    r.swing = RHYTHM_DEF[def].swing;
    r.atk = RHYTHM_DEF[def].atk;
    if (!r.hold)
        for (i = 0; i < 16; i++) {
            if (s[i] == 'X' || s[i] == 'x') {
                int len = 1;
                while (i + len < 16 && s[i + len] == '-') len++;
                r.v[i] = s[i] == 'X' ? 1.0f : 0.62f;
                r.len[i] = (uint8_t)len;
            }
        }
    eng_rhythm(&r);
}
static void send_params(void)
{
    eng_params((float)set.bpm, cutoff_hz(), reso_q(), (float)set.side * 0.01f, set.drums_on);
}
static void send_fx(void)
{
    eng_fx((float)set.reverb * 0.01f, (float)set.delay * 0.01f, (float)set.trem * 0.01f, (float)set.tape * 0.01f);
}
static void send_groove(void)
{
    eng_groove((float)set.swing * 0.01f, ((float)set.dens - 50.0f) * 0.02f, (float)set.gate * 0.01f,
               (float)set.strum * 0.01f);
}
static float drum_v(char c) { return c == 'X' ? 1.0f : (c == 'x' || c == 'o') ? 0.6f : 0.0f; }
static void send_drums(void)
{
    drums_t d;
    int i;
    for (i = 0; i < 16; i++) {
        d.k[i] = drum_v(DRUM_DEF[set.dpat].k[i]);
        d.h[i] = drum_v(DRUM_DEF[set.dpat].h[i]);
        d.r[i] = drum_v(DRUM_DEF[set.dpat].r[i]);
        d.open[i] = DRUM_DEF[set.dpat].h[i] == 'o';
    }
    eng_drums(&d, (float)set.dkick * 0.01f, (float)set.dhat * 0.01f, (float)set.drim * 0.01f);
}
static void send_bass(void) { eng_bass(set.bsound, set.bmotif, (float)set.blevel * 0.01f, (float)set.bcut * 0.01f); }

/* ------------------------------------------------------------------ playing --- */
static int cur_idx = -1, held_idx = -1, active;
static char chord_txt[16] = "OSR-1";
/* the voices' pitches for the screen, as the engine glides them */
static struct { float from, to; uint32_t t0, dur; } vis[5];

static float vis_pitch(int i, uint32_t now)
{
    float k;
    if (vis[i].dur == 0u) return vis[i].to;
    k = (float)(now - vis[i].t0) / (float)vis[i].dur;
    if (k > 1.0f) k = 1.0f;
    return vis[i].from + (vis[i].to - vis[i].from) * k;
}

static void play(int idx)
{
    chord_t c = build_chord(idx);
    float notes[4], bass, g = active ? glide_s() : 0.0f;
    uint32_t now = plat_ms();
    int i;
    voice_chord(&c, notes);
    spread(notes, set.voicing);
    bass = (float)(36 + (c.root + c.bass_off) % 12 + clamp(c.shift, -12, 12));
    chord_name(&c, chord_txt);
    for (i = 0; i < 4; i++) {
        vis[i].from = vis_pitch(i, now); vis[i].to = notes[i]; vis[i].t0 = now; vis[i].dur = (uint32_t)(g * 1000.0f);
    }
    vis[4].from = vis_pitch(4, now); vis[4].to = bass; vis[4].t0 = now; vis[4].dur = (uint32_t)(g * 1000.0f);
    cur_idx = idx;
    active = 1;
    eng_chord(notes, bass, glide_s());
}

static void stop(void)
{
    active = 0;
    cur_idx = -1;
    eng_stop();
}

static void rechord(void) { if (active && cur_idx >= 0) play(cur_idx); }

/* ------------------------------------------------------------------ pages --- */
enum { PG_HOME, PG_FX, PG_ACC, PG_GROOVE, PG_DRUMS, PG_BASS, PG_DECOR, NPAGE };
static const char *const PAGE_NAME[NPAGE] = {"HOME", "FX", "ACCORDS", "GROOVE", "BATTERIE", "BASSE", "DECOR"};
static const uint8_t PAGE_BTN[NPAGE] = {B_HOME, B_FX, B_SEL, B_ENV, B_GLO, B_SEQ, B_EDIT};
static const uint8_t PAGE_RGB[NPAGE][3] = {{255, 150, 220}, {190, 150, 255}, {120, 230, 255}, {255, 214, 110},
                                          {255, 140, 100}, {140, 240, 150}, {235, 235, 240}};

enum { KD_PCT, KD_OFF, KD_GLIDE, KD_BPM, KD_BIP, KD_LIST };
enum { G_NONE, G_PARAMS, G_FX, G_GROOVE, G_DRUMS, G_BASS, G_CHORD };
typedef struct {
    const char *lab;
    uint8_t *v;
    uint8_t lo, hi, kind, grp;
    const char *const *names;
} kdef_t;
static const kdef_t KD[NPAGE][4] = {
    {{"CUTOFF", &set.cutoff, 0, 100, KD_PCT, G_PARAMS, 0}, {"GLIDE", &set.glide, 0, 100, KD_GLIDE, G_NONE, 0},
     {"RESO", &set.reso, 0, 100, KD_PCT, G_PARAMS, 0}, {"SIDECHAIN", &set.side, 0, 100, KD_OFF, G_PARAMS, 0}},
    {{"DELAY", &set.delay, 0, 100, KD_OFF, G_FX, 0}, {"TREMOLO", &set.trem, 0, 100, KD_OFF, G_FX, 0},
     {"CASSETTE", &set.tape, 0, 100, KD_OFF, G_FX, 0}, {"REVERB", &set.reverb, 0, 100, KD_PCT, G_FX, 0}},
    {{"TONALITE", &set.key, 0, 11, KD_LIST, G_CHORD, NOTE}, {"VOICING", &set.voicing, 0, 2, KD_LIST, G_CHORD, VOICING_NAME},
     {"RICHESSE", &set.rich, 0, 3, KD_LIST, G_CHORD, RICH_NAME}, {"STRUM", &set.strum, 0, 100, KD_OFF, G_GROOVE, 0}},
    {{"BPM", &set.bpm, 70, 160, KD_BPM, G_PARAMS, 0}, {"SWING", &set.swing, 0, 100, KD_OFF, G_GROOVE, 0},
     {"DENSITE", &set.dens, 0, 100, KD_BIP, G_GROOVE, 0}, {"GATE", &set.gate, 0, 100, KD_PCT, G_GROOVE, 0}},
    {{"PATTERN", &set.dpat, 0, NDRUM - 1, KD_LIST, G_DRUMS, DRUM_NAME}, {"KICK", &set.dkick, 0, 100, KD_OFF, G_DRUMS, 0},
     {"HI-HAT", &set.dhat, 0, 100, KD_OFF, G_DRUMS, 0}, {"RIM", &set.drim, 0, 100, KD_OFF, G_DRUMS, 0}},
    {{"SON", &set.bsound, 0, NBASS_SOUND - 1, KD_LIST, G_BASS, BASS_NAME}, {"NIVEAU", &set.blevel, 0, 100, KD_PCT, G_BASS, 0},
     {"MOTIF", &set.bmotif, 0, NMOTIF - 1, KD_LIST, G_BASS, MOTIF_NAME}, {"FILTRE", &set.bcut, 0, 100, KD_PCT, G_BASS, 0}},
    {{"BESTIOLE", &set.creature, 0, NCREATURE - 1, KD_LIST, G_NONE, CREATURE_NAME},
     {"YEUX", &set.eyes, 0, NEYE - 1, KD_LIST, G_NONE, EYE_NAME},
     {"DECOR", &set.decor, 0, NDECOR - 1, KD_LIST, G_NONE, DECOR_NAME},
     {"HUMEUR", &set.mood, 0, NMOOD - 1, KD_LIST, G_NONE, MOOD_NAME}},
};

static int settings_ok(void)
{
    int p, k;
    if (set.magic != SET_MAGIC || set.grid >= NGRID || set.octave < -2 || set.octave > 2 || set.sound >= NFAMILY * PER_FAMILY
        || set.rhythm >= NRHYTHM || set.page >= NPAGE || set.latch > 1u || set.drums_on > 1u)
        return 0;
    for (p = 0; p < NPAGE; p++)
        for (k = 0; k < 4; k++)
            if (*KD[p][k].v < KD[p][k].lo || *KD[p][k].v > KD[p][k].hi) return 0;
    return 1;
}

/* ------------------------------------------------------------------ platform contract --- */
void app_init(void)
{
    int g;
    for (g = 0; g < NGRID; g++) {
        parse_chords(GRID_DEF[g].white, grid_w[g], 8);
        parse_chords(GRID_DEF[g].black, grid_b[g], 5);
    }
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || !settings_ok())
        settings_defaults();
    set.latch = 0;
    eng_init();
    send_sound();
    send_rhythm();
    send_groove();
    send_drums();
    send_bass();
    send_fx();
    send_params();
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    eng_render(lr, n, (float)master_q12 * (1.0f / 4096.0f) * 8300000.0f);
}

void app_panic(void) { eng_panic(); }

/* ------------------------------------------------------------------ UI --- */
static struct {
    char msg[2][24];
    uint32_t msg_until;
    uint32_t keys, btn;
    uint32_t frame_at, home_at;
    uint8_t help, help_drawn;
} ui;

static void int_text(int32_t v, char *b)
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do t[n++] = (char)('0' + u % 10u); while ((u /= 10u) && n < 11);
    if (v < 0) *b++ = '-';
    while (n) *b++ = t[--n];
    *b = 0;
}

void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++) ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++) ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    if (ann_on()) {
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        text_cat(t, " ");
        text_cat(t, ui.msg[1]);
        ann_say(t, 0, ANN_NOW);
    }
}

static void dirty(void) { set_dirty = 1; dirty_at = plat_ms(); }

static void wrap_u8(uint8_t *v, int32_t d, int n) { *v = (uint8_t)(((int32_t)*v + d % n + n) % n); }

/* a quick turn moves further: x1 slow, x2, x4 when the detents come close together */
static uint32_t enc_at[NE];
static int32_t accel(int k, int32_t e)
{
    uint32_t now = plat_ms(), dt = now - enc_at[k];
    enc_at[k] = now;
    if (dt < 40u) return e * 4;
    if (dt < 90u) return e * 2;
    return e;
}

/* a knob's value as text, and as 0..1 for its bar */
static float knob_text(const kdef_t *d, char *b)
{
    int v = *d->v;
    float f = (float)(v - d->lo) / (float)(d->hi - d->lo);
    switch (d->kind) {
    case KD_LIST: b[0] = 0; text_cat(b, d->names[v]); break;
    case KD_BPM: int_text(v, b); break;
    case KD_GLIDE:
        if (v < 1) { b[0] = 0; text_cat(b, "OFF"); }
        else { int_text(v * 15, b); text_cat(b, " MS"); }
        break;
    case KD_BIP:
        if (v == 50) { b[0] = 0; text_cat(b, "0"); }
        else { if (v > 50) { b[0] = '+'; b[1] = 0; } else b[0] = 0; int_text((v - 50) * 2, b + (v > 50)); text_cat(b, "%"); }
        break;
    case KD_OFF:
        if (v == 0) { b[0] = 0; text_cat(b, "OFF"); break; }
        /* fall through */
    default: int_text(v, b); text_cat(b, "%"); break;
    }
    return f;
}

static void apply_group(int g)
{
    switch (g) {
    case G_PARAMS: send_params(); break;
    case G_FX: send_fx(); break;
    case G_GROOVE: send_groove(); break;
    case G_DRUMS: send_drums(); break;
    case G_BASS: send_bass(); break;
    case G_CHORD: rechord(); break;
    default: break;
    }
}

static void knob(int k, int32_t e)
{
    const kdef_t *d = &KD[set.page][k];
    int32_t step = d->kind == KD_LIST ? e : d->kind == KD_BPM ? accel(EN_K1 + k, e) : accel(EN_K1 + k, e) * 3;
    int32_t n = clamp((int32_t)*d->v + step, d->lo, d->hi);
    if (n == *d->v) return;
    *d->v = (uint8_t)n;
    apply_group(d->grp);
    dirty();
}

static void encoder(int k, int32_t e)
{
    switch (k) {
    case EN_SELECT:
        wrap_u8(&set.grid, e, NGRID); set.rich = GRID_DEF[set.grid].rich; app_say("GRILLE", GRID_DEF[set.grid].name);
        rechord();
        break;
    case EN_ALGO:
        wrap_u8(&set.rhythm, e, NRHYTHM);
        app_say("RYTHME", set.rhythm ? RHYTHM_NAME[set.rhythm] : "OFF");
        send_rhythm();
        break;
    case EN_PRESET:
        wrap_u8(&set.sound, e, NFAMILY * PER_FAMILY);
        app_say(FAMILY_NAME[set.sound / PER_FAMILY], SOUND_NAME[set.sound]);
        send_sound();
        break;
    case EN_K1: case EN_K2: case EN_K3: case EN_K4: knob(k - EN_K1, e); return;
    default: return;
    }
    dirty();
}

static void set_variation(int on)
{
    if (variation == on) return;
    variation = (uint8_t)on;
    rechord();
}

static void button(int k)
{
    char t[8];
    int p;
    for (p = 0; p < NPAGE; p++)
        if (PAGE_BTN[p] == k) {
            if (k == B_HOME) ui.home_at = plat_ms();
            if (set.page != p) { set.page = (uint8_t)p; dirty(); }
            return;
        }
    switch (k) {
    case B_ARP:
        set.latch ^= 1u;
        if (!set.latch && held_idx < 0 && active) stop();
        app_say("LATCH", set.latch ? "ON" : "OFF");
        break;
    case B_PLAY: set.drums_on ^= 1u; send_params(); app_say("BATTERIE", set.drums_on ? "ON" : "OFF"); break;
    case B_OCTDN:
    case B_OCTUP:
        set.octave = (int8_t)clamp(set.octave + (k == B_OCTUP ? 1 : -1), -2, 2);
        t[0] = set.octave < 0 ? '-' : '+'; t[1] = (char)('0' + (set.octave < 0 ? -set.octave : set.octave)); t[2] = 0;
        app_say("OCTAVE", t);
        rechord();
        break;
    default: return;
    }
    dirty();
}

static void input(void)
{
    uint32_t keys = plat_keys(), btn = plat_buttons(), k, lit_keys = 0, lit_btn = 0, now = plat_ms();
    int32_t e;
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(ui.keys & bit)) { held_idx = (int)k; play((int)k); }
        else if (!(keys & bit) && (ui.keys & bit) && held_idx == (int)k) {
            held_idx = -1;
            if (!set.latch) stop();
        }
    }
    set_variation((btn >> B_LFO) & 1u);
    for (k = 0; k < NB; k++) {
        uint32_t bit = 1u << k;
        if ((btn & bit) && !(ui.btn & bit)) button((int)k);
    }
    /* HOME held: the help screen, as long as it is held */
    {
        uint8_t h = (btn >> B_HOME) & 1u && now - ui.home_at > 600u;
        if (h != ui.help) { ui.help = h; ui.help_drawn = 0; }
    }
    for (k = 0; k < NE; k++)
        if ((e = plat_enc((int)k)) != 0)
            encoder((int)k, e);
    ui.keys = keys;
    ui.btn = btn;
    /* auto-save: 4 s after the last change, and never while a chord plays (a flash write stops the sound) */
    if (set_dirty && !active && plat_ms() - dirty_at > 4000u) {
        if (plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0) set_dirty = 0;
        else dirty_at = plat_ms();
    }
    /* lights: the keys glow, the one playing is lit; the page's button; latch, drums, variation */
    if (cur_idx >= 0) lit_keys |= 1u << (uint32_t)cur_idx;
    lit_btn |= 1u << PAGE_BTN[set.page];
    if (set.latch) lit_btn |= 1u << B_ARP;
    if (set.drums_on) lit_btn |= LED_PLAY_GREEN;
    if (variation) lit_btn |= 1u << B_LFO;
    plat_lights(lit_btn, lit_keys, 0, (1u << NKEYS) - 1u, 1);
}

void app_input_only(void) { input(); }

/* ------------------------------------------------------------------ screen (240 x 240) --- */
/* A creature (black, a bit wobbly, glowing eyes) in water seen like a macro photo of an LCD: the picture is
 * 80 x 80 big pixels, each drawn as 3 x 3 screen pixels (red, green, blue columns over a dark row).
 * It swims to the chord (left/right = the key, up/down = the pitch, gliding with GLIDE), puffs up on the
 * rhythm's hits, is squashed by the sidechain, sends a ripple with each kick. The water changes colour with
 * the chords. CUTOFF: bright or murky water; REVERB: glowing specks; DELAY: echoes of the creature;
 * TREMOLO: the picture pulses; TAPE: it wobbles like a VHS. Info on two translucent bands, top and bottom.
 * Each half (240 x 120) is drawn into the canvas and sent, ~25 times a second. */
#define NC 80                            /* big pixels a side */
#define BAND_TOP 13                      /* rows 0..12 under the top band */
#define BAND_BOT 63                      /* rows 63..79 under the bottom band */
#define TAU_F 6.2831853f

static inline float sinr(float x) { return fm_sin_turns(x * 0.15915494f); }
static inline float frac(float x) { return x - fm_floorf(x); }
static inline float clamp01(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }

/* atan2 in turns (-0.5 .. 0.5) */
static float at2(float y, float x)
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), mx = fm_maxf(ax, ay), a, s, r;
    if (mx < 1e-6f) return 0.0f;
    a = fm_minf(ax, ay) / mx;
    s = a * a;
    r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax) r = 1.57079637f - r;
    if (x < 0.0f) r = 3.14159274f - r;
    if (y < 0.0f) r = -r;
    return r * 0.15915494f;
}

static void hsv(float h, float s, float v, float *o)
{
    float hh = frac(h) * 6.0f, f, p, q, t;
    int i = (int)hh;
    f = hh - (float)i;
    p = v * (1.0f - s); q = v * (1.0f - f * s); t = v * (1.0f - (1.0f - f) * s);
    switch (i) {
    case 0: o[0] = v; o[1] = t; o[2] = p; break;
    case 1: o[0] = q; o[1] = v; o[2] = p; break;
    case 2: o[0] = p; o[1] = v; o[2] = t; break;
    case 3: o[0] = p; o[1] = q; o[2] = v; break;
    case 4: o[0] = t; o[1] = p; o[2] = v; break;
    default: o[0] = v; o[1] = p; o[2] = q; break;
    }
}

static float capsule(float px, float py, float ax, float ay, float bx, float by, float ra, float rb)
{
    float pax = px - ax, pay = py - ay, bax = bx - ax, bay = by - ay;
    float h = clamp01((pax * bax + pay * bay) / (bax * bax + bay * bay + 1e-6f));
    float dx = pax - bax * h, dy = pay - bay * h;
    return fm_sqrtf(dx * dx + dy * dy) - (ra + (rb - ra) * h);
}

/* ---- the scene's state */
static struct {
    float t;                             /* seconds */
    float x, y, x_from, x_to;            /* the creature, in big pixels */
    uint32_t x_t0, x_dur;
    float dir, puff, spin, hue, hue_to;
    uint32_t kick_at, hit_at, blink_at, still_since;
    int hit_kind, last_step, last_idx;
    uint32_t last_hit_ms;
    float ex[16], ey[16];                /* recent positions, for the delay's echoes */
    int epos;
    struct { float x, y, v, ph; uint8_t on; } sp[14];   /* glowing specks (reverb) */
    float sp_acc;
    uint32_t rnd;
    /* per frame, for the cells */
    float R, sq, sx, ripple_r, ripple_a, trem, tape, bright, eye_r;
    int mood, blink;
    float eyes[2][2];
} sc;

static float srnd(void)
{
    sc.rnd = sc.rnd * 1664525u + 1013904223u;
    return (float)(sc.rnd >> 8) * (1.0f / 16777216.0f);
}

static void scene_init(void)
{
    sc.x = sc.x_from = sc.x_to = 40.0f;
    sc.y = 40.0f;
    sc.dir = 1.0f;
    sc.last_step = -1;
    sc.last_idx = -1;
    sc.rnd = 0x2468ACEu;
    sc.blink_at = 3000u;
}

/* the creature's distance field at (px, py), relative to its centre, facing right; < 0 inside */
static float creature_sdf(float px, float py)
{
    float R = sc.R, d, dist, a;
    switch (set.creature) {
    case 0: {                            /* pufferfish */
        float wob, s, sp, flap = sinr(sc.t * 9.0f) * 0.12f * R;
        dist = fm_sqrtf(px * px + py * py);
        a = at2(py, px);
        wob = 1.0f + 0.06f * (fm_sin_turns(3.0f * a + 0.13f) + 0.6f * fm_sin_turns(5.0f * a + 0.41f)
                              + 0.4f * fm_sin_turns(9.0f * a + 0.77f));
        s = fm_sin_turns(16.0f * a + 0.25f);
        sp = s > 0.0f ? s * s * s * s * s * s : 0.0f;
        d = dist - (R * 0.96f * wob + sp * R * (0.14f + 0.42f * sc.puff));
        d = fm_minf(d, capsule(px, py, -0.8f * R, 0.0f, -1.45f * R, -0.45f * R + flap, 0.22f * R, 0.1f * R));
        d = fm_minf(d, capsule(px, py, -0.8f * R, 0.0f, -1.5f * R, 0.42f * R + flap, 0.22f * R, 0.1f * R));
        return d;
    }
    case 1: {                            /* ocean sunfish */
        float q, flap = sinr(sc.t * 4.0f) * 0.16f * R;
        int i;
        q = fm_sqrtf((px / R) * (px / R) + (py / (0.9f * R)) * (py / (0.9f * R)));
        d = (q - 1.0f) * 0.9f * R + 0.04f * R * sinr(px * 0.5f + py * 0.3f + 1.0f);
        d = fm_minf(d, capsule(px, py, -0.2f * R, -0.6f * R, -0.45f * R + flap, -(1.75f + 0.1f * sc.puff) * R, 0.22f * R, 0.06f * R));
        d = fm_minf(d, capsule(px, py, -0.2f * R, 0.6f * R, -0.45f * R - flap, (1.7f + 0.1f * sc.puff) * R, 0.22f * R, 0.06f * R));
        for (i = 0; i < 5; i++) {
            float cx = -0.98f * R - px, cy = (-0.55f + 0.27f * (float)i) * R - py;
            d = fm_minf(d, fm_sqrtf(cx * cx + cy * cy) - 0.2f * R);
        }
        return d;
    }
    case 2: {                            /* jellyfish: it pulses on the hits */
        float rx = R * (1.0f + 0.14f * sc.puff), ry = R * (1.0f - 0.16f * sc.puff), qx = px / rx, qy = py / ry;
        int i, k;
        d = (fm_sqrtf(qx * qx + qy * qy) - 1.0f - 0.05f * sinr(px * 0.7f)) * fm_minf(rx, ry);
        d = fm_maxf(d, py - 0.12f * R);
        for (i = 0; i < 6; i++) {
            float x0 = (-0.7f + 0.28f * (float)i) * R, pxp = x0, pyp = 0.05f * R;
            for (k = 1; k <= 3; k++) {
                float x1 = x0 + sinr(sc.t * 2.2f + (float)i * 1.7f + (float)k * 0.9f) * 0.14f * R * (float)k / 2.0f;
                float y1 = (0.05f + (0.45f + 0.03f * (float)(i % 3)) * (float)k) * R * (1.0f - 0.15f * sc.puff);
                d = fm_minf(d, capsule(px, py, pxp, pyp, x1, y1, (0.14f - 0.03f * (float)k) * R, (0.11f - 0.03f * (float)k) * R));
                pxp = x1; pyp = y1;
            }
        }
        return d;
    }
    default: {                           /* starfish: turns a little on the hits */
        float arm, r;
        R *= 1.2f;
        dist = fm_sqrtf(px * px + py * py);
        a = at2(py, px);
        arm = 0.5f + 0.5f * fm_sin_turns(5.0f * (a - sc.spin) + 0.25f);
        r = R * (0.38f + 0.62f * arm * arm) * (1.0f + 0.05f * fm_sin_turns(7.0f * a + 0.3f)) * (1.0f + 0.1f * fm_sin_turns(a + 0.5f));
        return dist - r;
    }
    }
}

/* the eyes' places, relative to the centre, facing right */
static void eye_places(void)
{
    static const float E[4][4] = {{0.1f, -0.24f, 0.56f, -0.2f}, {0.38f, -0.2f, 0.68f, -0.18f},
                                  {-0.3f, -0.25f, 0.3f, -0.25f}, {-0.2f, -0.1f, 0.2f, -0.1f}};
    const float *e = E[set.creature];
    float R = sc.R * (set.creature == 3 ? 1.2f : 1.0f), sym = set.creature >= 2;
    sc.eyes[0][0] = sc.x + (sym ? e[0] : e[0] * sc.dir) * R * sc.sx;
    sc.eyes[0][1] = sc.y + e[1] * R * sc.sq;
    sc.eyes[1][0] = sc.x + (sym ? e[2] : e[2] * sc.dir) * R * sc.sx;
    sc.eyes[1][1] = sc.y + e[3] * R * sc.sq;
}

/* one eye's light on a cell: adds to c */
static void eye_light(float *c, float cx, float cy, float ex, float ey, int side)
{
    const float *col = EYE_COL[set.eyes];
    float dx = cx - ex, dy = cy - ey, r = sc.eye_r, d, glow;
    int core;
    if (dx > 5.0f * r || dx < -5.0f * r || dy > 5.0f * r || dy < -5.0f * r) return;
    d = fm_sqrtf(dx * dx + dy * dy);
    if (sc.blink || sc.mood == MOOD_DORT) {
        core = fm_fabsf(dy) < 0.4f && fm_fabsf(dx) < 1.3f * r;
        glow = 0.25f * fm_expf(-(dx * dx * 0.3f + dy * dy * 2.0f));
    } else if (sc.mood == MOOD_HEUREUX) {
        float dd = dy + 0.6f * fm_fabsf(dx), ox = fm_maxf(fm_fabsf(dx) - 1.5f * r, 0.0f);
        core = fm_fabsf(dd) < 0.7f && fm_fabsf(dx) < 1.7f * r;
        glow = 0.6f * fm_expf(-(dd * dd + ox * ox) / 1.2f);
    } else if (sc.mood == MOOD_GRINCHEUX) {
        core = d < r && dy > -0.15f + 0.35f * dx * (float)side;
        glow = 0.45f * fm_expf(-(d / (r * 1.1f)) * (d / (r * 1.1f)));
    } else {
        float rr = sc.mood == MOOD_SURPRIS ? r * 1.45f : r;
        core = d < rr;
        glow = 0.6f * fm_expf(-(d / (rr * 1.15f)) * (d / (rr * 1.15f)));
    }
    c[0] += glow * col[0]; c[1] += glow * col[1]; c[2] += glow * col[2];
    if (core) { c[0] = col[0] * 0.5f + 0.55f; c[1] = col[1] * 0.5f + 0.55f; c[2] = col[2] * 0.5f + 0.55f; }
}

/* the background at a cell */
static void water(float x, float y, float *c)
{
    float t = sc.t, h = sc.hue;
    switch (set.decor) {
    case 0: {                            /* rainbow moire on lilac */
        float u = x * 0.55f + y * 0.95f + 3.0f * sinr(y * 0.08f + t) + 2.0f * sinr(x * 0.05f - t * 0.5f), st;
        st = 0.5f + 0.5f * sinr(u * 0.9f);
        hsv(u * 0.045f + t * 0.05f + h, 0.85f * (0.35f + 0.65f * st), 0.85f, c);
        c[0] = c[0] * 0.72f + 0.2f; c[1] = c[1] * 0.72f + 0.17f; c[2] = c[2] * 0.72f + 0.27f;
        break;
    }
    case 1: {                            /* soft blue, tinted by the chord */
        float v = 0.04f * sinr(x * 0.3f + y * 0.2f + t), tint[3];
        hsv(0.6f + h * 0.25f, 0.5f, 0.86f, tint);
        c[0] = tint[0] + v; c[1] = tint[1] + v; c[2] = tint[2] + v;
        break;
    }
    case 2: {                            /* embers, darker below */
        float k = y / (float)NC, w = 0.05f * sinr(x * 0.4f - y * 0.3f + t), top[3];
        hsv(0.03f + h * 0.12f, 0.7f, 0.95f, top);
        c[0] = top[0] * (1.0f - k) + 0.35f * k + w; c[1] = top[1] * (1.0f - k) + 0.05f * k + w;
        c[2] = top[2] * (1.0f - k) + 0.12f * k + w;
        break;
    }
    default: {                           /* acid: rings around the creature, kicked by the kick */
        float dx = x - sc.x, dy = y - sc.y, d = fm_sqrtf(dx * dx + dy * dy), s;
        float kk = sc.ripple_a;
        s = 0.6f + 0.3f * sinr(d * 0.7f - t * 3.0f + kk * 6.0f);
        hsv(d * 0.035f - t * 0.2f + 0.08f * sinr(x * 0.2f + t) + h, s, 0.85f, c);
        break;
    }
    }
}

static inline uint16_t px565(float r, float g, float b)
{
    int32_t R = (int32_t)(r * 255.0f), G = (int32_t)(g * 255.0f), B = (int32_t)(b * 255.0f);
    R = R < 0 ? 0 : R > 255 ? 255 : R; G = G < 0 ? 0 : G > 255 ? 255 : G; B = B < 0 ? 0 : B > 255 ? 255 : B;
    return swap16(RGB(R, G, B));
}

/* one cell's colour */
static void cell(int cx, int cy, float *c)
{
    float x = (float)cx + 0.5f, y = (float)cy + 0.5f, dx, dy, d, cov, halo;
    int i;
    if (sc.tape > 0.01f)                 /* tape: the rows wobble */
        x += sc.tape * 1.8f * sinr(y * 0.35f + sc.t * 5.0f);
    water(x, y, c);
    c[0] *= sc.bright; c[1] *= sc.bright; c[2] *= sc.bright;
    /* the creature */
    dx = (x - sc.x) * sc.dir / sc.sx;
    dy = (y - sc.y) / sc.sq;
    if (dx > -2.4f * sc.R && dx < 2.4f * sc.R && dy > -2.4f * sc.R && dy < 2.6f * sc.R) {
        d = creature_sdf(dx, dy);
        cov = clamp01(0.5f - d);
        halo = d > 0.0f ? clamp01(1.0f - d / 6.0f) : 1.0f;
        halo *= halo;
        for (i = 0; i < 3; i++) c[i] = c[i] * (1.0f - 0.38f * halo) * (1.0f - cov) + 0.02f * cov;
    }
    /* the delay's echoes: dark rings where it was */
    if (set.delay && active) {
        for (i = 1; i <= 2; i++) {
            int h = (sc.epos - i * 5 + 16 * 4) % 16;
            float ex = x - sc.ex[h], ey = y - sc.ey[h], rr = fm_sqrtf(ex * ex + ey * ey);
            if (fm_fabsf(rr - sc.R) < 0.6f + 0.2f * (float)i) {
                float k = 1.0f - (0.5f - 0.15f * (float)i) * (float)set.delay * 0.012f;
                c[0] *= k; c[1] *= k; c[2] *= k;
            }
        }
    }
    /* the kick's ripple */
    if (sc.ripple_a > 0.01f) {
        float rx = x - sc.x, ry = y - sc.y, rr = fm_sqrtf(rx * rx + ry * ry), w = fm_fabsf(rr - sc.ripple_r);
        if (w < 1.2f) { float k = sc.ripple_a * (1.0f - w / 1.2f) * 0.45f; c[0] += k; c[1] += k; c[2] += k; }
    }
    /* specks of light (reverb) */
    for (i = 0; i < 14; i++)
        if (sc.sp[i].on) {
            float ex = x - sc.sp[i].x, ey = y - sc.sp[i].y;
            if (ex > -2.5f && ex < 2.5f && ey > -2.5f && ey < 2.5f) {
                float g = 0.55f / (1.0f + (ex * ex + ey * ey) * 1.6f);
                c[0] += g * 0.9f; c[1] += g; c[2] += g;
            }
        }
    eye_light(c, x, y, sc.eyes[0][0], sc.eyes[0][1], set.creature >= 2 ? 1 : (sc.dir > 0.0f ? 1 : -1));
    eye_light(c, x, y, sc.eyes[1][0], sc.eyes[1][1], set.creature >= 2 ? -1 : (sc.dir > 0.0f ? -1 : 1));
    if (sc.tape > 0.2f && cy == ((int)(sc.t * 18.0f) % NC)) { c[0] += 0.12f; c[1] += 0.12f; c[2] += 0.12f; }
    c[0] *= sc.trem; c[1] *= sc.trem; c[2] *= sc.trem;
    if (cy < BAND_TOP || cy >= BAND_BOT) { c[0] *= 0.3f; c[1] *= 0.3f; c[2] *= 0.3f; }
}

/* draw big-pixel rows r0..r0+39 into the canvas (240 x 120) */
static void draw_cells(int r0)
{
    int cx, cy;
    float c[3];
    for (cy = r0; cy < r0 + 40; cy++) {
        uint16_t *row = cv_px + (uint32_t)(cy - r0) * 3u * 240u;
        for (cx = 0; cx < NC; cx++) {
            float r, g, b;
            uint16_t a0, a1, a2, d0, d1, d2;
            uint16_t *p = row + cx * 3;
            cell(cx, cy, c);
            r = c[0] * 1.15f; g = c[1] * 1.15f; b = c[2] * 1.15f;
            a0 = px565(r, g * 0.35f, b * 0.35f); a1 = px565(r * 0.35f, g, b * 0.35f); a2 = px565(r * 0.35f, g * 0.35f, b);
            d0 = px565(r * 0.4f, g * 0.14f, b * 0.14f); d1 = px565(r * 0.14f, g * 0.4f, b * 0.14f);
            d2 = px565(r * 0.14f, g * 0.14f, b * 0.4f);
            p[0] = a0; p[1] = a1; p[2] = a2;
            p[240] = a0; p[241] = a1; p[242] = a2;
            p[480] = d0; p[481] = d1; p[482] = d2;
        }
    }
}

/* ---- the scene, once a frame */
static void scene_step(uint32_t now, float dt)
{
    int st, kick, i, hk;
    uint32_t at;
    float pitch = 0.0f, ty;
    sc.t += dt;
    /* left / right: the key, in the glide time */
    if (cur_idx >= 0 && cur_idx != sc.last_idx) {
        float tx = 20.0f + (float)cur_idx * (40.0f / 26.0f);
        sc.x_from = sc.x; sc.x_to = tx; sc.x_t0 = now; sc.x_dur = (uint32_t)(glide_s() * 1000.0f) + 160u;
        if (tx > sc.x + 0.5f) sc.dir = 1.0f;
        else if (tx < sc.x - 0.5f) sc.dir = -1.0f;
    }
    sc.last_idx = cur_idx;
    {
        float k = sc.x_dur ? (float)(now - sc.x_t0) / (float)sc.x_dur : 1.0f;
        k = clamp01(k);
        k = k * k * (3.0f - 2.0f * k);
        sc.x = sc.x_from + (sc.x_to - sc.x_from) * k;
    }
    /* up / down: the chord's pitch (gliding as the engine glides it), a slow bob when idle */
    for (i = 0; i < 4; i++) pitch += vis_pitch(i, now);
    pitch *= 0.25f;
    ty = active ? 50.0f - (pitch - 52.0f) * (22.0f / 30.0f) : 40.0f + 2.5f * sinr(sc.t * 0.9f);
    ty = fm_clampf(ty, 28.0f, 50.0f);
    sc.y += (ty - sc.y) * fm_minf(1.0f, dt * (active ? 8.0f : 1.5f));
    /* the water's hue follows the chord's root */
    if (active && cur_idx >= 0) sc.hue_to = (float)((set.key + (cur_idx * 7)) % 12) / 12.0f;
    {
        float dh = sc.hue_to - frac(sc.hue);
        if (dh > 0.5f) dh -= 1.0f;
        if (dh < -0.5f) dh += 1.0f;
        sc.hue = frac(sc.hue + dh * fm_minf(1.0f, dt * 1.2f));
    }
    /* hits: puff up, turn; the kick: a ripple */
    hk = eng_last_hit(&at);
    if (at != sc.last_hit_ms && active) {
        sc.last_hit_ms = at;
        sc.hit_at = now;
        sc.hit_kind = hk;
        sc.puff = fm_maxf(sc.puff, hk == 1 ? 1.0f : 0.6f);
        sc.spin += hk == 1 ? 0.04f : 0.02f;
    }
    st = eng_last_step(&at, &kick);
    if (st != sc.last_step) { sc.last_step = st; if (kick) sc.kick_at = now; }
    sc.puff *= fm_expf(-dt / 0.22f);
    if (active && RHYTHM_DEF[set.rhythm].s[0] == 0) sc.puff = fm_maxf(sc.puff, 0.12f + eng_level() * 0.8f);
    sc.spin += dt * 0.01f;
    /* size, sidechain squash, ripple */
    sc.R = (active ? 11.0f : 10.0f) * (set.creature == 0 ? 1.0f + 0.35f * sc.puff : 1.0f);
    sc.sq = 1.0f; sc.sx = 1.0f;
    if (set.drums_on && set.side) {
        uint32_t age = now - sc.kick_at;
        if (age < 300u) {
            float k = (float)set.side * 0.01f * 0.25f * fm_expf(-(float)age / 90.0f);
            sc.sq = 1.0f - k; sc.sx = 1.0f + k * 0.7f;
        }
    }
    sc.ripple_a = 0.0f;
    if (set.drums_on && set.dkick && now - sc.kick_at < 420u) {
        float k = (float)(now - sc.kick_at) / 420.0f;
        sc.ripple_r = sc.R + 3.0f + k * 32.0f;
        sc.ripple_a = 1.0f - k;
    }
    /* the delay's echoes */
    sc.ex[sc.epos] = sc.x; sc.ey[sc.epos] = sc.y;
    sc.epos = (sc.epos + 1) % 16;
    /* specks: reverb is how many */
    sc.sp_acc += dt * (float)set.reverb * 0.06f;
    while (sc.sp_acc >= 1.0f) {
        sc.sp_acc -= 1.0f;
        for (i = 0; i < 14; i++)
            if (!sc.sp[i].on) {
                sc.sp[i].on = 1; sc.sp[i].x = 4.0f + srnd() * 72.0f; sc.sp[i].y = (float)BAND_BOT + 2.0f;
                sc.sp[i].v = 3.0f + srnd() * 5.0f; sc.sp[i].ph = srnd();
                break;
            }
    }
    for (i = 0; i < 14; i++)
        if (sc.sp[i].on) {
            sc.sp[i].y -= sc.sp[i].v * dt;
            sc.sp[i].x += sinr(sc.t * 1.3f + sc.sp[i].ph * TAU_F) * dt * 1.5f;
            if (sc.sp[i].y < -3.0f) sc.sp[i].on = 0;
        }
    /* mood: in AUTO, asleep when nothing plays, surprised on an accent, happy on the variation,
     * grumpy when only the drums play */
    if (set.mood == MOOD_AUTO) {
        if (!active && !set.drums_on) sc.mood = MOOD_DORT;
        else if (active && sc.hit_kind == 1 && now - sc.hit_at < 220u) sc.mood = MOOD_SURPRIS;
        else if (variation) sc.mood = MOOD_HEUREUX;
        else if (!active) sc.mood = MOOD_GRINCHEUX;
        else sc.mood = MOOD_CONTENT;
    } else sc.mood = set.mood;
    if (now > sc.blink_at) { sc.blink_at = now + 2500u + (uint32_t)(srnd() * 4000.0f); }
    sc.blink = (sc.blink_at > now && sc.blink_at - now < 120u);
    sc.eye_r = 1.35f;
    sc.bright = 0.5f + 0.5f * (float)set.cutoff * 0.01f;
    sc.trem = 1.0f - (float)set.trem * 0.01f * 0.4f * (0.5f + 0.5f * sinr(sc.t * (float)set.bpm / 60.0f * 2.0f * TAU_F));
    sc.tape = (float)set.tape * 0.01f;
    eye_places();
}

/* ---- the info bands, crisp over the picture */
static uint16_t page_col(void) { const uint8_t *c = PAGE_RGB[set.page]; return RGB(c[0], c[1], c[2]); }

static void draw_top_text(void)
{
    char t[24];
    const char *chord = active ? chord_txt : (chord_txt[0] ? chord_txt : "OSR-1");
    uint32_t now = plat_ms();
    if (now < ui.msg_until) {
        int32_t x = cv_text(8, 0, &FONT_XS, ui.msg[0], RGB(200, 196, 210));
        cv_text(x + 6, 0, &FONT_XS, ui.msg[1], page_col());
    } else {
        cv_text(8, 0, &FONT_XS, SOUND_NAME[set.sound], page_col());
        cv_text(232 - text_w(&FONT_XS, GRID_DEF[set.grid].name), 0, &FONT_XS, GRID_DEF[set.grid].name, RGB(200, 196, 210));
    }
    cv_text(8, 9, &FONT_L, chord, active ? RGB(250, 248, 252) : RGB(170, 166, 180));
    t[0] = 0;
    if (variation) text_cat(t, "VAR  ");
    text_cat(t, NOTE[set.key]);
    cv_text(232 - text_w(&FONT_S, t), 19, &FONT_S, t, variation ? RGB(255, 150, 220) : RGB(200, 196, 210));
}

static void draw_bottom_text(void)       /* in the second canvas: y 0 = screen row 120 */
{
    int k;
    int32_t y0 = 189 - 120, tw = text_w(&FONT_T, PAGE_NAME[set.page]) + 10;
    uint16_t pc = page_col();
    cv_rect(6, y0 + 1, tw, 13, pc);
    cv_text(11, y0 + 1, &FONT_T, PAGE_NAME[set.page], RGB(0, 0, 0));
    {   /* the rhythm's name and the drums, small, to the right of the tab */
        char t[32];
        t[0] = 0;
        text_cat(t, set.rhythm ? RHYTHM_NAME[set.rhythm] : "RYTHME OFF");
        if (set.drums_on) text_cat(t, " + DRUMS");
        cv_text(232 - text_w(&FONT_T, t), y0 + 1, &FONT_T, t, RGB(170, 166, 180));
    }
    for (k = 0; k < 4; k++) {
        char v[24];
        const kdef_t *d = &KD[set.page][k];
        int32_t x = 6 + k * 58;
        float f = knob_text(d, v);
        const felucca_font_t *vf = text_w(&FONT_S, v) <= 56 ? &FONT_S : &FONT_T;
        cv_text(x, y0 + 15, &FONT_T, d->lab, RGB(190, 186, 200));
        cv_text(x, y0 + 28 + (vf == &FONT_T ? 2 : 0), vf, v, RGB(250, 248, 252));
        cv_rect(x, y0 + 47, 52, 2, RGB(60, 58, 70));
        cv_rect(x, y0 + 47, (int32_t)(52.0f * f + 0.5f), 2, pc);
    }
}

/* ---- HOME held: the help, laid out like the panel */
static void help_box(int32_t x, int32_t y, int32_t w, int32_t h, const char *dev, const char *fn, uint16_t fc,
                     uint16_t edge, const felucca_font_t *ff)
{
    cv_rect(x, y, w, h, edge);
    cv_rect(x + 1, y + 1, w - 2, h - 2, RGB(16, 14, 22));
    cv_text(x + (w - text_w(&FONT_T, dev)) / 2, y + 3, &FONT_T, dev, RGB(150, 146, 160));
    cv_text(x + (w - text_w(ff, fn)) / 2, y + h - ff->h - 3, ff, fn, fc);
}

static void draw_help(void)
{
    static const char *const ENC_DEV[3] = {"SELECT", "ALGO", "PRESET"}, *const ENC_FN[3] = {"GRILLE", "RYTHME", "SON"};
    static const char *const B_DEV[12] = {"FX", "SCL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"};
    static const char *const B_FN[12] = {"FX", "ACCORD", "GROOVE", "VAR", "DECOR", "BATT", "HOME", "-", "LATCH", "BASSE", "DRUMS", "-"};
    static const uint8_t B_PAGE[12] = {1, 1, 1, 0, 1, 1, 1, 0, 0, 1, 0, 0};
    int32_t i;
    cv_begin(240, 120, RGB(8, 6, 12));
    cv_text(6, 2, &FONT_XS, "AIDE", RGB(255, 214, 110));
    cv_text(234 - text_w(&FONT_XS, "LACHE HOME"), 2, &FONT_XS, "LACHE HOME", RGB(150, 146, 160));
    for (i = 0; i < 3; i++)
        help_box(6 + i * 77, 22, 74, 44, ENC_DEV[i], ENC_FN[i], RGB(120, 230, 255), RGB(50, 46, 70), &FONT_XS);
    cv_text(6, 74, &FONT_T, "K1 K2 K3 K4 : LES 4 REGLAGES DE LA PAGE,", RGB(220, 216, 228));
    cv_text(6, 88, &FONT_T, "AFFICHES EN BAS DE L'ECRAN.", RGB(220, 216, 228));
    cv_text(6, 104, &FONT_T, "BOUTONS EN COULEUR = PAGES", RGB(255, 214, 110));
    cv_blit(0, 0);
    cv_begin(240, 120, RGB(8, 6, 12));
    for (i = 0; i < 12; i++) {
        uint16_t edge = B_PAGE[i] ? RGB(120, 100, 40) : RGB(50, 46, 70);
        uint16_t fc = B_PAGE[i] ? RGB(255, 214, 110) : RGB(240, 238, 244);
        if (B_FN[i][0] == '-') fc = RGB(80, 76, 90);
        help_box(4 + (i % 6) * 39, 2 + (i / 6) * 46, 37, 42, B_DEV[i], B_FN[i], fc, edge, &FONT_T);
    }
    cv_text(120 - text_w(&FONT_T, "LFO TENU = VARIATION   OCT- / OCT+") / 2, 100, &FONT_T,
            "LFO TENU = VARIATION   OCT- / OCT+", RGB(150, 146, 160));
    cv_blit(0, 120);
}

void app_ui_init(void)
{
    int i;
    lcd_fill(0, 0, 240, 240, RGB(8, 6, 12));
    for (i = 0; i < 5; i++) vis[i].to = vis[i].from = i == 4 ? 36.0f : 60.0f + (float)i * 4.0f;
    scene_init();
}

void app_frame(void)
{
    uint32_t now = plat_ms();
    input();
    if (ui.help) {
        if (!ui.help_drawn) { ui.help_drawn = 1; draw_help(); }
        ann_tick(now);
        return;
    }
    if (now - ui.frame_at >= 40u) {
        float dt = ui.frame_at ? (float)(now - ui.frame_at) * 0.001f : 0.04f;
        if (dt > 0.1f) dt = 0.1f;
        ui.frame_at = now;
        scene_step(now, dt);
        cv_begin(240, 120, 0);
        draw_cells(0);
        draw_top_text();
        cv_blit(0, 0);
        cv_begin(240, 120, 0);
        draw_cells(40);
        draw_bottom_text();
        cv_blit(0, 120);
    }
    ann_tick(now);
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "grid")) *got = set.grid;
    else if (!strcmp(what, "key")) *got = set.key;
    else if (!strcmp(what, "sound")) *got = set.sound;
    else if (!strcmp(what, "rhythm")) *got = set.rhythm;
    else if (!strcmp(what, "chord")) *got = cur_idx;
    else if (!strcmp(what, "active")) *got = active;
    else if (!strcmp(what, "rich")) *got = set.rich;
    else if (!strcmp(what, "octave")) *got = set.octave;
    else if (!strcmp(what, "variation")) *got = variation;
    else if (!strcmp(what, "dirty")) *got = set_dirty;
    else if (!strcmp(what, "bpm")) *got = set.bpm;
    else if (!strcmp(what, "side")) *got = set.side;
    else if (!strcmp(what, "page")) *got = set.page;
    else if (!strcmp(what, "drums")) *got = set.drums_on;
    else if (!strcmp(what, "latch")) *got = set.latch;
    else if (!strcmp(what, "help")) *got = ui.help;
    else if (!strcmp(what, "cutoff")) *got = set.cutoff;
    else if (!strcmp(what, "delay")) *got = set.delay;
    else if (!strcmp(what, "bsound")) *got = set.bsound;
    else if (!strcmp(what, "creature")) *got = set.creature;
    else return 0;
    return 1;
}
#endif
