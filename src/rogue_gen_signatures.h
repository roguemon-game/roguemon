// Town signatures: what each generated town is known for, as Hoenn's towns
// are (Lavaridge's hot spring, Slateport's market, Mossdeep's Space Center).
// Included by rogue_gen.c, so the host build (gen_host) sees the same thing.
//
// A signature is a trade, a landmark GenTown builds on the map, a specialty
// (a gift, a counter or one small thing to do, in rogue_town.c) and people
// whose look and talk fit. Every town but the summit gets one, no two towns
// of a region share one, and the choice is a pure function of the plan on its
// own random stream: no RAM, no save, and the rest of the region is untouched.
// A town no built signature fits gets a trade told in text only.

#include "constants/species.h"
#include "constants/items.h"

#define SIG_SALT      23100 // the assignment
#define SIG_GIFT_SALT 23200 // which towns give a Pokemon, and which

// Each signature's trade and landmark in the town facts' tables
// (src/rogue_gen_facts.h: gRogueTrades, gRogueLandmarks), which hold their
// words; 0xFF: the facts choose. A fishing town's landmark is its pier by the
// sea, else its pond.
static const u8 sSigFacts[RSIG_COUNT][2] =
{
    [RSIG_NONE]       = { 0xFF, 0xFF },
    [RSIG_FISHING]    = { RTRADE_FISHING, RLANDMARK_PIER },
    [RSIG_HOT_SPRING] = { RTRADE_INNS, RLANDMARK_HOT_SPRING },
    [RSIG_SPACE]      = { RTRADE_SPACE, RLANDMARK_SPACE },
    [RSIG_MARKET]     = { RTRADE_MARKET, RLANDMARK_MARKET },
    [RSIG_MUSEUM]     = { 0xFF, RLANDMARK_MUSEUM },
    [RSIG_BERRY]      = { RTRADE_BERRIES, RLANDMARK_ORCHARD },
    [RSIG_FLOWERS]    = { RTRADE_FLOWERS, RLANDMARK_FIELDS },
    [RSIG_RANCH]      = { RTRADE_MILK, RLANDMARK_RANCH },
    [RSIG_WEATHER]    = { RTRADE_WEATHER, RLANDMARK_WEATHER },
    [RSIG_FOSSIL]     = { RTRADE_FOSSILS, RLANDMARK_DIG },
    [RSIG_MINING]     = { RTRADE_MINING, RLANDMARK_MINES }, // a working mine (the OLD MINE is a worked-out one)
    [RSIG_DOJO]       = { RTRADE_FIGHTING, RLANDMARK_DOJO },
    [RSIG_POTTERY]    = { RTRADE_POTTERY, 0xFF },
    [RSIG_WEAVING]    = { RTRADE_WEAVING, 0xFF },
    [RSIG_BAKING]     = { RTRADE_BAKING, 0xFF },
    [RSIG_TEA]        = { RTRADE_TEA, 0xFF },
    [RSIG_CARVING]    = { RTRADE_WOOD, 0xFF },
    [RSIG_HONEY]      = { RTRADE_HONEY, 0xFF },
    [RSIG_LUMBER]     = { RTRADE_LUMBER, 0xFF },
    [RSIG_SALT]       = { RTRADE_SALT, 0xFF },
};

static bool8 SigNextTo(const struct RogueRegion *r, const struct RoguePlace *p, u8 routeType)
{
    int k;
    for (k = 0; k < p->nLinks; k++)
    {
        const struct RoguePlace *q = &r->places[p->links[k].target];
        if (q->kind == RPLACE_ROUTE && q->type == routeType)
            return TRUE;
    }
    return FALSE;
}

// v1.1 sea: a maze's sea route beside it, drawn with Pacifidlog's tileset.
static bool8 SigSeaOfOtherTiles(const struct RogueRegion *r, const struct RoguePlace *p)
{
    int k;
    for (k = 0; k < p->nLinks; k++)
    {
        const struct RoguePlace *q = &r->places[p->links[k].target];
        if (q->kind == RPLACE_ROUTE && q->type == RROUTE_SEA && q->theme == RTHEME_CURRENTS)
            return TRUE;
    }
    return FALSE;
}

// On the sea: a harbor's waterfront, or a sea route setting out from it.
static bool8 SigCoast(const struct RogueRegion *r, const struct RoguePlace *p)
{
    return (p->layout == RLAYOUT_HARBOR && p->seaSide != RDIR_NONE) || SigNextTo(r, p, RROUTE_SEA)
        || SigNextTo(r, p, RROUTE_COAST);
}

// Its open ground is grass (Dewford's villages are all sand, Fallarbor's ash,
// and Fortree's treehouses stand in deep woods with no lawn).
static bool8 SigGrass(const struct RoguePlace *p)
{
    if (p->theme == RTHEME_ASH || p->theme == RTHEME_TREETOP)
        return FALSE;
    return p->theme != RTHEME_BEACH || p->type == RTOWN_GYM;
}

// (v1.1 peaks) A town's dungeon, for the signatures: the tower that moved
// out into the desert (src/rogue_gen_peaks.h) still counts in the town it was
// planned in, so no town's signature changes for it.
static s32 SigDungeonAt(const struct RogueRegion *r, u8 place)
{
    if (r->places[place].kind == RPLACE_TOWN && r->places[place].dark == ROGUE_TOWER_MOVED)
        return 0;
    return RogueGen_DungeonAt(r, place);
}

// v1.1: a village whose top is a dungeon's hill has no room left for a
// fishing pond or berry plots (seeds 139 and 173 once the routes round it
// changed with the zones).
static bool8 SigCrampedVillage(const struct RogueRegion *r, u8 place)
{
    return r->places[place].type == RTOWN_VILLAGE && SigDungeonAt(r, place) >= 0;
}

// How well a signature fits a town: 0 if it cannot stand there.
static int SigWeight(const struct RogueRegion *r, u8 place, u8 sig)
{
    const struct RoguePlace *p = &r->places[place];
    bool8 start = p->type == RTOWN_START, city = p->type == RTOWN_GYM;
    bool8 grass = SigGrass(p), coast = SigCoast(r, p), seaOnMap = p->layout == RLAYOUT_HARBOR && p->seaSide != RDIR_NONE;
    int x0, mw;
    bool8 water = SigNextTo(r, p, RROUTE_LAKE) || SigNextTo(r, p, RROUTE_RIVER);
    int w = 0;
    // The first town is a quiet farming or fishing village.
    if (start && sig != RSIG_BERRY && sig != RSIG_FLOWERS && sig != RSIG_RANCH && sig != RSIG_FISHING)
        return 0;
    // (v1.1 towns2) The raft village and the crater town lay out their own
    // maps, with no landmark lot: a trade told in words.
    if (p->theme == RTHEME_RAFT || p->theme == RTHEME_CRATER)
        return 0;
    switch (sig)
    {
    case RSIG_FISHING:
        // Piers where the sea runs east or west of a Dewford town, the
        // shore elsewhere; inland, a fishing pond needs grass round it.
        // (Fortree's woods keep the shore out of reach.)
        if (seaOnMap && p->theme != RTHEME_TREETOP)
            w = p->theme == RTHEME_BEACH || p->theme == RTHEME_HARBOR ? 45 : 30;
        else if ((water || coast) && grass && p->theme != RTHEME_SPRING && !SigCrampedVillage(r, place))
            w = 15;
        break;
    case RSIG_HOT_SPRING:
        // Under the spring town's red rock: not where a cave's mouth or a
        // dungeon's door takes the top of the town.
        // Lavaridge's corner of cliffs and spring takes the town's top left:
        // no way out north or west there, nor a cave's mouth or a dungeon's
        // door along the top.
        if (p->theme == RTHEME_SPRING && p->mouth == 0xFF && SigDungeonAt(r, place) < 0
         && !(p->layout == RLAYOUT_HARBOR && (p->seaSide == RDIR_NORTH || p->seaSide == RDIR_WEST)))
        {
            // (Clear of what the next map sees past an exit toward another
            // tileset, too: InSeamView reaches four units before the exit.)
            const struct RogueLink *n = LinkOnSide(p, RDIR_NORTH), *wl = LinkOnSide(p, RDIR_WEST);
            if ((!n || n->pos / 2 >= (r->places[n->target].theme != p->theme ? 11 : 8))
             && (!wl || wl->pos / 2 >= (r->places[wl->target].theme != p->theme ? 11 : 8)))
                w = p->type == RTOWN_HAMLET ? 30 : 60;
        }
        break;
    case RSIG_SPACE:
        // Its rocket rises into the woods along the top: not where a cave's
        // mouth or a dungeon's door takes the top of the town.
        // A harbor's sea along the top or the bottom leaves it no room.
        if (p->theme == RTHEME_ISLAND && city && p->mouth == 0xFF
         && !(p->layout == RLAYOUT_HARBOR && (p->seaSide == RDIR_NORTH || p->seaSide == RDIR_SOUTH))
         && SigDungeonAt(r, place) < 0)
            w = 60;
        break;
    case RSIG_MARKET:
        if (p->theme == RTHEME_HARBOR && p->type != RTOWN_HAMLET)
            w = city ? 50 : 25;
        break;
    case RSIG_MUSEUM:
        if ((p->theme == RTHEME_HARBOR || p->theme == RTHEME_COVE) && city)
            w = 25 + (RogueGen_LegendCount(r) ? 15 : 0);
        break;
    case RSIG_BERRY:
        // (A harbor's sea leaves no room for the plots.)
        if (grass && !seaOnMap && !SigCrampedVillage(r, place))
            w = 12 + (p->theme == RTHEME_MEADOW || p->theme == RTHEME_COVE || p->theme == RTHEME_VERDANT ? 8 : 0);
        break;
    case RSIG_FLOWERS:
        // (Not a village cramped by a dungeon's hill or by three ways out:
        // its beds never fit round the houses; seeds 173, 176 and 297.)
        if (grass && !SigCrampedVillage(r, place) && !(p->type == RTOWN_VILLAGE && p->nLinks >= 3))
            w = 12 + (p->theme == RTHEME_MEADOW || p->theme == RTHEME_VERDANT || p->theme == RTHEME_STONE ? 10 : 0);
        break;
    case RSIG_RANCH:
        if (grass && p->theme != RTHEME_HARBOR && p->theme != RTHEME_STONE && !seaOnMap)
            w = 10 + (SigNextTo(r, p, RROUTE_FIELD) || SigNextTo(r, p, RROUTE_HILLS) ? 20 : 0);
        break;
    case RSIG_WEATHER:
        // In the woods, with room for it: not squeezed by a harbor's sea.
        if (p->theme == RTHEME_TREETOP && p->type != RTOWN_HAMLET && !(p->layout == RLAYOUT_HARBOR && p->seaSide != RDIR_NONE)
         && p->mouth == 0xFF && SigDungeonAt(r, place) < 0)
            w = 60;
        break;
    case RSIG_FOSSIL:
        // (Not a village whose top is a dungeon's hill: no room left for the
        // maniac's house; v1.1 towns2, seed 90's once a raft village moved
        // its region.)
        if ((p->theme == RTHEME_ASH || p->theme == RTHEME_STONE) && !SigCrampedVillage(r, place))
            w = 25 + (SigNextTo(r, p, RROUTE_DESERT) || RogueGen_RuinsDungeon(r) >= 0 ? 15 : 0);
        break;
    case RSIG_MINING:
        // A mine in the rock face along the top of a Stone town, or a Stone
        // or Spring town's cave, whose workings the miners keep (a Spring
        // town's red rock is its own, so only by a cave).
        if (p->mouth != 0xFF ? (p->theme == RTHEME_STONE || p->theme == RTHEME_SPRING) : p->theme == RTHEME_STONE)
        {
            w = 40 + (p->mouth != 0xFF || SigNextTo(r, p, RROUTE_MOUNTAIN) || SigNextTo(r, p, RROUTE_HILLS) ? 25 : 0);
            // (A rock face along the top: not in a harbor, whose beach would
            // run under it.)
            if (p->mouth == 0xFF && (SigDungeonAt(r, place) >= 0 || !DungeonMesaSpan(r, p, &x0, &mw)
                                     || (p->layout == RLAYOUT_HARBOR && p->seaSide != RDIR_NONE)))
                w = 0;
        }
        break;
    case RSIG_DOJO:
        // (Not among a Treetop village's few lots in the woods: the weather
        // station's town, and too tight for a fourth house.)
        // (v1.1 towns2: nor a Treetop town on the sea, whose woods the sea
        // squeezes: seed 64's, once a raft village moved its region.)
        // Nor a village whose top is a dungeon's hill (seed 90's, likewise).
        w = p->type == RTOWN_HAMLET ? 15 : (p->theme == RTHEME_TREETOP && (p->type == RTOWN_VILLAGE
                                                                            || (p->layout == RLAYOUT_HARBOR && p->seaSide != RDIR_NONE)) ? 0 : 8);
        if (SigCrampedVillage(r, place))
            w = 0;
        // (v1.1 sea: nor in a Treetop harbor below Victory Road, or beside
        // a maze's sea route (Pacifidlog's tileset): the sea, the woods and
        // the road up or the view kept clear across the seam leave no room
        // for a hall.)
        if (p->theme == RTHEME_TREETOP && seaOnMap && (SigNextTo(r, p, RROUTE_MOUNTAIN) || SigSeaOfOtherTiles(r, p)))
            w = 0;
        break;
    }
    return w;
}

