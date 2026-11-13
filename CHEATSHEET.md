# Blop cheat sheet: every control

## FM-1: knobs (encoders)

| Control | What it does |
| --- | --- |
| **SELECT** | Chord grid (38). Used when SCALE = GRID |
| **ALGORITHM** | Rhythm (16). OFF: the chord sounds while held. A rhythm: the chord keeps playing (latch); the same key again stops it |
| **PRESET** | Sound (48, in 6 families: PADS, KEYS, PLUCKS, EXPERIMENTAL, **ANALOG**: PWM STRINGS, SOFT BRASS, LADDER PAD, LADDER SWEEP, SUPER SAW, WARM POLY, DARK DRONE, DREAM PAD; **DARK WAVE**: COLD STRINGS, BLACK CHOIR, ICE PAD, FARFISA, SCREAM PAD, NIGHT DRIFT, GHOST ORGAN, STATIC WALL) |
| **K1 – K4** | The 4 settings of the page on screen (shown at the bottom) |
| **MASTER** | Volume |

## FM-1: pages (press again = back to MAIN)

| Button | Page | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- | --- |
| **HOME** | MAIN (synth) | Sound (same as PRESET) | Cutoff | Glide | Length (chord note length) |
| **HOME + ENV** | GROOVE 2 | Strum (0 = all notes together) | free | free | free |
| **FX** (synth only) | FX | TAPE ECHO (synced, more = more repeats) | PHASER | JUNO (I → II → I+II) | CLOUD (hall, 3–10 s) |
| **SCL** | CHORDS | Key | Voicing | Richness | **Scale** (GRID, MAJOR, MINOR, DORIAN, MIXOLYDIAN, HARM MINOR, LYDIAN, LOCRIAN) |
| **ENV** | GROOVE | BPM | Swing | Density (0 = centre, as written; below 0 drops hits at random; above adds ghost hits; stops on 0, says "CENTRED") | Sidechain on/off |
| **LFO** | BASS | Sound (21: HOOK BASS, PICKED, DARK PULSE and SUB, ROUND, ACID, REESE, PLUCK, FM, MOOG, FUNK, JUNO SUB, 808 SUB, HOUSE PLUCK, ACID SQUARE, ACID 24, DX BASS, FM TECHNO, REESE DEEP, WOBBLE) | Level | Riff (29 patterns, incl. 5 post-punk: POST PUNK, HOOK A, HOOK B, LEGATO 8THS, GOTH RUN; 16 by style: DISCO, HOUSE OFFBEAT, TECHNO ROLL, REGGAE, DUB, FUNK, SLAP, WALKING, ACID A/B, TRANCE, 808 TRAP, SYNTHWAVE, BOSSA, DNB) | Filter |
| **GLO** | DRUMS | Pattern (40) | Kick | **Snare** | Hi-hat |
| **HOME + GLO** | DRUMS 2 | Rim | Clap | Tom | Cowbell |
| **EDIT** | DECOR | Creature | Eyes | Backdrop | Extra |
| **SEQ** | SEQ (looper) | Length (1 beat … 4 bars) | Transpose (±12 st) | Order (forward, backward, ping-pong, random) | Sound (preset) |
| **ARP** | MIXER | SYNTH (chords) | BASS | DRUMS (master of all drum voices) | MASTER |

## FM-1: buttons and shortcuts

