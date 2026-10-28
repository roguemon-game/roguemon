// Caves as dungeons: which caves get floors below, and what each cave map
// (the cave's own ground floor, 1F, and the floors below it) holds. Included
// by rogue_gen_dungeon.h; GenCave in rogue_gen.c draws the maps.
//
// A required dungeon is the way on, as Granite Cave and Victory Road are.
// Its 1F is in two parts that do not join: the way in leads to a ladder
// down, and the way out starts at a second ladder. The way through goes down
// the first, across the floors below and back up the second. Every floor
// above the deepest one the way reaches is split the same way, so the way
// down and the way back up are always different ladders:
//
//   1F   [way in .. down]          [down 2 .. way out]
//   B1F  [up .. down]              [down 2 .. up 2]     (Victory Road, big regions)
//   B2F  [up ........................... up 2]
//
// Below the deepest floor of the way lies at most one more floor, off the
// way, with the guardian at the bottom (else the guardian keeps a chamber of
// its own beside the way).

// Victory Road goes down to B2F in regions with this many places or more.
#define VICTORY_DEEP_PLACES 40

// The deepest floor the way through a dungeon reaches: 0 if it is not the
// way on.
static u8 CaveThroughDepth(const struct RogueDungeon *d)
{
    if (!d->required)
        return 0;
    return d->kind == RDUN_VICTORY ? d->nFloors : 1;
}

// Boulders and cracked rocks may stand across the way only where the
// player is sure to hold Strength and Rock Smash by the time it opens: on
// Victory Road, behind the League's gate.
static bool8 CaveGatesAllowed(const struct RogueRegion *r, const struct RogueDungeon *d)
{
    u8 need = ROGUE_KEY(ROBST_STRENGTH) | ROGUE_KEY(ROBST_ROCK_SMASH);
    u8 won = d->badges >= ROGUE_NUM_GYMS ? 0xFF : (u8)((1 << d->badges) - 1);
    return d->kind == RDUN_VICTORY && CaveThroughDepth(d) > 0 && (RogueGen_KeysWon(r, won) & need) == need;
}

static void CaveRoleOf(struct Gen *gen, struct CaveRole *role)
{
    const struct RogueRegion *r = gen->r;
    const struct RogueDungeon *d;
    u8 depth, f = sGenFloor;

    memset(role, 0, sizeof(*role));
    role->dungeon = -1;
    role->far = 0xFF;
    role->aside = 0xFF;
    if (f == 0)
    {
        // The cave itself: through to the place beyond, or a dead end.
        bool8 through = gen->p->caveOut != 0xFF;
        s32 di = RogueGen_DungeonAt(r, gen->m->place);
        role->far = through ? RCAVE_NORTH : 0xFF;
        role->deadEnd = !through;
        if (di < 0 || r->dungeons[di].host != gen->m->place)
            return;
        d = &r->dungeons[di];
        role->dungeon = di;
        if (CaveThroughDepth(d) > 0 && through)
        {
            role->split = TRUE;
            if (BossIsTrainer(d))
                role->boss = CBOSS_DOOR;
        }
        else if (through)
        {
            role->aside = RCAVE_DOWN;
        }
        else
        {
            role->far = RCAVE_DOWN;
        }
        return;
    }
    d = &r->dungeons[sGenDungeon];
    role->dungeon = sGenDungeon;
    depth = CaveThroughDepth(d);
    if (f < depth)
    {
        role->split = TRUE;
        role->far = RFLOOR_UP_2;
    }
    else if (f == depth)
    {
        role->far = RFLOOR_UP_2;
        if (d->nFloors > depth)
            role->aside = RFLOOR_DOWN;
        else if (d->boss != RBOSS_NONE && !BossIsTrainer(d))
            role->boss = CBOSS_ASIDE;
    }
    else if (f < d->nFloors)
    {
        role->far = RFLOOR_DOWN;
    }
    else
    {
        role->deadEnd = TRUE;
        // A trainer boss of a dungeon on the way waits by the way out.
        if (d->boss != RBOSS_NONE && !(BossIsTrainer(d) && depth > 0))
            role->boss = CBOSS_FAR;
    }
    role->obstacles = CaveGatesAllowed(r, d);
    role->tunnel = r->tunnel == sGenDungeon && f == RogueGen_ExitFloor(r, sGenDungeon);
}

