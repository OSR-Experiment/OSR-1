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
enum { W_SAW, W_SQUARE, W_TRI, W_SINE, W_PWM };   /* W_PWM: a pulse whose width an LFO moves (sound_t pwm) */

typedef struct {
    uint8_t engine, w1, w2;
    float det, oct2, mix2;             /* sub: detune (cents), second osc (semitones), its level */
    float ratio, idx, idec, isus, fb;  /* fm: modulator ratio, index, index decay (s), index sustain, feedback */
    float bright, damp;                /* pluck: excitation brightness, loop damping */
    float atk, dec, sus, rel;          /* amp envelope (s, s, 0..1, s) */
    float fenv, fdec;                  /* filter envelope (octaves, s) */
    float chorus, wob, wob_rate;       /* chorus amount, pitch wobble (cents, Hz) */
    float noise, bass, level;          /* noise level, bass level (unused: the bass has its own), output level */
    /* analog-style additions (all 0 = off: the sounds above are unchanged) */
    float mix3, oct3, det3;            /* sub: a third oscillator (wave w1): its level, semitones, detune (cents) */
    float pwm, pwm_rate;               /* W_PWM: the pulse width swings 0.5 +- pwm (0..0.42) at pwm_rate Hz */
    float flfo, flfo_rate;             /* the filter's cutoff swings +- flfo octaves at flfo_rate Hz */
    float reso;                        /* the filter's resonance: Q x reso (0: x 1) */
    uint8_t ladder;                    /* 1: a 24 dB ladder-style filter with a soft saturation (Moog-like) */
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
    /* the second drum page (0 = those voices are not in the kit's patterns) */
    float sn_f, sn_noise, sn_dec;                    /* snare: tone (Hz), noise share 0..1, decay (s) */
    float cl_dec;                                    /* clap: its tail's decay (s) after three short bursts */
    float tm_f, tm_dec;                              /* tom: pitch (Hz; an accent a fourth higher), decay (s) */
    float cb_dec;                                    /* cowbell: decay (s) of its two squares (540 and 800 Hz) */
} kit_t;

typedef struct {                       /* a drum pattern: velocities per step (0 = rest), and its kit */
    float k[16], h[16], r[16];
    float s[16], c[16], t[16], b[16];  /* snare, clap, tom, cowbell */
    uint8_t open[16];                  /* the hi-hat rings longer on these steps */
    kit_t kit;
} drums_t;

enum { BLIP_PLOP, BLIP_TOC };

enum { BASS_OFF, BASS_SUB, BASS_ROND, BASS_ACID, BASS_REESE, BASS_PLUCK, BASS_FM,
       /* synth basses from the engine's bass table (bass_def_t) */
       BASS_MOOG, BASS_FUNK, BASS_JUNO, BASS_808, BASS_HOUSE, BASS_ACIDSQ, BASS_ACID24, BASS_DX, BASS_FMTECH,
       BASS_REESEDEEP, BASS_WOBBLE, BASS_HOOK, BASS_PICK, BASS_DARKPULSE, NBASS_SOUND };
enum { MOTIF_SUIT, MOTIF_TENUE, MOTIF_CONTRE, MOTIF_OCTAVES, MOTIF_UN, MOTIF_GALOP, MOTIF_SYNCOPE, MOTIF_ROULE,
       /* patterns by style (MOTIF_STR): x note, X accent, o octave up, 5 fifth up, 7 flat seventh up, - tie, . rest */
       MOTIF_DISCO, MOTIF_PULSE8, MOTIF_HOUSE, MOTIF_TECHNO, MOTIF_REGGAE, MOTIF_DUB, MOTIF_FUNK, MOTIF_SLAP,
       MOTIF_WALK, MOTIF_ACIDA, MOTIF_ACIDB, MOTIF_TRANCE, MOTIF_TRAP, MOTIF_SYNTHWAVE, MOTIF_BOSSA, MOTIF_DNB,
       MOTIF_POSTPUNK, MOTIF_HOOKA, MOTIF_HOOKB, MOTIF_LEGATO8, MOTIF_GOTHRUN, NMOTIF };

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
/* the second page's levels, 0..1 */
void eng_drums2(float snare, float clap, float tom, float bell);
void eng_bass(int sound, int motif, float level, float cut);
void eng_panic(void);
/* the mixer: the chords' level and the output's, 0..1 (they glide, so a knob never clicks) */
void eng_mix(float synth, float master);
/* the chord progression (the looper): N chords (4 notes and a bass each), each lasting STEPS sixteenth-steps;
 * the engine changes them on the steps, in time with the rhythm, the drums and an outside clock. Sent again while it
 * plays: the new voicings (the scale, the key ...) from the chord under way. eng_prog_run(1) starts it from the
 * first chord, (0) or eng_stop stops it; eng_prog_pos: the chord playing (0..N-1), -1 when it is not on */
void eng_prog(const float notes[][4], const float *bass, int n, int steps, int random);   /* N <= 32; random: each
                                                   * next chord a random other one */
void eng_prog_run(int on);
int eng_prog_pos(void);
void eng_render(int32_t *lr, uint32_t n, float gain);

/* for the screen: the last step played (-1 none), when (ms since start, the engine's clock), and whether a
 * kick fell on it; eng_hit_step: the last step where the chords hit (accent 1, soft 2) */
int eng_last_step(uint32_t *at_ms, int *kick);
int eng_last_hit(uint32_t *at_ms);
float eng_level(void);                 /* output level, 0..1, smoothed */
