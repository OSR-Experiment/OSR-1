/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Obsimo (OSR Records) */
/* BLOP (by Obsimo) app: input, pages, chord grids, the aquarium screen, settings. Part of the unity build (device/unity.c,
 * host/plat_host.c), after the platform: gfx.c and announce.c are in scope. The sound is dsp/engine.c.
 *
 * Controls
 *   all 27 keys     chords of the grid (same note name, same chord; C5 = the grid's 8th chord),
 *                   white keys safe, black keys spices; below C4 an octave down, above C5 an octave up.
 *                   With a rhythm the chord keeps playing (latch): the same key again stops it.
 *   SELECT grid   ALGORITHM rhythm (OFF = the chord held, only while the key is down)   PRESET sound
 *   K1..K4          the page's four settings, always shown at the bottom of the screen
 *   top row: page buttons (the same button again: back to the main page)
 *                   main  cutoff, glide, note length, drums (level; 0 = no drums)
 *                   FX    delay, tremolo, chorus, reverb
 *                   SCL   chords: key, voicing, richness, strum
 *                   ENV   groove: BPM, swing, density, sidechain on/off
 *                   LFO   bass: sound, level, motif, filter
 *                   EDIT  decor: creature, eyes, background, accessory
 *                   GLO   drums: pattern (each with its own kit), kick, hi-hat, rim
 *   bottom row      HOME  back to the main page (held: the title, BLOP by Obsimo)
 *                   SAVE  step back: the creature in its bowl, the music heard from outside the room
 *                   ARP   the aquarium's light   SEQ  a pirouette   PLAY  hide the info (just the picture)
 *                   REC   feed it
 *   OCT- / OCT+ octave.   HOME + REC held 3 s: back to the first-start settings (a countdown).
 *   Settings save themselves a few seconds after the last change. */
#include "fastmath.h"
#include "engine.h"
#include "data.h"
#include "patterns.h"

/* ------------------------------------------------------------------ settings --- */
#define SET_MAGIC 0x4F535236u            /* "OSR6" (v6: bass 80 %, 2-STEP first) */
typedef struct {
    uint32_t magic;
    uint8_t grid, key, rich, voicing;
    int8_t octave;
    uint8_t sound, rhythm, page;
    uint8_t cutoff, glide, gate, drums;         /* main */
    uint8_t delay, trem, chorus, reverb;        /* FX */
    uint8_t strum;                              /* chords (with key, voicing, richness) */
    uint8_t bpm, swing, dens, side;             /* groove (density: 50 = as written; side: 0 off, 1 on) */
    uint8_t dpat, dkick, dhat, drim;            /* drums */
    uint8_t bsound, blevel, bmotif, bcut;       /* bass */
    uint8_t creature, eyes, decor, acc;         /* decor */
    uint8_t rsv[3];
} settings_t;
static settings_t set;
static uint8_t set_dirty;
static uint32_t dirty_at;             /* when the settings last changed (auto-save)*/

static void settings_defaults(void)
{
    memset(&set, 0, sizeof set);
    set.magic = SET_MAGIC;
    set.cutoff = 72; set.glide = 17; set.gate = 50; set.drums = 0;      /* first start: no drums */
    set.delay = 0; set.trem = 0; set.chorus = 0; set.reverb = 22;   /* no FX, a light reverb */
    set.strum = 0;
    set.bpm = 118; set.swing = 0; set.dens = 50; set.side = 1;
    set.dpat = 0; set.dkick = 75;   /* the drums start as a plain kick */ set.dhat = 30; set.drim = 25;
    set.bsound = BASS_ROND; set.blevel = 80; set.bmotif = MOTIF_SUIT; set.bcut = 45;
    set.creature = 0; set.eyes = 0; set.decor = 0; set.acc = 0;
}

static float glide_s(void) { return set.glide < 1u ? 0.0f : (float)set.glide * 0.015f; }
static float cutoff_hz(void) { return 70.0f * fm_powf(12000.0f / 70.0f, (float)set.cutoff * 0.01f); }   /* 70 Hz .. 12 kHz */
static float reso_q(void) { return 1.0f; }

/* ------------------------------------------------------------------ chords --- */
typedef struct { uint8_t semi; char q; } chord_def_t;
static chord_def_t grid_w[NGRID][8], grid_b[NGRID][5];
static const char *const NOTE[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
static const char *const RICH_NAME[4] = {"TRIAD", "7TH", "9TH", "LUSH"};
static const char *const VOICING_NAME[3] = {"CLOSE", "OPEN", "WIDE"};
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

typedef struct { int root; int8_t iv[6]; int n; int bass_off; int oct; int shift; } chord_t;

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
    c.n = quality_ivs(d->q, set.rich, c.iv);
    c.bass_off = 0;
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
/* ------------------------------------------------------------------ MIDI clock in (USB) --- */
/* Ableton (or any host) sending MIDI clock to Blop: its tempo replaces BPM, and its beats place the rhythm's steps.
 * The 1 kHz tick reads the MIDI in (each clock's time to the ms); the main loop takes the tempo and the beats from it.
 * Start (FA) puts the next clock on beat 1, Continue (FB) and Song Position (F2) where the host is, Stop (FC) lets
 * Blop run on alone at the last tempo, as when the clock goes quiet. */
#define CK_N 49u                                  /* 48 clocks: two beats, for the tempo */
static volatile struct {
    uint32_t t[CK_N], n;                          /* the clocks' times (ms), how many so far */
    int32_t pos;                                  /* clocks since beat 1 (24 a beat), counted while the host plays */
    uint8_t run;                                  /* the host's transport plays (Start / Continue .. Stop) */
    uint32_t beat_ms, beat_seq;                   /* the last beat's time, and a count of beats */
    int32_t beat_pos;
    uint32_t last_ms;
} ck;
static float ck_bpm;                              /* the tempo it gives (0: no clock, BPM leads) */
static uint32_t ck_seen_seq;
static uint8_t ck_placed;                         /* the engine's steps follow the beats */

void app_tick(void)
{
    uint32_t pkt, now;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = pkt >> 8 & 0xFFu;
        if (port != PORT_USB) continue;
        now = plat_ms();
        switch (st) {
        case 0xF8:
            ck.t[ck.n % CK_N] = now; ck.n++; ck.last_ms = now;
            if (ck.run) {
                ck.pos++;
                if (ck.pos % 24 == 0) { ck.beat_ms = now; ck.beat_pos = ck.pos; ck.beat_seq++; }
            }
            break;
        case 0xFA: ck.run = 1; ck.pos = -1; break;
        case 0xFB: ck.run = 1; break;
        case 0xFC: ck.run = 0; break;
        case 0xF2: ck.pos = (int32_t)((pkt >> 16 & 0x7Fu) | (pkt >> 24 & 0x7Fu) << 7) * 6 - 1; break;
        default: break;
        }
    }
}

static void send_params(void)
{
    eng_params(ck_bpm > 0.0f ? ck_bpm : (float)set.bpm, cutoff_hz(), reso_q(), set.side ? 0.75f : 0.0f, set.drums > 0);
}

static void clock_follow(uint32_t now)
{
    uint32_t f, n, t0, t1, seq, bms;
    int32_t bpos;
    uint8_t run;
    float bpm = 0.0f;
    f = plat_lock();
    n = ck.n; t1 = ck.t[(n + CK_N - 1u) % CK_N]; t0 = ck.t[n % CK_N];
    seq = ck.beat_seq; bms = ck.beat_ms; bpos = ck.beat_pos; run = ck.run;
    plat_unlock(f);
    if (n >= CK_N && now - t1 < 300u && t1 - t0 > 0u) bpm = 120000.0f / (float)(t1 - t0);   /* 48 clocks = 2 beats */
    if (bpm > 0.0f && (bpm < 30.0f || bpm > 300.0f)) bpm = 0.0f;
    if (bpm > 0.0f) {
        float b = ck_bpm > 0.0f ? ck_bpm + 0.25f * (bpm - ck_bpm) : bpm;
        b = (float)(int32_t)(b * 10.0f + 0.5f) * 0.1f;               /* to a tenth */
        if (ck_bpm <= 0.0f) app_say("MIDI SYNC", "TEMPO FROM USB");
        if (b != ck_bpm) { ck_bpm = b; send_params(); }
        if (seq != ck_seen_seq) {
            ck_seen_seq = seq;
            if (run) { eng_sync((int)((bpos / 6) & 15), bms); ck_placed = 1; }
        }
        if (!run && ck_placed) { eng_sync(-1, 0); ck_placed = 0; }   /* the host stopped: free again, its tempo kept */
    } else if (ck_bpm > 0.0f) {
        ck_bpm = 0.0f;
        eng_sync(-1, 0);
        ck_placed = 0;
        send_params();
        app_say("MIDI SYNC", "OFF");
    }
}
static void send_fx(void)
{
    eng_fx((float)set.reverb * 0.01f, (float)set.delay * 0.01f, (float)set.trem * 0.01f, (float)set.chorus * 0.01f);
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
    d.kit = DRUM_KIT[set.dpat];
    {   /* the main page's DRUMS is the kit's master level */
        float m = (float)set.drums * 0.01f;
        m = m * (1.6f - 0.6f * m);
        eng_drums(&d, (float)set.dkick * 0.01f * m, (float)set.dhat * 0.01f * m, (float)set.drim * 0.01f * m);
    }
}
static void send_bass(void) { eng_bass(set.bsound, set.bmotif, (float)set.blevel * 0.01f, (float)set.bcut * 0.01f); }

