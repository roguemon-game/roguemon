// Water routes (v1.1): rivers that wind across the map with wooden bridges
// where the way crosses, as on Routes 119 and 120; pond country, as Route
// 120's middle, with a pond the way crosses on a bridge, puddles through
// the grass and now and then a patch of marsh; and wide beaches, as Route
// 104's south and Route 109.
//
// Part of src/rogue_gen.c (included near its end, prototypes at the top).
// Every piece is a vanilla metatile. The log bridges and the marsh are in
// Fortree's tileset only, so river and lake routes are drawn with it
// (WaterPlanRoutes), and these pieces keep out of view of a seam toward
// another tileset (InRouteSeam), as a town keeps its own pieces.

// Fortree's log bridge over sea water (Route 119) and over pond water (Route
// 120): land end, first plank, middle plank (repeated), last plank, land end;
// two rows. As in vanilla the land ends stand at elevation 4 and the planks
// at 15 (a walker keeps the 4 over them, a surfer passes under), and the
// ground just outside each land end is grass at elevation 0, the step from
// the ground's 3 up to the 4. At 3 the walker would be drawn under the
// planks' upper layer.
static const u16 sWaterBridgeSea[2][5] =
{
    { 0x4211, 0xF216, 0xF210, 0xF217, 0x4215 },
    { 0x4219, 0xF21E, 0xF218, 0xF21F, 0x421D },
};
static const u16 sWaterBridgePond[2][5] =
{
    { 0x42FB, 0xF285, 0xF287, 0xF286, 0x42FC },
    { 0x4303, 0xF28D, 0xF28F, 0xF28E, 0x4304 },
};
#define B_WATER_STEP 0x0001 // grass at elevation 0, outside a land end
// The water row just south of a bridge, in its shadow: the banks, then dark
// water between.
static const u16 sWaterShadowSea[3] = { 0x1266, 0x1171, 0x1267 };
static const u16 sWaterShadowPond[3] = { 0x126E, 0x1277, 0x126F };

// Puddles in the grass (General, MB_PUDDLE: walked through with a splash),
// as Route 120 has them: TL, T, TR, L, C, R, BL, B, BR.
static const u16 sWaterPuddle[9] = { 0x30C8, 0x30C9, 0x30CA, 0x30D0, 0x30D1, 0x30D2, 0x30D8, 0x30D9, 0x30DA };
// Fortree's marsh, Route 120's wet ground (MB_PUDDLE), the same 3x3 set, and
// the small pools in it, 2x2.
static const u16 sWaterMarsh[9] = { 0x3288, 0x3289, 0x328A, 0x3290, 0x3291, 0x3292, 0x3298, 0x3299, 0x329A };
static const u16 sWaterMarshPool[4] = { 0x3274, 0x3275, 0x327C, 0x327D };

#if defined(ROGUE_HOST) && defined(WATER_DEBUG)
#define WDBG(...) (getenv("WATER_DEBUG") ? fprintf(stderr, __VA_ARGS__) : 0)
#else
#define WDBG(...) ((void)0)
#endif

// ---------------------------------------------------------------------------
// The plan

// A town of the route's tileset that already keeps its own tiles out of
// view of two or more neighbours with other tilesets (InSeamView): a third
// would leave it little room for its buildings, so the route keeps the
// town's tileset.
static bool8 WaterTownCramped(const struct RogueRegion *r, const struct RoguePlace *p)
{
    int k, j;
    for (k = 0; k < p->nLinks; k++)
    {
        const struct RoguePlace *t = &r->places[p->links[k].target];
        int other = 0;
        if (t->kind != RPLACE_TOWN || t->theme != p->theme)
            continue;
        for (j = 0; j < t->nLinks; j++)
            if (r->places[t->links[j].target].theme != t->theme)
                other++;
        // (A gym city with three ways out has the most to fit: seed 20's
        // BRIGHTVALE CITY lost its berry farm.)
        if (other >= 2 || (other >= 1 && t->nLinks >= 3 && t->type == RTOWN_GYM))
            return TRUE;
    }
    return FALSE;
}

// River and lake routes are drawn with Fortree's tileset, for the bridges
// and the marsh; but beside an ash town they stay in the ash (Fallarbor's),
// with a ford across. Coasts more often have a beach cabin (the rest house)
// standing on the sand. Called once the route kinds are known; draws no
// numbers from the plan's stream.
static void WaterPlanRoutes(struct RogueRegion *r)
{
    int i;
    for (i = 0; i < r->nPlaces; i++)
    {
        struct RoguePlace *p = &r->places[i];
        if (p->kind != RPLACE_ROUTE)
            continue;
        if ((p->type == RROUTE_RIVER || p->type == RROUTE_LAKE) && p->theme != RTHEME_ASH && !WaterTownCramped(r, p))
            p->theme = RTHEME_TREETOP;
        if (p->type == RROUTE_COAST && p->tier >= 1 && !p->side && !(p->obstacle && !ObstacleOutside(r, p))
         && RogueRng_Hash(r->seed, 42900 + i) % 100 < 50)
            p->interiors |= 1 << RINT_REST;
    }
}

// ---------------------------------------------------------------------------
// Helpers

static bool8 WaterBridgeU(struct Gen *gen, int ux, int uy)
{
    return gen->p->kind == RPLACE_ROUTE && Cell(gen, ux, uy) == C_FORD && BitU(gen->bridgeU, gen, ux, uy);
}

static bool8 WaterBridgeF(struct Gen *gen, int fx, int fy)
{
    return InMap(gen, fx, fy) && WaterBridgeU(gen, fx >> 1, fy >> 1);
}

// TRUE on a bridge's step (B_WATER_STEP): the outer block of the unit at
// either end, as WaterRenderBridges draws it.
static bool8 WaterStepF(struct Gen *gen, int fx, int fy)
{
    int ux = fx >> 1, uy = fy >> 1;
    if (gen->p->kind != RPLACE_ROUTE || !InMap(gen, fx, fy) || WaterBridgeU(gen, ux, uy))
        return FALSE;
    return (fx & 1) ? WaterBridgeU(gen, ux - 1, uy) : WaterBridgeU(gen, ux + 1, uy);
}

