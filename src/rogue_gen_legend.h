// Legends: which legendary Pokemon a region has, where they wait and how
// the way to each opens. Included by rogue_gen_dungeon.h, after every
// other kind's planning.
//
// A real region has a handful, most of them late in the journey, and they
// belong together: Hoenn's three Regis in three sealed chambers and the Eon
// pair, Kanto's three birds, Johto's three beasts, Sinnoh's lake trio and
// Heatran by the volcano, Unova's Swords of Justice and Forces of Nature.
// So a region takes one trio (Regigigas may follow the Regis, as in
// Sinnoh), then a pair or a single to make three to five.
//
// Each waits in its chamber, a vanilla legend's room (Desert Ruins for
// Regirock, Terra Cave's end for Heatran, Southern Island for the Latis),
// at the end of a dungeon of its own: below a side cave, past the heart of a
// grove, or down a cave or two through a door in a town's rock face. The way
// in opens as in the real games (enum RogueLegendGate): the dungeon is
// trial enough, a field move breaks the way, old words name a move to use,
// a gym leader or a dungeon's prize gives the key, the villain leader
// stands in the way, or it shows itself once the rest of its set are met.
// Where the keys come from keeps them late.

#include "constants/items.h"

// Where a legend may live (besides a lair of its own) and the land it
// likes.
#define LHOME_CAVE   (1 << 0)
#define LHOME_FOREST (1 << 1)
#define LHOME_TOWER  (1 << 2)
#define LLAND_VOLCANO (1 << 3)
#define LLAND_SEA     (1 << 4)
#define LLAND_LAKE    (1 << 5)

struct LegendHome
{
    u8 homes; // LHOME_* bits, and the LLAND_* it likes
    u8 lair[2]; // the chamber it would have, and another if a legend before it took that one
};

static const struct LegendHome sLegendHomes[RLEG_COUNT] =
{
    [RLEG_REGIROCK]  = { LHOME_CAVE, { ROOM_STYLE_RUINS, ROOM_STYLE_CHAMBER } },
    [RLEG_REGICE]    = { LHOME_CAVE | LLAND_SEA, { ROOM_STYLE_ISLAND_CAVE, ROOM_STYLE_CHAMBER } },
    [RLEG_REGISTEEL] = { LHOME_CAVE, { ROOM_STYLE_TOMB, ROOM_STYLE_CHAMBER } },
    [RLEG_REGIGIGAS] = { 0, { ROOM_STYLE_CHAMBER, ROOM_STYLE_ORIGIN } },
    [RLEG_ARTICUNO]  = { LHOME_CAVE | LLAND_SEA, { ROOM_STYLE_MARINE, ROOM_STYLE_ISLAND_CAVE } },
    [RLEG_ZAPDOS]    = { LHOME_TOWER | LHOME_FOREST, { ROOM_STYLE_PEAK, ROOM_STYLE_PILLAR } },
    [RLEG_MOLTRES]   = { LHOME_CAVE | LLAND_VOLCANO, { ROOM_STYLE_TERRA, ROOM_STYLE_PEAK } },
    [RLEG_RAIKOU]    = { LHOME_TOWER | LHOME_FOREST, { ROOM_STYLE_PEAK, ROOM_STYLE_HOLLOW } },
    [RLEG_ENTEI]     = { LHOME_TOWER | LHOME_CAVE | LLAND_VOLCANO, { ROOM_STYLE_TERRA, ROOM_STYLE_DEN } },
    [RLEG_SUICUNE]   = { LHOME_TOWER | LHOME_FOREST | LHOME_CAVE | LLAND_LAKE | LLAND_SEA, { ROOM_STYLE_MARINE, ROOM_STYLE_ISLE } },
    [RLEG_UXIE]      = { LHOME_CAVE | LLAND_LAKE, { ROOM_STYLE_MARINE, ROOM_STYLE_HOLLOW } },
    [RLEG_MESPRIT]   = { LHOME_CAVE | LLAND_LAKE, { ROOM_STYLE_HOLLOW, ROOM_STYLE_MARINE } },
    [RLEG_AZELF]     = { LHOME_CAVE | LLAND_LAKE, { ROOM_STYLE_DEN, ROOM_STYLE_MARINE } },
    [RLEG_COBALION]  = { LHOME_CAVE | LHOME_FOREST, { ROOM_STYLE_DEN, ROOM_STYLE_HOLLOW } },
    [RLEG_TERRAKION] = { LHOME_CAVE, { ROOM_STYLE_HOLLOW, ROOM_STYLE_CHAMBER } },
    [RLEG_VIRIZION]  = { LHOME_FOREST, { ROOM_STYLE_GLADE, ROOM_STYLE_ISLE } },
    [RLEG_TORNADUS]  = { LHOME_TOWER | LHOME_FOREST, { ROOM_STYLE_PEAK, ROOM_STYLE_PILLAR } },
    [RLEG_THUNDURUS] = { LHOME_TOWER | LHOME_FOREST, { ROOM_STYLE_PILLAR, ROOM_STYLE_PEAK } },
    [RLEG_LANDORUS]  = { LHOME_CAVE, { ROOM_STYLE_RUINS, ROOM_STYLE_CHAMBER } },
    [RLEG_LATIAS]    = { LHOME_TOWER | LHOME_FOREST | LLAND_SEA, { ROOM_STYLE_ISLE, ROOM_STYLE_GLADE } },
    [RLEG_LATIOS]    = { LHOME_TOWER | LHOME_FOREST | LLAND_SEA, { ROOM_STYLE_ISLE, ROOM_STYLE_GLADE } },
    [RLEG_HEATRAN]   = { LLAND_VOLCANO, { ROOM_STYLE_TERRA, ROOM_STYLE_DEN } },
    [RLEG_CRESSELIA] = { LHOME_TOWER | LHOME_FOREST, { ROOM_STYLE_GLADE, ROOM_STYLE_ISLE } },
    [RLEG_MEWTWO]    = { 0, { ROOM_STYLE_ORIGIN, ROOM_STYLE_HOLLOW } },
    [RLEG_LUGIA]     = { 0, { ROOM_STYLE_DEPTHS, ROOM_STYLE_MARINE } },
    [RLEG_HO_OH]     = { 0, { ROOM_STYLE_PEAK, ROOM_STYLE_PILLAR } },
    [RLEG_KYOGRE]    = { 0, { ROOM_STYLE_MARINE, ROOM_STYLE_DEPTHS } },
    [RLEG_GROUDON]   = { 0, { ROOM_STYLE_TERRA, ROOM_STYLE_ORIGIN } },
    [RLEG_RAYQUAZA]  = { 0, { ROOM_STYLE_PILLAR, ROOM_STYLE_PEAK } },
};

