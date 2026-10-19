#!/bin/sh
# Cross-build SDL and reciv for Windows using MinGW-w64 GCC 14+ on Linux.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build=${1:-"$root/build-win"}
mkdir -p "$build"
build=$(CDPATH= cd -- "$build" && pwd)
deps="$build/_deps"
version=3.2.10
archive="SDL3-$version.tar.gz"
sha=f87be7b4dec66db4098e9c167b2aa34e2ca10aeb5443bdde95ae03185ed513e0
toolchain="$root/cmake/mingw-w64-x86_64.cmake"
for command in cmake ninja curl sha256sum tar x86_64-w64-mingw32-gcc x86_64-w64-mingw32-windres; do
    command -v "$command" >/dev/null || { echo "Missing build tool: $command" >&2; exit 1; }
done
major=$(x86_64-w64-mingw32-gcc -dumpfullversion | sed 's/[^0-9].*//')
[ "$major" -ge 14 ] || { echo 'MinGW-w64 GCC 14+ is required for _BitInt.' >&2; exit 1; }
mkdir -p "$deps"
if [ ! -f "$deps/$archive" ]; then
    curl -fL --retry 3 "https://github.com/libsdl-org/SDL/releases/download/release-$version/$archive" -o "$deps/$archive.part"
    mv "$deps/$archive.part" "$deps/$archive"
fi
printf '%s  %s\n' "$sha" "$deps/$archive" | sha256sum --check
if [ ! -d "$deps/SDL3-$version" ]; then
    tar -xf "$deps/$archive" -C "$deps"
fi
cmake -S "$deps/SDL3-$version" -B "$deps/sdl-build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$toolchain" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$deps/sdl-install" \
    -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TESTS=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_EXAMPLES=OFF
cmake --build "$deps/sdl-build" --parallel "${RECIV_BUILD_JOBS:-4}"
cmake --install "$deps/sdl-build"
cmake -S "$root" -B "$build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$toolchain" -DCMAKE_BUILD_TYPE=Release \
    -DSDL3_DIR="$deps/sdl-install/lib/cmake/SDL3"
cmake --build "$build" --parallel "${RECIV_BUILD_JOBS:-4}"
cmake --install "$build" --prefix "$build/package"
cp "$deps/SDL3-$version/LICENSE.txt" "$build/package/licenses/SDL3.txt"
(cd "$build/package" && cmake -E tar cf "$build/reciv-windows-x86_64.zip" --format=zip -- \
    reciv.exe SDL3.dll README.md licenses sources)
printf '\nWindows build: %s/reciv.exe (with SDL3.dll beside it)\n' "$build"
printf 'Portable folder: %s/package\n' "$build"
printf 'Archive: %s/reciv-windows-x86_64.zip\n' "$build"
