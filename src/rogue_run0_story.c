// RUN 0's story (Roguémon v1.0). See include/rogue_run0_story.h for the
// hooks, data/scripts/rogue_run0.inc for the scenes, and
// src/rogue_text_director.h for every word. The design is the approved RUN 0
// page and the Beta v1.0 brief: the Director's region, TERRY (who made the
// Director, or was made by it), the eight custom leaders, the Elite Four of
// the player's own history, the Director's battle and three endings.
//
// Nothing here writes the save in a way a normal save does not: the
// Director's sector is written after a save or at a scene's end, never
// during one, and the fake crash and the fake corrupted save touch nothing.

#include "global.h"
#include "rogue.h"
#include "rogue_gen.h"
#include "rogue_run0_story.h"
#include "battle.h"
#include "battle_setup.h"
#include "field_camera.h"
#include "gpu_regs.h"
#include "script_pokemon_util.h"
#include "constants/battle_ai.h"
#include "constants/rgb.h"
#include "data.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "event_scripts.h"
#include "field_screen_effect.h"
#include "fieldmap.h"
#include "hall_of_fame.h"
#include "item.h"
#include "load_save.h"
#include "m4a.h"
#include "main.h"
#include "menu.h"
#include "script_menu.h"
#include "credits.h"
#include "title_screen.h"
#include "intro.h"
#include "malloc.h"
#include "pokedex.h"
#include "metatile_behavior.h"
#include "overworld.h"
#include "palette.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "main_menu.h"
#include "random.h"
#include "rtc.h"
#include "save.h"
#include "script.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "constants/battle.h"
#include "constants/event_objects.h"
#include "constants/items.h"
#include "constants/map_groups.h"
#include "constants/songs.h"
#include "constants/species.h"
#include "constants/trainers.h"

#include "rogue_text_director.h"

#define ROGUE_GROUP MAP_GROUP(MAP_ROGUE_PLACE_0)
#define TERRY_OT_ID 0x07D10517 // TEST TEAM's OT, as src/rogue_run0.c gives it

STATIC_ASSERT(sizeof(struct RogueRun0Story) <= ROGUE_DIRECTOR_STORY_BYTES, Run0StoryFits);
STATIC_ASSERT(RUN0_BEAT_VICTORY < 64, Run0BeatsFit);

extern const u8 Rogue_EventScript_Run0Scene[];
extern const u8 Rogue_EventScript_Run0Crash[];
extern const u8 Rogue_EventScript_Run0Switch[];
extern const u8 Rogue_EventScript_Run0EndlessArrive[];
extern const u8 Rogue_EventScript_Run0EndlessGate[];
extern const u8 Rogue_EventScript_Run0ReadThing[];
extern const u8 Rogue_EventScript_Run0Believer[];
extern const u8 Rogue_EventScript_Run0Terry[];
extern const u8 Rogue_EventScript_Run0TestQuestion[];

// The test hook (tools/roguemon/run0test.py): a scene to run as if due.
u8 gRogueTestStory;
// The fake crash holds the sound on one frame (src/main.c skips the mixer).
bool8 gRogueSoundStuck;
// A scene waiting for the field to be free (RUN0_SCENE_*), and the boot's
// first frame.
static u8 sPendingScene;
static bool8 sBootChecked;
// The fake boot: the title and the menu with only RUN 0 on it.
static bool8 sFakeBoot; // RUN0_FAKE_BOOT_*
#define RUN0_FAKE_BOOT_CRASH 1
#define RUN0_FAKE_BOOT_SAVE  2
static bool8 sForceDraft; // the test hook's DRAFT
static u8 sDepthCache; // ENDLESS's region number, read once a run (0: not yet; Rogue_Run0EndlessDepth)

// ---------------------------------------------------------------------------
// The story's save

static void StoryFresh(struct RogueRun0Story *s)
{
    memset(s, 0, sizeof(*s));
    s->version = RUN0_STORY_VERSION;
}

struct RogueRun0Story *Rogue_Run0Story(struct RogueDirector *d)
{
    struct RogueRun0Story *s;
    if (d == NULL)
        return NULL;
    s = (struct RogueRun0Story *)d->story;
    if (s->version != RUN0_STORY_VERSION)
        StoryFresh(s);
    return s;
}

static bool32 BeatUsed(struct RogueDirector *d, u32 beat)
{
    return d != NULL && (d->beats[beat / 32] & (1u << (beat % 32)));
}

static void UseBeat(struct RogueDirector *d, u32 beat)
{
    if (d != NULL)
        d->beats[beat / 32] |= 1u << (beat % 32);
}

// ---------------------------------------------------------------------------
// Facts and lines

// Names in ASCII for a line, kept while it is filled.
struct FactBufs
{
    char player[PLAYER_NAME_LENGTH + 2];
    char name[POKEMON_NAME_LENGTH + 2];
    char nick[POKEMON_NAME_LENGTH + 2];
    char mon[POKEMON_NAME_LENGTH + 2];
    char region[ROGUE_REGION_NAME_LEN + 2];
    char time[8];
};

static struct FactBufs *sBufs; // on the heap while a line is made

static void BaseFacts(struct RogueDirFacts *f, struct FactBufs *b)
{
    memset(f, 0, sizeof(*f));
    Rogue_GameToAscii(b->player, gSaveBlock2Ptr->playerName, PLAYER_NAME_LENGTH);
    f->player = b->player;
    f->runs = VarGet(VAR_ROGUE_META_RUNS);
    f->wins = VarGet(VAR_ROGUE_META_WINS);
}

static void SetNick(struct RogueDirFacts *f, struct FactBufs *b, const u8 *name)
{
    Rogue_GameToAscii(b->nick, name, POKEMON_NAME_LENGTH);
    f->nick = b->nick;
}

static void SetMon(struct RogueDirFacts *f, struct FactBufs *b, u16 species)
{
    Rogue_GameToAscii(b->mon, GetSpeciesName(species), POKEMON_NAME_LENGTH);
    f->mon = b->mon;
}

static void SetRegion(struct RogueDirFacts *f, struct FactBufs *b, u32 seed)
{
    RogueGen_RegionName(seed, b->region);
    f->region = b->region;
}

static void SetName(struct RogueDirFacts *f, struct FactBufs *b, const u8 *name)
{
    Rogue_GameToAscii(b->name, name, TRAINER_NAME_LENGTH);
    f->name = b->name;
}

// Line id filled into dest (the game's characters). FALSE if a slot had no
// value (dest is then empty).
static bool32 SayInto(u8 *dest, u32 size, u32 id, const struct RogueDirFacts *f)
{
    u32 n;
    if (id >= DIR_COUNT || sDirLines[id] == NULL)
    {
        *dest = EOS;
        return FALSE;
    }
    // Made in ASCII where it lies, then turned into the game's characters
    // (one character each).
    n = RogueDir_Say((char *)dest, size, sDirLines[id], f);
    if (n == 0)
    {
        *dest = EOS;
        return FALSE;
    }
    Rogue_CopyLoreAscii(dest, (const char *)dest);
    return TRUE;
}

// Appends a line, as a new box, to what dest holds.
static void SayMore(u8 *dest, u32 size, u32 id, const struct RogueDirFacts *f)
{
    u32 len = StringLength(dest);
    if (len + 4 >= size)
        return;
    if (len != 0)
    {
        dest[len++] = CHAR_PROMPT_CLEAR;
        dest[len] = EOS;
    }
    if (!SayInto(dest + len, size - len, id, f) && len != 0)
        dest[len - 1] = EOS;
}

static bool32 Say(u32 id, const struct RogueDirFacts *f)
{
    return SayInto(gStringVar4, sizeof(gStringVar4), id, f);
}

static struct FactBufs *Bufs(void)
{
    if (sBufs == NULL)
        sBufs = AllocZeroed(sizeof(struct FactBufs));
    return sBufs;
}

static void FreeBufs(void)
{
    TRY_FREE_AND_SET_NULL(sBufs);
}

// Script special: line VAR_0x8004 with the save's basic facts into gStringVar4.
void Rogue_Run0Say(void)
{
    struct RogueDirFacts f;
    struct FactBufs *b = Bufs();
    if (b == NULL)
        return;
    BaseFacts(&f, b);
    f.n = gSpecialVar_0x8005;
    Say(gSpecialVar_0x8004, &f);
    FreeBufs();
}

// ---------------------------------------------------------------------------
// The run log, read for the people's lines

// The k-th newest run of the log that is not RUN 0 and matches result
// (0xFF any), or NULL.
static const struct RogueRunLogEntry *PastRun(const struct RogueDirector *d, u32 k, u32 result)
{
    const struct RogueRunLogEntry *e;
    u32 i;
    for (i = 0; (e = Rogue_RunLogEntry(d, i)) != NULL; i++)
    {
        if (e->kind == ROGUE_RUN_KIND_RUN0 || e->result == ROGUE_LOG_ACTIVE)
            continue;
        if (result != 0xFF && e->result != result)
            continue;
        if (k-- == 0)
            return e;
    }
    return NULL;
}

// The oldest run of the log (not RUN 0), or NULL.
static const struct RogueRunLogEntry *FirstRun(const struct RogueDirector *d)
{
    const struct RogueRunLogEntry *e, *first = NULL;
    u32 i;
    for (i = 0; (e = Rogue_RunLogEntry(d, i)) != NULL; i++)
        if (e->kind != ROGUE_RUN_KIND_RUN0)
            first = e;
    return first;
}

// ---------------------------------------------------------------------------
// A new attempt, a new run

void Rogue_RegionStarters(u32 seed, u16 *out);    // src/rogue.c
u16 Rogue_EvolveTo(u16 species, u32 level);       // src/rogue.c
void Rogue_GameClear(void);                       // src/rogue_run.c
void Rogue_InvalidateTrainer(void);               // src/rogue.c

void Rogue_Run0StoryNewAttempt(void)
{
    struct RogueDirector *d = Rogue_Director();
    struct RogueRun0Story *s = Rogue_Run0Story(d);
    struct HallofFameTeam *teams;
    const struct RogueRunLogEntry *first;
    u32 i;

    if (s == NULL)
        return;
    s->patchBadges = 0;
    s->patchesShown = 0;
    s->slips = 0;
    s->drafts = 0;
    s->truckDone = 0;
    s->cameHome = 0;
    // The Previous Player: the first HALL OF FAME team (the oldest kept).
    memset(s->prevSpecies, 0, sizeof(s->prevSpecies));
    teams = Alloc(SECTOR_DATA_SIZE * NUM_HOF_SECTORS);
    if (teams != NULL)
    {
        gHoFSaveBuffer = teams;
        if (LoadGameSave(SAVE_HALL_OF_FAME) == SAVE_STATUS_OK)
        {
            for (i = 0; i < PARTY_SIZE; i++)
            {
                s->prevSpecies[i] = teams[0].mon[i].species;
                s->prevLevel[i] = teams[0].mon[i].lvl;
            }
        }
        gHoFSaveBuffer = NULL;
        Free(teams);
    }
    // No win to remember: the Previous Player brings what the first run lost.
    first = FirstRun(d);
    if (s->prevSpecies[0] == SPECIES_NONE && first != NULL)
    {
        for (i = 0; i < ROGUE_RUNLOG_LOST && i < PARTY_SIZE; i++)
        {
            s->prevSpecies[i] = first->lost[i].species;
            s->prevLevel[i] = first->lost[i].level;
        }
    }
    // PROF. BIRCH's three: the first run's starters (the oldest in the run
    // log), else Emerald's.
    if (first != NULL)
    {
        Rogue_RegionStarters(first->seed, s->birchStarters);
    }
    else
    {
        s->birchStarters[0] = SPECIES_TREECKO;
        s->birchStarters[1] = SPECIES_TORCHIC;
        s->birchStarters[2] = SPECIES_MUDKIP;
    }
    // Deleted in an attempt before: he is back, and does not know it, until
    // the attempt reaches act 4 again.
    if (s->terryDeleted && VarGet(VAR_ROGUE_RUN0_ACT) >= ROGUE_RUN0_ACTS)
        FlagSet(FLAG_ROGUE_RUN0_TERRY_DELETED);
    Rogue_SaveDirector();
}

// ---------------------------------------------------------------------------
// The lab

static u32 NormalRuns(const struct RogueDirector *d)
{
    u32 runs = VarGet(VAR_ROGUE_META_RUNS), attempts = d != NULL ? d->attempts : 1;
    return runs > attempts ? runs - attempts : 0;
}

