// The ice cave (RDUN_ICE): floors of ice below a late side cave, as Shoal
// Cave's ice room, Ice Path and Seafoam Islands are. Included by
// rogue_gen_dungeon.h; GenCave draws its floors in the ice room's blue rock
// (CSTYLE_ICE, Shoal Cave's metatiles in gTileset_Cave). Each floor has a
// chamber of ice to slide across (IcePuzzle), with rocks frozen into it to
// stop on, checked to be crossable both ways and never to trap the player
// (IceSafe), and Strength boulders before the side chambers.
//
// A side cave late in the run (four badges or more) becomes one, most of
// all one reached from hills or the coast, or by the mountain.

// The land the region stands on, as the plan laid it (the region map draws
// it the same way).
static void LandOf(const struct RogueRegion *r, struct RogueLand *L)
{
    PlanLand(r->seed, r->land, L);
}

// How far a place's cell lies from the land's mountain (0 on it).
static int FeatureReach(const struct RogueLand *L, const struct RoguePlace *p)
{
    int dx = p->gridX < L->fx ? L->fx - p->gridX : p->gridX >= L->fx + L->fw ? p->gridX - (L->fx + L->fw - 1) : 0;
    int dy = p->gridY < L->fy ? L->fy - p->gridY : p->gridY >= L->fy + L->fh ? p->gridY - (L->fy + L->fh - 1) : 0;
    return dx + dy;
}

// Hills or the coast beside a place (or the mountain): where an ice cave
// belongs, as Shoal Cave lies under the sea's edge and Ice Path in the
// mountains.
static bool8 IceLand(const struct RogueRegion *r, const struct RoguePlace *p)
{
    struct RogueLand L;
    int k;
    if (p->seaSide != RDIR_NONE)
        return TRUE;
    for (k = 0; k < p->nLinks + 2; k++)
    {
        u8 next = k < p->nLinks ? p->links[k].target : k == p->nLinks ? p->caveIn : p->caveOut;
        const struct RoguePlace *q;
        if (next >= r->nPlaces || (p->kind != RPLACE_CAVE && k >= p->nLinks))
            continue;
        q = &r->places[next];
        if (q->kind == RPLACE_ROUTE && (q->type == RROUTE_HILLS || q->type == RROUTE_COAST || q->type == RROUTE_SEA))
            return TRUE;
        if (q->seaSide != RDIR_NONE)
            return TRUE;
    }
    LandOf(r, &L);
    return L.feature == FEATURE_MOUNTAIN && FeatureReach(&L, p) <= 1;
}

// A side cave late in the run, the best of all one reached from the hills
// or the coast; else, now and then, a door in the rock face of a late town
// by the hills or the sea (as Shoal Cave is entered from the beach).
static void PlanIceCave(struct RogueRegion *r)
{
    u32 rng = RogueRng_Hash(r->seed, 11960);
    u64 tried = 0;
    int i, best = -1, bestScore = -1;
    for (i = 0; i < r->nDungeons; i++)
    {
        const struct RogueDungeon *d = &r->dungeons[i];
        const struct RoguePlace *p = &r->places[d->host];
        int score;
        if (d->kind != RDUN_CAVE || d->required || p->kind != RPLACE_CAVE || d->badges < 4)
            continue;
        score = IceLand(r, p) * 8 + (d->badges >= 5) * 2 + RogueRng_Range(&rng, 2);
        if (score > bestScore)
        {
            bestScore = score;
            best = i;
        }
    }
    if (best >= 0 && (bestScore >= 8 || RogueRng_Range(&rng, 2)))
    {
        r->dungeons[best].kind = RDUN_ICE;
        r->dungeons[best].nFloors = 2;
        return;
    }
    if (RogueRng_Range(&rng, 100) >= 70)
        return;
    for (;;)
    {
        best = -1;
        bestScore = 0;
        for (i = 0; i < r->nPlaces; i++)
        {
            const struct RoguePlace *p = &r->places[i];
            int score;
            if (p->kind != RPLACE_TOWN || p->type == RTOWN_SUMMIT || p->badges < 4 || !IceLand(r, p)
             || RogueGen_DungeonAt(r, i) >= 0 || ((tried >> i) & 1) || TOWNS2_TOWN(p))
                continue;
            score = 1 + (p->badges >= 5) * 2 + RogueRng_Range(&rng, 3);
            if (score > bestScore)
            {
                bestScore = score;
                best = i;
            }
        }
        if (best < 0 || !AddDungeon(r, RDUN_ICE, best, 2, RBOSS_GUARDIAN, FALSE))
            return;
        if (TownDungeonEntrance(r, best) == RENTRY_MESA)
            return;
        r->nDungeons--;
        tried |= (u64)1 << best;
    }
}

// ---------------------------------------------------------------------------
// Drawing: the ice room's rock, one ring deep round its snowy floor
// (0x385), as Shoal Cave's ice room has it, and its rock top (0x381).

static u16 IceBlock(struct Gen *gen, int fx, int fy, u8 l, const bool8 *lower)
{
    bool8 n, s, e, w;
    if (l == 0)
        return 0x3385;
    if (l > 1)
        return 0x0781;
    n = lower[1];
    s = lower[7];
    e = lower[5];
    w = lower[3];
    if (n && s) return 0x0789;
    if (e && w) return 0x0780;
    if (n && w) return 0x0778;
    if (n && e) return 0x077A;
    if (s && w) return 0x0788;
    if (s && e) return 0x078A;
    if (s)      return 0x0789;
    if (n)      return 0x0779;
    if (w)      return 0x0780;
    if (e)      return 0x0782;
    if (lower[8]) return 0x077B;
    if (lower[6]) return 0x077C;
    if (lower[2] || lower[0]) return 0x0783;
    return 0x0781;
}

