// Townsfolk, their Pokemon, and the insides of Pokemon Centers, Marts,
// houses and gyms.
//
// What people say is built from the seed and the region: the Pokemon on the
// route nearby, the next gym leader, the town up the road, the town's own
// character. Every town shares one Pokemon Center, Mart, house and gym map;
// the layout, the people and the way back out are picked here at runtime for
// the town the player came from (VAR_ROGUE_TOWN) and served through
// Rogue_GetInteriorHeader.

#include "global.h"
#include "rogue.h"
#include "rogue_run0_story.h"
#include "rogue_gen.h"
#include "battle_main.h"
#include "battle_setup.h"
#include "script_pokemon_util.h"
#include "data.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "event_scripts.h"
#include "field_player_avatar.h"
#include "fieldmap.h"
#include "item.h"
#include "overworld.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "random.h"
#include "shop.h"
#include "string_util.h"
#include "constants/event_bg.h"
#include "constants/event_object_movement.h"
#include "constants/event_objects.h"
#include "constants/items.h"
#include "constants/layouts.h"
#include "constants/map_groups.h"
#include "constants/region_map_sections.h"
#include "constants/songs.h"
#include "constants/trainer_types.h"
#include "constants/species.h"

#define ROGUE_GROUP MAP_GROUP(MAP_ROGUE_PLACE_0)

extern const struct MapLayout *const gMapLayouts[];
extern const struct MapHeader *const *const gMapGroups[];

extern const u8 Rogue_EventScript_HouseChat[];
extern const u8 Rogue_EventScript_CenterFolk[];
extern const u8 Rogue_EventScript_RestTraveler[];
extern const u8 Rogue_EventScript_ReadThing[];
extern const u8 Rogue_EventScript_Professor[];
extern const u8 Rogue_EventScript_Run0Birch[];
extern const u8 Rogue_EventScript_HouseGift[];
extern const u8 Rogue_EventScript_MoveRelearner[];
extern const u8 Rogue_EventScript_TownMon[];
extern const u8 Rogue_EventScript_GymGuide[];
extern const u8 Rogue_EventScript_SpareAide[];
extern const u8 Rogue_EventScript_GymStatue[];
extern const u8 *const gRogueLeaderScripts[];
extern const u8 *const gRogueGymTrainerScripts[];
extern const u8 SlateportCity_NameRatersHouse_EventScript_NameRater[];
extern const u8 LilycoveCity_MoveDeletersHouse_EventScript_MoveDeleter[];
extern const u8 Rogue_EventScript_BikeShop[];
extern const u8 Rogue_EventScript_FossilScientist[];
extern const u8 Rogue_EventScript_RestHouse[];

// ---------------------------------------------------------------------------
// Townsfolk

// What people look like in each theme's towns, and at the summit.
static const u16 sTownsfolkGfx[RTHEME_COUNT + 1][8] =
{
    [RTHEME_MEADOW]  = { OBJ_EVENT_GFX_BOY_1, OBJ_EVENT_GFX_GIRL_1, OBJ_EVENT_GFX_LITTLE_BOY, OBJ_EVENT_GFX_OLD_WOMAN,
                         OBJ_EVENT_GFX_MAN_1, OBJ_EVENT_GFX_WOMAN_1, OBJ_EVENT_GFX_FAT_MAN, OBJ_EVENT_GFX_LITTLE_GIRL },
    [RTHEME_VERDANT] = { OBJ_EVENT_GFX_TWIN, OBJ_EVENT_GFX_GIRL_2, OBJ_EVENT_GFX_WOMAN_4, OBJ_EVENT_GFX_MAN_4,
                         OBJ_EVENT_GFX_CYCLING_TRIATHLETE_F, OBJ_EVENT_GFX_OLD_MAN, OBJ_EVENT_GFX_BOY_2, OBJ_EVENT_GFX_POKEFAN_F },
    [RTHEME_HARBOR]  = { OBJ_EVENT_GFX_SAILOR, OBJ_EVENT_GFX_FISHERMAN, OBJ_EVENT_GFX_BEAUTY, OBJ_EVENT_GFX_TUBER_M,
                         OBJ_EVENT_GFX_MAN_2, OBJ_EVENT_GFX_WOMAN_3, OBJ_EVENT_GFX_OLD_MAN, OBJ_EVENT_GFX_BOY_3 },
    [RTHEME_ISLAND]  = { OBJ_EVENT_GFX_TUBER_F, OBJ_EVENT_GFX_OLD_WOMAN, OBJ_EVENT_GFX_GIRL_3, OBJ_EVENT_GFX_MAN_4,
                         OBJ_EVENT_GFX_LITTLE_GIRL, OBJ_EVENT_GFX_SCIENTIST_1, OBJ_EVENT_GFX_WOMAN_5, OBJ_EVENT_GFX_FISHERMAN },
    [RTHEME_STONE]   = { OBJ_EVENT_GFX_SCHOOL_KID_M, OBJ_EVENT_GFX_SCIENTIST_1, OBJ_EVENT_GFX_GENTLEMAN, OBJ_EVENT_GFX_WOMAN_2,
                         OBJ_EVENT_GFX_BOY_2, OBJ_EVENT_GFX_LASS, OBJ_EVENT_GFX_DEVON_EMPLOYEE, OBJ_EVENT_GFX_MAN_3 },
    [RTHEME_COVE]    = { OBJ_EVENT_GFX_ARTIST, OBJ_EVENT_GFX_WOMAN_1, OBJ_EVENT_GFX_SAILOR, OBJ_EVENT_GFX_BEAUTY,
                         OBJ_EVENT_GFX_RICH_BOY, OBJ_EVENT_GFX_OLD_MAN, OBJ_EVENT_GFX_GIRL_2, OBJ_EVENT_GFX_MAN_1 },
    [RTHEME_BEACH]   = { OBJ_EVENT_GFX_TUBER_M, OBJ_EVENT_GFX_GIRL_3, OBJ_EVENT_GFX_FISHERMAN, OBJ_EVENT_GFX_SAILOR,
                         OBJ_EVENT_GFX_TUBER_F, OBJ_EVENT_GFX_OLD_MAN, OBJ_EVENT_GFX_BEAUTY, OBJ_EVENT_GFX_BOY_3 },
    [RTHEME_ASH]     = { OBJ_EVENT_GFX_HIKER, OBJ_EVENT_GFX_OLD_WOMAN, OBJ_EVENT_GFX_CAMPER, OBJ_EVENT_GFX_PICNICKER,
                         OBJ_EVENT_GFX_MAN_4, OBJ_EVENT_GFX_LITTLE_BOY, OBJ_EVENT_GFX_FAT_MAN, OBJ_EVENT_GFX_WOMAN_2 },
    [RTHEME_SPRING]  = { OBJ_EVENT_GFX_HOT_SPRINGS_OLD_WOMAN, OBJ_EVENT_GFX_OLD_MAN, OBJ_EVENT_GFX_WOMAN_4, OBJ_EVENT_GFX_EXPERT_F,
                         OBJ_EVENT_GFX_BLACK_BELT, OBJ_EVENT_GFX_LITTLE_GIRL, OBJ_EVENT_GFX_MAN_2, OBJ_EVENT_GFX_BEAUTY },
    [RTHEME_TREETOP] = { OBJ_EVENT_GFX_MAN_5, OBJ_EVENT_GFX_GIRL_1, OBJ_EVENT_GFX_BUG_CATCHER, OBJ_EVENT_GFX_OLD_WOMAN,
                         OBJ_EVENT_GFX_PICNICKER, OBJ_EVENT_GFX_BOY_1, OBJ_EVENT_GFX_NINJA_BOY, OBJ_EVENT_GFX_WOMAN_5 },
    // v1.1 towns2: Pacifidlog's raft folk and Sootopolis's people.
    [RTHEME_RAFT]    = { OBJ_EVENT_GFX_FISHERMAN, OBJ_EVENT_GFX_OLD_MAN, OBJ_EVENT_GFX_TUBER_M, OBJ_EVENT_GFX_SAILOR,
                         OBJ_EVENT_GFX_WOMAN_3, OBJ_EVENT_GFX_LITTLE_GIRL, OBJ_EVENT_GFX_OLD_WOMAN, OBJ_EVENT_GFX_SWIMMER_M },
    [RTHEME_CRATER]  = { OBJ_EVENT_GFX_GENTLEMAN, OBJ_EVENT_GFX_OLD_WOMAN, OBJ_EVENT_GFX_BOY_2, OBJ_EVENT_GFX_BEAUTY,
                         OBJ_EVENT_GFX_EXPERT_M, OBJ_EVENT_GFX_LASS, OBJ_EVENT_GFX_MAN_3, OBJ_EVENT_GFX_WOMAN_2 },
    [RTHEME_COUNT]   = { OBJ_EVENT_GFX_EXPERT_M, OBJ_EVENT_GFX_EXPERT_F, OBJ_EVENT_GFX_MAN_3, OBJ_EVENT_GFX_WOMAN_5,
                         OBJ_EVENT_GFX_BLACK_BELT, OBJ_EVENT_GFX_HIKER, OBJ_EVENT_GFX_GENTLEMAN, OBJ_EVENT_GFX_BEAUTY },
};

u16 Rogue_TownsfolkGfx(u32 theme, bool32 summit, u32 slot)
{
    if (summit)
        theme = RTHEME_COUNT;
    else if (theme >= RTHEME_COUNT)
        theme = RTHEME_MEADOW;
    return sTownsfolkGfx[theme][slot % 8];
}

// What water a town has in and around it (RogueText_TownWater: the sea,
// fresh water, or none). People and what they say fit it; the generator's
// text knows the same voices (RogueText_FolkVoice: keep sTownsfolkGfx and
// its sFolkVoice in step).
static u32 TownWater(u32 place, const struct RogueMap *m)
{
    return RogueText_TownWater(Rogue_GetRegion(), place, m);
}

// People of the sea (sailors, kids with swim rings) only where there is
// sea, and fishermen only by water. Elsewhere someone from the land.
static const u16 sLandFolkGfx[] =
{
    OBJ_EVENT_GFX_MAN_1, OBJ_EVENT_GFX_WOMAN_2, OBJ_EVENT_GFX_BOY_1, OBJ_EVENT_GFX_GIRL_2,
    OBJ_EVENT_GFX_OLD_MAN, OBJ_EVENT_GFX_WOMAN_1, OBJ_EVENT_GFX_MAN_4, OBJ_EVENT_GFX_LITTLE_GIRL,
};

static u16 FitTownsfolkGfx(u16 gfx, u32 water, u32 slot)
{
    bool32 sea = (gfx == OBJ_EVENT_GFX_SAILOR || gfx == OBJ_EVENT_GFX_TUBER_M || gfx == OBJ_EVENT_GFX_TUBER_F);
    if ((sea && water != RWATER_SEA) || (gfx == OBJ_EVENT_GFX_FISHERMAN && water == RWATER_NONE))
        return sLandFolkGfx[slot % ARRAY_COUNT(sLandFolkGfx)];
    return gfx;
}

u16 Rogue_PlaceTownsfolkGfx(u32 place, const struct RogueMap *m, u32 slot)
{
    const struct RoguePlace *p = &Rogue_GetRegion()->places[place];
    return FitTownsfolkGfx(Rogue_TownsfolkGfx(p->theme, p->type == RTOWN_SUMMIT, slot), TownWater(place, m), slot);
}

// How a town's townsperson looks (the obj-th object of its map): the
// reporter pair in their towns (RogueText_ReporterRole), else by their slot.
u16 Rogue_TownsfolkObjGfx(const struct RogueMap *m, u32 obj)
{
    u32 i, turn = 0, nFolk = 0;
    for (i = 0; i < m->nObjs; i++)
    {
        if (m->objs[i].kind != ROBJ_VILLAGER)
            continue;
        if (i < obj)
            turn++;
        nFolk++;
    }
    switch (RogueText_ReporterRole(Rogue_GetRegion(), m->place, nFolk, turn))
    {
    case RTEXT_REPORTER: return OBJ_EVENT_GFX_REPORTER_F;
    case RTEXT_CAMERA:   return OBJ_EVENT_GFX_CAMERAMAN;
    }
    return Rogue_PlaceTownsfolkGfx(m->place, m, m->objs[obj].slot);
}

// The facts a place's text needs (src/rogue_gen_facts.h keeps them: the
// region's, the cast, the volcano, the nearest town's), worked out as its map
// loads so nobody's line stalls a frame.
void Rogue_WarmText(u32 place)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    struct RogueRegionFacts rf;
    struct RogueCast cast;
    struct RogueTownFacts tf;
    s8 x, y;
    u8 town;
    if (place >= r->nPlaces)
        return;
    RogueGen_RegionFacts(r, &rf);
    RogueGen_Cast(r, &cast);
    RogueGen_FactsVolcano(r, &x, &y);
    town = RogueGen_TownNear(r, place);
    if (town < r->nPlaces && r->places[town].kind == RPLACE_TOWN && r->places[town].type != RTOWN_SUMMIT)
        RogueGen_TownFacts(r, town, &tf);
}

// What townsfolk say lives in the generator (src/rogue_text_towns.h), so
// the host tools see it; these turn it into the game's characters.

// The game's own topics among the townsfolk's: the dungeon rumors.
static bool8 GameTopic(u8 topic, u8 place, u32 rng)
{
    return Rogue_BufferDungeonRumor(place, rng);
}

// What someone says once the special people are done, into STR_VAR_1.
static void BufferTalk(u32 place, u32 order, u32 rng, bool32 onRoute, u8 voice)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    Rogue_TextCtx(&c, &st, place, RSPK_FOLK);
    if (RogueText_TownTalk(&c, order, rng, onRoute, voice, GameTopic, (char *)gStringVar1, sizeof(gStringVar1)) == RTALK_TEXT)
        Rogue_TextToGame(gStringVar1);
}

// A townsperson's tale of a legend whose hint is told in this town.
static bool32 BufferFolkLore(u32 place)
{
    s32 d = RogueGen_LoreAt(Rogue_GetRegion(), LORE_WAY_FOLK, place);
    if (d < 0)
        return FALSE;
    Rogue_BufferLore(gStringVar1, d, LORE_HINT, LORE_WAY_FOLK, 250);
    return TRUE;
}

// Script special: what the townsperson just talked to says, into STR_VAR_1.
void Rogue_BufferTownTalk(void)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    s32 place = Rogue_GetCurrentPlace();
    u32 order = 0, i;
    u32 localId = gSpecialVar_LastTalked;
    u32 nFolk = 0;
    s32 loreOrder;
    bool32 outdoors = (place >= 0 && gSaveBlock1Ptr->location.mapNum == (u32)place);
    const struct RogueMap *m = NULL;
    u8 voice = RVOICE_ANY, reporter = 0;

    if (place < 0)
        place = r->start;
    // RUN 0's second teaser: once, in the first town, a name from the last
    // region slips out.
    if (outdoors && (u32)place == r->start && Rogue_TeaseTownLine(gStringVar1))
        return;
    // RUN 0, acts 1 and 2: one person a town slips, naming an old region.
    if (outdoors && Rogue_Run0TownSlip(place, gStringVar1))
        return;
    if (outdoors)
    {
        m = Rogue_GetPlaceMap(place);
        // Count the townsfolk before this one, so each gets the next topic.
        for (i = 0; i < m->nObjs; i++)
        {
            if (m->objs[i].kind != ROBJ_VILLAGER)
                continue;
            if (i + 1 < localId)
                order++;
            nFolk++;
        }
        if (localId >= 1 && localId <= m->nObjs)
            voice = r->places[place].kind == RPLACE_TOWN ? RogueText_FolkVoice(r, place, m, m->objs[localId - 1].slot)
                  : RVOICE_ANY;
        reporter = RogueText_ReporterRole(r, place, nFolk, order);
    }
    // The reporter pair, in their towns: each meeting goes on from the last
    // one had (FLAG_ROGUE_REPORTER_0...), and a talk again is short.
    if (reporter)
    {
        static const u16 sMetFlags[3] = { FLAG_ROGUE_REPORTER_0, FLAG_ROGUE_REPORTER_1, FLAG_ROGUE_REPORTER_2 };
        struct RogueTextCtx c;
        struct RogueTextState st;
        s32 meeting = RogueText_ReporterMeeting(r, place);
        u8 met = 0;
        for (i = 0; i < 3; i++)
            met |= FlagGet(sMetFlags[i]) << i;
        Rogue_TextCtx(&c, &st, place, RSPK_REPORTER);
        RogueText_Reporter(&c, reporter, met, (char *)gStringVar1, sizeof(gStringVar1));
        Rogue_TextToGame(gStringVar1);
        if (reporter == RTEXT_REPORTER && meeting >= 0)
            FlagSet(sMetFlags[meeting]);
        return;
    }
    // The last of them tells what people say of a legend nearby, where
    // that is told here (src/rogue_gen_lore.h), or the one before where the
    // last has a gift: before the others' news when they tell the first two.
    loreOrder = (s32)nFolk - 1 - (r->places[place].kind == RPLACE_TOWN && RogueGen_HasFolkGift(r, r->places[place].number));
    if (loreOrder < 0)
        loreOrder = (s32)nFolk - 1; // the only one: after the gift
    if (outdoors && loreOrder >= 2 && order == (u32)loreOrder && BufferFolkLore(place))
        return;
    // In a hamlet, the first person tells where a rare POKéMON lives, if
    // the region has one.
    if (outdoors && order == 0 && r->places[place].type == RTOWN_HAMLET && r->places[place].kind == RPLACE_TOWN)
    {
        Rogue_BufferRareFindHint();
        if (gSpecialVar_Result)
            return;
    }
    // In the villains' hideout town, the first people talk about them.
    if (outdoors && Rogue_BufferVillainTownTalk(place, order))
        return;
    // While the wanderer waits by the POKeMON CENTER, someone says so.
    if (outdoors && Rogue_BufferWandererPointer(place, order))
        return;
    // In the town ruins are named after, the second person tells their
    // riddle (the only one, in a town of one).
    if (outdoors && Rogue_BufferRuinsRiddleTalk(place, order == (nFolk >= 2 ? 1u : 0u)))
        return;
    if (outdoors && loreOrder >= 0 && order == (u32)loreOrder && BufferFolkLore(place))
        return;
    if (outdoors && r->places[place].kind != RPLACE_TOWN && localId >= 1 && localId <= m->nObjs)
        voice = RogueText_RouteFolkVoice(m->objs[localId - 1].slot);
    BufferTalk(place, RogueText_TownTalkOrder(r, place, order), RogueText_TownTalkRng(r, place, localId),
               r->places[place].kind != RPLACE_TOWN, voice);
}