// Script special: what Birch says, into gStringVar4. VAR_RESULT is TRUE
// when he puts the player back at the act's town after it.
void Rogue_Run0BirchTalk(void)
{
    struct RogueDirector *d = Rogue_Director();
    struct RogueDirFacts f;
    struct FactBufs *b = Bufs();
    u32 act = VarGet(VAR_ROGUE_RUN0_ACT);

    gSpecialVar_Result = FALSE;
    if (b == NULL)
        return;
    BaseFacts(&f, b);
    if (act >= 2 && act <= ROGUE_RUN0_ACTS && !FlagGet(RUN0_FLAG_CHECKPOINT))
    {
        Say(DIR_BIRCH_CHECKPOINT, &f);
        gSpecialVar_Result = TRUE;
    }
    else if (!FlagGet(RUN0_FLAG_BIRCH_TALKED))
    {
        FlagSet(RUN0_FLAG_BIRCH_TALKED);
        f.n = NormalRuns(d);
        Say(d != NULL && d->attempts > 1 ? DIR_BIRCH_LATER : DIR_BIRCH_FIRST, &f);
    }
    else
    {
        Say(DIR_BIRCH_AGAIN, &f);
    }
    Rogue_ReleaseDirector();
    FreeBufs();
}

static void WarpNow(void)
{
    LockPlayerFieldControls();
    DoWarp();
    ResetInitialPlayerAvatarState();
}

// Script special: the checkpoint's town, the last gym town before the act.
void Rogue_Run0PlaceCheckpoint(void)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    u32 act = VarGet(VAR_ROGUE_RUN0_ACT), gym;
    FlagSet(RUN0_FLAG_CHECKPOINT);
    gym = act >= 2 ? (act - 1) * 2 - 1 : 0;
    if (gym >= ROGUE_NUM_GYMS)
        gym = ROGUE_NUM_GYMS - 1;
    Rogue_SetFlyWarp(r->gymTown[gym]);
    VarSet(VAR_ROGUE_TOWN, r->gymTown[gym]);
    WarpNow();
}

// ---------------------------------------------------------------------------
// Townsfolk who slip, the leaders, the rival

bool32 Rogue_Run0TownSlip(u32 place, u8 *dest)
{
    struct RogueDirector *d;
    struct RogueRun0Story *s;
    const struct RogueRunLogEntry *e;
    struct RogueDirFacts f;
    struct FactBufs *b;
    u32 line, k;
    bool32 ok = FALSE;

    if (!Rogue_IsRun0() || VarGet(VAR_ROGUE_RUN0_ACT) > 2 || place >= 32)
        return FALSE;
    d = Rogue_Director();
    s = Rogue_Run0Story(d);
    if (s == NULL || (s->slips & (1u << place)) || (b = Bufs()) == NULL)
    {
        Rogue_ReleaseDirector();
        return FALSE;
    }
    // Each town the next line, so no two towns say the same thing (a
    // town's place number picked them, and two towns shared "My cousin
    // says..."); once all five are said, nobody else slips.
    for (line = 0, k = s->slips; k != 0; k &= k - 1)
        line++;
    if (line >= DIR_SLIPS)
    {
        Rogue_ReleaseDirector();
        FreeBufs();
        return FALSE;
    }
    e = PastRun(d, place % 3, line == 1 ? ROGUE_LOG_WON : line == 2 ? ROGUE_LOG_LOST : 0xFF);
    if (e == NULL)
        e = PastRun(d, 0, line == 1 ? ROGUE_LOG_WON : line == 2 ? ROGUE_LOG_LOST : 0xFF);
    if (e == NULL)
    {
        line = 3;
        e = PastRun(d, 0, 0xFF);
    }
    if (e != NULL)
    {
        BaseFacts(&f, b);
        SetRegion(&f, b, e->seed);
        ok = SayInto(dest, 600, DIR_SLIP_0 + line, &f);
    }
    if (ok)
    {
        s->slips |= 1u << place;
        Rogue_SaveDirector();
    }
    else
    {
        Rogue_ReleaseDirector();
    }
    FreeBufs();
    return ok;
}

bool32 Rogue_Run0LeaderLine(u32 leader, u32 field, u8 *dest, u32 size)
{
    struct RogueDirFacts f;
    struct FactBufs *b;
    u32 k;
    bool32 ok;
    if (!Rogue_IsRun0() || leader >= DIR_LEADERS)
        return FALSE;
    k = field == RLEADER_INTRO || field == RLEADER_E4_INTRO ? 0 : field == RLEADER_DEFEAT ? 1 : 2;
    if ((b = Bufs()) == NULL)
        return FALSE;
    BaseFacts(&f, b);
    ok = SayInto(dest, size, DIR_LEADER_0 + leader * 3 + k, &f);
    FreeBufs();
    return ok;
}

// The Director gives back a Pokemon it kept, after a badge (the challenge's
// Rogue_Run0ReturnKept picks it): its words into dest, the badges picking
// one of three lines, and where it went if the party was full.
u8 *Rogue_Run0ReturnLine(u8 *dest, const u8 *nickname, bool32 toPC)
{
    struct RogueDirFacts f;
    struct FactBufs *b = Bufs();
    *dest = EOS;
    if (b == NULL)
        return dest;
    BaseFacts(&f, b);
    SetNick(&f, b, nickname);
    SayMore(dest, 600, DIR_RETURN_0 + Rogue_BadgeCount() % DIR_RETURNS, &f);
    if (toPC)
        SayMore(dest, 600, DIR_RETURN_PC, &f);
    FreeBufs();
    return dest + StringLength(dest);
}

bool32 Rogue_Run0BufferRivalAfter(void)
{
    if (!Rogue_IsRun0())
        return FALSE;
    StringCopy(gStringVar4, COMPOUND_STRING("..."));
    return TRUE;
}

// The Previous Player looks like the player did in the old games.
u16 Rogue_Run0RivalGfx(void)
{
    if (!Rogue_IsRun0())
        return 0;
    return gSaveBlock2Ptr->playerGender == MALE ? OBJ_EVENT_GFX_LINK_RS_BRENDAN : OBJ_EVENT_GFX_LINK_RS_MAY;
}

u16 Rogue_Run0E4Gfx(u32 member)
{
    if (!Rogue_IsRun0())
        return 0;
    switch (member)
    {
    case 0:  return OBJ_EVENT_GFX_HEX_MANIAC;                 // the Fallen
    case 1:  return Rogue_Run0RivalGfx();                       // the Previous Player
    case 2:  return OBJ_EVENT_GFX_PROF_BIRCH;
    default: return gSaveBlock2Ptr->playerGender == MALE ? OBJ_EVENT_GFX_BRENDAN_NORMAL : OBJ_EVENT_GFX_MAY_NORMAL; // the Copy
    }
}

// ---------------------------------------------------------------------------
// The story's trainers

// The Fallen's six: what the Director keeps (Terry's first), then the run
// log's lost, newest first, no species twice.
struct FallenMon
{
    u16 species;
    u8 shiny;
    u8 nickname[POKEMON_NAME_LENGTH + 1];
};

static u32 FallenTeam(struct RogueDirector *d, struct FallenMon *out)
{
    const struct RogueRunLogEntry *e;
    u32 n = 0, i, j, pass;
    if (d == NULL)
        return 0;
    for (pass = 0; pass < 2; pass++)
    {
        for (i = 0; i < d->keptCount && i < ROGUE_KEPT_MAX && n < PARTY_SIZE; i++)
        {
            const struct RogueKeptMon *k = &d->kept[i];
            if (k->species == SPECIES_NONE || (k->flags & ROGUE_KEPT_RETURNED)
             || !!(k->flags & ROGUE_KEPT_TERRY) != (pass == 0))
                continue;
            out[n].species = k->species;
            out[n].shiny = (k->flags & ROGUE_KEPT_SHINY) != 0;
            memcpy(out[n].nickname, k->nickname, POKEMON_NAME_LENGTH);
            out[n].nickname[POKEMON_NAME_LENGTH] = EOS;
            n++;
        }
    }
    for (i = 0; (e = Rogue_RunLogEntry(d, i)) != NULL && n < PARTY_SIZE; i++)
    {
        if (e->kind == ROGUE_RUN_KIND_RUN0)
            continue;
        for (j = 0; j < ROGUE_RUNLOG_LOST && n < PARTY_SIZE; j++)
        {
            u32 k;
            bool32 dup = FALSE;
            if (e->lost[j].species == SPECIES_NONE)
                continue;
            for (k = 0; k < n; k++)
                if (out[k].species == e->lost[j].species)
                    dup = TRUE;
            if (dup)
                continue;
            out[n].species = e->lost[j].species;
            out[n].shiny = FALSE;
            memcpy(out[n].nickname, e->lost[j].nickname, POKEMON_NAME_LENGTH);
            out[n].nickname[POKEMON_NAME_LENGTH] = EOS;
            n++;
        }
    }
    return n;
}

// CHAMPION TERRY's old team, the one a Champion should have.
static const u16 sChampionTerryTeam[PARTY_SIZE] =
{
    SPECIES_GYARADOS, SPECIES_CHARIZARD, SPECIES_LUCARIO, SPECIES_METAGROSS, SPECIES_SALAMENCE, SPECIES_DRAGONITE,
};

static bool32 HasSpecies(const struct TrainerMon *party, u32 n, u16 species)
{
    u32 i;
    for (i = 0; i < n; i++)
        if (party[i].species == species)
            return TRUE;
    return FALSE;
}

static void StoryMon(struct TrainerMon *m, u16 species, u32 level)
{
    memset(m, 0, sizeof(*m));
    m->species = species;
    m->lvl = level > MAX_LEVEL ? MAX_LEVEL : level;
    m->gender = TRAINER_MON_RANDOM_GENDER;
    m->iv = TRAINER_PARTY_IVS(31, 31, 31, 31, 31, 31);
}

static const u8 sText_Fallen[] = _("FALLEN");
static const u8 sText_Birch[] = _("BIRCH");
static const u8 sText_Terry[] = _("TERRY");

static void PlayerName(u8 *dest)
{
    StringCopyN(dest, gSaveBlock2Ptr->playerName, PLAYER_NAME_LENGTH);
    dest[PLAYER_NAME_LENGTH] = EOS;
}

// The story's own trainer: CHAMPION TERRY, the self-insert. The Previous
// Player, the Elite Four and the Director are the challenge's
// (src/rogue_run0_trainers.h).
u32 Rogue_Run0BuildTrainer(u32 slot, struct Trainer *t, struct TrainerMon *party)
{
    u32 n = 0, i;
    if (!Rogue_IsRun0() || slot != ROGUE_SLOT_RUN0_TERRY)
        return 0;
    memset(party, 0, sizeof(*party) * PARTY_SIZE);
    // The team a Champion should have, DRAGONITE ("my favorite") last.
    for (i = 0; i < PARTY_SIZE; i++)
        StoryMon(&party[n++], sChampionTerryTeam[i], 88 + i / 2);
    party[PARTY_SIZE - 1].heldItem = ITEM_SITRUS_BERRY;
    t->items[0] = ITEM_FULL_RESTORE;
    t->items[1] = ITEM_FULL_RESTORE;
    t->aiFlags = AI_FLAG_BASIC_TRAINER | AI_FLAG_SMART_MON_CHOICES | AI_FLAG_ACE_POKEMON;
    t->trainerClass = TRAINER_CLASS_CHAMPION;
    t->trainerPic = TRAINER_PIC_ROGUE_CHAMPION;
    t->gender = MALE;
    t->encounterMusic = TRAINER_ENCOUNTER_MUSIC_MALE;
    StringCopy(t->trainerName, sText_Terry);
    return n;
}

const u8 *Rogue_Run0TrainerText(u32 slot, u32 kind)
{
    struct RogueDirFacts f;
    struct FactBufs *b;
    u32 id = DIR_COUNT;
    bool32 defeat = kind == ROGUE_TEXT_DEFEAT;

    if (!Rogue_IsRun0())
        return NULL;
    if (slot >= ROGUE_SLOT_RIVAL_0 && slot < ROGUE_SLOT_RIVAL_0 + 4)
        id = DIR_PREV_SILENT;
    else if (slot == ROGUE_SLOT_E4_0)
    {
        id = defeat ? DIR_FALLEN_DEFEAT : DIR_FALLEN_INTRO;
    }
    else if (slot == ROGUE_SLOT_E4_0 + 1)
        id = DIR_PREV_SILENT;
    else if (slot == ROGUE_SLOT_E4_0 + 2)
        id = defeat ? DIR_BIRCH_E4_DEFEAT : DIR_BIRCH_E4_INTRO;
    else if (slot == ROGUE_SLOT_E4_0 + 3)
        id = defeat ? DIR_COPY_DEFEAT : DIR_COPY_INTRO;
    else if (slot == ROGUE_SLOT_RUN0_TERRY)
        id = defeat ? DIR_CT_DEFEAT : DIR_CT_INTRO;
    else if (slot == ROGUE_SLOT_CHAMPION)
        id = defeat ? (Rogue_Run0DirectorPhase() == 2 ? DIR_PHASE2_DEFEAT : DIR_PHASE1_DEFEAT) : DIR_CONFESSION;
    if (id == DIR_COUNT || (b = Bufs()) == NULL)
        return NULL;
    BaseFacts(&f, b);
    Say(id, &f);
    FreeBufs();
    return gStringVar4;
}

