/* OSR-1 app: input, chord grids, the screen, settings. Part of the unity build (device/unity.c,
 * host/plat_host.c), after the platform: gfx.c and announce.c are in scope. The sound is dsp/engine.c.
 *
 * Controls
 *   keys C4..C5 (13)  the chords of the grid: white keys safe, black keys spices
 *   key B3 / ENV      VARIATION while held (sus version, bass on the fifth)
 *   PRESET            grid                      SEL + PRESET   key (transpose)
 *   ALGORITHM         rhythm                    SEL + ALGO     richness
 *   SELECT            sound                     SEL + SELECT   family
 *   K1 cutoff  K2 resonance  K3 reverb  K4 delay
 *   SEL + K1 glide    SEL + K2 voicing          SEL + K3 swing (reserved)   SEL + K4 BPM
 *   OCT- / OCT+       octave ("couleur")
 *   SEQ rhythm on/off   FX kick   LFO sidechain   ARP bass   PLAY latch   SAVE settings */
#include "fastmath.h"
#include "engine.h"
#include "data.h"

/* ------------------------------------------------------------------ settings --- */
#define SET_MAGIC 0x4F535231u            /* "OSR1" */
typedef struct {
    uint32_t magic;
    uint8_t grid, key, rich, voicing;
    int8_t octave;
    uint8_t sound, rhythm, rhythm_on;
    uint8_t kick, side, bass_on, latch;
    uint8_t glide, cutoff, reso, reverb, delay, bpm;   /* knobs: 0..100 (glide in 1/100 of 1.5 s, bpm 70..160) */
    uint8_t rsv[2];
} settings_t;
static settings_t set;
static uint8_t set_dirty;

