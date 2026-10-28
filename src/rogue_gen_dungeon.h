// Dungeons: planning them and generating their floors. Included by
// rogue_gen.c, so it shares the generator's helpers.
//
// A dungeon hangs off a host place. Its floors are generated like places,
// from a place made up for each floor (RogueGen_FloorPlace), so the code that
// fills caves with trainers and items serves them too.

static ROGUE_EWRAM struct RoguePlace sFloorGenPlace;

// The badges the player holds, for what follows them (an optional
// dungeon's and a tunnel's levels). gen_host sets them only to print the
// levels a floor takes with each count, after the region is made.
#ifdef ROGUE_HOST
static u8 sHostBadges;
static u32 HeldBadgeCount(void)
{
    return sHostBadges;
}
#else
u32 Rogue_BadgeCount(void);
#define HeldBadgeCount Rogue_BadgeCount
#endif

static void ForestFloorPlace(struct RoguePlace *out, u32 k);

s32 RogueGen_DungeonAt(const struct RogueRegion *r, u8 place)
{
    int i;
    for (i = 0; i < r->nDungeons; i++)
        if (r->dungeons[i].host == place || r->dungeons[i].exit == place)
            return i;
    return -1;
}

// ---------------------------------------------------------------------------
// Dungeon content: extras, bosses and the legend

s32 RogueGen_FossilCave(const struct RogueRegion *r)
{
    int i;
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        if (p->kind == RPLACE_CAVE && !p->side && p->badges == 3 && p->nItems != 0)
            return i;
    }
    return -1;
}

s32 RogueGen_LegendDungeon(const struct RogueRegion *r)
{
    int i;
    for (i = 0; i < r->nDungeons; i++)
        if (r->dungeons[i].boss == RBOSS_LEGEND)
            return i;
    return -1;
}

static bool8 IsRoomDungeon(const struct RogueDungeon *d);

// The floors generated as caves or woods: all but a legend's chamber.
static u8 DungeonGenFloors(const struct RogueDungeon *d)
{
    return d->nFloors - (RogueGen_HasChamber(d) && d->nFloors > 1);
}
static bool8 RoomHasPrize(const struct RogueRegion *r, u8 dungeon);
static s32 ExplorerDungeon(const struct RogueRegion *r);
static u8 ExplorerFloorOf(const struct RogueRegion *r, u8 dungeon);
static u8 RoomChoiceFloor(const struct RogueRegion *r, u8 dungeon, u8 *n, u32 *rng);
static u8 RoomCampFloor(const struct RogueRegion *r, u8 dungeon);
static u8 DungeonKeyCount(const struct RogueRegion *r, u8 dungeon);
static u8 DungeonDecoyCount(const struct RogueRegion *r, u8 dungeon);

// The extras a dungeon has, with slots counted from 0, before item flags are
// given out. Uses only the plan (and the rooms a tower's floors are), so it
// is the same before and after FillDungeons. What each kind gets:
//
//   prize   beside the boss on the last floor: a cave's guardian, master or
//           legend, a grove's guardian, a tower's master, the hideout's
//           admin (the team's stolen goods). Not a master by the way out of
//           a cave on the way on, as on Victory Road: he stands on the cave
//           itself, not on a floor, and hands over his own reward.
//   choice  the fossil cave's always, like Mt. Moon's fossils; half the
//           optional caves, groves and towers (a tower on a floor whose room
//           has a row for it). Never on the way on, nor in the hideout.
//   camp    halfway down a deep optional dungeon: a cave of three floors or
//           more, a grove of two, a tower of four or more (on a middle floor
//           whose room has a place for the caretaker); and on the first
//           floor of a cave on the way on and the deepest of Victory Road,
//           the longest walks between Pokemon Centers. Never in the hideout.
//   explorer someone lost deep in the region's one dungeon that has them
//           (src/rogue_gen_ruins.h).
//
// Ruins always have the prize and a choice of stones in the vault, behind
// the riddle door, and no camp.
static void AddExplorer(const struct RogueRegion *r, u8 i, struct RogueDungeonExtras *e, u8 *n)
{
    if (ExplorerDungeon(r) != i)
        return;
    e->explorerFloor = ExplorerFloorOf(r, i);
    e->explorer = (*n)++;
}

