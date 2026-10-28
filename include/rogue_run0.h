#ifndef GUARD_ROGUE_RUN0_H
#define GUARD_ROGUE_RUN0_H

// RUN 0 (Roguémon v1.0): the secret last region, made by the Director, the
// thing that makes all the others. This header is the foundation's shared
// interface: the region, story and challenge work codes against these names.
// The code is in src/rogue_run0.c. The vars and flags are in
// include/constants/rogue.h (VAR_ROGUE_RUN_KIND, VAR_ROGUE_RUN0_ACT,
// FLAG_ROGUE_RUN0_*), the seed in include/rogue_gen.h (ROGUE_RUN0_SEED,
// RogueGen_IsRun0).

#include "constants/rogue.h"

struct Pokemon;
struct TrainerMon;

// ---------------------------------------------------------------------------
// The Director's save: a sector of its own
//
// struct RogueDirector lives in the Trainer Hill save sector
// (SECTOR_ID_TRAINER_HILL), which nothing in Roguémon writes (the Trainer
// Hill and the e-Reader cannot be reached). It is not part of the save
// slots, so it survives a new run, a new game and a reset. It is written on
// its own, by Rogue_SaveDirector, at the moments listed there, and after a
// normal save that changed what it notes of the save (struct
// RogueDirectorSaved): the Pokemon it keeps follow the save. A sector that is empty, from another
// version, or fails its checksum reads as a fresh Director.

#define ROGUE_DIRECTOR_MAGIC   0x30524944 // "DIR0"
#define ROGUE_DIRECTOR_VERSION 1
// The bytes a special save sector holds (SECTOR_COUNTER_OFFSET in save.h).
#define ROGUE_DIRECTOR_SIZE    4092

#define ROGUE_KEPT_MAX       30 // Pokemon the Director keeps (lost in RUN 0)
#define ROGUE_RUNLOG_MAX     26 // runs in the run log (a ring: the oldest goes)
#define ROGUE_RUNLOG_LOST    6  // Pokemon lost kept per run in the log
#define ROGUE_DIRECTOR_TYPES 24 // lead type counts, by enum Type
#define ROGUE_DIRECTOR_TOWN_LEN 16 // a place's name (ROGUE_NAME_LEN)
#define ROGUE_DIRECTOR_STORY_BYTES     256 // free for the story's own fields
#define ROGUE_DIRECTOR_CHALLENGE_BYTES 256 // free for the challenge's own fields

// A Pokemon the Director keeps: one that fainted in RUN 0. It turns up on the
// Director's trainers with the player's nickname, and comes back after STOP.
struct RogueKeptMon
{
    u32 personality;
    u16 species;              // SPECIES_NONE: an empty slot
    u8 level;
    u8 flags;                 // ROGUE_KEPT_*
    u8 nickname[POKEMON_NAME_LENGTH]; // no EOS when the name is 12 long
}; // 20 bytes

#define ROGUE_KEPT_TERRY    (1 << 0) // one of TEST TEAM (Terry's)
#define ROGUE_KEPT_SHINY    (1 << 1)
#define ROGUE_KEPT_RETURNED (1 << 2) // given back (STOP, a badge, lent); kept for the record
#define ROGUE_KEPT_NEW      (1 << 3) // kept in the battle just played (not given back after it)

// A Pokemon lost in a run, for the run log (the graveyard tower reads it).
struct RogueLostMon
{
    u16 species;              // SPECIES_NONE: none
    u8 level;
    u8 nickname[POKEMON_NAME_LENGTH];
    u8 pad;
}; // 16 bytes

