// Mountain passes (RROUTE_HILLS) and desert canyons (RROUTE_DESERT).
// Part of rogue_gen.c (included at its end; forward declared near the top).
//
// A pass climbs, as Routes 114, 115 and 112 do and as Mossdeep's hillside is
// drawn: its open ground lies on up to three levels, each a step higher to
// the north, behind a one-block rock rim along its lower sides (the pieces
// of a rock edge against grass, Hoenn's own), with stone steps where the way
// goes up and down. The rock round it rises above the highest ground it
// stands beside, in terraces of its own (RenderMountain). The levels are two
// unit bitmaps: gen->upper (level 1 and up, as a town's terrace) and
// gen->grassZone (level 2; only island towns use it otherwise). A level
// runs from the top of the map down to a row per column (a rim's
// profile), so every rim faces south, east or west, never north, which no
// Hoenn piece draws on grass.
//
// A canyon is sand between tall rock walls, with boulders on the sand
// (Route 111): its way is wide, its rock thick, and a lone rock standing on
// the sand is one of Route 111's boulders, not a tree.

#define PASS_MAX_LANES 32
#define PASS_ROCK_STEPS 2 // how many steps a pass's rock rises inside a mass
enum { PASS_SIDE, PASS_BELOW, PASS_ABOVE, PASS_DIAG }; // where a neighbour is (PassNbLevel)

// Is this unit open ground a person can stand on, for the levels?
static bool8 PassWalkU(struct Gen *gen, int x, int y)
{
    return InBounds(gen, x, y) && IsOpenU(Cell(gen, x, y));
}

// The level a tree unit stands at: its own, or that of open ground right
// below it or beside it if that is lower. A tree on the rim's line stands
// on the lower ground in a notch of the rim, its tips over the rim's foot
// as on Route 116, and the rim beside it is its side.
static u8 PassTreeLevel(struct Gen *gen, int x, int y)
{
    u8 l = TerraceLevelU(gen, x, y);
    int d;
    for (d = 0; d < 3; d++)
    {
        int nx = x + (d == 0 ? 0 : d == 1 ? -1 : 1), ny = y + (d == 0);
        if (PassWalkU(gen, nx, ny) && TerraceLevelU(gen, nx, ny) < l)
            l = TerraceLevelU(gen, nx, ny);
    }
    return l;
}

