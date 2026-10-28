// Towers, hideouts and lairs: floors built from vanilla rooms. Included by
// rogue_gen_dungeon.h.
//
// Each floor is a room of Hoenn's own (src/data/rogue_rooms.h, written by
// tools/roguemon/gen_rooms.py): Mt. Pyre's floors for a tower, the Aqua or
// Magma Hideout's for the villains' hideout. The room's blocks are copied as
// they are; its stairs lead to the floor before and the next, its warp pads
// to each other, and the dungeon's trainers and items stand where the room's
// own trainers, items and people stood, or in its open places.

// Roles of a room's warps. A room lists its BACK warps first, the way in at
// index 0, then its ON warps, then its pads.
// A ruins room also has LAND: plain floor where a fall from the floor above
// lands (src/rogue_gen_ruins.h), never stepped on as a warp.
enum { ROOM_BACK, ROOM_ON, ROOM_PAD, ROOM_LAND };
// PRIZE, CHOICE and CAMP are the dungeon extras' places (gen_rooms.py
// extra_spots): taken only by them, so trainers and items keep the room's
// other spots. LEADER is where the villain leader waits before a legend.
// GUIDE is beside the CHOICE row, for the person who says only one may be
// taken. CARVING is a wall in a lair with old words on it (a sign). GATE is
// a block every walk to a lair's legend crosses, where what keeps it shut
// stands (ROBJ_SEAL).
// A shipwreck's floors (gen_rooms.py cabin_spots) have cabins, numbered by
// the spot's range: KEY where a key to another cabin lies, LOCK on a door
// that can be locked (range + 0x80: always locked), HOLD and HOLD_PRIZE
// where the boss and the prize wait if it is the last cabin opened, and
// SHUT on a door that never opens. DECOY is where a Voltorb poses as an
// item ball (New Mauville's).
enum { ROOM_SPOT_TRAINER, ROOM_SPOT_ITEM, ROOM_SPOT_PERSON, ROOM_SPOT_BOSS, ROOM_SPOT_OPEN,
       ROOM_SPOT_PRIZE, ROOM_SPOT_CHOICE, ROOM_SPOT_CAMP, ROOM_SPOT_LEADER, ROOM_SPOT_GUIDE,
       ROOM_SPOT_CARVING, ROOM_SPOT_GATE, ROOM_SPOT_TABLET,
       ROOM_SPOT_KEY, ROOM_SPOT_LOCK, ROOM_SPOT_DECOY, ROOM_SPOT_HOLD, ROOM_SPOT_HOLD_PRIZE, ROOM_SPOT_SHUT };
enum { ROOM_USE_GROUND, ROOM_USE_MIDDLE, ROOM_USE_LAST };

struct RogueRoomWarp
{
    u8 x, y, role, pair; // pair: a pad's partner
};

struct RogueRoomSpot
{
    u8 x, y, kind, facing, range; // facing as RogueObj.facing
};

struct RogueRoom
{
    const u16 *blocks;
    const struct RogueRoomWarp *warps;
    const struct RogueRoomSpot *spots;
    u8 w, h, nWarps, nSpots;
    u8 style, use;
};

#include "data/rogue_rooms.h"

#define NUM_ROOMS (sizeof(sRogueRooms) / sizeof(sRogueRooms[0]))

// A tower, hideout, shipwreck or power plant: every floor a room. (A
// legend's dungeon has only its chamber, the last floor: RogueGen_HasChamber.)
static bool8 IsRoomDungeon(const struct RogueDungeon *d)
{
    return d->kind == RDUN_TOWER || d->kind == RDUN_HIDEOUT || d->kind == RDUN_SHIP || d->kind == RDUN_PLANT
        || d->kind == RDUN_MIRAGE;
}

static u8 LairStyle(const struct RogueRegion *r, u8 dungeon);
s32 RogueGen_LeaderDungeon(const struct RogueRegion *r);
static bool8 IsRuins(const struct RogueDungeon *d);
static u8 RuinsFloorRoom(const struct RogueRegion *r, u8 dungeon, u8 floor);
static u8 FloorVisit(const struct RogueDungeon *d);

static u8 TeamStyle(const struct RogueRegion *r, u8 team); // src/rogue_gen_hideout.h

// A hideout's rooms: its team's (Aqua or Magma Hideout's; TeamStyle).
u8 RogueGen_HideoutStyle(const struct RogueRegion *r, u8 dungeon)
{
    u8 t = RogueGen_HideoutTeam(r, dungeon);
    return TeamStyle(r, t == 0xFF ? 0 : t);
}

static u8 RoomStyle(const struct RogueRegion *r, const struct RogueDungeon *d)
{
    if (RogueGen_HasChamber(d))
        return LairStyle(r, d - r->dungeons);
    if (d->kind == RDUN_SHIP)
        return ROOM_STYLE_SHIP;
    if (d->kind == RDUN_PLANT)
        return ROOM_STYLE_PLANT;
    if (d->kind == RDUN_MIRAGE)
        return ROOM_STYLE_MIRAGE;
    return d->kind == RDUN_TOWER ? ROOM_STYLE_PYRE : RogueGen_HideoutStyle(r, d - r->dungeons);
}

// The most floors a dungeon of this style can have: a ground floor, the
// middle rooms, a last floor.
static u8 RoomStyleFloors(u8 style)
{
    u32 i, n = 0;
    for (i = 0; i < NUM_ROOMS; i++)
        if (sRogueRooms[i].style == style && sRogueRooms[i].use == ROOM_USE_MIDDLE)
            n++;
    return 2 + n;
}

