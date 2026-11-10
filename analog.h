/* SPDX-License-Identifier: GPL-3.0-only */
/* ANALOG: a fifth family of 8 pads, built on the engine's analog-style additions (sound_t: a third oscillator,
 * PWM, the filter's LFO, resonance, the 24 dB ladder filter). Same logic as the hardware they are named after:
 *   PWM:    a pulse whose width a slow LFO moves + a sub, behind the chorus  (Juno-style strings)
 *   stack:  saws detuned against each other, a filter envelope that opens     (poly-synth brass and pads)
 *   ladder: three oscillators, a 24 dB filter with resonance and a soft saturation (Moog-style)
 * Kept apart from data.h (generated from the prototype), which has the 32 sounds before. */
#pragma once
#include "engine.h"

#define NSOUND 48                               /* the 32 sounds, the 8 ANALOG pads, the 8 DARK pads */
static const char *const ANALOG_NAME[8] = {"PWM STRINGS", "SOFT BRASS", "LADDER PAD", "LADDER SWEEP", "SUPER SAW",
                                           "WARM POLY", "DARK DRONE", "DREAM PAD"};

#define SUBS .engine = ENGINE_SUB, .ratio = 1.0f, .idx = 1.0f, .idec = 0.4f, .isus = 0.2f, .bright = 0.6f, .damp = 0.996f, \
             .bass = 0.45f
static const sound_t ANALOG_SOUNDS[8] = {
    /* PWM STRINGS: pulse (width moving) + saw, a sub-octave pulse, both chorus modes' worth of width */
    {SUBS, .w1 = W_PWM, .w2 = W_SAW, .det = 9.0f, .mix2 = 0.55f, .mix3 = 0.45f, .oct3 = -12.0f,
     .atk = 0.55f, .dec = 0.5f, .sus = 0.85f, .rel = 1.5f, .fenv = 0.35f, .fdec = 1.2f,
     .chorus = 1.0f, .wob = 5.0f, .wob_rate = 0.35f, .pwm = 0.32f, .pwm_rate = 0.55f,
     .flfo = 0.12f, .flfo_rate = 0.22f, .reso = 1.1f, .level = 0.64f},
    /* SOFT BRASS: two detuned saws and a third, a filter envelope that opens fast and settles */
    {SUBS, .w1 = W_SAW, .w2 = W_SAW, .det = 10.0f, .mix2 = 0.9f, .mix3 = 0.7f, .det3 = -8.0f,
     .atk = 0.08f, .dec = 0.5f, .sus = 0.75f, .rel = 0.5f, .fenv = 1.4f, .fdec = 0.45f,
     .chorus = 0.4f, .wob = 4.0f, .wob_rate = 5.2f, .reso = 1.4f, .level = 0.97f},
    /* LADDER PAD: three saws (one an octave down), the 24 dB ladder, a slow swell, a gentle sweep */
    {SUBS, .w1 = W_SAW, .w2 = W_SAW, .det = 12.0f, .mix2 = 0.9f, .mix3 = 0.9f, .oct3 = -12.0f,
     .atk = 0.9f, .dec = 0.6f, .sus = 0.9f, .rel = 1.8f, .fenv = 1.0f, .fdec = 1.4f,
     .chorus = 0.5f, .wob = 6.0f, .wob_rate = 0.3f, .flfo = 0.18f, .flfo_rate = 0.15f,
     .reso = 1.8f, .ladder = 1, .level = 0.82f},
    /* LADDER SWEEP: the same, with the resonance up and the cutoff sweeping slowly */
    {SUBS, .w1 = W_SAW, .w2 = W_SAW, .det = 16.0f, .mix2 = 0.9f, .mix3 = 0.6f,
     .atk = 0.6f, .dec = 0.6f, .sus = 0.9f, .rel = 2.2f, .fenv = 0.5f, .fdec = 1.0f,
     .chorus = 0.3f, .wob = 5.0f, .wob_rate = 0.3f, .flfo = 0.9f, .flfo_rate = 0.12f,
     .reso = 3.2f, .ladder = 1, .level = 0.88f},
    /* SUPER SAW: wide detune, three saws, the chorus at full */
    {SUBS, .w1 = W_SAW, .w2 = W_SAW, .det = 34.0f, .mix2 = 1.0f, .mix3 = 1.0f,
     .atk = 0.18f, .dec = 0.6f, .sus = 0.9f, .rel = 1.2f, .fenv = 0.8f, .fdec = 0.9f,
     .chorus = 1.0f, .wob = 9.0f, .wob_rate = 0.4f, .reso = 1.0f, .level = 0.54f},
    /* WARM POLY: saw + triangle, a soft filter envelope, the ladder for warmth */
    {SUBS, .w1 = W_SAW, .w2 = W_TRI, .det = 6.0f, .mix2 = 0.8f,
     .atk = 0.25f, .dec = 0.8f, .sus = 0.7f, .rel = 0.9f, .fenv = 0.8f, .fdec = 0.8f,
     .chorus = 0.5f, .wob = 5.0f, .wob_rate = 0.5f, .flfo = 0.1f, .flfo_rate = 0.2f,
     .reso = 1.3f, .ladder = 1, .level = 1.45f},
    /* DARK DRONE: a slow pulse, a saw an octave down, a sub, a very long swell and release */
    {SUBS, .w1 = W_PWM, .w2 = W_SAW, .det = 9.0f, .oct2 = -12.0f, .mix2 = 0.9f, .mix3 = 0.7f, .oct3 = -12.0f,
     .atk = 1.4f, .dec = 1.0f, .sus = 0.95f, .rel = 3.5f, .fenv = 0.2f, .fdec = 2.0f,
     .chorus = 0.7f, .wob = 10.0f, .wob_rate = 0.15f, .pwm = 0.15f, .pwm_rate = 0.08f,
     .flfo = 0.3f, .flfo_rate = 0.07f, .reso = 1.5f, .ladder = 1, .level = 0.6f},
    /* DREAM PAD: triangle and sine an octave up, a detuned third, a very slow swell, the chorus at full */
    {SUBS, .w1 = W_TRI, .w2 = W_SINE, .det = 15.0f, .oct2 = 12.0f, .mix2 = 0.55f, .mix3 = 0.4f, .det3 = 7.0f,
     .atk = 1.3f, .dec = 1.2f, .sus = 0.9f, .rel = 3.0f, .fenv = 0.4f, .fdec = 2.0f,
     .chorus = 1.0f, .wob = 12.0f, .wob_rate = 0.22f, .flfo = 0.2f, .flfo_rate = 0.09f, .level = 0.85f},
};

