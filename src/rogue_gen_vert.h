// High ground on a mountain pass (v1.1 vert). Part of rogue_gen.c (included
// at its end; forward declared near the top).
//
// On Routes 111 north, 112, 114 and Jagged Pass the player walks up onto the
// mountain: stone steps climb a cliff face, the trail runs on across the
// rock's flat top for a good stretch, with the rock rising higher round it,
// and comes down again by steps. Here that is a plateau: a stretch of the
// main road with a step up at one end and a step down at the other (the
// road heads north into it and south out of it, as Hoenn's steps only ever
// climb northwards), and the ground round that stretch, all of it drawn as
// General's rock top (0x071, walkable as on Route 111). Its edges are the
// rock's own: faces (0x079) down to lower ground, side edges, a crest
// (0x069) along a top with lower ground beyond, inner corners; nothing but
// its steps leads on or off it, as in Hoenn, where rock top never meets
// walkable grass. The rock masses beside it stand on it (their foot is the
// rock-on-rock edge, 0x07C), so it reads as a shelf of the mountain.
//
// The plateau's units are gen->grassZone on a hills route (PlanPass keeps
// to one level there, gen->upper), a level above the highest ground beside
// them; their steps are in gen->stairX/Y with PlanPass's.

#define PLATEAU_MIN_STRETCH 5 // units of road on top, at the least

static bool8 PlateauU(struct Gen *gen, int x, int y)
{
    return gen->p->kind == RPLACE_ROUTE && gen->p->type == RROUTE_HILLS && gen->terraced
        && BitU(gen->grassZone, gen, x, y);
}

// A tree right below the middle of a plateau's face (the unit above has
// plateau on both sides): it stays, its tips drawn on the face as on Route
// 116. Ground beside the plateau or under a corner of it is grass
// (PlateauOpenSkirt), so no rock rising above the plateau stands against
// its edges (a rock's foot would change from rock to ground along one
// edge, which no Hoenn piece joins) and its corners stand on grass.
static bool8 PlateauPlugU(struct Gen *gen, int x, int y)
{
    return PlateauU(gen, x, y - 1) && PlateauU(gen, x - 1, y - 1) && PlateauU(gen, x + 1, y - 1);
}

// A tree below or beside the plateau (those PlateauOpenSkirt left, and
// those PlateauCutBypass planted) stays a tree, never rock.
static bool8 PlateauSkirtU(struct Gen *gen, int x, int y)
{
    if (Cell(gen, x, y) != C_TREE || BitU(gen->grassZone, gen, x, y) || gen->p->type != RROUTE_HILLS || !gen->terraced)
        return FALSE;
    return PlateauU(gen, x - 1, y) || PlateauU(gen, x + 1, y) || PlateauU(gen, x, y - 1)
        || PlateauU(gen, x - 1, y - 1) || PlateauU(gen, x + 1, y - 1);
}

// Open the forest beside and below the plateau (but its skirt of trees) to
// grass.
static void PlateauOpenSkirt(struct Gen *gen)
{
    int x, y;
    for (y = 1; y < gen->ch - 1; y++)
        for (x = 1; x < gen->cw - 1; x++)
        {
            if (Cell(gen, x, y) != C_TREE || BitU(gen->grassZone, gen, x, y) || BitU(gen->keep, gen, x, y)
             || PlateauPlugU(gen, x, y) || NearExitU(gen, x, y, 1))
                continue;
            if (BitU(gen->grassZone, gen, x - 1, y) || BitU(gen->grassZone, gen, x + 1, y) || BitU(gen->grassZone, gen, x, y - 1)
             || BitU(gen->grassZone, gen, x - 1, y - 1) || BitU(gen->grassZone, gen, x + 1, y - 1))
                SetCell(gen, x, y, C_OPEN);
        }
}

// A forest or a field with a raised shelf: open ground on a level a step
// up behind a one-block rim of rock, with stone steps where the trail climbs
// it, as Mossdeep's hillside (PlanPass draws it, as a mountain pass's grass
// level; PlanShelf keeps it only where it comes out clean). Tried on most of
// them from the second tier, by a hash of the seed and the place, never
// with ash (gen->upper is the ash's map there) or below the volcano's own
// mountain (rogue_gen_volcano.h).
static bool8 ShelfRoute(struct Gen *gen)
{
    const struct RoguePlace *p = gen->p;
    u32 roll;
    if (p->kind != RPLACE_ROUTE || p->tier == 0 || gen->ashFade
     || (p->type != RROUTE_FOREST && p->type != RROUTE_FIELD) || VolcanoMountainAt(gen->r, p - gen->r->places))
        return FALSE;
    roll = RogueRng_Hash(gen->r->seed, 7400 + (u32)(p - gen->r->places)) % 100;
    return roll < 70;
}

static void ClearBitU(u8 *bits, struct Gen *gen, int x, int y)
{
    int i;
    if (!InBounds(gen, x, y))
        return;
    i = y * gen->cw + x;
    bits[i >> 3] &= ~(1 << (i & 7));
}

// Does the neighbour (x, y) of a plateau unit at level self stand lower, so
// the plateau ends there in an edge? level is RenderMountain's (rock units
// nonzero) or NULL before the rock is known, when every tree counts as lower
// (the rims then are as many as they can be, never fewer). Off the map: no
// (the plateau keeps off the edges). A boulder on the plateau stands on it.
static bool8 PlateauLower(struct Gen *gen, const u8 *level, int x, int y, u8 self)
{
    if (!InBounds(gen, x, y))
        return FALSE;
    if (level != NULL && level[y * gen->cw + x])
        return FALSE;
    if (BitU(gen->grassZone, gen, x, y))
        return Cell(gen, x, y) != C_DECOR && TerraceLevelU(gen, x, y) < self;
    return TRUE;
}

