// v1.1 sea: more sea, and seas that differ (included at the end of
// src/rogue_gen.c).
//
// Hoenn is about half water. Before v1.1 a run had one stretch of sea, one or
// two routes right after the fifth gym's town. Now:
//
// - Some regions (SEA2_PCT) have a second, longer stretch later on, two or
//   three routes after the sixth or seventh gym, as Routes 124 to 127 lie
//   between Mossdeep and Sootopolis (SeaSecondStretch, planned in PlanSpine).
//   Surf is always won before the first stretch, so it needs no new key.
// - Sea routes come in kinds of their own (RogueGen_SeaKind): the open sea
//   with islands of before, Route 132 to 134's rock maze with fast currents,
//   and Route 109 and 126's shallows with sandbars to walk on.

// ---------------------------------------------------------------------------
// The second stretch

#define SEA2_PCT 55

// 0, or the stretch (6: after the sixth gym, 7: after the seventh) | the
// number of sea routes << 4, from a hash of the seed alone, so a region
// without it is planned as before.
static u8 SeaSecondStretch(u32 seed)
{
    u32 h = RogueRng_Hash(seed, 0x5EA2);
    u8 stretch, routes;
    if (h % 100 >= SEA2_PCT)
        return 0;
    h /= 100;
    stretch = (h % 100) < 65 ? 7 : 6;
    h /= 100;
    routes = (h % 100) < 55 ? 3 : 2;
    return stretch | (routes << 4);
}

// The places between gym g and the next become the sea's routes (after the
// sixth gym the rival's route first), with no village among them; the
// trimming of the road leaves that stretch alone.
static void SeaSecondGap(u8 *gap, u8 *villageAt, u8 *sea2)
{
    int g = *sea2 & 15, n = *sea2 >> 4;
    gap[g] = n + (g == 6);
    villageAt[g] = 0xFF;
}

// The main road's walk heads for the guide's point at each step
// (LaySpine). The second stretch lies on the sea along the coast, which the
// guide knows nothing of: its routes aim for the sea cell beside the land
// nearest their guide point, and the three places before it for the land
// beside the sea, so the walk turns to the coast in time.
static void SeaSecondAims(s8 *aimX, s8 *aimY)
{
    int i, start = -1, end, x, y, k;
    for (i = 1; i < SPINE_LEN; i++)
        if (sSpine[i] == SP_SEA && sSpine[i - 1] != SP_SEA && SeaStep() != i)
            start = i;
    if (start < 0)
        return;
    for (end = start; end + 1 < SPINE_LEN && sSpine[end + 1] == SP_SEA; end++)
        ;
    for (i = start - 3 > 1 ? start - 3 : 1; i <= end; i++)
    {
        u8 want = i >= start ? CELL_SEA : CELL_LAND, other = i >= start ? CELL_LAND : CELL_SEA;
        int best = 0x7FFF, bx = aimX[i], by = aimY[i];
        for (y = 0; y < RGRID_H; y++)
        {
            for (x = 0; x < RGRID_W; x++)
            {
                int d = Abs(x - aimX[i]) + Abs(y - aimY[i]);
                bool8 by2 = FALSE;
                if (sLand.cell[y][x] != want || d >= best)
                    continue;
                for (k = 0; k < 4; k++)
                    by2 |= LandAt(&sLand, x + sDirDX[k], y + sDirDY[k]) == other
                        && x + sDirDX[k] >= 0 && x + sDirDX[k] < RGRID_W && y + sDirDY[k] >= 0 && y + sDirDY[k] < RGRID_H;
                if (by2)
                {
                    best = d;
                    bx = x;
                    by = y;
                }
            }
        }
        aimX[i] = bx;
        aimY[i] = by;
    }
}

// TRUE if a cell of the plan's land lies beside (x, y).
static bool8 SeaByLand(int x, int y)
{
    u8 d;
    for (d = 0; d < 4; d++)
        if (LandAt(&sLand, x + sDirDX[d], y + sDirDY[d]) == CELL_LAND)
            return TRUE;
    return FALSE;
}

// TRUE if a cell of open sea lies beside (x, y).
static bool8 SeaBySea(int x, int y)
{
    u8 d;
    for (d = 0; d < 4; d++)
        if (LandAt(&sLand, x + sDirDX[d], y + sDirDY[d]) == CELL_SEA)
            return TRUE;
    return FALSE;
}

// Land on the shore, beside the open sea: the second stretch may lie on it.
static bool8 SeaShoreCell(int x, int y)
{
    return LandAt(&sLand, x, y) == CELL_LAND && SeaBySea(x, y);
}

// TRUE if a free cell beside (x, y) is sea, or land on the shore.
static bool8 SeaFreeShoreNear(int x, int y)
{
    u8 d;
    for (d = 0; d < 4; d++)
    {
        int nx = x + sDirDX[d], ny = y + sDirDY[d];
        if (nx >= 0 && ny >= 0 && nx < RGRID_W && ny < RGRID_H && !sOcc[ny][nx]
         && (sLand.cell[ny][nx] == CELL_SEA || SeaShoreCell(nx, ny)))
            return TRUE;
    }
    return FALSE;
}

// The cell of every sea route is sea, on the region map too (a second
// stretch may have been laid on the shore's land).
static void SeaSinkRoutes(struct RogueLand *L, const struct RogueRegion *r)
{
    int i;
    for (i = 0; i < r->nPlaces; i++)
        if (r->places[i].kind == RPLACE_ROUTE && r->places[i].type == RROUTE_SEA)
            L->cell[r->places[i].gridY][r->places[i].gridX] = CELL_SEA;
}

// ---------------------------------------------------------------------------
// Kinds of sea
//
// Each region has the open sea, and some (by a hash of the seed, so nothing
// else in the plan moves) Route 132 to 134's rock maze with currents and
// Route 109's shallows with sandbars. Each sea route takes one of the
// region's kinds, never the kind of the sea route before it where there is
// another. The wreck's route stays open sea (Slateport's tileset), and a
// maze's route has Pacifidlog's (RTHEME_CURRENTS, set by SeaPlanKinds), the
// only tileset with currents.

#define SEA_MAZE_PCT     45 // regions with the rock maze
#define SEA_SHALLOWS_PCT 55 // regions with the shallows

static u8 SeaRegionKinds(u32 seed)
{
    u32 h = RogueRng_Hash(seed, 0x5EA3);
    u8 kinds = 1 << SEA_OPEN;
    if (h % 100 < SEA_MAZE_PCT)
        kinds |= 1 << SEA_MAZE;
    if ((h / 100) % 100 < SEA_SHALLOWS_PCT)
        kinds |= 1 << SEA_SHALLOWS;
    return kinds;
}

static u8 SeaKindOf(const struct RogueRegion *r, u8 place, u8 depth)
{
    static const u8 sWeight[SEA_KIND_COUNT] = { [SEA_OPEN] = 30, [SEA_MAZE] = 40, [SEA_SHALLOWS] = 40 };
    const struct RoguePlace *p = &r->places[place];
    u8 kinds, prev = 0xFF, k, w[SEA_KIND_COUNT];
    u32 total = 0, others, roll;
    int i;
    if (p->kind != RPLACE_ROUTE || p->type != RROUTE_SEA || RogueGen_IsRun0(r) || RogueGen_DungeonAt(r, place) >= 0)
        return SEA_OPEN;
    kinds = SeaRegionKinds(r->seed);
    for (i = 0; i < p->nLinks && depth < 6; i++)
    {
        u8 t = p->links[i].target;
        if (t < place && r->places[t].kind == RPLACE_ROUTE && r->places[t].type == RROUTE_SEA)
            prev = SeaKindOf(r, t, depth + 1);
    }
    for (k = 0; k < SEA_KIND_COUNT; k++)
    {
        w[k] = (kinds >> k) & 1 ? sWeight[k] : 0;
        total += w[k];
    }
    others = prev < SEA_KIND_COUNT ? total - w[prev] : 0;
    if (others > 0)
    {
        total = others;
        w[prev] = 0;
    }
    roll = RogueRng_Hash(r->seed, 0x5EA4 + place) % total;
    for (k = 0; k + 1 < SEA_KIND_COUNT && roll >= w[k]; k++)
        roll -= w[k];
    return k;
}

u8 RogueGen_SeaKind(const struct RogueRegion *r, u8 place)
{
    return SeaKindOf(r, place, 0);
}

// After the dungeons (the wreck): a maze's route takes Pacifidlog's tileset.
static void SeaPlanKinds(struct RogueRegion *r)
{
    int i;
    for (i = 0; i < r->nPlaces; i++)
        if (RogueGen_SeaKind(r, i) == SEA_MAZE)
            r->places[i].theme = RTHEME_CURRENTS;
}

// Round corners (v1.1 round 2). The shallows and the sandbars are laid in
// whole units; the outer corner block of a unit of shallows that juts into
// the sea is sea, and of a sandbar's sand that juts into its shallows is
// shallows, so the banks' outlines step a block at a time, as Route 132's
// do (each piece is one Hoenn draws there). Only where the bank stays two
// blocks thick, and four corners in five.
static bool8 SeaCornerBit(struct Gen *gen, int fx, int fy, u8 here, u8 out)
{
    int ux = fx >> 1, uy = fy >> 1, dx = (fx & 1) ? 1 : -1, dy = (fy & 1) ? 1 : -1;
    u8 a, b;
    if (gen->p->kind != RPLACE_ROUTE || gen->p->type != RROUTE_SEA || !InMap(gen, fx, fy)
     || Cell(gen, ux, uy) != here || BitU(gen->keep, gen, ux, uy) || IsEdge(gen, ux, uy))
        return FALSE;
    if (Cell(gen, ux + dx, uy) != out || Cell(gen, ux, uy + dy) != out || Cell(gen, ux + dx, uy + dy) != out
     || BitU(gen->river, gen, ux + dx, uy) || BitU(gen->river, gen, ux, uy + dy))
        return FALSE;
    a = Cell(gen, ux - dx, uy);
    b = Cell(gen, ux, uy - dy);
    if ((a != C_FORD && a != C_OPEN) || (b != C_FORD && b != C_OPEN) || (here == C_OPEN && (a != C_OPEN || b != C_OPEN)))
        return FALSE;
    return RogueRng_Hash(gen->r->seed ^ 0x5EAB, fy * 64 + fx) % 5 != 0;
}

// A block of a unit of shallows drawn as sea, and of a sandbar drawn as shallows.
static bool8 SeaFordBittenF(struct Gen *gen, int fx, int fy)
{
    return SeaCornerBit(gen, fx, fy, C_FORD, C_SEA);
}

static bool8 SeaSandBittenF(struct Gen *gen, int fx, int fy)
{
    return SeaCornerBit(gen, fx, fy, C_OPEN, C_FORD);
}

// Swimmers keep to the open water of a maze or the shallows' channels, as on
// Routes 109 and 132 to 134: half the water trainers there, the rest
// Fishermen, Tubers and Sailors on the sand.
bool8 RogueGen_TrainerSwims(const struct RogueRegion *r, u8 place, u8 index)
{
    if (place >= r->nPlaces || RogueGen_SeaKind(r, place) == SEA_OPEN || !RogueGen_TrainerByWater(r, place, index))
        return FALSE;
    return RogueRng_Hash(r->seed, 0x5EA5 + place * 16 + index) % 100 < 50;
}

