// The Game Corner's prize counter and the Mega Stones (v1.1.7).
//
// The prize desk of the region's one Game Corner (RogueGen_GameCornerTown;
// first a clerk in every gym town's Mart), open from the 2nd badge
// (ROGUE_PRIZE_BADGES). Paid with money: 2 or 3 TMs and 2 held items from
// the seed, the town and the badges (RogueGen_PrizeStock), so the stock
// changes each run.
//
// Megas match the player's own Pokemon, since in a Nuzlocke a stone for a
// Pokemon the player never caught is no use:
// - The 5th gym leader gives the MEGA RING and a stone for a Pokemon in the
//   party that can Mega Evolve, counting whole evolution lines (a CHARMANDER
//   gets a CHARIZARDITE). With several to choose from (two party members,
//   or CHARIZARD's X and Y), the player picks from a menu. With none, the
//   leader says so, and the first prize counter visit with a fitting
//   Pokemon hands one over free.
// - From the 5th badge the counter's last slot is a stone for one of the
//   player's living Pokemon, in the party or the boxes (fainted ones are
//   gone from both: Rogue_OnBattleEnd), never one already owned. It is
//   picked from the seed, the town and the badges, so leaving and coming
//   back does not change it. With nothing to fit, a TM takes the slot.
// - Legendary and mythical Pokemon's stones are never given or sold.
// - A run gains at most badges - 3 stones, and never more than
//   ROGUE_MEGA_STONE_CAP (the gift counts), and a stone costs
//   ROGUE_MEGA_STONE_PRICE.
//
// Saved: three per-run flags that vanilla leaves unused (cleared with the
// run, 0 in older saves): FLAG_ROGUE_MEGA_GIFT, the free stone given, and
// FLAG_ROGUE_MEGA_BOUGHT_0/1, stones bought (0 to 3). Tossing or releasing
// a stone gives neither the gift nor a place under the cap back. RUN 0 has
// none of this.
#include "global.h"
#include "rogue.h"
#include "rogue_gen.h"
#include "rogue_run0.h"
#include "event_data.h"
#include "constants/rogue.h"
#include "item.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "shop.h"
#include "string_util.h"
#include "constants/form_change_types.h"
#include "constants/hold_effects.h"
#include "constants/items.h"

// The 5th badge, not the 4th: a Gyarados caught early and given its
// stone at the 4th won the 5th gym 24 of 24 times, against 18 of 24 without.
#define ROGUE_MEGA_BADGES 5
#define ROGUE_MEGA_STONE_CAP 3
#define ROGUE_MEGA_STONE_PRICE 10000
#define MAX_MEGA_CHOICES 8
#define MAX_PRIZE_STOCK 10

static EWRAM_DATA u16 sPrizeStock[MAX_PRIZE_STOCK] = {0};
static EWRAM_DATA u16 sMegaChoices[MAX_MEGA_CHOICES] = {0};
static EWRAM_DATA u8 sMegaChoiceCount = 0;

bool32 Rogue_IsMegaStone(u16 item)
{
    return item != ITEM_NONE && item < ITEMS_COUNT && GetItemHoldEffect(item) == HOLD_EFFECT_MEGA_STONE;
}

static void AddUnique(u16 *list, u32 *n, u32 max, u16 item)
{
    u32 i;
    for (i = 0; i < *n; i++)
        if (list[i] == item)
            return;
    if (*n < max)
        list[(*n)++] = item;
}

// The stones a species and everything it evolves into can use.
static void StonesOfLine(u16 species, u32 depth, u16 *list, u32 *n, u32 max)
{
    const struct FormChange *fc;
    const struct Evolution *evo;
    u32 i;
    if (species == SPECIES_NONE || species >= NUM_SPECIES || depth > 3)
        return;
    fc = GetSpeciesFormChanges(species);
    // Not the legends' stones (MEWTWO, the LATIs, DIANCIE...): a caught
    // legend that Mega Evolves would make the League easy.
    if (gSpeciesInfo[species].isRestrictedLegendary || gSpeciesInfo[species].isSubLegendary || gSpeciesInfo[species].isMythical)
        fc = NULL;
    for (i = 0; fc != NULL && fc[i].method != FORM_CHANGE_TERMINATOR; i++)
        if (fc[i].method == FORM_CHANGE_BATTLE_MEGA_EVOLUTION_ITEM && fc[i].targetSpecies != species
         && Rogue_IsMegaStone(fc[i].param1))
            AddUnique(list, n, max, fc[i].param1);
    evo = GetSpeciesEvolutions(species);
    for (i = 0; evo != NULL && evo[i].method != EVOLUTIONS_END; i++)
        if (evo[i].targetSpecies != species)
            StonesOfLine(evo[i].targetSpecies, depth + 1, list, n, max);
}