// The edge of a plateau unit in quadrant q, as MountainKey: MK_C where the
// rock top runs on.
static u8 PlateauKey(struct Gen *gen, const u8 *level, int ux, int uy, int q)
{
    int sx = (q & 1) ? 1 : -1, sy = (q & 2) ? 1 : -1;
    u8 self = TerraceLevelU(gen, ux, uy);
    bool8 v = !PlateauLower(gen, level, ux, uy + sy, self);
    bool8 h = !PlateauLower(gen, level, ux + sx, uy, self);
    bool8 d = !PlateauLower(gen, level, ux + sx, uy + sy, self);
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

// Is block (fx, fy) of a plateau unit part of its edge (solid), before the
// rock is known? Its steps are not.
static bool8 PlateauRimBlock(struct Gen *gen, int fx, int fy)
{
    int ux = fx >> 1, uy = fy >> 1;
    if ((fy & 1) && IsStairU(gen, ux, uy))
        return FALSE;
    return PlateauKey(gen, NULL, ux, uy, (fx & 1) | ((fy & 1) << 1)) != MK_C;
}

// The units of the plateau standing on their own: lower ground on two
// opposite sides, so all of the unit is edge. Off the plateau they go (to
// trees, the rock round it); FALSE if one is on the road.
static bool8 PlateauThin(struct Gen *gen, int x, int y)
{
    u8 l = TerraceLevelU(gen, x, y);
    return (PlateauLower(gen, NULL, x, y - 1, l) && PlateauLower(gen, NULL, x, y + 1, l))
        || (PlateauLower(gen, NULL, x - 1, y, l) && PlateauLower(gen, NULL, x + 1, y, l));
}

// May unit (x, y) become plateau? Forest or open ground off the road, inside
// the map and away from the openings, with nothing standing on it.
static bool8 PlateauFree(struct Gen *gen, int x, int y)
{
    u8 c = Cell(gen, x, y);
    if ((c != C_OPEN && c != C_TREE) || IsEdge(gen, x, y) || x < 2 || y < 2 || x >= gen->cw - 2 || y >= gen->ch - 2
     || BitU(gen->keep, gen, x, y) || NearExitU(gen, x, y, 2))
        return FALSE;
    if (IsOccupied(gen, x * 2, y * 2) || IsOccupied(gen, x * 2 + 1, y * 2)
     || IsOccupied(gen, x * 2, y * 2 + 1) || IsOccupied(gen, x * 2 + 1, y * 2 + 1))
        return FALSE;
    return TRUE;
}

static u8 PlateauStepDir(struct Gen *gen, int i)
{
    int d = gen->path[i + 1] - gen->path[i];
    return d == -gen->cw ? RDIR_NORTH : d == gen->cw ? RDIR_SOUTH : d == 1 ? RDIR_EAST : RDIR_WEST;
}

// The ground below the plateau must not join the way in to the way on
// round it: the two ends' roads spread over the ground at once, and where
// they meet a tree is planted (on open ground, never the road), until they
// no longer meet. FALSE if only road joins them.
static bool8 PlateauCutBypass(struct Gen *gen, int i, int j, int nMain)
{
    s16 *owner = sBfsPrev;
    int tries, k, n = gen->cw * gen->ch;
    for (tries = 0; tries < 12; tries++)
    {
        int head = 0, tail = 0, cut = -1;
        memset(owner, 0, n * sizeof(owner[0]));
        for (k = 0; k < nMain; k++)
        {
            if (k > i && k <= j)
                continue;
            owner[gen->path[k]] = k <= i ? 1 : 2;
            sBfsQueue[tail++ & (BFS_RING - 1)] = gen->path[k];
        }
        while (head != tail && cut < 0)
        {
            int u = sBfsQueue[head++ & (BFS_RING - 1)], d;
            int ux = u % gen->cw, uy = u / gen->cw;
            YIELD();
            for (d = 0; d < 4 && cut < 0; d++)
            {
                int vx = ux + sDirDX[d], vy = uy + sDirDY[d], v = vy * gen->cw + vx;
                if (!InBounds(gen, vx, vy) || !IsOpenU(gen->g[v]) || BitU(gen->grassZone, gen, vx, vy))
                    continue;
                if (gen->g[u] == C_LEDGE && d == RDIR_SOUTH)
                    continue;
                if (owner[v] == 0)
                {
                    owner[v] = owner[u];
                    if (tail - head < BFS_RING)
                        sBfsQueue[tail++ & (BFS_RING - 1)] = v;
                }
                else if (owner[v] != owner[u])
                {
                    // Plant on whichever is plain open ground.
                    if (gen->g[v] == C_OPEN && !IsEdge(gen, vx, vy))
                        cut = v;
                    else if (gen->g[u] == C_OPEN && !IsEdge(gen, ux, uy))
                        cut = u;
                    else
                        return FALSE;
                }
            }
        }
        if (cut < 0)
            return TRUE;
        gen->g[cut] = C_TREE;
    }
    return FALSE;
}

// Scratch for a try, in the carved walk's buffer (gen->walk is only filled
// again by FindShortestWalk, once the map is drawn; the generator's stack
// is too small for these): the cells, the road and the level bits as they
// were, and a bitmap.
#define PLATEAU_SAVED_CELLS(gen) ((u8 *)(gen)->walk)
#define PLATEAU_SAVED_PATH(gen) ((gen)->walk + MAX_CELLS / 2)
#define PLATEAU_SAVED_UPPER(gen) ((u8 *)((gen)->walk + MAX_CELLS / 2 + MAX_PATH))
#define PLATEAU_RING(gen) (PLATEAU_SAVED_UPPER(gen) + MAX_CELLS / 8 + 2)

// Try the road's stretch from path index i + 1 (the step up) to j (the
// step down) as a plateau. PlanPlateau puts everything back on failure.
static bool8 TryPlateau(struct Gen *gen, int i, int j, int nMain, int ax, int ay, u8 savedStairs, bool8 wasTerraced,
                        const u8 *box)
{
    bool8 filled = TRUE, ok = TRUE;
    int k, x, y, dx, dy, round, base = 0;

    memset(gen->grassZone, 0, sizeof(gen->grassZone));
    // A planned plateau's whole box (PlateauCarve): x0, y0, x1, y1.
    if (box != NULL)
        for (y = box[1]; y <= box[3]; y++)
            for (x = box[0]; x <= box[2]; x++)
                if (PlateauFree(gen, x, y))
                    SetBitU(gen->grassZone, gen, x, y);
    // The stretch, and a ring round it.
    for (k = i + 1; k <= j && ok; k++)
    {
        x = gen->path[k] % gen->cw;
        y = gen->path[k] / gen->cw;
        if (IsEdge(gen, x, y) || NearExitU(gen, x, y, 2) || (wasTerraced && IsStairU(gen, x, y)))
            ok = FALSE;
        SetBitU(gen->grassZone, gen, x, y);
    }
    for (k = i + 1; k <= j && ok; k++)
    {
        x = gen->path[k] % gen->cw;
        y = gen->path[k] / gen->cw;
        for (dy = -1; dy <= 1 && ok; dy++)
            for (dx = -1; dx <= 1; dx++)
            {
                int nx = x + dx, ny = y + dy;
                u8 c = Cell(gen, nx, ny);
                if (BitU(gen->grassZone, gen, nx, ny) || c == C_ROAD)
                    continue;
                // (A ledge, a fence or a house right by the stretch: not here.)
                if (c != C_OPEN && c != C_TREE && c != C_CLIFF)
                {
                    ok = FALSE;
                    break;
                }
                if (PlateauFree(gen, nx, ny))
                    SetBitU(gen->grassZone, gen, nx, ny);
            }
    }
    // A second ring: the open ground round it, and some of the forest, so
    // the top is wide as Hoenn's are.
    if (ok)
    {
        u8 *ring = PLATEAU_RING(gen);
        memcpy(ring, gen->grassZone, MAX_CELLS / 8 + 1);
        for (y = 1; y < gen->ch - 1; y++)
            for (x = 1; x < gen->cw - 1; x++)
            {
                bool8 by = FALSE;
                if (BitU(ring, gen, x, y) || !PlateauFree(gen, x, y))
                    continue;
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++)
                        if (BitU(ring, gen, x + dx, y + dy))
                            by = TRUE;
                if (by && (Cell(gen, x, y) == C_OPEN || RogueRng_Hash(gen->rng, 7100 + y * gen->cw + x) % 100 < 45))
                    SetBitU(gen->grassZone, gen, x, y);
            }
        for (y = 0; y < gen->ch; y++)
            for (x = 0; x < gen->cw; x++)
                if (BitU(gen->grassZone, gen, x, y) && Cell(gen, x, y) == C_TREE)
                    SetCell(gen, x, y, C_OPEN);
    }
    // Its level: one above the highest ground beside it.
    if (ok)
    {
        gen->terraced = TRUE;
        for (y = 0; y < gen->ch; y++)
            for (x = 0; x < gen->cw; x++)
            {
                if (!BitU(gen->grassZone, gen, x, y))
                    continue;
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++)
                        if (InBounds(gen, x + dx, y + dy) && !BitU(gen->grassZone, gen, x + dx, y + dy)
                         && IsOpenU(Cell(gen, x + dx, y + dy)) && UpperU(gen, x + dx, y + dy))
                            base = 1;
            }
        if (!wasTerraced)
            memset(gen->upper, 0, sizeof(gen->upper));
        for (y = 0; y < gen->ch; y++)
            for (x = 0; x < gen->cw; x++)
                if (BitU(gen->grassZone, gen, x, y))
                {
                    if (base)
                        SetBitU(gen->upper, gen, x, y);
                    else
                        ClearBitU(gen->upper, gen, x, y);
                }
        if (gen->nStairs + 2 > 4)
            ok = FALSE;
        else
        {
            gen->stairX[gen->nStairs] = gen->path[i + 1] % gen->cw;
            gen->stairY[gen->nStairs++] = gen->path[i + 1] / gen->cw;
            gen->stairX[gen->nStairs] = gen->path[j] % gen->cw;
            gen->stairY[gen->nStairs++] = gen->path[j] / gen->cw;
        }
    }
    // Pare it: no unit with lower ground on opposite sides, and none cut off.
    for (round = 0; round < 6 && ok && filled; round++)
    {
        bool8 pared = TRUE;
        while (pared && ok)
        {
            pared = FALSE;
            for (y = 0; y < gen->ch && ok; y++)
                for (x = 0; x < gen->cw; x++)
                {
                    if (!BitU(gen->grassZone, gen, x, y) || !PlateauThin(gen, x, y))
                        continue;
                    if (Cell(gen, x, y) == C_ROAD)
                    {
                        ok = FALSE;
                        break;
                    }
                    ClearBitU(gen->grassZone, gen, x, y);
                    ClearBitU(gen->upper, gen, x, y);
                    SetCell(gen, x, y, C_TREE);
                    pared = TRUE;
                }
        }
        if (!ok)
            break;
        // Each step: plateau beside it, lower ground below, and the ground
        // above it its own rock top.
        for (k = savedStairs; k < gen->nStairs && ok; k++)
        {
            x = gen->stairX[k];
            y = gen->stairY[k];
            if (!PlateauU(gen, x - 1, y) || !PlateauU(gen, x + 1, y) || !PlateauU(gen, x, y - 1) || PlateauU(gen, x, y + 1))
                ok = FALSE;
        }
        if (!ok)
            break;
        if (round == 0)
            PlateauOpenSkirt(gen);
        if (!PassBlocksJoin(gen, &filled))
            ok = FALSE;
        // (Plateau filled in as cut off is forest again.)
        for (y = 0; y < gen->ch; y++)
            for (x = 0; x < gen->cw; x++)
                if (BitU(gen->grassZone, gen, x, y) && Cell(gen, x, y) == C_TREE)
                {
                    ClearBitU(gen->grassZone, gen, x, y);
                    ClearBitU(gen->upper, gen, x, y);
                }
    }
    if (ok && filled)
        ok = FALSE;
    // No way round below it (PlateauCutBypass), and what that shut off is
    // filled in.
    if (ok && !PlateauCutBypass(gen, i, j, nMain))
        ok = FALSE;
    if (ok)
    {
        if (!PassBlocksJoin(gen, &filled))
            ok = FALSE;
        else if (filled && (!PassBlocksJoin(gen, &filled) || filled))
            ok = FALSE;
    }
    // The whole road is still walked, and only over the top: with the steps
    // gone, the way on is out of reach.
    if (ok)
    {
        FloodReachable(gen, ax, ay);
        for (k = 0; k < gen->nPath && ok; k++)
            if (!Reached(gen, gen->path[k] % gen->cw, gen->path[k] / gen->cw))
                ok = FALSE;
    }
    if (ok)
    {
        u8 kept = gen->nStairs;
        gen->nStairs = savedStairs;
        FloodReachable(gen, ax, ay);
        if (Reached(gen, gen->path[nMain - 1] % gen->cw, gen->path[nMain - 1] / gen->cw))
            ok = FALSE;
        gen->nStairs = kept;
    }
    if (!ok)
        return FALSE;
    // Units with an edge hold nobody (as a terrace's rim).
    for (y = 0; y < gen->ch; y++)
        for (x = 0; x < gen->cw; x++)
        {
            int q;
            bool8 rim = FALSE;
            if (!PlateauU(gen, x, y) || Cell(gen, x, y) != C_OPEN)
                continue;
            for (q = 0; q < 4; q++)
                if (PlateauRimBlock(gen, x * 2 + (q & 1), y * 2 + (q >> 1)))
                    rim = TRUE;
            if (rim)
                SetCell(gen, x, y, C_CLIFF);
        }
    return TRUE;
}

