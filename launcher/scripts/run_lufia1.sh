#!/usr/bin/env bash
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 1

ROM="local/roms/Lufia & the Fortress of Doom (USA).sfc"
EXE="local/build/lufia1/lufia1"

if [ ! -f "$ROM" ]; then
    echo "Lufia I ROM is not imported." >&2
    exit 1
fi

if [ ! -x "$EXE" ]; then
    echo "Lufia I is not built." >&2
    exit 1
fi

exec "$EXE" "$ROM"
