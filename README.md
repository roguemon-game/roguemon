# Roguémon

![Roguémon](cover.png)

A Pokémon Emerald ROM hack where every run grows a new region, played as a Nuzlocke.

Every time you start, Professor Birch shows you a land nobody has seen before: towns, routes, caves, the sea, eight gyms, Victory Road and the Champion. Catch only the first Pokémon you meet in each area. A Pokémon that faints is gone. Wins and losses unlock new starters, items and rules for the next run.

**[Download the latest release](https://github.com/roguemon-game/roguemon/releases/latest)**: the patch, a README with patching steps, and two player guides as PDFs.

- [Trailer](https://youtu.be/Mo60_J5mIKs)
- [Hackdex](https://www.hackdex.app/hack/roguemon)
- [romhack.ing](https://romhack.ing/database/content/entry/oySUxziUSpa1x8ssvQDuqQ/rogu-mon)
- [Discord](https://discord.gg/5UgSGxcZQF): bug reports, region seeds and Nuzlocke stories

## How to play

1. Get your own copy of Pokémon Emerald (USA, Europe), SHA-1 `f3ae088181bf583e55daf962a92bb46f4f1d07b7`. It is not included.
2. Apply the `.bps` patch with [Rom Patcher JS](https://www.marcrobledo.com/RomPatcher.js/).
3. Play in a GBA emulator such as mGBA, with the save type set to Flash 128K.

## Source code

This repository holds the game's source, a fork of pokeemerald-expansion. The `main` branch holds the source of the latest release, v1.1.5-beta. The region generator is in `src/rogue_gen*.c` and `src/rogue_gen_*.h`, and the run rules are in `src/rogue*.c`. `INSTALL.md` covers the build toolchain. Run `make` to build `pokeemerald.gba`. Bug reports go to the Discord.

## Credits

- Based off RHH's pokeemerald-expansion 1.17.1: https://github.com/rh-hideout/pokeemerald-expansion/ (thanks to the Rom Hacking Hideout and [all its contributors](https://github.com/rh-hideout/pokeemerald-expansion/blob/master/CREDITS.md))
- Built on [pokeemerald](https://github.com/pret/pokeemerald), the decompilation of Pokémon Emerald by pret and its contributors

Roguémon is a free fan game. It is not made or approved by Nintendo, and it is not for sale. This repository shares source code and patches only, never ROMs.
