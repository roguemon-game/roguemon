// RUN 0's region (include/rogue_run0_region.h): the plan changed by hand after it
// is built from ROGUE_RUN0_SEED, and each map changed after it is made.
// Included at the end of rogue_gen.c. Nothing here runs for any other seed,
// so normal regions do not change (nonroute_diff.py and a gen_host compare
// over many seeds prove it).
//
// The plan (Run0Plan): the eight custom leaders (RogueGen_RegionLeaders),
// towns named PLACE 14, a side route that loops into itself (leave east,
// enter from the west), a side route that is a copy of Route 101, and the
// meadow tileset for the act 4 route Littleroot's houses are copied down.
// Everything else is found again from the plan whenever it is needed (the
// Run0* finders below), so RUN 0 keeps nothing in memory.
//
// Each map (Run0Finish), after it is made, by the act the story is in
// (RogueGen_Run0Act): the story's hooks, then the corruption. Corruption
// only changes what blocks look like on blocks nobody can walk on (and
// blacks out Victory Road's edges), so every way stays as it was made and
// check_seeds.py still walks every map.

#include "rogue_run0_region.h"
#include "data/rogue_run0_cuts.h"

// Black: General's metatile 0x1E3, an empty slot no map uses, set to the
// covered layer type so nothing is drawn under it either (an empty normal
// metatile shows the engine's filler tile) and the black backdrop shows. Solid.
#define RUN0_BLACK_METATILE 0x1E3
#define RUN0_BLACK    (0x0400 | RUN0_BLACK_METATILE)
#define RUN0_SOLID(b) (((b) >> 10) & 3)
#define RUN0_GRASS    0x3001

#ifdef ROGUE_HOST
// gen_host: ROGUE_RUN0_ACT, ROGUE_RUN0_VISIT and ROGUE_RUN0_STATE stand in
// for the game's state.
u8 RogueGen_Run0Act(void)
{
    const char *s = getenv("ROGUE_RUN0_ACT");
    return s ? atoi(s) : 1;
}

u8 RogueGen_Run0Visit(u8 which)
{
    const char *s = getenv("ROGUE_RUN0_VISIT");
    (void)which;
    return s ? atoi(s) : 0;
}

u16 RogueGen_Run0State(void)
{
    const char *s = getenv("ROGUE_RUN0_STATE");
    return s ? atoi(s) : 0;
}

bool8 RogueGen_KeptRegionName(u32 seed, char *out)
{
    const char *s = getenv("ROGUE_KEPT_REGION"), *name = getenv("ROGUE_KEPT_NAME");
    if (s == NULL || (u32)atoi(s) != seed)
        return FALSE;
    AppendAscii(out, name != NULL ? name : "BRENDAN");
    return TRUE;
}
#endif

static u8 Run0Act(void)
{
    u8 a = RogueGen_Run0Act();
    return a == 0 ? 1 : (a > RUN0_ACT_MAX ? RUN0_ACT_MAX : a);
}

// The act a place of the road belongs to: two gyms an act, Victory Road and
// the League in the fourth.
static u8 Run0ActOf(const struct RoguePlace *p)
{
    return p->badges >= 6 ? 4 : 1 + p->badges / 2;
}

static u32 Run0Hash(const struct RogueRegion *r, u32 salt)
{
    return RogueRng_Hash(r->seed, 77000 + salt);
}

// ---------------------------------------------------------------------------
// Finding RUN 0's places in the plan

// RUN 0's region is fixed, so the story's homes are chosen by hand for it
// (gen_host --run0 lists every town's homes in run0.txt): the island doors
// and the house hooks, a town and its house each. A pick that is not an
// ordinary home of a town (the generator changed) falls back to the last
// homes of the act's towns (Run0TownHouses).
static const struct { u8 what, place, house; } sRun0Homes[] =
{
    { RUN0_HOUSE_ISLAND + RUN0_ISLAND_DEWFORD,      5, 3 }, // DUSKBURY CITY (act 1)
    { RUN0_HOUSE_ISLAND + RUN0_ISLAND_LITTLEROOT, 15, 0 }, // MAPLEVALE CITY (act 2; its gift home)
    { RUN0_HOUSE_ISLAND + RUN0_ISLAND_FALLARBOR,  17, 1 }, // THORNVALE CITY (act 3)
    { RUN0_HOUSE_ISLAND + RUN0_ISLAND_PYRE,       24, 0 }, // HAVENBURY CITY (act 4)
    { RUN0_HOOK_BELIEVER_0,                       12, 1 }, // FROSTBURY CITY
    { RUN0_HOOK_BELIEVER_1,                       7, 3 },  // FERNPORT TOWN
    { RUN0_HOOK_BELIEVER_2,                       7, 2 },
    { RUN0_HOOK_LIBRARY,                          17, 2 }, // THORNVALE CITY (its move tutor's home)
    { RUN0_HOOK_TERRY_HOUSE,                      21, 1 }, // SILVERCREST CITY (act 3)
};

static s32 Run0AuthoredHome(const struct RogueRegion *r, s32 what, u8 *house)
{
    u32 i;
    for (i = 0; i < sizeof(sRun0Homes) / sizeof(sRun0Homes[0]); i++)
    {
        if (sRun0Homes[i].what != what || sRun0Homes[i].place >= r->nPlaces
         || r->places[sRun0Homes[i].place].kind != RPLACE_TOWN || r->places[sRun0Homes[i].place].type == RTOWN_START
         || r->places[sRun0Homes[i].place].type == RTOWN_SUMMIT)
            continue;
        if (house != NULL)
            *house = sRun0Homes[i].house;
        return sRun0Homes[i].place;
    }
    return -1;
}

// The town laid out again on each entry: a village of act 3 on the road
// that holds no dungeon's door.
s32 RogueGen_Run0RelayTown(const struct RogueRegion *r)
{
    int i, pass;
    if (!RogueGen_IsRun0(r))
        return -1;
    // An act 3 village, else act 2's, an act 3 gym city, act 4's village or
    // gym city: not one holding a home chosen for the story (its homes would
    // move with the layout).
    static const u8 acts[] = { 3, 2, 3, 4, 4 }, types[] = { RTOWN_VILLAGE, RTOWN_VILLAGE, RTOWN_GYM, RTOWN_VILLAGE, RTOWN_GYM };
    for (pass = 0; pass < sizeof(acts); pass++)
    {
        for (i = 0; i < r->nPlaces; i++)
        {
            const struct RoguePlace *p = &r->places[i];
            s32 d = RogueGen_DungeonAt(r, i);
            u32 k;
            bool8 homes = FALSE;
            for (k = 0; k < sizeof(sRun0Homes) / sizeof(sRun0Homes[0]); k++)
                homes |= sRun0Homes[k].place == i;
            if (p->kind != RPLACE_TOWN || p->side || Run0ActOf(p) != acts[pass] || p->type != types[pass] || homes)
                continue;
            if (d >= 0 && (r->dungeons[d].host == i || r->dungeons[d].exit == i))
                continue;
            return i;
        }
    }
    return -1;
}

// A town that can take the story's houses: not the first town, the summit
// or the one laid out again.
static bool8 Run0HookTown(const struct RogueRegion *r, int i)
{
    const struct RoguePlace *p = &r->places[i];
    return p->kind == RPLACE_TOWN && p->type != RTOWN_START && p->type != RTOWN_SUMMIT
        && i != RogueGen_Run0RelayTown(r);
}

// The towns of an act (road order: places are numbered along the road
// first), into out; returns how many.
static u32 Run0ActTowns(const struct RogueRegion *r, u8 act, u8 *out)
{
    u32 n = 0;
    int i;
    for (i = 0; i < r->nPlaces; i++)
        if (Run0HookTown(r, i) && Run0ActOf(&r->places[i]) == act)
            out[n++] = i;
    return n;
}

// The routes of an act on the main road that signs can stand beside.
static u32 Run0ActRoutes(const struct RogueRegion *r, u8 act, u8 *out)
{
    u32 n = 0;
    int i;
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        if (p->kind == RPLACE_ROUTE && !p->side && p->type != RROUTE_SEA && p->type != RROUTE_MOUNTAIN
         && Run0ActOf(p) == act)
            out[n++] = i;
    }
    return n;
}

