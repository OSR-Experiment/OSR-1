/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Obsimo (OSR Records) */
/* OSR-1 app: input, chord grids, the screen, settings. Part of the unity build (device/unity.c,
 * host/plat_host.c), after the platform: gfx.c and announce.c are in scope. The sound is dsp/engine.c.
 *
 * Controls: one action per control, no shift layer
 *   all 27 keys     chords of the grid (same note name, same chord; C5 = the grid's 8th chord),
 *                   white keys safe, black keys spices; below C4 an octave down, above C5 an octave up
 *   SELECT grid   ALGORITHM rhythm (TENU = no rhythm)   PRESET sound (all 32, family shown)
 *   K1 cutoff   K2 glide   K3 reverb   K4 BPM            (fast turns move further)
 *   top row:     FX kick   SCL sidechain   ENV variation (hold)   LFO bass   EDIT voicing   GLO richness
 *   bottom row:  HOME help   SAVE delay   ARP key -   SEQ key +   PLAY latch   REC free
 *   OCT- / OCT+ octave
 *   Settings save themselves a few seconds after the last change. */
#include "fastmath.h"
#include "engine.h"
#include "data.h"

/* ------------------------------------------------------------------ settings --- */
#define SET_MAGIC 0x4F535232u            /* "OSR2" (v2 layout) */
typedef struct {
    uint32_t magic;
    uint8_t grid, key, rich, voicing;
    int8_t octave;
    uint8_t sound, rhythm, rhythm_on;   /* rhythm_on: unused (TENU is the no-rhythm setting), kept for the layout */
    uint8_t kick, side, bass_on, latch;
    uint8_t glide, cutoff, reso, reverb, delay, bpm;   /* knobs: 0..100 (glide in 1/100 of 1.5 s, bpm 70..160) */
    uint8_t delay_on;
    uint8_t rsv[1];
} settings_t;
static settings_t set;
static uint8_t set_dirty;
static uint32_t dirty_at;             /* when the settings last changed (auto-save)*/

static void settings_defaults(void)
{
    memset(&set, 0, sizeof set);
    set.magic = SET_MAGIC;
    set.rich = 0;
    set.bass_on = 1;
    set.glide = 17;                      /* 0.25 s */
    set.cutoff = 72;
    set.reso = 20;
    set.reverb = 45;
    set.delay = 30;
    set.delay_on = 0;
    set.bpm = 118;
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
    eng_params((float)set.bpm, cutoff_hz(), reso_q(), set.kick, set.side, set.bass_on);
}
static void send_fx(void) { eng_fx((float)set.reverb * 0.01f, set.delay_on ? (float)set.delay * 0.01f : 0.0f); }

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

/* ------------------------------------------------------------------ platform contract --- */
void app_init(void)
{
    int g;
    for (g = 0; g < NGRID; g++) {
        parse_chords(GRID_DEF[g].white, grid_w[g], 8);
        parse_chords(GRID_DEF[g].black, grid_b[g], 5);
    }
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC
        || set.grid >= NGRID || set.key > 11u || set.rich > 3u || set.voicing > 2u || set.octave < -2
        || set.octave > 2 || set.sound >= NFAMILY * PER_FAMILY || set.rhythm >= NRHYTHM || set.bpm < 70u
        || set.bpm > 160u || set.delay_on > 1u)
        settings_defaults();
    eng_init();
    send_sound();
    send_rhythm();
    send_params();
    send_fx();
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
    uint8_t redraw_top, redraw_bottom;
    uint32_t trail_at;
    int last_step;
    uint32_t step_at, kick_at;
} ui;

static uint8_t help_on, help_drawn;     /* HOME: the help screen */
static void ui_redraw_all(void) { ui.redraw_top = ui.redraw_bottom = 1; }

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
    ui.redraw_top = 1;
    if (ann_on()) {
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        text_cat(t, " ");
        text_cat(t, ui.msg[1]);
        ann_say(t, 0, ANN_NOW);
    }
}

static void say_pct(const char *what, uint8_t v)
{
    char t[12];
    int_text(v, t);
    text_cat(t, "%");
    app_say(what, t);
}

static void dirty(void) { set_dirty = 1; dirty_at = plat_ms(); ui.redraw_top = ui.redraw_bottom = 1; }

static int step_u8(uint8_t *v, int32_t d, int lo, int hi)
{
    int32_t n = clamp((int32_t)*v + d, lo, hi);
    if (n == *v) return 0;
    *v = (uint8_t)n;
    return 1;
}

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

