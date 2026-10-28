// Ruins, and the lost explorer. Included by rogue_gen_dungeon.h.
//
// Ruins (RDUN_RUINS) are Hoenn's Desert Ruins, Mirage Tower and Sky Pillar
// put together: below a side cave in sand or hills, a ladder leads down
// into the Mirage Tower's sandy floors, and at the bottom lies a vault, a
// Regi chamber (Desert Ruins, Island Cave or the Ancient Tomb) whose inner
// door is shut. Its three tablets open it when read in the order a riddle
// the townsfolk tell gives. A guardian of the old kind (Claydol, Bronzong,
// Golurk...) waits inside with the vault's treasure, a choice of stones.
//
// The floors shift, as a Mystery Dungeon's do: the sandy floors come in
// another order and everything on them stands somewhere else on every
// visit (RogueGen_DungeonVisit). What is on them stays the same: the same
// trainer slots and item flags, so nothing beaten or taken comes back, and
// the floors share the cave's one catch. Their cracked floor is kept as
// sand pits (gen_rooms.py leaves a way round them): a step onto one drops
// the player to the floor below, where the fall lands (RogueGen_RuinsLandWarp).

static bool8 IsRuins(const struct RogueDungeon *d)
{
    return d->kind == RDUN_RUINS;
}

s32 RogueGen_RuinsDungeon(const struct RogueRegion *r)
{
    int i;
    for (i = 0; i < r->nDungeons; i++)
        if (IsRuins(&r->dungeons[i]))
            return i;
    return -1;
}

// The visit the floors of a dungeon are made for: ruins shift with every
// visit, the others never do.
static u8 FloorVisit(const struct RogueDungeon *d)
{
    return IsRuins(d) ? (RogueGen_DungeonVisit() & 63) : 0;
}

// How well a side cave suits ruins (0: not at all): a dead end past the
// first badge, not by a volcano (its caves are another kind's), in rough
// country, the nearer a sandy or hilly route on the region's grid the
// better (side caves hang off woods and lakes, so a desert or hills a cell
// or three away is the land they lie in).
static u8 RuinsCaveFit(const struct RogueRegion *r, const struct RogueDungeon *d)
{
    const struct RoguePlace *host = &r->places[d->host];
    int i, best = 99;
    if (d->kind != RDUN_CAVE || d->required || d->exit != 0xFF || host->kind != RPLACE_CAVE
     || host->caveOut != 0xFF || host->caveIn >= r->nPlaces || d->badges < 1)
        return 0;
    if (r->places[host->caveIn].theme == RTHEME_ASH || r->places[host->caveIn].theme == RTHEME_SPRING)
        return 0;
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        int dist = Abs(p->gridX - host->gridX) + Abs(p->gridY - host->gridY);
        if (p->kind == RPLACE_ROUTE && (p->type == RROUTE_DESERT || p->type == RROUTE_HILLS) && dist < best)
            best = dist;
    }
    return best <= 3 ? 6 - best : best <= 5 ? 1 : 0;
}

// A region with such a cave has ruins below it (one at most): two or
// three of the Mirage Tower's floors, then the vault.
static void PlanRuinsDungeons(struct RogueRegion *r)
{
    u32 rng = RogueRng_Hash(r->seed, 11450), total = 0, pick;
    int i;
    for (i = 0; i < r->nDungeons; i++)
        total += RuinsCaveFit(r, &r->dungeons[i]);
    if (total == 0)
        return;
    pick = RogueRng_Range(&rng, total);
    for (i = 0; i < r->nDungeons; i++)
    {
        struct RogueDungeon *d = &r->dungeons[i];
        u8 fit = RuinsCaveFit(r, d);
        if (pick >= fit)
        {
            pick -= fit;
            continue;
        }
        d->kind = RDUN_RUINS;
        d->boss = RBOSS_GUARDIAN;
        d->nFloors = 3 + (d->badges >= 4 && RogueRng_Range(&rng, 100) < 40);
        return;
    }
}

// The rooms of a ruins dungeon's floors: the sandy floors (ROOM_STYLE_SANDS)
// shuffled for this visit, then the vault, the same every visit (the riddle
// is told of it): one no legend of the region has for its chamber.
static u8 RuinsFloorRoom(const struct RogueRegion *r, u8 dungeon, u8 floor)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon];
    u8 ids[8], n = 0, i, k, taken = 0;
    u32 rng;
    if (floor == d->nFloors)
    {
        static const u8 sChambers[] = { ROOM_STYLE_RUINS, ROOM_STYLE_ISLAND_CAVE, ROOM_STYLE_TOMB };
        for (i = 0; i < r->nDungeons; i++)
            for (k = 0; k < 3; k++)
                if (RogueGen_HasChamber(&r->dungeons[i]) && LairStyle(r, i) == sChambers[k])
                    taken |= 1 << k;
        // The vault rooms come in the order of sChambers (gen_rooms.py).
        for (i = 0, k = 0; i < NUM_ROOMS; i++)
        {
            if (sRogueRooms[i].style != ROOM_STYLE_VAULT)
                continue;
            if (!((taken >> k) & 1) && n < sizeof(ids))
                ids[n++] = i;
            k++;
        }
        // All three taken: any will do.
        if (n == 0)
            for (i = 0; i < NUM_ROOMS && n < sizeof(ids); i++)
                if (sRogueRooms[i].style == ROOM_STYLE_VAULT)
                    ids[n++] = i;
        if (n == 0)
            return 0xFF;
        rng = RogueRng_Hash(r->seed, 14600 + dungeon);
        return ids[RogueRng_Range(&rng, n)];
    }
    for (i = 0; i < NUM_ROOMS && n < sizeof(ids); i++)
        if (sRogueRooms[i].style == ROOM_STYLE_SANDS)
            ids[n++] = i;
    if (n == 0)
        return 0xFF;
    rng = RogueRng_Hash(r->seed, 14500 + dungeon * 64 + FloorVisit(d));
    for (i = n - 1; i > 0; i--)
    {
        u8 j = RogueRng_Range(&rng, i + 1), t = ids[i];
        ids[i] = ids[j];
        ids[j] = t;
    }
    return ids[(floor - 1) % n];
}