// A dead-end side route on land whose way in comes from the north or the
// south, free of dungeons (before Run0Plan changes it): the k-th one whose
// first badge falls in [lo, hi].
static s32 Run0DeadEnd(const struct RogueRegion *r, u8 lo, u8 hi, s32 skip, bool8 dry)
{
    int i;
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        if (i == skip || p->kind != RPLACE_ROUTE || !p->side || p->nLinks != 1
         || p->mouth != 0xFF || p->type == RROUTE_SEA || (dry && (p->type == RROUTE_LAKE || p->type == RROUTE_COAST))
         || p->badges < lo || p->badges > hi || RogueGen_DungeonAt(r, i) >= 0
         || (p->links[0].dir != RDIR_NORTH && p->links[0].dir != RDIR_SOUTH))
            continue;
        return i;
    }
    return -1;
}

// The side route that runs into itself (a link to itself east and west).
s32 RogueGen_Run0LoopRoute(const struct RogueRegion *r)
{
    int i, k;
    if (!RogueGen_IsRun0(r))
        return -1;
    for (i = 0; i < r->nPlaces; i++)
        for (k = 0; k < r->places[i].nLinks; k++)
            if (r->places[i].links[k].target == i)
                return i;
    return -1;
}

// The copy of Route 101: a route made Route 101's size (no other place is).
s32 RogueGen_Run0CopyRoute(const struct RogueRegion *r)
{
    int i;
    if (!RogueGen_IsRun0(r))
        return -1;
    for (i = 0; i < r->nPlaces; i++)
        if (r->places[i].kind == RPLACE_ROUTE && r->places[i].w == RUN0ROUTE101_W && r->places[i].h == RUN0ROUTE101_H)
            return i;
    return -1;
}

// Act 4: where Petalburg's gym stands in the water (the first place with
// water on the road past the sixth gym, else the sea route), and the route
// Littleroot's houses are copied down.
static s32 Run0GymPlace(const struct RogueRegion *r)
{
    int i;
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        if (p->side || p->badges < 6)
            continue;
        if ((p->kind == RPLACE_ROUTE && (p->type == RROUTE_COAST || p->type == RROUTE_LAKE || p->type == RROUTE_RIVER))
         || (p->kind == RPLACE_TOWN && p->layout == RLAYOUT_HARBOR && p->seaSide != RDIR_NONE))
            return i;
    }
    for (i = 0; i < r->nPlaces; i++)
        if (IsSeaRoute(&r->places[i]))
            return i;
    return -1;
}

// The nth route Littleroot's houses are copied down (two of them).
static s32 Run0HousesRoute(const struct RogueRegion *r, u32 nth)
{
    s32 gym = Run0GymPlace(r);
    int i;
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        if (p->kind == RPLACE_ROUTE && !p->side && p->badges >= 6 && p->type != RROUTE_MOUNTAIN && p->type != RROUTE_SEA
         && i != gym && i != r->victoryRoad && nth-- == 0)
            return i;
    }
    return -1;
}

static bool8 Run0IsHousesRoute(const struct RogueRegion *r, u8 place)
{
    return (s32)place == Run0HousesRoute(r, 0) || (s32)place == Run0HousesRoute(r, 1);
}

// The cave the League is reached from (Victory Road's end).
static s32 Run0LastCave(const struct RogueRegion *r)
{
    int i;
    for (i = 0; i < r->nPlaces; i++)
        if (r->places[i].kind == RPLACE_CAVE && r->places[i].caveOut == r->summit)
            return i;
    return -1;
}

s32 RogueGen_Run0IslandTown(const struct RogueRegion *r, u8 island)
{
    u8 towns[ROGUE_MAX_PLACES];
    u32 n;
    s32 hand;
    if (!RogueGen_IsRun0(r) || island >= RUN0_ISLAND_COUNT)
        return -1;
    if ((hand = Run0AuthoredHome(r, RUN0_HOUSE_ISLAND + island, NULL)) >= 0)
        return hand;
    n = Run0ActTowns(r, island + 1, towns);
    return n ? towns[0] : -1;
}

s32 RogueGen_Run0Graveyard(const struct RogueRegion *r)
{
    int i;
    if (!RogueGen_IsRun0(r))
        return -1;
    for (i = 0; i < r->nDungeons; i++)
        if (r->dungeons[i].kind == RDUN_TOWER)
            return i;
    return -1;
}

// The place of a hook (the town of a house, the map of a thing or a sign,
// the tower's town for the graveyard), or -1 if the region has none.
s32 RogueGen_Run0HookPlace(const struct RogueRegion *r, u8 hook)
{
    u8 list[ROGUE_MAX_PLACES];
    u32 n;
    int i;
    if (!RogueGen_IsRun0(r))
        return -1;
    if (RUN0_HOOK_IS_HOUSE(hook) && (i = Run0AuthoredHome(r, hook, NULL)) >= 0)
        return i;
    switch (hook)
    {
    case RUN0_HOOK_BELIEVER_0:
    case RUN0_HOOK_BELIEVER_1:
    case RUN0_HOOK_BELIEVER_2:
        // Act 2's towns, from the second, so the island's door is not
        // crowded by them.
        n = Run0ActTowns(r, 2, list);
        return n ? list[(hook - RUN0_HOOK_BELIEVER_0 + 1) % n] : -1;
    case RUN0_HOOK_LIBRARY:
        n = Run0ActTowns(r, 3, list);
        return n ? list[n > 1 ? 1 : 0] : -1;
    case RUN0_HOOK_TERRY_HOUSE:
        // A quiet hamlet off the road if there is one about then.
        for (i = 0; i < r->nPlaces; i++)
            if (r->places[i].kind == RPLACE_TOWN && r->places[i].type == RTOWN_HAMLET
             && r->places[i].badges >= 3 && r->places[i].badges <= 6)
                return i;
        n = Run0ActTowns(r, 3, list);
        return n ? list[n - 1] : -1;
    case RUN0_HOOK_GIFT_BOX:
        // A gym city of act 3 (it has a POKeMON CENTER).
        n = Run0ActTowns(r, 3, list);
        for (i = 0; i < (int)n; i++)
            if (r->places[list[i]].type == RTOWN_GYM)
                return list[i];
        return n ? list[0] : -1;
    case RUN0_HOOK_CHAMPION_TERRY:
        return Run0LastCave(r);
    case RUN0_HOOK_GRAVEYARD:
        i = RogueGen_Run0Graveyard(r);
        return i >= 0 ? r->dungeons[i].host : -1;
    }
    if (RUN0_HOOK_IS_SIGN(hook))
    {
        u8 k = hook - RUN0_HOOK_PROMPT_0;
        n = Run0ActRoutes(r, 1 + k / 2, list);
        return n ? list[(k & 1) * (n - 1)] : -1;
    }
    return -1;
}

// What each ordinary home of a town map is for, into out (one per house:
// -1, a hook, or RUN0_HOUSE_ISLAND + an island). The island's door first,
// then the hooks, each taking the last chat home not yet taken (so no gift
// house, move tutor or Name Rater moves).
static void Run0TownHouses(const struct RogueRegion *r, const struct RogueMap *m, s8 *out)
{
    s8 want[RUN0_HOOK_COUNT + 1];
    u32 nWant = 0, k;
    int house;
    for (k = 0; k < ROGUE_MAX_HOUSES; k++)
        out[k] = -1;
    if (!RogueGen_IsRun0(r) || m->floor || r->places[m->place].kind != RPLACE_TOWN)
        return;
    for (k = 0; k < RUN0_ISLAND_COUNT; k++)
        if (RogueGen_Run0IslandTown(r, k) == m->place)
            want[nWant++] = RUN0_HOUSE_ISLAND + k;
    for (k = 0; k < RUN0_HOOK_COUNT; k++)
        if (RUN0_HOOK_IS_HOUSE(k) && RogueGen_Run0HookPlace(r, k) == m->place)
            want[nWant++] = k;
    // The homes chosen by hand first, where they are homes.
    for (k = 0; k < nWant; k++)
    {
        u8 h;
        if (Run0AuthoredHome(r, want[k], &h) == m->place && h < m->nHouses && m->interiors[h] == RINT_HOME && out[h] < 0)
        {
            out[h] = want[k];
            want[k] = -1;
        }
    }
    // Chat homes first; a small town (a hamlet) gives up its other homes.
    k = 0;
    while (k < nWant && want[k] < 0)
        k++;
    for (house = m->nHouses - 1; house >= 0 && k < nWant; house--)
        if (m->interiors[house] == RINT_HOME && out[house] < 0
         && RogueGen_HomeRole(r, m->place, m->interiors, m->nHouses, house) == RHOME_CHAT)
            for (out[house] = want[k++]; k < nWant && want[k] < 0; k++)
                ;
    for (house = m->nHouses - 1; house >= 0 && k < nWant; house--)
        if (m->interiors[house] == RINT_HOME && out[house] < 0)
            for (out[house] = want[k++]; k < nWant && want[k] < 0; k++)
                ;
}

