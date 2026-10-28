// RUN 0 in the game (include/rogue_run0_region.h; the region itself is made in
// src/rogue_gen_run0.h): what the generator asks the game, the real Emerald
// maps as islands, the music and colors of generated maps, and the story's
// hooks.
//
// Real maps are Emerald's own, untouched: their blocks, people, signs,
// scripts and music. Only what would lead out of the island or start
// Emerald's story is held back, from outside the maps: their ways off the
// island lead back to the generated door (Rogue_Run0WarpOut), edges with no
// real map beyond them draw black (Rogue_Run0Border), Oldale's and
// Fallarbor's roads to Routes 102, 103, 113 and 114 are not joined
// (Rogue_Run0PatchHeader), the story's vars are set to after Emerald's
// opening so no scene starts (Run0IslandState), and item balls, hidden
// items and trainers stay away: their flags and trainer ids are the
// generated region's own (Rogue_Run0OnObjectsLoaded, Rogue_Run0HiddenItem).

#include "global.h"
#include "rogue.h"
#include "rogue_gen.h"
#include "rogue_run0_story.h"
#include "event_data.h"
#include "event_scripts.h"
#include "fieldmap.h"
#include "m4a.h"
#include "overworld.h"
#include "palette.h"
#include "script.h"
#include "string_util.h"
#include "gba/m4a_internal.h"
#include "constants/rgb.h"
#include "constants/event_objects.h"
#include "constants/flags.h"
#include "constants/map_groups.h"
#include "constants/trainer_types.h"
#include "constants/vars.h"

#define ROGUE_GROUP MAP_GROUP(MAP_ROGUE_PLACE_0)

extern const u8 Common_EventScript_FindItem[];

// Entries to the maps that change on each visit (RogueGen_Run0Visit), and
// the count each one's cached map was made with. The session only: after a
// reload they start again, which the maps do not mind.
static EWRAM_DATA u8 sRun0Visits[RUN0_VISIT_COUNT] = {0};
static EWRAM_DATA u8 sRun0MadeWith[RUN0_VISIT_COUNT] = {0};

bool32 Rogue_Run0Active(void)
{
    return Rogue_GetSeed() == ROGUE_RUN0_SEED;
}

u8 RogueGen_Run0Act(void)
{
    return VarGet(VAR_ROGUE_RUN0_ACT);
}

u8 RogueGen_Run0Visit(u8 which)
{
    return which < RUN0_VISIT_COUNT ? sRun0Visits[which] : 0;
}

u16 RogueGen_Run0State(void)
{
    return FlagGet(FLAG_ROGUE_RUN0_TERRY_DELETED) ? RUN0_STATE_TERRY_DELETED : 0;
}

// Which counter a place's map follows, or -1.
static s32 VisitKind(u32 place)
{
    const struct RogueRegion *r;
    if (!Rogue_Run0Active())
        return -1;
    r = Rogue_GetRegion();
    if ((s32)place == RogueGen_Run0RelayTown(r))
        return RUN0_VISIT_RELAY;
    if ((s32)place == RogueGen_Run0CopyRoute(r))
        return RUN0_VISIT_COPY;
    if (place == r->victoryRoad && RogueGen_Run0Act() >= 4)
        return RUN0_VISIT_VICTORY;
    return -1;
}

// A place's map was made (src/rogue.c FillSlot).
void Rogue_Run0Generated(u32 place)
{
    s32 k = VisitKind(place);
    if (k >= 0)
        sRun0MadeWith[k] = sRun0Visits[k];
}

// The player walks into place: TRUE if its cached map is from another visit
// and must be made again now.
bool32 Rogue_Run0Entering(u32 place)
{
    s32 k = VisitKind(place);
    return k >= 0 && sRun0MadeWith[k] != sRun0Visits[k];
}

// ...and once it is made for this visit, the next one will differ.
void Rogue_Run0Entered(u32 place)
{
    s32 k = VisitKind(place);
    if (k >= 0 && sRun0Visits[k] < 255)
        sRun0Visits[k]++;
}

// ---------------------------------------------------------------------------
// Islands

struct Run0IslandMap
{
    u16 map;   // MAP_*
    u8 island; // enum RogueRun0Island
    u8 back;   // bit per warp of the map that leads back to the generated door
};