// What the player has, read once per talk: the Mega Stones held by any of
// their Pokemon, and the stones that fit their living Pokemon (the party's,
// and the boxes' too), so the 420 box slots are decrypted once.
struct MegaScan
{
    u16 held[16];
    u16 fitParty[MAX_MEGA_CHOICES * 2];
    u16 fitAll[MAX_MEGA_CHOICES * 3];
    u16 seen[64]; // species already looked up, so a box of one kind costs one lookup
    u32 nHeld, nParty, nAll, nSeen;
};

static void ScanMon(struct MegaScan *sc, u16 species, u16 held, bool32 egg, bool32 party)
{
    u32 i;
    if (species == SPECIES_NONE || egg)
        return;
    if (Rogue_IsMegaStone(held))
        AddUnique(sc->held, &sc->nHeld, ARRAY_COUNT(sc->held), held);
    if (party)
        StonesOfLine(species, 0, sc->fitParty, &sc->nParty, ARRAY_COUNT(sc->fitParty));
    for (i = 0; i < sc->nSeen && sc->seen[i] != species; i++)
        ;
    if (i < sc->nSeen)
        return;
    if (sc->nSeen < ARRAY_COUNT(sc->seen))
        sc->seen[sc->nSeen++] = species;
    StonesOfLine(species, 0, sc->fitAll, &sc->nAll, ARRAY_COUNT(sc->fitAll));
}

static void Scan(struct MegaScan *sc)
{
    u32 i, b;
    memset(sc, 0, sizeof(*sc));
    for (i = 0; i < PARTY_SIZE; i++)
        ScanMon(sc, GetMonData(&gPlayerParty[i], MON_DATA_SPECIES), GetMonData(&gPlayerParty[i], MON_DATA_HELD_ITEM),
                GetMonData(&gPlayerParty[i], MON_DATA_IS_EGG), TRUE);
    for (b = 0; b < TOTAL_BOXES_COUNT; b++)
        for (i = 0; i < IN_BOX_COUNT; i++)
        {
            // The unencrypted bits first: most slots are empty.
            struct BoxPokemon *mon = &gPokemonStoragePtr->boxes[b][i];
            if (mon->hasSpecies && !mon->isEgg)
                ScanMon(sc, GetBoxMonData(mon, MON_DATA_SPECIES), GetBoxMonData(mon, MON_DATA_HELD_ITEM), FALSE, FALSE);
        }
}

// Owned: in the bag, in the PC's items or held by any of the player's
// Pokemon.
static bool32 StoneOwned(const struct MegaScan *sc, u16 stone)
{
    u32 i;
    if (CheckBagHasItem(stone, 1) || CheckPCHasItem(stone, 1))
        return TRUE;
    for (i = 0; i < sc->nHeld; i++)
        if (sc->held[i] == stone)
            return TRUE;
    return FALSE;
}

// The fitting stones not owned yet, in item order (so the pick below does
// not depend on the box order).
static u32 FittingStones(const struct MegaScan *sc, bool32 boxes, u16 *list, u32 max)
{
    const u16 *all = boxes ? sc->fitAll : sc->fitParty;
    u32 n = boxes ? sc->nAll : sc->nParty, out = 0, i, j;
    for (i = 0; i < n; i++)
        if (!StoneOwned(sc, all[i]) && out < max)
            list[out++] = all[i];
    for (i = 1; i < out; i++)
        for (j = i; j > 0 && list[j - 1] > list[j]; j--)
        {
            u16 t = list[j];
            list[j] = list[j - 1];
            list[j - 1] = t;
        }
    return out;
}

// Stones gained this run (the gift and the ones bought), from the run's
// flags: tossing or releasing a stone does not give the place back.
static u32 StonesGained(void)
{
    return FlagGet(FLAG_ROGUE_MEGA_GIFT) + FlagGet(FLAG_ROGUE_MEGA_BOUGHT_0) + 2 * FlagGet(FLAG_ROGUE_MEGA_BOUGHT_1);
}

