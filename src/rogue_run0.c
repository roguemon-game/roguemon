// RUN 0 (Roguémon v1.0): the Director's save, the run kind, the Hall of
// Fame import, TEST TEAM, the run log, battles and resets, and the teasers.
// See include/rogue_run0.h for what each piece is for.

#include "global.h"
#include "move.h"
#include "rogue.h"
#include "rogue_gen.h"
#include "rogue_run0_story.h"
#include "battle.h"
#include "battle_main.h"
#include "battle_setup.h"
#include "battle_util.h"
#include "data.h"
#include "event_data.h"
#include "hall_of_fame.h"
#include "load_save.h"
#include "malloc.h"
#include "new_game.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "random.h"
#include "save.h"
#include "string_util.h"
#include "constants/characters.h"
#include "constants/moves.h"
#include "constants/species.h"

STATIC_ASSERT(sizeof(struct RogueDirector) <= ROGUE_DIRECTOR_SIZE, RogueDirectorFits);
STATIC_ASSERT(SECTOR_COUNTER_OFFSET == ROGUE_DIRECTOR_SIZE, RogueDirectorSectorSize);
STATIC_ASSERT(NUMBER_OF_MON_TYPES <= ROGUE_DIRECTOR_TYPES, RogueDirectorTypes);
STATIC_ASSERT(sizeof(struct RogueRunLogEntry) == 108, RogueRunLogEntrySize);
// The story's and the challenge's structs are laid over these bytes: they
// must start on a word, or the CPU's halfword and word stores land early.
STATIC_ASSERT(offsetof(struct RogueDirector, story) % 4 == 0, RogueDirectorStoryAligned);
STATIC_ASSERT(offsetof(struct RogueDirector, challenge) % 4 == 0, RogueDirectorChallengeAligned);

#define HALL_OF_FAME_MAX_TEAMS 30 // as src/hall_of_fame.c

// ---------------------------------------------------------------------------
// The Director's save

// The copy on the heap while it is loaded. InitHeap forgets it (the heap is
// reset at a battle's start and on some map loads), see Rogue_OnHeapReset.
static EWRAM_DATA struct RogueDirector *sDirector = NULL;

static u16 DirectorChecksum(const struct RogueDirector *d)
{
    const u32 *words = (const u32 *)d;
    u32 i, sum = 0;
    // Everything after the magic, version and checksum.
    for (i = 2; i < ROGUE_DIRECTOR_SIZE / 4; i++)
        sum += words[i];
    return (sum >> 16) + sum;
}

static void DirectorFresh(struct RogueDirector *d)
{
    memset(d, 0, ROGUE_DIRECTOR_SIZE);
    d->magic = ROGUE_DIRECTOR_MAGIC;
    d->version = ROGUE_DIRECTOR_VERSION;
    d->lastTownName[0] = EOS;
}

struct RogueDirector *Rogue_Director(void)
{
    if (sDirector != NULL)
        return sDirector;
    sDirector = Alloc(ROGUE_DIRECTOR_SIZE);
    if (sDirector == NULL)
        return NULL;
    if (gFlashMemoryPresent
     && TryReadSpecialSaveSector(SECTOR_ID_TRAINER_HILL, (u8 *)sDirector) == SAVE_STATUS_OK
     && sDirector->magic == ROGUE_DIRECTOR_MAGIC
     && sDirector->version == ROGUE_DIRECTOR_VERSION
     && sDirector->checksum == DirectorChecksum(sDirector))
        return sDirector;
    DirectorFresh(sDirector);
    return sDirector;
}

static void WriteDirector(struct RogueDirector *d)
{
    d->magic = ROGUE_DIRECTOR_MAGIC;
    d->version = ROGUE_DIRECTOR_VERSION;
    d->checksum = DirectorChecksum(d);
    if (gFlashMemoryPresent)
        TryWriteSpecialSaveSector(SECTOR_ID_TRAINER_HILL, (u8 *)d);
}

void Rogue_SaveDirector(void)
{
    if (sDirector == NULL)
        return;
    Rogue_Run0ForgetKeptRegion();
    WriteDirector(sDirector);
    Free(sDirector);
    sDirector = NULL;
}

void Rogue_ReleaseDirector(void)
{
    if (sDirector != NULL)
        Free(sDirector);
    sDirector = NULL;
}

bool32 Rogue_IsDirectorLoaded(void)
{
    return sDirector != NULL;
}

// InitHeap (src/malloc.c): the heap was just reset, so the copy is gone.
void Rogue_OnHeapReset(void)
{
    sDirector = NULL;
}

static void CountUp16(u16 *counter)
{
    if (*counter < 0xFFFF)
        (*counter)++;
}

// ---------------------------------------------------------------------------
// The kind of run

u32 Rogue_GetRunKind(void)
{
    return VarGet(VAR_ROGUE_RUN_KIND);
}

bool32 Rogue_IsRun0(void)
{
    return Rogue_GetRunKind() == ROGUE_RUN_KIND_RUN0;
}

bool32 Rogue_IsEndless(void)
{
    return Rogue_GetRunKind() == ROGUE_RUN_KIND_ENDLESS;
}

bool32 Rogue_IsRun0Unlocked(void)
{
    return VarGet(VAR_ROGUE_META_WINS) >= ROGUE_RUN0_WINS || Rogue_GetRecord()->losses >= ROGUE_RUN0_LOSSES;
}

// The menu row. Earned, no run in progress (the caller checks the save:
// only the menu with no run to continue shows it), and RUN 0 not left for
// good with STOP.
bool32 Rogue_ShowRun0Row(void)
{
    struct RogueDirector *d;
    bool32 show;

    if (!Rogue_IsRun0Unlocked() || !Rogue_IsRunOver())
        return FALSE;
    d = Rogue_Director();
    if (d == NULL)
        return TRUE;
    show = d->ending != ROGUE_RUN0_ENDING_STOP;
    if (show && !(d->teasers & ROGUE_TEASE_ROW))
    {
        d->teasers |= ROGUE_TEASE_ROW;
        Rogue_SaveDirector();
    }
    else
    {
        Rogue_ReleaseDirector();
    }
    return show;
}

// ---------------------------------------------------------------------------
// The run log

static struct RogueRunLogEntry *NewestEntry(struct RogueDirector *d)
{
    if (d->runLogCount == 0)
        return NULL;
    return &d->runLog[(d->runLogCount - 1) % ROGUE_RUNLOG_MAX];
}

const struct RogueRunLogEntry *Rogue_RunLogEntry(const struct RogueDirector *d, u32 n)
{
    if (d == NULL || n >= d->runLogCount || n >= ROGUE_RUNLOG_MAX)
        return NULL;
    return &d->runLog[(d->runLogCount - 1 - n) % ROGUE_RUNLOG_MAX];
}

// The newest entry if it is the run being played.
static struct RogueRunLogEntry *ThisRunEntry(struct RogueDirector *d)
{
    struct RogueRunLogEntry *e = NewestEntry(d);
    if (e == NULL || e->result != ROGUE_LOG_ACTIVE || e->run != VarGet(VAR_ROGUE_META_RUNS))
        return NULL;
    return e;
}

static void RunLogPush(struct RogueDirector *d)
{
    struct RogueRunLogEntry *e;
    u32 i;

    e = &d->runLog[d->runLogCount % ROGUE_RUNLOG_MAX];
    memset(e, 0, sizeof(*e));
    e->seed = Rogue_GetSeed();
    e->run = VarGet(VAR_ROGUE_META_RUNS);
    e->result = ROGUE_LOG_ACTIVE;
    e->place = 0xFF;
    e->kind = Rogue_GetRunKind();
    e->mix = VarGet(VAR_ROGUE_DEX_MIX);
    for (i = 0; i < ROGUE_RUNLOG_LOST; i++)
        e->lost[i].nickname[0] = EOS;
    CountUp16(&d->runLogCount);
}

static void LogLostMon(struct RogueRunLogEntry *e, struct Pokemon *mon)
{
    u32 i;
    u8 name[POKEMON_NAME_LENGTH + 1];

    if (e->lostCount < 0xFF)
        e->lostCount++;
    for (i = 0; i < ROGUE_RUNLOG_LOST; i++)
    {
        if (e->lost[i].species != SPECIES_NONE)
            continue;
        e->lost[i].species = GetMonData(mon, MON_DATA_SPECIES);
        e->lost[i].level = GetMonData(mon, MON_DATA_LEVEL);
        GetMonData(mon, MON_DATA_NICKNAME, name);
        memcpy(e->lost[i].nickname, name, POKEMON_NAME_LENGTH);
        return;
    }
}

// A new game is a different save file (the first ever, or after a corrupt
// one): whatever the sector held belongs to no one now.
static struct RogueDirector *NewGameDirector(void)
{
    struct RogueDirector *d = Rogue_Director();
    if (d != NULL && gDifferentSaveFile)
        DirectorFresh(d);
    return d;
}

// A normal run starts (Rogue_NewGame): its log entry, and the win 2 teaser.
void Rogue_RunLogStart(void)
{
    struct RogueDirector *d = NewGameDirector();

    if (d == NULL)
        return;
    RunLogPush(d);
    if (Rogue_GetRunKind() == ROGUE_RUN_KIND_NORMAL && VarGet(VAR_ROGUE_META_WINS) == 2
     && !(d->teasers & ROGUE_TEASE_TOWN) && d->lastTownName[0] != EOS)
    {
        d->teasers |= ROGUE_TEASE_TOWN;
        FlagSet(FLAG_ROGUE_TEASE_TOWN);
    }
    Rogue_SaveDirector();
}

// The run that was being played is left for a new one (Rogue_StashRecord,
// while the old save is still loaded).
void Rogue_RunLogLeave(void)
{
    struct RogueDirector *d;
    struct RogueRunLogEntry *e;

    if (gSaveFileStatus != SAVE_STATUS_OK)
        return;
    d = Rogue_Director();
    if (d == NULL)
        return;
    e = NewestEntry(d);
    if (e == NULL || e->result != ROGUE_LOG_ACTIVE)
    {
        Rogue_ReleaseDirector();
        return;
    }
    e->result = ROGUE_LOG_LEFT;
    if (e->run == VarGet(VAR_ROGUE_META_RUNS) && !Rogue_IsRunOver())
    {
        e->badges = Rogue_BadgeCount();
        e->lostCount = min(VarGet(VAR_ROGUE_FALLEN), 0xFF);
        if (e->kind == ROGUE_RUN_KIND_RUN0)
        {
            // Left in the middle: the next attempt may start at this act.
            u32 act = VarGet(VAR_ROGUE_RUN0_ACT);
            if (act >= 1 && act <= ROGUE_RUN0_ACTS)
            {
                d->checkpointAct = act;
                d->offerCheckpoint = act > 1;
                if (act > d->bestAct)
                    d->bestAct = act;
            }
            d->battleDirty = FALSE;
        }
    }
    Rogue_SaveDirector();
}

