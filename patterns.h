/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Obsimo (OSR Records) */
/* OSR-1: drum patterns, bass sounds and motifs, the screen's cast. Written by hand (data.h is generated). */
#pragma once

/* 16 steps: X accent, x soft, o open hi-hat (hat row only), . rest. Drums stay discreet: minimal patterns. */
#define NDRUM 40
static const char *const DRUM_NAME[NDRUM] = {"KICK ONLY", "HOUSE", "TECHNO", "MINIMAL", "2-STEP", "BREAK",
                                             "HALF-TIME", "ELECTRO", "DUB", "BOSSA",
                                             "BOOM BAP", "TRAP", "REGGAE", "DISCO", "FUNK", "808 BOUNCE",
                                             "BREAKBEAT", "TRIBAL", "ROCK", "HALF STEP",
                                             "AFRO", "SON CLAVE", "SAMBA", "DRILL", "ELECTRO FUNK", "INDUSTRIAL",
                                             "LO-FI", "AMBIENT", "JAZZ", "TRANCE",
                                             "POST PUNK", "MECHANICAL", "TOM TRIBE", "TOM DRIVE", "MOTORIK",
                                             "GOTH MACHINE", "SLOW TOMS", "COLD WAVE", "NEW WAVE", "DUB WAVE"};
/* k kick, h hi-hat (o: open), r rim, then the second page's voices: s snare, c clap, t tom (X: higher), b cowbell
 * (a missing row: that voice does not play) */
static const struct { const char *k, *h, *r, *s, *c, *t, *b; } DRUM_DEF[NDRUM] = {
    {"X...X...X...X...", "................", "................"},   /* kick seul */
    {"X...X...X...X...", "..x...x...x...x.", "....x.......x..."},   /* house */
    {"X...X...X...X...", "xxXxxxXxxxXxxxXx", "..........x....."},   /* techno */
    {"X.....x...X.....", "..o...o...o...o.", ".......x.....x.."},   /* minimal */
    {"X......x..X.....", "..x...x.x.x...x.", "....x.......x..."},   /* 2-step */
    {"X.....X...X..X..", "x.x.x.x.x.x.x.x.", "....X.......X..x"},   /* break */
    {"X.........x.....", "x...x...x...x...", "........X......."},   /* demi-temps */
    {"X..X......X..X..", "x.x.x.x.x.x.x.x.", "....X.......X..."},   /* electro */
    {"X.......X.......", "..o.......o.....", "....x.......x..x"},   /* dub */
    {"X..xX..xX..xX..x", "x.x.x.x.x.x.x.x.", "x..x..x...x..x.."},   /* bossa */
    {"X.....x...X.....", "x.x.x.x.x.x.x.x.", "................", "....X.......X..."},                  /* boom bap */
    {"X.....x....x....", "x.x.x.x.x.x.xoxx", "................", "........X.......", "........x......."},  /* trap */
    {"........X.......", "x.x.x.x.x.x.x.x.", "........x.......", "................"},                  /* reggae: one drop */
    {"X...X...X...X...", "..o...o...o...o.", "................", "....X.......X...", "....x.......x..."},  /* disco */
    {"X..x..X...x..X..", "x.xxx.xxx.xxx.xx", "................", "....X..x.x..X..."},                  /* funk */
    {"X..x..x...x..x..", "x.x.x.x.x.x.x.x.", "................", "....X.......X...", "....x.......x...",
     "................", "..........x....."},                                                              /* 808 bounce */
    {"X.X.......X.X...", "x.x.x.x.x.x.x.x.", "................", "....X..x.x..X..."},                  /* breakbeat */
    {"X.....X.....X...", "x...x...x...x...", "................", "................", "................",
     "..x.x...x.X.x...", "......x...x....."},                                                              /* tribal */
    {"X.....X.X.......", "x.x.x.x.x.x.x.x.", "................", "....X.......X..."},                  /* rock */
    {"X.........X.....", "x.x.x.x.x.x.x.xx", "................", "........X.......", "........x......."},  /* half step */
    {"X..x..X..x..X...", "x.x.x.x.x.x.x.x.", "..x.......x.....", "................", "................",
     "................", "x.x..x.x..x.x..."},                                                              /* afro */
    {"X.......X.......", "................", "x..x..x...x.x...", "................", "................",
     "................", "..x...x...x...x."},                                                              /* son clave: 3-2 */
    {"X.......X.......", "x.xxx.xxx.xxx.xx", "................", "................", "................",
     "..x.x.x...x.x.x.", "x...x...x...x..."},                                                              /* samba */
    {"X.....x.....x...", "x.x.xxx.x.x.xxx.", "................", "........X.......", "........x......."},  /* drill */
    {"X..x....X..x....", "x.x.x.x.x.x.x.x.", "................", "....X.......X..x", "....x.......x..."},  /* electro funk */
    {"X.X...X.X.X...X.", "................", "x...............", "....X.......X...", "................",
     "..........x.x..."},                                                                                  /* industrial */
    {"X.....x..x......", "x.x...x.x.x...x.", "................", "....X.......X..."},                  /* lo-fi */
    {"X...............", "x...............", "................", "................", "................",
     "........x.......", "............x..."},                                                              /* ambient */
    {"X.......X.......", "x..xx..xx..xx..x", "....x.......x...", "....x..x.x..x..."},                   /* jazz */
    {"X...X...X...X...", "..o...o...o...o.", "................", "................", "....x.......x..."},  /* trance */
    {"X.....X.X.......", "x.x.x.x.x.x.x.o.", "................", "....X.......X..."},                  /* post punk */
    {"X.......X.......", "XxxxXxxxXxxxXxxx", "................", "....X.......X..."},                  /* mechanical */
    {"X.......X.......", "................", "................", "....x.......X...", "................",
     "..x.x.X...x.x.X."},                                                                                  /* tom tribe */
    {"X.....X.........", "x...x...x...x...", "................", "................", "................",
     "X.x.x.X.x.x.X.x."},                                                                                  /* tom drive */
    {"X...X...X...X...", "X.x.X.x.X.x.X.x.", "................", "....X.......X..."},                  /* motorik */
    {"X.....X.X.......", "x.x.x.x.x.x.x.x.", "................", "....X.......X...", "....x.......x..."},  /* goth machine */
    {"X.........X.....", "................", "................", "........X.......", "................",
     "....x.....X.x..."},                                                                                  /* slow toms */
    {"X.......X...X...", "x...x...x...x...", "..x...x...x...x.", "................", "....X.......X..."},  /* cold wave */
    {"X...X...X...X...", "x.o.x.o.x.o.x.xo", "................", "....X.......X...", "....x.......x..."},  /* new wave */
    {"X.......X.......", "..x...........x.", "................", "........X......."},                  /* dub wave */
};

