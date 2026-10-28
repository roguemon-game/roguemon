// Who the people of a region are: the gym leaders and Elite Four drawn for
// it, and the class of every trainer slot. Included at the end of
// rogue_gen.c, so the host tools (gen_host's text dump) know who says what.
// The game (src/rogue.c) asks here and keeps the battle data (sprites,
// music, teams, lines) in its own tables, in the same order as these.
// Nothing here changes the region: it only reads the plan and the seed.

#include "constants/rogue.h"

// ---------------------------------------------------------------------------
// Gym leaders and the Elite Four

// Roguémon's own eight, Emerald's gym leaders and Elite Four, and FireRed
// and LeafGreen's. In the order of the game's sLeaders (src/rogue.c checks the count).
const struct RogueGenLeader gRogueGenLeaders[ROGUE_LEADER_COUNT] =
{
    { "CRAG", RTYPE_ROCK, 0, FALSE, "BEDROCK BADGE" },
    { "MARINA", RTYPE_WATER, 1, FALSE, "TIDE BADGE" },
    { "BASIL", RTYPE_GRASS, 0, FALSE, "BLOSSOM BADGE" },
    { "CINDER", RTYPE_FIRE, 0, FALSE, "FORGE BADGE" },
    { "VOLTZ", RTYPE_ELECTRIC, 0, FALSE, "VOLT BADGE" },
    { "MAKO", RTYPE_FIGHTING, 1, FALSE, "FIST BADGE" },
    { "LUNA", RTYPE_PSYCHIC, 1, FALSE, "DREAM BADGE" },
    { "ACE", RTYPE_FLYING, 0, FALSE, "GALE BADGE" },
    { "ROXANNE", RTYPE_ROCK, 1, FALSE, "STONE BADGE" },
    { "BRAWLY", RTYPE_FIGHTING, 0, FALSE, "KNUCKLE BADGE" },
    { "WATTSON", RTYPE_ELECTRIC, 0, FALSE, "DYNAMO BADGE" },
    { "FLANNERY", RTYPE_FIRE, 1, FALSE, "HEAT BADGE" },
    { "NORMAN", RTYPE_NORMAL, 0, FALSE, "BALANCE BADGE" },
    { "WINONA", RTYPE_FLYING, 1, FALSE, "FEATHER BADGE" },
    { "TATE&LIZA", RTYPE_PSYCHIC, 0, TRUE, "MIND BADGE" },
    { "JUAN", RTYPE_WATER, 0, FALSE, "RAIN BADGE" },
    { "SIDNEY", RTYPE_DARK, 0, FALSE, "DUSK BADGE" },
    { "PHOEBE", RTYPE_GHOST, 1, FALSE, "SPIRIT BADGE" },
    { "GLACIA", RTYPE_ICE, 1, FALSE, "GLACIER BADGE" },
    { "DRAKE", RTYPE_DRAGON, 0, FALSE, "WYRM BADGE" },
    // FireRed and LeafGreen.
    { "BROCK", RTYPE_ROCK, 0, FALSE, "BOULDER BADGE" },
    { "MISTY", RTYPE_WATER, 1, FALSE, "CASCADE BADGE" },
    { "LT. SURGE", RTYPE_ELECTRIC, 0, FALSE, "THUNDER BADGE" },
    { "ERIKA", RTYPE_GRASS, 1, FALSE, "RAINBOW BADGE" },
    { "KOGA", RTYPE_POISON, 0, FALSE, "SOUL BADGE" },
    { "SABRINA", RTYPE_PSYCHIC, 1, FALSE, "MARSH BADGE" },
    { "BLAINE", RTYPE_FIRE, 0, FALSE, "VOLCANO BADGE" },
    { "GIOVANNI", RTYPE_GROUND, 0, FALSE, "EARTH BADGE" },
    { "LORELEI", RTYPE_ICE, 1, FALSE, "ICICLE BADGE" },
    { "BRUNO", RTYPE_FIGHTING, 0, FALSE, "STORM BADGE" },
    { "AGATHA", RTYPE_GHOST, 1, FALSE, "FOG BADGE" },
    { "LANCE", RTYPE_DRAGON, 0, FALSE, "RISING BADGE" },
};

