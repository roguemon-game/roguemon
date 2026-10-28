// v1.1 towns2: two kinds of town each region may have, placed on their own
// so nothing else in the plan moves (included by rogue_gen.c, so gen_host
// sees the same):
//
// - A raft village out on the sea, as Pacifidlog: houses on log rafts and
//   log walkways on open water between the two sea routes of a stretch
//   (PlanSpine's SP_RAFT, about a third of regions). It is reached by Surf
//   from either sea route, has a POKeMON CENTER and homes and no Mart, as
//   Pacifidlog has none. Drawn with Pacifidlog's tileset (RTHEME_RAFT),
//   from Pacifidlog's own pieces (GenRaftTown).
// - A crater town, as Sootopolis: a town in a ring of white cliffs round a
//   lake, entered through a gap in the ring (RTHEME_CRATER, GenCraterTown).
//
// Neither theme is ever picked by AssignThemes (ThemeWeight); the plan sets
// them.

// Why a try of a town failed (gen_host with T2DBG set prints it).
#ifdef ROGUE_HOST
#define T2FAIL(why) do { if (getenv("T2DBG")) fprintf(stderr, "towns2 place %d try %d: %s\n", gen->m->place, gen->attempt, why); return FALSE; } while (0)
#else
#define T2FAIL(why) return FALSE
#endif

#define TOWNS2_SALT_RAFT   0x7A1F7
#define TOWNS2_SALT_CRATER 0x7A1F8

// About two regions in five have a raft village out on their stretch of sea
// (PlanSpine). From the seed alone, so a region without one is planned as
// before.
static bool8 Towns2RaftWanted(u32 seed)
{
    // (RUN 0's region is planned as it always was.) Every other region
    // with one stretch of sea tries for one (BuildRegion: on the first
    // TOWNS2_RAFT_LANDS lands); about half of them lay, near 30% of all regions.
    return seed != ROGUE_RUN0_SEED;
}

bool8 RogueGen_IsRaftTown(const struct RoguePlace *p)
{
    return p->kind == RPLACE_TOWN && p->theme == RTHEME_RAFT;
}

bool8 RogueGen_IsCraterTown(const struct RoguePlace *p)
{
    return p->kind == RPLACE_TOWN && p->theme == RTHEME_CRATER;
}

// A route next to a crater town takes Sootopolis's tileset, so the white
// cliffs show right across the seam: one drawn only from General's pieces
// (green country, hills, a canyon, a beach; not a river's or a lake's
// bridges, flowers or long grass), not in another town's ash or springs.
static bool8 CraterRouteOk(const struct RoguePlace *q)
{
    if (q->kind != RPLACE_ROUTE || q->theme == RTHEME_ASH || q->theme == RTHEME_SPRING || q->mouth != 0xFF)
        return FALSE;
    switch (q->type)
    {
    case RROUTE_FIELD:
    case RROUTE_FOREST:
    case RROUTE_WOODS:
    case RROUTE_HILLS:
    case RROUTE_DESERT:
    case RROUTE_COAST:
        return TRUE;
    }
    return FALSE;
}

// A town that can stand in a crater: a gym city or a village of the main
// road or a side path (not the first town nor the League's), not on the sea
// nor at a cave's end, every way out a plain green route.
static bool8 CraterTownOk(const struct RogueRegion *r, u8 place)
{
    const struct RoguePlace *p = &r->places[place];
    int k;
    if (p->kind != RPLACE_TOWN || (p->type != RTOWN_GYM && p->type != RTOWN_VILLAGE) || p->theme == RTHEME_RAFT
     || place == r->start || p->mouth != 0xFF || (p->layout == RLAYOUT_HARBOR && p->seaSide != RDIR_NONE) || p->nLinks == 0)
        return FALSE;
    for (k = 0; k < p->nLinks; k++)
        if (!CraterRouteOk(&r->places[p->links[k].target]))
            return FALSE;
    return TRUE;
}

// v1.1: crater towns are off (2026-10-14: the rings read as boxes and the
// white rim meets routes in a hard edge). The code stays for later.
#define CRATER_SHARE 0

// Once the towns' layouts are chosen (AssignTownLayouts, whose draws stay
// as they were): a raft village is no harbor, its sea is all round it; and
// a crater town is picked, from the seed alone, so nothing else moves.
static void Towns2Plan(struct RogueRegion *r)
{
    u32 h = RogueRng_Hash(r->seed, TOWNS2_SALT_CRATER);
    int i, n = 0, pick = -1;
    for (i = 0; i < r->nTowns; i++)
    {
        struct RoguePlace *p = &r->places[r->towns[i]];
        if (p->theme != RTHEME_RAFT)
            continue;
        p->layout = RLAYOUT_STREETS;
        p->seaSide = RDIR_NONE;
    }
    if (h % 100 >= CRATER_SHARE || RogueGen_IsRun0(r))
        return;
    for (i = 0; i < r->nTowns; i++)
        if (CraterTownOk(r, r->towns[i]))
            n++;
    if (n == 0)
        return;
    n = (h >> 8) % n;
    for (i = 0; i < r->nTowns && pick < 0; i++)
        if (CraterTownOk(r, r->towns[i]) && n-- == 0)
            pick = r->towns[i];
    {
        struct RoguePlace *p = &r->places[pick];
        p->theme = RTHEME_CRATER;
        p->layout = RLAYOUT_STREETS;
        p->seaSide = RDIR_NONE;
        for (i = 0; i < p->nLinks; i++)
            r->places[p->links[i].target].theme = RTHEME_CRATER;
    }
}