static const struct Run0IslandMap sIslandMaps[] =
{
    { MAP_DEWFORD_TOWN,     RUN0_ISLAND_DEWFORD,    0x0C }, // a home (the way in) and the gym (Emerald's story)
    { MAP_OLDALE_TOWN,      RUN0_ISLAND_LITTLEROOT, 0x01 }, // the house the island is entered by
    { MAP_ROUTE101,         RUN0_ISLAND_LITTLEROOT, 0x00 },
    { MAP_LITTLEROOT_TOWN,  RUN0_ISLAND_LITTLEROOT, 0x07 }, // the two homes and the lab: Emerald's story
    { MAP_FALLARBOR_TOWN,   RUN0_ISLAND_FALLARBOR,  0x02 }, // the Battle Tent
    { MAP_MT_PYRE_EXTERIOR, RUN0_ISLAND_PYRE,       0x07 }, // the mountain's door and the summit
};

// Where each island is entered: the real map's warp the player comes out of.
static const struct { u16 map; u8 warp; } sIslandArrivals[RUN0_ISLAND_COUNT] =
{
    [RUN0_ISLAND_DEWFORD]    = { MAP_DEWFORD_TOWN, 3 },
    [RUN0_ISLAND_LITTLEROOT] = { MAP_OLDALE_TOWN, 0 },
    [RUN0_ISLAND_FALLARBOR]  = { MAP_FALLARBOR_TOWN, 1 },
    [RUN0_ISLAND_PYRE]       = { MAP_MT_PYRE_EXTERIOR, 0 },
};

static const struct Run0IslandMap *IslandMap(u16 group, u16 num)
{
    u32 i;
    if (group == ROGUE_GROUP || !Rogue_Run0Active())
        return NULL;
    for (i = 0; i < ARRAY_COUNT(sIslandMaps); i++)
        if (MAP_GROUP(sIslandMaps[i].map) == group && MAP_NUM(sIslandMaps[i].map) == num)
            return &sIslandMaps[i];
    return NULL;
}

static const struct Run0IslandMap *IslandHere(void)
{
    return IslandMap(gSaveBlock1Ptr->location.mapGroup, gSaveBlock1Ptr->location.mapNum);
}

s32 Rogue_Run0IslandHere(void)
{
    const struct Run0IslandMap *i = IslandHere();
    return i ? i->island : -1;
}

// The generated door of an island: its town and house, or FALSE.
static bool32 IslandDoor(u32 island, u32 *place, u32 *house)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    const struct RogueMap *m;
    s32 town = RogueGen_Run0IslandTown(r, island);
    u32 k;
    if (town < 0 || (m = Rogue_GetPlaceMap(town)) == NULL)
        return FALSE;
    for (k = 0; k < m->nHouses; k++)
    {
        if (RogueGen_Run0HouseHook(r, m, k) == RUN0_HOUSE_ISLAND + (s32)island)
        {
            *place = town;
            *house = k;
            return TRUE;
        }
    }
    return FALSE;
}

// A house door of a generated town that leads to an island (src/rogue.c
// BuildWarps): where it leads, or FALSE.
bool32 Rogue_Run0IslandDoorDest(const struct RogueMap *m, u32 house, struct WarpEvent *w)
{
    s32 what;
    if (!Rogue_Run0Active())
        return FALSE;
    what = RogueGen_Run0HouseHook(Rogue_GetRegion(), m, house);
    if (what < RUN0_HOUSE_ISLAND || what >= RUN0_HOUSE_ISLAND + RUN0_ISLAND_COUNT)
        return FALSE;
    what -= RUN0_HOUSE_ISLAND;
    w->mapGroup = MAP_GROUP(sIslandArrivals[what].map);
    w->mapNum = MAP_NUM(sIslandArrivals[what].map);
    w->warpId = sIslandArrivals[what].warp;
    return TRUE;
}

// A warp of an island map that leads off the island goes back to the
// generated door instead (field_control_avatar.c SetupWarp).
bool32 Rogue_Run0WarpOut(s8 warpEventId)
{
    const struct Run0IslandMap *i = IslandHere();
    u32 place, house;
    if (i == NULL || warpEventId < 0 || warpEventId >= 8 || !((i->back >> warpEventId) & 1))
        return FALSE;
    if (!IslandDoor(i->island, &place, &house))
        return FALSE;
    SetWarpDestination(ROGUE_GROUP, place, ROGUE_HOUSE_WARP(house), -1, -1);
    return TRUE;
}