// The road of a mountain pass planned round a plateau (PlateauCarve): a
// list of corners, each leg straight across or up and down.
struct PlateauWay
{
    u8 x[10], y[10];
    u8 n;
};

static void PlateauWayAdd(struct PlateauWay *w, int x, int y)
{
    if (w->n > 0 && w->x[w->n - 1] == x && w->y[w->n - 1] == y)
        return;
    w->x[w->n] = x;
    w->y[w->n++] = y;
}

// The k-th unit along the way (k from 0), or FALSE past its end.
static bool8 PlateauWayUnit(const struct PlateauWay *w, int k, int *ux, int *uy)
{
    int s, x, y;
    for (s = 0; s + 1 < w->n; s++)
    {
        int len = Abs(w->x[s + 1] - w->x[s]) + Abs(w->y[s + 1] - w->y[s]);
        if (k < len)
        {
            x = w->x[s] + (w->x[s + 1] > w->x[s] ? k : w->x[s + 1] < w->x[s] ? -k : 0);
            y = w->y[s] + (w->y[s + 1] > w->y[s] ? k : w->y[s + 1] < w->y[s] ? -k : 0);
            *ux = x;
            *uy = y;
            return TRUE;
        }
        k -= len;
    }
    if (k == 0 && w->n > 0)
    {
        *ux = w->x[w->n - 1];
        *uy = w->y[w->n - 1];
        return TRUE;
    }
    return FALSE;
}

// Carve a mountain pass's road round a plateau planned now, as Jagged Pass
// and Route 112 are walked: from the way in to the foot of one stone stair,
// up it and across the top, down the other and on to the way out, going
// round the plateau's side where an end lies above it. The plan waits in the
// stair lists for PlanPlateau. FALSE (nothing carved) where none fits.
static bool8 PlateauCarve(struct Gen *gen, int ax, int ay, int bx, int by, int top)
{
    const struct RoguePlace *p = gen->p;
    int tries;
    // (Not the volcano's foot route, whose mountain is the volcano's own:
    // rogue_gen_volcano.h.)
    if (p->type != RROUTE_HILLS || p->tier == 0 || gen->forestCut || gen->attempt >= 8 || gen->ashFade
     || VolcanoMountainAt(gen->r, p - gen->r->places)
     || RogueRng_Hash(gen->r->seed, 7300 + (u32)(p - gen->r->places)) % 100 < 12)
        return FALSE;
    for (tries = 0; tries < 24; tries++)
    {
        struct PlateauWay w;
        int len = 5 + RAND(gen->cw > 20 ? 6 : 4), h = 2 + RAND(2);
        int xs1, xs2, yS, yT, x0, x1, xin, xout, k, n, ux, uy, k2;
        bool8 ok = TRUE;
        YIELD();
        if (len > gen->cw - 9)
            len = gen->cw - 9;
        if (len < 5 || gen->ch - top - 5 - h < 1)
            return FALSE;
        xs1 = 4 + RAND(gen->cw - 8 - len + 1);
        xs2 = xs1 + len - 1;
        yS = top + 3 + h - 2 + RAND(gen->ch - top - 7 - (h - 2));
        yT = yS - h;
        x0 = xs1 - 1;
        x1 = xs2 + 1;
        if (yT < 2 || yT < top + 1 || yS + 1 > gen->ch - 3 || x0 < 3 || x1 > gen->cw - 4)
            continue;
        // The stair on the way in's side.
        if (ax == bx ? RAND(2) : Abs(ax - xs1) < Abs(ax - xs2))
            xin = xs1, xout = xs2;
        else
            xin = xs2, xout = xs1;
        w.n = 0;
        PlateauWayAdd(&w, ax, ay);
        if (ay >= yS + 1)
            PlateauWayAdd(&w, ax, yS + 1);
        else
        {
            int c = xin == xs1 ? x0 - 1 : x1 + 1;
            PlateauWayAdd(&w, c, ay);
            PlateauWayAdd(&w, c, yS + 1);
        }
        PlateauWayAdd(&w, xin, yS + 1);
        PlateauWayAdd(&w, xin, yS);
        PlateauWayAdd(&w, xout, yS);
        PlateauWayAdd(&w, xout, yS + 1);
        if (by >= yS + 1)
            PlateauWayAdd(&w, bx, yS + 1);
        else
        {
            int c = xout == xs1 ? x0 - 1 : x1 + 1;
            PlateauWayAdd(&w, c, yS + 1);
            PlateauWayAdd(&w, c, by);
        }
        PlateauWayAdd(&w, bx, by);
        // Every leg straight, every unit forest (but the ends), none twice,
        // and the box kept for the plateau: only the top's own road in it.
        for (k = 0; k + 1 < w.n && ok; k++)
            if (w.x[k] != w.x[k + 1] && w.y[k] != w.y[k + 1])
                ok = FALSE;
        for (n = 0; ok && PlateauWayUnit(&w, n, &ux, &uy); n++)
        {
            bool8 inBox = ux >= x0 && ux <= x1 && uy >= yT - 1 && uy <= yS;
            bool8 onTop = uy == yS && ux >= xs1 && ux <= xs2;
            if (n == 0 || !PlateauWayUnit(&w, n + 1, &k2, &k2))
                continue;
            u8 c = Cell(gen, ux, uy);
            // (Forest, or the kept ground under a cave's hill.)
            if (IsEdge(gen, ux, uy) || (c != C_TREE && c != C_KEEP && c != C_OPEN) || BitU(gen->keep, gen, ux, uy)
             || (inBox && !onTop))
                ok = FALSE;
        }
        for (k = 0; ok && PlateauWayUnit(&w, k, &ux, &uy); k++)
            for (k2 = k + 1; ok && PlateauWayUnit(&w, k2, &x0, &yT); k2++)
                if (x0 == ux && yT == uy)
                    ok = FALSE;
        x0 = xs1 - 1;
        yT = yS - h;
        for (uy = yT - 1; uy <= yS && ok; uy++)
            for (ux = x0 - 1; ux <= x1 + 1 && ok; ux++)
                if (Cell(gen, ux, uy) != C_TREE || NearExitU(gen, ux, uy, 2) || BitU(gen->keep, gen, ux, uy))
                    ok = FALSE;
        if (!ok)
            continue;
        for (n = 0; PlateauWayUnit(&w, n, &ux, &uy); n++)
            AddRoadCell(gen, ux, uy);
        gen->stairX[0] = xin;
        gen->stairY[0] = yS;
        gen->stairX[1] = xout;
        gen->stairY[1] = yS;
        gen->stairX[2] = x0;
        gen->stairY[2] = yT;
        gen->stairX[3] = x1;
        gen->stairY[3] = 0xA5;
        return TRUE;
    }
    return FALSE;
}

// Lift the road's units from index a + 1 to b - 1 (a straight run across)
// a unit north, so it climbs at a and comes down at b: the old units are
// open ground (the plateau's foot), the new ones road. FALSE if the ground
// there is not forest or open ground.
static bool8 PlateauLift(struct Gen *gen, int a, int b)
{
    int k, n = b - a + 1, x, y;
    for (k = a; k <= b; k++)
    {
        u8 c;
        x = gen->path[k] % gen->cw;
        y = gen->path[k] / gen->cw - 1;
        c = Cell(gen, x, y);
        if ((c != C_OPEN && c != C_TREE) || IsEdge(gen, x, y) || y < 2 || BitU(gen->keep, gen, x, y))
            return FALSE;
    }
    if (gen->nPath + 2 > MAX_PATH)
        return FALSE;
    for (k = a + 1; k < b; k++)
        SetCell(gen, gen->path[k] % gen->cw, gen->path[k] / gen->cw, C_OPEN);
    // The units after b move up two places.
    memmove(&gen->path[b + 2], &gen->path[b], (gen->nPath - b) * sizeof(gen->path[0]));
    gen->nPath += 2;
    for (k = 0; k < n; k++)
    {
        // (path[b + 2] is the old path[b], still in place.)
        u16 u = (k == 0 ? gen->path[a] : k == n - 1 ? gen->path[b + 2] : PLATEAU_SAVED_PATH(gen)[a + k]) - gen->cw;
        gen->path[a + 1 + k] = u;
        SetCell(gen, u % gen->cw, u / gen->cw, C_ROAD);
    }
    return TRUE;
}