// Their names end as they live, as Pacifidlog's (a drift of logs) and
// Sootopolis's do: the name's start stays (so names stay apart), its end is
// one of these, from the seed, where it fits.
static void Towns2Names(struct RogueRegion *r)
{
    static const char *const sRaftEnds[] = { "DRIFT", "WAKE", "FLOAT", "LOG", "SPRAY" };
    static const char *const sCraterEnds[] = { "POLIS", "HOLLOW", "RIM", "BASIN", "DELL" };
    int i;
    for (i = 0; i < r->nTowns; i++)
    {
        const struct RoguePlace *p = &r->places[r->towns[i]];
        const char *const *ends;
        u8 *name = r->townNames[i], start[ROGUE_NAME_LEN + 1], tail[8];
        int n, k, s, t, tries;
        if (p->theme != RTHEME_RAFT && p->theme != RTHEME_CRATER)
            continue;
        ends = p->theme == RTHEME_RAFT ? sRaftEnds : sCraterEnds;
        // The name's start: one of sNameStarts it begins with.
        for (s = 0, k = 0; k < (int)(sizeof(sNameStarts) / sizeof(sNameStarts[0])); k++)
        {
            int len = StrLen(sNameStarts[k]), j;
            for (j = 0; j < len && name[j] == sNameStarts[k][j]; j++)
                ;
            if (j == len && len > s)
                s = len;
        }
        // Its suffix: " TOWN" or " CITY".
        for (n = 0; name[n]; n++)
            ;
        for (t = n; t > 0 && name[t - 1] != ' '; t--)
            ;
        if (s == 0 || t == 0 || n - t + 1 > (int)sizeof(tail))
            continue;
        memcpy(start, name, s);
        for (k = 0; k + t - 1 < n; k++)
            tail[k] = name[t - 1 + k];
        tail[k] = 0;
        for (tries = 0; tries < 5; tries++)
        {
            const char *e = ends[(RogueRng_Hash(r->seed, TOWNS2_SALT_RAFT + 7 + i) + tries) % 5];
            int el = StrLen(e), tl = n - t + 1, j;
            if (s + el + tl > ROGUE_NAME_LEN)
                continue;
            for (j = 0; j < el; j++)
                name[s + j] = e[j];
            for (j = 0; j < tl; j++)
                name[s + el + j] = tail[j];
            name[s + el + tl] = 0;
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// The raft village (Pacifidlog)
//
// Pacifidlog's pieces, as its map draws them (block values, collision and
// elevation included). A house raft is five blocks wide: the roof's tip and
// the roof on the water above it, the house's two rows with the raft's
// edges beside them, one or two rows of deck, and the raft's lower edge on
// the water with a step down to the logs. The POKeMON CENTER's raft is six
// wide. Both have their door three rows down and their deck below it.
// The logs float at the deck's height: a log walkway down is pairs of
// 0x258 over 0x260 under a step, one across pairs of 0x250 and 0x251, and
// one across meets a raft only at the corner of its last deck row, as in
// Pacifidlog.

#define RAFT_SEA        0x1170
#define RAFT_LOG_TOP    0x3258 // a log down: its top half
#define RAFT_LOG_BOTTOM 0x3260 // and its bottom half
#define RAFT_LOG_LEFT   0x3250 // a log across: its left half
#define RAFT_LOG_RIGHT  0x3251 // and its right half
#define RAFT_STEP       0x322A // the step down off a raft's lower edge
#define RAFT_DECK       0x3221
#define RAFT_TOWN_SIGN  0x0403

static const u16 sRaftHouseRows[4][5] = {
    { RAFT_SEA, RAFT_SEA, 0x1202, RAFT_SEA, RAFT_SEA }, // the roof's tip, out over the water
    { 0x1208, 0x1209, 0x120A, 0x120B, 0x120C },         // the roof on the raft's top edge
    { 0x3210, 0x0611, 0x0612, 0x0613, 0x3214 },
    { 0x3218, 0x0619, 0x061A, 0x061B, 0x321C },         // the door at x + 2
};
static const u16 sRaftCenterRows[4][6] = {
    { RAFT_SEA, 0x1230, 0x1231, 0x1232, 0x1233, RAFT_SEA }, // the roof over the water
    { 0x1208, 0x1450, 0x1451, 0x1452, 0x1453, 0x120C },
    { 0x3210, 0x0458, 0x0459, 0x045A, 0x045B, 0x3214 },
    { 0x3218, 0x0648, 0x0461, 0x0462, 0x064B, 0x321C }, // the door at x + 2, the CENTER's sign beside it
};
// Deck rows: left edge, deck, right edge; the last row's corners differ.
#define RAFT_EDGE_L      0x3218
#define RAFT_EDGE_R      0x321C
#define RAFT_EDGE_L_LAST 0x3220
#define RAFT_EDGE_R_LAST 0x3224
// The raft's lower edge on the water: left corner, middle, right corner.
#define RAFT_LOW_L 0x1228
#define RAFT_LOW   0x1229
#define RAFT_LOW_R 0x122C

#define RAFT_MAX 7 // the CENTER's raft and up to six homes (ROGUE_MAX_HOUSES)
// A rock in the sea, as Pacifidlog has.
static const u16 sRaftSeaRock[4] = { 0x1575, 0x1576, 0x157D, 0x157E };

struct Raft
{
    s8 x, y;     // top left block (the roof's row)
    u8 w, h;     // in blocks, roof row and lower edge included
    u8 step;     // block x of the step down
    u8 center;   // the POKeMON CENTER's raft
    s8 joinY;    // row where its walkway meets the spine (the CENTER's: -1)
    s8 toX;      // the column its walkway runs across to (the spine's, or a branch's)
    u8 branched; // another home's walkway meets its own
};

static bool8 RaftUsed(struct Gen *gen, int x, int y)
{
    int i = y * gen->m->w + x;
    return (gen->onWalk[i >> 3] >> (i & 7)) & 1;
}

static void RaftMark(struct Gen *gen, int x, int y, bool8 on)
{
    int i = y * gen->m->w + x;
    if (on)
        gen->onWalk[i >> 3] |= 1 << (i & 7);
    else
        gen->onWalk[i >> 3] &= ~(1 << (i & 7));
}

// Open sea with nothing built on it, a block in from the map's edge.
static bool8 RaftSeaF(struct Gen *gen, int x, int y)
{
    if (x < 1 || y < 1 || x >= gen->m->w - 1 || y >= gen->m->h - 1)
        return FALSE;
    return gen->m->blocks[y * gen->m->w + x] == RAFT_SEA && !RaftUsed(gen, x, y);
}

// A rectangle of open sea with a ring of open sea round it, two units clear
// of the map's edge and out of view of a seam toward another tileset.
static bool8 RaftRectClear(struct Gen *gen, int x0, int y0, int w, int h)
{
    int x, y, ux0, uy0, ux1, uy1;
    if (x0 < 4 || y0 < 3 || x0 + w > gen->m->w - 4 || y0 + h > gen->m->h - 3)
        return FALSE;
    for (y = y0 - 1; y <= y0 + h; y++)
        for (x = x0 - 1; x <= x0 + w; x++)
            if (!RaftSeaF(gen, x, y))
                return FALSE;
    ux0 = (x0 - 1) / 2;
    uy0 = (y0 - 1) / 2;
    ux1 = (x0 + w) / 2;
    uy1 = (y0 + h) / 2;
    return !InSeamView(gen, ux0, uy0, ux1 - ux0 + 1, uy1 - uy0 + 1);
}

static void RaftMarkRect(struct Gen *gen, int x0, int y0, int w, int h, bool8 on)
{
    int x, y;
    for (y = y0; y < y0 + h; y++)
        for (x = x0; x < x0 + w; x++)
            RaftMark(gen, x, y, on);
}

// A log walkway block at (x, y) with open sea on the two sides across its
// run (vertical: left and right; across: above and below), out of view of
// a seam toward another tileset.
static bool8 RaftLogClear(struct Gen *gen, int x, int y, bool8 across)
{
    if (!RaftSeaF(gen, x, y) || InSeamView(gen, x / 2, y / 2, 1, 1))
        return FALSE;
    if (across)
        return RaftSeaF(gen, x, y - 1) && RaftSeaF(gen, x, y + 1);
    return RaftSeaF(gen, x - 1, y) && RaftSeaF(gen, x + 1, y);
}

// Tries a home's raft whose walkway meets the spine (column sx, open from
// row s0 to s1) at row r from side (-1 left, 1 right): steps down 2k blocks
// from the raft's step, then across to the spine. Marks it on success.
static bool8 RaftTryHome(struct Gen *gen, struct Raft *out, int sx, int s0, int s1, int r, int side, int floors, int stepAt,
                         int k, int gap, const u8 *joinSide)
{
    int h = 5 + floors, stepX = side < 0 ? sx - gap - 1 : sx + gap + 1, x0 = stepX - stepAt, bottom = r - 2 * k;
    int y0 = bottom - (h - 1), x, y;
    // (A walkway from the left meets the spine at a log's lower half, as
    // Pacifidlog's do.)
    if (r <= s0 || r + ((r - s0) % 2 == 0) > s1 || joinSide[r] != 0 || (side < 0 && (r - s0) % 2 == 0) || (r > 0 && joinSide[r - 1] == (u8)(side + 2))
     || joinSide[r + 1] == (u8)(side + 2))
        return FALSE;
    // The raft stays two blocks clear of the spine.
    if (side < 0 ? x0 + 5 > sx - 2 : x0 < sx + 3)
        return FALSE;
    if (!RaftRectClear(gen, x0, y0, 5, h))
        return FALSE;
    for (y = bottom + 1; y <= r; y++)
        if (!RaftLogClear(gen, stepX, y, FALSE))
            return FALSE;
    // The run across: open sea above and below, the corner block's too.
    for (x = side < 0 ? stepX + 1 : sx + 1; x <= (side < 0 ? sx - 1 : stepX - 1); x++)
        if (!RaftLogClear(gen, x, r, TRUE))
            return FALSE;
    if (!RaftSeaF(gen, stepX, r + 1) || !RaftSeaF(gen, stepX - side, r + 1) || !RaftSeaF(gen, stepX + side, r))
        return FALSE;
    out->x = x0;
    out->y = y0;
    out->w = 5;
    out->h = h;
    out->step = stepX;
    out->center = FALSE;
    out->joinY = r;
    out->toX = sx;
    out->branched = FALSE;
    RaftMarkRect(gen, x0, y0, 5, h, TRUE);
    for (y = bottom + 1; y <= r; y++)
        RaftMark(gen, stepX, y, TRUE);
    for (x = side < 0 ? stepX + 1 : sx + 1; x <= (side < 0 ? sx - 1 : stepX - 1); x++)
        RaftMark(gen, x, r, TRUE);
    return TRUE;
}

// Draws a raft: its roof row, house rows, deck and lower edge.
static void RaftDraw(struct Gen *gen, const struct Raft *rf)
{
    int x, y, deck = 4, last = rf->y + rf->h - 2;
    for (x = 0; x < rf->w; x++)
    {
        for (y = 0; y < deck; y++)
            Put(gen, rf->x + x, rf->y + y, rf->center ? sRaftCenterRows[y][x] : sRaftHouseRows[y][x]);
        for (y = rf->y + deck; y <= last; y++)
        {
            u16 b = RAFT_DECK;
            if (x == 0)
                b = y == last ? RAFT_EDGE_L_LAST : RAFT_EDGE_L;
            else if (x == rf->w - 1)
                b = y == last ? RAFT_EDGE_R_LAST : RAFT_EDGE_R;
            Put(gen, rf->x + x, y, b);
        }
        Put(gen, rf->x + x, last + 1, rf->x + x == rf->step ? RAFT_STEP
                                      : (x == 0 ? RAFT_LOW_L : (x == rf->w - 1 ? RAFT_LOW_R : RAFT_LOW)));
    }
}

// Where the player can walk in a raft village: the decks, edges, steps and
// logs (ground at the deck's height), with nobody standing there.
static bool8 RaftWalkF(struct Gen *gen, int x, int y)
{
    u16 b;
    if (!InMap(gen, x, y) || IsOccupied(gen, x, y))
        return FALSE;
    b = gen->m->blocks[y * gen->m->w + x];
    return !((b >> 10) & 3) && (b >> 12) == 3;
}

// How many blocks can be walked to from (sx, sy) (sBfsQueue as the queue,
// sBfsPrev as a bitmap: a village's map has more blocks than MAX_CELLS
// bits, but not than its 16-bit entries' bits).
static int RaftFlood(struct Gen *gen, int sx, int sy)
{
    u16 *seen = (u16 *)sBfsPrev;
    int head = 0, tail = 0, n = 0, w = gen->m->w;
    memset(sBfsPrev, 0, sizeof(sBfsPrev));
    if (!RaftWalkF(gen, sx, sy))
        return 0;
    sBfsQueue[tail++] = PACK(sx, sy);
    seen[(sy * w + sx) >> 4] |= 1 << ((sy * w + sx) & 15);
    while (head != tail)
    {
        int cur = sBfsQueue[head], cx = PX(cur), cy = PY(cur);
        u8 d;
        head = (head + 1) & (BFS_RING - 1);
        n++;
        for (d = 0; d < 4; d++)
        {
            int nx = cx + sDirDX[d], ny = cy + sDirDY[d], i = ny * w + nx;
            if (!RaftWalkF(gen, nx, ny) || ((seen[i >> 4] >> (i & 15)) & 1))
                continue;
            seen[i >> 4] |= 1 << (i & 15);
            sBfsQueue[tail] = PACK(nx, ny);
            tail = (tail + 1) & (BFS_RING - 1);
        }
    }
    return n;
}

static bool8 RaftReached(struct Gen *gen, int x, int y)
{
    const u16 *seen = (const u16 *)sBfsPrev;
    int i = y * gen->m->w + x;
    return InMap(gen, x, y) && ((seen[i >> 4] >> (i & 15)) & 1);
}

// A spot for someone on a raft's deck or edge: not on a log, a step, a
// door's front or beside a sign's front, and nothing cut off by them
// standing there.
static bool8 RaftSpot(struct Gen *gen, const struct Raft *rafts, int nRafts, int fx, int fy, int pcX, int pcY, int whole)
{
    u16 b;
    int k;
    if (!RaftWalkF(gen, fx, fy))
        return FALSE;
    b = gen->m->blocks[fy * gen->m->w + fx];
    if (b != RAFT_DECK && b != RAFT_EDGE_L && b != RAFT_EDGE_R && b != RAFT_EDGE_L_LAST && b != RAFT_EDGE_R_LAST
     && b != 0x3210 && b != 0x3214)
        return FALSE;
    for (k = 0; k < nRafts; k++)
    {
        // Not in front of a door, nor on the deck block above the step.
        if (fx == rafts[k].x + 2 && fy == rafts[k].y + 4)
            return FALSE;
        if (fx == rafts[k].step && fy == rafts[k].y + rafts[k].h - 2)
            return FALSE;
    }
    for (k = 0; k < gen->m->nSigns; k++)
        if (gen->m->signs[k].x == fx && gen->m->signs[k].y + 1 == fy)
            return FALSE;
    MarkOccupied(gen, fx, fy);
    k = RaftFlood(gen, pcX, pcY);
    {
        int i = fy * gen->m->w + fx;
        gen->occupied[i >> 3] &= ~(1 << (i & 7));
        if (gen->nTaken && gen->taken[gen->nTaken - 1] == PACK(fx, fy))
            gen->nTaken--;
    }
    return k == whole - 1;
}

static bool8 GenRaftTown(struct Gen *gen)
{
    const struct RoguePlace *p = gen->p;
    const struct RogueRegion *r = gen->r;
    struct RogueMap *m = gen->m;
    struct Raft rafts[RAFT_MAX];
    u8 joinSide[ROGUE_MAX_H + 2]; // per row: 0 none, 1 a walkway meets the spine from the left, 3 from the right
    int nRafts = 0, i, k, x, y, pcX, pcY, sx, s0, s1, sEnd, want, tries, whole, homes = 0, plan, side;
    u8 d;

    // The sea, with shores where land lies beyond a side, as a sea route
    // has (SeaLandDepth): a line of trees and a beach. The sea routes'
    // water runs on across the seams.
    gen->groundKind = GROUND_SAND;
    for (i = 0; i < gen->cw * gen->ch; i++)
        gen->g[i] = C_SEA;
    for (d = 0; d < 4; d++)
    {
        int len = EdgeLength(gen, d), pos;
        for (pos = 0; pos < len; pos++)
        {
            int depth = SeaLandDepth(r, p, d, pos, SEA_SHORE_LEVELS);
            for (k = 0; k < depth; k++)
            {
                EdgeCell(gen, d, pos, k, &x, &y);
                SetCell(gen, x, y, k == 0 ? C_TREE : (Cell(gen, x, y) == C_TREE ? C_TREE : C_OPEN));
            }
        }
    }
    for (i = 0; i < p->nLinks; i++)
    {
        const struct RogueLink *l = &p->links[i];
        gen->exitPos[l->dir] = l->pos;
        // A sea route drawn with another tileset (the one on toward the
        // next town, or a wreck's) sees none of the rafts.
        gen->keepOut[l->dir] = r->places[l->target].theme != p->theme;
    }
    for (i = 0; i < gen->cw * gen->ch; i++)
        if (gen->g[i] == C_OPEN)
            PaintUnit(gen, i % gen->cw, i / gen->cw, V_GROUND);
    TidyGround(gen);
    Render(gen);
    memset(gen->onWalk, 0, sizeof(gen->onWalk));
    memset(joinSide, 0, sizeof(joinSide));

    // The POKeMON CENTER's raft near the top, the spine of logs going down
    // from its step through the middle of the village.
    // (Lower down where the top is in view of a seam toward another
    // tileset.)
    // v1.1 layouts: three plans. 0: the CENTER in the middle at the top, a
    // long spine down; 1: the CENTER to one side, homes hanging off other
    // homes' walkways as well; 2: the CENTER lower down, homes spread wide
    // beside it on long walkways, some off other homes' walkways. (Later
    // tries keep to the first.)
    plan = gen->attempt < 40 ? RAND(3) : 0;
    side = RAND(2);
#ifdef ROGUE_HOST
    if (getenv("T2DBG"))
        fprintf(stderr, "towns2 place %d try %d plan raft %d\n", gen->m->place, gen->attempt, plan);
#endif
    for (tries = 0; tries < 60; tries++)
    {
        pcX = m->w / 2 - 5 + RAND(5);
        pcY = 3 + RAND(3) + (tries >= 20 ? RAND(m->h / 2 - 8) : 0);
        if (plan == 1)
            pcX = side ? m->w - 13 + RAND(3) : 7 + RAND(3);
        else if (plan == 2)
            pcY = 6 + RAND(3);
        if (RaftRectClear(gen, pcX, pcY, 6, 7))
            break;
    }
    if (tries == 60)
        T2FAIL("no room for the CENTER's raft");
    rafts[0].x = pcX;
    rafts[0].y = pcY;
    rafts[0].w = 6;
    rafts[0].h = 7;
    rafts[0].step = pcX + 2;
    rafts[0].center = TRUE;
    rafts[0].joinY = -1;
    rafts[0].toX = -1;
    rafts[0].branched = TRUE;
    nRafts = 1;
    RaftMarkRect(gen, pcX, pcY, 6, 7, TRUE);
    sx = pcX + 2;
    s0 = pcY + 7;
    // The spine may run down as far as the sea is open below it.
    for (s1 = s0; s1 + 1 < m->h - 3 && RaftLogClear(gen, sx, s1 + 1, FALSE); s1++)
        ;
    if (!RaftLogClear(gen, sx, s0, FALSE) || s1 - s0 < 5)
        T2FAIL("no spine");
    // Hold the spine's blocks while the homes are laid round it.
    for (y = s0; y <= s1; y++)
        RaftMark(gen, sx, y, TRUE);

    // Homes on both sides, each joined to the spine by its own walkway: down
    // from its step, then across. Four or five, as in Pacifidlog.
    // (v1.1 layouts: four to six.)
    want = 4 + RAND(3);
    for (tries = 0; tries < 600 && nRafts < want + 1; tries++)
    {
        int side = RAND(2) ? 1 : -1, floors = 1 + RAND(2), stepAt = RAND(3) ? 2 : 3, kk = 1 + RAND(tries < 300 ? 2 : 3);
        int gap = 4 + 2 * RAND(tries < 200 ? 3 : 5), row = s0 + 1 + RAND(s1 - s0);
        // (Longer walkways down in the other plans, for homes to meet.)
        if (plan != 0 && CHANCE(50))
            kk = 2 + RAND(2);
        if (plan == 2)
            gap = 8 + 2 * RAND(3);
        if (RaftTryHome(gen, &rafts[nRafts], sx, s0, s1, row, side, floors, stepAt, kk, gap, joinSide))
        {
            joinSide[row] = (u8)(side + 2);
            nRafts++;
        }
    }
    // v1.1 layouts: homes off other homes' walkways, down from a home's
    // step then across to that walkway, as onto the spine.
    if (plan != 0)
    {
        u8 none[ROGUE_MAX_H + 2];
        memset(none, 0, sizeof(none));
        for (tries = 0; tries < 300 && nRafts < want + 2 && nRafts < RAFT_MAX; tries++)
        {
            struct Raft *on = &rafts[1 + RAND(nRafts - 1)];
            int b0 = on->y + on->h, b1 = on->joinY - 1, side = RAND(2) ? 1 : -1, row;
            if (nRafts < 2 || on->branched || on->toX != sx || b1 - b0 < 2)
                continue;
            row = b0 + 1 + RAND(b1 - b0);
            // (Six blocks or more across, to clear that home's raft.)
            if (RaftTryHome(gen, &rafts[nRafts], on->step, b0, b1, row, side, 1 + RAND(2), RAND(3) ? 2 : 3, 1 + RAND(2),
                            6 + 2 * RAND(3), none))
            {
                on->branched = TRUE;
                rafts[nRafts].branched = TRUE;
                nRafts++;
            }
        }
    }
    // Fewer than four homes: another layout (v1.1 layouts: four, not
    // three, so no village is as bare as seed 22's was).
    if (nRafts < 5 && gen->attempt < 50)
        T2FAIL("too few homes");
    if (nRafts < 2)
        T2FAIL("no home");
    // The spine ends at the lowest walkway, after a whole log (an even
    // number of blocks).
    sEnd = s0;
    for (k = 1; k < nRafts; k++)
        if (rafts[k].toX == sx && rafts[k].joinY > sEnd)
            sEnd = rafts[k].joinY;
    if ((sEnd - s0) % 2 == 0)
        sEnd++;
    if (sEnd > s1)
        T2FAIL("spine too long");
    for (y = sEnd + 1; y <= s1; y++)
        RaftMark(gen, sx, y, FALSE);

    // Draw it all.
    for (k = 0; k < nRafts; k++)
        RaftDraw(gen, &rafts[k]);
    for (y = s0; y <= sEnd; y++)
        Put(gen, sx, y, (y - s0) % 2 ? RAFT_LOG_BOTTOM : RAFT_LOG_TOP);
    for (k = 1; k < nRafts; k++)
    {
        const struct Raft *rf = &rafts[k];
        int bottom = rf->y + rf->h - 1, a, b;
        for (y = bottom + 1; y <= rf->joinY; y++)
            Put(gen, rf->step, y, (y - bottom - 1) % 2 ? RAFT_LOG_BOTTOM : RAFT_LOG_TOP);
        a = rf->step < rf->toX ? rf->step + 1 : rf->toX + 1;
        b = rf->step < rf->toX ? rf->toX - 1 : rf->step - 1;
        for (x = a; x <= b; x++)
            Put(gen, x, rf->joinY, (x - a) % 2 ? RAFT_LOG_RIGHT : RAFT_LOG_LEFT);
    }

    // A few rocks out in the open water, as off Pacifidlog.
    for (tries = 0, k = 1 + RAND(3); tries < 80 && k > 0; tries++)
    {
        int rx = 3 + RAND(m->w - 7), ry = 3 + RAND(m->h - 7);
        bool8 ok = TRUE;
        for (y = ry - 2; y < ry + 4 && ok; y++)
            for (x = rx - 2; x < rx + 4 && ok; x++)
                if (!RaftSeaF(gen, x, y))
                    ok = FALSE;
        if (!ok)
            continue;
        for (i = 0; i < 4; i++)
            Put(gen, rx + (i & 1), ry + (i >> 1), sRaftSeaRock[i]);
        RaftMarkRect(gen, rx, ry, 2, 2, TRUE);
        k--;
    }

    // The doors: the CENTER's, with its sign beside the door, and the homes'.
    AddWarp(gen, pcX + 2, pcY + 3, RWARP_PC);
    AddSign(gen, pcX + 3, pcY + 3, RSIGN_PC);
    AddSign(gen, pcX + 4, pcY + 3, RSIGN_PC);
    // The town's sign on the CENTER's deck, where Pacifidlog's stands.
    AddSign(gen, pcX + 1, pcY + 4, RSIGN_TOWN);
    for (k = 1; k < nRafts && m->nHouses < ROGUE_MAX_HOUSES; k++)
    {
        AddWarp(gen, rafts[k].x + 2, rafts[k].y + 3, ROGUE_HOUSE_WARP(m->nHouses));
        m->interiors[m->nHouses++] = RINT_HOME;
        homes++;
    }
    // A plate by the door of the homes Emerald names (the Name Rater's, the
    // move tutor's), on the deck beside the door, at most two.
    for (k = 0, i = 0; k < m->nHouses && i < 2; k++)
    {
        const struct Raft *rf = &rafts[k + 1];
        u8 sign = HouseSignOf(gen, k);
        int fx = rf->x + 1, fy = rf->y + 4;
        if (sign != RSIGN_NAME_RATER && sign != RSIGN_TUTOR)
            continue;
        if (fx == rf->step && fy == rf->y + rf->h - 2)
            fx = rf->x + 3;
        if (fx == rf->step && fy == rf->y + rf->h - 2)
            continue;
        AddSign(gen, fx, fy, sign);
        i++;
    }
    RenderSigns(gen);

    // Everything can be walked to from the CENTER's door.
    whole = RaftFlood(gen, pcX + 2, pcY + 4);
    for (k = 0; k < nRafts; k++)
        if (!RaftReached(gen, rafts[k].x + 2, rafts[k].y + 4))
            T2FAIL("a door cut off");

    // Townsfolk on the decks and edges (one by the water, as Pacifidlog's
    // fisherman), and a POKeMON.
    {
        int folk = 0, mons = 0, wantFolk = 3 + RAND(2), start = RAND(8);
        for (tries = 0; tries < 300 && (folk < wantFolk || mons < 1); tries++)
        {
            const struct Raft *rf = &rafts[RAND(nRafts)];
            int fx = rf->x + RAND(rf->w), fy = rf->y + 1 + RAND(rf->h - 2);
            u8 facing = RAND(4);
            // (v1.1 layouts: nor in view of a seam, as in a crater town.)
            if ((tries < 200 && SeenFromNextDoor(gen, fx, fy)) || !RaftSpot(gen, rafts, nRafts, fx, fy, pcX + 2, pcY + 4, whole))
                continue;
            // Facing out over the water from an edge.
            if (fx == rf->x)
                facing = RDIR_WEST;
            else if (fx == rf->x + rf->w - 1)
                facing = RDIR_EAST;
            if (folk < wantFolk)
            {
                AddObj(gen, ROBJ_VILLAGER, fx, fy, facing, (start + folk * 3) & 7, 0);
                folk++;
            }
            else
            {
                AddObj(gen, ROBJ_MON, fx, fy, facing, mons, 0);
                mons++;
            }
            whole--;
        }
    }
    PairReporters(gen);
    return homes > 0;
}

// ---------------------------------------------------------------------------
// The crater town (Sootopolis)
//
// Three rings of white cliff round the crater, each a contour line round
// the terraces inside it, drawn with the piece Sootopolis uses for its run
// or corner (a face seen from below, a rim seen from above, sides, and each
// corner convex or concave), so the lines join as Sootopolis's do; the
// innermost has the brown rock on its lower side. Homes are cut into the
// first ring's face along the top, their roofs' tips in the face. Stone
// steps lead down through each ring. Inside, a band of brown rock round a
// sunken floor of grass (a face along its top with steps down, walls down
// its sides, an edge along its bottom: Sootopolis's own pieces for a pit),
// and on the floor the gym, the POKeMON CENTER and the Mart, a pond and
// trees. tools/roguemon/gen_crater_town.py lays out the same plan and checks
// every pair of blocks it draws against Hoenn's maps.

#define CR_RINGS 3
// What each block is while the town is laid out (gen->cover): a terrace
// (its ring count), a ring's line, the brown rock, the floor, or set (a
// home, a building, steps, the pond, a tree).
enum { CR_LINE = 0x10, CR_BROWN = 0x20, CR_FLOOR = 0x30, CR_SET = 0x40, CR_TAKEN = 0x80 /* kept clear round a home or steps */ };
#define CR_WALK(c) ((c) < CR_LINE || (c) == CR_BROWN || (c) == CR_FLOOR)

// Each ring's rows, as the x of their first and last block (gen->g; the
// fourth slot is scratch). An empty row has its first after its last.
#define CR_LO(gen, i, y) (((s8 *)(gen)->g)[((i) * ROGUE_MAX_H + (y)) * 2])
#define CR_HI(gen, i, y) (((s8 *)(gen)->g)[((i) * ROGUE_MAX_H + (y)) * 2 + 1])

// (Off the map a ring runs on as at the edge: a way in's pass through the
// rings runs off the map, its lines with it.)
static bool8 CrIn(struct Gen *gen, int i, int x, int y)
{
    if (i < 0)
        return TRUE;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= gen->m->w) x = gen->m->w - 1;
    if (y >= gen->m->h) y = gen->m->h - 1;
    return x >= CR_LO(gen, i, y) && x <= CR_HI(gen, i, y);
}

static void CrEmpty(struct Gen *gen, int i)
{
    int y;
    for (y = 0; y < ROGUE_MAX_H; y++)
    {
        CR_LO(gen, i, y) = 1;
        CR_HI(gen, i, y) = 0;
    }
}

static bool8 CrRowSet(struct Gen *gen, int i, int y)
{
    return y >= 0 && y < gen->m->h && CR_LO(gen, i, y) <= CR_HI(gen, i, y);
}

// Steps the four corners of ring i in, a staircase each. (The lower right
// steps run three blocks across or more: Sootopolis has no rim corner
// meeting the next one there.)
static void CrCutCorners(struct Gen *gen, int i, int x0, int y0, int x1, int y1, int deep)
{
    int c;
    for (c = 0; c < 4; c++)
    {
        bool8 right = c & 1, bottom = c >> 1;
        int d = deep + (int)RAND(3) - 1, row = 0;
        if (right && bottom && d < 3)
            d = 3;
        while (d > 0)
        {
            int y = bottom ? y1 - row : y0 + row;
            if (y < 0 || y >= gen->m->h)
                break;
            if (right)
            {
                if (CR_HI(gen, i, y) > x1 - d)
                    CR_HI(gen, i, y) = x1 - d;
            }
            else if (CR_LO(gen, i, y) < x0 + d)
                CR_LO(gen, i, y) = x0 + d;
            row++;
            if (right && bottom)
            {
                static const u8 sDec[] = { 0, 3, 3, 4 };
                int dec = sDec[RAND(4)];
                d -= (dec && d - dec < 3) ? d : dec;
            }
            else
            {
                static const u8 sDecA[] = { 0, 1, 1, 1, 2 }, sDecB[] = { 1, 1, 2 };
                d -= row > 1 ? sDecA[RAND(5)] : sDecB[RAND(3)];
            }
        }
    }
}

// Ring to: ring from shrunk by l, r, t and b blocks on its left, right,
// top and bottom (v1.1 layouts: unequal sides, so the rings are not
// copies of each other one size down). Each side keeps its steps' widths.
static void CrShrinkSides(struct Gen *gen, int from, int to, int l, int r, int t, int b)
{
    int y, k;
    for (y = 0; y < gen->m->h; y++)
    {
        int lo = -128, hi = 127;
        bool8 ok = TRUE;
        for (k = -t; k <= b && ok; k++)
        {
            if (!CrRowSet(gen, from, y + k))
                ok = FALSE;
            else
            {
                if (CR_LO(gen, from, y + k) > lo)
                    lo = CR_LO(gen, from, y + k);
                if (CR_HI(gen, from, y + k) < hi)
                    hi = CR_HI(gen, from, y + k);
            }
        }
        CR_LO(gen, to, y) = ok ? lo + l : 1;
        CR_HI(gen, to, y) = ok ? hi - r : 0;
        if (CR_LO(gen, to, y) > CR_HI(gen, to, y))
        {
            CR_LO(gen, to, y) = 1;
            CR_HI(gen, to, y) = 0;
        }
    }
}

#define CrShrink(gen, from, to, n) CrShrinkSides(gen, from, to, n, n, n, n)

// v1.1 layouts: once the rings are drawn, a stretch of ring i's side moves
// in or out, so the terraces between them narrow and widen as
// Sootopolis's contour lines do, instead of three copies of one ring.
// The left side moves one to three blocks, the right side three (a right
// side's step down and in must run three, CrCutCorners). A ring keeps two
// blocks from the rings beside it, and the innermost only moves out (the
// floor is inside it). FALSE when the stretch did not fit.
static bool8 CrWobble(struct Gen *gen, int i, bool8 right)
{
    int top, last, a, b, y, d, k;
    for (top = 0; top < gen->m->h && !CrRowSet(gen, i, top); top++)
        ;
    for (last = gen->m->h - 1; last >= 0 && !CrRowSet(gen, i, last); last--)
        ;
    top += i == 0 ? 8 : 4;
    last -= 4;
    if (last - top < 6)
        return FALSE;
    a = top + RAND(last - top - 4);
    b = a + 3 + RAND(last - a - 3 < 8 ? last - a - 3 : 8);
    d = right ? 3 : 1 + RAND(3);
    if (i == CR_RINGS - 1 || CHANCE(50))
        d = -d; // out
    for (y = a; y <= b; y++)
    {
        int lo = CR_LO(gen, i, y), hi = CR_HI(gen, i, y);
        if (!CrRowSet(gen, i, y))
            return FALSE;
        if (right)
            hi -= d;
        else
            lo += d;
        if (lo < 2 || hi > gen->m->w - 3 || hi - lo < 8)
            return FALSE;
        for (k = -1; k <= 1; k++)
        {
            if (y + k < 0 || y + k >= gen->m->h)
                continue;
            if (i > 0 && CrRowSet(gen, i - 1, y + k)
             && (lo < CR_LO(gen, i - 1, y + k) + 2 || hi > CR_HI(gen, i - 1, y + k) - 2))
                return FALSE;
            if (i > 0 && !CrRowSet(gen, i - 1, y + k))
                return FALSE;
            if (i + 1 < CR_RINGS && CrRowSet(gen, i + 1, y + k)
             && (lo > CR_LO(gen, i + 1, y + k) - 2 || hi < CR_HI(gen, i + 1, y + k) + 2))
                return FALSE;
        }
    }
    for (y = a; y <= b; y++)
    {
        if (right)
            CR_HI(gen, i, y) -= d;
        else
            CR_LO(gen, i, y) += d;
    }
    return TRUE;
}

// A block of ring i's line: outside the ring, inside the one round it,
// touching the ring (across a corner too).
static bool8 CrLine(struct Gen *gen, int i, int x, int y)
{
    int dx, dy;
    if (x < -1 || y < -1 || x > gen->m->w || y > gen->m->h || CrIn(gen, i, x, y) || !CrIn(gen, i - 1, x, y))
        return FALSE;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
            if (CrIn(gen, i, x + dx, y + dy))
                return TRUE;
    return FALSE;
}

// The piece of ring i's line at (x, y), or 0 if the line does not run
// through it as a line (it would branch). The lower ground is inside.
static u16 CrLinePiece(struct Gen *gen, int i, int x, int y)
{
    bool8 n = CrLine(gen, i, x, y - 1), s = CrLine(gen, i, x, y + 1);
    bool8 w = CrLine(gen, i, x - 1, y), e = CrLine(gen, i, x + 1, y);
    u16 b;
    if (n + s + w + e != 2)
        return 0;
    if (w && e)
        b = CrIn(gen, i, x, y + 1) ? 0x06E1 : 0x06D1;
    else if (n && s)
        b = CrIn(gen, i, x + 1, y) ? 0x06DA : 0x06D8;
    else if (s && e)
        b = (CrIn(gen, i, x - 1, y) || CrIn(gen, i, x, y - 1)) ? 0x06D0 : 0x06D3;
    else if (w && s)
        b = (CrIn(gen, i, x + 1, y) || CrIn(gen, i, x, y - 1)) ? 0x06D2 : 0x06D4;
    else if (n && w)
        b = (CrIn(gen, i, x + 1, y) || CrIn(gen, i, x, y + 1)) ? 0x06E2 : 0x06DB;
    else
        b = (CrIn(gen, i, x - 1, y) || CrIn(gen, i, x, y + 1)) ? 0x06E0 : 0x06DC;
    // The innermost ring's lower side is the brown rock.
    if (i == CR_RINGS - 1)
    {
        switch (b)
        {
        case 0x06D1: b = 0x06EC; break;
        case 0x06D8: b = 0x06F3; break;
        case 0x06D0: b = 0x06EB; break;
        case 0x06D2: b = 0x06ED; break;
        case 0x06E2: b = 0x06FD; break;
        case 0x06E0: b = 0x06FB; break;
        }
    }
    return b;
}

static u8 *CrCls(struct Gen *gen, int x, int y)
{
    return &gen->cover[y * gen->m->w + x];
}

// A block's kind, and whether it is kept clear (outside the map: a line).
static u8 CrKind(struct Gen *gen, int x, int y)
{
    return InMap(gen, x, y) ? (gen->cover[y * gen->m->w + x] & 0x7F) : CR_LINE;
}

static bool8 CrTaken(struct Gen *gen, int x, int y)
{
    return !InMap(gen, x, y) || (gen->cover[y * gen->m->w + x] & CR_TAKEN);
}

// Two steps 0x244 0x245 at (x, y) in a ring's face, from the terrace above
// to the one below.
static bool8 CrStairsOk(struct Gen *gen, int x, int y, int ring)
{
    int k;
    // (v1.1 layouts: with a piece Sootopolis has beside its steps on each
    // side, not a corner of the brown rock.)
    if (x < 1 || x + 2 >= gen->m->w)
        return FALSE;
    {
        u16 l = gen->m->blocks[y * gen->m->w + x - 1] & 0x3FF, r = gen->m->blocks[y * gen->m->w + x + 2] & 0x3FF;
        if ((l != 0x2E1 && l != 0x2D3 && l != 0x2E0) || (r != 0x2E1 && r != 0x2D4 && r != 0x2E2 && r != 0x2FD))
            return FALSE;
    }
    for (k = 0; k < 2; k++)
    {
        if (!InMap(gen, x + k, y + 1) || y < 1 || gen->m->blocks[y * gen->m->w + x + k] != 0x06E1
         || CrKind(gen, x + k, y) != CR_LINE + ring || !CR_WALK(CrKind(gen, x + k, y - 1))
         || !CR_WALK(CrKind(gen, x + k, y + 1)) || CrTaken(gen, x + k, y))
            return FALSE;
    }
    return TRUE;
}

static void CrTake(struct Gen *gen, int x0, int y0, int x1, int y1)
{
    int x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            if (InMap(gen, x, y))
                *CrCls(gen, x, y) |= CR_TAKEN;
}

static bool8 CrPlaceStairs(struct Gen *gen, int ring, int want)
{
    int tries, placed = 0;
    for (tries = 0; tries < 400 && placed < want; tries++)
    {
        int x = 1 + RAND(gen->m->w - 3), y = 1 + RAND(gen->m->h - 3);
        if (!CrStairsOk(gen, x, y, ring))
            continue;
        Put(gen, x, y, 0x3244);
        Put(gen, x + 1, y, 0x3245);
        *CrCls(gen, x, y) = *CrCls(gen, x + 1, y) = CR_SET | CR_TAKEN;
        CrTake(gen, x - 1, y - 1, x + 2, y + 1);
        placed++;
    }
    return placed > 0;
}

// v1.1 layouts: a block seen from just across a way in (as check_seeds.py
// counts it: about half a screen round the opening, as deep as the camera
// shows), where someone standing would pop in.
static bool8 CrNearWayIn(struct Gen *gen, int x, int y)
{
    int i;
    for (i = 0; i < gen->p->nLinks; i++)
    {
        const struct RogueLink *l = &gen->p->links[i];
        int d = l->dir, depth, along;
        depth = d == RDIR_NORTH ? y : d == RDIR_SOUTH ? gen->m->h - 1 - y : d == RDIR_WEST ? x : gen->m->w - 1 - x;
        along = ((d == RDIR_NORTH || d == RDIR_SOUTH) ? x : y) - (l->pos & ~1);
        if (depth <= 2)
            return TRUE;
        if ((d == RDIR_NORTH || d == RDIR_SOUTH) && depth <= 4 && along >= -7 && along <= 8)
            return TRUE;
        if ((d == RDIR_WEST || d == RDIR_EAST) && depth <= 6 && along >= -5 && along <= 6)
            return TRUE;
    }
    return FALSE;
}

// Where the player walks in a crater town: ground at the terraces' height,
// with nobody on it (as in a raft village).
#define CrFlood(gen, x, y) RaftFlood(gen, x, y)
#define CrReached(gen, x, y) RaftReached(gen, x, y)

static bool8 GenCraterTown(struct Gen *gen)
{
    const struct RoguePlace *p = gen->p;
    struct RogueMap *m = gen->m;
    int W = m->w, H = m->h, i, k, x, y, tries;
    int ix0 = 99, ix1 = -1, iy0 = 99, iy1 = -1, fx0 = 0, fx1 = 0, fy0 = 0, fy1 = 0, best = -1;
    bool8 city = p->type == RTOWN_GYM;
    int homeX[ROGUE_MAX_HOUSES], homeY[ROGUE_MAX_HOUSES], nHomes = 0, want;
    int doorX[3], doorY[3], nDoors = 0, pcX = -1, pcY = -1, sx = -1;

    // The rings: the first three blocks in from the edges with its corners
    // stepped in, each next one two blocks further in; the first ring's
    // band along the top is deep enough for the homes cut into its face.
    // A ring's left side now and then steps in further for a stretch.
    for (i = 0; i <= CR_RINGS; i++)
        CrEmpty(gen, i);
    for (y = 3; y <= H - 4; y++)
    {
        CR_LO(gen, 0, y) = 3;
        CR_HI(gen, 0, y) = W - 4;
    }
    // (Rounder than a box: corners cut deep, more or less so each time.)
    // (Later tries go back to the squarer shape, which leaves the floor
    // more room.)
    CrCutCorners(gen, 0, 3, 3, W - 4, H - 4, gen->attempt < 30 ? 4 + RAND(3) : 3);
    for (i = 1; i < CR_RINGS; i++)
    {
        int top, last, minLo = 127;
        // The terraces between the rings one or two blocks wide. (v1.1
        // layouts: on early tries the left and right ones one to three
        // wide, each its own, so the rings are not copies one size down;
        // the map has room to spare across, not down.)
        {
            int n = i == 1 && gen->attempt < 20 && CHANCE(50) ? 3 : 2;
            if (gen->attempt < 30)
            {
                int l = 2 + RAND(3), rr = 2 + RAND(3);
                if (l + rr > 7)
                    rr = 7 - l;
                CrShrinkSides(gen, i - 1, i, l, rr, n, 2);
            }
            else
                CrShrink(gen, i - 1, i, n);
        }
        for (top = 0; top < H && !CrRowSet(gen, i, top); top++)
            ;
        for (last = H - 1; last >= 0 && !CrRowSet(gen, i, last); last--)
            ;
        if (top >= last)
            T2FAIL("no ring");
        if (i == 1)
        {
            for (y = top; y < top + 4; y++)
            {
                CR_LO(gen, i, y) = 1;
                CR_HI(gen, i, y) = 0;
            }
            top += 4;
        }
        if (CHANCE(50))
        {
            int rows = last - top + 1, a = rows / 4 + RAND(rows / 2 - rows / 4 + 1), b = a + 3 + RAND(rows / 3 - 2 > 0 ? rows / 3 - 2 : 1);
            for (y = top; y <= last; y++)
                if (CR_LO(gen, i, y) < minLo)
                    minLo = CR_LO(gen, i, y);
            for (y = top + a; y <= top + b && y <= last; y++)
                if (CR_LO(gen, i, y) == minLo)
                    CR_LO(gen, i, y)++;
        }
        // The right side steps in too now and then: three blocks, as its
        // lower step must run (CrCutCorners).
        if (CHANCE(40))
        {
            int rows = last - top + 1, a = rows / 3 + RAND(rows / 3 + 1), b = a + 3 + RAND(4), maxHi = -1;
            for (y = top; y <= last; y++)
                if (CR_HI(gen, i, y) > maxHi)
                    maxHi = CR_HI(gen, i, y);
            for (y = top + a; y <= top + b && y <= last; y++)
                if (CR_HI(gen, i, y) == maxHi)
                    CR_HI(gen, i, y) -= 3;
        }
    }
    // v1.1 layouts: the rings' sides wander (early tries).
    if (gen->attempt < 40)
    {
        int moved = 0;
        for (tries = 0; tries < 24 && moved < 4 + (int)RAND(3); tries++)
            moved += CrWobble(gen, RAND(CR_RINGS), CHANCE(40));
    }
    // The floor: the biggest rectangle that keeps two blocks of brown rock
    // inside the innermost ring (three at the top, for the face).
    for (y = 0; y < H; y++)
    {
        if (!CrRowSet(gen, CR_RINGS - 1, y))
            continue;
        if (y < iy0) iy0 = y;
        if (y > iy1) iy1 = y;
        if (CR_LO(gen, CR_RINGS - 1, y) < ix0) ix0 = CR_LO(gen, CR_RINGS - 1, y);
        if (CR_HI(gen, CR_RINGS - 1, y) > ix1) ix1 = CR_HI(gen, CR_RINGS - 1, y);
    }
    CrShrink(gen, CR_RINGS - 1, CR_RINGS, 2);
    for (i = 0; i < 5 * 5 * 3; i++)
    {
        int a = i % 5, b = (i / 5) % 5, c = i / 25;
        int x0 = ix0 + 2 + a, x1 = ix1 - 2 - b, y0 = iy0 + 3, y1 = iy1 - 2 - c, area;
        bool8 ok = x1 - x0 >= 8 && y1 - y0 >= 6;
        for (y = y0; y <= y1 && ok; y++)
            if (!CrRowSet(gen, CR_RINGS, y) || CR_LO(gen, CR_RINGS, y) > x0 || CR_HI(gen, CR_RINGS, y) < x1)
                ok = FALSE;
        area = (x1 - x0 + 1) * (y1 - y0 + 1) - Abs(a - b) * 2;
        if (ok && area > best)
        {
            best = area;
            fx0 = x0, fx1 = x1, fy0 = y0, fy1 = y1;
        }
    }
    if (best < 0 || fx1 - fx0 < (city ? 19 : 15) || fy1 - fy0 < (city ? 11 : 9))
        T2FAIL("no room for the floor");

    // A pass through the outer two rings at each way in but the top's, so
    // the way comes in at the inner terrace between white walls, not over
    // the outer rim and round to the steps.
    for (k = 0; k < p->nLinks; k++)
    {
        const struct RogueLink *l = &p->links[k];
        int r;
        if (l->dir == RDIR_NORTH)
            continue;
        for (r = 0; r < 2; r++)
        {
            int hw = r == 0 ? 4 : 2, pos = l->pos;
            if (l->dir == RDIR_SOUTH)
            {
                for (y = H / 2; y < H; y++)
                {
                    if (!CrRowSet(gen, r, y))
                    {
                        CR_LO(gen, r, y) = pos - hw;
                        CR_HI(gen, r, y) = pos + hw;
                    }
                    else
                    {
                        if (CR_LO(gen, r, y) > pos - hw) CR_LO(gen, r, y) = pos - hw;
                        if (CR_HI(gen, r, y) < pos + hw) CR_HI(gen, r, y) = pos + hw;
                    }
                }
            }
            else
            {
                for (y = pos - hw; y <= pos + hw; y++)
                {
                    if (y < 0 || y >= H || !CrRowSet(gen, r, y))
                        T2FAIL("no ring where a way comes in");
                    if (l->dir == RDIR_EAST)
                        CR_HI(gen, r, y) = W - 1;
                    else
                        CR_LO(gen, r, y) = 0;
                }
            }
        }
    }

    // What each block is, and its first drawing.
    for (y = 0; y < H; y++)
    {
        YIELD();
        for (x = 0; x < W; x++)
        {
            u8 c = 0;
            u16 b = 0x32D9;
            for (i = 0; i < CR_RINGS; i++)
                c += CrIn(gen, i, x, y);
            if (c == CR_RINGS)
            {
                bool8 fl = x >= fx0 && x <= fx1 && y >= fy0 && y <= fy1;
                c = fl ? CR_FLOOR : CR_BROWN;
                b = fl ? 0x3001 : 0x3071;
            }
            else
            {
                for (i = 0; i < CR_RINGS; i++)
                {
                    if (CrLine(gen, i, x, y))
                    {
                        c = CR_LINE + i;
                        b = CrLinePiece(gen, i, x, y);
                        if (b == 0)
                            T2FAIL("a ring's line branches");
                        break;
                    }
                }
            }
            *CrCls(gen, x, y) = c;
            Put(gen, x, y, b);
        }
    }
    // (A pass's wall meeting a rim a block from a corner leaves a corner
    // piece beside the next corner, which Sootopolis never has: try again.)
    for (y = 0; y < H; y++)
        for (x = 0; x + 1 < W; x++)
            if ((m->blocks[y * W + x] & 0x3FF) == 0x2D0 && (m->blocks[y * W + x + 1] & 0x3FF) == 0x2DB)
                T2FAIL("a pass's corner too tight");
    // The pit's edges: Sootopolis's face along the top, walls down the
    // sides and edge along the bottom.
    for (x = fx0 - 1; x <= fx1 + 1; x++)
    {
        Put(gen, x, fy0 - 1, x == fx0 - 1 ? 0x0474 : (x == fx1 + 1 ? 0x0489 : 0x0479));
        Put(gen, x, fy1 + 1, x == fx0 - 1 ? 0x04BF : (x == fx1 + 1 ? 0x30BE : 0x0469));
        *CrCls(gen, x, fy0 - 1) = *CrCls(gen, x, fy1 + 1) = CR_SET;
    }
    for (y = fy0; y <= fy1; y++)
    {
        Put(gen, fx0 - 1, y, 0x0472);
        Put(gen, fx1 + 1, y, 0x0470);
        *CrCls(gen, fx0 - 1, y) = *CrCls(gen, fx1 + 1, y) = CR_SET;
    }
    // (The top corner piece of the bottom edge stays walkable, as
    // Sootopolis's.)
    *CrCls(gen, fx1 + 1, fy1 + 1) = CR_BROWN;

    // Steps from the outer terrace to the homes' terrace, before the homes.
    if (!CrPlaceStairs(gen, 0, 1 + RAND(2)))
        T2FAIL("no steps down the first ring");
    // Homes cut into the first ring's face along the top: three to five,
    // under a run of the face five blocks wide with level ground beside the
    // roof, its tip in the face.
    want = 3 + RAND(3);
    for (tries = 0; tries < 300 && nHomes < want; tries++)
    {
        int hx, hy, dx, dy;
        bool8 ok = TRUE;
        x = 3 + RAND(W - 6);
        for (y = 1; y < H / 2 && CrKind(gen, x, y) != CR_LINE; y++)
            ;
        if (y >= H / 2)
            continue;
        for (k = -2; k <= 2 && ok; k++)
            if (m->blocks[y * W + x + k] != 0x06E1 || CrTaken(gen, x + k, y))
                ok = FALSE;
        hx = x - 1;
        hy = y + 1;
        if (!ok || CrKind(gen, x - 2, hy) != 1 || CrKind(gen, x + 2, hy) != 1)
            continue;
        for (dy = 0; dy <= 4 && ok; dy++)
            for (dx = 0; dx < 3 && ok; dx++)
                if (CrKind(gen, hx + dx, hy + dy) != 1 || CrTaken(gen, hx + dx, hy + dy))
                    ok = FALSE;
        if (!ok)
            continue;
        Put(gen, x, y, 0x0639);
        for (dy = 0; dy < 4; dy++)
        {
            static const u16 sHome[4][3] = { { 0x0601, 0x0602, 0x0603 }, { 0x0609, 0x060A, 0x060B },
                                             { 0x0623, 0x0614, 0x0624 }, { 0x062B, 0x061C, 0x062C } };
            for (dx = 0; dx < 3; dx++)
            {
                Put(gen, hx + dx, hy + dy, sHome[dy][dx]);
                *CrCls(gen, hx + dx, hy + dy) = CR_SET | CR_TAKEN;
            }
        }
        homeX[nHomes] = x;
        homeY[nHomes++] = hy + 3;
        CrTake(gen, hx - 2, hy - 1, hx + 4, hy + 5);
    }
    if (nHomes < 3)
        T2FAIL("too few homes");
    // Steps down the other rings, onto the brown rock at last.
    for (i = 1; i < CR_RINGS; i++)
        if (!CrPlaceStairs(gen, i, 1 + RAND(2)))
            T2FAIL("no steps down a ring");

    // The floor: the gym (a city's), the CENTER and the Mart along its top.
    {
        static const struct { const u16 *stamp; u8 w, h, doorX, doorY; u8 warp; } sFloorB[3] = {
            { sStamp_Gym, 6, 5, 3, 4, RWARP_GYM },
            { sStamp_PokemonCenter, 4, 4, 1, 3, RWARP_PC },
            { sStamp_Mart, 4, 4, 1, 3, RWARP_MART },
        };
        u8 order[3], n = 0, t;
        int cur[2], rowOf, rowEnd = 0, rowStart = 99, total = 0, lowest = fy0 + 1, highest = fy1, bx, by, bxFirst, bxEnd;
        // v1.1 layouts: three floor plans. 0: the buildings along the top,
        // the pond below them; 1: the pond under the steps, the buildings
        // along the bottom; 2: the buildings in two rows at one end (the
        // gym alone in its row), the pond beside them. (Later tries keep to
        // the first.)
        int plan = gen->attempt < 40 ? (int)RAND(3) : 0, side = RAND(2), flip = RAND(2);
#ifdef ROGUE_HOST
        if (getenv("T2DBG"))
            fprintf(stderr, "towns2 place %d try %d plan crater %d\n", gen->m->place, gen->attempt, plan);
#endif
        if (city)
            order[n++] = 0;
        order[n++] = 1;
        order[n++] = 2;
        for (i = n - 1; i > 0; i--)
        {
            k = RAND(i + 1);
            t = order[i];
            order[i] = order[k];
            order[k] = t;
        }
        for (i = 0; i < n; i++)
            total += sFloorB[order[i]].w + 2;
        total -= 2;
        bx = fx0 + 2 + RAND((fx1 - fx0 - 3) - total > 0 ? (fx1 - fx0 - 3) - total + 1 : 1);
        cur[0] = cur[1] = side ? fx1 - 1 : fx0 + 2;
        bxFirst = fx1;
        bxEnd = fx0;
        for (i = 0; i < n; i++)
        {
            const u16 *st = sFloorB[order[i]].stamp;
            int sw = sFloorB[order[i]].w, sh = sFloorB[order[i]].h, dx, dy;
            by = fy0 + 1 + (city ? 5 - sh : 0);
            if (plan == 1)
                by = fy1 - sh;
            if (plan == 2)
            {
                // (The top row or the bottom one; the gym alone.)
                rowOf = (city ? order[i] != 0 : (i & 1)) ^ flip;
                bx = side ? cur[rowOf] - sw : cur[rowOf];
                cur[rowOf] = side ? bx - 2 : bx + sw + 2;
                by = rowOf ? fy1 - sh : fy0 + 1;
                if (rowOf && by < rowStart)
                    rowStart = by;
                if (!rowOf && by + sh > rowEnd)
                    rowEnd = by + sh;
            }
            if (bx - 1 < fx0 || bx + sw > fx1 || by + sh > fy1)
                T2FAIL("no room for the buildings");
            for (dy = 0; dy < sh; dy++)
                for (dx = 0; dx < sw; dx++)
                {
                    Put(gen, bx + dx, by + dy, st[dy * sw + dx]);
                    *CrCls(gen, bx + dx, by + dy) = CR_SET;
                }
            AddWarp(gen, bx + sFloorB[order[i]].doorX, by + sFloorB[order[i]].doorY, sFloorB[order[i]].warp);
            doorX[nDoors] = bx + sFloorB[order[i]].doorX;
            doorY[nDoors++] = by + sFloorB[order[i]].doorY;
            switch (sFloorB[order[i]].warp)
            {
            case RWARP_PC:
                pcX = doorX[nDoors - 1];
                pcY = doorY[nDoors - 1];
                // (fall through: the sign on its front)
            case RWARP_MART:
                AddSign(gen, bx + 2, by + 3, sFloorB[order[i]].warp == RWARP_PC ? RSIGN_PC : RSIGN_MART);
                AddSign(gen, bx + 3, by + 3, sFloorB[order[i]].warp == RWARP_PC ? RSIGN_PC : RSIGN_MART);
                break;
            default:
                AddSign(gen, bx + sFloorB[order[i]].doorX + 1, by + sFloorB[order[i]].doorY, RSIGN_GYM);
                // Flowers by the roof's left end, as Hoenn's gyms have them.
                Put(gen, bx - 1, by, 0x3004);
                Put(gen, bx - 1, by + 1, 0x3004);
                break;
            }
            if (by + sh > lowest)
                lowest = by + sh;
            if (by < highest)
                highest = by;
            if (bx + sw > bxEnd)
                bxEnd = bx + sw;
            if (bx < bxFirst)
                bxFirst = bx;
            bx += sw + 2 + RAND(2);
        }
        // (Plan 2: a row of grass at least between the two rows, below the
        // upper doors.)
        if (plan == 2 && rowStart <= rowEnd)
            T2FAIL("the two rows too close");
        // Steps down the brown rock's face to the floor, where the floor
        // below them is open.
        for (tries = 0; tries < 80; tries++)
        {
            bool8 ok = TRUE;
            x = fx0 + 1 + RAND(fx1 - fx0 - 2);
            for (y = fy0; y <= fy0 + 1 && ok; y++)
                for (k = -1; k <= 2 && ok; k++)
                    if (CrKind(gen, x + k, y) != CR_FLOOR)
                        ok = FALSE;
            if (!ok)
                continue;
            Put(gen, x, fy0 - 1, 0x3244);
            Put(gen, x + 1, fy0 - 1, 0x3245);
            sx = x;
            break;
        }
        if (sx < 0)
            T2FAIL("no steps down to the floor");
        // A pond below, as General's ponds: banks on top and sides, and the
        // bank under it on the grass.
        {
            int top = lowest + 1, lx0 = fx0 + 3 + RAND(3), lx1 = fx1 - 3 - RAND(3), bottom = fy1 - 2;
            if (plan == 1)
            {
                top = fy0 + 2;
                bottom = highest - 3;
            }
            else if (plan == 2)
            {
                top = fy0 + 3;
                lx0 = side ? fx0 + 2 + RAND(2) : bxEnd + 2;
                lx1 = side ? bxFirst - 3 : fx1 - 2 - RAND(2);
            }
            if (bottom - top < 1 || lx1 - lx0 < 4)
                T2FAIL("no room for the pond");
            for (y = top; y <= bottom; y++)
                for (x = lx0; x <= lx1; x++)
                {
                    u16 b = y == top ? (x == lx0 ? 0x10B0 : x == lx1 ? 0x10B2 : 0x10B1)
                                     : (x == lx0 ? 0x10B8 : x == lx1 ? 0x10BA : 0x10A1);
                    Put(gen, x, y, b);
                    *CrCls(gen, x, y) = CR_SET;
                }
            for (x = lx0; x <= lx1; x++)
                Put(gen, x, bottom + 1, 0x3002);
            // Trees by the pond's ends, on grass with grass all round.
            for (k = 0; k < 3; k++)
            {
                int tx = k == 2 ? fx0 + 3 + RAND(fx1 - fx0 - 6) : k ? fx1 - 3 : fx0 + 2, ty;
                for (ty = fy1 - 3; ty >= fy0 + 2; ty--)
                {
                    bool8 ok = TRUE;
                    int dx, dy;
                    for (dy = -2; dy <= 2 && ok; dy++)
                        for (dx = -1; dx <= 2 && ok; dx++)
                        {
                            int cx = tx + dx, cy = ty + dy;
                            if (cx < fx0 || cx > fx1 || cy < fy0 || cy > fy1)
                                continue;
                            if (CrKind(gen, cx, cy) != CR_FLOOR || m->blocks[cy * W + cx] != 0x3001)
                                ok = FALSE;
                        }
                    if (!ok)
                        continue;
                    Put(gen, tx, ty, 0x05D6);
                    Put(gen, tx + 1, ty, 0x05D7);
                    Put(gen, tx, ty + 1, 0x05E6);
                    Put(gen, tx + 1, ty + 1, 0x05E7);
                    Put(gen, tx, ty - 1, 0x31CE);
                    Put(gen, tx + 1, ty - 1, 0x31CF);
                    *CrCls(gen, tx, ty) = *CrCls(gen, tx + 1, ty) = CR_SET;
                    *CrCls(gen, tx, ty + 1) = *CrCls(gen, tx + 1, ty + 1) = CR_SET;
                    break;
                }
            }
        }
    }
    if (pcX < 0)
        T2FAIL("no CENTER");
    // The ways in, the homes' doors and the town's sign on the floor by
    // the steps down.
    for (i = 0; i < p->nLinks; i++)
    {
        gen->exitPos[p->links[i].dir] = p->links[i].pos;
        gen->keepOut[p->links[i].dir] = gen->r->places[p->links[i].target].theme != p->theme;
    }
    for (k = 0; k < nHomes && m->nHouses < ROGUE_MAX_HOUSES; k++)
    {
        AddWarp(gen, homeX[k], homeY[k], ROGUE_HOUSE_WARP(m->nHouses));
        m->interiors[m->nHouses++] = RINT_HOME;
    }
    {
        // Beside the steps down first; failing that, further along the
        // floor's top rows (seed 86's).
        static const s8 sAt[][2] = { { -1, 1 }, { 2, 1 }, { -2, 1 }, { 3, 1 }, { -1, 2 }, { 2, 2 } };
        enum { N_AT = sizeof(sAt) / sizeof(sAt[0]), N_FAR = 3 * 2 * 12 * 3 };
        for (k = 0; k < N_AT + N_FAR; k++)
        {
            int fx, fy, j, dx, dy;
            bool8 ok;
            if (k < N_AT)
            {
                fx = sx + sAt[k][0];
                fy = fy0 + sAt[k][1];
            }
            else
            {
                int f = k - N_AT, d = 3 + f / 6;
                fx = sx + ((f & 1) ? d : -d);
                fy = fy0 + 1 + (f / 2) % (fy1 - fy0 - 1);
            }
            ok = fx > fx0 && fx < fx1 && !IsOccupied(gen, fx, fy);
            // (On grass with grass all round it.)
            for (dy = -1; dy <= 1 && ok; dy++)
                for (dx = -1; dx <= 1 && ok; dx++)
                    if (CrKind(gen, fx + dx, fy + dy) != CR_FLOOR || m->blocks[(fy + dy) * W + fx + dx] != 0x3001)
                        ok = FALSE;
            for (j = 0; j < nDoors && ok; j++)
                if (Abs(fx - doorX[j]) <= 1 && fy - doorY[j] >= 0 && fy - doorY[j] <= 1)
                    ok = FALSE;
            if (!ok)
                continue;
            AddSign(gen, fx, fy, RSIGN_TOWN);
            break;
        }
        if (k == N_AT + N_FAR)
            T2FAIL("no room for the town's sign");
    }
    RenderSigns(gen);
    // A plate by the door of the homes Emerald names (the Name Rater's,
    // the move tutor's): on the wall beside the door, read from its front.
    for (k = 0, i = 0; k < m->nHouses && i < 2; k++)
    {
        u8 sign = HouseSignOf(gen, k);
        if (sign != RSIGN_NAME_RATER && sign != RSIGN_TUTOR)
            continue;
        AddSign(gen, homeX[k] - 1, homeY[k], sign);
        i++;
    }

    // Everything can be walked to from the CENTER's door.
    {
        int whole = CrFlood(gen, pcX, pcY + 1), start = RAND(8), folk = 0, mons = 0;
        int wantFolk = city ? 5 : 4, wantMons = city ? 2 : 1;
        for (k = 0; k < nHomes; k++)
            if (!CrReached(gen, homeX[k], homeY[k] + 1))
                T2FAIL("a home cut off");
        for (k = 0; k < nDoors; k++)
            if (!CrReached(gen, doorX[k], doorY[k] + 1))
                T2FAIL("a building cut off");
        for (i = 0; i < p->nLinks; i++)
        {
            const struct RogueLink *l = &p->links[i];
            int ex = l->dir == RDIR_WEST ? 0 : l->dir == RDIR_EAST ? W - 1 : l->pos;
            int ey = l->dir == RDIR_NORTH ? 0 : l->dir == RDIR_SOUTH ? H - 1 : l->pos;
            if (!CrReached(gen, ex, ey))
                T2FAIL("a way in cut off");
        }
        // Townsfolk on the terraces and the floor, and a POKeMON or two:
        // not on steps, a door's front or a sign's, nor in view of a seam,
        // and nothing cut off by them standing there.
        for (tries = 0; tries < 400 && (folk < wantFolk || mons < wantMons); tries++)
        {
            int fx = 1 + RAND(W - 2), fy = 1 + RAND(H - 2), j;
            u8 c = CrKind(gen, fx, fy);
            bool8 ok = CR_WALK(c) && CrReached(gen, fx, fy) && !IsOccupied(gen, fx, fy) && !SeenFromNextDoor(gen, fx, fy)
                    && !CrNearWayIn(gen, fx, fy)
                    && CrKind(gen, fx, fy - 1) != CR_SET && CrKind(gen, fx, fy + 1) != CR_SET && !CrTaken(gen, fx, fy);
            for (j = 0; j < m->nWarps && ok; j++)
                if (Abs(m->warps[j].x - fx) <= 1 && Abs(m->warps[j].y + 1 - fy) <= 1)
                    ok = FALSE;
            for (j = 0; j < m->nSigns && ok; j++)
                if (Abs(m->signs[j].x - fx) + Abs(m->signs[j].y - fy) <= 1)
                    ok = FALSE;
            if (!ok)
                continue;
            MarkOccupied(gen, fx, fy);
            if (CrFlood(gen, pcX, pcY + 1) != whole - 1)
            {
                j = fy * W + fx;
                gen->occupied[j >> 3] &= ~(1 << (j & 7));
                if (gen->nTaken && gen->taken[gen->nTaken - 1] == PACK(fx, fy))
                    gen->nTaken--;
                CrFlood(gen, pcX, pcY + 1);
                continue;
            }
            whole--;
            if (folk < wantFolk)
                AddObj(gen, ROBJ_VILLAGER, fx, fy, RAND(4), (start + folk++ * 3) & 7, 0);
            else
                AddObj(gen, ROBJ_MON, fx, fy, RAND(4), mons++, 0);
            CrFlood(gen, pcX, pcY + 1);
        }
    }
    // Flowers here and there on the floor's open grass.
    for (tries = 0, k = 6 + RAND(8); tries < 200 && k > 0; tries++)
    {
        x = fx0 + 1 + RAND(fx1 - fx0 - 1);
        y = fy0 + 1 + RAND(fy1 - fy0 - 1);
        bool8 ok = CrKind(gen, x, y) == CR_FLOOR && m->blocks[y * W + x] == 0x3001 && !IsOccupied(gen, x, y);
        for (i = 0; i < 4 && ok; i++)
        {
            u16 b = m->blocks[(y + sDirDY[i]) * W + x + sDirDX[i]];
            ok = b == 0x3001 || b == 0x3004;
        }
        if (!ok)
            continue;
        Put(gen, x, y, 0x3004);
        k--;
    }
    PairReporters(gen);
    for (i = 0; i < p->nLinks; i++)
        if (p->links[i].gate)
            AddGate(gen, p->links[i].dir, p->links[i].gate);
    return TRUE;
}
