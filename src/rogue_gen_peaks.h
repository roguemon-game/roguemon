// Peaks (v1.1): a misty mountainside below a tower's summit, and a tower in
// a sandstorm desert. Included by rogue_gen_dungeon.h, after the rooms.
//
// The misty mountainside, as Mt. Pyre's exterior: most towers in a town
// (RDUN_TOWER) are climbed indoors to Mt. Pyre's summit, out in the mist.
// In most of those with four floors or more, the floor below the summit is
// out on the mountain instead (RogueGen_PyreSlopeFloor): the stairs from
// the floor below come out of a door in the rock at the bottom, and a
// winding path climbs terraces of rock by stone steps past rows of graves
// and patches of tall grass, in fog (Mt. Pyre's own tilesets, General and
// Facility, with its music and fog), up to a second door in the rock at the
// top, the way on to the summit (GenPyreSlope). The floor keeps what the
// room it replaces held: two trainers and a find, and any extra the plan
// gave it (a choice row, the camp, someone lost).
//
// The desert tower, as the Mirage Tower on Route 111 (RDUN_MIRAGE): in a
// region with a tower and a desert in a sandstorm, the tower may stand out
// in the sand of that desert instead of in its town (PlanMirageTower, after
// every other dungeon is planned, so nothing else moves). The route is drawn
// with Mauville's tileset, as Route 111 is, so the tower is Hoenn's own,
// standing on the sand, its door at the foot (PlaceMirageTower). Inside are
// the Mirage Tower's four floors, climbed by their ladders, their cracked
// floor kept as sand pits (a fall drops the player to the floor below,
// near its ladder back up: Rogue_SetFallWarp), a guardian of the desert on
// the raised floor at the top and the tower's two fossils before it, one
// to take, as in Emerald (the choice row; rogue.c's FossilChoice).

// Of the towers that can have the mountainside, how many do; of the
// regions with a tower and a sandstorm desert, how many move the tower
// out into the sand.
#define PEAKS_SLOPE_SHARE  85
#define PEAKS_MIRAGE_SHARE 80
// The mountainside's size, in blocks: tall, as Mt. Pyre's exterior is.
#define PYRE_SLOPE_W 28
#define PYRE_SLOPE_H 56

static bool8 IsMirage(const struct RogueDungeon *d)
{
    return d->kind == RDUN_MIRAGE;
}

u8 RogueGen_PyreSlopeFloor(const struct RogueRegion *r, u8 dungeon)
{
    const struct RogueDungeon *d;
    if (dungeon >= r->nDungeons)
        return 0;
    d = &r->dungeons[dungeon];
    // (RUN 0's graveyard tower stays as it is.)
    if (d->kind != RDUN_TOWER || d->nFloors < 4 || RogueGen_IsRun0(r) || r->places[d->host].kind != RPLACE_TOWN)
        return 0;
    if (RogueRng_Hash(r->seed, 52400 + dungeon) % 100 >= PEAKS_SLOPE_SHARE)
        return 0;
    return d->nFloors - 1;
}

s32 RogueGen_MirageDungeon(const struct RogueRegion *r)
{
    int i;
    for (i = 0; i < r->nDungeons; i++)
        if (IsMirage(&r->dungeons[i]))
            return i;
    return -1;
}

// A desert that can take the tower: sand in a sandstorm (not ash, not the
// volcano's foot), with no cave or other dungeon on it, a badge on.
static bool8 MirageDesertOk(const struct RogueRegion *r, u8 place)
{
    const struct RoguePlace *p = &r->places[place];
    if (p->kind != RPLACE_ROUTE || p->type != RROUTE_DESERT || p->theme == RTHEME_ASH || p->theme == RTHEME_SPRING)
        return FALSE;
    if (p->mouth != 0xFF || RogueGen_DungeonAt(r, place) >= 0 || VolcanoMountainAt(r, place))
        return FALSE;
    if (p->badges < 1 || RogueGen_RouteWeather(r, place) != RWEATHER_SAND)
        return FALSE;
    return TRUE;
}