// Script special for the Elite Four (src/rogue.c Rogue_BufferE4 and
// Rogue_BufferE4After): their words in RUN 0.
bool32 Rogue_Run0BufferE4(u32 member, bool32 after)
{
    struct RogueDirFacts f;
    struct FactBufs *b;
    u32 id;
    if (!Rogue_IsRun0() || (b = Bufs()) == NULL)
        return FALSE;
    BaseFacts(&f, b);
    switch (member)
    {
    case 0:
        if (!after)
        {
            struct RogueDirector *d = Rogue_Director();
            struct FallenMon *fallen = Alloc(sizeof(struct FallenMon) * PARTY_SIZE);
            u32 count = fallen != NULL ? FallenTeam(d, fallen) : 0;
            TRY_FREE_AND_SET_NULL(fallen);
            Rogue_ReleaseDirector();
            id = count != 0 ? DIR_FALLEN_INTRO : DIR_FALLEN_NONE;
        }
        else
            id = DIR_FALLEN_AFTER;
        break;
    case 1:  id = after ? DIR_PREV_AFTER : DIR_PREV_SILENT; break;
    case 2:  id = after ? DIR_BIRCH_E4_AFTER : DIR_BIRCH_E4_INTRO; break;
    default: id = after ? DIR_COPY_AFTER : DIR_COPY_INTRO; break;
    }
    Say(id, &f);
    if (!after)
    {
        // The name for the battle's box.
        static const u8 *const names[] = { sText_Fallen, NULL, sText_Birch, NULL };
        if (names[member & 3] != NULL)
            StringCopy(gStringVar1, names[member & 3]);
        else
            PlayerName(gStringVar1);
    }
    FreeBufs();
    return TRUE;
}

// ---------------------------------------------------------------------------
// Battles: DRAFT, the darkened picture, the end of a battle

static const u8 sText_DraftName[] = _("DRAFT 0");

void Rogue_Run0OnWildBattleStart(void)
{
    struct RogueDirector *d;
    struct RogueRun0Story *s;
    struct Pokemon *mon = &gParties[B_TRAINER_OPPONENT_A][0];
    u8 name[POKEMON_NAME_LENGTH + 1];
    bool32 shiny = TRUE;
    u32 number;

    FlagClear(RUN0_FLAG_DRAFT_NOW);
    if (sForceDraft && Rogue_IsRun0())
        sForceDraft = FALSE;
    else if (!Rogue_IsRun0() || VarGet(VAR_ROGUE_RUN0_ACT) < 2 || Rogue_GetCurrentPlace() < 0
          || gSaveBlock1Ptr->location.mapGroup != ROGUE_GROUP || (Random() % 24) != 0)
        return;
    d = Rogue_Director();
    s = Rogue_Run0Story(d);
    if (s == NULL || s->drafts >= 3)
    {
        Rogue_ReleaseDirector();
        return;
    }
    s->drafts++;
    // Its number: the species as the Director files it.
    number = 10 + (GetMonData(mon, MON_DATA_SPECIES) * 37) % 90;
    ConvertIntToDecimalStringN(StringCopy(name, sText_DraftName), number, STR_CONV_MODE_LEADING_ZEROS, 2);
    SetMonData(mon, MON_DATA_NICKNAME, name);
    // The wrong colors.
    SetMonData(mon, MON_DATA_IS_SHINY, &shiny);
    FlagSet(RUN0_FLAG_DRAFT_NOW);
    Rogue_SaveDirector();
}

const u8 *Rogue_Run0CatchBlockedText(void)
{
    if (!Rogue_IsRun0() || !FlagGet(RUN0_FLAG_DRAFT_NOW) || (gBattleTypeFlags & BATTLE_TYPE_TRAINER))
        return NULL;
    return COMPOUND_STRING("It isn't finished.\nIt can't be caught.{PAUSE_UNTIL_PRESS}");
}

// The Director's picture is the player's own, darkened.
void Rogue_Run0TrainerPicLoaded(u16 paletteTag)
{
    u32 index, i;
    if (!Rogue_IsRun0() || !(gBattleTypeFlags & BATTLE_TYPE_TRAINER)
     || TRAINER_BATTLE_PARAM.opponentA != ROGUE_TRAINER(ROGUE_SLOT_CHAMPION))
        return;
    index = IndexOfSpritePaletteTag(paletteTag);
    if (index >= 16)
        return;
    for (i = 1; i < 16; i++)
    {
        u16 c = gPlttBufferUnfaded[OBJ_PLTT_ID(index) + i];
        u32 r = (c & 0x1F) / 3, g = ((c >> 5) & 0x1F) / 3, b = ((c >> 10) & 0x1F) / 3 + 1;
        gPlttBufferUnfaded[OBJ_PLTT_ID(index) + i] = RGB(r, g, b > 31 ? 31 : b);
        gPlttBufferFaded[OBJ_PLTT_ID(index) + i] = gPlttBufferUnfaded[OBJ_PLTT_ID(index) + i];
    }
}

void Rogue_Run0StoryBattleEnd(void)
{
    if (FlagGet(RUN0_FLAG_DRAFT_NOW))
    {
        FlagClear(RUN0_FLAG_DRAFT_NOW);
        FlagSet(RUN0_FLAG_DRAFT_SEEN);
    }
}

// ---------------------------------------------------------------------------
// The PATCH box after a badge is the challenge's (Rogue_Run0BufferPatch);
// the story plays the jingle before it.

// Script special: the unlock jingle, slow and low, as near backwards as the
// sound engine goes.
void Rogue_Run0PatchJingle(void)
{
    PlayFanfare(MUS_OBTAIN_ITEM);
    m4aMPlayTempoControl(&gMPlayInfo_BGM, 0x90);
    m4aMPlayPitchControl(&gMPlayInfo_BGM, TRACKS_ALL, -0x300);
}

// ---------------------------------------------------------------------------
// Things read: the graveyard's graves and the library's shelves

static const struct RogueRegion *Region(void)
{
    return Rogue_GetRegion();
}

// The hook of the house the player is in, or -1.
static s32 HouseHookHere(void);

bool32 Rogue_Run0InLibrary(void)
{
    return Rogue_IsRun0() && HouseHookHere() == RUN0_HOOK_LIBRARY;
}

s32 Rogue_Run0ShortcutPlace(void)
{
    return Rogue_Run0Active() ? RogueGen_Run0ShortcutPlace(Region()) : -1;
}

static s32 HouseHookHere(void)
{
    const struct RogueRegion *r;
    const struct RogueMap *m;
    s32 place = Rogue_GetCurrentPlace();
    u32 house = gSaveBlock1Ptr->location.warpId;
    if (!Rogue_Run0Active() || place < 0 || gSaveBlock1Ptr->location.mapGroup != ROGUE_GROUP
     || gSaveBlock1Ptr->location.mapNum != MAP_NUM(MAP_ROGUE_HOUSE))
        return -1;
    r = Region();
    m = Rogue_GetPlaceMap(place);
    if (m == NULL || house >= m->nHouses)
        return -1;
    return RogueGen_Run0HouseHook(r, m, house);
}

// The graveyard's grave k (in reading order): the run log's lost, newest
// run first.
static bool32 GraveLine(struct RogueDirector *d, u32 k, struct RogueDirFacts *f, struct FactBufs *b)
{
    const struct RogueRunLogEntry *e;
    u32 i, j;
    for (i = 0; (e = Rogue_RunLogEntry(d, i)) != NULL; i++)
    {
        for (j = 0; j < ROGUE_RUNLOG_LOST; j++)
        {
            if (e->lost[j].species == SPECIES_NONE)
                continue;
            if (k-- != 0)
                continue;
            {
                u8 name[POKEMON_NAME_LENGTH + 1];
                memcpy(name, e->lost[j].nickname, POKEMON_NAME_LENGTH);
                name[POKEMON_NAME_LENGTH] = EOS;
                SetNick(f, b, name[0] != EOS ? name : GetSpeciesName(e->lost[j].species));
            }
            SetMon(f, b, e->lost[j].species);
            f->n = e->lost[j].level;
            f->n2 = e->run;
            if (e->kind == ROGUE_RUN_KIND_RUN0)
            {
                StringCopy((u8 *)b->region, COMPOUND_STRING("RUN 0"));
                Rogue_GameToAscii(b->region, (const u8 *)b->region, ROGUE_REGION_NAME_LEN);
                f->region = b->region;
            }
            else
            {
                SetRegion(f, b, e->seed);
            }
            return TRUE;
        }
    }
    return FALSE;
}

static u32 LostCounts(const struct RogueDirector *d, u32 *named)
{
    const struct RogueRunLogEntry *e;
    u32 i, j, all = 0;
    *named = 0;
    for (i = 0; (e = Rogue_RunLogEntry(d, i)) != NULL; i++)
    {
        all += e->lostCount;
        for (j = 0; j < ROGUE_RUNLOG_LOST; j++)
            *named += e->lost[j].species != SPECIES_NONE;
    }
    return all > *named ? all : *named;
}

// A shelf of the library: its region in turn, the played ones first.
static void ShelfLine(struct RogueDirector *d, struct RogueRun0Story *s, struct RogueDirFacts *f, struct FactBufs *b)
{
    static const u8 *const rejected[] = { COMPOUND_STRING("CRAG"), COMPOUND_STRING("MARINA"), COMPOUND_STRING("BASIL"),
                                          COMPOUND_STRING("CINDER"), COMPOUND_STRING("VOLTZ"), COMPOUND_STRING("MAKO"),
                                          COMPOUND_STRING("LUNA"), COMPOUND_STRING("ACE") };
    const struct RogueRunLogEntry *e;
    u32 shelf = s->shelf++, played = 0, i;
    for (i = 0; Rogue_RunLogEntry(d, i) != NULL; i++)
        played++;
    // One shelf in five holds a leader who was not kept.
    if (shelf % 5 == 4)
    {
        u32 who = (shelf / 5) % ARRAY_COUNT(rejected), tryNo = 1 + (shelf * 13) % 40;
        // MARINA was try thirty-one: that one was kept.
        if (who == 1 && tryNo == 31)
            tryNo = 30;
        SetName(f, b, rejected[who]);
        f->n = tryNo;
        Say(DIR_SHELF_REJECTED, f);
        return;
    }
    shelf -= shelf / 5;
    if (shelf < played && (e = Rogue_RunLogEntry(d, shelf)) != NULL)
    {
        if (e->kind == ROGUE_RUN_KIND_RUN0)
        {
            f->n = d->attempts;
            Say(DIR_SHELF_RUN0, f);
            return;
        }
        SetRegion(f, b, e->seed);
        f->n = e->badges + 1;
        if (e->result == ROGUE_LOG_WON)
            Say(DIR_SHELF_WON, f);
        else if (e->result == ROGUE_LOG_LEFT || e->result == ROGUE_LOG_ACTIVE)
            Say(DIR_SHELF_LEFT, f);
        else
            Say(e->badges == 0 ? DIR_SHELF_LOST0 : DIR_SHELF_LOST, f);
        return;
    }
    // The rest: regions never played, in the order the names go.
    SetRegion(f, b, (shelf * 4099 + 17) % ROGUE_REGION_COUNT);
    Say(DIR_SHELF_UNPLAYED, f);
}

static bool32 IsShelf(u32 behavior)
{
    return MetatileBehavior_IsBookShelf(behavior) || MetatileBehavior_IsPictureBookShelf(behavior)
        || MetatileBehavior_IsPlayerFacingTVScreen(behavior, DIR_NORTH) || MetatileBehavior_IsCabinet(behavior);
}

