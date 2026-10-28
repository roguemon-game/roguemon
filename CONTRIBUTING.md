# Contributing to Roguémon

Thanks for looking. Roguémon is a small fan project, so the process is light.

## Bugs and ideas

Report bugs on the [Discord](https://discord.gg/5UgSGxcZQF), with the game's version and your region's name (the run code on the run summary, such as `KALONA-N`). Anyone can type that code on Prof. Birch's map to play the same region, so a bug report with one is easy to reproduce.

## Code changes

Pull requests against `main` are welcome. For anything bigger than a fix, ask on the Discord first, so your work fits where the game is going.

Before you open one:

1. Build it: `make -j8` (see [INSTALL.md](INSTALL.md)). The build must have no new errors and still fit in memory (EWRAM is nearly full).
2. If you touched the region generator (`src/rogue_gen*`), run the host checks from [tools/roguemon](tools/roguemon/README.md):

    ```console
    python3 tools/roguemon/check_seeds.py 200
    ```

    It generates 200 regions on your computer and fails if any of them can't be finished or breaks a rule. Look at a region or two with `preview_region.py` as well.
3. Play the part you changed in an emulator (mGBA is a good one).

Some things to know:

- **The generator is shared.** `src/rogue_gen.c` and the `src/rogue_gen_*.h` files it includes are plain C with no GBA calls, so the same code builds into the ROM and into `gen_host` on your computer. Keep them that way.
- **Regions come from seeds.** Any change to what the generator draws changes every region, so the same run code shows another region after your change. That's fine for a new version. Say so in the pull request.
- **Saves carry over between versions.** A player's record, unlocks and current run live in vars and flags listed in `include/constants/rogue.h`. Don't reuse or renumber them.
- **Comments explain why.** Follow the style around you: plain sentences that say what a piece does and why, with the Hoenn map or game it copies where there is one.

[ARCHITECTURE.md](ARCHITECTURE.md) maps the code. Comments in the code also name some of the project's own emulator tests (`playtest.py`, `run0test.py`, `*test.py`). Those need a private emulator harness and are not published.

## Credits and rights

Roguémon is built on [pokeemerald-expansion](https://github.com/rh-hideout/pokeemerald-expansion) by the Rom Hacking Hideout and [pokeemerald](https://github.com/pret/pokeemerald) by pret. Like them, it has no license file. It is a free fan game and is not made or approved by Nintendo. Never commit a ROM.