u8 RogueGen_FloorRoom(const struct RogueRegion *r, u8 dungeon, u8 floor)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon % ROGUE_MAX_DUNGEONS];
    u8 style, use, ids[8];
    u32 i, n = 0, rng;

    YIELD();
    if (dungeon >= r->nDungeons || floor < 1 || floor > d->nFloors)
        return 0xFF;
    if (IsRuins(d))
        return RuinsFloorRoom(r, dungeon, floor);
    // A legend's dungeon ends in its chamber; the floors before are caves
    // or woods.
    if (RogueGen_HasChamber(d) ? floor != d->nFloors : !IsRoomDungeon(d))
        return 0xFF;
    // (v1.1 peaks) The tower's misty mountainside is generated.
    if (floor == RogueGen_PyreSlopeFloor(r, dungeon))
        return 0xFF;
    style = RoomStyle(r, d);
    // (A dungeon of one floor, a power plant, is its last room.)
    use = floor == d->nFloors ? ROOM_USE_LAST : floor == 1 ? ROOM_USE_GROUND : ROOM_USE_MIDDLE;
    if (RogueGen_HasChamber(d))
        use = ROOM_USE_LAST;
    for (i = 0; i < NUM_ROOMS && n < sizeof(ids); i++)
        if (sRogueRooms[i].style == style && sRogueRooms[i].use == use)
            ids[n++] = i;
    if (n == 0)
        return 0xFF;
    rng = RogueRng_Hash(r->seed, 14000 + dungeon * 16 + use);
    // The middle floors take the middle rooms in a shuffled order, so no
    // two are alike while there are rooms enough; a ship's go down in order,
    // and the Mirage Tower's up.
    for (i = n - 1; i > 0 && style != ROOM_STYLE_SHIP && style != ROOM_STYLE_MIRAGE; i--)
    {
        u32 j = RogueRng_Range(&rng, i + 1);
        u8 t = ids[i];
        ids[i] = ids[j];
        ids[j] = t;
    }
    return use == ROOM_USE_MIDDLE ? ids[(floor - 2) % n] : ids[0];
}

// Where a legend's seal (ROBJ_SEAL) stands: 0xFF for none (the dungeon is
// trial enough), FALSE at the room's narrow place (GATE), TRUE where the
// legend appears once it opens: the stone of a rite or the marks of the
// rest of its set, which call it out, or a room open all round. The
// villain team's lookout stands where his leader will.
u8 RogueGen_SealAtLegend(const struct RogueRegion *r, u8 dungeon)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon % ROGUE_MAX_DUNGEONS];
    u8 room, i;
    if (!RogueGen_HasChamber(d))
        return 0xFF;
    switch (ROGUE_GATE_KIND(d->gate))
    {
    case RGATE_OPEN:
        return 0xFF;
    case RGATE_RITUAL:
    case RGATE_AFTER:
        return TRUE;
    case RGATE_VILLAIN:
        return FALSE;
    }
    room = RogueGen_FloorRoom(r, dungeon, d->nFloors);
    if (room == 0xFF)
        return TRUE;
    for (i = 0; i < sRogueRooms[room].nSpots; i++)
        if (sRogueRooms[room].spots[i].kind == ROOM_SPOT_GATE)
            return FALSE;
    return TRUE;
}

// The rank of warp i among the room's warps of its role.
static u8 RoomWarpRank(const struct RogueRoom *room, u8 i)
{
    u8 k, rank = 0;
    for (k = 0; k < i; k++)
        if (room->warps[k].role == room->warps[i].role)
            rank++;
    return rank;
}

// The index of the room's rank-th warp of a role (the last one if it has
// fewer).
static u8 RoomWarpOfRole(const struct RogueRoom *room, u8 role, u8 rank)
{
    u8 k, found = 0xFF;
    for (k = 0; k < room->nWarps; k++)
    {
        if (room->warps[k].role != role)
            continue;
        found = k;
        if (rank-- == 0)
            break;
    }
    return found == 0xFF ? 0 : found;
}

// (v1.1 peaks) The warp of a room floor's (first) stairs on, for a way back
// down onto it from a generated floor above (a tower's mountainside).
u8 RogueGen_RoomOnWarp(const struct RogueRegion *r, u8 dungeon, u8 floor)
{
    u8 id = RogueGen_FloorRoom(r, dungeon, floor);
    return id == 0xFF ? RFLOOR_DOWN : RoomWarpOfRole(&sRogueRooms[id], ROOM_ON, 0);
}

void RogueGen_RoomWarpDest(const struct RogueRegion *r, u8 dungeon, u8 floor, u8 warp, u8 *toFloor, u8 *toWarp)
{
    u8 id = RogueGen_FloorRoom(r, dungeon, floor), other;
    const struct RogueRoom *room;

    *toFloor = floor;
    *toWarp = 0;
    if (id == 0xFF || warp >= sRogueRooms[id].nWarps)
        return;
    room = &sRogueRooms[id];
    switch (room->warps[warp].role)
    {
    case ROOM_PAD:
        *toWarp = room->warps[warp].pair;
        break;
    case ROOM_LAND:
        *toWarp = warp; // not a way anywhere: plain floor
        break;
    case ROOM_BACK:
        // Back to the floor before: its stairs on, the same one where it
        // has as many; from the ground floor out to the town.
        *toFloor = floor - 1;
        if (floor > 1 && (other = RogueGen_FloorRoom(r, dungeon, floor - 1)) != 0xFF)
            *toWarp = RoomWarpOfRole(&sRogueRooms[other], ROOM_ON, RoomWarpRank(room, warp));
        else if (floor > 1)
            *toWarp = RFLOOR_DOWN; // a chamber: back up the ladder or the path it came by
        break;
    case ROOM_ON:
        *toFloor = floor + 1;
        if ((other = RogueGen_FloorRoom(r, dungeon, floor + 1)) != 0xFF)
            *toWarp = RoomWarpOfRole(&sRogueRooms[other], ROOM_BACK, RoomWarpRank(room, warp));
        break;
    }
}

