#ifndef GUARD_ROGUE_GEN_H
#define GUARD_ROGUE_GEN_H

// Region generator. This file and src/rogue_gen.c are plain C with no GBA
// dependencies so the generator can also be built on the host for previews
// (see tools/roguemon/gen_host.c).

#ifdef ROGUE_HOST
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef uint64_t u64;
typedef u8 bool8;
#define TRUE 1
#define FALSE 0
#else
#include "global.h"
// The generator, running in the spare time at the end of a frame, hands the
// frame back at its next YIELD once the screen has drawn this many lines.
// Its longest stretch between yields is under 20 lines, so it is done well
// before the refresh at line 160. Lines 160 to 227 are the refresh itself,
// where a frame's work starts: a frame whose work ends there still has its
// time to line 128 ahead of it.
#define ROGUE_YIELD_LINE 128
#define ROGUE_LATE_IN_FRAME() ((u16)(REG_VCOUNT - ROGUE_YIELD_LINE) < 160 - ROGUE_YIELD_LINE)
// A build for tools/roguemon/yieldprof.py, which times the stretches between
// yields, defines this (and is not for play).
// #define ROGUE_YIELD_PROFILE
#endif

// A region is a plan of places (towns, routes and caves) on a grid, joined by
// map edges or, for caves, by cave mouths. The plan is small and is built in
// one go from the seed. The block data of a place is only generated when the
// game needs it, by RogueGen_GenerateMap, from the seed and the plan alone,
// so any place can be rebuilt at any time.
//
// The main road runs from the first town through two villages' worth of
// routes to eight gym cities, then Victory Road, a cave and the summit with
// the Pokemon League. The gyms fall into chapters of one to three, won in any
// order; an obstacle on the main road (or the sea) ends each chapter, and one
// of the chapter's badges clears it. Side branches hang off the road behind
// obstacles too (Cut, Rock Smash, Strength, Surf), and one cave lies on the
// main road.
#define ROGUE_MAX_PLACES  42 // map sections are a byte, and 0xFD to 0xFF mean other things
#define ROGUE_MAX_TOWNS   14
#define ROGUE_NUM_GYMS    8
#define ROGUE_NUM_E4      4
#define ROGUE_MAX_AREAS   48 // wild encounter areas for the one-catch rule: places, then dungeons
#define ROGUE_MAX_EXITS   4  // edge connections of a place, one per side

#define ROGUE_MAX_W 48
#define ROGUE_MAX_H 48
#define ROGUE_MAX_BLOCKS 1600 // the largest map, in blocks (44x36); (w + 15) * (h + 14) must stay <= MAX_MAP_DATA_SIZE (3600)
#define ROGUE_MAX_CELLS (ROGUE_MAX_BLOCKS / 4)

#define ROGUE_MAX_OBJS  16
#define ROGUE_MAX_WARPS 12
#define ROGUE_MAX_SIGNS 20
#define ROGUE_MAX_HOUSES 6 // buildings you can enter besides the PC, Mart and gym
#define ROGUE_MAX_GATES 4

#define ROGUE_MAX_TRAINER_SLOTS 240
#define ROGUE_MAX_ROUTE_TRAINERS 192 // route, cave and dungeon trainers; gym slots follow (ROGUE_SLOT_GYM_0)
#define ROGUE_MAX_ITEM_FLAGS    160
#define ROGUE_GYM_TRAINERS      3
#define ROGUE_NAME_LEN 16

enum RogueDir
{
    RDIR_NORTH,
    RDIR_EAST,
    RDIR_SOUTH,
    RDIR_WEST,
    RDIR_NONE = 0xFF,
};

enum RoguePlaceKind
{
    RPLACE_TOWN,
    RPLACE_ROUTE,
    RPLACE_CAVE,
};

// What a route looks like. Each type has its own layout and set pieces.
enum RogueRouteType
{
    RROUTE_FOREST,   // a trail through woods
    RROUTE_LAKE,     // woods with ponds
    RROUTE_MOUNTAIN, // Victory Road: rock terraces up to a cave mouth
    RROUTE_COAST,    // the sea along one side, a beach
    RROUTE_RIVER,    // a river across the map with bridges
    RROUTE_HILLS,    // ledge terraces and cliffs
    RROUTE_FIELD,    // wide meadows of tall grass
    RROUTE_DESERT,   // sand and rock, or volcanic ash on the way to an ash town
    RROUTE_SEA,      // open water to surf, with islands
    RROUTE_WOODS,    // deep woods: a dense forest of small trees
    RROUTE_FLOWERS,  // flower beds and fenced patches, like Route 117
    RROUTE_LONGGRASS,// a maze of long grass, like Routes 119 and 120
    RROUTE_BRIDGE,   // a long raised road over water, like Route 110
    RROUTE_COUNT,
};

// Town themes. Each one borrows the houses, gym, road and decorations of a
// Hoenn town, and the game gives the map that town's tileset and music.
enum RogueTheme
{
    RTHEME_MEADOW,  // Petalburg and Oldale: the first town is always this
    RTHEME_VERDANT, // Verdanturf and Mauville
    RTHEME_HARBOR,  // Slateport
    RTHEME_ISLAND,  // Mossdeep
    RTHEME_STONE,   // Rustboro
    RTHEME_COVE,    // Lilycove
    RTHEME_BEACH,   // Dewford
    RTHEME_ASH,     // Fallarbor
    RTHEME_SPRING,  // Lavaridge
    RTHEME_TREETOP, // Fortree
    // v1.1 towns2 (src/rogue_gen_towns2.h): two kinds of town placed on their
    // own, never by AssignThemes.
    RTHEME_RAFT,    // Pacifidlog: houses on log rafts out on the sea
    RTHEME_CRATER,  // Sootopolis: a town in a ring of white cliffs round a lake
    RTHEME_COUNT,
};
// v1.1 sea: a theme only a sea route has, for Pacifidlog's tileset (the one
// with currents, src/rogue_gen_sea.h). No town has it; tables of the town
// themes stop before it.
#define RTHEME_CURRENTS RTHEME_COUNT

// What a town is for.
enum RogueTownRole
{
    RTOWN_START,   // the first town, with the professor's lab
    RTOWN_VILLAGE, // no gym
    RTOWN_GYM,     // a gym city
    RTOWN_HAMLET,  // a small town at the end of a side branch
    RTOWN_SUMMIT,  // the Pokemon League
};

// How a town is laid out.
enum RogueTownLayout
{
    RLAYOUT_STREETS, // houses along one to three streets
    RLAYOUT_PLAZA,   // a paved square in the middle, the big buildings facing it
    RLAYOUT_HARBOR,  // on the sea, with a beach along the waterfront
};

// What a place is part of. Side places (side = TRUE) are branches,
// shortcuts or forks.
enum RogueBranch
{
    RBRANCH_MAIN,     // the main road
    RBRANCH_SIDE,     // a dead-end side path: a grove, a dark cave, a hamlet
    RBRANCH_SHORTCUT, // a way back from a later stretch to an earlier town, behind an obstacle
    RBRANCH_FORK,     // a second way between two places of one stretch, behind an obstacle
};

// What blocks a way until a badge clears it.
// Which badge clears which is rolled per region (RogueRegion.gymKey).
enum RogueObstacle
{
    ROBST_NONE,
    ROBST_CUT,        // small trees
    ROBST_ROCK_SMASH, // cracked rocks
    ROBST_STRENGTH,   // a boulder to push
    ROBST_SURF,       // water to cross
};

// Keys: what opens a way. A badge gives at most one of the four field moves
// that clear obstacles; the League's gate needs all eight badges. The villain
// team's blockade opens once the admin in their hideout is beaten.
#define ROGUE_KEY(obstacle) (1 << (obstacle))
#define ROGUE_KEY_LEAGUE    (1 << 5)
#define ROGUE_KEY_VILLAIN   (1 << 6)
// A region's tunnel (RogueRegion.tunnel) behind a key a gym leader hands
// over: held once that gym is won (RogueGen_KeysWon).
#define ROGUE_KEY_TUNNEL    (1 << 7)
#define ROGUE_KEYS_ALL      (ROGUE_KEY(ROBST_CUT) | ROGUE_KEY(ROBST_ROCK_SMASH) | ROGUE_KEY(ROBST_STRENGTH) \
                             | ROGUE_KEY(ROBST_SURF) | ROGUE_KEY_LEAGUE | ROGUE_KEY_VILLAIN)

// RogueLink.gate values.
#define ROGUE_GATE_STARTER 1
#define ROGUE_GATE_LEAGUE  (1 + ROGUE_NUM_GYMS)
#define ROGUE_GATE_VILLAIN 10 // the villain team's grunts, until their admin is beaten

enum RogueObjKind
{
    ROBJ_PROFESSOR, // unused: the professor waits in his lab (rogue_town.c)
    ROBJ_TRAINER,
    ROBJ_ITEM,
    ROBJ_VILLAGER,
    ROBJ_BERRY, // berry tree on a soil patch
    ROBJ_MON,   // a townsperson's Pokemon, out for a walk
    ROBJ_RIVAL, // slot is which rival battle
    ROBJ_CUT_TREE,
    ROBJ_ROCK,     // Rock Smash
    ROBJ_BOULDER,  // Strength
    ROBJ_BOSS,     // a dungeon's boss: slot is the dungeon (see enum RogueBoss)
    // Dungeon extras (RogueGen_DungeonExtras). Their slot is the item flag
    // index itself, not one relative to the floor's items.
    ROBJ_PRIZE,    // the item ball beside the boss
    ROBJ_CHOICE,   // one of a choice room's item balls: taking one makes the others vanish
    ROBJ_CAMP,     // a camper who heals the team once per run; the item flag marks it used
    ROBJ_GRUNT,    // a villain grunt standing in the way out of the main team's hideout's town: slot 0 or 1; 2 by a hideout's door (its team's)
    ROBJ_VILLAIN_LEADER, // a villain team's leader, met after its hideout: slot is the team (see Rogue_TeamLeaderReady)
    ROBJ_GUIDE,    // beside a choice room's balls: says only one may be taken, as Mt. Moon's fossil maniac
    ROBJ_SEAL,     // what keeps a legend's chamber shut (enum RogueLegendGate): slot is the dungeon
    ROBJ_EXPLORER, // someone lost deep in a dungeon, who gives a reward for being led out: slot is the item flag
    ROBJ_KEY,      // a shipwreck's cabin key in an item ball: slot is its item flag (RogueGen_KeyItem)
    ROBJ_DECOY,    // an item ball that is a Voltorb, as New Mauville's: slot is its item flag
    ROBJ_TUNNEL_GUARD, // in a tunnel's narrow way until its key is held (RogueRegion.tunnelGate): slot 0 a Hiker, 1 a grunt
    ROBJ_SIGFOLK,  // one of the town's signature people (fishermen...): slot is SIGROLE_* | look << 2
    ROBJ_SIGMON,   // a signature Pokemon about town (a ranch's Miltank): slot picks it from the signature's
    // Route features (src/rogue_gen_features.h).
    ROBJ_FEATFOLK, // one of the route feature's people: slot is FEATROLE_*
    ROBJ_FEATMON,  // a feature's Pokemon (the Day Care's, the old sailor's Wingull): slot picks it
    ROBJ_PAIR,     // one of a double battle pair (TWINS...): slot is the pair (0-2), + 4 for the second of the two
    ROBJ_SNORLAX,  // a SNORLAX asleep across a way into a pocket of the route
    ROBJ_RUN0,     // RUN 0: one of the story's things (slot is enum RogueRun0Hook, include/rogue_run0_region.h)
};

// What a route feature's person does (RogueObj.slot of ROBJ_FEATFOLK).
enum
{
    FEATROLE_TALK,  // talks about the feature
    FEATROLE_HOST,  // the feature's host: the gift, the reward
    FEATROLE_KID,   // the child by the sleeping SNORLAX, with the POKe FLUTE
    FEATROLE_PRIZE, // at the end of the bridge gauntlet, with the prize
};

// What a signature person does (RogueObj.slot & 3).
enum
{
    SIGROLE_TALK, // talks about the town's trade
    SIGROLE_GIFT, // gives the town's gift Pokemon, if it has one (else talks)
    SIGROLE_ITEM, // the town's small thing: a soak, a bottle of milk, a stall
    SIGROLE_POST, // stands at a post (a miner in the mine's mouth, a fisher on a pier)
};

// Warp slots are fixed so interiors can warp back to a known index.
enum RogueWarpKind
{
    RWARP_PC,
    RWARP_MART,
    RWARP_HOUSE, // the first house; the others follow RWARP_CAVE
    RWARP_GYM,   // the League in the summit town
    RWARP_CAVE,  // a cave mouth
    // RWARP_CAVE + 1 to + 5: the other houses
    // On a route, which has no houses but a rest house: the right-hand
    // block of a two-block way into a forest (RWARP_DUNGEON, RWARP_DUNGEON_2).
    RWARP_DUNGEON_WIDE = 8,
    RWARP_DUNGEON_2_WIDE = 9,
    RWARP_DUNGEON = 10,   // the way into a dungeon's floors from a town or route
    RWARP_DUNGEON_2 = 11, // a dungeon's second way in or out
};
#define ROGUE_HOUSE_WARP(k) ((k) == 0 ? RWARP_HOUSE : RWARP_CAVE + (k))

// What is inside each house of a town.
enum RogueInterior
{
    RINT_HOME,
    RINT_LAB,        // the professor's lab in the first town
    RINT_SCHOOL,     // Rustboro's Pokemon school
    RINT_FAN_CLUB,
    RINT_BIKE_SHOP,
    RINT_NAME_RATER, // the Name Rater's house
    RINT_MOVE_DELETER, // Lilycove's Move Deleter
    RINT_REST,         // a rest house on a route, where the team can heal
    RINT_SIGNATURE,    // a town's signature landmark (src/rogue_gen_signatures.h): its vanilla interior
    RINT_FEATURE,      // a route feature's house (src/rogue_gen_features.h): the Day Care, the Seashore House...
    RINT_COUNT,
};