static void encoder(int k, int32_t e)
{
    char t[24];
    switch (k) {
    case EN_SELECT:
        wrap_u8(&set.grid, e, NGRID); set.rich = GRID_DEF[set.grid].rich; app_say("GRILLE", GRID_DEF[set.grid].name);
        rechord();
        break;
    case EN_ALGO: wrap_u8(&set.rhythm, e, NRHYTHM); app_say("RYTHME", RHYTHM_NAME[set.rhythm]); send_rhythm(); break;
    case EN_PRESET:
        wrap_u8(&set.sound, e, NFAMILY * PER_FAMILY);
        app_say(FAMILY_NAME[set.sound / PER_FAMILY], SOUND_NAME[set.sound]);
        send_sound();
        break;
    case EN_K1:
        step_u8(&set.cutoff, accel(k, e) * 3, 0, 100);
        int_text((int32_t)cutoff_hz(), t); text_cat(t, " HZ"); app_say("CUTOFF", t);
        send_params();
        break;
    case EN_K2:
        step_u8(&set.glide, accel(k, e) * 3, 0, 100);
        if (set.glide < 1u) app_say("GLIDE", "OFF");
        else { int_text((int32_t)(set.glide * 15u), t); text_cat(t, " MS"); app_say("GLIDE", t); }
        break;
    case EN_K3: step_u8(&set.reverb, accel(k, e) * 3, 0, 100); say_pct("REVERB", set.reverb); send_fx(); break;
    case EN_K4: step_u8(&set.bpm, accel(k, e), 70, 160); int_text(set.bpm, t); app_say("BPM", t); send_params(); break;
    default: break;
    }
    dirty();
}


static void set_variation(int on)
{
    if (variation == on) return;
    variation = (uint8_t)on;
    rechord();
    ui.redraw_top = 1;
}

static void button(int k)
{
    char t[8];
    switch (k) {
    case B_HOME: help_on ^= 1u; help_drawn = 0; if (!help_on) ui_redraw_all(); return;
    case B_FX: set.kick ^= 1u; send_params(); app_say("KICK", set.kick ? "ON" : "OFF"); break;
    case B_SEL: set.side ^= 1u; send_params(); app_say("SIDECHAIN", set.side ? "ON" : "OFF"); break;
    case B_LFO: set.bass_on ^= 1u; send_params(); app_say("BASSE", set.bass_on ? "ON" : "OFF"); break;
    case B_SAVE: set.delay_on ^= 1u; send_fx(); app_say("DELAY", set.delay_on ? "ON" : "OFF"); break;
    case B_GLO: wrap_u8(&set.rich, 1, 4); app_say("RICHESSE", RICH_NAME[set.rich]); rechord(); break;
    case B_EDIT: wrap_u8(&set.voicing, 1, 3); app_say("VOICING", VOICING_NAME[set.voicing]); rechord(); break;
    case B_ARP:
    case B_SEQ:
        wrap_u8(&set.key, k == B_SEQ ? 1 : -1, 12);
        app_say("TONALITE", NOTE[set.key]);
        rechord();
        break;
    case B_PLAY:
        set.latch ^= 1u;
        if (!set.latch && held_idx < 0 && active) stop();
        app_say("LATCH", set.latch ? "ON" : "OFF");
        break;
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
    uint32_t keys = plat_keys(), btn = plat_buttons(), k, lit_keys = 0, lit_btn = 0;
    int32_t e;
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(ui.keys & bit)) { held_idx = (int)k; play((int)k); ui.redraw_top = 1; }
        else if (!(keys & bit) && (ui.keys & bit) && held_idx == (int)k) {
            held_idx = -1;
            if (!set.latch) stop();
            ui.redraw_top = 1;
        }
    }
    set_variation((btn >> B_ENV) & 1u);
    for (k = 0; k < NB; k++) {
        uint32_t bit = 1u << k;
        if ((btn & bit) && !(ui.btn & bit)) button((int)k);
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
    /* lights: the keys glow, the one playing is lit; the toggles lit when on */
    if (cur_idx >= 0) lit_keys |= 1u << (uint32_t)cur_idx;
    if (set.kick) lit_btn |= 1u << B_FX;
    if (set.side) lit_btn |= 1u << B_SEL;
    if (set.bass_on) lit_btn |= 1u << B_LFO;
    if (help_on) lit_btn |= 1u << B_HOME;
    if (set.delay_on) lit_btn |= 1u << B_SAVE;
    if (set.latch) lit_btn |= LED_PLAY_GREEN;
    if (variation) lit_btn |= 1u << B_ENV;
    plat_lights(lit_btn, lit_keys, 0, (1u << NKEYS) - 1u, 1);
}

