// The volcano (RDUN_VOLCANO): the dungeon inside the region's volcano, as
// Fiery Path, Mt. Chimney and Mt. Ember are Hoenn's and Kanto's. Included by
// rogue_gen_dungeon.h; GenCave draws its floors in Fiery Path's rock
// (CSTYLE_FIERY, gTileset_Lavaridge), with drifts of ash, Strength boulders
// and a lava lake by the guardian on the bottom floor.
//
// A region has one only where the region map draws Mt. Chimney's crater
// (RogueGen_VolcanoCell: on the land's volcano, or beside the first ash
// town), and its way in is a place beside the crater, so the map's volcano
// and the dungeon are one. A cave there becomes it (a side cave's floors go
// down into the mountain; the main road's cave becomes the way through, as
// Fiery Path is); failing that, a door in the rock face of a town beside
// it, as Mt. Pyre's and the Aqua Hideout's are entered. That door is at the
// top of the town, so the map draws the crater above it (MapCraterByDoor).

// How far a place's cell lies from the crater's (the larger of the two
// steps, so a diagonal neighbour is beside it too).
static int CraterReach(const struct RoguePlace *p, int vx, int vy)
{
    int dx = Abs(p->gridX - vx), dy = Abs(p->gridY - vy);
    return dx > dy ? dx : dy;
}

// v1.1: a place that can take the volcano's own mountain along its top
// (PlaceVolcanoHill), drawn with Lavaridge's tileset: a spring town, as Mt.
// Chimney stands over Lavaridge, or a route beside the crater, the foot
// route (two badges on at least), which takes Lavaridge's tileset for it as
// Route 112 does. Nothing may lead in from the north, where it stands, nor
// from a side beside it, and the route is one GenRoute draws with a mesa
// along its top (PlaceVolcanoHill is called where a cave's mesa would be:
// a kind with its own generator that skips that must be left out here).
// A foot route's kinds: drawn from the General tileset alone, as a foot
// route takes Lavaridge's (flower beds from Mauville, long grass and log
// bridges from Fortree would show as the wrong tiles).
static bool8 VolcanoFootKind(u8 type)
{
    return type == RROUTE_FIELD || type == RROUTE_FOREST || type == RROUTE_WOODS;
}

// A route of another kind the volcano may turn into a forest to be its foot
// route (v1.1: the zones put few plain routes by the crater). It must be
// as tall as a forest (sizes are set before the dungeons).
static bool8 VolcanoFootConvertible(u8 type)
{
    return type == RROUTE_LAKE || type == RROUTE_RIVER || type == RROUTE_FLOWERS || type == RROUTE_LONGGRASS
        || type == RROUTE_HILLS || type == RROUTE_DESERT;
}

// The units along the top the mountain stands on: all along, or (a route
// with a way north, v1.1) the wider side of the way north, with two units of
// woods kept between it and the way's opening (VolcanoGapTrees), so its end
// is cut by trees as its ends at the map's edges are. FALSE if no side is
// eight units wide.
static bool8 VolcanoSpan(const struct RogueRegion *r, const struct RoguePlace *p, int *x0, int *w)
{
    const struct RogueLink *n = LinkOnSide(p, RDIR_NORTH);
    int cw = p->w / 2, e, lw, rw;
    if (!n)
    {
        *x0 = 1;
        *w = cw - 2;
        return TRUE;
    }
    if (p->kind != RPLACE_ROUTE || RockyRoute(&r->places[n->target]))
        return FALSE;
    e = n->pos / 2;
    lw = e - 5;
    rw = cw - 6 - e;
    *x0 = lw >= rw ? 1 : e + 5;
    *w = lw >= rw ? lw : rw;
    return *w >= 8;
}

// The units of woods between the mountain and a way north (k 0 and 1, out
// from the mountain's end toward the way), rows 0-5.
static int VolcanoGapX(const struct RogueRegion *r, const struct RoguePlace *p, int k)
{
    int x0, w;
    VolcanoSpan(r, p, &x0, &w);
    return x0 == 1 ? x0 + w + k : x0 - 1 - k;
}

// v1.1: a route beside the crater (one or two cells off) that could be the
// volcano's foot route, before its size and openings are set: not the sea,
// a coast, a bridge, Victory Road, a dead end or a cave's way up.
static bool8 VolcanoFootSite(const struct RoguePlace *p, int vx, int vy)
{
    int near;
    if (vx < 0 || p->kind != RPLACE_ROUTE || p->mouth != 0xFF || (p->side && p->nLinks == 1)
     || p->type == RROUTE_SEA || p->type == RROUTE_COAST || p->type == RROUTE_BRIDGE || p->type == RROUTE_MOUNTAIN)
        return FALSE;
    near = CraterReach(p, vx, vy);
    return near >= 1 && near <= 2;
}