// The region's tower moves out into a sandstorm desert, on a stream of its
// own: the dungeon keeps its slot, so no other dungeon, trainer or item
// moves but the tower's own.
static void PlanMirageTower(struct RogueRegion *r)
{
    u32 rng = RogueRng_Hash(r->seed, 52500), total = 0, pick;
    u8 weight[ROGUE_MAX_PLACES];
    s32 tower = -1;
    int i;
    struct RogueDungeon *d;

    if (RogueGen_IsRun0(r))
        return;
    for (i = 0; i < r->nDungeons; i++)
        if (r->dungeons[i].kind == RDUN_TOWER && !r->dungeons[i].legend)
            tower = i;
    if (tower < 0 || RogueRng_Range(&rng, 100) >= PEAKS_MIRAGE_SHARE)
        return;
    for (i = 0; i < r->nPlaces; i++)
    {
        // A desert on the main road is likelier: the player walks it.
        weight[i] = MirageDesertOk(r, i) ? (r->places[i].side ? 1 : 2) : 0;
        total += weight[i];
    }
    if (total == 0)
        return;
    pick = RogueRng_Range(&rng, total);
    for (i = 0; i < r->nPlaces; i++)
    {
        if (pick < weight[i])
            break;
        pick -= weight[i];
    }
    d = &r->dungeons[tower];
    // (What was planned round the town as if the tower stood there, the
    // town signatures, still is: RogueGen_SigDungeonAt.)
    r->places[d->host].dark = ROGUE_TOWER_MOVED;
    d->kind = RDUN_MIRAGE;
    d->host = i;
    d->exit = 0xFF;
    d->nFloors = 4;
    d->boss = RBOSS_GUARDIAN;
    d->required = FALSE;
    // As AddDungeon: a step up from its host.
    d->level = r->places[i].level + (r->places[i].badges <= 1 ? 0 : 2);
    d->badges = r->places[i].badges;
    // Route 111's tileset, with the tower on its sand.
    r->places[i].theme = RTHEME_VERDANT;
}

// ---------------------------------------------------------------------------
// The tower on the desert route

// Route 111's Mirage Tower (Mauville's tileset), standing on the sand: solid
// but for its door in the middle of the bottom row.
#define MIRAGE_TOWER_W 3
#define MIRAGE_TOWER_H 6
static const u16 sMirageTowerStamp[MIRAGE_TOWER_W * MIRAGE_TOWER_H] = {
    0x07D8, 0x07D9, 0x07DA,
    0x07E0, 0x07E1, 0x07E2,
    0x07E8, 0x07E9, 0x07EA,
    0x07F0, 0x07F1, 0x07F2,
    0x07DB, 0x07DC, 0x07DD,
    0x07E3, 0x03E4, 0x07E5,
};

static s32 MirageOf(struct Gen *gen)
{
    s32 d;
    if (sGenFloor || gen->p->kind != RPLACE_ROUTE)
        return -1;
    d = RogueGen_DungeonAt(gen->r, (u8)(gen->p - gen->r->places));
    return (d >= 0 && IsMirage(&gen->r->dungeons[d])) ? d : -1;
}

// The tower out on the sand beside the way, its door opening onto it: the
// tower's two units by three over a unit of the way, with open ground all
// round (the sand runs up to it, as round Route 111's boulders), out of
// view of a seam (its tiles are Mauville's). A map that keeps failing
// clears the rock round the lot, then under it (more sand never shuts a
// way). Its top left block goes in springX and springY (a town's spring
// rock; a route has none).
static bool8 MirageLotOk(struct Gen *gen, int bx, int by, u8 relax)
{
    int x, y;
    for (y = by - 1; y <= by + 3; y++)
    {
        for (x = bx - 1; x <= bx + 2; x++)
        {
            u8 c = Cell(gen, x, y);
            bool8 lot = x >= bx && x < bx + 2 && y >= by && y < by + 3;
            bool8 open = c == C_OPEN || c == C_ROAD;
            if (IsEdge(gen, x, y) || IsOccupied(gen, x * 2, y * 2) || IsOccupied(gen, x * 2 + 1, y * 2)
             || IsOccupied(gen, x * 2, y * 2 + 1) || IsOccupied(gen, x * 2 + 1, y * 2 + 1))
                return FALSE;
            if (lot && c == C_ROAD)
                return FALSE;
            // Rock round it (relax 1) or under it too (relax 2) is cleared.
            if (!open && !(c == C_TREE && !BitU(gen->keep, gen, x, y) && (relax >= 2 || (relax >= 1 && !lot))))
                return FALSE;
        }
    }
    return TRUE;
}