s32 RogueGen_Run0HouseHook(const struct RogueRegion *r, const struct RogueMap *m, u8 house)
{
    s8 out[ROGUE_MAX_HOUSES];
    if (house >= ROGUE_MAX_HOUSES)
        return -1;
    Run0TownHouses(r, m, out);
    return out[house];
}

// ---------------------------------------------------------------------------
// The plan

static void Run0Relink(struct RogueRegion *r, int place, u8 pos)
{
    struct RoguePlace *p = &r->places[place];
    struct RogueLink *l = &p->links[0];
    struct RogueLink *back = (struct RogueLink *)RogueGen_FindLink(&r->places[l->target], place);
    l->pos = pos;
    if (back != NULL)
    {
        l->offset = (s16)l->pos - (s16)back->pos;
        back->offset = (s16)back->pos - (s16)l->pos;
    }
}

// Real places the name banks can spell (HOLLY and WOOD), checked on RUN 0's
// fixed region only, so every other region keeps the names it always had.
static const char *const sRun0RealPlaces[] = {
    "HOLLYWOOD", "BRIGHTON", "ASHTON", "ASHFORD", "GLENDALE", "MAPLEWOOD", "STONEHAVEN", "OAKTON", "ROSEDALE",
    "ELMWOOD", "NORTHWOOD", "NORTHFIELD", "OAKFIELD", "OAKDALE", "BIRCHWOOD", "ROSEWOOD", "SILVERDALE", "ASHBURY",
    "STONEGATE", "NORTHGATE", "GLENWOOD", "PINEWOOD", "OAKWOOD", "ELMFIELD", "FERNDALE", "IRONTON",
};

// The stem of a town's name (up to its first space) is a real place.
static bool8 Run0RealPlace(const u8 *name)
{
    u32 i, k;
    for (i = 0; i < (sizeof(sRun0RealPlaces) / sizeof(sRun0RealPlaces[0])); i++)
    {
        const char *real = sRun0RealPlaces[i];
        for (k = 0; real[k] && name[k] == (u8)real[k]; k++)
            ;
        if (!real[k] && (name[k] == ' ' || name[k] == 0))
            return TRUE;
    }
    return FALSE;
}

// RUN 0's towns with a real place's name take another from the same banks
// (its suffix kept): HOLLYWOOD SUMMIT was the League's town.
static void Run0RenameRealPlaces(struct RogueRegion *r)
{
    u32 t, i, j, tries;
    for (t = 0; t < r->nTowns; t++)
    {
        u8 *name = r->townNames[t];
        char suffix[ROGUE_NAME_LEN + 1];
        u8 fresh[ROGUE_NAME_LEN + 2];
        int n, cut;
        if (!Run0RealPlace(name))
            continue;
        for (cut = 0; name[cut] && name[cut] != ' '; cut++)
            ;
        for (n = 0; name[cut + n] && n < ROGUE_NAME_LEN; n++)
            suffix[n] = name[cut + n];
        suffix[n] = 0;
        for (tries = 0; tries < 36 * 20; tries++)
        {
            const char *a = sNameStarts[(tries * 7 + t) % (sizeof(sNameStarts) / sizeof(sNameStarts[0]))];
            const char *b = sNameEnds[(tries * 3 + t) % (sizeof(sNameEnds) / sizeof(sNameEnds[0]))];
            bool8 ok = TRUE;
            if (StrLen(a) + StrLen(b) + StrLen(suffix) > ROGUE_NAME_LEN)
                continue;
            n = 0;
            for (i = 0; a[i]; i++)
                fresh[n++] = a[i];
            for (i = 0; b[i]; i++)
                fresh[n++] = b[i];
            for (i = 0; suffix[i]; i++)
                fresh[n++] = suffix[i];
            fresh[n] = 0;
            if (Run0RealPlace(fresh))
                continue;
            for (j = 0; j < r->nTowns && ok; j++)
                ok = j == t || !SameName(fresh, r->townNames[j]);
            if (!ok)
                continue;
            for (i = 0; i <= (u32)n; i++)
                name[i] = fresh[i];
            break;
        }
    }
}

static void Run0Plan(struct RogueRegion *r)
{
    s32 loop, copy, road;
    u32 named = 0, k;
    int i;

    Run0RenameRealPlaces(r);

    // Towns that were never given names: the generator's own numbers. Two,
    // villages first, then gym cities, from act 2.
    for (k = 0; k < 2 * ROGUE_MAX_TOWNS && named < 2; k++)
    {
        int place = r->towns[k % ROGUE_MAX_TOWNS];
        const struct RoguePlace *p = &r->places[place];
        char *s = (char *)r->townNames[k % ROGUE_MAX_TOWNS];
        if (k % ROGUE_MAX_TOWNS >= r->nTowns || memcmp(s, "PLACE ", 6) == 0)
            continue;
        if (k < ROGUE_MAX_TOWNS ? (p->type != RTOWN_VILLAGE && p->type != RTOWN_HAMLET) : p->type != RTOWN_GYM)
            continue;
        if (Run0ActOf(p) < 2 || place == RogueGen_Run0RelayTown(r))
            continue;
        memcpy(s, "PLACE ", 6);
        s[6] = '0' + (place / 10) % 10;
        s[7] = '0' + place % 10;
        s[8] = 0;
        named++;
    }

    // The copy of Route 101: Route 101's size, its way in where Route 101
    // has one (columns 10 and 11, both ends).
    copy = Run0DeadEnd(r, 1, 6, -1, TRUE);
    if (copy >= 0)
    {
        struct RoguePlace *p = &r->places[copy];
        p->w = RUN0ROUTE101_W;
        p->h = RUN0ROUTE101_H;
        // Route 101 has nobody to battle and nothing to find, and neither has
        // its copy (the slots and flags kept for them stay unused).
        p->nTrainers = 0;
        p->nItems = 0;
        p->nBerries = 0;
        Run0Relink(r, copy, 10);
    }

    // The loop: a route whose east edge leads into its own west edge (a
    // dead end first, then one of the road), or else north into south.
    loop = -1;
    for (k = 0; k < 2 && loop < 0; k++)
    {
        u8 a = k ? RDIR_NORTH : RDIR_EAST, b = k ? RDIR_SOUTH : RDIR_WEST;
        for (i = 0; i < r->nPlaces && loop < 0; i++)
        {
            const struct RoguePlace *p = &r->places[i];
            if (p->kind != RPLACE_ROUTE || p->type == RROUTE_SEA || p->type == RROUTE_MOUNTAIN || p->mouth != 0xFF
             || p->badges < 1 || p->badges > 6 || RogueGen_DungeonAt(r, i) >= 0 || i == copy || p->nLinks + 2 > ROGUE_MAX_EXITS
             || LinkOnSide(p, a) != NULL || LinkOnSide(p, b) != NULL || Run0IsHousesRoute(r, i) || i == Run0GymPlace(r)
             || i == r->victoryRoad)
                continue;
            if (Run0DeadEnd(r, 1, 8, copy, FALSE) >= 0 && i != Run0DeadEnd(r, 1, 8, copy, FALSE))
                continue;
            loop = i;
        }
        if (loop >= 0)
        {
            struct RoguePlace *p = &r->places[loop];
            u8 pos = ((k ? p->w : p->h) / 4) * 2;
            u8 d;
            for (d = a; d <= b; d += 2)
            {
                struct RogueLink *l = &p->links[p->nLinks++];
                l->dir = d;
                l->target = loop;
                l->pos = pos;
                l->gate = 0;
                l->offset = 0;
            }
        }
    }

    // Littleroot's houses need Petalburg's tileset, the meadow theme's.
    for (k = 0; k < 2; k++)
        if ((road = Run0HousesRoute(r, k)) >= 0)
            r->places[road].theme = RTHEME_MEADOW;
}