// Routes beside the crater are made as roomy as a meadow's (36 blocks
// across the way through, 44 along it), so the mountain and the two rows
// kept under it leave room for the way.
static void VolcanoFootSizes(struct RogueRegion *r)
{
    s8 vx, vy;
    int i;
    if (!VolcanoCell(r, &vx, &vy, FALSE))
        return;
    for (i = 0; i < r->nPlaces; i++)
    {
        struct RoguePlace *p = &r->places[i];
        bool8 vertical = (HasSide(p, RDIR_NORTH) || LinkOnSide(p, RDIR_SOUTH)) && !LinkOnSide(p, RDIR_EAST)
                      && !LinkOnSide(p, RDIR_WEST);
        if (!VolcanoFootSite(p, vx, vy))
            continue;
        if (p->w < (vertical ? 36 : 44))
            p->w = vertical ? 36 : 44;
        if (p->h < (vertical ? 44 : 36))
            p->h = vertical ? 44 : 36;
    }
}

// And their openings: at the sides below the mountain and its kept rows
// (unit 7 on), and a way north to one side, leaving eight units beside it
// for the mountain (VolcanoSpan).
static void VolcanoFootExits(const struct RogueRegion *r, const struct RoguePlace *p, u8 dir, int len, int vx, int vy,
                             int *first, int *last)
{
    if (!VolcanoFootSite(p, vx, vy))
        return;
    if (dir == RDIR_EAST || dir == RDIR_WEST)
    {
        if (*last >= 7 && *first < 7)
            *first = 7;
    }
    else if (dir == RDIR_NORTH)
    {
        if (len - 14 >= *first && *last > len - 14)
            *last = len - 14;
        else if (*last >= 13 && *first < 13)
            *first = 13;
    }
}

// Whether the place has room for the mountain, whatever kind of route it is.
static bool8 VolcanoMountainShape(const struct RogueRegion *r, u8 place)
{
    const struct RoguePlace *p = &r->places[place];
    const struct RogueLink *e = LinkOnSide(p, RDIR_EAST), *w = LinkOnSide(p, RDIR_WEST);
    int x0, span;
    if (p->mouth != 0xFF || RogueGen_TunnelMouthAt(r, place) || !VolcanoSpan(r, p, &x0, &span))
        return FALSE;
    // The ways in at the sides come out below it (it takes four units and
    // the row of ground under it).
    if ((e && e->pos / 2 < 6) || (w && w->pos / 2 < 6))
        return FALSE;
    // (v1.1 towns2: not over a harbor's sea along the top or a side, which
    // would run under the rock: seed 165's, once a raft village moved its
    // region.)
    if (p->kind == RPLACE_TOWN)
        return p->theme == RTHEME_SPRING && p->type != RTOWN_SUMMIT && place != r->start
            && !(p->layout == RLAYOUT_HARBOR && p->seaSide != RDIR_NONE && p->seaSide != RDIR_SOUTH);
    // (Not one an obstacle guards, or a block next door stands in the lane
    // of: the way to the doorway would go round it.)
    if (p->kind != RPLACE_ROUTE || p->obstacle || p->seaSide != RDIR_NONE)
        return FALSE;
    {
        int k;
        for (k = 0; k < p->nLinks; k++)
        {
            const struct RoguePlace *t = &r->places[p->links[k].target];
            if (ObstacleOutside(r, t) && t->guardDir == Opposite(p->links[k].dir))
                return FALSE;
        }
    }
    return TRUE;
}

// A route within two steps (a route next door, or one across a town next
// door) of this kind.
static bool8 VolcanoKindNear(const struct RogueRegion *r, const struct RoguePlace *p, u8 type)
{
    int k, j;
    for (k = 0; k < p->nLinks; k++)
    {
        const struct RoguePlace *t = &r->places[p->links[k].target];
        if (t->kind == RPLACE_ROUTE && t->type == type)
            return TRUE;
        for (j = 0; j < t->nLinks; j++)
        {
            const struct RoguePlace *u = &r->places[t->links[j].target];
            if (u != p && u->kind == RPLACE_ROUTE && u->type == type)
                return TRUE;
        }
    }
    return FALSE;
}

// The kind a converted foot route takes: one no route near it has, so three
// alike never stand in a row.
static u8 VolcanoFootPick(const struct RogueRegion *r, const struct RoguePlace *p)
{
    static const u8 sKinds[] = { RROUTE_FOREST, RROUTE_FIELD, RROUTE_WOODS };
    int i;
    for (i = 0; i < (int)(sizeof(sKinds) / sizeof(sKinds[0])); i++)
        if (!VolcanoKindNear(r, p, sKinds[i]))
            return sKinds[i];
    return RROUTE_FOREST;
}

static bool8 VolcanoMountainFits(const struct RogueRegion *r, u8 place)
{
    const struct RoguePlace *p = &r->places[place];
    return VolcanoMountainShape(r, place) && (p->kind == RPLACE_TOWN || VolcanoFootKind(p->type));
}

// Whether the volcano is entered here by its own mountain's doorway.
static bool8 VolcanoMountainAt(const struct RogueRegion *r, u8 place)
{
    s32 di = RogueGen_DungeonAt(r, place);
    return di >= 0 && r->dungeons[di].kind == RDUN_VOLCANO && r->dungeons[di].host == place
        && VolcanoMountainFits(r, place);
}