// One run, from v1.0 on. Written when the run starts (result ACTIVE), as
// its Pokemon are lost, and when it ends.
struct RogueRunLogEntry
{
    u16 seed;                 // the region (ROGUE_RUN0_SEED for RUN 0)
    u16 run;                  // its number (VAR_ROGUE_META_RUNS when it started)
    u8 result;                // ROGUE_LOG_*
    u8 badges;
    u8 place;                 // where it ended: a place, ROGUE_LAST_DUNGEON | dungeon, or 0xFF
    u8 kind;                  // ROGUE_RUN_KIND_*
    u8 lostCount;             // Pokemon lost in the run, all of them
    u8 mix;                   // ROGUE_MIX_*
    u8 pad[2];
    struct RogueLostMon lost[ROGUE_RUNLOG_LOST]; // the first six lost
}; // 108 bytes

#define ROGUE_LOG_ACTIVE 0 // still being played (or never ended: see ROGUE_LOG_LEFT)
#define ROGUE_LOG_LOST   1
#define ROGUE_LOG_WON    2
#define ROGUE_LOG_LEFT   3 // a new run was started while it was going

// Endings (the Director's last question).
#define ROGUE_RUN0_ENDING_NONE   0
#define ROGUE_RUN0_ENDING_STOP   1
#define ROGUE_RUN0_ENDING_MORE   2
#define ROGUE_RUN0_ENDING_CHOOSE 3

// The four teasers before RUN 0 shows (one bit each in teasers), each once.
#define ROGUE_TEASE_HOF_LEVEL (1 << 0) // win 1: "Lv. ??" for a frame in the HALL OF FAME
#define ROGUE_TEASE_TOWN      (1 << 1) // win 2: the next run's first town names a town of the last region
#define ROGUE_TEASE_GIVEN     (1 << 2) // win 3: "I've been given a new land"
#define ROGUE_TEASE_ROW       (1 << 3) // win 4 (or 20 losses): the menu row was seen

// Beats that happen once across attempts: bits in beats[]. The story adds
// its own after these (up to 64).
#define RUN0_BEAT_FAKE_CRASH  0
#define RUN0_BEAT_FAKE_SAVE   1
#define RUN0_BEAT_CLOCK       2
#define RUN0_BEAT_STORY_FIRST 8 // the first bit free for the story

// What the last normal save holds of the Director's keeping (the kept
// Pokemon and those given back, the challenge's turns of them, the faints and
// this run's lost Pokemon in the run log). The sector is written outside the
// save (at a battle's start and end, after a badge), so a session that ends
// without saving would leave the Director keeping a Pokemon the save still
// has: v1.0.0 kept one lost after the last save, the player turned the game
// off and loaded, and it was later given back as a second VIVILLON. Each
// normal save notes this (Rogue_DirectorNoteSave, from TrySavingData), and
// the next boot puts the keeping back to it (Rogue_Run0CheckReset).
struct RogueDirectorSaved
{
    u8 valid;                 // TRUE once a save was noted
    u8 keptCount;
    u8 keptAct;               // the challenge's keptAct, keptNext, keptShown and returned
    u8 keptNext;
    u32 keptShown;
    u32 returnedKept;         // a bit per kept Pokemon given back (ROGUE_KEPT_RETURNED)
    u16 returned;
    u16 faints;
    u16 runLogCount;          // the run log then, and its newest entry's lost Pokemon
    u8 lostFilled;            // lost[] entries filled
    u8 lostCount;
}; // 20 bytes