// Type names as Gen 3 prints them in text: ROCK, not Rock.
const char *const gRogueTypeNames[RTYPE_COUNT] =
{
    [RTYPE_NONE] = "???", [RTYPE_NORMAL] = "NORMAL", [RTYPE_FIGHTING] = "FIGHTING", [RTYPE_FLYING] = "FLYING",
    [RTYPE_POISON] = "POISON", [RTYPE_GROUND] = "GROUND", [RTYPE_ROCK] = "ROCK", [RTYPE_BUG] = "BUG",
    [RTYPE_GHOST] = "GHOST", [RTYPE_STEEL] = "STEEL", [RTYPE_MYSTERY] = "???", [RTYPE_FIRE] = "FIRE",
    [RTYPE_WATER] = "WATER", [RTYPE_GRASS] = "GRASS", [RTYPE_ELECTRIC] = "ELECTRIC", [RTYPE_PSYCHIC] = "PSYCHIC",
    [RTYPE_ICE] = "ICE", [RTYPE_DRAGON] = "DRAGON", [RTYPE_DARK] = "DARK", [RTYPE_FAIRY] = "FAIRY",
};

static bool8 PeopleBeginnerType(u8 type)
{
    switch (type)
    {
    case RTYPE_ICE:
    case RTYPE_DRAGON:
    case RTYPE_STEEL:
    case RTYPE_GHOST:
    case RTYPE_DARK:
    case RTYPE_PSYCHIC:
        return FALSE;
    default:
        return TRUE;
    }
}

// The leaders of one type a seat may have, into list; returns how many. A
// double battle gym comes late, as Tate and Liza are the seventh in
// Emerald: never one of the first five gyms.
static u32 PeopleSeatLeaders(u8 type, u32 seat, u8 *list)
{
    u32 i, n = 0;
    for (i = 0; i < ROGUE_LEADER_COUNT; i++)
    {
        if (gRogueGenLeaders[i].type != type || (seat < 5 && gRogueGenLeaders[i].doubles))
            continue;
        if (list != NULL)
            list[n] = i;
        n++;
    }
    return n;
}

// Twelve leaders of twelve different types: eight gyms and the Elite Four,
// from the run's random stream (the game goes on with it for the starters).
// Each seat picks its type evenly among the types left (the first two gyms
// only types a new team can face), then a leader of that type: so a type
// with three leaders is no likelier in a seat than one with a single
// leader (before v0.14.3 the first leader of each type in a shuffle took
// the seats in turn, and WATER ran the first gym three times as often as
// GROUND).
void RogueGen_PickLeaders(u32 *rng, u8 *gyms, u8 *e4)
{
    u8 list[ROGUE_LEADER_COUNT], types[RTYPE_COUNT];
    u32 n, t, nt, used = 0;
    for (n = 0; n < ROGUE_NUM_GYMS + ROGUE_NUM_E4; n++)
    {
        u8 pick;
        nt = 0;
        for (t = 0; t < RTYPE_COUNT; t++)
        {
            if ((used & (1u << t)) || (n < 2 && !PeopleBeginnerType(t)) || PeopleSeatLeaders(t, n, NULL) == 0)
                continue;
            types[nt++] = t;
        }
        t = types[RogueRng_Range(rng, nt)];
        used |= 1u << t;
        pick = list[RogueRng_Range(rng, PeopleSeatLeaders(t, n, list))];
        if (n < ROGUE_NUM_GYMS)
            gyms[n] = pick;
        else
            e4[n - ROGUE_NUM_GYMS] = pick;
    }
}

#define LEADER_GIOVANNI 27 // gRogueGenLeaders