static void settings_defaults(void)
{
    memset(&set, 0, sizeof set);
    set.magic = SET_MAGIC;
    set.rich = 0;
    set.rhythm_on = 1;
    set.bass_on = 1;
    set.glide = 17;                      /* 0.25 s */
    set.cutoff = 72;
    set.reso = 20;
    set.reverb = 45;
    set.delay = 25;
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

static chord_t build_chord(int idx)
{
    chord_t c;
    const chord_def_t *d = 0;
    int i;
    for (i = 0; i < 8; i++) if (WHITE_IDX[i] == idx) d = &grid_w[set.grid][i];
    for (i = 0; i < 5; i++) if (BLACK_IDX[i] == idx % 12) d = &grid_b[set.grid][i];
    c.root = (set.key + d->semi) % 12;
    c.n = quality_ivs(variation ? variation_of(d->q) : d->q, set.rich, c.iv);
    c.bass_off = variation ? 7 : 0;
    c.oct = idx == 12;
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
    int i, def = set.rhythm_on ? set.rhythm : 0;
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
static void send_fx(void) { eng_fx((float)set.reverb * 0.01f, (float)set.delay * 0.01f); }

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
        || set.bpm > 160u)
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

static void dirty(void) { set_dirty = 1; ui.redraw_top = ui.redraw_bottom = 1; }

static int step_u8(uint8_t *v, int32_t d, int lo, int hi)
{
    int32_t n = clamp((int32_t)*v + d, lo, hi);
    if (n == *v) return 0;
    *v = (uint8_t)n;
    return 1;
}

static void wrap_u8(uint8_t *v, int32_t d, int n) { *v = (uint8_t)(((int32_t)*v + d % n + n) % n); }

static void encoder(int k, int32_t e, int shift)
{
    char t[24];
    switch (k) {
    case EN_PRESET:
        if (shift) { wrap_u8(&set.key, e, 12); app_say("TONALITE", NOTE[set.key]); }
        else { wrap_u8(&set.grid, e, NGRID); set.rich = GRID_DEF[set.grid].rich; app_say("GRILLE", GRID_DEF[set.grid].name); }
        rechord();
        break;
    case EN_ALGO:
        if (shift) { step_u8(&set.rich, e, 0, 3); app_say("RICHESSE", RICH_NAME[set.rich]); rechord(); }
        else { wrap_u8(&set.rhythm, e, NRHYTHM); app_say("RYTHME", RHYTHM_NAME[set.rhythm]); send_rhythm(); }
        break;
    case EN_SELECT:
        if (shift) {
            uint8_t f = (uint8_t)(set.sound / PER_FAMILY);
            wrap_u8(&f, e, NFAMILY);
            set.sound = (uint8_t)(f * PER_FAMILY);
            app_say(FAMILY_NAME[f], SOUND_NAME[set.sound]);
        } else {
            wrap_u8(&set.sound, e, NFAMILY * PER_FAMILY);
            app_say("SON", SOUND_NAME[set.sound]);
        }
        send_sound();
        break;
    case EN_K1:
        if (shift) {
            step_u8(&set.glide, e, 0, 100);
            if (set.glide < 1u) app_say("GLIDE", "OFF");
            else { int_text((int32_t)(set.glide * 15u), t); text_cat(t, " MS"); app_say("GLIDE", t); }
        } else {
            step_u8(&set.cutoff, e, 0, 100);
            int_text((int32_t)cutoff_hz(), t); text_cat(t, " HZ"); app_say("CUTOFF", t);
            send_params();
        }
        break;
    case EN_K2:
        if (shift) { step_u8(&set.voicing, e, 0, 2); app_say("VOICING", VOICING_NAME[set.voicing]); rechord(); }
        else { step_u8(&set.reso, e, 0, 100); say_pct("RESONANCE", set.reso); send_params(); }
        break;
    case EN_K3:
        if (!shift) { step_u8(&set.reverb, e, 0, 100); say_pct("REVERB", set.reverb); send_fx(); }
        break;
    case EN_K4:
        if (shift) { step_u8(&set.bpm, e, 70, 160); int_text(set.bpm, t); app_say("BPM", t); send_params(); }
        else { step_u8(&set.delay, e, 0, 100); say_pct("DELAY", set.delay); send_fx(); }
        break;
    default: break;
    }
    dirty();
}

#define CHORD_KEY0 7                     /* C4 */
#define VAR_KEY 6                        /* B3 */

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
    case B_SEQ: set.rhythm_on ^= 1u; send_rhythm(); app_say("RYTHME", set.rhythm_on ? RHYTHM_NAME[set.rhythm] : "OFF"); break;
    case B_FX: set.kick ^= 1u; send_params(); app_say("KICK", set.kick ? "ON" : "OFF"); break;
    case B_LFO: set.side ^= 1u; send_params(); app_say("SIDECHAIN", set.side ? "ON" : "OFF"); break;
    case B_ARP: set.bass_on ^= 1u; send_params(); app_say("BASSE", set.bass_on ? "ON" : "OFF"); break;
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
    case B_SAVE:
        if (plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0) { set_dirty = 0; app_say("SAUVE", ""); }
        else app_say("SAUVEGARDE", "ECHEC");
        return;
    default: return;
    }
    dirty();
}

static void input(void)
{
    uint32_t keys = plat_keys(), btn = plat_buttons(), k, lit_keys = 0, dim_keys = 0, lit_btn = 0;
    int shift = (btn >> B_SEL) & 1u;
    int32_t e;
    for (k = 0; k < 13u; k++) {
        uint32_t bit = 1u << (CHORD_KEY0 + k);
        if ((keys & bit) && !(ui.keys & bit)) { held_idx = (int)k; play((int)k); ui.redraw_top = 1; }
        else if (!(keys & bit) && (ui.keys & bit) && held_idx == (int)k) {
            held_idx = -1;
            if (!set.latch) stop();
            ui.redraw_top = 1;
        }
    }
    set_variation(((keys >> VAR_KEY) & 1u) || ((btn >> B_ENV) & 1u));
    for (k = 0; k < NB; k++) {
        uint32_t bit = 1u << k;
        if ((btn & bit) && !(ui.btn & bit)) button((int)k);
    }
    for (k = 0; k < NE; k++)
        if ((e = plat_enc((int)k)) != 0)
            encoder((int)k, e, shift);
    ui.keys = keys;
    ui.btn = btn;
    /* lights: the chord keys glow, the one playing is lit; the toggles lit when on */
    for (k = 0; k < 13u; k++) dim_keys |= 1u << (CHORD_KEY0 + k);
    dim_keys |= 1u << VAR_KEY;
    if (cur_idx >= 0) lit_keys |= 1u << (CHORD_KEY0 + (uint32_t)cur_idx);
    if (variation) lit_keys |= 1u << VAR_KEY;
    if (set.rhythm_on) lit_btn |= 1u << B_SEQ;
    if (set.kick) lit_btn |= 1u << B_FX;
    if (set.side) lit_btn |= 1u << B_LFO;
    if (set.bass_on) lit_btn |= 1u << B_ARP;
    if (set.latch) lit_btn |= LED_PLAY_GREEN;
    if (set_dirty) lit_btn |= 1u << B_SAVE;
    if (shift) lit_btn |= 1u << B_SEL;
    if (variation) lit_btn |= 1u << B_ENV;
    plat_lights(lit_btn, lit_keys, 0, dim_keys, 1);
}

