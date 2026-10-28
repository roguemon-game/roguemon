// Berry farms (v1.1): the BERRY MASTER's farm, as on Routes 104 and 123.
//
// In some regions the route the BERRY MASTER lives on (src/rogue_gen_features.h)
// is a berry farm: his house beside a fenced field of soft soil, two long
// rows of it with berry trees in fruit, the gate open below onto the trail,
// as Route 123's garden is fenced beside his house. The soil is the General
// tileset's (Route 104's strips: 14B 14C 14D along the top, 113 114 115 the
// lip below), so every route tileset has it; a farm is only ever on a field,
// a flower meadow or pond country (the kinds the BERRY MASTER takes but the
// forest), never in the ash or by the sea.
//
// Nothing in the plan moves: whether a region has a farm is a roll on its own
// stream (salt 43000), and only the farm route's map changes (its lot is the
// farm instead of the small garden, and it has no roadside berry patch).
//
// The trees use their own berry tree ids, after the BERRY MASTER's garden's
// (ROGUE_BFARM_BERRY_BASE: 109 to 118 of the save's 128), planted in fruit at
// the start of the run (Rogue_PlantBerries), so picking one gives its berry
// as in Emerald, with no new save data. A map holds at most ROGUE_MAX_OBJS
// people and things, so the field has as many trees as are left once the
// route's trainers and items stand (six to eight most times); the other soil
// is left bare, as Route 104's unplanted soil is.
//
// Included into rogue_gen.c before GenRoute.

#include "constants/items.h"

#define BERRY_FARM_SALT  43000 // whether a region has a farm
#define BERRY_ROW_SALT   43100 // what each row grows
#define BERRY_FARM_SHARE 85    // % of the BERRY MASTER's fitting routes that are farms
// A farm that does not fit in this many tries of its map is the small garden
// after all (each try at a map costs time in the ROM).
#define BERRY_FARM_TRIES 6

// ---------------------------------------------------------------------------
// Planning

// The kinds a farm suits: open meadows (Routes 104, 117 and 123), not the
// woods, the ash or a coast.
static bool8 BerryFarmKind(const struct RoguePlace *p)
{
    if (p->type != RROUTE_FIELD && p->type != RROUTE_FLOWERS && p->type != RROUTE_LAKE)
        return FALSE;
    return p->theme != RTHEME_ASH && p->seaSide == RDIR_NONE;
}

bool8 RogueGen_BerryFarmRoute(const struct RogueRegion *r, u8 place)
{
    const struct RoguePlace *p;
    if (place >= r->nPlaces || RogueGen_IsRun0(r))
        return FALSE;
    p = &r->places[place];
    if (p->kind != RPLACE_ROUTE || !BerryFarmKind(p))
        return FALSE;
    if (RogueRng_Hash(r->seed, BERRY_FARM_SALT) % 100 >= BERRY_FARM_SHARE)
        return FALSE;
    return RogueGen_RouteFeature(r, place) == RFEAT_BERRY;
}

// What a row grows: one kind to a row, as Route 123's pairs of trees are,
// the two rows different. The everyday berries (as the route trees'), most
// of all Oran; no Sitrus (the BERRY MASTER gives those) or anything rarer.
static const u16 sBerryFarmCrops[] = {
    ITEM_ORAN_BERRY, ITEM_ORAN_BERRY, ITEM_ORAN_BERRY, ITEM_ORAN_BERRY,
    ITEM_PECHA_BERRY, ITEM_PECHA_BERRY, ITEM_CHERI_BERRY, ITEM_CHERI_BERRY,
    ITEM_CHESTO_BERRY, ITEM_CHESTO_BERRY, ITEM_RAWST_BERRY, ITEM_RAWST_BERRY,
    ITEM_ASPEAR_BERRY, ITEM_PERSIM_BERRY, ITEM_LEPPA_BERRY,
};

u16 RogueGen_FarmRowBerry(u32 seed, u8 row)
{
    u32 rng = RogueRng_Hash(seed, BERRY_ROW_SALT);
    u32 n = sizeof(sBerryFarmCrops) / sizeof(sBerryFarmCrops[0]);
    u16 first = sBerryFarmCrops[RogueRng_Range(&rng, n)], crop = first;
    int tries;
    if (row == 0)
        return first;
    for (tries = 0; tries < 16 && crop == first; tries++)
        crop = sBerryFarmCrops[RogueRng_Range(&rng, n)];
    return crop;
}