// Where the seed asks for Team Rocket, GIOVANNI, drawn as a gym leader or
// to the Elite Four, runs the last gym, as Viridian's in FireRed: the
// region meets him as Team Rocket's boss first, most likely, and finds him
// in his gym after (RogueText's Giovanni lines). He swaps with the eighth
// gym's leader (or the seventh, the sixth, keeping the first gyms' rules).
// From the seed alone, as the draw is.
static void RocketLeaders(u32 seed, u8 *gyms, u8 *e4)
{
    u8 ids[ROGUE_VILLAIN_IDS], n = RogueGen_TeamsWanted(seed, ids), t, k, pos = 0xFF, *at = NULL;
    bool8 rocket = FALSE;
    for (t = 0; t < n; t++)
        rocket |= ids[t] == ROGUE_VILLAIN_ROCKET;
    if (!rocket)
        return;
    for (k = 0; k < ROGUE_NUM_GYMS + ROGUE_NUM_E4; k++)
    {
        u8 *l = k < ROGUE_NUM_GYMS ? &gyms[k] : &e4[k - ROGUE_NUM_GYMS];
        if (*l == LEADER_GIOVANNI)
            at = l, pos = k;
    }
    if (at == NULL || pos == ROGUE_NUM_GYMS - 1)
        return;
    for (k = ROGUE_NUM_GYMS - 1; k >= 5; k--)
    {
        const struct RogueGenLeader *o = &gRogueGenLeaders[gyms[k]];
        if (k == pos)
            return;
        if ((pos < 5 && o->doubles) || (pos < 2 && !PeopleBeginnerType(o->type)))
            continue;
        *at = gyms[k];
        gyms[k] = LEADER_GIOVANNI;
        return;
    }
}

// The leaders of a region (gyms 0-7, then the Elite Four 8-11), drawn on
// their own random stream, so adding leaders to the pool changes no
// starters or wild Pokemon.
void RogueGen_RegionLeaders(const struct RogueRegion *r, u8 *gyms, u8 *e4)
{
    u32 rng = RogueRng_Hash(r->seed, 13013);
    // RUN 0's gyms are Roguemon's own eight (src/rogue_gen_run0.h).
    if (Run0Leaders(r, gyms, e4))
        return;
    RogueGen_PickLeaders(&rng, gyms, e4);
    RocketLeaders(r->seed, gyms, e4);
}

const struct RogueGenLeader *RogueGen_GymLeader(const struct RogueRegion *r, u8 gym)
{
    u8 gyms[ROGUE_NUM_GYMS], e4[ROGUE_NUM_E4];
    RogueGen_RegionLeaders(r, gyms, e4);
    return &gRogueGenLeaders[gym < ROGUE_NUM_GYMS ? gyms[gym] : e4[(gym - ROGUE_NUM_GYMS) % ROGUE_NUM_E4]];
}

// Trainers in each gym besides the leader.
const u8 gRogueGymTrainers[ROGUE_NUM_GYMS] = { 3, 2, 3, 3, 3, 3, 3, 3 };

// ---------------------------------------------------------------------------
// Trainer classes