// Every town's signature, worked out together (the summit's stays NONE),
// one try of it.
static void AssignSignaturesTry(const struct RogueRegion *r, u8 *out, u32 salt)
{
    u32 rng = RogueRng_Hash(r->seed, salt);
    u8 order[ROGUE_MAX_TOWNS], keys[ROGUE_MAX_TOWNS];
    u32 used = 0;
    int i, j, n = r->nTowns, s;
    u8 trades[RSIG_COUNT - RSIG_BUILT_COUNT], nTrades = 0;

    for (i = 0; i < n; i++)
    {
        out[i] = RSIG_NONE;
        keys[i] = RogueRng_Range(&rng, 256);
    }
    // The start town first, then the towns with the fewest choices left (ties
    // broken at random), so the picky ones are not left with nothing.
    for (;;)
    {
        int best = -1, bestCount = 99, count, total;
        for (i = 0; i < n; i++)
        {
            const struct RoguePlace *p = &r->places[r->towns[i]];
            if (out[i] != RSIG_NONE || p->type == RTOWN_SUMMIT)
                continue;
            for (count = 0, s = 1; s < RSIG_BUILT_COUNT; s++)
                if (!(used & (1 << s)) && SigWeight(r, r->towns[i], s))
                    count++;
            if (p->type == RTOWN_START)
                count = -1;
            if (count < bestCount || (count == bestCount && keys[i] < keys[best]))
            {
                best = i;
                bestCount = count;
            }
        }
        if (best < 0)
            break;
        if (bestCount == 0)
        {
            // Nothing built fits: a trade in text, below.
            out[best] = RSIG_COUNT;
            continue;
        }
        for (total = 0, s = 1; s < RSIG_BUILT_COUNT; s++)
            if (!(used & (1 << s)))
                total += SigWeight(r, r->towns[best], s);
        total = RogueRng_Range(&rng, total);
        for (s = 1; s < RSIG_BUILT_COUNT; s++)
        {
            if (used & (1 << s))
                continue;
            total -= SigWeight(r, r->towns[best], s);
            if (total < 0)
                break;
        }
        out[best] = s;
        used |= 1 << s;
    }
    // Text-only trades, shuffled; salt pans only where the sea is on the town's map.
    for (s = RSIG_BUILT_COUNT; s < RSIG_COUNT; s++)
        trades[nTrades++] = s;
    for (i = nTrades - 1; i > 0; i--)
    {
        u8 t;
        j = RogueRng_Range(&rng, i + 1);
        t = trades[i];
        trades[i] = trades[j];
        trades[j] = t;
    }
    for (i = 0, j = 0; i < n; i++)
    {
        int k;
        if (out[i] != RSIG_COUNT)
            continue;
        for (k = j; k < nTrades; k++)
            if (trades[k] != RSIG_SALT || RogueGen_TownSeaOnMap(r, r->towns[i]))
                break;
        if (k == nTrades)
            k = j;
        out[i] = trades[k];
        trades[k] = trades[j];
        trades[j++] = out[i];
    }
}

// How badly a try places its trades told only in text: fewer is better, and
// those there are should lie apart along the road, not bunched at its end.
static int SigTextPenalty(const struct RogueRegion *r, const u8 *sigs)
{
    int i, j, n = 0, sum = 0, last = 0, penalty = 0;
    for (i = 0; i < r->nTowns; i++)
    {
        if (r->places[r->towns[i]].type == RTOWN_SUMMIT)
            continue;
        last++;
        if (sigs[i] < RSIG_BUILT_COUNT)
            continue;
        n++;
        sum += last;
        for (j = i + 1; j < r->nTowns && j <= i + 2; j++)
            if (sigs[j] >= RSIG_BUILT_COUNT && sigs[j] < RSIG_COUNT)
                penalty += 30; // two in a row, or with one between
    }
    if (n == 0)
        return 0;
    // How far their mean lies from the middle of the road, in towns.
    return n * 1000 + penalty + Abs(sum * 2 - n * (last + 1)) * 10 / n;
}

#define SIG_TRIES 8

static ROGUE_EWRAM u32 sSigSeed = 0;
static ROGUE_EWRAM u8 sSigCache[ROGUE_MAX_TOWNS] = {0};

// The best of a few tries, kept for the region last asked about.
static void AssignSignatures(const struct RogueRegion *r, u8 *out)
{
    u8 best[ROGUE_MAX_TOWNS], cur[ROGUE_MAX_TOWNS];
    int k, bestPenalty = 0x7FFFFFFF, i;
    if (sSigSeed == r->seed + 1 && sSigCache[0] != 0)
    {
        memcpy(out, sSigCache, ROGUE_MAX_TOWNS);
        return;
    }
    for (k = 0; k < SIG_TRIES; k++)
    {
        int pen;
        AssignSignaturesTry(r, cur, SIG_SALT + k);
        pen = SigTextPenalty(r, cur);
        if (pen < bestPenalty)
        {
            bestPenalty = pen;
            memcpy(best, cur, ROGUE_MAX_TOWNS);
        }
    }
    for (i = r->nTowns; i < ROGUE_MAX_TOWNS; i++)
        best[i] = RSIG_NONE;
    memcpy(out, best, ROGUE_MAX_TOWNS);
    memcpy(sSigCache, best, ROGUE_MAX_TOWNS);
    sSigSeed = r->seed + 1;
}

u8 RogueGen_TownSignature(const struct RogueRegion *r, u8 place)
{
    u8 sigs[ROGUE_MAX_TOWNS];
    int i;
    if (place >= r->nPlaces || r->places[place].kind != RPLACE_TOWN || r->places[place].type == RTOWN_SUMMIT)
        return RSIG_NONE;
    AssignSignatures(r, sigs);
    for (i = 0; i < r->nTowns; i++)
        if (r->towns[i] == place)
            return sigs[i];
    return RSIG_NONE;
}

u8 RogueGen_SignatureTown(const struct RogueRegion *r, u8 signature)
{
    u8 sigs[ROGUE_MAX_TOWNS];
    int i;
    AssignSignatures(r, sigs);
    for (i = 0; i < r->nTowns; i++)
        if (sigs[i] == signature && signature != RSIG_NONE)
            return r->towns[i];
    return 0xFF;
}