// The town the win 2 teaser names: where the run ended, if a town, else its
// first town.
static void CopyRunTown(u8 *dest)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    u32 place = Rogue_GetCurrentTownPlace();
    if (place >= r->nPlaces || r->places[place].type == RTOWN_SUMMIT)
        place = r->start;
    Rogue_CopyPlaceName(dest, place);
}

void Rogue_RunLogEnd(u32 result)
{
    const struct RogueRecord *record = Rogue_GetRecord();
    struct RogueDirector *d = Rogue_Director();
    struct RogueRunLogEntry *e;
    u8 town[32];
    u32 i;

    if (d == NULL)
        return;
    e = ThisRunEntry(d);
    if (e == NULL)
        return;
    e->result = result;
    e->badges = result == ROGUE_LOG_WON ? ROGUE_NUM_GYMS : record->lastBadges;
    e->place = record->lastPlace;
    // The team that went down with the run.
    if (result == ROGUE_LOG_LOST)
    {
        for (i = 0; i < PARTY_SIZE; i++)
        {
            struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
            if (GetMonData(mon, MON_DATA_SANITY_HAS_SPECIES) && !GetMonData(mon, MON_DATA_IS_EGG))
                LogLostMon(e, mon);
        }
    }
    e->lostCount = min(VarGet(VAR_ROGUE_FALLEN), 0xFF);
    if (e->kind == ROGUE_RUN_KIND_NORMAL)
    {
        CopyRunTown(town);
        StringCopyN(d->lastTownName, town, ROGUE_DIRECTOR_TOWN_LEN);
        d->lastTownName[ROGUE_DIRECTOR_TOWN_LEN] = EOS;
    }
}

// ---------------------------------------------------------------------------
// Your Pokemon: the Hall of Fame and TEST TEAM

// Terry's team: trained by hand before the Director learned to test itself.
// OT TERRY, his nicknames. The challenge tunes species, levels and moves
// here (a team a good player can win RUN 0 with, with difficulty).
extern s8 gRogueTestLeader[4]; // src/rogue.c

#define TERRY_OT_ID 0x07D10517
struct TerryMon
{
    u16 species;
    u8 level;
    u8 nature;
    u32 personality;
    const u8 *nickname;
    u16 item;
    u16 moves[MAX_MON_MOVES];
    u8 ppMax; // a bit per move slot with its PP raised all the way (PP MAX)
};
// Tuned by the challenge so a good player can win RUN 0 with them, and a
// careless one loses (measured with a test bot playing RUN 0).
// The first six are the team; the last two his spares, in the box.
#define TEST_TEAM_SIZE 8
static const struct TerryMon sTestTeam[TEST_TEAM_SIZE] =
{
    { SPECIES_SWELLOW,   55, NATURE_JOLLY,  0x3C1A0E21, COMPOUND_STRING("PIPER"),  ITEM_SHARP_BEAK,   { MOVE_BRAVE_BIRD, MOVE_FACADE, MOVE_QUICK_ATTACK, MOVE_U_TURN } },
    // Terry's favourite. Fake Out first, as he always led with it.
    { SPECIES_LUDICOLO,  55, NATURE_MODEST, 0x8E13B7C4, COMPOUND_STRING("GORDON"), ITEM_SITRUS_BERRY, { MOVE_SURF, MOVE_GIGA_DRAIN, MOVE_ICE_BEAM, MOVE_FAKE_OUT } },
    { SPECIES_MAGNETON,  54, NATURE_MODEST, 0x51D0A97E, COMPOUND_STRING("DAVE"),   ITEM_EVIOLITE,     { MOVE_THUNDERBOLT, MOVE_FLASH_CANNON, MOVE_THUNDER_WAVE, MOVE_TRI_ATTACK } },
    { SPECIES_GARDEVOIR, 56, NATURE_TIMID,  0x2A7F6C15, COMPOUND_STRING("MOLLY"),  ITEM_SITRUS_BERRY, { MOVE_PSYCHIC, MOVE_MOONBLAST, MOVE_CALM_MIND, MOVE_SHADOW_BALL } },
    // Earthquake with its PP raised: ten were not enough for a League.
    { SPECIES_FLYGON,    56, NATURE_JOLLY,  0x9B2E44D8, COMPOUND_STRING("KEVIN"),  ITEM_SOFT_SAND,    { MOVE_EARTHQUAKE, MOVE_DRAGON_CLAW, MOVE_CRUNCH, MOVE_ROCK_SLIDE }, 1 << 0 },
    // Close Combat: his own tip says PORYGON-Z hates Fighting.
    { SPECIES_ARCANINE,  57, NATURE_HASTY,  0x07C3E930, COMPOUND_STRING("LINDA"),  ITEM_CHARCOAL,     { MOVE_FLAMETHROWER, MOVE_EXTREME_SPEED, MOVE_CLOSE_COMBAT, MOVE_MORNING_SUN } },
    { SPECIES_HARIYAMA,  54, NATURE_ADAMANT, 0x6E05B1A3, COMPOUND_STRING("BARRY"), ITEM_BLACK_BELT,   { MOVE_CLOSE_COMBAT, MOVE_KNOCK_OFF, MOVE_BULLET_PUNCH, MOVE_ICE_PUNCH } },
    { SPECIES_WALREIN,   54, NATURE_MODEST, 0xD41C7A52, COMPOUND_STRING("JOAN"),   ITEM_LEFTOVERS,    { MOVE_ICE_BEAM, MOVE_SURF, MOVE_EARTHQUAKE, MOVE_PROTECT } },
};
static const u8 sTerryName[] = _("TERRY");
// "TEST TEAM" is nine letters and a box name holds eight: Terry typed it
// as a programmer would.
static const u8 sTestTeamBox[] = _("TESTTEAM");

// IVs that always come out the same for the same Pokemon.
static void SetHashedIVs(struct Pokemon *mon, u32 personality)
{
    u32 i, iv;
    for (i = 0; i < NUM_STATS; i++)
    {
        iv = RogueRng_Hash(personality, 0x1F0 + i) & MAX_IV_MASK;
        SetMonData(mon, MON_DATA_HP_IV + i, &iv);
    }
}

static void SetNickname(struct Pokemon *mon, const u8 *name)
{
    u8 buf[POKEMON_NAME_LENGTH + 1];
    memcpy(buf, name, POKEMON_NAME_LENGTH);
    buf[POKEMON_NAME_LENGTH] = EOS;
    if (buf[0] != EOS)
        SetMonData(mon, MON_DATA_NICKNAME, buf);
}

// The moves a player teaches along the way: a plain, strong attack of
// each type, physical and special, for the four best (Rogue_Run0BestMoves).
static const u16 sRun0TmMoves[] =
{
    MOVE_BRICK_BREAK, MOVE_FOCUS_BLAST, MOVE_AIR_SLASH, MOVE_ACROBATICS, MOVE_SLUDGE_BOMB, MOVE_POISON_JAB,
    MOVE_EARTHQUAKE, MOVE_EARTH_POWER, MOVE_ROCK_SLIDE, MOVE_STONE_EDGE, MOVE_X_SCISSOR, MOVE_BUG_BUZZ,
    MOVE_SHADOW_BALL, MOVE_SHADOW_CLAW, MOVE_IRON_HEAD, MOVE_FLASH_CANNON, MOVE_FLAMETHROWER, MOVE_FIRE_PUNCH,
    MOVE_SURF, MOVE_WATERFALL, MOVE_ENERGY_BALL, MOVE_SEED_BOMB, MOVE_THUNDERBOLT, MOVE_THUNDER_PUNCH,
    MOVE_PSYCHIC, MOVE_ZEN_HEADBUTT, MOVE_ICE_BEAM, MOVE_ICE_PUNCH, MOVE_DRAGON_PULSE, MOVE_DRAGON_CLAW,
    MOVE_DARK_PULSE, MOVE_CRUNCH, MOVE_DAZZLING_GLEAM, MOVE_PLAY_ROUGH, MOVE_BODY_SLAM,
};

// Status moves worth a slot: setting up, healing, putting a foe out of
// action. Nothing else (no Double Team, Roar, Growl or Sweet Kiss).
static const u16 sRun0StatusMoves[] =
{
    MOVE_SWORDS_DANCE, MOVE_DRAGON_DANCE, MOVE_CALM_MIND, MOVE_NASTY_PLOT, MOVE_BULK_UP, MOVE_QUIVER_DANCE,
    MOVE_RECOVER, MOVE_ROOST, MOVE_SOFT_BOILED, MOVE_SLACK_OFF, MOVE_MOONLIGHT, MOVE_MORNING_SUN, MOVE_SYNTHESIS,
    MOVE_THUNDER_WAVE, MOVE_WILL_O_WISP, MOVE_SPORE,
};

static bool32 InMoveList(const u16 *list, u32 n, u16 move)
{
    u32 i;
    for (i = 0; i < n; i++)
        if (list[i] == move)
            return TRUE;
    return FALSE;
}

// Attacks a player would not keep among their four: two turns, a turn to
// recharge, fainting, only after the others, only at the start, only on a
// sleeping foe, fixed damage, or a gamble.
static bool32 AwkwardAttack(u16 move)
{
    switch (GetMoveEffect(move))
    {
    case EFFECT_TWO_TURNS_ATTACK:
    case EFFECT_SOLAR_BEAM:
    case EFFECT_SEMI_INVULNERABLE:
    case EFFECT_SKY_DROP:
    case EFFECT_LAST_RESORT:
    case EFFECT_FOCUS_PUNCH:
    case EFFECT_DREAM_EATER:
    case EFFECT_FUTURE_SIGHT:
    case EFFECT_SNORE:
    case EFFECT_SPIT_UP:
    case EFFECT_NATURAL_GIFT:
    case EFFECT_FLING:
    case EFFECT_SYNCHRONOISE:
    case EFFECT_BELCH:
    case EFFECT_FIRST_TURN_ONLY:
    case EFFECT_FINAL_GAMBIT:
    case EFFECT_MAX_HP_50_RECOIL:
    case EFFECT_OHKO:
    case EFFECT_BIDE:
    case EFFECT_PRESENT:
    case EFFECT_REFLECT_DAMAGE:
    case EFFECT_FIXED_PERCENT_DAMAGE:
    case EFFECT_FIXED_HP_DAMAGE:
    case EFFECT_LEVEL_DAMAGE:
    case EFFECT_POLTERGEIST:
    case EFFECT_STEEL_ROLLER:
        return TRUE;
    default:
        break;
    }
    return IsExplosionMove(move) || MoveHasAdditionalEffectSelf(move, MOVE_EFFECT_RECHARGE);
}