// A mountain pass's plateau, once the road and everything beside it is
// laid: a stretch of the main road between a step north and a step south,
// where the road already goes up and down so, or a straight run across that
// is lifted a unit to do so.
static bool8 PlanPlateau(struct Gen *gen, int ax, int ay)
{
    u16 cand[24];
    int nMain, nCand = 0, i, j, tries, lo, n = gen->cw * gen->ch, nPath = gen->nPath;
    const struct RoguePlace *p = gen->p;
    u8 savedStairs = gen->nStairs, plan[8];
    bool8 wasTerraced = gen->terraced, done = FALSE;
    // PlateauCarve's plan, kept in the stair lists until now.
    for (i = 0; i < 4; i++)
    {
        plan[i * 2] = gen->stairX[i];
        plan[i * 2 + 1] = gen->stairY[i];
        gen->stairX[i] = gen->stairY[i] = 0;
    }
    if (p->type != RROUTE_HILLS || p->tier == 0 || gen->forestCut || gen->nPath < 10 || gen->attempt >= 12
     || gen->ashFade || gen->terraced || VolcanoMountainAt(gen->r, p - gen->r->places))
        return FALSE;
    for (nMain = 1; nMain < gen->nPath; nMain++)
        if (Abs(gen->path[nMain] % gen->cw - gen->path[nMain - 1] % gen->cw)
          + Abs(gen->path[nMain] / gen->cw - gen->path[nMain - 1] / gen->cw) != 1)
            break;
    lo = gen->guardEnd ? gen->guardEnd + 3 : 2;
    // Where the road goes up and comes down again (0x8000 | i | j << 8)...
    for (i = lo; i + 2 < nMain && nCand < 12; i++)
    {
        if (PlateauStepDir(gen, i) != RDIR_NORTH)
            continue;
        for (j = i + 1; j + 2 < nMain; j++)
            if (PlateauStepDir(gen, j) == RDIR_SOUTH)
                break;
        if (j + 2 < nMain && j - i >= PLATEAU_MIN_STRETCH && j < 128)
            cand[nCand++] = 0x8000 | i | (j << 8);
    }
    // ... and straight runs across to lift (a | b << 8).
    for (i = lo; i + 2 < nMain && nCand < 24; i++)
    {
        u8 d = PlateauStepDir(gen, i);
        if (d != RDIR_EAST && d != RDIR_WEST)
            continue;
        for (j = i + 1; j + 1 < nMain && PlateauStepDir(gen, j) == d; j++)
            ;
        // Units path[i..j] in a row; the lift leaves the first and the last
        // two (the road comes in along the foot of the face, and goes on
        // along it a little before it turns).
        if (j - i >= PLATEAU_MIN_STRETCH + 2)
        {
            int a = i + 1 + RAND(j - i - PLATEAU_MIN_STRETCH - 1), b;
            b = j - 2 - RAND(j - 2 - (a + PLATEAU_MIN_STRETCH - 1) + 1);
            if (b - a >= PLATEAU_MIN_STRETCH - 1 && b < 127)
                cand[nCand++] = a | (b << 8);
        }
        i = j - 1;
    }
    // The plateau PlateauCarve planned the road for, first.
    if (plan[7] == 0xA5)
    {
        int si = -1, so = -1;
        for (i = 0; i < nMain; i++)
        {
            if (gen->path[i] == plan[1] * gen->cw + plan[0])
                si = i;
            if (gen->path[i] == plan[3] * gen->cw + plan[2])
                so = i;
        }
        if (si >= 2 && so > si + 1 && so + 2 < nMain && PlateauStepDir(gen, si - 1) == RDIR_NORTH
         && PlateauStepDir(gen, so) == RDIR_SOUTH)
        {
            u8 box[4];
            box[0] = plan[4];
            box[1] = plan[5];
            box[2] = plan[6];
            box[3] = plan[1];
            memcpy(PLATEAU_SAVED_CELLS(gen), gen->g, n);
            memcpy(PLATEAU_SAVED_PATH(gen), gen->path, nPath * sizeof(gen->path[0]));
            memcpy(PLATEAU_SAVED_UPPER(gen), gen->upper, MAX_CELLS / 8 + 1);
            done = TryPlateau(gen, si - 1, so, nMain, ax, ay, savedStairs, wasTerraced, box);
            if (!done)
            {
                memcpy(gen->g, PLATEAU_SAVED_CELLS(gen), n);
                memcpy(gen->upper, PLATEAU_SAVED_UPPER(gen), MAX_CELLS / 8 + 1);
                memset(gen->grassZone, 0, sizeof(gen->grassZone));
                gen->nStairs = savedStairs;
                gen->terraced = wasTerraced;
            }
        }
    }
    for (tries = 0; tries < 10 && nCand > 0 && !done; tries++)
    {
        int k = RAND(nCand), c = cand[k], a = c & 0xFF, b = (c >> 8) & 0x7F, m = nMain;
        YIELD();
        cand[k] = cand[--nCand];
        memcpy(PLATEAU_SAVED_CELLS(gen), gen->g, n);
        memcpy(PLATEAU_SAVED_PATH(gen), gen->path, nPath * sizeof(gen->path[0]));
        memcpy(PLATEAU_SAVED_UPPER(gen), gen->upper, MAX_CELLS / 8 + 1);
        if (c & 0x8000)
            done = TryPlateau(gen, a, b, m, ax, ay, savedStairs, wasTerraced, NULL);
        else if (PlateauLift(gen, a, b))
        {
            m += 2;
            done = TryPlateau(gen, a, b + 1, m, ax, ay, savedStairs, wasTerraced, NULL);
        }
        if (!done)
        {
            memcpy(gen->g, PLATEAU_SAVED_CELLS(gen), n);
            memcpy(gen->path, PLATEAU_SAVED_PATH(gen), nPath * sizeof(gen->path[0]));
            memcpy(gen->upper, PLATEAU_SAVED_UPPER(gen), MAX_CELLS / 8 + 1);
            memset(gen->grassZone, 0, sizeof(gen->grassZone));
            gen->nPath = nPath;
            gen->nStairs = savedStairs;
            gen->terraced = wasTerraced;
        }
    }
    // Dead ends now in the rock hold no items.
    for (i = 0, j = 0; i < gen->nPockets; i++)
        if (Cell(gen, gen->pocketX[i], gen->pocketY[i]) == C_OPEN)
        {
            gen->pocketX[j] = gen->pocketX[i];
            gen->pocketY[j++] = gen->pocketY[i];
        }
    gen->nPockets = j;
    // Until the ground cover is laid, all of it is held as a rim is, so no
    // house, ledge or garden stands on the rock (PlateauFinish).
    for (i = 0; i < n && done; i++)
        if (gen->g[i] == C_OPEN && BitU(gen->grassZone, gen, i % gen->cw, i / gen->cw))
            gen->g[i] = C_CLIFF;
    return done;
}

// Before the ground cover is tidied: the plateau is bare rock, with no
// grass, path or flowers, and its units with no edge are open to people
// again.
static void PlateauFinish(struct Gen *gen)
{
    int fx, fy, x, y, q;
    if (gen->p->kind != RPLACE_ROUTE || gen->p->type != RROUTE_HILLS || !gen->terraced)
        return;
    for (y = 0; y < gen->ch; y++)
        for (x = 0; x < gen->cw; x++)
        {
            bool8 rim = FALSE;
            if (!PlateauU(gen, x, y) || Cell(gen, x, y) != C_CLIFF)
                continue;
            for (q = 0; q < 4; q++)
                if (PlateauRimBlock(gen, x * 2 + (q & 1), y * 2 + (q >> 1)))
                    rim = TRUE;
            if (!rim)
                SetCell(gen, x, y, C_OPEN);
        }
    // A boulder or two out on the top, as on Route 111's (solid, the rock
    // top all round it), each leaving everything reachable as it was.
    {
        static const u16 sPlateauBoulder[4] = { 0x0493, 0x0494, 0x049B, 0x049C };
        int tries, placed = 0, want = 1 + RAND(3), ax = gen->path[0] % gen->cw, ay = gen->path[0] / gen->cw;
        for (tries = 0; tries < 40 && placed < want && gen->nDecor < MAX_DECOR; tries++)
        {
            int before, dx, dy;
            bool8 ok = TRUE;
            x = 1 + RAND(gen->cw - 2);
            y = 1 + RAND(gen->ch - 2);
            if (!PlateauU(gen, x, y) || Cell(gen, x, y) != C_OPEN)
                continue;
            for (dy = -1; dy <= 1; dy++)
                for (dx = -1; dx <= 1; dx++)
                    if (Cell(gen, x + dx, y + dy) == C_DECOR || IsStairU(gen, x + dx, y + dy) || !PlateauU(gen, x + dx, y + dy))
                        ok = FALSE;
            if (!ok)
                continue;
            before = FloodReachable(gen, ax, ay);
            SetCell(gen, x, y, C_DECOR);
            if (FloodReachable(gen, ax, ay) != before - 1 || !AddDecor(gen, sPlateauBoulder, x * 2, y * 2, 2, 2))
            {
                SetCell(gen, x, y, C_OPEN);
                continue;
            }
            placed++;
        }
    }
    // (The way's cover stays under the steps, so the path below them runs
    // on up to them, as onto a pass's steps.)
    for (fy = 0; fy < gen->m->h; fy++)
    {
        YIELD();
        for (fx = 0; fx < gen->m->w; fx++)
            if (PlateauU(gen, fx >> 1, fy >> 1) && !((fy & 1) && IsStairU(gen, fx >> 1, fy >> 1)))
                SetCover(gen, fx, fy, V_NONE);
    }
}