// Script special: the Pokemon just talked to. Its species goes to VAR_0x8004
// for the cry and its line to STR_VAR_1.
void Rogue_BufferTownMon(void)
{
    struct ObjectEvent *obj = &gObjectEvents[gSelectedObjectEvent];
    struct RogueTextCtx c;
    struct RogueTextState st;
    char name[POKEMON_NAME_LENGTH + 1];
    u16 species = SPECIES_ZIGZAGOON;
    if (IS_OW_MON_OBJ(obj))
        species = obj->graphicsId & OBJ_EVENT_MON_SPECIES_MASK;
    if (species >= NUM_SPECIES)
        species = SPECIES_ZIGZAGOON;
    gSpecialVar_0x8004 = species;
    Rogue_GameToAscii(name, GetSpeciesName(species), POKEMON_NAME_LENGTH);
    Rogue_TextCtx(&c, &st, -1, RSPK_PET);
    c.name = name;
    if (gSaveBlock1Ptr->location.mapNum == MAP_NUM(MAP_ROGUE_HOUSE))
        c.house = gSaveBlock1Ptr->location.warpId % ROGUE_MAX_HOUSES;
    RogueText_MonLine(&c, gSaveBlock1Ptr->location.mapNum >= ROGUE_MAX_PLACES, gSpecialVar_LastTalked, (char *)gStringVar1, sizeof(gStringVar1));
    Rogue_TextToGame(gStringVar1);
}

// Script special: town name into STR_VAR_1 and its motto into STR_VAR_2
// (src/rogue_text_towns.h: from the town's facts, or its legend's).
void Rogue_BufferTownSign(void)
{
    u32 place = Rogue_GetCurrentTownPlace();
    struct RogueTextCtx c;
    struct RogueTextState st;
    Rogue_CopyPlaceName(gStringVar1, place);
    Rogue_TextCtx(&c, &st, place, RSPK_SIGN);
    RogueText_TownMotto(&c, (char *)gStringVar2, sizeof(gStringVar2));
    Rogue_TextToGame(gStringVar2);
}

// Script special: the first gym's town into STR_VAR_1, for the rival.
void Rogue_BufferRivalInfo(void)
{
    Rogue_CopyPlaceName(gStringVar1, Rogue_GetRegion()->gymTown[0]);
}

// ---------------------------------------------------------------------------
// Interiors

struct InteriorDef
{
    u16 layout;
    u8 doorX, doorY; // where the player arrives; the exit is this tile and the one beside it
    u8 door2X;
    u8 spots[3][2];  // where people stand; the first is the host
};

// Ordinary homes from all over Hoenn (their people: sHomeSpots).
static const struct InteriorDef sHomes[] =
{
    { LAYOUT_HOUSE1,                   3, 8, 4 },
    { LAYOUT_HOUSE2,                   3, 7, 4 },
    { LAYOUT_HOUSE3,                   3, 7, 4 },
    { LAYOUT_HOUSE4,                   3, 8, 4 },
    { LAYOUT_HOUSE_WITH_BED,           4, 7, 3 },
    { LAYOUT_LILYCOVE_CITY_HOUSE2,     2, 7, 3 },
    { LAYOUT_FORTREE_CITY_HOUSE1,      3, 5, 4 },
    { LAYOUT_SOOTOPOLIS_CITY_HOUSE1,   3, 6, 4 },
    { LAYOUT_SOOTOPOLIS_CITY_HOUSE3,   3, 6, 4 },
    { LAYOUT_PACIFIDLOG_TOWN_HOUSE1,   4, 8, 5 },
    { LAYOUT_RUSTBORO_CITY_HOUSE,      5, 8, 6 },
    { LAYOUT_RUSTBORO_CITY_CUTTERS_HOUSE, 5, 8, 6 },
    { LAYOUT_ROUTE104_MR_BRINEYS_HOUSE, 5, 8, 6 },
    { LAYOUT_VERDANTURF_TOWN_WANDAS_HOUSE, 7, 7, 8 },
    { LAYOUT_MOSSDEEP_CITY_STEVENS_HOUSE, 3, 7, 4 },
    { LAYOUT_ROUTE124_DIVING_TREASURE_HUNTERS_HOUSE, 3, 8, 4 },
};

// Where the people of a home can be, as Emerald places them in its own
// homes on these rooms: at the table (facing across it), by the furniture,
// in the room. A home takes three in a row from a seeded start, so homes on
// one room differ. None stands on furniture, in a doorway or on the way out,
// and any three leave every spot and the door in reach.
struct HomeSpot
{
    u8 x, y, movement; // movement 0 ends the list
};

#define HOME_SPOTS 7

static const struct HomeSpot sHomeSpots[][HOME_SPOTS] =
{
    // LAYOUT_HOUSE1: the cushions right of the table, the room.
    { { 4, 4, MOVEMENT_TYPE_FACE_LEFT }, { 7, 4, MOVEMENT_TYPE_LOOK_AROUND }, { 4, 5, MOVEMENT_TYPE_FACE_LEFT },
      { 6, 6, MOVEMENT_TYPE_LOOK_AROUND }, { 1, 4, MOVEMENT_TYPE_FACE_RIGHT }, { 5, 3, MOVEMENT_TYPE_LOOK_AROUND } },
    // LAYOUT_HOUSE2: the stools either side of the table.
    { { 4, 4, MOVEMENT_TYPE_FACE_RIGHT }, { 7, 4, MOVEMENT_TYPE_FACE_LEFT }, { 2, 3, MOVEMENT_TYPE_LOOK_AROUND },
      { 7, 5, MOVEMENT_TYPE_FACE_LEFT }, { 4, 5, MOVEMENT_TYPE_FACE_RIGHT }, { 9, 5, MOVEMENT_TYPE_LOOK_AROUND } },
    // LAYOUT_HOUSE3: the cushions either side of the table.
    { { 3, 3, MOVEMENT_TYPE_FACE_RIGHT }, { 6, 3, MOVEMENT_TYPE_FACE_LEFT }, { 8, 4, MOVEMENT_TYPE_LOOK_AROUND },
      { 6, 4, MOVEMENT_TYPE_FACE_LEFT }, { 3, 4, MOVEMENT_TYPE_FACE_RIGHT }, { 1, 5, MOVEMENT_TYPE_LOOK_AROUND } },
    // LAYOUT_HOUSE4: the cushions by the table, the glass workshop's room.
    { { 6, 5, MOVEMENT_TYPE_FACE_RIGHT }, { 9, 4, MOVEMENT_TYPE_FACE_LEFT }, { 2, 3, MOVEMENT_TYPE_LOOK_AROUND },
      { 9, 5, MOVEMENT_TYPE_FACE_LEFT }, { 5, 3, MOVEMENT_TYPE_LOOK_AROUND }, { 1, 6, MOVEMENT_TYPE_LOOK_AROUND } },
    // LAYOUT_HOUSE_WITH_BED
    { { 7, 5, MOVEMENT_TYPE_FACE_DOWN }, { 3, 4, MOVEMENT_TYPE_FACE_LEFT }, { 6, 6, MOVEMENT_TYPE_LOOK_AROUND },
      { 2, 5, MOVEMENT_TYPE_LOOK_AROUND }, { 9, 3, MOVEMENT_TYPE_FACE_DOWN }, { 5, 3, MOVEMENT_TYPE_LOOK_AROUND } },
    // LAYOUT_LILYCOVE_CITY_HOUSE2
    { { 0, 4, MOVEMENT_TYPE_FACE_RIGHT }, { 3, 4, MOVEMENT_TYPE_FACE_LEFT }, { 5, 3, MOVEMENT_TYPE_LOOK_AROUND },
      { 6, 5, MOVEMENT_TYPE_LOOK_AROUND }, { 1, 5, MOVEMENT_TYPE_LOOK_AROUND }, { 4, 2, MOVEMENT_TYPE_FACE_DOWN } },
    // LAYOUT_FORTREE_CITY_HOUSE1
    { { 2, 3, MOVEMENT_TYPE_FACE_RIGHT }, { 5, 3, MOVEMENT_TYPE_FACE_LEFT }, { 1, 3, MOVEMENT_TYPE_LOOK_AROUND },
      { 7, 4, MOVEMENT_TYPE_LOOK_AROUND }, { 2, 4, MOVEMENT_TYPE_LOOK_AROUND }, { 7, 2, MOVEMENT_TYPE_FACE_DOWN } },
    // LAYOUT_SOOTOPOLIS_CITY_HOUSE1: the chairs either side of the table.
    { { 2, 3, MOVEMENT_TYPE_FACE_RIGHT }, { 5, 3, MOVEMENT_TYPE_FACE_LEFT }, { 6, 5, MOVEMENT_TYPE_LOOK_AROUND },
      { 5, 4, MOVEMENT_TYPE_FACE_LEFT }, { 2, 4, MOVEMENT_TYPE_FACE_RIGHT }, { 1, 5, MOVEMENT_TYPE_LOOK_AROUND } },
    // LAYOUT_SOOTOPOLIS_CITY_HOUSE3
    { { 2, 4, MOVEMENT_TYPE_FACE_DOWN }, { 4, 3, MOVEMENT_TYPE_LOOK_AROUND }, { 6, 4, MOVEMENT_TYPE_LOOK_AROUND },
      { 3, 2, MOVEMENT_TYPE_FACE_DOWN }, { 7, 5, MOVEMENT_TYPE_LOOK_AROUND }, { 5, 5, MOVEMENT_TYPE_LOOK_AROUND } },
    // LAYOUT_PACIFIDLOG_TOWN_HOUSE1: the stools either side of the table.
    { { 3, 4, MOVEMENT_TYPE_FACE_RIGHT }, { 6, 4, MOVEMENT_TYPE_FACE_LEFT }, { 9, 4, MOVEMENT_TYPE_LOOK_AROUND },
      { 6, 5, MOVEMENT_TYPE_FACE_LEFT }, { 3, 5, MOVEMENT_TYPE_FACE_RIGHT }, { 4, 2, MOVEMENT_TYPE_FACE_DOWN },
      { 2, 6, MOVEMENT_TYPE_LOOK_AROUND } },
    // LAYOUT_RUSTBORO_CITY_HOUSE: the chairs either side of the table.
    { { 4, 4, MOVEMENT_TYPE_FACE_RIGHT }, { 7, 4, MOVEMENT_TYPE_FACE_LEFT }, { 2, 3, MOVEMENT_TYPE_LOOK_AROUND },
      { 7, 5, MOVEMENT_TYPE_FACE_LEFT }, { 4, 5, MOVEMENT_TYPE_FACE_RIGHT }, { 10, 6, MOVEMENT_TYPE_LOOK_AROUND },
      { 9, 3, MOVEMENT_TYPE_LOOK_AROUND } },
    // LAYOUT_RUSTBORO_CITY_CUTTERS_HOUSE: the table's chairs, the rug.
    { { 7, 5, MOVEMENT_TYPE_FACE_RIGHT }, { 10, 4, MOVEMENT_TYPE_FACE_LEFT }, { 2, 4, MOVEMENT_TYPE_LOOK_AROUND },
      { 10, 5, MOVEMENT_TYPE_FACE_LEFT }, { 9, 2, MOVEMENT_TYPE_LOOK_AROUND }, { 3, 6, MOVEMENT_TYPE_LOOK_AROUND } },
    // LAYOUT_ROUTE104_MR_BRINEYS_HOUSE
    { { 5, 3, MOVEMENT_TYPE_FACE_DOWN }, { 9, 5, MOVEMENT_TYPE_FACE_LEFT }, { 6, 3, MOVEMENT_TYPE_FACE_DOWN },
      { 6, 5, MOVEMENT_TYPE_FACE_RIGHT }, { 10, 3, MOVEMENT_TYPE_LOOK_AROUND }, { 2, 6, MOVEMENT_TYPE_LOOK_AROUND } },
    // LAYOUT_VERDANTURF_TOWN_WANDAS_HOUSE: the table's chairs, both rooms.
    { { 2, 4, MOVEMENT_TYPE_FACE_RIGHT }, { 5, 5, MOVEMENT_TYPE_FACE_LEFT }, { 14, 5, MOVEMENT_TYPE_LOOK_AROUND },
      { 7, 2, MOVEMENT_TYPE_FACE_DOWN }, { 2, 5, MOVEMENT_TYPE_FACE_RIGHT }, { 9, 4, MOVEMENT_TYPE_LOOK_AROUND },
      { 14, 3, MOVEMENT_TYPE_LOOK_AROUND } },
    // LAYOUT_MOSSDEEP_CITY_STEVENS_HOUSE: the chair at the long table, the cases.
    { { 6, 5, MOVEMENT_TYPE_FACE_UP }, { 9, 6, MOVEMENT_TYPE_FACE_RIGHT }, { 2, 3, MOVEMENT_TYPE_LOOK_AROUND },
      { 8, 2, MOVEMENT_TYPE_LOOK_AROUND }, { 1, 5, MOVEMENT_TYPE_LOOK_AROUND }, { 1, 3, MOVEMENT_TYPE_LOOK_AROUND } },
    // LAYOUT_ROUTE124_DIVING_TREASURE_HUNTERS_HOUSE: the table's chairs.
    { { 5, 4, MOVEMENT_TYPE_FACE_LEFT }, { 2, 4, MOVEMENT_TYPE_FACE_RIGHT }, { 7, 3, MOVEMENT_TYPE_LOOK_AROUND },
      { 2, 5, MOVEMENT_TYPE_FACE_RIGHT }, { 5, 5, MOVEMENT_TYPE_FACE_LEFT }, { 8, 6, MOVEMENT_TYPE_LOOK_AROUND } },
};
STATIC_ASSERT(ARRAY_COUNT(sHomeSpots) == ARRAY_COUNT(sHomes), HomeSpotsForEachHome);

static const struct InteriorDef sSpecialInteriors[] =
{
    [RINT_LAB]        = { LAYOUT_LITTLEROOT_TOWN_PROFESSOR_BIRCHS_LAB, 6, 12, 7, { { 6, 4 }, { 9, 8 }, { 3, 3 } } },
    [RINT_SCHOOL]     = { LAYOUT_RUSTBORO_CITY_POKEMON_SCHOOL, 5, 10, 6, { { 5, 3 }, { 3, 6 }, { 10, 8 } } },
    [RINT_FAN_CLUB]   = { LAYOUT_SLATEPORT_CITY_POKEMON_FAN_CLUB, 6, 10, 7, { { 6, 5 }, { 11, 4 }, { 1, 5 } } },
    [RINT_BIKE_SHOP]  = { LAYOUT_MAUVILLE_CITY_BIKE_SHOP, 3, 8, 4, { { 2, 5 }, { 7, 6 }, { 9, 2 } } },
    [RINT_NAME_RATER] = { LAYOUT_HOUSE_WITH_BED, 4, 7, 3, { { 7, 5 }, { 1, 5 }, { 9, 6 } } },
    [RINT_MOVE_DELETER] = { LAYOUT_HOUSE2, 3, 7, 4, { { 4, 4 }, { 2, 3 }, { 7, 4 } } },
    [RINT_REST]       = { LAYOUT_HOUSE1, 3, 8, 4, { { 7, 4 }, { 6, 6 }, { 4, 4 } } },
};