// How many of the dungeon's trainers (not the boss) and items stand on a
// floor: a tower's ground floor is a quiet hall with something to find, the
// others have a couple of trainers each, and the last floor keeps a reward.
// No more than the room has spots for.
static void RoomFloorCounts(const struct RogueRegion *r, u8 dungeon, u8 floor, u8 *nTrainers, u8 *nItems)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon];
    const struct RogueRoom *room;
    u8 id = RogueGen_FloorRoom(r, dungeon, floor), people = 0, items = 0, open = 0, wantT, wantI, i, extra;

    *nTrainers = *nItems = 0;
    // (v1.1 peaks) The misty mountainside holds what a middle room does.
    if (id == 0xFF && floor == RogueGen_PyreSlopeFloor(r, dungeon) && floor != 0)
    {
        *nTrainers = 2;
        *nItems = 1;
        return;
    }
    if (id == 0xFF || RogueGen_HasChamber(d))
        return;
    room = &sRogueRooms[id];
    for (i = 0; i < room->nSpots; i++)
    {
        switch (room->spots[i].kind)
        {
        case ROOM_SPOT_TRAINER:
        case ROOM_SPOT_PERSON: people++; break;
        case ROOM_SPOT_ITEM:   items++; break;
        case ROOM_SPOT_OPEN:   open++; break;
        }
    }
    wantT = (d->kind == RDUN_TOWER && floor == 1) ? 0 : (floor == d->nFloors && d->kind == RDUN_TOWER) ? 1 : 2;
    wantI = floor == d->nFloors ? 2 : 1;
    // (v1.1 peaks) The desert tower: one on the ground floor, none at the
    // top with its guardian, whose prize and fossils are its finds there.
    if (d->kind == RDUN_MIRAGE)
    {
        wantT = floor == 1 ? 1 : floor == d->nFloors ? 0 : 2;
        wantI = 1;
    }
    // A shipwreck's decks are busier, and the power plant is one big hall.
    if (d->kind == RDUN_SHIP && floor > 1 && floor < d->nFloors)
        wantT = 3;
    if (d->kind == RDUN_PLANT)
    {
        wantT = 4;
        wantI = 3;
    }
    *nTrainers = wantT < people + open ? wantT : people + open;
    extra = *nTrainers > people ? *nTrainers - people : 0;
    open -= extra;
    *nItems = wantI < items + open ? wantI : items + open;
}

// The first of a floor's trainers or items among the dungeon's, and how many.
static void RoomFloorSlots(const struct RogueRegion *r, u8 dungeon, u8 floor, bool8 items, u8 *first, u8 *count)
{
    u8 f, t, i;
    *first = 0;
    for (f = 1; f < floor; f++)
    {
        RoomFloorCounts(r, dungeon, f, &t, &i);
        *first += items ? i : t;
    }
    RoomFloorCounts(r, dungeon, floor, &t, &i);
    *count = items ? i : t;
    // (v1.1 peaks) The desert tower keeps the share of trainers and items
    // the tower it was planned as had (PlanMirageTower): no more than that.
    if (r->dungeons[dungeon].kind == RDUN_MIRAGE)
    {
        u8 budget = r->dungeons[dungeon].nTrainers;
        if (items)
        {
            struct RogueDungeonExtras e;
            RogueGen_DungeonExtras(r, dungeon, &e);
            budget = r->dungeons[dungeon].nItems - e.total;
        }
        if (*first > budget)
            *first = budget;
        if (*first + *count > budget)
            *count = budget - *first;
    }
}

// A tower or hideout's trainers (boss included) and items, all floors.
static void RoomDungeonTotals(const struct RogueRegion *r, u8 dungeon, u8 *nTrainers, u8 *nItems)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon];
    u8 first, t, i;
    RoomFloorSlots(r, dungeon, d->nFloors, FALSE, &first, &t);
    *nTrainers = first + t + BossIsTrainer(d);
    RoomFloorSlots(r, dungeon, d->nFloors, TRUE, &first, &i);
    *nItems = first + i;
}

// How many spots of a kind the room of a floor has (0 off the dungeon).
static u8 RoomSpotCount(const struct RogueRegion *r, u8 dungeon, u8 floor, u8 kind)
{
    u8 id = RogueGen_FloorRoom(r, dungeon, floor), i, n = 0;
    // (v1.1 peaks) The misty mountainside finds room for a choice row of
    // three or the camp, as a middle room has.
    if (id == 0xFF && floor != 0 && floor == RogueGen_PyreSlopeFloor(r, dungeon))
        return kind == ROOM_SPOT_CHOICE ? 3 : kind == ROOM_SPOT_CAMP ? 1 : 0;
    if (id == 0xFF)
        return 0;
    for (i = 0; i < sRogueRooms[id].nSpots; i++)
        n += sRogueRooms[id].spots[i].kind == kind;
    return n;
}

// For PlanExtras: whether the boss's room has a place for the prize, a
// floor for a choice room of *n balls (three if a room has a row of three,
// else two; 0 if no room has a row), and the floor nearest halfway up whose
// room has a place for a camp (0 for none).
static bool8 RoomHasPrize(const struct RogueRegion *r, u8 dungeon)
{
    u8 last = r->dungeons[dungeon].nFloors;
    return RoomSpotCount(r, dungeon, last, ROOM_SPOT_PRIZE) + RoomSpotCount(r, dungeon, last, ROOM_SPOT_HOLD_PRIZE) != 0;
}

// ---------------------------------------------------------------------------
// A shipwreck's cabins and keys
//
// Every floor's lockable doors (LOCK spots) are worked out from the seed: a
// door always locked (B1F's storage room) has its key in another cabin of
// the floor; on the hidden deck the cabins open one after another, the
// first open and each holding the key to the next, as Hoenn's Rooms 3, 1,
// 4 and 6 do, and the boss waits in the last. Keys are item balls (their
// flags are the dungeon's, RogueDungeonExtras.keys on, floor by floor), so
// a door is open once its key is picked up and used on it, which takes the
// key from the bag (Rogue_CheckShipDoor): nothing else is saved.

#define MAX_FLOOR_LOCKS 4

