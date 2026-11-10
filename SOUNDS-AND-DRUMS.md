# What I added to Blop's sounds, drums and bass (v23 to v28)

## The 8 new ANALOG pads (PRESET knob, the 5th family)

None of these is a copy of a factory preset. Each one is built from the *technique* that tutorials and articles describe
for that kind of hardware. Names are descriptive on purpose.

| # | Pad | How it is built | Technique it follows |
| --- | --- | --- | --- |
| 33 | **PWM STRINGS** | A pulse wave whose width a slow LFO moves (0.55 Hz), a saw, a sub-octave pulse, slow swell, chorus at full | Juno-style: one oscillator with PWM, a sub, and the chorus doing the width |
| 34 | **SOFT BRASS** | Two detuned saws plus a third, a filter envelope that opens fast (1.4 octaves) and settles | Poly-synth brass pad |
| 35 | **LADDER PAD** | Three saws (one an octave down) into the 24 dB ladder filter with resonance and soft saturation, slow swell, gentle filter sweep | Moog-style: three oscillators, ladder filter |
| 36 | **LADDER SWEEP** | The same, with the resonance up (3.2) and the cutoff sweeping slowly (0.9 octaves, 0.12 Hz) | Resonant ladder sweep |
| 37 | **SUPER SAW** | Three saws, wide detune (34 cents), chorus at full | Detuned-saw stack |
| 38 | **WARM POLY** | Saw plus triangle, soft filter envelope, the ladder for warmth | Warm analog poly |
| 39 | **DARK DRONE** | A slow pulse, a saw an octave down, a sub, a very long swell (1.4 s) and release (3.5 s) | Dark, slow pad |
| 40 | **DREAM PAD** | Triangle plus a sine an octave up and a detuned third, very slow swell, chorus at full | Soft ambient pad |

(Their numbers on the PRESET knob are 33 to 40; the 32 older sounds are unchanged, byte for byte.)

### The engine tools I added for them
- **PWM**: a pulse wave whose width swings around 50% (a new wave type, band-limited).
- **A third oscillator** per voice (level, octave and detune), for a Moog's three or a sub.
- **A 24 dB ladder-style filter**: two filters in a row, the resonance on the second, behind a soft saturation.
- **Resonance per sound**, and a **filter LFO** (a slow cutoff sweep) so a pad is never static.

