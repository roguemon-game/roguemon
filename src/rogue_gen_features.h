// Route features: the one thing a route is remembered for, as Hoenn's are
// (Route 117's Day Care, Route 109's Seashore House, the glass workshop in
// Route 113's ash, the twins on Route 103, Kanto's sleeping SNORLAX).
// Included at the end of rogue_gen.c, so the host build (gen_host) sees the
// same thing.
//
// A feature is something built on the route's map from vanilla Gen 3 pieces,
// a person who lives there, something to do and a reward. A region has at
// most one of each, at most one a route, none on many, spread so no two
// routes that meet both have one, and fitted to the route's kind, tileset and
// level. Double battle pairs (TWINS, couples...) come on top, on up to three
// routes without a feature. All of it is a pure function of the plan on its
// own random streams (salts 42000 on): nothing is saved, and only route maps
// change.
//
// The game side: rogue_town.c (the houses inside, the people's scripts),
// rogue.c (the feature trainers' teams and lines), data/scripts/rogue.inc.
// The words: src/rogue_text_features.h.

#include "constants/species.h"
#include "constants/items.h"

#define FEAT_SALT       42000 // the assignment
#define FEAT_PAIR_SALT  42100 // the double battle pairs
#define FEAT_NICK_SALT  42200 // the routes' local names
#define FEAT_NAME_SALT  42300 // the residents' names
#define FEAT_GIFT_SALT  42400 // gifts and rewards

// ---------------------------------------------------------------------------
// Where a feature fits

// A route that can take a feature at all: not Victory Road or the sea, not
// the first route, not the rival's (who needs the road), not the route up to a
// cave or a power plant, not one with a block standing on it, nor the far
// mouth of the region's tunnel or LATE CAMP's camper's road.
static bool8 FeatRouteBase(const struct RogueRegion *r, u8 place)
{
    const struct RoguePlace *p = &r->places[place];
    if (p->kind != RPLACE_ROUTE || p->type == RROUTE_MOUNTAIN || p->type == RROUTE_SEA)
        return FALSE;
    if (p->rival || p->mouth != 0xFF)
        return FALSE;
    if (!p->side && p->number == 1)
        return FALSE;
    if (!p->side && p->obstacle && !ObstacleOutside(r, p))
        return FALSE;
    if (r->tunnel < r->nDungeons && r->dungeons[r->tunnel].exit == place)
        return FALSE;
    if (RogueGen_RouteCamp(r) == (s32)place)
        return FALSE;
    return TRUE;
}

// A dungeon's way in stands on the route (a forest's, a power plant's door).
static bool8 FeatHostsDungeon(const struct RogueRegion *r, u8 place)
{
    u32 i;
    for (i = 0; i < r->nDungeons; i++)
        if (r->dungeons[i].host == place || r->dungeons[i].exit == place)
            return TRUE;
    return FALSE;
}

// The trainers of the features fit after the region's own.
static bool8 FeatTrainersFit(const struct RogueRegion *r)
{
    return r->nTrainerSlots <= ROGUE_SLOT_FEATURE_0;
}

static bool8 FeatKind(const struct RoguePlace *p, u8 a, u8 b, u8 c, u8 d)
{
    return p->type == a || p->type == b || p->type == c || p->type == d;
}

static bool8 FeatNeighbours(const struct RogueRegion *r, u8 a, u8 b);

// How well a feature fits a route: 0 if it cannot stand there.
static int FeatWeight(const struct RogueRegion *r, u8 place, u8 feat)
{
    const struct RoguePlace *p = &r->places[place];
    bool8 house = feat != RFEAT_SNORLAX && feat != RFEAT_GAUNTLET;
    u8 tier = p->tier;
    if (house && FeatHostsDungeon(r, place))
        return 0;
    switch (feat)
    {
    case RFEAT_DAYCARE:
        // Route 117: meadows between two towns, early in the journey.
        if (!(FeatKind(p, RROUTE_FIELD, RROUTE_FOREST, RROUTE_LAKE, RROUTE_RIVER) || p->type == RROUTE_FLOWERS)
         || tier < 1 || tier > 5)
            return 0;
        return p->type == RROUTE_FLOWERS ? 40 : p->type == RROUTE_FIELD ? 30 : 20;
    case RFEAT_BERRY:
        // Route 123: a long meadow; never on the road out of a town of
        // berry farmers (one berry master to a stretch of road).
        if (!FeatKind(p, RROUTE_FIELD, RROUTE_FOREST, RROUTE_LAKE, RROUTE_FLOWERS) || tier < 2 || tier > 8)
            return 0;
        {
            u8 farm = RogueGen_SignatureTown(r, RSIG_BERRY);
            if (farm != 0xFF && FeatNeighbours(r, place, farm))
                return 0;
        }
        return p->type == RROUTE_FIELD || p->type == RROUTE_FLOWERS ? 30 : 15;
    case RFEAT_WORKSHOP:
        // Route 113: in the ash, drawn with Fallarbor's tiles.
        if (p->theme != RTHEME_ASH || p->type == RROUTE_WOODS || p->type == RROUTE_COAST || tier < 1 || tier > 7)
            return 0;
        return p->type == RROUTE_DESERT ? 60 : 20;
    case RFEAT_SEASHORE:
        // Route 109: a beach. Slateport's own house on its tiles, a beach
        // hut on others.
        if (p->type != RROUTE_COAST || p->seaSide == RDIR_NONE || tier < 1 || !FeatTrainersFit(r))
            return 0;
        return p->theme == RTHEME_HARBOR ? 60 : 25;
    case RFEAT_SNORLAX:
        // Kanto's: it blocks a way the player can leave for later.
        if (!FeatKind(p, RROUTE_FOREST, RROUTE_WOODS, RROUTE_HILLS, RROUTE_FIELD) || tier < 2 || tier > 9)
            return 0;
        return p->side ? 40 : 8;
    case RFEAT_FAMILY:
        // Route 111: a family who live out on the road and love a battle.
        if (!FeatKind(p, RROUTE_DESERT, RROUTE_FIELD, RROUTE_HILLS, RROUTE_FOREST) || tier < 3 || tier > 7
         || !FeatTrainersFit(r) || p->side)
            return 0;
        // v1.1: a narrow desert between its rock walls has no room for
        // their house and yard (seed 111's route 12, 24 blocks across).
        if (p->type == RROUTE_DESERT && (p->w < 30 || p->h < 30))
            return 0;
        return p->type == RROUTE_DESERT ? 40 : 15;
    case RFEAT_GAUNTLET:
        // Nugget Bridge: a crossing on the main road with its five.
        if (!FeatKind(p, RROUTE_RIVER, RROUTE_LAKE, RROUTE_RIVER, RROUTE_LAKE) || p->side || p->nTrainers < 4
         || tier < 1 || tier > 6 || p->obstacle)
            return 0;
        return p->type == RROUTE_RIVER ? 40 : 20;
    case RFEAT_GURU:
        if (!FeatKind(p, RROUTE_LAKE, RROUTE_RIVER, RROUTE_COAST, RROUTE_LAKE) || tier < 1)
            return 0;
        return p->type == RROUTE_COAST ? 10 : 25;
    case RFEAT_SAILOR:
        // Route 104's cottage by the sea, before the sea route.
        if (p->type != RROUTE_COAST || p->seaSide == RDIR_NONE || p->badges > 4)
            return 0;
        return 40;
    }
    return 0;
}