/* ------------------------------------------------------------------ playing --- */
static int cur_idx = -1, held_idx = -1, active;
static char chord_txt[16] = "BLOP";
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
enum { PG_MAIN, PG_FX, PG_ACC, PG_GROOVE, PG_BASS, PG_DECOR, PG_DRUMS, NPAGE };
static const char *const PAGE_NAME[NPAGE] = {"MAIN", "FX", "CHORDS", "GROOVE", "BASS", "DECOR", "DRUMS"};
static const uint8_t PAGE_BTN[NPAGE] = {0xFF, B_FX, B_SEL, B_ENV, B_LFO, B_EDIT, B_GLO};
static const uint8_t PAGE_RGB[NPAGE][3] = {{255, 150, 220}, {190, 150, 255}, {120, 230, 255}, {255, 214, 110},
                                          {140, 240, 150}, {235, 235, 240}, {255, 140, 100}};

enum { KD_PCT, KD_OFF, KD_GLIDE, KD_BPM, KD_BIP, KD_LIST };
enum { G_NONE, G_PARAMS, G_FX, G_GROOVE, G_DRUMS, G_BASS, G_CHORD, G_DRUMLVL };
typedef struct {
    const char *lab;
    uint8_t *v;
    uint8_t lo, hi, kind, grp;
    const char *const *names;
} kdef_t;
static const kdef_t KD[NPAGE][4] = {
    {{"CUTOFF", &set.cutoff, 0, 100, KD_PCT, G_PARAMS, 0}, {"GLIDE", &set.glide, 0, 100, KD_GLIDE, G_NONE, 0},
     {"LENGTH", &set.gate, 0, 100, KD_PCT, G_GROOVE, 0}, {"DRUMS", &set.drums, 0, 100, KD_OFF, G_DRUMLVL, 0}},
    {{"DELAY", &set.delay, 0, 100, KD_OFF, G_FX, 0}, {"TREMOLO", &set.trem, 0, 100, KD_OFF, G_FX, 0},
     {"CHORUS", &set.chorus, 0, 100, KD_OFF, G_FX, 0}, {"REVERB", &set.reverb, 0, 100, KD_PCT, G_FX, 0}},
    {{"KEY", &set.key, 0, 11, KD_LIST, G_CHORD, NOTE}, {"VOICING", &set.voicing, 0, 2, KD_LIST, G_CHORD, VOICING_NAME},
     {"RICHNESS", &set.rich, 0, 3, KD_LIST, G_CHORD, RICH_NAME}, {"STRUM", &set.strum, 0, 100, KD_OFF, G_GROOVE, 0}},
    {{"BPM", &set.bpm, 70, 160, KD_BPM, G_PARAMS, 0}, {"SWING", &set.swing, 0, 100, KD_OFF, G_GROOVE, 0},
     {"DENSITY", &set.dens, 0, 100, KD_BIP, G_GROOVE, 0}, {"SIDECHAIN", &set.side, 0, 1, KD_LIST, G_PARAMS, ONOFF_NAME}},
    {{"SOUND", &set.bsound, 0, NBASS_SOUND - 1, KD_LIST, G_BASS, BASS_NAME}, {"LEVEL", &set.blevel, 0, 100, KD_PCT, G_BASS, 0},
     {"RIFF", &set.bmotif, 0, NMOTIF - 1, KD_LIST, G_BASS, MOTIF_NAME}, {"FILTER", &set.bcut, 0, 100, KD_PCT, G_BASS, 0}},
    {{"CREATURE", &set.creature, 0, NCREATURE - 1, KD_LIST, G_NONE, CREATURE_NAME},
     {"EYES", &set.eyes, 0, NEYE - 1, KD_LIST, G_NONE, EYE_NAME},
     {"BACKDROP", &set.decor, 0, NDECOR - 1, KD_LIST, G_NONE, DECOR_NAME},
     {"EXTRA", &set.acc, 0, NACC - 1, KD_LIST, G_NONE, ACC_NAME}},
    {{"PATTERN", &set.dpat, 0, NDRUM - 1, KD_LIST, G_DRUMS, DRUM_NAME}, {"KICK", &set.dkick, 0, 100, KD_OFF, G_DRUMS, 0},
     {"HI-HAT", &set.dhat, 0, 100, KD_OFF, G_DRUMS, 0}, {"RIM", &set.drim, 0, 100, KD_OFF, G_DRUMS, 0}},
};

static int settings_ok(void)
{
    int p, k;
    if (set.magic != SET_MAGIC || set.grid >= NGRID || set.octave < -2 || set.octave > 2 || set.sound >= NFAMILY * PER_FAMILY
        || set.rhythm >= NRHYTHM || set.page >= NPAGE)
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
    uint8_t help, help_drawn, home_long;
    uint32_t reset_at;                   /* HOME + REC held since (0: not) */
    uint8_t reset_shown, reset_done;
    uint8_t bowl, dark, clean;           /* fun: the bowl view, the light off, no info on the screen */
} ui;
static void fun(int k);                  /* the bottom row (screen part) */

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
    case KD_BPM: if (ck_bpm > 0.0f) { int_text((int32_t)(ck_bpm + 0.5f), b); text_cat(b, " SYNC"); } else int_text(v, b); break;
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

static uint8_t prev_drums;
static void apply_group(int g)
{
    switch (g) {
    case G_PARAMS: send_params(); break;
    case G_FX: send_fx(); break;
    case G_GROOVE: send_groove(); break;
    case G_DRUMS: send_drums(); break;
    case G_BASS: send_bass(); break;
    case G_CHORD: rechord(); break;
    case G_DRUMLVL:
        if ((*KD[PG_MAIN][3].v > 0) != (prev_drums > 0)) send_params();
        send_drums();
        break;
    default: break;
    }
}