// ---------------------------------------------------------------------------
// The sliding puzzle
//
// A chamber on the way is laid with ice (Emerald's MB_ICE: a step onto it
// slides the player on until something stops them), with rocks frozen
// into it and now and then a patch of snow to stop on. The tunnels into it
// are its mouths: toward the way in, toward the way on, or off the way to a
// side chamber. As Ice Path's and Sootopolis's floors, it is built round
// a way across (IceBuild): from a mouth toward the way in, slides that turn
// at a rock frozen in just beyond each stop, then out to a mouth toward the
// way on, five to seven moves on the first floor and eight to twelve below.
// More rocks are strewn off that way, for wrong turns. A layout is kept
// only if solving it (IceCross, slide by slide) finds every mouth reached,
// nowhere the player can slide to leaves them stuck (every stand leads
// back to the way in), and the crossing as long as wanted; of a few the
// one nearest is kept. Then one of the floor's finds may go out on the
// ice, on a patch of snow off the way, where it stops a slide (IcePrize).
// tools/roguemon/check_seeds.py checks the same of every floor
// (check_ice).
//
// The chamber and a ring of one block round it are worked on as a small
// grid of cells; the generator's per-block scratch (gen->cover, gen->free,
// gen->onWalk) is free by now and holds it.

enum { IC_WALL, IC_ICE, IC_FLOOR, IC_ROCK };
// Which way a mouth leads: off the way, toward the way in, toward the way on.
enum { ICE_SIDE, ICE_FROM, ICE_TO };
#ifndef ICE_TRIES
#define ICE_TRIES 12   // ways across built in a chamber
#define ICE_MENDS 12   // ways round (or stuck places) mended in each
#define ICE_OUTP 3     // a way across goes out when it first can 2 times in 3
#define ICE_DECOY 7    // a rock strewn for every 7 cells, at most
#define ICE_POLISH 200 // rocks frozen in or thawed when the best falls short
#define ICE_GIVE_UP 6  // floor attempts before a floor goes without
#endif
#define ICE_CELLS 168 // (6 units * 2 + 2) * (5 * 2 + 2) at most
#define ICE_MOUTHS 15
#define B_ICE      0x338D
#define B_ICE_SNOW 0x328F // a patch of snow on the ice, to stop on

struct IceGrid
{
    int x0, y0, w, h; // in blocks, the ring included
    u8 *kind, *next, *best, *label, *keep, *mark, *dist, *base;
    u8 nMouths;
    u8 stops;                  // IceCross: stands reached on the ice
    u8 stuck;                  // IceCross: a cell the player would be stuck on, or 0xFF
    bool8 toJoined;            // the mouths toward the way on are joined outside
    u8 side[ICE_MOUTHS + 1];   // by label: ICE_FROM, ICE_TO or ICE_SIDE
};

static u8 IceSlide(const struct IceGrid *g, int c, int d)
{
    int x = c % g->w + sDirDX[d], y = c / g->w + sDirDY[d];
    u8 k;
    if (x < 0 || y < 0 || x >= g->w || y >= g->h)
        return 0xFF;
    k = g->kind[y * g->w + x];
    if (k == IC_WALL || k == IC_ROCK)
        return 0xFF;
    while (g->kind[y * g->w + x] == IC_ICE)
    {
        int tx = x + sDirDX[d], ty = y + sDirDY[d];
        YIELD();
        if (tx < 0 || ty < 0 || tx >= g->w || ty >= g->h)
            break;
        k = g->kind[ty * g->w + tx];
        if (k == IC_WALL || k == IC_ROCK)
            break;
        x = tx;
        y = ty;
    }
    return y * g->w + x;
}

// Where a move from cell c goes (IceSlide), worked out once per layout.
static u8 IceNext(struct IceGrid *g, int c, int d)
{
    u8 *t = &g->next[c * 4 + d];
    if (*t == 0xFE)
        *t = g->kind[c] == IC_ICE || g->kind[c] == IC_FLOOR ? IceSlide(g, c, d) : 0xFF;
    return *t;
}

// A mouth's cell: floor in the ring round the chamber.
static bool8 IceMouth(const struct IceGrid *g, int c)
{
    int x = c % g->w, y = c / g->w;
    return g->kind[c] == IC_FLOOR && (x == 0 || y == 0 || x == g->w - 1 || y == g->h - 1);
}

// The cell d of c in the grid, or -1.
static int IceStep(const struct IceGrid *g, int c, int d)
{
    int x = c % g->w + sDirDX[d], y = c / g->w + sDirDY[d];
    if (x < 0 || y < 0 || x >= g->w || y >= g->h)
        return -1;
    return y * g->w + x;
}

// The mouths, numbered from 1 (label): the ring's floor cells, joined along
// the ring. FALSE if there are too many to tell apart.
static bool8 IceLabels(struct IceGrid *g)
{
    int n = g->w * g->h, c, d, head, tail;
    u16 *queue = sBfsQueue;
    memset(g->label, 0, (u8)n);
    g->nMouths = 0;
    for (c = 0; c < n; c++)
    {
        if (!IceMouth(g, c) || g->label[c])
            continue;
        if (g->nMouths == ICE_MOUTHS)
            return FALSE;
        g->label[c] = ++g->nMouths;
        g->side[g->nMouths] = ICE_SIDE;
        head = tail = 0;
        queue[tail++] = c;
        while (head < tail)
        {
            int cur = queue[head++];
            for (d = 0; d < 4; d++)
            {
                int t = IceStep(g, cur, d);
                if (t < 0 || g->label[t] || !IceMouth(g, t))
                    continue;
                g->label[t] = g->nMouths;
                queue[tail++] = t;
            }
        }
    }
    return g->nMouths >= 2;
}

// Mouths with no ice beside them (cut off by the rocks along an edge, or
// round a corner) are left out of the puzzle: nothing slides there.
static bool8 IceLiveMouths(struct IceGrid *g)
{
    int n = g->w * g->h, c, d;
    u8 live[ICE_MOUTHS + 1];
    if (!IceLabels(g))
        return FALSE;
    memset(live, 0, sizeof(live));
    for (c = 0; c < n; c++)
        for (d = 0; d < 4 && g->label[c]; d++)
            if (IceStep(g, c, d) >= 0 && g->kind[IceStep(g, c, d)] == IC_ICE)
                live[g->label[c]] = 1;
    for (c = 0; c < n; c++)
        if (g->label[c] && !live[g->label[c]])
            g->kind[c] = IC_WALL;
    return IceLabels(g);
}

