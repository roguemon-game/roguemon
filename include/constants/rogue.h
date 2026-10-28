#ifndef GUARD_CONSTANTS_ROGUE_H
#define GUARD_CONSTANTS_ROGUE_H

// Version of this build. tools/roguemon reads it for release names and covers.
#define ROGUEMON_VERSION "1.1.5-beta"

// Save data. These reuse vars and flags that vanilla Emerald leaves unused.
#define VAR_ROGUE_SEED_LO     VAR_UNUSED_0x40F7
#define VAR_ROGUE_SEED_HI     VAR_UNUSED_0x40F8
#define VAR_ROGUE_PROGRESS    VAR_UNUSED_0x40F9 // see ROGUE_PROGRESS_*; which gyms are won is in FLAG_BADGE01_GET on
#define VAR_ROGUE_ENCOUNTERS  VAR_UNUSED_0x40FA // one bit per area whose first encounter is used: areas 0 to 15
#define VAR_ROGUE_ENCOUNTERS_2 VAR_UNUSED_0x404E // areas 16 to 31
#define VAR_ROGUE_ENCOUNTERS_3 VAR_UNUSED_0x409B // areas 32 to 47
#define VAR_ROGUE_FALLEN      VAR_UNUSED_0x40FB // Pokemon lost this run
#define VAR_ROGUE_RUN_STATE   VAR_UNUSED_0x40FC // see ROGUE_RUN_*
#define VAR_ROGUE_STARTER     VAR_UNUSED_0x40FE // which starter was chosen, for the rival: ROGUE_STARTER_VALUE
// VAR_ROGUE_STARTER since v0.13: the starter menu row's kind and the species
// taken. A value below ROGUE_STARTER_NEW is a run started before v0.13: just
// the row, 0 to 2 for the Grass, Fire and Water starters.
#define ROGUE_STARTER_NEW          0x8000
#define ROGUE_STARTER_KIND_SHIFT   11
#define ROGUE_STARTER_SPECIES_MASK 0x7FF
#define ROGUE_STARTER_VALUE(kind, species) (ROGUE_STARTER_NEW | ((kind) << ROGUE_STARTER_KIND_SHIFT) | (species))
#define VAR_ROGUE_TOWN        VAR_UNUSED_0x4083 // outdoor place the player is in or last left, for interiors
#define VAR_ROGUE_VISITED     VAR_UNUSED_0x408B // one bit per town number visited, for Fly
#define VAR_ROGUE_SEALS       VAR_UNUSED_0x40A1 // one bit per dungeon whose legend's seal is opened (ROBJ_SEAL)
#define ROGUE_SEALS_VISIT_SHIFT 10 // VAR_ROGUE_SEALS bits 10 to 15: visits to the ruins (they shift with each), mod 64
#define VAR_ROGUE_E4          VAR_UNUSED_0x4091 // Elite Four members beaten on this visit to the League
#define VAR_ROGUE_SECOND_CHANCES VAR_UNUSED_0x40A8 // SECOND CHANCE areas reopened this run
// The run's POKeDEX mix (ROGUE_MIX_*), fixed when the run starts from the
// choice in UNLOCKS. 0 (ROGUE_MIX_LEGACY) is a run started before v0.14.
#define VAR_ROGUE_DEX_MIX     VAR_UNUSED_0x40B8
#define ROGUE_MIX_LEGACY   0 // every first form alike, as before v0.14
#define ROGUE_MIX_CLASSIC  1 // mostly Gens 1 to 3 (the default)
#define ROGUE_MIX_NATIONAL 2 // more of Gens 4 to 6 (NATIONAL DEX, the first win)
#define ROGUE_MIX_GLOBAL   3 // every generation alike (GLOBAL DEX, the third win)
#define ROGUE_MIX_COUNT    4
// The record below is carried over when a new game starts.
#define VAR_ROGUE_META_UNLOCKS VAR_UNUSED_0x40FD // ROGUE_UNLOCK_* bits
#define VAR_ROGUE_META_RUNS    VAR_UNUSED_0x40DB
#define VAR_ROGUE_META_WINS    VAR_UNUSED_0x40DC
#define VAR_ROGUE_META_BEST    VAR_UNUSED_0x40E5 // highest VAR_ROGUE_PROGRESS reached
#define VAR_ROGUE_META_CHAMPION_MON VAR_UNUSED_0x409D // species of the lead at the latest win, for the title screen
#define ROGUE_CHAMPION_MON_SHINY (1 << 15)              // in VAR_ROGUE_META_CHAMPION_MON: the lead was shiny