static bool8 PlaceMirageTower(struct Gen *gen)
{
    int tries;
    u8 relax = gen->attempt < 10 ? 1 : 2;
    if (gen->nPath < 8 || gen->nDecor >= MAX_DECOR)
        return FALSE;
    for (tries = 0; tries < 160; tries++)
    {
        int pi = 3 + RAND(gen->nPath - 6);
        int px = gen->path[pi] % gen->cw, py = gen->path[pi] / gen->cw;
        int bx = px, by = py - 3, x, y;
        YIELD();
        if (bx < 2 || by < 2 || bx + 2 > gen->cw - 2 || !MirageLotOk(gen, bx, by, relax))
            continue;
        if (InRouteSeam(gen, bx * 2, by * 2) || InRouteSeam(gen, bx * 2 + 3, by * 2 + 5)
         || InRouteSeam(gen, bx * 2 + 3, by * 2) || InRouteSeam(gen, bx * 2, by * 2 + 5))
            continue;
        if (!AddDecor(gen, sMirageTowerStamp, bx * 2, by * 2, MIRAGE_TOWER_W, MIRAGE_TOWER_H))
            return FALSE;
        for (y = by - 1; y <= by + 3; y++)
            for (x = bx - 1; x <= bx + 2; x++)
            {
                if (x >= bx && x < bx + 2 && y >= by && y < by + 3)
                    SetCell(gen, x, y, C_DECOR);
                else if (Cell(gen, x, y) == C_TREE)
                    SetCell(gen, x, y, C_OPEN);
            }
        // Nobody stands at its door, nor in its lot's last column of sand.
        for (y = 0; y < MIRAGE_TOWER_H; y++)
            MarkOccupied(gen, bx * 2 + 3, by * 2 + y);
        MarkOccupied(gen, bx * 2 + 1, by * 2 + 6);
        AddWarp(gen, bx * 2 + 1, by * 2 + 5, RWARP_DUNGEON);
        gen->springX = bx * 2;
        gen->springY = by * 2;
        return TRUE;
    }
    return FALSE;
}

// After the ground is painted: no grass round the tower, and its lot's last
// column (beside the tower's three) is the sand round it.
static void MirageGround(struct Gen *gen, bool8 painted)
{
    int x, y;
    if (MirageOf(gen) < 0 || gen->springX == 0)
        return;
    for (y = gen->springY - 2; y < gen->springY + MIRAGE_TOWER_H + 2; y++)
        for (x = gen->springX - 2; x < gen->springX + 4 + 2; x++)
        {
            bool8 lot = x >= gen->springX && x < gen->springX + 4 && y >= gen->springY && y < gen->springY + MIRAGE_TOWER_H;
            if (!InMap(gen, x, y))
                continue;
            if (!painted && (Cover(gen, x, y) == V_TALL || Cover(gen, x, y) == V_FLOWER))
                SetCover(gen, x, y, V_NONE);
            if (painted && lot)
                SetCover(gen, x, y, V_GROUND);
        }
}

// ---------------------------------------------------------------------------
// The misty mountainside (Mt. Pyre's exterior)
//
// Mt. Pyre's exterior is a stack of narrow terraces of grass, each three to
// six blocks deep, between tall faces of rock (a lip, two or three rows of
// face, a foot) running right across, climbed by long stone stairs cut
// into the faces, from side to side as one climbs, with rows of graves on
// the terraces. The floor is built the same way from bands that
// tools/roguemon/gen_pyre_slope.py works out from Mt. Pyre's own maps (so
// every pair and 2x2 window, and every join between bands, is one Mt. Pyre
// has): the top (the door on to the summit in the top face), the walls
// with their stairs, the bottom wall with the door in from the floor below
// at one end and the stairs up at the other, and the bottom. Between them,
// terrace rows; on the terraces, the graves and the tall grass.

#include "data/rogue_pyre_slope.h"