// Solves a layout: the least moves (each step or slide one) from a mouth
// toward the way in to one toward the way on, or 0 if a mouth to a side
// chamber cannot be reached from the way in, or the player could be left
// stuck: somewhere reached from any mouth that does not lead back to the
// way in (stuck is one such cell). The mouths toward the way in, and
// those toward the way on, are joined outside the chamber. Each move is
// worked out the first time it is needed (most cells are never stood on).
// Leaves dist (moves from the way in; 0xFE where reached only from another
// mouth, 0xFF where never) and stops.
static int IceCross(struct IceGrid *g)
{
    int n = g->w * g->h, c, d, head = 0, tail = 0, best = 0xFF, pass;
    u16 *queue = sBfsQueue;
    u8 *home = g->mark;
    bool8 changed;
    memset(g->next, 0xFE, (u16)(n * 4));
    memset(g->dist, 0xFF, (u8)n);
    g->stuck = 0xFF;
    for (pass = 0; pass < 2; pass++)
    {
        for (c = 0; c < n; c++)
        {
            if (g->label[c] && g->dist[c] == 0xFF && (pass || g->side[g->label[c]] == ICE_FROM))
            {
                g->dist[c] = pass ? 0xFE : 0;
                queue[tail++] = c;
            }
        }
        while (head < tail)
        {
            int cur = queue[head++];
            YIELD();
            for (d = 0; d < 4; d++)
            {
                u8 t = IceNext(g, cur, d), l;
                if (t == 0xFF || g->dist[t] != 0xFF)
                    continue;
                l = g->label[t];
                g->dist[t] = pass ? 0xFE : g->dist[cur] + 1;
                queue[tail++] = t;
                // Out at a mouth, the player may walk to its other cells.
                for (c = 0; c < n && l; c++)
                {
                    if (g->label[c] == l && g->dist[c] == 0xFF)
                    {
                        g->dist[c] = g->dist[t];
                        queue[tail++] = c;
                    }
                }
            }
        }
    }
    g->stops = 0;
    for (c = 0; c < n; c++)
    {
        u8 side = g->side[g->label[c]];
        if (g->label[c] && (side == ICE_SIDE || (side == ICE_TO && !g->toJoined)) && g->dist[c] == 0xFE)
            return 0;
        if (g->label[c] && side == ICE_TO && g->dist[c] < 0xFE && g->dist[c] < best)
            best = g->dist[c];
        if (!g->label[c] && g->dist[c] < 0xFE)
            g->stops++;
        home[c] = g->label[c] && side == ICE_FROM;
    }
    if (best == 0xFF)
        return 0;
    // Back to the way in from everywhere reached.
    do
    {
        changed = FALSE;
        for (c = 0; c < n; c++)
        {
            if (g->dist[c] == 0xFF || home[c])
                continue;
            YIELD();
            for (d = 0; d < 4; d++)
            {
                u8 t = IceNext(g, c, d);
                if (t != 0xFF && home[t])
                {
                    home[c] = 1;
                    changed = TRUE;
                    // (Out at the way on, walk round to its other mouths.)
                    if (g->label[c] && g->side[g->label[c]] == ICE_TO && g->toJoined)
                        for (t = 0; t < n; t++)
                            if (g->label[t] && g->side[g->label[t]] == ICE_TO)
                                home[t] = 1;
                    break;
                }
            }
        }
    } while (changed);
    for (c = 0; c < n; c++)
    {
        if (g->dist[c] != 0xFF && !home[c])
        {
            g->stuck = c;
            return 0;
        }
    }
    return best;
}

static bool8 IceRockOk(const struct IceGrid *g, int c);

// A layout that leaves the player stuck at g->stuck: a rock strewn beside
// it (not one of the way's, nor along an edge) thaws, or failing that one
// freezes there. FALSE if neither can be done.
static bool8 IceUnstick(struct Gen *gen, struct IceGrid *g)
{
    int c = g->stuck, x = c % g->w, y = c / g->w, dx, dy, nCand = 0;
    u8 cand[8];
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
        {
            int t = (y + dy) * g->w + x + dx;
            if (x + dx > 0 && y + dy > 0 && x + dx < g->w - 1 && y + dy < g->h - 1 && g->kind[t] == IC_ROCK
             && !g->keep[t] && g->base[t] == IC_ICE)
                cand[nCand++] = t;
        }
    if (nCand > 0)
    {
        g->kind[cand[RAND(nCand)]] = IC_ICE;
        return TRUE;
    }
    if (c == 0xFF || g->keep[c] || !IceRockOk(g, c))
        return FALSE;
    g->kind[c] = IC_ROCK;
    return TRUE;
}

// A walkable floor block (not rock, water or a warp's frame).
static bool8 IceFloorBlock(struct Gen *gen, int fx, int fy)
{
    u16 b;
    if (!InMap(gen, fx, fy) || Cell(gen, fx >> 1, fy >> 1) != C_ROAD)
        return FALSE;
    b = gen->m->blocks[fy * gen->m->w + fx];
    return ((b >> 10) & 3) == 0;
}

// The way into the chamber from a mouth cell (the ring's side it is on).
static u8 IceInward(const struct IceGrid *g, int c)
{
    int x = c % g->w, y = c / g->w;
    if (y == 0)
        return RDIR_SOUTH;
    if (y == g->h - 1)
        return RDIR_NORTH;
    if (x == 0)
        return RDIR_EAST;
    if (x == g->w - 1)
        return RDIR_WEST;
    return RDIR_NONE;
}

// A rock may go on ice that is not right in a mouth.
static bool8 IceRockOk(const struct IceGrid *g, int c)
{
    int d;
    if (c < 0 || g->kind[c] != IC_ICE)
        return FALSE;
    for (d = 0; d < 4; d++)
    {
        int t = IceStep(g, c, d);
        if (t >= 0 && IceMouth(g, t))
            return FALSE;
    }
    return TRUE;
}

// The chamber all ice, its ring floor (the mouths) or rock.
static void IceFresh(struct Gen *gen, struct IceGrid *g)
{
    int c;
    for (c = 0; c < g->w * g->h; c++)
    {
        int x = g->x0 + c % g->w, y = g->y0 + c / g->w;
        bool8 ring = c % g->w == 0 || c / g->w == 0 || c % g->w == g->w - 1 || c / g->w == g->h - 1;
        if (!IceFloorBlock(gen, x, y))
            g->kind[c] = IC_WALL;
        else
            g->kind[c] = ring ? IC_FLOOR : IC_ICE;
    }
}

