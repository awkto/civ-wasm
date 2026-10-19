# reciv

Sid Meier's Civilization (1991, DOS 474.05) running natively on modern
Windows and Linux. The aim is the original game, bugs included, in a window.

You need your own copy of the original game data; reciv does not include it.

The game logic in `src/game` comes from [civd](https://github.com/sjmelia/civd),
a reverse-engineered reconstruction of the DOS source, not MicroProse's
original code. reciv's changes to it are marked `reciv:`. `src/platform`
replaces DOS, the BIOS and the game's drivers with SDL3.

## Building on Linux

You need CMake, Ninja, GCC 14+ (for `_BitInt`) and SDL3.

    cmake -S . -B build -G Ninja
    ninja -C build
    ctest --test-dir build --output-on-failure
    ./build/reciv -d /path/to/CIVILIZATION -s /path/to/saves

`tools/build-windows.sh` cross-builds a Windows package with MinGW-w64 GCC 14+.

## Building on Windows (Visual Studio)

Install Visual Studio 2026 with **Desktop development with C++**, and CMake
4.2+ on PATH. Then, from PowerShell:

    .\tools\build-msvc.ps1
    .\build-msvc\package\reciv.exe -d C:\Games\Civilization -s C:\Games\reciv-saves

The script downloads and verifies SDL3, and produces `build-msvc/package/`
and `build-msvc/reciv-windows-msvc-x86_64.zip`. For VS 2022, add
`-VisualStudio 2022 -BuildDir build-msvc-2022`.

MSVC support is experimental; GCC is the reference build. Four of the eight
tests need GNU `ld --wrap` and are skipped under MSVC.

## Options

| Option | Effect |
| --- | --- |
| `-d DIR` | Original game files (default: current directory). |
| `-s DIR` | Saved games (default: the game directory). |
| `--skip-quiz` | Skip the copy-protection quiz. |
| `--filter NAME` | Artwork scaling: `hq4x` (default), `sharp`, `nearest`, `bilinear`. `--no-filter` means `nearest`. |
| `--text-filter NAME` | Lettering scaling: `sharp` (default), `nearest`, `bilinear`, or `same` to use the artwork filter. |

Filters change only what you see in the window. The game, its saves and its
screenshots are unaffected, and the picture is always shown at 4:3.

## Licences

Portable packages include the notices for the bundled
[SDL3](https://libsdl.org) (zlib), [HQx](third_party/hqx) (LGPL-2.1+) and
[Nuked-OPL3](third_party/nuked-opl3) (LGPL-2.1+) in `licenses/`, with the
HQx source in `sources/`.