static bool8 PlaceFloorExtras(struct Gen *gen, int bossX, int bossY, const struct Rect *side, int nSide,
                              const struct Rect *main, int nMain, bool8 (*open)(struct Gen *, int, int));

// The mountainside's place: a mountain pass, drawn with General's rock and
// grass (Mt. Pyre's exterior is: only its graves are Facility's).
static void PyreSlopePlace(struct RoguePlace *out)
{
    out->kind = RPLACE_ROUTE;
    out->type = RROUTE_HILLS;
    out->theme = RTHEME_MEADOW;
    out->w = PYRE_SLOPE_W;
    out->h = PYRE_SLOPE_H;
    out->dark = 0;
    out->number = 0; // (no route's own rules: route 1's one trainer, route 2's levels)
}

#ifdef ROGUE_DEBUG
#define PYRE_FAIL() do { fprintf(stderr, "pyre fail line %d\n", __LINE__); return FALSE; } while (0)
#else
#define PYRE_FAIL() return FALSE
#endif

static const struct PyreBand *PyreBandOf(u8 kind, u8 a, u8 b, u8 c, u8 height)
{
    u32 i;
    for (i = 0; i < sizeof(sPyreBands) / sizeof(sPyreBands[0]); i++)
        if (sPyreBands[i].kind == kind && sPyreBands[i].a == a && sPyreBands[i].b == b && sPyreBands[i].c == c
         && sPyreBands[i].height == height)
            return &sPyreBands[i];
    return NULL;
}

static u16 PyreAt(struct Gen *gen, int x, int y)
{
    return gen->m->blocks[y * gen->m->w + x];
}

static bool8 PyreGrassAt(struct Gen *gen, int x, int y)
{
    return InMap(gen, x, y) && PyreAt(gen, x, y) == PYRE_GRASS;
}

// TRUE by a flight of stairs or a door (dx -2 to 3, dy -2 to 2 from one):
// where the way goes up, through and out, kept clear.
static bool8 PyreByWay(struct Gen *gen, int x, int y)
{
    int dx, dy;
    for (dy = -2; dy <= 2; dy++)
        for (dx = -3; dx <= 2; dx++)
            if (InMap(gen, x + dx, y + dy))
            {
                u16 t = PyreAt(gen, x + dx, y + dy) & 0x3FF;
                if (t == 0x0AF || t == 0x0CF || t == 0x0A7)
                    return TRUE;
            }
    return FALSE;
}

// Ground on the mountainside one can stand on: grass, tall grass or path,
// off the way up.
static bool8 PyreOpenF(struct Gen *gen, int fx, int fy)
{
    u16 b;
    if (!InMap(gen, fx, fy))
        return FALSE;
    b = PyreAt(gen, fx, fy);
    return (b == PYRE_GRASS || b == PYRE_TALL) && !PyreByWay(gen, fx, fy);
}

// A row of graves along terrace row y, as Mt. Pyre's: on every other
// block, off the way, all grey, all orange or the two mixed.
static int PyreGraveRow(struct Gen *gen, int y, u8 *kinds)
{
    int n = 3 + RAND(4), x0, x, k, placed = 0;
    u8 mode = RAND(3);
    x0 = 3 + RAND(PYRE_W - 6 - 2 * (n - 1));
    for (k = 0; k < n; k++)
    {
        x = x0 + 2 * k;
        if (x > PYRE_W - 4 || PyreByWay(gen, x, y) || !PyreGrassAt(gen, x, y - 1) || !PyreGrassAt(gen, x, y + 1)
         || !PyreGrassAt(gen, x - 1, y) || !PyreGrassAt(gen, x + 1, y))
            continue;
        {
            bool8 orange = mode == 2 ? RAND(2) : mode;
            Put(gen, x, y, orange ? PYRE_GRAVE_ORANGE : PYRE_GRAVE_GREY);
            *kinds |= orange ? 2 : 1;
            placed++;
        }
    }
    return placed;
}

