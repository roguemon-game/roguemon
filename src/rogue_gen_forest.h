// Forest dungeons: deep woods like Petalburg Woods, Viridian Forest and Ilex
// Forest. Included by rogue_gen_dungeon.h, so it shares the generator's
// helpers.
//
// A forest hangs off a green route (forest, woods or field). Most often it
// is a deep grove off the route, with a guardian and good items at the far
// end. Sometimes the main road goes through it, as Route 104 runs through
// Petalburg Woods: a band of woods cuts the route in two, and the forest's
// two ends come out on either half (RWARP_DUNGEON on the half toward the
// lower numbered place, RWARP_DUNGEON_2 on the other). Such a forest's exit
// is its host.
//
// The way in is Route 104's: a dark gap two blocks wide in a tree line of
// deep woods, walked north or south into. Its metatiles are copied into each
// theme's tileset (make_forest_entrance.py). It never stands in view of the
// next map, which draws it with its own tileset. The floors are drawn like a
// woods route, with Petalburg Woods' tilesets (General and Rustboro), and
// their ends are Petalburg Woods' too: a lane of grass from the edge of the
// map with arrow warps across it.

#include "data/rogue_forest_entrance.h"

// The side of a forest's way in that the ground lies on: RDIR_SOUTH for one
// walked north into, as Petalburg Woods from the south half of Route 104.
// which: 0 for the way in (RWARP_DUNGEON), 1 for the way out of a forest
// the road goes through (RWARP_DUNGEON_2). It follows from the plan alone,
// so the floors know which end of the map each way comes out at: a half of
// the route that lies to the south is entered from the south. A grove off a
// route is always entered walking north.
static u8 ForestWayFace(const struct RogueRegion *r, const struct RogueDungeon *d, u8 which)
{
    const struct RoguePlace *p = &r->places[d->host];
    u8 dir[2] = { RDIR_NONE, RDIR_NONE };
    int i;
    if (d->exit != d->host)
        return RDIR_SOUTH;
    for (i = 0; i < p->nLinks; i++)
        dir[p->links[i].target < d->host ? 0 : 1] = p->links[i].dir;
    if (dir[which] == RDIR_NORTH || dir[which] == RDIR_SOUTH)
        return dir[which];
    return dir[!which] == RDIR_SOUTH ? RDIR_NORTH : RDIR_SOUTH;
}

// ---------------------------------------------------------------------------
// Planning

static bool8 ForestHostOk(const struct RogueRegion *r, int i)
{
    const struct RoguePlace *p = &r->places[i];
    int k;
    if (p->kind != RPLACE_ROUTE || (p->type != RROUTE_FOREST && p->type != RROUTE_WOODS && p->type != RROUTE_FIELD))
        return FALSE;
    // Not among ash trees, not where the road ends at a cave mouth, and not
    // behind an obstacle (the way in would stand before it).
    // (Nor beside a crater town, drawn with Sootopolis's tileset, which has
    // no way into the woods: v1.1 towns2.)
    if (p->theme == RTHEME_ASH || p->theme == RTHEME_CRATER || p->mouth != 0xFF || p->obstacle || RogueGen_DungeonAt(r, i) >= 0)
        return FALSE;
    // The first route stays gentle.
    for (k = 0; k < p->nLinks; k++)
        if (p->links[k].target == r->start)
            return FALSE;
    return TRUE;
}

// A route the woods can cut in two: on the main road with only the way in
// and the way on, no rival and no block in a lane, big enough for a band of
// woods, and running north or south at one end at least (the ways in face
// north or south).
static bool8 ForestCutOk(const struct RogueRegion *r, int i)
{
    const struct RoguePlace *p = &r->places[i];
    bool8 upright = FALSE;
    int k;
    if (p->side || p->nLinks != 2 || p->rival || p->w < 26 || p->h < 26)
        return FALSE;
    for (k = 0; k < p->nLinks; k++)
    {
        const struct RoguePlace *t = &r->places[p->links[k].target];
        if (t->side || t->obstacle || p->links[k].gate)
            return FALSE;
        if (p->links[k].dir == RDIR_NORTH || p->links[k].dir == RDIR_SOUTH)
            upright = TRUE;
    }
    return upright;
}

// Most regions have one forest. Half the time the road goes through it,
// before the first gym when it can, as Petalburg Woods comes before Rustboro;
// otherwise it is a grove off a route, early on most often. Uses its own
// random stream, so the rest of the region stays as it was.
static void PlanForestDungeons(struct RogueRegion *r)
{
    u32 rng = RogueRng_Hash(r->seed, 11100);
    u8 cand[ROGUE_MAX_PLACES];
    int n = 0, i, cap;
    struct RogueDungeon *d;

    if (RogueRng_Range(&rng, 100) >= 85)
        return;
    if (RogueRng_Range(&rng, 100) < 50)
    {
        for (cap = 0; cap <= 2 && n == 0; cap += 2)
            for (i = 0; i < r->nPlaces; i++)
                if (ForestHostOk(r, i) && ForestCutOk(r, i) && r->places[i].badges <= cap)
                    cand[n++] = i;
        if (n)
        {
            // One floor before the first gym, as Petalburg Woods and
            // Viridian Forest are: a long way with no Pokemon Center is hard
            // on a new team.
            u8 floors = 1 + (RogueRng_Range(&rng, 100) < 30);
            i = cand[RogueRng_Range(&rng, n)];
            if (!AddDungeon(r, RDUN_FOREST, i, r->places[i].badges ? floors : 1, RBOSS_NONE, TRUE))
                return;
            d = &r->dungeons[r->nDungeons - 1];
            d->exit = i;
            // On the way, so no harder than the route.
            d->level = r->places[i].level;
            return;
        }
    }
    for (cap = 4; cap <= 8 && n == 0; cap += 4)
        for (i = 0; i < r->nPlaces; i++)
            if (ForestHostOk(r, i) && r->places[i].badges <= cap)
                cand[n++] = i;
    if (n)
        AddDungeon(r, RDUN_FOREST, cand[RogueRng_Range(&rng, n)], 1 + RogueRng_Range(&rng, 2), RBOSS_GUARDIAN, FALSE);
}

