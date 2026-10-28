// Land routes (v1.1): flower fields and the long-grass maze.
//
// RROUTE_FLOWERS is a gentle meadow built round fenced flower beds, like
// Route 117: Mauville's flower beds inside a wooden fence beside the way,
// with loose flowers and ponds round them. The beds are Mauville's own
// tiles, so the route is drawn with Mauville's tileset (RTHEME_VERDANT).
//
// RROUTE_LONGGRASS is a route where most of the way through is long grass
// that hides the player, like Routes 119 and 120: a few wide fields of it
// with short grass round them, to wade or to wind round. Long grass's lower edge is only in
// Fortree's tileset, so the route is drawn with it (RTHEME_TREETOP).
//
// Both use the meadow generator (GreenRoute) with these passes on top.
// Included into rogue_gen.c before GenRoute.

// ---------------------------------------------------------------------------
// Planning

// Is a route of this type next to p, or on the far side of a town next to
// it? (Two alike either side of one town would look the same, and would
// leave the town both its ways out toward another tileset.)
static bool8 LandNextTo(const struct RogueRegion *r, const struct RoguePlace *p, u8 type)
{
    int k, j;
    for (k = 0; k < p->nLinks; k++)
    {
        const struct RoguePlace *t = &r->places[p->links[k].target];
        if (t->kind == RPLACE_ROUTE && t->type == type)
            return TRUE;
        if (t->kind != RPLACE_TOWN)
            continue;
        for (j = 0; j < t->nLinks; j++)
        {
            const struct RoguePlace *u = &r->places[t->links[j].target];
            if (u != p && u->kind == RPLACE_ROUTE && u->type == type)
                return TRUE;
        }
    }
    return FALSE;
}

// Is p next to a village or hamlet? Its own tileset would leave the small
// town's way out toward it clear of buildings (InSeamView), which a
// village's landmark may need.
static bool8 LandBySmallTown(const struct RogueRegion *r, const struct RoguePlace *p)
{
    int k;
    for (k = 0; k < p->nLinks; k++)
    {
        const struct RoguePlace *t = &r->places[p->links[k].target];
        if (t->kind == RPLACE_TOWN && (t->type == RTOWN_VILLAGE || t->type == RTOWN_HAMLET) && t->theme != RTHEME_VERDANT
         && t->theme != RTHEME_TREETOP)
            return TRUE;
    }
    return FALSE;
}

// With the v1.1 zone planner the zones pick flower fields and long grass.
// This keeps the land kinds' own limits on what the zones chose: they need
// their tilesets (Mauville's bed flowers, Fortree's long grass), so a route
// that cannot take one, stands by a village, or has one alike beside it or
// across a town, becomes a lake route (or a meadow or forest by a lake).
static void LandFitZoneKinds(struct RogueRegion *r)
{
    int i;
    for (i = 0; i < r->nPlaces; i++)
    {
        struct RoguePlace *p = &r->places[i];
        if (p->kind != RPLACE_ROUTE || (p->type != RROUTE_FLOWERS && p->type != RROUTE_LONGGRASS))
            continue;
        if (p->mouth != 0xFF || p->theme == RTHEME_ASH || p->seaSide != RDIR_NONE || p->obstacle == ROBST_SURF
         || LandBySmallTown(r, p) || LandNextTo(r, p, p->type))
        {
            // A lake keeps the zones' variety (a meadow or forest here could
            // make three look-alike routes in a row).
            p->type = !LandNextTo(r, p, RROUTE_LAKE)  ? RROUTE_LAKE
                    : !LandNextTo(r, p, RROUTE_FIELD) ? RROUTE_FIELD : RROUTE_FOREST;
            continue;
        }
        p->theme = p->type == RROUTE_FLOWERS ? RTHEME_VERDANT : RTHEME_TREETOP;
    }
}

// Meadow kinds: wide open fields along the way.
static bool8 FieldLike(const struct RoguePlace *p)
{
    return p->type == RROUTE_FIELD || p->type == RROUTE_FLOWERS || p->type == RROUTE_LONGGRASS;
}