static void PlanExtras(const struct RogueRegion *r, u8 i, struct RogueDungeonExtras *e)
{
    const struct RogueDungeon *d = &r->dungeons[i];
    u32 rng = RogueRng_Hash(r->seed, 11600 + i);
    // (The main road's cave keeps its camp and fossils as the volcano's way.)
    bool8 cave = d->kind == RDUN_CAVE || d->kind == RDUN_VICTORY || d->kind == RDUN_VOLCANO;
    u8 n = 0;

    u8 floors = DungeonGenFloors(d);

    memset(e, 0, sizeof(*e));
    e->prize = e->choice = e->camp = e->explorer = e->keys = e->decoys = 0xFF;
    // A legend's prize lies in its chamber, where the room has a place.
    if (d->boss != RBOSS_NONE && !(cave && d->required && BossIsTrainer(d))
     && (!(IsRoomDungeon(d) || RogueGen_HasChamber(d)) || RoomHasPrize(r, i)))
        e->prize = n++;
    // A lair's cave floors are a short walk: the prize is enough.
    if (d->kind == RDUN_LAIR || d->kind == RDUN_RUINS)
    {
        if (d->kind == RDUN_RUINS)
        {
            e->nChoice = 2 + RogueRng_Range(&rng, 2);
            e->choiceFloor = d->nFloors;
            e->choice = n;
            n += e->nChoice;
        }
        AddExplorer(r, i, e, &n);
        e->total = n;
        return;
    }
    if (d->kind == RDUN_MIRAGE)
    {
        // (v1.1 peaks) The desert tower's two fossils at its top, one to
        // take, as the Mirage Tower's.
        e->nChoice = 2;
        e->choiceFloor = d->nFloors;
    }
    else if ((d->kind == RDUN_CAVE || d->kind == RDUN_VOLCANO) && d->host == RogueGen_FossilCave(r))
    {
        // Two fossils when the FOSSILS unlock is on (the game decides
        // what is in it).
        e->nChoice = 2;
        e->choiceFloor = 1;
    }
    else if (!d->required && d->kind != RDUN_VICTORY && d->kind != RDUN_HIDEOUT && RogueRng_Range(&rng, 100) < 50)
    {
        e->nChoice = 2 + RogueRng_Range(&rng, 2);
        if (IsRoomDungeon(d))
            e->choiceFloor = RoomChoiceFloor(r, i, &e->nChoice, &rng);
        else
            e->choiceFloor = 1 + RogueRng_Range(&rng, floors);
    }
    if (e->nChoice)
    {
        e->choice = n;
        n += e->nChoice;
    }
    if (!d->required && d->kind != RDUN_HIDEOUT)
    {
        if (IsRoomDungeon(d))
            e->campFloor = d->nFloors >= 4 ? RoomCampFloor(r, i) : 0;
        else if (floors >= (d->kind == RDUN_FOREST ? 2 : 3))
            e->campFloor = (floors + 1) / 2;
        if (e->campFloor)
            e->camp = n++;
    }
    else if (d->required && cave)
    {
        // Victory Road's on its deepest floor, near the far end of the
        // walk; the main road cave's on its first.
        e->campFloor = d->kind == RDUN_VICTORY ? floors : 1;
        e->camp = n++;
    }
    AddExplorer(r, i, e, &n);
    // A shipwreck's cabin keys and a power plant's Voltorb balls: item
    // balls of their own.
    e->nKeys = DungeonKeyCount(r, i);
    if (e->nKeys)
    {
        e->keys = n;
        n += e->nKeys;
    }
    e->nDecoys = DungeonDecoyCount(r, i);
    if (e->nDecoys)
    {
        e->decoys = n;
        n += e->nDecoys;
    }
    e->total = n;
}

void RogueGen_DungeonExtras(const struct RogueRegion *r, u8 dungeon, struct RogueDungeonExtras *e)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon % ROGUE_MAX_DUNGEONS];
    u8 base;

    PlanExtras(r, dungeon % ROGUE_MAX_DUNGEONS, e);
    if (d->nItems < e->total)
    {
        // Out of item flags: no items at all, so no extras either.
        memset(e, 0, sizeof(*e));
        e->prize = e->choice = e->camp = e->explorer = e->keys = e->decoys = 0xFF;
        return;
    }
    base = d->itemBase + d->nItems - e->total;
    if (e->keys != 0xFF)
        e->keys += base;
    if (e->decoys != 0xFF)
        e->decoys += base;
    if (e->prize != 0xFF)
        e->prize += base;
    if (e->choice != 0xFF)
        e->choice += base;
    if (e->camp != 0xFF)
        e->camp += base;
    if (e->explorer != 0xFF)
        e->explorer += base;
}