// A tunnel that opens into the chamber along a whole side would let the
// player in anywhere: rocks frozen in along the edge in front of it leave a
// gap of two blocks, as the edges of Ice Path's ice are lined.
// (The ring's floor behind the rocks is left out of the puzzle: nothing
// slides there.)
static void IceGates(struct Gen *gen, struct IceGrid *g)
{
    int n = g->w * g->h, c, k;
    for (c = 0; c < n; c++)
    {
        int open = 0, keep, t, run;
        u8 d = IceInward(g, c);
        if (!IceMouth(g, c) || d == RDIR_NONE || g->kind[IceStep(g, c, d)] != IC_ICE)
            continue;
        // A run of open mouth cells along this side, from its first.
        k = (d == RDIR_NORTH || d == RDIR_SOUTH) ? RDIR_EAST : RDIR_SOUTH;
        t = IceStep(g, c, k ^ 2);
        if (t >= 0 && IceMouth(g, t) && IceInward(g, t) == d && g->kind[IceStep(g, t, d)] == IC_ICE)
            continue;
        for (t = c; t >= 0 && IceMouth(g, t) && IceInward(g, t) == d && g->kind[IceStep(g, t, d)] == IC_ICE;
             t = IceStep(g, t, k))
            open++;
        if (open <= 2)
            continue;
        keep = RAND(open - 1);
        for (t = c, run = 0; run < open; run++, t = IceStep(g, t, k))
        {
            YIELD();
            if (run == keep || run == keep + 1)
                continue;
            g->kind[IceStep(g, t, d)] = IC_ROCK;
            g->kind[t] = IC_WALL;
        }
    }
}

// The direction a slide from stop c (having come d, so not on that way)
// leaves the ice by, straight out to a mouth toward the way on, or
// RDIR_NONE.
static u8 IceExit(const struct IceGrid *g, int c, int d)
{
    int k, t;
    for (k = 1; k <= 3; k++)
    {
        u8 dd = (d + k) & 3;
        for (t = IceStep(g, c, dd); t >= 0 && g->kind[t] == IC_ICE; t = IceStep(g, t, dd))
            ;
        if (t >= 0 && g->label[t] && g->side[g->label[t]] == ICE_TO)
            return dd;
    }
    return RDIR_NONE;
}

// Lays a way across of lo to hi moves (keep marks its cells): in from a
// mouth toward the way in, a turn at each stop (a rock frozen in just
// beyond it, or the chamber's wall), and the last move out to a mouth
// toward the way on; then rocks strewn off it and now and then a patch of
// snow. FALSE if the way runs into a dead end.
static bool8 IceBuild(struct Gen *gen, struct IceGrid *g, int lo, int hi)
{
    int n = g->w * g->h, c, p = -1, s, k, t, nCand, nOut, seen = 0;
    u8 d, dd;
    u16 cand[24];
    memset(g->keep, 0, (u8)n);
    for (c = 0; c < n; c++)
    {
        t = IceStep(g, c, IceInward(g, c) & 3);
        if (g->label[c] && g->side[g->label[c]] == ICE_FROM && IceInward(g, c) != RDIR_NONE
         && t >= 0 && g->kind[t] == IC_ICE && RAND(++seen) == 0)
            p = c;
    }
    if (p < 0)
        return FALSE;
    d = IceInward(g, p);
    for (s = 1; ; s++)
    {
        // The stops the next slide could make (bit 15: the wall stops it,
        // else a rock goes in beyond), those the way out is open from
        // first (bit 14); the first move goes straight in.
        nCand = nOut = 0;
        for (k = 0; k < (s == 1 ? 1 : 2); k++)
        {
            int len = 0;
            dd = s == 1 ? d : (d + 1 + 2 * k) & 3;
            for (t = IceStep(g, p, dd); t >= 0 && g->kind[t] == IC_ICE && nCand < 24; t = IceStep(g, t, dd))
            {
                int u = IceStep(g, t, dd);
                bool8 wall = u < 0 || g->kind[u] == IC_WALL || g->kind[u] == IC_ROCK;
                len++;
                YIELD();
                if (wall || (len >= 2 && !g->keep[u] && IceRockOk(g, u)))
                {
                    u16 e = (wall ? 0x8000 : 0) | (dd << 12) | t;
                    if (s + 1 >= lo && IceExit(g, t, dd) != RDIR_NONE)
                    {
                        cand[nCand++] = cand[nOut];
                        cand[nOut++] = e | 0x4000;
                    }
                    else
                    {
                        cand[nCand++] = e;
                    }
                }
                if (wall || g->kind[u] != IC_ICE)
                    break;
            }
        }
        // Out when it can be (now and then one more turn first, short of hi).
        if (nOut > 0 && (s + 2 > hi || RAND(ICE_OUTP) != 0))
            nCand = nOut;
        else if (s + 2 > hi)
            return FALSE;
        if (nCand == 0)
            return FALSE;
        c = cand[RAND(nCand)];
        dd = (c >> 12) & 3;
        if (!(c & 0x8000))
        {
            g->kind[IceStep(g, c & 0xFFF, dd)] = IC_ROCK;
            g->keep[IceStep(g, c & 0xFFF, dd)] = 2;
        }
        for (t = p; t != (c & 0xFFF); )
        {
            t = IceStep(g, t, dd);
            g->keep[t] = 1;
        }
        p = t;
        d = dd;
        if (c & 0x4000)
            break;
    }
    dd = IceExit(g, p, d);
    for (t = IceStep(g, p, dd); g->kind[t] == IC_ICE; t = IceStep(g, t, dd))
        g->keep[t] = 1;
    // Wrong turns: rocks off the way, and now and then a patch of snow.
    for (k = 0; k < n / ICE_DECOY; k++)
    {
        c = RAND(n);
        if (!g->keep[c] && IceRockOk(g, c))
            g->kind[c] = IC_ROCK;
    }
    if (RAND(3) == 0)
    {
        c = RAND(n);
        if (!g->keep[c] && IceRockOk(g, c))
            g->kind[c] = IC_FLOOR;
    }
    return TRUE;
}