// ---------------------------------------------------------------------------
// One map

static bool8 Run0Taken(const struct RogueMap *m, int x, int y)
{
    int i;
    for (i = 0; i < m->nObjs; i++)
        if (m->objs[i].x == x && m->objs[i].y == y)
            return TRUE;
    for (i = 0; i < m->nWarps; i++)
        if (m->warps[i].x == x && m->warps[i].y == y)
            return TRUE;
    for (i = 0; i < m->nSigns; i++)
        if (m->signs[i].x == x && m->signs[i].y == y)
            return TRUE;
    return FALSE;
}

// Trees or rock as the generator laid them, not a building or a fence.
static bool8 Run0Natural(struct Gen *gen, int x, int y)
{
    u8 c;
    if (x < 0 || y < 0 || x >= gen->m->w || y >= gen->m->h)
        return FALSE;
    c = Cell(gen, x >> 1, y >> 1);
    return c == C_TREE || c == C_ROCK;
}

static u16 Run0Block(const struct RogueMap *m, int x, int y)
{
    if (x < 0 || y < 0 || x >= m->w || y >= m->h)
        return RUN0_BLACK;
    return m->blocks[y * m->w + x];
}

// Ground a person can stand on: walkable, at ground height.
static bool8 Run0Ground(const struct RogueMap *m, int x, int y)
{
    u16 b = Run0Block(m, x, y);
    return RUN0_SOLID(b) == 0 && (b >> 12) == 3;
}

// A free block of ground to stand something on, with at least three of its
// four neighbours walkable, so it narrows no way to nothing.
static bool8 Run0Spot(struct Gen *gen, int x, int y)
{
    const struct RogueMap *m = gen->m;
    int k, open = 0;
    if (x < 3 || y < 3 || x >= m->w - 3 || y >= m->h - 3 || !Run0Ground(m, x, y) || IsOccupied(gen, x, y)
     || Run0Taken(m, x, y) || ByLedgeF(gen, x, y))
        return FALSE;
    for (k = 0; k < 4; k++)
        open += Run0Ground(m, x + sDirDX[k], y + sDirDY[k]) && !Run0Taken(m, x + sDirDX[k], y + sDirDY[k]);
    return open >= 3;
}

// A sign post beside a route's way, a share of the way along (k of 4).
static bool8 Run0SignBesideWalk(struct Gen *gen, u32 k, u8 kind)
{
    const struct RogueMap *m = gen->m;
    int i, d, start;
    if (gen->nWalk < 8)
        return FALSE;
    start = gen->nWalk * (k + 1) / 5;
    for (i = 0; i < gen->nWalk; i++)
    {
        int w = gen->walk[(start + i) % gen->nWalk], wx = PX(w), wy = PY(w);
        for (d = 0; d < 4; d++)
        {
            int x = wx + sDirDX[d] * 2, y = wy + sDirDY[d] * 2;
            if (!Run0Spot(gen, x, y) || OnWalk(gen, x, y) || !Run0Ground(m, wx + sDirDX[d], wy + sDirDY[d]))
                continue;
            gen->m->blocks[y * m->w + x] = B_SIGN | 0x3000;
            AddSign(gen, x, y, kind);
            return TRUE;
        }
    }
    return FALSE;
}

// The rectangle of a building whose door is at (dx, dy): the first of the
// stamp shapes whose every block but the door is solid.
static bool8 Run0BuildingAt(const struct RogueMap *m, int dx, int dy, int *x0, int *y0, int *w, int *h)
{
    static const u8 shapes[][4] = { { 4, 4, 1, 3 }, { 5, 5, 3, 4 }, { 5, 5, 2, 4 }, { 5, 5, 1, 4 }, { 4, 4, 2, 3 }, { 6, 5, 3, 4 } };
    u32 s;
    for (s = 0; s < sizeof(shapes) / sizeof(shapes[0]); s++)
    {
        int bx = dx - shapes[s][2], by = dy - shapes[s][3], x, y;
        bool8 ok = bx >= 0 && by >= 0 && bx + shapes[s][0] <= m->w && by + shapes[s][1] <= m->h;
        for (y = 0; ok && y < shapes[s][1]; y++)
            for (x = 0; ok && x < shapes[s][0]; x++)
                if (!(bx + x == dx && by + y == dy) && !RUN0_SOLID(Run0Block(m, bx + x, by + y)))
                    ok = FALSE;
        if (ok)
        {
            *x0 = bx;
            *y0 = by;
            *w = shapes[s][0];
            *h = shapes[s][1];
            return TRUE;
        }
    }
    return FALSE;
}

static s32 Run0WarpOf(const struct RogueMap *m, u8 kind)
{
    int i;
    for (i = 0; i < m->nWarps; i++)
        if (m->warps[i].kind == kind)
            return i;
    return -1;
}

// A rectangle every block of which is solid, holds nothing, and lies clear
// of the map's edge and of the rectangles in avoid.
static bool8 Run0SolidRect(struct Gen *gen, int x0, int y0, int w, int h, const s8 (*avoid)[4], u32 nAvoid)
{
    const struct RogueMap *m = gen->m;
    int x, y;
    u32 k;
    if (x0 < 1 || y0 < 1 || x0 + w > m->w - 1 || y0 + h > m->h - 1)
        return FALSE;
    for (k = 0; k < nAvoid; k++)
        if (x0 < avoid[k][0] + avoid[k][2] && avoid[k][0] < x0 + w && y0 < avoid[k][1] + avoid[k][3] && avoid[k][1] < y0 + h)
            return FALSE;
    for (y = y0; y < y0 + h; y++)
    {
        YIELD();
        for (x = x0; x < x0 + w; x++)
            if (!RUN0_SOLID(m->blocks[y * m->w + x]) || Run0Taken(m, x, y) || !Run0Natural(gen, x, y))
                return FALSE;
    }
    return TRUE;
}

// Copies blocks (a stamp of w x h, or a rectangle of the map itself) onto
// the map at (x0, y0), every block solid. door, if not negative, is the
// stamp index of a door, drawn as the wall beside it.
static void Run0Paste(struct RogueMap *m, int x0, int y0, int w, int h, const u16 *src, int srcStride, int door)
{
    int x, y;
    for (y = 0; y < h; y++)
    {
        YIELD();
        for (x = 0; x < w; x++)
        {
            int i = y * srcStride + x;
            u16 b = src[i];
            if (i == door)
                b = src[i - 1];
            m->blocks[(y0 + y) * m->w + x0 + x] = (b & ~0x0C00) | 0x0400;
        }
    }
}

// Marks in gen->cover (free once a map is drawn) every block reached on
// foot from (sx, sy), walking on blocks nobody stands on; returns how many.
static u32 Run0Flood(struct Gen *gen, int sx, int sy, u8 mark)
{
    const struct RogueMap *m = gen->m;
    u32 head = 0, tail = 0, n = 0;
    int k;
    if (RUN0_SOLID(m->blocks[sy * m->w + sx]))
        return 0;
    gen->cover[sy * m->w + sx] = mark;
    sBfsQueue[tail++] = PACK(sx, sy);
    while (head < tail)
    {
        int cur = sBfsQueue[head++ & (BFS_RING - 1)], cx = PX(cur), cy = PY(cur);
        YIELD();
        n++;
        for (k = 0; k < 4; k++)
        {
            int nx = cx + sDirDX[k], ny = cy + sDirDY[k], ni = ny * m->w + nx;
            if (nx < 0 || ny < 0 || nx >= m->w || ny >= m->h || gen->cover[ni] == mark || RUN0_SOLID(m->blocks[ni]))
                continue;
            if (tail - head >= BFS_RING)
                return n;
            gen->cover[ni] = mark;
            sBfsQueue[tail++ & (BFS_RING - 1)] = PACK(nx, ny);
        }
    }
    return n;
}