// The trios a region is built round, and how likely each is.
static const u8 sLegendTrios[][4] =
{
    // members, weight
    { RLEG_REGIROCK, RLEG_REGICE, RLEG_REGISTEEL, 4 },
    { RLEG_ARTICUNO, RLEG_ZAPDOS, RLEG_MOLTRES, 3 },
    { RLEG_RAIKOU, RLEG_ENTEI, RLEG_SUICUNE, 3 },
    { RLEG_UXIE, RLEG_MESPRIT, RLEG_AZELF, 2 },
    { RLEG_COBALION, RLEG_TERRAKION, RLEG_VIRIZION, 2 },
    { RLEG_TORNADUS, RLEG_THUNDURUS, RLEG_LANDORUS, 1 },
};
static const u8 sLegendCovers[] = { RLEG_MEWTWO, RLEG_LUGIA, RLEG_HO_OH, RLEG_KYOGRE, RLEG_GROUDON, RLEG_RAYQUAZA };

#define MAX_REGION_LEGENDS 5
#define LEGEND_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

static bool8 IsWaterRoutePlace(const struct RoguePlace *p)
{
    return p->kind == RPLACE_ROUTE && (p->type == RROUTE_LAKE || p->type == RROUTE_RIVER);
}

// The land a dungeon lies in (LLAND_* bits): by a volcano, the sea or a lake.
static u8 DungeonLand(const struct RogueRegion *r, const struct RogueDungeon *d)
{
    const struct RoguePlace *host = &r->places[d->host];
    u8 land = 0, i;
    if (host->theme == RTHEME_ASH || host->theme == RTHEME_SPRING)
        land |= LLAND_VOLCANO;
    if (host->theme == RTHEME_HARBOR || host->theme == RTHEME_ISLAND || host->theme == RTHEME_BEACH
     || host->theme == RTHEME_COVE || host->seaSide != RDIR_NONE)
        land |= LLAND_SEA;
    if (IsWaterRoutePlace(host))
        land |= LLAND_LAKE;
    if (host->kind == RPLACE_CAVE)
    {
        if (host->caveIn < r->nPlaces && IsWaterRoutePlace(&r->places[host->caveIn]))
            land |= LLAND_LAKE;
        if (host->caveOut < r->nPlaces && IsWaterRoutePlace(&r->places[host->caveOut]))
            land |= LLAND_LAKE;
    }
    for (i = 0; i < host->nLinks; i++)
        if (host->links[i].target < r->nPlaces && IsWaterRoutePlace(&r->places[host->links[i].target]))
            land |= LLAND_LAKE;
    return land;
}

static u8 DungeonHomeBit(const struct RogueDungeon *d)
{
    switch (d->kind)
    {
    case RDUN_CAVE:   return LHOME_CAVE;
    case RDUN_FOREST: return LHOME_FOREST;
    case RDUN_TOWER:  return LHOME_TOWER;
    }
    return 0;
}

// How well a legend fits a dungeon already planned: 0 if not at all.
// Heatran lives only by a volcano.
static u8 LegendFits(const struct RogueRegion *r, const struct RogueDungeon *d, u8 legend)
{
    u8 homes = sLegendHomes[legend].homes, land = DungeonLand(r, d) & homes;
    // The ice cave keeps the ice legends, as Seafoam keeps Articuno; the
    // volcano the fire ones, as Mt. Ember keeps Moltres and Stark
    // Mountain Heatran.
    if (d->kind == RDUN_ICE)
        return (legend == RLEG_REGICE || legend == RLEG_ARTICUNO) ? 4 : 0;
    if (d->kind == RDUN_VOLCANO)
        return (legend == RLEG_MOLTRES || legend == RLEG_ENTEI || legend == RLEG_HEATRAN) ? 4 : 0;
    if (legend == RLEG_HEATRAN)
        return (d->kind == RDUN_CAVE && land) ? 3 : 0;
    if (!(homes & DungeonHomeBit(d)))
        return 0;
    return land ? 3 : 1;
}

