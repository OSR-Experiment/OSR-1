# Blop: MIDI in from a keyboard

Plug a MIDI keyboard into the FM-1's 1/8" (3.5 mm) **MIDI IN** jack (TRS). Blop then plays from it as from its own keys,
and a controller's knobs can change its settings. Every control on one page: [CHEATSHEET.md](CHEATSHEET.md).

## Notes

- A note plays the chord of the FM-1 key with the same name (C4 = the FM-1's C4 key).
- Octaves: a keyboard's three middle octaves (**C3 to C6**, a 37-key keyboard as it comes) sound where the FM-1's keys
  do; an octave beyond them (the keyboard's own octave buttons) plays an octave lower or higher.
- With a rhythm on (ALGORITHM), a chord keeps playing (latch); the same key again stops it, as on the FM-1.
- Several notes at once (a keyboard's chord mode) count as one: the lowest. Light, short presses always play.
- Any channel by default (see MIDI CHANNEL). Notes from USB are not played: they could be Blop's own coming back
  from the computer.

## SCALE (CHORDS page, K4; HOME + SCL)

**GRID** (default): Blop's chord grids, as always. **MAJOR, MINOR, DORIAN, MIXOLYDIAN, HARM MINOR**: the key you press is
the chord's root, and the chord is the one the scale gives on that degree of the song key (KEY, CHORDS page K1).
HOME + SCL switches between GRID and the last scale. On the FM-1's keys and from MIDI alike.

In KEY = C:

| You press | C | D | Eb | E | F | G | Ab | A | Bb | B |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| MAJOR | **C** | Dm | Eb | Em | **F** | **G** (G7) | Ab | Am | Bb | Bdim |
| MINOR | **Cm** | Ddim | **Eb** | E | Fm | Gm | **Ab** | Adim | **Bb** | Bdim |
| DORIAN | **Cm** | Dm | **Eb** | E | **F** | Gm | Ab | Adim | **Bb** | Bdim |
| MIXOLYDIAN | **C** | Dm | Eb | Edim | **F** | Gm | Ab | Am | **Bb** | Bdim |
| HARM MINOR | **Cm** | Ddim | Eb+ | E | Fm | **G** (G7) | **Ab** | Adim | Bb | Bdim |

Other keys move the same way: F# minor = KEY F#, SCALE MINOR. RICHNESS adds 7ths and 9ths as usual.

## MIDI MAP (HOME + LFO) and MIDI CHANNEL (HOME + ENV)

- **MIDI MAP:** **STANDARD** (default), the chart below; **OFF**, notes only (controllers ignored).
- **MIDI CHANNEL:** **ALL** (default), or 1 to 16.

Both are shown on the screen and saved.

## The CC chart

STANDARD keeps to the numbers MIDI leaves free, plus three that mean the same on most synths (74 cutoff, 91 reverb,
93 chorus): a controller changes nothing by surprise. Set your controller's knobs to these numbers (most have an editor
app for it).

**Four knobs for everything:** set four knobs to **CC 16, 17, 18, 19**: they are K1-K4 of the page on the FM-1's
screen, so the page buttons choose what they change. **CC 15** picks the page
(0-18 MAIN, 19-36 FX, 37-54 CHORDS, 55-73 GROOVE, 74-91 BASS, 92-109 DECOR, 110-127 DRUMS).

**Or one number per setting** (0-127 spans the whole range; a setting on another page shows on the screen):

| Page | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| MAIN (HOME) | 20 Cutoff | 21 Glide | 22 Length | 23 Drums level |
| FX | 24 Delay | 25 Tremolo | 26 Chorus | 27 Reverb |
| CHORDS (SCL) | 28 Key | 29 Voicing | 30 Richness | 31 Scale |
| GROOVE (ENV) | 102 BPM | 103 Swing | 104 Density | 105 Sidechain |
| BASS (LFO) | 106 Sound | 107 Level | 108 Riff | 109 Filter |
| DRUMS (GLO) | 110 Pattern | 111 Kick | 112 Hi-hat | 113 Rim |
| DECOR (EDIT) | 114 Creature | 115 Eyes | 116 Backdrop | 117 Extra |

| Other | CC |
| --- | --- |
| Chord grid (SELECT) | 85 |
| Rhythm (ALGORITHM) | 86 |
| Sound (PRESET) | 87 |
| Scale (as 31) | 89 |
| Octave (-2 ... +2) | 90 |
| Strum (it left the CHORDS page for SCALE) | 118 |
| Cutoff / Reverb / Chorus (the usual numbers) | 74 / 91 / 93 |
| All Notes Off | 123 (or 120) |

Turning a drum up (111, 112, 113) while the drums are off switches them on. MIDI clock from the jack is followed like
USB's (the tempo and the beats).

## Your own defaults: my_setup.h

A `my_setup.h` beside `app.c` (not in the repository) changes the first start and the reset (HOME + REC):

```c
#define BLOP_DEFAULT_MIDIMAP MAP_STANDARD   /* or MAP_OFF */
#define BLOP_DEFAULT_SCALE 0                /* 0 GRID, 1 MAJOR, 2 MINOR, 3 DORIAN, 4 MIXOLYDIAN, 5 HARM MINOR */
#define BLOP_DEFAULT_KEY 0                  /* 0 C .. 11 B */
#define BLOP_MIDI_OCTAVE 0                  /* -2 .. 2: octaves added to the notes from the jack */
```

A preset for a controller's own knob numbers is a name in `MAP_NAME` and a few lines in `CC_MAP` (app.c).

## Building on a Mac

`tools/build_mac.sh` builds the firmware in a Linux container (Docker Desktop running; the JieLi toolchain and the
AC79 SDK in `~/.jieli`, see the workflow). `tools/build_mac.sh NAME` also keeps the result in `versions/NAME/`.