// ---------------------------------------------------------------------------
// Flower beds (Route 117)
//
// Mauville's bed flowers (0x208 to 0x217) are laid on a diagonal: along a
// row each block is the next of the chain 20F 217 20E 215 20C 213 20A 211
// 208, and each row is the one above moved one block on, as in Route 117's
// beds (the tables were made by a script from that rule). The chain is
// nine long, so a bed's width plus height is at most ten, and each bed
// starts where its edges show only the flowers Hoenn puts against a fence
// (so a bed is six or four wide). The fence is the General tileset's
// (F_TL...). Each table is the bed with its fence, closed
// all round (tile_polish.py wants a fence to close; Route 117 leaves a gap).

static const u16 sLandBed6x2_2[8 * 4] = {
    F_TL, F_H, F_H, F_H, F_H, F_H, F_H, F_TR,
    F_L, 0x320E, 0x3215, 0x320C, 0x3213, 0x320A, 0x3211, F_R,
    F_L, 0x3217, 0x320E, 0x3215, 0x320C, 0x3213, 0x320A, F_R,
    F_BL, F_H, F_H, F_H, F_H, F_H, F_H, F_BR,
};
static const u16 sLandBed6x4_3[8 * 6] = {
    F_TL, F_H, F_H, F_H, F_H, F_H, F_H, F_TR,
    F_L, 0x3215, 0x320C, 0x3213, 0x320A, 0x3211, 0x3208, F_R,
    F_L, 0x320E, 0x3215, 0x320C, 0x3213, 0x320A, 0x3211, F_R,
    F_L, 0x3217, 0x320E, 0x3215, 0x320C, 0x3213, 0x320A, F_R,
    F_L, 0x320F, 0x3217, 0x320E, 0x3215, 0x320C, 0x3213, F_R,
    F_BL, F_H, F_H, F_H, F_H, F_H, F_H, F_BR,
};
static const u16 sLandBed4x2_3[6 * 4] = {
    F_TL, F_H, F_H, F_H, F_H, F_TR,
    F_L, 0x3215, 0x320C, 0x3213, 0x320A, F_R,
    F_L, 0x320E, 0x3215, 0x320C, 0x3213, F_R,
    F_BL, F_H, F_H, F_H, F_H, F_BR,
};

struct LandBed
{
    const u16 *blocks;
    u8 uw, uh; // in units, fence and all
};

static const struct LandBed sLandBeds[] = {
    { sLandBed6x2_2, 4, 2 },
    { sLandBed6x4_3, 4, 3 },
    { sLandBed4x2_3, 3, 2 },
};

// Can a bed stand on these units: open ground with only grass or flowers on
// it (or trees that may go), out of view of a neighbour with another tileset (the flowers are
// Mauville's), with grass all round its fence as on Route 117 (no
// building, rock, water, ledge or other bed, and only trees that may go),
// and the trail beside it.
// A tree in the ring round a bed that may go to leave grass round its fence:
// not the map's edge, a tree kept round a narrow way in, or by an opening.
static bool8 LandTreeGoes(struct Gen *gen, int x, int y)
{
    return !IsEdge(gen, x, y) && !BitU(gen->keep, gen, x, y) && !NearExitU(gen, x, y, 2);
}

static bool8 LandBedFits(struct Gen *gen, int ux, int uy, int uw, int uh, bool8 byRoad, int sand)
{
    int x, y;
    bool8 road = FALSE;
    if (ux < 1 || uy < 1 || ux + uw > gen->cw - 1 || uy + uh > gen->ch - 1)
        return FALSE;
    // The units themselves first: most spots fail there at once, and the
    // ring checks below cost more (LandFlowerBeds tries every spot on the map
    // many times).
    for (y = uy; y < uy + uh; y++)
    {
        for (x = ux; x < ux + uw; x++)
        {
            int k;
            u8 c = Cell(gen, x, y);
            if ((c != C_OPEN && !(c == C_TREE && LandTreeGoes(gen, x, y))) || IsEdge(gen, x, y) || AshZoneU(gen, x, y))
                return FALSE;
            for (k = 0; k < 4; k++)
            {
                int fx = x * 2 + (k & 1), fy = y * 2 + (k >> 1);
                u8 cv = Cover(gen, fx, fy);
                if ((cv != V_NONE && cv != V_TALL && cv != V_FLOWER) || IsOccupied(gen, fx, fy))
                    return FALSE;
            }
        }
    }
    if (InSeamView(gen, ux, uy, uw, uh) || !FenceRingClear(gen, ux, uy, uw, uh) || HasWaterRing(gen, ux, uy, uw, uh))
        return FALSE;
    // On a route with a sand road, a strip of grass between the fence and
    // the sand (as on Route 117), so the road is a unit further off.
    for (y = uy - 1 - sand; y <= uy + uh + sand; y++)
    {
        for (x = ux - 1 - sand; x <= ux + uw + sand; x++)
        {
            u8 c = Cell(gen, x, y);
            bool8 ring = x >= ux - 1 && x <= ux + uw && y >= uy - 1 && y <= uy + uh;
            if (ring && (c == C_LEDGE || c == C_DECOR || (sand && c == C_ROAD)
                      || (c == C_TREE && !LandTreeGoes(gen, x, y))))
                return FALSE;
            if (c == C_ROAD)
                road = TRUE;
        }
    }
    return road || !byRoad;
}

