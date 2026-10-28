// Nuzlocke rules, run results and the permanent record.
//
// A run ends when the whole party faints or when the Champion is beaten. The
// record (runs, wins, best result, unlocks) lives in a few vars that are
// carried across New Game, so it survives as long as the save file does.

#include "global.h"
#include "rogue.h"
#include "rogue_run0_story.h"
#include "rogue_gen.h"
#include "battle.h"
#include "battle_setup.h"
#include "credits.h"
#include "event_data.h"
#include "hall_of_fame.h"
#include "item.h"
#include "main.h"
#include "malloc.h"
#include "overworld.h"
#include "pokedex.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "save.h"
#include "script.h"
#include "script_pokemon_util.h"
#include "string_util.h"
#include "constants/characters.h"
#include "constants/items.h"
#include "constants/map_groups.h"
#include "field_move.h"

enum
{
    CATCH_FREE,  // not a wild battle under the one-catch rule
    CATCH_FIRST, // the area's first encounter: may be caught
    CATCH_DUPE,  // already owned: does not count and cannot be caught
    CATCH_USED,  // the area's encounter is already used
    CATCH_SHINY, // a shiny the Shiny clause lets the player catch anyway
};

// Test hook: the playtest reads this to check the one-catch rule.
EWRAM_DATA u8 gRogueCatchState = CATCH_FREE;
// Names of Pokemon that fainted in the last battle, for the farewell message.
static EWRAM_DATA u8 sFallenNames[PARTY_SIZE][POKEMON_NAME_LENGTH + 1] = {0};
static EWRAM_DATA u8 sFallenCount = 0;

extern const u8 Rogue_EventScript_Fallen[];
static void BufferFallen(void);
static const u8 sText_And[] = _(" and ");
static const u8 sText_FallenOne[] = _(" fainted.\nIt is gone for good...");
static const u8 sText_FallenMany[] = _(" fainted.\nThey are gone for good...");
// RUN 0: the Director keeps them (and gives them back after STOP).
static const u8 sText_Run0FallenOne[] = _(" fainted.\pI'll keep it.");
static const u8 sText_Run0FallenMany[] = _(" fainted.\pI'll keep them.");

static const u8 sText_CatchUsed[] = _("You already met a POKéMON here.\nYou can't catch this one.\p");
static const u8 sText_CatchDupe[] = _("You already have this POKéMON.\nIt doesn't count. Keep looking!\p");

struct UnlockDef
{
    const u8 *name;
    const u8 *hint;        // how to earn it, shown while it is locked
    const u8 *description; // what it does
};

static const struct UnlockDef sUnlocks[ROGUE_NUM_UNLOCKS] =
{
    [UNLOCK_PARTNERS] = {
        COMPOUND_STRING("PARTNER STARTERS"),
        COMPOUND_STRING("Win a GYM BADGE."),
        COMPOUND_STRING("A starter choice: a partner like\nPIKACHU, RIOLU or ZORUA."),
    },
    [UNLOCK_DRAGONS] = {
        COMPOUND_STRING("DRAGON STARTERS"),
        COMPOUND_STRING("Win three GYM BADGES in one run."),
        COMPOUND_STRING("Another starter choice: a young\ndragon like DRATINI or BAGON."),
    },
    [UNLOCK_KIT] = {
        COMPOUND_STRING("CHAMPION'S KIT"),
        COMPOUND_STRING("Become CHAMPION."),
        COMPOUND_STRING("GREAT BALLS and SUPER POTIONS in\nthe starting kit."),
    },
    [UNLOCK_VETERAN] = {
        COMPOUND_STRING("VETERAN'S COIN"),
        COMPOUND_STRING("Start three runs."),
        COMPOUND_STRING("An AMULET COIN in the kit. A POKéMON\nholding it doubles prize money."),
    },
    [UNLOCK_SCOUTING] = {
        COMPOUND_STRING("GYM SCOUTING"),
        COMPOUND_STRING("Lose to a GYM LEADER."),
        COMPOUND_STRING("GYM guides tell you the LEADER's\nteam and the ace's level."),
    },
    [UNLOCK_BERRIES] = {
        COMPOUND_STRING("BERRY POUCH"),
        COMPOUND_STRING("Lose two runs."),
        COMPOUND_STRING("ORAN and SITRUS BERRIES in the\nstarting kit, to hold in battle."),
    },
    [UNLOCK_SPARE_STARTERS] = {
        COMPOUND_STRING("SPARE STARTERS"),
        COMPOUND_STRING("Lose three runs."),
        COMPOUND_STRING("The starters you didn't pick wait\nin the 1st and 4th GYM towns."),
    },
    [UNLOCK_FOSSIL_STARTERS] = {
        COMPOUND_STRING("FOSSIL STARTERS"),
        COMPOUND_STRING("Revive a fossil."),
        COMPOUND_STRING("A starter choice brought back\nfrom a fossil."),
    },
    [UNLOCK_FALLEN_FRIEND] = {
        COMPOUND_STRING("FALLEN FRIEND"),
        COMPOUND_STRING("Lose a run with 4 BADGES or more."),
        COMPOUND_STRING("Your longest companion of your last\nlost run is a starter choice."),
    },
    [UNLOCK_CHAMP_PARTNER] = {
        COMPOUND_STRING("CHAMPION'S PARTNER"),
        COMPOUND_STRING("Become CHAMPION."),
        COMPOUND_STRING("Your winning lead's kind as a\nstarter choice."),
    },
    [UNLOCK_TYPE_CHARM] = {
        COMPOUND_STRING("TYPE CHARM"),
        COMPOUND_STRING("Win 2 BADGES with your starter."),
        COMPOUND_STRING("Starters of the types shown hold\nan item of their type."),
    },
    [UNLOCK_HIDDEN_ABILITY] = {
        COMPOUND_STRING("HIDDEN ABILITY"),
        COMPOUND_STRING("Win 4 BADGES with your starter."),
        COMPOUND_STRING("Starters of the types shown come\nwith their hidden ability."),
    },
    [UNLOCK_EGG_MOVE] = {
        COMPOUND_STRING("EGG MOVE"),
        COMPOUND_STRING("Win 8 BADGES with your starter."),
        COMPOUND_STRING("Starters of the types shown know\na move they could only breed."),
    },
    [UNLOCK_SHINY_CLAUSE] = {
        COMPOUND_STRING("SHINY CLAUSE"),
        COMPOUND_STRING("Meet a shiny POKéMON."),
        COMPOUND_STRING("You may always catch a shiny\nPOKéMON, even where you can't."),
    },
    [UNLOCK_SHINY_CHARM] = {
        COMPOUND_STRING("SHINY CHARM"),
        COMPOUND_STRING("Meet three shiny POKéMON."),
        COMPOUND_STRING("A SHINY CHARM at the start:\nshinies appear three times as often."),
    },
    [UNLOCK_FOSSILS] = {
        COMPOUND_STRING("FOSSILS"),
        COMPOUND_STRING("Explore five caves."),
        COMPOUND_STRING("A fossil hides in the cave before\nthe 4th GYM. A scientist revives it."),
    },
    [UNLOCK_RARE_FINDS] = {
        COMPOUND_STRING("RARE FINDS"),
        COMPOUND_STRING("Catch 50 POKéMON."),
        COMPOUND_STRING("A rare POKéMON lives off the main\nroad. The PROFESSOR knows where."),
    },
    [UNLOCK_RIVAL_GIFT] = {
        COMPOUND_STRING("RIVAL'S GIFT"),
        COMPOUND_STRING("Beat your rival ten times."),
        COMPOUND_STRING("Your rival gives you a held item\nat the foot of VICTORY ROAD."),
    },
    [UNLOCK_HARDCORE] = {
        COMPOUND_STRING("HARDCORE RULES"),
        COMPOUND_STRING("Become CHAMPION."),
        COMPOUND_STRING("Level caps at each GYM ace, no\nitems in battle and SET style."),
    },
    [UNLOCK_EXPLORER] = {
        COMPOUND_STRING("EXPLORER'S KIT"),
        COMPOUND_STRING("Clear five dungeons."),
        COMPOUND_STRING("ESCAPE ROPES and a MAX REPEL in\nthe starting kit."),
    },
    [UNLOCK_MARKING_MAP] = {
        COMPOUND_STRING("MARKING MAP"),
        COMPOUND_STRING("Catch three different legends."),
        COMPOUND_STRING("The region map shows where each\nlegend sleeps, and which one."),
    },
    [UNLOCK_MASTER_BALL] = {
        COMPOUND_STRING("MASTER BALL"),
        COMPOUND_STRING("Catch ten different legends."),
        COMPOUND_STRING("A MASTER BALL in the kit.\nIt never fails to catch."),
    },
    [UNLOCK_DOWSING] = {
        COMPOUND_STRING("DOWSING MACHINE"),
        COMPOUND_STRING("Clear every dungeon, then win."),
        COMPOUND_STRING("A DOWSING MACHINE in the kit.\nIt finds hidden items."),
    },
    [UNLOCK_LUCKY_EGG] = {
        COMPOUND_STRING("LUCKY EGG"),
        COMPOUND_STRING("Lose four runs."),
        COMPOUND_STRING("A LUCKY EGG in the kit. A POKéMON\nholding it gains more EXP."),
    },
    [UNLOCK_TRAINER_CANDY] = {
        COMPOUND_STRING("TRAINER'S CANDY"),
        COMPOUND_STRING("Win four GYM BADGES in one run."),
        COMPOUND_STRING("The 4th to 6th GYM LEADERS also\ngive 3 RARE CANDIES with the badge."),
    },
    [UNLOCK_LATE_CAMP] = {
        COMPOUND_STRING("ROADSIDE CAMP"),
        COMPOUND_STRING("Win five GYM BADGES in one run."),
        COMPOUND_STRING("A camper on the road to the 7th\nGYM lets your team rest once."),
    },
    [UNLOCK_SECOND_CHANCE] = {
        COMPOUND_STRING("SECOND CHANCE"),
        COMPOUND_STRING("Lose five runs."),
        COMPOUND_STRING("From the 4th BADGE, 3 areas whose\nfirst POKéMON got away stay open."),
    },
    [UNLOCK_EEVEE] = {
        COMPOUND_STRING("EEVEE"),
        COMPOUND_STRING("Start a second run."),
        COMPOUND_STRING("EEVEE is a starter choice in\nevery region."),
    },
    [UNLOCK_BABIES] = {
        COMPOUND_STRING("BABY POKéMON"),
        COMPOUND_STRING("Start a second run."),
        COMPOUND_STRING("A starter choice: a baby like\nPICHU, AZURILL or MUNCHLAX."),
    },
    [UNLOCK_RISING_STARS] = {
        COMPOUND_STRING("RISING STARS"),
        COMPOUND_STRING("Start three runs."),
        COMPOUND_STRING("A starter choice that grows in\nthree stages, like ARON or MAREEP."),
    },
    [UNLOCK_STARTER_CASE] = {
        COMPOUND_STRING("STARTER CASE"),
        COMPOUND_STRING("Start four runs."),
        COMPOUND_STRING("{LEFT_ARROW}{RIGHT_ARROW} on a GRASS, FIRE or WATER\nstarter: any of the 27 starters."),
    },
    [UNLOCK_FAR_AWAY] = {
        COMPOUND_STRING("FROM FAR AWAY"),
        COMPOUND_STRING("Start six runs."),
        COMPOUND_STRING("A starter choice from far away,\nlike an ALOLAN VULPIX."),
    },
    [UNLOCK_FREE_CHOICE] = {
        COMPOUND_STRING("FREE CHOICE"),
        COMPOUND_STRING("Start eight runs."),
        COMPOUND_STRING("{LEFT_ARROW}{RIGHT_ARROW} on any starter: every\nPOKéMON of that kind."),
    },
    [UNLOCK_POKEDEX_PICK] = {
        COMPOUND_STRING("POKéDEX PICK"),
        COMPOUND_STRING("Start ten runs."),
        COMPOUND_STRING("A starter choice: any POKéMON you\never caught. Shinies stay shiny!"),
    },
    [UNLOCK_ANY_POKEMON] = {
        COMPOUND_STRING("ANY POKéMON"),
        COMPOUND_STRING("Become CHAMPION three times."),
        COMPOUND_STRING("Any POKéMON as a starter, and\nthe legends you have caught."),
    },
    [UNLOCK_NATIONAL_DEX] = {
        COMPOUND_STRING("NATIONAL DEX"),
        COMPOUND_STRING("Become CHAMPION."),
        COMPOUND_STRING("More POKéMON from later regions.\nPick it under POKéDEX in UNLOCKS."),
    },
    [UNLOCK_GLOBAL_DEX] = {
        COMPOUND_STRING("GLOBAL DEX"),
        COMPOUND_STRING("Become CHAMPION three times."),
        COMPOUND_STRING("POKéMON of all nine regions alike.\nPick it under POKéDEX in UNLOCKS."),
    },
};

