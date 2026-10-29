# civ-wasm

**Play in the browser: https://awkto.github.io/civ-wasm/**

Sid Meier's Civilization (1991, DOS) running in a web browser: the original
game, rebuilt from a reconstruction of its source and compiled to
WebAssembly, with a few additions for playing it on a modern screen.

You need your own copy of the original game files. None are included here,
and the page never uploads yours: they are kept in your browser.

## Playing

Open the page and give it your game: the Civilization folder, a .zip of it,
or the loose files, chosen or dropped onto the page, or the URL of a .zip.
The next visit starts straight away. Mouse and keyboard work as in the
original.

A link can carry the files, so a game can be shared in one click:

    https://awkto.github.io/civ-wasm/?url=<zip of the DOS game>&skin=<civdata4.rsc, or a zip with it>

The server holding the zip must allow cross-origin requests.

## What's new compared with the DOS game

- **The map fills the window.** The View setting picks a zoom (1.5× to 4×,
  or the classic 320×200); the map shows as many tiles as fit, and the other
  screens keep their original size. Resizing the window, or the **− / +**
  buttons in Settings, change it during a game.
- **Windows map art.** Add `civdata4.rsc` from Civilization for Windows
  (1993), on its own or inside that game's folder or .zip, and the map uses
  the Windows version's terrain, units, cities and city screen, drawn at
  twice the detail. Menus and text stay DOS. The Map art setting switches
  between the two.
- **Your games, saved as you play.** Each game keeps its own autosave and
  the start of each of its last 20 turns. The start page lists your games to
  continue, rename, delete, or take back to an earlier turn. The game's own
  Save and Load slots still work and are listed apart. Download keeps a
  game as a .zip (back it up, or play it on another device), and Load save
  file adds one back, or a DOS game's CIVIL<i>n</i>.SVE with its .MAP.
- **Quicker play.** No sound and mouse setup questions, Continue goes
  straight back to the map, the copy-protection quiz can be skipped, and
  full-screen animations, wipes and fades run 2–3× faster (or at the
  original speed).
- **Enemy moves shown, fast or skipped.** Skipped means the map no longer
  jumps to each move another civilization makes. The setting is on the start
  page and in Settings during a game.
- **Your automatic moves shown, fast or skipped.** GoTo, Road To and the
  Auto orders can move without animation and without the map jumping to
  them; the view catches up when one of those units needs you. Moves you
  make yourself are always animated.
- **Settings while playing.** The Settings button changes zoom, display,
  map art, animation speed, enemy moves, the city blink, refusing other
  civilizations' emissaries, barbarians and autosave in the middle of a game, and saves
  and goes back to the start page to begin or continue another game.
  Shortcuts: Alt with - or _ zooms out, Alt with = or + zooms in, Alt with
  ` ~ * or Backspace switches between the Windows and DOS map art.
- **Road To (Shift+R).** A settlers' GoTo that builds road on every tile
  on its way, in the Orders menu below Build Road. The settlers walk over
  road already there and stop for orders at the target, before a river they
  cannot bridge yet, or when another civilization's unit comes next to
  them.
- **Auto Irrigate (Shift+I) and Auto Mine (Shift+M).** Auto Irrigate
  works for your cities on that landmass: the tiles they can work, the
  nearest city first, tiles a city is working before the rest, and cities
  that already have as many irrigated tiles as citizens only after the
  others. Auto Mine improves the land around where you gave the order,
  nearest tiles first, out to 6 tiles. Irrigation goes on Desert, Plains,
  Grassland and River wherever water reaches (each new irrigated tile lets
  the next one in), mines on Hills.
  Other terrain is left alone. Enemies don't interrupt them; when nothing
  is left to do they ask for orders. Auto Mine given again where no Hills
  are left is level 2: it mines the Mountains around. Activating them gives them back to
  you.
- **Rail To (Shift+T), Auto Road (Shift+A), Auto Rail (Shift+L).** Rail To
  is Road To laying railroad (road first where there is none). Auto Road
  and Auto Rail link your cities on the settlers' landmass with the
  shortest network of straight-ish roads or railroads, working the nearest
  tile still missing, then ask for orders. Given again on a landmass whose
  network is built, they go to level 2: more direct links between each city
  and its nearest neighbours (up to six, within 12 tiles), so there are
  several ways between cities. Rail To and Auto Rail need Railroad.
- **Auto Explore (Shift+E).** Any land or sea unit heads for the nearest
  known tile that borders unexplored land or sea, on its own landmass or
  sea, choosing again at every move, until nothing is left to explore
  there, or until another civilization's unit is next to it; then it asks
  for orders. Explorers don't share goals and pass over
  tiles that others hold. It is on the Orders menu too.
- **Better Default Unit.** A setting: new cities start on the best
  defender you know (Phalanx, Musketeers, Riflemen, Mech. Inf.) instead of
  Militia until Gunpowder.
- **No barbarians.** A setting: no raids from the sea, no uprisings near
  cities, and huts give mercenaries instead of a horde. The game rolls for
  barbarians each turn, so it can be switched at any time; barbarians
  already on the map stay.
- **Display filters.** Crisp pixels, raw pixels, hq4x smoothing or bilinear, always
  at the original 4:3 shape. Fullscreen is in the toolbar (or F11).

Saved games live in your browser's storage. Clearing the site's data
removes them.

## Building

Install the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)
in `~/emsdk` (or set `EMSDK`), plus CMake and Ninja, then:

    tools/wasm/build.sh                # the site, in build-web/site/
    cd build-web/site && python3 -m http.server 8765

`tools/wasm/build.sh /path/to/CIVILIZATION` makes a local test build
instead, with the game files baked in, served by `build-wasm/dev.html`.
Never publish that one.

Pushes to `main` publish the site to GitHub Pages; the `dev` branch is
published as a preview under `/dev/`. [WASM.md](WASM.md) describes how the
browser build works and what each addition changes.

## Where it comes from

- [civd](https://github.com/sjmelia/civd) reconstructed the DOS game's
  source by reverse engineering. It is not MicroProse's original code.
- [reciv](https://github.com/sjmelia/reciv) rebuilt that source on SDL3 so
  it runs natively on Windows and Linux. civ-wasm is a fork of reciv. Its
  native build is still in the tree (`cmake -S . -B build -G Ninja`; see
  reciv for details).
- civ-wasm adds the browser build and the features above. Changes to reciv's
  code are marked `civ-wasm:`; reciv's own changes to the game are marked
  `reciv:`.

## Licences

The build includes [SDL3](https://libsdl.org) (zlib),
[HQx](third_party/hqx) (LGPL-2.1+) and
[Nuked-OPL3](third_party/nuked-opl3) (LGPL-2.1+). The HQx and Nuked-OPL3
licences are in `third_party/`; SDL3 comes from Emscripten's ports. Civilization and its art belong to their owners. This
project contains none of it.