// The cabins of a floor locked this run, in the order they open, the cabin
// each one's key lies in, and the cabin left last (0xFF if none). Returns
// how many are locked.
static u8 FloorLocks(const struct RogueRegion *r, u8 dungeon, u8 floor, u8 *locked, u8 *keyIn, u8 *last)
{
    u8 id = RogueGen_FloorRoom(r, dungeon, floor), i, n = 0, chain[MAX_FLOOR_LOCKS], nChain = 0;
    u8 keyCabins[8], nKey = 0;
    const struct RogueRoom *room;
    u32 rng;

    *last = 0xFF;
    if (id == 0xFF || r->dungeons[dungeon].kind != RDUN_SHIP)
        return 0;
    room = &sRogueRooms[id];
    rng = RogueRng_Hash(r->seed, 14500 + dungeon * 16 + floor);
    for (i = 0; i < room->nSpots; i++)
    {
        const struct RogueRoomSpot *s = &room->spots[i];
        if (s->kind == ROOM_SPOT_KEY && nKey < sizeof(keyCabins))
            keyCabins[nKey++] = s->range;
        else if (s->kind == ROOM_SPOT_LOCK && !(s->range & 0x80) && nChain < MAX_FLOOR_LOCKS)
            chain[nChain++] = s->range;
    }
    // A door always locked: its key in a cabin left open.
    for (i = 0; i < room->nSpots && n < MAX_FLOOR_LOCKS; i++)
    {
        const struct RogueRoomSpot *s = &room->spots[i];
        u8 k, pick, open = 0;
        if (s->kind != ROOM_SPOT_LOCK || !(s->range & 0x80))
            continue;
        for (k = 0; k < nKey; k++)
            open += keyCabins[k] != (s->range & 0x7F);
        if (open == 0)
            continue;
        pick = RogueRng_Range(&rng, open);
        for (k = 0; k < nKey; k++)
            if (keyCabins[k] != (s->range & 0x7F) && pick-- == 0)
                break;
        locked[n] = s->range & 0x7F;
        keyIn[n++] = keyCabins[k];
    }
    // The cabins that open one after another, in a shuffled order.
    for (i = nChain; i > 1; i--)
    {
        u8 j = RogueRng_Range(&rng, i), t = chain[i - 1];
        chain[i - 1] = chain[j];
        chain[j] = t;
    }
    for (i = 1; i < nChain && n < MAX_FLOOR_LOCKS; i++)
    {
        locked[n] = chain[i];
        keyIn[n++] = chain[i - 1];
    }
    if (nChain)
        *last = chain[nChain - 1];
    return n;
}

// How many keys a shipwreck's floors before this one hold, and its own.
static u8 KeysBefore(const struct RogueRegion *r, u8 dungeon, u8 floor, u8 *count)
{
    u8 f, n = 0, locked[MAX_FLOOR_LOCKS], keyIn[MAX_FLOOR_LOCKS], last;
    for (f = 1; f < floor; f++)
        n += FloorLocks(r, dungeon, f, locked, keyIn, &last);
    if (count)
        *count = FloorLocks(r, dungeon, floor, locked, keyIn, &last);
    return n;
}

static u8 DungeonKeyCount(const struct RogueRegion *r, u8 dungeon)
{
    return r->dungeons[dungeon].kind == RDUN_SHIP ? KeysBefore(r, dungeon, r->dungeons[dungeon].nFloors + 1, NULL) : 0;
}

// The Voltorb balls of a power plant: its rooms' DECOY spots.
static u8 DungeonDecoyCount(const struct RogueRegion *r, u8 dungeon)
{
    u8 f, n = 0;
    if (r->dungeons[dungeon].kind != RDUN_PLANT)
        return 0;
    for (f = 1; f <= r->dungeons[dungeon].nFloors; f++)
        n += RoomSpotCount(r, dungeon, f, ROOM_SPOT_DECOY);
    return n;
}

// The doors of a floor's room that are always locked (B1F's storage room).
static u8 RoomForcedLocks(const struct RogueRegion *r, u8 dungeon, u8 floor)
{
    u8 id = RogueGen_FloorRoom(r, dungeon, floor), i, n = 0;
    if (id == 0xFF)
        return 0;
    for (i = 0; i < sRogueRooms[id].nSpots; i++)
        n += sRogueRooms[id].spots[i].kind == ROOM_SPOT_LOCK && (sRogueRooms[id].spots[i].range & 0x80);
    return n;
}

static u8 RoomChoiceFloor(const struct RogueRegion *r, u8 dungeon, u8 *n, u32 *rng)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon];
    u8 f, count = 0, pick;
    for (; *n >= 2; (*n)--)
    {
        count = 0;
        for (f = 1; f <= d->nFloors; f++)
            count += RoomSpotCount(r, dungeon, f, ROOM_SPOT_CHOICE) >= *n;
        if (count)
            break;
    }
    if (count == 0)
    {
        *n = 0;
        return 0;
    }
    pick = RogueRng_Range(rng, count);
    for (f = 1; f <= d->nFloors; f++)
        if (RoomSpotCount(r, dungeon, f, ROOM_SPOT_CHOICE) >= *n && pick-- == 0)
            return f;
    return 0;
}

static u8 RoomCampFloor(const struct RogueRegion *r, u8 dungeon)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon];
    int k, s, half = (d->nFloors + 1) / 2;
    // Halfway, else the nearest middle floor, higher first.
    for (k = 0; k < d->nFloors; k++)
    {
        for (s = 1; s >= -1; s -= 2)
        {
            int f = half + s * k;
            if (f > 1 && f < d->nFloors && RoomSpotCount(r, dungeon, f, ROOM_SPOT_CAMP))
                return f;
        }
    }
    return 0;
}