// A crossing shorter than wanted: a rock frozen into one of the slides of
// a shortest crossing (IceCross's dist), on ice off the way laid, so that
// way round is shut. Returns the cell, or -1 if there is none to take.
static int IceBlockShort(struct Gen *gen, struct IceGrid *g)
{
    int n = g->w * g->h, c, cur = -1, k, nCand = 0;
    u8 cand[32];
    for (c = 0; c < n; c++)
        if (g->label[c] && g->side[g->label[c]] == ICE_TO && (cur < 0 || g->dist[c] < g->dist[cur]))
            cur = c;
    while (cur >= 0 && g->dist[cur] > 0 && g->dist[cur] != 0xFF)
    {
        int from = -1;
        u8 dd = 0;
        YIELD();
        for (c = 0; c < n && from < 0; c++)
        {
            if (g->dist[c] != g->dist[cur] - 1)
                continue;
            for (k = 0; k < 4; k++)
            {
                if (IceNext(g, c, k) == cur)
                {
                    from = c;
                    dd = k;
                    break;
                }
            }
        }
        if (from < 0)
            break;
        for (c = IceStep(g, from, dd); c >= 0 && nCand < 32; c = IceStep(g, c, dd))
        {
            if (!g->keep[c] && IceRockOk(g, c))
                cand[nCand++] = c;
            if (c == cur)
                break;
        }
        cur = from;
    }
    if (nCand == 0)
        return -1;
    c = cand[RAND(nCand)];
    g->kind[c] = IC_ROCK;
    return c;
}

// A layout that always holds, for a chamber the ways across fail in: a
// patch of snow on the ice in front of each mouth. Sliding along the walls
// goes round the chamber from corner to corner, stopping at each patch, so
// every mouth is reached from every other, as along the walls of Shoal
// Cave's ice room. Then rocks frozen into the middle, each kept only if the
// layout still holds.
static int IceStones(struct Gen *gen, struct IceGrid *g)
{
    int n = g->w * g->h, c, k, score;
    memcpy(g->kind, g->base, (u8)n);
    for (c = 0; c < n; c++)
    {
        int d = IceInward(g, c), t;
        if (!g->label[c] || d == RDIR_NONE)
            continue;
        t = IceStep(g, c, d);
        if (t >= 0 && g->kind[t] == IC_ICE)
            g->kind[t] = IC_FLOOR;
    }
    score = IceCross(g);
    for (k = 0; k < 8 && score > 0; k++)
    {
        int x = 2 + RAND(g->w - 4), y = 2 + RAND(g->h - 4), s2;
        c = y * g->w + x;
        if (g->w < 5 || g->h < 5 || !IceRockOk(g, c))
            continue;
        g->kind[c] = IC_ROCK;
        s2 = IceCross(g);
        if (s2 == 0)
            g->kind[c] = IC_ICE;
        else
            score = s2;
    }
    return IceCross(g);
}

// Which way each mouth leads (side): toward the way in (reached from
// (sx, sy), in blocks, without crossing the chamber), toward the way on
// ((ex, ey)), or off the way. Where the way also passes the chamber by,
// the mouths not toward the way in lead on; failing those, the first mouth
// is the way in. FALSE if it has no two to cross between.
static bool8 IceSides(struct Gen *gen, struct IceGrid *g, const struct Rect *r, int sx, int sy, int ex, int ey)
{
    int n = g->w * g->h, c, x, y, pass, nFrom = 0, nTo = 0;
    u8 bits[ICE_MOUTHS + 1], saved[64];
    memset(bits, 0, sizeof(bits));
    for (y = 0; y < r->h; y++)
        for (x = 0; x < r->w; x++)
        {
            saved[y * r->w + x] = Cell(gen, r->x + x, r->y + y);
            SetCell(gen, r->x + x, r->y + y, C_TREE);
        }
    for (pass = 0; pass < 2; pass++)
    {
        FloodReachable(gen, (pass ? ex : sx) >> 1, (pass ? ey : sy) >> 1);
        for (c = 0; c < n; c++)
            if (g->label[c] && Reached(gen, (g->x0 + c % g->w) >> 1, (g->y0 + c / g->w) >> 1))
                bits[g->label[c]] |= 1 << pass;
    }
    for (y = 0; y < r->h; y++)
        for (x = 0; x < r->w; x++)
            SetCell(gen, r->x + x, r->y + y, saved[y * r->w + x]);
    for (c = 1; c <= g->nMouths; c++)
    {
        g->side[c] = bits[c] & 1 ? ICE_FROM : bits[c] & 2 ? ICE_TO : ICE_SIDE;
        nFrom += g->side[c] == ICE_FROM;
        nTo += g->side[c] == ICE_TO;
    }
    g->toJoined = nTo > 0;
    if (nFrom == 0)
        g->side[1] = ICE_FROM;
    for (c = 1; c <= g->nMouths && nTo == 0; c++)
        if (g->side[c] == ICE_SIDE || (nFrom == 0 && c > 1))
            g->side[c] = ICE_TO;
    for (c = 1; c <= g->nMouths && nTo == 0; c++)
        if (g->side[c] == ICE_TO)
            nTo++;
    if (nTo == 0 && g->nMouths >= 2)
    {
        // Every mouth leads back toward the way in: cross from the first.
        for (c = 2; c <= g->nMouths; c++)
            g->side[c] = ICE_TO;
        g->side[1] = ICE_FROM;
    }
    return g->nMouths >= 2;
}

// The least moves the crossing should take, more on the floors below, as
// Ice Path's puzzles grow on the way down.
static void IceWant(int *lo, int *hi)
{
    *lo = sGenFloor <= 1 ? 5 : 8;
    *hi = sGenFloor <= 1 ? 7 : 12;
}

// How near a crossing of `moves` comes to what is wanted (higher is nearer).
static int IceFit(int moves, int stops, int lo, int hi)
{
    if (moves == 0)
        return 0;
    if (moves < lo)
        return moves * 16;
    if (moves > hi)
        return (2 * hi - moves) * 16;
    return 1024 + stops;
}