u8 RogueGen_SignatureTrade(const struct RogueRegion *r, u8 place)
{
    return sSigFacts[RogueGen_TownSignature(r, place)][0];
}

u8 RogueGen_SignatureLandmarkId(const struct RogueRegion *r, u8 place)
{
    u8 sig = RogueGen_TownSignature(r, place);
    const struct RoguePlace *p = &r->places[place];
    if (sig == RSIG_FISHING && !(p->layout == RLAYOUT_HARBOR && p->seaSide != RDIR_NONE))
        return RLANDMARK_POND;
    return sSigFacts[sig][1];
}

void RogueGen_SignatureLandmark(const struct RogueRegion *r, u8 place, char *out)
{
    struct RogueTownFacts f;
    RogueGen_TownFacts(r, place, &f);
    RogueGen_LandmarkName(r, &f, out);
}

// ---------------------------------------------------------------------------
// Gift Pokemon. Some signatures' people give a Pokemon once per run, as
// Lavaridge's old woman gives an Egg, Steven a Beldum and the Weather
// Institute a Castform. Which towns give one, and which Pokemon, is rolled
// per region; a gift never uses up the town's catch.

#define SIG_GIFT_POOL 5

static const u16 sSignatureGifts[RSIG_BUILT_COUNT][SIG_GIFT_POOL] =
{
    // A fisherman's odd catch.
    [RSIG_FISHING]    = { SPECIES_FEEBAS, SPECIES_LUVDISC, SPECIES_CLAMPERL, SPECIES_CORSOLA, SPECIES_WISHIWASHI },
    // The old woman's Egg, as in Lavaridge, or a warm-blooded friend.
    [RSIG_HOT_SPRING] = { SPECIES_WYNAUT, SPECIES_NUMEL, SPECIES_SLUGMA, SPECIES_TORKOAL, SPECIES_WYNAUT },
    // From the stars, or Steven's Beldum.
    [RSIG_SPACE]      = { SPECIES_CLEFFA, SPECIES_LUNATONE, SPECIES_SOLROCK, SPECIES_BELDUM, SPECIES_MINIOR },
    // A merchant's lucky charm.
    [RSIG_MARKET]     = { SPECIES_MEOWTH, SPECIES_KECLEON, SPECIES_SPINDA, SPECIES_MEOWTH, SPECIES_KECLEON },
    // An exhibit brought back to life.
    [RSIG_MUSEUM]     = { SPECIES_OMANYTE, SPECIES_KABUTO, SPECIES_RELICANTH, SPECIES_BALTOY, SPECIES_ARCHEN },
    // A fossil from the dig, brought back to life.
    [RSIG_FOSSIL]     = { SPECIES_LILEEP, SPECIES_ANORITH, SPECIES_CRANIDOS, SPECIES_SHIELDON, SPECIES_TIRTOUGA },
    [RSIG_FLOWERS]    = { SPECIES_ROSELIA, SPECIES_BUDEW, SPECIES_FLABEBE, SPECIES_PETILIL, SPECIES_COMBEE },
    [RSIG_RANCH]      = { SPECIES_MAREEP, SPECIES_MILTANK, SPECIES_PONYTA, SPECIES_WOOLOO, SPECIES_SKIDDO },
    [RSIG_WEATHER]    = { SPECIES_CASTFORM, SPECIES_CASTFORM, SPECIES_CASTFORM, SPECIES_CASTFORM, SPECIES_CASTFORM },
    [RSIG_MINING]     = { SPECIES_ARON, SPECIES_NOSEPASS, SPECIES_ROGGENROLA, SPECIES_DRILBUR, SPECIES_ARON },
    // The master's own pupil.
    [RSIG_DOJO]       = { SPECIES_MAKUHITA, SPECIES_TYROGUE, SPECIES_MEDITITE, SPECIES_MACHOP, SPECIES_MIENFOO },
};

// The towns that give a Pokemon this region (bit per town number): two to
// four of the towns whose signature has gifts.
static u16 SigGiftTowns(const struct RogueRegion *r, const u8 *sigs)
{
    u32 rng = RogueRng_Hash(r->seed, SIG_GIFT_SALT);
    u8 cand[ROGUE_MAX_TOWNS], nCand = 0, want, i;
    u16 mask = 0;
    for (i = 0; i < r->nTowns; i++)
        if (sigs[i] < RSIG_BUILT_COUNT && sSignatureGifts[sigs[i]][0] != SPECIES_NONE)
            cand[nCand++] = i;
    want = 2 + RogueRng_Range(&rng, 3);
    // The Weather Institute always has its Castform to give, as in Emerald.
    for (i = 0; i < nCand; i++)
        if (sigs[cand[i]] == RSIG_WEATHER)
        {
            mask |= 1 << cand[i];
            cand[i--] = cand[--nCand];
            want--;
        }
    while (want > 0 && nCand > 0)
    {
        i = RogueRng_Range(&rng, nCand);
        mask |= 1 << cand[i];
        cand[i] = cand[--nCand];
        want--;
    }
    return mask;
}

u16 RogueGen_SignatureGift(const struct RogueRegion *r, u8 place, u8 *level)
{
    u8 sigs[ROGUE_MAX_TOWNS], n, i;
    const struct RoguePlace *p = &r->places[place];
    u32 rng;
    int lv;
    if (place >= r->nPlaces || p->kind != RPLACE_TOWN || p->type == RTOWN_SUMMIT)
        return SPECIES_NONE;
    AssignSignatures(r, sigs);
    for (n = 0; n < r->nTowns && r->towns[n] != place; n++)
        ;
    if (n == r->nTowns || !(SigGiftTowns(r, sigs) & (1 << n)))
        return SPECIES_NONE;
    rng = RogueRng_Hash(r->seed, SIG_GIFT_SALT + 1 + n);
    i = RogueRng_Range(&rng, SIG_GIFT_POOL);
    // As the spare starters are given: a few levels under the town's
    // trainers, so the gift helps without carrying the run.
    lv = (int)p->level - 3;
    *level = lv < 5 ? 5 : lv;
    return sSignatureGifts[sigs[n]][i];
}

u16 RogueGen_SignatureItem(const struct RogueRegion *r, u8 place, u8 *qty)
{
    switch (RogueGen_TownSignature(r, place))
    {
    case RSIG_HOT_SPRING:
        *qty = 1;
        return ITEM_LAVA_COOKIE;
    case RSIG_RANCH:
        *qty = 2;
        return ITEM_MOOMOO_MILK;
    case RSIG_DOJO:
    {
        // The MASTER's reward once beaten, where the town has no pupil to
        // give (src/rogue_town.c Rogue_PrepareSigFolk).
        u8 level;
        if (RogueGen_SignatureGift(r, place, &level) != SPECIES_NONE)
            break;
        *qty = 1;
        return ITEM_BLACK_BELT;
    }
    }
    *qty = 0;
    return ITEM_NONE;
}

// ---------------------------------------------------------------------------
// On the map: GenTown builds the landmark (struct SigTown in rogue_gen.c).
// Buildings you can enter (the Space Center, the Weather Institute, a museum,
// the fossil maniac's house, a dojo) take one of the town's house slots and
// open onto RINT_SIGNATURE; the rest are lots of scenery (the market, a
// pasture, berry plots, flower beds, the hot spring, a mine, piers or a pond)
// with the town's signature people and Pokemon about them.

#include "data/rogue_signature_stamps.h"

static const struct Building sSigSpaceCenter = { sSigStamp_SpaceCenter, 5, 7, 9, 14, 0, 4, 13, DOOR_SIGNATURE, 0 };
static const struct Building sSigWeather     = { sSigStamp_WeatherInstitute, 6, 4, 11, 7, 1, 5, 6, DOOR_SIGNATURE, 0 };
static const struct Building sSigOceanic     = { sSigStamp_OceanicMuseum, 3, 3, 6, 5, 1, 2, 4, DOOR_SIGNATURE, 0 };
// Its doorway runs down into its base row: the player walks up to the door from below that.
static const struct Building sSigLilyMuseum  = { sSigStamp_LilycoveMuseum, 5, 4, 10, 7, 1, 4, 5, DOOR_SIGNATURE, 0, 7 };
static const struct Building sSigFossilHouse = { sSigStamp_FossilHouse, 2, 2, 4, 4, 0, 1, 3, DOOR_SIGNATURE, 0 };

