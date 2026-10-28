// RUN 0's challenge (Roguémon v1.0): the level band, the play the Director
// counts, the patches it makes after each badge and what they change, and
// the counter-pick after a loss. The teams are built in
// src/rogue_run0_trainers.h (part of src/rogue.c). See include/rogue_run0.h.

#include "global.h"
#include "rogue.h"
#include "rogue_run0.h"
#include "rogue_gen.h"
#include "battle.h"
#include "event_data.h"
#include "field_player_avatar.h"
#include "fieldmap.h"
#include "item.h"
#include "malloc.h"
#include "metatile_behavior.h"
#include "money.h"
#include "overworld.h"
#include "pokemon.h"
#include "random.h"
#include "shop.h"
#include "string_util.h"
#include "constants/battle.h"
#include "constants/characters.h"
#include "constants/items.h"
#include "constants/moves.h"

STATIC_ASSERT(sizeof(struct RogueRun0Challenge) <= ROGUE_DIRECTOR_CHALLENGE_BYTES, RogueRun0ChallengeFits);
STATIC_ASSERT(RUN0_CHALLENGE_TOWNS == ROGUE_MAX_PLACES, RogueRun0ChallengeTowns);
STATIC_ASSERT(RUN0_PATCH_COUNT <= 16, RogueRun0PatchesFitVar);
STATIC_ASSERT(NUMBER_OF_MON_TYPES <= 32, RogueRun0TypesFitBand);
STATIC_ASSERT((offsetof(struct RogueDirector, challenge) & 3) == 0, RogueRun0ChallengeAligned);

#define RUN0_BASE_MIN 45
#define RUN0_BASE_MAX 80
#define RUN0_CENTER_HEALS 3
#define RUN0_START_MONEY  10000

static void CountUp(u16 *counter)
{
    if (*counter < 0xFFFF)
        (*counter)++;
}

// The challenge's part of a Director, set up empty if it is from no attempt.
static struct RogueRun0Challenge *Challenge(struct RogueDirector *d)
{
    struct RogueRun0Challenge *c = RUN0_CHALLENGE(d);
    if (c->version != RUN0_CHALLENGE_VERSION)
    {
        memset(c, 0, sizeof(*c));
        c->version = RUN0_CHALLENGE_VERSION;
        c->lastPatch = c->giftPending = c->lastCost = RUN0_PATCH_NONE;
        memset(c->patchOrder, RUN0_PATCH_NONE, sizeof(c->patchOrder));
    }
    return c;
}

// The Director for a moment: loaded if it was not, let go after if so.
static struct RogueDirector *Peek(bool32 *wasLoaded)
{
    *wasLoaded = Rogue_IsDirectorLoaded();
    return Rogue_Director();
}

static void Unpeek(bool32 wasLoaded)
{
    if (!wasLoaded)
        Rogue_ReleaseDirector();
}

// ---------------------------------------------------------------------------
// The attempt

static u32 PartyMedianLevel(void)
{
    u8 levels[PARTY_SIZE];
    u32 n = 0, i, j;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        u8 level;
        if (!GetMonData(mon, MON_DATA_SANITY_HAS_SPECIES) || GetMonData(mon, MON_DATA_IS_EGG))
            continue;
        level = GetMonData(mon, MON_DATA_LEVEL);
        for (j = n; j > 0 && levels[j - 1] > level; j--)
            levels[j] = levels[j - 1];
        levels[j] = level;
        n++;
    }
    if (n == 0)
        return 55;
    return (levels[(n - 1) / 2] + levels[n / 2] + 1) / 2;
}

void Rogue_Run0ChallengeStart(struct RogueDirector *d)
{
    struct RogueRun0Challenge *c = Challenge(d);
    u32 loss = c->lossType, base = PartyMedianLevel();

    memset(c, 0, sizeof(*c));
    c->version = RUN0_CHALLENGE_VERSION;
    c->lossType = loss < NUMBER_OF_MON_TYPES ? loss : TYPE_NONE;
    c->lastPatch = c->giftPending = c->lastCost = RUN0_PATCH_NONE;
    c->answerType = TYPE_NONE;
    memset(c->patchOrder, RUN0_PATCH_NONE, sizeof(c->patchOrder));
    // A team far under or over a League's levels still gets a climb.
    if (base < RUN0_BASE_MIN)
        base = RUN0_BASE_MIN;
    if (base > RUN0_BASE_MAX)
        base = RUN0_BASE_MAX;
    c->baseLevel = base;
    VarSet(VAR_ROGUE_RUN0_BAND, ROGUE_RUN0_BAND_VALUE(base, c->lossType, TYPE_NONE));
    VarSet(VAR_ROGUE_RUN0_PATCHES, 0);
    FlagClear(FLAG_ROGUE_RUN0_PATCH_NEW);
    d->patches = 0;
}

void Rogue_Run0GiveKit(void)
{
    FlagSet(FLAG_ROGUE_EXP_SHARE);
    SetMoney(&gSaveBlock1Ptr->money, RUN0_START_MONEY);
    AddBagItem(ITEM_HYPER_POTION, 8);
    AddBagItem(ITEM_FULL_HEAL, 3);
    AddBagItem(ITEM_ULTRA_BALL, 5);
    AddBagItem(ITEM_OLD_ROD, 1);
}

