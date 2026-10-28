// ---------------------------------------------------------------------------
// Bridge routes (RROUTE_BRIDGE)
//
// A long raised road over open water, as Route 110's cycling road: from a
// platform on the land at one end, down stairs at the other. The road is
// built only from pieces cut out of Route 110 (tools/roguemon/gen_bridge.py,
// src/data/rogue_bridge.h), so every join inside a piece is one Emerald has:
//
//   the upper landing   a platform on the land with stairs down its south
//                       face; the road leaves south over the grass to the
//                       shore (Route 110's north end)
//   the lower landing   the road comes in from the north over the water
//                       onto a platform across the shore, with stairs down
//                       its south face (Route 110's south end)
//   a step              where the road steps east or west on a band of
//                       road with its pillars in the water (Route 110 rows
//                       75-82 and 28-35), its band stretched or shortened
//   the road            three blocks wide between its rails, its shadow on
//                       the water to the east
//
// The road and its platforms are elevation 4: the player climbs to them by
// the stairs (elevation 0 at their foot) and walks them at 4, above the
// water (1) and out of reach of the grass beside them (3), whose edges
// along the road are solid. Trainers stand on the road (they are put at
// elevation 4 where the block is, BuildObjects), and nobody needs a Bike or
// an HM: the road is walked like any other.
//
// A bridge route is drawn with Mauville's tileset (its theme is Verdant,
// PickBridgeRoute): 32 x 50 blocks between ways in north and south, 36 x 44
// round a corner, 40 x 40 from west to east (SizePlace). The road always
// runs down the map, from the upper landing to the lower one; the way in on
// the north side (else the one on the west, high on that side, or the one
// that is not on the south) is on the land at the top. The pieces stay out
// of view across the connections (BridgeInView), where a neighbour with
// another tileset draws this map with its own.

#include "data/rogue_bridge.h"

#define BRIDGE_ITEM_SPOTS 6

// A piece stamped at (x0, y0); 0xFFFF leaves the ground, and the plain sea
// in a piece is the sea already there.
static void BridgePut(struct Gen *gen, const u16 *s, int sw, int sh, int x0, int y0)
{
    int x, y;
    for (y = 0; y < sh; y++)
    {
        for (x = 0; x < sw; x++)
        {
            u16 v = s[y * sw + x];
            if (v != 0xFFFF && v != 0x1170 && InMap(gen, x0 + x, y0 + y))
                Put(gen, x0 + x, y0 + y, v);
        }
    }
}

// What the road does between the landings.
enum { BJOG_NONE, BJOG_EAST, BJOG_WEST, BJOG_MID };

// The columns of the piece for jog kind (a step's band stretched by n
// spans, one span being the road between two pillars; the wide spot as it
// is). Returns the width; *in and *out are the road's west column coming in
// and going out.
static int BridgeJogCols(u8 kind, int n, u8 *cols, int *in, int *out)
{
    int k, i, w = 0;
    switch (kind)
    {
    case BJOG_EAST:
        // Columns 0-6, then 7-11 (a pillar at 9) n times, then 12-18.
        for (i = 0; i <= 6; i++)
            cols[w++] = i;
        for (k = 0; k < n; k++)
            for (i = 7; i <= 11; i++)
                cols[w++] = i;
        for (i = 12; i <= 18; i++)
            cols[w++] = i;
        *in = BRIDGE_ZEAST_A;
        *out = w - (BRIDGE_ZEAST_W - BRIDGE_ZEAST_B);
        break;
    case BJOG_WEST:
        // Columns 0-10, then 11-16 (a pillar at 16) n times, then 17-23.
        for (i = 0; i <= 10; i++)
            cols[w++] = i;
        for (k = 0; k < n; k++)
            for (i = 11; i <= 16; i++)
                cols[w++] = i;
        for (i = 17; i <= 23; i++)
            cols[w++] = i;
        *in = w - (BRIDGE_ZWEST_W - BRIDGE_ZWEST_A);
        *out = BRIDGE_ZWEST_B;
        break;
    default:
        for (i = 0; i < BRIDGE_MID_W; i++)
            cols[w++] = i;
        *in = BRIDGE_MID_A;
        *out = BRIDGE_MID_B;
        break;
    }
    return w;
}

static int BridgeJogH(u8 kind)
{
    return kind == BJOG_MID ? BRIDGE_MID_H : BRIDGE_ZEAST_H;
}

