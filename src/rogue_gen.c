// Procedural region generator for roguemon.
//
// A region is a plan of towns, routes and caves (see include/rogue_gen.h),
// and each place's map is generated on its own when the game needs it. Each
// outdoor map has two layers. A coarse grid of 2x2
// block "units" says what is solid (trees, water, rock, buildings) and what is
// open. A cover layer, one entry per block, says what grows on open ground
// (tall grass, worn paths, sand roads, flowers). Both are then rendered to real
// metatiles from the General + Petalburg tilesets with autotiling rules copied
// from the vanilla Hoenn maps. The cave uses the coarse grid with the General +
// Cave tilesets.
//
// Everything here is deterministic for a given seed and uses its own RNG, so a
// save file only needs to store the seed.

#include "rogue_gen.h"

#include <string.h>
#ifdef ROGUE_DEBUG
#include <stdio.h>
#endif

#ifdef ROGUE_HOST
#define ROGUE_EWRAM
#define ROGUE_FAST
#define YIELD()
#else
#define ROGUE_EWRAM EWRAM_DATA
// The busiest searches once ran from IWRAM, but the 1.2 KB they took there
// left the stack too little room for the expansion's sprite decompressor,
// which runs code copied onto the stack. They now run from ROM.
#define ROGUE_FAST __attribute__((noinline))
// The game may run the generator in the background, in the spare time at the
// end of each frame (see src/rogue.c). It hands the frame back before the
// screen refresh, at points like these, which are cheap to pass otherwise.
// The work between two of them must stay short (under about 20 scanlines,
// see ROGUE_YIELD_LINE), or walking drops frames: a new loop over a whole
// map yields in it. tools/roguemon/yieldprof.py finds the long stretches.
extern bool8 gRogueGenBackground;
void RogueThread_Yield(void) __attribute__((long_call));
#ifdef ROGUE_YIELD_PROFILE
// Functions keep their own names in the profile.
#pragma GCC optimize("no-inline-functions-called-once")
void RogueGen_YieldPoint(u32 line, const char *file);
#define YIELD() RogueGen_YieldPoint(__LINE__, __FILE__)
#else
#define YIELD() do { if (gRogueGenBackground && ROGUE_LATE_IN_FRAME()) RogueThread_Yield(); } while (0)
#endif
#endif

#include "data/rogue_stamps.h"
#include "data/rogue_region_map.h"


// Coarse cell types, one per 2x2 block unit. In the cave, C_TREE is solid rock
// and C_ROAD is floor.
enum
{
    C_TREE,
    C_OPEN,  // open ground
    C_ROAD,  // open ground that must stay clear: the way through the map
    C_BLDG,
    C_WATER,
    C_SHORE, // open ground just below water, drawn with the bank
    C_ROCK,  // part of a mesa
    C_LEDGE, // open ground with a one-way ledge along its bottom edge
    C_KEEP,  // open ground reserved for a berry patch
    C_DECOR, // solid scenery drawn from gen->decor (fences, gardens...)
    C_SEA,   // sea or river water, surfable, with banks drawn by the water
    C_FORD,  // shallow water where the trail wades across a river
    C_CLIFF, // towns: open ground on a terrace, with the terrace's rock rim along its lower sides
};

// Ground cover, one per block, on open ground.
enum
{
    V_NONE,
    V_TALL,   // tall grass
    V_PATH,   // worn light-grass path
    V_SAND,   // sand road
    V_FLOWER,
    V_MAT,    // bare patch in front of a door
    V_ROAD,   // the town theme's own road (cobbles, paving...)
    V_GROUND, // the map's special ground: sand (beaches, deserts) or volcanic ash
};

// (v1.1 towns2, src/rogue_gen_towns2.h) A raft village or a crater town:
// it lays out its own map, so no dungeon has its way in there.
#define TOWNS2_TOWN(p) ((p)->kind == RPLACE_TOWN && ((p)->theme == RTHEME_RAFT || (p)->theme == RTHEME_CRATER))

// A route's seam toward a paved town: grass, with the route's own way kept
// further back than by a seam of plain grass (SeamWay).
#define V_SEAM_GAP 0xFE

enum { GROUND_SAND, GROUND_ASH };

#define MAX_CW (ROGUE_MAX_W / 2)
#define MAX_CELLS ROGUE_MAX_CELLS
#define MAX_PATH 320
#define MAX_WALK 600
#define MAX_ROOMS 8
#define MAX_DECOR 12
#define MAX_POCKETS 6
#define MAX_LOTS 12 // buildings in a town
// How far the sea reaches into a coast route, in units, at most.
#define SEA_DEPTH 6

// Raw block values (metatile | collision << 10 | elevation << 12).
#define B_GRASS        0x3001
#define B_SHORE        0x3002
#define B_FLOWERS      0x3004
#define B_TALL_GRASS   0x300D
#define B_SIGN         0x0403
#define B_DOORMAT      0x3201
#define B_TREE_TL      0x05D4
#define B_TREE_TR      0x05D5
#define B_TREE_TL_END  0x05D6
#define B_TREE_TR_END  0x05D7
#define B_TREE_BL      0x05DC
#define B_TREE_BR      0x05DD
#define B_TREE_BL_BASE 0x05E4
#define B_TREE_BR_BASE 0x05E5
#define B_TREE_BL_END  0x05E6
#define B_TREE_BR_END  0x05E7
#define B_TREE_ON_WOODS 0x05EC // a trunk among the crowns of deep woods below, and 0x05ED (Route 104)
#define B_CANOPY_L     0x31CE
#define B_CANOPY_R     0x31CF
#define B_CANOPY_TALL_L 0x31C6
#define B_CANOPY_TALL_R 0x31C7
// Both path styles are 3x3 sets laid out the same way in the tileset: the
// top left block, then +1 and +2 across and +8 and +16 down.
#define B_SAND_TL      0x3118
#define B_PATH_TL      0x31D0
#define B_WATER_TL 0x10B0
#define B_WATER_T  0x10B1
#define B_WATER_TR 0x10B2
#define B_WATER_L  0x10B8
#define B_WATER_C  0x10A1
#define B_WATER_R  0x10BA
#define B_LEDGE_L  0x34D5
#define B_LEDGE    0x3487
#define B_LEDGE_R  0x34D6
#define B_SOIL_L   0x314B
#define B_SOIL     0x354C
#define B_SOIL_R   0x314D
#define B_SOIL_FRONT_L 0x3113
#define B_SOIL_FRONT   0x3114
#define B_SOIL_FRONT_R 0x3115
#define B_ROCK_TL  0x0468
#define B_ROCK_T   0x0469
#define B_ROCK_TR  0x046A
#define B_ROCK_L   0x0470
#define B_ROCK_C   0x0471
#define B_ROCK_R   0x0472
#define B_ROCK_FACE 0x04A9
#define B_ROCK_BL  0x0478
#define B_ROCK_B   0x0479
#define B_ROCK_BR  0x047A
#define B_MOUTH_TOP 0x049F
#define B_MOUTH     0x00A7
#define B_MOUTH_TOP_PORCH 0x0464 // the mouth's top between a face's slanting ends
// Cave interior (General + Cave tilesets).
#define B_CAVE_FLOOR   0x3211
#define B_CAVE_FLOOR2  0x3201
#define B_CAVE_SOLID   0x0611
#define B_CAVE_ROCK    0x0666
#define B_CAVE_WALL_S  0x0619 // wall cell with floor to its south
#define B_CAVE_WALL_N  0x0623
#define B_CAVE_WALL_E  0x0612
#define B_CAVE_WALL_W  0x0610
#define B_CAVE_IN_SE   0x061A // floor to the south and east
#define B_CAVE_IN_SW   0x0618
#define B_CAVE_IN_NE   0x0622
#define B_CAVE_IN_NW   0x0620
#define B_CAVE_OUT_SE  0x061B // floor only diagonally to the south east
#define B_CAVE_OUT_SW  0x061C
#define B_CAVE_MAT     0x023C
#define B_CAVE_LIGHT_L 0x0658
#define B_CAVE_LIGHT   0x0659
#define B_CAVE_LIGHT_R 0x065A
#define B_CAVE_DOOR    0x3214
#define B_CAVE_DOOR_TOP 0x060C
#define B_LADDER_UP    0x323E // a ladder standing on the floor, up to the floor above
#define B_LADDER_DOWN  0x323F // a hole with a ladder, down to the floor below
// Sea and rivers (General): open sea, and the banks drawn inside water cells
// against grass and against sand. See tools/roguemon/route_tiles.md.
#define B_SEA          0x1170
#define B_LIP          0x3002 // grass with water to the north
#define B_SAND         0x3124 // beach and desert sand, with footprints
#define B_SAND_SHORE   0x311C // sand with water to the north
#define B_ASH          0x3218 // volcanic ash ground (Fallarbor)
#define B_ASH_TL       0x32D0 // ash against grass, a 3x3 set like the paths
#define B_ASH_PINK_TL  0x32D3 // ash against pink ash dirt, the same 3x3 set (Route 113)
#define B_ASH_GRASS    0x320A // ash-covered grass with encounters (Fallarbor)
#define B_LONG_GRASS   0x3015 // long grass with encounters
#define B_LONG_GRASS_EDGE 0x3208 // its lower edge: blades over grass (Fortree only)
// Town ground from Dewford and Fallarbor (their own tilesets).
#define B_ISLAND_TREE  0x0643 // a bushy island tree crown on sand
#define B_ISLAND_FOOT  0x063A // its lowest crown, with the trunk starting
#define B_ISLAND_TOP   0x3239 // the tip of an island tree, on the sand above it
#define B_ISLAND_TIP   0x0639 // the same tip within a grove, solid
#define B_TOWN_ASH     0x3279 // Fallarbor's ash dirt
#define B_TOWN_ASH_TUFT 0x329F // the same with a tuft of grass
// Fallarbor's ash trees. One tree is 2x2 blocks: its four quarters on bare
// ash. In a forest the trees stand on a diagonal lattice, each one block
// down and across from the last, so most blocks show the lower half of one
// tree over the upper half of the next; Route 113 picks which of two
// drawings by the row. Each set below is: top left, top right, bottom left,
// bottom right alone; then top left over bottom right on an even and an odd
// row; then top right over bottom left on an even and an odd row. The grey
// set stands on a route's grey ash (Route 113), the pink one on a town's
// pink ash dirt (Fallarbor).
enum { ASHQ_TL, ASHQ_TR, ASHQ_BL, ASHQ_BR, ASHQ_TLBR_EVEN, ASHQ_TLBR_ODD, ASHQ_TRBL_EVEN, ASHQ_TRBL_ODD };
static const u16 sAshTreeGrey[8] = { 0x0604, 0x0605, 0x060C, 0x060D, 0x066D, 0x0676, 0x066E, 0x0675 };
static const u16 sAshTreePink[8] = { 0x0606, 0x0607, 0x060E, 0x060F, 0x066B, 0x0674, 0x066C, 0x0673 };
// Deep woods: small trees with no 2x2 grid.
#define B_WOODS_CROWN  0x04C7
#define B_WOODS_CROWN2 0x04C6
#define B_WOODS_TRUNK  0x0416 // and 0x0417 beside it
#define B_WOODS_TOP    0x040E // the top of the woods, and 0x040F beside it
#define B_WOODS_ON_ROUND 0x05F4 // the woods' trunks on an ordinary tree's crown, and 0x05F5 (Route 104)
#define B_WOODS_SHADE  0x300E // grass in the shade of woods to the south, and 0x300F
// A rock in the sea, 2x2.
static const u16 sSeaRock[4] = { 0x0550, 0x0551, 0x0558, 0x0559 };
static const u16 sSeaRock2[4] = { 0x0575, 0x0576, 0x057D, 0x057E };

// A flower bed inside a wooden fence, like the ones on Route 117. 6x4 blocks.
#define F_TL 0x0538
#define F_H  0x0549
#define F_TR 0x053A
#define F_L  0x0540
#define F_R  0x0542
#define F_BL 0x0548
#define F_BR 0x054A
#define F_IN 0x0404
static const u16 sFenceGarden[6 * 4] = {
    F_TL, F_H,  F_H,  F_H,  F_H,  F_TR,
    F_L,  F_IN, F_IN, F_IN, F_IN, F_R,
    F_L,  F_IN, F_IN, F_IN, F_IN, F_R,
    F_BL, F_H,  F_H,  F_H,  F_H,  F_BR,
};
// A short fence along a path edge, 4 blocks.
static const u16 sFenceRow[4] = { F_H, F_H, F_H, F_H };

// The Pokemon League front at the summit: 11x7 blocks, door at (5, 5).
#define LEAGUE_UW 6
#define LEAGUE_UH 3
#define LEAGUE_DOOR_X 5
#define LEAGUE_DOOR_Y 5

#define DOOR_LOCKED 0xFF
#define DOOR_LAB    0xFE
#define DOOR_DUNGEON 0xFD // the way into the town's tower or hideout (RWARP_DUNGEON)
#define DOOR_SIGNATURE 0xFC // the town's signature landmark (RINT_SIGNATURE)
#define DOOR_SIGLOT    0xFB // a signature lot laid like a building along a street (a market, a pasture): no door
// A town whose landmark has not fitted by then is built without it; until
// then, it is tried past the 60 tries other maps get (and after, 30 more
// without it). A town with an exit toward another tileset keeps the view
// from there clear of its buildings (InSeamView), which leaves less room:
// it gets more tries (SigGiveUp).
#define SIG_GIVE_UP 86
#define SEA_LONE_LANDMARK 70 // v1.1 sea (GenTown)
#define SIG_GIVE_UP_SEAM 130
// After this many tries a town's buildings may stand in view from a route
// of another tileset (gen->seamLots): late, so the other ways of making
// room (a smaller tower, a door in the rock) come first, and in a town with
// a landmark only once the landmark has been given up.
#ifndef SEAM_LOTS_TRY
#define SEAM_LOTS_TRY 58
#endif
#ifndef SEAM_LOTS_SIG_TRY
#define SEAM_LOTS_SIG_TRY (SIG_GIVE_UP_SEAM + 15)
#endif
static bool8 SigTownTries(const struct RogueRegion *r, u8 place);
static u32 SigGiveUp(const struct RogueRegion *r, u8 place);
static bool8 SigEnterable(u8 sig);

struct Building
{
    const u16 *stamp;
    u8 uw, uh;   // footprint in units
    u8 sw, sh;   // stamp size in blocks
    u8 sy;       // stamp y offset inside the footprint, in blocks
    u8 doorX, doorY; // door position inside the stamp
    u8 door;     // enum RogueWarpKind, DOOR_LOCKED or DOOR_LAB
    u8 signX;    // x of a name plate on the door row, relative to the stamp, or 0
    u8 frontY;   // row where the player walks up to the door, relative to the
                 // stamp; 0: the row below the door (raised doors differ)
};

// The row in front of a building's door, relative to its stamp.
static int FrontY(const struct Building *b)
{
    return b->frontY ? b->frontY : b->doorY + 1;
}

static const struct Building sPokemonCenter = { sStamp_PokemonCenter, 2, 2, 4, 4, 0, 1, 3, RWARP_PC, 0 };
static const struct Building sMart          = { sStamp_Mart,          2, 2, 4, 4, 0, 1, 3, RWARP_MART, 0 };
static const struct Building sGym           = { sStamp_Gym,           3, 3, 6, 5, 1, 3, 4, RWARP_GYM, 0 };
static const struct Building sLab           = { sStamp_Lab,           4, 3, 7, 5, 1, 4, 4, DOOR_LAB, 7 };
// Three house styles for the meadow theme.
static const struct Building sHouses[] =
{
    { sStamp_BigHouse, 3, 3, 5, 5, 1, 3, 4, RWARP_HOUSE, 5 },
    { sStamp_House2,   2, 2, 4, 4, 0, 1, 3, RWARP_HOUSE, 0 },
    { sStamp_House,    2, 2, 4, 4, 0, 1, 3, RWARP_HOUSE, 0 },
};

struct DecorDef
{
    const u16 *blocks;
    u8 w, h; // in blocks
};

// What covers a town's open ground, and what its trees look like.
enum TownGround
{
    TGROUND_GRASS,
    TGROUND_SAND, // Dewford: sand everywhere and island trees
    TGROUND_ASH,  // Fallarbor: ash dirt and olive ash trees
};

struct TownTheme
{
    const struct Building *houses;
    u8 nHouses;
    const struct Building *gym; // NULL: the plain gym
    const struct Building *big; // landmark buildings, always locked
    u8 nBig;
    const struct DecorDef *decor;
    u8 nDecor;
    u16 roadTL, roadCentre;     // 3x3 road set, 0: sand or worn grass
    u16 fence[3];               // left end, middle, right end; 0: none
    const struct Building *pc, *mart; // with corners on the theme's ground, or NULL
    u8 ground;                  // enum TownGround
};

#include "data/rogue_themes.h"
#include "data/rogue_spring_rock.h"
#include "data/rogue_cave_hill.h"
#include "data/rogue_volcano_hill.h"
#include "data/rogue_ground_variants.h"
#include "data/rogue_tower_entrance.h"

struct Rect
{
    s8 x, y, w, h;
};

struct Gen
{
    const struct RogueRegion *r;
    const struct RoguePlace *p;
    struct RogueMap *m;
    u8 cw, ch;
    u8 exitPos[4];      // block coordinate of each opening along its side, 0xFF if none
    u8 g[MAX_CELLS];
    u8 cover[ROGUE_MAX_BLOCKS];
    u32 rng;
    u16 path[MAX_PATH]; // road units in walking order (y * cw + x)
    u16 nPath;
    u16 walk[MAX_WALK]; // every step of a carved road, repeats included
    u16 nWalk;
    u8 occupied[ROGUE_MAX_BLOCKS / 8 + 1]; // blocks taken by objects or signs
    u16 taken[96];                         // the same blocks as a list (packed), for IsCrowded
    u8 nTaken;
    u8 onWalk[ROGUE_MAX_BLOCKS / 8 + 1];   // blocks on the shortest walk
    u8 free[ROGUE_MAX_BLOCKS / 8 + 1];     // standable and not occupied, while placing trainers
    u8 cutChecks;                          // CutsRoute calls left for the current trainer
    u16 tallCount;                         // blocks of tall grass
    bool8 hasMesa;
    struct Rect mesa; // in units
    u8 caveHill;            // the hill RenderMesa draws (sCaveHills index + 1, 0x80 | sCaveHillTwins index + 1,
                            // or 0x40 | sVolcanoHills index + 1), or 0
    u8 mouthX;        // block x of the cave mouth
    bool8 hasBerry;
    u8 berryX, berryY; // top left block of the berry patch
    u8 caveMatX, caveMatY, caveDoorX; // blocks of the cave exits
    struct Decor
    {
        const u16 *blocks;
        u8 x, y, w, h; // in blocks
    } decor[MAX_DECOR];
    u8 nDecor;
    u8 pocketX[MAX_POCKETS], pocketY[MAX_POCKETS]; // dead-end units off the road
    u8 nPockets;
    u16 roadTL, roadCentre; // V_ROAD tiles
    u8 keepOut[4];          // edges whose connection view must not show theme tiles
    bool8 seamLots;         // buildings may stand in that view (a town that can't fit them otherwise)
    struct { u8 x, y, len; } fences[8]; // fence lines, in blocks
    u8 nFences;
    u16 fence[3];
    u8 attempt;             // how many times this map has been tried
    u8 crowdRadius;         // IsCrowded distance; 0 means the usual 3
    u8 leagueX;             // summit: unit x of the League building
    u8 walkSX, walkSY, walkEX, walkEY; // ends of the shortest walk, in blocks
    u8 endX, endY;          // dead ends: the clearing at the far end, in units
    u8 groundKind;          // enum GROUND_*: what V_GROUND draws
    u8 noLawn;              // the route's buildings stand in its ash, with no lawn (the glass workshop)
    u8 townGround;          // enum TownGround: a town's ground and trees
    bool8 longGrass;        // encounter grass is long grass
    bool8 woods;            // trees are drawn as deep woods
    bool8 rocky;            // solid ground is drawn as rock (hills, deserts)
    u8 rockTreeChance;      // hills: how many solid units stay trees, in percent
    const u16 *rockFoot;    // rock edges against the ground: grass or sand
    u8 seaDepth[MAX_CW];    // coast: how many units the sea reaches in, along its side
    u8 beach[MAX_CELLS / 8 + 1]; // coast: units of beach, painted as sand at the end
    u8 guardStep;           // path index of the obstacle unit, or 0
    u8 guardEnd;            // last path index of the narrow way in past the obstacle
    bool8 ashFade;          // ash covers only part of this route (ashZone)
    bool8 ashGrassPlanned;  // routes: gen->free marks the tall grass drawn as ash grass (PlanAshGrass)
    union
    {
        u8 ashZone[MAX_CELLS / 8 + 1]; // routes: units where the ash lies
        u8 upper[MAX_CELLS / 8 + 1];   // towns: units on the raised terrace
    };
    u8 ashTree[MAX_CELLS / 8 + 1]; // trees drawn as ash trees
    u8 keep[MAX_CELLS / 8 + 1];    // routes: tree units that open ground may not take
    u8 river[MAX_CELLS / 8 + 1];   // routes: units of a river the trail may wade across
    u8 layout;              // routes: which of the kind's layouts this one uses
    u8 placing;             // the object being placed among the trainers
    u16 walkFrom;           // the first step of the walk a trainer may watch (Victory Road: past the rival)
    bool8 wetSpot;          // the trainer being placed wants water in sight
    bool8 wetMiss;          // a water trainer ended up away from the water
    bool8 terraced;         // a town with a raised terrace (upper)
    u8 nStairs;
    u8 stairX[4], stairY[4]; // units where stone steps lead down the terrace's rim
    bool8 forestTown;       // a town in deep woods: trees drawn as the woods' small trees
    u8 springRock;          // a spring town's mesa (RenderSpringRock): sSpringRocks index + 1, or 0
    u8 springX, springY;    // its top left block
    bool8 islandTrees;      // a beach town: its own trees are Dewford's island trees
    bool8 islandTown;       // a beach town all on sand, as Dewford (not a harbor city on grass)
    bool8 zoned;            // grassZone is filled in
    union
    {
        u8 grassZone[MAX_CELLS / 8 + 1]; // island towns: units of grass, with ordinary trees
        u8 bridgeU[MAX_CELLS / 8 + 1];   // routes: units of a log bridge (rogue_gen_water.h)
    };
    bool8 logBridge;        // routes: a log bridge was laid (bridgeU), kept if it is dropped
    bool8 bites;            // routes and caves: water corners are rounded off (BittenF)
    bool8 inView;           // what is being placed may stand in view of the next map (a sign)
    // A route with a way into a forest (rogue_gen_forest.h): the tree unit
    // of each way in and the side of it the ground lies on. When the road
    // goes through the forest, the woods cut the route in two between the
    // units inside the way in (A) and the way on (B).
    u8 nForestWays;
    u8 forestX[2], forestY[2], forestFace[2];
    bool8 forestCut;
    u8 cutAX, cutAY, cutBX, cutBY;
    // Towns: the buildings placed so far (MAX_LOTS of them, unused ones
    // zero), for LotRingClear.
    const struct Placed *lots;
};

static ROGUE_EWRAM s16 sBfsPrev[MAX_CELLS] = {0};
// A queue for searches; at least MAX_CELLS long, and a power of two so the
// ring buffer needs no division.
#define BFS_RING 1024
static ROGUE_EWRAM u16 sBfsQueue[BFS_RING] = {0};
static ROGUE_EWRAM struct Gen sGen = {0};
#ifdef ROGUE_HOST
// The host build's record of whether GenTown built the town's signature
// landmark (gen_host prints it; sigtest.py checks it).
static u8 gHostLandmark;
#define HOST_LANDMARK(v) (gHostLandmark = (v))
// And whether GenRoute laid its feature's lot (featuretest.py checks it).
static u8 gHostFeatLot;
#define HOST_FEATLOT(v) (gHostFeatLot = (v))
#else
#define HOST_LANDMARK(v)
#define HOST_FEATLOT(v)
#endif
// The dungeon floor being generated (rogue_gen_dungeon.h), or floor 0 for a
// place's own map.
static ROGUE_EWRAM u8 sGenFloor = 0;
static ROGUE_EWRAM u8 sGenDungeon = 0;

static u8 BareCover(struct Gen *gen, int fx, int fy);
// Forests (rogue_gen_forest.h).
static bool8 ForestWoodsU(struct Gen *gen, int ux, int uy);
static void ForestWayGround(struct Gen *gen, int k, int *ux, int *uy);
// Mountain passes and canyons (rogue_gen_rock.h).
static void PlanPass(struct Gen *gen, int ax, int ay);
static void PassClearRims(struct Gen *gen);
static void PassRockLevels(struct Gen *gen, u8 *level);
static void PassRenderRock(struct Gen *gen, const u8 *level);
static void RenderPassRims(struct Gen *gen, const u8 *level);
// High ground on a mountain pass (rogue_gen_vert.h).
static bool8 PlateauU(struct Gen *gen, int x, int y);
static bool8 ShelfRoute(struct Gen *gen);
static u16 RimFoot(struct Gen *gen, u8 k, int fx, int fy);
static void PlanShelf(struct Gen *gen, int ax, int ay);
static bool8 FallsRoute(struct Gen *gen);
static void PlanFalls(struct Gen *gen, int ax, int ay);
static void FallsFinish(struct Gen *gen);
static void RenderFalls(struct Gen *gen);
static void PlanHillFalls(struct Gen *gen, int ax, int ay);
static void HillFallsFinish(struct Gen *gen);
static void RenderHillFalls(struct Gen *gen);
static bool8 PlateauSkirtU(struct Gen *gen, int x, int y);
static bool8 PlateauRimBlock(struct Gen *gen, int fx, int fy);
static void RenderPlateauUnit(struct Gen *gen, const u8 *level, int ux, int uy);
static bool8 PlanPlateau(struct Gen *gen, int ax, int ay);
static bool8 PlateauCarve(struct Gen *gen, int ax, int ay, int bx, int by, int top);
static void PlateauFinish(struct Gen *gen);
static void RockRepair(struct Gen *gen);
static void PlaceCanyonBoulders(struct Gen *gen, int ax, int ay);
static bool8 IsCanyon(struct Gen *gen);
static bool8 RockDecorU(struct Gen *gen, int x, int y);
static void CanyonThickWalls(struct Gen *gen, int ax, int ay);
static void PlaceAshOutcrops(struct Gen *gen, int ax, int ay);
// (v1.1 peaks) The desert tower on its route (rogue_gen_peaks.h).
static s32 MirageOf(struct Gen *gen);
static bool8 PlaceMirageTower(struct Gen *gen);
static void MirageGround(struct Gen *gen, bool8 painted);
static bool8 NearTrainerLine(struct Gen *gen, int x, int y);
static void CanyonClearGrass(struct Gen *gen);

// The place being generated, as a number for hashes. A dungeon floor gets
// its own: its made-up place is not in the region, so its address would hash
// differently on the host and the GBA.
static u32 GenPlaceId(struct Gen *gen)
{
    return sGenFloor ? 64 + sGenDungeon * ROGUE_MAX_FLOORS + sGenFloor : gen->m->place;
}

// ---------------------------------------------------------------------------
// RNG

u32 RogueRng_Next(u32 *state)
{
    u32 x;
    // Nearly every step of the generator draws numbers, so this is where it
    // offers to hand the frame back most often.
    YIELD();
    x = *state;
    if (x == 0)
        x = 0x9E3779B9;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

// A number below n. Scales the top 16 bits instead of taking a remainder,
// since the GBA has no divide instruction. n must be at most 65536.
u32 RogueRng_Range(u32 *state, u32 n)
{
    if (n == 0)
        return 0;
    return ((RogueRng_Next(state) >> 16) * n) >> 16;
}

u32 RogueRng_Hash(u32 seed, u32 salt)
{
    u32 h = seed ^ (salt * 0x9E3779B1);
    h ^= h >> 16;
    h *= 0x85EBCA6B;
    h ^= h >> 13;
    h *= 0xC2B2AE35;
    h ^= h >> 16;
    return h ? h : 1;
}

// ---------------------------------------------------------------------------
// Region names

// A start that ends in a vowel, an optional middle syllable, and an end that
// starts with a consonant, so the joins always read well and no name can be
// split two ways.
static const char *const sRegionStart[32] = {
    "KA", "JO", "HO", "SI", "PA", "GA", "A", "U", "O", "E", "TA", "RO", "MA", "VA", "SO", "LU",
    "NA", "DO", "ZE", "BRA", "KE", "MI", "TO", "SE", "FA", "CA", "LA", "VE", "NO", "RI", "CO", "DRA",
};
static const char *const sRegionMiddle[16] = {
    "", "LO", "RA", "NI", "VA", "DE", "MO", "LI", "RU", "SA", "TE", "NO", "RI", "LA", "DO", "VE",
};
static const char *const sRegionEnd[32] = {
    "NTO", "LOS", "LA", "NOVA", "LAR", "DEA", "RRE", "NIA", "VIA", "SEI", "REN", "DOR", "RIA", "NOR", "MA", "LIS",
    "ROS", "TIA", "STA", "NDO", "RA", "NE", "LO", "VAN", "THA", "KAI", "NOA", "SSA", "LEN", "DIA", "RIM", "GAR",
};
// Seeds are scrambled before they are split into syllables, so neighbouring
// seeds get unlike names. The two numbers are inverses mod 2^14.
#define REGION_SCRAMBLE   0x2C9B
#define REGION_UNSCRAMBLE 0x1993

static char *AppendAscii(char *dest, const char *src)
{
    while (*src)
        *dest++ = *src++;
    *dest = 0;
    return dest;
}

bool8 RogueGen_IsRun0(const struct RogueRegion *r)
{
    return r->seed == ROGUE_RUN0_SEED;
}

void RogueGen_RegionName(u32 seed, char *out)
{
    u32 k;
    if (seed == ROGUE_RUN0_SEED)
    {
        AppendAscii(out, ROGUE_RUN0_NAME);
        return;
    }
    k = ((seed % ROGUE_REGION_COUNT) * REGION_SCRAMBLE) & (ROGUE_REGION_COUNT - 1);
    out = AppendAscii(out, sRegionStart[k >> 9]);
    out = AppendAscii(out, sRegionMiddle[(k >> 5) & 15]);
    AppendAscii(out, sRegionEnd[k & 31]);
}

static bool8 SameAscii(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
    {
        char ca = (*a >= 'a' && *a <= 'z') ? *a - 'a' + 'A' : *a;
        if (ca != *b)
            return FALSE;
    }
    return *a == *b;
}

s32 RogueGen_ParseRegionName(const char *name)
{
    u32 seed;
    char buf[ROGUE_REGION_NAME_LEN + 1];
    for (seed = 0; seed < ROGUE_REGION_COUNT; seed++)
    {
        RogueGen_RegionName(seed, buf);
        if (SameAscii(name, buf))
            return seed;
    }
    return -1;
}

static bool8 HasAscii(const char *s, const char *word)
{
    for (; *s; s++)
    {
        const char *a = s, *b = word;
        while (*b && *a == *b)
            a++, b++;
        if (*b == 0)
            return TRUE;
    }
    return FALSE;
}

bool8 RogueGen_IsAwkwardRegionName(u32 seed)
{
    static const char *const sRude[] = { "ANAL", "ANUS", "RAPE", "NIG", "FAG", "SEX", "CUM", "TIT", "POO",
                                         "DIE", "NAZI", "SLUT", "SHIT", "DICK", "COCK", "PENIS", "VAGIN", "DAMN" };
    u32 k = ((seed % ROGUE_REGION_COUNT) * REGION_SCRAMBLE) & (ROGUE_REGION_COUNT - 1);
    const char *start = sRegionStart[k >> 9], *middle = sRegionMiddle[(k >> 5) & 15], *end = sRegionEnd[k & 31];
    char buf[ROGUE_REGION_NAME_LEN + 1];
    u32 i, len = 0;

    RogueGen_RegionName(seed, buf);
    while (buf[len])
        len++;
    if (len < 4)
        return TRUE;
    // DODONOR, LALAR, VEVEKAI.
    if (SameAscii(middle, start) || (middle[0] && middle[0] == end[0] && middle[1] == end[1]))
        return TRUE;
    for (i = 0; i < sizeof(sRude) / sizeof(sRude[0]); i++)
        if (HasAscii(buf, sRude[i]))
            return TRUE;
    return FALSE;
}

#define RAND(n) RogueRng_Range(&gen->rng, (n))
// Searches keep coordinates packed as y << 8 | x: the GBA has no divide
// instruction, so turning an index back into x and y is slow.
#define PACK(x, y) (((y) << 8) | (x))
#define PX(v) ((v) & 0xFF)
#define PY(v) ((v) >> 8)
#define CHANCE(pct) (RAND(100) < (pct))

// ---------------------------------------------------------------------------
// Coarse grid helpers

static const s8 sDirDX[4] = { 0, 1, 0, -1 };
static const s8 sDirDY[4] = { -1, 0, 1, 0 };

static u8 Opposite(u8 dir)
{
    return (dir + 2) & 3;
}

static const struct RogueLink *LinkOnSide(const struct RoguePlace *p, u8 dir);
static bool8 HasSide(const struct RoguePlace *p, u8 dir);
struct Gen;
static bool8 IsBeachU(struct Gen *gen, int x, int y);
static u16 OnCityGrass(struct Gen *gen, u16 block, int fx, int fy);

static int Abs(int v)
{
    return v < 0 ? -v : v;
}

static bool8 InBounds(struct Gen *gen, int x, int y)
{
    return x >= 0 && y >= 0 && x < gen->cw && y < gen->ch;
}

// Most loops over a map read it here, so this yields too (see YIELD).
static u8 Cell(struct Gen *gen, int x, int y)
{
    YIELD();
    if (!InBounds(gen, x, y))
        return C_TREE;
    return gen->g[y * gen->cw + x];
}

static void SetCell(struct Gen *gen, int x, int y, u8 type)
{
    if (InBounds(gen, x, y))
        gen->g[y * gen->cw + x] = type;
}

static bool8 IsEdge(struct Gen *gen, int x, int y)
{
    return x <= 0 || y <= 0 || x >= gen->cw - 1 || y >= gen->ch - 1;
}

// A bit per unit, in one of the maps' bitmaps.
static bool8 BitU(const u8 *bits, struct Gen *gen, int x, int y)
{
    int i;
    if (!InBounds(gen, x, y))
        return FALSE;
    i = y * gen->cw + x;
    return (bits[i >> 3] >> (i & 7)) & 1;
}

static void SetBitU(u8 *bits, struct Gen *gen, int x, int y)
{
    int i;
    if (!InBounds(gen, x, y))
        return;
    i = y * gen->cw + x;
    bits[i >> 3] |= 1 << (i & 7);
}

// Units the player can stand in.
static bool8 IsOpenU(u8 c)
{
    return c == C_OPEN || c == C_ROAD || c == C_SHORE || c == C_LEDGE || c == C_KEEP || c == C_FORD || c == C_CLIFF;
}

// Water is laid in whole units, so its outline steps two blocks at a time.
// On routes and in caves the outer corner block of a water unit that sticks
// out into the land is land instead, which turns each step into a diagonal,
// as on the shores of Routes 103, 120 and Granite Cave. Only where the
// water stays at least two blocks thick, never on the map edge (the seams)
// and never where the corner would join two stretches of ground that were
// apart. wet is C_SEA on routes and C_WATER in caves.
static bool8 BittenF(struct Gen *gen, int fx, int fy, u8 wet)
{
    int ux = fx >> 1, uy = fy >> 1, dx = (fx & 1) ? 1 : -1, dy = (fy & 1) ? 1 : -1;
    u8 a, b, c;
    if (!gen->bites || fx < 0 || fy < 0 || fx >= gen->m->w || fy >= gen->m->h
     || IsEdge(gen, ux, uy) || Cell(gen, ux, uy) != wet)
        return FALSE;
    a = Cell(gen, ux + dx, uy);
    b = Cell(gen, ux, uy + dy);
    c = Cell(gen, ux + dx, uy + dy);
    if (a == wet || b == wet || c == wet || a == C_FORD || b == C_FORD || c == C_FORD
     || Cell(gen, ux - dx, uy) != wet || Cell(gen, ux, uy - dy) != wet)
        return FALSE;
    // Beside open ground only, and no new way between two open units that
    // only met at a corner.
    if (!IsOpenU(a) && !IsOpenU(b))
        return FALSE;
    if (IsOpenU(a) && IsOpenU(b) && !IsOpenU(c))
        return FALSE;
    return TRUE;
}

// A town's raised terrace: the units on it, and the stone steps in its rim.
// Outside the map a unit counts as being on the level of the nearest one.
static bool8 UpperU(struct Gen *gen, int x, int y)
{
    int i;
    if (!gen->terraced)
        return FALSE;
    x = x < 0 ? 0 : (x >= gen->cw ? gen->cw - 1 : x);
    y = y < 0 ? 0 : (y >= gen->ch ? gen->ch - 1 : y);
    i = y * gen->cw + x;
    return (gen->upper[i >> 3] >> (i & 7)) & 1;
}

// A terraced place's level at unit (x, y): a town's terrace is one step;
// a mountain pass (rogue_gen_rock.h) has a second, in gen->grassZone.
static u8 TerraceLevelU(struct Gen *gen, int x, int y)
{
    int i;
    if (!gen->terraced)
        return 0;
    if (gen->p->kind != RPLACE_ROUTE)
        return UpperU(gen, x, y);
    x = x < 0 ? 0 : (x >= gen->cw ? gen->cw - 1 : x);
    y = y < 0 ? 0 : (y >= gen->ch ? gen->ch - 1 : y);
    i = y * gen->cw + x;
    return UpperU(gen, x, y) + ((gen->grassZone[i >> 3] >> (i & 7)) & 1);
}

static bool8 IsStairU(struct Gen *gen, int x, int y)
{
    int i;
    for (i = 0; i < gen->nStairs; i++)
        if (gen->stairX[i] == x && gen->stairY[i] == y)
            return TRUE;
    return FALSE;
}

// Can the player step from unit a to the next unit b, in direction d? Only
// down or up the steps between the terrace and the ground below it.
static bool8 TerraceStep(struct Gen *gen, int ax, int ay, int bx, int by, u8 d)
{
    u8 ua = TerraceLevelU(gen, ax, ay), ub = TerraceLevelU(gen, bx, by);
    if (ua == ub)
        return TRUE;
    if (d == RDIR_SOUTH)
        return ua > ub && IsStairU(gen, ax, ay);
    if (d == RDIR_NORTH)
        return ub > ua && IsStairU(gen, bx, by);
    return FALSE;
}

// Is the rim of the terrace just above unit (x, y)? A tree there would lose
// the tips of its crown under the rock.
static bool8 RimAbove(struct Gen *gen, int x, int y)
{
    return gen->terraced && TerraceLevelU(gen, x, y - 1) > TerraceLevelU(gen, x, y);
}

static bool8 HasNeighbor(struct Gen *gen, int x, int y, u8 type, bool8 diagonal)
{
    int dx, dy;
    for (dy = -1; dy <= 1; dy++)
    {
        for (dx = -1; dx <= 1; dx++)
        {
            if ((dx == 0 && dy == 0) || (!diagonal && dx != 0 && dy != 0))
                continue;
            if (InBounds(gen, x + dx, y + dy) && Cell(gen, x + dx, y + dy) == type)
                return TRUE;
        }
    }
    return FALSE;
}

static void AddRoadCell(struct Gen *gen, int x, int y)
{
    u8 c = Cell(gen, x, y);
    if (!InBounds(gen, x, y))
        return;
    if (c != C_ROAD && gen->nPath < MAX_PATH)
        gen->path[gen->nPath++] = y * gen->cw + x;
    if (gen->nWalk < MAX_WALK)
        gen->walk[gen->nWalk++] = y * gen->cw + x;
    SetCell(gen, x, y, C_ROAD);
}

// Rebuild a route's road from its carved walk with the loops taken out: when
// the walk comes back to a unit it already passed, the steps in between were a
// detour out and back. Those units stay open ground but are not road, so the
// trail never leads into a dead end. first and last are the edge openings.
static void LoopEraseRoad(struct Gen *gen, u16 first, int last)
{
    int i, j, n = 0;
    for (i = 0; i < gen->nPath; i++)
    {
        YIELD();
        SetCell(gen, gen->path[i] % gen->cw, gen->path[i] / gen->cw, C_OPEN);
    }
    gen->path[n++] = first;
    for (i = 0; i < gen->nWalk; i++)
    {
        YIELD();
        u16 c = gen->walk[i];
        for (j = 1; j < n; j++)
            if (gen->path[j] == c)
                break;
        if (j < n)
            n = j + 1;
        else if (n < MAX_PATH - 1)
            gen->path[n++] = c;
    }
    if (last >= 0)
        gen->path[n++] = last;
    gen->nPath = n;
    for (i = 0; i < n; i++)
    {
        YIELD();
        SetCell(gen, gen->path[i] % gen->cw, gen->path[i] / gen->cw, C_ROAD);
    }
}

// Carve an L-shaped road between two cells.
static void CarveRoad(struct Gen *gen, int x0, int y0, int x1, int y1)
{
    bool8 horizontalFirst = RAND(2);
    int x = x0, y = y0;

    AddRoadCell(gen, x, y);
    if (horizontalFirst)
    {
        while (x != x1) { x += (x1 > x) ? 1 : -1; AddRoadCell(gen, x, y); }
        while (y != y1) { y += (y1 > y) ? 1 : -1; AddRoadCell(gen, x, y); }
    }
    else
    {
        while (y != y1) { y += (y1 > y) ? 1 : -1; AddRoadCell(gen, x, y); }
        while (x != x1) { x += (x1 > x) ? 1 : -1; AddRoadCell(gen, x, y); }
    }
}

static void Fill(struct Gen *gen, u8 interior)
{
    int x, y;
    for (y = 0; y < gen->ch; y++)
    {
        YIELD();
        for (x = 0; x < gen->cw; x++)
            SetCell(gen, x, y, IsEdge(gen, x, y) ? C_TREE : interior);
    }
}

// Cells of an opening on a map edge. pos is a coarse coordinate along the edge.
static void EdgeCell(struct Gen *gen, u8 dir, int pos, int inset, int *x, int *y)
{
    switch (dir)
    {
    case RDIR_NORTH: *x = pos; *y = inset; break;
    case RDIR_SOUTH: *x = pos; *y = gen->ch - 1 - inset; break;
    case RDIR_WEST:  *x = inset; *y = pos; break;
    default:         *x = gen->cw - 1 - inset; *y = pos; break;
    }
}

static int EdgeLength(struct Gen *gen, u8 dir)
{
    return (dir == RDIR_NORTH || dir == RDIR_SOUTH) ? gen->cw : gen->ch;
}

// Open an edge where the plan puts it and return the first interior cell.
static void MakeOpening(struct Gen *gen, u8 dir, int *ix, int *iy)
{
    const struct RogueLink *l = LinkOnSide(gen->p, dir);
    int pos = l ? l->pos / 2 : EdgeLength(gen, dir) / 2;
    int ex, ey;

    gen->exitPos[dir] = pos * 2;
    EdgeCell(gen, dir, pos, 0, &ex, &ey);
    EdgeCell(gen, dir, pos, 1, ix, iy);
    AddRoadCell(gen, ex, ey);
}

// Flood fill over open units from (sx, sy) and return how many were reached.
// A ledge can only be crossed downward, so here it is a wall between its unit
// and the unit below. Afterwards Reached() tells whether a unit was reached.
static int FloodReachable(struct Gen *gen, int sx, int sy)
{
    int head = 0, tail = 0;

    memset(sBfsPrev, 0, gen->cw * gen->ch * sizeof(sBfsPrev[0]));
    sBfsQueue[tail++] = PACK(sx, sy);
    sBfsPrev[sy * gen->cw + sx] = 1;
    while (head < tail)
    {
        int cur = sBfsQueue[head++];
        int cx = PX(cur), cy = PY(cur);
        u8 here = gen->g[cy * gen->cw + cx];
        YIELD();
        u8 d;
        for (d = 0; d < 4; d++)
        {
            int nx = cx + sDirDX[d], ny = cy + sDirDY[d];
            int ni = ny * gen->cw + nx;
            u8 c;
            if (nx < 0 || ny < 0 || nx >= gen->cw || ny >= gen->ch || sBfsPrev[ni])
                continue;
            c = gen->g[ni];
            if (!IsOpenU(c))
                continue;
            if (d == RDIR_SOUTH && here == C_LEDGE)
                continue;
            if (d == RDIR_NORTH && c == C_LEDGE)
                continue;
            if (gen->terraced && !TerraceStep(gen, cx, cy, nx, ny, d))
                continue;
            sBfsPrev[ni] = 1;
            sBfsQueue[tail++] = PACK(nx, ny);
        }
        // A forest the road goes through joins the two halves of its route.
        if (gen->forestCut && gen->nForestWays == 2)
        {
            int k, gx, gy, ox, oy;
            for (k = 0; k < 2; k++)
            {
                ForestWayGround(gen, k, &gx, &gy);
                ForestWayGround(gen, !k, &ox, &oy);
                if (gx == cx && gy == cy && !sBfsPrev[oy * gen->cw + ox] && IsOpenU(gen->g[oy * gen->cw + ox]))
                {
                    sBfsPrev[oy * gen->cw + ox] = 1;
                    sBfsQueue[tail++] = PACK(ox, oy);
                }
            }
        }
    }
    return tail;
}

static bool8 Reached(struct Gen *gen, int x, int y)
{
    return InBounds(gen, x, y) && sBfsPrev[y * gen->cw + x] == 1;
}

// ---------------------------------------------------------------------------
// Block grid helpers

static bool8 InMap(struct Gen *gen, int fx, int fy)
{
    return fx >= 0 && fy >= 0 && fx < gen->m->w && fy < gen->m->h;
}

static u8 Cover(struct Gen *gen, int fx, int fy)
{
    return gen->cover[fy * gen->m->w + fx];
}

static void SetCover(struct Gen *gen, int fx, int fy, u8 cover)
{
    u8 *c = &gen->cover[fy * gen->m->w + fx];
    if (*c == V_TALL)
        gen->tallCount--;
    if (cover == V_TALL)
        gen->tallCount++;
    *c = cover;
}

static void MarkOccupied(struct Gen *gen, int fx, int fy)
{
    int i = fy * gen->m->w + fx;
    if (!((gen->occupied[i >> 3] >> (i & 7)) & 1) && gen->nTaken < sizeof(gen->taken) / sizeof(gen->taken[0]))
        gen->taken[gen->nTaken++] = PACK(fx, fy);
    gen->occupied[i >> 3] |= 1 << (i & 7);
    gen->free[i >> 3] &= ~(1 << (i & 7));
}

static bool8 IsOccupied(struct Gen *gen, int fx, int fy)
{
    int i = fy * gen->m->w + fx;
    return (gen->occupied[i >> 3] >> (i & 7)) & 1;
}

// TRUE on the block a ledge lands on and the block it is jumped from: a
// ledge runs along the bottom row of a C_LEDGE unit. Nobody may stand or
// lie there, or the player lands on them or cannot jump at all.
static bool8 WaterNoStandF(struct Gen *gen, int fx, int fy);
static bool8 WaterLineNearStep(struct Gen *gen, int x, int y, u8 mask, int n);

static bool8 ByLedgeF(struct Gen *gen, int fx, int fy)
{
    // (Nor on a log bridge or its land ends: rogue_gen_water.h.)
    if (WaterNoStandF(gen, fx, fy))
        return TRUE;
    if (fy >= 1 && ((fy - 1) & 1) && Cell(gen, fx >> 1, (fy - 1) >> 1) == C_LEDGE)
        return TRUE;
    return ((fy + 1) & 1) && Cell(gen, fx >> 1, (fy + 1) >> 1) == C_LEDGE;
}

// Plain open ground that can take ground cover.
static bool8 CanCover(struct Gen *gen, int fx, int fy)
{
    u8 c;
    if (!InMap(gen, fx, fy))
        return FALSE;
    c = Cell(gen, fx >> 1, fy >> 1);
    return (c == C_OPEN || c == C_ROAD) && !IsOccupied(gen, fx, fy);
}

static void AddObj(struct Gen *gen, u8 kind, int fx, int fy, u8 facing, u8 slot, u8 range)
{
    struct RogueMap *m = gen->m;
    struct RogueObj *o;
    if (m->nObjs >= ROGUE_MAX_OBJS)
        return;
    o = &m->objs[m->nObjs++];
    o->kind = kind;
    o->x = fx;
    o->y = fy;
    o->facing = facing;
    o->slot = slot;
    o->range = range;
    MarkOccupied(gen, fx, fy);
}

// In the reporter pair's towns (src/rogue_text_towns.h
// RogueText_ReporterRole), the camera operator stands beside the reporter,
// as TY stands by GABBY: on the other block of the reporter's spot along the
// road, else above or below it. Moves only that one person, with no draw
// from the map's random numbers.
static bool8 SeenFromNextDoor(struct Gen *gen, int fx, int fy);
static void PairReporters(struct Gen *gen)
{
    struct RogueMap *m = gen->m;
    struct RogueObj *rep = NULL, *cam = NULL;
    u32 i, nFolk = 0, turn = 0;
    int k;
    for (i = 0; i < m->nObjs; i++)
        nFolk += m->objs[i].kind == ROBJ_VILLAGER;
    for (i = 0; i < m->nObjs; i++)
    {
        u8 role;
        if (m->objs[i].kind != ROBJ_VILLAGER)
            continue;
        role = RogueText_ReporterRole(gen->r, m->place, nFolk, turn++);
        if (role == RTEXT_REPORTER)
            rep = &m->objs[i];
        else if (role == RTEXT_CAMERA)
            cam = &m->objs[i];
    }
    if (rep == NULL || cam == NULL)
        return;
    for (k = 0; k < 4; k++)
    {
        // Along the road first: the reporter faces it.
        bool8 horiz = (rep->facing == RDIR_NORTH || rep->facing == RDIR_SOUTH) == (k < 2);
        int x = rep->x + (horiz ? (k & 1 ? 1 : -1) : 0), y = rep->y + (horiz ? 0 : (k & 1 ? 1 : -1));
        if (!InMap(gen, x, y) || IsOccupied(gen, x, y) || Cell(gen, x >> 1, y >> 1) != C_OPEN || !Reached(gen, x >> 1, y >> 1)
         || IsEdge(gen, x >> 1, y >> 1) || ByLedgeF(gen, x, y) || SeenFromNextDoor(gen, x, y))
            continue;
        i = cam->y * m->w + cam->x;
        gen->occupied[i >> 3] &= ~(1 << (i & 7));
        if (Cover(gen, x, y) == V_FLOWER)
            SetCover(gen, x, y, BareCover(gen, x, y));
        cam->x = x;
        cam->y = y;
        cam->facing = rep->facing;
        MarkOccupied(gen, x, y);
        return;
    }
}

static void AddSign(struct Gen *gen, int fx, int fy, u8 kind)
{
    struct RogueMap *m = gen->m;
    if (m->nSigns >= ROGUE_MAX_SIGNS)
        return;
    m->signs[m->nSigns].x = fx;
    m->signs[m->nSigns].y = fy;
    m->signs[m->nSigns].kind = kind;
    m->nSigns++;
    MarkOccupied(gen, fx, fy);
}

static void AddWarp(struct Gen *gen, int fx, int fy, u8 kind)
{
    struct RogueMap *m = gen->m;
    if (m->nWarps >= ROGUE_MAX_WARPS)
        return;
    m->warps[m->nWarps].x = fx;
    m->warps[m->nWarps].y = fy;
    m->warps[m->nWarps].kind = kind;
    m->nWarps++;
}

// True if another object or sign already stands within three blocks.
static bool8 IsCrowded(struct Gen *gen, int fx, int fy)
{
    YIELD();
    int r = gen->crowdRadius ? gen->crowdRadius : 3, i;
    for (i = 0; i < gen->nTaken; i++)
    {
        int x = PX(gen->taken[i]), y = PY(gen->taken[i]);
        int dx = x - fx, dy = y - fy;
        if (dx >= -r && dx <= r && dy >= -r && dy <= r && IsOccupied(gen, x, y))
            return TRUE;
    }
    return FALSE;
}

// After this many tries at a map, people and things may stand in view of the
// next map again, so the map can always be made.
#define SEAM_VIEW_TRIES 20

static bool8 InGateLaneF(struct Gen *gen, int fx, int fy);
static int GateLaneStep(struct Gen *gen, const struct RogueLink *l);
static void LaneCell(struct Gen *gen, u8 d, int k, int *x, int *y);

// The game only shows the objects of the map the player is on, so anyone
// standing where the player sees them from across a connection would pop in
// on crossing. Hoenn keeps its people and items clear of that view, and so do
// we: TRUE for a block a player near an opening of the next map would see,
// about as deep as the camera shows past the seam (5 blocks up or down, 7
// across) and half a screen to each side of the opening, or right by any
// connected edge.
static bool8 SeenFromNextDoor(struct Gen *gen, int fx, int fy)
{
    YIELD();
    const struct RoguePlace *p = gen->p;
    int i;
    // (Nor ever in the lane behind an obstacle.)
    if (InGateLaneF(gen, fx, fy))
        return TRUE;
    if (gen->inView || gen->attempt >= SEAM_VIEW_TRIES)
        return FALSE;
    for (i = 0; i < p->nLinks; i++)
    {
        const struct RogueLink *l = &p->links[i];
        bool8 across = (l->dir == RDIR_WEST || l->dir == RDIR_EAST);
        int depth = l->dir == RDIR_NORTH ? fy : l->dir == RDIR_SOUTH ? gen->m->h - 1 - fy
                  : l->dir == RDIR_WEST ? fx : gen->m->w - 1 - fx;
        // Along the edge from the opening's first block.
        int along = (across ? fy : fx) - (l->pos & ~1);
        if (depth <= 2)
            return TRUE;
        if (across ? (depth <= 6 && along >= -5 && along <= 6) : (depth <= 4 && along >= -7 && along <= 8))
            return TRUE;
    }
    return FALSE;
}

// Find a free block in an open unit beside road unit (px, py). Returns the
// block that touches the road and the direction from it toward the road.
// Call FloodReachable first.
static bool8 FindSpotBesideRoad(struct Gen *gen, int px, int py, int *fx, int *fy, u8 *facing)
{
    u8 start = RAND(4), i;
    for (i = 0; i < 4; i++)
    {
        u8 d = (start + i) & 3;
        int nx = px + sDirDX[d], ny = py + sDirDY[d];
        int x, y;
        if (!InBounds(gen, nx, ny) || IsEdge(gen, nx, ny))
            continue;
        if (Cell(gen, nx, ny) != C_OPEN || !Reached(gen, nx, ny))
            continue;
        // Block on the side of the unit that faces the road.
        x = nx * 2 + (d == RDIR_WEST ? 1 : (d == RDIR_EAST ? 0 : RAND(2)));
        y = ny * 2 + (d == RDIR_NORTH ? 1 : (d == RDIR_SOUTH ? 0 : RAND(2)));
        if (IsOccupied(gen, x, y) || IsCrowded(gen, x, y) || ByLedgeF(gen, x, y)
         || SeenFromNextDoor(gen, x, y))
            continue;
        if (Cover(gen, x, y) == V_FLOWER)
            SetCover(gen, x, y, BareCover(gen, x, y));
        *fx = x;
        *fy = y;
        *facing = Opposite(d);
        return TRUE;
    }
    return FALSE;
}

static bool8 BlocksWay(struct Gen *gen, int x, int y);
static bool8 UnitWalkF(struct Gen *gen, int fx, int fy);

// TRUE on the bottom block of a unit above a tree: the tree's crown reaches
// up over it, and a sign there would be cleared away when the map is drawn.
static bool8 UnderCrown(struct Gen *gen, int fx, int fy)
{
    return (fy & 1) && fy + 1 < gen->m->h && Cell(gen, fx >> 1, (fy + 1) >> 1) == C_TREE;
}

static bool8 PathCoverF(struct Gen *gen, int fx, int fy)
{
    u8 cv;
    if (!InMap(gen, fx, fy))
        return FALSE;
    cv = Cover(gen, fx, fy);
    return cv == V_PATH || cv == V_SAND || cv == V_ROAD || cv == V_MAT;
}

// A sign stands beside the way, as in Hoenn: never on the trail, a road or
// a worn path, never where it would close a gap, and not right in front of
// a way out.
static bool8 FindSignSpot(struct Gen *gen, int px, int py, int *fx, int *fy, u8 *facing)
{
    int tries;
    for (tries = 0; tries < 8; tries++)
    {
        bool8 pn, ps, pw, pe, ok = TRUE, found;
        u8 d;
        // A sign is part of the map, not an object: it shows across a seam.
        gen->inView = TRUE;
        found = FindSpotBesideRoad(gen, px, py, fx, fy, facing);
        gen->inView = FALSE;
        if (!found)
            return FALSE;
        pn = PathCoverF(gen, *fx, *fy - 1);
        ps = PathCoverF(gen, *fx, *fy + 1);
        pw = PathCoverF(gen, *fx - 1, *fy);
        pe = PathCoverF(gen, *fx + 1, *fy);
        // On the path, in a notch of it, or between two stretches of it.
        if (PathCoverF(gen, *fx, *fy) || (pn && ps) || (pw && pe) || pn + ps + pw + pe >= 3
         || BlocksWay(gen, *fx, *fy) || UnderCrown(gen, *fx, *fy))
            continue;
        for (d = 0; d < 4 && ok; d++)
        {
            int edge = d == RDIR_NORTH ? *fy : d == RDIR_SOUTH ? gen->m->h - 1 - *fy
                     : d == RDIR_WEST ? *fx : gen->m->w - 1 - *fx;
            int along = (d == RDIR_NORTH || d == RDIR_SOUTH) ? *fx : *fy;
            if (gen->exitPos[d] != 0xFF && edge < 3 && Abs(along - gen->exitPos[d]) < 4)
                ok = FALSE;
        }
        if (ok)
            return TRUE;
    }
    return FALSE;
}

// A free block in a quiet corner: an open unit away from the road with trees
// or other walls on at least two sides. Call FloodReachable first.
static bool8 FindNook(struct Gen *gen, int *fx, int *fy)
{
    int tries;
    for (tries = 0; tries < 120; tries++)
    {
        int x = 1 + RAND(gen->cw - 2), y = 1 + RAND(gen->ch - 2);
        int walls = 0, bx, by;
        u8 d;
        if (Cell(gen, x, y) != C_OPEN || !Reached(gen, x, y) || HasNeighbor(gen, x, y, C_ROAD, FALSE))
            continue;
        for (d = 0; d < 4; d++)
            if (!IsOpenU(Cell(gen, x + sDirDX[d], y + sDirDY[d])))
                walls++;
        if (walls < 2)
            continue;
        bx = x * 2 + RAND(2);
        by = y * 2 + RAND(2);
        if (IsOccupied(gen, bx, by) || IsCrowded(gen, bx, by) || ByLedgeF(gen, bx, by) || SeenFromNextDoor(gen, bx, by))
            continue;
        SetCover(gen, bx, by, BareCover(gen, bx, by));
        *fx = bx;
        *fy = by;
        return TRUE;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// Ground cover

// Paint a 3x3 brush of cover around a block, skipping some corners so the
// edge comes out uneven.
static void Brush(struct Gen *gen, int fx, int fy, u8 cover, bool8 replace)
{
    int dx, dy;
    for (dy = -1; dy <= 1; dy++)
    {
        for (dx = -1; dx <= 1; dx++)
        {
            int x = fx + dx, y = fy + dy;
            if (dx != 0 && dy != 0 && CHANCE(35))
                continue;
            if (!CanCover(gen, x, y))
                continue;
            if (Cover(gen, x, y) != V_NONE && !replace)
                continue;
            SetCover(gen, x, y, cover);
        }
    }
}

// A wandering blob of cover, like the stepped patches of tall grass on a
// vanilla route.
static void PaintBlob(struct Gen *gen, int fx, int fy, int steps, u8 cover, bool8 replace)
{
    int i;
    for (i = 0; i < steps; i++)
    {
        Brush(gen, fx, fy, cover, replace);
        fx += (int)RAND(5) - 2;
        fy += (int)RAND(3) - 1;
        if (fx < 2) fx = 2;
        if (fy < 2) fy = 2;
        if (fx > gen->m->w - 3) fx = gen->m->w - 3;
        if (fy > gen->m->h - 3) fy = gen->m->h - 3;
    }
}

static void PaintUnit(struct Gen *gen, int ux, int uy, u8 cover)
{
    int dx, dy;
    for (dy = 0; dy < 2; dy++)
        for (dx = 0; dx < 2; dx++)
            if (CanCover(gen, ux * 2 + dx, uy * 2 + dy))
                SetCover(gen, ux * 2 + dx, uy * 2 + dy, cover);
}

// A road unit as a worn path: the unit itself plus a ragged fringe.
static void PaintWornUnit(struct Gen *gen, int ux, int uy)
{
    int i;
    PaintUnit(gen, ux, uy, V_PATH);
    for (i = 0; i < 4; i++)
    {
        // One of the four sides, both blocks of it, pushed out by one.
        int x0 = ux * 2 + (i == 1 ? 2 : (i == 3 ? -1 : 0));
        int y0 = uy * 2 + (i == 2 ? 2 : (i == 0 ? -1 : 0));
        int x1 = x0 + ((i == 0 || i == 2) ? 1 : 0);
        int y1 = y0 + ((i == 1 || i == 3) ? 1 : 0);
        if (!CHANCE(40))
            continue;
        if (CanCover(gen, x0, y0) && CanCover(gen, x1, y1) && Cover(gen, x0, y0) == V_NONE && Cover(gen, x1, y1) == V_NONE)
        {
            SetCover(gen, x0, y0, V_PATH);
            SetCover(gen, x1, y1, V_PATH);
        }
    }
}

static void ScatterFlowers(struct Gen *gen, int clusters)
{
    int i, tries;
    for (i = 0; i < clusters; i++)
    {
        for (tries = 0; tries < 20; tries++)
        {
            int x = 2 + RAND(gen->m->w - 4), y = 2 + RAND(gen->m->h - 4);
            if (!CanCover(gen, x, y) || Cover(gen, x, y) != V_NONE || Cell(gen, x >> 1, y >> 1) != C_OPEN)
                continue;
            SetCover(gen, x, y, V_FLOWER);
            if (CHANCE(70) && CanCover(gen, x + 1, y + 1) && Cover(gen, x + 1, y + 1) == V_NONE)
                SetCover(gen, x + 1, y + 1, V_FLOWER);
            if (CHANCE(50) && CanCover(gen, x - 1, y + 1) && Cover(gen, x - 1, y + 1) == V_NONE)
                SetCover(gen, x - 1, y + 1, V_FLOWER);
            if (CHANCE(30) && CanCover(gen, x + 2, y) && Cover(gen, x + 2, y) == V_NONE)
                SetCover(gen, x + 2, y, V_FLOWER);
            break;
        }
    }
}

static bool8 HasTreeBelow(struct Gen *gen, int ux, int uy);
static bool8 AddDecor(struct Gen *gen, const u16 *blocks, int fx, int fy, int w, int h);

static bool8 SameCover(struct Gen *gen, int fx, int fy, u8 cover)
{
    if (!InMap(gen, fx, fy))
        return TRUE;
    return Cover(gen, fx, fy) == cover;
}

static bool8 IsSeaF(struct Gen *gen, int fx, int fy);
static bool8 SeaFordBittenF(struct Gen *gen, int fx, int fy); // v1.1 sea (src/rogue_gen_sea.h)
static bool8 SeaSandBittenF(struct Gen *gen, int fx, int fy);
static bool8 IsGroundLike(struct Gen *gen, int fx, int fy);
static bool8 ThemedTreeU(struct Gen *gen, int ux, int uy);

// A tree standing alone inside a town, away from its edges.
static bool8 LoneTownTree(struct Gen *gen, int ux, int uy)
{
    if (gen->p->kind != RPLACE_TOWN || ux < 1 || uy < 1 || ux >= gen->cw - 1 || uy >= gen->ch - 1)
        return FALSE;
    return Cell(gen, ux, uy) == C_TREE && Cell(gen, ux - 1, uy) != C_TREE && Cell(gen, ux + 1, uy) != C_TREE
        && Cell(gen, ux, uy + 1) != C_TREE;
}

// Is a sign of this kind drawn on the map as a signpost (RenderSigns)?
static bool8 SignDrawn(u8 kind)
{
    return kind == RSIGN_TOWN || kind == RSIGN_ROUTE || kind == RSIGN_HOUSE || kind == RSIGN_LAB
        || (kind >= RSIGN_REST && kind < RSIGN_HIDDEN);
}

static bool8 IsPathCover(u8 c)
{
    return c == V_PATH || c == V_SAND || c == V_ROAD;
}

// Is cover drawn with the sand's own tiles? A sand road, a town road of
// sand (an island town's), and the sand of beaches and deserts.
static bool8 SandyCover(struct Gen *gen, u8 c)
{
    if (c == V_SAND)
        return TRUE;
    if (c == V_ROAD)
        return gen->roadTL == B_SAND_TL;
    if (c == V_GROUND)
        return gen->groundKind == GROUND_SAND && gen->townGround != TGROUND_ASH;
    return FALSE;
}

// Does the block show the same ground as cover, so no edge is drawn between
// them? Off the map the ground goes on.
static bool8 LooksSame(struct Gen *gen, int fx, int fy, u8 cover)
{
    u8 c;
    if (!InMap(gen, fx, fy))
        return TRUE;
    c = Cover(gen, fx, fy);
    if (c == cover)
        return TRUE;
    // Two kinds of path meet without a strip of grass between them (a
    // town's road running on into the sand by its exit).
    if (IsPathCover(cover) && (IsPathCover(c) || (c == V_GROUND && SandyCover(gen, c))))
        return TRUE;
    return cover != V_GROUND && SandyCover(gen, cover) && SandyCover(gen, c);
}

// Can a path of cover c take this block over from grass?
static bool8 PathCanTake(struct Gen *gen, int fx, int fy, u8 c)
{
    u8 v;
    if (!CanCover(gen, fx, fy))
        return FALSE;
    v = Cover(gen, fx, fy);
    return v == V_NONE || v == V_TALL || v == V_FLOWER || v == c;
}

// Can the bend take a path block here: open, not under a tree's canopy?
static bool8 BendCanTake(struct Gen *gen, int fx, int fy, u8 c)
{
    return InMap(gen, fx, fy) && PathCanTake(gen, fx, fy, c) && !((fy & 1) && HasTreeBelow(gen, fx >> 1, fy >> 1));
}

static bool8 PathAtF(struct Gen *gen, int fx, int fy, u8 c)
{
    return InMap(gen, fx, fy) && Cover(gen, fx, fy) == c;
}

// Make the two-block run at (fx, fy) and the block beside it on side -dx
// one block taller, up (dy -1) or down (dy 1), if both can take it.
static bool8 GrowBend(struct Gen *gen, int fx, int fy, int dx, int dy, u8 c)
{
    if (!PathAtF(gen, fx - dx, fy, c) || !BendCanTake(gen, fx, fy + dy, c) || !BendCanTake(gen, fx - dx, fy + dy, c))
        return FALSE;
    SetCover(gen, fx, fy + dy, c);
    SetCover(gen, fx - dx, fy + dy, c);
    return TRUE;
}

// The piece of path c joined to (fx, fy), not counting the way across to
// the block beside it at ox in this row: how many blocks, up to max (in
// list if given), or max + 1 if it is bigger or runs off the map (it goes
// on next door).
#define BEND_STUB_MAX 8
#define WAY_PATCH_MIN 12 // a pale path patch smaller than this is a scrap (DropPathScraps)
static int BendPiece(struct Gen *gen, int fx, int fy, int ox, u8 c, u16 *list, int max)
{
    int n = 1, i, d;
    list[0] = PACK(fx, fy);
    for (i = 0; i < n; i++)
    {
        for (d = 0; d < 4; d++)
        {
            int x = PX(list[i]) + (d == 1) - (d == 3), y = PY(list[i]) + (d == 2) - (d == 0), k;
            if (!InMap(gen, x, y))
                return max + 1;
            if (Cover(gen, x, y) != c || (i == 0 && y == fy && x == ox))
                continue;
            for (k = 0; k < n && list[k] != PACK(x, y); k++)
                ;
            if (k < n)
                continue;
            if (n == max)
                return max + 1;
            list[n++] = PACK(x, y);
        }
    }
    return n;
}

// No room to grow either leg (trees above and below): if one leg of a pale
// path is only a spur ending there (up to 2x4), it goes and the trail ends a
// step sooner, as long as the rest is at least as big (a 2x4 patch or more).
static bool8 TrimBendStub(struct Gen *gen, int x, int y, u8 c)
{
    u16 list[WAY_PATCH_MIN + 1];
    int n, k, side;
    // (Only a route's pale path, in soft patches: a sand road goes on, and
    // a town's streets lead to doors.)
    if (c != V_PATH || gen->p->kind != RPLACE_ROUTE)
        return FALSE;
    for (side = 0; side < 2; side++)
    {
        int rest = BendPiece(gen, x + 1 - side, y, x + side, c, list, WAY_PATCH_MIN);
        n = BendPiece(gen, x + side, y, x + 1 - side, c, list, BEND_STUB_MAX);
        if (n > BEND_STUB_MAX || rest < BEND_STUB_MAX || rest < n)
            continue;
        for (k = 0; k < n; k++)
            SetCover(gen, PX(list[k]), PY(list[k]), V_NONE);
        return TRUE;
    }
    return FALSE;
}

// Where a trail bends, its two legs side by side must overlap by at least
// two rows, so the corner is drawn with centre pieces as Oldale's is. Moving
// a path up round a tree (TidyPaths) can leave one leg ending in the row
// where the other begins: a top edge piece beside a bottom edge piece, a
// notch in the trail. Grow one leg a row toward the other.
static void MendPathBends(struct Gen *gen)
{
    int x, y, pass;
    bool8 changed = TRUE;
    for (pass = 0; pass < 3 && changed; pass++)
    {
        changed = FALSE;
        for (y = 0; y < gen->m->h; y++)
        {
            YIELD();
            for (x = 0; x + 1 < gen->m->w; x++)
            {
                u8 c = Cover(gen, x, y);
                bool8 aUp, aDown, bUp, bDown;
                if (!IsPathCover(c) || Cover(gen, x + 1, y) != c)
                    continue;
                aUp = PathAtF(gen, x, y - 1, c);
                aDown = PathAtF(gen, x, y + 1, c);
                bUp = PathAtF(gen, x + 1, y - 1, c);
                bDown = PathAtF(gen, x + 1, y + 1, c);
                // The left leg starts in this row and the right one ends
                // in it (or the other way round).
                if (!aUp && aDown && bUp && !bDown)
                    changed |= GrowBend(gen, x, y, 1, -1, c) || GrowBend(gen, x + 1, y, -1, 1, c) || TrimBendStub(gen, x, y, c);
                else if (aUp && !aDown && !bUp && bDown)
                    changed |= GrowBend(gen, x, y, 1, 1, c) || GrowBend(gen, x + 1, y, -1, -1, c) || TrimBendStub(gen, x, y, c);
            }
        }
    }
}

// Path and sand tiles only have edges on one side, so every path block must
// sit in a 2x2 square of the same cover. Drop the ones that do not, and the
// ones under a tree canopy.
//
// A tree's canopy reaches up into the lower row of the unit above it, so a
// path unit over a tree keeps only its upper row: a path one block tall, or
// a notch of grass in the trail. Instead the path there moves up a row, into
// the unit above, when that is open ground: the trail bends round the tree
// as Route 101's does, and stays two blocks wide.
static ROGUE_FAST void TidyPathBlocks(struct Gen *gen)
{
    u8 drop[ROGUE_MAX_BLOCKS / 8 + 1];
    int x, y, i;

    for (y = 1; y < gen->m->h; y++)
    {
        YIELD();
        for (x = 0; x < gen->m->w; x++)
        {
            u8 c = Cover(gen, x, y);
            if (!IsPathCover(c))
                continue;
            // Grass with the sea to the north is drawn as its bank's lip,
            // whatever covers it: a path there would end in a hard line.
            if (IsSeaF(gen, x, y - 1))
            {
                SetCover(gen, x, y, V_NONE);
                continue;
            }
            if (!(y & 1) || !HasTreeBelow(gen, x >> 1, y >> 1))
                continue;
            if (y >= 3 && Cover(gen, x, y - 1) == c && PathCanTake(gen, x, y - 2, c))
            {
                SetCover(gen, x, y, V_NONE);
                SetCover(gen, x, y - 2, c);
            }
            else if (LoneTownTree(gen, x >> 1, (y >> 1) + 1))
            {
                // A street can't bend round a lone tree in a town (a door
                // or a fence above): the tree gives way to the street.
                SetCell(gen, x >> 1, (y >> 1) + 1, C_OPEN);
            }
            else
            {
                SetCover(gen, x, y, V_NONE);
            }
        }
    }
    memset(drop, 0, sizeof(drop));
    for (y = 0; y < gen->m->h; y++)
    {
        YIELD();
        for (x = 0; x < gen->m->w; x++)
        {
            u8 c = Cover(gen, x, y);
            bool8 keep = FALSE;
            int sx, sy;
            if (!IsPathCover(c))
                continue;
            YIELD();
            for (sy = y - 1; sy <= y && !keep; sy++)
                for (sx = x - 1; sx <= x && !keep; sx++)
                    if (LooksSame(gen, sx, sy, c) && LooksSame(gen, sx + 1, sy, c)
                     && LooksSame(gen, sx, sy + 1, c) && LooksSame(gen, sx + 1, sy + 1, c))
                        keep = TRUE;
            if (!keep)
            {
                i = y * gen->m->w + x;
                drop[i >> 3] |= 1 << (i & 7);
            }
        }
    }
    for (i = 0; i < gen->m->w * gen->m->h; i++)
    {
        if (!(i & 31))
            YIELD();
        if (drop[i >> 3] & (1 << (i & 7)))
            gen->cover[i] = V_NONE;
    }
}

// (In two steps, so the bends are mended with TidyPathBlocks' list off the
// generator's small stack.)
static void TidyPaths(struct Gen *gen)
{
    TidyPathBlocks(gen);
    if (gen->p->kind == RPLACE_ROUTE || gen->p->kind == RPLACE_TOWN)
        MendPathBends(gen);
}

// What a block shows underfoot, for TidyGround: a cover of the paths and
// grounds, V_NONE for grass (tall grass and flowers too), or 0xFF where it
// is not open ground (trees, water, rock, buildings).
#define GROUND_SOLID 0xFF
static u8 GroundOf(struct Gen *gen, int fx, int fy)
{
    u8 c, v;
    if (!InMap(gen, fx, fy))
        return GROUND_SOLID;
    c = Cell(gen, fx >> 1, fy >> 1);
    if (c != C_OPEN && c != C_ROAD && c != C_LEDGE && c != C_KEEP)
        return GROUND_SOLID;
    v = Cover(gen, fx, fy);
    if (v == V_TALL || v == V_FLOWER)
        return V_NONE;
    return v;
}

// Up to max blocks of one ground joined to fx, fy, into list (packed);
// returns how many, or max + 1 if there are more, or 0 if the piece runs
// off the map (it goes on next door).
static int SmallPiece(struct Gen *gen, int fx, int fy, u8 ground, u16 *list, int max)
{
    int n = 1, i, d;
    list[0] = PACK(fx, fy);
    for (i = 0; i < n; i++)
    {
        for (d = 0; d < 4; d++)
        {
            int x = PX(list[i]) + (d == 1) - (d == 3), y = PY(list[i]) + (d == 2) - (d == 0), k;
            if (!InMap(gen, x, y))
                return 0;
            if (GroundOf(gen, x, y) != ground)
                continue;
            for (k = 0; k < n && list[k] != PACK(x, y); k++)
                ;
            if (k < n)
                continue;
            if (n == max)
                return max + 1;
            list[n++] = PACK(x, y);
        }
    }
    return n;
}

// A last tidy of the ground before it is drawn, as a real map would have it
// (only covers change, so nothing about where one can walk):
// - a trail broken by a block or two of grass is joined up,
// - a scrap of path, four blocks or fewer, standing alone is grassed over,
// - a block or three of grass left inside a path, a road or the sand takes
//   the ground round it, and a lone block or two of sand in the grass goes.
static void TidyGround(struct Gen *gen)
{
    int x, y, d, k, n;
    u16 list[5];
    u8 row[2][ROGUE_MAX_W]; // the ground of this row and the one above
    bool8 changed = FALSE;

    // A sign stands on grass, so the path goes round it.
    for (k = 0; k < gen->m->nSigns; k++)
        if (SignDrawn(gen->m->signs[k].kind) && InMap(gen, gen->m->signs[k].x, gen->m->signs[k].y)
         && IsPathCover(Cover(gen, gen->m->signs[k].x, gen->m->signs[k].y)))
            SetCover(gen, gen->m->signs[k].x, gen->m->signs[k].y, V_NONE);
    for (y = 0; y < gen->m->h; y++)
    {
        YIELD();
        for (x = 0; x < gen->m->w; x++)
        {
            u8 c = Cover(gen, x, y);
            if (IsPathCover(c))
            {
                // A path laid before trees grew over its unit
                // (ThickenSeamTrees) runs on under them: its edge would be
                // missing where it meets the trunks.
                // (A plaza's paving runs on under its fountain and lamps,
                // whose own blocks are drawn on it, as Rustboro's are.)
                u8 cell = Cell(gen, x >> 1, y >> 1);
                if (cell == C_OPEN || cell == C_ROAD || (cell == C_DECOR && c == V_ROAD))
                    continue;
                SetCover(gen, x, y, c = V_NONE);
            }
            for (d = 0; d < 2; d++)
            {
                int dx = d == 0, dy = d == 1;
                u8 a;
                if (!InMap(gen, x - dx, y - dy) || !IsPathCover(a = Cover(gen, x - dx, y - dy)) || !PathCanTake(gen, x, y, a))
                    continue;
                // The way on may be another kind of path (a town's road
                // meeting the sand it turns to by the seam): they join.
                if (InMap(gen, x + dx, y + dy) && IsPathCover(Cover(gen, x + dx, y + dy)))
                    SetCover(gen, x, y, a);
                else if (InMap(gen, x + 2 * dx, y + 2 * dy) && IsPathCover(Cover(gen, x + 2 * dx, y + 2 * dy))
                      && PathCanTake(gen, x + dx, y + dy, a))
                {
                    SetCover(gen, x, y, a);
                    SetCover(gen, x + dx, y + dy, a);
                }
                break;
            }
        }
    }
    TidyPaths(gen);
    for (y = 0; y < gen->m->h; y++)
    {
        YIELD();
        for (x = 0; x < gen->m->w; x++)
            row[y & 1][x] = GroundOf(gen, x, y);
        for (x = 0; x < gen->m->w; x++)
        {
            u8 g = row[y & 1][x];
            u8 votes[V_GROUND + 1];
            int border = 0, best = 0, open = 0;
            if (g == GROUND_SOLID || g == V_MAT)
                continue;
            // A piece is looked at from its top left block only: the first
            // in reading order, with other ground above it and to its left.
            if ((y > 0 && row[(y - 1) & 1][x] == g) || (x > 0 && row[y & 1][x - 1] == g))
                continue;
            YIELD();
            n = SmallPiece(gen, x, y, g, list, IsPathCover(g) ? 4 : 3);
            if (n == 0 || n > (IsPathCover(g) ? 4 : 3))
                continue;
            memset(votes, 0, sizeof(votes));
            for (k = 0; k < n; k++)
                for (d = 0; d < 4; d++)
                {
                    int bx = PX(list[k]) + (d == 1) - (d == 3), by = PY(list[k]) + (d == 2) - (d == 0);
                    u8 o = GroundOf(gen, bx, by);
                    if (o == g)
                        continue;
                    border++;
                    if (o != GROUND_SOLID)
                        open++;
                    if (o != GROUND_SOLID && o != V_MAT)
                        votes[o]++;
                }
            for (d = 1; d <= V_GROUND; d++)
                if (votes[d] > votes[best])
                    best = d;
            if (IsPathCover(g))
            {
                // A scrap of path takes the ground round it: grass, sand or
                // the other path it touches.
                if (votes[best] * 2 < border && votes[best] != open)
                    continue;
            }
            else if (g == V_GROUND)
            {
                // A lone block or two of sand in the grass.
                if (n > 2 || votes[V_NONE] * 2 < border)
                    continue;
                best = V_NONE;
            }
            else
            {
                // Grass inside a path, a road or the sand, or at the end of
                // one shut in by trees or rock.
                if (best == V_NONE || (votes[best] * 2 < border && votes[best] != open))
                    continue;
                // (Not the grass an ordinary tree stands on, beside it.)
                for (k = 0; k < n; k++)
                {
                    int ux = PX(list[k]) >> 1, uy = PY(list[k]) >> 1;
                    if ((PY(list[k]) & 1) && HasTreeBelow(gen, ux, uy))
                        break;
                    for (d = 0; d < 4; d++)
                    {
                        int bx = PX(list[k]) + (d == 1) - (d == 3), by = PY(list[k]) + (d == 2) - (d == 0);
                        if (InMap(gen, bx, by) && Cell(gen, bx >> 1, by >> 1) == C_TREE && !ThemedTreeU(gen, bx >> 1, by >> 1))
                            break;
                    }
                    if (d < 4)
                        break;
                }
                if (k < n)
                    continue;
            }
            // Under a person, an item or a sign too: it is only what the
            // ground looks like there (a square of grass under an item ball
            // in the sand, left when it is picked up).
            for (k = 0; k < n; k++)
                SetCover(gen, PX(list[k]), PY(list[k]), best);
            changed = TRUE;
        }
    }
    // Sand one block wide or tall (between trees and a tree's canopy, say)
    // has no edge set to draw it: it is grass there.
    if (SandyCover(gen, V_GROUND) && gen->townGround != TGROUND_SAND)
    {
        for (y = 0; y < gen->m->h; y++)
        {
            YIELD();
            for (x = 0; x < gen->m->w; x++)
            {
                if (Cover(gen, x, y) != V_GROUND || !IsGroundLike(gen, x, y))
                    continue;
                if ((!IsGroundLike(gen, x, y - 1) && !IsGroundLike(gen, x, y + 1))
                 || (!IsGroundLike(gen, x - 1, y) && !IsGroundLike(gen, x + 1, y)))
                    SetCover(gen, x, y, V_NONE);
            }
        }
    }
    if (changed)
        TidyPaths(gen);
}

// ---------------------------------------------------------------------------
// Rendering

static bool8 IsTreeU(struct Gen *gen, int x, int y)
{
    return Cell(gen, x, y) == C_TREE;
}

enum { TREES_ORDINARY, TREES_WOODS, TREES_OTHER };

static bool8 ThemedTreeU(struct Gen *gen, int ux, int uy);

// How a place draws its trees within view of a connection: ordinary trees,
// the small trees of deep woods, or something else (rock and sea).
static u8 PlaceTrees(const struct RoguePlace *q)
{
    // Fortree's woods are the small trees of deep woods.
    if (q->kind == RPLACE_TOWN)
        return (q->theme == RTHEME_TREETOP && q->type != RTOWN_SUMMIT) ? TREES_WOODS : TREES_ORDINARY;
    if (q->kind != RPLACE_ROUTE)
        return TREES_OTHER;
    switch (q->type)
    {
    case RROUTE_WOODS:
        return TREES_WOODS;
    case RROUTE_FOREST:
    case RROUTE_LAKE:
    case RROUTE_COAST:
    case RROUTE_LONGGRASS: // Routes 119 and 120's small trees
        return TREES_WOODS;
    case RROUTE_RIVER:
    case RROUTE_FIELD:
    case RROUTE_FLOWERS:
    case RROUTE_BRIDGE:
        return TREES_ORDINARY;
    default:
        return TREES_OTHER;
    }
}

// Places drawn mostly in rock: hills, deserts of sand and Victory Road.
static bool8 RockyPlace(const struct RoguePlace *q)
{
    return q->kind == RPLACE_ROUTE && (q->type == RROUTE_HILLS || q->type == RROUTE_MOUNTAIN
        || (q->type == RROUTE_DESERT && q->theme != RTHEME_ASH));
}

static bool8 WoodsFringeSide(const struct RogueRegion *r, const struct RoguePlace *q, u8 d);

// How many units deep the trees along a route's edge reach at a spot along
// it: one to three, changing every couple of units, so the tree line by a
// seam is ragged like Emerald's instead of one straight wall. It depends
// only on the plan, the side and the spot.
static int EdgeBand(const struct RogueRegion *r, const struct RoguePlace *q, u8 d, int pos)
{
    u32 h = RogueRng_Hash(r->seed, 7000 + (u32)(q - r->places) * 4 + d);
    return 1 + RogueRng_Hash(h, (pos + 1) >> 1) % 3;
}

// How far unit (ux, uy) is from edge d, and where along it.
static int EdgeDist(struct Gen *gen, u8 d, int ux, int uy, int *pos)
{
    *pos = (d == RDIR_NORTH || d == RDIR_SOUTH) ? ux : uy;
    return d == RDIR_NORTH ? uy : d == RDIR_SOUTH ? gen->ch - 1 - uy : d == RDIR_WEST ? ux : gen->cw - 1 - ux;
}

// A woods route meets a neighbour with other trees through a ragged band of
// ordinary trees, so its deep woods begin a little way in, not right at
// the seam. A rocky route keeps a ragged band of trees along a side that
// meets green country, so the neighbour's forest runs on across the seam
// and the rock starts unevenly behind it (not on its south side over a
// town, whose top row is the foot of the route's cliff).
static bool8 EdgeBandSide(const struct RogueRegion *r, const struct RoguePlace *q, u8 d)
{
    const struct RogueLink *l;
    const struct RoguePlace *t;
    if (q->kind != RPLACE_ROUTE)
        return FALSE;
    l = LinkOnSide(q, d);
    if (l == NULL)
        return FALSE;
    t = &r->places[l->target];
    if (q->type == RROUTE_LONGGRASS)
        return PlaceTrees(t) != TREES_WOODS;
    if (q->type == RROUTE_WOODS)
        return PlaceTrees(t) != TREES_WOODS && !WoodsFringeSide(r, t, Opposite(d));
    if (RockyPlace(q))
        return t->kind != RPLACE_CAVE && !RockyPlace(t) && !(d == RDIR_SOUTH && t->kind == RPLACE_TOWN);
    return FALSE;
}

static u8 EdgeTreesOf(const struct RogueRegion *r, const struct RoguePlace *q, u8 d);

// v1.1: a green route beside deep woods takes on their look toward them:
// the woods run on across the seam and give way to its own round trees a
// few units in (InWoodsFringe), as Route 104's trees do round Petalburg
// Woods, instead of the woods route ending in a band of round trees at the
// seam (EdgeBandSide). Not on a beach, nor among ash or island trees.
static bool8 WoodsFringeSide(const struct RogueRegion *r, const struct RoguePlace *q, u8 d)
{
    const struct RogueLink *l;
    const struct RoguePlace *t;
    if (q->kind != RPLACE_ROUTE || PlaceTrees(q) != TREES_ORDINARY || q->type == RROUTE_COAST
     || q->theme == RTHEME_ASH || q->theme == RTHEME_BEACH)
        return FALSE;
    l = LinkOnSide(q, d);
    if (l == NULL)
        return FALSE;
    t = &r->places[l->target];
    return t->kind == RPLACE_ROUTE && t->type == RROUTE_WOODS;
}

// A connected map is drawn with this map's tileset, and a neighbour with
// another theme draws ours with its own. Within this many blocks of an
// opening toward such a neighbour, a route keeps to the shared tiles.
#define SEAM_BLOCKS 9

// TRUE for a route that leaves the first town.
static bool8 LeavesStart(const struct RogueRegion *r, const struct RoguePlace *q)
{
    int i;
    for (i = 0; i < q->nLinks; i++)
        if (q->links[i].target == r->start)
            return TRUE;
    return FALSE;
}

static bool8 IsAshTown(const struct RoguePlace *q)
{
    return q->kind == RPLACE_TOWN && q->theme == RTHEME_ASH && q->type != RTOWN_SUMMIT;
}

// v1.1: is the place on side d of an ash route an ash field (a desert of
// ash all the way along, AshAtSide), whose tileset this route shares?
static bool8 AshFieldNextDoor(const struct RogueRegion *r, const struct RoguePlace *q, u8 d)
{
    const struct RogueLink *l = LinkOnSide(q, d);
    const struct RoguePlace *t;
    if (l == NULL || q->kind != RPLACE_ROUTE || q->theme != RTHEME_ASH)
        return FALSE;
    t = &r->places[l->target];
    return t->kind == RPLACE_ROUTE && t->theme == RTHEME_ASH && t->type == RROUTE_DESERT && !LeavesStart(r, t);
}

// Does a route lay its grey ash right up to side d? An ash route does on
// the side of the ash town it leads to (the ash drifts out from there), and
// an ash field all the way along on every side; then the ash may run on
// into a neighbour that has it too. Hills, Victory Road and the sea have
// none.
static bool8 AshAtSide(const struct RogueRegion *r, const struct RoguePlace *q, u8 d)
{
    const struct RogueLink *l = LinkOnSide(q, d);
    if (q->kind != RPLACE_ROUTE || q->theme != RTHEME_ASH || l == NULL)
        return FALSE;
    if (q->type == RROUTE_HILLS || q->type == RROUTE_MOUNTAIN || q->type == RROUTE_SEA)
        return FALSE;
    if (IsAshTown(&r->places[l->target]))
        return TRUE;
    // v1.1: an ash field next door drifts in across the seam too, and thins
    // out into this route's grass (PlanAshFade), as Route 113 does.
    if (AshFieldNextDoor(r, q, d))
        return TRUE;
    return q->type == RROUTE_DESERT && !LeavesStart(r, q);
}

// Does an ash route keep its ash back from unit (ux, uy)? Ash tiles may not
// show across a seam with a place of another look, so the ash keeps back
// from those edges, with a ragged edge of its own rather than the straight
// line of the seam view. Next to a route of the same tileset with no ash
// at the seam it keeps back less, but still unevenly. It runs on into the
// ash town (whose pink dirt comes out to meet it, PinkU) and into a route
// that has ash at the seam too. Within the seam view of another tileset
// (SEAM_BLOCKS) it never shows. It depends only on the plan, so the ash
// town next door knows where the route's ash comes to the seam.
static bool8 AshKeptBack(const struct RogueRegion *r, const struct RoguePlace *q, int ux, int uy)
{
    u32 h = RogueRng_Hash(r->seed, 6100 + (u32)(q - r->places));
    int k, cw = q->w / 2, ch = q->h / 2;
    for (k = 0; k < q->nLinks; k++)
    {
        const struct RogueLink *l = &q->links[k];
        const struct RoguePlace *t = &r->places[l->target];
        int dist = l->dir == RDIR_NORTH ? uy : l->dir == RDIR_SOUTH ? ch - 1 - uy
                 : l->dir == RDIR_WEST ? ux : cw - 1 - ux;
        int side = (l->dir == RDIR_NORTH || l->dir == RDIR_SOUTH) ? ux : uy;
        int keep = (int)(RogueRng_Hash(h, 200 + k * 64 + side / 2) % 3);
        if (IsAshTown(t) || (AshAtSide(r, q, l->dir) && AshAtSide(r, t, Opposite(l->dir))))
            continue;
        if (t->theme != q->theme && dist * 2 < SEAM_BLOCKS)
            return TRUE;
        keep += t->theme == q->theme ? 1 : SEAM_BLOCKS / 2 + 1;
        if (dist < keep)
            return TRUE;
    }
    return FALSE;
}

// A town whose own trees (Fortree's woods, Dewford's island trees,
// Fallarbor's ash trees) would meet a route's ordinary trees in a straight
// line at the seam. Along such a side the town's trees are ordinary ones in
// a ragged band (EdgeBand), so the trees run on across the seam and the
// town's own begin unevenly a little way in, as where Route 119 meets
// Fortree. An ash town keeps its ash trees toward a route with ash.
static bool8 TownBandSide(const struct RogueRegion *r, const struct RoguePlace *q, u8 d)
{
    const struct RogueLink *l;
    const struct RoguePlace *t;
    if (q->kind != RPLACE_TOWN || q->type == RTOWN_SUMMIT)
        return FALSE;
    l = LinkOnSide(q, d);
    if (l == NULL)
        return FALSE;
    t = &r->places[l->target];
    if (t->kind != RPLACE_ROUTE)
        return FALSE;
    switch (q->theme)
    {
    case RTHEME_TREETOP:
        // Not under a rocky route's cliff, whose foot is the town's edge.
        return EdgeTreesOf(r, t, Opposite(d)) == TREES_ORDINARY
            || (EdgeTreesOf(r, t, Opposite(d)) == TREES_OTHER && !RockyPlace(t));
    case RTHEME_BEACH:
        return TRUE;
    case RTHEME_ASH:
        return !AshAtSide(r, t, Opposite(d));
    default:
        return FALSE;
    }
}

// Does the ash route on side d of an ash town keep its ash back from the
// seam at unit pos along it (AshKeptBack)? Then its trees there are
// ordinary ones, and so are the town's.
static bool8 AshRouteKeptBackAt(struct Gen *gen, u8 d, int pos)
{
    YIELD();
    const struct RogueLink *l = LinkOnSide(gen->p, d);
    const struct RoguePlace *t;
    int rx, ry, along;
    if (l == NULL || !IsAshTown(gen->p))
        return FALSE;
    t = &gen->r->places[l->target];
    if (t->kind != RPLACE_ROUTE || !AshAtSide(gen->r, t, Opposite(d)))
        return FALSE;
    along = (pos * 2 - l->offset) >> 1;
    rx = d == RDIR_NORTH || d == RDIR_SOUTH ? along : d == RDIR_WEST ? t->w / 2 - 1 : 0;
    ry = d == RDIR_WEST || d == RDIR_EAST ? along : d == RDIR_NORTH ? t->h / 2 - 1 : 0;
    if (rx < 0 || ry < 0 || rx >= t->w / 2 || ry >= t->h / 2)
        return FALSE;
    return AshKeptBack(gen->r, t, rx, ry);
}

// Inside a town's band of ordinary trees (TownBandSide).
static bool8 InTownBand(struct Gen *gen, int ux, int uy)
{
    YIELD();
    u8 d;
    for (d = 0; d < 4; d++)
    {
        int pos, dist;
        dist = EdgeDist(gen, d, ux, uy, &pos);
        if (!TownBandSide(gen->r, gen->p, d)
         && !(AshRouteKeptBackAt(gen, d, pos) && AshRouteKeptBackAt(gen, d, pos - 1) && AshRouteKeptBackAt(gen, d, pos + 1)))
            continue;
        // An island town's forest there is at least two units deep.
        if (dist < EdgeBand(gen->r, gen->p, d, pos) + gen->islandTown)
            return TRUE;
    }
    return FALSE;
}

static bool8 InEdgeBand(struct Gen *gen, int ux, int uy)
{
    u8 d;
    for (d = 0; d < 4; d++)
    {
        int pos, dist;
        if (!EdgeBandSide(gen->r, gen->p, d))
            continue;
        dist = EdgeDist(gen, d, ux, uy, &pos);
        if (dist < EdgeBand(gen->r, gen->p, d, pos))
            return TRUE;
    }
    return FALSE;
}

// v1.1: inside a green route's fringe of deep woods (WoodsFringeSide),
// one unit deeper than a woods route's band of round trees.
static bool8 InWoodsFringe(struct Gen *gen, int ux, int uy)
{
    u8 d;
    bool8 in = FALSE;
    for (d = 0; d < 4 && !in; d++)
    {
        int pos, dist;
        if (!WoodsFringeSide(gen->r, gen->p, d))
            continue;
        dist = EdgeDist(gen, d, ux, uy, &pos);
        if (dist < EdgeBand(gen->r, gen->p, d, pos) + 1)
            in = TRUE;
    }
    // Not near another seam whose neighbour shows round trees there: the
    // woods would meet them at the corner.
    for (d = 0; d < 4 && in; d++)
    {
        int pos;
        const struct RogueLink *l = LinkOnSide(gen->p, d);
        if (l != NULL && !WoodsFringeSide(gen->r, gen->p, d) && EdgeDist(gen, d, ux, uy, &pos) < 1
         && EdgeTreesOf(gen->r, &gen->r->places[l->target], Opposite(d)) != TREES_WOODS)
            in = FALSE;
    }
    return in;
}

// The trees a place shows along its edge on side d.
static u8 EdgeTreesOf(const struct RogueRegion *r, const struct RoguePlace *q, u8 d)
{
    if (WoodsFringeSide(r, q, d))
        return TREES_WOODS;
    if (EdgeBandSide(r, q, d) || TownBandSide(r, q, d))
        return TREES_ORDINARY;
    return PlaceTrees(q);
}

// Trees drawn as the small trees of deep woods: on a woods route, and in a
// forest town.
// (v1.1) The trees beside the volcano's mountain's west face, and the one
// in front of its foot: round ones, the only trees Hoenn stands beside that
// face (Jagged Pass), whatever the place's woods or ash.
static bool8 VolcanoFaceTreeU(struct Gen *gen, int ux, int uy)
{
    return (gen->caveHill & 0x40) && ((ux == gen->mesa.x - 1 && uy >= 0 && uy <= 5) || (ux == gen->mesa.x && uy == 4));
}

static bool8 WoodsU(struct Gen *gen, int ux, int uy)
{
    if (VolcanoFaceTreeU(gen, ux, uy))
        return FALSE;
    if (gen->forestTown)
        return !InTownBand(gen, ux, uy);
    // Deep woods round the way into a forest, as on Route 104.
    if (gen->nForestWays && ForestWoodsU(gen, ux, uy))
        return TRUE;
    if (!gen->woods)
        return InWoodsFringe(gen, ux, uy);
    return !InEdgeBand(gen, ux, uy);
}

// How the tree in unit (ux, uy) is drawn.
static u8 UnitTrees(struct Gen *gen, int ux, int uy)
{
    if (WoodsU(gen, ux, uy))
        return TREES_WOODS;
    if (ThemedTreeU(gen, ux, uy))
        return TREES_OTHER;
    return TREES_ORDINARY;
}

// An ordinary (round) tree in unit (ux, uy) of this map.
static bool8 OrdinaryTreeU(struct Gen *gen, int ux, int uy)
{
    return InBounds(gen, ux, uy) && Cell(gen, ux, uy) == C_TREE && UnitTrees(gen, ux, uy) == TREES_ORDINARY;
}

// An ash or island tree in unit (ux, uy) of this map.
static bool8 OtherTreeU(struct Gen *gen, int ux, int uy)
{
    return InBounds(gen, ux, uy) && Cell(gen, ux, uy) == C_TREE && ThemedTreeU(gen, ux, uy);
}

// Does the ordinary tree in unit (ux, uy) stand with its trunk among the
// crowns of deep woods below, as round Petalburg Woods on Route 104
// (0x1EC 0x1ED)? Only where the line of trees goes on to its right: Route 104
// draws no end of a line there, so a tree at the end keeps its trunk on the
// ground, with the woods' tops below.
static bool8 RoundOverWoodsU(struct Gen *gen, int ux, int uy)
{
    return OrdinaryTreeU(gen, ux, uy) && uy + 1 < gen->ch && Cell(gen, ux, uy + 1) == C_TREE
        && WoodsU(gen, ux, uy + 1) && !ThemedTreeU(gen, ux, uy + 1)
        && IsTreeU(gen, ux + 1, uy) && !OtherTreeU(gen, ux + 1, uy);
}

// Is there a tree in the unit below, whose canopy reaches up into this one?
// Past the bottom edge is the connected map or the border. The trees go on
// there only if the map below draws the same trees; the border is drawn
// with this map's own trees. Open ground at the edge is an exit and gets no
// canopy.
static bool8 HasTreeBelow(struct Gen *gen, int ux, int uy)
{
    int i;
    if (uy + 1 < gen->ch)
        return IsTreeU(gen, ux, uy + 1);
    if (Cell(gen, ux, uy) != C_TREE)
        return FALSE;
    for (i = 0; i < gen->p->nLinks; i++)
        if (gen->p->links[i].dir == RDIR_SOUTH)
            return UnitTrees(gen, ux, uy) != TREES_OTHER
                && EdgeTreesOf(gen->r, &gen->r->places[gen->p->links[i].target], RDIR_NORTH) == UnitTrees(gen, ux, uy);
    return !ThemedTreeU(gen, ux, uy);
}

static bool8 IsTypeF(struct Gen *gen, int fx, int fy, u8 type, bool8 outside)
{
    if (!InMap(gen, fx, fy))
        return outside;
    return Cell(gen, fx >> 1, fy >> 1) == type;
}

// Pick from a 3x3 path set by which neighbors share the cover.
static u16 PathBlock(struct Gen *gen, int fx, int fy, u8 cover)
{
    u16 base = (cover == V_SAND) ? B_SAND_TL : (cover == V_ROAD ? gen->roadTL : B_PATH_TL);
    bool8 n = LooksSame(gen, fx, fy - 1, cover);
    bool8 s = LooksSame(gen, fx, fy + 1, cover);
    bool8 w = LooksSame(gen, fx - 1, fy, cover);
    bool8 e = LooksSame(gen, fx + 1, fy, cover);
    int row = !n ? 0 : (!s ? 2 : 1);
    int col = !w ? 0 : (!e ? 2 : 1);
    if (cover == V_ROAD && row == 1 && col == 1 && gen->roadCentre)
        return gen->roadCentre;
    return base + row * 8 + col;
}

// Lakes are rectangles, so only the top and side banks need edge tiles. The
// bottom bank is drawn by the shore row below.
static u16 WaterBlock(struct Gen *gen, int fx, int fy)
{
    bool8 n = IsTypeF(gen, fx, fy - 1, C_WATER, FALSE);
    bool8 w = IsTypeF(gen, fx - 1, fy, C_WATER, FALSE);
    bool8 e = IsTypeF(gen, fx + 1, fy, C_WATER, FALSE);

    if (!n && !w) return B_WATER_TL;
    if (!n && !e) return B_WATER_TR;
    if (!n)       return B_WATER_T;
    if (!w)       return B_WATER_L;
    if (!e)       return B_WATER_R;
    return B_WATER_C;
}

static void Put(struct Gen *gen, int fx, int fy, u16 block)
{
    YIELD();
    gen->m->blocks[fy * gen->m->w + fx] = block;
}

static bool8 InRouteSeam(struct Gen *gen, int fx, int fy);

static bool8 IsSeaF(struct Gen *gen, int fx, int fy)
{
    // Beyond the map edge the sea goes on, so banks run off the edge.
    if (!InMap(gen, fx, fy))
        return Cell(gen, fx < 0 ? 0 : (fx >= gen->m->w ? gen->cw - 1 : fx >> 1),
                    fy < 0 ? 0 : (fy >= gen->m->h ? gen->ch - 1 : fy >> 1)) == C_SEA;
    return (Cell(gen, fx >> 1, fy >> 1) == C_SEA && !BittenF(gen, fx, fy, C_SEA))
        || SeaFordBittenF(gen, fx, fy); // (v1.1 sea: a bank's round corner)
}

// Water routes (src/rogue_gen_water.h, included at the end).
static bool8 WaterPondTiles(struct Gen *gen);
static bool8 WaterPondF(struct Gen *gen, int fx, int fy);
static u16 WaterPondBlock(struct Gen *gen, int fx, int fy);
static bool8 WaterBridgeF(struct Gen *gen, int fx, int fy);
static bool8 WaterPondCountry(struct Gen *gen);
static bool8 WaterWindingRiver(struct Gen *gen, int top, u8 inDir, u8 outDir);
static bool8 WaterBridgePond(struct Gen *gen, int top, u8 inDir, u8 outDir);
static bool8 WaterBridgeWay(struct Gen *gen, int ax, int ay, int bx, int by, int *mx, int *my);
static void WaterRender(struct Gen *gen);
static void WaterRiverVerge(struct Gen *gen);
static void WaterClearBridgeEnds(struct Gen *gen);
static void WaterFinish(struct Gen *gen);
static bool8 WaterObjsOffBridges(struct Gen *gen);
static void WaterPlanRoutes(struct RogueRegion *r);
static bool8 WaterNoLongGrass(struct Gen *gen);

// A block of sea or river. The west, east and north banks are drawn inside
// the water, in one of two styles: against sand or against grass.
static u16 SeaBlock(struct Gen *gen, int fx, int fy)
{
    // Banks against sand where any land block around is sand.
    static const u16 sGrass[] = { 0x118A, 0x1188, 0x1189, 0x1192, 0x1178, 0x1190, 0x1179, 0x119A, 0x1198 };
    static const u16 sSand[]  = { 0x1145, 0x1146, 0x112C, 0x1123, 0x111B, 0x1125, 0x111D, 0x112B, 0x112D };
    // Against the shallows of a ford, as in Lilycove's bay.
    static const u16 sShallow[] = { 0x11A3, 0x11A4, 0x11BE, 0x11B5, 0x11AD, 0x11B7, 0x11AF, 0x11BD, 0x11BF };
    bool8 n = !IsSeaF(gen, fx, fy - 1), e = !IsSeaF(gen, fx + 1, fy), w = !IsSeaF(gen, fx - 1, fy);
    bool8 ne = !IsSeaF(gen, fx + 1, fy - 1), nw = !IsSeaF(gen, fx - 1, fy - 1);
    const u16 *set = sGrass;
    int dx, dy;
    // A lake route's water is pond water (rogue_gen_water.h).
    if (WaterPondF(gen, fx, fy))
        return WaterPondBlock(gen, fx, fy);
    for (dy = -1; dy <= 1 && set != sShallow; dy++)
        for (dx = -1; dx <= 1; dx++)
            if ((IsTypeF(gen, fx + dx, fy + dy, C_FORD, FALSE) && !WaterBridgeF(gen, fx + dx, fy + dy)
                 && !SeaFordBittenF(gen, fx + dx, fy + dy)) || SeaSandBittenF(gen, fx + dx, fy + dy))
                set = sShallow;
    for (dy = -1; dy <= 1 && set == sGrass; dy++)
        for (dx = -1; dx <= 1; dx++)
            if (InMap(gen, fx + dx, fy + dy) && !IsSeaF(gen, fx + dx, fy + dy) && Cover(gen, fx + dx, fy + dy) == V_GROUND)
                set = sSand;
    // On the sea route all the land is sand, the trees on its edges too:
    // no dark grass bank where they meet the water.
    if (set == sGrass && ((gen->p->kind == RPLACE_ROUTE && gen->p->type == RROUTE_SEA) || gen->p->theme == RTHEME_RAFT))
        set = sSand;
    if (n && e) return set[0];
    if (n && w) return set[1];
    if (n)      return set[2];
    if (e && ne) return set[3];
    if (e)      return set[4];
    if (w && nw) return set[5];
    if (w)      return set[6];
    if (ne)     return set[7];
    if (nw)     return set[8];
    return B_SEA;
}

// A block of a ford: shallow water, with its edge against the deep water
// to the north drawn in it.
static u16 FordBlock(struct Gen *gen, int fx, int fy)
{
    if (!IsSeaF(gen, fx, fy - 1))
        return 0x319E;
    if (!IsSeaF(gen, fx + 1, fy - 1))
        return 0x319B;
    if (!IsSeaF(gen, fx - 1, fy - 1))
        return 0x319C;
    return 0x31AE;
}

static bool8 InSeamView(struct Gen *gen, int ux, int uy, int uw, int uh);
static u16 OnTownGround(struct Gen *gen, u16 block, bool8 inSeamView);

// TRUE if a route's ash lies on this unit: all over an ash field, or near
// the ash town on a route where it gives way to grass.
static bool8 AshZoneU(struct Gen *gen, int ux, int uy)
{
    int i;
    if (gen->groundKind != GROUND_ASH)
        return FALSE;
    if (!gen->ashFade)
        return TRUE;
    if (ux < 0 || uy < 0 || ux >= gen->cw || uy >= gen->ch)
        return FALSE;
    i = uy * gen->cw + ux;
    return (gen->ashZone[i >> 3] >> (i & 7)) & 1;
}

static bool8 AshAt(struct Gen *gen, int fx, int fy)
{
    return AshZoneU(gen, fx >> 1, fy >> 1);
}

// Is the tall grass at this block drawn as ash grass? Once the route is
// about to be drawn, whole patches are (PlanAshGrass); before that, any in
// the ash away from a seam.
static bool8 AshGrassF(struct Gen *gen, int fx, int fy)
{
    int i;
    if (!gen->ashGrassPlanned)
        return AshAt(gen, fx, fy) && !InRouteSeam(gen, fx, fy);
    if (!InMap(gen, fx, fy))
        return FALSE;
    i = fy * gen->m->w + fx;
    return (gen->free[i >> 3] >> (i & 7)) & 1;
}

// How many units deep the town's pink ash dirt runs into a route at a spot
// along the side of the ash town: two to five, in broad steps.
static int PinkDepth(struct Gen *gen, u8 d, int pos)
{
    u32 h = RogueRng_Hash(gen->r->seed, 7400 + (u32)(gen->p - gen->r->places) * 4 + d);
    return 2 + RogueRng_Hash(h, pos / 3) % 3 + RogueRng_Hash(h, 64 + pos) % 2;
}

static bool8 PinkReach(struct Gen *gen, int ux, int uy)
{
    u8 d;
    for (d = 0; d < 4; d++)
    {
        const struct RogueLink *l = LinkOnSide(gen->p, d);
        int pos, dist;
        if (l == NULL || !IsAshTown(&gen->r->places[l->target]))
            continue;
        dist = EdgeDist(gen, d, ux, uy, &pos);
        if (dist < PinkDepth(gen, d, pos))
            return TRUE;
    }
    return FALSE;
}

// Next to an ash town, the town's pink ash dirt runs on into the route's
// grey ash and stops a few units in with a ragged edge, as on Route 113
// west of Fallarbor, so the ground does not change at the seam. Only where
// the ash lies all round, since pink dirt has no edge against grass.
static bool8 PinkU(struct Gen *gen, int ux, int uy)
{
    int dx, dy;
    YIELD();
    if (gen->p->kind != RPLACE_ROUTE || gen->groundKind != GROUND_ASH || !gen->ashFade
     || ux < 0 || uy < 0 || ux >= gen->cw || uy >= gen->ch || !PinkReach(gen, ux, uy))
        return FALSE;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
            if (InBounds(gen, ux + dx, uy + dy) && !AshZoneU(gen, ux + dx, uy + dy))
                return FALSE;
    return TRUE;
}

static bool8 PinkF(struct Gen *gen, int fx, int fy)
{
    return InMap(gen, fx, fy) && PinkU(gen, fx >> 1, fy >> 1);
}

// A general metatile with grass under it, drawn on the pink ash dirt.
static u16 OnPink(u16 block)
{
    u32 i;
    for (i = 0; i < GROUND_VARIANT_COUNT; i++)
        if (sGroundVariantFrom[i] == (block & 0x3FF))
            return (block & ~0x3FF) | (GROUND_VARIANT_ASH_BASE + i);
    return block;
}

// What the ground becomes where grass or flowers are cleared for someone to
// stand on: ash in the ash, otherwise plain grass.
static u8 BareCover(struct Gen *gen, int fx, int fy)
{
    return (AshAt(gen, fx, fy) && !InRouteSeam(gen, fx, fy)) ? V_GROUND : V_NONE;
}

static bool8 AshTreeU(struct Gen *gen, int ux, int uy)
{
    int i;
    if (ux < 0 || uy < 0 || ux >= gen->cw || uy >= gen->ch || Cell(gen, ux, uy) != C_TREE)
        return FALSE;
    i = uy * gen->cw + ux;
    return (gen->ashTree[i >> 3] >> (i & 7)) & 1;
}

static bool8 ThemedTreeHereU(struct Gen *gen, int ux, int uy);

// A town tree drawn with the town's own tiles (island or ash trees). Within
// view of a neighbour with another tileset they stay ordinary trees. On a
// route, the olive trees in its ash. Island trees stack in columns, each
// tree's crown over the trunk of the one behind. An ordinary tree may stand
// behind them (their tips show under its trunk), but never in front: its
// crown would be cut flat under their trunks. So in a column of tree units
// everything above an ordinary tree is ordinary too.
static bool8 ThemedTreeU(struct Gen *gen, int ux, int uy)
{
    int y;
    if (VolcanoFaceTreeU(gen, ux, uy))
        return FALSE;
    if (gen->townGround == TGROUND_GRASS)
        return AshTreeU(gen, ux, uy);
    if (!ThemedTreeHereU(gen, ux, uy))
        return FALSE;
    if ((gen->townGround != TGROUND_SAND && gen->townGround != TGROUND_ASH) || !InBounds(gen, ux, uy) || Cell(gen, ux, uy) != C_TREE)
        return TRUE;
    for (y = uy + 1; y < gen->ch && Cell(gen, ux, y) == C_TREE; y++)
        if (!ThemedTreeHereU(gen, ux, y))
            return FALSE;
    return TRUE;
}

static bool8 ThemedTreeHereU(struct Gen *gen, int ux, int uy)
{
    YIELD();
    u8 d;
    if (gen->zoned)
        return !BitU(gen->grassZone, gen, ux, uy) && !InSeamView(gen, ux, uy, 1, 1);
    // The edge along a side that joins another tileset shows all along the
    // route next door, not just by the way in: ordinary trees there, two
    // to four units deep, whose crowns and trunks meet the route's trees
    // across the seam.
    for (d = 0; d < 4; d++)
    {
        int pos, dist;
        if (!gen->keepOut[d] || gen->exitPos[d] == 0xFF)
            continue;
        dist = EdgeDist(gen, d, ux, uy, &pos);
        if (dist < 1 + EdgeBand(gen->r, gen->p, d, pos))
            return FALSE;
    }
    return !InTownBand(gen, ux, uy) && !InSeamView(gen, ux, uy, 1, 1);
}

// A tree with no canopy reaching up into the unit above: the town's own
// trees, which draw their own tips.
static bool8 NoCanopyTreeU(struct Gen *gen, int ux, int uy)
{
    return ThemedTreeU(gen, ux, uy);
}

// Does the game draw an ash town's border (its dense mesh of ash trees,
// sBorderAshTrees) at unit (ux, uy) past the map's edge? Everywhere but
// where a neighbour's map is drawn across a seam. (A town on the sea draws
// its sea past its sea, but past its trees its trees.)
static bool8 AshBorderU(struct Gen *gen, int ux, int uy)
{
    const struct RoguePlace *p = gen->p;
    int i, k;
    if (p->kind != RPLACE_TOWN || p->theme != RTHEME_ASH || p->type == RTOWN_SUMMIT)
        return FALSE;
    for (i = 0; i < p->nLinks; i++)
    {
        const struct RogueLink *l = &p->links[i];
        const struct RoguePlace *t = &gen->r->places[l->target];
        for (k = 0; k < 2; k++)
        {
            int bx = ux * 2 + k, by = uy * 2 + k;
            switch (l->dir)
            {
            case RDIR_NORTH: if (uy < 0 && bx >= l->offset && bx < l->offset + t->w) return FALSE; break;
            case RDIR_SOUTH: if (uy >= gen->ch && bx >= l->offset && bx < l->offset + t->w) return FALSE; break;
            case RDIR_WEST:  if (ux < 0 && by >= l->offset && by < l->offset + t->h) return FALSE; break;
            default:         if (ux >= gen->cw && by >= l->offset && by < l->offset + t->h) return FALSE; break;
            }
        }
    }
    return TRUE;
}

// Is an ash tree's top left corner at block (x, y)? Every ash tree unit has
// one at its own corner. Where four ash tree units meet, another tree
// stands on the corner between them, so the forest closes up the way
// Route 113's does. Nothing reaches out of the ash tree units. Past an ash
// town's edge the border's mesh of trees goes on, so the units there count
// as ash trees and the town's edge trees close up with it instead of
// standing cut off under it.
static bool8 AshTreeUnit(struct Gen *gen, int ux, int uy)
{
    if (ux < 0 || uy < 0 || ux >= gen->cw || uy >= gen->ch)
        return !gen->islandTrees && AshBorderU(gen, ux, uy);
    return Cell(gen, ux, uy) == C_TREE && !gen->islandTrees && ThemedTreeU(gen, ux, uy);
}

static bool8 AshTreeAnchor(struct Gen *gen, int x, int y)
{
    int ux, uy;
    // (A tree on the corner just past the top or left edge reaches in.)
    if (x < -1 || y < -1 || ((x ^ y) & 1))
        return FALSE;
    ux = (x + 2) / 2 - 1;
    uy = (y + 2) / 2 - 1;
    if (!(x & 1))
        return AshTreeUnit(gen, ux, uy);
    return AshTreeUnit(gen, ux, uy) && AshTreeUnit(gen, ux + 1, uy)
        && AshTreeUnit(gen, ux, uy + 1) && AshTreeUnit(gen, ux + 1, uy + 1);
}

// One block of an ash tree unit, from the trees whose quarters cover it.
static u16 AshTreeBlock(struct Gen *gen, int x, int y)
{
    const u16 *set = (gen->townGround == TGROUND_ASH || PinkF(gen, x, y)) ? sAshTreePink : sAshTreeGrey;
    bool8 tl = AshTreeAnchor(gen, x, y), tr = AshTreeAnchor(gen, x - 1, y);
    bool8 bl = AshTreeAnchor(gen, x, y - 1), br = AshTreeAnchor(gen, x - 1, y - 1);
    if (tl && br)
        return set[(y & 1) ? ASHQ_TLBR_ODD : ASHQ_TLBR_EVEN];
    if (tr && bl)
        return set[(y & 1) ? ASHQ_TRBL_ODD : ASHQ_TRBL_EVEN];
    if (tl)
        return set[ASHQ_TL];
    if (tr)
        return set[ASHQ_TR];
    if (bl)
        return set[ASHQ_BL];
    return set[ASHQ_BR];
}

// Open ground that counts as the map's special ground for its edges: the
// ground itself, and anything that is not open ground (an edge against a
// rock or a tree looks wrong).
static bool8 IsGroundLike(struct Gen *gen, int fx, int fy)
{
    u8 c, cover;
    if (!InMap(gen, fx, fy))
        return TRUE;
    c = Cell(gen, fx >> 1, fy >> 1);
    // A building's lot on grass shows grass round the stamp, and ordinary
    // trees and fences stand on grass: the sand keeps its edge by them.
    if (gen->townGround != TGROUND_SAND && (c == C_BLDG || c == C_DECOR) && !(gen->noLawn && c == C_BLDG)
     && !(c == C_DECOR && RockDecorU(gen, fx >> 1, fy >> 1)))
        return FALSE;
    if (c == C_TREE && SandyCover(gen, V_GROUND) && !(gen->islandTrees && ThemedTreeU(gen, fx >> 1, fy >> 1)))
        return FALSE;
    if (c != C_OPEN && c != C_ROAD && c != C_SHORE && c != C_KEEP && c != C_LEDGE)
        return TRUE;
    cover = Cover(gen, fx, fy);
    // A tree's canopy over the ground is drawn over grass (General has no
    // tips over sand or ash), so the sand or ash keeps its edge above it.
    if (cover == V_GROUND && gen->townGround == TGROUND_GRASS && !PinkF(gen, fx, fy) && (fy & 1)
     && HasTreeBelow(gen, fx >> 1, fy >> 1) && !NoCanopyTreeU(gen, fx >> 1, (fy >> 1) + 1))
        return FALSE;
    return cover == V_GROUND || (cover == V_TALL && AshGrassF(gen, fx, fy))
        || (IsPathCover(cover) && SandyCover(gen, V_GROUND));
}

// Sand or ash, with an edge toward any grass beside it.
static u16 SpecialGroundBlock(struct Gen *gen, int fx, int fy)
{
    bool8 n = IsGroundLike(gen, fx, fy - 1), s = IsGroundLike(gen, fx, fy + 1);
    bool8 w = IsGroundLike(gen, fx - 1, fy), e = IsGroundLike(gen, fx + 1, fy);
    int row = !n ? 0 : (!s ? 2 : 1);
    int col = !w ? 0 : (!e ? 2 : 1);
    // A town on ash has no grass to edge against, but for a hard line by
    // the seams.
    if (gen->townGround == TGROUND_ASH)
    {
        return (RogueRng_Hash(gen->rng, fy * gen->m->w + fx) & 15) == 0 ? B_TOWN_ASH_TUFT : B_TOWN_ASH;
    }
    if (gen->groundKind == GROUND_ASH)
    {
        bool8 pn, ps, pw, pe;
        if (PinkF(gen, fx, fy))
            return (RogueRng_Hash(gen->rng, fy * gen->m->w + fx) & 15) == 0 ? B_TOWN_ASH_TUFT : B_TOWN_ASH;
        // Grey ash meeting the pink dirt has its own edge, pink outside.
        pn = PinkF(gen, fx, fy - 1);
        ps = PinkF(gen, fx, fy + 1);
        pw = PinkF(gen, fx - 1, fy);
        pe = PinkF(gen, fx + 1, fy);
        if (pn || ps || pw || pe)
        {
            row = (pn || !n) ? 0 : ((ps || !s) ? 2 : 1);
            col = (pw || !w) ? 0 : ((pe || !e) ? 2 : 1);
            return (row == 1 && col == 1) ? B_ASH : B_ASH_PINK_TL + row * 8 + col;
        }
        return (row == 1 && col == 1) ? B_ASH : B_ASH_TL + row * 8 + col;
    }
    // Sand meeting the sea from below.
    if (IsSeaF(gen, fx, fy - 1) && InMap(gen, fx, fy - 1))
    {
        if (!IsSeaF(gen, fx + 1, fy - 1))
            return 0x313D;
        if (!IsSeaF(gen, fx - 1, fy - 1))
            return 0x313E;
        return B_SAND_SHORE;
    }
    // (The tips of island trees on the sand above them: ShapeIslandTrees.)
    if (row == 1 && col == 1)
        return B_SAND;
    return B_SAND_TL + row * 8 + col;
}

// Long grass is drawn in whole patches, each column at least two blocks
// tall and ending in the lower edge, as on Routes 119 and 120. The edge is
// Fortree's own, so a patch that comes within view of a neighbour with
// another tileset stays ordinary tall grass, all of it. Marked in
// gen->free, which is free until the trainers are placed after rendering.
static bool8 LongGrassAt(struct Gen *gen, int fx, int fy)
{
    int i;
    if (!gen->longGrass || !InMap(gen, fx, fy))
        return FALSE;
    i = fy * gen->m->w + fx;
    return (gen->free[i >> 3] >> (i & 7)) & 1;
}

static void SetLongGrass(struct Gen *gen, int fx, int fy, bool8 on)
{
    int i = fy * gen->m->w + fx;
    if (on)
        gen->free[i >> 3] |= 1 << (i & 7);
    else
        gen->free[i >> 3] &= ~(1 << (i & 7));
}

// Under a tree's canopy, which is drawn over the grass.
static bool8 UnderCanopyF(struct Gen *gen, int fx, int fy)
{
    return (fy & 1) && HasTreeBelow(gen, fx >> 1, fy >> 1) && !NoCanopyTreeU(gen, fx >> 1, (fy >> 1) + 1);
}

// Grass that is drawn as encounter grass: not under a canopy, a sign or a
// bank, and not the ash's own grass.
static bool8 DrawnTallF(struct Gen *gen, int fx, int fy)
{
    return InMap(gen, fx, fy) && Cover(gen, fx, fy) == V_TALL && !AshAt(gen, fx, fy) && !UnderCanopyF(gen, fx, fy)
        && !IsOccupied(gen, fx, fy) && !(InMap(gen, fx, fy - 1) && IsSeaF(gen, fx, fy - 1));
}

static bool8 LongGrassCandidate(struct Gen *gen, int fx, int fy)
{
    return DrawnTallF(gen, fx, fy) && !InRouteSeam(gen, fx, fy) && !InRouteSeam(gen, fx, fy + 1)
        && !InSeamView(gen, fx >> 1, fy >> 1, 1, 1) && !InSeamView(gen, fx >> 1, (fy + 1) >> 1, 1, 1);
}

static bool8 PlainGrassF(struct Gen *gen, int fx, int fy)
{
    u8 c;
    if (!InMap(gen, fx, fy) || Cover(gen, fx, fy) != V_NONE || IsOccupied(gen, fx, fy) || UnderCanopyF(gen, fx, fy))
        return FALSE;
    c = Cell(gen, fx >> 1, fy >> 1);
    return c == C_OPEN || c == C_KEEP;
}

static void PlanLongGrass(struct Gen *gen)
{
    int x, y, d, w = gen->m->w, h = gen->m->h;
    bool8 changed = TRUE;
    memset(gen->free, 0, sizeof(gen->free));
    if (!gen->longGrass)
        return;
    // A lone row of grass grows a second row onto plain grass below it, or
    // goes.
    for (y = 0; y < h; y++)
    {
        YIELD();
        for (x = 0; x < w; x++)
        {
            if (!LongGrassCandidate(gen, x, y) || DrawnTallF(gen, x, y - 1)
             || (y + 1 < h && LongGrassCandidate(gen, x, y + 1)))
                continue;
            if (PlainGrassF(gen, x, y + 1) && !InRouteSeam(gen, x, y + 2))
                SetCover(gen, x, y + 1, V_TALL);
            else
                SetCover(gen, x, y, V_NONE);
        }
    }
    for (y = 0; y < h; y++)
    {
        YIELD();
        for (x = 0; x < w; x++)
            if (LongGrassCandidate(gen, x, y))
                SetLongGrass(gen, x, y, TRUE);
    }
    // A patch is long only if every block of it may be: one that reaches
    // into view of another tileset stays tall grass.
    while (changed)
    {
        changed = FALSE;
        for (y = 0; y < h; y++)
        {
            YIELD();
            for (x = 0; x < w; x++)
            {
                if (!LongGrassAt(gen, x, y))
                    continue;
                for (d = 0; d < 4; d++)
                {
                    int nx = x + sDirDX[d], ny = y + sDirDY[d];
                    if (DrawnTallF(gen, nx, ny) && !LongGrassAt(gen, nx, ny))
                    {
                        SetLongGrass(gen, x, y, FALSE);
                        changed = TRUE;
                        break;
                    }
                }
            }
        }
    }
}

// One block of open ground.
static u16 GroundBlock(struct Gen *gen, int fx, int fy, u8 unit, bool8 treeBelow)
{
    u8 cover = Cover(gen, fx, fy);

    // The canopy of an ordinary tree below reaches up into this block.
    if (treeBelow && (fy & 1) && !NoCanopyTreeU(gen, fx >> 1, (fy >> 1) + 1))
    {
        if (WoodsU(gen, fx >> 1, (fy >> 1) + 1))
            return (fx & 1) ? B_WOODS_SHADE + 1 : B_WOODS_SHADE;
        if (cover == V_TALL && !gen->longGrass && !AshGrassF(gen, fx, fy))
            return (fx & 1) ? B_CANOPY_TALL_R : B_CANOPY_TALL_L;
        // On a town's sand, Route 109's tips on sand, which every tileset
        // shares; on ash, tips drawn over that ground.
        if (cover == V_GROUND && gen->townGround == TGROUND_SAND)
            return (fx & 1) ? 0x31F3 : 0x31F2;
        if (cover == V_GROUND && PinkF(gen, fx, fy))
            return OnPink((fx & 1) ? B_CANOPY_R : B_CANOPY_L);
        if (cover == V_GROUND)
            return OnTownGround(gen, (fx & 1) ? B_CANOPY_R : B_CANOPY_L, InSeamView(gen, fx >> 1, fy >> 1, 1, 1));
        return (fx & 1) ? B_CANOPY_R : B_CANOPY_L;
    }
    if (unit == C_SHORE && !(fy & 1) && (IsTypeF(gen, fx, fy - 1, C_WATER, FALSE) || IsSeaF(gen, fx, fy - 1)))
        return B_SHORE;
    if (cover == V_GROUND)
    {
        u16 block = SpecialGroundBlock(gen, fx, fy);
        // In view from a route of another tileset the ground is grass.
        if ((block & 0x3FF) >= 0x200 && InSeamView(gen, fx >> 1, fy >> 1, 1, 1))
            return B_GRASS;
        return block;
    }
    // A town's road or path in its own tileset (Fortree's forest floor) is
    // a worn path or sand in view from a route of another tileset, shaped
    // the same.
    if (IsPathCover(cover) && InSeamView(gen, fx >> 1, fy >> 1, 1, 1) && (PathBlock(gen, fx, fy, cover) & 0x3FF) >= 0x200)
    {
        bool8 n = LooksSame(gen, fx, fy - 1, cover), s = LooksSame(gen, fx, fy + 1, cover);
        bool8 w = LooksSame(gen, fx - 1, fy, cover), e = LooksSame(gen, fx + 1, fy, cover);
        return (cover == V_SAND ? B_SAND_TL : B_PATH_TL) + (!n ? 0 : (!s ? 2 : 1)) * 8 + (!w ? 0 : (!e ? 2 : 1));
    }
    // Grass with the sea or a river to the north.
    if (InMap(gen, fx, fy - 1) && IsSeaF(gen, fx, fy - 1))
        return B_LIP;
    switch (cover)
    {
    case V_TALL:
        if (AshGrassF(gen, fx, fy))
            return B_ASH_GRASS;
        if (LongGrassAt(gen, fx, fy))
            return LongGrassAt(gen, fx, fy + 1) ? B_LONG_GRASS : B_LONG_GRASS_EDGE;
        return B_TALL_GRASS;
    case V_PATH:
    case V_SAND:
    case V_ROAD:   return PathBlock(gen, fx, fy, cover);
    case V_FLOWER: return B_FLOWERS;
    // Petalburg's bare patch; in any other tileset that metatile is
    // something else (rock, water, a lamp), so plain grass there.
    // (Not in view from a route of another tileset.)
    case V_MAT:    return gen->p->theme == RTHEME_MEADOW && !(gen->p->kind == RPLACE_TOWN && gen->p->type == RTOWN_SUMMIT)
                       && !InSeamView(gen, fx >> 1, fy >> 1, 1, 1) ? B_DOORMAT : B_GRASS;
    default:       return B_GRASS;
    }
}

static void RenderVolcanoHill(struct Gen *gen);

// The hill with the cave mouth along the top (PlaceCaveHill): one of a set
// worked out from the neighbours Hoenn's maps have
// (tools/roguemon/gen_cave_hill.py). The grass round its edges stays the
// ground below it.
static void RenderMesa(struct Gen *gen)
{
    const struct CaveHill *t;
    int x, y;
    if (!gen->caveHill)
        return;
    if (gen->caveHill & 0x40)
    {
        RenderVolcanoHill(gen); // (v1.1)
        return;
    }
    // (A hill with a hideout's door beside the cave's mouth: sCaveHillTwins.)
    t = (gen->caveHill & 0x80) ? &sCaveHillTwins[(gen->caveHill & 0x7F) - 1] : &sCaveHills[gen->caveHill - 1];
    for (y = 0; y < CAVE_HILL_H; y++)
    {
        for (x = 0; x < t->w; x++)
        {
            u16 block = t->blocks[y * t->w + x];
            int fx = gen->mesa.x * 2 + x;
            // On an ash or sand town's ground, the rock's corners too.
            if (block)
                Put(gen, fx, y, OnTownGround(gen, block, InSeamView(gen, fx >> 1, y >> 1, 1, 1)));
        }
    }
}

static void RenderBerryPatch(struct Gen *gen)
{
    int x = gen->berryX, y = gen->berryY;
    Put(gen, x, y, B_SOIL_L);
    Put(gen, x + 1, y, B_SOIL);
    Put(gen, x + 2, y, B_SOIL);
    Put(gen, x + 3, y, B_SOIL_R);
    Put(gen, x, y + 1, B_SOIL_FRONT_L);
    Put(gen, x + 1, y + 1, B_SOIL_FRONT);
    Put(gen, x + 2, y + 1, B_SOIL_FRONT);
    Put(gen, x + 3, y + 1, B_SOIL_FRONT_R);
}

// Buildings, decorations and signs are drawn after the trees, over the tips
// that a tree in the unit below reaches up with. A tree in a dead end gives
// way to open ground, which cannot join two parts of the map. Otherwise the
// sign or decoration goes: it was only placed where it blocked nothing, so
// open ground in its place opens no new way either.
static bool8 CoversBlock(int x, int y, int w, int h, int fx, int fy)
{
    return fx + 1 >= x && fx < x + w && fy >= y && fy < y + h;
}

static void ClearTreesUnderScenery(struct Gen *gen)
{
    int ux, uy, i, k;
    for (uy = 1; uy < gen->ch; uy++)
    {
        YIELD();
        for (ux = 0; ux < gen->cw; ux++)
        {
            int fx = ux * 2, fy = uy * 2 - 1, open = 0;
            bool8 covered = Cell(gen, ux, uy - 1) == C_BLDG;
            if (Cell(gen, ux, uy) != C_TREE)
                continue;
            for (i = 0; i < gen->m->nSigns; i++)
                if (CoversBlock(gen->m->signs[i].x, gen->m->signs[i].y, 1, 1, fx, fy))
                    covered = TRUE;
            for (i = 0; i < gen->nDecor; i++)
                if (CoversBlock(gen->decor[i].x, gen->decor[i].y, gen->decor[i].w, gen->decor[i].h, fx, fy))
                    covered = TRUE;
            for (i = 0; i < gen->nFences; i++)
                if (CoversBlock(gen->fences[i].x, gen->fences[i].y, gen->fences[i].len, 1, fx, fy))
                    covered = TRUE;
            if (!covered)
                continue;
            // Rocky maps have worked out their rock levels from the units.
            if (!IsEdge(gen, ux, uy) && !gen->rocky && !(gen->p->kind == RPLACE_ROUTE && gen->p->type == RROUTE_MOUNTAIN))
            {
                open += IsOpenU(Cell(gen, ux - 1, uy));
                open += IsOpenU(Cell(gen, ux + 1, uy));
                open += IsOpenU(Cell(gen, ux, uy + 1));
                if (open <= 1)
                {
                    SetCell(gen, ux, uy, C_OPEN);
                    continue;
                }
            }
            for (i = 0; i < gen->m->nSigns; i++)
            {
                if (!CoversBlock(gen->m->signs[i].x, gen->m->signs[i].y, 1, 1, fx, fy))
                    continue;
                gen->m->signs[i] = gen->m->signs[--gen->m->nSigns];
                i--;
            }
            for (i = 0; i < gen->nDecor; i++)
            {
                struct Decor *d = &gen->decor[i];
                if (!CoversBlock(d->x, d->y, d->w, d->h, fx, fy))
                    continue;
                for (k = 0; k < d->w * d->h; k++)
                    if (Cell(gen, (d->x + k % d->w) >> 1, (d->y + k / d->w) >> 1) == C_DECOR)
                        SetCell(gen, (d->x + k % d->w) >> 1, (d->y + k / d->w) >> 1, C_OPEN);
                *d = gen->decor[--gen->nDecor];
                i--;
            }
        }
    }
}

// Water never sits right on top of a tree: there is shore between, as on
// every Hoenn coast. A tree just south of the sea gives way to shore.
static void ShoreUnderSea(struct Gen *gen)
{
    int ux, uy;
    if (gen->rocky || (gen->p->kind == RPLACE_ROUTE && gen->p->type == RROUTE_MOUNTAIN))
        return;
    for (uy = 1; uy < gen->ch; uy++)
    {
        YIELD();
        for (ux = 0; ux < gen->cw; ux++)
            if (Cell(gen, ux, uy) == C_TREE && Cell(gen, ux, uy - 1) == C_SEA)
                SetCell(gen, ux, uy, C_SHORE);
    }
}

// The rim of a town's terrace: a one-block rock edge along the terrace's
// lower sides, as on Route 111 and in Lilycove, with stone steps where a
// road goes down. A block of the terrace is rim if the ground below it, or
// beside it, or diagonally below it, is lower.
static u16 OnTownGround(struct Gen *gen, u16 block, bool8 inSeamView);
static bool8 InSeamView(struct Gen *gen, int ux, int uy, int uw, int uh);

static bool8 LowerF(struct Gen *gen, int fx, int fy)
{
    if (!InMap(gen, fx, fy))
        return FALSE;
    return !UpperU(gen, fx >> 1, fy >> 1);
}

static void RenderTerrace(struct Gen *gen)
{
    int fx, fy;
    for (fy = 0; fy < gen->m->h; fy++)
    {
        for (fx = 0; fx < gen->m->w; fx++)
        {
            int ux = fx >> 1, uy = fy >> 1;
            u8 c = Cell(gen, ux, uy);
            bool8 s, e, w, se, sw;
            u16 block;
            if (!UpperU(gen, ux, uy) || (c != C_CLIFF && c != C_ROAD))
                continue;
            s = LowerF(gen, fx, fy + 1);
            e = LowerF(gen, fx + 1, fy);
            w = LowerF(gen, fx - 1, fy);
            se = LowerF(gen, fx + 1, fy + 1);
            sw = LowerF(gen, fx - 1, fy + 1);
            if (s && !e && !w && IsStairU(gen, ux, uy))
                block = (fx & 1) ? 0x30CF : 0x30AF;
            else if (s && w)
                block = 0x0478;
            else if (s && e)
                block = 0x047A;
            else if (s)
                block = 0x0479;
            else if (w)
                block = 0x0470;
            else if (e)
                block = 0x0472;
            else if (sw)
                block = 0x0489;
            else if (se)
                block = 0x0474;
            else
                continue;
            Put(gen, fx, fy, OnTownGround(gen, block, InSeamView(gen, ux, uy, 1, 1)));
        }
    }
}

// One column of island trees, blocks fy0 to fy1. As in Dewford each tree is
// a tip, a crown or more and the lowest crown with its trunk, and behind a
// trunk stands the next tree's tip. The first tip is on the sand above
// where there is plain sand (not a building's, which is stamped later), or
// else within the grove (solid there). A column that runs off the map's
// edge goes on into the border's crowns.
static void ShapeIslandColumn(struct Gen *gen, int fx, int fy0, int fy1, bool8 topOpen, bool8 botOpen)
{
    u32 h = RogueRng_Hash(gen->rng ^ 0x1517, fy0 * gen->m->w + fx);
    int y = fy0, k;
    if (!topOpen && fy0 > 0 && gen->m->blocks[(fy0 - 1) * gen->m->w + fx] == B_SAND
     && Cell(gen, fx >> 1, (fy0 >> 1) - 1) != C_BLDG && (h & 1) && !InSeamView(gen, fx >> 1, (fy0 - 1) >> 1, 1, 1))
        Put(gen, fx, fy0 - 1, B_ISLAND_TOP);
    else if (!topOpen)
        Put(gen, fx, y++, B_ISLAND_TIP);
    h >>= 1;
    while (y <= fy1)
    {
        int left = fy1 - y + 1, body = 2 + h % 3;
        h = RogueRng_Hash(h, y);
        // Room for the next tree, a tip and a trunk, or none.
        if (botOpen ? body >= left : left - body < 2)
            body = left;
        for (k = 0; k < body; k++)
            Put(gen, fx, y + k, (k == body - 1 && !(botOpen && body == left)) ? B_ISLAND_FOOT : B_ISLAND_TREE);
        y += body;
        if (y <= fy1)
            Put(gen, fx, y++, B_ISLAND_TIP);
    }
}

// Island trees stand in groves of columns one block wide, each column going
// its own way, so the tops and the trees within are staggered as in Dewford.
static void ShapeIslandTrees(struct Gen *gen)
{
    int ux, uy, uy1, end;
    bool8 openN = LinkOnSide(gen->p, RDIR_NORTH) == NULL, openS = LinkOnSide(gen->p, RDIR_SOUTH) == NULL;
    bool8 topOpen, botOpen;
    if (!gen->islandTrees)
        return;
    for (ux = 0; ux < gen->cw; ux++)
    {
        for (uy = 0; uy < gen->ch; uy = uy1 + 1)
        {
            uy1 = uy;
            if (Cell(gen, ux, uy) != C_TREE)
                continue;
            while (uy1 + 1 < gen->ch && Cell(gen, ux, uy1 + 1) == C_TREE)
                uy1++;
            // The island trees are the foot of the column, if any: up to the
            // first ordinary tree below them (in view from a route of another
            // tileset), which stays as drawn.
            while (uy <= uy1 && !ThemedTreeU(gen, ux, uy))
                uy++;
            if (uy > uy1)
                continue;
            for (end = uy; end < uy1 && ThemedTreeU(gen, ux, end + 1); end++)
                ;
            topOpen = uy == 0 && openN;
            botOpen = end == gen->ch - 1 && openS;
            ShapeIslandColumn(gen, ux * 2, uy * 2, end * 2 + 1, topOpen, botOpen);
            ShapeIslandColumn(gen, ux * 2 + 1, uy * 2, end * 2 + 1, topOpen, botOpen);
        }
    }
}

// A rounded-off water corner takes the ground of the land beside it.
static void CoverBites(struct Gen *gen)
{
    int fx, fy;
    if (!gen->bites)
        return;
    for (fy = 0; fy < gen->m->h; fy++)
    {
        YIELD();
        for (fx = 0; fx < gen->m->w; fx++)
        {
            int dx = (fx & 1) ? 1 : -1, dy = (fy & 1) ? 1 : -1;
            u8 cover = V_NONE;
            if (!BittenF(gen, fx, fy, C_SEA))
                continue;
            if (Cover(gen, fx + dx, fy) == V_GROUND || Cover(gen, fx, fy + dy) == V_GROUND
             || Cover(gen, fx + dx, fy + dy) == V_GROUND || gen->p->type == RROUTE_SEA)
                cover = V_GROUND;
            SetCover(gen, fx, fy, cover);
        }
    }
}

static void RenderSpringRock(struct Gen *gen);
static u16 OnStoneGrass(struct Gen *gen, u16 block, u16 under);

// Woods' crowns drawn straight over an ordinary tree's crown (a treehouse's
// thicket over a town's tree line) end in their trunks on it, as Route 104
// draws them. Render does this for the woods it lays; this catches stamps.
static void JoinWoodsOnRound(struct Gen *gen)
{
    int fx, fy, w = gen->m->w;
    for (fy = 0; fy + 1 < gen->m->h; fy++)
    {
        YIELD();
        for (fx = 0; fx < w; fx++)
        {
            u16 t = gen->m->blocks[fy * w + fx] & 0x3FF, below = gen->m->blocks[(fy + 1) * w + fx] & 0x3FF;
            if ((t == (B_WOODS_CROWN & 0x3FF) || t == (B_WOODS_CROWN2 & 0x3FF))
             && below >= (B_TREE_TL & 0x3FF) && below <= (B_TREE_TR_END & 0x3FF))
                Put(gen, fx, fy, B_WOODS_ON_ROUND + (below & 1));
        }
    }
}

// Open ground a woods' trunk stands on or its top shows over: grass,
// flowers, tall grass, a sign, or a forest town's path (Fortree's 0x24B and
// 0x258 set). 2 for the path, 1 for the rest, 0 for anything else.
static u8 WoodsGroundKind(u16 block)
{
    u16 t = block & 0x3FF;
    if (t == (B_GRASS & 0x3FF) || t == 0x002 || t == (B_SIGN & 0x3FF) || t == (B_FLOWERS & 0x3FF)
     || t == (B_TALL_GRASS & 0x3FF))
        return 1;
    if (t == 0x24B || (t >= 0x258 && t <= 0x26A))
        return 2;
    return 0;
}

// Trees a woods crown goes on up into: the woods' own crowns, top and
// trunks, and an ordinary tree's crown or its trunk among the woods.
static bool8 WoodsGoOnUp(u16 block)
{
    u16 t = block & 0x3FF;
    return t == (B_WOODS_CROWN & 0x3FF) || t == (B_WOODS_CROWN2 & 0x3FF)
        || t == (B_WOODS_TOP & 0x3FF) || t == (B_WOODS_TOP & 0x3FF) + 1
        || t == (B_WOODS_TRUNK & 0x3FF) || t == (B_WOODS_TRUNK & 0x3FF) + 1
        || t == (B_WOODS_ON_ROUND & 0x3FF) || t == (B_WOODS_ON_ROUND & 0x3FF) + 1
        || (t >= (B_TREE_TL & 0x3FF) && t <= (B_TREE_BR & 0x3FF))
        || t == (B_TREE_ON_WOODS & 0x3FF) || t == (B_TREE_ON_WOODS & 0x3FF) + 1;
}

static bool8 IsRoundTreeFoot(u16 block)
{
    u16 t = block & 0x3FF;
    return t >= (B_TREE_BL_BASE & 0x3FF) && t <= (B_TREE_BR_END & 0x3FF);
}

// The woods round a forest town's stamps (a treehouse's thicket, the
// trees beside its lots) as Fortree draws them: a woods crown over open
// ground ends in its trunk (Fortree's 0x253 over its path), a crown under
// anything but more trees (open ground, a building's foot, a cliff, an
// ordinary tree's foot) shows the woods' top, and the trunks the woods above a lot
// end in become crowns where a treehouse's roof or thicket now stands
// under them.
static void TidyTownWoods(struct Gen *gen)
{
    int fx, fy, w = gen->m->w, h = gen->m->h;
    u16 *bl = gen->m->blocks;
    if (!gen->forestTown)
        return;
    for (fy = 0; fy + 1 < h; fy++)
    {
        YIELD();
        for (fx = 0; fx < w; fx++)
        {
            u16 t = bl[fy * w + fx] & 0x3FF, below = bl[(fy + 1) * w + fx] & 0x3FF;
            if ((t == (B_WOODS_TRUNK & 0x3FF) || t == (B_WOODS_TRUNK & 0x3FF) + 1)
             && (below == (B_WOODS_CROWN & 0x3FF) || below == (B_WOODS_CROWN2 & 0x3FF) || (below >= 0x224 && below <= 0x226)))
                Put(gen, fx, fy, B_WOODS_CROWN);
        }
    }
    for (fy = 0; fy + 1 < h; fy++)
    {
        YIELD();
        for (fx = 0; fx < w; fx++)
        {
            u16 t = bl[fy * w + fx] & 0x3FF;
            u8 below;
            if (t != (B_WOODS_CROWN & 0x3FF) && t != (B_WOODS_CROWN2 & 0x3FF))
                continue;
            below = WoodsGroundKind(bl[(fy + 1) * w + fx]);
            if (below)
                Put(gen, fx, fy, below == 2 ? 0x0653 : B_WOODS_TRUNK + (fx & 1));
            else if (fy > 0 && !WoodsGoOnUp(bl[(fy - 1) * w + fx]))
                // (Route 104 has its 0x00E under an ordinary tree's trunk.)
                Put(gen, fx, fy, IsRoundTreeFoot(bl[(fy - 1) * w + fx]) ? B_WOODS_TOP : B_WOODS_TOP + (fx & 1));
        }
    }
}
static void FinishRouteAsh(struct Gen *gen);

static void Render(struct Gen *gen)
{
    int ux, uy, i;
    ShoreUnderSea(gen);
    ClearTreesUnderScenery(gen);
    CoverBites(gen);
    PlanLongGrass(gen);
    FinishRouteAsh(gen);
    for (uy = 0; uy < gen->ch; uy++)
    {
        YIELD();
        for (ux = 0; ux < gen->cw; ux++)
        {
            YIELD();
            int fx = ux * 2, fy = uy * 2;
            u8 c = Cell(gen, ux, uy);
            bool8 treeBelow = HasTreeBelow(gen, ux, uy);
            switch (c)
            {
            case C_SEA:
                for (i = 0; i < 4; i++)
                {
                    int bx = fx + (i & 1), by = fy + (i >> 1);
                    Put(gen, bx, by, BittenF(gen, bx, by, C_SEA) ? GroundBlock(gen, bx, by, C_OPEN, FALSE) : SeaBlock(gen, bx, by));
                }
                break;
            case C_TREE:
            if (ThemedTreeU(gen, ux, uy))
            {
                if (gen->islandTrees)
                {
                    // Island trees, shaped once the whole map is drawn.
                    for (i = 0; i < 4; i++)
                        Put(gen, fx + (i & 1), fy + (i >> 1), B_ISLAND_TREE);
                }
                else
                {
                    for (i = 0; i < 4; i++)
                        Put(gen, fx + (i & 1), fy + (i >> 1), AshTreeBlock(gen, fx + (i & 1), fy + (i >> 1)));
                }
                break;
            }
            if (WoodsU(gen, ux, uy))
            {
                // Small trees: crowns, and trunks along the bottom of the woods.
                u32 h = RogueRng_Hash(gen->rng, uy * gen->cw + ux);
                // Woods meet ordinary trees as round Petalburg Woods on
                // Route 104: an ordinary tree above stands with its trunk
                // among the woods' crowns (0x1EC 0x1ED, drawn by that tree),
                // and over an ordinary tree the woods' trunks stand on its
                // crown (0x1F4 0x1F5). Beside ordinary trees, and over that
                // join, the crowns are the ones Route 104 draws there.
                bool8 roundAbove = RoundOverWoodsU(gen, ux, uy - 1);
                // Over an ash or island tree the woods end in their trunks:
                // that tree stands on its own ground.
                if (treeBelow && OtherTreeU(gen, ux, uy + 1))
                    treeBelow = FALSE;
                bool8 roundBelow = treeBelow && OrdinaryTreeU(gen, ux, uy + 1);
                bool8 roundL = OrdinaryTreeU(gen, ux - 1, uy), roundR = OrdinaryTreeU(gen, ux + 1, uy);
                // Under another tree's trunks (an ash or island tree) the
                // woods show their own tops.
                bool8 topped = uy > 0 && Cell(gen, ux, uy - 1) == C_TREE && !WoodsU(gen, ux, uy - 1) && !roundAbove;
                // (Route 104 has its 0x00E under an ordinary tree's trunk.)
                u16 top2 = OrdinaryTreeU(gen, ux, uy - 1) ? B_WOODS_TOP : B_WOODS_TOP + 1;
                Put(gen, fx, fy, topped ? B_WOODS_TOP : ((h & 1) && !roundL && !roundBelow) ? B_WOODS_CROWN2 : B_WOODS_CROWN);
                Put(gen, fx + 1, fy, topped ? top2 : ((h & 2) && !roundR && !roundBelow) ? B_WOODS_CROWN2 : B_WOODS_CROWN);
                if (roundBelow)
                {
                    Put(gen, fx, fy + 1, B_WOODS_ON_ROUND);
                    Put(gen, fx + 1, fy + 1, B_WOODS_ON_ROUND + 1);
                }
                else
                {
                    Put(gen, fx, fy + 1, treeBelow ? (((h & 4) && !roundL) ? B_WOODS_CROWN2 : B_WOODS_CROWN) : B_WOODS_TRUNK);
                    Put(gen, fx + 1, fy + 1, treeBelow ? (((h & 8) && !roundR) ? B_WOODS_CROWN2 : B_WOODS_CROWN) : B_WOODS_TRUNK + 1);
                }
                break;
            }
            else
            {
                // An ash or island tree beside an ordinary one is a tree of
                // its own on its own ground: the ordinary tree ends there.
                bool8 left = IsTreeU(gen, ux - 1, uy) && !OtherTreeU(gen, ux - 1, uy);
                bool8 right = IsTreeU(gen, ux + 1, uy) && !OtherTreeU(gen, ux + 1, uy);
                bool8 overWoods = RoundOverWoodsU(gen, ux, uy);
                // The tips of an ordinary tree below; a town's own trees
                // have none. Over the woods the trunk stands among their
                // crowns, as on Route 104.
                if (treeBelow && uy + 1 < gen->ch && (ThemedTreeU(gen, ux, uy + 1) || WoodsU(gen, ux, uy + 1)))
                    treeBelow = FALSE;
                Put(gen, fx, fy, left ? B_TREE_TL : B_TREE_TL_END);
                Put(gen, fx + 1, fy, right ? B_TREE_TR : B_TREE_TR_END);
                if (overWoods)
                {
                    Put(gen, fx, fy + 1, B_TREE_ON_WOODS);
                    Put(gen, fx + 1, fy + 1, B_TREE_ON_WOODS + 1);
                    break;
                }
                Put(gen, fx, fy + 1, treeBelow ? B_TREE_BL : (left ? B_TREE_BL_BASE : B_TREE_BL_END));
                Put(gen, fx + 1, fy + 1, treeBelow ? B_TREE_BR : (right ? B_TREE_BR_BASE : B_TREE_BR_END));
                break;
            }
            case C_WATER:
                for (i = 0; i < 4; i++)
                    Put(gen, fx + (i & 1), fy + (i >> 1), WaterBlock(gen, fx + (i & 1), fy + (i >> 1)));
                break;
            case C_FORD:
                for (i = 0; i < 4; i++)
                    Put(gen, fx + (i & 1), fy + (i >> 1), FordBlock(gen, fx + (i & 1), fy + (i >> 1)));
                break;
            case C_LEDGE:
            {
                bool8 left = Cell(gen, ux - 1, uy) == C_LEDGE;
                bool8 right = Cell(gen, ux + 1, uy) == C_LEDGE;
                Put(gen, fx, fy, GroundBlock(gen, fx, fy, c, FALSE));
                Put(gen, fx + 1, fy, GroundBlock(gen, fx + 1, fy, c, FALSE));
                Put(gen, fx, fy + 1, left ? B_LEDGE : B_LEDGE_L);
                Put(gen, fx + 1, fy + 1, right ? B_LEDGE : B_LEDGE_R);
                break;
            }
            default: // open ground, and the ground under buildings and rock
                for (i = 0; i < 4; i++)
                    Put(gen, fx + (i & 1), fy + (i >> 1), GroundBlock(gen, fx + (i & 1), fy + (i >> 1), c, treeBelow));
                break;
            }
        }
    }
    if (gen->terraced && gen->p->kind == RPLACE_TOWN)
        RenderTerrace(gen);
    if (gen->springRock)
        RenderSpringRock(gen);
    else if (gen->hasMesa)
        RenderMesa(gen);
    if (gen->hasBerry)
        RenderBerryPatch(gen);
    for (i = 0; i < gen->nFences; i++)
    {
        int k, len = gen->fences[i].len;
        bool8 seam = InSeamView(gen, gen->fences[i].x >> 1, gen->fences[i].y >> 1, (len + 1) >> 1, 1);
        for (k = 0; k < len; k++)
        {
            int fx = gen->fences[i].x + k, fy = gen->fences[i].y;
            Put(gen, fx, fy, OnStoneGrass(gen, OnTownGround(gen, gen->fence[k == 0 ? 0 : (k == len - 1 ? 2 : 1)], seam),
                                          gen->m->blocks[fy * gen->m->w + fx]));
        }
    }
    for (i = 0; i < gen->nDecor; i++)
    {
        const struct Decor *d = &gen->decor[i];
        int x, y;
        bool8 seam = InSeamView(gen, d->x >> 1, d->y >> 1, (d->w + 1) >> 1, (d->h + 1) >> 1);
        for (y = 0; y < d->h; y++)
            for (x = 0; x < d->w; x++)
                if (d->blocks[y * d->w + x] != 0xFFFF) // (v1.1c: not part of the stamp, as an irregular outcrop's corners)
                    Put(gen, d->x + x, d->y + y, OnCityGrass(gen, OnTownGround(gen, d->blocks[y * d->w + x], seam), d->x + x, d->y + y));
    }
    ShapeIslandTrees(gen);
}

// ---------------------------------------------------------------------------
// Victory Road
//
// The mountain route keeps the route layout but draws the solid units as
// rock instead of forest, like Route 111 and the Ever Grande approach: a
// cliff edge against the grass, and higher terraces stepping up inside the
// bigger masses. The tiles and their neighbors are taken from those maps.

#define MOUNTAIN_LEVELS 3
#define PASS_CAP 5 // the highest rock of a terraced mountain pass (rogue_gen_rock.h)

// Which edge of its level a block sits on. Only the three neighbors outside
// the block's own unit matter: the other three are in the same unit.
enum { MK_C, MK_TL, MK_T, MK_TR, MK_L, MK_R, MK_BL, MK_B, MK_BR, MK_NW, MK_NE, MK_SW, MK_SE };

// A rock edge against open ground, and a terrace edge against lower rock.
// The inner corners (MK_NW...: open only diagonally) are the same in both.
static const u16 sRockOnGround[] = { 0x0471, 0x0468, 0x0469, 0x046A, 0x0470, 0x0472, 0x0478, 0x0479, 0x047A, 0x04BE, 0x04BF, 0x0489, 0x0474 };
static const u16 sRockOnRock[]   = { 0x0471, 0x046B, 0x046C, 0x046D, 0x0473, 0x0475, 0x047B, 0x047C, 0x047D, 0x04BE, 0x04BF, 0x0489, 0x0474 };
// The same rock with a sand foot, for deserts (Route 111).
static const u16 sRockOnSand[]   = { 0x0471, 0x0480, 0x0481, 0x0482, 0x0488, 0x048A, 0x0490, 0x0491, 0x0492, 0x04BE, 0x04BF, 0x0489, 0x0474 };

static u8 MountainLevel(struct Gen *gen, const u8 *level, int ux, int uy)
{
    if (!InBounds(gen, ux, uy))
    {
        // Rock runs on past the edge, unless a map with trees or a town
        // joins there: then the rock ends in its own edge, not cut flat.
        u8 d = uy < 0 ? RDIR_NORTH : uy >= gen->ch ? RDIR_SOUTH : ux < 0 ? RDIR_WEST : RDIR_EAST;
        const struct RogueLink *l = gen->p->kind == RPLACE_ROUTE ? LinkOnSide(gen->p, d) : NULL;
        if (l != NULL && EdgeTreesOf(gen->r, &gen->r->places[l->target], Opposite(d)) != TREES_OTHER)
            return 0;
        // (A mountain pass's rock rises higher: rogue_gen_rock.h.)
        return gen->terraced && gen->p->kind == RPLACE_ROUTE ? PASS_CAP : MOUNTAIN_LEVELS;
    }
    return level[uy * gen->cw + ux];
}

static bool8 IsSandBlock(u16 b)
{
    b &= 0x3FF;
    return (b >= 0x118 && b <= 0x12A) || b == 0x0E2 || b == 0x145;
}

#define BIT_AT(a, i) (((a)[(i) >> 3] >> ((i) & 7)) & 1)
#define BIT_SET(a, i) ((a)[(i) >> 3] |= 1 << ((i) & 7))

// Sand and grass blocks along the open sides of rock unit (ux, uy): +1 for
// each sand block, -1 for each other (grass, a tree). The ground is drawn.
static int SandVotes(struct Gen *gen, const u8 *level, int ux, int uy)
{
    int d, k, n = 0;
    for (d = 0; d < 4; d++)
    {
        int nx = ux + sDirDX[d], ny = uy + sDirDY[d];
        if (!InBounds(gen, nx, ny) || level[ny * gen->cw + nx])
            continue;
        for (k = 0; k < 2; k++)
        {
            // The two blocks of the neighbour along the shared side.
            int fx = nx * 2 + (sDirDX[d] ? (sDirDX[d] > 0 ? 0 : 1) : k);
            int fy = ny * 2 + (sDirDY[d] ? (sDirDY[d] > 0 ? 0 : 1) : k);
            n += IsSandBlock(gen->m->blocks[fy * gen->m->w + fx]) ? 1 : -1;
        }
    }
    return n;
}

// A desert's rock masses (rock units joined side by side or corner to
// corner) have the sand foot where they stand mostly on sand, and the grass
// foot where they stand mostly on grass or by trees, as in Hoenn, where rock
// against grass has the grass foot. One foot for a whole mass, so its edges
// never change kind halfway. Sets a bit per unit of the grass-footed masses.
// A mass is found by sweeps, not a queue: the thread's stack is small.
static void GrassFootMasses(struct Gen *gen, const u8 *level, u8 *grass)
{
    u8 done[(MAX_CELLS + 7) / 8], cur[(MAX_CELLS + 7) / 8];
    int n = gen->cw * gen->ch, seed, i, votes;
    bool8 changed;
    memset(grass, 0, (MAX_CELLS + 7) / 8);
    memset(done, 0, sizeof(done));
    for (seed = 0; seed < n; seed++)
    {
        if (!level[seed] || BIT_AT(done, seed))
            continue;
        memset(cur, 0, sizeof(cur));
        BIT_SET(cur, seed);
        do
        {
            changed = FALSE;
            for (i = 0; i < n; i++)
            {
                int ux = i % gen->cw, uy = i / gen->cw, dx, dy;
                if (ux == 0)
                    YIELD();
                if (!level[i] || BIT_AT(cur, i))
                    continue;
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++)
                        if (InBounds(gen, ux + dx, uy + dy) && BIT_AT(cur, (uy + dy) * gen->cw + ux + dx) && !BIT_AT(cur, i))
                        {
                            BIT_SET(cur, i);
                            changed = TRUE;
                        }
            }
        } while (changed);
        votes = 0;
        for (i = 0; i < n; i++)
            if (BIT_AT(cur, i))
                votes += SandVotes(gen, level, i % gen->cw, i / gen->cw);
        for (i = 0; i < n; i++)
            if (BIT_AT(cur, i))
            {
                BIT_SET(done, i);
                if (votes < 0)
                    BIT_SET(grass, i);
            }
    }
}

static u8 MountainKey(struct Gen *gen, const u8 *level, int ux, int uy, int q, u8 l)
{
    int sx = (q & 1) ? 1 : -1, sy = (q & 2) ? 1 : -1;
    bool8 v = MountainLevel(gen, level, ux, uy + sy) >= l;
    bool8 h = MountainLevel(gen, level, ux + sx, uy) >= l;
    bool8 d = MountainLevel(gen, level, ux + sx, uy + sy) >= l;
    bool8 top = sy < 0, left = sx < 0;
    if (!v && !h)
        return top ? (left ? MK_TL : MK_TR) : (left ? MK_BL : MK_BR);
    if (!v)
        return top ? MK_T : MK_B;
    if (!h)
        return left ? MK_L : MK_R;
    if (!d)
        return top ? (left ? MK_NW : MK_NE) : (left ? MK_SW : MK_SE);
    return MK_C;
}

// The cave mouth in a rocky route's rock, along the foot of row y1. Where
// the rock round it is plain, as Route 110's and Route 120's: a porch on the
// face, a terrace's edge, its sides and the mouth's top over the mouth. Else
// a tall face over the clearing in front of the cave.
static void RenderMountainMouth(struct Gen *gen, int y1)
{
    static const u16 porch[4][3] = {
        { 0x046B, 0x046C, 0x046D },
        { 0x0473, 0x0471, 0x0475 },
        { 0x047B, B_MOUTH_TOP_PORCH, 0x047D },
        { 0x0479, B_MOUTH, 0x0479 },
    };
    int w = gen->m->w, mx = gen->mouthX, x, y;
    // (A mountain pass keeps the tall face, as before its rock was made
    // flatter in v1.1, 2026-10-14.)
    bool8 plain = mx >= 1 && mx + 1 < w && y1 >= 3 && gen->p->type != RROUTE_HILLS;
    for (y = y1 - 3; y <= y1 && plain; y++)
        for (x = mx - 1; x <= mx + 1 && plain; x++)
            if (gen->m->blocks[y * w + x] != (y == y1 ? sRockOnGround[MK_B] : sRockOnGround[MK_C]))
                plain = FALSE;
    if (plain)
    {
        for (y = 0; y < 4; y++)
            for (x = 0; x < 3; x++)
                Put(gen, mx - 1 + x, y1 - 3 + y, porch[y][x]);
        return;
    }
    for (x = 0; x < w; x++)
    {
        u16 *b = &gen->m->blocks[y1 * w + x];
        u16 *a = b - w;
        if (*b == sRockOnGround[MK_B] && *a == sRockOnGround[MK_C])
        {
            *a = B_ROCK_FACE;
            *b = 0x0491;
        }
    }
    Put(gen, mx, y1 - 1, B_MOUTH_TOP);
    Put(gen, mx, y1, B_MOUTH);
    // The mouth's top is never the end of a face: a face's top to its right
    // (a terrace's) goes on as the face beside it.
    if (mx + 1 < w && gen->m->blocks[(y1 - 1) * w + mx + 1] == 0x047C)
    {
        Put(gen, mx + 1, y1 - 1, B_ROCK_FACE);
        if (gen->m->blocks[y1 * w + mx + 1] == sRockOnGround[MK_B])
            Put(gen, mx + 1, y1, 0x0491);
    }
}

// Rock on sand ends in a face two blocks tall (Route 111): its sand foot
// with the face's top over it, from the rock's left side to its right side
// (the face's slanting ends against them), never the flat top right over
// the foot. Where a step of the rock meets a higher one instead, the foot
// stays as it is. A strip one unit thin on grass above keeps its grass foot.
static void SandFootFaces(struct Gen *gen)
{
    int w = gen->m->w, x, y, x0;
    for (y = 1; y < gen->m->h; y++)
    {
        u16 *b = &gen->m->blocks[y * w], *a = b - w;
        YIELD();
        for (x = 0; x < w; x++)
        {
            if (a[x] == sRockOnGround[MK_T] || a[x] == sRockOnGround[MK_TL] || a[x] == sRockOnGround[MK_TR])
            {
                if (b[x] == sRockOnSand[MK_B])
                    b[x] = sRockOnGround[MK_B];
                else if (b[x] == sRockOnSand[MK_BL])
                    b[x] = sRockOnGround[MK_BL];
                else if (b[x] == sRockOnSand[MK_BR])
                    b[x] = sRockOnGround[MK_BR];
            }
        }
        for (x = 0; x < w; x++)
        {
            if (b[x] != sRockOnSand[MK_B] || a[x] != sRockOnGround[MK_C])
                continue;
            for (x0 = x; x < w && b[x] == sRockOnSand[MK_B] && a[x] == sRockOnGround[MK_C]; x++)
                ;
            if (x0 > 0 && x < w && a[x0 - 1] == sRockOnSand[MK_L] && a[x] == sRockOnSand[MK_R] && x - x0 >= 2)
            {
                int i;
                for (i = x0; i < x; i++)
                    a[i] = 0x047C;
                a[x0] = 0x047B;
                a[x - 1] = 0x047D;
            }
        }
    }
}

// A cliff's foot right above a tree: the tree's top stands in front of the
// rock, as on Route 116 and in Lilycove (the tree's tips drawn over the
// rock's foot), not a crown cut flat against it. Where the foot is a corner
// it stays.
static void RockTips(struct Gen *gen)
{
    u8 tip[ROGUE_MAX_W];
    int w = gen->m->w, x, y;
    for (y = 1; y + 1 < gen->m->h; y++)
    {
        u16 *row = &gen->m->blocks[y * w], *up = row - w, *dn = row + w;
        YIELD();
        // 1: a short foot (tips 1F0/1F1), 2: a tall face's foot (1EE/1EF).
        for (x = 0; x < w; x++)
            tip[x] = 0;
        for (x = 0; x + 1 < w; x += 2)
        {
            u16 t0 = dn[x] & 0x3FF, t1 = dn[x + 1] & 0x3FF, u0 = up[x] & 0x3FF, u1 = up[x + 1] & 0x3FF;
            if (!((t0 == 0x1D4 || t0 == 0x1D6) && (t1 == 0x1D5 || t1 == 0x1D7)))
                continue;
            if (row[x] == 0x0479 && row[x + 1] == 0x0479 && (u0 == 0x071 || u0 == 0x07C) && (u1 == 0x071 || u1 == 0x07C))
                tip[x] = tip[x + 1] = 1;
            else if (row[x] == 0x0491 && row[x + 1] == 0x0491 && (u0 == 0x07C || u0 == 0x0A9) && (u1 == 0x07C || u1 == 0x0A9))
                tip[x] = tip[x + 1] = 2;
        }
        for (x = 0; x < w; x++)
            if (tip[x])
                row[x] = 0x0400 | ((tip[x] == 1 ? 0x1F0 : 0x1EE) + (x & 1));
    }
}

// Rock among trees comes in masses at least two units across, as Hoenn's
// cliffs do: a spike or a wall one unit thin, which no Hoenn pieces draw
// whole, stays forest. Then a tree by a cliff's corner (half sunk in its
// side, a crown against a corner of its foot, a trunk on its top corner)
// becomes rock where that rock is a mass too. Rock and trees are both
// solid, so no way opens or shuts. The map's edge and the mesa stay.
static bool8 RockMass(struct Gen *gen, const u8 *level, int ux, int uy)
{
    int dx, dy;
    for (dy = -1; dy <= 0; dy++)
        for (dx = -1; dx <= 0; dx++)
            if (MountainLevel(gen, level, ux + dx, uy + dy) && MountainLevel(gen, level, ux + dx + 1, uy + dy)
             && MountainLevel(gen, level, ux + dx, uy + dy + 1) && MountainLevel(gen, level, ux + dx + 1, uy + dy + 1))
                return TRUE;
    return FALSE;
}

static void RockInMasses(struct Gen *gen, u8 *level)
{
    int ux, uy, pass, changed;
#define ROCK_AT(x, y) (MountainLevel(gen, level, (x), (y)) >= 1)
    for (pass = 0; pass < 8; pass++)
    {
        changed = 0;
        for (uy = 0; uy < gen->ch; uy++)
        {
            YIELD();
            for (ux = 0; ux < gen->cw; ux++)
            {
                u8 *l = &level[uy * gen->cw + ux];
                if (*l && Cell(gen, ux, uy) == C_TREE && !IsEdge(gen, ux, uy) && !RockMass(gen, level, ux, uy))
                {
                    *l = 0;
                    changed++;
                }
            }
        }
        if (!changed)
            break;
    }
    for (pass = 0; pass < 4; pass++)
    {
        changed = 0;
        for (uy = 0; uy < gen->ch; uy++)
        {
            YIELD();
            for (ux = 0; ux < gen->cw; ux++)
            {
                bool8 bad = FALSE;
                u8 *l = &level[uy * gen->cw + ux];
                if (*l || Cell(gen, ux, uy) != C_TREE || IsEdge(gen, ux, uy) || PlateauSkirtU(gen, ux, uy))
                    continue;
                if (ROCK_AT(ux - 1, uy) && !ROCK_AT(ux - 1, uy - 1))
                    bad = TRUE;
                if (ROCK_AT(ux + 1, uy) && !ROCK_AT(ux + 1, uy - 1))
                    bad = TRUE;
                if (ROCK_AT(ux, uy - 1) && (!ROCK_AT(ux - 1, uy - 1) || !ROCK_AT(ux + 1, uy - 1)))
                    bad = TRUE;
                if (ROCK_AT(ux, uy + 1) && (!ROCK_AT(ux - 1, uy + 1) || !ROCK_AT(ux + 1, uy + 1)))
                    bad = TRUE;
                if (!bad)
                    continue;
                *l = 1;
                if (RockMass(gen, level, ux, uy))
                    changed++;
                else
                    *l = 0;
            }
        }
        if (!changed)
            break;
    }
#undef ROCK_AT
}

// A boulder on a flat top of rock here and there, never two side by side
// or one over another (v1.1 vert: Hoenn's stand apart).
static bool8 RockBoulderRoll(struct Gen *gen, int ux, int uy)
{
    return RogueRng_Hash(gen->rng, uy * gen->cw + ux) % 100 < 10;
}

static bool8 RockBoulderU(struct Gen *gen, int ux, int uy)
{
    return RockBoulderRoll(gen, ux, uy) && !RockBoulderRoll(gen, ux - 1, uy) && !RockBoulderRoll(gen, ux, uy - 1)
        && !RockBoulderRoll(gen, ux - 1, uy - 1) && !RockBoulderRoll(gen, ux + 1, uy - 1);
}

// (v1.1 joins) Does the hills' grove block round unit (x, y) (3x3 units)
// have open ground in it or beside it?
static bool8 GroveByOpen(struct Gen *gen, int x, int y)
{
    int bx = (x / 3) * 3, by = (y / 3) * 3, dx, dy;
    for (dy = -1; dy <= 3; dy++)
        for (dx = -1; dx <= 3; dx++)
            if (InBounds(gen, bx + dx, by + dy) && IsOpenU(Cell(gen, bx + dx, by + dy)))
                return TRUE;
    return FALSE;
}

static void RenderMountain(struct Gen *gen)
{
    u8 level[MAX_CELLS], grass[(MAX_CELLS + 7) / 8];
    int ux, uy, q, dx, dy;
    u8 l;

    // Forest and the mesa turn to rock, except lone trees out in the open.
    // Rock units are level 1, and a unit is a level higher than the lowest
    // of its eight neighbors.
    for (uy = 0; uy < gen->ch; uy++)
    {
        YIELD();
        for (ux = 0; ux < gen->cw; ux++)
        {
            YIELD();
            u8 c = Cell(gen, ux, uy);
            bool8 rock = (c == C_ROCK);
            // Hills keep groves of trees among the rock.
            // (v1.1 joins: only a grove beside open ground; one deep in
            // the rock stood in a walled pit, rings of rock round it.)
            if (c == C_TREE && gen->rockTreeChance && !IsEdge(gen, ux, uy)
             && RogueRng_Hash(gen->rng ^ 0x5A5A, (uy / 3) * 64 + ux / 3) % 100 < gen->rockTreeChance
             && GroveByOpen(gen, ux, uy))
                c = C_OPEN;
            // Hills keep the trees along an edge that meets green country,
            // so the forest there runs on across the seam instead of a
            // sliver of cliff standing against it.
            // (v1.1 vert: and a plateau's skirt of trees, rogue_gen_vert.h.)
            if (c == C_TREE && (InEdgeBand(gen, ux, uy) || PlateauSkirtU(gen, ux, uy)))
            {
                level[uy * gen->cw + ux] = 0;
                continue;
            }
            if (c == C_TREE)
            {
                for (q = 0; q < 4 && !rock; q++)
                {
                    u8 n = Cell(gen, ux + sDirDX[q], uy + sDirDY[q]);
                    if (n == C_TREE || n == C_ROCK || IsEdge(gen, ux, uy))
                        rock = TRUE;
                }
            }
            level[uy * gen->cw + ux] = rock;
        }
    }
    RockInMasses(gen, level);
    for (q = 0; q < gen->cw * gen->ch; q++)
        if (level[q])
            gen->g[q] = C_ROCK;
    // A terraced pass's rock stands above its levels (rogue_gen_rock.h).
    if (gen->terraced)
        PassRockLevels(gen, level);
    // (v1.1c land2: a mountain pass on one level rises two steps at most,
    // then a flat top, as its terraced rock does: not rings inside rings.)
    for (l = 2; l <= (gen->p->kind == RPLACE_ROUTE && gen->p->type == RROUTE_HILLS ? 2 : MOUNTAIN_LEVELS)
                && !gen->terraced; l++)
    {
        for (uy = 0; uy < gen->ch; uy++)
        {
            YIELD();
            for (ux = 0; ux < gen->cw; ux++)
            {
                YIELD();
                bool8 up = (level[uy * gen->cw + ux] == l - 1);
                for (dy = -1; dy <= 1 && up; dy++)
                    for (dx = -1; dx <= 1 && up; dx++)
                        if (MountainLevel(gen, level, ux + dx, uy + dy) < l - 1)
                            up = FALSE;
                if (up)
                    level[uy * gen->cw + ux] = l | 0x80; // marked until the pass ends
            }
        }
        for (q = 0; q < gen->cw * gen->ch; q++)
            level[q] &= 0x7F;
    }

    Render(gen);
    if (gen->rockFoot == sRockOnSand)
        GrassFootMasses(gen, level, grass);
    if (gen->terraced)
    {
        PassRenderRock(gen, level);
        RenderPassRims(gen, level);
    }
    for (uy = 0; uy < gen->ch && !gen->terraced; uy++)
    {
        YIELD();
        for (ux = 0; ux < gen->cw; ux++)
        {
            YIELD();
            u8 top = level[uy * gen->cw + ux];
            bool8 plain = TRUE;
            if (top == 0)
                continue;
            for (q = 0; q < 4; q++)
            {
                const u16 *foot = gen->rockFoot ? gen->rockFoot : sRockOnGround;
                u16 block = foot[MK_C];
                u8 k = MountainKey(gen, level, ux, uy, q, 1);
                // A desert's rock mass on grass has the grass foot.
                if (foot == sRockOnSand && (grass[(uy * gen->cw + ux) >> 3] & (1 << ((uy * gen->cw + ux) & 7))))
                    foot = sRockOnGround;
                if (k != MK_C)
                    block = foot[k];
                for (l = 2; l <= top && k == MK_C; l++)
                {
                    k = MountainKey(gen, level, ux, uy, q, l);
                    if (k != MK_C)
                        block = sRockOnRock[k];
                }
                if (k != MK_C)
                    plain = FALSE;
                Put(gen, ux * 2 + (q & 1), uy * 2 + (q >> 1), block);
            }
            // A boulder here and there on the flat tops (v1.1c land2: not
            // beside higher rock, whose edge would meet it).
            for (q = 0; q < 9 && plain; q++)
                if (q != 4 && MountainLevel(gen, level, ux + q % 3 - 1, uy + q / 3 - 1) > top)
                    plain = FALSE;
            if (plain && RockBoulderU(gen, ux, uy))
            {
                Put(gen, ux * 2, uy * 2, 0x0493);
                Put(gen, ux * 2 + 1, uy * 2, 0x0494);
                Put(gen, ux * 2, uy * 2 + 1, 0x049B);
                Put(gen, ux * 2 + 1, uy * 2 + 1, 0x049C);
            }
        }
    }
    if (gen->rockFoot == sRockOnSand)
        SandFootFaces(gen);
    if (gen->hasMesa)
        RenderMountainMouth(gen, (gen->mesa.y + gen->mesa.h) * 2 - 1);
    RockTips(gen);
    // (v1.1 vert: a mountain pass's rock joins as Hoenn's does, rogue_gen_vert.h.)
    if (gen->p->kind == RPLACE_ROUTE && gen->p->type == RROUTE_HILLS)
        RockRepair(gen);
}

// A General block standing on grass, swapped for its version on the town's
// sand or ash (see make_ground_variants.py). Those versions belong to the
// town's own tileset, so not within view of a connection.
static u16 OnTownGround(struct Gen *gen, u16 block, bool8 inSeamView)
{
    u32 i, base;
    if (gen->townGround == TGROUND_GRASS || inSeamView)
        return block;
    base = gen->townGround == TGROUND_SAND ? GROUND_VARIANT_SAND_BASE : GROUND_VARIANT_ASH_BASE;
    for (i = 0; i < GROUND_VARIANT_COUNT; i++)
        if (sGroundVariantFrom[i] == (block & 0x3FF))
            return (block & ~0x3FF) | (base + i);
    return block;
}

// A beach city stands on grass like Slateport, but Dewford's buildings and
// its rock were cut standing on sand: on the grass they take their grass
// versions (make_ground_variants.py), so no square of sand shows round a
// rock or at a house's corners. On the beach itself they keep the sand.
static u16 OnCityGrass(struct Gen *gen, u16 block, int fx, int fy)
{
    u32 i;
    if (gen->p->kind == RPLACE_TOWN && gen->p->theme == RTHEME_ISLAND && gen->p->type != RTOWN_SUMMIT)
    {
        for (i = 0; i < ISLAND_GRASS_COUNT; i++)
            if (sIslandGrass[i][0] == (block & 0x3FF))
                return (block & ~0x3FF) | sIslandGrass[i][1];
        return block;
    }
    // An island town is sand but for the grass where a route joins.
    if (!gen->islandTrees || IsBeachU(gen, fx >> 1, fy >> 1)
     || (gen->islandTown && !(gen->zoned && BitU(gen->grassZone, gen, fx >> 1, fy >> 1))))
        return block;
    for (i = 0; i < GRASS_VARIANT_COUNT; i++)
        if (sGrassVariantFrom[i] == (block & 0x3FF))
            return (block & ~0x3FF) | (GRASS_VARIANT_BASE + i);
    return block;
}

// Rustboro's buildings were cut standing on its paving. A stone town's
// lots are grass: where the ground under a building is grass it takes their
// grass versions (make_ground_variants.py), so no strip of paving shows
// under the Gym's walls, round a roof's edge or between an iron fence's
// bars. On paving they keep it.
static u16 OnStoneGrass(struct Gen *gen, u16 block, u16 under)
{
    u32 i;
    if (gen->p->kind != RPLACE_TOWN || gen->p->theme != RTHEME_STONE || gen->p->type == RTOWN_SUMMIT
     || (under & 0x3FF) >= 0x200 || (block & 0x3FF) < sStoneGrass[0][0])
        return block;
    for (i = 0; i < STONE_GRASS_COUNT; i++)
        if (sStoneGrass[i][0] == (block & 0x3FF))
            return (block & ~0x3FF) | sStoneGrass[i][1];
    return block;
}

static void Stamp(struct Gen *gen, const struct Building *b, int bx, int by)
{
    int x, y;
    bool8 seam = InSeamView(gen, bx, by, b->uw, b->uh);
    for (y = 0; y < b->sh; y++)
    {
        YIELD();
        for (x = 0; x < b->sw; x++)
        {
            u16 block;
            // (0xFFFF leaves the ground: a rock standing in the town, sHideoutRocks.)
            if (b->stamp[y * b->sw + x] == 0xFFFF)
                continue;
            block = OnTownGround(gen, b->stamp[y * b->sw + x], seam);
            block = OnCityGrass(gen, block, bx * 2 + x, by * 2 + b->sy + y);
            block = OnStoneGrass(gen, block, gen->m->blocks[(by * 2 + b->sy + y) * gen->m->w + bx * 2 + x]);
            // A corner of plain sand in the stamp (the beach gym's) is grass.
            if ((block & 0x3FF) == (B_SAND & 0x3FF) && gen->islandTrees && !gen->islandTown
             && !IsBeachU(gen, bx, by))
                block = B_GRASS;
            // Vanilla lets you walk behind some roof tops. Here that only
            // hides the player under the roof, so the top row is solid.
            if (y == 0)
                block |= 0x0400;
            Put(gen, bx * 2 + x, by * 2 + b->sy + y, block);
        }
    }
}

static void RenderSigns(struct Gen *gen)
{
    int i;
    for (i = 0; i < gen->m->nSigns; i++)
    {
        struct RogueSign *s = &gen->m->signs[i];
        if (SignDrawn(s->kind))
            Put(gen, s->x, s->y, B_SIGN);
    }
}

// ---------------------------------------------------------------------------
// Scenery shared by towns and routes

// A hill along the top of the map with a cave mouth in its south face, as
// Route 116's Rusturf Tunnel or Route 110's: one of sCaveHills that fits in
// the w units from unit x0, at a random place there. It takes the top three
// rows of units (running on up past the top of the map), and the row below
// it is kept as open ground, so trees and houses stand clear of its face.
// Returns the road cell in front of the mouth.
// The hill's units are rock, and the row below it is kept open.
static void LayCaveHill(struct Gen *gen, int x0, int hw)
{
    int x, y;
    gen->hasMesa = TRUE;
    gen->mesa.x = x0;
    gen->mesa.y = 0;
    gen->mesa.w = hw;
    gen->mesa.h = 3;
    for (y = 0; y <= 2; y++)
        for (x = x0; x < x0 + hw; x++)
            SetCell(gen, x, y, C_ROCK);
    // (A woods town keeps open ground under rock, and grows trees on kept ground.)
    for (x = x0; x < x0 + hw; x++)
        if (!IsEdge(gen, x, 3))
            SetCell(gen, x, 3, gen->forestTown ? C_OPEN : C_KEEP);
}

static void PlaceCaveHill(struct Gen *gen, int x0, int w, int *frontX, int *frontY)
{
    u8 fit[sizeof(sCaveHills) / sizeof(sCaveHills[0])];
    int n = 0, k, hw, mouth;
    for (k = 0; k < (int)(sizeof(sCaveHills) / sizeof(sCaveHills[0])); k++)
        if (sCaveHills[k].w / 2 <= w)
            fit[n++] = k;
    // (The narrowest one fits any span a mouth is placed in.)
    k = n ? fit[RAND(n)] : 0;
    hw = sCaveHills[k].w / 2;
    if (hw > w)
        hw = w;
    x0 += RAND(w - hw + 1);
    gen->caveHill = k + 1;
    LayCaveHill(gen, x0, hw);
    mouth = x0 + sCaveHills[k].mouth / 2;
    gen->mouthX = mouth * 2;
    *frontX = mouth;
    *frontY = 3;
    AddRoadCell(gen, mouth, 3);
}

// v1.1: the volcano's mountain along the top, as Mt. Chimney stands over
// Lavaridge (src/data/rogue_volcano_hill.h, tools/roguemon/gen_volcano_hill.py):
// the stretch of one of sVolcanoHills that fills units x0 to x0 + w - 1,
// with Fiery Path's doorway over a terrace and a stair down from it, at
// least three units from either end. Four units tall, its ends closed
// (the stretch's caps): at the left a west face beside the trees, with a
// round tree in front of its foot (unit x0 of the first kept row), at the
// right an east face and a strip of grass; the two rows below are kept
// open, and the stair comes down onto them. The doorway's warp
// (RWARP_DUNGEON) is added here. Returns the
// road cell below the stair, at the foot of the kept rows. Only on
// Lavaridge's tileset (a spring town, or the volcano's foot route).
static void PlaceVolcanoHill(struct Gen *gen, int x0, int w, int *frontX, int *frontY)
{
    int k = RAND(sizeof(sVolcanoHills) / sizeof(sVolcanoHills[0])), x, y, d, lo, hi;
    if (w > VOLCANO_HILL_W / 2)
    {
        // (Wider than the mountain: the units left of it are trees, so its
        // west face stands beside trees.)
        int m = (w - VOLCANO_HILL_W / 2) / 2;
        for (y = 0; y <= 5; y++)
            for (x = x0; x < x0 + m; x++)
                SetCell(gen, x, y, C_TREE);
        x0 += m;
        w = VOLCANO_HILL_W / 2;
    }
    // The stretch shown starts VOLCANO_HILL_MOUTH - 2 * d into the hill, d
    // the doorway's unit from x0: within the hill at both ends.
    lo = w - (VOLCANO_HILL_W - VOLCANO_HILL_MOUTH) / 2;
    hi = VOLCANO_HILL_MOUTH / 2;
    if (lo < 3)
        lo = 3;
    if (hi > w - 4)
        hi = w - 4;
    d = hi >= lo ? lo + RAND(hi - lo + 1) : w / 2;
    gen->caveHill = 0x40 | (k + 1);
    gen->hasMesa = TRUE;
    gen->mesa.x = x0;
    gen->mesa.y = 0;
    gen->mesa.w = w;
    gen->mesa.h = 4;
    for (y = 0; y < 4; y++)
        for (x = x0; x < x0 + w; x++)
            SetCell(gen, x, y, C_ROCK);
    // Two rows of units kept open under it: no tree's tips reach up under
    // the terrace's face, which Hoenn's red rock never stands on.
    // (A crowded town gives up the second row after many tries.)
    for (y = 4; y <= (gen->p->kind == RPLACE_TOWN && gen->attempt >= 30 ? 4 : 5); y++)
        for (x = x0; x < x0 + w; x++)
            if (!IsEdge(gen, x, y))
                SetCell(gen, x, y, gen->forestTown ? C_OPEN : C_KEEP);
    // The round tree in front of the west face's foot (its crown drawn
    // over the rock's foot by the left cap), as at Jagged Pass's foot.
    // (No meadow opened on a route may take it.)
    SetCell(gen, x0, 4, C_TREE);
    SetBitU(gen->keep, gen, x0, 4);
    gen->mouthX = (x0 + d) * 2;
    // The way from the stair runs down through both kept rows.
    *frontX = x0 + d;
    *frontY = 5;
    AddRoadCell(gen, x0 + d, 4);
    if (Cell(gen, x0 + d, 5) == C_KEEP)
        AddRoadCell(gen, x0 + d, 5);
    else
        *frontY = 4;
    AddWarp(gen, gen->mouthX, VOLCANO_HILL_DOOR_Y, RWARP_DUNGEON);
    MarkOccupied(gen, gen->mouthX, VOLCANO_HILL_DOOR_Y + 1); // where the player steps out
}

static void RenderVolcanoHill(struct Gen *gen)
{
    int k = (gen->caveHill & 0x3F) - 1;
    const u16 *t = sVolcanoHills[k];
    int c = VOLCANO_HILL_MOUTH - (gen->mouthX - gen->mesa.x * 2), x, y, w = gen->mesa.w * 2, fx0 = gen->mesa.x * 2;
    int l = (c - VOLCANO_CAP_L_FIRST) / 2, r = (c + w - VOLCANO_CAP_R_FIRST) / 2;
    for (y = 0; y < VOLCANO_HILL_H; y++)
        for (x = 0; x < w; x++)
            if (c + x >= 0 && c + x < VOLCANO_HILL_W && t[y * VOLCANO_HILL_W + c + x])
                Put(gen, fx0 + x, y, t[y * VOLCANO_HILL_W + c + x]);
    // (v1.1) Its ends closed (gen_volcano_hill.py caps): the stretch's
    // first and last columns drawn again for where it is cut. The terrace's
    // run goes on into a cap only as far as the run beside it does.
    if (l < 0 || l >= VOLCANO_CAP_L_N || r < 0 || r >= VOLCANO_CAP_R_N)
        return;
    for (y = 0; y < VOLCANO_HILL_H; y++)
    {
        const u16 *cl = &sVolcanoHillCapL[k][l][y * VOLCANO_CAP_L_W], *cr = &sVolcanoHillCapR[k][r][y * VOLCANO_CAP_R_W];
        for (x = VOLCANO_CAP_L_W - 1; x >= 0; x--)
        {
            u16 b = cl[x];
            if (!b)
                continue;
            if ((b & 0x0C00) == 0 && (gen->m->blocks[y * gen->m->w + fx0 + x + 1] & 0x0C00))
                b = (b & 0x3FF) | 0x0400;
            Put(gen, fx0 + x, y, b);
        }
        for (x = 0; x < VOLCANO_CAP_R_W; x++)
        {
            int fx = fx0 + w - VOLCANO_CAP_R_W + x;
            u16 b = cr[x];
            if ((b & 0x0C00) == 0 && (gen->m->blocks[y * gen->m->w + fx - 1] & 0x0C00))
                b = (b & 0x3FF) | 0x0400;
            Put(gen, fx, y, b);
        }
    }
}

// A cave town whose hideout's door is in the cave's own rock, a few steps
// along from the cave, as Meteor Falls and the Magma Hideout open off one
// rock wall: one of sCaveHillTwins, two mouths three units apart or more,
// the cave's either one, along the top of the town. Returns the door's
// block x (and its road cell in front), or -1 if no hill fits.
static int PlaceCaveHillTwin(struct Gen *gen, int *frontX, int *frontY, int *doorFrontX, int *doorFrontY)
{
    u8 fit[sizeof(sCaveHillTwins) / sizeof(sCaveHillTwins[0])];
    int n = 0, k, hw, x0 = 1, w = gen->cw - 2, cave, door;
    for (k = 0; k < (int)(sizeof(sCaveHillTwins) / sizeof(sCaveHillTwins[0])); k++)
        if (sCaveHillTwins[k].w / 2 <= w)
            fit[n++] = k;
    if (!n)
        return -1;
    k = fit[RAND(n)];
    hw = sCaveHillTwins[k].w / 2;
    x0 += RAND(w - hw + 1);
    gen->caveHill = 0x80 | (k + 1);
    LayCaveHill(gen, x0, hw);
    cave = sCaveHillTwins[k].mouth;
    door = sCaveHillTwins[k].mouth2;
    if (RAND(2))
    {
        cave = door;
        door = sCaveHillTwins[k].mouth;
    }
    cave = x0 + cave / 2;
    door = x0 + door / 2;
    gen->mouthX = cave * 2;
    *frontX = cave;
    *frontY = 3;
    AddRoadCell(gen, cave, 3);
    AddWarp(gen, gen->mouthX, 5, RWARP_CAVE);
    MarkOccupied(gen, gen->mouthX, 6); // where the player steps out of the cave
    AddWarp(gen, door * 2, 5, RWARP_DUNGEON);
    MarkOccupied(gen, door * 2, 6);
    AddRoadCell(gen, door, 3);
    *doorFrontX = door;
    *doorFrontY = 3;
    return door * 2;
}

// A cave mouth along the top, from unit x0, w units wide.
static void PlaceMesaAt(struct Gen *gen, int x0, int w, int *frontX, int *frontY)
{
    PlaceCaveHill(gen, x0, w, frontX, frontY);
    AddWarp(gen, gen->mouthX, 5, RWARP_CAVE);
    MarkOccupied(gen, gen->mouthX, 6); // where the player steps out of the cave
}

static void PlaceMesaW(struct Gen *gen, int w, int *frontX, int *frontY)
{
    PlaceMesaAt(gen, 1, w, frontX, frontY);
}

static void PlaceMesa(struct Gen *gen, int *frontX, int *frontY)
{
    PlaceMesaW(gen, gen->cw - 2, frontX, frontY);
}

// A rectangular lake. It may replace forest, and must touch ground that is
// already open so the player can reach the water. In towns it clears a ring of
// open ground around itself. On routes it sits in the forest beside the road,
// so it does not open up a new lawn. The row of units below it becomes shore.
static bool8 PlaceLake(struct Gen *gen, int w, int h, bool8 clearRing)
{
    int tries, x, y;
    for (tries = 0; tries < 60; tries++)
    {
        int rx = 2 + RAND(gen->cw - 3 - w);
        int ry = 2 + RAND(gen->ch - 4 - h);
        bool8 ok = TRUE, touchesOpen = FALSE;

        if (rx + w > gen->cw - 2 || ry + h > gen->ch - 3)
            continue;
        // No ponds in the ash: their grassy banks would look wrong on it.
        if (AshZoneU(gen, rx, ry) || AshZoneU(gen, rx + w - 1, ry + h - 1)
         || AshZoneU(gen, rx + w - 1, ry) || AshZoneU(gen, rx, ry + h - 1))
            continue;
        for (y = ry - 1; y <= ry + h + 1 && ok; y++)
        {
            for (x = rx - 1; x <= rx + w && ok; x++)
            {
                u8 c = Cell(gen, x, y);
                bool8 inside = (x >= rx && x < rx + w && y >= ry && y <= ry + h);
                if (inside && c != C_TREE && c != C_OPEN)
                    ok = FALSE;
                else if (c == C_WATER || c == C_SHORE || c == C_ROCK || c == C_BLDG || c == C_KEEP || c == C_SEA || c == C_CLIFF
                      || IsBeachU(gen, x, y))
                    ok = FALSE;
                // A pond keeps to one level of a terraced town.
                if (gen->terraced && (UpperU(gen, x, y) != UpperU(gen, rx, ry) || RimAbove(gen, x, y)))
                    ok = FALSE;
                if (!inside && (c == C_OPEN || c == C_ROAD))
                    touchesOpen = TRUE;
            }
        }
        if (!ok || !touchesOpen)
            continue;
        if (!clearRing)
        {
            // The shore row must open onto ground that is already open, so
            // the player can stand on it and fish.
            bool8 shoreOpen = FALSE;
            for (x = rx; x < rx + w; x++)
                if (IsOpenU(Cell(gen, x, ry + h + 1)) || IsOpenU(Cell(gen, x, ry + h)))
                    shoreOpen = TRUE;
            if (IsOpenU(Cell(gen, rx - 1, ry + h)) || IsOpenU(Cell(gen, rx + w, ry + h)))
                shoreOpen = TRUE;
            if (!shoreOpen)
                continue;
        }
        for (y = ry - 1; y <= ry + h + 1 && clearRing; y++)
            for (x = rx - 1; x <= rx + w; x++)
                if (Cell(gen, x, y) == C_TREE && !IsEdge(gen, x, y))
                    SetCell(gen, x, y, C_OPEN);
        for (y = ry; y < ry + h; y++)
            for (x = rx; x < rx + w; x++)
                SetCell(gen, x, y, C_WATER);
        for (x = rx; x < rx + w; x++)
            SetCell(gen, x, ry + h, C_SHORE);
        return TRUE;
    }
    return FALSE;
}

// One-way ledges: a row of ledge across open ground, ending at a tree line on
// one side and at the road on the other, so the road is the gap you walk
// around by and the ledge is a shortcut down. A ledge is only kept if
// everything that could be reached before can still be reached without
// crossing it, so jumping down can never trap the player.
static void PlaceLedges(struct Gen *gen, int startX, int startY, int count)
{
    int tries, placed = 0;
    for (tries = 0; tries < 150 && placed < count; tries++)
    {
        int y = 2 + RAND(gen->ch - 4);
        YIELD();
        int x = 1 + RAND(gen->cw - 2);
        int x0 = x, x1 = x, cx, before;
        bool8 ok = TRUE;

        if (Cell(gen, x, y) != C_OPEN || AshZoneU(gen, x, y))
            continue;
        while (Cell(gen, x0 - 1, y) == C_OPEN)
            x0--;
        while (Cell(gen, x1 + 1, y) == C_OPEN)
            x1++;
        if (x1 - x0 + 1 > 8 || x1 - x0 + 1 < 3)
            continue;
        if ((IsOpenU(Cell(gen, x0 - 1, y)) && Cell(gen, x0 - 1, y) != C_ROAD)
         || (IsOpenU(Cell(gen, x1 + 1, y)) && Cell(gen, x1 + 1, y) != C_ROAD))
            continue;
        for (cx = x0; cx <= x1 && ok; cx++)
        {
            u8 above = Cell(gen, cx, y - 1), below = Cell(gen, cx, y + 1);
            if ((above != C_OPEN && above != C_ROAD) || (below != C_OPEN && below != C_ROAD) || AshZoneU(gen, cx, y))
                ok = FALSE;
        }
        if (HasNeighbor(gen, x0 - 1, y, C_WATER, TRUE) || HasNeighbor(gen, x1 + 1, y, C_WATER, TRUE)
         || HasNeighbor(gen, x0 - 1, y, C_SHORE, TRUE) || HasNeighbor(gen, x1 + 1, y, C_SHORE, TRUE))
            ok = FALSE;
        if (!ok)
            continue;
        before = FloodReachable(gen, startX, startY);
        for (cx = x0; cx <= x1; cx++)
            SetCell(gen, cx, y, C_LEDGE);
        if (FloodReachable(gen, startX, startY) != before)
        {
            for (cx = x0; cx <= x1; cx++)
                SetCell(gen, cx, y, C_OPEN);
            continue;
        }
        placed++;
    }
}

// Two berry trees on a soil patch, backed against trees or rock like the
// vanilla ones. The patch is four blocks wide and takes two units.
static void PlaceBerryPatch(struct Gen *gen)
{
    int tries;
    for (tries = 0; tries < 140; tries++)
    {
        int x = 1 + RAND(gen->cw - 3), y = 2 + RAND(gen->ch - 4);
        if (Cell(gen, x, y) != C_OPEN || Cell(gen, x + 1, y) != C_OPEN)
            continue;
        // Soft soil in a ring of grass: not in the ash.
        if (AshZoneU(gen, x, y) || AshZoneU(gen, x + 1, y) || AshZoneU(gen, x, y + 1) || AshZoneU(gen, x + 1, y + 1))
            continue;
        if (IsOpenU(Cell(gen, x, y - 1)) || IsOpenU(Cell(gen, x + 1, y - 1)))
            continue;
        if (Cell(gen, x, y - 1) == C_WATER || Cell(gen, x + 1, y - 1) == C_WATER
         || Cell(gen, x, y - 1) == C_SEA || Cell(gen, x + 1, y - 1) == C_SEA)
            continue;
        if (Cell(gen, x, y + 1) != C_OPEN && Cell(gen, x, y + 1) != C_ROAD)
            continue;
        if (Cell(gen, x + 1, y + 1) != C_OPEN && Cell(gen, x + 1, y + 1) != C_ROAD)
            continue;
        if (SeenFromNextDoor(gen, x * 2 + 1, y * 2) || SeenFromNextDoor(gen, x * 2 + 2, y * 2))
            continue;
        SetCell(gen, x, y, C_KEEP);
        SetCell(gen, x + 1, y, C_KEEP);
        gen->hasBerry = TRUE;
        gen->berryX = x * 2;
        gen->berryY = y * 2;
        return;
    }
}

// ---------------------------------------------------------------------------
// Towns

struct Placed
{
    const struct Building *b;
    int bx, by;
    u8 door; // enum RogueWarpKind or DOOR_LOCKED, may differ from b->door
};

// True if every block of a stamp is from the primary tileset, which looks the
// same with any town tileset.
static bool8 StampIsPrimary(const u16 *stamp, int n)
{
    int i;
    for (i = 0; i < n; i++)
        if ((stamp[i] & 0x3FF) >= 0x200 && stamp[i] != 0xFFFF)
            return FALSE;
    return TRUE;
}

// A connected map is drawn with this map's tileset. Standing just across a
// connection the player sees about 7 blocks into this map, and the route
// next door uses a different secondary tileset, so theme tiles there would
// show as garbage. True if a rectangle of units falls in that view.
static bool8 InSeamView(struct Gen *gen, int ux, int uy, int uw, int uh)
{
    u8 d;
    for (d = 0; d < 4; d++)
    {
        int p;
        if (!gen->keepOut[d] || gen->exitPos[d] == 0xFF)
            continue;
        p = gen->exitPos[d] / 2;
        switch (d)
        {
        case RDIR_NORTH:
            if (uy < 3 && ux + uw > p - 5 && ux <= p + 5)
                return TRUE;
            break;
        case RDIR_SOUTH:
            if (uy + uh > gen->ch - 3 && ux + uw > p - 5 && ux <= p + 5)
                return TRUE;
            break;
        case RDIR_WEST:
            if (ux < 4 && uy + uh > p - 4 && uy <= p + 4)
                return TRUE;
            break;
        default:
            if (ux + uw > gen->cw - 4 && uy + uh > p - 4 && uy <= p + 4)
                return TRUE;
            break;
        }
    }
    return FALSE;
}

// A building's lot in that view: none may stand there, unless the town
// has failed to fit its buildings for 40 tries.
static bool8 LotInSeamView(struct Gen *gen, int ux, int uy, int uw, int uh)
{
    return !gen->seamLots && InSeamView(gen, ux, uy, uw, uh);
}

// Breadth-first search from (sx, sy) over open ground to the nearest road
// cell, then carve the way as road. Connects building doors to the town roads.
static bool8 ConnectToRoad(struct Gen *gen, int sx, int sy)
{
    s16 *prev = sBfsPrev;
    u16 *queue = sBfsQueue;
    int head = 0, tail = 0, i, found = -1;

    for (i = 0; i < gen->cw * gen->ch; i++)
        prev[i] = -2;
    if (Cell(gen, sx, sy) == C_ROAD)
        return TRUE;
    queue[tail++] = PACK(sx, sy);
    prev[sy * gen->cw + sx] = -1;
    while (head < tail && found < 0)
    {
        int cur = queue[head++];
        int cx = PX(cur), cy = PY(cur);
        u8 d;
        YIELD();
        for (d = 0; d < 4; d++)
        {
            int nx = cx + sDirDX[d], ny = cy + sDirDY[d];
            int ni = ny * gen->cw + nx;
            u8 c;
            if (!InBounds(gen, nx, ny) || prev[ni] != -2 || IsEdge(gen, nx, ny))
                continue;
            if (gen->terraced && !TerraceStep(gen, cx, cy, nx, ny, d))
                continue;
            c = Cell(gen, nx, ny);
            if (c == C_ROAD)
            {
                prev[ni] = cur;
                found = ni;
                break;
            }
            if (c != C_OPEN)
                continue;
            prev[ni] = cur;
            queue[tail++] = PACK(nx, ny);
        }
    }
    if (found < 0)
        return FALSE;
    // prev holds packed coordinates, -1 at the start.
    for (i = prev[found]; i >= 0; i = prev[PY(i) * gen->cw + PX(i)])
        AddRoadCell(gen, PX(i), PY(i));
    return TRUE;
}

// A building's stamp may leave a column of blocks free on its right, or a
// row free along its top: how many blocks of ground it shows on that side.
static int RightSlack(const struct Building *b)
{
    return b->uw * 2 - b->sw;
}

// The building whose footprint covers unit (x, y), among those the town
// has placed so far, or NULL.
static const struct Building *LotAt(struct Gen *gen, int x, int y)
{
    int i;
    for (i = 0; gen->lots && i < MAX_LOTS; i++)
    {
        const struct Placed *p = &gen->lots[i];
        if (p->b && x >= p->bx && x < p->bx + p->b->uw && y >= p->by && y < p->by + p->b->uh)
            return p->b;
    }
    return NULL;
}

// The ring of units round a lot of uw x uh units. As in Hoenn's towns, a
// building keeps ground on every side: it never stands wall to wall with
// another or with a fence, nor under a rock face or a terrace's rim, nor with
// its walls or roof in the water. On a route drawn as rock, the trees round
// it are rock faces too. A side where the stamp itself leaves a block of
// ground (top: its first row; right: its last column), or the neighbour's
// on the left, may come up to it.
static bool8 RingClear(struct Gen *gen, int bx, int by, int uw, int uh, bool8 topSlack, bool8 rightSlack, bool8 wetOk)
{
    int x, y;
    bool8 rockTrees = gen->p->kind == RPLACE_ROUTE && (gen->rocky || gen->p->type == RROUTE_MOUNTAIN);
    for (y = by - 1; y <= by + uh; y++)
    {
        for (x = bx - 1; x <= bx + uw; x++)
        {
            u8 c;
            bool8 side = y >= by && y < by + uh, top = y == by - 1 && x >= bx && x < bx + uw;
            const struct Building *n;
            if (x >= bx && x < bx + uw && y >= by && y < by + uh)
                continue;
            c = Cell(gen, x, y);
            if (c != C_BLDG && c != C_DECOR && c != C_WATER && c != C_SHORE && c != C_SEA && c != C_FORD
             && c != C_ROCK && c != C_CLIFF && !(rockTrees && c == C_TREE && !IsEdge(gen, x, y)))
                continue;
            if (wetOk && (c == C_WATER || c == C_SHORE || c == C_SEA || c == C_FORD))
                continue;
            // Rock may wrap round a corner of it.
            if (!side && (x == bx - 1 || x == bx + uw) && (c == C_ROCK || c == C_CLIFF || c == C_TREE))
                continue;
            if ((top && topSlack) || (side && x == bx + uw && rightSlack))
                continue;
            if (side && x == bx - 1 && c == C_BLDG && (n = LotAt(gen, x, y)) != NULL && RightSlack(n) > 0)
                continue;
            return FALSE;
        }
    }
    return TRUE;
}

static bool8 LotRingClear(struct Gen *gen, const struct Building *b, int bx, int by)
{
    return RingClear(gen, bx, by, b->uw, b->uh, b->sy > 0, RightSlack(b) > 0, FALSE);
}

// The same for a fenced lot (a flower bed, a pen): its fence keeps clear of
// buildings' walls and roofs and of rock faces above it.
static bool8 FenceRingClear(struct Gen *gen, int ux, int uy, int uw, int uh)
{
    int x, y;
    for (y = uy - 1; y <= uy + uh; y++)
    {
        for (x = ux - 1; x <= ux + uw; x++)
        {
            u8 c = Cell(gen, x, y);
            if (c == C_BLDG || c == C_ROCK || c == C_CLIFF)
                return FALSE;
        }
    }
    return TRUE;
}

// Water, a shore or a ford in the ring round a lot.
static bool8 HasWaterRing(struct Gen *gen, int ux, int uy, int uw, int uh)
{
    int x, y;
    for (y = uy - 1; y <= uy + uh; y++)
        for (x = ux - 1; x <= ux + uw; x++)
        {
            u8 c = Cell(gen, x, y);
            if (c == C_WATER || c == C_SHORE || c == C_SEA || c == C_FORD)
                return TRUE;
        }
    return FALSE;
}

static bool8 PlaceBuilding(struct Gen *gen, const struct Building *b, struct Placed *out)
{
    int tries;
    for (tries = 0; tries < 100; tries++)
    {
        YIELD();
        int bx = 1 + RAND(gen->cw - 1 - b->uw);
        int by = 1 + RAND(gen->ch - 2 - b->uh);
        int fx = bx + b->doorX / 2, fy = by + b->uh;
        int x, y;
        bool8 ok = TRUE;

        if (fy >= gen->ch - 1)
            continue;
        // The footprint must be plain open ground with a clear ring round it.
        for (y = by; y < by + b->uh && ok; y++)
            for (x = bx; x < bx + b->uw && ok; x++)
                if (Cell(gen, x, y) != C_OPEN)
                    ok = FALSE;
        if (!ok || !LotRingClear(gen, b, bx, by))
            continue;
        if (Cell(gen, fx, fy) != C_OPEN && Cell(gen, fx, fy) != C_ROAD)
            continue;
        if (!StampIsPrimary(b->stamp, b->sw * b->sh) && LotInSeamView(gen, bx, by, b->uw, b->uh))
            continue;
        for (y = by; y < by + b->uh; y++)
            for (x = bx; x < bx + b->uw; x++)
                SetCell(gen, x, y, C_BLDG);
        if (!ConnectToRoad(gen, fx, fy))
        {
            for (y = by; y < by + b->uh; y++)
                for (x = bx; x < bx + b->uw; x++)
                    SetCell(gen, x, y, C_OPEN);
            continue;
        }
        AddRoadCell(gen, fx, fy);
        out->b = b;
        out->bx = bx;
        out->by = by;
        out->door = b->door;
        return TRUE;
    }
    return FALSE;
}

// What is inside a building you can enter, or the sign on a landmark that
// stays shut (0xFF for an ordinary locked door).
#define INTERIOR_NONE 0xFF
static u8 InteriorOf(const struct Building *b)
{
    if (b->stamp == sStamp_Lab)
        return RINT_LAB;
    if (b->stamp == sThemeStone_PokemonSchool)
        return RINT_SCHOOL;
    if (b->stamp == sThemeVerdant_BikeShop)
        return RINT_BIKE_SHOP;
    if (b->stamp == sThemeHarbor_FanClub || b->stamp == sThemeCove_FanClub)
        return RINT_FAN_CLUB;
    if (b->stamp == sThemeHarbor_NameRatersHouse)
        return RINT_NAME_RATER;
    if (b->stamp == sThemeCove_BlueHouse2 || b->stamp == sThemeAsh_CozmosHouse)
        return RINT_MOVE_DELETER;
    if (b->stamp == sThemeBeach_TownHall)
        return RINT_NAME_RATER;
    if (b->door == DOOR_LOCKED)
        return INTERIOR_NONE;
    return RINT_HOME;
}

static u8 LandmarkSign(const struct Building *b)
{
    if (b->stamp == sThemeVerdant_GameCorner || b->stamp == sThemeIsland_GameCorner)
        return RSIGN_GAME_CORNER;
    if (b->stamp == sThemeHarbor_BattleTent)
        return RSIGN_BATTLE_TENT;
    if (b->stamp == sThemeCove_CoveLilyMotel)
        return RSIGN_MOTEL;
    return RSIGN_LOCKED;
}

// The sign naming the town's house-th house if it is special, or
// RSIGN_HOUSE for an ordinary home.
static u8 HouseSignOf(struct Gen *gen, int house)
{
    switch (gen->m->interiors[house])
    {
    case RINT_LAB:          return RSIGN_LAB;
    case RINT_SCHOOL:       return RSIGN_SCHOOL;
    case RINT_FAN_CLUB:     return RSIGN_FAN_CLUB;
    case RINT_BIKE_SHOP:    return RSIGN_BIKE_SHOP;
    case RINT_NAME_RATER:   return RSIGN_NAME_RATER;
    case RINT_MOVE_DELETER: return RSIGN_MOVE_DELETER;
    case RINT_SIGNATURE:    return RSIGN_LANDMARK;
    }
    switch (RogueGen_HomeRole(gen->r, gen->m->place, gen->m->interiors, gen->m->nHouses, house))
    {
    case RHOME_GIFT:        return RSIGN_GIFT;
    case RHOME_RELEARNER:   return RSIGN_TUTOR;
    case RHOME_NAME_RATER:  return RSIGN_NAME_RATER;
    }
    return RSIGN_HOUSE;
}

static bool8 InLot(const struct Placed *p, int fx, int fy)
{
    int lx = fx - p->bx * 2, ly = fy - p->by * 2;
    return lx >= 0 && ly >= 0 && lx < p->b->uw * 2 && ly < p->b->uh * 2;
}

// TRUE on a block of a building's lot that its stamp leaves bare, beside or
// above it.
static bool8 InBareLot(const struct Placed *p, int fx, int fy)
{
    const struct Building *b = p->b;
    int lx = fx - p->bx * 2, ly = fy - p->by * 2;
    if (!InLot(p, fx, fy))
        return FALSE;
    return lx >= b->sw || ly < b->sy || ly >= b->sy + b->sh;
}

// A block the player can reach and read a sign from (FloodReachable first).
// Where the flood does not reach the door either (it does not climb some
// terraces), open ground is enough.
static bool8 ReadableFrom(struct Gen *gen, const struct Placed *p, int fx, int fy)
{
    int doorX = p->bx * 2 + p->b->doorX, frontY = p->by * 2 + p->b->sy + FrontY(p->b);
    return UnitWalkF(gen, fx, fy) && (Reached(gen, fx >> 1, fy >> 1) || !Reached(gen, doorX >> 1, frontY >> 1));
}

// Where a special house's sign may stand: open ground off the path, as
// Emerald's stand by the Move Tutor's and the Name Rater's doors. Not in a
// notch of the path, on a ledge's landing, in front of a door, where it would
// close a gap, or where nobody can stand beside it to read it. For a house
// with no bare ground beside it, onRoad 1 lets it stand on paving at the edge
// of a street that goes on below it, as Mauville's and Rustboro's do, and 2
// on a path with ground below, as by a treehouse's ladder.
static bool8 HouseSignFits(struct Gen *gen, const struct Placed *p, int fx, int fy, u8 onRoad)
{
    bool8 pn, ps, pw, pe;
    int i;
    if (!InMap(gen, fx, fy) || !InMap(gen, fx, fy + 1) || IsOccupied(gen, fx, fy))
        return FALSE;
    if (!(Cell(gen, fx >> 1, fy >> 1) == C_OPEN || Cell(gen, fx >> 1, fy >> 1) == C_ROAD || InBareLot(p, fx, fy))
     || ByLedgeF(gen, fx, fy) || BlocksWay(gen, fx, fy))
        return FALSE;
    if (UnderCrown(gen, fx, fy))
        return FALSE;
    if (!ReadableFrom(gen, p, fx, fy + 1) && !ReadableFrom(gen, p, fx, fy - 1) && !ReadableFrom(gen, p, fx - 1, fy) && !ReadableFrom(gen, p, fx + 1, fy))
        return FALSE;
    for (i = 0; i < gen->m->nWarps; i++)
        if (gen->m->warps[i].x == fx && gen->m->warps[i].y == fy - 1)
            return FALSE;
    // Not under another building's wall, where it would name that one.
    if (fy > 0 && Cell(gen, fx >> 1, (fy - 1) >> 1) == C_BLDG && !InLot(p, fx, fy - 1))
        return FALSE;
    if (onRoad)
        return PathCoverF(gen, fx, fy) && UnitWalkF(gen, fx, fy + 1) && (onRoad > 1 || PathCoverF(gen, fx, fy + 1));
    pn = PathCoverF(gen, fx, fy - 1);
    ps = PathCoverF(gen, fx, fy + 1);
    pw = PathCoverF(gen, fx - 1, fy);
    pe = PathCoverF(gen, fx + 1, fy);
    return !(PathCoverF(gen, fx, fy) || (pn && ps) || (pw && pe) || pn + ps + pw + pe >= 3);
}

// A sign beside a special house's door: on the door row just beside the
// wall, nearest side first, or on the row in front a step or two to the
// side of the door (raised doors, on treehouses, only take that row), or a
// row lower. Where there is no bare ground there, it stands on the street,
// two steps or one to the side, or last of all on the path beside the door.
static void AddHouseSign(struct Gen *gen, const struct Placed *p, u8 kind)
{
    // x from the door (or, with WALL, out from the wall on the door's side),
    // y from the row in front of the door, and how far onto the path.
    #define WALL 16
    static const s8 sSpots[][3] =
    {
        { WALL + 1, -1, 0 }, { -WALL - 1, -1, 0 },
        { 1, 0, 0 }, { -1, 0, 0 }, { 2, 0, 0 }, { -2, 0, 0 },
        { 1, 1, 0 }, { -1, 1, 0 }, { 2, 1, 0 }, { -2, 1, 0 },
        { WALL + 2, -1, 0 }, { -WALL - 2, -1, 0 },
        { 2, 0, 1 }, { -2, 0, 1 }, { 1, 0, 1 }, { -1, 0, 1 },
        { 1, 0, 2 }, { -1, 0, 2 }, { 2, 0, 2 }, { -2, 0, 2 },
    };
    const struct Building *b = p->b;
    int sx = p->bx * 2, sy = p->by * 2 + b->sy;
    int doorX = sx + b->doorX, fy = sy + FrontY(b);
    int side = (b->doorX * 2 < b->sw) ? -1 : 1, wallSide = side, i;
    int uy = (sy + b->doorY) >> 1;

    // Beside the wall the sign takes the side with no other house just
    // across the gap, where it could name that one.
    if (Cell(gen, side < 0 ? p->bx - 2 : p->bx + b->uw + 1, uy) == C_BLDG
     && Cell(gen, side < 0 ? p->bx + b->uw + 1 : p->bx - 2, uy) != C_BLDG)
        wallSide = -side;
    for (i = 0; i < (int)(sizeof(sSpots) / sizeof(sSpots[0])); i++)
    {
        int dx = sSpots[i][0] * side, x, y = fy + sSpots[i][1];
        if (dx >= WALL || dx <= -WALL)
        {
            dx = sSpots[i][0] * wallSide;
            // Beside the wall, on the door row: only for doors at ground level.
            if (b->frontY)
                continue;
            x = dx < 0 ? sx - (-dx - WALL) : sx + b->sw - 1 + (dx - WALL);
        }
        else
        {
            x = doorX + dx;
        }
        if (HouseSignFits(gen, p, x, y, sSpots[i][2]))
        {
            AddSign(gen, x, y, kind);
            return;
        }
    }
    #undef WALL
}

// TRUE if the town already has this special interior.
static bool8 HasInterior(struct Gen *gen, u8 interior)
{
    int i;
    for (i = 0; i < gen->m->nHouses; i++)
        if (gen->m->interiors[i] == interior)
            return TRUE;
    return FALSE;
}

// A landmark that stays shut (a Game Corner, a tent: its sign stands on its
// door, AddBuildingEvents) is read from below: its door is solid, so nobody
// walks into the doorway and stands there.
static void ShutLandmarkDoor(struct Gen *gen, const struct Placed *p)
{
    int x = p->bx * 2 + p->b->doorX, y = p->by * 2 + p->b->sy + p->b->doorY;
    u32 i;
    for (i = 0; i < gen->m->nSigns; i++)
        if (gen->m->signs[i].x == x && gen->m->signs[i].y == y && gen->m->signs[i].kind == LandmarkSign(p->b))
            gen->m->blocks[y * gen->m->w + x] |= 0x0400;
}

// Adds a building's door and signs. Returns the house number when it opens
// onto the town's house map, else -1; its sign, if it gets one, comes later
// (AddHouseSigns).
static int AddBuildingEvents(struct Gen *gen, const struct Placed *p)
{
    const struct Building *b = p->b;
    int sx = p->bx * 2, sy = p->by * 2 + b->sy;
    int doorX = sx + b->doorX, doorY = sy + b->doorY;
    struct RogueMap *m = gen->m;
    u8 interior = InteriorOf(b);
    int house = -1;

    // Each special interior belongs to one town in the region (see
    // ThemeInteriors), once: a second building like it in the town is an
    // ordinary home too. Elsewhere it becomes an ordinary home, or stays shut.
    if (interior != RINT_HOME && interior != INTERIOR_NONE
     && (!(gen->p->interiors & (1 << interior)) || HasInterior(gen, interior)))
        interior = (b->door == DOOR_LOCKED) ? INTERIOR_NONE : RINT_HOME;
    // Houses, the lab and some landmarks open onto the town's house map.
    if ((p->door == RWARP_HOUSE || p->door == DOOR_LAB || p->door == DOOR_LOCKED)
     && interior != INTERIOR_NONE && m->nHouses < ROGUE_MAX_HOUSES)
    {
        AddWarp(gen, doorX, doorY, ROGUE_HOUSE_WARP(m->nHouses));
        house = m->nHouses;
        m->interiors[m->nHouses++] = interior;
    }
    else switch (p->door)
    {
    case RWARP_PC:
        AddWarp(gen, doorX, doorY, p->door);
        AddSign(gen, sx + 2, sy + 3, RSIGN_PC);
        AddSign(gen, sx + 3, sy + 3, RSIGN_PC);
        break;
    case RWARP_MART:
        AddWarp(gen, doorX, doorY, p->door);
        AddSign(gen, sx + 2, sy + 3, RSIGN_MART);
        AddSign(gen, sx + 3, sy + 3, RSIGN_MART);
        break;
    case RWARP_GYM:
        AddWarp(gen, doorX, doorY, p->door);
        AddSign(gen, sx + b->doorX + 1, sy + b->doorY, RSIGN_GYM);
        break;
    case DOOR_SIGLOT:
        // A lot along the street (src/rogue_gen_signatures.h): no door.
        return -1;
    case DOOR_SIGNATURE:
        // The town's landmark, onto its own interior.
        if (m->nHouses < ROGUE_MAX_HOUSES)
        {
            AddWarp(gen, doorX, doorY, ROGUE_HOUSE_WARP(m->nHouses));
            house = m->nHouses;
            m->interiors[m->nHouses++] = RINT_SIGNATURE;
        }
        break;
    case DOOR_DUNGEON:
        AddWarp(gen, doorX, doorY, RWARP_DUNGEON);
        AddHouseSign(gen, p, RSIGN_DUNGEON);
        break;
    default:
        AddSign(gen, doorX, doorY, LandmarkSign(b));
        break;
    }
    // A bare patch in front of the door, where there is no road.
    if (InMap(gen, doorX, sy + FrontY(b)) && Cover(gen, doorX, sy + FrontY(b)) == V_NONE)
        SetCover(gen, doorX, sy + FrontY(b), V_MAT);
    // Flowers by the front wall.
    if (CHANCE(60))
    {
        int x, fy = sy + FrontY(b);
        for (x = sx - 1; x <= sx + b->sw; x++)
        {
            if (x == doorX || !CHANCE(45))
                continue;
            if (CanCover(gen, x, fy) && Cover(gen, x, fy) == V_NONE)
                SetCover(gen, x, fy, V_FLOWER);
        }
    }
    return house;
}

// Once every house has its interior: the signs beside houses, as few as
// Emerald has. A house Emerald names (the lab, the Name Rater's, the Move
// Deleter's, the tutor's, the Cycle Shop, the Fan Club, the school) keeps
// its sign, the Name Rater's first since every region needs his; at most
// MAX_HOUSE_SIGNS a town. Ordinary homes (the gift's too) have none, except
// that one big house a town may keep its family's plate on the door row
// (signX), as Littleroot's houses do, while there is room under the cap.
// A signature landmark's sign is not a house sign and always stands.
// A family plate the game reads by the nearest door (RogueText_SignHouse)
// stands only where that door is its house's alone: not between its door
// and a neighbour's, where it would show the neighbour's family.
#define MAX_HOUSE_SIGNS 2
static bool8 PutHouseSign(struct Gen *gen, const struct Placed *p, int house, u8 sign)
{
    u32 before = gen->m->nSigns;
    if (p->b->signX)
    {
        int x = p->bx * 2 + p->b->signX, y = p->by * 2 + p->b->sy + p->b->doorY;
        if ((sign == RSIGN_HOUSE || sign == RSIGN_GIFT) && RogueText_SignHouse(gen->m, x, y) != house)
            return FALSE;
        AddSign(gen, x, y, sign);
    }
    else
    {
        AddHouseSign(gen, p, sign);
    }
    return gen->m->nSigns != before;
}

static void AddHouseSigns(struct Gen *gen, const struct Placed *placed, const s8 *houseOf)
{
    static const u8 sOrder[] =
    {
        RSIGN_NAME_RATER, RSIGN_LAB, RSIGN_MOVE_DELETER, RSIGN_TUTOR,
        RSIGN_BIKE_SHOP, RSIGN_FAN_CLUB, RSIGN_SCHOOL, RSIGN_HOUSE,
    };
    int k, i, n = 0;
    bool8 plate = FALSE;
    for (k = 0; k < gen->m->nHouses; k++)
    {
        if (HouseSignOf(gen, k) == RSIGN_LANDMARK)
            PutHouseSign(gen, &placed[houseOf[k]], k, RSIGN_LANDMARK);
    }
    for (i = 0; i < (int)(sizeof(sOrder) / sizeof(sOrder[0])); i++)
    {
        for (k = 0; k < gen->m->nHouses && n < MAX_HOUSE_SIGNS; k++)
        {
            const struct Placed *p = &placed[houseOf[k]];
            u8 sign = HouseSignOf(gen, k);
            // (A gift home's plate is a home's plate with a welcome.)
            if ((sign == RSIGN_GIFT ? RSIGN_HOUSE : sign) != sOrder[i])
                continue;
            if (sOrder[i] == RSIGN_HOUSE)
            {
                if (plate || !p->b->signX)
                    continue;
                // (A big house whose plate would stand by a neighbour's
                // door leaves it to the next.)
                plate = PutHouseSign(gen, p, k, sign);
                n += plate;
                continue;
            }
            n += PutHouseSign(gen, p, k, sign);
        }
    }
}
#undef MAX_HOUSE_SIGNS

static void AddBlockade(struct Gen *gen, u8 dir); // src/rogue_gen_hideout.h

static void AddGate(struct Gen *gen, u8 dir, u8 need)
{
    struct RogueMap *m = gen->m;
    int pos = gen->exitPos[dir] / 2, x, y, i;
    EdgeCell(gen, dir, pos, 0, &x, &y);
    for (i = 0; i < 2 && m->nGates < ROGUE_MAX_GATES; i++)
    {
        struct RogueGate *g = &m->gates[m->nGates++];
        switch (dir)
        {
        case RDIR_NORTH: g->x = x * 2 + i; g->y = 1; break;
        case RDIR_SOUTH: g->x = x * 2 + i; g->y = y * 2; break;
        case RDIR_WEST:  g->x = 1; g->y = y * 2 + i; break;
        default:         g->x = x * 2; g->y = y * 2 + i; break;
        }
        g->dir = dir;
        g->need = need;
    }
}

// Ragged tree line inside the border, and the odd tree in the open.
static void GrowTownTrees(struct Gen *gen)
{
    int x, y;
    for (y = 1; y < gen->ch - 1; y++)
    {
        for (x = 1; x < gen->cw - 1; x++)
        {
            // Not under a rock face, which would hide the tree's tips.
            if (Cell(gen, x, y) != C_OPEN || Cell(gen, x, y - 1) == C_ROCK || RimAbove(gen, x, y)
             || (gen->townGround != TGROUND_GRASS && Cell(gen, x, y - 1) == C_DECOR))
                continue;
            if (HasNeighbor(gen, x, y, C_ROAD, FALSE) || HasNeighbor(gen, x, y, C_BLDG, TRUE)
             || HasNeighbor(gen, x, y, C_WATER, TRUE) || HasNeighbor(gen, x, y, C_SHORE, TRUE)
             || HasNeighbor(gen, x, y, C_SEA, TRUE) || IsBeachU(gen, x, y))
                continue;
            // Island trees stand in groves, never alone in the sand.
            if (HasNeighbor(gen, x, y, C_TREE, FALSE) ? CHANCE(45) : (gen->townGround != TGROUND_SAND && CHANCE(5)))
                SetCell(gen, x, y, C_TREE);
        }
    }
}

// Place a building with its door opening straight onto the street below it,
// so the fronts line up like a real town. Returns FALSE if the lot is taken.
static bool8 PlaceBuildingAt(struct Gen *gen, const struct Building *b, int bx, int by, struct Placed *out)
{
    int x, y, fx = bx + b->doorX / 2, fy = by + b->uh;
    if (bx < 1 || by < 1 || bx + b->uw > gen->cw - 1)
        return FALSE;
    if (Cell(gen, fx, fy) != C_ROAD)
        return FALSE;
    for (y = by; y < by + b->uh; y++)
        for (x = bx; x < bx + b->uw; x++)
            if (Cell(gen, x, y) != C_OPEN)
                return FALSE;
    if (!LotRingClear(gen, b, bx, by))
        return FALSE;
    if (!StampIsPrimary(b->stamp, b->sw * b->sh) && LotInSeamView(gen, bx, by, b->uw, b->uh))
        return FALSE;
    for (y = by; y < by + b->uh; y++)
        for (x = bx; x < bx + b->uw; x++)
            SetCell(gen, x, y, C_BLDG);
    out->b = b;
    out->bx = bx;
    out->by = by;
    out->door = b->door;
    return TRUE;
}

// Something a row of buildings ends against: a building, a lane, trees or
// the edge.
static bool8 RowEndAt(struct Gen *gen, int x, int y)
{
    u8 c = Cell(gen, x, y);
    return c == C_BLDG || c == C_ROAD || c == C_TREE || IsEdge(gen, x, y);
}

// Where along the streets a building goes: anywhere, near a point (the
// Pokemon Center by the way in), far from it (the gym at the end of the
// main road), or up on the terrace.
enum { LOT_ANY, LOT_NEAR, LOT_FAR, LOT_UPPER };

// Try the lots along the streets, best first by the preference (with a
// little randomness), or at random.
static bool8 PlaceOnStreetPref(struct Gen *gen, const struct Building *b, const u8 *streetY, int nStreets, struct Placed *out,
                               u8 pref, int tx, int ty)
{
    u8 cand[64]; // street * 32 + x
    int n = 0, s, x;
    // Mostly the lots that close up a row (a unit from a building, a lane or
    // the end of the street), so the buildings stand in rows with even gaps
    // as on Hoenn's streets, and more of them fit.
    bool8 snugFirst = pref == LOT_ANY && CHANCE(75);
    for (s = 0; s < nStreets; s++)
        for (x = 1; x + b->uw < gen->cw && n < 64; x++)
            if (Cell(gen, x + b->doorX / 2, streetY[s]) == C_ROAD)
                if (pref != LOT_UPPER || UpperU(gen, x, streetY[s] - 1))
                    cand[n++] = s * 32 + x;
    while (n > 0)
    {
        int k = RAND(n), c;
        if (snugFirst)
        {
            int i, j = RAND(n);
            for (i = 0; i < n; i++)
            {
                int cx = cand[(j + i) % n] % 32, row = streetY[cand[(j + i) % n] / 32] - 1;
                if (RowEndAt(gen, cx - 1, row) || Cell(gen, cx - 2, row) == C_BLDG
                 || RowEndAt(gen, cx + b->uw, row) || Cell(gen, cx + b->uw + 1, row) == C_BLDG)
                {
                    k = (j + i) % n;
                    break;
                }
            }
        }
        if (pref == LOT_NEAR || pref == LOT_FAR)
        {
            int i, best = 0x7FFF;
            for (i = 0; i < n; i++)
            {
                int d = Abs(cand[i] % 32 + b->doorX / 2 - tx) + Abs(streetY[cand[i] / 32] - ty) + RAND(4);
                if (pref == LOT_FAR)
                    d = 200 - d;
                if (d < best)
                {
                    best = d;
                    k = i;
                }
            }
        }
        c = cand[k];
        if (PlaceBuildingAt(gen, b, c % 32, streetY[c / 32] - b->uh, out))
            return TRUE;
        cand[k] = cand[--n];
    }
    return FALSE;
}

static bool8 PlaceOnStreet(struct Gen *gen, const struct Building *b, const u8 *streetY, int nStreets, struct Placed *out)
{
    return PlaceOnStreetPref(gen, b, streetY, nStreets, out, LOT_ANY, 0, 0);
}

// Like PlaceOnStreet, but only lots whose door opens onto row y between xa
// and xb (a plaza).
static bool8 PlaceFacing(struct Gen *gen, const struct Building *b, int y, int xa, int xb, struct Placed *out)
{
    int cand[32], n = 0, x;
    for (x = 1; x + b->uw < gen->cw && n < 32; x++)
    {
        int door = x + b->doorX / 2;
        if (door >= xa && door <= xb && Cell(gen, door, y) == C_ROAD)
            cand[n++] = x;
    }
    while (n > 0)
    {
        int k = RAND(n);
        if (PlaceBuildingAt(gen, b, cand[k], y - b->uh, out))
            return TRUE;
        cand[k] = cand[--n];
    }
    return FALSE;
}

// Fill leftover lawns in a town: fenced flower beds, then trees, so there is
// no bare stretch bigger than a few steps. Ground right beside a road stays
// open.
// Open ground where a sign post may stand.
static bool8 WalkableCover(struct Gen *gen, int fx, int fy)
{
    u8 c = Cell(gen, fx >> 1, fy >> 1);
    return c == C_OPEN || c == C_ROAD;
}

static bool8 IsBeachU(struct Gen *gen, int x, int y)
{
    int i = y * gen->cw + x;
    return InBounds(gen, x, y) && ((gen->beach[i >> 3] >> (i & 7)) & 1);
}

static bool8 IsLawn(struct Gen *gen, int x, int y)
{
    return Cell(gen, x, y) == C_OPEN && !HasNeighbor(gen, x, y, C_ROAD, FALSE) && !IsBeachU(gen, x, y)
        && !HasNeighbor(gen, x, y, C_SEA, TRUE);
}

static void FurnishTown(struct Gen *gen, int startX, int startY, int maxGardens)
{
    int x, y, dx, dy, tries;

    // A fenced flower bed, 3x2 units, where it fits on lawn: one at most,
    // and only in some towns, so it never becomes the town's pattern.
    int gardens = 0;
    for (tries = 0; tries < 40 && gardens < maxGardens; tries++)
    {
        YIELD();
        // Not just above the border trees along the bottom, whose tips it
        // would cover.
        int ux = 1 + RAND(gen->cw - 4), uy = 1 + RAND(gen->ch - 4), before;
        bool8 ok = TRUE;
        for (dy = 0; dy < 2 && ok; dy++)
            for (dx = 0; dx < 3 && ok; dx++)
                if (Cell(gen, ux + dx, uy + dy) != C_OPEN || IsEdge(gen, ux + dx, uy + dy) || IsBeachU(gen, ux + dx, uy + dy))
                    ok = FALSE;
        if (!ok || !FenceRingClear(gen, ux, uy, 3, 2))
            continue;
        before = FloodReachable(gen, startX, startY);
        for (dy = 0; dy < 2; dy++)
            for (dx = 0; dx < 3; dx++)
                SetCell(gen, ux + dx, uy + dy, C_DECOR);
        if (FloodReachable(gen, startX, startY) == before - 6 && AddDecor(gen, sFenceGarden, ux * 2, uy * 2, 6, 4))
        {
            gardens++;
            continue;
        }
        for (dy = 0; dy < 2; dy++)
            for (dx = 0; dx < 3; dx++)
                SetCell(gen, ux + dx, uy + dy, C_OPEN);
    }
    // Trees in any 2x2 stretch of lawn.
    for (y = 1; y < gen->ch - 2; y++)
    {
        for (x = 1; x < gen->cw - 2; x++)
        {
            if (!IsLawn(gen, x, y) || !IsLawn(gen, x + 1, y) || !IsLawn(gen, x, y + 1) || !IsLawn(gen, x + 1, y + 1))
                continue;
            if (!CHANCE(75))
                continue;
            {
                int tx = x + RAND(2), ty = y + RAND(2);
                // On sand only beside other trees or a building, so they
                // make groves at the edge of the open sand.
                if (Cell(gen, tx, ty - 1) != C_ROCK && !RimAbove(gen, tx, ty)
                 && !(gen->townGround != TGROUND_GRASS && Cell(gen, tx, ty - 1) == C_DECOR)
                 && (gen->townGround != TGROUND_SAND || HasNeighbor(gen, tx, ty, C_TREE, FALSE)
                     || HasNeighbor(gen, tx, ty, C_BLDG, FALSE)))
                    SetCell(gen, tx, ty, C_TREE);
            }
        }
    }
}

// Theme decorations (lamps, fences, bike racks, stalls, shrubs...) on lawn
// near the streets. Each takes whole units, which become solid.
static bool8 PlacePropOnce(struct Gen *gen, const struct DecorDef *d, int startX, int startY, bool8 anywhere, int tries);

// Two of a kind at most, so no prop (a bike rack, a stall) fills the town.
#define MAX_SAME_PROP 2

static void PlaceProps(struct Gen *gen, const struct TownTheme *th, int count, int startX, int startY)
{
    int tries, placed = 0;
    u8 used[16];
    memset(used, 0, sizeof(used));
    for (tries = 0; tries < 30 && placed < count && th->nDecor; tries++)
    {
        int k = RAND(th->nDecor);
        if (k < 16 && used[k] >= MAX_SAME_PROP)
            continue;
        if (PlacePropOnce(gen, &th->decor[k], startX, startY, FALSE, 4))
        {
            placed++;
            if (k < 16)
                used[k]++;
        }
    }
}

// One decoration somewhere on the lawn, near a street unless anywhere.
static bool8 PlacePropOnce(struct Gen *gen, const struct DecorDef *d, int startX, int startY, bool8 anywhere, int count)
{
    int tries, x, y;
    for (tries = 0; tries < count * 30; tries++)
    {
        YIELD();
        int uw = (d->w + 1) / 2, uh = (d->h + 1) / 2;
        int ux = 1 + RAND(gen->cw - 1 - uw), uy = 1 + RAND(gen->ch - 2 - uh);
        bool8 ok = TRUE, nearRoad = FALSE;
        int before;
        for (y = uy; y < uy + uh && ok; y++)
            for (x = ux; x < ux + uw && ok; x++)
                if (Cell(gen, x, y) != C_OPEN || IsEdge(gen, x, y) || IsBeachU(gen, x, y))
                    ok = FALSE;
        if (!ok || InSeamView(gen, ux, uy, uw, uh))
            continue;
        // A big prop (a row house, an awning) keeps a unit from buildings
        // and water, and none stands right under a rock face or a terrace's
        // rim.
        if (d->w >= 3 && d->h >= 3 && (!FenceRingClear(gen, ux, uy, uw, uh)
         || HasWaterRing(gen, ux, uy, uw, uh)))
            continue;
        for (x = ux; x < ux + uw && ok; x++)
            if (Cell(gen, x, uy - 1) == C_ROCK || Cell(gen, x, uy - 1) == C_CLIFF || RimAbove(gen, x, uy))
                ok = FALSE;
        if (!ok)
            continue;
        for (y = uy - 1; y <= uy + uh; y++)
            for (x = ux - 1; x <= ux + uw; x++)
                if (Cell(gen, x, y) == C_ROAD)
                    nearRoad = TRUE;
        if (!nearRoad && !anywhere && tries < count * 20)
            continue;
        before = FloodReachable(gen, startX, startY);
        for (y = uy; y < uy + uh; y++)
            for (x = ux; x < ux + uw; x++)
                SetCell(gen, x, y, C_DECOR);
        if (FloodReachable(gen, startX, startY) != before - uw * uh || !AddDecor(gen, d->blocks, ux * 2, uy * 2, d->w, d->h))
        {
            for (y = uy; y < uy + uh; y++)
                for (x = ux; x < ux + uw; x++)
                    SetCell(gen, x, y, C_OPEN);
            continue;
        }
        return TRUE;
    }
    return FALSE;
}

// TRUE if nothing stands within a unit of (x, y): open ground or road
// all round.
static bool8 OpenAllRound(struct Gen *gen, int x, int y)
{
    int dx, dy;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
        {
            u8 c = Cell(gen, x + dx, y + dy);
            if ((c != C_OPEN && c != C_ROAD) || IsBeachU(gen, x + dx, y + dy))
                return FALSE;
        }
    return TRUE;
}

// What lawn is still wide open after the yards and trees: a lone tree here
// and there, as in Petalburg, until no stretch of lawn is more than a unit
// from something. Never on a beach, under a rock face or where it would
// shut a way. Sand stays open, as in Dewford, whose island trees stand
// only in groves.
static void FillOpenLawn(struct Gen *gen, int startX, int startY)
{
    int pass, x, y, n = gen->cw * gen->ch, start = RAND(n), k, before = -1;
    if (gen->townGround == TGROUND_SAND)
        return;
    for (pass = 0; pass < 2; pass++)
    {
        for (k = 0; k < n; k++)
        {
            int i = (start + k) % n;
            YIELD();
            x = i % gen->cw;
            y = i / gen->cw;
            if (x < 2 || y < 2 || x >= gen->cw - 2 || y >= gen->ch - 2 || Cell(gen, x, y) != C_OPEN || !OpenAllRound(gen, x, y)
             || Cell(gen, x, y - 1) == C_ROCK || RimAbove(gen, x, y) || InSeamView(gen, x, y, 1, 1)
             || (gen->townGround != TGROUND_GRASS && Cell(gen, x, y - 1) == C_DECOR))
                continue;
            // The first pass keeps off the roads' edges.
            if (pass == 0 && HasNeighbor(gen, x, y, C_ROAD, FALSE))
                continue;
            if (before < 0)
                before = FloodReachable(gen, startX, startY);
            SetCell(gen, x, y, C_TREE);
            if (FloodReachable(gen, startX, startY) != before - 1)
            {
                SetCell(gen, x, y, C_OPEN);
                continue;
            }
            before--;
        }
    }
}

// Front-yard fences: one continuous line along the south curb of a street,
// wherever there is lawn, stopping at lanes, doors and buildings.
static void AddCurbFences(struct Gen *gen, const u8 *streetY, int nStreets, int startX, int startY)
{
    int s, x;
    bool8 secondary = (gen->fence[1] & 0x3FF) >= 0x200;
    if (!gen->fence[1])
        return;
    for (s = 0; s < nStreets; s++)
    {
        int u = streetY[s] + 1;
        x = 1;
        while (x < gen->cw - 1 && gen->nFences < 8)
        {
            YIELD();
            int x0 = x, len, k, before;
            while (x < gen->cw - 1 && Cell(gen, x, u) == C_OPEN && Cell(gen, x, u - 1) == C_ROAD
                   && !IsEdge(gen, x, u) && !(secondary && InSeamView(gen, x, u, 1, 1)))
                x++;
            len = x - x0;
            // It stops a unit short of a building's wall, as Hoenn's fences
            // never run into one at roof height.
            if (len > 0 && Cell(gen, x0 - 1, u) == C_BLDG)
            {
                x0++;
                len--;
            }
            if (len > 0 && Cell(gen, x0 + len, u) == C_BLDG)
                len--;
            if (len < 2 || !CHANCE(70))
            {
                x = (len == 0) ? x + 1 : x;
                continue;
            }
            before = FloodReachable(gen, startX, startY);
            for (k = 0; k < len; k++)
                SetCell(gen, x0 + k, u, C_DECOR);
            if (FloodReachable(gen, startX, startY) != before - len)
            {
                for (k = 0; k < len; k++)
                    SetCell(gen, x0 + k, u, C_OPEN);
                continue;
            }
            gen->fences[gen->nFences].x = x0 * 2;
            gen->fences[gen->nFences].y = u * 2;
            gen->fences[gen->nFences].len = len * 2;
            gen->nFences++;
        }
    }
}

// The sea along the place's sea side, between minD and maxD units deep and
// wandering. Returns the deepest it reaches.
static int ShapeCoastBand(struct Gen *gen, int minD, int maxD, bool8 uneven)
{
    u8 d = gen->p->seaSide;
    int len = EdgeLength(gen, d), pos, k, depth = minD + RAND(maxD - minD + 1), step = 0, deepest = 0, run = 0;
    for (pos = 0; pos < len; pos++)
    {
        // The shore steps in or out every couple of units, in runs; an
        // uneven one (a harbor town's) after one to three units, so it
        // never settles into a regular pattern of notches.
        if (uneven ? run-- <= 0 : pos % 2 == 0)
        {
            if (uneven)
                run = RAND(3);
            if (step == 0 || CHANCE(30))
                step = RAND(2) ? 1 : -1;
            depth += step;
            if (depth < minD) { depth = minD; step = 1; }
            if (depth > maxD) { depth = maxD; step = -1; }
        }
        gen->seaDepth[pos] = depth;
        if (depth > deepest)
            deepest = depth;
        for (k = 0; k < depth; k++)
        {
            int x, y;
            EdgeCell(gen, d, pos, k, &x, &y);
            SetCell(gen, x, y, C_SEA);
        }
    }
    return deepest;
}

// The first unit of land all along the shore is beach.
static void MarkBeach(struct Gen *gen)
{
    u8 d = gen->p->seaSide;
    int len = EdgeLength(gen, d), pos;
    for (pos = 1; pos < len - 1; pos++)
    {
        // Down to the next unit's shore where the coast steps in, so the
        // beach runs on along it in one piece, not in pockets.
        int x, y, k, to = gen->seaDepth[pos];
        if (gen->seaDepth[pos - 1] > to)
            to = gen->seaDepth[pos - 1];
        if (gen->seaDepth[pos + 1] > to)
            to = gen->seaDepth[pos + 1];
        for (k = gen->seaDepth[pos]; k <= to; k++)
        {
            EdgeCell(gen, d, pos, k, &x, &y);
            if (Cell(gen, x, y) == C_OPEN && !IsEdge(gen, x, y))
                gen->beach[(y * gen->cw + x) >> 3] |= 1 << ((y * gen->cw + x) & 7);
        }
    }
}

static void PaintBeach(struct Gen *gen);

// A spring town's red rock, as on Route 112. Lavaridge's red rock is drawn
// with pieces that only fit next to certain others, and on grass Hoenn uses
// it only in low mesas whose ends step down to the grass. So the town has
// one of a set of mesas worked out from the neighbours Hoenn's maps have
// (tools/roguemon/gen_spring_rock.py; check_seeds.py checks every pair).
// Lavaridge's hot spring only fits against Lavaridge's own cliffs, so spring
// towns have none.
static void RenderSpringRock(struct Gen *gen)
{
    const struct SpringRock *t = &sSpringRocks[gen->springRock - 1];
    int x, y;
    for (y = 0; y < t->h; y++)
        for (x = 0; x < t->w; x++)
            if (t->blocks[y * t->w + x])
                Put(gen, gen->springX + x, gen->springY + y, t->blocks[y * t->w + x]);
}

// A mesa along the top of the town, below the trees there with a row of
// grass between (which the rock's top edge needs), clear of the exits'
// view. The grass all round it stays plain grass. FALSE if none fits.
static bool8 PlaceSpringRock(struct Gen *gen, int startX, int startY)
{
    u16 cand[64];
    u8 saved[16 * 8];
    int n = 0, i, k, k0, x, y, before, count, u0 = 0, uw = 0, v1 = 0;
    const struct SpringRock *t;
    for (k = 0; k < (int)(sizeof(sSpringRocks) / sizeof(sSpringRocks[0])); k++)
    {
        int ux;
        t = &sSpringRocks[k];
        for (ux = 1; ux * 2 + t->w <= gen->m->w - 2 && n < 64; ux++)
        {
            int u1 = (ux * 2 + t->w - 1) >> 1, vv = (2 + t->h - 1) >> 1;
            bool8 ok = !InSeamView(gen, ux, 1, u1 - ux + 1, vv);
            for (y = 1; y <= vv && ok; y++)
                for (x = ux; x <= u1 && ok; x++)
                    if (Cell(gen, x, y) != C_OPEN || IsEdge(gen, x, y))
                        ok = FALSE;
            // The trees along the top behind it.
            for (x = ux; x <= u1 && ok; x++)
                if (Cell(gen, x, 0) != C_TREE)
                    ok = FALSE;
            if (ok)
                cand[n++] = PACK(ux, k);
        }
    }
    // Any that shuts no ground off, starting from a random one.
    for (i = 0, k0 = n ? RAND(n) : 0; i < n; i++)
    {
        k = cand[(k0 + i) % n];
        t = &sSpringRocks[PY(k)];
        gen->springX = PX(k) * 2;
        gen->springY = 2;
        u0 = PX(k);
        uw = ((gen->springX + t->w - 1) >> 1) - u0 + 1;
        v1 = (gen->springY + t->h - 1) >> 1;
        for (y = 1; y <= v1; y++)
            for (x = 0; x < uw; x++)
                saved[(y - 1) * 16 + x] = Cell(gen, u0 + x, y);
        // The rock is solid; the grass round it is kept clear.
        before = FloodReachable(gen, startX, startY);
        count = 0;
        for (y = 0; y < t->h * t->w; y++)
        {
            int ux = (gen->springX + y % t->w) >> 1, uy = (gen->springY + y / t->w) >> 1;
            if (t->blocks[y] && Cell(gen, ux, uy) == C_OPEN)
            {
                count++;
                SetCell(gen, ux, uy, C_ROCK);
            }
        }
        for (y = 0; y < t->h * t->w; y++)
        {
            int ux = (gen->springX + y % t->w) >> 1, uy = (gen->springY + y / t->w) >> 1;
            if (Cell(gen, ux, uy) == C_OPEN)
                SetCell(gen, ux, uy, C_KEEP);
        }
        if (FloodReachable(gen, startX, startY) == before - count)
            break;
        for (y = 1; y <= v1; y++)
            for (x = 0; x < uw; x++)
                SetCell(gen, u0 + x, y, saved[(y - 1) * 16 + x]);
    }
    if (i == n)
        return FALSE;
    gen->springRock = PY(k) + 1;
    return TRUE;
}

// ---------------------------------------------------------------------------
// Terraces
//
// Part of a town stands a step higher, behind a one-block rock rim, as in
// Lilycove, Lavaridge and Fallarbor. The terrace runs down from the top of
// the map: column x is raised from the top down to row top[x]. Roads cross
// its rim only straight down a lane, where the rim becomes stone steps.

static void SetUpper(struct Gen *gen, int x, int y, bool8 on)
{
    int i = y * gen->cw + x;
    if (on)
        gen->upper[i >> 3] |= 1 << (i & 7);
    else
        gen->upper[i >> 3] &= ~(1 << (i & 7));
}

// The ground beside, below or diagonally below unit (x, y) that is lower.
static bool8 RimU(struct Gen *gen, int x, int y)
{
    int dx;
    if (!UpperU(gen, x, y))
        return FALSE;
    for (dx = -1; dx <= 1; dx++)
        if ((InBounds(gen, x + dx, y + 1) && !UpperU(gen, x + dx, y + 1))
         || (dx != 0 && InBounds(gen, x + dx, y) && !UpperU(gen, x + dx, y)))
            return TRUE;
    return FALSE;
}

// Can stone steps stand at unit (x, y) of the rim? A lane must cross it
// straight down. Steps stand within a stretch of cliff, never where it ends
// against the trees at the side, and in a paved town never in view of a
// seam, where the road is drawn as grass (InSeamView, see the roads in
// GenTown) and the steps would lead nowhere.
static bool8 StairLane(struct Gen *gen, int x, int y)
{
    u16 road = sThemes[gen->p->type == RTOWN_SUMMIT ? RTHEME_MEADOW : gen->p->theme].roadTL;
    return Cell(gen, x, y) == C_ROAD && Cell(gen, x, y - 1) == C_ROAD && Cell(gen, x, y + 1) == C_ROAD
        && Cell(gen, x - 1, y) != C_ROAD && Cell(gen, x + 1, y) != C_ROAD && x >= 2 && x <= gen->cw - 3
        && !(road && road != B_SAND_TL && InSeamView(gen, x, y - 1, 1, 3));
}

// Try a terrace with the given heights. Every road unit on the rim must be
// a lane going straight down it, and every road must stay reachable.
static bool8 TryTerrace(struct Gen *gen, const u8 *top, int startX, int startY)
{
    int x, y, i;
    memset(gen->upper, 0, sizeof(gen->upper));
    gen->terraced = TRUE;
    gen->nStairs = 0;
    for (x = 0; x < gen->cw; x++)
        for (y = 0; y <= top[x] && y < gen->ch - 2; y++)
            SetUpper(gen, x, y, TRUE);
    for (y = 1; y < gen->ch - 1 && gen->terraced; y++)
    {
        for (x = 1; x < gen->cw - 1 && gen->terraced; x++)
        {
            YIELD();
            u8 c = Cell(gen, x, y);
            bool8 s = !UpperU(gen, x, y + 1), e = !UpperU(gen, x + 1, y), w = !UpperU(gen, x - 1, y);
            if (!RimU(gen, x, y))
                continue;
            if (c == C_OPEN)
                continue;
            if (c == C_ROAD && !e && !w && (!s || (StairLane(gen, x, y) && gen->nStairs < 4)))
            {
                if (s)
                {
                    gen->stairX[gen->nStairs] = x;
                    gen->stairY[gen->nStairs++] = y;
                }
                continue;
            }
            gen->terraced = FALSE;
        }
    }
    // At least one way up, unless the terrace is only the trees at the top.
    if (gen->terraced && gen->nStairs == 0)
        gen->terraced = FALSE;
    if (gen->terraced)
    {
        for (y = 1; y < gen->ch - 1; y++)
            for (x = 1; x < gen->cw - 1; x++)
            {
                YIELD();
                if (Cell(gen, x, y) == C_OPEN && RimU(gen, x, y))
                    SetCell(gen, x, y, C_CLIFF);
            }
        FloodReachable(gen, startX, startY);
        for (i = 0; i < gen->nPath && gen->terraced; i++)
        {
            YIELD();
            if (!Reached(gen, gen->path[i] % gen->cw, gen->path[i] / gen->cw))
                gen->terraced = FALSE;
        }
        if (!gen->terraced)
            for (y = 1; y < gen->ch - 1; y++)
                for (x = 1; x < gen->cw - 1; x++)
                {
                    YIELD();
                    if (Cell(gen, x, y) == C_CLIFF)
                        SetCell(gen, x, y, C_OPEN);
                }
    }
    if (!gen->terraced)
    {
        memset(gen->upper, 0, sizeof(gen->upper));
        gen->nStairs = 0;
    }
    return gen->terraced;
}

// Can the rim run along row y between columns xa and xb? Only open ground and
// lanes crossing it straight are in the way.
static bool8 RimRowFree(struct Gen *gen, int y, int xa, int xb)
{
    int x;
    for (x = xa; x <= xb; x++)
    {
        u8 c = Cell(gen, x, y);
        if (c == C_OPEN)
            continue;
        if (c == C_ROAD && Cell(gen, x, y - 1) == C_ROAD && Cell(gen, x, y + 1) == C_ROAD
         && Cell(gen, x - 1, y) != C_ROAD && Cell(gen, x + 1, y) != C_ROAD)
            continue;
        return FALSE;
    }
    return TRUE;
}

// The nearest road straight up (dir -1) or down (dir 1) from (x, y) over
// open ground, or -1.
static int RoadAlong(struct Gen *gen, int x, int y, int dir)
{
    for (y += dir; y > 0 && y < gen->ch - 1; y += dir)
    {
        if (Cell(gen, x, y) == C_ROAD)
            return y;
        if (Cell(gen, x, y) != C_OPEN)
            return -1;
    }
    return -1;
}

// A way up a terrace that no road crosses yet: a lane from a street on one
// side to one on the other, or out to a short new street on the terrace (or
// below it) for houses to stand along.
static bool8 AddTerraceLane(struct Gen *gen, int t)
{
    int tries;
    for (tries = 0; tries < 20; tries++)
    {
        int x = 3 + RAND(gen->cw - 6), up, down, y, k, end, len;
        bool8 ok = !InSeamView(gen, x, t - 1, 1, 3);
        for (y = t - 1; y <= t + 1 && ok; y++)
            for (k = -1; k <= 1; k++)
                if (Cell(gen, x + k, y) != C_OPEN)
                    ok = FALSE;
        if (!ok)
            continue;
        up = RoadAlong(gen, x, t, -1);
        down = RoadAlong(gen, x, t, 1);
        if (up < 0 && down < 0)
            continue;
        if (up >= 0 && down >= 0)
        {
            CarveRoad(gen, x, up, x, down);
            return TRUE;
        }
        // A new street two units past the rim, if there is room for it.
        end = up >= 0 ? t + 2 : t - 2;
        if (end < 3 || end > gen->ch - 3)
            continue;
        len = 2 + RAND(3);
        for (k = -len; k <= len && ok; k++)
            if (Cell(gen, x + k, end) != C_OPEN || IsEdge(gen, x + k, end) || IsEdge(gen, x + k, end + (up >= 0 ? 1 : -1)))
                ok = FALSE;
        if (!ok)
            continue;
        CarveRoad(gen, x, up >= 0 ? up : down, x, end);
        for (k = -len; k <= len; k++)
            AddRoadCell(gen, x + k, end);
        return TRUE;
    }
    return FALSE;
}

// A few shapes of terrace: across the whole town, stepping down partway
// across, or a raised corner on one side.
static bool8 PlaceTerrace(struct Gen *gen, int minTop, int maxTop, int startX, int startY)
{
    u8 top[MAX_CW], rows[MAX_CW];
    int tries, x, y, nRows = 0;
    if (minTop < 2)
        minTop = 2;
    for (y = minTop; y <= maxTop && nRows < MAX_CW; y++)
        if (RimRowFree(gen, y, 1, gen->cw - 2))
            rows[nRows++] = y;
    if (nRows == 0)
        return FALSE;
    // Not along the tops of the roofs of a street's houses (they keep a
    // unit of lawn below it): with no other row, the town keeps to one
    // level.
    {
        int k, n = 0;
        for (k = 0; k < nRows; k++)
        {
            int roads = 0;
            for (x = 1; x < gen->cw - 1; x++)
                roads += Cell(gen, x, rows[k] + 3) == C_ROAD;
            if (roads < gen->cw / 3)
                rows[n++] = rows[k];
        }
        if (n == 0)
            return FALSE;
        nRows = n;
    }
    for (tries = 0; tries < 12; tries++)
    {
        int shape = RAND(5), t1 = rows[RAND(nRows)], t2 = rows[RAND(nRows)];
        int split = gen->cw / 3 + RAND(gen->cw / 3 + 1);
        if (shape == 1 && Abs(t1 - t2) < 2)
            shape = 0;
        for (x = 0; x < gen->cw; x++)
        {
            switch (shape)
            {
            case 0:
            case 4:  top[x] = t1; break;
            case 1:  top[x] = x < split ? t1 : t2; break;
            case 2:  top[x] = x < split ? t1 : 0; break;
            default: top[x] = x < split ? 0 : t1; break;
            }
        }
        top[0] = top[1];
        top[gen->cw - 1] = top[gen->cw - 2];
        if (TryTerrace(gen, top, startX, startY))
            return TRUE;
        // No road goes up: make a way, once, then try this shape again.
        if (tries < 6 && shape != 1 && AddTerraceLane(gen, t1) && TryTerrace(gen, top, startX, startY))
            return TRUE;
    }
    return FALSE;
}

static bool8 IslandTreeF(struct Gen *gen, int fx, int fy);

// Sand or ash over all of a town's open ground; on sand the roads too.
// Ash keeps to grass within view of a neighbour with another tileset.
static void CoverTownGround(struct Gen *gen)
{
    int fx, fy;
    for (fy = 0; fy < gen->m->h; fy++)
    {
        YIELD();
        for (fx = 0; fx < gen->m->w; fx++)
        {
            u8 c = Cell(gen, fx >> 1, fy >> 1), cover = Cover(gen, fx, fy);
            if (c == C_TREE || c == C_WATER || c == C_SEA || c == C_SHORE)
                continue;
            // Within view of a connection the ground stays grass, like the
            // route next door.
            if (gen->zoned)
            {
                // Grass round the ordinary trees, but sand right beside an
                // island tree, and the roads sand there too.
                if (!BitU(gen->grassZone, gen, fx >> 1, fy >> 1) || IslandTreeF(gen, fx - 1, fy)
                 || IslandTreeF(gen, fx + 1, fy) || IslandTreeF(gen, fx, fy - 1) || IslandTreeF(gen, fx, fy + 1))
                    SetCover(gen, fx, fy, V_GROUND);
                else if (cover == V_PATH)
                    SetCover(gen, fx, fy, V_SAND);
            }
            else if (!InSeamView(gen, fx >> 1, fy >> 1, 1, 1) && (gen->townGround == TGROUND_SAND || cover == V_NONE))
            {
                SetCover(gen, fx, fy, V_GROUND);
            }
        }
    }
}

// The middle of a plaza, 3x2 units from (ux, uy): something of the theme's
// own, never the fenced flower bed of the streets. Paved squares get a
// fountain or a market stall on the paving; the others a garden with a
// tree, flower boxes or the theme's landmark.
static void PlaceCentrepiece(struct Gen *gen, const struct TownTheme *th, u8 theme, int ux, int uy)
{
    int x, y, fx = ux * 2, fy = uy * 2;
    u8 cell = C_KEEP, cover = V_NONE;
    // In view from a route of another tileset: Meadow's trees and flowers,
    // which every tileset draws.
    if (InSeamView(gen, ux, uy, 3, 2))
        theme = RTHEME_MEADOW;
    switch (theme)
    {
    case RTHEME_STONE:
    case RTHEME_HARBOR:
        cell = C_DECOR;
        cover = V_ROAD;
        break;
    case RTHEME_VERDANT:
    case RTHEME_BEACH:
    case RTHEME_TREETOP:
    case RTHEME_ISLAND:
        cell = C_DECOR;
        break;
    case RTHEME_ASH:
        cover = V_GROUND;
        break;
    }
    for (y = uy; y < uy + 2; y++)
        for (x = ux; x < ux + 3; x++)
            SetCell(gen, x, y, cell);
    for (y = fy; y < fy + 4; y++)
        for (x = fx; x < fx + 6; x++)
            SetCover(gen, x, y, cover);
    switch (theme)
    {
    case RTHEME_STONE:
        // Rustboro's fountain, with a lamp on either side.
        AddDecor(gen, sThemeStone_Fountain, fx + 1, fy + 1, 3, 3);
        AddDecor(gen, sThemeStone_Lamp, fx, fy + 2, 1, 2);
        AddDecor(gen, sThemeStone_Lamp, fx + 5, fy + 2, 1, 2);
        break;
    case RTHEME_HARBOR:
        // A market stall, as in Slateport.
        AddDecor(gen, sThemeHarbor_Market, fx, fy, 6, 4);
        break;
    case RTHEME_VERDANT:
        // Flower boxes and planters, as in Verdanturf.
        for (y = fy; y < fy + 4; y++)
            for (x = fx; x < fx + 6; x++)
                SetCover(gen, x, y, CHANCE(40) ? V_FLOWER : V_NONE);
        AddDecor(gen, sThemeVerdant_FlowerBoxes, fx + 1 + RAND(2), fy + 1, 3, 1);
        AddDecor(gen, sThemeVerdant_FlowerPlanter, fx, fy + 2, 1, 2);
        AddDecor(gen, sThemeVerdant_FlowerPlanter, fx + 5, fy + 2, 1, 2);
        break;
    case RTHEME_ISLAND:
        // Mossdeep's round trees in a bed of grass.
        AddDecor(gen, th->decor[1].blocks, fx + 1 + RAND(2), fy, th->decor[1].w, th->decor[1].h);
        break;
    case RTHEME_BEACH:
        // A grove of island trees and a rock on the sand.
        AddDecor(gen, sThemeBeach_IslandGrove, fx + RAND(2), fy, 3, 4);
        AddDecor(gen, th->decor[0].blocks, fx + 4 + RAND(2), fy + 1 + RAND(3), 1, 1);
        break;
    case RTHEME_ASH:
        // A meteor crater, with a boulder or two.
        AddDecor(gen, th->decor[0].blocks, fx + 1 + RAND(2), fy, 3, 3);
        AddDecor(gen, th->decor[1].blocks, RAND(2) ? fx : fx + 5, fy + 1, 1, 2);
        for (y = uy; y < uy + 2; y++)
            for (x = ux; x < ux + 3; x++)
                SetCell(gen, x, y, C_DECOR);
        break;
    case RTHEME_TREETOP:
        // Fortree's big palm.
        AddDecor(gen, sThemeTreetop_BigTree, fx + 1 + RAND(2), fy, 3, 3);
        break;
    case RTHEME_SPRING:
        // Lavaridge's red flower clumps on the grass.
        for (x = 0; x < 3; x++)
            AddDecor(gen, th->decor[0].blocks, fx + x * 2, fy + RAND(4), 2, 1);
        break;
    default:
        // A big tree among flowers: one in the middle, or two (Meadow).
        for (y = fy; y < fy + 4; y++)
            for (x = fx; x < fx + 6; x++)
                if (CHANCE(45))
                    SetCover(gen, x, y, V_FLOWER);
        if (theme == RTHEME_MEADOW)
        {
            SetCell(gen, ux, uy + 1, C_TREE);
            SetCell(gen, ux + 2, uy + 1, C_TREE);
        }
        else
        {
            SetCell(gen, ux + 1, uy + 1, C_TREE);
        }
        break;
    }
}

// A route drawn as rock all along its edges (hills, Victory Road, a sandy
// desert): a town below it has no tree tops to meet, so the cliff comes
// down into the town's top row instead of its trees.
static bool8 RockyRoute(const struct RoguePlace *q)
{
    return q->kind == RPLACE_ROUTE && (q->type == RROUTE_HILLS || q->type == RROUTE_MOUNTAIN
                                       || (q->type == RROUTE_DESERT && q->theme != RTHEME_ASH));
}

static void PlaceTopRock(struct Gen *gen)
{
    const struct RogueLink *l = LinkOnSide(gen->p, RDIR_NORTH);
    const struct RoguePlace *q;
    int x;
    if (!l)
        return;
    q = &gen->r->places[l->target];
    if (!RockyRoute(q))
        return;
    // Not in the corners, whose trees run on down the sides.
    for (x = 1; x < gen->cw - 1; x++)
        if (x * 2 - l->offset >= 0 && x * 2 + 1 - l->offset < q->w)
            SetCell(gen, x, 0, C_ROCK);
}

// The foot of that cliff: rock over a face, ending in round corners.
static void RenderTopRock(struct Gen *gen)
{
    int x;
    for (x = 0; x < gen->cw; x++)
    {
        bool8 left = Cell(gen, x - 1, 0) == C_ROCK, right = Cell(gen, x + 1, 0) == C_ROCK;
        bool8 seam = InSeamView(gen, x, 0, 1, 1);
        // (Not a spring town's own mountain, nor the cave's hill.)
        if (Cell(gen, x, 0) != C_ROCK || (gen->hasMesa && x >= gen->mesa.x && x < gen->mesa.x + gen->mesa.w))
            continue;
        // The top row joins the route's own rock across the seam.
        Put(gen, x * 2, 0, left ? 0x0471 : 0x0470);
        Put(gen, x * 2 + 1, 0, right ? 0x0471 : 0x0472);
        Put(gen, x * 2, 1, OnTownGround(gen, left ? 0x0479 : 0x0478, seam));
        Put(gen, x * 2 + 1, 1, OnTownGround(gen, right ? 0x0479 : 0x047A, seam));
    }
}

// A beach town's tree line is two units deep or more, never a single file:
// open ground just inside the edge trees becomes trees, mostly, where that
// shuts no way and stands clear of the roads, the beach and the water.
static void ThickenBorder(struct Gen *gen, int startX, int startY)
{
    int x, y, before = FloodReachable(gen, startX, startY);
    for (y = 1; y < gen->ch - 1; y++)
    {
        YIELD();
        for (x = 1; x < gen->cw - 1; x++)
        {
            bool8 inner = (x == 1 && Cell(gen, 0, y) == C_TREE) || (x == gen->cw - 2 && Cell(gen, gen->cw - 1, y) == C_TREE)
                       || (y == 1 && Cell(gen, x, 0) == C_TREE) || (y == gen->ch - 2 && Cell(gen, x, gen->ch - 1) == C_TREE);
            if (!inner || Cell(gen, x, y) != C_OPEN || IsBeachU(gen, x, y) || !CHANCE(80)
             || HasNeighbor(gen, x, y, C_ROAD, FALSE) || HasNeighbor(gen, x, y, C_BLDG, TRUE)
             || HasNeighbor(gen, x, y, C_WATER, TRUE) || HasNeighbor(gen, x, y, C_SHORE, TRUE)
             || HasNeighbor(gen, x, y, C_SEA, TRUE) || Cell(gen, x, y - 1) == C_ROCK || Cell(gen, x, y - 1) == C_DECOR
             || RimAbove(gen, x, y))
                continue;
            SetCell(gen, x, y, C_TREE);
            if (FloodReachable(gen, startX, startY) != before - 1)
                SetCell(gen, x, y, C_OPEN);
            else
                before--;
        }
    }
}

static void BeachClearOfTrees(struct Gen *gen);

// Is there an ordinary tree (one in the grass zone) next to unit (x, y)?
static bool8 OrdinaryTreeNear(struct Gen *gen, int x, int y)
{
    int dx, dy;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
            if (InBounds(gen, x + dx, y + dy) && Cell(gen, x + dx, y + dy) == C_TREE
             && BitU(gen->grassZone, gen, x + dx, y + dy))
                return TRUE;
    return FALSE;
}

// Is block (fx, fy) part of an island tree? (After ShapeIslandTown.)
static bool8 IslandTreeF(struct Gen *gen, int fx, int fy)
{
    return InMap(gen, fx, fy) && Cell(gen, fx >> 1, fy >> 1) == C_TREE && !BitU(gen->grassZone, gen, fx >> 1, fy >> 1);
}

// Dewford's island trees stand on sand, an ordinary tree only on grass.
// Where a route joins, an island town keeps ordinary trees, so the route's
// own run on across the seam: there it has a forest on grass, over all the
// route can see and a ragged band two to four units deep (InTownBand), with
// trees wherever they shut no way and a verge of grass round them. Island
// trees keep a unit clear of it, grass on the side of the ordinary trees
// and sand beside the island trees (CoverTownGround), so the two kinds
// never meet.
static void ShapeIslandTown(struct Gen *gen, int startX, int startY)
{
    u8 verge[MAX_CELLS / 8 + 1];
    int x, y, dx, dy, before;
    memset(gen->grassZone, 0, sizeof(gen->grassZone));
    memset(verge, 0, sizeof(verge));
    for (y = 0; y < gen->ch; y++)
        for (x = 0; x < gen->cw; x++)
            if (!ThemedTreeHereU(gen, x, y))
                SetBitU(gen->grassZone, gen, x, y);
    before = FloodReachable(gen, startX, startY);
    for (y = 1; y < gen->ch - 1; y++)
    {
        YIELD();
        for (x = 1; x < gen->cw - 1; x++)
        {
            if (!BitU(gen->grassZone, gen, x, y) || Cell(gen, x, y) != C_OPEN || IsBeachU(gen, x, y)
             || Cell(gen, x, y - 1) == C_ROCK || Cell(gen, x, y - 1) == C_DECOR || RimAbove(gen, x, y)
             || HasNeighbor(gen, x, y, C_WATER, TRUE) || HasNeighbor(gen, x, y, C_SHORE, TRUE)
             || HasNeighbor(gen, x, y, C_SEA, TRUE))
                continue;
            SetCell(gen, x, y, C_TREE);
            if (FloodReachable(gen, startX, startY) != before - 1)
                SetCell(gen, x, y, C_OPEN);
            else
                before--;
        }
    }
    for (y = 0; y < gen->ch; y++)
        for (x = 0; x < gen->cw; x++)
            if (Cell(gen, x, y) == C_TREE && !BitU(gen->grassZone, gen, x, y) && OrdinaryTreeNear(gen, x, y))
                SetCell(gen, x, y, C_OPEN);
    for (y = 0; y < gen->ch; y++)
        for (x = 0; x < gen->cw; x++)
            for (dy = -1; dy <= 1; dy++)
                for (dx = -1; dx <= 1; dx++)
                    if (Cell(gen, x, y) != C_TREE && InBounds(gen, x + dx, y + dy) && Cell(gen, x + dx, y + dy) == C_TREE
                     && BitU(gen->grassZone, gen, x + dx, y + dy))
                        SetBitU(verge, gen, x, y);
    for (x = 0; x < (int)sizeof(verge); x++)
        gen->grassZone[x] |= verge[x];
    gen->zoned = TRUE;
    BeachClearOfTrees(gen);
}

// The beach stops short of the ordinary trees, which stand on grass.
static void BeachClearOfTrees(struct Gen *gen)
{
    int x, y, dx, dy;
    for (y = 0; y < gen->ch; y++)
        for (x = 0; x < gen->cw; x++)
        {
            YIELD();
            for (dy = -1; dy <= 1; dy++)
                for (dx = -1; dx <= 1; dx++)
                    if (IsBeachU(gen, x, y) && InBounds(gen, x + dx, y + dy) && Cell(gen, x + dx, y + dy) == C_TREE
                     && (gen->zoned ? BitU(gen->grassZone, gen, x + dx, y + dy) : !BitU(gen->ashTree, gen, x + dx, y + dy)))
                        gen->beach[(y * gen->cw + x) >> 3] &= ~(1 << ((y * gen->cw + x) & 7));
        }
}

// A harbor city's beach runs the length of its sea front in a band with a
// straight inner edge, as at Slateport, out to the deepest the sea comes
// in and a unit more. Ordinary trees keep off it: the odd trees beside it
// go, and where the tree line meets it at the ends the sand stops short.
static void StraightenBeach(struct Gen *gen)
{
    u8 d = gen->p->seaSide;
    int len = EdgeLength(gen, d), pos, k, x, y, dx, dy, deep = 0;
    for (pos = 1; pos < len - 1; pos++)
        if (gen->seaDepth[pos] > deep)
            deep = gen->seaDepth[pos];
    for (k = 0; k < 2; k++)
    {
        for (pos = 1; pos < len - 1; pos++)
        {
            YIELD();
            int at;
            for (at = gen->seaDepth[pos]; at <= deep; at++)
            {
                EdgeCell(gen, d, pos, at, &x, &y);
                // (The grassy bank under the sea is beach too.)
                if (Cell(gen, x, y) == C_SHORE)
                    SetCell(gen, x, y, C_OPEN);
                if (Cell(gen, x, y) == C_OPEN && !IsEdge(gen, x, y))
                    SetBitU(gen->beach, gen, x, y);
            }
        }
        // (Then once more over the ground the trees leave.)
        for (y = 0; y < gen->ch && k == 0; y++)
            for (x = 0; x < gen->cw; x++)
            {
                YIELD();
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++)
                        if (IsBeachU(gen, x, y) && InBounds(gen, x + dx, y + dy) && Cell(gen, x + dx, y + dy) == C_TREE
                         && !IsEdge(gen, x + dx, y + dy))
                            SetCell(gen, x + dx, y + dy, C_OPEN);
            }
    }
    BeachClearOfTrees(gen);
}

// Its island trees: a grove at either end of the beach, one or two units
// long, on sand with sand all round it (not at the water's edge, which is
// the bank).
static bool8 TryBeachGrove(struct Gen *gen, int x, int y, int startX, int startY)
{
    int dx, dy, before;
    // (None in view from a route of another tileset.)
    if (Cell(gen, x, y) != C_OPEN || IsEdge(gen, x, y) || !IsBeachU(gen, x, y) || InSeamView(gen, x, y, 1, 1))
        return FALSE;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
            if (!IsBeachU(gen, x + dx, y + dy)
             && !(Cell(gen, x + dx, y + dy) == C_TREE && BitU(gen->ashTree, gen, x + dx, y + dy)))
                return FALSE;
    before = FloodReachable(gen, startX, startY);
    SetCell(gen, x, y, C_TREE);
    if (FloodReachable(gen, startX, startY) != before - 1)
    {
        SetCell(gen, x, y, C_OPEN);
        return FALSE;
    }
    SetBitU(gen->ashTree, gen, x, y);
    return TRUE;
}

static void PlaceBeachGroves(struct Gen *gen, int startX, int startY)
{
    u8 d = gen->p->seaSide;
    int len = EdgeLength(gen, d), end, j, k, x, y;
    StraightenBeach(gen);
    for (end = 0; end < 2; end++)
    {
        int placed = 0;
        for (j = 2; j < len / 3 && placed < 2; j++)
        {
            int pos = end ? len - 1 - j : j;
            bool8 ok = FALSE;
            for (k = gen->seaDepth[pos]; k < gen->seaDepth[pos] + 3 && !ok; k++)
            {
                EdgeCell(gen, d, pos, k, &x, &y);
                ok = TryBeachGrove(gen, x, y, startX, startY);
            }
            if (ok)
                placed++;
            else if (placed)
                break;
        }
    }
}

// A unit of lawn between two pieces of road joins them. Otherwise the road
// from an exit can stop a step short of the street, and a boulder or a
// flower bed can land in the gap, so the road runs into it.
static void JoinRoadGaps(struct Gen *gen)
{
    int gx, gy;
    for (gy = 0; gy < gen->ch; gy++)
        for (gx = 0; gx < gen->cw; gx++)
            if (Cell(gen, gx, gy) == C_OPEN
             && ((Cell(gen, gx - 1, gy) == C_ROAD && Cell(gen, gx + 1, gy) == C_ROAD)
              || (Cell(gen, gx, gy - 1) == C_ROAD && Cell(gen, gx, gy + 1) == C_ROAD)))
                AddRoadCell(gen, gx, gy);
}

// Takes back road units in view of a seam that lead nowhere: dead ends that
// are neither a way out on the edge nor in front of a building.
static void PruneSeamStubs(struct Gen *gen)
{
    bool8 changed = TRUE;
    int i;
    while (changed)
    {
        changed = FALSE;
        for (i = 0; i < gen->nPath; i++)
        {
            int x = gen->path[i] % gen->cw, y = gen->path[i] / gen->cw, n = 0;
            u8 d;
            if (Cell(gen, x, y) != C_ROAD || IsEdge(gen, x, y) || !InSeamView(gen, x, y, 1, 1)
             || HasNeighbor(gen, x, y, C_BLDG, TRUE) || HasNeighbor(gen, x, y, C_DECOR, FALSE))
                continue;
            for (d = 0; d < 4; d++)
                n += Cell(gen, x + sDirDX[d], y + sDirDY[d]) == C_ROAD;
            if (n > 1)
                continue;
            SetCell(gen, x, y, C_OPEN);
            gen->path[i] = gen->path[--gen->nPath];
            i--;
            changed = TRUE;
        }
    }
}

enum { HAMLET_POND, HAMLET_GARDEN, HAMLET_CLIFF, HAMLET_GROVE };

// How often a town of each theme has a fenced flower bed, in percent.
static const u8 sBedChance[RTHEME_COUNT] =
{
    [RTHEME_MEADOW] = 45, [RTHEME_VERDANT] = 35, [RTHEME_SPRING] = 20, [RTHEME_COVE] = 15, [RTHEME_HARBOR] = 10,
};

// How often a town of each theme stands on two levels, in percent.
static const u8 sTerraceChance[RTHEME_COUNT] =
{
    [RTHEME_MEADOW] = 25, [RTHEME_VERDANT] = 30, [RTHEME_HARBOR] = 30, [RTHEME_ISLAND] = 50,
    [RTHEME_STONE] = 55, [RTHEME_COVE] = 65, [RTHEME_BEACH] = 15, [RTHEME_ASH] = 50,
    [RTHEME_SPRING] = 60, [RTHEME_TREETOP] = 0,
};

// ---------------------------------------------------------------------------
// Fortree: a town in deep woods has no streets. Its treehouses and buildings
// stand about the woods at different heights, some side by side for a
// walkway between them, joined by winding paths on the forest floor.

static bool8 CarveRoadBfs(struct Gen *gen, int x0, int y0, int x1, int y1);

// Could building b stand at (bx, by), with its door's front unit open? The
// same rules as PlaceBuildingAt, before the front is a path.
static bool8 WoodsLotFree(struct Gen *gen, const struct Building *b, int bx, int by)
{
    int x, y, fx = bx + b->doorX / 2, fy = by + b->uh;
    u8 f;
    if (bx < 1 || by < 1 || bx + b->uw > gen->cw - 1 || !InBounds(gen, fx, fy) || IsEdge(gen, fx, fy))
        return FALSE;
    f = Cell(gen, fx, fy);
    if (f != C_OPEN && f != C_ROAD)
        return FALSE;
    for (y = by - 1; y <= by + b->uh; y++)
    {
        for (x = bx - 1; x <= bx + b->uw; x++)
        {
            u8 c = Cell(gen, x, y);
            bool8 inside = (x >= bx && x < bx + b->uw && y >= by && y < by + b->uh);
            if (c == C_BLDG || c == C_DECOR || c == C_WATER || c == C_ROCK || c == C_SEA || c == C_KEEP)
                return FALSE;
            if (inside && c != C_OPEN)
                return FALSE;
        }
    }
    return StampIsPrimary(b->stamp, b->sw * b->sh) || !LotInSeamView(gen, bx, by, b->uw, b->uh);
}

static bool8 PlaceWoodsAt(struct Gen *gen, const struct Building *b, int bx, int by, struct Placed *out)
{
    int fx = bx + b->doorX / 2, fy = by + b->uh;
    u8 old;
    if (!WoodsLotFree(gen, b, bx, by))
        return FALSE;
    old = Cell(gen, fx, fy);
    SetCell(gen, fx, fy, C_ROAD);
    if (!PlaceBuildingAt(gen, b, bx, by, out))
    {
        SetCell(gen, fx, fy, old);
        return FALSE;
    }
    SetCell(gen, fx, fy, old);
    AddRoadCell(gen, fx, fy);
    return TRUE;
}

// Units of open ground between two placed buildings' footprints (0 when
// they touch or overlap), the larger of the two axes.
static int LotGap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh)
{
    int gx = bx >= ax + aw ? bx - (ax + aw) : (ax >= bx + bw ? ax - (bx + bw) : 0);
    int gy = by >= ay + ah ? by - (ay + ah) : (ay >= by + bh ? ay - (by + bh) : 0);
    return gx > gy ? gx : gy;
}

// Place the town's buildings about the woods. Returns FALSE if a required
// one found no lot.
static bool8 PlaceWoodsBuildings(struct Gen *gen, const struct Building *const *list, const u8 *doors, int nList, int nRequired,
                                 struct Placed *placed, int *nPlaced, int minX, int maxX, int minY, int maxY, int entryX, int entryY)
{
    int i, n = 0, lastTree = -1, pairs = 0, pcX = -1, pcY = -1;
    if (minY < 1)
        minY = 1;
    for (i = 0; i < nList; i++)
    {
        const struct Building *b = list[i];
        struct Placed *out = &placed[n];
        bool8 ok = FALSE;
        // Now and then a treehouse beside the last one, at its height, with
        // a walkway across between their platforms.
        if (b->frontY && lastTree >= 0 && pairs < 3 && CHANCE(50))
        {
            const struct Placed *h = &placed[lastTree];
            int gap = 1 + RAND(2), k, first = RAND(2);
            for (k = 0; k < 2 && !ok; k++)
            {
                int bx = ((k ^ first) == 0) ? h->bx + h->b->uw + gap : h->bx - gap - b->uw;
                if (bx >= minX && bx + b->uw - 1 <= maxX && PlaceWoodsAt(gen, b, bx, h->by, out))
                {
                    int x, y, xa = bx > h->bx ? h->bx + h->b->uw : bx + b->uw, xb = bx > h->bx ? bx : h->bx;
                    // Keep the trees under the walkway clear of paths.
                    for (y = h->by; y < h->by + h->b->uh; y++)
                        for (x = xa; x < xb; x++)
                            SetCell(gen, x, y, C_KEEP);
                    ok = TRUE;
                    pairs++;
                }
            }
        }
        if (!ok)
        {
            int tries, best = -0x7FFF, bbx = 0, bby = 0, xs = maxX - minX - b->uw + 2, ys = maxY - minY - b->uh + 1;
            if (xs < 1 || ys < 1)
                xs = ys = 0;
            for (tries = 0; tries < 90 && xs > 0; tries++)
            {
                int bx = minX + RAND(xs), by = minY + RAND(ys), j, near = 99, score;
                int fx = bx + b->doorX / 2, fy = by + b->uh;
                bool8 level = FALSE;
                if (!WoodsLotFree(gen, b, bx, by))
                    continue;
                for (j = 0; j < n; j++)
                {
                    int g = LotGap(bx, by, b->uw, b->uh, placed[j].bx, placed[j].by, placed[j].b->uw, placed[j].b->uh);
                    if (g < near)
                        near = g;
                    // Not in a row with the others, as on a street.
                    if (placed[j].by == by || placed[j].by + placed[j].b->uh == fy)
                        level = TRUE;
                }
                if (near < 1)
                    continue;
                score = (near > 5 ? 5 : near) * 6 + RAND(24) - (level ? 14 : 0) - (near < 2 ? 12 : 0);
                // The Pokemon Center near the way in, the Mart near it, and
                // the gym deep in the woods.
                if (doors[i] == RWARP_PC)
                    score -= (Abs(fx - entryX) + Abs(fy - entryY)) * 3;
                else if (doors[i] == RWARP_MART && pcX >= 0)
                    score -= (Abs(fx - pcX) + Abs(fy - pcY)) * 2;
                else if (doors[i] == RWARP_GYM)
                    score += (Abs(fx - entryX) + Abs(fy - entryY)) * 2;
                if (score > best)
                {
                    best = score;
                    bbx = bx;
                    bby = by;
                }
            }
            if (best > -0x7FFF)
                ok = PlaceWoodsAt(gen, b, bbx, bby, out);
        }
        if (ok)
        {
            out->door = doors[i];
            if (b->frontY)
                lastTree = n;
            if (doors[i] == RWARP_PC)
            {
                pcX = out->bx + b->doorX / 2;
                pcY = out->by + b->uh;
            }
            n++;
        }
        else if (i < nRequired)
        {
            *nPlaced = n;
            return FALSE;
        }
    }
    *nPlaced = n;
    return TRUE;
}

static bool8 WoodsFree(struct Gen *gen, int x, int y, int tx, int ty)
{
    u8 c;
    if (x == tx && y == ty)
        return TRUE;
    if (!InBounds(gen, x, y) || IsEdge(gen, x, y))
        return FALSE;
    c = Cell(gen, x, y);
    return c == C_OPEN || c == C_ROAD;
}

// A straight run from one unit to another (same row or column): check that
// it is free, or carve it.
static bool8 WoodsRun(struct Gen *gen, int x0, int y0, int x1, int y1, int tx, int ty, bool8 carve)
{
    int x = x0, y = y0;
    for (;;)
    {
        if (carve)
            AddRoadCell(gen, x, y);
        else if (!WoodsFree(gen, x, y, tx, ty))
            return FALSE;
        if (x == x1 && y == y1)
            return TRUE;
        x += (x1 > x) - (x1 < x);
        y += (y1 > y) - (y1 < y);
    }
}

// A winding path from (sx, sy) to the path at (tx, ty): a dogleg with its
// bend somewhere along the way, or a plain corner, round the buildings.
static bool8 WoodsPath(struct Gen *gen, int sx, int sy, int tx, int ty)
{
    int attempt;
    for (attempt = 0; attempt < 8; attempt++)
    {
        int px[4], py[4], k, np = 4;
        bool8 ok = TRUE;
        px[0] = sx;
        py[0] = sy;
        if (attempt < 5)
        {
            // Down the long way, across at a bend, then on.
            bool8 vert = (attempt & 1) ? Abs(ty - sy) < Abs(tx - sx) : Abs(ty - sy) >= Abs(tx - sx);
            if (vert)
            {
                int lo = sy < ty ? sy : ty, hi = sy < ty ? ty : sy, my = lo + RAND(hi - lo + 1);
                px[1] = sx; py[1] = my; px[2] = tx; py[2] = my;
            }
            else
            {
                int lo = sx < tx ? sx : tx, hi = sx < tx ? tx : sx, mx = lo + RAND(hi - lo + 1);
                px[1] = mx; py[1] = sy; px[2] = mx; py[2] = ty;
            }
        }
        else
        {
            bool8 hFirst = attempt & 1;
            px[1] = hFirst ? tx : sx;
            py[1] = hFirst ? sy : ty;
            px[2] = px[1];
            py[2] = py[1];
        }
        px[3] = tx;
        py[3] = ty;
        for (k = 0; k + 1 < np && ok; k++)
            ok = WoodsRun(gen, px[k], py[k], px[k + 1], py[k + 1], tx, ty, FALSE);
        if (!ok)
            continue;
        for (k = 0; k + 1 < np; k++)
            WoodsRun(gen, px[k], py[k], px[k + 1], py[k + 1], tx, ty, TRUE);
        return TRUE;
    }
    return CarveRoadBfs(gen, sx, sy, tx, ty);
}

// The nearest unit of path to (x, y) inside the map, skipping the given
// fronts (buildings not yet joined).
static bool8 NearestPath(struct Gen *gen, int x, int y, const u16 *skip, int nSkip, int *ox, int *oy)
{
    int i, j, best = 0x7FFF;
    for (i = 0; i < gen->nPath; i++)
    {
        int px = gen->path[i] % gen->cw, py = gen->path[i] / gen->cw, d;
        bool8 skipped = FALSE;
        if (IsEdge(gen, px, py) || Cell(gen, px, py) != C_ROAD || (px == x && py == y))
            continue;
        for (j = 0; j < nSkip && !skipped; j++)
            skipped = skip[j] == gen->path[i];
        if (skipped)
            continue;
        d = Abs(px - x) + Abs(py - y) + RAND(3);
        if (d < best)
        {
            best = d;
            *ox = px;
            *oy = py;
        }
    }
    return best < 0x7FFF;
}

// Join the ways in and every building's front to the glade at (hx, hy).
static bool8 JoinWoods(struct Gen *gen, const struct Placed *placed, int nPlaced, const u8 *exitX, const u8 *exitY, int nExits,
                       int hx, int hy)
{
    u16 fronts[12];
    int i, x, y, tx, ty;
    for (i = 0; i < nExits; i++)
        if (!WoodsPath(gen, exitX[i], exitY[i], hx, hy))
            return FALSE;
    for (i = 0; i < nPlaced; i++)
        fronts[i] = (placed[i].by + placed[i].b->uh) * gen->cw + placed[i].bx + placed[i].b->doorX / 2;
    for (i = 0; i < nPlaced; i++)
    {
        int fx = fronts[i] % gen->cw, fy = fronts[i] / gen->cw;
        if (!NearestPath(gen, fx, fy, &fronts[i], nPlaced - i, &tx, &ty) || !WoodsPath(gen, fx, fy, tx, ty))
            return FALSE;
    }
    // The ground kept under the walkways grows trees again.
    for (y = 0; y < gen->ch; y++)
        for (x = 0; x < gen->cw; x++)
            if (Cell(gen, x, y) == C_KEEP)
                SetCell(gen, x, y, C_TREE);
    return TRUE;
}

static bool8 IsSeaRoute(const struct RoguePlace *p);
static void PlaceTunnelHidden(struct Gen *gen);

// How a town's tower or hideout is entered (TownDungeonEntrance in
// rogue_gen_rooms.h). RENTRY_TOWER: the Mirage Tower (sTowerBuildings).
enum { RENTRY_NONE, RENTRY_BUILDING, RENTRY_GAME_CORNER, RENTRY_MESA, RENTRY_HOUSE, RENTRY_TOWER };
static bool8 VolcanoMountainAt(const struct RogueRegion *r, u8 place); // src/rogue_gen_volcano.h (v1.1)
static bool8 VolcanoMountainFits(const struct RogueRegion *r, u8 place);
static bool8 VolcanoSpan(const struct RogueRegion *r, const struct RoguePlace *p, int *x0, int *w);
static int VolcanoGapX(const struct RogueRegion *r, const struct RoguePlace *p, int k);
static void VolcanoFootSizes(struct RogueRegion *r);
static void VolcanoFootExits(const struct RogueRegion *r, const struct RoguePlace *p, u8 dir, int len, int vx, int vy,
                             int *first, int *last);
static u8 TownDungeonEntrance(const struct RogueRegion *r, u8 place);

// Where a dungeon's rock face stands along the top of a town: all along it,
// or beside the way north (not under a rocky route's cliff), on its wider
// side. FALSE if there is no room.
static bool8 DungeonMesaSpan(const struct RogueRegion *r, const struct RoguePlace *p, int *x0, int *w)
{
    const struct RogueLink *l = LinkOnSide(p, RDIR_NORTH);
    int cw = p->w / 2, e, lw, rw;
    if (!l)
    {
        *x0 = 1;
        *w = cw - 2;
        return TRUE;
    }
    if (RockyRoute(&r->places[l->target]))
        return FALSE;
    e = l->pos / 2;
    lw = e - 3;
    rw = cw - 4 - e;
    *x0 = lw >= rw ? 1 : e + 3;
    *w = lw >= rw ? lw : rw;
    return *w >= 6;
}

// Where a hideout's door in the rock stands (DungeonMesaSpan), not under a
// cave's own rock, and in a harbor town clear of the sea along its side, as
// Lilycove's Aqua Hideout is entered from the land: the sea band is up to
// four units deep with a unit of beach, so the rock stops a unit short of
// that. FALSE with the sea along the top or no room left.
static bool8 HideoutMesaSpan(const struct RogueRegion *r, const struct RoguePlace *p, int *x0, int *w)
{
    int cw = p->w / 2;
    if (p->mouth != 0xFF || p->type == RTOWN_SUMMIT || !DungeonMesaSpan(r, p, x0, w))
        return FALSE;
    if (p->layout != RLAYOUT_HARBOR || p->seaSide == RDIR_NONE || p->seaSide == RDIR_SOUTH)
        return TRUE;
    if (p->seaSide == RDIR_NORTH)
        return FALSE;
    if (p->seaSide == RDIR_WEST && *x0 < 7)
    {
        *w -= 7 - *x0;
        *x0 = 7;
    }
    if (p->seaSide == RDIR_EAST && *x0 + *w > cw - 7)
        *w = cw - 7 - *x0;
    return *w >= 6;
}

// (v1.1 legends) Where a legend's lair door stands in a town's rock face: as
// a hideout's (HideoutMesaSpan), so a harbor town on the sea can take one
// clear of its beach, as Lilycove's Aqua Hideout is entered from the land;
// not by the springs, in Fortree's woods or on Dewford's sand. FALSE if
// there is no room.
static bool8 LairMesaSpan(const struct RogueRegion *r, const struct RoguePlace *p, int *x0, int *w)
{
    return p->theme != RTHEME_SPRING && p->theme != RTHEME_TREETOP && p->theme != RTHEME_BEACH
        && HideoutMesaSpan(r, p, x0, w);
}

// TRUE if the town's dungeon is a legend's lair (v1.1 legends).
static bool8 HostsLair(const struct RogueRegion *r, const struct RoguePlace *p)
{
    s32 di = RogueGen_DungeonAt(r, p - r->places);
    return di >= 0 && r->dungeons[di].kind == RDUN_LAIR;
}

// TRUE if the town's dungeon is a villain team's hideout.
static bool8 HostsHideout(const struct RogueRegion *r, const struct RoguePlace *p)
{
    s32 di = RogueGen_DungeonAt(r, p - r->places);
    return di >= 0 && r->dungeons[di].kind == RDUN_HIDEOUT;
}

// The building a town's dungeon is entered by: the theme's tall building or
// warehouse, the tower, or the shut Game Corner. NULL for one of the town's
// houses (also when the big building keeps failing to fit).
static const struct Building *DungeonBuilding(struct Gen *gen, const struct TownTheme *th, u8 entrance)
{
    u8 theme = gen->p->theme;
    if (entrance == RENTRY_BUILDING && sDungeonBuildings[theme] && gen->attempt < 40)
        return sDungeonBuildings[theme];
    if (entrance == RENTRY_TOWER && sTowerBuildings[theme].stamp && gen->attempt < 55)
        return &sTowerBuildings[theme];
    if (entrance == RENTRY_GAME_CORNER && th->nBig)
        return theme == RTHEME_VERDANT && th->nBig > 1 ? &th->big[1] : &th->big[0];
    // A hideout's last resort is a door in a rock standing in the town (one
    // of a set worked out like the cave hills), never a home: any of them at
    // first, then only the narrowest (four units wide) where they keep
    // failing to fit.
    if ((entrance == RENTRY_HOUSE || entrance == RENTRY_BUILDING) && HostsHideout(gen->r, gen->p) && gen->attempt < 58)
    {
        u32 n = sizeof(sHideoutRocks) / sizeof(sHideoutRocks[0]), k;
        if (gen->attempt >= 20)
            for (n = 0; n < sizeof(sHideoutRocks) / sizeof(sHideoutRocks[0]) && sHideoutRocks[n].uw == sHideoutRocks[0].uw; n++)
                ;
        k = RogueRng_Hash(gen->rng, 7700) % n;
        return &sHideoutRocks[k];
    }
    return NULL;
}

// Someone standing beside the town's way into its dungeon: a step or two to
// the side of the door and in front of it, never in its way. A villain
// grunt (ROBJ_GRUNT, slot 2) by the hideout's, or the keeper (ROBJ_GUIDE)
// by a tower's that is only a plain house.
static void PlaceLookout(struct Gen *gen, u8 kind, u8 slot)
{
    static const s8 sSpots[][2] = { { 1, 1 }, { -1, 1 }, { 2, 1 }, { -2, 1 }, { 1, 2 }, { -1, 2 }, { 2, 2 }, { -2, 2 },
                                    { 3, 1 }, { -3, 1 }, { 3, 2 }, { -3, 2 }, { 1, 3 }, { -1, 3 } };
    const struct RogueMap *m = gen->m;
    int i, k, wx = -1, wy = -1, first = RAND(2);
    for (i = 0; i < m->nWarps; i++)
        if (m->warps[i].kind == RWARP_DUNGEON)
            wx = m->warps[i].x, wy = m->warps[i].y;
    if (wx < 0)
        return;
    for (k = 0; k < (int)(sizeof(sSpots) / sizeof(sSpots[0])); k++)
    {
        // Either side first, by the dice.
        int fx = wx + (first ? -sSpots[k][0] : sSpots[k][0]), fy = wy + sSpots[k][1];
        u8 c;
        if (!InMap(gen, fx, fy) || IsEdge(gen, fx >> 1, fy >> 1))
            continue;
        c = Cell(gen, fx >> 1, fy >> 1);
        if ((c != C_OPEN && c != C_ROAD) || !Reached(gen, fx >> 1, fy >> 1) || IsOccupied(gen, fx, fy)
         || ByLedgeF(gen, fx, fy) || SeenFromNextDoor(gen, fx, fy) || BlocksWay(gen, fx, fy))
            continue;
        AddObj(gen, kind, fx, fy, RDIR_SOUTH, slot, 0);
        return;
    }
}

static bool8 HouseSignsReadable(struct Gen *gen);
static bool8 TownReachable(struct Gen *gen);
static bool8 SeaTownExitsJoin(struct Gen *gen); // v1.1 sea (src/rogue_gen_sea.h)

// The town's signature landmark (src/rogue_gen_signatures.h).
struct SigTown
{
    u8 sig;   // enum RogueSignature
    u8 kind;  // what stands: 0 nothing yet, 1 a mine, 2 the hot spring, 3 the market, 4 a lot, 5 fishers
    s8 lx, ly; // the lot's top left unit (the hot spring's top left block, the mine's front unit)
    u8 nPiers;
    u8 pierX[3], pierY[3], pierLen[3]; // blocks
    bool8 built;
    bool8 small; // the small paddock or plot
    u8 lotY;     // a street lot's stamp offset (Building.sy)
    u8 lotW, lotH; // a lot's size in blocks
};
static void SigTownStart(struct Gen *gen, struct SigTown *st);
static u8 SigHamletStyle(const struct SigTown *st, u8 style, u8 ground);
static const struct Building *SigBuilding(struct Gen *gen, const struct TownTheme *th, const struct SigTown *st,
                                          struct Building *scratch);
static bool8 SigPlaceMine(struct Gen *gen, struct SigTown *st);

static bool8 SigBeforeBuildings(struct Gen *gen, struct SigTown *st, int startX, int startY);
static bool8 SigAfterBuildings(struct Gen *gen, struct SigTown *st, const struct TownTheme *th, int startX, int startY);
static int SigPeople(struct Gen *gen, struct SigTown *st, const struct Placed *placed, int nPlaced, int *mons);
static int SigMonsWanted(const struct SigTown *st, bool8 hamlet);
static void SigRender(struct Gen *gen, struct SigTown *st);
static bool8 SigPlaceTall(struct Gen *gen, const struct Building *b, const u8 *streetY, int nStreets, int top, struct Placed *out);
static void SigFinish(struct Gen *gen, struct SigTown *st, const struct Placed *placed, int nPlaced);

// v1.1 towns2 (src/rogue_gen_towns2.h): the raft village and the crater
// town lay themselves out.
static bool8 GenRaftTown(struct Gen *gen);
static bool8 GenCraterTown(struct Gen *gen);

static bool8 GenTown(struct Gen *gen)
{
    const struct RoguePlace *p = gen->p;
    const struct RogueRegion *r = gen->r;
    bool8 summit = (p->type == RTOWN_SUMMIT);
    if (p->theme == RTHEME_RAFT && !summit)
        return GenRaftTown(gen);
    if (p->theme == RTHEME_CRATER && !summit)
        return GenCraterTown(gen);
    bool8 city = (p->type == RTOWN_GYM);
    bool8 hamlet = (p->type == RTOWN_HAMLET);
    // The summit is drawn with Ever Grande's tileset for the League, so it
    // uses only the plain pieces that every tileset shares.
    const struct TownTheme *th = &sThemes[summit ? RTHEME_MEADOW : p->theme];
    u8 doors[10];
    struct RogueMap *m = gen->m;
    struct Placed placed[MAX_LOTS];
    int nPlaced = 0, i, cx, cy, ix, iy, x;
    s8 houseOf[ROGUE_MAX_HOUSES];
    const struct Building *list[10];
    int nList = 0, nRequired, lastHouse = -1, nWalks = 0;
    u8 gymLot, walkA[3], walkB[3];
    bool8 centreDone = FALSE, harborCity = FALSE;
    u8 streetY[3];
    int nStreets, x0, x1, top = 4, minX, maxX, minY, maxY, plazaX = -1, plazaY = -1, terraceRow = -1, pw = 7;
    int entryX = -1, entryY = -1, entryTarget = 0xFF;
    // Fortree's woods: no streets (PlaceWoodsBuildings).
    bool8 woodsLayout = FALSE;
    u8 exitX[6], exitY[6];
    int nExits = 0, hx = 0, hy = 0;
    // A hamlet is a small, odd place at the end of a side path: a lone
    // house or two by a pond, in a garden, up on a ledge or in a grove.
    u8 hamletStyle = hamlet ? RAND(4) : 0xFF;
    // A tower or hideout here: its door, its sign once the town is laid.
    u8 entrance = TownDungeonEntrance(r, (u8)(p - r->places));
    int dunX = -1, dunY = -1, dunMouthX = -1;
    const struct Building *dunB = NULL;
    // On sand or ash: a grove or a ledge (no grassy pond or garden).
    if (hamlet && th->ground != TGROUND_GRASS && hamletStyle < HAMLET_CLIFF)
        hamletStyle = RAND(2) ? HAMLET_CLIFF : HAMLET_GROVE;
    // The town's signature (its draws come after the town's own, below,
    // where they can, so a town without a landmark is laid as before).
    struct SigTown st;
    struct Building sigScratch;
    const struct Building *sigB;
    int sigMons = 0;
    memset(placed, 0, sizeof(placed));
    gen->lots = placed;
    SigTownStart(gen, &st);
    if (hamlet)
        hamletStyle = SigHamletStyle(&st, hamletStyle, th->ground);
    sigB = SigBuilding(gen, th, &st, &sigScratch);
    // A town that keeps failing to fit its landmark is built without it at
    // last, rather than not at all.
    if (gen->attempt >= SigGiveUp(gen->r, gen->m->place))
        sigB = NULL;
    bool8 wantTerrace = !summit && (hamlet ? hamletStyle == HAMLET_CLIFF : CHANCE(sTerraceChance[p->theme]))
                     && p->seaSide != RDIR_NORTH;

    Fill(gen, C_OPEN);
    gen->townGround = th->ground;
    // A tall building that keeps failing to fit gives way to the smaller
    // tower for a tower, else to a door in the rock along the top (a
    // hideout's, as the Aqua Hideout's), before a plain house.
    if (entrance == RENTRY_BUILDING && gen->attempt >= 40)
    {
        s32 di = RogueGen_DungeonAt(r, (u8)(p - r->places));
        int mx0, mw;
        if (di >= 0 && r->dungeons[di].kind == RDUN_TOWER && sTowerBuildings[p->theme].stamp)
            entrance = RENTRY_TOWER;
        else if (HostsHideout(r, p) ? (p->mouth != 0xFF && p->type != RTOWN_SUMMIT) || HideoutMesaSpan(r, p, &mx0, &mw)
                                    : p->mouth == 0xFF && p->type != RTOWN_SUMMIT && DungeonMesaSpan(r, p, &mx0, &mw))
            entrance = RENTRY_MESA;
    }
    if (entrance != RENTRY_NONE && entrance != RENTRY_MESA)
        dunB = DungeonBuilding(gen, th, entrance);
    // Beach towns come in two kinds. A gym town is a harbor city: it stands
    // on grass like Slateport and Lilycove, with sand streets and, on the
    // sea, a beach with groves of island trees on it. Villages and hamlets
    // are Dewford: all sand and island trees, but for a forest on grass
    // where a route joins (ShapeIslandTown).
    if (p->theme == RTHEME_BEACH && !summit)
    {
        gen->islandTrees = TRUE;
        if (city)
        {
            gen->townGround = TGROUND_GRASS;
            harborCity = TRUE;
        }
        else
            gen->islandTown = TRUE;
    }
    // Fortree's treehouses stand in deep woods.
    gen->forestTown = (p->theme == RTHEME_TREETOP && !summit);
    woodsLayout = gen->forestTown && !hamlet;
    // A cave mouth in a rock face along the top: the town lies at the far
    // end of a cave. The summit's mesa sits beside the League instead.
    if (p->mouth != 0xFF && !summit)
    {
        // A hideout's door in the same rock, or the cave's alone.
        if (entrance == RENTRY_MESA)
        {
            if ((dunMouthX = PlaceCaveHillTwin(gen, &ix, &iy, &dunX, &dunY)) < 0)
                return FALSE;
        }
        else
            PlaceMesa(gen, &ix, &iy);
        top = 6;
        // A mining town by a cave: its workings are in the cave.
        if (st.sig == RSIG_MINING)
        {
            st.kind = 1;
            st.lx = gen->mouthX / 2;
            st.ly = 3;
            st.small = TRUE; // (the cave's own mouth: nobody stands in it)
        }
    }
    else if (entrance == RENTRY_MESA)
    {
        // A door into the rock along the top, as Mt. Pyre's and the Aqua
        // Hideout's are: the mesa's mouth leads into the dungeon.
        int x0, w;
        if (!(HostsHideout(r, p) || HostsLair(r, p)) || !HideoutMesaSpan(r, p, &x0, &w))
            DungeonMesaSpan(r, p, &x0, &w);
        // (v1.1) A spring town's volcano: Mt. Chimney over Lavaridge.
        if (VolcanoMountainAt(r, p - r->places))
        {
            PlaceVolcanoHill(gen, x0, w, &dunX, &dunY);
            top = 7;
        }
        else
        {
            PlaceMesaAt(gen, x0, w, &dunX, &dunY);
            // A tunnel's far mouth is the dungeon's second way in.
            m->warps[m->nWarps - 1].kind = RogueGen_TunnelMouthAt(r, p - r->places) ? RWARP_DUNGEON_2 : RWARP_DUNGEON;
            top = 6;
        }
    }
    else if (!summit && st.sig == RSIG_MINING && SigPlaceMine(gen, &st))
    {
        // The mine's rock face along the top.
        top = 6;
    }
    else if (!summit)
    {
        PlaceTopRock(gen);
        // Room for a spring town's mesa (PlaceSpringRock), and for the hot
        // spring under it.
        if (p->theme == RTHEME_SPRING)
            top = st.sig == RSIG_HOT_SPRING ? 7 : 6;
    }
    // A harbor town has the sea along one side and a beach above it, and
    // its streets keep to the land.
    minY = top;
    maxY = gen->ch - 3;
    minX = 1;
    maxX = gen->cw - 2;
    if (p->layout == RLAYOUT_HARBOR && p->seaSide != RDIR_NONE)
    {
        int depth = ShapeCoastBand(gen, 2, 4, TRUE), end;
        // Its water corners are rounded off where they meet the beach, as on
        // a coast route.
        gen->bites = TRUE;
        // Where a route joins the side at either end of the sea front,
        // the sea stops short of that side behind the tree wall, so it
        // never meets the route's land across the seam.
        for (end = 0; end < 2; end++)
        {
            u8 side = (p->seaSide == RDIR_NORTH || p->seaSide == RDIR_SOUTH) ? (end ? RDIR_EAST : RDIR_WEST)
                                                                           : (end ? RDIR_SOUTH : RDIR_NORTH);
            int pos = end ? EdgeLength(gen, p->seaSide) - 1 : 0, k;
            const struct RogueLink *l = LinkOnSide(p, side);
            if (!l)
                continue;
            if (IsSeaRoute(&r->places[l->target]))
            {
                // The sea route's water runs on across the seam beyond its
                // landing, a few units past the way out (see SeaLandDepth):
                // the shore bends round to meet it there.
                int len = EdgeLength(gen, side), e = l->pos / 2, bay, j;
                bay = (p->seaSide == RDIR_WEST || p->seaSide == RDIR_NORTH) ? e - 3 : len - (e + 4);
                for (j = 0; j < bay; j++)
                {
                    int at = end ? pos - j : pos + j, want = bay - j, q;
                    if (at < 0 || at >= EdgeLength(gen, p->seaSide) || want <= gen->seaDepth[at])
                        break;
                    for (q = gen->seaDepth[at]; q < want; q++)
                    {
                        int sx, sy;
                        EdgeCell(gen, p->seaSide, at, q, &sx, &sy);
                        SetCell(gen, sx, sy, C_SEA);
                    }
                    gen->seaDepth[at] = want;
                }
                continue;
            }
            for (k = 0; k < gen->seaDepth[pos]; k++)
            {
                int sx, sy;
                EdgeCell(gen, p->seaSide, pos, k, &sx, &sy);
                // Under a rocky route the top row is the cliff's foot.
                SetCell(gen, sx, sy, (side == RDIR_NORTH && RockyRoute(&r->places[l->target])
                                      && r->places[l->target].type != RROUTE_HILLS) ? C_ROCK : C_TREE);
            }
            gen->seaDepth[pos] = 0;
        }
        switch (p->seaSide)
        {
        case RDIR_NORTH: minY = depth + 4; break;
        case RDIR_SOUTH: maxY = gen->ch - 2 - depth - 2; break;
        case RDIR_WEST:  minX = depth + 2; break;
        default:         maxX = gen->cw - 2 - depth - 2; break;
        }
        MarkBeach(gen);
    }

    if (woodsLayout)
    {
        // The glade the paths meet at, somewhere in the middle of the woods,
        // open in a plaza town.
        int sx = maxX - minX, sy = maxY - minY, y;
        hx = minX + sx / 4 + RAND(sx / 2 + 1);
        hy = minY + sy / 4 + RAND(sy / 2 + 1);
        nStreets = 1;
        streetY[0] = hy;
        x0 = x1 = hx;
        if (p->layout == RLAYOUT_PLAZA)
        {
            pw = 5;
            plazaX = hx - 2 < 1 ? 1 : (hx + 2 > gen->cw - 2 ? gen->cw - 6 : hx - 2);
            plazaY = hy - 1 < 1 ? 1 : (hy + 1 > gen->ch - 2 ? gen->ch - 4 : hy - 1);
            for (y = plazaY; y < plazaY + 3; y++)
                for (x = plazaX; x < plazaX + pw; x++)
                    AddRoadCell(gen, x, y);
        }
        else
        {
            AddRoadCell(gen, hx, hy);
        }
    }
    else
    {
        // Streets first. Houses sit along the north side of each street with
        // their doors on it. Villages have one or two streets, cities two or three.
        nStreets = (city || CHANCE(50)) ? 2 : 1;
        if (city && CHANCE(60) && maxY - minY >= 7)
            nStreets = 3;
        if (hamlet)
            nStreets = 1;
        if (summit)
        {
            nStreets = 1;
            streetY[0] = 7 + RAND(2);
        }
        else if (wantTerrace && CHANCE(60) && maxY - minY >= 6)
        {
            // A street along the top of a terrace, with houses behind it, and
            // one below with houses under the rim, as in Lilycove, with a
            // strip of lawn between the rim and their roofs.
            int s = minY + RAND(2), y;
            if (s + 5 > maxY)
                s = maxY - 5;
            // The rim may not lie on an exit's row.
            for (i = 0; i < p->nLinks; i++)
                if ((p->links[i].dir == RDIR_WEST || p->links[i].dir == RDIR_EAST) && p->links[i].pos / 2 == s + 1
                 && s + 6 <= maxY)
                    s++;
            terraceRow = s + 1;
            streetY[0] = s;
            streetY[1] = s + 5;
            nStreets = 2;
            y = streetY[1] + 3 + RAND(2);
            if (city && y <= maxY)
                streetY[nStreets++] = y;
        }
        else if (nStreets == 1)
        {
            streetY[0] = minY + 1 + RAND(maxY - minY > 4 ? 4 : 2);
        }
        else
        {
            streetY[0] = minY + RAND(2);
            for (i = 1; i < nStreets; i++)
            {
                streetY[i] = streetY[i - 1] + 3 + RAND(2);
                // Room for a terrace's rim between two streets, a unit of
                // lawn under it and the houses of the lower street.
                if (wantTerrace && i == 1 && streetY[0] + 5 <= maxY)
                    streetY[i] = streetY[0] + 5;
                if (streetY[i] > maxY)
                    streetY[i] = maxY;
            }
            while (nStreets > 1 && streetY[nStreets - 1] - streetY[nStreets - 2] < 3)
                nStreets--;
        }
        // A tall tower stands on the top street: room above it, with fewer
        // streets in a short town (else it would end up a plain house).
        while ((entrance == RENTRY_BUILDING || entrance == RENTRY_TOWER) && dunB && terraceRow < 0 && !summit && streetY[0] < dunB->uh + 1
               && nStreets > 1 && streetY[nStreets - 1] + dunB->uh + 1 - streetY[0] > maxY)
            nStreets--;
        if ((entrance == RENTRY_BUILDING || entrance == RENTRY_TOWER) && dunB && terraceRow < 0 && !summit
         && streetY[0] < dunB->uh + 1 && streetY[nStreets - 1] + dunB->uh + 1 - streetY[0] <= maxY)
        {
            int shift = dunB->uh + 1 - streetY[0];
            for (i = 0; i < nStreets; i++)
                streetY[i] += shift;
        }
        // So does a tall signature landmark (the Space Center).
        while (sigB && sigB->uh >= 5 && terraceRow < 0 && !summit && streetY[0] < sigB->uh + 1 && nStreets > 1
               && streetY[nStreets - 1] + sigB->uh + 1 - streetY[0] > maxY)
            nStreets--;
        if (sigB && sigB->uh >= 5 && terraceRow < 0 && !summit && streetY[0] < sigB->uh + 1
         && streetY[nStreets - 1] + sigB->uh + 1 - streetY[0] <= maxY)
        {
            int shift = sigB->uh + 1 - streetY[0];
            for (i = 0; i < nStreets; i++)
                streetY[i] += shift;
        }
        x0 = minX + RAND(2);
        x1 = maxX - RAND(2);
        // A hamlet's lane is short.
        if (hamlet)
        {
            x0 = gen->cw / 2 - 3 - RAND(2);
            x1 = gen->cw / 2 + 2 + RAND(2);
        }
        for (i = 0; i < nStreets; i++)
        {
            int a = x0 + (i == 1 ? RAND(3) : 0), b = x1 - (i == 1 ? RAND(3) : 0);
            for (x = a; x <= b; x++)
                AddRoadCell(gen, x, streetY[i]);
        }
        for (i = 0; i + 1 < nStreets; i++)
        {
            // A lane between each pair of streets.
            x = x0 + 2 + RAND(x1 - x0 - 4 > 1 ? x1 - x0 - 4 : 1);
            CarveRoad(gen, x, streetY[i], x, streetY[i + 1]);
        }
        // A plaza: a paved square below one street, with the big buildings
        // along its north side and a garden in the middle.
        if (p->layout == RLAYOUT_PLAZA)
        {
            int k = nStreets > 1 ? RAND(nStreets - 1) : 0, y;
            int ph = 4;
            if (terraceRow >= 0)
                k = 1;
            pw = 6 + RAND(3);
            // In the woods, a small glade.
            if (gen->forestTown)
            {
                pw = 5;
                ph = 3;
            }
            plazaY = streetY[k];
            if (plazaY + ph - 1 > gen->ch - 2)
                plazaY = gen->ch - 1 - ph;
            plazaX = (gen->cw - pw) / 2 - 1 + RAND(3);
            for (y = plazaY; y < plazaY + ph; y++)
                for (x = plazaX; x < plazaX + pw; x++)
                    AddRoadCell(gen, x, y);
        }
    }
    cx = woodsLayout ? hx : gen->cw / 2 - 1 + RAND(3);
    cy = streetY[0];

    if (summit)
    {
        // The League stands at the top left or right, with its front step
        // and road below. The cave comes out of a mesa beside it.
        bool8 left = RAND(2);
        int lx = left ? 1 : gen->cw - 1 - LEAGUE_UW, y;
        for (y = 0; y < LEAGUE_UH; y++)
            for (x = lx; x < lx + LEAGUE_UW; x++)
                SetCell(gen, x, y, C_BLDG);
        // Its front step takes the top row of the units below: no trees.
        for (x = lx; x < lx + LEAGUE_UW; x++)
            SetCell(gen, x, LEAGUE_UH, C_KEEP);
        CarveRoad(gen, lx + LEAGUE_DOOR_X / 2, LEAGUE_UH, lx + LEAGUE_DOOR_X / 2, streetY[0]);
        gen->leagueX = lx;
        if (left)
            PlaceMesaAt(gen, lx + LEAGUE_UW + 1, gen->cw - 2 - (lx + LEAGUE_UW), &ix, &iy);
        else
            PlaceMesaW(gen, lx - 2, &ix, &iy);
        CarveRoad(gen, ix, iy, ix, streetY[0]);
    }
    else
    {
        if (p->mouth != 0xFF && woodsLayout)
        {
            // Each way in is path from the start, so no lot takes it.
            AddRoadCell(gen, ix, iy);
            exitX[nExits] = ix;
            exitY[nExits++] = iy;
        }
        else if (p->mouth != 0xFF)
            CarveRoad(gen, ix, iy, ix, streetY[0]);
        if (entrance == RENTRY_MESA)
            CarveRoad(gen, dunX, dunY, dunX, streetY[0]);
        // (A cave's mines take the cave's own way in, carved above.)
        if (st.kind == 1 && !st.small)
            CarveRoad(gen, st.lx, st.ly, st.lx, streetY[0]);
        for (i = 0; i < p->nLinks; i++)
        {
            const struct RogueLink *l = &p->links[i];
            int best = 0, s, tx, ty;
            MakeOpening(gen, l->dir, &ix, &iy);
            // The way in from the start of the road: the link to the
            // place with the lowest number.
            if (l->target < entryTarget)
            {
                entryTarget = l->target;
                entryX = ix;
                entryY = iy;
            }
            // A connected map is drawn with this map's tileset. A route next
            // door with another town's tileset needs a clear view.
            // After 40 tries buildings may stand there (seamLots), but the
            // ground, roads, fences and props still keep to the General
            // tileset.
            gen->keepOut[l->dir] = r->places[l->target].theme != p->theme;
            gen->seamLots = gen->attempt >= (SigTownTries(r, gen->m->place) ? SEAM_LOTS_SIG_TRY : SEAM_LOTS_TRY);
            if (woodsLayout)
            {
                if (nExits < 6)
                {
                    AddRoadCell(gen, ix, iy);
                    exitX[nExits] = ix;
                    exitY[nExits++] = iy;
                }
                continue;
            }
            // Join the nearest street.
            for (s = 1; s < nStreets; s++)
                if (Abs(streetY[s] - iy) < Abs(streetY[best] - iy))
                    best = s;
            ty = streetY[best];
            tx = ix < x0 ? x0 : (ix > x1 ? x1 : ix);
            if (l->dir == RDIR_NORTH || l->dir == RDIR_SOUTH)
            {
                CarveRoad(gen, ix, iy, ix, ty);
                CarveRoad(gen, ix, ty, tx, ty);
            }
            else
            {
                CarveRoad(gen, ix, iy, tx, iy);
                CarveRoad(gen, tx, iy, tx, ty);
            }
        }
    }

    // Some towns stand on two levels, as Lilycove, Lavaridge and Fallarbor
    // do. The League stands on a terrace above the rest of the summit,
    // reached by a flight of steps.
    if (summit)
    {
        u8 tops[MAX_CW];
        int high = LEAGUE_UH + 1 + RAND(2);
        for (x = 0; x < gen->cw; x++)
            tops[x] = (x >= gen->leagueX - 1 && x <= gen->leagueX + LEAGUE_UW) ? high : 0;
        tops[0] = tops[1];
        tops[gen->cw - 1] = tops[gen->cw - 2];
        if (!TryTerrace(gen, tops, cx, cy) && high > LEAGUE_UH + 1)
        {
            for (x = 0; x < gen->cw; x++)
                if (tops[x])
                    tops[x] = LEAGUE_UH + 1;
            TryTerrace(gen, tops, cx, cy);
        }
    }
    else if (terraceRow >= 0)
    {
        u8 tops[MAX_CW];
        for (x = 0; x < gen->cw; x++)
            tops[x] = terraceRow;
        // A lane by the side or in view of a seam can't take steps there:
        // try the other shapes.
        if (!TryTerrace(gen, tops, cx, cy))
            PlaceTerrace(gen, top - 1, gen->ch - 6, cx, cy);
    }
    else if (wantTerrace)
    {
        PlaceTerrace(gen, top - 1, gen->ch - 6, cx, cy);
    }

    // A hamlet by a pond has a big one, before the houses.
    if (hamletStyle == HAMLET_POND && th->ground == TGROUND_GRASS)
        PlaceLake(gen, 4 + RAND(2), 2, TRUE);

    // A spring town has its red rock before anything else is built.
    // (A hot spring town has Lavaridge's own corner of cliffs instead.)
    if (p->theme == RTHEME_SPRING && !summit && st.kind != 1 && st.sig != RSIG_HOT_SPRING)
        PlaceSpringRock(gen, cx, cy);
    // The town's signature lots that need room most: the hot spring under
    // that rock, a city's market.
    if (!SigBeforeBuildings(gen, &st, cx, cy) && gen->attempt < SigGiveUp(gen->r, gen->m->place))
        return FALSE;

    // Buildings. The first town has the professor's lab, gym towns a gym, and
    // the summit the League hall. Then one house you can enter and a few
    // more, more in cities.
    // (A harbor city on grass has the plain buildings: Dewford's stand on
    // sand.)
    // The dungeon's building first, while the streets have room for it,
    // unless it is the Game Corner a Verdant city's square already has
    // (below).
    if (dunB && !(entrance == RENTRY_GAME_CORNER && plazaY >= 0 && city && p->theme == RTHEME_VERDANT && pw >= 6))
    {
        doors[nList] = DOOR_DUNGEON;
        list[nList++] = dunB;
    }
    if (city)
    {
        doors[nList] = RWARP_GYM;
        list[nList++] = (th->gym && !harborCity) ? th->gym : &sGym;
    }
    if (p->type == RTOWN_START)
    {
        doors[nList] = DOOR_LAB;
        list[nList++] = &sLab;
    }
    doors[nList] = RWARP_PC;
    list[nList++] = (th->pc && !harborCity) ? th->pc : &sPokemonCenter;
    if (!hamlet)
    {
        doors[nList] = RWARP_MART;
        list[nList++] = (th->mart && !harborCity) ? th->mart : &sMart;
    }
    // (v1.1 sea: a town that has failed SEA_LONE_LANDMARK times over, with a
    // landmark you enter, lets the landmark stand in for its house: seed
    // 153's village under a lair's hill, a plan the second stretch of sea
    // made, had no room for both. No town of seeds 1 to 200 before v1.1
    // took more than 63 tries.)
    if (!summit && !(sigB && SigEnterable(st.sig) && !(p->interiors & (1 << RINT_NAME_RATER))
                     && gen->attempt >= SEA_LONE_LANDMARK))
    {
        doors[nList] = RWARP_HOUSE;
        list[nList] = &th->houses[RAND(th->nHouses)];
        // The town given the Name Rater always builds his house when its
        // theme has one.
        for (i = 0; i < th->nHouses && (p->interiors & (1 << RINT_NAME_RATER)); i++)
            if (InteriorOf(&th->houses[i]) == RINT_NAME_RATER)
                list[nList] = &th->houses[i];
        nList++;
    }
    // The signature landmark, first after the dungeon's way in, while the
    // streets have room for it.
    if (sigB && nList < 10)
    {
        int at = (dunB && doors[0] == DOOR_DUNGEON) ? 1 : 0;
        for (i = nList; i > at; i--)
        {
            doors[i] = doors[i - 1];
            list[i] = list[i - 1];
        }
        doors[at] = sigB->door == DOOR_SIGLOT ? DOOR_SIGLOT : DOOR_SIGNATURE;
        list[at] = sigB;
        nList++;
    }
    nRequired = nList;
    if (city && th->nBig)
    {
        doors[nList] = DOOR_LOCKED;
        list[nList] = &th->big[RAND(th->nBig)];
        // Not a second, shut Game Corner beside the one that is the way in.
        if (list[nList] != dunB)
            nList++;
        else if (th->nBig > 1)
            list[nList++] = &th->big[0];
    }
    // A plain house as the way in comes first among the extras.
    if (entrance != RENTRY_NONE && entrance != RENTRY_MESA && dunB == NULL && nList < 10)
    {
        doors[nList] = DOOR_DUNGEON;
        list[nList++] = &th->houses[RAND(th->nHouses)];
    }
    if (!summit)
    {
        int extra = city ? 3 + RAND(2) : (hamlet ? 1 : 2 + RAND(2)); // a hamlet: the move tutor and a gift
        for (i = 0; i < extra && nList < 10; i++)
        {
            doors[nList] = RWARP_HOUSE;
            list[nList++] = &th->houses[RAND(th->nHouses)];
        }
    }
    if (entryX < 0)
    {
        entryX = cx;
        entryY = cy;
    }
    // Mauville's Game Corner stands in the middle of a Verdant city's
    // square, with the square all round it.
    if (plazaY >= 0 && city && p->theme == RTHEME_VERDANT && pw >= 6)
    {
        const struct Building *gc = &th->big[1];
        int bx = plazaX + (pw - gc->uw) / 2, y;
        for (y = plazaY + 1; y < plazaY + 1 + gc->uh; y++)
            for (x = bx; x < bx + gc->uw; x++)
                SetCell(gen, x, y, C_OPEN);
        if (PlaceBuildingAt(gen, gc, bx, plazaY + 1, &placed[nPlaced]))
        {
            placed[nPlaced++].door = gc == dunB ? DOOR_DUNGEON : DOOR_LOCKED;
            centreDone = TRUE;
            // The town's other landmark is the bike shop.
            for (i = 0; i < nList; i++)
                if (list[i] == gc && doors[i] != DOOR_DUNGEON)
                    list[i] = &th->big[0];
        }
        else
        {
            for (y = plazaY + 1; y < plazaY + 1 + gc->uh; y++)
                for (x = bx; x < bx + gc->uw; x++)
                    SetCell(gen, x, y, C_ROAD);
            // The way into the hideout must stand.
            if (gc == dunB)
                return FALSE;
        }
    }
    if (woodsLayout)
    {
        if (!PlaceWoodsBuildings(gen, list, doors, nList, nRequired, placed, &nPlaced, minX, maxX, minY - 3, maxY,
                                 entryX, entryY)
         || !JoinWoods(gen, placed, nPlaced, exitX, exitY, nExits, hx, hy))
            return FALSE;
    }
    else
    {
        // Where the gym stands: facing the plaza, up on the terrace, at the far
        // end of the main road from the way in, or anywhere.
        gymLot = RAND(3) ? LOT_FAR : LOT_ANY;
        if (gen->terraced && CHANCE(60))
            gymLot = LOT_UPPER;
        for (i = 0; i < nList; i++)
        {
            bool8 ok = FALSE;
            u8 pref = LOT_ANY;
            // The gym, Pokemon Center and Mart face the plaza if they fit.
            if (plazaY >= 0 && i < nRequired && doors[i] != RWARP_HOUSE && (doors[i] == RWARP_GYM || CHANCE(70)))
                ok = PlaceFacing(gen, list[i], plazaY, plazaX, plazaX + pw - 1, &placed[nPlaced]);
            if (doors[i] == RWARP_GYM)
                pref = gymLot;
            else if ((doors[i] == RWARP_PC && CHANCE(75)) || (doors[i] == RWARP_MART && gen->forestTown))
                pref = LOT_NEAR;
            // Treehouses in a row, so walkways can join them.
            else if (gen->forestTown && doors[i] == RWARP_HOUSE && lastHouse >= 0 && CHANCE(70))
            {
                const struct Placed *h = &placed[lastHouse];
                if (PlaceOnStreetPref(gen, list[i], streetY, nStreets, &placed[nPlaced], LOT_NEAR, h->bx + 5, h->by + h->b->uh))
                    ok = TRUE;
            }
            // The Space Center's rocket may rise into the woods along the top.
            if (!ok && doors[i] == DOOR_SIGNATURE && list[i]->uh >= 6)
                ok = SigPlaceTall(gen, list[i], streetY, nStreets, top, &placed[nPlaced]);
            if (!ok && pref != LOT_ANY)
                ok = PlaceOnStreetPref(gen, list[i], streetY, nStreets, &placed[nPlaced], pref, entryX, entryY);
            if (!ok)
                ok = PlaceOnStreet(gen, list[i], streetY, nStreets, &placed[nPlaced]);
            // Fall back to anywhere with a path to the road, for the
            // buildings the town needs (a house as the way in too) and,
            // while it looks empty, a house.
            if (!ok && (i < nRequired || doors[i] == DOOR_DUNGEON
                        || (!summit && !hamlet && nPlaced < (city ? 5 : 4) - (top > 4) - gen->terraced)))
                ok = PlaceBuilding(gen, list[i], &placed[nPlaced]);
            if (ok && doors[i] == RWARP_HOUSE)
                lastHouse = nPlaced;
            if (ok)
                placed[nPlaced++].door = doors[i];
            else if (i < nRequired)
            {
#ifdef ROGUE_HOST
                if (getenv("SIGDBG"))
                    fprintf(stderr, "place %d attempt %d: no room for building %d (door %d, %dx%d), streets %d at %d\n", gen->m->place,
                            gen->attempt, i, doors[i], list[i]->uw, list[i]->uh, nStreets, streetY[0]);
#endif
                return FALSE;
            }
        }
    }
    // Else a plain house is the way in; failing that one of the town's
    // other houses (not the first, with the gift).
    if (entrance != RENTRY_NONE && entrance != RENTRY_MESA && dunB == NULL)
    {
        int first = -1, last = -1, have = 0;
        for (i = 0; i < nPlaced; i++)
        {
            have |= placed[i].door == DOOR_DUNGEON;
            if (placed[i].door != RWARP_HOUSE)
                continue;
            if (first < 0)
                first = i;
            else
                last = i;
        }
        // A cramped town with one house gives it up at last (later in the
        // Name Rater's town, whose home it may be).
        if (!have && last < 0 && (first < 0 || gen->attempt < ((p->interiors & (1 << RINT_NAME_RATER)) ? 58 : 45)))
            return FALSE;
        if (!have)
            placed[last >= 0 ? last : first].door = DOOR_DUNGEON;
    }
    // A town should look lived in. Try another layout if too few houses fit,
    // unless this map has already been tried many times.
    // (Buildings keep a unit apart, so a cramped city settles for one fewer
    // after a few tries.)
    if (gen->attempt < 30 && !summit && !hamlet
     && nPlaced < (city ? (gen->attempt < 4 ? 6 : 5) : 4) - (top > 4) - gen->terraced)
        return FALSE;
    JoinRoadGaps(gen);
    // Treehouses side by side on a street are joined by a raised walkway
    // through the trees between them, as in Fortree.
    for (i = 0; i < nPlaced && gen->forestTown; i++)
    {
        int j;
        for (j = 0; j < nPlaced && nWalks < 3; j++)
        {
            const struct Placed *a = &placed[i], *b = &placed[j];
            int gap = b->bx - (a->bx + a->b->uw), y;
            bool8 ok = a->b->frontY && b->b->frontY && a->by == b->by && gap >= 1 && gap <= 3;
            for (y = a->by; y < a->by + a->b->uh && ok; y++)
                for (x = a->bx + a->b->uw; x < b->bx; x++)
                    if (Cell(gen, x, y) != C_OPEN && Cell(gen, x, y) != C_TREE)
                        ok = FALSE;
            if (!ok)
                continue;
            for (y = a->by; y < a->by + a->b->uh; y++)
                for (x = a->bx + a->b->uw; x < b->bx; x++)
                    SetCell(gen, x, y, C_TREE);
            walkA[nWalks] = i;
            walkB[nWalks++] = j;
        }
    }
    gen->fence[0] = th->fence[0];
    gen->fence[1] = th->fence[1];
    gen->fence[2] = th->fence[2];
    AddCurbFences(gen, streetY, nStreets, cx, cy);
    if (plazaY >= 0 && !centreDone)
        PlaceCentrepiece(gen, th, p->theme, plazaX + (pw - 3) / 2, plazaY + 1);
    // The rest of the signature's lots: a pasture, berry plots, flower beds.
    if (!summit && !SigAfterBuildings(gen, &st, th, cx, cy) && gen->attempt < SigGiveUp(gen->r, gen->m->place))
        return FALSE;

    // A landmark pond in some towns, the theme's decorations along the
    // streets, then yards and trees in whatever lawn is left.
    // (A pond's grassy banks would look wrong on sand or ash.)
    if (th->ground == TGROUND_GRASS && !hamlet && CHANCE(55))
        PlaceLake(gen, 2 + RAND(2), 1 + RAND(2), TRUE);
    if (p->theme == RTHEME_ASH)
    {
        // Fallarbor: none to three craters anywhere on the ash, and
        // boulders.
        int n = RAND(4);
        for (i = 0; i < n; i++)
            PlacePropOnce(gen, &th->decor[0], cx, cy, TRUE, 2);
        n = 1 + RAND(4);
        for (i = 0; i < n; i++)
            PlacePropOnce(gen, &th->decor[1], cx, cy, CHANCE(50), 2);
    }
    else
    {
        PlaceProps(gen, th, city ? 5 + RAND(3) : 3 + RAND(2), cx, cy);
    }
    GrowTownTrees(gen);
    // Island trees in deep groves against the edge of the sand instead of
    // the lone trees of a lawn.
    if (gen->townGround == TGROUND_SAND)
        GrowTownTrees(gen);
    // A spring town with no room for its spring gets a flower bed instead.
    FurnishTown(gen, cx, cy, (plazaY < 0 && (p->theme == RTHEME_SPRING || hamletStyle == HAMLET_GARDEN || CHANCE(sBedChance[p->theme]))) ? 1 : 0);
    FillOpenLawn(gen, cx, cy);

    JoinRoadGaps(gen);
    // A forest town is woods but for its paths and the verges beside them,
    // and so is a hamlet in a grove.
    if (gen->forestTown || hamletStyle == HAMLET_GROVE)
    {
        int gx, gy;
        for (gy = 1; gy < gen->ch - 1; gy++)
            for (gx = 1; gx < gen->cw - 1; gx++)
                if (Cell(gen, gx, gy) == C_OPEN && Cell(gen, gx, gy - 1) != C_ROCK && Cell(gen, gx, gy - 1) != C_DECOR
                 && !IsBeachU(gen, gx, gy)
                 && !(gen->townGround != TGROUND_GRASS && Cell(gen, gx, gy - 1) == C_BLDG) // as under Fallarbor's crater
                 && (!HasNeighbor(gen, gx, gy, C_ROAD, FALSE) || CHANCE(woodsLayout ? 70 : 55)))
                    SetCell(gen, gx, gy, C_TREE);
    }
    if (gen->islandTrees)
        ThickenBorder(gen, cx, cy);
    if (gen->islandTown)
        ShapeIslandTown(gen, cx, cy);
    else if (gen->islandTrees && p->layout == RLAYOUT_HARBOR && p->seaSide != RDIR_NONE)
        PlaceBeachGroves(gen, cx, cy);

    // Roads: the theme's paving, or crisp sand in cities and worn grass paths
    // that widen into a small plaza in villages.
    if (th->roadTL)
    {
        // Near an exit toward another tileset the road turns to sand (see
        // below): a street's dead end there would be a patch of sand going
        // nowhere, so it stops short instead.
        PruneSeamStubs(gen);
        gen->roadTL = th->roadTL;
        gen->roadCentre = th->roadCentre;
        for (i = 0; i < gen->nPath; i++)
        {
            int px = gen->path[i] % gen->cw, py = gen->path[i] / gen->cw;
            // Near the exit toward a route drawn with another tileset the
            // paving stops, and the way out is grass, as Hoenn's paving
            // never meets a sand path (the route's sand stops short too).
            // An island town's road is sand, which looks the same there.
            if (!InSeamView(gen, px, py, 1, 1))
                PaintUnit(gen, px, py, V_ROAD);
            else if (th->roadTL == B_SAND_TL)
                PaintUnit(gen, px, py, V_SAND);
        }
    }
    else if (city)
    {
        for (i = 0; i < gen->nPath; i++)
            PaintUnit(gen, gen->path[i] % gen->cw, gen->path[i] / gen->cw, V_SAND);
    }
    else
    {
        for (i = 0; i < gen->nPath; i++)
            PaintWornUnit(gen, gen->path[i] % gen->cw, gen->path[i] / gen->cw);
        PaintBlob(gen, cx * 2 + 1, cy * 2 + 1, 4, V_PATH, FALSE);
    }
    TidyPaths(gen);
    if (p->layout == RLAYOUT_HARBOR)
        PaintBeach(gen);
    // On a sand town the ground where the sea ends is beach, not a grassy
    // bank.
    if (gen->townGround == TGROUND_SAND)
    {
        int gx, gy;
        ShoreUnderSea(gen);
        for (gy = 0; gy < gen->ch; gy++)
            for (gx = 0; gx < gen->cw; gx++)
                if (Cell(gen, gx, gy) == C_SHORE)
                    SetCell(gen, gx, gy, C_OPEN);
    }
    if (gen->townGround != TGROUND_GRASS)
        CoverTownGround(gen);

    if (p->type == RTOWN_START)
    {
        // A spot on the main street, kept clear (before the signs by the
        // houses): the town's fallback for Fly and the heal location. The
        // run starts in the professor's lab, where he waits now; the block
        // beside it, where he stood until v0.11.0, stays clear too, so the
        // rest of the town is laid out as before.
        m->startX = cx * 2;
        m->startY = cy * 2 + 1;
        MarkOccupied(gen, cx * 2 + 1, cy * 2 + 1);
        MarkOccupied(gen, m->startX, m->startY);
    }
    FloodReachable(gen, cx, cy);
    {
        // The town sign beside a road comes first, so the signs by the
        // houses never take its place. Small towns like the summit run out
        // of room: stand closer.
        int tries, fx, fy;
        u8 facing;
        for (tries = 0; tries < 400; tries++)
        {
            int pi = RAND(gen->nPath);
            gen->crowdRadius = (tries >= 200) ? 2 : 0;
            if (FindSignSpot(gen, gen->path[pi] % gen->cw, gen->path[pi] / gen->cw, &fx, &fy, &facing))
            {
                AddSign(gen, fx, fy, RSIGN_TOWN);
                break;
            }
        }
        gen->crowdRadius = 0;
        // A hamlet shut in by woods may have no open ground beside its road:
        // the sign stands at the edge of the road itself, against the trees,
        // where it closes no way and stands clear of the doors.
        for (i = 0; tries == 400 && i < gen->nPath * 4; i++)
        {
            int k, ok;
            fx = (gen->path[i / 4] % gen->cw) * 2 + (i & 1);
            fy = (gen->path[i / 4] / gen->cw) * 2 + ((i >> 1) & 1);
            // (Never in a way out at the map's edge.)
            ok = !IsOccupied(gen, fx, fy) && !BlocksWay(gen, fx, fy) && !UnderCrown(gen, fx, fy) && !ByLedgeF(gen, fx, fy)
              && !IsEdge(gen, fx >> 1, fy >> 1)
              && (Cell(gen, fx >> 1, (fy - 1) >> 1) == C_TREE || Cell(gen, (fx - 1) >> 1, fy >> 1) == C_TREE
               || Cell(gen, (fx + 1) >> 1, fy >> 1) == C_TREE);
            for (k = 0; k < nPlaced && ok; k++)
            {
                int dx = fx - (placed[k].bx * 2 + placed[k].b->doorX);
                int dy = fy - (placed[k].by * 2 + placed[k].b->sy + FrontY(placed[k].b));
                if (dx >= -1 && dx <= 1 && dy >= 0 && dy <= 1)
                    ok = FALSE;
            }
            if (ok)
            {
                AddSign(gen, fx, fy, RSIGN_TOWN);
                break;
            }
        }
        // No room for it at all: the town is made again.
        if (tries == 400 && i == gen->nPath * 4 && gen->attempt < 50)
            return FALSE;
    }
    // A sign beside the door in the rock, naming the tower or hideout.
    if (entrance == RENTRY_MESA)
    {
        int tries, fx, fy, signs = gen->m->nSigns, mouthX = dunMouthX >= 0 ? dunMouthX : gen->mouthX;
        // (The ground at the foot of the volcano's mountain is below its
        // terrace's face, v1.1.)
        int foot = (gen->caveHill & 0x40) ? VOLCANO_HILL_H : 6;
        u8 facing;
        for (tries = 0; tries < 60; tries++)
        {
            if (FindSignSpot(gen, dunX + (int)RAND(3) - 1, dunY + (tries >= 20) + (tries >= 40), &fx, &fy, &facing)
             && Abs(fx - mouthX) + Abs(fy - foot) <= (tries >= 40 ? 9 : 6) && fy >= foot)
            {
                AddSign(gen, fx, fy, RSIGN_DUNGEON);
                break;
            }
        }
        // Else on the ground at the foot of the rock beside the way in, as
        // the signs beside Hoenn's cave mouths stand.
        for (i = 0; tries == 60 && i < 96; i++)
        {
            // Nearest first: a step or a few to either side, the row in
            // front of the face, then the rows below (further out on a
            // crowded street).
            int d = i < 48 ? 1 + (i >> 1) % 6 : 1 + (i >> 1) % 8;
            fx = mouthX + ((i & 1) ? d + 1 : -d);
            fy = i < 48 ? foot + i / 12 : foot + (i - 48) / 16;
            if (InMap(gen, fx, fy) && (Cell(gen, fx >> 1, fy >> 1) == C_OPEN || Cell(gen, fx >> 1, fy >> 1) == C_KEEP)
             && !IsOccupied(gen, fx, fy) && !PathCoverF(gen, fx, fy) && !BlocksWay(gen, fx, fy))
            {
                AddSign(gen, fx, fy, RSIGN_DUNGEON);
                break;
            }
        }
        // With no room for it, the town is made again: the way in would
        // be a door in the rock nobody can tell is there.
        if (gen->m->nSigns == signs && gen->attempt < 50)
            return FALSE;
    }
    for (i = 0; i < nPlaced; i++)
    {
        int house = AddBuildingEvents(gen, &placed[i]);
        if (house >= 0)
            houseOf[house] = i;
    }
    AddHouseSigns(gen, placed, houseOf);
    ScatterFlowers(gen, city ? 4 : (hamletStyle == HAMLET_GARDEN ? 24 : 8));

    // Objects and signs.
    FloodReachable(gen, cx, cy);
    // A grunt loitering by the hideout's door, as Team Aqua's hang about
    // Lilycove, so the way in is easy to find.
    {
        s32 di = RogueGen_DungeonAt(r, p - r->places);
        // (Every team's hideout has its lookout.)
        if (di >= 0 && r->dungeons[di].kind == RDUN_HIDEOUT && r->dungeons[di].host == (u8)(p - r->places))
            PlaceLookout(gen, ROBJ_GRUNT, 2);
        else if (di >= 0 && r->dungeons[di].kind == RDUN_TOWER && entrance != RENTRY_MESA && dunB == NULL)
            PlaceLookout(gen, ROBJ_GUIDE, 1);
    }
    {
        // A few townsfolk (each looks different), and a Pokemon or two out
        // for a walk.
        int tries, fx, fy, villagers = 0, mons = 0, want = city ? 5 : (hamlet ? 2 : 4);
        int wantMons = city ? 2 : 1, start = RAND(8);
        u8 facing;
        // The signature's people stand in for some of them.
        // The townsfolk come first: they tell the legends' lore and give
        // their gift. The Pokemon out for a walk give way to the trade's,
        // and the signature's people take what spots are left (below).
        wantMons -= SigMonsWanted(&st, hamlet);
        for (tries = 0; tries < 700 && (villagers < want || mons < wantMons); tries++)
        {
            int pi = RAND(gen->nPath);
            int px = gen->path[pi] % gen->cw, py = gen->path[pi] / gen->cw;
            // Small towns like the summit run out of room: stand closer,
            // and at last in view of the next map.
            gen->crowdRadius = (tries >= 400) ? 1 : (tries >= 200) ? 2 : 0;
            gen->inView = tries >= 600;
            if (!FindSpotBesideRoad(gen, px, py, &fx, &fy, &facing))
                continue;
            // In a crowded town the townsfolk come first: they tell the
            // legends' lore (the signature's people took a spot or two).
            if (mons < wantMons && (villagers >= want || st.sig == RSIG_NONE))
            {
                AddObj(gen, ROBJ_MON, fx, fy, facing, mons, 0);
                mons++;
            }
            else if (villagers < want)
            {
                AddObj(gen, ROBJ_VILLAGER, fx, fy, facing, (start + villagers * 3) & 7, 0);
                villagers++;
            }
            else
            {
                AddObj(gen, ROBJ_MON, fx, fy, facing, mons, 0);
                mons++;
            }
        }
    }
    gen->crowdRadius = 0;
    gen->inView = FALSE;
    SigPeople(gen, &st, placed, nPlaced, &sigMons);
    PairReporters(gen); // (after everyone else, so nobody else moves)
    // A fishing town whose fishers found nowhere by the water is made again.
    if (st.sig == RSIG_FISHING && st.kind != 5 && gen->attempt < SigGiveUp(gen->r, gen->m->place))
        return FALSE;
    // Gates hold the player back until they have a starter, or this town's
    // badge, on the way on along the main road.
    for (i = 0; i < p->nLinks; i++)
    {
        if (p->links[i].gate)
            AddGate(gen, p->links[i].dir, p->links[i].gate);
        if (p->links[i].gate == ROGUE_GATE_VILLAIN)
            AddBlockade(gen, p->links[i].dir);
    }

    if (summit)
        AddWarp(gen, gen->leagueX * 2 + LEAGUE_DOOR_X, LEAGUE_DOOR_Y, RWARP_GYM);
    // Under the rocky route's cliff the top corners are a step of ground,
    // shut in by the rock and the trees, so the corner trees show their
    // tips below the cliff's foot instead of a crown cut flat against it.
    if (!summit && p->mouth == 0xFF)
    {
        const struct RogueLink *l = LinkOnSide(p, RDIR_NORTH);
        // Hills keep a cliff to the seam above a town (EdgeBandSide).
        if (l && RockyRoute(&r->places[l->target]))
            for (x = 0; x < gen->cw; x += gen->cw - 1)
                if (Cell(gen, x, 0) == C_TREE && x * 2 - l->offset >= 0
                 && x * 2 + 1 - l->offset < r->places[l->target].w)
                    SetCell(gen, x, 0, C_OPEN);
    }

    TidyGround(gen);
    Render(gen);
    if (p->mouth == 0xFF)
        RenderTopRock(gen);
    for (i = 0; i < nPlaced; i++)
    {
        Stamp(gen, placed[i].b, placed[i].bx, placed[i].by);
        ShutLandmarkDoor(gen, &placed[i]);
    }
    // A Dewford pier out into the sea from the beach, in a beach town with
    // the sea to the east (the pier's land end faces west).
    SigRender(gen, &st);
    if (p->theme == RTHEME_BEACH && p->layout == RLAYOUT_HARBOR && p->seaSide == RDIR_EAST && st.sig != RSIG_FISHING)
    {
        int tries;
        for (tries = 0; tries < 20; tries++)
        {
            int uy = 2 + RAND(gen->ch - 4), d = gen->seaDepth[uy], fx = (gen->cw - d) * 2, len, k;
            if (d < 2 || gen->seaDepth[uy - 1] < 2 || gen->seaDepth[uy + 1] < 2
             || Cell(gen, gen->cw - 1 - d, uy) != C_OPEN || !IsBeachU(gen, gen->cw - 1 - d, uy)
             || InSeamView(gen, gen->cw - d, uy, d, 1))
                continue;
            len = 3 + RAND(3);
            if (len > d * 2 - 1)
                len = d * 2 - 1;
            for (k = 0; k < len; k++)
            {
                Put(gen, fx + k, uy * 2, k == 0 ? 0x3228 : 0x3229);
                Put(gen, fx + k, uy * 2 + 1, k == 0 ? 0x0630 : 0x0631);
            }
            break;
        }
    }
    // Each treehouse's thicket fills the last column of its lot, and a
    // walkway runs from platform to platform: planks over their posts.
    for (i = 0; i < nPlaced && gen->forestTown; i++)
    {
        int y, bx = placed[i].bx * 2 + 5, by = placed[i].by * 2;
        if (!placed[i].b->frontY)
            continue;
        for (y = 0; y < 6; y++)
            Put(gen, bx, by + y, y == 5 ? 0x0417 : ((bx + y) & 1 ? 0x04C7 : 0x04C6));
    }
    JoinWoodsOnRound(gen);
    TidyTownWoods(gen);
    for (i = 0; i < nWalks; i++)
    {
        const struct Placed *a = &placed[walkA[i]], *b = &placed[walkB[i]];
        int y = a->by * 2 + 3;
        Put(gen, a->bx * 2 + 4, y, 0x423F);
        Put(gen, b->bx * 2, y, 0x423B);
        for (x = a->bx * 2 + 5; x < b->bx * 2; x++)
        {
            Put(gen, x, y, 0x4256);
            Put(gen, x, y + 1, 0x0644);
        }
    }
    if (summit)
    {
        int y;
        for (y = 0; y < 7; y++)
            for (x = 0; x < 11; x++)
            {
                // Ever Grande's own grass is a lighter green than the
                // town's: plain grass instead.
                u16 b = sStamp_League[y * 11 + x];
                Put(gen, gen->leagueX * 2 + x, y, b == 0x3222 ? B_GRASS : b);
            }
    }
    SigFinish(gen, &st, placed, nPlaced);
    RenderSigns(gen);
    // A special house's sign must be read from ground the player can walk
    // to, on the drawn blocks: a bank drawn narrower than its units can cut
    // off the ground beside it. Else the town is made again.
    if (gen->attempt < 50 && !HouseSignsReadable(gen))
        return FALSE;
    // Everyone and every door can be walked up to, on the drawn blocks with
    // everyone standing where they stand: two townsfolk side by side can
    // shut a lane, and a signature's people (a pier's fishers) can stand on
    // ground the drawing cut off. Else the town is made again.
    if (gen->attempt < 60 && !TownReachable(gen))
        return FALSE;
    // (v1.1 sea: and its ways in join: src/rogue_gen_sea.h.)
    if (gen->attempt < 60 && !SeaTownExitsJoin(gen))
        return FALSE;
    return TRUE;
}

// ---------------------------------------------------------------------------
// Routes

// Direction from path cell i to the next one (or from the previous one at the
// end of the path).
static u8 PathDir(struct Gen *gen, int i)
{
    int a = i, b = i + 1;
    int ax, ay, bx, by;
    u8 d;
    YIELD();
    if (b >= gen->nPath)
    {
        a = i - 1;
        b = i;
    }
    if (a < 0)
        return RDIR_EAST;
    ax = gen->path[a] % gen->cw; ay = gen->path[a] / gen->cw;
    bx = gen->path[b] % gen->cw; by = gen->path[b] / gen->cw;
    for (d = 0; d < 4; d++)
        if (ax + sDirDX[d] == bx && ay + sDirDY[d] == by)
            return d;
    return RDIR_EAST;
}

static void OpenUnit(struct Gen *gen, int x, int y)
{
    if (Cell(gen, x, y) == C_TREE && !IsEdge(gen, x, y) && !BitU(gen->keep, gen, x, y))
        SetCell(gen, x, y, C_OPEN);
}

// The way through a route, cut out of the forest in stretches of different
// widths: a squeeze between trees, a lane with trees on one side, a lane
// with room on both sides, or a small field. Real routes change like this
// every few steps; one even corridor feels empty.
enum { SEG_NARROW, SEG_SIDE, SEG_BOTH, SEG_FIELD };

static void CarveSegments(struct Gen *gen)
{
    int i = 0, j, x, y;
    while (i < gen->nPath)
    {
        int len = 3 + RAND(5);
        u32 roll = RAND(100);
        u8 mode = roll < 18 ? SEG_NARROW : (roll < 55 ? SEG_SIDE : (roll < 82 ? SEG_BOTH : SEG_FIELD));
        // Meadows open out into wide fields most of the way.
        if (gen->p->type == RROUTE_FIELD)
            mode = roll < 8 ? SEG_SIDE : (roll < 35 ? SEG_BOTH : SEG_FIELD);
        // A canyon's floor is wide, sand from wall to wall (rogue_gen_rock.h).
        else if (gen->p->type == RROUTE_DESERT && gen->rocky)
            mode = roll < 6 ? SEG_SIDE : (roll < 55 ? SEG_BOTH : SEG_FIELD);
        // (v1.1c land2) A mountain pass's way is wide: less rock, more of
        // Route 114's and 115's open ground between the masses.
        else if (gen->p->type == RROUTE_HILLS)
            mode = roll < 10 ? SEG_NARROW : (roll < 30 ? SEG_SIDE : (roll < 70 ? SEG_BOTH : SEG_FIELD));
        u8 side = RAND(2);
        if (i < 2 || i + len >= gen->nPath - 1)
            mode = SEG_BOTH;
        for (j = i; j < i + len && j < gen->nPath; j++)
        {
            int px = gen->path[j] % gen->cw, py = gen->path[j] / gen->cw;
            u8 d = PathDir(gen, j);
            u8 left = (d + 3) & 3, right = (d + 1) & 3;
            // The way in past an obstacle stays one unit wide.
            if (gen->guardEnd && j <= gen->guardEnd)
                continue;
            switch (mode)
            {
            case SEG_SIDE:
                OpenUnit(gen, px + sDirDX[side ? left : right], py + sDirDY[side ? left : right]);
                break;
            case SEG_BOTH:
                OpenUnit(gen, px + sDirDX[left], py + sDirDY[left]);
                OpenUnit(gen, px + sDirDX[right], py + sDirDY[right]);
                break;
            case SEG_FIELD:
            {
                int r = gen->p->type == RROUTE_FIELD ? 3 + RAND(2) : 2 + RAND(2);
                for (y = py - r; y <= py + r; y++)
                    for (x = px - r; x <= px + r; x++)
                        if (Abs(x - px) + Abs(y - py) <= r)
                            OpenUnit(gen, x, y);
                break;
            }
            }
        }
        i += len;
    }
}

// Short dead ends off the road, one unit wide, that end in a little pocket.
// Items, berry trees and the odd trainer go there, the way vanilla routes hide
// a Potion behind a tree line.
static void AddSpurs(struct Gen *gen, int count)
{
    int tries;
    for (tries = 0; tries < 60 && gen->nPockets < count && gen->nPockets < MAX_POCKETS; tries++)
    {
        int p = 3 + RAND(gen->nPath > 7 ? gen->nPath - 6 : 1);
        int px = gen->path[p] % gen->cw, py = gen->path[p] / gen->cw;
        u8 d = (PathDir(gen, p) + (RAND(2) ? 1 : 3)) & 3;
        if (gen->guardEnd && p <= gen->guardEnd + 1)
            continue;
        int len = 2 + RAND(4), k, x = px, y = py;
        bool8 ok = TRUE;

        // Step out of the corridor first.
        while (IsOpenU(Cell(gen, x + sDirDX[d], y + sDirDY[d])) && ok)
        {
            x += sDirDX[d];
            y += sDirDY[d];
            if (Abs(x - px) + Abs(y - py) > 3)
                ok = FALSE;
        }
        for (k = 1; k <= len && ok; k++)
        {
            int nx = x + sDirDX[d] * k, ny = y + sDirDY[d] * k;
            u8 l = (d + 3) & 3, r = (d + 1) & 3;
            if (Cell(gen, nx, ny) != C_TREE || IsEdge(gen, nx, ny) || IsEdge(gen, nx + sDirDX[d], ny + sDirDY[d])
             || BitU(gen->keep, gen, nx, ny))
                ok = FALSE;
            // The far end stays shut, or it is no dead end.
            else if (k == len && IsOpenU(Cell(gen, nx + sDirDX[d], ny + sDirDY[d])))
                ok = FALSE;
            // Keep a tree wall on both sides so it reads as a dead end.
            else if (IsOpenU(Cell(gen, nx + sDirDX[l], ny + sDirDY[l])) || IsOpenU(Cell(gen, nx + sDirDX[r], ny + sDirDY[r])))
                ok = FALSE;
        }
        if (!ok)
            continue;
        for (k = 1; k <= len; k++)
            SetCell(gen, x + sDirDX[d] * k, y + sDirDY[d] * k, C_OPEN);
        x += sDirDX[d] * len;
        y += sDirDY[d] * len;
        gen->pocketX[gen->nPockets] = x;
        gen->pocketY[gen->nPockets] = y;
        gen->nPockets++;
    }
}

// Break up open ground: any 3x3 stretch of open units with no road in it gets
// a tree, or a pair of trees, in the middle. Keeps fields from turning into
// flat lawns.
static void BreakUpFields(struct Gen *gen)
{
    int x, y, dx, dy;
    for (y = 2; y < gen->ch - 2; y++)
    {
        for (x = 2; x < gen->cw - 2; x++)
        {
            bool8 open = TRUE;
            for (dy = -1; dy <= 1 && open; dy++)
                for (dx = -1; dx <= 1 && open; dx++)
                    if (Cell(gen, x + dx, y + dy) != C_OPEN)
                        open = FALSE;
            if (!open || !CHANCE(70))
                continue;
            SetCell(gen, x, y, C_TREE);
            // (A canyon's lone rocks are boulders, one unit each.)
            if (CHANCE(40) && !(gen->p->type == RROUTE_DESERT && gen->rocky))
            {
                u8 d = RAND(4);
                if (Cell(gen, x + sDirDX[d], y + sDirDY[d]) == C_OPEN && !HasNeighbor(gen, x + sDirDX[d], y + sDirDY[d], C_ROAD, FALSE))
                    SetCell(gen, x + sDirDX[d], y + sDirDY[d], C_TREE);
            }
        }
    }
}

static bool8 AddDecor(struct Gen *gen, const u16 *blocks, int fx, int fy, int w, int h)
{
    struct Decor *d;
    if (gen->nDecor >= MAX_DECOR)
        return FALSE;
    d = &gen->decor[gen->nDecor++];
    d->blocks = blocks;
    d->x = fx;
    d->y = fy;
    d->w = w;
    d->h = h;
    return TRUE;
}

// TRUE if a unit lies within eight units of the opening toward a town.
static bool8 NearTownExit(struct Gen *gen, int x, int y)
{
    int i;
    for (i = 0; i < gen->p->nLinks; i++)
    {
        const struct RogueLink *l = &gen->p->links[i];
        int ex, ey;
        if (gen->r->places[l->target].kind != RPLACE_TOWN)
            continue;
        EdgeCell(gen, l->dir, l->pos / 2, 0, &ex, &ey);
        if (Abs(x - ex) + Abs(y - ey) <= 8)
            return TRUE;
    }
    return FALSE;
}

// A fenced flower bed on open ground beside the road. It must not cut off
// anything that was reachable before.
static void PlaceFenceGarden(struct Gen *gen, int startX, int startY)
{
    int tries, x, y;
    for (tries = 0; tries < 80; tries++)
    {
        int ux = 1 + RAND(gen->cw - 4), uy = 1 + RAND(gen->ch - 3);
        YIELD();
        bool8 ok = TRUE, byRoad = FALSE;
        int before;
        for (y = uy; y < uy + 2 && ok; y++)
            for (x = ux; x < ux + 3 && ok; x++)
                if (Cell(gen, x, y) != C_OPEN || IsEdge(gen, x, y) || AshZoneU(gen, x, y))
                    ok = FALSE;
        if (!ok)
            continue;
        for (y = uy - 1; y <= uy + 2; y++)
            for (x = ux - 1; x <= ux + 3; x++)
                if (Cell(gen, x, y) == C_ROAD)
                    byRoad = TRUE;
        if (!byRoad || !NearTownExit(gen, ux, uy) || !FenceRingClear(gen, ux, uy, 3, 2))
            continue;
        before = FloodReachable(gen, startX, startY);
        for (y = uy; y < uy + 2; y++)
            for (x = ux; x < ux + 3; x++)
                SetCell(gen, x, y, C_DECOR);
        if (FloodReachable(gen, startX, startY) != before - 6)
        {
            for (y = uy; y < uy + 2; y++)
                for (x = ux; x < ux + 3; x++)
                    SetCell(gen, x, y, C_OPEN);
            continue;
        }
        AddDecor(gen, sFenceGarden, ux * 2, uy * 2, 6, 4);
        return;
    }
}

// inStyle and outStyle are the road covers of the towns at each end, so the
// first steps of the route match the town the player just left.
// ---------------------------------------------------------------------------
// Trainers in the way
//
// After rendering, trainers move to block-level spots on the shortest walk
// through the route: beside it, facing across it, with a wall at their back
// and their line of sight reaching the far side, so nobody walks past them
// unseen.

// The heights people walk at: the ground (3) and doors (0), and on a
// bridge route its raised road (4, src/rogue_gen_bridge.h).
static bool8 WalkHeight(struct Gen *gen, u16 b)
{
    return (b >> 12) == 3 || (b >> 12) == 0
        || ((b >> 12) == 4 && gen->p->kind == RPLACE_ROUTE && gen->p->type == RROUTE_BRIDGE && !sGenFloor)
        // A river or pond's log bridge: land ends at 4, planks at 15.
        || (((b >> 12) == 4 || (b >> 12) == 15) && gen->p->kind == RPLACE_ROUTE && !sGenFloor);
}

// Ground you can stand on, whatever is standing there.
static bool8 StandableBlock(struct Gen *gen, int x, int y)
{
    u16 b;
    if (!InMap(gen, x, y))
        return FALSE;
    b = gen->m->blocks[y * gen->m->w + x];
    return ((b >> 10) & 3) == 0 && WalkHeight(gen, b);
}

static bool8 WalkableBlock(struct Gen *gen, int x, int y)
{
    u16 b;
    if (!InMap(gen, x, y) || IsOccupied(gen, x, y))
        return FALSE;
    b = gen->m->blocks[y * gen->m->w + x];
    // Ground is elevation 3; elevation 0 (doors, the cave mouth) can be
    // walked on from any height. Water sits at elevation 1. A route's log
    // bridge has its land ends at 4 and its planks at 15 (rogue_gen_water.h).
    return ((b >> 10) & 3) == 0 && WalkHeight(gen, b);
}

// A block a trainer's eyes pass over: walkable, but not a log bridge's land
// end or planks (elevation 4 and 15), which a trainer on the ground at 3
// cannot walk onto (rogue_gen_water.h). A bridge route's raised road is at 4
// too, and its trainers stand and look along it.
static bool8 SeeBlock(struct Gen *gen, int x, int y)
{
    u16 b;
    if (!WalkableBlock(gen, x, y))
        return FALSE;
    b = gen->m->blocks[y * gen->m->w + x];
    return (b >> 12) != 15 && ((b >> 12) != 4 || gen->p->type == RROUTE_BRIDGE);
}

// The shortest walk from the start block to the end block, into gen->walk,
// and a bit per block on it in gen->onWalk. Breadth-first search with a ring
// buffer; gen->cover keeps the direction each block was reached from.
ROGUE_FAST static bool8 FindShortestWalk(struct Gen *gen, int sx, int sy, int ex, int ey)
{
    int w = gen->m->w, h = gen->m->h, head = 0, tail = 0, n, x, y;
    u8 *from = gen->cover;
    memset(from, 0xFF, w * h);
    memset(gen->onWalk, 0, sizeof(gen->onWalk));
    from[sy * w + sx] = 4;
    sBfsQueue[tail++] = PACK(sx, sy);
    while (head < tail && from[ey * w + ex] == 0xFF)
    {
        int cur = sBfsQueue[head++ & (BFS_RING - 1)], k;
        YIELD();
        int cx = PX(cur), cy = PY(cur);
        for (k = 0; k < 4; k++)
        {
            int nx = cx + sDirDX[k], ny = cy + sDirDY[k], ni = ny * w + nx;
            if (nx < 0 || ny < 0 || nx >= w || ny >= h || from[ni] != 0xFF)
                continue;
            if (!(nx == ex && ny == ey) && !((gen->free[ni >> 3] >> (ni & 7)) & 1))
                continue;
            if (tail - head >= BFS_RING)
                return FALSE;
            from[ni] = k;
            sBfsQueue[tail++ & (BFS_RING - 1)] = PACK(nx, ny);
        }
    }
    if (from[ey * w + ex] == 0xFF)
        return FALSE;
    // Count the steps, then walk back from the end. The walk holds packed
    // coordinates.
    n = 1;
    for (x = ex, y = ey; from[y * w + x] != 4; n++)
    {
        u8 k = from[y * w + x];
        x -= sDirDX[k];
        y -= sDirDY[k];
    }
    if (n > MAX_WALK)
        return FALSE;
    gen->nWalk = n;
    for (x = ex, y = ey; n > 0; )
    {
        int i = y * w + x;
        u8 k = from[i];
        gen->walk[--n] = PACK(x, y);
        gen->onWalk[i >> 3] |= 1 << (i & 7);
        if (k == 4)
            break;
        x -= sDirDX[k];
        y -= sDirDY[k];
    }
    return TRUE;
}

static bool8 OnWalk(struct Gen *gen, int x, int y)
{
    int i = y * gen->m->w + x;
    return (gen->onWalk[i >> 3] >> (i & 7)) & 1;
}

// TRUE if a trainer at (tx, ty) seeing n blocks toward face cuts the route:
// with them and their line of sight blocked, the far end cannot be reached.
// Breadth-first search with a ring buffer; gen->cover marks visited blocks.
// Labels the walls: blocks that are not free, in 8-connected groups, into
// gen->cover. Everything beyond the map edge counts as one wall with the
// blocks along the edge (label 1). 0 is free ground; 255 means the labels ran
// out.
static inline bool8 IsWallBlock(struct Gen *gen, int i)
{
    return !((gen->free[i >> 3] >> (i & 7)) & 1);
}

// Fills one wall group from the blocks already in the queue.
ROGUE_FAST static void FillWall(struct Gen *gen, int head, int tail, u8 label)
{
    int w = gen->m->w, h = gen->m->h;
    u8 *lab = gen->cover;
    while (head < tail)
    {
        int cur = sBfsQueue[head++ & (BFS_RING - 1)], dx, dy;
        int cx = PX(cur), cy = PY(cur);
        YIELD();
        for (dy = -1; dy <= 1; dy++)
        {
            for (dx = -1; dx <= 1; dx++)
            {
                int nx = cx + dx, ny = cy + dy, ni = ny * w + nx;
                if (nx < 0 || ny < 0 || nx >= w || ny >= h || lab[ni] || !IsWallBlock(gen, ni))
                    continue;
                if (tail - head >= BFS_RING)
                    continue; // left for a later group of its own
                lab[ni] = label;
                sBfsQueue[tail++ & (BFS_RING - 1)] = PACK(nx, ny);
            }
        }
    }
}

static void LabelWalls(struct Gen *gen)
{
    int w = gen->m->w, h = gen->m->h, x, y, tail = 0;
    u8 *lab = gen->cover, label = 2;
    memset(lab, 0, w * h);
    // The edge group first, from every wall block along the edge.
    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
        {
            YIELD();
            int i = y * w + x;
            if ((x == 0 || y == 0 || x == w - 1 || y == h - 1) && IsWallBlock(gen, i) && tail < BFS_RING)
            {
                lab[i] = 1;
                sBfsQueue[tail++] = PACK(x, y);
            }
        }
    }
    FillWall(gen, 0, tail, 1);
    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
        {
            YIELD();
            int i = y * w + x;
            if (lab[i] || !IsWallBlock(gen, i))
                continue;
            lab[i] = label;
            sBfsQueue[0] = PACK(x, y);
            FillWall(gen, 0, 1, label);
            if (label < 255)
                label++;
        }
    }
}

static u8 WallLabel(struct Gen *gen, int x, int y)
{
    if (!InMap(gen, x, y))
        return 1;
    return gen->cover[y * gen->m->w + x];
}

// TRUE if a trainer at (tx, ty) seeing n blocks toward face cuts the route:
// with them and their line of sight blocked, the far end cannot be reached.
//
// Their line runs from the wall at their back to the wall they look at. If
// those are two separate walls, the way goes round. If they are the same
// wall, the line closes a loop with it, and the route is cut exactly when
// the walk from the start to the end crosses the line an odd number of
// times. Needs LabelWalls and the walk.
static bool8 CutsRoute(struct Gen *gen, int tx, int ty, u8 face, int n)
{
    u8 back = Opposite(face);
    u8 a = WallLabel(gen, tx + sDirDX[back], ty + sDirDY[back]);
    u8 b = WallLabel(gen, tx + sDirDX[face] * (n + 1), ty + sDirDY[face] * (n + 1));
    bool8 across = (face == RDIR_EAST || face == RDIR_WEST); // the line is a row
    int lo, hi, line, i, crossings = 0, before = 0;
    bool8 inside = FALSE;

    if (a == 0 || b == 0 || a != b || a == 255)
        return FALSE;
    YIELD();
    line = across ? ty : tx;
    lo = across ? (tx < tx + sDirDX[face] * n ? tx : tx + sDirDX[face] * n) : (ty < ty + sDirDY[face] * n ? ty : ty + sDirDY[face] * n);
    hi = across ? (tx > tx + sDirDX[face] * n ? tx : tx + sDirDX[face] * n) : (ty > ty + sDirDY[face] * n ? ty : ty + sDirDY[face] * n);
    for (i = 0; i < gen->nWalk; i++)
    {
        int x = PX(gen->walk[i]), y = PY(gen->walk[i]);
        int along = across ? x : y, off = across ? y : x;
        bool8 on = (off == line && along >= lo && along <= hi);
        if (on && (i == 0 || i == gen->nWalk - 1))
            return TRUE; // the trainer sees the way in or out itself
        if (on && !inside)
        {
            int px = PX(gen->walk[i - 1]), py = PY(gen->walk[i - 1]);
            before = (across ? py : px) < line ? -1 : 1;
            inside = TRUE;
        }
        else if (!on && inside)
        {
            int side = off < line ? -1 : 1;
            if (side != before)
                crossings++;
            inside = FALSE;
        }
    }
    return crossings & 1;
}

// TRUE if someone standing on block (x, y) of the drawn map would cut the
// ground around it in two: its free neighbours do not all meet round the
// ring of eight blocks. A trainer may close the way on, never a side pocket.
static bool8 RingSplits(struct Gen *gen, int x, int y)
{
    YIELD();
    static const s8 sRingX[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    static const s8 sRingY[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
    bool8 open[8];
    int k, start = -1, groups = 0, sides = 0;
    for (k = 0; k < 8; k++)
        open[k] = WalkableBlock(gen, x + sRingX[k], y + sRingY[k]);
    for (k = 0; k < 8; k += 2)
        sides += open[k];
    if (sides <= 1)
        return FALSE;
    for (k = 0; k < 8; k++)
        if (!open[k])
            start = k;
    if (start < 0)
        return FALSE;
    for (k = 1; k <= 8; k++)
    {
        int i = (start + k) & 7;
        bool8 side = FALSE;
        if (!open[i])
            continue;
        while (open[i] && k <= 8)
        {
            if (!(i & 1))
                side = TRUE;
            k++;
            i = (start + k) & 7;
        }
        groups += side;
    }
    return groups > 1;
}

// TRUE if there is water within r blocks of (x, y), for a Fisherman or a
// Swimmer to look at.
static bool8 NearWaterBlock(struct Gen *gen, int x, int y, int r)
{
    YIELD();
    int dx, dy;
    for (dy = -r; dy <= r; dy++)
        for (dx = -r; dx <= r; dx++)
            if (InMap(gen, x + dx, y + dy) && (gen->m->blocks[(y + dy) * gen->m->w + x + dx] >> 12) == 1)
                return TRUE;
    return FALSE;
}

// TRUE if a trainer already placed stands in the same line, looking the same
// way: two of them in one column facing one way read as a queue, not a route.
static bool8 LinedUp(struct Gen *gen, int x, int y, u8 face)
{
    int j;
    for (j = 0; j < gen->placing && j < gen->m->nObjs; j++)
    {
        const struct RogueObj *o = &gen->m->objs[j];
        if ((o->kind != ROBJ_TRAINER && o->kind != ROBJ_RIVAL) || (o->facing & 3) != face)
            continue;
        if ((face == RDIR_EAST || face == RDIR_WEST) ? (o->x == x && Abs(o->y - y) <= 16)
                                                     : (o->y == y && Abs(o->x - x) <= 16))
            return TRUE;
    }
    return FALSE;
}

// TRUE if (x, y) lies in the sight of a trainer or the rival already placed:
// anyone standing there would cut their line short, and the rival could then
// be walked past.
static bool8 InPlacedSight(struct Gen *gen, int x, int y)
{
    YIELD();
    int j;
    for (j = 0; j < gen->placing && j < gen->m->nObjs; j++)
    {
        const struct RogueObj *o = &gen->m->objs[j];
        u8 f;
        if (o->kind != ROBJ_TRAINER && o->kind != ROBJ_RIVAL)
            continue;
        for (f = 0; f < 4; f++)
        {
            int dx = x - o->x, dy = y - o->y, k;
            bool8 looks = f == (o->facing & 3) || (o->facing & ROBJ_LOOKS)
                       || ((o->facing & ROBJ_TURNS) && f == ((o->facing >> 2) & 3));
            if (!looks)
                continue;
            for (k = 1; k <= o->range; k++)
                if (dx == sDirDX[f] * k && dy == sDirDY[f] * k)
                    return TRUE;
        }
    }
    return FALSE;
}

// TRUE if a trainer stands within two blocks of (x, y), or two within
// four: three would stand in a huddle.
static bool8 NearTrainer(struct Gen *gen, int x, int y)
{
    int j, near = 0;
    for (j = 0; j < gen->m->nObjs; j++)
    {
        const struct RogueObj *o = &gen->m->objs[j];
        int d = Abs(o->x - x) > Abs(o->y - y) ? Abs(o->x - x) : Abs(o->y - y);
        if (o->kind != ROBJ_TRAINER && o->kind != ROBJ_RIVAL)
            continue;
        if (d <= 2 || (d <= 4 && ++near >= 2))
            return TRUE;
    }
    return FALSE;
}

// TRUE if a trainer or the rival already placed stands within two blocks
// of (x, y). Emerald spreads its route trainers out; three in a huddle read
// as a crowd. (IsCrowded keeps them further apart while its list of taken
// blocks has room; this holds on the busiest routes too.)
static bool8 NearPlaced(struct Gen *gen, int x, int y)
{
    int j;
    for (j = 0; j < gen->placing && j < gen->m->nObjs; j++)
    {
        const struct RogueObj *o = &gen->m->objs[j];
        if ((o->kind == ROBJ_TRAINER || o->kind == ROBJ_RIVAL) && Abs(o->x - x) <= 2 && Abs(o->y - y) <= 2)
            return TRUE;
    }
    return FALSE;
}

// The ways a placed trainer or the rival looks, as a bit per direction.
static u8 ObjSightMask(const struct RogueObj *o)
{
    if (o->facing & ROBJ_LOOKS)
        return 0xF;
    if (o->facing & ROBJ_TURNS)
        return (1 << (o->facing & 3)) | (1 << ((o->facing >> 2) & 3));
    return 1 << (o->facing & 3);
}

// TRUE if a trainer at (x, y) looking the ways in mask, n blocks, is safe
// to place. A trainer who sees the player walks up and then stays where they
// stopped, on any block of their line. So no block they could stop on may
// cut the ground around it in two, and their line (with their own block)
// keeps a block clear of every line and block of those placed before:
// two trainers who walk up from crossing lines could close a corridor
// between them for good.
static bool8 SightLineSafe(struct Gen *gen, int x, int y, u8 mask, int n)
{
    YIELD();
    int j, k, f;
    // Nor on or beside a log bridge's step (rogue_gen_water.h).
    if (WaterLineNearStep(gen, x, y, mask, n))
        return FALSE;
    for (f = 0; f < 4; f++)
    {
        if (!(mask & (1 << f)))
            continue;
        for (k = 1; k < n; k++)
        {
            int cx = x + sDirDX[f] * k, cy = y + sDirDY[f] * k;
            if (!SeeBlock(gen, cx, cy))
                break;
            if (RingSplits(gen, cx, cy))
                return FALSE;
        }
    }
    for (j = 0; j < gen->placing && j < gen->m->nObjs; j++)
    {
        YIELD();
        const struct RogueObj *o = &gen->m->objs[j];
        u8 om;
        int g, a, b;
        if (o->kind != ROBJ_TRAINER && o->kind != ROBJ_RIVAL)
            continue;
        om = ObjSightMask(o);
        // Every block of the new line against every block of the old one.
        for (f = -1; f < 4; f++)
        {
            if (f >= 0 && !(mask & (1 << f)))
                continue;
            for (a = (f < 0 ? 0 : 1); a <= (f < 0 ? 0 : n); a++)
            {
                YIELD();
                int cx = f < 0 ? x : x + sDirDX[f] * a, cy = f < 0 ? y : y + sDirDY[f] * a;
                // The line ends at the first wall or water.
                if (a > 0 && !StandableBlock(gen, cx, cy))
                    break;
                for (g = -1; g < 4; g++)
                {
                    if (g >= 0 && !(om & (1 << g)))
                        continue;
                    for (b = (g < 0 ? 0 : 1); b <= (g < 0 ? 0 : o->range); b++)
                    {
                        int ox = g < 0 ? o->x : o->x + sDirDX[g] * b, oy = g < 0 ? o->y : o->y + sDirDY[g] * b;
                        if (b > 0 && !StandableBlock(gen, ox, oy))
                            break;
                        if (Abs(ox - cx) <= 1 && Abs(oy - cy) <= 1)
                            return FALSE;
                    }
                }
            }
        }
    }
    return TRUE;
}

// A spot for a trainer beside the walk near step target. Returns the range.
// With needCut, only spots nobody can get past unseen. The way in front of
// them is at least minAcross blocks wide.
static int FindTrainerSpot(struct Gen *gen, int target, int *outX, int *outY, u8 *outFacing, bool8 needCut, int minRadius, int maxRadius, int maxSight, int minAcross)
{
    int radius, sign;
    for (radius = minRadius; radius < maxRadius; radius++)
    {
        YIELD();
        for (sign = -1; sign <= 1; sign += 2)
        {
            int step = target + radius * sign, k;
            int px, py;
            if (step < 4 || step < gen->walkFrom || step >= gen->nWalk - 3)
                continue;
            px = PX(gen->walk[step]);
            py = PY(gen->walk[step]);
            for (k = 0; k < 4; k++)
            {
                YIELD();
                int dist;
                for (dist = 1; dist <= 3; dist++)
                {
                    // The trainer stands dist blocks from the walk in
                    // direction k and looks back across it.
                    int tx = px + sDirDX[k] * dist, ty = py + sDirDY[k] * dist, i, n = 0;
                    u8 face = Opposite(k);
                    bool8 ok = TRUE;
                    if (!WalkableBlock(gen, tx, ty) || OnWalk(gen, tx, ty) || ByLedgeF(gen, tx, ty))
                        continue;
                    // Wall at their back, so nobody slips round behind.
                    if (WalkableBlock(gen, tx + sDirDX[k], ty + sDirDY[k]))
                        continue;
                    for (i = 1; i < dist && ok; i++)
                        if (!WalkableBlock(gen, tx + sDirDX[face] * i, ty + sDirDY[face] * i))
                            ok = FALSE;
                    if (!ok)
                        continue;
                    // How far they can see: to the far wall, at most maxSight.
                    while (n <= maxSight && SeeBlock(gen, tx + sDirDX[face] * (n + 1), ty + sDirDY[face] * (n + 1)))
                        n++;
                    if (n == 0 || n > maxSight || n < dist || n < minAcross || IsCrowded(gen, tx, ty) || InPlacedSight(gen, tx, ty) || NearPlaced(gen, tx, ty)
                     || SeenFromNextDoor(gen, tx, ty))
                        continue;
                    if (gen->wetSpot && !NearWaterBlock(gen, tx, ty, 2))
                        continue;
                    if (RingSplits(gen, tx, ty) || !SightLineSafe(gen, tx, ty, 1 << face, n))
                        continue;
                    if (needCut && LinedUp(gen, tx, ty, face))
                        continue;
                    if (needCut)
                    {
                        // Each check is a search of the whole map: only so
                        // many per trainer.
                        if (gen->cutChecks == 0)
                            return 0;
                        gen->cutChecks--;
                        if (!CutsRoute(gen, tx, ty, face, n))
                            continue;
                    }
                    *outX = tx;
                    *outY = ty;
                    *outFacing = face;
                    return n;
                }
            }
        }
    }
    return 0;
}

// The block just inside an edge opening.
static void EdgeBlock(struct Gen *gen, u8 dir, int *x, int *y)
{
    int pos = gen->exitPos[dir];
    switch (dir)
    {
    case RDIR_NORTH: *x = pos; *y = 0; break;
    case RDIR_SOUTH: *x = pos; *y = gen->m->h - 1; break;
    case RDIR_WEST:  *x = 0; *y = pos; break;
    default:         *x = gen->m->w - 1; *y = pos; break;
    }
}

static void PlaceTrainersBetween(struct Gen *gen, int sx, int sy, int ex, int ey);
static void ForestWayFront(struct Gen *gen, int k, int *fx, int *fy);

// The block in the middle of a unit's side that faces direction dir, nudged
// to a walkable block.
static void UnitBlock(struct Gen *gen, int ux, int uy, int *fx, int *fy)
{
    int k;
    for (k = 0; k < 4; k++)
    {
        int x = ux * 2 + (k & 1), y = uy * 2 + (k >> 1);
        if (WalkableBlock(gen, x, y))
        {
            *fx = x;
            *fy = y;
            return;
        }
    }
    *fx = ux * 2;
    *fy = uy * 2;
}

static void PlaceTrainersOnWalk(struct Gen *gen, u8 inSide, u8 outDir)
{
    struct RogueMap *m = gen->m;
    int sx, sy, ex, ey, i;
    // From the entrance (or just past the obstacle at a side route's way in)
    // to the exit, the cave mouth, or the clearing at a dead end.
    if (inSide == RDIR_NONE || gen->exitPos[inSide] == 0xFF)
        return;
    EdgeBlock(gen, inSide, &sx, &sy);
    // Where the woods cut the route, the way on is into the forest.
    if (gen->forestCut && gen->nForestWays == 2)
    {
        ForestWayFront(gen, 0, &ex, &ey);
        PlaceTrainersBetween(gen, sx, sy, ex, ey);
        return;
    }
    if (gen->guardStep)
    {
        int u = gen->path[gen->guardEnd];
        UnitBlock(gen, u % gen->cw, u / gen->cw, &sx, &sy);
    }
    ex = -1;
    for (i = 0; i < m->nWarps; i++)
    {
        if (m->warps[i].kind == RWARP_CAVE)
        {
            ex = m->warps[i].x;
            ey = m->warps[i].y + 1;
        }
    }
    if (ex < 0)
    {
        int step = outDir != RDIR_NONE ? GateLaneStep(gen, LinkOnSide(gen->p, outDir)) : 0;
        if (step && gen->exitPos[outDir] != 0xFF)
        {
            // Up to the block on the main road, in its lane.
            int ux, uy;
            LaneCell(gen, outDir, step + 1, &ux, &uy);
            UnitBlock(gen, ux, uy, &ex, &ey);
        }
        else if (outDir != RDIR_NONE && gen->exitPos[outDir] != 0xFF)
            EdgeBlock(gen, outDir, &ex, &ey);
        else
            UnitBlock(gen, gen->endX, gen->endY, &ex, &ey);
    }
    PlaceTrainersBetween(gen, sx, sy, ex, ey);
}

// TRUE if a trainer not yet moved onto the walk stands within two blocks;
// they may stay where they are.
static bool8 NearUnplaced(struct Gen *gen, int x, int y)
{
    int j;
    for (j = gen->placing + 1; j < gen->m->nObjs; j++)
    {
        const struct RogueObj *o = &gen->m->objs[j];
        if ((o->kind == ROBJ_TRAINER || o->kind == ROBJ_RIVAL) && Abs(o->x - x) <= 2 && Abs(o->y - y) <= 2)
            return TRUE;
    }
    return FALSE;
}

// A spot on the shore for a water trainer the walk does not pass near: the
// free block by the water nearest to step target, facing out over it, as
// the Fishermen of Routes 104 and 118 stand.
static int ShoreSpot(struct Gen *gen, int sx, int sy, int target, int *outX, int *outY, u8 *outFacing)
{
    int x, y, best = 9999, tx = PX(gen->walk[target]), ty = PY(gen->walk[target]);
    FloodReachable(gen, sx >> 1, sy >> 1);
    for (y = 1; y < gen->m->h - 1; y++)
    {
        YIELD();
        for (x = 1; x < gen->m->w - 1; x++)
        {
            u8 k;
            int d = Abs(x - tx) + Abs(y - ty);
            if (d >= best || !WalkableBlock(gen, x, y) || !Reached(gen, x >> 1, y >> 1) || IsCrowded(gen, x, y))
                continue;
            if (OnWalk(gen, x, y) || NearUnplaced(gen, x, y) || RingSplits(gen, x, y) || ByLedgeF(gen, x, y) || InPlacedSight(gen, x, y)
             || SeenFromNextDoor(gen, x, y))
                continue;
            for (k = 0; k < 4; k++)
            {
                int wx = x + sDirDX[k], wy = y + sDirDY[k];
                if (InMap(gen, wx, wy) && (gen->m->blocks[wy * gen->m->w + wx] >> 12) == 1 && SightLineSafe(gen, x, y, 1 << k, 3))
                {
                    best = d;
                    *outX = x;
                    *outY = y;
                    *outFacing = k;
                    break;
                }
            }
        }
    }
    return best < 9999 ? 3 : 0;
}

// How many free blocks a trainer at (x, y) sees toward face, at most max.
// With stopAtWalk, the count stops short of the shortest walk, and *hit is
// set if the line would reach it.
static int SightLine(struct Gen *gen, int x, int y, u8 face, int max, bool8 stopAtWalk, bool8 *hit)
{
    int n = 0;
    if (hit)
        *hit = FALSE;
    while (n < max && SeeBlock(gen, x + sDirDX[face] * (n + 1), y + sDirDY[face] * (n + 1)))
    {
        if (stopAtWalk && OnWalk(gen, x + sDirDX[face] * (n + 1), y + sDirDY[face] * (n + 1)))
        {
            if (hit)
                *hit = TRUE;
            break;
        }
        n++;
    }
    return n;
}

// The fewest blocks from (x, y) to any block of the shortest walk.
static int WalkDistance(struct Gen *gen, int x, int y)
{
    int i, best = 999;
    for (i = 0; i < gen->nWalk; i++)
    {
        int d = Abs(PX(gen->walk[i]) - x) + Abs(PY(gen->walk[i]) - y);
        if (d < best)
            best = d;
    }
    return best;
}

// TRUE if the player can still walk from the start of the walk to its end
// with a trainer on (tx, ty) seeing n blocks toward face, so they can be
// passed unseen. Ledges are one way, as in the game. Marks visited blocks in
// a bit set over sBfsPrev, which no caller needs here.
static bool8 WayStaysOpen(struct Gen *gen, int tx, int ty, u8 face, int n)
{
    int w = gen->m->w, h = gen->m->h, head = 0, tail = 0, k;
    u8 *seen = (u8 *)sBfsPrev;
#define SEEN(x, y) ((seen[((y) * w + (x)) >> 3] >> (((y) * w + (x)) & 7)) & 1)
#define SEE(x, y) (seen[((y) * w + (x)) >> 3] |= 1 << (((y) * w + (x)) & 7))
    memset(seen, 0, (w * h + 7) / 8);
    for (k = 0; k <= n; k++)
    {
        int x = tx + sDirDX[face] * k, y = ty + sDirDY[face] * k;
        if (!InMap(gen, x, y))
            break;
        if ((x == gen->walkSX && y == gen->walkSY) || (x == gen->walkEX && y == gen->walkEY))
            return FALSE;
        SEE(x, y);
    }
    SEE(gen->walkSX, gen->walkSY);
    sBfsQueue[tail++] = PACK(gen->walkSX, gen->walkSY);
    while (head < tail)
    {
        int cur = sBfsQueue[head++ & (BFS_RING - 1)];
        int cx = PX(cur), cy = PY(cur);
        YIELD();
        for (k = 0; k < 4; k++)
        {
            int nx = cx + sDirDX[k], ny = cy + sDirDY[k];
            if (!InMap(gen, nx, ny) || SEEN(nx, ny))
                continue;
            if (nx == gen->walkEX && ny == gen->walkEY)
                return TRUE;
            if (!((gen->free[(ny * w + nx) >> 3] >> ((ny * w + nx) & 7)) & 1))
                continue;
            // A ledge block is only crossed going south, landing below it.
            if ((ny & 1) && Cell(gen, nx >> 1, ny >> 1) == C_LEDGE)
            {
                if (k != RDIR_SOUTH || ny + 1 >= h || SEEN(nx, ny + 1))
                    continue;
                ny++;
                if (!((gen->free[(ny * w + nx) >> 3] >> ((ny * w + nx) & 7)) & 1))
                    continue;
            }
            if (tail - head >= BFS_RING)
                return TRUE;
            SEE(nx, ny);
            sBfsQueue[tail++ & (BFS_RING - 1)] = PACK(nx, ny);
        }
    }
#undef SEEN
#undef SEE
    return FALSE;
}

// A spot beside the walk near step target, as on Emerald's open routes: one
// to three blocks off it, looking across it, with room on the far side, so
// the player walks into their sight on the shortest way but can step round
// them. Some turn between that and a second way. With strict, only spots
// the player can get past unseen and where the way is more than a squeeze.
// They see at least minAcross blocks.
static int FindPathSpot(struct Gen *gen, int target, int *outX, int *outY, u8 *outFacing, bool8 strict, int maxSight, bool8 turn, int firstDist, int minAcross)
{
    int radius, sign;
    for (radius = 0; radius < gen->nWalk; radius++)
    {
        YIELD();
        for (sign = -1; sign <= 1; sign += 2)
        {
            int step = target + radius * sign, k, px, py;
            if (step < 4 || step < gen->walkFrom || step >= gen->nWalk - 3 || (radius == 0 && sign > 0))
                continue;
            px = PX(gen->walk[step]);
            py = PY(gen->walk[step]);
            for (k = 0; k < 4; k++)
            {
                YIELD();
                int j;
                // Some stand right by the way, some a little back from it.
                for (j = 0; j < 3; j++)
                {
                    int dist = 1 + (firstDist - 1 + j) % 3;
                    int tx = px + sDirDX[k] * dist, ty = py + sDirDY[k] * dist, i, n;
                    u8 face = Opposite(k);
                    bool8 ok = TRUE;
                    if (!WalkableBlock(gen, tx, ty) || OnWalk(gen, tx, ty) || ByLedgeF(gen, tx, ty)
                     || tx < 2 || ty < 2 || tx >= gen->m->w - 2 || ty >= gen->m->h - 2)
                        continue;
                    for (i = 1; i < dist && ok; i++)
                        if (!WalkableBlock(gen, tx + sDirDX[face] * i, ty + sDirDY[face] * i) || OnWalk(gen, tx + sDirDX[face] * i, ty + sDirDY[face] * i))
                            ok = FALSE;
                    if (!ok)
                        continue;
                    n = SightLine(gen, tx, ty, face, maxSight, FALSE, NULL);
                    // They see the walk; strictly, the ground goes on past it.
                    if (n < dist || n < minAcross || (strict && n == dist))
                        continue;
                    if (IsCrowded(gen, tx, ty) || RingSplits(gen, tx, ty) || LinedUp(gen, tx, ty, face) || InPlacedSight(gen, tx, ty) || NearPlaced(gen, tx, ty)
                     || SeenFromNextDoor(gen, tx, ty))
                        continue;
                    if (gen->wetSpot && !NearWaterBlock(gen, tx, ty, 2))
                        continue;
                    if (!SightLineSafe(gen, tx, ty, 1 << face, n))
                        continue;
                    if (strict)
                    {
                        // The quick test first, then a search of the map.
                        if (gen->cutChecks < 4)
                            return 0;
                        gen->cutChecks--;
                        if (CutsRoute(gen, tx, ty, face, n))
                            continue;
                        gen->cutChecks -= 3;
                        if (!WayStaysOpen(gen, tx, ty, face, n))
                            continue;
                    }
                    *outX = tx;
                    *outY = ty;
                    *outFacing = face;
                    if (turn)
                    {
                        // The second way: along the walk, whichever side
                        // is more open.
                        u8 a = (face + 1) & 3, b = (face + 3) & 3;
                        int na = SightLine(gen, tx, ty, a, maxSight, FALSE, NULL);
                        int nb = SightLine(gen, tx, ty, b, maxSight, FALSE, NULL);
                        u8 second = na >= nb ? a : b;
                        if ((na >= nb ? na : nb) >= 2 && SightLineSafe(gen, tx, ty, (1 << face) | (1 << second), n))
                            *outFacing = face | (second << 2) | ROBJ_TURNS;
                    }
                    return n;
                }
            }
        }
    }
    return 0;
}

// A spot off the walk, as Emerald puts trainers in grass, clearings and
// side bays: three to seven blocks out along open ground, looking somewhere
// their sight stops short of the walk. The player can leave them be or go
// over to fight. Some look all round.
static int FindSideSpot(struct Gen *gen, int target, int *outX, int *outY, u8 *outFacing, int maxSight, bool8 look)
{
    int tries;
    for (tries = 0; tries < 120; tries++)
    {
        int step = target + (int)RogueRng_Range(&gen->rng, 13) - 6, k, d, side, tx, ty, i, best = 0;
        u8 face = 0, bestFace = 0;
        bool8 ok = TRUE, hit;
        if (tries > 60)
            step = 4 + RogueRng_Range(&gen->rng, gen->nWalk > 8 ? gen->nWalk - 8 : 1);
        if (step < 4 || step < gen->walkFrom || step >= gen->nWalk - 3)
            continue;
        YIELD();
        k = RogueRng_Range(&gen->rng, 4);
        d = 3 + RogueRng_Range(&gen->rng, 5);
        tx = PX(gen->walk[step]);
        ty = PY(gen->walk[step]);
        // Out from the walk along free ground, so the spot can be reached.
        for (i = 1; i <= d && ok; i++)
        {
            tx += sDirDX[k];
            ty += sDirDY[k];
            if (!WalkableBlock(gen, tx, ty) || ByLedgeF(gen, tx, ty) || (i > 1 && OnWalk(gen, tx, ty)))
                ok = FALSE;
        }
        // And a step or two aside, sometimes.
        side = RogueRng_Range(&gen->rng, 5) - 2;
        for (i = 0; i < Abs(side) && ok; i++)
        {
            u8 s = side < 0 ? (k + 3) & 3 : (k + 1) & 3;
            tx += sDirDX[s];
            ty += sDirDY[s];
            if (!WalkableBlock(gen, tx, ty) || ByLedgeF(gen, tx, ty))
                ok = FALSE;
        }
        if (!ok || OnWalk(gen, tx, ty) || WalkDistance(gen, tx, ty) < 3)
            continue;
        // Never in the way in from another neighbour along the edge.
        if (tx < 2 || ty < 2 || tx >= gen->m->w - 2 || ty >= gen->m->h - 2)
            continue;
        if (IsCrowded(gen, tx, ty) || RingSplits(gen, tx, ty) || InPlacedSight(gen, tx, ty) || NearPlaced(gen, tx, ty)
         || SeenFromNextDoor(gen, tx, ty))
            continue;
        if (gen->wetSpot && !NearWaterBlock(gen, tx, ty, 2))
            continue;
        if (look)
        {
            // Every way they look stops short of the walk.
            int r = maxSight;
            u8 f;
            for (f = 0; f < 4; f++)
            {
                int n = SightLine(gen, tx, ty, f, maxSight, TRUE, &hit);
                if (hit && n < r)
                    r = n;
            }
            if (r < 2 || !SightLineSafe(gen, tx, ty, 0xF, r))
                continue;
            *outX = tx;
            *outY = ty;
            *outFacing = Opposite(k) | ROBJ_LOOKS;
            return r;
        }
        // Face the way with the most open ground that stays off the walk,
        // back toward the walk when it can.
        for (i = 0; i < 4; i++)
        {
            int n;
            face = (Opposite(k) + i) & 3;
            n = SightLine(gen, tx, ty, face, maxSight, TRUE, &hit);
            if (hit && n < 2)
                continue;
            if (LinedUp(gen, tx, ty, face) || !SightLineSafe(gen, tx, ty, 1 << face, n))
                continue;
            if (n > best + (i == 0 ? -1 : 0))
            {
                best = n;
                bestFace = face;
            }
        }
        if (best < 2)
            continue;
        *outX = tx;
        *outY = ty;
        *outFacing = bestFace;
        return best;
    }
    return 0;
}

// What each route trainer is for. Emerald's routes mix trainers nobody can
// walk past, trainers beside the way the player can slip round or take on,
// and trainers off in the grass or a side bay. About one in four blocks.
enum
{
    TROLE_BLOCK,
    TROLE_PATH,
    TROLE_SIDE,
};

static u8 TrainerRole(struct Gen *gen, u8 slot)
{
    int n = gen->p->nTrainers, nBlock, nSide, i, order[8], rank = 0;
    u32 h = RogueRng_Hash(gen->r->seed, 7300 + GenPlaceId(gen));
    if (n <= 1)
        return TROLE_BLOCK;
    if (n > 8)
        n = 8;
    nBlock = (n * 30 + 50) / 100;
    if (nBlock < 1)
        nBlock = 1;
    // Most stand where a player on the way meets them, as on Emerald's
    // routes, so the EXP of the main road holds: one in four or five stands
    // off it, none on short routes.
    nSide = n / 4;
    // A shuffle of the slots by the hash; the first ones block, the last
    // ones stand off the way.
    for (i = 0; i < n; i++)
        order[i] = i;
    for (i = n - 1; i > 0; i--)
    {
        int j = RogueRng_Range(&h, i + 1), t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    for (i = 0; i < n; i++)
        if (order[i] == slot)
            rank = i;
    if (rank < nBlock)
        return TROLE_BLOCK;
    if (rank >= n - nSide)
        return TROLE_SIDE;
    return TROLE_PATH;
}

// Victory Road's own map (not a floor of its cave).
static bool8 IsVictoryRoad(struct Gen *gen)
{
    return !sGenFloor && gen->p == &gen->r->places[gen->r->victoryRoad];
}

// The first step of the shortest walk that a trainer or the rival sees:
// where the player is stopped. -1 if they see none of it.
static int FirstSeenStep(struct Gen *gen, const struct RogueObj *o)
{
    u8 mask = ObjSightMask(o), f;
    int i, k;
    for (i = 0; i < gen->nWalk; i++)
    {
        int dx = PX(gen->walk[i]) - o->x, dy = PY(gen->walk[i]) - o->y;
        if ((i & 63) == 0)
            YIELD();
        for (f = 0; f < 4; f++)
            for (k = 1; k <= o->range && ((mask >> f) & 1); k++)
                if (dx == sDirDX[f] * k && dy == sDirDY[f] * k)
                    return i;
    }
    return -1;
}

// Blocks of walk on Victory Road between the rival and the first trainer,
// for the camper.
#define VICTORY_CAMP_GAP 8

// A drawn block the player can stand on, with nobody on it for good: no
// collision, at ground height (3), a door's or a cave mouth's (0) or a
// treehouse's platform (4).
static bool8 FootBlock(struct Gen *gen, int x, int y)
{
    u16 b;
    int i;
    if (!InMap(gen, x, y))
        return FALSE;
    b = gen->m->blocks[y * gen->m->w + x];
    if (((b >> 10) & 3) != 0 || !((b >> 12) == 3 || (b >> 12) == 0 || (b >> 12) == 4))
        return FALSE;
    if (!IsOccupied(gen, x, y))
        return TRUE;
    // (The blockade's grunts and a tunnel's guard leave in time.)
    for (i = 0; i < gen->m->nObjs; i++)
    {
        const struct RogueObj *o = &gen->m->objs[i];
        if (o->x == x && o->y == y && !(o->kind == ROBJ_GRUNT && o->slot < 2) && o->kind != ROBJ_TUNNEL_GUARD)
            return FALSE;
    }
    return TRUE;
}

// The blocks reached on foot from every way into the map, as a bit per
// block in the second half of sBfsQueue (the first half is the search's
// ring). Objects stand in the way.
static void FloodBlocks(struct Gen *gen)
{
    int w = gen->m->w, head = 0, tail = 0, k;
    u8 *seen = (u8 *)&sBfsQueue[BFS_RING / 2];
    memset(seen, 0, (ROGUE_MAX_BLOCKS + 7) / 8);
    for (k = 0; k < 4; k++)
    {
        int x, y;
        if (gen->exitPos[k] == 0xFF)
            continue;
        EdgeBlock(gen, k, &x, &y);
        if (!FootBlock(gen, x, y) || ((seen[(y * w + x) >> 3] >> ((y * w + x) & 7)) & 1))
            continue;
        seen[(y * w + x) >> 3] |= 1 << ((y * w + x) & 7);
        sBfsQueue[tail++] = PACK(x, y);
    }
    // A map with no way in on foot here (the region's first town, an
    // island): from the block in front of its POKeMON CENTER, or of any
    // door, instead.
    for (k = 0; k < gen->m->nWarps && tail == 0; k++)
    {
        int x = gen->m->warps[k].x, y = gen->m->warps[k].y + 1;
        if (gen->m->warps[k].kind != RWARP_PC && k + 1 < gen->m->nWarps)
            continue;
        if (!FootBlock(gen, x, y))
            continue;
        seen[(y * w + x) >> 3] |= 1 << ((y * w + x) & 7);
        sBfsQueue[tail++] = PACK(x, y);
    }
    while (head < tail)
    {
        int cur = sBfsQueue[head++ & (BFS_RING / 2 - 1)];
        int cx = PX(cur), cy = PY(cur);
        YIELD();
        for (k = 0; k < 4; k++)
        {
            int nx = cx + sDirDX[k], ny = cy + sDirDY[k], ni = ny * w + nx;
            if (!FootBlock(gen, nx, ny) || ((seen[ni >> 3] >> (ni & 7)) & 1) || tail - head >= BFS_RING / 2)
                continue;
            seen[ni >> 3] |= 1 << (ni & 7);
            sBfsQueue[tail++ & (BFS_RING / 2 - 1)] = PACK(nx, ny);
        }
    }
}

// TRUE if every sign beside a house (AddHouseSigns: a named house's, the
// lab's, a family plate) has a block the player can reach beside it.
static bool8 HouseSignsReadable(struct Gen *gen)
{
    const struct RogueMap *m = gen->m;
    const u8 *seen = (const u8 *)&sBfsQueue[BFS_RING / 2];
    int i, k;
    FloodBlocks(gen);
    for (i = 0; i < m->nSigns; i++)
    {
        bool8 read = FALSE;
        u8 kind = m->signs[i].kind;
        if ((kind < RSIGN_SCHOOL || kind > RSIGN_GIFT) && kind != RSIGN_HOUSE && kind != RSIGN_LAB)
            continue;
        for (k = 0; k < 4 && !read; k++)
        {
            int x = m->signs[i].x + sDirDX[k], y = m->signs[i].y + sDirDY[k], b = y * m->w + x;
            read = InMap(gen, x, y) && ((seen[b >> 3] >> (b & 7)) & 1);
        }
        if (!read)
            return FALSE;
    }
    return TRUE;
}

// TRUE if every person, Pokemon, berry tree and door of the map has a block
// the player can reach beside it (or is one), with everyone standing where
// they stand (FloodBlocks). Obstacles to clear, the Day Care's yard
// Pokemon and the blockade's grunts aside.
static bool8 TownReachable(struct Gen *gen)
{
    const struct RogueMap *m = gen->m;
    const u8 *seen = (const u8 *)&sBfsQueue[BFS_RING / 2];
    int i, k;
    FloodBlocks(gen);
    for (i = 0; i < m->nObjs + m->nWarps; i++)
    {
        bool8 near = FALSE;
        int ox, oy;
        if (i < m->nObjs)
        {
            const struct RogueObj *o = &m->objs[i];
            if (o->kind == ROBJ_CUT_TREE || o->kind == ROBJ_ROCK || o->kind == ROBJ_BOULDER || o->kind == ROBJ_FEATMON
             || (o->kind == ROBJ_GRUNT && o->slot < 2) || o->kind == ROBJ_TUNNEL_GUARD)
                continue;
            ox = o->x, oy = o->y;
        }
        else
        {
            ox = m->warps[i - m->nObjs].x, oy = m->warps[i - m->nObjs].y;
        }
        for (k = -1; k < 4 && !near; k++)
        {
            int x = ox + (k < 0 ? 0 : sDirDX[k]), y = oy + (k < 0 ? 0 : sDirDY[k]), b = y * m->w + x;
            near = InMap(gen, x, y) && ((seen[b >> 3] >> (b & 7)) & 1);
        }
        if (!near)
            return FALSE;
    }
    return TRUE;
}

// TRUE if someone standing on block (x, y) would cut the free ground (with
// everyone placed so far) in two: its free neighbours no longer all meet.
// RingSplits first, then a search round. The search uses the first half of
// sBfsQueue as its ring and the second half as a bit per block seen.
static bool8 SplitsGround(struct Gen *gen, int x, int y)
{
    int w = gen->m->w, head = 0, tail = 0, left = 0, k;
    u8 *seen = (u8 *)&sBfsQueue[BFS_RING / 2];
    if (!RingSplits(gen, x, y))
        return FALSE;
    memset(seen, 0, (ROGUE_MAX_BLOCKS + 7) / 8);
    seen[(y * w + x) >> 3] |= 1 << ((y * w + x) & 7);
    for (k = 0; k < 4; k++)
    {
        int nx = x + sDirDX[k], ny = y + sDirDY[k];
        if (!WalkableBlock(gen, nx, ny))
            continue;
        if (tail == 0)
        {
            seen[(ny * w + nx) >> 3] |= 1 << ((ny * w + nx) & 7);
            sBfsQueue[tail++] = PACK(nx, ny);
        }
        else
        {
            left++;
        }
    }
    while (head < tail && left > 0)
    {
        int cur = sBfsQueue[head++ & (BFS_RING / 2 - 1)];
        int cx = PX(cur), cy = PY(cur);
        YIELD();
        for (k = 0; k < 4; k++)
        {
            int nx = cx + sDirDX[k], ny = cy + sDirDY[k], ni = ny * w + nx;
            if (!WalkableBlock(gen, nx, ny) || ((seen[ni >> 3] >> (ni & 7)) & 1))
                continue;
            if (tail - head >= BFS_RING / 2)
                return TRUE; // (never on maps this size)
            seen[ni >> 3] |= 1 << (ni & 7);
            sBfsQueue[tail++ & (BFS_RING / 2 - 1)] = PACK(nx, ny);
            if (Abs(nx - x) + Abs(ny - y) == 1)
                left--;
        }
    }
    return left > 0;
}

// A trainer with no spot of their own: a block beside the walk, nearest
// step target, where they close nothing and cross no one's line. They
// face the walk and wait to be talked to. Left as they are if none.
static void WaitBesideWalk(struct Gen *gen, struct RogueObj *o, int target)
{
    int d, pass;
    for (pass = 0; pass < 2; pass++)
    {
        for (d = 0; d < 2 * gen->nWalk; d++)
        {
            int step = target + ((d & 1) ? -(d + 1) / 2 : d / 2), x, y;
            u8 k;
            if (step < 1 || step >= gen->nWalk - 1)
                continue;
            YIELD();
            for (k = 0; k < 4; k++)
            {
                x = PX(gen->walk[step]) + sDirDX[k];
                y = PY(gen->walk[step]) + sDirDY[k];
                if (!WalkableBlock(gen, x, y) || OnWalk(gen, x, y) || ByLedgeF(gen, x, y) || RingSplits(gen, x, y)
                 || !SightLineSafe(gen, x, y, 0, 0) || InPlacedSight(gen, x, y) || (pass == 0 && SeenFromNextDoor(gen, x, y)))
                    continue;
                o->x = x;
                o->y = y;
                o->facing = Opposite(k);
                o->range = 0;
                return;
            }
        }
    }
}

// Moves the trainers onto the shortest walk from (sx, sy) to (ex, ey).
static void PlaceTrainersBetween(struct Gen *gen, int sx, int sy, int ex, int ey)
{
    struct RogueMap *m = gen->m;
    int i;
    // Cave rooms are wider than routes' squeezes, so cave trainers see further.
    int sight = gen->p->kind == RPLACE_CAVE ? 6 : 4;
    // Take the trainers off the map while finding the walk; each is put
    // back as it is placed.
    for (i = 0; i < m->nObjs; i++)
    {
        if (m->objs[i].kind == ROBJ_TRAINER || m->objs[i].kind == ROBJ_RIVAL)
        {
            int b = m->objs[i].y * m->w + m->objs[i].x;
            gen->occupied[b >> 3] &= ~(1 << (b & 7));
        }
    }
    {
        // Which blocks a walk or a search may pass: standable and free.
        int b;
        memset(gen->free, 0, sizeof(gen->free));
        for (b = 0; b < m->w * m->h; b++)
        {
            u16 v = m->blocks[b];
            if ((b & 63) == 0)
                YIELD();
            // (A log bridge's land ends at 4 and planks at 15 are walked
            // too: rogue_gen_water.h, WalkHeight.)
            if (((v >> 10) & 3) == 0 && WalkHeight(gen, v) && !((gen->occupied[b >> 3] >> (b & 7)) & 1))
                gen->free[b >> 3] |= 1 << (b & 7);
        }
    }
    if (!StandableBlock(gen, sx, sy) || !StandableBlock(gen, ex, ey))
    {
        for (i = 0; i < m->nObjs; i++)
            if (m->objs[i].kind == ROBJ_TRAINER || m->objs[i].kind == ROBJ_RIVAL)
                MarkOccupied(gen, m->objs[i].x, m->objs[i].y);
        return;
    }
    if (!FindShortestWalk(gen, sx, sy, ex, ey) || gen->nWalk < 10)
    {
        for (i = 0; i < m->nObjs; i++)
            if (m->objs[i].kind == ROBJ_TRAINER || m->objs[i].kind == ROBJ_RIVAL)
                MarkOccupied(gen, m->objs[i].x, m->objs[i].y);
        return;
    }
    gen->walkSX = sx;
    gen->walkSY = sy;
    gen->walkEX = ex;
    gen->walkEY = ey;
    gen->walkFrom = 0;
    LabelWalls(gen);
    for (i = 0; i < m->nObjs; i++)
    {
        struct RogueObj *o = &m->objs[i];
        int target, x, y, range, wide;
        u8 facing;
        if (o->kind == ROBJ_RIVAL)
            target = gen->nWalk / 8 + 3;
        else if (o->kind == ROBJ_TRAINER)
            target = gen->walkFrom + ((gen->nWalk - gen->walkFrom) * (o->slot + 1)) / (gen->p->nTrainers + 1);
        else
            continue;
        // By their role: off the way, beside it, or where nobody can get
        // past unseen (near the planned place and then anywhere); failing
        // that, the nearest spot beside the walk. A Fisherman, Swimmer,
        // Sailor or Tuber looks for one by the water. The rival always
        // blocks the way.
        gen->placing = i;
        gen->wetSpot = o->kind == ROBJ_TRAINER && gen->p->kind == RPLACE_ROUTE && !sGenFloor
                    && RogueGen_TrainerByWater(gen->r, (u8)(gen->p - gen->r->places), o->slot);
        gen->cutChecks = o->kind == ROBJ_RIVAL ? 60 : 24;
        range = 0;
        if (o->kind == ROBJ_TRAINER)
        {
            u8 role = TrainerRole(gen, o->slot);
            u32 h = RogueRng_Hash(gen->r->seed, 7400 + GenPlaceId(gen) * 16 + o->slot);
            if (role == TROLE_SIDE)
                range = FindSideSpot(gen, target, &x, &y, &facing, sight, h & 1);
            gen->cutChecks = 80;
            if (role != TROLE_BLOCK && range == 0)
                range = FindPathSpot(gen, target, &x, &y, &facing, TRUE, sight, (h >> 1) & 1, 1 + ((h >> 2) % 3), 1);
            if (role == TROLE_PATH && range == 0)
                range = FindSideSpot(gen, target, &x, &y, &facing, sight, h & 1);
            for (wide = 2; wide >= 1 && role != TROLE_BLOCK && range == 0; wide--)
                range = FindPathSpot(gen, target, &x, &y, &facing, FALSE, sight, (h >> 1) & 1, 1 + ((h >> 2) % 3), wide);
            gen->cutChecks = 24;
        }
        // Where the way is three or four blocks across, as at most of
        // Emerald's; two will do, and a one-block gap only if nothing else.
        for (wide = 3; wide >= 1 && range == 0; wide--)
        {
            gen->cutChecks = o->kind == ROBJ_RIVAL ? 60 : 24;
            range = FindTrainerSpot(gen, target, &x, &y, &facing, TRUE, 0, gen->nWalk / 6 + 2, sight, wide);
            if (range == 0)
                range = FindTrainerSpot(gen, target, &x, &y, &facing, TRUE, gen->nWalk / 6 + 2, gen->nWalk, sight, wide);
        }
        if (range == 0 && gen->wetSpot)
        {
            gen->cutChecks = 24;
            range = FindTrainerSpot(gen, target, &x, &y, &facing, FALSE, 0, gen->nWalk, sight, 2);
            if (range == 0)
                range = FindTrainerSpot(gen, target, &x, &y, &facing, FALSE, 0, gen->nWalk, sight, 1);
            if (range == 0)
                range = ShoreSpot(gen, sx, sy, target, &x, &y, &facing);
            if (range == 0)
                gen->wetMiss = TRUE;
        }
        gen->wetSpot = FALSE;
        // The rival sees further.
        if (range == 0 && o->kind == ROBJ_RIVAL)
        {
            gen->cutChecks = 40;
            range = FindTrainerSpot(gen, target, &x, &y, &facing, TRUE, 0, gen->nWalk, 8, 1);
        }
        // The rival must never be missed. As a last resort they stand in a
        // one-block squeeze of the walk itself, facing the player; they
        // leave after the battle, so they never block the way for good.
        if (range == 0 && o->kind == ROBJ_RIVAL)
        {
            int step;
            for (step = 4; step < gen->nWalk - 3 && range == 0; step++)
            {
                YIELD();
                int bx = PX(gen->walk[step]), by = PY(gen->walk[step]);
                int px = PX(gen->walk[step - 1]), py = PY(gen->walk[step - 1]);
                u8 face = (px < bx) ? RDIR_WEST : (px > bx) ? RDIR_EAST : (py < by) ? RDIR_NORTH : RDIR_SOUTH;
                if (IsOccupied(gen, bx, by) || IsCrowded(gen, bx, by) || ByLedgeF(gen, bx, by) || SeenFromNextDoor(gen, bx, by)
                 || !CutsRoute(gen, bx, by, face, 0))
                    continue;
                x = bx;
                y = by;
                facing = face;
                range = 3;
            }
        }
        // Or on the walk itself, looking across a narrow stretch.
        if (range == 0 && o->kind == ROBJ_RIVAL)
        {
            int step;
            for (step = gen->nWalk - 2; step > 3 && range == 0; step--)
            {
                YIELD();
                int bx = PX(gen->walk[step]), by = PY(gen->walk[step]);
                u8 k;
                if (IsOccupied(gen, bx, by) || ByLedgeF(gen, bx, by) || SeenFromNextDoor(gen, bx, by))
                    continue;
                for (k = 0; k < 4 && range == 0; k++)
                {
                    int n = 0;
                    while (n < 4 && SeeBlock(gen, bx + sDirDX[k] * (n + 1), by + sDirDY[k] * (n + 1)))
                        n++;
                    if (n > 0 && n <= 3 && CutsRoute(gen, bx, by, k, n))
                    {
                        x = bx;
                        y = by;
                        facing = k;
                        range = n;
                    }
                }
            }
        }
        // Where a cave mouth is the only way on, the rival can wait right in
        // front of it.
        if (range == 0 && o->kind == ROBJ_RIVAL && gen->p->mouth != 0xFF && !IsOccupied(gen, ex, ey))
        {
            x = ex;
            y = ey;
            facing = RDIR_SOUTH;
            range = 3;
        }
        if (range == 0)
            range = FindTrainerSpot(gen, target, &x, &y, &facing, FALSE, 0, gen->nWalk / 6 + 2, sight, 2);
        if (range == 0)
            range = FindTrainerSpot(gen, target, &x, &y, &facing, FALSE, 0, gen->nWalk / 6 + 2, sight, 1);
        if (range == 0 && o->kind == ROBJ_TRAINER)
            range = FindSideSpot(gen, target, &x, &y, &facing, sight, FALSE);
        if (range == 0 && o->kind == ROBJ_TRAINER)
            range = FindPathSpot(gen, target, &x, &y, &facing, FALSE, sight, FALSE, 1, 1);
        if (range > 0)
        {
            o->x = x;
            o->y = y;
            o->facing = facing;
            o->range = range;
        }
        else if (o->kind == ROBJ_TRAINER)
        {
            // Left where the route put them: they see only as far as is
            // safe, or not at all and wait to be talked to, as a few of
            // Emerald's trainers do.
            o->facing &= 3;
            while (o->range > 0 && !SightLineSafe(gen, o->x, o->y, 1 << o->facing, o->range))
                o->range--;
            // But not where they would close a corridor, alone or beside
            // someone placed before them: then beside the walk, waiting.
            if (SplitsGround(gen, o->x, o->y))
                WaitBesideWalk(gen, o, target);
        }
        MarkOccupied(gen, o->x, o->y);
        // On Victory Road the trainers wait past the rival, with room
        // between for the camper (PlaceVictoryCamp).
        if (o->kind == ROBJ_RIVAL && IsVictoryRoad(gen))
        {
            int seen = FirstSeenStep(gen, o);
            if (seen >= 0 && seen + VICTORY_CAMP_GAP < gen->nWalk - 10)
                gen->walkFrom = seen + VICTORY_CAMP_GAP;
        }
    }
}

// TRUE if a roadside camper stands on or next to block (x, y).
static bool8 NearCamp(struct Gen *gen, int x, int y)
{
    int j;
    for (j = 0; j < gen->m->nObjs; j++)
    {
        const struct RogueObj *o = &gen->m->objs[j];
        if (o->kind == ROBJ_CAMP && Abs(o->x - x) <= 1 && Abs(o->y - y) <= 1)
            return TRUE;
    }
    return FALSE;
}

// TRUE if a camper on block (x, y) would close the ground with a trainer
// or the rival who walks up to a block of their line beside him.
static bool8 CampClosesLine(struct Gen *gen, int x, int y)
{
    int j, k, b = y * gen->m->w + x;
    bool8 closes = FALSE;
    gen->occupied[b >> 3] |= 1 << (b & 7);
    for (j = 0; j < gen->m->nObjs && !closes; j++)
    {
        const struct RogueObj *o = &gen->m->objs[j];
        u8 mask = ObjSightMask(o), f;
        if (o->kind != ROBJ_TRAINER && o->kind != ROBJ_RIVAL)
            continue;
        for (f = 0; f < 4 && !closes; f++)
        {
            if (!((mask >> f) & 1))
                continue;
            for (k = 1; k < o->range && !closes; k++)
            {
                int cx = o->x + sDirDX[f] * k, cy = o->y + sDirDY[f] * k;
                if (!SeeBlock(gen, cx, cy))
                    break;
                if (Abs(cx - x) <= 1 && Abs(cy - y) <= 1 && SplitsGround(gen, cx, cy))
                    closes = TRUE;
            }
        }
    }
    gen->occupied[b >> 3] &= ~(1 << (b & 7));
    return closes;
}

// TRUE if block (x, y) is beside a block the rival sees: the walk would
// pass the camper there just as the rival stops the player.
static bool8 BesideRivalLine(struct Gen *gen, int x, int y)
{
    int j, k;
    for (j = 0; j < gen->m->nObjs; j++)
    {
        const struct RogueObj *o = &gen->m->objs[j];
        u8 mask = ObjSightMask(o), f;
        if (o->kind != ROBJ_RIVAL)
            continue;
        for (f = 0; f < 4; f++)
            for (k = 1; k <= o->range && ((mask >> f) & 1); k++)
                if (Abs(o->x + sDirDX[f] * k - x) + Abs(o->y + sDirDY[f] * k - y) <= 1)
                    return TRUE;
    }
    return FALSE;
}

// Victory Road: a camper beside the walk just past the rival and before
// the first trainer, so a team worn out by the rival can rest once before
// the road's trainers. Emerald has the Ever Grande Center at the foot of
// its Victory Road; here the last gym town's is behind the rival, and the
// camper stands before the rest. FALSE if the trainers are not all past the
// rival with room for him (the map is tried again).
static bool8 PlaceVictoryCamp(struct Gen *gen)
{
    struct RogueMap *m = gen->m;
    int rival = -1, first = gen->nWalk, i, s, pass;
    s32 slot = RogueGen_RoadCampFlag(gen->r, m->place);
    if (slot < 0 || gen->nWalk < 10)
        return gen->attempt >= 40;
    for (i = 0; i < m->nObjs; i++)
    {
        const struct RogueObj *o = &m->objs[i];
        int seen;
        if (o->kind != ROBJ_RIVAL && o->kind != ROBJ_TRAINER)
            continue;
        seen = FirstSeenStep(gen, o);
        if (o->kind == ROBJ_RIVAL)
        {
            rival = seen;
            continue;
        }
        // One standing on the walk is met there even if they look nowhere:
        // the walk goes round them, a step or two sooner.
        for (s = 0; s < gen->nWalk; s++)
            if (PX(gen->walk[s]) == o->x && PY(gen->walk[s]) == o->y && (seen < 0 || s - 2 < seen))
                seen = s - 2;
        if (seen >= 0 && seen < first)
            first = seen;
    }
    if (gen->attempt < 40 && (rival < 0 || first < rival + 4))
        return FALSE;
    gen->placing = m->nObjs; // (InPlacedSight and NearPlaced look at everyone)
    // Between the rival and the first trainer; failing that, anywhere past
    // the rival.
    for (pass = 0; pass < 2; pass++)
    {
        int from = rival >= 0 ? rival + 2 : 4, to = pass == 0 ? first - 1 : gen->nWalk - 2;
        for (s = from; s < to; s++)
        {
            int px = PX(gen->walk[s]), py = PY(gen->walk[s]);
            u8 k;
            YIELD();
            for (k = 0; k < 4; k++)
            {
                int x = px + sDirDX[k], y = py + sDirDY[k];
                // (Where nobody who walks up beside him closes the way with
                // him, and off the rival's line, so the walk passes him
                // after the rival.)
                if (!WalkableBlock(gen, x, y) || OnWalk(gen, x, y) || ByLedgeF(gen, x, y) || RingSplits(gen, x, y)
                 || InPlacedSight(gen, x, y) || NearPlaced(gen, x, y) || SeenFromNextDoor(gen, x, y)
                 || CampClosesLine(gen, x, y) || BesideRivalLine(gen, x, y))
                    continue;
                AddObj(gen, ROBJ_CAMP, x, y, Opposite(k), (u8)slot, 0);
                return TRUE;
            }
        }
        if (gen->attempt < 40)
            break;
    }
    return FALSE;
}

// Road style at the end of a route that meets a town: that town's own paving
// if the route shares its tileset, sand for paved towns and cities, worn
// grass for villages. Routes meet routes with worn grass.
static u8 EndStyle(struct Gen *gen, u8 target)
{
    const struct RoguePlace *t = &gen->r->places[target];
    const struct TownTheme *th = &sThemes[t->theme < RTHEME_COUNT ? t->theme : 0];
    if (t->kind != RPLACE_TOWN)
        return V_PATH;
    // (Ash paving has ash edges, so it stops at the town.)
    if (t->theme == gen->p->theme && th->roadTL && th->ground == TGROUND_GRASS && t->type != RTOWN_SUMMIT)
        return V_ROAD;
    if (th->roadTL || t->type == RTOWN_GYM || t->type == RTOWN_SUMMIT)
        return V_SAND;
    return V_PATH;
}

// Where a side route's obstacle, or a block on the main road, stands.
// Objects are only shown for the map the player is on, so an obstacle just
// past a seam would pop in at the crossing. Beside a route it stands on this
// side of the seam instead, in a lane of the route the player comes from, so
// it is in sight before the crossing (GateLaneStep). A town has no room for
// the lane, so beside one it stays on the guarded route, too deep to be seen
// from the town. Water to cross is part of the map and stays where it is.
static bool8 ObstacleOutside(const struct RogueRegion *r, const struct RoguePlace *q)
{
    const struct RogueLink *l;
    const struct RoguePlace *t;
    if (q->obstacle == ROBST_NONE || q->obstacle == ROBST_SURF)
        return FALSE;
    l = LinkOnSide(q, q->guardDir);
    if (l == NULL)
        return FALSE;
    t = &r->places[l->target];
    return t->kind == RPLACE_ROUTE && t->type != RROUTE_SEA && t->type != RROUTE_MOUNTAIN;
}

// The unit, counted in from the edge, where the obstacle of the side place
// through l stands in this map's lane, or 0 if there is no lane. The camera
// shows about 5 blocks past a seam up or down and 7 across, so it stands
// just beyond that and does not pop in on the way back either.
static int GateLaneStep(struct Gen *gen, const struct RogueLink *l)
{
    const struct RoguePlace *t = &gen->r->places[l->target];
    if (!ObstacleOutside(gen->r, t) || t->guardDir != Opposite(l->dir))
        return 0;
    return (l->dir == RDIR_NORTH || l->dir == RDIR_SOUTH) ? 2 : 3;
}

static bool8 InGateLaneU(struct Gen *gen, u8 d, int step, int x, int y);

// TRUE for a block in a lane up to its obstacle.
static bool8 InGateLaneF(struct Gen *gen, int fx, int fy)
{
    const struct RoguePlace *p = gen->p;
    int i;
    for (i = 0; i < p->nLinks; i++)
    {
        int step = GateLaneStep(gen, &p->links[i]);
        if (step && gen->exitPos[p->links[i].dir] != 0xFF && InGateLaneU(gen, p->links[i].dir, step, fx >> 1, fy >> 1))
            return TRUE;
    }
    return FALSE;
}

// The kind of object for a side place's obstacle.
static u8 ObstacleObj(u8 obstacle)
{
    return obstacle == ROBST_CUT ? ROBJ_CUT_TREE : (obstacle == ROBST_ROCK_SMASH ? ROBJ_ROCK : ROBJ_BOULDER);
}

// Obstacle objects already on the map; each has its own flag for being
// cleared.
static u8 CountObstacles(struct Gen *gen)
{
    int i, n = 0;
    for (i = 0; i < gen->m->nObjs; i++)
        if (gen->m->objs[i].kind >= ROBJ_CUT_TREE)
            n++;
    return n;
}

// Block k (0 or 1) of an obstacle across the one-unit way at unit (ux, uy),
// for a player walking in toward into: side by side on the row nearer the
// way in, or staggered, one on each row, which still shuts the way until
// one is cleared (as Hoenn's small trees stand in twos and threes, not
// always in a row). Boulders stay side by side: pushed, one of a staggered
// pair would only meet the other. stagger 1 puts the second block on the
// unit's other row, deeper in; 2 on the row before the near one, into the
// unit before (a lane's, whose other row is in view across the seam).
#define STAGGER_DEEPER 1
#define STAGGER_BEFORE 2
static void ObstacleSpot(int ux, int uy, u8 into, int k, u8 stagger, int *fx, int *fy)
{
    int shift = !k ? 0 : stagger == STAGGER_DEEPER ? 1 : stagger == STAGGER_BEFORE ? -1 : 0;
    switch (into)
    {
    case RDIR_NORTH: *fx = ux * 2 + k; *fy = uy * 2 + 1 - shift; break;
    case RDIR_SOUTH: *fx = ux * 2 + k; *fy = uy * 2 + shift; break;
    case RDIR_EAST:  *fx = ux * 2 + shift; *fy = uy * 2 + k; break;
    default:         *fx = ux * 2 + 1 - shift; *fy = uy * 2 + k; break;
    }
}

// Staggered (ObstacleSpot) for about half the small trees and cracked rocks,
// by place and side, without drawing on the map's own random numbers.
static bool8 ObstacleStaggered(struct Gen *gen, u8 kind, u32 salt)
{
    return kind != ROBJ_BOULDER && (RogueRng_Hash(gen->r->seed, 7310 + salt) & 1);
}

// The obstacle at a side route's way in: two of a kind across the one-unit
// way (only one needs clearing), or water to cross.
static void PlaceGuard(struct Gen *gen)
{
    const struct RoguePlace *p = gen->p;
    int u = gen->path[gen->guardStep], ux = u % gen->cw, uy = u / gen->cw;
    u8 into = Opposite(p->guardDir); // the way the player walks in
    u8 kind;
    int k;
    u8 stagger;

    if (p->obstacle == ROBST_SURF)
    {
        int a = gen->path[gen->guardStep], b = gen->path[gen->guardStep + 1];
        int before = gen->path[gen->guardStep - 1], after = gen->path[gen->guardStep + 2];
        SetCell(gen, a % gen->cw, a / gen->cw, C_WATER);
        SetCell(gen, b % gen->cw, b / gen->cw, C_WATER);
        // The bank is drawn by the shore unit below the water.
        if (into == RDIR_NORTH)
            SetCell(gen, before % gen->cw, before / gen->cw, C_SHORE);
        else
            SetCell(gen, after % gen->cw, after / gen->cw, C_SHORE);
        return;
    }
    kind = ObstacleObj(p->obstacle);
    stagger = ObstacleStaggered(gen, kind, (u32)(p - gen->r->places) * 8) ? STAGGER_DEEPER : 0;
    for (k = 0; k < 2; k++)
    {
        int fx, fy;
        ObstacleSpot(ux, uy, into, k, stagger, &fx, &fy);
        SetCover(gen, fx, fy, BareCover(gen, fx, fy));
        AddObj(gen, kind, fx, fy, RDIR_SOUTH, k, 0);
    }
}

// TRUE if someone or something on the finished map stands on (x, y).
static bool8 ObjectAt(struct Gen *gen, int x, int y)
{
    int i;
    for (i = 0; i < gen->m->nObjs; i++)
        if (gen->m->objs[i].x == x && gen->m->objs[i].y == y)
            return TRUE;
    return FALSE;
}

// TRUE if nobody can walk the route from end to end without stepping into
// the rival's sight (n blocks ahead) or onto the rival.
static bool8 RivalBlocksWay(struct Gen *gen, const struct RogueObj *o, int n)
{
    int w = gen->m->w, h = gen->m->h, head = 0, tail = 0, k;
    u8 *seen = gen->cover;
    memset(seen, 0, w * h);
    for (k = 0; k <= n; k++)
    {
        int x = o->x + sDirDX[o->facing] * k, y = o->y + sDirDY[o->facing] * k;
        // The game's sight stops at anyone standing in the way.
        if (!InMap(gen, x, y) || (k > 0 && ObjectAt(gen, x, y)))
            break;
        // And at a wall: the game looks along the ground (no seeing over
        // rock to the next corridor).
        if (k > 0 && !StandableBlock(gen, x, y))
            break;
        if ((x == gen->walkSX && y == gen->walkSY) || (x == gen->walkEX && y == gen->walkEY))
            return TRUE;
        seen[y * w + x] = 1;
    }
    seen[gen->walkSY * w + gen->walkSX] = 1;
    sBfsQueue[tail++] = PACK(gen->walkSX, gen->walkSY);
    while (head < tail)
    {
        int cur = sBfsQueue[head++ & (BFS_RING - 1)];
        int cx = PX(cur), cy = PY(cur);
        YIELD();
        for (k = 0; k < 4; k++)
        {
            int nx = cx + sDirDX[k], ny = cy + sDirDY[k];
            if (!InMap(gen, nx, ny) || seen[ny * w + nx])
                continue;
            if (nx == gen->walkEX && ny == gen->walkEY)
                return FALSE;
            if (!StandableBlock(gen, nx, ny) || ObjectAt(gen, nx, ny))
                continue;
            if (tail - head >= BFS_RING)
                return FALSE;
            seen[ny * w + nx] = 1;
            sBfsQueue[tail++ & (BFS_RING - 1)] = PACK(nx, ny);
        }
    }
    return TRUE;
}

// TRUE if, on the finished blocks, the way in cannot reach the far side of
// the obstacle: water and the obstacle's own objects block the walk.
static bool8 GuardHoldsOnBlocks(struct Gen *gen)
{
    int w = gen->m->w, h = gen->m->h, head = 0, tail = 0, sx, sy, k;
    int beyond = gen->path[gen->guardEnd];
    u8 d = gen->p->guardDir, *seen = gen->cover;
    int pos = gen->exitPos[d];
    if (pos == 0xFF)
        return TRUE;
    sx = d == RDIR_EAST ? w - 1 : d == RDIR_WEST ? 0 : pos;
    sy = d == RDIR_SOUTH ? h - 1 : d == RDIR_NORTH ? 0 : pos;
    memset(seen, 0, w * h);
    seen[sy * w + sx] = 1;
    sBfsQueue[tail++] = PACK(sx, sy);
    while (head < tail)
    {
        int cur = sBfsQueue[head++ & (BFS_RING - 1)];
        int cx = PX(cur), cy = PY(cur);
        YIELD();
        for (k = 0; k < 4; k++)
        {
            int nx = cx + sDirDX[k], ny = cy + sDirDY[k];
            if (!WalkableBlock(gen, nx, ny) || seen[ny * w + nx])
                continue;
            if (tail - head >= BFS_RING)
                return FALSE;
            seen[ny * w + nx] = 1;
            sBfsQueue[tail++ & (BFS_RING - 1)] = PACK(nx, ny);
        }
    }
    for (k = 0; k < 4; k++)
        if (seen[((beyond / gen->cw) * 2 + (k >> 1)) * w + (beyond % gen->cw) * 2 + (k & 1)])
            return FALSE;
    return TRUE;
}

static void KeepAround(struct Gen *gen, int x, int y);

// The unit k units in from the edge d along this map's opening there.
static void LaneCell(struct Gen *gen, u8 d, int k, int *x, int *y)
{
    EdgeCell(gen, d, gen->exitPos[d] / 2, k, x, y);
}

// A straight lane in from the opening on side d, one unit wide, walled on
// both sides up to the obstacle's unit (step), and open a unit past it, like
// a side route's own way in. Returns the unit past the obstacle.
static bool8 CarveGateLane(struct Gen *gen, u8 d, int step, int *ix, int *iy)
{
    u8 a = (d + 1) & 3, b = (d + 3) & 3;
    int k, x, y;
    for (k = 1; k <= step + 1; k++)
    {
        u8 c;
        LaneCell(gen, d, k, &x, &y);
        c = Cell(gen, x, y);
        if (IsEdge(gen, x, y) || (c != C_TREE && c != C_OPEN && c != C_ROAD))
            return FALSE;
        AddRoadCell(gen, x, y);
    }
    *ix = x;
    *iy = y;
    for (k = 0; k <= step; k++)
    {
        int s;
        LaneCell(gen, d, k, &x, &y);
        for (s = 0; s < 2; s++)
        {
            u8 side = s ? b : a;
            int wx = x + sDirDX[side], wy = y + sDirDY[side];
            u8 c = Cell(gen, wx, wy);
            if (c == C_OPEN)
                SetCell(gen, wx, wy, C_TREE);
            else if (IsOpenU(c))
                return FALSE;
        }
        KeepAround(gen, x, y);
    }
    return TRUE;
}

// TRUE if (x, y) lies in the lane of the opening on side d, up to and
// including the obstacle's unit.
static bool8 InGateLaneU(struct Gen *gen, u8 d, int step, int x, int y)
{
    int k, lx, ly;
    for (k = 0; k <= step; k++)
    {
        LaneCell(gen, d, k, &lx, &ly);
        if (lx == x && ly == y)
            return TRUE;
    }
    return FALSE;
}

// The obstacles of the side places beside this route: two of a kind across
// each lane, on the obstacle unit's row away from the edge, facing the
// player who comes up the lane.
static void PlaceGateObstacles(struct Gen *gen)
{
    const struct RoguePlace *p = gen->p;
    int i, k;
    for (i = 0; i < p->nLinks; i++)
    {
        const struct RogueLink *l = &p->links[i];
        int step = GateLaneStep(gen, l), ux, uy;
        u8 kind;
        u8 stagger;
        if (step == 0)
            continue;
        kind = ObstacleObj(gen->r->places[l->target].obstacle);
        stagger = ObstacleStaggered(gen, kind, (u32)l->target * 8 + 1 + l->dir) ? STAGGER_BEFORE : 0;
        LaneCell(gen, l->dir, step, &ux, &uy);
        for (k = 0; k < 2; k++)
        {
            int fx, fy;
            ObstacleSpot(ux, uy, l->dir, k, stagger, &fx, &fy);
            SetCover(gen, fx, fy, BareCover(gen, fx, fy));
            AddObj(gen, kind, fx, fy, Opposite(l->dir), CountObstacles(gen) & 3, 0);
        }
    }
}

// TRUE if, with each lane's obstacle unit walled off, nothing of the lane
// past it can be reached from (sx, sy).
static bool8 GatesHold(struct Gen *gen, int sx, int sy)
{
    const struct RoguePlace *p = gen->p;
    int i, k;
    for (i = 0; i < p->nLinks; i++)
    {
        const struct RogueLink *l = &p->links[i];
        int step = GateLaneStep(gen, l), x, y;
        u8 saved;
        bool8 held = TRUE;
        if (step == 0)
            continue;
        LaneCell(gen, l->dir, step, &x, &y);
        // The obstacle stands in only one row of its unit: the units beside
        // it must stay shut.
        if (IsOpenU(Cell(gen, x + sDirDX[(l->dir + 1) & 3], y + sDirDY[(l->dir + 1) & 3]))
         || IsOpenU(Cell(gen, x + sDirDX[(l->dir + 3) & 3], y + sDirDY[(l->dir + 3) & 3])))
            return FALSE;
        saved = gen->g[y * gen->cw + x];
        gen->g[y * gen->cw + x] = C_TREE;
        FloodReachable(gen, sx, sy);
        for (k = 0; k < step && held; k++)
        {
            int lx, ly;
            LaneCell(gen, l->dir, k, &lx, &ly);
            if (Reached(gen, lx, ly))
                held = FALSE;
        }
        gen->g[y * gen->cw + x] = saved;
        if (!held)
            return FALSE;
    }
    return TRUE;
}

// The same on the finished blocks, with the obstacles standing: from each
// lane's opening neither the unit past the obstacle nor another way out can
// be reached.
// A tunnel's far mouth on a route: every way out of the map can be walked
// to from in front of it, on the drawn blocks (a tree or a ledge band can
// come between, as the rock takes the top rows).
static bool8 TunnelMouthJoined(struct Gen *gen)
{
    const struct RoguePlace *p = gen->p;
    int w = gen->m->w, h = gen->m->h, i, k, head = 0, tail = 0;
    u8 *seen = gen->cover;
    // From the block the player steps out onto: below a cave mouth, or on
    // the volcano mountain's terrace (v1.1).
    int sy = (gen->caveHill & 0x40) ? VOLCANO_HILL_DOOR_Y + 1 : 6;
    memset(seen, 0, w * h);
    seen[sy * w + gen->mouthX] = 1;
    sBfsQueue[tail++] = PACK(gen->mouthX, sy);
    while (head < tail)
    {
        int cur = sBfsQueue[head++ & (BFS_RING - 1)];
        int cx = PX(cur), cy = PY(cur);
        YIELD();
        for (k = 0; k < 4; k++)
        {
            int nx = cx + sDirDX[k], ny = cy + sDirDY[k];
            if (!WalkableBlock(gen, nx, ny) || seen[ny * w + nx])
                continue;
            if (tail - head >= BFS_RING)
                return FALSE;
            seen[ny * w + nx] = 1;
            sBfsQueue[tail++ & (BFS_RING - 1)] = PACK(nx, ny);
        }
    }
    for (i = 0; i < p->nLinks; i++)
    {
        u8 d = p->links[i].dir;
        int pos = gen->exitPos[d], sx, sy, j;
        bool8 ok = FALSE;
        // (Not past an obstacle: a lane's or the guarded way in's.)
        if (pos == 0xFF || GateLaneStep(gen, &p->links[i]) || (p->obstacle && d == p->guardDir))
            continue;
        // The opening's two blocks, on the edge or a step in.
        for (j = 0; j < 4 && !ok; j++)
        {
            int in = j >> 1, along = j & 1;
            sx = d == RDIR_EAST ? w - 1 - in : d == RDIR_WEST ? in : pos + along;
            sy = d == RDIR_SOUTH ? h - 1 - in : d == RDIR_NORTH ? in : pos + along;
            ok = sx < w && sy < h && seen[sy * w + sx];
        }
        if (!ok)
            return FALSE;
    }
    return TRUE;
}

static bool8 GatesHoldOnBlocks(struct Gen *gen)
{
    const struct RoguePlace *p = gen->p;
    int w = gen->m->w, h = gen->m->h, i, k;
    u8 *seen = gen->cover;
    for (i = 0; i < p->nLinks; i++)
    {
        const struct RogueLink *l = &p->links[i];
        int step = GateLaneStep(gen, l), head = 0, tail = 0, sx, sy, ux, uy;
        u8 d = l->dir;
        int pos = gen->exitPos[d];
        if (step == 0 || pos == 0xFF)
            continue;
        sx = d == RDIR_EAST ? w - 1 : d == RDIR_WEST ? 0 : pos;
        sy = d == RDIR_SOUTH ? h - 1 : d == RDIR_NORTH ? 0 : pos;
        memset(seen, 0, w * h);
        seen[sy * w + sx] = 1;
        sBfsQueue[tail++] = PACK(sx, sy);
        while (head < tail)
        {
            int cur = sBfsQueue[head++ & (BFS_RING - 1)];
            int cx = PX(cur), cy = PY(cur);
            YIELD();
            for (k = 0; k < 4; k++)
            {
                int nx = cx + sDirDX[k], ny = cy + sDirDY[k];
                if (!WalkableBlock(gen, nx, ny) || seen[ny * w + nx])
                    continue;
                if (tail - head >= BFS_RING)
                    return FALSE;
                seen[ny * w + nx] = 1;
                sBfsQueue[tail++ & (BFS_RING - 1)] = PACK(nx, ny);
            }
        }
        LaneCell(gen, d, step + 1, &ux, &uy);
        for (k = 0; k < 4; k++)
            if (seen[(uy * 2 + (k >> 1)) * w + ux * 2 + (k & 1)])
                return FALSE;
        for (k = 0; k < 4; k++)
        {
            int ex, ey;
            if (k == d || gen->exitPos[k] == 0xFF)
                continue;
            EdgeBlock(gen, k, &ex, &ey);
            if (seen[ey * w + ex])
                return FALSE;
        }
    }
    return TRUE;
}

// TRUE if, with the obstacle unit walled off, nothing past it can be reached
// from the way in.
static bool8 GuardHolds(struct Gen *gen)
{
    int u = gen->path[gen->guardStep], start = gen->path[1], beyond = gen->path[gen->guardEnd];
    u8 saved = gen->g[u];
    bool8 held;
    gen->g[u] = C_TREE;
    FloodReachable(gen, start % gen->cw, start / gen->cw);
    held = !Reached(gen, beyond % gen->cw, beyond / gen->cw);
    gen->g[u] = saved;
    return held;
}


// ---------------------------------------------------------------------------
// Route types


static bool8 InRouteSeam(struct Gen *gen, int fx, int fy)
{
    const struct RoguePlace *p = gen->p;
    int i;
    for (i = 0; i < p->nLinks; i++)
    {
        const struct RogueLink *l = &p->links[i];
        if (gen->r->places[l->target].theme == p->theme)
            continue;
        switch (l->dir)
        {
        case RDIR_NORTH: if (fy < SEAM_BLOCKS) return TRUE; break;
        case RDIR_SOUTH: if (fy >= gen->m->h - SEAM_BLOCKS) return TRUE; break;
        case RDIR_WEST:  if (fx < SEAM_BLOCKS) return TRUE; break;
        default:         if (fx >= gen->m->w - SEAM_BLOCKS) return TRUE; break;
        }
    }
    return FALSE;
}

// TRUE for a route that leaves the first town.
static bool8 FirstRoute(struct Gen *gen)
{
    int i;
    for (i = 0; i < gen->p->nLinks; i++)
        if (gen->p->links[i].target == gen->r->start)
            return TRUE;
    return FALSE;
}

// v1.1: does the ash field across link l of route p have its ash at the
// seam, at unit pos along p's side (AshKeptBack, which is all an ash
// field keeps clear)?
static bool8 FieldAshAt(const struct RogueRegion *r, const struct RoguePlace *p, const struct RogueLink *l, int pos)
{
    const struct RoguePlace *t = &r->places[l->target];
    int rx, ry, along = (pos * 2 - l->offset) >> 1;
    u8 d = l->dir;
    rx = d == RDIR_NORTH || d == RDIR_SOUTH ? along : d == RDIR_WEST ? t->w / 2 - 1 : 0;
    ry = d == RDIR_WEST || d == RDIR_EAST ? along : d == RDIR_NORTH ? t->h / 2 - 1 : 0;
    if (rx < 0 || ry < 0 || rx >= t->w / 2 || ry >= t->h / 2)
        return FALSE;
    return !AshKeptBack(r, t, rx, ry);
}

// Next to an ash town the ash drifts out along the route, the way it covers
// Route 113 from Fallarbor, and gives way to grass partway along with a
// ragged edge.
static void PlanAshFade(struct Gen *gen)
{
    const struct RoguePlace *p = gen->p;
    u32 h = RogueRng_Hash(gen->r->seed, 6100 + (u32)(p - gen->r->places));
    int k, ux, uy;
    // An ash field is ash all the way along, but for its edges.
    bool8 field = (p->type == RROUTE_DESERT && !FirstRoute(gen));
    bool8 any = field;
    memset(gen->ashZone, field ? 0xFF : 0, sizeof(gen->ashZone));
    for (k = 0; k < p->nLinks && !field; k++)
    {
        const struct RogueLink *l = &p->links[k];
        const struct RoguePlace *t = &gen->r->places[l->target];
        bool8 across = (l->dir == RDIR_NORTH || l->dir == RDIR_SOUTH);
        int len = across ? gen->ch : gen->cw;
        int reach = len * (45 + (h >> (k * 4)) % 35) / 100; // 45 to 79 percent of the way
        // The first route opens green and only turns to ash toward the town.
        if (FirstRoute(gen))
            reach = len * (30 + (h >> (k * 4)) % 16) / 100;
        // A river or a lake needs green banks: the ash only drifts in a
        // little way.
        if (p->type == RROUTE_RIVER || p->type == RROUTE_LAKE)
            reach = len * (20 + (h >> (k * 4)) % 14) / 100;
        // v1.1: from an ash field next door the ash drifts in less far,
        // a quarter to two fifths of the way, thinning into the grass.
        if (AshFieldNextDoor(gen->r, p, l->dir) && !FirstRoute(gen))
            reach = len * (25 + (h >> (k * 4 + 2)) % 16) / 100;
        else if (t->kind != RPLACE_TOWN || t->theme != RTHEME_ASH)
            continue;
        any = TRUE;
        for (uy = 0; uy < gen->ch; uy++)
        {
            for (ux = 0; ux < gen->cw; ux++)
            {
                YIELD();
                int along = l->dir == RDIR_NORTH ? uy : l->dir == RDIR_SOUTH ? gen->ch - 1 - uy
                          : l->dir == RDIR_WEST ? ux : gen->cw - 1 - ux;
                int side = across ? ux : uy;
                // A ragged edge: broad bays and points every few rows or
                // columns, and small steps between them.
                int jag = (int)(RogueRng_Hash(h, 40 + side / 4) % 5) - 2 + (int)(RogueRng_Hash(h, 80 + side / 2) % 3) - 1
                        + (int)(RogueRng_Hash(h, 120 + uy * 64 + ux) % 2);
                // v1.1: from an ash field, only where its ash comes to
                // the seam, and a unit to each side.
                if (along < reach + jag && (t->kind == RPLACE_TOWN
                 || (FieldAshAt(gen->r, p, l, side) && FieldAshAt(gen->r, p, l, side - 1) && FieldAshAt(gen->r, p, l, side + 1))))
                {
                    int i = uy * gen->cw + ux;
                    gen->ashZone[i >> 3] |= 1 << (i & 7);
                }
            }
        }
    }
    if (any)
    {
        for (uy = 0; uy < gen->ch; uy++)
        {
            for (ux = 0; ux < gen->cw; ux++)
            {
                YIELD();
                int i = uy * gen->cw + ux;
                if (AshKeptBack(gen->r, p, ux, uy))
                    gen->ashZone[i >> 3] &= ~(1 << (i & 7));
            }
        }
        gen->groundKind = GROUND_ASH;
        gen->ashFade = TRUE;
    }
}

// Which trees in the ash are drawn as olive ash trees: those with ash (or
// another tree) above for their tips and below for their trunks, so no
// tree is cut in half where the ash meets the grass. An ash tree may stand
// on an ordinary tree, whose round crown shows whole under its trunk, so a
// line of trees running out of the ash changes from ash trees to ordinary
// ones where the ash ends, instead of being ordinary all along (which put
// ordinary trees against an ash town's at the seam).
static void ChooseAshTrees(struct Gen *gen)
{
    int ux, uy, i;
    bool8 changed = TRUE;
    memset(gen->ashTree, 0, sizeof(gen->ashTree));
    if (gen->groundKind != GROUND_ASH || gen->townGround != TGROUND_GRASS)
        return;
    for (uy = 0; uy < gen->ch; uy++)
    {
        for (ux = 0; ux < gen->cw; ux++)
        {
            bool8 above;
            if (Cell(gen, ux, uy) != C_TREE || !AshZoneU(gen, ux, uy)
             || (uy + 1 < gen->ch && !AshZoneU(gen, ux, uy + 1) && Cell(gen, ux, uy + 1) != C_TREE)
             || InRouteSeam(gen, ux * 2, uy * 2) || InRouteSeam(gen, ux * 2 + 1, uy * 2 + 1))
                continue;
            if (uy == 0)
                above = TRUE;
            else if (Cell(gen, ux, uy - 1) == C_TREE)
                above = TRUE; // an ordinary tree above simply ends there
            else
                above = Cover(gen, ux * 2, uy * 2 - 1) == V_GROUND && Cover(gen, ux * 2 + 1, uy * 2 - 1) == V_GROUND;
            if (above)
            {
                i = uy * gen->cw + ux;
                gen->ashTree[i >> 3] |= 1 << (i & 7);
            }
        }
    }
    // An ash tree never stands on an ordinary tree: its trunk would cut
    // the round crown flat, a join no Hoenn map has. So in a column of
    // tree units everything above an ordinary tree is ordinary too. (An
    // ordinary tree above an ash tree is fine: its trunk stands on the
    // ground, with the ash tree's top on the ash below, as at a town's
    // seams.)
    while (changed)
    {
        changed = FALSE;
        for (uy = gen->ch - 2; uy >= 0; uy--)
        {
            for (ux = 0; ux < gen->cw; ux++)
            {
                if (!AshTreeU(gen, ux, uy))
                    continue;
                if (Cell(gen, ux, uy + 1) == C_TREE && !AshTreeU(gen, ux, uy + 1))
                {
                    i = uy * gen->cw + ux;
                    gen->ashTree[i >> 3] &= ~(1 << (i & 7));
                    changed = TRUE;
                }
            }
        }
    }
}

// A tree unit in the ash that may be drawn as an ash tree, if it has the
// ash above it for its tips (ChooseAshTrees' first test, but for that).
static bool8 AshTreeSpotU(struct Gen *gen, int ux, int uy)
{
    return Cell(gen, ux, uy) == C_TREE && AshZoneU(gen, ux, uy)
        && !(uy + 1 < gen->ch && !AshZoneU(gen, ux, uy + 1) && Cell(gen, ux, uy + 1) != C_TREE)
        && !InRouteSeam(gen, ux * 2, uy * 2) && !InRouteSeam(gen, ux * 2 + 1, uy * 2 + 1);
}

// An ordinary tree standing in the ash draws its crown's tips on grass
// above it, and grass round its crown: a square of green cut into the ash.
// The ash above it was taken back to grass only because of those tips
// (no 2x2 of ash beside a canopy). Give such a tree the ash above it, so it
// becomes an ash tree with its own tips, as all of Route 113's are.
static bool8 AshLikeF(struct Gen *gen, int fx, int fy, bool8 others);

static bool8 GroundUnderAshTrees(struct Gen *gen)
{
    int ux, uy, k, s;
    bool8 any = FALSE;
    for (uy = 1; uy < gen->ch; uy++)
    {
        YIELD();
        for (ux = 0; ux < gen->cw; ux++)
        {
            u8 above = Cell(gen, ux, uy - 1);
            bool8 ok = TRUE;
            if (AshTreeU(gen, ux, uy) || !AshTreeSpotU(gen, ux, uy)
             || (above != C_OPEN && above != C_ROAD && above != C_KEEP)
             || (HasNeighbor(gen, ux, uy - 1, C_BLDG, TRUE) && !gen->noLawn)
             || (uy + 1 < gen->ch && Cell(gen, ux, uy + 1) == C_TREE && !AshTreeU(gen, ux, uy + 1))
             // (Only beside ash: not one of a line of ordinary trees.)
             || (!AshLikeF(gen, ux * 2 - 1, uy * 2 - 1, FALSE) && !AshLikeF(gen, ux * 2 + 2, uy * 2 - 1, FALSE)))
                continue;
            for (k = 0; k < 2 && ok; k++)
            {
                int fx = ux * 2 + k, fy = uy * 2 - 1;
                u8 c = Cover(gen, fx, fy);
                if (!AshAt(gen, fx, fy) || InRouteSeam(gen, fx, fy) || (c != V_NONE && c != V_GROUND))
                    ok = FALSE;
                // (Under a person or an item the ground is only what it
                // looks like; a signpost is drawn on grass.)
                for (s = 0; s < gen->m->nSigns && ok; s++)
                    if (SignDrawn(gen->m->signs[s].kind) && gen->m->signs[s].x == fx && gen->m->signs[s].y == fy)
                        ok = FALSE;
            }
            if (!ok)
                continue;
            SetCover(gen, ux * 2, uy * 2 - 1, V_GROUND);
            SetCover(gen, ux * 2 + 1, uy * 2 - 1, V_GROUND);
            any = TRUE;
        }
    }
    return any;
}

// Is this block drawn as ash, for the route's ash to keep its 2x2 rule and
// for ash grass to have ash all round? Ash ground (not under an ordinary
// tree's canopy, which is drawn on grass), ash grass and ash trees. Past
// the map's edge the ground goes on next door. With others: rock, water,
// buildings and the like stand on any ground and count; with grass: not.
static bool8 AshLikeF(struct Gen *gen, int fx, int fy, bool8 others)
{
    u8 c, v;
    if (!InMap(gen, fx, fy))
        return TRUE;
    c = Cell(gen, fx >> 1, fy >> 1);
    if (c == C_TREE)
        return ThemedTreeU(gen, fx >> 1, fy >> 1);
    if (c != C_OPEN && c != C_ROAD && c != C_KEEP)
        return others;
    if (!AshAt(gen, fx, fy) || InRouteSeam(gen, fx, fy))
        return FALSE;
    v = Cover(gen, fx, fy);
    if (v == V_TALL)
        return AshGrassF(gen, fx, fy);
    return v == V_GROUND && !UnderCanopyF(gen, fx, fy);
}

// Ash grass in whole patches: a patch of tall grass (blocks joined across
// sides or corners) is all ash grass if every block of it and all round it
// is ash, else all ordinary tall grass, so it never ends in a hard line
// against the grass or sits beside green tall grass. Marked in gen->free,
// as long grass is (a route has one or the other).
static void PlanAshGrass(struct Gen *gen)
{
    int x, y, dx, dy, w = gen->m->w, h = gen->m->h;
    bool8 changed = TRUE;
    gen->ashGrassPlanned = FALSE;
    memset(gen->free, 0, sizeof(gen->free));
    for (y = 0; y < h; y++)
    {
        YIELD();
        for (x = 0; x < w; x++)
        {
            bool8 ok;
            int i = y * w + x;
            YIELD();
            if (Cover(gen, x, y) != V_TALL || !AshAt(gen, x, y) || InRouteSeam(gen, x, y) || UnderCanopyF(gen, x, y))
                continue;
            ok = TRUE;
            for (dy = -1; dy <= 1 && ok; dy++)
                for (dx = -1; dx <= 1 && ok; dx++)
                    if ((dx || dy) && InMap(gen, x + dx, y + dy) && Cover(gen, x + dx, y + dy) != V_TALL
                     && !AshLikeF(gen, x + dx, y + dy, FALSE))
                        ok = FALSE;
            if (ok)
                gen->free[i >> 3] |= 1 << (i & 7);
        }
    }
    while (changed)
    {
        changed = FALSE;
        for (y = 0; y < h; y++)
        {
            YIELD();
            for (x = 0; x < w; x++)
            {
                int i = y * w + x;
                if (!((gen->free[i >> 3] >> (i & 7)) & 1))
                    continue;
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++)
                    {
                        int j = (y + dy) * w + x + dx;
                        if (InMap(gen, x + dx, y + dy) && Cover(gen, x + dx, y + dy) == V_TALL
                         && !((gen->free[j >> 3] >> (j & 7)) & 1) && ((gen->free[i >> 3] >> (i & 7)) & 1))
                        {
                            gen->free[i >> 3] &= ~(1 << (i & 7));
                            changed = TRUE;
                        }
                    }
            }
        }
    }
    gen->ashGrassPlanned = TRUE;
}

// TidyPaths' rule for the route's ash: every block of it must sit in a 2x2
// of ash, or its edge set cannot draw it and it shows as a one-row scrap
// (between a tree line and a canopy, say). The leftovers go to grass.
// Returns TRUE if any went.
static bool8 TidyRouteAsh(struct Gen *gen)
{
    int x, y, sx, sy;
    bool8 any = FALSE;
    // (Dropped as found, with no list, to spare the generator's small
    // stack; the caller goes round again until nothing changes.)
    for (y = 0; y < gen->m->h; y++)
    {
        for (x = 0; x < gen->m->w; x++)
        {
            bool8 keep = FALSE;
            YIELD();
            if (Cover(gen, x, y) != V_GROUND || !AshAt(gen, x, y) || PinkF(gen, x, y) || UnderCanopyF(gen, x, y))
                continue;
            for (sy = y - 1; sy <= y && !keep; sy++)
                for (sx = x - 1; sx <= x && !keep; sx++)
                    if (AshLikeF(gen, sx, sy, TRUE) && AshLikeF(gen, sx + 1, sy, TRUE)
                     && AshLikeF(gen, sx, sy + 1, TRUE) && AshLikeF(gen, sx + 1, sy + 1, TRUE))
                        keep = TRUE;
            if (!keep)
            {
                SetCover(gen, x, y, V_NONE);
                any = TRUE;
            }
        }
    }
    // A square of ash on its own in the grass, away from the rest.
    for (y = 0; y < gen->m->h; y++)
    {
        YIELD();
        for (x = 0; x < gen->m->w; x++)
        {
            u16 list[5];
            int n, k, d;
            YIELD();
            if (Cover(gen, x, y) != V_GROUND || GroundOf(gen, x, y) != V_GROUND || PinkF(gen, x, y))
                continue;
            n = SmallPiece(gen, x, y, V_GROUND, list, 4);
            if (n == 0 || n > 4)
                continue;
            for (k = 0; k < n; k++)
            {
                for (d = 0; d < 4; d++)
                {
                    int bx = PX(list[k]) + (d == 1) - (d == 3), by = PY(list[k]) + (d == 2) - (d == 0);
                    if ((!InMap(gen, bx, by) || Cover(gen, bx, by) != V_GROUND) && AshLikeF(gen, bx, by, FALSE))
                        break;
                }
                if (d < 4)
                    break;
            }
            if (k < n)
                continue;
            for (k = 0; k < n; k++)
                SetCover(gen, PX(list[k]), PY(list[k]), V_NONE);
            any = TRUE;
        }
    }
    return any;
}

// The last look at a route's ash before it is drawn: trees, ash grass and
// the ash's 2x2 rule depend on each other, so round until they settle.
static void FinishRouteAsh(struct Gen *gen)
{
    int pass;
    gen->ashGrassPlanned = FALSE;
    if (gen->p->kind != RPLACE_ROUTE || gen->groundKind != GROUND_ASH || gen->townGround != TGROUND_GRASS
     || gen->longGrass)
        return;
    for (pass = 0; pass < 6; pass++)
    {
        bool8 grew = GroundUnderAshTrees(gen);
        ChooseAshTrees(gen);
        PlanAshGrass(gen);
        if (!TidyRouteAsh(gen) && !grew)
            return;
    }
    ChooseAshTrees(gen);
    PlanAshGrass(gen);
}

// Can open unit (x, y) turn to trees without cutting anything off? Its
// open neighbours, walking round it, must form one run (a simple point).
static bool8 PassableU(struct Gen *gen, int x, int y)
{
    u8 c;
    if (!InBounds(gen, x, y))
        return FALSE;
    c = Cell(gen, x, y);
    // A ledge is one way, so it does not count as a way round.
    return c == C_OPEN || c == C_ROAD || c == C_SHORE || c == C_KEEP || c == C_FORD;
}

static bool8 SafeToFillU(struct Gen *gen, int x, int y)
{
    static const s8 rx[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    static const s8 ry[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
    bool8 open[8];
    int i, runs = 0;
    for (i = 0; i < 8; i++)
        open[i] = PassableU(gen, x + rx[i], y + ry[i]);
    // Count the runs of open cells round the ring that hold a side
    // neighbour; a corner alone is not joined to this unit anyway.
    for (i = 0; i < 8 && open[i]; i++)
        ;
    if (i == 8)
        return TRUE;
    {
        int start = i, k;
        bool8 inRun = FALSE, orth = FALSE;
        for (k = 1; k <= 8; k++)
        {
            int j = (start + k) & 7;
            if (open[j])
            {
                inRun = TRUE;
                if (!(j & 1))
                    orth = TRUE;
            }
            else if (inRun)
            {
                runs += orth;
                inRun = orth = FALSE;
            }
        }
    }
    return runs <= 1;
}

// Along a side with a neighbour the trees stand one to three units deep
// (EdgeBand), so the tree line along the seam is ragged instead of one
// straight wall facing the next map's. Only plain ground with nothing on it
// turns to trees, never next to the way across, and never where it could
// cut off any ground.
static void ThickenSeamTrees(struct Gen *gen)
{
    u8 d;
    int depth;
    for (depth = 1; depth < 3; depth++)
    {
        for (d = 0; d < 4; d++)
        {
            const struct RogueLink *l = LinkOnSide(gen->p, d);
            int pos, n = (d == RDIR_NORTH || d == RDIR_SOUTH) ? gen->cw : gen->ch;
            if (l == NULL || gen->exitPos[d] == 0xFF)
                continue;
            for (pos = 1; pos < n - 1; pos++)
            {
                YIELD();
                int x, y, ox, oy, k;
                bool8 ok = TRUE;
                if (depth >= EdgeBand(gen->r, gen->p, d, pos) || Abs(pos - gen->exitPos[d] / 2) < 3)
                    continue;
                x = d == RDIR_WEST ? depth : d == RDIR_EAST ? gen->cw - 1 - depth : pos;
                y = d == RDIR_NORTH ? depth : d == RDIR_SOUTH ? gen->ch - 1 - depth : pos;
                ox = x - (d == RDIR_WEST ? 1 : d == RDIR_EAST ? -1 : 0);
                oy = y - (d == RDIR_NORTH ? 1 : d == RDIR_SOUTH ? -1 : 0);
                if (Cell(gen, x, y) != C_OPEN || Cell(gen, ox, oy) != C_TREE || HasNeighbor(gen, x, y, C_LEDGE, TRUE))
                    continue;
                for (k = 0; k < 4 && ok; k++)
                {
                    int fx = x * 2 + (k & 1), fy = y * 2 + (k >> 1);
                    u8 cv = Cover(gen, fx, fy);
                    if ((cv != V_NONE && cv != V_TALL && cv != V_FLOWER) || IsOccupied(gen, fx, fy))
                        ok = FALSE;
                }
                if (!ok || !SafeToFillU(gen, x, y))
                    continue;
                SetCell(gen, x, y, C_TREE);
                for (k = 0; k < 4; k++)
                    SetCover(gen, x * 2 + (k & 1), y * 2 + (k >> 1), V_NONE);
            }
        }
    }
}

// How a route of each type looks.
static void ChooseRouteStyle(struct Gen *gen)
{
    const struct RoguePlace *p = gen->p;
    gen->groundKind = GROUND_SAND;
    switch (p->type)
    {
    case RROUTE_WOODS:
        gen->woods = TRUE;
        break;
    case RROUTE_LONGGRASS:
        // v1.1 land: Routes 119 and 120, on Fortree's tileset, among the
        // small trees of deep woods, as there.
        gen->longGrass = TRUE;
        gen->woods = TRUE;
        break;
    case RROUTE_FIELD:
    case RROUTE_FOREST:
    case RROUTE_RIVER:
    case RROUTE_LAKE:
        // Long grass grows round Fortree, on Routes 119 and 120. Its lower
        // edge (blades over the ground below them) is only in Fortree's
        // tileset, so only routes drawn with it have long grass: most
        // meadows, half the others. It also hides the player and the
        // trainers coming at them.
        // (River and lake routes all have Fortree's tiles since v1.1, for
        // their bridges: long grass only beside a Fortree town, as before.)
        if (p->theme == RTHEME_TREETOP && !WaterNoLongGrass(gen))
            gen->longGrass = RogueRng_Hash(gen->r->seed, 6000 + (u32)(p - gen->r->places)) % 4
                           < (p->type == RROUTE_FIELD ? 3u : 2u);
        break;
    case RROUTE_HILLS:
        // A mountain pass (rogue_gen_rock.h): rock all round, with a grove
        // of trees here and there, as on Routes 112, 114 and 115.
        gen->rocky = TRUE;
        gen->rockTreeChance = 25; // (v1.1c land2: was 15, too much rock)
        break;
    case RROUTE_DESERT:
        // On the way to an ash town the desert is volcanic ash, with ash
        // grass and trees, as on Route 113. Otherwise it is sand and rock.
        if (p->theme == RTHEME_ASH)
        {
            gen->groundKind = GROUND_ASH;
            // The first route never starts the run in ash: it fades. An
            // ash field keeps its ash back from neighbours without it.
            PlanAshFade(gen);
        }
        else
        {
            gen->rocky = TRUE;
            gen->rockFoot = sRockOnSand;
        }
        break;
    }
    if (p->theme == RTHEME_ASH && p->type != RROUTE_DESERT && p->type != RROUTE_MOUNTAIN && p->type != RROUTE_SEA
     && p->type != RROUTE_HILLS)
        PlanAshFade(gen);
}

// Keeps a waypoint clear of the sea.
static void ClampFromSea(struct Gen *gen, int *x, int *y)
{
    u8 d = gen->p->seaSide;
    int m = SEA_DEPTH + 2;
    if (d == RDIR_WEST && *x < m) *x = m;
    if (d == RDIR_EAST && *x > gen->cw - 1 - m) *x = gen->cw - 1 - m;
    if (d == RDIR_NORTH && *y < m) *y = m;
    if (d == RDIR_SOUTH && *y > gen->ch - 1 - m) *y = gen->ch - 1 - m;
}

static void OpenAlong(struct Gen *gen, int x0, int y0, int x1, int y1)
{
    while (x0 != x1 || y0 != y1)
    {
        if (x0 != x1)
            x0 += x1 > x0 ? 1 : -1;
        else
            y0 += y1 > y0 ? 1 : -1;
        if (Cell(gen, x0, y0) == C_TREE && !IsEdge(gen, x0, y0))
            SetCell(gen, x0, y0, C_OPEN);
    }
}

// A way down from the road to the beach, from its nearest stretch of sand.
static void JoinBeach(struct Gen *gen)
{
    int x, y, i, best = -1, bestDist = 9999, bx = 0, by = 0;
    for (y = 1; y < gen->ch - 1; y++)
    {
        YIELD();
        for (x = 1; x < gen->cw - 1; x++)
        {
            YIELD();
            if (!BitU(gen->beach, gen, x, y) || Cell(gen, x, y) != C_OPEN)
                continue;
            for (i = 0; i < gen->nPath; i++)
            {
                YIELD();
                int dist = Abs(gen->path[i] % gen->cw - x) + Abs(gen->path[i] / gen->cw - y);
                if (gen->guardEnd && i <= gen->guardEnd + 1)
                    continue;
                if (dist < bestDist)
                {
                    bestDist = dist;
                    best = i;
                    bx = x;
                    by = y;
                }
            }
        }
    }
    if (best >= 0)
        OpenAlong(gen, bx, by, gen->path[best] % gen->cw, gen->path[best] / gen->cw);
}

// The beach marks become sand cover, a unit at a time.
static void PaintBeach(struct Gen *gen)
{
    int ux, uy;
    for (uy = 0; uy < gen->ch; uy++)
    {
        for (ux = 0; ux < gen->cw; ux++)
        {
            int i = uy * gen->cw + ux;
            if ((gen->beach[i >> 3] >> (i & 7)) & 1)
                PaintUnit(gen, ux, uy, V_GROUND);
        }
    }
}

// Special ground, or what ash needs no edge against: ash-covered grass, and
// trees or rock, so a strip of ash may run between trees.
static bool8 AshOrGround(struct Gen *gen, int fx, int fy, bool8 ash)
{
    if (ash)
        return IsGroundLike(gen, fx, fy);
    return Cover(gen, fx, fy) == V_GROUND;
}

// A sand desert's ground turns to grass in a ragged band two to four units
// deep along a side that meets a place without sand, so the sand does not
// stop at the seam in a straight line.
static bool8 DesertGrassBandU(struct Gen *gen, int ux, int uy)
{
    u8 d;
    for (d = 0; d < 4; d++)
    {
        const struct RogueLink *l = LinkOnSide(gen->p, d);
        const struct RoguePlace *t;
        int pos, dist;
        if (l == NULL)
            continue;
        t = &gen->r->places[l->target];
        if (t->kind == RPLACE_CAVE || (t->kind == RPLACE_ROUTE && t->type == RROUTE_DESERT && t->theme != RTHEME_ASH))
            continue;
        dist = EdgeDist(gen, d, ux, uy, &pos);
        if (dist <= EdgeBand(gen->r, gen->p, d, pos + 5))
            return TRUE;
    }
    return FALSE;
}

// Deserts and ash fields: every stretch of open ground becomes sand or ash,
// except the grass around tall grass (an oasis) in a sand desert, and the
// ground near a neighbour with another tileset in an ash field.
static void PaintSpecialGround(struct Gen *gen)
{
    int w = gen->m->w, h = gen->m->h, x, y;
    bool8 ash = (gen->groundKind == GROUND_ASH);
    for (y = 0; y < h; y++)
    {
        YIELD();
        for (x = 0; x < w; x++)
        {
            u8 c = Cell(gen, x >> 1, y >> 1), cover = Cover(gen, x, y);
            int dx, dy;
            bool8 oasis = FALSE;
            if (c != C_OPEN && c != C_ROAD && !(ash && c == C_KEEP))
                continue;
            if (ash && (!AshAt(gen, x, y) || InRouteSeam(gen, x, y)))
                continue;
            if (!ash && DesertGrassBandU(gen, x >> 1, y >> 1))
                continue;
            // A lawn round a rest house: its edges are drawn on grass.
            if ((HasNeighbor(gen, x >> 1, y >> 1, C_BLDG, TRUE) || c == C_BLDG) && !gen->noLawn)
                continue;
            // Bare ash under the crown of a tree, where its tips are drawn.
            if (ash && cover == V_TALL && (y & 1) && y + 1 < h && Cell(gen, x >> 1, (y >> 1) + 1) == C_TREE)
                cover = V_NONE;
            // The pink dirt by an ash town is bare, and so is the ash next
            // to it: ash grass has no edge against it.
            if (ash && cover == V_TALL && (PinkU(gen, x >> 1, y >> 1) || PinkU(gen, (x >> 1) - 1, y >> 1)
             || PinkU(gen, (x >> 1) + 1, y >> 1) || PinkU(gen, x >> 1, (y >> 1) - 1) || PinkU(gen, x >> 1, (y >> 1) + 1)))
                cover = V_NONE;
            if (cover != V_NONE && cover != V_PATH && cover != V_SAND && cover != V_FLOWER && cover != V_ROAD)
                continue;
            // The road is sand all the way, even through an oasis.
            for (dy = -1; dy <= 1 && !ash && cover != V_PATH; dy++)
                for (dx = -1; dx <= 1; dx++)
                    if (InMap(gen, x + dx, y + dy) && Cover(gen, x + dx, y + dy) == V_TALL)
                        oasis = TRUE;
            if (!oasis)
                SetCover(gen, x, y, V_GROUND);
        }
    }
    // Ash grass needs ash all round; near the seam it is plain tall grass.
    // Every block of special ground must sit in a 2x2 of it, or its edges
    // cannot be drawn.
    for (y = 0; y < h; y++)
    {
        YIELD();
        for (x = 0; x < w; x++)
        {
            int sx, sy;
            bool8 keep = FALSE;
            if (Cover(gen, x, y) != V_GROUND)
                continue;
            for (sy = y - 1; sy <= y && !keep; sy++)
                for (sx = x - 1; sx <= x && !keep; sx++)
                    if (sx >= 0 && sy >= 0 && sx + 1 < w && sy + 1 < h
                     && AshOrGround(gen, sx, sy, ash) && AshOrGround(gen, sx + 1, sy, ash)
                     && AshOrGround(gen, sx, sy + 1, ash) && AshOrGround(gen, sx + 1, sy + 1, ash))
                        keep = TRUE;
            if (!keep)
                SetCover(gen, x, y, V_NONE);
        }
    }
}

// Rocks standing in the sea.
static void PlaceSeaRocks(struct Gen *gen, int count)
{
    int tries, placed = 0;
    for (tries = 0; tries < 80 && placed < count; tries++)
    {
        int ux = RAND(gen->cw), uy = RAND(gen->ch), dx, dy;
        bool8 ok = TRUE;
        for (dy = -1; dy <= 1 && ok; dy++)
            for (dx = -1; dx <= 1 && ok; dx++)
                if (Cell(gen, ux + dx, uy + dy) != C_SEA || !InBounds(gen, ux + dx, uy + dy))
                    ok = FALSE;
        if (!ok || !AddDecor(gen, RAND(2) ? sSeaRock : sSeaRock2, ux * 2, uy * 2, 2, 2))
            continue;
        placed++;
    }
}

// ---------------------------------------------------------------------------
// Open routes
//
// Forest, field, lake, river and coast routes start as a forest with the
// trail cut through, then open out the way Routes 101 to 104, 110 and 117
// do: meadows along the trail, tree lines between them, clumps and single
// trees standing in the open, ponds, ledges and tall grass. Between the
// meadows the trail squeezes through a gap in the trees, and that is where
// the trainers wait. Woods keep their dense forest, and hills, deserts and
// Victory Road their rock.

enum { LAYOUT_GLADE, LAYOUT_BELT, LAYOUT_GROVE, LAYOUT_COUNT };
#define MAX_SQUEEZE 6

bool8 RogueGen_TrainerByWater(const struct RogueRegion *r, u8 place, u8 index)
{
    const struct RoguePlace *p = &r->places[place];
    u32 pct;
    if (p->kind != RPLACE_ROUTE)
        return FALSE;
    switch (p->type)
    {
    case RROUTE_COAST: pct = 75; break; // beach people: tubers, sailors, fishermen
    case RROUTE_SEA:   pct = 70; break;
    case RROUTE_LAKE:
    case RROUTE_RIVER: pct = 45; break;
    default:           return FALSE;
    }
    return RogueRng_Hash(r->seed, 7300 + place * 16 + index) % 100 < pct;
}

static bool8 GreenRoute(const struct RoguePlace *p)
{
    return p->kind == RPLACE_ROUTE
        && (p->type == RROUTE_FOREST || p->type == RROUTE_FIELD || p->type == RROUTE_LAKE
         || p->type == RROUTE_RIVER || p->type == RROUTE_COAST
         || p->type == RROUTE_FLOWERS || p->type == RROUTE_LONGGRASS);
}

// Trees round a unit of the trail stay trees: no meadow may open beside it.
static void KeepAround(struct Gen *gen, int x, int y)
{
    int dx, dy;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
            if (Cell(gen, x + dx, y + dy) == C_TREE)
                SetBitU(gen->keep, gen, x + dx, y + dy);
}

// TRUE if a unit lies within r units of an edge opening, or of the narrow
// way in past a side route's obstacle, or of a lane to one.
static bool8 NearExitU(struct Gen *gen, int x, int y, int r)
{
    const struct RoguePlace *p = gen->p;
    int i, k;
    for (i = 0; i < p->nLinks; i++)
    {
        const struct RogueLink *l = &p->links[i];
        int step = GateLaneStep(gen, l);
        int deep = (p->obstacle && l->dir == p->guardDir) ? 6 : step ? step + 2 : 1;
        for (k = 0; k < deep; k++)
        {
            int ex, ey;
            EdgeCell(gen, l->dir, l->pos / 2, k, &ex, &ey);
            if (Abs(x - ex) + Abs(y - ey) <= r)
                return TRUE;
        }
    }
    return FALSE;
}

// The nearest unit to (x, y) that is not water, rock or a building.
static void LandNear(struct Gen *gen, int *x, int *y)
{
    int r, dx, dy;
    for (r = 0; r < 8; r++)
    {
        for (dy = -r; dy <= r; dy++)
        {
            for (dx = -r; dx <= r; dx++)
            {
                int nx = *x + dx, ny = *y + dy;
                u8 c;
                if (Abs(dx) != r && Abs(dy) != r)
                    continue;
                if (!InBounds(gen, nx, ny) || IsEdge(gen, nx, ny))
                    continue;
                c = Cell(gen, nx, ny);
                if (c == C_TREE || c == C_OPEN || c == C_ROAD)
                {
                    *x = nx;
                    *y = ny;
                    return;
                }
            }
        }
    }
}

// The shortest way between two units, carved as road. It goes round water,
// rock and buildings, but may wade across a river where the way over is
// short; those units become a ford once the road is settled.
static bool8 CarveRoadBfs(struct Gen *gen, int x0, int y0, int x1, int y1)
{
    u8 *dist = gen->cover; // unused until the ground cover is painted
    s16 *prev = sBfsPrev;
    int n = gen->cw * gen->ch, head = 0, tail = 0, i, len = 0, cur;
    u16 *trail = sBfsQueue;

    for (i = 0; i < n; i++)
    {
        dist[i] = 0xFF;
        prev[i] = -1;
    }
    if (!InBounds(gen, x0, y0) || !InBounds(gen, x1, y1))
        return FALSE;
    dist[y0 * gen->cw + x0] = 0;
    sBfsQueue[tail++] = PACK(x0, y0);
    while (head != tail)
    {
        int c = sBfsQueue[head++ & (BFS_RING - 1)];
        int cx = PX(c), cy = PY(c), ci = cy * gen->cw + cx;
        u8 k, start = RAND(4);
        YIELD();
        for (k = 0; k < 4; k++)
        {
            u8 d = (start + k) & 3;
            int nx = cx + sDirDX[d], ny = cy + sDirDY[d], ni, cost;
            u8 t;
            if (!InBounds(gen, nx, ny))
                continue;
            ni = ny * gen->cw + nx;
            t = gen->g[ni];
            if (nx == x1 && ny == y1)
                cost = 1;
            // Never through a lane to a side route's obstacle or its walls.
            else if (IsEdge(gen, nx, ny) || BitU(gen->keep, gen, nx, ny) || InGateLaneF(gen, nx * 2, ny * 2))
                continue;
            else if (t == C_TREE || t == C_OPEN || t == C_ROAD || t == C_FORD)
                cost = 1;
            else if (t == C_SEA && BitU(gen->river, gen, nx, ny))
                cost = 6;
            else
                continue;
            if (dist[ci] + cost >= dist[ni] || dist[ci] + cost >= 0xFF)
                continue;
            dist[ni] = dist[ci] + cost;
            prev[ni] = ci;
            if (tail - head < BFS_RING)
                sBfsQueue[tail++ & (BFS_RING - 1)] = PACK(nx, ny);
        }
    }
    cur = y1 * gen->cw + x1;
    if (dist[cur] == 0xFF)
    {
        memset(dist, 0, n);
        return FALSE;
    }
    // Back from the end into the queue, then forward as road.
    for (; cur >= 0 && len < BFS_RING; cur = prev[cur])
        trail[len++] = cur;
    memset(dist, 0, n);
    while (len > 0)
    {
        cur = trail[--len];
        AddRoadCell(gen, cur % gen->cw, cur / gen->cw);
    }
    return TRUE;
}

// Carve the road between two units: straight runs through the forest, or
// on open routes a way round the water.
static bool8 CarveWay(struct Gen *gen, int x0, int y0, int x1, int y1)
{
    if (!GreenRoute(gen->p))
    {
        CarveRoad(gen, x0, y0, x1, y1);
        return TRUE;
    }
    return CarveRoadBfs(gen, x0, y0, x1, y1);
}

// Road units in a river are a ford; river units the road left again are
// water once more.
static void SettleFords(struct Gen *gen)
{
    int x, y;
    for (y = 0; y < gen->ch; y++)
    {
        for (x = 0; x < gen->cw; x++)
        {
            u8 c = Cell(gen, x, y);
            if (!BitU(gen->river, gen, x, y))
                continue;
            if (c == C_ROAD)
                SetCell(gen, x, y, C_FORD);
            else if (c == C_OPEN || c == C_TREE)
                SetCell(gen, x, y, C_SEA);
        }
    }
}

// An ellipse of units, tested without division.
static bool8 InEllipse(int dx, int dy, int rx, int ry)
{
    return dx * dx * ry * ry + dy * dy * rx * rx <= rx * rx * ry * ry;
}

// The box round a blob's marked units. The passes below only look inside it
// and one unit round it, where a unit can have a marked neighbour; everything
// further out has none, so they come out as if they had swept the whole map,
// at a fraction of the cost (they ran dozens of times a route).
struct MarkBox
{
    int x0, y0, x1, y1;
};

static void MarkBoxAdd(struct MarkBox *b, int x, int y)
{
    if (x < b->x0) b->x0 = x;
    if (x > b->x1) b->x1 = x;
    if (y < b->y0) b->y0 = y;
    if (y > b->y1) b->y1 = y;
}

static void MarkBoxOf(struct Gen *gen, const u8 *mark, struct MarkBox *b)
{
    int i, n = gen->cw * gen->ch;
    b->x0 = gen->cw;
    b->y0 = gen->ch;
    b->x1 = b->y1 = -1;
    for (i = 0; i < n; i++)
        if (mark[i] & 1)
            MarkBoxAdd(b, i % gen->cw, i / gen->cw);
}

// Round off a blob of marked units: drop spikes, fill notches.
static void SmoothMarks(struct Gen *gen, u8 *mark)
{
    int pass, x, y;
    struct MarkBox b;
    MarkBoxOf(gen, mark, &b);
    for (pass = 0; pass < 2; pass++)
    {
        for (y = b.y0 > 1 ? b.y0 - 1 : 1; y < gen->ch - 1 && y <= b.y1 + 1; y++)
        {
            YIELD();
            for (x = b.x0 > 1 ? b.x0 - 1 : 1; x < gen->cw - 1 && x <= b.x1 + 1; x++)
            {
                int i = y * gen->cw + x, n = 0;
                u8 d;
                for (d = 0; d < 4; d++)
                    n += mark[(y + sDirDY[d]) * gen->cw + x + sDirDX[d]] & 1;
                if ((mark[i] & 1) && n <= 1)
                {
                    mark[i] = 2; // dropped
                }
                else if (!(mark[i] & 1) && n >= 3 && mark[i] != 2)
                {
                    mark[i] = 1;
                    MarkBoxAdd(&b, x, y);
                }
            }
        }
        for (y = 0; y < gen->ch * gen->cw; y++)
            if (mark[y] == 2)
                mark[y] = 0;
    }
}

// Bite into the edge of a blob here and there, and push it out elsewhere,
// so it does not come out as a clean ellipse.
static void RoughenMarks(struct Gen *gen, u8 *mark)
{
    int x, y;
    struct MarkBox b;
    MarkBoxOf(gen, mark, &b);
    for (y = b.y0 > 1 ? b.y0 - 1 : 1; y < gen->ch - 1 && y <= b.y1 + 1; y++)
    {
        YIELD();
        for (x = b.x0 > 1 ? b.x0 - 1 : 1; x < gen->cw - 1 && x <= b.x1 + 1; x++)
        {
            int i = y * gen->cw + x, n = 0;
            u8 d;
            for (d = 0; d < 4; d++)
                n += mark[(y + sDirDY[d]) * gen->cw + x + sDirDX[d]] & 1;
            if (n == 0 || n == 4)
                continue;
            if ((mark[i] & 1) && CHANCE(22))
            {
                mark[i] = 4; // bitten out, after the pass
            }
            else if (!mark[i] && CHANCE(22))
            {
                mark[i] = 5; // pushed out, after the pass
                MarkBoxAdd(&b, x, y);
            }
            else
            {
                continue;
            }
            // Two units along the edge, or smoothing would undo it: a bay
            // or a point one unit across is filled in or cut off again.
            {
                bool8 alongX = !(mark[i - gen->cw] & 1) != !(mark[i + gen->cw] & 1);
                int j = alongX ? i + (CHANCE(50) ? 1 : -1) : i + (CHANCE(50) ? gen->cw : -gen->cw);
                int jx = j % gen->cw, jy = j / gen->cw;
                if (jx >= 1 && jy >= 1 && jx < gen->cw - 1 && jy < gen->ch - 1 && (mark[j] & 1) == (mark[i] == 4))
                {
                    mark[j] = mark[i] == 4 ? 6 : 7;
                    if (mark[j] == 7)
                        MarkBoxAdd(&b, jx, jy);
                }
            }
        }
    }
    for (y = 0; y < gen->ch * gen->cw; y++)
        mark[y] = (mark[y] == 1 || mark[y] == 5 || mark[y] == 7) ? 1 : 0;
}

// Whether a unit may be a lake's or pond's water.
static bool8 BlobUnitFits(struct Gen *gen, int ux, int uy, int top)
{
    return !(ux < 2 || uy < top || ux > gen->cw - 3 || uy > gen->ch - 3 || gen->g[uy * gen->cw + ux] != C_TREE
          || NearExitU(gen, ux, uy, 3) || BitU(gen->keep, gen, ux, uy) || AshZoneU(gen, ux, uy));
}

// Water in a blob of overlapping ellipses: a lake or a pond. It may only
// replace forest, keeps off the edges and the openings, and must not cut
// anything off. Returns the number of units, or 0.
static int PlaceBlobWater(struct Gen *gen, int cx, int cy, int rx, int ry, int lobes, int top)
{
    u8 *mark = gen->cover; // unused until the ground cover is painted
    int n = gen->cw * gen->ch, i, k, x, y, area = 0;
    // The middle of the first lobe, two units in from its rim, stays water
    // through the roughening: if any of it cannot be water, the blob would
    // fail at the end, so give up before the costly passes. (Lakes and
    // ponds were tried dozens of times a route, most of them failing.)
    {
        int crx = rx > 2 ? rx - 2 : 0, cry = ry > 2 ? ry - 2 : 0;
        for (y = cy - cry; y <= cy + cry; y++)
        {
            YIELD();
            for (x = cx - crx; x <= cx + crx; x++)
                if (InEllipse(x - cx, y - cy, crx, cry) && !BlobUnitFits(gen, x, y, top))
                    return 0;
        }
    }
    memset(mark, 0, n);
    for (k = 0; k <= lobes; k++)
    {
        // Each lobe is centred toward the rim of the first, so it bulges
        // out of it: bays and points, not one clean oval.
        int ox = 0, oy = 0, lrx = rx, lry = ry, t;
        for (t = 0; k && t < 12; t++)
        {
            ox = (int)RAND(2 * rx + 1) - rx;
            oy = (int)RAND(2 * ry + 1) - ry;
            if (InEllipse(ox, oy, rx, ry) && (t >= 8 || !InEllipse(ox * 2, oy * 2, rx, ry)))
                break;
        }
        if (k)
        {
            lrx = 1 + RAND(rx * 2 / 3 + 1);
            lry = 1 + RAND(ry * 2 / 3 + 1);
        }
        for (y = cy + oy - lry; y <= cy + oy + lry; y++)
        {
            YIELD();
            for (x = cx + ox - lrx; x <= cx + ox + lrx; x++)
                if (InBounds(gen, x, y) && InEllipse(x - cx - ox, y - cy - oy, lrx, lry))
                    mark[y * gen->cw + x] = 1;
        }
    }
    RoughenMarks(gen, mark);
    SmoothMarks(gen, mark);
    if (rx >= 4 || ry >= 4)
    {
        // A big lake gets a second, so its long sides wander too.
        RoughenMarks(gen, mark);
        SmoothMarks(gen, mark);
    }
    for (i = 0; i < n; i++)
    {
        YIELD();
        int ux = i % gen->cw, uy = i / gen->cw;
        if (!mark[i])
            continue;
        if (!BlobUnitFits(gen, ux, uy, top))
        {
            memset(mark, 0, n);
            return 0;
        }
        area++;
    }
    {
        // No long thin strips: they read as a ditch, not a pond.
        int x0 = gen->cw, x1 = -1, y0 = gen->ch, y1 = -1, bw, bh;
        for (i = 0; i < n; i++)
        {
            YIELD();
            if (!mark[i])
                continue;
            if (i % gen->cw < x0) x0 = i % gen->cw;
            if (i % gen->cw > x1) x1 = i % gen->cw;
            if (i / gen->cw < y0) y0 = i / gen->cw;
            if (i / gen->cw > y1) y1 = i / gen->cw;
        }
        bw = x1 - x0 + 1;
        bh = y1 - y0 + 1;
        if (bw < 3 || bh < 2 || bw > bh * 4 || bh > bw * 3)
        {
            memset(mark, 0, n);
            return 0;
        }
    }
    for (i = 0; i < n; i++)
        if (mark[i])
            gen->g[i] = C_SEA;
    memset(mark, 0, n);
    return area;
}

// Lake routes: one big lake in the middle that the trail goes round, as on
// Routes 120 and 103.
static bool8 ShapeBigLake(struct Gen *gen, int top)
{
    int tries;
    for (tries = 0; tries < 24; tries++)
    {
        int h = gen->ch - top;
        int cx = gen->cw / 2 + (int)RAND(5) - 2, cy = top + h / 2 + (int)RAND(3) - 1;
        int rx = gen->cw * (17 + RAND(9)) / 100, ry = h * (17 + RAND(9)) / 100;
        if (rx < 3) rx = 3;
        if (ry < 2) ry = 2;
        // Clear of the map's edges, where it could never go: on a narrow
        // map a lake off the middle always ran into them.
        if (cx < 2 + rx && 2 + rx <= gen->cw - 3 - rx)
            cx = 2 + rx;
        if (cx > gen->cw - 3 - rx && 2 + rx <= gen->cw - 3 - rx)
            cx = gen->cw - 3 - rx;
        int area = PlaceBlobWater(gen, cx, cy, rx, ry, 2 + RAND(3), top + 1);
        if (area * 100 >= gen->cw * gen->ch * 12)
            return TRUE;
        // Too small a lake stays as a pond, and the next try would have to
        // keep clear of it: they nearly all failed, at a cost.
        if (area)
            return FALSE;
    }
    return FALSE;
}

// Rasterise a river from one unit to another, one unit at a time with no
// diagonal steps, two units wide (a little more on the slants).
static bool8 RiverLine(struct Gen *gen, int x0, int y0, int x1, int y1)
{
    int x = x0, y = y0, guard = 0, tx = Abs(x1 - x0), ty = Abs(y1 - y0);
    while (guard++ < 200)
    {
        int k, dx = Abs(x1 - x), dy = Abs(y1 - y);
        bool8 stepX;
        for (k = 0; k < 4; k++)
        {
            int rx = x + (k & 1), ry = y + (k >> 1);
            u8 c;
            if (!InBounds(gen, rx, ry))
                continue;
            c = Cell(gen, rx, ry);
            if ((c != C_TREE && c != C_SEA) || NearExitU(gen, rx, ry, 2) || AshZoneU(gen, rx, ry))
                return FALSE;
            SetCell(gen, rx, ry, C_SEA);
            SetBitU(gen->river, gen, rx, ry);
        }
        if (dx == 0 && dy == 0)
            return TRUE;
        // Keep near the straight line, with a little wander.
        if (dx == 0)
            stepX = FALSE;
        else if (dy == 0)
            stepX = TRUE;
        else
            stepX = (dx * (ty + 1) >= dy * (tx + 1)) ? !CHANCE(20) : CHANCE(20);
        if (stepX)
            x += x1 > x ? 1 : -1;
        else
            y += y1 > y ? 1 : -1;
    }
    return FALSE;
}

// Undo a river that did not work out: its units were all forest.
static void ClearRiver(struct Gen *gen)
{
    int x, y;
    for (y = 0; y < gen->ch; y++)
    {
        YIELD();
        for (x = 0; x < gen->cw; x++)
            if (BitU(gen->river, gen, x, y))
                SetCell(gen, x, y, C_TREE);
    }
    memset(gen->river, 0, sizeof(gen->river));
}

// The unit just inside the opening on side d, or the middle of the map.
static void OpeningUnit(struct Gen *gen, u8 d, int top, int *x, int *y)
{
    const struct RogueLink *l = d == RDIR_NONE ? NULL : LinkOnSide(gen->p, d);
    if (l == NULL)
    {
        *x = gen->cw / 2;
        *y = top + (gen->ch - top) / 2;
        return;
    }
    EdgeCell(gen, d, l->pos / 2, 1, x, y);
}

// How close the river comes to the straight way from (x0, y0) to (x1, y1),
// in units.
static int RiverGap(struct Gen *gen, int x0, int y0, int x1, int y1)
{
    int best = 99, k, x, y;
    for (k = 0; k <= 16; k++)
    {
        int px = x0 + (x1 - x0) * k / 16, py = y0 + (y1 - y0) * k / 16;
        for (y = py - 3; y <= py + 3; y++)
        {
            YIELD();
            for (x = px - 3; x <= px + 3; x++)
            {
                int dd = Abs(x - px) > Abs(y - py) ? Abs(x - px) : Abs(y - py);
                if (dd < best && BitU(gen->river, gen, x, y))
                    best = dd;
            }
        }
    }
    return best;
}

// A river across the map from one side with no way out to another, with a
// bend or two, or from such a side into a pool. It always comes within a
// unit or two of the straight way between the openings (inDir and outDir),
// so the trail fords it or runs along its bank, as on Routes 110 and 119.
static bool8 ShapeRiverAcrossFrom(struct Gen *gen, int top, u8 inDir, u8 outDir, bool8 linked);

static bool8 ShapeRiverAcross(struct Gen *gen, int top, u8 inDir, u8 outDir)
{
    if (ShapeRiverAcrossFrom(gen, top, inDir, outDir, FALSE))
        return TRUE;
    // A map with ways out on every free side (or a rock face along the top)
    // that keeps failing: from a side with a way out too, away from it.
    if (gen->attempt >= 10)
        return ShapeRiverAcrossFrom(gen, top, inDir, outDir, TRUE);
    return FALSE;
}

static bool8 ShapeRiverAcrossFrom(struct Gen *gen, int top, u8 inDir, u8 outDir, bool8 linked)
{
    const struct RoguePlace *p = gen->p;
    u8 sides[4], nSides = 0, d;
    int tries, wx0, wy0, wx1, wy1;

    for (d = 0; d < 4; d++)
        if ((linked || !HasSide(p, d)) && !(d == RDIR_NORTH && p->mouth != 0xFF))
            sides[nSides++] = d;
    if (nSides == 0)
        return FALSE;
    OpeningUnit(gen, inDir, top, &wx0, &wy0);
    OpeningUnit(gen, outDir, top, &wx1, &wy1);
    if (outDir == RDIR_NONE && p->mouth != 0xFF)
    {
        wx1 = gen->cw / 2;
        wy1 = top + 1;
    }
    for (tries = 0; tries < 16; tries++)
    {
        YIELD();
        u8 a = sides[RAND(nSides)], b = 0xFF;
        int ax, ay, bx, by, px[4], py[4], i, n = 0;
        bool8 ok = TRUE, pool = tries >= 8;
        for (i = 0; i < nSides; i++)
            if (sides[i] == Opposite(a))
                b = sides[i];
        if (b == 0xFF && nSides > 1 && CHANCE(60))
        {
            do
                b = sides[RAND(nSides)];
            while (b == a);
        }
        EdgeCell(gen, a, 3 + RAND(EdgeLength(gen, a) - 6), 0, &ax, &ay);
        if (pool)
        {
            // Into a pool beside the way, halfway along it.
            b = 0xFF;
            bx = (wx0 + wx1) / 2 + (int)RAND(5) - 2;
            by = (wy0 + wy1) / 2 + (int)RAND(5) - 2;
            if (bx < 4) bx = 4;
            if (bx > gen->cw - 6) bx = gen->cw - 6;
            if (by < top + 3) by = top + 3;
            if (by > gen->ch - 5) by = gen->ch - 5;
        }
        else if (b != 0xFF)
        {
            EdgeCell(gen, b, 3 + RAND(EdgeLength(gen, b) - 6), 0, &bx, &by);
        }
        else
        {
            // Into a pool well inside the map.
            bx = gen->cw / 4 + RAND(gen->cw / 2);
            by = top + 2 + RAND(gen->ch - top - 5);
        }
        // Bends: two points pushed off the straight line.
        px[0] = ax; py[0] = ay;
        px[3] = bx; py[3] = by;
        for (i = 1; i <= 2; i++)
        {
            bool8 across = (a == RDIR_NORTH || a == RDIR_SOUTH);
            int jx = across ? gen->cw / 4 : 2, jy = across ? 2 : (gen->ch - top) / 4;
            px[i] = ax + (bx - ax) * i / 3 + (int)RAND(2 * jx + 1) - jx;
            py[i] = ay + (by - ay) * i / 3 + (int)RAND(2 * jy + 1) - jy;
            if (px[i] < 2) px[i] = 2;
            if (px[i] > gen->cw - 4) px[i] = gen->cw - 4;
            if (py[i] < top + 1) py[i] = top + 1;
            if (py[i] > gen->ch - 4) py[i] = gen->ch - 4;
        }
        // The far edge: the 2x2 brush reaches one unit right and down.
        if (b == RDIR_EAST) px[3] = gen->cw - 2;
        if (b == RDIR_SOUTH) py[3] = gen->ch - 2;
        if (a == RDIR_EAST) px[0] = gen->cw - 2;
        if (a == RDIR_SOUTH) py[0] = gen->ch - 2;
        // A side with a way out has a place beyond whose ground meets this
        // map's at the seam, with no river of its own: the river ends two
        // units short of it, in its bank, so water never meets land across
        // the seam.
        if (linked)
        {
            int e;
            for (e = 0; e < 4; e += 3)
            {
                u8 side = e == 0 ? a : b;
                if (side == 0xFF || !HasSide(p, side))
                    continue;
                px[e] += side == RDIR_WEST ? 2 : side == RDIR_EAST ? -2 : 0;
                py[e] += side == RDIR_NORTH ? 2 : side == RDIR_SOUTH ? -2 : 0;
            }
        }
        for (i = 0; i < 3 && ok; i++)
            ok = RiverLine(gen, px[i], py[i], px[i + 1], py[i + 1]);
        if (ok && b == 0xFF)
        {
            int x, y;
            // The pool at its end.
            for (y = by - 1; y <= by + 2; y++)
                for (x = bx - 2; x <= bx + 3; x++)
                    if (Cell(gen, x, y) == C_TREE && !IsEdge(gen, x, y) && !NearExitU(gen, x, y, 2)
                     && !((x == bx - 2 || x == bx + 3) && (y == by - 1 || y == by + 2)))
                    {
                        SetCell(gen, x, y, C_SEA);
                        SetBitU(gen->river, gen, x, y);
                    }
        }
        for (i = 0; i < gen->cw * gen->ch; i++)
        {
            YIELD();
            n += gen->g[i] == C_SEA;
        }
        if (ok && n >= 10 && RiverGap(gen, wx0, wy0, wx1, wy1) <= (pool ? 2 : 1))
            return TRUE;
        ClearRiver(gen);
    }
    return FALSE;
}

// The side of a straight stretch of trail at step i (with the steps before
// and after it) that can open out one more unit, into trees that go on
// beyond, or RDIR_NONE.
static u8 WideSide(struct Gen *gen, int i)
{
    u8 d = PathDir(gen, i), k;
    if (i < 1 || i + 1 >= gen->nPath || PathDir(gen, i - 1) != d)
        return RDIR_NONE;
    for (k = 0; k < 2; k++)
    {
        u8 e = (d + 1 + 2 * ((k + i) & 1)) & 3;
        int a;
        bool8 ok = TRUE;
        for (a = i - 1; a <= i + 1 && ok; a++)
        {
            int x = gen->path[a] % gen->cw + sDirDX[e], y = gen->path[a] / gen->cw + sDirDY[e];
            if (Cell(gen, x, y) != C_TREE || IsEdge(gen, x, y) || BitU(gen->keep, gen, x, y)
             || Cell(gen, x + sDirDX[e], y + sDirDY[e]) != C_TREE || NearExitU(gen, x, y, 2))
                ok = FALSE;
        }
        if (ok)
            return e;
    }
    return RDIR_NONE;
}

// Where the trail squeezes between trees: evenly along it. Trainers wait at
// these, so where it can the squeeze is two units across for three units
// along, and a trainer by one wall looks three blocks over to the other, as
// at most of Emerald's; failing that it is the trail alone, at a bend where
// there is one nearby. Returns how many.
static int PickSqueezes(struct Gen *gen, int count, int first, u16 *out)
{
    int j, n = 0, span = gen->nPath - 3 - first;
    if (span < 4)
        return 0;
    if (count > MAX_SQUEEZE)
        count = MAX_SQUEEZE;
    for (j = 0; j < count; j++)
    {
        int at = first + span * (j + 1) / (count + 1), best = at, k, a;
        u8 side = RDIR_NONE;
        // Nearest the even spot first.
        for (k = 0; k <= 6 && side == RDIR_NONE; k++)
        {
            int i = at + ((k & 1) ? -(k + 1) / 2 : k / 2);
            if (i <= first || i >= gen->nPath - 3 || (n && i <= out[n - 1] + 2))
                continue;
            side = WideSide(gen, i);
            if (side != RDIR_NONE)
                best = i;
        }
        for (k = -3; k <= 3 && side == RDIR_NONE; k++)
        {
            int i = at + k;
            if (i <= first || i >= gen->nPath - 3 || (n && i <= out[n - 1] + 2))
                continue;
            if (PathDir(gen, i - 1) != PathDir(gen, i))
            {
                best = i;
                break;
            }
        }
        if (n && best <= out[n - 1] + 2)
            continue;
        out[n++] = best;
        if (side != RDIR_NONE)
        {
            for (a = best - 1; a <= best + 1; a++)
                OpenUnit(gen, gen->path[a] % gen->cw + sDirDX[side], gen->path[a] / gen->cw + sDirDY[side]);
            for (a = best - 1; a <= best + 1; a++)
                KeepAround(gen, gen->path[a] % gen->cw + sDirDX[side], gen->path[a] / gen->cw + sDirDY[side]);
        }
        KeepAround(gen, gen->path[best] % gen->cw, gen->path[best] / gen->cw);
    }
    return n;
}

static int Jitter(struct Gen *gen, int x, int y)
{
    return (int)(RogueRng_Hash(gen->rng ^ 0x6A09, (y >> 1) * 64 + (x >> 1)) % 3) - 1;
}

// Meadows: open ground grows out from the trail, a different width along
// each stretch between squeezes. Ground near two stretches stays forest, so
// tree lines divide the meadows and the only way on is the trail's squeeze.
// Returns the share of the map left as trees, in percent.
static int GrowMeadows(struct Gen *gen, const u16 *squeeze, int nSqueeze, const u8 *radius, int extra)
{
    s16 *lab = sBfsPrev;
    u8 *dist = gen->cover; // unused until the ground cover is painted
    int n = gen->cw * gen->ch, i, j, head = 0, tail = 0, trees = 0;

    for (i = 0; i < n; i++)
    {
        YIELD();
        lab[i] = -1;
        dist[i] = 0xFF;
    }
    for (i = 0, j = 0; i < gen->nPath; i++)
    {
        YIELD();
        int u = gen->path[i];
        while (j < nSqueeze && squeeze[j] <= i)
            j++;
        if (lab[u] >= 0)
            continue;
        lab[u] = j;
        dist[u] = 0;
        sBfsQueue[tail++] = PACK(u % gen->cw, u / gen->cw);
    }
    while (head < tail)
    {
        int c = sBfsQueue[head++];
        int cx = PX(c), cy = PY(c), ci = cy * gen->cw + cx;
        u8 d;
        YIELD();
        for (d = 0; d < 4; d++)
        {
            int nx = cx + sDirDX[d], ny = cy + sDirDY[d], ni, dx, dy, r;
            bool8 clash = FALSE;
            u8 t;
            if (!InBounds(gen, nx, ny) || IsEdge(gen, nx, ny))
                continue;
            ni = ny * gen->cw + nx;
            t = gen->g[ni];
            if (lab[ni] >= 0 || (t != C_TREE && t != C_OPEN) || BitU(gen->keep, gen, nx, ny))
                continue;
            r = radius[lab[ci]] + extra + Jitter(gen, nx, ny);
            if (dist[ci] + 1 > r)
                continue;
            // A ragged band of forest along the edges of the map.
            if ((nx == 1 || ny == 1 || nx == gen->cw - 2 || ny == gen->ch - 2) && !NearExitU(gen, nx, ny, 3)
             && RogueRng_Hash(gen->rng ^ 0xB0DE, ni) % 100 >= 40)
                continue;
            for (dy = -1; dy <= 1 && !clash; dy++)
                for (dx = -1; dx <= 1; dx++)
                    if (InBounds(gen, nx + dx, ny + dy) && lab[(ny + dy) * gen->cw + nx + dx] >= 0
                     && lab[(ny + dy) * gen->cw + nx + dx] != lab[ci])
                        clash = TRUE;
            if (clash)
                continue;
            lab[ni] = lab[ci];
            dist[ni] = dist[ci] + 1;
            if (t == C_TREE)
                gen->g[ni] = C_OPEN;
            if (tail < BFS_RING)
                sBfsQueue[tail++] = PACK(nx, ny);
        }
    }
    memset(dist, 0, n);
    for (i = 0; i < n; i++)
    {
        YIELD();
        trees += gen->g[i] == C_TREE;
    }
    return trees * 100 / n;
}

// A stand of trees out in the open: a single tree or a clump of up to 3x2
// units, with open ground all round so it never closes a way. Next to the
// trail it makes the way split round it and join again.
static bool8 PlaceClump(struct Gen *gen, bool8 byTrail)
{
    int tries;
    for (tries = 0; tries < 40; tries++)
    {
        int w = 1 + RAND(3), h = 1 + RAND(2), x0, y0, x, y;
        bool8 ok = TRUE;
        if (byTrail)
        {
            // One or two units off a stretch of trail.
            int i = 3 + RAND(gen->nPath > 6 ? gen->nPath - 6 : 1), u = gen->path[i], k = 1 + RAND(2);
            int ux = u % gen->cw, uy = u / gen->cw;
            u8 d = (PathDir(gen, i) + (RAND(2) ? 1 : 3)) & 3;
            switch (d)
            {
            case RDIR_EAST:  x0 = ux + k;           y0 = uy - RAND(h); break;
            case RDIR_WEST:  x0 = ux - k - (w - 1); y0 = uy - RAND(h); break;
            case RDIR_SOUTH: y0 = uy + k;           x0 = ux - RAND(w); break;
            default:         y0 = uy - k - (h - 1); x0 = ux - RAND(w); break;
            }
        }
        else
        {
            x0 = 2 + RAND(gen->cw - 3 - w);
            y0 = 2 + RAND(gen->ch - 3 - h);
        }
        for (y = y0 - 1; y <= y0 + h && ok; y++)
        {
            for (x = x0 - 1; x <= x0 + w && ok; x++)
            {
                u8 c = Cell(gen, x, y);
                bool8 inside = x >= x0 && x < x0 + w && y >= y0 && y < y0 + h;
                if (!InBounds(gen, x, y) || IsEdge(gen, x, y))
                    ok = FALSE;
                else if (inside ? c != C_OPEN || NearExitU(gen, x, y, 3) : (c != C_OPEN && c != C_ROAD))
                    ok = FALSE;
            }
        }
        if (!ok)
            continue;
        for (y = y0; y < y0 + h; y++)
            for (x = x0; x < x0 + w; x++)
                SetCell(gen, x, y, C_TREE);
        return TRUE;
    }
    return FALSE;
}

// Ledge bands across open ground, three to six units long, with open ground
// above and below. One end may meet trees or water, the other the open, so
// the band is a one-way shortcut and the way round is never far. A band is
// only kept if everything stays reachable without jumping it.
static void PlaceLedgeBands(struct Gen *gen, int startX, int startY, int count)
{
    int tries, placed = 0;
    for (tries = 0; tries < 160 && placed < count; tries++)
    {
        int y = 3 + RAND(gen->ch - 6), x = 2 + RAND(gen->cw - 4);
        int x0 = x, x1 = x, cx, before, len;
        bool8 ok = TRUE;
        YIELD();
        if (Cell(gen, x, y) != C_OPEN || AshZoneU(gen, x, y))
            continue;
        while (Cell(gen, x0 - 1, y) == C_OPEN && x - x0 < 8)
            x0--;
        while (Cell(gen, x1 + 1, y) == C_OPEN && x1 - x < 8)
            x1++;
        if (x1 - x0 + 1 < 3)
            continue;
        len = 3 + RAND(4);
        if (x1 - x0 + 1 > len)
        {
            // Against one end of the run, so it meets the trees there.
            if (RAND(2))
                x1 = x0 + len - 1;
            else
                x0 = x1 - len + 1;
        }
        for (cx = x0; cx <= x1 && ok; cx++)
        {
            u8 above = Cell(gen, cx, y - 1), below = Cell(gen, cx, y + 1);
            if ((above != C_OPEN && above != C_ROAD) || (below != C_OPEN && below != C_ROAD) || AshZoneU(gen, cx, y)
             || NearExitU(gen, cx, y, 2) || HasNeighbor(gen, cx, y, C_SEA, TRUE) || HasNeighbor(gen, cx, y, C_WATER, TRUE))
                ok = FALSE;
        }
        if (!ok)
            continue;
        before = FloodReachable(gen, startX, startY);
        for (cx = x0; cx <= x1; cx++)
            SetCell(gen, cx, y, C_LEDGE);
        if (FloodReachable(gen, startX, startY) != before)
        {
            for (cx = x0; cx <= x1; cx++)
                SetCell(gen, cx, y, C_OPEN);
            continue;
        }
        placed++;
    }
}

// A pond with a rounded shore in a meadow, that cuts nothing off.
static bool8 PlacePond(struct Gen *gen, int startX, int startY, int top)
{
    int tries, i, x, y, before = -1;
    for (tries = 0; tries < 30; tries++)
    {
        int cx = 3 + RAND(gen->cw - 6), cy = top + 2 + RAND(gen->ch - top - 5), area, n = 0;
        int rx = 2 + RAND(2), ry = 2 + RAND(2);
        // The box round the pond that the passes below touch.
        int x0 = cx - rx - 2 > 0 ? cx - rx - 2 : 0, x1 = cx + rx + 2 < gen->cw - 1 ? cx + rx + 2 : gen->cw - 1;
        int y0 = cy - ry - 2 > 0 ? cy - ry - 2 : 0, y1 = cy + ry + 2 < gen->ch - 1 ? cy + ry + 2 : gen->ch - 1;
        u8 saved[MAX_CELLS / 8 + 1];
        if (Cell(gen, cx, cy) != C_OPEN && Cell(gen, cx, cy) != C_TREE)
            continue;
        // Beside open ground, so it can be fished from.
        if (!HasNeighbor(gen, cx + rx + 1, cy, C_OPEN, TRUE) && !HasNeighbor(gen, cx - rx - 1, cy, C_OPEN, TRUE)
         && !HasNeighbor(gen, cx, cy + ry + 1, C_OPEN, TRUE))
            continue;
        // The same for every try until one leaves water behind (most leave
        // the map as it was).
        if (before < 0)
            before = FloodReachable(gen, startX, startY);
        // Ponds may take meadow as well as forest.
        memset(saved, 0, sizeof(saved));
        for (y = y0; y <= y1; y++)
        {
            YIELD();
            for (x = x0; x <= x1; x++)
            {
                i = y * gen->cw + x;
                if (gen->g[i] == C_OPEN)
                {
                    saved[i >> 3] |= 1 << (i & 7);
                    gen->g[i] = C_TREE;
                }
            }
        }
        area = PlaceBlobWater(gen, cx, cy, rx, ry, 1 + RAND(2), top + 1);
        for (y = y0; y <= y1; y++)
        {
            YIELD();
            for (x = x0; x <= x1; x++)
            {
                i = y * gen->cw + x;
                if (((saved[i >> 3] >> (i & 7)) & 1) && gen->g[i] == C_TREE)
                    gen->g[i] = C_OPEN;
            }
        }
        if (!area)
            continue;
        for (y = y0; y <= y1; y++)
            for (x = x0; x <= x1; x++)
            {
                i = y * gen->cw + x;
                if (((saved[i >> 3] >> (i & 7)) & 1) && gen->g[i] == C_SEA)
                    n++;
            }
        if (FloodReachable(gen, startX, startY) == before - n && area >= 6)
            return TRUE;
        for (y = y0; y <= y1; y++)
        {
            YIELD();
            for (x = x0; x <= x1; x++)
            {
                i = y * gen->cw + x;
                // (Not water laid before: a river's, or a bridged pond's.)
                if (gen->g[i] == C_SEA && !BitU(gen->river, gen, x, y) && !BitU(gen->bridgeU, gen, x, y))
                    gen->g[i] = ((saved[i >> 3] >> (i & 7)) & 1) ? C_OPEN : C_TREE;
            }
        }
        // Water outside the box stays.
        before = -1;
    }
    return FALSE;
}

// How many units of sea a coast with its sea along side sea has at its
// seam on side d, where the place beyond is another coast with its sea on
// the same side, or a sea route; 0 where the shore must curve in to land
// at the seam instead (a town, any other place, or two coasts whose
// openings lie too far apart for their seas to meet). Both sides work it
// out the same way, counted from their openings on that side, which the
// connection lines up, so the water runs on across the seam.
#define SEAM_SEA_MAX (SEA_DEPTH + 4)
static int CoastSeamDepth(struct Gen *gen, u8 d, u8 sea)
{
    const struct RoguePlace *p = gen->p;
    const struct RogueLink *l = LinkOnSide(p, d), *back;
    const struct RoguePlace *t;
    int len, dA, dB, lo, hi, gap, me = (int)(p - gen->r->places);
    bool8 fromStart = (sea == RDIR_NORTH || sea == RDIR_WEST);
    if (l == NULL)
        return -1; // only the border, which is sea: the band runs on
    t = &gen->r->places[l->target];
    back = RogueGen_FindLink(t, me);
    if (back == NULL || t->kind != RPLACE_ROUTE)
        return 0;
    len = EdgeLength(gen, d);
    dA = fromStart ? l->pos / 2 : len - 1 - l->pos / 2;
    if (t->type == RROUTE_SEA)
        return dA - 3; // the sea route's shore starts three units short of the opening
    if (t->type != RROUTE_COAST || t->seaSide != sea)
        return 0;
    {
        int lenT = (d == RDIR_NORTH || d == RDIR_SOUTH) ? t->w / 2 : t->h / 2;
        dB = fromStart ? back->pos / 2 : lenT - 1 - back->pos / 2;
    }
    // The gap between the water and the opening, the same on both sides.
    lo = (dA > dB ? dA : dB) - SEAM_SEA_MAX;
    hi = (dA < dB ? dA : dB) - 2;
    if (lo < 3)
        lo = 3;
    if (hi < lo)
        return 0;
    gap = lo + (int)(RogueRng_Hash(gen->r->seed, 7300 + (me < l->target ? me * 64 + l->target : l->target * 64 + me)) % (u32)(hi - lo + 1));
    return dA - gap;
}

// Coast: the sea along one side with a wandering shore. Where it reaches a
// neighbour with no sea of its own, the shore curves in to meet the land, so
// the water never stops in a straight line at the seam; where the
// neighbour's sea runs on, the shore swings to the depth both sides agree
// on. Then a beach along the water, two units deep, now and then one or
// three, and here and there the trees come right down to it.
static void ShapeShore(struct Gen *gen)
{
    u8 d = gen->p->seaSide, ends[2];
    int len = EdgeLength(gen, d), pos, k, depth = 2, e;

    ShapeCoastBand(gen, 2, SEA_DEPTH, FALSE);
    ends[0] = (d == RDIR_NORTH || d == RDIR_SOUTH) ? RDIR_WEST : RDIR_NORTH;
    ends[1] = Opposite(ends[0]);
    for (e = 0; e < 2; e++)
    {
        int seam = CoastSeamDepth(gen, ends[e], d);
        if (seam < 0)
            continue;
        if (seam > EdgeLength(gen, ends[e]) - 4)
            seam = EdgeLength(gen, ends[e]) - 4;
        for (k = 0; k < 6 && k < len; k++)
        {
            int at = e ? len - 1 - k : k, j;
            int want = seam > 0 ? seam + (gen->seaDepth[at] - seam) * k / 6 : (k < gen->seaDepth[at] ? k : gen->seaDepth[at]);
            for (j = 0; j < SEAM_SEA_MAX + 2; j++)
            {
                int x, y;
                EdgeCell(gen, d, at, j, &x, &y);
                if (j < want)
                    SetCell(gen, x, y, C_SEA);
                else if (j < gen->seaDepth[at] || Cell(gen, x, y) == C_SEA)
                    SetCell(gen, x, y, C_TREE);
            }
            gen->seaDepth[at] = want;
        }
    }
    for (pos = 1; pos < len - 1; pos++)
    {
        int x, y;
        // A wide beach all along, as Route 109's and Route 104's south
        // (v1.1; before, the trees often came down to the water).
        if (pos % 3 == 1)
        {
            u32 roll = RAND(100);
            depth = roll < 4 ? 1 : roll < 25 ? 2 : roll < 75 ? 3 : 4;
        }
        if (gen->seaDepth[pos] == 0)
            continue;
        for (k = 0; k < depth; k++)
        {
            EdgeCell(gen, d, pos, gen->seaDepth[pos] + k, &x, &y);
            if (Cell(gen, x, y) == C_TREE && !IsEdge(gen, x, y))
                SetCell(gen, x, y, C_OPEN);
            if (Cell(gen, x, y) == C_OPEN)
                SetBitU(gen->beach, gen, x, y);
        }
    }
}

// The beach unit at a spot along the shore, a step inland from the water.
static void ShoreUnit(struct Gen *gen, int pos, int *x, int *y)
{
    u8 d = gen->p->seaSide;
    int len = EdgeLength(gen, d);
    if (pos < 2) pos = 2;
    if (pos > len - 3) pos = len - 3;
    EdgeCell(gen, d, pos, gen->seaDepth[pos], x, y);
}

// ---------------------------------------------------------------------------
// The sea route
//
// Open water to Surf across, with islands. A side with nothing beyond it on
// the region grid is open sea running off the map. A side with a place
// beyond it is a shore of beach backed by trees, so the water never stops in
// a straight line at a seam; where the place beyond has its own sea along
// the same coast, the water runs on across that part of the seam.

// TRUE if nothing lies beyond side d of place p on the region grid.
static bool8 OpenSeaSide(const struct RogueRegion *r, const struct RoguePlace *p, u8 d)
{
    int i;
    if (LinkOnSide(p, d) != NULL)
        return FALSE;
    for (i = 0; i < r->nPlaces; i++)
    {
        YIELD();
        if (r->places[i].gridX == p->gridX + sDirDX[d] && r->places[i].gridY == p->gridY + sDirDY[d])
            return FALSE;
    }
    return TRUE;
}

static int PlaceEdgeLength(const struct RoguePlace *p, u8 d)
{
    return (d == RDIR_NORTH || d == RDIR_SOUTH) ? p->w / 2 : p->h / 2;
}

// How many units of land reach in from side d of sea route p at pos. It
// depends only on the plan, so a neighbouring sea route can work it out
// too. Along a neighbour the shore runs the whole length they share, so
// land always meets land across the seam: a town or a land route has trees
// all along its edge. A coast or a harbor town with its sea on a side
// across this one has open water up to three units short of its opening, as
// this side does (see CoastSeamDepth and GenTown). Another sea route has open water all along, but for a
// line of trees where its own shores come down to the seam (mirror).
// How far SeaLandDepth looks round corners and across seams.
#define SEA_SHORE_LEVELS 3
static int SeaLandDepth(const struct RogueRegion *r, const struct RoguePlace *p, u8 d, int pos, u8 level)
{
    const struct RogueLink *l = LinkOnSide(p, d);
    int len = PlaceEdgeLength(p, d), depth, me = (int)(p - r->places);
    u8 e0 = (d == RDIR_NORTH || d == RDIR_SOUTH) ? RDIR_WEST : RDIR_NORTH;
    bool8 shared = FALSE;
    if (OpenSeaSide(r, p, d))
        return 0;
    // Ragged: the trees on the edge and up to two units of beach,
    // changing every few units.
    depth = 1 + (int)(RogueRng_Hash(r->seed ^ (0x5EA0 + d), me * 64 + pos / 3) % 3);
    if (l != NULL)
    {
        const struct RoguePlace *t = &r->places[l->target];
        int split, exit = l->pos / 2, there = pos - l->offset / 2;
        int lenT = PlaceEdgeLength(t, d);
        u8 s = t->seaSide;
        shared = there >= 0 && there < lenT;
        // (v1.1 towns2) A raft village's water meets the sea route's as
        // another sea route's does.
        if ((t->kind == RPLACE_ROUTE && t->type == RROUTE_SEA) || (t->kind == RPLACE_TOWN && t->theme == RTHEME_RAFT))
        {
            u8 f0 = e0, f1 = Opposite(e0), back = Opposite(d);
            int along;
            if (level == 0 || !shared)
                return 0;
            // The neighbour's own shores on its sides across this seam reach
            // its edge here: meet them with a line of trees.
            along = (back == RDIR_WEST || back == RDIR_NORTH) ? 0 : PlaceEdgeLength(t, f0) - 1;
            if (SeaLandDepth(r, t, f0, along, level) > there || SeaLandDepth(r, t, f1, along, level) > lenT - 1 - there)
                return 1;
            return 0;
        }
        if (((t->kind == RPLACE_ROUTE && t->type == RROUTE_COAST) || (t->kind == RPLACE_TOWN && t->layout == RLAYOUT_HARBOR))
         && s != RDIR_NONE && s != d && s != Opposite(d))
        {
            // The neighbour's sea lies toward s along this edge.
            split = (s == RDIR_WEST || s == RDIR_NORTH) ? exit - 3 : exit + 3;
            if (split < 0) split = 0;
            if (split > len - 1) split = len - 1;
            if ((s == RDIR_WEST || s == RDIR_NORTH) ? pos < split : pos > split)
                return 0;
            if (Abs(pos - split) < 2)
                depth = 2;
        }
    }
    // Toward a corner where the side across has open water, the shore
    // runs out into it, past the end of the neighbour.
    if (!shared)
    {
        int corner = (d == RDIR_NORTH || d == RDIR_WEST) ? 0 : PlaceEdgeLength(p, e0) - 1;
        bool8 sea0 = level ? SeaLandDepth(r, p, e0, corner, level - 1) == 0 : OpenSeaSide(r, p, e0);
        bool8 sea1 = level ? SeaLandDepth(r, p, Opposite(e0), corner, level - 1) == 0 : OpenSeaSide(r, p, Opposite(e0));
        if (sea0 && depth > pos)
            depth = pos;
        if (sea1 && depth > len - 1 - pos)
            depth = len - 1 - pos;
    }
    return depth;
}

// An island: overlapping blobs of sand.
static void SeaIsland(struct Gen *gen, int cx, int cy, int rx, int ry)
{
    u8 *mark = gen->cover; // the cover is painted after the islands
    int k, x, y, n = gen->cw * gen->ch;
    memset(mark, 0, n);
    for (k = 0; k < 2; k++)
    {
        int ox = k ? (int)RAND(3) - 1 : 0, oy = k ? (int)RAND(3) - 1 : 0;
        int lrx = k ? 1 + RAND(rx + 1) : rx, lry = k ? RAND(ry + 1) : ry;
        for (y = cy + oy - lry; y <= cy + oy + lry; y++)
            for (x = cx + ox - lrx; x <= cx + ox + lrx; x++)
                if (InBounds(gen, x, y) && InEllipse(x - cx - ox, y - cy - oy, lrx, lry))
                    mark[y * gen->cw + x] = 1;
    }
    if (rx >= 2 && ry >= 1)
    {
        RoughenMarks(gen, mark);
        SmoothMarks(gen, mark);
    }
    for (y = 2; y < gen->ch - 2; y++)
    {
        YIELD();
        for (x = 2; x < gen->cw - 2; x++)
            if (mark[y * gen->cw + x])
                SetCell(gen, x, y, C_OPEN);
    }
    memset(mark, 0, n);
}

// TRUE if every unit of the mesa is still rock.
static bool8 MesaWhole(struct Gen *gen)
{
    int x, y;
    for (y = gen->mesa.y; y < gen->mesa.y + gen->mesa.h; y++)
        for (x = gen->mesa.x; x < gen->mesa.x + gen->mesa.w; x++)
            if (Cell(gen, x, y) != C_ROCK)
                return FALSE;
    return TRUE;
}

// TRUE for a unit of a way into the woods already placed: its gap, the
// trees each side and behind, and the ground in front.
static bool8 ForestWayUnit(struct Gen *gen, int x, int y)
{
    int k;
    for (k = 0; k < gen->nForestWays; k++)
    {
        int fx = gen->forestX[k], fy = gen->forestY[k], dy = gen->forestFace[k] == RDIR_SOUTH ? 1 : -1;
        if ((y == fy && Abs(x - fx) <= 1) || (x == fx && (y == fy - dy || y == fy + dy)))
            return TRUE;
    }
    return FALSE;
}

// A rest house beside the road: a house of the route's theme, its door
// opening onto the road, standing where the woods were.
static bool8 PlaceRestHouse(struct Gen *gen, struct Placed *out)
{
    const struct RoguePlace *p = gen->p;
    const struct TownTheme *th = &sThemes[p->theme];
    // The house of the town the route leads to, on green routes. Houses made
    // for sand or ash, treehouses away from the forest, and any house in the
    // desert or on the coast are the plain primary house instead, the one
    // meadow house that looks right with any tileset.
    bool8 green = p->type == RROUTE_FOREST || p->type == RROUTE_WOODS || p->type == RROUTE_FIELD
               || p->type == RROUTE_LAKE || p->type == RROUTE_RIVER
               || p->type == RROUTE_FLOWERS || p->type == RROUTE_LONGGRASS;
    bool8 themed = green && th->ground == TGROUND_GRASS
                && (p->theme != RTHEME_TREETOP || p->type == RROUTE_FOREST || p->type == RROUTE_WOODS);
    const struct Building *b = themed ? &th->houses[RAND(th->nHouses)] : &sHouses[2];
    int tries;
    if (gen->nPath < 12)
        return FALSE;
    for (tries = 0; tries < 60; tries++)
    {
        int pi = gen->nPath / 4 + RAND(gen->nPath / 2);
        int px = gen->path[pi] % gen->cw, py = gen->path[pi] / gen->cw;
        int bx = px - b->doorX / 2, by = py - b->uh, x, y;
        bool8 ok = TRUE;
        YIELD();
        if (bx < 1 || by < 1 || bx + b->uw > gen->cw - 1 || InGateLaneF(gen, px * 2, py * 2))
            continue;
        for (y = by; y < by + b->uh && ok; y++)
        {
            for (x = bx; x < bx + b->uw && ok; x++)
            {
                u8 c = Cell(gen, x, y);
                if ((c != C_TREE && c != C_OPEN) || IsEdge(gen, x, y))
                    ok = FALSE;
                // Nor over a way into the woods, its trees or its ground
                // (ForestWaysClear).
                if (ForestWayUnit(gen, x, y))
                    ok = FALSE;
            }
        }
        // Ground or trees round it, never water or a rock face.
        if (!ok || !LotRingClear(gen, b, bx, by))
            continue;
        // v1.1: a treehouse stands in the crowns of deep woods, and its
        // bottom row is their crowns: no ordinary tree below it, whose top
        // has no piece to meet them (seed 39's route 17 had one).
        for (x = 0; x < b->sw && ok; x++)
        {
            u16 t = b->stamp[(b->sh - 1) * b->sw + x] & 0x3FF;
            if ((t == 0x0C6 || t == 0x0C7) && Cell(gen, bx + x / 2, by + b->uh) == C_TREE)
                ok = FALSE;
        }
        if (!ok)
            continue;
        // Theme tiles must stay out of view of a neighbour with another tileset.
        if (!StampIsPrimary(b->stamp, b->sw * b->sh)
         && (InRouteSeam(gen, bx * 2, by * 2) || InRouteSeam(gen, (bx + b->uw) * 2 - 1, (by + b->uh) * 2 - 1)))
            continue;
        for (y = by; y < by + b->uh; y++)
            for (x = bx; x < bx + b->uw; x++)
                SetCell(gen, x, y, C_BLDG);
        // Nobody may stand in front of the door.
        MarkOccupied(gen, bx * 2 + b->doorX, by * 2 + b->sy + FrontY(b));
        out->b = b;
        out->bx = bx;
        out->by = by;
        out->door = RWARP_HOUSE;
        return TRUE;
    }
    return FALSE;
}

// The unit halfway along the shortest walk on land from one unit to
// another, or -1 if there is none.
static int LandWalkMiddle(struct Gen *gen, int x0, int y0, int x1, int y1)
{
    int head = 0, tail = 0, n = gen->cw * gen->ch, goal = y1 * gen->cw + x1, i, len;
    for (i = 0; i < n; i++)
        sBfsPrev[i] = -2;
    sBfsPrev[y0 * gen->cw + x0] = -1;
    sBfsQueue[tail++] = y0 * gen->cw + x0;
    while (head < tail && sBfsPrev[goal] == -2)
    {
        int cur = sBfsQueue[head++], cx = cur % gen->cw, cy = cur / gen->cw;
        u8 d;
        for (d = 0; d < 4; d++)
        {
            int nx = cx + sDirDX[d], ny = cy + sDirDY[d], ni = ny * gen->cw + nx;
            if (!InBounds(gen, nx, ny) || sBfsPrev[ni] != -2 || !IsOpenU(Cell(gen, nx, ny)) || tail >= BFS_RING)
                continue;
            sBfsPrev[ni] = cur;
            sBfsQueue[tail++] = ni;
        }
    }
    if (sBfsPrev[goal] == -2)
        return -1;
    for (len = 0, i = goal; sBfsPrev[i] >= 0; i = sBfsPrev[i])
        len++;
    for (len /= 2, i = goal; len > 0; i = sBfsPrev[i])
        len--;
    return i;
}

static bool8 PlaceShipwreck(struct Gen *gen, int ax, int ay, int bx, int by);

// (v1.1 towns2) TRUE if side d of the map leads to a raft village.
static bool8 SeaToRaft(struct Gen *gen, u8 d)
{
    const struct RogueLink *l = LinkOnSide(gen->p, d);
    return l != NULL && gen->r->places[l->target].kind == RPLACE_TOWN && gen->r->places[l->target].theme == RTHEME_RAFT;
}
static bool8 SeaGenKind(struct Gen *gen, u8 kind, u8 inDir, u8 outDir);
static bool8 SeaSwimSpot(struct Gen *gen, int fx, int fy, u8 *facing);
#define SEA_KIND_TRIES 50

static bool8 GenSeaRoute(struct Gen *gen, u8 inDir, u8 outDir)
{
    const struct RoguePlace *p = gen->p;
    int ax, ay, bx, by, i, n, k, placedT = 0, placedI = 0, tries;
    int ix[8], iy[8];
    u8 d, face;

    // v1.1 sea: seas of other kinds (src/rogue_gen_sea.h).
    {
        u8 kind = RogueGen_SeaKind(gen->r, gen->m->place);
        if (kind != SEA_OPEN && gen->attempt < SEA_KIND_TRIES)
            return SeaGenKind(gen, kind, inDir, outDir);
    }
    gen->groundKind = GROUND_SAND;
    for (i = 0; i < gen->cw * gen->ch; i++)
        gen->g[i] = C_SEA;
    // Shores along the sides with land beyond: trees on the edge, beach
    // in front of them.
    for (d = 0; d < 4; d++)
    {
        int len = EdgeLength(gen, d), pos;
        for (pos = 0; pos < len; pos++)
        {
            YIELD();
            int depth = SeaLandDepth(gen->r, gen->p, d, pos, SEA_SHORE_LEVELS), x, y;
            for (k = 0; k < depth; k++)
            {
                EdgeCell(gen, d, pos, k, &x, &y);
                SetCell(gen, x, y, k == 0 ? C_TREE : (Cell(gen, x, y) == C_TREE ? C_TREE : C_OPEN));
            }
        }
    }
    // Landings at both ends: the opening and beach around it.
    MakeOpening(gen, inDir, &ax, &ay);
    MakeOpening(gen, outDir, &bx, &by);
    for (k = 0; k < 2; k++)
    {
        int lx = k ? bx : ax, ly = k ? by : ay, x, y;
        d = k ? outDir : inDir;
        // (v1.1 towns2) Toward a raft village the water runs on across the
        // seam: no landing, as Routes 131 and 132 reach Pacifidlog.
        if (SeaToRaft(gen, d))
        {
            EdgeCell(gen, d, gen->exitPos[d] / 2, 0, &x, &y);
            SetCell(gen, x, y, C_SEA);
            SetCell(gen, lx, ly, C_SEA);
            continue;
        }
        for (y = ly - 2; y <= ly + 2; y++)
            for (x = lx - 2; x <= lx + 2; x++)
                if (InBounds(gen, x, y) && !IsEdge(gen, x, y) && Abs(x - lx) + Abs(y - ly) <= 3)
                    SetCell(gen, x, y, C_OPEN);
        EdgeCell(gen, d, gen->exitPos[d] / 2, 0, &x, &y);
        SetCell(gen, x, y, C_ROAD);
        SetCell(gen, lx, ly, C_ROAD);
    }
    // Islands along the way, a surf apart, and now and then a long sandbar.
    n = 3 + RAND(2);
    for (i = 0; i < n; i++)
    {
        int t = (i + 1) * 100 / (n + 1);
        int cx = ax + (bx - ax) * t / 100 + (int)RAND(7) - 3;
        int cy = ay + (by - ay) * t / 100 + (int)RAND(7) - 3;
        if (cx < 3) cx = 3;
        if (cy < 3) cy = 3;
        if (cx > gen->cw - 4) cx = gen->cw - 4;
        if (cy > gen->ch - 4) cy = gen->ch - 4;
        ix[i] = cx;
        iy[i] = cy;
        if (i == n / 2 && CHANCE(50))
        {
            // A sandbar across the way.
            bool8 across = Abs(bx - ax) < Abs(by - ay);
            SeaIsland(gen, cx, cy, across ? 3 : 0, across ? 0 : 3);
        }
        else
        {
            SeaIsland(gen, cx, cy, 2 + RAND(2), 1 + RAND(2));
        }
    }
    // A tree or two on the bigger islands.
    for (i = 0; i < n; i++)
    {
        int x = ix[i] + (int)RAND(3) - 1, y = iy[i] - 1;
        // Not by a landing, where a tree could shut the way in.
        if (Abs(x - ax) <= 3 && Abs(y - ay) <= 3)
            continue;
        if (Abs(x - bx) <= 3 && Abs(y - by) <= 3)
            continue;
        if (Cell(gen, x, y) == C_OPEN && Cell(gen, x, y - 1) == C_OPEN && Cell(gen, x, y + 1) == C_OPEN && CHANCE(50))
            SetCell(gen, x, y, C_TREE);
    }
    // Fill in inlets one unit across, which would read as puddles. Then,
    // as on Routes 105 to 109, the way across needs Surf: where the beaches
    // and islands join up into a walk from one landing to the other, the
    // sea breaks through it halfway, and again until no walk is left.
    for (tries = 0; tries < 8; tries++)
    {
        int x, y, at;
        for (k = 0; k < 2; k++)
        {
            for (y = 1; y < gen->ch - 1; y++)
            {
                for (x = 1; x < gen->cw - 1; x++)
                {
                    int land = 0;
                    u8 dd;
                    if (Cell(gen, x, y) != C_SEA)
                        continue;
                    for (dd = 0; dd < 4; dd++)
                        land += Cell(gen, x + sDirDX[dd], y + sDirDY[dd]) != C_SEA;
                    if (land >= 3)
                        SetCell(gen, x, y, C_OPEN);
                }
            }
        }
        at = LandWalkMiddle(gen, ax, ay, bx, by);
        if (at < 0)
            break;
        for (y = at / gen->cw - 1; y <= at / gen->cw + 1; y++)
            for (x = at % gen->cw - 1; x <= at % gen->cw + 1; x++)
                if (InBounds(gen, x, y) && !IsEdge(gen, x, y) && IsOpenU(Cell(gen, x, y))
                 && Abs(x - ax) + Abs(y - ay) > 3 && Abs(x - bx) + Abs(y - by) > 3)
                    SetCell(gen, x, y, C_SEA);
    }
    if (tries == 8)
        return FALSE;
    PlaceSeaRocks(gen, 4 + RAND(4));
    // A shipwreck on its shoal, out on the water (src/rogue_gen_ship.h).
    if (!PlaceShipwreck(gen, ax, ay, bx, by))
        return FALSE;
    // Ground: sand everywhere on land, tall grass in patches on islands.
    for (i = 0; i < gen->cw * gen->ch; i++)
        if (gen->g[i] == C_OPEN || gen->g[i] == C_ROAD)
            PaintUnit(gen, i % gen->cw, i / gen->cw, V_GROUND);
    for (i = 0; i < n; i++)
        if (CHANCE(60))
            PaintBlob(gen, ix[i] * 2 + 1, iy[i] * 2 + 1, 2 + RAND(2), V_TALL, TRUE);
    PaintSpecialGround(gen);

    // Trainers on the islands, looking out across the water the player
    // surfs by; items on the far ones; a sign at the first landing.
    face = Abs(bx - ax) >= Abs(by - ay) ? RDIR_NORTH : RDIR_WEST;
    for (i = 0; i < n && placedT < p->nTrainers; i++)
    {
        for (tries = 0; tries < 30; tries++)
        {
            int fx = ix[i] * 2 + (int)RAND(4) - 1, fy = iy[i] * 2 + (int)RAND(4) - 1;
            u8 f = RAND(2) ? face : Opposite(face);
            // (v1.1 sea: a Swimmer, where a sea of another kind fell back to
            // this one, treads water off the island.)
            if (RogueGen_TrainerSwims(gen->r, gen->m->place, placedT))
            {
                fx += (int)RAND(9) - 4;
                fy += (int)RAND(9) - 4;
                if (!SeaSwimSpot(gen, fx, fy, &f) || IsCrowded(gen, fx, fy) || NearTrainer(gen, fx, fy) || SeenFromNextDoor(gen, fx, fy))
                    continue;
                AddObj(gen, ROBJ_TRAINER, fx, fy, f, placedT++, 3);
                break;
            }
            if (!InMap(gen, fx, fy) || !WalkableCover(gen, fx, fy) || IsOccupied(gen, fx, fy) || IsCrowded(gen, fx, fy)
             || NearTrainer(gen, fx, fy) || SeenFromNextDoor(gen, fx, fy))
                continue;
            // On the shore of the island, at first.
            if (tries < 20 && !HasNeighbor(gen, fx >> 1, fy >> 1, C_SEA, FALSE))
                continue;
            // Facing open water, from the water's edge rather than one
            // block back from it, at first.
            if (!IsSeaF(gen, fx + sDirDX[f], fy + sDirDY[f]) && (IsSeaF(gen, fx - sDirDX[f], fy - sDirDY[f])
             || !IsSeaF(gen, fx + sDirDX[f] * 2, fy + sDirDY[f] * 2)))
                f = Opposite(f);
            if (tries < 20 && !IsSeaF(gen, fx + sDirDX[f], fy + sDirDY[f]) && IsSeaF(gen, fx + sDirDX[f] * 2, fy + sDirDY[f] * 2))
                continue;
            AddObj(gen, ROBJ_TRAINER, fx, fy, f, placedT++, 3);
            break;
        }
    }
    for (i = n - 1; i >= 0 && placedI < p->nItems; i--)
    {
        for (tries = 0; tries < 30; tries++)
        {
            int fx = ix[i] * 2 + (int)RAND(4) - 1, fy = iy[i] * 2 + (int)RAND(4) - 1;
            if (!InMap(gen, fx, fy) || !WalkableCover(gen, fx, fy) || IsOccupied(gen, fx, fy) || IsCrowded(gen, fx, fy)
             || SeenFromNextDoor(gen, fx, fy))
                continue;
            SetCover(gen, fx, fy, V_GROUND);
            AddObj(gen, ROBJ_ITEM, fx, fy, RDIR_SOUTH, placedI++, 0);
            break;
        }
    }
    gen->crowdRadius = 1;
    for (tries = 0; tries < 30 && (placedT < p->nTrainers || placedI < p->nItems); tries++)
    {
        int i2 = RAND(n), fx = ix[i2] * 2 + (int)RAND(4) - 1, fy = iy[i2] * 2 + (int)RAND(4) - 1;
        if (!InMap(gen, fx, fy) || !WalkableCover(gen, fx, fy) || IsOccupied(gen, fx, fy) || IsCrowded(gen, fx, fy)
         || SeenFromNextDoor(gen, fx, fy))
            continue;
        if (placedT < p->nTrainers && NearTrainer(gen, fx, fy))
            continue;
        if (placedT < p->nTrainers)
            AddObj(gen, ROBJ_TRAINER, fx, fy, RAND(2) ? face : Opposite(face), placedT++, 3);
        else
            AddObj(gen, ROBJ_ITEM, fx, fy, RDIR_SOUTH, placedI++, 0);
    }
    gen->crowdRadius = 0;
    if (placedT < p->nTrainers || placedI < p->nItems)
        return FALSE;
    TidyGround(gen);
    Render(gen);
    {
        // A sign at the way in, beside the landing, off the way.
        int fx, fy, k;
        u8 facing;
        // (v1.1 towns2) From a raft village the landing is at the far end.
        if (SeaToRaft(gen, inDir))
        {
            ax = bx;
            ay = by;
        }
        fx = ax * 2 + 1;
        fy = ay * 2 + 1;
        FloodReachable(gen, ax, ay);
        for (k = 0; k < 4; k++)
            if (FindSignSpot(gen, ax, ay, &fx, &fy, &facing))
                break;
        if (k == 4)
        {
            fx = ax * 2 + 1;
            fy = ay * 2 + 1;
        }
        if (!IsOccupied(gen, fx, fy))
        {
            AddSign(gen, fx, fy, RSIGN_ROUTE);
            RenderSigns(gen);
        }
    }
    return TRUE;
}

// Whether a block can be walked on, judged from the units before they are
// drawn: open ground, the top half of a ledge unit, and no one standing there.
static bool8 UnitWalkF(struct Gen *gen, int fx, int fy)
{
    u8 c;
    if (!InMap(gen, fx, fy) || IsOccupied(gen, fx, fy))
        return FALSE;
    c = Cell(gen, fx >> 1, fy >> 1);
    if (c == C_LEDGE)
        return !(fy & 1);
    return IsOpenU(c) && c != C_KEEP;
}

// TRUE if something standing on block (x, y) would cut the ground around it
// in two: its open neighbours do not all meet round the ring of eight blocks.
static bool8 BlocksWay(struct Gen *gen, int x, int y)
{
    static const s8 sRingX[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    static const s8 sRingY[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
    bool8 open[8];
    int k, start = -1, groups = 0, sides = 0;
    for (k = 0; k < 8; k++)
        open[k] = UnitWalkF(gen, x + sRingX[k], y + sRingY[k]);
    for (k = 0; k < 8; k += 2)
        sides += open[k];
    if (sides <= 1)
        return FALSE;
    // Count the runs of open ring blocks that hold a side neighbour.
    for (k = 0; k < 8; k++)
        if (!open[k])
            start = k;
    if (start < 0)
        return FALSE;
    for (k = 1; k <= 8; k++)
    {
        int i = (start + k) & 7;
        bool8 side = FALSE;
        if (!open[i])
            continue;
        while (open[i] && k <= 8)
        {
            if (!(i & 1))
                side = TRUE;
            k++;
            i = (start + k) & 7;
        }
        groups += side;
    }
    return groups > 1;
}

// TRUE if water shows on screen somewhere along the shortest walk: within
// 7 blocks across and 5 up or down, as the camera follows the player.
static bool8 WaterInView(struct Gen *gen)
{
    int i, x, y;
    for (i = 0; i < gen->nWalk; i += 2)
    {
        YIELD();
        int px = PX(gen->walk[i]), py = PY(gen->walk[i]);
        for (y = py - 5; y <= py + 5; y++)
            for (x = px - 7; x <= px + 7; x++)
                if (InMap(gen, x, y) && (gen->m->blocks[y * gen->m->w + x] >> 12) == 1
                 && !((gen->m->blocks[y * gen->m->w + x] >> 10) & 3))
                    return TRUE;
    }
    return FALSE;
}

// WaterInView for the shortest walk taken either way: shortest walks that
// tie can run far apart in the open, and a player may take either, so the
// water must show from both (the walk is left as it was).
static bool8 WaterInViewBothWays(struct Gen *gen)
{
    int sx = gen->walkSX, sy = gen->walkSY, ex = gen->walkEX, ey = gen->walkEY;
    bool8 ok = WaterInView(gen);
    if (ok && FindShortestWalk(gen, ex, ey, sx, sy))
        ok = WaterInView(gen);
    FindShortestWalk(gen, sx, sy, ex, ey);
    return ok;
}

static s32 ForestOf(struct Gen *gen);
static bool8 ForestCutsRoute(struct Gen *gen);
static bool8 PlanForestCut(struct Gen *gen, int ax, int ay, int bx, int by);
static bool8 CarveForestCut(struct Gen *gen, int ax, int ay, int bx, int by);
static bool8 PlaceForestWay(struct Gen *gen, int ax, int ay);
static void RenderForestWays(struct Gen *gen);
static bool8 ForestWaysClear(struct Gen *gen, int ax, int ay);

static s32 PlantOf(struct Gen *gen);

// Any water unit on the map.
static bool8 HasWater(struct Gen *gen)
{
    int i;
    for (i = 0; i < gen->cw * gen->ch; i++)
        if (gen->g[i] == C_SEA)
            return TRUE;
    return FALSE;
}

// TRUE if block (fx, fy) lies in the unit in front of a way into the woods.
static bool8 OnForestWayGround(struct Gen *gen, int fx, int fy)
{
    int k, gx, gy;
    for (k = 0; k < gen->nForestWays; k++)
    {
        ForestWayGround(gen, k, &gx, &gy);
        if ((fx >> 1) == gx && (fy >> 1) == gy)
            return TRUE;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// Route features (src/rogue_gen_features.h, included at the end): the lot
// GenRoute lays for a feature, and the steps it calls.

// The lot of a feature: its house and what stands beside it, in units.
struct FeatLot
{
    const struct Building *house;
    u8 hx, hy;              // the house within the lot (units)
    const struct Building *side;
    u8 sx, sy;              // the yard or garden within the lot (units)
    u8 uw, uh;              // the lot (units)
    u8 openX;               // a second unit below the lot that must stay open (the garden's way in), 0xFF none
};

struct FeatRoute
{
    u8 feature;
    s8 pair;
    bool8 hasLot;
    bool8 restPlanned;      // the plan put a rest house here: the feature's house lets travellers rest
    struct FeatLot lot;
    int bx, by;             // the lot's top left unit
    int signX, signY;       // the feature's sign (blocks), -1 none
};

// A feature's lot is fitted smaller from FEAT_SMALL_FROM tries (a beach hut
// for the Seashore House), may lengthen the walk a little from
// FEAT_ANY_WALK_FROM, and after FEAT_GIVE_UP tries the route is built
// without it (before the tries where a coast's sea may leave the walk's view).
#define FEAT_SMALL_FROM    10
#define FEAT_ANY_WALK_FROM 18
#define FEAT_GIVE_UP       28
static void FeatBegin(struct Gen *gen, struct FeatRoute *fr);
static bool8 FeatPlaceLot(struct Gen *gen, struct FeatRoute *fr);
static bool8 FeatPeople(struct Gen *gen, struct FeatRoute *fr, bool8 *fail);
static void FeatDoor(struct Gen *gen, struct FeatRoute *fr);
static void FeatSigns(struct Gen *gen, struct FeatRoute *fr);
static void FeatRender(struct Gen *gen, struct FeatRoute *fr);
static void FeatWalkBy(struct Gen *gen, struct FeatRoute *fr, bool8 occupied);

// ---------------------------------------------------------------------------
// The way across a route, as Emerald draws it: a field route (101, 102) is
// open grass with a few wide, soft patches of pale path that hint at the way;
// a road route (103's east, 104, 110, 117) has one sand road, two to four
// blocks wide, with wide elbows and stepped diagonals at its bends. Each
// route takes one from its own hash, fitted to its kind and theme.
static bool8 RouteRoadStyle(const struct RogueRegion *r, u32 place)
{
    const struct RoguePlace *q = &r->places[place];
    int pct;
    if (q->kind != RPLACE_ROUTE)
        return FALSE;
    switch (q->type)
    {
    case RROUTE_FIELD:  pct = 25; break;
    case RROUTE_FLOWERS: pct = 60; break;   // Route 117's sand road
    case RROUTE_LONGGRASS: return FALSE;    // the way is through the grass
    case RROUTE_FOREST: pct = 55; break;
    case RROUTE_LAKE:
    case RROUTE_RIVER:  pct = 45; break;
    case RROUTE_COAST:  pct = 55; break;
    case RROUTE_WOODS:  pct = 70; break;
    case RROUTE_HILLS:  pct = 60; break;
    case RROUTE_DESERT:
    case RROUTE_MOUNTAIN: pct = 100; break;
    default:            return FALSE;
    }
    switch (q->theme)
    {
    case RTHEME_MEADOW:
    case RTHEME_VERDANT:
        pct -= 15;
        break;
    case RTHEME_HARBOR:
    case RTHEME_ISLAND:
    case RTHEME_COVE:
    case RTHEME_BEACH:
    case RTHEME_STONE:
    case RTHEME_TREETOP:
        pct += 15;
        break;
    }
    return (int)(RogueRng_Hash(r->seed, 6650 + place) % 100) < pct;
}

// What the way shows where it crosses the seam toward target, the same seen
// from either side: a town's own way out, sand between two road routes, and
// otherwise grass, as Hoenn's routes meet. A paved town's paving runs to its
// edge (or stops before a seam toward another tileset), and the route's sand
// or pale path stops short of it in the grass, as Route 104's sand stops
// below Rustboro and Route 117's before Mauville: paving never meets sand.
static u8 SeamWay(struct Gen *gen, u8 target)
{
    const struct RoguePlace *t = &gen->r->places[target];
    if (t->kind == RPLACE_TOWN)
    {
        if (sThemes[t->theme].roadTL && sThemes[t->theme].roadTL != B_SAND_TL)
            return V_SEAM_GAP;
        return EndStyle(gen, target);
    }
    if (RouteRoadStyle(gen->r, target) && RouteRoadStyle(gen->r, gen->p - gen->r->places))
        return V_SAND;
    return V_NONE;
}

// How far a unit is from the nearest way out (in units, walking distance
// ignored), and which material that end shows at its seam.
static int WayEndDist(int nEnds, const int *ex, const int *ey, const u8 *seam, int x, int y, u8 *mat)
{
    int k, best = 99;
    *mat = V_NONE;
    for (k = 0; k < nEnds; k++)
    {
        int d = Abs(x - ex[k]) + Abs(y - ey[k]);
        if (d < best)
        {
            best = d;
            *mat = seam[k];
        }
    }
    return best;
}

// May the route's own way (cover) be drawn d units from an end whose seam
// shows mat? Right by a seam of grass it is grass too; by another material,
// that material's tongue and then grass until minEnd.
static bool8 WayMayTake(int d, u8 mat, u8 cover, int minEnd)
{
    if (mat == cover)
        return TRUE;
    if (mat == V_NONE)
        return d > 0;
    if (mat == V_SEAM_GAP)
        return d >= 2;
    return d >= minEnd;
}

// A path unit's centre, in quarter blocks, smoothed over the units two either
// side along its stretch of the walk (a stretch ends where the walk jumps: a
// branch, a forest cut). Right-angle turns come out as diagonals.
static void WayCentre(struct Gen *gen, int i, int s, int e, int *cx, int *cy)
{
    int j, n = 0, sx = 0, sy = 0;
    for (j = i - 2; j <= i + 2; j++)
    {
        if (j < s || j >= e)
            continue;
        sx += 4 * (2 * (gen->path[j] % gen->cw) + 1);
        sy += 4 * (2 * (gen->path[j] / gen->cw) + 1);
        n++;
    }
    *cx = sx / n;
    *cy = sy / n;
}

// Paint a square of way cover, half-width hw quarter blocks round (cx, cy)
// in quarter blocks, on open ground away from the ends (minEnd units, or the
// tongue's own when the end shows the same).
static void WayBrush(struct Gen *gen, int cx, int cy, int hw, u8 cover, int nEnds, const int *ex, const int *ey,
                     const u8 *seam, int minEnd)
{
    int fx, fy;
    for (fy = (cy - hw - 2) / 4; fy <= (cy + hw - 2) / 4 + 1; fy++)
    {
        for (fx = (cx - hw - 2) / 4; fx <= (cx + hw - 2) / 4 + 1; fx++)
        {
            u8 mat;
            int d;
            if (4 * fx + 2 < cx - hw || 4 * fx + 2 > cx + hw || 4 * fy + 2 < cy - hw || 4 * fy + 2 > cy + hw)
                continue;
            if (!InMap(gen, fx, fy) || !PathCanTake(gen, fx, fy, cover))
                continue;
            // A block of grass between the way and a tree beside it, as
            // Hoenn keeps (the walk's own units are painted whole).
            if (Cell(gen, fx >> 1, fy >> 1) != C_ROAD
             && ((InMap(gen, fx - 1, fy) && Cell(gen, (fx - 1) >> 1, fy >> 1) == C_TREE)
              || (InMap(gen, fx + 1, fy) && Cell(gen, (fx + 1) >> 1, fy >> 1) == C_TREE)
              || (InMap(gen, fx, fy - 1) && Cell(gen, fx >> 1, (fy - 1) >> 1) == C_TREE)
              || (InMap(gen, fx, fy + 1) && Cell(gen, fx >> 1, (fy + 1) >> 1) == C_TREE)))
                continue;
            d = WayEndDist(nEnds, ex, ey, seam, fx >> 1, fy >> 1, &mat);
            if (!WayMayTake(d, mat, cover, minEnd))
                continue;
            SetCover(gen, fx, fy, cover);
        }
    }
}

// Where the stretch of the walk from s ends: at the first unit that is not
// beside the one before it.
static int WayStretchEnd(struct Gen *gen, int s)
{
    int e = s + 1;
    while (e < gen->nPath)
    {
        int a = gen->path[e - 1], b = gen->path[e];
        if (Abs(a % gen->cw - b % gen->cw) + Abs(a / gen->cw - b / gen->cw) != 1)
            break;
        e++;
    }
    return e;
}

// The route's way, in its style (RouteRoadStyle). Each end first shows what
// its seam shows (SeamWay): the same material runs on; another one is a
// tongue of two units ending in its own edge, then grass before the route's
// own way starts, so two materials never meet in a square join.
#define WAY_TONGUE 1 // units from the edge unit that show the seam's material
#define WAY_GAP    4 // the route's own material starts this far from a foreign end
static void PaintRouteWay(struct Gen *gen, bool8 road, int nEnds, const int *ex, const int *ey, const u8 *seam)
{
    int i, s, e;
    u8 own = road ? V_SAND : V_PATH;

    if (road)
    {
        // A road across the map between two rows of trees would lose its
        // lower row under the canopy of the trees below (TidyPaths) and
        // break. There the tree below gives way to a nook of grass, shut in
        // on its other sides, so the way is no wider than it was. Not in the
        // narrow way past an obstacle or a lane to one.
        u16 nooks[32];
        int nNooks = 0;
        for (i = 0; i < gen->nPath; i++)
        {
            int px = gen->path[i] % gen->cw, py = gen->path[i] / gen->cw, k, j;
            bool8 ok = TRUE;
            YIELD();
            if ((gen->guardEnd && i <= gen->guardEnd + 1) || py + 2 >= gen->ch || IsOpenU(Cell(gen, px, py - 1))
             || Cell(gen, px, py + 1) != C_TREE || IsEdge(gen, px, py + 1) || IsOpenU(Cell(gen, px, py + 2))
             || InGateLaneF(gen, px * 2, py * 2) || nNooks == 32)
                continue;
            // Beside it only trees, rock and the like, another such nook, or
            // the road itself where it turns down (not past an obstacle).
            for (k = -1; k <= 1 && ok; k += 2)
            {
                int nx = px + k, ny = py + 1;
                if (!IsOpenU(Cell(gen, nx, ny)))
                    continue;
                if (Cell(gen, nx, ny) == C_ROAD && !InGateLaneF(gen, nx * 2, ny * 2))
                {
                    for (j = 0; j <= gen->guardEnd + 1 && j < gen->nPath && gen->path[j] != ny * gen->cw + nx; j++)
                        ;
                    ok = !gen->guardEnd || j > gen->guardEnd + 1 || j >= gen->nPath;
                    continue;
                }
                for (j = 0; j < nNooks && nooks[j] != PACK(nx, ny); j++)
                    ;
                ok = j < nNooks;
            }
            if (!ok)
                continue;
            SetCell(gen, px, py + 1, C_OPEN);
            nooks[nNooks++] = PACK(px, py + 1);
        }
    }

    // Every unit near an end: the seam's material, one unit wide so both
    // sides of the seam agree.
    for (i = 0; i < gen->nPath; i++)
    {
        int px = gen->path[i] % gen->cw, py = gen->path[i] / gen->cw, d;
        u8 mat;
        YIELD();
        d = WayEndDist(nEnds, ex, ey, seam, px, py, &mat);
        if (d <= WAY_TONGUE && mat != V_NONE && mat != V_SEAM_GAP)
            PaintUnit(gen, px, py, mat);
        else if (road && WayMayTake(d, mat, own, WAY_GAP))
            PaintUnit(gen, px, py, own);
    }
    if (road)
    {
        // The road: the units themselves (so it never breaks) and a brush
        // along the smoothed walk, three blocks wide with the third row on
        // the open side, four here and there.
        for (s = 0; s < gen->nPath; s = e)
        {
            int lean = RAND(2) ? 2 : -2, run = 0, wide = 0;
            e = WayStretchEnd(gen, s);
            for (i = s; i + 1 < e; i++)
            {
                int ax, ay, bx, by, t, px = gen->path[i] % gen->cw, py = gen->path[i] / gen->cw;
                bool8 across;
                YIELD();
                WayCentre(gen, i, s, e, &ax, &ay);
                WayCentre(gen, i + 1, s, e, &bx, &by);
                across = Abs(bx - ax) >= Abs(by - ay);
                if (--run <= 0)
                {
                    run = 4 + RAND(6);
                    lean = RAND(2) ? 2 : -2;
                    wide = RAND(4) == 0;
                }
                // Across the map the extra row goes up over a tree below (its
                // canopy would take the lower row), else to the open side.
                if (across)
                {
                    if (HasTreeBelow(gen, px, py))
                        lean = -2;
                    else if (Cell(gen, px, py - 1) == C_TREE && IsOpenU(Cell(gen, px, py + 1)))
                        lean = 2;
                }
                else if (Cell(gen, px - 1, py) == C_TREE && IsOpenU(Cell(gen, px + 1, py)))
                {
                    lean = 2;
                }
                else if (Cell(gen, px + 1, py) == C_TREE && IsOpenU(Cell(gen, px - 1, py)))
                {
                    lean = -2;
                }
                for (t = 0; t < 4; t++)
                {
                    int cx = ax + (bx - ax) * t / 4, cy = ay + (by - ay) * t / 4;
                    if (!wide)
                    {
                        if (across)
                            cy += lean;
                        else
                            cx += lean;
                    }
                    WayBrush(gen, cx, cy, 6, own, nEnds, ex, ey, seam, WAY_GAP);
                }
            }
        }
    }
    else
    {
        // A field: a few soft patches of pale path spread along the walk,
        // never two touching, and the way between them plain grass.
        int want = 2 + RAND(2), placed = 0, tries, last = -99;
        s = 0;
        e = WayStretchEnd(gen, 0);
        for (tries = 0; tries < 3 * want && placed < want && e - s > 6; tries++)
        {
            int span = (e - s) / want, c = s + span * placed + span / 2 + (int)RAND(3) - 1;
            int a = 1 + RAND(2), j, k, hw = 6 + 2 * RAND(2);
            u8 mat;
            if (c - a - 2 <= last || c + a >= e)
                continue;
            if (!WayMayTake(WayEndDist(nEnds, ex, ey, seam, gen->path[c] % gen->cw, gen->path[c] / gen->cw, &mat),
                            mat, V_PATH, WAY_GAP))
            {
                placed++;
                continue;
            }
            for (j = c - a; j < c + a; j++)
            {
                int ax, ay, bx, by;
                YIELD();
                WayCentre(gen, j, s, e, &ax, &ay);
                WayCentre(gen, j + 1, s, e, &bx, &by);
                for (k = 0; k < 4; k++)
                {
                    // Narrower at the two ends, so the patch is soft.
                    int h = (j == c - a && k < 2) || (j == c + a - 1 && k >= 2) ? hw - 2 - 2 * RAND(2) : hw - 2 * RAND(2);
                    WayBrush(gen, ax + (bx - ax) * k / 4, ay + (by - ay) * k / 4, h, V_PATH, nEnds, ex, ey, seam, WAY_GAP);
                }
            }
            last = c + a;
            placed++;
        }
        // Where a village's pale path comes in, it widens into a patch.
        for (i = 0; i < gen->nPath; i++)
        {
            int px = gen->path[i] % gen->cw, py = gen->path[i] / gen->cw, d;
            u8 mat;
            d = WayEndDist(nEnds, ex, ey, seam, px, py, &mat);
            if (mat == V_PATH && d >= 2 && d <= 3)
                WayBrush(gen, 4 * (2 * px + 1), 4 * (2 * py + 1), 8, V_PATH, nEnds, ex, ey, seam, WAY_GAP);
        }
    }
}

// A patch of pale path cut down to a scrap (by trees' canopies, a sign) no
// longer hints at the way: grass. Pieces running off the map go on next door.
static void DropPathScraps(struct Gen *gen)
{
    u16 list[WAY_PATCH_MIN + 1];
    int x, y, n, k;
    for (y = 0; y < gen->m->h; y++)
    {
        YIELD();
        for (x = 0; x < gen->m->w; x++)
        {
            if (Cover(gen, x, y) != V_PATH || GroundOf(gen, x, y) != V_PATH)
                continue;
            n = SmallPiece(gen, x, y, V_PATH, list, WAY_PATCH_MIN);
            if (n == 0 || n > WAY_PATCH_MIN)
                continue;
            for (k = 0; k < n; k++)
                SetCover(gen, PX(list[k]), PY(list[k]), V_NONE);
        }
    }
}

// Tall grass where Hoenn never has it against a way: left of the pale
// path's or the sand's left edge, above the sand's top corners and right of
// its right corners.
static void ClearGrassByWay(struct Gen *gen)
{
    int x, y;
    for (y = 0; y < gen->m->h; y++)
    {
        YIELD();
        for (x = 0; x < gen->m->w; x++)
        {
            u8 c = Cover(gen, x, y);
            bool8 n, s, w, e;
            if (c != V_PATH && c != V_SAND)
                continue;
            n = LooksSame(gen, x, y - 1, c);
            s = LooksSame(gen, x, y + 1, c);
            w = LooksSame(gen, x - 1, y, c);
            e = LooksSame(gen, x + 1, y, c);
            if (!w && x > 0 && Cover(gen, x - 1, y) == V_TALL)
                SetCover(gen, x - 1, y, V_NONE);
            if (c != V_SAND)
                continue;
            if (!n && (!w || !e) && y > 0 && Cover(gen, x, y - 1) == V_TALL)
                SetCover(gen, x, y - 1, V_NONE);
            if (!e && (!n || !s) && x + 1 < gen->m->w && Cover(gen, x + 1, y) == V_TALL)
                SetCover(gen, x + 1, y, V_NONE);
        }
    }
}

// v1.1 land: flower fields and the long-grass maze (LandRouteKinds,
// LandFlowerBeds, LandLongGrassFields).
#include "rogue_gen_land.h"
#include "rogue_gen_berry.h" // (v1.1 berry farms)
// Bridge routes (v1.1 bridge).
#include "rogue_gen_bridge.h"

static bool8 GenRoute(struct Gen *gen)
{
    const struct RoguePlace *p = gen->p;
    const struct RogueRegion *r = gen->r;
    const struct TownTheme *th = &sThemes[p->theme < RTHEME_COUNT ? p->theme : 0];
    bool8 mountain = (p->type == RROUTE_MOUNTAIN);
    u8 inDir = RDIR_NONE, outDir = RDIR_NONE;
    u8 ends[ROGUE_MAX_EXITS], endStyle[ROGUE_MAX_EXITS];
    int nEnds = 0;
    int ex[ROGUE_MAX_EXITS], ey[ROGUE_MAX_EXITS];
    int ax, ay, bx, by, i, n, tries, top = 2, guardLen = 0;
    int mx[3], my[3];
    struct Placed rest;
    struct FeatRoute feat;
    bool8 hasRest = FALSE, green = GreenRoute(p), lake = FALSE, beachTrail = FALSE, mouth, featQuiet = FALSE, road;
    int nookX = -1, nookY = -1, tunX = -1, tunY = -1;
    // The obstacle at the way in, unless it stands in the route next door.
    bool8 guarded = p->obstacle && !ObstacleOutside(r, p);
    int guardStep = 2, outStep = 0;

    // The way in and the way on: along the main road (lower and higher place
    // numbers), or for a side route from the main road to whatever lies
    // beyond. Other links are side branches.
    for (i = 0; i < p->nLinks; i++)
    {
        const struct RogueLink *l = &p->links[i];
        const struct RoguePlace *t = &r->places[l->target];
        if (p->side)
        {
            if (l->dir == p->guardDir)
                inDir = l->dir;
            else
                outDir = l->dir;
        }
        else if (t->side)
        {
            continue;
        }
        else if (l->target < (u8)(p - r->places))
        {
            inDir = l->dir;
        }
        else
        {
            outDir = l->dir;
        }
    }
    if (inDir == RDIR_NONE)
        return FALSE;
    if (p->type == RROUTE_SEA && outDir != RDIR_NONE)
        return GenSeaRoute(gen, inDir, outDir);
    // v1.1 sea: a side route out to an island (src/rogue_gen_sea.h).
    if (p->type == RROUTE_SEA)
        return SeaGenKind(gen, RogueGen_SeaKind(r, gen->m->place), inDir, RDIR_NONE);
    // (A bridge that will not fit is a route of the ordinary kind.)
    if (p->type == RROUTE_BRIDGE && outDir != RDIR_NONE && gen->attempt < 50)
        return GenBridgeRoute(gen, inDir, outDir);

    ChooseRouteStyle(gen);
    if (gen->rocky || mountain)
        gen->bites = FALSE;
    // A forest the road goes through cuts the route in two.
    gen->forestCut = ForestCutsRoute(gen);
    Fill(gen, C_TREE);
    gen->layout = RogueRng_Hash(r->seed, 6200 + (u32)(p - r->places)) % LAYOUT_COUNT;
    if (p->type == RROUTE_COAST && p->seaSide != RDIR_NONE)
    {
        ShapeShore(gen);
        // The trail runs along the beach, as on Routes 104 and 109 (v1.1:
        // every coast; some kept it up on the bank before).
        beachTrail = TRUE;
    }
    // A route up to a cave, or to a power plant's door (as Route 110's
    // New Mauville), ends at a rock face along the top.
    mouth = p->mouth != 0xFF || PlantOf(gen) >= 0 || VolcanoMountainAt(r, p - r->places);
    if (mouth && VolcanoMountainAt(r, p - r->places))
    {
        // (v1.1) The volcano's foot route: its mountain along the top, the
        // doorway into Fiery Path in it, as on Route 112.
        // A dead end's trail ends at the stair; on a way through, a road
        // joins the stair to the trail, as to a tunnel's mouth.
        // Beside a way north, if there is one (VolcanoSpan), with woods
        // kept between them.
        int vx0, vw, k, y;
        VolcanoSpan(r, p, &vx0, &vw);
        if (outDir == RDIR_NONE)
            PlaceVolcanoHill(gen, vx0, vw, &bx, &by);
        else
            PlaceVolcanoHill(gen, vx0, vw, &tunX, &tunY);
        if (LinkOnSide(p, RDIR_NORTH))
            for (k = 0; k < 2; k++)
                for (y = 0; y <= 5; y++)
                    SetCell(gen, VolcanoGapX(r, p, k), y, C_TREE);
        top = 6;
    }
    else if (mouth)
    {
        // The road ends at a cave mouth instead of an open edge.
        PlaceMesa(gen, &bx, &by);
        if (p->mouth == 0xFF)
            gen->m->warps[gen->m->nWarps - 1].kind = RWARP_DUNGEON;
        top = 4;
    }
    else if (RogueGen_TunnelMouthAt(r, p - r->places))
    {
        // A tunnel's far mouth in a rock face along the top, off the trail.
        int x0, w;
        DungeonMesaSpan(r, p, &x0, &w);
        PlaceMesaAt(gen, x0, w, &tunX, &tunY);
        gen->m->warps[gen->m->nWarps - 1].kind = RWARP_DUNGEON_2;
        top = 4;
    }
    // The water comes first, and the trail finds its way round it.
    // On a small map that keeps failing, a pond will do instead.
    // (v1.1 water: a winding river with a bridge, pond country with a
    // bridged pond; rogue_gen_water.h. The old river and lake after that.)
    if (p->type == RROUTE_RIVER && !WaterWindingRiver(gen, top, inDir, outDir)
     && !ShapeRiverAcross(gen, top, inDir, outDir) && gen->attempt < 10)
        lake = ShapeBigLake(gen, top);
    if (p->type == RROUTE_LAKE && WaterPondCountry(gen))
        lake = WaterBridgePond(gen, top, inDir, outDir);
    if (p->type == RROUTE_LAKE && !lake && gen->attempt < 10)
        lake = ShapeBigLake(gen, top);
    // A river route must show its water from the walk (checked at the
    // end, as for a coast): with none at all, try again now.
    if (p->type == RROUTE_RIVER && gen->attempt < 30 && !HasWater(gen))
        return FALSE;
    MakeOpening(gen, inDir, &ax, &ay);
    ends[nEnds] = inDir;
    ex[nEnds] = gen->path[gen->nPath - 1] % gen->cw;
    ey[nEnds] = gen->path[gen->nPath - 1] / gen->cw;
    endStyle[nEnds++] = SeamWay(gen, LinkOnSide(p, inDir)->target);
    if (guarded)
    {
        // A straight way in, one unit wide, so the obstacle cannot be
        // walked around, and short, so what lies beyond is in sight. Beside
        // a town it stands deeper, out of sight from the town.
        u8 d = Opposite(inDir);
        if (p->obstacle != ROBST_SURF)
            guardStep = (inDir == RDIR_NORTH || inDir == RDIR_SOUTH) ? 3 : 4;
        guardLen = guardStep + 1;
        for (i = 0; i < guardLen; i++)
        {
            YIELD();
            ax += sDirDX[d];
            ay += sDirDY[d];
            if (IsEdge(gen, ax, ay))
                return FALSE;
            AddRoadCell(gen, ax, ay);
        }
        if (p->obstacle == ROBST_STRENGTH)
        {
            // The way turns aside, and straight on is a nook: the boulder
            // is pushed into it and out of the way, as in Emerald.
            u8 side = (d + (RAND(2) ? 1 : 3)) & 3;
            if (IsEdge(gen, ax + sDirDX[side] * 2, ay + sDirDY[side] * 2))
                side = Opposite(side);
            nookX = ax + sDirDX[d];
            nookY = ay + sDirDY[d];
            ax += sDirDX[side];
            ay += sDirDY[side];
            if (IsEdge(gen, ax, ay) || IsEdge(gen, nookX + sDirDX[d], nookY + sDirDY[d]))
                return FALSE;
            AddRoadCell(gen, ax, ay);
        }
    }
    if (outDir != RDIR_NONE)
    {
        MakeOpening(gen, outDir, &bx, &by);
        ends[nEnds] = outDir;
        ex[nEnds] = gen->path[gen->nPath - 1] % gen->cw;
        ey[nEnds] = gen->path[gen->nPath - 1] / gen->cw;
        endStyle[nEnds++] = SeamWay(gen, LinkOnSide(p, outDir)->target);
        // The block on the main road ahead stands in a lane at the way on,
        // on this side of the seam: the road leads up to it.
        outStep = GateLaneStep(gen, LinkOnSide(p, outDir));
        if (outStep && !CarveGateLane(gen, outDir, outStep, &bx, &by))
            return FALSE;
    }
    else if (!mouth)
    {
        // A dead end: the road leads to a clearing on the far side.
        switch (inDir)
        {
        case RDIR_SOUTH: by = 3 + RAND(2); bx = 3 + RAND(gen->cw - 6); break;
        case RDIR_NORTH: by = gen->ch - 4 - RAND(2); bx = 3 + RAND(gen->cw - 6); break;
        case RDIR_WEST:  bx = gen->cw - 4 - RAND(2); by = 3 + RAND(gen->ch - 6); break;
        default:         bx = 3 + RAND(2); by = 3 + RAND(gen->ch - 6); break;
        }
        gen->endX = bx;
        gen->endY = by;
    }
    if (gen->forestCut && !PlanForestCut(gen, ax, ay, bx, by))
        return FALSE;
    gen->roadTL = th->roadTL;
    gen->roadCentre = th->roadCentre;
    // Waypoints: halfway to the middle of the map, the middle, and halfway
    // on to the exit, each pushed well off the straight line. Going through
    // the middle uses the whole map, even when the exit is round a corner.
    // The road keeps within the box around them, so a box that covers only
    // a corner of the map (a slab of dead forest) is rolled again.
    for (tries = 0; tries < 30; tries++)
    {
        int cx = gen->cw / 2, cy = top + (gen->ch - top) / 2;
        int sx = gen->cw * 3 / 10, sy = (gen->ch - top) * 3 / 10;
        bool8 across = Abs(bx - ax) >= Abs(by - ay);
        int sign = RAND(2) ? 1 : -1;
        int x0, x1, y0, y1;
        mx[0] = (ax + cx) / 2; my[0] = (ay + cy) / 2;
        mx[1] = cx;            my[1] = cy;
        mx[2] = (cx + bx) / 2; my[2] = (cy + by) / 2;
        for (i = 0; i < 3; i++)
        {
            int rx = i == 1 ? 2 : sx, ry = i == 1 ? 2 : sy;
            if (i == 1)
            {
                mx[i] += (int)RAND(2 * rx + 1) - rx;
                my[i] += (int)RAND(2 * ry + 1) - ry;
            }
            else
            {
                // Swing to one side on the way in and the other side on the
                // way out, so the trail never runs dead straight.
                int swing = sign * (int)(((across ? gen->ch - top : gen->cw) * (20 + RAND(16))) / 100);
                if (across)
                {
                    mx[i] += (int)RAND(5) - 2;
                    my[i] = cy + swing;
                }
                else
                {
                    mx[i] = cx + swing;
                    my[i] += (int)RAND(5) - 2;
                }
                sign = -sign;
            }
            if (mx[i] < 2) mx[i] = 2;
            if (mx[i] > gen->cw - 3) mx[i] = gen->cw - 3;
            if (my[i] < top) my[i] = top;
            if (my[i] > gen->ch - 3) my[i] = gen->ch - 3;
            if (p->seaSide != RDIR_NONE)
                ClampFromSea(gen, &mx[i], &my[i]);
        }
        x0 = x1 = ax;
        y0 = y1 = ay;
        for (i = 0; i < 4; i++)
        {
            int px = i < 3 ? mx[i] : bx, py = i < 3 ? my[i] : by;
            if (px < x0) x0 = px;
            if (px > x1) x1 = px;
            if (py < y0) y0 = py;
            if (py > y1) y1 = py;
        }
        if ((x1 - x0 + 1) * (y1 - y0 + 1) * 2 >= gen->cw * gen->ch)
            break;
    }
    if (green)
    {
        if (WaterBridgeWay(gen, ax, ay, bx, by, mx, my))
        {
            // Over the bridge (rogue_gen_water.h).
        }
        else if (beachTrail)
        {
            // Down to the beach and along it, as on Routes 104 and 109.
            bool8 vert = (p->seaSide == RDIR_WEST || p->seaSide == RDIR_EAST);
            int pa = vert ? ay : ax, pb = vert ? by : bx;
            for (i = 0; i < 3; i++)
                ShoreUnit(gen, pa + (pb - pa) * (i + 1) / 4, &mx[i], &my[i]);
        }
        else if (lake)
        {
            // Round one side of the lake, along its shore.
            int lx0 = gen->cw, lx1 = -1, ly0 = gen->ch, ly1 = -1, x, y, k;
            for (y = 0; y < gen->ch; y++)
            {
                for (x = 0; x < gen->cw; x++)
                {
                    if (Cell(gen, x, y) != C_SEA || BitU(gen->river, gen, x, y))
                        continue;
                    if (x < lx0) lx0 = x;
                    if (x > lx1) lx1 = x;
                    if (y < ly0) ly0 = y;
                    if (y > ly1) ly1 = y;
                }
            }
            if (lx1 >= 0)
            {
                bool8 across = Abs(bx - ax) >= Abs(by - ay);
                u8 pick = RAND(2);
                for (k = 0; k < 2; k++, pick ^= 1)
                {
                    mx[1] = across ? (lx0 + lx1) / 2 : (pick ? lx0 - 1 : lx1 + 1);
                    my[1] = across ? (pick ? ly0 - 1 : ly1 + 1) : (ly0 + ly1) / 2;
                    if (mx[1] >= 2 && mx[1] <= gen->cw - 3 && my[1] >= top && my[1] <= gen->ch - 3)
                        break;
                }
                mx[0] = (ax + mx[1]) / 2;
                my[0] = (ay + my[1]) / 2;
                mx[2] = (mx[1] + bx) / 2;
                my[2] = (my[1] + by) / 2;
            }
        }
        else if (p->type == RROUTE_RIVER && gen->attempt < 20)
        {
            // By the river bank where it comes nearest the straight way,
            // so the river is in view even where the trail need not
            // cross it.
            int x, y, k, best = 9999;
            for (y = top; y < gen->ch - 1; y++)
            {
                for (x = 1; x < gen->cw - 1; x++)
                {
                    u8 c = Cell(gen, x, y);
                    int dd = 9999;
                    if (c != C_TREE && c != C_OPEN)
                        continue;
                    if (!BitU(gen->river, gen, x - 1, y) && !BitU(gen->river, gen, x + 1, y)
                     && !BitU(gen->river, gen, x, y - 1) && !BitU(gen->river, gen, x, y + 1))
                        continue;
                    for (k = 0; k <= 16; k++)
                    {
                        int px = ax + (bx - ax) * k / 16, py = ay + (by - ay) * k / 16;
                        int e = Abs(x - px) + Abs(y - py) + Abs(k - 8) / 4;
                        if (e < dd)
                            dd = e;
                    }
                    if (dd < best)
                    {
                        best = dd;
                        mx[1] = x;
                        my[1] = y;
                    }
                }
            }
            // The swings on the way there and on stay, so the trail is
            // not cut short; on later tries it heads straight there.
            if (best < 9999 && (gen->attempt & 1))
            {
                mx[0] = (ax + mx[1]) / 2;
                my[0] = (ay + my[1]) / 2;
                mx[2] = (mx[1] + bx) / 2;
                my[2] = (my[1] + by) / 2;
            }
        }
        for (i = 0; i < 3; i++)
            LandNear(gen, &mx[i], &my[i]);
        if (outDir == RDIR_NONE && !mouth)
        {
            LandNear(gen, &bx, &by);
            gen->endX = bx;
            gen->endY = by;
        }
    }
    gen->nWalk = 0;
    {
        // The walk so far is only the openings; carve on from the way in.
        u16 e0 = ey[0] * gen->cw + ex[0];
        // (v1.1 vert: a mountain pass's road round a plateau, rogue_gen_vert.h.)
        if (gen->forestCut ? !CarveForestCut(gen, ax, ay, bx, by)
                           : PlateauCarve(gen, ax, ay, bx, by, top) ? FALSE
                           : (!CarveWay(gen, ax, ay, mx[0], my[0]) || !CarveWay(gen, mx[0], my[0], mx[1], my[1])
                           || !CarveWay(gen, mx[1], my[1], mx[2], my[2]) || !CarveWay(gen, mx[2], my[2], bx, by)))
            return FALSE;
        if (guarded)
        {
            // Keep the straight way in at the start of the path.
            int k;
            u16 head[8];
            u8 d = Opposite(inDir);
            int hx = ex[0], hy = ey[0];
            for (k = 0; k < guardLen && k < 8; k++)
            {
                hx += sDirDX[d];
                hy += sDirDY[d];
                head[k] = hy * gen->cw + hx;
            }
            // Prepend the stub to the walk so loop erasing keeps it.
            if (gen->nWalk + guardLen < MAX_WALK)
            {
                for (k = gen->nWalk - 1; k >= 0; k--)
                {
                    YIELD();
                    gen->walk[k + guardLen] = gen->walk[k];
                }
                for (k = 0; k < guardLen; k++)
                {
                    YIELD();
                    gen->walk[k] = head[k];
                }
                gen->nWalk += guardLen;
            }
        }
        if (outStep)
        {
            // On down the lane to the way on, past the block.
            int k, lx, ly;
            for (k = outStep; k >= 1 && gen->nWalk < MAX_WALK; k--)
            {
                YIELD();
                LaneCell(gen, outDir, k, &lx, &ly);
                gen->walk[gen->nWalk++] = ly * gen->cw + lx;
            }
        }
        LoopEraseRoad(gen, e0, outDir != RDIR_NONE ? ey[1] * gen->cw + ex[1] : -1);
        SettleFords(gen);
    }
    if (guarded)
    {
        // The obstacle unit and the narrow way past it.
        int k;
        u8 d = Opposite(inDir);
        for (k = 1; k <= guardLen; k++)
        {
            YIELD();
            int u = gen->path[k];
            if (k >= gen->nPath || u % gen->cw != ex[0] + sDirDX[d] * k || u / gen->cw != ey[0] + sDirDY[d] * k)
                return FALSE;
        }
        gen->guardStep = guardStep;
        gen->guardEnd = guardLen;
    }
    // Branches: a road from each other opening to the nearest stretch of road.
    for (i = 0; i < p->nLinks; i++)
    {
        const struct RogueLink *l = &p->links[i];
        int k, best = -1, bestD = 9999, ix, iy, n = gen->nPath, step = GateLaneStep(gen, l);
        if (l->dir == inDir || l->dir == outDir)
            continue;
        MakeOpening(gen, l->dir, &ix, &iy);
        ends[nEnds] = l->dir;
        ex[nEnds] = gen->path[gen->nPath - 1] % gen->cw;
        ey[nEnds] = gen->path[gen->nPath - 1] / gen->cw;
        endStyle[nEnds++] = SeamWay(gen, l->target);
        // A side place's obstacle at the end of a lane in this route.
        if (step && !CarveGateLane(gen, l->dir, step, &ix, &iy))
            return FALSE;
        for (k = 0; k < n; k++)
        {
            YIELD();
            int px = gen->path[k] % gen->cw, py = gen->path[k] / gen->cw, along;
            int dd = Abs(px - ix) + Abs(py - iy);
            if (k <= gen->guardEnd + 1 && gen->guardEnd)
                continue;
            // Not back beside the lane.
            if (step && EdgeDist(gen, l->dir, px, py, &along) <= step)
                continue;
            if (dd < bestD)
            {
                bestD = dd;
                best = k;
            }
        }
        if (best < 0 || !CarveWay(gen, ix, iy, gen->path[best] % gen->cw, gen->path[best] / gen->cw))
            return FALSE;
    }
    if (tunX >= 0)
    {
        // A road from the tunnel's mouth to the nearest stretch of road.
        int k, best = -1, bestD = 9999, n = gen->nPath;
        for (k = 0; k < n; k++)
        {
            int dd = Abs(gen->path[k] % gen->cw - tunX) + Abs(gen->path[k] / gen->cw - tunY);
            if (gen->guardEnd && k <= gen->guardEnd + 1)
                continue;
            if (dd < bestD)
            {
                bestD = dd;
                best = k;
            }
        }
        if (best < 0 || !CarveWay(gen, tunX, tunY, gen->path[best] % gen->cw, gen->path[best] / gen->cw))
            return FALSE;
    }
    SettleFords(gen);
    if (nookX >= 0)
        OpenUnit(gen, nookX, nookY);
    if (guarded)
    {
        // Trees all round the narrow way in, so it cannot be walked round.
        for (i = 0; i <= gen->guardEnd + (nookX >= 0) && i < gen->nPath; i++)
        {
            YIELD();
            KeepAround(gen, gen->path[i] % gen->cw, gen->path[i] / gen->cw);
        }
    }
    if (nookX >= 0)
        KeepAround(gen, nookX, nookY);
    if (green)
    {
        // Meadows along the trail, with squeezes between them for the
        // trainers. Each layout widens the stretches its own way, and they
        // widen together until no more than about half the map is trees.
        u16 squeeze[MAX_SQUEEZE];
        u8 radius[MAX_SQUEEZE + 1];
        int nSqueeze, cap, extra, k;
        nSqueeze = PickSqueezes(gen, p->nTrainers + (p->rival ? 1 : 0) + (gen->layout == LAYOUT_BELT), gen->guardEnd ? gen->guardEnd + 2 : 3, squeeze);
        for (k = 0; k <= nSqueeze; k++)
        {
            if (p->type == RROUTE_LONGGRASS)
                radius[k] = 4 + RAND(4);
            else if (FieldLike(p) || WaterPondCountry(gen))
                radius[k] = 3 + RAND(4);
            else if (beachTrail)
                radius[k] = 1 + RAND(2);
            else if (gen->layout == LAYOUT_GLADE)
                radius[k] = RAND(3) == 0 ? 1 : 3 + RAND(3);
            else if (gen->layout == LAYOUT_BELT)
                radius[k] = 2 + RAND(2);
            else
                radius[k] = 2 + RAND(3);
        }
        if (gen->layout == LAYOUT_GROVE && p->type != RROUTE_COAST)
        {
            // A grove left standing in the middle, that the meadows go round.
            int tries2;
            for (tries2 = 0; tries2 < 30; tries2++)
            {
                int gx = 4 + RAND(gen->cw - 8), gy = top + 3 + RAND(gen->ch - top - 7), x, y, near = 0;
                int rx = 2 + RAND(2), ry = 2;
                for (i = 0; i < gen->nPath; i++)
                {
                    YIELD();
                    if (Abs(gen->path[i] % gen->cw - gx) <= rx + 1 && Abs(gen->path[i] / gen->cw - gy) <= ry + 1)
                        near++;
                }
                if (near || Cell(gen, gx, gy) != C_TREE)
                    continue;
                for (y = gy - ry; y <= gy + ry; y++)
                    for (x = gx - rx; x <= gx + rx; x++)
                        if (InEllipse(x - gx, y - gy, rx, ry) && Cell(gen, x, y) == C_TREE)
                            SetBitU(gen->keep, gen, x, y);
                break;
            }
        }
        cap = p->type == RROUTE_LONGGRASS ? 30 : WaterPondCountry(gen) ? 34 : FieldLike(p) ? 40 : p->type == RROUTE_FOREST ? 52 : 46;
        for (extra = 0; extra < 4; extra++)
            if (GrowMeadows(gen, squeeze, nSqueeze, radius, extra) <= cap)
                break;
    }
    if (p->type == RROUTE_LONGGRASS)
        LandOpenFields(gen, top);
    else
    {
        CarveSegments(gen);
    }
    if (outDir == RDIR_NONE && !mouth)
    {
        // The clearing at a dead end, where the best items wait.
        int x, y;
        for (y = gen->endY - 2; y <= gen->endY + 2; y++)
            for (x = gen->endX - 2; x <= gen->endX + 2; x++)
                if (Abs(x - gen->endX) + Abs(y - gen->endY) <= 2)
                    OpenUnit(gen, x, y);
        gen->pocketX[gen->nPockets] = gen->endX;
        gen->pocketY[gen->nPockets] = gen->endY;
        gen->nPockets++;
    }
    WaterRiverVerge(gen); // (rogue_gen_water.h)
    AddSpurs(gen, 2 + RAND(2));
    if (p->type == RROUTE_COAST && p->seaSide != RDIR_NONE)
    {
        JoinBeach(gen);
        PlaceSeaRocks(gen, 3 + RAND(4));
    }

    // Ponds with rounded shores on some routes; lake and river routes have
    // their water already.
    if ((p->type == RROUTE_FOREST || FieldLike(p) || p->type == RROUTE_WOODS || (p->type == RROUTE_LAKE && !lake))
     && CHANCE(p->type == RROUTE_LAKE ? 100 : p->type == RROUTE_FLOWERS ? 60 : 35))
        PlacePond(gen, ax, ay, top);
    if (p->type == RROUTE_LAKE && !lake)
        PlacePond(gen, ax, ay, top);
    // Pond country has more ponds beside its bridged one.
    if (p->type == RROUTE_LAKE && lake && WaterPondCountry(gen))
    {
        PlacePond(gen, ax, ay, top);
        if (CHANCE(50))
            PlacePond(gen, ax, ay, top);
    }
    // A lake route with no water at all would only be thrown away at the
    // end, when its anglers find nothing to fish: try again now, a few
    // times (the second try finds room seven times in ten; where there is
    // none, as beside an ash town, more only cost time).
    if (p->type == RROUTE_LAKE && gen->attempt < 3 && !HasWater(gen))
        return FALSE;
    if (green)
    {
        // Single trees and clumps out in the open, some beside the trail so
        // the way splits round them and joins again.
        int clumps = gen->layout == LAYOUT_BELT ? 5 + RAND(3) : gen->layout == LAYOUT_GLADE ? 3 + RAND(3) : 2 + RAND(2);
        if (p->type == RROUTE_FIELD || p->type == RROUTE_FLOWERS)
            clumps += 2;
        else if (p->type == RROUTE_LONGGRASS)
            clumps = 2 + RAND(2); // the fields stay open for the grass
        for (i = 0; i < clumps; i++)
            PlaceClump(gen, i < 2);
    }
    else if (p->type != RROUTE_FIELD || CHANCE(50))
    {
        BreakUpFields(gen);
    }
    // A mountain pass climbs: its open ground on levels, and a canyon has
    // boulders on its sand (rogue_gen_rock.h).
    // (v1.1 vert: or the road climbs onto the rock for a stretch, a plateau
    // of the mountain, rogue_gen_vert.h.)
    if (p->type == RROUTE_HILLS && !PlanPlateau(gen, ax, ay))
        PlanPass(gen, ax, ay);
    if (p->type == RROUTE_HILLS)
        PlanHillFalls(gen, ax, ay); // (v1.1 vert: a waterfall in the rock)
    else if (ShelfRoute(gen))
        PlanShelf(gen, ax, ay);
    else if (p->type == RROUTE_RIVER && FallsRoute(gen))
        PlanFalls(gen, ax, ay); // (v1.1 vert: a waterfall)
    // (v1.1 peaks) The desert tower out on the sand, before the boulders.
    if (MirageOf(gen) >= 0 && !PlaceMirageTower(gen))
        return FALSE;
    CanyonThickWalls(gen, ax, ay); // (v1.1c land2)
    PlaceCanyonBoulders(gen, ax, ay);
    PlaceAshOutcrops(gen, ax, ay);
    if (guarded)
    {
        PlaceGuard(gen);
        if (!GuardHolds(gen))
            return FALSE;
    }
    PlaceGateObstacles(gen);
    if (!GatesHold(gen, ax, ay))
        return FALSE;
    // A route feature's house and yard take the rest house's place (its
    // people let travellers rest where the plan put one).
    FeatBegin(gen, &feat);
    if (feat.lot.house != NULL && !FeatPlaceLot(gen, &feat) && !BerryFarmRefit(gen, &feat) && gen->attempt < FEAT_GIVE_UP)
        return FALSE;
    if (!feat.hasLot && (p->interiors & (1 << RINT_REST)))
        hasRest = PlaceRestHouse(gen, &rest);
    // (No ledges in the long grass: Routes 119 and 120 have none there.)
    if (green)
    {
        if (p->type != RROUTE_LONGGRASS)
            PlaceLedgeBands(gen, ax, ay, FieldLike(p) ? 3 + RAND(2) : 2 + RAND(2));
    }
    else if (p->type != RROUTE_DESERT)
        PlaceLedges(gen, ax, ay, p->type == RROUTE_HILLS ? 4 + RAND(3) : 2 + RAND(3));
    // (v1.1) A berry farm's trees are its field's: no patch by the way.
    if (p->nBerries && CHANCE(75) && !(feat.hasLot && BerryFarmLot(&feat.lot)))
        PlaceBerryPatch(gen);
    // A fenced flower bed now and then, mostly on meadows, near a town.
    if (p->type == RROUTE_FLOWERS)
        LandFlowerBeds(gen, ax, ay);
    else if (CHANCE(p->type == RROUTE_FIELD ? 30 : 10) && p->type != RROUTE_DESERT && p->type != RROUTE_HILLS
     && p->type != RROUTE_MOUNTAIN && p->type != RROUTE_WOODS)
        PlaceFenceGarden(gen, ax, ay);

    // The way into a forest off this route, in a tree line beside the way.
    if (!gen->forestCut && ForestOf(gen) >= 0 && !PlaceForestWay(gen, ax, ay))
        return FALSE;

    // Ground cover: the way in the route's style (PaintRouteWay), and tall
    // grass in patches, across the way on a field route but never over a
    // road.
    road = RouteRoadStyle(r, p - r->places);
    PaintRouteWay(gen, road, nEnds, ex, ey, endStyle);
    // (v1.1) The woods between the volcano's mountain and a way north, where
    // the opening's landing left open ground: grown again, so the
    // mountain's end meets trees (the check at the end fails a road there).
    if (VolcanoMountainAt(r, p - r->places) && LinkOnSide(p, RDIR_NORTH))
    {
        int k, y;
        for (k = 0; k < 2; k++)
            for (y = 0; y <= 5; y++)
            {
                int gx = VolcanoGapX(r, p, k);
                u8 c = Cell(gen, gx, y);
                if ((c == C_OPEN || c == C_KEEP) && !IsOccupied(gen, gx * 2, y * 2) && !IsOccupied(gen, gx * 2 + 1, y * 2)
                 && !IsOccupied(gen, gx * 2, y * 2 + 1) && !IsOccupied(gen, gx * 2 + 1, y * 2 + 1))
                    SetCell(gen, gx, y, C_TREE);
            }
    }
    for (tries = 0; tries < (p->type == RROUTE_DESERT ? 5 : 18)
                 && (tries < (p->type == RROUTE_DESERT ? 3 : 6) || gen->tallCount < gen->m->w * gen->m->h / (FieldLike(p) ? 5 : 9)); tries++)
    {
        int pi = 3 + RAND(gen->nPath > 6 ? gen->nPath - 6 : 1);
        int px = gen->path[pi] % gen->cw + (int)RAND(5) - 2;
        int py = gen->path[pi] / gen->cw + (int)RAND(5) - 2;
        if (Abs(px - ax) + Abs(py - ay) < 2 || Abs(px - bx) + Abs(py - by) < 2)
            continue;
        if (gen->guardEnd && pi <= gen->guardEnd)
            continue;
        PaintBlob(gen, px * 2 + RAND(2), py * 2 + RAND(2), 5 + RAND(8), V_TALL, FALSE);
    }
    n = road ? 0 : 2 + RAND(2);
    for (i = 0; i < n && gen->nPath > 8; i++)
    {
        int pi = 3 + (i * (gen->nPath - 6)) / n + RAND(3);
        if (pi >= gen->nPath - 3 || (gen->guardEnd && pi <= gen->guardEnd + 1))
            continue;
        PaintBlob(gen, (gen->path[pi] % gen->cw) * 2 + 1, (gen->path[pi] / gen->cw) * 2 + 1, 2 + RAND(3), V_TALL, FALSE);
    }
    PassClearRims(gen);
    PlateauFinish(gen); // (v1.1 vert)
    FallsFinish(gen);
    HillFallsFinish(gen);
    CanyonClearGrass(gen);
    MirageGround(gen, FALSE); // (v1.1 peaks)
    if (p->type == RROUTE_LONGGRASS)
        LandLongGrassFields(gen, nEnds, ex, ey);
    TidyPaths(gen);
    if (!road)
        DropPathScraps(gen);
    ClearGrassByWay(gen);
    if (p->type != RROUTE_DESERT)
        ScatterFlowers(gen, p->type == RROUTE_FLOWERS ? 24 : p->type == RROUTE_FIELD ? 14 : p->type == RROUTE_LONGGRASS ? 4 : 7);
    if (p->type == RROUTE_FLOWERS)
        LandBedMargins(gen);
    if (p->type == RROUTE_COAST)
        PaintBeach(gen);
    if (p->type == RROUTE_DESERT || gen->ashFade)
        PaintSpecialGround(gen);
    MirageGround(gen, TRUE); // (v1.1 peaks)

    // Trainers spread along the road, items tucked into corners, and a sign.
    {
        int fx, fy;
        u8 facing;
        int placedTrainers = 0, placedItems = 0, first = gen->guardEnd ? gen->guardEnd + 1 : 3;

        FloodReachable(gen, ax, ay);
        if (gen->hasBerry)
        {
            if (Reached(gen, gen->berryX / 2, gen->berryY / 2))
            {
                AddObj(gen, ROBJ_BERRY, gen->berryX + 1, gen->berryY, RDIR_SOUTH, 0, 0);
                AddObj(gen, ROBJ_BERRY, gen->berryX + 2, gen->berryY, RDIR_SOUTH, 1, 0);
            }
            else
            {
                gen->hasBerry = FALSE;
            }
        }
        // The rival waits a little way along, beside a squeeze in the road.
        if (p->rival)
        {
            bool8 placed = FALSE;
            for (tries = 0; tries < 60 && !placed; tries++)
            {
                int pi = 4 + RAND(5);
                int rx, ry;
                if (pi >= gen->nPath - 2)
                    break;
                if (!FindSpotBesideRoad(gen, gen->path[pi] % gen->cw, gen->path[pi] / gen->cw, &fx, &fy, &facing))
                    continue;
                rx = gen->path[pi] % gen->cw + sDirDX[facing];
                ry = gen->path[pi] / gen->cw + sDirDY[facing];
                if (tries < 40 && IsOpenU(Cell(gen, rx, ry)))
                    continue;
                AddObj(gen, ROBJ_RIVAL, fx, fy, facing, p->rival - 1, 4);
                placed = TRUE;
            }
            if (!placed)
                return FALSE;
        }
        for (i = 0; i < p->nTrainers; i++)
        {
            for (tries = 0; tries < 80; tries++)
            {
                int base = (gen->nPath * (i + 1)) / (p->nTrainers + 1);
                // Near their share of the road, or anywhere along it.
                int pi = tries < 40 ? base + RAND(7) - 3 : first + RAND(gen->nPath - first), ux, uy;
                if (pi < first || pi >= gen->nPath - 2)
                    continue;
                ux = gen->path[pi] % gen->cw;
                uy = gen->path[pi] / gen->cw;
                // Where the woods cut the route short, after many tries at
                // it: beside any open ground (PlaceTrainersOnWalk moves
                // them to the way through).
                if (gen->forestCut && gen->attempt >= 20 && tries >= 40)
                {
                    ux = RAND(gen->cw);
                    uy = RAND(gen->ch);
                    if (!Reached(gen, ux, uy) || !IsOpenU(Cell(gen, ux, uy)))
                        continue;
                }
                if (FindSpotBesideRoad(gen, ux, uy, &fx, &fy, &facing))
                {
                    // Prefer a squeeze: nothing open on the far side of the
                    // road, so the player has to walk through the trainer's
                    // line of sight.
                    int rx = ux + sDirDX[facing], ry = uy + sDirDY[facing];
                    if (tries < 25 && IsOpenU(Cell(gen, rx, ry)))
                        continue;
                    // (Never on the ground in front of a way into the woods.)
                    if (gen->forestCut && gen->attempt >= 20 && tries >= 40 && OnForestWayGround(gen, fx, fy))
                        continue;
                    AddObj(gen, ROBJ_TRAINER, fx, fy, facing, placedTrainers++, 4);
                    break;
                }
            }
        }
        // A road with no room for them all is thrown away now rather than
        // after the rest is built (some take dozens of tries).
        if (placedTrainers < p->nTrainers)
            return FALSE;
        // Items go at the end of the dead ends first.
        for (i = 0; i < gen->nPockets && placedItems < p->nItems; i++)
        {
            fx = gen->pocketX[i] * 2 + RAND(2);
            fy = gen->pocketY[i] * 2 + RAND(2);
            if (Reached(gen, gen->pocketX[i], gen->pocketY[i]) && !IsOccupied(gen, fx, fy) && !IsCrowded(gen, fx, fy)
             && !BlocksWay(gen, fx, fy) && !ByLedgeF(gen, fx, fy) && !SeenFromNextDoor(gen, fx, fy))
            {
                SetCover(gen, fx, fy, BareCover(gen, fx, fy));
                AddObj(gen, ROBJ_ITEM, fx, fy, RDIR_SOUTH, placedItems++, 0);
            }
        }
        for (tries = 0; tries < 80 && placedItems < p->nItems; tries++)
        {
            int pi = first + RAND(gen->nPath - first - 1 > 0 ? gen->nPath - first - 1 : 1);
            // Prefer a hidden corner, and fall back to the roadside.
            if (tries < 40 ? FindNook(gen, &fx, &fy)
                           : FindSpotBesideRoad(gen, gen->path[pi] % gen->cw, gen->path[pi] / gen->cw, &fx, &fy, &facing))
            {
                if (BlocksWay(gen, fx, fy))
                    continue;
                // Past the obstacle only.
                if (gen->guardStep)
                {
                    u8 saved = gen->g[gen->path[gen->guardStep]];
                    bool8 before;
                    gen->g[gen->path[gen->guardStep]] = C_TREE;
                    FloodReachable(gen, gen->path[1] % gen->cw, gen->path[1] / gen->cw);
                    before = Reached(gen, fx / 2, fy / 2);
                    gen->g[gen->path[gen->guardStep]] = saved;
                    FloodReachable(gen, ax, ay);
                    if (before)
                        continue;
                }
                AddObj(gen, ROBJ_ITEM, fx, fy, RDIR_SOUTH, placedItems++, 0);
            }
        }
        // LATE CAMP: a camper beside the road, halfway along.
        if (RogueGen_RouteCamp(gen->r) >= 0 && gen->m->place == RogueGen_RouteCamp(gen->r))
        {
            bool8 placed = FALSE;
            for (tries = 0; tries < 80 && !placed; tries++)
            {
                int pi = first + (gen->nPath - first) / 2 + RAND(9) - 4;
                if (pi < first || pi >= gen->nPath - 2)
                    continue;
                if (FindSpotBesideRoad(gen, gen->path[pi] % gen->cw, gen->path[pi] / gen->cw, &fx, &fy, &facing)
                 && !BlocksWay(gen, fx, fy) && !IsCrowded(gen, fx, fy))
                {
                    AddObj(gen, ROBJ_CAMP, fx, fy, RDIR_SOUTH, RogueGen_RoadCampFlag(gen->r, gen->m->place), 0);
                    placed = TRUE;
                }
            }
            // Failing that, the nearest spot to halfway anywhere along the
            // road, rather than the whole map again (up to 20 times).
            for (tries = 0; !placed && tries < 2 * gen->nPath; tries++)
            {
                int pi = first + (gen->nPath - first) / 2 + ((tries & 1) ? -(tries + 1) / 2 : tries / 2);
                if (pi < first || pi >= gen->nPath - 2)
                    continue;
                if (FindSpotBesideRoad(gen, gen->path[pi] % gen->cw, gen->path[pi] / gen->cw, &fx, &fy, &facing)
                 && !BlocksWay(gen, fx, fy) && !IsCrowded(gen, fx, fy))
                {
                    AddObj(gen, ROBJ_CAMP, fx, fy, RDIR_SOUTH, RogueGen_RoadCampFlag(gen->r, gen->m->place), 0);
                    placed = TRUE;
                }
            }
            if (!placed)
                return FALSE;
        }
        for (tries = 0; tries < 30; tries++)
        {
            int pi = 1 + RAND(3 + tries / 10);
            if (pi < gen->nPath && FindSignSpot(gen, gen->path[pi] % gen->cw, gen->path[pi] / gen->cw, &fx, &fy, &facing))
            {
                AddSign(gen, fx, fy, RSIGN_ROUTE);
                break;
            }
        }
        // The route feature's people and Pokemon, and a double battle pair.
        {
            bool8 fail;
            featQuiet = FeatPeople(gen, &feat, &fail);
            if (fail && gen->attempt < FEAT_GIVE_UP)
                return FALSE;
        }
        // Someone out for a walk who is not looking for a battle.
        if (!mountain && !featQuiet)
        {
            for (tries = 0; tries < 30; tries++)
            {
                int pi = gen->nPath / 3 + RAND(gen->nPath / 3 + 1);
                if (pi < gen->nPath && FindSpotBesideRoad(gen, gen->path[pi] % gen->cw, gen->path[pi] / gen->cw, &fx, &fy, &facing))
                {
                    // Not in tall grass, where only a red cap would show.
                    // Plain grass, not the route's sand, or they stand on a
                    // lone square of it.
                    if (Cover(gen, fx, fy) == V_TALL)
                        SetCover(gen, fx, fy, BareCover(gen, fx, fy));
                    AddObj(gen, ROBJ_VILLAGER, fx, fy, facing, RAND(8), 0);
                    break;
                }
            }
        }
        // A second sign near the far end points the way on.
        for (tries = 0; tries < 30 && gen->nPath > 8 && outDir != RDIR_NONE; tries++)
        {
            int pi = gen->nPath - 3 - RAND(4 + tries / 10);
            if (pi > 0 && FindSignSpot(gen, gen->path[pi] % gen->cw, gen->path[pi] / gen->cw, &fx, &fy, &facing))
            {
                AddSign(gen, fx, fy, RSIGN_ROUTE);
                break;
            }
        }
        // A sign beside each way into the woods names them, as Route 104's
        // does Petalburg Woods: a gap in the trees is easy to walk past.
        for (tries = 0; tries < gen->nForestWays; tries++)
        {
            int k = tries, gx, gy, j;
            ForestWayGround(gen, k, &gx, &gy);
            for (j = 0; j < 30; j++)
            {
                int px = gx + (j < 10 ? 0 : (int)RAND(3) - 1), py = gy + (j < 20 ? 0 : (int)RAND(3) - 1);
                if (FindSignSpot(gen, px, py, &fx, &fy, &facing)
                 && !((fx >> 1) == gx && (fy >> 1) == gy) && Abs((fy >> 1) - gy) <= 1 && Abs((fx >> 1) - gx) <= 2
                 && (fx >> 1) != gen->forestX[k])
                {
                    AddSign(gen, fx, fy, RSIGN_FOREST);
                    break;
                }
            }
        }
        FeatSigns(gen, &feat);
        if (placedItems < p->nItems)
            return FALSE;
    }
    FeatDoor(gen, &feat);
    if (hasRest)
    {
        // Its door, and the house map behind it.
        const struct Building *b = rest.b;
        int dx = rest.bx * 2 + b->doorX, dy = rest.by * 2 + b->sy + b->doorY;
        int fy = rest.by * 2 + b->sy + FrontY(b);
        AddWarp(gen, dx, dy, RWARP_HOUSE);
        gen->m->interiors[gen->m->nHouses++] = RINT_REST;
        if (InMap(gen, dx + 1, fy) && !IsOccupied(gen, dx + 1, fy) && Cell(gen, (dx + 1) >> 1, fy >> 1) != C_BLDG
         && !ByLedgeF(gen, dx + 1, fy)
         && WalkableCover(gen, dx + 1, fy))
            AddSign(gen, dx + 1, fy, RSIGN_REST);
        SetCover(gen, dx, fy, Cover(gen, dx, fy) == V_TALL ? V_NONE : Cover(gen, dx, fy));
    }

    // Ledges, gardens and the rest house come after the guard; make sure
    // none of them opened a way round it.
    if ((guarded && !GuardHolds(gen)) || !GatesHold(gen, ax, ay))
        return FALSE;
    // (Not where the water must stay in view of the shortest walk, which
    // the new trees could turn away from it.)
    if (!mountain && !gen->rocky && !gen->woods && p->type != RROUTE_SEA && p->type != RROUTE_COAST
     && p->type != RROUTE_RIVER)
        ThickenSeamTrees(gen);
    ChooseAshTrees(gen);
    // A rocky route draws its rock by levels, and the mesa only where its
    // units are still rock: a road cut through it would leave a piece of
    // the mesa drawn on its own that does not join the rest.
    if ((mountain || gen->rocky) && gen->hasMesa && !MesaWhole(gen))
        return FALSE;
    WaterClearBridgeEnds(gen); // (rogue_gen_water.h)
    TidyGround(gen);
    if (mountain || gen->rocky)
    {
        RenderMountain(gen);
        RenderHillFalls(gen); // (v1.1 vert)
    }
    else
    {
        Render(gen);
        WaterRender(gen);
        RenderFalls(gen); // (v1.1 vert: a waterfall)
        // (v1.1 vert: a raised shelf's rims, rogue_gen_vert.h.)
        if (gen->terraced)
        {
            RenderPassRims(gen, NULL);
            RockRepair(gen);
        }
    }
    if (hasRest)
        Stamp(gen, rest.b, rest.bx, rest.by);
    FeatRender(gen, &feat);
    RenderSigns(gen);
    RenderForestWays(gen);
    // (The walk the player takes may pass the feature's door: nobody
    // stands there, but people walk by it.)
    FeatWalkBy(gen, &feat, FALSE);
    PlaceTrainersOnWalk(gen, inDir, outDir);
    FeatWalkBy(gen, &feat, TRUE);
    // (Not where there is no water to stand by, which no try will change.)
    if (gen->wetMiss && gen->attempt < 8 && HasWater(gen))
        return FALSE;
    if (IsVictoryRoad(gen) && !PlaceVictoryCamp(gen))
        return FALSE;
    // A river, or a coast's sea, must come on screen from the shortest
    // walk through.
    if ((p->type == RROUTE_RIVER || (p->type == RROUTE_COAST && p->seaSide != RDIR_NONE))
     && gen->nWalk && gen->attempt < 30 && !WaterInViewBothWays(gen))
        return FALSE;
    // The rival must not be walked past: if their spot does not cut the
    // way through, try the map again.
    // A longer look across the open ground can close the gap first.
    {
        int i;
        for (i = 0; i < gen->m->nObjs; i++)
        {
            struct RogueObj *o = &gen->m->objs[i];
            int n;
            if (o->kind != ROBJ_RIVAL || gen->nWalk < 10 || RivalBlocksWay(gen, o, o->range))
                continue;
            for (n = o->range + 1; n <= 8; n++)
            {
                int sx = o->x + sDirDX[o->facing] * n, sy = o->y + sDirDY[o->facing] * n;
                // (Never up beside Victory Road's camper: the rival
                // walking up there would close the way with him.)
                if (!StandableBlock(gen, sx, sy) || ObjectAt(gen, sx, sy) || NearCamp(gen, sx, sy))
                    break;
                // (Nor up to a trainer's line: two who walk up from lines
                // that meet could close the way between them.)
                if (NearTrainerLine(gen, sx, sy))
                    break;
                // (Nor up to a log bridge's step.)
                if (WaterLineNearStep(gen, o->x, o->y, 1 << o->facing, n))
                    break;
                if (RivalBlocksWay(gen, o, n))
                {
                    o->range = n;
                    break;
                }
            }
            if (!RivalBlocksWay(gen, o, o->range))
                return FALSE;
        }
    }
    // And on the drawn blocks, where a pond can come out narrower than its
    // cells.
    if ((guarded && !GuardHoldsOnBlocks(gen)) || !GatesHoldOnBlocks(gen))
        return FALSE;
    if (gen->nForestWays && !ForestWaysClear(gen, ax, ay))
        return FALSE;
    if (tunX >= 0 && !TunnelMouthJoined(gen))
        return FALSE;
    // (v1.1) The woods between the volcano's mountain and a way north stood.
    if (VolcanoMountainAt(r, p - r->places) && LinkOnSide(p, RDIR_NORTH))
    {
        int k, y;
        for (k = 0; k < 2; k++)
            for (y = 0; y <= 5; y++)
                if (Cell(gen, VolcanoGapX(r, p, k), y) != C_TREE)
                    return FALSE;
        // (And drawn as trees: a lot laid after, a rest house's, may not
        // stand under them.)
        for (k = 0; k < 4; k++)
            for (y = 0; y < 12; y++)
            {
                int gx = VolcanoGapX(r, p, k >> 1) * 2 + (k & 1);
                u16 t = gen->m->blocks[y * gen->m->w + gx] & 0x3FF;
                // (Round trees, or the deep woods' small ones.)
                if ((t < 0x1C6 || t > 0x1F5) && t != 0x0C6 && t != 0x0C7 && t != 0x00E && t != 0x00F
                 && t != 0x016 && t != 0x017)
                    return FALSE;
            }
    }
    if (!WaterObjsOffBridges(gen) && gen->attempt < 30)
        return FALSE;
    WaterFinish(gen);
    return TRUE;
}

// ---------------------------------------------------------------------------
// Cave
//
// A cave is a few chambers joined by tunnels on the coarse grid: C_ROAD is
// floor, C_TREE solid rock and C_WATER an underground pool. Rock is drawn as
// Granite Cave and Victory Road draw it: the lower floor, rock tops, and wall
// edges stacked two deep around every chamber, so the walls stand up from the
// floor. Each cave has its own size (SizePlace), its way in and way out where
// the chambers fall, and one thing of its own: a pool, rock pillars, a sandy
// chamber or a field of rubble.

#define B_CAVE_LIP     0x3357 // floor with the pool to its north
#define B_CAVE_WATER   0x127B
#define B_CAVE_RUBBLE  0x0202
#define B_CAVE_RUBBLE2 0x0203
// How many rings of wall edge stand around the floor before the flat rock top.
#define CAVE_LEVELS 2
// Main and side chambers of one cave, at most.
#define CAVE_ROOMS 10

enum { CAVE_PLAIN, CAVE_POOL, CAVE_PILLARS, CAVE_SAND, CAVE_RUBBLE, CAVE_LAVA };

// Dungeon floors drawn in another vanilla cave's look, with its own things
// to do: Shoal Cave's ice room (src/rogue_gen_ice.h) and Fiery Path
// (src/rogue_gen_volcano.h). A place's own cave map is always CSTYLE_CAVE.
enum { CSTYLE_CAVE, CSTYLE_ICE, CSTYLE_FIERY };
static u8 CaveStyle(struct Gen *gen);
static u16 StyledCaveBlock(struct Gen *gen, u8 style, int fx, int fy, u8 l, const bool8 *lower, const u8 *lev);
static void StyledLadder(struct Gen *gen, u8 style, int fx, int fy, bool8 up);
static u8 StyledFeature(struct Gen *gen, u8 feature);
static u16 StyledRock(struct Gen *gen, u16 block);
static u16 StyledSolidRock(struct Gen *gen, u16 block);
static void StyledRooms(struct Gen *gen, const struct Rect *rooms, int nRooms, int nMain, int best, int inX, int inY);
static bool8 LavaLake(struct Gen *gen, const struct Rect *r, int sx, int sy);
static bool8 StyledFinish(struct Gen *gen, const struct Rect *rooms, int nRooms, int nMain, int best, int inX, int inY,
                          int farX, int farY);
static bool8 PlaceFieryDoor(struct Gen *gen, const struct Rect *r, u8 kind, int *lx, int *ly);

static bool8 IsFloorF(struct Gen *gen, int fx, int fy)
{
    return IsTypeF(gen, fx, fy, C_ROAD, FALSE);
}

// Floor and water: the open ground that walls stand around.
static bool8 IsCaveOpenF(struct Gen *gen, int fx, int fy)
{
    u8 c;
    if (!InMap(gen, fx, fy))
        return FALSE;
    c = Cell(gen, fx >> 1, fy >> 1);
    return c == C_ROAD || c == C_WATER;
}

static bool8 IsPoolF(struct Gen *gen, int fx, int fy)
{
    return IsTypeF(gen, fx, fy, C_WATER, FALSE) && !BittenF(gen, fx, fy, C_WATER);
}

// A wall block, by which of its eight neighbours stand lower than it. The
// pieces and the rules are Granite Cave's and Victory Road's.
static u16 CaveWallPiece(bool8 n, bool8 s, bool8 e, bool8 w, bool8 ne, bool8 nw, bool8 se, bool8 sw)
{
    if (n && s)  return 0x0619;
    if (e && w)  return 0x0612;
    if (n && w)  return sw ? 0x0608 : 0x0620;
    if (n && e)  return se ? 0x060A : 0x0622;
    if (s && w)  return 0x0618;
    if (s && e)  return 0x061A;
    if (n)       return 0x0609;
    if (s)       return 0x0619;
    if (w)       return 0x0610;
    if (e)       return 0x0612;
    if (se)      return 0x061B;
    if (sw)      return 0x061C;
    if (ne || nw) return 0x0623;
    return B_CAVE_SOLID;
}

// A block of the pool, with banks against the floor and rock around it.
static u16 CavePoolBlock(struct Gen *gen, int fx, int fy)
{
    bool8 n = !IsPoolF(gen, fx, fy - 1), e = !IsPoolF(gen, fx + 1, fy), w = !IsPoolF(gen, fx - 1, fy);
    bool8 ne = !IsPoolF(gen, fx + 1, fy - 1), nw = !IsPoolF(gen, fx - 1, fy - 1);
    if (n && e) return 0x1181;
    if (n && w) return 0x1180;
    if (n)      return 0x1189;
    if (e)      return ne ? 0x1192 : 0x1178;
    if (w)      return nw ? 0x1190 : 0x1179;
    if (ne)     return 0x119A;
    if (nw)     return 0x1198;
    return B_CAVE_WATER;
}

static void RenderCave(struct Gen *gen)
{
    // How far each block lies from open ground, at most CAVE_LEVELS + 1,
    // kept in the search queue, which is free while rendering.
    u8 *lev = (u8 *)sBfsQueue;
    int w = gen->m->w, h = gen->m->h, fx, fy, i, dx, dy;
    u8 l, style = CaveStyle(gen);
    const struct RogueWarp *door = NULL;

    for (i = 0; i < w * h; i++)
    {
        YIELD();
        lev[i] = IsCaveOpenF(gen, i % w, i / w) ? 0 : CAVE_LEVELS + 1;
    }
    // Each ring of rock is one step further from the open ground.
    for (l = 1; l <= CAVE_LEVELS; l++)
    {
        YIELD();
        for (fy = 0; fy < h; fy++)
        {
            YIELD();
            for (fx = 0; fx < w; fx++)
            {
                YIELD();
                u8 *here = &lev[fy * w + fx];
                if (*here <= l)
                    continue;
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++)
                        if (fx + dx >= 0 && fy + dy >= 0 && fx + dx < w && fy + dy < h
                         && lev[(fy + dy) * w + fx + dx] == l - 1)
                            *here = l;
            }
        }
    }
    for (fy = 0; fy < h; fy++)
    {
        YIELD();
        for (fx = 0; fx < w; fx++)
        {
            YIELD();
            u8 l = lev[fy * w + fx];
            u16 block;
            if (l == 0 && style != CSTYLE_CAVE)
            {
                block = StyledCaveBlock(gen, style, fx, fy, 0, NULL, lev);
            }
            else if (l == 0)
            {
                if (IsPoolF(gen, fx, fy))
                    block = CavePoolBlock(gen, fx, fy);
                else if (IsPoolF(gen, fx, fy - 1))
                    block = B_CAVE_LIP;
                else if (Cover(gen, fx, fy) == V_SAND)
                    block = B_CAVE_FLOOR; // the smooth floor (CAVE_SAND)
                else
                    block = B_CAVE_FLOOR2;
            }
            else
            {
                bool8 lower[9];
                for (i = 0; i < 9; i++)
                {
                    int nx = fx + (i % 3) - 1, ny = fy + (i / 3) - 1;
                    // Beyond the map the rock goes on, as high as it gets.
                    u8 nl = (nx < 0 || ny < 0 || nx >= w || ny >= h) ? CAVE_LEVELS + 1 : lev[ny * w + nx];
                    lower[i] = nl < l;
                }
                if (style != CSTYLE_CAVE)
                    block = StyledCaveBlock(gen, style, fx, fy, l, lower, lev);
                else
                    block = CaveWallPiece(lower[1], lower[7], lower[5], lower[3], lower[2], lower[0], lower[8], lower[6]);
            }
            Put(gen, fx, fy, block);
        }
    }
    // Ladders up and down.
    for (i = 0; i < gen->m->nWarps; i++)
    {
        const struct RogueWarp *wp = &gen->m->warps[i];
        if (style != CSTYLE_CAVE)
            StyledLadder(gen, style, wp->x, wp->y, wp->kind == RFLOOR_UP || wp->kind == RFLOOR_UP_2 || wp->kind == RFLOOR_EXIT);
        else if (sGenFloor ? (wp->kind == RFLOOR_UP || wp->kind == RFLOOR_UP_2 || wp->kind == RFLOOR_EXIT) : FALSE)
            Put(gen, wp->x, wp->y, B_LADDER_UP);
        else if (sGenFloor ? (wp->kind == RFLOOR_DOWN || wp->kind == RFLOOR_DOWN_2) : (wp->kind == RCAVE_DOWN || wp->kind == RCAVE_DOWN_2))
            Put(gen, wp->x, wp->y, B_LADDER_DOWN);
    }
    if (sGenFloor)
        return;
    // The way in: a lit mat at the foot of the chamber's south wall.
    Put(gen, gen->caveMatX, gen->caveMatY, B_CAVE_MAT);
    Put(gen, gen->caveMatX - 1, gen->caveMatY + 1, B_CAVE_LIGHT_L);
    Put(gen, gen->caveMatX, gen->caveMatY + 1, B_CAVE_LIGHT);
    Put(gen, gen->caveMatX + 1, gen->caveMatY + 1, B_CAVE_LIGHT_R);
    // The way out: a dark doorway in a north wall, in its frame.
    for (i = 0; i < gen->m->nWarps; i++)
        if (gen->m->warps[i].kind == RCAVE_NORTH)
            door = &gen->m->warps[i];
    if (door)
    {
        Put(gen, door->x - 1, door->y - 1, 0x060B);
        Put(gen, door->x, door->y - 1, B_CAVE_DOOR_TOP);
        Put(gen, door->x + 1, door->y - 1, 0x060D);
        Put(gen, door->x - 1, door->y, 0x0613);
        Put(gen, door->x, door->y, B_CAVE_DOOR);
        Put(gen, door->x + 1, door->y, 0x0615);
    }
}

static void CarveRoom(struct Gen *gen, const struct Rect *r)
{
    int x, y;
    for (y = r->y; y < r->y + r->h; y++)
        for (x = r->x; x < r->x + r->w; x++)
            SetCell(gen, x, y, C_ROAD);
}

// True if anything stands on one of a unit's blocks.
static bool8 CellTaken(struct Gen *gen, int x, int y)
{
    return IsOccupied(gen, x * 2, y * 2) || IsOccupied(gen, x * 2 + 1, y * 2)
        || IsOccupied(gen, x * 2, y * 2 + 1) || IsOccupied(gen, x * 2 + 1, y * 2 + 1);
}

// Each map's chambers have one shape, so the floors of a dungeon differ:
// plain halls, halls rounded off at the corners, or halls with bays in
// their walls.
enum { CHAMBER_PLAIN, CHAMBER_ROUND, CHAMBER_BAYS, CHAMBER_SHAPES };

static void CarveChamber(struct Gen *gen, const struct Rect *r, u8 shape)
{
    int k;
    if (shape == CHAMBER_ROUND && r->w >= 3 && r->h >= 3)
    {
        // The corners stay rock (where nothing was dug before); tunnels
        // join the middle.
        u8 was[4];
        for (k = 0; k < 4; k++)
            was[k] = Cell(gen, r->x + (k & 1) * (r->w - 1), r->y + (k >> 1) * (r->h - 1));
        CarveRoom(gen, r);
        for (k = 0; k < 4; k++)
            SetCell(gen, r->x + (k & 1) * (r->w - 1), r->y + (k >> 1) * (r->h - 1), was[k]);
        return;
    }
    CarveRoom(gen, r);
    if (shape == CHAMBER_BAYS)
    {
        // A bay or two a unit deep out of the walls, off the outer ring.
        int n = 1 + RAND(2);
        for (k = 0; k < n; k++)
        {
            u8 d = RAND(4);
            int len = 1 + RAND(2), i;
            bool8 across = (d == RDIR_NORTH || d == RDIR_SOUTH);
            int pos = across ? r->x + RAND(r->w) : r->y + RAND(r->h);
            for (i = 0; i < len; i++)
            {
                int x = across ? pos + i : (d == RDIR_WEST ? r->x - 1 : r->x + r->w);
                int y = across ? (d == RDIR_NORTH ? r->y - 1 : r->y + r->h) : pos + i;
                if (x >= 2 && y >= 2 && x <= gen->cw - 3 && y <= gen->ch - 3
                 && (across ? x < r->x + r->w : y < r->y + r->h))
                    SetCell(gen, x, y, C_ROAD);
            }
        }
    }
}

// A tunnel between two cells, one or two units wide, with one bend: across
// first, or up or down first.
static void CarveTunnelBend(struct Gen *gen, int x0, int y0, int x1, int y1, bool8 wide, bool8 horizontalFirst)
{
    int x = x0, y = y0, k;
    for (k = 0; k < 2; k++)
    {
        bool8 across = (k == 0) == horizontalFirst;
        int tx = across ? x1 : x, ty = across ? y : y1;
        for (;;)
        {
            SetCell(gen, x, y, C_ROAD);
            if (wide)
            {
                // The second lane runs beside the first, kept off the outer ring.
                int wx = across ? x : (x + 1 < gen->cw - 1 ? x + 1 : x - 1);
                int wy = across ? (y + 1 < gen->ch - 1 ? y + 1 : y - 1) : y;
                SetCell(gen, wx, wy, C_ROAD);
            }
            if (x == tx && y == ty)
                break;
            x += (tx > x) - (tx < x);
            y += (ty > y) - (ty < y);
        }
    }
}

static void CarveTunnel(struct Gen *gen, int x0, int y0, int x1, int y1, bool8 wide)
{
    bool8 horizontalFirst = RAND(2);
    CarveTunnelBend(gen, x0, y0, x1, y1, wide, horizontalFirst);
}

// True if (x, y) has open floor all around it and nothing placed within two
// cells. Anything put on such a cell can always be walked around.
static bool8 IsOpenCaveSpot(struct Gen *gen, int x, int y)
{
    int dx, dy;
    for (dy = -2; dy <= 2; dy++)
    {
        for (dx = -2; dx <= 2; dx++)
        {
            int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= gen->m->w || ny >= gen->m->h)
                return FALSE;
            if (dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1 && (!IsFloorF(gen, nx, ny) || IsPoolF(gen, nx, ny - 1)))
                return FALSE;
            if (IsOccupied(gen, nx, ny))
                return FALSE;
        }
    }
    return TRUE;
}

// A free floor cell inside a room that cannot block the way.
static bool8 FindCaveSpot(struct Gen *gen, const struct Rect *r, int *fx, int *fy)
{
    int tries;
    for (tries = 0; tries < 30; tries++)
    {
        int x = (r->x * 2) + RAND(r->w * 2);
        int y = (r->y * 2) + RAND(r->h * 2);
        if (!IsOpenCaveSpot(gen, x, y))
            continue;
        *fx = x;
        *fy = y;
        return TRUE;
    }
    return FALSE;
}

// A room of the given size around (cx, cy), kept inside the outer ring of rock.
static void CaveRoomAt(struct Gen *gen, struct Rect *r, int cx, int cy, int w, int h)
{
    int x = cx - w / 2, y = cy - h / 2;
    if (w > gen->cw - 2)
        w = gen->cw - 2;
    if (h > gen->ch - 2)
        h = gen->ch - 2;
    if (x < 1) x = 1;
    if (y < 1) y = 1;
    if (x + w > gen->cw - 1) x = gen->cw - 1 - w;
    if (y + h > gen->ch - 1) y = gen->ch - 1 - h;
    r->x = x;
    r->y = y;
    r->w = w;
    r->h = h;
}

// The cave's own feature in a chamber, if it fits without cutting the way:
// everything joined to (sx, sy) stays joined. Nothing goes where something
// stands already.
static void CaveFeature(struct Gen *gen, u8 kind, const struct Rect *r, int sx, int sy)
{
    int x, y, before = FloodReachable(gen, sx, sy);
    if (kind == CAVE_LAVA)
        LavaLake(gen, r, sx, sy);
    else if (kind == CAVE_POOL && r->w >= 4 && r->h >= 3)
    {
        // An underground pool against the chamber's north wall, a blob of
        // overlapping ovals in whole units (its corners are rounded off when
        // drawn), with floor below it to fish from. Its bottom row of the
        // chamber always stays floor.
        int k, n, attempt;
        for (attempt = 0; attempt < 6; attempt++)
        {
            int rx = r->w >= 5 ? 2 + RAND(r->w >= 7 ? 2 : 1) : 1;
            int ry = 1 + RAND((r->h - 1) / 2 > 2 ? 2 : ((r->h - 1) / 2 > 1 ? (r->h - 1) / 2 : 1));
            int lobe, cx = r->x + rx + RAND(r->w - 2 * rx > 0 ? r->w - 2 * rx : 1), cy = r->y + ry - (int)RAND(2);
            n = 0;
            for (lobe = 0; lobe < 3; lobe++)
            {
                int ox = lobe ? (int)RAND(2 * rx + 1) - rx : 0, oy = lobe ? (int)RAND(ry + 1) : 0;
                int lrx = lobe ? 1 + RAND(rx) : rx, lry = lobe ? 1 + RAND(ry) : ry;
                for (y = cy + oy - lry; y <= cy + oy + lry; y++)
                {
                    for (x = cx + ox - lrx; x <= cx + ox + lrx; x++)
                    {
                        if (x < r->x || x >= r->x + r->w || y < r->y || y > r->y + r->h - 2)
                            continue;
                        if (Cell(gen, x, y) == C_ROAD && InEllipse(x - cx - ox, y - cy - oy, lrx, lry) && !CellTaken(gen, x, y))
                        {
                            SetCell(gen, x, y, C_WATER);
                            n++;
                        }
                    }
                }
            }
            // No arms one unit wide: trim water with one water neighbour.
            for (k = 0; k < 4; k++)
            {
                for (y = r->y; y < r->y + r->h; y++)
                {
                    for (x = r->x; x < r->x + r->w; x++)
                    {
                        int nb;
                        if (Cell(gen, x, y) != C_WATER)
                            continue;
                        nb = (Cell(gen, x + 1, y) == C_WATER) + (Cell(gen, x - 1, y) == C_WATER)
                           + (Cell(gen, x, y + 1) == C_WATER) + (Cell(gen, x, y - 1) == C_WATER);
                        if (nb <= 1 && n > 4)
                        {
                            SetCell(gen, x, y, C_ROAD);
                            n--;
                        }
                    }
                }
            }
            if (n >= (r->w >= 5 ? 6 : 3) && FloodReachable(gen, sx, sy) == before - n)
                break;
            for (y = r->y; y < r->y + r->h; y++)
                for (x = r->x; x < r->x + r->w; x++)
                    if (Cell(gen, x, y) == C_WATER)
                        SetCell(gen, x, y, C_ROAD);
        }
    }
    else if (kind == CAVE_PILLARS)
    {
        // Rock pillars standing in the chamber, each with floor all round.
        // A volcano's are two units square: Fiery Path's rock has no
        // pieces for a knob one unit across (v1.1).
        int tries, n = 0, sz = CaveStyle(gen) == CSTYLE_FIERY ? 2 : 1, want = 1 + (r->w * r->h) / (8 * sz * sz);
        for (tries = 0; tries < 30 && n < want * sz * sz; tries++)
        {
            int dx, dy;
            bool8 ok = TRUE;
            x = r->x + 1 + RAND(r->w - 1 - sz > 0 ? r->w - 1 - sz : 1);
            y = r->y + 1 + RAND(r->h - 1 - sz > 0 ? r->h - 1 - sz : 1);
            for (dy = -1; dy <= sz && ok; dy++)
                for (dx = -1; dx <= sz && ok; dx++)
                    if (Cell(gen, x + dx, y + dy) != C_ROAD || (dx >= 0 && dy >= 0 && dx < sz && dy < sz && CellTaken(gen, x + dx, y + dy)))
                        ok = FALSE;
            if (!ok)
                continue;
            for (dy = 0; dy < sz; dy++)
                for (dx = 0; dx < sz; dx++)
                    SetCell(gen, x + dx, y + dy, C_TREE);
            if (FloodReachable(gen, sx, sy) != before - n - sz * sz)
            {
                for (dy = 0; dy < sz; dy++)
                    for (dx = 0; dx < sz; dx++)
                        SetCell(gen, x + dx, y + dy, C_ROAD);
                continue;
            }
            n += sz * sz;
        }
    }
    else if (kind == CAVE_SAND)
    {
        // A chamber of smooth floor among the rough, as in Granite Cave
        // and Altering Cave: the units nearest the chamber's middle, out
        // to the walls, with a ragged edge toward the rough floor beyond.
        u16 *queue = sBfsQueue;
        int head = 0, tail = 0, want = r->w * r->h + 4, d;
        queue[tail++] = PACK(r->x + r->w / 2, r->y + r->h / 2);
        while (head < tail && head < want)
        {
            int cx = PX(queue[head]), cy = PY(queue[head]);
            head++;
            for (y = cy * 2; y < cy * 2 + 2; y++)
                for (x = cx * 2; x < cx * 2 + 2; x++)
                    SetCover(gen, x, y, V_SAND);
            for (d = 0; d < 4; d++)
            {
                int nx = cx + sDirDX[d], ny = cy + sDirDY[d];
                int k;
                bool8 seen = FALSE;
                // Not out along a narrow tunnel.
                if (Cell(gen, nx, ny) != C_ROAD
                 || (Cell(gen, nx + 1, ny) == C_ROAD) + (Cell(gen, nx - 1, ny) == C_ROAD)
                  + (Cell(gen, nx, ny + 1) == C_ROAD) + (Cell(gen, nx, ny - 1) == C_ROAD) < 3)
                    continue;
                for (k = 0; k < tail && !seen; k++)
                    seen = queue[k] == PACK(nx, ny);
                if (!seen && tail < 64)
                    queue[tail++] = PACK(nx, ny);
            }
        }
        // Fray the edge a block at a time, so it never follows the units.
        for (d = 0; d < 2; d++)
        {
            int x0 = (r->x - 1) * 2, y0 = (r->y - 1) * 2;
            for (y = y0; y < y0 + (r->h + 2) * 2; y++)
                for (x = x0; x < x0 + (r->w + 2) * 2; x++)
                    if (SameCover(gen, x, y, V_SAND)
                     && (!SameCover(gen, x + 1, y, V_SAND) || !SameCover(gen, x - 1, y, V_SAND)
                      || !SameCover(gen, x, y + 1, V_SAND) || !SameCover(gen, x, y - 1, V_SAND))
                     && CHANCE(35))
                        SetCover(gen, x, y, V_NONE);
        }
    }
}

static bool8 FindLadderSpot(struct Gen *gen, const struct Rect *r, int *fx, int *fy);
static bool8 BossIsTrainer(const struct RogueDungeon *d);
static bool8 PlaceFloorExtras(struct Gen *gen, int bossX, int bossY, const struct Rect *side, int nSide,
                              const struct Rect *main, int nMain, bool8 (*open)(struct Gen *, int, int));
static void PlanDungeons(struct RogueRegion *r);
static void ShareLairAreas(struct RogueRegion *r);

// Where a dungeon's boss waits on a cave map.
enum { CBOSS_NONE, CBOSS_FAR, CBOSS_ASIDE, CBOSS_DOOR };

// What a cave map holds besides its chambers, from the dungeon it belongs to
// (CaveRoleOf in rogue_gen_cave.h). The way runs from the way in (the mat,
// or the ladder up on a floor) to the far chamber.
struct CaveRole
{
    s8 dungeon;      // the dungeon, or -1
    bool8 split;     // two parts that do not join: the way in to a ladder down, a second ladder down to the far end
    bool8 deadEnd;   // the way ends in the far chamber
    u8 far;          // the warp in the far chamber, or 0xFF
    u8 aside;        // a ladder down in a side chamber off the way, or 0xFF
    u8 boss;         // CBOSS_*
    bool8 obstacles; // boulders and cracked rocks across the way
    bool8 tunnel;    // a tunnel's way out (RFLOOR_EXIT) behind its barrier (src/rogue_gen_tunnel.h)
};

static void CaveRoleOf(struct Gen *gen, struct CaveRole *role);
static bool8 CarveTunnelEnd(struct Gen *gen, const struct Rect *rooms, int nRooms, struct Rect *t);
static bool8 FindTunnelChoke(struct Gen *gen, const struct Rect *rooms, int nRooms, const struct Rect *t, int *cx, int *cy);
static void PlaceTunnelBarrier(struct Gen *gen, int cx, int cy);
static void RenderTunnelBarrier(struct Gen *gen);
static int PlaceCaveGates(struct Gen *gen, int sx, int sy, int ex, int ey, int pairs);
static bool8 CaveLedge(struct Gen *gen, int sx, int sy, int tx, int ty, bool8 split);

// A ladder in a nook off a chamber's north wall, as Granite Cave's stand in
// bays of the rock; failing that, out on the chamber's floor. The block in
// front of it is kept clear.
static bool8 PlaceCaveLadder(struct Gen *gen, const struct Rect *r, u8 kind, int *lx, int *ly)
{
    int tries;
    if (CaveStyle(gen) == CSTYLE_FIERY)
        return PlaceFieryDoor(gen, r, kind, lx, ly);
    for (tries = 0; tries < 12; tries++)
    {
        int x = r->x + RAND(r->w), y = r->y - 1;
        if (y < 2 || x < 2 || x > gen->cw - 3)
            continue;
        if (Cell(gen, x, y) != C_TREE || Cell(gen, x - 1, y) != C_TREE || Cell(gen, x + 1, y) != C_TREE
         || Cell(gen, x, y - 1) != C_TREE || Cell(gen, x - 1, y - 1) != C_TREE || Cell(gen, x + 1, y - 1) != C_TREE
         || Cell(gen, x, y + 1) != C_ROAD || CellTaken(gen, x, y + 1))
            continue;
        SetCell(gen, x, y, C_ROAD);
        *lx = x * 2 + RAND(2);
        *ly = y * 2;
        AddWarp(gen, *lx, *ly, kind);
        MarkOccupied(gen, *lx, *ly);
        MarkOccupied(gen, *lx, *ly + 1);
        return TRUE;
    }
    if (!FindLadderSpot(gen, r, lx, ly))
        return FALSE;
    AddWarp(gen, *lx, *ly, kind);
    MarkOccupied(gen, *lx, *ly);
    MarkOccupied(gen, *lx, *ly + 1);
    return TRUE;
}

// True if the chambers a and b are not joined.
static bool8 CaveApart(struct Gen *gen, const struct Rect *a, const struct Rect *b)
{
    FloodReachable(gen, a->x + a->w / 2, a->y + a->h / 2);
    return !Reached(gen, b->x + b->w / 2, b->y + b->h / 2);
}

#define ROBJ_TRAINER_ASIDE 0x7F // a trainer left out while the others are placed

// Places count trainers from slot first on the way from (sx, sy) to
// (ex, ey), as if they were the map's only ones. The rest stay put.
static void PlaceTrainersOnPart(struct Gen *gen, int sx, int sy, int ex, int ey, int first, int count)
{
    const struct RoguePlace *p = gen->p;
    struct RogueMap *m = gen->m;
    struct RoguePlace part;
    int i;
    if (count <= 0)
        return;
    part = *p;
    part.nTrainers = count;
    for (i = 0; i < m->nObjs; i++)
    {
        if (m->objs[i].kind != ROBJ_TRAINER)
            continue;
        if (m->objs[i].slot >= first && m->objs[i].slot < first + count)
            m->objs[i].slot -= first;
        else
            m->objs[i].kind = ROBJ_TRAINER_ASIDE;
    }
    gen->p = &part;
    PlaceTrainersBetween(gen, sx, sy, ex, ey);
    gen->p = p;
    for (i = 0; i < m->nObjs; i++)
    {
        if (m->objs[i].kind == ROBJ_TRAINER)
            m->objs[i].slot += first;
        else if (m->objs[i].kind == ROBJ_TRAINER_ASIDE)
            m->objs[i].kind = ROBJ_TRAINER;
    }
}

#ifdef ROGUE_DEBUG
#define CAVE_FAIL(n) do { fprintf(stderr, "cave fail %d\n", n); return FALSE; } while (0)
#else
#define CAVE_FAIL(n) return FALSE
#endif

// A cave's hidden items, in its side chambers first, drawn with their own
// random stream so the rest of the cave stays as it was.
static void PlaceCaveHidden(struct Gen *gen, const struct Rect *rooms, int nRooms, int nMain)
{
    u32 n = RogueGen_HiddenCount(gen->r, gen->m->place), base = RogueGen_HiddenBase(gen->r, gen->m->place), k = 0;
    u32 saved = gen->rng;
    int tries, fx, fy;
    if (base >= ROGUE_MAX_HIDDEN)
        return;
    if (base + n > ROGUE_MAX_HIDDEN)
        n = ROGUE_MAX_HIDDEN - base;
    gen->rng = RogueRng_Hash(gen->r->seed, 17600 + gen->m->place);
    for (tries = 0; tries < 60 && k < n && gen->m->nSigns < ROGUE_MAX_SIGNS; tries++)
    {
        const struct Rect *r = (nRooms > nMain && tries < 40) ? &rooms[nMain + RAND(nRooms - nMain)] : &rooms[RAND(nRooms)];
        if (FindCaveSpot(gen, r, &fx, &fy))
            AddSign(gen, fx, fy, RSIGN_HIDDEN + k++);
    }
    gen->rng = saved;
}

static bool8 GenCave(struct Gen *gen)
{
    struct Rect rooms[CAVE_ROOMS];
    u8 bend[CAVE_ROOMS]; // the way each tunnel between main chambers turns
    struct CaveRole role;
    const struct RogueDungeon *d;
    int nRooms = 0, nMain, i, fx, fy, trainers = 0, items = 0, tries, best, ex, ey, xx, yy, doorY = 0;
    int split = 0;  // split: rooms[0] to rooms[split - 1] are the near part
    int aside = 0;  // the side chamber with the ladder or boss set aside
    int nPlain = gen->p->nTrainers, nNear;
    int inX, inY, farX, farY; // the ends of the way, in blocks
    int cutX[2] = {0}, cutY[2] = {0}; // split: in front of the ladder down and of the second one
    int bossX = -1, bossY = 0; // the boss, for the prize beside it
    struct Rect tunnelEnd;     // a tunnel's way out, and the passage unit its barrier stands on
    int chokeX = 0, chokeY = 0;
    bool8 wideMap = gen->cw > gen->ch;
    u8 feature, shape;

    CaveRoleOf(gen, &role);
    d = role.dungeon >= 0 ? &gen->r->dungeons[role.dungeon] : NULL;
    if (!sGenFloor && d != NULL)
        gen->m->dungeon = role.dungeon;

    // Everything starts as solid rock.
    memset(gen->g, C_TREE, sizeof(gen->g));

    // Where the way in and the far end lie: across the long side of the map,
    // from one third of it to another.
    {
        int a = RAND(3), b = (a + 1 + RAND(2)) % 3;
        if (wideMap)
        {
            // In from the west or east, out on the other side.
            bool8 west = RAND(2);
            xx = west ? 3 : gen->cw - 4;
            ex = west ? gen->cw - 4 : 3;
            yy = 3 + (a * (gen->ch - 6)) / 2;
            ey = 3 + (b * (gen->ch - 6)) / 2;
            if (yy < gen->ch / 2 && ey < gen->ch / 2)
                yy = gen->ch - 4;
        }
        else
        {
            // In from the south, out near the top.
            yy = gen->ch - 4;
            ey = 3;
            xx = 3 + (a * (gen->cw - 6)) / 2;
            ex = 3 + (b * (gen->cw - 6)) / 2;
        }
    }

    // Main chambers from the way in to the far end, wandering from side to
    // side, joined in order. A split map leaves one join out: the way from
    // the near part to the far part goes round by the floor below.
    nMain = 3 + RAND(2) + (gen->cw * gen->ch >= 360) + role.split;
    if (nMain > CAVE_ROOMS - 4)
        nMain = CAVE_ROOMS - 4;
    feature = 1 + RAND(4);
    // Dark caves have no pool to see by.
    if (gen->p->dark && feature == CAVE_POOL)
        feature = CAVE_PILLARS;
    feature = StyledFeature(gen, feature);
    shape = RAND(CHAMBER_SHAPES);
    best = 1 + RAND(nMain - 2); // the chamber that holds the feature
    // A split map is cut between two chambers where no chamber of one part
    // comes within a unit of one of the other; the chambers are laid out
    // again a few times until there is such a cut.
    for (tries = 0; tries < (role.split ? 4 : 1); tries++)
    {
        for (i = 0; i < nMain; i++)
        {
            struct Rect *r = &rooms[i];
            int cx = xx + ((ex - xx) * i) / (nMain - 1), cy = yy + ((ey - yy) * i) / (nMain - 1);
            int w = 2 + RAND(3), h = 2 + RAND(2);
            if (i > 0 && i < nMain - 1)
            {
                // Swing away from the straight line.
                int swing = (wideMap ? gen->ch : gen->cw) / 3;
                int off = (int)RAND(swing * 2 + 1) - swing;
                if (wideMap)
                    cy += off;
                else
                    cx += off;
            }
            if (i == best && feature != CAVE_RUBBLE)
            {
                w = 5 + RAND(2);
                h = 4 + RAND(2);
            }
            CaveRoomAt(gen, r, cx, cy, w, h);
        }
        if (role.split)
        {
            int from = 2 + RAND(nMain - 3), n;
            for (n = 0, split = 0; n < nMain - 3 && split == 0; n++)
            {
                int s = 2 + (from - 2 + n) % (nMain - 3), a, b;
                bool8 apart = TRUE;
                for (a = 0; a < s && apart; a++)
                    for (b = s; b < nMain && apart; b++)
                        if (rooms[a].x <= rooms[b].x + rooms[b].w && rooms[b].x <= rooms[a].x + rooms[a].w
                         && rooms[a].y <= rooms[b].y + rooms[b].h && rooms[b].y <= rooms[a].y + rooms[a].h)
                            apart = FALSE;
                if (apart)
                    split = s;
            }
            if (split != 0)
                break;
        }
    }
    if (role.split && split == 0)
        CAVE_FAIL(1);
    nRooms = nMain;
    for (i = 0; i < nMain; i++)
        CarveChamber(gen, &rooms[i], i == best ? CHAMBER_PLAIN : shape);
    // Tunnels join the chambers in order. On a split map each is dug where it
    // keeps the parts apart: with its bend one way, or else the other.
    for (i = 0; i + 1 < nMain; i++)
    {
        bool8 wide = CHANCE(30), first = RAND(2);
        bend[i] = first;
        int x0 = rooms[i].x + rooms[i].w / 2, y0 = rooms[i].y + rooms[i].h / 2;
        int x1 = rooms[i + 1].x + rooms[i + 1].w / 2, y1 = rooms[i + 1].y + rooms[i + 1].h / 2;
        int k;
        if (i + 1 == split)
            continue;
        for (k = 0; k < 2; k++)
        {
            if (role.split)
                memcpy(gen->cover, gen->g, gen->cw * gen->ch);
            CarveTunnelBend(gen, x0, y0, x1, y1, wide, first ^ k);
            bend[i] = first ^ k;
            if (!role.split || CaveApart(gen, &rooms[0], &rooms[split]))
                break;
            memcpy(gen->g, gen->cover, gen->cw * gen->ch);
        }
        if (k == 2)
            CAVE_FAIL(2);
    }
    // A loop: a second, wide tunnel between two chambers, bent the other
    // way, so a ledge can cut across one side of it (CaveLedge). Victory
    // Road's 1F has one most times; its floors below keep their tunnels
    // single more often, for the boulders and rocks (PlaceCaveGates). An
    // ice cave has none: it would lead round the ice.
    if (CHANCE(d == NULL || d->kind != RDUN_VICTORY ? 25 : role.obstacles ? 35 : 80) && CaveStyle(gen) != CSTYLE_ICE)
    {
        int j = RAND(nMain - 1), k;
        for (k = 0; k < nMain - 1; k++, j = (j + 1) % (nMain - 1))
        {
            int x0 = rooms[j].x + rooms[j].w / 2, y0 = rooms[j].y + rooms[j].h / 2;
            int x1 = rooms[j + 1].x + rooms[j + 1].w / 2, y1 = rooms[j + 1].y + rooms[j + 1].h / 2;
            if (j + 1 == split || Abs(x1 - x0) < 3 || Abs(y1 - y0) < 4)
                continue;
            if (role.split)
                memcpy(gen->cover, gen->g, gen->cw * gen->ch);
            CarveTunnelBend(gen, x0, y0, x1, y1, TRUE, !bend[j]);
            if (!role.split || CaveApart(gen, &rooms[0], &rooms[split]))
                break;
            memcpy(gen->g, gen->cover, gen->cw * gen->ch);
        }
    }
    // A tunnel's way out, in the emptiest part of the map (src/rogue_gen_tunnel.h).
    if (role.tunnel && !CarveTunnelEnd(gen, rooms, nMain, &tunnelEnd))
        CAVE_FAIL(18);
    // Side chambers off the way through, for items, each in the emptiest
    // part of the map left, joined to the nearest main chamber. On a split
    // map one that would join the parts is filled in again.
    {
        int nSide = 2 + RAND(2) + (gen->cw * gen->ch >= 280);
        for (i = 0; i < nSide && nRooms < CAVE_ROOMS; i++)
        {
            struct Rect *r = &rooms[nRooms];
            const struct Rect *from = &rooms[1];
            int k, bx = 0, by = 0, bestFar = -1, j, dd, sw, sh;
            for (k = 0; k < 12; k++)
            {
                int cx = 2 + RAND(gen->cw - 4), cy = 2 + RAND(gen->ch - 4), far = 99;
                for (j = 0; j < gen->cw * gen->ch; j++)
                {
                    YIELD();
                    if (gen->g[j] != C_ROAD)
                        continue;
                    dd = Abs(j % gen->cw - cx);
                    if (Abs(j / gen->cw - cy) > dd)
                        dd = Abs(j / gen->cw - cy);
                    if (dd < far)
                        far = dd;
                }
                if (far > bestFar)
                {
                    bestFar = far;
                    bx = cx;
                    by = cy;
                }
            }
            sw = 2 + RAND(3);
            sh = 2 + RAND(2);
            CaveRoomAt(gen, r, bx, by, sw, sh);
            for (j = 1, dd = 999; j < nMain - 1; j++)
            {
                int e = Abs(rooms[j].x + rooms[j].w / 2 - bx) + Abs(rooms[j].y + rooms[j].h / 2 - by);
                if (e < dd)
                {
                    dd = e;
                    from = &rooms[j];
                }
            }
            if (role.split)
                memcpy(gen->cover, gen->g, gen->cw * gen->ch);
            CarveChamber(gen, r, shape);
            CarveTunnel(gen, r->x + r->w / 2, r->y + r->h / 2, from->x + from->w / 2, from->y + from->h / 2, FALSE);
            if (role.split && !CaveApart(gen, &rooms[0], &rooms[split]))
            {
                memcpy(gen->g, gen->cover, gen->cw * gen->ch);
                continue;
            }
            nRooms++;
        }
        if (role.split)
            memset(gen->cover, V_NONE, gen->cw * gen->ch);
    }
    aside = nRooms > nMain ? nMain : best;
    if (role.tunnel && !FindTunnelChoke(gen, rooms, nRooms, &tunnelEnd, &chokeX, &chokeY))
        CAVE_FAIL(16);

    // The way in: on a floor a ladder up in the first chamber; in the cave
    // itself a mat in the bottom row of the first chamber's floor, with rock
    // below it for the light.
    if (sGenFloor)
    {
        if (!PlaceCaveLadder(gen, &rooms[0], RFLOOR_UP, &fx, &fy))
            CAVE_FAIL(3);
        gen->caveMatX = fx;
        gen->caveMatY = fy;
        inX = fx;
        inY = fy + 1;
    }
    else
    {
        const struct Rect *r = &rooms[0];
        int y = (r->y + r->h) * 2 - 1, n = 0;
        u8 cand[16];
        for (fx = r->x * 2 + 1; fx < (r->x + r->w) * 2 - 1 && n < 16; fx++)
            if (Cell(gen, (fx - 1) >> 1, (y + 1) >> 1) == C_TREE && Cell(gen, fx >> 1, (y + 1) >> 1) == C_TREE
             && Cell(gen, (fx + 1) >> 1, (y + 1) >> 1) == C_TREE && Cell(gen, fx >> 1, y >> 1) == C_ROAD)
                cand[n++] = fx;
        if (n == 0)
            CAVE_FAIL(4);
        gen->caveMatX = cand[RAND(n)];
        gen->caveMatY = y;
        inX = gen->caveMatX;
        inY = gen->caveMatY;
    }
    // The far end: a doorway in the north wall of the last chamber, with
    // two rows of rock above for its frame; or a ladder; or nothing.
    farX = (rooms[nMain - 1].x + rooms[nMain - 1].w / 2) * 2;
    farY = (rooms[nMain - 1].y + rooms[nMain - 1].h / 2) * 2;
    if (role.far == RCAVE_NORTH && !sGenFloor)
    {
        const struct Rect *r = &rooms[nMain - 1];
        int y = r->y * 2 - 1, n = 0;
        u8 cand[16];
        // The frame stands in the wall face, clear of the chamber's corners
        // where the room allows.
        int k;
        for (k = 2; k >= 1 && n == 0; k--)
            for (fx = r->x * 2 + k; fx < (r->x + r->w) * 2 - k && n < 16; fx++)
                if (y >= 2 && Cell(gen, (fx - k) >> 1, y >> 1) == C_TREE && Cell(gen, fx >> 1, y >> 1) == C_TREE
                 && Cell(gen, (fx + k) >> 1, y >> 1) == C_TREE && Cell(gen, fx >> 1, (y + 1) >> 1) == C_ROAD
                 && Cell(gen, (fx - 1) >> 1, (y + 1) >> 1) == C_ROAD && Cell(gen, (fx + 1) >> 1, (y + 1) >> 1) == C_ROAD)
                    cand[n++] = fx;
        if (n == 0)
            CAVE_FAIL(5);
        gen->caveDoorX = cand[RAND(n)];
        doorY = y;
        AddWarp(gen, gen->caveDoorX, y, RCAVE_NORTH);
        MarkOccupied(gen, gen->caveDoorX, y + 1);
        farX = gen->caveDoorX;
        farY = y + 1;
    }
    else if (role.far != 0xFF)
    {
        if (!PlaceCaveLadder(gen, &rooms[nMain - 1], role.far, &fx, &fy))
            CAVE_FAIL(6);
        gen->caveDoorX = fx;
        doorY = fy;
        farX = fx;
        farY = fy + 1;
    }
    if (!sGenFloor)
    {
        AddWarp(gen, gen->caveMatX, gen->caveMatY, RCAVE_SOUTH);
        MarkOccupied(gen, gen->caveMatX, gen->caveMatY);
        MarkOccupied(gen, gen->caveMatX, gen->caveMatY - 1);
    }
    // A split map: the near part ends at a ladder down, and the far part
    // starts at the second one, where the way comes back up.
    if (role.split)
    {
        if (!PlaceCaveLadder(gen, &rooms[split - 1], sGenFloor ? RFLOOR_DOWN : RCAVE_DOWN, &fx, &fy))
            CAVE_FAIL(7);
        cutX[0] = fx;
        cutY[0] = fy + 1;
        if (!PlaceCaveLadder(gen, &rooms[split], sGenFloor ? RFLOOR_DOWN_2 : RCAVE_DOWN_2, &fx, &fy))
            CAVE_FAIL(8);
        cutX[1] = fx;
        cutY[1] = fy + 1;
    }
    // A tunnel: the ladder up to its exit place, and the barrier before it.
    if (role.tunnel)
    {
        if (!PlaceCaveLadder(gen, &tunnelEnd, RFLOOR_EXIT, &fx, &fy))
            CAVE_FAIL(17);
        PlaceTunnelBarrier(gen, chokeX, chokeY);
    }

    if (!role.split || best < split)
        CaveFeature(gen, feature, &rooms[best], inX >> 1, inY >> 1);
    else
        CaveFeature(gen, feature, &rooms[best], farX >> 1, farY >> 1);
    StyledRooms(gen, rooms, nRooms, nMain, best, inX, inY);
    // A pool may not swallow a ladder placed before it.
    // (A volcano's doorways stand in the rock: the block in front of them.)
    for (i = 0; i < gen->m->nWarps; i++)
        if (gen->m->warps[i].kind != RCAVE_SOUTH && gen->m->warps[i].kind != RCAVE_NORTH
         && !IsFloorF(gen, gen->m->warps[i].x, gen->m->warps[i].y + (CaveStyle(gen) == CSTYLE_FIERY)))
            CAVE_FAIL(14);

    // A ladder down off the way: to floors below a cave the way goes
    // through, or to the guardian's floor below the floor the way crosses.
    if (role.aside != 0xFF)
    {
        if (!PlaceCaveLadder(gen, &rooms[aside], role.aside, &fx, &fy)
         && !PlaceCaveLadder(gen, &rooms[best], role.aside, &fx, &fy))
            CAVE_FAIL(9);
    }
    // The boss: a guardian in the far chamber of the bottom floor or in a
    // chamber of its own off the way, or a trainer by the way out, as Wally
    // waits at the top of Victory Road.
    if (d != NULL && BossIsTrainer(d) && sGenFloor == d->nFloors)
        nPlain--; // the boss's slot is the bottom floor's last
    if (d != NULL && role.boss == CBOSS_DOOR)
    {
        u8 slot = d->trainerBase + d->nTrainers - 1 - gen->p->trainerBase;
        int side = RAND(2) ? 1 : -1, s;
        for (s = 0; s < 2; s++, side = -side)
        {
            int bx = gen->caveDoorX + side, by = doorY + 1;
            if (IsFloorF(gen, bx, by) && !IsOccupied(gen, bx, by))
            {
                AddObj(gen, ROBJ_BOSS, bx, by, side > 0 ? RDIR_WEST : RDIR_EAST, slot, 1);
                bossX = bx;
                bossY = by;
                break;
            }
        }
        if (s == 2)
            CAVE_FAIL(10);
    }
    else if (d != NULL && role.boss != CBOSS_NONE)
    {
        const struct Rect *r = role.boss == CBOSS_ASIDE ? &rooms[aside] : &rooms[nMain - 1];
        if (!FindCaveSpot(gen, r, &fx, &fy) && (role.boss != CBOSS_ASIDE || !FindCaveSpot(gen, &rooms[best], &fx, &fy)))
            CAVE_FAIL(11);
        AddObj(gen, ROBJ_BOSS, fx, fy, RDIR_SOUTH, BossIsTrainer(d) ? gen->p->nTrainers - 1 : role.dungeon,
               BossIsTrainer(d) ? 4 : 0);
        bossX = fx;
        bossY = fy;
    }
    // The prize by the boss, a choice room, a camp.
    if (sGenFloor && !PlaceFloorExtras(gen, bossX, bossY, &rooms[nMain], nRooms - nMain, rooms, nMain, IsFloorF))
        CAVE_FAIL(15);
    // Trainers in the middle chambers (of each part), items in the side
    // chambers, and in a dead end the last chamber too.
    nNear = role.split ? (nPlain + 1) / 2 : nPlain;
    for (tries = 0; tries < 90 && trainers < nPlain; tries++)
    {
        const struct Rect *r = &rooms[1 + RAND(nMain - 2)];
        // (They move to the way when the map is drawn; where they start
        // matters only if there is no room on it.)
        if (tries >= 60)
            r = &rooms[RAND(nRooms)];
        else if (role.split)
        {
            // In the middle chambers of its part, else any of the part's.
            bool8 near = trainers < nNear, wider = tries >= 30;
            int lo = near ? 1 - wider : split, hi = near ? split - 1 : nMain - 2 + wider;
            r = &rooms[lo + RAND(hi - lo + 1)];
        }
        if (FindCaveSpot(gen, r, &fx, &fy))
            AddObj(gen, ROBJ_TRAINER, fx, fy, RAND(4), trainers++, 3);
    }
    for (tries = 0; tries < 80 && items < gen->p->nItems; tries++)
    {
        const struct Rect *r = (nRooms > nMain) ? &rooms[nMain + RAND(nRooms - nMain)] : &rooms[1];
        // (On an ice or volcano floor the far chamber, crowded by the boss,
        // the prize and the ice or lava beside it, is tried only a while.)
        if (role.deadEnd && (items == 0 || tries >= 40)
         && !(CaveStyle(gen) != CSTYLE_CAVE && items == 0 && tries >= 12 && tries < 40))
            r = &rooms[nMain - 1];
        else if (tries >= 50)
            r = &rooms[RAND(nRooms)];
        if (FindCaveSpot(gen, r, &fx, &fy))
            AddObj(gen, ROBJ_ITEM, fx, fy, RDIR_SOUTH, items++, 0);
    }
    if (trainers < nPlain)
        CAVE_FAIL(12);
    if (items < gen->p->nItems)
        CAVE_FAIL(13);
    if (!sGenFloor)
        PlaceCaveHidden(gen, rooms, nRooms, nMain);

    RenderCave(gen);
    if (role.tunnel)
        RenderTunnelBarrier(gen);
    // A ledge across a chamber now and then, on Victory Road most times.
    if (CHANCE(d != NULL && d->kind == RDUN_VICTORY ? 70 : 30) && CaveStyle(gen) == CSTYLE_CAVE)
        CaveLedge(gen, inX >> 1, inY >> 1, cutX[1] >> 1, cutY[1] >> 1, role.split);

    // Loose rocks in the chambers, placed like objects so they never block a
    // passage; a field of rubble in some caves.
    for (tries = 0; tries < (feature == CAVE_RUBBLE ? 160 : 60); tries++)
    {
        const struct Rect *r = &rooms[RAND(nRooms)];
        int x = r->x * 2 + RAND(r->w * 2), y = r->y * 2 + RAND(r->h * 2);
        if (!IsOpenCaveSpot(gen, x, y))
            continue;
        Put(gen, x, y, StyledRock(gen, feature == CAVE_RUBBLE ? (RAND(2) ? B_CAVE_RUBBLE : B_CAVE_RUBBLE2) : B_CAVE_ROCK));
        MarkOccupied(gen, x, y);
    }
    // A rock or two standing in the pool.
    for (tries = 0, i = 0; tries < 20 && i < 2; tries++)
    {
        int ux = 1 + RAND(gen->cw - 2), uy = 1 + RAND(gen->ch - 2), dx, dy;
        bool8 ok = TRUE;
        for (dy = -1; dy <= 1 && ok; dy++)
            for (dx = -1; dx <= 1 && ok; dx++)
                if (Cell(gen, ux + dx, uy + dy) != C_WATER)
                    ok = FALSE;
        // (A lava lake has no rocks standing in it.)
        if (!ok || CaveStyle(gen) != CSTYLE_CAVE)
            continue;
        for (dy = 0; dy < 4; dy++)
            Put(gen, ux * 2 + (dy & 1), uy * 2 + (dy >> 1), (i & 1) ? sSeaRock2[dy] : sSeaRock[dy]);
        i++;
    }
    // The trainers wait on the way through, from the way in to the far end,
    // in each part of a split map.
    if (role.split)
    {
        PlaceTrainersOnPart(gen, inX, inY, cutX[0], cutY[0], 0, nNear);
        PlaceTrainersOnPart(gen, cutX[1], cutY[1], farX, farY, nNear, nPlain - nNear);
    }
    else
    {
        PlaceTrainersOnPart(gen, inX, inY, farX, farY, 0, nPlain);
    }
    // Boulders and cracked rocks across the way, last, where nobody stands.
    // A map whose way has no tunnel it must pass is made again a few times,
    // so Victory Road's boulders stand in the way, as Emerald's do.
    if (role.obstacles)
    {
        int pairs;
        if (role.split)
            pairs = PlaceCaveGates(gen, inX, inY, cutX[0], cutY[0], 1) + PlaceCaveGates(gen, cutX[1], cutY[1], farX, farY, 1);
        else
            pairs = PlaceCaveGates(gen, inX, inY, farX, farY, 2);
        if (pairs == 0 && gen->attempt < 6)
            CAVE_FAIL(40);
    }
    // An ice cave's sliding puzzle, a volcano's boulders.
    if (!StyledFinish(gen, rooms, nRooms, nMain, best, inX, inY, farX, farY))
        CAVE_FAIL(41);
    return TRUE;
}

// ---------------------------------------------------------------------------
// Region plan

static const char *const sNameStarts[] = {
    "ASH", "BRIGHT", "CEDAR", "DAWN", "ELM", "FERN", "GLEN", "HAVEN", "IRON", "JUNI",
    "KEST", "LARK", "MAPLE", "NORTH", "OAK", "PINE", "QUILL", "ROSE", "STONE", "THORN",
    "UMBER", "VALE", "WILLOW", "AMBER", "BIRCH", "COBALT", "DUSK", "EMBER", "FROST", "GALE",
    "HOLLY", "MOSS", "RAVEN", "SILVER", "TIDE", "WREN",
};

static const char *const sNameEnds[] = {
    "WOOD", "FORD", "VALE", "BROOK", "FIELD", "RIDGE", "PORT", "HAVEN", "MOOR", "DALE",
    "CREST", "HOLLOW", "MILL", "STEAD", "BURY", "TON", "GATE", "WICK", "MERE", "FALL",
};

static int StrLen(const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    return n;
}

static void MakeTownName(u32 *rng, u8 *out, const char *suffix)
{
    const char *a, *b;
    int n = 0, i;
    do
    {
        a = sNameStarts[RogueRng_Range(rng, sizeof(sNameStarts) / sizeof(sNameStarts[0]))];
        b = sNameEnds[RogueRng_Range(rng, sizeof(sNameEnds) / sizeof(sNameEnds[0]))];
    } while (StrLen(a) + StrLen(b) + StrLen(suffix) > ROGUE_NAME_LEN);
    for (i = 0; a[i]; i++)
        out[n++] = a[i];
    for (i = 0; b[i]; i++)
        out[n++] = b[i];
    for (i = 0; suffix[i]; i++)
        out[n++] = suffix[i];
    out[n] = 0;
}

// Names that start the same way (OAKSTEAD and OAKHAVEN) are too alike.
static bool8 SameName(const u8 *a, const u8 *b)
{
    int i;
    for (i = 0; i < 3; i++)
        if (a[i] != b[i])
            return FALSE;
    return TRUE;
}

const struct RogueLink *RogueGen_FindLink(const struct RoguePlace *p, u8 target)
{
    int i;
    for (i = 0; i < p->nLinks; i++)
        if (p->links[i].target == target)
            return &p->links[i];
    return NULL;
}

static const struct RogueLink *LinkOnSide(const struct RoguePlace *p, u8 dir)
{
    int i;
    for (i = 0; i < p->nLinks; i++)
        if (p->links[i].dir == dir)
            return &p->links[i];
    return NULL;
}

bool8 RogueGen_IsGymTown(const struct RogueRegion *r, u8 place)
{
    return r->places[place].kind == RPLACE_TOWN && r->places[place].type == RTOWN_GYM;
}

// The first ordinary home in each town has a gift, and the next one the
// Name Rater in a village (unless a harbor or beach town has his house) or,
// from the third gym city on, a move tutor. The town given the Name Rater
// (ThemeInteriors) that has no building of his has him in a home instead,
// the one after the gift (and after the tutor, where there is one) or the
// last, so every region has him once. The map's signs and the house interiors (src/rogue_town.c) both
// ask here, so they agree.
u8 RogueGen_HomeRole(const struct RogueRegion *r, u8 place, const u8 *interiors, u32 nHouses, u32 house)
{
    const struct RoguePlace *p = &r->places[place];
    u32 i, nth = 0, homes = 0, rater = 0xFF;
    bool8 tutor = (p->gym != 0xFF && p->gym >= 2) || p->type == RTOWN_HAMLET;
    for (i = 0; i < nHouses; i++)
        homes += interiors[i] == RINT_HOME;
    for (i = 0; i < house; i++)
        if (interiors[i] == RINT_HOME)
            nth++;
    // Where he is owed, he takes the home after the gift and the tutor, or
    // the last one when there are fewer.
    if (p->interiors & (1 << RINT_NAME_RATER))
    {
        rater = tutor ? 2 : 1;
        for (i = 0; i < nHouses; i++)
            if (interiors[i] == RINT_NAME_RATER)
                rater = 0xFF;
        if (rater != 0xFF && rater >= homes)
            rater = homes - 1;
        if (nth == rater)
            return RHOME_NAME_RATER;
    }
    // A hamlet's lone house is the move tutor's; the next one has the gift.
    if (p->type == RTOWN_HAMLET)
        return nth == 0 ? RHOME_RELEARNER : (nth == 1 ? RHOME_GIFT : RHOME_CHAT);
    if (nth == 0)
        return RHOME_GIFT;
    if (nth == 1 && p->type == RTOWN_VILLAGE)
    {
        bool8 harbor = FALSE;
        for (i = 0; i < r->nTowns; i++)
            if (r->places[r->towns[i]].interiors & (1 << RINT_NAME_RATER))
                harbor = TRUE;
        if (!harbor)
            return RHOME_NAME_RATER;
    }
    // The move tutor waits until teams have moves worth remembering.
    if (nth == 1 && tutor)
        return RHOME_RELEARNER;
    return RHOME_CHAT;
}

#define ARRAY_COUNT_GEN(a) (sizeof(a) / sizeof((a)[0]))

// ---------------------------------------------------------------------------
// Land and sea

// The region is planned on a grid of cells, one place a cell. At two tiles a
// cell and a tile of sea round it, the grid is one screen of region map. The
// land comes first: the side or two the open sea lies on, its coast, now and
// then a bay, and inland a mountain, a volcano or a lake. Places stand on
// land, the sea route on the sea, and caves may run into the mountain.
#define RGRID_W ROGUE_GRID_W
#define RGRID_H ROGUE_GRID_H

enum { CELL_SEA, CELL_LAND, CELL_LAKE, CELL_PEAK };
enum { FEATURE_NONE, FEATURE_MOUNTAIN, FEATURE_VOLCANO, FEATURE_LAKE };

struct RogueLand
{
    u8 cell[RGRID_H][RGRID_W];
    u8 feature;
    s8 fx, fy;   // the feature's top left cell
    u8 fw, fh;
    u8 seaSides; // bit per enum RogueDir: the open sea lies off that side
    s8 x0, y0, x1, y1; // the bounds of the land
    u8 nLand;    // land cells a place can stand on
};

static u8 LandAt(const struct RogueLand *L, int x, int y)
{
    if (x < 0 || y < 0 || x >= RGRID_W || y >= RGRID_H)
        return CELL_SEA;
    return L->cell[y][x];
}

// v1.1 sea (src/rogue_gen_sea.h).
static u8 SeaSecondStretch(u32 seed);
static void SeaSecondGap(u8 *gap, u8 *villageAt, u8 *sea2);
static void SeaSecondAims(s8 *aimX, s8 *aimY);
static bool8 SeaByLand(int x, int y);
static bool8 SeaBySea(int x, int y);
static bool8 SeaShoreCell(int x, int y);
static bool8 SeaFreeShoreNear(int x, int y);
static void SeaSinkRoutes(struct RogueLand *L, const struct RogueRegion *r);
static void SeaPlanKinds(struct RogueRegion *r);
static void SeaAddIslands(struct RogueRegion *r, const u8 *spinePlace);
static void SeaFootReach(struct Gen *gen, u8 *seen);
static bool8 SeaFootF(struct Gen *gen, int x, int y);
static bool8 SeaHiddenReached(struct Gen *gen, int fx, int fy);
// (v1.1 speed: each walk gives up once it has gone SEA2_STALL steps without
// getting further along the road, so a failed guide costs a few hundred steps
// rather than SEA2_BUDGET; more lands, fewer guides each, keep the share of
// regions with the second stretch near half.)
#define SEA2_LANDS 20
#define SEA2_LAND_0 201 // land numbers past any a region without it uses (RogueRegion.land is a byte)
#define SEA2_GUIDES 4
#define SEA2_BUDGET 2400
#define SEA2_STALL 100
static bool8 sSeaSecond; // the main road being laid has a second stretch of sea
static u8 sSeaSecondAt;  // the step where it starts, or 0xFF
static u8 sSeaSecondFrom; // the first step after the first stretch, or 0xFF
#define SEA2_COAST 12

// The land shape number salt of a seed. The same seed and salt always give
// the same land, so the region map redraws it from the plan's land number.
static bool8 PlanLand(u32 seed, u32 salt, struct RogueLand *L)
{
    u32 rng = RogueRng_Hash(seed, 31000 + salt);
    u8 sides[2], nSides, stack[RGRID_W * RGRID_H], comp[RGRID_H][RGRID_W];
    int x, y, i, k, mode, best = 0, bestComp = 0, nComp = 0;

    memset(L, 0, sizeof(*L));
    for (y = 0; y < RGRID_H; y++)
        for (x = 0; x < RGRID_W; x++)
            L->cell[y][x] = CELL_LAND;

    // The open sea: off one side, or round a corner.
    // The deep sea lies east or west, the long way across the map, and
    // half the time round a corner to the north or south as well.
    mode = RogueRng_Range(&rng, 4);
    // (v1.1 sea: the lands of a region with a second stretch of sea always
    // have the sea round a corner, so the road can meet it twice.)
    if (salt >= SEA2_LAND_0)
        mode |= 2;
    sides[0] = (mode & 1) ? RDIR_EAST : RDIR_WEST;
    nSides = 1;
    if (mode & 2)
        sides[nSides++] = RogueRng_Range(&rng, 2) ? RDIR_NORTH : RDIR_SOUTH;
    for (i = 0; i < nSides; i++)
    {
        u8 s = sides[i];
        bool8 across = (s == RDIR_EAST || s == RDIR_WEST);
        int len = across ? RGRID_H : RGRID_W;
        int lo = 1, hi = across ? 3 : 2;
        int depth = 1 + RogueRng_Range(&rng, 2);
        L->seaSides |= 1 << s;
        for (k = 0; k < len; k++)
        {
            int j;
            if (k > 0 && RogueRng_Range(&rng, across ? 2 : 4) == 0)
                depth += RogueRng_Range(&rng, 2) ? 1 : -1;
            if (depth < lo) depth = lo;
            if (depth > hi) depth = hi;
            for (j = 0; j < depth; j++)
            {
                switch (s)
                {
                case RDIR_NORTH: L->cell[j][k] = CELL_SEA; break;
                case RDIR_SOUTH: L->cell[RGRID_H - 1 - j][k] = CELL_SEA; break;
                case RDIR_WEST:  L->cell[k][j] = CELL_SEA; break;
                default:         L->cell[k][RGRID_W - 1 - j] = CELL_SEA; break;
                }
            }
        }
    }
    // A ragged edge on the other sides, the corners most of all.
    for (y = 0; y < RGRID_H; y++)
    {
        for (x = 0; x < RGRID_W; x++)
        {
            bool8 edgeX = (x == 0 || x == RGRID_W - 1), edgeY = (y == 0 || y == RGRID_H - 1);
            if ((edgeX || edgeY) && RogueRng_Range(&rng, 100) < ((edgeX && edgeY) ? 60 : 22))
                L->cell[y][x] = CELL_SEA;
        }
    }
    // Half the time a bay cuts into the coast.
    if (RogueRng_Range(&rng, 2) == 0)
    {
        u8 s = sides[RogueRng_Range(&rng, nSides)];
        bool8 across = (s == RDIR_EAST || s == RDIR_WEST);
        int along = 1 + RogueRng_Range(&rng, (across ? RGRID_H : RGRID_W) - 2);
        int depth = 1 + RogueRng_Range(&rng, 2), wide = RogueRng_Range(&rng, 2);
        int step = (s == RDIR_EAST || s == RDIR_SOUTH) ? -1 : 1;
        int pos = (s == RDIR_EAST) ? RGRID_W - 1 : (s == RDIR_SOUTH) ? RGRID_H - 1 : 0;
        for (k = 0; k < 8 && depth > 0; k++, pos += step)
        {
            int cx = across ? pos : along, cy = across ? along : pos;
            if (LandAt(L, cx, cy) != CELL_LAND)
                continue;
            L->cell[cy][cx] = CELL_SEA;
            if (wide && LandAt(L, cx + (across ? 0 : 1), cy + (across ? 1 : 0)) == CELL_LAND)
                L->cell[cy + (across ? 1 : 0)][cx + (across ? 0 : 1)] = CELL_SEA;
            depth--;
        }
    }
    // Inland, now and then a mountain, a volcano or a lake, with land all
    // round it.
    k = RogueRng_Range(&rng, 10);
    L->feature = k < 3 ? FEATURE_MOUNTAIN : k < 5 ? FEATURE_VOLCANO : k < 7 ? FEATURE_LAKE : FEATURE_NONE;
    if (L->feature != FEATURE_NONE)
    {
        int sx = 0, sy = 0, n = 0, tries;
        bool8 placed = FALSE;
        if (L->feature == FEATURE_LAKE)
        {
            L->fw = 1 + RogueRng_Range(&rng, 2);
            L->fh = L->fw == 1 ? 2 : 1 + RogueRng_Range(&rng, 2);
        }
        else
        {
            L->fw = 2 + RogueRng_Range(&rng, L->feature == FEATURE_MOUNTAIN ? 2 : 1);
            L->fh = 1 + RogueRng_Range(&rng, 2);
        }
        for (y = 0; y < RGRID_H; y++)
            for (x = 0; x < RGRID_W; x++)
                if (L->cell[y][x] == CELL_LAND)
                {
                    sx += x;
                    sy += y;
                    n++;
                }
        for (tries = 0; tries < 16 && n && !placed; tries++)
        {
            int fx = sx / n - L->fw / 2 + (int)RogueRng_Range(&rng, 5) - 2;
            int fy = sy / n - L->fh / 2 + (int)RogueRng_Range(&rng, 3) - 1;
            bool8 ok = TRUE;
            for (y = fy - 1; y <= fy + L->fh && ok; y++)
                for (x = fx - 1; x <= fx + L->fw && ok; x++)
                    if (LandAt(L, x, y) != CELL_LAND)
                        ok = FALSE;
            if (!ok)
                continue;
            for (y = fy; y < fy + L->fh; y++)
                for (x = fx; x < fx + L->fw; x++)
                    L->cell[y][x] = L->feature == FEATURE_LAKE ? CELL_LAKE : CELL_PEAK;
            L->fx = fx;
            L->fy = fy;
            placed = TRUE;
        }
        if (!placed)
            L->feature = FEATURE_NONE;
    }
    // One body of land: smaller pieces go under the sea.
    memset(comp, 0, sizeof(comp));
    for (y = 0; y < RGRID_H; y++)
    {
        for (x = 0; x < RGRID_W; x++)
        {
            int top = 0, size = 0;
            if (L->cell[y][x] == CELL_SEA || comp[y][x])
                continue;
            nComp++;
            comp[y][x] = nComp;
            stack[top++] = y * RGRID_W + x;
            while (top)
            {
                int c = stack[--top], d;
                size++;
                for (d = 0; d < 4; d++)
                {
                    int nx = c % RGRID_W + sDirDX[d], ny = c / RGRID_W + sDirDY[d];
                    if (LandAt(L, nx, ny) == CELL_SEA || comp[ny][nx])
                        continue;
                    comp[ny][nx] = nComp;
                    stack[top++] = ny * RGRID_W + nx;
                }
            }
            if (size > best)
            {
                best = size;
                bestComp = nComp;
            }
        }
    }
    L->x0 = RGRID_W;
    L->y0 = RGRID_H;
    L->x1 = L->y1 = -1;
    for (y = 0; y < RGRID_H; y++)
    {
        for (x = 0; x < RGRID_W; x++)
        {
            if (comp[y][x] != bestComp)
                L->cell[y][x] = CELL_SEA;
            if (L->cell[y][x] == CELL_SEA)
                continue;
            if (x < L->x0) L->x0 = x;
            if (x > L->x1) L->x1 = x;
            if (y < L->y0) L->y0 = y;
            if (y > L->y1) L->y1 = y;
            L->nLand += (L->cell[y][x] == CELL_LAND);
        }
    }
    return L->nLand >= 52 && L->x1 - L->x0 >= 10 && L->y1 - L->y0 >= 5;
}

// Once the places stand, land far from all of them goes under the sea, so
// the coast follows the roads as Hoenn's does. The mountain, the volcano and
// a lake keep their land round them.
// TRUE if (x, y) is land with the sea beside it or at a corner.
static bool8 ShoreCell(const struct RogueLand *L, int x, int y)
{
    int dx, dy;
    if (LandAt(L, x, y) != CELL_LAND)
        return FALSE;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
            if (LandAt(L, x + dx, y + dy) == CELL_SEA)
                return TRUE;
    return FALSE;
}

// TRUE if a harbor town's sea beyond its edge, at (x, y), goes under the
// sea on the plan: shore land with no place on it.
static bool8 HarborSeaCell(const struct RogueLand *L, const struct RogueRegion *r, int x, int y)
{
    int i;
    if (!ShoreCell(L, x, y))
        return FALSE;
    for (i = 0; i < r->nPlaces; i++)
        if (r->places[i].gridX == x && r->places[i].gridY == y)
            return FALSE;
    return TRUE;
}

static void TrimLand(struct RogueLand *L, const struct RogueRegion *r)
{
    u8 keep[RGRID_H][RGRID_W], harbor[RGRID_H][RGRID_W];
    int x, y, i, dx, dy, pass;
    // v1.1 sea: a sea route laid on the shore's land.
    SeaSinkRoutes(L, r);
    memset(keep, 0, sizeof(keep));
    memset(harbor, 0, sizeof(harbor));
    for (i = 0; i < r->nPlaces; i++)
        for (dy = -1; dy <= 1; dy++)
            for (dx = -1; dx <= 1; dx++)
                if (LandAt(L, r->places[i].gridX + dx, r->places[i].gridY + dy) != CELL_SEA)
                    keep[r->places[i].gridY + dy][r->places[i].gridX + dx] = 1;
    for (y = 0; y < RGRID_H; y++)
        for (x = 0; x < RGRID_W; x++)
            if (L->cell[y][x] == CELL_PEAK || L->cell[y][x] == CELL_LAKE)
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++)
                        if (LandAt(L, x + dx, y + dy) != CELL_SEA)
                            keep[y + dy][x + dx] = 1;
    // Land between two parts of the road, along a row or a column of the
    // plan's land, stays land: the region is one body of land with the
    // roads across it, not causeways with the sea between them.
    for (pass = 0; pass < 2; pass++)
    {
        for (y = 0; y < RGRID_H; y++)
        {
            for (x = 0; x < RGRID_W; x++)
            {
                int k, a, b;
                if (keep[y][x] || L->cell[y][x] == CELL_SEA)
                    continue;
                for (k = 0; k < 2 && !keep[y][x]; k++)
                {
                    int ddx = k == 0, ddy = k == 1;
                    for (a = 1; LandAt(L, x - a * ddx, y - a * ddy) != CELL_SEA && !keep[y - a * ddy][x - a * ddx]; a++)
                        ;
                    for (b = 1; LandAt(L, x + b * ddx, y + b * ddy) != CELL_SEA && !keep[y + b * ddy][x + b * ddx]; b++)
                        ;
                    if (LandAt(L, x - a * ddx, y - a * ddy) != CELL_SEA && LandAt(L, x + b * ddx, y + b * ddy) != CELL_SEA)
                        keep[y][x] = 1;
                }
            }
        }
    }
    // A harbor town's sea, where the plan had land on the shore with no
    // place on it (see AssignTownLayouts): all judged first, then sunk.
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        if (p->kind == RPLACE_TOWN && p->layout == RLAYOUT_HARBOR && p->seaSide != RDIR_NONE)
        {
            x = p->gridX + sDirDX[p->seaSide];
            y = p->gridY + sDirDY[p->seaSide];
            if (HarborSeaCell(L, r, x, y))
                harbor[y][x] = 1;
        }
    }
    for (y = 0; y < RGRID_H; y++)
        for (x = 0; x < RGRID_W; x++)
            if (harbor[y][x])
                L->cell[y][x] = CELL_SEA;
    L->x0 = RGRID_W;
    L->y0 = RGRID_H;
    L->x1 = L->y1 = -1;
    for (y = 0; y < RGRID_H; y++)
    {
        for (x = 0; x < RGRID_W; x++)
        {
            if (L->cell[y][x] == CELL_LAND && !keep[y][x])
                L->cell[y][x] = CELL_SEA;
            if (L->cell[y][x] == CELL_SEA)
                continue;
            if (x < L->x0) L->x0 = x;
            if (x > L->x1) L->x1 = x;
            if (y < L->y0) L->y0 = y;
            if (y > L->y1) L->y1 = y;
        }
    }
}

// ---------------------------------------------------------------------------
// Region map

// Tiles of Hoenn's region map that the places and roads use. The land, the
// coast and the road surface are picked from Hoenn's tiles by how they join
// (data/rogue_region_map.h, made by tools/roguemon/gen_region_map.py).
#define RMAP_SEA        0x00
#define RMAP_SEA_ROUTE  0x4D
#define RMAP_CITY       0x14
#define RMAP_TOWN       0x0D
#define RMAP_CITY_W     RMAP_TILE_CITY_W // a city two tiles wide: its west and east halves
#define RMAP_CITY_E     0x4C
#define RMAP_CITY_N     RMAP_TILE_CITY_N // a city two tiles tall: its north and south halves
#define RMAP_CITY_S     0x78

// While the map is laid out, tiles that belong to a place or a road hold
// one of these.
#define RMAP_MARK       0x8000
#define RMAP_MARK_ROAD  (RMAP_MARK | 1)
#define RMAP_MARK_WATER (RMAP_MARK | 2)
#define RMAP_MARK_TOWN  (RMAP_MARK | 3)
#define RMAP_MARK_CITY  (RMAP_MARK | 4)
#define RMAP_MARK_SAND  (RMAP_MARK | 5)
#define RMAP_MARK_RAFT  (RMAP_MARK | 6) // (v1.1 towns2) a raft village: a town out on the water
#define RMAP_RAFT       0xC9            // Pacifidlog's and Dewford's: a town on a sea route

// While the tiles are picked, a tile to pick holds its kind here and the
// land corners it should have (as the coast tiles: top left 8, top right 4,
// bottom left 2, bottom right 1) above the tile picked so far.
#define RMAP_PICK_LAND  0x1000
#define RMAP_PICK_ROAD  0x2000
#define RMAP_PICK_SAND  0x3000
#define RMAP_PICK       0x3000
#define RMAP_PICK_INLAND 0x4000 // land with land all round, away from the coast
#define RMAP_PICK_MASK(t) (((t) >> 8) & 15)
// A land tile set beforehand (the crater): the tiles round it are picked to
// join it, and it stays.
#define RMAP_FIXED      (RMAP_MARK | RMAP_PICK_INLAND)
#define IsFixedTile(t)  (((t) & RMAP_FIXED) == RMAP_FIXED)

// Mt. Chimney on Hoenn's map: a crater two tiles by two, a dark ring round
// a light middle, with its slope to the west.
static const u8 sCraterTiles[2][3] = { { 0x1C, 0x1D, 0x1E }, { 0x2C, 0x2D, 0x2E } };

static bool8 IsSeaRoute(const struct RoguePlace *p)
{
    return p->kind == RPLACE_ROUTE && p->type == RROUTE_SEA;
}

// Places a and b are joined by a map edge or a cave mouth.
static bool8 MapJoined(const struct RogueRegion *r, u32 a, u32 b)
{
    const struct RoguePlace *pa = &r->places[a], *pb = &r->places[b];
    if (RogueGen_FindLink(pa, b) != NULL)
        return TRUE;
    if (pa->kind == RPLACE_CAVE && (pa->caveIn == b || pa->caveOut == b))
        return TRUE;
    if (pb->kind == RPLACE_CAVE && (pb->caveIn == a || pb->caveOut == a))
        return TRUE;
    return FALSE;
}

// The land is kept at the corners of the tiles, so two tiles side by side
// always agree on the edge between them, and each tile picks the coast that
// fits its four corners. shade holds how dark the land should be at each
// corner, 0 (Hoenn's lightest green) to 16 (its darkest).
struct RegionMapDraw
{
    const struct RogueRegion *r;
    u16 *tiles;
    u8 *land, *spare, *shade;
    int w, h; // tiles used; the corners are (w + 1) x (h + 1)
};

static u8 *Corner(struct RegionMapDraw *d, u8 *grid, int x, int y)
{
    static u8 sOutside;
    if (x < 0 || y < 0 || x > d->w || y > d->h)
    {
        sOutside = 0;
        return &sOutside;
    }
    return &grid[y * (d->w + 1) + x];
}

static u16 MapTile(struct RegionMapDraw *d, int x, int y)
{
    if (x < 0 || y < 0 || x >= d->w || y >= d->h)
        return 0;
    return d->tiles[y * ROGUE_RMAP_W + x];
}

static int CornerNeighbours(struct RegionMapDraw *d, u8 *grid, int x, int y)
{
    return *Corner(d, grid, x + 1, y) + *Corner(d, grid, x - 1, y) + *Corner(d, grid, x, y + 1) + *Corner(d, grid, x, y - 1);
}

// Sea that the open sea cannot reach becomes land; lakes are cut in after.
static void FillInlandSea(struct RegionMapDraw *d)
{
    int x, y, n = (d->w + 1) * (d->h + 1);
    bool8 changed = TRUE;
    u8 *open = d->spare;
    for (x = 0; x < n; x++)
        open[x] = FALSE;
    while (changed)
    {
        changed = FALSE;
        for (y = 0; y <= d->h; y++)
        {
            for (x = 0; x <= d->w; x++)
            {
                u8 *o = Corner(d, open, x, y);
                if (*o || *Corner(d, d->land, x, y))
                    continue;
                if (x == 0 || y == 0 || x == d->w || y == d->h || CornerNeighbours(d, open, x, y))
                {
                    *o = TRUE;
                    changed = TRUE;
                }
            }
        }
    }
    for (x = 0; x < n; x++)
        if (!open[x])
            d->land[x] = TRUE;
}

// Rounds the coast off: land sticking out on its own and the outer corners
// of the land go, notches and the inner corners of bays fill in. Hoenn's
// coast tiles then draw it as one smooth line.
static void SmoothCoast(struct RegionMapDraw *d)
{
    int x, y, pass, n = (d->w + 1) * (d->h + 1);
    for (pass = 0; pass < 2; pass++)
    {
        for (x = 0; x < n; x++)
            d->spare[x] = d->land[x];
        for (y = 0; y <= d->h; y++)
        {
            for (x = 0; x <= d->w; x++)
            {
                u8 *c = Corner(d, d->land, x, y);
                u8 e = *Corner(d, d->spare, x + 1, y), w = *Corner(d, d->spare, x - 1, y);
                u8 s = *Corner(d, d->spare, x, y + 1), nn = *Corner(d, d->spare, x, y - 1);
                int around = e + w + s + nn;
                // Two neighbours at a right angle: an outer corner of the land,
                // or an inner corner of the sea.
                bool8 bend = around == 2 && e != w;
                if (*c && (around <= 1 || (bend && pass == 0)))
                    *c = FALSE;
                else if (!*c && (around >= 3 || (bend && pass == 1)))
                    *c = TRUE;
            }
        }
    }
}

// The darkness each corner's land should have: a dark rim of trees inside
// the coast as on Hoenn's map, rolling light and mid greens inland, woods
// round forest routes and treetop towns, and light hills round mountains,
// hill routes and caves.
static void ShadeLand(struct RegionMapDraw *d, const struct RogueLand *L, const s8 *placeX, const s8 *placeY,
                      int ox, int oy, int minX, int minY, int spacing)
{
    int x, y, i, stride = d->w + 1, n = stride * (d->h + 1);
    u8 *dist = d->spare, *shade = d->shade, *woods = d->shade + n;
    bool8 full[ROGUE_MAX_PLACES];
    u8 lattice[ROGUE_RMAP_VIEW_H / 4 + 2][ROGUE_RMAP_VIEW_W / 4 + 2];

    // Distance to the sea, in corners (diagonals count as one), there and
    // back again across the grid. Off the map is sea.
    for (i = 0; i < n; i++)
        dist[i] = d->land[i] ? 99 : 0;
    for (y = 0; y <= d->h; y++)
    {
        for (x = 0; x <= d->w; x++)
        {
            u8 *c = &dist[y * stride + x], v;
            v = (x > 0) ? c[-1] : 0;
            v = (y > 0 && c[-stride] < v) ? c[-stride] : (y > 0 ? v : 0);
            if (x > 0 && y > 0 && c[-stride - 1] < v)
                v = c[-stride - 1];
            if (x < d->w && y > 0 && c[-stride + 1] < v)
                v = c[-stride + 1];
            if (x == d->w)
                v = 0;
            if (v + 1 < *c)
                *c = v + 1;
        }
    }
    for (y = d->h; y >= 0; y--)
    {
        for (x = d->w; x >= 0; x--)
        {
            u8 *c = &dist[y * stride + x], v;
            v = (x < d->w) ? c[1] : 0;
            v = (y < d->h && c[stride] < v) ? c[stride] : (y < d->h ? v : 0);
            if (x < d->w && y < d->h && c[stride + 1] < v)
                v = c[stride + 1];
            if (x > 0 && y < d->h && c[stride - 1] < v)
                v = c[stride - 1];
            if (x == 0)
                v = 0;
            if (v + 1 < *c)
                *c = v + 1;
        }
    }
    // Rolling ground: a value noise over a lattice four corners apart, from
    // 4 to 14; the rim of the land darkest.
    for (y = 0; y <= (d->h >> 2) + 1; y++)
        for (x = 0; x <= (d->w >> 2) + 1; x++)
            lattice[y][x] = RogueRng_Hash(d->r->seed, 27000 + y * 16 + x) & 15;
    for (y = 0; y <= d->h; y++)
    {
        const u8 *top = lattice[y >> 2], *bottom = lattice[(y >> 2) + 1];
        int fy = y & 3;
        for (x = 0; x <= d->w; x++)
        {
            int gx = x >> 2, fx = x & 3;
            int v = ((top[gx] * (4 - fx) + top[gx + 1] * fx) * (4 - fy) + (bottom[gx] * (4 - fx) + bottom[gx + 1] * fx) * fy) >> 4;
            u8 dd = dist[y * stride + x];
            v = 4 + ((v * 43) >> 6); // 4 to 14
            if (dd <= 1)
                v = 16;
            else if (dd == 2 && v < 14)
                v = 14;
            shade[y * stride + x] = v;
        }
    }
    // Hills round hill routes, mountains and caves, light ground by desert
    // routes, fading out over three corners.
    for (i = 0; i < d->r->nPlaces; i++)
    {
        const struct RoguePlace *p = &d->r->places[i];
        int want = -1;
        if ((p->kind == RPLACE_ROUTE && (p->type == RROUTE_HILLS || p->type == RROUTE_MOUNTAIN)) || p->kind == RPLACE_CAVE)
            want = 0;
        else if (p->kind == RPLACE_ROUTE && p->type == RROUTE_DESERT)
            want = 2;
        if (want < 0)
            continue;
        for (y = placeY[i] - 2; y <= placeY[i] + 3; y++)
        {
            for (x = placeX[i] - 2; x <= placeX[i] + 3; x++)
            {
                int fx = placeX[i] - x > x - placeX[i] - 1 ? placeX[i] - x : x - placeX[i] - 1;
                int fy = placeY[i] - y > y - placeY[i] - 1 ? placeY[i] - y : y - placeY[i] - 1;
                int far = fx > fy ? fx : fy;
                u8 *s;
                if (x < 0 || y < 0 || x > d->w || y > d->h || dist[y * stride + x] <= 1)
                    continue;
                s = &shade[y * stride + x];
                if (far <= 0)
                    *s = want;
                else // a third of the way, or two thirds
                    *s = ((*s * far + want * (3 - far)) * 171) >> 9;
            }
        }
    }
    // Woods round forest routes, treetop towns and forests, as round clumps like
    // Hoenn's: dark at a corner of the place's tile, fading over two corners.
    // Clumps take the darkest pull where they overlap rather than adding up,
    // and a forest place next to one that already has a full clump gets a
    // smaller, lighter one, so a run of forest routes reads as patches of
    // woods with greens between, not one dark band.
    for (i = 0; i < n; i++)
        woods[i] = 0;
    for (i = 0; i < d->r->nPlaces; i++)
    {
        static const u8 sPull[2][3] = { { 8, 5, 2 }, { 6, 2, 0 } }; // eighths, by distance
        const struct RoguePlace *p = &d->r->places[i];
        u32 h = RogueRng_Hash(d->r->seed, 27500 + i);
        int cx = placeX[i] + (h & 1), cy = placeY[i] + ((h >> 1) & 1), j, small = FALSE;
        s32 di = RogueGen_DungeonAt(d->r, i);
        bool8 forest = (p->kind == RPLACE_ROUTE && (p->type == RROUTE_FOREST || p->type == RROUTE_WOODS))
                    || (p->kind == RPLACE_TOWN && p->theme == RTHEME_TREETOP)
                    || (di >= 0 && d->r->dungeons[di].kind == RDUN_FOREST); // a deep forest off a field route
        full[i] = FALSE;
        if (!forest)
            continue;
        for (j = 0; j < i; j++)
            if (full[j] && (d->r->places[j].gridX - p->gridX) * (d->r->places[j].gridX - p->gridX)
                         + (d->r->places[j].gridY - p->gridY) * (d->r->places[j].gridY - p->gridY) <= 2)
                small = TRUE;
        full[i] = !small;
        for (y = cy - 2; y <= cy + 2; y++)
        {
            for (x = cx - 2; x <= cx + 2; x++)
            {
                int fx = x > cx ? x - cx : cx - x, fy = y > cy ? y - cy : cy - y;
                u8 pull = sPull[small][fx > fy ? fx : fy];
                if (x >= 0 && y >= 0 && x <= d->w && y <= d->h && pull > woods[y * stride + x])
                    woods[y * stride + x] = pull;
            }
        }
    }
    for (i = 0; i < n; i++)
        if (woods[i] && dist[i] > 1)
            shade[i] += ((16 - shade[i]) * woods[i]) >> 3;
    // Mountains and the volcano as hills.
    for (y = 0; y < RGRID_H; y++)
    {
        for (x = 0; x < RGRID_W; x++)
        {
            int tx = ox + (x - minX) * spacing, ty = oy + (y - minY) * spacing, i2, j2;
            if (L->cell[y][x] != CELL_PEAK)
                continue;
            for (j2 = 0; j2 < 3; j2++)
            {
                for (i2 = 0; i2 < 3; i2++)
                {
                    int cx = tx + i2, cy = ty + j2;
                    if (cx > d->w || cy > d->h || dist[cy * stride + cx] <= 1)
                        continue;
                    shade[cy * stride + cx] = (i2 == 1 && j2 == 1) ? 0 : (shade[cy * stride + cx] * 85) >> 8;
                }
            }
        }
    }
}

// The tile picked so far for one place on the map, and what it is
// measured against.
struct TilePick
{
    const u8 *seam[4]; // seam costs with the tiles west, east, north and south
    const u8 *fit;
    int shade[4];
    u32 jitter;
    u32 west, north; // the tiles picked west and north of it, if any
    u32 best, bestCost, bestSeams;
};

// A tile costs its seams with the tiles round it that are picked or fixed
// (8 for each unit of sRegionMapSeam*, so a seam Hoenn has costs nothing),
// how badly it fits (sRegionMapFit), twice how far its corners' darkness is
// from the shading (so a chain of Hoenn's dark-edged tiles cannot run on
// across light ground), and up to 7 at random so like places do not all get
// like tiles. The tile west or north of it is never taken again, so no tile
// runs on in a row.
ROGUE_FAST static void TryTiles(struct TilePick *p, const u8 *list, int n)
{
    const u8 *fit = p->fit, *west = p->seam[0], *east = p->seam[1], *north = p->seam[2], *south = p->seam[3];
    u32 bestCost = p->bestCost, jitter = p->jitter;
    int s0 = p->shade[0], s1 = p->shade[1], s2 = p->shade[2], s3 = p->shade[3];
    for (; n > 0; n--)
    {
        u32 c = *list++, f = fit[c], e, s, seams, cost;
        int v;
        if (f == 255 || c == p->west || c == p->north)
            continue;
        e = sRegionMapTileInfo[c].edges;
        seams = west[e & 0xFF] + east[(e >> 8) & 0xFF] + north[(e >> 16) & 0xFF] + south[e >> 24];
        cost = (seams << 3) + f;
        if (cost >= bestCost)
            continue;
        s = sRegionMapTileInfo[c].shade;
        v = (int)(s & 0xFF) - s0;
        cost += 2 * (v < 0 ? -v : v);
        v = (int)((s >> 8) & 0xFF) - s1;
        cost += 2 * (v < 0 ? -v : v);
        v = (int)((s >> 16) & 0xFF) - s2;
        cost += 2 * (v < 0 ? -v : v);
        v = (int)(s >> 24) - s3;
        cost += 2 * (v < 0 ? -v : v);
        cost += sRegionMapJitter[(jitter + c) & 0xFF];
        if (cost < bestCost)
        {
            bestCost = cost;
            p->bestSeams = seams;
            p->best = c;
        }
    }
    p->bestCost = bestCost;
}

// Picks every land, coast and road tile: first in reading order against
// the tiles before it, then once more, but for inland, against all four
// neighbours. Trying every tile everywhere would take too long on the GBA
// (a try costs about 300 cycles), so the search goes by steps and stops
// when the seams (in units of sRegionMapSeam*) are good enough:
// - with no neighbour to join, the evenly shaded tiles at the start of the
//   list;
// - else the coast tiles that fit the corners exactly, then, past 4 a
//   seam, the tiles that make a seam Hoenn has with a neighbour (other than
//   open sea, which joins too many), then, past 8 a seam, the rest (only the
//   evenly shaded tiles for land and roads);
// - on the second pass a tile with seams of 4 or less stays.
ROGUE_FAST static void PickTiles(struct RegionMapDraw *d)
{
    static const u8 sNoSeam[128] = {0};
    static const s8 sNear[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
    static const u8 *const sExact[3][4] =
    {
        { sRegionMapExactLandH, sRegionMapExactLandHT, sRegionMapExactLandV, sRegionMapExactLandVT },
        { sRegionMapExactCoastH, sRegionMapExactCoastHT, sRegionMapExactCoastV, sRegionMapExactCoastVT },
        { sRegionMapExactRoadH, sRegionMapExactRoadHT, sRegionMapExactRoadV, sRegionMapExactRoadVT },
    };
    static const u16 *const sExactStart[3][4] =
    {
        { sRegionMapExactLandHStart, sRegionMapExactLandHTStart, sRegionMapExactLandVStart, sRegionMapExactLandVTStart },
        { sRegionMapExactCoastHStart, sRegionMapExactCoastHTStart, sRegionMapExactCoastVStart, sRegionMapExactCoastVTStart },
        { sRegionMapExactRoadHStart, sRegionMapExactRoadHTStart, sRegionMapExactRoadVStart, sRegionMapExactRoadVTStart },
    };
    struct TilePick pick, *p = &pick;
    int x, y, pass, i, stride = d->w + 1;

    // Land tiles with land all round them, away from any coast, take only
    // land tiles: all land tiles start so, then the sea and the coast clear
    // it round them, and so does the edge of the map.
    for (y = 0; y < d->h; y++)
    {
        for (x = 0; x < d->w; x++)
        {
            u16 *t = &d->tiles[y * ROGUE_RMAP_W + x];
            if ((*t & (RMAP_MARK | RMAP_PICK)) == RMAP_PICK_LAND && RMAP_PICK_MASK(*t) == 15
             && x > 0 && y > 0 && x < d->w - 1 && y < d->h - 1)
                *t |= RMAP_PICK_INLAND;
        }
    }
    for (y = 0; y < d->h; y++)
    {
        for (x = 0; x < d->w; x++)
        {
            u16 t = d->tiles[y * ROGUE_RMAP_W + x];
            int nx, ny;
            if (t != RMAP_SEA && !((t & (RMAP_MARK | RMAP_PICK)) == RMAP_PICK_LAND && RMAP_PICK_MASK(t) != 15))
                continue;
            for (ny = y - 1; ny <= y + 1; ny++)
                for (nx = x - 1; nx <= x + 1; nx++)
                    if (nx >= 0 && ny >= 0 && nx < d->w && ny < d->h)
                        d->tiles[ny * ROGUE_RMAP_W + nx] &= ~RMAP_PICK_INLAND;
        }
    }

    for (pass = 0; pass < 2; pass++)
    {
        for (y = 0; y < d->h; y++)
        {
            for (x = 0; x < d->w; x++)
            {
                u16 *t = &d->tiles[y * ROGUE_RMAP_W + x];
                u32 kind = *t & RMAP_PICK, list, known = 0, group;
                const u8 *exact[4];
                u8 nearTile[4];
                int start[4], end[4], first, last;
                if (!kind || (*t & RMAP_MARK) || (pass > 0 && (*t & RMAP_PICK_INLAND)))
                    continue; // (a fixed tile has RMAP_MARK too)
                if (kind == RMAP_PICK_ROAD)
                    list = RMAP_LIST_ROAD;
                else if (kind == RMAP_PICK_SAND)
                    list = RMAP_LIST_SAND;
                else if (*t & RMAP_PICK_INLAND)
                    list = RMAP_LIST_INLAND;
                else
                    list = RMAP_PICK_MASK(*t);
                group = (kind != RMAP_PICK_LAND) ? 2 : (list < 15); // land, coast or road
                first = sRegionMapListStart[list];
                last = sRegionMapListStart[list + 1];
                p->fit = sRegionMapFit[list];
                p->jitter = RogueRng_Hash(d->r->seed, 28000 + y * 32 + x) & 0xFF;
                p->best = 0;
                p->bestCost = p->bestSeams = 0xFFFF;
                p->west = p->north = 0xFFFF;
                p->shade[0] = d->shade[y * stride + x];
                p->shade[1] = d->shade[y * stride + x + 1];
                p->shade[2] = d->shade[(y + 1) * stride + x];
                p->shade[3] = d->shade[(y + 1) * stride + x + 1];
                // The seams with the tiles round it. Land and coast join the
                // sea and each other; roads join roads. Places, roads and sea
                // routes stand on top with edges of their own.
                for (i = 0; i < 4; i++)
                {
                    int nx = x + sNear[i][0], ny = y + sNear[i][1];
                    u32 o = (nx < 0 || ny < 0 || nx >= d->w || ny >= d->h) ? RMAP_SEA : d->tiles[ny * ROGUE_RMAP_W + nx];
                    u32 oKind = o & RMAP_PICK, e = sRegionMapTileInfo[o & 0xFF].edges;
                    p->seam[i] = sNoSeam;
                    if (((o & RMAP_MARK) && !IsFixedTile(o)) || (oKind && (o & 0xFF) == 0xFF))
                        continue; // not picked yet
                    if (group < 2 ? !(oKind == RMAP_PICK_LAND || o == RMAP_SEA) : (oKind != RMAP_PICK_ROAD && oKind != RMAP_PICK_SAND))
                        continue;
                    switch (i)
                    {
                    case 0:
                        e = (e >> 8) & 0xFF; // its right edge
                        p->seam[0] = sRegionMapSeamH[e];
                        p->west = o & 0xFF;
                        break;
                    case 1:
                        e = e & 0xFF; // its left edge
                        p->seam[1] = sRegionMapSeamHT[e];
                        break;
                    case 2:
                        e = e >> 24; // its bottom edge
                        p->seam[2] = sRegionMapSeamV[e];
                        p->north = o & 0xFF;
                        break;
                    default:
                        e = (e >> 16) & 0xFF; // its top edge
                        p->seam[3] = sRegionMapSeamVT[e];
                        break;
                    }
                    nearTile[known] = o & 0xFF;
                    exact[known] = sExact[group][i];
                    start[known] = sExactStart[group][i][e];
                    end[known] = sExactStart[group][i][e + 1];
                    known++;
                }
                if (pass > 0)
                {
                    u8 current = *t & 0xFF;
                    TryTiles(p, &current, 1);
                    if (p->bestSeams <= 4 * known)
                        continue;
                }
                if (known == 0)
                {
                    TryTiles(p, sRegionMapLists + first, sRegionMapListFree[list]);
                }
                else
                {
                    TryTiles(p, sRegionMapLists + first, sRegionMapListCore[list]);
                    if (p->bestSeams > 4 * known)
                    {
                        for (i = 0; i < (int)known; i++)
                            if (nearTile[i] != RMAP_SEA)
                                TryTiles(p, exact[i] + start[i], end[i] - start[i]);
                    }
                    if (p->bestSeams > 8 * known)
                        TryTiles(p, sRegionMapLists + first, list >= 15 ? sRegionMapListFree[list] : last - first);
                }
                *t = (*t & 0xFF00) | p->best;
            }
        }
    }
}

// Landmarks the map draws on free land, so what the professor says about the
// region shows on it: the volcano's crater and small lakes by lake and river
// routes, as Hoenn's map has Mt. Chimney and the ponds by Routes 104 and
// 120. A free cell holds no place, so no road crosses its tile or the tiles
// east, south and south-east of it.

static bool8 PlaceOnCell(const struct RogueRegion *r, int x, int y)
{
    int i;
    for (i = 0; i < r->nPlaces; i++)
        if (r->places[i].gridX == x && r->places[i].gridY == y)
            return TRUE;
    return FALSE;
}

// TRUE if cell (x, y) is free land with dry land all round it (and no sea
// route within two cells, whose open water clears the land near it), so a
// landmark there stands inland.
static bool8 LandmarkFits(const struct RogueRegion *r, const struct RogueLand *L, int x, int y)
{
    int dx, dy, i;
    u8 c = LandAt(L, x, y);
    if ((c != CELL_LAND && c != CELL_PEAK) || PlaceOnCell(r, x, y))
        return FALSE;
    for (dy = -1; dy <= 1; dy++)
    {
        for (dx = -1; dx <= 1; dx++)
        {
            c = LandAt(L, x + dx, y + dy);
            if (c != CELL_LAND && c != CELL_PEAK)
                return FALSE;
        }
    }
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        dx = p->gridX - x;
        dy = p->gridY - y;
        if (IsSeaRoute(p) && dx >= -2 && dx <= 2 && dy >= -2 && dy <= 2)
            return FALSE;
    }
    return TRUE;
}

// Round a place: behind it (north) first, then beside, then in front.
static const s8 sAroundCell[8][2] = { { 0, -1 }, { 1, -1 }, { -1, -1 }, { 1, 0 }, { -1, 0 }, { 0, 1 }, { 1, 1 }, { -1, 1 } };

// The cell for the volcano's crater: on the land's own volcano (or beside
// it), else beside the first ash town it fits by (or a cell further off),
// so "a volcano looms over one town".
static bool8 MapVolcano(const struct RogueRegion *r, const struct RogueLand *L, s8 *gx, s8 *gy)
{
    int x, y, i, k, far;
    if (L->feature == FEATURE_VOLCANO)
    {
        for (y = L->fy; y < L->fy + L->fh; y++)
            for (x = L->fx; x < L->fx + L->fw; x++)
                if (LandmarkFits(r, L, x, y))
                    goto found;
        for (y = L->fy - 1; y <= L->fy + L->fh; y++)
            for (x = L->fx - 1; x <= L->fx + L->fw; x++)
                if (LandmarkFits(r, L, x, y))
                    goto found;
    }
    for (far = 1; far <= 2; far++)
    {
        for (i = 0; i < r->nPlaces; i++)
        {
            const struct RoguePlace *p = &r->places[i];
            if (p->kind != RPLACE_TOWN || p->theme != RTHEME_ASH)
                continue;
            // The ring of cells far off it, from the north round.
            for (k = 0; k < 8 * far; k++)
            {
                int side = k / (2 * far), step = k % (2 * far);
                static const s8 sStart[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
                static const s8 sStep[4][2] = { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } };
                if (far == 1)
                {
                    x = p->gridX + sAroundCell[k][0];
                    y = p->gridY + sAroundCell[k][1];
                }
                else
                {
                    x = p->gridX + sStart[side][0] * far + sStep[side][0] * step;
                    y = p->gridY + sStart[side][1] * far + sStep[side][1] * step;
                }
                if (LandmarkFits(r, L, x, y))
                    goto found;
            }
        }
    }
    return FALSE;
found:
    *gx = x;
    *gy = y;
    return TRUE;
}

#define MAX_PONDS 3

// TRUE for a gym city or the summit, drawn as a capsule over the tile east
// or south of its own.
static bool8 CityOnCell(const struct RogueRegion *r, int x, int y)
{
    int i;
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        if (p->gridX == x && p->gridY == y)
            return p->kind == RPLACE_TOWN && (p->type == RTOWN_GYM || p->type == RTOWN_SUMMIT);
    }
    return FALSE;
}

// Small lakes: one beside each lake or river route with no lake near it
// yet, up to MAX_PONDS, never beside the crater or where a city's capsule
// would lie on its shore. They become lake cells of the land and are cut in
// with the land's own lake.
static void MapPonds(const struct RogueRegion *r, struct RogueLand *L, int vx, int vy)
{
    int i, k, x, y, dx, dy, n = 0;
    for (i = 0; i < r->nPlaces && n < MAX_PONDS; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        bool8 near = FALSE;
        if (p->kind != RPLACE_ROUTE || (p->type != RROUTE_LAKE && p->type != RROUTE_RIVER))
            continue;
        for (dy = -1; dy <= 1; dy++)
            for (dx = -1; dx <= 1; dx++)
                if (LandAt(L, p->gridX + dx, p->gridY + dy) == CELL_LAKE)
                    near = TRUE;
        for (k = 0; k < 8 && !near; k++)
        {
            x = p->gridX + sAroundCell[(k + 3) & 7][0]; // beside it first
            y = p->gridY + sAroundCell[(k + 3) & 7][1];
            if (vx >= 0 && x >= vx - 1 && x <= vx + 2 && y >= vy - 1 && y <= vy + 2)
                continue;
            if (LandmarkFits(r, L, x, y) && L->cell[y][x] == CELL_LAND && !CityOnCell(r, x - 1, y) && !CityOnCell(r, x, y - 1))
            {
                L->cell[y][x] = CELL_LAKE;
                n++;
                break;
            }
        }
    }
}

// Round a town's north side: straight above first, then the corners (the
// door's side first), then two cells up.
static const s8 sNorthOfTown[8][2] = { { 0, -1 }, { -1, -1 }, { 1, -1 }, { 0, -2 }, { -1, -2 }, { 1, -2 }, { -2, -2 }, { 2, -2 } };

// Relaxed: free land whose tiles (its own and those east, south and
// south-east) are all land, even with the sea beyond it.
static bool8 CraterFitsCoast(const struct RogueRegion *r, const struct RogueLand *L, int x, int y)
{
    int dx, dy, i;
    for (dy = 0; dy <= 1; dy++)
        for (dx = 0; dx <= 1; dx++)
            if (LandAt(L, x + dx, y + dy) != CELL_LAND && LandAt(L, x + dx, y + dy) != CELL_PEAK)
                return FALSE;
    if (PlaceOnCell(r, x, y))
        return FALSE;
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        dx = p->gridX - x;
        dy = p->gridY - y;
        if (IsSeaRoute(p) && dx >= -2 && dx <= 2 && dy >= -2 && dy <= 2)
            return FALSE;
    }
    return TRUE;
}

// A town's volcano is entered by a door in the rock along the top of the
// town, so the map draws the crater on the town's north side when there is
// room for it, as Mt. Chimney stands above Lavaridge. Only the picture
// moves: the volcano is planned from the crater's first cell, and
// PlanVolcano passes over a town the crater cannot be drawn above.
static void MapCraterByDoor(const struct RogueRegion *r, const struct RogueLand *L, s8 *vx, s8 *vy)
{
    int i, k, x, y, dx, dy, x0, w, flip;
    for (i = 0; i < r->nDungeons; i++)
    {
        const struct RogueDungeon *d = &r->dungeons[i];
        const struct RoguePlace *p = &r->places[d->host];
        // (v1.1: and a foot route's, along its top.)
        if (d->kind != RDUN_VOLCANO || (p->kind == RPLACE_TOWN ? TownDungeonEntrance(r, d->host) != RENTRY_MESA
                                                               : !VolcanoMountainAt(r, d->host))
         || (p->gridY > *vy && p->gridY - *vy <= 2 && Abs(p->gridX - *vx) <= 2))
            continue;
        // The door is in the rock on the left or the right of the top.
        if (VolcanoMountainAt(r, d->host))
            flip = VolcanoSpan(r, p, &x0, &w) && 2 * x0 + w > p->w / 2 ? -1 : 1;
        else
            flip = DungeonMesaSpan(r, p, &x0, &w) && 2 * x0 + w > p->w / 2 ? -1 : 1;
        for (k = 0; k < 16; k++)
        {
            bool8 dry = TRUE;
            x = p->gridX + sNorthOfTown[k & 7][0] * flip;
            y = p->gridY + sNorthOfTown[k & 7][1];
            if (k < 8 ? !LandmarkFits(r, L, x, y) : !CraterFitsCoast(r, L, x, y))
                continue;
            // No pond beside the crater's tiles either (as MapPonds keeps).
            for (dy = -1; dy <= 2; dy++)
                for (dx = -1; dx <= 2; dx++)
                    if (LandAt(L, x + dx, y + dy) == CELL_LAKE)
                        dry = FALSE;
            if (dry)
            {
                *vx = x;
                *vy = y;
                return;
            }
        }
    }
}

// The land as the map draws it: the plan's, trimmed, with the ponds. The
// crater's cell, or -1.
static void MapLand(const struct RogueRegion *r, struct RogueLand *L, s8 *vx, s8 *vy)
{
    PlanLand(r->seed, r->land, L);
    TrimLand(L, r);
    if (!MapVolcano(r, L, vx, vy))
        *vx = *vy = -1;
    MapPonds(r, L, *vx, *vy);
}

// The crater's cell: where it was planned (for PlanVolcano), or where the
// map draws it (byDoor).
static bool8 VolcanoCell(const struct RogueRegion *r, s8 *gx, s8 *gy, bool8 byDoor)
{
    struct RogueLand land;
    MapLand(r, &land, gx, gy);
    if (byDoor && *gx >= 0)
        MapCraterByDoor(r, &land, gx, gy);
    return *gx >= 0;
}

bool8 RogueGen_VolcanoCell(const struct RogueRegion *r, s8 *gx, s8 *gy)
{
    return VolcanoCell(r, gx, gy, TRUE);
}

void RogueGen_RegionTraits(const struct RogueRegion *r, u8 *first, u8 *second)
{
    struct RogueLand land;
    s32 n[RTRAIT_COUNT] = {0}, score[RTRAIT_COUNT];
    u32 i, best = 0, next;
    int x, y;
    bool8 lake = FALSE;
    s8 vx, vy;

    MapLand(r, &land, &vx, &vy);
    for (y = 0; y < RGRID_H; y++)
        for (x = 0; x < RGRID_W; x++)
            if (land.cell[y][x] == CELL_LAKE)
                lake = TRUE;
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        if (p->kind != RPLACE_ROUTE)
            continue;
        switch (p->type)
        {
        case RROUTE_LAKE:
        case RROUTE_RIVER:  n[RTRAIT_LAKES]++; break;
        case RROUTE_COAST:
        case RROUTE_SEA:    n[RTRAIT_SEA]++; break;
        case RROUTE_FOREST:
        case RROUTE_WOODS:  n[RTRAIT_WOODS]++; break;
        case RROUTE_HILLS:  n[RTRAIT_ROCK]++; break;
        case RROUTE_DESERT: n[p->theme == RTHEME_ASH ? RTRAIT_ASH : RTRAIT_ROCK]++; break;
        case RROUTE_FIELD:
        case RROUTE_FLOWERS:
        case RROUTE_LONGGRASS: n[RTRAIT_FIELDS]++; break;
        }
    }
    // How far each route count is above the average region, in hundredths
    // of a typical spread (measured over 1500 seeds).
    score[RTRAIT_LAKES] = (n[RTRAIT_LAKES] * 100 - 518) * 100 / 136;
    score[RTRAIT_SEA] = (n[RTRAIT_SEA] * 100 - 314) * 100 / 76;
    score[RTRAIT_WOODS] = (n[RTRAIT_WOODS] * 100 - 552) * 100 / 146;
    score[RTRAIT_ROCK] = (n[RTRAIT_ROCK] * 100 - 337) * 100 / 80;
    score[RTRAIT_ASH] = n[RTRAIT_ASH] ? 250 : -1000;
    score[RTRAIT_FIELDS] = (n[RTRAIT_FIELDS] * 100 - 356) * 100 / 103;
    // Only what the map shows.
    if (vx < 0)
        score[RTRAIT_ASH] = -1000;
    if (!lake)
        score[RTRAIT_LAKES] = -1000;
    for (i = 1; i < RTRAIT_COUNT; i++)
        if (score[i] > score[best])
            best = i;
    next = best == 0 ? 1 : 0;
    for (i = 0; i < RTRAIT_COUNT; i++)
        if (i != best && score[i] > score[next])
            next = i;
    *first = best;
    *second = next;
}

u32 RogueGen_DrawRegionMap(const struct RogueRegion *r, u16 *tiles, s8 *placeX, s8 *placeY, u8 *work, u32 spacing)
{
    struct RegionMapDraw draw, *d = &draw;
    struct RogueLand *L = (struct RogueLand *)work;
    int minX, maxX, minY, maxY, ox, oy, x, y, i, j, gx, gy, n;
    bool8 wide = (spacing == 2);
    s8 vx, vy;
    int craterX = -1, craterY = 0, craterTown = -1;

    MapLand(r, L, &vx, &vy);
    if (vx >= 0)
        MapCraterByDoor(r, L, &vx, &vy);
    minX = L->x0;
    maxX = L->x1;
    minY = L->y0;
    maxY = L->y1;
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        if (p->gridX < minX) minX = p->gridX;
        if (p->gridX > maxX) maxX = p->gridX;
        if (p->gridY < minY) minY = p->gridY;
        if (p->gridY > maxY) maxY = p->gridY;
    }
    // The whole screen at spacing 2, the land in the middle of it with sea
    // round the edge; the compact map is the land and a tile of sea.
    d->r = r;
    d->tiles = tiles;
    d->w = ROGUE_RMAP_VIEW_W;
    d->h = wide ? ROGUE_RMAP_VIEW_H : (maxY - minY) + 3;
    ox = (d->w - (maxX - minX) * (int)spacing - 1) / 2;
    oy = (d->h - (maxY - minY) * (int)spacing - 1) / 2;
    if (ox < 1) ox = 1;
    if (oy < 1) oy = 1;
    n = (d->w + 1) * (d->h + 1);
    d->land = work + ((sizeof(struct RogueLand) + 3) & ~3);
    d->spare = d->land + n;
    d->shade = d->spare + n;
    for (i = 0; i < ROGUE_RMAP_W * ROGUE_RMAP_H; i++)
        tiles[i] = RMAP_SEA;
    for (i = 0; i < n; i++)
        d->land[i] = FALSE;

    // The land of the plan: each land cell holds the corners of its tile,
    // so neighbouring land cells join and a cell of sea between leaves a
    // channel.
    for (gy = 0; gy < RGRID_H; gy++)
    {
        for (gx = 0; gx < RGRID_W; gx++)
        {
            u8 c = L->cell[gy][gx];
            int tx = ox + (gx - minX) * spacing, ty = oy + (gy - minY) * spacing;
            if (c != CELL_LAND && c != CELL_PEAK && c != CELL_LAKE)
                continue;
            for (j = 0; j < 4; j++)
                *Corner(d, d->land, tx + (j & 1), ty + (j >> 1)) = TRUE;
            // Land fills the gap to a land neighbour east, south and
            // south-east, so a block of land has no holes.
            if (wide)
            {
                bool8 e = LandAt(L, gx + 1, gy) != CELL_SEA, s = LandAt(L, gx, gy + 1) != CELL_SEA;
                if (e)
                    *Corner(d, d->land, tx + 2, ty) = *Corner(d, d->land, tx + 2, ty + 1) = TRUE;
                if (s)
                    *Corner(d, d->land, tx, ty + 2) = *Corner(d, d->land, tx + 1, ty + 2) = TRUE;
                if (e && s && LandAt(L, gx + 1, gy + 1) != CELL_SEA)
                    *Corner(d, d->land, tx + 2, ty + 2) = TRUE;
            }
        }
    }

    // Places and the roads between them.
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        u16 m = RMAP_MARK_ROAD;
        placeX[i] = ox + (p->gridX - minX) * spacing;
        placeY[i] = oy + (p->gridY - minY) * spacing;
        if (p->kind == RPLACE_TOWN && p->theme == RTHEME_RAFT)
            m = RMAP_MARK_RAFT;
        else if (p->kind == RPLACE_TOWN)
            m = (p->type == RTOWN_GYM || p->type == RTOWN_SUMMIT) ? RMAP_MARK_CITY : RMAP_MARK_TOWN;
        else if (IsSeaRoute(p))
            m = RMAP_MARK_WATER;
        else if (wide && p->kind == RPLACE_ROUTE && p->type == RROUTE_DESERT)
            m = RMAP_MARK_SAND;
        tiles[placeY[i] * ROGUE_RMAP_W + placeX[i]] = m;
    }
    for (i = 0; i < r->nPlaces && wide; i++)
    {
        for (j = i + 1; j < r->nPlaces; j++)
        {
            const struct RoguePlace *a = &r->places[i], *b = &r->places[j];
            int dx = b->gridX - a->gridX, dy = b->gridY - a->gridY;
            u16 m = RMAP_MARK_ROAD;
            if (dx * dx + dy * dy != 1 || !MapJoined(r, i, j))
                continue;
            if (IsSeaRoute(a) || IsSeaRoute(b))
                m = RMAP_MARK_WATER;
            else if ((a->kind == RPLACE_ROUTE && a->type == RROUTE_DESERT && b->kind != RPLACE_TOWN)
                  || (b->kind == RPLACE_ROUTE && b->type == RROUTE_DESERT && a->kind != RPLACE_TOWN))
                m = RMAP_MARK_SAND;
            tiles[(placeY[i] + dy) * ROGUE_RMAP_W + placeX[i] + dx] = m;
        }
    }

    // Sea that cannot reach the open sea becomes land, then the coast is
    // smoothed and the lakes cut in.
    FillInlandSea(d);
    SmoothCoast(d);
    FillInlandSea(d);
    for (gy = 0; gy < RGRID_H; gy++)
    {
        for (gx = 0; gx < RGRID_W; gx++)
        {
            int tx = ox + (gx - minX) * spacing, ty = oy + (gy - minY) * spacing;
            if (L->cell[gy][gx] != CELL_LAKE)
                continue;
            for (j = 0; j < 4; j++)
                *Corner(d, d->land, tx + (j & 1), ty + (j >> 1)) = FALSE;
            if (wide && LandAt(L, gx + 1, gy) == CELL_LAKE)
                *Corner(d, d->land, tx + 2, ty) = *Corner(d, d->land, tx + 2, ty + 1) = FALSE;
            if (wide && LandAt(L, gx, gy + 1) == CELL_LAKE)
                *Corner(d, d->land, tx, ty + 2) = *Corner(d, d->land, tx + 1, ty + 2) = FALSE;
        }
    }
    // A few small islands out at sea.
    for (i = 0; i < (wide ? 8 : 3); i++)
    {
        int cx = RogueRng_Hash(r->seed, 22000 + i) % (d->w + 1), cy = RogueRng_Hash(r->seed, 22100 + i) % (d->h + 1);
        int size = 2 + RogueRng_Hash(r->seed, 22200 + i) % 3;
        bool8 clear = TRUE;
        for (y = cy - 2; y <= cy + 3 && clear; y++)
            for (x = cx - 2; x <= cx + 3 && clear; x++)
                if (*Corner(d, d->land, x, y) || (MapTile(d, x, y) & RMAP_MARK))
                    clear = FALSE;
        if (!clear)
            continue;
        for (j = 0; j < size; j++)
            *Corner(d, d->land, cx + (j & 1), cy + (j >> 1)) = TRUE;
    }
    // The sea routes out on open water.
    for (y = 0; y < d->h; y++)
        for (x = 0; x < d->w; x++)
            if (MapTile(d, x, y) == RMAP_MARK_WATER || MapTile(d, x, y) == RMAP_MARK_RAFT)
                for (j = 0; j < 16; j++)
                    *Corner(d, d->land, x - 1 + (j & 3), y - 1 + (j >> 2)) = FALSE;
    // Open sea all round the edge, so no coast is cut off by it.
    for (x = 0; x <= d->w; x++)
        *Corner(d, d->land, x, 0) = *Corner(d, d->land, x, d->h) = FALSE;
    for (y = 0; y <= d->h; y++)
        *Corner(d, d->land, 0, y) = *Corner(d, d->land, d->w, y) = FALSE;
    // Everything on land stands on land all round; sea routes on sea.
    for (y = 0; y < d->h; y++)
    {
        for (x = 0; x < d->w; x++)
        {
            u16 t = MapTile(d, x, y);
            if (!(t & RMAP_MARK))
                continue;
            for (j = 0; j < 4; j++)
                *Corner(d, d->land, x + (j & 1), y + (j >> 1)) = (t != RMAP_MARK_WATER && t != RMAP_MARK_RAFT);
        }
    }
    // Land in only two opposite corners has no art: fill the tile in.
    for (y = 0; y < d->h; y++)
    {
        for (x = 0; x < d->w; x++)
        {
            u8 tl = *Corner(d, d->land, x, y), tr = *Corner(d, d->land, x + 1, y);
            u8 bl = *Corner(d, d->land, x, y + 1), br = *Corner(d, d->land, x + 1, y + 1);
            if (tl == br && tr == bl && tl != tr)
                for (j = 0; j < 4; j++)
                    *Corner(d, d->land, x + (j & 1), y + (j >> 1)) = TRUE;
        }
    }

    // What each tile is: a place or a road, open sea, or land and coast to
    // pick with the land corners it should have.
    for (y = 0; y < d->h; y++)
    {
        for (x = 0; x < d->w; x++)
        {
            u16 *t = &tiles[y * ROGUE_RMAP_W + x];
            u32 mask = (*Corner(d, d->land, x, y) << 3) | (*Corner(d, d->land, x + 1, y) << 2)
                     | (*Corner(d, d->land, x, y + 1) << 1) | *Corner(d, d->land, x + 1, y + 1);
            switch (*t)
            {
            case RMAP_MARK_ROAD:  *t = RMAP_PICK_ROAD | 0xFF; break;
            case RMAP_MARK_SAND:  *t = RMAP_PICK_SAND | 0xFF; break;
            case RMAP_MARK_WATER: *t = RMAP_SEA_ROUTE; break;
            case RMAP_MARK_RAFT:  *t = RMAP_RAFT; break;
            case RMAP_MARK_TOWN:  *t = RMAP_TOWN; break;
            case RMAP_MARK_CITY:  *t = RMAP_CITY; break;
            default:
                *t = mask ? (RMAP_PICK_LAND | (mask << 8) | 0xFF) : RMAP_SEA;
                break;
            }
        }
    }
    // Cities as Hoenn draws them: a capsule two tiles long, over the land
    // or road east of them, or south.
    for (i = 0; i < r->nPlaces && wide; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        u16 *t = &tiles[placeY[i] * ROGUE_RMAP_W + placeX[i]];
        if (p->kind != RPLACE_TOWN || (p->type != RTOWN_GYM && p->type != RTOWN_SUMMIT) || p->theme == RTHEME_CRATER)
            continue;
        if (placeX[i] + 1 < d->w && ((t[1] & 0xFF00) == (RMAP_PICK_LAND | 0xF00) || (t[1] & RMAP_PICK) == RMAP_PICK_ROAD))
        {
            t[0] = RMAP_CITY_W;
            t[1] = RMAP_CITY_E;
        }
        else if (placeY[i] + 1 < d->h && ((t[ROGUE_RMAP_W] & 0xFF00) == (RMAP_PICK_LAND | 0xF00) || (t[ROGUE_RMAP_W] & RMAP_PICK) == RMAP_PICK_ROAD))
        {
            t[0] = RMAP_CITY_N;
            t[ROGUE_RMAP_W] = RMAP_CITY_S;
        }
    }
    // The volcano's crater, on light hills as Mt. Chimney's.
    if (wide && vx >= 0)
    {
        int tx = ox + (vx - minX) * 2, ty = oy + (vy - minY) * 2, i0 = 0;
        // The slope goes where the land beside is free (no road or city).
        for (j = 0; j < 2; j++)
            if (tiles[(ty + j) * ROGUE_RMAP_W + tx - 1] != (RMAP_PICK_LAND | (15 << 8) | 0xFF))
                i0 = 1;
        for (j = 0; j < 2; j++)
            for (i = i0; i < 3; i++)
                tiles[(ty + j) * ROGUE_RMAP_W + tx - 1 + i] = RMAP_FIXED | RMAP_PICK_LAND | (15 << 8) | sCraterTiles[j][i];
        L->cell[vy][vx] = CELL_PEAK;
        craterX = tx;
        craterY = ty;
    }
    // (v1.1 towns2) A crater town: its mark in the middle of a crater as
    // Mt. Chimney's (Hoenn's map draws Sootopolis in its ring of rock), the
    // place on the crater's top left tile.
    for (i = 0; i < r->nPlaces && wide && craterTown < 0; i++)
    {
        bool8 free = TRUE;
        if (!RogueGen_IsCraterTown(&r->places[i]) || placeX[i] + 1 >= d->w || placeY[i] + 1 >= d->h)
            continue;
        // (Not over the volcano's crater.)
        for (j = 0; j < 4; j++)
            if (IsFixedTile(tiles[(placeY[i] + (j >> 1)) * ROGUE_RMAP_W + placeX[i] + (j & 1)]))
                free = FALSE;
        if (!free)
            continue;
        craterTown = i;
        for (j = 0; j < 4; j++)
            tiles[(placeY[i] + (j >> 1)) * ROGUE_RMAP_W + placeX[i] + (j & 1)]
                = RMAP_FIXED | RMAP_PICK_LAND | (15 << 8) | sCraterTiles[j >> 1][1 + (j & 1)];
    }
    ShadeLand(d, L, placeX, placeY, ox, oy, minX, minY, spacing);
    PickTiles(d);
    for (y = 0; y < d->h; y++)
        for (x = 0; x < d->w; x++)
            tiles[y * ROGUE_RMAP_W + x] &= 0xFF;
    // The crater joins its neighbours as Mt. Chimney's, and is shown in the
    // orange and red of a volcano.
    if (craterX >= 0)
        for (j = 0; j < 4; j++)
            tiles[(craterY + (j >> 1)) * ROGUE_RMAP_W + craterX + (j & 1)] = RMAP_TILE_CRATER + j;
    if (craterTown >= 0)
        for (j = 0; j < 4; j++)
            tiles[(placeY[craterTown] + (j >> 1)) * ROGUE_RMAP_W + placeX[craterTown] + (j & 1)]
                = RMAP_TILE_CRATER_TOWN + (r->places[craterTown].type == RTOWN_GYM ? 0 : 4) + j;
    return d->h;
}


// ---------------------------------------------------------------------------
// The plan

// The main road, in order.
enum { SP_START, SP_VILLAGE, SP_GYM, SP_SUMMIT, SP_ROUTE, SP_MOUTH_ROUTE, SP_CAVE, SP_VICTORY, SP_SEA,
       SP_RAFT }; // v1.1 towns2: a raft village out on the sea, between two sea routes (src/rogue_gen_towns2.h)
// TRUE for the main road's steps out on the sea: the sea routes and a raft
// village between them.
#define SP_ON_SEA(k) ((k) == SP_SEA || (k) == SP_RAFT)
#define TOWNS2_RAFT_LANDS 6
#define TOWNS2_RAFT_STALL 300 // (v1.1 speed: as SEA2_STALL)
static bool8 Towns2RaftWanted(u32 seed);
static void Towns2Plan(struct RogueRegion *r);
static void Towns2Names(struct RogueRegion *r);

// Planned per region by PlanSpine. The shape follows the real games: two to
// four routes before the first gym, usually with a village between, then one
// to three places between later gyms, a cave on the way to the fourth gym,
// the sea after the fifth, and Victory Road. Leaves room for the side
// places within ROGUE_MAX_PLACES.
#define SPINE_MAX 34
static u8 sSpine[SPINE_MAX];
static u8 sSpineRival[SPINE_MAX]; // rival battle on this route (1 based), or 0
static int sSpineLen;
#define SPINE_LEN sSpineLen

static void PushSpine(u8 kind, u8 rival)
{
    sSpine[sSpineLen] = kind;
    sSpineRival[sSpineLen] = rival;
    sSpineLen++;
}


// v1.1 sea: a second, longer stretch of sea later on in some regions
// (src/rogue_gen_sea.h, SeaSecondStretch): sea2 is 0, or the stretch it
// lies in (6 or 7: after the sixth or seventh gym) | the routes << 4.
static void PlanSpine(u32 *rng, bool8 sea, u8 sea2, bool8 raft)
{
    // Places between gym g and gym g + 1: one to three, two most often.
    static const u8 sGapPick[] = { 1, 2, 2, 3 };
    enum { MIN_EARLY_GAP = 2 };
    u8 gap[ROGUE_NUM_GYMS];
    u8 villageAt[ROGUE_NUM_GYMS]; // route index a village comes before, or 0xFF
    int g, k, first, total, villages = 0, seaRoutes, stuck = 0;

    first = 3 + (RogueRng_Range(rng, 4) == 0); // 3 routes, or 4: with the woods or caves off them, 3 to 6 places to catch in
    for (g = 0; g < ROGUE_NUM_GYMS; g++)
    {
        gap[g] = sGapPick[RogueRng_Range(rng, sizeof(sGapPick))];
        villageAt[g] = 0xFF;
    }
    // Before the fourth gym: a route or two, the last climbing to a cave.
    gap[3] = 1 + RogueRng_Range(rng, 2);
    // Before the second and third gyms, at least two places, as Emerald
    // has Route 116 and Rusturf before Brawly, and Routes 109 and 110
    // before Wattson. One short route of three or four trainers leaves the
    // team three levels under the ace.
    for (g = 1; g <= 2; g++)
        if (gap[g] < MIN_EARLY_GAP)
            gap[g] = MIN_EARLY_GAP;
    // A village like Oldale after the first or second route, most times.
    if (RogueRng_Range(rng, 4) != 0)
        villageAt[0] = 1 + RogueRng_Range(rng, first - 1);
    // Now and then a town without a gym further on, like Slateport.
    for (g = 1; g < ROGUE_NUM_GYMS && villages < 2; g++)
    {
        if (g != 3 && gap[g] >= 2 && RogueRng_Range(rng, 100) < 30)
        {
            villageAt[g] = 1 + RogueRng_Range(rng, gap[g] - 1);
            villages++;
        }
    }
    // v1.1 sea: the second stretch, as Routes 124 to 127 lie between
    // Mossdeep and Sootopolis. After the sixth gym the rival's route comes
    // first, then the sea; it comes ashore at the next gym's town. The
    // trimming below leaves it be.
    if (sea2 && sea)
        SeaSecondGap(gap, villageAt, &sea2);
    // Trim the longest stretches until the road fits.
    for (;;)
    {
        total = 1 + first + 1 + 3; // start, routes, first gym, Victory Road, cave, summit
        for (g = 0; g < ROGUE_NUM_GYMS; g++)
            total += (villageAt[g] != 0xFF);
        for (g = 1; g < ROGUE_NUM_GYMS; g++)
            total += gap[g] + 1 + (g == 3); // places, the gym, and the cave
        if (total <= SPINE_MAX - 2)
            break;
        // (v1.1 sea: with nothing else left to trim, the second stretch
        // of sea loses a route, or is dropped.)
        if (sea2 && ++stuck > 64)
        {
            stuck = 0;
            gap[sea2 & 15]--;
            sea2 = (sea2 >> 4) > 2 ? sea2 - (1 << 4) : 0;
            continue;
        }
        g = 1 + RogueRng_Range(rng, ROGUE_NUM_GYMS - 1);
        if (gap[g] > ((g == 1 || g == 2) ? MIN_EARLY_GAP : 1) && g != 3 && !(sea2 && g == (sea2 & 15)))
        {
            gap[g]--;
            if (villageAt[g] != 0xFF && villageAt[g] >= gap[g])
                villageAt[g] = 0xFF;
        }
    }
    // The sea after the fifth gym: one stretch of open water, now and then
    // two, the first right out of the fifth gym's town.
    seaRoutes = !sea ? 0 : (gap[5] >= 2 && RogueRng_Range(rng, 100) < 60) ? 2 : 1;
    // The sea comes ashore at a town: the sixth gym's, or a village on the
    // shore when the road goes on over land.
    // (v1.1 towns2) A raft village out on the stretch of sea, between two
    // sea routes, as Pacifidlog lies between Routes 131 and 132; the second
    // comes ashore at the sixth gym's town, so the stretch keeps to three
    // places.
    raft = raft && seaRoutes > 0;
    if (raft)
    {
        seaRoutes = 2;
        gap[5] = 2;
        villageAt[5] = 0xFF;
    }
    if (seaRoutes && gap[5] > seaRoutes)
        villageAt[5] = seaRoutes;
    if (!seaRoutes)
        sea2 = 0;
    sSeaSecond = sea2 != 0;

    sSpineLen = 0;
    sSeaSecondAt = sSeaSecondFrom = 0xFF;
    PushSpine(SP_START, 0);
    for (k = 0; k < first; k++)
    {
        if (villageAt[0] == k)
            PushSpine(SP_VILLAGE, 0);
        PushSpine(SP_ROUTE, k == 1 ? 1 : 0); // the rival waits on the second route
    }
    PushSpine(SP_GYM, 0);
    for (g = 1; g < ROGUE_NUM_GYMS; g++)
    {
        if (g == 3)
        {
            // A cave on the way to the fourth gym, the rival at its mouth.
            for (k = 0; k + 1 < gap[g]; k++)
                PushSpine(SP_ROUTE, 0);
            PushSpine(SP_MOUTH_ROUTE, 2);
            PushSpine(SP_CAVE, 0);
        }
        else
        {
            for (k = 0; k < gap[g]; k++)
            {
                if (villageAt[g] == k)
                    PushSpine(SP_VILLAGE, 0);
                if (g == 5 && k < seaRoutes)
                {
                    PushSpine(SP_SEA, 0);
                    // (v1.1 towns2) A raft village between the two sea
                    // routes, as Pacifidlog lies between Routes 131 and 132.
                    if (raft && k == 0)
                        PushSpine(SP_RAFT, 0);
                }
                else if (sea2 && g == (sea2 & 15) && k >= (g == 6)) // v1.1 sea
                {
                    if (sSeaSecondAt == 0xFF)
                        sSeaSecondAt = sSpineLen;
                    PushSpine(SP_SEA, 0);
                }
                else
                    PushSpine(SP_ROUTE, (g == 6 && k == 0) ? 3 : 0); // and again after the sixth gym
            }
        }
        PushSpine(SP_GYM, 0);
    }
    if (sSeaSecondAt != 0xFF)
        for (k = 1; k < sSeaSecondAt; k++)
            if (SP_ON_SEA(sSpine[k - 1]) && !SP_ON_SEA(sSpine[k]))
            {
                sSeaSecondFrom = k;
                break;
            }
    PushSpine(SP_VICTORY, 4);
    PushSpine(SP_CAVE, 0);
    PushSpine(SP_SUMMIT, 0);
}

// Gyms passed along the main road on leaving each place of it, and on
// arriving: the stretch it is part of.
static u8 sSpineBadges[SPINE_MAX];
#define MAX_BLOCKS 6

static u8 SpineArrive(int i)
{
    return sSpineBadges[i] - (sSpine[i] == SP_GYM);
}

static int NewPlace(struct RogueRegion *r, u8 kind, u8 type, int x, int y)
{
    struct RoguePlace *p = &r->places[r->nPlaces];
    memset(p, 0, sizeof(*p));
    p->kind = kind;
    p->type = type;
    p->gridX = x;
    p->gridY = y;
    p->gym = 0xFF;
    p->area = 0xFF;
    p->mouth = 0xFF;
    p->caveIn = p->caveOut = 0xFF;
    p->guardDir = RDIR_NONE;
    p->seaSide = RDIR_NONE;
    return r->nPlaces++;
}

static void Link(struct RogueRegion *r, int a, int b, u8 dir)
{
    struct RoguePlace *pa = &r->places[a], *pb = &r->places[b];
    pa->links[pa->nLinks].dir = dir;
    pa->links[pa->nLinks].target = b;
    pa->nLinks++;
    pb->links[pb->nLinks].dir = Opposite(dir);
    pb->links[pb->nLinks].target = a;
    pb->nLinks++;
}

// A cave between two places: in through a mouth at the top of below, out
// through a mouth at the top of above (or a dead end).
static void LinkCave(struct RogueRegion *r, int below, int cave, int above)
{
    r->places[below].mouth = cave;
    r->places[cave].caveIn = below;
    if (above >= 0)
    {
        r->places[above].mouth = cave;
        r->places[cave].caveOut = above;
    }
}

static bool8 HasSide(const struct RoguePlace *p, u8 dir)
{
    return LinkOnSide(p, dir) != NULL || (dir == RDIR_NORTH && p->mouth != 0xFF);
}

// While the plan is laid: the land, which place stands on each cell, the
// main road's cells and the moves still to try at each step (two bits a
// move). It lives in the map generator's search queue, which is idle while a
// region is built (the game stops any map job first), so it costs no EWRAM.
struct LayWork
{
    struct RogueLand land;
    u8 occ[RGRID_H][RGRID_W]; // place + 1, or 0
    u8 occSave[RGRID_H][RGRID_W]; // occ before the extras, to try them again
    s8 x[SPINE_MAX], y[SPINE_MAX];
    u8 moves[SPINE_MAX], n[SPINE_MAX], k[SPINE_MAX];
    s8 rx[SPINE_MAX], ry[SPINE_MAX]; // a crossroads town's cell kept free for a way out
    s8 bx[SPINE_MAX], by[SPINE_MAX], brx[SPINE_MAX], bry[SPINE_MAX]; // the best main road laid so far
    // What ends each chapter, in order along the main road (see PlanChapters).
    u8 nBlocks;
    u8 blockAt[MAX_BLOCKS];    // step of the main road the block keeps out: a route, the sea, Victory Road
    u8 blockGym[MAX_BLOCKS];   // the gym whose chapter it ends
    u8 blockKind[MAX_BLOCKS];  // enum RogueObstacle on the road, or ROBST_NONE for the League's gate
    u8 blockKeys[MAX_BLOCKS];  // the keys that pass it
    u8 blockFork[MAX_BLOCKS];  // a second way on, behind this obstacle, or ROBST_NONE
    u8 keysSave[MAX_BLOCKS];   // blockKeys before the extras, to try them again
};
typedef char LayWorkFits[(sizeof(struct LayWork) <= sizeof(sBfsQueue)) ? 1 : -1];
#define sLayWork ((struct LayWork *)sBfsQueue)
#define sLand (sLayWork->land)
#define sOcc (sLayWork->occ)
#define sLayX (sLayWork->x)
#define sLayY (sLayWork->y)
#define sLayMoves (sLayWork->moves)
#define sLayN (sLayWork->n)
#define sLayK (sLayWork->k)
#define RESERVED 0xFF // in sOcc: kept free beside a crossroads town

#define sNBlocks (sLayWork->nBlocks)
#define sBlockAt (sLayWork->blockAt)
#define sBlockGym (sLayWork->blockGym)
#define sBlockKind (sLayWork->blockKind)
#define sBlockKeys (sLayWork->blockKeys)
#define sBlockFork (sLayWork->blockFork)

// The steps of the main road that are crossroads towns, or -1.
static s8 sHubStep[2];

static bool8 CellIs(int x, int y, u8 kind)
{
    return x >= 0 && y >= 0 && x < RGRID_W && y < RGRID_H && !sOcc[y][x] && sLand.cell[y][x] == kind;
}

// A free cell next to (x, y) of the given kind, other than (ax, ay).
static bool8 FreeNear(int x, int y, u8 kind, int ax, int ay)
{
    u8 d;
    for (d = 0; d < 4; d++)
    {
        int nx = x + sDirDX[d], ny = y + sDirDY[d];
        if ((nx != ax || ny != ay) && CellIs(nx, ny, kind))
            return TRUE;
    }
    return FALSE;
}

// The way the main road should go: a few waypoints across the land (a ring
// round the middle that turns in at the end, a snake back and forth, or
// columns up and down), each turned and flipped per region.
#define GUIDE_MAX 9
#define SPINE_TRIES 12
struct Guide
{
    u8 n;
    s8 x[GUIDE_MAX], y[GUIDE_MAX];
    int len;
};

static void GuideAt(const struct Guide *g, int num, int den, int *ox, int *oy)
{
    int want = den ? g->len * num / den : 0, k;
    for (k = 0; k + 1 < g->n; k++)
    {
        int seg = Abs(g->x[k + 1] - g->x[k]) + Abs(g->y[k + 1] - g->y[k]);
        if (want <= seg || k + 2 == g->n)
        {
            if (want > seg)
                want = seg;
            *ox = seg ? g->x[k] + (g->x[k + 1] - g->x[k]) * want / seg : g->x[k];
            *oy = seg ? g->y[k] + (g->y[k + 1] - g->y[k]) * want / seg : g->y[k];
            return;
        }
        want -= seg;
    }
    *ox = g->x[0];
    *oy = g->y[0];
}

static void GuideAdd(struct Guide *g, int x, int y)
{
    if (g->n > 0)
        g->len += Abs(x - g->x[g->n - 1]) + Abs(y - g->y[g->n - 1]);
    g->x[g->n] = x;
    g->y[g->n] = y;
    g->n++;
}

static void MakeGuide(u32 *rng, struct Guide *g)
{
    const struct RogueLand *L = &sLand;
    int ax0 = L->x0 + 1, ax1 = L->x1 - 1, ay0 = L->y0 + 1, ay1 = L->y1 - 1, k, t;
    int cx = (ax0 + ax1) / 2, cy = (ay0 + ay1) / 2;
    bool8 flipX = RogueRng_Range(rng, 2), flipY = RogueRng_Range(rng, 2);
    s8 px[GUIDE_MAX], py[GUIDE_MAX];
    int n = 0;
    g->n = 0;
    g->len = 0;
    t = RogueRng_Range(rng, 10);
    if (t < 5)
    {
        // A ring round the middle, starting at a corner, turning in to the
        // middle at the end, where Victory Road climbs.
        static const u8 sCornerX[4] = { 0, 1, 1, 0 }, sCornerY[4] = { 0, 0, 1, 1 };
        int start = RogueRng_Range(rng, 4), dir = RogueRng_Range(rng, 2) ? 1 : 3;
        for (k = 0; k < 4; k++)
        {
            int c = (start + dir * k) & 3;
            px[n] = sCornerX[c] ? ax1 : ax0;
            py[n] = sCornerY[c] ? ay1 : ay0;
            n++;
        }
        px[n] = (px[3] + px[0]) / 2;
        py[n] = (py[3] + py[0]) / 2;
        n++;
        px[n] = cx;
        py[n] = cy;
        n++;
    }
    else if (t < 8)
    {
        // A snake: across, back and across again.
        px[0] = ax0; py[0] = ay0;
        px[1] = ax1; py[1] = ay0;
        px[2] = ax1; py[2] = cy;
        px[3] = ax0; py[3] = cy;
        px[4] = ax0; py[4] = ay1;
        px[5] = ax1; py[5] = ay1;
        n = 6;
    }
    else
    {
        // Columns: up and down across the land.
        int cols = (ax1 - ax0) >= 9 ? 4 : 3;
        for (k = 0; k < cols; k++)
        {
            int x = ax0 + (ax1 - ax0) * k / (cols - 1);
            px[n] = x; py[n] = (k & 1) ? ay0 : ay1; n++;
            px[n] = x; py[n] = (k & 1) ? ay1 : ay0; n++;
        }
    }
    for (k = 0; k < n; k++)
        GuideAdd(g, flipX ? L->x0 + L->x1 - px[k] : px[k], flipY ? L->y0 + L->y1 - py[k] : py[k]);
}

static u8 DirTo(int x0, int y0, int x1, int y1)
{
    return x1 > x0 ? RDIR_EAST : x1 < x0 ? RDIR_WEST : y1 < y0 ? RDIR_NORTH : RDIR_SOUTH;
}

// TRUE if step i of the main road may stand at (x, y), reached by moving d
// from step i - 1. (Inlined: LaySpine tries it tens of thousands of times
// on a hard land.)
static inline __attribute__((always_inline)) bool8 SpineStepOk(int i, int x, int y, u8 d)
{
    u8 s = sSpine[i], ps = sSpine[i - 1];
    if (x < 0 || y < 0 || x >= RGRID_W || y >= RGRID_H || sOcc[y][x])
        return FALSE;
    if (s == SP_CAVE)
        return d == RDIR_NORTH && (sLand.cell[y][x] == CELL_LAND || sLand.cell[y][x] == CELL_PEAK);
    // (v1.1 sea: the second stretch may also lie on the shore's land,
    // which goes under the sea: TrimLand.)
    if (sLand.cell[y][x] != (SP_ON_SEA(s) ? CELL_SEA : CELL_LAND)
     && !(s == SP_SEA && i >= sSeaSecondAt && SeaShoreCell(x, y)))
        return FALSE;
    if (ps == SP_CAVE)
        return TRUE; // beside the cave, which it leads out of
    // Out of a place with a cave mouth along its top: not north.
    if (i >= 2 && sSpine[i - 2] == SP_CAVE && d == RDIR_NORTH)
        return FALSE;
    if (s == SP_MOUTH_ROUTE || s == SP_VICTORY)
    {
        // Into a route whose mouth leads north: not from the north, and
        // room on the far side for the cave and the place beyond.
        if (d == RDIR_SOUTH || !(CellIs(x, y - 1, CELL_LAND) || CellIs(x, y - 1, CELL_PEAK)))
            return FALSE;
        if (!FreeNear(x, y - 1, CELL_LAND, x, y))
            return FALSE;
    }
    // The sea: a town on the shore, the sea route beside it, land beyond.
    if (i + 1 < SPINE_LEN && SP_ON_SEA(sSpine[i + 1]) && !FreeNear(x, y, CELL_SEA, -1, -1)
     && !(i + 1 >= sSeaSecondAt && SeaFreeShoreNear(x, y)))
        return FALSE;
    if (SP_ON_SEA(s) && !FreeNear(x, y, (i + 1 < SPINE_LEN && SP_ON_SEA(sSpine[i + 1])) ? CELL_SEA : CELL_LAND, -1, -1)
     && !(i >= sSeaSecondAt && i + 1 < SPINE_LEN && sSpine[i + 1] == SP_SEA && SeaFreeShoreNear(x, y)))
        return FALSE;
    // v1.1 sea: the second stretch keeps along the coast, so it can come
    // ashore again (the sea is one to three cells deep).
    if (s == SP_SEA && i >= sSeaSecondAt && !SeaByLand(x, y))
        return FALSE;
    // A sea route that turns has open sea in the corner it turns round, so
    // the coasts on its two sides never meet and it cannot be walked.
    if (SP_ON_SEA(ps) && i >= 2 && d != DirTo(sLayX[i - 2], sLayY[i - 2], sLayX[i - 1], sLayY[i - 1])
     && LandAt(&sLand, sLayX[i - 2] + sDirDX[d], sLayY[i - 2] + sDirDY[d]) != CELL_SEA)
        return FALSE;
    return TRUE;
}

// Lays the main road on the land along the guide, as a walk that never
// steps on itself, backing up from dead ends. FALSE if it runs out of steps,
// or (stall not 0) if it goes stall steps without getting further along the
// road than it has been: a walk stuck that way backs up over the same
// stretch until the budget is spent, seconds on the GBA (v1.1 speed).
static bool8 LaySpine(u32 *rng, const struct Guide *g, int budget, int stall)
{
    int i, x, y, bestX = 0, bestY = 0, best = 9999, tx, ty, deepest = 0, since = 0;
    s8 aimX[SPINE_MAX], aimY[SPINE_MAX];

    // The guide's point for each step, worked out once: on a land that
    // will not take the road the walk backs up over a step many times.
    for (i = 1; i < SPINE_LEN; i++)
    {
        GuideAt(g, i, SPINE_LEN - 1, &tx, &ty);
        aimX[i] = tx;
        aimY[i] = ty;
    }
    // v1.1 sea: the second stretch of sea aims for the coast.
    SeaSecondAims(aimX, aimY);

    memset(sOcc, 0, sizeof(sOcc));
    GuideAt(g, 0, 1, &tx, &ty);
    for (y = 0; y < RGRID_H; y++)
    {
        for (x = 0; x < RGRID_W; x++)
        {
            int score = (Abs(x - tx) + Abs(y - ty)) * 4 + RogueRng_Range(rng, 3);
            if (sLand.cell[y][x] == CELL_LAND && FreeNear(x, y, CELL_LAND, -1, -1) && score < best)
            {
                best = score;
                bestX = x;
                bestY = y;
            }
        }
    }
    sLayX[0] = bestX;
    sLayY[0] = bestY;
    sOcc[bestY][bestX] = 1;
    i = 1;
    sLayN[1] = 0xFF;
    while (i < SPINE_LEN)
    {
        if (budget-- <= 0)
            return FALSE;
        if (i > deepest)
        {
            deepest = i;
            since = 0;
        }
        else if (stall && ++since > stall)
            return FALSE;
        if (sLayN[i] == 0xFF)
        {
            // The moves from here, best first: toward the guide's point for
            // this step, off the cells beside other places.
            u8 moves[4];
            int scores[4], n = 0, a, b;
            u8 d;
            tx = aimX[i];
            ty = aimY[i];
            for (d = 0; d < 4; d++)
            {
                int nx = sLayX[i - 1] + sDirDX[d], ny = sLayY[i - 1] + sDirDY[d], score, k, touch = 0;
                if (!SpineStepOk(i, nx, ny, d))
                    continue;
                for (k = 0; k < 4; k++)
                {
                    int ax = nx + sDirDX[k], ay = ny + sDirDY[k];
                    if (ax >= 0 && ay >= 0 && ax < RGRID_W && ay < RGRID_H && sOcc[ay][ax] && sOcc[ay][ax] != i)
                        touch++;
                }
                score = (Abs(nx - tx) + Abs(ny - ty)) * 4 + touch * 16 + RogueRng_Range(rng, 5);
                // (v1.1 sea: between the two stretches of sea, the road
                // keeps to the coast.)
                if (i >= sSeaSecondFrom && i < sSeaSecondAt && SeaBySea(nx, ny))
                    score -= SEA2_COAST;
                // Roads run straight for a while, as Hoenn's do.
                if (i >= 2 && nx - sLayX[i - 1] == sLayX[i - 1] - sLayX[i - 2] && ny - sLayY[i - 1] == sLayY[i - 1] - sLayY[i - 2])
                    score -= 3;
                for (a = n; a > 0 && scores[a - 1] > score; a--)
                {
                    scores[a] = scores[a - 1];
                    moves[a] = moves[a - 1];
                }
                scores[a] = score;
                moves[a] = d;
                n++;
            }
            sLayMoves[i] = 0;
            for (b = 0; b < n; b++)
                sLayMoves[i] |= moves[b] << (2 * b);
            sLayN[i] = n;
            sLayK[i] = 0;
        }
        if (sLayK[i] < sLayN[i])
        {
            u8 d = (sLayMoves[i] >> (2 * sLayK[i])) & 3;
            sLayK[i]++;
            sLayX[i] = sLayX[i - 1] + sDirDX[d];
            sLayY[i] = sLayY[i - 1] + sDirDY[d];
            sOcc[sLayY[i]][sLayX[i]] = i + 1;
            if (i == sHubStep[0] || i == sHubStep[1])
            {
                // A crossroads town keeps a cell of land free beside it
                // for a way out, besides the one the road goes on by.
                u8 k, free = 0, pick, start = RogueRng_Range(rng, 4);
                for (k = 0; k < 4; k++)
                    free += CellIs(sLayX[i] + sDirDX[k], sLayY[i] + sDirDY[k], CELL_LAND);
                if (free < 2)
                {
                    sOcc[sLayY[i]][sLayX[i]] = 0;
                    continue;
                }
                for (k = 0; k < 4; k++)
                {
                    pick = (start + k) & 3;
                    if (CellIs(sLayX[i] + sDirDX[pick], sLayY[i] + sDirDY[pick], CELL_LAND))
                        break;
                }
                sLayWork->rx[i] = sLayX[i] + sDirDX[pick];
                sLayWork->ry[i] = sLayY[i] + sDirDY[pick];
                sOcc[sLayWork->ry[i]][sLayWork->rx[i]] = RESERVED;
            }
            i++;
            if (i < SPINE_LEN)
                sLayN[i] = 0xFF;
        }
        else
        {
            sLayN[i] = 0xFF;
            i--;
            if (i == 0)
                return FALSE;
            sOcc[sLayY[i]][sLayX[i]] = 0;
            if (i == sHubStep[0] || i == sHubStep[1])
                sOcc[sLayWork->ry[i]][sLayWork->rx[i]] = 0;
        }
    }
    return TRUE;
}

// Stretches of the laid main road side by side on the grid that do not
// meet: on the region map, roads a tile apart with a sliver of land between.
static int SpineCrowd(void)
{
    int i, j, n = 0;
    for (i = 0; i < SPINE_LEN; i++)
        for (j = i + 2; j < SPINE_LEN; j++)
            if (Abs(sLayX[i] - sLayX[j]) + Abs(sLayY[i] - sLayY[j]) == 1)
                n++;
    return n;
}

// How many columns the laid main road spans.
static int SpineSpan(void)
{
    int i, x0 = RGRID_W, x1 = 0;
    for (i = 0; i < SPINE_LEN; i++)
    {
        if (sLayX[i] < x0) x0 = sLayX[i];
        if (sLayX[i] > x1) x1 = sLayX[i];
    }
    return x1 - x0;
}


// The places of the main road, from the laid cells.
static void BuildSpine(struct RogueRegion *r, u32 *rng, u8 *spinePlace)
{
    int i;
    r->nPlaces = 0;
    for (i = 0; i < SPINE_LEN; i++)
    {
        int x = sLayX[i], y = sLayY[i], place;
        switch (sSpine[i])
        {
        case SP_START:   place = NewPlace(r, RPLACE_TOWN, RTOWN_START, x, y); break;
        case SP_VILLAGE: place = NewPlace(r, RPLACE_TOWN, RTOWN_VILLAGE, x, y); break;
        case SP_GYM:     place = NewPlace(r, RPLACE_TOWN, RTOWN_GYM, x, y); break;
        case SP_SUMMIT:  place = NewPlace(r, RPLACE_TOWN, RTOWN_SUMMIT, x, y); break;
        case SP_CAVE:    place = NewPlace(r, RPLACE_CAVE, 0, x, y); break;
        case SP_VICTORY: place = NewPlace(r, RPLACE_ROUTE, RROUTE_MOUNTAIN, x, y); break;
        case SP_SEA:     place = NewPlace(r, RPLACE_ROUTE, RROUTE_SEA, x, y); break;
        case SP_RAFT:    place = NewPlace(r, RPLACE_TOWN, RTOWN_VILLAGE, x, y); r->places[place].theme = RTHEME_RAFT; break;
        default:         place = NewPlace(r, RPLACE_ROUTE, RogueRng_Range(rng, 100) < 35 ? RROUTE_LAKE : RROUTE_FOREST, x, y); break;
        }
        spinePlace[i] = place;
        sOcc[y][x] = place + 1;
        if (i == 0 || sSpine[i] == SP_CAVE)
            continue;
        if (sSpine[i - 1] == SP_CAVE)
            LinkCave(r, spinePlace[i - 2], spinePlace[i - 1], place);
        else
            Link(r, spinePlace[i - 1], place, DirTo(sLayX[i - 1], sLayY[i - 1], x, y));
    }
    // The way on out of the first town needs a starter, and the way out of
    // the last gym town to Victory Road all eight badges, as in Emerald.
    for (i = 0; i + 1 < SPINE_LEN; i++)
    {
        struct RoguePlace *p = &r->places[spinePlace[i]];
        struct RogueLink *l;
        if (sSpine[i] != SP_START && !(sSpine[i] == SP_GYM && sSpine[i + 1] == SP_VICTORY))
            continue;
        l = (struct RogueLink *)RogueGen_FindLink(p, spinePlace[i + 1]);
        if (l != NULL)
            l->gate = (sSpine[i] == SP_START) ? ROGUE_GATE_STARTER : ROGUE_GATE_LEAGUE;
    }
}

// ---------------------------------------------------------------------------
// Chapters and keys
//
// The eight gyms fall into chapters of one to three, in order along the main
// road, and the gyms of a chapter can be won in any order. A block on the
// main road ends each chapter: small trees, a cracked rock or a boulder that
// the badge of one of its gyms clears, or the sea after the fifth gym's
// town, which Surf crosses. Some chapters have a second way on, a fork
// behind the obstacle another of their badges clears, so the first badge won
// picks the way. The last chapter ends at the League's gate, which needs all
// eight badges. Which badge clears what is rolled per region: Cut, Rock
// Smash and Strength in any order, Surf from one of the five gyms before the
// sea. A block's key always comes from a gym of the chapter it ends, so no
// order of winning the gyms can leave the player stuck.

static int GymStep(int g)
{
    int i, n = 0;
    for (i = 0; i < SPINE_LEN; i++)
        if (sSpine[i] == SP_GYM && n++ == g)
            return i;
    return SPINE_LEN - 1;
}

// Steps between gym g's town and the next gym's where an obstacle can block
// the main road: a plain route reached over a map edge, with no rival.
static int BlockSpots(int g, u8 *out)
{
    int i, n = 0, b = g + 1 < ROGUE_NUM_GYMS ? GymStep(g + 1) : SPINE_LEN;
    for (i = GymStep(g) + 1; i < b; i++)
        if (sSpine[i] == SP_ROUTE && !sSpineRival[i] && sSpine[i - 1] != SP_CAVE)
            out[n++] = i;
    return n;
}

static int SeaStep(void)
{
    int i;
    for (i = 0; i < SPINE_LEN; i++)
        if (sSpine[i] == SP_SEA)
            return i;
    return -1;
}

// Splits gyms first to first + n - 1 into chapters: ends[g] is set where one
// ends. The first chapter takes firstSize gyms, if given.
static void SplitGyms(u32 *rng, int first, int n, u8 *ends, int firstSize)
{
    static const u8 sSizes[] = { 1, 2, 2, 3 };
    int g = first, left = n, size;
    while (left > 0)
    {
        if (firstSize && g == first)
            size = firstSize;
        else if (left <= 3)
        {
            size = (left == 1 || RogueRng_Range(rng, 100) < 45) ? left : 1 + RogueRng_Range(rng, left - 1);
        }
        else
        {
            size = sSizes[RogueRng_Range(rng, sizeof(sSizes))];
        }
        if (size > left)
            size = left;
        g += size;
        left -= size;
        if (left > 0)
            ends[g - 1] = TRUE;
    }
}

// A random gym of the chapter that gym g is in with no key yet, or -1.
static int KeylessGym(const struct RogueRegion *r, u32 *rng, int g, int but)
{
    int k, n = 0, pick = -1;
    for (k = 0; k < ROGUE_NUM_GYMS; k++)
        if (r->gymChapter[k] == r->gymChapter[g] && r->gymKey[k] == ROBST_NONE && k != but
         && RogueRng_Range(rng, ++n) == 0)
            pick = k;
    return pick;
}

// The chapters, the blocks that end them and which gym gives which key.
// Where each block stands is picked once the road is laid (PlaceBlocks).
static void PlanChapters(struct RogueRegion *r, u32 *rng)
{
    u8 ends[ROGUE_NUM_GYMS], spots[SPINE_MAX], hm[3] = { ROBST_CUT, ROBST_ROCK_SMASH, ROBST_STRENGTH }, order[MAX_BLOCKS];
    int sea = SeaStep(), g, j, k, tries, land, used = 0, firstSize;
    u32 roll = RogueRng_Range(rng, 100);

    // The first chapter is short most times, so the start stays gentle.
    firstSize = roll < 45 ? 1 : (roll < 90 ? 2 : 3);
    for (tries = 0; tries < 60; tries++)
    {
        memset(ends, 0, sizeof(ends));
        SplitGyms(rng, 0, 5, ends, tries < 30 ? firstSize : 0);
        SplitGyms(rng, 5, ROGUE_NUM_GYMS - 5, ends, 0);
        ends[4] = TRUE; // the sea, or an obstacle in its place
        for (g = 0, land = 0; g + 1 < ROGUE_NUM_GYMS; g++)
        {
            if (!ends[g] || (g == 4 && sea >= 0))
                continue;
            if (!BlockSpots(g, spots) && tries < 59)
                break;
            land += (g != 4);
        }
        if (g + 1 == ROGUE_NUM_GYMS && land <= 3)
            break;
    }
    // A chapter that cannot end where it was rolled runs on into the next.
    for (g = 0, land = 0; g + 1 < ROGUE_NUM_GYMS; g++)
    {
        if (ends[g] && !(g == 4 && sea >= 0) && (!BlockSpots(g, spots) || (g != 4 && land >= 3)))
            ends[g] = FALSE;
        land += ends[g] && g != 4;
    }
    r->nChapters = 0;
    for (g = 0; g < ROGUE_NUM_GYMS; g++)
    {
        r->gymChapter[g] = r->nChapters;
        r->gymKey[g] = ROBST_NONE;
        r->nChapters += ends[g] || g + 1 == ROGUE_NUM_GYMS;
    }

    // Cut, Rock Smash and Strength in a random order: the first blocks on
    // the road take them in turn.
    for (k = 2; k > 0; k--)
    {
        u8 t, x = RogueRng_Range(rng, k + 1);
        t = hm[k];
        hm[k] = hm[x];
        hm[x] = t;
    }
    sNBlocks = 0;
    for (g = 0; g + 1 < ROGUE_NUM_GYMS; g++)
    {
        int giver;
        if (!ends[g])
            continue;
        j = sNBlocks++;
        sBlockGym[j] = g;
        sBlockFork[j] = ROBST_NONE;
        sBlockAt[j] = 0xFF;
        if (g == 4)
            sBlockKind[j] = ROBST_SURF;
        else
            sBlockKind[j] = hm[used++];
        sBlockKeys[j] = ROGUE_KEY(sBlockKind[j]);
        if (g == 4 && sea >= 0)
        {
            sBlockAt[j] = sea;
            continue;
        }
        giver = KeylessGym(r, rng, g, -1);
        r->gymKey[giver] = sBlockKind[j];
    }
    // A second way on from some chapters, behind the obstacle of another of
    // their badges.
    for (j = 0; j < sNBlocks; j++)
        order[j] = j;
    for (j = sNBlocks - 1; j > 0; j--)
    {
        u8 t, x = RogueRng_Range(rng, j + 1);
        t = order[j];
        order[j] = order[x];
        order[x] = t;
    }
    for (k = 0; k < sNBlocks && used < 3; k++)
    {
        int giver;
        j = order[k];
        if (sBlockKind[j] == ROBST_SURF || RogueRng_Range(rng, 100) >= 60)
            continue;
        giver = KeylessGym(r, rng, sBlockGym[j], -1);
        if (giver < 0)
            continue;
        sBlockFork[j] = hm[used++];
        r->gymKey[giver] = sBlockFork[j];
    }
    // The other field moves from any gym, to open side paths, and Surf from
    // one of the five before the sea.
    while (used < 3)
    {
        g = RogueRng_Range(rng, ROGUE_NUM_GYMS);
        if (r->gymKey[g] == ROBST_NONE)
            r->gymKey[g] = hm[used++];
    }
    if (sea >= 0)
    {
        do
            g = RogueRng_Range(rng, 5);
        while (r->gymKey[g] != ROBST_NONE);
        r->gymKey[g] = ROBST_SURF;
    }
    else if (!ends[4])
    {
        // No sea, and no block in its place: Surf from any gym, for the
        // lakes off the road.
        do
            g = RogueRng_Range(rng, ROGUE_NUM_GYMS);
        while (r->gymKey[g] != ROBST_NONE);
        r->gymKey[g] = ROBST_SURF;
    }
    // The League's gate.
    j = sNBlocks++;
    sBlockAt[j] = GymStep(ROGUE_NUM_GYMS - 1) + 1;
    sBlockGym[j] = ROGUE_NUM_GYMS - 1;
    sBlockKind[j] = ROBST_NONE;
    sBlockKeys[j] = ROGUE_KEY_LEAGUE;
    sBlockFork[j] = ROBST_NONE;
}

// Once the road is laid: the route each obstacle stands on, just out of the
// gym town or further along, at its way in. Water is only crossed going
// north or south. FALSE if one has nowhere to stand.
static bool8 PlaceBlocks(struct RogueRegion *r, u32 *rng, const u8 *spinePlace)
{
    u8 spots[SPINE_MAX];
    int j, k, n;
    for (j = 0; j < sNBlocks; j++)
    {
        struct RoguePlace *p;
        u8 d;
        if (sBlockKind[j] == ROBST_NONE || (sBlockKind[j] == ROBST_SURF && SeaStep() >= 0))
            continue;
        n = BlockSpots(sBlockGym[j], spots);
        if (sBlockKind[j] == ROBST_SURF)
        {
            for (k = 0; k < n; k++)
            {
                d = DirTo(sLayX[spots[k] - 1], sLayY[spots[k] - 1], sLayX[spots[k]], sLayY[spots[k]]);
                if (d != RDIR_NORTH && d != RDIR_SOUTH)
                    spots[k--] = spots[--n];
            }
        }
        if (n == 0)
            return FALSE;
        // Right out of the gym town half the time, else further on.
        k = RogueRng_Range(rng, 2) ? 0 : RogueRng_Range(rng, n);
        sBlockAt[j] = spots[k];
        p = &r->places[spinePlace[spots[k]]];
        p->obstacle = sBlockKind[j];
        p->guardDir = Opposite(DirTo(sLayX[spots[k] - 1], sLayY[spots[k] - 1], sLayX[spots[k]], sLayY[spots[k]]));
    }
    return TRUE;
}

// How many blocks lie on the main road up to step i: the chapter it is in.
static int Zone(int i)
{
    int j, z = 0;
    for (j = 0; j < sNBlocks; j++)
        z += sBlockAt[j] <= i;
    return z;
}

// TRUE if the keys pass every block before zone z.
static bool8 ZoneOpen(int z, u8 keys)
{
    int j;
    for (j = 0; j < z; j++)
        if (!(sBlockKeys[j] & keys))
            return FALSE;
    return TRUE;
}

// The gym whose badge clears an obstacle, or -1.
static int KeyGym(const struct RogueRegion *r, u8 obstacle)
{
    int g;
    for (g = 0; g < ROGUE_NUM_GYMS; g++)
        if (r->gymKey[g] == obstacle)
            return g;
    return -1;
}

// Gyms passed along the main road once the badge that clears an obstacle
// is won: the stretch where that way can first open.
static int KeyStage(const struct RogueRegion *r, u8 obstacle)
{
    int g = KeyGym(r, obstacle);
    return g < 0 ? ROGUE_NUM_GYMS : g + 1;
}

u8 RogueGen_KeysWon(const struct RogueRegion *r, u8 won)
{
    u8 keys = 0;
    int g;
    for (g = 0; g < ROGUE_NUM_GYMS; g++)
        if ((won >> g) & 1)
            keys |= r->gymKey[g] ? ROGUE_KEY(r->gymKey[g]) : 0;
    if (won == (1 << ROGUE_NUM_GYMS) - 1)
        keys |= ROGUE_KEY_LEAGUE;
    if (r->tunnel < r->nDungeons && ROGUE_GATE_KIND(r->tunnelGate) == RGATE_GIFT && ((won >> ROGUE_GATE_SRC(r->tunnelGate)) & 1))
        keys |= ROGUE_KEY_TUNNEL;
    return keys;
}

// The keys that open the region's tunnel.
static u8 TunnelKeys(const struct RogueRegion *r)
{
    switch (ROGUE_GATE_KIND(r->tunnelGate))
    {
    case RGATE_FIELD:   return ROGUE_KEY(ROGUE_GATE_SRC(r->tunnelGate));
    case RGATE_VILLAIN: return ROGUE_KEY_VILLAIN;
    default:            return ROGUE_KEY_TUNNEL;
    }
}

// The keys needed to walk from place u through link l: the League's gate,
// the villains' blockade, an obstacle on the way in (or out past it from
// behind), Surf for the sea.
static u8 LinkKeys(const struct RogueRegion *r, int u, const struct RogueLink *l)
{
    const struct RoguePlace *pu = &r->places[u], *pv = &r->places[l->target];
    u8 keys = l->gate == ROGUE_GATE_LEAGUE ? ROGUE_KEY_LEAGUE : (l->gate == ROGUE_GATE_VILLAIN ? ROGUE_KEY_VILLAIN : 0);
    if (pv->obstacle && pv->guardDir == Opposite(l->dir))
        keys |= ROGUE_KEY(pv->obstacle);
    if (pu->obstacle && l->dir == pu->guardDir)
        keys |= ROGUE_KEY(pu->obstacle);
    if (IsSeaRoute(pv))
        keys |= ROGUE_KEY(ROBST_SURF);
    return keys;
}

// The places reachable from the first town with these keys, and the place
// each is first reached from (from may be NULL).
static void Reach(const struct RogueRegion *r, u8 keys, u8 *seen, u8 *from)
{
    u8 queue[ROGUE_MAX_PLACES];
    int head = 0, tail = 0, k;
    memset(seen, 0, r->nPlaces);
    if (from != NULL)
        memset(from, 0xFF, r->nPlaces);
    seen[r->start] = TRUE;
    queue[tail++] = r->start;
    while (head < tail)
    {
        int u = queue[head++];
        const struct RoguePlace *p = &r->places[u];
        u8 next[ROGUE_MAX_EXITS + 4];
        int n = 0;
        for (k = 0; k < p->nLinks; k++)
        {
            u8 need = LinkKeys(r, u, &p->links[k]);
            if ((need & keys) == need)
                next[n++] = p->links[k].target;
        }
        if (p->mouth != 0xFF)
            next[n++] = p->mouth;
        if (p->kind == RPLACE_CAVE && p->caveIn != 0xFF)
            next[n++] = p->caveIn;
        if (p->kind == RPLACE_CAVE && p->caveOut != 0xFF)
            next[n++] = p->caveOut;
        // Through the tunnel, once its barrier opens.
        if (r->tunnel < r->nDungeons && (keys & TunnelKeys(r)) == TunnelKeys(r))
        {
            const struct RogueDungeon *d = &r->dungeons[r->tunnel];
            if (u == d->host)
                next[n++] = d->exit;
            else if (u == d->exit)
                next[n++] = d->host;
        }
        for (k = 0; k < n; k++)
        {
            if (seen[next[k]])
                continue;
            seen[next[k]] = TRUE;
            if (from != NULL)
                from[next[k]] = u;
            queue[tail++] = next[k];
        }
    }
}

void RogueGen_ReachWith(const struct RogueRegion *r, u8 keys, u8 *seen)
{
    Reach(r, keys, seen, NULL);
}

// When each place opens if the gyms are won in order along the main road:
// the gyms won by then (0xFF if never), and the place it is first reached
// from.
static void Stages(const struct RogueRegion *r, u8 *stage, u8 *from)
{
    u8 seen[ROGUE_MAX_PLACES], at[ROGUE_MAX_PLACES], keys = 0;
    int g, i;
    memset(stage, 0xFF, r->nPlaces);
    memset(from, 0xFF, r->nPlaces);
    for (g = 0; g <= ROGUE_NUM_GYMS; g++)
    {
        if (g > 0)
            keys = RogueGen_KeysWon(r, (1 << g) - 1);
        Reach(r, keys, seen, at);
        for (i = 0; i < r->nPlaces; i++)
        {
            if (seen[i] && stage[i] == 0xFF)
            {
                stage[i] = g;
                from[i] = at[i];
            }
        }
    }
}

// TRUE if, with these keys, the main road is open exactly up to the first
// block they do not pass, and with all eight badges everything is open.
// Sets *gyms to the gyms whose towns these keys reach (a bit per gym).
static bool8 KeysHold(const struct RogueRegion *r, const u8 *spinePlace, u8 keys, u8 *gyms)
{
    u8 seen[ROGUE_MAX_PLACES];
    int i;
    Reach(r, keys, seen, NULL);
    *gyms = 0;
    for (i = 0; i < ROGUE_NUM_GYMS; i++)
        if (seen[spinePlace[GymStep(i)]])
            *gyms |= 1 << i;
    for (i = 0; i < SPINE_LEN; i++)
        if (seen[spinePlace[i]] != ZoneOpen(Zone(i), keys))
            return FALSE;
    if (keys & ROGUE_KEY_LEAGUE)
        for (i = 0; i < r->nPlaces; i++)
            if (!seen[i])
                return FALSE;
    return TRUE;
}

// The keys a player holds with the keys of their badges: the villain
// team's too once the town over its hideout (the place before the
// blockade) is reached, as the hideout is cleared from there.
static u8 HeldKeys(const struct RogueRegion *r, const u8 *spinePlace, u8 keys)
{
    u8 seen[ROGUE_MAX_PLACES];
    int j;
    for (j = 0; j < sNBlocks; j++)
    {
        if (sBlockKeys[j] != ROGUE_KEY_VILLAIN)
            continue;
        Reach(r, keys, seen, NULL);
        if (seen[spinePlace[sBlockAt[j] - 1]])
            keys |= ROGUE_KEY_VILLAIN;
    }
    return keys;
}

// TRUE if KeysHold for every set of badges a player can really hold: from
// none, win any gym whose town the keys held reach. So nothing added opens
// a way round a block, whatever order the gyms are won in, and no order
// leaves the player stuck; but a way may cross several blocks when the
// badge that opens it cannot be held before they are passed. A set of
// badges comes after every set it grows from, so one pass in order finds
// them all. Each set of keys (bits 1 to 7) is walked once.
static bool8 GatingHolds(const struct RogueRegion *r, const u8 *spinePlace)
{
    u32 held[(1 << ROGUE_NUM_GYMS) / 32], heldDone[4], holdDone[4];
    u8 heldOf[128], gymsOf[128];
    u32 won;
    int g;
    memset(held, 0, sizeof(held));
    memset(heldDone, 0, sizeof(heldDone));
    memset(holdDone, 0, sizeof(holdDone));
    held[0] = 1;
    for (won = 0; won < (1 << ROGUE_NUM_GYMS); won++)
    {
        u8 base, keys, open;
        if (!((held[won >> 5] >> (won & 31)) & 1))
            continue;
        base = RogueGen_KeysWon(r, won) >> 1;
        if (!((heldDone[base >> 5] >> (base & 31)) & 1))
        {
            heldDone[base >> 5] |= 1u << (base & 31);
            heldOf[base] = HeldKeys(r, spinePlace, base << 1) >> 1;
        }
        keys = heldOf[base];
        if (!((holdDone[keys >> 5] >> (keys & 31)) & 1))
        {
            holdDone[keys >> 5] |= 1u << (keys & 31);
            if (!KeysHold(r, spinePlace, keys << 1, &gymsOf[keys]))
                return FALSE;
        }
        if (won == (1 << ROGUE_NUM_GYMS) - 1)
            return TRUE;
        open = gymsOf[keys] & ~won;
        if (!open)
            return FALSE; // stuck
        for (g = 0; g < ROGUE_NUM_GYMS; g++)
            if ((open >> g) & 1)
                held[(won | (1 << g)) >> 5] |= 1u << ((won | (1 << g)) & 31);
    }
    return FALSE;
}

// A loop (a tunnel or a ring road) may lead at most this many stretches past
// the one where the badge that opens it is won along the road, so it never
// drops a team with two badges among the fourth gym's trainers.
#define LOOP_AHEAD 1

// TRUE if no way leads round a block that this key passes, even with every
// other key: GatingHolds lets a way cross a block behind a badge won only
// past it, and this keeps a story gate the only way on.
static bool8 NoWayRound(const struct RogueRegion *r, const u8 *spinePlace, u8 key)
{
    u8 seen[ROGUE_MAX_PLACES];
    int i;
    Reach(r, ROGUE_KEYS_ALL & ~key, seen, NULL);
    for (i = 0; i < SPINE_LEN; i++)
        if (seen[spinePlace[i]] != ZoneOpen(Zone(i), ROGUE_KEYS_ALL & ~key))
            return FALSE;
    return TRUE;
}

// Takes back every place from n on, and the links to them.
static void DropPlaces(struct RogueRegion *r, int n)
{
    int i, k;
    for (i = n; i < r->nPlaces; i++)
        sOcc[r->places[i].gridY][r->places[i].gridX] = 0;
    for (i = 0; i < n; i++)
    {
        struct RoguePlace *p = &r->places[i];
        for (k = 0; k < p->nLinks; k++)
        {
            if (p->links[k].target >= n)
            {
                p->links[k] = p->links[p->nLinks - 1];
                p->nLinks--;
                k--;
            }
        }
        if (p->mouth != 0xFF && p->mouth >= n)
            p->mouth = 0xFF;
    }
    r->nPlaces = n;
}

// Unlinked neighbours a new way would have: cells (cx, cy) in order, each
// joined only to the one before and after it. The first is a place that
// stands already, and the last too if lastStands. Every other cell is new,
// and counts each place beside it it does not join: on the region map, a
// road a tile from another with a sliver of land between.
static int ChainCrowd(const s8 *cx, const s8 *cy, int n, bool8 lastStands)
{
    int i, j, d, count = 0;
    for (i = 1; i < n - (lastStands ? 1 : 0); i++)
    {
        for (d = 0; d < 4; d++)
        {
            int nx = cx[i] + sDirDX[d], ny = cy[i] + sDirDY[d];
            for (j = 0; j < n && (cx[j] != nx || cy[j] != ny); j++)
                ;
            if (j < n)
            {
                bool8 stands = (j == 0 || (lastStands && j == n - 1));
                if (Abs(i - j) != 1 && (stands || j > i))
                    count++;
            }
            else if (nx >= 0 && ny >= 0 && nx < RGRID_W && ny < RGRID_H && sOcc[ny][nx] && sOcc[ny][nx] != RESERVED)
            {
                count++;
            }
        }
    }
    return count;
}

// TRUE if a side place may hang off place at on side d.
static bool8 CanBranch(const struct RogueRegion *r, int at, u8 d)
{
    const struct RoguePlace *p = &r->places[at];
    if (p->kind == RPLACE_CAVE || (p->kind == RPLACE_TOWN && p->type == RTOWN_SUMMIT)
     || (p->kind == RPLACE_ROUTE && (p->type == RROUTE_MOUNTAIN || p->type == RROUTE_SEA)))
        return FALSE;
    return !HasSide(p, d) && p->nLinks < ROGUE_MAX_EXITS;
}

static int SideRoute(struct RogueRegion *r, u32 *rng, int x, int y, u8 branch, u8 obstacle, u8 guardDir)
{
    int s = NewPlace(r, RPLACE_ROUTE, RogueRng_Range(rng, 100) < 40 ? RROUTE_LAKE : RROUTE_FOREST, x, y);
    struct RoguePlace *p = &r->places[s];
    p->side = TRUE;
    p->branch = branch;
    p->obstacle = obstacle;
    p->guardDir = guardDir;
    sOcc[y][x] = s + 1;
    return s;
}

// Crossroads towns, like Mauville: side paths, shortcuts and forks hang off
// them first.
static u8 sHubs[2];

static int HubBonus(int place)
{
    return (place == sHubs[0] || place == sHubs[1]) ? 8 : 0;
}

// Side places hang off routes, or off a crossroads town: other towns keep
// to the way in and the way on.
static bool8 MayHangOff(const struct RogueRegion *r, int at, u8 d)
{
    if (r->places[at].kind == RPLACE_TOWN && !HubBonus(at))
        return FALSE;
    return CanBranch(r, at, d);
}

// TRUE if (x, y) is free land, or the cell kept free beside place at (a
// crossroads town), which a way out of that town may take.
static bool8 CellNextTo(const struct RogueRegion *r, int x, int y, int at)
{
    int h;
    if (CellIs(x, y, CELL_LAND))
        return TRUE;
    for (h = 0; h < 2; h++)
        if (sHubs[h] == at && sHubStep[h] >= 0 && sLayWork->rx[sHubStep[h]] == x && sLayWork->ry[sHubStep[h]] == y
         && sOcc[y][x] == RESERVED)
            return TRUE;
    return FALSE;
}

// A second way between main road places a and b: one or two side routes
// over free land, the first behind an obstacle on a's side. Picks the best
// of the ways found; FALSE if there is none.
struct WayChoice
{
    int a, b, score;
    u8 da, e, f, len; // a's side, then the moves on to the next cell and into b
};

#define WAY_CROWD 6
#define WAY_NONE (-10000)

static void TryWays(const struct RogueRegion *r, u32 *rng, int a, int b, int bonus, bool8 vertical, struct WayChoice *best)
{
    const struct RoguePlace *pa = &r->places[a], *pb = &r->places[b];
    u8 da, e, f;
    s8 cx[4], cy[4];
    for (da = 0; da < 4; da++)
    {
        int x1 = pa->gridX + sDirDX[da], y1 = pa->gridY + sDirDY[da];
        if ((vertical && da != RDIR_NORTH && da != RDIR_SOUTH) || !MayHangOff(r, a, da) || !(CellNextTo(r, x1, y1, a) || CellNextTo(r, x1, y1, b)))
            continue;
        for (e = 0; e < 4; e++)
        {
            int x2 = x1 + sDirDX[e], y2 = y1 + sDirDY[e], score;
            if (x2 == pb->gridX && y2 == pb->gridY)
            {
                if (!MayHangOff(r, b, Opposite(e)))
                    continue;
                cx[0] = pa->gridX; cy[0] = pa->gridY;
                cx[1] = x1; cy[1] = y1;
                cx[2] = pb->gridX; cy[2] = pb->gridY;
                score = bonus + RogueRng_Range(rng, 6) - ChainCrowd(cx, cy, 3, TRUE) * WAY_CROWD;
                if (score > best->score)
                {
                    best->a = a; best->b = b; best->score = score;
                    best->da = da; best->e = e; best->len = 1;
                }
                continue;
            }
            if (!CellNextTo(r, x2, y2, b) || (x2 == pa->gridX && y2 == pa->gridY))
                continue;
            for (f = 0; f < 4; f++)
            {
                if (x2 + sDirDX[f] != pb->gridX || y2 + sDirDY[f] != pb->gridY || !MayHangOff(r, b, Opposite(f)))
                    continue;
                cx[0] = pa->gridX; cy[0] = pa->gridY;
                cx[1] = x1; cy[1] = y1;
                cx[2] = x2; cy[2] = y2;
                cx[3] = pb->gridX; cy[3] = pb->gridY;
                score = bonus + RogueRng_Range(rng, 6) + 1 - ChainCrowd(cx, cy, 4, TRUE) * WAY_CROWD;
                if (score > best->score)
                {
                    best->a = a; best->b = b; best->score = score;
                    best->da = da; best->e = e; best->f = f; best->len = 2;
                }
            }
        }
    }
}

static void BuildWay(struct RogueRegion *r, u32 *rng, const struct WayChoice *w, u8 branch, u8 obstacle)
{
    const struct RoguePlace *pa = &r->places[w->a];
    int x1 = pa->gridX + sDirDX[w->da], y1 = pa->gridY + sDirDY[w->da];
    int s1 = SideRoute(r, rng, x1, y1, branch, obstacle, Opposite(w->da));
    Link(r, w->a, s1, w->da);
    if (w->len == 1)
    {
        Link(r, s1, w->b, w->e);
    }
    else
    {
        // The second route is open from b's side; its way in faces b.
        int s2 = SideRoute(r, rng, x1 + sDirDX[w->e], y1 + sDirDY[w->e], branch, ROBST_NONE, w->f);
        Link(r, s1, s2, w->e);
        Link(r, s2, w->b, w->f);
    }
}

// A shortcut back, like Rusturf Tunnel: from a place of a later stretch to
// a town of an earlier one, behind an obstacle. Across a block on the road
// its obstacle is the block's own (or that of the chapter's second way on),
// so it never opens a way round one; within a chapter, one whose badge comes
// no earlier than the later stretch, so it opens later on.
static u8 ShortcutKey(const struct RogueRegion *r, u32 *rng, int i, int j)
{
    static const u8 sKeys[] = { ROBST_CUT, ROBST_ROCK_SMASH, ROBST_STRENGTH, ROBST_SURF };
    int k, n = 0, b = -1;
    u8 o = ROBST_NONE;
    for (k = 0; k < sNBlocks; k++)
    {
        if (sBlockAt[k] > i && sBlockAt[k] <= j)
        {
            if (b >= 0)
                return ROBST_NONE;
            b = k;
        }
    }
    if (b >= 0)
    {
        if (sBlockKind[b] == ROBST_NONE)
            return ROBST_NONE;
        return (sBlockFork[b] != ROBST_NONE && RogueRng_Range(rng, 2)) ? sBlockFork[b] : sBlockKind[b];
    }
    for (k = 0; k < (int)sizeof(sKeys); k++)
        if (KeyStage(r, sKeys[k]) >= SpineArrive(j) && RogueRng_Range(rng, ++n) == 0)
            o = sKeys[k];
    return o;
}

static bool8 AddShortcut(struct RogueRegion *r, u32 *rng, const u8 *spinePlace)
{
    struct WayChoice best;
    int i, j, n = r->nPlaces;
    u8 o = ROBST_NONE;
    // Room left for a crossroads town's side path.
    if (r->nPlaces + 2 > ROGUE_MAX_PLACES - 2)
        return FALSE;
    best.score = WAY_NONE;
    for (i = 0; i < SPINE_LEN; i++)
    {
        for (j = i + 3; j < SPINE_LEN; j++)
        {
            u8 ba = SpineArrive(i), bb = SpineArrive(j), k;
            int bonus, before = best.score;
            if (bb <= ba || bb > 5)
                continue;
            k = ShortcutKey(r, rng, i, j);
            if (k == ROBST_NONE)
                continue;
            bonus = (r->places[spinePlace[i]].kind == RPLACE_TOWN ? 6 : 0) + (r->places[spinePlace[j]].kind == RPLACE_TOWN ? 3 : 0)
                  + (j - i > 8 ? 8 : j - i) + HubBonus(spinePlace[i]) + HubBonus(spinePlace[j]);
            TryWays(r, rng, spinePlace[i], spinePlace[j], bonus, k == ROBST_SURF, &best);
            if (best.score != before)
                o = k;
        }
    }
    if (best.score == WAY_NONE)
        return FALSE;
    BuildWay(r, rng, &best, RBRANCH_SHORTCUT, o);
    if (!GatingHolds(r, spinePlace))
    {
        DropPlaces(r, n);
        return FALSE;
    }
    return TRUE;
}

// A ring road, like Kanto's Routes 11 to 15 from Vermilion round to
// Fuchsia: one or two side routes over free land from an early stretch of
// the main road to one three gyms or more on, behind an obstacle on the
// early side, laid before the other extras wherever one fits. Any
// obstacle whose badge the exact check allows will do, the one that opens
// soonest first; it may cross several blocks. Uses its own random stream,
// so a region where no ring fits is laid as it was.
static bool8 sRingBuilt;
static bool8 sRingFits; // one closes on the plan before its extras

static bool8 AddRingRoad(struct RogueRegion *r, const u8 *spinePlace)
{
    static const u8 sKeys[] = { ROBST_CUT, ROBST_ROCK_SMASH, ROBST_STRENGTH };
    u32 rng = RogueRng_Hash(r->seed, 11980);
    struct WayChoice best;
    u8 stage[ROGUE_MAX_PLACES], from[ROGUE_MAX_PLACES], order[3], latest = 0, ringFar = 0;
    int i, j, k, n = r->nPlaces;
    if (r->nPlaces + 2 > ROGUE_MAX_PLACES)
        return FALSE;
    // A ring leads at most LOOP_AHEAD stretches past the one where its
    // key's badge is won: no further than the latest key allows.
    for (k = 0; k < 3; k++)
        if (KeyGym(r, sKeys[k]) >= 0 && KeyStage(r, sKeys[k]) + LOOP_AHEAD > latest)
            latest = KeyStage(r, sKeys[k]) + LOOP_AHEAD;
    best.score = WAY_NONE;
    for (i = 0; i < SPINE_LEN; i++)
    {
        for (j = i + 4; j < SPINE_LEN; j++)
        {
            int ba = SpineArrive(i), bb = SpineArrive(j);
            if (bb < ba + 3 || bb > latest || sSpine[j] == SP_VICTORY || sSpine[j] == SP_CAVE || sSpine[j] == SP_SUMMIT)
                continue;
            // The longer the ring the better, and from early on.
            TryWays(r, &rng, spinePlace[i], spinePlace[j], (j - i > 16 ? 16 : j - i) + 2 * (bb - ba) - 2 * ba,
                    FALSE, &best);
        }
    }
    if (best.score == WAY_NONE)
        return FALSE;
    for (i = 0; i < SPINE_LEN; i++)
        if ((spinePlace[i] == best.a || spinePlace[i] == best.b) && SpineArrive(i) > ringFar)
            ringFar = SpineArrive(i);
    // The keys by the stretch their badge is won in, soonest first.
    for (k = 0; k < 3; k++)
        order[k] = sKeys[k];
    for (k = 0; k < 3; k++)
        for (i = 2; i > k; i--)
            if (KeyStage(r, order[i]) < KeyStage(r, order[i - 1]))
            {
                u8 t = order[i];
                order[i] = order[i - 1];
                order[i - 1] = t;
            }
    Stages(r, stage, from);
    for (k = 0; k < 3; k++)
    {
        u8 after[ROGUE_MAX_PLACES];
        if (KeyGym(r, order[k]) < 0 || ringFar > KeyStage(r, order[k]) + LOOP_AHEAD)
            continue;
        BuildWay(r, &rng, &best, RBRANCH_SHORTCUT, order[k]);
        if (GatingHolds(r, spinePlace))
        {
            // No place of the road opens sooner than it did.
            Stages(r, after, from);
            for (i = 0; i < n && after[i] == stage[i]; i++)
                ;
            if (i == n)
                return TRUE;
        }
        DropPlaces(r, n);
    }
    return FALSE;
}

// A fork, like Route 110's cycling road: a second way between two places of
// one stretch, behind an obstacle. With the badge in hand it is a choice
// now; otherwise one for later.
static bool8 AddFork(struct RogueRegion *r, u32 *rng, const u8 *spinePlace)
{
    static const u8 sKeys[] = { ROBST_CUT, ROBST_ROCK_SMASH, ROBST_STRENGTH };
    struct WayChoice best;
    int i, j, n = r->nPlaces;
    best.score = WAY_NONE;
    // Room left for a crossroads town's side path.
    if (r->nPlaces + 2 > ROGUE_MAX_PLACES - 2)
        return FALSE;
    for (i = 0; i < SPINE_LEN; i++)
    {
        for (j = i + 1; j < SPINE_LEN && j <= i + 5; j++)
        {
            if (SpineArrive(i) != SpineArrive(j) || Zone(i) != Zone(j))
                continue;
            TryWays(r, rng, spinePlace[i], spinePlace[j], HubBonus(spinePlace[i]) + HubBonus(spinePlace[j]) + (j - i), FALSE, &best);
        }
    }
    if (best.score == WAY_NONE)
        return FALSE;
    BuildWay(r, rng, &best, RBRANCH_FORK, sKeys[RogueRng_Range(rng, sizeof(sKeys))]);
    if (!GatingHolds(r, spinePlace))
    {
        DropPlaces(r, n);
        return FALSE;
    }
    return TRUE;
}

// A chapter's second way on: a fork from its stretch round the block on the
// road, behind the obstacle another of its badges clears.
static bool8 AddBypass(struct RogueRegion *r, u32 *rng, const u8 *spinePlace, int b)
{
    struct WayChoice best;
    int i, j, n = r->nPlaces, at = sBlockAt[b];
    u8 keys = sBlockKeys[b];
    if (r->nPlaces + 2 > ROGUE_MAX_PLACES - 2)
        return FALSE;
    best.score = WAY_NONE;
    for (i = at - 1; i >= 0 && i >= at - 4 && Zone(i) == b; i--)
        for (j = at; j < SPINE_LEN && j <= i + 5 && Zone(j) == b + 1; j++)
            if (sSpine[j] != SP_CAVE && sSpine[i] != SP_CAVE)
                TryWays(r, rng, spinePlace[i], spinePlace[j], (j - i) + (i == at - 1 ? 0 : 3), FALSE, &best);
    if (best.score == WAY_NONE)
        return FALSE;
    BuildWay(r, rng, &best, RBRANCH_FORK, sBlockFork[b]);
    sBlockKeys[b] |= ROGUE_KEY(sBlockFork[b]);
    if (!GatingHolds(r, spinePlace))
    {
        sBlockKeys[b] = keys;
        DropPlaces(r, n);
        return FALSE;
    }
    return TRUE;
}

// Side paths: what blocks the way in, which stretch of the main road they
// may hang off (by the gyms passed on leaving that place: from the stretch
// before the one where the obstacle's badge is won to a few after), how
// many routes deep they run, and what lies at the end.
enum { BEYOND_NONE, BEYOND_CAVE, BEYOND_HAMLET };

struct BranchDef
{
    u8 obstacle;
    u8 first, last; // stretches after the badge's: first before it, last after it
    u8 routes;
    u8 beyond;
};

#define BRANCH_ANYWHERE 0xFF // in first: any stretch

static const struct BranchDef sBranches[] =
{
    { ROBST_CUT,        1, 2, 1, BEYOND_CAVE },   // a dark cave, lit once the second badge is won
    { ROBST_CUT,        1, 2, 2, BEYOND_NONE },   // a deep grove behind small trees
    { ROBST_CUT,        1, 3, 1, BEYOND_HAMLET }, // a hamlet in the woods behind small trees
    { ROBST_ROCK_SMASH, 1, 3, 2, BEYOND_HAMLET }, // a hamlet behind cracked rocks
    { ROBST_STRENGTH,   1, 3, 2, BEYOND_NONE },   // a boulder to push aside, and a long way in
    { ROBST_SURF,       1, 3, 1, BEYOND_HAMLET }, // a hamlet over the water
};

static bool8 AddBranch(struct RogueRegion *r, u32 *rng, const struct BranchDef *def, const u8 *spinePlace, int fromPlace)
{
    u8 cand[SPINE_MAX];
    int tries, i, nCand = 0, n = r->nPlaces, ks = KeyStage(r, def->obstacle), lo = 0, hi = ROGUE_NUM_GYMS;
    if (def->first != BRANCH_ANYWHERE)
    {
        lo = ks - def->first;
        hi = ks + def->last;
    }
    for (i = 0; i < SPINE_LEN; i++)
        if (sSpineBadges[i] >= lo && sSpineBadges[i] <= hi)
            cand[nCand++] = i;
    int bestCost = 0x7FFF, bAt = 0, bRoutes = 1, bBx = 0, bBy = 0;
    u8 bD = 0, bE = 0;
    for (tries = 0; tries < 80 && (nCand || fromPlace >= 0) && r->nPlaces + def->routes + 1 <= ROGUE_MAX_PLACES; tries++)
    {
        s8 cx[4], cy[4];
        int nc, cost;
        int at = fromPlace >= 0 ? fromPlace : spinePlace[cand[RogueRng_Range(rng, nCand)]];
        const struct RoguePlace *p = &r->places[at];
        u8 d = RogueRng_Range(rng, 4), e = d, last;
        int x = p->gridX + sDirDX[d], y = p->gridY + sDirDY[d], x2 = x, y2 = y, bx = 0, by = 0;
        int routes = def->routes == 2 && RogueRng_Range(rng, 4) ? 2 : 1;
        // Crossroads towns first.
        if (fromPlace < 0 && tries < 30 && (sHubs[0] != 0xFF || sHubs[1] != 0xFF) && RogueRng_Range(rng, 2))
        {
            int h = sHubs[RogueRng_Range(rng, 2)];
            if (h != 0xFF)
            {
                for (i = 0; i < SPINE_LEN && spinePlace[i] != h; i++)
                    ;
                if (i < SPINE_LEN && sSpineBadges[i] >= lo && sSpineBadges[i] <= hi)
                {
                    at = h;
                    p = &r->places[at];
                    x = x2 = p->gridX + sDirDX[d];
                    y = y2 = p->gridY + sDirDY[d];
                }
            }
        }
        if (!MayHangOff(r, at, d) || !CellNextTo(r, x, y, at))
            continue;
        // Water is only crossed going north or south.
        if (def->obstacle == ROBST_SURF && d != RDIR_NORTH && d != RDIR_SOUTH)
            continue;
        if (routes == 2)
        {
            e = (d + (RogueRng_Range(rng, 3) + 3)) & 3; // on, or to one side
            if (e == Opposite(d))
                e = d;
            x2 = x + sDirDX[e];
            y2 = y + sDirDY[e];
            if (!CellIs(x2, y2, CELL_LAND))
                continue;
        }
        last = routes == 2 ? e : d; // the way into the last route
        if (def->beyond == BEYOND_CAVE)
        {
            // In through a mouth at the top of the last route, so its way in
            // cannot be from the north.
            bx = x2;
            by = y2 - 1;
            if (last == RDIR_SOUTH || !(CellIs(bx, by, CELL_LAND) || CellIs(bx, by, CELL_PEAK)))
                continue;
        }
        else if (def->beyond == BEYOND_HAMLET)
        {
            u8 f = last;
            if (RogueRng_Range(rng, 2))
                f = (last + (RogueRng_Range(rng, 2) ? 1 : 3)) & 3;
            bx = x2 + sDirDX[f];
            by = y2 + sDirDY[f];
            if (!CellIs(bx, by, CELL_LAND))
                continue;
            if (r->nTowns >= ROGUE_MAX_TOWNS)
                continue;
        }
        // The way that crowds the other roads least.
        cx[0] = p->gridX; cy[0] = p->gridY;
        cx[1] = x; cy[1] = y;
        nc = 2;
        if (routes == 2)
        {
            cx[nc] = x2; cy[nc] = y2; nc++;
        }
        if (def->beyond != BEYOND_NONE)
        {
            cx[nc] = bx; cy[nc] = by; nc++;
        }
        cost = ChainCrowd(cx, cy, nc, FALSE) * 8 + RogueRng_Range(rng, 4);
        if (cost < bestCost)
        {
            bestCost = cost;
            bAt = at; bD = d; bE = e; bRoutes = routes; bBx = bx; bBy = by;
        }
    }
    if (bestCost == 0x7FFF)
        return FALSE;
    {
        int at = bAt, routes = bRoutes, bx = bBx, by = bBy, s, k;
        u8 d = bD, e = bE;
        const struct RoguePlace *p = &r->places[at];
        int x = p->gridX + sDirDX[d], y = p->gridY + sDirDY[d], x2 = x, y2 = y;
        if (routes == 2)
        {
            x2 = x + sDirDX[e];
            y2 = y + sDirDY[e];
        }
        s = SideRoute(r, rng, x, y, RBRANCH_SIDE, def->obstacle, Opposite(d));
        Link(r, at, s, d);
        if (routes == 2)
        {
            int s2 = SideRoute(r, rng, x2, y2, RBRANCH_SIDE, ROBST_NONE, Opposite(e));
            Link(r, s, s2, e);
            s = s2;
        }
        if (def->beyond == BEYOND_CAVE)
        {
            int cave = NewPlace(r, RPLACE_CAVE, 0, bx, by);
            r->places[cave].side = TRUE;
            r->places[cave].branch = RBRANCH_SIDE;
            r->places[cave].dark = TRUE;
            sOcc[by][bx] = cave + 1;
            LinkCave(r, s, cave, -1);
        }
        else if (def->beyond == BEYOND_HAMLET)
        {
            k = NewPlace(r, RPLACE_TOWN, RTOWN_HAMLET, bx, by);
            r->places[k].side = TRUE;
            r->places[k].branch = RBRANCH_SIDE;
            sOcc[by][bx] = k + 1;
            r->nTowns++;
            Link(r, s, k, DirTo(x2, y2, bx, by));
        }
        if (!GatingHolds(r, spinePlace))
        {
            if (def->beyond == BEYOND_HAMLET)
                r->nTowns--;
            DropPlaces(r, n);
            return FALSE;
        }
        return TRUE;
    }
}

static int Exits(const struct RoguePlace *p)
{
    return p->nLinks + (p->mouth != 0xFF);
}

// The extras on the main road: shortcuts back, a fork, a crossroads town's
// way out, then side paths, a hamlet first most times.
static void AddExtras(struct RogueRegion *r, u32 *rng, const u8 *spinePlace)
{
    int i;
    // A shortcut and a fork, in either order, as each may take the land
    // the other wants. First the chapters' second ways on.
    bool8 forkFirst = RogueRng_Range(rng, 2);
    for (i = 0; i < sNBlocks; i++)
        if (sBlockFork[i] != ROBST_NONE)
            AddBypass(r, rng, spinePlace, i);
    sRingBuilt = sRingFits && AddRingRoad(r, spinePlace);
    if (forkFirst)
        AddFork(r, rng, spinePlace);
    if (AddShortcut(r, rng, spinePlace) && RogueRng_Range(rng, 100) < 25)
        AddShortcut(r, rng, spinePlace);
    if (!forkFirst)
        AddFork(r, rng, spinePlace);
    // A crossroads town has three ways out at least: a short side path
    // gives it one more.
    for (i = 0; i < 2; i++)
    {
        int h = sHubs[i], b;
        if (h == 0xFF || Exits(&r->places[h]) >= 3)
            continue;
        for (b = 0; b < (int)ARRAY_COUNT_GEN(sBranches) && Exits(&r->places[h]) < 3; b++)
        {
            struct BranchDef def = sBranches[b];
            def.first = BRANCH_ANYWHERE;
            def.routes = 1;
            if (def.beyond == BEYOND_HAMLET)
                def.beyond = BEYOND_NONE;
            AddBranch(r, rng, &def, spinePlace, h);
        }
    }
    for (i = 0; i < 2; i++)
        if (sHubStep[i] >= 0 && sOcc[sLayWork->ry[sHubStep[i]]][sLayWork->rx[sHubStep[i]]] == RESERVED)
            sOcc[sLayWork->ry[sHubStep[i]]][sLayWork->rx[sHubStep[i]]] = 0;
    {
        u8 order[ARRAY_COUNT_GEN(sBranches)];
        int n = ARRAY_COUNT_GEN(sBranches), want = 2 + RogueRng_Range(rng, 2), got = 0;
        for (i = 0; i < n; i++)
            order[i] = i;
        for (i = n - 1; i > 0; i--)
        {
            int j = RogueRng_Range(rng, i + 1);
            u8 t = order[i];
            order[i] = order[j];
            order[j] = t;
        }
        for (i = 0; i < n && got < want; i++)
            got += AddBranch(r, rng, &sBranches[order[i]], spinePlace, -1);
    }
    // No crossroads yet: the first town of the main road (from a random
    // one on) that takes a short side path becomes one.
    {
        int crossroads = 0, start = RogueRng_Range(rng, SPINE_LEN), k;
        for (i = 0; i < SPINE_LEN; i++)
            crossroads += (r->places[spinePlace[i]].kind == RPLACE_TOWN && Exits(&r->places[spinePlace[i]]) >= 3);
        for (k = 0; k < SPINE_LEN && !crossroads; k++)
        {
            int at = spinePlace[(start + k) % SPINE_LEN], b;
            const struct RoguePlace *p = &r->places[at];
            if (p->kind != RPLACE_TOWN || p->type == RTOWN_SUMMIT)
                continue;
            sHubs[0] = at;
            for (b = 0; b < (int)ARRAY_COUNT_GEN(sBranches) && !crossroads; b++)
            {
                struct BranchDef def = sBranches[b];
                def.first = BRANCH_ANYWHERE;
                def.routes = 1;
                if (def.beyond == BEYOND_HAMLET)
                    def.beyond = BEYOND_NONE;
                if (AddBranch(r, rng, &def, spinePlace, at))
                    crossroads = 1;
            }
        }
    }
}

// How well the extras give reasons to wander and come back: a shortcut
// back or a fork (both is better), obstacles of two kinds or more, a way
// that opens only with a later badge, and a hamlet (one reached before the
// sixth badge is better).
#define EXTRA_FLOOR 100
#define EXTRA_PASS  1000
#define EXTRA_TRIES 6

// Places side by side on the grid with no way between them.
static int RegionCrowd(const struct RogueRegion *r)
{
    int i, j, n = 0;
    for (i = 0; i < r->nPlaces; i++)
        for (j = i + 1; j < r->nPlaces; j++)
            if (Abs(r->places[i].gridX - r->places[j].gridX) + Abs(r->places[i].gridY - r->places[j].gridY) == 1
             && !MapJoined(r, i, j))
                n++;
    return n;
}

static int ExtrasScore(const struct RogueRegion *r, const u8 *spinePlace)
{
    u8 stage[ROGUE_MAX_PLACES], from[ROGUE_MAX_PLACES];
    int i, k, kinds = 0, nKinds = 0, score = 0;
    bool8 shortcut = FALSE, fork = FALSE, later = FALSE, hamlet = FALSE, early = FALSE;
    Stages(r, stage, from);
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        shortcut |= p->branch == RBRANCH_SHORTCUT;
        fork |= p->branch == RBRANCH_FORK;
        if (p->kind == RPLACE_TOWN && p->type == RTOWN_HAMLET)
        {
            hamlet = TRUE;
            early |= stage[i] < 6;
        }
        if (!p->obstacle)
            continue;
        kinds |= 1 << p->obstacle;
        for (k = 0; k < p->nLinks; k++)
            if (p->side && p->links[k].dir == p->guardDir && KeyStage(r, p->obstacle) > stage[p->links[k].target])
                later = TRUE;
    }
    for (k = ROBST_CUT; k <= ROBST_SURF; k++)
        nKinds += (kinds >> k) & 1;
    if ((shortcut || fork) && nKinds >= 2 && (shortcut || later))
        score += EXTRA_FLOOR;
    score += (shortcut && fork) * 15 + hamlet * 6 + early * 9 + (nKinds >= 3) * 4;
    return score;
}

// Lays the plan out: the land, the main road on it, then crossroads,
// shortcuts, forks and side paths. FALSE if this land will not take it.
static bool8 LayRegion(struct RogueRegion *r, u32 *rng, u8 *spinePlace, u32 salt)
{
    struct Guide g;
    int k, i, bestCost = 0x7FFF, nLaid = 0, stall = 0;
    if (!PlanLand(r->seed, salt, &sLand))
        return FALSE;
    // (v1.1 speed) Walks for the second stretch of sea or a raft village give
    // up when stuck; other regions lay as they always did.
    if (sSeaSecond)
        stall = SEA2_STALL;
    for (i = 0; i < SPINE_LEN; i++)
        if (sSpine[i] == SP_RAFT)
            stall = TOWNS2_RAFT_STALL;
    // One or two crossroads towns, like Mauville: a village or a gym city
    // along the way, not one reached through a cave.
    sHubStep[0] = sHubStep[1] = -1;
    {
        int nHubs = RogueRng_Range(rng, 100) < 60 ? 1 : 2, h, seen;
        for (h = 0; h < nHubs; h++)
        {
            for (i = 1, seen = 0; i < SPINE_LEN; i++)
                if ((sSpine[i] == SP_VILLAGE || sSpine[i] == SP_GYM) && sSpine[i - 1] != SP_CAVE && i != sHubStep[0]
                 && RogueRng_Range(rng, ++seen) == 0)
                    sHubStep[h] = i;
        }
    }
    // The main road along a few guides, keeping the one that runs widest
    // across the land with the fewest stretches side by side that do not
    // meet: roads with room between them, as Hoenn's have.
    // A land where the first few will not lay is passed over, as is one
    // that keeps failing: each failure costs the whole step budget.
    // (v1.1 sea: a road with a second stretch of sea along the coast is
    // harder to lay, and is given more guides and steps.)
    for (k = 0; k < SPINE_TRIES + 6 + sSeaSecond * SEA2_GUIDES && nLaid < SPINE_TRIES && (nLaid > 0 || k < 4 + sSeaSecond * SEA2_GUIDES); k++)
    {
        int cost;
        MakeGuide(rng, &g);
        if (!LaySpine(rng, &g, sSeaSecond ? SEA2_BUDGET : 1200, stall))
            continue;
        nLaid++;
        cost = SpineCrowd() * 10 - SpineSpan() * 3 + RogueRng_Range(rng, 6);
        if (cost < bestCost)
        {
            bestCost = cost;
            memcpy(sLayWork->bx, sLayX, sizeof(sLayX));
            memcpy(sLayWork->by, sLayY, sizeof(sLayY));
            memcpy(sLayWork->brx, sLayWork->rx, sizeof(sLayWork->rx));
            memcpy(sLayWork->bry, sLayWork->ry, sizeof(sLayWork->ry));
        }
    }
    if (nLaid == 0)
        return FALSE;
    memcpy(sLayX, sLayWork->bx, sizeof(sLayX));
    memcpy(sLayY, sLayWork->by, sizeof(sLayY));
    memcpy(sLayWork->rx, sLayWork->brx, sizeof(sLayWork->rx));
    memcpy(sLayWork->ry, sLayWork->bry, sizeof(sLayWork->ry));
    memset(sOcc, 0, sizeof(sOcc));
    for (i = 0; i < SPINE_LEN; i++)
    {
        sOcc[sLayY[i]][sLayX[i]] = i + 1;
        if (i == sHubStep[0] || i == sHubStep[1])
            sOcc[sLayWork->ry[i]][sLayWork->rx[i]] = RESERVED;
    }
    r->land = salt;
    BuildSpine(r, rng, spinePlace);
    r->start = spinePlace[0];
    if (!PlaceBlocks(r, rng, spinePlace))
        return FALSE;
    r->nTowns = 0;
    for (i = 0; i < SPINE_LEN; i++)
        r->nTowns += (r->places[spinePlace[i]].kind == RPLACE_TOWN);

    // The crossroads towns, whose kept cells only their own ways out may
    // take, until each has its third.
    sHubs[0] = sHubs[1] = 0xFF;
    for (i = 0; i < 2; i++)
    {
        if (sHubStep[i] < 0)
            continue;
        sHubs[i] = spinePlace[sHubStep[i]];
    }

    // The extras, a few times over from the same start, keeping the one
    // that gives the region the most reasons to wander and come back.
    {
        int n = r->nPlaces, nTowns = r->nTowns, attempt, bestScore = -1;
        u8 hubs[2];
        u32 bestRng = *rng, start;
        hubs[0] = sHubs[0];
        hubs[1] = sHubs[1];
        memcpy(sLayWork->occSave, sOcc, sizeof(sOcc));
        memcpy(sLayWork->keysSave, sBlockKeys, sizeof(sBlockKeys));
        // Does a ring road close here (tried on its own stream, and taken
        // back)? If so each try of the extras lays it first.
        sRingFits = AddRingRoad(r, spinePlace);
        DropPlaces(r, n);
        memcpy(sOcc, sLayWork->occSave, sizeof(sOcc));
        for (attempt = 0; attempt < EXTRA_TRIES; attempt++)
        {
            int score;
            start = *rng;
            AddExtras(r, rng, spinePlace);
            score = ExtrasScore(r, spinePlace);
            // Of those that give enough, the one with the fewest places
            // side by side with no way between them; a ring road first.
            if (score >= EXTRA_FLOOR)
                score += EXTRA_PASS - RegionCrowd(r) * 3 + sRingBuilt * 500;
            if (score > bestScore)
            {
                bestScore = score;
                bestRng = start;
            }
            DropPlaces(r, n);
            r->nTowns = nTowns;
            sHubs[0] = hubs[0];
            sHubs[1] = hubs[1];
            memcpy(sOcc, sLayWork->occSave, sizeof(sOcc));
            memcpy(sBlockKeys, sLayWork->keysSave, sizeof(sBlockKeys));
        }
        // Too few reasons to wander on this land: try the next one, while
        // there are others to try.
        if (bestScore < EXTRA_PASS && (salt < 30 || salt >= SEA2_LAND_0))
            return FALSE;
        start = *rng;
        *rng = bestRng;
        AddExtras(r, rng, spinePlace);
        *rng = start;
    }
    // v1.1 sea: side routes out to islands, once Surf is won (rogue_gen_sea.h).
    SeaAddIslands(r, spinePlace);
    TrimLand(&sLand, r);
    return TRUE;
}

// Map sizes, in blocks.
static void SizePlace(struct RoguePlace *p)
{
    bool8 n = HasSide(p, RDIR_NORTH), s = LinkOnSide(p, RDIR_SOUTH) != NULL;
    bool8 e = LinkOnSide(p, RDIR_EAST) != NULL, w = LinkOnSide(p, RDIR_WEST) != NULL;
    if (p->kind == RPLACE_TOWN)
    {
        // Cities are bigger than villages; plazas and harbors bigger still.
        p->w = 28;
        p->h = 24;
        if (p->type == RTOWN_GYM)
        {
            p->w = p->layout == RLAYOUT_STREETS ? 32 : 36;
            p->h = p->layout == RLAYOUT_STREETS ? 26 : 28;
        }
        // (v1.1 towns2) A raft village has open water round its rafts; a
        // crater town its ring of cliffs round the lake.
        if (p->theme == RTHEME_RAFT)
        {
            // (v1.1 layouts: room for four plans and four to six homes.)
            p->w = 40;
            p->h = 34;
        }
        else if (p->theme == RTHEME_CRATER)
        {
            p->w = p->type == RTOWN_GYM ? 44 : 40;
            p->h = p->type == RTOWN_GYM ? 36 : 34;
        }
    }
    else if (p->kind == RPLACE_CAVE)
    {
        // Caves come tall, wide or square; side caves smaller.
        static const u8 sCaveSizes[][2] = { { 26, 40 }, { 40, 30 }, { 34, 34 }, { 30, 40 }, { 40, 26 }, { 28, 30 }, { 32, 28 } };
        u32 k = RogueRng_Hash(p->number * 37 + p->gridX * 5, p->gridY + 11) % (p->side ? 3 : 5);
        k += p->side ? 4 : 0;
        p->w = sCaveSizes[k][0];
        p->h = sCaveSizes[k][1];
    }
    else if (p->kind == RPLACE_ROUTE && p->type == RROUTE_SEA && p->side)
    {
        // v1.1 sea: a side route out to an island, deep along the way in.
        bool8 vertical = (p->guardDir == RDIR_NORTH || p->guardDir == RDIR_SOUTH);
        p->w = vertical ? 36 : 44;
        p->h = vertical ? 44 : 36;
    }
    else if (p->side && p->nLinks == 1 && p->mouth == 0xFF)
    {
        // A dead end, deep along the way in.
        bool8 vertical = (p->guardDir == RDIR_NORTH || p->guardDir == RDIR_SOUTH);
        p->w = vertical ? 28 : 40;
        p->h = vertical ? 40 : 28;
    }
    else if (p->kind == RPLACE_ROUTE && p->type == RROUTE_BRIDGE)
    {
        // A bridge runs north to south, long and narrow between ways in
        // north and south; tall round a corner, square from west to east
        // (rogue_gen_bridge.h).
        p->w = (n && s) ? 32 : (e && w) ? 40 : 36;
        p->h = (n && s) ? 50 : (e && w) ? 40 : 44;
    }
    else if (p->type == RROUTE_COAST || FieldLike(p) || p->type == RROUTE_RIVER || p->type == RROUTE_SEA)
    {
        // Room for the sea, a river or wide meadows beside the way through.
        bool8 vertical = (n || s) && !(e || w);
        p->w = vertical ? 36 : 44;
        p->h = vertical ? 44 : 36;
    }
    else if (n && s && !e && !w)
    {
        p->w = 24;
        p->h = 44;
    }
    else if (e && w && !n && !s)
    {
        p->w = 44;
        p->h = 24;
    }
    else
    {
        p->w = 36;
        p->h = 36;
    }
}

// Where each opening sits along its side, and the connection offsets.
static void PlaceExits(struct RogueRegion *r, u32 *rng)
{
    int i, j;
    s8 vx, vy;
    // (v1.1) Where the volcano's crater is: routes beside it keep their
    // openings clear of the mountain's place (VolcanoFootExits).
    if (!VolcanoCell(r, &vx, &vy, FALSE))
        vx = vy = -1;
    for (i = 0; i < r->nPlaces; i++)
    {
        struct RoguePlace *p = &r->places[i];
        for (j = 0; j < p->nLinks; j++)
        {
            struct RogueLink *l = &p->links[j];
            bool8 across = (l->dir == RDIR_NORTH || l->dir == RDIR_SOUTH);
            int len = across ? p->w / 2 : p->h / 2;
            int first = 3, last = len - 4;
            // Side openings stay clear of a mesa along the top.
            if (p->mouth != 0xFF && !across)
                first = 6;
            // (v1.1 towns2: a crater town's side ways in come in at the
            // middle of its rings, clear of the top run of rim, where its
            // homes and stairs are, and of the bottom corners: seed 86's.)
            if (p->kind == RPLACE_TOWN && p->theme == RTHEME_CRATER && !across)
            {
                first = 6;
                last = len - 6;
            }
            // A harbor town's way to the sea route lies near the shore, so the
            // town's water runs on into the route's just beyond it.
            if (p->kind == RPLACE_TOWN && p->layout == RLAYOUT_HARBOR && p->seaSide != l->dir && p->seaSide != Opposite(l->dir)
             && IsSeaRoute(&r->places[l->target]))
            {
                if (p->seaSide == RDIR_WEST || p->seaSide == RDIR_NORTH)
                    first += 4;
                else
                    last -= 4;
                if (last < first)
                    last = first;
                l->pos = ((p->seaSide == RDIR_WEST || p->seaSide == RDIR_NORTH) ? first : last) * 2;
                continue;
            }
            // Openings on a side next to the sea stay clear of the shore.
            if (p->seaSide != RDIR_NONE && p->seaSide != l->dir && p->seaSide != Opposite(l->dir))
            {
                if (p->seaSide == RDIR_WEST || p->seaSide == RDIR_NORTH)
                    first += SEA_DEPTH + 2;
                else
                    last -= SEA_DEPTH + 2;
            }
            // On a route, keep away from the corner shared with an opening
            // on the next side, so the way through does not cut the corner.
            if (p->kind == RPLACE_ROUTE)
            {
                bool8 low = across ? HasSide(p, RDIR_WEST) : HasSide(p, RDIR_NORTH);
                bool8 high = across ? HasSide(p, RDIR_EAST) : LinkOnSide(p, RDIR_SOUTH) != NULL;
                int third = (last - first) / 3;
                if (low && !high)
                    first += (last - first) / 2;
                else if (high && !low)
                    last -= (last - first) / 2;
                else if (low && high)
                {
                    first += third;
                    last -= third;
                }
            }
            VolcanoFootExits(r, p, l->dir, len, vx, vy, &first, &last);
            if (last < first)
                last = first;
            l->pos = (first + RogueRng_Range(rng, last - first + 1)) * 2;
            // (A bridge's west way in high, its east one low.)
            l->pos = BridgeExitPos(p, l->dir, first, last, l->pos / 2) * 2;
        }
    }
    for (i = 0; i < r->nPlaces; i++)
    {
        struct RoguePlace *p = &r->places[i];
        for (j = 0; j < p->nLinks; j++)
        {
            struct RogueLink *l = &p->links[j];
            const struct RogueLink *back = RogueGen_FindLink(&r->places[l->target], i);
            l->offset = (s16)l->pos - (s16)back->pos;
        }
    }
}

// The Name Rater's town (ThemeInteriors) must keep a home for him: a
// village or hamlet that also holds a dungeon's way in (a tower, a hideout,
// a lair) can be too cramped to keep one, so he moves to another town of a
// theme that can hold him with no dungeon, where there is one. Run once the
// dungeons are planned.
static u8 ThemeInteriors(u8 theme);
static void MoveNameRater(struct RogueRegion *r)
{
    int i, from = -1, to = -1;
    for (i = 0; i < r->nTowns; i++)
        if (r->places[r->towns[i]].interiors & (1 << RINT_NAME_RATER))
            from = r->towns[i];
    if (from < 0 || r->places[from].type == RTOWN_GYM || RogueGen_DungeonAt(r, from) < 0
     || r->dungeons[RogueGen_DungeonAt(r, from)].host != from)
        return;
    for (i = 0; i < r->nTowns && to < 0; i++)
    {
        const struct RoguePlace *q = &r->places[r->towns[i]];
        s32 d = RogueGen_DungeonAt(r, r->towns[i]);
        if (r->towns[i] != from && (ThemeInteriors(q->theme) & (1 << RINT_NAME_RATER)) && q->type != RTOWN_SUMMIT
         && (d < 0 || r->dungeons[d].host != r->towns[i]))
            to = r->towns[i];
    }
    if (to < 0)
        return;
    r->places[from].interiors &= ~(1 << RINT_NAME_RATER);
    r->places[to].interiors |= 1 << RINT_NAME_RATER;
}

// Which town themes can hold which special interiors. Each special appears
// in one town per region at most: the first town whose theme can hold it.
static u8 ThemeInteriors(u8 theme)
{
    switch (theme)
    {
    case RTHEME_STONE:   return 1 << RINT_SCHOOL;
    case RTHEME_VERDANT: return 1 << RINT_BIKE_SHOP;
    case RTHEME_HARBOR:  return (1 << RINT_FAN_CLUB) | (1 << RINT_NAME_RATER);
    case RTHEME_COVE:    return (1 << RINT_FAN_CLUB) | (1 << RINT_MOVE_DELETER);
    case RTHEME_BEACH:   return 1 << RINT_NAME_RATER;
    case RTHEME_ASH:     return 1 << RINT_MOVE_DELETER;
    }
    return 0;
}

// The level curve: Emerald's gym, Elite Four and Champion levels, each
// region a little higher or lower.
static void RollCurve(struct RogueRegion *r, u32 *rng)
{
    static const u8 sEmeraldAces[ROGUE_NUM_GYMS] = { 15, 19, 24, 29, 31, 33, 42, 46 };
    static const u8 sEmeraldE4[ROGUE_NUM_E4] = { 49, 51, 53, 55 };
    int g, j;
    for (g = 0; g < ROGUE_NUM_GYMS; g++)
    {
        // The first three gyms never above Emerald's, so the early climb
        // stays steady; later ones a level either way, or two under.
        int ace = sEmeraldAces[g] + (int)RogueRng_Range(rng, g < 3 ? 3 : 4) - 2;
        if (g > 0 && ace < r->aceLevel[g - 1] + 2)
            ace = r->aceLevel[g - 1] + 2;
        r->aceLevel[g] = ace;
    }
    j = (int)RogueRng_Range(rng, 3) - 1;
    for (g = 0; g < ROGUE_NUM_E4; g++)
        r->e4Level[g] = sEmeraldE4[g] + j;
    r->championLevel = 58 + j;
    r->rivalLevel[0] = 5;
    r->rivalLevel[1] = r->aceLevel[2] + RogueRng_Range(rng, 2);
    r->rivalLevel[2] = r->aceLevel[5] + RogueRng_Range(rng, 2);
    r->rivalLevel[3] = r->e4Level[0] - 4;
}

static bool8 IsSpineArea(u8 kind)
{
    return kind == SP_ROUTE || kind == SP_MOUTH_ROUTE || kind == SP_CAVE || kind == SP_VICTORY;
}

// Trainer levels along the main road. Between two gyms they climb from a
// little under the last leader's ace to a little under the next one's, so
// the leaders stand out the way Emerald's do. The climb is spread evenly
// over the places between, so no one route jumps several levels.
static void AssignSpineLevels(struct RogueRegion *r, const u8 *spinePlace)
{
    int i = 0, g;
    for (g = 0; g <= ROGUE_NUM_GYMS; g++)
    {
        // After the first three gyms the road starts well under the last
        // ace, as Route 116 (trainers at 8 and 9) follows Roxanne's 15.
        int lo = g == 0 ? 3 : r->aceLevel[g - 1] - (g == ROGUE_NUM_GYMS ? 3 : (g <= 3 ? 6 : 4));
        // The road tops out well under the next ace on the way to gyms 2
        // to 4, as Emerald's do (Route 104's best trainers are Brawly's
        // ace less eight).
        int hi = g < ROGUE_NUM_GYMS ? r->aceLevel[g] - (g >= 1 && g <= 3 ? 7 : 5) : r->e4Level[0] - 5;
        int n = 0, k = 0, j, level = lo;
        if (hi < lo)
            hi = lo;
        for (j = i; j < SPINE_LEN && sSpine[j] != SP_GYM; j++)
            n += IsSpineArea(sSpine[j]);
        for (; i < SPINE_LEN && sSpine[i] != SP_GYM; i++)
        {
            struct RoguePlace *p = &r->places[spinePlace[i]];
            if (IsSpineArea(sSpine[i]))
            {
                // Before the first gym the first route stays gentle, as
                // Route 101 is, then the climb starts at Route 102's
                // level; its gym trainers are the last step. Later each
                // place takes the middle of its even share of the climb.
                if (g == 0)
                    level = k == 0 ? lo : 5 + (hi - 5) * (k - 1) / n;
                else
                    level = lo + ((hi - lo) * (2 * k + 1) + n) / (2 * n);
                k++;
            }
            p->level = level;
            p->badges = g;
        }
        if (i < SPINE_LEN)
        {
            // The gym town: its gym trainers are set by the leader's ace;
            // its water by the route before.
            r->places[spinePlace[i]].level = level;
            r->places[spinePlace[i]].badges = g;
            i++;
        }
    }
}

// Difficulty step of a level, for items, music and species strength. Matches
// the old route-by-route steps: 4, 7, 11, 15 and so on.
static u8 TierOfLevel(int level)
{
    int t = level <= 5 ? 0 : (level - 3) / 4;
    return t > 10 ? 10 : t;
}


// Sides of a place with no way out that face the open sea (or the edge of
// the map, which is sea), and those that face a lake.
static u8 FreeSidesFacing(const struct RoguePlace *p, u8 kind)
{
    u8 d, sides = 0;
    for (d = 0; d < 4; d++)
    {
        int x = p->gridX + sDirDX[d], y = p->gridY + sDirDY[d];
        bool8 out = (x < 0 || y < 0 || x >= RGRID_W || y >= RGRID_H);
        if (HasSide(p, d))
            continue;
        if (out ? kind == CELL_SEA : (sLand.cell[y][x] == kind && (kind != CELL_SEA || !sOcc[y][x])))
            sides |= 1 << d;
    }
    return sides;
}

static u8 PickSide(u32 *rng, u8 sides)
{
    u8 d, start = RogueRng_Range(rng, 4);
    for (d = 0; d < 4; d++)
        if (sides & (1 << ((start + d) & 3)))
            return (start + d) & 3;
    return RDIR_NONE;
}

// TRUE if the land feature lies within reach of a place.
static bool8 NearFeature(const struct RoguePlace *p, u8 feature, int reach)
{
    const struct RogueLand *L = &sLand;
    int dx, dy;
    if (L->feature != feature)
        return FALSE;
    dx = p->gridX < L->fx ? L->fx - p->gridX : p->gridX >= L->fx + L->fw ? p->gridX - (L->fx + L->fw - 1) : 0;
    dy = p->gridY < L->fy ? L->fy - p->gridY : p->gridY >= L->fy + L->fh ? p->gridY - (L->fy + L->fh - 1) : 0;
    return dx + dy <= reach;
}

// TRUE if the main road turns a corner through route p.
static bool8 MainTurns(const struct RogueRegion *r, const struct RoguePlace *p)
{
    u8 dirs = 0;
    int k;
    for (k = 0; k < p->nLinks; k++)
        if (!r->places[p->links[k].target].side)
            dirs |= 1 << p->links[k].dir;
    return dirs != ((1 << RDIR_NORTH) | (1 << RDIR_SOUTH)) && dirs != ((1 << RDIR_EAST) | (1 << RDIR_WEST))
        && (dirs & (dirs - 1)) != 0;
}

// Route kinds a block on the main road can stand on: green ones, where the
// walled way in is a line of trees. Not a lake: its big lake and the walled
// way in leave little room, and the map is tried again many times over.
static bool8 GuardType(u8 type)
{
    return type == RROUTE_FOREST || type == RROUTE_FIELD || type == RROUTE_WOODS;
}

// Route types along the main road: gentle woods and meadows first, then a
// shuffled mix, never three of a kind in a row. Coast routes stand where the
// sea is, lake routes by the lake, hills by the mountain, sand by the
// volcano. The route before a cave climbs hills, and Victory Road is a
// mountain. Side routes stay in the woods, where their obstacle has a
// narrow way to block.
static void AssignRouteTypes(struct RogueRegion *r, u32 *rng, const u8 *spinePlace)
{
    static const u8 sBag[] = { RROUTE_COAST, RROUTE_RIVER, RROUTE_FIELD, RROUTE_HILLS, RROUTE_DESERT,
                               RROUTE_WOODS, RROUTE_LAKE, RROUTE_COAST, RROUTE_FIELD, RROUTE_RIVER };
    u8 bag[sizeof(sBag)];
    int i, n = 0, last = -1, last2 = -1;

    for (i = 0; i < (int)SPINE_LEN; i++)
    {
        struct RoguePlace *p = &r->places[spinePlace[i]];
        u8 sea = FreeSidesFacing(p, CELL_SEA), lake = FreeSidesFacing(p, CELL_LAKE), want = 0xFF;
        if (p->kind != RPLACE_ROUTE || p->type == RROUTE_MOUNTAIN || p->type == RROUTE_SEA)
            continue;
        // No coast where the main road turns a corner and a side path takes
        // the way on, which leaves the sea far from the walk.
        if (p->nLinks >= 3 && MainTurns(r, p))
            sea = 0;
        if (p->mouth != 0xFF)
        {
            p->type = RROUTE_HILLS;
        }
        else if (p->tier == 0)
        {
            p->type = RogueRng_Range(rng, 2) ? RROUTE_FIELD : RROUTE_FOREST;
        }
        else
        {
            int k = 0, tries;
            // What the land around asks for, half the time.
            if (sea && RogueRng_Range(rng, 100) < 55)
                want = RROUTE_COAST;
            else if (lake && RogueRng_Range(rng, 100) < 60)
                want = RogueRng_Range(rng, 2) ? RROUTE_LAKE : RROUTE_RIVER;
            else if (NearFeature(p, FEATURE_MOUNTAIN, 1) && RogueRng_Range(rng, 100) < 50)
                want = RROUTE_HILLS;
            else if (NearFeature(p, FEATURE_VOLCANO, 1) && RogueRng_Range(rng, 100) < 50)
                want = RROUTE_DESERT;
            if (want != 0xFF && !(want == last && want == last2))
            {
                p->type = want;
            }
            else
            {
                for (tries = 0; tries < 20; tries++)
                {
                    if (n == 0)
                    {
                        memcpy(bag, sBag, sizeof(bag));
                        n = sizeof(bag);
                    }
                    k = RogueRng_Range(rng, n);
                    // The coast only where the sea is, and not three of a
                    // kind in a row.
                    if ((bag[k] == RROUTE_COAST && !sea) || (bag[k] == last && (bag[k] == last2 || tries < 10)))
                        continue;
                    break;
                }
                p->type = bag[k];
                if (p->type == RROUTE_COAST && !sea)
                    p->type = RROUTE_FIELD;
                bag[k] = bag[--n];
            }
            // Beside an ash town, some routes are ash all the way along,
            // but never the first ones.
            if (p->theme == RTHEME_ASH && p->type != RROUTE_COAST && !p->obstacle && RogueRng_Range(rng, 100) < 30
             && !(last == RROUTE_DESERT && last2 == RROUTE_DESERT))
            {
                int k2;
                for (k2 = 0; k2 < p->nLinks; k2++)
                    if (r->places[p->links[k2].target].kind == RPLACE_TOWN && r->places[p->links[k2].target].theme == RTHEME_ASH)
                        p->type = RROUTE_DESERT;
            }
        }
        // A block on the road beside a town stands on the route itself, in
        // green country at the end of a walled way in, as on a side route;
        // water to cross on a lake. Beside a route it stands in that
        // route's lane (ObstacleOutside).
        if (p->obstacle == ROBST_SURF)
            p->type = RROUTE_LAKE;
        else if (p->obstacle && !ObstacleOutside(r, p) && !GuardType(p->type))
        {
            static const u8 sGuard[] = { RROUTE_FOREST, RROUTE_FIELD, RROUTE_WOODS };
            int k = RogueRng_Range(rng, sizeof(sGuard)), tries;
            for (tries = 0; tries < 4 && sGuard[k] == last && sGuard[k] == last2; tries++)
                k = (k + 1) % sizeof(sGuard);
            p->type = sGuard[k];
        }
        last2 = last;
        last = p->type;
        // A rest house on some of the longer routes from the third on, not
        // where a block stands on the route: past its narrow way in there
        // is no room for one.
        if (p->tier >= 2 && p->type != RROUTE_HILLS && !(p->obstacle && !ObstacleOutside(r, p)) && RogueRng_Range(rng, 100) < 45)
            p->interiors |= 1 << RINT_REST;
    }
    // Never three of a kind in a row along the main road: one of a run (not
    // a route up to a cave, nor the sea) turns to another kind.
    {
        u8 road[SPINE_MAX];
        int nRoad = 0;
        static const u8 sOther[] = { RROUTE_FIELD, RROUTE_FOREST, RROUTE_WOODS, RROUTE_RIVER, RROUTE_LAKE, RROUTE_HILLS };
        for (i = 0; i < (int)SPINE_LEN; i++)
        {
            const struct RoguePlace *p = &r->places[spinePlace[i]];
            if (p->kind == RPLACE_ROUTE && p->type != RROUTE_MOUNTAIN)
                road[nRoad++] = spinePlace[i];
        }
        for (i = 2; i < nRoad; i++)
        {
            struct RoguePlace *a = &r->places[road[i - 2]], *b = &r->places[road[i - 1]], *c = &r->places[road[i]], *p;
            int k, start;
            if (a->type != b->type || b->type != c->type)
                continue;
            p = (c->mouth == 0xFF && c->type != RROUTE_SEA) ? c : b;
            if (p->mouth != 0xFF || p->type == RROUTE_SEA)
                p = a;
            start = RogueRng_Range(rng, sizeof(sOther));
            for (k = 0; k < (int)sizeof(sOther); k++)
            {
                u8 t = sOther[(start + k) % sizeof(sOther)];
                bool8 next = (p == c && i + 1 < nRoad && r->places[road[i + 1]].type == t);
                // Gentle kinds only on the first routes.
                if (t != b->type && !next && (p->tier > 0 || t == RROUTE_FIELD || t == RROUTE_FOREST || t == RROUTE_WOODS)
                 && (!p->obstacle || ObstacleOutside(r, p) || (GuardType(t) && p->obstacle != ROBST_SURF)))
                {
                    p->type = t;
                    p->interiors &= ~(1 << RINT_REST);
                    break;
                }
            }
        }
    }
    for (i = 0; i < r->nPlaces; i++)
    {
        struct RoguePlace *p = &r->places[i];
        if (p->kind != RPLACE_ROUTE)
            continue;
        // A side route over water is a lake.
        if (p->side)
            p->type = p->obstacle == ROBST_SURF ? RROUTE_LAKE
                    : RogueRng_Range(rng, 3) == 0 ? RROUTE_WOODS : (RogueRng_Range(rng, 2) ? RROUTE_LAKE : RROUTE_FOREST);
        // A coast has the sea along a side with no way out, facing the sea.
        if (p->type == RROUTE_COAST)
        {
            p->seaSide = PickSide(rng, FreeSidesFacing(p, CELL_SEA));
            if (p->seaSide == RDIR_NONE || (!p->side && p->nLinks >= 3 && MainTurns(r, p)))
            {
                p->type = RROUTE_FIELD;
                p->seaSide = RDIR_NONE;
            }
        }
    }
}

// v1.1: route kinds by zone (RUN 0's region keeps AssignRouteTypes).
#include "rogue_gen_zones.h"

// Gym cities on the coast themes may stand on the sea, on a side that faces
// it; others may have a plaza. Villages, hamlets and the summit keep to
// streets.
static void AssignTownLayouts(struct RogueRegion *r, u32 *rng)
{
    int i;
    for (i = 0; i < r->nTowns; i++)
    {
        struct RoguePlace *p = &r->places[r->towns[i]];
        bool8 coastal = (p->theme == RTHEME_HARBOR || p->theme == RTHEME_ISLAND || p->theme == RTHEME_COVE || p->theme == RTHEME_BEACH);
        u8 seaLink = RDIR_NONE;
        int k;
        p->layout = RLAYOUT_STREETS;
        // Where the sea route sets out or comes ashore, the town faces the
        // sea on a side beside it, as Slateport and Lilycove do, and the
        // water runs on into the route's.
        for (k = 0; k < p->nLinks; k++)
            if (IsSeaRoute(&r->places[p->links[k].target]))
                seaLink = p->links[k].dir;
        if (seaLink != RDIR_NONE && p->type != RTOWN_SUMMIT)
        {
            u8 across = ~((1 << seaLink) | (1 << Opposite(seaLink))), sea = FreeSidesFacing(p, CELL_SEA) & across;
            if (p->mouth != 0xFF)
                across &= 1 << RDIR_SOUTH;
            sea &= across;
            // Or a free cell of land on the shore beside it, which goes under
            // the sea when the map is drawn (TrimLand); or else any side with
            // no way out, as no map is seen beyond it.
            if (!sea)
            {
                u8 d;
                for (d = 0; d < 4; d++)
                {
                    int x = p->gridX + sDirDX[d], y = p->gridY + sDirDY[d];
                    if ((across & (1 << d)) && !HasSide(p, d) && x >= 0 && y >= 0 && x < RGRID_W && y < RGRID_H
                     && !sOcc[y][x] && ShoreCell(&sLand, x, y))
                        sea |= 1 << d;
                }
            }
            if (!sea)
            {
                u8 d;
                for (d = 0; d < 4; d++)
                    if ((across & (1 << d)) && !HasSide(p, d))
                        sea |= 1 << d;
            }
            p->seaSide = PickSide(rng, sea);
            if (p->seaSide != RDIR_NONE)
            {
                p->layout = RLAYOUT_HARBOR;
                continue;
            }
        }
        if (p->type != RTOWN_GYM)
            continue;
        if (coastal && RogueRng_Range(rng, 100) < 80)
        {
            // The sea on a side with no way out that faces it, best to the
            // south.
            static const u8 order[] = { RDIR_SOUTH, RDIR_EAST, RDIR_WEST, RDIR_NORTH };
            u8 sea = FreeSidesFacing(p, CELL_SEA);
            // Below a cave mouth's cliff the sea keeps to the south.
            if (p->mouth != 0xFF)
                sea &= 1 << RDIR_SOUTH;
            for (k = 0; k < 4 && p->seaSide == RDIR_NONE; k++)
                if (sea & (1 << order[k]))
                    p->seaSide = order[k];
            if (p->seaSide != RDIR_NONE)
            {
                p->layout = RLAYOUT_HARBOR;
                continue;
            }
        }
        // Fallarbor and Lavaridge are small towns of houses round a road,
        // with no square: a paved square on ash or sand stands empty.
        if (RogueRng_Range(rng, 100) < 55 && p->theme != RTHEME_ASH && p->theme != RTHEME_SPRING)
            p->layout = RLAYOUT_PLAZA;
    }
}

// TRUE for themes that belong by the sea: Slateport's harbor and Dewford's
// beach only stand on the coast.
static bool8 SeaOnlyTheme(u8 theme)
{
    return theme == RTHEME_HARBOR || theme == RTHEME_BEACH;
}

static bool8 SeaTheme(u8 theme)
{
    return SeaOnlyTheme(theme) || theme == RTHEME_COVE || theme == RTHEME_ISLAND;
}

// How well a theme suits a town where it stands.
static int ThemeWeight(const struct RogueRegion *r, const struct RoguePlace *p, u8 theme)
{
    bool8 coast = FreeSidesFacing(p, CELL_SEA) != 0;
    int k;
    // (v1.1 towns2) The raft village and the crater town are placed on
    // their own (src/rogue_gen_towns2.h).
    if (theme >= RTHEME_RAFT)
        return 0;
    for (k = 0; k < p->nLinks && !coast; k++)
        if (IsSeaRoute(&r->places[p->links[k].target]))
            coast = TRUE;
    if (SeaOnlyTheme(theme) && !coast)
        return 0;
    if (coast)
        return SeaTheme(theme) ? 30 : 6;
    if (SeaTheme(theme))
        return 2;
    if ((theme == RTHEME_ASH || theme == RTHEME_SPRING) && NearFeature(p, FEATURE_VOLCANO, 2))
        return 40;
    if (theme == RTHEME_STONE && NearFeature(p, FEATURE_MOUNTAIN, 2))
        return 30;
    if (theme == RTHEME_VERDANT && NearFeature(p, FEATURE_LAKE, 2))
        return 20;
    return 10;
}

// Picks a theme from the unused ones by weight; 0xFF if none fits.
static u8 PickTheme(const struct RogueRegion *r, u32 *rng, const struct RoguePlace *p, u16 used, u8 avoid)
{
    int total = 0, t, roll;
    for (t = 1; t < RTHEME_COUNT; t++)
        if (!(used & (1 << t)) && t != avoid)
            total += ThemeWeight(r, p, t);
    if (total == 0)
        return 0xFF;
    roll = RogueRng_Range(rng, total);
    for (t = 1; t < RTHEME_COUNT; t++)
    {
        if ((used & (1 << t)) || t == avoid)
            continue;
        roll -= ThemeWeight(r, p, t);
        if (roll < 0)
            return t;
    }
    return 0xFF;
}

// Themes: the first town is a meadow village and the summit plain. The gym
// cities each get their own theme, harbors and beaches only on the coast,
// inland cities first so the coast keeps the sea themes. Villages take an
// unused one or a meadow; a hamlet any but that of the town it hangs off.
static void AssignThemes(struct RogueRegion *r, u32 *rng, const u8 *from, const u8 *spinePlace)
{
    u16 used = 0;
    int pass, i;
    for (pass = 0; pass < 3; pass++)
    {
        for (i = 0; i < r->nTowns; i++)
        {
            struct RoguePlace *p = &r->places[r->towns[i]];
            bool8 coast = FreeSidesFacing(p, CELL_SEA) != 0;
            int k;
            u8 t;
            for (k = 0; k < p->nLinks; k++)
                coast |= IsSeaRoute(&r->places[p->links[k].target]);
            if (p->type == RTOWN_START || p->type == RTOWN_SUMMIT)
            {
                p->theme = RTHEME_MEADOW;
                continue;
            }
            // (v1.1 towns2) A raft village keeps Pacifidlog's look.
            if (p->theme == RTHEME_RAFT)
                continue;
            if (p->type == RTOWN_GYM && pass == (coast ? 1 : 0))
            {
                t = PickTheme(r, rng, p, used, 0xFF);
                if (t == 0xFF)
                {
                    // Every inland theme taken: a sea theme that can stand
                    // inland, else any left.
                    // (Not the raft village's or the crater town's, placed
                    // on their own: v1.1 towns2.)
                    for (t = 1; t < RTHEME_RAFT && ((used & (1 << t)) || SeaOnlyTheme(t)); t++)
                        ;
                    for (k = 1; t >= RTHEME_RAFT && k < RTHEME_RAFT; k++)
                        if (!(used & (1 << k)))
                            t = k;
                }
                p->theme = t;
                used |= 1 << t;
            }
            else if (p->type == RTOWN_VILLAGE && pass == 2)
            {
                t = PickTheme(r, rng, p, used, 0xFF);
                p->theme = t == 0xFF ? RTHEME_MEADOW : t;
                if (t != 0xFF)
                    used |= 1 << t;
            }
            else if (p->type == RTOWN_HAMLET && pass == 2)
            {
                // The town it hangs off: back along the way in to the main
                // road, then that place's town.
                int at = r->towns[i], guard;
                u8 avoid = 0xFF;
                for (guard = 0; guard < ROGUE_MAX_PLACES && r->places[at].side && from[at] != 0xFF; guard++)
                    at = from[at];
                // A route takes the theme of the town it leads to.
                for (k = 0; k < SPINE_LEN && spinePlace[k] != at; k++)
                    ;
                for (; k < SPINE_LEN && r->places[spinePlace[k]].kind != RPLACE_TOWN; k++)
                    ;
                if (k < SPINE_LEN)
                    avoid = r->places[spinePlace[k]].theme;
                t = PickTheme(r, rng, p, 0, avoid);
                p->theme = t == 0xFF ? RTHEME_MEADOW : t;
            }
        }
    }
}

static void FactsForget(void); // src/rogue_gen_facts.h

static void Run0Plan(struct RogueRegion *r);

void RogueGen_BuildRegion(struct RogueRegion *r, u32 seed)
{
    u32 rng = RogueRng_Hash(seed, 0);
    u8 spinePlace[SPINE_MAX], need[ROGUE_MAX_PLACES], from[ROGUE_MAX_PLACES];
    int i, j, routeNumber = 1, town = 0, gym = 0, cave = 0;
    u32 salt, rng0, land;
    u8 granted = 0, sea2;
    bool8 raft;

    // Facts kept from a region of this seed built before are of no use to
    // this one until it is whole.
    FactsForget();

    memset(r, 0, sizeof(*r));
    memset(spinePlace, 0, sizeof(spinePlace));
    r->seed = seed;
    r->tunnel = 0xFF;
    // Land shapes in turn until one takes the main road; the sea is left
    // out only if none will. v1.1 sea: a region with a second stretch of sea
    // tries a few lands with it, then starts again without it from the same
    // stream, so it is the region it was before.
    sea2 = seed == ROGUE_RUN0_SEED ? 0 : SeaSecondStretch(seed);
    // (v1.1 towns2) A raft village only in a region with one stretch of sea:
    // with two the road seldom lays, and each try costs seconds on the GBA.
    raft = !sea2 && Towns2RaftWanted(seed);
    rng0 = rng;
    for (salt = 0;; salt++)
    {
        // (v1.1 towns2: a raft village is tried on the first few lands
        // only, each land costing seconds on the GBA; then the region is
        // planned from the start as it would be without one.)
        if (raft && salt == TOWNS2_RAFT_LANDS)
        {
            raft = FALSE;
            salt = 0;
            rng = rng0;
        }
        if (sea2 && salt == SEA2_LANDS)
        {
            sea2 = 0;
            salt = 0;
            rng = rng0;
        }
        PlanSpine(&rng, salt < 40, sea2, raft);
        land = sea2 ? SEA2_LAND_0 + salt : salt;
        for (i = 0, j = 0; i < SPINE_LEN; i++)
        {
            j += (sSpine[i] == SP_GYM);
            sSpineBadges[i] = j;
        }
        PlanChapters(r, &rng);
        memset(r->places, 0, sizeof(r->places));
        r->nPlaces = 0;
        if (LayRegion(r, &rng, spinePlace, land) || salt >= 200)
            break;
    }
    Stages(r, need, from);

    RollCurve(r, &rng);
    AssignSpineLevels(r, spinePlace);

    // Numbers and roles along the main road.
    for (i = 0; i < (int)SPINE_LEN; i++)
    {
        struct RoguePlace *p = &r->places[spinePlace[i]];
        if (p->kind == RPLACE_ROUTE)
        {
            if (p->type != RROUTE_MOUNTAIN)
                p->number = routeNumber++;
            p->rival = sSpineRival[i];
        }
        else if (p->kind == RPLACE_CAVE)
        {
            p->number = cave++;
        }
        if (p->kind == RPLACE_TOWN)
        {
            r->towns[town] = spinePlace[i];
            p->number = town++;
            if (p->type == RTOWN_START)
                r->start = spinePlace[i];
            if (p->type == RTOWN_SUMMIT)
                r->summit = spinePlace[i];
            if (p->type == RTOWN_GYM)
            {
                p->gym = gym;
                r->gymTown[gym++] = spinePlace[i];
            }
        }
        if (sSpine[i] == SP_VICTORY)
            r->victoryRoad = spinePlace[i];
    }
    // Side places: the gyms won, in order along the road, when they first
    // open, and as hard as the place they are first reached from, or the
    // start of the stretch after those gyms, whichever is harder (a dark
    // cave a little harder), the way Emerald's optional areas match when you
    // can reach them. Numbered after the main road.
    {
        u8 stretch[ROGUE_NUM_GYMS + 1], done[ROGUE_MAX_PLACES];
        int b, pass;
        for (b = 0; b <= ROGUE_NUM_GYMS; b++)
        {
            stretch[b] = r->places[spinePlace[SPINE_LEN - 1]].level;
            for (i = 0; i < SPINE_LEN; i++)
            {
                if (IsSpineArea(sSpine[i]) && SpineArrive(i) >= b)
                {
                    stretch[b] = r->places[spinePlace[i]].level;
                    break;
                }
            }
        }
        for (i = 0; i < r->nPlaces; i++)
            done[i] = !r->places[i].side;
        for (pass = 0; pass < ROGUE_MAX_PLACES; pass++)
        {
            for (i = 0; i < r->nPlaces; i++)
            {
                struct RoguePlace *p = &r->places[i];
                const struct RoguePlace *q;
                if (done[i] || from[i] == 0xFF || !done[from[i]])
                    continue;
                q = &r->places[from[i]];
                p->badges = need[i] == 0xFF ? 0 : need[i];
                p->level = q->level;
                if (p->badges <= ROGUE_NUM_GYMS && stretch[p->badges] > p->level)
                    p->level = stretch[p->badges];
                if (p->kind == RPLACE_CAVE)
                    p->level = q->level + 2;
                done[i] = TRUE;
            }
        }
    }
    for (i = 0; i < r->nPlaces; i++)
    {
        struct RoguePlace *p = &r->places[i];
        if (!p->side)
            continue;
        if (p->kind == RPLACE_ROUTE)
            p->number = routeNumber++;
        else if (p->kind == RPLACE_CAVE)
            p->number = cave++;
        else
        {
            r->towns[town] = i;
            p->number = town++;
        }
    }
    r->nTowns = town;
    for (i = 0; i < r->nPlaces; i++)
        r->places[i].tier = TierOfLevel(r->places[i].level);

    AssignThemes(r, &rng, from, spinePlace);
    for (i = 0; i < r->nTowns; i++)
    {
        struct RoguePlace *p = &r->places[r->towns[i]];
        p->interiors = ThemeInteriors(p->theme) & ~granted;
        if (p->type == RTOWN_START)
            p->interiors |= 1 << RINT_LAB;
        granted |= p->interiors;
    }
    // Routes and caves use the tileset of the town they lead to on the main
    // road (Victory Road the one it leaves); side places that of the town
    // they hang off, or of the hamlet at their end.
    for (i = 0; i < (int)SPINE_LEN; i++)
    {
        struct RoguePlace *p = &r->places[spinePlace[i]];
        int k;
        if (p->kind == RPLACE_TOWN)
            continue;
        if (sSpine[i] == SP_VICTORY || (sSpine[i] == SP_CAVE && i + 1 == (int)SPINE_LEN - 1))
        {
            p->theme = r->places[spinePlace[i - 1]].theme;
            if (sSpine[i] == SP_CAVE)
                p->theme = r->places[spinePlace[i - 2]].theme;
            continue;
        }
        for (k = i + 1; k < (int)SPINE_LEN; k++)
        {
            if (r->places[spinePlace[k]].kind == RPLACE_TOWN)
            {
                p->theme = r->places[spinePlace[k]].theme;
                break;
            }
        }
        // The route out of an ash town is ash too where it leaves, as
        // Route 113 is beside Fallarbor.
        if (p->kind == RPLACE_ROUTE && i > 0 && r->places[spinePlace[i - 1]].kind == RPLACE_TOWN
         && r->places[spinePlace[i - 1]].theme == RTHEME_ASH)
            p->theme = RTHEME_ASH;
    }
    for (i = 0; i < r->nPlaces; i++)
    {
        struct RoguePlace *p = &r->places[i];
        int k;
        if (!p->side || p->kind == RPLACE_TOWN)
            continue;
        p->theme = RTHEME_MEADOW;
        for (k = 0; k < p->nLinks; k++)
            if (r->places[p->links[k].target].kind == RPLACE_TOWN)
                p->theme = r->places[p->links[k].target].theme;
        if (p->kind == RPLACE_CAVE)
            p->theme = r->places[p->caveIn].theme;
        else if (p->theme == RTHEME_MEADOW)
            for (k = 0; k < p->nLinks; k++)
                p->theme = r->places[p->links[k].target].theme;
    }

    // v1.1 zones: RUN 0's region keeps the old route kinds.
    if (RogueGen_IsRun0(r))
        AssignRouteTypes(r, &rng, spinePlace);
    else
    {
        AssignZoneRouteTypes(r, spinePlace);
        LandFitZoneKinds(r); // v1.1 land: the tilesets flowers and long grass need
        // At most one bridge route, over open water (v1.1 bridge,
        // src/rogue_gen_bridge.h).
        PickBridgeRoute(r);
    }
    WaterPlanRoutes(r); // (rogue_gen_water.h; after every route kind is known)
    AssignTownLayouts(r, &rng);
    Towns2Plan(r); // (v1.1 towns2: the raft village's layout, the crater town)

    // Town names.
    for (i = 0; i < r->nTowns; i++)
    {
        const struct RoguePlace *p = &r->places[r->towns[i]];
        const char *suffix = p->type == RTOWN_SUMMIT ? " SUMMIT" : (p->type == RTOWN_GYM ? " CITY" : " TOWN");
        bool8 unique;
        do
        {
            MakeTownName(&rng, r->townNames[i], suffix);
            unique = TRUE;
            for (j = 0; j < i; j++)
                if (SameName(r->townNames[i], r->townNames[j]))
                    unique = FALSE;
        } while (!unique);
    }
    Towns2Names(r); // (v1.1 towns2: a raft village's and a crater town's names fit them)

    // Sizes and openings.
    for (i = 0; i < r->nPlaces; i++)
        SizePlace(&r->places[i]);
    VolcanoFootSizes(r); // (v1.1, src/rogue_gen_volcano.h)
    PlaceExits(r, &rng);

    // Encounter areas, trainers, items and berry trees. Routes and caves in
    // place order, then towns (for fishing).
    for (i = 0; i < r->nPlaces; i++)
    {
        struct RoguePlace *p = &r->places[i];
        if (p->kind == RPLACE_TOWN)
            continue;
        p->area = r->nAreas++;
        if (p->kind == RPLACE_CAVE)
            p->nTrainers = p->dark ? 2 : 3;
        else if (p->side)
            p->nTrainers = p->branch == RBRANCH_SIDE ? 3 : 2;
        else if (p->type == RROUTE_MOUNTAIN)
            p->nTrainers = 5;
        else if (p->number == 1)
            p->nTrainers = 1; // a gentle first route: one trainer with one Pokemon
        else
            p->nTrainers = p->tier < 2 ? 4 : 5; // Emerald's routes have five or more
        p->trainerBase = r->nTrainerSlots;
        r->nTrainerSlots += p->nTrainers;
        p->nItems = p->side ? 3 : (p->kind == RPLACE_CAVE ? 2 : (p->tier >= 4 ? 3 : 2));
        p->itemBase = r->nItems;
        r->nItems += p->nItems;
        if (p->kind == RPLACE_ROUTE && p->type != RROUTE_MOUNTAIN)
        {
            p->nBerries = 2;
            p->berryBase = r->nBerries;
            r->nBerries += 2;
        }
    }
    PlanDungeons(r);
    SeaPlanKinds(r); // v1.1 sea: after the wreck (src/rogue_gen_sea.h)
    MoveNameRater(r);
    // The campers by the road take the last item flags: Victory Road's,
    // then LATE CAMP's (RogueGen_RoadCampFlag).
    r->nItems++;
    if (RogueGen_RouteCamp(r) >= 0)
        r->nItems++;
    for (i = 0; i < r->nTowns && r->nAreas < ROGUE_MAX_AREAS; i++)
        r->places[r->towns[i]].area = r->nAreas++;
    ShareLairAreas(r);
    // RUN 0's region, changed by hand (src/rogue_gen_run0.h).
    if (RogueGen_IsRun0(r))
        Run0Plan(r);
}

s32 RogueGen_RoadCampFlag(const struct RogueRegion *r, u32 place)
{
    s32 late = RogueGen_RouteCamp(r);
    if (late >= 0 && place == (u32)late)
        return r->nItems - 1;
    if (place == r->victoryRoad)
        return r->nItems - 1 - (late >= 0);
    return -1;
}

s32 RogueGen_RouteCamp(const struct RogueRegion *r)
{
    s32 best = -1;
    int i;
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        if (p->kind != RPLACE_ROUTE || p->side || p->badges != 6 || p->type == RROUTE_MOUNTAIN || p->type == RROUTE_SEA)
            continue;
        if (p->rival == 3)
            return i;
        if (best < 0)
            best = i;
    }
    return best;
}


// ---------------------------------------------------------------------------
// One map

#include "rogue_gen_run0.h"
#include "rogue_gen_dungeon.h"
#include "rogue_gen_items.h"
#include "rogue_gen_lore.h"
#include "rogue_gen_names.h"
#include "rogue_gen_people.h"
#include "rogue_gen_signatures.h"
#include "rogue_gen_facts.h"
#include "rogue_gen_features.h"
#include "rogue_gen_text.h"
#include "rogue_gen_rock.h"
#include "rogue_gen_water.h"
#include "rogue_gen_vert.h"
// v1.1 sea: more sea, and seas that differ.
#include "rogue_gen_sea.h"
// v1.1 towns2: raft villages and crater towns.
#include "rogue_gen_towns2.h"

// Hidden items, as Emerald hides them (RogueGen_HiddenCount): at the ends of
// dead ends first, then in quiet corners off the road, on a beach or an
// island. They go in once the map is drawn, on ground that can be walked on,
// with their own random stream so the rest of the map stays as it was.
static bool8 TryHidden(struct Gen *gen, int fx, int fy, bool8 sea)
{
    int i;
    if (!InMap(gen, fx, fy) || gen->m->nSigns >= ROGUE_MAX_SIGNS || Cell(gen, fx >> 1, fy >> 1) != C_OPEN
     || SeaSandBittenF(gen, fx, fy))
        return FALSE;
    if ((!sea && !Reached(gen, fx >> 1, fy >> 1)) || IsOccupied(gen, fx, fy) || ByLedgeF(gen, fx, fy))
        return FALSE;
    // (v1.1 sea: and reached on foot past everyone standing: rogue_gen_sea.h.)
    if (!sea && !SeaHiddenReached(gen, fx, fy))
        return FALSE;
    if ((gen->m->blocks[fy * gen->m->w + fx] >> 10) & 3)
        return FALSE;
    // Not where someone stands (trainers move to the walk once drawn), on a
    // door, or right beside another hidden item.
    for (i = 0; i < gen->m->nObjs; i++)
        if (Abs(gen->m->objs[i].x - fx) + Abs(gen->m->objs[i].y - fy) < 2)
            return FALSE;
    for (i = 0; i < gen->m->nWarps; i++)
        if (Abs(gen->m->warps[i].x - fx) + Abs(gen->m->warps[i].y - fy) < 2)
            return FALSE;
    for (i = 0; i < gen->m->nSigns; i++)
        if (gen->m->signs[i].kind >= RSIGN_HIDDEN && Abs(gen->m->signs[i].x - fx) + Abs(gen->m->signs[i].y - fy) < 6)
            return FALSE;
    return TRUE;
}

static void PlaceHiddenItems(struct Gen *gen)
{
    u32 n = RogueGen_HiddenCount(gen->r, gen->m->place), base = RogueGen_HiddenBase(gen->r, gen->m->place), k = 0;
    u32 saved = gen->rng;
    bool8 sea = gen->p->type == RROUTE_SEA;
    int i, tries;
    if (base >= ROGUE_MAX_HIDDEN || (!sea && gen->nPath == 0))
        return;
    if (base + n > ROGUE_MAX_HIDDEN)
        n = ROGUE_MAX_HIDDEN - base;
    gen->rng = RogueRng_Hash(gen->r->seed, 17600 + gen->m->place);
    if (!sea)
    {
        FloodReachable(gen, gen->path[0] % gen->cw, gen->path[0] / gen->cw);
        SeaFootReach(gen, gen->onWalk);
    }
    for (i = 0; i < gen->nPockets && k < n; i++)
    {
        int fx = gen->pocketX[i] * 2 + RAND(2), fy = gen->pocketY[i] * 2 + RAND(2);
        if (TryHidden(gen, fx, fy, sea))
            AddSign(gen, fx, fy, RSIGN_HIDDEN + k++);
    }
    for (tries = 0; tries < 400 && k < n; tries++)
    {
        int x = 1 + RAND(gen->cw - 2), y = 1 + RAND(gen->ch - 2), walls = 0, fx, fy;
        u8 d;
        for (d = 0; d < 4; d++)
            if (!IsOpenU(Cell(gen, x + sDirDX[d], y + sDirDY[d])))
                walls++;
        if (walls < (tries < 200 ? 2 : 1) || (tries < 300 && HasNeighbor(gen, x, y, C_ROAD, FALSE)))
            continue;
        // A coast's beach first.
        if (tries < 100 && gen->p->type == RROUTE_COAST && !IsBeachU(gen, x, y))
            continue;
        fx = x * 2 + RAND(2);
        fy = y * 2 + RAND(2);
        if (TryHidden(gen, fx, fy, sea))
            AddSign(gen, fx, fy, RSIGN_HIDDEN + k++);
    }
    gen->rng = saved;
}

// The tunnel's hidden item, a few steps from its far mouth, once the map
// is drawn, on its own random stream.
static void PlaceTunnelHidden(struct Gen *gen)
{
    u32 saved = gen->rng;
    int tries;
    if (!RogueGen_TunnelMouthAt(gen->r, gen->m->place) || RogueGen_TunnelHidden(gen->r) >= ROGUE_MAX_HIDDEN || !gen->hasMesa)
        return;
    gen->rng = RogueRng_Hash(gen->r->seed, 17650);
    FloodReachable(gen, gen->mouthX >> 1, 3);
    for (tries = 0; tries < 200; tries++)
    {
        int fx = gen->mouthX + (int)RAND(11) - 5, fy = 6 + RAND(3 + tries / 40);
        if (Abs(fx - gen->mouthX) < 2 || !TryHidden(gen, fx, fy, FALSE))
            continue;
        AddSign(gen, fx, fy, RSIGN_HIDDEN_TUNNEL);
        break;
    }
    gen->rng = saved;
}

void RogueGen_GenerateMap(const struct RogueRegion *r, u8 place, struct RogueMap *m, u16 *blocks)
{
    const struct RoguePlace *p = &r->places[place];
    struct Gen *gen = &sGen;
    u32 attempt = 0, salt = Run0MapSalt(r, place);
    bool8 ok;

    // RUN 0's copy of Route 101 is Route 101's own blocks.
    if ((s32)place == RogueGen_Run0CopyRoute(r))
    {
        Run0GenCopy(r, place, m, blocks);
        return;
    }
    do
    {
        int k;
        YIELD();
        memset(m, 0, sizeof(*m));
        m->place = place;
        m->blocks = blocks;
        m->w = p->w;
        m->h = p->h;
        YIELD();
        memset(gen, 0, sizeof(*gen));
        gen->r = r;
        gen->p = p;
        gen->m = m;
        gen->cw = m->w / 2;
        gen->ch = m->h / 2;
        gen->attempt = attempt;
        gen->bites = p->kind != RPLACE_TOWN;
        gen->rng = RogueRng_Hash(r->seed, 100 + place * 1000 + attempt++) ^ salt;
        for (k = 0; k < 4; k++)
            gen->exitPos[k] = 0xFF;
        if (p->kind == RPLACE_CAVE)
            ok = GenCave(gen);
        else if (p->kind == RPLACE_TOWN)
            ok = GenTown(gen);
        else
            ok = GenRoute(gen);
    } while (!ok && (attempt < 60 || (attempt < SigGiveUp(r, place) + 30 && SigTownTries(r, place))));
    if (ok && p->kind == RPLACE_ROUTE)
        PlaceHiddenItems(gen);
    if (ok)
        PlaceTunnelHidden(gen);
    if (ok && RogueGen_IsRun0(r))
        Run0Finish(gen);
#ifdef ROGUE_DEBUG
    fprintf(stderr, "place %d kind %d type %d attempts %u ok %d wetMiss %d\n", place, p->kind, p->type, attempt, ok, gen->wetMiss);
#endif
}