const u8 *Rogue_Run0ThingScript(s16 x, s16 y)
{
    struct RogueDirector *d;
    struct RogueRun0Story *s;
    struct RogueDirFacts f;
    struct FactBufs *b;
    s32 dungeon, hook;

    if (!Rogue_Run0Active() || !Rogue_IsRun0())
        return NULL;
    dungeon = Rogue_GetFloorDungeon();
    if (dungeon >= 0 && dungeon == RogueGen_Run0Graveyard(Region()))
    {
        // The graveyard's graves: each the Pokemon of the run log in turn.
        s32 floor = Rogue_GetFloorNumber();
        u8 k, thing;
        bool8 lore;
        u32 named, all;
        if (floor < 0 || x < MAP_OFFSET || y < MAP_OFFSET)
            return NULL;
        thing = RogueGen_FloorThingAt(Region(), dungeon, floor, x - MAP_OFFSET, y - MAP_OFFSET, &k, &lore);
        if (thing != RTHING_GRAVE || (b = Bufs()) == NULL)
            return NULL;
        d = Rogue_Director();
        BaseFacts(&f, b);
        if (GraveLine(d, floor * 12 + k, &f, b))
            Say(DIR_GRAVE, &f);
        else
            Say(DIR_GRAVE_BLANK, &f);
        // The first grave read: what the Director kept of them, once.
        if (!BeatUsed(d, RUN0_BEAT_GRAVE) && d != NULL && d->runLogCount != 0)
        {
            all = LostCounts(d, &named);
            f.n = all;
            f.n2 = named;
            SayMore(gStringVar4, sizeof(gStringVar4), all > named ? DIR_GRAVE_ROOM : DIR_GRAVE_ALL, &f);
            UseBeat(d, RUN0_BEAT_GRAVE);
            Rogue_SaveDirector();
        }
        else
        {
            Rogue_ReleaseDirector();
        }
        FreeBufs();
        return Rogue_EventScript_Run0ReadThing;
    }
    hook = HouseHookHere();
    // The sign by the far door: the room's number.
    if (hook == RUN0_HOOK_LIBRARY && y - MAP_OFFSET == RUN0_LIBRARY_DOOR_Y
     && (x - MAP_OFFSET == RUN0_LIBRARY_DOOR_X - 1 || x - MAP_OFFSET == RUN0_LIBRARY_DOOR_X + 1))
    {
        if ((b = Bufs()) == NULL)
            return NULL;
        d = Rogue_Director();
        s = Rogue_Run0Story(d);
        BaseFacts(&f, b);
        f.n = (s != NULL ? s->libRoom : 0) + 1;
        Rogue_ReleaseDirector();
        Say(DIR_LIB_ROOM, &f);
        FreeBufs();
        return Rogue_EventScript_Run0ReadThing;
    }
    if (hook == RUN0_HOOK_LIBRARY && IsShelf(MapGridGetMetatileBehaviorAt(x, y)))
    {
        if ((b = Bufs()) == NULL)
            return NULL;
        d = Rogue_Director();
        s = Rogue_Run0Story(d);
        if (s == NULL)
        {
            FreeBufs();
            return NULL;
        }
        BaseFacts(&f, b);
        ShelfLine(d, s, &f, b);
        // Every third shelf, the Director's case, a line at a time.
        if (s->shelf % 3 == 0 && s->caseLine < DIR_CASES)
        {
            u32 line = s->caseLine++;
            if (line == 1)
            {
                // The fallen friend, if there is one.
                const struct RogueRecord *record = Rogue_GetRecord();
                if (record->fallenName[0] != EOS)
                {
                    SetNick(&f, b, record->fallenName);
                    SayMore(gStringVar4, sizeof(gStringVar4), DIR_CASE_0 + line, &f);
                }
            }
            else
            {
                SayMore(gStringVar4, sizeof(gStringVar4), DIR_CASE_0 + line, &f);
            }
        }
        Rogue_SaveDirector();
        FreeBufs();
        return Rogue_EventScript_Run0ReadThing;
    }
    return NULL;
}

// ---------------------------------------------------------------------------
// The people of the story's houses: the Believers, TERRY, the empty library

u32 Rogue_Run0HouseCast(u32 place, u32 house, struct Run0Cast *cast)
{
    const struct RogueMap *m = Rogue_GetPlaceMap(place);
    s32 hook;
    if (!Rogue_IsRun0() || m == NULL)
        return 0;
    hook = RogueGen_Run0HouseHook(Region(), m, house);
    switch (hook)
    {
    case RUN0_HOOK_BELIEVER_0:
    case RUN0_HOOK_BELIEVER_1:
    case RUN0_HOOK_BELIEVER_2:
        cast[0].gfx = OBJ_EVENT_GFX_HEX_MANIAC;
        cast[0].script = Rogue_EventScript_Run0Believer;
        cast[0].movement = MOVEMENT_TYPE_FACE_DOWN;
        cast[1].gfx = (hook & 1) ? OBJ_EVENT_GFX_GIRL_1 : OBJ_EVENT_GFX_BOY_1;
        cast[1].script = Rogue_EventScript_Run0Believer;
        cast[1].movement = MOVEMENT_TYPE_LOOK_AROUND;
        return 2;
    case RUN0_HOOK_TERRY_HOUSE:
        if (FlagGet(FLAG_ROGUE_RUN0_TERRY_DELETED))
            return 0;
        // An ordinary, tired man (as Emerald's own designer in Lilycove).
        cast[0].gfx = OBJ_EVENT_GFX_MAN_4;
        cast[0].script = Rogue_EventScript_Run0Terry;
        cast[0].movement = MOVEMENT_TYPE_FACE_DOWN;
        return 1;
    }
    return 0;
}

// Script special: a Believer's line (the one talked to) into gStringVar4.
void Rogue_Run0BelieverTalk(void)
{
    struct RogueDirFacts f;
    struct FactBufs *b = Bufs();
    s32 hook = HouseHookHere();
    u32 i, who = 0;
    if (b == NULL)
        return;
    // The first person of the room is the HEX MANIAC, the second the kid.
    for (i = 0; i < OBJECT_EVENT_TEMPLATES_COUNT; i++)
    {
        if (gSaveBlock1Ptr->objectEventTemplates[i].localId == gSpecialVar_LastTalked)
        {
            who = gSaveBlock1Ptr->objectEventTemplates[i].graphicsId != OBJ_EVENT_GFX_HEX_MANIAC;
            break;
        }
    }
    if (hook < RUN0_HOOK_BELIEVER_0 || hook > RUN0_HOOK_BELIEVER_2)
        hook = RUN0_HOOK_BELIEVER_0;
    BaseFacts(&f, b);
    Say(DIR_BELIEVER_0 + (hook - RUN0_HOOK_BELIEVER_0) * 2 + who, &f);
    FreeBufs();
}

// Script special at the arrival: the Director's word on TERRY's team, if
// it is in the party or in BOX 1 (gStringVar4; VAR_RESULT FALSE if not).
void Rogue_Run0TestTeamLine(void)
{
    struct RogueDirFacts f;
    struct FactBufs *b;
    u32 i, id = DIR_COUNT;
    bool32 lent;
    gSpecialVar_Result = FALSE;
    if (!Rogue_IsRun0())
        return;
    // "These six are TERRY's." only when they are: a party filled from the
    // boxes or lent may hold two of his among others.
    for (i = 0; i < gPartiesCount[B_TRAINER_PLAYER] && i < PARTY_SIZE; i++)
        if (!Rogue_IsTerrysMon(&gParties[B_TRAINER_PLAYER][i]))
            break;
    if (i != 0 && i == gPartiesCount[B_TRAINER_PLAYER])
        id = DIR_TEST_TEAM_PARTY;
    for (i = 0; i < IN_BOX_COUNT && id == DIR_COUNT; i++)
        if (GetBoxMonDataAt(ROGUE_TEST_TEAM_BOX, i, MON_DATA_SPECIES) != SPECIES_NONE)
            id = DIR_TEST_TEAM_BOX;
    lent = Rogue_Run0LentCount() != 0;
    if ((id == DIR_COUNT && !lent) || (b = Bufs()) == NULL)
        return;
    BaseFacts(&f, b);
    gStringVar4[0] = EOS;
    if (id != DIR_COUNT)
        Say(id, &f);
    // Fewer than six in all: it lent some of the ones it kept.
    if (lent)
        SayMore(gStringVar4, sizeof(gStringVar4), DIR_LEND, &f);
    gSpecialVar_Result = gStringVar4[0] != EOS;
    FreeBufs();
}

// The first of Terry's team in the party, or in the box, or kept.
static bool32 TerryMonHere(u8 *name, u32 *where)
{
    u32 i;
    struct RogueDirector *d;
    for (i = 0; i < gPartiesCount[B_TRAINER_PLAYER] && i < PARTY_SIZE; i++)
    {
        if (Rogue_IsTerrysMon(&gParties[B_TRAINER_PLAYER][i]))
        {
            GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_NICKNAME, name);
            *where = 0;
            return TRUE;
        }
    }
    d = Rogue_Director();
    for (i = 0; d != NULL && i < d->keptCount && i < ROGUE_KEPT_MAX; i++)
    {
        if ((d->kept[i].flags & ROGUE_KEPT_TERRY) && !(d->kept[i].flags & ROGUE_KEPT_RETURNED))
        {
            memcpy(name, d->kept[i].nickname, POKEMON_NAME_LENGTH);
            name[POKEMON_NAME_LENGTH] = EOS;
            *where = 1;
            Rogue_ReleaseDirector();
            return TRUE;
        }
    }
    Rogue_ReleaseDirector();
    *where = 2;
    return FALSE;
}

// Script special: what Terry says, VAR_0x8004 the part (0 the first talk,
// 3 its three things and the gift, 1 after the gift, 2 talked to again),
// into gStringVar4.
void Rogue_Run0TerryTalk(void)
{
    struct RogueDirector *d;
    struct RogueRun0Story *s;
    struct RogueDirFacts f;
    struct FactBufs *b = Bufs();
    u8 name[POKEMON_NAME_LENGTH + 1];
    u32 where;

    if (b == NULL)
        return;
    BaseFacts(&f, b);
    gStringVar4[0] = EOS;
    switch (gSpecialVar_0x8004)
    {
    case 0:
        d = Rogue_Director();
        s = Rogue_Run0Story(d);
        SayMore(gStringVar4, sizeof(gStringVar4), s != NULL && s->terryDeleted ? DIR_TERRY_HELLO_AGAIN : DIR_TERRY_HELLO, &f);
        Rogue_ReleaseDirector();
        if (TerryMonHere(name, &where))
        {
            SetNick(&f, b, name);
            SayMore(gStringVar4, sizeof(gStringVar4), where == 0 ? DIR_TERRY_TEAM_HERE : DIR_TERRY_TEAM_KEPT, &f);
        }
        else
        {
            SayMore(gStringVar4, sizeof(gStringVar4), DIR_TERRY_TEAM_PC, &f);
        }
        SayMore(gStringVar4, sizeof(gStringVar4), DIR_TERRY_PLAY, &f);
        break;
    case 3:
        // (A box of its own: the whole first talk does not fit one buffer.)
        SayMore(gStringVar4, sizeof(gStringVar4), DIR_TERRY_THREE, &f);
        SayMore(gStringVar4, sizeof(gStringVar4), DIR_TERRY_GIFT, &f);
        break;
    case 1:
        // Four: the shortcut, its trees cut (RogueGen_Run0ShortcutPlace).
        if (Rogue_Run0ShortcutPlace() >= 0)
        {
            SayMore(gStringVar4, sizeof(gStringVar4), DIR_TERRY_SHORTCUT, &f);
            FlagSet(RUN0_FLAG_SHORTCUT);
        }
        SayMore(gStringVar4, sizeof(gStringVar4), DIR_TERRY_SWITCH, &f);
        SayMore(gStringVar4, sizeof(gStringVar4), DIR_TERRY_CAPE, &f);
        FlagSet(RUN0_FLAG_TERRY_MET);
        FlagSet(RUN0_FLAG_SWITCH_READY);
        break;
    default:
        Say(DIR_TERRY_LATER, &f);
        break;
    }
    FreeBufs();
}

// ---------------------------------------------------------------------------
// Story objects and signs: the gift box, CHAMPION TERRY, the PROMPT LOGs

// Script special (Rogue_EventScript_Run0Thing, Rogue_EventScript_Run0Hook):
// VAR_0x8004 the hook. Its words into gStringVar4; VAR_RESULT what follows:
// 0 nothing, 1 a RARE CANDY, 2 CHAMPION TERRY's battle.
void Rogue_Run0HookTalk(void)
{
    struct RogueDirector *d;
    struct RogueRun0Story *s;
    struct RogueDirFacts f;
    struct FactBufs *b = Bufs();
    u32 hook = gSpecialVar_0x8004;

    gSpecialVar_Result = 0;
    if (b == NULL)
        return;
    BaseFacts(&f, b);
    if (RUN0_HOOK_IS_SIGN(hook))
    {
        u32 k = hook - RUN0_HOOK_PROMPT_0;
        bool32 deleted = FlagGet(FLAG_ROGUE_RUN0_TERRY_DELETED);
        Say((deleted ? DIR_PROMPT_DEL_0 : DIR_PROMPT_0) + k, &f);
        // The Director answers three of TERRY's, once.
        if (!deleted && (k == 2 || k == 4 || k == 7))
        {
            u32 beat = k == 2 ? RUN0_BEAT_REPLY_2 : k == 4 ? RUN0_BEAT_REPLY_4 : RUN0_BEAT_REPLY_7;
            d = Rogue_Director();
            if (d != NULL && !BeatUsed(d, beat))
            {
                UseBeat(d, beat);
                SayMore(gStringVar4, sizeof(gStringVar4), k == 2 ? DIR_PROMPT_REPLY_2 : k == 4 ? DIR_PROMPT_REPLY_4 : DIR_PROMPT_REPLY_7, &f);
                Rogue_SaveDirector();
            }
            else
            {
                Rogue_ReleaseDirector();
            }
        }
        FreeBufs();
        return;
    }
    switch (hook)
    {
    case RUN0_HOOK_GIFT_BOX:
    {
        // A RARE CANDY once a real day (the cartridge's clock), or once an
        // attempt with no clock.
        struct SiiRtcInfo rtc;
        bool32 clock = !(RtcGetErrorStatus() & RTC_ERR_FLAG_MASK);
        u16 day = 0;
        d = Rogue_Director();
        s = Rogue_Run0Story(d);
        if (clock)
        {
            RtcGetInfo(&rtc);
            day = RtcGetDayCount(&rtc) + 1;
        }
        if (s != NULL && (clock ? s->giftDay != day : !FlagGet(RUN0_FLAG_GIFT_TAKEN)))
        {
            if (clock)
                s->giftDay = day;
            FlagSet(RUN0_FLAG_GIFT_TAKEN);
            Say(DIR_GIFT, &f);
            gSpecialVar_Result = 1;
            Rogue_SaveDirector();
        }
        else
        {
            Say(DIR_GIFT_EMPTY, &f);
            Rogue_ReleaseDirector();
        }
        break;
    }
    case RUN0_HOOK_CHAMPION_TERRY:
        if (!FlagGet(TRAINER_FLAGS_START + ROGUE_TRAINER(ROGUE_SLOT_RUN0_TERRY)))
        {
            gSpecialVar_Result = 2;
            gStringVar4[0] = EOS;
        }
        else
        {
            Say(FlagGet(RUN0_FLAG_CT_TALKED) ? DIR_CT_AGAIN : DIR_CT_AFTER, &f);
            FlagSet(RUN0_FLAG_CT_TALKED);
        }
        break;
    default:
        Say(DIR_TERRY_EMPTY_LOT, &f);
        break;
    }
    FreeBufs();
}