// TRUE on a step or the ground beside one, where it is walked onto from.
// The step is the only way from the ground's 3 up to the land end's 4, so
// whoever stands or stops there can close that side's way over.
static bool8 WaterNearStepF(struct Gen *gen, int fx, int fy)
{
    int k;
    if (!gen->logBridge)
        return FALSE;
    if (WaterStepF(gen, fx, fy))
        return TRUE;
    for (k = 0; k < 4; k++)
        if (WaterStepF(gen, fx + sDirDX[k], fy + sDirDY[k]))
            return TRUE;
    return FALSE;
}

// A block of a bridge or of the unit at either end of it, or beside a step,
// where nobody may stand (ByLedgeF): they would block the only way over, or
// be drawn under the planks.
static bool8 WaterNoStandF(struct Gen *gen, int fx, int fy)
{
    int ux = fx >> 1, uy = fy >> 1;
    if (gen->p->kind != RPLACE_ROUTE || !InMap(gen, fx, fy))
        return FALSE;
    return WaterBridgeU(gen, ux, uy) || WaterBridgeU(gen, ux - 1, uy) || WaterBridgeU(gen, ux + 1, uy)
        || WaterNearStepF(gen, fx, fy);
}

// TRUE if a trainer at (x, y) looking the ways in mask, n blocks, could
// walk up to the player and stop on or beside a step. Their line ends at a
// wall, water or a bridge, as WaterObjsOffBridges reads it on the drawn map.
static bool8 WaterLineNearStep(struct Gen *gen, int x, int y, u8 mask, int n)
{
    int k, i;
    if (!gen->logBridge || gen->p->kind != RPLACE_ROUTE)
        return FALSE;
    for (k = 0; k < 4; k++)
    {
        if (!(mask & (1 << k)))
            continue;
        for (i = 1; i < n; i++)
        {
            int fx = x + sDirDX[k] * i, fy = y + sDirDY[k] * i;
            u16 b;
            if (!InMap(gen, fx, fy))
                break;
            if (WaterNearStepF(gen, fx, fy))
                return TRUE;
            b = gen->m->blocks[fy * gen->m->w + fx];
            if (((b >> 10) & 3) || (b >> 12) == 4 || (b >> 12) == 15)
                break;
        }
    }
    return FALSE;
}

// A river or lake route with Fortree's tiles only for its bridges (no
// Fortree town beside it): no long grass, which it would not have had before.
static bool8 WaterNoLongGrass(struct Gen *gen)
{
    const struct RoguePlace *p = gen->p;
    int k;
    if (p->type != RROUTE_RIVER && p->type != RROUTE_LAKE)
        return FALSE;
    for (k = 0; k < p->nLinks; k++)
    {
        const struct RoguePlace *t = &gen->r->places[p->links[k].target];
        if (t->kind == RPLACE_TOWN && t->theme == RTHEME_TREETOP)
            return FALSE;
    }
    return TRUE;
}

// Fortree's tileset, with its bridges.
static bool8 WaterBridgeTiles(struct Gen *gen)
{
    return gen->p->kind == RPLACE_ROUTE && gen->p->theme == RTHEME_TREETOP;
}

// A lake route's bridged pond is pond water, the lighter blue of Route
// 120's (its units are marked in bridgeU too). Other ponds keep sea water:
// the pond set has no ends for a side bank, so it only suits square ponds.
static bool8 WaterPondTiles(struct Gen *gen)
{
    return gen->p->kind == RPLACE_ROUTE && gen->p->type == RROUTE_LAKE && sGenFloor == 0;
}

static bool8 WaterPondF(struct Gen *gen, int fx, int fy)
{
    return WaterPondTiles(gen) && InMap(gen, fx, fy) && BitU(gen->bridgeU, gen, fx >> 1, fy >> 1);
}

// A lake route laid out as pond country: not one whose water is the
// obstacle (crossed by Surf on purpose), which keeps its big lake.
static bool8 WaterPondCountry(struct Gen *gen)
{
    return gen->p->kind == RPLACE_ROUTE && gen->p->type == RROUTE_LAKE && gen->p->obstacle != ROBST_SURF
        && sGenFloor == 0;
}

// A block of pond water, from the pond set (Route 104's and 120's ponds):
// the top and side banks are drawn in the water, the bottom bank by the
// land below it.
static u16 WaterPondBlock(struct Gen *gen, int fx, int fy)
{
    bool8 n = !IsSeaF(gen, fx, fy - 1), e = !IsSeaF(gen, fx + 1, fy), w = !IsSeaF(gen, fx - 1, fy);
    bool8 ne = !IsSeaF(gen, fx + 1, fy - 1), nw = !IsSeaF(gen, fx - 1, fy - 1);
    if (n && w) return 0x10B0;
    if (n && e) return 0x10B2;
    if (n)      return 0x10B1;
    if (w)      return 0x10B8;
    if (e)      return 0x10BA;
    if (nw)     return 0x10C0;
    if (ne)     return 0x10C2;
    return 0x10A1;
}

// After how many tries at a map the water steps back for the old layout:
// sooner where a feature's lot needs room beside the road (the Day Care's
// and the BERRY MASTER's are big) or a double battle pair stands.
static int WaterGiveUp(struct Gen *gen)
{
    switch (RogueGen_RouteFeature(gen->r, gen->m->place))
    {
    case RFEAT_DAYCARE:
    case RFEAT_BERRY:
        return 8;
    case RFEAT_NONE:
    case RFEAT_SNORLAX:
    case RFEAT_GAUNTLET:
        // (A double battle pair needs two spots side by side by the road.)
        return RogueGen_RoutePair(gen->r, gen->m->place) >= 0 ? 16 : 26;
    default:
        return 16;
    }
}

// Every block of a bridge row, with its two land ends, out of view of a
// seam toward another tileset.
static bool8 WaterBridgeClearOfSeams(struct Gen *gen, int x0, int x1, int uy)
{
    int fx;
    for (fx = x0 * 2 - 1; fx <= x1 * 2 + 2; fx++)
        if (InRouteSeam(gen, fx, uy * 2) || InRouteSeam(gen, fx, uy * 2 + 2))
            return FALSE;
    return TRUE;
}

// The water units of row uy that touch unit (x, uy) in one run: x0 to x1.
static void WaterRun(struct Gen *gen, int x, int uy, int *x0, int *x1)
{
    *x0 = *x1 = x;
    while (Cell(gen, *x0 - 1, uy) == C_SEA && *x0 - 1 >= 0)
        (*x0)--;
    while (Cell(gen, *x1 + 1, uy) == C_SEA && *x1 + 1 < gen->cw)
        (*x1)++;
}