// A tree beside the road that stands lower than it would make the road a
// rim: it gives way to open ground (on the road's level, a rim itself).
static bool8 PassOpenTreeBy(struct Gen *gen, int x, int y, u8 l)
{
    int dx, dy;
    bool8 opened = FALSE;
    for (dy = 0; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
        {
            int nx = x + dx, ny = y + dy;
            if ((dx == 0 && dy == 0) || !InBounds(gen, nx, ny) || Cell(gen, nx, ny) != C_TREE)
                continue;
            if (PassTreeLevel(gen, nx, ny) >= l)
                continue;
            if (IsEdge(gen, nx, ny) || BitU(gen->keep, gen, nx, ny))
                return FALSE;
            SetCell(gen, nx, ny, C_OPEN);
            opened = TRUE;
        }
    return opened;
}

// How high a neighbour of open ground at level self stands, for drawing that
// ground's rim. level is RenderMountain's (rock units nonzero), or NULL
// before the rock is known, when every solid unit counts as a tree. Off the
// map, a building, the water: the same level (no rim). A tree above or
// only diagonally below never makes a rim (there are no rims facing north,
// and a tree's tips reach up over the ground right above it).
static u8 PassNbLevel(struct Gen *gen, const u8 *level, int x, int y, int at, u8 self)
{
    u8 c;
    if (!InBounds(gen, x, y))
        return self;
    if (level != NULL && level[y * gen->cw + x])
        return PASS_CAP + 1;
    c = Cell(gen, x, y);
    if (IsOpenU(c))
        return TerraceLevelU(gen, x, y);
    if (c == C_TREE)
        return (at == PASS_SIDE || at == PASS_BELOW) ? PassTreeLevel(gen, x, y) : self;
    return self;
}

// The edge of open ground at level self in quadrant q of unit (ux, uy), as
// MountainKey: the rim facing lower ground.
static u8 PassKey(struct Gen *gen, const u8 *level, int ux, int uy, int q, u8 self)
{
    int sx = (q & 1) ? 1 : -1, sy = (q & 2) ? 1 : -1;
    bool8 v = PassNbLevel(gen, level, ux, uy + sy, sy > 0 ? PASS_BELOW : PASS_ABOVE, self) >= self;
    bool8 h = PassNbLevel(gen, level, ux + sx, uy, PASS_SIDE, self) >= self;
    bool8 d = PassNbLevel(gen, level, ux + sx, uy + sy, PASS_DIAG, self) >= self;
    bool8 top = sy < 0, left = sx < 0;
    if (!v && !h)
        return top ? (left ? MK_TL : MK_TR) : (left ? MK_BL : MK_BR);
    if (!v)
        return top ? MK_T : MK_B;
    if (!h)
        return left ? MK_L : MK_R;
    if (!d)
        return top ? (left ? MK_NW : MK_NE) : (left ? MK_SW : MK_SE);
    return MK_C;
}

// Is block (fx, fy) of open ground part of a rim (solid), before the rock is
// known? Stone steps are not.
static bool8 PassRimBlock(struct Gen *gen, int fx, int fy)
{
    int ux = fx >> 1, uy = fy >> 1;
    u8 self;
    if (!gen->terraced || !PassWalkU(gen, ux, uy))
        return FALSE;
    self = TerraceLevelU(gen, ux, uy);
    if (self == 0)
        return FALSE;
    if (PlateauU(gen, ux, uy)) // (v1.1 vert: rogue_gen_vert.h)
        return PlateauRimBlock(gen, fx, fy);
    if ((fy & 1) && IsStairU(gen, ux, uy))
        return FALSE;
    return PassKey(gen, NULL, ux, uy, (fx & 1) | ((fy & 1) << 1), self) != MK_C;
}

// Can the path step from index i to i + 1 up or down a stone stair? A
// straight lane, one unit wide, from a unit above to one below, away from
// the openings, the obstacle's narrow way in and any gate lane. Gives the
// upper unit.
static bool8 PassLaneAt(struct Gen *gen, int i, int *ux, int *uy)
{
    int ax = gen->path[i] % gen->cw, ay = gen->path[i] / gen->cw;
    int bx = gen->path[i + 1] % gen->cw, by = gen->path[i + 1] / gen->cw;
    int x = ax, y = ay < by ? ay : by;
    if (ax != bx || Abs(ay - by) != 1 || y < 2 || y + 1 >= gen->ch - 2)
        return FALSE;
    if (Cell(gen, x, y - 1) != C_ROAD || Cell(gen, x, y + 2) != C_ROAD)
        return FALSE;
    if (Cell(gen, x - 1, y) == C_ROAD || Cell(gen, x + 1, y) == C_ROAD
     || Cell(gen, x - 1, y + 1) == C_ROAD || Cell(gen, x + 1, y + 1) == C_ROAD)
        return FALSE;
    if (IsEdge(gen, x, y) || NearExitU(gen, x, y, 2) || NearExitU(gen, x, y + 1, 2)
     || InGateLaneF(gen, x * 2, y * 2) || InGateLaneF(gen, x * 2, y * 2 + 2))
        return FALSE;
    *ux = x;
    *uy = y;
    return TRUE;
}

// May a rim run along these units of column x, rows y0 to y1? Not over the
// road, which crosses a rim only straight down a lane (checked later).
static bool8 PassSideFree(struct Gen *gen, int x, int y0, int y1)
{
    int y;
    for (y = y0; y <= y1; y++)
        if (Cell(gen, x, y) == C_ROAD)
            return FALSE;
    return TRUE;
}

// Can column x's level end at row t (-1: not raised) beside column x - dir,
// whose level ends at prev? Only the rim it makes is looked at.
static bool8 PassColumnOk(struct Gen *gen, int x, int dir, int t, int prev, const s8 *cap)
{
    if (t >= 0)
    {
        if (t < 2 || t > gen->ch - 3)
            return FALSE;
        if (cap != NULL && (cap[x] < 0 || t > cap[x] - 2))
            return FALSE;
        // Its foot: the road crosses only at a lane.
        if (Cell(gen, x, t) == C_ROAD
         && !(Cell(gen, x, t - 1) == C_ROAD && Cell(gen, x, t + 1) == C_ROAD
              && Cell(gen, x - 1, t) != C_ROAD && Cell(gen, x + 1, t) != C_ROAD))
            return FALSE;
    }
    if (t > prev)
        return PassSideFree(gen, x, prev + 1, t);
    if (t < prev)
        return PassSideFree(gen, x - dir, t + 1, prev);
    return TRUE;
}

// A rim's profile through the stair at (lx, ly), out to both sides of the
// map: mostly level, stepping a row or two now and then (never for a
// single column), or ending. cap: the profile of the level below, which
// this one keeps two units inside.
static bool8 PassProfile(struct Gen *gen, s8 *top, const s8 *cap, int lx, int ly)
{
    int dir, x, k;
    if (cap != NULL && (cap[lx] < 0 || ly > cap[lx] - 2))
        return FALSE;
    top[lx] = ly;
    for (dir = -1; dir <= 1; dir += 2)
    {
        int run = 1 + RAND(2);
        for (x = lx + dir; x >= 0 && x < gen->cw; x += dir)
        {
            int prev = top[x - dir], n = 0, s = RAND(2) ? 1 : -1;
            s8 cand[7];
            bool8 found = FALSE;
            YIELD();
            if (prev >= 0)
            {
                if (run >= 2 && CHANCE(35))
                {
                    cand[n++] = prev + s;
                    cand[n++] = prev;
                }
                else
                {
                    cand[n++] = prev;
                    cand[n++] = prev + s;
                }
                cand[n++] = prev - s;
                cand[n++] = prev + 2 * s;
                cand[n++] = prev - 2 * s;
            }
            cand[n++] = -1;
            for (k = 0; k < n && !found; k++)
            {
                if (!PassColumnOk(gen, x, dir, cand[k], prev, cap))
                    continue;
                run = cand[k] == prev ? run + 1 : 1;
                top[x] = cand[k];
                found = TRUE;
            }
            if (!found)
                return FALSE;
        }
    }
    return TRUE;
}

// Clear the levels again, and put back the units the try opened or filled
// (PlanPass keeps them in gen->cover, unused until the cover is painted).
static void PassUndo(struct Gen *gen)
{
    memcpy(gen->g, gen->cover, gen->cw * gen->ch);
    memset(gen->upper, 0, sizeof(gen->upper));
    memset(gen->grassZone, 0, sizeof(gen->grassZone));
    gen->terraced = FALSE;
    gen->nStairs = 0;
}

// Every block of open ground the player can walk on is reached from the way
// in, rims being solid and the steps not. Open ground left out (a nook
// above a rim) is filled in: it becomes rock or trees. FALSE if the road
// itself is cut. Uses gen->free for the reached blocks (it is only needed
// once the trainers are placed).
static bool8 PassBlocksJoin(struct Gen *gen, bool8 *filled)
{
    int w = gen->m->w, h = gen->m->h, head = 0, tail = 0, fx, fy;
    int sx = (gen->path[0] % gen->cw) * 2, sy = (gen->path[0] / gen->cw) * 2;
#define PASS_WALK_F(x, y) ((x) >= 0 && (y) >= 0 && (x) < w && (y) < h && PassWalkU(gen, (x) >> 1, (y) >> 1) \
                           && !PassRimBlock(gen, (x), (y)))
#define PASS_SEEN(x, y) ((gen->free[((y) * w + (x)) >> 3] >> (((y) * w + (x)) & 7)) & 1)
    *filled = FALSE;
    memset(gen->free, 0, sizeof(gen->free));
    if (!PASS_WALK_F(sx, sy))
        sx++;
    if (!PASS_WALK_F(sx, sy))
        return FALSE;
    gen->free[(sy * w + sx) >> 3] |= 1 << ((sy * w + sx) & 7);
    sBfsQueue[tail++] = PACK(sx, sy);
    while (head != tail)
    {
        int c = sBfsQueue[head++ & (BFS_RING - 1)], d;
        YIELD();
        for (d = 0; d < 4; d++)
        {
            int nx = PX(c) + sDirDX[d], ny = PY(c) + sDirDY[d];
            if (!PASS_WALK_F(nx, ny) || PASS_SEEN(nx, ny))
                continue;
            gen->free[(ny * w + nx) >> 3] |= 1 << ((ny * w + nx) & 7);
            if (tail - head < BFS_RING)
                sBfsQueue[tail++ & (BFS_RING - 1)] = PACK(nx, ny);
        }
    }
    for (fy = 0; fy < h; fy++)
    {
        YIELD();
        for (fx = 0; fx < w; fx++)
        {
            int ux = fx >> 1, uy = fy >> 1;
            if (!PASS_WALK_F(fx, fy) || PASS_SEEN(fx, fy))
                continue;
            if (Cell(gen, ux, uy) != C_OPEN && Cell(gen, ux, uy) != C_CLIFF)
            {
                memset(gen->free, 0, sizeof(gen->free));
                return FALSE;
            }
            SetCell(gen, ux, uy, C_TREE);
            *filled = TRUE;
        }
    }
    memset(gen->free, 0, sizeof(gen->free));
#undef PASS_WALK_F
#undef PASS_SEEN
    return TRUE;
}