// RUN 0 (v1.0): what kind of run this is, fixed when it starts. The rest of
// RUN 0's save is the Director's sector (struct RogueDirector,
// include/rogue_run0.h). The kind is carried through the new game like the
// record (Rogue_StashRecord), set before it by the menu.
#define VAR_ROGUE_RUN_KIND    VAR_UNUSED_0x40BB // ROGUE_RUN_KIND_*
#define ROGUE_RUN_KIND_NORMAL  0
#define ROGUE_RUN_KIND_RUN0    1 // the secret last region (ROGUE_RUN0_SEED)
#define ROGUE_RUN_KIND_ENDLESS 2 // regions with no League, one after another (after MORE)
// RUN 0's act: 1 to 4 (two gyms each), ROGUE_RUN0_ACT_ENDING once the
// Champion's room is won. 0 outside RUN 0.
#define VAR_ROGUE_RUN0_ACT    VAR_UNUSED_0x40FF
#define ROGUE_RUN0_ACTS       4
#define ROGUE_RUN0_ACT_ENDING 5
// RUN 0 shows on the main menu after this many wins or losses.
#define ROGUE_RUN0_WINS   4
#define ROGUE_RUN0_LOSSES 20
// RUN 0's one-time beats within an attempt (cleared with the run): the
// foundation's named ones, then 19 for the story (FLAG_ROGUE_RUN0_BEAT_0 + n,
// in two blocks: 0 to 8 are 0x2B3 to 0x2BB, 9 to 18 are 0x276 to 0x27F; use
// FLAG_ROGUE_RUN0_BEAT(n)). Beats that hold across attempts are bits in the
// Director's beats (RUN0_BEAT_*, include/rogue_run0.h).
#define FLAG_ROGUE_RUN0_ARRIVED   FLAG_UNUSED_0x2B0 // the lab's arrival scene ran
#define FLAG_ROGUE_TEASE_TOWN     FLAG_UNUSED_0x2B1 // this (normal) run's first town has the win 2 teaser
#define FLAG_ROGUE_TEASE_TOLD     FLAG_UNUSED_0x2B2 // ...and a townsperson has said it
#define FLAG_ROGUE_RUN0_BEAT_0    FLAG_UNUSED_0x2B3 // first of 9 (to 0x2BB)
#define FLAG_ROGUE_RUN0_BEAT_9    FLAG_UNUSED_0x276 // 10 more (to 0x27F)
#define ROGUE_RUN0_BEAT_FLAGS     19
#define FLAG_ROGUE_RUN0_BEAT(n)   ((n) < 9 ? FLAG_ROGUE_RUN0_BEAT_0 + (n) : FLAG_ROGUE_RUN0_BEAT_9 + (n) - 9)
// RUN 0's challenge (src/rogue_run0_challenge.c, src/rogue_run0_trainers.h):
// the attempt's level band and the patches in force, read whenever a trainer
// or a wild Pokemon is made, so they live in vars, not the Director's sector.
// Two of the Trick House's vanilla vars, which no map
// of RUN 0 (generated or real) reaches.
#define VAR_ROGUE_RUN0_BAND    VAR_TRICK_HOUSE_PUZZLE_8_STATE // ROGUE_RUN0_BAND_*: the base level and the counter types
#define ROGUE_RUN0_BAND_BASE(v)   (((v) & 0x3F) + 30) // the party's median level at the start (30 to 93)
#define ROGUE_RUN0_BAND_LOSS(v)   (((v) >> 6) & 0x1F)   // the type the last lost attempt led with most (TYPE_NONE: none)
#define ROGUE_RUN0_BAND_ANSWER(v) (((v) >> 11) & 0x1F)  // RUN0_PATCH_TYPE_ANSWERS's type (TYPE_NONE: none)
#define ROGUE_RUN0_ENDLESS_BASE_MAX 93 // the band's base holds 30 to 93
#define ROGUE_RUN0_BAND_VALUE(base, loss, answer) ((((base) - 30) & 0x3F) | (((loss) & 0x1F) << 6) | (((answer) & 0x1F) << 11))
#define VAR_ROGUE_RUN0_PATCHES VAR_TRICK_HOUSE_PUZZLE_7_STATE // a bit per RUN0_PATCH_* in force (include/rogue_run0.h)
#define FLAG_ROGUE_RUN0_PATCH_NEW FLAG_UNUSED_0x02B     // a badge brought a patch whose PATCH box is still to show