// Can a bridge cross row uy from x0 to x1 (all water)? The banks run
// straight past it a unit above and below, the land at both ends is
// still forest (the trail will reach it) and away from the openings.
static bool8 WaterBridgeFits(struct Gen *gen, int x0, int x1, int uy)
{
    int x, a0, a1;
    if (x0 < 2 || x1 > gen->cw - 3 || uy < 2 || uy > gen->ch - 3 || x1 - x0 < 1 || x1 - x0 > 7)
        return FALSE;
    for (x = x0; x <= x1; x++)
        if (Cell(gen, x, uy - 1) != C_SEA || Cell(gen, x, uy + 1) != C_SEA)
            return FALSE;
    WaterRun(gen, x0, uy - 1, &a0, &a1);
    if (a0 != x0 || a1 != x1)
        return FALSE;
    WaterRun(gen, x0, uy + 1, &a0, &a1);
    if (a0 != x0 || a1 != x1)
        return FALSE;
    if (Cell(gen, x0 - 1, uy) != C_TREE || Cell(gen, x1 + 1, uy) != C_TREE
     || IsEdge(gen, x0 - 1, uy) || IsEdge(gen, x1 + 1, uy)
     || NearExitU(gen, x0 - 1, uy, 2) || NearExitU(gen, x1 + 1, uy, 2))
        return FALSE;
    return WaterBridgeClearOfSeams(gen, x0, x1, uy);
}

static void WaterLayBridge(struct Gen *gen, int x0, int x1, int uy)
{
    int x;
    for (x = x0; x <= x1; x++)
    {
        SetCell(gen, x, uy, C_FORD);
        SetBitU(gen->bridgeU, gen, x, uy);
        gen->logBridge = TRUE;
        // (Kept as river, so the road over it settles back into a ford.)
        SetBitU(gen->river, gen, x, uy);
    }
}

// How far unit (x, y) lies from the straight way between (x0, y0) and
// (x1, y1), in units.
static int WaterOffLine(int x, int y, int x0, int y0, int x1, int y1)
{
    int k, best = 999;
    for (k = 0; k <= 16; k++)
    {
        int px = x0 + (x1 - x0) * k / 16, py = y0 + (y1 - y0) * k / 16;
        int d = Abs(x - px) + Abs(y - py);
        if (d < best)
            best = d;
    }
    return best;
}

// ---------------------------------------------------------------------------
// Rivers

// One reach of a river, from (x, y0) down to row y1 (vert) or from (y, x0)
// across to column x1: two units wide, stepping a unit aside now and then so
// its banks wander, but never within two units of row keep (or column), where
// the bridge may cross. Returns the column (or row) it ends at, or -1.
static int WaterReach(struct Gen *gen, bool8 vert, int at, int from, int to, int lo, int hi, int keep)
{
    int pos = from, dir = to > from ? 1 : -1, run = 2 + RAND(3);
    while (pos != to)
    {
        int next = pos + dir, side = at;
        if (--run <= 0 && Abs(next - keep) > 2 && Abs(pos - keep) > 2 && Abs(to - next) > 1)
        {
            side = at + (RAND(2) ? 1 : -1);
            if (side < lo || side > hi)
                side = at - (side - at);
            run = 3 + RAND(3);
        }
        if (vert ? !RiverLine(gen, at, pos, side, next) : !RiverLine(gen, pos, at, next, side))
            return -1;
        at = side;
        pos = next;
    }
    return at;
}

// A pool where a river starts or ends inside the map, so it never stops
// square like a ditch: a rounded blob round (x, y), on forest only.
static void WaterRiverPool(struct Gen *gen, int x, int y)
{
    int rx = 2 + RAND(2), ry = 2, dx, dy;
    for (; rx >= 1; rx--, ry = rx < 2 ? rx : 2)
    {
        bool8 fits = TRUE;
        for (dy = -ry; dy <= ry && fits; dy++)
            for (dx = -rx; dx <= rx + 1 && fits; dx++)
            {
                int ux = x + dx, uy = y + dy;
                if (!InEllipse(dx * 2 - 1, dy * 2, rx * 2 + 1, ry * 2) || Cell(gen, ux, uy) == C_SEA)
                    continue;
                fits = InBounds(gen, ux, uy) && !IsEdge(gen, ux, uy) && Cell(gen, ux, uy) == C_TREE
                    && !NearExitU(gen, ux, uy, 2) && !AshZoneU(gen, ux, uy);
            }
        if (!fits)
            continue;
        for (dy = -ry; dy <= ry; dy++)
            for (dx = -rx; dx <= rx + 1; dx++)
                if (InEllipse(dx * 2 - 1, dy * 2, rx * 2 + 1, ry * 2))
                {
                    SetCell(gen, x + dx, y + dy, C_SEA);
                    SetBitU(gen->river, gen, x + dx, y + dy);
                }
        return;
    }
}