struct RogueDirector
{
    u32 magic;                // ROGUE_DIRECTOR_MAGIC
    u16 version;              // ROGUE_DIRECTOR_VERSION
    u16 checksum;             // over everything after it (Rogue_SaveDirector sets it)
    // RUN 0 attempts.
    u16 attempts;             // RUN 0 runs started
    u8 bestAct;               // the furthest act reached, across attempts
    u8 checkpointAct;         // the act the last attempt reached (offered next time)
    u8 offerCheckpoint;       // TRUE: the next attempt offers checkpointAct
    u8 startAct;              // the act the next attempt starts at (set by the offer)
    u8 ending;                // ROGUE_RUN0_ENDING_*: the last one chosen
    u8 endingsSeen;           // a bit per ending
    u8 feat;                  // TRUE once RUN 0 is won (the RUN 0 feat)
    u8 teasers;               // ROGUE_TEASE_*
    u8 battleDirty;           // TRUE while a RUN 0 battle runs (reset detection)
    u8 resetPending;          // a reset was found at boot; the story says its line and clears it
    u16 resets;               // RUN 0 battles cut short by turning the game off
    u16 resetTrainer;         // the last such battle's trainer id (0: a wild battle)
    u16 resetSpecies;         // its foe's lead
    u8 resetName[POKEMON_NAME_LENGTH + 1]; // the trainer's or the wild Pokemon's name then
    u8 keptCount;
    u8 pad0[2];
    u32 beats[2];             // RUN0_BEAT_* used, across attempts
    u32 patches;              // PATCH bits taken this attempt (the challenge's)
    // This attempt's play, for DATA COLLECTED and the patches.
    u16 battles;              // battles started
    u16 leadTypes[ROGUE_DIRECTOR_TYPES]; // battles led by a Pokemon of each type (both types count)
    u16 heals;                // POKeMON CENTER heals
    u16 itemsUsed;            // items used in battle
    u16 catches;
    u16 faints;
    struct RogueKeptMon kept[ROGUE_KEPT_MAX];
    // The run log: every run from v1.0, newest at (runLogCount - 1) % ROGUE_RUNLOG_MAX.
    u16 runLogCount;          // entries ever written
    u16 pad1;
    struct RogueRunLogEntry runLog[ROGUE_RUNLOG_MAX];
    // A town of the last run's region (where it ended, or its first town),
    // in the game's characters: the win 2 teaser names it.
    u8 lastTownName[ROGUE_DIRECTOR_TOWN_LEN + 1];
    // Word aligned, so a struct cast over them may hold u16 and u32 fields
    // (the ARM7 cannot store a halfword at an odd address).
    u8 story[ROGUE_DIRECTOR_STORY_BYTES] __attribute__((aligned(4)));         // the story's fields (its own struct over these bytes)
    u8 challenge[ROGUE_DIRECTOR_CHALLENGE_BYTES] __attribute__((aligned(4))); // the challenge's fields (struct RogueRun0Challenge)
    // The keeping as the last normal save has it (zero in a v1.0.0 sector:
    // none noted yet, nothing put back).
    struct RogueDirectorSaved saved;
};

// The Director as it stands in the save, on the heap. Load it, change it,
// then Rogue_SaveDirector (writes and frees) or Rogue_ReleaseDirector (frees,
// dropping changes). Do not keep the pointer across frames: a map load or a
// battle start drops a copy left loaded. Returns NULL only if the heap is out.
struct RogueDirector *Rogue_Director(void);
// Writes the loaded Director to its sector and frees it. Never call it while
// a normal save is being written (it is not: saves run to the end within a
// frame); call it right after one.
void Rogue_SaveDirector(void);
void Rogue_ReleaseDirector(void);
bool32 Rogue_IsDirectorLoaded(void);
// A normal save was just written (TrySavingData): the Director notes the
// keeping it holds, and writes its sector if that changed (at once, even
// with the Director loaded by the caller, who may still write it again).
// Called too after the Director's own changes that belong with a save just
// written (a run's end: the team kept, the run log).
void Rogue_DirectorNoteSave(void);
// InitHeap calls this: the heap was reset, so a loaded copy is gone.
void Rogue_OnHeapReset(void);

// ---------------------------------------------------------------------------
// The kind of run