// Draw a plateau unit: rock top, its edges against lower ground (with a
// tree's tips on a face over the tree's crown, as RenderPassRims does), and
// its steps.
static void RenderPlateauUnit(struct Gen *gen, const u8 *level, int ux, int uy)
{
    int q, w = gen->m->w;
    for (q = 0; q < 4; q++)
    {
        int fx = ux * 2 + (q & 1), fy = uy * 2 + (q >> 1);
        u8 k = PlateauKey(gen, level, ux, uy, q);
        u16 block;
        if (q >= 2 && IsStairU(gen, ux, uy))
        {
            Put(gen, fx, fy, (q & 1) ? 0x30CF : 0x30AF);
            continue;
        }
        if (k == MK_C)
        {
            Put(gen, fx, fy, 0x3071);
            continue;
        }
        block = sRockOnGround[k];
        if (k == MK_B && fy + 1 < gen->m->h)
        {
            u16 t = gen->m->blocks[(fy + 1) * w + fx] & 0x3FF;
            if ((q & 1) ? (t == 0x1D5 || t == 0x1D7) : (t == 0x1D4 || t == 0x1D6))
                block = 0x0400 | (0x1F0 + (q & 1));
        }
        Put(gen, fx, fy, block);
    }
}

// Does unit (x, y) of a shelf's upper ground have a rim block, or steps?
static bool8 ShelfRimU(struct Gen *gen, int x, int y)
{
    int q;
    if (!PassWalkU(gen, x, y) || TerraceLevelU(gen, x, y) == 0)
        return FALSE;
    if (IsStairU(gen, x, y))
        return TRUE;
    for (q = 0; q < 4; q++)
        if (PassRimBlock(gen, x * 2 + (q & 1), y * 2 + (q >> 1)))
            return TRUE;
    return FALSE;
}

// A shelf's rims end in their corners on grass, as Hoenn's: a tree beside
// a rim (Hoenn has no piece for a rim against a tree's side) becomes grass
// on the lower level, so the rim turns its corner there; beside the steps,
// grass on the upper level, so the rim runs on past them first. Until no rim
// meets a tree but the map's edge.
static bool8 ShelfRimEnds(struct Gen *gen)
{
    int round, x, y, side;
    bool8 changed = TRUE;
    for (round = 0; round < 12 && changed; round++)
    {
        changed = FALSE;
        for (y = 1; y < gen->ch - 1; y++)
            for (x = 1; x < gen->cw - 1; x++)
            {
                if (!ShelfRimU(gen, x, y))
                    continue;
                for (side = -1; side <= 1; side += 2)
                {
                    int tx = x + side;
                    if (Cell(gen, tx, y) != C_TREE || IsEdge(gen, tx, y) || BitU(gen->keep, gen, tx, y))
                        continue;
                    SetCell(gen, tx, y, C_OPEN);
                    if (!IsStairU(gen, x, y))
                        ClearBitU(gen->upper, gen, tx, y);
                    changed = TRUE;
                }
                // A rim's corner stands on grass too (a tree's crown under
                // it has no piece); a straight rim over a tree has its tips.
                if (Cell(gen, x, y + 1) == C_TREE && !IsEdge(gen, x, y + 1) && !BitU(gen->keep, gen, x, y + 1)
                 && (PassRimBlock(gen, x * 2, y * 2 + 1) || PassRimBlock(gen, x * 2 + 1, y * 2 + 1))
                 && (PassNbLevel(gen, NULL, x - 1, y, PASS_SIDE, 1) < TerraceLevelU(gen, x, y)
                  || PassNbLevel(gen, NULL, x + 1, y, PASS_SIDE, 1) < TerraceLevelU(gen, x, y)))
                {
                    SetCell(gen, x, y + 1, C_OPEN);
                    changed = TRUE;
                }
            }
    }
    // Rim units hold nobody, and ground now cut off is filled in.
    for (y = 0; y < gen->ch; y++)
        for (x = 0; x < gen->cw; x++)
            if (Cell(gen, x, y) == C_OPEN && ShelfRimU(gen, x, y) && !IsStairU(gen, x, y))
                SetCell(gen, x, y, C_CLIFF);
    // (Grass opened where no one can walk stays grass: filled in, it would
    // be a tree beside the rim again.)
    return round < 12;
}

// (v1.1 joins) A shelf too thin to read as one: under half of its upper
// ground at least three units across both ways (a strip two units wide
// reads as a chute), or cut by an inlet of the lower ground one or two
// units wide. Mossdeep's and Route 116's shelves are deep.
#define SHELF_MIN_DEPTH 3
static bool8 ShelfUpOpen(struct Gen *gen, int x, int y)
{
    return InBounds(gen, x, y) && IsOpenU(Cell(gen, x, y)) && UpperU(gen, x, y);
}

static int ShelfRun(struct Gen *gen, int x, int y, int dx, int dy)
{
    int n = 1, k;
    for (k = 1; ShelfUpOpen(gen, x + k * dx, y + k * dy); k++)
        n++;
    for (k = 1; ShelfUpOpen(gen, x - k * dx, y - k * dy); k++)
        n++;
    return n;
}

static bool8 ShelfThin(struct Gen *gen)
{
    int x, y, all = 0, deep = 0;
    for (y = 0; y < gen->ch; y++)
        for (x = 0; x < gen->cw; x++)
            if (ShelfUpOpen(gen, x, y))
            {
                all++;
                if (ShelfRun(gen, x, y, 1, 0) >= SHELF_MIN_DEPTH && ShelfRun(gen, x, y, 0, 1) >= SHELF_MIN_DEPTH)
                    deep++;
            }
    if (deep * 2 < all || deep < 8)
        return TRUE;
    for (y = 0; y < gen->ch; y++)
        for (x = 1; x < gen->cw - 2; x++)
            if (IsOpenU(Cell(gen, x, y)) && !UpperU(gen, x, y) && ShelfUpOpen(gen, x - 1, y)
             && (ShelfUpOpen(gen, x + 1, y) || (!UpperU(gen, x + 1, y) && ShelfUpOpen(gen, x + 2, y))))
                return TRUE;
    return FALSE;
}

// A forest's or a field's shelf (PlanPass), kept only where its rims run
// for a good few units: a scrap of rim by a house or a tree reads as a
// mistake, not a step in the land.
#define SHELF_MIN_RIM 6
#define SHELF_MIN_UPPER 16 // open units up on the shelf, at the least
static void PlanShelf(struct Gen *gen, int ax, int ay)
{
    int n = gen->cw * gen->ch, i, rim = 0, nPockets = gen->nPockets;
    u8 px[MAX_POCKETS], py[MAX_POCKETS];
    bool8 bad = FALSE;
    memcpy(PLATEAU_SAVED_CELLS(gen), gen->g, n);
    memcpy(px, gen->pocketX, sizeof(px));
    memcpy(py, gen->pocketY, sizeof(py));
    PlanPass(gen, ax, ay);
    if (!gen->terraced)
        return;
    if (!ShelfRimEnds(gen))
        bad = TRUE;
    // A rim ends in its corner on grass, as Route 116's: never against a
    // tree's side, which no Hoenn piece joins; nor has it lower ground on
    // both sides, all of it edge.
    for (i = 0; i < n && !bad; i++)
    {
        int x = i % gen->cw, y = i / gen->cw;
        if (!ShelfRimU(gen, x, y))
            continue;
        rim++;
        if (TerraceLevelU(gen, x, y) > PassNbLevel(gen, NULL, x - 1, y, PASS_SIDE, 1)
         && TerraceLevelU(gen, x, y) > PassNbLevel(gen, NULL, x + 1, y, PASS_SIDE, 1))
            bad = TRUE;
        if ((Cell(gen, x - 1, y) == C_TREE && !IsEdge(gen, x - 1, y)) || (Cell(gen, x + 1, y) == C_TREE && !IsEdge(gen, x + 1, y)))
            bad = TRUE;
    }
    // The whole road still walked.
    if (!bad)
    {
        FloodReachable(gen, ax, ay);
        for (i = 0; i < gen->nPath && !bad; i++)
            if (!Reached(gen, gen->path[i] % gen->cw, gen->path[i] / gen->cw))
                bad = TRUE;
    }
    // (And room up there: a strip round the trail's way in reads as a
    // chute, not a shelf.)
    for (i = 0, n = 0; i < gen->cw * gen->ch && !bad; i++)
        if (IsOpenU(gen->g[i]) && UpperU(gen, i % gen->cw, i / gen->cw))
            n++;
    if (n < SHELF_MIN_UPPER)
        bad = TRUE;
    if (!bad && ShelfThin(gen))
        bad = TRUE;
    n = gen->cw * gen->ch;
    if (!bad && rim >= SHELF_MIN_RIM)
        return;
    memcpy(gen->g, PLATEAU_SAVED_CELLS(gen), n);
    memcpy(gen->pocketX, px, sizeof(px));
    memcpy(gen->pocketY, py, sizeof(py));
    gen->nPockets = nPockets;
    memset(gen->upper, 0, sizeof(gen->upper));
    memset(gen->grassZone, 0, sizeof(gen->grassZone));
    gen->terraced = FALSE;
    gen->nStairs = 0;
}