// ---------------------------------------------------------------------------
// Drawing the new kinds
//
// Units are laid as for the open sea (C_SEA, C_OPEN sand, C_TREE), with
// shallows as C_FORD (MB_SHALLOW_WATER: walked on, with splashes). Three
// bitmaps the sea route has no other use for mark the rest: gen->keep the
// units of a sea rock (on the sea or on the shallows below it), gen->river
// the units of a current, and gen->ashZone those whose current runs the
// second way (gen->layout: the first way | the second << 4).

#define SeaRockU(gen, x, y)    BitU((gen)->keep, gen, x, y)
#define SeaCurrentU(gen, x, y) (Cell(gen, x, y) == C_SEA && BitU((gen)->river, gen, x, y))

static u8 SeaCurrentDir(struct Gen *gen, int ux, int uy)
{
    if (!InBounds(gen, ux, uy) || !SeaCurrentU(gen, ux, uy))
        return RDIR_NONE;
    return BitU(gen->ashZone, gen, ux, uy) ? (gen->layout >> 4) & 3 : gen->layout & 3;
}

// The current a block carries the player along, or RDIR_NONE.
// The stream's edge steps a block at a time, as Route 133's does: the outer
// corner block of a unit at a corner of the stream is plain sea.
static u8 SeaCurrentF(struct Gen *gen, int fx, int fy)
{
    int ux = fx >> 1, uy = fy >> 1, dx = (fx & 1) ? 1 : -1, dy = (fy & 1) ? 1 : -1;
    u8 dir;
    if (!InMap(gen, fx, fy) || BittenF(gen, fx, fy, C_SEA) || SeaRockU(gen, ux, uy))
        return RDIR_NONE;
    dir = SeaCurrentDir(gen, ux, uy);
    if (dir != RDIR_NONE && SeaCurrentDir(gen, ux + dx, uy) == RDIR_NONE && SeaCurrentDir(gen, ux, uy + dy) == RDIR_NONE
     && SeaCurrentDir(gen, ux + dx, uy + dy) == RDIR_NONE)
        return RDIR_NONE;
    return dir;
}

// TRUE if the player may be on a block: on foot or surfing, not on a rock,
// a tree or anyone standing there.
static bool8 SeaPassF(struct Gen *gen, int fx, int fy)
{
    u8 c;
    if (!InMap(gen, fx, fy) || IsOccupied(gen, fx, fy))
        return FALSE;
    c = Cell(gen, fx >> 1, fy >> 1);
    if (SeaRockU(gen, fx >> 1, fy >> 1))
        return FALSE;
    if (c == C_SEA)
        return TRUE; // (a bitten corner is beach)
    return IsOpenU(c);
}

// Where the player can get to from block (sx, sy), surfing and walking, the
// currents carrying them on until something stops them or the current ends,
// as the game's forced movement does. In seen, a bit per block.
static void SeaReach(struct Gen *gen, int sx, int sy, u8 *seen)
{
    u16 *queue = sBfsQueue;
    int head = 0, tail = 0, w = gen->m->w, n = w * gen->m->h;
    memset(seen, 0, (n + 7) / 8);
    if (!SeaPassF(gen, sx, sy))
        return;
    seen[(sy * w + sx) >> 3] |= 1 << ((sy * w + sx) & 7);
    queue[tail++ & (BFS_RING - 1)] = PACK(sx, sy);
    while (head < tail)
    {
        int cur = queue[head++ & (BFS_RING - 1)], cx = PX(cur), cy = PY(cur);
        u8 d;
        YIELD();
        for (d = 0; d < 4; d++)
        {
            int nx = cx + sDirDX[d], ny = cy + sDirDY[d], k, i;
            u8 c;
            if (!SeaPassF(gen, nx, ny))
                continue;
            for (k = 0; k < 64 && (c = SeaCurrentF(gen, nx, ny)) != RDIR_NONE; k++)
            {
                if (!SeaPassF(gen, nx + sDirDX[c], ny + sDirDY[c]))
                    break;
                nx += sDirDX[c];
                ny += sDirDY[c];
            }
            i = ny * w + nx;
            if ((seen[i >> 3] >> (i & 7)) & 1)
                continue;
            seen[i >> 3] |= 1 << (i & 7);
            if (tail - head < BFS_RING)
                queue[tail++ & (BFS_RING - 1)] = PACK(nx, ny);
        }
    }
}

static bool8 SeaSeen(struct Gen *gen, const u8 *seen, int fx, int fy)
{
    int i = fy * gen->m->w + fx;
    return InMap(gen, fx, fy) && ((seen[i >> 3] >> (i & 7)) & 1);
}

// TRUE if a block beside (fx, fy) is in seen.
static bool8 SeaSeenBeside(struct Gen *gen, const u8 *seen, int fx, int fy)
{
    u8 d;
    for (d = 0; d < 4; d++)
        if (SeaSeen(gen, seen, fx + sDirDX[d], fy + sDirDY[d]))
            return TRUE;
    return FALSE;
}

// A shallows block: as a ford against deep water to the north, and against
// sand (a sandbar) with the edges and corners Routes 132 to 134 have, drawn
// in the shallows.
static bool8 SeaSandF(struct Gen *gen, int fx, int fy)
{
    u8 c;
    if (!InMap(gen, fx, fy))
        return FALSE;
    c = Cell(gen, fx >> 1, fy >> 1);
    return ((c == C_OPEN || c == C_ROAD) && !SeaSandBittenF(gen, fx, fy)) || (c == C_SEA && BittenF(gen, fx, fy, C_SEA));
}

static u16 SeaShallowBlock(struct Gen *gen, int fx, int fy)
{
    bool8 n = SeaSandF(gen, fx, fy - 1), s = SeaSandF(gen, fx, fy + 1), e = SeaSandF(gen, fx + 1, fy), w = SeaSandF(gen, fx - 1, fy);
    // (Deep water only off a corner is drawn by the sea block there: the
    // sand's edges come first.)
    if (IsSeaF(gen, fx, fy - 1))
        return FordBlock(gen, fx, fy);
    if (n && w) return 0x318E;
    if (n && e) return 0x318D;
    if (s && w) return 0x3186;
    if (s && e) return 0x3185;
    if (n) return 0x31A6;
    if (s) return 0x3196;
    if (e) return 0x319D;
    if (w) return 0x319F;
    if (SeaSandF(gen, fx - 1, fy - 1)) return 0x31A7;
    if (SeaSandF(gen, fx + 1, fy - 1)) return 0x31A5;
    if (SeaSandF(gen, fx + 1, fy + 1)) return 0x3195;
    if (SeaSandF(gen, fx - 1, fy + 1)) return 0x3197;
    return 0x319E;
}