/* DARK: pads for post-punk and darkwave: string-machine strings with a heavy chorus, a choir, a cheap organ, a cold sweep */
static const char *const DARK_NAME[8] = {"COLD STRINGS", "BLACK CHOIR", "ICE PAD", "FARFISA", "SCREAM PAD", "NIGHT DRIFT",
                                         "GHOST ORGAN", "STATIC WALL"};
static const sound_t DARK_SOUNDS[8] = {
    /* COLD STRINGS: saws at three octaves, slow swell, the chorus at full (a string machine's) */
    {SUBS, .w1 = W_SAW, .w2 = W_SAW, .det = 18.0f, .oct2 = 12.0f, .mix2 = 0.4f, .mix3 = 0.5f, .oct3 = -12.0f, .det3 = -6.0f,
     .atk = 0.55f, .dec = 0.6f, .sus = 0.9f, .rel = 1.4f, .fenv = 0.3f, .fdec = 1.0f,
     .chorus = 1.0f, .wob = 6.0f, .wob_rate = 0.3f, .level = 0.79f},
    /* BLACK CHOIR: a slow pulse and a triangle an octave up, a dark filter that drifts */
    {SUBS, .w1 = W_PWM, .w2 = W_TRI, .det = 10.0f, .oct2 = 12.0f, .mix2 = 0.45f, .mix3 = 0.4f, .oct3 = -12.0f,
     .atk = 0.9f, .dec = 0.8f, .sus = 0.9f, .rel = 2.0f, .fenv = 0.2f, .fdec = 1.5f,
     .chorus = 0.9f, .wob = 8.0f, .wob_rate = 0.25f, .pwm = 0.25f, .pwm_rate = 0.3f,
     .flfo = 0.1f, .flfo_rate = 0.15f, .reso = 1.3f, .level = 0.56f},
    /* ICE PAD: a thin bright saw, a sine an octave up and a shimmer a fifth above, a long swell */
    {SUBS, .w1 = W_SAW, .w2 = W_SINE, .det = 24.0f, .oct2 = 12.0f, .mix2 = 0.6f, .mix3 = 0.5f, .oct3 = 7.0f, .det3 = 8.0f,
     .atk = 0.8f, .dec = 1.0f, .sus = 0.9f, .rel = 2.5f, .fenv = 0.8f, .fdec = 2.0f,
     .chorus = 1.0f, .wob = 10.0f, .wob_rate = 0.2f, .level = 0.67f},
    /* FARFISA: squares at three octaves, an instant attack, no filter movement, a quick vibrato (a cheap combo organ) */
    {SUBS, .w1 = W_SQUARE, .w2 = W_SQUARE, .det = 4.0f, .oct2 = 12.0f, .mix2 = 0.5f, .mix3 = 0.4f, .oct3 = -12.0f,
     .atk = 0.01f, .dec = 0.1f, .sus = 1.0f, .rel = 0.15f, .fenv = 0.0f, .fdec = 0.5f,
     .chorus = 0.3f, .wob = 7.0f, .wob_rate = 6.0f, .level = 0.45f},
    /* SCREAM PAD: saws into a resonant ladder that swings and sweeps (a cold-wave synth's screaming filter) */
    {SUBS, .w1 = W_SAW, .w2 = W_SAW, .det = 14.0f, .mix2 = 0.9f, .mix3 = 0.5f, .oct3 = -12.0f,
     .atk = 0.05f, .dec = 0.5f, .sus = 0.8f, .rel = 0.5f, .fenv = 2.0f, .fdec = 0.8f,
     .chorus = 0.2f, .wob = 4.0f, .wob_rate = 5.0f, .flfo = 0.35f, .flfo_rate = 0.4f,
     .reso = 3.0f, .ladder = 1, .level = 0.8f},
    /* NIGHT DRIFT: a dark saw stack, the ladder, a very slow sweep and a very long release */
    {SUBS, .w1 = W_SAW, .w2 = W_SAW, .det = 16.0f, .mix2 = 0.9f, .mix3 = 0.7f, .oct3 = -12.0f,
     .atk = 1.2f, .dec = 1.0f, .sus = 0.9f, .rel = 3.5f, .fenv = 0.4f, .fdec = 2.0f,
     .chorus = 0.6f, .wob = 8.0f, .wob_rate = 0.2f, .flfo = 0.7f, .flfo_rate = 0.06f,
     .reso = 1.6f, .ladder = 1, .level = 0.71f},
    /* GHOST ORGAN: a triangle and a sine stack, a slow swell, a gentle wavering */
    {SUBS, .w1 = W_TRI, .w2 = W_SINE, .det = 6.0f, .oct2 = 12.0f, .mix2 = 0.7f, .mix3 = 0.5f, .oct3 = -12.0f,
     .atk = 0.3f, .dec = 0.5f, .sus = 1.0f, .rel = 1.0f, .fenv = 0.1f, .fdec = 1.0f,
     .chorus = 0.7f, .wob = 9.0f, .wob_rate = 5.5f, .level = 0.7f},
    /* STATIC WALL: a wall of widely detuned saws through the ladder, the chorus at full */
    {SUBS, .w1 = W_SAW, .w2 = W_SAW, .det = 40.0f, .mix2 = 1.0f, .mix3 = 1.0f, .oct3 = -12.0f, .det3 = -20.0f,
     .atk = 0.4f, .dec = 0.8f, .sus = 0.9f, .rel = 1.6f, .fenv = 0.5f, .fdec = 1.2f,
     .chorus = 1.0f, .wob = 10.0f, .wob_rate = 0.3f, .flfo = 0.2f, .flfo_rate = 0.1f,
     .reso = 1.2f, .ladder = 1, .level = 0.58f},
};
#undef SUBS

static inline const sound_t *snd_def(int i) { return i < 32 ? &SOUNDS[i] : i < 40 ? &ANALOG_SOUNDS[i - 32] : &DARK_SOUNDS[i - 40]; }
static inline const char *snd_name(int i) { return i < 32 ? SOUND_NAME[i] : i < 40 ? ANALOG_NAME[i - 32] : DARK_NAME[i - 40]; }
static inline const char *fam_name(int i) { return i < 4 ? FAMILY_NAME[i] : i == 4 ? "ANALOG" : "DARK WAVE"; }
