#!/bin/bash
# Build civ-wasm. With no argument: the publishable site in build-web/site/
# (players load their own game files). With a Civilization directory: a
# local test build in build-wasm/ with those files baked into reciv.data,
# served by dev.html; never publish that one.
set -e
root=$(cd "$(dirname "$0")/../.." && pwd)
. "${EMSDK:-$HOME/emsdk}/emsdk_env.sh" >/dev/null 2>&1
if [ -n "$1" ]; then
    build=$root/build-wasm
    emcmake cmake -S "$root" -B "$build" -G Ninja -DBUILD_TESTING=OFF \
        -DCMAKE_BUILD_TYPE=Release -DCIVWASM_PRELOAD="$(cd "$1" && pwd)"
    ninja -C "$build"
    cp "$root/web/dev.html" "$build/"
else
    build=$root/build-web
    emcmake cmake -S "$root" -B "$build" -G Ninja -DBUILD_TESTING=OFF \
        -DCMAKE_BUILD_TYPE=Release -DCIVWASM_PRELOAD=
    ninja -C "$build"
    rm -rf "$build/site"
    mkdir -p "$build/site"
    cp "$build/reciv.js" "$build/reciv.wasm" "$root/web/touch.js" \
        "$root/web/manifest.webmanifest" "$root/web/icon.svg" "$build/site/"
    # The page, with the game rules for the AI (docs/GAME-RULES.md) inside it.
    awk -v rules="$root/docs/GAME-RULES.md" '
        /<script type="text\/markdown" id="game-rules"><\/script>/ {
            print "<script type=\"text/markdown\" id=\"game-rules\">"
            while ((getline line < rules) > 0) print line
            print "</script>"
            next
        }
        { print }' "$root/web/index.html" > "$build/site/index.html"
    touch "$build/site/.nojekyll"
fi
