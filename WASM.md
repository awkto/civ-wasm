# civ-wasm

reciv built for the browser with Emscripten. The upstream reciv README
describes the game and the native builds; this file covers the WebAssembly
build only. Changes to upstream code are marked `civ-wasm:`.

## Building

Install the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)
in `~/emsdk` (or set `EMSDK`), plus CMake and Ninja. Then:

    tools/wasm/build.sh gamedata/CIV
    cd build-wasm && python3 -m http.server 8765
    # open http://localhost:8765/dev.html

The argument is a directory holding the original game files. It is packed
into `reciv.data` for local testing, so never publish that build. SDL3 comes
from Emscripten's ports.

## How it differs from the native build

- **Blocking waits.** The game waits in `reciv_poll`'s `SDL_DelayNS`, which
  Asyncify turns into returns to the browser's event loop.
- **Mismatched calls.** The period code calls functions with other argument
  counts and return types than they were defined with (MSC's cdecl allowed
  it). wasm traps on such calls, so wasm-ld would link a trap stub for each
  one. `tools/wasm/wasm-ld` runs the real wasm-ld and then
  `fix-signature-mismatch.py`, which turns each stub into an adapter that
  drops extra arguments, passes zero for missing ones and fixes up the
  return value. This happens before Binaryen, so Asyncify sees the real
  calls. The build reaches the wrapper through a symlink copy of emsdk's
  LLVM directory (`EM_LLVM_ROOT`, set only for the link step).
- **No archive.** The game and platform objects are linked directly rather
  than from `libreciv-core.a`. wasm-ld does not check the signature of a
  symbol it pulls lazily from an archive, so some mismatches went undetected
  and produced invalid code.
- **No section GC.** wasm-ld runs with `--no-gc-sections`, because a trap
  stub does not reference the real function and GC would discard it.
  Binaryen removes what stays unused.
- `main.c` calls `game_main()` with no arguments, matching `GAME.C`.