// Two to four fenced beds beside the way, none cutting anything off. Each
// bed takes a spot picked at random from all those it fits, by the trail
// if it can; trees where it stands and round it give way, so it stands in
// a clearing of grass.
static void LandFlowerBeds(struct Gen *gen, int startX, int startY)
{
    int want = 2 + RAND(3), placed = 0, tries;
    int sand = RouteRoadStyle(gen->r, gen->p - gen->r->places) ? 1 : 0;
    for (tries = 0; tries < 20 && placed < want; tries++)
    {
        // (v1.1c land2) The big bed first wherever it fits, as Route 117's
        // beds are big; the small ones only where it does not, or now and
        // then for variety.
        const struct LandBed *b = &sLandBeds[tries < 8 && !CHANCE(20) ? 1 : RAND(sizeof(sLandBeds) / sizeof(sLandBeds[0]))];
        int ux, uy, x, y, n = 0, pick, before, nOpen;
        u8 was[5 * 3];
        bool8 byRoad = TRUE;
        for (uy = 1; uy + b->uh < gen->ch; uy++)
            for (ux = 1; ux + b->uw < gen->cw; ux++)
                n += LandBedFits(gen, ux, uy, b->uw, b->uh, TRUE, sand);
        if (n == 0)
        {
            byRoad = FALSE;
            for (uy = 1; uy + b->uh < gen->ch; uy++)
                for (ux = 1; ux + b->uw < gen->cw; ux++)
                    n += LandBedFits(gen, ux, uy, b->uw, b->uh, FALSE, sand);
        }
        if (n == 0)
            continue;
        pick = RAND(n);
        for (uy = 1; uy + b->uh < gen->ch; uy++)
        {
            for (ux = 1; ux + b->uw < gen->cw; ux++)
                if (LandBedFits(gen, ux, uy, b->uw, b->uh, byRoad, sand) && pick-- == 0)
                    break;
            if (ux + b->uw < gen->cw)
                break;
        }
        before = FloodReachable(gen, startX, startY);
        nOpen = 0;
        for (y = uy; y < uy + b->uh; y++)
        {
            for (x = ux; x < ux + b->uw; x++)
            {
                was[(y - uy) * 5 + (x - ux)] = Cell(gen, x, y);
                nOpen += Cell(gen, x, y) == C_OPEN;
                SetCell(gen, x, y, C_DECOR);
            }
        }
        if (FloodReachable(gen, startX, startY) != before - nOpen
         || !AddDecor(gen, b->blocks, ux * 2, uy * 2, b->uw * 2, b->uh * 2))
        {
            for (y = uy; y < uy + b->uh; y++)
                for (x = ux; x < ux + b->uw; x++)
                    SetCell(gen, x, y, was[(y - uy) * 5 + (x - ux)]);
            continue;
        }
        for (y = uy * 2; y < (uy + b->uh) * 2; y++)
            for (x = ux * 2; x < (ux + b->uw) * 2; x++)
                SetCover(gen, x, y, V_NONE);
        // The trees round it give way to grass (opening ground cuts nothing
        // off).
        for (y = uy - 1; y <= uy + b->uh; y++)
            for (x = ux - 1; x <= ux + b->uw; x++)
                if (Cell(gen, x, y) == C_TREE)
                    SetCell(gen, x, y, C_OPEN);
        placed++;
    }
#ifdef LAND_DEBUG
    fprintf(stderr, "beds place=%d placed=%d/%d\n", (int)(gen->p - gen->r->places), placed, want);
#endif
}