// One of the floor's finds out on the ice, on a patch of snow off the way,
// as Ice Path's and Seafoam's item balls wait on the ice: the ball stops a
// slide as a rock does, and once it is taken the patch does. It is kept
// only where the layout holds both with the ball and without it, the
// crossing stays as long as wanted, and reaching the ball takes nearly as
// many moves as crossing (lo - 1 at least): a second puzzle. A floor with
// one find keeps it in its chamber. FALSE if no place does.
static bool8 IcePrize(struct Gen *gen, struct IceGrid *g, int lo)
{
    struct RogueMap *m = gen->m;
    int n = g->w * g->h, c, i, k, tries, item = -1, nItems = 0;
    for (i = 0; i < m->nObjs; i++)
    {
        if (m->objs[i].kind == ROBJ_ITEM)
        {
            nItems++;
            item = i;
        }
    }
    if (nItems < 2)
        return FALSE;
    memcpy(g->kind, g->best, (u8)n);
    for (tries = 0; tries < 8; tries++)
    {
        int reach = 0xFF, moves, d;
        // Beside a stand the crossing reaches, deep in.
        if (IceCross(g) == 0)
            return FALSE;
        c = RAND(n);
        if (!IceRockOk(g, c))
            continue;
        for (d = 0; d < 4; d++)
        {
            k = IceStep(g, c, d);
            if (k >= 0 && !g->label[k] && g->dist[k] < 0xFE && g->dist[k] < reach)
                reach = g->dist[k];
        }
        if (reach == 0xFF || reach + 1 < lo || reach < 3)
            continue;
        // With the ball there, then with only its patch.
        g->kind[c] = IC_ROCK;
        moves = IceCross(g);
        reach = 0xFF;
        for (d = 0; d < 4 && moves; d++)
        {
            k = IceStep(g, c, d);
            if (k >= 0 && !g->label[k] && g->dist[k] < 0xFE && g->dist[k] < reach)
                reach = g->dist[k];
        }
        if (moves >= lo && reach + 1 >= lo && reach >= 3)
        {
            g->kind[c] = IC_FLOOR;
            if (IceCross(g) >= lo)
            {
                m->objs[item].x = g->x0 + c % g->w;
                m->objs[item].y = g->y0 + c / g->w;
                memcpy(g->best, g->kind, (u8)n);
                return TRUE;
            }
        }
        g->kind[c] = IC_ICE;
    }
    return FALSE;
}

static bool8 IcePuzzle(struct Gen *gen, const struct Rect *r, int sx, int sy, int ex, int ey)
{
    struct IceGrid grid, *g = &grid;
    int tries, c, bestFit = 0, bestMoves = 0, x, y, lo, hi;
    g->x0 = r->x * 2 - 1;
    g->y0 = r->y * 2 - 1;
    g->w = r->w * 2 + 2;
    g->h = r->h * 2 + 2;
    if (g->w * g->h > ICE_CELLS || r->w * r->h > 64)
        return FALSE;
    g->kind = gen->cover;
    g->next = gen->cover + ICE_CELLS;
    g->best = gen->cover + ICE_CELLS * 5;
    g->label = gen->cover + ICE_CELLS * 6;
    g->keep = gen->cover + ICE_CELLS * 7;
    g->base = gen->cover + ICE_CELLS * 8;
    g->mark = gen->free;
    g->dist = gen->onWalk;
    IceFresh(gen, g);
    IceGates(gen, g);
    if (!IceLiveMouths(g) || !IceSides(gen, g, r, sx, sy, ex, ey))
        return FALSE;
    memcpy(g->base, g->kind, (u8)(g->w * g->h));
    IceWant(&lo, &hi);
    for (tries = 0; tries < ICE_TRIES && bestFit < 1024 + 8; tries++)
    {
        int moves, fit, mend, rock;
        memcpy(g->kind, g->base, (u8)(g->w * g->h));
        if (!IceBuild(gen, g, lo, hi))
            continue;
        moves = IceCross(g);
        for (mend = 0; ; mend++)
        {
            fit = IceFit(moves, g->stops, lo, hi);
            if (fit > bestFit)
            {
                bestFit = fit;
                bestMoves = moves;
                memcpy(g->best, g->kind, (u8)(g->w * g->h));
            }
            if (moves >= lo || mend == ICE_MENDS)
                break;
            if (moves == 0)
            {
                // Stuck somewhere: open it up.
                if (g->stuck == 0xFF || !IceUnstick(gen, g))
                    break;
                moves = IceCross(g);
                continue;
            }
            // Too short: shut a way round, unless that spoils it.
            if ((rock = IceBlockShort(gen, g)) < 0)
                break;
            fit = IceCross(g);
            if (fit == 0)
            {
                g->kind[rock] = IC_ICE;
                fit = IceCross(g);
            }
            moves = fit;
        }
    }
    // Short of what is wanted: rocks frozen in or thawed one at a time, a
    // rock kept if the crossing holds and comes nearer to what is wanted, a
    // thaw if it comes no further from it.
    if (bestFit > 0 && bestFit < 1024)
    {
        int step, cur = bestFit, n = g->w * g->h;
        memcpy(g->kind, g->best, (u8)n);
        for (step = 0; step < ICE_POLISH && bestFit < 1024; step++)
        {
            int moves, fit;
            u8 old;
            c = RAND(n);
            old = g->kind[c];
            if (g->base[c] != IC_ICE || (old == IC_ICE && !IceRockOk(g, c)) || (old != IC_ICE && old != IC_ROCK))
                continue;
            g->kind[c] = old == IC_ROCK ? IC_ICE : IC_ROCK;
            moves = IceCross(g);
            fit = IceFit(moves, g->stops, lo, hi);
            // (A rock that only matches what was there before is not kept:
            // the ice would fill with them.)
            if (fit < cur || (fit == cur && old == IC_ICE))
            {
                g->kind[c] = old;
                continue;
            }
            cur = fit;
            if (fit > bestFit)
            {
                bestFit = fit;
                bestMoves = moves;
                memcpy(g->best, g->kind, (u8)n);
            }
        }
    }
    // Failing a way across, the patches of snow, which hold.
    if (bestMoves < 3)
    {
        int moves = IceStones(gen, g);
        if (moves >= 2 && moves > bestMoves)
        {
            bestMoves = moves;
            memcpy(g->best, g->kind, (u8)(g->w * g->h));
        }
    }
    if (bestMoves < 2)
        return FALSE;
    if (bestFit >= 1024)
        IcePrize(gen, g, lo);
    for (c = 0; c < g->w * g->h; c++)
    {
        u8 k = g->best[c];
        x = g->x0 + c % g->w;
        y = g->y0 + c / g->w;
        if (k == IC_WALL)
            continue;
        MarkOccupied(gen, x, y);
        if (k == IC_ICE)
            Put(gen, x, y, B_ICE);
        else if (k == IC_ROCK)
            Put(gen, x, y, ((x + y) & 1) ? 0x077D : 0x077E);
        else if (c / g->w == 0)
            Put(gen, x, y, g->best[c + g->w] == IC_ICE ? 0x3387 : 0x3385);
        else if (c / g->w == g->h - 1)
            Put(gen, x, y, g->best[c - g->w] == IC_ICE ? 0x338F : 0x3385);
        else if (c % g->w == 0)
            Put(gen, x, y, g->best[c + 1] == IC_ICE ? 0x338C : 0x3385);
        else if (c % g->w == g->w - 1)
            Put(gen, x, y, g->best[c - 1] == IC_ICE ? 0x338E : 0x3385);
        else
            Put(gen, x, y, B_ICE_SNOW);
    }
    return TRUE;
}