// TRUE if every block reached before (mark 1) that matters is reached now
// (mark 2): people, things, doors and the openings along the edges.
static bool8 Run0StillReached(struct Gen *gen, int sx, int sy)
{
    const struct RogueMap *m = gen->m;
    int i, x, y;
    Run0Flood(gen, sx, sy, 2);
    for (i = 0; i < m->nObjs; i++)
        if (gen->cover[m->objs[i].y * m->w + m->objs[i].x] == 1)
            return FALSE;
    for (i = 0; i < m->nWarps; i++)
        if (gen->cover[m->warps[i].y * m->w + m->warps[i].x] == 1)
            return FALSE;
    for (y = 0; y < m->h; y++)
    {
        YIELD();
        for (x = 0; x < m->w; x += (y == 0 || y == m->h - 1) ? 1 : m->w - 1)
            if (gen->cover[y * m->w + x] == 1)
                return FALSE;
    }
    return TRUE;
}

// A rectangle a copied building may stand on: trees, rock or open ground
// (not a road, a building, a fence, water or anyone's spot), clear of the
// map's edge and of the rectangles in avoid.
static bool8 Run0Lot(struct Gen *gen, int x0, int y0, int w, int h, const s8 (*avoid)[4], u32 nAvoid)
{
    const struct RogueMap *m = gen->m;
    int x, y;
    u32 k;
    if (x0 < 1 || y0 < 1 || x0 + w > m->w - 1 || y0 + h > m->h - 1)
        return FALSE;
    for (k = 0; k < nAvoid; k++)
        if (x0 < avoid[k][0] + avoid[k][2] && avoid[k][0] < x0 + w && y0 < avoid[k][1] + avoid[k][3] && avoid[k][1] < y0 + h)
            return FALSE;
    for (y = y0; y < y0 + h; y++)
    {
        YIELD();
        for (x = x0; x < x0 + w; x++)
        {
            u16 b = m->blocks[y * m->w + x];
            u8 c = Cell(gen, x >> 1, y >> 1);
            if (Run0Taken(m, x, y) || (c != C_TREE && c != C_ROCK && c != C_OPEN) || ByLedgeF(gen, x, y)
             || (!RUN0_SOLID(b) && (b >> 12) != 3))
                return FALSE;
        }
    }
    return TRUE;
}

// Copies of a building of the map in the same rows (or a row or two off),
// as near it as they fit, each kept only if everyone and every door can
// still be reached: three POKeMON CENTERS in a row, or homes whose copies
// have no door.
static void Run0RepeatBuilding(struct Gen *gen, int dx, int dy, u32 copies, bool8 doorless)
{
    struct RogueMap *m = gen->m;
    u16 stamp[6 * 5], under[6 * 5];
    s8 avoid[4][4];
    u32 nAvoid = 1, placed = 0;
    int x0, y0, w, h, x, y, dist, off;
    if (!Run0BuildingAt(m, dx, dy, &x0, &y0, &w, &h) || w * h > 30 || dy + 1 >= m->h)
        return;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            stamp[y * w + x] = m->blocks[(y0 + y) * m->w + x0 + x];
    avoid[0][0] = x0 - 1;
    avoid[0][1] = y0 - 1;
    avoid[0][2] = w + 2;
    avoid[0][3] = h + 2;
    for (off = 0; off <= 2 && placed < copies; off++)
    {
        for (dist = w; dist < m->w && placed < copies; dist++)
        {
            int side;
            for (side = 0; side < 4 && placed < copies; side++)
            {
                int tx = (side & 1) ? x0 + dist : x0 - dist, ty = y0 + ((side & 2) ? -off : off);
                if ((off == 0 && (side & 2)) || !Run0Lot(gen, tx, ty, w, h, (const s8 (*)[4])avoid, nAvoid))
                    continue;
                memset(gen->cover, 0, m->w * m->h);
                Run0Flood(gen, dx, dy + 1, 1);
                for (y = 0; y < h; y++)
                    for (x = 0; x < w; x++)
                        under[y * w + x] = m->blocks[(ty + y) * m->w + tx + x];
                Run0Paste(m, tx, ty, w, h, stamp, w, doorless ? (dy - y0) * w + (dx - x0) : -1);
                if (!Run0StillReached(gen, dx, dy + 1))
                {
                    for (y = 0; y < h; y++)
                        for (x = 0; x < w; x++)
                            m->blocks[(ty + y) * m->w + tx + x] = under[y * w + x];
                    continue;
                }
                avoid[nAvoid][0] = tx;
                avoid[nAvoid][1] = ty - 1;
                avoid[nAvoid][2] = w;
                avoid[nAvoid][3] = h + 2;
                nAvoid++;
                placed++;
            }
        }
    }
}

// Broken joins: bands of solid blocks (trees, cliffs, walls) drawn from the
// block beside them, so trees stop halfway and cliffs do not meet. Only
// solid blocks change, into solid blocks.
static void Run0BandAt(struct Gen *gen, int cx, int cy, int w, int h, int dx, int dy)
{
    struct RogueMap *m = gen->m;
    int x, y;
    // Copy from the side the band reads from first, so each block takes
    // its neighbour's old look.
    for (y = 0; y < h; y++)
    {
        int ty = dy > 0 ? cy + y : cy + h - 1 - y;
        YIELD();
        for (x = 0; x < w; x++)
        {
            int tx = dx > 0 ? cx + x : cx + w - 1 - x;
            u16 to = Run0Block(m, tx, ty), from = Run0Block(m, tx + dx, ty + dy);
            if (tx < 0 || ty < 0 || tx >= m->w || ty >= m->h || !RUN0_SOLID(to) || !RUN0_SOLID(from)
             || from == RUN0_BLACK || Run0Taken(m, tx, ty) || !Run0Natural(gen, tx, ty)
             || !Run0Natural(gen, tx + dx, ty + dy))
                continue;
            m->blocks[ty * m->w + tx] = (to & ~0x03FF) | (from & 0x03FF);
        }
    }
}

static void Run0BandDir(u32 *rng, int *dx, int *dy)
{
    *dx = *dy = 0;
    switch (RogueRng_Range(rng, 4))
    {
    case 0: *dx = 1; break;
    case 1: *dx = -1; break;
    case 2: *dy = 1; break;
    default: *dy = -1; break;
    }
}

static void Run0BreakJoins(struct Gen *gen, u32 bands, u32 *rng)
{
    struct RogueMap *m = gen->m;
    u32 b;
    for (b = 0; b < bands; b++)
    {
        int tries, cx = 0, cy = 0, w, h, dx, dy;
        bool8 found = FALSE;
        for (tries = 0; tries < 40 && !found; tries++)
        {
            cx = RogueRng_Range(rng, m->w);
            cy = RogueRng_Range(rng, m->h);
            found = RUN0_SOLID(m->blocks[cy * m->w + cx]) && !Run0Taken(m, cx, cy) && Run0Natural(gen, cx, cy);
        }
        if (!found)
            continue;
        w = 3 + RogueRng_Range(rng, 4 + bands);
        h = 2 + RogueRng_Range(rng, 2 + bands / 2);
        Run0BandDir(rng, &dx, &dy);
        Run0BandAt(gen, cx, cy, w, h, dx, dy);
    }
}

// Broken joins where the player walks: a band on the trees or rock right
// beside the way, so acts 1 and 2 show it on the main road (the bands
// anywhere on the map were mostly out of sight, and players saw only the
// gray).
static void Run0RoadsideBreaks(struct Gen *gen, u32 count, u32 *rng)
{
    struct RogueMap *m = gen->m;
    u32 n;
    if (gen->nWalk < 4)
        return;
    for (n = 0; n < count; n++)
    {
        int tries, dx, dy;
        for (tries = 0; tries < 60; tries++)
        {
            int i = gen->nWalk / 6 + RogueRng_Range(rng, gen->nWalk - gen->nWalk / 3);
            int side = RogueRng_Range(rng, 4), out;
            int wx = PX(gen->walk[i]), wy = PY(gen->walk[i]), sx = wx, sy = wy;
            // The first solid block out from the way on that side.
            for (out = 1; out <= 3; out++)
            {
                sx = wx + sDirDX[side] * out;
                sy = wy + sDirDY[side] * out;
                if (sx < 0 || sy < 0 || sx >= m->w || sy >= m->h || RUN0_SOLID(m->blocks[sy * m->w + sx]))
                    break;
            }
            if (sx < 1 || sy < 1 || sx >= m->w - 1 || sy >= m->h - 1 || !RUN0_SOLID(m->blocks[sy * m->w + sx])
             || Run0Taken(m, sx, sy) || !Run0Natural(gen, sx, sy))
                continue;
            Run0BandDir(rng, &dx, &dy);
            // Along the way, four deep, across the grain of the trees (a
            // band read from the block beside it along the way: a tree's
            // halves no longer meet).
            if (sDirDX[side] != 0)
                Run0BandAt(gen, sx - (sDirDX[side] < 0 ? 3 : 0), sy - 3, 4, 6 + RogueRng_Range(rng, 4), 0, RogueRng_Range(rng, 2) ? 1 : -1);
            else
                Run0BandAt(gen, sx - 3, sy - (sDirDY[side] < 0 ? 3 : 0), 6 + RogueRng_Range(rng, 4), 4, RogueRng_Range(rng, 2) ? 1 : -1, 0);
            break;
        }
    }
}