static void knob(int k, int32_t e)
{
    const kdef_t *d = &KD[set.page][k];
    prev_drums = set.drums;
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
        wrap_u8(&set.grid, e, NGRID); set.rich = GRID_DEF[set.grid].rich; app_say("GRID", GRID_DEF[set.grid].name);
        rechord();
        break;
    case EN_ALGO:
        wrap_u8(&set.rhythm, e, NRHYTHM);
        app_say("RHYTHM", set.rhythm ? RHYTHM_NAME[set.rhythm] : "OFF");
        send_rhythm();
        if (!set.rhythm && held_idx < 0 && active) stop();   /* no rhythm, no latch */
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

static void button(int k)
{
    char t[8];
    int p;
    for (p = 1; p < NPAGE; p++)
        if (PAGE_BTN[p] == k) {          /* a page; the same button again: back to the main page */
            set.page = (uint8_t)(set.page == p ? PG_MAIN : p);
            dirty();
            return;
        }
    switch (k) {
    case B_HOME: ui.home_at = plat_ms(); ui.home_long = 0; return;    /* acts on release (held = help) */
    case B_SAVE:
        ui.bowl ^= 1u;
        eng_muffle(ui.bowl ? 1.0f : 0.0f);
        return;
    case B_REC:
        if ((ui.btn >> B_HOME) & 1u) return;      /* HOME + REC: the reset (input), not food */
        fun(k);
        return;
    case B_ARP: case B_SEQ: case B_PLAY: fun(k); return;
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
    clock_follow(now);
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(ui.keys & bit)) {
            /* with a rhythm the chord keeps playing: the same key again stops it */
            if (set.rhythm && active && held_idx < 0 && cur_idx == (int)k) stop();
            else { held_idx = (int)k; play((int)k); }
        } else if (!(keys & bit) && (ui.keys & bit) && held_idx == (int)k) {
            held_idx = -1;
            if (!set.rhythm) stop();
        }
    }
    for (k = 0; k < NB; k++) {
        uint32_t bit = 1u << k;
        if ((btn & bit) && !(ui.btn & bit)) button((int)k);
    }
    /* HOME + REC held 3 s: back to the settings of the first start (a countdown; let go before: nothing) */
    if (((btn >> B_HOME) & 1u) && ((btn >> B_REC) & 1u)) {
        uint32_t held;
        char t[4];
        if (!ui.reset_at) { ui.reset_at = now | 1u; ui.reset_shown = 0; ui.reset_done = 0; }
        ui.home_long = 1; ui.help = 0;
        held = now - ui.reset_at;
        if (!ui.reset_done && held >= 3000u) {
            uint8_t page = PG_MAIN;
            ui.reset_done = 1;
            if (active) stop();
            settings_defaults();
            set.page = page;
            ui.bowl = ui.dark = ui.clean = 0;
            eng_muffle(0.0f);
            send_sound(); send_rhythm(); send_groove(); send_drums(); send_bass(); send_fx(); send_params();
            dirty();
            app_say("RESET", "FIRST SETTINGS BACK");
        } else if (!ui.reset_done && ui.reset_shown != 1u + held / 1000u) {
            ui.reset_shown = (uint8_t)(1u + held / 1000u);
            t[0] = (char)('0' + 4 - ui.reset_shown); t[1] = 0;
            app_say("RESET IN", t);
        }
    } else if (ui.reset_at) {
        if (!ui.reset_done) app_say("RESET", "CANCELLED");
        ui.reset_at = 0;
    }
    /* HOME: a short press, back to the main page; held, the help screen while held */
    if ((btn >> B_HOME) & 1u) {
        if (!ui.home_long && now - ui.home_at > 600u) { ui.home_long = 1; ui.help = 1; ui.help_drawn = 0; }
        if (ui.reset_at) ui.help = 0;
    } else if ((ui.btn >> B_HOME) & 1u) {
        if (ui.home_long) { ui.help = 0; ui.help_drawn = 0; }
        else if (set.page != PG_MAIN) { set.page = PG_MAIN; dirty(); }
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
    /* lights: the keys glow, the one playing is lit; the page's button, the fun ones that are on, the pressed one */
    if (cur_idx >= 0) lit_keys |= 1u << (uint32_t)cur_idx;
    if (set.page != PG_MAIN) lit_btn |= 1u << PAGE_BTN[set.page];
    if (ui.bowl) lit_btn |= 1u << B_SAVE;
    if (ui.dark) lit_btn |= 1u << B_ARP;
    if (ui.clean) lit_btn |= LED_PLAY_GREEN;
    /* OCT- / OCT+: lit on the side the octave went, blinking at two octaves */
    if (set.octave) {
        uint32_t ob = 1u << (set.octave < 0 ? B_OCTDN : B_OCTUP);
        if (set.octave == 1 || set.octave == -1 || (now / 300u) % 2u) lit_btn |= ob;
    }
    lit_btn |= btn & ((1u << 14) - 1u);  /* a button lights fully while it is pressed */
    plat_lights(lit_btn, lit_keys, 0, (1u << NKEYS) - 1u, 1);
    /* the 12 buttons and OCT- / OCT+ always softly on (one LED colour each: the active one fully lit, the others mid) */
    plat_lights_mid(((1u << 14) - 1u) & ~lit_btn, 0);
}

void app_input_only(void) { input(); }

/* ------------------------------------------------------------------ screen (240 x 240) --- */
/* A creature (black, a bit wobbly, glowing eyes) in water seen like a macro photo of an LCD: the picture is
 * 80 x 80 big pixels, each drawn as 3 x 3 screen pixels (red, green, blue columns over a dark row).
 * It swims to the chord (left/right = the key, up/down = the pitch, gliding with GLIDE), puffs up on the
 * rhythm's hits, is squashed by the sidechain, each kick sends a ripple. The water changes colour with the
 * chords. CUTOFF: bright or murky water; REVERB: glowing specks; DELAY: echoes of the creature.
 * At the start, and while HOME is held: the title, BLOP by Obsimo. The bottom row plays with it: the bowl (SAVE), the light (ARP), a pirouette (SEQ), no info (PLAY),
 * food (REC). Info on two translucent bands. Each half (240 x 120) is drawn into the
 * canvas and sent, ~25 times a second. */
#define NC 80                            /* big pixels a side */
#define BAND_TOP 13                      /* rows 0..12 under the top band */
#define BAND_BOT 63                      /* rows 63..79 under the bottom band */
#define TAU_F 6.2831853f
#define NFLAKE 9

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

static inline float circle(float px, float py, float cx, float cy, float r)
{
    float dx = px - cx, dy = py - cy;
    return fm_sqrtf(dx * dx + dy * dy) - r;
}

static inline float ellipse(float px, float py, float cx, float cy, float rx, float ry)
{
    float qx = (px - cx) / rx, qy = (py - cy) / ry;
    return (fm_sqrtf(qx * qx + qy * qy) - 1.0f) * fm_minf(rx, ry);
}

static inline float hash2(int x, int y)
{
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xFFFFu) * (1.0f / 65536.0f);
}

/* ---- the scene's state */
enum { EYE_OPEN, EYE_SHUT, EYE_HEART, EYE_WIDE };
static struct {
    float t;                             /* seconds */
    float x, y, home_x;                  /* the creature, in big pixels (scene) */
    float dir, puff, spin, hue, hue_to;
    uint32_t kick_at, hit_at, blink_at, eat_at, rot_at, sleep_at, start_at;
    uint8_t started;
    int last_step, last_idx;
    uint32_t last_hit_ms;
    float ex[16], ey[16];                /* recent positions, for the delay's echoes */
    int epos;
    struct { float x, y, v, ph; uint8_t on; } sp[14];   /* glowing specks (reverb) */
    float sp_acc;
    struct { float x, y, v, ph; uint32_t at; uint8_t on; } fl[NFLAKE];   /* food */
    uint32_t rnd;
    float zoom, dark;                    /* bowl view 0..1, light off 0..1 */
    /* per frame, for the cells */
    float title, title_ms, rim, R, sq, sx, ripple_r, ripple_a, bright, eye_r, rot, rc, rs, bands, ecol[3];
    int eye_state;
    float eyes[2][2], tip[2];
    uint8_t has_tip;
} sc;

static float srnd(void)
{
    sc.rnd = sc.rnd * 1664525u + 1013904223u;
    return (float)(sc.rnd >> 8) * (1.0f / 16777216.0f);
}

static void scene_init(void)
{
    sc.x = sc.home_x = 40.0f;
    sc.y = 40.0f;
    sc.dir = 1.0f;
    sc.last_step = -1;
    sc.last_idx = -1;
    sc.rnd = 0x2468ACEu;
    sc.blink_at = 3000u;
    sc.rc = 1.0f;
}

static int symmetric(void) { int c = set.creature; return c == 2 || c == 3 || c == 4 || c == 6 || c == 7; }

/* where an accessory sits on each creature (its head's top), relative to the centre, in R */
static const float ACC_AT[NCREATURE][2] = {{0.25f, -0.92f}, {0.35f, -0.85f}, {0.0f, -0.92f}, {0.0f, -0.5f},
                                           {0.0f, -1.22f}, {0.05f, -1.3f}, {0.0f, -0.4f}, {0.0f, -0.6f}};

static float accessory_sdf(float px, float py)
{
    float R = sc.R, ax = ACC_AT[set.creature][0] * R, ay = ACC_AT[set.creature][1] * R, d;
    int i;
    switch (set.acc) {
    case 1:                              /* party hat */
        d = capsule(px, py, ax, ay + 0.05f * R, ax + 0.12f * R, ay - 0.75f * R, 0.3f * R, 0.03f * R);
        return fm_minf(d, circle(px, py, ax + 0.13f * R, ay - 0.8f * R, 0.12f * R));
    case 2: {                            /* crown */
        float bx = fm_fabsf(px - ax) - 0.42f * R, by = fm_fabsf(py - (ay - 0.08f * R)) - 0.12f * R;
        d = fm_maxf(bx, by);
        for (i = -1; i <= 1; i++)
            d = fm_minf(d, capsule(px, py, ax + (float)i * 0.3f * R, ay - 0.12f * R, ax + (float)i * 0.34f * R,
                                   ay - 0.52f * R, 0.12f * R, 0.02f * R));
        return d;
    }
    case 3: {                            /* headphones */
        float cy = ay + 0.6f * R, r = circle(px, py, ax, cy, 0.72f * R);
        d = py < cy - 0.1f * R ? fm_fabsf(r) - 0.08f * R : 1e3f;
        d = fm_minf(d, circle(px, py, ax - 0.72f * R, cy - 0.05f * R, 0.22f * R));
        return fm_minf(d, circle(px, py, ax + 0.72f * R, cy - 0.05f * R, 0.22f * R));
    }
    case 4:                              /* bow */
        d = capsule(px, py, ax, ay - 0.1f * R, ax - 0.45f * R, ay - 0.3f * R, 0.05f * R, 0.2f * R);
        d = fm_minf(d, capsule(px, py, ax, ay - 0.1f * R, ax + 0.45f * R, ay - 0.3f * R, 0.05f * R, 0.2f * R));
        return fm_minf(d, circle(px, py, ax, ay - 0.12f * R, 0.11f * R));
    case 5:                              /* antenna (its ball glows: see tip) */
        d = capsule(px, py, ax, ay + 0.05f * R, ax + 0.15f * R + sinr(sc.t * 3.0f) * 0.08f * R, ay - 0.8f * R,
                    0.07f * R, 0.06f * R);
        return fm_minf(d, circle(px, py, ax + 0.15f * R + sinr(sc.t * 3.0f) * 0.08f * R, ay - 0.85f * R, 0.13f * R));
    default: return 1e3f;
    }
}