bool32 Rogue_IsRun0(void);
bool32 Rogue_IsEndless(void);
// RUN 0, or an ENDLESS region a party was carried into: levels on a band.
bool32 Rogue_Run0Scaled(void);
u32 Rogue_GetRunKind(void);
// RUN 0 is earned: ROGUE_RUN0_WINS wins or ROGUE_RUN0_LOSSES losses.
bool32 Rogue_IsRun0Unlocked(void);
// The menu row: earned, no run in progress, and not left for good (STOP).
bool32 Rogue_ShowRun0Row(void);
// The menu: the next new game is RUN 0 (or ENDLESS, a stub for MORE).
void Rogue_PrepareRun0(void);
void Rogue_PrepareEndlessRun(void);
// Called by Rogue_NewGame for a RUN 0 run, after the region is built: the
// act, the teams, the Director's attempt and the run log.
void Rogue_Run0NewGame(void);
// The act a RUN 0 attempt starts at, with what came before it (the badges
// of the acts before). The region and story place the player (a hook).
void Rogue_Run0ApplyAct(u32 act);
// The main menu's RUN 0 row leads here: two seconds of black and silence,
// the Director's line with no name box, the checkpoint offer after a loss,
// then the run (CB2_NewGame). In src/rogue_ui.c.
void CB2_RogueRun0Intro(void);
// Script special at the end of the lab's arrival scene in RUN 0
// (Rogue_EventScript_Run0Arrival in data/scripts/rogue.inc, the story's to
// fill): sets FLAG_ROGUE_RUN0_ARRIVED and the run's progress.
void Rogue_Run0Arrived(void);

// ---------------------------------------------------------------------------
// Your Pokemon: the Hall of Fame imported

// The teams of the Hall of Fame (the last 30 wins), never changed. Returns
// how many there are.
u32 Rogue_CountHofTeams(void);
// Builds every Hall of Fame team with its personality, the player's OT,
// level-up moves (the last four), IVs from a hash of the personality and no
// items: the party is the latest team, every other team a box named after
// its region (from the run log) or TEAM n, and box TEST TEAM holds Terry's.
// A Pokemon the Director keeps is left out. With no Hall of Fame, TEST TEAM
// is the party. Returns the teams imported.
u32 Rogue_Run0ImportTeams(void);
#define ROGUE_TEST_TEAM_BOX 0 // box 1: TEST TEAM
// Is this Pokemon one of Terry's (TEST TEAM)?
bool32 Rogue_IsTerrysMon(struct Pokemon *mon);
// How many kept Pokemon the Director lent at this attempt's start (with
// fewer than six Pokemon in all), for the arrival's line.
u32 Rogue_Run0LentCount(void);

// ---------------------------------------------------------------------------
// Battles, losing and resets

// Battle start and end hooks (src/battle_main.c, Rogue_OnBattleEnd): the
// stats, the kept Pokemon and the reset marker. Normal runs only log.
void Rogue_Run0OnBattleStart(void);
void Rogue_Run0OnBattleEnd(void);
// A Pokemon fainted in a run: logged, and in RUN 0 kept by the Director.
void Rogue_Run0OnFainted(struct Pokemon *mon);
// At boot (the main menu): the Director's keeping goes back to what the save
// holds (a Pokemon kept or given back since it was written is not), and a
// RUN 0 battle that never ended is counted, with resetPending set for the
// story's line.
void Rogue_Run0CheckReset(void);
// The type RUN 0 was led with most this attempt, and its share in percent
// (0 if no battle): "You lead with WATER 58% of the time."
u32 Rogue_Run0TopLeadType(u32 *percent);
// The DATA COLLECTED page of a RUN 0 loss into dest (the summary's first page).
void Rogue_Run0BufferDataCollected(u8 *dest);
// A RUN 0 run ended: the Director takes the act reached as the next
// checkpoint. Called by Rogue_LoseRun; the summary writes the Director after
// its save.
void Rogue_Run0OnRunEnd(bool32 won);

// ---------------------------------------------------------------------------
// The run log (every run, from v1.0)

void Rogue_RunLogStart(void);
// The run being played is left for a new one (Rogue_StashRecord).
void Rogue_RunLogLeave(void);
// The run ended (ROGUE_LOG_*). Writes into the loaded Director; the caller saves.
void Rogue_RunLogEnd(u32 result);
// The newest entry (n = 0), the one before (1)... NULL past the last.
const struct RogueRunLogEntry *Rogue_RunLogEntry(const struct RogueDirector *d, u32 n);

