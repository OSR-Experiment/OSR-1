# What changed since Blop 1.1.0

## Later additions (v10 to v33)

Every control is in [CHEATSHEET.md](CHEATSHEET.md), a visual map is in [BLOP-MAP.html](BLOP-MAP.html), and the sounds, drums and
bass with their sources are in [SOUNDS-AND-DRUMS.md](SOUNDS-AND-DRUMS.md).

- **Pages:** HOME is the synth page (SOUND, CUTOFF, GLIDE, LENGTH). New pages: MIXER (ARP: synth, bass, drums, master),
  SEQ (looper: length up to 4 bars, transpose, order, sound), DRUMS (pattern, kick, snare, hi-hat), DRUMS 2 (HOME + GLO: rim, clap, tom,
  cowbell), GROOVE 2 (HOME + ENV: strum). MIDI channel moved to HOME + EDIT.
- **Content:** 48 sounds (new ANALOG and DARK WAVE families), 21 bass sounds with 29 patterns, 40 drum patterns, 4 more drum voices.
- **Scales:** LYDIAN and LOCRIAN added (8 in all). **Song shown:** the song loaded or stored last shows at the bottom of the screen. **Effects (echo, phaser, Juno, cloud) apply to the synth only;** the bass and drums stay dry.
- **Looper** (REC / PLAY), 12 songs with on-screen banners, a scale selector, a CPU meter (HOME + OCT+).
- **Fixes:** the short dip when changing chord with GLIDE off; DENSITY could not return to 0; saves from older builds are migrated.
- Measured on a real FM-1: worst case with heavy pad, bass, drums and all effects at maximum is about 75% CPU, no dropouts.


Tested in the host simulator (`tests/basic.txt`, `tests/midi_in.txt`, run by the workflow) and on a real FM-1 (a newer
unit, UC25HQ80 flash) with a MIDI keyboard on the MIDI IN jack.

## Blop 1.1.0 and now, side by side

| | Blop 1.1.0 (original) | Now |
| --- | --- | --- |
| **MIDI IN jack (1/8")** | Ignored: every message dropped | Notes play the chords; CCs change the settings; clock followed |
| **Newer FM-1s (UC25HQ80 flash)** | Refused updates ("stopped after 0 requests") and lost their settings at power-off | Update directly, settings saved (fm1-base 4587fda → 317cb86) |
| **A keyboard's octaves** | — | C3–C6 at the FM-1's pitch, beyond: an octave lower / higher |
| **A keyboard's chord mode** | — | Counts as one note (the lowest); latch and press-again-to-stop keep working |
| **Light, short presses** | — | Always play |
| **MIDI controllers (CC)** | — | Every setting, with a chart (MIDI.md); 4 knobs reach all (CC 16–19 = the page's K1–K4) |
| **MIDI MAP** | — | HOME + LFO: STANDARD (default) / OFF |
| **MIDI CHANNEL** | — | HOME + ENV: ALL (default) / 1–16 |
| **Chords** | The grids only: each key is a slot in a progression | ★ **SCALE** (CHORDS page K4): GRID, or MAJOR / MINOR / DORIAN / MIXOLYDIAN / HARM MINOR, where the key you press is the root. HOME + SCL toggles |
| **STRUM** | CHORDS page K4 | CC 118 (SCALE took the knob; the value is kept) |
| **Memories** | One setup (saved by itself) | ★ **8 memories**: SAVE + hold a white key 1.5 s = store, SAVE + tap = recall |
| **SAVE button** | The bowl, on press | The bowl, on release (unless a memory was used) |
| **Own defaults** | — | Optional `my_setup.h`: MIDI map, scale, key, the jack's octave |
| **Building on a Mac** | — | `tools/build_mac.sh` (Docker) |

Unchanged: the FM-1's own keys and controls (except K4 of the CHORDS page), the sounds, rhythms, bass, drums, effects,
the screen and the creature; USB clock in and MIDI out.

## Details

- **MIDI IN.** `app_tick` dropped everything not from USB (`if (port != PORT_USB) continue;`). The jack's notes and
  CCs now go through two small rings to the main loop (the tick is an interrupt); notes play as key presses.
- **Newer FM-1s.** On units with the UC25HQ80 flash, fm1-base before e75b072 (2026-11-04) left `flash_ok` at 0: saves
  stayed in RAM and the update entry ignored the upgrade command. The submodule is now at 317cb86.
- **Notes that come together** wait 8 ms for each other; a release plays a note still waiting (a short press is never
  lost).
- **Settings:** `root` (the scale), `midimap` and `midich` use the settings struct's three spare bytes, so settings
  saved by 1.1.0 stay valid. The memories are a new store object (`OBJ_SLOTS`, at 0xDA000).
- **Tests:** `tests/midi_in.txt` covers notes, octaves, chord mode, short presses, latch, CCs, presets, channel,
  scales, strum and memories.