void Rogue_Run0ChallengeBattleStart(struct RogueDirector *d, u16 leadSpecies)
{
    struct RogueRun0Challenge *c = Challenge(d);
    u32 i, low = 0;

    if (gBattleTypeFlags & BATTLE_TYPE_TRAINER)
        CountUp(&c->trainerBattles);
    else
        CountUp(&c->wildBattles);
    if (leadSpecies == SPECIES_NONE)
        return;
    // The leads, by species: a new one takes the place of the least used.
    for (i = 0; i < RUN0_CHALLENGE_LEADS; i++)
    {
        if (c->leads[i].species == leadSpecies)
        {
            CountUp(&c->leads[i].count);
            return;
        }
        if (c->leads[i].count < c->leads[low].count)
            low = i;
    }
    c->leads[low].species = leadSpecies;
    c->leads[low].count = 1;
}

void Rogue_Run0ChallengeBattleEnd(struct RogueDirector *d)
{
    struct RogueRun0Challenge *c = Challenge(d);
    u32 i;
    if (gBattleOutcome == B_OUTCOME_RAN && !(gBattleTypeFlags & BATTLE_TYPE_TRAINER))
        CountUp(&c->fled);
    for (i = 0; i < gBattleResults.numHealingItemsUsed; i++)
        CountUp(&d->itemsUsed);
}

// The type led with most, and the Pokemon.
static u32 TopType(const struct RogueDirector *d, u32 *count)
{
    u32 t, best = TYPE_NONE, most = 0;
    for (t = TYPE_NORMAL; t < NUMBER_OF_MON_TYPES && t < ROGUE_DIRECTOR_TYPES; t++)
    {
        if (d->leadTypes[t] > most)
        {
            most = d->leadTypes[t];
            best = t;
        }
    }
    *count = most;
    return best;
}

// The lead used most that still stands in the party (a PATCH never aims
// at a Pokemon that just fainted and was kept).
static bool32 LeadStands(u16 species)
{
    u32 i;
    for (i = 0; i < PARTY_SIZE; i++)
        if (GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_SPECIES) == species
         && GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_HP) != 0)
            return TRUE;
    return FALSE;
}

static u16 TopLead(const struct RogueRun0Challenge *c, u32 *count)
{
    s32 i, best = -1;
    for (i = 0; i < RUN0_CHALLENGE_LEADS; i++)
        if (c->leads[i].species != SPECIES_NONE && LeadStands(c->leads[i].species)
         && (best < 0 || c->leads[i].count > c->leads[best].count))
            best = i;
    *count = best < 0 ? 0 : c->leads[best].count;
    return best < 0 ? SPECIES_NONE : c->leads[best].species;
}

void Rogue_Run0ChallengeRunEnd(struct RogueDirector *d, bool32 won)
{
    struct RogueRun0Challenge *c = Challenge(d);
    u32 count;
    // "Next time I'll know." The next attempt's trainers lean against it.
    c->lossType = won ? TYPE_NONE : TopType(d, &count);
}

// ---------------------------------------------------------------------------
// The patches

bool32 Rogue_Run0PatchOn(u32 patch)
{
    return Rogue_IsRun0() && patch < RUN0_PATCH_COUNT && (VarGet(VAR_ROGUE_RUN0_PATCHES) & (1u << patch));
}

static u32 GiftCost(u32 gift)
{
    switch (gift)
    {
    case RUN0_PATCH_GIFT_CANDY:  return RUN0_PATCH_COST_CANDY;
    case RUN0_PATCH_GIFT_EXP:    return RUN0_PATCH_COST_EXP;
    case RUN0_PATCH_GIFT_PRICES: return RUN0_PATCH_COST_PRICES;
    }
    return RUN0_PATCH_NONE;
}

static bool32 IsGift(u32 patch)
{
    return GiftCost(patch) != RUN0_PATCH_NONE;
}

// How much the play asks for each patch: the one the player earned most
// is made. Shares are percent of battles; counts are per badge.
static s32 PatchScore(struct RogueDirector *d, struct RogueRun0Challenge *c, u32 patch, u32 badges)
{
    u32 battles = d->battles ? d->battles : 1, count;
    switch (patch)
    {
    case RUN0_PATCH_LEAD_MOVES:
        if (TopLead(c, &count) == SPECIES_NONE)
            return -100;
        return (s32)(count * 100 / battles) - 25;
    case RUN0_PATCH_TYPE_ANSWERS:
        if (TopType(d, &count) == TYPE_NONE)
            return -100;
        return (s32)(count * 100 / battles) - 35;
    case RUN0_PATCH_CENTERS:
        return (s32)(d->heals * 12 / badges) - 12;
    case RUN0_PATCH_SHOPS:
        return (s32)(d->itemsUsed * 15 / badges) - 10;
    case RUN0_PATCH_BALLS:
        return (s32)(d->catches * 15 / badges) - 12;
    case RUN0_PATCH_WILD_EXP:
        return (s32)(c->wildBattles * 100 / battles) - 45;
    case RUN0_PATCH_NO_ESCAPE:
        return (s32)(c->fled * 12) - 10;
    case RUN0_PATCH_OMNISCIENT:
    case RUN0_PATCH_LEADERS:
    case RUN0_PATCH_PAIRS:
        return 6;
    default:
        // A gift now and then: one an act is the Director being generous.
        // Never at the last badge, whose cost would never come.
        if (badges >= ROGUE_NUM_GYMS)
            return -100;
        return (badges == 2 || badges == 5) ? 30 : 4;
    }
}