// ---------------------------------------------------------------------------
// Teasers before RUN 0 (wins 1 to 4)

// The Hall of Fame shows "Lv. ??" for one frame on the first win.
bool32 Rogue_TeaseHofLevel(void);
// The townsperson's line in the first town (win 2): TRUE and the line in
// dest once per run that has it.
bool32 Rogue_TeaseTownLine(u8 *dest);
// PROF. BIRCH says "given" for "found" (win 3), once.
bool32 Rogue_TeaseGiven(void);

// ---------------------------------------------------------------------------
// The challenge (src/rogue_run0_challenge.c, src/rogue_run0_trainers.h)
//
// Levels: the band starts at the party's median level (base, about 55; a
// player with no Hall of Fame has TEST TEAM's) and the gyms climb to 88 by
// the eighth (about 4 a badge), the Elite Four stand at 90 to 95 and the
// Director at 100. Route trainers, gym trainers and wild Pokemon follow the
// region's own curve mapped onto that band.

// After every badge the Director patches the game, picking from what the
// player does most. Each is a rule the game enforces while it is in force
// (VAR_ROGUE_RUN0_PATCHES, and the Director's patches for the attempt).
enum Run0Patch
{
    RUN0_PATCH_LEAD_MOVES,   // wild Pokemon know a move against the lead used most
    RUN0_PATCH_TYPE_ANSWERS, // trainers bring answers to the type led with most
    RUN0_PATCH_CENTERS,      // POKeMON CENTERS heal three times per town
    RUN0_PATCH_SHOPS,        // shops stop selling what heals in battle
    RUN0_PATCH_BALLS,        // POKe BALLS work half as well
    RUN0_PATCH_WILD_EXP,     // wild Pokemon give half the EXP
    RUN0_PATCH_NO_ESCAPE,    // running from wild Pokemon fails half the time
    RUN0_PATCH_OMNISCIENT,   // trainers know your moves, abilities and items
    RUN0_PATCH_LEADERS,      // gym leaders remember you: two levels and the best IVs
    RUN0_PATCH_PAIRS,        // wild Pokemon in tall grass come two at a time
    RUN0_PATCH_GIFT_CANDY,   // a gift: three RARE CANDIES (its cost: RUN0_PATCH_COST_CANDY)
    RUN0_PATCH_GIFT_EXP,     // a gift: double EXP until the next badge (then RUN0_PATCH_COST_EXP)
    RUN0_PATCH_GIFT_PRICES,  // a gift: half prices until the next badge (then RUN0_PATCH_COST_PRICES)
    RUN0_PATCH_PICKS,        // the patches a badge picks from end here
    RUN0_PATCH_COST_CANDY = RUN0_PATCH_PICKS, // the candy's cost: trainers are three levels stronger
    RUN0_PATCH_COST_EXP,     // the EXP's cost: three quarters of the EXP from then on
    RUN0_PATCH_COST_PRICES,  // the prices' cost: double prices from then on
    RUN0_PATCH_COUNT,
};
#define RUN0_PATCH_NONE 0xFF