// What a move is worth to this Pokemon: its power at its accuracy, its own
// type's bonus, its stronger side (attack or special attack), less for
// recoil; a useful status move a fixed middling worth; 0 for none.
static s32 MoveWorth(u16 species, u16 move)
{
    const struct SpeciesInfo *info = &gSpeciesInfo[species];
    u32 power, acc, strikes, stat, best;
    s32 worth;
    if (GetMoveCategory(move) == DAMAGE_CATEGORY_STATUS)
        return InMoveList(sRun0StatusMoves, ARRAY_COUNT(sRun0StatusMoves), move) ? 55 : 0;
    if (AwkwardAttack(move))
        return 0;
    power = GetMovePower(move);
    if (power <= 1)
        power = 60; // power by weight or speed
    strikes = GetMoveStrikeCount(move);
    if (strikes > 1)
        power *= strikes;
    acc = GetMoveAccuracy(move);
    if (acc == 0 || acc > 100)
        acc = 100;
    worth = power * acc / 100;
    if (GetMoveType(move) == GetSpeciesType(species, 0) || GetMoveType(move) == GetSpeciesType(species, 1))
        worth = worth * 3 / 2;
    stat = GetMoveCategory(move) == DAMAGE_CATEGORY_PHYSICAL ? info->baseAttack : info->baseSpAttack;
    best = max(info->baseAttack, info->baseSpAttack);
    if (best != 0)
        worth = worth * (50 + 50 * stat / best) / 100;
    if (GetMoveEffect(move) == EFFECT_RECOIL)
        worth = worth * (100 - GetMoveRecoil(move) / 2) / 100;
    if (GetMovePriority(move) > 0)
        worth = worth * 11 / 10;
    return worth > 1 ? worth : 1;
}

// The four best moves a Pokemon knows by its level, from what it learns by
// then and the plain TM attacks it can learn: the strongest by power,
// accuracy, its own types and its better side, then a spread of types (a
// second move of a type counts half), at most one status move and only a
// useful one. (The last four learned left a TOGEKISS with Growl, Pound,
// Sweet Kiss and Life Dew; level-up only, with Sky Attack and Last Resort.)
u32 Rogue_Run0BestMoves(u16 species, u32 level, u16 *out)
{
    const struct LevelUpMove *ls = GetSpeciesLevelUpLearnset(species);
    u16 cand[96];
    s32 worth[96];
    u32 n = 0, i, k, j, chosen = 0;
    bool32 status = FALSE;

    for (i = 0; ls != NULL && ls[i].move != LEVEL_UP_MOVE_END && i < 128 && n < ARRAY_COUNT(cand); i++)
        if (ls[i].level <= level && ls[i].move != MOVE_NONE && !InMoveList(cand, n, ls[i].move))
            cand[n++] = ls[i].move;
    for (i = 0; i < ARRAY_COUNT(sRun0TmMoves) && n < ARRAY_COUNT(cand); i++)
        if (!InMoveList(cand, n, sRun0TmMoves[i]) && CanLearnTeachableMove(species, sRun0TmMoves[i]))
            cand[n++] = sRun0TmMoves[i];
    for (i = 0; i < n; i++)
        worth[i] = MoveWorth(species, cand[i]);
    for (k = 0; k < MAX_MON_MOVES; k++)
    {
        s32 best = -1, bestWorth = 0;
        for (i = 0; i < n; i++)
        {
            s32 w = worth[i];
            bool32 isStatus = GetMoveCategory(cand[i]) == DAMAGE_CATEGORY_STATUS;
            if (w <= 0 || InMoveList(out, chosen, cand[i]) || (isStatus && status))
                continue;
            for (j = 0; j < chosen && !isStatus; j++)
                if (GetMoveCategory(out[j]) != DAMAGE_CATEGORY_STATUS && GetMoveType(out[j]) == GetMoveType(cand[i]))
                    w /= 2;
            if (w > bestWorth)
            {
                best = i;
                bestWorth = w;
            }
        }
        if (best < 0)
            break;
        status |= GetMoveCategory(cand[best]) == DAMAGE_CATEGORY_STATUS;
        out[chosen++] = cand[best];
    }
    for (k = chosen; k < MAX_MON_MOVES; k++)
        out[k] = MOVE_NONE;
    return chosen;
}

static void SetBestMoves(struct Pokemon *mon, u16 species, u32 level)
{
    u16 moves[MAX_MON_MOVES];
    u32 k, n = Rogue_Run0BestMoves(species, level, moves);
    if (n == 0)
        return;
    for (k = 0; k < MAX_MON_MOVES; k++)
        SetMonMoveSlot(mon, moves[k], k);
}

// A Hall of Fame Pokemon, rebuilt: its personality, the player's OT, the
// best four moves it learns by level, IVs from its personality, no item.
static void BuildHofMon(struct Pokemon *mon, const struct HallofFameMon *hof)
{
    bool32 shiny = hof->isShiny;
    u32 level = hof->lvl ? hof->lvl : 5;
    // Test hook (tools/roguemon/run0bench.py --ready): the team at a level
    // of the bench's, as BuildTerryMon's.
    if (gRogueTestLeader[0] == 'T' && (u8)gRogueTestLeader[1] >= 1 && (u8)gRogueTestLeader[1] <= MAX_LEVEL)
        level = (u8)gRogueTestLeader[1] - (level < 59);
    CreateMon(mon, hof->species, level, hof->personality, OTID_STRUCT_PLAYER_ID);
    SetMonData(mon, MON_DATA_IS_SHINY, &shiny);
    SetHashedIVs(mon, hof->personality);
    SetNickname(mon, hof->nickname);
    GiveMonInitialMoveset(mon);
    SetBestMoves(mon, hof->species, level);
    CalculateMonStats(mon);
}

static void BuildTerryMonAt(struct Pokemon *mon, const struct TerryMon *t, u32 level)
{
    u32 i, gender = MALE;
    CreateMon(mon, t->species, level, t->personality, OTID_STRUCT_PRESET(TERRY_OT_ID));
    SetHashedIVs(mon, t->personality);
    SetMonData(mon, MON_DATA_OT_NAME, sTerryName);
    SetMonData(mon, MON_DATA_OT_GENDER, &gender);
    SetMonData(mon, MON_DATA_NICKNAME, t->nickname);
    for (i = 0; i < MAX_MON_MOVES; i++)
        SetMonMoveSlot(mon, t->moves[i], i);
    if (t->ppMax)
    {
        u8 bonuses = 0;
        for (i = 0; i < MAX_MON_MOVES; i++)
            if (t->ppMax & (1 << i))
                bonuses |= 3 << (i * 2);
        SetMonData(mon, MON_DATA_PP_BONUSES, &bonuses);
        MonRestorePP(mon);
    }
    // His natures (a mint's, so the personality keeps the rest) and items.
    SetMonData(mon, MON_DATA_HIDDEN_NATURE, &t->nature);
    SetMonData(mon, MON_DATA_HELD_ITEM, &t->item);
    CalculateMonStats(mon);
}

static void BuildTerryMon(struct Pokemon *mon, const struct TerryMon *t)
{
    u32 level = t->level;
    // Test hook (tools/roguemon/run0bench.py --league): TEST TEAM at a level
    // of the bench's ('T' in gRogueTestLeader[0], the level in [1]).
    if (gRogueTestLeader[0] == 'T' && (u8)gRogueTestLeader[1] >= 1 && (u8)gRogueTestLeader[1] <= MAX_LEVEL)
        level = (u8)gRogueTestLeader[1] - (t->level < 56);
    BuildTerryMonAt(mon, t, level);
}

void Rogue_Run0BuildKeptMon(struct Pokemon *mon, const struct RogueKeptMon *k, u32 level)
{
    u8 name[POKEMON_NAME_LENGTH + 1];
    bool32 shiny = (k->flags & ROGUE_KEPT_SHINY) != 0;
    u32 i;
    if (level < 1 || level > MAX_LEVEL)
        level = k->level ? k->level : 5;
    // Terry's, with his own set.
    for (i = 0; i < TEST_TEAM_SIZE; i++)
    {
        if (sTestTeam[i].species == k->species && sTestTeam[i].personality == k->personality)
        {
            BuildTerryMonAt(mon, &sTestTeam[i], level);
            return;
        }
    }
    CreateMon(mon, k->species, level, k->personality, OTID_STRUCT_PLAYER_ID);
    SetMonData(mon, MON_DATA_IS_SHINY, &shiny);
    SetHashedIVs(mon, k->personality);
    memcpy(name, k->nickname, POKEMON_NAME_LENGTH);
    name[POKEMON_NAME_LENGTH] = EOS;
    SetNickname(mon, name);
    GiveMonInitialMoveset(mon);
    SetBestMoves(mon, k->species, level);
    CalculateMonStats(mon);
}

u16 Rogue_Run0TerrySpecies(u32 i, u8 *level)
{
    i %= PARTY_SIZE;
    *level = sTestTeam[i].level;
    return sTestTeam[i].species;
}

bool32 Rogue_IsTerrysMon(struct Pokemon *mon)
{
    return GetMonData(mon, MON_DATA_OT_ID) == TERRY_OT_ID;
}

static u32 KeptSlots(const struct RogueDirector *d)
{
    return d->keptCount < ROGUE_KEPT_MAX ? d->keptCount : ROGUE_KEPT_MAX;
}

// The Pokemon the Director keeps, each counted once (an older save may hold
// one twice: given back, then lost again).
static u32 DistinctKept(const struct RogueDirector *d)
{
    u32 i, j, n = KeptSlots(d), count = 0;
    for (i = 0; i < n; i++)
    {
        for (j = i + 1; j < n; j++)
            if (d->kept[j].species == d->kept[i].species && d->kept[j].personality == d->kept[i].personality)
                break;
        if (j == n && d->kept[i].species != SPECIES_NONE)
            count++;
    }
    return count;
}

static bool32 IsKept(const struct RogueDirector *d, u16 species, u32 personality)
{
    u32 i;
    if (d == NULL)
        return FALSE;
    for (i = 0; i < d->keptCount && i < ROGUE_KEPT_MAX; i++)
        if (d->kept[i].species == species && d->kept[i].personality == personality
         && !(d->kept[i].flags & ROGUE_KEPT_RETURNED))
            return TRUE;
    return FALSE;
}

// The Hall of Fame's teams (the last 30, oldest first), on the heap; the
// caller frees it. NULL if there are none.
static struct HallofFameTeam *LoadHofTeams(u32 *count)
{
    struct HallofFameTeam *teams = Alloc(SECTOR_DATA_SIZE * NUM_HOF_SECTORS);
    u32 n;

    *count = 0;
    if (teams == NULL)
        return NULL;
    // Read into a buffer of our own: the real Hall of Fame is never written.
    gHoFSaveBuffer = teams;
    if (LoadGameSave(SAVE_HALL_OF_FAME) != SAVE_STATUS_OK)
    {
        gHoFSaveBuffer = NULL;
        Free(teams);
        return NULL;
    }
    gHoFSaveBuffer = NULL;
    for (n = 0; n < HALL_OF_FAME_MAX_TEAMS && teams[n].mon[0].species != SPECIES_NONE; n++)
        ;
    *count = n;
    if (n == 0)
    {
        Free(teams);
        return NULL;
    }
    return teams;
}