// ---------------------------------------------------------------------------
// The POKeDEX mix: CLASSIC, or an upgrade the professor hands out after the
// HALL OF FAME. The record keeps which upgrades are earned, and the choice as
// which of them is switched on (none: CLASSIC).

static const u8 *const sDexChoiceNames[ROGUE_MIX_COUNT] =
{
    [ROGUE_MIX_LEGACY]   = COMPOUND_STRING("CLASSIC"),
    [ROGUE_MIX_CLASSIC]  = COMPOUND_STRING("CLASSIC"),
    [ROGUE_MIX_NATIONAL] = COMPOUND_STRING("NATIONAL"),
    [ROGUE_MIX_GLOBAL]   = COMPOUND_STRING("GLOBAL"),
};

static const u8 *const sDexChoiceDescriptions[ROGUE_MIX_COUNT] =
{
    [ROGUE_MIX_LEGACY]   = COMPOUND_STRING("Mostly KANTO, JOHTO and HOENN."),
    [ROGUE_MIX_CLASSIC]  = COMPOUND_STRING("Mostly KANTO, JOHTO and HOENN."),
    [ROGUE_MIX_NATIONAL] = COMPOUND_STRING("More from SINNOH, UNOVA and KALOS."),
    [ROGUE_MIX_GLOBAL]   = COMPOUND_STRING("All nine regions, as many of each."),
};

u32 Rogue_GetDexChoice(void)
{
    if (Rogue_IsUnlockOn(UNLOCK_GLOBAL_DEX))
        return ROGUE_MIX_GLOBAL;
    if (Rogue_IsUnlockOn(UNLOCK_NATIONAL_DEX))
        return ROGUE_MIX_NATIONAL;
    return ROGUE_MIX_CLASSIC;
}

void Rogue_SetDexChoice(u32 mix)
{
    Rogue_SetUnlockEnabled(UNLOCK_NATIONAL_DEX, mix == ROGUE_MIX_NATIONAL);
    Rogue_SetUnlockEnabled(UNLOCK_GLOBAL_DEX, mix == ROGUE_MIX_GLOBAL);
}

u32 Rogue_NextDexChoice(u32 mix)
{
    if (mix <= ROGUE_MIX_CLASSIC && Rogue_IsUnlockEarned(UNLOCK_NATIONAL_DEX))
        return ROGUE_MIX_NATIONAL;
    if (mix <= ROGUE_MIX_NATIONAL && Rogue_IsUnlockEarned(UNLOCK_GLOBAL_DEX))
        return ROGUE_MIX_GLOBAL;
    return ROGUE_MIX_CLASSIC;
}

const u8 *Rogue_GetDexChoiceName(u32 mix)
{
    return sDexChoiceNames[mix % ROGUE_MIX_COUNT];
}

const u8 *Rogue_GetDexChoiceDescription(u32 mix)
{
    return sDexChoiceDescriptions[mix % ROGUE_MIX_COUNT];
}

// Feats: goals that last past a win. Each has how to do it and what it
// gives, if anything beyond the record. The first four stars of the trainer
// card come from feats (CountPlayerTrainerStars).
struct FeatDef
{
    const u8 *name;
    const u8 *hint;
    const u8 *reward; // NULL if none
};

static const u8 sFeatStar[] = _("A star on your TRAINER CARD.");
static const u8 sFeatTypeStar[] = _("All three: a TRAINER CARD star.");

static const struct FeatDef sFeats[ROGUE_NUM_FEATS] =
{
    [FEAT_CHAMPION]     = { COMPOUND_STRING("CHAMPION"),       COMPOUND_STRING("Beat the CHAMPION."), sFeatStar },
    [FEAT_HARDCORE]     = { COMPOUND_STRING("HARDCORE"),       COMPOUND_STRING("Win under the HARDCORE RULES."), sFeatStar },
    [FEAT_WIN_GRASS]    = { COMPOUND_STRING("GRASS STARTER"),  COMPOUND_STRING("Win with a GRASS starter."), sFeatTypeStar },
    [FEAT_WIN_FIRE]     = { COMPOUND_STRING("FIRE STARTER"),   COMPOUND_STRING("Win with a FIRE starter."), sFeatTypeStar },
    [FEAT_WIN_WATER]    = { COMPOUND_STRING("WATER STARTER"),  COMPOUND_STRING("Win with a WATER starter."), sFeatTypeStar },
    [FEAT_FLAWLESS]     = { COMPOUND_STRING("NOBODY LEFT BEHIND"), COMPOUND_STRING("Win without losing a POKéMON."), NULL },
    [FEAT_ALL_DUNGEONS] = { COMPOUND_STRING("EVERY DUNGEON"),  COMPOUND_STRING("Clear every dungeon, then win."), COMPOUND_STRING("A DOWSING MACHINE in the kit.") },
    [FEAT_LEGEND]       = { COMPOUND_STRING("FIRST LEGEND"),   COMPOUND_STRING("Catch a legendary POKéMON."), NULL },
    [FEAT_LEGENDS_3]    = { COMPOUND_STRING("LEGEND TRACKER"), COMPOUND_STRING("Catch three different legends."), COMPOUND_STRING("The MARKING MAP.") },
    [FEAT_LEGEND_SET]   = { COMPOUND_STRING("A WHOLE SET"),    COMPOUND_STRING("Catch every legend of a set."), NULL },
    [FEAT_LEGENDS_10]   = { COMPOUND_STRING("LEGEND HUNTER"),  COMPOUND_STRING("Catch ten different legends."), COMPOUND_STRING("A MASTER BALL, and a card star.") },
    [FEAT_ALL_LEGENDS]  = { COMPOUND_STRING("LEGEND LOG"),     COMPOUND_STRING("Catch all 29 legends."), NULL },
    [FEAT_RUN0]         = { COMPOUND_STRING("RUN 0"),          COMPOUND_STRING("Finish RUN 0."), sFeatStar },
};

// The LEGEND LOG's rows, as the legends come in a region (see
// src/rogue_gen_legend.h): the trios and pairs, then the lone ones.
const u8 gRogueLegendRows[ROGUE_LEGEND_ROWS][4] =
{
    { RLEG_REGIROCK, RLEG_REGICE, RLEG_REGISTEEL },
    { RLEG_ARTICUNO, RLEG_ZAPDOS, RLEG_MOLTRES },
    { RLEG_RAIKOU, RLEG_ENTEI, RLEG_SUICUNE },
    { RLEG_UXIE, RLEG_MESPRIT, RLEG_AZELF },
    { RLEG_COBALION, RLEG_TERRAKION, RLEG_VIRIZION },
    { RLEG_TORNADUS, RLEG_THUNDURUS, RLEG_LANDORUS },
    { RLEG_LATIAS, RLEG_LATIOS },
    { RLEG_LUGIA, RLEG_HO_OH },
    { RLEG_KYOGRE, RLEG_GROUDON, RLEG_RAYQUAZA },
    { RLEG_REGIGIGAS, RLEG_HEATRAN, RLEG_CRESSELIA, RLEG_MEWTWO },
};

static const u8 *const sLegendRowNames[ROGUE_LEGEND_ROWS][2] =
{
    { COMPOUND_STRING("REGIS"),   COMPOUND_STRING("the three REGIS") },
    { COMPOUND_STRING("BIRDS"),   COMPOUND_STRING("the legendary birds") },
    { COMPOUND_STRING("BEASTS"),  COMPOUND_STRING("the legendary beasts") },
    { COMPOUND_STRING("LAKES"),   COMPOUND_STRING("the LAKE GUARDIANS") },
    { COMPOUND_STRING("SWORDS"),  COMPOUND_STRING("the SWORDS OF JUSTICE") },
    { COMPOUND_STRING("FORCES"),  COMPOUND_STRING("the FORCES OF NATURE") },
    { COMPOUND_STRING("EON"),     COMPOUND_STRING("the EON DUO") },
    { COMPOUND_STRING("TOWERS"),  COMPOUND_STRING("the TOWER DUO") },
    { COMPOUND_STRING("WEATHER"), COMPOUND_STRING("the WEATHER TRIO") },
    { COMPOUND_STRING("LONE"),    COMPOUND_STRING("Lone legends") },
};

STATIC_ASSERT(RLEG_COUNT - 1 == ROGUE_NUM_LEGENDS, RogueLegendCount);

// ---------------------------------------------------------------------------
// One catch per area