// In the order of the game's sClasses (src/rogue.c checks the count): the
// name text uses, gender, the team's type on routes (RTYPE_NONE for any),
// the earliest route tier and the kind of talk.
const struct RogueGenClass gRogueGenClasses[ROGUE_CLASS_COUNT] =
{
    { "YOUNGSTER",   0, RTYPE_NONE,     0, RTALK_KID },
    { "LASS",        1, RTYPE_NONE,     0, RTALK_KID },
    { "BUG CATCHER", 0, RTYPE_BUG,      0, RTALK_BUGS },
    { "CAMPER",      0, RTYPE_NONE,     0, RTALK_OUTDOORS },
    { "PICNICKER",   1, RTYPE_GRASS,    1, RTALK_OUTDOORS },
    { "FISHERMAN",   0, RTYPE_WATER,    1, RTALK_WATER },
    { "BIRD KEEPER", 0, RTYPE_FLYING,   0, RTALK_BIRDS },
    { "TUBER",       1, RTYPE_WATER,    1, RTALK_KID },
    { "SCHOOL KID",  0, RTYPE_NONE,     1, RTALK_KID },
    { "HIKER",       0, RTYPE_ROCK,     1, RTALK_HIKER },
    { "RUIN MANIAC", 0, RTYPE_GROUND,   1, RTALK_HIKER },
    { "NINJA BOY",   0, RTYPE_POISON,   1, RTALK_KID },
    { "BLACK BELT",  0, RTYPE_FIGHTING, 1, RTALK_FIGHTER },
    { "BATTLE GIRL", 1, RTYPE_FIGHTING, 1, RTALK_FIGHTER },
    { "KINDLER",     0, RTYPE_FIRE,     1, RTALK_FIRE },
    { "AROMA LADY",  1, RTYPE_GRASS,    1, RTALK_FANCY },
    { "BEAUTY",      1, RTYPE_FAIRY,    1, RTALK_FANCY },
    { "GUITARIST",   0, RTYPE_ELECTRIC, 1, RTALK_MUSIC },
    { "SAILOR",      0, RTYPE_WATER,    1, RTALK_SAILOR },
    { "POKEFAN",     1, RTYPE_NONE,     1, RTALK_FANCY },
    { "PSYCHIC",     0, RTYPE_PSYCHIC,  2, RTALK_MYSTIC },
    { "HEX MANIAC",  1, RTYPE_GHOST,    2, RTALK_MYSTIC },
    { "GENTLEMAN",   0, RTYPE_NONE,     2, RTALK_FANCY },
    { "COOLTRAINER", 0, RTYPE_NONE,     3, RTALK_ACE },
    { "COOLTRAINER", 1, RTYPE_NONE,     3, RTALK_ACE },
    { "EXPERT",      0, RTYPE_NONE,     3, RTALK_ACE },
    { "SWIMMER",     0, RTYPE_WATER,    1, RTALK_WATER },
    { "SWIMMER",     1, RTYPE_WATER,    1, RTALK_WATER },
};

// The cave is full of Hikers, Ruin Maniacs and Black Belts.
static const u8 sCaveClasses[] = { RCLASS_HIKER, RCLASS_BLACK_BELT, RCLASS_RUIN_MANIAC };
// Each kind's master: a Hiker, a Cool Trainer, a Bug Catcher (a Bug Maniac in
// battle), a Hex Maniac, an Expert.
static const u8 sMasterClass[RDUN_COUNT] = { [RDUN_CAVE] = RCLASS_HIKER, [RDUN_VICTORY] = RCLASS_COOLTRAINER_F,
                                             [RDUN_FOREST] = RCLASS_BUG_CATCHER, [RDUN_TOWER] = RCLASS_HEX_MANIAC,
                                             [RDUN_HIDEOUT] = RCLASS_EXPERT };
// A tower is Mt. Pyre's: Hex Maniacs and Psychics, now and then a Black Belt.
static const u8 sTowerClasses[] = { RCLASS_HEX_MANIAC, RCLASS_PSYCHIC, RCLASS_HEX_MANIAC, RCLASS_PSYCHIC, RCLASS_BLACK_BELT };
// Ruins are Ruin Maniacs' country, as Route 111's desert is, with Psychics
// drawn to old things and now and then a Hiker or a Hex Maniac.
static const u8 sRuinsClasses[] = { RCLASS_RUIN_MANIAC, RCLASS_RUIN_MANIAC, RCLASS_PSYCHIC, RCLASS_RUIN_MANIAC, RCLASS_HIKER, RCLASS_HEX_MANIAC };
// (v1.1 peaks) The desert tower: Hikers and Ruin Maniacs come for its
// stone, campers and picnickers out of the sandstorm.
static const u8 sMirageClasses[] = { RCLASS_HIKER, RCLASS_RUIN_MANIAC, RCLASS_CAMPER, RCLASS_PICNICKER, RCLASS_RUIN_MANIAC, RCLASS_HIKER };
// A wreck has the Abandoned Ship's: Sailors, Tubers and a Beauty about the
// cabins, a Gentleman, and a Hex Maniac after its ghosts; a power plant
// Guitarists for the electric hum, a Ninja Boy and a Cool Trainer.
static const u8 sShipClasses[] = { RCLASS_SAILOR, RCLASS_SAILOR, RCLASS_TUBER, RCLASS_BEAUTY, RCLASS_GENTLEMAN, RCLASS_HEX_MANIAC };
static const u8 sPlantClasses[] = { RCLASS_GUITARIST, RCLASS_GUITARIST, RCLASS_GUITARIST, RCLASS_NINJA_BOY, RCLASS_COOLTRAINER_M };
// Forests are Bug Catchers' country, as Petalburg Woods and Viridian Forest
// are, with campers, picnickers and kids out exploring.
static const u8 sForestClasses[] = { RCLASS_BUG_CATCHER, RCLASS_BUG_CATCHER, RCLASS_BUG_CATCHER, RCLASS_CAMPER,
                                     RCLASS_PICNICKER, RCLASS_LASS, RCLASS_YOUNGSTER, RCLASS_BIRD_KEEPER };