u32 Rogue_CountHofTeams(void)
{
    u32 n;
    struct HallofFameTeam *teams = LoadHofTeams(&n);
    if (teams != NULL)
        Free(teams);
    return n;
}

// ---------------------------------------------------------------------------
// What the challenge's trainers read (src/rogue_run0_trainers.h)

u32 Rogue_Run0KeptMons(u16 *species, u8 *levels, u8 *index, u32 max)
{
    bool32 was = Rogue_IsDirectorLoaded();
    struct RogueDirector *d = Rogue_Director();
    u32 n = 0, pass, i;
    if (d == NULL)
        return 0;
    // Terry's first, then the rest, newest first.
    for (pass = 0; pass < 2; pass++)
    {
        for (i = d->keptCount < ROGUE_KEPT_MAX ? d->keptCount : ROGUE_KEPT_MAX; i-- > 0 && n < max;)
        {
            const struct RogueKeptMon *k = &d->kept[i];
            if (k->species == SPECIES_NONE || k->species >= NUM_SPECIES || (k->flags & ROGUE_KEPT_RETURNED)
             || !!(k->flags & ROGUE_KEPT_TERRY) != (pass == 0))
                continue;
            species[n] = k->species;
            levels[n] = k->level;
            index[n] = i;
            n++;
        }
    }
    if (!was)
        Rogue_ReleaseDirector();
    return n;
}

u32 Rogue_Run0LostMons(u16 *species, u8 *levels, u8 *entry, u8 *lost, u32 max)
{
    bool32 was = Rogue_IsDirectorLoaded();
    struct RogueDirector *d = Rogue_Director();
    const struct RogueRunLogEntry *e;
    u32 n = 0, k, i;
    if (d == NULL)
        return 0;
    for (i = 0; n < max && (e = Rogue_RunLogEntry(d, i)) != NULL; i++)
    {
        if (e->kind == ROGUE_RUN_KIND_RUN0)
            continue;
        for (k = 0; k < ROGUE_RUNLOG_LOST && n < max; k++)
        {
            if (e->lost[k].species == SPECIES_NONE || e->lost[k].species >= NUM_SPECIES)
                continue;
            species[n] = e->lost[k].species;
            levels[n] = e->lost[k].level;
            entry[n] = i;
            lost[n] = k;
            n++;
        }
    }
    if (!was)
        Rogue_ReleaseDirector();
    return n;
}

s32 Rogue_Run0FirstRunSeed(void)
{
    bool32 was = Rogue_IsDirectorLoaded();
    struct RogueDirector *d = Rogue_Director();
    const struct RogueRunLogEntry *e;
    s32 seed = -1;
    u32 i;
    for (i = 0; d != NULL && (e = Rogue_RunLogEntry(d, i)) != NULL; i++)
        if (e->kind == ROGUE_RUN_KIND_NORMAL && e->seed < ROGUE_RUN0_SEED)
            seed = e->seed;
    if (!was)
        Rogue_ReleaseDirector();
    return seed;
}

u32 Rogue_Run0HofTeam(u32 team, u16 *species, u8 *levels)
{
    u32 n, i, size = 0;
    struct HallofFameTeam *teams = LoadHofTeams(&n);
    if (teams == NULL)
        return 0;
    for (i = 0; team < n && i < PARTY_SIZE; i++)
    {
        if (teams[team].mon[i].species == SPECIES_NONE || teams[team].mon[i].species >= NUM_SPECIES)
            break;
        species[size] = teams[team].mon[i].species;
        levels[size] = teams[team].mon[i].lvl;
        size++;
    }
    Free(teams);
    return size;
}

u32 Rogue_Run0HofAces(u16 *species, u8 *team, u8 *mon, u32 max)
{
    u32 n, t, i, count = 0;
    struct HallofFameTeam *teams = LoadHofTeams(&n);
    if (teams == NULL)
        return 0;
    for (t = n; t-- > 0 && count < max;)
    {
        u32 best = 0;
        for (i = 1; i < PARTY_SIZE && teams[t].mon[i].species != SPECIES_NONE; i++)
            if (teams[t].mon[i].lvl > teams[t].mon[best].lvl)
                best = i;
        if (teams[t].mon[best].species == SPECIES_NONE || teams[t].mon[best].species >= NUM_SPECIES)
            continue;
        // One of each species: two wins led by the same ace field it once.
        for (i = 0; i < count && species[i] != teams[t].mon[best].species; i++)
            ;
        if (i < count)
            continue;
        species[count] = teams[t].mon[best].species;
        team[count] = t;
        mon[count] = best;
        count++;
    }
    Free(teams);
    return count;
}

static void NameMon(struct Pokemon *mon, const u8 *name)
{
    u8 buf[POKEMON_NAME_LENGTH + 1];
    memcpy(buf, name, POKEMON_NAME_LENGTH);
    buf[POKEMON_NAME_LENGTH] = EOS;
    if (buf[0] != EOS && buf[0] != 0)
        SetMonData(mon, MON_DATA_NICKNAME, buf);
}

// A trainer's Pokemon made again with the player's personality, so it comes
// back as it was (its gender: a female LINDA came back male): the same
// species, level, moves, item, IVs, EVs and ability slot, and still the
// trainer's: its OT, ball, friendship and the marks that keep the AI from
// Dynamaxing or Terastallizing it. CreateMon gives a fresh Pokemon a Dynamax
// level and a Tera type, and the AI reads those as "use it": v1.0.0's kept
// Pokemon Dynamaxed, then Terastallized after it ended.
static void Repersonalize(struct Pokemon *mon, u32 personality, bool32 shiny)
{
    u16 species = GetMonData(mon, MON_DATA_SPECIES), moves[MAX_MON_MOVES], item = GetMonData(mon, MON_DATA_HELD_ITEM);
    u32 level = GetMonData(mon, MON_DATA_LEVEL), otId = GetMonData(mon, MON_DATA_OT_ID);
    u32 abilityNum = GetMonData(mon, MON_DATA_ABILITY_NUM), i;
    u32 dynamax = GetMonData(mon, MON_DATA_DYNAMAX_LEVEL), gmax = GetMonData(mon, MON_DATA_GIGANTAMAX_FACTOR);
    u32 tera = GetMonData(mon, MON_DATA_TERA_TYPE), ball = GetMonData(mon, MON_DATA_POKEBALL);
    u32 friendship = GetMonData(mon, MON_DATA_FRIENDSHIP), otGender = GetMonData(mon, MON_DATA_OT_GENDER);
    u8 ivs[NUM_STATS], evs[NUM_STATS], otName[PLAYER_NAME_LENGTH + 1];
    if (species == SPECIES_NONE || GetMonData(mon, MON_DATA_PERSONALITY) == personality)
        return;
    for (i = 0; i < MAX_MON_MOVES; i++)
        moves[i] = GetMonData(mon, MON_DATA_MOVE1 + i);
    for (i = 0; i < NUM_STATS; i++)
    {
        ivs[i] = GetMonData(mon, MON_DATA_HP_IV + i);
        evs[i] = GetMonData(mon, MON_DATA_HP_EV + i);
    }
    GetMonData(mon, MON_DATA_OT_NAME, otName);
    CreateMon(mon, species, level, personality, OTID_STRUCT_PRESET(otId));
    for (i = 0; i < MAX_MON_MOVES; i++)
        SetMonMoveSlot(mon, moves[i], i);
    for (i = 0; i < NUM_STATS; i++)
    {
        SetMonData(mon, MON_DATA_HP_IV + i, &ivs[i]);
        SetMonData(mon, MON_DATA_HP_EV + i, &evs[i]);
    }
    SetMonData(mon, MON_DATA_HELD_ITEM, &item);
    SetMonData(mon, MON_DATA_ABILITY_NUM, &abilityNum);
    SetMonData(mon, MON_DATA_IS_SHINY, &shiny);
    SetMonData(mon, MON_DATA_DYNAMAX_LEVEL, &dynamax);
    SetMonData(mon, MON_DATA_GIGANTAMAX_FACTOR, &gmax);
    SetMonData(mon, MON_DATA_TERA_TYPE, &tera);
    SetMonData(mon, MON_DATA_POKEBALL, &ball);
    SetMonData(mon, MON_DATA_FRIENDSHIP, &friendship);
    SetMonData(mon, MON_DATA_OT_NAME, otName);
    SetMonData(mon, MON_DATA_OT_GENDER, &otGender);
    CalculateMonStats(mon);
}

// The trainer's Pokemon, made: the ones that were the player's get their
// nicknames back (and a shiny its colours, and their personality); the Copy
// is the player's party.
void Rogue_Run0FinishTrainerParty(struct Pokemon *party, const struct TrainerMon *mons, u32 count)
{
    struct RogueDirector *d = NULL;
    struct HallofFameTeam *teams = NULL;
    bool32 was = Rogue_IsDirectorLoaded();
    u32 i, nTeams = 0;

    for (i = 0; i < count; i++)
    {
        u32 kind = mons[i].tags >> RUN0_TAG_SHIFT, a = (mons[i].tags >> 16) & 0xFF, b = (mons[i].tags >> 8) & 0xFF;
        struct Pokemon *mon = &party[i];
        switch (kind)
        {
        case RUN0_TAG_KEPT:
            if (d == NULL)
                d = Rogue_Director();
            if (d != NULL && a < ROGUE_KEPT_MAX)
            {
                bool32 shiny = (d->kept[a].flags & ROGUE_KEPT_SHINY) != 0;
                Repersonalize(mon, d->kept[a].personality, shiny);
                NameMon(mon, d->kept[a].nickname);
                SetMonData(mon, MON_DATA_IS_SHINY, &shiny);
            }
            break;
        case RUN0_TAG_RUNLOG:
        {
            const struct RogueRunLogEntry *e;
            if (d == NULL)
                d = Rogue_Director();
            if (d != NULL && (e = Rogue_RunLogEntry(d, a)) != NULL && b < ROGUE_RUNLOG_LOST)
                NameMon(mon, e->lost[b].nickname);
            break;
        }
        case RUN0_TAG_HOF:
            if (teams == NULL)
                teams = LoadHofTeams(&nTeams);
            if (teams != NULL && a < nTeams && b < PARTY_SIZE)
            {
                bool32 shiny = teams[a].mon[b].isShiny;
                Repersonalize(mon, teams[a].mon[b].personality, shiny);
                NameMon(mon, teams[a].mon[b].nickname);
                SetMonData(mon, MON_DATA_IS_SHINY, &shiny);
            }
            break;
        case RUN0_TAG_COPY:
            // "Your current party, exactly, built when the battle starts":
            // as fit as it was when it was made (its HP, PP and status too).
            // It is still a trainer's: it never Dynamaxes or Terastallizes.
            if (a < PARTY_SIZE && GetMonData(&gParties[B_TRAINER_PLAYER][a], MON_DATA_SANITY_HAS_SPECIES))
            {
                u32 block = BLOCK_AI_DYNAMAX, tera = TYPE_MYSTERY, gmax = FALSE;
                *mon = gParties[B_TRAINER_PLAYER][a];
                SetMonData(mon, MON_DATA_DYNAMAX_LEVEL, &block);
                SetMonData(mon, MON_DATA_GIGANTAMAX_FACTOR, &gmax);
                SetMonData(mon, MON_DATA_TERA_TYPE, &tera);
            }
            break;
        case RUN0_TAG_TERRY:
            if (a < PARTY_SIZE)
                SetMonData(mon, MON_DATA_NICKNAME, sTestTeam[a].nickname);
            break;
        }
    }
    if (teams != NULL)
        Free(teams);
    if (d != NULL && !was)
        Rogue_ReleaseDirector();
}