// A tear: one row or column right across the map whose solid blocks are
// drawn from the next one over, so every tree and cliff it crosses breaks
// along a straight line, as a screen does when it stops drawing.
static void Run0Tear(struct Gen *gen, u32 *rng)
{
    struct RogueMap *m = gen->m;
    bool8 across = RogueRng_Range(rng, 2);
    int len = across ? m->w : m->h, at = 2 + RogueRng_Range(rng, (across ? m->h : m->w) - 4), i;
    for (i = 0; i < len; i++)
    {
        int x = across ? i : at, y = across ? at : i;
        u16 to = m->blocks[y * m->w + x], from = Run0Block(m, x + !across, y + across);
        YIELD();
        if (RUN0_SOLID(to) && RUN0_SOLID(from) && from != RUN0_BLACK && !Run0Taken(m, x, y)
         && Run0Natural(gen, x, y) && Run0Natural(gen, x + !across, y + across))
            m->blocks[y * m->w + x] = (to & ~0x03FF) | (from & 0x03FF);
    }
}

// Copies of a real stamp (Littleroot's houses, Petalburg's gym) standing
// where every block under them is solid (or, for the gym, water), as near
// the way as they fit.
// Littleroot's two houses, copied down the route: on trees, grass or flowers
// beside the way (not on it, nor on water, nor where anyone stands), each
// kept only if every person, thing, door and opening can still be reached.
static u32 Run0CutHouses(struct Gen *gen, u32 want)
{
    struct RogueMap *m = gen->m;
    s8 avoid[12][4];
    u16 under[25];
    u32 n = 0;
    int i, x0, y0, x, y;
    if (want > 12)
        want = 12;
    if (gen->nWalk < 2)
        return 0;
    for (i = gen->nWalk / 12; i < gen->nWalk && n < want; i += 2)
    {
        int wx = PX(gen->walk[i]), wy = PY(gen->walk[i]), side;
        for (side = 0; side < 4 * 3 && n < want; side++)
        {
            bool8 ok = TRUE;
            int gap = 2 + side / 4, d = side & 3;
            // Beside the way, a block of grass or more between.
            x0 = sDirDX[d] ? wx + (sDirDX[d] > 0 ? gap : -4 - gap) : wx - 2;
            y0 = sDirDY[d] ? wy + (sDirDY[d] > 0 ? gap : -4 - gap) : wy - 2;
            if (x0 < 1 || y0 < 1 || x0 + 5 > m->w - 1 || y0 + 5 > m->h - 1)
                continue;
            for (y = y0 - 1; ok && y <= y0 + 5; y++)
                for (x = x0 - 1; ok && x <= x0 + 5; x++)
                {
                    u16 b = Run0Block(m, x, y);
                    bool8 inside = x >= x0 && x < x0 + 5 && y >= y0 && y < y0 + 5;
                    if (OnWalk(gen, x, y) || Run0Taken(m, x, y)
                     || (x >= 0 && y >= 0 && x < m->w && y < m->h && (Cell(gen, x >> 1, y >> 1) == C_BLDG || Cell(gen, x >> 1, y >> 1) == C_DECOR)))
                        ok = FALSE;
                    else if (inside && !RUN0_SOLID(b) && ((b >> 12) != 3 || ByLedgeF(gen, x, y)))
                        ok = FALSE;
                }
            for (x = 0; ok && x < (int)n; x++)
                if (x0 < avoid[x][0] + avoid[x][2] && avoid[x][0] < x0 + 5 && y0 < avoid[x][1] + avoid[x][3] && avoid[x][1] < y0 + 5)
                    ok = FALSE;
            if (!ok)
                continue;
            memset(gen->cover, 0, m->w * m->h);
            Run0Flood(gen, PX(gen->walk[0]), PY(gen->walk[0]), 1);
            for (y = 0; y < 5; y++)
                for (x = 0; x < 5; x++)
                    under[y * 5 + x] = m->blocks[(y0 + y) * m->w + x0 + x];
            // (Their doors cannot be walked into: Run0Paste keeps every block solid.)
            Run0Paste(m, x0, y0, 5, 5, (n & 1) ? sRun0LittlerootHouseR : sRun0LittlerootHouseL, 5, -1);
            if (!Run0StillReached(gen, PX(gen->walk[0]), PY(gen->walk[0])))
            {
                for (y = 0; y < 5; y++)
                    for (x = 0; x < 5; x++)
                        m->blocks[(y0 + y) * m->w + x0 + x] = under[y * 5 + x];
                continue;
            }
            avoid[n][0] = x0 - 1;
            avoid[n][1] = y0 - 1;
            avoid[n][2] = 7;
            avoid[n][3] = 7;
            n++;
        }
    }
    return n;
}

static bool8 Run0Water(u16 b)
{
    return RUN0_SOLID(b) == 0 && (b >> 12) == 1;
}

static bool8 Run0GymInWater(struct Gen *gen, u32 *rng)
{
    struct RogueMap *m = gen->m;
    int margin, x0, y0, x, y, sx = RogueRng_Range(rng, m->w), sy = RogueRng_Range(rng, m->h), k;
    for (margin = 1; margin >= 0; margin--)
    {
        for (k = 0; k < m->w * m->h; k++)
        {
            bool8 ok = TRUE;
            x0 = (sx + k) % m->w;
            y0 = (sy + k / m->w) % m->h;
            if (x0 - margin < 0 || y0 - margin < 0 || x0 + 6 + margin > m->w || y0 + 5 + margin > m->h)
                continue;
            YIELD();
            for (y = y0 - margin; ok && y < y0 + 5 + margin; y++)
                for (x = x0 - margin; ok && x < x0 + 6 + margin; x++)
                    if (!Run0Water(m->blocks[y * m->w + x]) || Run0Taken(m, x, y))
                        ok = FALSE;
            if (!ok)
                continue;
            Run0Paste(m, x0, y0, 6, 5, sRun0PetalburgGym, 6, -1);
            return TRUE;
        }
    }
    return FALSE;
}

// Victory Road stops being made: blocks far from the way, its doors and its
// people turn to black, further the more often it is entered in act 4,
// until only the way crosses black.
static void Run0ThinVictoryRoad(struct Gen *gen, u8 level)
{
    struct RogueMap *m = gen->m;
    u8 *keep = gen->free;
    int x, y, i, k;
    u32 rng = RogueRng_Hash(gen->r->seed, 77900 + level);
    memset(keep, 0, sizeof(gen->free));
    for (y = 0; y < m->h; y++)
    {
        YIELD();
        for (x = 0; x < m->w; x++)
        {
            int dx, dy;
            bool8 near = FALSE;
            for (dy = -1; dy <= 1 && !near; dy++)
                for (dx = -1; dx <= 1 && !near; dx++)
                    if (x + dx >= 0 && y + dy >= 0 && x + dx < m->w && y + dy < m->h
                     && (OnWalk(gen, x + dx, y + dy) || Run0Taken(m, x + dx, y + dy)))
                        near = TRUE;
            if (near)
            {
                i = y * m->w + x;
                keep[i >> 3] |= 1 << (i & 7);
            }
        }
    }
    for (y = 0; y < m->h; y++)
    {
        YIELD();
        for (x = 0; x < m->w; x++)
        {
            int dist = 6, dx, dy;
            i = y * m->w + x;
            if ((keep[i >> 3] >> (i & 7)) & 1)
                continue;
            for (dy = -5; dy <= 5; dy++)
                for (dx = -5; dx <= 5; dx++)
                {
                    int nx = x + dx, ny = y + dy, d = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
                    if (d < dist && nx >= 0 && ny >= 0 && nx < m->w && ny < m->h)
                    {
                        k = ny * m->w + nx;
                        if ((keep[k >> 3] >> (k & 7)) & 1)
                            dist = d;
                    }
                }
            // Level 1 takes the far edges, level 4 everything but the way
            // (one block less each entry: at 6 - level a second entry
            // hardly showed).
            if (dist + (int)RogueRng_Range(&rng, 2) > 5 - level)
                m->blocks[i] = RUN0_BLACK;
        }
    }
}