// ---------------------------------------------------------------------------
// The farm lot: the house (2x2 units) in a bottom corner, a lawn with a few
// flowers above it, a strip of grass and then the fenced field (five units
// tall), laid as Route 123's garden is: General's fence (the pieces the
// Day Care's yard and the BERRY MASTER's garden use, which tile_polish.py
// knows), a margin of grass inside, the gate a gap of three in the bottom
// rail between corner posts; in it two rows of Route 104's soil (14B 14C
// 14D, the lip 113 114 115 below), flowers along each one's top edge as
// Route 104 has them, a tree on each middle block. Every pair of blocks
// inside the fence is one Hoenn has. Each stamp covers the whole lot;
// 0xFFFF where the house stands.

#define FT_TL F_TL
#define FT_H  F_H
#define FT_TR F_TR
#define FT_L  F_L
#define FT_R  F_R
#define FT_BL F_BL
#define FT_BR F_BR
#define BG_ 0x3001 // grass
#define BF_ 0x3004 // flowers
#define BX_ 0xFFFF // the house
#define SL_ B_SOIL_L
#define SM_ B_SOIL
#define SR_ B_SOIL_R
#define FL_ B_SOIL_FRONT_L
#define FM_ B_SOIL_FRONT
#define FR_ B_SOIL_FRONT_R