/* the creature's distance field at (px, py), relative to its centre, facing right; < 0 inside */
static float creature_sdf(float px, float py)
{
    float R = sc.R, d, dist, a;
    int i, k;
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
        break;
    }
    case 1: {                            /* ocean sunfish */
        float flap = sinr(sc.t * 4.0f) * 0.16f * R;
        d = ellipse(px, py, 0.0f, 0.0f, R, 0.9f * R) + 0.04f * R * sinr(px * 0.5f + py * 0.3f + 1.0f);
        d = fm_minf(d, capsule(px, py, -0.2f * R, -0.6f * R, -0.45f * R + flap, -(1.75f + 0.1f * sc.puff) * R, 0.22f * R, 0.06f * R));
        d = fm_minf(d, capsule(px, py, -0.2f * R, 0.6f * R, -0.45f * R - flap, (1.7f + 0.1f * sc.puff) * R, 0.22f * R, 0.06f * R));
        for (i = 0; i < 5; i++) d = fm_minf(d, circle(px, py, -0.98f * R, (-0.55f + 0.27f * (float)i) * R, 0.2f * R));
        break;
    }
    case 2: {                            /* jellyfish: it pulses on the hits */
        float rx = R * (1.0f + 0.14f * sc.puff), ry = R * (1.0f - 0.16f * sc.puff);
        d = ellipse(px, py, 0.0f, 0.0f, rx, ry) - 0.05f * R * sinr(px * 0.7f);
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
        break;
    }
    case 3: {                            /* starfish: turns a little on the hits */
        float arm, r, Rs = R * 1.2f;
        dist = fm_sqrtf(px * px + py * py);
        a = at2(py, px);
        arm = 0.5f + 0.5f * fm_sin_turns(5.0f * (a - sc.spin) + 0.25f);
        r = Rs * (0.38f + 0.62f * arm * arm) * (1.0f + 0.05f * fm_sin_turns(7.0f * a + 0.3f)) * (1.0f + 0.1f * fm_sin_turns(a + 0.5f));
        d = dist - r;
        break;
    }
    case 4: {                            /* octopus: a big head, eight wavy arms that spread on the hits */
        d = ellipse(px, py, 0.0f, -0.35f * R, 0.75f * R, 0.9f * R);
        for (i = 0; i < 8; i++) {
            float bx = (-0.6f + 0.17f * (float)i) * R, pxp = bx, pyp = 0.3f * R;
            for (k = 1; k <= 2; k++) {
                float x1 = bx * (1.0f + (0.5f + 0.4f * sc.puff) * (float)k) + sinr(sc.t * 2.5f + (float)i * 0.9f + (float)k) * 0.18f * R * (float)k;
                float y1 = 0.3f * R + (float)k * 0.45f * R;
                d = fm_minf(d, capsule(px, py, pxp, pyp, x1, y1, (0.17f - 0.05f * (float)k) * R, (0.12f - 0.05f * (float)k) * R));
                pxp = x1; pyp = y1;
            }
        }
        break;
    }
    case 5: {                            /* seahorse, bobbing */
        static const float P[7][3] = {{0.0f, -0.7f, 0.36f}, {-0.2f, -0.15f, 0.5f}, {-0.05f, 0.45f, 0.36f},
                                      {0.05f, 0.95f, 0.2f}, {0.38f, 1.25f, 0.13f}, {0.58f, 0.98f, 0.1f},
                                      {0.36f, 0.82f, 0.08f}};
        float b = sinr(sc.t * 1.5f) * 0.08f * R, fin = sinr(sc.t * 12.0f) * 0.08f * R;
        py -= b;
        d = circle(px, py, 0.05f * R, -1.0f * R, 0.32f * R);
        d = fm_minf(d, capsule(px, py, 0.3f * R, -0.97f * R, 0.85f * R, -0.85f * R, 0.12f * R, 0.08f * R));
        for (i = 0; i < 6; i++)
            d = fm_minf(d, capsule(px, py, P[i][0] * R, P[i][1] * R, P[i + 1][0] * R, P[i + 1][1] * R, P[i][2] * R, P[i + 1][2] * R));
        d = fm_minf(d, capsule(px, py, -0.5f * R, -0.25f * R, -0.78f * R + fin, 0.0f, 0.14f * R, 0.05f * R));
        d = fm_minf(d, capsule(px, py, -0.1f * R, -1.2f * R, -0.25f * R, -1.4f * R, 0.08f * R, 0.03f * R));
        break;
    }
    case 6: {                            /* manta ray, from the front: the wings flap */
        float A = 1.7f * R, B = 0.42f * R, flap = sinr(sc.t * 3.0f) * (0.35f + 0.25f * sc.puff) * R;
        float yy = py - flap * fm_fabsf(px) / A;
        d = (fm_fabsf(px) / A + fm_fabsf(yy) / B - 1.0f) * (A * B / fm_sqrtf(A * A + B * B));
        d = fm_minf(d, ellipse(px, py, 0.0f, 0.0f, 0.55f * R, 0.5f * R));
        d = fm_minf(d, capsule(px, py, -0.3f * R, -0.35f * R, -0.45f * R, -0.75f * R, 0.1f * R, 0.06f * R));
        d = fm_minf(d, capsule(px, py, 0.3f * R, -0.35f * R, 0.45f * R, -0.75f * R, 0.1f * R, 0.06f * R));
        d = fm_minf(d, capsule(px, py, 0.0f, 0.45f * R, 0.05f * R + sinr(sc.t * 2.0f) * 0.15f * R, 1.6f * R, 0.06f * R, 0.03f * R));
        break;
    }
    default: {                           /* crab: snaps its claws on the hits */
        float s;
        d = ellipse(px, py, 0.0f, 0.0f, R, 0.62f * R);
        for (s = -1.0f; s <= 1.0f; s += 2.0f) {
            float notch = (0.15f + 0.1f * sc.puff) * R, pc;
            d = fm_minf(d, capsule(px, py, s * 0.28f * R, -0.5f * R, s * 0.32f * R, -0.95f * R, 0.07f * R, 0.06f * R));
            d = fm_minf(d, capsule(px, py, s * 0.85f * R, -0.15f * R, s * 1.25f * R, -0.6f * R, 0.14f * R, 0.14f * R));
            pc = circle(px, py, s * 1.35f * R, -0.88f * R, 0.32f * R);
            pc = fm_maxf(pc, -circle(px, py, s * 1.55f * R, -1.1f * R, notch));
            d = fm_minf(d, pc);
            for (i = 0; i < 3; i++) {
                float lift = sinr(sc.t * 6.0f + (float)i * 2.0f + s) * 0.08f * R;
                d = fm_minf(d, capsule(px, py, s * 0.7f * R, (0.2f + 0.15f * (float)i) * R, s * 1.35f * R,
                                       (0.55f + 0.3f * (float)i) * R + lift, 0.08f * R, 0.06f * R));
            }
        }
        break;
    }
    }
    if (set.acc) d = fm_minf(d, accessory_sdf(px, py));
    return d;
}

/* scene -> the creature's own frame (unrotated, facing right, unsquashed) */
static void to_local(float x, float y, float *lx, float *ly)
{
    float dx = x - sc.x, dy = y - sc.y, rx = dx * sc.rc + dy * sc.rs, ry = -dx * sc.rs + dy * sc.rc;
    if (!symmetric()) rx *= sc.dir;
    *lx = rx / sc.sx;
    *ly = ry / sc.sq;
}

static void to_scene(float lx, float ly, float *x, float *y)
{
    float rx = lx * sc.sx, ry = ly * sc.sq;
    if (!symmetric()) rx *= sc.dir;
    *x = sc.x + rx * sc.rc - ry * sc.rs;
    *y = sc.y + rx * sc.rs + ry * sc.rc;
}

/* the eyes' places, relative to the centre, facing right, in R */
static const float EYE_AT[NCREATURE][4] = {{0.1f, -0.24f, 0.56f, -0.2f}, {0.38f, -0.2f, 0.68f, -0.18f},
                                           {-0.3f, -0.25f, 0.3f, -0.25f}, {-0.24f, -0.12f, 0.24f, -0.12f},
                                           {-0.28f, -0.35f, 0.28f, -0.35f}, {-0.02f, -1.08f, 0.22f, -1.05f},
                                           {-0.28f, -0.12f, 0.28f, -0.12f}, {-0.32f, -1.0f, 0.32f, -1.0f}};
static void eye_places(void)
{
    const float *e = EYE_AT[set.creature];
    float R = sc.R, b = set.creature == 5 ? sinr(sc.t * 1.5f) * 0.08f * R : 0.0f;
    to_scene(e[0] * R, e[1] * R + b, &sc.eyes[0][0], &sc.eyes[0][1]);
    to_scene(e[2] * R, e[3] * R + b, &sc.eyes[1][0], &sc.eyes[1][1]);
    sc.has_tip = set.acc == 5;
    if (sc.has_tip) {
        float ax = ACC_AT[set.creature][0] * R, ay = ACC_AT[set.creature][1] * R;
        to_scene(ax + 0.15f * R + sinr(sc.t * 3.0f) * 0.08f * R, ay - 0.85f * R, &sc.tip[0], &sc.tip[1]);
    }
}