// Script special: what CHAMPION TERRY says after his battle.
void Rogue_Run0ChampionTerryAfter(void)
{
    struct RogueDirFacts f;
    struct FactBufs *b = Bufs();
    if (b == NULL)
        return;
    BaseFacts(&f, b);
    Say(DIR_CT_AFTER, &f);
    FlagSet(RUN0_FLAG_CT_TALKED);
    FreeBufs();
}

// ---------------------------------------------------------------------------
// Scenes that wait for the field: on loading a map, and each frame

// People standing on the real Littleroot's map now.
static u32 LittlerootPeople(void)
{
    const struct MapHeader *h = Overworld_GetMapHeaderByGroupAndId(MAP_GROUP(MAP_LITTLEROOT_TOWN), MAP_NUM(MAP_LITTLEROOT_TOWN));
    u32 i, n = 0;
    for (i = 0; i < h->events->objectEventCount; i++)
    {
        const struct ObjectEventTemplate *t = &h->events->objectEvents[i];
        if (t->kind != OBJ_KIND_NORMAL || t->graphicsId == OBJ_EVENT_GFX_TRUCK || t->graphicsId == OBJ_EVENT_GFX_ITEM_BALL)
            continue;
        if (t->flagId != 0 && FlagGet(t->flagId))
            continue;
        n++;
    }
    return n;
}

static u32 PlayMinutes(void)
{
    return gSaveBlock2Ptr->playTimeHours * 60 + gSaveBlock2Ptr->playTimeMinutes;
}

static bool32 ClockLate(u8 *hour, u8 *minute)
{
    struct SiiRtcInfo rtc;
    if (RtcGetErrorStatus() & RTC_ERR_FLAG_MASK)
        return FALSE;
    RtcGetInfo(&rtc);
    *hour = ConvertBcdToBinary(rtc.hour);
    *minute = ConvertBcdToBinary(rtc.minute);
    return *hour < 5 && *minute < 60;
}

void Rogue_Run0OnMapLoad(void)
{
    struct RogueDirector *d;
    struct RogueRun0Story *s;
    s32 place = Rogue_GetCurrentPlace(), island;
    const struct RogueRegion *r;
    u8 hour, minute;

    if (Rogue_IsEndless() && place >= 0 && gSaveBlock1Ptr->location.mapNum == MAP_NUM(MAP_ROGUE_LEAGUE)
     && gSaveBlock1Ptr->location.mapGroup == ROGUE_GROUP)
    {
        sPendingScene = RUN0_SCENE_ENDLESS_GATE;
        return;
    }
    if (!Rogue_IsRun0() || !Rogue_Run0Active())
        return;
    r = Region();
    island = Rogue_Run0IslandHere();
    if (island == RUN0_ISLAND_LITTLEROOT)
    {
        if (!FlagGet(RUN0_FLAG_ISLAND_ON))
        {
            FlagSet(RUN0_FLAG_ISLAND_ON);
            VarSet(VAR_ROGUE_RUN0_ISLAND_MIN, PlayMinutes());
        }
    }
    else if (FlagGet(RUN0_FLAG_ISLAND_ON) && gSaveBlock1Ptr->location.mapGroup == ROGUE_GROUP && place >= 0
          && gSaveBlock1Ptr->location.mapNum < ROGUE_MAX_PLACES)
    {
        FlagClear(RUN0_FLAG_ISLAND_ON);
        if (!FlagGet(RUN0_FLAG_STOP_WALK))
            sPendingScene = RUN0_SCENE_ISLAND;
    }
    // The library: once on the way in, once on the way out. Come in again
    // from the library itself (its far door): the next room.
    if (HouseHookHere() == RUN0_HOOK_LIBRARY)
    {
        if (FlagGet(RUN0_FLAG_IN_LIBRARY))
        {
            struct RogueDirector *d = Rogue_Director();
            struct RogueRun0Story *s = Rogue_Run0Story(d);
            if (s != NULL && s->libRoom < 4095)
                s->libRoom++;
            Rogue_SaveDirector();
        }
        else
        {
            // In from the town: the welcome (once ever).
            sPendingScene = RUN0_SCENE_LIB_IN;
        }
        FlagSet(RUN0_FLAG_IN_LIBRARY);
    }
    else if (FlagGet(RUN0_FLAG_IN_LIBRARY))
    {
        FlagClear(RUN0_FLAG_IN_LIBRARY);
        sPendingScene = RUN0_SCENE_LIB_OUT;
    }
    if (place >= 0 && (u32)place == r->victoryRoad && gSaveBlock1Ptr->location.mapNum == (u32)place
     && VarGet(VAR_ROGUE_RUN0_ACT) >= ROGUE_RUN0_ACTS)
        sPendingScene = RUN0_SCENE_VICTORY;
    // Walking into the sixth gym: the fake crash (once ever).
    if (gSaveBlock1Ptr->location.mapGroup == ROGUE_GROUP && gSaveBlock1Ptr->location.mapNum == MAP_NUM(MAP_ROGUE_GYM)
     && Rogue_BadgeCount() == 5)
        sPendingScene = RUN0_SCENE_CRASH;
    // The LEAGUE's door in act 4, CHAMPION TERRY beaten: TERRY's switch.
    if (gSaveBlock1Ptr->location.mapGroup == ROGUE_GROUP && gSaveBlock1Ptr->location.mapNum == MAP_NUM(MAP_ROGUE_LEAGUE)
     && !FlagGet(FLAG_ROGUE_RUN0_TERRY_DELETED) && FlagGet(TRAINER_FLAGS_START + ROGUE_TRAINER(ROGUE_SLOT_RUN0_TERRY)))
        sPendingScene = RUN0_SCENE_SWITCH;
    // The clock, past midnight (once ever).
    if (sPendingScene == RUN0_SCENE_NONE && ClockLate(&hour, &minute))
    {
        d = Rogue_Director();
        if (d != NULL && !BeatUsed(d, RUN0_BEAT_CLOCK))
            sPendingScene = RUN0_SCENE_CLOCK;
        Rogue_ReleaseDirector();
    }
    (void)s;
}

// The League lobby's TERRY stands only while his switch waits.
void Rogue_Run0LeagueTransition(void)
{
    if (Rogue_IsRun0() && !FlagGet(FLAG_ROGUE_RUN0_TERRY_DELETED)
     && FlagGet(TRAINER_FLAGS_START + ROGUE_TRAINER(ROGUE_SLOT_RUN0_TERRY)))
        FlagClear(FLAG_ROGUE_RUN0_HIDE_TERRY);
    else
        FlagSet(FLAG_ROGUE_RUN0_HIDE_TERRY);
}

// Script special: a waiting scene's words (VAR_0x8004 the scene) into
// gStringVar4; VAR_RESULT FALSE if it has nothing to say after all.
void Rogue_Run0SceneText(void)
{
    struct RogueDirector *d = Rogue_Director();
    struct RogueRun0Story *s = Rogue_Run0Story(d);
    struct RogueDirFacts f;
    struct FactBufs *b = Bufs();
    u32 beat = 0xFF, id = DIR_COUNT;
    u8 hour, minute;

    gSpecialVar_Result = FALSE;
    if (b == NULL || d == NULL)
    {
        Rogue_ReleaseDirector();
        FreeBufs();
        return;
    }
    BaseFacts(&f, b);
    switch (gSpecialVar_0x8004)
    {
    case RUN0_SCENE_RESET:
        if (!d->resetPending)
            break;
        d->resetPending = FALSE;
        Rogue_GameToAscii(b->name, d->resetName, POKEMON_NAME_LENGTH);
        f.name = b->name;
        id = d->resetTrainer != 0 ? DIR_RESET_TRAINER : DIR_RESET_WILD;
        beat = 0xFE; // written below, not a beat
        break;
    case RUN0_SCENE_CLOCK:
        if (!ClockLate(&hour, &minute))
            break;
        b->time[0] = '0' + hour;
        b->time[1] = ':';
        b->time[2] = '0' + minute / 10;
        b->time[3] = '0' + minute % 10;
        b->time[4] = '\0';
        f.time = b->time;
        id = DIR_CLOCK, beat = RUN0_BEAT_CLOCK;
        break;
    case RUN0_SCENE_ISLAND:
    {
        u32 then = VarGet(VAR_ROGUE_RUN0_ISLAND_MIN), now = PlayMinutes();
        f.n = now >= then ? now - then : 0;
        if (f.n == 0)
            f.n = 1;
        f.n2 = LittlerootPeople();
        f.runs = VarGet(VAR_ROGUE_META_RUNS);
        id = DIR_LEAVE_ISLAND, beat = RUN0_BEAT_ISLAND;
        break;
    }
    case RUN0_SCENE_VICTORY:
        id = DIR_VICTORY_ROAD, beat = RUN0_BEAT_VICTORY;
        break;
    case RUN0_SCENE_LIB_IN:
        id = DIR_LIB_ENTER, beat = RUN0_BEAT_LIB_ENTER;
        break;
    case RUN0_SCENE_LIB_OUT:
        // Once the case is made, on the way out (nothing before).
        if (s->caseLine >= DIR_CASES)
            id = DIR_LIB_OUT, beat = RUN0_BEAT_LIB_OUT;
        break;
    case RUN0_SCENE_ENDLESS:
        f.n = s->endlessDepth;
        id = s->endlessDepth <= 1 ? DIR_ENDLESS_1 : s->endlessDepth == 2 ? DIR_ENDLESS_2
           : s->endlessDepth == 3 ? DIR_ENDLESS_3 : DIR_ENDLESS_N;
        break;
    case RUN0_SCENE_DRAFT:
        id = DIR_DRAFT_AFTER, beat = RUN0_BEAT_DRAFT;
        break;
    }
    if (id != DIR_COUNT && (beat >= 0xFE || !BeatUsed(d, beat)))
    {
        if (beat < 0xFE)
            UseBeat(d, beat);
        gSpecialVar_Result = Say(id, &f);
        Rogue_SaveDirector();
    }
    else
    {
        Rogue_ReleaseDirector();
    }
    FreeBufs();
}