// An optional dungeon whose end a legend may take: a side cave or a grove
// off a route, opened after the first gym, with a guardian or a master (who
// gives way), and a floor to spare for the chamber.
static bool8 LegendDungeonOk(const struct RogueRegion *r, const struct RogueDungeon *d)
{
    if (d->required || d->legend || d->badges < 1 || (d->exit != 0xFF && d->exit == d->host)
     || d->nFloors >= ROGUE_MAX_FLOORS)
        return FALSE;
    // Not a tower: its top is Mt. Pyre's summit, with no stairs on.
    if (d->kind != RDUN_CAVE && d->kind != RDUN_FOREST && d->kind != RDUN_ICE && d->kind != RDUN_VOLCANO)
        return FALSE;
    return d->boss == RBOSS_GUARDIAN || d->boss == RBOSS_MASTER || d->boss == RBOSS_NONE;
}

// A town that can take a lair: a door in a rock face along its top
// (TownDungeonEntrance's RENTRY_MESA), not the first town nor the League's.
static bool8 LairTownOk(const struct RogueRegion *r, u8 place)
{
    const struct RoguePlace *p = &r->places[place];
    int x0, w;
    if (p->kind != RPLACE_TOWN || place == r->start || p->type == RTOWN_START || p->type == RTOWN_SUMMIT
     || RogueGen_DungeonAt(r, place) >= 0 || TOWNS2_TOWN(p))
        return FALSE;
    return LairMesaSpan(r, p, &x0, &w);
}

// The legend's chamber is the dungeon's last floor, below the floors it
// had: a cave two at least, so the walk down is a dungeon of its own.
static void MakeLegend(struct RogueDungeon *d, u8 legend)
{
    d->boss = RBOSS_LEGEND;
    d->legend = legend;
    if (d->kind == RDUN_CAVE || d->kind == RDUN_VOLCANO)
    {
        // A floor down off the way through, for a cave on the way.
        if (d->required)
            d->nFloors++;
        else if (d->nFloors < 2)
            d->nFloors = 2;
    }
    if (d->nFloors < ROGUE_MAX_FLOORS)
        d->nFloors++;
}

// A lair for a legend in a town that can take one, near the badges asked
// for (a little shuffled): a door in the rock face, a cave floor or two
// down, and the chamber. Found early, it often waits for its key, as the
// Regis' chambers wait for the braille.
static bool8 AddLair(struct RogueRegion *r, u8 legend, u8 near, u32 *rng)
{
    int i, best = -1, bestScore = -1;
    for (i = 0; i < r->nPlaces; i++)
    {
        int score;
        if (!LairTownOk(r, i))
            continue;
        score = 40 - 4 * (r->places[i].badges > near ? r->places[i].badges - near : near - r->places[i].badges)
              + RogueRng_Range(rng, 10);
        if (score > bestScore)
        {
            best = i;
            bestScore = score;
        }
    }
    // One cave floor before the chamber, two in a later town.
    if (best < 0 || !AddDungeon(r, RDUN_LAIR, best, r->places[best].badges >= 4 ? 3 : 2, RBOSS_LEGEND, FALSE))
        return FALSE;
    r->dungeons[r->nDungeons - 1].legend = legend;
    return TRUE;
}

// Puts one legend somewhere: the end of an optional dungeon it fits (most
// of the time, so some keep their guardian or master), else a lair of its
// own, else below the main road's cave. FALSE if there is no room.
static bool8 PlaceLegend(struct RogueRegion *r, u8 legend, bool8 lairOnly, u8 near, u32 *rng)
{
    int i, best = -1, bestScore = 0;
    if (!lairOnly)
    {
        for (i = 0; i < r->nDungeons; i++)
        {
            struct RogueDungeon *d = &r->dungeons[i];
            int fit;
            if (!LegendDungeonOk(r, d) || !(fit = LegendFits(r, d, legend)))
                continue;
            fit = fit * 16 + d->badges * 2 + RogueRng_Range(rng, 4);
            if (fit > bestScore)
            {
                bestScore = fit;
                best = i;
            }
        }
        if (best >= 0 && RogueRng_Range(rng, 100) < 65)
        {
            MakeLegend(&r->dungeons[best], legend);
            return TRUE;
        }
    }
    if (AddLair(r, legend, near, rng))
        return TRUE;
    if (lairOnly)
        return FALSE;
    if (best >= 0)
    {
        MakeLegend(&r->dungeons[best], legend);
        return TRUE;
    }
    // Below the main road's cave, a floor deeper than the way through.
    for (i = 0; i < r->nDungeons; i++)
    {
        struct RogueDungeon *d = &r->dungeons[i];
        if ((d->kind == RDUN_CAVE || d->kind == RDUN_VOLCANO) && d->required && !d->legend && d->boss != RBOSS_ADMIN
         && d->nFloors + 2 <= ROGUE_MAX_FLOORS
         && (d->exit == 0xFF || d->exit != d->host) && (sLegendHomes[legend].homes & LHOME_CAVE))
        {
            MakeLegend(d, legend);
            return TRUE;
        }
    }
    // Last, the end of an optional ice cave or volcano, whatever the
    // legend: those kinds took the side caves legends once had, and a
    // region with no lair town left fell short of three. The chamber is
    // the legend's own, below.
    for (i = 0; i < r->nDungeons; i++)
    {
        struct RogueDungeon *d = &r->dungeons[i];
        if ((d->kind == RDUN_ICE || d->kind == RDUN_VOLCANO) && LegendDungeonOk(r, d))
        {
            MakeLegend(d, legend);
            return TRUE;
        }
    }
    return FALSE;
}