/* each pattern's kit: kick (base Hz, sweep Hz, pitch drop s, body s, drive), hi-hat (tone, closed s, open s),
 * rim (Hz, Hz, decay s, click/noise) */
static const kit_t DRUM_KIT[NDRUM] = {
    {55.0f, 110.0f, 0.020f, 0.070f, 1.8f, 0.50f, 0.016f, 0.09f, 1720.0f, 480.0f, 0.008f, 0.25f},   /* kick seul: deep */
    {58.0f, 100.0f, 0.018f, 0.060f, 1.6f, 0.55f, 0.018f, 0.11f, 1200.0f, 700.0f, 0.030f, 0.90f},   /* house: round, clap-ish */
    {50.0f, 140.0f, 0.012f, 0.090f, 3.0f, 0.80f, 0.012f, 0.06f, 1900.0f, 900.0f, 0.006f, 0.30f},   /* techno: hard */
    {60.0f, 70.0f, 0.015f, 0.035f, 1.3f, 0.90f, 0.008f, 0.07f, 2400.0f, 1200.0f, 0.005f, 0.15f},   /* minimal: dry ticks */
    {62.0f, 120.0f, 0.014f, 0.040f, 2.0f, 0.70f, 0.012f, 0.05f, 1650.0f, 500.0f, 0.012f, 0.50f},   /* 2-step: garage rim */
    {65.0f, 130.0f, 0.020f, 0.050f, 2.5f, 0.60f, 0.020f, 0.10f, 220.0f, 330.0f, 0.060f, 0.90f},    /* break: snare */
    {48.0f, 90.0f, 0.030f, 0.120f, 1.4f, 0.45f, 0.020f, 0.12f, 200.0f, 310.0f, 0.090f, 1.00f},     /* half-time: big soft snare */
    {46.0f, 60.0f, 0.040f, 0.350f, 1.2f, 0.85f, 0.010f, 0.05f, 1000.0f, 1500.0f, 0.025f, 1.00f},   /* electro: 808, clap */
    {45.0f, 70.0f, 0.030f, 0.200f, 1.1f, 0.35f, 0.025f, 0.15f, 1500.0f, 520.0f, 0.020f, 0.20f},    /* dub: sub kick */
    {70.0f, 40.0f, 0.020f, 0.050f, 1.0f, 0.30f, 0.010f, 0.04f, 2500.0f, 1250.0f, 0.030f, 0.05f},   /* bossa: soft, clave */
    /* then: snare (Hz, noise 0..1, decay s), clap tail s, tom Hz, tom decay s, cowbell decay s */
    {52.0f, 120.0f, 0.020f, 0.110f, 1.8f, 0.45f, 0.014f, 0.08f, 1700.0f, 500.0f, 0.010f, 0.25f,
     175.0f, 0.55f, 0.20f, 0.15f, 110.0f, 0.28f, 0.22f},                                             /* boom bap: round, dusty */
    {44.0f, 80.0f, 0.045f, 0.550f, 1.2f, 0.85f, 0.009f, 0.06f, 1700.0f, 500.0f, 0.010f, 0.25f,
     200.0f, 0.85f, 0.12f, 0.12f, 110.0f, 0.28f, 0.22f},                                             /* trap: long 808 kick, tight snare */
    {60.0f, 70.0f, 0.020f, 0.140f, 1.3f, 0.50f, 0.012f, 0.07f, 1500.0f, 520.0f, 0.020f, 0.30f,
     190.0f, 0.60f, 0.15f, 0.15f, 110.0f, 0.28f, 0.22f},                                             /* reggae: cross-stick rim */
    {56.0f, 110.0f, 0.018f, 0.070f, 1.6f, 0.60f, 0.012f, 0.10f, 1700.0f, 500.0f, 0.010f, 0.25f,
     190.0f, 0.60f, 0.15f, 0.16f, 110.0f, 0.28f, 0.22f},                                             /* disco */
    {58.0f, 100.0f, 0.016f, 0.060f, 1.7f, 0.55f, 0.010f, 0.07f, 1700.0f, 500.0f, 0.010f, 0.25f,
     185.0f, 0.60f, 0.13f, 0.15f, 110.0f, 0.28f, 0.22f},                                             /* funk: dry, snappy */
    {46.0f, 60.0f, 0.040f, 0.450f, 1.2f, 0.80f, 0.010f, 0.05f, 1700.0f, 500.0f, 0.010f, 0.25f,
     210.0f, 0.75f, 0.14f, 0.14f, 110.0f, 0.28f, 0.24f},                                             /* 808 bounce: long kick, cowbell */
    {62.0f, 120.0f, 0.016f, 0.060f, 2.0f, 0.55f, 0.012f, 0.08f, 1700.0f, 500.0f, 0.010f, 0.25f,
     200.0f, 0.65f, 0.14f, 0.15f, 110.0f, 0.28f, 0.22f},                                             /* breakbeat */
    {56.0f, 100.0f, 0.020f, 0.100f, 1.5f, 0.40f, 0.014f, 0.08f, 1700.0f, 500.0f, 0.010f, 0.25f,
     190.0f, 0.60f, 0.15f, 0.15f, 95.0f, 0.32f, 0.20f},                                              /* tribal: toms, cowbell */
    {60.0f, 110.0f, 0.015f, 0.070f, 2.2f, 0.50f, 0.012f, 0.09f, 1700.0f, 500.0f, 0.010f, 0.25f,
     180.0f, 0.70f, 0.17f, 0.15f, 110.0f, 0.28f, 0.22f},                                             /* rock: hard kick, open snare */
    {42.0f, 90.0f, 0.040f, 0.600f, 1.4f, 0.80f, 0.010f, 0.06f, 1700.0f, 500.0f, 0.010f, 0.25f,
     195.0f, 0.80f, 0.22f, 0.20f, 110.0f, 0.28f, 0.22f},                                             /* half step: sub kick, big snare */
    {58.0f, 100.0f, 0.016f, 0.080f, 1.5f, 0.55f, 0.012f, 0.07f, 1500.0f, 520.0f, 0.018f, 0.30f,
     190.0f, 0.60f, 0.14f, 0.15f, 105.0f, 0.30f, 0.20f},                                             /* afro: rim, cowbell */
    {60.0f, 70.0f, 0.020f, 0.100f, 1.3f, 0.40f, 0.012f, 0.07f, 2600.0f, 1300.0f, 0.035f, 0.05f,
     190.0f, 0.60f, 0.14f, 0.15f, 120.0f, 0.30f, 0.24f},                                             /* son clave: clave, cowbell */
    {52.0f, 90.0f, 0.020f, 0.140f, 1.4f, 0.50f, 0.012f, 0.06f, 1700.0f, 500.0f, 0.010f, 0.25f,
     190.0f, 0.60f, 0.14f, 0.15f, 85.0f, 0.34f, 0.20f},                                              /* samba: surdo toms, bell */
    {44.0f, 85.0f, 0.040f, 0.500f, 1.3f, 0.85f, 0.009f, 0.06f, 1700.0f, 500.0f, 0.010f, 0.25f,
     205.0f, 0.80f, 0.13f, 0.12f, 110.0f, 0.28f, 0.22f},                                             /* drill: 808 kick, tight snare */
    {58.0f, 100.0f, 0.016f, 0.070f, 1.7f, 0.60f, 0.010f, 0.08f, 1700.0f, 500.0f, 0.010f, 0.25f,
     185.0f, 0.60f, 0.13f, 0.15f, 110.0f, 0.28f, 0.22f},                                             /* electro funk */
    {50.0f, 140.0f, 0.012f, 0.090f, 3.0f, 0.80f, 0.012f, 0.06f, 1900.0f, 900.0f, 0.006f, 0.30f,
     200.0f, 0.85f, 0.10f, 0.15f, 130.0f, 0.18f, 0.18f},                                             /* industrial: hard and dry */
    {55.0f, 90.0f, 0.022f, 0.090f, 1.1f, 0.35f, 0.016f, 0.07f, 1700.0f, 500.0f, 0.010f, 0.25f,
     170.0f, 0.40f, 0.20f, 0.15f, 110.0f, 0.28f, 0.22f},                                             /* lo-fi: soft, dusty */
    {50.0f, 70.0f, 0.030f, 0.200f, 1.1f, 0.30f, 0.025f, 0.15f, 1700.0f, 500.0f, 0.010f, 0.25f,
     190.0f, 0.60f, 0.15f, 0.15f, 90.0f, 0.45f, 0.30f},                                              /* ambient: sparse, long */
    {60.0f, 60.0f, 0.020f, 0.080f, 1.0f, 0.40f, 0.012f, 0.10f, 1700.0f, 500.0f, 0.040f, 0.20f,
     180.0f, 0.30f, 0.12f, 0.15f, 110.0f, 0.28f, 0.22f},                                             /* jazz: brushed */
    {56.0f, 110.0f, 0.016f, 0.075f, 1.7f, 0.60f, 0.012f, 0.14f, 1700.0f, 500.0f, 0.010f, 0.25f,
     190.0f, 0.60f, 0.15f, 0.16f, 110.0f, 0.28f, 0.22f},                                             /* trance */
    {58.0f, 110.0f, 0.016f, 0.070f, 1.8f, 0.55f, 0.012f, 0.08f, 1700.0f, 500.0f, 0.010f, 0.25f,
     185.0f, 0.85f, 0.12f, 0.15f, 110.0f, 0.28f, 0.22f},                                             /* post punk: tight kick, dry hissing snare */
    {56.0f, 100.0f, 0.016f, 0.060f, 1.6f, 0.50f, 0.010f, 0.06f, 1700.0f, 500.0f, 0.010f, 0.25f,
     180.0f, 0.80f, 0.11f, 0.15f, 110.0f, 0.28f, 0.22f},                                             /* mechanical: a steady machine */
    {54.0f, 100.0f, 0.020f, 0.100f, 1.5f, 0.45f, 0.014f, 0.08f, 1700.0f, 500.0f, 0.010f, 0.25f,
     190.0f, 0.70f, 0.15f, 0.15f, 90.0f, 0.38f, 0.22f},                                              /* tom tribe: deep, ringing toms */
    {56.0f, 100.0f, 0.018f, 0.090f, 1.5f, 0.50f, 0.012f, 0.08f, 1700.0f, 500.0f, 0.010f, 0.25f,
     190.0f, 0.70f, 0.14f, 0.15f, 100.0f, 0.30f, 0.22f},                                             /* tom drive */
    {60.0f, 110.0f, 0.016f, 0.075f, 1.7f, 0.60f, 0.012f, 0.10f, 1700.0f, 500.0f, 0.010f, 0.25f,
     190.0f, 0.70f, 0.13f, 0.15f, 110.0f, 0.28f, 0.22f},                                             /* motorik */
    {56.0f, 100.0f, 0.016f, 0.065f, 1.8f, 0.55f, 0.012f, 0.07f, 1700.0f, 500.0f, 0.010f, 0.25f,
     200.0f, 0.80f, 0.13f, 0.16f, 110.0f, 0.28f, 0.22f},                                             /* goth machine: gated, dry */
    {52.0f, 90.0f, 0.025f, 0.130f, 1.4f, 0.40f, 0.014f, 0.08f, 1700.0f, 500.0f, 0.010f, 0.25f,
     190.0f, 0.75f, 0.20f, 0.15f, 80.0f, 0.45f, 0.22f},                                              /* slow toms: half-time, long toms */
    {62.0f, 120.0f, 0.014f, 0.050f, 2.0f, 0.65f, 0.010f, 0.06f, 2200.0f, 1100.0f, 0.012f, 0.10f,
     220.0f, 0.75f, 0.10f, 0.14f, 110.0f, 0.28f, 0.22f},                                             /* cold wave: dry, electronic */
    {58.0f, 100.0f, 0.016f, 0.070f, 1.6f, 0.55f, 0.012f, 0.12f, 1700.0f, 500.0f, 0.010f, 0.25f,
     195.0f, 0.65f, 0.14f, 0.15f, 110.0f, 0.28f, 0.22f},                                             /* new wave */
    {50.0f, 80.0f, 0.030f, 0.200f, 1.3f, 0.45f, 0.014f, 0.10f, 1700.0f, 500.0f, 0.010f, 0.25f,
     190.0f, 0.70f, 0.25f, 0.18f, 100.0f, 0.30f, 0.22f},                                             /* dub wave: sparse, long */
};