// A rim's piece of rock: with the sand foot (Route 111's, Route 118's rim
// above its sand) where the ground it drops to is plain sand, the middle of
// a sand road or a beach.
static u16 RimFoot(struct Gen *gen, u8 k, int fx, int fy)
{
    int dx = (k == MK_L || k == MK_TL || k == MK_BL || k == MK_NW || k == MK_SW) ? -1
           : (k == MK_R || k == MK_TR || k == MK_BR || k == MK_NE || k == MK_SE) ? 1 : 0;
    int dy = (k == MK_B || k == MK_BL || k == MK_BR || k == MK_SW || k == MK_SE) ? 1
           : (k == MK_T || k == MK_TL || k == MK_TR || k == MK_NW || k == MK_NE) ? -1 : 0;
    // (A corner by its side below, as Route 111's sand foot runs.)
#define PLAIN_SAND(b) (((b) & 0x3FF) == 0x121 || ((b) & 0x3FF) == 0x124)
    if (dy != 0 && InMap(gen, fx, fy + dy) && PLAIN_SAND(gen->m->blocks[(fy + dy) * gen->m->w + fx]))
        return sRockOnSand[k];
    if (dx != 0 && InMap(gen, fx + dx, fy) && PLAIN_SAND(gen->m->blocks[fy * gen->m->w + fx + dx]))
        return sRockOnSand[k];
#undef PLAIN_SAND
    return sRockOnGround[k];
}

// ---------------------------------------------------------------------------
// Rock joins

#include "data/rogue_rock_pairs.h"

static int RockPairIndex(u16 t)
{
    int i;
    for (i = 0; i < ROCK_PAIR_PIECES; i++)
        if (sRockPairPieces[i] == t)
            return i;
    return -1;
}

// How many of the neighbours of block (fx, fy), were it rock piece k, make a
// pair no Hoenn map has. Off the map, or a tileset's own block: no matter.
static int RockPairMisses(struct Gen *gen, int fx, int fy, int k)
{
    int d, miss = 0;
    for (d = 0; d < 4; d++)
    {
        int nx = fx + sDirDX[d], ny = fy + sDirDY[d];
        u16 t;
        if (!InMap(gen, nx, ny))
            continue;
        t = gen->m->blocks[ny * gen->m->w + nx] & 0x3FF;
        if (t >= 0x200)
            continue;
        if (!((sRockPairs[k][d][t >> 3] >> (t & 7)) & 1))
            miss++;
    }
    return miss;
}

// A rocky route's rock, once drawn: a solid rock block that meets a
// neighbour in a pair no Hoenn map has (where two levels' edges meet, a
// face over a tree, a corner by a side) becomes the piece that meets all
// four as Hoenn's maps do; failing that, it and a solid rock block beside it
// become two pieces that do. Only solid blocks change, into solid ones, so
// no way opens or shuts.
static void RockRepair(struct Gen *gen)
{
    int pass, fx, fy, w = gen->m->w;
    for (pass = 0; pass < 3; pass++)
    {
        bool8 changed = FALSE;
        for (fy = 0; fy < gen->m->h; fy++)
        {
            YIELD();
            for (fx = 0; fx < w; fx++)
            {
                u16 b = gen->m->blocks[fy * w + fx];
                int k = ((b >> 10) & 3) ? RockPairIndex(b & 0x3FF) : -1, c, d, c2;
                bool8 done = FALSE;
                if (k < 0 || RockPairMisses(gen, fx, fy, k) == 0)
                    continue;
                for (c = 0; c < ROCK_PAIR_PIECES && !done; c++)
                {
                    if (c == k || RockPairMisses(gen, fx, fy, c) != 0)
                        continue;
                    Put(gen, fx, fy, (b & ~0x3FF) | sRockPairPieces[c]);
                    done = TRUE;
                }
                // Two at once: this block and a solid rock neighbour.
                for (d = 0; d < 4 && !done; d++)
                {
                    int nx = fx + sDirDX[d], ny = fy + sDirDY[d], k2;
                    u16 nb;
                    if (!InMap(gen, nx, ny))
                        continue;
                    nb = gen->m->blocks[ny * w + nx];
                    k2 = ((nb >> 10) & 3) ? RockPairIndex(nb & 0x3FF) : -1;
                    if (k2 < 0)
                        continue;
                    // (Tried in place, without Put's yield for each.)
                    for (c = 0; c < ROCK_PAIR_PIECES && !done; c++)
                    {
                        YIELD();
                        gen->m->blocks[fy * w + fx] = (b & ~0x3FF) | sRockPairPieces[c];
                        for (c2 = 0; c2 < ROCK_PAIR_PIECES && !done; c2++)
                        {
                            gen->m->blocks[ny * w + nx] = (nb & ~0x3FF) | sRockPairPieces[c2];
                            if (RockPairMisses(gen, fx, fy, c) == 0 && RockPairMisses(gen, nx, ny, c2) == 0)
                                done = TRUE;
                        }
                        if (!done)
                            gen->m->blocks[ny * w + nx] = nb;
                    }
                    if (!done)
                        gen->m->blocks[fy * w + fx] = b;
                }
                if (done)
                    changed = TRUE;
            }
        }
        if (!changed)
            break;
    }
}

// ---------------------------------------------------------------------------
// Waterfalls (v1.1 vert, round 2)
//
// On a river route whose river runs north to south, now and then a cliff
// band four blocks tall crosses the map from edge to edge (no way out east
// or west, so both ends run on into the tree border, as Route 119's bands
// run on into its rock), and the river pours over it, as on Routes 114 and
// 119: the fall's lip (0x0BB 0x0B3 0x0B3 0x0BC) in the band's top row, its
// falling water (0x0B9) down the face, and its foam (0x02C 0x034 0x034
// 0x03C) in the pool below. The band is Route 119's: the top rim 0x07C,
// the face 0x0A9; the way through it is a flight of stone steps four blocks
// tall where the trail crosses (as Mt. Pyre's). Every neighbour pair is one
// Hoenn has (the lip beside 0x07C, the falling water and the steps beside
// 0x0A9, the foam beside grass); a few 2x2 windows round the foam are not.
//
// Only on a route with no way out east or west, where the band's ends run
// off the map into the border: Hoenn's tall bands never close on open ground
// (no set of its pieces ends a band four blocks tall in grass or round
// trees), and at a seam the band would meet the next map's trees. Over seeds
// 1 to 200 no such route has a river running north to south, so for now no
// region gets one (see the v1.1 vert round 2 report).
//
// The plan waits in gen->stairX/Y (a river route has no levels): stairY[3]
// is FALLS_MARK, stairY[0] the band's top unit row, stairX[0] the river's
// left unit, stairX[1] the steps' unit.

#define FALLS_MARK 0xFA

static bool8 FallsPlanned(struct Gen *gen)
{
    return gen->p->kind == RPLACE_ROUTE && gen->p->type == RROUTE_RIVER && !gen->terraced
        && gen->stairY[3] == FALLS_MARK;
}

// The river route of a region that may have a waterfall: at most one, the
// one with the lowest hash among the river routes past the first tier with
// no way out east or west, in a little over half the regions.
static bool8 FallsRoute(struct Gen *gen)
{
    const struct RogueRegion *r = gen->r;
    int i, best = -1;
    u32 bestHash = 0xFFFFFFFF;
    if (RogueRng_Hash(r->seed, 7500) % 100 >= 60)
        return FALSE;
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *q = &r->places[i];
        u32 hh;
        if (q->kind != RPLACE_ROUTE || q->type != RROUTE_RIVER || q->tier == 0
         || LinkOnSide(q, RDIR_EAST) != NULL || LinkOnSide(q, RDIR_WEST) != NULL || VolcanoMountainAt(r, i))
            continue;
        hh = RogueRng_Hash(r->seed, 7600 + i);
        if (hh < bestHash)
        {
            bestHash = hh;
            best = i;
        }
    }
    return best == (int)(gen->p - r->places);
}

