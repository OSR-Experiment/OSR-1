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
enum { CMD_SOUND, CMD_RHYTHM, CMD_CHORD, CMD_STOP, CMD_PARAMS, CMD_FX, CMD_PANIC };
typedef struct {
    uint8_t t;
    union {
        sound_t s;
        rhythm_t r;
        struct { float notes[4], bass, glide; } c;
        struct { float bpm, cut, q; uint8_t kick, side, bass_on; } p;
        struct { float rev, del; } f;
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
    float bm, btarget, binc, bp, bs;
    float cut, q, bpm;
    uint8_t kick, side, bass_on, active, running;
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
void eng_params(float bpm, float cut_hz, float q, int kick, int side, int bass_on)
{
    cmd_t c;
    c.t = CMD_PARAMS;
    c.u.p.bpm = bpm; c.u.p.cut = cut_hz; c.u.p.q = q;
    c.u.p.kick = (uint8_t)kick; c.u.p.side = (uint8_t)side; c.u.p.bass_on = (uint8_t)bass_on;
    push(&c);
}
void eng_fx(float reverb, float delay) { cmd_t c; c.t = CMD_FX; c.u.f.rev = reverb; c.u.f.del = delay; push(&c); }

int eng_last_step(uint32_t *at_ms, int *kick)
{
    if (at_ms) *at_ms = E.last_step_ms;
    if (kick) *kick = E.last_kick;
    return E.last_step;
}
float eng_level(void) { return E.level; }

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
    float L = SR / mtof(v->m), lp = 0.0f, a = 0.12f + 0.88f * bright;
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
    if (p->engine == ENGINE_PLUCK)
        for (i = 0; i < 4; i++)
            excite(&E.v[i], p->bright);
}

static float step_len(int i)
{
    float sw = E.rh.swing, d = 60.0f / E.bpm / 4.0f * SR;
    return (i & 1) ? d * (1.0f - sw) : d * (1.0f + sw);
}

static void start_transport(void) { E.running = 1; E.step = 0; E.step_left = 0.0f; }

static void on_step(void)
{
    int i = E.step, beat = (i & 3) == 0;
    if (beat && E.kick) { E.k_tail = E.k_last; E.k_ph = 0.0f; E.k_amp = 1.0f; E.k_pitch = 1.0f; }
    if (beat && (E.kick || E.side)) E.duck_atk = 1;
    if (E.active && !E.rh.hold && E.rh.v[i] > 0.0f) {
        float len = 0.0f;
        int k;
        for (k = 0; k < E.rh.len[i]; k++)
            len += step_len((i + k) & 15);
        hit(E.rh.v[i], len * E.rh.gate, E.rh.atk);
    }
    E.last_step = i;
    E.last_step_ms = (uint32_t)((float)E.samples * (1000.0f / SR));
    E.last_kick = beat && E.kick;
    if (!E.active && !E.kick && !E.side)
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
        else if (!E.running) start_transport();
        return;
    }
    if (gc >= 1.0f) { set_notes(notes, bass, gc); return; }
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
                else { E.gate_left = 0.0f; if (!E.running) start_transport(); }
            }
            break;
        case CMD_CHORD: chord_cmd(c->u.c.notes, c->u.c.bass, c->u.c.glide); break;
        case CMD_STOP:
            E.active = 0;
            if (E.stage >= 1 && E.stage <= 3) E.gate_left = 0.0f;
            if (!E.kick && !E.side) E.running = 0;
            break;
        case CMD_PARAMS:
            E.bpm = c->u.p.bpm; E.cut = c->u.p.cut; E.q = c->u.p.q;
            E.kick = c->u.p.kick; E.side = c->u.p.side; E.bass_on = c->u.p.bass_on;
            if (!E.active && !E.kick && !E.side) E.running = 0;
            if ((E.kick || E.side) && !E.running) start_transport();
            E.dl_time = fm_minf(0.75f * 60.0f / E.bpm * SR, (float)(DL_N - 2u));
            break;
        case CMD_FX: E.rev = c->u.f.rev; E.del = c->u.f.del; break;
        case CMD_PANIC: E.active = 0; E.running = 0; E.e = 0.0f; E.stage = 0; break;
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
    E.bass_on = 1;
    E.last_step = -1;
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
        const float kDecay = 0.99956925f;         /* exp(-19 / SR): the kick's body */
        const float kPdecay = 0.99875357f;        /* exp(-55 / SR): its pitch drop */
        const int eng = p->engine;
        const float lvl = p->level * (eng == ENGINE_FM ? 0.16f : eng == ENGINE_PLUCK ? 0.30f : 0.11f);
        const float kdamp = 1.0f / fm_maxf(0.3f, E.q);
        const float ang = E.rev * 1.5707963f;
        const float dry = fm_maxf(0.18f, fm_cosf(ang)), rsend = fm_sinf(ang) * 1.1f;
        const float dsend = E.del * 0.7f, cw = p->chorus * 0.8f;
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
            E.ic1 = fm_flush(E.ic1); E.ic2 = fm_flush(E.ic2); E.dl_lp = fm_flush(E.dl_lp);
            E.rv_hpl = fm_flush(E.rv_hpl); E.rv_hpr = fm_flush(E.rv_hpr); E.k_tail = fm_flush(E.k_tail);
            E.duck = fm_flush(E.duck);
            for (i = 0; i < 4; i++) { E.rv_lp[i] = fm_flush(E.rv_lp[i]); E.v[i].prev = fm_flush(E.v[i].prev); E.v[i].last = fm_flush(E.v[i].last); }
            fc = fm_minf(SR * 0.45f, E.cut * fm_exp2f(p->fenv * E.fenv * E.vel));
            E.g = fm_tanf(3.14159265f * fc / SR);
            /* chorus taps for this chunk: 12 ms and 19 ms, +-2.8 ms */
            E.ch_ph1 += 0.31f * (float)cn / SR; if (E.ch_ph1 >= 1.0f) E.ch_ph1 -= 1.0f;
            E.ch_ph2 += 0.37f * (float)cn / SR; if (E.ch_ph2 >= 1.0f) E.ch_ph2 -= 1.0f;
            ch1 = (0.012f + 0.0028f * fm_sin_turns(E.ch_ph1)) * SR;
            ch2 = (0.019f + 0.0028f * fm_sin_turns(E.ch_ph2)) * SR;

            for (s = c0; s < c0 + cn; s++) {
                float sum = 0.0f, y, bass, kv = 0.0f, fade = 1.0f, l, r;
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
                        else E.stage = 0;
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
                    if (eng == ENGINE_SUB) {
                        sum += osc(p->w1, v->p1, inc1[i]) + p->mix2 * osc(p->w2, v->p2, inc2[i]);
                        v->p1 += inc1[i]; if (v->p1 >= 1.0f) v->p1 -= 1.0f;
                        v->p2 += inc2[i]; if (v->p2 >= 1.0f) v->p2 -= 1.0f;
                    } else if (eng == ENGINE_FM) {
                        float idx = p->idx * (p->isus + (1.0f - p->isus) * E.ienv);
                        float mod = fm_sin_turns(v->pm + p->fb * v->prev * 0.15915494f) * idx;
                        v->prev = mod;
                        sum += fm_sin_turns(v->pc + mod * 0.15915494f);
                        v->pc += incc[i]; if (v->pc >= 1.0f) v->pc -= 1.0f;
                        v->pm += incm[i]; if (v->pm >= 1.0f) v->pm -= 1.0f;
                    } else {
                        float *b = ks_buf[i];
                        float x = rd_line(b, KS_N, v->w, ksL[i]);
                        b[v->w] = p->damp * 0.5f * (x + v->last) + 1e-15f;
                        v->last = x;
                        v->w = (v->w + 1u) & (KS_N - 1u);
                        sum += x;
                    }
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
                /* bass: triangle + sine an octave down */
                bass = E.bass_on ? ((1.0f - 4.0f * fm_fabsf(E.bp - 0.5f)) * 0.55f + fm_sin_turns(E.bs) * 0.8f) * p->bass
                                 : 0.0f;
                E.bp += binc; if (E.bp >= 1.0f) E.bp -= 1.0f;
                E.bs += binc * 0.5f; if (E.bs >= 1.0f) E.bs -= 1.0f;
                y = (sum * lvl + bass * 0.2f) * E.e * fade;
                /* kick: short, dry, no click */
                if (E.k_amp > 1e-4f) {
                    float f = 62.0f + 95.0f * E.k_pitch;
                    float ramp = E.k_ph < 0.05f ? 1.0f : 1.0f;
                    E.k_ph += f / SR;
                    if (E.k_ph >= 1.0f) E.k_ph -= 1.0f;
                    kv = fm_tanhf(fm_sin_turns(E.k_ph) * E.k_amp * 1.6f) * 1.0805f * ramp;
                    E.k_amp *= kDecay;
                    E.k_pitch *= kPdecay;
                }
                E.k_tail *= tailC;
                E.k_last = kv;
                kv = (kv + E.k_tail) * 0.5f;
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
                            float dg = E.side ? 1.0f - 0.8f * sh : 1.0f;
                            l = (l * dry + dlo + ol) * dg + kv;
                            r = (r * dry + dro + orr) * dg + kv;
                        }
                    }
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