static void BridgePutJog(struct Gen *gen, u8 kind, int n, int x0, int y0)
{
    u8 cols[40];
    int in, out, w = BridgeJogCols(kind, n, cols, &in, &out), x, y;
    const u16 *s = kind == BJOG_EAST ? sBridgeZeast : kind == BJOG_WEST ? sBridgeZwest : sBridgeMid;
    int sw = kind == BJOG_EAST ? BRIDGE_ZEAST_W : kind == BJOG_WEST ? BRIDGE_ZWEST_W : BRIDGE_MID_W;
    for (y = 0; y < BridgeJogH(kind); y++)
    {
        for (x = 0; x < w; x++)
        {
            u16 v = s[y * sw + cols[x]];
            if (v != 0xFFFF && v != 0x1170 && InMap(gen, x0 + x, y0 + y))
                Put(gen, x0 + x, y0 + y, v);
        }
    }
}

// The plan of one bridge, in blocks.
struct BridgePlan
{
    u8 upX, upY;     // the upper landing's top left
    u8 vland;        // rows of road over the grass below it
    u8 lowX, lowY;   // the lower landing's top left
    u8 jog, span;    // what the road does between (BJOG_*), and a step's spans
    u8 jogX, jogY;   // that piece's top left
};

// The first row of water below the upper landing.
static int BridgeWaterTop(const struct BridgePlan *b)
{
    return b->upY + BRIDGE_UP_H + b->vland + 1;
}

// The first row of land below the water (the lower landing's waist).
static int BridgeLandTop(const struct BridgePlan *b)
{
    return b->lowY + 5;
}

// Which way in stands on the land at the top, by the upper landing: one on
// the north side, else one on the west (PlaceExits puts a bridge's west way
// in high and its east one low, BridgeExitPos) or the one that is not
// beside a way in on the south.
static u8 BridgeUpperDir(const struct RoguePlace *p)
{
    if (HasSide(p, RDIR_NORTH))
        return RDIR_NORTH;
    if (LinkOnSide(p, RDIR_SOUTH) != NULL)
        return HasSide(p, RDIR_WEST) ? RDIR_WEST : RDIR_EAST;
    return RDIR_WEST;
}

// TRUE if a rectangle of blocks is in view from just across a connection,
// where the map next door draws this one with its own tileset: the camera
// shows 7 blocks to each side of the player and 5 below (4 above), and the
// player walks along the seam there.
static bool8 BridgeInView(struct Gen *gen, int x0, int y0, int x1, int y1)
{
    int i;
    for (i = 0; i < gen->p->nLinks; i++)
    {
        const struct RogueLink *l = &gen->p->links[i];
        int p = l->pos, vx0, vy0, vx1, vy1;
        switch (l->dir)
        {
        case RDIR_NORTH: vx0 = p - 10; vx1 = p + 11; vy0 = 0; vy1 = 6; break;
        case RDIR_SOUTH: vx0 = p - 10; vx1 = p + 11; vy0 = gen->m->h - 7; vy1 = gen->m->h - 1; break;
        case RDIR_WEST:  vx0 = 0; vx1 = 8; vy0 = p - 7; vy1 = p + 8; break;
        default:         vx0 = gen->m->w - 9; vx1 = gen->m->w - 1; vy0 = p - 7; vy1 = p + 8; break;
        }
        if (x0 <= vx1 && x1 >= vx0 && y0 <= vy1 && y1 >= vy0)
            return TRUE;
    }
    return FALSE;
}