### Where the ideas come from
- Juno-style pads: [MusicRadar, "How to create an '80s Juno-style pad sound"](https://musicradar.com/how-to/how-to-create-an-80s-juno-style-pad-sound),
  [Attack Magazine, "Overmono-style pads with Roland Cloud Juno-106"](https://www.attackmagazine.com/technique/tutorials/overmono-style-pads-with-roland-cloud-juno-106/),
  [Bonedo, "Sound-Workshop: das Pad aus Team von Lorde"](https://www.bonedo.de/artikel/sound-workshop-pad-lorde-team):
  saw and pulse with slow PWM, a sub, a filter envelope, a little LFO on the filter, very slow attack and release,
  chorus doing most of the width.
- Moog-style: [Wikipedia, Memorymoog](https://en.wikipedia.org/wiki/Memorymoog),
  [Perfect Circuit, history of the Memorymoog](https://www.perfectcircuit.com/signal/moog-memorymoog-history),
  [Reverb, a guide to synth filter types](https://applink.reverb.com/news/a-guide-to-synth-filter-types-ladders-steiner-parkers-and-more):
  three oscillators per voice, a 24 dB ladder low-pass that can be pushed to resonate.
- **Honest limits of the research:** the searches did not return specific Moog pad settings, and nothing on the
  Jupiter-8. The values in the table (times, detune amounts, resonance) are my own tuning of those general techniques,
  checked by measurement (loudness matched to SYRUP within about 1.5 dB, no clipping), not by ear.

## The new drum voices (DRUMS 2 page: HOME + GLO; MIDI CC 52 to 55)

| Voice | How it is made | Technique |
| --- | --- | --- |
| **SNARE** | Two sine tones (dropping a little in pitch) plus high-passed noise | 909-style: a tuned tone plus noise |
| **CLAP** | Band-passed noise: three short bursts 9 ms apart, then a tail | The hand-clap circuits of the classic machines |
| **TOM** | A sine that drops in pitch from high to low; an accent (X) a fourth higher | Analog tom |
| **COWBELL** | Two square waves, 540 Hz and 800 Hz, softened | 808-style cowbell |
| **808 KICK** | The existing kick with a long body and a slow pitch drop (in TRAP, 808 BOUNCE, DRILL, HALF STEP) | Long sine kick |

Sources: [Wikipedia, TR-808](https://en.wikipedia.org/wiki/Roland_TR-808),
[Wikipedia, TR-909](https://en.wikipedia.org/wiki/Roland_TR-909),
[Sonic State, "Synthesizing an 808 drum kit"](https://sonicstate.com/amped/2021/03/16/synthesizing-a-tr-808-drum-kit),
[Sound On Sound, "Practical snare drum synthesis"](https://www.soundonsound.com/techniques/practical-snare-drum-synthesis).
(The research did not give the cowbell's circuit in detail; its two frequencies are the commonly quoted 808 ones.)

Levels: SNARE, CLAP, TOM and COWBELL each have their own level (K1 to K4 of DRUMS 2), all under the master DRUMS level
on the MIXER. I matched them to your kick and rim: their peaks are within about 3 dB.

## The 30 drum patterns (DRUMS page K1)

Original 10: KICK ONLY, HOUSE, TECHNO, MINIMAL, 2-STEP, BREAK, HALF-TIME, ELECTRO, DUB, BOSSA.

New 20: BOOM BAP, TRAP, REGGAE, DISCO, FUNK, 808 BOUNCE, BREAKBEAT, TRIBAL, ROCK, HALF STEP, AFRO, SON CLAVE, SAMBA,
DRILL, ELECTRO FUNK, INDUSTRIAL, LO-FI, AMBIENT, JAZZ, TRANCE. Generic grooves of those styles, 16 steps each.
Drums only play while a rhythm is running (not with rhythm HELD), as before.

## Chord-change glitch: what was checked (v24)

- The dip when changing chord with GLIDE OFF (a fade to silence then a new attack, about 10 ms) is fixed: the new chord
  switches at once and keeps the level.
- Audit: all 40 sounds, with glide off and on, rhythm and held chord, each change compared with a no-change run: no
  remaining dip (the two cases that first looked like one, DAWN and SOFT METAL, were 99% and 95% with a longer window).
- Not tested by ear. Not covered: a brief interruption when the FM-1 writes to its memory (saving songs and settings),
  which the dropout counter (HOME + OCT+) may show.

## Random silent steps in a rhythm

The engine log of every step (15 rhythms, 20 bars each): with DENSITY at its default (0), 4,875 steps, 0 skipped, 0 dropped.
With DENSITY at 30, about 26% of hits are dropped at random, in every rhythm. DENSITY is on the GROOVE page (ENV button), K3,
and shows 0 when centered; below it, "fewer hits" is its job.

## The bass (v27): 11 new sounds and 16 new patterns (BASS page: K1 sound, K3 riff)

Blop had 7 bass sounds (OFF, SUB, ROUND, ACID, REESE, PLUCK, FM) and 8 patterns. They are unchanged: I rendered all 48
combinations of the old sounds and patterns with the old and the new engine, and the audio is byte for byte the same.

### New bass sounds

| Bass | How it is built | What the sources say |
| --- | --- | --- |
| **MOOG** | Three saws an octave apart (outer two slightly detuned), 24 dB ladder filter, moderate resonance | Minimoog bass: three oscillators an octave apart, low-pass only, the filter does the character |
| **FUNK** | Square plus saw plus a sub, a snappy filter, short body | Funk and "Thriller"-style Minimoog basses: sharp filter envelope |
| **JUNO SUB** | Pulse, saw and a sub sine, a soft filter | Roland's own guide: the Juno's architecture suits thick analog bass |
| **808 SUB** | A saturated sine with a long decay | Trap and hip-hop sub bass (not researched, my own knowledge) |
| **HOUSE PLUCK** | Saw plus square, a resonant, short pluck | House basses are often plucked sounds that give the bounce |
| **ACID SQUARE** | A single square wave, high resonance, short filter decay | The 303 has one oscillator, saw or square; resonance is the key |
| **ACID 24** | A saw into two filters in a row (24 dB), resonant, driven | Guides suggest two low-pass filters with heavy envelope and resonance |
| **DX BASS** | FM: the modulator at the note's pitch with a little feedback, its depth falling fast | The DX bass: a modulator whose level falls quickly acts like a filter sweep; feedback adds punch |
| **FM TECHNO** | FM with the modulator an octave up (ratio 2) | "Modulator ratio 2 gives the classic techno bass; 4 goes into rave" |
| **REESE DEEP** | Three saws detuned by plus and minus 0.3 semitone, steep 24 dB filter, drive | Reese: detuned saws (about 0.3 semitone), steep low-pass, phasing from the detune |
| **WOBBLE** | Saws and a sub, the filter swung in eighth notes (2.2 octaves) | Only the general idea (an LFO on the cutoff) is in the sources; the dubstep wobble itself is my own knowledge |

New engine tools: a bass table of recipes, a second filter stage (24 dB), soft saturation, FM, the tempo-synced filter
wobble. Patterns can now also use the **fifth** (5) and the **flat seventh** (7) above the root, and an **accent** (X): louder
(measured +2.1 dB) with a deeper filter sweep, like a 303's accent. (The 303's slide is not implemented.)

### New bass patterns (16 steps; x note, X accent, o octave, 5 fifth, 7 flat seventh, - tie, . rest)

| Pattern | Steps | Idea |
| --- | --- | --- |
| DISCO | `xoxoxoxoxoxoxoxo` | Alternating octaves (the best-supported pattern in the sources) |
| PULSE 8THS | `x.x.x.x.x.x.x.x.` | Pulsing eighths |
| HOUSE OFFBEAT | `..x...x...x...x.` | Short notes between the kicks |
| TECHNO ROLL | `.xxx.xxx.xxx.xxx` | Rolling sixteenths that leave the kick's beat |
| REGGAE | `x-----5---x-5---` | Root and fifth, long notes (the sources describe root-fifth reggae figures) |
| DUB | `x-----..x-----5-` | Sparse |
| FUNK | `x..x..x...x.7x.x` | Syncopated, with a flat seventh |
| SLAP | `X..x.xo.x..xX.o.` | Accents and octaves |
| WALKING | `x---5---o---5---` | Root, fifth, octave, fifth (a simple version) |
| ACID A, ACID B | `X.xox.xX.o.x.xo.`, `x.xxox.xX.x.o.xX` | Accents and octave jumps (the 303 advice: notes that fit the track, then random octaves and accents) |
| TRANCE | `xxoxxxoxxxoxxxox` | Sixteenths with octaves |
| 808 TRAP | `X-----x---x-----` | Long sub hits |
| SYNTHWAVE | `XxxxXxxxXxxxXxxx` | Accented sixteenths |
| BOSSA | `x--.5--.x--.5-..` | Root and fifth |
| DNB | `x-----.x..x-----` | Two-step |

**Honest limits.** The sources are tutorials and articles about the techniques; none gave exact patch values for
hardware, and the numbers I used are my own tuning (balanced for loudness against your ROUND bass, checked for pitch,
accent, wobble rate and FM brightness on rendered audio, not by ear). The walking-bass figure, the funk and slap figures and
the trance, synthwave, bossa and DNB patterns are my own simple versions: the searches did not give those patterns.

### Sources
- Overview and classics: [Lunacy, essential synth bass sounds](https://lunacy.audio/synth-bass/),
  [MusicRadar, 4 legendary bass synths](https://www.musicradar.com/how-to/4-legendary-bass-synths-find-out-how-they-work-and-get-their-sound),
  [Emastered, synth bass guide](https://emastered.com/blog/synth-bass)
- Moog and funk: [MusicRadar, a Minimoog-style bass patch](https://musicradar.com/how-to/minimoog-analogue-bass),
  [MusicRadar, the Thriller bass](https://www.musicradar.com/how-to/recreate-thriller-synth-minimoog)
- Acid: [MusicRadar, an acid bassline](https://www.musicradar.com/how-to/make-acid-bassline-fatboy-slim-303),
  [Attack Magazine, an acid house bassline](https://www.attackmagazine.com/technique/tutorials/how-to-make-an-acid-house-bassline/),
  [MusicRadar, the TB-303 guide](https://www.musicradar.com/news/producers-guide-roland-tb-303)
- Reese: [Native Instruments, Reese bass](https://blog.native-instruments.com/reese-bass/),
  [MusicRadar, an old-school Reese](https://musicradar.com/how-to/how-to-create-a-classic-old-school-reese-bass-sound)
- FM: [MusicTech, advanced FM synthesis with Dexed](https://musictech.com/tutorials/learn-advanced-fm-synthesis-with-dexed/)
- Juno and house: [Roland, creating Juno basses](https://articles.roland.com/?p=64263),
  [MusicRadar, classic house bass sounds](https://www.musicradar.com/news/5-house-bass-sounds)
- Patterns: [Attack Magazine, eight common bassline styles](https://www.attackmagazine.com/technique/tutorials/low-end-theory-exploring-eight-common-bassline-styles/),
  [Splice, basslines for different genres](https://splice.com/blog/how-to-make-basslines/)

### Measured on the FM-1 (v27)
Worst case I could build (DARK DRONE pad, REESE DEEP bass with the TRANCE pattern, drums, every effect at max, 2 minutes):
CPU 75 to 76%, no dropouts during the run. Heavy basses one by one, with the same load: 56 to 61%. The idle level depends on
what is switched on in the setup (an A/B install of v26 and v27 gave the same idle level).


## Post-punk and dark wave (v28)

**Why the snare seemed to play when kick, hi-hat and rim were off:** the snare has its own level. It was on the hidden DRUMS 2 page
(default 70%), so nothing on the DRUMS page turned it down. Now every voice has its own knob: DRUMS = PATTERN, KICK, SNARE, HI-HAT;
DRUMS 2 (HOME + GLO) = RIM, CLAP, TOM, COWBELL. The MIXER page (ARP button) has DRUMS, the master over all of them.

### 10 new drum patterns (30 to 39)
POST PUNK, MECHANICAL, TOM TRIBE, TOM DRIVE, MOTORIK, GOTH MACHINE, SLOW TOMS, COLD WAVE, NEW WAVE, DUB WAVE. 16 steps each.

### 3 bass sounds and 5 patterns
Sounds: HOOK BASS (bright, high on the neck, chorus-like), PICKED (plucked, clicky), DARK PULSE (dark filtered pulse).
Patterns: POST PUNK, HOOK A, HOOK B, LEGATO 8THS, GOTH RUN.

### 8 DARK WAVE pads (6th family of PRESET)
COLD STRINGS, BLACK CHOIR, ICE PAD, FARFISA, SCREAM PAD, NIGHT DRIFT, GHOST ORGAN, STATIC WALL.
Levels were measured and balanced against the other pads.

### Sources and honest limits
Researched: style descriptions of post-punk and dark wave drumming, Peter Hook-style high bass, string-machine (Solina) pads,
cold wave and Sisters of Mercy drum-machine style (BeatKey, Far Out, Reverb and Guitar World on Peter Hook, MusicRadar on the Solina,
Wikipedia on cold wave and The Sisters of Mercy). These give descriptions, not note-by-note patterns.
My own: the exact patterns and pad tunings are my interpretation. FARFISA was not covered by the research. I cannot judge sound by ear.
Not possible: 3/4 and 6/8 feels, because the grid is a fixed 16 steps.

### Measured on the FM-1 (v28)
DARK WAVE pads (STATIC WALL, NIGHT DRIFT) with DARK PULSE bass, POST PUNK drums, every effect at max: CPU 58 to 66%, peak 66%,
0 dropouts, also over 60 seconds of playing. Your setup was stored in song 12 first and loaded back afterwards.