// Raise the levels of two profiles (top2 may be NULL) and check them: no
// rim faces north or is crossed but by steps, no unit has rims on two
// opposite sides, every rim is crossed by the road, and everything stays
// reachable. Rims become C_CLIFF. Undone on failure.
static bool8 PassApply(struct Gen *gen, const s8 *top1, const s8 *top2, int ax, int ay, int nRims)
{
    int x, y, round, i;
    bool8 filled = TRUE;
    memset(gen->upper, 0, sizeof(gen->upper));
    memset(gen->grassZone, 0, sizeof(gen->grassZone));
    for (y = 0; y < gen->ch; y++)
        for (x = 0; x < gen->cw; x++)
        {
            if (top1[x] >= 0 && y <= top1[x])
                SetBitU(gen->upper, gen, x, y);
            if (top2 != NULL && top2[x] >= 0 && y <= top2[x])
                SetBitU(gen->grassZone, gen, x, y);
        }
    gen->terraced = TRUE;
    for (round = 0; round < 6 && filled; round++)
    {
        filled = FALSE;
        gen->nStairs = 0;
        for (y = 0; y < gen->ch; y++)
            for (x = 0; x < gen->cw; x++)
                if (Cell(gen, x, y) == C_CLIFF)
                    SetCell(gen, x, y, C_OPEN);
        for (y = 0; y < gen->ch; y++)
        {
            YIELD();
            for (x = 0; x < gen->cw; x++)
            {
                u8 c = Cell(gen, x, y), l, lo[3], k;
                bool8 lowS, lowE, lowW, lowSE, lowSW, lowN;
                if (!IsOpenU(c))
                    continue;
                l = TerraceLevelU(gen, x, y);
                for (k = 0; k < 3; k++)
                    lo[k] = PassNbLevel(gen, NULL, x + k - 1, y - 1, PASS_ABOVE, l) < l;
                lowN = lo[0] || lo[1] || lo[2];
                lowSW = PassNbLevel(gen, NULL, x - 1, y + 1, PASS_DIAG, l) < l;
                lowS = PassNbLevel(gen, NULL, x, y + 1, PASS_BELOW, l) < l;
                lowSE = PassNbLevel(gen, NULL, x + 1, y + 1, PASS_DIAG, l) < l;
                lowW = PassNbLevel(gen, NULL, x - 1, y, PASS_SIDE, l) < l;
                lowE = PassNbLevel(gen, NULL, x + 1, y, PASS_SIDE, l) < l;
                if (lowN || (lowE && lowW))
                {
                    PassUndo(gen);
                    return FALSE;
                }
                if (!(lowS || lowSE || lowSW || lowE || lowW))
                    continue;
                if (c == C_ROAD)
                {
                    // (Steps stand in a rim of their own level: no higher
                    // ground's side comes down beside them.)
                    if (lowS && !lowE && !lowW && gen->nStairs < 4
                     && Cell(gen, x, y - 1) == C_ROAD && Cell(gen, x, y + 1) == C_ROAD
                     && Cell(gen, x - 1, y) != C_ROAD && Cell(gen, x + 1, y) != C_ROAD
                     && !(PassWalkU(gen, x - 1, y) && TerraceLevelU(gen, x - 1, y) > l)
                     && !(PassWalkU(gen, x + 1, y) && TerraceLevelU(gen, x + 1, y) > l))
                    {
                        gen->stairX[gen->nStairs] = x;
                        gen->stairY[gen->nStairs++] = y;
                        continue;
                    }
                    // Only a tree lower beside it? It makes way.
                    if (PassOpenTreeBy(gen, x, y, l))
                    {
                        filled = TRUE;
                        continue;
                    }
                    PassUndo(gen);
                    return FALSE;
                }
                if (c != C_OPEN)
                {
                    PassUndo(gen);
                    return FALSE;
                }
                // A rim stands at the edge of ground on its level, never
                // as a strip one block wide along it (a person standing
                // in it would shut the way): open ground behind it, or
                // rock and trees fill it in.
                if ((lowS && !(PassWalkU(gen, x, y - 1) && TerraceLevelU(gen, x, y - 1) == l))
                 || (lowE && !(PassWalkU(gen, x - 1, y) && TerraceLevelU(gen, x - 1, y) == l))
                 || (lowW && !(PassWalkU(gen, x + 1, y) && TerraceLevelU(gen, x + 1, y) == l)))
                {
                    SetCell(gen, x, y, C_TREE);
                    filled = TRUE;
                    continue;
                }
                SetCell(gen, x, y, C_CLIFF);
            }
        }
        if (filled)
            continue;
        if (!PassBlocksJoin(gen, &filled))
        {
            PassUndo(gen);
            return FALSE;
        }
    }
    // Every rim is climbed somewhere, and the way through is whole.
    if (filled || gen->nStairs < nRims)
    {
        PassUndo(gen);
        return FALSE;
    }
    FloodReachable(gen, ax, ay);
    for (i = 0; i < gen->nPath; i++)
        if (!Reached(gen, gen->path[i] % gen->cw, gen->path[i] / gen->cw))
        {
            PassUndo(gen);
            return FALSE;
        }
    return TRUE;
}