// Wide: five trees to a row (8x5 units); narrow: three (7x5).
static const u16 sBerryFarmWideL[16 * 10] = {
    BG_, BG_, BG_, BG_, BG_, FT_TL, FT_H, FT_H, FT_H, FT_H, FT_H, FT_H, FT_H, FT_H, FT_H, FT_TR,
    BG_, BF_, BG_, BG_, BG_, FT_L, BG_, BF_, BF_, BF_, BF_, BF_, BF_, BF_, BG_, FT_R,
    BF_, BF_, BG_, BG_, BG_, FT_L, BG_, SL_, SM_, SM_, SM_, SM_, SM_, SR_, BG_, FT_R,
    BG_, BG_, BG_, BF_, BG_, FT_L, BG_, FL_, FM_, FM_, FM_, FM_, FM_, FR_, BG_, FT_R,
    BG_, BG_, BF_, BF_, BG_, FT_L, BG_, BG_, BG_, BG_, BG_, BG_, BG_, BG_, BG_, FT_R,
    BG_, BG_, BG_, BG_, BG_, FT_L, BG_, BF_, BF_, BF_, BF_, BF_, BF_, BF_, BG_, FT_R,
    BX_, BX_, BX_, BX_, BG_, FT_L, BG_, SL_, SM_, SM_, SM_, SM_, SM_, SR_, BG_, FT_R,
    BX_, BX_, BX_, BX_, BG_, FT_L, BG_, FL_, FM_, FM_, FM_, FM_, FM_, FR_, BG_, FT_R,
    BX_, BX_, BX_, BX_, BG_, FT_L, BG_, BG_, BG_, BG_, BG_, BG_, BG_, BG_, BG_, FT_R,
    BX_, BX_, BX_, BX_, BG_, FT_BL, FT_H, FT_H, FT_BR, BG_, BG_, BG_, FT_BL, FT_H, FT_H, FT_BR,
};
static const u16 sBerryFarmWideR[16 * 10] = {
    FT_TL, FT_H, FT_H, FT_H, FT_H, FT_H, FT_H, FT_H, FT_H, FT_H, FT_TR, BG_, BG_, BG_, BG_, BG_,
    FT_L, BG_, BF_, BF_, BF_, BF_, BF_, BF_, BF_, BG_, FT_R, BG_, BG_, BG_, BF_, BG_,
    FT_L, BG_, SL_, SM_, SM_, SM_, SM_, SM_, SR_, BG_, FT_R, BG_, BG_, BG_, BF_, BF_,
    FT_L, BG_, FL_, FM_, FM_, FM_, FM_, FM_, FR_, BG_, FT_R, BG_, BF_, BG_, BG_, BG_,
    FT_L, BG_, BG_, BG_, BG_, BG_, BG_, BG_, BG_, BG_, FT_R, BG_, BF_, BF_, BG_, BG_,
    FT_L, BG_, BF_, BF_, BF_, BF_, BF_, BF_, BF_, BG_, FT_R, BG_, BG_, BG_, BG_, BG_,
    FT_L, BG_, SL_, SM_, SM_, SM_, SM_, SM_, SR_, BG_, FT_R, BG_, BX_, BX_, BX_, BX_,
    FT_L, BG_, FL_, FM_, FM_, FM_, FM_, FM_, FR_, BG_, FT_R, BG_, BX_, BX_, BX_, BX_,
    FT_L, BG_, BG_, BG_, BG_, BG_, BG_, BG_, BG_, BG_, FT_R, BG_, BX_, BX_, BX_, BX_,
    FT_BL, FT_H, FT_H, FT_BR, BG_, BG_, BG_, FT_BL, FT_H, FT_H, FT_BR, BG_, BX_, BX_, BX_, BX_,
};
static const u16 sBerryFarmNarrowL[14 * 10] = {
    BG_, BG_, BG_, BG_, BG_, FT_TL, FT_H, FT_H, FT_H, FT_H, FT_H, FT_H, FT_H, FT_TR,
    BG_, BF_, BG_, BG_, BG_, FT_L, BG_, BF_, BF_, BF_, BF_, BF_, BG_, FT_R,
    BF_, BF_, BG_, BG_, BG_, FT_L, BG_, SL_, SM_, SM_, SM_, SR_, BG_, FT_R,
    BG_, BG_, BG_, BF_, BG_, FT_L, BG_, FL_, FM_, FM_, FM_, FR_, BG_, FT_R,
    BG_, BG_, BF_, BF_, BG_, FT_L, BG_, BG_, BG_, BG_, BG_, BG_, BG_, FT_R,
    BG_, BG_, BG_, BG_, BG_, FT_L, BG_, BF_, BF_, BF_, BF_, BF_, BG_, FT_R,
    BX_, BX_, BX_, BX_, BG_, FT_L, BG_, SL_, SM_, SM_, SM_, SR_, BG_, FT_R,
    BX_, BX_, BX_, BX_, BG_, FT_L, BG_, FL_, FM_, FM_, FM_, FR_, BG_, FT_R,
    BX_, BX_, BX_, BX_, BG_, FT_L, BG_, BG_, BG_, BG_, BG_, BG_, BG_, FT_R,
    BX_, BX_, BX_, BX_, BG_, FT_BL, FT_H, FT_BR, BG_, BG_, BG_, FT_BL, FT_H, FT_BR,
};
static const u16 sBerryFarmNarrowR[14 * 10] = {
    FT_TL, FT_H, FT_H, FT_H, FT_H, FT_H, FT_H, FT_H, FT_TR, BG_, BG_, BG_, BG_, BG_,
    FT_L, BG_, BF_, BF_, BF_, BF_, BF_, BG_, FT_R, BG_, BG_, BG_, BF_, BG_,
    FT_L, BG_, SL_, SM_, SM_, SM_, SR_, BG_, FT_R, BG_, BG_, BG_, BF_, BF_,
    FT_L, BG_, FL_, FM_, FM_, FM_, FR_, BG_, FT_R, BG_, BF_, BG_, BG_, BG_,
    FT_L, BG_, BG_, BG_, BG_, BG_, BG_, BG_, FT_R, BG_, BF_, BF_, BG_, BG_,
    FT_L, BG_, BF_, BF_, BF_, BF_, BF_, BG_, FT_R, BG_, BG_, BG_, BG_, BG_,
    FT_L, BG_, SL_, SM_, SM_, SM_, SR_, BG_, FT_R, BG_, BX_, BX_, BX_, BX_,
    FT_L, BG_, FL_, FM_, FM_, FM_, FR_, BG_, FT_R, BG_, BX_, BX_, BX_, BX_,
    FT_L, BG_, BG_, BG_, BG_, BG_, BG_, BG_, FT_R, BG_, BX_, BX_, BX_, BX_,
    FT_BL, FT_H, FT_BR, BG_, BG_, BG_, FT_BL, FT_H, FT_BR, BG_, BX_, BX_, BX_, BX_,
};
#undef FT_TL
#undef FT_H
#undef FT_TR
#undef FT_L
#undef FT_R
#undef FT_BL
#undef FT_BR
#undef BG_
#undef BF_
#undef BX_
#undef SL_
#undef SM_
#undef SR_
#undef FL_
#undef FM_
#undef FR_

static const struct Building sBerryFarmWideLB   = { sBerryFarmWideL, 8, 5, 16, 10, 0, 0, 0, DOOR_LOCKED, 0 };
static const struct Building sBerryFarmWideRB   = { sBerryFarmWideR, 8, 5, 16, 10, 0, 0, 0, DOOR_LOCKED, 0 };
static const struct Building sBerryFarmNarrowLB = { sBerryFarmNarrowL, 7, 5, 14, 10, 0, 0, 0, DOOR_LOCKED, 0 };
static const struct Building sBerryFarmNarrowRB = { sBerryFarmNarrowR, 7, 5, 14, 10, 0, 0, 0, DOOR_LOCKED, 0 };

