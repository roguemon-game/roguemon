// Tunnels: an optional dungeon near the early road with a second way out
// onto the road a few gyms later, as Diglett's Cave joins Route 11 to
// Route 2, Johto's Dark Cave Route 31 to Route 45, and Rusturf Tunnel
// Rustboro to Verdanturf. Included by rogue_gen_dungeon.h, after the
// legends' planning.
//
// The dungeon's last floor before any legend's chamber (RogueGen_ExitFloor)
// gets a way out (RFLOOR_EXIT) in a chamber of its own, joined to the rest
// of the floor by one narrow passage where the barrier stands, so each side
// can be explored up to it. A legend's chamber stays a dead end below. The
// exit place, a road place three or more gyms on and at most three cells
// away on the grid, gets a mouth in a rock face (RWARP_DUNGEON_2).
//
// The barrier is a cracked rock or a boulder (the badge that gives ROCK
// SMASH or STRENGTH in this region), or someone standing in the way: a
// worker who lets those with a gym leader's badge by, or a villain grunt
// until the hideout is cleared. A barrier is only taken if the exact gating
// check (GatingHolds, with Reach following the tunnel) holds: whatever order
// the gyms are won in, the tunnel never opens a way round a block, and no
// side place opens sooner than before. And it opens late enough for the
// far end: with the gyms won in order along the road, the badge that opens
// it is won at most one stretch before the far end's (LOOP_AHEAD), so a
// team with two badges never comes out among the fourth gym's trainers.
//
// It is planned last, on its own random stream, so the rest of the region
// stays as it was; only the two ends' maps and the exit floor change.

#define TUNNEL_REACH 3 // grid cells between the two ends, at most
#define TUNNEL_GAP   3 // gyms between the two ends, at least
#define TUNNEL_SAVES 4 // places saved on the walk between the ends, at least

// What draws a dungeon's exit floor, and so how its way out is made.
// Floors drawn by GenCave (caves, lairs and any kind that uses it) take a
// chamber with a ladder up; tower and hideout rooms take none. A new kind
// whose floors GenCave draws works with no change here.
enum { TUNNEL_NONE, TUNNEL_CAVE };

static u8 TunnelStyle(const struct RogueRegion *r, u8 dungeon)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon];
    if (d->required || d->exit != 0xFF || IsRoomDungeon(d) || d->kind == RDUN_FOREST
     || RogueGen_FloorRoom(r, dungeon, RogueGen_ExitFloor(r, dungeon)) != 0xFF)
        return TUNNEL_NONE;
    return TUNNEL_CAVE;
}

u8 RogueGen_ExitFloor(const struct RogueRegion *r, u8 dungeon)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon % ROGUE_MAX_DUNGEONS];
    u8 f = DungeonGenFloors(d);
    return f < 1 ? 1 : f;
}

bool8 RogueGen_TunnelMouthAt(const struct RogueRegion *r, u8 place)
{
    return r->tunnel < r->nDungeons && r->dungeons[r->tunnel].exit == place;
}

u8 RogueGen_TunnelHidden(const struct RogueRegion *r)
{
    u32 base;
    if (r->tunnel >= r->nDungeons)
        return ROGUE_MAX_HIDDEN;
    base = RogueGen_HiddenBase(r, r->nPlaces);
    return base < ROGUE_MAX_HIDDEN ? base : ROGUE_MAX_HIDDEN;
}

u8 RogueGen_StretchLevel(const struct RogueRegion *r, u8 badges)
{
    int i;
    u8 last = 0;
    // The main road is the first places, in order.
    for (i = 0; i < r->nPlaces && !r->places[i].side; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        if (p->kind == RPLACE_TOWN)
            continue;
        last = p->level;
        if (p->badges >= badges)
            return p->level;
    }
    return last;
}