static u32 PickPatch(struct RogueDirector *d, struct RogueRun0Challenge *c, u32 badges)
{
    u32 p, best = RUN0_PATCH_NONE, taken = d->patches;
    s32 bestScore = -1000;
    u32 salt = RogueRng_Hash(d->attempts, 0x5A0 + badges);
    for (p = 0; p < RUN0_PATCH_PICKS; p++)
    {
        s32 score;
        if (taken & (1u << p))
            continue;
        // Give and take back: one gift at a time.
        if (IsGift(p) && c->giftPending != RUN0_PATCH_NONE)
            continue;
        score = PatchScore(d, c, p, badges) * 8 + (s32)((salt >> (p * 2)) & 7);
        if (score > bestScore)
        {
            bestScore = score;
            best = p;
        }
    }
    return best;
}

static void SetPatches(struct RogueDirector *d, u32 patches)
{
    d->patches = patches;
    VarSet(VAR_ROGUE_RUN0_PATCHES, patches);
}

void Rogue_Run0PatchAfterBadge(void)
{
    struct RogueDirector *d;
    struct RogueRun0Challenge *c;
    u32 badges = Rogue_BadgeCount(), patches, pick, cost = RUN0_PATCH_NONE, count;

    if (!Rogue_IsRun0() || badges == 0)
        return;
    d = Rogue_Director();
    if (d == NULL)
        return;
    c = Challenge(d);
    patches = d->patches | VarGet(VAR_ROGUE_RUN0_PATCHES);
    // Last badge's gift is paid for now; a gift that lasted until this badge ends.
    if (c->giftPending != RUN0_PATCH_NONE)
    {
        cost = GiftCost(c->giftPending);
        patches |= 1u << cost;
        if (c->giftPending != RUN0_PATCH_GIFT_CANDY)
            patches &= ~(1u << c->giftPending);
        c->giftPending = RUN0_PATCH_NONE;
    }
    SetPatches(d, patches);
    pick = PickPatch(d, c, badges);
    if (pick != RUN0_PATCH_NONE)
    {
        patches |= 1u << pick;
        if (IsGift(pick))
            c->giftPending = pick;
        if (pick == RUN0_PATCH_LEAD_MOVES)
            c->leadSpecies = TopLead(c, &count);
        if (pick == RUN0_PATCH_TYPE_ANSWERS)
        {
            u16 band = VarGet(VAR_ROGUE_RUN0_BAND);
            c->answerType = TopType(d, &count);
            VarSet(VAR_ROGUE_RUN0_BAND, ROGUE_RUN0_BAND_VALUE(ROGUE_RUN0_BAND_BASE(band), ROGUE_RUN0_BAND_LOSS(band), c->answerType));
        }
        if (c->patchCount < ARRAY_COUNT(c->patchOrder))
            c->patchOrder[c->patchCount] = pick;
        c->patchCount++;
    }
    c->lastPatch = pick;
    c->lastCost = cost;
    SetPatches(d, patches);
    if (pick != RUN0_PATCH_NONE || cost != RUN0_PATCH_NONE)
        FlagSet(FLAG_ROGUE_RUN0_PATCH_NEW);
    Rogue_SaveDirector();
}

u32 Rogue_Run0LastPatch(u32 *version)
{
    bool32 was;
    struct RogueDirector *d = Peek(&was);
    u32 patch = RUN0_PATCH_NONE;
    *version = 0;
    if (d != NULL && RUN0_CHALLENGE(d)->version == RUN0_CHALLENGE_VERSION)
    {
        patch = RUN0_CHALLENGE(d)->lastPatch;
        *version = RUN0_CHALLENGE(d)->patchCount;
    }
    Unpeek(was);
    return patch;
}

// The type's name in capitals, as Gen 3 writes them (WATER).
static u8 *TypeName(u8 *dest, u32 type)
{
    u8 *start = dest;
    dest = StringCopy(dest, gTypesInfo[type].name);
    for (; start < dest; start++)
        if (*start >= CHAR_a && *start <= CHAR_z)
            *start += CHAR_A - CHAR_a;
    return dest;
}