// Oldale's, Fallarbor's and Dewford's roads and seas lead off the island: not joined.
static const struct MapConnection sOldaleConnections[] =
{
    { CONNECTION_SOUTH, 0, MAP_GROUP(MAP_ROUTE101), MAP_NUM(MAP_ROUTE101) },
};
static const struct MapConnections sOldaleIsland = { ARRAY_COUNT(sOldaleConnections), sOldaleConnections };
static const struct MapConnections sNoConnections = { 0, NULL };

// The map header the engine is about to use (overworld.c LoadCurrentMapData).
void Rogue_Run0PatchHeader(struct MapHeader *h, u16 group, u16 num)
{
    const struct Run0IslandMap *i;
    if (!Rogue_Run0Active())
        return;
    // The Champion's room is dark and empty.
    if (group == MAP_GROUP(MAP_ROGUE_CHAMPION_ROOM) && num == MAP_NUM(MAP_ROGUE_CHAMPION_ROOM))
        h->cave = TRUE;
    // The story's own (TERRY by the truck in the STOP ending).
    Rogue_Run0StoryHeader(h, group, num);
    i = IslandMap(group, num);
    if (i == NULL)
        return;
    if (i->map == MAP_OLDALE_TOWN)
        h->connections = &sOldaleIsland;
    else if (i->map == MAP_FALLARBOR_TOWN || i->map == MAP_DEWFORD_TOWN)
        h->connections = &sNoConnections;
}

// Past an island's edge, where no real map joins it: black.
// (General's metatile 0x1E3, drawn as nothing on the black backdrop: src/rogue_gen_run0.h.)
static const u16 sBlackBorder[4] = { 0x1E3, 0x1E3, 0x1E3, 0x1E3 };

const u16 *Rogue_Run0Border(void)
{
    return IslandHere() ? sBlackBorder : NULL;
}

// Emerald's story on the islands is over before the player comes: Birch is
// rescued, the rival has gone ahead, Mr. Briney has sailed. The
// trucks are gone too, until the ending (act 5) parks Brendan's.
static void Run0IslandState(u32 island)
{
    switch (island)
    {
    case RUN0_ISLAND_DEWFORD:
        // Mr. Briney and his boat are away: no sailing off the island.
        FlagSet(FLAG_HIDE_MR_BRINEY_DEWFORD_TOWN);
        FlagSet(FLAG_HIDE_MR_BRINEY_BOAT_DEWFORD_TOWN);
        // The fisherman's OLD ROD: RUN 0 starts with one (he asks how the
        // fishing is instead of giving a second).
        FlagSet(FLAG_RECEIVED_OLD_ROD);
        break;
    case RUN0_ISLAND_LITTLEROOT:
        VarSet(VAR_LITTLEROOT_INTRO_STATE, 7);
        VarSet(VAR_LITTLEROOT_TOWN_STATE, 4);
        VarSet(VAR_LITTLEROOT_HOUSES_STATE_MAY, 0);
        VarSet(VAR_LITTLEROOT_HOUSES_STATE_BRENDAN, 0);
        VarSet(VAR_LITTLEROOT_RIVAL_STATE, 4);
        VarSet(VAR_DEX_UPGRADE_JOHTO_STARTER_STATE, 0);
        VarSet(VAR_OLDALE_RIVAL_STATE, 0);
        VarSet(VAR_ROUTE101_STATE, 3);
        VarSet(VAR_OLDALE_TOWN_STATE, 1);
        FlagSet(FLAG_RESCUED_BIRCH);
        FlagSet(FLAG_HIDE_LITTLEROOT_TOWN_MOM_OUTSIDE);
        FlagSet(FLAG_HIDE_LITTLEROOT_TOWN_RIVAL);
        FlagSet(FLAG_HIDE_LITTLEROOT_TOWN_BIRCH);
        FlagClear(FLAG_HIDE_LITTLEROOT_TOWN_FAT_MAN);
        FlagSet(FLAG_HIDE_LITTLEROOT_TOWN_MAYS_HOUSE_TRUCK);
        if (RogueGen_Run0Act() >= RUN0_ACT_MAX)
            FlagClear(FLAG_HIDE_LITTLEROOT_TOWN_BRENDANS_HOUSE_TRUCK);
        else
            FlagSet(FLAG_HIDE_LITTLEROOT_TOWN_BRENDANS_HOUSE_TRUCK);
        FlagSet(FLAG_HIDE_ROUTE_101_BIRCH_ZIGZAGOON_BATTLE);
        FlagSet(FLAG_HIDE_ROUTE_101_BIRCH_STARTERS_BAG);
        FlagSet(FLAG_HIDE_ROUTE_101_ZIGZAGOON);
        FlagSet(FLAG_HIDE_ROUTE_101_BIRCH);
        FlagClear(FLAG_HIDE_ROUTE_101_BOY);
        FlagSet(FLAG_HIDE_OLDALE_TOWN_RIVAL);
        break;
    }
}

