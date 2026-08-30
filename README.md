# Tsengo Voice Synth (Tovoice) v5.0

VST3 Synth — mandray mic mivantana → MIDI notes ao Piano Roll FL Studio.
Kendrena ho mitovy fiasa amin'i **Dubler 2** (Vochlea Music).

## Produits telo, code iray
| Produit | Ampiasaina rehefa | Fomba fandraisana feo |
|---|---|---|
| `Tsengo Voice Synth.vst3` | Channel Rack FL Studio | manokatra micro manokana (tab SETUP) |
| `Tsengo Voice FX.vst3` | insert amin'ny piste misy ny micro (Reaper/Ableton/Cubase/Studio One) | entree audio an'ny hote |
| `Tsengo Voice.exe` (standalone) | ivelan'ny DAW | micro + **sortie MIDI externe** (loopMIDI / IAC) |

Ny fanovana lehibe indrindra amin'ny v5: tsy voatery hifanandrina amin'ny DAW
amin'ny fibahanana ny périphérique audio intsony ny plugin. Raha misy audio
alefan'ny hôte dia izay no raisina (`Auto`), ka miasa amin'ny setup ASIO rehetra.

## Setup
1. Copy .vst3 → `C:\Program Files\Common Files\VST3\`
2. FL Studio → Scan → Channel Rack → Tsengo Voice Synth
3. Sokafy plugin → tab **PLAY** → safidio microphone → CONNECT
4. Tsindrio **CALIBRER LE MICRO** (4 dingana, 12 segondra)
5. Record → mihira → MIDI notes! 🎵

## Nova tao amin'ny v4.0 (ny elanelana telo tamin'i Dubler dia voatapaka)

### 1. Triggers voaofana (trained per-pad), fa tsy heuristika intsony
Ny pad enina dia mianatra ny feo ataonao manokana:
- Tsindrio `TRAIN` amin'ny pad iray, avereno in-5 ka hatramin'ny in-10 ilay feo
  (bm / ts / psh…), dia tsindrio `STOP`.
- Isaky ny onset dia alaina **fingerprint 14 dimensions** (ZCR, spectral centroid,
  spread, rolloff, flatness, crest, + 8 bandes log). Ny fanasokajiana dia
  1-nearest-neighbour amin'ny z-score normalized distance manoloana ny ohatra
  rehetra voatahiry.
- **Ny pad tsy voaofana dia tsy mandefa MIDI mihitsy** — mitovy amin'i Dubler.
- `SENSIBILITE` = fetran'ny onset, `PRECISION` = hamafin'ny fitakiana mba hifanaraka.
- Voatahiry miaraka amin'ny projet FL Studio ny training (jereo #4).
- Ny fitiliana onset dia ao **anatin'ny mic callback** (hop 128 samples), fa tsy
  amin'ny block-rate an'ny DAW intsony → tsy mivadika intsony ny timing.

### 2. Interface misy tabs
`PLAY | TRIGGERS | KEY | CHORDS | ASSIGN` — tahaka ny an'i Dubler 2.

### 3. Calibration wizard
Dingana 4 mitarika: mangina (noise floor + threshold) → mihira mahazatra (gain) →
nota ambany indrindra → nota avo indrindra. Ny halavan'ny feonao (`pitchMin/Max`)
dia ampiasaina hametra ny fikarohana YIN: **haingana kokoa sady tsy dia diso octave**.
Miasa amin'ny mikro rehetra — tsy mila USB mic manokana toa an'i Dubler.

### 4. Fanovana hafa
- **Preset/state save** : voatahiry ao amin'ny projet ny paramètre rehetra
  + ny training an'ny triggers (`getStateInformation`).
- **Canaux MIDI misaraka** : notes hiraina (Ch 1 default) sy triggers (Ch 10 default).
- **Latence/précision** : 1024 / 2048 / 4096 samples azo safidiana.
- YIN threshold (0.15) tsy mifangaro amin'ny mic gate intsony.
- Ny triggers dia mandeha na dia mangina aza ny lalana pitch.

## Fonctionnalités feno
- **Pitch → MIDI** : YIN + median filter + note-hold anti-flicker
- **Vowel/Formant (aaa/eee/ooo/env)** : LPC (Levinson-Durbin) → 4 macros continus → MIDI CC
- **Key/Scale Quantize** : Chromatic, Major, Minor, Maj/Min Pentatonic, Dorian
- **Chords** : Major, Minor, Sus4, Maj7, Min7, Octave
- **Pitch Bend** : vibrato/glide (1–12 semitones)
- **Time Quantize** : grille tempo an'ny host (1/4 → 1/32)
- **Monitor Synth** : sinus intégré
- **Triggers** : 6 pads voaofana, note MIDI azo safidiana isaky ny pad
- **Octave shift**, **Stickiness**, **Calibration**
- **Assign** : CC MIDI + canaux MIDI

## Mbola tsy mitovy amin'i Dubler 2
- Dubler dia mivarotra **USB mic** manokana voaomana mialoha; eto dia ny mikro
  anananao no calibrena.
- Ny classifier eto dia k-NN amin'ny features DSP; an'i Dubler dia modely
  neural trained. Mitovy ny fomba fiasa (train → recognise), fa mety kely kokoa
  ny fahaiza-manavaka rehefa mitovitovy be ny feo roa.
- Tsy misy application standalone (VST3 ihany).

## Qualité / tests
Ny lalana DSP (`Source/PitchTracker.*`) dia tsy miankina amin'ny framework, ka
azo testena mivantana:
```
cmake -B build-tests -DTSENGO_TESTS_ONLY=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```
Mandeha alohan'ny build Windows ao amin'ny CI izany. Ny test ankehitriny:
précision < 15 cents (82–880 Hz), tsy misy diso octave, fandavana ny feo ivelan'ny
range, fandavana ny tabataba, median filter, ary fanitsiana range mifamadika.

## Build
Automatique amin'ny GitHub Actions (`.github/workflows/build.yml`) → artifact
`TsengoVoiceSynth-VST3-Windows`. Ho an'ny local:
```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```
