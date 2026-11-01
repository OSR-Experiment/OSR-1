/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Obsimo (OSR Records) */
/* OSR-1: drum patterns, bass sounds and motifs, the screen's cast. Written by hand (data.h is generated). */
#pragma once

/* 16 steps: X accent, x soft, o open hi-hat (hat row only), . rest. Drums stay discreet: minimal patterns. */
#define NDRUM 10
static const char *const DRUM_NAME[NDRUM] = {"KICK SEUL", "HOUSE", "TECHNO", "MINIMAL", "2-STEP", "BREAK",
                                             "DEMI-TEMPS", "ELECTRO", "DUB", "BOSSA"};
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

static const char *const BASS_NAME[NBASS_SOUND] = {"OFF", "SUB", "ROND", "ACID", "REESE", "PLUCK", "FM"};
static const char *const MOTIF_NAME[NMOTIF] = {"SUIT", "TENUE", "CONTRE", "OCTAVES", "BLANCHES", "GALOP",
                                               "SYNCOPE", "ROULANTE"};

#define NCREATURE 4
static const char *const CREATURE_NAME[NCREATURE] = {"GLOBE", "LUNE", "MEDUSE", "ETOILE"};
#define NEYE 6
static const char *const EYE_NAME[NEYE] = {"ROSE", "OR", "GLACE", "CYAN", "VERT", "ROUGE"};
static const float EYE_COL[NEYE][3] = {{1.0f, 0.35f, 0.75f}, {1.0f, 0.8f, 0.3f}, {0.85f, 0.95f, 1.0f},
                                       {0.4f, 1.0f, 0.95f}, {0.55f, 1.0f, 0.4f}, {1.0f, 0.3f, 0.22f}};
#define NDECOR 4
static const char *const DECOR_NAME[NDECOR] = {"MOIRE", "BLEU", "BRAISE", "ACIDE"};
enum { MOOD_AUTO, MOOD_CONTENT, MOOD_HEUREUX, MOOD_GRINCHEUX, MOOD_SURPRIS, NMOOD, MOOD_DORT };
static const char *const MOOD_NAME[NMOOD] = {"AUTO", "CONTENT", "HEUREUX", "GRINCHEUX", "SURPRIS"};
