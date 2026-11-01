/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Obsimo (OSR Records) */
/* OSR-1: drum patterns, bass sounds and motifs, the screen's cast. Written by hand (data.h is generated). */
#pragma once

/* 16 steps: X accent, x soft, o open hi-hat (hat row only), . rest. Drums stay discreet: minimal patterns. */
#define NDRUM 10
static const char *const DRUM_NAME[NDRUM] = {"KICK ONLY", "HOUSE", "TECHNO", "MINIMAL", "2-STEP", "BREAK",
                                             "HALF-TIME", "ELECTRO", "DUB", "BOSSA"};
static const struct { const char *k, *h, *r; } DRUM_DEF[NDRUM] = {
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
};

static const char *const BASS_NAME[NBASS_SOUND] = {"OFF", "SUB", "ROUND", "ACID", "REESE", "PLUCK", "FM"};
static const char *const MOTIF_NAME[NMOTIF] = {"FOLLOW", "HOLD", "OFFBEAT", "OCTAVES", "HALVES", "GALLOP",
                                               "SYNCOPE", "ROLLING"};

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