// Two routes that meet are neighbours (a town between them is a break).
static bool8 FeatNeighbours(const struct RogueRegion *r, u8 a, u8 b)
{
    const struct RoguePlace *p = &r->places[a];
    u32 i;
    for (i = 0; i < p->nLinks; i++)
        if (p->links[i].target == b)
            return TRUE;
    return FALSE;
}

// ---------------------------------------------------------------------------
// The assignment, kept for the region last asked about.

static ROGUE_EWRAM u32 sFeatSeed = 0;
static ROGUE_EWRAM u8 sFeatAt[RFEAT_COUNT] = {0};   // the route of each feature, 0xFF none
static ROGUE_EWRAM u8 sPairAt[ROGUE_PAIRS] = {0};   // the routes of the pairs, 0xFF none

static bool8 FeatTaken(u8 place, const u8 *at, u32 n)
{
    u32 i;
    for (i = 0; i < n; i++)
        if (at[i] == place)
            return TRUE;
    return FALSE;
}

static bool8 FeatNearTaken(const struct RogueRegion *r, u8 place, const u8 *at, u32 n)
{
    u32 i;
    for (i = 0; i < n; i++)
        if (at[i] != 0xFF && (at[i] == place || FeatNeighbours(r, place, at[i])))
            return TRUE;
    return FALSE;
}