// Plan a pass's levels once its way is carved: one rim or two across the
// map, each through a stretch of the main road that runs straight up and
// down, where stone steps climb it. Without a profile that fits, the pass
// stays on one level.
static void PlanPass(struct Gen *gen, int ax, int ay)
{
    u16 lanes[PASS_MAX_LANES];
    s8 top1[MAX_CW], top2[MAX_CW];
    int nMain, nLanes = 0, i, tries, lo, lx, ly;
    // (A map that keeps failing, most often for want of room for a
    // feature's house, is built on one level from its tenth try.)
    // (v1.1 vert: or a raised shelf on a forest or a field, ShelfRoute.)
    if ((gen->p->type != RROUTE_HILLS && !ShelfRoute(gen)) || gen->forestCut || gen->nPath < 8 || gen->attempt >= 10)
        return;
    // The main road runs on unit by unit; the branches come after it.
    for (nMain = 1; nMain < gen->nPath; nMain++)
        if (Abs(gen->path[nMain] % gen->cw - gen->path[nMain - 1] % gen->cw)
          + Abs(gen->path[nMain] / gen->cw - gen->path[nMain - 1] / gen->cw) != 1)
            break;
    lo = gen->guardEnd ? gen->guardEnd + 3 : 2;
    for (i = lo; i + 2 < nMain && nLanes < PASS_MAX_LANES; i++)
        if (PassLaneAt(gen, i, &lx, &ly))
            lanes[nLanes++] = i;
    if (nLanes == 0)
        return;
    memcpy(gen->cover, gen->g, gen->cw * gen->ch);
    for (tries = 0; tries < 24 && !gen->terraced; tries++)
    {
        // (v1.1 vert: one level of grass; the second is a plateau of rock,
        // gen->grassZone, rogue_gen_vert.h. The roll stays, so the rest of
        // the map is drawn as before.)
        int a = lanes[RAND(nLanes)], b, two = CHANCE(60) && nLanes > 1 && FALSE;
        YIELD();
        PassLaneAt(gen, a, &lx, &ly);
        if (!PassProfile(gen, top1, NULL, lx, ly))
            continue;
        if (two)
        {
            int k;
            two = FALSE;
            for (k = 0; k < 6 && !two; k++)
            {
                int bx, by;
                b = lanes[RAND(nLanes)];
                if (Abs(b - a) < 5)
                    continue;
                PassLaneAt(gen, b, &bx, &by);
                if (PassProfile(gen, top2, top1, bx, by))
                    two = TRUE;
            }
            if (two && PassApply(gen, top1, top2, ax, ay, 2))
                break;
        }
        if (PassApply(gen, top1, NULL, ax, ay, 1))
            break;
    }
    memset(gen->cover, 0, gen->cw * gen->ch);
    // Dead ends now under a rim or filled in hold no items.
    for (i = 0, nLanes = 0; i < gen->nPockets; i++)
        if (Cell(gen, gen->pocketX[i], gen->pocketY[i]) == C_OPEN)
        {
            gen->pocketX[nLanes] = gen->pocketX[i];
            gen->pocketY[nLanes++] = gen->pocketY[i];
        }
    gen->nPockets = nLanes;
}

// Before the ground cover is tidied: no grass, path or sand on a rim's
// blocks, so the cover beside them ends in its own edge. A path runs on
// over the steps, as onto a town's.
static void PassClearRims(struct Gen *gen)
{
    int fx, fy;
    if (!gen->terraced || gen->p->kind != RPLACE_ROUTE)
        return;
    for (fy = 0; fy < gen->m->h; fy++)
    {
        YIELD();
        for (fx = 0; fx < gen->m->w; fx++)
        {
            if (PassRimBlock(gen, fx, fy))
                SetCover(gen, fx, fy, V_NONE);
            // (v1.1 vert: long grass ends in its lower edge, drawn on the
            // block below it, which a rim or the steps would cover.)
            else if (gen->longGrass && fy + 1 < gen->m->h && Cover(gen, fx, fy) == V_TALL
                  && (PassRimBlock(gen, fx, fy + 1) || (((fy + 1) & 1) && IsStairU(gen, fx >> 1, (fy + 1) >> 1))))
                SetCover(gen, fx, fy, V_NONE);
        }
    }
}

// How high open ground or a tree stands, for the rock beside it.
static u8 PassGroundLevel(struct Gen *gen, int x, int y)
{
    return Cell(gen, x, y) == C_TREE ? PassTreeLevel(gen, x, y) : TerraceLevelU(gen, x, y);
}

// The rock of a terraced pass. Hoenn's rock steps up a level at a time
// from its foot, and every unit of a mass's foot stands at one height (a
// foot never meets lower rock: no piece joins the two). So each mass's foot
// stands a level above the highest ground anywhere along it, and inside the
// mass the rock rises a level a unit as RenderMountain's does.
static void PassRockLevels(struct Gen *gen, u8 *level)
{
    int ux, uy, dx, dy, n = gen->cw * gen->ch, i;
    u8 l;
    bool8 changed;
    // The depth of each rock unit inside its mass (1 at the foot); the
    // map's edge counts as rock unless ground joins there. (v1.1 vert: two
    // steps at most, then a flat top, as Route 111's and 114's rock, not
    // rings inside rings.)
    for (l = 2; l <= PASS_ROCK_STEPS; l++)
    {
        for (uy = 0; uy < gen->ch; uy++)
        {
            YIELD();
            for (ux = 0; ux < gen->cw; ux++)
            {
                bool8 up = (level[uy * gen->cw + ux] == l - 1);
                // (v1.1c land2: a step only where the mass is at least five
                // units across, so a narrow tongue of rock is one flat level,
                // not a ring inside a ring.)
                for (dy = -2; dy <= 2 && up; dy++)
                    for (dx = -2; dx <= 2 && up; dx++)
                    {
                        int nx = ux + dx, ny = uy + dy;
                        u8 h = InBounds(gen, nx, ny) ? level[ny * gen->cw + nx] & 0x7F : MountainLevel(gen, level, nx, ny);
                        if ((Abs(dx) == 2 || Abs(dy) == 2) && h == 0 && l == 2)
                            up = FALSE;
                        if (Abs(dx) == 2 || Abs(dy) == 2)
                            continue;
                        if (h < l - 1)
                            up = FALSE;
                    }
                if (up)
                    level[uy * gen->cw + ux] = l | 0x80;
            }
        }
        for (i = 0; i < n; i++)
            level[i] &= 0x7F;
    }
    // The highest ground beside each unit, kept in the high bits of level
    // (the depth is at most PASS_CAP).
    for (uy = 0; uy < gen->ch; uy++)
    {
        YIELD();
        for (ux = 0; ux < gen->cw; ux++)
        {
            u8 foot = 0;
            if (!level[uy * gen->cw + ux])
                continue;
            for (dy = -1; dy <= 1; dy++)
                for (dx = -1; dx <= 1; dx++)
                {
                    int nx = ux + dx, ny = uy + dy;
                    if (InBounds(gen, nx, ny) && !level[ny * gen->cw + nx] && PassGroundLevel(gen, nx, ny) > foot)
                        foot = PassGroundLevel(gen, nx, ny);
                }
            level[uy * gen->cw + ux] |= foot << 4;
        }
    }
    // The same foot all through a mass (rock joined side by side or corner
    // to corner).
    do
    {
        changed = FALSE;
        for (i = 0; i < n; i++)
        {
            int x = i % gen->cw, y = i / gen->cw;
            if (!level[i])
                continue;
            if (x == 0)
                YIELD();
            for (dy = -1; dy <= 1; dy++)
                for (dx = -1; dx <= 1; dx++)
                {
                    int nx = x + dx, ny = y + dy, k = ny * gen->cw + nx;
                    if (InBounds(gen, nx, ny) && level[k] && (level[k] >> 4) > (level[i] >> 4))
                    {
                        level[i] = (level[i] & 0x0F) | (level[k] & 0xF0);
                        changed = TRUE;
                    }
                }
        }
    } while (changed);
    for (i = 0; i < n; i++)
    {
        int h;
        if (!level[i])
            continue;
        h = (level[i] >> 4) + (level[i] & 0x0F);
        level[i] = h > PASS_CAP ? PASS_CAP : h;
    }
}