// Victory Road, its floors too, is mostly veterans, as Emerald's is: Cool
// Trainers and Experts, with Black Belts, Battle Girls and a Hiker or two.
static const u8 sVictoryClasses[] = { RCLASS_COOLTRAINER_M, RCLASS_COOLTRAINER_F, RCLASS_EXPERT, RCLASS_COOLTRAINER_M,
                                      RCLASS_COOLTRAINER_F, RCLASS_BLACK_BELT, RCLASS_BATTLE_GIRL, RCLASS_HIKER };
// A volcano has Jagged Pass's and Mt. Chimney's hikers and fighters, an ice
// cave hikers, fighters and an old hand braving the cold: no Ruin Maniacs
// away from old stones.
static const u8 sVolcanoClasses[] = { RCLASS_HIKER, RCLASS_HIKER, RCLASS_BLACK_BELT, RCLASS_BATTLE_GIRL };
static const u8 sIceClasses[] = { RCLASS_HIKER, RCLASS_BLACK_BELT, RCLASS_BATTLE_GIRL, RCLASS_EXPERT };

// The dungeon a trainer slot belongs to, or -1.
s32 RogueGen_SlotDungeon(const struct RogueRegion *r, u32 slot)
{
    u32 i;
    for (i = 0; i < r->nDungeons; i++)
    {
        const struct RogueDungeon *d = &r->dungeons[i];
        if (d->nTrainers && slot >= d->trainerBase && slot < (u32)d->trainerBase + d->nTrainers)
            return i;
    }
    return -1;
}

// The place a route or cave trainer slot stands in, or 0xFF (a dungeon's,
// a gym's or a story slot).
u8 RogueGen_SlotPlace(const struct RogueRegion *r, u32 slot)
{
    u32 i;
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        if (p->nTrainers && slot >= p->trainerBase && slot < (u32)p->trainerBase + p->nTrainers)
            return i;
    }
    return 0xFF;
}

// The kind of dungeon a trainer stands in: on its floors, or in the cave
// whose floors it is. RDUN_COUNT for none.
u8 RogueGen_SlotDungeonKind(const struct RogueRegion *r, u32 slot)
{
    s32 d = RogueGen_SlotDungeon(r, slot);
    return RogueGen_DungeonKindAt(r, d >= 0 ? 0xFF : RogueGen_SlotPlace(r, slot), d);
}

// The same for a trainer whose place (0xFF for none) and dungeon (-1) are known.
u8 RogueGen_DungeonKindAt(const struct RogueRegion *r, u8 place, s32 d)
{
    if (d >= 0)
        return r->dungeons[d].kind;
    if (place != 0xFF)
    {
        d = RogueGen_DungeonAt(r, place);
        if (r->places[place].kind == RPLACE_CAVE && d >= 0 && r->dungeons[d].host == place)
            return r->dungeons[d].kind;
    }
    return RDUN_COUNT;
}

