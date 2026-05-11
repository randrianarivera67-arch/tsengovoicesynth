# Tsengo Voice Synth — VST3

**Mic → MIDI pitch detection plugin** — chante dans ton micro, les notes apparaissent automatiquement dans le piano roll de ton DAW.

## Fonctionnalités

| Feature | Détails |
|---|---|
| 🎤 Entrée microphone | Sélection du périphérique dans l'UI |
| 🎵 Détection de pitch | Algorithme YIN (de Cheveigné & Kawahara 2002) |
| 🎹 Sortie MIDI | noteOn / noteOff → canal MIDI 1 |
| ⚙️ Paramètres | Threshold · Volume · Attack · Release |
| 🖥️ Interface | Crystal dark-cyan — 620×420 px |
| 🎼 Piano roll visuel | C3–B5, touche active en surbrillance |
| 📊 Level meters | IN (vert) · OUT (orange) |

## Compatibilité

- **FL Studio 20/21** ✅
- **Ableton Live 11/12** ✅
- **Reaper** ✅
- Windows 10/11, macOS 12+, Linux (Ubuntu 22+)

## Build

### Prérequis

- CMake ≥ 3.22
- Windows : Visual Studio 2022
- macOS : Xcode 14+
- Linux : GCC 11+ + libs ALSA/Jack

### Compilation

```bash
git clone https://github.com/TON_USERNAME/TsengoVoiceSynth.git
cd TsengoVoiceSynth
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Le VST3 compilé se trouve dans :
```
build/TsengoVoiceSynth_artefacts/Release/VST3/
```

## Installation FL Studio

1. Copier le dossier `.vst3` vers :
   - Windows : `C:\Program Files\Common Files\VST3\`
   - macOS   : `/Library/Audio/Plug-Ins/VST3/`
2. FL Studio → Options → Manage plugins → Scan
3. Le plugin apparaît sous **"Tsengo Voice Synth"**
4. Activer **MIDI Output** sur la piste dans le mixer

## Paramètres

| Paramètre | Plage | Description |
|---|---|---|
| Threshold | 0.05–0.50 | Sensibilité YIN (bas = plus sensible) |
| Volume | 0–1 | Niveau pass-through |
| Attack | 5–200 ms | Délai avant noteOn (anti-faux déclenchement) |
| Release | 50–2000 ms | Durée avant noteOff après silence |

## Architecture

```
Mic Input
   │
   ▼
YIN Pitch Detection (2048 samples)
   │
   ├── frequency (Hz) → MIDI note
   ├── confidence    → seuil activable
   │
   ▼
State Machine (Attack/Release debounce)
   │
   ▼
MidiBuffer → DAW Piano Roll
```

## CI/CD

GitHub Actions compile automatiquement sur Windows, macOS et Linux à chaque push.

---
*Tsengo Voice Synth © 2026 — RANDRIANARA*