void Rogue_OnWildBattleStart(void)
{
    s32 area = Rogue_GetAreaId();
    u16 species = GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES);
    bool32 shiny;

    // RUN 0: now and then the wild one is a DRAFT (before the shiny check:
    // its wrong colors are not a shiny met).
    Rogue_Run0OnWildBattleStart();
    shiny = IsMonShiny(&gParties[B_TRAINER_OPPONENT_A][0]);

    gRogueCatchState = CATCH_FREE;
    // A DRAFT cannot be caught, and does not use up the area.
    if (area < 0 || Rogue_Run0CatchBlockedText() != NULL)
        return;
    if (shiny)
        Rogue_RecordShinySeen();
    if (Rogue_IsAreaCatchUsed(area))
    {
        gRogueCatchState = CATCH_USED;
    }
    else if (GetSetPokedexFlag(SpeciesToNationalPokedexNum(species), FLAG_GET_CAUGHT))
    {
        // Dupes clause: a species the player already owns does not use up
        // the area.
        gRogueCatchState = CATCH_DUPE;
    }
    if (gRogueCatchState != CATCH_FREE)
    {
        // Shiny clause: a shiny can be caught anyway, and does not use up
        // the area either.
        if (shiny && Rogue_IsUnlockOn(UNLOCK_SHINY_CLAUSE))
            gRogueCatchState = CATCH_SHINY;
    }
    else
    {
        gRogueCatchState = CATCH_FIRST;
        Rogue_SetAreaCatchUsed(area);
    }
}

const u8 *Rogue_GetCatchBlockedText(void)
{
    if (gBattleTypeFlags & BATTLE_TYPE_TRAINER)
        return NULL;
    // RUN 0: a DRAFT is not finished.
    if (Rogue_Run0CatchBlockedText() != NULL)
        return Rogue_Run0CatchBlockedText();
    if (gRogueCatchState == CATCH_USED)
        return sText_CatchUsed;
    if (gRogueCatchState == CATCH_DUPE)
        return sText_CatchDupe;
    return NULL;
}

bool32 Rogue_IsAreaUsed(void)
{
    return gRogueCatchState == CATCH_USED && !(gBattleTypeFlags & BATTLE_TYPE_TRAINER);
}

bool32 Rogue_IsShinyClauseCatch(void)
{
    return gRogueCatchState == CATCH_SHINY && !(gBattleTypeFlags & BATTLE_TYPE_TRAINER);
}

// A species the player already has, met in an area whose catch is still
// open (dupes clause): it can't be caught and leaves the area open.
bool32 Rogue_IsDupeEncounter(void)
{
    return gRogueCatchState == CATCH_DUPE && !(gBattleTypeFlags & BATTLE_TYPE_TRAINER);
}

bool32 Rogue_IsFirstEncounter(void)
{
    return gRogueCatchState == CATCH_FIRST && !(gBattleTypeFlags & BATTLE_TYPE_TRAINER);
}

// ---------------------------------------------------------------------------
// Fainted Pokemon are gone

// Unlocks earned but not yet announced (Rogue_CheckUnlocks).
static EWRAM_DATA u64 sPendingUnlocks = 0;
// Feats done but not yet told, and a legend just caught for the LEGEND LOG
// (enum RogueLegend).
static EWRAM_DATA u16 sPendingFeats = 0;
static EWRAM_DATA u8 sPendingLegend = 0;
extern const u8 Rogue_EventScript_FieldUnlocks[];

// A dungeon's boss trainer is beaten: the dungeon is cleared, and an unlock
// it earns is announced once the field is free.
static void RecordBossTrainer(u16 trainerId)
{
    s32 dungeon = Rogue_DungeonOfBossTrainer(trainerId);
    u8 t;
    // The villains' admins have slots of their own, outside the hideouts'.
    for (t = 0; t < ROGUE_MAX_TEAMS; t++)
        if (trainerId == ROGUE_TRAINER(RogueGen_TeamAdminSlot(t)))
            dungeon = RogueGen_TeamHideout(Rogue_GetRegion(), t);
    if (dungeon < 0)
        return;
    Rogue_RecordDungeonCleared(dungeon);
    sPendingUnlocks |= Rogue_UpdateRecord();
}

// ROADSIDE CAMP is on and was told at the start of this run (it is earned at
// the 5th badge, so the run that earned it plays on without it).
bool32 Rogue_IsLateCampOn(void)
{
    return Rogue_IsUnlockOn(UNLOCK_LATE_CAMP) && (Rogue_GetRecord()->announced & (1u << UNLOCK_LATE_CAMP));
}

// Script special, after a badge: VAR_RESULT TRUE if TRAINER'S CANDY is on
// and this was the 4th, 5th or 6th badge. Only from the run after it was
// earned (the professor has told it), so a first run that wins its 4th
// badge plays on as it would have.
void Rogue_CheckTrainerCandy(void)
{
    u32 badges = Rogue_BadgeCount();
    gSpecialVar_Result = Rogue_IsUnlockOn(UNLOCK_TRAINER_CANDY)
                      && (Rogue_GetRecord()->announced & (1u << UNLOCK_TRAINER_CANDY))
                      && badges >= 4 && badges <= 6;
}

void Rogue_OnBattleEnd(void)
{
    u32 i, fallen = 0;
    bool32 first = Rogue_IsFirstEncounter();

    gRogueCatchState = CATCH_FREE;
    // SECOND CHANCE: from the 4th badge, an area's first Pokemon that got
    // away (fled, fainted, or the player ran) leaves the area open, a few
    // times a run.
    if (first && gBattleOutcome != B_OUTCOME_CAUGHT && gBattleOutcome != B_OUTCOME_LOST
     && Rogue_IsUnlockOn(UNLOCK_SECOND_CHANCE) && Rogue_BadgeCount() >= ROGUE_SECOND_CHANCE_BADGES
     && VarGet(VAR_ROGUE_SECOND_CHANCES) < ROGUE_SECOND_CHANCES && Rogue_GetAreaId() >= 0)
    {
        Rogue_ClearAreaCatchUsed(Rogue_GetAreaId());
        VarSet(VAR_ROGUE_SECOND_CHANCES, VarGet(VAR_ROGUE_SECOND_CHANCES) + 1);
    }
    if (Rogue_GetCurrentPlace() < 0)
        return;
    if (gBattleOutcome == B_OUTCOME_WON && (gBattleTypeFlags & BATTLE_TYPE_TRAINER))
    {
        RecordBossTrainer(TRAINER_BATTLE_PARAM.opponentA);
        // Whichever battle beats a hideout's boss or a team's leader ends
        // it, a battle of two trainers too.
        Rogue_VillainBattleWon(TRAINER_BATTLE_PARAM.opponentA);
        if (gBattleTypeFlags & BATTLE_TYPE_TWO_OPPONENTS)
        {
            RecordBossTrainer(TRAINER_BATTLE_PARAM.opponentB);
            Rogue_VillainBattleWon(TRAINER_BATTLE_PARAM.opponentB);
        }
    }
    if (gBattleOutcome == B_OUTCOME_CAUGHT)
    {
        s32 area = Rogue_GetAreaId();
        Rogue_RecordCatch();
        // A dungeon boss (a legendary-style battle) leaves the area open, so
        // the map does not show it as caught there either.
        if (area >= 0 && !(gBattleTypeFlags & BATTLE_TYPE_LEGENDARY))
            Rogue_SetAreaCaught(area);
    }
    // After a lost battle the run is over. Leave the party as it fell.
    if (gBattleOutcome == B_OUTCOME_LOST || gBattleOutcome == B_OUTCOME_DREW)
        return;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        u16 item;
        if (!GetMonData(mon, MON_DATA_SANITY_HAS_SPECIES) || GetMonData(mon, MON_DATA_IS_EGG))
            continue;
        if (GetMonData(mon, MON_DATA_HP) != 0)
            continue;
        item = GetMonData(mon, MON_DATA_HELD_ITEM);
        if (item != ITEM_NONE)
            AddBagItem(item, 1);
        GetMonData(mon, MON_DATA_NICKNAME, sFallenNames[fallen]);
        // The run log keeps it; in RUN 0 the Director keeps it.
        Rogue_Run0OnFainted(mon);
        ZeroMonData(mon);
        fallen++;
    }
    if (fallen != 0)
    {
        CompactPartySlots();
        CalculatePlayerPartyCount();
        VarSet(VAR_ROGUE_FALLEN, VarGet(VAR_ROGUE_FALLEN) + fallen);
        sFallenCount = fallen;
    }
}

// Called every overworld frame. Once the field is free after a battle in
// which Pokemon fainted, says goodbye to them, so the rule is felt and not
// just noticed later in the party screen.
void Rogue_CheckFallenMessage(void)
{
    if (ScriptContext_IsEnabled() || ArePlayerFieldControlsLocked())
        return;
    if (sFallenCount == 0)
    {
        if (sPendingUnlocks != 0 || sPendingFeats != 0 || sPendingLegend != RLEG_NONE)
            ScriptContext_SetupScript(Rogue_EventScript_FieldUnlocks);
        return;
    }
    BufferFallen();
    ScriptContext_SetupScript(Rogue_EventScript_Fallen);
}

// RUN 0, a leader's script after the badge (Rogue_EventScript_Run0PatchNotes):
// the Director's line on the ones that fainted in the leader's battle comes
// first, before the PATCH and before one it kept is given back (it said
// "Take JOAN." then "JOAN fainted. I'll keep it."). VAR_RESULT TRUE and the
// line in gStringVar1, or FALSE.
void Rogue_Run0FallenNow(void)
{
    gSpecialVar_Result = FALSE;
    if (!Rogue_IsRun0() || sFallenCount == 0)
        return;
    BufferFallen();
    gSpecialVar_Result = TRUE;
}

// The farewell to the Pokemon that fainted in the last battle, into
// gStringVar1; the count is cleared.
static void BufferFallen(void)
{
    u32 i;
    u8 *str = gStringVar1;
    *str = EOS;
    for (i = 0; i < sFallenCount; i++)
    {
        if (i > 0)
            str = StringAppend(gStringVar1, sText_And);
        str = StringAppend(gStringVar1, sFallenNames[i]);
    }
    if (Rogue_IsRun0())
        StringAppend(gStringVar1, sFallenCount > 1 ? sText_Run0FallenMany : sText_Run0FallenOne);
    else
        StringAppend(gStringVar1, sFallenCount > 1 ? sText_FallenMany : sText_FallenOne);
    sFallenCount = 0;
}

// ---------------------------------------------------------------------------
// The permanent record

bool32 Rogue_IsRunOver(void)
{
    return VarGet(VAR_ROGUE_RUN_STATE) != ROGUE_RUN_ACTIVE;
}

// The unlock bit sets are 64 bits: the first 32 in unlocks, disabled,
// announced and runStart, the rest in the v0.13 fields after them.
#define UNLOCK_BIT(i) (1ull << (i))
#define RECORD_BITS(r, f) ((u64)(r)->f | ((u64)(r)->f##Hi << 32))
#define SET_RECORD_BITS(r, f, v) do { u64 v_ = (v); (r)->f = (u32)v_; (r)->f##Hi = (u32)(v_ >> 32); } while (0)

static u64 LadderUnlocks(void);

static void SetCaughtBit(u8 *bits, u16 species)
{
    u32 dex = SpeciesToNationalPokedexNum(Rogue_FirstForm(species));
    if (dex != 0 && dex < ROGUE_DEX_BYTES * 8)
        bits[dex / 8] |= 1 << (dex % 8);
}

