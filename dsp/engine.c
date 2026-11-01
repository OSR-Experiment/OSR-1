/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Obsimo (OSR Records) */
/* OSR-1 sound engine. See engine.h. Single precision only (the FM-1's FPU), no double.
 * The render works in chunks of 32 frames: pitches, the filter coefficient and the LFOs are updated per chunk,
 * envelopes, oscillators and effects per sample. */
#include "engine.h"
#include "fastmath.h"
#include "plat.h"

#define SR ENG_SR

/* no libc on the device: small copies and clears of our own */
static void zero(void *d, uint32_t n) { uint8_t *p = (uint8_t *)d; while (n--) *p++ = 0; }
static void copy(void *d, const void *s, uint32_t n)
{
    uint8_t *p = (uint8_t *)d;
    const uint8_t *q = (const uint8_t *)s;
    while (n--) *p++ = *q++;
}
#define CHUNK 32u
#define KS_N 2048u                      /* plucked string loop: down to ~22 Hz */
#define CH_N 2048u                      /* chorus line */
#define DL_N 28672u                     /* ping-pong delay, per side: a dotted eighth at 70 BPM (0.64 s) */
#define RV_PRE 1323u                    /* reverb pre-delay: 30 ms */
static const uint32_t RV_LEN[4] = {1427u, 1663u, 1871u, 2053u};
static const uint32_t AP_LEN[4] = {142u, 107u, 379u, 277u};

/* ------------------------------------------------------------------ big buffers (the pool on the device) --- */
static float ks_buf[4][KS_N] PLAT_POOL;
static float ch_buf[CH_N] PLAT_POOL;
static int16_t dl_buf[2][DL_N] PLAT_POOL;
static float rv_pre[RV_PRE] PLAT_POOL;
static float rv_line0[1427] PLAT_POOL, rv_line1[1663] PLAT_POOL, rv_line2[1871] PLAT_POOL, rv_line3[2053] PLAT_POOL;
static float ap_buf0[142] PLAT_POOL, ap_buf1[107] PLAT_POOL, ap_buf2[379] PLAT_POOL, ap_buf3[277] PLAT_POOL;
static float *const rv_line[4] = {rv_line0, rv_line1, rv_line2, rv_line3};
static float *const ap_buf[4] = {ap_buf0, ap_buf1, ap_buf2, ap_buf3};

/* ------------------------------------------------------------------ command queue (main loop -> render) --- */
enum { CMD_SOUND, CMD_RHYTHM, CMD_CHORD, CMD_STOP, CMD_PARAMS, CMD_FX, CMD_GROOVE, CMD_DRUMS, CMD_BASS, CMD_MUFFLE,
       CMD_BLIP, CMD_PANIC };
typedef struct {
    uint8_t t;
    union {
        sound_t s;
        rhythm_t r;
        struct { float notes[4], bass, glide; } c;
        struct { float bpm, cut, q, side; uint8_t drums; } p;
        struct { float rev, del, trem, chorus; } f;
        float m;
        int blip;
        struct { float swing, dens, gate, strum; } g;
        struct { drums_t d; float k, h, r; } d;
        struct { uint8_t sound, motif; float level, cut; } b;
    } u;
} cmd_t;
#define QN 16u
static cmd_t cq[QN];
static volatile uint32_t cq_head, cq_tail;

static void push(const cmd_t *c)
{
    uint32_t h = cq_head, nh = (h + 1u) % QN;
    if (nh == cq_tail)                  /* full (the render is not running): drop */
        return;
    cq[h] = *c;
    __asm__ volatile("" ::: "memory");
    cq_head = nh;
}

/* ------------------------------------------------------------------ state (render side) --- */
typedef struct {
    float m, target, inc;               /* pitch (MIDI, fractional) and glide step per chunk */
    float p1, p2, pc, pm, prev, last;
    uint32_t w;                         /* pluck write position */
} voice_t;

static struct {
    sound_t p;
    rhythm_t rh;
    voice_t v[4];
    float bm, btarget, binc, bp, bs, b2;
    float cut, q, bpm, side;
    uint8_t drums_on, active, running;
    /* groove */
    float swing, dens, gatem, strum, sdel[4], sg[4];
    /* drums */
    drums_t dr;
    float dk, dh, dr_l, k_gain, h_env, h_gain, h_dec, h_lp, h_lp2, r_env, r_gain, r_ph1, r_ph2;
    /* bass */
    uint8_t bsound, bmotif, bstage, boct;
    float blevel, bcut, be, bgate, bfenv, bic1, bic2, bg, bk;
    /* chorus, tremolo */
    float chorus, trem, tr_ph;
    /* muffle, blips */
    float muf, muf_t, ml, mr;
    float bl_env, bl_ph, bl_f, bl_df, bl_dec, bl_ph2;
    int bl_kind;
    int step;
    float step_left;
    /* envelope */
    float e, vel, atk_s, rel_s;
    int stage;
    float gate_left;
    float fenv, ienv;
    /* filter */
    float ic1, ic2, g;
    /* transition fade */
    float fade_len, fade_left;
    uint8_t pending, retrig;
    float pend_notes[4], pend_bass;
    /* lfo, noise */
    float wob_ph;
    uint32_t rng;
    /* kick + sidechain */
    float k_ph, k_amp, k_pitch, k_tail, k_last, duck;
    uint8_t duck_atk;
    /* fx */
    float rev, del, ch_ph1, ch_ph2;
    uint32_t ch_w, dl_w, rv_w[4], ap_w[4], pre_w;
    float rv_lp[4], rv_g[4], rv_hpl, rv_hpr, rv_xl, rv_xr, dl_lp;
    float dl_time;
    /* clock for the screen */
    uint32_t samples;
    volatile int last_step;
    volatile uint32_t last_step_ms;
    volatile int last_kick;
    /* MIDI out (USB): the chord's and the bass's notes as they sound, channel 1 */
    uint8_t mv[4], mb, mvel;
    volatile int last_hit;
    volatile uint32_t last_hit_ms;
    float level;
} E;

static inline float mtof(float m) { return 440.0f * fm_exp2f((m - 69.0f) * (1.0f / 12.0f)); }
static inline float rnd(void)
{
    uint32_t x = E.rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    E.rng = x;
    return (float)(int32_t)x * (1.0f / 2147483648.0f);
}