#define ROGUE_PROGRESS_NO_STARTER 0
#define ROGUE_PROGRESS_STARTER    1
// 2 to 9 mean one to eight badges, won in any order.
#define ROGUE_PROGRESS_BADGE_1    2
#define ROGUE_PROGRESS_BADGE_3    4
#define ROGUE_PROGRESS_BADGE_8    9
#define ROGUE_PROGRESS_CHAMPION   10

// Set while in the last gym town once the way to Victory Road has seen all
// eight badges; a temp var, so the gate checks again on the next visit.
#define VAR_ROGUE_GATE_OPEN VAR_TEMP_9

#define ROGUE_RUN_ACTIVE 0
#define ROGUE_RUN_LOST   1
#define ROGUE_RUN_WON    2

// PROF. BIRCH's lines in his lab, for Rogue_BufferProfessor (VAR_0x8006).
#define BIRCH_LINE_WELCOME   0 // on arrival
#define BIRCH_LINE_CHOOSE    1 // before the starter menu
#define BIRCH_LINE_GOOD_LUCK 2 // with the starter and the kit
#define BIRCH_LINE_AFTER     3 // talked to again
#define BIRCH_LINE_NEWS      4 // before the unlock news

// Permanent unlocks, by number. ROGUE_UNLOCK_* are their bits in the
// record's unlocks (see struct RogueRecord in include/rogue.h). What earns
// each one is in Rogue_UpdateRecord, src/rogue_run.c.
#define UNLOCK_PARTNERS        0  // a fourth starter choice
#define UNLOCK_DRAGONS         1  // a fifth starter choice
#define UNLOCK_KIT             2  // a better starting kit
#define UNLOCK_VETERAN         3  // an Amulet Coin at the start
#define UNLOCK_SCOUTING        4  // gym guides tell the leader's team
#define UNLOCK_BERRIES         5  // Oran and Sitrus Berries at the start
#define UNLOCK_SPARE_STARTERS  6  // the two starters not picked, as gifts later on
#define UNLOCK_FOSSIL_STARTERS 7  // a fossil Pokemon as a starter choice
#define UNLOCK_FALLEN_FRIEND   8  // a lost run's longest companion as a starter choice
#define UNLOCK_CHAMP_PARTNER   9  // the latest winning lead as a starter choice
#define UNLOCK_TYPE_CHARM      10 // a type's starter holds its type's item
#define UNLOCK_HIDDEN_ABILITY  11 // a type's starter has its hidden ability
#define UNLOCK_EGG_MOVE        12 // a type's starter knows an egg move
#define UNLOCK_SHINY_CLAUSE    13 // a shiny can always be caught
#define UNLOCK_SHINY_CHARM     14 // shinies appear more often
#define UNLOCK_FOSSILS         15 // a fossil in a cave, and a scientist to revive it
#define UNLOCK_RARE_FINDS      16 // a rare Pokemon in one side area
#define UNLOCK_RIVAL_GIFT      17 // the rival gives a held item before Victory Road
#define UNLOCK_HARDCORE        18 // level caps, no items in battle, SET style
#define UNLOCK_EXPLORER        19 // Escape Ropes and a Max Repel at the start
#define UNLOCK_MARKING_MAP     20 // the region map shows where the legends sleep
#define UNLOCK_MASTER_BALL     21 // a Master Ball at the start
#define UNLOCK_DOWSING         22 // a Dowsing Machine at the start
#define UNLOCK_LUCKY_EGG       23 // a Lucky Egg at the start
#define UNLOCK_TRAINER_CANDY   24 // Rare Candies from the 4th to 6th gym leaders
#define UNLOCK_SECOND_CHANCE   25 // from the 4th badge, a missed first encounter can be met again
#define UNLOCK_LATE_CAMP       26 // a camper on the road between the 6th and 7th gyms
// Bits 32 on live in the record's unlocksHi (v0.13): Rogue_IsUnlockOn and the
// rest take any of them.
#define UNLOCK_EEVEE           27 // an Eevee row in the starter menu
#define UNLOCK_BABIES          28 // a baby Pokemon row
#define UNLOCK_RISING_STARS    29 // a row with a three-stage first form (Abra, Aron...)
#define UNLOCK_STARTER_CASE    30 // left/right on the Grass, Fire and Water rows: all 27 starters
#define UNLOCK_FAR_AWAY        31 // a regional form row (Alolan Vulpix...)
#define UNLOCK_FREE_CHOICE     32 // left/right on every row: the whole pool of its kind
#define UNLOCK_POKEDEX_PICK    33 // any first form caught across runs
#define UNLOCK_ANY_POKEMON     34 // any first form, and the legends caught
// The POKeDEX mix (v0.14): one row in UNLOCKS, CLASSIC, NATIONAL or GLOBAL.
// Both are earned switched off (CLASSIC stays the choice until the player
// picks another); which one is on is the choice (Rogue_GetDexChoice).
#define UNLOCK_NATIONAL_DEX    35 // the NATIONAL DEX may be chosen
#define UNLOCK_GLOBAL_DEX      36 // the GLOBAL DEX may be chosen (no row of its own)
#define ROGUE_NUM_UNLOCKS      37
#define ROGUE_NUM_UNLOCK_ROWS  (ROGUE_NUM_UNLOCKS - 1) // rows in UNLOCKS: the two DEX unlocks share one