struct RogueRecord *Rogue_GetRecord(void)
{
    struct RogueRecord *record = (struct RogueRecord *)gSaveBlock2Ptr->hallRecords1P;
    if (record->magic != ROGUE_RECORD_MAGIC)
    {
        memset(record, 0, sizeof(*record));
        record->magic = ROGUE_RECORD_MAGIC;
        record->fallenName[0] = EOS;
        // The four unlocks of earlier versions were kept in a var.
        record->unlocks = VarGet(VAR_ROGUE_META_UNLOCKS);
        record->announced = record->unlocks;
        record->runStart = record->unlocks;
    }
    if (record->extMagic != ROGUE_RECORD_EXT_MAGIC)
    {
        // A record from before v0.10: what it lacks starts empty, but for
        // the feats its wins already did, filled in quietly.
        record->extMagic = ROGUE_RECORD_EXT_MAGIC;
        record->legends = 0;
        record->legendsSeen = 0;
        record->feats = 0;
        if (VarGet(VAR_ROGUE_META_WINS) != 0)
            record->feats |= 1u << FEAT_CHAMPION;
        if (record->hardcoreWins != 0)
            record->feats |= 1u << FEAT_HARDCORE;
        record->featsStart = record->feats;
        record->runLegends = 0;
    }
    if (record->ext2Magic != ROGUE_RECORD_EXT2_MAGIC)
    {
        // A record from before v0.13 (the bytes here were hall records).
        // The type badges carry over, and this run's catches start the
        // caught record. The starter rows its runs and wins have earned
        // count as earned before this run, so the run plays on quietly and
        // the professor tells them at the start of the next one.
        u32 i;
        memset(&record->ext2Magic, 0, sizeof(*record) - offsetof(struct RogueRecord, ext2Magic));
        record->ext2Magic = ROGUE_RECORD_EXT2_MAGIC;
        record->typeBadgesAll[TYPE_GRASS] = record->typeBadges[ROGUE_STARTER_GRASS];
        record->typeBadgesAll[TYPE_FIRE] = record->typeBadges[ROGUE_STARTER_FIRE];
        record->typeBadgesAll[TYPE_WATER] = record->typeBadges[ROGUE_STARTER_WATER];
        for (i = 1; i <= NATIONAL_DEX_COUNT; i++)
            if (GetSetPokedexFlag(i, FLAG_GET_CAUGHT))
                SetCaughtBit(record->caught, NationalPokedexNumToSpecies(i));
        SET_RECORD_BITS(record, disabled, RECORD_BITS(record, disabled)
                        | (LadderUnlocks() & ~RECORD_BITS(record, unlocks) & (UNLOCK_BIT(UNLOCK_NATIONAL_DEX) | UNLOCK_BIT(UNLOCK_GLOBAL_DEX))));
        SET_RECORD_BITS(record, unlocks, RECORD_BITS(record, unlocks) | LadderUnlocks());
        SET_RECORD_BITS(record, runStart, RECORD_BITS(record, runStart) | LadderUnlocks());
    }
    // The POKeDEX upgrades follow the wins as soon as they are counted (a
    // record from before v0.14 shows them in UNLOCKS at once, as earned
    // before the run). They are earned switched off: CLASSIC stays the
    // choice until the player picks another. A win's shows on its summary,
    // where the professor hands it out.
    {
        u64 dex = LadderUnlocks() & (UNLOCK_BIT(UNLOCK_NATIONAL_DEX) | UNLOCK_BIT(UNLOCK_GLOBAL_DEX)) & ~RECORD_BITS(record, unlocks);
        if (dex != 0)
        {
            SET_RECORD_BITS(record, disabled, RECORD_BITS(record, disabled) | dex);
            SET_RECORD_BITS(record, unlocks, RECORD_BITS(record, unlocks) | dex);
            if (VarGet(VAR_ROGUE_RUN_STATE) != ROGUE_RUN_WON)
                SET_RECORD_BITS(record, runStart, RECORD_BITS(record, runStart) | dex);
        }
    }
    return record;
}

STATIC_ASSERT(sizeof(struct RogueRecord) <= sizeof(((struct SaveBlock2 *)0)->hallRecords1P), RogueRecordFits);
STATIC_ASSERT(offsetof(struct RogueRecord, dungeons) == 52, RogueRecordDungeonsAt52); // tools/roguemon/playtest.py read_record
STATIC_ASSERT(offsetof(struct RogueRecord, legends) == 56, RogueRecordLegendsAt56);
STATIC_ASSERT(offsetof(struct RogueRecord, feats) == 64, RogueRecordFeatsAt64);
STATIC_ASSERT(offsetof(struct RogueRecord, ext2Magic) == 72, RogueRecordExt2At72);
STATIC_ASSERT(offsetof(struct RogueRecord, unlocksHi) == 76, RogueRecordUnlocksHiAt76);
STATIC_ASSERT(offsetof(struct RogueRecord, typeBadgesAll) == 92, RogueRecordTypeBadgesAt92);
STATIC_ASSERT(offsetof(struct RogueRecord, caught) == 116, RogueRecordCaughtAt116);
STATIC_ASSERT(offsetof(struct RogueRecord, caughtShiny) == 246, RogueRecordCaughtShinyAt246);
STATIC_ASSERT(sizeof(struct RogueRecord) == 376, RogueRecordSize);
STATIC_ASSERT(NUMBER_OF_MON_TYPES <= sizeof(((struct RogueRecord *)0)->typeBadgesAll), RogueRecordTypes);
STATIC_ASSERT(NATIONAL_DEX_COUNT < ROGUE_DEX_BYTES * 8, RogueRecordDex);
STATIC_ASSERT(ROGUE_NUM_UNLOCKS <= 64, RogueUnlockBits);

u64 Rogue_GetUnlocks(void)
{
    struct RogueRecord *record = Rogue_GetRecord();
    return RECORD_BITS(record, unlocks) & ~RECORD_BITS(record, disabled);
}

bool32 Rogue_IsUnlockOn(u32 unlock)
{
    return (Rogue_GetUnlocks() >> unlock) & 1;
}

bool32 Rogue_IsUnlockEarned(u32 unlock)
{
    struct RogueRecord *record = Rogue_GetRecord();
    return (RECORD_BITS(record, unlocks) >> unlock) & 1;
}

// Counted as the UNLOCKS rows: the two POKeDEX upgrades are one row.
u32 Rogue_CountEarnedUnlocks(void)
{
    u32 i, count = 0;
    for (i = 0; i < ROGUE_NUM_UNLOCKS; i++)
        count += i != UNLOCK_GLOBAL_DEX && Rogue_IsUnlockEarned(i);
    return count;
}

bool32 Rogue_IsSummaryUnlock(u32 unlock)
{
    struct RogueRecord *record = Rogue_GetRecord();
    return ((RECORD_BITS(record, unlocks) & ~RECORD_BITS(record, runStart)) >> unlock) & 1;
}

void Rogue_SetUnlockEnabled(u32 unlock, bool32 enabled)
{
    struct RogueRecord *record = Rogue_GetRecord();
    u64 disabled = RECORD_BITS(record, disabled);
    if (enabled)
        disabled &= ~UNLOCK_BIT(unlock);
    else
        disabled |= UNLOCK_BIT(unlock);
    SET_RECORD_BITS(record, disabled, disabled);
}

const u8 *Rogue_GetUnlockName(u32 index)
{
    return sUnlocks[index % ROGUE_NUM_UNLOCKS].name;
}

const u8 *Rogue_GetUnlockHint(u32 index)
{
    return sUnlocks[index % ROGUE_NUM_UNLOCKS].hint;
}

const u8 *Rogue_GetUnlockDescription(u32 index)
{
    return sUnlocks[index % ROGUE_NUM_UNLOCKS].description;
}

static const u8 sTypeUpgradeBadges[] =
{
    [UNLOCK_TYPE_CHARM] = ROGUE_TYPE_CHARM_BADGES,
    [UNLOCK_HIDDEN_ABILITY] = ROGUE_HIDDEN_ABILITY_BADGES,
    [UNLOCK_EGG_MOVE] = ROGUE_EGG_MOVE_BADGES,
};

u32 Rogue_GetTypeBadges(u32 type)
{
    if (type == TYPE_NONE || type >= NUMBER_OF_MON_TYPES)
        return 0;
    return Rogue_GetRecord()->typeBadgesAll[type];
}

bool32 Rogue_HasTypeUpgrade(u32 unlock, u32 type)
{
    if (unlock >= ARRAY_COUNT(sTypeUpgradeBadges) || sTypeUpgradeBadges[unlock] == 0)
        return FALSE;
    return Rogue_IsUnlockOn(unlock) && Rogue_GetTypeBadges(type) >= sTypeUpgradeBadges[unlock];
}

static bool32 AnyTypeBadges(const struct RogueRecord *record, u32 badges)
{
    u32 type;
    for (type = 0; type < NUMBER_OF_MON_TYPES; type++)
        if (record->typeBadgesAll[type] >= badges)
            return TRUE;
    return FALSE;
}

// ---------------------------------------------------------------------------
// The LEGEND LOG and feats

u32 Rogue_CountLegends(u32 bits)
{
    u32 n = 0;
    for (bits &= ~1u; bits != 0; bits &= bits - 1)
        n++;
    return n;
}

static u32 LegendRowBits(u32 row)
{
    u32 i, bits = 0;
    for (i = 0; i < 4 && gRogueLegendRows[row][i] != RLEG_NONE; i++)
        bits |= 1u << gRogueLegendRows[row][i];
    return bits;
}

// The set row a legend belongs to, or -1 for a lone one.
static s32 LegendSetOf(u32 legend)
{
    u32 row;
    for (row = 0; row < ROGUE_LEGEND_SETS; row++)
        if (LegendRowBits(row) & (1u << legend))
            return row;
    return -1;
}

const u8 *Rogue_GetLegendRowName(u32 row, bool32 full)
{
    return sLegendRowNames[row % ROGUE_LEGEND_ROWS][full ? 1 : 0];
}

// The feats the record shows by itself. The ones about a win are set by
// Rogue_GameClear.
static void UpdateFeats(struct RogueRecord *record)
{
    u32 feats = record->feats, n = Rogue_CountLegends(record->legends), row;

    if (VarGet(VAR_ROGUE_META_WINS) != 0)
        feats |= 1u << FEAT_CHAMPION;
    if (record->hardcoreWins != 0)
        feats |= 1u << FEAT_HARDCORE;
    if (n >= 1)
        feats |= 1u << FEAT_LEGEND;
    if (n >= ROGUE_MARKING_MAP_LEGENDS)
        feats |= 1u << FEAT_LEGENDS_3;
    if (n >= ROGUE_MASTER_BALL_LEGENDS)
        feats |= 1u << FEAT_LEGENDS_10;
    if (n >= ROGUE_NUM_LEGENDS)
        feats |= 1u << FEAT_ALL_LEGENDS;
    for (row = 0; row < ROGUE_LEGEND_SETS; row++)
        if ((record->legends & LegendRowBits(row)) == LegendRowBits(row))
            feats |= 1u << FEAT_LEGEND_SET;
    sPendingFeats |= feats & ~record->feats;
    record->feats = feats;
}