// RUN 0's library: Route 114's Fossil Maniac's house, its bookshelves, and
// its back doorway open as the far door into the same room (one number on).
static const struct InteriorDef sRun0Library = { LAYOUT_ROUTE114_FOSSIL_MANIACS_HOUSE, 4, 7, 5, { { 3, 2 }, { 7, 5 }, { 8, 3 } } };

// The doors and stairs of reused vanilla rooms that led on to rooms a region
// does not have (the Fossil Maniac's tunnel, the floors upstairs): walled
// over with the wall beside them, stairs floored over with the floor beside
// them, as each room loads (Rogue_SealInterior). Blocks are metatile and
// collision; the height stays the room's own.
struct InteriorSeal
{
    u16 layout;
    u8 x, y;
    u16 block;
};

static const struct InteriorSeal sInteriorSeals[] =
{
    // The Fossil Dig's back doorway, into the tunnel.
    { LAYOUT_ROUTE114_FOSSIL_MANIACS_HOUSE, 3, 0, 0x0615 }, { LAYOUT_ROUTE114_FOSSIL_MANIACS_HOUSE, 4, 0, 0x0615 },
    { LAYOUT_ROUTE114_FOSSIL_MANIACS_HOUSE, 5, 0, 0x0615 }, { LAYOUT_ROUTE114_FOSSIL_MANIACS_HOUSE, 3, 1, 0x061D },
    { LAYOUT_ROUTE114_FOSSIL_MANIACS_HOUSE, 4, 1, 0x061D }, { LAYOUT_ROUTE114_FOSSIL_MANIACS_HOUSE, 5, 1, 0x061D },
    { LAYOUT_ROUTE114_FOSSIL_MANIACS_HOUSE, 3, 2, 0x0221 }, { LAYOUT_ROUTE114_FOSSIL_MANIACS_HOUSE, 4, 2, 0x0221 },
    // The Space Center's door up to 2F.
    { LAYOUT_MOSSDEEP_CITY_SPACE_CENTER_1F, 12, 0, 0x06F8 }, { LAYOUT_MOSSDEEP_CITY_SPACE_CENTER_1F, 13, 0, 0x06F8 },
    { LAYOUT_MOSSDEEP_CITY_SPACE_CENTER_1F, 14, 0, 0x06F8 }, { LAYOUT_MOSSDEEP_CITY_SPACE_CENTER_1F, 12, 1, 0x0700 },
    { LAYOUT_MOSSDEEP_CITY_SPACE_CENTER_1F, 13, 1, 0x0700 }, { LAYOUT_MOSSDEEP_CITY_SPACE_CENTER_1F, 14, 1, 0x0700 },
    { LAYOUT_MOSSDEEP_CITY_SPACE_CENTER_1F, 12, 2, 0x0366 }, { LAYOUT_MOSSDEEP_CITY_SPACE_CENTER_1F, 14, 2, 0x0366 },
    // The Weather Institute's door up to 2F.
    { LAYOUT_ROUTE119_WEATHER_INSTITUTE_1F, 16, 0, 0x065F }, { LAYOUT_ROUTE119_WEATHER_INSTITUTE_1F, 17, 0, 0x065F },
    { LAYOUT_ROUTE119_WEATHER_INSTITUTE_1F, 18, 0, 0x065F }, { LAYOUT_ROUTE119_WEATHER_INSTITUTE_1F, 16, 1, 0x0667 },
    { LAYOUT_ROUTE119_WEATHER_INSTITUTE_1F, 17, 1, 0x0667 }, { LAYOUT_ROUTE119_WEATHER_INSTITUTE_1F, 18, 1, 0x0667 },
    { LAYOUT_ROUTE119_WEATHER_INSTITUTE_1F, 16, 2, 0x027A }, { LAYOUT_ROUTE119_WEATHER_INSTITUTE_1F, 17, 2, 0x027A },
    { LAYOUT_ROUTE119_WEATHER_INSTITUTE_1F, 18, 2, 0x027A },
    // The Oceanic Museum's door up to 2F.
    { LAYOUT_SLATEPORT_CITY_OCEANIC_MUSEUM_1F, 5, 0, 0x0605 }, { LAYOUT_SLATEPORT_CITY_OCEANIC_MUSEUM_1F, 6, 0, 0x0604 },
    { LAYOUT_SLATEPORT_CITY_OCEANIC_MUSEUM_1F, 7, 0, 0x0605 }, { LAYOUT_SLATEPORT_CITY_OCEANIC_MUSEUM_1F, 5, 1, 0x060D },
    { LAYOUT_SLATEPORT_CITY_OCEANIC_MUSEUM_1F, 6, 1, 0x060C }, { LAYOUT_SLATEPORT_CITY_OCEANIC_MUSEUM_1F, 7, 1, 0x060D },
    // The Lilycove Museum's door up to 2F.
    { LAYOUT_LILYCOVE_CITY_LILYCOVE_MUSEUM_1F, 15, 0, 0x060B }, { LAYOUT_LILYCOVE_CITY_LILYCOVE_MUSEUM_1F, 16, 0, 0x060B },
    { LAYOUT_LILYCOVE_CITY_LILYCOVE_MUSEUM_1F, 17, 0, 0x060B }, { LAYOUT_LILYCOVE_CITY_LILYCOVE_MUSEUM_1F, 15, 1, 0x0613 },
    { LAYOUT_LILYCOVE_CITY_LILYCOVE_MUSEUM_1F, 16, 1, 0x0613 }, { LAYOUT_LILYCOVE_CITY_LILYCOVE_MUSEUM_1F, 17, 1, 0x0613 },
    // The POKéMON CENTER's and the League's stairs up to the Cable Club.
    { LAYOUT_POKEMON_CENTER_1F, 0, 5, 0x0220 }, { LAYOUT_POKEMON_CENTER_1F, 0, 6, 0x0220 },
    { LAYOUT_POKEMON_CENTER_1F, 0, 7, 0x0220 }, { LAYOUT_POKEMON_CENTER_1F, 1, 5, 0x0202 },
    { LAYOUT_POKEMON_CENTER_1F, 1, 6, 0x0202 }, { LAYOUT_POKEMON_CENTER_1F, 1, 7, 0x0202 },
    { LAYOUT_EVER_GRANDE_CITY_POKEMON_LEAGUE_1F, 0, 6, 0x0220 }, { LAYOUT_EVER_GRANDE_CITY_POKEMON_LEAGUE_1F, 0, 7, 0x0220 },
    { LAYOUT_EVER_GRANDE_CITY_POKEMON_LEAGUE_1F, 0, 8, 0x0220 }, { LAYOUT_EVER_GRANDE_CITY_POKEMON_LEAGUE_1F, 1, 6, 0x0202 },
    { LAYOUT_EVER_GRANDE_CITY_POKEMON_LEAGUE_1F, 1, 7, 0x0202 }, { LAYOUT_EVER_GRANDE_CITY_POKEMON_LEAGUE_1F, 1, 8, 0x0240 },
};

// Map script special (ON_LOAD, after a battle or a menu too) for the region's
// houses, POKéMON CENTERS and League: seals the room's dead doors and stairs.
void Rogue_SealInterior(void)
{
    u32 i;
    if (gSaveBlock1Ptr->location.mapGroup != ROGUE_GROUP || Rogue_Run0InLibrary())
        return;
    for (i = 0; i < ARRAY_COUNT(sInteriorSeals); i++)
        if (sInteriorSeals[i].layout == gMapHeader.mapLayoutId)
            MapGridSetMetatileIdAt(sInteriorSeals[i].x + MAP_OFFSET, sInteriorSeals[i].y + MAP_OFFSET, sInteriorSeals[i].block);
}

// What the people in a house do.
enum
{
    ROLE_CHAT = RHOME_CHAT,
    ROLE_GIFT = RHOME_GIFT,
    ROLE_RELEARNER = RHOME_RELEARNER,
    ROLE_NAME_RATER = RHOME_NAME_RATER,
    ROLE_LAB,
    ROLE_TEACHER,
    ROLE_STUDENT,
    ROLE_FAN,
    ROLE_BIKES,
    ROLE_PET,
    ROLE_CENTER_FOLK = 0x50, // and 0x51: the visitors in a POKeMON CENTER
    ROLE_REST_TRAVELER = 0x52, // a traveler resting in a route's rest house
};

// A rest house's keeper and traveler: each route's own looks.
static u16 RestKeeperGfx(u32 place)
{
    static const u16 sKeepers[] = { OBJ_EVENT_GFX_WOMAN_2, OBJ_EVENT_GFX_OLD_WOMAN, OBJ_EVENT_GFX_MAN_1, OBJ_EVENT_GFX_WOMAN_5,
                                    OBJ_EVENT_GFX_OLD_MAN, OBJ_EVENT_GFX_FAT_MAN, OBJ_EVENT_GFX_POKEFAN_F, OBJ_EVENT_GFX_GENTLEMAN };
    return sKeepers[RogueRng_Hash(Rogue_GetSeed(), 9760 + place) % ARRAY_COUNT(sKeepers)];
}

static u16 RestTravelerGfx(u32 place)
{
    static const u16 sTravelers[] = { OBJ_EVENT_GFX_HIKER, OBJ_EVENT_GFX_CAMPER, OBJ_EVENT_GFX_PICNICKER, OBJ_EVENT_GFX_MAN_4,
                                      OBJ_EVENT_GFX_CYCLING_TRIATHLETE_F, OBJ_EVENT_GFX_RUNNING_TRIATHLETE_M, OBJ_EVENT_GFX_YOUNGSTER,
                                      OBJ_EVENT_GFX_LASS };
    return sTravelers[RogueRng_Hash(Rogue_GetSeed(), 9770 + place) % ARRAY_COUNT(sTravelers)];
}
// The roles with lines of their own, as the generator numbers them.
STATIC_ASSERT((int)ROLE_LAB == (int)RTEXT_ROLE_LAB && (int)ROLE_TEACHER == (int)RTEXT_ROLE_TEACHER
           && (int)ROLE_STUDENT == (int)RTEXT_ROLE_STUDENT && (int)ROLE_FAN == (int)RTEXT_ROLE_FAN, HouseRolesMatchText);

// Pets that live in houses.
static const u16 sPets[] =
{
    SPECIES_SKITTY, SPECIES_ZIGZAGOON, SPECIES_MEOWTH, SPECIES_PURRLOIN, SPECIES_LILLIPUP,
    SPECIES_POOCHYENA, SPECIES_EEVEE, SPECIES_PIKACHU, SPECIES_AZURILL, SPECIES_WHISMUR,
    SPECIES_GLAMEOW, SPECIES_BUNEARY,
};

#define MAX_INTERIOR_OBJS 6
#define MAX_INTERIOR_WARPS 16
#define MAX_INTERIOR_BGS 2

static EWRAM_DATA struct MapHeader sInteriorHeader = {0};
static EWRAM_DATA struct MapEvents sInteriorEvents = {0};
static EWRAM_DATA struct ObjectEventTemplate sInteriorObjects[MAX_INTERIOR_OBJS] = {0};
static EWRAM_DATA struct WarpEvent sInteriorWarps[MAX_INTERIOR_WARPS] = {0};
static EWRAM_DATA struct BgEvent sInteriorBgs[MAX_INTERIOR_BGS] = {0};

static u32 HouseRng(u32 town, u32 house, u32 salt)
{
    return RogueRng_Hash(Rogue_GetSeed(), 9700 + town * 64 + house * 8 + salt);
}

// What the people of an ordinary home do (see RogueGen_HomeRole).
static u32 HomeRole(u32 place, u32 house)
{
    const struct RogueMap *m = Rogue_GetPlaceMap(place);
    return RogueGen_HomeRole(Rogue_GetRegion(), place, m->interiors, m->nHouses, house < m->nHouses ? house : m->nHouses);
}

static void AddInteriorObject(u16 gfx, const u8 *spot, u32 role, const u8 *script, u8 movement)
{
    struct ObjectEventTemplate *t = &sInteriorObjects[sInteriorEvents.objectEventCount];
    memset(t, 0, sizeof(*t));
    t->localId = sInteriorEvents.objectEventCount + 1;
    t->graphicsId = gfx;
    t->kind = OBJ_KIND_NORMAL;
    t->x = spot[0];
    t->y = spot[1];
    t->elevation = 3;
    t->movementType = movement;
    t->trainerRange_berryTreeId = role; // read back by the house scripts
    t->script = script;
    sInteriorEvents.objectEventCount++;
}

// Warps back to the door outside: one per possible arrival index, all on the
// door, then the other door tile.
static void AddInteriorWarps(u8 doorX, u8 doorY, u8 door2X, u16 destMap, u8 destWarp, u32 arriveIndex)
{
    u32 i;
    for (i = 0; i <= arriveIndex && i + 1 < MAX_INTERIOR_WARPS; i++)
    {
        struct WarpEvent *w = &sInteriorWarps[i];
        w->x = doorX;
        w->y = doorY;
        w->elevation = 0;
        w->warpId = destWarp;
        w->mapNum = destMap;
        w->mapGroup = ROGUE_GROUP;
    }
    sInteriorWarps[i] = sInteriorWarps[0];
    sInteriorWarps[i].x = door2X;
    sInteriorEvents.warpCount = i + 1;
    sInteriorEvents.warps = sInteriorWarps;
}

static void BeginInterior(void)
{
    sInteriorEvents.objectEventCount = 0;
    sInteriorEvents.objectEvents = sInteriorObjects;
    sInteriorEvents.coordEventCount = 0;
    sInteriorEvents.bgEventCount = 0;
    sInteriorEvents.bgEvents = sInteriorBgs;
}

static const struct MapHeader *FinishInterior(u32 mapNum, u32 townPlace, u16 layout)
{
    sInteriorHeader = *gMapGroups[ROGUE_GROUP][mapNum];
    if (layout)
    {
        sInteriorHeader.mapLayout = gMapLayouts[layout - 1];
        sInteriorHeader.mapLayoutId = layout;
    }
    sInteriorHeader.events = &sInteriorEvents;
    sInteriorHeader.regionMapSectionId = MAPSEC_ROGUE_PLACE_0 + townPlace;
    return &sInteriorHeader;
}

// The home whose old resident tells a legend's tale (LORE_WAY_ELDER): the
// town's first ordinary home with people who only chat, or -1.
static s32 ElderHouse(u32 place)
{
    const struct RogueMap *m = Rogue_GetPlaceMap(place);
    u32 i;
    if (RogueGen_LoreAt(Rogue_GetRegion(), LORE_WAY_ELDER, place) < 0)
        return -1;
    for (i = 0; i < m->nHouses; i++)
        if (m->interiors[i] == RINT_HOME && HomeRole(place, i) == ROLE_CHAT)
            return i;
    return -1;
}

// What the old one says, into STR_VAR_1: the tale once the player has the
// badges its way opens with, else to come back then.
static void BufferElder(u32 place)
{
    s32 d = RogueGen_LoreAt(Rogue_GetRegion(), LORE_WAY_ELDER, place);
    if (d < 0)
        BufferTalk(place, 0, 0, FALSE, RVOICE_OLD);
    else if (Rogue_BadgeCount() >= RogueGen_LegendGateBadges(Rogue_GetRegion(), d))
        Rogue_BufferLore(gStringVar1, d, LORE_STORY, LORE_WAY_ELDER, 250);
    else
        Rogue_BufferLore(gStringVar1, d, LORE_LATER, LORE_WAYS, 250);
}

// Script special: the old one resting in the POKéMON CENTER, where the
// town has no home for them.
void Rogue_BufferLoreElder(void)
{
    BufferElder(Rogue_GetCurrentTownPlace());
}

static const struct InteriorDef *SigInterior(u32 place);
static void BuildSigPeople(u32 place, const struct InteriorDef *def);
static const struct InteriorDef *FeatInterior(u32 place);
static void BuildFeatPeople(u32 place, const struct InteriorDef *def);