static inline float blep(float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}
static inline float osc(int w, float ph, float dt)
{
    switch (w) {
    case W_SAW: return 2.0f * ph - 1.0f - blep(ph, dt);
    case W_SQUARE: {
        float s = ph < 0.5f ? 1.0f : -1.0f, p2 = ph + 0.5f;
        if (p2 >= 1.0f) p2 -= 1.0f;
        return s + blep(ph, dt) - blep(p2, dt);
    }
    case W_TRI: return 1.0f - 4.0f * fm_fabsf(ph - 0.5f);
    default: return fm_sin_turns(ph);
    }
}

/* ------------------------------------------------------------------ main-loop API --- */
void eng_sound(const sound_t *p) { cmd_t c; c.t = CMD_SOUND; c.u.s = *p; push(&c); }
void eng_rhythm(const rhythm_t *r) { cmd_t c; c.t = CMD_RHYTHM; c.u.r = *r; push(&c); }
void eng_chord(const float notes[4], float bass, float glide_s)
{
    cmd_t c;
    c.t = CMD_CHORD;
    copy(c.u.c.notes, notes, sizeof c.u.c.notes);
    c.u.c.bass = bass;
    c.u.c.glide = glide_s;
    push(&c);
}
void eng_stop(void) { cmd_t c; c.t = CMD_STOP; push(&c); }
void eng_panic(void) { cmd_t c; c.t = CMD_PANIC; push(&c); }
void eng_params(float bpm, float cut_hz, float q, float side, int drums_on)
{
    cmd_t c;
    c.t = CMD_PARAMS;
    c.u.p.bpm = bpm; c.u.p.cut = cut_hz; c.u.p.q = q; c.u.p.side = side; c.u.p.drums = (uint8_t)drums_on;
    push(&c);
}
void eng_fx(float reverb, float delay, float trem, float chorus)
{
    cmd_t c;
    c.t = CMD_FX; c.u.f.rev = reverb; c.u.f.del = delay; c.u.f.trem = trem; c.u.f.chorus = chorus;
    push(&c);
}
void eng_muffle(float m) { cmd_t c; c.t = CMD_MUFFLE; c.u.m = m; push(&c); }
void eng_blip(int kind) { cmd_t c; c.t = CMD_BLIP; c.u.blip = kind; push(&c); }
void eng_groove(float swing, float density, float gate, float strum)
{
    cmd_t c;
    c.t = CMD_GROOVE; c.u.g.swing = swing; c.u.g.dens = density; c.u.g.gate = gate; c.u.g.strum = strum;
    push(&c);
}
void eng_drums(const drums_t *d, float kick, float hat, float rim)
{
    cmd_t c;
    c.t = CMD_DRUMS; c.u.d.d = *d; c.u.d.k = kick; c.u.d.h = hat; c.u.d.r = rim;
    push(&c);
}
void eng_bass(int sound, int motif, float level, float cut)
{
    cmd_t c;
    c.t = CMD_BASS; c.u.b.sound = (uint8_t)sound; c.u.b.motif = (uint8_t)motif; c.u.b.level = level; c.u.b.cut = cut;
    push(&c);
}

int eng_last_step(uint32_t *at_ms, int *kick)
{
    if (at_ms) *at_ms = E.last_step_ms;
    if (kick) *kick = E.last_kick;
    return E.last_step;
}
int eng_last_hit(uint32_t *at_ms)
{
    if (at_ms) *at_ms = E.last_hit_ms;
    return E.last_hit;
}
float eng_level(void) { return E.level; }

static inline float rndu(void) { return 0.5f + 0.5f * rnd(); }
static uint32_t now_ms(void) { return (uint32_t)((float)E.samples * (1000.0f / SR)); }

/* ------------------------------------------------------------------ render side --- */
static void set_notes(const float *notes, float bass, float glide_chunks)
{
    int i;
    for (i = 0; i < 4; i++) {
        voice_t *v = &E.v[i];
        v->target = notes[i];
        if (glide_chunks < 1.0f) { v->m = notes[i]; v->inc = 0.0f; }
        else v->inc = (notes[i] - v->m) / glide_chunks;
    }
    E.btarget = bass;
    if (glide_chunks < 1.0f) { E.bm = bass; E.binc = 0.0f; }
    else E.binc = (bass - E.bm) / glide_chunks;
}

static void excite(voice_t *v, float bright)
{
    float L = SR / mtof(v->m), lp = 0.0f, a = 0.06f + 0.42f * bright;   /* a softer pick: less of a hard click */
    uint32_t n, j;
    float *buf = ks_buf[v - E.v];
    if (L > (float)(KS_N - 4u)) L = (float)(KS_N - 4u);
    n = (uint32_t)L + 2u;
    for (j = 0; j < n; j++) {
        uint32_t k = (v->w + KS_N - n + j) & (KS_N - 1u);
        lp += a * (rnd() - lp);
        buf[k] = buf[k] * 0.25f + lp * 1.4f;
    }
}

/* ------------------------------------------------------------------ MIDI out --- */
/* The notes go out as they sound: rhythm, strum, latch, glide (a glide = the new notes, legato). The chord's four
 * voices and the bass, all on channel 1, by USB (the FM-1 has no MIDI out jack). 0xFF: no note on. */