// A wooden pasture of General's fence (every tileset has it), 10x6 blocks,
// open at the bottom in the middle between two corner posts (a fence
// closes, as tile_polish.py has it); and a berry farm's fenced plots.
#define SG_ 0x3001 // grass
static const u16 sSigPasture[10 * 6] = {
    F_TL, F_H, F_H, F_H, F_H, F_H, F_H, F_H, F_H, F_TR,
    F_L,  SG_, SG_, SG_, SG_, SG_, SG_, SG_, SG_, F_R,
    F_L,  SG_, SG_, SG_, SG_, SG_, SG_, SG_, SG_, F_R,
    F_L,  SG_, SG_, SG_, SG_, SG_, SG_, SG_, SG_, F_R,
    F_L,  SG_, SG_, SG_, SG_, SG_, SG_, SG_, SG_, F_R,
    F_BL, F_H, F_H, F_BR, SG_, SG_, F_BL, F_H, F_H, F_BR,
};
static const u16 sSigBerryPlots[10 * 5] = {
    F_TL, F_H,      F_H,    F_H,    F_H,      F_H,      F_H,    F_H,    F_H,      F_TR,
    F_L,  B_SOIL_L, B_SOIL, B_SOIL, B_SOIL_R, B_SOIL_L, B_SOIL, B_SOIL, B_SOIL_R, F_R,
    F_L,  B_SOIL_FRONT_L, B_SOIL_FRONT, B_SOIL_FRONT, B_SOIL_FRONT_R, B_SOIL_FRONT_L, B_SOIL_FRONT, B_SOIL_FRONT, B_SOIL_FRONT_R, F_R,
    F_L,  SG_,      SG_,    SG_,    SG_,      SG_,      SG_,    SG_,    SG_,      F_R,
    F_BL, F_H,      F_H,    F_BR,   SG_,      SG_,      F_BL,   F_H,    F_H,      F_BR,
};
static const u16 sSigPastureSmall[6 * 5] = {
    F_TL, F_H, F_H, F_H, F_H, F_TR,
    F_L,  SG_, SG_, SG_, SG_, F_R,
    F_L,  SG_, SG_, SG_, SG_, F_R,
    F_L,  SG_, SG_, SG_, SG_, F_R,
    F_BL, F_BR, SG_, SG_, F_BL, F_BR,
};
static const u16 sSigBerryPlotSmall[6 * 5] = {
    F_TL, F_H,      F_H,    F_H,    F_H,      F_TR,
    F_L,  B_SOIL_L, B_SOIL, B_SOIL, B_SOIL_R, F_R,
    F_L,  B_SOIL_FRONT_L, B_SOIL_FRONT, B_SOIL_FRONT, B_SOIL_FRONT_R, F_R,
    F_L,  SG_,      SG_,    SG_,    SG_,      F_R,
    F_BL, F_BR,     SG_,    SG_,    F_BL,     F_BR,
};
// A village market's stall: Slateport's (sThemeHarbor_Market) on the lawn,
// its foot of cobbles (it stands on a plaza's) grass.
static const u16 sSigStall[6 * 4] = {
    0x3205, 0x320D, 0x320E, 0x320F, 0x3227, 0x062B,
    0x3235, 0x32F8, 0x32F9, 0x32FA, 0x322F, 0x073E,
    0x063D, 0x061B, 0x061B, 0x3285, 0x066C, 0x061B,
    SG_,    0x060C, SG_,    SG_,    SG_,    SG_,
};
#undef SG_
// Lots laid like buildings along a street, open at the bottom where the
// street runs past (DOOR_SIGLOT): the market, a pasture, berry plots.
static const struct Building sSigMarketLot   = { sSigStamp_Market, 7, 4, 14, 8, 0, 6, 7, DOOR_SIGLOT, 0 };
static const struct Building sSigPastureLot  = { sSigPasture, 5, 3, 10, 6, 0, 4, 5, DOOR_SIGLOT, 0 };
static const struct Building sSigPastureLotS = { sSigPastureSmall, 3, 3, 6, 5, 1, 2, 4, DOOR_SIGLOT, 0 };
static const struct Building sSigBerryLot    = { sSigBerryPlots, 5, 3, 10, 5, 1, 4, 4, DOOR_SIGLOT, 0 };
static const struct Building sSigBerryLotS   = { sSigBerryPlotSmall, 3, 3, 6, 5, 1, 2, 4, DOOR_SIGLOT, 0 };
// Berry trees on the plots (blocks inside the lot), as on Route 123.
static const u8 sSigBerrySpots[ROGUE_FARM_BERRIES][2] = { { 2, 1 }, { 3, 1 }, { 6, 1 }, { 7, 1 } };
// Where a pasture's Pokemon graze (blocks inside the lot).
static const u8 sSigGrazeSpots[3][2] = { { 3, 2 }, { 6, 3 }, { 2, 4 } };

// TRUE for a town with a landmark to build (and so more tries to fit it).
static bool8 SigTownTries(const struct RogueRegion *r, u8 place)
{
    u8 sig;
    if (r->places[place].kind != RPLACE_TOWN)
        return FALSE;
    sig = RogueGen_TownSignature(r, place);
    return sig != RSIG_NONE && sig < RSIG_BUILT_COUNT;
}

// Tries before a town is built without its landmark: more where an exit
// toward another tileset keeps part of the town clear (SIG_GIVE_UP_SEAM).
static u32 SigGiveUp(const struct RogueRegion *r, u8 place)
{
    const struct RoguePlace *p = &r->places[place];
    int i;
    for (i = 0; i < p->nLinks; i++)
        if (r->places[p->links[i].target].theme != p->theme)
            return SIG_GIVE_UP_SEAM;
    return SIG_GIVE_UP;
}

static bool8 SigEnterable(u8 sig)
{
    return sig == RSIG_SPACE || sig == RSIG_WEATHER || sig == RSIG_MUSEUM || sig == RSIG_FOSSIL || sig == RSIG_DOJO;
}

static void SigTownStart(struct Gen *gen, struct SigTown *st)
{
    memset(st, 0, sizeof(*st));
    st->sig = RogueGen_TownSignature(gen->r, gen->m->place);
    st->lx = st->ly = -1;
}

// A hamlet's style follows its trade: fishers by a pond, a flower garden.
static u8 SigHamletStyle(const struct SigTown *st, u8 style, u8 ground)
{
    if (ground != TGROUND_GRASS)
        return style;
    if (st->sig == RSIG_FISHING)
        return HAMLET_POND;
    if (st->sig == RSIG_FLOWERS)
        return HAMLET_GARDEN;
    if (st->sig == RSIG_RANCH || st->sig == RSIG_BERRY)
        return style == HAMLET_GROVE ? HAMLET_GARDEN : style;
    return style;
}

// Attempts after which a landmark takes its small form, and after which a
// pasture or plots may go anywhere on the lawn.
#define SIG_SMALL_FROM    45
// A city's market lot is large: it gives way to the stalls sooner, before a
// map's later tries let the rest of the town loosen (rogue_gen.c, from 40).
#define SIG_MARKET_SMALL_FROM 30
#define SIG_ANYWHERE_FROM 55

// The landmark building the town must build, or NULL. scratch holds a
// theme house standing in as the dojo or the fossil maniac's house.
static const struct Building *SigBuilding(struct Gen *gen, const struct TownTheme *th, const struct SigTown *st,
                                          struct Building *scratch)
{
    int i, best = 0;
    // A crowded town that keeps failing to fit its landmark gets its small
    // form, so the landmark always stands (its facts and its people's lines
    // name it): a building you enter becomes one of the theme's own houses
    // opening onto the same interior; a market becomes a stall or two; a
    // pasture or plots go anywhere on the lawn (SigAfterBuildings).
    if (st->sig == RSIG_MARKET && gen->attempt >= SIG_MARKET_SMALL_FROM)
        return NULL;
    if (gen->attempt >= SIG_SMALL_FROM)
    {
        switch (st->sig)
        {
        case RSIG_SPACE:
        case RSIG_WEATHER:
        case RSIG_MUSEUM:
        case RSIG_FOSSIL:
        case RSIG_DOJO:
            for (i = 1; i < th->nHouses; i++)
                if (th->houses[i].uw * th->houses[i].uh < th->houses[best].uw * th->houses[best].uh)
                    best = i;
            *scratch = th->houses[best];
            return scratch;
        case RSIG_MARKET:
            return NULL;
        case RSIG_RANCH:
        case RSIG_BERRY:
            if (gen->attempt >= SIG_ANYWHERE_FROM)
                return NULL;
            break;
        }
    }
    switch (st->sig)
    {
    case RSIG_SPACE:
        return &sSigSpaceCenter;
    case RSIG_WEATHER:
        return &sSigWeather;
    case RSIG_MUSEUM:
        return gen->p->theme == RTHEME_COVE ? &sSigLilyMuseum : &sSigOceanic;
    case RSIG_MARKET:
        return gen->p->type == RTOWN_GYM ? &sSigMarketLot : NULL;
    case RSIG_RANCH:
        return gen->attempt < 15 && gen->p->type != RTOWN_HAMLET ? &sSigPastureLot : &sSigPastureLotS;
    case RSIG_BERRY:
        return gen->attempt < 15 && gen->p->type != RTOWN_HAMLET ? &sSigBerryLot : &sSigBerryLotS;
    case RSIG_FOSSIL:
        if (gen->p->theme == RTHEME_ASH)
            return &sSigFossilHouse;
        // fall through: a stone town's own house
    case RSIG_DOJO:
        // The theme's widest house: a hall for the dojo.
        for (i = 1; i < th->nHouses; i++)
            if (th->houses[i].uw > th->houses[best].uw)
                best = i;
        *scratch = th->houses[best];
        return scratch;
    }
    return NULL;
}

// A tall landmark (the Space Center) on the north side of a street, its top
// rising into the band of woods along the top of the town (rows above top),
// as Mossdeep's rocket stands up against the sky.
static bool8 SigPlaceTall(struct Gen *gen, const struct Building *b, const u8 *streetY, int nStreets, int top, struct Placed *out)
{
    u8 cand[64]; // street * 32 + x
    int n = 0, s, x, y;
    for (s = 0; s < nStreets; s++)
        for (x = 1; x + b->uw < gen->cw && n < 64; x++)
            if (Cell(gen, x + b->doorX / 2, streetY[s]) == C_ROAD && streetY[s] - b->uh >= 1)
                cand[n++] = s * 32 + x;
    while (n > 0)
    {
        int k = RAND(n), c = cand[k], bx = c % 32, by = streetY[c / 32] - b->uh;
        bool8 ok = TRUE;
        cand[k] = cand[--n];
        for (y = by - 1; y <= by + b->uh && ok; y++)
            for (x = bx - 1; x <= bx + b->uw && ok; x++)
            {
                u8 cell = Cell(gen, x, y);
                bool8 inside = x >= bx && x < bx + b->uw && y >= by && y < by + b->uh;
                if (cell == C_BLDG || cell == C_DECOR || cell == C_WATER || cell == C_ROCK || cell == C_SEA)
                    ok = FALSE;
                else if (inside && cell != C_OPEN && !(cell == C_TREE && y < top && y >= 1 && !IsEdge(gen, x, y)))
                    ok = FALSE;
            }
        if (!ok || !LotRingClear(gen, b, bx, by) || InSeamView(gen, bx, by, b->uw, b->uh))
            continue;
        for (y = by; y < by + b->uh; y++)
            for (x = bx; x < bx + b->uw; x++)
                SetCell(gen, x, y, C_BLDG);
        out->b = b;
        out->bx = bx;
        out->by = by;
        out->door = b->door;
        return TRUE;
    }
    return FALSE;
}