static bool8 FallsWaterU(struct Gen *gen, int x, int y)
{
    return Cell(gen, x, y) == C_SEA;
}

// Plan the band at unit rows R and R + 1: the river straight down two units
// wide from R - 1 to R + 3, land beside it, the trail crossing the band only
// once, straight down a column, away from the river. Margins (rows R - 1
// and R + 2) become open ground kept clear (C_KEEP), the band rock.
static bool8 FallsTry(struct Gen *gen, int R, int ax, int ay)
{
    int x, y, rx = -1, sx = -1, n = gen->cw * gen->ch, k, wd = 0;
    // The river: one straight run two or three units wide, from the row
    // above the band to the pool's row below it.
    for (x = 1; x + 2 < gen->cw && rx < 0; x++)
        if (FallsWaterU(gen, x, R))
        {
            rx = x;
            while (FallsWaterU(gen, x + wd, R))
                wd++;
        }
    if (rx < 2 || wd < 2 || wd > 3 || rx + wd + 1 >= gen->cw)
        return FALSE;
    for (y = R - 1; y <= R + 2; y++)
    {
        for (k = 0; k < wd; k++)
            if (!FallsWaterU(gen, rx + k, y) || BitU(gen->bridgeU, gen, rx + k, y))
                return FALSE;
        if (Cell(gen, rx - 1, y) == C_SEA || Cell(gen, rx + wd, y) == C_SEA || Cell(gen, rx - 1, y) == C_FORD
         || Cell(gen, rx + wd, y) == C_FORD)
            return FALSE;
    }
    // Nothing else but land in the band's rows and margins.
    for (y = R - 1; y <= R + 2; y++)
        for (x = 0; x < gen->cw; x++)
        {
            u8 c = Cell(gen, x, y);
            if (x >= rx && x < rx + wd)
                continue;
            if (c != C_TREE && c != C_OPEN && c != C_ROAD)
                return FALSE;
            if (c == C_ROAD && (y == R || y == R + 1))
            {
                if (sx >= 0 && sx != x)
                    return FALSE;
                sx = x;
            }
        }
    // The trail goes straight down through the steps, away from the river
    // and the map's edges.
    if (sx < 0 || sx < 2 || sx + 2 >= gen->cw || (sx > rx - 3 && sx < rx + wd + 2))
        return FALSE;
    for (y = R - 1; y <= R + 2; y++)
        if (Cell(gen, sx, y) != C_ROAD)
            return FALSE;
    for (y = R; y <= R + 1; y++)
        if (Cell(gen, sx - 1, y) == C_ROAD || Cell(gen, sx + 1, y) == C_ROAD)
            return FALSE;
    memcpy(PLATEAU_SAVED_CELLS(gen), gen->g, n);
    for (y = R - 1; y <= R + 2; y++)
        for (x = 0; x < gen->cw; x++)
        {
            if ((x >= rx && x < rx + wd) || x == sx || Cell(gen, x, y) == C_ROAD)
                continue;
            SetCell(gen, x, y, (y == R || y == R + 1) ? C_ROCK : C_KEEP);
        }
    // Everything on the road is still reached.
    FloodReachable(gen, ax, ay);
    for (k = 0; k < gen->nPath; k++)
        if (!Reached(gen, gen->path[k] % gen->cw, gen->path[k] / gen->cw))
        {
            memcpy(gen->g, PLATEAU_SAVED_CELLS(gen), n);
            return FALSE;
        }
    gen->stairY[0] = R;
    gen->stairX[0] = rx;
    gen->stairX[1] = sx;
    gen->stairX[2] = wd;
    gen->stairY[3] = FALLS_MARK;
    return TRUE;
}

static void PlanFalls(struct Gen *gen, int ax, int ay)
{
    int tries, i, j;
    if (gen->forestCut || gen->attempt >= 15)
        return;
    for (tries = 0; tries < 16; tries++)
    {
        int R = 3 + RAND(gen->ch - 8);
        YIELD();
        if (FallsTry(gen, R, ax, ay))
            break;
    }
    if (!FallsPlanned(gen))
        return;
    for (i = 0, j = 0; i < gen->nPockets; i++)
        if (Cell(gen, gen->pocketX[i], gen->pocketY[i]) == C_OPEN)
        {
            gen->pocketX[j] = gen->pocketX[i];
            gen->pocketY[j++] = gen->pocketY[i];
        }
    gen->nPockets = j;
}

// Before the ground cover is tidied: the margins' blocks against the band
// are plain grass (the way's path stays), the band's have none.
static void FallsFinish(struct Gen *gen)
{
    int fx, a;
    if (!FallsPlanned(gen))
        return;
    a = gen->stairY[0] * 2;
    for (fx = 0; fx < gen->m->w; fx++)
    {
        int fy;
        if ((fx >> 1) >= gen->stairX[0] && (fx >> 1) < gen->stairX[0] + gen->stairX[2])
            continue;
        for (fy = a - 1; fy <= a + 4; fy++)
        {
            u8 v = Cover(gen, fx, fy);
            if ((fx >> 1) == gen->stairX[1] && (v == V_PATH || v == V_SAND || v == V_ROAD))
                continue;
            SetCover(gen, fx, fy, V_NONE);
        }
    }
}

// Draw the band, the steps and the fall, once the water is drawn.
static void RenderFalls(struct Gen *gen)
{
    int fx, a, rf, sf, q;
    if (!FallsPlanned(gen))
        return;
    a = gen->stairY[0] * 2;
    rf = gen->stairX[0] * 2;
    sf = gen->stairX[1] * 2;
    for (fx = 0; fx < gen->m->w; fx++)
    {
        static const u16 sBand[4] = { 0x047C, 0x04A9, 0x04A9, 0x04A9 };
        const u16 *col = sBand;
        if (fx >= rf && fx < rf + 2 * gen->stairX[2])
        {
            bool8 first = fx == rf, last = fx == rf + 2 * gen->stairX[2] - 1;
            Put(gen, fx, a, first ? 0x10BB : last ? 0x10BC : 0x10B3);
            for (q = 1; q < 4; q++)
                Put(gen, fx, a + q, 0x10B9);
            Put(gen, fx, a + 4, first ? 0x102C : last ? 0x103C : 0x1034);
            continue;
        }
        if (fx == sf || fx == sf + 1)
        {
            for (q = 0; q < 4; q++)
                Put(gen, fx, a + q, fx == sf ? 0x30AF : 0x30CF);
            continue;
        }
        for (q = 0; q < 4; q++)
            Put(gen, fx, a + q, col[q]);
    }
}

// ---------------------------------------------------------------------------
// Waterfalls in a mountain pass (v1.1 vert, round 3)
//
// On some passes a stream falls through the rock, as on Route 114 and
// Meteor Falls' outside: a pool two units wide in a strip of grass high in
// the rock, pouring over a cliff band four blocks tall (the river's lip,
// three rows of falling water, the foam: RenderFalls's rows) into a pool
// below, with a strip of grass either side and the mountain's rock beyond,
// so the band's ends close into rock (RockRepair joins them). It stands
// off the way, in the rock or a meadow beside the trail: seen from it, never
// crossed, so neither Surf nor Waterfall is needed. Kept in gen->springX (left unit + 1) and
// gen->springY (the band's top unit row), which only towns use.

#define HILL_FALLS_TOP 3    // units of the structure above the band's top row
#define HILL_FALLS_BELOW 5  // units from the band's top row to the bottom grass row

static bool8 HillFallsPlanned(struct Gen *gen)
{
    return gen->p->kind == RPLACE_ROUTE && gen->p->type == RROUTE_HILLS && gen->springX != 0;
}

// The pass of a region that may have a waterfall: one, the lowest hash among
// its passes past the first tier (not the volcano's foot route); it gets one
// where there is room for it.
static bool8 HillFallsRoute(struct Gen *gen)
{
    const struct RogueRegion *r = gen->r;
    int i, best = -1;
    u32 bestHash = 0xFFFFFFFF;
    if (gen->p->type != RROUTE_HILLS)
        return FALSE;
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *q = &r->places[i];
        u32 hh;
        if (q->kind != RPLACE_ROUTE || q->type != RROUTE_HILLS || q->tier == 0 || VolcanoMountainAt(r, i))
            continue;
        hh = RogueRng_Hash(r->seed, 7800 + i);
        if (hh < bestHash)
        {
            bestHash = hh;
            best = i;
        }
    }
    return best == (int)(gen->p - r->places);
}

// The cell the structure puts at (dx, dy) from (rx, R): 0 none, else a cell.
static u8 HillFallsCell(int dx, int dy)
{
    // Columns -3..-2 and 3..4 rock down to the band and grass below it
    // (v1.1 joins: two units, so the walls never read as posts, and the band
    // runs on as the walls' own face instead of butting into their sides),
    // -1 and 2 grass (band at rows 0 and 1), 0 and 1 water; the top and
    // bottom rows grass across -1..2.
    if (dy < -HILL_FALLS_TOP || dy > HILL_FALLS_BELOW - 1 || dx < -3 || dx > 4)
        return 0xFF;
    if (dx <= -2 || dx >= 3)
        return dy <= 1 ? C_ROCK : C_OPEN;
    if (dy == -HILL_FALLS_TOP || dy == HILL_FALLS_BELOW - 1)
        return C_OPEN;
    if (dx == 0 || dx == 1)
        return C_SEA;
    return (dy == 0 || dy == 1) ? C_ROCK : C_OPEN;
}