// Takes the next unused spot of a kind, from a random start so floors of
// one room differ. Returns its index or 0xFF.
static u8 TakeRoomSpot(const struct RogueRoom *room, u32 *used, u8 kind, u32 *rng)
{
    u8 n = 0, i, start;
    for (i = 0; i < room->nSpots; i++)
        if (room->spots[i].kind == kind && !((*used >> i) & 1))
            n++;
    if (n == 0)
        return 0xFF;
    start = RogueRng_Range(rng, n);
    for (i = 0; i < room->nSpots; i++)
    {
        if (room->spots[i].kind != kind || ((*used >> i) & 1))
            continue;
        if (start-- == 0)
        {
            *used |= 1u << i;
            return i;
        }
    }
    return 0xFF;
}

// A cabin's spot of a kind (a LOCK's range holds 0x80 too), or NULL.
static const struct RogueRoomSpot *CabinSpot(const struct RogueRoom *room, u8 kind, u8 cabin)
{
    u8 i;
    for (i = 0; i < room->nSpots; i++)
        if (room->spots[i].kind == kind && (room->spots[i].range & 0x7F) == cabin)
            return &room->spots[i];
    return NULL;
}

// A shipwreck floor's storage room (the cabin whose door is always locked,
// B1F's): its key spot, which no key takes, holds the floor's find, as the
// Abandoned Ship's storage room holds TM13 Ice Beam. NULL if none.
static const struct RogueRoomSpot *StorageSpot(const struct RogueRegion *r, u8 dungeon, u8 floor)
{
    u8 id = RogueGen_FloorRoom(r, dungeon, floor), i;
    if (id == 0xFF || r->dungeons[dungeon].kind != RDUN_SHIP)
        return NULL;
    for (i = 0; i < sRogueRooms[id].nSpots; i++)
        if (sRogueRooms[id].spots[i].kind == ROOM_SPOT_LOCK && (sRogueRooms[id].spots[i].range & 0x80))
            return CabinSpot(&sRogueRooms[id], ROOM_SPOT_KEY, sRogueRooms[id].spots[i].range & 0x7F);
    return NULL;
}

// A shipwreck's keys, locked doors, doors that stay shut and the boss in the
// last cabin to open; a power plant's Voltorb balls.
static void PlaceCabinsAndDecoys(struct Gen *gen, const struct RogueRoom *room, u8 dungeon, u8 floor)
{
    const struct RogueRegion *r = gen->r;
    const struct RogueDungeon *d = &r->dungeons[dungeon];
    const struct RogueRoomSpot *s;
    struct RogueDungeonExtras e;
    u8 locked[MAX_FLOOR_LOCKS], keyIn[MAX_FLOOR_LOCKS], last, n, base, i, k = 0, f;

    RogueGen_DungeonExtras(r, dungeon, &e);
    n = FloorLocks(r, dungeon, floor, locked, keyIn, &last);
    base = KeysBefore(r, dungeon, floor, NULL);
    // (Out of item flags, no keys: then every door stays open.)
    for (i = 0; i < n && e.keys != 0xFF; i++)
    {
        if ((s = CabinSpot(room, ROOM_SPOT_KEY, keyIn[i])) != NULL)
            AddObj(gen, ROBJ_KEY, s->x, s->y, RDIR_SOUTH, e.keys + base + i, 0);
        if ((s = CabinSpot(room, ROOM_SPOT_LOCK, locked[i])) != NULL)
            AddSign(gen, s->x, s->y, RSIGN_LOCK + base + i);
    }
    if (floor == d->nFloors && d->boss != RBOSS_NONE && last != 0xFF && (s = CabinSpot(room, ROOM_SPOT_HOLD, last)) != NULL)
    {
        AddObj(gen, ROBJ_BOSS, s->x, s->y, RDIR_SOUTH, dungeon, 4);
        if (e.prize != 0xFF && (s = CabinSpot(room, ROOM_SPOT_HOLD_PRIZE, last)) != NULL)
            AddObj(gen, ROBJ_PRIZE, s->x, s->y, RDIR_SOUTH, e.prize, 0);
    }
    for (i = 0; i < room->nSpots; i++)
        if (room->spots[i].kind == ROOM_SPOT_SHUT)
            AddSign(gen, room->spots[i].x, room->spots[i].y, RSIGN_LOCKED);
    if (e.decoys == 0xFF)
        return;
    for (f = 1; f < floor; f++)
        k += RoomSpotCount(r, dungeon, f, ROOM_SPOT_DECOY);
    for (i = 0; i < room->nSpots; i++)
        if (room->spots[i].kind == ROOM_SPOT_DECOY)
            AddObj(gen, ROBJ_DECOY, room->spots[i].x, room->spots[i].y, RDIR_SOUTH, e.decoys + k++, 0);
}

// The blocks a trainer object sees, as a bit per block in seen (its
// directions as it faces, turns or looks round, up to range, stopped by a
// wall or anyone standing): TRUE if one of them is set in against, else
// adds them. With against NULL it only adds. stop gets the shortest range
// that keeps clear of against (range itself if none).
static bool8 RoomSight(struct Gen *gen, const struct RogueObj *o, int range, u8 *seen, const u8 *against, int *stop)
{
    const struct RogueMap *m = gen->m;
    u8 dirs[4], nd = 0, k;
    bool8 hit = FALSE;
    if (o->facing & ROBJ_LOOKS)
        dirs[nd++] = 0, dirs[nd++] = 1, dirs[nd++] = 2, dirs[nd++] = 3;
    else if (o->facing & ROBJ_TURNS)
        dirs[nd++] = o->facing & 3, dirs[nd++] = (o->facing >> 2) & 3;
    else
        dirs[nd++] = o->facing & 3;
    *stop = range;
    for (k = 0; k < nd; k++)
    {
        int n, j;
        for (n = 1; n <= range; n++)
        {
            int x = o->x + sDirDX[dirs[k]] * n, y = o->y + sDirDY[dirs[k]] * n, b;
            bool8 busy = FALSE;
            if (x < 0 || y < 0 || x >= m->w || y >= m->h || (m->blocks[y * m->w + x] & 0x0C00))
                break;
            for (j = 0; j < m->nObjs; j++)
                busy |= m->objs[j].x == x && m->objs[j].y == y;
            if (busy)
                break;
            b = y * m->w + x;
            if (against != NULL && ((against[b >> 3] >> (b & 7)) & 1))
            {
                hit = TRUE;
                if (n - 1 < *stop)
                    *stop = n - 1;
                break;
            }
            if (seen != NULL)
                seen[b >> 3] |= 1 << (b & 7);
        }
    }
    return hit;
}