// The share of a dungeon's trainers (not the boss) or items on one floor.
static void FloorShare(u8 total, u8 nFloors, u8 floor, u8 *first, u8 *count)
{
    u8 a = ((floor - 1) * total) / nFloors, b = (floor * total) / nFloors;
    *first = a;
    *count = b - a;
}

static bool8 BossIsTrainer(const struct RogueDungeon *d)
{
    return d->boss == RBOSS_MASTER || d->boss == RBOSS_ADMIN;
}

#include "rogue_gen_rooms.h"
#include "rogue_gen_ruins.h"
#include "rogue_gen_peaks.h"

// The first of a floor's trainers (not the boss) or items among the
// dungeon's, and how many: shared evenly, or as a tower's or hideout's rooms
// have spots for.
static void FloorSlots(const struct RogueRegion *r, u8 dungeon, u8 floor, bool8 items, u8 *first, u8 *count)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon % ROGUE_MAX_DUNGEONS];
    if (IsRoomDungeon(d))
    {
        RoomFloorSlots(r, dungeon % ROGUE_MAX_DUNGEONS, floor, items, first, count);
        if ((items ? d->nItems : d->nTrainers) == 0) // over the region's budget (FillDungeons)
            *count = 0;
    }
    else if (floor > DungeonGenFloors(d))
    {
        // A legend's chamber: only the legend, and the prize.
        *first = items ? d->nItems : d->nTrainers;
        *count = 0;
    }
    else if (items)
    {
        // The extras (prize, choice, camp) take the last of the items.
        struct RogueDungeonExtras e;
        RogueGen_DungeonExtras(r, dungeon % ROGUE_MAX_DUNGEONS, &e);
        if (IsRuins(d))
            RuinsFloorSlots(d, floor, TRUE, d->nItems - e.total, first, count);
        else
            FloorShare(d->nItems - e.total, DungeonGenFloors(d), floor, first, count);
    }
    else if (IsRuins(d))
        RuinsFloorSlots(d, floor, FALSE, d->nTrainers, first, count);
    else
        FloorShare(d->nTrainers - BossIsTrainer(d), DungeonGenFloors(d), floor, first, count);
}

u8 RogueGen_FloorOfTrainer(const struct RogueRegion *r, u8 dungeon, u8 slot)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon];
    u8 f, n = d->nTrainers - BossIsTrainer(d);
    if (slot < d->trainerBase || slot >= d->trainerBase + d->nTrainers)
        return 0;
    slot -= d->trainerBase;
    if (slot >= n)
        return d->nFloors;
    for (f = 1; f <= d->nFloors; f++)
    {
        u8 first, count;
        FloorSlots(r, dungeon, f, FALSE, &first, &count);
        if (slot >= first && slot < first + count)
            return f;
    }
    return d->nFloors;
}

// A floor's level. Floors climb a level each, as Emerald's do (Granite
// Cave, Mt. Pyre, Victory Road barely climb). An optional dungeon follows
// the badges held when it is entered, never under its own level: one
// visited late is as tense as the road there then (the stretch the badges
// open, two up, as an optional dungeon is a step up from its host). A
// tunnel's floors follow the badges too, from the stretch's own level: it
// is walked from either end, early or late. A villain team's hideout is
// manned like the gym it stands before (below). (The generator itself never asks: no badges.)
u8 RogueGen_FloorLevel(const struct RogueRegion *r, u8 dungeon, u8 floor)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon % ROGUE_MAX_DUNGEONS];
    u8 level = d->level + (floor - 1), held;
    // A villain team's hideout is manned like the gym it stands before: its
    // grunts from seven under that gym's ace, a level more each floor down
    // (as a gym's trainers stand), and an optional one by the badges held
    // once the player has more.
    if (d->kind == RDUN_HIDEOUT)
    {
        u8 b = d->badges, target;
        held = HeldBadgeCount();
        if (!d->required && held > b)
            b = held;
        if (b >= ROGUE_NUM_GYMS)
            b = ROGUE_NUM_GYMS - 1;
        target = r->aceLevel[b] > 7 ? r->aceLevel[b] - 7 + (floor - 1) : level;
        return target > level ? target : level;
    }
    if (d->required || d->kind >= RDUN_COUNT)
        return level;
    held = HeldBadgeCount();
    if (held > d->badges || dungeon == r->tunnel)
    {
        u8 now = RogueGen_StretchLevel(r, held) + (floor - 1);
        if (dungeon != r->tunnel)
            now += held <= 1 ? 0 : 2;
        if (now > level)
            level = now;
    }
    return level;
}