static u8 *PatchLine(u8 *dest, u32 patch, const struct RogueDirector *d, const struct RogueRun0Challenge *c)
{
    switch (patch)
    {
    case RUN0_PATCH_LEAD_MOVES:
        dest = StringCopy(dest, COMPOUND_STRING("· Wild POKéMON now know moves\nagainst "));
        if (c != NULL && c->leadSpecies != SPECIES_NONE)
            dest = StringCopy(dest, GetSpeciesName(c->leadSpecies));
        else
            dest = StringCopy(dest, COMPOUND_STRING("your lead"));
        return StringCopy(dest, COMPOUND_STRING("."));
    case RUN0_PATCH_TYPE_ANSWERS:
        dest = StringCopy(dest, COMPOUND_STRING("· Trainers now bring answers\nto "));
        if (c != NULL && c->answerType != TYPE_NONE)
        {
            dest = TypeName(dest, c->answerType);
            return StringCopy(dest, COMPOUND_STRING(" POKéMON."));
        }
        return StringCopy(dest, COMPOUND_STRING("your POKéMON."));
    case RUN0_PATCH_CENTERS:
        return StringCopy(dest, COMPOUND_STRING("· POKéMON CENTERS now heal\nthree times per town."));
    case RUN0_PATCH_SHOPS:
        dest = StringCopy(dest, COMPOUND_STRING("· Shops no longer sell medicine.\n"));
        if (d != NULL && d->itemsUsed > 1)
        {
            dest = StringCopy(dest, COMPOUND_STRING("You used "));
            dest = ConvertIntToDecimalStringN(dest, d->itemsUsed, STR_CONV_MODE_LEFT_ALIGN, 5);
            return StringCopy(dest, COMPOUND_STRING(" in battle."));
        }
        return StringCopy(dest, COMPOUND_STRING("You know why."));
    case RUN0_PATCH_BALLS:
        return StringCopy(dest, COMPOUND_STRING("· POKé BALLS now work\nhalf as well."));
    case RUN0_PATCH_WILD_EXP:
        dest = StringCopy(dest, COMPOUND_STRING("· Wild POKéMON now give half\nthe EXP."));
        if (c != NULL && c->wildBattles > 1)
        {
            dest = StringCopy(dest, COMPOUND_STRING(" You fought "));
            dest = ConvertIntToDecimalStringN(dest, c->wildBattles, STR_CONV_MODE_LEFT_ALIGN, 5);
            dest = StringCopy(dest, COMPOUND_STRING("."));
        }
        return dest;
    case RUN0_PATCH_NO_ESCAPE:
        return StringCopy(dest, COMPOUND_STRING("· Running away now fails\nhalf the time."));
    case RUN0_PATCH_OMNISCIENT:
        return StringCopy(dest, COMPOUND_STRING("· Trainers now know your\nmoves before you use them."));
    case RUN0_PATCH_LEADERS:
        return StringCopy(dest, COMPOUND_STRING("· Gym leaders now\nremember you."));
    case RUN0_PATCH_PAIRS:
        return StringCopy(dest, COMPOUND_STRING("· Wild POKéMON in tall grass\nnow come in pairs."));
    case RUN0_PATCH_GIFT_CANDY:
        return StringCopy(dest, COMPOUND_STRING("· A gift: three RARE CANDIES.\nNo charge."));
    case RUN0_PATCH_GIFT_EXP:
        return StringCopy(dest, COMPOUND_STRING("· A gift: double EXP until\nthe next BADGE."));
    case RUN0_PATCH_GIFT_PRICES:
        return StringCopy(dest, COMPOUND_STRING("· A gift: half prices in shops\nuntil the next BADGE."));
    case RUN0_PATCH_COST_CANDY:
        return StringCopy(dest, COMPOUND_STRING("· The CANDIES were a loan.\nTrainers are 3 levels higher."));
    case RUN0_PATCH_COST_EXP:
        return StringCopy(dest, COMPOUND_STRING("· The EXP was a loan.\nYou now get three quarters."));
    case RUN0_PATCH_COST_PRICES:
        return StringCopy(dest, COMPOUND_STRING("· The sale is over.\nPrices are now doubled."));
    }
    *dest = EOS;
    return dest;
}

u8 *Rogue_Run0PatchText(u8 *dest, u32 patch)
{
    bool32 was;
    struct RogueDirector *d = Peek(&was);
    const struct RogueRun0Challenge *c = (d != NULL && RUN0_CHALLENGE(d)->version == RUN0_CHALLENGE_VERSION) ? RUN0_CHALLENGE(d) : NULL;
    dest = PatchLine(dest, patch, d, c);
    Unpeek(was);
    return dest;
}

void Rogue_Run0BufferPatch(void)
{
    bool32 was;
    struct RogueDirector *d = Peek(&was);
    const struct RogueRun0Challenge *c;
    u8 *s = gStringVar4;

    gSpecialVar_0x8000 = ITEM_NONE;
    gSpecialVar_0x8001 = 0;
    FlagClear(FLAG_ROGUE_RUN0_PATCH_NEW);
    if (d == NULL || RUN0_CHALLENGE(d)->version != RUN0_CHALLENGE_VERSION)
    {
        Unpeek(was);
        StringCopy(s, COMPOUND_STRING("Thank you for playing ROGUéMON."));
        return;
    }
    c = RUN0_CHALLENGE(d);
    s = StringCopy(s, COMPOUND_STRING("PATCH 0."));
    s = ConvertIntToDecimalStringN(s, c->patchCount, STR_CONV_MODE_LEFT_ALIGN, 2);
    *s++ = CHAR_NEWLINE;
    if (c->lastCost != RUN0_PATCH_NONE)
    {
        s = PatchLine(s, c->lastCost, d, c);
        s = StringCopy(s, COMPOUND_STRING("\p"));
    }
    if (c->lastPatch != RUN0_PATCH_NONE)
    {
        s = PatchLine(s, c->lastPatch, d, c);
        s = StringCopy(s, COMPOUND_STRING("\p"));
    }
    StringCopy(s, COMPOUND_STRING("Thank you for playing ROGUéMON."));
    if (c->lastPatch == RUN0_PATCH_GIFT_CANDY)
    {
        gSpecialVar_0x8000 = ITEM_RARE_CANDY;
        gSpecialVar_0x8001 = 3;
    }
    Unpeek(was);
}

// ---------------------------------------------------------------------------
// What the patches change

u32 Rogue_Run0ShopPrice(u32 price, u16 item)
{
    (void)item;
    if (!Rogue_IsRun0())
        return price;
    if (Rogue_Run0PatchOn(RUN0_PATCH_GIFT_PRICES))
        price /= 2;
    if (Rogue_Run0PatchOn(RUN0_PATCH_COST_PRICES))
        price *= 2;
    return price;
}