// How high a unit stands for the rock's edges: rock its level, open ground
// its level of the map, a tree the level it stands at.
static u8 PassHeight(struct Gen *gen, const u8 *level, int x, int y)
{
    if (!InBounds(gen, x, y))
        return MountainLevel(gen, level, x, y);
    if (level[y * gen->cw + x])
        return level[y * gen->cw + x];
    if (Cell(gen, x, y) == C_TREE)
        return PassTreeLevel(gen, x, y);
    return TerraceLevelU(gen, x, y);
}

// A terraced pass's rock: an edge where a neighbour is lower, with the foot
// against ground (open ground or trees below it) or the step against lower
// rock, and a boulder now and then on a flat top.
static void PassRenderRock(struct Gen *gen, const u8 *level)
{
    int ux, uy, q;
    for (uy = 0; uy < gen->ch; uy++)
    {
        YIELD();
        for (ux = 0; ux < gen->cw; ux++)
        {
            u8 top = level[uy * gen->cw + ux];
            bool8 plain = TRUE;
            if (top == 0)
                continue;
            for (q = 0; q < 4; q++)
            {
                int sx = (q & 1) ? 1 : -1, sy = (q & 2) ? 1 : -1;
                u8 k = MK_C, hv = 0, hh = 0, hd = 0;
                u16 block = sRockOnGround[MK_C];
                hv = PassHeight(gen, level, ux, uy + sy);
                hh = PassHeight(gen, level, ux + sx, uy);
                hd = PassHeight(gen, level, ux + sx, uy + sy);
                {
                    // The edge toward whatever is lower than this rock's top,
                    // against ground if what is lower there is not rock.
                    bool8 v = hv >= top, h = hh >= top, dd = hd >= top, ground;
                    // (v1.1 vert: a plateau is rock too, so rock on it has
                    // the rock-on-rock foot.)
                    bool8 rv = InBounds(gen, ux, uy + sy) && (level[(uy + sy) * gen->cw + ux] || PlateauU(gen, ux, uy + sy));
                    bool8 rh = InBounds(gen, ux + sx, uy) && (level[uy * gen->cw + ux + sx] || PlateauU(gen, ux + sx, uy));
                    bool8 rd = InBounds(gen, ux + sx, uy + sy)
                            && (level[(uy + sy) * gen->cw + ux + sx] || PlateauU(gen, ux + sx, uy + sy));
                    if (!v && !h)
                        k = sy < 0 ? (sx < 0 ? MK_TL : MK_TR) : (sx < 0 ? MK_BL : MK_BR);
                    else if (!v)
                        k = sy < 0 ? MK_T : MK_B;
                    else if (!h)
                        k = sx < 0 ? MK_L : MK_R;
                    else if (!dd)
                        k = sy < 0 ? (sx < 0 ? MK_NW : MK_NE) : (sx < 0 ? MK_SW : MK_SE);
                    ground = (!v && !rv) || (!h && !rh) || (v && h && !dd && !rd);
                    if (k != MK_C)
                        block = ground ? sRockOnGround[k] : sRockOnRock[k];
                }
                if (k != MK_C)
                    plain = FALSE;
                Put(gen, ux * 2 + (q & 1), uy * 2 + (q >> 1), block);
            }
            // (v1.1c land2: and no higher rock beside it, whose edge would
            // meet the boulder: Hoenn's boulders stand clear of steps.)
            for (q = 0; q < 9 && plain; q++)
                if (q != 4 && PassHeight(gen, level, ux + q % 3 - 1, uy + q / 3 - 1) > top)
                    plain = FALSE;
            if (plain && RockBoulderU(gen, ux, uy))
            {
                Put(gen, ux * 2, uy * 2, 0x0493);
                Put(gen, ux * 2 + 1, uy * 2, 0x0494);
                Put(gen, ux * 2, uy * 2 + 1, 0x049B);
                Put(gen, ux * 2 + 1, uy * 2 + 1, 0x049C);
            }
        }
    }
}