static const struct MapHeader *BuildHouse(u32 place, u32 house)
{
    const struct RoguePlace *p = &Rogue_GetRegion()->places[place];
    const struct RogueMap *m = Rogue_GetPlaceMap(place);
    u32 kind = house < m->nHouses ? m->interiors[house] : RINT_HOME;
    const struct InteriorDef *def;
    u32 rng = HouseRng(place, house, 0);

    // Homes step through the list from a seeded start, so a region rarely
    // shows the same room twice.
    if (kind == RINT_HOME)
        def = &sHomes[(RogueRng_Hash(Rogue_GetSeed(), 9690) + p->number * ROGUE_MAX_HOUSES + house) % ARRAY_COUNT(sHomes)];
    else if (kind == RINT_SIGNATURE)
        def = SigInterior(place);
    else if (kind == RINT_FEATURE)
        def = FeatInterior(place);
    else
        def = &sSpecialInteriors[kind];

    if (Rogue_Run0Active() && RogueGen_Run0HouseHook(Rogue_GetRegion(), m, house) == RUN0_HOOK_LIBRARY)
        def = &sRun0Library;

    BeginInterior();
    AddInteriorWarps(def->doorX, def->doorY, def->door2X, place, ROGUE_HOUSE_WARP(house), house);
    if (def == &sRun0Library && sInteriorEvents.warpCount < MAX_INTERIOR_WARPS)
    {
        // Its far door: into this room again, by its front door.
        struct WarpEvent *w = &sInteriorWarps[sInteriorEvents.warpCount++];
        w->x = RUN0_LIBRARY_DOOR_X;
        w->y = RUN0_LIBRARY_DOOR_Y;
        w->elevation = 0;
        w->warpId = house;
        w->mapNum = MAP_NUM(MAP_ROGUE_HOUSE);
        w->mapGroup = ROGUE_GROUP;
    }

    // RUN 0: one of the story's houses (include/rogue_run0_region.h, the hook from
    // RogueGen_Run0HouseHook: the library, Terry's house, a Believers' home)
    // is an empty room; the story adds its people here.
    if (Rogue_Run0Active() && RogueGen_Run0HouseHook(Rogue_GetRegion(), m, house) >= 0
     && RogueGen_Run0HouseHook(Rogue_GetRegion(), m, house) < RUN0_HOUSE_ISLAND)
        kind = RINT_COUNT;
    switch (kind)
    {
    case RINT_COUNT:
    {
        // The story's people (src/rogue_run0_story.c): the Believers, TERRY.
        // A home's room: they stand where a home's people would.
        struct Run0Cast cast[3];
        u32 i, n = Rogue_Run0HouseCast(place, house, cast);
        for (i = 0; i < n && i < 3; i++)
        {
            u8 spot[2];
            if (def >= sHomes && def < sHomes + ARRAY_COUNT(sHomes))
            {
                const struct HomeSpot *hs = &sHomeSpots[def - sHomes][i];
                spot[0] = hs->x;
                spot[1] = hs->y;
            }
            else
            {
                spot[0] = def->spots[i][0];
                spot[1] = def->spots[i][1];
            }
            AddInteriorObject(cast[i].gfx, spot, ROLE_CHAT, cast[i].script, cast[i].movement);
        }
        break;
    }
    case RINT_SIGNATURE:
        BuildSigPeople(place, def);
        break;
    case RINT_FEATURE:
        BuildFeatPeople(place, def);
        break;
    case RINT_LAB:
        // RUN 0: the aide is gone, and Birch faces the wall (his scenes are
        // the story's, Rogue_EventScript_Run0Birch).
        if (Rogue_IsRun0())
        {
            AddInteriorObject(OBJ_EVENT_GFX_PROF_BIRCH, def->spots[0], ROLE_CHAT, Rogue_EventScript_Run0Birch, MOVEMENT_TYPE_FACE_UP);
            break;
        }
        // Prof. Birch where he stands in Emerald's lab, and his aide. The
        // run starts here, in front of him (WarpToStart in rogue.c).
        AddInteriorObject(OBJ_EVENT_GFX_PROF_BIRCH, def->spots[0], ROLE_CHAT, Rogue_EventScript_Professor, MOVEMENT_TYPE_FACE_DOWN);
        // The aide wanders a block either way of 9,8, as in Emerald: his
        // square (8-10, 7-9) stays clear of the way from the door (6-7, 12)
        // up to Birch.
        AddInteriorObject(OBJ_EVENT_GFX_SCIENTIST_1, def->spots[1], ROLE_LAB, Rogue_EventScript_HouseChat, MOVEMENT_TYPE_WANDER_AROUND);
        sInteriorObjects[sInteriorEvents.objectEventCount - 1].movementRangeX = 1;
        sInteriorObjects[sInteriorEvents.objectEventCount - 1].movementRangeY = 1;
        break;
    case RINT_SCHOOL:
        AddInteriorObject(OBJ_EVENT_GFX_MAN_2, def->spots[0], ROLE_TEACHER, Rogue_EventScript_HouseChat, MOVEMENT_TYPE_FACE_DOWN);
        AddInteriorObject(OBJ_EVENT_GFX_SCHOOL_KID_M, def->spots[1], ROLE_STUDENT, Rogue_EventScript_HouseChat, MOVEMENT_TYPE_FACE_UP);
        AddInteriorObject(OBJ_EVENT_GFX_LASS, def->spots[2], ROLE_STUDENT, Rogue_EventScript_HouseChat, MOVEMENT_TYPE_FACE_UP);
        break;
    case RINT_FAN_CLUB:
        AddInteriorObject(OBJ_EVENT_GFX_GENTLEMAN, def->spots[0], ROLE_FAN, Rogue_EventScript_HouseChat, MOVEMENT_TYPE_FACE_DOWN);
        AddInteriorObject(OBJ_EVENT_GFX_POKEFAN_F, def->spots[1], ROLE_FAN, Rogue_EventScript_HouseChat, MOVEMENT_TYPE_LOOK_AROUND);
        AddInteriorObject(OBJ_EVENT_MON + sPets[rng % ARRAY_COUNT(sPets)], def->spots[2], ROLE_PET, Rogue_EventScript_TownMon, MOVEMENT_TYPE_WANDER_AROUND);
        break;
    case RINT_BIKE_SHOP:
        // The owner gives the player a bike, as Rydel does in Mauville.
        AddInteriorObject(OBJ_EVENT_GFX_MAN_1, def->spots[0], ROLE_BIKES, Rogue_EventScript_BikeShop, MOVEMENT_TYPE_FACE_DOWN);
        AddInteriorObject(OBJ_EVENT_GFX_CYCLING_TRIATHLETE_F, def->spots[1], ROLE_CHAT, Rogue_EventScript_HouseChat, MOVEMENT_TYPE_LOOK_AROUND);
        break;
    case RINT_NAME_RATER:
        AddInteriorObject(OBJ_EVENT_GFX_POKEFAN_M, def->spots[0], ROLE_NAME_RATER, SlateportCity_NameRatersHouse_EventScript_NameRater, MOVEMENT_TYPE_FACE_DOWN);
        break;
    case RINT_REST:
        // Someone who lets travellers rest (each route's own), a traveler
        // resting, and a pet.
        AddInteriorObject(RestKeeperGfx(place), def->spots[0], ROLE_CHAT, Rogue_EventScript_RestHouse, MOVEMENT_TYPE_FACE_DOWN);
        AddInteriorObject(OBJ_EVENT_MON + sPets[(rng >> 24) % ARRAY_COUNT(sPets)], def->spots[1], ROLE_PET,
                          Rogue_EventScript_TownMon, MOVEMENT_TYPE_WANDER_AROUND);
        AddInteriorObject(RestTravelerGfx(place), def->spots[2], ROLE_REST_TRAVELER, Rogue_EventScript_RestTraveler,
                          MOVEMENT_TYPE_LOOK_AROUND);
        break;
    case RINT_MOVE_DELETER:
        AddInteriorObject(OBJ_EVENT_GFX_OLD_MAN, def->spots[0], ROLE_CHAT, LilycoveCity_MoveDeletersHouse_EventScript_MoveDeleter, MOVEMENT_TYPE_FACE_DOWN);
        if ((rng >> 12) % 100 < 60)
            AddInteriorObject(OBJ_EVENT_MON + sPets[(rng >> 24) % ARRAY_COUNT(sPets)], def->spots[1], ROLE_PET,
                              Rogue_EventScript_TownMon, MOVEMENT_TYPE_WANDER_AROUND);
        break;
    default:
    {
        u32 role = HomeRole(place, house);
        const u8 *script = Rogue_EventScript_HouseChat;
        u16 gfx = Rogue_PlaceTownsfolkGfx(place, Rogue_GetPlaceMap(place), rng >> 8);
        const struct HomeSpot *spots = sHomeSpots[def - sHomes];
        u8 at[3][2];
        u32 n = 0, i, first;
        while (n < HOME_SPOTS && spots[n].movement != 0)
            n++;
        first = (rng >> 4) % n;
        for (i = 0; i < 3; i++)
        {
            at[i][0] = spots[(first + i) % n].x;
            at[i][1] = spots[(first + i) % n].y;
        }
        if (role == ROLE_GIFT)
            script = Rogue_EventScript_HouseGift;
        else if (role == ROLE_RELEARNER)
            script = Rogue_EventScript_MoveRelearner, gfx = OBJ_EVENT_GFX_OLD_MAN;
        else if (role == ROLE_NAME_RATER)
            script = SlateportCity_NameRatersHouse_EventScript_NameRater, gfx = OBJ_EVENT_GFX_POKEFAN_M;
        else if ((s32)house == ElderHouse(place))
            gfx = (rng >> 3) & 1 ? OBJ_EVENT_GFX_OLD_WOMAN : OBJ_EVENT_GFX_OLD_MAN; // who knows the old tale
        else if ((s32)house == RogueText_CastElderHouse(Rogue_GetRegion(), place, Rogue_GetPlaceMap(place)))
        {
            // The region's lore elder, GRANNY or GRAMPS (RogueCast.elder).
            struct RogueCast cast;
            RogueGen_Cast(Rogue_GetRegion(), &cast);
            gfx = RogueGen_ElderFemale(&cast) ? OBJ_EVENT_GFX_OLD_WOMAN : OBJ_EVENT_GFX_OLD_MAN;
        }
        AddInteriorObject(gfx, at[0], role, script, spots[first].movement);
        // A scientist visits the gift house of the fourth gym town and
        // revives fossils, as Devon does in Rustboro.
        if (role == ROLE_GIFT && Rogue_IsFossilTown(place))
            AddInteriorObject(OBJ_EVENT_GFX_SCIENTIST_1, at[1], ROLE_CHAT, Rogue_EventScript_FossilScientist, spots[(first + 1) % n].movement);
        else if ((rng >> 12) % 100 < 75)
            AddInteriorObject(Rogue_PlaceTownsfolkGfx(place, Rogue_GetPlaceMap(place), (rng >> 8) + 3), at[1], ROLE_CHAT,
                              Rogue_EventScript_HouseChat, spots[(first + 1) % n].movement);
        if ((rng >> 20) % 100 < 45)
            AddInteriorObject(OBJ_EVENT_MON + sPets[(rng >> 24) % ARRAY_COUNT(sPets)], at[2], ROLE_PET,
                              Rogue_EventScript_TownMon, MOVEMENT_TYPE_WANDER_AROUND);
        break;
    }
    }
    FinishInterior(MAP_NUM(MAP_ROGUE_HOUSE), place, def->layout);
    // Houses play their town's music, the lab its own, as in Emerald.
    sInteriorHeader.music = Overworld_GetMapHeaderByGroupAndId(ROGUE_GROUP, place)->music;
    if (kind == RINT_LAB)
        sInteriorHeader.music = MUS_BIRCH_LAB;
    return &sInteriorHeader;
}

// TRUE if a POKeMON of the party is hurt (HP or status): the rest house's
// keeper says so only then.
static bool32 PartyHurt(void)
{
    u32 i;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        u16 species = GetMonData(&gPlayerParty[i], MON_DATA_SPECIES);
        if (species == SPECIES_NONE || GetMonData(&gPlayerParty[i], MON_DATA_IS_EGG))
            continue;
        if (GetMonData(&gPlayerParty[i], MON_DATA_HP) < GetMonData(&gPlayerParty[i], MON_DATA_MAX_HP)
         || GetMonData(&gPlayerParty[i], MON_DATA_STATUS) != 0)
            return TRUE;
    }
    return FALSE;
}

static void RestSay(u8 what, u8 *dest, u32 size)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    s32 place = Rogue_GetCurrentPlace();
    Rogue_TextCtx(&c, &st, place, what == RREST_SIGN ? RSPK_SIGN : RSPK_HOME);
    RogueText_RestHouse(&c, what, (char *)dest, size);
    Rogue_TextToGame(dest);
}

// Script special: a rest house keeper's words: the offer into STR_VAR_1,
// what they say once the team has rested into STR_VAR_2, turned down into
// STR_VAR_3.
void Rogue_BufferRestHouse(void)
{
    RestSay(PartyHurt() ? RREST_OFFER_HURT : RREST_OFFER_FINE, gStringVar1, sizeof(gStringVar1));
    RestSay(RREST_DONE, gStringVar2, sizeof(gStringVar2));
    RestSay(RREST_DECLINED, gStringVar3, sizeof(gStringVar3));
}

// Script specials: the rest house's sign, and its traveler, into STR_VAR_1.
void Rogue_BufferRestSign(void)
{
    RestSay(RREST_SIGN, gStringVar1, sizeof(gStringVar1));
}

void Rogue_BufferRestTraveler(void)
{
    RestSay(RREST_TRAVELER, gStringVar1, sizeof(gStringVar1));
}

// Script special: what a visitor in a POKeMON CENTER says, into STR_VAR_1.
void Rogue_BufferCenterTalk(void)
{
    u32 place = Rogue_GetCurrentTownPlace(), localId = gSpecialVar_LastTalked, k = 0;
    const struct RogueMap *m = Rogue_GetPlaceMap(place);
    struct RogueTextCtx c;
    struct RogueTextState st;
    if (localId >= 1 && localId <= sInteriorEvents.objectEventCount)
        k = (sInteriorObjects[localId - 1].trainerRange_berryTreeId - ROLE_CENTER_FOLK) & 1;
    Rogue_TextCtx(&c, &st, place, RSPK_FOLK);
    RogueText_CenterFolk(&c, m, k, (char *)gStringVar1, sizeof(gStringVar1));
    Rogue_TextToGame(gStringVar1);
}

// The glass cabinets of the home rooms (their lower halves, which the
// player faces): players take them for TVs, so they say what is inside.
static bool32 IsHomeCabinet(u32 mt)
{
    return mt == 0x28E || mt == 0x28F || mt == 0x2C6 || mt == 0x2C7;
}

// The script for a cabinet the player faces in a region's home (its words
// in gStringVar4), or NULL. Asked after Rogue_GetThingScript.
const u8 *Rogue_GetHomeThingScript(s16 x, s16 y)
{
    s32 place = Rogue_GetCurrentPlace();
    u32 house = gSaveBlock1Ptr->location.warpId;
    const struct RogueMap *m;
    struct RogueTextCtx c;
    struct RogueTextState st;
    if (gSaveBlock1Ptr->location.mapGroup != ROGUE_GROUP || gSaveBlock1Ptr->location.mapNum != MAP_NUM(MAP_ROGUE_HOUSE)
     || place < 0 || !IsHomeCabinet(MapGridGetMetatileIdAt(x, y)))
        return NULL;
    m = Rogue_GetPlaceMap(place);
    if (house >= m->nHouses || m->interiors[house] != RINT_HOME)
        return NULL;
    Rogue_TextCtx(&c, &st, place, RSPK_SIGN);
    RogueText_Cabinet(&c, house, (char *)gStringVar4, sizeof(gStringVar4));
    Rogue_TextToGame(gStringVar4);
    return Rogue_EventScript_ReadThing;
}

// Script special: what the person just talked to in a house says, into
// STR_VAR_1.
void Rogue_BufferHouseTalk(void)
{
    u32 place = Rogue_GetCurrentPlace(), house = gSaveBlock1Ptr->location.warpId;
    u32 localId = gSpecialVar_LastTalked, role = ROLE_CHAT;
    struct RogueTextCtx c;
    struct RogueTextState st;

    if (localId >= 1 && localId <= sInteriorEvents.objectEventCount)
        role = sInteriorObjects[localId - 1].trainerRange_berryTreeId;
    Rogue_TextCtx(&c, &st, place, RSPK_HOME);
    switch (role)
    {
    case ROLE_LAB:
    {
        // PROF. BIRCH's reading, where a legend is told of in the lab.
        s32 d = RogueGen_LoreAt(Rogue_GetRegion(), LORE_WAY_LAB, place);
        if (d >= 0)
        {
            struct RogueLore lore;
            RogueGen_LegendLore(Rogue_GetRegion(), d, &lore);
            Rogue_BufferLore(gStringVar1, d, lore.story == LORE_WAY_LAB ? LORE_STORY : LORE_HINT, LORE_WAY_LAB, 250);
            break;
        }
        RogueText_HouseRole(&c, RTEXT_ROLE_LAB, house, localId, (char *)gStringVar1, sizeof(gStringVar1));
        Rogue_TextToGame(gStringVar1);
        break;
    }
    case ROLE_TEACHER:
    case ROLE_STUDENT:
    case ROLE_FAN:
        RogueText_HouseRole(&c, role, house, localId, (char *)gStringVar1, sizeof(gStringVar1));
        Rogue_TextToGame(gStringVar1);
        break;
    default:
        // The old one who knows the town's tale of a legend tells it once
        // the player could go and see (LORE_WAY_ELDER).
        if (localId == 1 && (s32)house == ElderHouse(place))
        {
            BufferElder(place);
            break;
        }
        if (RogueText_HomeTalk(&c, Rogue_GetPlaceMap(place), house, localId, GameTopic, (char *)gStringVar1,
                               sizeof(gStringVar1)) == RTALK_TEXT)
            Rogue_TextToGame(gStringVar1);
        break;
    }
}