// Warp slots inside a cave.
enum RogueCaveWarp
{
    RCAVE_SOUTH,  // the way in
    RCAVE_NORTH,  // the way out, if the cave goes through
    RCAVE_DOWN,   // a ladder down to the dungeon's first floor below
    RCAVE_DOWN_2, // a second ladder down
};

// Warp slots on a dungeon floor (a RogueFloor map).
enum RogueFloorWarp
{
    RFLOOR_UP,     // to the floor above, or out to the host place
    RFLOOR_DOWN,   // to the floor below
    RFLOOR_UP_2,   // a second way up
    RFLOOR_DOWN_2, // a second way down
    RFLOOR_EXIT,   // out to the dungeon's exit place, if it goes through
    RFLOOR_WARPS,
    // RFLOOR_WARPS + one of the above: the second block of a way two blocks
    // wide (a forest's), leading to the second block at the other end.
};

enum RogueSignKind
{
    RSIGN_TOWN,
    RSIGN_ROUTE,
    RSIGN_PC,
    RSIGN_MART,
    RSIGN_GYM,
    RSIGN_LOCKED,
    RSIGN_HOUSE, // name plate beside a house
    RSIGN_LAB,
    RSIGN_GAME_CORNER, // landmark buildings that stay shut
    RSIGN_BATTLE_TENT,
    RSIGN_MOTEL,
    RSIGN_REST,  // the sign beside a rest house
    // Signs beside special houses, naming them.
    RSIGN_SCHOOL,
    RSIGN_FAN_CLUB,
    RSIGN_BIKE_SHOP,
    RSIGN_NAME_RATER,
    RSIGN_MOVE_DELETER,
    RSIGN_TUTOR, // the move tutor's home
    RSIGN_GIFT,  // the home with a gift for trainers
    RSIGN_DUNGEON, // beside the way into a town's tower or hideout, naming it
    RSIGN_FOREST,  // beside a route's way into the woods, naming them
    RSIGN_CARVING, // old words on a wall of a legend's lair
    RSIGN_TABLET,  // one of the three tablets of a ruins vault's riddle door (left to right)
    RSIGN_LANDMARK, // beside a town's signature landmark, naming it
    RSIGN_FEATURE,  // beside a route feature, naming it (src/rogue_gen_features.h)
    // RUN 0 (include/rogue_run0_region.h): a PROMPT LOG sign post, RSIGN_RUN0 + the
    // prompt (RUN0_HOOK_PROMPT_0 + k), and a real Emerald sign moved onto a
    // generated route, RSIGN_RUN0_REAL + RUN0_SIGN_*.
    RSIGN_RUN0 = 0xC0,
    RSIGN_RUN0_REAL = 0xC8,
    // A shipwreck's locked cabin door: RSIGN_LOCK + k is locked by the
    // dungeon's k-th key (RogueDungeonExtras.keys + k is its item flag).
    RSIGN_LOCK = 0xE0,
    // Hidden items, Emerald's: RSIGN_HIDDEN + k is the place's k-th
    // (RogueGen_HiddenBase). Not signs at all, but bg events like them.
    RSIGN_HIDDEN = 0xF0,
    // The tunnel's hidden item by its far mouth (RogueGen_TunnelHidden), kept
    // after every place's so no other item moves.
    RSIGN_HIDDEN_TUNNEL = 0xFF,
};

// What the people in an ordinary home do (RogueGen_HomeRole).
enum RogueHomeRole
{
    RHOME_CHAT,
    RHOME_GIFT,
    RHOME_RELEARNER,
    RHOME_NAME_RATER,
};

// ---------------------------------------------------------------------------
// Dungeons
//
// A dungeon is a set of floors reached from a host place: a cave (whose own
// map is the dungeon's ground floor), a town (through a building) or a route
// (through an entrance). The floors are RogueFloor maps that are not places,
// so they need no grid cell or map section: they use their host's. Floors are
// numbered from 1, the floor next to the host.

// Boss flags (FLAG_ROGUE_BOSS_0) cover ten.
#define ROGUE_MAX_DUNGEONS 10
#define ROGUE_MAX_FLOORS   5

enum RogueDungeonKind
{
    RDUN_CAVE,    // floors below a cave: ladders, dark corners, a guardian at the bottom
    RDUN_VICTORY, // Victory Road's cave, with floors the way on goes through
    RDUN_FOREST,  // a deep forest off a route, like Petalburg Woods; or across it (exit == host)
    RDUN_TOWER,   // a tower in a town, climbed to a top floor, like Mt. Pyre
    RDUN_HIDEOUT, // the villain team's hideout under a town
    RDUN_LAIR,    // a legend's own chamber, one vanilla room, through a door in a town's rock face
    RDUN_RUINS,   // ruins under a side cave in sand or hills: shifting floors, sand pits, a riddle door, a guardian
    RDUN_SHIP,    // a shipwreck on a sea route, the Abandoned Ship's rooms: cabin keys, a ghost at the bottom
    RDUN_PLANT,   // a power plant below a side cave, New Mauville's hall: switches, barriers, Voltorb balls
    RDUN_ICE,     // an ice cave late in the run, like Shoal Cave's ice room: sliding puzzles, boulders
    RDUN_VOLCANO, // the region's volcano, like Fiery Path: ash, boulders, a lava lake at the bottom
    RDUN_MIRAGE,  // (v1.1 peaks) a tower in a sandstorm desert, the Mirage Tower: sand pits, fossils at the top
    RDUN_COUNT,
};

// What waits at the end of a dungeon.
enum RogueBoss
{
    RBOSS_NONE,
    RBOSS_GUARDIAN, // a strong wild Pokemon, boosted like a totem; catching it uses no area
    RBOSS_MASTER,   // a strong trainer who lives there
    RBOSS_ADMIN,    // the villain team's admin
    RBOSS_LEGEND,   // a legendary Pokemon
};

struct RogueDungeon
{
    u8 kind;     // enum RogueDungeonKind
    u8 host;     // place it is entered from
    u8 exit;     // place the last way out leads to (RFLOOR_EXIT), or 0xFF; the host for a
                 // forest the road goes through, out to the host's other half (RWARP_DUNGEON_2)
    u8 nFloors;  // RogueFloor maps, 1 to ROGUE_MAX_FLOORS
    u8 boss;     // enum RogueBoss, on the last floor
    u8 required; // the main road goes through it
    u8 level;    // trainers' level on the first floor; deeper floors are harder
    u8 badges;   // gyms passed when it first opens (as RoguePlace.badges)
    u8 area;     // wild encounter area (a cave's own)
    u8 trainerBase, nTrainers; // all floors, boss included (the last)
    u8 itemBase, nItems;
    u8 legend;   // enum RogueLegend of an RBOSS_LEGEND boss
    u8 gate;     // how a legend's chamber opens: ROGUE_GATE_KIND (enum RogueLegendGate) and ROGUE_GATE_SRC
};

// How the way to a legend opens, as in the real games (src/rogue_gen_legend.h).
// A legend's dungeon ends in its chamber, a vanilla legend's room; what
// keeps it shut stands at the narrow place on the walk in (ROBJ_SEAL), or
// where the legend appears once it opens.
enum RogueLegendGate
{
    RGATE_OPEN,    // the dungeon itself is the trial, as the birds at the bottom of Seafoam
    RGATE_FIELD,   // a cracked rock or a boulder: src is ROBST_ROCK_SMASH or ROBST_STRENGTH
    RGATE_RITUAL,  // old words say which move to use, as the Regis' braille: src is RRITUAL_*
    RGATE_GIFT,    // a key a gym leader hands over with the badge: src is the gym
    RGATE_FOUND,   // a key in the prize ball of another dungeon: src is that dungeon
    RGATE_VILLAIN, // the villain team's leader stands in the way, after the hideout
    RGATE_AFTER,   // appears once the rest of its set are met, as Regigigas
};
enum { RRITUAL_FLY, RRITUAL_DIVE, RRITUAL_WATERFALL };
#define ROGUE_GATE(kind, src) ((kind) | ((src) << 4))
#define ROGUE_GATE_KIND(gate) ((gate) & 0x0F)
#define ROGUE_GATE_SRC(gate)  ((gate) >> 4)

// The legends a region may have (src/rogue_gen_legend.h). The game maps
// them to species.
enum RogueLegend
{
    RLEG_NONE,
    RLEG_REGIROCK, RLEG_REGICE, RLEG_REGISTEEL, RLEG_REGIGIGAS,
    RLEG_ARTICUNO, RLEG_ZAPDOS, RLEG_MOLTRES,
    RLEG_RAIKOU, RLEG_ENTEI, RLEG_SUICUNE,
    RLEG_UXIE, RLEG_MESPRIT, RLEG_AZELF,
    RLEG_COBALION, RLEG_TERRAKION, RLEG_VIRIZION,
    RLEG_TORNADUS, RLEG_THUNDURUS, RLEG_LANDORUS,
    RLEG_LATIAS, RLEG_LATIOS,
    RLEG_HEATRAN, RLEG_CRESSELIA,
    // Box-art legends: only once the League's road is open.
    RLEG_MEWTWO, RLEG_LUGIA, RLEG_HO_OH, RLEG_KYOGRE, RLEG_GROUDON, RLEG_RAYQUAZA,
    RLEG_COUNT,
};

// ---------------------------------------------------------------------------
// The plan

struct RogueLink
{
    u8 dir;     // side of this place
    u8 target;  // place on the other side
    u8 pos;     // block coordinate of the opening along this side
    u8 gate;    // ROGUE_GATE_*: a starter, all eight badges or the villains' blockade (0: none)
    s16 offset; // engine connection offset
};

struct RoguePlace
{
    u8 kind;     // enum RoguePlaceKind
    u8 type;     // routes: enum RogueRouteType; towns: enum RogueTownRole
    u8 theme;    // enum RogueTheme; for routes and caves, the theme whose tileset they use
    u8 number;   // towns: town number; routes: route number shown; caves: cave number
    u8 w, h;     // in blocks
    s8 gridX, gridY;
    u8 gym;      // gym number, or 0xFF
    u8 tier;     // difficulty step for items, music and species strength: 0 to 10
    u8 level;    // level of the trainers here; wild Pokemon are a little lower
    u8 badges;   // main road: gyms passed along it; side places: those passed, winning them in
                 // order, when the way in opens
    u8 area;     // wild encounter area, or 0xFF
    u8 trainerBase, nTrainers;
    u8 itemBase, nItems;
    u8 berryBase, nBerries;
    u8 rival;    // rival battle here (1 based), or 0
    u8 side;     // part of a side branch
    u8 obstacle; // enum RogueObstacle: what blocks the way in from guardDir
    u8 guardDir; // side the obstacle guards
    u8 mouth;    // cave reached through a cave mouth in this place, or 0xFF
    u8 caveIn, caveOut; // caves: the place at the way in and the way out (0xFF: a dead end)
    u8 dark;     // caves: dark until the second badge; towns: ROGUE_TOWER_MOVED where the region's
                 // tower was planned before it moved out into the desert (v1.1 peaks)
    u8 interiors; // towns: special interiors this town may hold (bit per enum RogueInterior)
    u8 seaSide;   // coast routes and harbor towns: the side the sea is on, or RDIR_NONE
    u8 layout;    // towns: enum RogueTownLayout
    u8 nLinks;
    u8 branch;    // enum RogueBranch: what this place is part of
    struct RogueLink links[ROGUE_MAX_EXITS];
};

struct RogueRegion
{
    u32 seed;
    u8 nPlaces;
    u8 nTowns;
    u8 nAreas;
    u8 nTrainerSlots; // route and cave trainers; gym, leader and story slots follow
    u8 nItems;
    u8 nBerries;
    u8 start, summit, victoryRoad; // places
    // The level curve, rolled per region around Emerald's.
    u8 aceLevel[ROGUE_NUM_GYMS];   // each gym leader's strongest Pokemon
    u8 e4Level[ROGUE_NUM_E4];
    u8 championLevel;
    u8 rivalLevel[4];
    u8 towns[ROGUE_MAX_TOWNS];     // place of each town number
    u8 gymTown[ROGUE_NUM_GYMS];    // place of each gym
    u8 gymKey[ROGUE_NUM_GYMS];     // enum RogueObstacle the gym's badge clears, or ROBST_NONE
    u8 gymChapter[ROGUE_NUM_GYMS]; // chapter of each gym: those of one chapter are won in any order
    u8 nChapters;
    u8 townNames[ROGUE_MAX_TOWNS][ROGUE_NAME_LEN + 1]; // ASCII, converted by the game
    struct RoguePlace places[ROGUE_MAX_PLACES];
    u8 nDungeons;
    struct RogueDungeon dungeons[ROGUE_MAX_DUNGEONS];
    u8 land; // which land shape of the seed the plan stands on (see PlanLand)
    // A tunnel (src/rogue_gen_tunnel.h): an optional dungeon whose last floor
    // before any chamber has a second way out (RFLOOR_EXIT) to its exit place,
    // a road place several gyms later, behind a barrier on that floor.
    u8 tunnel;     // the dungeon, or 0xFF
    u8 tunnelGate; // ROGUE_GATE(RGATE_FIELD, obstacle), (RGATE_GIFT, gym) or (RGATE_VILLAIN, 0)
};

// ---------------------------------------------------------------------------
// A generated map

struct RogueObj
{
    u8 kind;
    u8 x, y;
    u8 facing; // enum RogueDir in bits 0-1; trainers may add ROBJ_TURNS or ROBJ_LOOKS
    u8 slot;   // trainer slot, item slot or villager variant
    u8 range;  // trainer sight range
};

// A trainer that turns between its facing and a second one, (facing >> 2) & 3,
// or looks all round, as Emerald's FACE_X_AND_Y and LOOK_AROUND trainers do.
#define ROBJ_TURNS 0x10
#define ROBJ_LOOKS 0x20

struct RogueWarp
{
    u8 x, y;
    u8 kind;
};

struct RogueSign
{
    u8 x, y;
    u8 kind;
};

// A coord trigger that turns the player back until they meet a requirement.
struct RogueGate
{
    u8 x, y;
    u8 dir;   // direction the player is walking when they hit the gate
    u8 need;  // RogueLink.gate (ROGUE_GATE_*)
};