#define MIDI_NONE 0xFFu
static void midi_on(uint8_t n, uint8_t v)
{
    if (plat_midi_out_room(TO_USB) < 8u) return;      /* keep room for the note-offs we owe */
    plat_midi_send(0x09u | 0x90u << 8 | (uint32_t)n << 16 | (uint32_t)v << 24, TO_USB);
}
static void midi_off(uint8_t n) { plat_midi_send(0x08u | 0x80u << 8 | (uint32_t)n << 16 | 64u << 24, TO_USB); }
static uint8_t midi_note(float m)
{
    int n = (int)(m + 0.5f);
    return (uint8_t)(n < 0 ? 0 : n > 127 ? 127 : n);
}
static void midi_voice_on(int i)
{
    uint8_t n = midi_note(E.v[i].target);
    if (E.mv[i] != MIDI_NONE) midi_off(E.mv[i]);
    midi_on(n, E.mvel);
    E.mv[i] = n;
}
static void midi_bass_on(void)
{
    uint8_t n;
    if (E.bsound == BASS_OFF) return;
    n = midi_note(E.btarget + (E.boct ? 12.0f : 0.0f));
    if (E.mb != MIDI_NONE) midi_off(E.mb);
    midi_on(n, E.mvel);
    E.mb = n;
}
static void midi_bass_off(void) { if (E.mb != MIDI_NONE) { midi_off(E.mb); E.mb = MIDI_NONE; } }
static void midi_chord_off(void)
{
    int i;
    for (i = 0; i < 4; i++)
        if (E.mv[i] != MIDI_NONE) { midi_off(E.mv[i]); E.mv[i] = MIDI_NONE; }
    if (E.bmotif == MOTIF_SUIT) midi_bass_off();
}
/* the chord changed under held notes (a glide): the new notes, legato */
static void midi_retarget(void)
{
    int i;
    for (i = 0; i < 4; i++)
        if (E.mv[i] != MIDI_NONE && E.mv[i] != midi_note(E.v[i].target)) midi_voice_on(i);
    if (E.mb != MIDI_NONE && E.mb != midi_note(E.btarget + (E.boct ? 12.0f : 0.0f))) midi_bass_on();
}

static void hit(float vel, float gate_s, float atk)
{
    const sound_t *p = &E.p;
    int i;
    if (E.fade_left > 0.0f && E.pending) {
        set_notes(E.pend_notes, E.pend_bass, 0.0f);
        E.pending = 0;
        E.fade_left = 0.0f;
    }
    E.vel = vel;
    E.stage = 1;
    E.gate_left = gate_s;
    if (atk <= 0.0f)
        atk = E.rh.hold ? p->atk : fm_minf(p->atk, 0.012f);   /* in a rhythm, attacks stay short */
    E.atk_s = fm_maxf(1.0f, atk * SR);
    E.rel_s = (E.rh.hold ? p->rel : fm_minf(p->rel, 0.3f)) * SR;
    E.fenv = 1.0f;
    E.ienv = 1.0f;
    if (E.bmotif == MOTIF_SUIT || E.bmotif == MOTIF_TENUE) E.bfenv = 1.0f;
    E.mvel = (uint8_t)(30.0f + 97.0f * fm_minf(1.0f, vel));
    for (i = 0; i < 4; i++) {           /* strum: the voices start one after the other, low to high */
        E.sdel[i] = E.strum * 0.032f * SR * (float)i;
        if (E.sdel[i] > 0.0f) E.sg[i] = 0.0f;
        else {
            if (p->engine == ENGINE_PLUCK) excite(&E.v[i], p->bright);
            midi_voice_on(i);
        }
    }
    if (E.bmotif == MOTIF_SUIT) midi_bass_on();
    E.last_hit = vel > 0.8f ? 1 : 2;
    E.last_hit_ms = now_ms();
}

static float step_len(int i)
{
    float sw = fm_minf(0.42f, E.rh.swing + E.swing), d = 60.0f / E.bpm / 4.0f * SR;
    return (i & 1) ? d * (1.0f - sw) : d * (1.0f + sw);
}

static void start_transport(void) { E.running = 1; E.step = 0; E.step_left = 0.0f; }

/* the transport runs while chords play a rhythm (with the drums), or the bass a motif */
static int need_run(void) { return E.active && (!E.rh.hold || E.bmotif >= MOTIF_CONTRE); }   /* rhythm OFF: no drums */
static void check_run(void)
{
    if (need_run()) { if (!E.running) start_transport(); }
    else E.running = 0;
}

/* bass motifs: x a note, o an octave up, - it lasts one more step */
static const char *const MOTIF_STR[NMOTIF] = {
    "", "", "..x-..x-..x-..x-", "x.o.x.o.x.o.x.o.", "x-------x-------", "x.xxx.xxx.xxx.xx", "x-.x-.x-..x-.x-.",
    "xxxxxxxxxxxxxxxx",
};

static void bass_hit(float gate_s, int oct)
{
    E.bstage = 1;
    E.bgate = gate_s;
    E.boct = (uint8_t)oct;
    E.bfenv = 1.0f;
    if (!E.mvel) E.mvel = 100;
    midi_bass_on();
}

static void on_step(void)
{
    int i = E.step, kick = 0;
    float sl = step_len(i);
    if (E.drums_on && E.active && !E.rh.hold) {      /* the drums play with a rhythm only */
        if (E.dr.k[i] > 0.0f) {
            E.k_tail = E.k_last; E.k_ph = 0.0f; E.k_amp = 1.0f; E.k_pitch = 1.0f;
            E.k_gain = E.dr.k[i] * E.dk;
            E.duck_atk = 1;
            kick = 1;
        }
        if (E.dr.h[i] > 0.0f) {
            E.h_env = 1.0f;
            E.h_gain = E.dr.h[i] * E.dh * (0.85f + 0.15f * rndu());
            E.h_dec = fm_expf(-1.0f / (fm_maxf(0.003f, E.dr.open[i] ? E.dr.kit.h_open : E.dr.kit.h_closed) * SR));
        }
        if (E.dr.r[i] > 0.0f) { E.r_env = 1.0f; E.r_gain = E.dr.r[i] * E.dr_l; E.r_ph1 = E.r_ph2 = 0.0f; }
    }
    if (E.active && !E.rh.hold) {
        float v = E.rh.v[i];
        int nlen = E.rh.len[i];
        if (v > 0.0f && E.dens < 0.0f && i != 0 && rndu() < -E.dens * 0.75f) v = 0.0f;      /* fewer hits */
        else if (v <= 0.0f && E.dens > 0.0f && (i & 1) && !(E.stage >= 1 && E.stage <= 3)
                 && rndu() < E.dens * 0.5f) { v = 0.45f; nlen = 1; }                        /* ghost hits */
        if (v > 0.0f) {
            float len = 0.0f, g;
            int k;
            for (k = 0; k < nlen; k++)
                len += step_len((i + k) & 15);
            g = fm_minf(len * 1.02f, len * E.rh.gate * E.gatem);
            hit(v * (1.0f - 0.12f * E.swing * 2.0f * rndu()), g, E.rh.atk);
        }
    }
    if (E.active && E.bmotif >= MOTIF_CONTRE) {
        const char *m = MOTIF_STR[E.bmotif];
        if (m[i] == 'x' || m[i] == 'o') {
            int k = 1;
            while (i + k < 16 && m[i + k] == '-') k++;
            bass_hit(sl * (float)k * 0.8f, m[i] == 'o');
        }
    }
    E.last_step = i;
    E.last_step_ms = now_ms();
    E.last_kick = kick;
    if (!need_run())
        E.running = 0;
}