// Script special for the bookshelf of a POKéMON CENTER: a book of the
// region's legends, where one is told of in this town (LORE_WAY_BOOK), into
// gStringVar4. VAR_RESULT FALSE for the usual shelf.
void Rogue_BufferLoreBook(void)
{
    s32 place = Rogue_GetCurrentPlace(), d = -1;
    gSpecialVar_Result = FALSE;
    if (place < 0 || gSaveBlock1Ptr->location.mapNum != MAP_NUM(MAP_ROGUE_POKEMON_CENTER))
        return;
    d = RogueGen_LoreAt(Rogue_GetRegion(), LORE_WAY_BOOK, place);
    if (d >= 0)
    {
        Rogue_BufferLore(gStringVar4, d, LORE_STORY, LORE_WAY_BOOK, 600);
    }
    else
    {
        // Else the region's own books (src/rogue_text_towns.h).
        struct RogueTextCtx c;
        struct RogueTextState st;
        Rogue_TextCtx(&c, &st, place, RSPK_BOOK);
        RogueText_Book(&c, RBOOK_CENTER, 0, (char *)gStringVar4, sizeof(gStringVar4));
        Rogue_TextToGame(gStringVar4);
    }
    gSpecialVar_Result = TRUE;
}

// Script special for a bookshelf (VAR_0x8004 0) or a picture-book shelf (1)
// in a house: the region's books, into gStringVar4. VAR_RESULT FALSE away
// from the region's houses (the usual shelf).
void Rogue_BufferRegionBook(void)
{
    s32 place = Rogue_GetCurrentPlace();
    u32 house = gSaveBlock1Ptr->location.warpId;
    struct RogueTextCtx c;
    struct RogueTextState st;
    gSpecialVar_Result = FALSE;
    if (place < 0 || gSaveBlock1Ptr->location.mapNum != MAP_NUM(MAP_ROGUE_HOUSE))
        return;
    Rogue_TextCtx(&c, &st, place, RSPK_BOOK);
    // A family's books only in a family's home.
    if (house >= Rogue_GetPlaceMap(place)->nHouses || Rogue_GetPlaceMap(place)->interiors[house] != RINT_HOME)
        house = 0xFF;
    RogueText_Book(&c, gSpecialVar_0x8004 ? RBOOK_PICTURE : RBOOK_HOME, house, (char *)gStringVar4, sizeof(gStringVar4));
    Rogue_TextToGame(gStringVar4);
    gSpecialVar_Result = TRUE;
}

// Script special for a TV in a house: the news of a legend, where the
// region's lore has one (LORE_WAY_TV), as Kanto's TVs tell of the birds.
// Elsewhere (most houses) the region's own shows and news, in place of
// Hoenn's (RogueText_TV).
void Rogue_BufferLoreTV(void)
{
    s32 place = Rogue_GetCurrentPlace();
    u32 house = gSaveBlock1Ptr->location.warpId % ROGUE_MAX_HOUSES;
    struct RogueTextCtx c;
    struct RogueTextState st;
    gSpecialVar_Result = FALSE;
    if (place < 0)
        return;
    if (gSaveBlock1Ptr->location.mapNum != MAP_NUM(MAP_ROGUE_HOUSE))
        house = 0;
    if (RogueText_HouseLoreTV(Rogue_GetRegion(), place, house))
    {
        Rogue_BufferLore(gStringVar4, RogueGen_LoreAt(Rogue_GetRegion(), LORE_WAY_TV, 0), LORE_HINT, LORE_WAY_TV, 600);
    }
    else
    {
        Rogue_TextCtx(&c, &st, place, RSPK_TV);
        RogueText_TV(&c, house, (char *)gStringVar4, sizeof(gStringVar4));
        Rogue_TextToGame(gStringVar4);
    }
    gSpecialVar_Result = TRUE;
}

// Script special: the plate beside a home the player faces, its family's
// name, into STR_VAR_1.
void Rogue_BufferHouseSign(void)
{
    u32 place = Rogue_GetCurrentTownPlace();
    const struct RogueMap *m = Rogue_GetPlaceMap(place);
    struct RogueTextCtx c;
    struct RogueTextState st;
    s16 x, y;
    s32 house;
    GetXYCoordsOneStepInFrontOfPlayer(&x, &y);
    house = RogueText_SignHouse(m, x - MAP_OFFSET, y - MAP_OFFSET);
    Rogue_TextCtx(&c, &st, place, RSPK_SIGN);
    if (house < 0)
    {
        Rogue_CopyPlaceName(gStringVar1, place);
        return;
    }
    // The gift house's plate (RSIGN_GIFT) has a welcome for TRAINERS.
    RogueText_HouseSign(&c, house, RogueGen_HomeRole(Rogue_GetRegion(), place, m->interiors, m->nHouses, house) == RHOME_GIFT
                                    && m->interiors[house] == RINT_HOME,
                        (char *)gStringVar1, sizeof(gStringVar1));
    Rogue_TextToGame(gStringVar1);
}

// The gift house: one gift per town, rolled per run from what suits the
// town's theme (src/rogue_gen_items.h), with a line of its own
// (src/rogue_text_towns.h).
// Script special: sets VAR_0x8000 and VAR_0x8001 to this town's gift, and
// VAR_RESULT to FALSE if it was already given (its line: Rogue_BufferGiftOffer).
void Rogue_PrepareGift(void)
{
    u32 town = Rogue_GetCurrentTown() % ROGUE_MAX_TOWNS;
    gSpecialVar_Result = !FlagGet(FLAG_ROGUE_GIFT_0 + town);
    if (!Rogue_GetTownGift(town, FALSE))
    {
        gSpecialVar_0x8000 = ITEM_SUPER_POTION;
        gSpecialVar_0x8001 = 2;
    }
}

// Script special: the gift house's offer into STR_VAR_1 (a frame after
// Rogue_PrepareGift: the gift's ledger and the words together would take
// more than one).
void Rogue_BufferGiftOffer(void)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    Rogue_TextCtx(&c, &st, Rogue_GetCurrentTownPlace(), RSPK_HOME);
    RogueText_GiftOffer(&c, Rogue_GetPlaceMap(Rogue_GetCurrentTownPlace()), gSaveBlock1Ptr->location.warpId % ROGUE_MAX_HOUSES,
                        gSpecialVar_0x8000, (char *)gStringVar1, sizeof(gStringVar1));
    Rogue_TextToGame(gStringVar1);
}

// Script special: what the gift house says once its gift is given, into
// STR_VAR_1.
void Rogue_BufferGiftAfter(void)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    Rogue_TextCtx(&c, &st, Rogue_GetCurrentTownPlace(), RSPK_HOME);
    RogueText_GiftAfter(&c, Rogue_GetPlaceMap(Rogue_GetCurrentTownPlace()), gSaveBlock1Ptr->location.warpId % ROGUE_MAX_HOUSES,
                        gSpecialVar_0x8000, (char *)gStringVar1, sizeof(gStringVar1));
    Rogue_TextToGame(gStringVar1);
}

void Rogue_MarkGiftGiven(void)
{
    FlagSet(FLAG_ROGUE_GIFT_0 + Rogue_GetCurrentTown() % ROGUE_MAX_TOWNS);
}

// A townsperson in some towns has a small gift, as in Emerald (Rustboro's
// Quick Claw, Route 109's Soft Sand): the last of the town's people.
// Script special: VAR_RESULT TRUE if the townsperson just talked to has a
// gift for the player, in VAR_0x8000 and VAR_0x8001 (their line:
// Rogue_BufferFolkGift).
void Rogue_PrepareFolkGift(void)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    s32 place = Rogue_GetCurrentPlace();
    const struct RogueMap *m;
    u32 town, i, last = 0;

    gSpecialVar_Result = FALSE;
    if (place < 0 || gSaveBlock1Ptr->location.mapNum != (u32)place || r->places[place].kind != RPLACE_TOWN)
        return;
    town = r->places[place].number;
    if (town >= ROGUE_MAX_TOWNS || !RogueGen_HasFolkGift(r, town) || FlagGet(FLAG_ROGUE_FOLK_GIFT_0 + town))
        return;
    m = Rogue_GetPlaceMap(place);
    for (i = 0; i < m->nObjs; i++)
        if (m->objs[i].kind == ROBJ_VILLAGER)
            last = i + 1;
    if (gSpecialVar_LastTalked != last || !Rogue_GetTownGift(town, TRUE))
        return;
    gSpecialVar_Result = TRUE;
}

// Script special: the townsperson's words with their gift, into STR_VAR_1
// (a frame after Rogue_PrepareFolkGift, whose ledger takes most of one).
void Rogue_BufferFolkGift(void)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    u32 place = Rogue_GetCurrentTownPlace(), localId = gSpecialVar_LastTalked;
    const struct RogueMap *m = Rogue_GetPlaceMap(place);
    struct RogueTextCtx c;
    struct RogueTextState st;
    u8 voice = RVOICE_ANY;
    if (localId >= 1 && localId <= m->nObjs)
        voice = RogueText_FolkVoice(r, place, m, m->objs[localId - 1].slot);
    Rogue_TextCtx(&c, &st, place, RSPK_FOLK);
    RogueText_FolkGift(&c, voice, FALSE, gSpecialVar_0x8000, (char *)gStringVar1, sizeof(gStringVar1));
    Rogue_TextToGame(gStringVar1);
}

// Script special: the townsperson's word once their gift is given, into
// STR_VAR_1.
void Rogue_BufferFolkGiftAfter(void)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    u32 place = Rogue_GetCurrentTownPlace(), localId = gSpecialVar_LastTalked;
    const struct RogueMap *m = Rogue_GetPlaceMap(place);
    struct RogueTextCtx c;
    struct RogueTextState st;
    u8 voice = RVOICE_ANY;
    if (localId >= 1 && localId <= m->nObjs)
        voice = RogueText_FolkVoice(r, place, m, m->objs[localId - 1].slot);
    Rogue_TextCtx(&c, &st, place, RSPK_FOLK);
    RogueText_FolkGift(&c, voice, TRUE, gSpecialVar_0x8000, (char *)gStringVar1, sizeof(gStringVar1));
    Rogue_TextToGame(gStringVar1);
}

void Rogue_MarkFolkGiftGiven(void)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    s32 place = Rogue_GetCurrentPlace();
    if (place >= 0 && r->places[place].number < ROGUE_MAX_TOWNS)
        FlagSet(FLAG_ROGUE_FOLK_GIFT_0 + r->places[place].number);
}

// ---------------------------------------------------------------------------
// Pokemon Center and Mart

extern const u8 Rogue_EventScript_MartClerk[];
extern const u8 Rogue_EventScript_SpecialtyClerk[];
extern const u8 Rogue_EventScript_RuinsScholar[];
extern const u8 Rogue_EventScript_RhymeChild[];
extern const u8 Rogue_EventScript_ExplorerThanks[];
extern const u8 Rogue_EventScript_LoreElder[];

// The specialty counter's clerk, behind the counter beside the Mart's own,
// as Lilycove's department store has two at its counter.
static const u16 sShopClerks[] =
{
    [RSHOP_NONE]    = OBJ_EVENT_GFX_MART_EMPLOYEE,
    [RSHOP_BATTLE]  = OBJ_EVENT_GFX_MART_EMPLOYEE,
    [RSHOP_VITAMIN] = OBJ_EVENT_GFX_MAN_3,
    [RSHOP_STONE]   = OBJ_EVENT_GFX_SCIENTIST_1,
    [RSHOP_BALL]    = OBJ_EVENT_GFX_SCIENTIST_1,
    [RSHOP_TM]      = OBJ_EVENT_GFX_WOMAN_2,
    [RSHOP_SEA]     = OBJ_EVENT_GFX_SAILOR,
    [RSHOP_HELD]    = OBJ_EVENT_GFX_GENTLEMAN,
    [RSHOP_HERB]    = OBJ_EVENT_GFX_OLD_WOMAN,
    [RSHOP_TYPE]    = OBJ_EVENT_GFX_MART_EMPLOYEE,
};

static EWRAM_DATA u16 sSpecialtyStock[13] = {0};

// Script special: the specialty counter's stock for the badges won, and
// the clerk's greeting into STR_VAR_1.
void Rogue_PrepareSpecialtyShop(void)
{
    u32 place = Rogue_GetCurrentTownPlace(), shop = RogueGen_SpecialtyShop(Rogue_GetRegion(), place);
    u8 flags = Rogue_IsHardcore() ? ROGUE_ITEMS_HARDCORE : 0;
    RogueGen_SpecialtyStock(Rogue_GetRegion(), place, Rogue_BadgeCount(), sSpecialtyStock, ARRAY_COUNT(sSpecialtyStock), flags);
    // Under the Hardcore rules the battle counter sells berries instead.
    if (shop == RSHOP_BATTLE && flags)
        shop = RSHOP_COUNT;
    {
        struct RogueTextCtx c;
        struct RogueTextState st;
        Rogue_TextCtx(&c, &st, place, RSPK_CLERK);
        RogueText_ShopGreeting(&c, shop, (char *)gStringVar1, sizeof(gStringVar1));
        Rogue_TextToGame(gStringVar1);
    }
}

// Script special, then waitstate: opens the counter's shop.
void Rogue_OpenSpecialtyShop(void)
{
    CreatePokemartMenu(sSpecialtyStock);
}

// Where the POKeMON CENTER's child and visitors stand, rolled per town so
// Centers differ between towns and runs. The spots keep clear of the
// counter, the walk from the door to it, the door itself and the others'
// fixed spots (scholar 10,6, elder 5,4, aide 3,4, explorer 11,5), and the
// two lists share none, so no two people meet.
static const u8 sCenterChildSpots[][2] = { { 2, 5 }, { 11, 4 }, { 1, 5 }, { 3, 6 } };
static const u8 sCenterFolkSpots[][3] =
{
    { 13, 3, MOVEMENT_TYPE_FACE_LEFT },
    {  4, 7, MOVEMENT_TYPE_FACE_RIGHT },
    { 11, 2, MOVEMENT_TYPE_FACE_DOWN },
    {  9, 6, MOVEMENT_TYPE_FACE_LEFT },
    { 13, 7, MOVEMENT_TYPE_FACE_UP },
    {  0, 7, MOVEMENT_TYPE_FACE_RIGHT },
    { 10, 7, MOVEMENT_TYPE_FACE_LEFT },
    {  2, 8, MOVEMENT_TYPE_FACE_UP },
};

static u32 CenterSpotRoll(u32 place)
{
    return RogueRng_Hash(Rogue_GetSeed(), 9780 + place);
}