u8 RogueGen_LegendCount(const struct RogueRegion *r)
{
    u8 i, n = 0;
    for (i = 0; i < r->nDungeons; i++)
        n += r->dungeons[i].boss == RBOSS_LEGEND;
    return n;
}

// The main villain team's leader waits before the legend its grunts are
// after (its gate is ROGUE_GATE(RGATE_VILLAIN, 0); the other teams' are
// RogueGen_TeamLeaderDungeon's).
s32 RogueGen_LeaderDungeon(const struct RogueRegion *r)
{
    return RogueGen_TeamLeaderDungeon(r, 0);
}

bool8 RogueGen_HasChamber(const struct RogueDungeon *d)
{
    return d->boss == RBOSS_LEGEND && (d->kind == RDUN_LAIR || d->kind == RDUN_CAVE || d->kind == RDUN_FOREST
                                       || d->kind == RDUN_ICE || d->kind == RDUN_VOLCANO);
}

// A legend's chamber: its own, or its second if a legend before it in the
// region has that one.
static u8 LairStyle(const struct RogueRegion *r, u8 dungeon)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon];
    u8 i, want = sLegendHomes[d->legend % RLEG_COUNT].lair[0];
    for (i = 0; i < dungeon; i++)
        if (RogueGen_HasChamber(&r->dungeons[i]) && sLegendHomes[r->dungeons[i].legend % RLEG_COUNT].lair[0] == want)
            return sLegendHomes[d->legend % RLEG_COUNT].lair[1];
    return want;
}


// Picks the region's legends and where they sleep. Call after every other
// kind's planning and before FillDungeons.
static void PlanLegends(struct RogueRegion *r)
{
    u32 rng = RogueRng_Hash(r->seed, 11800);
    u8 want[MAX_REGION_LEGENDS + 1], n = 0, target, roll, i, total = 0, trio, placed = 0;

    // Three to five, most often four.
    roll = RogueRng_Range(&rng, 100);
    target = roll < 30 ? 3 : roll < 75 ? 4 : 5;
    for (i = 0; i < LEGEND_COUNT_OF(sLegendTrios); i++)
        total += sLegendTrios[i][3];
    roll = RogueRng_Range(&rng, total);
    for (trio = 0; roll >= sLegendTrios[trio][3]; trio++)
        roll -= sLegendTrios[trio][3];
    // The trio in a shuffled order, so the earliest home is not always the
    // same one's.
    for (i = 0; i < 3; i++)
        want[n++] = sLegendTrios[trio][i];
    for (i = 2; i > 0; i--)
    {
        u8 j = RogueRng_Range(&rng, i + 1), t = want[i];
        want[i] = want[j];
        want[j] = t;
    }
    // Then a pair or a single.
    if (target == 5)
    {
        want[n++] = RLEG_LATIAS;
        want[n++] = RLEG_LATIOS;
    }
    else if (target == 4)
    {
        static const u8 sSingles[] = { RLEG_HEATRAN, RLEG_CRESSELIA, RLEG_LATIAS, RLEG_LATIOS };
        want[n++] = trio == 0 && RogueRng_Range(&rng, 2) ? RLEG_REGIGIGAS : sSingles[RogueRng_Range(&rng, LEGEND_COUNT_OF(sSingles))];
    }
    for (i = 0; i < n; i++)
        placed += PlaceLegend(r, want[i], want[i] == RLEG_REGIGIGAS, 2 + (5 * i) / (n > 1 ? n - 1 : 1), &rng);
    // Sometimes a box-art legend, once the League's road is open, in a lair
    // of its own (as Mewtwo in Cerulean Cave).
    if (placed < MAX_REGION_LEGENDS && RogueRng_Range(&rng, 100) < 25)
        placed += PlaceLegend(r, sLegendCovers[RogueRng_Range(&rng, LEGEND_COUNT_OF(sLegendCovers))], TRUE, 7, &rng);
    // At least one, always: in any optional dungeon, or below the main
    // road's cave.
    for (i = 0; placed == 0 && i < r->nDungeons; i++)
    {
        struct RogueDungeon *d = &r->dungeons[i];
        if (LegendDungeonOk(r, d))
        {
            MakeLegend(d, d->kind == RDUN_FOREST ? RLEG_VIRIZION : RLEG_REGIROCK);
            placed = 1;
        }
    }
    if (placed == 0)
        PlaceLegend(r, RLEG_REGIROCK, FALSE, 4, &rng);
}