// The rows of trees within the lot (blocks), and the first tree's column
// from the lot's left (the house on the left) or right side.
#define BERRY_FARM_ROW_Y0 2
#define BERRY_FARM_ROW_Y1 6
#define BERRY_FARM_X_L    8
#define BERRY_FARM_X_R    3

static bool8 BerryFarmLot(const struct FeatLot *lot)
{
    return lot->side == &sBerryFarmWideLB || lot->side == &sBerryFarmWideRB
        || lot->side == &sBerryFarmNarrowLB || lot->side == &sBerryFarmNarrowRB;
}

// FeatBegin's lot for the BERRY MASTER on a farm route: the wide field where
// the route will have room for eight trees or more (its trainers, items and
// rival leave that many places), else the narrow one, the house left or
// right (BerryFarmRefit tries the rest). FALSE (the small garden) on a route
// that is not a farm, from BERRY_FARM_TRIES tries on, or with ash drifting in
// from next door.
static bool8 BerryFarmBegin(struct Gen *gen, struct FeatLot *lot)
{
    const struct RoguePlace *p = gen->p;
    bool8 right = RAND(2), wide = ROGUE_MAX_OBJS - 1 - p->nTrainers - p->nItems - (p->rival ? 1 : 0) >= 8;
    if (!RogueGen_BerryFarmRoute(gen->r, gen->p - gen->r->places))
        return FALSE;
    if (gen->attempt >= BERRY_FARM_TRIES || gen->ashFade)
        return FALSE;
    lot->house = &sHouses[2];
    lot->side = wide ? (right ? &sBerryFarmWideRB : &sBerryFarmWideLB) : (right ? &sBerryFarmNarrowRB : &sBerryFarmNarrowLB);
    lot->sx = 0, lot->sy = 0;
    lot->uw = lot->side->uw, lot->uh = 5;
    lot->hx = right ? lot->uw - 2 : 0, lot->hy = 3;
    // The unit below the gate (the fence's middle block).
    lot->openX = right ? 2 : (wide ? 5 : 4);
    return TRUE;
}

static bool8 FeatPlaceLot(struct Gen *gen, struct FeatRoute *fr);

// A farm lot that found no room: the other ways round (the house on the
// other side, the narrow field) on the same map before it tries again.
static bool8 BerryFarmRefit(struct Gen *gen, struct FeatRoute *fr)
{
    struct FeatLot *lot = &fr->lot;
    static const struct Building *const sOrder[4] = {
        &sBerryFarmWideLB, &sBerryFarmWideRB, &sBerryFarmNarrowLB, &sBerryFarmNarrowRB,
    };
    int k, start;
    if (!BerryFarmLot(lot))
        return FALSE;
    for (start = 0; start < 4 && sOrder[start] != lot->side; start++)
        ;
    for (k = start + 1; k < start + 4; k++)
    {
        bool8 wide = (k & 3) < 2, right = k & 1;
        lot->side = sOrder[k & 3];
        lot->uw = lot->side->uw;
        lot->hx = right ? lot->uw - 2 : 0;
        lot->openX = right ? 2 : (wide ? 5 : 4);
        if (FeatPlaceLot(gen, fr))
            return TRUE;
    }
    return FALSE;
}

// The field's trees, as many as the map has room for (one place kept
// spare), both rows filled together from their middles, so a field with
// few trees still shows two even rows. Each block of the rows' middle soil
// holds one: slot 0x40 | row * 5 + k, berry tree id ROGUE_BFARM_BERRY_BASE
// + 1 + that (rogue.c).
static void BerryFarmTrees(struct Gen *gen, const struct FeatRoute *fr)
{
    struct RogueMap *m = gen->m;
    const struct FeatLot *lot = &fr->lot;
    bool8 wide = lot->side->uw == 8, right = lot->hx != 0;
    int n = wide ? 5 : 3, k;
    int x0 = (fr->bx + lot->sx) * 2 + (right ? BERRY_FARM_X_R : BERRY_FARM_X_L);
    int room = ROGUE_MAX_OBJS - 1 - m->nObjs;
    static const u8 sMiddleOut[5] = { 2, 1, 3, 0, 4 };
    for (k = 0; k < 10 && room > 0; k++)
    {
        int row = k & 1, j = sMiddleOut[k >> 1] - (wide ? 0 : 1);
        int y = (fr->by + lot->sy) * 2 + (row ? BERRY_FARM_ROW_Y1 : BERRY_FARM_ROW_Y0);
        if (j < 0 || j >= n || IsOccupied(gen, x0 + j, y))
            continue;
        AddObj(gen, ROBJ_BERRY, x0 + j, y, RDIR_SOUTH, 0x40 | (row * ROGUE_BFARM_ROW + j), 0);
        room--;
    }
}