// Runs started (losses count) that earn the starter rows of v0.13, and the
// wins that earn ANY POKEMON.
#define ROGUE_EEVEE_RUNS        2
#define ROGUE_BABY_RUNS         2
#define ROGUE_RISING_RUNS       3
#define ROGUE_STARTER_CASE_RUNS 4
#define ROGUE_FAR_AWAY_RUNS     6
#define ROGUE_FREE_CHOICE_RUNS  8
#define ROGUE_POKEDEX_PICK_RUNS 10
#define ROGUE_ANY_POKEMON_WINS  3
#define ROGUE_NATIONAL_DEX_WINS 1
#define ROGUE_GLOBAL_DEX_WINS   3

// SECOND CHANCE: areas a run may reopen, from this many badges on.
#define ROGUE_SECOND_CHANCES      3
#define ROGUE_SECOND_CHANCE_BADGES 4

// Dungeon bosses beaten across runs that earn the EXPLORER'S KIT.
#define ROGUE_EXPLORER_DUNGEONS 5
// Different legends caught across runs that earn the MARKING MAP and the
// MASTER BALL.
#define ROGUE_MARKING_MAP_LEGENDS 3
#define ROGUE_MASTER_BALL_LEGENDS 10

// Feats: goals that last past a win, a bit each in the record's feats. The
// FEATS page of UNLOCKS lists them. What each needs and gives is in sFeats,
// src/rogue_run.c.
#define FEAT_CHAMPION     0  // beat the Champion
#define FEAT_HARDCORE     1  // win under the Hardcore rules
#define FEAT_WIN_GRASS    2  // win with the Grass starter in the team
#define FEAT_WIN_FIRE     3
#define FEAT_WIN_WATER    4
#define FEAT_FLAWLESS     5  // win without losing a Pokemon
#define FEAT_ALL_DUNGEONS 6  // clear every dungeon of a region and win
#define FEAT_LEGEND       7  // catch a legend
#define FEAT_LEGENDS_3    8  // catch three different legends
#define FEAT_LEGEND_SET   9  // catch every legend of a set (a trio, a pair)
#define FEAT_LEGENDS_10   10 // catch ten different legends
#define FEAT_ALL_LEGENDS  11 // catch every legend
#define FEAT_RUN0         12 // finish RUN 0 (any of its three endings): the fifth card star
#define ROGUE_NUM_FEATS   13

#define ROGUE_UNLOCK_PARTNERS (1 << UNLOCK_PARTNERS)
#define ROGUE_UNLOCK_DRAGONS  (1 << UNLOCK_DRAGONS)
#define ROGUE_UNLOCK_KIT      (1 << UNLOCK_KIT)
#define ROGUE_UNLOCK_VETERAN  (1 << UNLOCK_VETERAN)

// The three starter types, for the feats of a win. They match the first
// rows of the starter menu and its kinds 0 to 2.
#define ROGUE_STARTER_GRASS 0
#define ROGUE_STARTER_FIRE  1
#define ROGUE_STARTER_WATER 2