// ---------------------------------------------------------------------------
// How each legend's way opens

// The ways each legend's chamber may open, in the order it likes them, as
// its own games have them: the Regis' braille (Registeel's Ancient Tomb
// asks for FLY, Regice's chamber for DIVE), the Eon Ticket to the Latis'
// island, the Reveal Glass for the Forces of Nature, the orbs for Groudon
// and Kyogre, Regigigas once the Regis are met.
#define G_OPEN    ROGUE_GATE(RGATE_OPEN, 0)
#define G_SMASH   ROGUE_GATE(RGATE_FIELD, ROBST_ROCK_SMASH)
#define G_STR     ROGUE_GATE(RGATE_FIELD, ROBST_STRENGTH)
#define G_FLY     ROGUE_GATE(RGATE_RITUAL, RRITUAL_FLY)
#define G_DIVE    ROGUE_GATE(RGATE_RITUAL, RRITUAL_DIVE)
#define G_FALLS   ROGUE_GATE(RGATE_RITUAL, RRITUAL_WATERFALL)
#define G_GIFT    ROGUE_GATE(RGATE_GIFT, 0)
#define G_FOUND   ROGUE_GATE(RGATE_FOUND, 0)
#define G_VILLAIN ROGUE_GATE(RGATE_VILLAIN, 0)
#define G_AFTER   ROGUE_GATE(RGATE_AFTER, 0)
#define G_END     0xFF

static const u8 sLegendGates[RLEG_COUNT][5] =
{
    [RLEG_REGIROCK]  = { G_SMASH, G_STR, G_FLY, G_OPEN, G_END },
    [RLEG_REGICE]    = { G_DIVE, G_SMASH, G_OPEN, G_END, G_END },
    [RLEG_REGISTEEL] = { G_FLY, G_STR, G_OPEN, G_END, G_END },
    [RLEG_REGIGIGAS] = { G_AFTER, G_FALLS, G_END, G_END, G_END },
    [RLEG_ARTICUNO]  = { G_OPEN, G_DIVE, G_STR, G_VILLAIN, G_END },
    [RLEG_ZAPDOS]    = { G_OPEN, G_FLY, G_SMASH, G_VILLAIN, G_END },
    [RLEG_MOLTRES]   = { G_OPEN, G_STR, G_FLY, G_VILLAIN, G_END },
    [RLEG_RAIKOU]    = { G_OPEN, G_SMASH, G_FLY, G_VILLAIN, G_END },
    [RLEG_ENTEI]     = { G_OPEN, G_STR, G_VILLAIN, G_END, G_END },
    [RLEG_SUICUNE]   = { G_DIVE, G_OPEN, G_VILLAIN, G_END, G_END },
    [RLEG_UXIE]      = { G_OPEN, G_VILLAIN, G_DIVE, G_END, G_END },
    [RLEG_MESPRIT]   = { G_OPEN, G_VILLAIN, G_FLY, G_END, G_END },
    [RLEG_AZELF]     = { G_OPEN, G_VILLAIN, G_SMASH, G_END, G_END },
    [RLEG_COBALION]  = { G_OPEN, G_STR, G_SMASH, G_FLY, G_END },
    [RLEG_TERRAKION] = { G_SMASH, G_OPEN, G_STR, G_DIVE, G_END },
    [RLEG_VIRIZION]  = { G_OPEN, G_FLY, G_SMASH, G_END, G_END },
    [RLEG_TORNADUS]  = { G_FLY, G_GIFT, G_OPEN, G_END, G_END },
    [RLEG_THUNDURUS] = { G_GIFT, G_FLY, G_OPEN, G_END, G_END },
    [RLEG_LANDORUS]  = { G_GIFT, G_SMASH, G_OPEN, G_END, G_END },
    [RLEG_LATIAS]    = { G_GIFT, G_END, G_END, G_END, G_END },
    [RLEG_LATIOS]    = { G_GIFT, G_END, G_END, G_END, G_END },
    [RLEG_HEATRAN]   = { G_FOUND, G_STR, G_VILLAIN, G_FLY, G_END },
    [RLEG_CRESSELIA] = { G_FOUND, G_GIFT, G_END, G_END, G_END },
    [RLEG_MEWTWO]    = { G_FOUND, G_FALLS, G_END, G_END, G_END },
    [RLEG_LUGIA]     = { G_GIFT, G_DIVE, G_END, G_END, G_END },
    [RLEG_HO_OH]     = { G_GIFT, G_FLY, G_END, G_END, G_END },
    [RLEG_KYOGRE]    = { G_FOUND, G_VILLAIN, G_DIVE, G_END, G_END },
    [RLEG_GROUDON]   = { G_FOUND, G_VILLAIN, G_STR, G_END, G_END },
    [RLEG_RAYQUAZA]  = { G_FALLS, G_FLY, G_END, G_END, G_END },
};