// A plan that fits the map, or FALSE. Both landings stand clear of the
// edges and out of view across the seams, with room to walk round the
// upper one to its stairs; the road in between runs straight down, takes a
// step east or west, or widens out on the water, and the lower landing
// stands as low as it can, for a long way over the water.
static bool8 PlanBridge(struct Gen *gen, struct BridgePlan *b, u8 upper, int upperY, int lowerY)
{
    int w = gen->m->w, h = gen->m->h, tries;
    // Now and then straight down; else a jog, which needs more room, so
    // straight only once none fits.
    bool8 straight = RAND(8) == 0;
    // The trail from the foot of the lower landing's stairs starts inside
    // the trees round the edge.
    int lowMax = h - 15;
    if (!(lowMax & 1))
        lowMax--;
    for (tries = 0; tries < 400; tries++)
    {
        int upRoad, lowRoad, below, jogW = 0, in = 0, out = 0, room, r, y1;
        u8 cols[40];
        memset(b, 0, sizeof(*b));
        b->vland = 1 + RAND(3);
        // Room above to walk round it from a way in on the east.
        b->upY = (upper == RDIR_EAST ? 4 : 2) + RAND(8);
        // The water starts on an even row (a unit's top).
        if ((BridgeWaterTop(b) & 1))
            b->upY++;
        // Room to walk round it on the west (beside the trees there).
        b->upX = 2 + RAND(w - 2 - BRIDGE_UP_W - 2 + 1);
        upRoad = b->upX + BRIDGE_UP_A;
        below = b->upY + BRIDGE_UP_H + b->vland + BRIDGE_BANK_H;
        lowRoad = upRoad;
        r = RAND(10);
        b->jog = (straight || tries >= 300) ? BJOG_NONE : r < 4 ? BJOG_EAST : r < 8 ? BJOG_WEST : BJOG_MID;
        if (b->jog != BJOG_NONE)
        {
            b->span = b->jog == BJOG_MID ? 0 : RAND(3);
            jogW = BridgeJogCols(b->jog, b->span, cols, &in, &out);
            // Plain water between it and the banks.
            if (upRoad - in < 4 || upRoad - in + jogW - 1 > w - 5)
                continue;
            b->jogX = upRoad - in;
            lowRoad = b->jogX + out;
            below += BridgeJogH(b->jog);
        }
        if (lowRoad - BRIDGE_LOW_A < 3 || lowRoad - BRIDGE_LOW_A + BRIDGE_LOW_W - 1 > w - 4)
            continue;
        b->lowX = lowRoad - BRIDGE_LOW_A;
        // As low as it goes, now and then a little higher. (Its top on an
        // odd row: the land starts on an even one.)
        b->lowY = lowMax - 2 * RAND(2);
        if (b->lowY < below)
            b->lowY = lowMax;
        if (b->lowY < below)
            continue;
        // The jog anywhere between.
        room = b->lowY - below;
        if (b->jog != BJOG_NONE)
            b->jogY = below - BridgeJogH(b->jog) + RAND(room + 1);
        // Water at least four rows deep beside the road.
        if (BridgeLandTop(b) - BridgeWaterTop(b) < 8)
            continue;
        // A way in on a side: its land reaches it along the edge, with
        // water still between the two lands (unit rows).
        if (upperY >= 0 && upperY + 3 >= BridgeLandTop(b) / 2)
            continue;
        if (lowerY >= 0 && lowerY - 3 <= BridgeWaterTop(b) / 2 - 1)
            continue;
        // (Nor right under the water, where the bank would take the top of
        // the way in.)
        if (lowerY >= 0 && lowerY == BridgeLandTop(b) / 2)
            continue;
        // Out of view across the seams.
        y1 = b->upY + BRIDGE_UP_H + b->vland + BRIDGE_BANK_H - 1;
        if (BridgeInView(gen, b->upX, b->upY, b->upX + BRIDGE_UP_W - 1, y1)
         || BridgeInView(gen, upRoad - 1, b->upY, upRoad + 4, b->jog ? b->jogY : b->lowY)
         || (b->jog && BridgeInView(gen, b->jogX, b->jogY, b->jogX + jogW - 1, b->jogY + BridgeJogH(b->jog) - 1))
         || BridgeInView(gen, lowRoad - 1, b->jog ? b->jogY : b->upY, lowRoad + 4, b->lowY)
         || BridgeInView(gen, b->lowX, b->lowY, b->lowX + BRIDGE_LOW_W - 1, b->lowY + BRIDGE_LOW_H - 1))
            continue;
        return TRUE;
    }
    return FALSE;
}

// Marks in gen->keep the units within a unit of the road and its shadow
// (rows y0 to y1, columns x0 to x1, in blocks).
static void BridgeNear(struct Gen *gen, int x0, int y0, int x1, int y1)
{
    int x, y;
    for (y = (y0 >> 1) - 1; y <= (y1 >> 1) + 1; y++)
        for (x = (x0 >> 1) - 1; x <= (x1 >> 1) + 1; x++)
            SetBitU(gen->keep, gen, x, y);
}