struct RogueMap
{
    u8 place;   // the place, or for a dungeon floor its host
    u8 dungeon; // dungeon floors: the dungeon
    u8 floor;   // dungeon floors: 1 to nFloors; 0 for a place's own map
    u8 w, h;
    u16 *blocks;
    u8 nObjs, nWarps, nSigns, nGates;
    u8 nHouses;
    u8 interiors[ROGUE_MAX_HOUSES]; // enum RogueInterior of each house
    struct RogueObj objs[ROGUE_MAX_OBJS];
    struct RogueWarp warps[ROGUE_MAX_WARPS];
    struct RogueSign signs[ROGUE_MAX_SIGNS];
    struct RogueGate gates[ROGUE_MAX_GATES];
    u8 startX, startY;
};

// RUN 0 (src/rogue_gen_run0.h, include/rogue_run0_region.h). The region of
// ROGUE_RUN0_SEED, changed by hand; the game (and gen_host) say which act the
// story is in, how often the changing maps were entered and the story's state.
#include "rogue_run0_region.h"
u8 RogueGen_Run0Act(void);
u8 RogueGen_Run0Visit(u8 which);
u16 RogueGen_Run0State(void);
// Where the story's hooks are (enum RogueRun0Hook): the place, or -1.
s32 RogueGen_Run0HookPlace(const struct RogueRegion *r, u8 hook);
// The graveyard tower (a dungeon), or -1.
s32 RogueGen_Run0Graveyard(const struct RogueRegion *r);
// What a house of a town map is for: a hook, RUN0_HOUSE_ISLAND + an island
// whose door it is, or -1 (an ordinary home, or not RUN 0).
s32 RogueGen_Run0HouseHook(const struct RogueRegion *r, const struct RogueMap *m, u8 house);
// The town holding an island's door, the town laid out again on each entry,
// the copy of Route 101 and the route that loops into itself (or -1).
s32 RogueGen_Run0IslandTown(const struct RogueRegion *r, u8 island);
s32 RogueGen_Run0RelayTown(const struct RogueRegion *r);
s32 RogueGen_Run0CopyRoute(const struct RogueRegion *r);
s32 RogueGen_Run0LoopRoute(const struct RogueRegion *r);
// Act 4: where Petalburg's gym stands in the water, and the route
// Littleroot's houses are copied down.
void RogueGen_Run0CutPlaces(const struct RogueRegion *r, s32 *gym, s32 *houses);
// After STOP: where the moving truck parks in a first town (bottom middle of
// its picture). FALSE if nowhere fits.
bool8 RogueGen_TruckSpot(const struct RogueMap *m, s32 *x, s32 *y);
// The shortcut whose obstacles TERRY clears in act 3, or -1.
s32 RogueGen_Run0ShortcutPlace(const struct RogueRegion *r);
// The region kept in RUN 0's CHOOSE carries the player's name: for that seed,
// the name in ASCII into out and TRUE (the game's, or gen_host's
// ROGUE_KEPT_REGION and ROGUE_KEPT_NAME). The text says it where it says the
// region's name.
bool8 RogueGen_KeptRegionName(u32 seed, char *out);

// Builds the plan for a seed.
void RogueGen_BuildRegion(struct RogueRegion *region, u32 seed);
// v1.1 towns2 (src/rogue_gen_towns2.h): a raft village out on the sea
// (Pacifidlog's tileset) and a town in a crater of white cliffs round a lake
// (Sootopolis's).
bool8 RogueGen_IsRaftTown(const struct RoguePlace *p);
bool8 RogueGen_IsCraterTown(const struct RoguePlace *p);
// Generates the blocks and events of one place into map and blocks (at least
// ROGUE_MAX_BLOCKS long).
void RogueGen_GenerateMap(const struct RogueRegion *region, u8 place, struct RogueMap *map, u16 *blocks);

// Generates floor (1 to nFloors) of a dungeon. The map's place is the host.
void RogueGen_GenerateFloor(const struct RogueRegion *region, u8 dungeon, u8 floor, struct RogueMap *map, u16 *blocks);
// Forgets a generation stopped part way (the game dropped its job).
void RogueGen_DropJob(void);
// The dungeon entered from place (its host or its exit), or -1.
s32 RogueGen_DungeonAt(const struct RogueRegion *r, u8 place);
// The floor whose RFLOOR_EXIT leads out to a dungeon's exit place, and that
// a warp in from the exit place leads to: the last floor before any
// legend's chamber.
u8 RogueGen_ExitFloor(const struct RogueRegion *r, u8 dungeon);
// Tunnels (src/rogue_gen_tunnel.h). TRUE if place has a tunnel's far mouth
// (found or not), for the region map's marks.
bool8 RogueGen_TunnelMouthAt(const struct RogueRegion *r, u8 place);
// The badges held when the region's tunnel first opens, winning the gyms
// in order along the road (0xFF if it has none).
u8 RogueGen_TunnelOpensAt(const struct RogueRegion *r);
// The hidden item index (from FLAG_ROGUE_HIDDEN_0) by the tunnel's far
// mouth, or ROGUE_MAX_HIDDEN for none.
u8 RogueGen_TunnelHidden(const struct RogueRegion *r);
// The level of the main road where a stretch begins: after this many gyms.
u8 RogueGen_StretchLevel(const struct RogueRegion *r, u8 badges);
u8 RogueGen_FloorLevel(const struct RogueRegion *r, u8 dungeon, u8 floor);
// A place standing for one floor of a dungeon, for the code that sizes,
// levels and fills places: a cave (or route) at that floor's level, with that
// floor's share of the dungeon's trainers and items.
void RogueGen_FloorPlace(const struct RogueRegion *r, u8 dungeon, u8 floor, struct RoguePlace *out);
// A floor's wild encounter rate (0 for none: the hideout, a legend's chamber).
u8 RogueGen_FloorWildRate(const struct RogueRegion *r, u8 dungeon, u8 floor);
// The lowest level of the wild Pokemon of a place (or floor) of this level:
// a few under its trainers, more so late in a run, as in Emerald.
u8 RogueGen_WildLevel(u8 placeLevel);
// Levels more for a land slot (0 to 11) over a table's wild level: late, the
// rarer slots are a little higher, as Emerald's evolved ones are.
u8 RogueGen_RareSlotBonus(u8 slot, u8 wildLevel);
// The floor a dungeon trainer slot stands on (1 based), or 0 if it is not one.
u8 RogueGen_FloorOfTrainer(const struct RogueRegion *r, u8 dungeon, u8 slot);
// Towers and hideouts are built from vanilla rooms (src/rogue_gen_rooms.h):
// the room a floor is (an index into the room library), or 0xFF for a floor
// of another kind. A room floor's warp kinds are the room's warp indexes.
u8 RogueGen_FloorRoom(const struct RogueRegion *r, u8 dungeon, u8 floor);
// Where warp of a room floor leads: toFloor 0 is out to the host place,
// else the floor and its warp.
void RogueGen_RoomWarpDest(const struct RogueRegion *r, u8 dungeon, u8 floor, u8 warp, u8 *toFloor, u8 *toWarp);
// (v1.1 peaks) The warp of a room floor's stairs on (for the way back down
// onto it from a generated floor above).
u8 RogueGen_RoomOnWarp(const struct RogueRegion *r, u8 dungeon, u8 floor);
// Which vanilla hideout the villains' hideout is built from (ROOM_STYLE_AQUA
// or ROOM_STYLE_MAGMA in src/data/rogue_rooms.h).
u8 RogueGen_HideoutStyle(const struct RogueRegion *r, u8 dungeon);

// What a dungeon holds besides its trainers and ordinary items: a prize ball
// beside the boss, a choice room of two or three balls (like Mt. Moon's
// fossils) and a camp where a camper heals the team once. Each takes item
// flags at the end of the dungeon's (itemBase + nItems - total on), so a
// floor's own items are nItems - total shared over the floors. Slots are
// item flag indexes, or 0xFF when the dungeon has none.
struct RogueDungeonExtras
{
    u8 prize;                // on the last floor, beside the boss
    u8 choice, nChoice, choiceFloor;
    u8 camp, campFloor;
    u8 explorer, explorerFloor; // someone lost on a deep floor (src/rogue_gen_ruins.h); the flag marks them led out
    u8 keys, nKeys;          // a shipwreck's cabin keys, floor by floor (RogueGen_KeyItem)
    u8 decoys, nDecoys;      // a power plant's Voltorb balls
    u8 total;                // item flags the extras take
};
void RogueGen_DungeonExtras(const struct RogueRegion *r, u8 dungeon, struct RogueDungeonExtras *e);
// Ruins (src/rogue_gen_ruins.h). Their floors are shuffled again on every
// visit: the game counts visits (VAR_ROGUE_SEALS' high bits), the host
// generator takes ROGUE_VISIT. Both define this.
u8 RogueGen_DungeonVisit(void);
// The warp of a ruins floor a fall from the floor above lands on, or 0xFF.
u8 RogueGen_RuinsLandWarp(const struct RogueRegion *r, u8 dungeon, u8 floor);
// The riddle of a ruins vault's door: the word on each tablet, left to
// right (an index into the game's word list, RUINS_WORDS of them), and the
// order they are to be read in (tablet indexes).
#define RUINS_WORDS 8
void RogueGen_RuinsRiddle(const struct RogueRegion *r, u8 dungeon, u8 words[3], u8 order[3]);
// The region's ruins dungeon, or -1.
s32 RogueGen_RuinsDungeon(const struct RogueRegion *r);
// (v1.1 peaks, src/rogue_gen_peaks.h) The floor of a tower that is out on
// the misty mountainside below its summit (a generated floor, not a room),
// or 0; and the region's desert tower (RDUN_MIRAGE), or -1.
u8 RogueGen_PyreSlopeFloor(const struct RogueRegion *r, u8 dungeon);
#define ROGUE_TOWER_MOVED 2
s32 RogueGen_MirageDungeon(const struct RogueRegion *r);
// The first dungeon whose boss is a legend (RBOSS_LEGEND), or -1.
s32 RogueGen_LegendDungeon(const struct RogueRegion *r);
// How many legends the region has.
u8 RogueGen_LegendCount(const struct RogueRegion *r);
// The badges held when a legend's way opens (its level follows them), the
// key item that opens it (ITEM_NONE for none), and whether a dungeon ends
// in a legend's chamber.
u8 RogueGen_LegendGateBadges(const struct RogueRegion *r, u8 dungeon);
u16 RogueGen_LegendKey(u8 legend);
bool8 RogueGen_HasChamber(const struct RogueDungeon *d);
// A shipwreck's cabin keys (src/rogue_gen_rooms.h): the key item a key ball
// (an item flag index) holds, or ITEM_NONE if it is no key.
u16 RogueGen_KeyItem(const struct RogueRegion *r, u8 dungeon, u8 slot);
u8 RogueGen_SealAtLegend(const struct RogueRegion *r, u8 dungeon);
u8 RogueGen_LegendSetOf(u8 legend);
// The legend's dungeon the villain team's leader waits in, before the
// legend, or -1.
s32 RogueGen_LeaderDungeon(const struct RogueRegion *r);
// The main-road cave before the fourth gym, which holds the FOSSILS unlock's
// fossil, or -1.
s32 RogueGen_FossilCave(const struct RogueRegion *r);
// LATE CAMP: the main road route between the 6th and 7th gyms that has a
// camper (the rival's, if one has it), or -1.
s32 RogueGen_RouteCamp(const struct RogueRegion *r);
// The item flag (an index from FLAG_ROGUE_ITEM_0) of the camper on a place's
// own map by the road: Victory Road's, just past the rival, and LATE
// CAMP's. They take the region's last item flags. -1 for other places.
s32 RogueGen_RoadCampFlag(const struct RogueRegion *r, u32 place);