// The challenge's fields, over the Director's challenge bytes.
#define RUN0_CHALLENGE_VERSION 1
#define RUN0_CHALLENGE_LEADS   8  // lead species counted
#define RUN0_CHALLENGE_TOWNS   42 // ROGUE_MAX_PLACES
struct RogueRun0Challenge
{
    u8 version;               // RUN0_CHALLENGE_VERSION, else read as empty
    u8 baseLevel;             // this attempt's band (the party's median at the start)
    u8 lossType;              // the type the last lost attempt led with most (TYPE_NONE: none)
    u8 patchCount;            // patches made this attempt
    u8 lastPatch;             // the newest (RUN0_PATCH_*), RUN0_PATCH_NONE for none
    u8 giftPending;           // a gift whose cost comes with the next badge, or RUN0_PATCH_NONE
    u8 lastCost;              // the cost that came with the newest patch, or RUN0_PATCH_NONE
    u8 answerType;            // RUN0_PATCH_TYPE_ANSWERS's type
    u16 leadSpecies;          // RUN0_PATCH_LEAD_MOVES's Pokemon
    u16 wildBattles;          // this attempt's battles by kind, and wild battles run from
    u16 trainerBattles;
    u16 fled;
    u8 patchOrder[8];         // the patch of each badge, in order (RUN0_PATCH_NONE: none)
    struct { u16 species; u16 count; } leads[RUN0_CHALLENGE_LEADS]; // battles led, by species
    u8 centerHeals[RUN0_CHALLENGE_TOWNS]; // heals at each town's CENTER (RUN0_PATCH_CENTERS)
    // The kept Pokemon on the Director's trainers: once an act each, in turn.
    u32 keptShown;            // a bit per kept index shown on a trainer in keptAct
    u8 keptAct;               // the act keptShown counts (1 to 5), 0 for none
    u8 keptNext;              // the kept index the next one is looked for from
    u16 returned;             // a bit per badge whose kept Pokemon was given back
    u8 lent;                  // kept Pokemon lent at the attempt's start (the arrival's line)
};

// The challenge's part of a loaded Director (version checked by the caller).
#define RUN0_CHALLENGE(d) ((struct RogueRun0Challenge *)(d)->challenge)

// A RUN 0 run starts (Rogue_Run0NewGame, after the import): the band from the
// party, the counter-pick from the last loss, this attempt's play from zero.
void Rogue_Run0ChallengeStart(struct RogueDirector *d);
// What a RUN 0 run starts with: the whole party sharing EXP, as every run's
// starter brings, and a kit for its levels.
void Rogue_Run0GiveKit(void);
// Counted at a battle's start and end (the foundation's battle hooks).
void Rogue_Run0ChallengeBattleStart(struct RogueDirector *d, u16 leadSpecies);
void Rogue_Run0ChallengeBattleEnd(struct RogueDirector *d);
// A RUN 0 run ends: the types it led with most become the next attempt's
// counter-pick (a loss), or none (a win).
void Rogue_Run0ChallengeRunEnd(struct RogueDirector *d, bool32 won);

// The patches. Rogue_RecordBadge calls Rogue_Run0PatchAfterBadge, which picks
// the badge's patch, puts it in force and sets FLAG_ROGUE_RUN0_PATCH_NEW.
// The leader's script then shows its box (Rogue_EventScript_Run0Patch in
// data/scripts/rogue.inc, special Rogue_Run0BufferPatch).
bool32 Rogue_Run0PatchOn(u32 patch);
void Rogue_Run0PatchAfterBadge(void);
// The newest patch, or RUN0_PATCH_NONE; its version number (1 for PATCH 0.1).
u32 Rogue_Run0LastPatch(u32 *version);
// One patch's line for its box ("Gym leaders now remember you."), into dest;
// returns the end. For the story's own boxes and scenes.
u8 *Rogue_Run0PatchText(u8 *dest, u32 patch);
// Script special: the newest patch's box into gStringVar4 ("PATCH 0.3",
// its lines, "Thank you for playing ROGUeMON."), clears
// FLAG_ROGUE_RUN0_PATCH_NEW; a gift's item and count in VAR_0x8000 and
// VAR_0x8001 (0 for none).
void Rogue_Run0BufferPatch(void);

