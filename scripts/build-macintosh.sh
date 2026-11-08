#!/bin/sh
# Build without needing a Macintosh ROM or System disk. Requires Docker.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
IMAGE=ghcr.io/autc04/retro68@sha256:dd923969100f408943f73c9618b3c7628686aae819583eecb0ddd1837da6943c
docker run --rm --platform linux/amd64 -v "$ROOT:/src" "$IMAGE" sh -c 'cmake -S /src/ports/macintosh -B /src/build/macintosh -DCMAKE_TOOLCHAIN_FILE=/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake && cmake --build /src/build/macintosh -j2'
# Finder's bundle bit is required to associate the embedded application icon.
python3 "$ROOT/ports/macintosh/finder_icon.py" "$ROOT/build/macintosh/SkiFreePlus.bin" "$ROOT/build/macintosh/SkiAuto.bin"