static void chord_cmd(const float *notes, float bass, float glide)
{
    float gc = glide * SR / (float)CHUNK;
    if (!E.active) {
        E.active = 1;
        set_notes(notes, bass, 0.0f);
        E.fade_left = 0.0f;
        E.pending = 0;
        if (E.rh.hold) hit(1.0f, 1e9f, 0.0f);
        if (E.bmotif == MOTIF_TENUE) bass_hit(1e9f, 0);
        check_run();
        return;
    }
    if (gc >= 1.0f) { set_notes(notes, bass, gc); midi_retarget(); return; }
    if (E.e > 0.002f) {                 /* a quick fade, then the new chord as a fresh hit */
        copy(E.pend_notes, notes, sizeof E.pend_notes);
        E.pend_bass = bass;
        E.pending = 1;
        E.retrig = E.stage >= 1 && E.stage <= 3;
        if (E.fade_left <= 0.0f) E.fade_left = E.fade_len;
    } else {
        set_notes(notes, bass, 0.0f);
    }
}

static void run_commands(void)
{
    while (cq_tail != cq_head) {
        const cmd_t *c = &cq[cq_tail];
        switch (c->t) {
        case CMD_SOUND: E.p = c->u.s; break;
        case CMD_RHYTHM:
            E.rh = c->u.r;
            if (E.active) {
                if (E.rh.hold) hit(1.0f, 1e9f, 0.0f);
                else E.gate_left = 0.0f;
            }
            check_run();
            break;
        case CMD_CHORD: chord_cmd(c->u.c.notes, c->u.c.bass, c->u.c.glide); break;
        case CMD_STOP:
            E.active = 0;
            if (E.stage >= 1 && E.stage <= 3) E.gate_left = 0.0f;
            if (E.bstage >= 1 && E.bstage <= 2) E.bgate = 0.0f;
            check_run();
            break;
        case CMD_PARAMS:
            E.bpm = c->u.p.bpm; E.cut = c->u.p.cut; E.q = c->u.p.q;
            E.side = c->u.p.side; E.drums_on = c->u.p.drums;
            check_run();
            E.dl_time = fm_minf(0.75f * 60.0f / E.bpm * SR, (float)(DL_N - 2u));
            break;
        case CMD_FX: E.rev = c->u.f.rev; E.del = c->u.f.del; E.trem = c->u.f.trem; E.chorus = c->u.f.chorus; break;
        case CMD_MUFFLE: E.muf_t = c->u.m; break;
        case CMD_BLIP:
            E.bl_kind = c->u.blip; E.bl_env = 1.0f; E.bl_ph = E.bl_ph2 = 0.0f;
            if (E.bl_kind == BLIP_PLOP) { E.bl_f = 320.0f; E.bl_df = 1.0006f; E.bl_dec = 0.99972f; }
            else { E.bl_f = 880.0f; E.bl_df = 1.0f; E.bl_dec = 0.99905f; }
            break;
        case CMD_GROOVE:
            E.swing = c->u.g.swing * 0.3f; E.dens = c->u.g.dens;
            E.gatem = 0.2f + 1.6f * c->u.g.gate; E.strum = c->u.g.strum;
            break;
        case CMD_DRUMS: E.dr = c->u.d.d; E.dk = c->u.d.k; E.dh = c->u.d.h; E.dr_l = c->u.d.r; break;
        case CMD_BASS: {
            uint8_t was = E.bmotif;
            if (c->u.b.sound != E.bsound || c->u.b.motif != E.bmotif) midi_bass_off();
            E.bsound = c->u.b.sound; E.bmotif = c->u.b.motif; E.blevel = c->u.b.level;
            E.bcut = 55.0f * fm_exp2f(c->u.b.cut * 6.6f);        /* 55 Hz .. 5.4 kHz */
            if (E.active && E.bmotif == MOTIF_TENUE && was != MOTIF_TENUE) bass_hit(1e9f, 0);
            check_run();
            break;
        }
        case CMD_PANIC: E.active = 0; E.running = 0; E.e = 0.0f; E.stage = 0; midi_chord_off(); midi_bass_off(); break;
        }
        __asm__ volatile("" ::: "memory");
        cq_tail = (cq_tail + 1u) % QN;
    }
}

void eng_init(void)
{
    int i;
    zero(&E, sizeof E);
    zero(ks_buf, sizeof ks_buf);
    zero(ch_buf, sizeof ch_buf);
    zero(dl_buf, sizeof dl_buf);
    zero(rv_pre, sizeof rv_pre);
    for (i = 0; i < 4; i++) {
        zero(rv_line[i], RV_LEN[i] * sizeof(float));
        zero(ap_buf[i], AP_LEN[i] * sizeof(float));
        /* decay per line for a 2.4 s RT60: 10^(-3 len / (SR T60)) */
        E.rv_g[i] = fm_exp2f(-3.0f * 3.321928f * (float)RV_LEN[i] / (SR * 2.4f));
        E.v[i].m = E.v[i].target = 60.0f;
        E.v[i].p1 = 0.13f * (float)i;
        E.v[i].p2 = 0.37f * (float)i;
    }
    E.bm = E.btarget = 36.0f;
    E.rng = 0x1234567u;
    E.cut = 3800.0f; E.q = 1.0f; E.bpm = 118.0f;
    E.rh.hold = 1;
    E.fade_len = 0.004f * SR;
    E.last_step = -1;
    E.gatem = 1.0f;
    for (i = 0; i < 4; i++) E.mv[i] = MIDI_NONE;
    E.mb = MIDI_NONE;
    E.dr.kit.k_base = 62.0f; E.dr.kit.k_sweep = 95.0f; E.dr.kit.k_pdec = 0.018f; E.dr.kit.k_dec = 0.052f;
    E.dr.kit.k_drive = 1.6f; E.dr.kit.h_tone = 0.5f; E.dr.kit.h_closed = 0.016f; E.dr.kit.h_open = 0.09f;
    E.dr.kit.r_f1 = 1720.0f; E.dr.kit.r_f2 = 480.0f; E.dr.kit.r_dec = 0.008f; E.dr.kit.r_noise = 0.25f;
    E.bsound = BASS_ROND; E.blevel = 0.6f; E.bcut = 600.0f;
    for (i = 0; i < 4; i++) E.sg[i] = 1.0f;
    E.dl_time = 0.75f * 60.0f / 118.0f * SR;
    E.p.engine = ENGINE_SUB; E.p.sus = 1.0f; E.p.atk = 0.01f; E.p.rel = 0.5f; E.p.dec = 0.3f; E.p.level = 1.0f;
    E.p.bass = 0.45f; E.p.mix2 = 0.5f; E.p.ratio = 1.0f; E.p.idx = 1.0f; E.p.idec = 0.4f; E.p.bright = 0.6f;
    E.p.damp = 0.996f; E.p.fdec = 0.3f; E.p.wob_rate = 0.3f;
}