// The k-th newest win of the run log (not RUN 0's), its region seed, or -1.
static s32 WonSeed(const struct RogueDirector *d, u32 k)
{
    u32 i;
    const struct RogueRunLogEntry *e;
    for (i = 0; d != NULL && (e = Rogue_RunLogEntry(d, i)) != NULL; i++)
    {
        if (e->result != ROGUE_LOG_WON || e->kind == ROGUE_RUN_KIND_RUN0)
            continue;
        if (k-- == 0)
            return e->seed;
    }
    return -1;
}

static void NameTeamBox(u32 box, const struct RogueDirector *d, u32 team, u32 count)
{
    u8 name[ROGUE_REGION_NAME_LEN + 2];
    u8 *dest = GetBoxNamePtr(box);
    s32 seed = WonSeed(d, count - 1 - team);
    if (seed >= 0)
    {
        Rogue_RegionName(name, seed);
        name[BOX_NAME_LENGTH] = EOS;
        StringCopy(dest, name);
    }
    else
    {
        u8 *end = StringCopy(dest, COMPOUND_STRING("TEAM "));
        ConvertIntToDecimalStringN(end, team + 1, STR_CONV_MODE_LEFT_ALIGN, 2);
    }
}

// Puts the team's Pokemon the Director has not kept into a box from pos on;
// returns the next free position.
static u32 BoxTeam(const struct HallofFameTeam *team, const struct RogueDirector *d, u32 box, u32 pos, struct Pokemon *work)
{
    u32 i;
    for (i = 0; i < PARTY_SIZE && pos < IN_BOX_COUNT; i++)
    {
        const struct HallofFameMon *hof = &team->mon[i];
        if (hof->species == SPECIES_NONE || IsKept(d, hof->species, hof->personality))
            continue;
        BuildHofMon(work, hof);
        SetBoxMonAt(box, pos++, &work->box);
    }
    return pos;
}

// Does a team have a Pokemon the Director has not kept?
static bool32 TeamLeft(const struct HallofFameTeam *team, const struct RogueDirector *d)
{
    u32 i;
    for (i = 0; i < PARTY_SIZE; i++)
        if (team->mon[i].species != SPECIES_NONE && !IsKept(d, team->mon[i].species, team->mon[i].personality))
            return TRUE;
    return FALSE;
}

u32 Rogue_Run0ImportTeams(void)
{
    struct RogueDirector *d = Rogue_Director();
    struct HallofFameTeam *teams;
    struct Pokemon *work = AllocZeroed(sizeof(struct Pokemon));
    u32 n, i, slot = 0, box, others, singles, team;
    s32 partyTeam = -1;

    if (work == NULL)
        return 0;
    teams = LoadHofTeams(&n);
    ZeroPlayerPartyMons();

    // The party: the latest champion team, or the newest one the Director
    // has not kept all of.
    for (i = n; i-- > 0 && partyTeam < 0;)
        if (TeamLeft(&teams[i], d))
            partyTeam = i;

    // Box 1: TEST TEAM, Terry's. With no team for the party they are the
    // party (all of them, if the Director had kept every one).
    StringCopy(GetBoxNamePtr(ROGUE_TEST_TEAM_BOX), sTestTeamBox);
    for (i = 0; i < TEST_TEAM_SIZE; i++)
    {
        BuildTerryMon(work, &sTestTeam[i]);
        if (IsKept(d, sTestTeam[i].species, sTestTeam[i].personality) && (partyTeam >= 0 || i >= PARTY_SIZE))
            continue;
        // His two spares stay in the box.
        if (partyTeam < 0 && i < PARTY_SIZE)
            gParties[B_TRAINER_PLAYER][slot++] = *work;
        else
            SetBoxMonAt(ROGUE_TEST_TEAM_BOX, i, &work->box);
    }
    if (partyTeam >= 0)
    {
        for (i = 0; i < PARTY_SIZE; i++)
        {
            const struct HallofFameMon *hof = &teams[partyTeam].mon[i];
            if (hof->species == SPECIES_NONE || IsKept(d, hof->species, hof->personality))
                continue;
            BuildHofMon(&gParties[B_TRAINER_PLAYER][slot++], hof);
        }
    }

    // The other teams, newest first, a box each, after TEST TEAM. With more
    // than the boxes hold, the oldest share boxes, five teams to a box.
    others = n - (partyTeam >= 0);
    singles = others;
    if (others > TOTAL_BOXES_COUNT - 1)
        for (singles = TOTAL_BOXES_COUNT - 1; singles + (others - singles + 4) / 5 > TOTAL_BOXES_COUNT - 1; singles--)
            ;
    box = ROGUE_TEST_TEAM_BOX + 1;
    team = n;
    for (i = 0; i < singles; i++, box++)
    {
        if (--team == (u32)partyTeam)
            team--;
        NameTeamBox(box, d, team, n);
        BoxTeam(&teams[team], d, box, 0, work);
    }
    // The oldest, five teams a box, named TEAM 1+ (from the first win) and on.
    while (i < others && box < TOTAL_BOXES_COUNT)
    {
        u32 k, pos = 0;
        for (k = 0; k < 5 && i < others; k++, i++)
        {
            if (--team == (u32)partyTeam)
                team--;
            pos = BoxTeam(&teams[team], d, box, pos, work);
        }
        ConvertIntToDecimalStringN(StringCopy(GetBoxNamePtr(box), COMPOUND_STRING("TEAM ")), team + 1, STR_CONV_MODE_LEFT_ALIGN, 2);
        StringAppend(GetBoxNamePtr(box), COMPOUND_STRING("+"));
        box++;
    }
    gPartiesCount[B_TRAINER_PLAYER] = slot;
    CalculatePlayerPartyCount();
    if (teams != NULL)
        Free(teams);
    Free(work);
    return n;
}

// ---------------------------------------------------------------------------
// A RUN 0 attempt starts

// How strong a Pokemon in a box looks: its level, then its base stats.
static u32 BoxStrength(struct BoxPokemon *box)
{
    u16 species = GetBoxMonData(box, MON_DATA_SPECIES);
    const struct SpeciesInfo *s;
    if (species == SPECIES_NONE || species >= NUM_SPECIES || GetBoxMonData(box, MON_DATA_IS_EGG))
        return 0;
    s = &gSpeciesInfo[species];
    return GetLevelFromBoxMonExp(box) * 1000 + s->baseHP + s->baseAttack + s->baseDefense
         + s->baseSpeed + s->baseSpAttack + s->baseSpDefense;
}

// Every attempt starts with a team: a party short of six (the Director kept
// the rest of the latest team: an attempt started with one ROSERADE) takes
// the strongest left in the boxes; with fewer than six in all, the Director
// lends ones it kept, the newest first ("Bring them back."). Returns how
// many it lent.
static u32 FillParty(struct RogueDirector *d)
{
    u32 slot = CalculatePlayerPartyCount(), lent = 0, b, pos, best, bestBox = 0, bestPos = 0, i;
    while (slot < PARTY_SIZE)
    {
        best = 0;
        for (b = 0; b < TOTAL_BOXES_COUNT; b++)
        {
            for (pos = 0; pos < IN_BOX_COUNT; pos++)
            {
                u32 strength = BoxStrength(GetBoxedMonPtr(b, pos));
                if (strength > best)
                {
                    best = strength;
                    bestBox = b;
                    bestPos = pos;
                }
            }
        }
        if (best == 0)
            break;
        BoxMonToMon(GetBoxedMonPtr(bestBox, bestPos), &gParties[B_TRAINER_PLAYER][slot++]);
        ZeroBoxMonAt(bestBox, bestPos);
    }
    for (i = KeptSlots(d); d != NULL && slot < PARTY_SIZE && i-- > 0;)
    {
        struct RogueKeptMon *k = &d->kept[i];
        if (k->species == SPECIES_NONE || k->species >= NUM_SPECIES || (k->flags & ROGUE_KEPT_RETURNED))
            continue;
        Rogue_Run0BuildKeptMon(&gParties[B_TRAINER_PLAYER][slot++], k, k->level);
        k->flags |= ROGUE_KEPT_RETURNED;
        lent++;
    }
    gPartiesCount[B_TRAINER_PLAYER] = slot;
    CalculatePlayerPartyCount();
    return lent;
}

// The boxes at the band's floor too (the ones the Director had no room to
// keep came back to the PC), as a withdrawal would raise them.
static void RaiseBoxesToFloor(void)
{
    u32 floor = Rogue_Run0BandFloor(), b, pos;
    if (floor > MAX_LEVEL)
        floor = MAX_LEVEL;
    for (b = 0; b < TOTAL_BOXES_COUNT; b++)
    {
        for (pos = 0; pos < IN_BOX_COUNT; pos++)
        {
            struct BoxPokemon *box = GetBoxedMonPtr(b, pos);
            u16 species = GetBoxMonData(box, MON_DATA_SPECIES);
            u32 exp;
            if (species == SPECIES_NONE || species >= NUM_SPECIES || GetBoxMonData(box, MON_DATA_IS_EGG)
             || GetLevelFromBoxMonExp(box) >= floor)
                continue;
            exp = gExperienceTables[gSpeciesInfo[species].growthRate][floor];
            SetBoxMonData(box, MON_DATA_EXP, &exp);
        }
    }
}

u32 Rogue_Run0LentCount(void)
{
    struct RogueDirector *d;
    u32 n = 0;
    if (!Rogue_IsRun0())
        return 0;
    d = Rogue_Director();
    if (d != NULL && RUN0_CHALLENGE(d)->version == RUN0_CHALLENGE_VERSION)
        n = RUN0_CHALLENGE(d)->lent;
    Rogue_ReleaseDirector();
    return n;
}