bool32 Rogue_IsFeatDone(u32 feat)
{
    return (Rogue_GetRecord()->feats >> feat) & 1;
}

const u8 *Rogue_GetFeatName(u32 feat)
{
    return sFeats[feat % ROGUE_NUM_FEATS].name;
}

const u8 *Rogue_GetFeatHint(u32 feat)
{
    return sFeats[feat % ROGUE_NUM_FEATS].hint;
}

const u8 *Rogue_GetFeatReward(u32 feat)
{
    return sFeats[feat % ROGUE_NUM_FEATS].reward;
}

// How far along a feat that counts something is. FALSE for the others.
bool32 Rogue_GetFeatProgress(u32 feat, u32 *have, u32 *need)
{
    const struct RogueRecord *record = Rogue_GetRecord();
    u32 row, best = 0;

    *have = Rogue_CountLegends(record->legends);
    switch (feat)
    {
    case FEAT_LEGENDS_3:
        *need = ROGUE_MARKING_MAP_LEGENDS;
        return TRUE;
    case FEAT_LEGENDS_10:
        *need = ROGUE_MASTER_BALL_LEGENDS;
        return TRUE;
    case FEAT_ALL_LEGENDS:
        *need = ROGUE_NUM_LEGENDS;
        return TRUE;
    case FEAT_LEGEND_SET:
        // The set nearest done.
        *have = 0, *need = 3;
        for (row = 0; row < ROGUE_LEGEND_SETS; row++)
        {
            u32 got = Rogue_CountLegends(record->legends & LegendRowBits(row)), size = Rogue_CountLegends(LegendRowBits(row));
            if (got * 256 / size > best)
            {
                best = got * 256 / size;
                *have = got, *need = size;
            }
        }
        return TRUE;
    }
    return FALSE;
}

void Rogue_RecordLegendSeen(u32 legend)
{
    if (legend != RLEG_NONE && legend < RLEG_COUNT)
        Rogue_GetRecord()->legendsSeen |= 1u << legend;
}

void Rogue_RecordLegendCaught(u32 legend)
{
    struct RogueRecord *record = Rogue_GetRecord();
    // RUN 0 earns none of a normal run's unlocks (RecordsCount).
    if (Rogue_IsRun0())
        return;
    if (legend == RLEG_NONE || legend >= RLEG_COUNT)
        return;
    record->legendsSeen |= 1u << legend;
    if (record->legends & (1u << legend))
        return;
    record->legends |= 1u << legend;
    sPendingLegend = legend;
}

// The starter rows earned by runs started and wins.
static u64 LadderUnlocks(void)
{
    u32 runs = VarGet(VAR_ROGUE_META_RUNS);
    u64 unlocks = 0;
    if (runs >= ROGUE_EEVEE_RUNS)
        unlocks |= UNLOCK_BIT(UNLOCK_EEVEE);
    if (runs >= ROGUE_BABY_RUNS)
        unlocks |= UNLOCK_BIT(UNLOCK_BABIES);
    if (runs >= ROGUE_RISING_RUNS)
        unlocks |= UNLOCK_BIT(UNLOCK_RISING_STARS);
    if (runs >= ROGUE_STARTER_CASE_RUNS)
        unlocks |= UNLOCK_BIT(UNLOCK_STARTER_CASE);
    if (runs >= ROGUE_FAR_AWAY_RUNS)
        unlocks |= UNLOCK_BIT(UNLOCK_FAR_AWAY);
    if (runs >= ROGUE_FREE_CHOICE_RUNS)
        unlocks |= UNLOCK_BIT(UNLOCK_FREE_CHOICE);
    if (runs >= ROGUE_POKEDEX_PICK_RUNS)
        unlocks |= UNLOCK_BIT(UNLOCK_POKEDEX_PICK);
    if (VarGet(VAR_ROGUE_META_WINS) >= ROGUE_ANY_POKEMON_WINS)
        unlocks |= UNLOCK_BIT(UNLOCK_ANY_POKEMON);
    if (VarGet(VAR_ROGUE_META_WINS) >= ROGUE_NATIONAL_DEX_WINS)
        unlocks |= UNLOCK_BIT(UNLOCK_NATIONAL_DEX);
    if (VarGet(VAR_ROGUE_META_WINS) >= ROGUE_GLOBAL_DEX_WINS)
        unlocks |= UNLOCK_BIT(UNLOCK_GLOBAL_DEX);
    return unlocks;
}

u64 Rogue_UpdateRecord(void)
{
    struct RogueRecord *record = Rogue_GetRecord();
    u32 progress = VarGet(VAR_ROGUE_PROGRESS);
    u64 old = RECORD_BITS(record, unlocks);
    u64 unlocks = old | LadderUnlocks();
    u32 best;

    // RUN 0's badges, catches and losses earn none of a normal run's
    // unlocks (its own reward is its feat): its progress is not the best.
    if (Rogue_IsRun0())
    {
        UpdateFeats(record);
        return 0;
    }

    if (progress > VarGet(VAR_ROGUE_META_BEST))
        VarSet(VAR_ROGUE_META_BEST, progress);
    best = VarGet(VAR_ROGUE_META_BEST);
    if (best >= ROGUE_PROGRESS_BADGE_1)
        unlocks |= 1u << UNLOCK_PARTNERS;
    if (best >= ROGUE_PROGRESS_BADGE_3)
        unlocks |= 1u << UNLOCK_DRAGONS;
    if (best >= ROGUE_PROGRESS_CHAMPION)
        unlocks |= (1u << UNLOCK_KIT) | (1u << UNLOCK_CHAMP_PARTNER) | (1u << UNLOCK_HARDCORE);
    if (VarGet(VAR_ROGUE_META_RUNS) >= 3)
        unlocks |= 1u << UNLOCK_VETERAN;
    if (record->losses >= 2)
        unlocks |= 1u << UNLOCK_BERRIES;
    if (record->losses >= 3)
        unlocks |= 1u << UNLOCK_SPARE_STARTERS;
    if (record->losses >= 4)
        unlocks |= 1u << UNLOCK_LUCKY_EGG;
    if (record->losses >= 5)
        unlocks |= 1u << UNLOCK_SECOND_CHANCE;
    if (best >= ROGUE_PROGRESS_BADGE_1 + 3)
        unlocks |= 1u << UNLOCK_TRAINER_CANDY;
    if (best >= ROGUE_PROGRESS_BADGE_1 + 4)
        unlocks |= 1u << UNLOCK_LATE_CAMP;
    if (record->fossils >= 1)
        unlocks |= 1u << UNLOCK_FOSSIL_STARTERS;
    if (record->fallenSpecies != SPECIES_NONE)
        unlocks |= 1u << UNLOCK_FALLEN_FRIEND;
    if (AnyTypeBadges(record, ROGUE_TYPE_CHARM_BADGES))
        unlocks |= 1u << UNLOCK_TYPE_CHARM;
    if (AnyTypeBadges(record, ROGUE_HIDDEN_ABILITY_BADGES))
        unlocks |= 1u << UNLOCK_HIDDEN_ABILITY;
    if (AnyTypeBadges(record, ROGUE_EGG_MOVE_BADGES))
        unlocks |= 1u << UNLOCK_EGG_MOVE;
    if (record->shinies >= 1)
        unlocks |= 1u << UNLOCK_SHINY_CLAUSE;
    if (record->shinies >= 3)
        unlocks |= 1u << UNLOCK_SHINY_CHARM;
    if (record->caves >= 5)
        unlocks |= 1u << UNLOCK_FOSSILS;
    if (record->catches >= 50)
        unlocks |= 1u << UNLOCK_RARE_FINDS;
    if (record->rivalWins >= 10)
        unlocks |= 1u << UNLOCK_RIVAL_GIFT;
    if (record->dungeons >= ROGUE_EXPLORER_DUNGEONS)
        unlocks |= 1u << UNLOCK_EXPLORER;
    UpdateFeats(record);
    if (Rogue_CountLegends(record->legends) >= ROGUE_MARKING_MAP_LEGENDS)
        unlocks |= 1u << UNLOCK_MARKING_MAP;
    if (Rogue_CountLegends(record->legends) >= ROGUE_MASTER_BALL_LEGENDS)
        unlocks |= 1u << UNLOCK_MASTER_BALL;
    if (record->feats & (1u << FEAT_ALL_DUNGEONS))
        unlocks |= 1u << UNLOCK_DOWSING;
    // Hardcore rules are a challenge: earned switched off. (So are the
    // POKeDEX upgrades, in Rogue_GetRecord.)
    if ((unlocks & ~old) & (1u << UNLOCK_HARDCORE))
        record->disabled |= 1u << UNLOCK_HARDCORE;
    SET_RECORD_BITS(record, unlocks, unlocks);
    VarSet(VAR_ROGUE_META_UNLOCKS, unlocks & 0xFFFF);
    return unlocks & ~old;
}

void Rogue_StashRecord(struct RogueRecordStash *stash)
{
    // Nothing in a new game clears the hall records, so if the heap is
    // short the record simply stays where it is.
    stash->record = Alloc(sizeof(struct RogueRecord));
    if (stash->record != NULL)
        *stash->record = *Rogue_GetRecord();
    stash->vars[0] = VarGet(VAR_ROGUE_META_UNLOCKS);
    stash->vars[1] = VarGet(VAR_ROGUE_META_RUNS);
    stash->vars[2] = VarGet(VAR_ROGUE_META_WINS);
    stash->vars[3] = VarGet(VAR_ROGUE_META_BEST);
    stash->vars[4] = VarGet(VAR_ROGUE_META_CHAMPION_MON);
    // The kind of the run to come, set by the menu (RUN 0, ENDLESS).
    stash->vars[5] = VarGet(VAR_ROGUE_RUN_KIND);
    // The run being left, in the Director's run log.
    Rogue_RunLogLeave();
}

// Called once the new game's event data is cleared. Counts the new run.
void Rogue_RestoreRecord(const struct RogueRecordStash *stash)
{
    struct RogueRecord *record = (struct RogueRecord *)gSaveBlock2Ptr->hallRecords1P;

    if (stash->record != NULL)
    {
        *record = *stash->record;
        Free(stash->record);
    }
    VarSet(VAR_ROGUE_META_UNLOCKS, stash->vars[0]);
    VarSet(VAR_ROGUE_META_RUNS, stash->vars[1] + 1);
    VarSet(VAR_ROGUE_META_WINS, stash->vars[2]);
    VarSet(VAR_ROGUE_META_BEST, stash->vars[3]);
    VarSet(VAR_ROGUE_META_CHAMPION_MON, stash->vars[4]);
    VarSet(VAR_ROGUE_RUN_KIND, stash->vars[5]);
    Rogue_UpdateRecord();
    Rogue_GetRecord()->runStart = Rogue_GetRecord()->unlocks;
    Rogue_GetRecord()->runStartHi = Rogue_GetRecord()->unlocksHi;
    // Feats an older record already had are filled in quietly.
    Rogue_GetRecord()->featsStart = Rogue_GetRecord()->feats;
    sPendingFeats = 0;
    sPendingLegend = RLEG_NONE;
}

