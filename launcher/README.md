# Estopolis Recompendium Launcher

Initial Linux launcher prototype.

## Current behavior

- accepts ROM drag-and-drop;
- provides a **Select ROM** file chooser;
- identifies supported ROMs by size and SHA-256;
- currently supports the verified Lufia I USA ROM;
- copies the verified ROM into `local/roms/`;
- runs SNESRecomp generation;
- configures and compiles the current Lufia I host;
- shows **Loaded / Built** when the game is ready;
- enables **Play** only for a ready target;
- launches the game as a child process;
- keeps the launcher open so closing the game returns to it.

The future Estpolis I / Lufia II / Estpolis II rows are placeholders until
their exact ROM baselines and build targets are established.

## Linux dependencies

Debian / Ubuntu:

```bash
sudo apt install \
  libsdl2-dev \
  libsdl2-image-dev \
  libsdl2-ttf-dev \
  zenity
```

## Build

From the repository root:

```bash
cmake -S launcher -B local/build/launcher -G Ninja \
  -DCMAKE_BUILD_TYPE=Release

cmake --build local/build/launcher
```

## Run

```bash
launcher/run_launcher.sh
```

The prototype intentionally reuses the existing development SNESRecomp CLI
under `local/tools/`. A later packaging phase should make this dependency
self-contained.