// Over the drawn map: currents, rocks, and the shallows' sand edges.
static void SeaRenderKind(struct Gen *gen)
{
    // A swirling rock and a pointed rock on the sea; a pointed rock on the
    // shallows (Routes 132 to 134).
    static const u16 sSwirl[4] = { 0x0550, 0x0551, 0x0558, 0x0559 };
    static const u16 sPointed[4] = { 0x0575, 0x0576, 0x057D, 0x057E };
    static const u16 sShoalRock[4] = { 0x055E, 0x055F, 0x056E, 0x056F };
    // Pacifidlog's current water, by the way it runs (N E S W).
    static const u16 sCurrent[4] = { 0x1283, 0x1265, 0x128C, 0x1263 };
    int ux, uy, k;
    for (uy = 0; uy < gen->ch; uy++)
    {
        for (ux = 0; ux < gen->cw; ux++)
        {
            u8 c = Cell(gen, ux, uy);
            for (k = 0; k < 4; k++)
            {
                int fx = ux * 2 + (k & 1), fy = uy * 2 + (k >> 1);
                if (SeaRockU(gen, ux, uy))
                {
                    const u16 *rock = c == C_FORD ? sShoalRock : (RogueRng_Hash(gen->r->seed, ux * 64 + uy) & 1) ? sSwirl : sPointed;
                    Put(gen, fx, fy, rock[k]);
                }
                else if (c == C_FORD && SeaFordBittenF(gen, fx, fy))
                    Put(gen, fx, fy, SeaBlock(gen, fx, fy));
                else if (c == C_FORD || SeaSandBittenF(gen, fx, fy))
                    Put(gen, fx, fy, SeaShallowBlock(gen, fx, fy));
                else if (SeaCurrentF(gen, fx, fy) != RDIR_NONE)
                    Put(gen, fx, fy, sCurrent[SeaCurrentF(gen, fx, fy)]);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Laying the new kinds out

// The open water, the shores along sides with land beyond and the landings
// at the ways in and out, as the open sea has them (GenSeaRoute). outDir is
// RDIR_NONE for a dead end.
static void SeaLayBase(struct Gen *gen, u8 inDir, u8 outDir, int *ax, int *ay, int *bx, int *by)
{
    int i, k;
    u8 d;
    gen->groundKind = GROUND_SAND;
    for (i = 0; i < gen->cw * gen->ch; i++)
        gen->g[i] = C_SEA;
    for (d = 0; d < 4; d++)
    {
        int len = EdgeLength(gen, d), pos;
        for (pos = 0; pos < len; pos++)
        {
            int depth = SeaLandDepth(gen->r, gen->p, d, pos, SEA_SHORE_LEVELS), x, y;
            YIELD();
            for (k = 0; k < depth; k++)
            {
                EdgeCell(gen, d, pos, k, &x, &y);
                SetCell(gen, x, y, k == 0 ? C_TREE : (Cell(gen, x, y) == C_TREE ? C_TREE : C_OPEN));
            }
        }
    }
    MakeOpening(gen, inDir, ax, ay);
    *bx = *by = -1;
    if (outDir != RDIR_NONE)
        MakeOpening(gen, outDir, bx, by);
    for (k = 0; k < 2; k++)
    {
        int lx = k ? *bx : *ax, ly = k ? *by : *ay, x, y;
        d = k ? outDir : inDir;
        if (d == RDIR_NONE)
            continue;
        // (v1.1 towns2) Toward a raft village the water runs on across the
        // seam: no landing (GenSeaRoute).
        if (SeaToRaft(gen, d))
        {
            EdgeCell(gen, d, gen->exitPos[d] / 2, 0, &x, &y);
            SetCell(gen, x, y, C_SEA);
            SetCell(gen, lx, ly, C_SEA);
            continue;
        }
        for (y = ly - 2; y <= ly + 2; y++)
            for (x = lx - 2; x <= lx + 2; x++)
                if (InBounds(gen, x, y) && !IsEdge(gen, x, y) && Abs(x - lx) + Abs(y - ly) <= 3)
                    SetCell(gen, x, y, C_OPEN);
        EdgeCell(gen, d, gen->exitPos[d] / 2, 0, &x, &y);
        SetCell(gen, x, y, C_ROAD);
        SetCell(gen, lx, ly, C_ROAD);
    }
}

// TRUE if no unit within r units of (x, y) is anything but plain sea: no
// land, shallows, rock or current.
static bool8 SeaClearAround(struct Gen *gen, int x, int y, int r)
{
    int dx, dy;
    for (dy = -r; dy <= r; dy++)
        for (dx = -r; dx <= r; dx++)
            if (!InBounds(gen, x + dx, y + dy) || Cell(gen, x + dx, y + dy) != C_SEA
             || SeaRockU(gen, x + dx, y + dy) || BitU(gen->river, gen, x + dx, y + dy))
                return FALSE;
    return TRUE;
}

// TRUE if every unit within r units of (x, y) is sea (a current's or not)
// with no rock.
static bool8 SeaOpenAround(struct Gen *gen, int x, int y, int r)
{
    int dx, dy;
    for (dy = -r; dy <= r; dy++)
        for (dx = -r; dx <= r; dx++)
            if (!InBounds(gen, x + dx, y + dy) || Cell(gen, x + dx, y + dy) != C_SEA || SeaRockU(gen, x + dx, y + dy))
                return FALSE;
    return TRUE;
}

// TRUE if (x, y) lies within r units of an edge with a place beyond it,
// where the next map draws this one with its own tileset (InRouteSeam), or
// within two of an open edge.
static bool8 SeaNearEdge(struct Gen *gen, int x, int y, int r)
{
    u8 d;
    for (d = 0; d < 4; d++)
    {
        int at = d == RDIR_NORTH ? y : d == RDIR_SOUTH ? gen->ch - 1 - y : d == RDIR_WEST ? x : gen->cw - 1 - x;
        const struct RogueLink *l = LinkOnSide(gen->p, d);
        int depth = 2;
        if (l != NULL)
        {
            // (v1.1 round 2: only along the neighbour, and four units past
            // its ends, as far as the camera shows from its corners; beyond
            // it the edge is open sea.)
            int along = (d == RDIR_NORTH || d == RDIR_SOUTH) ? x : y, there = along - l->offset / 2;
            int lenT = PlaceEdgeLength(&gen->r->places[l->target], d);
            if (there >= -4 && there < lenT + 4)
                depth = r;
        }
        else if (!OpenSeaSide(gen->r, gen->p, d))
            depth = r;
        if (at < depth)
            return TRUE;
    }
    return FALSE;
}

// As on Routes 105 to 109, the way across needs Surf: where beaches,
// shallows and islands join into a walk from one landing to the other (or
// to the island at a dead end's far end), the sea breaks through it
// halfway, and again until no walk is left. Inlets one unit across are
// filled in first. FALSE if the walk will not break.
static bool8 SeaNeedSurf(struct Gen *gen, int ax, int ay, int bx, int by)
{
    int tries, x, y, k, at;
    for (tries = 0; tries < 8; tries++)
    {
        for (k = 0; k < 2; k++)
        {
            for (y = 1; y < gen->ch - 1; y++)
            {
                for (x = 1; x < gen->cw - 1; x++)
                {
                    int land = 0, ford = 0;
                    u8 dd;
                    if (Cell(gen, x, y) != C_SEA || BitU(gen->river, gen, x, y) || SeaRockU(gen, x, y))
                        continue;
                    for (dd = 0; dd < 4; dd++)
                    {
                        u8 t = Cell(gen, x + sDirDX[dd], y + sDirDY[dd]);
                        ford += t == C_FORD;
                        land += t != C_SEA && t != C_FORD;
                    }
                    if (land + ford >= 3)
                        SetCell(gen, x, y, ford ? C_FORD : C_OPEN);
                }
            }
        }
        at = LandWalkMiddle(gen, ax, ay, bx, by);
        if (at < 0)
            return TRUE;
        for (y = at / gen->cw - 1; y <= at / gen->cw + 1; y++)
            for (x = at % gen->cw - 1; x <= at % gen->cw + 1; x++)
                if (InBounds(gen, x, y) && !IsEdge(gen, x, y) && IsOpenU(Cell(gen, x, y))
                 && Abs(x - ax) + Abs(y - ay) > 3 && Abs(x - bx) + Abs(y - by) > 3)
                {
                    SetCell(gen, x, y, C_SEA);
                    // (A rock left standing on dry land would be drawn on the sea.)
                    gen->keep[(y * gen->cw + x) >> 3] &= ~(1 << ((y * gen->cw + x) & 7));
                }
    }
    return FALSE;
}

// Shallows meet sand only round a sandbar's edge and the deep sea only at
// the shallows' own edge: no unit of deep sea touches both, and none of the
// shallows touches the shore's land (Routes 132 to 134 draw neither). Sea
// beside both becomes shallows, a sandbar's sand beside the deep sea too.
static void SeaTidyShallows(struct Gen *gen)
{
    int pass, x, y, dx, dy;
    for (pass = 0; pass < 4; pass++)
    {
        bool8 changed = FALSE;
        for (y = 0; y < gen->ch; y++)
        {
            YIELD();
            for (x = 0; x < gen->cw; x++)
            {
                u8 c = Cell(gen, x, y);
                bool8 ford = FALSE, land = FALSE;
                if (c != C_SEA || BitU(gen->river, gen, x, y))
                    continue;
                for (dy = -1; dy <= 1; dy++)
                {
                    for (dx = -1; dx <= 1; dx++)
                    {
                        u8 t = Cell(gen, x + dx, y + dy);
                        if (!InBounds(gen, x + dx, y + dy))
                            continue;
                        ford |= t == C_FORD;
                        land |= t != C_SEA && t != C_FORD;
                    }
                }
                // (And a channel of sea one unit across between shallows,
                // which Hoenn's pieces cannot draw.)
                if ((ford && land) || (Cell(gen, x - 1, y) == C_FORD && Cell(gen, x + 1, y) == C_FORD)
                 || (Cell(gen, x, y - 1) == C_FORD && Cell(gen, x, y + 1) == C_FORD))
                {
                    SetCell(gen, x, y, C_FORD);
                    changed = TRUE;
                }
            }
        }
        // Shallows that meet only at a corner (a checkerboard of two by two
        // units): the sea between them joins them.
        for (y = 0; y + 1 < gen->ch; y++)
            for (x = 0; x + 1 < gen->cw; x++)
            {
                u8 a = Cell(gen, x, y), b = Cell(gen, x + 1, y), c = Cell(gen, x, y + 1), d = Cell(gen, x + 1, y + 1);
                if (a == C_FORD && d == C_FORD && b == C_SEA && c == C_SEA && !BitU(gen->river, gen, x + 1, y))
                {
                    SetCell(gen, x + 1, y, C_FORD);
                    changed = TRUE;
                }
                else if (b == C_FORD && c == C_FORD && a == C_SEA && d == C_SEA && !BitU(gen->river, gen, x, y))
                {
                    SetCell(gen, x, y, C_FORD);
                    changed = TRUE;
                }
            }
        if (!changed)
            break;
    }
}

// TRUE if a unit of shallows touches land that is no sandbar's (the shore's
// trees and beach, a landing): the shallows must stay out at sea.
static bool8 SeaShallowsAshore(struct Gen *gen)
{
    int x, y, dx, dy;
    for (y = 0; y < gen->ch; y++)
        for (x = 0; x < gen->cw; x++)
            if (Cell(gen, x, y) == C_FORD)
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++)
                    {
                        u8 t = Cell(gen, x + dx, y + dy);
                        if (InBounds(gen, x + dx, y + dy) && (t == C_TREE || t == C_ROAD || IsEdge(gen, x + dx, y + dy)))
                            return TRUE;
                    }
    return FALSE;
}

// A shoal: a ragged patch of shallows from overlapping ellipses (radii rx,
// ry), with a sandbar on it where it is deep enough (sand), only on open
// sea with plain sea round it, as on Routes 132 to 134.
static bool8 SeaSandbar(struct Gen *gen, int cx, int cy, int rx, int ry, bool8 sand, bool8 merge)
{
    u8 *mark = gen->cover; // the cover is painted after the shoals
    int k, x, y, n = gen->cw * gen->ch, cores = 0;
    bool8 ok = TRUE;
    memset(mark, 0, n);
    // Grown a unit at a time from the middle, within rx and ry of it, then
    // its bays filled in and its points cut back, so its edge steps about
    // as Route 132's shoals do.
    {
        // Within an ellipse, filled a little over half, so the outline
        // wanders; a lean toward one side now and then gives a long bank.
        // (v1.1 round 2: a bank of two or three lobes, each grown on its
        // own, so it is never one box.)
        u16 *grown = sBfsQueue;
        int lobe, lobes = rx >= 3 ? 2 + RAND(2) : 1;
        for (lobe = 0; lobe < lobes; lobe++)
        {
            int lx = cx, ly = cy, lrx = rx, lry = ry, nGrown = 1, want, lean = RAND(5);
            if (lobe)
            {
                lx = cx + (int)RAND(rx + 1) - rx / 2 * 1 + (RAND(2) ? rx / 2 : -rx / 2);
                ly = cy + (int)RAND(2 * ry + 1) - ry;
                lrx = 1 + RAND(rx);
                lry = 1 + RAND(ry);
                if (!InBounds(gen, lx, ly) || !(mark[ly * gen->cw + lx] & 1))
                    continue;
            }
            want = (3 * (2 * lrx + 1) * (2 * lry + 1)) / 7;
            mark[ly * gen->cw + lx] = 1;
            grown[0] = PACK(lx, ly);
            for (k = 0; k < want * 8 && nGrown < want; k++)
            {
                int g = grown[RAND(nGrown)], tx = PX(g), ty = PY(g);
                u8 d = lean < 4 && RAND(3) == 0 ? lean : RAND(4);
                tx += sDirDX[d];
                ty += sDirDY[d];
                if (!InBounds(gen, tx, ty) || !InEllipse(tx - lx, ty - ly, lrx, lry) || (mark[ty * gen->cw + tx] & 1))
                    continue;
                mark[ty * gen->cw + tx] = 1;
                grown[nGrown++] = PACK(tx, ty);
            }
        }
    }
    for (k = 0; k < 2; k++)
    {
        for (y = 1; y < gen->ch - 1; y++)
            for (x = 1; x < gen->cw - 1; x++)
            {
                int i = y * gen->cw + x, nb = (mark[i - 1] & 1) + (mark[i + 1] & 1) + (mark[i - gen->cw] & 1) + (mark[i + gen->cw] & 1);
                if (!(mark[i] & 1) && nb >= 3)
                    mark[i] = 1;
                else if ((mark[i] & 1) && nb <= 1 && !(x == cx && y == cy))
                    mark[i] = 0;
            }
    }
    // Plain sea all round, two units out, away from the edges, rocks and
    // currents, and three from any land.
    for (y = 0; y < gen->ch && ok; y++)
    {
        for (x = 0; x < gen->cw && ok; x++)
        {
            int dx, dy;
            if (!(mark[y * gen->cw + x] & 1))
                continue;
            for (dy = -3; dy <= 3 && ok; dy++)
                for (dx = -3; dx <= 3 && ok; dx++)
                {
                    int nx = x + dx, ny = y + dy;
                    bool8 near = Abs(dx) <= 2 && Abs(dy) <= 2, next = Abs(dx) <= 1 && Abs(dy) <= 1;
                    if (!InBounds(gen, nx, ny) || (near && BitU(gen->river, gen, nx, ny))
                     || (next && (IsEdge(gen, nx, ny) || SeaRockU(gen, nx, ny)))
                     || (Cell(gen, nx, ny) != C_SEA && !(mark[ny * gen->cw + nx] & 1)
                         && !(merge && Cell(gen, nx, ny) == C_FORD)))
                        ok = FALSE;
                }
        }
    }
    if (ok)
    {
        for (y = 0; y < gen->ch; y++)
            for (x = 0; x < gen->cw; x++)
                if (mark[y * gen->cw + x] & 1)
                    SetCell(gen, x, y, C_FORD);
        // The sandbar: shoal units with shoal all round them.
        for (y = 1; y < gen->ch - 1 && sand; y++)
            for (x = 1; x < gen->cw - 1; x++)
            {
                int dx, dy, all = 1;
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++)
                        all &= mark[(y + dy) * gen->cw + x + dx] & 1;
                if (all)
                {
                    SetCell(gen, x, y, C_OPEN);
                    cores++;
                }
            }
    }
    memset(mark, 0, n);
    return ok;
}

// The sandbars of the shallows: in pct of the banks (shoals joined by
// their shallows), the units with shallows all round them are sand.
static void SeaSandCores(struct Gen *gen, int pct)
{
    u8 *mark = gen->cover; // painted later
    u16 *queue = sBfsQueue;
    int x, y, n = gen->cw * gen->ch;
    memset(mark, 0, n);
    for (y = 0; y < gen->ch; y++)
    {
        for (x = 0; x < gen->cw; x++)
        {
            int head = 0, tail = 0, k;
            bool8 sand = CHANCE(pct);
            if (Cell(gen, x, y) != C_FORD || mark[y * gen->cw + x])
                continue;
            mark[y * gen->cw + x] = 1;
            queue[tail++] = PACK(x, y);
            while (head < tail)
            {
                int cur = queue[head++];
                u8 d;
                for (d = 0; d < 4; d++)
                {
                    int nx = PX(cur) + sDirDX[d], ny = PY(cur) + sDirDY[d];
                    if (!InBounds(gen, nx, ny) || Cell(gen, nx, ny) != C_FORD || mark[ny * gen->cw + nx])
                        continue;
                    mark[ny * gen->cw + nx] = 1;
                    queue[tail++] = PACK(nx, ny);
                }
            }
            for (k = 0; k < tail && sand; k++)
            {
                int ux = PX(queue[k]), uy = PY(queue[k]), dx, dy, all = 1;
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++)
                    {
                        u8 c = Cell(gen, ux + dx, uy + dy);
                        all &= InBounds(gen, ux + dx, uy + dy) && (c == C_FORD || (c == C_OPEN && mark[(uy + dy) * gen->cw + ux + dx] == 2));
                    }
                if (all && !SeaRockU(gen, ux, uy))
                    mark[uy * gen->cw + ux] = 2;
            }
        }
    }
    for (y = 0; y < n; y++)
        if (mark[y] == 2)
            gen->g[y] = C_OPEN;
    memset(mark, 0, n);
}

// A sandy islet of a few units, plain sea all round (the open sea's
// beaches against the water), well away from the current.
static bool8 SeaIslet(struct Gen *gen, int cx, int cy)
{
    int x, y, rx = 1, ry = 1;
    for (y = cy - ry - 3; y <= cy + ry + 3; y++)
        for (x = cx - rx - 3; x <= cx + rx + 3; x++)
            if (!InBounds(gen, x, y) || IsEdge(gen, x, y) || Cell(gen, x, y) != C_SEA
             || BitU(gen->river, gen, x, y) || SeaRockU(gen, x, y))
                return FALSE;
    SeaIsland(gen, cx, cy, rx, ry);
    return TRUE;
}

// A rock at unit (x, y): on plain sea or shallows, never beside land or
// another kind of water, so it stands whole on its own water.
static bool8 SeaAddRock(struct Gen *gen, int x, int y)
{
    int dx, dy;
    u8 c = Cell(gen, x, y);
    if (!InBounds(gen, x, y) || IsEdge(gen, x, y) || (c != C_SEA && c != C_FORD) || BitU(gen->river, gen, x, y))
        return FALSE;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
        {
            u8 t = Cell(gen, x + dx, y + dy), d = SeaCurrentDir(gen, x + dx, y + dy);
            if (InBounds(gen, x + dx, y + dy) && t != c && !(c == C_SEA && t == C_SEA))
                return FALSE;
            // (Hoenn's rocks stand beside its westward streams only; a
            // stream running north or south keeps a unit of calm water.)
            if (d == RDIR_NORTH || d == RDIR_SOUTH)
                return FALSE;
        }
    SetBitU(gen->keep, gen, x, y);
    return TRUE;
}

// The current: a band along the way from one landing toward the other, as
// Route 132 to 134's streams run, wandering a little and changing width, in
// two legs round a corner (the second leg's units in gen->ashZone). It keeps
// a unit of plain sea from anything else, and out of view of the seams,
// where the next map draws this one with its own tileset (which has no
// current tiles). forward: it runs toward the way on (else back toward the
// way in).
static void SeaLayCurrent(struct Gen *gen, int ax, int ay, int bx, int by, u8 inDir, bool8 forward, int hw)
{
    bool8 across = inDir == RDIR_WEST || inDir == RDIR_EAST;
    int leg;
    u8 dirs[2];
    dirs[0] = across ? (bx >= ax ? RDIR_EAST : RDIR_WEST) : (by >= ay ? RDIR_SOUTH : RDIR_NORTH);
    dirs[1] = across ? (by >= ay ? RDIR_SOUTH : RDIR_NORTH) : (bx >= ax ? RDIR_EAST : RDIR_WEST);
    if (!forward)
    {
        dirs[0] = Opposite(dirs[0]);
        dirs[1] = Opposite(dirs[1]);
    }
    gen->layout = dirs[0] | (dirs[1] << 4);
    // The stream starts a little toward the middle of the map from the way in.
    if (across)
        ay = (ay + gen->ch / 2) / 2;
    else
        ax = (ax + gen->cw / 2) / 2;
    for (leg = 0; leg < 2; leg++)
    {
        // Along the leg's axis (t) from its start to its end; the stream's
        // middle (c) drifts a unit now and then, its width too.
        bool8 alongX = leg ? !across : across;
        int t0 = leg ? (alongX ? ax : ay) : (alongX ? ax : ay), t1, c, w = hw, t, k, step;
        if (leg == 0)
        {
            t0 = alongX ? ax : ay;
            t1 = alongX ? bx : by;
            c = alongX ? ay : ax;
        }
        else
        {
            t0 = alongX ? ax : ay;
            t1 = alongX ? bx : by;
            c = alongX ? by : bx;
            // A straight route has no second leg.
            if (!across == !alongX || Abs(t1 - t0) < 3)
                break;
        }
        step = t1 >= t0 ? 1 : -1;
        for (t = t0; t != t1 + step; t += step)
        {
            if (RAND(3) == 0)
                c += RAND(2) ? 1 : -1;
            if (RAND(4) == 0)
                w = hw + (int)RAND(3) - 1;
            if (w < 1)
                w = 1;
            for (k = -w; k <= w; k++)
            {
                int x = alongX ? t : c + k, y = alongX ? c + k : t;
                if (!InBounds(gen, x, y) || SeaNearEdge(gen, x, y, 5) || !SeaOpenAround(gen, x, y, 1))
                    continue;
                SetBitU(gen->river, gen, x, y);
                if (leg)
                    SetBitU(gen->ashZone, gen, x, y);
            }
        }
    }
}

// On a straight route, now and then a second stream beside the first,
// running the other way (in gen->ashZone, the second way), with calm water
// and a line of rocks between, as Route 133's streams lie side by side: the
// way there rides one, the way back the other.
static void SeaLayReturnStream(struct Gen *gen, int ax, int ay, int bx, int by, u8 inDir, int hw)
{
    bool8 across = inDir == RDIR_WEST || inDir == RDIR_EAST;
    u8 d0 = gen->layout & 3;
    int c = (across ? (ay + gen->ch / 2) / 2 : (ax + gen->cw / 2) / 2) + (RAND(2) ? 1 : -1) * (2 * hw + 3);
    int t0 = across ? ax : ay, t1 = across ? bx : by, step = t1 >= t0 ? 1 : -1, t, k, w = hw;
    gen->layout = (gen->layout & 0x0F) | (Opposite(d0) << 4);
    for (t = t0; t != t1 + step; t += step)
    {
        if (RAND(4) == 0)
            c += RAND(2) ? 1 : -1;
        if (RAND(4) == 0)
            w = hw + (int)RAND(3) - 1;
        if (w < 1)
            w = 1;
        for (k = -w; k <= w; k++)
        {
            int x = across ? t : c + k, y = across ? c + k : t, dx, dy;
            bool8 clear = TRUE;
            if (!InBounds(gen, x, y) || SeaNearEdge(gen, x, y, 5) || !SeaOpenAround(gen, x, y, 1))
                continue;
            // A unit of calm water from the first stream.
            for (dy = -1; dy <= 1; dy++)
                for (dx = -1; dx <= 1; dx++)
                    if (BitU(gen->river, gen, x + dx, y + dy) && !BitU(gen->ashZone, gen, x + dx, y + dy))
                        clear = FALSE;
            if (!clear)
                continue;
            SetBitU(gen->river, gen, x, y);
            SetBitU(gen->ashZone, gen, x, y);
        }
    }
}

static void SeaDropSlivers(struct Gen *gen, int min);

// After the shallows are tidied and the walk broken: no rock beside
// another kind of ground, no unit of current beside anything but plain sea,
// rocks and current (the shallows' and the sand's edges have no current
// pieces), then no slivers.
static void SeaCurrentsClear(struct Gen *gen)
{
    int x, y, dx, dy;
    // A rock left with shallows or land beside it, once the shallows were
    // tidied, goes: it stands whole on its own water only.
    for (y = 0; y < gen->ch; y++)
        for (x = 0; x < gen->cw; x++)
        {
            bool8 whole = TRUE;
            if (!SeaRockU(gen, x, y))
                continue;
            for (dy = -1; dy <= 1; dy++)
                for (dx = -1; dx <= 1; dx++)
                    if (InBounds(gen, x + dx, y + dy) && Cell(gen, x + dx, y + dy) != Cell(gen, x, y))
                        whole = FALSE;
            if (!whole || (Cell(gen, x, y) != C_SEA && Cell(gen, x, y) != C_FORD))
                gen->keep[(y * gen->cw + x) >> 3] &= ~(1 << ((y * gen->cw + x) & 7));
        }
    for (y = 0; y < gen->ch; y++)
        for (x = 0; x < gen->cw; x++)
        {
            bool8 clear = TRUE;
            if (!BitU(gen->river, gen, x, y))
                continue;
            for (dy = -1; dy <= 1; dy++)
                for (dx = -1; dx <= 1; dx++)
                    if (InBounds(gen, x + dx, y + dy) && Cell(gen, x + dx, y + dy) != C_SEA)
                        clear = FALSE;
            if (!clear || Cell(gen, x, y) != C_SEA)
                gen->river[(y * gen->cw + x) >> 3] &= ~(1 << ((y * gen->cw + x) & 7));
        }
    SeaDropSlivers(gen, 10);
}

// Pieces of the stream too small to read as one (fewer than min units, or
// a unit across) go back to plain sea.
static void SeaDropSlivers(struct Gen *gen, int min)
{
    u8 *mark = gen->cover; // painted later
    u16 *queue = sBfsQueue;
    int x, y, n = gen->cw * gen->ch;
    memset(mark, 0, n);
    // A unit with no current on both sides across one axis is a sliver.
    for (y = 0; y < gen->ch; y++)
        for (x = 0; x < gen->cw; x++)
            if (BitU(gen->river, gen, x, y)
             && ((!BitU(gen->river, gen, x - 1, y) && !BitU(gen->river, gen, x + 1, y))
              || (!BitU(gen->river, gen, x, y - 1) && !BitU(gen->river, gen, x, y + 1))))
                mark[y * gen->cw + x] = 2;
    for (y = 0; y < n; y++)
        if (mark[y] == 2)
            gen->river[y >> 3] &= ~(1 << (y & 7));
    memset(mark, 0, n);
    for (y = 0; y < gen->ch; y++)
    {
        for (x = 0; x < gen->cw; x++)
        {
            int head = 0, tail = 0, k;
            if (!BitU(gen->river, gen, x, y) || mark[y * gen->cw + x])
                continue;
            mark[y * gen->cw + x] = 1;
            queue[tail++] = PACK(x, y);
            while (head < tail)
            {
                int cur = queue[head++];
                u8 d;
                for (d = 0; d < 4; d++)
                {
                    int nx = PX(cur) + sDirDX[d], ny = PY(cur) + sDirDY[d];
                    if (!InBounds(gen, nx, ny) || !BitU(gen->river, gen, nx, ny) || mark[ny * gen->cw + nx])
                        continue;
                    mark[ny * gen->cw + nx] = 1;
                    queue[tail++] = PACK(nx, ny);
                }
            }
            if (tail < min)
                for (k = 0; k < tail; k++)
                {
                    int i = PY(queue[k]) * gen->cw + PX(queue[k]);
                    gen->river[i >> 3] &= ~(1 << (i & 7));
                }
        }
    }
    memset(mark, 0, n);
}

// Rocks in broken lines along the current's sides, as on Route 133, with
// gaps to come and go by, and a rock or two in the stream that stops the
// ride. Returns how many stand.
static int SeaRockLines(struct Gen *gen)
{
    int x, y, n = 0, run = 0, gap = 0;
    for (y = 1; y < gen->ch - 1; y++)
    {
        for (x = 1; x < gen->cw - 1; x++)
        {
            u8 d;
            bool8 side = FALSE;
            if (Cell(gen, x, y) != C_SEA || BitU(gen->river, gen, x, y))
                continue;
            for (d = 0; d < 4; d++)
            {
                u8 k = SeaCurrentDir(gen, x + sDirDX[d], y + sDirDY[d]), k2 = SeaCurrentDir(gen, x + 2 * sDirDX[d], y + 2 * sDirDY[d]);
                side |= (k == RDIR_EAST || k == RDIR_WEST) || (k == RDIR_NONE && (k2 == RDIR_NORTH || k2 == RDIR_SOUTH));
            }
            if (!side)
                continue;
            if (gap > 0)
            {
                gap--;
                continue;
            }
            if (run == 0)
                run = 2 + RAND(3);
            if (SeaAddRock(gen, x, y))
                n++;
            if (--run == 0)
                gap = 1 + RAND(2);
        }
    }
    // In the stream itself.
    for (y = 0; y < 40 && n < 60; y++)
    {
        int ux = 1 + RAND(gen->cw - 2), uy = 1 + RAND(gen->ch - 2);
        if (!BitU(gen->river, gen, ux, uy) || RAND(4))
            continue;
        gen->river[(uy * gen->cw + ux) >> 3] &= ~(1 << ((uy * gen->cw + ux) & 7));
        if (SeaAddRock(gen, ux, uy))
            n++;
        else
            SetBitU(gen->river, gen, ux, uy);
    }
    return n;
}

// Rocks off the open edges of the map (no place beyond) in clusters and
// short reefs, as Route 109's and 124's lie, with open stretches between,
// and now and then a reef out in the open water.
static void SeaEdgeRocks(struct Gen *gen, int inset)
{
    u8 d;
    int k, n;
    for (d = 0; d < 4; d++)
    {
        int len = EdgeLength(gen, d);
        if (!OpenSeaSide(gen->r, gen->p, d) || CHANCE(20))
            continue;
        n = len / 7 + RAND(2);
        for (k = 0; k < n; k++)
        {
            int x, y, size = 1 + RAND(4), j;
            EdgeCell(gen, d, 2 + RAND(len - 4), inset + RAND(3), &x, &y);
            // A reef grows a unit at a time, mostly along the edge.
            for (j = 0; j < size; j++)
            {
                u8 step = (d == RDIR_NORTH || d == RDIR_SOUTH) ? (RAND(3) ? RDIR_EAST : RDIR_SOUTH) : (RAND(3) ? RDIR_SOUTH : RDIR_EAST);
                SeaAddRock(gen, x, y);
                if (RAND(2))
                    step = Opposite(step);
                x += sDirDX[step];
                y += sDirDY[step];
            }
        }
    }
    // Out in the open water.
    for (k = RAND(3); k > 0; k--)
    {
        int x = 3 + RAND(gen->cw - 6), y = 3 + RAND(gen->ch - 6), j;
        if (!SeaClearAround(gen, x, y, 2))
            continue;
        for (j = 1 + RAND(3); j > 0; j--)
        {
            u8 step = RAND(4);
            SeaAddRock(gen, x, y);
            x += sDirDX[step];
            y += sDirDY[step];
        }
    }
}

// Spots for islands and sandbars: out on open sea, spread along the way.
static void SeaSpots(struct Gen *gen, int ax, int ay, int bx, int by, int n, int *ix, int *iy)
{
    int i;
    if (bx < 0)
    {
        bx = gen->cw - 1 - ax;
        by = gen->ch - 1 - ay;
    }
    for (i = 0; i < n; i++)
    {
        int t = (i + 1) * 100 / (n + 1);
        ix[i] = ax + (bx - ax) * t / 100 + (int)RAND(9) - 4;
        iy[i] = ay + (by - ay) * t / 100 + (int)RAND(9) - 4;
        if (ix[i] < 3) ix[i] = 3;
        if (iy[i] < 3) iy[i] = 3;
        if (ix[i] > gen->cw - 4) ix[i] = gen->cw - 4;
        if (iy[i] > gen->ch - 4) iy[i] = gen->ch - 4;
    }
}

// Route 132 to 134: a stream of current through the middle, lined with
// rocks, calm water each side to come back by, a few shoals of shallows (a
// sandbar on some) and sandy islets, the stream parting round those in it.
static bool8 SeaLayMaze(struct Gen *gen, u8 inDir, u8 outDir, int ax, int ay, int bx, int by, int *ix, int *iy, int *nIsl)
{
    int i, n, tries, hw = 2 + RAND(2), rocks;
    int tx = bx, ty = by;
    if (outDir == RDIR_NONE)
    {
        // A dead end: the stream runs out toward the far side.
        tx = inDir == RDIR_WEST ? gen->cw - 4 : inDir == RDIR_EAST ? 3 : ax;
        ty = inDir == RDIR_NORTH ? gen->ch - 4 : inDir == RDIR_SOUTH ? 3 : ay;
    }
    // Shoals and islets first, spread along the way, off to either side of
    // where the stream will run.
    n = 2 + RAND(3);
    SeaSpots(gen, ax, ay, tx, ty, n, ix, iy);
    for (i = 0; i < n; i++)
    {
        int off = (i & 1) ? hw + 4 : -(hw + 4);
        if (inDir == RDIR_WEST || inDir == RDIR_EAST)
            iy[i] = (ay + gen->ch / 2) / 2 + off;
        else
            ix[i] = (ax + gen->cw / 2) / 2 + off;
    }
    *nIsl = 0;
    for (i = 0; i < n; i++)
    {
        for (tries = 0; tries < 20; tries++)
        {
            int cx = tries < 10 ? ix[i] + (int)RAND(7) - 3 : 2 + RAND(gen->cw - 4);
            int cy = tries < 10 ? iy[i] + (int)RAND(7) - 3 : 2 + RAND(gen->ch - 4);
            bool8 bar = CHANCE(60);
            if (!InBounds(gen, cx, cy) || Abs(cx - ax) + Abs(cy - ay) < 5 || (outDir != RDIR_NONE && Abs(cx - bx) + Abs(cy - by) < 5))
                continue;
            // A shoal (a sandbar on some), or a sandy islet in the sea.
            if (i % 2 == 0 ? !SeaSandbar(gen, cx, cy, 2, 1 + RAND(2), bar, FALSE) : !SeaIslet(gen, cx, cy))
                continue;
            ix[*nIsl] = cx;
            iy[*nIsl] = cy;
            (*nIsl)++;
            break;
        }
    }
    SeaLayCurrent(gen, ax, ay, tx, ty, inDir, CHANCE(60), hw);
    if (outDir == Opposite(inDir) && CHANCE(60))
        SeaLayReturnStream(gen, ax, ay, tx, ty, inDir, hw > 2 ? 2 : hw);
    SeaDropSlivers(gen, 10);
    rocks = SeaRockLines(gen);
#if defined(ROGUE_HOST) && defined(SEA_DEBUG)
    { int q, nc = 0; for (q = 0; q < gen->cw * gen->ch; q++) nc += (gen->river[q >> 3] >> (q & 7)) & 1;
      fprintf(stderr, "maze rocks=%d current=%d isl=%d cw=%d ch=%d a=%d,%d b=%d,%d\n", rocks, nc, *nIsl, gen->cw, gen->ch, ax, ay, bx, by); }
#endif
    if (rocks < 6)
        return FALSE;
    SeaEdgeRocks(gen, 1);
    // A rock or two on the flats, as on Route 132's.
    for (i = 0; i < 30; i++)
    {
        int x = 1 + RAND(gen->cw - 2), y = 1 + RAND(gen->ch - 2);
        if (Cell(gen, x, y) == C_FORD && !HasNeighbor(gen, x, y, C_OPEN, TRUE) && !HasNeighbor(gen, x, y, C_SEA, TRUE) && CHANCE(40))
            SeaAddRock(gen, x, y);
    }
    return TRUE;
}

// The open sea's way to an island: a sandy islet or two on the way out, and
// rocks along the open edges.
static bool8 SeaLayIslets(struct Gen *gen, int ax, int ay, int *ix, int *iy, int *nIsl)
{
    int i, tries;
    *nIsl = 0;
    SeaEdgeRocks(gen, 1);
    for (i = 0; i < 2; i++)
        for (tries = 0; tries < 20; tries++)
        {
            int cx = 3 + RAND(gen->cw - 6), cy = 3 + RAND(gen->ch - 6);
            if (Abs(cx - ax) + Abs(cy - ay) < 6 || !SeaIslet(gen, cx, cy))
                continue;
            ix[*nIsl] = cx;
            iy[*nIsl] = cy;
            (*nIsl)++;
            break;
        }
    return TRUE;
}

// Route 109 and 126: sandbars ringed by shallows strung out along the way,
// flats of shallows between, deep channels to surf across, rocks along the
// open edges.
static bool8 SeaLayShallows(struct Gen *gen, u8 inDir, u8 outDir, int ax, int ay, int bx, int by, int *ix, int *iy, int *nIsl)
{
    int i, n = 4 + RAND(3), tries;
    SeaSpots(gen, ax, ay, bx, by, n, ix, iy);
    *nIsl = 0;
    for (i = 0; i < n; i++)
    {
        for (tries = 0; tries < 40; tries++)
        {
            int cx = tries < 14 ? ix[i] + (int)RAND(5) - 2 : 2 + RAND(gen->cw - 4);
            int cy = tries < 14 ? iy[i] + (int)RAND(5) - 2 : 2 + RAND(gen->ch - 4);
            int rx = tries < 10 ? 4 + RAND(3) : tries < 25 ? 3 + RAND(2) : 2 + RAND(2), ry = tries < 10 ? 2 + RAND(2) : 1 + RAND(2);
            if (!InBounds(gen, cx, cy) || (Abs(cx - ax) + Abs(cy - ay) < 6) || (bx >= 0 && Abs(cx - bx) + Abs(cy - by) < 6))
                continue;
            if (!SeaSandbar(gen, cx, cy, rx, ry, FALSE, TRUE))
                continue;
            ix[*nIsl] = cx;
            iy[*nIsl] = cy;
            (*nIsl)++;
            break;
        }
    }
    // Shoals that ran together make one bank; the sandbars lie in their
    // middles (most of them; a flat or two stays shallows alone).
    SeaSandCores(gen, 85);
    // Rocks along the open edges, where the shoals left room, and a rock or
    // two on the flats.
    SeaEdgeRocks(gen, 1);
    for (i = 0; i < 30; i++)
    {
        int x = 1 + RAND(gen->cw - 2), y = 1 + RAND(gen->ch - 2);
        if (Cell(gen, x, y) == C_FORD && !HasNeighbor(gen, x, y, C_OPEN, TRUE) && !HasNeighbor(gen, x, y, C_SEA, TRUE) && CHANCE(40))
            SeaAddRock(gen, x, y);
    }
    return *nIsl >= 1;
}

// ---------------------------------------------------------------------------
// People and things on the new kinds

// A spot for a swimmer: open water out of the currents, with water ahead
// to look across (three blocks at least), not by a landing.
static bool8 SeaSwimSpot(struct Gen *gen, int fx, int fy, u8 *facing)
{
    u8 start = RAND(4), k;
    int ux = fx >> 1, uy = fy >> 1;
    if (!InMap(gen, fx, fy) || Cell(gen, ux, uy) != C_SEA || SeaRockU(gen, ux, uy) || BittenF(gen, fx, fy, C_SEA)
     || SeaCurrentF(gen, fx, fy) != RDIR_NONE || IsOccupied(gen, fx, fy))
        return FALSE;
    for (k = 0; k < 4; k++)
    {
        u8 d = (start + k) & 3;
        int j;
        bool8 ok = TRUE;
        for (j = 1; j <= 3 && ok; j++)
        {
            int nx = fx + sDirDX[d] * j, ny = fy + sDirDY[d] * j;
            ok = InMap(gen, nx, ny) && Cell(gen, nx >> 1, ny >> 1) == C_SEA && !SeaRockU(gen, nx >> 1, ny >> 1)
              && !BittenF(gen, nx, ny, C_SEA);
        }
        if (ok)
        {
            *facing = d;
            return TRUE;
        }
    }
    return FALSE;
}

// A spot on a sandbar or islet (sand or shallows), looking out over the
// water if it can.
static bool8 SeaBarSpot(struct Gen *gen, int fx, int fy, int ax, int ay, int bx, int by, u8 *facing)
{
    int ux = fx >> 1, uy = fy >> 1;
    u8 c, start = RAND(4), k;
    if (!InMap(gen, fx, fy) || IsOccupied(gen, fx, fy) || SeaRockU(gen, ux, uy))
        return FALSE;
    c = Cell(gen, ux, uy);
    if ((c != C_OPEN && c != C_FORD) || SeaFordBittenF(gen, fx, fy))
        return FALSE;
    if (Abs(ux - ax) + Abs(uy - ay) <= 4 || (bx >= 0 && Abs(ux - bx) + Abs(uy - by) <= 4))
        return FALSE;
    *facing = start;
    for (k = 0; k < 4; k++)
    {
        u8 d = (start + k) & 3;
        if (IsSeaF(gen, fx + sDirDX[d], fy + sDirDY[d]) || IsSeaF(gen, fx + sDirDX[d] * 2, fy + sDirDY[d] * 2))
        {
            *facing = d;
            break;
        }
    }
    return TRUE;
}

// From the block beside each item and from each landing, both landings
// (or the way in, at a dead end) can be reached, and every islet's sand.
static bool8 SeaAllReach(struct Gen *gen, int ax, int ay, int bx, int by)
{
    u8 *seen = gen->free, *back = gen->onWalk;
    int i, x, y;
    SeaReach(gen, ax * 2, ay * 2, seen);
    if (bx >= 0)
    {
        if (!SeaSeen(gen, seen, bx * 2, by * 2))
            return FALSE;
        SeaReach(gen, bx * 2, by * 2, back);
        if (!SeaSeen(gen, back, ax * 2, ay * 2))
            return FALSE;
    }
    for (y = 0; y < gen->ch; y++)
        for (x = 0; x < gen->cw; x++)
            if (Cell(gen, x, y) == C_OPEN && !SeaSeen(gen, seen, x * 2, y * 2) && !SeaSeen(gen, seen, x * 2 + 1, y * 2)
             && !SeaSeen(gen, seen, x * 2, y * 2 + 1) && !SeaSeen(gen, seen, x * 2 + 1, y * 2 + 1))
                return FALSE;
    for (i = 0; i < gen->m->nObjs; i++)
    {
        const struct RogueObj *o = &gen->m->objs[i];
        u8 d;
        bool8 ok = FALSE;
        for (d = 0; d < 4 && !ok; d++)
        {
            int nx = o->x + sDirDX[d], ny = o->y + sDirDY[d];
            if (!SeaSeen(gen, seen, nx, ny))
                continue;
            if (o->kind != ROBJ_ITEM)
            {
                ok = TRUE;
                break;
            }
            SeaReach(gen, nx, ny, back);
            ok = SeaSeen(gen, back, ax * 2, ay * 2);
        }
        if (!ok)
            return FALSE;
        if (o->kind == ROBJ_ITEM)
            SeaReach(gen, ax * 2, ay * 2, seen);
    }
    return TRUE;
}

#if defined(ROGUE_HOST) && defined(SEA_DEBUG)
#include <stdio.h>
#include <stdlib.h>
static void SeaDump(struct Gen *gen)
{
    int x, y;
    if (!getenv("SEADUMP"))
        return;
    for (y = 0; y < gen->ch; y++)
    {
        for (x = 0; x < gen->cw; x++)
        {
            u8 c = Cell(gen, x, y);
            char ch = c == C_SEA ? '.' : c == C_FORD ? '-' : c == C_OPEN ? 's' : c == C_ROAD ? 'R' : c == C_TREE ? 'T' : '?';
            if (SeaRockU(gen, x, y)) ch = c == C_FORD ? 'r' : 'o';
            else if (c == C_SEA && BitU(gen->river, gen, x, y)) ch = "^>v<"[SeaCurrentDir(gen, x, y) & 3];
            fputc(ch, stderr);
        }
        fputc('\n', stderr);
    }
}
#define SEAFAIL(why) do { fprintf(stderr, "seafail place=%d kind=%d attempt=%d %s\n", gen->m->place, kind, gen->attempt, why); SeaDump(gen); return FALSE; } while (0)
#else
#define SEAFAIL(why) return FALSE
#endif

static bool8 SeaFarIsland(struct Gen *gen, u8 inDir, int *cx, int *cy);
static bool8 SeaHiddenTrainer(struct Gen *gen, int cx, int cy, u8 inDir, u8 slot);

// SeaIslandGround's units seen are bits in gen->free. (Before v1.1.2 they were a byte
// each, past the end of gen->free on a big map: on the GBA that cleared
// gen->attempt, so the sea's fallbacks for later tries never came there and
// the game's maps could differ from gen_host's.)
#define ISLE_MARK(i) ((mark[(i) >> 3] >> ((i) & 7)) & 1)
#define ISLE_SET(i) (mark[(i) >> 3] |= 1 << ((i) & 7))

// v1.1 round 2: an island of 12 units or more (land the shore does not
// reach, not a sandbar in shallows) has ground inside its rim of sand, as Hoenn's larger islands do:
// grass on the units with land all round them, a tree or two standing on
// that grass and a patch of tall grass. TRUE if one had any.
static bool8 SeaIslandGround(struct Gen *gen)
{
    u8 *mark = gen->free; // a bit per unit, set once seen
    u16 *queue = sBfsQueue;
    int x, y, k;
    bool8 any = FALSE;
    memset(mark, 0, sizeof(gen->free));
    for (y = 0; y < gen->ch; y++)
    {
        for (x = 0; x < gen->cw; x++)
        {
            int head = 0, tail = 0, inner = 0, trees;
            bool8 shore = FALSE;
            u8 c = Cell(gen, x, y);
            if (ISLE_MARK(y * gen->cw + x) || (c != C_OPEN && c != C_TREE))
                continue;
            ISLE_SET(y * gen->cw + x);
            queue[tail++] = PACK(x, y);
            while (head < tail)
            {
                int cur = queue[head++];
                u8 d, t;
                shore |= IsEdge(gen, PX(cur), PY(cur)) || Cell(gen, PX(cur), PY(cur)) == C_ROAD;
                for (d = 0; d < 4; d++)
                {
                    int nx = PX(cur) + sDirDX[d], ny = PY(cur) + sDirDY[d];
                    if (!InBounds(gen, nx, ny) || ISLE_MARK(ny * gen->cw + nx))
                        continue;
                    t = Cell(gen, nx, ny);
                    // (A sandbar in its shallows stays bare sand.)
                    shore |= t == C_FORD;
                    if (t != C_OPEN && t != C_TREE && t != C_ROAD)
                        continue;
                    ISLE_SET(ny * gen->cw + nx);
                    queue[tail++] = PACK(nx, ny);
                }
            }
            if (shore || tail < 12)
                continue;
            // Its inner units: land all round them.
            for (k = 0; k < tail; k++)
            {
                int ux = PX(queue[k]), uy = PY(queue[k]), dx, dy, all = 1;
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++)
                    {
                        u8 t = Cell(gen, ux + dx, uy + dy);
                        all &= t == C_OPEN || t == C_TREE;
                    }
                if (all && Cell(gen, ux, uy) == C_OPEN)
                {
                    PaintUnit(gen, ux, uy, V_NONE);
                    inner++;
                }
            }
            if (!inner)
                continue;
            any = TRUE;
            // A tree on grass with grass all round it; then tall grass.
            for (trees = inner >= 6 ? 2 : 1, k = 0; k < tail && trees; k++)
            {
                int ux = PX(queue[(k * 7) % tail]), uy = PY(queue[(k * 7) % tail]), dx, dy, all = 1;
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++)
                        all &= Cell(gen, ux + dx, uy + dy) == C_OPEN && Cover(gen, (ux + dx) * 2, (uy + dy) * 2) == V_NONE;
                if (all && CHANCE(60))
                {
                    SetCell(gen, ux, uy, C_TREE);
                    trees--;
                }
            }
            for (k = 0; k < tail; k++)
            {
                int ux = PX(queue[k]), uy = PY(queue[k]);
                if (Cell(gen, ux, uy) == C_OPEN && Cover(gen, ux * 2, uy * 2) == V_NONE && CHANCE(55))
                    PaintUnit(gen, ux, uy, V_TALL);
            }
        }
    }
    memset(mark, 0, sizeof(gen->free));
    return any;
}

