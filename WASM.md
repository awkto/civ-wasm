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

With Autosave on (`--autosave SECONDS`), the game writes CIVILA.SVE and
.MAP (its own silent save routine; not one of Load's slots 0-9) at most
that often while it waits for an order. The page files each new one under
the game being played, in `/saves/games/<id>/`: `current.*`, and
`history/T<turn>.*` for the start of each of the last 20 turns. The start
page lists those games to resume, rename, delete or restore to an earlier
turn; the game's own Save slots are listed apart. `--continue [N]` loads
CIVILA (or slot N) without the introduction, `--quick-setup` answers AdLib
and mouse, and `--browser-saves` turns off the game's own AutoSave (slots
4-9). A turn-0 save loads as a new game, so the first turn has none.
Everything lives in the browser's IndexedDB.

### Windows map art

The map can use the art of Civilization for Windows (1993) instead of the
DOS tiles. The player adds `civdata4.rsc` from a Windows install (loose, in
its folder or in its .zip, the same ways as the game files, or with
`?skin=<url>` in a link; a site can name one in `<meta name="civ-skin">`).
The page keeps it in `/saves/skin/` and passes `--skin FILE`; the Map art
setting switches between the two. Nothing of it is in this repository.

That file holds one picture, `SPR32X32.GIF` (CvPc resource 200, a GIF-coded
1472x400 sheet with its own 72-colour palette). It is the DOS sprite
pictures drawn again at twice the size and at the same places: SP257 (x, y)
is the sheet's (2x, 2y), TER257 is (640 + 2x, 2y) and SP299 is
(832 + 2x, 2y - 100). `skin.c` decodes it. When `load_screen` (FILEIO.C)
loads one of those pictures, the parts the sheet has (terrain, coasts,
rivers, roads, railways, specials, huts, citizens, units, ocean tiles) go
into the page's skin layer, a buffer at twice the page's size
(`mgraphic.c`). Skin pixels travel with blits, `set_page`, dissolves and
the sprites `cut_tile` makes from them; recolouring a unit picture into a
civilization's colours recolours its skin too. Any other write to a page
pixel (fills, lines, text, other sprites) clears the skin there, so
everything else on the screen, and anything the sheet lacks, shows the DOS
pixels. `video.c` composites the screen at twice its size when the skin is
on. The sheet's water colours (64 to 71) turn with the map's first colour
cycle, its colours follow the palette fades (`timer.c` keeps how far a fade
has gone), and a sentry's skin is washed out to light greys as DOS turns
its dark colours grey. `--xray` (the game's debug view of the whole map)
helps testing it.

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