// The rims of a terraced pass's open ground, and its stone steps. A rim's
// foot over a tree has the tree's tips drawn on it.
static void RenderPassRims(struct Gen *gen, const u8 *level)
{
    int ux, uy, q, w = gen->m->w;
    for (uy = 0; uy < gen->ch; uy++)
    {
        YIELD();
        for (ux = 0; ux < gen->cw; ux++)
        {
            u8 self;
            if ((level != NULL && level[uy * gen->cw + ux]) || !PassWalkU(gen, ux, uy))
                continue;
            self = TerraceLevelU(gen, ux, uy);
            if (self == 0)
                continue;
            if (PlateauU(gen, ux, uy)) // (v1.1 vert: rogue_gen_vert.h)
            {
                RenderPlateauUnit(gen, level, ux, uy);
                continue;
            }
            for (q = 0; q < 4; q++)
            {
                int fx = ux * 2 + (q & 1), fy = uy * 2 + (q >> 1);
                u8 k = PassKey(gen, level, ux, uy, q, self);
                u16 block;
                if (k == MK_C)
                    continue;
                if (q >= 2 && IsStairU(gen, ux, uy))
                {
                    Put(gen, fx, fy, (q & 1) ? 0x30CF : 0x30AF);
                    continue;
                }
                block = RimFoot(gen, k, fx, fy); // (v1.1 vert: sand foot over sand)
                if (k == MK_B && fy + 1 < gen->m->h)
                {
                    u16 t = gen->m->blocks[(fy + 1) * w + fx] & 0x3FF;
                    if ((q & 1) ? (t == 0x1D5 || t == 0x1D7) : (t == 0x1D4 || t == 0x1D6))
                        block = 0x0400 | (0x1F0 + (q & 1));
                }
                Put(gen, fx, fy, block);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Canyons

// A canyon (a desert of sand, not ash) is drawn as rock.
static bool8 IsCanyon(struct Gen *gen)
{
    return gen->p->kind == RPLACE_ROUTE && gen->p->type == RROUTE_DESERT && gen->rocky;
}

// Route 111's boulder on the sand (General).
static const u16 sCanyonBoulder[4] = { 0x04A3, 0x04A4, 0x04AB, 0x04AC };

static bool8 CanyonBoulderAt(struct Gen *gen, int x, int y)
{
    if (gen->nDecor >= MAX_DECOR || !AddDecor(gen, sCanyonBoulder, x * 2, y * 2, 2, 2))
        return FALSE;
    SetCell(gen, x, y, C_DECOR);
    return TRUE;
}

// Is unit (x, y) on its own, open ground all round it?
static bool8 CanyonLoneU(struct Gen *gen, int x, int y)
{
    int dx, dy;
    if (IsEdge(gen, x, y))
        return FALSE;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
        {
            u8 c = Cell(gen, x + dx, y + dy);
            if ((dx || dy) && c != C_OPEN && c != C_ROAD)
                return FALSE;
        }
    return TRUE;
}

// Boulders out on a canyon's sand, as on Route 111 (decor, solid, with the
// sand running up to them): each lone tree the fields were broken up with,
// and a few more off the road and away from the openings, each leaving
// everything reachable as it was.
static void PlaceCanyonBoulders(struct Gen *gen, int ax, int ay)
{
    int tries, placed = 0, count, x, y;
    if (!IsCanyon(gen))
        return;
    for (y = 1; y < gen->ch - 1; y++)
        for (x = 1; x < gen->cw - 1; x++)
            if (Cell(gen, x, y) == C_TREE && CanyonLoneU(gen, x, y) && !DesertGrassBandU(gen, x, y))
                CanyonBoulderAt(gen, x, y);
    // (None more on a map that keeps failing, most often for want of
    // room for a feature's house.)
    count = gen->attempt >= 10 ? 0 : 3 + RAND(4);
    for (tries = 0; tries < 60 && placed < count; tries++)
    {
        int before;
        x = 2 + RAND(gen->cw - 4);
        y = 2 + RAND(gen->ch - 4);
        YIELD();
        if (Cell(gen, x, y) != C_OPEN || NearExitU(gen, x, y, 3) || DesertGrassBandU(gen, x, y) || !CanyonLoneU(gen, x, y))
            continue;
        before = FloodReachable(gen, ax, ay);
        SetCell(gen, x, y, C_DECOR);
        if (FloodReachable(gen, ax, ay) != before - 1 || !CanyonBoulderAt(gen, x, y))
        {
            SetCell(gen, x, y, C_OPEN);
            continue;
        }
        placed++;
    }
}

// (v1.1c land2) Route 111's walls are thick: on a wide canyon the carved
// floor left its rock in strips two units across (drawn as rings inside
// rings). Each mass grows a unit into the open floor round it, where that
// floor is two units or more from the trail: not by an opening
// (NearExitU), in the grass band by a green neighbour, or by a dead end.
// A quarter of the map at a time, kept only if it cuts nothing else off.
static void CanyonThickWalls(struct Gen *gen, int ax, int ay)
{
    int side, x, y, i;
    if (!IsCanyon(gen) || gen->attempt >= 10 || gen->cw * gen->ch < 320)
        return;
    for (side = 0; side < 4; side++)
    {
        int before = FloodReachable(gen, ax, ay), n = 0, after;
        bool8 ok = TRUE;
        u8 *was = gen->free; // free until the trainers are placed: a bit per unit grown
        memset(gen->free, 0, sizeof(gen->free));
        for (y = 1; y < gen->ch - 1; y++)
        {
            YIELD();
            for (x = 1; x < gen->cw - 1; x++)
            {
                int q = (x >= gen->cw / 2) + 2 * (y >= gen->ch / 2), k;
                bool8 byRock = FALSE;
                bool8 pocket = FALSE;
                for (k = 0; k < 4; k++)
                    if (Cell(gen, x + sDirDX[k], y + sDirDY[k]) == C_TREE && !BitU(was, gen, x + sDirDX[k], y + sDirDY[k]))
                        byRock = TRUE;
                if (q != side || !byRock || Cell(gen, x, y) != C_OPEN || NearExitU(gen, x, y, 3) || DesertGrassBandU(gen, x, y)
                 || BitU(gen->keep, gen, x, y) || !Reached(gen, x, y))
                    continue;
                for (i = 0; i < gen->nPockets; i++)
                    if (Abs(gen->pocketX[i] - x) + Abs(gen->pocketY[i] - y) <= 1)
                        pocket = TRUE;
                for (i = 0; i < gen->nPath && !pocket; i++)
                    if (Abs(gen->path[i] % gen->cw - x) + Abs(gen->path[i] / gen->cw - y) <= 2)
                        pocket = TRUE;
                if (pocket)
                    continue;
                SetBitU(was, gen, x, y);
                SetCell(gen, x, y, C_TREE);
                n++;
            }
        }
        after = FloodReachable(gen, ax, ay);
        for (i = 0; i < gen->nPath && ok; i++)
            if (!Reached(gen, gen->path[i] % gen->cw, gen->path[i] / gen->cw))
                ok = FALSE;
#ifdef LAND_DEBUG
        fprintf(stderr, "canyon side %d n=%d ok=%d before=%d after=%d\n", side, n, ok, before, after);
#endif
        if (!ok || after != before - n)
        {
            for (y = 1; y < gen->ch - 1; y++)
                for (x = 1; x < gen->cw - 1; x++)
                    if (BitU(was, gen, x, y))
                        SetCell(gen, x, y, C_OPEN);
            FloodReachable(gen, ax, ay);
        }
    }
    memset(gen->free, 0, sizeof(gen->free));
}

// Before the ground cover is tidied: no grass round a canyon's boulders,
// which stand on the sand (Route 111 has none on grass).
static void CanyonClearGrass(struct Gen *gen)
{
    int i, x, y;
    if (!IsCanyon(gen))
        return;
    for (i = 0; i < gen->nDecor; i++)
    {
        const struct Decor *dc = &gen->decor[i];
        if (dc->blocks != sCanyonBoulder)
            continue;
        for (y = dc->y - 2; y < dc->y + 4; y++)
            for (x = dc->x - 2; x < dc->x + 4; x++)
                if (InMap(gen, x, y) && (Cover(gen, x, y) == V_TALL || Cover(gen, x, y) == V_FLOWER))
                    SetCover(gen, x, y, V_NONE);
    }
}

// ---------------------------------------------------------------------------
// Ash fields

// Route 113's outcrops of rock standing in the ash: General rock with
// Fallarbor's ash foot (0x201-0x213), so only on an ash field's own
// tileset and never in view of a seam. Two to three units a side.
#define ASH_ROCK(t) (0x0400 | (t))
static const u16 sAshRock4x4[] = {
    ASH_ROCK(0x201), ASH_ROCK(0x202), ASH_ROCK(0x202), ASH_ROCK(0x203),
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    ASH_ROCK(0x211), ASH_ROCK(0x079), ASH_ROCK(0x079), ASH_ROCK(0x213),
};
static const u16 sAshRock6x4[] = {
    ASH_ROCK(0x201), ASH_ROCK(0x202), ASH_ROCK(0x202), ASH_ROCK(0x202), ASH_ROCK(0x202), ASH_ROCK(0x203),
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    ASH_ROCK(0x211), ASH_ROCK(0x079), ASH_ROCK(0x079), ASH_ROCK(0x079), ASH_ROCK(0x079), ASH_ROCK(0x213),
};
static const u16 sAshRock4x6[] = {
    ASH_ROCK(0x201), ASH_ROCK(0x202), ASH_ROCK(0x202), ASH_ROCK(0x203),
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    ASH_ROCK(0x211), ASH_ROCK(0x079), ASH_ROCK(0x079), ASH_ROCK(0x213),
};
// A boulder on the rock's top (Route 113's 0x093 0x094 over 0x09B 0x09C).
static const u16 sAshRock6x6[] = {
    ASH_ROCK(0x201), ASH_ROCK(0x202), ASH_ROCK(0x202), ASH_ROCK(0x202), ASH_ROCK(0x202), ASH_ROCK(0x203),
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x093), ASH_ROCK(0x094), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x09B), ASH_ROCK(0x09C), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    ASH_ROCK(0x211), ASH_ROCK(0x079), ASH_ROCK(0x079), ASH_ROCK(0x079), ASH_ROCK(0x079), ASH_ROCK(0x213),
};
// (v1.1c land2) Route 113's outcrops are not plain boxes: their tops and
// feet step in and out, with its inner corners (0x0BE, 0x0BF, 0x089; made by
// a script from unit masks, every pair as Route 113 has it). 0xFFFF is not
// rock: the ground is drawn there (Render skips it).
static const u16 sAshRockStepNE[] = {
    ASH_ROCK(0x201), ASH_ROCK(0x202), ASH_ROCK(0x202), ASH_ROCK(0x203), 0xFFFF, 0xFFFF,
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B), 0xFFFF, 0xFFFF,
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x0BF), ASH_ROCK(0x202), ASH_ROCK(0x203),
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    ASH_ROCK(0x211), ASH_ROCK(0x079), ASH_ROCK(0x079), ASH_ROCK(0x079), ASH_ROCK(0x079), ASH_ROCK(0x213),
};
static const u16 sAshRockStepNW[] = {
    0xFFFF, 0xFFFF, ASH_ROCK(0x201), ASH_ROCK(0x202), ASH_ROCK(0x202), ASH_ROCK(0x203),
    0xFFFF, 0xFFFF, ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    ASH_ROCK(0x201), ASH_ROCK(0x202), ASH_ROCK(0x0BE), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    ASH_ROCK(0x211), ASH_ROCK(0x079), ASH_ROCK(0x079), ASH_ROCK(0x079), ASH_ROCK(0x079), ASH_ROCK(0x213),
};
static const u16 sAshRockStepSW[] = {
    ASH_ROCK(0x201), ASH_ROCK(0x202), ASH_ROCK(0x202), ASH_ROCK(0x202), ASH_ROCK(0x202), ASH_ROCK(0x203),
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    ASH_ROCK(0x211), ASH_ROCK(0x079), ASH_ROCK(0x089), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    0xFFFF, 0xFFFF, ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    0xFFFF, 0xFFFF, ASH_ROCK(0x211), ASH_ROCK(0x079), ASH_ROCK(0x079), ASH_ROCK(0x213),
};
static const u16 sAshRockHump[] = {
    0xFFFF, 0xFFFF, ASH_ROCK(0x201), ASH_ROCK(0x202), ASH_ROCK(0x202), ASH_ROCK(0x203), 0xFFFF, 0xFFFF,
    0xFFFF, 0xFFFF, ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B), 0xFFFF, 0xFFFF,
    ASH_ROCK(0x201), ASH_ROCK(0x202), ASH_ROCK(0x0BE), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x0BF), ASH_ROCK(0x202), ASH_ROCK(0x203),
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    ASH_ROCK(0x211), ASH_ROCK(0x079), ASH_ROCK(0x079), ASH_ROCK(0x079), ASH_ROCK(0x079), ASH_ROCK(0x079), ASH_ROCK(0x079), ASH_ROCK(0x213),
};
static const u16 sAshRockZig[] = {
    ASH_ROCK(0x201), ASH_ROCK(0x202), ASH_ROCK(0x202), ASH_ROCK(0x202), ASH_ROCK(0x202), ASH_ROCK(0x203), 0xFFFF, 0xFFFF,
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B), 0xFFFF, 0xFFFF,
    ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x0BF), ASH_ROCK(0x202), ASH_ROCK(0x203),
    ASH_ROCK(0x211), ASH_ROCK(0x079), ASH_ROCK(0x089), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    0xFFFF, 0xFFFF, ASH_ROCK(0x209), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x071), ASH_ROCK(0x20B),
    0xFFFF, 0xFFFF, ASH_ROCK(0x211), ASH_ROCK(0x079), ASH_ROCK(0x079), ASH_ROCK(0x079), ASH_ROCK(0x079), ASH_ROCK(0x213),
};
static const struct { const u16 *blocks; u8 uw, uh; u16 mask; } sAshRocks[] = {
    { sAshRock4x4, 2, 2, 0xF }, { sAshRock6x4, 3, 2, 0x3F }, { sAshRock4x6, 2, 3, 0x3F }, { sAshRock6x6, 3, 3, 0x1FF },
    { sAshRockStepNE, 3, 3, 0x1FB }, { sAshRockStepNW, 3, 3, 0x1FE }, { sAshRockStepSW, 3, 3, 0x1BF },
    { sAshRockHump, 4, 3, 0xFF6 }, { sAshRockZig, 4, 3, 0xEF7 },
};
#define ASH_ROCK_IN(k, x, y) ((x) >= 0 && (y) >= 0 && (x) < sAshRocks[k].uw && (y) < sAshRocks[k].uh \
                             && ((sAshRocks[k].mask >> ((y) * sAshRocks[k].uw + (x))) & 1))

