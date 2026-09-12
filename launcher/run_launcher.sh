#!/usr/bin/env bash
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

EXE="local/build/launcher/estopolis-launcher"

if [ ! -x "$EXE" ]; then
    echo "Launcher is not built."
    echo
    echo "Build it with:"
    echo "  cmake -S launcher -B local/build/launcher -G Ninja -DCMAKE_BUILD_TYPE=Release"
    echo "  cmake --build local/build/launcher"
    exit 1
fi

exec "$EXE"