void app_input_only(void) { input(); }

/* ------------------------------------------------------------------ screen (240 x 240) --- */
#define COL_BG RGB(10, 7, 16)
#define COL_INK RGB(241, 232, 239)
#define COL_MUTED RGB(165, 151, 176)
#define COL_ORANGE RGB(255, 154, 92)
#define COL_DIMLINE RGB(42, 34, 54)
static const uint16_t VOICE_COL[5] = {RGB(255, 154, 92), RGB(255, 111, 156), RGB(185, 139, 255), RGB(127, 209, 255),
                                      RGB(107, 90, 126)};
#define TRAIL_N 120
static float trail[5][TRAIL_N];
static int trail_pos;

static void draw_top(void)               /* 0..72: sound, family, chord, grid */
{
    char t[40];
    uint32_t now = plat_ms();
    cv_begin(240, 72, COL_BG);
    cv_text(12, 6, &FONT_XS, SOUND_NAME[set.sound], COL_ORANGE);
    cv_text(228 - text_w(&FONT_XS, FAMILY_NAME[set.sound / PER_FAMILY]), 6, &FONT_XS, FAMILY_NAME[set.sound / PER_FAMILY],
            COL_MUTED);
    cv_text(12, 24, &FONT_L, active ? chord_txt : (chord_txt[0] ? chord_txt : "OSR-1"), active ? COL_INK : COL_MUTED);
    if (now < ui.msg_until) {
        int32_t x = cv_text(12, 56, &FONT_XS, ui.msg[0], COL_MUTED);
        cv_text(x + 6, 56, &FONT_XS, ui.msg[1], COL_ORANGE);
    } else {
        t[0] = 0;
        text_cat(t, GRID_DEF[set.grid].name);
        text_cat(t, " - ");
        text_cat(t, NOTE[set.key]);
        if (variation) text_cat(t, " - VAR");
        cv_text(12, 56, &FONT_XS, t, COL_MUTED);
    }
    cv_blit(0, 0);
}

static void draw_trails(void)            /* 72..176: the voices gliding */
{
    uint32_t now = plat_ms();
    int i, j;
    float lvl = eng_level();
    for (i = 0; i < 5; i++) trail[i][trail_pos] = vis_pitch(i, now);
    trail_pos = (trail_pos + 1) % TRAIL_N;
    cv_begin(240, 104, COL_BG);
    for (i = 4; i >= 0; i--) {
        uint16_t c = VOICE_COL[i];
        int32_t px = 0, py = 0;
        if (i == 4 && !set.bass_on) continue;
        if (!active) c = COL_DIMLINE;
        for (j = 0; j < TRAIL_N; j++) {
            float m = trail[i][(trail_pos + j) % TRAIL_N];
            int32_t x = 12 + j * 216 / (TRAIL_N - 1), y = 98 - (int32_t)((m - 32.0f) * 94.0f / 50.0f);   /* bass .. top voice */
            y = clamp(y, 2, 101);
            if (j) {
                cv_line(px, py, x, y, c);
                if (i < 4) cv_line(px, py + 1, x, y + 1, c);
            }
            px = x; py = y;
        }
        if (active && i < 4) {
            int32_t r = 2 + (int32_t)(lvl * 3.0f);
            cv_rect(px - r, py - r, 2 * r + 1, 2 * r + 1, c);
        }
    }
    cv_blit(0, 72);
}