// The key a legend's gift or find is: real key items where the games have
// one (the Eon and Mystic Tickets, the Reveal Glass, the orbs), else one of
// a like story (the Ruby from Mt. Ember's depths for Heatran, the Old Sea
// Map to Faraway Island for Cresselia, the Secret Key for Mewtwo).
u16 RogueGen_LegendKey(u8 legend)
{
    switch (legend)
    {
    case RLEG_LATIAS:
    case RLEG_LATIOS:    return ITEM_EON_TICKET;
    case RLEG_LUGIA:
    case RLEG_HO_OH:     return ITEM_MYSTIC_TICKET;
    case RLEG_TORNADUS:
    case RLEG_THUNDURUS:
    case RLEG_LANDORUS:  return ITEM_REVEAL_GLASS;
    case RLEG_KYOGRE:    return ITEM_BLUE_ORB;
    case RLEG_GROUDON:   return ITEM_RED_ORB;
    case RLEG_HEATRAN:   return ITEM_RUBY;
    case RLEG_CRESSELIA: return ITEM_OLD_SEA_MAP;
    case RLEG_MEWTWO:    return ITEM_SECRET_KEY;
    }
    return ITEM_NONE;
}

// The set a legend belongs to (its trio's index), for RGATE_AFTER:
// Regigigas belongs with the Regis.
u8 RogueGen_LegendSetOf(u8 legend)
{
    u8 t, k;
    if (legend == RLEG_REGIGIGAS)
        return 0;
    for (t = 0; t < LEGEND_COUNT_OF(sLegendTrios); t++)
        for (k = 0; k < 3; k++)
            if (sLegendTrios[t][k] == legend)
                return t;
    return 0xFF;
}

// The gym whose badge gives a field move (ROBST_*), or 0xFF.
static u8 GymOfKey(const struct RogueRegion *r, u8 obstacle)
{
    u8 g;
    for (g = 0; g < ROGUE_NUM_GYMS; g++)
        if (r->gymKey[g] == obstacle)
            return g;
    return 0xFF;
}

static const u8 sRitualBadges[] = { [RRITUAL_FLY] = 6, [RRITUAL_DIVE] = 7, [RRITUAL_WATERFALL] = 8 };

// The badges held when a gate opens, before the home's own are counted.
static u8 GateOpensAt(const struct RogueRegion *r, u8 dungeon, u8 gate)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon];
    u8 src = ROGUE_GATE_SRC(gate), most = 0, i;
    s32 h;
    switch (ROGUE_GATE_KIND(gate))
    {
    case RGATE_FIELD:
        i = GymOfKey(r, src);
        return i == 0xFF ? ROGUE_NUM_GYMS : i + 1;
    case RGATE_RITUAL:
        return sRitualBadges[src < 3 ? src : 2];
    case RGATE_GIFT:
        return src + 1;
    case RGATE_FOUND:
        return src < r->nDungeons ? r->dungeons[src].badges + 1 : ROGUE_NUM_GYMS;
    case RGATE_VILLAIN:
        h = RogueGen_TeamHideout(r, src);
        return h < 0 ? ROGUE_NUM_GYMS : r->places[r->dungeons[h].host].badges + 1;
    case RGATE_AFTER:
        for (i = 0; i < r->nDungeons; i++)
        {
            const struct RogueDungeon *o = &r->dungeons[i];
            if (i != dungeon && o->boss == RBOSS_LEGEND && ROGUE_GATE_KIND(o->gate) != RGATE_AFTER
             && RogueGen_LegendSetOf(o->legend) == RogueGen_LegendSetOf(d->legend))
            {
                u8 b = GateOpensAt(r, i, o->gate);
                if (b < o->badges)
                    b = o->badges;
                if (b > most)
                    most = b;
            }
        }
        return most;
    }
    return d->badges;
}

u8 RogueGen_LegendGateBadges(const struct RogueRegion *r, u8 dungeon)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon % ROGUE_MAX_DUNGEONS];
    u8 b = GateOpensAt(r, dungeon % ROGUE_MAX_DUNGEONS, d->gate);
    if (b < d->badges)
        b = d->badges;
    if (b < 1)
        b = 1;
    return b > ROGUE_NUM_GYMS ? ROGUE_NUM_GYMS : b;
}

// TRUE if the others of a legend's set are in the region with gates of
// their own, so it can wait for them.
static bool8 SetOthersHere(const struct RogueRegion *r, u8 dungeon)
{
    u8 i, n = 0;
    if (RogueGen_LegendSetOf(r->dungeons[dungeon].legend) == 0xFF)
        return FALSE;
    for (i = 0; i < r->nDungeons; i++)
        if (i != dungeon && r->dungeons[i].boss == RBOSS_LEGEND
         && RogueGen_LegendSetOf(r->dungeons[i].legend) == RogueGen_LegendSetOf(r->dungeons[dungeon].legend))
            n++;
    return n >= 2;
}