void Rogue_Run0ApplyAct(u32 act)
{
    u32 i, badges;
    if (act < 1 || act > ROGUE_RUN0_ACTS)
        act = 1;
    VarSet(VAR_ROGUE_RUN0_ACT, act);
    // Two gyms an act: the acts before were won. (The region and the story
    // put the player at the act's start; until then the lab.)
    badges = (act - 1) * 2;
    for (i = 0; i < badges; i++)
        FlagSet(FLAG_BADGE01_GET + i);
}

// Script special at the end of the lab's arrival scene (the story's,
// Rogue_EventScript_Run0Arrival): the run is under way, with the badges of
// the acts before the one it started at.
void Rogue_Run0Arrived(void)
{
    u32 badges = Rogue_BadgeCount();
    FlagSet(FLAG_ROGUE_RUN0_ARRIVED);
    VarSet(VAR_ROGUE_PROGRESS, badges ? ROGUE_PROGRESS_BADGE_1 + badges - 1 : ROGUE_PROGRESS_STARTER);
}

void Rogue_Run0NewGame(void)
{
    struct RogueDirector *d = NewGameDirector();
    u32 act = 1, lent, i;

    if (d != NULL)
    {
        if (d->startAct >= 2 && d->startAct <= ROGUE_RUN0_ACTS)
            act = d->startAct;
        d->startAct = 0;
        d->offerCheckpoint = FALSE;
        CountUp16(&d->attempts);
        // This attempt's play starts from nothing.
        d->patches = 0;
        d->battles = 0;
        memset(d->leadTypes, 0, sizeof(d->leadTypes));
        d->heals = d->itemsUsed = d->catches = d->faints = 0;
        d->battleDirty = FALSE;
        // The last attempt's losses are no longer "just kept".
        for (i = 0; i < KeptSlots(d); i++)
            d->kept[i].flags &= ~ROGUE_KEPT_NEW;
    }
    Rogue_Run0ImportTeams();
    lent = d != NULL ? FillParty(d) : 0;
    // The challenge: the band from this party, the counter-pick from the last
    // loss, the play counted from nothing (src/rogue_run0_challenge.c).
    if (d != NULL)
    {
        Rogue_Run0ChallengeStart(d);
        RUN0_CHALLENGE(d)->lent = lent;
    }
    Rogue_Run0GiveKit();
    FlagSet(FLAG_SYS_POKEMON_GET);
    // Its party is the starter row of no kind: its lead, as a CHAMPION's.
    VarSet(VAR_ROGUE_STARTER, ROGUE_STARTER_VALUE(ROGUE_STARTER_KIND_CHAMPION,
                                                  GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES)));
    Rogue_Run0ApplyAct(act);
    if (d != NULL)
    {
        // A checkpoint start: the band from where the party stands at this
        // act, and none of the party under its floor (the Hall of Fame's
        // levels are those of the win, not of this act).
        Rogue_Run0ChallengeRebase(d, (act - 1) * 2);
        for (i = 0; i < PARTY_SIZE; i++)
            Rogue_Run0RaiseToFloor(&gParties[B_TRAINER_PLAYER][i]);
        RaiseBoxesToFloor();
        if (act > d->bestAct)
            d->bestAct = act;
        RunLogPush(d);
    }
    Rogue_SaveDirector();
    // The story's attempt: the Previous Player's and PROF. BIRCH's teams.
    Rogue_Run0StoryNewAttempt();
}

// ---------------------------------------------------------------------------
// Battles, Pokemon lost, the end of a run, resets

static void KeepMon(struct RogueDirector *d, struct Pokemon *mon)
{
    struct RogueKeptMon *k;
    u8 name[POKEMON_NAME_LENGTH + 1];
    u16 species = GetMonData(mon, MON_DATA_SPECIES);
    u32 personality = GetMonData(mon, MON_DATA_PERSONALITY), i, n = KeptSlots(d);

    if (IsKept(d, species, personality))
        return;
    // One given back (or lent) and lost again takes its old place: the
    // Director keeps each Pokemon once (DATA COLLECTED counted JAWS twice).
    for (i = 0; i < n; i++)
        if (d->kept[i].species == species && d->kept[i].personality == personality)
            break;
    if (i < n)
        k = &d->kept[i];
    else if (d->keptCount >= ROGUE_KEPT_MAX)
        return;
    else
        k = &d->kept[d->keptCount++];
    k->species = species;
    k->personality = personality;
    k->level = GetMonData(mon, MON_DATA_LEVEL);
    k->flags = (Rogue_IsTerrysMon(mon) ? ROGUE_KEPT_TERRY : 0) | (IsMonShiny(mon) ? ROGUE_KEPT_SHINY : 0) | ROGUE_KEPT_NEW;
    GetMonData(mon, MON_DATA_NICKNAME, name);
    memcpy(k->nickname, name, POKEMON_NAME_LENGTH);
}

void Rogue_Run0OnFainted(struct Pokemon *mon)
{
    struct RogueDirector *d = Rogue_Director();
    struct RogueRunLogEntry *e;

    if (d == NULL)
        return;
    e = ThisRunEntry(d);
    if (e != NULL)
        LogLostMon(e, mon);
    if (Rogue_IsRun0())
    {
        KeepMon(d, mon);
        CountUp16(&d->faints);
    }
}

static struct Pokemon *PartyLead(void)
{
    u32 i;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        if (GetMonData(mon, MON_DATA_SANITY_HAS_SPECIES) && !GetMonData(mon, MON_DATA_IS_EGG) && GetMonData(mon, MON_DATA_HP) != 0)
            return mon;
    }
    return NULL;
}

// CB2_InitBattle, before the heap is reset: the stats, and the mark that a
// battle is on, written at once so turning the game off is noticed.
void Rogue_Run0OnBattleStart(void)
{
    struct RogueDirector *d;
    struct Pokemon *lead;
    u32 t1, t2;

    if (!Rogue_IsRun0() || (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED)) || Rogue_GetCurrentPlace() < 0)
    {
        Rogue_ReleaseDirector();
        return;
    }
    d = Rogue_Director();
    if (d == NULL)
        return;
    CountUp16(&d->battles);
    // What fainted in the last battle is no longer "just kept".
    for (t1 = 0; t1 < KeptSlots(d); t1++)
        d->kept[t1].flags &= ~ROGUE_KEPT_NEW;
    lead = PartyLead();
    if (lead != NULL)
    {
        u16 species = GetMonData(lead, MON_DATA_SPECIES);
        t1 = GetSpeciesType(species, 0);
        t2 = GetSpeciesType(species, 1);
        if (t1 < ROGUE_DIRECTOR_TYPES)
            CountUp16(&d->leadTypes[t1]);
        if (t2 != t1 && t2 < ROGUE_DIRECTOR_TYPES)
            CountUp16(&d->leadTypes[t2]);
    }
    Rogue_Run0ChallengeBattleStart(d, lead != NULL ? GetMonData(lead, MON_DATA_SPECIES) : SPECIES_NONE);
    // The kept Pokemon this trainer brings have been shown this act.
    if (gBattleTypeFlags & BATTLE_TYPE_TRAINER)
        Rogue_Run0NoteKeptShown(d, TRAINER_BATTLE_PARAM.opponentA);
    d->battleDirty = TRUE;
    // Who it was against, unless a reset's line is still to be said.
    if (!d->resetPending)
    {
        const u8 *name;
        if (gBattleTypeFlags & BATTLE_TYPE_TRAINER)
        {
            d->resetTrainer = TRAINER_BATTLE_PARAM.opponentA;
            d->resetSpecies = SPECIES_NONE;
            name = GetTrainerNameFromId(TRAINER_BATTLE_PARAM.opponentA);
        }
        else
        {
            d->resetTrainer = 0;
            d->resetSpecies = GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES);
            name = GetSpeciesName(d->resetSpecies);
        }
        StringCopyN(d->resetName, name, POKEMON_NAME_LENGTH);
        d->resetName[POKEMON_NAME_LENGTH] = EOS;
    }
    Rogue_SaveDirector();
}

// After Rogue_OnBattleEnd: the battle ended, so it was not cut short; and
// what fainted in it is written.
void Rogue_Run0OnBattleEnd(void)
{
    struct RogueDirector *d;

    if (Rogue_IsRun0() && !(gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED)))
    {
        d = Rogue_Director();
        if (d == NULL)
            return;
        d->battleDirty = FALSE;
        if (gBattleOutcome == B_OUTCOME_CAUGHT)
            CountUp16(&d->catches);
        Rogue_Run0ChallengeBattleEnd(d);
    }
    // A normal run writes only when a Pokemon was lost (logged).
    Rogue_SaveDirector();
}

// ---------------------------------------------------------------------------
// The keeping follows the save (struct RogueDirectorSaved)

static void NoteKeeping(const struct RogueDirector *d, struct RogueDirectorSaved *s)
{
    const struct RogueRun0Challenge *c = (const struct RogueRun0Challenge *)d->challenge;
    const struct RogueRunLogEntry *e = d->runLogCount ? &d->runLog[(d->runLogCount - 1) % ROGUE_RUNLOG_MAX] : NULL;
    u32 i;

    memset(s, 0, sizeof(*s));
    s->valid = TRUE;
    s->keptCount = d->keptCount;
    for (i = 0; i < d->keptCount && i < ROGUE_KEPT_MAX; i++)
        if (d->kept[i].flags & ROGUE_KEPT_RETURNED)
            s->returnedKept |= 1u << i;
    s->faints = d->faints;
    if (c->version == RUN0_CHALLENGE_VERSION)
    {
        s->keptShown = c->keptShown;
        s->keptAct = c->keptAct;
        s->keptNext = c->keptNext;
        s->returned = c->returned;
    }
    s->runLogCount = d->runLogCount;
    if (e != NULL)
    {
        s->lostCount = e->lostCount;
        for (i = 0; i < ROGUE_RUNLOG_LOST && e->lost[i].species != SPECIES_NONE; i++)
            s->lostFilled++;
    }
}

void Rogue_DirectorNoteSave(void)
{
    bool32 was = Rogue_IsDirectorLoaded();
    struct RogueDirector *d = Rogue_Director();
    struct RogueDirectorSaved now;

    if (d == NULL)
        return;
    NoteKeeping(d, &now);
    if (memcmp(&now, &d->saved, sizeof(now)) == 0)
    {
        if (!was)
            Rogue_ReleaseDirector();
        return;
    }
    d->saved = now;
    if (was)
        WriteDirector(d);
    else
        Rogue_SaveDirector();
}