// Islands of grass out on the water, as on Route 110 below its road, clear
// of the road and of the land. Only Surf reaches them.
static void BridgeIslands(struct Gen *gen, int n)
{
    int tries, k = 0;
    for (tries = 0; tries < 80 && k < n; tries++)
    {
        int iw = 1 + RAND(3), ih = 1 + RAND(2), x0 = 1 + RAND(gen->cw - 2), y0 = 1 + RAND(gen->ch - 2), x, y;
        bool8 ok = TRUE;
        for (y = y0 - 1; y <= y0 + ih && ok; y++)
            for (x = x0 - 1; x <= x0 + iw && ok; x++)
                if (Cell(gen, x, y) != C_SEA || BitU(gen->keep, gen, x, y))
                    ok = FALSE;
        if (!ok)
            continue;
        for (y = y0; y < y0 + ih; y++)
            for (x = x0; x < x0 + iw; x++)
                SetCell(gen, x, y, C_OPEN);
        // A tree on a bigger one.
        if (iw >= 2 && ih == 2 && CHANCE(50))
            SetCell(gen, x0 + RAND(iw), y0, C_TREE);
        PaintBlob(gen, x0 * 2 + 1, y0 * 2 + 1, 2, V_TALL, FALSE);
        k++;
    }
}

// The shores along the sides of the water come and go: bays and points of
// grass and trees a unit or two deep, clear of the road and of the land at
// either end, so they never join the two.
static void BridgeShores(struct Gen *gen, int y0, int y1)
{
    int side, y;
    for (side = 0; side < 2; side++)
    {
        int depth = 0;
        for (y = y0; y <= y1; y++)
        {
            int k;
            if (RAND(3) == 0)
                depth = RAND(3);
            for (k = 0; k < depth; k++)
            {
                int x = side ? gen->cw - 2 - k : 1 + k;
                if (Cell(gen, x, y) != C_SEA || BitU(gen->keep, gen, x, y) || BitU(gen->keep, gen, x + (side ? -1 : 1), y))
                    break;
                SetCell(gen, x, y, k == 0 && CHANCE(60) ? C_TREE : C_OPEN);
            }
        }
    }
}

// Along the edges away from the ways in, the trees reach in one to three
// units, unevenly, as on Hoenn's routes; never onto a trail or the land
// beside one, and not into the water.
static void BridgeEdgeTrees(struct Gen *gen)
{
    u8 d;
    for (d = 0; d < 4; d++)
    {
        int len = EdgeLength(gen, d), pos;
        for (pos = 0; pos < len; pos++)
        {
            int depth = EdgeBand(gen->r, gen->p, d, pos), k;
            for (k = 1; k < depth; k++)
            {
                int x, y;
                EdgeCell(gen, d, pos, k, &x, &y);
                if (Cell(gen, x, y) != C_OPEN || HasNeighbor(gen, x, y, C_ROAD, TRUE) || HasNeighbor(gen, x, y, C_SEA, FALSE)
                 || HasNeighbor(gen, x, y, C_BLDG, TRUE))
                    break;
                SetCell(gen, x, y, C_TREE);
            }
        }
    }
}

// The white fence along the shore beside the road where it leaves the
// land, as Route 110 has it: on the land's last row, a few blocks each
// way, on grass only.
static void BridgeFence(struct Gen *gen, int road, int y)
{
    int side, k;
    for (side = 0; side < 2; side++)
    {
        int n = 2 + RAND(5);
        for (k = 0; k < n; k++)
        {
            int x = side ? road + 4 + k : road - 2 - k;
            u16 b;
            if (!InMap(gen, x, y) || !IsSeaF(gen, x, y + 1) || IsOccupied(gen, x, y))
                break;
            b = gen->m->blocks[y * gen->m->w + x];
            if (b != B_GRASS && b != B_TALL_GRASS && b != B_FLOWERS)
                break;
            // (Not up against a path.)
            b = InMap(gen, x + (side ? 1 : -1), y) ? gen->m->blocks[y * gen->m->w + x + (side ? 1 : -1)] : B_GRASS;
            if (((b & 0x3FF) >= (B_SAND_TL & 0x3FF) && (b & 0x3FF) <= (B_SAND_TL & 0x3FF) + 18)
             || ((b & 0x3FF) >= (B_PATH_TL & 0x3FF) && (b & 0x3FF) <= (B_PATH_TL & 0x3FF) + 18))
                break;
            Put(gen, x, y, 0x104E);
        }
    }
}

// The units a piece covers (blocks that are not 0xFFFF): solid for the
// plan, so the trail, trees and grass keep off it.
static void BridgeClaim(struct Gen *gen, const u16 *s, int sw, int sh, int x0, int y0, bool8 landOnly)
{
    int x, y;
    for (y = 0; y < sh; y++)
    {
        for (x = 0; x < sw; x++)
        {
            int ux = (x0 + x) >> 1, uy = (y0 + y) >> 1;
            if (s[y * sw + x] == 0xFFFF || !InBounds(gen, ux, uy))
                continue;
            if (landOnly && Cell(gen, ux, uy) == C_SEA)
                continue;
            SetCell(gen, ux, uy, C_BLDG);
        }
    }
}