// A river that winds across the map, as Route 119's does: reaches joined by
// bends, two units wide with banks that wander, from a side with no way out
// to the side across (or from a pool well inside the map where that side
// has a way out). It runs across the way between the openings, so the trail
// crosses it on a log bridge where a reach runs north and south (Fortree's
// bridges are all east to west): across a north-south reach when the way
// runs east and west, across a long bend when it runs north and south.
// Beside an ash town there is no bridge: the trail fords it. FALSE leaves
// the map as it was, for the old river.
static bool8 WaterWindingRiver(struct Gen *gen, int top, u8 inDir, u8 outDir)
{
    const struct RoguePlace *p = gen->p;
    int wx0, wy0, wx1, wy1, mx, my, tries;
    bool8 vertFirst, bridge = WaterBridgeTiles(gen);

    if (gen->attempt >= WaterGiveUp(gen))
        return FALSE;
    WDBG("river place %d attempt %d in %d out %d top %d\n", gen->m->place, gen->attempt, inDir, outDir, top);
    OpeningUnit(gen, inDir, top, &wx0, &wy0);
    OpeningUnit(gen, outDir, top, &wx1, &wy1);
    if (outDir == RDIR_NONE)
    {
        if (p->mouth != 0xFF)
        {
            wx1 = gen->cw / 2;
            wy1 = top + 1;
        }
        else
        {
            wx1 = gen->cw - 1 - wx0;
            wy1 = gen->ch - 1 - wy0;
        }
    }
    mx = (wx0 + wx1) / 2;
    my = (wy0 + wy1) / 2;
    // Across the way: north to south when the way runs east and west.
    vertFirst = Abs(wx1 - wx0) >= Abs(wy1 - wy0);
    for (tries = 0; tries < 16; tries++)
    {
        int k, n, ok = TRUE, bestScore = 999, bx0 = 0, bx1 = 0, by = 0, x, y;
        bool8 vert = (tries & 1) ? !vertFirst : vertFirst;
        YIELD();
        if (vert)
        {
            // Down the map: the reach the way crosses, with a bend above it
            // and below it now and then.
            bool8 openN = !HasSide(p, RDIR_NORTH) && p->mouth == 0xFF && top <= 2, openS = !HasSide(p, RDIR_SOUTH);
            int ys = openN ? 0 : top + 3, ye = openS ? gen->ch - 2 : gen->ch - 5;
            int xc = mx - 1 + (int)RAND(3) - 1, ya, yb, xa, xb;
            if (xc < 5) xc = 5;
            if (xc > gen->cw - 7) xc = gen->cw - 7;
            if (my - 2 < ys || my + 2 > ye)
                continue;
            ya = ys + RAND((my - 2 - ys) + 1);
            yb = my + 2 + RAND((ye - my - 2) + 1);
            if (ya - ys < 3)
                ya = ys;
            if (ye - yb < 3)
                yb = ye;
            // Above: from the north side to the bend, at its own column.
            xa = xc;
            if (ya > ys)
            {
                xa = xc + (RAND(2) ? 1 : -1) * (int)(2 + RAND(4));
                if (xa < 3 || xa > gen->cw - 5)
                    xa = xc - (xa - xc);
                if (xa < 3) xa = 3;
                if (xa > gen->cw - 5) xa = gen->cw - 5;
                ok = (x = WaterReach(gen, TRUE, xa, ys, ya, 3, gen->cw - 5, -99)) >= 0
                  && RiverLine(gen, x, ya, xc, ya);
            }
            ok = ok && (x = WaterReach(gen, TRUE, xc, ya, yb, 3, gen->cw - 5, my)) >= 0;
            xb = x;
            if (ok && yb < ye)
            {
                int xd = x + (RAND(2) ? 1 : -1) * (int)(2 + RAND(4));
                if (xd < 3 || xd > gen->cw - 5)
                    xd = x - (xd - x);
                if (xd < 3) xd = 3;
                if (xd > gen->cw - 5) xd = gen->cw - 5;
                ok = RiverLine(gen, x, yb, xd, yb) && (xb = WaterReach(gen, TRUE, xd, yb, ye, 3, gen->cw - 5, -99)) >= 0;
            }
            if (ok && !openN)
                WaterRiverPool(gen, xa, ys);
            if (ok && !openS)
                WaterRiverPool(gen, xb, ye);
        }
        else
        {
            // Across the map from west to east, with one long bend north or
            // south where the way crosses it.
            bool8 openW = !HasSide(p, RDIR_WEST), openE = !HasSide(p, RDIR_EAST);
            int xs = openW ? 0 : 3, xe = openE ? gen->cw - 2 : gen->cw - 5;
            int xj = mx - 1 + (int)RAND(3) - 1, ya, yb, span = 5 + RAND(3), lo = top + 1, hi = gen->ch - 3;
            if (xj < 5) xj = 5;
            if (xj > gen->cw - 7) xj = gen->cw - 7;
            if (xj - xs < 2 || xe - xj < 2)
                continue;
            ya = my - span / 2 + (int)RAND(3) - 1;
            if (ya < lo) ya = lo;
            if (ya + span > hi) ya = hi - span;
            if (ya < lo)
                continue;
            yb = ya + span;
            if (RAND(2))
            {
                int t = ya;
                ya = yb;
                yb = t;
            }
            ok = (y = WaterReach(gen, FALSE, ya, xj, xs, lo, hi, -99)) >= 0;
            if (ok && !openW)
                WaterRiverPool(gen, xs, y);
            ok = ok && RiverLine(gen, xj, ya, xj, yb) && (y = WaterReach(gen, FALSE, yb, xj, xe, lo, hi, -99)) >= 0;
            if (ok && !openE)
                WaterRiverPool(gen, xe, y);
        }
        for (n = 0, k = 0; k < gen->cw * gen->ch; k++)
            n += gen->g[k] == C_SEA;
        WDBG("try %d vert %d ok %d n %d\n", tries, vert, ok, n);
        if (!ok || n < 14)
        {
            ClearRiver(gen);
            continue;
        }
        if (!bridge)
        {
            // A ford where the way crosses, as before.
            if (RiverGap(gen, wx0, wy0, wx1, wy1) <= 1)
                return TRUE;
            ClearRiver(gen);
            continue;
        }
        // The bridge: on a straight reach running north and south, as near
        // the middle of the way between the openings as it can be.
        for (y = 2; y < gen->ch - 2; y++)
        {
            for (x = 2; x < gen->cw - 2; x++)
            {
                int a0, a1, s;
                if (Cell(gen, x, y) != C_SEA || Cell(gen, x - 1, y) == C_SEA)
                    continue;
                WaterRun(gen, x, y, &a0, &a1);
                if (a1 - a0 != 1 || !WaterBridgeFits(gen, a0, a1, y))
                    continue;
                s = Abs(a0 + 1 - mx) / 2 + Abs(y - my) + WaterOffLine(a0, y, wx0, wy0, wx1, wy1);
                if (s < bestScore)
                {
                    bestScore = s;
                    bx0 = a0;
                    bx1 = a1;
                    by = y;
                }
            }
        }
        WDBG("score %d\n", bestScore);
        if (bestScore > 6)
        {
            ClearRiver(gen);
            continue;
        }
        WaterLayBridge(gen, bx0, bx1, by);
        // Only the bridge crosses: the rest of the river may not be waded.
        for (y = 0; y < gen->ch; y++)
            for (x = 0; x < gen->cw; x++)
                if (!WaterBridgeU(gen, x, y) && BitU(gen->river, gen, x, y))
                {
                    int i = y * gen->cw + x;
                    gen->river[i >> 3] &= ~(1 << (i & 7));
                }
        return TRUE;
    }
    return FALSE;
}