static const struct MapHeader *BuildRoomWithWarps(u32 mapNum, u32 place, u8 warpKind)
{
    const struct MapHeader *rom = gMapGroups[ROGUE_GROUP][mapNum];
    const struct MapEvents *ev = rom->events;
    u32 i;

    BeginInterior();
    for (i = 0; i < ev->objectEventCount && i < MAX_INTERIOR_OBJS; i++)
        sInteriorObjects[i] = ev->objectEvents[i];
    sInteriorEvents.objectEventCount = i;
    for (i = 0; i < ev->warpCount && i < MAX_INTERIOR_WARPS; i++)
    {
        sInteriorWarps[i] = ev->warps[i];
        sInteriorWarps[i].mapNum = place;
        sInteriorWarps[i].warpId = warpKind;
    }
    sInteriorEvents.warpCount = i;
    sInteriorEvents.warps = sInteriorWarps;
    // A themed town's specialty counter.
    if (warpKind == RWARP_MART && RogueGen_SpecialtyShop(Rogue_GetRegion(), place) != RSHOP_NONE
     && sInteriorEvents.objectEventCount < MAX_INTERIOR_OBJS)
    {
        static const u8 spot[] = { 1, 2 };
        u32 shop = RogueGen_SpecialtyShop(Rogue_GetRegion(), place);
        AddInteriorObject(sShopClerks[shop < ARRAY_COUNT(sShopClerks) ? shop : RSHOP_NONE], spot, 0,
                          Rogue_EventScript_SpecialtyClerk, MOVEMENT_TYPE_FACE_RIGHT);
    }
    // A ruin maniac resting by the PC, who tells the riddle of the ruins
    // nearby, as Center visitors in Emerald tell of Route 111's desert.
    if (warpKind == RWARP_PC && Rogue_RuinsScholarHere(place) && sInteriorEvents.objectEventCount < MAX_INTERIOR_OBJS)
    {
        static const u8 spot[] = { 10, 6 };
        AddInteriorObject(OBJ_EVENT_GFX_HIKER, spot, 0, Rogue_EventScript_RuinsScholar, MOVEMENT_TYPE_FACE_LEFT);
    }
    // An old one who knows a legend's tale, where the town has no home of
    // its own for them (LORE_WAY_ELDER), as Celestic's elder tells Sinnoh's.
    if (warpKind == RWARP_PC && RogueGen_LoreAt(Rogue_GetRegion(), LORE_WAY_ELDER, place) >= 0 && ElderHouse(place) < 0
     && sInteriorEvents.objectEventCount < MAX_INTERIOR_OBJS)
    {
        static const u8 spot[] = { 5, 4 };
        AddInteriorObject(place & 1 ? OBJ_EVENT_GFX_OLD_WOMAN : OBJ_EVENT_GFX_OLD_MAN, spot, 0, Rogue_EventScript_LoreElder,
                          MOVEMENT_TYPE_LOOK_AROUND);
    }
    // PROF. BIRCH's aide, with a spare starter (SPARE STARTERS), before the
    // people who only talk. The nurse and at most five more fill
    // MAX_INTERIOR_OBJS: the scholar, the elder, the aide, the child and
    // the explorer.
    if (warpKind == RWARP_PC && Rogue_GetSpareGift(place) >= 0 && sInteriorEvents.objectEventCount < MAX_INTERIOR_OBJS)
    {
        static const u8 spot[] = { 3, 4 };
        AddInteriorObject(OBJ_EVENT_GFX_SCIENTIST_1, spot, 0, Rogue_EventScript_SpareAide, MOVEMENT_TYPE_FACE_DOWN);
    }
    // A child who knows the town's skipping rhyme (LORE_WAY_RHYME), and the
    // explorer once led out of the dungeon named after the town, resting.
    if (warpKind == RWARP_PC && Rogue_RhymeChildHere(place) && sInteriorEvents.objectEventCount < MAX_INTERIOR_OBJS)
        AddInteriorObject(place & 1 ? OBJ_EVENT_GFX_LITTLE_GIRL : OBJ_EVENT_GFX_LITTLE_BOY,
                          sCenterChildSpots[CenterSpotRoll(place) % ARRAY_COUNT(sCenterChildSpots)], 0,
                          Rogue_EventScript_RhymeChild, MOVEMENT_TYPE_LOOK_AROUND);
    if (warpKind == RWARP_PC && Rogue_ExplorerThanksHere(place) && sInteriorEvents.objectEventCount < MAX_INTERIOR_OBJS)
    {
        static const u8 spot[] = { 11, 5 };
        AddInteriorObject(RogueDungeon_ExplorerGfx(0), spot, 0, Rogue_EventScript_ExplorerThanks, MOVEMENT_TYPE_FACE_LEFT);
    }
    // A visitor or two resting on the benches, as Hoenn's Centers have, in
    // the room the game's own people leave (RogueText_CenterFolk).
    // Two different spots of sCenterFolkSpots, rolled per town.
    if (warpKind == RWARP_PC)
    {
        const struct RogueMap *m = Rogue_GetPlaceMap(place);
        u32 h = CenterSpotRoll(place), n = ARRAY_COUNT(sCenterFolkSpots);
        u32 at[2];
        at[0] = (h >> 4) % n;
        at[1] = (at[0] + 1 + (h >> 12) % (n - 1)) % n;
        for (i = 0; i < 2 && sInteriorEvents.objectEventCount < MAX_INTERIOR_OBJS; i++)
            AddInteriorObject(Rogue_PlaceTownsfolkGfx(place, m, RogueText_CenterFolkSlot(Rogue_GetRegion(), place, m, i)),
                              sCenterFolkSpots[at[i]], ROLE_CENTER_FOLK + i, Rogue_EventScript_CenterFolk,
                              sCenterFolkSpots[at[i]][2]);
    }
    return FinishInterior(mapNum, place, 0);
}

// ---------------------------------------------------------------------------
// Gyms

#include "data/rogue_gyms.h"

static const u8 sFacingMovement[] =
{
    [RDIR_NORTH] = MOVEMENT_TYPE_FACE_UP,
    [RDIR_EAST]  = MOVEMENT_TYPE_FACE_RIGHT,
    [RDIR_SOUTH] = MOVEMENT_TYPE_FACE_DOWN,
    [RDIR_WEST]  = MOVEMENT_TYPE_FACE_LEFT,
};

static void AddGymObject(u16 gfx, const struct GymSpot *spot, const u8 *script, bool32 trainer)
{
    struct ObjectEventTemplate *t = &sInteriorObjects[sInteriorEvents.objectEventCount];
    memset(t, 0, sizeof(*t));
    t->localId = sInteriorEvents.objectEventCount + 1;
    t->graphicsId = gfx;
    t->kind = OBJ_KIND_NORMAL;
    t->x = spot->x;
    t->y = spot->y;
    t->elevation = spot->elevation;
    t->movementType = sFacingMovement[spot->facing & 3];
    if (trainer)
    {
        t->trainerType = TRAINER_TYPE_NORMAL;
        t->trainerRange_berryTreeId = spot->sight;
    }
    t->script = script;
    sInteriorEvents.objectEventCount++;
}

static const struct MapHeader *BuildGym(u32 place)
{
    u32 gym = Rogue_GetRegion()->places[place].gym % ROGUE_NUM_GYMS;
    u32 type = Rogue_GetGymLeaderType(gym), i, n = Rogue_GetGymTrainerCount(gym);
    const struct GymDesign *d = &sGymDesigns[type < NUMBER_OF_MON_TYPES ? sGymDesignOfType[type] : 0];

    BeginInterior();
    AddGymObject(Rogue_GetGymGfx(gym, 0), &d->leader, gRogueLeaderScripts[gym], FALSE);
    for (i = 0; i < n; i++)
        AddGymObject(Rogue_GetGymGfx(gym, i + 1), &d->trainers[i], gRogueGymTrainerScripts[gym * 3 + i], TRUE);
    AddGymObject(OBJ_EVENT_GFX_MAN_2, &d->guide, Rogue_EventScript_GymGuide, FALSE);

    for (i = 0; i < 2; i++)
    {
        struct WarpEvent *w = &sInteriorWarps[i];
        w->x = d->doorX + i;
        w->y = d->doorY;
        w->elevation = 0;
        w->warpId = RWARP_GYM;
        w->mapNum = place;
        w->mapGroup = ROGUE_GROUP;
    }
    sInteriorEvents.warpCount = 2;
    sInteriorEvents.warps = sInteriorWarps;

    for (i = 0; i < MAX_INTERIOR_BGS; i++)
    {
        struct BgEvent *b = &sInteriorBgs[i];
        memset(b, 0, sizeof(*b));
        b->x = d->statues[i][0];
        b->y = d->statues[i][1];
        b->kind = BG_EVENT_PLAYER_FACING_NORTH;
        b->bgUnion.script = Rogue_EventScript_GymStatue;
    }
    sInteriorEvents.bgEventCount = MAX_INTERIOR_BGS;
    return FinishInterior(MAP_NUM(MAP_ROGUE_GYM), place, d->layout);
}

// The LEAGUE's lobby: its way out leads to the summit's door, not to the
// first place (its map's static warps name ROGUE_PLACE_0, which left the
// player at 65535,65535 in the first town), and it is named after the summit.
static const struct MapHeader *BuildLeague(u32 place)
{
    const struct MapEvents *ev = gMapGroups[ROGUE_GROUP][MAP_NUM(MAP_ROGUE_LEAGUE)]->events;
    u32 i;

    BeginInterior();
    for (i = 0; i < ev->objectEventCount && i < MAX_INTERIOR_OBJS; i++)
        sInteriorObjects[i] = ev->objectEvents[i];
    sInteriorEvents.objectEventCount = i;
    for (i = 0; i < ev->warpCount && i < MAX_INTERIOR_WARPS; i++)
    {
        sInteriorWarps[i] = ev->warps[i];
        if (ev->warps[i].mapGroup == ROGUE_GROUP && ev->warps[i].mapNum == MAP_NUM(MAP_ROGUE_PLACE_0))
        {
            sInteriorWarps[i].mapNum = place;
            sInteriorWarps[i].warpId = RWARP_GYM;
        }
    }
    sInteriorEvents.warpCount = i;
    sInteriorEvents.warps = sInteriorWarps;
    return FinishInterior(MAP_NUM(MAP_ROGUE_LEAGUE), place, 0);
}

// Header for an interior, built only for the map the player is in (or is
// warping into), so other lookups never disturb the loaded events.
const struct MapHeader *Rogue_GetInteriorHeader(u16 mapGroup, u16 mapNum)
{
    u32 place;
    if (mapGroup != ROGUE_GROUP || gSaveBlock1Ptr->location.mapGroup != mapGroup
     || gSaveBlock1Ptr->location.mapNum != mapNum)
        return NULL;
    place = Rogue_GetCurrentPlace();
    switch (mapNum)
    {
    case MAP_NUM(MAP_ROGUE_GYM):
        return BuildGym(place);
    case MAP_NUM(MAP_ROGUE_LEAGUE):
        return BuildLeague(place);
    case MAP_NUM(MAP_ROGUE_POKEMON_CENTER):
        return BuildRoomWithWarps(mapNum, place, RWARP_PC);
    case MAP_NUM(MAP_ROGUE_MART):
        return BuildRoomWithWarps(mapNum, place, RWARP_MART);
    case MAP_NUM(MAP_ROGUE_HOUSE):
    {
        u32 house = gSaveBlock1Ptr->location.warpId;
        if (house >= ROGUE_MAX_HOUSES)
            house = 0;
        return BuildHouse(place, house);
    }
    }
    return NULL;
}

// ---------------------------------------------------------------------------
// Town signatures (src/rogue_gen_signatures.h): each town's trade, its
// landmark, its people and their small things to do.
//
// A signature person (ROBJ_SIGFOLK) talks about the town's trade; one may
// give the town's gift Pokemon (rolled per region, two to four towns), one
// its small thing (a soak in the hot spring, a bottle of MOOMOO MILK, a
// market stall). Inside a landmark you can enter (RINT_SIGNATURE) its host
// gives the gift. The gifts never use up the town's catch.

extern const u8 Rogue_EventScript_SigFolk[];
extern const u8 Rogue_EventScript_Dojo0[];
extern const u8 Rogue_EventScript_Dojo1[];
extern const u8 Rogue_EventScript_DojoMaster[];

// What a signature person does when spoken to (VAR_RESULT for the script).
enum { SIGACT_TALK, SIGACT_GIFT_MON, SIGACT_GIFT_ITEM, SIGACT_STALL, SIGACT_SOAK };

// Who they are: four looks per signature (RogueObj.slot >> 2).
static const u16 sSigFolkGfx[RSIG_BUILT_COUNT][4] =
{
    [RSIG_FISHING]    = { OBJ_EVENT_GFX_FISHERMAN, OBJ_EVENT_GFX_FISHERMAN, OBJ_EVENT_GFX_SAILOR, OBJ_EVENT_GFX_OLD_MAN },
    [RSIG_HOT_SPRING] = { OBJ_EVENT_GFX_OLD_WOMAN, OBJ_EVENT_GFX_OLD_MAN, OBJ_EVENT_GFX_WOMAN_5, OBJ_EVENT_GFX_FAT_MAN },
    [RSIG_SPACE]      = { OBJ_EVENT_GFX_SCIENTIST_1, OBJ_EVENT_GFX_SCIENTIST_2, OBJ_EVENT_GFX_LITTLE_BOY, OBJ_EVENT_GFX_WOMAN_4 },
    [RSIG_MARKET]     = { OBJ_EVENT_GFX_WOMAN_3, OBJ_EVENT_GFX_MAN_4, OBJ_EVENT_GFX_ROOFTOP_SALE_WOMAN, OBJ_EVENT_GFX_FAT_MAN },
    [RSIG_MUSEUM]     = { OBJ_EVENT_GFX_GENTLEMAN, OBJ_EVENT_GFX_SCIENTIST_1, OBJ_EVENT_GFX_SCHOOL_KID_M, OBJ_EVENT_GFX_ARTIST },
    [RSIG_BERRY]      = { OBJ_EVENT_GFX_EXPERT_M, OBJ_EVENT_GFX_OLD_WOMAN, OBJ_EVENT_GFX_GIRL_1, OBJ_EVENT_GFX_BOY_1 },
    [RSIG_FLOWERS]    = { OBJ_EVENT_GFX_WOMAN_2, OBJ_EVENT_GFX_GIRL_3, OBJ_EVENT_GFX_LASS, OBJ_EVENT_GFX_WOMAN_5 },
    [RSIG_RANCH]      = { OBJ_EVENT_GFX_MAN_1, OBJ_EVENT_GFX_GIRL_2, OBJ_EVENT_GFX_FAT_MAN, OBJ_EVENT_GFX_BOY_2 },
    [RSIG_WEATHER]    = { OBJ_EVENT_GFX_SCIENTIST_1, OBJ_EVENT_GFX_SCIENTIST_2, OBJ_EVENT_GFX_MAN_4, OBJ_EVENT_GFX_WOMAN_4 },
    [RSIG_FOSSIL]     = { OBJ_EVENT_GFX_HIKER, OBJ_EVENT_GFX_MANIAC, OBJ_EVENT_GFX_SCIENTIST_1, OBJ_EVENT_GFX_HIKER },
    // (Not FRLG's WORKER_M: with one in view, closing START > MAP resets
    // the game, found 2026-10-07 on v0.13.0 seed 4 and 909.)
    [RSIG_MINING]     = { OBJ_EVENT_GFX_HIKER, OBJ_EVENT_GFX_MAN_4, OBJ_EVENT_GFX_MAN_3, OBJ_EVENT_GFX_HIKER },
    [RSIG_DOJO]       = { OBJ_EVENT_GFX_BLACK_BELT, OBJ_EVENT_GFX_GIRL_3, OBJ_EVENT_GFX_EXPERT_M, OBJ_EVENT_GFX_BLACK_BELT },
};

// The Pokemon about town that fit the trade (ROBJ_SIGMON, by slot).
static const u16 sSigMons[RSIG_BUILT_COUNT][3] =
{
    [RSIG_FISHING]    = { SPECIES_WINGULL, SPECIES_WINGULL, SPECIES_WINGULL },
    [RSIG_HOT_SPRING] = { SPECIES_NUMEL, SPECIES_TORKOAL, SPECIES_NUMEL },
    [RSIG_SPACE]      = { SPECIES_CLEFAIRY, SPECIES_LUNATONE, SPECIES_SOLROCK },
    [RSIG_MARKET]     = { SPECIES_KECLEON, SPECIES_MEOWTH, SPECIES_KECLEON },
    [RSIG_MUSEUM]     = { SPECIES_BALTOY, SPECIES_BALTOY, SPECIES_BALTOY },
    [RSIG_BERRY]      = { SPECIES_ODDISH, SPECIES_SKIPLOOM, SPECIES_ODDISH },
    [RSIG_FLOWERS]    = { SPECIES_COMBEE, SPECIES_ROSELIA, SPECIES_COMBEE },
    [RSIG_RANCH]      = { SPECIES_MILTANK, SPECIES_MAREEP, SPECIES_PONYTA },
    [RSIG_WEATHER]    = { SPECIES_CASTFORM, SPECIES_CASTFORM, SPECIES_CASTFORM },
    [RSIG_FOSSIL]     = { SPECIES_GEODUDE, SPECIES_BALTOY, SPECIES_GEODUDE },
    [RSIG_MINING]     = { SPECIES_GEODUDE, SPECIES_MACHOP, SPECIES_GEODUDE },
    [RSIG_DOJO]       = { SPECIES_MAKUHITA, SPECIES_MACHOP, SPECIES_MAKUHITA },
};