static bool32 MegasHere(void)
{
    return !Rogue_IsRun0() && Rogue_BadgeCount() >= ROGUE_MEGA_BADGES;
}

// How many stones the run may hold now: one more for each badge from the
// ROGUE_MEGA_BADGES one's two (the gift and one bought), at most
// ROGUE_MEGA_STONE_CAP.
static u32 StoneAllowance(void)
{
    u32 badges = Rogue_BadgeCount();
    if (badges < ROGUE_MEGA_BADGES)
        return 0;
    badges -= ROGUE_MEGA_BADGES - 2;
    return badges > ROGUE_MEGA_STONE_CAP ? ROGUE_MEGA_STONE_CAP : badges;
}

// The counter's stone in this town now, or ITEM_NONE.
static u16 CounterStone(u32 place)
{
    struct MegaScan sc;
    u16 list[MAX_MEGA_CHOICES * 3];
    u32 n, badges = Rogue_BadgeCount();
    if (!MegasHere() || StonesGained() >= StoneAllowance())
        return ITEM_NONE;
    Scan(&sc);
    n = FittingStones(&sc, TRUE, list, ARRAY_COUNT(list));
    if (n == 0)
        return ITEM_NONE;
    return list[RogueRng_Hash(Rogue_GetSeed(), 18700 + place * 16 + (badges > 8 ? 8 : badges)) % n];
}

// The price of a Mega Stone at the counter. Items with no price of their
// own are not sold back (vanilla's rule), so a stone can't be sold.
u32 Rogue_PrizePrice(u32 price, u16 item)
{
    if (Rogue_IsMegaStone(item))
        return ROGUE_MEGA_STONE_PRICE;
    return price;
}

// One of each, for the shop: TRUE if the player owns this stone already.
bool32 Rogue_PrizeOwnedAlready(u16 item)
{
    struct MegaScan sc;
    if (!Rogue_IsMegaStone(item))
        return FALSE;
    if (CheckBagHasItem(item, 1) || CheckPCHasItem(item, 1))
        return TRUE;
    Scan(&sc);
    return StoneOwned(&sc, item);
}

// The shop, after a purchase: a Mega Stone bought counts toward the run's
// stones.
void Rogue_OnPrizeBought(u16 item)
{
    u32 bought;
    if (!Rogue_IsMegaStone(item))
        return;
    bought = FlagGet(FLAG_ROGUE_MEGA_BOUGHT_0) + 2 * FlagGet(FLAG_ROGUE_MEGA_BOUGHT_1) + 1;
    if (bought > 3)
        bought = 3;
    if (bought & 1) FlagSet(FLAG_ROGUE_MEGA_BOUGHT_0); else FlagClear(FLAG_ROGUE_MEGA_BOUGHT_0);
    if (bought & 2) FlagSet(FLAG_ROGUE_MEGA_BOUGHT_1); else FlagClear(FLAG_ROGUE_MEGA_BOUGHT_1);
}

static u32 ItemPrice(u16 item)
{
    return GetItemPrice(item);
}

// Script special, the prize clerk: VAR_RESULT FALSE before RogueGen_PrizeBadges
// (or in RUN 0); else TRUE and the stock ready.
void Rogue_PreparePrizeCounter(void)
{
    u32 place = Rogue_GetCurrentTownPlace(), n;
    u16 stone;
    gSpecialVar_Result = FALSE;
    if (Rogue_IsRun0() || Rogue_BadgeCount() < RogueGen_PrizeBadges(Rogue_GetRegion()))
        return;
    stone = CounterStone(place);
    n = RogueGen_PrizeStock(Rogue_GetRegion(), place, Rogue_BadgeCount(), sPrizeStock, MAX_PRIZE_STOCK - 1,
                            MegasHere() && stone == ITEM_NONE, ItemPrice);
    if (stone != ITEM_NONE)
        sPrizeStock[n++] = stone;
    sPrizeStock[n] = ITEM_NONE;
    gSpecialVar_Result = n != 0;
}

// Script special, then waitstate: opens the prize counter.
void Rogue_OpenPrizeCounter(void)
{
    CreatePokemartMenu(sPrizeStock);
}