#define SEA_ITEM_TRIES 200 // a find's random tries at the usual distance from others

// Can a find lie at (fx, fy)? On the sand, beside the water the landing
// reaches, not crowded, not seen from the next map, and only by Surf.
static bool8 SeaFindSpot(struct Gen *gen, int fx, int fy, int ax, int ay, int bx, int by, u8 *face)
{
    return SeaBarSpot(gen, fx, fy, ax, ay, bx, by, face) && SeaSeenBeside(gen, gen->free, fx, fy)
        && !IsCrowded(gen, fx, fy) && !SeenFromNextDoor(gen, fx, fy) && !Reached(gen, fx >> 1, fy >> 1);
}

static bool8 SeaGenKind(struct Gen *gen, u8 kind, u8 inDir, u8 outDir)
{
    const struct RoguePlace *p = gen->p;
    int ax, ay, bx, by, ix[9], iy[9], nIsl = 0, i, tries, placedT = 0, placedI = 0, fcx = -1, fcy = -1;
    bool8 hidden = outDir != RDIR_NONE;
    u8 face;

    SeaLayBase(gen, inDir, outDir, &ax, &ay, &bx, &by);
    // A side route's island at the far end, first (it is what the route is
    // for), then the kind's own layout round it.
    if (outDir == RDIR_NONE && !SeaFarIsland(gen, inDir, &fcx, &fcy))
        SEAFAIL("island");
    if (kind == SEA_MAZE ? !SeaLayMaze(gen, inDir, outDir, ax, ay, bx, by, ix, iy, &nIsl)
       : kind == SEA_SHALLOWS ? !SeaLayShallows(gen, inDir, outDir, ax, ay, bx, by, ix, iy, &nIsl)
       : !SeaLayIslets(gen, ax, ay, ix, iy, &nIsl))
        SEAFAIL("layout");
    if (outDir == RDIR_NONE && nIsl < 8)
    {
        ix[nIsl] = fcx;
        iy[nIsl] = fcy;
        nIsl++;
    }
    SeaTidyShallows(gen);
    if (!SeaNeedSurf(gen, ax, ay, outDir != RDIR_NONE ? bx : fcx, outDir != RDIR_NONE ? by : fcy))
        SEAFAIL("surf");
    SeaTidyShallows(gen);
    SeaCurrentsClear(gen);
    if (SeaShallowsAshore(gen))
        SEAFAIL("ashore");
    if (LandWalkMiddle(gen, ax, ay, outDir != RDIR_NONE ? bx : fcx, outDir != RDIR_NONE ? by : fcy) >= 0)
        SEAFAIL("walk");
    for (i = 0; i < gen->cw * gen->ch; i++)
        if (gen->g[i] == C_OPEN || gen->g[i] == C_ROAD)
            PaintUnit(gen, i % gen->cw, i / gen->cw, V_GROUND);
    // Larger islands have ground of their own, grass and trees inside a rim
    // of sand; a side route's island its grass, where its rare POKeMON lives.
    if (!SeaIslandGround(gen) && outDir == RDIR_NONE)
        PaintBlob(gen, fcx * 2 + 1, fcy * 2 + 1, 2 + RAND(2), V_TALL, TRUE);
    PaintSpecialGround(gen);
    if (!SeaAllReach(gen, ax, ay, bx, by))
        SEAFAIL("reach");

    // Trainers: swimmers out on the water, the others on the sandbars and
    // islets looking out over it; then the items on the sand.
    for (i = 0; i < p->nTrainers; i++)
    {
        bool8 swims = RogueGen_TrainerSwims(gen->r, gen->m->place, i);
        // An island's trainer behind its trees.
        if (!swims && !hidden && SeaHiddenTrainer(gen, fcx, fcy, inDir, i))
        {
            hidden = TRUE;
            placedT++;
            continue;
        }
        for (tries = 0; tries < 300; tries++)
        {
            int fx, fy;
            if (swims || nIsl == 0)
            {
                fx = 2 + RAND(gen->m->w - 4);
                fy = 2 + RAND(gen->m->h - 4);
                if (!SeaSwimSpot(gen, fx, fy, &face))
                    continue;
                // Near a rock line or a sandbar at first, where Hoenn's
                // swimmers tread water.
                if (tries < 150 && !HasNeighbor(gen, fx >> 1, fy >> 1, C_FORD, TRUE) && !HasNeighbor(gen, fx >> 1, fy >> 1, C_OPEN, TRUE)
                 && !(SeaRockU(gen, (fx >> 1) + 1, fy >> 1) || SeaRockU(gen, (fx >> 1) - 1, fy >> 1)
                   || SeaRockU(gen, fx >> 1, (fy >> 1) + 1) || SeaRockU(gen, fx >> 1, (fy >> 1) - 1)))
                    continue;
            }
            else
            {
                int k = RAND(nIsl);
                fx = tries < 150 ? ix[k] * 2 + (int)RAND(6) - 2 : 2 + RAND(gen->m->w - 4);
                fy = tries < 150 ? iy[k] * 2 + (int)RAND(6) - 2 : 2 + RAND(gen->m->h - 4);
                if (!SeaBarSpot(gen, fx, fy, ax, ay, bx, by, &face))
                    continue;
            }
            if (!SeaSeen(gen, gen->free, fx, fy) && !SeaSeenBeside(gen, gen->free, fx, fy))
                continue;
            if (IsCrowded(gen, fx, fy) || NearTrainer(gen, fx, fy) || SeenFromNextDoor(gen, fx, fy))
                continue;
            AddObj(gen, ROBJ_TRAINER, fx, fy, face, placedT++, 3);
            break;
        }
    }
    // (Every find needs Surf: none where the way in's beach leads on foot.)
    // A maze's shoals are small, so its finds often found no room three
    // blocks apart in 200 tries: up to 50 tries of the whole route failed and
    // it fell back to the open sea, which froze the game for most of a minute
    // when it was made on the spot. Now such a find looks at every spot in
    // turn, two blocks apart from the others and then one (v1.1.2; a find
    // that has room in its 200 tries is as before).
    FloodReachable(gen, ax, ay);
    for (i = 0; i < p->nItems && nIsl; i++)
    {
        int n = gen->m->w * gen->m->h, start = 0, j, fx = 0, fy = 0;
        gen->crowdRadius = 0; // (the last find's scan may have set it)
        for (tries = 0; tries < SEA_ITEM_TRIES + 2; tries++)
        {
            if (tries < SEA_ITEM_TRIES)
            {
                // (An island's finds lie on it.)
                int k = outDir == RDIR_NONE && tries < 120 ? nIsl - 1 : RAND(nIsl);
                fx = tries < 100 ? ix[k] * 2 + (int)RAND(6) - 2 : 2 + RAND(gen->m->w - 4);
                fy = tries < 100 ? iy[k] * 2 + (int)RAND(6) - 2 : 2 + RAND(gen->m->h - 4);
                if (SeaFindSpot(gen, fx, fy, ax, ay, bx, by, &face))
                    break;
                continue;
            }
            gen->crowdRadius = tries == SEA_ITEM_TRIES ? 2 : 1;
            if (tries == SEA_ITEM_TRIES)
                start = RAND(n);
            for (j = 0; j < n; j++)
            {
                fx = (start + j) % n % gen->m->w;
                fy = (start + j) % n / gen->m->w;
                if (fx >= 2 && fy >= 2 && fx < gen->m->w - 2 && fy < gen->m->h - 2 && SeaFindSpot(gen, fx, fy, ax, ay, bx, by, &face))
                    break;
            }
            if (j < n)
                break;
        }
        if (tries < SEA_ITEM_TRIES + 2)
            AddObj(gen, ROBJ_ITEM, fx, fy, RDIR_SOUTH, placedI++, 0);
    }
    gen->crowdRadius = 0;
    if (placedT < p->nTrainers)
        SEAFAIL("trainers");
    if (placedI < p->nItems)
        SEAFAIL("items");
    if (!SeaAllReach(gen, ax, ay, bx, by))
        SEAFAIL("reach2");
    TidyGround(gen);
    Render(gen);
    SeaRenderKind(gen);
    {
        // A sign at the way in, beside the landing, off the way (from a
        // raft village, the landing at the far end: v1.1 towns2).
        int sx = ax, sy = ay, fx, fy, k;
        u8 facing;
        if (SeaToRaft(gen, inDir) && bx >= 0)
        {
            sx = bx;
            sy = by;
        }
        fx = sx * 2 + 1;
        fy = sy * 2 + 1;
        FloodReachable(gen, sx, sy);
        for (k = 0; k < 4; k++)
            if (FindSignSpot(gen, sx, sy, &fx, &fy, &facing))
                break;
        if (k == 4)
        {
            fx = sx * 2 + 1;
            fy = sy * 2 + 1;
        }
        if (!IsOccupied(gen, fx, fy))
        {
            AddSign(gen, fx, fy, RSIGN_ROUTE);
            RenderSigns(gen);
        }
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
// Side routes out to islands
//
// Once Surf is won, a stretch of the coast may have a side route out over the
// sea to an island, as Hoenn's Routes 105 to 108 lead to islets off the way
// and Route 115's beach looks out on one: the island at its far end has the
// route's finds, a trainer behind its trees and a patch of grass where a rare
// POKeMON lives (RogueGen_IslandRare). About SEA_SIDE_PCT regions have one or
// two, picked on a stream of their own after the other side paths, so a
// region without them is planned as before. Its way in needs Surf as any sea
// route's (LinkKeys).

#define SEA_SIDE_PCT 60

static void SeaAddIslands(struct RogueRegion *r, const u8 *spinePlace)
{
    u32 rng = RogueRng_Hash(r->seed, 0x5EA8);
    int want, got = 0, tries, lo = KeyStage(r, ROBST_SURF) - 1;
    if (r->seed == ROGUE_RUN0_SEED || SeaStep() < 0 || RogueRng_Range(&rng, 100) >= SEA_SIDE_PCT)
        return;
    want = 1 + (RogueRng_Range(&rng, 100) < 35);
    for (tries = 0; tries < 80 && got < want && r->nPlaces + 1 <= ROGUE_MAX_PLACES; tries++)
    {
        int i = RogueRng_Range(&rng, SPINE_LEN), at = spinePlace[i], x, y, s, n = r->nPlaces, k;
        u8 d = RogueRng_Range(&rng, 4);
        bool8 alone = TRUE;
        if (sSpineBadges[i] < lo || !MayHangOff(r, at, d))
            continue;
        // (v1.1 towns2: not off a raft village, whose rafts keep to its
        // two sea routes.)
        if (r->places[at].kind == RPLACE_TOWN && r->places[at].theme == RTHEME_RAFT)
            continue;
        x = r->places[at].gridX + sDirDX[d];
        y = r->places[at].gridY + sDirDY[d];
        if (!CellIs(x, y, CELL_SEA))
            continue;
        // Out on its own: no other place beside it (its sea would meet theirs
        // with no way between).
        for (k = 0; k < 4; k++)
        {
            int nx = x + sDirDX[k], ny = y + sDirDY[k];
            if ((nx != r->places[at].gridX || ny != r->places[at].gridY) && nx >= 0 && ny >= 0 && nx < RGRID_W && ny < RGRID_H
             && sOcc[ny][nx])
                alone = FALSE;
        }
        if (!alone)
            continue;
        s = NewPlace(r, RPLACE_ROUTE, RROUTE_SEA, x, y);
        r->places[s].side = TRUE;
        r->places[s].branch = RBRANCH_SIDE;
        r->places[s].guardDir = Opposite(d);
        sOcc[y][x] = s + 1;
        Link(r, at, s, d);
        if (!GatingHolds(r, spinePlace))
        {
            DropPlaces(r, n);
            continue;
        }
        got++;
    }
}

// A side route's island, at the far end from the way in: sand, a grove and
// grass. Its middle unit in (*cx, *cy).
static bool8 SeaFarIsland(struct Gen *gen, u8 inDir, int *cx, int *cy)
{
    int tries;
    for (tries = 0; tries < 20; tries++)
    {
        int rx = tries < 10 ? 3 + RAND(2) : 2 + RAND(2), ry = tries < 10 ? 2 + RAND(2) : 2, x, y;
        bool8 ok = TRUE;
        switch (inDir)
        {
        case RDIR_WEST:  x = gen->cw - 4 - rx; y = 4 + RAND(gen->ch - 8); break;
        case RDIR_EAST:  x = 3 + rx; y = 4 + RAND(gen->ch - 8); break;
        case RDIR_NORTH: x = 4 + RAND(gen->cw - 8); y = gen->ch - 4 - ry; break;
        default:         x = 4 + RAND(gen->cw - 8); y = 3 + ry; break;
        }
        for (*cy = y - ry - 2; *cy <= y + ry + 2 && ok; (*cy)++)
            for (*cx = x - rx - 2; *cx <= x + rx + 2 && ok; (*cx)++)
                if (!InBounds(gen, *cx, *cy) || Cell(gen, *cx, *cy) != C_SEA)
                    ok = FALSE;
        if (!ok)
            continue;
        SeaIsland(gen, x, y, rx, ry);
        *cx = x;
        *cy = y;
        return Cell(gen, x, y) == C_OPEN;
    }
    return FALSE;
}

// A trainer on the island, out of sight from the water by a tree between
// them and the way in, looking along the island (Hoenn's islands keep one
// in the shade, as Route 108's).
static bool8 SeaHiddenTrainer(struct Gen *gen, int cx, int cy, u8 inDir, u8 slot)
{
    int tries;
    for (tries = 0; tries < 40; tries++)
    {
        int ux = cx + (int)RAND(5) - 2, uy = cy + (int)RAND(3) - 1;
        // The tree stands between the trainer and the way in.
        int tx = ux + (inDir == RDIR_WEST ? -1 : inDir == RDIR_EAST ? 1 : 0);
        int ty = uy + (inDir == RDIR_NORTH ? -1 : inDir == RDIR_SOUTH ? 1 : 0);
        int fx = ux * 2 + RAND(2), fy = uy * 2 + RAND(2), k;
        u8 face = (inDir + 1 + 2 * RAND(2)) & 3;
        bool8 ok = TRUE;
        if (Cell(gen, ux, uy) != C_OPEN || Cell(gen, tx, ty) != C_OPEN)
            continue;
        // The tree on sand all round, so it stands whole on the island.
        for (k = 0; k < 8 && ok; k++)
        {
            static const s8 sX[8] = { -1, 0, 1, 1, 1, 0, -1, -1 }, sY[8] = { -1, -1, -1, 0, 1, 1, 1, 0 };
            if (Cell(gen, tx + sX[k], ty + sY[k]) != C_OPEN)
                ok = FALSE;
        }
        if (!ok || IsOccupied(gen, fx, fy) || IsCrowded(gen, fx, fy) || NearTrainer(gen, fx, fy) || SeenFromNextDoor(gen, fx, fy))
            continue;
        SetCell(gen, tx, ty, C_TREE);
        AddObj(gen, ROBJ_TRAINER, fx, fy, face, slot, 3);
        return TRUE;
    }
    return FALSE;
}

// An island's rare POKeMON, in its grass in the 10% slot (rogue.c), as
// Route 119 has Tropius and the deep sea Relicanth: a POKeMON rare in
// Hoenn and Kanto's seas, by a hash of the seed and the island. SPECIES_NONE
// for any other place.
u16 RogueGen_IslandRare(const struct RogueRegion *r, u8 place)
{
    // Early islands (Surf can come with the first badge) have the milder
    // ones; from the fourth tier the strong ones too.
    static const u16 sIslandRares[] =
    {
        SPECIES_CORSOLA, SPECIES_CHINCHOU, SPECIES_CLAMPERL, SPECIES_STARYU,
        SPECIES_LAPRAS, SPECIES_RELICANTH, SPECIES_TROPIUS, SPECIES_SKARMORY, SPECIES_MANTINE, SPECIES_DRATINI,
    };
    const struct RoguePlace *p;
    u32 n;
    if (place >= r->nPlaces)
        return SPECIES_NONE;
    p = &r->places[place];
    if (p->kind != RPLACE_ROUTE || p->type != RROUTE_SEA || !p->side)
        return SPECIES_NONE;
    n = p->tier >= 4 ? sizeof(sIslandRares) / sizeof(sIslandRares[0]) : 4;
    return sIslandRares[RogueRng_Hash(r->seed, 0x5EA9 + place) % n];
}

// ---------------------------------------------------------------------------
// Two checks a new region plan can need (v1.1 sea: the second stretch and
// the islands give regions plans no seed had before, and these turned up
// in them: seed 45's town and seed 135's hills). Each only turns down what
// check_seeds.py would fail, so no map that passed before changes.

// Blocks the player can walk to on foot from the map's ways in, everyone
// standing where they stand (the rival aside, who moves on), as
// check_seeds.py walks them. A bit per block in seen.
static void SeaFootReach(struct Gen *gen, u8 *seen)
{
    const struct RogueMap *m = gen->m;
    u16 *queue = sBfsQueue;
    int w = m->w, head = 0, tail = 0, k, i;
    memset(seen, 0, (m->w * m->h + 7) / 8);
    for (k = 0; k < 4; k++)
    {
        int x, y;
        if (gen->exitPos[k] == 0xFF)
            continue;
        EdgeBlock(gen, k, &x, &y);
        if (SeaFootF(gen, x, y) && !((seen[(y * w + x) >> 3] >> ((y * w + x) & 7)) & 1))
        {
            seen[(y * w + x) >> 3] |= 1 << ((y * w + x) & 7);
            queue[tail++ & (BFS_RING - 1)] = PACK(x, y);
        }
    }
    while (head < tail)
    {
        int cur = queue[head++ & (BFS_RING - 1)];
        YIELD();
        for (k = 0; k < 4; k++)
        {
            int nx = PX(cur) + sDirDX[k], ny = PY(cur) + sDirDY[k];
            i = ny * w + nx;
            if (!SeaFootF(gen, nx, ny) || ((seen[i >> 3] >> (i & 7)) & 1) || tail - head >= BFS_RING)
                continue;
            seen[i >> 3] |= 1 << (i & 7);
            queue[tail++ & (BFS_RING - 1)] = PACK(nx, ny);
        }
    }
}

static bool8 SeaFootF(struct Gen *gen, int x, int y)
{
    const struct RogueMap *m = gen->m;
    u16 b;
    int i;
    // A town as TownReachable walks it (the blockade's grunts and a
    // tunnel's guard leave in time).
    if (gen->p->kind == RPLACE_TOWN)
        return FootBlock(gen, x, y);
    if (!InMap(gen, x, y))
        return FALSE;
    b = m->blocks[y * m->w + x];
    if (((b >> 10) & 3) != 0 || !((b >> 12) == 0 || (b >> 12) == 3 || (b >> 12) == 4 || (b >> 12) == 15))
        return FALSE;
    // (Obstacles the badges clear, a sleeper woken, a blockade or a guard
    // that leaves count as passed, as check_seeds.py clears them.)
    for (i = 0; i < m->nObjs; i++)
    {
        u8 k = m->objs[i].kind;
        if (m->objs[i].x == x && m->objs[i].y == y && k != ROBJ_RIVAL && k != ROBJ_CUT_TREE && k != ROBJ_ROCK
         && k != ROBJ_BOULDER && k != ROBJ_SNORLAX && k != ROBJ_GRUNT && k != ROBJ_TUNNEL_GUARD)
            return FALSE;
    }
    return TRUE;
}

// A hidden item on a land route: on or beside a block reached on foot
// (SeaFootReach, worked out once into gen->onWalk by PlaceHiddenItems).
static bool8 SeaHiddenReached(struct Gen *gen, int fx, int fy)
{
    int k;
    for (k = -1; k < 4; k++)
    {
        int x = fx + (k < 0 ? 0 : sDirDX[k]), y = fy + (k < 0 ? 0 : sDirDY[k]), i = y * gen->m->w + x;
        if (InMap(gen, x, y) && ((gen->onWalk[i >> 3] >> (i & 7)) & 1))
            return TRUE;
    }
    return FALSE;
}

// A town's ways in all join on foot, everyone standing where they stand:
// two townsfolk side by side can shut the only lane between two parts of a
// town (TownReachable, flooding from every way in at once, saw each part
// reached).
static bool8 SeaTownExitsJoin(struct Gen *gen)
{
    u8 *seen = gen->onWalk;
    int k, first = -1;
    SeaFootReach(gen, seen);
    // From the first way in alone: flood again only from it.
    for (k = 0; k < 4 && first < 0; k++)
        if (gen->exitPos[k] != 0xFF)
            first = k;
    if (first < 0)
        return TRUE;
    {
        u8 saved[4];
        int x, y;
        for (k = 0; k < 4; k++)
        {
            saved[k] = gen->exitPos[k];
            if (k != first)
                gen->exitPos[k] = 0xFF;
        }
        SeaFootReach(gen, seen);
        for (k = 0; k < 4; k++)
            gen->exitPos[k] = saved[k];
        for (k = 0; k < 4; k++)
        {
            if (gen->exitPos[k] == 0xFF || k == first)
                continue;
            EdgeBlock(gen, k, &x, &y);
            if (SeaFootF(gen, x, y) && !((seen[(y * gen->m->w + x) >> 3] >> ((y * gen->m->w + x) & 7)) & 1))
                return FALSE;
        }
    }
    return TRUE;
}
