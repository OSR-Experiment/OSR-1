# Blop cheat sheet: every control

★ = new in this version (not in Blop 1.1.0).

## Knobs (encoders)

| Control | What it does |
| --- | --- |
| **SELECT** | Chord grid (38). Used when SCALE = GRID |
| **ALGORITHM** | Rhythm (16). OFF: the chord sounds while held. A rhythm: the chord keeps playing (latch); the same key again stops it |
| **PRESET** | Sound (32: PADS, KEYS, PLUCKS, EXPERIMENTAL) |
| **K1 – K4** | The 4 settings of the page on screen (shown at the bottom) |

## Pages (the same button again = back to MAIN)

| Button | Page | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- | --- |
| **HOME** | MAIN | Cutoff | Glide | Length | Drums level (0 = no drums) |
| **FX** | FX | Delay | Tremolo | Chorus | Reverb |
| **SCL** | CHORDS | Key | Voicing | Richness | ★ **Scale** (GRID, MAJOR, MINOR, DORIAN, MIXOLYDIAN, HARM MINOR) |
| **ENV** | GROOVE | BPM | Swing | Density | Sidechain on/off |
| **LFO** | BASS | Sound | Level | Riff | Filter |
| **GLO** | DRUMS | Pattern | Kick | Hi-hat | Rim |
| **EDIT** | DECOR | Creature | Eyes | Backdrop | Extra |

## Buttons and shortcuts

| Press | What it does |
| --- | --- |
| **27 keys** | Chords: GRID, the grid's chords; a scale, the key is the root |
| **OCT− / OCT+** | Octave −2 … +2 |
| **HOME** (short / held) | MAIN page / help screen |
| **SAVE** (alone) | The creature in its bowl, the music heard from outside |
| **ARP / SEQ / PLAY / REC** | Lamp / pirouette / the picture only / feed the creature |
| ★ **SAVE + hold a white key** (C4–C5) 1.5 s | Store the setup in memory 1–8 |
| ★ **SAVE + tap a white key** (C4–C5) | Recall memory 1–8 |
| ★ **HOME + SCL** | Scale: GRID ↔ the last scale |
| ★ **HOME + LFO** | MIDI MAP: STANDARD ↔ OFF |
| ★ **HOME + ENV** | MIDI channel: ALL → 1 → … → 16 |
| **HOME + REC** (hold 3 s) | Back to the first-start settings (countdown; let go = cancel). The memories are kept |

Memories: C4 = 1, D4 = 2, E4 = 3, F4 = 4, G4 = 5, A4 = 6, B4 = 7, C5 = 8. They survive power-off and updates, and
leave the MIDI map and channel as they are. The current setup saves itself 4 s after the last change, when no chord
plays.

## ★ MIDI IN (the 1/8" jack)

| From the keyboard | Blop |
| --- | --- |
| Notes | Chords, like the FM-1's keys; C3–C6 at the FM-1's pitch, beyond: an octave lower / higher |
| Several notes at once | One chord: the lowest note's |
| CC 16 / 17 / 18 / 19 | K1 / K2 / K3 / K4 of the page on screen |
| CC 15 | The page |
| CC 20 – 31, 102 – 117 | One setting each (chart below) |
| CC 85 / 86 / 87 | Chord grid / rhythm / sound |
| CC 89 / 90 / 118 | Scale / octave / strum |
| CC 74 / 91 / 93 | Cutoff / reverb / chorus |
| CC 123 | All notes off |
| MIDI clock | Tempo and beats |

| Page | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| MAIN | 20 | 21 | 22 | 23 |
| FX | 24 | 25 | 26 | 27 |
| CHORDS | 28 | 29 | 30 | 31 |
| GROOVE | 102 | 103 | 104 | 105 |
| BASS | 106 | 107 | 108 | 109 |
| DRUMS | 110 | 111 | 112 | 113 |
| DECOR | 114 | 115 | 116 | 117 |

## USB

- **In:** MIDI clock (Ableton's tempo leads: GROOVE shows "SYNC"). Notes from USB are not played.
- **Out:** the 4 chord notes and the bass, channel 1. The drums are not sent.

The details: [MIDI.md](MIDI.md). What changed since Blop 1.1.0: [CHANGES.md](CHANGES.md).
