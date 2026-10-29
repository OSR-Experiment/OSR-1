# OSR-1

Firmware pour le synthé M-VAVE FM-1 : grilles d'accords jouables à un doigt, rythmes d'accords, kick et
sidechain, glide, reverb. Par Obsimo / OSR Records. Construit sur [fm1-base](https://github.com/charlesvestal/fm1-base)
et [felucca-bsp](https://github.com/hugelton/felucca-bsp).

État : **prototype, jamais testé sur un vrai FM-1.** Le code tourne dans le simulateur et dans le navigateur.

## Commandes

| Commande | Fonction | Avec SEL (shift) |
| --- | --- | --- |
| Touches Do4 à Do5 (13) | Accords de la grille : blanches = sûrs, noires = épices | |
| Touche Si3 ou ENV (tenir) | VARIATION : version sus, basse sur la quinte | |
| PRESET | Grille d'accords (38) | Tonalité |
| ALGORITHM | Rythme (16) | Richesse (triade, 7e, 9e, riche) |
| SELECT | Son (32) | Famille |
| K1 | Cutoff | Glide |
| K2 | Résonance | Voicing (serré, ouvert, large) |
| K3 | Reverb | (libre) |
| K4 | Delay | BPM |
| OCT- / OCT+ | Octave | |
| SEQ | Rythme on/off | |
| FX | Kick | |
| LFO | Sidechain | |
| ARP | Basse on/off | |
| PLAY | Latch (l'accord continue) | |
| SAVE | Sauver les réglages | |
| EDIT, GLO, HOME, REC | Libres (REC : futur LOOP) | |

## Fabriquer le firmware

Le plus simple : pousser ce dépôt sur GitHub. Le workflow `.github/workflows/build.yml` télécharge l'outil de
compilation de JieLi et le SDK AC79, lance les tests du simulateur, puis fabrique `build/osr1.fwsc`. Le fichier se
récupère dans l'onglet Actions, rubrique « Artifacts ».

En local (Linux x86-64) :

    git clone --recursive <ce dépôt> && cd osr1
    python3 fm1-base/tools/build.py . --host            # simulateur
    build/host/osr1_host tests/basic.txt build/host      # tests, captures d'écran, sons
    fm1-base/tools/get_toolchain.sh && fm1-base/bsp/tools/get_sdk.sh
    JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/.jieli/ac79-sdk python3 fm1-base/tools/build.py .
    tools/build_web.sh                                   # version navigateur (Zig)

## Fichiers

- `app.c` : commandes, grilles, écran 240 x 240, réglages
- `dsp/engine.c` : moteur sonore (voix sub / FM / corde pincée, filtre, enveloppes, séquenceur, kick, sidechain,
  chorus, delay, reverb)
- `data.h` : sons, rythmes et grilles, générés depuis le prototype (`node tools/gen_data.mjs`)
- `prototype/` : le prototype navigateur d'origine
- `web/emu/` : la page navigateur du vrai firmware

## Charge processeur

Firmware réel (`OSR-1.fwsc`) lancé dans l'émulateur de la puce [fm1-emu](https://github.com/joakimeriksson/fm1-emu)
(horloge émulée à 240 MHz) : **18 à 19 % du temps dans l'interruption audio**, accord tenu (son SIROP), kick et
sidechain compris. Le vrai FM-1 tourne vers 360 MHz. Il reste donc beaucoup de marge (reverb plus riche, plus de voix).
À confirmer sur la machine (`plat_cpu_pct`).

## Télécharger

La dernière version compilée est sur la page **Releases** du dépôt (`OSR-1.fwsc`, pas encore testée sur un vrai FM-1).

## Licence

OSR-1 est un logiciel libre sous **GPL-3.0-only** (voir `LICENSE`). Le code reste public, chacun peut l'étudier, le
modifier et le redistribuer sous la même licence. Il peut donc reprendre du code d'autres firmwares FM-1 sous GPL
(Felucca, Clipper...) ; chaque emprunt est crédité dans `CREDITS.md`.

Les fichiers de fm1-base et felucca-bsp restent sous MPL-2.0 (compatible avec la GPL), `web/emu/worklet.js` aussi
(repris de fm1-base). Le paquet installable contient trois fichiers du SDK AC79 de JieLi (Apache-2.0).