// The kinds of starter menu rows. 0 to 7 are what VAR_ROGUE_STARTER held
// before v0.13 (as the row number then), so keep them.
#define ROGUE_STARTER_KIND_PARTNER  3
#define ROGUE_STARTER_KIND_DRAGON   4
#define ROGUE_STARTER_KIND_FOSSIL   5
#define ROGUE_STARTER_KIND_FALLEN   6
#define ROGUE_STARTER_KIND_CHAMPION 7
#define ROGUE_STARTER_KIND_EEVEE    8
#define ROGUE_STARTER_KIND_BABY     9
#define ROGUE_STARTER_KIND_RISING   10
#define ROGUE_STARTER_KIND_REGIONAL 11
#define ROGUE_STARTER_KIND_DEX      12
#define ROGUE_STARTER_KIND_ANY      13
#define ROGUE_STARTER_KIND_COUNT    14

// Badges won with a type's starter still in the party that earn its
// upgrades. Eight badges means the starter reached the League.

#define ROGUE_TYPE_CHARM_BADGES  2
#define ROGUE_HIDDEN_ABILITY_BADGES 4
#define ROGUE_EGG_MOVE_BADGES    8

// Per-run flags for the unlocks. Cleared with the rest of the run.
#define FLAG_ROGUE_CAVE_0        FLAG_UNUSED_0x037 // first of 16 (to 0x046): caves explored this run
#define FLAG_ROGUE_SPARE_1       FLAG_UNUSED_0x030 // first spare starter given
#define FLAG_ROGUE_SPARE_2       FLAG_UNUSED_0x031 // second spare starter given
#define FLAG_ROGUE_FOSSIL_FOUND  FLAG_UNUSED_0x032 // the cave fossil was picked up
#define FLAG_ROGUE_FOSSIL_REVIVED FLAG_UNUSED_0x033 // the scientist revived it
#define FLAG_ROGUE_RIVAL_GIFT    FLAG_UNUSED_0x034 // the rival's gift was given
#define FLAG_ROGUE_HARDCORE      FLAG_UNUSED_0x035 // this run plays by the Hardcore rules
#define FLAG_ROGUE_BIRCH_NEWS    FLAG_UNUSED_0x036 // the professor told this run's news
#define FLAG_ROGUE_BOSS_0        FLAG_UNUSED_0x020 // first of 10 (to 0x029, ROGUE_MAX_DUNGEONS): a dungeon's boss beaten, caught or gone
#define FLAG_ROGUE_HIDEOUT_CLEARED FLAG_UNUSED_0x04D // the villain admin in the hideout is beaten: the blockade is gone
#define FLAG_ROGUE_WANDERER_0    FLAG_UNUSED_0x1DE // first of 4 (to 0x1E1): the meetings with the wanderer had this run
#define FLAG_ROGUE_POINTED       FLAG_TEMP_1D // a townsperson pointed the wanderer out on this visit (clears as the map loads)
// The meetings with the reporter pair had this run (RogueCast.reporterTowns),
// so each goes on from the last and a talk again is short.
#define FLAG_ROGUE_REPORTER_0    FLAG_UNUSED_0x1E2 // the first meeting
#define FLAG_ROGUE_REPORTER_1    FLAG_UNUSED_0x1E3 // the second
#define FLAG_ROGUE_REPORTER_2    FLAG_UNUSED_0x4AF // the third

// Item balls: one flag each, numbered through the region. These are
// vanilla's item ball flags, which nothing else sets in a generated region.
#define FLAG_ROGUE_ITEM_0   FLAG_ITEM_ROUTE_102_POTION // first of 160
// The whole party shares experience (I_EXP_SHARE_FLAG in include/config/item.h),
// so a new catch can keep up with the starter.
#define FLAG_ROGUE_EXP_SHARE FLAG_UNUSED_0x02A
// One-time gifts from the gift house: one flag per town.
#define FLAG_ROGUE_GIFT_0   FLAG_UNUSED_0x264 // first of 14
// Hidden items, numbered through the region (RogueGen_HiddenBase), and a
// townsperson's gift in some towns: vanilla's hidden item flags, which
// nothing else sets in a generated region.
#define FLAG_ROGUE_HIDDEN_0     FLAG_HIDDEN_ITEMS_START          // first of 96 (0x1F4 to 0x253)
#define FLAG_ROGUE_FOLK_GIFT_0  (FLAG_HIDDEN_ITEMS_START + 96)   // first of 14 (0x254 to 0x261), by town
// The rival leaves the route after each battle.
#define FLAG_ROGUE_RIVAL_1  FLAG_UNUSED_0x272 // first of 4
// The region map's record of the run: places visited and areas where a
// Pokemon was caught (not only met).
#define FLAG_ROGUE_VISITED_0 FLAG_UNUSED_0x280 // first of 48 (to 0x2AF), by place
#define FLAG_ROGUE_CAUGHT_0  FLAG_UNUSED_0x4B0 // first of 48 (to 0x4DF), by area
// A town signature's gift Pokemon and its small item gift (a LAVA COOKIE at
// the hot spring, MOOMOO MILK at the ranch), once per run (src/rogue_town.c).
#define FLAG_ROGUE_SIG_MON_0  FLAG_UNUSED_0x493 // first of 14 (to 0x4A0), by town
#define FLAG_ROGUE_SIG_ITEM_0 FLAG_UNUSED_0x4A1 // first of 14 (to 0x4AE), by town
// A route feature's once-per-run thing done (src/rogue_gen_features.h), by
// feature (RFEAT_*): the Day Care's or the old sailor's gift, a reward given,
// SNORLAX woken. The rest of the block is kept for more features.
#define FLAG_ROGUE_FEAT_0     FLAG_UNUSED_0x4E0 // first of 16 (to 0x4EF), by feature