// RUN 0's Marts (v1.0.1): the tier's list (Rogue_Run0MartTier), or under
// the SHOPS patch the list with no medicine, then ETHERS, and MAX ETHERS from
// the sixth badge. Playtests ran out of PP late, with the CENTERS patch's
// three heals a town. PP items are not medicine here: the patch takes away
// what restores HP and status in battle, and an ETHER does neither.
#define RUN0_MART_STOCK 24
extern const u16 Rogue_Mart4_Items[];
extern const u16 Rogue_Mart5_Items[];
extern const u16 Rogue_Run0Mart_Items[];
static EWRAM_DATA u16 sRun0MartStock[RUN0_MART_STOCK] = {0};

static void BuildRun0MartStock(u16 *stock, u32 size, bool32 shopsPatch, u32 tier, u32 badges)
{
    const u16 *base = shopsPatch ? Rogue_Run0Mart_Items : (tier >= 5 ? Rogue_Mart5_Items : Rogue_Mart4_Items);
    u32 n = 0;
    while (base[n] != ITEM_NONE && n + 3 < size)
    {
        stock[n] = base[n];
        n++;
    }
    stock[n++] = ITEM_ETHER;
    if (badges >= 6)
        stock[n++] = ITEM_MAX_ETHER;
    stock[n] = ITEM_NONE;
}

// Script special: VAR_RESULT TRUE in RUN 0, with its Mart's list made
// (opened by Rogue_Run0OpenMart).
void Rogue_Run0PrepareMart(void)
{
    gSpecialVar_Result = Rogue_IsRun0();
    if (gSpecialVar_Result)
        BuildRun0MartStock(sRun0MartStock, RUN0_MART_STOCK, Rogue_Run0PatchOn(RUN0_PATCH_SHOPS),
                           Rogue_GetMartTier(), Rogue_BadgeCount());
}

// Script special, then waitstate: opens RUN 0's Mart.
void Rogue_Run0OpenMart(void)
{
    CreatePokemartMenu(sRun0MartStock);
}

// Script special: VAR_RESULT TRUE in RUN 0, where the LEAGUE's nurse says
// that her heals are not counted (no patch reaches the LEAGUE).
void Rogue_Run0LeagueNurse(void)
{
    gSpecialVar_Result = Rogue_IsRun0();
}

// RUN 0's Marts sell for its levels from the first town on.
u32 Rogue_Run0MartTier(u32 tier)
{
    if (Rogue_IsRun0() && tier < 4)
        return 4;
    return tier;
}

// The generated CENTERS only: the real maps' own nurses heal as they always did.
void Rogue_Run0NurseBefore(void)
{
    gSpecialVar_Result = TRUE;
    gSpecialVar_0x8005 = GetGameStat(GAME_STAT_USED_POKECENTER);
    gSpecialVar_0x8006 = 0xFFFF; // the heals this CENTER has left (the nurse says)
    if (Rogue_Run0PatchOn(RUN0_PATCH_CENTERS) && Rogue_GetCurrentPlace() >= 0)
    {
        bool32 was;
        struct RogueDirector *d = Peek(&was);
        u32 town = Rogue_GetCurrentTownPlace();
        if (d != NULL && town < RUN0_CHALLENGE_TOWNS && Challenge(d)->centerHeals[town] >= RUN0_CENTER_HEALS)
            gSpecialVar_Result = FALSE;
        else if (d != NULL && town < RUN0_CHALLENGE_TOWNS)
            gSpecialVar_0x8006 = RUN0_CENTER_HEALS - Challenge(d)->centerHeals[town];
        Unpeek(was);
    }
}

void Rogue_Run0NurseAfter(void)
{
    struct RogueDirector *d;
    u32 town = Rogue_GetCurrentTownPlace();
    // A real map's CENTER (an island's) never counts.
    if (!Rogue_IsRun0() || GetGameStat(GAME_STAT_USED_POKECENTER) == gSpecialVar_0x8005 || Rogue_GetCurrentPlace() < 0)
        return;
    d = Rogue_Director();
    if (d == NULL)
        return;
    CountUp(&d->heals);
    // The CENTERS patch's three heals count from the patch on: the heals
    // before it are not taken out of them.
    if (Rogue_Run0PatchOn(RUN0_PATCH_CENTERS) && town < RUN0_CHALLENGE_TOWNS && Challenge(d)->centerHeals[town] < 0xFF)
        Challenge(d)->centerHeals[town]++;
    Rogue_SaveDirector();
}

void Rogue_Run0Exp(s32 *exp)
{
    if (!Rogue_IsRun0())
        return;
    if (Rogue_Run0PatchOn(RUN0_PATCH_WILD_EXP) && !(gBattleTypeFlags & BATTLE_TYPE_TRAINER))
        *exp /= 2;
    if (Rogue_Run0PatchOn(RUN0_PATCH_GIFT_EXP))
        *exp *= 2;
    if (Rogue_Run0PatchOn(RUN0_PATCH_COST_EXP))
        *exp = *exp * 3 / 4;
    if (*exp < 1)
        *exp = 1;
}

u32 Rogue_Run0CatchRate(u32 rate)
{
    if (Rogue_Run0PatchOn(RUN0_PATCH_BALLS))
        rate = (rate + 1) / 2;
    return rate;
}