static bool8 IsAshField(struct Gen *gen)
{
    return gen->p->kind == RPLACE_ROUTE && gen->p->type == RROUTE_DESERT && gen->groundKind == GROUND_ASH;
}

// Is the decor at unit (x, y) rock standing on the special ground (a
// canyon's boulder, an ash field's outcrop)? The ground runs up to it.
static bool8 RockDecorU(struct Gen *gen, int x, int y)
{
    int i;
    for (i = 0; i < gen->nDecor; i++)
    {
        const struct Decor *dc = &gen->decor[i];
        if (x * 2 >= dc->x && x * 2 < dc->x + dc->w && y * 2 >= dc->y && y * 2 < dc->y + dc->h)
        {
            int k;
            if (dc->blocks[(y * 2 - dc->y) * dc->w + x * 2 - dc->x] == 0xFFFF)
                continue; // (v1.1c: ground inside an outcrop's box)
            for (k = 0; k < (int)(sizeof(sAshRocks) / sizeof(sAshRocks[0])); k++)
                if (dc->blocks == sAshRocks[k].blocks)
                    return TRUE;
            return dc->blocks == sCanyonBoulder
                || dc->blocks == sMirageTowerStamp; // (v1.1 peaks: the desert tower, rogue_gen_peaks.h)
        }
    }
    return FALSE;
}