// No block of a floor is seen by its boss (a trainer: an admin, a master)
// or a team's leader and by another trainer at once: a player stepping
// there would be caught by both, and a boss pulled into a battle of two
// trainers (the game keeps them apart too: Rogue_IsSoloTrainer). The other
// trainers' sight stops short of the boss's.
static void SeparateBossSight(struct Gen *gen, bool8 bossTrainer)
{
    struct RogueMap *m = gen->m;
    u8 *boss = gen->onWalk; // (a bit a block, free once the floor is laid)
    int i, stop;
    bool8 any = FALSE;
    memset(boss, 0, sizeof(gen->onWalk));
    for (i = 0; i < m->nObjs; i++)
        if ((m->objs[i].kind == ROBJ_BOSS && bossTrainer) || m->objs[i].kind == ROBJ_VILLAIN_LEADER)
        {
            RoomSight(gen, &m->objs[i], m->objs[i].range > 4 ? m->objs[i].range : 4, boss, NULL, &stop);
            any = TRUE;
        }
    if (!any)
        return;
    for (i = 0; i < m->nObjs; i++)
        if (m->objs[i].kind == ROBJ_TRAINER && RoomSight(gen, &m->objs[i], m->objs[i].range, NULL, boss, &stop))
            m->objs[i].range = stop;
}

static void GenRoomFloor(const struct RogueRegion *r, u8 dungeon, u8 floor, struct RogueMap *m, u16 *blocks)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon];
    const struct RogueRoom *room = &sRogueRooms[RogueGen_FloorRoom(r, dungeon, floor)];
    struct Gen *gen = &sGen;
    struct RoguePlace *p = &sFloorGenPlace;
    // Ruins shift: the same floor is laid out again on each visit.
    u32 used = 0, rng = RogueRng_Hash(r->seed, 60000 + dungeon * 4000 + floor * 200 + (IsRuins(d) ? 1 + FloorVisit(d) : 0));
    u8 i, k, nPlain;
    static const u8 sTrainerSpots[] = { ROOM_SPOT_TRAINER, ROOM_SPOT_PERSON, ROOM_SPOT_OPEN };
    static const u8 sItemSpots[] = { ROOM_SPOT_ITEM, ROOM_SPOT_OPEN };

    RogueGen_FloorPlace(r, dungeon, floor, p);
    memset(m, 0, sizeof(*m));
    m->place = d->host;
    m->dungeon = dungeon;
    m->floor = floor;
    m->blocks = blocks;
    m->w = room->w;
    m->h = room->h;
    // A room bigger than a slot (New Mauville) is drawn straight from ROM:
    // nothing writes to a room's blocks.
    if (room->w * room->h > ROGUE_MAX_BLOCKS)
        m->blocks = (u16 *)room->blocks;
    else
        memcpy(blocks, room->blocks, room->w * room->h * sizeof(u16));
    YIELD();
    memset(gen, 0, sizeof(*gen));
    gen->r = r;
    gen->p = p;
    gen->m = m;
    gen->cw = m->w / 2;
    gen->ch = m->h / 2;
    // A warp's kind is its index in the room (RogueGen_RoomWarpDest).
    for (i = 0; i < room->nWarps; i++)
        AddWarp(gen, room->warps[i].x, room->warps[i].y, i);
    nPlain = p->nTrainers;
    if (floor == d->nFloors && d->boss != RBOSS_NONE)
    {
        u8 s = TakeRoomSpot(room, &used, ROOM_SPOT_BOSS, &rng);
        if (BossIsTrainer(d) && nPlain)
            nPlain--;
        if (s != 0xFF)
            AddObj(gen, ROBJ_BOSS, room->spots[s].x, room->spots[s].y, room->spots[s].facing,
                   BossIsTrainer(d) ? p->nTrainers - 1 : dungeon, room->spots[s].range);
    }
    // A villain team's leader, before the legend he is after: slot is the
    // team.
    if (floor == d->nFloors && d->boss == RBOSS_LEGEND && ROGUE_GATE_KIND(d->gate) == RGATE_VILLAIN
     && (s32)dungeon == RogueGen_TeamLeaderDungeon(r, ROGUE_GATE_SRC(d->gate)))
    {
        u8 s = TakeRoomSpot(room, &used, ROOM_SPOT_LEADER, &rng);
        if (s != 0xFF)
            AddObj(gen, ROBJ_VILLAIN_LEADER, room->spots[s].x, room->spots[s].y, room->spots[s].facing,
                   ROGUE_GATE_SRC(d->gate), room->spots[s].range);
    }
    // Someone lost, where a trainer or an item could have stood (first, so
    // there is room).
    {
        struct RogueDungeonExtras e;
        RogueGen_DungeonExtras(r, dungeon, &e);
        if (e.explorer != 0xFF && floor == e.explorerFloor)
        {
            u8 s = TakeRoomSpot(room, &used, ROOM_SPOT_OPEN, &rng);
            if (s == 0xFF)
                s = TakeRoomSpot(room, &used, ROOM_SPOT_PERSON, &rng);
            if (s != 0xFF)
                AddObj(gen, ROBJ_EXPLORER, room->spots[s].x, room->spots[s].y, room->spots[s].facing & 3, e.explorer, 0);
        }
    }
    for (i = 0; i < nPlain; i++)
    {
        u8 s = 0xFF;
        for (k = 0; k < sizeof(sTrainerSpots) && s == 0xFF; k++)
            s = TakeRoomSpot(room, &used, sTrainerSpots[k], &rng);
        if (s != 0xFF)
            AddObj(gen, ROBJ_TRAINER, room->spots[s].x, room->spots[s].y, room->spots[s].facing, i, room->spots[s].range);
    }
    for (i = 0; i < p->nItems; i++)
    {
        u8 s = 0xFF;
        // A wreck's storage room keeps the floor's first find
        // (RogueGen_ShipStorageItem).
        if (i == 0 && StorageSpot(r, dungeon, floor) != NULL)
        {
            const struct RogueRoomSpot *st = StorageSpot(r, dungeon, floor);
            AddObj(gen, ROBJ_ITEM, st->x, st->y, RDIR_SOUTH, i, 0);
            continue;
        }
        for (k = 0; k < sizeof(sItemSpots) && s == 0xFF; k++)
            s = TakeRoomSpot(room, &used, sItemSpots[k], &rng);
        if (s != 0xFF)
            AddObj(gen, ROBJ_ITEM, room->spots[s].x, room->spots[s].y, RDIR_SOUTH, i, 0);
    }
    // The extras on the places the room keeps for them (PlanExtras chose
    // floors whose rooms have them). The choice row's spots run left to right.
    {
        struct RogueDungeonExtras e;
        u8 nChoice = 0;
        RogueGen_DungeonExtras(r, dungeon, &e);
        for (i = 0; i < room->nSpots; i++)
        {
            const struct RogueRoomSpot *s = &room->spots[i];
            if (s->kind == ROOM_SPOT_PRIZE && e.prize != 0xFF && floor == d->nFloors)
                AddObj(gen, ROBJ_PRIZE, s->x, s->y, RDIR_SOUTH, e.prize, 0);
            else if (s->kind == ROOM_SPOT_CHOICE && e.nChoice && floor == e.choiceFloor && nChoice < e.nChoice)
                AddObj(gen, ROBJ_CHOICE, s->x, s->y, RDIR_SOUTH, e.choice + nChoice++, 0);
            else if (s->kind == ROOM_SPOT_CAMP && e.camp != 0xFF && floor == e.campFloor)
                AddObj(gen, ROBJ_CAMP, s->x, s->y, RDIR_SOUTH, e.camp, 0);
            else if (s->kind == ROOM_SPOT_GUIDE && e.nChoice && floor == e.choiceFloor)
                AddObj(gen, ROBJ_GUIDE, s->x, s->y, s->facing & 3, 0, 0);
            else if (s->kind == ROOM_SPOT_CARVING && d->boss == RBOSS_LEGEND)
                AddSign(gen, s->x, s->y, RSIGN_CARVING);
            else if (s->kind == ROOM_SPOT_TABLET)
                AddSign(gen, s->x, s->y, RSIGN_TABLET);
        }
    }
    if (d->kind == RDUN_SHIP || d->kind == RDUN_PLANT)
        PlaceCabinsAndDecoys(gen, room, dungeon, floor);
    // What keeps a legend's chamber shut: at the narrow place on the walk
    // in, or where the legend appears once it opens (a rite's stone, the
    // marks of the rest of its set, and any room open all round).
    if (RogueGen_HasChamber(d) && RogueGen_SealAtLegend(r, dungeon) != 0xFF)
    {
        u8 s = 0xFF;
        if (!RogueGen_SealAtLegend(r, dungeon))
            s = TakeRoomSpot(room, &used, ROOM_SPOT_GATE, &rng);
        for (i = 0; i < room->nSpots && s == 0xFF && ROGUE_GATE_KIND(d->gate) == RGATE_VILLAIN; i++)
            if (room->spots[i].kind == ROOM_SPOT_LEADER)
                s = i;
        if (s == 0xFF)
            for (i = 0; i < room->nSpots && s == 0xFF; i++)
                if (room->spots[i].kind == ROOM_SPOT_BOSS)
                    s = i;
        if (s != 0xFF)
            AddObj(gen, ROBJ_SEAL, room->spots[s].x, room->spots[s].y, room->spots[s].facing & 3, dungeon, 0);
    }
    SeparateBossSight(gen, BossIsTrainer(d));
