#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

EXPECTED_SHA256="73731a5a7932965de02a9e98055dcf88b4d17b8f710a6ecfde3e36a1f248773b"
EXPECTED_SIZE="1048576"

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 /path/to/lufia1-rom.sfc" >&2
    exit 2
fi

SOURCE_ROM="$1"

CANONICAL_ROM="local/roms/Lufia & the Fortress of Doom (USA).sfc"
GENERATED="local/generated/lufia1-test2"
BUILD_DIR="local/build/lufia1"
LOG="local/logs/launcher-lufia1-build.log"

CLI="local/tools/snesrecomp-cli-dist/snesrecomp-cli-linux-x86_64/snesrecomp"

mkdir -p \
    local/roms \
    local/generated \
    local/build \
    local/logs

exec > >(tee "$LOG") 2>&1

echo "=== Estopolis Recompendium: Lufia I build ==="
echo

if [ ! -f "$SOURCE_ROM" ]; then
    echo "ERROR: ROM does not exist:"
    echo "  $SOURCE_ROM"
    exit 1
fi

SIZE="$(stat -c '%s' "$SOURCE_ROM")"
SHA="$(sha256sum "$SOURCE_ROM" | awk '{print $1}')"

echo "ROM size : $SIZE"
echo "SHA-256  : $SHA"

if [ "$SIZE" != "$EXPECTED_SIZE" ] ||
   [ "$SHA" != "$EXPECTED_SHA256" ]; then

    echo
    echo "ERROR: This is not the supported Lufia I USA baseline ROM."
    exit 1
fi

if [ ! -x "$CLI" ]; then
    echo
    echo "ERROR: SNESRecomp CLI was not found at:"
    echo "  $CLI"
    exit 1
fi

echo
echo "[1/5] Importing ROM..."

# Avoid copying a file onto itself if the user selected the already-imported ROM.
SOURCE_REAL="$(readlink -f "$SOURCE_ROM")"
DEST_REAL="$(readlink -m "$CANONICAL_ROM")"

if [ "$SOURCE_REAL" != "$DEST_REAL" ]; then
    cp -f "$SOURCE_ROM" "$CANONICAL_ROM"
else
    echo "ROM is already in the project ROM directory."
fi

echo
echo "[2/5] Generating SNESRecomp output..."

rm -rf "$GENERATED"

"$CLI" build \
    --rom "$CANONICAL_ROM" \
    --output "$GENERATED" \
    --name "Lufia I"

if [ ! -f "$GENERATED/build.sh" ]; then
    echo
    echo "ERROR: SNESRecomp did not create:"
    echo "  $GENERATED/build.sh"
    exit 1
fi

echo
echo "[3/5] Building generated SNESRecomp static library..."

bash "$GENERATED/build.sh"

GENERATED_LIB="$GENERATED/build/libsnesrecomp_game.a"

if [ ! -f "$GENERATED_LIB" ]; then
    echo
    echo "ERROR: generated static library was not produced:"
    echo "  $GENERATED_LIB"
    exit 1
fi

echo
echo "Generated library:"
echo "  $GENERATED_LIB"

echo
echo "[4/5] Configuring Lufia I host..."

cmake -S games/lufia1 -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DSNESRECOMP_SDL_BACKEND=SDL2

echo
echo "[5/5] Compiling Lufia I host..."

cmake --build "$BUILD_DIR"

EXE="$BUILD_DIR/lufia1"

if [ ! -x "$EXE" ]; then
    echo
    echo "ERROR: build completed without producing:"
    echo "  $EXE"
    exit 1
fi

echo
echo "============================================================"
echo " LUFIA I BUILD COMPLETE"
echo "============================================================"
echo
echo "Executable:"
echo "  $EXE"
echo
echo "Lufia I is ready to play."