// A dungeon whose prize ball may hold a key: a cave, grove or tower without
// a legend, opened after the second badge, with a prize and items to keep
// it in (FillDungeons gave them).
static bool8 KeyDungeonOk(const struct RogueRegion *r, u8 i, u16 taken)
{
    const struct RogueDungeon *d = &r->dungeons[i];
    struct RogueDungeonExtras e;
    if (d->boss == RBOSS_LEGEND || d->kind == RDUN_HIDEOUT || d->kind == RDUN_VICTORY || d->badges < 2 || ((taken >> i) & 1))
        return FALSE;
    RogueGen_DungeonExtras(r, i, &e);
    return e.prize != 0xFF;
}

// How well a legend suits a villain team: a sea one for Team Aqua, one of
// fire or earth for Team Magma (as Maxie's Groudon), and for Team Rocket
// one worth a fortune to sell: the birds and the beasts.
static bool8 SuitsVillains(u8 id, u8 legend)
{
    u8 land = sLegendHomes[legend % RLEG_COUNT].homes;
    if (id == ROGUE_VILLAIN_AQUA)
        return (land & (LLAND_SEA | LLAND_LAKE)) || legend == RLEG_KYOGRE || legend == RLEG_LUGIA;
    if (id == ROGUE_VILLAIN_ROCKET)
        return (legend >= RLEG_ARTICUNO && legend <= RLEG_SUICUNE) || legend == RLEG_MEWTWO;
    return (land & LLAND_VOLCANO) || legend == RLEG_GROUDON || legend == RLEG_REGIROCK
        || legend == RLEG_TERRAKION || legend == RLEG_LANDORUS;
}

// The legend a team's leader waits before: of those whose way a villain may
// keep, the one that suits it best, the latest home of those, not another
// team's (taken: a bit a dungeon). 0xFF for none.
static u8 TeamLegend(const struct RogueRegion *r, u8 id, const u8 *order, u8 n, u16 taken)
{
    u8 i, k, pick = 0xFF;
    int bestFit = -1;
    for (i = 0; i < n; i++)
    {
        const struct RogueDungeon *d = &r->dungeons[order[i]];
        int fit = SuitsVillains(id, d->legend) * 16 + d->badges;
        if ((taken >> order[i]) & 1)
            continue;
        for (k = 0; k < 5 && sLegendGates[d->legend][k] != G_END; k++)
            if (sLegendGates[d->legend][k] == G_VILLAIN)
                break;
        if (k == 5 || sLegendGates[d->legend][k] == G_END)
            continue;
        if (fit > bestFit)
        {
            bestFit = fit;
            pick = order[i];
        }
    }
    return pick;
}