static void draw_bottom(void)            /* 176..240: steps, rhythm, BPM, kick / sidechain */
{
    char t[16];
    const char *s = RHYTHM_DEF[set.rhythm_on ? set.rhythm : 0].s;
    uint32_t now = plat_ms();
    int i, hold = s[0] == 0;
    cv_begin(240, 64, COL_BG);
    for (i = 0; i < 16; i++) {
        int32_t x = 12 + i * 135 / 10, w = 11, h;
        int hitv = !hold && (s[i] == 'X' || s[i] == 'x'), tie = !hold && s[i] == '-';
        uint16_t c = hitv ? (s[i] == 'X' ? RGB(90, 74, 110) : RGB(60, 49, 73)) : (hold || tie ? RGB(42, 34, 54) : RGB(23, 18, 31));
        if (ui.last_step == i && now - ui.step_at < 160u && (hitv || hold)) c = COL_ORANGE;
        h = hold ? 6 : hitv ? 9 : 4;
        cv_rect(x, 14 - h / 2, w, h, c);
    }
    cv_text(12, 36, &FONT_XS, set.rhythm_on ? RHYTHM_NAME[set.rhythm] : "RYTHME OFF", COL_MUTED);
    int_text(set.bpm, t);
    text_cat(t, " BPM");
    cv_text(120 - text_w(&FONT_XS, t) / 2, 36, &FONT_XS, t, RGB(107, 90, 126));
    {
        int32_t x = 228 - text_w(&FONT_XS, "SC");
        cv_text(x, 36, &FONT_XS, "SC", set.side ? RGB(127, 209, 255) : RGB(90, 79, 102));
        x -= text_w(&FONT_XS, "KICK") + 8;
        cv_text(x, 36, &FONT_XS, "KICK", set.kick ? COL_INK : RGB(90, 79, 102));
        cv_rect(x - 10, 42, 6, 6, set.kick ? (now - ui.kick_at < 120u ? RGB(255, 111, 156) : RGB(128, 56, 78))
                                            : RGB(58, 48, 74));
    }
    cv_blit(0, 176);
}

void app_ui_init(void)
{
    int i, j;
    lcd_fill(0, 0, 240, 240, COL_BG);
    for (i = 0; i < 5; i++) {
        vis[i].to = vis[i].from = i == 4 ? 36.0f : 60.0f + (float)i * 4.0f;
        for (j = 0; j < TRAIL_N; j++) trail[i][j] = vis[i].to;
    }
    ui.last_step = -1;
    ui.redraw_top = ui.redraw_bottom = 1;
}

void app_frame(void)
{
    uint32_t now = plat_ms(), at;
    int kick, st;
    static char last_chord[16];
    input();
    st = eng_last_step(&at, &kick);
    if (st != ui.last_step || (st >= 0 && at != ui.step_at)) {
        if (st != ui.last_step) {
            ui.last_step = st;
            ui.step_at = now;
            if (kick) ui.kick_at = now;
            ui.redraw_bottom = 1;
        }
    }
    if (ui.step_at && now - ui.step_at > 160u && now - ui.step_at < 200u) ui.redraw_bottom = 1;
    if (ui.msg_until && now >= ui.msg_until) { ui.msg_until = 0; ui.redraw_top = 1; }
    if (memcmp(last_chord, chord_txt, sizeof last_chord)) { memcpy(last_chord, chord_txt, sizeof last_chord); ui.redraw_top = 1; }
    if (ui.redraw_top) { ui.redraw_top = 0; draw_top(); }
    if (now - ui.trail_at >= 33u) { ui.trail_at = now; draw_trails(); }
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
    else return 0;
    return 1;
}
#endif