// Milestones toward unlocks.

static void CountUp16(u16 *counter)
{
    if (*counter < 0xFFFF)
        (*counter)++;
}

static void CountUp8(u8 *counter)
{
    if (*counter < 0xFF)
        (*counter)++;
}

// RUN 0 counts nothing toward a normal run's unlocks (the record's caves,
// catches, shinies, rival wins, fossils, dungeons, legends, type badges and
// losses), so none is earned there or by the next normal run because of it.
static bool32 RecordsCount(void)
{
    return !Rogue_IsRun0();
}

void Rogue_RecordCaveVisit(u32 place)
{
    const struct RogueRegion *region = Rogue_GetRegion();
    u32 i, cave = 0;

    if (!RecordsCount())
        return;
    if (place >= region->nPlaces || region->places[place].kind != RPLACE_CAVE)
        return;
    // Caves are counted once per run, by their order in the region.
    for (i = 0; i < place; i++)
        if (region->places[i].kind == RPLACE_CAVE)
            cave++;
    if (cave >= 16 || FlagGet(FLAG_ROGUE_CAVE_0 + cave))
        return;
    FlagSet(FLAG_ROGUE_CAVE_0 + cave);
    CountUp16(&Rogue_GetRecord()->caves);
}

void Rogue_RecordCatch(void)
{
    struct RogueRecord *record = Rogue_GetRecord();
    struct Pokemon *mon = &gParties[B_TRAINER_OPPONENT_A][0];
    u16 species = GetMonData(mon, MON_DATA_SPECIES);

    if (!RecordsCount())
        return;
    CountUp16(&record->catches);
    // For POKEDEX PICK: its first form, and shiny if it was.
    SetCaughtBit(record->caught, species);
    if (IsMonShiny(mon))
        SetCaughtBit(record->caughtShiny, species);
}

bool32 Rogue_WasCaught(u16 species, bool32 *shiny)
{
    const struct RogueRecord *record = Rogue_GetRecord();
    u32 dex = SpeciesToNationalPokedexNum(species);

    *shiny = FALSE;
    if (dex == 0 || dex >= ROGUE_DEX_BYTES * 8)
        return FALSE;
    *shiny = (record->caughtShiny[dex / 8] >> (dex % 8)) & 1;
    return (record->caught[dex / 8] >> (dex % 8)) & 1;
}

void Rogue_RecordShinySeen(void)
{
    if (RecordsCount())
        CountUp8(&Rogue_GetRecord()->shinies);
}

void Rogue_RecordRivalWin(void)
{
    if (RecordsCount())
        CountUp8(&Rogue_GetRecord()->rivalWins);
}

void Rogue_RecordFossilRevived(void)
{
    if (RecordsCount())
        CountUp8(&Rogue_GetRecord()->fossils);
}

void Rogue_RecordDungeonCleared(u32 dungeon)
{
    dungeon %= ROGUE_MAX_DUNGEONS;
    if (FlagGet(FLAG_ROGUE_BOSS_0 + dungeon))
        return;
    FlagSet(FLAG_ROGUE_BOSS_0 + dungeon);
    if (RecordsCount())
        CountUp16(&Rogue_GetRecord()->dungeons);
}

// Counted by the starter's first type, whatever row it came from.
void Rogue_RecordBadge(void)
{
    struct RogueRecord *record = Rogue_GetRecord();
    u32 type = Rogue_GetRunStarterFirstType();
    u32 progress = VarGet(VAR_ROGUE_PROGRESS);
    u32 badges = progress < ROGUE_PROGRESS_BADGE_1 ? 0 : progress - ROGUE_PROGRESS_BADGE_1 + 1;

    // RUN 0: two gyms an act.
    if (Rogue_IsRun0())
    {
        u32 act = 1 + badges / 2;
        if (act > ROGUE_RUN0_ACTS)
            act = ROGUE_RUN0_ACTS;
        if (act > VarGet(VAR_ROGUE_RUN0_ACT))
            VarSet(VAR_ROGUE_RUN0_ACT, act);
        // The Director patches the game (src/rogue_run0_challenge.c).
        Rogue_Run0PatchAfterBadge();
        // Its badges are not a normal run's (no type badges, no unlocks).
        return;
    }
    if (type == TYPE_NONE || type >= NUMBER_OF_MON_TYPES || !Rogue_PartyHasStarter())
        return;
    if (badges > record->typeBadgesAll[type])
        record->typeBadgesAll[type] = badges;
    // The fields of earlier versions, which the tools still read.
    if (type == TYPE_GRASS && badges > record->typeBadges[ROGUE_STARTER_GRASS])
        record->typeBadges[ROGUE_STARTER_GRASS] = badges;
    if (type == TYPE_FIRE && badges > record->typeBadges[ROGUE_STARTER_FIRE])
        record->typeBadges[ROGUE_STARTER_FIRE] = badges;
    if (type == TYPE_WATER && badges > record->typeBadges[ROGUE_STARTER_WATER])
        record->typeBadges[ROGUE_STARTER_WATER] = badges;
}

// ---------------------------------------------------------------------------
// Ending a run

// The fallen friend: of the team that went down, the one that had been
// along longest, which is the one met at the lowest level.
static void RecordFallenFriend(void)
{
    struct RogueRecord *record = Rogue_GetRecord();
    u32 i, best = PARTY_SIZE, bestLevel = 0xFF;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        u32 metLevel;
        if (!GetMonData(mon, MON_DATA_SANITY_HAS_SPECIES) || GetMonData(mon, MON_DATA_IS_EGG))
            continue;
        metLevel = GetMonData(mon, MON_DATA_MET_LEVEL);
        if (metLevel < bestLevel)
        {
            best = i;
            bestLevel = metLevel;
        }
    }
    if (best == PARTY_SIZE)
        return;
    record->fallenSpecies = GetMonData(&gParties[B_TRAINER_PLAYER][best], MON_DATA_SPECIES)
                          | (IsMonShiny(&gParties[B_TRAINER_PLAYER][best]) ? ROGUE_CHAMPION_MON_SHINY : 0);
    record->fallenLevel = GetMonData(&gParties[B_TRAINER_PLAYER][best], MON_DATA_LEVEL);
    GetMonData(&gParties[B_TRAINER_PLAYER][best], MON_DATA_NICKNAME, record->fallenName);
}

static void RecordRunEnd(void)
{
    struct RogueRecord *record = Rogue_GetRecord();
    u32 progress = VarGet(VAR_ROGUE_PROGRESS);
    s32 place = Rogue_GetCurrentPlace(), dungeon = Rogue_GetFloorDungeon();

    record->lastPlace = place < 0 ? 0xFF : place;
    // Lost inside a dungeon: the summary names it (ROGUE_LAST_DUNGEON).
    if (dungeon >= 0)
        record->lastPlace = ROGUE_LAST_DUNGEON | dungeon;
    record->lastBadges = progress < ROGUE_PROGRESS_BADGE_1 ? 0 : progress - ROGUE_PROGRESS_BADGE_1 + 1;
}

void Rogue_LoseRun(void)
{
    struct RogueRecord *record = Rogue_GetRecord();
    u32 i, fallen = 0;
    u16 opponent = TRAINER_BATTLE_PARAM.opponentA;

    // The team that just went down counts as lost too.
    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        if (GetMonData(mon, MON_DATA_SANITY_HAS_SPECIES) && !GetMonData(mon, MON_DATA_IS_EGG))
            fallen++;
    }
    VarSet(VAR_ROGUE_FALLEN, VarGet(VAR_ROGUE_FALLEN) + fallen);
    VarSet(VAR_ROGUE_RUN_STATE, ROGUE_RUN_LOST);
    if (record->losses < 0xFFFF && RecordsCount())
        record->losses++;
    RecordRunEnd();
    if ((gBattleTypeFlags & BATTLE_TYPE_TRAINER) && RecordsCount()
     && opponent >= ROGUE_TRAINER(ROGUE_SLOT_LEADER_0)
     && opponent < ROGUE_TRAINER(ROGUE_SLOT_LEADER_0 + ROGUE_NUM_GYMS))
        record->unlocks |= 1u << UNLOCK_SCOUTING;
    if (VarGet(VAR_ROGUE_PROGRESS) >= ROGUE_PROGRESS_BADGE_1 + 3 && RecordsCount())
        RecordFallenFriend();
    Rogue_UpdateRecord();
    SetMainCallback2(CB2_RogueRunSummary);
}

// The feats of a win: with the starter's type (still in the team, as for
// its upgrades), with no Pokemon lost, with every dungeon cleared.
static void RecordWinFeats(void)
{
    struct RogueRecord *record = Rogue_GetRecord();
    const struct RogueRegion *region = Rogue_GetRegion();
    s32 type = Rogue_GetRunStarterType();
    u32 i, bosses = 0;

    // Every dungeon with a boss: a dungeon with none is never cleared.
    for (i = 0; i < region->nDungeons; i++)
        bosses += region->dungeons[i].boss != RBOSS_NONE;
    if (type >= 0 && Rogue_PartyHasStarter())
        record->feats |= 1u << (FEAT_WIN_GRASS + type);
    if (VarGet(VAR_ROGUE_FALLEN) == 0)
        record->feats |= 1u << FEAT_FLAWLESS;
    if (bosses != 0 && Rogue_CountDungeonsCleared() >= bosses)
        record->feats |= 1u << FEAT_ALL_DUNGEONS;
}

// Script special: the Champion is beaten. Records the win and starts the Hall
// of Fame, which saves the game and then shows the run summary.
void Rogue_GameClear(void)
{
    VarSet(VAR_ROGUE_PROGRESS, ROGUE_PROGRESS_CHAMPION);
    VarSet(VAR_ROGUE_RUN_STATE, ROGUE_RUN_WON);
    // RUN 0 never touches the real Hall of Fame, and is not one of the
    // wins: the Director's room is its end (the story's endings go here).
    // The run summary saves the game, then the Director.
    if (Rogue_IsRun0())
    {
        VarSet(VAR_ROGUE_RUN0_ACT, ROGUE_RUN0_ACT_ENDING);
        RecordRunEnd();
        // Any of its three endings is the RUN 0 feat: the fifth card star.
        Rogue_GetRecord()->feats |= 1u << FEAT_RUN0;
        HealPlayerParty();
        SetMainCallback2(CB2_RogueRunSummary);
        return;
    }
    VarSet(VAR_ROGUE_META_WINS, VarGet(VAR_ROGUE_META_WINS) + 1);
    // The lead stands beside the hero on the title screen from now on.
    VarSet(VAR_ROGUE_META_CHAMPION_MON, GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES)
                                      | (IsMonShiny(&gParties[B_TRAINER_PLAYER][0]) ? ROGUE_CHAMPION_MON_SHINY : 0));
    if (FlagGet(FLAG_ROGUE_HARDCORE))
        CountUp8(&Rogue_GetRecord()->hardcoreWins);
    RecordWinFeats();
    RecordRunEnd();
    Rogue_UpdateRecord();
    HealPlayerParty();
    gHasHallOfFameRecords = FlagGet(FLAG_SYS_GAME_CLEAR);
    FlagSet(FLAG_SYS_GAME_CLEAR);
    SetMainCallback2(CB2_DoHallOfFameScreen);
}