// Terry's house deleted: its lot is bare ground, with no door or plate.
static void Run0DeleteHouse(struct Gen *gen, u8 house)
{
    struct RogueMap *m = gen->m;
    s32 wi = Run0WarpOf(m, ROGUE_HOUSE_WARP(house));
    int x0, y0, w, h, x, y, i;
    u16 ground;
    if (wi < 0)
        return;
    ground = Run0Block(m, m->warps[wi].x, m->warps[wi].y + 1);
    if (RUN0_SOLID(ground))
        ground = RUN0_GRASS;
    if (Run0BuildingAt(m, m->warps[wi].x, m->warps[wi].y, &x0, &y0, &w, &h))
        for (y = y0; y < y0 + h; y++)
            for (x = x0; x < x0 + w; x++)
                m->blocks[y * m->w + x] = ground;
    for (i = 0; i < m->nSigns; i++)
    {
        if (m->signs[i].kind == RSIGN_HOUSE && m->signs[i].x >= x0 - 1 && m->signs[i].x <= x0 + w
         && m->signs[i].y >= y0 && m->signs[i].y <= y0 + h)
        {
            m->blocks[m->signs[i].y * m->w + m->signs[i].x] = ground;
            m->signs[i] = m->signs[--m->nSigns];
            i--;
        }
    }
    m->warps[wi] = m->warps[--m->nWarps];
}

// The copy of Route 101 (a map of its own: nothing is generated). It
// changes a little on each visit: grass moves, the sign walks, trees break.
static void Run0GenCopy(const struct RogueRegion *r, u8 place, struct RogueMap *m, u16 *blocks)
{
    u8 visit = RogueGen_Run0Visit(RUN0_VISIT_COPY);
    u32 rng = RogueRng_Hash(r->seed, 78000 + visit), n, j;
    u8 sx = 5, sy = 9;
    int i;
    memset(m, 0, sizeof(*m));
    m->place = place;
    m->blocks = blocks;
    m->w = RUN0ROUTE101_W;
    m->h = RUN0ROUTE101_H;
    for (i = 0; i < RUN0ROUTE101_W * RUN0ROUTE101_H; i++)
        blocks[i] = sRun0Route101[i];
    n = visit == 0 ? 0 : (visit < 5 ? 2 + 2 * visit : 12);
    for (j = 0; j < n; j++)
    {
        u32 kind = RogueRng_Range(&rng, visit >= 2 ? 3 : 2), tries;
        for (tries = 0; tries < 60; tries++)
        {
            int x = 2 + RogueRng_Range(&rng, 16), y = 2 + RogueRng_Range(&rng, 16), x2, y2, k, open = 0;
            u16 *b = &blocks[y * m->w + x];
            YIELD();
            if (kind == 0)
            {
                // Tall grass and short grass trade places.
                x2 = 2 + RogueRng_Range(&rng, 16);
                y2 = 2 + RogueRng_Range(&rng, 16);
                if ((*b & 0x3FF) == 0x001 && (blocks[y2 * m->w + x2] & 0x3FF) == 0x00D)
                {
                    u16 t = *b;
                    *b = blocks[y2 * m->w + x2];
                    blocks[y2 * m->w + x2] = t;
                    break;
                }
            }
            else if (kind == 1)
            {
                // The sign stands somewhere else, off the trail.
                if (*b != RUN0_GRASS || (x >= 9 && x <= 12) || (x == sx && y == sy))
                    continue;
                for (k = 0; k < 4; k++)
                    open += RUN0_SOLID(blocks[(y + sDirDY[k]) * m->w + x + sDirDX[k]]) == 0;
                if (open < 4)
                    continue;
                blocks[sy * m->w + sx] = RUN0_GRASS;
                *b = B_SIGN | 0x3000;
                sx = x;
                sy = y;
                break;
            }
            else
            {
                // A tree takes its neighbour's half.
                if (!RUN0_SOLID(*b) || !RUN0_SOLID(b[1]) || (*b & 0x3FF) == 0x003 || (b[1] & 0x3FF) == 0x003)
                    continue;
                *b = (*b & ~0x3FF) | (b[1] & 0x3FF);
                break;
            }
        }
    }
    m->signs[0].x = sx;
    m->signs[0].y = sy;
    m->signs[0].kind = RSIGN_RUN0_REAL + RUN0_SIGN_ROUTE101;
    m->nSigns = 1;
}

// After a place's map is made.
static void Run0Finish(struct Gen *gen)
{
    const struct RogueRegion *r = gen->r;
    struct RogueMap *m = gen->m;
    const struct RoguePlace *p = &r->places[m->place];
    u8 act = Run0Act();
    u32 rng = RogueRng_Hash(r->seed, 77100 + m->place * 8 + act), bands, k;
    s8 houses[ROGUE_MAX_HOUSES];

    // The story's things first, so the corruption keeps clear of them.
    Run0TownHouses(r, m, houses);
    for (k = 0; k < m->nHouses; k++)
        if (houses[k] == RUN0_HOOK_TERRY_HOUSE && (RogueGen_Run0State() & RUN0_STATE_TERRY_DELETED))
            Run0DeleteHouse(gen, k);
    if (RogueGen_Run0HookPlace(r, RUN0_HOOK_GIFT_BOX) == m->place)
    {
        s32 pc = Run0WarpOf(m, RWARP_PC);
        static const s8 spots[][2] = { { 2, 1 }, { -2, 1 }, { 2, 2 }, { -2, 2 }, { 3, 1 }, { -3, 1 } };
        for (k = 0; pc >= 0 && k < sizeof(spots) / sizeof(spots[0]); k++)
        {
            int x = m->warps[pc].x + spots[k][0], y = m->warps[pc].y + spots[k][1];
            if (Run0Spot(gen, x, y))
            {
                AddObj(gen, ROBJ_RUN0, x, y, RDIR_SOUTH, RUN0_HOOK_GIFT_BOX, 0);
                break;
            }
        }
    }
    if (RogueGen_Run0HookPlace(r, RUN0_HOOK_CHAMPION_TERRY) == m->place)
    {
        s32 out = Run0WarpOf(m, RCAVE_NORTH);
        static const s8 spots[][2] = { { 1, 1 }, { -1, 1 }, { 1, 2 }, { -1, 2 }, { 2, 1 }, { -2, 1 } };
        for (k = 0; out >= 0 && k < sizeof(spots) / sizeof(spots[0]); k++)
        {
            int x = m->warps[out].x + spots[k][0], y = m->warps[out].y + spots[k][1];
            if (x > 0 && y > 0 && x < m->w - 1 && y < m->h - 1 && !RUN0_SOLID(m->blocks[y * m->w + x])
             && !Run0Taken(m, x, y))
            {
                AddObj(gen, ROBJ_RUN0, x, y, RDIR_SOUTH, RUN0_HOOK_CHAMPION_TERRY, 0);
                break;
            }
        }
    }
    if (p->kind == RPLACE_ROUTE)
        for (k = 0; k < RUN0_HOOK_PROMPTS; k++)
            if (RogueGen_Run0HookPlace(r, RUN0_HOOK_PROMPT_0 + k) == m->place)
                Run0SignBesideWalk(gen, k & 1 ? 2 : 0, RSIGN_RUN0 + k);

    // Act 4: real pieces cut into generated maps.
    if (act >= 4)
    {
        if ((s32)m->place == Run0GymPlace(r))
            Run0GymInWater(gen, &rng);
        if (Run0IsHousesRoute(r, m->place))
            Run0CutHouses(gen, 12);
        if (p->kind == RPLACE_ROUTE && !p->side && p->badges >= 6 && p->type != RROUTE_SEA && p->type != RROUTE_MOUNTAIN)
            Run0SignBesideWalk(gen, 1, RSIGN_RUN0_REAL + (m->place % RUN0_SIGN_COUNT));
    }

    // Towns from act 2: three POKeMON CENTERS in a row, or homes copied
    // with no door (not the first town, nor the summit).
    if (act >= 2 && p->kind == RPLACE_TOWN && p->type != RTOWN_START && p->type != RTOWN_SUMMIT)
    {
        u32 h = Run0Hash(r, m->place) % 4;
        s32 pc = Run0WarpOf(m, RWARP_PC), home = Run0WarpOf(m, RWARP_HOUSE);
        // Act 2 already doubles half the towns' CENTERS (a quarter left too
        // few to see before the third act).
        if ((h == 0 || h == 2) && pc >= 0)
            Run0RepeatBuilding(gen, m->warps[pc].x, m->warps[pc].y, 2, FALSE);
        if ((h == 1 || h == 3 || (act >= 3 && h == 2)) && home >= 0)
            Run0RepeatBuilding(gen, m->warps[home].x, m->warps[home].y, act >= 4 ? 3 : 2, TRUE);
    }

    // Broken joins, more each act. The first town keeps clear in act 1.
    if (p->kind == RPLACE_TOWN && p->type == RTOWN_START && act < 2)
        bands = 0;
    else if (act <= 1)
        bands = Run0Hash(r, m->place + 100) & 1;
    else
        bands = 2 * act - 2 + RogueRng_Range(&rng, act + 1);
    if (p->kind == RPLACE_CAVE)
        bands = bands / 2;
    Run0BreakJoins(gen, bands, &rng);
    // And where the player walks: one beside the way in act 1, two in act 2
    // (not in the first town in act 1, nor in caves).
    if (p->kind != RPLACE_CAVE && !(p->kind == RPLACE_TOWN && p->type == RTOWN_START && act < 2))
        Run0RoadsideBreaks(gen, act <= 1 ? 2 : 3, &rng);
    // Tears from act 2, one more each act.
    if (act >= 2 && !(p->kind == RPLACE_TOWN && p->type == RTOWN_START))
        for (k = 0; k + 2 <= act; k++)
            Run0Tear(gen, &rng);

    if (act >= 4 && m->place == r->victoryRoad && p->kind == RPLACE_ROUTE)
    {
        u8 level = 1 + RogueGen_Run0Visit(RUN0_VISIT_VICTORY);
        Run0ThinVictoryRoad(gen, level > 4 ? 4 : level);
    }
}