bool32 Rogue_Run0BlocksEscape(void)
{
    return Rogue_Run0PatchOn(RUN0_PATCH_NO_ESCAPE) && (Random() & 1);
}

// Wild doubles in tall grass from the second act, one battle in five
// (every one with the patch), never for the area's first encounter (the catch the Nuzlocke
// rules allow is a fair one) nor with a single Pokemon able to fight.
bool32 Rogue_Run0WildDouble(void)
{
    s16 x, y;
    s32 area;
    u32 i, able = 0;
    if (!Rogue_IsRun0())
        return FALSE;
    PlayerGetDestCoords(&x, &y);
    if (!MetatileBehavior_IsTallGrass(MapGridGetMetatileBehaviorAt(x, y)))
        return FALSE;
    area = Rogue_GetAreaId();
    if (area < 0 || !Rogue_IsAreaCatchUsed(area))
        return FALSE;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        if (GetMonData(mon, MON_DATA_SANITY_HAS_SPECIES) && !GetMonData(mon, MON_DATA_IS_EGG) && GetMonData(mon, MON_DATA_HP) != 0)
            able++;
    }
    if (able < 2 || Rogue_BadgeCount() < 2)
        return FALSE;
    return Rogue_Run0PatchOn(RUN0_PATCH_PAIRS) || (Random() % 5) == 0;
}

// A move of each type that hits hard, for wild Pokemon that know one against
// the player's lead (RUN0_PATCH_LEAD_MOVES): the Director patched it in.
static const u16 sAnswerMoves[NUMBER_OF_MON_TYPES] =
{
    [TYPE_NORMAL]   = MOVE_BODY_SLAM,
    [TYPE_FIGHTING] = MOVE_BRICK_BREAK,
    [TYPE_FLYING]   = MOVE_AIR_SLASH,
    [TYPE_POISON]   = MOVE_SLUDGE_BOMB,
    [TYPE_GROUND]   = MOVE_EARTHQUAKE,
    [TYPE_ROCK]     = MOVE_ROCK_SLIDE,
    [TYPE_BUG]      = MOVE_X_SCISSOR,
    [TYPE_GHOST]    = MOVE_SHADOW_BALL,
    [TYPE_STEEL]    = MOVE_IRON_HEAD,
    [TYPE_FIRE]     = MOVE_FLAMETHROWER,
    [TYPE_WATER]    = MOVE_SURF,
    [TYPE_GRASS]    = MOVE_ENERGY_BALL,
    [TYPE_ELECTRIC] = MOVE_THUNDERBOLT,
    [TYPE_PSYCHIC]  = MOVE_PSYCHIC,
    [TYPE_ICE]      = MOVE_ICE_BEAM,
    [TYPE_DRAGON]   = MOVE_DRAGON_PULSE,
    [TYPE_DARK]     = MOVE_DARK_PULSE,
    [TYPE_FAIRY]    = MOVE_MOONBLAST,
};

// The attacking type that hits a species hardest (TYPE_NONE if none hits
// it better than normally).
u32 Rogue_Run0AnswerType(u16 species, u32 salt)
{
    u32 t, best = TYPE_NONE, bestMod = UQ_4_12(1.0), n = 0;
    enum Type t1 = GetSpeciesType(species, 0), t2 = GetSpeciesType(species, 1);
    for (t = TYPE_NORMAL; t < NUMBER_OF_MON_TYPES; t++)
    {
        u32 mod;
        if (sAnswerMoves[t] == MOVE_NONE)
            continue;
        mod = GetTypeModifier(t, t1);
        if (t2 != t1)
            mod = mod * GetTypeModifier(t, t2) / UQ_4_12(1.0);
        if (mod > bestMod)
        {
            bestMod = mod;
            best = t;
            n = 1;
        }
        else if (mod == bestMod && best != TYPE_NONE && (RogueRng_Hash(salt, t) % ++n) == 0)
        {
            best = t;
        }
    }
    return best;
}

u16 Rogue_Run0AnswerMove(u32 type)
{
    return type < NUMBER_OF_MON_TYPES ? sAnswerMoves[type] : MOVE_NONE;
}

// Puts move over the weakest attack (or an empty slot) of a Pokemon.
void Rogue_Run0TeachAnswer(struct Pokemon *mon, u16 move)
{
    u32 i, slot = MAX_MON_MOVES, weakest = 0xFFFF;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        u16 m = GetMonData(mon, MON_DATA_MOVE1 + i);
        u32 power;
        if (m == move)
            return;
        if (m == MOVE_NONE)
        {
            slot = i;
            break;
        }
        power = GetMoveCategory(m) == DAMAGE_CATEGORY_STATUS ? 0 : GetMovePower(m);
        if (power < weakest)
        {
            weakest = power;
            slot = i;
        }
    }
    if (slot < MAX_MON_MOVES)
        SetMonMoveSlot(mon, move, slot);
}