// The dungeon whose master (RBOSS_MASTER) a trainer slot is, or -1.
s32 RogueGen_MasterDungeon(const struct RogueRegion *r, u32 slot)
{
    s32 d = RogueGen_SlotDungeon(r, slot);
    if (d < 0 || r->dungeons[d].boss != RBOSS_MASTER
     || slot != (u32)r->dungeons[d].trainerBase + r->dungeons[d].nTrainers - 1)
        return -1;
    return d;
}

// Which villain a slot is (RVROLE_*): grunts in a hideout and on the route
// before its town, the admins and the leaders (RogueGen_SlotTeam).
u8 RogueGen_VillainRole(const struct RogueRegion *r, u32 slot)
{
    struct RogueTeams tm;
    RogueGen_Teams(r, &tm);
    return RogueGen_VillainRoleAt(r, slot, &tm);
}

// RogueGen_VillainRole with the teams (RogueGen_Teams) worked out already,
// for a pass over many slots.
u8 RogueGen_VillainRoleAt(const struct RogueRegion *r, u32 slot, const struct RogueTeams *tm)
{
    u8 role;
    bool8 clash;
    RogueGen_SlotTeam(r, tm, slot, &role, &clash);
    return role;
}

// The class of a trainer slot (RCLASS_*, an index of gRogueGenClasses), or
// ROGUE_CLASS_VILLAIN for the villain team's people (RogueGen_VillainRole).
// Route trainers suit their route: only the classes allowed that far along,
// and water trainers only by the water. Gym trainers share their leader's
// type.
u8 RogueGen_TrainerClass(const struct RogueRegion *r, u32 slot)
{
    if (RogueGen_VillainRole(r, slot) != RVROLE_NONE)
        return ROGUE_CLASS_VILLAIN;
    return RogueGen_TrainerClassAt(r, slot, RogueGen_SlotPlace(r, slot), RogueGen_SlotDungeon(r, slot));
}

// A gym trainer's class: one of the leader's type (type), drawn per slot.
u8 RogueGen_GymTrainerClass(const struct RogueRegion *r, u32 slot, u8 type)
{
    u32 rng = RogueRng_Hash(r->seed, 5000 + slot);
    u32 pick = RogueRng_Range(&rng, ROGUE_CLASS_PICK), tries;
    for (tries = 0; tries < ROGUE_CLASS_PICK; tries++)
    {
        u32 c = (pick + tries) % ROGUE_CLASS_PICK;
        if (gRogueGenClasses[c].type == type)
            return c;
    }
    // No class of this type: veterans and fighters stand in.
    return RCLASS_BLACK_BELT + (pick & 1) * (ROGUE_CLASS_PICK - 1 - RCLASS_BLACK_BELT);
}