// A road place whose rock face can take the tunnel's far mouth: a route
// (not the sea or Victory Road's) or a town that could take a lair's door,
// with no dungeon of its own and room along its top.
static bool8 TunnelEndOk(const struct RogueRegion *r, u8 place)
{
    const struct RoguePlace *p = &r->places[place];
    int x0, w;
    if (p->side || p->mouth != 0xFF || place == r->start || RogueGen_DungeonAt(r, place) >= 0)
        return FALSE;
    if (p->kind == RPLACE_TOWN)
        return LairTownOk(r, place);
    if (p->kind != RPLACE_ROUTE || p->type == RROUTE_SEA || p->type == RROUTE_MOUNTAIN || p->seaSide == RDIR_NORTH)
        return FALSE;
    // (No rock face on a bridge route's map, v1.1 bridge.)
    if (p->type == RROUTE_BRIDGE)
        return FALSE;
    // Not a route the woods cut in two, nor one whose block stands at its
    // top (the lane would run into the rock).
    if (p->obstacle && p->guardDir == RDIR_NORTH)
        return FALSE;
    return DungeonMesaSpan(r, p, &x0, &w);
}

// Places walked from a to b with every key, not through the tunnel.
static int TunnelWalk(const struct RogueRegion *r, u8 a, u8 b)
{
    u8 dist[ROGUE_MAX_PLACES], queue[ROGUE_MAX_PLACES];
    int head = 0, tail = 0, k;
    memset(dist, 0xFF, sizeof(dist));
    dist[a] = 0;
    queue[tail++] = a;
    while (head < tail)
    {
        int u = queue[head++];
        const struct RoguePlace *p = &r->places[u];
        u8 next[ROGUE_MAX_EXITS + 3];
        int n = 0;
        for (k = 0; k < p->nLinks; k++)
            next[n++] = p->links[k].target;
        if (p->mouth != 0xFF)
            next[n++] = p->mouth;
        if (p->kind == RPLACE_CAVE && p->caveIn != 0xFF)
            next[n++] = p->caveIn;
        if (p->kind == RPLACE_CAVE && p->caveOut != 0xFF)
            next[n++] = p->caveOut;
        for (k = 0; k < n; k++)
        {
            if (dist[next[k]] != 0xFF)
                continue;
            dist[next[k]] = dist[u] + 1;
            queue[tail++] = next[k];
        }
    }
    return dist[b] == 0xFF ? 0 : dist[b];
}

// The keys a player holds with the first gyms along the road won: the
// villains' too once the hideout's town is reached.
static u8 TunnelHeldKeys(const struct RogueRegion *r, u8 won, u8 *seen)
{
    u8 keys = RogueGen_KeysWon(r, won);
    s32 h = RogueGen_Hideout(r);
    Reach(r, keys, seen, NULL);
    if (h >= 0 && seen[r->dungeons[h].host])
    {
        keys |= ROGUE_KEY_VILLAIN;
        Reach(r, keys, seen, NULL);
    }
    return keys;
}

u8 RogueGen_TunnelOpensAt(const struct RogueRegion *r)
{
    u8 seen[ROGUE_MAX_PLACES], g;
    const struct RogueDungeon *d;
    if (r->tunnel >= r->nDungeons)
        return 0xFF;
    d = &r->dungeons[r->tunnel];
    for (g = 0; g <= ROGUE_NUM_GYMS; g++)
    {
        u8 keys = TunnelHeldKeys(r, g ? (u8)((1 << g) - 1) : 0, seen);
        if ((keys & TunnelKeys(r)) == TunnelKeys(r) && (seen[d->host] || seen[d->exit]))
            return g;
    }
    return 0xFF;
}

// When each place opens if the gyms are won in order along the road (as
// Stages), counting the villains' key once their hideout's town is reached.
static void TunnelStages(const struct RogueRegion *r, u8 *stage)
{
    u8 seen[ROGUE_MAX_PLACES];
    int g, i;
    memset(stage, 0xFF, r->nPlaces);
    for (g = 0; g <= ROGUE_NUM_GYMS; g++)
    {
        TunnelHeldKeys(r, g ? (u8)((1 << g) - 1) : 0, seen);
        for (i = 0; i < r->nPlaces; i++)
            if (seen[i] && stage[i] == 0xFF)
                stage[i] = g;
    }
}

