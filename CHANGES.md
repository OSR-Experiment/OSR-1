# Changes in this branch (MIDI in and controllers)

Based on Blop 1.1.0. On a real FM-1 (a newer unit, UC25HQ80 flash) with an Arturia KeyStep 37 on the MIDI IN jack:
notes from the jack and direct installs are confirmed; the rest is being tried there now. Everything is covered in the
host simulator (`tests/midi_in.txt`, run by the workflow).

## Fixes

- **MIDI IN jack did nothing.** Blop dropped every message from the TRS jack (`if (port != PORT_USB) continue;`) and
  had no note input at all. Notes from the jack now play the chords like the FM-1's keys.
- **Newer FM-1s (UC25HQ80 flash) refused updates and lost their settings.** fm1-base was pinned at 4587fda
  (2026-10-28), before fm1-base learned that part (e75b072, 2026-11-04): on those units `flash_ok` was 0, so saves stayed
  in RAM and the install entry ignored the upgrade command ("stopped after 0 requests"; the only way out was back to
  M-VAVE's firmware first). fm1-base is now 317cb86: updates install directly and settings are saved.
- **A keyboard's octaves all sounded the same:** notes outside F3..G5 now play the chord in their own octave.
- **A keyboard's chord mode** (several notes at once) counts as one note, the lowest; latch / press-again-to-stop works
  with it.

## New

- MIDI CC control of every setting (MIDI.md has the chart), with **MIDI MAP** presets (HOME + LFO): STANDARD
  (default), KEYSTEP 37, OFF.
- **MIDI CHANNEL** (HOME + ENV): ALL (default) or 1-16.
- **ROOT mode** (HOME + SCL, or CC 89): the key pressed is the chord's root, the chord fits the song key. Off by default.
- MIDI clock from the jack is followed like USB's.
- The settings use the struct's three spare bytes (root, midimap, midich): saved settings stay valid, no reset.
- `tools/build_mac.sh`: the firmware built on a Mac in a Docker container (the toolchain is Linux x86-64 only).
- `tests/midi_in.txt`: notes, octaves, chord mode, short presses, latch, CCs, presets, channel, ROOT mode.

Unchanged: the FM-1's own keys, buttons, sounds, screen; USB MIDI in (clock) and out; notes from USB are still not
played.
