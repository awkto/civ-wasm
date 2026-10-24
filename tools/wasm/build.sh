#!/bin/sh
# Build civ-wasm into build-wasm/. Pass a Civilization directory to bake it
# into reciv.data for local testing (never publish that build).
set -e
root=$(cd "$(dirname "$0")/../.." && pwd)
. "${EMSDK:-$HOME/emsdk}/emsdk_env.sh" >/dev/null 2>&1
emcmake cmake -S "$root" -B "$root/build-wasm" -G Ninja -DBUILD_TESTING=OFF \
    -DCMAKE_BUILD_TYPE=Release -DCIVWASM_PRELOAD="${1:+$(cd "$1" && pwd)}"
ninja -C "$root/build-wasm"
cp "$root/web/dev.html" "$root/build-wasm/"