// The map's people were just copied into the save block (overworld.c
// LoadObjEventTemplatesFromHeader), before its scripts run: on an island,
// item balls and trainers stand far off the map, so they never appear.
void Rogue_Run0OnObjectsLoaded(void)
{
    const struct Run0IslandMap *island = IslandHere();
    u32 i;
    if (island == NULL)
        return;
    Run0IslandState(island->island);
    // The story's people on the island (src/rogue_run0_story.c).
    Rogue_Run0StoryObjects();
    for (i = 0; i < gMapHeader.events->objectEventCount && i < OBJECT_EVENT_TEMPLATES_COUNT; i++)
    {
        struct ObjectEventTemplate *t = &gSaveBlock1Ptr->objectEventTemplates[i];
        if (t->kind != OBJ_KIND_NORMAL)
            continue;
        if (t->trainerType != TRAINER_TYPE_NONE || t->graphicsId == OBJ_EVENT_GFX_ITEM_BALL
         || t->script == Common_EventScript_FindItem)
        {
            t->x = -1000;
            t->y = -1000;
        }
    }
}

// Hidden items on an island are not there (field_control_avatar.c).
bool32 Rogue_Run0HiddenItem(void)
{
    return IslandHere() != NULL;
}

// ---------------------------------------------------------------------------
// Music and colors of generated maps

// A map the Director made: a place or a dungeon floor of the region (not
// the rooms inside, which are Emerald's, nor the real maps).
static bool32 GeneratedHere(void)
{
    // ENDLESS (after RUN 0's MORE) gets stranger: from its second region
    // the music bends too, from its third the colors (Rogue_Run0TintPalette).
    return (Rogue_Run0Active() || Rogue_Run0EndlessDepth() >= 2) && gSaveBlock1Ptr->location.mapGroup == ROGUE_GROUP
        && (gSaveBlock1Ptr->location.mapNum < ROGUE_MAX_PLACES
            || (gSaveBlock1Ptr->location.mapNum >= MAP_NUM(MAP_ROGUE_FLOOR_1)
                && gSaveBlock1Ptr->location.mapNum < MAP_NUM(MAP_ROGUE_FLOOR_1) + ROGUE_MAX_FLOORS));
}

#define RUN0_TEMPO 0xEA  // about 91% of the speed
#define RUN0_PITCH (-0x50) // about a third of a semitone down

// Each overworld frame: generated maps play their track a little slow and
// low, as the Berry Blender bends Emerald's music; real maps play it as it
// is. A new song starts at full speed, so this puts it right again.
void Rogue_Run0Field(void)
{
    struct MusicPlayerInfo *mp = &gMPlayInfo_BGM;
    bool32 low;
    u16 tempo;
    s16 pitch;
    if ((!Rogue_Run0Active() && Rogue_Run0EndlessDepth() < 2) || mp->songHeader == NULL || mp->tracks == NULL || mp->trackCount == 0)
        return;
    low = GeneratedHere();
    tempo = low ? RUN0_TEMPO : 0x100;
    pitch = low ? RUN0_PITCH : 0;
    if (mp->tempoU != tempo)
        m4aMPlayTempoControl(mp, tempo);
    if (mp->tracks[0].keyShiftX != (s8)(pitch >> 8) || mp->tracks[0].pitX != (u8)pitch)
        m4aMPlayPitchControl(mp, TRACKS_ALL, pitch);
}