// A forest floor's place: a woods route drawn with Petalburg Woods'
// tilesets, as big as a woods route. k picks the size.
static void ForestFloorPlace(struct RoguePlace *out, u32 k)
{
    static const u8 sForestSizes[][2] = { { 32, 30 }, { 28, 36 }, { 36, 28 }, { 34, 32 }, { 30, 34 } };
    out->kind = RPLACE_ROUTE;
    out->type = RROUTE_WOODS;
    out->theme = RTHEME_STONE;
    out->w = sForestSizes[k % 5][0];
    out->h = sForestSizes[k % 5][1];
    out->dark = 0;
}

// ---------------------------------------------------------------------------
// The way in on the host route

// The forest dungeon the route being generated hosts, or -1.
static s32 ForestOf(struct Gen *gen)
{
    s32 d;
    if (sGenFloor)
        return -1;
    d = RogueGen_DungeonAt(gen->r, (u8)(gen->p - gen->r->places));
    return (d >= 0 && gen->r->dungeons[d].kind == RDUN_FOREST) ? d : -1;
}

static bool8 ForestCutsRoute(struct Gen *gen)
{
    s32 d = ForestOf(gen);
    return d >= 0 && gen->r->dungeons[d].exit == gen->r->dungeons[d].host;
}

// The ground unit before way k, where the player stands to walk in.
static void ForestWayGround(struct Gen *gen, int k, int *ux, int *uy)
{
    *ux = gen->forestX[k];
    *uy = gen->forestY[k] + (gen->forestFace[k] == RDIR_SOUTH ? 1 : -1);
}

// The block the player walks into way k from (the left one of the two).
static void ForestWayFront(struct Gen *gen, int k, int *fx, int *fy)
{
    *fx = gen->forestX[k] * 2;
    *fy = gen->forestY[k] * 2 + (gen->forestFace[k] == RDIR_SOUTH ? 2 : -1);
}