// A patch of tall grass round (cx, cy): a few rows, grass all round it.
static void PyreTallPatch(struct Gen *gen, int cx, int cy)
{
    int w = 3 + RAND(4), h = 2 + RAND(2), x, y, dx, dy;
    for (y = cy; y < cy + h; y++)
        for (x = cx + (y == cy ? RAND(2) : 0); x < cx + w - (y == cy + h - 1 ? RAND(2) : 0); x++)
        {
            bool8 ok = InMap(gen, x, y) && PyreAt(gen, x, y) == PYRE_GRASS && !PyreByWay(gen, x, y);
            for (dy = -1; dy <= 1 && ok; dy++)
                for (dx = -1; dx <= 1 && ok; dx++)
                {
                    u16 b = InMap(gen, x + dx, y + dy) ? PyreAt(gen, x + dx, y + dy) : 0;
                    ok = b == PYRE_GRASS || b == PYRE_TALL;
                }
            if (ok)
            {
                Put(gen, x, y, PYRE_TALL);
                gen->tallCount++;
            }
        }
}

// Rows of blocks (indexes into sPyreBlocks), n of them, from rows on (the
// same row n times if stride is 0).
static void PyreRows(struct Gen *gen, int *y, const u8 *rows, int n, int stride)
{
    int k, x;
    for (k = 0; k < n && *y < gen->m->h; k++, (*y)++)
        for (x = 0; x < PYRE_W; x++)
            Put(gen, x, *y, sPyreBlocks[rows[k * stride + x]]);
}

static void PyreBandRows(struct Gen *gen, int *y, const struct PyreBand *b)
{
    PyreRows(gen, y, &sPyreBandRows[b->start * PYRE_W], b->rows, PYRE_W);
}

static void PyreFillRows(struct Gen *gen, int *y, int n)
{
    int k, x;
    for (k = 0; k < n && *y < gen->m->h; k++, (*y)++)
        for (x = 0; x < PYRE_W; x++)
            Put(gen, x, *y, PYRE_FILL);
}

// A rock standing on a terrace (x, y its top left), as Mt. Pyre's: a
// pillar three or four rows tall (General's 068 06A, 070 072, 078 07A) or a
// small one (068 06A over 08B 08C), with grass all round it and off the
// way. FALSE if there is no room.
static bool8 PyreStandingRock(struct Gen *gen, int x, int y, int h)
{
    int dx, dy;
    for (dy = -1; dy <= h; dy++)
        for (dx = -1; dx <= 2; dx++)
            if (!PyreGrassAt(gen, x + dx, y + dy) || PyreByWay(gen, x + dx, y + dy) || IsOccupied(gen, x + dx, y + dy))
                return FALSE;
    for (dy = 0; dy < h; dy++)
    {
        bool8 top = dy == 0, bottom = dy == h - 1;
        Put(gen, x, y + dy, top ? PYRE_ROCK_TL : bottom ? (h == 2 ? PYRE_LUMP_L : PYRE_ROCK_BL) : PYRE_ROCK_L);
        Put(gen, x + 1, y + dy, top ? PYRE_ROCK_TR : bottom ? (h == 2 ? PYRE_LUMP_R : PYRE_ROCK_BR) : PYRE_ROCK_R);
    }
    return TRUE;
}

#define PYRE_WALLS_MAX 5

// (v1.1 joins) Worn path at the stairs' feet and before the doors, as Mt.
// Pyre has it (its exterior's rows 34-35 and 42-44): below a flight, a
// path four blocks wide (1E0 1D9 1D9 1E2) and its end (1E0 1E2) under the
// stairs, or just the end when the terrace is shallow; below a door, 1D8
// 1D9 1E2 over 1E0 1E2. Only on plain grass with grass either side.
static bool8 PyreGrassRun(struct Gen *gen, int x, int y, int n)
{
    int k;
    for (k = -1; k <= n; k++)
        if (!PyreGrassAt(gen, x + k, y))
            return FALSE;
    return TRUE;
}

static void PyreDoorPath(struct Gen *gen, int x, int y)
{
    if (PyreGrassRun(gen, x - 1, y + 1, 3) && PyreGrassRun(gen, x - 1, y + 2, 2) && PyreGrassRun(gen, x - 1, y + 3, 2)
     && (PyreAt(gen, x + 1, y) & 0x3FF) == 0x091)
    {
        Put(gen, x - 1, y + 1, 0x31D8);
        Put(gen, x, y + 1, 0x31D9);
        Put(gen, x + 1, y + 1, 0x31E2);
        Put(gen, x - 1, y + 2, 0x31E0);
        Put(gen, x, y + 2, 0x31E2);
    }
}