// One lot of scenery on open ground (as PlacePropOnce), near a road unless
// anywhere; its top left unit in *ux, *uy. The ground just below it is kept
// clear of trees, whose crowns would reach up over its bottom row.
static bool8 SigPlaceLot(struct Gen *gen, const u16 *blocks, int w, int h, int startX, int startY, bool8 anywhere,
                         int *outX, int *outY)
{
    int tries, x, y, uw = (w + 1) / 2, uh = (h + 1) / 2;
    for (tries = 0; tries < 150; tries++)
    {
        YIELD();
        int ux = 1 + RAND(gen->cw - 1 - uw), uy = 1 + RAND(gen->ch - 2 - uh);
        bool8 ok = TRUE, nearRoad = FALSE;
        int before;
        for (y = uy; y < uy + uh && ok; y++)
            for (x = ux; x < ux + uw && ok; x++)
                if (Cell(gen, x, y) != C_OPEN || IsEdge(gen, x, y) || IsBeachU(gen, x, y)
                 || (gen->terraced && UpperU(gen, x, y) != UpperU(gen, ux, uy)) || RimAbove(gen, x, y))
                    ok = FALSE;
        if (!ok || !FenceRingClear(gen, ux, uy, uw, uh) || InSeamView(gen, ux, uy, uw, uh))
            continue;
        for (y = uy - 1; y <= uy + uh; y++)
            for (x = ux - 1; x <= ux + uw; x++)
                if (Cell(gen, x, y) == C_ROAD)
                    nearRoad = TRUE;
        if (!nearRoad && !anywhere && tries < 100)
            continue;
        before = FloodReachable(gen, startX, startY);
        for (y = uy; y < uy + uh; y++)
            for (x = ux; x < ux + uw; x++)
                SetCell(gen, x, y, C_DECOR);
        if (FloodReachable(gen, startX, startY) != before - uw * uh || !AddDecor(gen, blocks, ux * 2, uy * 2, w, h))
        {
            for (y = uy; y < uy + uh; y++)
                for (x = ux; x < ux + uw; x++)
                    SetCell(gen, x, y, C_OPEN);
            continue;
        }
        for (x = ux; x < ux + uw; x++)
            if (Cell(gen, x, uy + uh) == C_OPEN && !IsEdge(gen, x, uy + uh))
                SetCell(gen, x, uy + uh, C_KEEP);
        *outX = ux;
        *outY = uy;
        return TRUE;
    }
    return FALSE;
}

// The mine: a rock face along the top with a mouth in it, as a dungeon's
// (PlaceCaveHill), but no way in: a miner stands in the mouth. Its front unit
// in st->lx, st->ly.
static bool8 SigPlaceMine(struct Gen *gen, struct SigTown *st)
{
    int x0, w, y, mouth;
    if (!DungeonMesaSpan(gen->r, gen->p, &x0, &w))
        return FALSE;
    if (w > 8)
    {
        x0 += RAND(w - 7);
        w = 8;
    }
    PlaceCaveHill(gen, x0, w, &mouth, &y);
    st->lx = mouth;
    st->ly = 3;
    st->kind = 1;
    return TRUE;
}

// Under a spring town's mesa: a run of six columns where the mesa's lowest
// row is its plain bottom face, with open ground below for the pool and the
// sand bath. The pool's top row takes the place of that face.
static bool8 SigPlaceSpring(struct Gen *gen, struct SigTown *st, int startX, int startY)
{
    int x, y, before, n = 0, uw = (2 + SIG_STAMP_HOTSPRING_W + 1) / 2, uh = (2 + SIG_STAMP_HOTSPRING_H + 1) / 2;
    // Lavaridge's corner in the town's top left, inside its border of trees.
    for (y = 1; y < uh; y++)
        for (x = 1; x < uw; x++)
            if (Cell(gen, x, y) == C_ROAD || Cell(gen, x, y) == C_BLDG || Cell(gen, x, y) == C_ROCK || Cell(gen, x, y) == C_SEA)
                return FALSE;
    before = FloodReachable(gen, startX, startY);
    for (y = 1; y < uh; y++)
        for (x = 1; x < uw; x++)
        {
            n += Cell(gen, x, y) == C_OPEN || Cell(gen, x, y) == C_KEEP;
            SetCell(gen, x, y, C_DECOR);
        }
    if (FloodReachable(gen, startX, startY) != before - n)
    {
        for (y = 1; y < uh; y++)
            for (x = 1; x < uw; x++)
                SetCell(gen, x, y, C_OPEN);
        return FALSE;
    }
    // The way up to its steps (the right of its cliff), and the sand bath's
    // edge, stay open ground.
    for (y = 1; y <= uh; y++)
        if (Cell(gen, uw, y) == C_OPEN || Cell(gen, uw, y) == C_TREE)
            SetCell(gen, uw, y, C_KEEP);
    for (x = 1; x <= uw; x++)
        if (Cell(gen, x, uh) == C_OPEN || Cell(gen, x, uh) == C_TREE)
            SetCell(gen, x, uh, C_KEEP);
    st->lx = 2;
    st->ly = 2;
    st->kind = 2;
    return TRUE;
}

// Lots that go in before the buildings: the market, the hot spring.
static bool8 SigBeforeBuildings(struct Gen *gen, struct SigTown *st, int startX, int startY)
{
    int ux, uy;
    if (st->sig == RSIG_HOT_SPRING)
        return SigPlaceSpring(gen, st, startX, startY);
    // Inland, a pond to fish in, before the houses take the lawns.
    if (st->sig == RSIG_FISHING && !(gen->p->layout == RLAYOUT_HARBOR && gen->p->seaSide != RDIR_NONE)
     && gen->townGround == TGROUND_GRASS)
    {
        int x, y;
        for (y = 0; y < gen->ch; y++)
            for (x = 0; x < gen->cw; x++)
                if (Cell(gen, x, y) == C_WATER)
                    return TRUE;
        return PlaceLake(gen, gen->attempt >= SIG_SMALL_FROM ? 2 : 3 + RAND(2), 2, TRUE);
    }
    return TRUE;
}

// Lots that go in after the buildings, among the lawns. FALSE if the town's
// landmark found no room.
static bool8 SigAfterBuildings(struct Gen *gen, struct SigTown *st, const struct TownTheme *th, int startX, int startY)
{
    int ux, uy, i, n;
    bool8 hamlet = gen->p->type == RTOWN_HAMLET;
    switch (st->sig)
    {
    case RSIG_RANCH:
    case RSIG_BERRY:
        // The small form: anywhere on the lawn.
        if (gen->attempt < SIG_ANYWHERE_FROM)
            return TRUE;
        if (!SigPlaceLot(gen, st->sig == RSIG_RANCH ? sSigPastureSmall : sSigBerryPlotSmall, 6, 5, startX, startY, TRUE, &ux, &uy))
            return FALSE;
        st->lx = ux;
        st->ly = uy;
        st->lotY = 0;
        st->lotW = 6;
        st->lotH = 5;
        st->small = TRUE;
        st->kind = 4;
        return TRUE;
    case RSIG_FLOWERS:
        // Two or three fenced beds; a hamlet's garden is its own. A crowded
        // town makes do with one, anywhere.
        n = hamlet || gen->attempt >= SIG_SMALL_FROM ? 1 : 2 + (gen->p->type == RTOWN_GYM);
        for (i = 0; i < n; i++)
            if (SigPlaceLot(gen, sFenceGarden, 6, 4, startX, startY, i > 0 || gen->attempt >= SIG_SMALL_FROM, &ux, &uy) && st->lx < 0)
            {
                st->lx = ux;
                st->ly = uy;
                st->lotW = 6;
                st->lotH = 4;
            }
        st->kind = st->lx >= 0 ? 4 : 0;
        return st->lx >= 0;
    case RSIG_MARKET:
        if (gen->p->type == RTOWN_GYM && gen->attempt < SIG_MARKET_SMALL_FROM)
            return TRUE;
        // A village market: a stall or two (Slateport's).
        for (i = 0, n = 0; i < 2; i++)
            if (SigPlaceLot(gen, sSigStall, 6, 4, startX, startY, gen->attempt >= SIG_MARKET_SMALL_FROM, &ux, &uy))
            {
                if (st->lx < 0)
                    st->lx = ux, st->ly = uy, st->lotW = 6, st->lotH = 4;
                n++;
            }
        st->kind = n ? 4 : 0;
        return n > 0;
    case RSIG_FISHING:
        return TRUE;
    case RSIG_FOSSIL:
        // Craters and boulders by the dig (Fallarbor's), in an ash town.
        if (gen->p->theme == RTHEME_ASH)
        {
            PlacePropOnce(gen, &th->decor[0], startX, startY, TRUE, 2);
            PlacePropOnce(gen, &th->decor[1], startX, startY, TRUE, 2);
        }
        return TRUE;
    }
    return TRUE;
}

// TRUE on the step in front of a door or a sign (never stand there).
static bool8 SigByDoor(struct Gen *gen, int x, int y)
{
    int i;
    for (i = 0; i < gen->m->nWarps; i++)
        if (gen->m->warps[i].x == x && (gen->m->warps[i].y == y - 1 || gen->m->warps[i].y == y - 2))
            return TRUE;
    return FALSE;
}