#ifdef ROGUE_DEBUG
    fprintf(stderr, "dungeon %d floor %d room %d\n", dungeon, floor, (int)(room - sRogueRooms));
#endif
}

// ---------------------------------------------------------------------------
// Entrances in town

// A villain hideout's way in, never one of the town's homes: Stern's
// shipyard in a Harbor town, the shut Game Corner in a Verdant or Island
// town, the Weather Institute in Treetop's woods (Team Aqua took Route
// 119's), else a door in a rock face along the top as Lilycove's Aqua
// Hideout is (on a beach or by the springs too, as Dewford's and Jagged
// Pass's rock; beside a cave's own mouth in a town at a cave's end), else
// Devon Corp. in a Stone town. RENTRY_HOUSE only where none of those can
// stand: a harbor with the sea along its top, or a cliff along it, which
// hideouts are kept out of where the road has another town (HideoutTownOk).
static u8 HideoutEntrance(const struct RogueRegion *r, const struct RoguePlace *p)
{
    int x0, w;
    if (p->theme == RTHEME_HARBOR)
        return RENTRY_BUILDING;
    if (p->theme == RTHEME_VERDANT || p->theme == RTHEME_ISLAND)
        return RENTRY_GAME_CORNER;
    // A town at a cave's far end: a second door in the cave's rock face
    // (PlaceCaveHillTwin), where a big building seldom fits under it.
    if (p->mouth != 0xFF && p->type != RTOWN_SUMMIT)
        return RENTRY_MESA;
    if (p->theme == RTHEME_TREETOP)
        return RENTRY_BUILDING;
    if (HideoutMesaSpan(r, p, &x0, &w))
        return RENTRY_MESA;
    if (p->theme == RTHEME_STONE)
        return RENTRY_BUILDING;
    return RENTRY_HOUSE;
}