// What an area's catch never knows: setting up or anything else on itself
// (Focus Energy, Swords Dance, Recover, Protect), healing from the foe,
// fainting on purpose. (Wild catches at the floor with Focus Energy and
// Drill Run, Recover, Explosion: the playtests' deadliest battles.)
static bool32 CatchMoveBanned(u16 move)
{
    if (IsExplosionMove(move))
        return TRUE;
    switch (GetMoveEffect(move))
    {
    case EFFECT_FINAL_GAMBIT:
    case EFFECT_MEMENTO:
    case EFFECT_HEALING_WISH:
    case EFFECT_LUNAR_DANCE:
    case EFFECT_LEECH_SEED:
    case EFFECT_PAIN_SPLIT:
    case EFFECT_STRENGTH_SAP:
    case EFFECT_PSYCH_UP:
        return TRUE;
    default:
        break;
    }
    return GetMoveCategory(move) == DAMAGE_CATEGORY_STATUS
        && (GetMoveTarget(move) == TARGET_USER || GetMoveTarget(move) == TARGET_USER_AND_ALLY
         || GetMoveTarget(move) == TARGET_USER_OR_ALLY);
}

// An area's catch knows the ordinary wild moves: the last four it learned
// by its level, leaving out the ones above.
static void CatchMoves(struct Pokemon *mon)
{
    u16 species = GetMonData(mon, MON_DATA_SPECIES), moves[MAX_MON_MOVES] = {0};
    u32 level = GetMonData(mon, MON_DATA_LEVEL), i, k, n = 0;
    const struct LevelUpMove *ls = GetSpeciesLevelUpLearnset(species);
    for (i = 0; ls != NULL && ls[i].move != LEVEL_UP_MOVE_END && i < 128; i++)
    {
        u16 move = ls[i].move;
        if (ls[i].level > level || move == MOVE_NONE || CatchMoveBanned(move))
            continue;
        for (k = 0; k < n && moves[k] != move; k++)
            ;
        if (k < n)
            continue;
        if (n == MAX_MON_MOVES)
        {
            memmove(moves, moves + 1, sizeof(moves[0]) * (MAX_MON_MOVES - 1));
            n--;
        }
        moves[n++] = move;
    }
    if (n == 0)
        return;
    for (k = 0; k < MAX_MON_MOVES; k++)
        SetMonMoveSlot(mon, moves[k], k);
}

void Rogue_Run0OnWildMon(struct Pokemon *mon, bool32 isCatch)
{
    bool32 was;
    struct RogueDirector *d;
    u16 lead = SPECIES_NONE;
    u32 type;
    if (!Rogue_IsRun0())
        return;
    // The catch: ordinary moves, and no move patched in against the lead.
    if (isCatch)
    {
        CatchMoves(mon);
        return;
    }
    if (!Rogue_Run0PatchOn(RUN0_PATCH_LEAD_MOVES))
        return;
    d = Peek(&was);
    if (d != NULL)
        lead = Challenge(d)->leadSpecies;
    Unpeek(was);
    if (lead == SPECIES_NONE)
        return;
    type = Rogue_Run0AnswerType(lead, GetMonData(mon, MON_DATA_PERSONALITY));
    if (type != TYPE_NONE)
        Rogue_Run0TeachAnswer(mon, sAnswerMoves[type]);
}

// ---------------------------------------------------------------------------
// The Director's own on its trainers, and giving losses back

static u32 CurrentAct(void)
{
    return 1 + Rogue_BadgeCount() / 2;
}

static bool32 KeptUsable(const struct RogueKeptMon *k)
{
    return k->species != SPECIES_NONE && k->species < NUM_SPECIES && !(k->flags & ROGUE_KEPT_RETURNED);
}

// The next kept Pokemon not yet on a trainer this act, from where the last
// one left off: each once an act, in turn.
bool32 Rogue_Run0PickKept(u32 *index, u16 *species)
{
    bool32 was, found = FALSE;
    struct RogueDirector *d = Peek(&was);
    u32 n, j, shown = 0, start = 0;
    if (d == NULL)
        return FALSE;
    n = d->keptCount < ROGUE_KEPT_MAX ? d->keptCount : ROGUE_KEPT_MAX;
    if (RUN0_CHALLENGE(d)->version == RUN0_CHALLENGE_VERSION)
    {
        const struct RogueRun0Challenge *c = RUN0_CHALLENGE(d);
        if (c->keptAct == CurrentAct())
            shown = c->keptShown;
        start = c->keptNext;
    }
    for (j = 0; j < n && !found; j++)
    {
        u32 i = (start + j) % n;
        if (KeptUsable(&d->kept[i]) && !(shown & (1u << i)))
        {
            *index = i;
            *species = d->kept[i].species;
            found = TRUE;
        }
    }
    Unpeek(was);
    return found;
}

void Rogue_Run0MarkKeptShown(struct RogueDirector *d, u32 index)
{
    struct RogueRun0Challenge *c = Challenge(d);
    if (index >= ROGUE_KEPT_MAX)
        return;
    if (c->keptAct != CurrentAct())
    {
        c->keptAct = CurrentAct();
        c->keptShown = 0;
    }
    c->keptShown |= 1u << index;
    c->keptNext = index + 1;
}

static void SetLevel(struct Pokemon *mon, u32 level)
{
    u32 exp = gExperienceTables[gSpeciesInfo[GetMonData(mon, MON_DATA_SPECIES)].growthRate][level];
    SetMonData(mon, MON_DATA_EXP, &exp);
    CalculateMonStats(mon);
}

