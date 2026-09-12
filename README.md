<p align="center">
  <img src="docs/assets/banner.png" alt="Estpolis Recompendium" width="100%">
</p>

<h1 align="center">Estpolis Recompendium</h1>

<p align="center">
  A native recompilation and enhancement project for the Super Famicom/Super Nintendo games <em>Estpolis Denki I and II</em> (known as <em>Lufia &amp; the Fortress of Doom</em> and <em>Lufia II: Rise of the Sinistrals</em> in North America).
</p>

---

## About

**Estpolis Recompendium** is a native recompilation and enhancement project built around [SNESRecomp](https://github.com/RetroPortingToolKit/snesrecomp).

Recompendium is intended to provide a single native environment for both games and to provide optional enhancements such as improved rendering, expanded audio options, language switching, quality-of-life features, and other modernization work while retaining a faithful vanilla mode.

> [!IMPORTANT]
> This repository does **not** contain commercial game ROMs or copyrighted game data. You must provide your own legally obtained ROM dumps.

---

## Current Status

|  | Lufia I (USA) | Estpolis Denki I (Japan) | Lufia II (USA) | Estpolis Denki II (Japan) |
|---|:---:|:---:|:---:|:---:|
| ROM baseline identified | ✅ | ➖ | ➖ | ➖ |
| SNESRecomp generation | ✅ | ➖ | ➖ | ➖ |
| Generated C builds | ✅ | ➖ | ➖ | ➖ |
| CPU / runtime | ✅ | ➖ | ➖ | ➖ |
| PPU / video | ✅ | ➖ | ➖ | ➖ |
| Keyboard input | ✅ | ➖ | ➖ | ➖ |
| Gamepad input | ✅ | ➖ | ➖ | ➖ |
| Real-time SPC / S-DSP audio | ✅ | ➖ | ➖ | ➖ |
| Music and sound effects | ✅ | ➖ | ➖ | ➖ |
| Stable gameplay testing | 🟡 | ➖ | ➖ | ➖ |
| Shared regional architecture | ➖ | ➖ | ➖ | ➖ |
| Runtime English / Japanese switching | ➖ | ➖ | ➖ | ➖ |
| Enhancement systems | ➖ | ➖ | ➖ | ➖ |

**Legend:** ✅ working • 🟡 working but needs further testing/validation • ❌ failing / known broken • ➖ not started

---

## Required ROMs

ROM files are **not included** in this repository.

| Game | Region | Expected filename | Size | SHA-256 | Status |
|---|---|---|---:|---|:---:|
| Lufia & the Fortress of Doom | USA | `Lufia & the Fortress of Doom (USA).sfc` | 1,048,576 bytes | `73731a5a7932965de02a9e98055dcf88b4d17b8f710a6ecfde3e36a1f248773b` | ✅ Baseline |
| Estpolis Denki | Japan | TBD | TBD | TBD | ➖ Future|
| Lufia II: Rise of the Sinistrals | USA | TBD | TBD | TBD | ➖ Future |
| Estpolis Denki II | Japan | TBD | TBD | TBD | ➖ Future |

---

## Roadmap

| Milestone | Goal | Status |
|---|---|:---:|
| **1** | Lufia I recompilation | 🟡 Near completion |
| **2** | Estpolis Denki I recompilation + English/Japanese unification | ➖ Planned |
| **3** | Enhancement phase for Lufia / Estpolis I | ➖ Planned |
| **4** | Lufia II / Estpolis Denki II integration | ➖ Planned |
| **5+** | Cross-game refinement, packaging, release engineering, and other enhancements | ➖ Planned |

---

## Potential Enhancements/Features:

| Feature | Status |
|---|:---:|
| Vanilla mode | 🟡 Baseline in progress |
| Adjustable perspective / tilt | ➖ Planned |
| 2.5D terrain / scene rendering experiments | ➖ Planned |
| Improved lighting and visual effects | ➖ Planned |
| Enhanced water / environmental presentation | ➖ Planned |
| Enhanced audio playback | ➖ Planned |
| SoundFont support | ➖ Planned |
| Orchestral / electronic / FM-style audio profiles | ➖ Planned |
| Multi-language support | ➖ Planned |
| New English translation kept more faithful to the original script | ➖ Planned |
| UI improvements | ➖ Planned |
| Quality-of-life options | ➖ Planned |
| Configurable enhancement presets | ➖ Planned |

---

## Building

### Requirements

Current development is **Linux-first**.

You will need:

- Git
- CMake
- Ninja
- a modern C/C++ compiler
- SDL2 development libraries
- the required ROM placed locally

Clone with submodules:

```bash
git clone --recursive git@github.com:dothacktranslate/Lufia-Estpolis-Recompendium.git
cd Lufia-Estpolis-Recompendium
```

If the repository was cloned without submodules:

```bash
git submodule update --init --recursive
```

Configure and build Lufia I:

```bash
cmake -S games/lufia1 -B local/build/lufia1 -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DSNESRECOMP_SDL_BACKEND=SDL2

cmake --build local/build/lufia1
```

Run:

```bash
local/build/lufia1/lufia1 \
  "local/roms/Lufia & the Fortress of Doom (USA).sfc"
```

---

## Controls

| Input | SNES Button |
|---|---|
| Arrow keys | D-Pad |
| Z | B |
| X | A |
| A | Y |
| S | X |
| Q | L |
| W | R |
| Enter | Start |
| Backspace | Select |
| Esc | Quit |

Gamepads use the usual spatial face-button layout where possible.

---

## Project Structure

```text
Lufia-Estpolis-Recompendium/
├── common/                 # Shared Recompendium infrastructure
├── docs/                   # Documentation and project notes
├── extern/
│   └── snesrecomp/         # Pinned SNESRecomp fork/submodule
├── games/
│   ├── lufia1/             # Lufia I host/runtime
│   └── lufia2/             # Lufia II host/runtime
├── local/                  # Untracked local ROMs/builds/logs/generated files
└── tools/
```

---

## SNESRecomp

This project uses a pinned fork of **SNESRecomp**.

---

## Known Issues

### Lufia I
- Post-battle text briefly flickers. Currently investigating.

---

## Legal

This project is an independent fan-made reverse-engineering project.

No commercial ROMs, copyrighted game assets, or proprietary Nintendo/Taito/Natsume data are distributed by this repository. Users are responsible for supplying their own game dumps.

**Lufia**, **Estpolis**, and related names and trademarks belong to their respective owners.

---

## Credits

- **SNESRecomp / RetroPortingToolKit**: static recompilation framework and SNES runtime
- **Cellenseres / Lufia2SNESRecomp**: research reference for SNES recompilation work
- **Original Staff**: the work this project seeks to extend
