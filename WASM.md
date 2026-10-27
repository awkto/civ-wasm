# civ-wasm

How the browser build works: reciv built with Emscripten, and the
additions made for the browser. [README.md](README.md) covers playing and
building; reciv's own README describes the native builds. Changes to upstream code are marked `civ-wasm:`.

## Playing

The page (`web/index.html`) asks for the original game files: a folder, a
.zip, or loose files, chosen or dropped onto the page, or the URL of a .zip
(the server must allow cross-origin requests). A link with `?url=<zip>`
downloads that zip straight away (`?skin=` the same for the Windows map
art). A site that hosts the game puts a `site.json` beside the page,
`{"data": "civ1.zip", "skin": "civdata4.rsc"}` (URLs, relative to the page
or not; the older `<meta name="civ-data">` and `civ-skin` work too): every
visitor then plays from those files, the file choosers fold away under "Use
your own game files instead", and a URL remembered from elsewhere is not
used. The files are kept in the
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
Everything lives in the browser's IndexedDB. Download gives a game (or a
slot) as a .zip of CIVIL0.SVE and .MAP; Load save file makes a new game
from each .SVE with its .MAP, zipped or not.

### Sharp scaling and the layers

The DOS picture, the skin's 2x picture, an hq4x picture and the text layer
are each enlarged by nearest pixels to the same multiple (of 2 with the
skin, of 4 with hq4x) nearest the view's scale, then stretched once
linearly (`sharp_factor`, `sharp_stage` in video.c); scale modes are set
every frame. The page keeps text on its own layer (`--text-filter sharp`),
so hq4x never smooths it. With the skin and hq4x, the screen is smoothed as
one picture (each pixel the average of its 2 x 2 in the skin's picture) and
the skin's own pixels go back on top unsmoothed.

### Settings while playing

The toolbar's Settings button opens an overlay holding the start page's
own options, which apply at once: View (zoom, also − and +), Display
(`civwasm_set_filter`, taken at the next frame), Map art
(`civwasm_show_skin`; the page always loads `civdata4.rsc` when it has it,
with `--skin-hidden` for the DOS art), Animations
(`civwasm_set_anim_speed`), Enemy moves, the city blink
(`civwasm_set_city_blink`, `--no-city-blink`), turning away emissaries
(`civwasm_set_refuse_emissaries`, `--refuse-emissaries`: AUDIENCE.C's
"Will you receive him?" is answered No without asking, as if the player
had chosen it), Better Default Unit (`civwasm_set_better_default`,
`--better-default-unit`: CITY.C starts a new city on Phalanx with Bronze
Working and Mech. Inf. with Labor Union, besides the game's own Musketeers
and Riflemen) and Autosave (`civwasm_set_autosave`). While playing, the
page takes Alt with `-`/`_` and `=`/`+` (or the numpad's - and +) as zoom
out and in, and Alt with `` ` ``/`~`/`*`/Backspace as a switch of map art, by key
position (`KeyboardEvent.code`) and before the game sees them. "Save and go to the start page" asks for an
autosave at the next wait for an order (`civwasm_save_now`), files it under
the game and reloads.

### Enemy moves

`--enemy-moves show|fast|skip` (the page's Enemy moves setting, and its
toolbar button while playing through `civwasm_set_enemy_moves`). Fast
slides other civilizations' units in four steps instead of sixteen and
pauses 8 ticks after each move instead of 30. Skip treats the game's own
Enemy Moves option as off (no centring on each move, no slide) and drops
the pause it still makes for a move in view. The game's option is kept:
with it off, show and fast behave as it always did.

### Cities that need a new build

A city whose production can no longer complete (an improvement it already
has, a wonder built elsewhere, a spaceship part after launch, a unit at the
127-unit cap) blinks on the map while the game waits for an order or at End
of Turn: a thin yellow ring around the city and a yellow minimap dot, at
a quarter of the active unit's pace (on for two of its blinks, off for two). It is the same test that makes the city
screen's Change button blink (CITYMGT.C `city_change_due`), drawn from
UNITS.C `blink_unit`; changing the production stops it.

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

With the skin, the city screen takes WinCiv's colours: cream panels, a dark
red frame and headers, grey buttons with the chosen tab white. CITYMGT.C
paints them into the skin layer only (`civwasm_skin_fill`, skin colours 80
and 81 that the sheet leaves unused), after the DOS drawing, so hiding the
skin shows the DOS screen unchanged; `skin_frame` draws light text that sits
on a cream panel in near-black. Irrigation is drawn again over WinCiv's land
(`civwasm_skin_sprite`), since the DOS map draws it under the terrain.

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

## Road To

Shift+R (or Orders, Road To) on settlers chooses a target as GoTo does.
`civwasm_road_to[civ][unit]` (PATH.C) marks the order; CONTROL.C then asks
`road_to_goal` instead of `unit_goal`. It answers `r` while the settlers'
tile has no road (or city), and otherwise unit_goal's step, keeping the
GoTo to the target so that the target gets its road too. It stops for
orders, clearing the GoTo, at the target, before a step onto a tile that
cannot take a road (river without Bridge Building), and whenever another
civilization's unit stands next to the settlers. The marks are written
after the game's own data in the save (SAVE.C); a save without them loads
with none, and the DOS game ignores the extra bytes.

## Auto Irrigate, Auto Mine

Shift+I and Shift+M (or the Orders menu) on settlers set
`civwasm_auto_work[civ][unit]` to `i` or `m` and remember the tile as the
centre (`civwasm_auto_x`, `civwasm_auto_y`, saved after the Road To marks).
CONTROL.C then asks `auto_work_goal` (PATH.C) at every move. It keeps work
under way, and otherwise picks the tile closest to the centre (then closest
to the settlers) within 6 tiles, on the centre's landmass, seen by the
player, without a foreign unit and not taken by other auto settlers, that
the game's own `work_tile` would improve the same way: 1 irrigate
(irrigable terrain with water next to it), 2 mine (Hills). It answers that
order on the tile, a GoTo step towards it, or -1 with the order cleared
when nothing is left. Since the choice is made again each time, every new
irrigated tile lets irrigation reach further. Shift+M where
`auto_work_goal` finds nothing gives level 2, `n`: Mountains (terrain 5)
without a mine, which `work_tile` leaves alone. Waking the settlers
(`unit_reset`, the stack list, `kill_cb`) clears the order.

## Your automatic moves

`civwasm_own_moves` (`--own-moves show|fast|skip`, `civwasm_set_own_moves`,
the page's "Your automatic moves") mirrors Enemy moves for the player's own
GoTo, Road To and Auto orders. CONTROL.C sets `civwasm_auto_step` at `uc:`
when the command comes from one of those, never for a key or click. Fast
steps `anim_unit` by 4 and shortens the wait before the view jumps to an
off-screen unit; skip leaves out `map_center` and `anim_unit` (the unit is
only redrawn where it lands, if in view) and the jump to off-screen
automatic units. The view catches up (`own_unseen`) when such a unit next
needs the player, and before a battle or hut it walks into.

## Rail To, Auto Road, Auto Rail

Rail To (Shift+T) is Road To with `civwasm_road_to` 2: `lay_needed` asks
for railroad, so 'r' lays road and then rail on each tile; its cursor reads
RAIL (`civwasm_road_cursor_cut` word 1). Auto Road (Shift+A) and Auto Rail
(Shift+L) are `civwasm_auto_work` 'a' and 'l', answered by `auto_road_goal`
(PATH.C): Kruskal over each own city's six nearest on the order's
landmass. Given where that network is built (CONTROL.C asks
`auto_road_goal` at the order), they are 'A' and 'L', level 2: after the
network, every other of those links within 12 tiles. Each link is walked by `link_step`, steps that always come closer
to the far city (so at most 40), preferring tiles already done and the
straight line, over land that takes a road and past no foreign city or
unit; an unwalkable link is left out. The settlers go to the needed tile
nearest them that no other auto settler has taken, and ask for orders when
none is left. Rail To and Auto Rail need Railroad (tech 49). An auto
settler held by a zone of control waits for the next turn instead of
cancelling.