// A place by the crater that could be the volcano's way in, and how well it
// fits: 0 if not at all. Beside it first, then two cells off. First of all
// (v1.1) a place with the volcano's own mountain two cells off at most: a
// spring town, or a route to be the foot route; then a cave, then another
// town, whose ways in are the brown cave hill.
static int VolcanoHostScore(const struct RogueRegion *r, int vx, int vy, u32 place, int reach, u32 *rng)
{
    const struct RoguePlace *p = &r->places[place];
    s32 di = RogueGen_DungeonAt(r, place);
    int near = CraterReach(p, vx, vy);
    // (v1.1 towns2: not a crater town's route, whose white rock the town's
    // seam needs: seed 86's.)
    if (p->kind == RPLACE_ROUTE && p->theme == RTHEME_CRATER)
        return 0;
    // (reach 0: the first look, for a place with the mountain, two cells
    // off at most.)
    if (reach == 0)
    {
        if (near > 2 || di >= 0 || p->kind == RPLACE_CAVE)
            return 0;
        // A route of another kind can still be the foot route, made a
        // forest: after the places that fit as they are.
        if (p->kind == RPLACE_ROUTE && !VolcanoMountainFits(r, place) && VolcanoMountainShape(r, place)
         && VolcanoFootConvertible(p->type) && p->h >= 36 && near >= 1 && p->badges >= 1)
            return 30 - near * 6 + RogueRng_Range(rng, 4);
        if (!VolcanoMountainFits(r, place))
            return 0;
        reach = 2;
    }
    if (near > reach || (near > 2 && p->kind != RPLACE_TOWN))
        return 0;
    if (p->kind == RPLACE_CAVE)
    {
        // A cave's own dungeon becomes the volcano's, but never Victory Road.
        const struct RogueDungeon *d = di >= 0 ? &r->dungeons[di] : NULL;
        if (d == NULL || d->host != place || d->kind != RDUN_CAVE)
            return 0;
        return 40 - near * 8 + !d->required * 4 + RogueRng_Range(rng, 4);
    }
    if (p->kind == RPLACE_ROUTE)
        return near >= 1 && near <= 2 && di < 0 && p->badges >= 1 && VolcanoMountainFits(r, place)
             ? 50 - near * 6 + RogueRng_Range(rng, 4) : 0;
    if (p->kind != RPLACE_TOWN || p->type == RTOWN_SUMMIT || place == r->start || di >= 0 || TOWNS2_TOWN(p))
        return 0;
    if (near <= 2 && VolcanoMountainFits(r, place))
        return 60 - near * 8 + RogueRng_Range(rng, 4);
    // A town the crater stands north of, as Lavaridge lies under Mt.
    // Chimney, and one along the way rather than an early one. One three
    // cells off is taken only when the map can draw the crater over it.
    return 20 - (near > 2 ? 2 : near) * 8 + (p->gridY > vy) * 4 + (p->badges >= 2) * 2 + RogueRng_Range(rng, 2);
}

static void PlanVolcano(struct RogueRegion *r)
{
    u32 rng = RogueRng_Hash(r->seed, 11900);
    u64 tried = 0;
    int reach;
    s8 vx, vy;

    if (!VolcanoCell(r, &vx, &vy, FALSE))
        return;
    for (reach = 0; reach <= 3; reach++)
    {
        for (;;)
        {
            int i, best = -1, bestScore = 0;
            u8 theme, type;
            for (i = 0; i < r->nPlaces; i++)
            {
                int score = ((tried >> i) & 1) ? 0 : VolcanoHostScore(r, vx, vy, i, reach, &rng);
                if (score > bestScore)
                {
                    bestScore = score;
                    best = i;
                }
            }
            if (best < 0)
                break;
            tried |= (u64)1 << best;
            if (r->places[best].kind == RPLACE_CAVE)
            {
                struct RogueDungeon *d = &r->dungeons[RogueGen_DungeonAt(r, best)];
                d->kind = RDUN_VOLCANO;
                if (!d->required)
                    d->nFloors = 2 + (d->badges >= 4 && RogueRng_Range(&rng, 2));
                return;
            }
            // A town: a door into the mountain in the rock along its top. A
            // town with no room for it is passed over, and so is one the map
            // cannot draw the crater above (MapCraterByDoor), so the door
            // never faces away from the volcano. The foot route's doorway is
            // along its top too, and it takes Lavaridge's tileset (v1.1).
            if (!AddDungeon(r, RDUN_VOLCANO, best, 2 + (r->places[best].badges >= 4), RBOSS_GUARDIAN, FALSE))
                return;
            theme = r->places[best].theme;
            type = r->places[best].type;
            if (r->places[best].kind == RPLACE_ROUTE)
            {
                r->places[best].theme = RTHEME_SPRING;
                if (!VolcanoFootKind(type))
                    r->places[best].type = VolcanoFootPick(r, &r->places[best]);
            }
            if (r->places[best].kind == RPLACE_ROUTE || TownDungeonEntrance(r, best) == RENTRY_MESA)
            {
                s8 dx, dy;
                VolcanoCell(r, &dx, &dy, TRUE);
                if (dy < r->places[best].gridY && CraterReach(&r->places[best], dx, dy) <= 2)
                    return;
            }
            r->places[best].theme = theme;
            r->places[best].type = type;
            r->nDungeons--;
        }
    }
}

// ---------------------------------------------------------------------------
// Floors in another cave's look (CaveStyle), for the ice cave and the
// volcano. GenCave lays the chambers as for any cave; these draw them and
// add each kind's own things.