void Rogue_Run0StoryFrame(void)
{
    u32 scene;
    if (ScriptContext_IsEnabled() || ArePlayerFieldControlsLocked() || gPaletteFade.active)
        return;
    if (gRogueTestStory != 0)
    {
        u32 test = gRogueTestStory;
        gRogueTestStory = 0;
        if (test == RUN0_TEST_DRAFT)
        {
            sForceDraft = TRUE;
            return;
        }
        if (test == RUN0_TEST_DEX)
        {
            u32 dex = SpeciesToNationalPokedexNum(gSpecialVar_0x8004);
            GetSetPokedexFlag(dex, FLAG_SET_SEEN);
            GetSetPokedexFlag(dex, FLAG_SET_CAUGHT);
            Rogue_Run0DexNote(gSpecialVar_0x8004);
            return;
        }
        if (test == RUN0_TEST_LITTLEROOT)
        {
            SetWarpDestination(MAP_GROUP(MAP_LITTLEROOT_TOWN), MAP_NUM(MAP_LITTLEROOT_TOWN), WARP_ID_NONE,
                               RUN0_LITTLEROOT_TERRY_X, RUN0_LITTLEROOT_TERRY_Y + 1);
            WarpNow();
            return;
        }
        sPendingScene = test;
    }
    // The first free frame after the game was turned on: a reset's line.
    if (!sBootChecked)
    {
        sBootChecked = TRUE;
        if (Rogue_IsRun0())
        {
            struct RogueDirector *d = Rogue_Director();
            if (d != NULL && d->resetPending)
                sPendingScene = RUN0_SCENE_RESET;
            Rogue_ReleaseDirector();
        }
    }
    if (FlagGet(RUN0_FLAG_DRAFT_SEEN))
    {
        FlagClear(RUN0_FLAG_DRAFT_SEEN);
        if (sPendingScene == RUN0_SCENE_NONE)
            sPendingScene = RUN0_SCENE_DRAFT;
    }
    if (Rogue_IsEndless() && !FlagGet(RUN0_FLAG_ENDLESS_TOLD) && Rogue_GetCurrentPlace() >= 0)
    {
        FlagSet(RUN0_FLAG_ENDLESS_TOLD);
        if (sPendingScene == RUN0_SCENE_NONE)
            sPendingScene = RUN0_SCENE_ENDLESS;
    }
    if (FlagGet(RUN0_FLAG_STOP_WALK))
        Rogue_Run0StopWalkStep();
    scene = sPendingScene;
    if (scene == RUN0_SCENE_NONE)
        return;
    sPendingScene = RUN0_SCENE_NONE;
    gSpecialVar_0x8004 = scene;
    switch (scene)
    {
    case RUN0_SCENE_CRASH:
    {
        struct RogueDirector *d = Rogue_Director();
        bool32 due = d != NULL && !BeatUsed(d, RUN0_BEAT_FAKE_CRASH);
        Rogue_ReleaseDirector();
        if (due)
            ScriptContext_SetupScript(Rogue_EventScript_Run0Crash);
        break;
    }
    case RUN0_SCENE_SWITCH:
        ScriptContext_SetupScript(Rogue_EventScript_Run0Switch);
        break;
    case RUN0_SCENE_ENDLESS_GATE:
        ScriptContext_SetupScript(Rogue_EventScript_Run0EndlessGate);
        break;
    case RUN0_SCENE_ENDING:
        ScriptContext_SetupScript(Rogue_EventScript_Run0TestQuestion);
        break;
    case RUN0_SCENE_STOP_WALK:
        break;
    default:
        ScriptContext_SetupScript(Rogue_EventScript_Run0Scene);
        break;
    }
}

// ---------------------------------------------------------------------------
// The fake crash: a freeze with the sound stuck on one frame, black, then
// Emerald's own boot to a title and a menu with only RUN 0 on it, and back
// to the gym's door. No reset, nothing saved or loaded: the game in memory
// is the game it returns to.

static u16 sCrashFrames;

static void CB2_Run0Frozen(void)
{
    // The picture stays as it was; only the sound is stuck.
    if (++sCrashFrames == 110)
    {
        SetGpuReg(REG_OFFSET_DISPCNT, 0);
    }
    else if (sCrashFrames == 150)
    {
        gRogueSoundStuck = FALSE;
        m4aMPlayAllStop();
        m4aSoundVSyncOff();
        m4aSoundVSyncOn();
    }
    else if (sCrashFrames >= 200)
    {
        sFakeBoot = RUN0_FAKE_BOOT_CRASH;
        gMain.state = 0;
        SetMainCallback2(CB2_InitCopyrightScreenAfterTitleScreen);
    }
}

// Script special (Rogue_EventScript_Run0Crash), followed by waitstate.
void Rogue_Run0FakeCrash(void)
{
    struct RogueDirector *d = Rogue_Director();
    UseBeat(d, RUN0_BEAT_FAKE_CRASH);
    Rogue_SaveDirector();
    sCrashFrames = 0;
    gRogueSoundStuck = TRUE;
    SetMainCallback2(CB2_Run0Frozen);
}

bool32 Rogue_Run0FakeBoot(void)
{
    return sFakeBoot != FALSE;
}

// The fake corrupted save's boot: no title, the main menu's own error box.
bool32 Rogue_Run0FakeBootSave(void)
{
    return sFakeBoot == RUN0_FAKE_BOOT_SAVE;
}

// Script special (Rogue_EventScript_Run0FakeSave, the screen black),
// followed by waitstate: the main menu as Emerald boots it with a save
// it could not read, then back to the field (Rogue_Run0FakeBootReturn).
void Rogue_Run0FakeSaveBoot(void)
{
    sFakeBoot = RUN0_FAKE_BOOT_SAVE;
    m4aMPlayAllStop();
    gMain.state = 0;
    SetMainCallback2(CB2_InitMainMenu);
}

// The menu's RUN 0, picked: back to the field, where the script waits.
void Rogue_Run0FakeBootReturn(void)
{
    sFakeBoot = FALSE;
    m4aMPlayAllStop();
    // The map's song starts again from the top: it is still the current
    // one, so the field would leave the stopped player as it is (silent,
    // and a fade out a warp waits for would never end).
    StopMapMusic();
    SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic);
}

// ---------------------------------------------------------------------------
// The fake corrupted save: once, in act 3, after a real save has finished.

bool32 Rogue_Run0FakeSaveDue(void)
{
    struct RogueDirector *d;
    bool32 due;
    if (!Rogue_IsRun0() || VarGet(VAR_ROGUE_RUN0_ACT) != 3)
        return FALSE;
    d = Rogue_Director();
    due = d != NULL && !BeatUsed(d, RUN0_BEAT_FAKE_SAVE);
    if (due)
    {
        // Written after the save that just finished, in the Director's own
        // sector: the save itself stays as it was written.
        UseBeat(d, RUN0_BEAT_FAKE_SAVE);
        Rogue_SaveDirector();
    }
    else
    {
        Rogue_ReleaseDirector();
    }
    return due;
}

// ---------------------------------------------------------------------------
// TERRY's switch, and the Director's battle

// Script special: Terry is deleted.
void Rogue_Run0DeleteTerry(void)
{
    struct RogueDirector *d = Rogue_Director();
    struct RogueRun0Story *s = Rogue_Run0Story(d);
    FlagSet(FLAG_ROGUE_RUN0_TERRY_DELETED);
    FlagClear(RUN0_FLAG_SWITCH_READY);
    if (s != NULL)
        s->terryDeleted = TRUE;
    Rogue_SaveDirector();
}

// Script special after the Director's first team: the patch. Its second
// team, the party healed ("It's only fair."), the box into gStringVar4.
void Rogue_Run0DirectorPatch(void)
{
    struct RogueDirFacts f;
    struct FactBufs *b = Bufs();
    // (Its second team is the challenge's: the trainer is beaten once,
    // Rogue_Run0DirectorPhase. Made again for the second battle.)
    Rogue_InvalidateTrainer();
    HealPlayerParty();
    if (b == NULL)
        return;
    BaseFacts(&f, b);
    gStringVar4[0] = EOS;
    SayMore(gStringVar4, sizeof(gStringVar4), DIR_PATCH101, &f);
    SayMore(gStringVar4, sizeof(gStringVar4), DIR_PATCH_THANKS, &f);
    SayMore(gStringVar4, sizeof(gStringVar4), DIR_PHASE2_HEAL, &f);
    FreeBufs();
}

// ---------------------------------------------------------------------------
// A list on the field: the endings' answer, CHOOSE's six regions

#define tWindow   data[0]
#define tCount    data[1]
#define tPicked   data[2]
#define tStep     data[3]
#define tTimer    data[4]
#define tChoose   data[5]

static const u8 *const sEndingAnswers[] =
{
    COMPOUND_STRING("STOP"), COMPOUND_STRING("MORE"), COMPOUND_STRING("CHOOSE"),
};

static void ListPrint(u8 taskId, s32 skip)
{
    s16 *data = gTasks[taskId].data;
    struct RogueDirector *d;
    struct RogueRun0Story *s;
    u32 i;
    u8 name[ROGUE_REGION_NAME_LEN + 2];
    FillWindowPixelBuffer(tWindow, PIXEL_FILL(1));
    d = tChoose ? Rogue_Director() : NULL;
    s = Rogue_Run0Story(d);
    for (i = 0; i < (u32)tCount; i++)
    {
        const u8 *text;
        // Gone already: the ones before step, but not the one kept.
        if (skip >= 0 && i != (u32)tPicked && (s32)i < skip + (i > (u32)tPicked ? 1 : 0))
            continue;
        if (tChoose)
        {
            Rogue_RegionName(name, s != NULL ? s->chooseSeeds[i] : i);
            text = name;
        }
        else
        {
            text = sEndingAnswers[i];
        }
        AddTextPrinterParameterized(tWindow, FONT_NORMAL, text, 8, 1 + i * 16, TEXT_SKIP_DRAW, NULL);
    }
    Rogue_ReleaseDirector();
    CopyWindowToVram(tWindow, COPYWIN_FULL);
}

static void Task_Run0List(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    s8 input;
    switch (tStep)
    {
    case 0:
        input = Menu_ProcessInputNoWrap();
        if (input == MENU_NOTHING_CHOSEN || input == MENU_B_PRESSED)
            return;
        tPicked = input;
        gSpecialVar_Result = input;
        if (!tChoose)
        {
            tStep = 99;
            return;
        }
        // The five not kept go, one by one.
        ClearStdWindowAndFrameToTransparent(tWindow, FALSE);
        SetStandardWindowBorderStyle(tWindow, FALSE);
        tStep = 1;
        tTimer = 40;
        ListPrint(taskId, 0);
        return;
    case 99:
        ClearStdWindowAndFrameToTransparent(tWindow, TRUE);
        RemoveWindow(tWindow);
        DestroyTask(taskId);
        ScriptContext_Enable();
        return;
    default:
        if (--tTimer > 0)
            return;
        if (tStep > 5)
        {
            tStep = 99;
            return;
        }
        PlaySE(SE_PC_OFF);
        ListPrint(taskId, tStep);
        tStep++;
        tTimer = 36;
        return;
    }
}

static void OpenList(bool32 choose, u32 count)
{
    u8 taskId = CreateTask(Task_Run0List, 80);
    s16 *data = gTasks[taskId].data;
    tChoose = choose;
    tCount = count;
    tStep = 0;
    tPicked = 0;
    tWindow = CreateWindowFromRect(choose ? 17 : 21, 1, choose ? 10 : 7, count * 2);
    SetStandardWindowBorderStyle(tWindow, FALSE);
    ListPrint(taskId, -1);
    InitMenuInUpperLeftCornerNormal(tWindow, count, 0);
    ScheduleBgCopyTilemapToVram(0);
}

#undef tWindow
#undef tCount
#undef tPicked
#undef tStep
#undef tTimer
#undef tChoose

// Script special (then waitstate): STOP, MORE or CHOOSE into VAR_RESULT.
void Rogue_Run0AskEnding(void)
{
    OpenList(FALSE, ARRAY_COUNT(sEndingAnswers));
}

static void RecordEnding(u32 ending)
{
    struct RogueDirector *d = Rogue_Director();
    if (d == NULL)
        return;
    d->ending = ending;
    d->endingsSeen |= 1 << ending;
    Rogue_SaveDirector();
}

// ---------------------------------------------------------------------------
// STOP: the walk home, Terry by the truck, the credits

// The door to the real Littleroot's island: its town, and the door's spot.
static bool32 IslandDoor(u32 *town, u32 *x, u32 *y)
{
    const struct RogueRegion *r = Region();
    const struct RogueMap *m;
    s32 place = RogueGen_Run0IslandTown(r, RUN0_ISLAND_LITTLEROOT);
    u32 k, i;
    if (place < 0 || (m = Rogue_GetPlaceMap(place)) == NULL)
        return FALSE;
    for (k = 0; k < m->nHouses; k++)
    {
        if (RogueGen_Run0HouseHook(r, m, k) != RUN0_HOUSE_ISLAND + RUN0_ISLAND_LITTLEROOT)
            continue;
        for (i = 0; i < m->nWarps; i++)
        {
            if (m->warps[i].kind == ROGUE_HOUSE_WARP(k))
            {
                *town = place;
                *x = m->warps[i].x;
                *y = m->warps[i].y;
                return TRUE;
            }
        }
    }
    return FALSE;
}

// Script special (then waitstate): STOP. The Director remembers it; the
// player is set down in the town of the door home.
void Rogue_Run0StartStop(void)
{
    u32 town, x, y;
    RecordEnding(ROGUE_RUN0_ENDING_STOP);
    VarSet(VAR_ROGUE_RUN0_ACT, ROGUE_RUN0_ACT_ENDING);
    FlagSet(RUN0_FLAG_STOP_WALK);
    FlagSet(RUN0_FLAG_TRUCK_TERRY);
    if (IslandDoor(&town, &x, &y))
        Rogue_SetFlyWarp(town);
    else
        Rogue_SetFlyWarp(Region()->start);
    WarpNow();
}