// What the patches change, asked where it happens.
u32 Rogue_Run0ShopPrice(u32 price, u16 item);
void Rogue_Run0PrepareMart(void);        // special: VAR_RESULT TRUE in RUN 0, its Mart's list made (ETHERS, the SHOPS patch)
void Rogue_Run0OpenMart(void);           // special, then waitstate: opens that list
void Rogue_Run0LeagueNurse(void);        // special: VAR_RESULT TRUE in RUN 0 (the LEAGUE nurse's line)
void Rogue_Run0NurseBefore(void);        // special: VAR_RESULT FALSE when the CENTER has no heals left
void Rogue_Run0NurseAfter(void);         // special: counts a heal given
void Rogue_Run0Exp(s32 *exp);            // battle EXP for one Pokemon
u32 Rogue_Run0CatchRate(u32 rate);
bool32 Rogue_Run0BlocksEscape(void);
bool32 Rogue_Run0WildDouble(void);       // a wild battle in tall grass is a double
// A wild Pokemon made (CreateWildMon): an area's catch (isCatch, from
// Rogue_Run0WildCatch) knows only ordinary moves (the last four it learned,
// no setting up, healing or Explosion); others may learn the patch's move.
void Rogue_Run0OnWildMon(struct Pokemon *mon, bool32 isCatch);
// A Pokemon caught in RUN 0, before it is given to the player: raised to the
// band's floor, with the import's best four moves (Rogue_Run0BestMoves).
void Rogue_Run0OnCaught(struct Pokemon *mon);
// The Mart's tier in RUN 0 (its Pokemon are high level from the start).
u32 Rogue_Run0MartTier(u32 tier);
// The attacking type that hits a species hardest (TYPE_NONE if none does),
// a strong move of a type, and teaching it over a Pokemon's weakest attack.
u32 Rogue_Run0AnswerType(u16 species, u32 salt);
u16 Rogue_Run0AnswerMove(u32 type);
void Rogue_Run0TeachAnswer(struct Pokemon *mon, u16 move);

// The band (src/rogue_run0_trainers.h). The ace level of the gym fought at
// a step (0 for the first, 7 for the eighth), for a base or this attempt's;
// where the party should stand now (the base, then the last gym won's ace)
// and four under it (the floor: wild Pokemon, Pokemon from the PC); a level
// of the region's own curve mapped onto the band; a wild Pokemon's level
// for a place's level (normal: what a normal run would use, returned
// outside RUN 0).
#define RUN0_FLOOR_UNDER 4
u32 Rogue_Run0AceFor(u32 base, u32 step);
u32 Rogue_Run0GymAce(u32 step);
u32 Rogue_Run0BandLevel(void);
u32 Rogue_Run0BandFloor(void);
u32 Rogue_Run0MapLevel(u32 level);
u32 Rogue_Run0WildLevel(u32 placeLevel, u32 normal);
// An area's first wild Pokemon (the catch) at two under the band's floor
// at least (RUN0_CATCH_UNDER), grown for that level (CreateWildMon); TRUE
// if it is the area's catch.
#define RUN0_CATCH_UNDER 2
bool32 Rogue_Run0WildCatch(u16 *species, u8 *level);
#define RUN0_LEVEL_FALLEN    90 // the Elite Four: the Fallen
#define RUN0_LEVEL_PREVIOUS  92 //   the Previous Player
#define RUN0_LEVEL_BIRCH     95 //   PROF. BIRCH
#define RUN0_LEVEL_DIRECTOR 100

