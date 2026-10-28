#ifndef GUARD_ROGUE_RUN0_REGION_H
#define GUARD_ROGUE_RUN0_REGION_H

// RUN 0's region: the fixed region the Director made first (Beta v1.0).
// Shared by the generator (src/rogue_gen_run0.h, portable C, so gen_host
// builds it too), the game (src/rogue_run0_region.c) and the story's scripts.
//
// The region is built from ROGUE_RUN0_SEED (include/rogue_gen.h) like any other, so it is the same
// for every player, then changed by hand (RogueGen_IsRun0): the eight custom
// leaders, real Emerald maps as islands entered through doors, stitched maps
// cut from real ones, corruption that grows with the act, and the places the
// story attaches its scenes to (the hooks below).

// What the generator asks the game (and gen_host) for. Acts are 1 to 4,
// 5 the ending (VAR_ROGUE_RUN0_ACT); 0 reads as 1.
#define RUN0_ACT_MAX 5

// Counters of entries, for the maps that change on each visit
// (RogueGen_Run0Visit). The game keeps them for the session only.
enum
{
    RUN0_VISIT_RELAY,   // the town laid out again on each entry
    RUN0_VISIT_COPY,    // the copy of Route 101 that changes a little each visit
    RUN0_VISIT_VICTORY, // Victory Road, thinning to a path over black in act 4
    RUN0_VISIT_COUNT,
};

// Story state the maps follow (RogueGen_Run0State).
#define RUN0_STATE_TERRY_DELETED (1 << 0) // act 4: Terry's house is an empty lot

// ---------------------------------------------------------------------------
// Place hooks for the story. Each is found by RogueGen_Run0HookPlace; what
// stands there is drawn by the generator, and the game gives it the default
// script Rogue_EventScript_Run0Hook (src/rogue_run0_region.c, Rogue_Run0HookTalk),
// which the story replaces by hook.
//
// House hooks: an ordinary home of a town (its door, plate and room stay a
// home's); inside, the game builds an empty room (RINT_RUN0 in the house
// map) and the story adds its people. RogueGen_Run0HouseHook says which
// house of a town map is which.
// Object hooks: a person or thing standing on the map (ROBJ_RUN0, slot = the
// hook), with the graphics in sRun0HookGfx (src/rogue_run0_region.c).
// Sign hooks: a sign post (RSIGN_RUN0 + k), read with the hook's script.
enum RogueRun0Hook
{
    RUN0_HOOK_LIBRARY,        // house, act 3: the library whose rooms repeat (one region per shelf)
    RUN0_HOOK_TERRY_HOUSE,    // house, act 3: Terry's quiet house. An empty lot once
                              // FLAG_ROGUE_RUN0_TERRY_DELETED is set (act 4): no door, no house.
    RUN0_HOOK_BELIEVER_0,     // house, act 2: the Believers (Hex Maniacs and kids who love the Director)
    RUN0_HOOK_BELIEVER_1,     // house, act 2
    RUN0_HOOK_BELIEVER_2,     // house, act 2
    RUN0_HOOK_GIFT_BOX,       // object, act 3: the daily gift box beside a POKeMON CENTER's door
    RUN0_HOOK_CHAMPION_TERRY, // object, act 4: CHAMPION TERRY, patched out, at the end of Victory Road
                              // (beside the cave's way out to the League)
    RUN0_HOOK_PROMPT_0,       // (7: data/scripts/rogue.inc uses the number) signs: PROMPT LOG notes, two an act, on routes beside the way
    RUN0_HOOK_PROMPT_1,       //   (act 1: 0 and 1, act 2: 2 and 3, act 3: 4 and 5, act 4: 6 and 7)
    RUN0_HOOK_PROMPT_2,
    RUN0_HOOK_PROMPT_3,
    RUN0_HOOK_PROMPT_4,
    RUN0_HOOK_PROMPT_5,
    RUN0_HOOK_PROMPT_6,
    RUN0_HOOK_PROMPT_7,
    RUN0_HOOK_GRAVEYARD,      // dungeon, act 2: the graveyard tower (a generated tower, never Mt. Pyre);
                              // RogueGen_Run0Graveyard gives the dungeon, its graves are the tower's
    RUN0_HOOK_COUNT,
};
#define RUN0_HOOK_PROMPTS 8
#define RUN0_HOOK_IS_HOUSE(h) ((h) <= RUN0_HOOK_BELIEVER_2)
#define RUN0_HOOK_IS_OBJECT(h) ((h) == RUN0_HOOK_GIFT_BOX || (h) == RUN0_HOOK_CHAMPION_TERRY)
#define RUN0_HOOK_IS_SIGN(h) ((h) >= RUN0_HOOK_PROMPT_0 && (h) <= RUN0_HOOK_PROMPT_7)

// Not places in the region:
// - The Champion's dark empty room: MAP_ROGUE_CHAMPION_ROOM is dark in RUN 0
//   (Flash lights a circle; the story puts the DIRECTOR's battle in it).
// - Terry by the moving truck in real Littleroot (the STOP ending): stand him
//   here, facing down, beside Brendan's truck (the truck object at 2,10).
#define RUN0_LITTLEROOT_TERRY_X 4
#define RUN0_LITTLEROOT_TERRY_Y 10

// ---------------------------------------------------------------------------
// Real Emerald maps, untouched, as islands. Each is entered through the door
// of an ordinary home in a generated town (RogueGen_Run0IslandDoor) and comes
// out of one of the real map's own doors; that door, and every other way off
// the island, leads back to the generated door. Edges with no real map
// beyond them end in black.
enum RogueRun0Island
{
    RUN0_ISLAND_DEWFORD,   // act 1: Dewford Town, an island already (in and out by a home's door)
                           // (Petalburg Woods was the first choice: at 48 x 44 it is too big for
                           // the map buffer Roguemon shrank, MAX_MAP_DATA_SIZE)
    RUN0_ISLAND_LITTLEROOT,// act 2: Oldale Town, Route 101 and Littleroot Town (in by Oldale's house)
    RUN0_ISLAND_FALLARBOR, // act 3: Fallarbor Town (in and out by the Battle Tent's door)
    RUN0_ISLAND_PYRE,      // act 4: Mt. Pyre's outside (in and out by the mountain's door)
    RUN0_ISLAND_COUNT,
};

// Real signs moved onto generated routes, pointing nowhere (RSIGN_RUN0_REAL
// + one of these): each reads Emerald's own sign script.
enum
{
    RUN0_SIGN_ROUTE101,   // Route 101's: ROUTE 101, OLDALE TOWN, LITTLEROOT TOWN
    RUN0_SIGN_LITTLEROOT, // "LITTLEROOT TOWN: A town that can't be shaded any hue."
    RUN0_SIGN_OLDALE,     // Oldale's town sign
    RUN0_SIGN_PETALBURG,  // Petalburg's city sign
    RUN0_SIGN_COUNT,
};

// RogueGen_Run0HouseHook: a home whose door leads to a real map's island.
#define RUN0_HOUSE_ISLAND 0x40

#endif // GUARD_ROGUE_RUN0_REGION_H