static void PyrePathFeet(struct Gen *gen, int doorX, int doorY, int topX, int topY)
{
    int x, y;
    for (y = 1; y < gen->m->h - 3; y++)
        for (x = 2; x < PYRE_W - 3; x++)
        {
            if ((PyreAt(gen, x, y) & 0x3FF) != 0x0AF || (PyreAt(gen, x + 1, y) & 0x3FF) != 0x0CF
             || (PyreAt(gen, x, y + 1) & 0x3FF) == 0x0AF)
                continue;
            if (PyreGrassRun(gen, x - 1, y + 1, 4) && PyreGrassRun(gen, x, y + 2, 2) && PyreGrassRun(gen, x, y + 3, 2)
             && (PyreAt(gen, x + 2, y) & 0x3FF) == 0x091) // (1E2 only ever under the foot)
            {
                Put(gen, x - 1, y + 1, 0x31E0);
                Put(gen, x, y + 1, 0x31D9);
                Put(gen, x + 1, y + 1, 0x31D9);
                Put(gen, x + 2, y + 1, 0x31E2);
                Put(gen, x, y + 2, 0x31E0);
                Put(gen, x + 1, y + 2, 0x31E2);
            }
            else if (PyreGrassRun(gen, x, y + 1, 2) && PyreGrassRun(gen, x, y + 2, 2))
            {
                Put(gen, x, y + 1, 0x31E0);
                Put(gen, x + 1, y + 1, 0x31E2);
            }
        }
    PyreDoorPath(gen, doorX, doorY);
    PyreDoorPath(gen, topX, topY);
}