// Legend lore (src/rogue_gen_lore.h): why each legend lives where it does,
// told two ways a legend, in ASCII ('@' the e of POKéMON, '<' '>' the curly
// quotes, '\n' a line break, '\v' a scroll, '\f' a new box). The story's
// ways come first.
enum RogueLoreWay
{
    LORE_WAY_CARVING, // the old words in its chamber
    LORE_WAY_BOOK,    // a book on the shelf of its town's POKéMON CENTER
    LORE_WAY_ELDER,   // an old person at home in its town, once the way could open
    LORE_WAY_LAB,     // PROF. BIRCH's aide in the first town
    LORE_WAY_LEADER,  // the gym leader who hands over its key
    LORE_WAY_VILLAIN, // the villain leader, after the battle before it
    LORE_WAY_FOLK,    // a townsperson in its town (a hint)
    LORE_WAY_SIGN,    // its town's sign (a hint)
    LORE_WAY_TV,      // the news on a house's TV (a hint)
    LORE_WAY_SHRINE,  // words added under the name on a lair's sign (a hint)
    LORE_WAY_DIARY,   // a diary page under the prize ball of another dungeon (a story)
    LORE_WAY_GRAVE,   // an old keeper's grave on a floor of a tower (a story)
    LORE_WAY_MUSEUM,  // an exhibit in its town's museum, else a plaque in its POKéMON CENTER (a story)
    LORE_WAY_RHYME,   // a child's skipping rhyme in its town's POKéMON CENTER (a hint)
    LORE_WAY_PAINTING, // a painting in a home of its town, else on its POKéMON CENTER's wall (a hint)
    LORE_WAYS,        // 15 at most: a way is kept in a nibble
};
// What a legend is like, and what its home is like: a legend is at home
// where the two agree (Articuno in an ice cave), and needs a reason where
// they do not (Zapdos below a volcano).
enum
{
    LELEM_FIRE,
    LELEM_ICE,
    LELEM_WATER,
    LELEM_STORM,
    LELEM_SKY,
    LELEM_STONE,
    LELEM_GRASS,
    LELEM_MIND,
    LELEM_COUNT,
};
enum { LORE_STORY, LORE_HINT, LORE_MOTTO, LORE_LATER, LORE_VILLAIN_WHY };
struct RogueLore
{
    u8 story, hint; // enum RogueLoreWay, 0xFF for a dungeon without a legend
    u8 town;        // the town it is told of in
    u8 home, homeElem, elem, fits; // its home's kind and nature, its own, and if they agree
};
void RogueGen_LegendLore(const struct RogueRegion *r, u8 dungeon, struct RogueLore *lore);
bool8 RogueGen_LegendFeared(const struct RogueRegion *r, u8 dungeon);
// The legend's dungeon a way tells of at a place (a town; a gym for the
// leader; for LORE_WAY_DIARY and LORE_WAY_GRAVE the dungeon that holds the
// page or the grave), or -1.
s32 RogueGen_LoreAt(const struct RogueRegion *r, u8 way, u8 place);
// The dungeon whose prize ball holds a legend's diary page, or whose floor
// holds its keeper's grave (LORE_WAY_DIARY, LORE_WAY_GRAVE), or 0xFF.
u8 RogueGen_LoreHolder(const struct RogueRegion *r, u8 dungeon);
// The floor of a tower whose graves hold the keeper's (LORE_WAY_GRAVE), or 0.
u8 RogueGen_GraveFloor(const struct RogueRegion *r, u8 dungeon);
// TRUE where a town has a museum (the town signatures build one): a
// museum exhibit tells LORE_WAY_MUSEUM there, else a plaque in the
// POKéMON CENTER does.
bool8 RogueGen_TownHasMuseum(const struct RogueRegion *r, u8 place);
// The room (an index of src/rogue_town.c's sHomes) a town's house-th
// ordinary home is drawn with, and whether that room has a painting on
// its wall (the Building tileset's 0x207 over 0x20F).
u8 RogueGen_HomeRoom(const struct RogueRegion *r, u8 place, u8 house);
bool8 RogueGen_HomeRoomHasPainting(u8 room);
// The first ordinary home of a town whose room has a painting, or -1:
// where LORE_WAY_PAINTING hangs (else on the POKéMON CENTER's wall).
s32 RogueGen_PaintingHouse(const struct RogueRegion *r, u8 place, const u8 *interiors, u8 nHouses);
// A piece of a legend's lore (LORE_STORY...), with the way's frame
// (LORE_WAYS for none), broken into boxes. Returns its length.
u32 RogueGen_LoreText(const struct RogueRegion *r, u8 dungeon, u8 piece, u8 way, char *out, u32 size);
// The town a place is named after, the nearest by the roads.
u8 RogueGen_TownNear(const struct RogueRegion *r, u8 place);
// The region's volcano's name and an ice cave's, in ASCII.
char *RogueGen_VolcanoName(const struct RogueRegion *r, char *dest);
char *RogueGen_IceCaveName(const struct RogueRegion *r, u8 dungeon, char *dest);

// Names (src/rogue_gen_names.h), in ASCII as the game shows them: a place's
// (a town, ROUTE 12, VICTORY ROAD, a cave's or its dungeon's), a dungeon's,
// a legend's chamber's, and a place's name without its last word (PINEWICK).
// Each returns the end of what it wrote.
char *RogueGen_PlaceName(const struct RogueRegion *r, u8 place, char *dest);
char *RogueGen_DungeonName(const struct RogueRegion *r, u8 dungeon, char *dest);
char *RogueGen_ChamberName(const struct RogueRegion *r, u8 dungeon, char *dest);
s8 RogueGen_FloorPopup(const struct RogueRegion *r, u8 dungeon, u8 floor, char *dest);
char *RogueGen_TownStem(const struct RogueRegion *r, u8 place, char *dest);
// The dungeon a place hosts, or -1; whether a cave goes by its dungeon's name.
s32 RogueGen_HostedDungeon(const struct RogueRegion *r, u8 place);
bool8 RogueGen_CaveHasDungeonName(const struct RogueRegion *r, u8 place);
// The next town along the road from a place (itself at the end), and the
// route out of a town (the main road's way on first).
u8 RogueGen_NextTownFrom(const struct RogueRegion *r, u8 place);
u8 RogueGen_RouteNear(const struct RogueRegion *r, u8 place);

// People (src/rogue_gen_people.h): the leaders a region draws and every
// trainer slot's class, so the host tools know who says what. The game keeps
// sprites, teams and music in tables of the same order (src/rogue.c).
// Types as the game's enum Type numbers them.
enum
{
    RTYPE_NONE, RTYPE_NORMAL, RTYPE_FIGHTING, RTYPE_FLYING, RTYPE_POISON, RTYPE_GROUND, RTYPE_ROCK, RTYPE_BUG,
    RTYPE_GHOST, RTYPE_STEEL, RTYPE_MYSTERY, RTYPE_FIRE, RTYPE_WATER, RTYPE_GRASS, RTYPE_ELECTRIC, RTYPE_PSYCHIC,
    RTYPE_ICE, RTYPE_DRAGON, RTYPE_DARK, RTYPE_FAIRY, RTYPE_COUNT,
};
extern const char *const gRogueTypeNames[RTYPE_COUNT]; // ROCK, as Gen 3 text prints them
#define ROGUE_LEADER_COUNT 32
struct RogueGenLeader
{
    const char *name; // ASCII
    u8 type;          // RTYPE_*
    u8 female;
    u8 doubles;       // battles two at a time (Tate and Liza)
    const char *badge;
};
extern const struct RogueGenLeader gRogueGenLeaders[ROGUE_LEADER_COUNT];
// Draws eight gym leaders and four Elite Four (indexes of gRogueGenLeaders)
// from rng; a region's draw uses RogueRng_Hash(seed, 13013).
void RogueGen_PickLeaders(u32 *rng, u8 *gyms, u8 *e4);
void RogueGen_RegionLeaders(const struct RogueRegion *r, u8 *gyms, u8 *e4);
// A gym's leader (0-7), or an Elite Four member (8-11).
const struct RogueGenLeader *RogueGen_GymLeader(const struct RogueRegion *r, u8 gym);
// What kind of thing a trainer class talks about.
enum RogueTalk
{
    RTALK_KID, RTALK_BUGS, RTALK_HIKER, RTALK_OUTDOORS, RTALK_FIGHTER, RTALK_WATER, RTALK_FANCY,
    RTALK_MYSTIC, RTALK_BIRDS, RTALK_FIRE, RTALK_MUSIC, RTALK_ACE, RTALK_SAILOR, RTALK_COUNT,
};
enum RogueClass
{
    RCLASS_YOUNGSTER, RCLASS_LASS, RCLASS_BUG_CATCHER, RCLASS_CAMPER, RCLASS_PICNICKER, RCLASS_FISHERMAN,
    RCLASS_BIRD_KEEPER, RCLASS_TUBER, RCLASS_SCHOOL_KID, RCLASS_HIKER, RCLASS_RUIN_MANIAC, RCLASS_NINJA_BOY,
    RCLASS_BLACK_BELT, RCLASS_BATTLE_GIRL, RCLASS_KINDLER, RCLASS_AROMA_LADY, RCLASS_BEAUTY, RCLASS_GUITARIST,
    RCLASS_SAILOR, RCLASS_POKEFAN, RCLASS_PSYCHIC, RCLASS_HEX_MANIAC, RCLASS_GENTLEMAN, RCLASS_COOLTRAINER_M,
    RCLASS_COOLTRAINER_F, RCLASS_EXPERT,
    // v1.1 sea: Swimmers, out on the water of a maze or the shallows only
    // (RogueGen_TrainerSwims), never drawn at random: classes drawn at
    // random come from the ones before them (ROGUE_CLASS_PICK), so no other
    // trainer changed.
    RCLASS_SWIMMER_M, RCLASS_SWIMMER_F,
    ROGUE_CLASS_COUNT,
};
#define ROGUE_CLASS_PICK RCLASS_SWIMMER_M
#define ROGUE_CLASS_VILLAIN 0xFF
struct RogueGenClass
{
    const char *name; // as text names it: BUG CATCHER
    u8 female;
    u8 type;          // the team's type on routes, RTYPE_NONE for any
    u8 firstRoute;    // earliest route tier it appears on
    u8 talk;          // enum RogueTalk
};
extern const struct RogueGenClass gRogueGenClasses[ROGUE_CLASS_COUNT];
enum { RVROLE_NONE, RVROLE_GRUNT, RVROLE_ADMIN, RVROLE_LEADER };
// A trainer slot's class (RCLASS_*), or ROGUE_CLASS_VILLAIN.
u8 RogueGen_TrainerClass(const struct RogueRegion *r, u32 slot);
// The same for a slot that is no villain, whose place (0xFF) and dungeon
// (-1) are known; and the villain role and dungeon kind likewise, for passes
// over many slots (the trainers' lines).
u8 RogueGen_TrainerClassAt(const struct RogueRegion *r, u32 slot, u8 place, s32 d);
struct RogueTeams;
u8 RogueGen_VillainRoleAt(const struct RogueRegion *r, u32 slot, const struct RogueTeams *tm);
u8 RogueGen_DungeonKindAt(const struct RogueRegion *r, u8 place, s32 d);
u8 RogueGen_GymTrainerClass(const struct RogueRegion *r, u32 slot, u8 leaderType);
u8 RogueGen_VillainRole(const struct RogueRegion *r, u32 slot);
// Where a trainer slot stands: its place (0xFF for none: a dungeon floor, a
// gym), its dungeon (-1), the dungeon kind it stands in (RDUN_COUNT for
// none), and the dungeon it is the master of (-1).
u8 RogueGen_SlotPlace(const struct RogueRegion *r, u32 slot);
s32 RogueGen_SlotDungeon(const struct RogueRegion *r, u32 slot);
u8 RogueGen_SlotDungeonKind(const struct RogueRegion *r, u32 slot);
s32 RogueGen_MasterDungeon(const struct RogueRegion *r, u32 slot);