static void AssignFeatures(const struct RogueRegion *r)
{
    u32 rng, i, k;
    u8 order[RFEAT_COUNT - 1];
    if (sFeatSeed == r->seed + 1)
        return;
    for (i = 0; i < RFEAT_COUNT; i++)
        sFeatAt[i] = 0xFF;
    for (i = 0; i < ROGUE_PAIRS; i++)
        sPairAt[i] = 0xFF;
    // The features in a shuffled order, each taking a route by weight.
    rng = RogueRng_Hash(r->seed, FEAT_SALT);
    for (i = 0; i < RFEAT_COUNT - 1; i++)
        order[i] = i + 1;
    for (i = RFEAT_COUNT - 2; i > 0; i--)
    {
        u32 j = RogueRng_Range(&rng, i + 1);
        u8 t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    for (k = 0; k < RFEAT_COUNT - 1; k++)
    {
        u8 feat = order[k];
        int total = 0, pick;
        for (i = 0; i < r->nPlaces; i++)
            if (FeatRouteBase(r, i) && !FeatNearTaken(r, i, sFeatAt, RFEAT_COUNT))
                total += FeatWeight(r, i, feat);
        if (total == 0)
            continue;
        pick = RogueRng_Range(&rng, total);
        for (i = 0; i < r->nPlaces; i++)
        {
            int w;
            if (!FeatRouteBase(r, i) || FeatNearTaken(r, i, sFeatAt, RFEAT_COUNT))
                continue;
            w = FeatWeight(r, i, feat);
            if (pick < w)
            {
                sFeatAt[feat] = i;
                break;
            }
            pick -= w;
        }
    }
    // Double battle pairs on routes with no feature, apart from each other.
    if (FeatTrainersFit(r))
    {
        rng = RogueRng_Hash(r->seed, FEAT_PAIR_SALT);
        for (k = 0; k < ROGUE_PAIRS; k++)
        {
            u8 cand[ROGUE_MAX_PLACES], n = 0;
            for (i = 0; i < r->nPlaces; i++)
            {
                const struct RoguePlace *p = &r->places[i];
                if (!FeatRouteBase(r, i) || FeatTaken(i, sFeatAt, RFEAT_COUNT) || FeatNearTaken(r, i, sPairAt, ROGUE_PAIRS))
                    continue;
                if (p->type == RROUTE_DESERT || p->type == RROUTE_HILLS)
                    continue; // narrow ways between rocks: no room for two
                if (p->type == RROUTE_BRIDGE)
                    continue; // a raised road: no roadside for them (v1.1 bridge)
                {
                    // (v1.1 towns2) Nor a route already full of people and
                    // things: two in each lane to an obstacle beside it and
                    // one on its own way in, besides its trainers, items,
                    // berry trees and a walker (seed 72's long grass, once
                    // a raft village moved its region).
                    int objs = p->nTrainers + p->nItems + p->nBerries + 1, j;
                    for (j = 0; j < p->nLinks; j++)
                    {
                        const struct RoguePlace *q = &r->places[p->links[j].target];
                        if (ObstacleOutside(r, q) && q->guardDir == Opposite(p->links[j].dir))
                            objs += 2;
                    }
                    if (p->obstacle && !ObstacleOutside(r, p))
                        objs += 2;
                    if (objs + 2 > ROGUE_MAX_OBJS)
                        continue;
                }
                {
                    // (v1.1) Nor where a forest crosses the route: its two
                    // short stretches of road have little room either.
                    s32 di = RogueGen_DungeonAt(r, i);
                    if (di >= 0 && r->dungeons[di].kind == RDUN_FOREST && r->dungeons[di].exit == r->dungeons[di].host)
                        continue;
                }
                cand[n++] = i;
            }
            if (n == 0)
                break;
            sPairAt[k] = cand[RogueRng_Range(&rng, n)];
        }
    }
    sFeatSeed = r->seed + 1;
}

u8 RogueGen_RouteFeature(const struct RogueRegion *r, u8 place)
{
    u32 i;
    if (place >= r->nPlaces || r->places[place].kind != RPLACE_ROUTE)
        return RFEAT_NONE;
    AssignFeatures(r);
    for (i = 1; i < RFEAT_COUNT; i++)
        if (sFeatAt[i] == place)
            return i;
    return RFEAT_NONE;
}

u8 RogueGen_FeatureRoute(const struct RogueRegion *r, u8 feature)
{
    if (feature == RFEAT_NONE || feature >= RFEAT_COUNT)
        return 0xFF;
    AssignFeatures(r);
    return sFeatAt[feature];
}

u8 RogueGen_PairRoute(const struct RogueRegion *r, u8 k)
{
    if (k >= ROGUE_PAIRS)
        return 0xFF;
    AssignFeatures(r);
    return sPairAt[k];
}

s8 RogueGen_RoutePair(const struct RogueRegion *r, u8 place)
{
    u32 k;
    AssignFeatures(r);
    for (k = 0; k < ROGUE_PAIRS; k++)
        if (sPairAt[k] == place && place != 0xFF)
            return k;
    return -1;
}

// The pairs of a region are of different kinds: TWINS and SIS AND BRO
// early, couples and SR. AND JR. later, as Emerald has them.
u8 RogueGen_PairKind(const struct RogueRegion *r, u8 k)
{
    static const u8 sEarly[] = { RPAIR_TWINS, RPAIR_SIS_AND_BRO, RPAIR_YOUNG_COUPLE };
    static const u8 sLate[] = { RPAIR_YOUNG_COUPLE, RPAIR_OLD_COUPLE, RPAIR_SR_AND_JR, RPAIR_SIS_AND_BRO };
    u8 place = RogueGen_PairRoute(r, k), kind, used = 0, j, tries;
    u32 rng;
    for (j = 0; j <= k && j < ROGUE_PAIRS; j++)
    {
        u8 pl = RogueGen_PairRoute(r, j);
        bool8 early = pl < r->nPlaces && r->places[pl].tier <= 3;
        rng = RogueRng_Hash(r->seed, FEAT_PAIR_SALT + 1 + j);
        for (tries = 0; tries < 8; tries++)
        {
            kind = early ? sEarly[RogueRng_Range(&rng, LORE_COUNT_OF(sEarly))] : sLate[RogueRng_Range(&rng, LORE_COUNT_OF(sLate))];
            if (!((used >> kind) & 1))
                break;
        }
        used |= 1 << kind;
        if (j == k)
            return place == 0xFF ? RPAIR_TWINS : kind;
    }
    return RPAIR_TWINS;
}

u8 RogueGen_FeatureSlotPlace(const struct RogueRegion *r, u32 slot)
{
    if (!FeatTrainersFit(r) || slot < ROGUE_SLOT_FEATURE_0 || slot >= ROGUE_SLOT_FEATURE_0 + ROGUE_FEATURE_TRAINERS)
        return 0xFF;
    if (slot < ROGUE_SLOT_FAMILY_0)
        return RogueGen_PairRoute(r, slot - ROGUE_SLOT_PAIR_0);
    if (slot < ROGUE_SLOT_SEASHORE_0)
        return RogueGen_FeatureRoute(r, RFEAT_FAMILY);
    return RogueGen_FeatureRoute(r, RFEAT_SEASHORE);
}

// ---------------------------------------------------------------------------
// Gifts and rewards

// A POKeMON someone left at the Day Care and never came back for.
static const u16 sDayCareGifts[] = { SPECIES_EEVEE, SPECIES_SKITTY, SPECIES_TOGEPI, SPECIES_LILLIPUP, SPECIES_BUNEARY, SPECIES_SMOOCHUM };

u16 RogueGen_FeatureGift(const struct RogueRegion *r, u8 feature, u8 *level)
{
    u8 place = RogueGen_FeatureRoute(r, feature);
    u32 rng = RogueRng_Hash(r->seed, FEAT_GIFT_SALT + feature);
    int lv;
    if (place == 0xFF)
        return SPECIES_NONE;
    // As the signature gifts: a few levels under the route's trainers.
    lv = (int)r->places[place].level - 3;
    *level = lv < 5 ? 5 : lv;
    switch (feature)
    {
    case RFEAT_DAYCARE:
        return sDayCareGifts[RogueRng_Range(&rng, LORE_COUNT_OF(sDayCareGifts))];
    case RFEAT_SAILOR:
        return SPECIES_WINGULL;
    }
    return SPECIES_NONE;
}

u16 RogueGen_FeatureItem(const struct RogueRegion *r, u8 feature, u8 *qty)
{
    static const u16 sFlutes[] = { ITEM_BLUE_FLUTE, ITEM_YELLOW_FLUTE, ITEM_RED_FLUTE };
    u32 rng = RogueRng_Hash(r->seed, FEAT_GIFT_SALT + 16 + feature);
    *qty = 1;
    if (RogueGen_FeatureRoute(r, feature) == 0xFF)
        return ITEM_NONE;
    switch (feature)
    {
    case RFEAT_BERRY:
        *qty = 3;
        return ITEM_SITRUS_BERRY;
    case RFEAT_WORKSHOP:
        return sFlutes[RogueRng_Range(&rng, LORE_COUNT_OF(sFlutes))];
    case RFEAT_SEASHORE:
        *qty = 6;
        return ITEM_SODA_POP;
    case RFEAT_FAMILY:
        return ITEM_MACHO_BRACE;
    case RFEAT_GAUNTLET:
        return ITEM_NUGGET;
    case RFEAT_GURU:
        *qty = 5;
        return ITEM_LURE_BALL;
    }
    *qty = 0;
    return ITEM_NONE;
}

u8 RogueGen_SnorlaxLevel(const struct RogueRegion *r)
{
    u8 place = RogueGen_FeatureRoute(r, RFEAT_SNORLAX);
    return place == 0xFF ? 30 : r->places[place].level + 3;
}

// ---------------------------------------------------------------------------
// Route facts: local names and who lives there

// Local names, as Kanto's NUGGET BRIDGE and SILENCE BRIDGE or Hoenn's
// CYCLING ROAD: by feature first, else by the land, about half the routes.
// Written as said in a sentence ("the LONG FORD"); a sign shows them in caps.
#define NICK_BY_FEATURE 0
static const char *const gRogueRouteNicks[] =
{
    // Features (RFEAT_* order, two each).
    "NURSERY LANE", "CRADLE MEADOW",           // RFEAT_DAYCARE
    "BERRY LANE", "ORCHARD ROAD",              // RFEAT_BERRY
    "the CINDER PATH", "the ASH ROAD",         // RFEAT_WORKSHOP
    "SUNNY SHORE", "SEASHELL BEACH",           // RFEAT_SEASHORE
    "SNOOZE HOLLOW", "the SLEEPY WAY",         // RFEAT_SNORLAX
    "the PROVING ROAD", "CHALLENGE ROW",       // RFEAT_FAMILY
    "the LONG FORD", "the CROSSING",           // RFEAT_GAUNTLET
    "ANGLER'S BEND", "STILLWATER",             // RFEAT_GURU
    "GULL POINT", "OLD SALT COVE",             // RFEAT_SAILOR
    // The land (RROUTE_* order, three each; the mountain and sea have none).
    "MAPLE WALK", "the OLD OAK ROAD", "FERNWAY",            // forest
    "MIRROR LAKE", "the WILLOW POOLS", "LILYPAD WAY",       // lake
    0, 0, 0,                                                 // mountain
    "DRIFTWOOD SHORE", "SALT BEACH", "the WINDY STRAND",    // coast
    "the RUSHES", "WILLOW BEND", "the STEPPING STONES",     // river
    "the STEPS", "STONY RISE", "BOULDER BRAE",              // hills
    "SKYLARK FIELDS", "the LONG GRASS", "WINDY MEADOW",     // field
    "the DRY ROAD", "DUNE WAY", "the SUNBAKED TRAIL",       // desert
    0, 0, 0,                                                 // sea
    "WHISPER WOODS", "the DEEP WOODS", "TANGLEWOOD",        // woods
    0, 0, 0,                                                 // flowers
    0, 0, 0,                                                 // long grass
    "the HIGH ROAD", "the LONG BRIDGE", "the SPAN",          // bridge (v1.1 bridge)
};
#define NICK_LAND_BASE (2 * RFEAT_COUNT - 2)
// Ash country takes these instead of the desert's.
static const char *const sAshNicks[3] = { "the ASHEN WAY", "GRAY DUNES", "SOOTFALL ROAD" };

// The residents' names: the BERRY MASTER, the Day Care couple, the
// glassblower, the guru, the old sailor (and his Wingull), the family.
static const char *const sResidentNames[] = { "OLWEN", "BARNABY", "HOLLIS", "MARGO", "TOBIAS", "EDNA", "PERCY", "GRETA", "RUFUS", "IMOGEN" };
static const char *const sPetNames[] = { "PEEKO", "PIPPA", "SKIPPER", "BREEZE", "DOT", "CAPTAIN" };
static const char *const sFamilyBattlers[] = { "HARDING", "BRANDT", "COLLIER", "DARROW", "FLETCHER", "GRIFFIN", "HOLT", "KIRBY", "MARSH", "STEELE" };

void RogueGen_RouteFacts(const struct RogueRegion *r, u8 place, struct RogueRouteFacts *f)
{
    const struct RoguePlace *p = &r->places[place % ROGUE_MAX_PLACES];
    u32 i, k = 0;
    memset(f, 0xFF, sizeof(*f));
    f->place = place;
    f->feature = RogueGen_RouteFeature(r, place);
    if (place >= r->nPlaces || p->kind != RPLACE_ROUTE)
        return;
    // A feature's route always has its local name; about half the others,
    // never two alike (each a land name's three dealt in road order).
    if (f->feature != RFEAT_NONE && f->feature != RFEAT_FAMILY)
        f->nick = NICK_BY_FEATURE + (f->feature - 1) * 2 + (RogueRng_Hash(r->seed, FEAT_NICK_SALT + place) & 1);
    else if (f->feature == RFEAT_NONE && p->type < RROUTE_COUNT && p->type != RROUTE_MOUNTAIN && p->type != RROUTE_SEA
          && RogueRng_Hash(r->seed, FEAT_NICK_SALT + 64 + place) % 100 < 45)
    {
        // The how-many-th route of its kind with a name so far.
        for (i = 0; i < place; i++)
        {
            const struct RoguePlace *q = &r->places[i];
            if (q->kind == RPLACE_ROUTE && q->type == p->type && RogueGen_RouteFeature(r, i) == RFEAT_NONE
             && RogueRng_Hash(r->seed, FEAT_NICK_SALT + 64 + i) % 100 < 45)
                k++;
        }
        if (k < 3)
            f->nick = NICK_LAND_BASE + p->type * 3 + RogueText_Deal(r->seed, FEAT_NICK_SALT + 128 + p->type, k, 3);
    }
    if (f->feature != RFEAT_NONE)
        f->resident = RogueText_Deal(r->seed, FEAT_NAME_SALT, f->feature, LORE_COUNT_OF(sResidentNames));
    f->pet = RogueRng_Hash(r->seed, FEAT_NAME_SALT + 1) % LORE_COUNT_OF(sPetNames);
}

static bool8 FeatCopy(char *out, const char *s)
{
    if (s == NULL)
        return FALSE;
    while ((*out++ = *s++))
        ;
    return TRUE;
}

bool8 RogueGen_RouteNick(const struct RogueRegion *r, u8 place, char *out)
{
    struct RogueRouteFacts f;
    const struct RoguePlace *p;
    const char *weather;
    out[0] = 0;
    if (place >= r->nPlaces)
        return FALSE;
    RogueGen_RouteFacts(r, place, &f);
    // v1.1: a route known for its rain or mist is named for it, unless its
    // feature names it (src/rogue_gen_zones.h).
    weather = (f.feature == RFEAT_NONE || f.feature == RFEAT_FAMILY) ? KnownWeatherNick(r, place) : NULL;
    if (weather != NULL)
        return FeatCopy(out, weather);
    if (f.nick == 0xFF || f.nick >= LORE_COUNT_OF(gRogueRouteNicks))
        return FALSE;
    p = &r->places[place];
    // Ash country's dry road is ash, not sand.
    if (f.nick >= NICK_LAND_BASE + RROUTE_DESERT * 3 && f.nick < NICK_LAND_BASE + RROUTE_DESERT * 3 + 3 && p->theme == RTHEME_ASH)
        return FeatCopy(out, sAshNicks[f.nick - NICK_LAND_BASE - RROUTE_DESERT * 3]);
    return FeatCopy(out, gRogueRouteNicks[f.nick]);
}

bool8 RogueGen_FeatureName(const struct RogueRegion *r, u8 place, char *out)
{
    static const char *const sNames[RFEAT_COUNT] =
    {
        [RFEAT_DAYCARE] = "the DAY CARE", [RFEAT_BERRY] = "the BERRY MASTER's garden",
        [RFEAT_WORKSHOP] = "the GLASS WORKSHOP", [RFEAT_SEASHORE] = "the SEASHORE HOUSE",
        [RFEAT_SNORLAX] = "the sleeping SNORLAX", [RFEAT_GURU] = "the fishing hut",
        [RFEAT_SAILOR] = "the old sailor's cottage",
    };
    u8 feat = RogueGen_RouteFeature(r, place);
    out[0] = 0;
    if (feat == RFEAT_FAMILY)
    {
        // "the HARDING house"
        const char *s = sFamilyBattlers[RogueRng_Hash(r->seed, FEAT_NAME_SALT + 2) % LORE_COUNT_OF(sFamilyBattlers)];
        out = out + 4, FeatCopy(out - 4, "the ");
        while (*s)
            *out++ = *s++;
        return FeatCopy(out, " house");
    }
    if (feat == RFEAT_GAUNTLET)
        return RogueGen_RouteNick(r, place, out);
    return feat < RFEAT_COUNT && FeatCopy(out, sNames[feat]);
}

// The battling family's name (HARDING).
const char *RogueGen_FamilyBattlers(const struct RogueRegion *r)
{
    return sFamilyBattlers[RogueRng_Hash(r->seed, FEAT_NAME_SALT + 2) % LORE_COUNT_OF(sFamilyBattlers)];
}

const char *RogueGen_SailorPet(const struct RogueRegion *r)
{
    return sPetNames[RogueRng_Hash(r->seed, FEAT_NAME_SALT + 1) % LORE_COUNT_OF(sPetNames)];
}

bool8 RogueGen_ResidentName(const struct RogueRegion *r, u8 place, char *out)
{
    struct RogueRouteFacts f;
    out[0] = 0;
    if (place >= r->nPlaces)
        return FALSE;
    RogueGen_RouteFacts(r, place, &f);
    if (f.feature == RFEAT_FAMILY)
        return FeatCopy(out, RogueGen_FamilyBattlers(r));
    if (f.resident == 0xFF)
        return FALSE;
    return FeatCopy(out, sResidentNames[f.resident % LORE_COUNT_OF(sResidentNames)]);
}

// ---------------------------------------------------------------------------
// On the map (GenRoute in rogue_gen.c): a feature's lot is laid where the rest
// house would stand, and its house takes the rest house's door (RINT_FEATURE);
// its people, Pokemon and pair stand beside the road once the trainers and
// items have their places.

#define FG_ 0x3001 // grass
#define FS_ 0x3124 // sand
// Route 117's Day Care yard: a closed pen of General's fence (6x6) left of
// the house.
static const u16 sFeatYard[6 * 6] = {
    F_TL, F_H, F_H, F_H, F_H, F_TR,
    F_L,  FG_, FG_, FG_, FG_, F_R,
    F_L,  FG_, FG_, FG_, FG_, F_R,
    F_L,  FG_, FG_, FG_, FG_, F_R,
    F_L,  FG_, FG_, FG_, FG_, F_R,
    F_BL, F_H, F_H, F_H, F_H, F_BR,
};
// Route 123's garden: soft soil in a fence, open below (6x5).
static const u16 sFeatGarden[6 * 5] = {
    F_TL, F_H,      F_H,    F_H,    F_H,      F_TR,
    F_L,  B_SOIL_L, B_SOIL, B_SOIL, B_SOIL_R, F_R,
    F_L,  B_SOIL_FRONT_L, B_SOIL_FRONT, B_SOIL_FRONT, B_SOIL_FRONT_R, F_R,
    F_L,  FG_,      FG_,    FG_,    FG_,      F_R,
    F_BL, F_BR,     FG_,    FG_,    F_BL,     F_BR,
};
// Above the house on a lot two units taller than it: grass.
static const u16 sFeatGrass[4 * 2] = { FG_, FG_, FG_, FG_, FG_, FG_, FG_, FG_ };
// Route 109's Seashore House (Slateport's tiles), with its sign on sand.
static const u16 sFeatSeashore[6 * 4] = {
    0x32C0, 0x32C1, 0x32C1, 0x32C1, 0x32C2, FS_,
    0x06C8, 0x06C9, 0x06C9, 0x06C9, 0x06CA, FS_,
    0x06D8, 0x06D1, 0x06D3, 0x06D1, 0x06DA, FS_,
    0x06CD, 0x0413, 0x02DB, 0x0413, 0x06CE, B_SIGN,
};
// Route 113's glass workshop (Fallarbor's tiles).
static const u16 sFeatWorkshop[4 * 4] = {
    0x02DE, 0x02DF, 0x02DF, 0x06FE,
    0x06E6, 0x06E7, 0x06E7, 0x0706,
    0x06EE, 0x06EF, 0x02FF, 0x070E,
    0x06F6, 0x06F7, 0x0707, 0x070F,
};
#undef FG_
#undef FS_

static const struct Building sFeatYardB     = { sFeatYard, 3, 3, 6, 6, 0, 0, 0, DOOR_LOCKED, 0 };
static const struct Building sFeatGardenB   = { sFeatGarden, 3, 3, 6, 5, 1, 0, 0, DOOR_LOCKED, 0 };
static const struct Building sFeatGrassB    = { sFeatGrass, 2, 1, 4, 2, 0, 0, 0, DOOR_LOCKED, 0 };
static const struct Building sFeatSeashoreB = { sFeatSeashore, 3, 2, 6, 4, 0, 2, 3, RWARP_HOUSE, 0 };
static const struct Building sFeatWorkshopB = { sFeatWorkshop, 2, 2, 4, 4, 0, 1, 3, RWARP_HOUSE, 0 };

// struct FeatLot and struct FeatRoute are in rogue_gen.c, before GenRoute.

// Houses on routes: the next town's on green routes, else the plain one, as
// the rest house (PlaceRestHouse).
static const struct Building *FeatPlainHouse(struct Gen *gen)
{
    const struct RoguePlace *p = gen->p;
    const struct TownTheme *th = &sThemes[p->theme];
    bool8 green = p->type == RROUTE_FOREST || p->type == RROUTE_WOODS || p->type == RROUTE_FIELD
               || p->type == RROUTE_LAKE || p->type == RROUTE_RIVER
               || p->type == RROUTE_FLOWERS || p->type == RROUTE_LONGGRASS;
    bool8 themed = green && th->ground == TGROUND_GRASS
                && (p->theme != RTHEME_TREETOP || p->type == RROUTE_FOREST || p->type == RROUTE_WOODS);
    return themed ? &th->houses[RAND(th->nHouses)] : &sHouses[2];
}

static void FeatBegin(struct Gen *gen, struct FeatRoute *fr)
{
    const struct RoguePlace *p = gen->p;
    u8 place = p - gen->r->places;
    memset(fr, 0, sizeof(*fr));
    fr->feature = RogueGen_RouteFeature(gen->r, place);
    fr->pair = RogueGen_RoutePair(gen->r, place);
    fr->restPlanned = (p->interiors & (1 << RINT_REST)) != 0;
    fr->signX = fr->signY = -1;
    fr->lot.openX = 0xFF;
    switch (fr->feature)
    {
    case RFEAT_DAYCARE:
        // The yard on the left, the house on the right a unit down.
        fr->lot.side = &sFeatYardB;
        fr->lot.house = &sHouses[2];
        fr->lot.hx = 3, fr->lot.hy = 1, fr->lot.uw = 5, fr->lot.uh = 3;
        break;
    case RFEAT_BERRY:
        // (v1.1) On a berry farm route, his house beside the fenced field
        // (src/rogue_gen_berry.h).
        if (BerryFarmBegin(gen, &fr->lot))
            break;
        fr->lot.house = &sHouses[2];
        fr->lot.hx = 0, fr->lot.hy = 1;
        fr->lot.side = &sFeatGardenB;
        fr->lot.sx = 2, fr->lot.sy = 0, fr->lot.uw = 5, fr->lot.uh = 3;
        fr->lot.openX = 3; // the garden's way in, below its middle
        break;
    case RFEAT_WORKSHOP:
        fr->lot.house = &sFeatWorkshopB;
        fr->lot.uw = 2, fr->lot.uh = 2;
        break;
    case RFEAT_SEASHORE:
        // Slateport's own house, or a beach hut (also where the big one
        // will not fit, after a few tries).
        fr->lot.house = p->theme == RTHEME_HARBOR && gen->attempt < FEAT_SMALL_FROM ? &sFeatSeashoreB : &sHouses[2];
        fr->lot.uw = fr->lot.house->uw, fr->lot.uh = fr->lot.house->uh;
        break;
    case RFEAT_FAMILY:
    case RFEAT_GURU:
    case RFEAT_SAILOR:
        fr->lot.house = FeatPlainHouse(gen);
        fr->lot.uw = fr->lot.house->uw, fr->lot.uh = fr->lot.house->uh;
        break;
    }
}

// The unit of the lot's door, and the unit in front of it.
static void FeatDoorUnit(const struct FeatRoute *fr, int bx, int by, int *dx, int *fy)
{
    const struct Building *b = fr->lot.house;
    *dx = bx + fr->lot.hx + b->doorX / 2;
    *fy = by + fr->lot.hy + b->uh;
}

// Water or sea within a unit or two of a unit.
static bool8 FeatNearWater(struct Gen *gen, int ux, int uy, int reach)
{
    int x, y;
    for (y = uy - reach; y <= uy + reach; y++)
        for (x = ux - reach; x <= ux + reach; x++)
            if (InBounds(gen, x, y) && (Cell(gen, x, y) == C_WATER || Cell(gen, x, y) == C_SEA))
                return TRUE;
    return FALSE;
}

static bool8 FeatWantsWater(const struct FeatRoute *fr)
{
    return fr->feature == RFEAT_SEASHORE || fr->feature == RFEAT_GURU || fr->feature == RFEAT_SAILOR;
}

// Units from the road's first unit to its last over open ground (a long
// way round when 0x7FFF): a lot must not make the player's way longer, or
// the walk turns away from the sea or river a route shows.
static int FeatWalkLen(struct Gen *gen)
{
    int head = 0, tail = 0, goal;
    if (gen->nPath < 2)
        return 0;
    goal = gen->path[gen->nPath - 1];
    memset(sBfsPrev, 0, gen->cw * gen->ch * sizeof(sBfsPrev[0]));
    sBfsQueue[tail++] = PACK(gen->path[0] % gen->cw, gen->path[0] / gen->cw);
    sBfsPrev[gen->path[0]] = 1;
    while (head < tail)
    {
        int cur = sBfsQueue[head++], cx = PX(cur), cy = PY(cur), ci = cy * gen->cw + cx;
        u8 d;
        YIELD();
        if (ci == goal)
            return sBfsPrev[ci] - 1;
        for (d = 0; d < 4; d++)
        {
            int nx = cx + sDirDX[d], ny = cy + sDirDY[d], ni = ny * gen->cw + nx;
            if (nx < 0 || ny < 0 || nx >= gen->cw || ny >= gen->ch || sBfsPrev[ni] || !IsOpenU(gen->g[ni]) || tail >= BFS_RING)
                continue;
            sBfsPrev[ni] = sBfsPrev[ci] + 1;
            sBfsQueue[tail++] = PACK(nx, ny);
        }
    }
    return 0x7FFF;
}

// Lays the feature's lot beside the road, where a rest house would go: on
// trees and open ground only, its door onto the road. Beside water for the
// beach and fishing houses, the nearest of the spots tried.
static bool8 FeatPlaceLot(struct Gen *gen, struct FeatRoute *fr)
{
    const struct FeatLot *lot = &fr->lot;
    int tries, bestX = -1, bestY = -1, best = 0x7FFF, walk = FeatWalkLen(gen);
    bool8 secondary;
    u8 onPath[(MAX_CELLS + 7) / 8];
    if (gen->nPath < 10)
        return FALSE;
    secondary = !StampIsPrimary(lot->house->stamp, lot->house->sw * lot->house->sh);
    // The road's units: a lot never stands on one.
    memset(onPath, 0, sizeof(onPath));
    for (tries = 0; tries < gen->nPath; tries++)
        onPath[gen->path[tries] >> 3] |= 1 << (gen->path[tries] & 7);
    for (tries = 0; tries < 80; tries++)
    {
        int pi = gen->nPath / 5 + RAND(gen->nPath * 3 / 5);
        int px = gen->path[pi] % gen->cw, py = gen->path[pi] / gen->cw;
        int doorU = lot->hx + lot->house->doorX / 2;
        int bx = px - doorU, by = py - lot->uh, x, y, score;
        bool8 ok = TRUE;
        YIELD();
        // (Nor beside a lane to a side place's obstacle, whose objects could
        // stand in front of the door: v1.1, seed 164's river.)
        if (bx < 1 || by < 1 || bx + lot->uw > gen->cw - 1 || InGateLaneF(gen, px * 2, py * 2)
         || InGateLaneF(gen, px * 2 + 2, py * 2) || InGateLaneF(gen, px * 2 - 2, py * 2)
         || InGateLaneF(gen, px * 2, py * 2 + 2) || InGateLaneF(gen, px * 2, py * 2 - 2))
            continue;
        // A shorter house leaves the units below its side of the lot to the
        // road: the door's row is the lot's bottom row.
        for (y = by; y < by + lot->uh && ok; y++)
            for (x = bx; x < bx + lot->uw && ok; x++)
            {
                u8 c = Cell(gen, x, y);
                int u = y * gen->cw + x;
                if ((c != C_TREE && c != C_OPEN) || IsEdge(gen, x, y) || ForestWayUnit(gen, x, y) || ((onPath[u >> 3] >> (u & 7)) & 1))
                    ok = FALSE;
            }
        // (v1.1 towns2) A treehouse's thicket ends in the woods' crowns:
        // never over an ordinary tree, whose crown they would cut (seed 13
        // once a raft village moved its region's routes).
        for (x = bx; x < bx + lot->uw && ok && lot->house->frontY; x++)
            if (Cell(gen, x, by + lot->uh) == C_TREE)
                ok = FALSE;
        // Ground or trees round it, never water or a rock face (but for a
        // coast so cramped that a house by the sea has nowhere else to go).
        if (!ok || !RingClear(gen, bx, by, lot->uw, lot->uh, FALSE, FALSE, gen->attempt >= FEAT_ANY_WALK_FROM))
            continue;
        // The lot must not cut the road, nor any open ground off it (the
        // garden's way in, below it, is joined to the road too: trees and
        // ledges come later, so it is kept as road).
        {
            u8 saved[5 * 8]; // (v1.1 berry: a farm lot is up to 8x5)
            int before, after;
            bool8 joined = TRUE;
            if (lot->openX != 0xFF)
            {
                u8 c = Cell(gen, bx + lot->openX, by + lot->uh);
                if (c != C_OPEN && c != C_ROAD)
                    continue;
            }
            before = FloodReachable(gen, px, py);
            for (y = 0; y < lot->uh; y++)
                for (x = 0; x < lot->uw; x++)
                {
                    saved[y * 8 + x] = Cell(gen, bx + x, by + y);
                    if (Reached(gen, bx + x, by + y))
                        before--; // ground the lot itself takes
                    SetCell(gen, bx + x, by + y, C_BLDG);
                }
            after = FloodReachable(gen, px, py);
            if (lot->openX != 0xFF)
                joined = Reached(gen, bx + lot->openX, by + lot->uh);
            if (joined && after >= before && gen->attempt < FEAT_ANY_WALK_FROM && FeatWalkLen(gen) > walk)
                joined = FALSE;
            for (y = 0; y < lot->uh; y++)
                for (x = 0; x < lot->uw; x++)
                    SetCell(gen, bx + x, by + y, saved[y * 8 + x]);
            if (!joined || after < before)
                continue;
        }
        // Theme tiles stay out of view of a neighbour with another tileset.
        if (secondary && (InRouteSeam(gen, bx * 2, by * 2) || InRouteSeam(gen, (bx + lot->uw) * 2 - 1, (by + lot->uh) * 2 - 1)))
            continue;
        // Nor anywhere a person across the seam would see the lot pop in.
        if (SeenFromNextDoor(gen, bx * 2 + 1, by * 2 + 1) || SeenFromNextDoor(gen, (bx + lot->uw) * 2 - 2, (by + lot->uh) * 2 - 2))
            continue;
        score = 0;
        if (FeatWantsWater(fr))
            score = FeatNearWater(gen, px, py, 1) ? 0 : FeatNearWater(gen, px, py, 3) ? 10 : 100;
        score = score * 4 + RAND(4);
        if (score < best)
        {
            best = score;
            bestX = bx;
            bestY = by;
            if (score < 4)
                break;
        }
    }
    if (bestX < 0)
        return FALSE;
    {
        int x, y, dx, fy;
        for (y = bestY; y < bestY + lot->uh; y++)
            for (x = bestX; x < bestX + lot->uw; x++)
                SetCell(gen, x, y, C_BLDG);
        FeatDoorUnit(fr, bestX, bestY, &dx, &fy);
        MarkOccupied(gen, (bestX + lot->hx) * 2 + lot->house->doorX, (bestY + lot->hy) * 2 + lot->house->sy + FrontY(lot->house));
        (void)dx, (void)fy;
        if (lot->openX != 0xFF)
        {
            if (Cell(gen, bestX + lot->openX, bestY + lot->uh) == C_OPEN)
                SetCell(gen, bestX + lot->openX, bestY + lot->uh, C_ROAD);
            MarkOccupied(gen, (bestX + lot->openX) * 2, (bestY + lot->uh) * 2);
            MarkOccupied(gen, (bestX + lot->openX) * 2 + 1, (bestY + lot->uh) * 2);
        }
    }
    fr->bx = bestX;
    fr->by = bestY;
    fr->hasLot = TRUE;
    // Route 113's workshop stands right in the ash.
    gen->noLawn = fr->feature == RFEAT_WORKSHOP;
    return TRUE;
}

// A spot beside the road for a feature's person or Pokemon, near (nx, ny)
// (units) if it can: within reach units, else anywhere along the road.
static bool8 FeatSpot(struct Gen *gen, int nx, int ny, int reach, bool8 water, int *fx, int *fy, u8 *facing)
{
    int tries;
    for (tries = 0; tries < 120; tries++)
    {
        int pi = 2 + RAND(gen->nPath > 4 ? gen->nPath - 4 : 1);
        int px = gen->path[pi] % gen->cw, py = gen->path[pi] / gen->cw;
        if (nx >= 0 && tries < 80 && (Abs(px - nx) > reach || Abs(py - ny) > reach))
            continue;
        if (water && tries < 60 && !FeatNearWater(gen, px, py, 1))
            continue;
        if (!FindSpotBesideRoad(gen, px, py, fx, fy, facing))
            continue;
        if (BlocksWay(gen, *fx, *fy))
            continue;
        if (Cover(gen, *fx, *fy) == V_TALL)
            SetCover(gen, *fx, *fy, BareCover(gen, *fx, *fy));
        return TRUE;
    }
    return FALSE;
}

// The occupied bit of a block alone (MarkOccupied also lists it as taken),
// to try a spot before taking it.
static void FeatSetOccupied(struct Gen *gen, int fx, int fy, bool8 on)
{
    int i = fy * gen->m->w + fx;
    if (on)
        gen->occupied[i >> 3] |= 1 << (i & 7);
    else
        gen->occupied[i >> 3] &= ~(1 << (i & 7));
}

// A double battle pair: two side by side beside the road, both looking at it.
static bool8 FeatPlacePair(struct Gen *gen, u8 k)
{
    int tries;
    for (tries = 0; tries < 100; tries++)
    {
        int pi = 3 + RAND(gen->nPath > 6 ? gen->nPath - 6 : 1), fx, fy, ox, oy;
        u8 facing;
        if (!FindSpotBesideRoad(gen, gen->path[pi] % gen->cw, gen->path[pi] / gen->cw, &fx, &fy, &facing))
            continue;
        // The second stands beside the first, along the road.
        ox = fx + ((facing == RDIR_NORTH || facing == RDIR_SOUTH) ? ((fx & 1) ? -1 : 1) : 0);
        oy = fy + ((facing == RDIR_EAST || facing == RDIR_WEST) ? ((fy & 1) ? -1 : 1) : 0);
        if (!InMap(gen, ox, oy) || Cell(gen, ox >> 1, oy >> 1) != C_OPEN || IsOccupied(gen, ox, oy) || ByLedgeF(gen, ox, oy)
         || SeenFromNextDoor(gen, ox, oy) || BlocksWay(gen, fx, fy) || BlocksWay(gen, ox, oy))
            continue;
        // The two together must not close a way either: each with the
        // other standing there.
        FeatSetOccupied(gen, fx, fy, TRUE);
        if (BlocksWay(gen, ox, oy))
        {
            FeatSetOccupied(gen, fx, fy, FALSE);
            continue;
        }
        FeatSetOccupied(gen, ox, oy, TRUE);
        FeatSetOccupied(gen, fx, fy, FALSE);
        if (BlocksWay(gen, fx, fy))
        {
            FeatSetOccupied(gen, ox, oy, FALSE);
            continue;
        }
        FeatSetOccupied(gen, ox, oy, FALSE);
        if (Cover(gen, fx, fy) == V_TALL)
            SetCover(gen, fx, fy, BareCover(gen, fx, fy));
        if (Cover(gen, ox, oy) == V_TALL)
            SetCover(gen, ox, oy, BareCover(gen, ox, oy));
        AddObj(gen, ROBJ_PAIR, fx, fy, facing, k, 3);
        AddObj(gen, ROBJ_PAIR, ox, oy, facing, k | 4, 3);
        return TRUE;
    }
    return FALSE;
}

// SNORLAX asleep in the way into a dead end that holds one of the route's
// items, the child who has waited for it beside it: the two fill the way,
// and the item lies behind them. Returns FALSE if no pocket will do.
static bool8 FeatPlaceSnorlax(struct Gen *gen)
{
    struct RogueMap *m = gen->m;
    int i;
    for (i = 0; i < m->nObjs; i++)
    {
        struct RogueObj *o = &m->objs[i];
        int ux = o->x >> 1, uy = o->y >> 1, d, open = -1, n = 0, ax, ay, bxx, byy, kx, ky;
        if (o->kind != ROBJ_ITEM)
            continue;
        for (d = 0; d < 4; d++)
        {
            u8 c = Cell(gen, ux + sDirDX[d], uy + sDirDY[d]);
            if (IsOpenU(c) || c == C_LEDGE || c == C_FORD || c == C_KEEP)
                open = d, n++;
        }
        // A dead end: one way in, on the road's side.
        if (n != 1 || IsEdge(gen, ux, uy))
            continue;
        // The two blocks of the unit on that side, and the item behind.
        if (open == RDIR_NORTH || open == RDIR_SOUTH)
        {
            ay = byy = uy * 2 + (open == RDIR_NORTH ? 0 : 1);
            ax = ux * 2, bxx = ux * 2 + 1;
            ky = uy * 2 + (open == RDIR_NORTH ? 1 : 0), kx = o->x;
        }
        else
        {
            ax = bxx = ux * 2 + (open == RDIR_WEST ? 0 : 1);
            ay = uy * 2, byy = uy * 2 + 1;
            kx = ux * 2 + (open == RDIR_WEST ? 1 : 0), ky = o->y;
        }
        if (ByLedgeF(gen, ax, ay) || ByLedgeF(gen, bxx, byy) || SeenFromNextDoor(gen, ax, ay) || SeenFromNextDoor(gen, bxx, byy)
         || SeenFromNextDoor(gen, kx, ky))
            continue;
        if ((ax != o->x || ay != o->y) && IsOccupied(gen, ax, ay))
            continue;
        if ((bxx != o->x || byy != o->y) && IsOccupied(gen, bxx, byy))
            continue;
        if (m->nObjs + 2 > ROGUE_MAX_OBJS)
            return FALSE;
        // The item moves behind them.
        o->x = kx;
        o->y = ky;
        MarkOccupied(gen, kx, ky);
        if (Cover(gen, ax, ay) == V_TALL)
            SetCover(gen, ax, ay, BareCover(gen, ax, ay));
        if (Cover(gen, bxx, byy) == V_TALL)
            SetCover(gen, bxx, byy, BareCover(gen, bxx, byy));
        AddObj(gen, ROBJ_SNORLAX, ax, ay, RDIR_SOUTH, 0, 0);
        AddObj(gen, ROBJ_FEATFOLK, bxx, byy, open, FEATROLE_KID, 0);
        return TRUE;
    }
    return FALSE;
}

// The feature's people and Pokemon beside the road, and the pair. Call
// FloodReachable first (GenRoute's objects do). Returns TRUE if the walker
// out for a stroll should stay home (the feature's people talk instead).
static bool8 FeatPeople(struct Gen *gen, struct FeatRoute *fr, bool8 *fail)
{
    struct RogueMap *m = gen->m;
    int fx, fy, dx, dfy;
    u8 facing;
    bool8 quiet = FALSE;
    *fail = FALSE;
    if (fr->pair >= 0 && m->nObjs + 2 <= ROGUE_MAX_OBJS && !FeatPlacePair(gen, fr->pair))
    {
        *fail = TRUE;
        return FALSE;
    }
    if (fr->hasLot)
        FeatDoorUnit(fr, fr->bx, fr->by, &dx, &dfy);
    else
        dx = dfy = -1;
    switch (fr->feature)
    {
    case RFEAT_DAYCARE:
    {
        // Two Pokemon in the yard, the old man at its gate.
        int yx = (fr->bx + fr->lot.sx) * 2, yy = (fr->by + fr->lot.sy) * 2;
        if (m->nObjs + 3 > ROGUE_MAX_OBJS)
            break;
        AddObj(gen, ROBJ_FEATMON, yx + 2, yy + 2, RDIR_SOUTH, 0, 0);
        AddObj(gen, ROBJ_FEATMON, yx + 3, yy + 4, RDIR_SOUTH, 1, 0);
        if (FeatSpot(gen, dx, dfy, 3, FALSE, &fx, &fy, &facing))
            AddObj(gen, ROBJ_FEATFOLK, fx, fy, facing, FEATROLE_TALK, 0);
        quiet = TRUE;
        break;
    }
    case RFEAT_BERRY:
    {
        // The garden's four trees, on the soil (their own tree ids).
        // (v1.1) A farm's field: as many trees as there is room for, and
        // nobody out for a walk (src/rogue_gen_berry.h).
        int gx = (fr->bx + fr->lot.sx) * 2, gy = (fr->by + fr->lot.sy) * 2 + fr->lot.side->sy, k;
        if (fr->hasLot && BerryFarmLot(&fr->lot))
        {
            BerryFarmTrees(gen, fr);
            quiet = TRUE;
            break;
        }
        if (m->nObjs + ROGUE_GARDEN_BERRIES > ROGUE_MAX_OBJS)
            break;
        for (k = 0; k < ROGUE_GARDEN_BERRIES; k++)
            AddObj(gen, ROBJ_BERRY, gx + 1 + k, gy + 1, RDIR_SOUTH, 0x80 | k, 0);
        break;
    }
    case RFEAT_GURU:
        // His brother fishing from the bank.
        if (m->nObjs < ROGUE_MAX_OBJS && FeatSpot(gen, -1, -1, 0, TRUE, &fx, &fy, &facing))
            AddObj(gen, ROBJ_FEATFOLK, fx, fy, facing, FEATROLE_TALK, 0);
        quiet = TRUE;
        break;
    case RFEAT_SAILOR:
        // His Wingull out on the sand by the cottage.
        if (m->nObjs < ROGUE_MAX_OBJS && FeatSpot(gen, dx, dfy, 3, FALSE, &fx, &fy, &facing))
            AddObj(gen, ROBJ_FEATMON, fx, fy, RDIR_SOUTH, 2, 0);
        break;
    case RFEAT_GAUNTLET:
        // The prize at the far end of the crossing.
        if (m->nObjs < ROGUE_MAX_OBJS)
        {
            int tries;
            for (tries = 0; tries < 40; tries++)
            {
                int pi = gen->nPath - 3 - RAND(4 + tries / 8);
                if (pi > 2 && FindSpotBesideRoad(gen, gen->path[pi] % gen->cw, gen->path[pi] / gen->cw, &fx, &fy, &facing)
                 && !BlocksWay(gen, fx, fy))
                {
                    if (Cover(gen, fx, fy) == V_TALL)
                        SetCover(gen, fx, fy, BareCover(gen, fx, fy));
                    AddObj(gen, ROBJ_FEATFOLK, fx, fy, facing, FEATROLE_PRIZE, 0);
                    break;
                }
            }
            // (v1.1 sea: on the tries where the route would be built
            // without him, he waits anywhere along the later half of the
            // way, closer to others: a short crossing crowded with its five
            // left him no spot near its end, in seed 6047's new plan.)
            for (; tries >= 40 && tries < 120 && gen->attempt >= FEAT_GIVE_UP; tries++)
            {
                int pi = gen->nPath / 2 + RAND(gen->nPath - gen->nPath / 2);
                gen->crowdRadius = 1;
                if (FindSpotBesideRoad(gen, gen->path[pi] % gen->cw, gen->path[pi] / gen->cw, &fx, &fy, &facing)
                 && !BlocksWay(gen, fx, fy))
                {
                    gen->crowdRadius = 0;
                    if (Cover(gen, fx, fy) == V_TALL)
                        SetCover(gen, fx, fy, BareCover(gen, fx, fy));
                    AddObj(gen, ROBJ_FEATFOLK, fx, fy, facing, FEATROLE_PRIZE, 0);
                    break;
                }
                gen->crowdRadius = 0;
            }
            if (tries == 40 || tries == 120)
                *fail = TRUE;
        }
        quiet = TRUE;
        break;
    case RFEAT_SNORLAX:
        if (!FeatPlaceSnorlax(gen))
            *fail = TRUE;
        quiet = TRUE;
        break;
    }
    return quiet;
}

// The feature house's door, its sign, and what lies inside it.
static void FeatDoor(struct Gen *gen, struct FeatRoute *fr)
{
    const struct Building *b;
    int hx, hy, dx, dy, fy;
    if (!fr->hasLot)
        return;
    b = fr->lot.house;
    hx = fr->bx + fr->lot.hx;
    hy = fr->by + fr->lot.hy;
    dx = hx * 2 + b->doorX;
    dy = hy * 2 + b->sy + b->doorY;
    fy = hy * 2 + b->sy + FrontY(b);
    AddWarp(gen, dx, dy, RWARP_HOUSE);
    gen->m->interiors[gen->m->nHouses++] = RINT_FEATURE;
    if (b == &sFeatSeashoreB)
    {
        // The Seashore House's sign is in its stamp, on the sand beside it.
        fr->signX = hx * 2 + 5;
        fr->signY = hy * 2 + 3;
        AddSign(gen, fr->signX, fr->signY, RSIGN_FEATURE);
    }
    else if (InMap(gen, dx + 1, fy) && !IsOccupied(gen, dx + 1, fy) && Cell(gen, (dx + 1) >> 1, fy >> 1) != C_BLDG
          && !ByLedgeF(gen, dx + 1, fy) && WalkableCover(gen, dx + 1, fy))
        AddSign(gen, dx + 1, fy, RSIGN_FEATURE);
    else if (InMap(gen, dx - 1, fy) && !IsOccupied(gen, dx - 1, fy) && Cell(gen, (dx - 1) >> 1, fy >> 1) != C_BLDG
          && !ByLedgeF(gen, dx - 1, fy) && WalkableCover(gen, dx - 1, fy))
        AddSign(gen, dx - 1, fy, RSIGN_FEATURE);
    SetCover(gen, dx, fy, Cover(gen, dx, fy) == V_TALL ? V_NONE : Cover(gen, dx, fy));
}

// The gauntlet's sign: by the start of the crossing, as Nugget Bridge's.
static void FeatSigns(struct Gen *gen, struct FeatRoute *fr)
{
    int tries, fx, fy;
    u8 facing;
    if (fr->feature != RFEAT_GAUNTLET && fr->feature != RFEAT_SNORLAX)
        return;
    for (tries = 0; tries < 30; tries++)
    {
        int pi = 2 + RAND(4 + tries / 6);
        if (pi < gen->nPath && FindSignSpot(gen, gen->path[pi] % gen->cw, gen->path[pi] / gen->cw, &fx, &fy, &facing))
        {
            AddSign(gen, fx, fy, RSIGN_FEATURE);
            break;
        }
    }
}

static void FeatRender(struct Gen *gen, struct FeatRoute *fr)
{
    const struct FeatLot *lot = &fr->lot;
    if (!fr->hasLot)
        return;
    HOST_FEATLOT(BerryFarmLot(lot) ? 2 : 1); // (2: a berry farm, v1.1)
    if (lot->side)
        Stamp(gen, lot->side, fr->bx + lot->sx, fr->by + lot->sy);
    // A house shorter than its lot has grass above it.
    if (lot->hy > 0 && lot->house->uw == 2)
        Stamp(gen, &sFeatGrassB, fr->bx + lot->hx, fr->by);
    Stamp(gen, lot->house, fr->bx + lot->hx, fr->by + lot->hy);
}

// The block in front of the feature house's door is kept free of people
// (occupied); while the player's walk is worked out it is open ground.
static void FeatWalkBy(struct Gen *gen, struct FeatRoute *fr, bool8 occupied)
{
    const struct Building *b;
    if (!fr->hasLot)
        return;
    b = fr->lot.house;
    FeatSetOccupied(gen, (fr->bx + fr->lot.hx) * 2 + b->doorX, (fr->by + fr->lot.hy) * 2 + b->sy + FrontY(b), occupied);
}