// A free block near (fx, fy), within reach, where someone can stand beside
// the road as the townsfolk do (FindSpotBesideRoad): the nearest stretch of
// road first.
static bool8 SigSpotNear(struct Gen *gen, int fx, int fy, int reach, int *ox, int *oy, bool8 inView)
{
    // As the townsfolk do: standing closer if they must, and at last in view
    // of the next map.
    static const u8 sCrowd[4] = { 0, 2, 1, 1 };
    int d, k, pass;
    u8 facing;
    for (pass = 0; pass < (inView ? 4 : 3); pass++)
    {
        gen->crowdRadius = sCrowd[pass];
        gen->inView = pass == 3;
        for (d = 0; d <= reach; d++)
            for (k = 0; k < gen->nPath; k++)
            {
                int px = gen->path[k] % gen->cw, py = gen->path[k] / gen->cw, tries;
                if (Abs(px - (fx >> 1)) + Abs(py - (fy >> 1)) != d)
                    continue;
                for (tries = 0; tries < 3; tries++)
                    if (FindSpotBesideRoad(gen, px, py, ox, oy, &facing) && !SigByDoor(gen, *ox, *oy))
                    {
                        gen->crowdRadius = 0;
                        gen->inView = FALSE;
                        return TRUE;
                    }
            }
    }
    gen->crowdRadius = 0;
    gen->inView = FALSE;
    return FALSE;
}

// Open ground beside water (the sea or a pond) facing it: a fisher's spot.
static bool8 SigShoreSpot(struct Gen *gen, int *ox, int *oy, u8 *facing)
{
    int n = gen->cw * gen->ch, start = RAND(n), k;
    // On the grass or sand by the water, else at the road's edge above it.
    for (k = 0; k < n * 2; k++)
    {
        int i = (start + k * 7) % n, ux = i % gen->cw, uy = i / gen->cw, d;
        u8 c = Cell(gen, ux, uy);
        YIELD();
        if ((c != C_OPEN && c != C_SHORE && (k < n || c != C_ROAD)) || IsEdge(gen, ux, uy) || !Reached(gen, ux, uy))
            continue;
        for (d = 0; d < 4; d++)
        {
            u8 nb = Cell(gen, ux + sDirDX[d], uy + sDirDY[d]);
            int x = ux * 2 + (d == RDIR_EAST), y = uy * 2 + (d == RDIR_SOUTH);
            if (nb != C_SEA && nb != C_WATER)
                continue;
            if (IsOccupied(gen, x, y) || BlocksWay(gen, x, y) || SeenFromNextDoor(gen, x, y) || IsCrowded(gen, x, y))
                continue;
            *ox = x;
            *oy = y;
            *facing = d;
            return TRUE;
        }
    }
    return FALSE;
}

// Dewford's piers out into an eastern sea: chosen before the people (who
// fish from their ends), drawn once the map is (SigRender).
static void SigPlanPiers(struct Gen *gen, struct SigTown *st, int want)
{
    int tries;
    for (tries = 0; tries < 40 && st->nPiers < want; tries++)
    {
        int uy = 2 + RAND(gen->ch - 4), d = gen->seaDepth[uy], len, k;
        bool8 ok = d >= 2 && gen->seaDepth[uy - 1] >= 2 && gen->seaDepth[uy + 1] >= 2
                && Cell(gen, gen->cw - 1 - d, uy) == C_OPEN && IsBeachU(gen, gen->cw - 1 - d, uy)
                && !InSeamView(gen, gen->cw - d, uy, d, 1);
        for (k = 0; k < st->nPiers && ok; k++)
            if (Abs(st->pierY[k] / 2 - uy) < 3)
                ok = FALSE;
        if (!ok)
            continue;
        len = 4 + RAND(3);
        if (len > d * 2 - 1)
            len = d * 2 - 1;
        st->pierX[st->nPiers] = (gen->cw - d) * 2;
        st->pierY[st->nPiers] = uy * 2;
        st->pierLen[st->nPiers++] = len;
    }
}

// How many Pokemon of the trade SigPeople puts out: a ranch's grazers, else
// one (the townsfolk's own walkers give way to them).
static int SigMonsWanted(const struct SigTown *st, bool8 hamlet)
{
    if (st->sig == RSIG_NONE || st->sig >= RSIG_BUILT_COUNT)
        return 0;
    if (st->sig == RSIG_BERRY)
        return 0;
    return st->sig == RSIG_RANCH ? 2 : 1;
}

static u8 SigFolkWanted(const struct Gen *gen)
{
    return gen->p->type == RTOWN_GYM ? 3 : (gen->p->type == RTOWN_HAMLET ? 1 : 2);
}

// The signature people and Pokemon, before the ordinary townsfolk. Returns
// how many people it placed (they stand in for townsfolk). *mons: Pokemon.
// TRUE where a block is seen from just across a connection: within about
// half a screen of an opening and as deep as the camera shows past the
// seam (check_seeds.py's in_seam_view). No obstacle may stand there.
static bool8 SigSeenAcross(struct Gen *gen, int x, int y)
{
    const struct RoguePlace *p = gen->p;
    int i;
    for (i = 0; i < p->nLinks; i++)
    {
        int d = p->links[i].dir, pos = p->links[i].pos & ~1, depth, along;
        depth = d == RDIR_NORTH ? y : d == RDIR_SOUTH ? gen->m->h - 1 - y : d == RDIR_WEST ? x : gen->m->w - 1 - x;
        along = (d == RDIR_NORTH || d == RDIR_SOUTH ? x : y) - pos;
        if (depth <= 2)
            return TRUE;
        if ((d == RDIR_NORTH || d == RDIR_SOUTH) && depth <= 4 && along >= -7 && along <= 8)
            return TRUE;
        if ((d == RDIR_EAST || d == RDIR_WEST) && depth <= 6 && along >= -5 && along <= 6)
            return TRUE;
    }
    return FALSE;
}

// The signature's people and things come after the townsfolk; two spots
// stay free for the villains' blockade (AddBlockade comes after).
static void SigAdd(struct Gen *gen, u8 kind, int fx, int fy, u8 facing, u8 slot, u8 range)
{
    if (gen->m->nObjs < ROGUE_MAX_OBJS - 2)
        AddObj(gen, kind, fx, fy, facing, slot, range);
}