// Trainers. Generated trainers replace vanilla trainer ids, which nothing
// else uses in a generated region, so they get defeat flags without growing
// the save file. Slots are numbered through the region: route and cave
// trainers first, then the fixed slots below (see src/rogue.c).
#define ROGUE_TRAINER_FIRST     TRAINER_GRUNT_SPACE_CENTER_6 // first of 244 ids in a row
#define ROGUE_TRAINER_SLOTS     244
#define ROGUE_SLOT_GYM_0        192 // 3 per gym
#define ROGUE_SLOT_LEADER_0     216 // 8
#define ROGUE_SLOT_E4_0         224 // 4
#define ROGUE_SLOT_CHAMPION     228
#define ROGUE_SLOT_RIVAL_0      229 // 4
// The villain teams' admins (at the bottom of each hideout) and leaders
// (met later): the main team's 233 and 234, the second's and the third's
// after the first 240 slots (vanilla trainer ids 829 to 832, CRISTIN_3 to
// FERNANDO_2, which nothing uses in a generated region). Their defeat
// flags are the teams' "hideout cleared" (with FLAG_ROGUE_HIDEOUT_CLEARED
// for the main team) and "leader beaten". The scripts battle 233 and 234,
// stand-ins resolved to the team of the dungeon the player is in
// (Rogue_ResolveTrainerId).
#define ROGUE_SLOT_VILLAIN_ADMIN  233 // the main villain team's admin, at the bottom of its hideout
#define ROGUE_SLOT_VILLAIN_LEADER 234 // the main villain team's leader, met later
#define ROGUE_SLOT_VILLAIN_ADMIN_1  240
#define ROGUE_SLOT_VILLAIN_LEADER_1 241
#define ROGUE_SLOT_VILLAIN_ADMIN_2  242
#define ROGUE_SLOT_VILLAIN_LEADER_2 243
// Route features' trainers (src/rogue_gen_features.h), after every route,
// cave and dungeon trainer (a region uses at most 168 of the 192; one with
// more than 182 has none of these): three double battle pairs, the battling
// family's four and the Seashore House's three.
#define ROGUE_SLOT_FEATURE_0      182
#define ROGUE_SLOT_PAIR_0         182 // 182 to 184
#define ROGUE_SLOT_FAMILY_0       185 // 185 to 188
#define ROGUE_SLOT_SEASHORE_0     189 // 189 to 191
#define ROGUE_FEATURE_TRAINERS    10
#define ROGUE_PAIRS               3
#define ROGUE_FAMILY_TRAINERS     4
#define ROGUE_SEASHORE_TRAINERS   3
#define ROGUE_SLOT_DOJO_0         235 // a dojo town's two disciples and its master (src/rogue_town.c), 235 to 237
#define ROGUE_DOJO_TRAINERS       3
#define ROGUE_SLOT_DUNGEON_MASTER 239 // stand-in for the master of the dungeon the player is in (Rogue_ResolveTrainerId)
#define ROGUE_TRAINER(slot)     (ROGUE_TRAINER_FIRST + (slot))

// RUN 0's region (include/rogue_run0_region.h): act 4, the Director deletes
// Terry, and his house is an empty lot (set by the story).
#define FLAG_ROGUE_RUN0_TERRY_DELETED FLAG_UNUSED_0x4FF

#endif // GUARD_CONSTANTS_ROGUE_H