void app_input_only(void) { input(); }

/* ------------------------------------------------------------------ screen (240 x 240) --- */
/* An aquarium. A pufferfish swims to the chord (left/right = the key, up/down = the pitch, gliding
 * with GLIDE), puffs up and bristles on every hit of the rhythm, flinches with the kick (sidechain:
 * it is squashed), flaps its tail to the BPM. CUTOFF clears or clouds the water, REVERB fills it with
 * bubbles, DELAY leaves echoes of the fish behind it, VARIATION makes it blush.
 *   0..48 chord and sound   48..192 the aquarium (one canvas, redrawn at ~30 fps)   192..240 rhythm */
#define TOP_H 48
#define AQ_Y 48
#define AQ_H 144
#define BOT_Y 192
#define FLOOR_Y 128                      /* sand, in aquarium rows */
#define COL_TOP RGB(5, 12, 24)
#define COL_INK RGB(236, 244, 248)
#define COL_MUTED RGB(122, 156, 176)
#define COL_CORAL RGB(255, 140, 96)
#define COL_AQUA RGB(110, 220, 235)
#define COL_SAND_BG RGB(22, 19, 16)
#define COL_SAND_TXT RGB(196, 178, 148)

static uint16_t rgbf(float r, float g, float b)
{
    int32_t R = clamp((int32_t)r, 0, 255), G = clamp((int32_t)g, 0, 255), B = clamp((int32_t)b, 0, 255);
    return RGB(R, G, B);
}

static uint32_t rnd_state = 0x1234567u;
static float rnd(void)                   /* 0..1 */
{
    rnd_state = rnd_state * 1664525u + 1013904223u;
    return (float)(rnd_state >> 8) * (1.0f / 16777216.0f);
}

/* filled ellipse, row by row */
static void disc(float cx, float cy, float rx, float ry, uint16_t c)
{
    int32_t y, y0 = (int32_t)(cy - ry), y1 = (int32_t)(cy + ry);
    if (rx < 0.5f || ry < 0.5f) return;
    for (y = y0; y <= y1; y++) {
        float d = ((float)y - cy) / ry, hw;
        if (d < -1.0f || d > 1.0f) continue;
        hw = rx * fm_sqrtf(1.0f - d * d);
        cv_rect((int32_t)(cx - hw + 0.5f), y, (int32_t)(2.0f * hw + 1.0f), 1, c);
    }
}