// Trainers. Rogue_GetTrainer builds RUN 0's teams (src/rogue_run0_trainers.h);
// a Pokemon whose TrainerMon.tags has RUN0_TAG_* set is finished here once it
// is made (the player's nicknames, the Copy's exact Pokemon).
#define RUN0_TAG_SHIFT 24
#define RUN0_TAG_KEPT   1 // a Pokemon the Director keeps: index (bits 16-23) into kept
#define RUN0_TAG_RUNLOG 2 // a Pokemon lost in a past run: entry n (bits 16-23), lost k (bits 8-15)
#define RUN0_TAG_HOF    3 // a Hall of Fame Pokemon: team (bits 16-23, 0 the oldest), mon (bits 8-15)
#define RUN0_TAG_COPY   4 // the Copy: party slot (bits 16-23)
#define RUN0_TAG_TERRY  5 // one of Terry's: sTestTeam index (bits 16-23)
#define RUN0_TAG(kind, a, b) (((u32)(kind) << RUN0_TAG_SHIFT) | ((u32)(a) << 16) | ((u32)(b) << 8))
void Rogue_Run0FinishTrainerParty(struct Pokemon *party, const struct TrainerMon *mons, u32 count);
// Before a trainer's party is made: the Copy and the Director are built
// again from the party as it is now.
void Rogue_Run0FreshTrainer(u16 trainerId);
// The kept Pokemon on route and gym trainers (one in four at most): the
// next one not yet shown this act, in turn (its kept index and species);
// FALSE if none is left. A trainer's battle marks its kept Pokemon shown
// (Rogue_Run0NoteKeptShown, from Rogue_Run0OnBattleStart, the Director
// loaded; Rogue_Run0MarkKeptShown is the challenge's record of it).
bool32 Rogue_Run0PickKept(u32 *index, u16 *species);
void Rogue_Run0NoteKeptShown(struct RogueDirector *d, u16 trainerId);
void Rogue_Run0MarkKeptShown(struct RogueDirector *d, u32 index);
// Replacing losses. After each badge the Director gives one kept Pokemon
// back (the newest it kept), healed, at the band's level, with its line
// (script special after the PATCH box: VAR_RESULT TRUE and the line in
// gStringVar4). The line's words are the story's: Rogue_Run0ReturnLine.
void Rogue_Run0ReturnKept(void);
u8 *Rogue_Run0ReturnLine(u8 *dest, const u8 *nickname, bool32 toPC);
// A kept Pokemon made again at a level: Terry's with his own set, the
// player's with its personality, nickname, IVs from the personality and the
// best four moves (the import's).
void Rogue_Run0BuildKeptMon(struct Pokemon *mon, const struct RogueKeptMon *k, u32 level);
// A Pokemon under the band's floor is raised to it: the party when an
// attempt starts, and a Pokemon taken from the PC into the party.
void Rogue_Run0RaiseToFloor(struct Pokemon *mon);
void Rogue_Run0OnWithdraw(struct Pokemon *mon);
// A checkpoint start (badges > 0): the band is set again so the act's start
// stands at the party's median level, not the first act's base's climb.
void Rogue_Run0ChallengeRebase(struct RogueDirector *d, u32 badges);
// The four best moves of a species at a level (the import's: power,
// accuracy, its own types, a spread of types, no two-turn, Last Resort or
// fainting attacks, one useful status move at most); returns how many.
u32 Rogue_Run0BestMoves(u16 species, u32 level, u16 *out);

// What the trainer builders read from the save (each loads and lets go).
// The Pokemon the Director keeps and has not returned: species, level and
// index into kept; returns how many (Terry's first).
u32 Rogue_Run0KeptMons(u16 *species, u8 *levels, u8 *index, u32 max);
// Pokemon lost in past runs (the run log, newest first): species, level,
// entry and lost index; returns how many.
u32 Rogue_Run0LostMons(u16 *species, u8 *levels, u8 *entry, u8 *lost, u32 max);
// A Hall of Fame team (0 the oldest): its species and levels; returns its size.
u32 Rogue_Run0HofTeam(u32 team, u16 *species, u8 *levels);
// The ace (highest level) of every Hall of Fame team, newest first: species,
// team and mon; returns how many.
u32 Rogue_Run0HofAces(u16 *species, u8 *team, u8 *mon, u32 max);
// The seed of the oldest run in the run log (the first run known), or -1.
s32 Rogue_Run0FirstRunSeed(void);
// Terry's TEST TEAM: species and level of each.
u16 Rogue_Run0TerrySpecies(u32 i, u8 *level);
// The Director's battle (the Champion's slot in RUN 0): 1, or 2 once the
// first battle with it is won (PATCH 1.0.1: the same trainer again).
u32 Rogue_Run0DirectorPhase(void);

// ---------------------------------------------------------------------------
// ROGUE_RUN0_READY builds: a fresh save comes with a history (4 wins, 19 runs),
// so the RUN 0 row shows at once. Not part of the normal build.
#ifdef ROGUE_RUN0_READY
void Rogue_SeedReadyHistory(void);
#endif

#endif // GUARD_ROGUE_RUN0_H