// The keeping put back to what the save holds; TRUE if anything changed.
static bool32 KeepingBackToSave(struct RogueDirector *d)
{
    struct RogueRun0Challenge *c = RUN0_CHALLENGE(d);
    const struct RogueDirectorSaved *s = &d->saved;
    struct RogueRunLogEntry *e;
    struct RogueDirectorSaved now;
    u32 i;

    if (!s->valid)
        return FALSE;
    NoteKeeping(d, &now);
    if (memcmp(&now, s, sizeof(now)) == 0)
        return FALSE;
    // Kept since the save: let go. (One kept again after being given back
    // takes its old place, so it is given back again just below.)
    for (i = s->keptCount; i < d->keptCount && i < ROGUE_KEPT_MAX; i++)
        memset(&d->kept[i], 0, sizeof(d->kept[i]));
    if (s->keptCount < d->keptCount)
        d->keptCount = s->keptCount;
    for (i = 0; i < d->keptCount && i < ROGUE_KEPT_MAX; i++)
    {
        d->kept[i].flags &= ~(ROGUE_KEPT_RETURNED | ROGUE_KEPT_NEW);
        if (s->returnedKept & (1u << i))
            d->kept[i].flags |= ROGUE_KEPT_RETURNED;
    }
    d->faints = s->faints;
    if (c->version == RUN0_CHALLENGE_VERSION)
    {
        c->keptShown = s->keptShown;
        c->keptAct = s->keptAct;
        c->keptNext = s->keptNext;
        c->returned = s->returned;
    }
    // This run's Pokemon lost since the save: not lost.
    if (d->runLogCount == s->runLogCount && (e = NewestEntry(d)) != NULL)
    {
        e->lostCount = s->lostCount;
        for (i = s->lostFilled; i < ROGUE_RUNLOG_LOST; i++)
        {
            memset(&e->lost[i], 0, sizeof(e->lost[i]));
            e->lost[i].nickname[0] = EOS;
        }
    }
    return TRUE;
}

// At the main menu: the keeping goes back to the save (the game was turned
// off without saving: a Pokemon lost since is alive in the save, so the
// Director does not keep it), and a RUN 0 battle that never ended means the
// game was turned off in it.
void Rogue_Run0CheckReset(void)
{
    struct RogueDirector *d = Rogue_Director();
    bool32 changed;

    if (d == NULL)
        return;
    changed = KeepingBackToSave(d);
    if (!d->battleDirty)
    {
        if (changed)
            Rogue_SaveDirector();
        else
            Rogue_ReleaseDirector();
        return;
    }
    d->battleDirty = FALSE;
    CountUp16(&d->resets);
    d->resetPending = TRUE;
    Rogue_SaveDirector();
}

u32 Rogue_Run0TopLeadType(u32 *percent)
{
    struct RogueDirector *d = Rogue_Director();
    u32 t, best = TYPE_NORMAL, most = 0;

    *percent = 0;
    if (d == NULL)
        return TYPE_NORMAL;
    for (t = 0; t < NUMBER_OF_MON_TYPES; t++)
    {
        if (d->leadTypes[t] > most)
        {
            most = d->leadTypes[t];
            best = t;
        }
    }
    if (d->battles != 0)
        *percent = (most * 100 + d->battles / 2) / d->battles;
    if (*percent > 100)
        *percent = 100;
    return best;
}

// The other type of the type led with most, when a dual-type lead made both
// (each battle counts both its lead's types): as many battles, or nearly.
static u32 SecondLeadType(const struct RogueDirector *d, u32 first)
{
    u32 t, best = TYPE_NONE, most = 0;
    for (t = 0; t < NUMBER_OF_MON_TYPES && t < ROGUE_DIRECTOR_TYPES; t++)
    {
        if (t != first && d->leadTypes[t] > most)
        {
            most = d->leadTypes[t];
            best = t;
        }
    }
    if (best == TYPE_NONE || most * 10 < d->leadTypes[first] * 9)
        return TYPE_NONE;
    return best;
}

// The type in capitals, as Gen 3 writes them (WATER).
static u8 *CopyTypeName(u8 *dest, u32 type)
{
    u8 *name = dest;
    dest = StringCopy(dest, gTypesInfo[type].name);
    for (; name < dest; name++)
        if (*name >= CHAR_a && *name <= CHAR_z)
            *name += CHAR_A - CHAR_a;
    return dest;
}

void Rogue_Run0BufferDataCollected(u8 *dest)
{
    struct RogueDirector *d = Rogue_Director();
    u32 percent, type = Rogue_Run0TopLeadType(&percent), second, kept;

    dest = StringCopy(dest, COMPOUND_STRING("{COLOR BLUE}DATA COLLECTED{COLOR DARK_GRAY}\n\n"));
    if (percent == 0)
    {
        dest = StringCopy(dest, COMPOUND_STRING("You didn't fight anyone.\nThat's data too."));
    }
    else if (d != NULL && (second = SecondLeadType(d, type)) != TYPE_NONE)
    {
        // Both types of a dual-type lead: "You lead with NORMAL and FLYING".
        dest = StringCopy(dest, COMPOUND_STRING("You lead with "));
        dest = CopyTypeName(dest, type);
        dest = StringCopy(dest, COMPOUND_STRING("\nand "));
        dest = CopyTypeName(dest, second);
        *dest++ = CHAR_SPACE;
        dest = ConvertIntToDecimalStringN(dest, percent, STR_CONV_MODE_LEFT_ALIGN, 3);
        dest = StringCopy(dest, COMPOUND_STRING("% of the time.\nNext time I'll know."));
    }
    else
    {
        dest = StringCopy(dest, COMPOUND_STRING("You lead with "));
        dest = CopyTypeName(dest, type);
        *dest++ = CHAR_SPACE;
        dest = ConvertIntToDecimalStringN(dest, percent, STR_CONV_MODE_LEFT_ALIGN, 3);
        dest = StringCopy(dest, COMPOUND_STRING("% of\nthe time. Next time I'll know."));
    }
    if (d != NULL && (kept = DistinctKept(d)) != 0)
    {
        dest = StringCopy(dest, COMPOUND_STRING("\n\nI kept "));
        dest = ConvertIntToDecimalStringN(dest, kept, STR_CONV_MODE_LEFT_ALIGN, 2);
        dest = StringCopy(dest, kept == 1 ? COMPOUND_STRING(" of yours.") : COMPOUND_STRING(" of yours. They're safe."));
    }
}

void Rogue_Run0OnRunEnd(bool32 won)
{
    struct RogueDirector *d = Rogue_Director();
    u32 act = VarGet(VAR_ROGUE_RUN0_ACT), i;

    if (d == NULL)
        return;
    if (act > ROGUE_RUN0_ACT_ENDING)
        act = 1;
    if (act > d->bestAct)
        d->bestAct = act;
    d->battleDirty = FALSE;
    Rogue_Run0ChallengeRunEnd(d, won);
    if (won)
    {
        d->feat = TRUE;
        d->offerCheckpoint = FALSE;
        d->checkpointAct = 0;
        return;
    }
    // The team that went down: the Director keeps them too.
    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        if (GetMonData(mon, MON_DATA_SANITY_HAS_SPECIES) && !GetMonData(mon, MON_DATA_IS_EGG))
            KeepMon(d, mon);
    }
    if (act > ROGUE_RUN0_ACTS)
        act = ROGUE_RUN0_ACTS;
    d->checkpointAct = act;
    d->offerCheckpoint = act > 1;
}

// ---------------------------------------------------------------------------
// The teasers

bool32 Rogue_TeaseHofLevel(void)
{
    struct RogueDirector *d;
    if (Rogue_GetRunKind() != ROGUE_RUN_KIND_NORMAL || VarGet(VAR_ROGUE_META_WINS) != 1)
        return FALSE;
    d = Rogue_Director();
    if (d == NULL || (d->teasers & ROGUE_TEASE_HOF_LEVEL))
        return FALSE;
    // Kept loaded: the run summary writes it after the Hall of Fame's save.
    d->teasers |= ROGUE_TEASE_HOF_LEVEL;
    return TRUE;
}

bool32 Rogue_TeaseTownLine(u8 *dest)
{
    struct RogueDirector *d;
    u8 *s;
    if (!FlagGet(FLAG_ROGUE_TEASE_TOWN) || FlagGet(FLAG_ROGUE_TEASE_TOLD))
        return FALSE;
    d = Rogue_Director();
    if (d == NULL || d->lastTownName[0] == EOS)
    {
        Rogue_ReleaseDirector();
        return FALSE;
    }
    s = StringCopy(dest, COMPOUND_STRING("Have you been to\n"));
    s = StringCopy(s, d->lastTownName);
    StringCopy(s, COMPOUND_STRING("?\p...No. That was somewhere else."));
    Rogue_ReleaseDirector();
    FlagSet(FLAG_ROGUE_TEASE_TOLD);
    return TRUE;
}

bool32 Rogue_TeaseGiven(void)
{
    struct RogueDirector *d;
    if (VarGet(VAR_ROGUE_META_WINS) != 3)
        return FALSE;
    d = Rogue_Director();
    if (d == NULL)
        return FALSE;
    if (d->teasers & ROGUE_TEASE_GIVEN)
    {
        Rogue_ReleaseDirector();
        return FALSE;
    }
    d->teasers |= ROGUE_TEASE_GIVEN;
    Rogue_SaveDirector();
    return TRUE;
}


// ---------------------------------------------------------------------------
// ROGUE_RUN0_READY: a fresh save with a history (make ROGUE_RUN0_READY=1).
// Four wins with their Hall of Fame teams from four real regions, nineteen
// runs, the losses between, a fallen friend and the run log, so the RUN 0
// row shows at once. Only in that build.

#ifdef ROGUE_RUN0_READY

struct ReadyMon
{
    u16 species;
    u8 level;
    const u8 *nickname; // NULL: the species name
};

struct ReadyRun
{
    u16 seed;
    u8 badges;              // ROGUE_NUM_GYMS for a win
    u8 team;                // a win's Hall of Fame team (sReadyTeams), or 0xFF
    struct ReadyMon lost[3]; // Pokemon lost on it
};

static const struct ReadyMon sReadyTeams[ROGUE_RUN0_WINS][PARTY_SIZE] =
{
    { // JOSADOR, the first win
        { SPECIES_SCEPTILE,  54, COMPOUND_STRING("LEAFY") },
        { SPECIES_PELIPPER,  50, NULL },
        { SPECIES_MANECTRIC, 51, COMPOUND_STRING("ZAPPY") },
        { SPECIES_BRELOOM,   52, NULL },
        { SPECIES_SWELLOW,   50, COMPOUND_STRING("SKY") },
        { SPECIES_GOLEM,     51, NULL },
    },
    { // LARULO
        { SPECIES_BLAZIKEN,  55, COMPOUND_STRING("BLAZE") },
        { SPECIES_GYARADOS,  53, COMPOUND_STRING("NESSIE") },
        { SPECIES_GARDEVOIR, 52, NULL },
        { SPECIES_FLYGON,    54, NULL },
        { SPECIES_AGGRON,    52, COMPOUND_STRING("TANK") },
        { SPECIES_RAICHU,    51, NULL },
    },
    { // NOMOGAR
        { SPECIES_SWAMPERT,  57, COMPOUND_STRING("MUDDY") },
        { SPECIES_STARAPTOR, 55, NULL },
        { SPECIES_LUXRAY,    54, COMPOUND_STRING("SPARKY") },
        { SPECIES_LUCARIO,   55, NULL },
        { SPECIES_ROSERADE,  54, NULL },
        { SPECIES_ARCANINE,  56, COMPOUND_STRING("REX") },
    },
    { // MARISEI, the latest win: RUN 0's party
        { SPECIES_TYPHLOSION, 60, COMPOUND_STRING("OTTO") },
        { SPECIES_TOGEKISS,   58, NULL },
        { SPECIES_GARCHOMP,   59, COMPOUND_STRING("JAWS") },
        { SPECIES_AZUMARILL,  57, NULL },
        { SPECIES_MAGNEZONE,  57, NULL },
        { SPECIES_TYRANITAR,  58, COMPOUND_STRING("ROCKY") },
    },
};

