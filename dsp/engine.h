/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Obsimo (OSR Records) */
/* OSR-1 sound engine: 4 chord voices + bass (sub / FM / plucked string), a resonant filter, envelopes,
 * a 16-step chord-rhythm sequencer, a 4/4 kick, sidechain, chorus, ping-pong delay and a light reverb.
 * Port of the browser prototype's worklet. The main loop talks to it through eng_* calls (a small queue);
 * eng_render runs in the audio interrupt. */
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
    float noise, bass, level;          /* noise level, bass level, output level */
} sound_t;

typedef struct {
    uint8_t hold;                      /* 1: the chord is held (no pattern) */
    float v[16];                       /* step velocity (0 = rest) */
    uint8_t len[16];                   /* steps the note lasts (ties) */
    float gate, swing, atk;
} rhythm_t;

void eng_init(void);
void eng_sound(const sound_t *p);
void eng_rhythm(const rhythm_t *r);
void eng_chord(const float notes[4], float bass, float glide_s);
void eng_stop(void);
void eng_params(float bpm, float cut_hz, float q, int kick, int side, int bass_on);
void eng_fx(float reverb, float delay);
void eng_panic(void);
void eng_render(int32_t *lr, uint32_t n, float gain);

/* for the screen: the last step played (-1 none), when (ms since start, the engine's clock), and the last kick */
int eng_last_step(uint32_t *at_ms, int *kick);
float eng_level(void);                 /* output level, 0..1, smoothed */