static u8 sEraseTimer;
#define STOP_BLACK 0x1E3 // General's metatile drawn as nothing on the black backdrop (RUN0_BLACK_METATILE)

static u32 Dist(s32 x1, s32 y1, s32 x2, s32 y2)
{
    s32 dx = x1 > x2 ? x1 - x2 : x2 - x1, dy = y1 > y2 ? y1 - y2 : y2 - y1;
    return dx > dy ? dx : dy;
}

// Each free frame while walking home: the map behind turns black, a few
// blocks at a time, all that is further from the door than the player.
void Rogue_Run0StopWalkStep(void)
{
    const struct RogueMap *m;
    u32 town, doorX, doorY, x, y, n = 0, near, i;
    s32 place = Rogue_GetCurrentPlace();
    if (++sEraseTimer < 3)
        return;
    sEraseTimer = 0;
    if (place < 0 || gSaveBlock1Ptr->location.mapGroup != ROGUE_GROUP || gSaveBlock1Ptr->location.mapNum >= ROGUE_MAX_PLACES
     || !IslandDoor(&town, &doorX, &doorY) || (u32)place != town || (m = Rogue_GetPlaceMap(place)) == NULL)
        return;
    near = Dist(gSaveBlock1Ptr->pos.x, gSaveBlock1Ptr->pos.y, doorX, doorY) + 1;
    for (y = 0; y < m->h && n < 24; y++)
    {
        for (x = 0; x < m->w && n < 24; x++)
        {
            if (Dist(x, y, doorX, doorY) <= near)
                continue;
            // The black the region draws past its edges (src/rogue_gen_run0.h).
            if ((MapGridGetMetatileIdAt(x + MAP_OFFSET, y + MAP_OFFSET) & MAPGRID_METATILE_ID_MASK) == STOP_BLACK)
                continue;
            MapGridSetMetatileIdAt(x + MAP_OFFSET, y + MAP_OFFSET, STOP_BLACK | MAPGRID_COLLISION_MASK);
            CurrentMapDrawMetatileAt(x + MAP_OFFSET, y + MAP_OFFSET);
            n++;
        }
    }
    // The townspeople go with their blocks (they stood on the black).
    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
    {
        struct ObjectEvent *o = &gObjectEvents[i];
        if (!o->active || o->isPlayer)
            continue;
        if ((MapGridGetMetatileIdAt(o->currentCoords.x, o->currentCoords.y) & MAPGRID_METATILE_ID_MASK) == STOP_BLACK
         || (MapGridGetMetatileIdAt(o->previousCoords.x, o->previousCoords.y) & MAPGRID_METATILE_ID_MASK) == STOP_BLACK)
            RemoveObjectEvent(o);
    }
}

// The real Littleroot in the STOP ending: TERRY by the truck. A template
// added after the map's own (the header's count is one more, see
// Rogue_Run0StoryHeader).
extern const u8 Rogue_EventScript_Run0TruckTerry[];
static struct MapEvents sLittlerootEvents;

void Rogue_Run0StoryHeader(struct MapHeader *h, u16 group, u16 num)
{
    if (!Rogue_IsRun0() || !FlagGet(RUN0_FLAG_TRUCK_TERRY) || group != MAP_GROUP(MAP_LITTLEROOT_TOWN)
     || num != MAP_NUM(MAP_LITTLEROOT_TOWN) || h->events->objectEventCount >= OBJECT_EVENT_TEMPLATES_COUNT)
        return;
    sLittlerootEvents = *h->events;
    sLittlerootEvents.objectEventCount++;
    h->events = &sLittlerootEvents;
}

void Rogue_Run0StoryObjects(void)
{
    struct ObjectEventTemplate *t;
    u32 n;
    if (gMapHeader.events != &sLittlerootEvents)
        return;
    n = sLittlerootEvents.objectEventCount - 1;
    t = &gSaveBlock1Ptr->objectEventTemplates[n];
    memset(t, 0, sizeof(*t));
    t->localId = 30;
    t->graphicsId = OBJ_EVENT_GFX_MAN_4;
    t->kind = OBJ_KIND_NORMAL;
    t->x = RUN0_LITTLEROOT_TERRY_X;
    t->y = RUN0_LITTLEROOT_TERRY_Y;
    t->elevation = 3;
    t->movementType = MOVEMENT_TYPE_FACE_DOWN;
    t->script = Rogue_EventScript_Run0TruckTerry;
}

// The one of Terry's team he leaves a player who has nobody else in the
// party (a 20-loss player who played with his team alone): the one that led
// the most battles this attempt, else the highest level. -1: none to leave.
static s32 TerryLeaves(struct RogueDirector *d)
{
    const struct RogueRun0Challenge *c = d != NULL ? RUN0_CHALLENGE(d) : NULL;
    s32 best = -1;
    u32 i, j, score, bestScore = 0;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        if (!GetMonData(mon, MON_DATA_SANITY_HAS_SPECIES) || GetMonData(mon, MON_DATA_IS_EGG))
            continue;
        if (!Rogue_IsTerrysMon(mon))
            return -1;
        score = GetMonData(mon, MON_DATA_LEVEL);
        for (j = 0; c != NULL && j < RUN0_CHALLENGE_LEADS; j++)
            if (c->leads[j].species == GetMonData(mon, MON_DATA_SPECIES))
                score += c->leads[j].count * 256;
        if (best < 0 || score > bestScore)
        {
            best = i;
            bestScore = score;
        }
    }
    return best;
}

// Whether any POKeMON the Director kept will come back to the player (those
// lost with Terry's team do not).
static bool32 KeptWillComeBack(struct RogueDirector *d)
{
    u32 i;
    for (i = 0; d != NULL && i < d->keptCount && i < ROGUE_KEPT_MAX; i++)
    {
        struct RogueKeptMon *k = &d->kept[i];
        if (k->species != SPECIES_NONE && !(k->flags & (ROGUE_KEPT_RETURNED | ROGUE_KEPT_TERRY)))
            return TRUE;
    }
    return FALSE;
}

// Terry's team out of the party and the PC (they go home with him), and
// what the Director kept back to the player. Returns how many came back;
// left is the nickname of the one Terry left (EOS if none): only when
// nobody comes back, so the player is never left with no POKeMON.
static u32 GiveBack(struct RogueDirector *d, bool32 *lost, u8 *left)
{
    u32 i, box, pos, back = 0;
    s32 leave = KeptWillComeBack(d) ? -1 : TerryLeaves(d);
    struct Pokemon *mon = AllocZeroed(sizeof(struct Pokemon));
    *lost = FALSE;
    left[0] = EOS;
    if (leave >= 0)
        GetMonData(&gParties[B_TRAINER_PLAYER][leave], MON_DATA_NICKNAME, left);
    for (i = 0; i < PARTY_SIZE; i++)
        if ((s32)i != leave && GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_SANITY_HAS_SPECIES)
         && Rogue_IsTerrysMon(&gParties[B_TRAINER_PLAYER][i]))
            ZeroMonData(&gParties[B_TRAINER_PLAYER][i]);
    for (box = 0; box < TOTAL_BOXES_COUNT; box++)
    {
        for (pos = 0; pos < IN_BOX_COUNT; pos++)
        {
            struct BoxPokemon *b = GetBoxedMonPtr(box, pos);
            if (GetBoxMonData(b, MON_DATA_SANITY_HAS_SPECIES) && GetBoxMonData(b, MON_DATA_OT_ID) == TERRY_OT_ID)
                ZeroBoxMonData(b);
        }
    }
    CompactPartySlots();
    CalculatePlayerPartyCount();
    for (i = 0; d != NULL && i < d->keptCount && i < ROGUE_KEPT_MAX; i++)
    {
        struct RogueKeptMon *k = &d->kept[i];
        if (k->species == SPECIES_NONE || (k->flags & ROGUE_KEPT_RETURNED))
            continue;
        k->flags |= ROGUE_KEPT_RETURNED;
        if (k->flags & ROGUE_KEPT_TERRY)
        {
            // Lost on the way: Terry takes the memory, not the POKeMON.
            *lost = TRUE;
            continue;
        }
        if (mon == NULL)
            continue;
        Rogue_Run0BuildKeptMon(mon, k, k->level); // its own level, full HP and moves
        if (GiveCapturedMonToPlayer(mon) != MON_CANT_GIVE)
            back++;
    }
    TRY_FREE_AND_SET_NULL(mon);
    return back;
}

// Script special: Terry by the truck. His words into gStringVar4; his team
// goes home with him, and the Director's kept POKeMON come back.
void Rogue_Run0TruckTerry(void)
{
    struct RogueDirector *d = Rogue_Director();
    struct RogueRun0Story *s = Rogue_Run0Story(d);
    struct RogueDirFacts f;
    struct FactBufs *b = Bufs();
    bool32 lost;
    u32 back;
    u8 left[POKEMON_NAME_LENGTH + 1];

    if (b == NULL || s == NULL)
    {
        Rogue_ReleaseDirector();
        FreeBufs();
        return;
    }
    BaseFacts(&f, b);
    gStringVar4[0] = EOS;
    if (s->truckDone)
    {
        Say(DIR_TERRY_TRUCK_TAKE, &f);
        Rogue_ReleaseDirector();
        FreeBufs();
        return;
    }
    back = GiveBack(d, &lost, left);
    s->truckDone = TRUE;
    s->cameHome = back;
    SayMore(gStringVar4, sizeof(gStringVar4), lost ? DIR_TERRY_TRUCK_LOST : DIR_TERRY_TRUCK, &f);
    if (left[0] != EOS)
    {
        // His team was all the player had: he leaves them the one they
        // led with most.
        SetNick(&f, b, left);
        SayMore(gStringVar4, sizeof(gStringVar4), DIR_TERRY_TRUCK_KEEP, &f);
    }
    if (back != 0)
    {
        f.n = back;
        SayMore(gStringVar4, sizeof(gStringVar4), DIR_CAME_HOME, &f);
    }
    SayMore(gStringVar4, sizeof(gStringVar4), DIR_TERRY_TRUCK_TAKE, &f);
    Rogue_SaveDirector();
    FreeBufs();
}

static bool8 sCreditsRun0;

// Script special: into the truck, the door shuts, the credits.
void Rogue_Run0StopCredits(void)
{
    FlagClear(RUN0_FLAG_STOP_WALK);
    sCreditsRun0 = TRUE;
    SetMainCallback2(CB2_StartCreditsSequence);
}

bool32 Rogue_Run0Credits(void)
{
    return sCreditsRun0;
}

// The credits are over: the run ends as a win (src/rogue_run.c).
void Rogue_Run0CreditsDone(void)
{
    sCreditsRun0 = FALSE;
    Rogue_GameClear();
}

// ---------------------------------------------------------------------------
// MORE, and ENDLESS

static struct Pokemon *sEndlessParty; // the party carried to the next region (heap)

static void CarryParty(void)
{
    TRY_FREE_AND_SET_NULL(sEndlessParty);
    sEndlessParty = Alloc(sizeof(struct Pokemon) * PARTY_SIZE);
    if (sEndlessParty != NULL)
        memcpy(sEndlessParty, gParties[B_TRAINER_PLAYER], sizeof(struct Pokemon) * PARTY_SIZE);
}

static void StartEndlessRegion(void)
{
    struct RogueDirector *d = Rogue_Director();
    struct RogueRun0Story *s = Rogue_Run0Story(d);
    if (s != NULL)
    {
        s->endlessCarry = TRUE;
        s->endlessDepth = Rogue_IsEndless() ? s->endlessDepth + 1 : 1;
    }
    Rogue_SaveDirector();
    CarryParty();
    Rogue_PrepareEndlessRun();
    SetMainCallback2(CB2_NewGame);
}

void Rogue_Run0FinishWon(void); // src/rogue_run.c

// Script special: MORE. RUN 0 is won (saved, and the Director told), then a
// new region at once, with no credits.
void Rogue_Run0StartMore(void)
{
    RecordEnding(ROGUE_RUN0_ENDING_MORE);
    Rogue_Run0FinishWon();
    StartEndlessRegion();
}

// Script special at an ENDLESS region's end, where the LEAGUE would be.
void Rogue_Run0NextEndless(void)
{
    StartEndlessRegion();
}

// An ENDLESS region the party was carried into stands on a band of its own,
// as RUN 0's (src/rogue_run0_trainers.h): from the party's median level, 60
// at least, two higher each region on, to 100 by its last gym.
static void EndlessBand(void)
{
    u8 levels[PARTY_SIZE];
    u32 n = 0, i, j, base, depth = Rogue_Run0EndlessDepth();
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
    base = n != 0 ? (levels[(n - 1) / 2] + levels[n / 2] + 1) / 2 : 60;
    if (base < 60)
        base = 60;
    base += 2 * (depth > 1 ? (depth - 1 < 8 ? depth - 1 : 8) : 0);
    if (base > ROGUE_RUN0_ENDLESS_BASE_MAX)
        base = ROGUE_RUN0_ENDLESS_BASE_MAX;
    VarSet(VAR_ROGUE_RUN0_BAND, ROGUE_RUN0_BAND_VALUE(base, TYPE_NONE, TYPE_NONE));
}