/* one eye's light on a cell: adds to c */
static void eye_light(float *c, float cx, float cy, float ex, float ey)
{
    const float *col = sc.ecol;
    float dx = cx - ex, dy = cy - ey, r = sc.eye_r, d, glow;
    int core, shape = set.eyes, st = sc.eye_state;
    if (shape == 3 && st == EYE_OPEN) {  /* laser: a beam to the edge, the way it looks */
        float face = symmetric() ? (ex < sc.x ? -1.0f : 1.0f) : sc.dir;
        if (dx * face > 0.0f && fm_fabsf(dy) < 1.2f) {
            float k = (0.75f + 0.25f * sinr(sc.t * 30.0f)) * fm_expf(-dy * dy * 2.0f);
            c[0] += k * 1.0f; c[1] += k * 0.12f; c[2] += k * 0.1f;
        }
    }
    if (dx > 6.0f * r || dx < -6.0f * r || dy > 6.0f * r || dy < -6.0f * r) return;
    d = fm_sqrtf(dx * dx + dy * dy);
    if (st == EYE_SHUT) {
        core = fm_fabsf(dy) < 0.4f && fm_fabsf(dx) < 1.3f * r;
        glow = 0.22f * fm_expf(-(dx * dx * 0.3f + dy * dy * 2.0f));
    } else {
        float R2 = r * 1.45f;
        if (st == EYE_HEART) shape = 1;
        if (st == EYE_WIDE) shape = 0;
        switch (shape) {
        case 1: {                        /* heart */
            float u = dx / R2, v = -dy / R2 + 0.3f, q = u * u + v * v - 1.0f;
            core = q * q * q - u * u * v * v * v < 0.0f;
            break;
        }
        case 2: {                        /* star */
            float s = fm_sin_turns(5.0f * at2(dy, dx) + 0.5f), m = s > 0.0f ? s * s : 0.0f;
            core = d < R2 * (0.42f + 0.65f * m);
            break;
        }
        case 3: core = d < r * 0.9f; break;          /* laser: a small hot core */
        case 4: core = d < R2 * 1.1f && frac(d * 0.8f - at2(dy, dx) + sc.t * 1.5f) < 0.5f; break;   /* spiral */
        case 5: core = d < R2 && (fm_fabsf(dx - dy) < 0.6f || fm_fabsf(dx + dy) < 0.6f); break;      /* cross (KO) */
        default: core = d < (st == EYE_WIDE ? r * 1.5f : r); break;                                   /* round */
        }
        glow = 0.55f * fm_expf(-(d / (R2 * 1.1f)) * (d / (R2 * 1.1f)));
    }
    if (shape == 3 && st == EYE_OPEN) { c[0] += glow; c[1] += glow * 0.15f; c[2] += glow * 0.1f;
        if (core) { c[0] = 1.2f; c[1] = 0.6f; c[2] = 0.5f; } return; }
    c[0] += glow * col[0]; c[1] += glow * col[1]; c[2] += glow * col[2];
    if (core) { c[0] = col[0] * 0.5f + 0.55f; c[1] = col[1] * 0.5f + 0.55f; c[2] = col[2] * 0.5f + 0.55f; }
}

/* the background at a point of the scene */
static void water(float x, float y, float *c)
{
    float t = sc.t, h = sc.hue;
    switch (set.decor) {
    case 1: {                            /* rainbow moire on lilac */
        float u = x * 0.55f + y * 0.95f + 3.0f * sinr(y * 0.08f + t) + 2.0f * sinr(x * 0.05f - t * 0.5f), st;
        st = 0.5f + 0.5f * sinr(u * 0.9f);
        hsv(u * 0.045f + t * 0.05f + h, 0.85f * (0.35f + 0.65f * st), 0.85f, c);
        c[0] = c[0] * 0.72f + 0.2f; c[1] = c[1] * 0.72f + 0.17f; c[2] = c[2] * 0.72f + 0.27f;
        break;
    }
    case 2: {                            /* embers, darker below */
        float k = clamp01(y / (float)NC), w = 0.05f * sinr(x * 0.4f - y * 0.3f + t), top[3];
        hsv(0.03f + h * 0.12f, 0.7f, 0.95f, top);
        c[0] = top[0] * (1.0f - k) + 0.35f * k + w; c[1] = top[1] * (1.0f - k) + 0.05f * k + w;
        c[2] = top[2] * (1.0f - k) + 0.12f * k + w;
        break;
    }
    case 3: {                            /* acid: rings around the creature, kicked by the kick */
        float dx = x - sc.x, dy = y - sc.y, d = fm_sqrtf(dx * dx + dy * dy), s;
        s = 0.6f + 0.3f * sinr(d * 0.7f - t * 3.0f + sc.ripple_a * 6.0f);
        hsv(d * 0.035f - t * 0.2f + 0.08f * sinr(x * 0.2f + t) + h, s, 0.85f, c);
        break;
    }
    case 4: {                            /* abyss: near black, glowing plankton drifting up */
        int ix = (int)fm_floorf(x * 0.5f), iy = (int)fm_floorf((y + t * 1.2f) * 0.5f);
        float hv = hash2(ix, iy), k = clamp01(y / (float)NC);
        c[0] = 0.03f + 0.02f * (1.0f - k); c[1] = 0.035f + 0.03f * (1.0f - k); c[2] = 0.09f + 0.06f * (1.0f - k);
        if (hv < 0.04f) {
            float tw = 0.5f + 0.5f * sinr(t * 2.0f + hv * 600.0f), pc[3];
            hsv(hv < 0.02f ? 0.5f + h * 0.2f : 0.85f + h * 0.2f, 0.7f, 1.0f, pc);
            c[0] += pc[0] * tw * 0.8f; c[1] += pc[1] * tw * 0.8f; c[2] += pc[2] * tw * 0.8f;
        }
        break;
    }
    case 5: {                            /* coral: peach to deep rose, soft waves */
        float k = clamp01(y / (float)NC), w = 0.06f * sinr(x * 0.2f + y * 0.1f + t), top[3];
        hsv(0.02f + h * 0.08f, 0.45f, 1.0f, top);
        c[0] = top[0] * (1.0f - k) + 0.55f * k + w; c[1] = top[1] * (1.0f - k) + 0.16f * k + w * 0.6f;
        c[2] = top[2] * (1.0f - k) + 0.32f * k + w * 0.8f;
        break;
    }
    case 6: {                            /* aurora: curtains of light over dark water */
        float cu = sinr(x * 0.18f + 2.0f * sinr(y * 0.05f + t * 0.5f) + t * 0.4f), fade = clamp01(1.0f - y / (float)NC), pc[3];
        cu = cu > 0.0f ? cu * cu : 0.0f;
        hsv(0.38f + 0.25f * x / (float)NC + h * 0.3f, 0.8f, 1.0f, pc);
        cu *= fade * fade * 0.9f;
        c[0] = 0.02f + pc[0] * cu; c[1] = 0.06f + pc[1] * cu; c[2] = 0.1f + pc[2] * cu;
        break;
    }
    case 7: {                            /* an old green screen */
        float l = 0.35f + 0.15f * sinr(x * 0.25f + t) + 0.1f * sinr(y * 0.5f - t * 2.0f);
        if (((int)fm_floorf(y)) & 1) l *= 0.8f;
        c[0] = l * 0.15f; c[1] = l; c[2] = l * 0.2f;
        break;
    }
    default: {                           /* soft blue, tinted by the chord */
        float v = 0.04f * sinr(x * 0.3f + y * 0.2f + t), tint[3];
        hsv(0.6f + h * 0.25f, 0.5f, 0.86f, tint);
        c[0] = tint[0] + v; c[1] = tint[1] + v; c[2] = tint[2] + v;
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

#define BOWL_CY 36.0f                    /* the bowl view's centre (screen rows): the table above the bottom band */
/* the room around the bowl, in screen big pixels; s the bowl's scale */
static void room(float sx, float sy, float s, float *c)
{
    float top = BOWL_CY + (42.0f - BOWL_CY) * s + 40.0f * s, d;
    int i;
    if (sy > top) {                      /* the table: its top, a bright edge, the front darker */
        float g = 0.05f * sinr(sx * 0.9f + sy * 0.15f) + 0.03f * sinr(sx * 0.21f);
        if (sy < top + 4.0f) { c[0] = 0.5f + g; c[1] = 0.31f + g * 0.6f; c[2] = 0.16f + g * 0.3f; }
        else if (sy < top + 5.0f) { c[0] = 0.66f; c[1] = 0.45f; c[2] = 0.26f; }
        else { c[0] = 0.26f + g; c[1] = 0.14f + g * 0.5f; c[2] = 0.08f + g * 0.3f; }
        if (sy < top + 4.0f) {           /* the bowl's shadow, and the plant's */
            float k = clamp01(1.0f - fm_fabsf(sx - 40.0f) / (36.0f * s)) * clamp01(1.0f - (sy - top) / 4.0f);
            float k2 = clamp01(1.0f - fm_fabsf(sx - 67.0f) / 6.0f) * clamp01(1.0f - (sy - top) / 3.0f);
            k = fm_maxf(k, k2);
            c[0] *= 1.0f - 0.55f * k; c[1] *= 1.0f - 0.55f * k; c[2] *= 1.0f - 0.55f * k;
        }
    } else {                             /* the wall, warm and dim, a soft lamp light */
        float k = sy / 80.0f, lamp = 0.18f / (1.0f + ((sx - 40.0f) * (sx - 40.0f) + (sy - 6.0f) * (sy - 6.0f)) * 0.003f);
        c[0] = 0.13f + 0.07f * k + lamp; c[1] = 0.09f + 0.04f * k + lamp * 0.8f; c[2] = 0.11f + 0.02f * k + lamp * 0.5f;
        if (sx > 4.0f && sx < 17.0f && sy > 15.0f && sy < 35.0f) {     /* a window: night sky, its frame */
            int frame = sx < 5.0f || sx > 16.0f || sy < 16.0f || sy > 34.0f || fm_fabsf(sx - 10.5f) < 0.6f
                        || fm_fabsf(sy - 25.0f) < 0.6f;
            if (frame) { c[0] = 0.32f; c[1] = 0.24f; c[2] = 0.2f; }
            else {
                float v = (sy - 15.0f) / 20.0f;
                c[0] = 0.12f + 0.15f * v; c[1] = 0.2f + 0.15f * v; c[2] = 0.42f + 0.1f * v;
                if (hash2((int)sx, (int)sy) < 0.06f) { c[0] += 0.5f; c[1] += 0.5f; c[2] += 0.45f; }   /* stars */
            }
        }
        /* the plant: a terracotta pot, leaves swaying */
        if (sy > 45.0f && sy <= top) {
            float hw = 4.2f - (sy - 45.0f) * 0.12f;
            if (fm_fabsf(sx - 67.0f) < hw) {
                if (sy < 47.0f) { c[0] = 0.72f; c[1] = 0.38f; c[2] = 0.24f; }
                else { c[0] = 0.58f - 0.02f * (sy - 47.0f); c[1] = 0.28f; c[2] = 0.17f; }
            }
        }
        d = 1e3f;
        for (i = 0; i < 6; i++) {
            static const float TIP[6][2] = {{57.0f, 33.0f}, {61.0f, 26.0f}, {66.0f, 22.0f}, {71.0f, 25.0f},
                                            {76.0f, 31.0f}, {78.0f, 40.0f}};
            float sw = sinr(sc.t * 0.8f + (float)i * 1.3f) * 1.4f;
            d = fm_minf(d, capsule(sx, sy, 67.0f, 45.5f, TIP[i][0] + sw, TIP[i][1], 1.0f, 1.7f));
        }
        if (d < 0.6f) {
            float k = clamp01(0.6f - d);
            float lc = 0.5f + 0.5f * sinr(sx * 0.7f + sy * 0.5f);
            c[0] = c[0] * (1.0f - k) + (0.12f + 0.08f * lc) * k;
            c[1] = c[1] * (1.0f - k) + (0.38f + 0.2f * lc) * k;
            c[2] = c[2] * (1.0f - k) + (0.16f + 0.06f * lc) * k;
        }
    }
    c[0] *= 1.0f - 0.85f * sc.dark; c[1] *= 1.0f - 0.85f * sc.dark; c[2] *= 1.0f - 0.85f * sc.dark;
}

/* the title, BLOP: a 5 x 7 font, each dot 3 x 3 big pixels, the letters bobbing */
static const uint8_t TITLE_FONT[4][7] = {
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},   /* B */
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},   /* L */
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},   /* O */
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},   /* P */
};
#define TITLE_X 7
#define TITLE_Y 11
static int title_dot(int cx, int cy)       /* the title's dot under a big pixel (0 / 1) */
{
    int lx = cx - TITLE_X, letter = lx / 17, col, row, oy;
    if (lx < 0 || letter > 3 || lx % 17 >= 15) return 0;
    oy = (int)(1.5f * sinr(sc.t * 4.0f + (float)letter * 1.2f) + 1.5f);
    row = (cy - TITLE_Y - oy) / 3;
    if (cy - TITLE_Y - oy < 0 || row > 6) return 0;
    col = (lx % 17) / 3;
    return (TITLE_FONT[letter][row] >> (4 - col)) & 1;
}
/* the lines under it, in the same lights (one big pixel a dot): BY OBSIMO a second after BLOP, and while HOME
 * is held GO STREAM MY MUSIC ;) after it; the letters drop in one after the other */
