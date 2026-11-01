/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Obsimo (OSR Records) */
/* OSR-1 sound engine: 4 chord voices (sub / FM / plucked string) with strum, a resonant filter, envelopes,
 * a 16-step chord-rhythm sequencer with groove (swing, density, gate), a bass with its own sounds and motifs,
 * a small drum kit (kick, hi-hat, rim) on 16-step patterns, sidechain, tape (wow, flutter, saturation),
 * tremolo, chorus, ping-pong delay and a light reverb.
 * The main loop talks to it through eng_* calls (a small queue); eng_render runs in the audio interrupt. */
#pragma once
#include <stdint.h>

#define ENG_SR 44100.0f

enum { ENGINE_SUB, ENGINE_FM, ENGINE_PLUCK };
enum { W_SAW, W_SQUARE, W_TRI, W_SINE };

typedef struct {
    uint8_t engine, w1, w2;
    float det, oct2, mix2;             /* sub: detune (cents), second osc (semitones), its level */
    float ratio, idx, idec, isus, fb;  /* fm: modulator ratio, index, index decay (s), index sustain, feedback */
    float bright, damp;                /* pluck: excitation brightness, loop damping */
    float atk, dec, sus, rel;          /* amp envelope (s, s, 0..1, s) */
    float fenv, fdec;                  /* filter envelope (octaves, s) */
    float chorus, wob, wob_rate;       /* chorus amount, pitch wobble (cents, Hz) */
    float noise, bass, level;          /* noise level, bass level (unused: the bass has its own), output level */
} sound_t;

typedef struct {
    uint8_t hold;                      /* 1: the chord is held (no pattern) */
    float v[16];                       /* step velocity (0 = rest) */
    uint8_t len[16];                   /* steps the note lasts (ties) */
    float gate, swing, atk;
} rhythm_t;

typedef struct {                       /* a drum pattern: velocities per step (0 = rest) */
    float k[16], h[16], r[16];
    uint8_t open[16];                  /* the hi-hat rings longer on these steps */
} drums_t;

enum { BASS_OFF, BASS_SUB, BASS_ROND, BASS_ACID, BASS_REESE, BASS_PLUCK, BASS_FM, NBASS_SOUND };
enum { MOTIF_SUIT, MOTIF_TENUE, MOTIF_CONTRE, MOTIF_OCTAVES, MOTIF_UN, MOTIF_GALOP, MOTIF_SYNCOPE, MOTIF_ROULE, NMOTIF };

void eng_init(void);
void eng_sound(const sound_t *p);
void eng_rhythm(const rhythm_t *r);
void eng_chord(const float notes[4], float bass, float glide_s);
void eng_stop(void);
/* side: sidechain amount 0..1; drums_on: the drum kit plays (and its kicks pump the sidechain) */
void eng_params(float bpm, float cut_hz, float q, float side, int drums_on);
/* all 0..1 */
void eng_fx(float reverb, float delay, float trem, float tape);
/* swing 0..1, density -1..1 (fewer / more hits), gate 0..1 (0.5 = as written), strum 0..1 */
void eng_groove(float swing, float density, float gate, float strum);
void eng_drums(const drums_t *d, float kick, float hat, float rim);
void eng_bass(int sound, int motif, float level, float cut);
void eng_panic(void);
void eng_render(int32_t *lr, uint32_t n, float gain);

/* for the screen: the last step played (-1 none), when (ms since start, the engine's clock), and whether a
 * kick fell on it; eng_hit_step: the last step where the chords hit (accent 1, soft 2) */
int eng_last_step(uint32_t *at_ms, int *kick);
int eng_last_hit(uint32_t *at_ms);
float eng_level(void);                 /* output level, 0..1, smoothed */