static inline float rd_line(const float *b, uint32_t n, uint32_t w, float d)   /* fractional read d behind w */
{
    float rp = (float)w - d;
    int32_t i0;
    float fr, a, c;
    while (rp < 0.0f) rp += (float)n;
    i0 = (int32_t)rp;
    fr = rp - (float)i0;
    a = b[(uint32_t)i0 % n];
    c = b[(uint32_t)(i0 + 1) % n];
    return a + (c - a) * fr;
}

void eng_render(int32_t *lr, uint32_t n, float gain)
{
    const sound_t *p = &E.p;
    uint32_t c0, s, i;
    float lvl_acc = 0.0f;
    run_commands();
    {
        float dC = fm_expf(-1.0f / (fm_maxf(0.005f, p->dec) * SR * 0.3f));
        float rC = fm_expf(-1.0f / (fm_maxf(0.004f, E.rel_s / SR) * SR * 0.25f));
        float fC = fm_expf(-1.0f / (fm_maxf(0.01f, p->fdec) * SR * 0.33f));
        float iC = fm_expf(-1.0f / (fm_maxf(0.01f, p->idec) * SR * 0.33f));
        const float duckC = 0.99974808f;          /* exp(-1 / (0.09 s)) */
        const float duckA = 0.00565385f;          /* 1 - exp(-1 / (4 ms)) */
        const float tailC = 0.99247253f;          /* exp(-1 / (3 ms)) */
        const kit_t *kt = &E.dr.kit;
        const float kDecay = fm_expf(-1.0f / (fm_maxf(0.01f, kt->k_dec) * SR));    /* the kick's body */
        const float kPdecay = fm_expf(-1.0f / (fm_maxf(0.002f, kt->k_pdec) * SR)); /* its pitch drop */
        const float hpC = 0.25f + 0.55f * kt->h_tone;
        const float rDec = fm_expf(-1.0f / (fm_maxf(0.002f, kt->r_dec) * SR));
        const int eng = p->engine;
        const float lvl = p->level * (eng == ENGINE_FM ? 0.16f : eng == ENGINE_PLUCK ? 0.30f : 0.11f);
        const float kdamp = 1.0f / fm_maxf(0.3f, E.q);
        const float ang = E.rev * 1.5707963f;
        const float dry = fm_maxf(0.18f, fm_cosf(ang)), rsend = fm_sinf(ang) * 1.1f;
        const float dsend = E.del * 0.7f, cw = fm_minf(1.3f, p->chorus * 0.8f + E.chorus * 1.1f);
        const float rv_damp = 0.575f;             /* ~6 kHz in the reverb's loop */

        for (c0 = 0; c0 < n; c0 += CHUNK) {
            uint32_t cn = n - c0 < CHUNK ? n - c0 : CHUNK;
            float inc1[4], inc2[4], incc[4], incm[4], ksL[4], binc;
            float wob = p->wob * 0.01f * fm_sin_turns(E.wob_ph), fc, ch1, ch2;
            E.wob_ph += p->wob_rate * (float)cn / SR;
            if (E.wob_ph >= 1.0f) E.wob_ph -= 1.0f;
            for (i = 0; i < 4; i++) {           /* pitches for this chunk */
                voice_t *v = &E.v[i];
                float m;
                if (v->inc != 0.0f) {
                    v->m += v->inc;
                    if ((v->inc > 0.0f && v->m >= v->target) || (v->inc < 0.0f && v->m <= v->target)) {
                        v->m = v->target; v->inc = 0.0f;
                    }
                }
                m = v->m + wob;
                if (eng == ENGINE_SUB) {
                    inc1[i] = mtof(m - p->det * 0.005f) / SR;
                    inc2[i] = mtof(m + p->oct2 + p->det * 0.005f) / SR;
                } else if (eng == ENGINE_FM) {
                    incc[i] = mtof(m) / SR;
                    incm[i] = incc[i] * p->ratio;
                } else {
                    ksL[i] = fm_minf(SR / mtof(m), (float)(KS_N - 4u));
                }
            }
            if (E.binc != 0.0f) {
                E.bm += E.binc;
                if ((E.binc > 0.0f && E.bm >= E.btarget) || (E.binc < 0.0f && E.bm <= E.btarget)) {
                    E.bm = E.btarget; E.binc = 0.0f;
                }
            }
            binc = mtof(E.bm) / SR;
            {   /* the bass's filter, its envelope and the tremolo phase for this chunk */
                int bs = E.bsound;
                float famt = bs == BASS_ACID ? 3.6f : bs == BASS_PLUCK ? 2.6f : bs == BASS_FM ? 0.0f : 0.6f;
                float tau = bs == BASS_ACID ? 0.16f : bs == BASS_PLUCK ? 0.09f : 0.25f;
                float bfc = fm_minf(SR * 0.45f, E.bcut * fm_exp2f(famt * E.bfenv));
                E.bg = fm_tanf(3.14159265f * bfc / SR);
                E.bk = bs == BASS_ACID ? 0.18f : bs == BASS_REESE ? 0.9f : 1.3f;
                E.bfenv *= fm_expf(-(float)cn / (tau * SR));
                if (E.bfenv < 1e-5f) E.bfenv = 0.0f;
                E.bic1 = fm_flush(E.bic1); E.bic2 = fm_flush(E.bic2);
                E.ml = fm_flush(E.ml); E.mr = fm_flush(E.mr);
                E.muf += (E.muf_t - E.muf) * 0.03f;
                E.h_lp = fm_flush(E.h_lp); E.h_lp2 = fm_flush(E.h_lp2);
                E.tr_ph += E.bpm * (2.0f / 60.0f) * (float)cn / SR; if (E.tr_ph >= 1.0f) E.tr_ph -= 1.0f;
            }
            E.ic1 = fm_flush(E.ic1); E.ic2 = fm_flush(E.ic2); E.dl_lp = fm_flush(E.dl_lp);
            E.rv_hpl = fm_flush(E.rv_hpl); E.rv_hpr = fm_flush(E.rv_hpr); E.k_tail = fm_flush(E.k_tail);
            E.duck = fm_flush(E.duck);
            for (i = 0; i < 4; i++) { E.rv_lp[i] = fm_flush(E.rv_lp[i]); E.v[i].prev = fm_flush(E.v[i].prev); E.v[i].last = fm_flush(E.v[i].last); }
            fc = fm_minf(SR * 0.45f, E.cut * fm_exp2f(p->fenv * E.fenv * E.vel));
            if (eng == ENGINE_PLUCK) fc = fm_minf(fc, 2400.0f);   /* plucked strings: their own ceiling, never harsh */
            E.g = fm_tanf(3.14159265f * fc / SR);
            /* chorus taps for this chunk: 12 ms and 19 ms, +-2.8 ms */
            E.ch_ph1 += 0.31f * (float)cn / SR; if (E.ch_ph1 >= 1.0f) E.ch_ph1 -= 1.0f;
            E.ch_ph2 += 0.37f * (float)cn / SR; if (E.ch_ph2 >= 1.0f) E.ch_ph2 -= 1.0f;
            ch1 = (0.012f + 0.0028f * fm_sin_turns(E.ch_ph1)) * SR;
            ch2 = (0.019f + 0.0028f * fm_sin_turns(E.ch_ph2)) * SR;

            for (s = c0; s < c0 + cn; s++) {
                float sum = 0.0f, y, bass, kv = 0.0f, fade = 1.0f, l, r, dh_l, dh_r;
                float susL = p->sus * E.vel;
                /* transport */
                if (E.running) {
                    if (E.step_left <= 0.0f) {
                        on_step();
                        E.step_left += step_len(E.step);
                        E.step = (E.step + 1) & 15;
                    }
                    E.step_left -= 1.0f;
                }
                /* amp envelope */
                switch (E.stage) {
                case 1: E.e += E.vel / E.atk_s; if (E.e >= E.vel) { E.e = E.vel; E.stage = 2; } break;
                case 2: E.e = susL + (E.e - susL) * dC; if (fm_fabsf(E.e - susL) < 1e-4f) E.stage = 3; break;
                case 3: E.e = susL; break;
                case 4: E.e *= rC; if (E.e < 1e-5f) { E.e = 0.0f; E.stage = 0; } break;
                default: break;
                }
                if (E.stage >= 1 && E.stage <= 3) {
                    E.gate_left -= 1.0f;
                    if (E.gate_left <= 0.0f) {
                        E.stage = 4;
                        midi_chord_off();
                        rC = fm_expf(-1.0f / (fm_maxf(0.004f, E.rel_s / SR) * SR * 0.25f));
                    }
                }
                E.fenv *= fC;
                E.ienv *= iC;
                if (E.fenv < 1e-6f) E.fenv = 0.0f;   /* no denormals: they cost a lot */
                if (E.ienv < 1e-6f) E.ienv = 0.0f;
                /* transition fade */
                if (E.fade_left > 0.0f) {
                    fade = E.fade_left / E.fade_len;
                    E.fade_left -= 1.0f;
                    if (E.fade_left <= 0.0f && E.pending) {
                        E.pending = 0;
                        set_notes(E.pend_notes, E.pend_bass, 0.0f);
                        E.e = 0.0f;
                        if (E.retrig) hit(E.vel, fm_maxf(E.gate_left, SR * 0.04f), 0.0f);
                        else { E.stage = 0; midi_chord_off(); }
                        fade = 0.0f;
                        for (i = 0; i < 4; i++) {   /* the new notes' increments right away */
                            float m = E.v[i].m + wob;
                            if (eng == ENGINE_SUB) {
                                inc1[i] = mtof(m - p->det * 0.005f) / SR;
                                inc2[i] = mtof(m + p->oct2 + p->det * 0.005f) / SR;
                            } else if (eng == ENGINE_FM) {
                                incc[i] = mtof(m) / SR; incm[i] = incc[i] * p->ratio;
                            } else {
                                ksL[i] = fm_minf(SR / mtof(m), (float)(KS_N - 4u));
                            }
                        }
                        binc = mtof(E.bm) / SR;
                    }
                }
                /* voices */
                for (i = 0; i < 4; i++) {
                    voice_t *v = &E.v[i];
                    float vo;
                    if (E.sdel[i] > 0.0f) {         /* strum: this voice waits its turn */
                        E.sdel[i] -= 1.0f;
                        if (E.sdel[i] <= 0.0f) {
                            if (eng == ENGINE_PLUCK) excite(v, p->bright);
                            midi_voice_on((int)i);
                        }
                    }
                    if (eng == ENGINE_SUB) {
                        vo = osc(p->w1, v->p1, inc1[i]) + p->mix2 * osc(p->w2, v->p2, inc2[i]);
                        v->p1 += inc1[i]; if (v->p1 >= 1.0f) v->p1 -= 1.0f;
                        v->p2 += inc2[i]; if (v->p2 >= 1.0f) v->p2 -= 1.0f;
                    } else if (eng == ENGINE_FM) {
                        float idx = p->idx * (p->isus + (1.0f - p->isus) * E.ienv);
                        float mod = fm_sin_turns(v->pm + p->fb * v->prev * 0.15915494f) * idx;
                        v->prev = mod;
                        vo = fm_sin_turns(v->pc + mod * 0.15915494f);
                        v->pc += incc[i]; if (v->pc >= 1.0f) v->pc -= 1.0f;
                        v->pm += incm[i]; if (v->pm >= 1.0f) v->pm -= 1.0f;
                    } else {
                        float *b = ks_buf[i];
                        float x = rd_line(b, KS_N, v->w, ksL[i]);
                        b[v->w] = p->damp * 0.5f * (x + v->last) + 1e-15f;
                        v->last = x;
                        v->w = (v->w + 1u) & (KS_N - 1u);
                        vo = x;
                    }
                    if (E.sg[i] < 1.0f) {
                        if (E.sdel[i] <= 0.0f) E.sg[i] = fm_minf(1.0f, E.sg[i] + 0.004f);
                        vo *= E.sg[i];
                    }
                    sum += vo;
                }
                sum += p->noise * rnd() * 4.0f;
                /* filter: TPT state-variable lowpass */
                {
                    float g = E.g, a1 = 1.0f / (1.0f + g * (g + kdamp)), a2 = g * a1, a3 = g * a2;
                    float v3 = sum - E.ic2, v1 = a1 * E.ic1 + a2 * v3, v2 = E.ic2 + a2 * E.ic1 + a3 * v3;
                    E.ic1 = 2.0f * v1 - E.ic1;
                    E.ic2 = 2.0f * v2 - E.ic2;
                    sum = v2;
                }
                /* bass: its own sound and filter; on SUIT it follows the chords' envelope, otherwise its own */
                bass = 0.0f;
                if (E.bsound != BASS_OFF) {
                    float bi = binc * (E.boct ? 2.0f : 1.0f), o, amp;
                    switch (E.bsound) {
                    case BASS_SUB: o = fm_sin_turns(E.bp) * 0.85f + fm_sin_turns(E.bs) * 0.55f; break;
                    case BASS_ROND: o = (1.0f - 4.0f * fm_fabsf(E.bp - 0.5f)) * 0.55f + fm_sin_turns(E.bs) * 0.8f; break;
                    case BASS_ACID: o = osc(W_SAW, E.bp, bi) * 0.9f; break;
                    case BASS_REESE: o = (osc(W_SAW, E.bp, bi) + osc(W_SAW, E.b2, bi * 1.007f)) * 0.5f; break;
                    case BASS_PLUCK: o = osc(W_SQUARE, E.bp, bi) * 0.45f + fm_sin_turns(E.bs) * 0.6f; break;
                    default: o = fm_sin_turns(E.bp + (0.1f + 0.35f * E.bfenv) * fm_sin_turns(E.bp)) * 1.1f; break;
                    }
                    E.bp += bi; if (E.bp >= 1.0f) E.bp -= 1.0f;
                    E.bs += bi * 0.5f; if (E.bs >= 1.0f) E.bs -= 1.0f;
                    E.b2 += bi * 1.007f; if (E.b2 >= 1.0f) E.b2 -= 1.0f;
                    {
                        float g = E.bg, a1 = 1.0f / (1.0f + g * (g + E.bk)), a2 = g * a1, a3 = g * a2;
                        float v3 = o - E.bic2, v1 = a1 * E.bic1 + a2 * v3, v2 = E.bic2 + a2 * E.bic1 + a3 * v3;
                        E.bic1 = 2.0f * v1 - E.bic1;
                        E.bic2 = 2.0f * v2 - E.bic2;
                        o = v2;
                    }
                    if (E.bmotif == MOTIF_SUIT) amp = E.e * fade;
                    else {
                        int perc = E.bsound == BASS_ACID || E.bsound == BASS_PLUCK || E.bsound == BASS_FM;
                        switch (E.bstage) {
                        case 1: E.be += 1.0f / (0.003f * SR); if (E.be >= 1.0f) { E.be = 1.0f; E.bstage = 2; } break;
                        case 2: if (perc) E.be *= 0.99987f; break;               /* ~0.18 s */
                        case 3: E.be *= 0.99943f; if (E.be < 1e-5f) { E.be = 0.0f; E.bstage = 0; } break;   /* ~40 ms */
                        default: break;
                        }
                        if (E.bstage == 1 || E.bstage == 2) {
                            E.bgate -= 1.0f;
                            if (E.bgate <= 0.0f) { E.bstage = 3; midi_bass_off(); }
                        }
                        amp = E.be;
                    }
                    bass = o * amp * E.blevel * 0.75f;
                }
                y = (sum * lvl * E.e * fade) + bass * 0.2f;
                if (E.trem > 0.001f) y *= 1.0f - E.trem * (0.5f + 0.5f * fm_sin_turns(E.tr_ph));
                /* kick: short, dry, no click */
                if (E.k_amp > 1e-4f) {
                    float f = kt->k_base + kt->k_sweep * E.k_pitch;
                    float ramp = E.k_ph < 0.05f ? 1.0f : 1.0f;
                    E.k_ph += f / SR;
                    if (E.k_ph >= 1.0f) E.k_ph -= 1.0f;
                    kv = fm_tanhf(fm_sin_turns(E.k_ph) * E.k_amp * kt->k_drive) / fm_tanhf(kt->k_drive) * ramp;
                    E.k_amp *= kDecay;
                    E.k_pitch *= kPdecay;
                }
                E.k_tail *= tailC;
                E.k_last = kv;
                kv = (kv + E.k_tail) * 0.5f * E.k_gain;
                /* hi-hat: high-passed noise; rim: two short tones and a click */
                {
                    float hv = 0.0f, rv = 0.0f;
                    if (E.h_env > 1e-4f) {
                        float nz = rnd(), hp;
                        E.h_lp += hpC * (nz - E.h_lp);
                        hp = nz - E.h_lp;
                        E.h_lp2 += 0.5f * (hp - E.h_lp2);
                        hv = (hp - 0.3f * E.h_lp2) * E.h_env * E.h_gain * 0.3f;
                        E.h_env *= E.h_dec;
                    }
                    if (E.r_env > 1e-4f) {
                        E.r_ph1 += kt->r_f1 / SR; if (E.r_ph1 >= 1.0f) E.r_ph1 -= 1.0f;
                        E.r_ph2 += kt->r_f2 / SR; if (E.r_ph2 >= 1.0f) E.r_ph2 -= 1.0f;
                        rv = (fm_sin_turns(E.r_ph1) * 0.55f + fm_sin_turns(E.r_ph2) * 0.45f + rnd() * kt->r_noise * E.r_env)
                             * E.r_env * E.r_gain * 0.35f;
                        E.r_env *= rDec;
                    }
                    dh_l = hv * 0.85f + rv; dh_r = hv + rv * 0.85f;
                }
                /* sidechain */
                if (E.duck_atk) { E.duck += (1.0f - E.duck) * duckA; if (E.duck > 0.985f) E.duck_atk = 0; }
                else E.duck *= duckC;
                /* chorus */
                ch_buf[E.ch_w] = y;
                {
                    float t1 = rd_line(ch_buf, CH_N, E.ch_w, ch1) * cw, t2 = rd_line(ch_buf, CH_N, E.ch_w, ch2) * cw;
                    l = y + 0.98f * t1 + 0.2f * t2;
                    r = y + 0.2f * t1 + 0.98f * t2;
                }
                E.ch_w = (E.ch_w + 1u) & (CH_N - 1u);
                /* ping-pong delay (16-bit lines) */
                {
                    uint32_t rp = (E.dl_w + DL_N - (uint32_t)E.dl_time) % DL_N;
                    float dlo = (float)dl_buf[0][rp] * (1.0f / 32767.0f), dro = (float)dl_buf[1][rp] * (1.0f / 32767.0f);
                    float in = (l + r) * 0.5f * dsend;
                    E.dl_lp += 0.36f * (dro - E.dl_lp);          /* ~3.2 kHz in the feedback */
                    dl_buf[0][E.dl_w] = (int16_t)(fm_clampf(in + 0.42f * E.dl_lp, -1.0f, 1.0f) * 32767.0f);
                    dl_buf[1][E.dl_w] = (int16_t)(fm_clampf(dlo, -1.0f, 1.0f) * 32767.0f);
                    E.dl_w = (E.dl_w + 1u) % DL_N;
                    dlo *= 0.8f; dro *= 0.8f;
                    /* reverb: pre-delay, 4 allpass diffusers, 4-line feedback network (Householder) */
                    {
                        float x = ((l + r) * 0.5f + (dlo + dro) * 0.5f) * rsend + 1e-15f, yl[4], sumy = 0.0f, ol, orr;
                        float pre = rv_pre[E.pre_w];
                        uint32_t k;
                        rv_pre[E.pre_w] = x;
                        E.pre_w = (E.pre_w + 1u) % RV_PRE;
                        x = pre;
                        for (k = 0; k < 4u; k++) {
                            float *ab = ap_buf[k], d = ab[E.ap_w[k]], o = d - 0.7f * x;
                            ab[E.ap_w[k]] = x + 0.7f * o;
                            E.ap_w[k] = (E.ap_w[k] + 1u) % AP_LEN[k];
                            x = o;
                        }
                        for (k = 0; k < 4u; k++) {
                            float o = rv_line[k][E.rv_w[k]];
                            E.rv_lp[k] += rv_damp * (o - E.rv_lp[k]);
                            yl[k] = E.rv_lp[k];
                            sumy += yl[k];
                        }
                        sumy *= 0.5f;
                        for (k = 0; k < 4u; k++) {
                            rv_line[k][E.rv_w[k]] = (yl[k] - sumy) * E.rv_g[k] + x;
                            E.rv_w[k] = (E.rv_w[k] + 1u) % RV_LEN[k];
                        }
                        ol = (yl[0] + yl[2]) * 0.6f;
                        orr = (yl[1] + yl[3]) * 0.6f;
                        /* ~180 Hz high-pass on the return */
                        E.rv_hpl += 0.0253f * (ol - E.rv_hpl);
                        E.rv_hpr += 0.0253f * (orr - E.rv_hpr);
                        ol -= E.rv_hpl;
                        orr -= E.rv_hpr;
                        {
                            float dk = fm_minf(1.0f, E.duck * 1.1f), sh = dk * dk * (3.0f - 2.0f * dk);
                            float dg = 1.0f - 0.85f * E.side * sh;
                            l = (l * dry + dlo + ol) * dg + kv + dh_l;
                            r = (r * dry + dro + orr) * dg + kv + dh_r;
                        }
                    }
                }
                if (E.bl_env > 1e-4f) {           /* plop (a rising bubble) / toc (a knock on the glass) */
                    float bv;
                    E.bl_ph += E.bl_f / SR; if (E.bl_ph >= 1.0f) E.bl_ph -= 1.0f;
                    E.bl_ph2 += E.bl_f * 1.73f / SR; if (E.bl_ph2 >= 1.0f) E.bl_ph2 -= 1.0f;
                    if (E.bl_kind == BLIP_PLOP) bv = fm_sin_turns(E.bl_ph);
                    else bv = fm_sin_turns(E.bl_ph) * 0.6f + fm_sin_turns(E.bl_ph2) * 0.3f + rnd() * 0.3f * E.bl_env;
                    bv *= E.bl_env * 0.22f;
                    E.bl_f *= E.bl_df;
                    E.bl_env *= E.bl_dec;
                    l += bv; r += bv;
                }
                if (E.muf > 0.002f) {            /* heard from outside the room */
                    float a = 1.0f - 0.9f * E.muf, g = 1.0f - 0.45f * E.muf;
                    E.ml += a * (l - E.ml); E.mr += a * (r - E.mr);
                    l = E.ml * g; r = E.mr * g;
                }
                lvl_acc += fm_fabsf(l) + fm_fabsf(r);
                l = fm_tanhf(l * 1.1f) * 0.9f;
                r = fm_tanhf(r * 1.1f) * 0.9f;
                lr[2 * s] = (int32_t)(l * gain);
                lr[2 * s + 1] = (int32_t)(r * gain);
                E.samples++;
            }
        }
    }
    E.level += (fm_minf(1.0f, lvl_acc / (float)n * 2.5f) - E.level) * 0.3f;
}