void RogueGen_FloorPlace(const struct RogueRegion *r, u8 dungeon, u8 floor, struct RoguePlace *out)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon % ROGUE_MAX_DUNGEONS];
    const struct RoguePlace *host = &r->places[d->host];
    u32 rng = RogueRng_Hash(r->seed, 12000 + dungeon * 16 + floor);
    u8 first, count;
    // Floor sizes: wide, tall and square, like the caves.
    static const u8 sFloorSizes[][2] = { { 30, 26 }, { 26, 32 }, { 30, 30 }, { 34, 26 }, { 28, 28 } };
    u32 k = RogueRng_Range(&rng, sizeof(sFloorSizes) / sizeof(sFloorSizes[0]));

    if (floor < 1)
        floor = 1;
    if (floor > d->nFloors)
        floor = d->nFloors;
    memset(out, 0, sizeof(*out));
    out->kind = RPLACE_CAVE;
    out->type = host->type;
    out->theme = host->theme;
    out->number = host->number;
    out->w = sFloorSizes[k][0];
    out->h = sFloorSizes[k][1];
    if (d->kind == RDUN_VICTORY)
    {
        out->w += 8;
        out->h += 6;
    }
    // Room for an ice floor's puzzle chamber besides the rest.
    if (d->kind == RDUN_ICE)
    {
        out->w += 6;
        out->h += 6;
    }
    if (d->kind == RDUN_FOREST)
        ForestFloorPlace(out, k);
    // (v1.1 peaks) A tower's misty mountainside.
    if (floor == RogueGen_PyreSlopeFloor(r, dungeon % ROGUE_MAX_DUNGEONS))
        PyreSlopePlace(out);
    out->gridX = host->gridX;
    out->gridY = host->gridY;
    out->gym = 0xFF;
    out->level = RogueGen_FloorLevel(r, dungeon % ROGUE_MAX_DUNGEONS, floor);
    out->tier = TierOfLevel(out->level);
    out->badges = d->badges;
    out->area = d->area;
    if (RogueGen_FloorRoom(r, dungeon % ROGUE_MAX_DUNGEONS, floor) != 0xFF)
    {
        const struct RogueRoom *room = &sRogueRooms[RogueGen_FloorRoom(r, dungeon % ROGUE_MAX_DUNGEONS, floor)];
        out->w = room->w;
        out->h = room->h;
    }
    FloorSlots(r, dungeon, floor, FALSE, &first, &count);
    out->trainerBase = d->trainerBase + first;
    out->nTrainers = count;
    // A trainer boss waits on the last floor, after the rest.
    if (floor == d->nFloors && BossIsTrainer(d))
        out->nTrainers++;
    FloorSlots(r, dungeon, floor, TRUE, &first, &count);
    out->itemBase = d->itemBase + first;
    out->nItems = count;
    out->side = !d->required;
    out->guardDir = RDIR_NONE;
    out->mouth = 0xFF;
    out->caveIn = d->host;
    // A floor with a way further down (or out) goes through.
    out->caveOut = (floor < d->nFloors || d->exit != 0xFF) ? d->host : 0xFF;
    // (An ice cave's and a volcano's floors are lit: rogue.c GetFloorHeader.)
    out->dark = host->kind == RPLACE_CAVE && d->kind != RDUN_ICE && d->kind != RDUN_VOLCANO ? host->dark : 0;
    out->seaSide = RDIR_NONE;
    out->branch = host->branch;
}

// How often a step on a floor meets a wild Pokemon, as the real place each
// kind borrows from; 0 for none. Caves, Victory Road, Mt. Pyre's floors, the
// Shoal Cave ice and New Mauville meet one at Emerald's 10, the woods near
// Petalburg Woods' 20. Ruins' sandy floors meet them at
// 7, as the Tanoby Chambers where Unown live, and their vault, like the
// Desert Ruins, Island Cave and the Ancient Tomb, has none, so the tablets
// are read in peace. The Abandoned Ship has none on its decks at all (only
// Tentacool on its flooded rooms' water, at 4): its ghosts come at that 4,
// a rare chill. The volcano's floors are the inside of a mountain, as Mt.
// Ember's are (7). The villains' hideout has none, like the Aqua Hideout,
// nor has a legend's chamber, like the Regis'. Every other floor (tower
// rooms too, as Mt. Pyre's) has its own table (RogueDungeon_FloorWild),
// never the slot map's placeholder.
u8 RogueGen_FloorWildRate(const struct RogueRegion *r, u8 dungeon, u8 floor)
{
    static const u8 sRates[RDUN_COUNT] = { [RDUN_CAVE] = 10, [RDUN_VICTORY] = 10, [RDUN_FOREST] = 18,
                                           [RDUN_TOWER] = 10, [RDUN_HIDEOUT] = 0, [RDUN_LAIR] = 10,
                                           [RDUN_RUINS] = 7, [RDUN_SHIP] = 4, [RDUN_PLANT] = 10,
                                           [RDUN_ICE] = 10, [RDUN_VOLCANO] = 7, [RDUN_MIRAGE] = 10 };
    const struct RogueDungeon *d = &r->dungeons[dungeon % ROGUE_MAX_DUNGEONS];
    if (dungeon >= r->nDungeons || floor < 1 || floor > d->nFloors || d->kind >= RDUN_COUNT)
        return 0;
    if (RogueGen_HasChamber(d) && floor == d->nFloors)
        return 0;
    if (d->kind == RDUN_RUINS && floor == d->nFloors)
        return 0;
    return sRates[d->kind];
}