// A berry farm's trees: berries that cure what ails a team on the road.
static const u16 sFarmBerries[ROGUE_FARM_BERRIES] = { ITEM_ORAN_BERRY, ITEM_PECHA_BERRY, ITEM_CHERI_BERRY, ITEM_RAWST_BERRY };

static u32 SigOf(u32 place)
{
    u32 sig = RogueGen_TownSignature(Rogue_GetRegion(), place);
    return sig < RSIG_BUILT_COUNT ? sig : RSIG_NONE;
}

u16 Rogue_SigFolkGfx(u32 place, u32 slot)
{
    u32 sig = SigOf(place);
    if ((slot & 3) == SIGROLE_POST && sig == RSIG_HOT_SPRING)
        return OBJ_EVENT_GFX_HOT_SPRINGS_OLD_WOMAN; // soaking in the water, as in Lavaridge
    if ((slot & 3) == SIGROLE_POST && sig == RSIG_MINING)
        return OBJ_EVENT_GFX_HIKER;
    if (sig == RSIG_NONE)
        return OBJ_EVENT_GFX_MAN_1;
    return sSigFolkGfx[sig][(slot >> 2) & 3];
}

u16 Rogue_SigMonSpecies(u32 place, u32 slot)
{
    u32 sig = SigOf(place);
    return sig == RSIG_NONE ? SPECIES_ZIGZAGOON : sSigMons[sig][slot % 3];
}

u16 Rogue_FarmBerry(u32 i)
{
    return sFarmBerries[i % ROGUE_FARM_BERRIES];
}

// What they say comes from the text engine (src/rogue_text_signatures.h),
// in ASCII, converted here.
static void SigSay(u32 place, u8 what, u32 k)
{
    char buf[400];
    RogueText_SigLine(Rogue_GetRegion(), place, what, k, buf, sizeof(buf));
    Rogue_CopyLoreAscii(gStringVar1, buf);
}

static bool32 InSigInterior(void)
{
    return gSaveBlock1Ptr->location.mapNum >= ROGUE_MAX_PLACES;
}

// Interior people keep their role in trainerRange_berryTreeId (ROLE_SIG_*).
enum { ROLE_SIG_TALK = 0x40, ROLE_SIG_GIFT };

// A signature person's role, and in *turn their turn among the town's
// signature people for their line (RogueText_SigTurn: the street's, then
// those inside), so no two of them say the same.
static u32 SigTalkerRole(u32 *turn)
{
    u32 localId = gSpecialVar_LastTalked;
    const struct RogueMap *m = Rogue_GetPlaceMap(Rogue_GetCurrentTownPlace());
    *turn = localId;
    if (InSigInterior())
    {
        *turn = RogueText_SigTurn(m, -1, localId >= 1 ? localId - 1 : 0);
        if (localId >= 1 && localId <= sInteriorEvents.objectEventCount
         && sInteriorObjects[localId - 1].trainerRange_berryTreeId == ROLE_SIG_GIFT)
            return SIGROLE_GIFT;
        return SIGROLE_TALK;
    }
    else if (localId >= 1 && localId <= m->nObjs && m->objs[localId - 1].kind == ROBJ_SIGFOLK)
    {
        *turn = RogueText_SigTurn(m, localId - 1, 0);
        return m->objs[localId - 1].slot & 3;
    }
    return SIGROLE_TALK;
}

// The town's small item: a LAVA COOKIE at the hot spring, MOOMOO MILK at
// the ranch (src/rogue_gen_signatures.h, in the ledger).
static bool32 SigItemDue(u32 place, u16 *item, u8 *qty)
{
    *item = RogueGen_SignatureItem(Rogue_GetRegion(), place, qty);
    return *item != ITEM_NONE && !FlagGet(FLAG_ROGUE_SIG_ITEM_0 + Rogue_GetCurrentTown() % ROGUE_MAX_TOWNS);
}

// Script special for a signature person: what they do into VAR_RESULT
// (SIGACT_*), their words into STR_VAR_1, and for a gift its species and
// level (or item and count) into VAR_0x8000 and VAR_0x8001.
void Rogue_PrepareSigFolk(void)
{
    u32 place = Rogue_GetCurrentTownPlace(), sig = SigOf(place), turn, role = SigTalkerRole(&turn);
    u16 item;
    u8 qty, level;

    gSpecialVar_Result = SIGACT_TALK;
    if (sig == RSIG_NONE)
    {
        StringCopy(gStringVar1, COMPOUND_STRING("Hello there!"));
        return;
    }
    // The dojo's MASTER, beaten (the battle comes first): his pupil where
    // the town gives one, else a BLACK BELT once; then a word about the win.
    if (sig == RSIG_DOJO && role == SIGROLE_GIFT && InSigInterior())
    {
        u16 species = RogueGen_SignatureGift(Rogue_GetRegion(), place, &level);
        if (species == SPECIES_NONE && SigItemDue(place, &item, &qty))
        {
            gSpecialVar_0x8000 = item;
            gSpecialVar_0x8001 = qty;
            SigSay(place, RSIGTEXT_DOJO_REWARD, 0);
            gSpecialVar_Result = SIGACT_GIFT_ITEM;
            return;
        }
        if (species == SPECIES_NONE || FlagGet(FLAG_ROGUE_SIG_MON_0 + Rogue_GetCurrentTown() % ROGUE_MAX_TOWNS))
        {
            SigSay(place, RSIGTEXT_DOJO, 2 * 3 + 2);
            return;
        }
    }
    if (role == SIGROLE_GIFT)
    {
        u16 species = RogueGen_SignatureGift(Rogue_GetRegion(), place, &level);
        if (species != SPECIES_NONE && !FlagGet(FLAG_ROGUE_SIG_MON_0 + Rogue_GetCurrentTown() % ROGUE_MAX_TOWNS))
        {
            gSpecialVar_0x8000 = species;
            gSpecialVar_0x8001 = level;
            SigSay(place, RSIGTEXT_GIFT, 0);
            gSpecialVar_Result = SIGACT_GIFT_MON;
            return;
        }
    }
    if (role == SIGROLE_ITEM)
    {
        switch (sig)
        {
        case RSIG_HOT_SPRING:
            SigSay(place, RSIGTEXT_SOAK, 0);
            gSpecialVar_Result = SIGACT_SOAK;
            return;
        case RSIG_MARKET:
            SigSay(place, RSIGTEXT_STALL, 0);
            gSpecialVar_Result = SIGACT_STALL;
            return;
        default:
            if (SigItemDue(place, &item, &qty))
            {
                gSpecialVar_0x8000 = item;
                gSpecialVar_0x8001 = qty;
                SigSay(place, RSIGTEXT_MILK, 0);
                gSpecialVar_Result = SIGACT_GIFT_ITEM;
                return;
            }
            break;
        }
    }
    // The town's signature people take turns in its bank.
    SigSay(place, RSIGTEXT_TALK, turn);
}

void Rogue_MarkSigMonGiven(void)
{
    FlagSet(FLAG_ROGUE_SIG_MON_0 + Rogue_GetCurrentTown() % ROGUE_MAX_TOWNS);
    StringCopy(gStringVar1, GetSpeciesName(gSpecialVar_0x8000));
}

void Rogue_MarkSigItemGiven(void)
{
    FlagSet(FLAG_ROGUE_SIG_ITEM_0 + Rogue_GetCurrentTown() % ROGUE_MAX_TOWNS);
}

void Rogue_BufferSigGiftAfter(void)
{
    SigSay(Rogue_GetCurrentTownPlace(), RSIGTEXT_GIFT_AFTER, 0);
}

// Script special: a soak in the hot spring cures the team's status (not
// their HP: that is the POKéMON CENTER's job). VAR_RESULT TRUE if the
// bather's LAVA COOKIE is still to give (VAR_0x8000, VAR_0x8001), its line
// in STR_VAR_1.
void Rogue_HotSpringSoak(void)
{
    u32 i, place = Rogue_GetCurrentTownPlace();
    u16 item;
    u8 qty;
    for (i = 0; i < gPlayerPartyCount; i++)
    {
        u32 status = STATUS1_NONE;
        if (GetMonData(&gPlayerParty[i], MON_DATA_SPECIES) != SPECIES_NONE && !GetMonData(&gPlayerParty[i], MON_DATA_IS_EGG))
            SetMonData(&gPlayerParty[i], MON_DATA_STATUS, &status);
    }
    gSpecialVar_Result = SigItemDue(place, &item, &qty);
    gSpecialVar_0x8000 = item;
    gSpecialVar_0x8001 = qty;
    SigSay(place, RSIGTEXT_COOKIE, 0);
}

// Script special: a market stall's goods, chosen per run and town, into the
// specialty counter's list (opened by Rogue_OpenSpecialtyShop).
void Rogue_PrepareMarketStall(void)
{
    static const u16 sGoods[] = { ITEM_POTION, ITEM_SUPER_POTION, ITEM_ANTIDOTE, ITEM_PARALYZE_HEAL, ITEM_AWAKENING,
                                  ITEM_BURN_HEAL, ITEM_ICE_HEAL, ITEM_REPEL, ITEM_ESCAPE_ROPE, ITEM_POKE_DOLL, ITEM_HONEY };
    static const u16 sDeals[] = { ITEM_NET_BALL, ITEM_NEST_BALL, ITEM_DIVE_BALL, ITEM_LUXURY_BALL, ITEM_HEAL_BALL,
                                  ITEM_REPEAT_BALL, ITEM_SUPER_REPEL, ITEM_FULL_HEAL };
    u32 place = Rogue_GetCurrentTownPlace(), rng = RogueRng_Hash(Rogue_GetSeed(), 23500 + place), n = 0, i, tries;
    for (tries = 0; tries < 40 && n < 5; tries++)
    {
        u16 item = sGoods[RogueRng_Range(&rng, ARRAY_COUNT(sGoods))];
        for (i = 0; i < n && sSpecialtyStock[i] != item; i++)
            ;
        if (i == n)
            sSpecialtyStock[n++] = item;
    }
    sSpecialtyStock[n++] = sDeals[RogueRng_Range(&rng, ARRAY_COUNT(sDeals))];
    if (Rogue_BadgeCount() >= 5)
        sSpecialtyStock[n++] = ITEM_HYPER_POTION;
    sSpecialtyStock[n] = ITEM_NONE;
}

// Script special: a landmark's sign, its name and motto into STR_VAR_1.
void Rogue_BufferLandmarkSign(void)
{
    SigSay(Rogue_GetCurrentTownPlace(), RSIGTEXT_SIGN, 0);
}

// Script special: a museum exhibit (VAR_0x8004 0 or 1) into STR_VAR_1.
void Rogue_BufferExhibit(void)
{
    SigSay(Rogue_GetCurrentTownPlace(), RSIGTEXT_EXHIBIT, gSpecialVar_0x8004);
}

// The landmarks you can enter, on their vanilla interiors.
static const struct InteriorDef sSigInteriors[RSIG_BUILT_COUNT] =
{
    [RSIG_SPACE]   = { LAYOUT_MOSSDEEP_CITY_SPACE_CENTER_1F, 7, 9, 8, { { 7, 2 }, { 2, 2 }, { 14, 7 } } },
    [RSIG_WEATHER] = { LAYOUT_ROUTE119_WEATHER_INSTITUTE_1F, 9, 12, 10, { { 5, 4 }, { 2, 11 }, { 15, 3 } } },
    [RSIG_MUSEUM]  = { LAYOUT_SLATEPORT_CITY_OCEANIC_MUSEUM_1F, 9, 8, 10, { { 7, 7 }, { 4, 2 }, { 12, 2 } } },
    [RSIG_FOSSIL]  = { LAYOUT_ROUTE114_FOSSIL_MANIACS_HOUSE, 4, 7, 5, { { 3, 2 }, { 7, 5 }, { 8, 3 } } },
    // Brawly's hall in Dewford, without his gym's darkness.
    [RSIG_DOJO]    = { LAYOUT_DEWFORD_TOWN_GYM, 5, 27, 6, { { 4, 3 }, { 7, 8 }, { 12, 25 } } },
};
static const struct InteriorDef sSigLilyMuseum = { LAYOUT_LILYCOVE_CITY_LILYCOVE_MUSEUM_1F, 9, 13, 10, { { 16, 2 }, { 13, 7 }, { 13, 10 } } };

static const struct InteriorDef *SigInterior(u32 place)
{
    u32 sig = SigOf(place);
    if (sig == RSIG_MUSEUM && Rogue_GetRegion()->places[place].theme == RTHEME_COVE)
        return &sSigLilyMuseum;
    if (sig == RSIG_NONE || sSigInteriors[sig].layout == 0)
        return &sSpecialInteriors[RINT_REST];
    return &sSigInteriors[sig];
}

static void AddSigTrainer(u16 gfx, const u8 *spot, const u8 *script, u8 movement, u8 sight)
{
    struct ObjectEventTemplate *t = &sInteriorObjects[sInteriorEvents.objectEventCount];
    if (sInteriorEvents.objectEventCount >= MAX_INTERIOR_OBJS)
        return;
    AddInteriorObject(gfx, spot, 0, script, movement);
    t->trainerType = TRAINER_TYPE_NORMAL;
    t->trainerRange_berryTreeId = sight;
}

// The people in a landmark you can enter: its host (who gives the gift, if
// the town has one), and one or two more; a dojo's disciples and master.
static void BuildSigPeople(u32 place, const struct InteriorDef *def)
{
    u32 sig = SigOf(place);
    if (sig == RSIG_DOJO)
    {
        AddInteriorObject(Rogue_GetDojoGfx(2), def->spots[0], ROLE_SIG_GIFT, Rogue_EventScript_DojoMaster, MOVEMENT_TYPE_FACE_DOWN);
        AddSigTrainer(Rogue_GetDojoGfx(0), def->spots[1], Rogue_EventScript_Dojo0, MOVEMENT_TYPE_FACE_RIGHT, 3);
        AddSigTrainer(Rogue_GetDojoGfx(1), def->spots[2], Rogue_EventScript_Dojo1, MOVEMENT_TYPE_FACE_UP, 3);
        return;
    }
    AddInteriorObject(sSigFolkGfx[sig][sig == RSIG_FOSSIL ? 1 : 0], def->spots[0], ROLE_SIG_GIFT, Rogue_EventScript_SigFolk,
                      MOVEMENT_TYPE_FACE_DOWN);
    AddInteriorObject(sSigFolkGfx[sig][sig == RSIG_FOSSIL ? 2 : 1], def->spots[1], ROLE_SIG_TALK, Rogue_EventScript_SigFolk,
                      MOVEMENT_TYPE_LOOK_AROUND);
    if (sig != RSIG_FOSSIL)
        AddInteriorObject(sSigFolkGfx[sig][2], def->spots[2], ROLE_SIG_TALK, Rogue_EventScript_SigFolk, MOVEMENT_TYPE_LOOK_AROUND);
    // (A museum's paintings, cases and statues are read by their place,
    // each saying what it shows: RogueText_MuseumExhibit.)
}

// ---------------------------------------------------------------------------
// Route features (src/rogue_gen_features.h): the Day Care, the BERRY MASTER,
// the glass workshop, the Seashore House, the sleeping SNORLAX, the battling
// family, the gauntlet's prize, the fishing guru and the old sailor. Their
// houses take a route's rest house slot (RINT_FEATURE, on vanilla
// interiors); their people's words come from src/rogue_text_features.h.
// Each feature's once-per-run thing (a gift, a reward, SNORLAX woken) is
// FLAG_ROGUE_FEAT_0 + the feature. Gifts use no catch.

extern const u8 Rogue_EventScript_FeatFolk[];
extern const u8 Rogue_EventScript_FamilyHead[];
extern const u8 *const gRogueTrainerScripts[];

// What a feature person does when spoken to (VAR_RESULT for the script).
enum { FEATACT_TALK, FEATACT_GIFT_MON, FEATACT_GIFT_ITEM, FEATACT_FLUTE };
// Interior people keep their role in trainerRange_berryTreeId.
enum { ROLE_FEAT_TALK = 0x60, ROLE_FEAT_HOST };

static u32 FeatPlace(void)
{
    s32 place = Rogue_GetCurrentPlace();
    return place < 0 ? 0 : place;
}

static u32 FeatOf(u32 place)
{
    return RogueGen_RouteFeature(Rogue_GetRegion(), place);
}

// The people outside: the Day Care's old man, the guru's sibling, the
// gauntlet's prize man, the child by SNORLAX (by FEATROLE_*).
u16 Rogue_FeatFolkGfx(u32 place, u32 slot)
{
    switch (slot & 3)
    {
    case FEATROLE_KID:   return OBJ_EVENT_GFX_LITTLE_BOY;
    case FEATROLE_PRIZE: return OBJ_EVENT_GFX_MAN_4;
    }
    switch (FeatOf(place))
    {
    case RFEAT_DAYCARE: return OBJ_EVENT_GFX_OLD_MAN;
    case RFEAT_GURU:    return OBJ_EVENT_GFX_FISHERMAN;
    }
    return OBJ_EVENT_GFX_MAN_1;
}