// Before any song starts on the music player (sound.c PlayBGM): no bend is
// left over from the map (battles, fanfares and every normal region play
// their music as it is).
void Rogue_ResetMusicPitch(void)
{
    struct MusicPlayerInfo *mp = &gMPlayInfo_BGM;
    u32 i;
    if (mp->tracks == NULL)
        return;
    for (i = 0; i < mp->trackCount; i++)
    {
        if (mp->tracks[i].keyShiftX != 0 || mp->tracks[i].pitX != 0)
        {
            mp->tracks[i].keyShiftX = 0;
            mp->tracks[i].pitX = 0;
            mp->tracks[i].flags |= MPT_FLG_PITCHG;
        }
    }
}

// A generated map's tileset colors (fieldmap.c LoadTilesetPalette): a third
// of the way to gray, and a step toward the map's own cast (cold, sickly
// green, dusty red, violet), so no two neighbours look quite alike. Real
// maps keep Emerald's colors.
void Rogue_Run0TintPalette(u16 offset, u16 count, bool8 skipFaded)
{
    static const s8 sCasts[4][3] = { { -1, 0, 1 }, { 0, 1, -1 }, { 1, 0, -1 }, { 1, -1, 1 } };
    const s8 *cast;
    u32 i;
    if (!GeneratedHere() || (!Rogue_Run0Active() && Rogue_Run0EndlessDepth() < 3))
        return;
    cast = sCasts[(gSaveBlock1Ptr->location.mapNum * 7 + 3) & 3];
    for (i = offset; i < offset + count && i < PLTT_BUFFER_SIZE; i++)
    {
        u16 c = gPlttBufferUnfaded[i];
        s32 r = c & 0x1F, g = (c >> 5) & 0x1F, b = (c >> 10) & 0x1F, lum;
        if ((i & 15) == 0 || i >= BG_PLTT_SIZE / 2)
            continue;
        lum = (r * 77 + g * 150 + b * 29) >> 8;
        r += (lum - r) / 3 + cast[0];
        g += (lum - g) / 3 + cast[1];
        b += (lum - b) / 3 + cast[2];
        r = r < 0 ? 0 : (r > 31 ? 31 : r);
        g = g < 0 ? 0 : (g > 31 ? 31 : g);
        b = b < 0 ? 0 : (b > 31 ? 31 : b);
        gPlttBufferUnfaded[i] = RGB(r, g, b);
        if (!skipFaded)
            gPlttBufferFaded[i] = gPlttBufferUnfaded[i];
    }
}

// ---------------------------------------------------------------------------
// The story's hooks: what stands there until the story gives it a scene.

// Graphics of the object hooks (ROBJ_RUN0).
u16 Rogue_Run0HookGfx(u32 hook)
{
    switch (hook)
    {
    case RUN0_HOOK_CHAMPION_TERRY:
        return OBJ_EVENT_GFX_ROGUE_CHAMPION; // the custom Champion sprite: Terry's self-insert
    case RUN0_HOOK_GIFT_BOX:
    default:
        return OBJ_EVENT_GFX_BIRCHS_BAG;
    }
}

// The hooks' words and scenes are the story's (Rogue_Run0HookTalk in
// src/rogue_run0_story.c, data/scripts/rogue_run0.inc).

// Script special for an object hook: its hook into VAR_0x8004, from the
// object talked to (its template keeps the hook in trainerRange_berryTreeId).
void Rogue_Run0ThingHook(void)
{
    u32 i;
    gSpecialVar_0x8004 = RUN0_HOOK_COUNT;
    for (i = 0; i < OBJECT_EVENT_TEMPLATES_COUNT; i++)
        if (gSaveBlock1Ptr->objectEventTemplates[i].localId == gSpecialVar_LastTalked)
            gSpecialVar_0x8004 = gSaveBlock1Ptr->objectEventTemplates[i].trainerRange_berryTreeId;
}
STATIC_ASSERT(RUN0_HOOK_PROMPT_0 == 7, Run0PromptNumberInScripts);