// A trail of road units from (sx, sy) to (ex, ey) over open ground, the
// shortest there is (breadth-first over units). With keepOff 1, not right
// under the water, whose bank takes the top of the unit; with 2 not along
// the landings either (a grass verge between), but for the ends.
static bool8 BridgeTrailOnce(struct Gen *gen, int sx, int sy, int ex, int ey, u8 keepOff)
{
    int head = 0, tail = 0, x, y, n = 0;
    memset(sBfsPrev, 0xFF, gen->cw * gen->ch * sizeof(sBfsPrev[0]));
    sBfsQueue[tail++] = PACK(sx, sy);
    sBfsPrev[sy * gen->cw + sx] = sy * gen->cw + sx;
    while (head < tail && sBfsPrev[ey * gen->cw + ex] < 0)
    {
        int cur = sBfsQueue[head++], d;
        YIELD();
        for (d = 0; d < 4; d++)
        {
            int nx = PX(cur) + sDirDX[d], ny = PY(cur) + sDirDY[d];
            u8 c;
            if (!InBounds(gen, nx, ny) || sBfsPrev[ny * gen->cw + nx] >= 0)
                continue;
            c = Cell(gen, nx, ny);
            if ((c != C_OPEN && c != C_ROAD) || (IsEdge(gen, nx, ny) && !(nx == ex && ny == ey)))
                continue;
            // (Beside the landing at either end it may.)
            if (keepOff && Abs(nx - ex) + Abs(ny - ey) > 2 && Abs(nx - sx) + Abs(ny - sy) > 2
             && ((keepOff > 1 && HasNeighbor(gen, nx, ny, C_BLDG, TRUE)) || Cell(gen, nx, ny - 1) == C_SEA))
                continue;
            sBfsPrev[ny * gen->cw + nx] = PY(cur) * gen->cw + PX(cur);
            sBfsQueue[tail++] = PACK(nx, ny);
        }
    }
    if (sBfsPrev[ey * gen->cw + ex] < 0)
        return FALSE;
    // Walk back from the end, then lay the road from the start.
    for (x = ex, y = ey; n < BFS_RING - 1; n++)
    {
        int p = sBfsPrev[y * gen->cw + x];
        sBfsQueue[n] = PACK(x, y);
        if (x == sx && y == sy)
            break;
        x = p % gen->cw;
        y = p / gen->cw;
    }
    for (; n >= 0; n--)
        AddRoadCell(gen, PX(sBfsQueue[n]), PY(sBfsQueue[n]));
    return TRUE;
}

static bool8 BridgeTrail(struct Gen *gen, int sx, int sy, int ex, int ey)
{
    return BridgeTrailOnce(gen, sx, sy, ex, ey, 2) || BridgeTrailOnce(gen, sx, sy, ex, ey, 1)
        || BridgeTrailOnce(gen, sx, sy, ex, ey, 0);
}

// Land along edge d from the land at the top (or the bottom) to just past
// the way in there, two units deep.
static void BridgeStrip(struct Gen *gen, u8 d, int y0, int y1)
{
    int x, y, ux = d == RDIR_WEST ? 1 : gen->cw - 3;
    for (y = y0; y <= y1; y++)
        for (x = ux; x < ux + 2; x++)
            if (InBounds(gen, x, y) && Cell(gen, x, y) == C_SEA)
                SetCell(gen, x, y, C_OPEN);
}

// The way through, on the drawn blocks: from the way in to the way out.
static bool8 BridgeWalks(struct Gen *gen, int sx, int sy, int ex, int ey)
{
    int b;
    memset(gen->free, 0, sizeof(gen->free));
    for (b = 0; b < gen->m->w * gen->m->h; b++)
    {
        u16 v = gen->m->blocks[b];
        if (((v >> 10) & 3) == 0 && WalkHeight(gen, v) && !IsOccupied(gen, b % gen->m->w, b / gen->m->w))
            gen->free[b >> 3] |= 1 << (b & 7);
    }
    return FindShortestWalk(gen, sx, sy, ex, ey);
}

