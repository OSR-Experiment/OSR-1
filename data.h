/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Obsimo (OSR Records) */
/* OSR-1 data: generated from the browser prototype (gen_data.mjs). Do not edit by hand. */
#pragma once
static const char *const FAMILY_NAME[4] = {"NAPPES", "CLAVIERS", "PLUCKS", "EXPERIMENTAL"};
#define NFAMILY 4
#define PER_FAMILY 8
static const char *const SOUND_NAME[32] = {
    "SIROP",
    "VITRAIL",
    "RAVE LOINTAIN",
    "VERRE",
    "AUBE",
    "DERIVE",
    "BRUME",
    "HARMONIUM",
    "VELOURS",
    "ORGUE FANE",
    "TINES",
    "WURLI",
    "PIANO JOUET",
    "CASSETTE",
    "CLAVECIN LUNAIRE",
    "MELLOW",
    "CLOCHE",
    "KALIMBA",
    "GUITARE SECHE",
    "HARPE",
    "KOTO",
    "MARIMBA",
    "STAB HOUSE",
    "GOUTTES",
    "SITAR FANTOME",
    "BANDE USEE",
    "SOUS L'EAU",
    "METAL DOUX",
    "CRISTAUX",
    "FLÛTE DE VERRE",
    "HARPE TORDUE",
    "ORGUE NOYE"
};
static const sound_t SOUNDS[32] = {
    {ENGINE_SUB, 0, 0, 14.0f, 0.0f, 0.8f, 1.0f, 1.0f, 0.4f, 0.2f, 0.0f, 0.6f, 0.996f, 0.12f, 0.3f, 1.0f, 1.2f, 0.6f, 0.6f, 0.8f, 14.0f, 0.25f, 0.0f, 0.45f, 0.78f},   /* Sirop */
    {ENGINE_SUB, 0, 1, 7.0f, 0.0f, 0.45f, 1.0f, 1.0f, 0.4f, 0.2f, 0.0f, 0.6f, 0.996f, 0.04f, 0.3f, 1.0f, 0.6f, 1.6f, 0.35f, 0.6f, 6.0f, 0.35f, 0.0f, 0.45f, 0.84f},   /* Vitrail */
    {ENGINE_SUB, 0, 0, 22.0f, 12.0f, 0.6f, 1.0f, 1.0f, 0.4f, 0.2f, 0.0f, 0.6f, 0.996f, 0.01f, 0.5f, 0.7f, 0.4f, 2.5f, 0.22f, 0.5f, 3.0f, 0.2f, 0.0f, 0.45f, 1.21f},   /* Rave lointain */
    {ENGINE_SUB, 1, 3, 5.0f, 24.0f, 0.3f, 1.0f, 1.0f, 0.4f, 0.2f, 0.0f, 0.6f, 0.996f, 0.25f, 0.3f, 1.0f, 1.8f, 0.5f, 1.0f, 0.7f, 5.0f, 0.3f, 0.0f, 0.45f, 0.6f},   /* Verre */
    {ENGINE_FM, 0, 0, 6.0f, 0.0f, 0.5f, 1.0f, 1.2f, 2.0f, 0.5f, 0.0f, 0.6f, 0.996f, 0.6f, 0.3f, 1.0f, 2.0f, 0.3f, 0.3f, 0.7f, 4.0f, 0.3f, 0.0f, 0.45f, 0.72f},   /* Aube */
    {ENGINE_FM, 0, 0, 6.0f, 0.0f, 0.5f, 0.5f, 2.0f, 3.0f, 0.6f, 0.2f, 0.6f, 0.996f, 0.3f, 0.3f, 1.0f, 1.5f, 0.3f, 0.3f, 0.8f, 18.0f, 0.15f, 0.0f, 0.45f, 0.64f},   /* Dérive */
    {ENGINE_SUB, 2, 0, 18.0f, 0.0f, 0.4f, 1.0f, 1.0f, 0.4f, 0.2f, 0.0f, 0.6f, 0.996f, 0.8f, 0.3f, 1.0f, 2.2f, 0.2f, 0.3f, 0.9f, 8.0f, 0.3f, 0.06f, 0.45f, 0.94f},   /* Brume */
    {ENGINE_SUB, 1, 1, 4.0f, 12.0f, 0.5f, 1.0f, 1.0f, 0.4f, 0.2f, 0.0f, 0.6f, 0.996f, 0.15f, 0.3f, 1.0f, 0.8f, 0.3f, 0.3f, 0.6f, 3.0f, 5.5f, 0.0f, 0.45f, 0.58f},   /* Harmonium */
    {ENGINE_SUB, 2, 3, 4.0f, 12.0f, 0.3f, 1.0f, 1.0f, 0.4f, 0.2f, 0.0f, 0.6f, 0.996f, 0.01f, 1.2f, 0.5f, 1.0f, 1.0f, 0.5f, 0.55f, 4.0f, 0.3f, 0.0f, 0.45f, 1.45f},   /* Velours */
    {ENGINE_SUB, 1, 1, 6.0f, 12.0f, 0.5f, 1.0f, 1.0f, 0.4f, 0.2f, 0.0f, 0.6f, 0.996f, 0.02f, 0.3f, 1.0f, 0.25f, 0.2f, 0.3f, 0.65f, 9.0f, 5.5f, 0.0f, 0.45f, 0.55f},   /* Orgue fané */
    {ENGINE_FM, 0, 0, 6.0f, 0.0f, 0.5f, 1.0f, 3.0f, 0.6f, 0.1f, 0.0f, 0.6f, 0.996f, 0.002f, 1.5f, 0.35f, 0.8f, 0.8f, 0.5f, 0.5f, 3.0f, 0.3f, 0.0f, 0.45f, 1.0f},   /* Tines */
    {ENGINE_FM, 0, 0, 6.0f, 0.0f, 0.5f, 1.0f, 2.2f, 0.3f, 0.25f, 0.5f, 0.6f, 0.996f, 0.002f, 0.8f, 0.4f, 0.4f, 1.0f, 0.3f, 0.4f, 4.0f, 0.3f, 0.0f, 0.45f, 1.2f},   /* Wurli */
    {ENGINE_FM, 0, 0, 6.0f, 0.0f, 0.5f, 3.5f, 1.6f, 0.25f, 0.0f, 0.0f, 0.6f, 0.996f, 0.001f, 0.6f, 0.15f, 0.5f, 0.5f, 0.3f, 0.3f, 2.0f, 0.3f, 0.0f, 0.45f, 1.52f},   /* Piano jouet */
    {ENGINE_SUB, 2, 0, 9.0f, 0.0f, 0.35f, 1.0f, 1.0f, 0.4f, 0.2f, 0.0f, 0.6f, 0.996f, 0.02f, 0.6f, 0.7f, 0.5f, 1.4f, 0.3f, 0.4f, 22.0f, 0.6f, 0.05f, 0.45f, 1.41f},   /* Cassette */
    {ENGINE_PLUCK, 0, 0, 6.0f, 0.0f, 0.5f, 1.0f, 1.0f, 0.4f, 0.2f, 0.0f, 0.95f, 0.997f, 0.001f, 0.3f, 1.0f, 0.3f, 0.5f, 0.3f, 0.5f, 3.0f, 0.3f, 0.0f, 0.45f, 1.22f},   /* Clavecin lunaire */
    {ENGINE_FM, 0, 0, 6.0f, 0.0f, 0.5f, 2.0f, 0.9f, 1.0f, 0.4f, 0.0f, 0.6f, 0.996f, 0.01f, 1.0f, 0.6f, 0.7f, 0.5f, 0.3f, 0.6f, 5.0f, 0.3f, 0.0f, 0.45f, 0.68f},   /* Mellow */
    {ENGINE_FM, 0, 0, 6.0f, 0.0f, 0.5f, 3.5f, 2.4f, 0.5f, 0.05f, 0.0f, 0.6f, 0.996f, 0.001f, 1.5f, 0.0f, 0.9f, 0.5f, 0.3f, 0.3f, 2.0f, 0.3f, 0.0f, 0.45f, 1.05f},   /* Cloche */
    {ENGINE_FM, 0, 0, 6.0f, 0.0f, 0.5f, 5.4f, 1.2f, 0.08f, 0.0f, 0.0f, 0.6f, 0.996f, 0.001f, 0.5f, 0.0f, 0.3f, 0.3f, 0.3f, 0.25f, 1.0f, 0.3f, 0.0f, 0.45f, 2.1f},   /* Kalimba */
    {ENGINE_PLUCK, 0, 0, 6.0f, 0.0f, 0.5f, 1.0f, 1.0f, 0.4f, 0.2f, 0.0f, 0.7f, 0.996f, 0.001f, 0.3f, 1.0f, 0.25f, 0.3f, 0.3f, 0.3f, 2.0f, 0.3f, 0.0f, 0.45f, 1.46f},   /* Guitare sèche */
    {ENGINE_PLUCK, 0, 0, 6.0f, 0.0f, 0.5f, 1.0f, 1.0f, 0.4f, 0.2f, 0.0f, 0.45f, 0.998f, 0.001f, 0.3f, 1.0f, 0.5f, 0.3f, 0.3f, 0.5f, 2.0f, 0.3f, 0.0f, 0.45f, 1.32f},   /* Harpe */
    {ENGINE_PLUCK, 0, 0, 6.0f, 0.0f, 0.5f, 1.0f, 1.0f, 0.4f, 0.2f, 0.0f, 0.95f, 0.992f, 0.001f, 0.3f, 1.0f, 0.2f, 0.3f, 0.3f, 0.2f, 6.0f, 4.0f, 0.0f, 0.45f, 1.64f},   /* Koto */
    {ENGINE_FM, 0, 0, 6.0f, 0.0f, 0.5f, 4.0f, 0.9f, 0.05f, 0.0f, 0.0f, 0.6f, 0.996f, 0.001f, 0.35f, 0.0f, 0.25f, 0.2f, 0.3f, 0.2f, 0.0f, 0.3f, 0.0f, 0.45f, 2.7f},   /* Marimba */
    {ENGINE_SUB, 0, 1, 10.0f, 0.0f, 0.5f, 1.0f, 1.0f, 0.4f, 0.2f, 0.0f, 0.6f, 0.996f, 0.001f, 0.25f, 0.0f, 0.15f, 3.0f, 0.12f, 0.4f, 2.0f, 0.3f, 0.0f, 0.45f, 3.71f},   /* Stab house */
    {ENGINE_FM, 0, 0, 6.0f, 0.0f, 0.5f, 7.0f, 1.5f, 0.04f, 0.0f, 0.0f, 0.6f, 0.996f, 0.001f, 0.2f, 0.0f, 0.2f, 0.2f, 0.3f, 0.5f, 3.0f, 0.3f, 0.0f, 0.45f, 3.25f},   /* Gouttes */
    {ENGINE_PLUCK, 0, 0, 6.0f, 0.0f, 0.5f, 1.0f, 1.0f, 0.4f, 0.2f, 0.0f, 0.9f, 0.9985f, 0.001f, 0.3f, 1.0f, 0.6f, 0.3f, 0.3f, 0.6f, 15.0f, 5.0f, 0.0f, 0.45f, 1.1f},   /* Sitar fantôme */
    {ENGINE_SUB, 2, 2, 12.0f, 0.0f, 0.5f, 1.0f, 1.0f, 0.4f, 0.2f, 0.0f, 0.6f, 0.996f, 0.3f, 0.3f, 1.0f, 1.5f, 0.3f, 0.3f, 0.7f, 35.0f, 0.4f, 0.08f, 0.45f, 0.98f},   /* Bande usée */
    {ENGINE_SUB, 3, 2, 6.0f, 12.0f, 0.5f, 1.0f, 1.0f, 0.4f, 0.2f, 0.0f, 0.6f, 0.996f, 0.4f, 0.3f, 1.0f, 1.8f, 0.2f, 0.3f, 1.0f, 25.0f, 0.12f, 0.0f, 0.45f, 0.75f},   /* Sous l'eau */
    {ENGINE_FM, 0, 0, 6.0f, 0.0f, 0.5f, 1.41f, 2.2f, 1.5f, 0.25f, 0.0f, 0.6f, 0.996f, 0.01f, 2.0f, 0.3f, 1.2f, 0.3f, 0.3f, 0.5f, 3.0f, 0.3f, 0.0f, 0.45f, 0.89f},   /* Métal doux */
    {ENGINE_PLUCK, 0, 0, 6.0f, 0.0f, 0.5f, 1.0f, 1.0f, 0.4f, 0.2f, 0.0f, 1.0f, 0.985f, 0.001f, 0.3f, 1.0f, 0.2f, 0.3f, 0.3f, 0.8f, 4.0f, 0.3f, 0.0f, 0.45f, 1.52f},   /* Cristaux */
    {ENGINE_FM, 0, 0, 6.0f, 0.0f, 0.5f, 2.0f, 0.6f, 1.0f, 0.5f, 0.0f, 0.6f, 0.996f, 0.12f, 0.3f, 1.0f, 0.8f, 0.4f, 0.3f, 0.6f, 6.0f, 4.5f, 0.025f, 0.45f, 0.56f},   /* Flûte de verre */
    {ENGINE_PLUCK, 0, 0, 6.0f, 0.0f, 0.5f, 1.0f, 1.0f, 0.4f, 0.2f, 0.0f, 0.6f, 0.998f, 0.001f, 0.3f, 1.0f, 0.6f, 0.3f, 0.3f, 0.7f, 28.0f, 0.3f, 0.0f, 0.45f, 1.22f},   /* Harpe tordue */
    {ENGINE_SUB, 1, 3, 5.0f, -12.0f, 0.6f, 1.0f, 1.0f, 0.4f, 0.2f, 0.0f, 0.6f, 0.996f, 0.2f, 0.3f, 1.0f, 1.4f, 0.2f, 0.3f, 0.9f, 18.0f, 0.18f, 0.0f, 0.45f, 0.53f},   /* Orgue noyé */
};
#define NRHYTHM 16
static const char *const RHYTHM_NAME[NRHYTHM] = {"TENU", "NOIRES", "CROCHES", "DOUBLES", "CONTRETEMPS", "POMPE", "TRESILLO", "TRANCE GATE", "2-STEP", "GARAGE STAB", "SYNCOPE", "SHUFFLE", "BOSSA", "STUTTER", "DEMI-TEMPS", "FLOTTANT"};
/* 16 steps: X accent, x soft, - tie, . rest ("" = held) */
static const struct { const char *s; float gate, swing, atk; } RHYTHM_DEF[NRHYTHM] = {
    {"", 0.0f, 0.0f, 0.0f},
    {"X---x---X---x---", 0.85f, 0.0f, 0.0f},
    {"X-x-X-x-X-x-X-x-", 0.6f, 0.0f, 0.0f},
    {"XxxxXxxxXxxxXxxx", 0.3f, 0.0f, 0.0f},
    {"..X...X...X...X.", 0.55f, 0.0f, 0.0f},
    {"X---X---X---X---", 0.95f, 0.0f, 0.09f},
    {"X--x--x-X--x--x-", 0.7f, 0.0f, 0.0f},
    {"X.XX.XX.X.XX.XX.", 0.55f, 0.0f, 0.0f},
    {"X..x...x..X..x..", 0.45f, 0.12f, 0.0f},
    {"..X..x....X..x.x", 0.35f, 0.15f, 0.0f},
    {"X..x..x...x..x..", 0.4f, 0.0f, 0.0f},
    {"X.xxX.xxX.xxX.x.", 0.45f, 0.2f, 0.0f},
    {"X..x..x...x.x...", 0.55f, 0.0f, 0.0f},
    {"XxXx....X...xxxx", 0.4f, 0.0f, 0.0f},
    {"X-------x-----x-", 0.9f, 0.0f, 0.0f},
    {"X-----x-----x---", 0.8f, 0.0f, 0.0f},
};
#define NGRID 38
/* chords: semitones above the tonic + quality (M maj, V dominant-major, m, D dom7, d dim, s sus4, S sus2, a aug); 8 white keys then 5 black */
static const struct { const char *name; uint8_t rich; const char *white, *black; } GRID_DEF[NGRID] = {
    {"POP LUMINEUX", 0, "0M 2m 4m 5M 7V 9m 7s 5S", "10M 5m 8M 4D 2D"},
    {"BALLADE", 1, "0M 9m 5M 7V 4m 2m 7s 0S", "10M 8M 4D 5m 3M"},
    {"INDIE PSYCHE", 1, "0M 7m 5M 5m 9m 4m 2m 10M", "8M 3M 2V 6d 1M"},
    {"MINEUR EUPHORIQUE", 1, "0m 8M 3M 10V 5m 7m 10s 0S", "7V 2d 1M 5M 8m"},
    {"PSYCHE REVEUR", 1, "0M 4m 5M 5m 9m 2D 7V 10M", "8M 3M 1M 6d 11m"},
    {"DORIEN CHAUD", 1, "0m 5V 3M 10M 2m 7m 9d 0s", "8M 7M 1M 5D 11M"},
    {"MIXOLYDIEN", 0, "0V 10M 5M 7m 2m 9m 4d 0s", "3M 8M 5m 7M 2D"},
    {"ANDALOUSE", 0, "0m 10V 8M 7V 5m 3M 2d 7s", "1M 7D 0M 6d 11d"},
    {"MELANCOLIE", 1, "0m 8M 3M 10V 5m 7m 10s 0s", "7V 1M 5V 8m 4M"},
    {"HOUSE PIANO", 1, "9m 5M 0M 7V 2m 4m 7s 0S", "10M 4D 8M 3M 5m"},
    {"DEEP HOUSE", 2, "0m 3M 10m 5m 8M 7m 2m 10V", "1M 6M 11M 4M 9M"},
    {"LYDIEN FLOTTANT", 1, "0M 2M 7M 4m 11m 9m 6d 0S", "5M 10M 8M 3M 1M"},
    {"SOMBRE", 1, "0m 1M 3M 5m 7d 8M 10m 0s", "6M 11M 4M 9M 2M"},
    {"SPLEEN ANGLAIS", 1, "0m 8M 3M 4M 9m 5M 10M 7s", "1M 6m 11M 2M 7V"},
    {"DISCO", 1, "2m 7D 0M 9m 5M 4D 0S 7s", "10M 3M 8M 1D 6d"},
    {"CIEL ETRANGE", 1, "0M 4M 9m 5m 0S 7s 2m 8M", "3M 10M 6M 1M 11m"},
    {"HYMNE", 0, "0M 7V 9m 4m 5M 2m 7s 2V", "10M 8M 5m 3M 4M"},
    {"NUIT", 2, "0m 3M 5m 7m 8M 10V 7s 0S", "11M 1M 6M 9M 4M"},
    {"REVERIE INDIE", 1, "0M 4V 9m 2V 5M 5m 7s 10M", "3M 8M 6d 11m 1M"},
    {"CHROMATIQUE", 1, "0M 4M 8M 3M 9M 5M 1M 7M", "10M 6M 11M 2M 0m"},
    {"MAJEUR-MINEUR", 1, "0M 0m 5M 5m 7V 3M 8M 10M", "1M 9m 2m 4m 6d"},
    {"SOLEIL", 0, "0M 5M 9m 7V 2m 0S 5S 7s", "10M 3M 8M 4D 6d"},
    {"BOSSA", 2, "2m 7D 0M 9D 5M 4m 9m 7s", "3d 8D 10D 11d 0m"},
    {"TRAP MELODIQUE", 0, "0m 8M 10V 3M 5m 7m 2d 0S", "1M 7M 11d 6M 9d"},
    {"EUPHORIE TRANCE", 0, "0m 8M 3M 10V 5m 0s 8S 10s", "7V 2d 1M 6M 11M"},
    {"FUNK", 1, "0D 5D 7D 10D 3D 2m 0s 9m", "1D 6D 8D 11D 4D"},
    {"BLUES", 1, "0D 5D 7D 9m 2m 4m 10M 0s", "3M 8M 6d 1D 11d"},
    {"CINEMA", 1, "0m 8M 5m 3M 10V 7V 1M 0s", "4M 6m 9M 11d 2d"},
    {"AMBIENT", 3, "0M 2m 4m 5M 7s 9m 0S 5S", "10M 3M 8M 6m 1M"},
    {"SHOEGAZE", 1, "0M 5M 9m 4V 2V 7V 10M 0S", "3M 8M 1M 6M 11m"},
    {"REVE 80S", 1, "0m 10V 8M 10s 5m 3M 7m 0S", "1M 6M 11M 2d 4M"},
    {"ROMANTIQUE", 1, "0M 4m 9m 2m 5M 7V 9D 4D", "8M 10M 3M 6d 1M"},
    {"PLANANT MINEUR", 2, "0m 3M 7m 10V 5m 2m 8M 0S", "1M 6M 4M 11M 9M"},
    {"RETRO JEU VIDEO", 0, "0M 7V 9m 5M 2m 4m 10M 8M", "3M 1M 6d 11m 0m"},
    {"TENSION", 1, "0m 1M 6d 11d 3M 8M 7D 0s", "4M 9M 2d 5m 10m"},
    {"COUCHER DE SOLEIL", 2, "0M 5M 7m 10M 2m 9m 4m 0S", "3M 8M 1M 6d 11M"},
    {"UK GARAGE", 2, "2m 0M 4m 9m 5M 7D 7s 0S", "1M 3M 6d 8M 10D"},
    {"RAVE 90S", 0, "0m 3M 10V 8M 5m 7V 0s 10s", "1M 6M 11M 2M 4M"},
};