static int SigPeople(struct Gen *gen, struct SigTown *st, const struct Placed *placed, int nPlaced, int *mons)
{
    int i, n = 0, want = SigFolkWanted(gen), fx = -1, fy = -1, x, y;
    u8 giftLevel;
    bool8 gift = !SigEnterable(st->sig) && RogueGen_SignatureGift(gen->r, gen->m->place, &giftLevel) != 0;
    u8 facing = RDIR_SOUTH, roles[4], nRoles = 0;
    *mons = 0;
    if (st->sig == RSIG_NONE || st->sig >= RSIG_BUILT_COUNT)
        return 0;
    // Where the landmark stands: its door, or the middle of its lot. A lot
    // laid along a street gives its top left (in blocks; its unit if even).
    for (i = 0; i < nPlaced; i++)
        if (placed[i].door == DOOR_SIGNATURE || placed[i].door == DOOR_SIGLOT)
        {
            fx = placed[i].bx * 2 + placed[i].b->doorX;
            fy = placed[i].by * 2 + placed[i].b->sy + FrontY(placed[i].b);
            if (placed[i].door == DOOR_SIGLOT)
            {
                st->lx = placed[i].bx;
                st->ly = placed[i].by;
                st->lotY = placed[i].b->sy;
                st->lotW = placed[i].b->sw;
                st->lotH = placed[i].b->sh;
                st->small = placed[i].b->uw == 3;
                st->kind = placed[i].b == &sSigMarketLot ? 3 : 4;
            }
        }
    if (fx < 0 && st->lx >= 0)
    {
        fx = st->kind == 2 ? st->lx + 9 : (st->kind == 1 ? st->lx * 2 + 3 : st->lx * 2 + 4);
        fy = st->kind == 2 ? st->ly + SIG_STAMP_HOTSPRING_H : (st->kind == 1 ? 7 : st->ly * 2 + 6);
    }
    // Who stands where, first come first.
    switch (st->sig)
    {
    case RSIG_HOT_SPRING:
        // A bather soaking in the pool (its rocks shut it in, as
        // Lavaridge's is reached down a ladder from the cliff), and the
        // keeper on the sand below, who lets the team soak.
        if (st->kind == 2)
        {
            SigAdd(gen, ROBJ_SIGFOLK, st->lx + 4, st->ly + 4, RDIR_SOUTH, SIGROLE_POST, 0);
            SigAdd(gen, ROBJ_SIGFOLK, st->lx + 3 + RAND(3), st->ly + 8, RDIR_SOUTH, SIGROLE_ITEM, 0);
            n++;
        }
        break;
    case RSIG_MINING:
        // A miner in the mine's mouth, keeping folk out; by a cave's mouth,
        // beside it.
        if (st->kind == 1 && st->small)
        {
            if (SigSpotNear(gen, gen->mouthX, 6, 3, &x, &y, FALSE))
            {
                SigAdd(gen, ROBJ_SIGFOLK, x, y, RDIR_SOUTH, SIGROLE_TALK | (1 << 2), 0);
                n++;
            }
        }
        else if (st->kind == 1)
        {
            SigAdd(gen, ROBJ_SIGFOLK, gen->mouthX, 5, RDIR_SOUTH, SIGROLE_POST, 0);
            n++;
        }
        break;
    case RSIG_MARKET:
        // Stall keepers on the cobbles beside their goods, in a city's market.
        if (st->kind == 3)
        {
            // On the cobbles inside, with goods beside them.
            int k = 0, start = RAND(SIG_STAMP_MARKET_W * 4);
            for (i = 0; i < SIG_STAMP_MARKET_W * 5 && k < 2; i++)
            {
                int j = (start + i * 3) % (SIG_STAMP_MARKET_W * 5), sx = j % SIG_STAMP_MARKET_W, sy = 1 + j / SIG_STAMP_MARKET_W;
                u16 v = sSigStamp_Market[sy * SIG_STAMP_MARKET_W + sx];
                x = st->lx * 2 + sx;
                y = st->ly * 2 + st->lotY + sy;
                if (sx < 2 || sx > SIG_STAMP_MARKET_W - 3 || (v & 0x0C00) || (v & 0x3FF) != 0x211 || IsOccupied(gen, x, y)
                 || !((sSigStamp_Market[sy * SIG_STAMP_MARKET_W + sx - 1] & 0x0C00)
                   || (sSigStamp_Market[sy * SIG_STAMP_MARKET_W + sx + 1] & 0x0C00)
                   || (sSigStamp_Market[(sy - 1) * SIG_STAMP_MARKET_W + sx] & 0x0C00))
                 || (k == 1 && Abs(x - gen->m->objs[gen->m->nObjs - 1].x) + Abs(y - gen->m->objs[gen->m->nObjs - 1].y) < 4))
                    continue;
                SigAdd(gen, ROBJ_SIGFOLK, x, y, RDIR_SOUTH, k == 0 ? SIGROLE_ITEM : (gift ? SIGROLE_GIFT : SIGROLE_TALK) | (1 << 2), 0);
                n++;
                if (k++ == 1)
                    gift = FALSE;
            }
        }
        break;
    case RSIG_RANCH:
    case RSIG_BERRY:
        if (st->kind == 4)
        {
            if (st->sig == RSIG_BERRY)
                for (i = 0; i < (st->small ? 2 : ROGUE_FARM_BERRIES); i++)
                    SigAdd(gen, ROBJ_BERRY, st->lx * 2 + sSigBerrySpots[i][0], st->ly * 2 + st->lotY + sSigBerrySpots[i][1], RDIR_SOUTH, i, 0);
            else
                for (i = 0; i < (gen->p->type == RTOWN_HAMLET || st->small ? 2 : 3); i++)
                {
                    SigAdd(gen, ROBJ_SIGMON, st->lx * 2 + sSigGrazeSpots[i][0] - (st->small && sSigGrazeSpots[i][0] > 4 ? 3 : 0),
                           st->ly * 2 + st->lotY + sSigGrazeSpots[i][1] - (st->small ? 1 : 0), RDIR_SOUTH, i, 0);
                    (*mons)++;
                }
        }
        break;
    case RSIG_FISHING:
        if (gen->p->theme == RTHEME_BEACH && gen->p->layout == RLAYOUT_HARBOR && gen->p->seaSide == RDIR_EAST)
            SigPlanPiers(gen, st, gen->p->type == RTOWN_GYM ? 3 : 2);
        for (i = 0; i < st->nPiers && n < want; i++)
        {
            SigAdd(gen, ROBJ_SIGFOLK, st->pierX[i] + st->pierLen[i] - 1, st->pierY[i], RDIR_EAST, SIGROLE_POST | (i << 2), 0);
            n++;
        }
        // (One fisher at least, even in a hamlet whose one is a giver.)
        while (n < (want > 1 && gift ? want - 1 : want) && SigShoreSpot(gen, &x, &y, &facing))
        {
            SigAdd(gen, ROBJ_SIGFOLK, x, y, facing, SIGROLE_POST | (n << 2), 0);
            n++;
        }
        // Else as near the water as they can stand, looking at it.
        if (n == 0)
        {
            int wx = 0, wy = 0, nw = 0;
            for (y = 0; y < gen->ch; y++)
                for (x = 0; x < gen->cw; x++)
                    if (Cell(gen, x, y) == C_WATER || Cell(gen, x, y) == C_SEA)
                        wx += x, wy += y, nw++;
            if (nw)
            {
                wx = wx * 2 / nw;
                wy = wy * 2 / nw;
                for (i = 0; i < (want > 1 && gift ? want - 1 : want) && SigSpotNear(gen, wx, wy, 12, &x, &y, TRUE); i++)
                {
                    facing = Abs(x - wx) > Abs(y - wy) ? (x < wx ? RDIR_EAST : RDIR_WEST) : (y < wy ? RDIR_SOUTH : RDIR_NORTH);
                    SigAdd(gen, ROBJ_SIGFOLK, x, y, facing, SIGROLE_POST | (n << 2), 0);
                    n++;
                }
            }
        }
        if (st->nPiers || n)
            st->kind = 5;
        fx = n ? gen->m->objs[gen->m->nObjs - 1].x : fx;
        fy = n ? gen->m->objs[gen->m->nObjs - 1].y : fy;
        break;
    }
    // The rest stand about the landmark: the giver first, then talkers.
    if (gift)
        roles[nRoles++] = SIGROLE_GIFT;
    if ((st->sig == RSIG_RANCH) && st->kind)
        roles[nRoles++] = SIGROLE_ITEM;
    while (nRoles < 4)
        roles[nRoles++] = SIGROLE_TALK;
    for (i = 0; i < nRoles && n < want + (gift ? 1 : 0) && fx >= 0; i++)
    {
        if (!SigSpotNear(gen, fx, fy, 8, &x, &y, TRUE))
        {
#ifdef ROGUE_HOST
            if (getenv("SIGDBG"))
                fprintf(stderr, "people place=%d: no spot near %d,%d (n=%d)\n", gen->m->place, fx, fy, n);
#endif
            break;
        }
        SigAdd(gen, ROBJ_SIGFOLK, x, y, y < fy ? RDIR_SOUTH : (y > fy ? RDIR_NORTH : (x < fx ? RDIR_EAST : RDIR_WEST)),
               roles[i] | (((n + i) & 3) << 2), 0);
        n++;
    }
    // Rocks to break by the dig or the mine (Rock Smash), as on Route 111.
    if ((st->sig == RSIG_FOSSIL || st->sig == RSIG_MINING) && fx >= 0)
        for (i = 0; i < (gen->p->type == RTOWN_HAMLET ? 1 : 2); i++)
            // (Never in view across a seam: an obstacle there is not this map's.)
            if (SigSpotNear(gen, fx + (i & 1 ? 3 : -3), fy + 1, 5, &x, &y, FALSE) && !SigSeenAcross(gen, x, y))
                SigAdd(gen, ROBJ_ROCK, x, y, RDIR_SOUTH, i, 0);
    // A Pokemon that fits the trade, out for a walk (the ranch's are grazing).
    if (*mons == 0 && fx >= 0 && SigSpotNear(gen, fx + 2, fy, 6, &x, &y, FALSE))
    {
        SigAdd(gen, ROBJ_SIGMON, x, y, RDIR_SOUTH, 0, 0);
        (*mons)++;
    }
    return n;
}

// Drawn over the rendered map: the hot spring, the piers.
static void SigRender(struct Gen *gen, struct SigTown *st)
{
    int i, k, x, y;
    if (st->kind == 2)
        for (y = 0; y < SIG_STAMP_HOTSPRING_H; y++)
            for (x = 0; x < SIG_STAMP_HOTSPRING_W; x++)
                if (sSigStamp_HotSpring[y * SIG_STAMP_HOTSPRING_W + x])
                    Put(gen, st->lx + x, st->ly + y, sSigStamp_HotSpring[y * SIG_STAMP_HOTSPRING_W + x]);
    // A city market's cobbles run on into the street below it: the street's
    // top edge there becomes its middle, and the market's foot its cobbles'
    // middle (a path, sand or town road: each set's top edge is its first
    // row, its middle the next).
    if (st->kind == 3 && st->sig == RSIG_MARKET)
    {
        int by = st->ly * 2 + st->lotY + SIG_STAMP_MARKET_H;
        for (x = st->lx * 2 + 2; x < st->lx * 2 + SIG_STAMP_MARKET_W - 2 && by < gen->m->h; x++)
        {
            u16 below = gen->m->blocks[by * gen->m->w + x], t = below & 0x3FF;
            static const u16 sSets[] = { 0x1D0, 0x118, 0x208 };
            for (k = 0; k < 3; k++)
                if (t >= sSets[k] && t <= sSets[k] + 2)
                    break;
            if (k == 3)
                continue;
            Put(gen, x, by, below + 8);
            Put(gen, x, by - 1, (gen->m->blocks[(by - 1) * gen->m->w + x] & ~0x3FF) | 0x211);
        }
    }
    for (i = 0; i < st->nPiers; i++)
        for (k = 0; k < st->pierLen[i]; k++)
        {
            Put(gen, st->pierX[i] + k, st->pierY[i], k == 0 ? 0x3228 : 0x3229);
            Put(gen, st->pierX[i] + k, st->pierY[i] + 1, k == 0 ? 0x0630 : 0x0631);
        }
}