static bool8 GenBridgeRoute(struct Gen *gen, u8 inDir, u8 outDir)
{
    const struct RoguePlace *p = gen->p;
    struct BridgePlan b;
    int ax, ay, bx, by, ux0, uy0, ux1, uy1, i, k, tries, placed;
    int waterTop, landTop, upRoad, lowRoad, y;
    u8 upper = BridgeUpperDir(p), lower = upper == inDir ? outDir : inDir;
    static const s8 sItemSpots[BRIDGE_ITEM_SPOTS][3] = {
        // Corners of the platforms, off the way across: (landing, x, y).
        { 0, 13, 2 }, { 0, 2, 2 }, { 1, 2, 3 }, { 1, 13, 3 }, { 0, 13, 3 }, { 1, 2, 2 },
    };

    if (upper != inDir && upper != outDir)
        return FALSE;
    {
        const struct RogueLink *lu = LinkOnSide(p, upper), *ll = LinkOnSide(p, lower);
        int uy = (upper == RDIR_WEST || upper == RDIR_EAST) ? lu->pos / 2 : -1;
        int ly = (lower == RDIR_WEST || lower == RDIR_EAST) ? ll->pos / 2 : -1;
        if (!PlanBridge(gen, &b, upper, uy, ly))
            return FALSE;
    }
    waterTop = BridgeWaterTop(&b);
    landTop = BridgeLandTop(&b);
    upRoad = b.upX + BRIDGE_UP_A;
    lowRoad = b.lowX + BRIDGE_LOW_A;
    gen->groundKind = GROUND_SAND;
    gen->bites = FALSE;

    // Land at both ends, the water between, trees round the edges; a way in
    // on the west or east side has land along that edge from its end.
    Fill(gen, C_OPEN);
    for (y = waterTop / 2; y < landTop / 2; y++)
        for (i = 1; i < gen->cw - 1; i++)
            SetCell(gen, i, y, C_SEA);
    MakeOpening(gen, upper, &ux0, &uy0);
    MakeOpening(gen, lower, &ux1, &uy1);
    if (upper == RDIR_WEST || upper == RDIR_EAST)
        BridgeStrip(gen, upper, waterTop / 2, uy0 + 2);
    if (lower == RDIR_WEST || lower == RDIR_EAST)
        BridgeStrip(gen, lower, uy1 - 2, landTop / 2);

    // The landings and the road over the grass are solid in the plan.
    BridgeClaim(gen, sBridgeUp, BRIDGE_UP_W, BRIDGE_UP_H, b.upX, b.upY, FALSE);
    for (k = 0; k < b.vland; k++)
        BridgeClaim(gen, sBridgeVland, BRIDGE_VLAND_W, BRIDGE_VLAND_H, upRoad - BRIDGE_VLAND_A, b.upY + BRIDGE_UP_H + k, TRUE);
    BridgeClaim(gen, sBridgeBank, BRIDGE_BANK_W, 1, upRoad - BRIDGE_BANK_A, b.upY + BRIDGE_UP_H + b.vland, TRUE);
    BridgeClaim(gen, sBridgeLow, BRIDGE_LOW_W, BRIDGE_LOW_H, b.lowX, b.lowY, TRUE);

    // Only the road joins the two lands.
    FloodReachable(gen, ux0, uy0);
    if (Reached(gen, ux1, uy1))
        return FALSE;
    // Islands on the water, clear of the road.
    memset(gen->keep, 0, sizeof(gen->keep));
    BridgeNear(gen, b.upX, b.upY, b.upX + BRIDGE_UP_W - 1, waterTop + 3);
    BridgeNear(gen, upRoad - 1, b.upY, upRoad + 4, b.lowY + BRIDGE_LOW_H - 1);
    BridgeNear(gen, lowRoad - 1, b.upY, lowRoad + 4, b.lowY + BRIDGE_LOW_H - 1);
    BridgeNear(gen, b.lowX, b.lowY, b.lowX + BRIDGE_LOW_W - 1, b.lowY + BRIDGE_LOW_H - 1);
    if (b.jog)
    {
        u8 cols[40];
        int in, out;
        BridgeNear(gen, b.jogX, b.jogY, b.jogX + BridgeJogCols(b.jog, b.span, cols, &in, &out) - 1, b.jogY + BridgeJogH(b.jog) - 1);
    }
    BridgeIslands(gen, RAND(4));
    BridgeShores(gen, waterTop / 2 + 1, landTop / 2 - 2);

    // Trails: from the way in above to the foot of the upper landing's
    // stairs (round its west side), and from the foot of the lower
    // landing's stairs to the way out below.
    {
        // (To the grass beside the foot of the stairs, from the west.)
        int fx = b.upX + BRIDGE_UP_B, fy = b.upY + BRIDGE_UP_H;
        if (!BridgeTrail(gen, ux0, uy0, (fx - 2) >> 1, (fy + 1) >> 1))
            return FALSE;
        fx = b.lowX + BRIDGE_LOW_B;
        fy = b.lowY + BRIDGE_LOW_H;
        if (!BridgeTrail(gen, fx >> 1, (fy + 1) >> 1, ux1, uy1))
            return FALSE;
        // (The edge units of the openings are road too.)
        EdgeCell(gen, upper, gen->exitPos[upper] / 2, 0, &ax, &ay);
        AddRoadCell(gen, ax, ay);
        EdgeCell(gen, lower, gen->exitPos[lower] / 2, 0, &bx, &by);
        AddRoadCell(gen, bx, by);
    }

    // Trees about the land, never on the trails or the landings.
    BridgeEdgeTrees(gen);
    for (tries = 0; tries < 40; tries++)
    {
        int x = 1 + RAND(gen->cw - 2), yy = 1 + RAND(gen->ch - 2);
        if (Cell(gen, x, yy) != C_OPEN || HasNeighbor(gen, x, yy, C_ROAD, FALSE) || HasNeighbor(gen, x, yy, C_BLDG, FALSE))
            continue;
        SetCell(gen, x, yy, C_TREE);
    }
    // A sand trail, as on Route 110 by the water, and tall grass.
    for (i = 0; i < gen->cw * gen->ch; i++)
        if (gen->g[i] == C_ROAD)
            PaintUnit(gen, i % gen->cw, i / gen->cw, V_SAND);
    for (k = 0; k < 10; k++)
    {
        int x = 2 + RAND(gen->cw - 4), yy = 1 + RAND(gen->ch - 2);
        if (Cell(gen, x, yy) != C_OPEN)
            continue;
        PaintBlob(gen, x * 2 + 1, yy * 2 + 1, 3 + RAND(4), V_TALL, FALSE);
    }
    ScatterFlowers(gen, 1 + RAND(3));
    TidyGround(gen);
    Render(gen);

    // The road: the upper landing, down over the grass to the shore, over
    // the water (with its step), and onto the lower landing.
    BridgePut(gen, sBridgeUp, BRIDGE_UP_W, BRIDGE_UP_H, b.upX, b.upY);
    y = b.upY + BRIDGE_UP_H;
    for (k = 0; k < b.vland; k++)
        BridgePut(gen, sBridgeVland, BRIDGE_VLAND_W, BRIDGE_VLAND_H, upRoad - BRIDGE_VLAND_A, y++);
    BridgePut(gen, sBridgeBank, BRIDGE_BANK_W, BRIDGE_BANK_H, upRoad - BRIDGE_BANK_A, y);
    y += BRIDGE_BANK_H;
    if (b.jog)
    {
        for (; y < b.jogY; y++)
            BridgePut(gen, sBridgeV, BRIDGE_V_W, BRIDGE_V_H, upRoad - BRIDGE_V_A, y);
        BridgePutJog(gen, b.jog, b.span, b.jogX, b.jogY);
        y = b.jogY + BridgeJogH(b.jog);
    }
    for (; y < b.lowY; y++)
        BridgePut(gen, sBridgeV, BRIDGE_V_W, BRIDGE_V_H, lowRoad - BRIDGE_V_A, y);
    BridgePut(gen, sBridgeLow, BRIDGE_LOW_W, BRIDGE_LOW_H, b.lowX, b.lowY);
    BridgeFence(gen, upRoad, waterTop - 1);

    // Items in the corners of the platforms, off the way across.
    placed = 0;
    for (tries = 0; tries < 20 && placed < p->nItems; tries++)
    {
        const s8 *s = sItemSpots[(tries + RAND(2)) % BRIDGE_ITEM_SPOTS];
        int fx = (s[0] ? b.lowX : b.upX) + s[1], fy = (s[0] ? b.lowY : b.upY) + s[2];
        if (IsOccupied(gen, fx, fy) || IsCrowded(gen, fx, fy))
            continue;
        AddObj(gen, ROBJ_ITEM, fx, fy, RDIR_SOUTH, placed++, 0);
    }
    if (placed < p->nItems)
        return FALSE;

    // The way through must be there on the drawn blocks.
    EdgeBlock(gen, inDir, &ax, &ay);
    EdgeBlock(gen, outDir, &bx, &by);
    if (!BridgeWalks(gen, ax, ay, bx, by))
        return FALSE;
    // Trainers along the road: put on the walk now, moved to their spots
    // by PlaceTrainersOnWalk.
    for (i = 0; i < p->nTrainers; i++)
    {
        int step = gen->nWalk * (i + 1) / (p->nTrainers + 1);
        AddObj(gen, ROBJ_TRAINER, PX(gen->walk[step]), PY(gen->walk[step]), RDIR_SOUTH, i, 3);
    }
    // A sign at each way in.
    for (k = 0; k < 2; k++)
    {
        int ux = k ? ux1 : ux0, uy = k ? uy1 : uy0, fx, fy;
        u8 facing;
        FloodReachable(gen, ux, uy);
        if (FindSignSpot(gen, ux, uy, &fx, &fy, &facing) && !IsOccupied(gen, fx, fy))
            AddSign(gen, fx, fy, RSIGN_ROUTE);
    }
    RenderSigns(gen);
    PlaceTrainersOnWalk(gen, inDir, outDir);
    return TRUE;
}