#define READY_RUNS 19
static const struct ReadyRun sReadyRuns[READY_RUNS] =
{
    {   233, 1, 0xFF, { { SPECIES_ZIGZAGOON, 6, COMPOUND_STRING("ZIGGY") } } },
    {   517, 2, 0xFF, { { SPECIES_COMBUSKEN, 19, COMPOUND_STRING("CHICKEN") }, { SPECIES_TAILLOW, 15, NULL } } },
    {  1201, 8, 0,    { { SPECIES_WINGULL, 22, NULL } } },
    {   902, 3, 0xFF, { { SPECIES_MARSHTOMP, 24, COMPOUND_STRING("MUD") }, { SPECIES_SHROOMISH, 20, NULL }, { SPECIES_POOCHYENA, 18, NULL } } },
    {  1450, 0, 0xFF, { { SPECIES_TREECKO, 8, NULL } } },
    {  2088, 4, 0xFF, { { SPECIES_GROVYLE, 30, COMPOUND_STRING("SPROUT") }, { SPECIES_MARILL, 27, NULL } } },
    {  8610, 8, 1,    { { SPECIES_ELECTRIKE, 25, COMPOUND_STRING("ZIP") } } },
    {  4096, 2, 0xFF, { { SPECIES_MUDKIP, 14, COMPOUND_STRING("KIPPER") }, { SPECIES_LOTAD, 12, NULL } } },
    {  4620, 5, 0xFF, { { SPECIES_MIGHTYENA, 36, COMPOUND_STRING("SHADOW") }, { SPECIES_KIRLIA, 33, NULL } } },
    {  5150, 1, 0xFF, { { SPECIES_TORCHIC, 9, COMPOUND_STRING("NUGGET") } } },
    {  6311, 6, 0xFF, { { SPECIES_HARIYAMA, 41, NULL }, { SPECIES_PIDGEOTTO, 38, COMPOUND_STRING("ACE") }, { SPECIES_WARTORTLE, 40, NULL } } },
    { 12045, 8, 2,    { { SPECIES_STARAVIA, 31, NULL } } },
    {  7022, 3, 0xFF, { { SPECIES_MONFERNO, 26, COMPOUND_STRING("MONTY") } } },
    {  9101, 7, 0xFF, { { SPECIES_DRAGONAIR, 47, COMPOUND_STRING("NOODLE") }, { SPECIES_SANDSLASH, 44, NULL } } },
    { 10240, 2, 0xFF, { { SPECIES_CYNDAQUIL, 13, NULL }, { SPECIES_HOOTHOOT, 11, COMPOUND_STRING("OWLBERT") } } },
    {  2731, 8, 3,    { { SPECIES_FURRET, 30, COMPOUND_STRING("NOODLES") } } },
    { 11111, 4, 0xFF, { { SPECIES_QUILAVA, 29, COMPOUND_STRING("TOAST") }, { SPECIES_NOCTOWL, 28, NULL } } },
    { 13007, 5, 0xFF, { { SPECIES_CROCONAW, 35, COMPOUND_STRING("CHOMPY") }, { SPECIES_PSYDUCK, 30, NULL } } },
    // The last run, lost with five badges: its fallen friend is BISCUIT.
    { 15321, 5, 0xFF, { { SPECIES_LINOONE, 41, COMPOUND_STRING("BISCUIT") }, { SPECIES_ALTARIA, 40, NULL }, { SPECIES_MACHOKE, 39, NULL } } },
};

static void SetName(u8 *dest, u16 species, const u8 *nickname)
{
    u8 name[POKEMON_NAME_LENGTH + 1];
    StringCopy(name, nickname != NULL ? nickname : GetSpeciesName(species));
    memcpy(dest, name, POKEMON_NAME_LENGTH);
}

static void MarkCaught(struct RogueRecord *record, u16 species)
{
    u32 dex = SpeciesToNationalPokedexNum(Rogue_FirstForm(species));
    if (dex != 0 && dex < ROGUE_DEX_BYTES * 8)
        record->caught[dex / 8] |= 1 << (dex % 8);
}

void Rogue_SeedReadyHistory(void)
{
    static const u8 sReadyPlayer[] = _("BRENDAN");
    struct HallofFameTeam *hof;
    struct RogueRecord *record;
    struct RogueDirector *d;
    u32 i, j, trainerId, wins = 0;

    // A save as a new game leaves it, with a name.
    Sav2_ClearSetDefault();
    StringCopy(gSaveBlock2Ptr->playerName, sReadyPlayer);
    gSaveBlock2Ptr->playerGender = MALE;
    trainerId = Random32();
    SetTrainerId(trainerId, gSaveBlock2Ptr->playerTrainerId);

    // The record: nineteen runs, four wins, fifteen losses.
    VarSet(VAR_ROGUE_META_RUNS, READY_RUNS);
    VarSet(VAR_ROGUE_META_WINS, ROGUE_RUN0_WINS);
    VarSet(VAR_ROGUE_PROGRESS, ROGUE_PROGRESS_CHAMPION);
    record = Rogue_GetRecord();
    record->losses = READY_RUNS - ROGUE_RUN0_WINS;
    record->caves = 41;
    record->catches = 163;
    record->shinies = 1;
    record->rivalWins = 27;
    record->dungeons = 14;
    record->typeBadges[ROGUE_STARTER_GRASS] = 8;
    record->typeBadges[ROGUE_STARTER_FIRE] = 8;
    record->typeBadges[ROGUE_STARTER_WATER] = 8;
    record->typeBadgesAll[TYPE_GRASS] = 8;
    record->typeBadgesAll[TYPE_FIRE] = 8;
    record->typeBadgesAll[TYPE_WATER] = 8;
    record->feats |= 1u << FEAT_CHAMPION;
    record->fallenSpecies = SPECIES_LINOONE;
    record->fallenLevel = 41;
    StringCopy(record->fallenName, COMPOUND_STRING("BISCUIT"));
    record->lastBadges = 5;
    record->lastPlace = 0xFF;
    Rogue_UpdateRecord();
    record->announced = record->unlocks;
    record->announcedHi = record->unlocksHi;
    record->runStart = record->unlocks;
    record->runStartHi = record->unlocksHi;
    record->featsStart = record->feats;
    // The last run: PANINOVA, lost with five badges.
    VarSet(VAR_ROGUE_PROGRESS, ROGUE_PROGRESS_BADGE_1 + 4);
    VarSet(VAR_ROGUE_SEED_LO, sReadyRuns[READY_RUNS - 1].seed);
    VarSet(VAR_ROGUE_SEED_HI, 0);
    VarSet(VAR_ROGUE_DEX_MIX, ROGUE_MIX_CLASSIC);
    VarSet(VAR_ROGUE_FALLEN, 6);
    VarSet(VAR_ROGUE_RUN_STATE, ROGUE_RUN_LOST);
    VarSet(VAR_ROGUE_META_CHAMPION_MON, sReadyTeams[ROGUE_RUN0_WINS - 1][0].species);
    FlagSet(FLAG_SYS_GAME_CLEAR);

    // The Hall of Fame: the four teams, oldest first.
    hof = AllocZeroed(SECTOR_DATA_SIZE * NUM_HOF_SECTORS);
    for (i = 0; i < ROGUE_RUN0_WINS && hof != NULL; i++)
    {
        for (j = 0; j < PARTY_SIZE; j++)
        {
            const struct ReadyMon *r = &sReadyTeams[i][j];
            struct HallofFameMon *m = &hof[i].mon[j];
            m->species = r->species;
            m->lvl = r->level;
            m->tid = trainerId;
            m->personality = RogueRng_Hash(trainerId, 0x700 + i * PARTY_SIZE + j);
            m->isShiny = (i == 2 && j == 4); // one shiny, as a long history has
            SetName(m->nickname, r->species, r->nickname);
            MarkCaught(record, r->species);
        }
    }

    // The Director: the run log of all nineteen, the teasers already seen.
    d = Rogue_Director();
    if (d != NULL)
    {
        DirectorFresh(d);
        d->teasers = ROGUE_TEASE_HOF_LEVEL | ROGUE_TEASE_TOWN | ROGUE_TEASE_GIVEN;
        for (i = 0; i < READY_RUNS; i++)
        {
            const struct ReadyRun *run = &sReadyRuns[i];
            struct RogueRunLogEntry *e = &d->runLog[i % ROGUE_RUNLOG_MAX];
            memset(e, 0, sizeof(*e));
            e->seed = run->seed;
            e->run = i + 1;
            e->result = run->team != 0xFF ? ROGUE_LOG_WON : ROGUE_LOG_LOST;
            e->badges = run->badges;
            e->place = 0xFF;
            e->kind = ROGUE_RUN_KIND_NORMAL;
            e->mix = ROGUE_MIX_CLASSIC;
            for (j = 0; j < ARRAY_COUNT(run->lost) && run->lost[j].species != SPECIES_NONE; j++)
            {
                e->lost[j].species = run->lost[j].species;
                e->lost[j].level = run->lost[j].level;
                SetName(e->lost[j].nickname, run->lost[j].species, run->lost[j].nickname);
                e->lostCount++;
                MarkCaught(record, run->lost[j].species);
            }
            // A loss took the rest of the team with it.
            if (e->result == ROGUE_LOG_LOST)
                e->lostCount += 2 + (i % 3);
            wins += e->result == ROGUE_LOG_WON;
        }
        d->runLogCount = READY_RUNS;
    }

    // Written as a first save, then the Hall of Fame with it, then the
    // Director after.
    if (hof != NULL)
    {
        gHoFSaveBuffer = hof;
        TrySavingData(SAVE_HALL_OF_FAME);
        gHoFSaveBuffer = NULL;
        Free(hof);
    }
    else
    {
        TrySavingData(SAVE_NORMAL);
    }
    Rogue_SaveDirector();
    gSaveFileStatus = SAVE_STATUS_OK;
    (void)wins;
}

#endif // ROGUE_RUN0_READY