// Where act 4 cuts real pieces in (for gen_host and the tests).
void RogueGen_Run0CutPlaces(const struct RogueRegion *r, s32 *gym, s32 *houses)
{
    *gym = RogueGen_IsRun0(r) ? Run0GymPlace(r) : -1;
    *houses = RogueGen_IsRun0(r) ? Run0HousesRoute(r, 0) : -1;
}

// The random numbers of a place's map: the town laid out again on each
// entry draws others on each visit. 0 for every other place.
static u32 Run0MapSalt(const struct RogueRegion *r, u8 place)
{
    if (!RogueGen_IsRun0(r) || (s32)place != RogueGen_Run0RelayTown(r))
        return 0;
    return RogueGen_Run0Visit(RUN0_VISIT_RELAY) * 0x9E3779B9u;
}

// The eight custom leaders, one gym each, in an order a new team can face
// first (the beginner types lead); the Elite Four seats Emerald's four, for
// the story to fill.
static bool8 Run0Leaders(const struct RogueRegion *r, u8 *gyms, u8 *e4)
{
    static const u8 sGyms[ROGUE_NUM_GYMS] = { 0, 2, 1, 3, 4, 5, 6, 7 }; // CRAG, BASIL, MARINA, CINDER, VOLTZ, MAKO, LUNA, ACE
    u32 i;
    if (!RogueGen_IsRun0(r))
        return FALSE;
    for (i = 0; i < ROGUE_NUM_GYMS; i++)
        gyms[i] = sGyms[i];
    for (i = 0; i < ROGUE_NUM_E4; i++)
        e4[i] = 16 + i; // SIDNEY, PHOEBE, GLACIA, DRAKE
    return TRUE;
}

// The way TERRY opens in act 3: the region's shortcut back (RBRANCH_SHORTCUT)
// whose obstacle stands as objects (trees, rocks, a boulder), the first of
// the third act's stretch, else any. -1 if none (or not RUN 0).
s32 RogueGen_Run0ShortcutPlace(const struct RogueRegion *r)
{
    s32 i, any = -1;
    if (!RogueGen_IsRun0(r))
        return -1;
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        if (p->branch != RBRANCH_SHORTCUT || p->obstacle == ROBST_NONE || p->obstacle == ROBST_SURF)
            continue;
        if (p->badges >= 4 && p->badges <= 5)
            return i;
        if (any < 0)
            any = i;
    }
    return any;
}

// ---------------------------------------------------------------------------
// After RUN 0's STOP: the moving truck parked in every normal region's first
// town (the game adds it as the map is made, ParkTruck in src/rogue.c).

// Is the truck's whole picture (three blocks wide, three tall, standing on
// x, y) on open ground, on no door and no one's spot? With top FALSE only its
// wheels' row and the one above count, and the cab's top may overhang.
static bool8 Run0TruckFits(const struct RogueMap *m, s32 x, s32 y, bool8 top)
{
    s32 dx, dy;
    u32 i;
    for (dy = top ? -2 : -1; dy <= 0; dy++)
    {
        for (dx = -1; dx <= 1; dx++)
        {
            s32 bx = x + dx, by = y + dy;
            if (bx < 1 || by < 1 || bx >= m->w - 1 || by >= m->h - 1)
                return FALSE;
            if (m->blocks[by * m->w + bx] & 0x0C00)
                return FALSE;
            for (i = 0; i < m->nWarps; i++)
                if (m->warps[i].x == bx && m->warps[i].y == by)
                    return FALSE;
            for (i = 0; i < m->nObjs; i++)
                if (m->objs[i].x == bx && m->objs[i].y == by)
                    return FALSE;
            for (i = 0; i < m->nSigns; i++)
                if (m->signs[i].x == bx && m->signs[i].y == by)
                    return FALSE;
        }
    }
    return TRUE;
}

// Rings round the lab's door, from three blocks out, each walked in the same
// order: the first spot where the truck fits (Run0TruckFits).
static bool8 Run0TruckRing(const struct RogueMap *m, bool8 top, s32 *outX, s32 *outY)
{
    s32 doorX = m->startX, doorY = m->startY, x, y, ring, k;
    u32 i, w, pass;
    for (i = 0; i < m->nHouses; i++)
    {
        if (m->interiors[i] != RINT_LAB)
            continue;
        for (w = 0; w < m->nWarps; w++)
        {
            if (m->warps[w].kind == ROGUE_HOUSE_WARP(i))
            {
                doorX = m->warps[w].x;
                doorY = m->warps[w].y;
            }
        }
        break;
    }
    // (A clear spot is looked for further out: crowded towns have few.)
    for (pass = 0; pass < (top ? 2 : 1); pass++)
    for (ring = 3; ring <= (top ? 24 : 12); ring++)
    {
        for (k = 0; k < ring * 8; k++)
        {
            s32 side = k / (ring * 2), step = k % (ring * 2) - ring;
            x = doorX + (side == 0 ? step : side == 1 ? ring : side == 2 ? -step : -ring);
            y = doorY + (side == 0 ? ring : side == 1 ? -step : side == 2 ? -ring : step);
            // Never across the door's own column, nor above the door's row
            // (the first pass looks below it, then anywhere).
            if ((pass == 0 && y <= doorY) || (x >= doorX - 1 && x <= doorX + 1))
                continue;
            if (Run0TruckFits(m, x, y, top))
            {
                *outX = x;
                *outY = y;
                return TRUE;
            }
        }
    }
    return FALSE;
}

// The first spot where the whole truck stands clear (its top over no fence,
// tree or roof), else the first where its wheels and body do. FALSE if none
// (it is then not parked).
bool8 RogueGen_TruckSpot(const struct RogueMap *m, s32 *outX, s32 *outY)
{
    return Run0TruckRing(m, TRUE, outX, outY) || Run0TruckRing(m, FALSE, outX, outY);
}
