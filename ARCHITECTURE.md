# How Roguémon's code fits together

Roguémon is pokeemerald-expansion with a region generator on top. Almost everything Roguémon adds lives in files whose names start with `rogue`. The rest of `src/` is the expansion's, with small changes where the game hooks in.

## The two halves

**The generator** makes a region from a seed: the plan (towns, routes, caves, gyms, the sea, dungeons), then each place's map, its people, items and words. It is plain C with no GBA calls, so it builds into the ROM and into `gen_host` on your computer (see [tools/roguemon](tools/roguemon/README.md)).

- `include/rogue_gen.h`: the generator's data (a region, a place, a map) and its entry points.
- `src/rogue_gen.c`: the core: the plan, towns, routes, the region map. It ends by including the `src/rogue_gen_*.h` files, so they share its helpers. Start here.

**The game side** runs the generated region inside Emerald: it loads maps, places events, builds trainers and wild tables, keeps the Nuzlocke rules and saves the record.

- `src/rogue.c`, `include/rogue.h`: the glue: warps, maps, events, wild Pokémon, trainers and the script specials the event scripts call.
- `data/scripts/rogue.inc`: the event scripts for generated people, signs and buildings.

## Where things are

| What | Files |
| --- | --- |
| Region plan, towns, routes, region map | `src/rogue_gen.c` |
| Route kinds and where they go | `src/rogue_gen_zones.h`, `src/rogue_gen_land.h`, `src/rogue_gen_water.h`, `src/rogue_gen_rock.h`, `src/rogue_gen_vert.h`, `src/rogue_gen_bridge.h`, `src/rogue_gen_sea.h` |
| Special towns and route features | `src/rogue_gen_towns2.h`, `src/rogue_gen_signatures.h`, `src/rogue_gen_features.h`, `src/rogue_gen_berry.h` |
| Dungeons and their floors | `src/rogue_gen_dungeon.h` and the kinds it includes: `cave`, `forest`, `ice`, `volcano`, `rooms` (towers, hideouts, lairs), `ruins`, `ship`, `plant`, `peaks`, `tunnel` |
| Legends, villain teams | `src/rogue_gen_legend.h`, `src/rogue_gen_hideout.h` |
| Items | `src/rogue_gen_items.h` |
| Who the people are | `src/rogue_gen_people.h`, `src/rogue_gen_names.h`, `src/rogue_gen_facts.h`, `src/rogue_gen_lore.h` |
| What people say | `src/rogue_gen_text.h` (the text engine: templates with slots for the region's facts) and its domains, `src/rogue_text_*.h` (towns, trainers, leaders, dungeons, Birch...); `src/rogue_text.c` fills in what only the game knows |
| Towns' people and interiors | `src/rogue_town.c` |
| Dungeons in the game | `src/rogue_dungeon.c` |
| Nuzlocke rules, run results, the record and unlocks | `src/rogue_run.c` |
| The starter menu in the lab | `src/rogue_starter_menu.c` |
| Region map, run summary, UNLOCKS screens | `src/rogue_ui.c` |
| Loading screen | `src/rogue_loading.c` |
| Saved vars and flags, version | `include/constants/rogue.h` |
| RUN 0, the secret last region | `src/rogue_run0*.c`, `include/rogue_run0*.h`, `src/rogue_gen_run0.h`, `src/rogue_run0_trainers.h`, `src/rogue_text_director.h`, `data/scripts/rogue_run0.inc` (spoilers) |

## Generated files

Some data was made by scripts that read Emerald's own maps and graphics. The results are committed, so the build never runs them. Most generated headers name their script at the top. Two of those scripts are published in `tools/roguemon`, and both write their files byte for byte as committed:

| Output | Script |
| --- | --- |
| `src/data/rogue_rooms.h` (rooms for towers, hideouts and lairs) | `gen_rooms.py` |
| `src/data/rogue_region_map.h` (how Hoenn's map tiles join) | `gen_region_map.py` |

The rest (`gen_maps.py` for `data/maps/Rogue*`, `gen_stamps.py`, `make_gyms.py`, `make_title.py` and others) are not published yet. Edit their outputs by hand, and ask on the Discord if you need one.

## How a run flows

1. The player starts a run. The game picks a seed (or takes one from a typed run code) and asks the generator for the plan.
2. Each time the player enters a place, the generator draws that place's map from the seed, and `src/rogue.c` loads it into one of the `RoguePlace*` map slots with its events.
3. Battles, catches and faints go through `src/rogue_run.c`, which keeps the Nuzlocke rules. When the run ends, the record and any unlocks are saved and the region is gone.

The same seed always gives the same region, which is how run codes and the DAILY REGION work.