// TRUE if a trainer standing at (x, y) facing as o would sees into the box
// (blocks x0..x1, y0..y1): walking up to a player on the ice, they would be
// left standing on it.
static bool8 IceSeen(struct Gen *gen, const struct RogueObj *o, int x, int y, int x0, int y0, int x1, int y1)
{
    u8 dirs = 1 << (o->facing & 3), d;
    int k;
    if (o->facing & ROBJ_TURNS)
        dirs |= 1 << ((o->facing >> 2) & 3);
    if (o->facing & ROBJ_LOOKS)
        dirs = 0x0F;
    for (d = 0; d < 4; d++)
    {
        if (!((dirs >> d) & 1))
            continue;
        for (k = 0; k <= o->range; k++)
        {
            int bx = x + sDirDX[d] * k, by = y + sDirDY[d] * k;
            if (k > 0 && !IceFloorBlock(gen, bx, by))
                break;
            if (bx >= x0 && bx <= x1 && by >= y0 && by <= y1)
                return TRUE;
        }
    }
    return FALSE;
}

// Readies chamber r for the ice: nothing but trainers near it (they are
// moved to other chambers, out of sight of it). FALSE if it cannot be.
static bool8 IceClearRoom(struct Gen *gen, const struct Rect *rooms, int nMain, const struct Rect *r)
{
    struct RogueMap *m = gen->m;
    int x0 = r->x * 2 - 1, y0 = r->y * 2 - 1, x1 = (r->x + r->w) * 2, y1 = (r->y + r->h) * 2, i;
    // Nothing in the chamber or its mouths (outside, nothing stops a slide).
    for (i = 0; i < m->nWarps; i++)
        if (m->warps[i].x >= x0 && m->warps[i].x <= x1 && m->warps[i].y >= y0 && m->warps[i].y <= y1)
            return FALSE;
    for (i = 0; i < m->nObjs; i++)
    {
        struct RogueObj *o = &m->objs[i];
        if (o->kind != ROBJ_TRAINER && o->x >= x0 && o->x <= x1 && o->y >= y0 && o->y <= y1)
            return FALSE;
    }
    for (i = 0; i < m->nObjs; i++)
    {
        struct RogueObj *o = &m->objs[i];
        int tries, b;
        if (o->kind != ROBJ_TRAINER || !IceSeen(gen, o, o->x, o->y, x0, y0, x1, y1))
            continue;
        // Somewhere else, looking away from the ice.
        b = o->y * m->w + o->x;
        gen->occupied[b >> 3] &= ~(1 << (b & 7));
        for (tries = 0; tries < 40; tries++)
        {
            const struct Rect *to = &rooms[RAND(nMain)];
            int fx, fy;
            u8 f;
            if (to == r || !FindCaveSpot(gen, to, &fx, &fy) || (fx >= x0 - 2 && fx <= x1 + 2 && fy >= y0 - 2 && fy <= y1 + 2))
                continue;
            o->facing = RAND(4);
            for (f = 0; f < 4 && IceSeen(gen, o, fx, fy, x0, y0, x1, y1); f++)
                o->facing = (o->facing + 1) & 3;
            if (f == 4)
                continue;
            o->x = fx;
            o->y = fy;
            break;
        }
        MarkOccupied(gen, o->x, o->y);
        if (tries == 40)
            return FALSE;
    }
    return TRUE;
}

// Fills chamber r with rock (saved keeps what was there), or puts it back.
static void IceBlockRoom(struct Gen *gen, const struct Rect *r, u8 *saved, bool8 back)
{
    int x, y;
    for (y = 0; y < r->h; y++)
    {
        for (x = 0; x < r->w; x++)
        {
            if (back)
                SetCell(gen, r->x + x, r->y + y, saved[y * r->w + x]);
            else
            {
                saved[y * r->w + x] = Cell(gen, r->x + x, r->y + y);
                SetCell(gen, r->x + x, r->y + y, C_TREE);
            }
        }
    }
}

static bool8 IceReachedRoom(struct Gen *gen, const struct Rect *r)
{
    int x, y;
    for (y = r->y; y < r->y + r->h; y++)
        for (x = r->x; x < r->x + r->w; x++)
            if (Reached(gen, x, y))
                return TRUE;
    return FALSE;
}

// TRUE if the way from (sx, sy) (in blocks) to the far chamber must cross
// chamber r: the puzzle there cannot be walked round.
static bool8 IceOnWay(struct Gen *gen, const struct Rect *r, const struct Rect *far, int sx, int sy)
{
    u8 saved[64];
    bool8 cut;
    if (r->w * r->h > 64)
        return FALSE;
    IceBlockRoom(gen, r, saved, FALSE);
    FloodReachable(gen, sx >> 1, sy >> 1);
    cut = !IceReachedRoom(gen, far);
    IceBlockRoom(gen, r, saved, TRUE);
    return cut;
}