static bool8 GenPyreSlope(struct Gen *gen)
{
    const struct RoguePlace *p = gen->p;
    const struct PyreBand *band[PYRE_WALLS_MAX + 2];
    u8 height[PYRE_WALLS_MAX], deep[PYRE_WALLS_MAX + 1], side[PYRE_WALLS_MAX], terraceY[PYRE_WALLS_MAX + 1];
    u8 width[PYRE_WALLS_MAX + 1];
    u8 kinds = 0;
    int n, i, k, y, total, fill, dx = 0, dy = 0, tx = 0, ty, fx, fy, tries, trainers = 0, items = 0, graves = 0;
    struct Rect all;

    if (gen->m->w != PYRE_W)
        PYRE_FAIL();
    gen->rocky = TRUE;
    Fill(gen, C_TREE);
    // How many walls (the bottom one with the door in), how tall each face
    // (three to five rows), how deep each terrace (three to six rows, the
    // rows by the faces counted): the floor's height, give or take a few
    // rows of rock at the top and bottom.
    n = 4 + RAND(2);
    total = PYRE_TOP_ROWS + PYRE_BOTTOM_ROWS;
    for (i = 0; i < n; i++)
    {
        height[i] = i == n - 1 ? 3 : 3 + RAND(3);
        total += height[i] + 2;
    }
    for (i = 0; i <= n; i++)
    {
        deep[i] = i == n ? 2 + RAND(3) : 1 + RAND(4);
        total += deep[i];
    }
    for (tries = 0; tries < 60 && total > PYRE_SLOPE_H; tries++)
    {
        k = RAND(2 * n + 1);
        if (k <= n && deep[k] > (k == n ? 2 : 1))
            deep[k]--, total--;
        else if (k > n && height[k - n - 1] > 3)
            height[k - n - 1]--, total--;
    }
    for (tries = 0; tries < 60 && total < PYRE_SLOPE_H - 3; tries++)
    {
        k = RAND(n + 1);
        if (deep[k] < 4)
            deep[k]++, total++;
    }
    if (total > PYRE_SLOPE_H)
        PYRE_FAIL();
    // Each terrace's width (the bottom one, the door's, the widest).
    for (i = 0; i <= n; i++)
        width[i] = i == n ? PYRE_BOTTOM_WIDTH : RAND(PYRE_WIDTHS);
    // The bottom wall's door at one side, its stairs at the other; each
    // wall above has its stairs at the other side from the one below.
    // (The pieces for a few of the combinations could not be made: then
    // the next.)
    side[n - 1] = RAND(2);
    for (i = n - 2; i >= 0; i--)
        side[i] = !side[i + 1];
    for (i = 0; i < n; i++)
    {
        u32 nStairs = sizeof(sPyreStairX[0]), nDoors = sizeof(sPyreDoorX[0]), first = RAND(nStairs * nDoors);
        band[i] = NULL;
        for (k = 0; k < (int)(nStairs * nDoors) && band[i] == NULL; k++)
        {
            u32 pick = (first + k) % (nStairs * nDoors);
            u8 sx = sPyreStairX[side[i]][pick % nStairs];
            if (i == n - 1)
            {
                dx = sPyreDoorX[!side[i]][pick / nStairs];
                band[i] = PyreBandOf(PYRE_BAND_DOOR, width[i], dx, sx, 3);
            }
            else
            {
                band[i] = PyreBandOf(PYRE_BAND_WALL, width[i], width[i + 1], sx, height[i]);
            }
        }
        if (band[i] == NULL)
            PYRE_FAIL();
    }
    // The door on to the summit, away from the top flight's side.
    band[n] = NULL;
    for (k = 0, i = RAND(3); k < 6 && band[n] == NULL; k++)
    {
        tx = sPyreTopDoorX[((side[0] ? 0 : 3) + (k < 3 ? (i + k) % 3 : 3 + (i + k) % 3)) % 6];
        band[n] = PyreBandOf(PYRE_BAND_TOP, width[0], tx, 0, 3);
    }
    band[n + 1] = PyreBandOf(PYRE_BAND_BOTTOM, PYRE_BOTTOM_WIDTH, 0, 0, 2);
    if (band[n] == NULL || band[n + 1] == NULL)
        PYRE_FAIL();

    // Draw it, top down.
    fill = PYRE_SLOPE_H - total;
    y = 0;
    PyreFillRows(gen, &y, fill - fill / 2);
    ty = y + 4;
    PyreBandRows(gen, &y, band[n]);
    for (i = 0; i <= n; i++)
    {
        terraceY[i] = y;
        PyreRows(gen, &y, sPyreTerrace[width[i]], deep[i], 0);
        if (i == n)
            break;
        if (i == n - 1)
            dy = y + 3;
        PyreBandRows(gen, &y, band[i]);
    }
    PyreBandRows(gen, &y, band[n + 1]);
    PyreFillRows(gen, &y, PYRE_SLOPE_H - y);
    if ((PyreAt(gen, dx, dy) & 0x3FF) != 0x0A7 || (PyreAt(gen, tx, ty) & 0x3FF) != 0x0A7)
        PYRE_FAIL();
    PyrePathFeet(gen, dx, dy, tx, ty); // (v1.1 joins)

    // Rows of graves on most terraces, on their middle rows (a row of
    // grass in front of each, and behind); grey and orange both.
    for (i = 0; i <= n; i++)
    {
        int rows = 0;
        if (i < n && RAND(100) < 15)
            continue;
        for (k = 0; k < deep[i] && rows < 2; k++)
        {
            int gy = terraceY[i] + k, got = 0, t;
            if (k > 0 && RAND(2))
                continue;
            for (t = 0; t < 4 && !got; t++)
                got = PyreGraveRow(gen, gy, &kinds);
            graves += got > 0;
            rows++;
            k++; // (a row of grass between rows)
        }
    }
    if (graves == 0)
        PYRE_FAIL();
    if (kinds != 3)
    {
        // Both kinds: one row the other colour.
        for (fy = 0; fy < PYRE_SLOPE_H; fy++)
            for (fx = 0; fx < PYRE_W; fx++)
                if (PyreAt(gen, fx, fy) == PYRE_GRAVE_GREY || PyreAt(gen, fx, fy) == PYRE_GRAVE_ORANGE)
                {
                    if (kinds == 1)
                        Put(gen, fx, fy, PYRE_GRAVE_ORANGE);
                    else
                        Put(gen, fx, fy, PYRE_GRAVE_GREY);
                    fy = PYRE_SLOPE_H;
                    break;
                }
    }
    // Rocks standing on the deeper terraces (the terrace rows by the faces
    // count), and a small one or two.
    for (tries = 0, k = 1 + RAND(3); tries < 40 && k > 0; tries++)
    {
        int h = 3 + RAND(2);
        i = RAND(n + 1);
        if (deep[i] < h)
            continue;
        if (PyreStandingRock(gen, 3 + RAND(PYRE_W - 7), terraceY[i] + RAND(deep[i] - h + 1), h))
            k--;
    }
    for (tries = 0, k = 1 + RAND(2); tries < 40 && k > 0; tries++)
    {
        i = RAND(n + 1);
        if (deep[i] >= 2 && PyreStandingRock(gen, 3 + RAND(PYRE_W - 7), terraceY[i] + RAND(deep[i] - 1), 2))
            k--;
    }
    // Tall grass in a few patches, not fields.
    for (tries = 0, k = 3 + RAND(3); tries < 50 && k > 0; tries++)
    {
        int before = gen->tallCount;
        i = RAND(n + 1);
        PyreTallPatch(gen, 3 + RAND(PYRE_W - 9), terraceY[i] + RAND(deep[i] + 1) - 1);
        if (gen->tallCount > before + 2)
            k--;
    }

    // Nothing stands at the doors, or in front.
    MarkOccupied(gen, dx, dy);
    MarkOccupied(gen, dx, dy + 1);
    MarkOccupied(gen, tx, ty);
    MarkOccupied(gen, tx, ty + 1);
    // The extras (a choice row, the camp, someone lost) out of the way.
    all.x = 1;
    all.y = 1;
    all.w = gen->cw - 2;
    all.h = gen->ch - 2;
    if (!PlaceFloorExtras(gen, -1, -1, NULL, 0, &all, 1, PyreOpenF))
        PYRE_FAIL();
    // The finds at the ends of the terraces, by the rock.
    for (tries = 0; tries < 80 && items < p->nItems; tries++)
    {
        i = RAND(n + 1);
        fx = tries < 40 ? (RAND(2) ? sPyreFlank[width[i]][0] : PYRE_W - sPyreFlank[width[i]][1] - 1) : 2 + RAND(PYRE_W - 4);
        fy = terraceY[i] + RAND(deep[i]);
        if (!PyreOpenF(gen, fx, fy) || IsOccupied(gen, fx, fy) || IsCrowded(gen, fx, fy) || SplitsGround(gen, fx, fy))
            continue;
        AddObj(gen, ROBJ_ITEM, fx, fy, RDIR_SOUTH, items++, 0);
    }
    // The trainers on the way up, from where they first stand.
    for (tries = 0; tries < 120 && trainers < p->nTrainers; tries++)
    {
        i = RAND(n + 1);
        fx = 2 + RAND(PYRE_W - 4);
        fy = terraceY[i] + RAND(deep[i]);
        if (!PyreOpenF(gen, fx, fy) || IsOccupied(gen, fx, fy) || SplitsGround(gen, fx, fy))
            continue;
        AddObj(gen, ROBJ_TRAINER, fx, fy, RDIR_SOUTH, trainers++, 3);
    }
    if (trainers < p->nTrainers || items < p->nItems)
        PYRE_FAIL();
    AddWarp(gen, dx, dy, RFLOOR_UP);
    AddWarp(gen, tx, ty, RFLOOR_DOWN);
    PlaceTrainersBetween(gen, dx, dy + 1, tx, ty + 1);
    // Nobody at a door or in front of one, nobody on the tall grass, and
    // everyone and both doors reached.
    for (i = 0; i < gen->m->nObjs; i++)
    {
        struct RogueObj *o = &gen->m->objs[i];
        if ((o->x == dx && (o->y == dy || o->y == dy + 1)) || (o->x == tx && (o->y == ty || o->y == ty + 1)))
            PYRE_FAIL();
        if (PyreAt(gen, o->x, o->y) == PYRE_TALL)
            Put(gen, o->x, o->y, PYRE_GRASS);
    }
    if (!TownReachable(gen))
        PYRE_FAIL();
    return TRUE;
}