// A lot's sign (the market's, a pasture's, the berry plots', the flower
// beds'): a step outside its fence, beside the gate or at a corner, as Hoenn's
// signs stand by what they name, within SIG_LOT_SIGN_REACH blocks of the lot
// (x0, y0, w, h in blocks), clear of the other signs and of the paths.
// FALSE, and no sign, if there is no such spot.
#define SIG_LOT_SIGN_REACH 3
static bool8 SigLotSign(struct Gen *gen, int x0, int y0, int w, int h)
{
    const struct RogueMap *m = gen->m;
    const u8 *seen = (const u8 *)&sBfsQueue[BFS_RING / 2];
    int x, y, k, best = 0x7FFF, bx = -1, by = -1, r = SIG_LOT_SIGN_REACH;
#define SEEN(xx, yy) (InMap(gen, xx, yy) && ((seen[((yy) * m->w + (xx)) >> 3] >> (((yy) * m->w + (xx)) & 7)) & 1))
    FloodBlocks(gen);
    for (y = y0 - r; y < y0 + h + r; y++)
        for (x = x0 - r; x < x0 + w + r; x++)
        {
            int dx = x < x0 ? x0 - x : (x >= x0 + w ? x - (x0 + w - 1) : 0);
            int dy = y < y0 ? y0 - y : (y >= y0 + h ? y - (y0 + h - 1) : 0);
            int score, gx, gy;
            bool8 ok = TRUE, gate = FALSE, path = FALSE, pn, ps, pw, pe;
            u8 c, d;
            YIELD();
            // A step clear of the fence (Hoenn's signs never touch one),
            // or diagonal to its corner.
            if (dx + dy < 2 || dx + dy > r || !SEEN(x, y))
                continue;
            c = Cell(gen, x >> 1, y >> 1);
            if ((c != C_OPEN && c != C_KEEP) || IsOccupied(gen, x, y) || PathCoverF(gen, x, y) || UnderCrown(gen, x, y)
             || ByLedgeF(gen, x, y) || BlocksWay(gen, x, y))
                continue;
            // Not in a notch of the path, or between two stretches of it.
            pn = PathCoverF(gen, x, y - 1);
            ps = PathCoverF(gen, x, y + 1);
            pw = PathCoverF(gen, x - 1, y);
            pe = PathCoverF(gen, x + 1, y);
            if ((pn && ps) || (pw && pe) || pn + ps + pw + pe >= 3)
                continue;
            // Free standing: nothing solid (a tree, a wall) beside it.
            for (d = 0; d < 4 && ok; d++)
                if (InMap(gen, x + sDirDX[d], y + sDirDY[d])
                 && ((m->blocks[(y + sDirDY[d]) * m->w + x + sDirDX[d]] >> 10) & 3))
                    ok = FALSE;
            if (!ok)
                continue;
            // Read from a block beside it.
            for (d = 0; d < 4 && !SEEN(x + sDirDX[d], y + sDirDY[d]); d++)
                ;
            if (d == 4)
                continue;
            // Not in front of a way out, nor by another sign or anyone.
            for (d = 0; d < 4 && ok; d++)
            {
                int edge = d == RDIR_NORTH ? y : d == RDIR_SOUTH ? m->h - 1 - y : d == RDIR_WEST ? x : m->w - 1 - x;
                int along = (d == RDIR_NORTH || d == RDIR_SOUTH) ? x : y;
                if (gen->exitPos[d] != 0xFF && edge < 3 && Abs(along - gen->exitPos[d]) < 4)
                    ok = FALSE;
            }
            for (k = 0; k < m->nSigns && ok; k++)
                if (Abs(m->signs[k].x - x) <= 2 && Abs(m->signs[k].y - y) <= 2)
                    ok = FALSE;
            for (k = 0; k < m->nObjs && ok; k++)
                if (Abs(m->objs[k].x - x) <= 1 && Abs(m->objs[k].y - y) <= 1)
                    ok = FALSE;
            if (!ok)
                continue;
            // Beside a gate: an opening in the lot's edge that leads in,
            // but not right in front of it.
            for (gy = y0; gy < y0 + h; gy++)
                for (gx = x0; gx < x0 + w; gx++)
                {
                    if ((gx != x0 && gx != x0 + w - 1 && gy != y0 && gy != y0 + h - 1) || !SEEN(gx, gy))
                        continue;
                    if ((gx == x && Abs(gy - y) == 1) || (gy == y && Abs(gx - x) == 1))
                        ok = FALSE;
                    else if (Abs(gx - x) <= 3 && Abs(gy - y) <= 2)
                        gate = TRUE;
                }
            if (!ok)
                continue;
            for (gy = y - 1; gy <= y + 1; gy++)
                for (gx = x - 1; gx <= x + 1; gx++)
                    path |= PathCoverF(gen, gx, gy);
            // Nearest first; then by the gate, by the way, below or beside
            // the lot rather than behind it, and the lower the better (the
            // gates open at the bottom).
            score = ((dx + dy) * 8 + (gate ? 0 : 3) + (path ? 0 : 2) + (y < y0 ? 2 : 0)) * 16
                  + (y < y0 + h - 1 ? (y0 + h - 1 - y > 15 ? 15 : y0 + h - 1 - y) : 0);
            if (score < best)
            {
                best = score;
                bx = x;
                by = y;
            }
        }
#undef SEEN
    if (bx < 0)
        return FALSE;
    if (Cover(gen, bx, by) == V_FLOWER)
        SetCover(gen, bx, by, BareCover(gen, bx, by));
    AddSign(gen, bx, by, RSIGN_LANDMARK);
    return TRUE;
}

// A sign naming a scenery landmark (a building's comes with its house signs),
// and whether the town's landmark got built.
static void SigFinish(struct Gen *gen, struct SigTown *st, const struct Placed *placed, int nPlaced)
{
    int i, x, y;
    u8 facing;
    st->built = st->sig >= RSIG_BUILT_COUNT;
    for (i = 0; i < nPlaced; i++)
        if (placed[i].door == DOOR_SIGNATURE || placed[i].door == DOOR_SIGLOT)
            st->built = TRUE;
    for (i = 0; i < gen->m->nHouses; i++)
        if (gen->m->interiors[i] == RINT_SIGNATURE)
            st->built = TRUE;
    if (SigEnterable(st->sig))
    {
        // Only if it opens onto its interior.
        st->built = FALSE;
        for (i = 0; i < gen->m->nHouses; i++)
            if (gen->m->interiors[i] == RINT_SIGNATURE)
                st->built = TRUE;
    }
    else if (st->kind)
    {
        st->built = TRUE;
        // Decor can be cleared under a tree's crown: a lot must still stand.
        if ((st->kind == 3 || st->kind == 4) && st->sig != RSIG_MARKET && st->sig != RSIG_RANCH && st->sig != RSIG_BERRY)
        {
            st->built = FALSE;
            for (i = 0; i < gen->nDecor; i++)
                if (gen->decor[i].x == st->lx * 2 && gen->decor[i].y == st->ly * 2)
                    st->built = TRUE;
        }
        if (st->sig == RSIG_FISHING)
            st->built = st->nPiers > 0 || st->kind == 5;
        if (st->built && st->kind != 5 && st->lx >= 0)
        {
            int fx = st->kind == 2 ? st->lx + 9 : (st->kind == 1 ? gen->mouthX : st->lx * 2 + 2);
            int fy = st->kind == 2 ? st->ly + SIG_STAMP_HOTSPRING_H : (st->kind == 1 ? 6 : st->ly * 2 + st->lotY + 6);
            // Beside the stretch of road nearest to it; else, nearer the
            // homes' name plates than a sign would usually stand.
            int d, k, pass, i0, signs = gen->m->nSigns;
            // A lot's sign stands by the lot, or there is none.
            if (st->kind == 3 || st->kind == 4)
            {
                SigLotSign(gen, st->lx * 2, st->ly * 2 + st->lotY, st->lotW, st->lotH);
                signs = -1;
            }
            // A cave's mines: on the ground at the foot of the rock beside
            // the cave's mouth, as the signs by Hoenn's cave mouths stand.
            for (i0 = 0; st->kind == 1 && st->small && i0 < 48; i0++)
            {
                d = 1 + (i0 >> 1) % 6;
                x = gen->mouthX + ((i0 & 1) ? d + 1 : -d);
                y = 6 + i0 / 12;
                if (InMap(gen, x, y) && (Cell(gen, x >> 1, y >> 1) == C_OPEN || Cell(gen, x >> 1, y >> 1) == C_KEEP)
                 && !IsOccupied(gen, x, y) && !PathCoverF(gen, x, y) && !BlocksWay(gen, x, y) && !UnderCrown(gen, x, y))
                {
                    AddSign(gen, x, y, RSIGN_LANDMARK);
                    break;
                }
            }
            for (pass = 0, d = 0; pass < 2 && gen->m->nSigns == signs; pass++, d = 0)
            {
                gen->crowdRadius = pass ? 1 : 0;
                for (; d <= 8; d++)
                {
                    for (k = 0; k < gen->nPath; k++)
                    {
                        int px = gen->path[k] % gen->cw, py = gen->path[k] / gen->cw;
                        if (Abs(px - (fx >> 1)) + Abs(py - (fy >> 1)) != d)
                            continue;
                        if (FindSignSpot(gen, px, py, &x, &y, &facing))
                            break;
                    }
                    if (k < gen->nPath)
                    {
                        AddSign(gen, x, y, RSIGN_LANDMARK);
                        break;
                    }
                }
                if (d <= 8)
                    break;
            }
            gen->crowdRadius = 0;
        }
    }
    else if (st->sig == RSIG_FISHING)
        st->built = FALSE;
#ifdef ROGUE_HOST
    if (getenv("SIGDBG"))
    {
        fprintf(stderr, "finish place=%d sig=%d kind=%d lx=%d ly=%d built=%d nDecor=%d attempt=%d\n", gen->m->place, st->sig, st->kind,
                st->lx, st->ly, st->built, gen->nDecor, gen->attempt);
        for (i = 0; i < gen->nDecor; i++)
            fprintf(stderr, "   decor %d,%d %dx%d\n", gen->decor[i].x, gen->decor[i].y, gen->decor[i].w, gen->decor[i].h);
        if (getenv("SIGDBG")[0] == '2')
            for (y = 0; y < gen->ch; y++)
            {
                for (x = 0; x < gen->cw; x++)
                    fputc(".T=#~sR-kDSFC"[Cell(gen, x, y)], stderr);
                fputc('\n', stderr);
            }
    }
#endif
    HOST_LANDMARK(st->built);
}