// Wild Pokemon run under the place's trainers, as in Emerald: two under on
// the first routes (Routes 102 to 104), three until the trainers near 20,
// and further under after that, as Emerald's late wilds sit well under its
// leaders: Route 119's at 25 to 27 before Winona's 33, Route 121's at 26 to
// 28 before Tate and Liza's 42, Victory Road's at 36 to 44 before the
// Elite Four's 49 to 53. A place's trainers climb to a little under the next
// gym ace, so the wilds' best are about five to ten under that ace midgame
// and ten to fifteen under late, and Victory Road's are 35 to 41.
u8 RogueGen_WildLevel(u8 placeLevel)
{
    if (placeLevel <= 4)
        return 2;
    if (placeLevel <= 9)
        return placeLevel - 2;
    if (placeLevel <= 22)
        return placeLevel - 3;
    return placeLevel - 3 - (placeLevel - 18) / 4;
}

// Victory Road's Golbat, Hariyama and Lairon are 40 to 44 among Zubat,
// Makuhita and Aron at 36 to 38: late, the 5, 4 and 1 percent slots are two
// levels up on the rest, so a table spans about six levels.
u8 RogueGen_RareSlotBonus(u8 slot, u8 wildLevel)
{
    return wildLevel >= 20 && slot >= 6 ? 2 : 0;
}

// ---------------------------------------------------------------------------
// Planning

static bool8 AddDungeon(struct RogueRegion *r, u8 kind, u8 host, u8 nFloors, u8 boss, bool8 required)
{
    struct RogueDungeon *d;
    if (r->nDungeons >= ROGUE_MAX_DUNGEONS || RogueGen_DungeonAt(r, host) >= 0)
        return FALSE;
    d = &r->dungeons[r->nDungeons++];
    memset(d, 0, sizeof(*d));
    d->kind = kind;
    d->host = host;
    d->exit = 0xFF;
    d->nFloors = nFloors;
    d->boss = boss;
    d->required = required;
    // An optional dungeon is a step up from its host, but not while the
    // team is young: one found before the second badge starts at its
    // host's level, as Petalburg Woods does.
    d->level = r->places[host].level + (!required && r->places[host].badges <= 1 ? 0 : 2);
    d->badges = r->places[host].badges;
    d->area = 0xFF;
    return TRUE;
}

#include "rogue_gen_hideout.h"

// Trainers, items and wild areas for each dungeon, after the places' own.
static void FillDungeons(struct RogueRegion *r)
{
    int i;
    for (i = 0; i < r->nDungeons; i++)
    {
        struct RogueDungeon *d = &r->dungeons[i];
        const struct RoguePlace *host = &r->places[d->host];
        // Victory Road has trainers on every floor, but few enough that a
        // team reaches the League with something left (five a floor with no
        // Center on the way wore out most teams that got there), the forests
        // have their Bug Catchers (Viridian Forest's), and the main road's
        // cave has a few more than a side cave.
        u8 perFloor = (d->kind == RDUN_FOREST || d->required) ? 3 : 2;
        d->area = host->kind == RPLACE_CAVE ? host->area : 0xFF;
        // A lair's caves share its town's area (given out after the
        // dungeons': RogueGen_ShareLairAreas), as catch areas are few.
        if (d->area == 0xFF && d->kind != RDUN_LAIR && r->nAreas < ROGUE_MAX_AREAS)
            d->area = r->nAreas++;
        if (d->kind == RDUN_LAIR)
            perFloor = 2;
        d->nTrainers = perFloor * DungeonGenFloors(d) + BossIsTrainer(d);
        d->nItems = 2 * DungeonGenFloors(d) + 1;
        if (IsRoomDungeon(d))
            RoomDungeonTotals(r, i, &d->nTrainers, &d->nItems);
        // Ruins: two a sandy floor, none in the vault.
        if (IsRuins(d))
            d->nTrainers = 2 * (d->nFloors - 1);
        if (r->nTrainerSlots + d->nTrainers > ROGUE_MAX_ROUTE_TRAINERS)
            d->nTrainers = 0;
        d->trainerBase = r->nTrainerSlots;
        r->nTrainerSlots += d->nTrainers;
        {
            struct RogueDungeonExtras e;
            PlanExtras(r, i, &e);
            d->nItems += e.total;
        }
        if (r->nItems + d->nItems > ROGUE_MAX_ITEM_FLAGS)
            d->nItems = 0;
        d->itemBase = r->nItems;
        r->nItems += d->nItems;
    }
}