// A bridge route's way in on a west or east side (PlaceExits) lies in the
// top third when it is on the land at the top (BridgeUpperDir), else in the
// bottom third, so the two lands can stay apart with water between. pos is
// the unit drawn in [first, last].
static int BridgeExitPos(const struct RoguePlace *p, u8 dir, int first, int last, int pos)
{
    int third = (last - first) / 3, span = last - first > 0 ? last - first : 1;
    if (p->kind != RPLACE_ROUTE || p->type != RROUTE_BRIDGE || (dir != RDIR_WEST && dir != RDIR_EAST))
        return pos;
    if (BridgeUpperDir(p) == dir)
        return first + (pos - first) * third / span;
    return last - third + (pos - first) * third / span;
}

// ---------------------------------------------------------------------------
// Where a region has its bridge
//
// About two regions in three have one bridge route: a route of the main
// road from the third tier on between two places and nothing else, with no block, cave mouth, rival or roadside camp on
// it and no lane up to a block next door. Routes into a Verdant town
// (which already have Mauville's tileset) and routes by water (a lake,
// river or coast next door) are likelier. It is drawn with Mauville's
// tileset, with no rest house. Its own random stream, so the rest of the
// plan is as it was.
#define BRIDGE_SALT 11302
#define BRIDGE_PCT 33