// (v1.1 joins) Whether the structure's cell (dx, dy) may go at (x, y): open
// ground or forest off the road, away from the ways out; a wall above the
// band may stand on the mountain's rock (it merges into it), and so may the
// pocket's top row.
static bool8 HillFallsFree(struct Gen *gen, int x, int y, int dx, int dy)
{
    u8 c = Cell(gen, x, y);
    bool8 onRock = ((dx <= -2 || dx >= 3) && dy < 0) || dy == -HILL_FALLS_TOP;
    if (c == C_ROCK && onRock)
        return !BitU(gen->keep, gen, x, y) && !InSeamView(gen, x, y, 1, 1);
    return (c == C_TREE || c == C_OPEN) && !IsEdge(gen, x, y) && !NearExitU(gen, x, y, 2) && !BitU(gen->keep, gen, x, y)
        && !BitU(gen->grassZone, gen, x, y) && !InSeamView(gen, x, y, 1, 1);
}

// (v1.1 joins) A unit beyond a band's end: open ground, or forest that
// may be cleared to grass (off the edge, the keep and seam views).
static bool8 HillFallsEndOk(struct Gen *gen, int x, int y)
{
    u8 c = Cell(gen, x, y);
    if (c == C_OPEN || c == C_LEDGE)
        return TRUE;
    return c == C_TREE && !IsEdge(gen, x, y) && !BitU(gen->keep, gen, x, y) && !InSeamView(gen, x, y, 1, 1);
}

// Whether a side's wall (columns dx0 and dx0 + 1) fits: the two columns from
// the top down to the band and the row below it, the inner one grass below
// that, and open ground beyond the band's end.
static bool8 HillFallsWallFits(struct Gen *gen, int rx, int R, int dx0)
{
    int dx, dy, out = dx0 < 0 ? dx0 - 1 : dx0 + 2, in = dx0 < 0 ? dx0 + 1 : dx0;
    for (dy = -HILL_FALLS_TOP; dy <= HILL_FALLS_BELOW - 1; dy++)
        for (dx = dx0; dx <= dx0 + 1; dx++)
            if ((dy <= 2 || dx == in) && !HillFallsFree(gen, rx + dx, R + dy, dx, dy))
                return FALSE;
    // (v1.1 joins) Beyond the band's end, ground PlanHillFalls can make
    // grass (HillFallsEndOk): a hills' forest there turns to rock later
    // (RenderMountain) and the face would run into its top edge.
    for (dy = -1; dy <= 2; dy++)
        if (!HillFallsEndOk(gen, rx + out, R + dy))
            return FALSE;
    return TRUE;
}

static void PlanHillFalls(struct Gen *gen, int ax, int ay)
{
    int tries, dx, dy, k, n = gen->cw * gen->ch, spots, first;
    if (!HillFallsRoute(gen) || gen->forestCut || gen->attempt >= 12)
        return;
    // (v1.1 joins) Every spot, from a random one on: the wider walls fit
    // fewer places than random tries find.
    spots = gen->cw * gen->ch;
    first = RAND(spots);
    for (tries = 0; tries < spots; tries++)
    {
        int rx = (first + tries) % spots % gen->cw, R = (first + tries) % spots / gen->cw;
        bool8 ok = TRUE, wl, wr;
        YIELD();
        if (R - HILL_FALLS_TOP < 2 || R + HILL_FALLS_BELOW > gen->ch - 2 || rx - 4 < 1 || rx + 5 > gen->cw - 2)
            continue;
        // The pool, its grass strips and the band (columns -1..2).
        for (dy = -HILL_FALLS_TOP; dy <= HILL_FALLS_BELOW - 1 && ok; dy++)
            for (dx = -1; dx <= 2 && ok; dx++)
                if (!HillFallsFree(gen, rx + dx, R + dy, dx, dy))
                    ok = FALSE;
        if (!ok)
            continue;
        // (v1.1 joins) A wall two units thick on each side, never a
        // one-unit post: the band runs on as the walls' face, so it never
        // ends in the open (Hoenn has no end piece for a lone face), with
        // grass below its ends.
        wl = HillFallsWallFits(gen, rx, R, -3);
        wr = HillFallsWallFits(gen, rx, R, 3);
        if (!wl || !wr)
            continue;
        if ((!wl && (Cell(gen, rx - 2, R) == C_ROCK || Cell(gen, rx - 2, R + 1) == C_ROCK))
         || (!wr && (Cell(gen, rx + 3, R) == C_ROCK || Cell(gen, rx + 3, R + 1) == C_ROCK)))
            continue;
        memcpy(PLATEAU_SAVED_CELLS(gen), gen->g, n);
        for (dy = -HILL_FALLS_TOP; dy <= HILL_FALLS_BELOW - 1; dy++)
            for (dx = -3; dx <= 4; dx++)
            {
                if ((dx <= -2 && !wl) || (dx >= 3 && !wr) || ((dx == -3 || dx == 4) && dy >= 3)
                 || (dy == -HILL_FALLS_TOP && Cell(gen, rx + dx, R + dy) == C_ROCK))
                    continue;
                SetCell(gen, rx + dx, R + dy, HillFallsCell(dx, dy));
            }
        // (v1.1 joins) Grass beyond both ends of the band.
        for (dy = -1; dy <= 2; dy++)
        {
            if (Cell(gen, rx - 4, R + dy) == C_TREE)
                SetCell(gen, rx - 4, R + dy, C_OPEN);
            if (Cell(gen, rx + 5, R + dy) == C_TREE)
                SetCell(gen, rx + 5, R + dy, C_OPEN);
        }
        // Nothing on the way cut off.
        FloodReachable(gen, ax, ay);
        for (k = 0; k < gen->nPath && ok; k++)
            if (!Reached(gen, gen->path[k] % gen->cw, gen->path[k] / gen->cw))
                ok = FALSE;
        if (!ok)
        {
            memcpy(gen->g, PLATEAU_SAVED_CELLS(gen), n);
            continue;
        }
        for (k = 0, dx = 0; k < gen->nPockets; k++)
            if (Cell(gen, gen->pocketX[k], gen->pocketY[k]) == C_OPEN
             && !(gen->pocketX[k] >= rx - 3 && gen->pocketX[k] <= rx + 4 && gen->pocketY[k] >= R - HILL_FALLS_TOP && gen->pocketY[k] < R + HILL_FALLS_BELOW))
            {
                gen->pocketX[dx] = gen->pocketX[k];
                gen->pocketY[dx++] = gen->pocketY[k];
            }
        gen->nPockets = dx;
        gen->springX = rx + 1;
        gen->springY = R;
        return;
    }
}

// Before the ground cover is tidied: the grass strips are plain grass.
static void HillFallsFinish(struct Gen *gen)
{
    int dx, dy, q;
    if (!HillFallsPlanned(gen))
        return;
    for (dy = -HILL_FALLS_TOP; dy <= HILL_FALLS_BELOW - 1; dy++)
        for (dx = -3; dx <= 4; dx++)
        {
            int rx = gen->springX - 1;
            if ((dx <= -2 && Cell(gen, rx - 2, gen->springY) != C_ROCK) || (dx >= 3 && Cell(gen, rx + 3, gen->springY) != C_ROCK)
             || ((dx == -3 || dx == 4) && dy >= 3))
                continue; // (v1.1 joins) a side without a wall is left as it was
            for (q = 0; q < 4; q++)
                SetCover(gen, (gen->springX - 1 + dx) * 2 + (q & 1), (gen->springY + dy) * 2 + (q >> 1), V_NONE);
        }
}

// Once the rock is drawn: the band and the fall over the rock's blocks,
// then the rock's joins put right.
static void RenderHillFalls(struct Gen *gen)
{
    int fx, q, a, rf;
    if (!HillFallsPlanned(gen))
        return;
    a = gen->springY * 2;
    rf = (gen->springX - 1) * 2;
    // (v1.1 joins) The band runs on as the face of each side's wall.
    for (fx = Cell(gen, gen->springX - 3, gen->springY) == C_ROCK ? rf - 6 : rf - 2;
         fx < (Cell(gen, gen->springX + 2, gen->springY) == C_ROCK ? rf + 10 : rf + 6); fx++)
    {
        if (fx >= rf && fx < rf + 4)
        {
            bool8 first = fx == rf, last = fx == rf + 3;
            Put(gen, fx, a, first ? 0x10BB : last ? 0x10BC : 0x10B3);
            for (q = 1; q < 4; q++)
                Put(gen, fx, a + q, 0x10B9);
            Put(gen, fx, a + 4, first ? 0x102C : last ? 0x103C : 0x1034);
            continue;
        }
        Put(gen, fx, a, 0x047C);
        for (q = 1; q < 4; q++)
            Put(gen, fx, a + q, 0x04A9);
    }
    RockRepair(gen);
}
