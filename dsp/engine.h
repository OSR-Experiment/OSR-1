/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Obsimo (OSR Records) */
/* OSR-1 sound engine: 4 chord voices (sub / FM / plucked string) with strum, a resonant filter, envelopes,
 * a 16-step chord-rhythm sequencer with groove (swing, density, gate), a bass with its own sounds and motifs,
 * a small drum kit (kick, hi-hat, rim) on 16-step patterns with a kit per style, sidechain, tremolo,
 * chorus, ping-pong delay, a light reverb, a muffled 'outside the room' mode and two little sound effects.
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

typedef struct {                       /* a drum kit's sound */
    float k_base, k_sweep, k_pdec, k_dec, k_drive;   /* kick: Hz, Hz added at the hit, its pitch drop (s), body (s) */
    float h_tone, h_closed, h_open;                  /* hi-hat: 0 dark .. 1 bright, decays (s) */
    float r_f1, r_f2, r_dec, r_noise;                /* rim: two tones (Hz), decay (s), click */
} kit_t;

typedef struct {                       /* a drum pattern: velocities per step (0 = rest), and its kit */
    float k[16], h[16], r[16];
    uint8_t open[16];                  /* the hi-hat rings longer on these steps */
    kit_t kit;
} drums_t;

enum { BLIP_PLOP, BLIP_TOC };

enum { BASS_OFF, BASS_SUB, BASS_ROND, BASS_ACID, BASS_REESE, BASS_PLUCK, BASS_FM, NBASS_SOUND };
enum { MOTIF_SUIT, MOTIF_TENUE, MOTIF_CONTRE, MOTIF_OCTAVES, MOTIF_UN, MOTIF_GALOP, MOTIF_SYNCOPE, MOTIF_ROULE, NMOTIF };

void eng_init(void);
void eng_sound(const sound_t *p);
void eng_rhythm(const rhythm_t *r);
void eng_chord(const float notes[4], float bass, float glide_s);
void eng_stop(void);
/* an outside MIDI clock's beat: the even step STEP (0..15) began at AT_MS (plat_ms); STEP -1: the clock is gone */
void eng_sync(int step, uint32_t at_ms);
/* side: sidechain amount 0..1; drums_on: the drum kit plays while chords play (its kicks pump the sidechain) */
void eng_params(float bpm, float cut_hz, float q, float side, int drums_on);
/* all 0..1 */
void eng_fx(float reverb, float delay, float trem, float chorus);
/* 0..1: the sound as heard from outside the room (muffled, quieter) */
void eng_muffle(float m);
/* a little sound effect (BLIP_*) */
void eng_blip(int kind);
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