// A lair's floors share its town's catch area (towns get theirs after the
// dungeons are planned).
static void ShareLairAreas(struct RogueRegion *r)
{
    int i;
    for (i = 0; i < r->nDungeons; i++)
        if (r->dungeons[i].kind == RDUN_LAIR)
            r->dungeons[i].area = r->places[r->dungeons[i].host].area;
}

// Some caves are a master's home instead of a guardian's: a Hiker elder who
// battles anyone who comes that deep.
static void PlanBosses(struct RogueRegion *r)
{
    int i;
    for (i = 0; i < r->nDungeons; i++)
    {
        struct RogueDungeon *d = &r->dungeons[i];
        u32 rng = RogueRng_Hash(r->seed, 11700 + i);
        if (d->kind == RDUN_CAVE && d->boss == RBOSS_GUARDIAN && RogueRng_Range(&rng, 100) < 30)
            d->boss = RBOSS_MASTER;
    }
}

#include "rogue_gen_forest.h"
#include "rogue_gen_cave.h"
#include "rogue_gen_volcano.h"
#include "rogue_gen_ice.h"
#include "rogue_gen_legend.h"
#include "rogue_gen_ship.h"
#include "rogue_gen_plant.h"
#include "rogue_gen_tunnel.h"

// Which places hold dungeons. Uses its own random stream, so the rest of the
// region stays as it was.
static void PlanDungeons(struct RogueRegion *r)
{
    u32 rng = RogueRng_Hash(r->seed, 11000);

    r->nDungeons = 0;
    PlanCaveDungeons(r, &rng);
    PlanForestDungeons(r);
    PlanTowerDungeons(r);
    PlanHideout(r);
    // The volcano takes a cave (or a town) by the crater first, then the
    // ruins a side cave in sand or hills, then the ice cave a late side cave
    // (or a town): each only a cave still RDUN_CAVE, so none is taken twice.
    PlanVolcano(r);
    PlanRuinsDungeons(r);
    PlanIceCave(r);
    PlanBosses(r);
    // Other kinds' planning goes above this line: the legends pick among all.
    // The other villain teams' hideouts, leaving the legends room.
    PlanTeamHideouts(r);
    PlanLegends(r);
    // A wreck and a power plant take what the legends left: a sea route
    // (a slot left under the cap) and a dead-end side route's end.
    PlanShipDungeons(r);
    PlanPlantDungeons(r);
    FillDungeons(r);
    PlanLegendGates(r);
    PlanTunnel(r);
    // (v1.1 peaks) The tower may move out into a sandstorm desert: last, in
    // its own slot and with its trainers' and items' share, so every other
    // dungeon, trainer, item and the tunnel stay as they were.
    PlanMirageTower(r);
}

// ---------------------------------------------------------------------------
// Floors

// A floor spot for a ladder: open floor with floor all round, off the
// chamber's edges, nothing placed near.
static bool8 FindLadderSpot(struct Gen *gen, const struct Rect *r, int *fx, int *fy)
{
    return FindCaveSpot(gen, r, fx, fy);
}

// ---------------------------------------------------------------------------
// Dungeon extras on a floor
//
// PlaceFloorExtras is for every kind's floor generator: call it once the
// boss stands and before the trainers and ordinary items, which keep clear of
// what it places. Rooms are rectangles of units (as GenCave's chambers);
// open(gen, fx, fy) says whether a block is floor one can walk on. The choice
// row goes in one of the side rooms if it fits (else a main room), the camp
// in a side room or a main room off the ends, and the prize beside the boss.
// Returns FALSE if something would not fit, so the floor is made again.