static u8 CaveStyle(struct Gen *gen)
{
    u8 kind;
    if (!sGenFloor)
        return CSTYLE_CAVE;
    kind = gen->r->dungeons[sGenDungeon].kind;
    return kind == RDUN_ICE ? CSTYLE_ICE : kind == RDUN_VOLCANO ? CSTYLE_FIERY : CSTYLE_CAVE;
}

static bool8 IceWallsGen(struct Gen *gen, int fx, int fy, u8 l, const bool8 *lower, u16 *block);
static u16 IceBlock(struct Gen *gen, int fx, int fy, u8 l, const bool8 *lower);

// The heart of a volcano is in the Magma Hideout's look, its smooth floor
// 0x3A2 as round Groudon's pool; the floors above are Fiery Path's rough
// 0x308. Only the heart has lava, as the lava's shore (0x3A3) meets only
// the Magma Hideout's floor in Hoenn, and the two floors never meet (v1.1).
static bool8 FieryMagma(struct Gen *gen)
{
    return sGenFloor == DungeonGenFloors(&gen->r->dungeons[sGenDungeon]);
}

// A block of lava: a C_WATER unit on a volcano floor, every block of it
// (the lake's corners are drawn with the lake's own pieces, not bitten off).
static bool8 LavaF(struct Gen *gen, int fx, int fy)
{
    return InMap(gen, fx, fy) && Cell(gen, fx >> 1, fy >> 1) == C_WATER;
}

// The piece for each block of a volcano floor comes from sFieryWalls
// (src/data/rogue_fiery_walls.h, made by tools/roguemon/gen_fiery_walls.py):
// for each look of a block and its eight neighbours (each floor, lava or a
// ring of rock one, two or three deep), the piece of Fiery Path's or the
// Magma Hideout's that only ever meets its neighbours' pieces as they meet
// in Hoenn's maps. A look the table lacks falls back on FieryRule.
#include "data/rogue_fiery_walls.h"

// The pieces by rule: the inner ring's faces and corners (0x306-0x30F,
// 0x274 above the floor), the outer ring's (0x270, 0x272, 0x274) and the
// rock top (0x271). Next to the lava, the Magma Hideout's rims (0x346,
// 0x347, 0x289) and the lake's own corners and shore.
static u16 FieryRule(struct Gen *gen, int fx, int fy, u8 l, const bool8 *lower)
{
    bool8 n, s, e, w, ne, nw, se, sw;
    if (l == 0)
    {
        if (LavaF(gen, fx, fy))
        {
            n = !LavaF(gen, fx, fy - 1);
            s = !LavaF(gen, fx, fy + 1);
            e = !LavaF(gen, fx + 1, fy);
            w = !LavaF(gen, fx - 1, fy);
            if (n && w) return 0x061D;
            if (n && e) return 0x061B;
            if (s && e) return 0x0613;
            if (s && w) return 0x0615;
            return 0x06BD;
        }
        // The lake's shore, as below the Magma Hideout's.
        if (LavaF(gen, fx, fy - 1))
            return 0x03A3;
        if (LavaF(gen, fx - 1, fy - 1) || LavaF(gen, fx + 1, fy - 1))
            return 0x03A4;
        return FieryMagma(gen) ? 0x33A2 : 0x3308;
    }
    n = lower[1];
    s = lower[7];
    e = lower[5];
    w = lower[3];
    ne = lower[2];
    nw = lower[0];
    se = lower[8];
    sw = lower[6];
    if (l == 1)
    {
        if (s && LavaF(gen, fx, fy + 1)) return 0x0747;
        if (e && LavaF(gen, fx + 1, fy)) return 0x0746;
        if (w && LavaF(gen, fx - 1, fy)) return 0x0689;
        if (n && s) return 0x0674;
        if (e && w) return 0x0706;
        if (s && w) return 0x070E;
        if (s && e) return 0x070F;
        if (n && w) return 0x070A;
        if (n && e) return 0x070B;
        if (s)      return 0x0674;
        if (n)      return 0x070C;
        if (w)      return 0x0706;
        if (e)      return 0x0707;
        if (se)     return 0x067B;
        if (sw)     return 0x067C;
        if (ne)     return 0x067F;
        if (nw)     return 0x067E;
        return 0x0671;
    }
    if (l == 2)
    {
        if (s)      return 0x0674;
        if (n)      return 0x0671;
        if (w && !e) return 0x0670;
        if (e && !w) return 0x0672;
        if (se)     return 0x067B;
        if (sw)     return 0x067C;
    }
    return 0x0671;
}