// TRUE if the region holds with the tunnel as set: the exact gating check,
// and every place opens at the stage it did without it (so no side place
// is reached sooner, and levels stay as planned).
static bool8 TunnelHolds(struct RogueRegion *r, const u8 *spine, const u8 *stageBefore)
{
    u8 stage[ROGUE_MAX_PLACES];
    if (!GatingHolds(r, spine))
        return FALSE;
    TunnelStages(r, stage);
    return memcmp(stage, stageBefore, r->nPlaces) == 0;
}

static void PlanTunnel(struct RogueRegion *r)
{
    u32 rng = RogueRng_Hash(r->seed, 11950);
    u8 spine[SPINE_MAX], stage[ROGUE_MAX_PLACES];
    int i, b, best = -1000, bestD = -1, bestB = -1, bestGate = 0;
    s32 hideout = RogueGen_Hideout(r);

    r->tunnel = 0xFF;
    for (i = 0; i < SPINE_LEN; i++)
        spine[i] = i;
    // (The blocks are still at hand from laying the road: if not, no tunnel.)
    if (!GatingHolds(r, spine))
        return;
    TunnelStages(r, stage);
    for (i = 0; i < r->nDungeons; i++)
    {
        struct RogueDungeon *d = &r->dungeons[i];
        const struct RoguePlace *host = &r->places[d->host];
        int kindScore;
        if (TunnelStyle(r, i) == TUNNEL_NONE)
            continue;
        // A lair first (its legend's dungeon becomes the tunnel), then a
        // side cave, then any other kind.
        kindScore = d->kind == RDUN_LAIR ? 20 : d->kind == RDUN_CAVE ? 10 : 0;
        for (b = 0; b < SPINE_LEN; b++)
        {
            const struct RoguePlace *pb = &r->places[b];
            int far = Abs(pb->gridX - host->gridX) + Abs(pb->gridY - host->gridY), saved, k;
            if (far > TUNNEL_REACH || pb->badges < d->badges + TUNNEL_GAP)
                continue;
            if (!TunnelEndOk(r, b))
                continue;
            // Worth taking: it saves a real walk.
            saved = TunnelWalk(r, d->host, b) - 1;
            if (saved < TUNNEL_SAVES)
                continue;
            // Each barrier that suits the floor: a cracked rock or a
            // boulder, a leader's badge, the villains' grunts.
            for (k = 0; k < 3 + ROGUE_NUM_GYMS; k++)
            {
                u8 gate, opens;
                int score;
                if (k == 0)
                    gate = ROGUE_GATE(RGATE_FIELD, ROBST_ROCK_SMASH);
                else if (k == 1)
                    gate = ROGUE_GATE(RGATE_FIELD, ROBST_STRENGTH);
                else if (k == 2)
                    gate = ROGUE_GATE(RGATE_VILLAIN, 0);
                else
                    gate = ROGUE_GATE(RGATE_GIFT, k - 3);
                if (k < 2 && KeyGym(r, ROGUE_GATE_SRC(gate)) < 0)
                    continue;
                if (k == 2 && hideout < 0)
                    continue;
                d->exit = b;
                r->tunnel = i;
                r->tunnelGate = gate;
                opens = RogueGen_TunnelOpensAt(r);
                // By five badges, before Fly, the walk it saves matters;
                // a field move first, as Rusturf's rocks.
                score = (opens <= 5 ? 100 : 0) - 4 * opens + (k < 2 ? 30 : 0) + kindScore + 2 * (saved > 15 ? 15 : saved)
                      + RogueRng_Range(&rng, 6);
                // The far end at most LOOP_AHEAD stretches past the
                // badges it opens with, won in order.
                if (opens <= ROGUE_NUM_GYMS && pb->badges <= opens + LOOP_AHEAD && score > best
                 && TunnelHolds(r, spine, stage))
                {
                    best = score;
                    bestD = i;
                    bestB = b;
                    bestGate = gate;
                }
                d->exit = 0xFF;
                r->tunnel = 0xFF;
            }
        }
    }
    if (bestD < 0)
        return;
    r->tunnel = bestD;
    r->tunnelGate = bestGate;
    r->dungeons[bestD].exit = bestB;
}

