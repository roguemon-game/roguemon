#ifndef GUARD_ROGUE_RUN0_STORY_H
#define GUARD_ROGUE_RUN0_STORY_H

// RUN 0's story (Roguémon v1.0): the scenes around the Director's region,
// the people in it, the Elite Four and the Director's battle, the three
// endings and what they leave behind in normal runs. The code is in
// src/rogue_run0_story.c, the scripts in data/scripts/rogue_run0.inc, the
// words in src/rogue_text_director.h (line numbers in
// include/constants/rogue_run0_story.h).

#include "rogue_run0.h"
#include "constants/rogue.h"
#include "constants/rogue_run0_story.h"

struct Trainer;
struct MapHeader;
struct RogueDirector;
struct TrainerMon;

// ---------------------------------------------------------------------------
// The story's own save: struct RogueRun0Story over the Director's 256 story
// bytes (struct RogueDirector.story, include/rogue_run0.h).

#define RUN0_STORY_VERSION 1

struct RogueRun0Story
{
    u8 version;          // RUN0_STORY_VERSION; anything else reads as fresh
    u8 patchBadges;      // badges whose PATCH box was shown (this attempt)
    u8 drafts;           // DRAFTs met (this attempt)
    u8 caseLine;         // the Director's case lines said in the library (across attempts)
    u32 slips;           // towns whose person slipped, a bit per place (this attempt)
    u32 patchesShown;    // RUN0_PATCH_* bits already listed in a box (this attempt)
    u16 shelf;           // the library's next shelf (across attempts)
    u16 giftDay;         // the cartridge clock's day of the last daily gift, + 1 (0: never)
    u16 keptRegion;      // CHOOSE: the region kept, its seed
    u8 hasKeptRegion;    // TRUE once CHOOSE kept one
    u8 birchTold;        // endings PROF. BIRCH has spoken of, a bit per ending
    u8 endlessDepth;     // ENDLESS: the number of the region being played (1 the first)
    u8 endlessCarry;     // TRUE: the next ENDLESS region keeps the party (MORE, or the gate)
    u8 terryDeleted;     // TRUE once Terry was deleted, in any attempt
    u8 pad0;
    // The Previous Player's team (the first HALL OF FAME team) and PROF.
    // BIRCH's three (the first run's starters), set at the attempt's start.
    u16 prevSpecies[6];
    u8 prevLevel[6];
    u16 birchStarters[3];
    u16 chooseSeeds[6];  // CHOOSE: the six regions shown
    u8 truckDone;        // STOP: Terry took his team back at the truck
    u8 cameHome;         // STOP: POKeMON the Director gave back
    u16 libRoom;         // the library's room, 0 the first (across attempts)
};

// Across attempts, bits in the Director's beats (RUN0_BEAT_STORY_FIRST + n):
// lines said once ever.
enum
{
    RUN0_BEAT_ISLAND = RUN0_BEAT_STORY_FIRST, // leaving the real Littleroot
    RUN0_BEAT_GRAVE,      // the graveyard's count
    RUN0_BEAT_DRAFT,      // the first DRAFT
    RUN0_BEAT_REPLY_2,    // "He asked for personality."
    RUN0_BEAT_REPLY_4,    // "Good enough to ship."
    RUN0_BEAT_REPLY_7,    // "I am."
    RUN0_BEAT_LIB_ENTER,
    RUN0_BEAT_LIB_OUT,
    RUN0_BEAT_VICTORY,    // "Nobody remembers VICTORY ROAD."
};


// The minutes of play when the real Littleroot was entered (Emerald's
// Trick House level: the Trick House cannot be reached).
#define VAR_ROGUE_RUN0_ISLAND_MIN VAR_TRICK_HOUSE_LEVEL

// ---------------------------------------------------------------------------
// Hooks