// Boulders and cracked rocks across the way, as on Emerald's Victory Road:
// up to `pairs` pairs (boulders first, then rocks) across tunnels one unit
// wide that the way from (sx, sy) to (ex, ey) (blocks) must pass. Side by
// side they fill the tunnel; one boulder pushed twice, or one rock
// smashed, opens it from either side. The units along the tunnel on both
// sides stay clear for the push.
// Returns how many pairs it placed.
static int PlaceCaveGates(struct Gen *gen, int sx, int sy, int ex, int ey, int pairs)
{
    struct RogueMap *m = gen->m;
    u16 cand[32];
    int n = 0, x, y, i, have = 0, k, placed = 0;

    for (i = 0; i < m->nObjs; i++)
        if (m->objs[i].kind == ROBJ_BOULDER || m->objs[i].kind == ROBJ_ROCK)
            have++;
    sx >>= 1;
    sy >>= 1;
    ex >>= 1;
    ey >>= 1;
    for (y = 2; y < gen->ch - 2; y++)
    {
        YIELD();
        for (x = 2; x < gen->cw - 2 && n < 32; x++)
        {
            bool8 vert = Cell(gen, x, y - 1) == C_ROAD && Cell(gen, x, y + 1) == C_ROAD
                      && Cell(gen, x - 1, y) == C_TREE && Cell(gen, x + 1, y) == C_TREE;
            bool8 horz = Cell(gen, x - 1, y) == C_ROAD && Cell(gen, x + 1, y) == C_ROAD
                      && Cell(gen, x, y - 1) == C_TREE && Cell(gen, x, y + 1) == C_TREE;
            if (Cell(gen, x, y) != C_ROAD || vert == horz)
                continue;
            if (Abs(x - sx) + Abs(y - sy) <= 2 || Abs(x - ex) + Abs(y - ey) <= 2)
                continue;
            if (CellTaken(gen, x, y)
             || (vert ? (CellTaken(gen, x, y - 1) || CellTaken(gen, x, y + 1))
                      : (CellTaken(gen, x - 1, y) || CellTaken(gen, x + 1, y))))
                continue;
            // Only where the way must pass.
            SetCell(gen, x, y, C_TREE);
            FloodReachable(gen, sx, sy);
            SetCell(gen, x, y, C_ROAD);
            if (!Reached(gen, ex, ey))
                cand[n++] = PACK(x, y);
        }
    }
    for (k = 0; k < pairs && n > 0 && have < 4 && m->nObjs + 2 <= ROGUE_MAX_OBJS; k++)
    {
        int c = RAND(n), j;
        u8 kind = have == 0 ? ROBJ_BOULDER : ROBJ_ROCK;
        x = PX(cand[c]);
        y = PY(cand[c]);
        if (Cell(gen, x, y - 1) == C_ROAD)
        {
            int by = y * 2 + RAND(2);
            AddObj(gen, kind, x * 2, by, RDIR_SOUTH, have, 0);
            AddObj(gen, kind, x * 2 + 1, by, RDIR_SOUTH, have + 1, 0);
        }
        else
        {
            int bx = x * 2 + RAND(2);
            AddObj(gen, kind, bx, y * 2, RDIR_SOUTH, have, 0);
            AddObj(gen, kind, bx, y * 2 + 1, RDIR_SOUTH, have + 1, 0);
        }
        have += 2;
        placed++;
        // The next pair stands well away.
        for (j = 0; j < n; )
        {
            if (Abs(PX(cand[j]) - x) + Abs(PY(cand[j]) - y) <= 4)
                cand[j] = cand[--n];
            else
                j++;
        }
    }
    return placed;
}

// Which caves have floors below, and what kind of dungeon each is. The cave
// on the main road is the way on: down a ladder, across B1F and up into the
// far part of the cave, with now and then a floor below B1F for the guardian
// and the best finds. Victory Road goes down through B1F (and B2F in bigger
// regions) and back up, with a master trainer by the way out. Side caves are
// dead ends with one or two floors and the guardian at the bottom.
static void PlanCaveDungeons(struct RogueRegion *r, u32 *rng)
{
    int i;
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        if (p->kind != RPLACE_CAVE)
            continue;
        if (p->caveOut == r->summit)
        {
            if (AddDungeon(r, RDUN_VICTORY, i, r->nPlaces >= VICTORY_DEEP_PLACES ? 2 : 1, RBOSS_MASTER, TRUE))
                r->dungeons[r->nDungeons - 1].level = p->level; // under the last leader's ace, as Emerald's
        }
        else if (p->side || p->caveOut == 0xFF)
        {
            AddDungeon(r, RDUN_CAVE, i, 1 + RogueRng_Range(rng, 2), RBOSS_GUARDIAN, FALSE);
        }
        else
        {
            if (AddDungeon(r, RDUN_CAVE, i, 1 + (RogueRng_Range(rng, 100) < 45), RBOSS_GUARDIAN, TRUE))
                r->dungeons[r->nDungeons - 1].level = p->level + 1;
        }
    }
}