// Picks each legend's gate. Called after FillDungeons, as a key may lie in
// another dungeon's prize ball. A region's legends open in different ways
// where they can, and late: a gate that opens with more badges than the
// home is reached at scores better, the more so past the fourth badge.
static void PlanLegendGates(struct RogueRegion *r)
{
    u32 rng = RogueRng_Hash(r->seed, 11900);
    u8 order[ROGUE_MAX_DUNGEONS], n = 0, i, j, used = 0, villain[ROGUE_MAX_TEAMS], teams = RogueGen_TeamCount(r), t;
    u16 keyTaken = 0; // (a bit per dungeon: ten of them, so not a u8; v1.1 towns2, seed 121)
    u8 giftGym[ROGUE_NUM_GYMS];
    u16 taken = 0;

    memset(giftGym, 0, sizeof(giftGym));
    for (i = 0; i < r->nDungeons; i++)
        if (r->dungeons[i].boss == RBOSS_LEGEND)
            order[n++] = i;
    // By the badges their homes open at, so the earliest pick first.
    for (i = 1; i < n; i++)
        for (j = i; j > 0 && r->dungeons[order[j]].badges < r->dungeons[order[j - 1]].badges; j--)
        {
            u8 t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
        }
    // The legend the main villain team is after, most of the time: the one
    // that suits it best, the latest home of those (always for Team Rocket,
    // whose boss is met nowhere else). The other teams' leaders each wait
    // before another, where there is one.
    for (t = 0; t < ROGUE_MAX_TEAMS; t++)
        villain[t] = 0xFF;
    if (teams > 0)
    {
        u8 roll = RogueRng_Range(&rng, 100);
        if (roll < 80 || RogueGen_TeamId(r, 0) == ROGUE_VILLAIN_ROCKET)
            villain[0] = TeamLegend(r, RogueGen_TeamId(r, 0), order, n, 0);
        if (villain[0] != 0xFF)
            taken |= 1 << villain[0];
        for (t = 1; t < teams; t++)
        {
            villain[t] = TeamLegend(r, RogueGen_TeamId(r, t), order, n, taken);
            if (villain[t] != 0xFF)
                taken |= 1 << villain[t];
        }
    }
    for (i = 0; i < n; i++)
    {
        struct RogueDungeon *d = &r->dungeons[order[i]];
        int best = -1000, k;
        u8 pick = G_OPEN;
        d->gate = G_OPEN;
        if ((taken >> order[i]) & 1)
        {
            for (t = 0; t < teams && villain[t] != order[i]; t++)
                ;
            d->gate = ROGUE_GATE(RGATE_VILLAIN, t);
            used |= 1 << RGATE_VILLAIN;
            continue;
        }
        for (k = 0; k < 5 && sLegendGates[d->legend][k] != G_END; k++)
        {
            u8 gate = sLegendGates[d->legend][k], kind = ROGUE_GATE_KIND(gate), at;
            int off;
            int score = 12 - 3 * k + RogueRng_Range(&rng, 6);
            switch (kind)
            {
            case RGATE_OPEN:
                // The dungeon alone is trial enough only well on.
                if (d->badges < 4)
                    continue;
                break;
            case RGATE_FIELD:
                if (GymOfKey(r, ROGUE_GATE_SRC(gate)) == 0xFF)
                    continue;
                break;
            case RGATE_GIFT:
            {
                // A leader from the fourth gym to the seventh (the last for
                // a box-art legend), after the home opens; one key a gym,
                // and the Eon pair share theirs.
                u8 g, lo = d->badges > 3 ? d->badges : 3, hi = d->legend >= RLEG_MEWTWO ? 7 : 6;
                u16 key = RogueGen_LegendKey(d->legend);
                u8 found = 0xFF;
                for (g = 0; g < ROGUE_NUM_GYMS; g++)
                    if (giftGym[g] && RogueGen_LegendKey(giftGym[g]) == key)
                        found = g;
                if (found == 0xFF)
                {
                    if (lo > hi)
                        lo = hi;
                    for (j = 0; j < 8 && found == 0xFF; j++)
                    {
                        g = lo + RogueRng_Range(&rng, hi - lo + 1);
                        if (!giftGym[g])
                            found = g;
                    }
                }
                if (found == 0xFF)
                    continue;
                gate = ROGUE_GATE(RGATE_GIFT, found);
                break;
            }
            case RGATE_FOUND:
            {
                // Another dungeon's prize: a tower's top for an orb, as
                // Mt. Pyre's, else the latest that can hold it.
                int bestK = -1, bestS = -1;
                for (j = 0; j < r->nDungeons; j++)
                {
                    int s;
                    if (!KeyDungeonOk(r, j, keyTaken))
                        continue;
                    s = r->dungeons[j].badges * 4 + RogueRng_Range(&rng, 4)
                      + ((d->legend == RLEG_KYOGRE || d->legend == RLEG_GROUDON) && r->dungeons[j].kind == RDUN_TOWER ? 40 : 0);
                    if (s > bestS)
                    {
                        bestS = s;
                        bestK = j;
                    }
                }
                if (bestK < 0)
                    continue;
                gate = ROGUE_GATE(RGATE_FOUND, bestK);
                break;
            }
            case RGATE_VILLAIN:
                continue; // only the one the team is after
            case RGATE_AFTER:
                // Regigigas waits for the Regis (the last of a trio may
                // wait for the others: below).
                if (d->legend != RLEG_REGIGIGAS || !SetOthersHere(r, order[i]))
                    continue;
                score += 20;
                break;
            }
            // Spread over the run from the fourth badge to the eighth, by
            // the order the homes open (as the Regis wait for the braille):
            // the nearer the better.
            at = GateOpensAt(r, order[i], gate);
            if (at <= d->badges)
                score -= 8;
            off = (int)at - (n == 1 ? 6 : 4 + (4 * i + (n - 1) / 2) / (n - 1));
            score -= 2 * (off < 0 ? -off : off);
            if ((used >> kind) & 1)
                score -= 9;
            if (score > best)
            {
                best = score;
                pick = gate;
            }
        }
        d->gate = pick;
        used |= 1 << ROGUE_GATE_KIND(pick);
        if (ROGUE_GATE_KIND(pick) == RGATE_GIFT)
            giftGym[ROGUE_GATE_SRC(pick)] = d->legend;
        if (ROGUE_GATE_KIND(pick) == RGATE_FOUND)
            keyTaken |= 1 << ROGUE_GATE_SRC(pick);
    }
    // The last of a trio found may wait for the other two (Landorus for
    // the Forces most of all), as Regigigas for the Regis.
    for (i = n; i-- > 0;)
    {
        struct RogueDungeon *d = &r->dungeons[order[i]];
        u8 k, chance = d->legend == RLEG_LANDORUS ? 70 : 35, others = 0;
        if (ROGUE_GATE_KIND(d->gate) == RGATE_AFTER || ROGUE_GATE_KIND(d->gate) == RGATE_VILLAIN
         || !SetOthersHere(r, order[i]) || d->legend == RLEG_REGIGIGAS)
            continue;
        // (Not where Regigigas already waits for this set.)
        for (k = 0; k < n; k++)
            if (k != i && RogueGen_LegendSetOf(r->dungeons[order[k]].legend) == RogueGen_LegendSetOf(d->legend))
                others += ROGUE_GATE_KIND(r->dungeons[order[k]].gate) == RGATE_AFTER ? 10 : 1;
        if (others == 2 && RogueRng_Range(&rng, 100) < chance)
        {
            if (ROGUE_GATE_KIND(d->gate) == RGATE_FOUND)
                keyTaken &= ~(1 << ROGUE_GATE_SRC(d->gate));
            d->gate = G_AFTER;
        }
        break;
    }
}