// Script special, VAR_0x8004 0 after the 5th leader, 1 at the prize
// counter. VAR_RESULT:
//   0xFF  nothing to give here
//   0     the MEGA RING only (VAR_0x8001 TRUE): no Pokemon fits yet
//   1     one stone, in VAR_0x8000
//   2+    that many stones to choose from (Rogue_BufferMegaChoice)
// VAR_0x8001 TRUE if the MEGA RING is to be given too (always after the
// leader; at the counter, for a run saved before v1.1.7 past the 5th gym).
void Rogue_PrepareMegaGift(void)
{
    struct MegaScan sc;
    bool32 atLeader = gSpecialVar_0x8004 == 0;
    u32 n = 0;
    gSpecialVar_Result = 0xFF;
    gSpecialVar_0x8000 = ITEM_NONE;
    gSpecialVar_0x8001 = MegasHere() && !CheckBagHasItem(ITEM_MEGA_RING, 1);
    sMegaChoiceCount = 0;
    if (!MegasHere())
        return;
    if (atLeader && Rogue_BadgeCount() != ROGUE_MEGA_BADGES)
        return;
    // The free stone, once a run (FLAG_ROGUE_MEGA_GIFT): the leader's, or
    // the one the leader could not give.
    if (!FlagGet(FLAG_ROGUE_MEGA_GIFT))
    {
        Scan(&sc);
        n = FittingStones(&sc, !atLeader, sMegaChoices, MAX_MEGA_CHOICES);
    }
    sMegaChoiceCount = n;
    if (n == 0 && !gSpecialVar_0x8001)
        return;
    gSpecialVar_Result = n;
    if (n == 1)
        gSpecialVar_0x8000 = sMegaChoices[0];
}

// Script special: the name of choice VAR_0x8004 into STR_VAR_1.
void Rogue_BufferMegaChoice(void)
{
    u32 i = gSpecialVar_0x8004;
    CopyItemName(i < sMegaChoiceCount ? sMegaChoices[i] : ITEM_NONE, gStringVar1);
}

// Script special: the stone picked from the menu (VAR_RESULT) into
// VAR_0x8000.
void Rogue_TakeMegaChoice(void)
{
    u32 i = gSpecialVar_Result;
    gSpecialVar_0x8000 = sMegaChoiceCount == 0 ? ITEM_NONE : sMegaChoices[i < sMegaChoiceCount ? i : 0];
}

// Script special: how many stones there are to choose from, into
// VAR_RESULT.
void Rogue_MegaChoiceCount(void)
{
    gSpecialVar_Result = sMegaChoiceCount;
}

// Script special, as the free stone is given: once a run.
void Rogue_MarkMegaGiftGiven(void)
{
    FlagSet(FLAG_ROGUE_MEGA_GIFT);
}

// Test hook for tools/roguemon/leaderbench.py --mega: set before a leader
// battle, gives the MEGA RING and puts a fitting stone on the first party
// member whose line can Mega Evolve (as the 5th leader's gift would).
EWRAM_DATA u8 gRogueTestMega = 0;

void Rogue_ApplyTestMega(void)
{
    u32 i;
    if (!gRogueTestMega)
        return;
    gRogueTestMega = 0;
    if (!CheckBagHasItem(ITEM_MEGA_RING, 1))
        AddBagItem(ITEM_MEGA_RING, 1);
    for (i = 0; i < PARTY_SIZE; i++)
    {
        u16 list[4];
        u32 n = 0;
        StonesOfLine(GetMonData(&gPlayerParty[i], MON_DATA_SPECIES), 0, list, &n, ARRAY_COUNT(list));
        if (n != 0 && !GetMonData(&gPlayerParty[i], MON_DATA_IS_EGG))
        {
            u16 item = list[0];
            SetMonData(&gPlayerParty[i], MON_DATA_HELD_ITEM, &item);
            return;
        }
    }
}

// For the tools: the counter's stone and stock as the game would show them.
u16 Rogue_PrizeCounterStone(void)
{
    return CounterStone(Rogue_GetCurrentTownPlace());
}

