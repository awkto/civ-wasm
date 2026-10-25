# civ-wasm

reciv built for the browser with Emscripten. The upstream reciv README
describes the game and the native builds; this file covers the WebAssembly
build only. Changes to upstream code are marked `civ-wasm:`.

## Playing

The page (`web/index.html`) asks for the original game files: a folder, a
.zip, or loose files, chosen or dropped onto the page, or the URL of a .zip
(the server must allow cross-origin requests). A link with `?url=<zip>`
downloads that zip straight away, and a site that bundles the game names
its zip in `<meta name="civ-data">`. They are kept in the
browser's IndexedDB, so later visits start straight away. Saved games are kept
there too (synced every few seconds). The canvas fills the window and the game
keeps a 4:3 picture inside it; mouse and keyboard work as in the original.

Pushes to `main` build and publish the site to GitHub Pages
(`.github/workflows/pages.yml`). Upstream's Linux and Windows workflows are
disabled in this repository. The View setting sizes the game's screen to
the window (`--screen WxH`): the map fills it, other screens stay 320x200.
When the window changes, the game takes the new size the next time it
waits for an order (UNITS.C `blink_unit`, platform `civwasm.c`).

With Autosave on (`--autosave SECONDS`), the game writes slot 9 (CIVIL9.SVE
and .MAP, through its own silent save routine) at most that often while it
waits for an order, so a closed tab loses at most the last few seconds.
Continue (`--continue`) answers the setup questions, skips the introduction
and loads slot 9. Saves live in the browser's IndexedDB.

## Building

Install the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)
in `~/emsdk` (or set `EMSDK`), plus CMake and Ninja. Then:

    tools/wasm/build.sh                # site in build-web/site/
    cd build-web/site && python3 -m http.server 8765

With a game directory as the argument (`tools/wasm/build.sh gamedata/CIV`),
the files are baked into `build-wasm/reciv.data` and `dev.html` starts the
game straight away, for local testing. Never publish that build. SDL3 comes
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