void Rogue_Run0RaiseToFloor(struct Pokemon *mon)
{
    u32 floor;
    if (!Rogue_IsRun0() || !GetMonData(mon, MON_DATA_SANITY_HAS_SPECIES) || GetMonData(mon, MON_DATA_IS_EGG))
        return;
    floor = Rogue_Run0BandFloor();
    if (floor > MAX_LEVEL)
        floor = MAX_LEVEL;
    if (GetMonData(mon, MON_DATA_LEVEL) < floor)
        SetLevel(mon, floor);
}

// Caught (Cmd_givecaughtmon): at the floor, with the four a player keeps.
void Rogue_Run0OnCaught(struct Pokemon *mon)
{
    u16 moves[MAX_MON_MOVES];
    u32 k;
    if (!Rogue_IsRun0() || !GetMonData(mon, MON_DATA_SANITY_HAS_SPECIES))
        return;
    Rogue_Run0RaiseToFloor(mon);
    if (Rogue_Run0BestMoves(GetMonData(mon, MON_DATA_SPECIES), GetMonData(mon, MON_DATA_LEVEL), moves) == 0)
        return;
    for (k = 0; k < MAX_MON_MOVES; k++)
        SetMonMoveSlot(mon, moves[k], k);
}

// Taken from the PC into the party (src/pokemon_storage_system.c).
void Rogue_Run0OnWithdraw(struct Pokemon *mon)
{
    Rogue_Run0RaiseToFloor(mon);
}

// A checkpoint start: the base whose band at this many badges is nearest
// the party's median, so the act starts where the party stands.
void Rogue_Run0ChallengeRebase(struct RogueDirector *d, u32 badges)
{
    struct RogueRun0Challenge *c = Challenge(d);
    u32 median = PartyMedianLevel(), base, best = c->baseLevel, bestGap = 0xFF;
    u16 band = VarGet(VAR_ROGUE_RUN0_BAND);
    if (badges == 0)
        return;
    for (base = RUN0_BASE_MIN; base <= RUN0_BASE_MAX; base++)
    {
        u32 level = Rogue_Run0AceFor(base, badges - 1);
        u32 gap = level > median ? level - median : median - level;
        if (gap < bestGap || (gap == bestGap && base > best))
        {
            best = base;
            bestGap = gap;
        }
    }
    c->baseLevel = best;
    VarSet(VAR_ROGUE_RUN0_BAND, ROGUE_RUN0_BAND_VALUE(best, ROGUE_RUN0_BAND_LOSS(band), ROGUE_RUN0_BAND_ANSWER(band)));
}

// The Director's words when it gives one back. The story's to write in the
// Director's voice (calm, short, polite, Terry's habits); this is a stand-in,
// weak so the story's own Rogue_Run0ReturnLine takes its place.
__attribute__((weak)) u8 *Rogue_Run0ReturnLine(u8 *dest, const u8 *nickname, bool32 toPC)
{
    dest = StringCopy(dest, COMPOUND_STRING("I kept "));
    dest = StringCopy(dest, nickname);
    dest = StringCopy(dest, COMPOUND_STRING(" for you.\nIt's healed. You can have it back.\p"));
    if (toPC)
        dest = StringCopy(dest, COMPOUND_STRING("Your team is full. It's in your PC.\p"));
    return StringCopy(dest, COMPOUND_STRING("That's okay. I have others."));
}

void Rogue_Run0ReturnKept(void)
{
    struct RogueDirector *d;
    struct RogueRun0Challenge *c;
    struct Pokemon *mon;
    u8 name[POKEMON_NAME_LENGTH + 1];
    u32 badges = Rogue_BadgeCount(), i, n, pick = ROGUE_KEPT_MAX;
    bool32 toPC;

    gSpecialVar_Result = FALSE;
    if (!Rogue_IsRun0() || badges == 0 || badges > 15)
        return;
    d = Rogue_Director();
    if (d == NULL)
        return;
    c = Challenge(d);
    n = d->keptCount < ROGUE_KEPT_MAX ? d->keptCount : ROGUE_KEPT_MAX;
    // Once a badge; the newest it kept before the leader's battle (never
    // one that fainted in it).
    if (!(c->returned & (1u << badges)))
        for (i = n; i-- > 0 && pick == ROGUE_KEPT_MAX;)
            if (KeptUsable(&d->kept[i]) && !(d->kept[i].flags & ROGUE_KEPT_NEW))
                pick = i;
    mon = pick < ROGUE_KEPT_MAX ? AllocZeroed(sizeof(struct Pokemon)) : NULL;
    if (mon == NULL)
    {
        Rogue_ReleaseDirector();
        return;
    }
    Rogue_Run0BuildKeptMon(mon, &d->kept[pick], Rogue_Run0BandLevel());
    toPC = CalculatePlayerPartyCount() >= PARTY_SIZE;
    if (!toPC)
    {
        gParties[B_TRAINER_PLAYER][gPartiesCount[B_TRAINER_PLAYER]] = *mon;
        CalculatePlayerPartyCount();
    }
    else if (CopyMonToPC(mon) != MON_GIVEN_TO_PC)
    {
        Free(mon);
        Rogue_ReleaseDirector();
        return;
    }
    d->kept[pick].flags |= ROGUE_KEPT_RETURNED;
    c->returned |= 1u << badges;
    memcpy(name, d->kept[pick].nickname, POKEMON_NAME_LENGTH);
    name[POKEMON_NAME_LENGTH] = EOS;
    Rogue_Run0ReturnLine(gStringVar4, name, toPC);
    Free(mon);
    Rogue_SaveDirector();
    gSpecialVar_Result = TRUE;
}