static const char *const BASS_NAME[NBASS_SOUND] = {"OFF", "SUB", "ROUND", "ACID", "REESE", "PLUCK", "FM",
                                                   "MOOG", "FUNK", "JUNO SUB", "808 SUB", "HOUSE PLUCK", "ACID SQUARE",
                                                   "ACID 24", "DX BASS", "FM TECHNO", "REESE DEEP", "WOBBLE",
                                                   "HOOK BASS", "PICKED", "DARK PULSE"};
static const char *const MOTIF_NAME[NMOTIF] = {"FOLLOW", "HOLD", "OFFBEAT", "OCTAVES", "HALVES", "GALLOP",
                                               "SYNCOPE", "ROLLING",
                                               "DISCO", "PULSE 8THS", "HOUSE OFFBEAT", "TECHNO ROLL", "REGGAE", "DUB",
                                               "FUNK", "SLAP", "WALKING", "ACID A", "ACID B", "TRANCE", "808 TRAP",
                                               "SYNTHWAVE", "BOSSA", "DNB",
                                               "POST PUNK", "HOOK A", "HOOK B", "LEGATO 8THS", "GOTH RUN"};

#define NCREATURE 8
static const char *const CREATURE_NAME[NCREATURE] = {"PUFFER", "SUNFISH", "JELLY", "STARFISH", "OCTOPUS", "SEAHORSE",
                                                     "MANTA", "CRAB"};
#define NEYE 6
static const char *const EYE_NAME[NEYE] = {"ROUND", "HEART", "STAR", "LASER", "SPIRAL", "CROSS"};
#define NDECOR 8
static const char *const DECOR_NAME[NDECOR] = {"BLUE", "MOIRE", "EMBERS", "ACID", "ABYSS", "CORAL", "AURORA",
                                               "GREEN CRT"};
/* the eyes' light, per background */
static const float DECOR_EYE[NDECOR][3] = {{0.85f, 0.95f, 1.0f}, {1.0f, 0.35f, 0.75f}, {1.0f, 0.8f, 0.3f},
                                           {0.4f, 1.0f, 0.95f}, {0.45f, 0.9f, 1.0f}, {1.0f, 0.95f, 0.7f},
                                           {0.7f, 1.0f, 0.6f}, {0.5f, 1.0f, 0.45f}};
#define NACC 6
static const char *const ACC_NAME[NACC] = {"NONE", "PARTY HAT", "CROWN", "HEADPHONES", "BOW", "ANTENNA"};
static const char *const ONOFF_NAME[2] = {"OFF", "ON"};