static const struct { char ch; uint8_t rows[7]; } GLYPH[] = {
    {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}}, {'Y', {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}}, {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}}, {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}}, {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}}, {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}}, {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}}, {';', {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x04, 0x08}},
    {')', {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}},
};
static const struct { const char *s; int x, y; float t0; uint8_t home_only; } LINE[3] = {
    {"BY OBSIMO", 13, 41, 1000.0f, 0}, {"GO STREAM", 13, 52, 1900.0f, 1}, {"MY MUSIC ;)", 7, 61, 2460.0f, 1},
};
static const uint8_t *glyph_rows(char ch)
{
    uint32_t i;
    for (i = 0; i < sizeof GLYPH / sizeof GLYPH[0]; i++) if (GLYPH[i].ch == ch) return GLYPH[i].rows;
    return 0;
}
static int line_dot(int l, int cx, int cy)
{
    const char *s = LINE[l].s;
    int lx = cx - LINE[l].x, i = lx / 6, n = 0, k, row;
    const uint8_t *g;
    float p, oy, q;
    if (lx < 0 || lx % 6 >= 5) return 0;
    for (k = 0; k < i; k++) { if (!s[k]) return 0; if (s[k] != ' ') n++; }
    if (!s[i] || s[i] == ' ' || !(g = glyph_rows(s[i]))) return 0;
    p = (sc.title_ms - LINE[l].t0 - 70.0f * (float)n) / 320.0f;     /* each letter's drop, 70 ms apart */
    if (p <= 0.0f) return 0;
    if (p > 1.0f) p = 1.0f;
    q = p - 1.0f;                                                    /* ease out, a little bounce */
    oy = -14.0f * (1.0f - (1.0f + 2.7f * q * q * q + 1.7f * q * q));
    row = cy - LINE[l].y - (int)(oy + (oy < 0.0f ? -0.5f : 0.5f));
    if (row < 0 || row > 6) return 0;
    return (g[row] >> (4 - lx % 6)) & 1;
}

static void title_light(int cx, int cy, float *c)
{
    float k = sc.title, g = 0.0f;
    int dx, dy, l;
    if (cx >= TITLE_X - 2 && cx <= TITLE_X + 68 && cy >= TITLE_Y - 2 && cy <= TITLE_Y + 26) {   /* BLOP */
        if (title_dot(cx, cy)) g = 1.0f;
        else
            for (dy = -1; dy <= 1; dy++)
                for (dx = -1; dx <= 1; dx++)
                    if (title_dot(cx + dx, cy + dy)) g = fm_maxf(g, 0.3f);
    }
    for (l = 0; l < 3 && g < 1.0f; l++) {                                                      /* the lines */
        if (LINE[l].home_only && !ui.help) continue;
        if (cy < LINE[l].y - 16 || cy > LINE[l].y + 8 || cx < LINE[l].x - 1 || cx > LINE[l].x + 66) continue;
        if (line_dot(l, cx, cy)) g = 1.0f;
        else
            for (dy = -1; dy <= 1; dy++)
                for (dx = -1; dx <= 1; dx++)
                    if (line_dot(l, cx + dx, cy + dy)) g = fm_maxf(g, 0.3f);
    }
    c[0] *= 1.0f - 0.6f * k; c[1] *= 1.0f - 0.6f * k; c[2] *= 1.0f - 0.6f * k;
    c[0] += k * g * 1.1f; c[1] += k * g * 0.86f; c[2] += k * g * 0.45f;
}