// Rogue_NewGame, any run: an ENDLESS region keeps the party it was given.
void Rogue_Run0StoryNewRun(void)
{
    struct RogueDirector *d;
    struct RogueRun0Story *s;
    sDepthCache = 0;
    if (!Rogue_IsRun0())
        VarSet(VAR_ROGUE_RUN0_BAND, 0);
    // After STOP, every normal region parks the truck in its first town.
    if (Rogue_Run0TruckParked())
        FlagSet(RUN0_FLAG_TRUCK_PARKED);
    if (sEndlessParty == NULL)
        return;
    if (Rogue_IsEndless())
    {
        memcpy(gParties[B_TRAINER_PLAYER], sEndlessParty, sizeof(struct Pokemon) * PARTY_SIZE);
        CalculatePlayerPartyCount();
        EndlessBand();
        FlagSet(FLAG_SYS_POKEMON_GET);
        VarSet(VAR_ROGUE_STARTER, ROGUE_STARTER_VALUE(ROGUE_STARTER_KIND_CHAMPION,
                                                      GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES)));
    }
    TRY_FREE_AND_SET_NULL(sEndlessParty);
    d = Rogue_Director();
    s = Rogue_Run0Story(d);
    if (s != NULL)
        s->endlessCarry = FALSE;
    Rogue_SaveDirector();
}

// The main menu's ENDLESS (after MORE): a first region of its own, with
// the professor's starter.
void Rogue_Run0MenuEndless(void)
{
    struct RogueDirector *d = Rogue_Director();
    struct RogueRun0Story *s = Rogue_Run0Story(d);
    if (s != NULL)
    {
        s->endlessDepth = 1;
        s->endlessCarry = FALSE;
    }
    Rogue_SaveDirector();
    TRY_FREE_AND_SET_NULL(sEndlessParty);
    Rogue_PrepareEndlessRun();
}

bool32 Rogue_Run0EndlessUnlocked(void)
{
    bool32 was = Rogue_IsDirectorLoaded();
    struct RogueDirector *d = Rogue_Director();
    bool32 yes = d != NULL && (d->endingsSeen & (1 << ROGUE_RUN0_ENDING_MORE));
    if (!was)
        Rogue_ReleaseDirector();
    return yes;
}

// How strange an ENDLESS region is: its number in the chain, 0 outside.
// (Asked every frame by the music: read from the Director once a run.)

u32 Rogue_Run0EndlessDepth(void)
{
    struct RogueDirector *d;
    struct RogueRun0Story *s;
    if (!Rogue_IsEndless())
        return 0;
    if (sDepthCache == 0)
    {
        d = Rogue_Director();
        s = Rogue_Run0Story(d);
        sDepthCache = s != NULL && s->endlessDepth ? s->endlessDepth : 1;
        Rogue_ReleaseDirector();
    }
    return sDepthCache;
}

// ---------------------------------------------------------------------------
// CHOOSE

// Script special (then waitstate): six regions; the one kept in VAR_RESULT.
void Rogue_Run0Choose(void)
{
    struct RogueDirector *d = Rogue_Director();
    struct RogueRun0Story *s = Rogue_Run0Story(d);
    u32 i, j;
    if (s != NULL)
    {
        for (i = 0; i < 6; i++)
        {
            u16 seed;
            do
            {
                seed = Random() % ROGUE_REGION_COUNT;
                for (j = 0; j < i && s->chooseSeeds[j] != seed; j++)
                    ;
            } while (j < i);
            s->chooseSeeds[i] = seed;
        }
    }
    Rogue_SaveDirector();
    OpenList(TRUE, 6);
}

// Script special: the region kept, the Director's last words into gStringVar4.
void Rogue_Run0ChooseDone(void)
{
    struct RogueDirector *d = Rogue_Director();
    struct RogueRun0Story *s = Rogue_Run0Story(d);
    struct RogueDirFacts f;
    struct FactBufs *b = Bufs();
    if (s != NULL)
    {
        s->keptRegion = s->chooseSeeds[gSpecialVar_Result % 6];
        s->hasKeptRegion = TRUE;
        d->ending = ROGUE_RUN0_ENDING_CHOOSE;
        d->endingsSeen |= 1 << ROGUE_RUN0_ENDING_CHOOSE;
    }
    Rogue_SaveDirector();
    if (b == NULL)
        return;
    BaseFacts(&f, b);
    Say(DIR_CHOOSE_DONE, &f);
    FreeBufs();
}

// The kept region, remembered (every line of a region's text with its name
// asks): 0 not read yet, 1 none, else its seed + 2. Forgotten whenever the
// Director is written (Rogue_SaveDirector).
static EWRAM_DATA u16 sKeptRegion = 0;

void Rogue_Run0ForgetKeptRegion(void)
{
    sKeptRegion = 0;
}

bool32 Rogue_Run0KeptRegion(u16 *seed)
{
    if (sKeptRegion == 0)
    {
        // (Asked while a caller holds the Director, as the RUN 0 start does
        // when it names the boxes: keep it loaded then, or the caller's
        // changes go.)
        bool32 was = Rogue_IsDirectorLoaded();
        struct RogueDirector *d = Rogue_Director();
        struct RogueRun0Story *s = Rogue_Run0Story(d);
        sKeptRegion = (s != NULL && s->hasKeptRegion) ? s->keptRegion + 2 : 1;
        if (!was)
            Rogue_ReleaseDirector();
    }
    if (sKeptRegion < 2)
        return FALSE;
    *seed = sKeptRegion - 2;
    return TRUE;
}

// ---------------------------------------------------------------------------
// After the endings: PROF. BIRCH, the truck, the card's star, the summary

static char sBirchLine[96];

// PROF. BIRCH's welcome at the next normal run, once after each ending
// (Rogue_BirchWelcomeBack): ASCII, or NULL.
const char *Rogue_Run0BirchEndingLine(void)
{
    struct RogueDirector *d;
    struct RogueRun0Story *s;
    struct RogueDirFacts f;
    struct FactBufs *b;
    const char *line = NULL;
    u32 ending;
    // (Said in the speech before a normal run, while the run kind is still
    // the last run's.)
    d = Rogue_Director();
    s = Rogue_Run0Story(d);
    ending = d != NULL ? d->ending : 0;
    if (s == NULL || ending == ROGUE_RUN0_ENDING_NONE || (s->birchTold & (1 << ending)) || (b = Bufs()) == NULL)
    {
        Rogue_ReleaseDirector();
        return NULL;
    }
    s->birchTold |= 1 << ending;
    BaseFacts(&f, b);
    if (DirFill(sBirchLine, sizeof(sBirchLine), sDirLines[ending == ROGUE_RUN0_ENDING_STOP ? DIR_BIRCH_AFTER_STOP
                                                      : ending == ROGUE_RUN0_ENDING_MORE ? DIR_BIRCH_AFTER_MORE
                                                      : DIR_BIRCH_AFTER_CHOOSE], &f))
        line = sBirchLine;
    Rogue_SaveDirector();
    FreeBufs();
    return line;
}

// The same, as the whole welcome after a RUN 0 run (in the game's
// characters, wrapped): FALSE if there is none.
bool32 Rogue_Run0BirchEndingWelcome(u8 *dest, u32 size)
{
    const char *line = Rogue_Run0BirchEndingLine();
    char *out = (char *)dest;
    u32 n;
    if (line == NULL || size < 8)
        return FALSE;
    for (n = 0; line[n] && n + 1 < size; n++)
        out[n] = line[n];
    out[n] = '\0';
    DirWrap(out);
    Rogue_CopyLoreAscii(dest, out);
    return TRUE;
}

// The moving truck in every normal region's first town, after STOP.
bool32 Rogue_Run0TruckParked(void)
{
    struct RogueDirector *d;
    bool32 yes, was;
    if (Rogue_GetRunKind() != ROGUE_RUN_KIND_NORMAL)
        return FALSE;
    was = Rogue_IsDirectorLoaded();
    d = Rogue_Director();
    yes = d != NULL && (d->endingsSeen & (1 << ROGUE_RUN0_ENDING_STOP));
    if (!was)
        Rogue_ReleaseDirector();
    return yes;
}

bool32 Rogue_Run0FeatStar(void)
{
    bool32 was = Rogue_IsDirectorLoaded();
    struct RogueDirector *d = Rogue_Director();
    bool32 yes = d != NULL && d->feat;
    if (!was)
        Rogue_ReleaseDirector();
    return yes;
}

// The run summary's first page after a RUN 0 win, into dest.
bool32 Rogue_Run0SummaryPage(u8 *dest)
{
    struct RogueDirector *d;
    struct RogueRun0Story *s;
    u8 *str;
    if (!Rogue_IsRun0() || VarGet(VAR_ROGUE_RUN_STATE) != ROGUE_RUN_WON)
        return FALSE;
    d = Rogue_Director();
    s = Rogue_Run0Story(d);
    str = StringCopy(dest, COMPOUND_STRING("{COLOR BLUE}RUN 0{COLOR DARK_GRAY}\n\n"));
    if (d != NULL && d->ending == ROGUE_RUN0_ENDING_STOP)
    {
        str = StringCopy(str, COMPOUND_STRING("You stopped."));
        if (s != NULL && s->cameHome)
        {
            str = StringCopy(str, COMPOUND_STRING("\n"));
            str = ConvertIntToDecimalStringN(str, s->cameHome, STR_CONV_MODE_LEFT_ALIGN, 2);
            str = StringCopy(str, COMPOUND_STRING(" POKéMON came home."));
        }
    }
    else if (d != NULL && d->ending == ROGUE_RUN0_ENDING_CHOOSE && s != NULL)
    {
        str = StringCopy(str, COMPOUND_STRING("You kept one region.\nIt's called "));
        str = StringCopy(str, gSaveBlock2Ptr->playerName);
        str = StringCopy(str, COMPOUND_STRING(" now."));
    }
    else
    {
        str = StringCopy(str, COMPOUND_STRING("You won."));
    }
    StringCopy(str, COMPOUND_STRING("\n\nThank you for playing."));
    Rogue_ReleaseDirector();
    return TRUE;
}

// ---------------------------------------------------------------------------
// The POKeDEX in RUN 0: what the player caught here has the Director's notes
// on the player instead of the entry (src/pokedex.c). One of a few notes for
// each species, with the save's own numbers.

const u8 *Rogue_Run0DexNote(u16 species)
{
    static const u8 sOrder[DIR_DEX_NOTES] = { 0, 1, 2, 3, 4 };
    struct RogueDirector *d;
    struct FactBufs *b;
    struct RogueDirFacts f;
    u32 note, percent, type, i, tries;
    bool32 was, ok = FALSE;

    if (!Rogue_IsRun0() || species == SPECIES_NONE || species >= NUM_SPECIES || (b = Bufs()) == NULL)
        return NULL;
    was = Rogue_IsDirectorLoaded();
    d = Rogue_Director();
    if (d == NULL)
    {
        FreeBufs();
        return NULL;
    }
    type = Rogue_Run0TopLeadType(&percent);
    note = RogueRng_Hash(species, 0xDE7) % DIR_DEX_NOTES;
    for (tries = 0; tries < DIR_DEX_NOTES && !ok; tries++, note = (note + 1) % DIR_DEX_NOTES)
    {
        BaseFacts(&f, b);
        SetMon(&f, b, species);
        switch (sOrder[note])
        {
        case 0: // caught this attempt
            f.n = d->catches;
            f.n2 = d->attempts;
            break;
        case 1: // battles this one led, and the type led with most
        {
            const struct RogueRun0Challenge *c = RUN0_CHALLENGE(d);
            if (percent == 0 || type >= NUMBER_OF_MON_TYPES)
                continue;
            f.n = 0;
            for (i = 0; i < RUN0_CHALLENGE_LEADS; i++)
                if (c->leads[i].species == species)
                    f.n = c->leads[i].count;
            Rogue_GameToAscii(b->name, gTypesInfo[type].name, POKEMON_NAME_LENGTH);
            for (i = 0; b->name[i]; i++)
                if (b->name[i] >= 'a' && b->name[i] <= 'z')
                    b->name[i] -= 'a' - 'A';
            f.type = b->name;
            f.n2 = percent;
            break;
        }
        case 2: // the Pokemon it kept
            if (d->keptCount == 0)
                continue;
            f.n = d->keptCount;
            break;
        case 4: // the times it was turned off in a battle
            if (d->resets == 0)
                continue;
            f.n = d->resets;
            break;
        }
        ok = SayInto(gStringVar4, sizeof(gStringVar4), DIR_DEX_0 + sOrder[note], &f);
    }
    if (!was)
        Rogue_ReleaseDirector();
    FreeBufs();
    return ok ? gStringVar4 : NULL;
}