// ---------------------------------------------------------------------------
// The Game Corner's prize desk, paid in COINS. The stock is the old
// counter's (RogueGen_PrizeStock and the stone), then 3 of the region's
// Pokemon from the places the player can have reached, a little under the
// level cap. Prices (balance pass, Nov 8): items a 30th
// of their money price, 100 to 400 coins; a stone 600; a Pokemon 300 (to 3
// badges), 600 (4 to 6), 1,000 (7 or more). Nothing is saved: the stock is worked out again on
// each talk.
#include "coins.h"
#include "script_pokemon_util.h"
#include "constants/pokemon.h"

#define DESK_STONE_COINS 600
#define DESK_MAX_MONS 3

// The region's one desk has three Pokemon prizes, each sold once a
// run (FLAG_ROGUE_DESK_MON_0 + its slot); the species in a slot not yet
// sold changes with each badge.
static u32 DeskMonCount(u32 place)
{
    u32 k, n = 0;
    for (k = 0; k < DESK_MAX_MONS; k++)
        n += !FlagGet(FLAG_ROGUE_DESK_MON_0 + k);
    return n;
}

// The slot of the j-th Pokemon on the menu (the slots not sold).
static u32 DeskMonSlot(u32 j)
{
    u32 k;
    for (k = 0; k < DESK_MAX_MONS; k++)
        if (!FlagGet(FLAG_ROGUE_DESK_MON_0 + k) && j-- == 0)
            return k;
    return DESK_MAX_MONS - 1;
}

// The k-th Pokemon prize: a wild species from a place of the region no
// harder than the player's badges allow, picked from the seed, the town and
// the badges.
static u16 DeskMon(u32 place, u32 k)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    u32 badges = Rogue_BadgeCount(), n = 0, i, pick, tries;
    for (i = 0; i < r->nPlaces; i++)
        if (r->places[i].area != 0xFF && r->places[i].tier <= badges + 1)
            n++;
    if (n == 0)
        return SPECIES_NONE;
    for (tries = 0; tries < 8; tries++)
    {
        u32 h = RogueRng_Hash(Rogue_GetSeed(), 19000 + place * 64 + (badges > 8 ? 8 : badges) * 8 + k + tries * 3);
        u16 species;
        pick = h % n;
        for (i = 0; i < r->nPlaces; i++)
            if (r->places[i].area != 0xFF && r->places[i].tier <= badges + 1 && pick-- == 0)
                break;
        species = Rogue_GetWildSpecies(r->places[i].area, h >> 16);
        if (species != SPECIES_NONE && species < NUM_SPECIES)
            return species;
    }
    return SPECIES_NONE;
}

// A little under the level cap (the next leader's ace), so a prize fits the
// run wherever it is bought, and never under the town's wild level.
static u8 DeskMonLevel(u32 place)
{
    u32 lvl = Rogue_GetLevelCap() * 85 / 100, wild = RogueGen_WildLevel(Rogue_GetRegion()->places[place].level);
    if (lvl < wild)
        lvl = wild;
    return lvl < 2 ? 2 : lvl > 100 ? 100 : lvl;
}

static u32 DeskMonCoins(void)
{
    u32 badges = Rogue_BadgeCount();
    return badges >= 7 ? 1000 : badges >= 4 ? 600 : 300;
}

static u32 DeskItemCoins(u16 item)
{
    u32 c;
    if (Rogue_IsMegaStone(item))
        return DESK_STONE_COINS;
    c = GetItemPrice(item) / 30;
    c = (c + 5) / 10 * 10;
    return c < 100 ? 100 : c > 400 ? 400 : c;
}

static u32 DeskItems(void)
{
    u32 n = 0;
    while (n < MAX_PRIZE_STOCK && sPrizeStock[n] != ITEM_NONE)
        n++;
    return n;
}

// Script special: VAR_RESULT the COIN CASE is owed (2 badges, not had).
void Rogue_CoinCaseDue(void)
{
    gSpecialVar_Result = !Rogue_IsRun0() && Rogue_BadgeCount() >= RogueGen_PrizeBadges(Rogue_GetRegion()) && !CheckBagHasItem(ITEM_COIN_CASE, 1);
}