// Script special after a milestone. If something was just unlocked, puts its
// name in STR_VAR_1 and sets VAR_RESULT to TRUE. Call until it returns FALSE.

// VAR_RESULT is 1 for an unlock (STR_VAR_1 and STR_VAR_2 hold it), and 2
// for a legend for the LEGEND LOG or a feat, with the whole message in
// gStringVar4. The log comes first, then unlocks, then feats.
void Rogue_CheckUnlocks(void)
{
    struct RogueRecord *record;
    u32 i;
    s32 set;
    u8 *str;

    sPendingUnlocks |= Rogue_UpdateRecord();
    record = Rogue_GetRecord();
    gSpecialVar_Result = FALSE;
    if (sPendingLegend != RLEG_NONE)
    {
        // As a new POKéDEX entry is told: the log, and a set made whole.
        str = StringCopy(gStringVar4, COMPOUND_STRING("LEGEND LOG: "));
        str = StringCopy(str, GetSpeciesName(Rogue_LegendSpecies(sPendingLegend)));
        str = StringCopy(str, COMPOUND_STRING(" caught!\nThat makes "));
        str = ConvertIntToDecimalStringN(str, Rogue_CountLegends(record->legends), STR_CONV_MODE_LEFT_ALIGN, 2);
        str = StringCopy(str, COMPOUND_STRING(" of 29."));
        set = LegendSetOf(sPendingLegend);
        if (set >= 0 && (record->legends & LegendRowBits(set)) == LegendRowBits(set))
        {
            str = StringCopy(str, COMPOUND_STRING("\pThat's all of\n"));
            str = StringCopy(str, sLegendRowNames[set][1]);
            str = StringCopy(str, COMPOUND_STRING("!"));
        }
        sPendingLegend = RLEG_NONE;
        gSpecialVar_Result = 2;
        return;
    }
    // The POKeDEX upgrades come with a win, told after the HALL OF FAME;
    // one a record from before v0.14 had earned is told at the next run.
    sPendingUnlocks &= ~(UNLOCK_BIT(UNLOCK_NATIONAL_DEX) | UNLOCK_BIT(UNLOCK_GLOBAL_DEX));
    for (i = 0; i < ROGUE_NUM_UNLOCKS; i++)
    {
        if (sPendingUnlocks & UNLOCK_BIT(i))
        {
            sPendingUnlocks &= ~UNLOCK_BIT(i);
            StringCopy(gStringVar1, sUnlocks[i].name);
            StringCopy(gStringVar2, sUnlocks[i].description);
            gSpecialVar_Result = 1;
            return;
        }
    }
    for (i = 0; i < ROGUE_NUM_FEATS; i++)
    {
        if (sPendingFeats & (1u << i))
        {
            sPendingFeats &= ~(1u << i);
            str = StringCopy(gStringVar4, COMPOUND_STRING("A new FEAT: "));
            str = StringCopy(str, sFeats[i].name);
            str = StringCopy(str, COMPOUND_STRING("!\n"));
            StringCopy(str, sFeats[i].hint);
            gSpecialVar_Result = 2;
            return;
        }
    }
}

// What the professor says about each unlock at the start of the first run
// after it was earned.
static const u8 *const sUnlockNews[ROGUE_NUM_UNLOCKS] =
{
    [UNLOCK_PARTNERS]        = COMPOUND_STRING("Not every partner has to be GRASS,\nFIRE or WATER, you know.\pI found a friendly one for you!"),
    [UNLOCK_DRAGONS]         = COMPOUND_STRING("A dragon breeder owed me a favor.\nGuess who has a baby dragon?\pIt's yours, if you want it!"),
    [UNLOCK_KIT]             = COMPOUND_STRING("A CHAMPION deserves good gear!\nI packed GREAT BALLS and SUPER\lPOTIONS. Don't tell my aide."),
    [UNLOCK_VETERAN]         = COMPOUND_STRING("You've been on so many journeys!\nHere, an AMULET COIN for the kit.\pA POKéMON holding it doubles your\nprize money. Treat yourself!"),
    [UNLOCK_SCOUTING]        = COMPOUND_STRING("The GYM guides heard about your\nlast GYM battle. They felt bad!\pFrom now on, they'll tell you\nwhat each LEADER uses."),
    [UNLOCK_BERRIES]         = COMPOUND_STRING("I picked BERRIES on the way here.\nToo many, honestly.\pThey're in your kit. A POKéMON\nholding one heals itself in battle!"),
    [UNLOCK_SPARE_STARTERS]  = COMPOUND_STRING("The POKéMON you don't choose\nwon't be left behind!\pMy aide will bring them to the\nfirst and fourth GYM towns."),
    [UNLOCK_FOSSIL_STARTERS] = COMPOUND_STRING("That fossil POKéMON you revived?\nIt's doing wonderfully!\pI can offer one like it as\nyour partner now."),
    [UNLOCK_FALLEN_FRIEND]   = COMPOUND_STRING("I still think about {STR_VAR_1}.\pThen, out in the grass, I met one\njust like it.\pIt seemed to be waiting for you."),
    [UNLOCK_CHAMP_PARTNER]   = COMPOUND_STRING("Your CHAMPION partner made quite\nan impression on me.\pI found one just like it. It can\nbe your partner this time!"),
    [UNLOCK_TYPE_CHARM]      = COMPOUND_STRING("I've been studying how you raise\nyour partners. Fascinating!\pStarters of those types will hold\na charm for their type now."),
    [UNLOCK_HIDDEN_ABILITY]  = COMPOUND_STRING("Your partners went so far with\nyou, I had to rethink my notes!\pStarters of those types will come\nwith rare hidden abilities."),
    [UNLOCK_EGG_MOVE]        = COMPOUND_STRING("A starter at the POKéMON LEAGUE!\nThat's one for my notes.\pStarters of those types will know\na move from their parents."),
    [UNLOCK_SHINY_CLAUSE]    = COMPOUND_STRING("You met a shiny POKéMON? Lucky!\pIf you meet one again, you may\nalways catch it. No rule stops you."),
    [UNLOCK_SHINY_CHARM]     = COMPOUND_STRING("Three shiny POKéMON! I'm jealous.\nTake this SHINY CHARM along.\pShiny POKéMON will show up\nmore often with it."),
    [UNLOCK_FOSSILS]         = COMPOUND_STRING("All that caving paid off! Diggers\nsay fossils turn up in the cave\lbefore the fourth GYM.\pA scientist in that GYM town can\nbring them back to life!"),
    [UNLOCK_RARE_FINDS]      = COMPOUND_STRING("Fifty catches! You've inspired me.\nI'll track rare POKéMON for you.\pIn every land, I'll tell you\nwhere one was seen."),
    [UNLOCK_RIVAL_GIFT]      = COMPOUND_STRING("My kid has a present for you.\nA thank-you for all those battles!\pIt'll be waiting at the foot\nof VICTORY ROAD."),
    [UNLOCK_HARDCORE]        = COMPOUND_STRING("A CHAMPION! Want a real test now?\nTurn on the HARDCORE RULES from\lUNLOCKS on the main menu."),
    [UNLOCK_EXPLORER]        = COMPOUND_STRING("You've been so deep underground!\nI packed ESCAPE ROPES and a\lMAX REPEL. Just in case."),
    [UNLOCK_MARKING_MAP]     = COMPOUND_STRING("Your legend reports helped me\nmake a MARKING MAP.\pYour region map will show where\neach legend sleeps!"),
    [UNLOCK_MASTER_BALL]     = COMPOUND_STRING("Ten legends! I can hardly believe\nit. Take this MASTER BALL.\pIt catches any POKéMON without\nfail. Use it wisely!"),
    [UNLOCK_DOWSING]         = COMPOUND_STRING("Every dungeon cleared, and the\nCHAMPION beaten!\pTake this DOWSING MACHINE. It\nbeeps near hidden items."),
    [UNLOCK_TRAINER_CANDY]   = COMPOUND_STRING("The GYM LEADERS have heard of you.\nWord gets around!\pFrom the fourth GYM on, they'll\nadd RARE CANDIES to the BADGE."),
    [UNLOCK_LATE_CAMP]       = COMPOUND_STRING("Five BADGES in one journey!\pA friend of mine camps on the road\nto the seventh GYM.\pIf your team is worn out there,\nstop by for a rest!"),
    [UNLOCK_SECOND_CHANCE]   = COMPOUND_STRING("Lost a POKéMON you wanted to catch?\pFrom your fourth BADGE on, a few\nplaces will give you another try."),
    [UNLOCK_LUCKY_EGG]       = COMPOUND_STRING("Tough luck lately, huh? Here,\ntake this LUCKY EGG.\pA POKéMON holding it gains more\nEXP. Chin up!"),
    [UNLOCK_EEVEE]           = COMPOUND_STRING("An EEVEE has moved into my lab!\nIt's yours to choose, every time."),
    [UNLOCK_BABIES]          = COMPOUND_STRING("A nursery sent me a baby POKéMON.\nWith love, it'll grow up fast!"),
    [UNLOCK_RISING_STARS]    = COMPOUND_STRING("I caught you a rising star. It\ngrows twice. Raise it well!"),
    [UNLOCK_STARTER_CASE]    = COMPOUND_STRING("My old STARTER CASE turned up!\nPress {LEFT_ARROW}{RIGHT_ARROW} on a starter to look."),
    [UNLOCK_FAR_AWAY]        = COMPOUND_STRING("A colleague sent me POKéMON\nfrom far away regions. Look!"),
    [UNLOCK_FREE_CHOICE]     = COMPOUND_STRING("Pick whoever you like now! Press\n{LEFT_ARROW}{RIGHT_ARROW} on any row to see them all."),
    [UNLOCK_POKEDEX_PICK]    = COMPOUND_STRING("Any POKéMON you ever caught can\nstart out with you. Shinies too!"),
    [UNLOCK_ANY_POKEMON]     = COMPOUND_STRING("Three times CHAMPION! Pick any\nPOKéMON, even legends you caught!"),
    [UNLOCK_NATIONAL_DEX]    = COMPOUND_STRING("Your POKéDEX has the NATIONAL\nupgrade now!\pNext time, pick NATIONAL under\nPOKéDEX in UNLOCKS.\pPOKéMON from far regions will\nshow up much more often!"),
    [UNLOCK_GLOBAL_DEX]      = COMPOUND_STRING("Three wins! Your POKéDEX is a\nGLOBAL DEX now.\pNext time, pick GLOBAL under\nPOKéDEX in UNLOCKS.\pPOKéMON of all nine regions\nwill show up just as often."),
};