// The Day Care's two in the yard, the old sailor's Wingull.
u16 Rogue_FeatMonSpecies(u32 place, u32 slot)
{
    u32 rng = RogueRng_Hash(Rogue_GetSeed(), 42500 + slot);
    if (slot == 2)
        return SPECIES_WINGULL;
    return sPets[(rng >> 8) % ARRAY_COUNT(sPets)];
}

static void FeatSay(u32 place, u8 what, u32 k)
{
    char buf[400];
    RogueText_FeatLine(Rogue_GetRegion(), place, what, k, buf, sizeof(buf));
    Rogue_CopyLoreAscii(gStringVar1, buf);
}

static bool32 InFeatInterior(void)
{
    return gSaveBlock1Ptr->location.mapNum >= ROGUE_MAX_PLACES;
}

static bool32 FeatDone(u32 feat)
{
    return FlagGet(FLAG_ROGUE_FEAT_0 + feat);
}

static bool32 SlotsBeaten(u32 first, u32 n)
{
    u32 i;
    for (i = 0; i < n; i++)
        if (!HasTrainerBeenFought(ROGUE_TRAINER(first + i)))
            return FALSE;
    return TRUE;
}

// Every trainer of the gauntlet's route beaten: how many are left.
static u32 GauntletLeft(u32 place)
{
    const struct RoguePlace *p = &Rogue_GetRegion()->places[place];
    u32 i, left = 0;
    for (i = 0; i < p->nTrainers; i++)
        if (!HasTrainerBeenFought(ROGUE_TRAINER(p->trainerBase + i)))
            left++;
    return left;
}

u32 Rogue_GauntletLeft(u32 place)
{
    return GauntletLeft(place);
}

// A WATER POKeMON the player caught, in the party or a BOX: on this route
// (place), or anywhere in the region (place 0xFF).
static bool32 IsWaterCaught(u16 species, bool32 egg, u32 met, u32 place)
{
    if (species == SPECIES_NONE || egg || met < MAPSEC_ROGUE_PLACE_0 || met >= MAPSEC_ROGUE_PLACE_0 + ROGUE_MAX_PLACES)
        return FALSE;
    if (place != 0xFF && met != MAPSEC_ROGUE_PLACE_0 + place)
        return FALSE;
    return gSpeciesInfo[species].types[0] == TYPE_WATER || gSpeciesInfo[species].types[1] == TYPE_WATER;
}

static bool32 HasWaterCaught(u32 place)
{
    u32 i, box;
    for (i = 0; i < PARTY_SIZE; i++)
        if (IsWaterCaught(GetMonData(&gPlayerParty[i], MON_DATA_SPECIES), GetMonData(&gPlayerParty[i], MON_DATA_IS_EGG),
                          GetMonData(&gPlayerParty[i], MON_DATA_MET_LOCATION), place))
            return TRUE;
    for (box = 0; box < TOTAL_BOXES_COUNT; box++)
        for (i = 0; i < IN_BOX_COUNT; i++)
            if (IsWaterCaught(GetBoxMonDataAt(box, i, MON_DATA_SPECIES), GetBoxMonDataAt(box, i, MON_DATA_IS_EGG),
                              GetBoxMonDataAt(box, i, MON_DATA_MET_LOCATION), place))
                return TRUE;
    return FALSE;
}

// The fishing guru's wish: a WATER POKeMON caught on his route; once its one
// catch has gone to something else (and no WATER POKeMON came of it), any
// WATER POKeMON caught in the region (*any TRUE).
static bool32 HasWaterCaughtHere(u32 place, bool32 *any)
{
    s32 area = Rogue_GetRegion()->places[place].area;
    *any = FALSE;
    if (HasWaterCaught(place))
        return TRUE;
    if (area != 0xFF && Rogue_IsAreaCatchUsed(area))
    {
        *any = TRUE;
        return HasWaterCaught(0xFF);
    }
    return FALSE;
}

// Who was spoken to: FEATROLE_* outside, the host or not inside, and their
// order among the place's feature people (for the talk lines).
static u32 FeatTalkerRole(u32 feat, u32 *order)
{
    u32 localId = gSpecialVar_LastTalked;
    *order = localId;
    if (InFeatInterior())
    {
        if (feat == RFEAT_FAMILY)
            return FEATROLE_HOST;
        if (localId >= 1 && localId <= sInteriorEvents.objectEventCount
         && sInteriorObjects[localId - 1].trainerRange_berryTreeId == ROLE_FEAT_HOST)
            return FEATROLE_HOST;
        return FEATROLE_TALK;
    }
    else
    {
        const struct RogueMap *m = Rogue_GetPlaceMap(FeatPlace());
        if (localId >= 1 && localId <= m->nObjs && m->objs[localId - 1].kind == ROBJ_FEATFOLK)
            return m->objs[localId - 1].slot & 3;
    }
    return FEATROLE_TALK;
}

// The local id of the sleeping SNORLAX on this route's map, or 0.
static u32 SnorlaxLocalId(u32 place)
{
    const struct RogueMap *m = Rogue_GetPlaceMap(place);
    u32 i;
    for (i = 0; i < m->nObjs; i++)
        if (m->objs[i].kind == ROBJ_SNORLAX)
            return i + 1;
    return 0;
}

// Script special for a feature person: what they do into VAR_RESULT
// (FEATACT_*), their words into STR_VAR_1, and for a gift its species and
// level (or item and count) into VAR_0x8000 and VAR_0x8001. The child by
// SNORLAX: its species, level and local id into VAR_0x8004 to VAR_0x8006.
void Rogue_PrepareFeatFolk(void)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    u32 place = FeatPlace(), feat = FeatOf(place), order, role = FeatTalkerRole(feat, &order);
    u8 level, qty;
    u16 species, item;
    bool32 earned, anyWater = FALSE;

    gSpecialVar_Result = FEATACT_TALK;
    if (feat == RFEAT_NONE)
    {
        StringCopy(gStringVar1, COMPOUND_STRING("Hello there!"));
        return;
    }
    if (role == FEATROLE_KID)
    {
        if (FeatDone(RFEAT_SNORLAX) || SnorlaxLocalId(place) == 0)
        {
            FeatSay(place, RFEATTEXT_KID_AFTER, order);
            return;
        }
        gSpecialVar_0x8004 = SPECIES_SNORLAX;
        gSpecialVar_0x8005 = RogueGen_SnorlaxLevel(r);
        gSpecialVar_0x8006 = SnorlaxLocalId(place);
        FeatSay(place, RFEATTEXT_KID, order);
        gSpecialVar_Result = FEATACT_FLUTE;
        return;
    }
    if (role == FEATROLE_HOST || role == FEATROLE_PRIZE)
    {
        if (!FeatDone(feat))
        {
            species = RogueGen_FeatureGift(r, feat, &level);
            if (species != SPECIES_NONE)
            {
                gSpecialVar_0x8000 = species;
                gSpecialVar_0x8001 = level;
                FeatSay(place, RFEATTEXT_GIFT, 0);
                gSpecialVar_Result = FEATACT_GIFT_MON;
                return;
            }
            item = RogueGen_FeatureItem(r, feat, &qty);
            switch (feat)
            {
            case RFEAT_SEASHORE: earned = SlotsBeaten(ROGUE_SLOT_SEASHORE_0, ROGUE_SEASHORE_TRAINERS); break;
            case RFEAT_FAMILY:   earned = SlotsBeaten(ROGUE_SLOT_FAMILY_0, ROGUE_FAMILY_TRAINERS); break;
            case RFEAT_GAUNTLET: earned = GauntletLeft(place) == 0; break;
            case RFEAT_GURU:     earned = HasWaterCaughtHere(place, &anyWater); break;
            default:             earned = TRUE; break;
            }
            if (item != ITEM_NONE && earned)
            {
                gSpecialVar_0x8000 = item;
                gSpecialVar_0x8001 = qty;
                FeatSay(place, RFEATTEXT_GIFT, anyWater);
                gSpecialVar_Result = FEATACT_GIFT_ITEM;
                return;
            }
            if (item != ITEM_NONE)
            {
                FeatSay(place, RFEATTEXT_NOT_YET, anyWater);
                return;
            }
        }
        FeatSay(place, RFEATTEXT_DONE, order);
        return;
    }
    FeatSay(place, RFEATTEXT_TALK, order);
}

void Rogue_MarkFeatDone(void)
{
    FlagSet(FLAG_ROGUE_FEAT_0 + FeatOf(FeatPlace()));
    StringCopy(gStringVar1, GetSpeciesName(gSpecialVar_0x8000));
}

void Rogue_BufferFeatAfter(void)
{
    u32 place = FeatPlace(), feat = FeatOf(place);
    FeatSay(place, feat == RFEAT_SNORLAX ? RFEATTEXT_KID_AFTER : RFEATTEXT_GIFT_AFTER, gSpecialVar_LastTalked);
}

// Script special: the feature's sign, its name and motto, into STR_VAR_1.
void Rogue_BufferFeatureSign(void)
{
    FeatSay(FeatPlace(), RFEATTEXT_SIGN, 0);
}

// Script special: SNORLAX asleep, into STR_VAR_1.
void Rogue_BufferSnorlax(void)
{
    FeatSay(FeatPlace(), RFEATTEXT_SNORLAX, 0);
}

// Script special: SNORLAX (VAR_0x8004 at VAR_0x8005) for dowildbattle, a
// one-off like a legend's: it uses no catch.
void Rogue_CreateSnorlax(void)
{
    CreateScriptedWildMon(gSpecialVar_0x8004, gSpecialVar_0x8005, ITEM_LEFTOVERS);
}

// The houses of the features, on their vanilla interiors: the host stands
// at the first spot.
static const struct InteriorDef sFeatInteriors[RFEAT_COUNT] =
{
    [RFEAT_DAYCARE]  = { LAYOUT_ROUTE117_POKEMON_DAY_CARE, 2, 8, 3, { { 2, 2 }, { 5, 6 }, { 5, 6 } } },
    [RFEAT_BERRY]    = { LAYOUT_HOUSE2, 3, 7, 4, { { 4, 4 }, { 7, 4 }, { 2, 3 } } },
    [RFEAT_WORKSHOP] = { LAYOUT_HOUSE4, 3, 8, 4, { { 2, 3 }, { 5, 4 }, { 8, 3 } } },
    [RFEAT_SEASHORE] = { LAYOUT_ROUTE109_SEASHORE_HOUSE, 6, 9, 7, { { 6, 2 }, { 2, 3 }, { 10, 5 } } },
    [RFEAT_FAMILY]   = { LAYOUT_HOUSE2, 3, 7, 4, { { 4, 4 }, { 7, 4 }, { 4, 5 } } },
    [RFEAT_GURU]     = { LAYOUT_HOUSE1, 3, 8, 4, { { 4, 4 }, { 6, 6 }, { 7, 4 } } },
    [RFEAT_SAILOR]   = { LAYOUT_ROUTE104_MR_BRINEYS_HOUSE, 5, 8, 6, { { 5, 3 }, { 6, 3 }, { 8, 3 } } },
};

static const struct InteriorDef *FeatInterior(u32 place)
{
    u32 feat = FeatOf(place);
    if (feat >= RFEAT_COUNT || sFeatInteriors[feat].layout == 0)
        return &sSpecialInteriors[RINT_REST];
    return &sFeatInteriors[feat];
}

static void BuildFeatPeople(u32 place, const struct InteriorDef *def)
{
    static const u8 sFamilySpots[ROGUE_FAMILY_TRAINERS][2] = { { 4, 4 }, { 7, 4 }, { 4, 5 }, { 7, 5 } };
    static const u8 sFamilyFacing[ROGUE_FAMILY_TRAINERS] = { MOVEMENT_TYPE_FACE_RIGHT, MOVEMENT_TYPE_FACE_LEFT,
                                                            MOVEMENT_TYPE_FACE_RIGHT, MOVEMENT_TYPE_FACE_LEFT };
    static const u8 sSeashoreSpots[ROGUE_SEASHORE_TRAINERS][2] = { { 14, 9 }, { 10, 5 }, { 2, 3 } };
    static const u8 sSeashoreFacing[ROGUE_SEASHORE_TRAINERS] = { MOVEMENT_TYPE_FACE_LEFT, MOVEMENT_TYPE_FACE_LEFT_AND_RIGHT,
                                                                MOVEMENT_TYPE_FACE_DOWN };
    const struct RoguePlace *p = &Rogue_GetRegion()->places[place];
    u32 feat = FeatOf(place), i;
    bool32 rest = (p->interiors & (1 << RINT_REST)) != 0;
    switch (feat)
    {
    case RFEAT_DAYCARE:
        AddInteriorObject(OBJ_EVENT_GFX_OLD_WOMAN, def->spots[0], ROLE_FEAT_HOST, Rogue_EventScript_FeatFolk, MOVEMENT_TYPE_FACE_DOWN);
        break;
    case RFEAT_BERRY:
        AddInteriorObject(OBJ_EVENT_GFX_EXPERT_M, def->spots[0], ROLE_FEAT_HOST, Rogue_EventScript_FeatFolk, MOVEMENT_TYPE_FACE_DOWN);
        AddInteriorObject(OBJ_EVENT_GFX_OLD_WOMAN, def->spots[1], ROLE_FEAT_TALK, Rogue_EventScript_FeatFolk, MOVEMENT_TYPE_FACE_DOWN);
        break;
    case RFEAT_WORKSHOP:
        AddInteriorObject(OBJ_EVENT_GFX_MAN_1, def->spots[0], ROLE_FEAT_HOST, Rogue_EventScript_FeatFolk, MOVEMENT_TYPE_FACE_DOWN);
        AddInteriorObject(OBJ_EVENT_GFX_WOMAN_2, def->spots[1], ROLE_FEAT_TALK, Rogue_EventScript_FeatFolk, MOVEMENT_TYPE_LOOK_AROUND);
        break;
    case RFEAT_SEASHORE:
        AddInteriorObject(OBJ_EVENT_GFX_POKEFAN_M, def->spots[0], ROLE_FEAT_HOST, Rogue_EventScript_FeatFolk, MOVEMENT_TYPE_FACE_DOWN);
        if (RogueGen_FeatureSlotPlace(Rogue_GetRegion(), ROGUE_SLOT_SEASHORE_0) == place)
            for (i = 0; i < ROGUE_SEASHORE_TRAINERS; i++)
                AddSigTrainer(Rogue_FeatureTrainerGfx(ROGUE_SLOT_SEASHORE_0 + i), sSeashoreSpots[i],
                              gRogueTrainerScripts[ROGUE_SLOT_SEASHORE_0 + i], sSeashoreFacing[i], 2);
        rest = FALSE;
        break;
    case RFEAT_FAMILY:
        // Four battles in a row, spoken to one after another; the last
        // gives the family's reward.
        if (RogueGen_FeatureSlotPlace(Rogue_GetRegion(), ROGUE_SLOT_FAMILY_0) == place)
            for (i = 0; i < ROGUE_FAMILY_TRAINERS; i++)
                AddSigTrainer(Rogue_FeatureTrainerGfx(ROGUE_SLOT_FAMILY_0 + i), sFamilySpots[i],
                              i == ROGUE_FAMILY_TRAINERS - 1 ? Rogue_EventScript_FamilyHead : gRogueTrainerScripts[ROGUE_SLOT_FAMILY_0 + i],
                              sFamilyFacing[i], 0);
        rest = FALSE;
        break;
    case RFEAT_GURU:
        AddInteriorObject(OBJ_EVENT_GFX_FISHERMAN, def->spots[0], ROLE_FEAT_HOST, Rogue_EventScript_FeatFolk, MOVEMENT_TYPE_FACE_DOWN);
        break;
    case RFEAT_SAILOR:
        AddInteriorObject(OBJ_EVENT_GFX_EXPERT_M, def->spots[0], ROLE_FEAT_HOST, Rogue_EventScript_FeatFolk, MOVEMENT_TYPE_FACE_DOWN);
        AddInteriorObject(OBJ_EVENT_MON + SPECIES_WINGULL, def->spots[1], ROLE_PET, Rogue_EventScript_TownMon, MOVEMENT_TYPE_FACE_DOWN);
        break;
    }
    // Where the plan put a rest house, the feature's house lets travellers
    // rest too.
    if (rest)
        AddInteriorObject(RestKeeperGfx(place), def->spots[2], ROLE_CHAT, Rogue_EventScript_RestHouse, MOVEMENT_TYPE_FACE_DOWN);
}