u8 RogueGen_RuinsLandWarp(const struct RogueRegion *r, u8 dungeon, u8 floor)
{
    u8 id, i;
    // (v1.1 peaks: and the desert tower's, src/rogue_gen_peaks.h.)
    if (dungeon >= r->nDungeons || (!IsRuins(&r->dungeons[dungeon]) && r->dungeons[dungeon].kind != RDUN_MIRAGE)
     || floor < 1 || floor > r->dungeons[dungeon].nFloors)
        return 0xFF;
    id = RogueGen_FloorRoom(r, dungeon, floor);
    if (id == 0xFF)
        return 0xFF;
    for (i = 0; i < sRogueRooms[id].nWarps; i++)
        if (sRogueRooms[id].warps[i].role == ROOM_LAND)
            return i;
    return 0xFF;
}

void RogueGen_RuinsRiddle(const struct RogueRegion *r, u8 dungeon, u8 words[3], u8 order[3])
{
    u32 rng = RogueRng_Hash(r->seed, 14700 + dungeon);
    u8 i, j, t;
    // Three different words, and an order that is not just left to right.
    words[0] = RogueRng_Range(&rng, RUINS_WORDS);
    do
        words[1] = RogueRng_Range(&rng, RUINS_WORDS);
    while (words[1] == words[0]);
    do
        words[2] = RogueRng_Range(&rng, RUINS_WORDS);
    while (words[2] == words[0] || words[2] == words[1]);
    do
    {
        for (i = 0; i < 3; i++)
            order[i] = i;
        for (i = 2; i > 0; i--)
        {
            j = RogueRng_Range(&rng, i + 1);
            t = order[i];
            order[i] = order[j];
            order[j] = t;
        }
    } while (order[0] == 0 && order[1] == 1);
}

// A ruins dungeon's trainers stand on its sandy floors, none in the vault.
static void RuinsFloorSlots(const struct RogueDungeon *d, u8 floor, bool8 items, u8 total, u8 *first, u8 *count)
{
    if (items)
    {
        FloorShare(total, d->nFloors, floor, first, count);
        return;
    }
    if (floor >= d->nFloors)
    {
        *first = total;
        *count = 0;
        return;
    }
    FloorShare(total, d->nFloors - 1, floor, first, count);
}

// ---------------------------------------------------------------------------
// The lost explorer
//
// Now and then (in about two regions of three) someone is lost deep in one
// of the region's dungeons, of any kind but the villains' hideout: a hiker
// in a cave, a picnicker in the woods, a girl in a tower, a ruin maniac in
// ruins. Spoken to, they ask to be led out; the player who agrees is taken
// out with them at once (their ESCAPE ROPE), and thanked with a reward at
// the way in. The rescue is a choice: out now, or finish the dungeon first
// and come back down for them.

// Whether a tower floor's room has a place to spare for someone lost: an
// open place or a person's beyond what its trainers and items take.
static bool8 RoomSpareSpot(const struct RogueRegion *r, u8 dungeon, u8 floor)
{
    // (v1.1 peaks) The misty mountainside has room enough.
    if (floor != 0 && floor == RogueGen_PyreSlopeFloor(r, dungeon))
        return TRUE;
    u8 t, i, trainers = RoomSpotCount(r, dungeon, floor, ROOM_SPOT_TRAINER),
       items = RoomSpotCount(r, dungeon, floor, ROOM_SPOT_ITEM),
       free = RoomSpotCount(r, dungeon, floor, ROOM_SPOT_OPEN) + RoomSpotCount(r, dungeon, floor, ROOM_SPOT_PERSON);
    RoomFloorCounts(r, dungeon, floor, &t, &i);
    free -= (t > trainers ? t - trainers : 0);
    if (i > items)
        free -= i - items;
    return (s8)free >= 1;
}

// The floor of a dungeon someone may be lost on (deep in), or 0.
static u8 ExplorerFloorOf(const struct RogueRegion *r, u8 dungeon)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon];
    u8 floors = DungeonGenFloors(d), f;
    switch (d->kind)
    {
    case RDUN_HIDEOUT:
        return 0;
    case RDUN_TOWER:
    case RDUN_SHIP:
    case RDUN_MIRAGE:
        // The highest floor below the top (the master's, or a wreck's hold
        // with its locked cabins) with room.
        for (f = d->nFloors - 1; f >= 2; f--)
            if (RoomSpareSpot(r, dungeon, f))
                return f;
        return 0;
    case RDUN_RUINS:
        // The last sandy floor, before the vault.
        return d->nFloors - 1;
    default:
        // The deepest cave or woods floor (before a legend's chamber).
        return floors >= 2 ? floors : 0;
    }
}

// The region's dungeon with someone lost in it, or -1.
static s32 ExplorerDungeon(const struct RogueRegion *r)
{
    u32 rng = RogueRng_Hash(r->seed, 11650), n = 0, pick;
    int i;
    if (RogueRng_Range(&rng, 100) >= 70)
        return -1;
    for (i = 0; i < r->nDungeons; i++)
        n += ExplorerFloorOf(r, i) != 0;
    if (n == 0)
        return -1;
    pick = RogueRng_Range(&rng, n);
    for (i = 0; i < r->nDungeons; i++)
        if (ExplorerFloorOf(r, i) != 0 && pick-- == 0)
            return i;
    return -1;
}