// TRUE if a w x h block area is open, with open ground all round it, and
// nothing placed within margin blocks.
static bool8 ExtrasAreaFree(struct Gen *gen, int x, int y, int w, int h, int margin, bool8 (*open)(struct Gen *, int, int))
{
    int fx, fy;
    for (fy = y - margin; fy < y + h + margin; fy++)
    {
        for (fx = x - margin; fx < x + w + margin; fx++)
        {
            bool8 ring = fx >= x - 1 && fx <= x + w && fy >= y - 1 && fy <= y + h;
            if (fx < 1 || fy < 1 || fx >= gen->m->w - 1 || fy >= gen->m->h - 1)
                return FALSE;
            if (ring && !open(gen, fx, fy))
                return FALSE;
            if (IsOccupied(gen, fx, fy))
                return FALSE;
        }
    }
    return TRUE;
}

// A spot for a row of n objects two blocks apart (n = 1 for one alone) in a
// room, clear of everything else.
static bool8 FindExtrasRow(struct Gen *gen, const struct Rect *room, int n, int *fx, int *fy, bool8 (*open)(struct Gen *, int, int))
{
    int tries, w = 2 * n - 1;
    for (tries = 0; tries < 40; tries++)
    {
        int x = room->x * 2 + RAND(room->w * 2), y = room->y * 2 + RAND(room->h * 2);
        if (!ExtrasAreaFree(gen, x, y, w, 1, 2, open))
            continue;
        *fx = x;
        *fy = y;
        return TRUE;
    }
    return FALSE;
}

// The person beside a choice row of n balls at fx, fy (two blocks apart)
// who says only one may be taken: at an end of the row, else beside it, on
// open ground with open ground all round, so they close no way. Nobody if
// there is no such place.
static void PlaceChoiceGuide(struct Gen *gen, int fx, int fy, int n, bool8 (*open)(struct Gen *, int, int))
{
    const s8 spots[][3] = {
        { -2, 0, RDIR_EAST }, { 0, 0, RDIR_WEST }, { -1, 1, RDIR_NORTH }, { -1, -1, RDIR_SOUTH },
        { 1, 1, RDIR_NORTH }, { 1, -1, RDIR_SOUTH },
    };
    u32 k, d;
    for (k = 0; k < sizeof(spots) / sizeof(spots[0]); k++)
    {
        // The second spot is the far end; the last two, beside the far end.
        int x = (k == 1 ? fx + 2 * n : k >= 4 ? fx + 2 * n - 2 : fx) + spots[k][0], y = fy + spots[k][1];
        bool8 ok = x >= 1 && y >= 1 && x < gen->m->w - 1 && y < gen->m->h - 1 && open(gen, x, y) && !IsOccupied(gen, x, y);
        for (d = 0; d < 4 && ok; d++)
            ok = open(gen, x + sDirDX[d], y + sDirDY[d]);
        if (ok)
        {
            AddObj(gen, ROBJ_GUIDE, x, y, spots[k][2], 0, 0);
            return;
        }
    }
}

// The prize lies just behind or beside the boss, so the boss guards it.
// FALSE if it is the prize's floor and nothing fits.
static bool8 PlacePrize(struct Gen *gen, const struct RogueDungeonExtras *e, int bossX, int bossY,
                        bool8 (*open)(struct Gen *, int, int))
{
    static const s8 sPrizeSpots[][2] = { { 0, -2 }, { -1, -2 }, { 1, -2 }, { -2, 0 }, { 2, 0 }, { -2, -1 }, { 2, -1 }, { 0, 2 } };
    int start, k;
    if (e->prize == 0xFF || sGenFloor != gen->r->dungeons[sGenDungeon].nFloors || bossX < 0)
        return TRUE;
    start = RAND(3);
    for (k = 0; k < 8; k++)
    {
        const s8 *d = sPrizeSpots[k < 3 ? (start + k) % 3 : k];
        int fx = bossX + d[0], fy = bossY + d[1];
        if (!ExtrasAreaFree(gen, fx, fy, 1, 1, 1, open))
            continue;
        AddObj(gen, ROBJ_PRIZE, fx, fy, RDIR_SOUTH, e->prize, 0);
        return TRUE;
    }
    return FALSE;
}