// Where the way can go round chamber k, the tunnel it goes round by is
// filled in at one unit (a dead end each side, as caves have), if that
// leaves every other unit of floor still reached: the ice is then on the
// way, as Ice Path's must be crossed. Before the chambers are drawn
// (IceReserve). FALSE if no one unit does it.
static bool8 IceShutWayRound(struct Gen *gen, const struct Rect *rooms, int nRooms, int nMain, int k, int sx, int sy)
{
    const struct Rect *r = &rooms[k], *far = &rooms[nMain - 1];
    u8 saved[64], cand[12];
    int head = 0, tail = 0, nCand = 0, all, i, j, cur = -1, n = gen->cw * gen->ch;
    if (r->w * r->h > 64)
        return FALSE;
    for (i = 0, all = 0; i < n; i++)
        all += gen->g[i] == C_ROAD;
    // The way round: a search from the way in with the chamber shut, each
    // unit noting the one it was reached from.
    IceBlockRoom(gen, r, saved, FALSE);
    memset(sBfsPrev, 0, n * sizeof(sBfsPrev[0]));
    sBfsQueue[tail++] = PACK(sx >> 1, sy >> 1);
    sBfsPrev[(sy >> 1) * gen->cw + (sx >> 1)] = -1;
    while (head < tail && cur < 0)
    {
        int c = sBfsQueue[head++], d;
        YIELD();
        for (d = 0; d < 4; d++)
        {
            int nx = PX(c) + sDirDX[d], ny = PY(c) + sDirDY[d];
            if (!InBounds(gen, nx, ny) || sBfsPrev[ny * gen->cw + nx] || gen->g[ny * gen->cw + nx] != C_ROAD)
                continue;
            sBfsPrev[ny * gen->cw + nx] = PY(c) * gen->cw + PX(c) + 1;
            sBfsQueue[tail++] = PACK(nx, ny);
            if (nx >= far->x && nx < far->x + far->w && ny >= far->y && ny < far->y + far->h)
                cur = ny * gen->cw + nx;
        }
    }
    // Its units in tunnels (in no chamber), far from either end.
    while (cur >= 0 && sBfsPrev[cur] > 0 && nCand < 12)
    {
        int x = cur % gen->cw, y = cur / gen->cw;
        bool8 inRoom = FALSE;
        for (j = 0; j < nRooms && !inRoom; j++)
            inRoom = x >= rooms[j].x - 1 && x <= rooms[j].x + rooms[j].w && y >= rooms[j].y - 1 && y <= rooms[j].y + rooms[j].h;
        for (j = 0; j < gen->m->nWarps && !inRoom; j++)
            inRoom = Abs((gen->m->warps[j].x >> 1) - x) + Abs((gen->m->warps[j].y >> 1) - y) <= 2;
        if (!inRoom)
            cand[nCand++] = cur;
        cur = sBfsPrev[cur] - 1;
    }
    for (i = 0; i < nCand; i++)
    {
        int x = cand[i] % gen->cw, y = cand[i] / gen->cw;
        bool8 ok;
        SetCell(gen, x, y, C_TREE);
        FloodReachable(gen, sx >> 1, sy >> 1);
        ok = !IceReachedRoom(gen, far);
        IceBlockRoom(gen, r, saved, TRUE);
        if (ok && FloodReachable(gen, sx >> 1, sy >> 1) == all - 1)
            return TRUE;
        IceBlockRoom(gen, r, saved, FALSE);
        SetCell(gen, x, y, C_ROAD);
    }
    IceBlockRoom(gen, r, saved, TRUE);
    return FALSE;
}

// The chamber an ice floor's puzzle takes: the biggest middle chamber the
// way must cross, else the biggest, but not one a ladder, the guardian
// aside or the finds would need (the feature chamber and the first, when
// there are no side chambers), or -1.
static int IceRoom(struct Gen *gen, const struct Rect *rooms, int nRooms, int nMain, int best, int sx, int sy)
{
    int i, pick = -1, pickScore = 0;
    for (i = 1; i + 1 < nMain; i++)
    {
        int score;
        // (With no side chambers, the finds go in the first middle one.)
        if (rooms[i].w < 3 || rooms[i].h < 3 || (nRooms == nMain && (i == best || i == 1)))
            continue;
        score = rooms[i].w * rooms[i].h + 64 * IceOnWay(gen, &rooms[i], &rooms[nMain - 1], sx, sy);
        if (score > pickScore)
        {
            pickScore = score;
            pick = i;
        }
    }
    return pick;
}

// Keeps everything placed after the chambers (ladders aside, the guardian,
// extras, trainers, items) off the puzzle's chamber and two blocks round
// it, without counting it in IsCrowded's list.
static void IceReserve(struct Gen *gen, const struct Rect *rooms, int nRooms, int nMain, int best, int sx, int sy)
{
    int k = IceRoom(gen, rooms, nRooms, nMain, best, sx, sy), x, y;
    if (k < 0)
        return;
    if (!IceOnWay(gen, &rooms[k], &rooms[nMain - 1], sx, sy))
        IceShutWayRound(gen, rooms, nRooms, nMain, k, sx, sy);
    for (y = rooms[k].y * 2 - 1; y <= (rooms[k].y + rooms[k].h) * 2; y++)
    {
        for (x = rooms[k].x * 2 - 1; x <= (rooms[k].x + rooms[k].w) * 2; x++)
        {
            int i = y * gen->m->w + x;
            if (InMap(gen, x, y))
                gen->occupied[i >> 3] |= 1 << (i & 7);
        }
    }
}

// The ice puzzle of an ice cave floor, in the chamber kept for it, crossed
// from the way in (sx, sy) toward the far end (ex, ey). A floor that keeps
// failing goes without (after six tries, so a floor made while the screen
// is dark is soon done).
static bool8 IceFinish(struct Gen *gen, const struct Rect *rooms, int nRooms, int nMain, int best, int sx, int sy,
                       int ex, int ey)
{
    int k = IceRoom(gen, rooms, nRooms, nMain, best, sx, sy), i;
    if (k >= 0 && IceClearRoom(gen, rooms, nMain, &rooms[k]) && IcePuzzle(gen, &rooms[k], sx, sy, ex, ey))
        return TRUE;
    // Another middle chamber that happens to be clear, before making the
    // floor again.
    for (i = 1; i + 1 < nMain; i++)
        if (i != k && rooms[i].w >= 3 && rooms[i].h >= 3 && IceClearRoom(gen, rooms, nMain, &rooms[i])
         && IcePuzzle(gen, &rooms[i], sx, sy, ex, ey))
            return TRUE;
#ifdef ROGUE_DEBUG
    fprintf(stderr, "ice: no puzzle (chamber %d)\n", k);
#endif
    return gen->attempt >= ICE_GIVE_UP;
}