// ---------------------------------------------------------------------------
// The exit floor

// TRUE if unit (x, y) is a passage one unit wide: floor on two opposite
// sides and rock on the other two. *vert: it runs north and south.
static bool8 TunnelPassage(struct Gen *gen, int x, int y, bool8 *vert)
{
    bool8 v = Cell(gen, x, y - 1) == C_ROAD && Cell(gen, x, y + 1) == C_ROAD
           && Cell(gen, x - 1, y) == C_TREE && Cell(gen, x + 1, y) == C_TREE;
    bool8 h = Cell(gen, x - 1, y) == C_ROAD && Cell(gen, x + 1, y) == C_ROAD
           && Cell(gen, x, y - 1) == C_TREE && Cell(gen, x, y + 1) == C_TREE;
    if (Cell(gen, x, y) != C_ROAD || v == h)
        return FALSE;
    *vert = v;
    return TRUE;
}

static bool8 InRect(const struct Rect *r, int x, int y)
{
    return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

// The tunnel's way out: a chamber in the emptiest part of the map, joined to
// the nearest chamber by a passage one unit wide (dug before the side
// chambers, so it gets the room). FALSE if there is no room.
static bool8 CarveTunnelEnd(struct Gen *gen, const struct Rect *rooms, int nRooms, struct Rect *t)
{
    int k, j, bx = 0, by = 0, bestFar = -1, dd;
    const struct Rect *from = &rooms[0];
    for (k = 0; k < 16; k++)
    {
        int tx = 2 + RAND(gen->cw - 4), ty = 2 + RAND(gen->ch - 4), far = 99;
        for (j = 0; j < gen->cw * gen->ch; j++)
        {
            YIELD();
            if (gen->g[j] != C_ROAD)
                continue;
            dd = Abs(j % gen->cw - tx);
            if (Abs(j / gen->cw - ty) > dd)
                dd = Abs(j / gen->cw - ty);
            if (dd < far)
                far = dd;
        }
        if (far > bestFar)
        {
            bestFar = far;
            bx = tx;
            by = ty;
        }
    }
    // Far enough from everything for a passage of two units or more.
    if (bestFar < 3)
        return FALSE;
    CaveRoomAt(gen, t, bx, by, 2 + RAND(2), 2);
    for (j = 0, dd = 999; j < nRooms; j++)
    {
        int e = Abs(rooms[j].x + rooms[j].w / 2 - bx) + Abs(rooms[j].y + rooms[j].h / 2 - by);
        if (e < dd)
        {
            dd = e;
            from = &rooms[j];
        }
    }
    CarveChamber(gen, t, CHAMBER_PLAIN);
    CarveTunnel(gen, t->x + t->w / 2, t->y + t->h / 2, from->x + from->w / 2, from->y + from->h / 2, FALSE);
    return TRUE;
}

// The unit of the tunnel's passage where the barrier stands: one that alone
// joins the way out's chamber t to the rest, with the passage on both sides
// of it clear, so a boulder can be pushed either way. FALSE if none (the
// map is made again).
static bool8 FindTunnelChoke(struct Gen *gen, const struct Rect *rooms, int nRooms, const struct Rect *t, int *cx, int *cy)
{
    u16 cand[24];
    int j, x, y, n = 0;
    for (y = 2; y < gen->ch - 2; y++)
    {
        YIELD();
        for (x = 2; x < gen->cw - 2 && n < 24; x++)
        {
            bool8 vert, cut = TRUE;
            int ax, ay, bx2, by2;
            if (!TunnelPassage(gen, x, y, &vert) || InRect(t, x, y))
                continue;
            for (j = 0; j < nRooms && cut; j++)
                cut = !InRect(&rooms[j], x, y);
            if (!cut)
                continue;
            ax = vert ? x : x - 1;
            ay = vert ? y - 1 : y;
            bx2 = vert ? x : x + 1;
            by2 = vert ? y + 1 : y;
            // Clear on both sides, for a boulder pushed along.
            if (CellTaken(gen, x, y) || CellTaken(gen, ax, ay) || CellTaken(gen, bx2, by2)
             || Cell(gen, ax, ay) != C_ROAD || Cell(gen, bx2, by2) != C_ROAD)
                continue;
            SetCell(gen, x, y, C_TREE);
            FloodReachable(gen, rooms[0].x + rooms[0].w / 2, rooms[0].y + rooms[0].h / 2);
            cut = !Reached(gen, t->x + t->w / 2, t->y + t->h / 2);
            for (j = 0; j < nRooms && cut; j++)
                cut = Reached(gen, rooms[j].x + rooms[j].w / 2, rooms[j].y + rooms[j].h / 2);
            SetCell(gen, x, y, C_ROAD);
            if (cut)
                cand[n++] = PACK(x, y);
        }
    }
    if (n == 0)
        return FALSE;
    j = RAND(n);
    *cx = PX(cand[j]);
    *cy = PY(cand[j]);
    return TRUE;
}

// The barrier across the passage unit (cx, cy): a pair of cracked rocks or
// boulders side by side, as on Victory Road (one smashed or pushed opens
// it), or someone in the way beside a rock (RenderTunnelBarrier).
static void PlaceTunnelBarrier(struct Gen *gen, int cx, int cy)
{
    const struct RogueRegion *r = gen->r;
    u8 src = ROGUE_GATE_SRC(r->tunnelGate), kind, i;
    bool8 vert;
    int k = RAND(2);
    TunnelPassage(gen, cx, cy, &vert);
    if (ROGUE_GATE_KIND(r->tunnelGate) == RGATE_FIELD)
        kind = src == ROBST_STRENGTH ? ROBJ_BOULDER : src == ROBST_CUT ? ROBJ_CUT_TREE : ROBJ_ROCK;
    else
        kind = ROBJ_TUNNEL_GUARD;
    for (i = 0; i < 2; i++)
    {
        int fx = vert ? cx * 2 + i : cx * 2 + k, fy = vert ? cy * 2 + k : cy * 2 + i;
        if (kind != ROBJ_TUNNEL_GUARD)
            AddObj(gen, kind, fx, fy, RDIR_SOUTH, i, 0);
        else if (i == 0)
            AddObj(gen, kind, fx, fy, vert ? RDIR_SOUTH : RDIR_EAST,
                   ROGUE_GATE_KIND(r->tunnelGate) == RGATE_VILLAIN, 0);
        MarkOccupied(gen, fx, fy);
    }
    // The passage either side stays clear.
    for (i = 0; i < 4; i++)
    {
        MarkOccupied(gen, (vert ? cx : cx - 1) * 2 + (i & 1), (vert ? cy - 1 : cy) * 2 + (i >> 1));
        MarkOccupied(gen, (vert ? cx : cx + 1) * 2 + (i & 1), (vert ? cy + 1 : cy) * 2 + (i >> 1));
    }
}

// Beside someone standing in the passage, a rock fills the rest of it.
static void RenderTunnelBarrier(struct Gen *gen)
{
    int i;
    for (i = 0; i < gen->m->nObjs; i++)
    {
        const struct RogueObj *o = &gen->m->objs[i];
        bool8 vert;
        if (o->kind != ROBJ_TUNNEL_GUARD || !TunnelPassage(gen, o->x >> 1, o->y >> 1, &vert))
            continue;
        Put(gen, vert ? o->x ^ 1 : o->x, vert ? o->y : o->y ^ 1, StyledSolidRock(gen, B_CAVE_ROCK));
    }
}