// Script special, after a gym leader: VAR_RESULT TRUE once, for the badge
// that opens the way to the region's Game Corner (and at least the 2nd),
// with its town's name in STR_VAR_1.
void Rogue_GameCornerNewsDue(void)
{
    s32 town = RogueGen_GameCornerTown(Rogue_GetRegion());
    u32 at;
    gSpecialVar_Result = FALSE;
    if (Rogue_IsRun0() || town < 0)
        return;
    at = Rogue_GetRegion()->places[town].badges;
    if (at < RogueGen_PrizeBadges(Rogue_GetRegion()))
        at = RogueGen_PrizeBadges(Rogue_GetRegion());
    if (Rogue_BadgeCount() != at)
        return;
    Rogue_CopyPlaceName(gStringVar1, town);
    gSpecialVar_Result = TRUE;
}

// Script special: VAR_RESULT the desk's number of prizes, 0 while closed.
void Rogue_PrepareCoinDesk(void)
{
    u32 place = Rogue_GetCurrentTownPlace();
    Rogue_PreparePrizeCounter();
    if (Rogue_IsRun0() || Rogue_BadgeCount() < RogueGen_PrizeBadges(Rogue_GetRegion()))
    {
        gSpecialVar_Result = 0;
        return;
    }
    gSpecialVar_Result = DeskItems() + DeskMonCount(place);
}

// Script special: the VAR_0x8004-th prize and its price into STR_VAR_1.
void Rogue_BufferDeskChoice(void)
{
    u32 place = Rogue_GetCurrentTownPlace(), i = gSpecialVar_0x8004, nItems = DeskItems(), coins;
    u8 *s = gStringVar1;
    if (i < nItems)
    {
        CopyItemName(sPrizeStock[i], s);
        coins = DeskItemCoins(sPrizeStock[i]);
    }
    else
    {
        u16 species = DeskMon(place, DeskMonSlot(i - nItems));
        s = StringCopy(s, GetSpeciesName(species));
        s = StringAppend(s, COMPOUND_STRING(" Lv"));
        ConvertIntToDecimalStringN(gStringVar3, DeskMonLevel(place), STR_CONV_MODE_LEFT_ALIGN, 3);
        StringAppend(s, gStringVar3);
        coins = DeskMonCoins();
    }
    s = gStringVar1 + StringLength(gStringVar1);
    s = StringAppend(s, COMPOUND_STRING(" "));
    ConvertIntToDecimalStringN(gStringVar3, coins, STR_CONV_MODE_LEFT_ALIGN, 5);
    s = StringAppend(s, gStringVar3);
    StringAppend(s, COMPOUND_STRING("C"));
}

// Script special after the menu. VAR_RESULT: 0 left, 1 too few coins,
// 2 a stone already owned, 3 an item to give (VAR_0x8000;
// Rogue_PayDesk once given), 4 a Pokemon given (its name in
// STR_VAR_1), 5 no room for a Pokemon.
void Rogue_TakeDeskChoice(void)
{
    u32 place = Rogue_GetCurrentTownPlace(), i = gSpecialVar_Result, nItems = DeskItems(), coins;
    if (i >= nItems + DeskMonCount(place))
    {
        gSpecialVar_Result = 0;
        return;
    }
    coins = i < nItems ? DeskItemCoins(sPrizeStock[i]) : DeskMonCoins();
    if (GetCoins() < coins)
    {
        gSpecialVar_Result = 1;
        return;
    }
    if (i < nItems)
    {
        if (Rogue_PrizeOwnedAlready(sPrizeStock[i]))
        {
            gSpecialVar_Result = 2;
            return;
        }
        gSpecialVar_0x8000 = sPrizeStock[i];
        gSpecialVar_Result = 3;
        return;
    }
    else
    {
        u16 species = DeskMon(place, DeskMonSlot(i - nItems));
        if (ScriptGiveMon(species, DeskMonLevel(place), ITEM_NONE) == MON_CANT_GIVE)
        {
            gSpecialVar_Result = 5;
            return;
        }
        RemoveCoins(coins);
        FlagSet(FLAG_ROGUE_DESK_MON_0 + DeskMonSlot(i - nItems));
        StringCopy(gStringVar1, GetSpeciesName(species));
        gSpecialVar_Result = 4;
    }
}

// Script special, once the item is in the bag: takes its COINS and counts
// a stone toward the run's.
void Rogue_PayDesk(void)
{
    // (Std_ObtainItem keeps the item in VAR_0x8006; VAR_0x8000 may not
    // survive its message.)
    RemoveCoins(DeskItemCoins(gSpecialVar_0x8006));
    Rogue_OnPrizeBought(gSpecialVar_0x8006);
}