// How a dungeon in a town is entered (see GenTown): the theme's own tall
// building or warehouse, the Mirage Tower for a tower (Rustboro's is Devon
// Corp.), the shut Game Corner, a door in a rock face along the top of the
// town for a hideout or a lair as the Aqua Hideout's and Terra Cave's are,
// or failing those a plain house.
static u8 TownDungeonEntrance(const struct RogueRegion *r, u8 place)
{
    const struct RoguePlace *p = &r->places[place];
    s32 di = RogueGen_DungeonAt(r, place);
    int x0, w;
    u8 kind;
    if (di >= 0 && p->kind == RPLACE_TOWN && RogueGen_TunnelMouthAt(r, place))
        return RENTRY_MESA; // a tunnel's far mouth (PlanTunnel checked the rock has room)
    if (di < 0 || p->kind != RPLACE_TOWN || r->dungeons[di].host != place)
        return RENTRY_NONE;
    kind = r->dungeons[di].kind;
    if (kind == RDUN_TOWER && p->theme == RTHEME_STONE)
        return RENTRY_BUILDING;
    if (kind == RDUN_TOWER && sTowerBuildings[p->theme].stamp)
        return RENTRY_TOWER;
    if (kind == RDUN_HIDEOUT)
        return HideoutEntrance(r, p);
    // (v1.1) A spring town's volcano: its own mountain (VolcanoMountainAt).
    if (kind == RDUN_VOLCANO && VolcanoMountainFits(r, place))
        return RENTRY_MESA;
    // (v1.1 legends) A lair's door: clear of a harbor's beach (LairMesaSpan).
    if (kind == RDUN_LAIR && p->type != RTOWN_SUMMIT && LairMesaSpan(r, p, &x0, &w))
        return RENTRY_MESA;
    if (p->mouth == 0xFF && DungeonMesaSpan(r, p, &x0, &w) && p->type != RTOWN_SUMMIT
     && p->theme != RTHEME_SPRING && p->theme != RTHEME_TREETOP && p->theme != RTHEME_BEACH
     && !(p->layout == RLAYOUT_HARBOR && p->seaSide != RDIR_NONE))
        return RENTRY_MESA;
    return RENTRY_HOUSE;
}

// ---------------------------------------------------------------------------
// Planning

static bool8 AddDungeon(struct RogueRegion *r, u8 kind, u8 host, u8 nFloors, u8 boss, bool8 required);

// A town that can take a tower: a gym city or village on the main road
// between the second gym and the sixth, entered by a tall building or a
// door in the rock.
static bool8 TowerTownOk(const struct RogueRegion *r, u8 place)
{
    const struct RoguePlace *p = &r->places[place];
    int x0, w;
    if (p->kind != RPLACE_TOWN || p->side || RogueGen_DungeonAt(r, place) >= 0 || TOWNS2_TOWN(p))
        return FALSE;
    if (p->type == RTOWN_GYM ? (p->gym < 1 || p->gym > 5) : (p->type != RTOWN_VILLAGE || p->badges < 2 || p->badges > 5))
        return FALSE;
    if (p->theme == RTHEME_STONE)
        return TRUE;
    return p->mouth == 0xFF && DungeonMesaSpan(r, p, &x0, &w) && p->theme != RTHEME_SPRING
        && p->theme != RTHEME_TREETOP && p->theme != RTHEME_BEACH
        && !(p->layout == RLAYOUT_HARBOR && p->seaSide != RDIR_NONE);
}

// Most regions have a tower, like Mt. Pyre or Lavender's Pokemon Tower: off
// the way on, climbed three to five floors to a master at the top. A Stone
// town, with its tall building, is the likeliest.
static void PlanTowerDungeons(struct RogueRegion *r)
{
    u32 rng = RogueRng_Hash(r->seed, 11100), total = 0, pick;
    u8 i, weight[ROGUE_MAX_PLACES];

    // (RUN 0 always has one: the graveyard tower, src/rogue_gen_run0.h.)
    if (RogueRng_Range(&rng, 100) >= 90 && !RogueGen_IsRun0(r))
        return;
    for (i = 0; i < r->nPlaces; i++)
    {
        weight[i] = TowerTownOk(r, i) ? (r->places[i].theme == RTHEME_STONE ? 4 : 1) : 0;
        total += weight[i];
    }
    // RUN 0's graveyard tower: if no town can take one, a village between
    // the second gym and the sixth (else a gym city there, if no other gym
    // city is one already) is made a Stone town, Rustboro's, with its tall
    // building.
    if (total == 0 && RogueGen_IsRun0(r))
    {
        bool8 stoneGym = FALSE;
        u8 pass;
        for (i = 0; i < r->nPlaces; i++)
            stoneGym |= r->places[i].kind == RPLACE_TOWN && r->places[i].type == RTOWN_GYM && r->places[i].theme == RTHEME_STONE;
        for (pass = 0; pass < 2 && total == 0; pass++)
        {
            for (i = 0; i < r->nPlaces && total == 0; i++)
            {
                struct RoguePlace *p = &r->places[i];
                if (p->kind != RPLACE_TOWN || p->side || RogueGen_DungeonAt(r, i) >= 0)
                    continue;
                if (pass == 0 ? (p->type != RTOWN_VILLAGE || p->badges < 2 || p->badges > 5)
                              : (p->type != RTOWN_GYM || p->gym < 1 || p->gym > 5 || stoneGym))
                    continue;
                p->theme = RTHEME_STONE;
                weight[i] = 4;
                total = 4;
            }
        }
    }
    if (total == 0)
        return;
    pick = RogueRng_Range(&rng, total);
    for (i = 0; i < r->nPlaces; i++)
    {
        if (pick < weight[i])
        {
            u8 most = RoomStyleFloors(ROOM_STYLE_PYRE) < 5 ? RoomStyleFloors(ROOM_STYLE_PYRE) : 5;
            AddDungeon(r, RDUN_TOWER, i, 3 + RogueRng_Range(&rng, most - 2), RBOSS_MASTER, FALSE);
            return;
        }
        pick -= weight[i];
    }
}

