# Blop: MIDI in from a keyboard

Plug a MIDI keyboard into the FM-1's 1/8" (3.5 mm) **MIDI IN** jack (TRS). Blop then plays from it as from its own keys,
and a controller's knobs can change its settings.

## Notes

- A note plays the chord of the FM-1 key with the same name, as the FM-1's own keys do (C4 = the FM-1's C4 key).
- The keyboard's octaves are real: lower notes play the chord lower, higher notes higher.
- With a rhythm on (ALGORITHM), a chord keeps playing (latch); the same key again stops it, as on the FM-1.
- Several notes at once (a keyboard's chord mode) count as one: the lowest.
- Any channel by default (see MIDI CHANNEL). Notes from USB are not played: they could be Blop's own coming back
  from the computer.

## ROOT mode (HOME + SCL)

Off by default. On: the key you press is the chord's root, and the chord fits the song key (KEY on the CHORDS page).
In C: C, Dm, Em, F, G (G7 with RICHNESS 7TH), Am, Bdim; the black keys give borrowed chords (C#, Eb, F#dim, Ab, Bb).
On the FM-1's keys and from MIDI alike. CC 89 switches it too (0-63 off, 64-127 on).

## MIDI MAP (HOME + LFO)

Each press of LFO while HOME is held goes to the next preset, shown on the screen and saved:

| Preset | What a controller's knobs do |
| --- | --- |
| **STANDARD** (default) | The chart below. Numbers MIDI leaves free, plus three that mean the same everywhere (74 cutoff, 91 reverb, 93 chorus): nothing changes by surprise |
| **KEYSTEP 37** | The chart, and an Arturia KeyStep 37's knobs as it comes: knob 1 (CC 74) cutoff, knob 2 (CC 71) kick, knob 3 (CC 76) hi-hat, knob 4 (CC 77) rim; the mod strip (CC 1) the bass sound. Turning a drum up while the drums are off switches them on |
| **OFF** | Notes only: controllers are ignored |

A preset for another controller is one name and a few lines in `app.c` (`MAP_NAME`, `CC_MAP`): contributions welcome.

## MIDI CHANNEL (HOME + ENV)

Each press goes to the next: ALL (default), 1, 2 ... 16. Shown on the screen and saved.

## The chart (STANDARD and KEYSTEP 37)

**Four knobs for everything:** set four knobs to **CC 16, 17, 18, 19**: they are K1-K4 of the page on the FM-1's
screen, so the page buttons (FX, SCL, ENV, LFO, EDIT, GLO, HOME) choose what they change. **CC 15** picks the page
(0-18 MAIN, 19-36 FX, 37-54 CHORDS, 55-73 GROOVE, 74-91 BASS, 92-109 DECOR, 110-127 DRUMS).

**Or one number per setting** (0-127 spans the whole range; a setting on another page shows on the screen):

| Page | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| MAIN (HOME) | 20 Cutoff | 21 Glide | 22 Length | 23 Drums level |
| FX | 24 Delay | 25 Tremolo | 26 Chorus | 27 Reverb |
| CHORDS (SCL) | 28 Key | 29 Voicing | 30 Richness | 31 Strum |
| GROOVE (ENV) | 102 BPM | 103 Swing | 104 Density | 105 Sidechain |
| BASS (LFO) | 106 Sound | 107 Level | 108 Riff | 109 Filter |
| DRUMS (GLO) | 110 Pattern | 111 Kick | 112 Hi-hat | 113 Rim |
| DECOR (EDIT) | 114 Creature | 115 Eyes | 116 Backdrop | 117 Extra |

| Other | CC |
| --- | --- |
| Chord grid (SELECT) | 85 |
| Rhythm (ALGORITHM) | 86 |
| Sound (PRESET) | 87 |
| ROOT mode | 89 |
| Octave (-2 ... +2) | 90 |
| Cutoff / Reverb / Chorus (the usual numbers) | 74 / 91 / 93 |
| All Notes Off | 123 (or 120) |

MIDI clock from the jack is followed like USB's (the tempo and the beats).

## Building on a Mac

`tools/build_mac.sh` builds the firmware in a Linux container (Docker Desktop running; the JieLi toolchain and the
AC79 SDK in `~/.jieli`, see the workflow). `tools/build_mac.sh NAME` also keeps the result in `versions/NAME/`.