// A verge of grass along a river's banks, as Route 119 has, rather than
// trees standing in the water's edge: forest beside the water to the west,
// east and north opens (the bank to the south is drawn by ShoreUnderSea).
// Not trees kept for a squeeze or a lane's walls, nor the map's edge.
static void WaterRiverVerge(struct Gen *gen)
{
    int x, y;
    u8 d;
    if (gen->p->type != RROUTE_RIVER)
        return;
    for (y = 0; y < gen->ch; y++)
        for (x = 0; x < gen->cw; x++)
        {
            if (Cell(gen, x, y) != C_SEA)
                continue;
            for (d = 0; d < 4; d++)
            {
                int nx = x + sDirDX[d], ny = y + sDirDY[d];
                if (d == RDIR_SOUTH || Cell(gen, nx, ny) != C_TREE || IsEdge(gen, nx, ny)
                 || BitU(gen->keep, gen, nx, ny) || InGateLaneF(gen, nx * 2, ny * 2) || NearExitU(gen, nx, ny, 1))
                    continue;
                SetCell(gen, nx, ny, C_OPEN);
            }
        }
}

// The bridge's two land ends, west and east; FALSE if the map has none.
static bool8 WaterBridgeEnds(struct Gen *gen, int *lx, int *ly, int *rx, int *ry)
{
    int x, y;
    for (y = 0; y < gen->ch; y++)
        for (x = 0; x < gen->cw; x++)
            if (WaterBridgeU(gen, x, y) && !WaterBridgeU(gen, x - 1, y))
            {
                int e = x;
                while (WaterBridgeU(gen, e + 1, y))
                    e++;
                *lx = x - 1;
                *ly = y;
                *rx = e + 1;
                *ry = y;
                return TRUE;
            }
    return FALSE;
}

// Steps from unit (sx, sy) to each of two units over ground or forest (the
// trail may cut through either), round water and never over a bridge; 999
// where there is no way.
static void WaterSteps(struct Gen *gen, int sx, int sy, int x0, int y0, int x1, int y1, int *d0, int *d1)
{
    s16 *dist = sBfsPrev;
    int head = 0, tail = 0, i, n = gen->cw * gen->ch;
    for (i = 0; i < n; i++)
        dist[i] = -1;
    *d0 = *d1 = 999;
    if (!InBounds(gen, sx, sy))
        return;
    dist[sy * gen->cw + sx] = 0;
    sBfsQueue[tail++] = PACK(sx, sy);
    while (head != tail)
    {
        int c = sBfsQueue[head++ & (BFS_RING - 1)], cx = PX(c), cy = PY(c);
        u8 d;
        YIELD();
        if (cx == x0 && cy == y0)
            *d0 = dist[cy * gen->cw + cx];
        if (cx == x1 && cy == y1)
            *d1 = dist[cy * gen->cw + cx];
        for (d = 0; d < 4; d++)
        {
            int nx = cx + sDirDX[d], ny = cy + sDirDY[d];
            u8 t;
            if (!InBounds(gen, nx, ny) || dist[ny * gen->cw + nx] >= 0 || IsEdge(gen, nx, ny))
                continue;
            t = Cell(gen, nx, ny);
            if (t != C_TREE && t != C_OPEN && t != C_ROAD)
                continue;
            dist[ny * gen->cw + nx] = dist[cy * gen->cw + cx] + 1;
            if (tail - head < BFS_RING)
                sBfsQueue[tail++ & (BFS_RING - 1)] = PACK(nx, ny);
        }
    }
    memset(sBfsPrev, 0, n * sizeof(sBfsPrev[0]));
}

// A bridge the way cannot use is water again.
static void WaterDropBridges(struct Gen *gen)
{
    int x, y;
    for (y = 0; y < gen->ch; y++)
        for (x = 0; x < gen->cw; x++)
            if (WaterBridgeU(gen, x, y))
            {
                int i = y * gen->cw + x;
                SetCell(gen, x, y, C_SEA);
                gen->bridgeU[i >> 3] &= ~(1 << (i & 7));
                gen->river[i >> 3] &= ~(1 << (i & 7));
            }
}

// The trail's waypoints over the bridge: to one end, across, and on halfway
// to the way on. The end it goes to first is nearer the way in than the
// other, and the end it leaves by nearer the way on, so the trail never
// turns back over the bridge (which would cut it out of the trail). A bridge
// that would need that is dropped. FALSE if there is no bridge.
static bool8 WaterBridgeWay(struct Gen *gen, int ax, int ay, int bx, int by, int *mx, int *my)
{
    int lx, ly, rx, ry, aL, aR, bL, bR;
    if (!WaterBridgeEnds(gen, &lx, &ly, &rx, &ry))
        return FALSE;
    WaterSteps(gen, ax, ay, lx, ly, rx, ry, &aL, &aR);
    WaterSteps(gen, bx, by, lx, ly, rx, ry, &bL, &bR);
    if (aL <= aR && bR <= bL && aL < 999 && bR < 999)
    {
        mx[0] = lx; my[0] = ly;
        mx[1] = rx; my[1] = ry;
    }
    else if (aR <= aL && bL <= bR && aR < 999 && bL < 999)
    {
        mx[0] = rx; my[0] = ry;
        mx[1] = lx; my[1] = ly;
    }
    else
    {
        WaterDropBridges(gen);
        return FALSE;
    }
    mx[2] = (mx[1] + bx) / 2;
    my[2] = (my[1] + by) / 2;
    return TRUE;
}

// ---------------------------------------------------------------------------
// Pond country