struct RogueRun0Story *Rogue_Run0Story(struct RogueDirector *d);
// Rogue_Run0NewGame, after the teams: the attempt's story starts.
void Rogue_Run0StoryNewAttempt(void);
// Rogue_NewGame for any run: ENDLESS's carried party and the truck.
void Rogue_Run0StoryNewRun(void);
// Each map load (Rogue_OnMapLoad), and each overworld frame (CB1_Overworld).
void Rogue_Run0OnMapLoad(void);
void Rogue_Run0StoryFrame(void);
// Words: the leaders, the town slip, trainers (dest in the game's
// characters; FALSE to say the normal line).
bool32 Rogue_Run0LeaderLine(u32 leader, u32 field, u8 *dest, u32 size);
bool32 Rogue_Run0TownSlip(u32 place, u8 *dest);
const u8 *Rogue_Run0TrainerText(u32 slot, u32 kind);
bool32 Rogue_Run0BufferE4(u32 member, bool32 after);
bool32 Rogue_Run0BufferRivalAfter(void);
// Trainers: builds the story's own trainer (CHAMPION TERRY); 0 for any other
// slot (the challenge builds the Previous Player, the Elite Four and the
// Director: src/rogue_run0_trainers.h).
u32 Rogue_Run0BuildTrainer(u32 slot, struct Trainer *t, struct TrainerMon *party);
u16 Rogue_Run0RivalGfx(void);
u16 Rogue_Run0E4Gfx(u32 member);
// Battles: DRAFT, the darkened picture, the end.
void Rogue_Run0OnWildBattleStart(void);
const u8 *Rogue_Run0CatchBlockedText(void);
void Rogue_Run0TrainerPicLoaded(u16 paletteTag);
void Rogue_Run0StoryBattleEnd(void);
// Things read (graves, shelves) and the people of the story's houses.
const u8 *Rogue_Run0ThingScript(s16 x, s16 y);
struct Run0Cast { u16 gfx; const u8 *script; u8 movement; };
u32 Rogue_Run0HouseCast(u32 place, u32 house, struct Run0Cast *cast);
// The fake corrupted save (start menu, after a save) and the fake boot.
bool32 Rogue_Run0FakeSaveDue(void);
bool32 Rogue_Run0FakeBoot(void);
bool32 Rogue_Run0FakeBootSave(void);
void Rogue_Run0FakeBootReturn(void);
extern bool8 gRogueSoundStuck;
// After the endings: PROF. BIRCH's line (ASCII, NULL for none), the truck,
// the kept region, ENDLESS, the trainer card's star, the credits.
const char *Rogue_Run0BirchEndingLine(void);
bool32 Rogue_Run0BirchEndingWelcome(u8 *dest, u32 size);
bool32 Rogue_Run0TruckParked(void);
bool32 Rogue_Run0KeptRegion(u16 *seed);
void Rogue_Run0ForgetKeptRegion(void);
// In the library's room (its far door is open: Rogue_SealInterior).
bool32 Rogue_Run0InLibrary(void);
// The place whose obstacles TERRY clears in act 3 (RUN0_FLAG_SHORTCUT), or -1.
s32 Rogue_Run0ShortcutPlace(void);
// The POKeDEX's entry for a POKeMON caught in RUN 0: the Director's note on
// the player (into gStringVar4), or NULL outside RUN 0.
const u8 *Rogue_Run0DexNote(u16 species);
bool32 Rogue_Run0EndlessUnlocked(void);
bool32 Rogue_Run0FeatStar(void);
bool32 Rogue_Run0Credits(void);
void Rogue_Run0CreditsDone(void);
bool32 Rogue_Run0SummaryPage(u8 *dest);
u32 Rogue_Run0EndlessDepth(void);
void Rogue_Run0MenuEndless(void);
void Rogue_Run0StopWalkStep(void);
void Rogue_Run0StoryHeader(struct MapHeader *h, u16 group, u16 num);
void Rogue_Run0StoryObjects(void);
// The test hook: a scene to start (RUN0_SCENE_*), as if it were due.
extern u8 gRogueTestStory;

#endif // GUARD_ROGUE_RUN0_STORY_H