static void PickBridgeRoute(struct RogueRegion *r)
{
    u32 rng = RogueRng_Hash(r->seed, BRIDGE_SALT);
    u8 cand[ROGUE_MAX_PLACES], weight[ROGUE_MAX_PLACES];
    s32 camp = RogueGen_RouteCamp(r);
    int n = 0, total = 0, i, k, pick;
    if (RogueGen_IsRun0(r) || RogueRng_Range(&rng, 100) >= BRIDGE_PCT)
        return;
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        bool8 ok = TRUE, wet = FALSE;
        if (p->kind != RPLACE_ROUTE || p->side || p->type == RROUTE_MOUNTAIN || p->type == RROUTE_SEA)
            continue;
        if (p->tier < 2 || p->obstacle || p->mouth != 0xFF || p->rival || (s32)i == camp || p->theme == RTHEME_ASH)
            continue;
        if (p->nLinks != 2)
            continue;
        for (k = 0; k < p->nLinks; k++)
        {
            const struct RoguePlace *t = &r->places[p->links[k].target];
            // (A block next door standing in this route's lane.)
            if (ObstacleOutside(r, t) && t->guardDir == Opposite(p->links[k].dir))
                ok = FALSE;
            if (t->kind == RPLACE_ROUTE && (t->type == RROUTE_LAKE || t->type == RROUTE_RIVER || t->type == RROUTE_COAST))
                wet = TRUE;
        }
        if (!ok)
            continue;
        cand[n] = i;
        weight[n] = 1 + (p->theme == RTHEME_VERDANT ? 3 : 0) + (wet ? 2 : 0);
        total += weight[n++];
    }
    if (n == 0)
        return;
    pick = RogueRng_Range(&rng, total);
    for (k = 0; k < n - 1 && pick >= weight[k]; k++)
        pick -= weight[k];
    {
        struct RoguePlace *p = &r->places[cand[k]];
        p->type = RROUTE_BRIDGE;
        p->theme = RTHEME_VERDANT;
        p->seaSide = RDIR_NONE;
        p->interiors &= ~(1 << RINT_REST);
    }
}