// Ledges, as on Victory Road and in Granite Cave: a row of the cave's ledge
// across a chamber from wall to wall, jumped down to the south. Only where
// the chamber's two halves are joined another way too, so a ledge is a
// shortcut and never the only way. The pieces and their neighbours are
// Victory Road's: the left end against a wall facing east, the right end
// against one facing west, the rough floor above and the floor below.
#define B_CAVE_LEDGE_L 0x064C
#define B_CAVE_LEDGE   0x0651
#define B_CAVE_LEDGE_R 0x064D

// Floor units reached from (sx, sy) and, on a split map, from (tx, ty).
static int CaveReach(struct Gen *gen, int sx, int sy, int tx, int ty, bool8 split)
{
    int n = FloodReachable(gen, sx, sy);
    return split ? n + FloodReachable(gen, tx, ty) : n;
}

static bool8 CaveLedge(struct Gen *gen, int sx, int sy, int tx, int ty, bool8 split)
{
    u16 cand[24];
    u8 width[24];
    int n = 0, cy, cx, k;
    // Runs of floor units two to six wide between rock, along a unit row.
    for (cy = 2; cy < gen->ch - 2 && n < 24; cy++)
    {
        YIELD();
        for (cx = 1; cx < gen->cw - 1 && n < 24; cx++)
        {
            int end = cx, x, y = cy * 2 + 1, x0 = cx * 2, x1;
            bool8 ok = TRUE;
            if (Cell(gen, cx, cy) != C_ROAD || Cell(gen, cx - 1, cy) != C_TREE)
                continue;
            while (end < gen->cw - 1 && Cell(gen, end + 1, cy) == C_ROAD)
                end++;
            x1 = end * 2 + 1;
            if (end - cx + 1 < 2 || end - cx + 1 > 6 || Cell(gen, end + 1, cy) != C_TREE)
                continue;
            // Straight walls at both ends; plain floor across, above and below.
            for (x = x0 - 1; x <= x1 + 1 && ok; x++)
            {
                int dy;
                for (dy = -1; dy <= 1 && ok; dy++)
                {
                    u16 b = gen->m->blocks[(y + dy) * gen->m->w + x];
                    if (x == x0 - 1)
                        ok = b == B_CAVE_WALL_E;
                    else if (x == x1 + 1)
                        ok = b == B_CAVE_WALL_W;
                    else if (dy < 0)
                        ok = b == B_CAVE_FLOOR2 && !IsOccupied(gen, x, y + dy);
                    else
                        ok = (b == B_CAVE_FLOOR2 || b == B_CAVE_FLOOR) && !IsOccupied(gen, x, y + dy);
                }
            }
            if (ok)
            {
                width[n] = end - cx + 1;
                cand[n++] = PACK(cx, cy);
            }
        }
    }
    while (n > 0)
    {
        int c = RAND(n), w, x, before, after, x0, x1, y;
        cx = PX(cand[c]);
        cy = PY(cand[c]);
        w = width[c];
        n--;
        cand[c] = cand[n];
        width[c] = width[n];
        // Everything stays joined without the row: a ledge is only ever a
        // shortcut.
        before = CaveReach(gen, sx, sy, tx, ty, split);
        for (k = 0; k < w; k++)
            SetCell(gen, cx + k, cy, C_TREE);
        after = CaveReach(gen, sx, sy, tx, ty, split);
        for (k = 0; k < w; k++)
            SetCell(gen, cx + k, cy, C_ROAD);
        if (after != before - w)
            continue;
        y = cy * 2 + 1;
        x0 = cx * 2;
        x1 = (cx + w) * 2 - 1;
        for (x = x0; x <= x1; x++)
        {
            Put(gen, x, y, x == x0 ? B_CAVE_LEDGE_L : x == x1 ? B_CAVE_LEDGE_R : B_CAVE_LEDGE);
            MarkOccupied(gen, x, y - 1);
            MarkOccupied(gen, x, y);
            MarkOccupied(gen, x, y + 1);
        }
        return TRUE;
    }
    return FALSE;
}