static u16 FieryBlock(struct Gen *gen, int fx, int fy, u8 l, const bool8 *lower, const u8 *lev)
{
    int w = gen->m->w, h = gen->m->h, i, lo = 0, hi = (int)(sizeof(sFieryWalls) / sizeof(sFieryWalls[0])) - 1;
    bool8 magma = FieryMagma(gen), open = TRUE;
    u32 key = magma;
    u16 block = 0;
    for (i = 8; i >= 0; i--)
    {
        int nx = fx + (i % 3) - 1, ny = fy + (i / 3) - 1;
        u8 c = (nx < 0 || ny < 0 || nx >= w || ny >= h) ? 3 : LavaF(gen, nx, ny) ? 4 : lev[ny * w + nx];
        key = key * 5 + c;
        open &= c == 0;
    }
    while (lo <= hi)
    {
        int mid = (lo + hi) / 2;
        if (sFieryWalls[mid].key == key)
        {
            block = sFieryWalls[mid].block;
            break;
        }
        if (sFieryWalls[mid].key < key)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    if (block == 0)
        return FieryRule(gen, fx, fy, l, lower);
    if (l > 0 || LavaF(gen, fx, fy))
        return 0x0400 | block;
    if (block == 0x3A3 || block == 0x3A4)
        return block;
    // The floor glints here and there, as Fiery Path's and the Magma
    // Hideout's do: out on the floor, never two side by side.
    if (open && ((fx + 2 * fy) & 3) == 0 && RogueRng_Hash(GenPlaceId(gen), fy * 64 + fx) % 100 < 14)
        return 0x3000 | (magma ? 0x313 : 0x310 + ((fx + fy) & 1));
    return 0x3000 | block;
}

static u16 StyledCaveBlock(struct Gen *gen, u8 style, int fx, int fy, u8 l, const bool8 *lower, const u8 *lev)
{
    if (style == CSTYLE_ICE)
        return IceBlock(gen, fx, fy, l, lower);
    return FieryBlock(gen, fx, fy, l, lower, lev);
}

// The ways between floors: the ice room's ladders (up the one standing on
// the floor, down the one in the rock), or on a volcano floor a doorway in
// the rock face with its frame above, as the Magma Hideout's and Jagged
// Pass's are.
static void StyledLadder(struct Gen *gen, u8 style, int fx, int fy, bool8 up)
{
    if (style == CSTYLE_ICE)
    {
        Put(gen, fx, fy, up ? 0x338B : 0x337F);
        return;
    }
    Put(gen, fx, fy, 0x025E);
    if (fy > 0)
        Put(gen, fx, fy - 1, 0x0656);
}

// Whether a volcano's doorway can stand at block (fx, y) of the rock with
// the floor below it: rock two blocks deep over it, and the face running on
// two blocks to each side of it over the floor, as every doorway's does in
// Hoenn (strict), or at least one (loose: a corner of the rock may stand
// beside it, as v1.0 placed them).
// A unit of rock beside the floor under a doorway that may be dug out so
// the floor runs on under the doorway's face: it joins nothing new, its
// only floor beside it the unit toward the doorway (dx from it).
static bool8 FieryDigOk(struct Gen *gen, int ux, int uy, int dx)
{
    if (ux < 1 || ux >= gen->cw - 1 || CellTaken(gen, ux, uy))
        return FALSE;
    return Cell(gen, ux - dx, uy) == C_ROAD && Cell(gen, ux + dx, uy) == C_TREE
        && Cell(gen, ux, uy + 1) == C_TREE && Cell(gen, ux, uy - 1) == C_TREE;
}

// Digs the floor under a doorway at (fx, y) out to two blocks each side.
static bool8 FieryDoorDig(struct Gen *gen, int fx, int y, bool8 dig)
{
    int d, uy = (y + 1) >> 1;
    // (Never in a tunnel, whose barrier must hold: a bay beside its choke
    // could lead round it.)
    if (gen->r->dungeons[sGenDungeon].exit != 0xFF)
        return FALSE;
    for (d = -2; d <= 2; d += 4)
    {
        int ux = (fx + d) >> 1;
        if (Cell(gen, ux, uy) == C_ROAD)
            continue;
        if (!FieryDigOk(gen, ux, uy, d < 0 ? -1 : 1))
            return FALSE;
        if (dig)
            SetCell(gen, ux, uy, C_ROAD);
    }
    return TRUE;
}

static bool8 FieryDoorFits(struct Gen *gen, int fx, int y, u8 strict)
{
    int k = strict ? 2 : 1, d;
    if (y < 2 || fx < k || fx >= gen->m->w - k || Cell(gen, fx >> 1, (y - 2) >> 1) != C_TREE
     || Cell(gen, fx >> 1, (y + 1) >> 1) != C_ROAD || IsOccupied(gen, fx, y + 1))
        return FALSE;
    for (d = -k; d <= k; d++)
    {
        if (Cell(gen, (fx + d) >> 1, y >> 1) != C_TREE || Cell(gen, (fx + d) >> 1, (y - 1) >> 1) != C_TREE)
            return FALSE;
        if (strict == 1 && Cell(gen, (fx + d) >> 1, (y + 1) >> 1) != C_ROAD)
            return FALSE;
    }
    // (strict 2: the floor below may be dug out to fit, FieryDoorDig.)
    return strict != 2 || FieryDoorDig(gen, fx, y, FALSE);
}

// A volcano's doorway stands in the north wall over a chamber's floor, the
// rock two blocks deep above it (Fiery Path has no ladders): in a straight
// run of the wall, else at the back of a nook dug out of it (as the cave's
// ladders stand in nooks), else up the floor where the chamber's wall or a
// bay of it ends. Each way is tried first with the face running on at both
// sides (FieryDoorFits). FALSE if the chamber has none.
static bool8 PlaceFieryDoor(struct Gen *gen, const struct Rect *r, u8 kind, int *lx, int *ly)
{
    int y = r->y * 2 - 1, fx, n, tries, pass;
    u8 cand[16], candY[16];
    if (r->y < 2)
        return FALSE;
    // (Loose only once a floor has been tried over and over.)
    for (pass = 0; pass < (gen->attempt < 3 ? 2 : 3); pass++)
    {
        // Strict, then strict with the floor below dug out, then loose.
        u8 strict = pass == 0 ? 1 : pass == 1 ? 2 : 0;
        n = 0;
        for (fx = r->x * 2 + 1; fx < (r->x + r->w) * 2 - 1 && n < 16; fx++)
            if (FieryDoorFits(gen, fx, y, strict) && !IsOccupied(gen, fx - 1, y + 1) && !IsOccupied(gen, fx + 1, y + 1))
                cand[n++] = fx;
        if (n > 0)
        {
            *lx = cand[RAND(n)];
            *ly = y;
            if (strict == 2)
                FieryDoorDig(gen, *lx, y, TRUE);
            break;
        }
        // A nook dug out of the wall: three units wide with the doorway in
        // its middle unit, or one (loose).
        for (tries = 0; tries < 12; tries++)
        {
            int x = r->x + RAND(r->w), uy = r->y - 1, k, half = strict != 0;
            // (Over the chamber's own floor all along, so it opens on
            // nothing else.)
            bool8 ok = uy >= 3 && x >= 2 + half && x <= gen->cw - 3 - half
                    && x - half >= r->x && x + half < r->x + r->w;
            for (k = -half - 1; k <= half + 1 && ok; k++)
                ok = Cell(gen, x + k, uy) == C_TREE && Cell(gen, x + k, uy - 1) == C_TREE;
            if (!ok || Cell(gen, x, uy + 1) != C_ROAD || CellTaken(gen, x, uy + 1))
                continue;
            for (k = -half; k <= half; k++)
                SetCell(gen, x + k, uy, C_ROAD);
            *lx = x * 2 + RAND(2);
            *ly = uy * 2 - 1;
            break;
        }
        if (tries < 12)
            break;
        // Up the floor from the chamber's top row, to the first rock with
        // rock beside it: the chamber's wall or the end of a bay.
        for (fx = r->x * 2; fx < (r->x + r->w) * 2 && n < 16; fx++)
        {
            int wy = r->y * 2;
            // (A unit up at most: a bay of the chamber, not a tunnel to
            // somewhere else.)
            while (wy > 2 && wy > r->y * 2 - 2 && Cell(gen, fx >> 1, (wy - 1) >> 1) == C_ROAD)
                wy--;
            wy--;
            if (FieryDoorFits(gen, fx, wy, strict))
            {
                candY[n] = wy;
                cand[n++] = fx;
            }
        }
        if (n > 0)
        {
            n = RAND(n);
            *lx = cand[n];
            *ly = candY[n];
            if (strict == 2)
                FieryDoorDig(gen, *lx, *ly, TRUE);
            break;
        }
    }
    if (pass == (gen->attempt < 3 ? 2 : 3))
        return FALSE;
    AddWarp(gen, *lx, *ly, kind);
    MarkOccupied(gen, *lx, *ly);
    MarkOccupied(gen, *lx, *ly + 1);
    return TRUE;
}

// Each kind's thing in the feature chamber: on a volcano floor rock
// pillars, or on a floor in the Magma Hideout's look a lava lake (no ash:
// Jagged Pass's lies only on its own ground, never on a cave's floor); on
// an ice floor plain chambers, for the puzzle.
static u8 StyledFeature(struct Gen *gen, u8 feature)
{
    u8 style = CaveStyle(gen), k;
    if (style == CSTYLE_CAVE)
        return feature;
    k = RAND(4);
    // (An ice floor's chambers stay plain: the puzzle's patches of snow are
    // reached along its walls.)
    if (style == CSTYLE_ICE)
        return CAVE_PLAIN;
    if (FieryMagma(gen))
        return k == 0 ? CAVE_LAVA : k == 1 ? CAVE_PILLARS : CAVE_PLAIN;
    return k < 2 ? CAVE_PILLARS : CAVE_PLAIN;
}

// Loose rocks and rubble are Granite Cave's: a volcano floor has Fiery
// Path's round rocks (the Magma Hideout has none: a glint of its floor
// where they would lie), an ice floor bare snow.
static u16 StyledRock(struct Gen *gen, u16 block)
{
    switch (CaveStyle(gen))
    {
    case CSTYLE_ICE:   return 0x3385;
    case CSTYLE_FIERY: return FieryMagma(gen) ? 0x3313 : 0x070D; // Fiery Path's round rock
    }
    return block;
}

// A rock that blocks a block of floor (beside a tunnel's guard): the cave's,
// the ice room's snow rock or Fiery Path's round rock.
static u16 StyledSolidRock(struct Gen *gen, u16 block)
{
    switch (CaveStyle(gen))
    {
    case CSTYLE_ICE:   return 0x077D;
    case CSTYLE_FIERY: return 0x070D;
    }
    return block;
}

// The deepest floor of a volcano (its chamber aside) is its heart: a lava
// lake across the back of the far chamber, where the guardian waits, as
// Groudon's pool is in the Magma Hideout.
static void IceReserve(struct Gen *gen, const struct Rect *rooms, int nRooms, int nMain, int best, int sx, int sy);

static void StyledRooms(struct Gen *gen, const struct Rect *rooms, int nRooms, int nMain, int best, int inX, int inY)
{
    const struct RogueDungeon *d;
    int k;
    if (CaveStyle(gen) == CSTYLE_ICE)
        IceReserve(gen, rooms, nRooms, nMain, best, inX, inY);
    if (CaveStyle(gen) != CSTYLE_FIERY)
        return;
    d = &gen->r->dungeons[sGenDungeon];
    if (sGenFloor != DungeonGenFloors(d))
        return;
    // The far chamber, where the guardian waits; else the nearest to it.
    for (k = nMain - 1; k >= 1; k--)
        if (LavaLake(gen, &rooms[k], inX >> 1, inY >> 1))
            return;
}

// A lava lake along the north wall of a chamber, one or two units deep:
// rock above it and at its sides (a rock outcrop is raised in the chamber
// beside it where the lake does not reach the wall), floor below,
// so every edge is one the vanilla lakes have. Everything stays joined to
// (sx, sy). Nothing if no run of the wall takes it.
static bool8 LavaLake(struct Gen *gen, const struct Rect *r, int sx, int sy)
{
    int depth, wide, x0, x1, x, y, y0, before, k;
    if (r->w < 3 || r->h < 3)
        return FALSE;
    before = FloodReachable(gen, sx, sy);
    // Deep and wide first, against the chamber's north wall where it can,
    // else where the rock comes down into it.
    for (depth = r->h >= 5 ? 2 : 1; depth >= 1; depth--)
    {
        for (wide = r->w; wide >= 2; wide--)
        {
            for (y0 = r->y; y0 + depth < r->y + r->h; y0++)
            {
                for (x0 = r->x; x0 + wide <= r->x + r->w; x0++)
                {
                    int n = 0, nRaised = 0;
                    u8 raised[4][2];
                    bool8 ok = TRUE;
                    x1 = x0 + wide - 1;
                    // Rock above all along; at the ends rock, or the
                    // chamber's floor to raise an outcrop on.
                    for (x = x0; x <= x1 && ok; x++)
                        ok = Cell(gen, x, y0 - 1) == C_TREE;
                    for (y = y0; y < y0 + depth && ok; y++)
                    {
                        for (x = x0 - 1; x <= x1 + 1 && ok; x++)
                        {
                            u8 c = Cell(gen, x, y);
                            bool8 side = x < x0 || x > x1, inside = x >= r->x && x < r->x + r->w;
                            if (CellTaken(gen, x, y))
                                ok = FALSE;
                            else if (!side)
                                ok = c == C_ROAD;
                            else if (!inside)
                                ok = c == C_TREE;
                            else
                                ok = c == C_TREE || c == C_ROAD;
                        }
                    }
                    // Floor below all along, to stand on by the lake.
                    for (x = x0; x <= x1 && ok; x++)
                        ok = Cell(gen, x, y0 + depth) == C_ROAD;
                    if (!ok)
                        continue;
                    for (y = y0; y < y0 + depth; y++)
                    {
                        for (x = x0 - 1; x <= x1 + 1; x++)
                        {
                            if (x >= x0 && x <= x1)
                            {
                                SetCell(gen, x, y, C_WATER);
                                n++;
                            }
                            else if (Cell(gen, x, y) == C_ROAD)
                            {
                                SetCell(gen, x, y, C_TREE);
                                raised[nRaised][0] = x;
                                raised[nRaised++][1] = y;
                                n++;
                            }
                        }
                    }
                    if (FloodReachable(gen, sx, sy) == before - n)
                        return TRUE;
                    // It cut a way: put the chamber back.
                    for (y = y0; y < y0 + depth; y++)
                        for (x = x0; x <= x1; x++)
                            SetCell(gen, x, y, C_ROAD);
                    for (k = 0; k < nRaised; k++)
                        SetCell(gen, raised[k][0], raised[k][1], C_ROAD);
                }
            }
        }
    }
#ifdef ROGUE_DEBUG
    fprintf(stderr, "lava: no lake in chamber %d,%d %dx%d\n", r->x, r->y, r->w, r->h);
#endif
    return FALSE;
}

// Strength boulders, as in Fiery Path and Seafoam Islands: a pair across a
// tunnel one unit wide, which one boulder pushed twice opens from either
// side (as PlaceCaveGates' pairs on Victory Road). Off the way, before a
// side chamber with a find in it: a player without Strength comes back for
// it. With wayToo (the player holds Strength by the time the dungeon
// opens), also across the way itself. Never within reach of the ice
// (occupied blocks: the puzzle marks its chamber and mouths). Returns how
// many pairs it placed.
static int PlaceBoulders(struct Gen *gen, int sx, int sy, int ex, int ey, int pairs, bool8 wayToo)
{
    struct RogueMap *m = gen->m;
    u16 cand[24];
    int n = 0, x, y, i, have = 0, placed = 0;

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
        for (x = 2; x < gen->cw - 2 && n < 24; x++)
        {
            bool8 vert = Cell(gen, x, y - 1) == C_ROAD && Cell(gen, x, y + 1) == C_ROAD
                      && Cell(gen, x - 1, y) == C_TREE && Cell(gen, x + 1, y) == C_TREE;
            bool8 horz = Cell(gen, x - 1, y) == C_ROAD && Cell(gen, x + 1, y) == C_ROAD
                      && Cell(gen, x, y - 1) == C_TREE && Cell(gen, x, y + 1) == C_TREE;
            bool8 cut = FALSE;
            int j = 0;
            if (Cell(gen, x, y) != C_ROAD || vert == horz)
                continue;
            if (Abs(x - sx) + Abs(y - sy) <= 2 || Abs(x - ex) + Abs(y - ey) <= 2)
                continue;
            if (CellTaken(gen, x, y)
             || (vert ? (CellTaken(gen, x, y - 1) || CellTaken(gen, x, y + 1) || CellTaken(gen, x, y - 2) || CellTaken(gen, x, y + 2))
                      : (CellTaken(gen, x - 1, y) || CellTaken(gen, x + 1, y) || CellTaken(gen, x - 2, y) || CellTaken(gen, x + 2, y))))
                continue;
            SetCell(gen, x, y, C_TREE);
            FloodReachable(gen, sx, sy);
            SetCell(gen, x, y, C_ROAD);
            if (!Reached(gen, ex, ey))
                cut = wayToo;
            else
            {
                // Before a find, off the way, and nothing else: no ladder,
                // trainer or the rest kept out.
                for (i = 0; i < m->nObjs; i++)
                {
                    bool8 out = !Reached(gen, m->objs[i].x >> 1, m->objs[i].y >> 1);
                    if (out && m->objs[i].kind == ROBJ_ITEM)
                        cut = TRUE;
                    else if (out && m->objs[i].kind != ROBJ_BOULDER)
                        break;
                }
                for (j = 0; j < m->nWarps && i == m->nObjs; j++)
                    if (!Reached(gen, m->warps[j].x >> 1, (m->warps[j].y + 1) >> 1))
                        break;
                if (i < m->nObjs || j < m->nWarps)
                    cut = FALSE;
            }
            if (cut)
                cand[n++] = PACK(x, y);
        }
    }
    while (placed < pairs && n > 0 && have < 6 && m->nObjs + 2 <= ROGUE_MAX_OBJS)
    {
        int c = RAND(n), j;
        x = PX(cand[c]);
        y = PY(cand[c]);
        if (Cell(gen, x, y - 1) == C_ROAD)
        {
            int by = y * 2 + RAND(2);
            AddObj(gen, ROBJ_BOULDER, x * 2, by, RDIR_SOUTH, have, 0);
            AddObj(gen, ROBJ_BOULDER, x * 2 + 1, by, RDIR_SOUTH, have + 1, 0);
        }
        else
        {
            int bx = x * 2 + RAND(2);
            AddObj(gen, ROBJ_BOULDER, bx, y * 2, RDIR_SOUTH, have, 0);
            AddObj(gen, ROBJ_BOULDER, bx, y * 2 + 1, RDIR_SOUTH, have + 1, 0);
        }
        have += 2;
        placed++;
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

// Whether the player holds Strength when a dungeon first opens: the gyms
// before it on the road, won in order, give its key (as CaveGatesAllowed).
static bool8 StrengthBy(const struct RogueRegion *r, const struct RogueDungeon *d)
{
    u8 won = d->badges >= ROGUE_NUM_GYMS ? 0xFF : (u8)((1 << d->badges) - 1);
    return (RogueGen_KeysWon(r, won) & ROGUE_KEY(ROBST_STRENGTH)) != 0;
}

static bool8 IceFinish(struct Gen *gen, const struct Rect *rooms, int nRooms, int nMain, int best, int sx, int sy,
                       int ex, int ey);

// The last of an ice or volcano floor: the ice cave's sliding puzzle, then
// each kind's boulders. A volcano's doorways must stand in the rock, not
// out on a floor, or the floor is made again.
static bool8 StyledFinish(struct Gen *gen, const struct Rect *rooms, int nRooms, int nMain, int best, int inX, int inY,
                          int farX, int farY)
{
    const struct RogueDungeon *d;
    u8 style = CaveStyle(gen);
    int i;
    if (style == CSTYLE_CAVE)
        return TRUE;
    d = &gen->r->dungeons[sGenDungeon];
    if (style == CSTYLE_FIERY)
    {
        for (i = 0; i < gen->m->nWarps; i++)
            if (Cell(gen, gen->m->warps[i].x >> 1, gen->m->warps[i].y >> 1) != C_TREE)
                return FALSE;
    }
    if (style == CSTYLE_ICE && !IceFinish(gen, rooms, nRooms, nMain, best, inX, inY, farX, farY))
        return FALSE;
    // Fiery Path's boulders, two pairs; Seafoam's, one. Not on a tunnel's
    // exit floor, whose one barrier is the tunnel's (src/rogue_gen_tunnel.h).
    // (Across the way only off the main road: its floors are split.)
    for (i = 0; i < gen->m->nWarps; i++)
        if (gen->m->warps[i].kind == RFLOOR_EXIT)
            return TRUE;
    PlaceBoulders(gen, inX, inY, farX, farY, style == CSTYLE_FIERY ? 2 : 1, StrengthBy(gen->r, d) && !d->required && RAND(2));
    return TRUE;
}