// The order the professor tells news in: the starter rows as they come up
// in the menu first.
static const u8 sNewsOrder[ROGUE_NUM_UNLOCKS] =
{
    UNLOCK_EEVEE, UNLOCK_PARTNERS, UNLOCK_BABIES, UNLOCK_RISING_STARS, UNLOCK_DRAGONS, UNLOCK_FOSSIL_STARTERS,
    UNLOCK_FAR_AWAY, UNLOCK_STARTER_CASE, UNLOCK_FREE_CHOICE, UNLOCK_POKEDEX_PICK, UNLOCK_ANY_POKEMON,
    UNLOCK_GLOBAL_DEX, UNLOCK_NATIONAL_DEX, UNLOCK_FALLEN_FRIEND, UNLOCK_CHAMP_PARTNER, UNLOCK_KIT, UNLOCK_VETERAN, UNLOCK_SCOUTING, UNLOCK_BERRIES,
    UNLOCK_SPARE_STARTERS, UNLOCK_TYPE_CHARM, UNLOCK_HIDDEN_ABILITY, UNLOCK_EGG_MOVE, UNLOCK_SHINY_CLAUSE,
    UNLOCK_SHINY_CHARM, UNLOCK_FOSSILS, UNLOCK_RARE_FINDS, UNLOCK_RIVAL_GIFT, UNLOCK_HARDCORE, UNLOCK_EXPLORER,
    UNLOCK_MARKING_MAP, UNLOCK_MASTER_BALL, UNLOCK_DOWSING, UNLOCK_TRAINER_CANDY, UNLOCK_LATE_CAMP,
    UNLOCK_SECOND_CHANCE, UNLOCK_LUCKY_EGG,
};

// Unlocks the professor has not told about yet. Ones switched off in the
// UNLOCKS menu are not news, except the Hardcore rules, which are earned
// switched off and are how a player learns to turn them on.
static u64 UnlockNews(const struct RogueRecord *record)
{
    // The POKeDEX upgrades too, which are earned switched off.
    u64 told = UNLOCK_BIT(UNLOCK_HARDCORE) | UNLOCK_BIT(UNLOCK_NATIONAL_DEX) | UNLOCK_BIT(UNLOCK_GLOBAL_DEX);
    u64 on = RECORD_BITS(record, unlocks) & ~(RECORD_BITS(record, disabled) & ~told);
    return on & ~RECORD_BITS(record, announced);
}

// Script special: TRUE in VAR_RESULT if anything was unlocked since the
// professor last spoke.
void Rogue_HasUnlockNews(void)
{
    const struct RogueRecord *record = Rogue_GetRecord();
    gSpecialVar_Result = UnlockNews(record) != 0;
}

// Script special: puts the professor's line about the next unlock he has not
// told about in gStringVar4 and sets VAR_RESULT to TRUE. Call until it
// returns FALSE. VAR_0x8005 counts the lines told: after three, one last line
// points to the UNLOCKS menu for the rest.
#define NEWS_LINES 3
void Rogue_BufferUnlockNews(void)
{
    struct RogueRecord *record = Rogue_GetRecord();
    u32 i, more = 0;
    u64 news = UnlockNews(record);

    gSpecialVar_Result = FALSE;
    // The v0.13 starter rows are all told, a box each (EEVEE and a baby
    // together); other news three at most.
    for (i = 0; i < ROGUE_NUM_UNLOCKS; i++)
    {
        u32 unlock = sNewsOrder[i];
        const u8 *line = sUnlockNews[unlock];
        // (And the POKeDEX upgrades, numbered after the rows: always told.)
        bool32 row = unlock >= UNLOCK_EEVEE;
        if (!(news & UNLOCK_BIT(unlock)))
            continue;
        if (!row && gSpecialVar_0x8005 >= NEWS_LINES)
        {
            more++;
            continue;
        }
        if (!row)
            gSpecialVar_0x8005++;
        // Both POKeDEX upgrades at once (a record from before v0.14 with
        // three wins): the GLOBAL DEX is told, and the NATIONAL with it.
        if (unlock == UNLOCK_GLOBAL_DEX)
        {
            news &= ~UNLOCK_BIT(UNLOCK_NATIONAL_DEX);
            SET_RECORD_BITS(record, announced, RECORD_BITS(record, announced) | UNLOCK_BIT(UNLOCK_NATIONAL_DEX));
        }
        // Already chosen for this run in UNLOCKS: what it does, not how.
        if ((unlock == UNLOCK_GLOBAL_DEX || unlock == UNLOCK_NATIONAL_DEX) && VarGet(VAR_ROGUE_DEX_MIX) == Rogue_GetDexChoice()
         && Rogue_GetDexChoice() == (unlock == UNLOCK_GLOBAL_DEX ? ROGUE_MIX_GLOBAL : ROGUE_MIX_NATIONAL))
            line = unlock == UNLOCK_GLOBAL_DEX
                 ? COMPOUND_STRING("Your GLOBAL DEX is on! POKéMON\nof all nine regions live here.")
                 : COMPOUND_STRING("Your NATIONAL DEX is on! Watch\nfor POKéMON from far regions.");
        if (unlock == UNLOCK_EEVEE && (news & UNLOCK_BIT(UNLOCK_BABIES)))
        {
            line = COMPOUND_STRING("EEVEE and a baby POKéMON have\nmoved into my lab. Take your pick!");
            news &= ~UNLOCK_BIT(UNLOCK_BABIES);
            SET_RECORD_BITS(record, announced, RECORD_BITS(record, announced) | UNLOCK_BIT(UNLOCK_BABIES));
        }
        SET_RECORD_BITS(record, announced, RECORD_BITS(record, announced) | UNLOCK_BIT(unlock));
        gStringVar1[0] = EOS;
        if (unlock == UNLOCK_FALLEN_FRIEND)
            StringCopyN(gStringVar1, record->fallenName, POKEMON_NAME_LENGTH);
        gStringVar1[POKEMON_NAME_LENGTH] = EOS;
        StringExpandPlaceholders(gStringVar4, line);
        gSpecialVar_Result = TRUE;
        return;
    }
    if (more != 0)
    {
        ConvertIntToDecimalStringN(gStringVar1, more, STR_CONV_MODE_LEFT_ALIGN, 2);
        StringExpandPlaceholders(gStringVar4, more == 1
            ? COMPOUND_STRING("And one more thing! You'll find it\nin UNLOCKS on the main menu.")
            : COMPOUND_STRING("And {STR_VAR_1} more things! You'll find\nthem in UNLOCKS on the main menu."));
        gSpecialVar_Result = TRUE;
    }
    SET_RECORD_BITS(record, announced, RECORD_BITS(record, unlocks));
}

// Runs, wins and unlocks found ("3 of 20") into STR_VAR_1 to STR_VAR_3.
void Rogue_BufferRecord(void)
{
    u32 count = Rogue_CountEarnedUnlocks();
    u8 *str;
    ConvertIntToDecimalStringN(gStringVar1, VarGet(VAR_ROGUE_META_RUNS), STR_CONV_MODE_LEFT_ALIGN, 4);
    ConvertIntToDecimalStringN(gStringVar2, VarGet(VAR_ROGUE_META_WINS), STR_CONV_MODE_LEFT_ALIGN, 4);
    str = ConvertIntToDecimalStringN(gStringVar3, count, STR_CONV_MODE_LEFT_ALIGN, 2);
    str = StringCopy(str, COMPOUND_STRING(" of "));
    ConvertIntToDecimalStringN(str, ROGUE_NUM_UNLOCK_ROWS, STR_CONV_MODE_LEFT_ALIGN, 2);
}

// ---------------------------------------------------------------------------
// Field moves

// Badges are the flags FLAG_BADGE01_GET on, one for each gym by its number
// along the road, set when it is won, in any order.
u32 Rogue_WonGyms(void)
{
    u32 g, won = 0;
    for (g = 0; g < ROGUE_NUM_GYMS; g++)
        if (FlagGet(FLAG_BADGE01_GET + g))
            won |= 1 << g;
    return won;
}

u32 Rogue_BadgeCount(void)
{
    u32 won = Rogue_WonGyms(), n = 0;
    for (; won; won &= won - 1)
        n++;
    return n;
}

// TRUE if a badge won lets the player use a field move. Each region rolls
// which gym's badge gives Cut, Rock Smash, Strength and Surf (gymKey); Flash
// comes with two badges, Fly with six, Dive with seven and Waterfall with
// eight, whichever they are.
bool32 Rogue_IsFieldMoveBadgeWon(u32 fieldMove)
{
    u8 obstacle;
    switch (fieldMove)
    {
    case FIELD_MOVE_CUT:        obstacle = ROBST_CUT; break;
    case FIELD_MOVE_ROCK_SMASH: obstacle = ROBST_ROCK_SMASH; break;
    case FIELD_MOVE_STRENGTH:   obstacle = ROBST_STRENGTH; break;
    case FIELD_MOVE_SURF:       obstacle = ROBST_SURF; break;
    case FIELD_MOVE_FLASH:      return Rogue_BadgeCount() >= 2;
    case FIELD_MOVE_FLY:        return Rogue_BadgeCount() >= 6;
    case FIELD_MOVE_DIVE:       return Rogue_BadgeCount() >= 7;
    default:                    return Rogue_BadgeCount() >= ROGUE_NUM_GYMS;
    }
    return (RogueGen_KeysWon(Rogue_GetRegion(), Rogue_WonGyms()) & ROGUE_KEY(obstacle)) != 0;
}

// Inside the region a badge alone lets the player use its field move: the
// party's lead Pokemon does it, whether or not it knows the move.
bool32 Rogue_HasFieldMoveBadge(u32 fieldMove)
{
    if (gSaveBlock1Ptr->location.mapGroup != MAP_GROUP(MAP_ROGUE_PLACE_0))
        return FALSE;
    if (!FlagGet(FLAG_SYS_POKEMON_GET))
        return FALSE;
    return IsFieldMoveUnlocked(fieldMove);
}

// RUN 0's MORE ending: the run is won and saved with no summary, and the
// Director told, before a new ENDLESS region starts at once
// (src/rogue_run0_story.c).
void Rogue_Run0FinishWon(void)
{
    VarSet(VAR_ROGUE_PROGRESS, ROGUE_PROGRESS_CHAMPION);
    VarSet(VAR_ROGUE_RUN_STATE, ROGUE_RUN_WON);
    VarSet(VAR_ROGUE_RUN0_ACT, ROGUE_RUN0_ACT_ENDING);
    RecordRunEnd();
    Rogue_GetRecord()->feats |= 1u << FEAT_RUN0;
    HealPlayerParty();
    TrySavingData(SAVE_NORMAL);
    Rogue_RunLogEnd(ROGUE_LOG_WON);
    Rogue_Run0OnRunEnd(TRUE);
    Rogue_DirectorNoteSave();
    Rogue_SaveDirector();
}