static bool8 PlaceFloorExtras(struct Gen *gen, int bossX, int bossY, const struct Rect *side, int nSide,
                              const struct Rect *main, int nMain, bool8 (*open)(struct Gen *, int, int))
{
    struct RogueDungeonExtras e;
    int fx, fy, i, k;

    RogueGen_DungeonExtras(gen->r, sGenDungeon, &e);
    if (!PlacePrize(gen, &e, bossX, bossY, open))
        return FALSE;
    // The choice room's balls on display in a row, in a side room.
    if (e.nChoice && sGenFloor == e.choiceFloor)
    {
        bool8 found = FALSE;
        int first = nSide ? RAND(nSide) : 0;
        for (i = 0; i < nSide && !found; i++)
            found = FindExtrasRow(gen, &side[(first + i) % nSide], e.nChoice, &fx, &fy, open);
        for (i = 0; i < nMain && !found; i++)
            found = FindExtrasRow(gen, &main[i], e.nChoice, &fx, &fy, open);
        if (!found)
            return FALSE;
        for (k = 0; k < e.nChoice; k++)
            AddObj(gen, ROBJ_CHOICE, fx + 2 * k, fy, RDIR_SOUTH, e.choice + k, 0);
        PlaceChoiceGuide(gen, fx, fy, e.nChoice, open);
    }
    // The camp, out of the way.
    if (e.camp != 0xFF && sGenFloor == e.campFloor)
    {
        bool8 found = FALSE;
        int first = nSide ? RAND(nSide) : 0;
        for (i = 0; i < nSide && !found; i++)
            found = FindExtrasRow(gen, &side[(first + i) % nSide], 1, &fx, &fy, open);
        for (i = 1; i + 1 < nMain && !found; i++)
            found = FindExtrasRow(gen, &main[i], 1, &fx, &fy, open);
        if (!found)
            return FALSE;
        AddObj(gen, ROBJ_CAMP, fx, fy, RDIR_SOUTH, e.camp, 0);
    }
    // Someone lost, off the way in a side room if there is one.
    if (e.explorer != 0xFF && sGenFloor == e.explorerFloor)
    {
        bool8 found = FALSE;
        int first = nSide ? RAND(nSide) : 0;
        for (i = 0; i < nSide && !found; i++)
            found = FindExtrasRow(gen, &side[(first + i) % nSide], 1, &fx, &fy, open);
        for (i = 0; i < nMain && !found; i++)
            found = FindExtrasRow(gen, &main[i], 1, &fx, &fy, open);
        if (!found)
            return FALSE;
        AddObj(gen, ROBJ_EXPLORER, fx, fy, RAND(4), e.explorer, 0);
    }
    return TRUE;
}

// A floor stopped part way would leave the generator in floor mode for the
// next map.
void RogueGen_DropJob(void)
{
    sGenFloor = 0;
}

void RogueGen_GenerateFloor(const struct RogueRegion *r, u8 dungeon, u8 floor, struct RogueMap *m, u16 *blocks)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon % ROGUE_MAX_DUNGEONS];
    struct Gen *gen = &sGen;
    struct RoguePlace *p = &sFloorGenPlace;
    u32 attempt = 0;
    bool8 ok;

    if (RogueGen_FloorRoom(r, dungeon, floor) != 0xFF)
    {
        GenRoomFloor(r, dungeon, floor, m, blocks);
        return;
    }
    RogueGen_FloorPlace(r, dungeon, floor, p);
    sGenDungeon = dungeon;
    sGenFloor = floor;
    // Up to 120 tries (the salts stay apart below 200 a floor): a volcano's
    // tunnel floor, with the lava lake, the way out and its barrier, can
    // take 90 (seed 617).
    do
    {
        int k;
        YIELD();
        memset(m, 0, sizeof(*m));
        m->place = d->host;
        m->dungeon = dungeon;
        m->floor = floor;
        m->blocks = blocks;
        m->w = p->w;
        m->h = p->h;
        memset(gen, 0, sizeof(*gen));
        gen->r = r;
        gen->p = p;
        gen->m = m;
        gen->cw = m->w / 2;
        gen->ch = m->h / 2;
        gen->attempt = attempt;
        gen->bites = TRUE;
        gen->rng = RogueRng_Hash(r->seed, 60000 + dungeon * 4000 + floor * 200 + attempt++);
        for (k = 0; k < 4; k++)
            gen->exitPos[k] = 0xFF;
        // (A kind with a generator of its own: see TunnelStyle.)
        ok = d->kind == RDUN_FOREST ? GenForest(gen)
           : floor == RogueGen_PyreSlopeFloor(r, dungeon) ? GenPyreSlope(gen) // (v1.1 peaks)
           : GenCave(gen);
    } while (!ok && attempt < 120);
    sGenFloor = 0;
#ifdef ROGUE_DEBUG
    fprintf(stderr, "dungeon %d floor %d attempts %u ok %d\n", dungeon, floor, attempt, ok);
#endif
}
