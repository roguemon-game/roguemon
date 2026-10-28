// Shipwrecks (RDUN_SHIP): Route 108's Abandoned Ship on a sea route.
// Included by rogue_gen_dungeon.h.
//
// The wreck lies out on the water of a sea route on the main road, on its
// shoal as on Route 108, surfed up to and climbed into by the stairs at its
// stern. Its floors are the Abandoned Ship's own rooms (src/rogue_gen_rooms.h,
// gen_rooms.py COMPOSE): the deck, 1F and B1F with their cabins, and the
// hidden deck, where cabin keys open one cabin after another.
//
// The hull is Slateport's tiles, so a route with a wreck uses Slateport's
// tileset (its theme becomes RTHEME_HARBOR): a sea route draws nothing but
// the General tileset's water, sand, rocks and trees, so only the wreck
// changes, and the towns at its ends keep their own tiles out of its view
// as they do beside any route of another tileset.

// Route 108's wreck on its shoal (11x8 blocks, a column of open sea beside
// it): the shoal's left and right edges are Route 108's own from its shoals further
// east. The stairs (MB_STAIRS_OUTSIDE_ABANDONED_SHIP) are the way in, all
// three blocks of them (Slateport's metatile attributes give the side ones
// the middle one's behaviour), so surfing up to any part of the stairs and
// climbing on boards, not only the middle block as on Route 108. The shoal
// block left of the hull, walled by a rock and the hull, is at the sea's
// height here, so no one lands on it with nowhere to walk.
#define SHIP_STAMP_W 12
#define SHIP_STAMP_H 8
#define SHIP_STAIRS_X 6
#define SHIP_STAIRS_Y 6
static const u16 sShipStamp[SHIP_STAMP_W * SHIP_STAMP_H] = {
    0x11AD, 0x31AE, 0x31AE, 0x31AE, 0x31AE, 0x31AE, 0x31AE, 0x31AE, 0x31AE, 0x31AE, 0x11AF, 0x1170,
    0x11B5, 0x319E, 0x355E, 0x355F, 0x319E, 0x3594, 0x355E, 0x355F, 0x319E, 0x319E, 0x11B7, 0x1170,
    0x11B5, 0x3594, 0x356E, 0x356F, 0x3594, 0x319E, 0x356E, 0x356F, 0x3594, 0x3594, 0x11B7, 0x1170,
    0x11B5, 0x319E, 0x355E, 0x355F, 0x319E, 0x3351, 0x3352, 0x3353, 0x3354, 0x3594, 0x11B7, 0x1170,
    0x11BD, 0x11A3, 0x356E, 0x356F, 0x3758, 0x0759, 0x075A, 0x075B, 0x375C, 0x175D, 0x11B7, 0x1170,
    0x1170, 0x11BD, 0x11A3, 0x119E, 0x1760, 0x0761, 0x0762, 0x0763, 0x1764, 0x1765, 0x11B7, 0x1170,
    0x1170, 0x1170, 0x11BD, 0x11BE, 0x11A3, 0x3369, 0x336A, 0x336B, 0x11A4, 0x11BE, 0x11BF, 0x1170,
    0x1170, 0x1170, 0x1170, 0x1170, 0x11BD, 0x11BE, 0x11BE, 0x11BE, 0x11BF, 0x1170, 0x1170, 0x1170,
};

// The shipwreck on this route, or -1.
static s32 ShipOf(const struct RogueRegion *r, u8 place)
{
    s32 di = RogueGen_DungeonAt(r, place);
    return (di >= 0 && r->dungeons[di].kind == RDUN_SHIP && r->dungeons[di].host == place) ? di : -1;
}

// TRUE if the units from (ux, uy), w x h, overlap a placed decoration.
static bool8 UnitsOnDecor(struct Gen *gen, int ux, int uy, int w, int h)
{
    int i;
    for (i = 0; i < gen->nDecor; i++)
    {
        const struct Decor *d = &gen->decor[i];
        if (d->x < (ux + w) * 2 && d->x + d->w > ux * 2 && d->y < (uy + h) * 2 && d->y + d->h > uy * 2)
            return TRUE;
    }
    return FALSE;
}

// Whether the two landings still join by water and land with the units
// from (sx, sy), w x h, shut.
static bool8 SeaWayAround(struct Gen *gen, int ax, int ay, int bx, int by, int sx, int sy, int w, int h)
{
    int head = 0, tail = 0, n = gen->cw * gen->ch, i;
    u8 *seen = gen->cover; // free until the cover is painted
    memset(seen, 0, n);
    seen[ay * gen->cw + ax] = 1;
    sBfsQueue[tail++] = ay * gen->cw + ax;
    while (head < tail)
    {
        int cur = sBfsQueue[head++], cx = cur % gen->cw, cy = cur / gen->cw;
        u8 d;
        if (cx == bx && cy == by)
        {
            memset(seen, 0, n);
            return TRUE;
        }
        for (d = 0; d < 4; d++)
        {
            int nx = cx + sDirDX[d], ny = cy + sDirDY[d], ni = ny * gen->cw + nx;
            if (!InBounds(gen, nx, ny) || seen[ni] || tail >= BFS_RING)
                continue;
            if (Cell(gen, nx, ny) == C_TREE || (nx >= sx && nx < sx + w && ny >= sy && ny < sy + h))
                continue;
            seen[ni] = 1;
            sBfsQueue[tail++] = ni;
        }
    }
    for (i = 0; i < n; i++)
        seen[i] = 0;
    return FALSE;
}