// A pond across the way with a log bridge over its middle, as Route 120's
// north pond: three units deep (now and then a fourth, narrower row on top),
// three to six across, near the middle of the straight way. TRUE if laid.
static bool8 WaterBridgePond(struct Gen *gen, int top, u8 inDir, u8 outDir)
{
    const struct RoguePlace *p = gen->p;
    int wx0, wy0, wx1, wy1, tries;
    if (!WaterBridgeTiles(gen) || gen->attempt >= WaterGiveUp(gen))
        return FALSE;
    OpeningUnit(gen, inDir, top, &wx0, &wy0);
    OpeningUnit(gen, outDir, top, &wx1, &wy1);
    if (outDir == RDIR_NONE)
    {
        wx1 = gen->cw / 2;
        wy1 = top + (gen->ch - top) / 2;
    }
    // Most where the way runs east and west, half the others (the trail then
    // steps aside over it).
    if (Abs(wx1 - wx0) < Abs(wy1 - wy0) && RogueRng_Hash(gen->r->seed, 42910 + (u32)(p - gen->r->places)) % 2)
        return FALSE;
    for (tries = 0; tries < 30; tries++)
    {
        int w = 3 + RAND(4), t = 35 + RAND(31), x, y, x0, y0, cy;
        bool8 cap = w >= 4 && CHANCE(50), ok = TRUE;
        int cx = wx0 + (wx1 - wx0) * t / 100, cyl = wy0 + (wy1 - wy0) * t / 100;
        YIELD();
        x0 = cx - w / 2 + (int)RAND(3) - 1;
        cy = cyl + (int)RAND(3) - 1;
        y0 = cy - 1 - cap;
        for (y = y0; y <= cy + 1 && ok; y++)
            for (x = x0; x < x0 + w && ok; x++)
            {
                if (cap && y == y0 && (x == x0 || x == x0 + w - 1))
                    continue;
                ok = BlobUnitFits(gen, x, y, top + 1);
            }
        // Ground all round, a unit wide, so the pond never touches other
        // water or the map's edge.
        for (y = y0 - 1; y <= cy + 2 && ok; y++)
            for (x = x0 - 1; x <= x0 + w && ok; x++)
                ok = InBounds(gen, x, y) && !IsEdge(gen, x, y) && Cell(gen, x, y) == C_TREE;
        if (!ok || NearExitU(gen, x0 - 1, cy, 2) || NearExitU(gen, x0 + w, cy, 2)
         || !WaterBridgeClearOfSeams(gen, x0, x0 + w - 1, cy))
            continue;
        for (y = y0; y <= cy + 1; y++)
            for (x = x0; x < x0 + w; x++)
                if (!(cap && y == y0 && (x == x0 || x == x0 + w - 1)))
                {
                    SetCell(gen, x, y, C_SEA);
                    SetBitU(gen->bridgeU, gen, x, y);
                }
        WaterLayBridge(gen, x0, x0 + w - 1, cy);
        return TRUE;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// Drawing

// The bridges, once the map is drawn: planks over the water, the land ends
// on the ground either side, and the shadow on the water below.
static void WaterRenderBridges(struct Gen *gen)
{
    int x, y;
    bool8 pond = WaterPondTiles(gen);
    const u16 (*set)[5] = pond ? sWaterBridgePond : sWaterBridgeSea;
    const u16 *shadow = pond ? sWaterShadowPond : sWaterShadowSea;
    for (y = 0; y < gen->ch; y++)
    {
        for (x = 0; x < gen->cw; x++)
        {
            int e, fx, fx0, fx1, r;
            if (!WaterBridgeU(gen, x, y) || WaterBridgeU(gen, x - 1, y))
                continue;
            for (e = x; WaterBridgeU(gen, e + 1, y); e++)
                ;
            fx0 = x * 2;
            fx1 = e * 2 + 1;
            for (r = 0; r < 2; r++)
            {
                int fy = y * 2 + r;
                Put(gen, fx0 - 2, fy, B_WATER_STEP);
                Put(gen, fx0 - 1, fy, set[r][0]);
                Put(gen, fx1 + 1, fy, set[r][4]);
                Put(gen, fx1 + 2, fy, B_WATER_STEP);
                for (fx = fx0; fx <= fx1; fx++)
                    Put(gen, fx, fy, set[r][fx == fx0 ? 1 : fx == fx1 ? 3 : 2]);
            }
            if (y * 2 + 2 < gen->m->h && Cell(gen, x, y + 1) == C_SEA)
                for (fx = fx0; fx <= fx1; fx++)
                    Put(gen, fx, y * 2 + 2, shadow[fx == fx0 ? 0 : fx == fx1 ? 2 : 1]);
        }
    }
}

static u16 WaterBlockAt(struct Gen *gen, int fx, int fy)
{
    return InMap(gen, fx, fy) ? gen->m->blocks[fy * gen->m->w + fx] : 0xFFFF;
}

// Grass, plain or tall, with nothing standing on it: what a puddle or the
// marsh may cover.
static bool8 WaterSoftGrass(struct Gen *gen, int fx, int fy, bool8 tall)
{
    u16 b = WaterBlockAt(gen, fx, fy);
    int i;
    if (!(b == B_GRASS || (tall && b == B_TALL_GRASS)))
        return FALSE;
    for (i = 0; i < gen->m->nObjs; i++)
        if (gen->m->objs[i].x == fx && gen->m->objs[i].y == fy)
            return FALSE;
    for (i = 0; i < gen->m->nWarps; i++)
        if (gen->m->warps[i].x == fx && gen->m->warps[i].y == fy)
            return FALSE;
    return TRUE;
}

// What may stand round a puddle: grass, tall grass, flowers, an ordinary
// tree's foot. Never water, sand, paths, ledges or another puddle.
static bool8 WaterPuddleRing(u16 b)
{
    u16 t = b & 0x3FF;
    (void)t;
    // (Never beside a round tree: Hoenn's puddles and marsh stand among
    // the small trees of the woods only.)
    // (Not long grass: each of its columns ends in its own lower edge.)
    return b == B_GRASS || b == B_TALL_GRASS || b == B_FLOWERS
        || b == B_CANOPY_L || b == B_CANOPY_R || b == B_CANOPY_TALL_L || b == B_CANOPY_TALL_R;
}

// A 3x3 set piece by which of the four direct neighbours are in the shape.
static u16 WaterNineSet(const u16 *set, bool8 n, bool8 s, bool8 w, bool8 e)
{
    int row = !n ? 0 : !s ? 2 : 1, col = !w ? 0 : !e ? 2 : 1;
    return set[row * 3 + col];
}

// The shape of a puddle on a 4x4 grid: a w x h box, and for a joined pair
// a second 2x2 offset by (1, 1) or (1, -1).
static void WaterPuddleShape(const u8 *s, u8 *mark)
{
    int x, y;
    memset(mark, 0, 16);
    for (y = 0; y < s[1]; y++)
        for (x = 0; x < s[0]; x++)
            mark[(y + (s[3] == 0xFF)) * 4 + x] = 1;
    if (s[2])
        for (y = 0; y < 2; y++)
            for (x = 0; x < 2; x++)
                mark[(y + (s[3] == 0xFF ? 0 : 1)) * 4 + x + 1] = 1;
}

// Does the puddle fit with its top left at (fx, fy)?
static bool8 WaterPuddleFits(struct Gen *gen, const u8 *mark, int fx, int fy)
{
    int i, x, y;
    for (i = 0; i < 16; i++)
    {
        int bx = fx + (i & 3), by = fy + (i >> 2);
        if (!mark[i])
            continue;
        if (!WaterSoftGrass(gen, bx, by, FALSE))
            return FALSE;
        for (y = -1; y <= 1; y++)
            for (x = -1; x <= 1; x++)
            {
                int rx = (i & 3) + x, ry = (i >> 2) + y;
                if (rx >= 0 && ry >= 0 && rx < 4 && ry < 4 && mark[ry * 4 + rx])
                    continue;
                if (!WaterPuddleRing(WaterBlockAt(gen, bx + x, by + y)))
                    return FALSE;
            }
    }
    return TRUE;
}

// Puddles through the grass, as on Route 120: a 2x2, 3x2, 2x3 or 3x3 pool,
// or two 2x2s joined at a corner, on plain grass with grass round it. Each
// is put at the first place it fits from a spot drawn at random.
static int WaterPuddles(struct Gen *gen, int count)
{
    static const u8 sShapes[][4] = { { 2, 2, 0, 0 }, { 3, 2, 0, 0 }, { 2, 3, 0, 0 }, { 3, 3, 0, 0 }, { 2, 2, 1, 1 }, { 2, 2, 1, 0xFF } };
    u8 mark[16];
    int k, made = 0, w = gen->m->w, h = gen->m->h, n = w * h;
    for (k = 0; k < count; k++)
    {
        const u8 *s = sShapes[RAND(sizeof(sShapes) / sizeof(sShapes[0]))];
        int start = RAND(n), j, at = -1;
        WaterPuddleShape(s, mark);
        for (j = 0; j < n && at < 0; j++)
        {
            int c = start + j >= n ? start + j - n : start + j, fx = c % w, fy = c / w;
            YIELD();
            // (A quick look at its first block before the whole check.)
            if (fx < 1 || fy < 1 || fx > w - 5 || fy > h - 5 || WaterBlockAt(gen, fx + 1, fy + 1) != B_GRASS)
                continue;
            if (WaterPuddleFits(gen, mark, fx, fy))
                at = c;
        }
        if (at < 0)
            continue;
        for (j = 0; j < 16; j++)
        {
            int rx = j & 3, ry = j >> 2;
            if (!mark[j])
                continue;
#define WATER_IN(xx, yy) ((xx) >= 0 && (yy) >= 0 && (xx) < 4 && (yy) < 4 && mark[(yy) * 4 + (xx)])
            Put(gen, at % w + rx, at / w + ry, WaterNineSet(sWaterPuddle, WATER_IN(rx, ry - 1), WATER_IN(rx, ry + 1),
                                                          WATER_IN(rx - 1, ry), WATER_IN(rx + 1, ry)));
#undef WATER_IN
        }
        made++;
    }
    return made;
}

static bool8 WaterMarshAt(struct Gen *gen, const u8 *bits, int fx, int fy)
{
    int i;
    if (!InMap(gen, fx, fy))
        return FALSE;
    i = fy * gen->m->w + fx;
    return (bits[i >> 3] >> (i & 7)) & 1;
}

// A patch of marsh, as in the middle of Route 120: wet ground of Fortree's
// with small pools, spread over a few units of open grass. TRUE if laid.
static bool8 WaterMarsh(struct Gen *gen)
{
    u8 *bits = gen->free; // a bit per block; free again once the map is done
    int tries;
    if (!WaterBridgeTiles(gen))
        return FALSE;
    int base = RAND(gen->cw * gen->ch), want = 6 + RAND(7);
    for (tries = 0; tries < gen->cw * gen->ch; tries++)
    {
        int c = (base + tries) % (gen->cw * gen->ch), ux = c % gen->cw, uy = c / gen->cw, n = 0, k, fx, fy, pools = 0;
        u16 queue[32];
        int head = 0, tail = 0;
        if (ux < 2 || uy < 2 || ux > gen->cw - 3 || uy > gen->ch - 3 || Cell(gen, ux, uy) != C_OPEN
         || !WaterSoftGrass(gen, ux * 2, uy * 2, TRUE))
            continue;
        memset(bits, 0, sizeof(gen->free));
        queue[tail++] = PACK(ux, uy);
        while (head < tail && n < want)
        {
            int cx = PX(queue[head]), cy = PY(queue[head]), i;
            bool8 fits = TRUE;
            head++;
            for (i = 0; i < 4 && fits; i++)
                fits = WaterSoftGrass(gen, cx * 2 + (i & 1), cy * 2 + (i >> 1), TRUE)
                    && !InRouteSeam(gen, cx * 2 + (i & 1), cy * 2 + (i >> 1));
            if (!fits || WaterMarshAt(gen, bits, cx * 2, cy * 2))
                continue;
            for (i = 0; i < 4; i++)
            {
                int b = (cy * 2 + (i >> 1)) * gen->m->w + cx * 2 + (i & 1);
                bits[b >> 3] |= 1 << (b & 7);
            }
            n++;
            for (i = 0; i < 4 && tail < 32; i++)
            {
                u8 d = (i + RAND(4)) & 3;
                int nx = cx + sDirDX[d], ny = cy + sDirDY[d];
                if (InBounds(gen, nx, ny) && !IsEdge(gen, nx, ny) && (Cell(gen, nx, ny) == C_OPEN || Cell(gen, nx, ny) == C_ROAD))
                    queue[tail++] = PACK(nx, ny);
            }
        }
        if (n < 4)
            continue;
        // The ring round it must take the marsh's edge (grass): a unit
        // beside anything else gives up its marsh, until none is.
        for (k = 1; k && n >= 4; )
        {
            k = 0;
            for (fy = 0; fy < gen->m->h; fy++)
                for (fx = 0; fx < gen->m->w; fx++)
                {
                    int dx, dy, i;
                    bool8 bad = FALSE;
                    if (!WaterMarshAt(gen, bits, fx, fy))
                        continue;
                    for (dy = -1; dy <= 1; dy++)
                        for (dx = -1; dx <= 1; dx++)
                        {
                            u16 b = WaterBlockAt(gen, fx + dx, fy + dy);
                            // (Its edge pieces end in a grass rim, so the
                            // trees of a meadow's edge may stand by it too.)
                            if (!WaterMarshAt(gen, bits, fx + dx, fy + dy) && InMap(gen, fx + dx, fy + dy)
                             && !WaterPuddleRing(b) && !((b & 0x3FF) >= (B_TREE_TL & 0x3FF) && (b & 0x3FF) <= (B_TREE_BR_END & 0x3FF)))
                                bad = TRUE;
                        }
                    if (!bad)
                        continue;
                    for (i = 0; i < 4; i++)
                    {
                        int b = ((fy & ~1) + (i >> 1)) * gen->m->w + (fx & ~1) + (i & 1);
                        bits[b >> 3] &= ~(1 << (b & 7));
                    }
                    n--;
                    k = 1;
                }
        }
        if (n < 4)
            continue;
        for (fy = 0; fy < gen->m->h; fy++)
            for (fx = 0; fx < gen->m->w; fx++)
                if (WaterMarshAt(gen, bits, fx, fy))
                    Put(gen, fx, fy, WaterNineSet(sWaterMarsh, WaterMarshAt(gen, bits, fx, fy - 1), WaterMarshAt(gen, bits, fx, fy + 1),
                                                  WaterMarshAt(gen, bits, fx - 1, fy), WaterMarshAt(gen, bits, fx + 1, fy)));
        // Small pools where the marsh is at least four blocks across.
        for (k = 0; k < 40 && pools < 3; k++)
        {
            int px = RAND(gen->m->w - 3), py = RAND(gen->m->h - 3), dx, dy;
            bool8 ok = TRUE;
            for (dy = -1; dy <= 2 && ok; dy++)
                for (dx = -1; dx <= 2 && ok; dx++)
                {
                    u16 b = WaterBlockAt(gen, px + dx, py + dy);
                    bool8 inner = dx >= 0 && dx <= 1 && dy >= 0 && dy <= 1;
                    if (inner ? b != 0x3291 : ((b & 0x3FF) < 0x288 || (b & 0x3FF) > 0x29A || (b & 7) > 2))
                        ok = FALSE;
                }
            if (!ok)
                continue;
            Put(gen, px, py, sWaterMarshPool[0]);
            Put(gen, px + 1, py, sWaterMarshPool[1]);
            Put(gen, px, py + 1, sWaterMarshPool[2]);
            Put(gen, px + 1, py + 1, sWaterMarshPool[3]);
            pools++;
        }
        memset(bits, 0, sizeof(gen->free));
        return TRUE;
    }
    memset(bits, 0, sizeof(gen->free));
    return FALSE;
}

// A bridge's land ends stand on plain grass, as on Routes 119 and 120:
// the way's sand or path and the tall grass stop a unit short of it, and
// the trees step back from them.
// Called before the map is drawn.
static void WaterClearBridgeEnds(struct Gen *gen)
{
    int lx, ly, rx, ry, i;
    if (!WaterBridgeEnds(gen, &lx, &ly, &rx, &ry))
        return;
    // No tree right above or below a land end: its crown or tips would be
    // cut off by the end's grass.
    // (Even a tree kept for a squeeze: bridges are never on a route with
    // its own obstacle, only a lane's is kept out of.)
    for (i = 0; i < 4; i++)
    {
        int x = (i & 1) ? rx : lx, y = ((i & 1) ? ry : ly) + ((i & 2) ? 1 : -1);
        if (Cell(gen, x, y) == C_TREE && !IsEdge(gen, x, y) && !InGateLaneF(gen, x * 2, y * 2))
            SetCell(gen, x, y, C_OPEN);
    }
    for (i = 0; i < 4; i++)
    {
        SetCover(gen, lx * 2 + (i & 1), ly * 2 + (i >> 1), V_NONE);
        SetCover(gen, rx * 2 + (i & 1), ry * 2 + (i >> 1), V_NONE);
    }
}

// TRUE if (fx, fy) is beside a bridge's step: the ground the step is
// walked onto from. A step's only other neighbours are its land end and the
// other step, so whoever stands there can close that side's way over.
static bool8 WaterNextToStep(struct Gen *gen, int fx, int fy)
{
    int k;
    for (k = 0; k < 4; k++)
        if (WaterBlockAt(gen, fx + sDirDX[k], fy + sDirDY[k]) == B_WATER_STEP)
            return TRUE;
    return FALSE;
}

// TRUE if a trainer who walks up to the player could stop on or beside a
// step, on the drawn map: each way they face, the blocks before the
// player's, up to a wall, water or a bridge.
static bool8 WaterStopsNearStep(struct Gen *gen, const struct RogueObj *o, int range)
{
    int k, n;
    for (k = 0; k < 4; k++)
    {
        if (!(ObjSightMask(o) & (1 << k)))
            continue;
        for (n = 1; n < range; n++)
        {
            u16 b = WaterBlockAt(gen, o->x + sDirDX[k] * n, o->y + sDirDY[k] * n);
            if (b == B_WATER_STEP || WaterNextToStep(gen, o->x + sDirDX[k] * n, o->y + sDirDY[k] * n))
                return TRUE;
            if (b == 0xFFFF || ((b >> 10) & 3) || (b >> 12) == 4 || (b >> 12) == 15)
                break;
        }
    }
    return FALSE;
}

// Nobody stands on a bridge or its steps: an object keeps the ground's
// elevation and would be drawn under the planks, or block the only way
// over. Nobody stands beside a step either, and no trainer who walks up to
// the player can stop on or beside one: the step is the only way from the
// ground's 3 up to the land end's 4. Placing keeps clear of them
// (WaterNoStandF, WaterLineNearStep); a trainer whose line still reaches
// one sees less far, and only what is left sends the map back for another
// try (FALSE), which redoes the whole route.
static bool8 WaterObjsOffBridges(struct Gen *gen)
{
    int i;
    for (i = 0; i < gen->m->nObjs; i++)
    {
        struct RogueObj *o = &gen->m->objs[i];
        u16 b = WaterBlockAt(gen, o->x, o->y);
        if ((b >> 12) == 4 || (b >> 12) == 15 || b == B_WATER_STEP || WaterNextToStep(gen, o->x, o->y))
            return FALSE;
        if (o->kind == ROBJ_TRAINER)
            while (o->range > 1 && WaterStopsNearStep(gen, o, o->range))
                o->range--;
        else if (o->kind == ROBJ_RIVAL && WaterStopsNearStep(gen, o, o->range))
            return FALSE;
    }
    return TRUE;
}

// After the map is drawn (Render).
static void WaterRender(struct Gen *gen)
{
    WaterRenderBridges(gen);
}

// The last touches, once everything stands: puddles and the marsh in pond
// country. Only plain grass changes, so no check made before can change.
static void WaterFinish(struct Gen *gen)
{
    if (!WaterPondCountry(gen))
        return;
    if (CHANCE(45))
        WaterMarsh(gen);
    WaterPuddles(gen, 5 + RAND(6));
}
