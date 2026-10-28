# Roguémon's host tools

These build the region generator for your computer (`gen_host`) and use it to check and look at regions without a ROM or an emulator. A check of 20 regions takes about 20 seconds.

## Setup

- Python 3 with [Pillow](https://pypi.org/project/pillow/): `python3 -m pip install pillow`
- A C compiler as `clang` (Xcode's command line tools on a Mac, or your system's clang on Linux).

They don't need the ARM toolchain or a built ROM.

The first run builds `gen_host` into `tools/bin/` (ignored by git), and rebuilds it whenever the generator's sources are newer.

## The tools

| Command | What it does |
| --- | --- |
| `python3 tools/roguemon/check_seeds.py [COUNT] [FIRST_SEED]` | Generates COUNT regions (200 by default) from FIRST_SEED (1) and checks that every one can be finished: every door, trainer and item reachable, badges gating the way in order, obstacles that open what they should, maps that join cleanly. It prints the failures and a set of statistics at the end, and exits non-zero if a region fails. Run it after any change to `src/rogue_gen*`. |
| `python3 tools/roguemon/preview_region.py SEED OUT.png [--scale S]` | Draws every map of a region into one PNG, with the real tilesets. |
| `tools/bin/gen_host SEED OUTDIR` | The generator itself: writes each place's map (`mapN.bin`) and `region.txt`, the plan with every map's size and events. |
| `python3 tools/roguemon/gen_rooms.py` | Writes `src/data/rogue_rooms.h` from Hoenn's tower, hideout and lair maps. `--png DIR` also renders the rooms. |
| `python3 tools/roguemon/gen_region_map.py` | Writes `src/data/rogue_region_map.h`, how the tiles of Hoenn's region map join, which the region map is drawn from. |

`rogue_emu.py`, `render_map.py`, `seam_check.py`, `split_check.py` and `tile_polish.py` are the modules these use.

## Not published

`rogue_emu.py` also has the emulator half of the project's test tools, and the code's comments name those tests (`playtest.py`, `run0test.py`, `savetest.py` and the other `*test.py`). They drive the ROM through a headless mGBA harness that is not published, so they can't run from this repo. The other scripts named as the source of a generated file (`gen_maps.py`, `gen_stamps.py`, `make_gyms.py`, `make_title.py` and so on) are not published yet either.