static u32 ISqrt(u32 v)
{
    u32 res = 0, bit = 1u << 30;
    while (bit > v)
        bit >>= 2;
    while (bit)
    {
        if (v >= res + bit)
        {
            v -= res + bit;
            res = (res >> 1) + bit;
        }
        else
        {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

// How far a unit lies toward the way in (A) from the line halfway between
// the way in and the way on (B), in sixteenths of a unit, with a step up or
// down counting double so the band leans toward lying across the map, as
// Petalburg Woods lies across Route 104. The band of woods is where neither
// is much nearer: no step changes this by more than two units, so no walk
// crosses the band.
#define CUT_BAND 64
static int CutSide(struct Gen *gen, int x, int y)
{
    YIELD();
    int dx = gen->cutBX - gen->cutAX, dy = gen->cutBY - gen->cutAY;
    int x2 = 2 * x - gen->cutAX - gen->cutBX, y2 = 2 * y - gen->cutAY - gen->cutBY;
    int len = (int)ISqrt(256 * (dx * dx + 4 * dy * dy));
    return len ? -(128 * (x2 * dx + 4 * y2 * dy)) / len : 0;
}

static bool8 InCutBand(struct Gen *gen, int x, int y)
{
    int s;
    if (!gen->forestCut)
        return FALSE;
    s = CutSide(gen, x, y);
    return s >= -CUT_BAND && s <= CUT_BAND;
}

// Trees drawn as deep woods: the band across a route the forest cuts, and
// round each way in. Never at the map's edge, whose trees meet the border's
// and the next map's.
static bool8 ForestWoodsU(struct Gen *gen, int ux, int uy)
{
    int i;
    if (ux < 2 || uy < 2 || ux >= gen->cw - 2 || uy >= gen->ch - 2)
        return FALSE;
    if (InCutBand(gen, ux, uy))
        return TRUE;
    // The edge of a wood five units wide and three deep round a way in.
    for (i = 0; i < gen->nForestWays; i++)
    {
        int deep = (uy - gen->forestY[i]) * (gen->forestFace[i] == RDIR_SOUTH ? -1 : 1);
        if (Abs(ux - gen->forestX[i]) <= 2 && deep >= 0 && deep <= 2)
            return TRUE;
    }
    return FALSE;
}

// Seen from the next map, however many tries the map has had: the next map
// draws this one with its own tileset, where the way in is something else.
static bool8 SeenAcrossSeam(struct Gen *gen, int fx, int fy)
{
    u8 attempt = gen->attempt;
    bool8 seen;
    gen->attempt = 0;
    seen = SeenFromNextDoor(gen, fx, fy);
    gen->attempt = attempt;
    return seen;
}

// Can tree unit (x, y) be a way in entered from side face? For a forest
// across the road (cut), the gap must stand in the band with the ground on
// half k; otherwise in a tree line beside ground reached from the way in.
static bool8 ForestWayFits(struct Gen *gen, int k, int x, int y, u8 face, bool8 cut)
{
    int gy = y + (face == RDIR_SOUTH ? 1 : -1), by = y - (face == RDIR_SOUTH ? 1 : -1), i;
    if (x < 2 || y < 2 || x >= gen->cw - 2 || y >= gen->ch - 2)
        return FALSE;
    if (cut)
    {
        int s = CutSide(gen, x, gy);
        if (!InCutBand(gen, x, y) || !InCutBand(gen, x, by) || !InCutBand(gen, x - 1, y) || !InCutBand(gen, x + 1, y))
            return FALSE;
        if (k == 0 ? s <= CUT_BAND : s >= -CUT_BAND)
            return FALSE;
    }
    else
    {
        // Into a stand of trees at least three units wide and deep, so it
        // reads as the edge of a wood.
        u8 c = Cell(gen, x, gy);
        int dx, deep;
        if ((c != C_OPEN && c != C_ROAD) || !Reached(gen, x, gy))
            return FALSE;
        for (deep = 0; deep <= 2; deep++)
            for (dx = -1; dx <= 1; dx++)
                if (Cell(gen, x + dx, y + (by - y) * deep) != C_TREE)
                    return FALSE;
    }
    if (NearExitU(gen, x, y, 3) || NearExitU(gen, x, gy, 3))
        return FALSE;
    // Clear of the trees that thicken along a seam (ThickenSeamTrees).
    for (i = 0; i < gen->p->nLinks; i++)
    {
        u8 dd = gen->p->links[i].dir;
        if ((dd == RDIR_NORTH ? gy : dd == RDIR_SOUTH ? gen->ch - 1 - gy : dd == RDIR_WEST ? x : gen->cw - 1 - x) < 3)
            return FALSE;
    }
    for (i = 0; i < 4; i++)
    {
        int fx = x * 2 + (i & 1);
        if (SeenAcrossSeam(gen, fx, y * 2 + (i >> 1)) || SeenAcrossSeam(gen, fx, gy * 2 + (i >> 1))
         || InGateLaneF(gen, fx, gy * 2 + (i >> 1)))
            return FALSE;
    }
    // Two ways in stand apart.
    if (k == 1 && Abs(x - gen->forestX[0]) + Abs(y - gen->forestY[0]) < 4)
        return FALSE;
    return TRUE;
}

// Picks way k, scoring each spot that fits (lower is better).
static bool8 PickForestWay(struct Gen *gen, int k, u8 face, bool8 cut)
{
    int x, y, best = 0x7FFF, bx = -1, by = -1, i;
    for (y = 2; y < gen->ch - 2; y++)
    {
        for (x = 2; x < gen->cw - 2; x++)
        {
            YIELD();
            int score, gy = y + (face == RDIR_SOUTH ? 1 : -1);
            if (!ForestWayFits(gen, k, x, y, face, cut))
                continue;
            if (cut)
                // Toward the middle of the route, so each half has a stretch.
                score = Abs(x - (gen->cutAX + gen->cutBX) / 2) + Abs(gy - (gen->cutAY + gen->cutBY) / 2);
            else
                // Beside the trail, past its first stretch.
                score = (Cell(gen, x, gy) == C_ROAD ? 0 : HasNeighbor(gen, x, gy, C_ROAD, FALSE) ? 2 : 5);
            score = score * 4 + RAND(8);
            if (score < best)
            {
                best = score;
                bx = x;
                by = y;
            }
        }
    }
    if (bx < 0)
        return FALSE;
    gen->forestX[k] = bx;
    gen->forestY[k] = by;
    gen->forestFace[k] = face;
    gen->nForestWays = k + 1;
    // The gap and the trees round it stay; nothing stands in the gap.
    SetBitU(gen->keep, gen, bx, by);
    SetBitU(gen->keep, gen, bx - 1, by);
    SetBitU(gen->keep, gen, bx + 1, by);
    SetBitU(gen->keep, gen, bx, by - (face == RDIR_SOUTH ? 1 : -1));
    for (i = 0; i < 4; i++)
        MarkOccupied(gen, bx * 2 + (i & 1), by * 2 + (i >> 1));
    return TRUE;
}

// The band of woods across a route the forest cuts in two, and the ways in
// on either side of it. a and b are the units inside the way in and on.
static bool8 PlanForestCut(struct Gen *gen, int ax, int ay, int bx, int by)
{
    const struct RogueDungeon *d = &gen->r->dungeons[ForestOf(gen)];
    int x, y, k;
    gen->cutAX = ax;
    gen->cutAY = ay;
    gen->cutBX = bx;
    gen->cutBY = by;
    for (y = 1; y < gen->ch - 1; y++)
    {
        YIELD();
        for (x = 1; x < gen->cw - 1; x++)
            if (InCutBand(gen, x, y))
                SetBitU(gen->keep, gen, x, y);
    }
    for (k = 0; k < 2; k++)
        if (!PickForestWay(gen, k, ForestWayFace(gen->r, d, k), TRUE))
            return FALSE;
    return TRUE;
}

// The trail of a route the forest cuts: from the way in to the forest, and
// from the forest's other end to the way on, each swinging out a little.
static bool8 CarveForestCut(struct Gen *gen, int ax, int ay, int bx, int by)
{
    int k;
    for (k = 0; k < 2; k++)
    {
        int gx, gy, sx, sy, tx, ty, mx, my, s;
        ForestWayGround(gen, k, &gx, &gy);
        sx = k ? gx : ax;
        sy = k ? gy : ay;
        tx = k ? bx : gx;
        ty = k ? by : gy;
        mx = (sx + tx) / 2 + (int)RAND(7) - 3;
        my = (sy + ty) / 2 + (int)RAND(7) - 3;
        if (mx < 2) mx = 2;
        if (mx > gen->cw - 3) mx = gen->cw - 3;
        if (my < 2) my = 2;
        if (my > gen->ch - 3) my = gen->ch - 3;
        s = CutSide(gen, mx, my);
        if (k == 0 ? s > CUT_BAND : s < -CUT_BAND)
        {
            if (!CarveRoadBfs(gen, sx, sy, mx, my) || !CarveRoadBfs(gen, mx, my, tx, ty))
                return FALSE;
        }
        else if (!CarveRoadBfs(gen, sx, sy, tx, ty))
        {
            return FALSE;
        }
    }
    return TRUE;
}

// A grove off the route: its way in, in a tree line beside the way.
static bool8 PlaceForestWay(struct Gen *gen, int ax, int ay)
{
    FloodReachable(gen, ax, ay);
    return PickForestWay(gen, 0, RDIR_SOUTH, FALSE);
}

// Draws each way in as Route 104 draws Petalburg Woods' (walked north into:
// the dark gap under a tree, the way in, the gap's foot on the ground row;
// walked south into: the ground, the way in, the gap over the woods' crowns
// below), and adds its two warps.
static void RenderForestWays(struct Gen *gen)
{
    const u16 *ent = sForestEntrance[gen->p->theme < RTHEME_COUNT ? gen->p->theme : 0];
    int k, i;
    for (k = 0; k < gen->nForestWays; k++)
    {
        int fx = gen->forestX[k] * 2, fy = gen->forestY[k] * 2;
        for (i = 0; i < 2; i++)
        {
            if (gen->forestFace[k] == RDIR_SOUTH)
            {
                // The gap's top is the foot of a tree of the woods. Where
                // an ordinary tree's crown stood over it, that tree has lost
                // its trunk: the woods go on up instead, as round Petalburg
                // Woods' way in, under the trunk of the tree above.
                u16 crown = fy >= 2 ? gen->m->blocks[(fy - 2) * gen->m->w + fx + i] & 0x3FF : 0;
                if (crown >= (B_TREE_TL & 0x3FF) && crown <= (B_TREE_TR_END & 0x3FF))
                {
                    u16 up = fy >= 3 ? gen->m->blocks[(fy - 3) * gen->m->w + fx + i] & 0x3FF : 0;
                    // A trunk over that crown becomes a trunk on the ground,
                    // with the woods' top (shaded grass) below it.
                    if (up == (B_TREE_BL & 0x3FF) || up == (B_TREE_BR & 0x3FF))
                    {
                        Put(gen, fx + i, fy - 3, i ? B_TREE_BR_BASE : B_TREE_BL_BASE);
                        up = i ? (B_TREE_BR_BASE & 0x3FF) : (B_TREE_BL_BASE & 0x3FF);
                    }
                    if (up >= (B_TREE_BL_BASE & 0x3FF) && up <= (B_TREE_BR_END & 0x3FF))
                        Put(gen, fx + i, fy - 2, B_WOODS_TOP + i);
                    else
                        Put(gen, fx + i, fy - 2, i ? B_WOODS_CROWN2 : B_WOODS_CROWN);
                }
                Put(gen, fx + i, fy - 1, 0x0400 | ent[FENT_GAP_TOP]);
                Put(gen, fx + i, fy, 0x3000 | ent[FENT_WAY_NORTH]);
                Put(gen, fx + i, fy + 1, 0x3000 | ent[FENT_GAP_FOOT]);
            }
            else
            {
                // No shade of the woods before the gap.
                Put(gen, fx + i, fy - 1, GroundBlock(gen, fx + i, fy - 1, Cell(gen, fx >> 1, (fy >> 1) - 1), FALSE));
                Put(gen, fx + i, fy, 0x3000 | ent[FENT_WAY_SOUTH]);
                Put(gen, fx + i, fy + 1, 0x0400 | ent[FENT_GAP_CROWNS]);
            }
        }
        AddWarp(gen, fx, fy, k ? RWARP_DUNGEON_2 : RWARP_DUNGEON);
        AddWarp(gen, fx + 1, fy, k ? RWARP_DUNGEON_2_WIDE : RWARP_DUNGEON_WIDE);
    }
}

// Nothing stands before a way in, and the woods still cut the route: the
// way on is reached only through the forest.
static bool8 ForestWaysClear(struct Gen *gen, int ax, int ay)
{
    int k, i, j;
    for (k = 0; k < gen->nForestWays; k++)
    {
        int gx, gy, x = gen->forestX[k], y = gen->forestY[k];
        ForestWayGround(gen, k, &gx, &gy);
        if (Cell(gen, x, y) != C_TREE || Cell(gen, x - 1, y) != C_TREE || Cell(gen, x + 1, y) != C_TREE
         || Cell(gen, x, y + (y - gy)) != C_TREE)
            return FALSE;
        for (i = 0; i < gen->m->nObjs; i++)
        {
            const struct RogueObj *o = &gen->m->objs[i];
            if ((o->x >> 1) == gx && (o->y >> 1) == gy)
                return FALSE;
        }
        for (j = 0; j < 4; j++)
            if (!StandableBlock(gen, gx * 2 + (j & 1), gy * 2 + (j >> 1)))
                return FALSE;
    }
    if (gen->forestCut)
    {
        int bx, by;
        bool8 apart;
        ForestWayGround(gen, 1, &bx, &by);
        gen->forestCut = FALSE;
        FloodReachable(gen, ax, ay);
        apart = !Reached(gen, bx, by);
        gen->forestCut = TRUE;
        if (!apart)
            return FALSE;
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
// Floors

// One end of a floor: a lane of grass from the map's edge (unit x on the
// bottom or top row) to the unit holding the arrow warps. Returns the unit
// the way leads on from.
static void OpenForestEnd(struct Gen *gen, int x, bool8 bottom, int *ix, int *iy)
{
    int warp = bottom ? gen->ch - 2 : 1;
    SetBitU(gen->keep, gen, x - 1, warp);
    SetBitU(gen->keep, gen, x + 1, warp);
    *ix = x;
    *iy = bottom ? gen->ch - 3 : 2;
}

// Draws an end: the arrow warps across the lane, grass the player cannot
// walk on beyond them, out to the edge, as at Petalburg Woods' ends.
static void RenderForestEnd(struct Gen *gen, int x, bool8 bottom, u8 kind)
{
    int fx = x * 2, wy = bottom ? gen->m->h - 3 : 2, i;
    for (i = 0; i < 2; i++)
    {
        Put(gen, fx + i, wy, bottom ? 0x3204 : 0x3203);
        Put(gen, fx + i, bottom ? wy + 1 : wy - 1, 0x0401);
        Put(gen, fx + i, bottom ? wy + 2 : wy - 2, 0x0401);
    }
    AddWarp(gen, fx, wy, kind);
    AddWarp(gen, fx + 1, wy, kind + RFLOOR_WARPS);
}

// ---------------------------------------------------------------------------
// Extras on a forest floor (PlanExtras): the prize beside a grove's
// guardian, the choice room's balls in a dead-end glade, and the camp beside
// a pond off the way.

static bool8 ExtrasAreaFree(struct Gen *gen, int x, int y, int w, int h, int margin, bool8 (*open)(struct Gen *, int, int));
static bool8 FindExtrasRow(struct Gen *gen, const struct Rect *room, int n, int *fx, int *fy, bool8 (*open)(struct Gen *, int, int));
static bool8 PlacePrize(struct Gen *gen, const struct RogueDungeonExtras *e, int bossX, int bossY,
                        bool8 (*open)(struct Gen *, int, int));
static void PlaceChoiceGuide(struct Gen *gen, int fx, int fy, int n, bool8 (*open)(struct Gen *, int, int));

// Ground in the woods one can walk round: reached from the way in, off
// the ledges.
static bool8 ForestOpenF(struct Gen *gen, int fx, int fy)
{
    u8 c = Cell(gen, fx >> 1, fy >> 1);
    return (c == C_OPEN || c == C_ROAD) && Reached(gen, fx >> 1, fy >> 1) && !ByLedgeF(gen, fx, fy);
}

// A glade four units across and two deep, at the end of a short side way
// cut off the trail, with trees all round but where the side way comes in:
// a dead end the balls lie in, off the way as Mt. Moon's fossils are. FALSE
// if there is no room for one.
static bool8 GladeCellFree(struct Gen *gen, int x, int y)
{
    return x >= 2 && y >= 2 && x < gen->cw - 2 && y < gen->ch - 2 && Cell(gen, x, y) == C_TREE
        && !IsEdge(gen, x, y) && !BitU(gen->keep, gen, x, y);
}

// Every unit of the side way (len units on from sx, sy along dx, dy) and
// of the glade is trees, and trees stand all round them but at (sx, sy).
static bool8 GladeFits(struct Gen *gen, int sx, int sy, int dx, int dy, int len, const struct Rect *glade)
{
    int i, k;
    for (i = 0; i < len + glade->w * glade->h; i++)
    {
        int cx = i < len ? sx + dx * (i + 1) : glade->x + (i - len) % glade->w;
        int cy = i < len ? sy + dy * (i + 1) : glade->y + (i - len) / glade->w;
        u8 n;
        if (!GladeCellFree(gen, cx, cy))
            return FALSE;
        for (n = 0; n < 4; n++)
        {
            int nx = cx + sDirDX[n], ny = cy + sDirDY[n];
            bool8 mine = (nx >= glade->x && nx < glade->x + glade->w && ny >= glade->y && ny < glade->y + glade->h)
                      || (nx == sx && ny == sy);
            for (k = 1; k <= len && !mine; k++)
                mine = nx == sx + dx * k && ny == sy + dy * k;
            if (!mine && IsOpenU(Cell(gen, nx, ny)))
                return FALSE;
        }
    }
    return TRUE;
}

static bool8 CarveForestGlade(struct Gen *gen, int fromX, int fromY, struct Rect *glade)
{
    int tries, k, x, y;
    FloodReachable(gen, fromX, fromY);
    for (tries = 0; tries < 400; tries++)
    {
        int len = RAND(3), at, first = RAND(4);
        int sx = 2 + RAND(gen->cw - 4), sy = 2 + RAND(gen->ch - 4);
        u8 d = RAND(4);
        int dx = sDirDX[d], dy = sDirDY[d], ex, ey;
        // From open ground the way in reaches, out to its edge.
        if ((Cell(gen, sx, sy) != C_OPEN && Cell(gen, sx, sy) != C_ROAD) || !Reached(gen, sx, sy))
            continue;
        for (k = 0; k < 4 && IsOpenU(Cell(gen, sx + dx, sy + dy)); k++)
        {
            sx += dx;
            sy += dy;
        }
        if (Cell(gen, sx, sy) != C_OPEN && Cell(gen, sx, sy) != C_ROAD)
            continue;
        // The glade lies beyond the side way's last unit, entered from
        // either of its long sides or its ends.
        ex = sx + dx * len;
        ey = sy + dy * len;
        glade->w = 4;
        glade->h = 2;
        for (at = 0; at < 4; at++)
        {
            int o = (first + at) & 3;
            glade->x = dy ? ex - o : (dx > 0 ? ex + 1 : ex - 4);
            glade->y = dy ? (dy > 0 ? ey + 1 : ey - 2) : ey - (o & 1);
            if (!GladeFits(gen, sx, sy, dx, dy, len, glade))
                continue;
            for (k = 1; k <= len; k++)
                SetCell(gen, sx + dx * k, sy + dy * k, C_OPEN);
            for (y = glade->y; y < glade->y + glade->h; y++)
                for (x = glade->x; x < glade->x + glade->w; x++)
                    SetCell(gen, x, y, C_OPEN);
            // The trees round it stay, so the meadows and side ways grown
            // after it never open a second way in.
            for (y = glade->y - 1; y <= glade->y + glade->h; y++)
                for (x = glade->x - 1; x <= glade->x + glade->w; x++)
                    if (Cell(gen, x, y) == C_TREE)
                        SetBitU(gen->keep, gen, x, y);
            for (k = 1; k <= len; k++)
                for (y = -1; y <= 1; y++)
                    for (x = -1; x <= 1; x++)
                        if (Cell(gen, sx + dx * k + x, sy + dy * k + y) == C_TREE)
                            SetBitU(gen->keep, gen, sx + dx * k + x, sy + dy * k + y);
            return TRUE;
        }
    }
    return FALSE;
}

// A place for the camp, off the trail: on the shore of a pond if one is
// reached, else in a quiet corner with open ground all round.
static bool8 FindForestCamp(struct Gen *gen, int *fx, int *fy)
{
    int x, y, best = 0, n = 0;
    for (y = 2; y < gen->m->h - 2; y++)
    {
        for (x = 2; x < gen->m->w - 2; x++)
        {
            int ux = x >> 1, uy = y >> 1, score = 0, dx, dy, sides = 0;
            u8 d;
            if (Cell(gen, ux, uy) != C_OPEN || !Reached(gen, ux, uy) || HasNeighbor(gen, ux, uy, C_ROAD, TRUE)
             || IsOccupied(gen, x, y) || IsCrowded(gen, x, y) || ByLedgeF(gen, x, y))
                continue;
            for (d = 0; d < 4; d++)
                sides += ForestOpenF(gen, x + sDirDX[d], y + sDirDY[d]) && !IsOccupied(gen, x + sDirDX[d], y + sDirDY[d]);
            for (dy = -1; dy <= 1; dy++)
                for (dx = -1; dx <= 1; dx++)
                    if (Cell(gen, (x + dx) >> 1, (y + dy) >> 1) == C_SEA)
                        score = 2;
            if (score ? sides < 2 || BlocksWay(gen, x, y) : !ExtrasAreaFree(gen, x, y, 1, 1, 2, ForestOpenF))
                continue;
            if (!score)
                score = 1;
            if (score > best)
            {
                best = score;
                n = 0;
            }
            if (score == best && RAND(++n) == 0)
            {
                *fx = x;
                *fy = y;
            }
        }
    }
    return n != 0;
}

static bool8 PlaceForestExtras(struct Gen *gen, int bossX, int bossY, const struct Rect *glade)
{
    struct RogueDungeonExtras e;
    int fx, fy, k, x, y;

    RogueGen_DungeonExtras(gen->r, sGenDungeon, &e);
    if (!PlacePrize(gen, &e, bossX, bossY, ForestOpenF))
        return FALSE;
    if (e.nChoice && sGenFloor == e.choiceFloor)
    {
        if (glade == NULL || !FindExtrasRow(gen, glade, e.nChoice, &fx, &fy, ForestOpenF))
            return FALSE;
        for (k = 0; k < e.nChoice; k++)
        {
            SetCover(gen, fx + 2 * k, fy, BareCover(gen, fx + 2 * k, fy));
            AddObj(gen, ROBJ_CHOICE, fx + 2 * k, fy, RDIR_SOUTH, e.choice + k, 0);
        }
        PlaceChoiceGuide(gen, fx, fy, e.nChoice, ForestOpenF);
    }
    if (e.camp != 0xFF && sGenFloor == e.campFloor)
    {
        if (!FindForestCamp(gen, &fx, &fy))
            return FALSE;
        // A trodden patch round the camp, no tall grass.
        for (y = fy - 1; y <= fy + 1; y++)
            for (x = fx - 1; x <= fx + 1; x++)
                if (Cover(gen, x, y) == V_TALL)
                    SetCover(gen, x, y, BareCover(gen, x, y));
        AddObj(gen, ROBJ_CAMP, fx, fy, RDIR_SOUTH, e.camp, 0);
    }
    // Someone lost in the woods, off the trail as a camp is.
    if (e.explorer != 0xFF && sGenFloor == e.explorerFloor)
    {
        if (!FindForestCamp(gen, &fx, &fy))
            return FALSE;
        for (y = fy - 1; y <= fy + 1; y++)
            for (x = fx - 1; x <= fx + 1; x++)
                if (Cover(gen, x, y) == V_TALL)
                    SetCover(gen, x, y, BareCover(gen, x, y));
        AddObj(gen, ROBJ_EXPLORER, fx, fy, RAND(4), e.explorer, 0);
    }
    return TRUE;
}

static bool8 GenForest(struct Gen *gen)
{
    const struct RoguePlace *p = gen->p;
    const struct RogueDungeon *d = &gen->r->dungeons[sGenDungeon];
    bool8 last = sGenFloor >= d->nFloors, through = !last || d->exit != 0xFF;
    // Every floor faces the same way: the way up at the end the host's way
    // in faces, the way on at the other, unless the last way out faces the
    // same way as the first.
    bool8 upBottom = ForestWayFace(gen->r, d, 0) == RDIR_SOUTH;
    bool8 farBottom = (last && d->exit != 0xFF) ? ForestWayFace(gen->r, d, 1) == RDIR_SOUTH : !upBottom;
    int x0, x1 = 0, sx, sy, tx, ty, px, py, i, n, tries, fx, fy;
    int nPlain = p->nTrainers, trainers = 0, items = 0, bossX = -1, bossY = -1;
    bool8 side, choiceHere, campHere;
    u8 facing;
    struct RogueDungeonExtras ex;
    struct Rect glade;

    RogueGen_DungeonExtras(gen->r, sGenDungeon, &ex);
    choiceHere = ex.nChoice && sGenFloor == ex.choiceFloor;
    campHere = ex.camp != 0xFF && sGenFloor == ex.campFloor;

    gen->woods = TRUE;
    gen->groundKind = GROUND_SAND;
    Fill(gen, C_TREE);
    x0 = 3 + RAND(gen->cw - 6);
    OpenForestEnd(gen, x0, upBottom, &sx, &sy);
    if (through)
    {
        // Two ends on one side stand well apart.
        for (tries = 0; tries < 20; tries++)
        {
            x1 = 3 + RAND(gen->cw - 6);
            if (farBottom != upBottom || Abs(x1 - x0) >= 7)
                break;
        }
        if (tries == 20)
            return FALSE;
        OpenForestEnd(gen, x1, farBottom, &tx, &ty);
    }
    else
    {
        // A grove: a clearing at the far end.
        tx = 3 + RAND(gen->cw - 6);
        ty = upBottom ? 3 + RAND(2) : gen->ch - 4 - RAND(2);
    }
    // The way winds from side to side between the ends, up and down the map.
    AddRoadCell(gen, x0, upBottom ? gen->ch - 1 : 0);
    gen->nWalk = 0;
    AddRoadCell(gen, x0, upBottom ? gen->ch - 2 : 1);
    n = 2 + RAND(2);
    side = RAND(2);
    px = sx;
    py = sy;
    for (i = 1; i <= n; i++)
    {
        int wy = sy + ((ty - sy) * i) / (n + 1) + (int)RAND(3) - 1;
        int third = gen->cw / 4;
        int wx = side ? 2 + RAND(third) : gen->cw - 3 - RAND(third);
        if (wy < 2) wy = 2;
        if (wy > gen->ch - 3) wy = gen->ch - 3;
        side = !side;
        if (!CarveRoadBfs(gen, px, py, wx, wy))
            return FALSE;
        px = wx;
        py = wy;
    }
    if (!CarveRoadBfs(gen, px, py, tx, ty))
        return FALSE;
    if (through)
    {
        AddRoadCell(gen, x1, farBottom ? gen->ch - 2 : 1);
        AddRoadCell(gen, x1, farBottom ? gen->ch - 1 : 0);
    }
    LoopEraseRoad(gen, (upBottom ? gen->ch - 1 : 0) * gen->cw + x0, -1);
    if (!through)
    {
        // The clearing where the guardian waits.
        int x, y;
        for (y = ty - 3; y <= ty + 3; y++)
            for (x = tx - 3; x <= tx + 3; x++)
                if (Abs(x - tx) + Abs(y - ty) <= 2 + (Abs(x - tx) <= 1 && Abs(y - ty) <= 1))
                    OpenUnit(gen, x, y);
    }
    // The choice room's glade is cut while the woods are still thick, so
    // there is room for a dead end off the trail (cut last, it failed on
    // most tries once the meadows had opened the woods up).
    if (choiceHere && !CarveForestGlade(gen, sx, sy, &glade))
        return FALSE;
    {
        // Glades along the way, narrow squeezes between them for the
        // trainers, as in Petalburg Woods: the woods open out, but trees
        // still stand over about half the map.
        u16 squeeze[MAX_SQUEEZE];
        u8 radius[MAX_SQUEEZE + 1];
        int nSqueeze = PickSqueezes(gen, p->nTrainers + 1, 3, squeeze), k, extra;
        for (k = 0; k <= nSqueeze; k++)
            radius[k] = RAND(4) == 0 ? 1 : 2 + RAND(2);
        for (extra = 0; extra < 3; extra++)
            if (GrowMeadows(gen, squeeze, nSqueeze, radius, extra) <= 56)
                break;
    }
    // A side way more where the choice room's glade opens off one, and
    // always a pond by the camp.
    AddSpurs(gen, 2 + RAND(2) + choiceHere);
    if (CHANCE(40) || campHere)
        PlacePond(gen, sx, sy, 2);
    for (i = 2 + RAND(3); i > 0; i--)
        PlaceClump(gen, i < 2);
    BreakUpFields(gen);
    if (CHANCE(50))
        PlaceLedgeBands(gen, sx, sy, 1 + RAND(2));

    // Ground cover: the worn trail, tall grass everywhere, a few flowers.
    for (i = 0; i < gen->nPath; i++)
        PaintUnit(gen, gen->path[i] % gen->cw, gen->path[i] / gen->cw, V_PATH);
    for (tries = 0; tries < 40 && (tries < 10 || gen->tallCount < gen->m->w * gen->m->h / 6); tries++)
    {
        int pi = 3 + RAND(gen->nPath > 6 ? gen->nPath - 6 : 1);
        int bx = gen->path[pi] % gen->cw + (int)RAND(5) - 2, by = gen->path[pi] / gen->cw + (int)RAND(5) - 2;
        if (Abs(bx - sx) + Abs(by - sy) < 2 || (through && Abs(bx - tx) + Abs(by - ty) < 2))
            continue;
        PaintBlob(gen, bx * 2 + RAND(2), by * 2 + RAND(2), 6 + RAND(10), V_TALL, FALSE);
    }
    TidyPaths(gen);
    ScatterFlowers(gen, 3);

    // Nothing in the lanes at the ends.
    for (i = 0; i < 4; i++)
    {
        MarkOccupied(gen, x0 * 2 + (i & 1), (upBottom ? gen->ch - 2 : 1) * 2 + (i >> 1));
        if (through)
            MarkOccupied(gen, x1 * 2 + (i & 1), (farBottom ? gen->ch - 2 : 1) * 2 + (i >> 1));
    }
    FloodReachable(gen, sx, sy);
    // The guardian (or the master) in the clearing at the end of a grove.
    if (!through && d->boss != RBOSS_NONE)
    {
        u8 slot = BossIsTrainer(d) ? p->nTrainers - 1 : sGenDungeon;
        fx = tx * 2 + RAND(2);
        fy = ty * 2 + RAND(2);
        SetCover(gen, fx, fy, V_NONE);
        AddObj(gen, ROBJ_BOSS, fx, fy, upBottom ? RDIR_SOUTH : RDIR_NORTH, slot, BossIsTrainer(d) ? 4 : 0);
        if (BossIsTrainer(d))
            nPlain--;
        bossX = fx;
        bossY = fy;
    }
    if (!PlaceForestExtras(gen, bossX, bossY, choiceHere ? &glade : NULL))
        return FALSE;
    for (i = 0; i < nPlain; i++)
    {
        for (tries = 0; tries < 80; tries++)
        {
            int base = (gen->nPath * (i + 1)) / (nPlain + 1);
            int pi = tries < 40 ? base + (int)RAND(7) - 3 : 3 + RAND(gen->nPath > 6 ? gen->nPath - 6 : 1);
            if (pi < 3 || pi >= gen->nPath - 3)
                continue;
            if (FindSpotBesideRoad(gen, gen->path[pi] % gen->cw, gen->path[pi] / gen->cw, &fx, &fy, &facing))
            {
                if (Cover(gen, fx, fy) == V_TALL)
                    SetCover(gen, fx, fy, V_NONE);
                AddObj(gen, ROBJ_TRAINER, fx, fy, facing, trainers++, 4);
                break;
            }
        }
    }
    // Items at the ends of the side ways first, then in nooks.
    for (i = 0; i < gen->nPockets && items < p->nItems; i++)
    {
        fx = gen->pocketX[i] * 2 + RAND(2);
        fy = gen->pocketY[i] * 2 + RAND(2);
        if (Reached(gen, gen->pocketX[i], gen->pocketY[i]) && !IsOccupied(gen, fx, fy) && !IsCrowded(gen, fx, fy)
         && !BlocksWay(gen, fx, fy) && !ByLedgeF(gen, fx, fy))
        {
            SetCover(gen, fx, fy, BareCover(gen, fx, fy));
            AddObj(gen, ROBJ_ITEM, fx, fy, RDIR_SOUTH, items++, 0);
        }
    }
    for (tries = 0; tries < 80 && items < p->nItems; tries++)
    {
        int pi = 3 + RAND(gen->nPath > 6 ? gen->nPath - 6 : 1);
        if ((tries < 40 ? FindNook(gen, &fx, &fy)
                        : FindSpotBesideRoad(gen, gen->path[pi] % gen->cw, gen->path[pi] / gen->cw, &fx, &fy, &facing))
         && !BlocksWay(gen, fx, fy))
            AddObj(gen, ROBJ_ITEM, fx, fy, RDIR_SOUTH, items++, 0);
    }
    if (trainers < nPlain || items < p->nItems)
        return FALSE;

    Render(gen);
    RenderForestEnd(gen, x0, upBottom, RFLOOR_UP);
    if (through)
        RenderForestEnd(gen, x1, farBottom, last ? RFLOOR_EXIT : RFLOOR_DOWN);
    // The trainers wait on the way through, or on the way to the clearing.
    PlaceTrainersBetween(gen, x0 * 2, upBottom ? gen->m->h - 4 : 3,
                         through ? x1 * 2 : tx * 2, through ? (farBottom ? gen->m->h - 4 : 3) : ty * 2);
    // Nobody stands where the player arrives.
    for (i = 0; i < gen->m->nObjs; i++)
    {
        const struct RogueObj *o = &gen->m->objs[i];
        if ((o->x >> 1) == x0 && (o->y >> 1) == sy)
            return FALSE;
        if (through && (o->x >> 1) == x1 && (o->y >> 1) == ty)
            return FALSE;
    }
    return TRUE;
}