// Facts (src/rogue_gen_facts.h): what a region and its towns are like, and
// the recurring cast, from the seed with their own random streams (nothing
// saved, no map changed). Words are ASCII ('@' the e of POK@MON).
// What lies round a town (RogueGen_TownSurroundings).
#define RFACT_NEAR_SEA     (1 << 0)
#define RFACT_NEAR_LAKE    (1 << 1) // a lake or river route next door
#define RFACT_NEAR_WOODS   (1 << 2)
#define RFACT_NEAR_HILLS   (1 << 3) // hills, desert, mountain, or a rocky theme
#define RFACT_NEAR_FIELDS  (1 << 4)
#define RFACT_NEAR_CAVE    (1 << 5) // a cave mouth here or next door
#define RFACT_NEAR_DUNGEON (1 << 6) // a dungeon named after the town (not the hideout)
#define RFACT_NEAR_VOLCANO (1 << 7) // the volcano the region map draws
u8 RogueGen_TownSurroundings(const struct RogueRegion *r, u8 place);
bool8 RogueGen_TownSeaOnMap(const struct RogueRegion *r, u8 place);
// What a town lives by (gRogueTrades), and what it is known by
// (gRogueLandmarks). Add a row at the end of each, before the COUNT.
enum RogueTradeId
{
    RTRADE_FISHING, RTRADE_BOATS, RTRADE_PEARLS, RTRADE_SALT, RTRADE_MARKET, RTRADE_BERRIES, RTRADE_FLOWERS,
    RTRADE_HERBS, RTRADE_MILK, RTRADE_HONEY, RTRADE_MINING, RTRADE_STONE, RTRADE_GLASS, RTRADE_INNS, RTRADE_POTTERY,
    RTRADE_WEAVING, RTRADE_WOOD, RTRADE_BALLS, RTRADE_STARS, RTRADE_PAINTING, RTRADE_SHELLS, RTRADE_ROPES,
    RTRADE_FOSSILS, RTRADE_TEA, RTRADE_BAKING, RTRADE_LUMBER,
    // Only a town signature's (src/rogue_gen_signatures.h): never chosen by theme.
    RTRADE_SPACE, RTRADE_WEATHER, RTRADE_FIGHTING,
    RTRADE_COUNT,
};
struct RogueTrade
{
    const char *name;  // "fishing": what the town does
    const char *folk;  // "fishers": who does it
    const char *goods; // "fish": what it makes
    u16 themes;        // a bit per RTHEME_* it suits
    u8 needs;          // RFACT_NEAR_* bits, any one of which it needs (0: none)
    const char *prides[2]; // noun phrases: "the freshest fish anywhere"
    u8 shop;           // the RSHOP_* specialty that matches it (for town signatures)
    const char *motto; // for the town sign: "Where the nets come home full."
};
extern const struct RogueTrade gRogueTrades[RTRADE_COUNT];
enum RogueLandmarkId
{
    RLANDMARK_LIGHTHOUSE, RLANDMARK_PIER, RLANDMARK_MARKET, RLANDMARK_WINDMILL, RLANDMARK_GREAT_TREE,
    RLANDMARK_CLOCK, RLANDMARK_BRIDGE, RLANDMARK_STATUE, RLANDMARK_MUSEUM, RLANDMARK_TELESCOPE,
    RLANDMARK_BATHHOUSE, RLANDMARK_KILN, RLANDMARK_GALLERY, RLANDMARK_FIELDS, RLANDMARK_ORCHARD, RLANDMARK_WELL,
    RLANDMARK_MINE, RLANDMARK_WATERWHEEL, RLANDMARK_SHELL_HALL, RLANDMARK_ROPEWALK, RLANDMARK_TEA_HOUSE,
    RLANDMARK_BELL, RLANDMARK_DOCKS, RLANDMARK_BAKERY,
    // Only a town signature's, built on the map (src/rogue_gen_signatures.h).
    RLANDMARK_HOT_SPRING, RLANDMARK_SPACE, RLANDMARK_WEATHER, RLANDMARK_DOJO, RLANDMARK_RANCH, RLANDMARK_DIG,
    RLANDMARK_POND, RLANDMARK_MINES,
    RLANDMARK_COUNT,
};
struct RogueLandmark
{
    const char *name; // ' ' first: after the town's name (PINEWICK LIGHTHOUSE); '*': the famous figure's name
    u16 themes;
    u8 needs;         // RFACT_NEAR_* bits, any one of which it needs (0: none)
    u8 trade;         // the trade it suits best, RTRADE_COUNT for any
    const char *motto; // for the town sign: "Its light guides every boat home."
};
extern const struct RogueLandmark gRogueLandmarks[RLANDMARK_COUNT];
// What a town argues over with its neighbour.
enum { RFEUD_RACE, RFEUD_SAME_TRADE, RFEUD_TRADES, RFEUD_GYMS, RFEUD_FESTIVAL, RFEUD_COUNT };
// What troubles a town now (RogueTownFacts.problem; problemAt says where).
enum RogueProblem
{
    RPROB_NONE,
    RPROB_VILLAINS,         // the team holds the town: at = the hideout dungeon
    RPROB_VILLAIN_SIGHTED,  // grunts seen on the route next door: at = that route
    RPROB_LOST,             // someone lost in a dungeon named after the town: at = the dungeon
    RPROB_LEGEND,           // a legend stirs in the town's lair: at = the dungeon
    RPROB_BLOCKED,          // the road out is blocked (tree, rock, boulder, water): at = the route
    RPROB_POWER,            // the lights flicker since the power plant shut: at = the dungeon
    RPROB_WRECK,            // a ship ran aground off the coast: at = the dungeon
    RPROB_VOLCANO,          // the volcano has been rumbling
    RPROB_ROUGH_SEA,        // the sea's too rough for the boats
    RPROB_PESTS,            // the local POK@MON keep raiding the gardens
    RPROB_PET_LOST,         // someone's pet POK@MON wandered off
    RPROB_FESTIVAL,         // the festival is coming and nothing is ready (its town)
    RPROB_COUNT,
};
struct RogueRegionFacts
{
    u8 legend;       // the dungeon whose legend the founding tale is about, 0xFF for none
    u8 founding;     // the tale (the legend's LELEM_* * 2 + which)
    u8 oldTown;      // place: the oldest town, where the tale is told
    u8 festival;     // the festival (LELEM_* * 3 + which)
    u8 festTown;     // place where it is held
    u8 figure;       // the famous figure's deed
    u8 figureFemale;
    u8 figureTown;   // place: their home town (with their statue, if it has one)
};
struct RogueTownFacts
{
    u8 place;
    u8 near;         // RFACT_NEAR_* bits
    u8 trade;        // RTRADE_*, RTRADE_COUNT for the summit
    u8 landmark;     // RLANDMARK_*, RLANDMARK_COUNT for the summit
    u8 pride;        // which of the trade's prides
    u8 neighbor;     // place of the rival town, or 0xFF
    u8 feud;         // RFEUD_*
    u8 localRoute;   // place of the route the local POKeMON comes from
    u8 localArea;    // its wild area (0xFF for none) and slot: the species is the game's
    u8 localSlot;
    u8 problem;      // RPROB_*
    u8 problemAt;    // the dungeon or place it is about, or 0xFF
};
void RogueGen_RegionFacts(const struct RogueRegion *r, struct RogueRegionFacts *f);
// The volcano's cell for the text (RogueGen_VolcanoCell, kept once worked out).
bool8 RogueGen_FactsVolcano(const struct RogueRegion *r, s8 *x, s8 *y);
void RogueGen_TownFacts(const struct RogueRegion *r, u8 place, struct RogueTownFacts *f);
// All towns' facts at once, by town number (ROGUE_MAX_TOWNS of them): cheaper than one by one.
void RogueGen_AllTownFacts(const struct RogueRegion *r, struct RogueTownFacts *f);
const char *RogueGen_FestivalName(const struct RogueRegionFacts *f);
const char *RogueGen_FestivalAct(const struct RogueRegionFacts *f);
const char *RogueGen_FoundingTale(const struct RogueRegionFacts *f);
const char *RogueGen_FigureName(const struct RogueRegion *r, const struct RogueRegionFacts *f);
const char *RogueGen_FigureTitle(const struct RogueRegionFacts *f);
const char *RogueGen_FigureDeed(const struct RogueRegionFacts *f);
const char *RogueGen_TownPride(const struct RogueTownFacts *f);
char *RogueGen_LandmarkName(const struct RogueRegion *r, const struct RogueTownFacts *f, char *dest);
// TRUE if it stands on the town's map (a signature's); people name only those.
bool8 RogueGen_LandmarkOnMap(const struct RogueRegion *r, const struct RogueTownFacts *f);
// The recurring cast. The wanderer's look is a vanilla trainer sprite the
// game maps (src/rogue.c): STEVEN's, Champion WALLACE's, a COOLTRAINER's, an
// EXPERT's.
enum { RLOOK_STEVEN, RLOOK_WALLACE, RLOOK_ACE, RLOOK_VETERAN, RLOOK_COUNT };
struct RogueCast
{
    u8 wandererLook, wandererFemale, wandererName;
    u8 wandererTowns[4];  // places of the meetings on the road, in order (0xFF unused)
    u8 wandererBadges[4]; // badges passed on the road to each (the meeting waits for them)
    u8 reporters;         // the reporter pair
    u8 reporterTowns[3];  // places they turn up, early, middle, late (0xFF unused)
    u8 elder, elderTown;  // the lore elder and the oldest town they live in
    u8 explorer, explorerDungeon, explorerTown; // lost in a dungeon (0xFF for none), thanks you in its town
};
void RogueGen_Cast(const struct RogueRegion *r, struct RogueCast *c);
const char *RogueGen_WandererName(const struct RogueCast *c);
s32 RogueGen_WandererAt(const struct RogueCast *c, u8 place);
const char *RogueGen_ReporterName(const struct RogueCast *c, u8 camera);
bool8 RogueGen_ReporterFemale(const struct RogueCast *c, u8 camera);
char *RogueGen_ElderName(const struct RogueCast *c, char *dest);
bool8 RogueGen_ElderFemale(const struct RogueCast *c);
const char *RogueGen_ExplorerName(const struct RogueCast *c);
bool8 RogueGen_ExplorerFemale(const struct RogueCast *c);
const char *RogueGen_FamilyName(const struct RogueRegion *r, u8 place, u8 house);
// The k-th of a run of picks from n under a key (RogueRng_Hash(seed, key)):
// consecutive k give different entries until all n are used.
u32 RogueText_Deal(u32 seed, u32 key, u32 k, u32 n);