static void aq_ring(float cx, float cy, float r, uint16_t c)
{
    int32_t x = (int32_t)r, y = 0, err = 1 - x, X = (int32_t)cx, Y = (int32_t)cy, guard = 400;
    while (x >= y && guard--) {
        cv_pset(X + x, Y + y, c); cv_pset(X + y, Y + x, c); cv_pset(X - y, Y + x, c); cv_pset(X - x, Y + y, c);
        cv_pset(X - x, Y - y, c); cv_pset(X - y, Y - x, c); cv_pset(X + y, Y - x, c); cv_pset(X + x, Y - y, c);
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}

/* ---- the fish's state */
static struct {
    float x, y;                          /* where it is */
    float x_from, x_to; uint32_t x_t0, x_dur;
    int dir;                             /* 1 faces right */
    float infl;                          /* 0 flat .. 1 fully puffed */
    float phase;                         /* tail, in turns */
    float sway;                          /* seaweed, in turns */
    float hx[16], hy[16], hr[16];        /* recent positions, for the delay echoes */
    int hpos;
} fish;

#define NBUB 28
static struct { float x, y, r, vy, ph; uint8_t on; } bub[NBUB];
static float bub_acc;
static uint32_t aq_at;

static void bubble_add(float x, float y, float r)
{
    int i;
    for (i = 0; i < NBUB; i++)
        if (!bub[i].on) {
            bub[i].on = 1; bub[i].x = x; bub[i].y = y; bub[i].r = r;
            bub[i].vy = 14.0f + rnd() * 16.0f + r * 3.0f; bub[i].ph = rnd();
            return;
        }
}

static void fish_target(int key_idx, uint32_t now)
{
    float tx = 58.0f + (float)key_idx * (124.0f / 26.0f);
    float g = glide_s();
    fish.x_from = fish.x;
    fish.x_to = tx;
    fish.x_t0 = now;
    fish.x_dur = (uint32_t)(g * 1000.0f) + 140u;
    if (tx > fish.x + 1.0f) fish.dir = 1;
    else if (tx < fish.x - 1.0f) fish.dir = -1;
}

static void step_fish(uint32_t now, float dt)
{
    float k, lvl = eng_level(), pitch = 0.0f;
    int i;
    /* left / right: eased towards the key, in the glide time */
    k = fish.x_dur ? (float)(now - fish.x_t0) / (float)fish.x_dur : 1.0f;
    if (k > 1.0f) k = 1.0f;
    k = k * k * (3.0f - 2.0f * k);
    fish.x = fish.x_from + (fish.x_to - fish.x_from) * k;
    /* up / down: the chord's pitch, as the engine glides it */
    for (i = 0; i < 4; i++) pitch += vis_pitch(i, now);
    pitch *= 0.25f;
    {
        float ty = active ? 100.0f - (pitch - 52.0f) * (56.0f / 30.0f) : 92.0f;
        ty = fm_clampf(ty, 46.0f, 98.0f);
        fish.y += (ty - fish.y) * fm_minf(1.0f, dt * (active ? 9.0f : 1.5f));
    }
    /* puffing: hits push it up (app_frame), it lets the air out; held chords breathe with the level */
    fish.infl *= fm_expf(-dt / 0.22f);
    if (active && RHYTHM_DEF[set.rhythm].s[0] == 0) fish.infl = fm_maxf(fish.infl, 0.15f + lvl * 0.9f);
    fish.phase += dt * (float)set.bpm / 60.0f * (active ? 2.0f : 0.6f);
    fish.sway += dt * (float)set.bpm / 60.0f * 0.25f;
    if (fish.phase > 1000.0f) fish.phase -= 1000.0f;
    if (fish.sway > 1000.0f) fish.sway -= 1000.0f;
    /* bubbles: REVERB is how many and how big */
    bub_acc += dt * (0.4f + (float)set.reverb * 0.075f);
    while (bub_acc >= 1.0f) {
        bub_acc -= 1.0f;
        bubble_add(8.0f + rnd() * 224.0f, (float)FLOOR_Y, 1.0f + rnd() * (1.0f + (float)set.reverb * 0.035f));
    }
    for (i = 0; i < NBUB; i++)
        if (bub[i].on) {
            bub[i].y -= bub[i].vy * dt;
            bub[i].ph += dt * 1.3f;
            if (bub[i].y < -6.0f) bub[i].on = 0;
        }
}

/* the pufferfish at its place: R the radius (puffed), sq the sidechain squash (1 = none) */
static void draw_fish(float cx, float cy, float R, float sq, int dir, float infl)
{
    float rx = R * (1.0f + (1.0f - sq) * 0.6f), ry = R * sq, d = (float)dir, i_f;
    float flap = fm_sin_turns(fish.phase) * (3.0f + R * 0.08f);
    uint16_t body = variation ? RGB(246, 178, 150) : RGB(236, 198, 104), belly = RGB(252, 240, 206);
    uint16_t dark = RGB(150, 104, 48), spike = RGB(196, 150, 78);
    int32_t y, n;
    if (!active) { body = RGB(176, 150, 92); belly = RGB(196, 186, 160); spike = RGB(140, 112, 64); }
    /* tail: a fan behind */
    {
        float bx = cx - d * rx * 0.8f, L = 10.0f + R * 0.35f;
        for (n = 0; n < (int32_t)L; n++) {
            float t = (float)n / L, hw = 2.0f + t * (4.0f + R * 0.18f);
            cv_rect((int32_t)(bx - d * (float)n), (int32_t)(cy + flap * t - hw), 1, (int32_t)(2.0f * hw), dark);
        }
    }
    /* spikes: short bumps when it is flat, long when puffed */
    for (n = 0; n < 18; n++) {
        float a = (float)n / 18.0f, ca = fm_sin_turns(a + 0.25f), sa = fm_sin_turns(a);
        float L = 1.5f + infl * (5.0f + R * 0.3f);
        int32_t x0 = (int32_t)(cx + ca * rx * 0.9f), y0 = (int32_t)(cy + sa * ry * 0.9f);
        int32_t x1 = (int32_t)(cx + ca * (rx + L)), y1 = (int32_t)(cy + sa * (ry + L));
        cv_line(x0, y0, x1, y1, spike);
        cv_line(x0 + 1, y0, x1 + 1, y1, spike);
    }
    /* body, the belly lighter, row by row */
    for (y = (int32_t)(cy - ry); y <= (int32_t)(cy + ry); y++) {
        float dy = ((float)y - cy) / ry, hw;
        if (dy < -1.0f || dy > 1.0f) continue;
        hw = rx * fm_sqrtf(1.0f - dy * dy);
        cv_rect((int32_t)(cx - hw + 0.5f), y, (int32_t)(2.0f * hw + 1.0f), 1, dy > 0.28f ? belly : body);
    }
    /* spots on the back */
    for (n = 0; n < 5; n++) {
        static const float SX[5] = {-0.45f, -0.1f, 0.2f, -0.6f, -0.25f}, SY[5] = {-0.55f, -0.7f, -0.62f, -0.15f, -0.25f};
        disc(cx + d * SX[n] * rx, cy + SY[n] * ry, R * 0.09f + 0.6f, R * 0.09f + 0.6f, dark);
    }
    /* side fin, flapping */
    disc(cx - d * rx * 0.05f, cy + ry * 0.12f, 3.0f + R * 0.12f, 1.5f + fm_fabsf(flap) * 0.5f, dark);
    /* cheek (VARIATION: blushing) */
    if (variation) disc(cx + d * rx * 0.32f, cy + ry * 0.18f, R * 0.14f, R * 0.09f, RGB(255, 120, 140));
    /* eye: bigger when puffed (surprised); closed when nothing plays */
    {
        float ex = cx + d * rx * 0.46f, ey = cy - ry * 0.26f, er = R * (0.2f + infl * 0.07f);
        if (active) {
            disc(ex, ey, er, er, RGB(250, 252, 255));
            disc(ex + d * er * 0.3f, ey, er * 0.55f, er * 0.55f, RGB(16, 20, 30));
            cv_pset((int32_t)(ex + d * er * 0.1f), (int32_t)(ey - er * 0.35f), RGB(255, 255, 255));
        } else
            cv_rect((int32_t)(ex - er), (int32_t)ey, (int32_t)(2.0f * er), 2, dark);
    }
    /* mouth: a little "o" that opens with the air */
    i_f = 1.2f + infl * (1.0f + R * 0.06f);
    disc(cx + d * rx * 0.94f, cy + ry * 0.1f, i_f, i_f, RGB(150, 70, 60));
}

static void draw_aquarium(void)
{
    uint32_t now = plat_ms();
    float dt = aq_at ? (float)(now - aq_at) * 0.001f : 0.033f, clear = (float)set.cutoff * 0.01f, sq = 1.0f, R;
    int32_t y, i;
    aq_at = now;
    if (dt > 0.1f) dt = 0.1f;
    step_fish(now, dt);
    cv_begin(240, AQ_H, COL_TOP);
    /* water: CUTOFF clears it (turquoise, sun rays) or clouds it (dark, green, specks) */
    for (y = 0; y < FLOOR_Y + 6; y++) {
        float t = (float)y / (float)FLOOR_Y;
        float r = (8.0f + 30.0f * clear) * (1.0f - t) + (3.0f + 8.0f * clear) * t;
        float g = (34.0f + 140.0f * clear) * (1.0f - t) + (14.0f + 50.0f * clear) * t;
        float b = (40.0f + 170.0f * clear) * (1.0f - t) + (22.0f + 92.0f * clear) * t;
        cv_rect(0, y, 240, 1, rgbf(r, g, b));
        if (clear > 0.3f) {              /* sun rays, slanted, drifting */
            float a = (clear - 0.3f) * 0.5f * (1.0f - t);
            uint16_t c = rgbf(r + (255.0f - r) * a, g + (255.0f - g) * a, b + (255.0f - b) * a);
            for (i = 0; i < 3; i++) {
                float x0 = 30.0f + (float)i * 78.0f + fm_sin_turns(fish.sway * 0.1f + (float)i * 0.3f) * 14.0f + (float)y * 0.45f;
                cv_rect((int32_t)x0, y, 12 + i * 4, 1, c);
            }
        }
    }
    /* the surface shimmering */
    for (i = 0; i < 240; i += 6) {
        int32_t w = 2 + (int32_t)(2.0f + 2.0f * fm_sin_turns(fish.sway + (float)i * 0.013f));
        cv_rect(i, 0, w, 1, rgbf(120.0f + 120.0f * clear, 200.0f + 50.0f * clear, 230.0f));
    }
    /* specks in murky water */
    for (i = 0; i < (int32_t)((1.0f - clear) * 40.0f); i++) {
        float x = (float)((i * 97 + 13) % 240) + fm_sin_turns(fish.sway * 0.2f + (float)i * 0.17f) * 6.0f;
        float yy = (float)((i * 53 + 7) % FLOOR_Y);
        cv_pset((int32_t)x, (int32_t)yy, RGB(70, 96, 70));
    }
    /* sand, with a wavy top, and seaweed swaying to the tempo */
    for (i = 0; i < 240; i++) {
        int32_t top = FLOOR_Y + (int32_t)(2.0f * fm_sin_turns((float)i * 0.011f));
        cv_rect(i, top, 1, AQ_H - top, ((i * 7) % 11) == 0 ? RGB(150, 128, 92) : RGB(118, 100, 72));
    }
    for (i = 0; i < 4; i++) {
        static const int32_t WX[4] = {14, 28, 206, 224}, WH[4] = {62, 44, 52, 70};
        int32_t s, px = WX[i], py = FLOOR_Y + 2;
        for (s = 1; s <= 8; s++) {
            float t = (float)s / 8.0f;
            int32_t x = WX[i] + (int32_t)(fm_sin_turns(fish.sway + (float)i * 0.23f - t * 0.35f) * 7.0f * t);
            int32_t yy = FLOOR_Y + 2 - (int32_t)((float)WH[i] * t);
            cv_line(px, py, x, yy, RGB(46, 140, 92));
            cv_line(px + 1, py, x + 1, yy, RGB(34, 112, 74));
            px = x; py = yy;
        }
    }
    /* the fish: size from the air in it, squashed by the sidechain when the kick hits */
    R = (active ? 19.0f : 16.0f) * (1.0f + 0.5f * fish.infl);
    if (set.kick && set.side && active) {
        uint32_t age = now - ui.kick_at;
        if (age < 300u) sq = 1.0f - 0.22f * fm_expf(-(float)age / 90.0f);
    }
    fish.hx[fish.hpos] = fish.x; fish.hy[fish.hpos] = fish.y; fish.hr[fish.hpos] = R;
    if (set.delay_on && active)          /* DELAY: two echoes of the fish, fading */
        for (i = 2; i >= 1; i--) {
            int h = (fish.hpos - i * 6 + 16 * 4) % 16;
            uint16_t c = i == 1 ? RGB(150, 200, 220) : RGB(90, 140, 170);
            aq_ring(fish.hx[h], fish.hy[h], fish.hr[h], c);
        }
    fish.hpos = (fish.hpos + 1) % 16;
    /* the kick: a ripple out of the fish */
    if (set.kick && active && now - ui.kick_at < 360u) {
        float k = (float)(now - ui.kick_at) / 360.0f;
        aq_ring(fish.x, fish.y, R + 6.0f + k * 46.0f, rgbf(180.0f + 60.0f * (1.0f - k), 230.0f, 255.0f));
    }
    draw_fish(fish.x, fish.y, R, sq, fish.dir, fish.infl);
    /* bubbles in front */
    for (i = 0; i < NBUB; i++)
        if (bub[i].on) {
            float x = bub[i].x + fm_sin_turns(bub[i].ph) * 2.0f;
            aq_ring(x, bub[i].y, bub[i].r, RGB(200, 236, 250));
            cv_pset((int32_t)(x - bub[i].r * 0.4f), (int32_t)(bub[i].y - bub[i].r * 0.4f), RGB(255, 255, 255));
        }
    cv_blit(0, AQ_Y);
}

static void draw_top(void)               /* 0..48: the sound and grid (or a message), the chord */
{
    char t[24];
    uint32_t now = plat_ms();
    cv_begin(240, TOP_H, COL_TOP);
    if (now < ui.msg_until) {
        int32_t x = cv_text(10, 2, &FONT_XS, ui.msg[0], COL_MUTED);
        cv_text(x + 6, 2, &FONT_XS, ui.msg[1], COL_CORAL);
    } else {
        cv_text(10, 2, &FONT_XS, SOUND_NAME[set.sound], COL_AQUA);
        cv_text(230 - text_w(&FONT_XS, GRID_DEF[set.grid].name), 2, &FONT_XS, GRID_DEF[set.grid].name, COL_MUTED);
    }
    cv_text(10, 15, &FONT_L, active ? chord_txt : (chord_txt[0] ? chord_txt : "OSR-1"), active ? COL_INK : COL_MUTED);
    t[0] = 0;
    text_cat(t, NOTE[set.key]);
    if (variation) text_cat(t, "  VAR");
    cv_text(230 - text_w(&FONT_XS, t), 28, &FONT_XS, t, variation ? RGB(255, 140, 160) : COL_MUTED);
    cv_blit(0, 0);
}

static void draw_bottom(void)            /* 192..240: the rhythm's steps as pebbles, rhythm, BPM, kick / sidechain */
{
    char t[16];
    const char *s = RHYTHM_DEF[set.rhythm].s;
    uint32_t now = plat_ms();
    int i, hold = s[0] == 0;
    cv_begin(240, 48, COL_SAND_BG);
    for (i = 0; i < 16; i++) {
        int32_t x = 13 + i * 14;
        int hitv = !hold && (s[i] == 'X' || s[i] == 'x'), tie = !hold && s[i] == '-';
        uint16_t c = hitv ? (s[i] == 'X' ? RGB(170, 150, 118) : RGB(120, 106, 84)) : (hold || tie ? RGB(64, 56, 46) : RGB(40, 35, 29));
        float h = hold ? 2.5f : hitv ? (s[i] == 'X' ? 4.5f : 3.5f) : 1.5f;
        if (ui.last_step == i && now - ui.step_at < 160u && (hitv || hold)) c = COL_CORAL;
        disc((float)x + 5.0f, 12.0f, 5.0f, h, c);
    }
    cv_text(10, 26, &FONT_XS, RHYTHM_NAME[set.rhythm], COL_SAND_TXT);
    int_text(set.bpm, t);
    text_cat(t, " BPM");
    cv_text(124 - text_w(&FONT_XS, t) / 2, 26, &FONT_XS, t, RGB(140, 126, 104));
    {
        int32_t x = 230 - text_w(&FONT_XS, "SC");
        cv_text(x, 26, &FONT_XS, "SC", set.side ? COL_AQUA : RGB(84, 76, 64));
        x -= text_w(&FONT_XS, "KICK") + 8;
        cv_text(x, 26, &FONT_XS, "KICK", set.kick ? (now - ui.kick_at < 120u ? COL_CORAL : COL_INK) : RGB(84, 76, 64));
    }
    cv_blit(0, BOT_Y);
}

/* ---- HOME: the help screen, laid out like the panel */
static void help_box(int32_t x, int32_t y, int32_t w, int32_t h, const char *dev, const char *fn, uint16_t fc,
                     uint16_t edge, const felucca_font_t *ff)
{
    cv_rect(x, y, w, h, edge);
    cv_rect(x + 1, y + 1, w - 2, h - 2, RGB(14, 26, 40));
    cv_text(x + (w - text_w(&FONT_T, dev)) / 2, y + 3, &FONT_T, dev, COL_MUTED);
    cv_text(x + (w - text_w(ff, fn)) / 2, y + h - ff->h - 3, ff, fn, fc);
}

static void draw_help(void)
{
    static const char *const ENC_DEV[3] = {"PRESET", "ALGO", "SELECT"}, *const ENC_FN[3] = {"SON", "RYTHME", "GRILLE"};
    static const char *const K_FN[4] = {"CUTOFF", "GLIDE", "REVERB", "BPM"}, *const K_DEV[4] = {"K1", "K2", "K3", "K4"};
    static const char *const B_DEV[12] = {"FX", "SCL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"};
    static const char *const B_FN[12] = {"KICK", "SIDE", "VAR", "BASSE", "VOIX", "RICHE", "AIDE", "DELAY", "TON-", "TON+", "LATCH", "-"};
    static const uint16_t B_COL[12] = {RGB(255, 140, 96), RGB(110, 220, 235), RGB(255, 140, 160), RGB(236, 244, 248),
                                       RGB(236, 244, 248), RGB(236, 244, 248), RGB(255, 214, 120), RGB(236, 244, 248),
                                       RGB(236, 244, 248), RGB(236, 244, 248), RGB(120, 230, 150), RGB(84, 96, 110)};
    int32_t i;
    /* rows 0..119: title, the 3 big knobs, the 4 small ones */
    cv_begin(240, 120, COL_TOP);
    cv_text(6, 2, &FONT_XS, "AIDE", RGB(255, 214, 120));
    cv_text(234 - text_w(&FONT_XS, "HOME = FERMER"), 2, &FONT_XS, "HOME = FERMER", COL_MUTED);
    for (i = 0; i < 3; i++)
        help_box(6 + i * 77, 22, 74, 44, ENC_DEV[i], ENC_FN[i], COL_AQUA, RGB(40, 70, 90), &FONT_XS);
    for (i = 0; i < 4; i++)
        help_box(6 + i * 57, 72, 54, 44, K_DEV[i], K_FN[i], COL_INK, RGB(40, 70, 90), &FONT_T);
    cv_blit(0, 0);
    /* rows 120..239: the 12 buttons, two rows of six, then the keys */
    cv_begin(240, 120, COL_TOP);
    for (i = 0; i < 12; i++) {
        uint16_t edge = i < 2 ? RGB(110, 80, 70) : i == 6 ? RGB(110, 96, 50) : RGB(40, 70, 90);
        help_box(4 + (i % 6) * 39, 2 + (i / 6) * 46, 37, 42, B_DEV[i], B_FN[i], B_COL[i], edge, &FONT_T);
    }
    cv_text(120 - text_w(&FONT_T, "TOUCHES = ACCORDS   OCT- / OCT+ = OCTAVE") / 2, 100, &FONT_T,
            "TOUCHES = ACCORDS   OCT- / OCT+ = OCTAVE", COL_MUTED);
    cv_blit(0, 120);
}

void app_ui_init(void)
{
    int i, j;
    lcd_fill(0, 0, 240, 240, COL_TOP);
    for (i = 0; i < 5; i++) vis[i].to = vis[i].from = i == 4 ? 36.0f : 60.0f + (float)i * 4.0f;
    (void)j;
    fish.x = fish.x_from = fish.x_to = 120.0f;
    fish.y = 92.0f;
    fish.dir = 1;
    ui.last_step = -1;
    ui.redraw_top = ui.redraw_bottom = 1;
}

void app_frame(void)
{
    uint32_t now = plat_ms(), at;
    int kick, st;
    static char last_chord[16];
    static int last_idx = -1;
    input();
    if (cur_idx >= 0 && cur_idx != last_idx) fish_target(cur_idx, now);
    last_idx = cur_idx;
    st = eng_last_step(&at, &kick);
    if (st != ui.last_step) {
        ui.last_step = st;
        ui.step_at = now;
        if (kick) ui.kick_at = now;
        ui.redraw_bottom = 1;
        if (st >= 0 && active) {         /* a hit of the rhythm: the fish puffs up, a bubble from its mouth */
            const char *s = RHYTHM_DEF[set.rhythm].s;
            if (s[0] && (s[st] == 'X' || s[st] == 'x')) {
                fish.infl = fm_maxf(fish.infl, s[st] == 'X' ? 1.0f : 0.6f);
                if (s[st] == 'X') bubble_add(fish.x + (float)fish.dir * 24.0f, fish.y, 1.5f + rnd() * 1.5f);
            }
        }
    }
    if (ui.step_at && now - ui.step_at > 160u && now - ui.step_at < 200u) ui.redraw_bottom = 1;
    if (ui.msg_until && now >= ui.msg_until) { ui.msg_until = 0; ui.redraw_top = 1; }
    if (memcmp(last_chord, chord_txt, sizeof last_chord)) { memcpy(last_chord, chord_txt, sizeof last_chord); ui.redraw_top = 1; }
    if (help_on) {
        if (!help_drawn) { help_drawn = 1; draw_help(); }
        ann_tick(now);
        return;
    }
    if (ui.redraw_top) { ui.redraw_top = 0; draw_top(); }
    if (now - ui.trail_at >= 33u) { ui.trail_at = now; draw_aquarium(); }
    if (ui.redraw_bottom) { ui.redraw_bottom = 0; draw_bottom(); }
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
    else if (!strcmp(what, "kick")) *got = set.kick;
    else if (!strcmp(what, "variation")) *got = variation;
    else if (!strcmp(what, "dirty")) *got = set_dirty;
    else if (!strcmp(what, "bpm")) *got = set.bpm;
    else if (!strcmp(what, "side")) *got = set.side;
    else if (!strcmp(what, "help")) *got = help_on;
    else return 0;
    return 1;
}
#endif