// Once the ground is painted: the blocks round each bed's fence are short
// grass, as on Route 117 (no tall grass or loose flowers against the
// fence). Paths stay.
static void LandBedMargins(struct Gen *gen)
{
    int i, x, y, k;
    for (i = 0; i < gen->nDecor; i++)
    {
        const struct Decor *d = &gen->decor[i];
        bool8 bed = FALSE;
        for (k = 0; k < (int)(sizeof(sLandBeds) / sizeof(sLandBeds[0])); k++)
            if (d->blocks == sLandBeds[k].blocks)
                bed = TRUE;
        if (!bed)
            continue;
        for (y = d->y - 1; y <= d->y + d->h; y++)
        {
            for (x = d->x - 1; x <= d->x + d->w; x++)
            {
                u8 cv;
                if (!InMap(gen, x, y) || (x >= d->x && x < d->x + d->w && y >= d->y && y < d->y + d->h))
                    continue;
                cv = Cover(gen, x, y);
                if (cv == V_TALL || cv == V_FLOWER)
                    SetCover(gen, x, y, V_NONE);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// The long grass (Routes 119 and 120)

// May this tree unit give way to open ground for the long grass: not the
// map's edge, a tree kept round a narrow way in, by an opening or a lane to
// an obstacle (NearExitU), or within sight of a neighbour with another
// tileset, where the grass could not be long.
static bool8 LandFieldTreeGoes(struct Gen *gen, int x, int y, int top)
{
    return y >= top && !IsEdge(gen, x, y) && Cell(gen, x, y) == C_TREE && !(gen->guardEnd && BitU(gen->keep, gen, x, y))
        && !NearExitU(gen, x, y, 3) && !InRouteSeam(gen, x * 2, y * 2) && !InRouteSeam(gen, x * 2 + 1, y * 2 + 1);
}

// Three or four wide fields opened out of the trees along the trail, as
// Route 120's long grass fills the ground between its tree lines, with
// their corners cut so they read as clearings, not boxes. Then a tree line
// only one unit thick between two open stretches goes too (Route 120 has
// none: its trees stand in wide stands, and one-unit strips between fields
// read as a hedge maze). Only trees turn to open ground, so nothing is cut
// off.
static void LandOpenFields(struct Gen *gen, int top)
{
    int n = 3 + RAND(2), i, x, y, pass;
    for (i = 0; i < n && gen->nPath > 4; i++)
    {
        int pi = 2 + ((i * 2 + 1) * (gen->nPath - 4)) / (n * 2);
        int cx = gen->path[pi] % gen->cw + (int)RAND(3) - 1, cy = gen->path[pi] / gen->cw + (int)RAND(3) - 1;
        int rw = 5 + RAND(4), rh = 3 + RAND(3);
        for (y = cy - rh; y <= cy + rh; y++)
        {
            YIELD();
            for (x = cx - rw; x <= cx + rw; x++)
                if (InEllipse(x - cx, y - cy, rw + 1, rh + 1) && LandFieldTreeGoes(gen, x, y, top))
                    SetCell(gen, x, y, C_OPEN);
        }
    }
    // One-unit tree lines between open ground (marked first, so a wider
    // stand is not eaten away a unit at a time). Two passes: a strip whose
    // end was a bump on a wider stand is thin once the bump has gone.
    for (pass = 0; pass < 2; pass++)
    {
        u8 *thin = gen->free; // free until the trainers are placed (PlanLongGrass clears it)
        memset(gen->free, 0, sizeof(gen->free));
        for (y = 1; y < gen->ch - 1; y++)
        {
            YIELD();
            for (x = 1; x < gen->cw - 1; x++)
            {
                bool8 we = Cell(gen, x - 1, y) == C_OPEN && Cell(gen, x + 1, y) == C_OPEN;
                bool8 ns = Cell(gen, x, y - 1) == C_OPEN && Cell(gen, x, y + 1) == C_OPEN;
                if ((we || ns) && LandFieldTreeGoes(gen, x, y, top))
                    thin[(y * gen->cw + x) >> 3] |= 1 << ((y * gen->cw + x) & 7);
            }
        }
        for (y = 1; y < gen->ch - 1; y++)
            for (x = 1; x < gen->cw - 1; x++)
                if ((thin[(y * gen->cw + x) >> 3] >> ((y * gen->cw + x) & 7)) & 1)
                    SetCell(gen, x, y, C_OPEN);
    }
    memset(gen->free, 0, sizeof(gen->free));
}

// Long grass in a few wide fields over the open ground, as on Routes 119
// and 120: each field two or three overlapping ovals, so its edge is
// irregular, with short grass between the fields to wind along. Not by
// the openings (the first steps in stay short grass) nor beside a path,
// and not within sight of a neighbour with another tileset (InRouteSeam),
// where PlanLongGrass could only draw ordinary tall grass. Paths stay
// paths. (Before v1.1c this was bands two blocks tall with one-block lanes
// between, which read as a maze of hedges.)
#define LAND_MAX_LOBES 24
static void LandLongGrassFields(struct Gen *gen, int nEnds, const int *ex, const int *ey)
{
    int x, y, k, w = gen->m->w, h = gen->m->h, nLobes = 0, nFields, tries;
    s8 lx[LAND_MAX_LOBES], ly[LAND_MAX_LOBES];
    u8 lrx[LAND_MAX_LOBES], lry[LAND_MAX_LOBES];
    s16 fcx[8], fcy[8];
    int nf = 0;
    nFields = 5 + RAND(2) + (w * h > 1800);
    for (tries = 0; tries < 80 && nf < nFields; tries++)
    {
        int cx = 3 + RAND(w - 6), cy = 3 + RAND(h - 6), j, lobes, rx, ry;
        bool8 close = FALSE;
        if (!CanCover(gen, cx, cy) || IsPathCover(Cover(gen, cx, cy)))
            continue;
        rx = 7 + RAND(6);
        ry = 4 + RAND(4);
        for (j = 0; j < nf; j++)
            if (Abs(fcx[j] - cx) < 7 && Abs(fcy[j] - cy) < 5)
                close = TRUE;
        if (close)
            continue;
        fcx[nf] = cx;
        fcy[nf] = cy;
        nf++;
        lobes = 2 + RAND(2);
        for (j = 0; j < lobes && nLobes < LAND_MAX_LOBES; j++)
        {
            lx[nLobes] = j == 0 ? cx : cx + (int)RAND(rx * 2 + 1) - rx;
            ly[nLobes] = j == 0 ? cy : cy + (int)RAND(ry + 1) - ry / 2;
            lrx[nLobes] = j == 0 ? rx : 4 + RAND(5);
            lry[nLobes] = j == 0 ? ry : 3 + RAND(3);
            nLobes++;
        }
    }
    for (y = 1; y < h - 1; y++)
    {
        YIELD();
        for (x = 1; x < w - 1; x++)
        {
            u8 cv = Cover(gen, x, y);
            bool8 nearEnd = FALSE, inField = FALSE;
            if (!CanCover(gen, x, y) || (cv != V_NONE && cv != V_TALL && cv != V_FLOWER))
                continue;
            // In view of a neighbour with another tileset the grass could
            // not be long, and a field touching tall grass there would be
            // drawn short-bladed all over (PlanLongGrass): short grass, with
            // a block's margin.
            if (InRouteSeam(gen, x, y) || InRouteSeam(gen, x, y + 1) || InRouteSeam(gen, x, y - 1)
             || InRouteSeam(gen, x - 1, y) || InRouteSeam(gen, x + 1, y) || InSeamView(gen, x >> 1, y >> 1, 1, 1)
             || InSeamView(gen, x >> 1, (y + 1) >> 1, 1, 1))
            {
                if (cv == V_TALL)
                    SetCover(gen, x, y, V_NONE);
                continue;
            }
            for (k = 0; k < nEnds && !nearEnd; k++)
                if (Abs((x >> 1) - ex[k]) + Abs((y >> 1) - ey[k]) < 3)
                    nearEnd = TRUE;
            // A strip of short grass along a path's edge, as Hoenn keeps.
            for (k = 0; k < 4 && !nearEnd; k++)
                if (InMap(gen, x + sDirDX[k], y + sDirDY[k]) && IsPathCover(Cover(gen, x + sDirDX[k], y + sDirDY[k])))
                    nearEnd = TRUE;
            if (nearEnd)
            {
                if (cv == V_TALL)
                    SetCover(gen, x, y, V_NONE);
                continue;
            }
            for (k = 0; k < nLobes && !inField; k++)
                inField = InEllipse(2 * (x - lx[k]), 2 * (y - ly[k]), 2 * lrx[k] + 1, 2 * lry[k] + 1); // (no one-block tips)
            if (inField)
                SetCover(gen, x, y, V_TALL);
            else if (cv == V_TALL)
                SetCover(gen, x, y, V_NONE);
        }
    }
}