// The text engine (src/rogue_gen_text.h): templates with named slots
// ({TOWN}, {LEADER}, {TRADE}...) filled from the region's facts, wrapped into
// Gen 3 boxes, and picked so a run spreads its lines. Domains keep their
// banks in src/rogue_text_<domain>.h.
// The slots a template may use ({TOWN}...), in the order of sTextSlots in
// src/rogue_gen_text.h, which says what each is.
enum RogueTextSlot
{
    TS_REGION, TS_TOWN, TS_STEM, TS_OTHER, TS_OTHER_STEM, TS_NEXT, TS_ROUTE, TS_PLACE, TS_DUNGEON, TS_LEGEND,
    TS_LEADER, TS_GYM_TOWN, TS_TYPE, TS_BADGE, TS_CLASS, TS_TEAM, TS_BOSS, TS_ADMIN, TS_VOLCANO,
    TS_TRADE, TS_TRADE_FOLK, TS_GOODS, TS_LANDMARK, TS_PRIDE, TS_NEIGHBOR, TS_FEUD, TS_LOCALMON, TS_PROBLEM_AT,
    TS_FESTIVAL, TS_FEST_ACT, TS_FEST_TOWN, TS_FOUNDING, TS_OLDTOWN, TS_FIGURE, TS_FIGURE_TITLE, TS_FIGURE_DEED,
    TS_FIGURE_TOWN, TS_WANDERER, TS_REPORTER, TS_CAMERA, TS_ELDER, TS_EXPLORER, TS_FAMILY,
    TS_PLAYER, TS_RIVAL, TS_LEAD, TS_MON, TS_RIVAL_MON, TS_BADGES, TS_N, TS_NAME,
    TS_NICK, TS_FEATURE, TS_RESIDENT, TS_TEAM2, TS_BOSS2, TS_OWN,
    TS_COUNT,
};
// Who says a line (for picks, frames and the dump).
enum RogueSpeaker
{
    RSPK_NONE, RSPK_SIGN, RSPK_FOLK, RSPK_HOME, RSPK_TRAINER, RSPK_GYM, RSPK_LEADER, RSPK_E4, RSPK_CHAMPION,
    RSPK_RIVAL, RSPK_VILLAIN, RSPK_WANDERER, RSPK_REPORTER, RSPK_ELDER, RSPK_EXPLORER, RSPK_BIRCH, RSPK_AIDE,
    RSPK_TV, RSPK_BOOK, RSPK_CLERK, RSPK_NURSE, RSPK_PET,
    RSPK_COUNT,
};
// What the run has done so far, for lines that react to it: filled by the
// game (Rogue_TextState in src/rogue_text.c), set by the host dump for each
// state it prints. Add fields at the end as a domain needs them.
#define RTSTATE_STARTER  (1 << 0) // the player has a POK@MON
#define RTSTATE_RIVAL1   (1 << 1) // the first rival battle is done
#define RTSTATE_HIDEOUT  (1 << 2) // the main villain team's hideout is cleared
#define RTSTATE_VLEADER  (1 << 3) // the main villain team's leader is beaten
#define RTSTATE_HARDCORE (1 << 4) // the Hardcore rules
struct RogueTextState
{
    u8 badges;    // badges won
    u8 wonGyms;   // a bit per gym won
    u8 fallen;    // POK@MON lost this run
    u8 flags;     // RTSTATE_*
    u16 bosses;   // a bit per dungeon whose boss is beaten
    u16 legends;  // a bit per dungeon whose legend is caught or met
    u8 leadType;  // the lead POK@MON's first type (RTYPE_*)
    u8 runs;      // runs before this one
    u8 wins;      // runs won before this one
    u16 rescued;  // a bit per dungeon whose lost explorer was led out
    u8 teamsCleared; // a bit per villain team whose hideout is cleared (team 0 is RTSTATE_HIDEOUT too)
    u8 teamsBeaten;  // a bit per villain team whose leader is beaten
};
struct RogueTextCtx
{
    const struct RogueRegion *r;
    const struct RogueTextState *state; // NULL: the start of a run
    u8 place;    // where it is said ({PLACE}, {NEXT}), 0xFF for nowhere
    u8 speaker;  // RSPK_*
    u8 town;     // {TOWN} and the town's facts; RogueText_Init sets the town nearest place
    u8 other;    // {OTHER}: a second place
    u8 route;    // {ROUTE}
    u8 dungeon;  // {DUNGEON}, {LEGEND}
    u8 gym;      // {LEADER}, {GYM_TOWN}, {TYPE}, {BADGE}: 0-7 a gym, 8-11 the Elite Four
    u8 house;    // {FAMILY}: a house of the town
    u8 area, mon; // {MON}: a wild area and its slot
    u8 slot;     // {CLASS}: a trainer slot
    u8 team;     // {TEAM}, {BOSS}, {ADMIN}: a villain team (0xFF: the place's hideout's, else the main one)
    u8 team2;    // {TEAM2}, {BOSS2}: the team they clash with (0xFF: none)
    u16 number;  // {N}
    const char *name; // {NAME}
    const struct RogueTownFacts *facts; // the town's facts, if the caller has them (saves working them out per slot)
    u8 missing;  // slots the last fills could not fill
};
// A bank of templates. TEXT_BANK(sId, "line", "line"...) defines one.
struct RogueTextBank
{
    const char *name;
    const char *const *lines;
    u16 count;
};
#define TEXT_BANK(id, ...) \
    static const char *const id##_Lines[] = { __VA_ARGS__ }; \
    static const struct RogueTextBank id = { #id, id##_Lines, sizeof(id##_Lines) / sizeof(id##_Lines[0]) }
void RogueText_Init(struct RogueTextCtx *c, const struct RogueRegion *r, u8 place, u8 speaker);
bool8 RogueText_CanFill(const char *t, struct RogueTextCtx *c);
u32 RogueText_Fill(char *out, u32 size, const char *t, struct RogueTextCtx *c);
bool8 RogueText_Wrap(char *s, u32 width);
// Fill and wrap: returns the length, 0 if a slot could not be filled.
u32 RogueText_Say(char *out, u32 size, const char *t, struct RogueTextCtx *c);
const char *RogueText_Pick(const struct RogueTextBank *b, struct RogueTextCtx *c, u32 k);
const char *RogueText_PickRegion(const struct RogueTextBank *b, struct RogueTextCtx *c, u32 k);
const char *RogueText_Frame(u8 speaker);
const char *RogueGen_VillainAdminName(const struct RogueRegion *r);
// Values only the game knows, in ASCII, for the slots PLAYER, RIVAL, LEAD,
// RIVAL_MON and MON (a = the wild area, b = its slot): the game defines it
// (src/rogue_text.c), and so do the host tools. Returns the end of what it
// wrote, or NULL if it has no value.
char *RogueText_Runtime(u8 slot, u8 a, u8 b, char *dest);
// The domains' entry points (src/rogue_text_*.h).
// A topic only the game can tell (a dungeon rumor): the game's function
// writes the line itself and returns TRUE.
typedef bool8 (*RogueGameTopic)(u8 topic, u8 place, u32 rng);
enum { RTALK_TEXT, RTALK_GAME };
// Trainers (rogue_text_trainers.h).
enum { RTRAINER_INTRO, RTRAINER_DEFEAT, RTRAINER_AFTER };
extern const u8 gRogueGymTrainers[ROGUE_NUM_GYMS]; // trainers in each gym
const char *RogueText_TrainerLine(const struct RogueRegion *r, u32 slot, u8 field);
// Where a trainer's lines are said, the context's slots for them, and the
// template checked against that context (the game's ClassLine, the dump).
u8 RogueText_TrainerPlace(const struct RogueRegion *r, u32 slot);
void RogueText_TrainerCtx(struct RogueTextCtx *c, u32 slot);
const char *RogueText_TrainerTemplate(struct RogueTextCtx *c, u32 slot, s32 index, u8 field);
// The set a trainer says (a pass over the region's trainers: the game keeps
// it on the trainer's object), and a line of it.
s32 RogueText_TrainerSetIndex(const struct RogueRegion *r, u32 slot);
const char *RogueText_TrainerLineOfSet(const struct RogueRegion *r, u32 slot, s32 index, u8 field);
// A route's weather, as the game's map headers draw it.
enum { RWEATHER_NONE, RWEATHER_SUN, RWEATHER_SHADE, RWEATHER_RAIN, RWEATHER_SAND, RWEATHER_ASH, RWEATHER_STORM, RWEATHER_FOG };
u8 RogueGen_RouteWeather(const struct RogueRegion *r, u8 place);
// The weather a route is known for (v1.1, src/rogue_gen_zones.h): its rain,
// storms or mist, or RWEATHER_NONE.
u8 RogueGen_KnownWeather(const struct RogueRegion *r, u8 place);
// Leaders (rogue_text_leaders.h): a leader's (an index of gRogueGenLeaders)
// lines, and the gossip about a gym's leader.
enum { RLEADER_INTRO, RLEADER_DEFEAT, RLEADER_AFTER, RLEADER_GOSSIP, RLEADER_E4_INTRO, RLEADER_E4_AFTER, RLEADER_LINES };
const char *RogueText_LeaderLine(u8 leader, u8 field);
const char *RogueText_LeaderGossip(const struct RogueRegion *r, u8 gym);
// Town signatures' people (rogue_text_signatures.h): a line of what into
// out (ASCII, wrapped), k picking among a bank (a person's order, an exhibit).
enum { RSIGTEXT_TALK, RSIGTEXT_GIFT, RSIGTEXT_GIFT_AFTER, RSIGTEXT_SOAK, RSIGTEXT_COOKIE, RSIGTEXT_MILK, RSIGTEXT_STALL,
       RSIGTEXT_SIGN, RSIGTEXT_EXHIBIT, RSIGTEXT_DOJO, RSIGTEXT_DOJO_REWARD, RSIGTEXT_COUNT };
u32 RogueText_SigLine(const struct RogueRegion *r, u8 place, u8 what, u32 k, char *out, u32 size);
u32 RogueText_SigTurn(const struct RogueMap *m, s32 obj, u32 inside);
#define RSIG_INSIDE 0x100 // RogueText_SigTurn: one of those inside the landmark
// A leader's line as this region hears it (c->gym: 0-7 a gym, 8-11 an
// Elite Four seat), the gym guide's, the TM handover's and the last Elite
// Four member's hint about the CHAMPION.
const char *RogueText_LeaderLineIn(struct RogueTextCtx *c, u8 leader, u8 field);
// A gym leader's after line as the badge is won (again: talked to after).
const char *RogueText_LeaderAfter(struct RogueTextCtx *c, u8 leader, bool8 again);
const char *RogueText_GymGuide(struct RogueTextCtx *c);
const char *RogueText_LeaderTM(struct RogueTextCtx *c, bool8 after);
const char *RogueText_E4LastHint(struct RogueTextCtx *c, u8 met);
// The rival's line for a battle (0-3).
enum { RRIVAL_INTRO, RRIVAL_DEFEAT, RRIVAL_AFTER };
const char *RogueText_RivalLine(struct RogueTextCtx *c, u8 battle, u8 field);
// The wanderer at a meeting on the road (0-3), into out: the talk (met:
// a bit per meeting had before; gift: a gift follows), the farewell after a gift, or what
// they say when talked to again.
enum { RWANDER_TALK, RWANDER_BYE, RWANDER_REPEAT };
u32 RogueText_Wanderer(struct RogueTextCtx *c, u8 meeting, u8 mode, u8 met, bool8 gift, char *out, u32 size);
// The townsperson of a town's map the wanderer stands in for (-1: none).
struct RogueMap;
s32 RogueText_WandererObj(const struct RogueRegion *r, const struct RogueMap *m);
// Where the wanderer waits in a meeting's town (by the POKeMON CENTER's
// door), self the townsperson they stand in for; FALSE for none.
bool8 RogueText_WandererSpot(const struct RogueMap *m, s32 self, u8 *x, u8 *y);
// The turn of the townsperson who points the wanderer out (-1: none), and
// what they say (metBefore: the player has met the wanderer on the road).
s32 RogueText_WandererPointerTurn(const struct RogueRegion *r, const struct RogueMap *m);
const char *RogueText_WandererPointer(struct RogueTextCtx *c, u8 meeting, bool8 met);
// The CHAMPION (the wanderer) in their room, and the last Elite Four
// member's hint; met: a bit per meeting had on the road.
enum { RCHAMP_INTRO, RCHAMP_DEFEAT, RCHAMP_AFTER, RCHAMP_HOF, RCHAMP_LINES };
const char *RogueText_ChampionLine(struct RogueTextCtx *c, u8 field, u8 met);
// The villain team's lines (kind 0 intro, 1 defeat, 2 after), and the
// dungeon of the legend they are after (-1 for none).
// (RVILLAIN_TOWN kind 3: the feud with ctx.team2, where two or three teams are.)
// (RVILLAIN_ADMIN kind 3: where the leader went, as the team leaves.
// RVILLAIN_HIDEOUT_LAST: a grunt on the hideout's bottom floor.)
enum { RVILLAIN_GRUNT, RVILLAIN_HIDEOUT, RVILLAIN_ADMIN, RVILLAIN_LEADER, RVILLAIN_BLOCK, RVILLAIN_LOOKOUT, RVILLAIN_TOWN,
       RVILLAIN_HIDEOUT_LAST };
// What the field says once a team has cleared out of its hideout.
const char *RogueText_HideoutLeft(struct RogueTextCtx *c);
const char *RogueText_VillainLine(struct RogueTextCtx *c, u8 role, u8 kind, u32 k);
s32 RogueText_VillainGoal(const struct RogueRegion *r);
s32 RogueText_TeamGoal(const struct RogueRegion *r, u8 team);
// Towns (rogue_text_towns.h).
u32 RogueText_TownMotto(struct RogueTextCtx *c, char *out, u32 size);
u8 RogueText_NextGym(const struct RogueRegion *r, u8 place);
// Who is talking, as the game dresses them: a kid, a grown-up, an old one.
enum { RVOICE_ANY, RVOICE_KID, RVOICE_ADULT, RVOICE_OLD, RVOICE_NONE = 0xFF };
// The water in and around a town (the game dresses its people by it).
enum { RWATER_NONE, RWATER_FRESH, RWATER_SEA };
u8 RogueText_TownWater(const struct RogueRegion *r, u8 place, const struct RogueMap *m);
// The voice of a townsperson the game dresses from slot (Rogue_PlaceTownsfolkGfx).
u8 RogueText_FolkVoice(const struct RogueRegion *r, u8 place, const struct RogueMap *m, u32 slot);
u8 RogueText_RouteFolkVoice(u32 slot);
u8 RogueText_TownTalk(struct RogueTextCtx *c, u32 order, u32 rng, bool8 onRoute, u8 voice, RogueGameTopic gameTopic,
                      char *out, u32 size);
u32 RogueText_TownTalkOrder(const struct RogueRegion *r, u8 place, u32 order);
u32 RogueText_TownTalkRng(const struct RogueRegion *r, u8 place, u32 localId);
// The reporter pair in their towns: which the turn-th of nFolk townsfolk is.
enum { RTEXT_REPORTER = 1, RTEXT_CAMERA };
u8 RogueText_ReporterRole(const struct RogueRegion *r, u8 place, u32 nFolk, u32 turn);
void RogueText_Reporter(struct RogueTextCtx *c, u8 role, u8 met, char *out, u32 size);
s32 RogueText_ReporterMeeting(const struct RogueRegion *r, u8 place);
u32 RogueText_HouseRng(const struct RogueRegion *r, u8 place, u8 house, u32 salt);
// A home's people (m the town's map): their voices (v2 RVOICE_NONE when
// one lives there), what they say, the cast's elder's home in the oldest town.
void RogueText_HomeVoices(const struct RogueRegion *r, u8 place, const struct RogueMap *m, u8 house, u8 *v1, u8 *v2);
u8 RogueText_HomeTalk(struct RogueTextCtx *c, const struct RogueMap *m, u8 house, u32 localId, RogueGameTopic gameTopic,
                      char *out, u32 size);
s32 RogueText_CastElderHouse(const struct RogueRegion *r, u8 place, const struct RogueMap *m);
// The plate beside a home (its family), and the house a sign at x, y names.
void RogueText_HouseSign(struct RogueTextCtx *c, u8 house, bool8 gift, char *out, u32 size);
void RogueText_Cabinet(struct RogueTextCtx *c, u8 house, char *out, u32 size);
enum { RREST_SIGN, RREST_OFFER_HURT, RREST_OFFER_FINE, RREST_DONE, RREST_DECLINED, RREST_TRAVELER, RREST_COUNT };
void RogueText_RestHouse(struct RogueTextCtx *c, u8 what, char *out, u32 size);
void RogueText_CenterFolk(struct RogueTextCtx *c, const struct RogueMap *m, u32 k, char *out, u32 size);
u8 RogueText_CenterFolkSlot(const struct RogueRegion *r, u8 place, const struct RogueMap *m, u32 k);
u8 RogueText_CenterFolkVoice(const struct RogueRegion *r, u8 place, const struct RogueMap *m, u32 k);
s32 RogueText_SignHouse(const struct RogueMap *m, u8 x, u8 y);
// The roles of src/rogue_town.c's house people that have their own lines
// (the same numbers as its ROLE_*).
enum { RTEXT_ROLE_LAB = RHOME_NAME_RATER + 1, RTEXT_ROLE_TEACHER, RTEXT_ROLE_STUDENT, RTEXT_ROLE_FAN };
void RogueText_HouseRole(struct RogueTextCtx *c, u8 role, u8 house, u32 localId, char *out, u32 size);
void RogueText_GiftOffer(struct RogueTextCtx *c, const struct RogueMap *m, u8 house, u16 item, char *out, u32 size);
void RogueText_GiftAfter(struct RogueTextCtx *c, const struct RogueMap *m, u8 house, u16 item, char *out, u32 size);
void RogueText_FolkGift(struct RogueTextCtx *c, u8 voice, bool8 after, u16 item, char *out, u32 size);
void RogueText_ShopGreeting(struct RogueTextCtx *c, u8 shop, char *out, u32 size);
void RogueText_MonLine(struct RogueTextCtx *c, bool8 home, u32 localId, char *out, u32 size);
// Dungeons (rogue_text_dungeons.h): c->dungeon is the dungeon spoken of.
// Each returns the length written (0 for nothing to say).
u32 RogueText_DungeonSign(struct RogueTextCtx *c, char *out, u32 size);          // c->number 1: woods the road goes through
u32 RogueText_GuardianAppears(struct RogueTextCtx *c, char *out, u32 size);      // c->name: its species
u32 RogueText_BossGone(struct RogueTextCtx *c, bool8 legend, char *out, u32 size);
enum { RMASTER_INTRO, RMASTER_DEFEAT, RMASTER_AFTER };
u32 RogueText_MasterLine(struct RogueTextCtx *c, u8 field, char *out, u32 size);
u32 RogueText_GuideLine(struct RogueTextCtx *c, bool8 taken, char *out, u32 size);
u32 RogueText_ChoiceGone(struct RogueTextCtx *c, bool8 several, char *out, u32 size);
enum { RCAMP_OFFER, RCAMP_DONE, RCAMP_DECLINED, RCAMP_USED };
u32 RogueText_CampLine(struct RogueTextCtx *c, u8 line, char *out, u32 size);   // c->dungeon 0xFF: the camp by the road at c->place
enum { REXPLORER_ASK, REXPLORER_THANKS, REXPLORER_ROPE, REXPLORER_WAIT, REXPLORER_FULL, REXPLORER_TOWN };
u32 RogueText_ExplorerLine(struct RogueTextCtx *c, u8 line, char *out, u32 size);
u32 RogueText_PrizeLine(struct RogueTextCtx *c, char *out, u32 size);
// Things to read on a floor: a tower's graves, a shipwreck's bins (its
// log), a power plant's terminals and generator.
enum { RTHING_NONE, RTHING_GRAVE, RTHING_BIN, RTHING_TERMINAL, RTHING_GENERATOR, RTHING_PLAIN_GRAVE };
u8 RogueGen_FloorThingAt(const struct RogueRegion *r, u8 dungeon, u8 floor, u32 x, u32 y, u8 *k, bool8 *lore);
u32 RogueText_FloorThing(struct RogueTextCtx *c, u8 floor, u8 thing, u8 k, bool8 lore, char *out, u32 size);
s32 RogueText_RumorDungeon(const struct RogueRegion *r, u8 place, u32 rng, u16 bosses);
u32 RogueText_DungeonRumor(struct RogueTextCtx *c, u32 rng, char *out, u32 size);
u32 RogueText_GateRumor(struct RogueTextCtx *c, u32 rng, char *out, u32 size);
// Paintings (house 0xFF: the POKéMON CENTER's picture), the child's rhyme
// in the POKéMON CENTER, and a museum's k-th exhibit (for the town
// signatures' museum; 0 past the last).
u32 RogueText_Painting(struct RogueTextCtx *c, u8 house, s32 paintingHouse, char *out, u32 size);
u32 RogueText_Rhyme(struct RogueTextCtx *c, char *out, u32 size);
u32 RogueText_MuseumExhibit(struct RogueTextCtx *c, u32 k, char *out, u32 size);
s32 RogueGen_MuseumCase(bool8 lilycove, u32 x, u32 y);
#ifdef ROGUE_HOST
u32 RogueText_DumpRumor(const struct RogueRegion *r, u8 place, u32 rng, const struct RogueTextState *state, char *out, u32 size);
#endif
// PROF. BIRCH (rogue_text_birch.h). What he remembers of a journey, for his
// "welcome back" (the last one, said in the last region) and his epilogue
// (the one just ended): from the record and the run's vars, nothing saved.
enum { RBIRCH_NONE, RBIRCH_LOST, RBIRCH_WON, RBIRCH_LEFT };
struct RogueBirchMemory
{
    u8 outcome;   // RBIRCH_*: lost, won, or left behind for a new land
    u8 badges;    // badges it won
    u8 fallen;    // POK@MON lost on it
    u8 place;     // where it ended, a place of the context's region (0xFF unknown); the epilogue's
    u8 dungeon;   // the dungeon it ended in (0xFF none); the epilogue's
    u8 runs;      // journeys started, this one included
    const char *friendName; // the fallen friend (FALLEN FRIEND) if lost on it, else NULL
    u8 same;      // the next land is this one again
    u8 league;    // how far into the LEAGUE a lost run got: 1-4 an ELITE FOUR member, 5 the CHAMPION; 0 not there
    u8 dex;       // a win's POKeDEX upgrade, handed out in the epilogue: 1 the NATIONAL DEX, 2 the GLOBAL DEX; 0 none
    u8 given;     // the third win's teaser (RUN 0): he was given the new land, not found it
    const char *endingLine; // after a RUN 0 ending, once: his welcome remembers it (NULL none)
};
// How he describes a region: on his map in the speech (the first time at
// length), or in the lab when the map did not show it.
enum { RBIRCH_DESC_LAB, RBIRCH_DESC_MAP, RBIRCH_DESC_MAP_FIRST };
u32 RogueText_BirchRegion(struct RogueTextCtx *c, u8 mode, char *out, u32 size);
u32 RogueText_BirchWelcomeBack(struct RogueTextCtx *c, const struct RogueBirchMemory *m, char *out, u32 size);
// His lines in the lab (BIRCH_LINE_* in include/constants/rogue.h).
enum { RBIRCH_LAB_WELCOME, RBIRCH_LAB_CHOOSE, RBIRCH_LAB_GOODLUCK, RBIRCH_LAB_AFTER, RBIRCH_LAB_NEWS, RBIRCH_LAB_COUNT };
u32 RogueText_BirchLab(struct RogueTextCtx *c, u8 what, char *out, u32 size);
// The epilogue after a win or a loss (the run summary's first page), wrapped
// to width characters, all its lines one after another (BIRCH_EPILOGUE_WIDTH
// for the summary page).
#define BIRCH_EPILOGUE_WIDTH 28
u32 RogueText_BirchEpilogue(struct RogueTextCtx *c, const struct RogueBirchMemory *m, u32 width, char *out, u32 size);
// What the CHAMPION (the wanderer) says walking the player into the HALL OF FAME.
u32 RogueText_HallOfFame(struct RogueTextCtx *c, char *out, u32 size);
// The home room the game builds for a house (src/rogue_town.c sHomes) and
// its furniture.
#define RFURN_TV       (1 << 0)
#define RFURN_BOOKS    (1 << 1)
#define RFURN_PICTURES (1 << 2)
#define RFURN_CABINET  (1 << 3) // a glass cabinet (src/rogue_town.c Rogue_GetHomeThingScript)
u8 RogueText_HomeRoom(const struct RogueRegion *r, u8 place, u8 house);
u8 RogueText_HomeFurniture(const struct RogueRegion *r, u8 place, u8 house);
// TVs (a house's, by its number) and books on a shelf.
bool8 RogueText_HouseLoreTV(const struct RogueRegion *r, u8 place, u8 house);
void RogueText_TV(struct RogueTextCtx *c, u8 house, char *out, u32 size);
enum { RBOOK_HOME, RBOOK_PICTURE, RBOOK_CENTER };
void RogueText_Book(struct RogueTextCtx *c, u8 kind, u8 house, char *out, u32 size);
#ifdef ROGUE_HOST
#include <stdio.h>
struct TextDump
{
    const struct RogueRegion *r;
    FILE *f;
    const char *domain;
    const struct RogueTextState *state;
    const char *stateName;
};
void TextDump_Line(struct TextDump *d, u8 place, u8 speaker, const char *who, const char *bank, s32 index, const char *text);
void TextDump_Say(struct TextDump *d, struct RogueTextCtx *c, const char *who, const char *bank, s32 index, const char *t);
void TextDump_Bank(struct TextDump *d, struct RogueTextCtx *c, const char *who, const struct RogueTextBank *b);
void RogueText_DumpRegion(const struct RogueRegion *r, FILE *f, const struct RogueTextState *state, const char *stateName);
#endif

// The villain teams (src/rogue_gen_hideout.h): one, two or three of Team
// Aqua, Team Magma and Team Rocket per region, each with its hideout under a
// town of the main road. Team 0, the region's main team, is the one whose
// grunts may block the road (the others' hideouts are optional); the i-th
// hideout among the dungeons is team i's.
#define ROGUE_VILLAIN_AQUA   0
#define ROGUE_VILLAIN_MAGMA  1
#define ROGUE_VILLAIN_ROCKET 2
#define ROGUE_VILLAIN_IDS    3
#define ROGUE_MAX_TEAMS      3
// The main team's identity (ROGUE_VILLAIN_*).
u8 RogueGen_VillainTeam(const struct RogueRegion *r);
// The main team's hideout dungeon, or -1 if the region has no villains.
s32 RogueGen_Hideout(const struct RogueRegion *r);
// How many teams the seed asks for (1 to 3) and who they are, in order:
// from the seed alone, so the leaders' draw can know (Giovanni and Team
// Rocket). The region has as many as its hideouts (RogueGen_TeamCount).
u8 RogueGen_TeamsWanted(u32 seed, u8 *ids);
// The teams the region has, team t's identity, its hideout dungeon (-1),
// the team of a hideout dungeon (0xFF for none), and the team index with an
// identity (0xFF if absent).
u8 RogueGen_TeamCount(const struct RogueRegion *r);
u8 RogueGen_TeamId(const struct RogueRegion *r, u8 team);
s32 RogueGen_TeamHideout(const struct RogueRegion *r, u8 team);
u8 RogueGen_HideoutTeam(const struct RogueRegion *r, s32 dungeon);
u8 RogueGen_TeamWithId(const struct RogueRegion *r, u8 id);
// The legend dungeon team t's leader waits before (its gate is
// ROGUE_GATE(RGATE_VILLAIN, t)), or -1.
s32 RogueGen_TeamLeaderDungeon(const struct RogueRegion *r, u8 team);
// TRUE where team t's leader is its hideout's boss (TEAM ROCKET's BOSS with
// no legend's way to keep, as in FireRed's Rocket Hideout).
bool8 RogueGen_TeamBossInHideout(const struct RogueRegion *r, u8 team);
// Everything about the teams worked out at once, for passes over many
// slots: each team's hideout, the route of grunts before its town (hint,
// mask: a bit a trainer), and on that route a grunt of the team before it
// (guest: a bit a trainer, of team t - 1), where two teams clash.
struct RogueTeams
{
    u8 n;
    u8 id[ROGUE_MAX_TEAMS];
    s8 hideout[ROGUE_MAX_TEAMS];
    u8 hint[ROGUE_MAX_TEAMS];
    u8 mask[ROGUE_MAX_TEAMS];
    u8 guest[ROGUE_MAX_TEAMS];
};
void RogueGen_Teams(const struct RogueRegion *r, struct RogueTeams *tm);
// The team (0xFF for none) and role (RVROLE_*) of a trainer slot; clash
// TRUE for a grunt on a route shared with another team.
u8 RogueGen_SlotTeam(const struct RogueRegion *r, const struct RogueTeams *tm, u32 slot, u8 *role, bool8 *clash);
// The admin's and the leader's trainer slots of team t.
u8 RogueGen_TeamAdminSlot(u8 team);
u8 RogueGen_TeamLeaderSlot(u8 team);
// Names by identity, and team t's admin (which of its two, and the name).
extern const char *const gRogueTeamNames[ROGUE_VILLAIN_IDS];
extern const char *const gRogueTeamBosses[ROGUE_VILLAIN_IDS];
u8 RogueGen_TeamAdmin(const struct RogueRegion *r, u8 team);
const char *RogueGen_TeamAdminName(const struct RogueRegion *r, u8 team);
// TRUE if the team's grunts block the way on out of the hideout's town
// until its admin is beaten (FALSE: a hideout with no blockade, or none).
bool8 RogueGen_VillainBlockade(const struct RogueRegion *r);
// The route where the main team's grunts first show up, before the
// hideout's town, or 0xFF; mask gets a bit for each of its trainers who is a
// grunt (RogueGen_Teams for every team).
u8 RogueGen_VillainHint(const struct RogueRegion *r, u8 *mask);

// Items (src/rogue_gen_items.h): one catalogue every source draws from, and
// a ledger worked out from the seed that keeps a run's counts in hand.
enum RogueItemRole
{
    RIR_HEAL,    // HP: potions, drinks
    RIR_STATUS,  // status cures
    RIR_PP,      // Ethers and Elixirs
    RIR_BALL,
    RIR_BATTLE,  // X items
    RIR_HELD,    // held items
    RIR_TYPE,    // type items (Charcoal...)
    RIR_BERRY,   // berries that work in battle
    RIR_EVO,     // evolution stones and the like
    RIR_CANDY,   // Rare and Exp. Candies
    RIR_VITAMIN, // vitamins, PP Up, mints, Ability Capsule
    RIR_TM,
    RIR_MONEY,   // things to sell: Nuggets, Pearls...
    RIR_FIELD,   // Repels, Escape Rope
    RIR_COUNT,
};

// Where an item comes from, for RogueGen_ItemFor.
enum RogueItemSource
{
    RITEMS_BALL,   // index: item flag (FLAG_ROGUE_ITEM_0 + index)
    RITEMS_HIDDEN, // index: hidden item (FLAG_ROGUE_HIDDEN_0 + index)
    RITEMS_GIFT,   // index: town number, the gift house
    RITEMS_FOLK,   // index: town number, a townsperson
    RITEMS_MASTER, // index: dungeon
    RITEMS_RIVAL,  // the rival's gift (RIVAL'S GIFT)
    RITEMS_SIGNATURE, // index: town number, the town signature's small gift (RogueGen_SignatureItem)
    RITEMS_FEATURE,   // index: route feature (RFEAT_*), its reward (RogueGen_FeatureItem)
};

// Specialty counters in themed towns' Marts.
enum RogueShop
{
    RSHOP_NONE,
    RSHOP_BATTLE,  // X items (Verdant towns)
    RSHOP_VITAMIN, // the Energy Guru's vitamins (harbors)
    RSHOP_STONE,   // evolution stones (islands)
    RSHOP_BALL,    // Devon's balls (stone towns)
    RSHOP_TM,      // the department store's TMs (coves)
    RSHOP_SEA,     // drinks and sea balls (beaches)
    RSHOP_HELD,    // held items (ash towns' exchange)
    RSHOP_HERB,    // the herb shop (spring towns)
    RSHOP_TYPE,    // type items (treetop towns)
    RSHOP_COUNT,
};

#define ROGUE_ITEMS_HARDCORE 1 // flags: no battle items
#define ROGUE_MAX_HIDDEN 96    // hidden items in a region (vanilla's hidden item flags)
#define ROGUE_HIDDEN_PER_PLACE 3

// The item (and how many) a source hands out, worked out from the seed.
u16 RogueGen_ItemFor(const struct RogueRegion *r, u8 source, u8 index, u8 *qty, u8 flags);
u8 RogueGen_ItemRole(u16 item); // enum RogueItemRole, RIR_COUNT if not in the catalogue
// Every item the region hands out, in the ledger's order (host tools).
typedef void (*RogueItemSink)(u32 source, u32 index, u16 item, u8 qty, u32 stage, u32 taste);
void RogueGen_TallyItems(const struct RogueRegion *r, u8 flags, RogueItemSink sink);
bool8 RogueGen_HasFolkGift(const struct RogueRegion *r, u8 town);
u8 RogueGen_HiddenCount(const struct RogueRegion *r, u8 place);
u8 RogueGen_HiddenBase(const struct RogueRegion *r, u8 place);
u16 RogueGen_GymTM(const struct RogueRegion *r, u8 gym, u8 type);
u8 RogueGen_SpecialtyShop(const struct RogueRegion *r, u8 place);
// flags: ROGUE_ITEMS_HARDCORE turns the battle counter into a berry stall.
u32 RogueGen_SpecialtyStock(const struct RogueRegion *r, u8 place, u8 badges, u16 *out, u32 max, u8 flags);

// Helpers for the plan.
const struct RogueLink *RogueGen_FindLink(const struct RoguePlace *p, u8 target);
// The keys (ROGUE_KEY bits) that the won gyms (a bit per gym) give.
u8 RogueGen_KeysWon(const struct RogueRegion *r, u8 won);
// Marks in seen (ROGUE_MAX_PLACES long) each place reachable from the first
// town with these keys.
void RogueGen_ReachWith(const struct RogueRegion *r, u8 keys, u8 *seen);
bool8 RogueGen_IsGymTown(const struct RogueRegion *r, u8 place);
// The role of a town's house-th house, an ordinary home, given the interiors
// of its nHouses houses.
u8 RogueGen_HomeRole(const struct RogueRegion *r, u8 place, const u8 *interiors, u32 nHouses, u32 house);
// TRUE if the trainer with this index on a route is a water trainer
// (Fisherman, Swimmer, Sailor, Tuber): the map puts them by the water, and
// no other trainer is one.
bool8 RogueGen_TrainerByWater(const struct RogueRegion *r, u8 place, u8 index);
// v1.1 sea: what a sea route is like (src/rogue_gen_sea.h): the open sea
// with islands, Route 132 to 134's rock maze with currents (on Pacifidlog's
// tileset), or Route 109's shallows and sandbars. SEA_OPEN for any other place.
enum { SEA_OPEN, SEA_MAZE, SEA_SHALLOWS, SEA_KIND_COUNT };
u8 RogueGen_SeaKind(const struct RogueRegion *r, u8 place);
// TRUE if a route trainer is a Swimmer, out on the water (a maze's or the
// shallows'; RogueGen_TrainerByWater's people).
bool8 RogueGen_TrainerSwims(const struct RogueRegion *r, u8 place, u8 index);
// v1.1 sea: the rare POKeMON in the grass of an island at the end of a side
// route out over the sea, or SPECIES_NONE.
u16 RogueGen_IslandRare(const struct RogueRegion *r, u8 place);

// The region map, drawn with the tiles of Hoenn's region map
// (graphics/pokenav/region_map/map.png): land and coasts, forests and hills,
// route bars, sea routes, towns and cities. The plan is a grid of
// ROGUE_GRID_W x ROGUE_GRID_H cells, one place a cell (gridX, gridY).
// Places sit spacing tiles apart: two for the map screen, with a tile of road
// between linked neighbours, or one for the Pokédex.
//
// At spacing 2 the whole region fits one screen: tiles (0, 0) to
// (ROGUE_RMAP_VIEW_W - 1, ROGUE_RMAP_VIEW_H - 1), sea all round, and every
// placeX, placeY inside that area. Gym cities and the summit are drawn as
// Hoenn's two-tile capsules: the place's tile and the one east of it (or
// south of it, when east is taken); placeX, placeY is the first of the two.
#define ROGUE_GRID_W 14
#define ROGUE_GRID_H 7
#define ROGUE_RMAP_W 32   // the tilemap is this wide and tall, in tiles
#define ROGUE_RMAP_H 64
#define RMAP_TILE_CITY_W 0x4B // the first halves of the two-tile cities the map draws
#define RMAP_TILE_CITY_N 0x66
#define RMAP_TILE_COUNT  233  // Hoenn's tiles; then the map screen loads:
#define RMAP_TILE_CRATER 233  // the volcano's crater, four tiles (src/data/rogue_region_map_crater.h)
#define RMAP_TILE_CRATER_TOWN (RMAP_TILE_CRATER + 4) // v1.1 towns2: a crater city's mark (4 tiles), a crater town's (4)
#define RMAP_IS_CRATER_TOWN(t) ((t) >= RMAP_TILE_CRATER_TOWN && (t) < RMAP_TILE_CRATER_TOWN + 8 && !(((t) - RMAP_TILE_CRATER_TOWN) & 3))
#define ROGUE_RMAP_VIEW_W 30 // the visible width, which the map is centred in
#define ROGUE_RMAP_VIEW_H 15 // the height of the map at spacing 2
#define ROGUE_RMAP_WORK ((ROGUE_RMAP_W + 1) * (ROGUE_RMAP_H + 1) * 2) // bytes of work space

// Fills tiles (ROGUE_RMAP_W x ROGUE_RMAP_H) and each place's tile position.
// Returns the height used, in tiles (ROGUE_RMAP_VIEW_H at spacing 2). work is
// ROGUE_RMAP_WORK bytes.
u32 RogueGen_DrawRegionMap(const struct RogueRegion *r, u16 *tiles, s8 *placeX, s8 *placeY, u8 *work, u32 spacing);

// What the professor says a region is like (its two strongest traits), from
// its routes against the average region. A trait is only said when the
// region map shows it: the volcano's crater (Mt. Chimney's, from Hoenn's map)
// for ash, a lake for lakes and rivers.
enum RogueTrait
{
    RTRAIT_LAKES,
    RTRAIT_SEA,
    RTRAIT_WOODS,
    RTRAIT_ROCK,
    RTRAIT_ASH,
    RTRAIT_FIELDS,
    RTRAIT_COUNT,
};
void RogueGen_RegionTraits(const struct RogueRegion *r, u8 *first, u8 *second);
// The grid cell the region map draws the volcano's crater on (its tile and
// the three east and south of it), or FALSE if the region has no volcano:
// the land's own volcano, or beside the first ash town. Dungeons that belong
// by a volcano can stand by it.
bool8 RogueGen_VolcanoCell(const struct RogueRegion *r, s8 *gx, s8 *gy);

// Region names. Every seed below ROGUE_REGION_COUNT has a name of up to
// ROGUE_REGION_NAME_LEN letters made of three syllables, and every such name
// leads back to its seed, so a region's name is its seed code.
#define ROGUE_REGION_COUNT    16384
#define ROGUE_REGION_NAME_LEN 9
// A run's code: the name, a dash and the POKeDEX mix's letter.
#define ROGUE_RUN_CODE_LEN    (ROGUE_REGION_NAME_LEN + 2)

// RUN 0 (v1.0): the secret last region has a seed of its own, the first one
// past the names, so no typed name and no new run reaches it. The generator
// and the game both know a region is RUN 0 by its seed; its name is "RUN 0".
#define ROGUE_RUN0_SEED       ROGUE_REGION_COUNT
#define ROGUE_RUN0_NAME       "RUN 0"
bool8 RogueGen_IsRun0(const struct RogueRegion *r);

// Writes the ASCII name of a seed into out (ROGUE_REGION_NAME_LEN + 1 long).
void RogueGen_RegionName(u32 seed, char *out);
// The seed of an ASCII region name, or -1 if it is not one. Case does not matter.
s32 RogueGen_ParseRegionName(const char *name);
// True if a seed's name reads badly (a repeated syllable, too short, or a
// rude word). New runs skip those seeds; typed names still work.
bool8 RogueGen_IsAwkwardRegionName(u32 seed);

// ---------------------------------------------------------------------------
// Town signatures (src/rogue_gen_signatures.h): what each town is known for.
// One per town, none shared within a region, decided from the plan on its own
// random stream, so nothing else in the region moves.
enum RogueSignature
{
    RSIG_NONE,      // the summit
    // Built on the map.
    RSIG_FISHING,   // piers or a pond, fishermen
    RSIG_HOT_SPRING,// Lavaridge's spring under the spring mesa
    RSIG_SPACE,     // Mossdeep's Space Center
    RSIG_MARKET,    // Slateport's market
    RSIG_MUSEUM,    // the Oceanic or Lilycove Museum
    RSIG_BERRY,     // a berry farm: fenced soil plots
    RSIG_FLOWERS,   // fenced flower fields
    RSIG_RANCH,     // a fenced pasture with Miltank and Mareep
    RSIG_WEATHER,   // the Weather Institute
    RSIG_FOSSIL,    // a fossil dig
    RSIG_MINING,    // a mine in the rock face
    RSIG_DOJO,      // a dojo, whose disciples battle once per run
    RSIG_BUILT_COUNT,
    // Trades with no landmark on the map: told in text only.
    RSIG_POTTERY = RSIG_BUILT_COUNT,
    RSIG_WEAVING,
    RSIG_BAKING,
    RSIG_TEA,
    RSIG_CARVING,
    RSIG_HONEY,
    RSIG_LUMBER,
    RSIG_SALT,
    RSIG_COUNT,
};

// The town's signature (enum RogueSignature); RSIG_NONE for a place that is
// not a town, or the summit.
u8 RogueGen_TownSignature(const struct RogueRegion *r, u8 place);
// A signature's trade (RTRADE_*, gRogueTrades: its words, folk and goods)
// and landmark (RLANDMARK_*, gRogueLandmarks) for the town's facts
// (src/rogue_gen_facts.h), or 0xFF where the facts choose (a museum town's
// trade, a trade told only in words's landmark).
u8 RogueGen_SignatureTrade(const struct RogueRegion *r, u8 place);
u8 RogueGen_SignatureLandmarkId(const struct RogueRegion *r, u8 place);
// The town's landmark's name in ASCII, as its facts give it ("the HOT SPRING",
// "ASHTON MINE"); out at least 32 long.
void RogueGen_SignatureLandmark(const struct RogueRegion *r, u8 place, char *out);
// A signature's gift Pokémon in this town, rolled per region: its species and
// level, or SPECIES_NONE (0) if this town gives none.
u16 RogueGen_SignatureGift(const struct RogueRegion *r, u8 place, u8 *level);
// Berry trees on a berry farm use these berry tree ids (after every route's).
#define ROGUE_FARM_BERRY_BASE 100
#define ROGUE_FARM_BERRIES    4
// A signature's small item gift in this town, once per run (a LAVA COOKIE at
// the hot spring, MOOMOO MILK at the ranch), or ITEM_NONE.
u16 RogueGen_SignatureItem(const struct RogueRegion *r, u8 place, u8 *qty);
// The place whose town has a signature, or 0xFF.
u8 RogueGen_SignatureTown(const struct RogueRegion *r, u8 signature);

// ---------------------------------------------------------------------------
// Route features (src/rogue_gen_features.h): the one thing a route is
// remembered for, as Route 117's Day Care or Route 109's Seashore House.
// At most one a route, none on some, each at most once a region, decided
// from the plan on its own random stream; only route maps change.
enum RogueRouteFeature
{
    RFEAT_NONE,
    RFEAT_DAYCARE,  // Route 117's Day Care: the house and a fenced yard; the couple give a POKeMON
    RFEAT_BERRY,    // Route 123's BERRY MASTER: his house and a garden of berry trees
    RFEAT_WORKSHOP, // Route 113's glass workshop in the ash; the glassblower gives a flute
    RFEAT_SEASHORE, // Route 109's Seashore House: three trainers inside, SODA POP for beating them
    RFEAT_SNORLAX,  // a SNORLAX asleep across a pocket of the route, woken with a POKe FLUTE
    RFEAT_FAMILY,   // Route 111's Winstrates: a family who battle one after another
    RFEAT_GAUNTLET, // Nugget Bridge: beat every trainer of the crossing for a NUGGET
    RFEAT_GURU,     // the fishing guru's hut: show him a WATER POKeMON caught here
    RFEAT_SAILOR,   // Mr. Briney's cottage: the old sailor and his Wingull, before the sea
    RFEAT_COUNT,
};
// The route's feature (enum RogueRouteFeature), RFEAT_NONE for none.
u8 RogueGen_RouteFeature(const struct RogueRegion *r, u8 place);
// The route with a feature, or 0xFF.
u8 RogueGen_FeatureRoute(const struct RogueRegion *r, u8 feature);
// The routes with a double battle pair (k 0-2), 0xFF for none.
u8 RogueGen_PairRoute(const struct RogueRegion *r, u8 k);
// The pair a route has (0-2), or -1.
s8 RogueGen_RoutePair(const struct RogueRegion *r, u8 place);
// Which pair class a pair is (RPAIR_*).
enum { RPAIR_TWINS, RPAIR_YOUNG_COUPLE, RPAIR_OLD_COUPLE, RPAIR_SIS_AND_BRO, RPAIR_SR_AND_JR, RPAIR_COUNT };
u8 RogueGen_PairKind(const struct RogueRegion *r, u8 k);
// The feature's gift POKeMON (the Day Care's, the old sailor's) and level,
// or SPECIES_NONE.
u16 RogueGen_FeatureGift(const struct RogueRegion *r, u8 feature, u8 *level);
// The feature's item reward (in the ledger, RITEMS_FEATURE), or ITEM_NONE.
u16 RogueGen_FeatureItem(const struct RogueRegion *r, u8 feature, u8 *qty);
// The level of SNORLAX, at its route.
u8 RogueGen_SnorlaxLevel(const struct RogueRegion *r);
// Berry trees of the BERRY MASTER's garden use these berry tree ids.
#define ROGUE_GARDEN_BERRY_BASE (ROGUE_FARM_BERRY_BASE + ROGUE_FARM_BERRIES)
#define ROGUE_GARDEN_BERRIES    4
// (v1.1) A berry farm route's trees (src/rogue_gen_berry.h): the BERRY
// MASTER's fenced field, two rows of up to five, after the garden's ids
// (109 to 118 of the save's BERRY_TREES_COUNT).
#define ROGUE_BFARM_BERRY_BASE (ROGUE_GARDEN_BERRY_BASE + ROGUE_GARDEN_BERRIES)
#define ROGUE_BFARM_ROW        5
#define ROGUE_BFARM_BERRIES    (2 * ROGUE_BFARM_ROW)
// Is this route a berry farm (the BERRY MASTER's route in some regions)?
bool8 RogueGen_BerryFarmRoute(const struct RogueRegion *r, u8 place);
// What a farm's row (0 or 1) grows this run: an ITEM_*_BERRY.
u16 RogueGen_FarmRowBerry(u32 seed, u8 row);
// Route facts: what people call a route and why, its feature and who lives by
// it. Pure functions of the plan on their own streams (salts 42000 on).
struct RogueRouteFacts
{
    u8 place;
    u8 feature;   // RFEAT_*
    u8 nick;      // the route's local name (gRogueRouteNicks), 0xFF for none
    u8 resident;  // the feature's person's name (an index), 0xFF for none
    u8 pet;       // the old sailor's Wingull's name (an index)
};
void RogueGen_RouteFacts(const struct RogueRegion *r, u8 place, struct RogueRouteFacts *f);
// The route's local name ("the LONG FORD"), its feature's name ("the DAY
// CARE"), the resident's name: into out (at least 24), FALSE for none.
bool8 RogueGen_RouteNick(const struct RogueRegion *r, u8 place, char *out);
bool8 RogueGen_FeatureName(const struct RogueRegion *r, u8 place, char *out);
bool8 RogueGen_ResidentName(const struct RogueRegion *r, u8 place, char *out);
// The feature trainers' slots (constants/rogue.h ROGUE_SLOT_PAIR_0...): the
// route they stand on, or 0xFF.
u8 RogueGen_FeatureSlotPlace(const struct RogueRegion *r, u32 slot);
// The battling family's name (HARDING) and the old sailor's Wingull's (PEEKO).
const char *RogueGen_FamilyBattlers(const struct RogueRegion *r);
const char *RogueGen_SailorPet(const struct RogueRegion *r);
// What route features' people say (src/rogue_text_features.h), in ASCII.
enum { RFEATTEXT_TALK, RFEATTEXT_DONE, RFEATTEXT_GIFT, RFEATTEXT_GIFT_AFTER, RFEATTEXT_NOT_YET, RFEATTEXT_SIGN,
       RFEATTEXT_SNORLAX, RFEATTEXT_KID, RFEATTEXT_KID_AFTER, RFEATTEXT_COUNT };
u32 RogueText_FeatLine(const struct RogueRegion *r, u8 place, u8 what, u32 k, char *out, u32 size);
u32 RogueText_FeatTrainerLine(const struct RogueRegion *r, u32 slot, u8 field, char *out, u32 size);
u32 RogueText_GauntletLine(const struct RogueRegion *r, u8 place, u32 left, u32 k, char *out, u32 size);
struct RogueTextCtx;
u32 RogueText_RouteFactLine(struct RogueTextCtx *c, u32 k, char *out, u32 size);

// Small deterministic RNG shared with the game side.
u32 RogueRng_Next(u32 *state);
u32 RogueRng_Range(u32 *state, u32 n);
u32 RogueRng_Hash(u32 seed, u32 salt);

#endif // GUARD_ROGUE_GEN_H