| Press | What it does |
| --- | --- |
| **27 keys** | Chords (GRID: the grid's chords; a scale: the key is the root) |
| **OCT− / OCT+** | Octave −2 … +2 (lit / blinking) |
| **HOME** (short) | Back to the MAIN page |
| **HOME** (hold) | Help screen |
| **SAVE** (alone) | The creature in its bowl, the music heard from outside (muffled) |
| **REC** | Record a loop: REC, play chords, REC again (REC blinks while recording) |
| **PLAY** | Play / stop the loop (green while playing). No loop: stop / bring back the chord |
| **HOME + SEQ / PLAY / ARP / SAVE** | A pirouette / the picture only / feed the creature / the lamp |
| **Hold FX + SCL**, then **ARP / SEQ / PLAY / REC** | The creature tricks on their own buttons: lamp / pirouette / picture only / feed |
| **HOME + OCT+** | CPU meter: load, peak since last time, dropouts (XRUNS) |
| **HOME + SCL** | Scale on/off: GRID ↔ your last scale |
| **HOME + LFO** | MIDI MAP: STANDARD → KEYSTEP 37 → OFF |
| **HOME + ENV** | GROOVE 2 page (again = back to GROOVE) |
| **HOME + EDIT** | MIDI channel: ALL → 1 → … → 16 |
| **HOME + REC** (hold 3 s) | Reset to the defaults (countdown; let go = cancel) |
| **SAVE + hold a white key** (C4–G5, 1.5 s) | Store the setup and its loop in song 1–12 |
| **SAVE + tap a white key** (C4–G5) | Load song 1–12 (then PLAY for its loop) |

Settings save themselves 4 s after the last change, when no chord is playing.

## Arturia KeyStep 37 (MIDI MAP = KEYSTEP 37)

| KeyStep | Blop |
| --- | --- |
| **Keys** | Chords, one octave higher than the FM-1's keys (optional my_setup.h). Light presses work; CHORD mode: the lowest note counts |
| KeyStep **OCT− / OCT+** | One octave lower / higher (below C3 / above C6) |
| **TYPE** knob (CC 74) | Cutoff |
| **NOTE** knob (CC 71) | Kick (switches the drums on if they are off) |
| **VEL** knob (CC 76) | Hi-hat |
| **4th** knob (CC 77) | Rim (DRUMS 2 K1) |
| **Mod strip** (CC 1) | Bass sound (OFF, SUB, ROUND, ACID, REESE, PLUCK, FM) |
| RATE, DIV | Nothing (they send no MIDI) |

## MIDI CC chart (STANDARD and KEYSTEP 37; any controller)

**4 knobs for everything:** CC **16, 17, 18, 19** = K1–K4 of the page on screen. **CC 15** picks the page
(0–12 MAIN, 13–25 FX, 26–38 CHORDS, 39–51 GROOVE, 52–64 BASS, 65–76 DECOR, 77–89 DRUMS, 90–102 SEQ, 103–115 MIXER, 116–127 DRUMS 2).

| Page | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| MAIN | 87 Sound | 20 Cutoff | 21 Glide | 22 Length |
| GROOVE 2 | 23 Strum | | | |
| FX | 24 Delay | 25 Tremolo | 26 Chorus | 27 Reverb |
| CHORDS | 28 Key | 29 Voicing | 30 Richness | 31 Scale |
| GROOVE | 102 BPM | 103 Swing | 104 Density | 105 Sidechain |
| BASS | 106 Sound | 107 Level | 108 Riff | 109 Filter |
| DRUMS | 110 Pattern | 111 Kick | 52 Snare | 112 Hi-hat |
| DRUMS 2 | 113 Rim | 53 Clap | 54 Tom | 55 Cowbell |
| DECOR | 114 Creature | 115 Eyes | 116 Backdrop | 117 Extra |

| Other | CC |
| --- | --- |
| Chord grid | 85 |
| Rhythm | 86 |
| Sound (preset) | 87 |
| Scale (same as 31) | 89 |
| Octave | 90 |
| Strum | 118 |
| Synth level / Drums level / Master (mixer) | 119 / 12 / 7 |
| Snare (DRUMS) / Clap / Tom / Cowbell (DRUMS 2) | 52 / 53 / 54 / 55 |
| Cutoff / Reverb / Chorus (usual numbers) | 74 / 91 / 93 |
| All Notes Off | 123 or 120 |

0–127 covers the whole range. A setting on another page shows its name and value on the screen.

## MIDI in general

- **MIDI IN jack (1/8"):** notes, CCs, clock. Channel: ALL or the one chosen (HOME + ENV).
- **USB in:** clock only (Ableton's tempo leads: GROOVE shows "SYNC"). USB notes are ignored on purpose.
- **USB out:** the 4 chord notes and the bass, channel 1 (to record in Ableton). Drums are not sent.
- **MIDI clock** from the jack or USB: Blop follows its tempo and beats; when it stops, Blop keeps the last tempo.

## Defaults

MIDI MAP = STANDARD, scale = GRID, key = C. An optional `my_setup.h` sets your own (MIDI map, scale, key, the jack's octave).