/* one cell's colour; (sx, sy) the cell on screen */
static void cell(int cx, int cy, float *c)
{
    float x = (float)cx + 0.5f, y = (float)cy + 0.5f, lx, ly, d, cov, halo, glass = 0.0f, s = 1.0f;
    int i, in_bowl = 1, air = 0, surface = 0;
    if (sc.zoom > 0.002f) {              /* the bowl: the scene, smaller, inside a round glass on a table */
        float bd;
        s = 1.0f - 0.55f * sc.zoom;
        x = 40.0f + (x - 40.0f) / s;
        y = BOWL_CY + (y - BOWL_CY) / s;
        bd = fm_sqrtf((x - 40.0f) * (x - 40.0f) + (y - 42.0f) * (y - 42.0f));
        if (bd > 46.0f || y < 6.0f) in_bowl = 0;                  /* the round glass, open at the top */
        else {
            glass = clamp01(1.0f - fm_fabsf(bd - 45.0f) * s / 0.9f) * sc.zoom;
            glass = fm_maxf(glass, clamp01(1.0f - fm_fabsf(y - 6.0f) * s / 0.8f) * sc.zoom * 0.8f);   /* the rim */
            if (y < 13.0f) air = 1;                                /* the water's surface a little below it */
            else if (y < 14.0f) surface = 1;
        }
    }
    if (!in_bowl) {                      /* the room: a wall with a window, a wooden table, a plant */
        room((float)cx + 0.5f, (float)cy + 0.5f, s, c);
    } else {
        water(x, y, c);
        if (air) { c[0] = c[0] * 0.25f + 0.2f; c[1] = c[1] * 0.25f + 0.18f; c[2] = c[2] * 0.25f + 0.2f; }
        if (surface) { c[0] += 0.25f; c[1] += 0.3f; c[2] += 0.35f; }
        c[0] *= sc.bright; c[1] *= sc.bright; c[2] *= sc.bright;
        c[0] *= 1.0f - 0.93f * sc.dark; c[1] *= 1.0f - 0.93f * sc.dark; c[2] *= 1.0f - 0.93f * sc.dark;
        /* the creature */
        to_local(x, y, &lx, &ly);
        if (lx > -2.4f * sc.R && lx < 2.4f * sc.R && ly > -2.4f * sc.R && ly < 2.4f * sc.R) {
            d = creature_sdf(lx, ly);
            cov = clamp01(0.5f - d * s);
            halo = d > 0.0f ? clamp01(1.0f - d / 6.0f) : 1.0f;
            halo *= halo;
            for (i = 0; i < 3; i++) c[i] = c[i] * (1.0f - 0.38f * halo) * (1.0f - cov) + 0.02f * cov;
            if (sc.rim > 0.0f) {         /* dark water: a thin glow on its edge, so it stays visible */
                float rk = clamp01(1.0f - fm_fabsf(d * s + 0.3f) / 1.1f) * sc.rim;
                c[0] += rk * sc.ecol[0]; c[1] += rk * sc.ecol[1]; c[2] += rk * sc.ecol[2];
            }
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
        /* specks of light (reverb), food */
        for (i = 0; i < 14; i++)
            if (sc.sp[i].on) {
                float ex = x - sc.sp[i].x, ey = y - sc.sp[i].y;
                if (ex > -2.5f && ex < 2.5f && ey > -2.5f && ey < 2.5f) {
                    float g = 0.55f / (1.0f + (ex * ex + ey * ey) * 1.6f);
                    c[0] += g * 0.9f; c[1] += g; c[2] += g;
                }
            }
        for (i = 0; i < NFLAKE; i++)
            if (sc.fl[i].on) {
                float ex = x - sc.fl[i].x, ey = y - sc.fl[i].y;
                if (ex > -3.0f && ex < 3.0f && ey > -3.0f && ey < 3.0f) {
                    float g = 1.2f / (1.0f + (ex * ex + ey * ey) * 1.2f);
                    c[0] += g; c[1] += g * 0.55f; c[2] += g * 0.2f;
                }
            }
        eye_light(c, x, y, sc.eyes[0][0], sc.eyes[0][1]);
        eye_light(c, x, y, sc.eyes[1][0], sc.eyes[1][1]);
        if (sc.has_tip) {                /* the antenna's ball */
            float ex = x - sc.tip[0], ey = y - sc.tip[1];
            if (ex > -4.0f && ex < 4.0f && ey > -4.0f && ey < 4.0f) {
                float g = 0.8f * fm_expf(-(ex * ex + ey * ey) * 0.35f);
                c[0] += g * sc.ecol[0]; c[1] += g * sc.ecol[1]; c[2] += g * sc.ecol[2];
            }
        }
        if (glass > 0.0f) {              /* the glass, with a highlight on the top left */
            float hl = (x < 40.0f && y < 42.0f) ? 0.5f : 0.2f;
            c[0] += glass * hl; c[1] += glass * hl; c[2] += glass * hl;
        }
    }
    if (sc.title > 0.0f) title_light(cx, cy, c);
    if (sc.bands > 0.0f && (cy < BAND_TOP || cy >= BAND_BOT)) {
        float k = 1.0f - 0.7f * sc.bands;
        c[0] *= k; c[1] *= k; c[2] *= k;
    }
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

/* ---- the bottom row */
static void fun(int k)
{
    uint32_t now = plat_ms();
    int i;
    switch (k) {
    case B_ARP: ui.dark ^= 1u; break;                                          /* the light */
    case B_SEQ: sc.rot_at = now; break;                                        /* a pirouette */
    case B_PLAY: ui.clean ^= 1u; break;                                        /* just the picture */
    case B_REC:                                                                /* food */
        for (i = 0; i < NFLAKE; i++) {
            sc.fl[i].on = 1;
            sc.fl[i].x = fm_clampf(sc.x + (srnd() - 0.5f) * 36.0f, 6.0f, 74.0f);
            sc.fl[i].y = (float)BAND_TOP + 1.0f - srnd() * 6.0f;
            sc.fl[i].v = 4.0f + srnd() * 4.0f;
            sc.fl[i].ph = srnd();
            sc.fl[i].at = now;
        }
        break;
    default: break;
    }
}

/* ---- the scene, once a frame */
static void scene_step(uint32_t now, float dt)
{
    int st, kick, i, hk, food = -1;
    uint32_t at;
    float pitch = 0.0f, ty, fd = 1e9f;
    sc.t += dt;
    /* where it wants to be: the key (left / right), the pitch (up / down) */
    if (cur_idx >= 0 && cur_idx != sc.last_idx) {
        float tx = 20.0f + (float)cur_idx * (40.0f / 26.0f);
        if (tx > sc.home_x + 0.5f) sc.dir = 1.0f;
        else if (tx < sc.home_x - 0.5f) sc.dir = -1.0f;
        sc.home_x = tx;
    }
    sc.last_idx = cur_idx;
    for (i = 0; i < 4; i++) pitch += vis_pitch(i, now);
    pitch *= 0.25f;
    ty = active ? 50.0f - (pitch - 52.0f) * (22.0f / 30.0f) : 40.0f + 2.5f * sinr(sc.t * 0.9f);
    ty = fm_clampf(ty, 28.0f, 50.0f);
    /* food: it goes for the nearest flake, and eats it */
    for (i = 0; i < NFLAKE; i++)
        if (sc.fl[i].on) {
            float dx = sc.fl[i].x - sc.x, dy = sc.fl[i].y - sc.y, dd = dx * dx + dy * dy;
            sc.fl[i].y = fm_minf(sc.fl[i].y + sc.fl[i].v * dt, (float)BAND_BOT - 2.0f);
            sc.fl[i].x += sinr(sc.t * 2.0f + sc.fl[i].ph * TAU_F) * dt * 2.0f;
            if (dd < fd) { fd = dd; food = i; }
            if (now - sc.fl[i].at > 9000u) sc.fl[i].on = 0;
        }
    if (food >= 0 && sc.fl[food].y > (float)BAND_TOP) {
        float dx = sc.fl[food].x - sc.x, dy = sc.fl[food].y - sc.y;
        if (dx > 0.5f) sc.dir = 1.0f; else if (dx < -0.5f) sc.dir = -1.0f;
        sc.x += dx * fm_minf(1.0f, dt * 2.6f);
        sc.y += dy * fm_minf(1.0f, dt * 2.6f);
        if (fm_sqrtf(fd) < sc.R * 0.7f) {
            sc.fl[food].on = 0;
            sc.puff = 1.0f;
            sc.eat_at = now;
        }
    } else {
        float k = dt / (glide_s() + 0.12f);
        sc.x += (sc.home_x - sc.x) * fm_minf(1.0f, k * 2.5f);
        sc.y += (ty - sc.y) * fm_minf(1.0f, dt * (active ? 8.0f : 1.5f));
    }
    sc.x = fm_clampf(sc.x, 10.0f, 70.0f);
    sc.y = fm_clampf(sc.y, 20.0f, 56.0f);
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
        sc.puff = fm_maxf(sc.puff, hk == 1 ? 1.0f : 0.6f);
        sc.spin += hk == 1 ? 0.04f : 0.02f;
    }
    st = eng_last_step(&at, &kick);
    if (st != sc.last_step) { sc.last_step = st; if (kick) sc.kick_at = now; }
    sc.puff *= fm_expf(-dt / 0.22f);
    if (active && RHYTHM_DEF[set.rhythm].s[0] == 0) sc.puff = fm_maxf(sc.puff, 0.12f + eng_level() * 0.8f);
    sc.spin += dt * 0.01f;
    /* size, sidechain squash, ripple, pirouette, shake */
    sc.R = (active ? 11.0f : 10.0f) * (set.creature == 0 ? 1.0f + 0.35f * sc.puff : 1.0f);
    sc.sq = 1.0f; sc.sx = 1.0f;
    if (set.drums && set.side && active) {
        uint32_t age = now - sc.kick_at;
        if (age < 300u) {
            float k = 0.2f * fm_expf(-(float)age / 90.0f);
            sc.sq = 1.0f - k; sc.sx = 1.0f + k * 0.7f;
        }
    }
    sc.ripple_a = 0.0f;
    if (set.drums && set.dkick && active && now - sc.kick_at < 420u) {
        float k = (float)(now - sc.kick_at) / 420.0f;
        sc.ripple_r = sc.R + 3.0f + k * 32.0f;
        sc.ripple_a = 1.0f - k;
    }
    sc.rot = 0.0f;
    if (now - sc.rot_at < 700u && sc.rot_at) {
        float k = (float)(now - sc.rot_at) / 700.0f;
        sc.rot = k * k * (3.0f - 2.0f * k);
    }
    sc.rc = fm_sin_turns(sc.rot + 0.25f);
    sc.rs = fm_sin_turns(sc.rot);
    /* bowl, light, bands, eased */
    sc.zoom += ((ui.bowl ? 1.0f : 0.0f) - sc.zoom) * fm_minf(1.0f, dt * 4.0f);
    sc.dark += ((ui.dark ? 1.0f : 0.0f) - sc.dark) * fm_minf(1.0f, dt * 5.0f);
    /* the title: at the start (BLOP, BY OBSIMO a second later, then the main page), and while HOME is held */
    {
        uint32_t since;
        float tt;
        if (!sc.started) { sc.started = 1; sc.start_at = now; }     /* the intro counts from the first frame */
        since = now - sc.start_at;
        tt = ui.help ? 1.0f : since < 3000u ? 1.0f : since < 3500u ? 1.0f - (float)(since - 3000u) / 500.0f : 0.0f;
        sc.title_ms = ui.help ? (float)(now - ui.home_at - 600u) : (float)since;
        sc.title += (tt - sc.title) * fm_minf(1.0f, dt * 8.0f);
        if (tt >= 1.0f && since < 3000u) sc.title = 1.0f;
    }
    sc.bands += ((ui.clean || sc.title > 0.05f ? 0.0f : 1.0f) - sc.bands) * fm_minf(1.0f, dt * 6.0f);
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
    /* the eyes: shut when it sleeps (nothing plays, no food) or blinks, hearts when it eats */
    if (now > sc.blink_at) sc.blink_at = now + 2500u + (uint32_t)(srnd() * 4000.0f);
    if (sc.eat_at && now - sc.eat_at < 800u) sc.eye_state = EYE_HEART;
    else if ((!active && food < 0) || (sc.blink_at > now && sc.blink_at - now < 120u)) sc.eye_state = EYE_SHUT;
    else sc.eye_state = EYE_OPEN;
    if (!active && food < 0) { if (!sc.sleep_at) sc.sleep_at = now; }     /* asleep since */
    else sc.sleep_at = 0;
    sc.eye_r = 1.35f;
    sc.ecol[0] = DECOR_EYE[set.decor][0]; sc.ecol[1] = DECOR_EYE[set.decor][1]; sc.ecol[2] = DECOR_EYE[set.decor][2];
    sc.bright = 0.5f + 0.5f * (float)set.cutoff * 0.01f;
    sc.rim = (set.decor == 4 ? 0.45f : set.decor == 6 ? 0.3f : 0.0f) * (1.0f - sc.dark);
    eye_places();
}

/* ---- the info bands, crisp over the picture */
static uint16_t page_col(void) { const uint8_t *c = PAGE_RGB[set.page]; return RGB(c[0], c[1], c[2]); }

static void draw_top_text(void)
{
    const char *chord = active ? chord_txt : (chord_txt[0] ? chord_txt : "BLOP");
    uint32_t now = plat_ms();
    if (now < ui.msg_until) {
        int32_t x = cv_text(8, 0, &FONT_XS, ui.msg[0], RGB(200, 196, 210));
        cv_text(x + 6, 0, &FONT_XS, ui.msg[1], page_col());
    } else if (sc.sleep_at && now - sc.sleep_at > 15000u) {
        /* the credit, while it sleeps */
        static const char *const CREDIT[3] = {"BLOP BY OBSIMO", "GO STREAM MY MUSIC ;)", "ZZZ..."};
        int k = (int)((now / 3000u) % 3u);
        cv_text(8, 0, &FONT_XS, CREDIT[k], k == 2 ? RGB(170, 166, 180) : RGB(255, 214, 110));
    } else {
        cv_text(8, 0, &FONT_XS, SOUND_NAME[set.sound], page_col());
        cv_text(232 - text_w(&FONT_XS, GRID_DEF[set.grid].name), 0, &FONT_XS, GRID_DEF[set.grid].name, RGB(200, 196, 210));
    }
    cv_text(8, 9, &FONT_L, chord, active ? RGB(250, 248, 252) : RGB(170, 166, 180));
    cv_text(232 - text_w(&FONT_S, NOTE[set.key]), 19, &FONT_S, NOTE[set.key], RGB(200, 196, 210));
}

static void draw_bottom_text(void)       /* in the second canvas: y 0 = screen row 120 */
{
    int k;
    int32_t y0 = 189 - 120, tw = text_w(&FONT_T, PAGE_NAME[set.page]) + 10;
    uint16_t pc = page_col();
    char t[32];
    cv_rect(6, y0 + 1, tw, 13, pc);
    cv_text(11, y0 + 1, &FONT_T, PAGE_NAME[set.page], RGB(0, 0, 0));
    t[0] = 0;
    text_cat(t, set.rhythm ? RHYTHM_NAME[set.rhythm] : "RHYTHM OFF");
    if (set.drums) text_cat(t, " + DRUMS");
    cv_text(232 - text_w(&FONT_T, t), y0 + 1, &FONT_T, t, RGB(170, 166, 180));
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


/* the first screen at boot (the platform's splash, while the FM-1 starts): the intro's first picture,
 * BLOP in its lights over the dark water, so the start runs on without a flash */
void app_splash(void)
{
    scene_init();
    sc.title = 1.0f; sc.title_ms = 0.0f;
    sc.R = 10.0f; sc.sq = sc.sx = 1.0f; sc.rc = 1.0f; sc.rs = 0.0f; sc.bright = 0.86f;
    sc.eye_state = EYE_SHUT; sc.eye_r = 1.35f;
    sc.ecol[0] = DECOR_EYE[0][0]; sc.ecol[1] = DECOR_EYE[0][1]; sc.ecol[2] = DECOR_EYE[0][2];
    eye_places();
    cv_begin(240, 120, 0);
    draw_cells(0);
    cv_blit(0, 0);
    cv_begin(240, 120, 0);
    draw_cells(40);
    cv_blit(0, 120);
}

void app_ui_init(void)
{
    int i;
    for (i = 0; i < 5; i++) vis[i].to = vis[i].from = i == 4 ? 36.0f : 60.0f + (float)i * 4.0f;
    scene_init();
}

void app_frame(void)
{
    uint32_t now = plat_ms();
    input();
    if (now - ui.frame_at >= 40u) {
        float dt = ui.frame_at ? (float)(now - ui.frame_at) * 0.001f : 0.04f;
        if (dt > 0.1f) dt = 0.1f;
        ui.frame_at = now;
        scene_step(now, dt);
        cv_begin(240, 120, 0);
        draw_cells(0);
        if (sc.title < 0.5f && !ui.clean) draw_top_text();
        cv_blit(0, 0);
        cv_begin(240, 120, 0);
        draw_cells(40);
        if (sc.title < 0.5f && !ui.clean) draw_bottom_text();
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
    else if (!strcmp(what, "dirty")) *got = set_dirty;
    else if (!strcmp(what, "bpm")) *got = set.bpm;
    else if (!strcmp(what, "syncbpm")) *got = (int)(ck_bpm + 0.5f);
    else if (!strcmp(what, "side")) *got = set.side;
    else if (!strcmp(what, "page")) *got = set.page;
    else if (!strcmp(what, "drums")) *got = set.drums;
    else if (!strcmp(what, "help")) *got = ui.help;
    else if (!strcmp(what, "bowl")) *got = ui.bowl;
    else if (!strcmp(what, "dark")) *got = ui.dark;
    else if (!strcmp(what, "clean")) *got = ui.clean;
    else if (!strcmp(what, "cutoff")) *got = set.cutoff;
    else if (!strcmp(what, "delay")) *got = set.delay;
    else if (!strcmp(what, "bsound")) *got = set.bsound;
    else if (!strcmp(what, "creature")) *got = set.creature;
    else if (!strcmp(what, "reverb")) *got = set.reverb;
    else return 0;
    return 1;
}
#endif