// RogueGen_TrainerClass of a slot that is no villain, whose place (0xFF for
// none) and dungeon (-1) are known, for a pass over many slots.
u8 RogueGen_TrainerClassAt(const struct RogueRegion *r, u32 slot, u8 place, s32 d)
{
    u32 rng = RogueRng_Hash(r->seed, 5000 + slot);
    u32 pick = RogueRng_Range(&rng, ROGUE_CLASS_PICK), tries;
    struct RoguePlace floor;
    const struct RoguePlace *p;
    u8 kind;

    if (slot >= ROGUE_SLOT_GYM_0 && slot < ROGUE_SLOT_LEADER_0)
        return RogueGen_GymTrainerClass(r, slot, RogueGen_GymLeader(r, (slot - ROGUE_SLOT_GYM_0) / ROGUE_GYM_TRAINERS)->type);
    if (d >= 0 && r->dungeons[d].boss == RBOSS_MASTER && slot == (u32)r->dungeons[d].trainerBase + r->dungeons[d].nTrainers - 1)
        return sMasterClass[r->dungeons[d].kind < RDUN_COUNT ? r->dungeons[d].kind : RDUN_CAVE];
    // Where the trainer stands: a place, or a dungeon's floor.
    if (place != 0xFF)
        p = &r->places[place];
    else if (d >= 0)
    {
        RogueGen_FloorPlace(r, d, RogueGen_FloorOfTrainer(r, d, slot), &floor);
        p = &floor;
    }
    else
        p = &r->places[place = 0];
    if (p == &floor)
    {
        switch (r->dungeons[d].kind)
        {
        case RDUN_TOWER: return sTowerClasses[pick % LORE_COUNT_OF(sTowerClasses)];
        case RDUN_RUINS: return sRuinsClasses[pick % LORE_COUNT_OF(sRuinsClasses)];
        case RDUN_MIRAGE: return sMirageClasses[pick % LORE_COUNT_OF(sMirageClasses)];
        case RDUN_SHIP:  return sShipClasses[pick % LORE_COUNT_OF(sShipClasses)];
        case RDUN_PLANT: return sPlantClasses[pick % LORE_COUNT_OF(sPlantClasses)];
        }
    }
    kind = RogueGen_DungeonKindAt(r, place, d);
    if (kind == RDUN_VOLCANO)
        return sVolcanoClasses[pick % LORE_COUNT_OF(sVolcanoClasses)];
    if (kind == RDUN_ICE)
        return sIceClasses[pick % LORE_COUNT_OF(sIceClasses)];
    if (p->kind == RPLACE_CAVE)
    {
        // A dungeon floor's made-up place comes in from its cave.
        u32 cave = p == &floor ? p->caveIn : place;
        if (cave < r->nPlaces && r->places[cave].caveOut == r->summit)
            return sVictoryClasses[pick % LORE_COUNT_OF(sVictoryClasses)];
        return sCaveClasses[pick % LORE_COUNT_OF(sCaveClasses)];
    }
    if (p == &floor && p->kind == RPLACE_ROUTE)
    {
        // One forest in three is a Bug Catchers' gauntlet, as Viridian
        // Forest is.
        if (RogueRng_Hash(r->seed, 13100 + d) % 3 == 0)
            return sForestClasses[0];
        for (tries = 0; tries < LORE_COUNT_OF(sForestClasses); tries++)
        {
            u8 c = sForestClasses[(pick + tries) % LORE_COUNT_OF(sForestClasses)];
            if (gRogueGenClasses[c].firstRoute <= p->tier)
                return c;
        }
        return sForestClasses[0];
    }
    if (p == &floor)
        return 0;
    {
        u32 tier = p->tier;
        // Water trainers only where the map puts them by the water.
        bool8 wantWater = RogueGen_TrainerByWater(r, place, slot - p->trainerBase);
        // (v1.1 sea: a Swimmer out on the water.)
        if (RogueGen_TrainerSwims(r, place, slot - p->trainerBase))
            return RCLASS_SWIMMER_M + (RogueRng_Hash(r->seed, 0x5EA7 + slot) & 1);
        bool8 rocky = (p->type == RROUTE_HILLS || p->type == RROUTE_DESERT);
        bool8 wantRock = rocky && RogueRng_Range(&rng, 100) < 50;
        bool8 victory = (p->type == RROUTE_MOUNTAIN);
        for (tries = 0; tries < 60; tries++)
        {
            u32 ci = RogueRng_Range(&rng, ROGUE_CLASS_PICK);
            const struct RogueGenClass *c = &gRogueGenClasses[ci];
            bool8 water = c->type == RTYPE_WATER;
            if (c->firstRoute > tier)
                continue;
            if (wantWater && !water && tries < 40)
                continue;
            if (!wantWater && water)
                continue;
            if (wantRock && c->talk != RTALK_HIKER && c->talk != RTALK_FIGHTER && c->talk != RTALK_FIRE && tries < 40)
                continue;
            // Veterans belong on the last routes, and rarely on early ones.
            // Victory Road is mostly veterans, hikers and fighters.
            if (c->talk == RTALK_ACE && tier < 6 && tries < 30)
                continue;
            if (victory && tries < 40
             && c->talk != RTALK_ACE && c->talk != RTALK_HIKER && c->talk != RTALK_FIGHTER && c->talk != RTALK_MYSTIC)
                continue;
            return ci;
        }
        return 0;
    }
}