// Lays the wreck out on open water, clear of the islands and the landings,
// out of view of any neighbour drawn with another tileset, and not across
// the only way over the water. FALSE if there is no room (the route is
// made again). Call before the cover is painted.
static bool8 PlaceShipwreck(struct Gen *gen, int ax, int ay, int bx, int by)
{
    int uw = SHIP_STAMP_W / 2, uh = SHIP_STAMP_H / 2, tries;
    if (ShipOf(gen->r, gen->m->place) < 0)
        return TRUE;
    for (tries = 0; tries < 200; tries++)
    {
        int ux = 1 + RAND(gen->cw - uw - 1), uy = 1 + RAND(gen->ch - uh - 1), x, y;
        bool8 ok = TRUE;
        YIELD();
        // Open sea all round, a unit wide.
        for (y = uy - 1; y <= uy + uh && ok; y++)
            for (x = ux - 1; x <= ux + uw && ok; x++)
                if (!InBounds(gen, x, y) || Cell(gen, x, y) != C_SEA)
                    ok = FALSE;
        if (!ok || UnitsOnDecor(gen, ux - 1, uy - 1, uw + 2, uh + 2))
            continue;
        if (Abs(ux + uw / 2 - ax) + Abs(uy + uh / 2 - ay) < 6 || Abs(ux + uw / 2 - bx) + Abs(uy + uh / 2 - by) < 6)
            continue;
        if (InRouteSeam(gen, ux * 2, uy * 2) || InRouteSeam(gen, ux * 2 + SHIP_STAMP_W - 1, uy * 2 + SHIP_STAMP_H - 1)
         || InRouteSeam(gen, ux * 2, uy * 2 + SHIP_STAMP_H - 1) || InRouteSeam(gen, ux * 2 + SHIP_STAMP_W - 1, uy * 2))
            continue;
        if (!SeaWayAround(gen, ax, ay, bx, by, ux, uy, uw, uh))
            continue;
        if (!AddDecor(gen, sShipStamp, ux * 2, uy * 2, SHIP_STAMP_W, SHIP_STAMP_H))
            return FALSE;
        AddWarp(gen, ux * 2 + SHIP_STAIRS_X, uy * 2 + SHIP_STAIRS_Y, RWARP_DUNGEON);
        // (A sea route has no houses: the side stairs take the spare
        // house slots, and lead aboard as the middle does.)
        AddWarp(gen, ux * 2 + SHIP_STAIRS_X - 1, uy * 2 + SHIP_STAIRS_Y, RWARP_DUNGEON_WIDE);
        AddWarp(gen, ux * 2 + SHIP_STAIRS_X + 1, uy * 2 + SHIP_STAIRS_Y, RWARP_DUNGEON_2_WIDE);
        // Nobody stands on the wreck or its shoal.
        for (y = uy * 2; y < uy * 2 + SHIP_STAMP_H; y++)
            for (x = ux * 2; x < ux * 2 + SHIP_STAMP_W; x++)
                MarkOccupied(gen, x, y);
        return TRUE;
    }
    return FALSE;
}

// About two regions in three have a wreck, on a sea route of the main road
// (Surf is the key that opens it, as Route 108's). It takes its route's
// level a step up, as other optional dungeons do, and floors down to the
// hidden deck: 1F and B1F, or 1F alone.
static void PlanShipDungeons(struct RogueRegion *r)
{
    u32 rng = RogueRng_Hash(r->seed, 11900), total = 0, pick;
    u8 i, cand[ROGUE_MAX_PLACES];

    if (RogueRng_Range(&rng, 100) >= 70)
        return;
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        if (p->kind == RPLACE_ROUTE && p->type == RROUTE_SEA && !p->side && RogueGen_DungeonAt(r, i) < 0)
            cand[total++] = i;
    }
    if (total == 0)
        return;
    pick = cand[RogueRng_Range(&rng, total)];
    if (AddDungeon(r, RDUN_SHIP, pick, 3 + RogueRng_Range(&rng, 2), RBOSS_GUARDIAN, FALSE))
        r->places[pick].theme = RTHEME_HARBOR;
}

// (Here, after the items are known: the keys are Hoenn's.)
u16 RogueGen_KeyItem(const struct RogueRegion *r, u8 dungeon, u8 slot)
{
    // The storage room's key is the STORAGE KEY; the hidden deck's go by
    // the order their doors open, as Hoenn's numbered room keys.
    static const u16 sChainKeys[] = { ITEM_KEY_TO_ROOM_1, ITEM_KEY_TO_ROOM_2, ITEM_KEY_TO_ROOM_4, ITEM_KEY_TO_ROOM_6 };
    struct RogueDungeonExtras e;
    u8 f, k, n, base, forced;
    if (dungeon >= r->nDungeons || r->dungeons[dungeon].kind != RDUN_SHIP)
        return ITEM_NONE;
    RogueGen_DungeonExtras(r, dungeon, &e);
    if (e.keys == 0xFF || slot < e.keys || slot >= e.keys + e.nKeys)
        return ITEM_NONE;
    k = slot - e.keys;
    for (f = 1; f <= r->dungeons[dungeon].nFloors; f++)
    {
        base = KeysBefore(r, dungeon, f, &n);
        if (k >= base + n)
            continue;
        forced = RoomForcedLocks(r, dungeon, f);
        return k - base < forced ? ITEM_STORAGE_KEY : sChainKeys[(k - base - forced) % 4];
    }
    return ITEM_NONE;
}