// One to three outcrops in an ash field's ash, standing out of its trees or
// on open ground, with open ash above and below them as Route 113's (trees
// there give way to it), off the road and the pink dirt, each leaving
// everything reachable as it was or more.
static void PlaceAshOutcrops(struct Gen *gen, int ax, int ay)
{
    int tries, placed = 0, count;
    if (!IsAshField(gen))
        return;
    count = gen->attempt >= 10 ? 0 : 1 + RAND(3);
    for (tries = 0; tries < 80 && placed < count && gen->nDecor < MAX_DECOR; tries++)
    {
        int k = RAND(sizeof(sAshRocks) / sizeof(sAshRocks[0])), uw = sAshRocks[k].uw, uh = sAshRocks[k].uh;
        int x0 = 2 + RAND(gen->cw - 3 - uw), y0 = 2 + RAND(gen->ch - 3 - uh), x, y, before, opened = 0, open = 0, n = 0;
        u8 saved[(3 + 2) * 4], sx[(3 + 2) * 4], sy[(3 + 2) * 4];
        bool8 ok = TRUE;
        YIELD();
        for (y = y0 - 1; y <= y0 + uh && ok; y++)
            for (x = x0 - 1; x <= x0 + uw && ok; x++)
            {
                // (Inside the shape's mask; a cap is any other unit of its
                // columns from the row above it to the row below, a notch
                // included, which stays open ash.)
                bool8 inside = ASH_ROCK_IN(k, x - x0, y - y0);
                bool8 cap = !inside && x >= x0 && x < x0 + uw;
                u8 c = Cell(gen, x, y);
                if (!AshZoneU(gen, x, y) || PinkU(gen, x, y) || InSeamView(gen, x, y, 1, 1))
                    ok = FALSE;
                else if (inside || cap)
                {
                    if (c == C_OPEN || (cap && c == C_ROAD))
                        open++;
                    else if (c != C_TREE || IsEdge(gen, x, y) || BitU(gen->keep, gen, x, y))
                        ok = FALSE;
                    if (inside && (c == C_ROAD || NearExitU(gen, x, y, 2)))
                        ok = FALSE;
                }
                else if (c != C_OPEN && c != C_ROAD && c != C_TREE)
                    ok = FALSE;
            }
        // Where it shows: some open ground round it already.
        if (!ok || open < 2)
            continue;
        before = FloodReachable(gen, ax, ay);
        for (y = y0 - 1; y <= y0 + uh; y++)
            for (x = x0; x < x0 + uw; x++)
            {
                bool8 inside = ASH_ROCK_IN(k, x - x0, y - y0);
                u8 c = Cell(gen, x, y);
                sx[n] = x;
                sy[n] = y;
                saved[n++] = c;
                if (inside)
                {
                    opened += c == C_OPEN;
                    SetCell(gen, x, y, C_DECOR);
                }
                else if (c == C_TREE)
                {
                    SetCell(gen, x, y, C_OPEN);
                }
            }
        if (FloodReachable(gen, ax, ay) < before - opened
         || !AddDecor(gen, sAshRocks[k].blocks, x0 * 2, y0 * 2, uw * 2, uh * 2))
        {
            while (n-- > 0)
                SetCell(gen, sx[n], sy[n], saved[n]);
            continue;
        }
        placed++;
    }
}

// Is block (x, y) on a route trainer's line of sight, or beside one (their
// own block and every block they see, up to a wall)? The rival's longer
// look keeps a block clear of them, as SightLineSafe keeps lines apart.
static bool8 NearTrainerLine(struct Gen *gen, int x, int y)
{
    int i, f, k;
    for (i = 0; i < gen->m->nObjs; i++)
    {
        const struct RogueObj *o = &gen->m->objs[i];
        u8 mask;
        if (o->kind != ROBJ_TRAINER)
            continue;
        mask = ObjSightMask(o);
        if (Abs(o->x - x) + Abs(o->y - y) <= 1)
            return TRUE;
        for (f = 0; f < 4; f++)
        {
            if (!(mask & (1 << f)))
                continue;
            for (k = 1; k <= o->range; k++)
            {
                int cx = o->x + sDirDX[f] * k, cy = o->y + sDirDY[f] * k;
                if (!StandableBlock(gen, cx, cy))
                    break;
                if (Abs(cx - x) + Abs(cy - y) <= 1)
                    return TRUE;
            }
        }
    }
    return FALSE;
}
