#ifndef GUARD_ROGUE_H
#define GUARD_ROGUE_H

#include "constants/rogue.h"

struct MapHeader;
struct MapLayout;
struct Trainer;
struct WildPokemonInfo;
struct RogueRegion;
struct RogueMap;
struct RoguePlace;
struct WildPokemon;

// Test hooks written by tools/roguemon/playtest.py. Always zero in normal play.
extern u8 gRogueTestStarterLevel;
extern u32 gRogueTestSeed;
extern u16 gRogueGenFrames;

// --- src/rogue.c: the generated region ---

// New game setup: picks a seed, generates the region and sets the start warp.
void Rogue_NewGame(void);
// The loading screen (src/rogue_loading.c) over a new run's start: up from
// CB2_NewGame until Rogue_LoadingHide, before the field loads.
void Rogue_LoadingNewGame(void);
// CONTINUE: makes the saved region and map behind the loading screen.
void Rogue_LoadingContinue(void);
// Plants the region's berry trees. Call after the new game scripts have run.
void Rogue_PlantBerries(void);

// Hooks into the engine's map lookups. Each returns NULL when the map or
// layout is not part of the generated region.
const struct MapHeader *Rogue_GetMapHeader(u16 mapGroup, u16 mapNum);
const struct MapLayout *Rogue_GetMapLayout(u16 mapLayoutId);
// Makes sure a connected map's blocks are generated before the engine copies
// the edge of the loading map's connection i. FALSE: the edge is far from
// the player and is copied in later (FillMapConnection).
bool32 Rogue_EnsureMapBlocks(u16 mapGroup, u16 mapNum, u32 i);
// Called as the engine starts laying out a map: no edge is left for later.
void Rogue_NewMapLayout(void);
const u16 *Rogue_LayoutBorder(const struct MapLayout *layout, int x, int y);
bool32 Rogue_GetMapName(u8 *dest, u16 mapSecId);
// Called when a map loads (field effects such as a lit cave).
void Rogue_OnMapLoad(void);
// Called every overworld frame: generates maps ahead in the spare time.
void Rogue_BackgroundStep(void);
void Rogue_YieldIfLate(void);

bool32 Rogue_IsRogueTrainer(u16 trainerId);
// Two trainers who see the player at once fight a double battle, as in
// Emerald, but the rival does that only in about one region in a thousand.
bool32 Rogue_AllowRivalDouble(void);
// A team's admin or leader: never in a battle of two trainers who saw the
// player at once. And the hook after any trainer battle won.
bool32 Rogue_IsSoloTrainer(u16 trainerId);
void Rogue_VillainBattleWon(u16 trainerId);
const struct Trainer *Rogue_GetTrainer(u16 trainerId);
// The species a generated trainer slot sends out first ({OWN} in their lines).
u16 Rogue_TrainerLeadSpecies(u32 slot);
const struct WildPokemonInfo *Rogue_GetLandMonsInfo(const struct WildPokemonInfo *original);
const struct WildPokemonInfo *Rogue_GetWaterMonsInfo(const struct WildPokemonInfo *original);
const struct WildPokemonInfo *Rogue_GetFishingMonsInfo(const struct WildPokemonInfo *original);

// The generated region, built on demand from the saved seed.
const struct RogueRegion *Rogue_GetRegion(void);
bool32 Rogue_PlaceHasWildSpecies(u32 place, u16 species);
const struct RogueMap *Rogue_GetPlaceMap(u32 place);
u32 Rogue_GetSeed(void);
u8 *Rogue_RegionName(u8 *dest, u32 seed);
void Rogue_PrepareNewRegion(void);
void Rogue_PrepareTypedRegion(const u8 *code);
// The run's code for a friend to type (KALONA-N) into dest; FALSE for RUN 0
// and runs from before v0.14, which have none.
bool32 Rogue_RunCode(u8 *dest);
// The DAILY REGION: one region a day from the cartridge's clock, the same
// for everyone (FALSE from Rogue_DailyAvailable with no clock).
bool32 Rogue_DailyAvailable(void);
u32 Rogue_DailySeed(u32 year, u32 month, u32 day);
void Rogue_PrepareDailyRegion(void);
bool32 Rogue_IsDailyRegion(void);
u8 *Rogue_DailyDateText(u8 *dest);
// The region of a seed (RUN 0's CHOOSE: the region kept).
void Rogue_PrepareSeedRegion(u32 seed);
u8 *Rogue_DescribeRegion(u8 *dest, u32 full);
// PROF. BIRCH's welcome back, before a new region replaces the saved one.
bool32 Rogue_BirchWelcomeBack(u8 *dest, u32 size);
u8 *Rogue_CopyAscii(u8 *dest, const u8 *src);
// Name of a place (town, route, Victory Road or cave).
u8 *Rogue_CopyPlaceName(u8 *dest, u32 place);
// Place the player is in. Interiors count as their town and the League rooms
// as the summit. Returns -1 outside the region.
s32 Rogue_GetCurrentPlace(void);
u32 Rogue_GetCurrentTownPlace(void);
// Town number of the current map. Interiors count as their town.
u32 Rogue_GetCurrentTown(void);
// Wild encounter area of the current map for the one-catch rule, or -1.
s32 Rogue_GetAreaId(void);
bool32 Rogue_IsAreaCatchUsed(u32 area);
void Rogue_SetAreaCatchUsed(u32 area);
void Rogue_ClearAreaCatchUsed(u32 area);
bool32 Rogue_IsLateCampOn(void);
// The region map's record: an area where a Pokemon was caught, and places
// the player has been to.
bool32 Rogue_IsAreaCaught(u32 area);
void Rogue_SetAreaCaught(u32 area);
bool32 Rogue_IsPlaceVisited(u32 place);
u8 Rogue_RouteNear(u8 place);
u32 Rogue_NextTownFrom(u32 place);
const u8 *Rogue_GetGymLeaderName(u32 gym);
const u8 *Rogue_GetBadgeName(u32 gym);
u32 Rogue_GetGymLeaderIndex(u32 gym);
u32 Rogue_GetE4Index(u32 member);
u32 Rogue_GetLeaderType(u32 leader);
const u8 *Rogue_GetLeaderName(u32 leader);
u16 Rogue_GetLeaderGfx(u32 leader);
u16 Rogue_GetGymGfx(u32 gym, u32 which);
// Town signatures (src/rogue_town.c): a signature person's look, a signature
// Pokemon's species, a berry farm's berries, a dojo trainer's look.
u16 Rogue_SigFolkGfx(u32 place, u32 slot);
u16 Rogue_SigMonSpecies(u32 place, u32 slot);
u16 Rogue_FarmBerry(u32 i);
u16 Rogue_GetDojoGfx(u32 which);
void Rogue_PrepareSigFolk(void);
// Route features (src/rogue_town.c, src/rogue_gen_features.h).
u16 Rogue_FeatFolkGfx(u32 place, u32 slot);
u16 Rogue_FeatMonSpecies(u32 place, u32 slot);
u16 Rogue_FeatureTrainerGfx(u32 slot);
u16 Rogue_PairGfx(u32 pair, bool32 second);
u32 Rogue_GauntletLeft(u32 place);
void Rogue_PrepareFeatFolk(void);
void Rogue_MarkFeatDone(void);
void Rogue_BufferFeatAfter(void);
void Rogue_BufferFeatureSign(void);
void Rogue_BufferSnorlax(void);
void Rogue_CreateSnorlax(void);
void Rogue_MarkSigMonGiven(void);
void Rogue_MarkSigItemGiven(void);
void Rogue_BufferSigGiftAfter(void);
void Rogue_HotSpringSoak(void);
void Rogue_PrepareMarketStall(void);
void Rogue_BufferLandmarkSign(void);
void Rogue_BufferExhibit(void);
u32 Rogue_GetGymTrainerCount(u32 gym);
u16 Rogue_GetWildSpecies(u32 area, u32 i);
u16 Rogue_GetRivalStarter(void);
// The rival's partner as it stands at the next (or current) rival battle.
u16 Rogue_GetRivalPartner(void);
// The starter menu (src/rogue.c has the rows, src/rogue_starter_menu.c draws
// them). Each row is a kind of starter (ROGUE_STARTER_KIND_*); a row may
// offer several species, left and right going through them, choice 0 being
// this region's roll. The POKEDEX PICK and ANY POKEMON rows open a list.
#define ROGUE_MAX_STARTER_ROWS 16
u32 Rogue_GetStarterRows(u8 *kinds);
// The rows' names into gStringVar4, 13 bytes apart, and their number into VAR_0x8004.
void Rogue_PrepareStarters(void);
u32 Rogue_GetStarterCount(void);
u32 Rogue_StarterRowChoices(u32 kind);
u16 Rogue_StarterRowSpecies(u32 kind, u32 choice, bool32 *shiny);
bool32 Rogue_StarterRowIsList(u32 kind);
const u8 *Rogue_StarterKindLabel(u32 kind, u16 species);
// The name a row shows: the species', or the fallen friend's nickname.
void Rogue_StarterRowName(u8 *dest, u32 kind, u16 species);
// A list row's species in alphabetical order, on the heap (the caller frees
// it), and the one its cursor starts on.
u16 *Rogue_BuildStarterList(u32 kind, u32 *count);
u16 Rogue_StarterListStart(u32 kind);
bool32 Rogue_StarterListShiny(u32 kind, u16 species);
// The first form of a species (Pichu for Raichu).
u16 Rogue_FirstForm(u16 species);
// The spare starter a town holds (0 or 1), or -1. See src/rogue.c.
s32 Rogue_GetSpareGift(u32 place);
bool32 Rogue_PartyHasStarter(void);
// The run's starter: its species and first type, or SPECIES_NONE and
// TYPE_NONE for a run started before v0.13 with an extra row.
u16 Rogue_GetRunStarterSpecies(void);
u32 Rogue_GetRunStarterFirstType(void);
// Script special: the starter menu. VAR_RESULT gets the row, VAR_0x8006 the
// species; while it is open VAR_0x8006 is the species under the cursor and
// VAR_0x8007 is 1 in a list (the tests read them).
void Rogue_ChooseStarter(void);
void Rogue_BufferStarterConfirm(void);
void Rogue_BufferStarterNickname(void);
u32 Rogue_GetMartTier(void);
// FOSSILS: the scientist who revives fossils visits this town.
bool32 Rogue_IsFossilTown(u32 place);
void Rogue_CheckFossilTown(void);
// HARDCORE RULES: this run plays by them, and the current level cap.
bool32 Rogue_IsHardcore(void);
u32 Rogue_GetLevelCap(void);
// RARE FINDS: the professor's hint about where the rare find lives, into
// STR_VAR_1. VAR_RESULT is FALSE when the unlock is off.
void Rogue_BufferRareFindHint(void);
// Lines for generated trainers, used by the battle setup.
#define ROGUE_TEXT_INTRO  0
#define ROGUE_TEXT_DEFEAT 1
const u8 *Rogue_GetTrainerText(u16 trainerId, u32 kind);
u32 Rogue_GetGymLeaderType(u32 gym);
// Fly from the region map.
bool32 Rogue_CanFlyTo(u32 place);
void Rogue_SetFlyWarp(u32 place);

// Dungeons (src/rogue.c): the dungeon the player is in or by, or -1, and
// helpers src/rogue_dungeon.c shares.
s32 Rogue_GetCurrentDungeon(void);
// The dungeon whose floor the player stands on, or -1 off the floors.
s32 Rogue_GetFloorDungeon(void);
s32 Rogue_GetFloorNumber(void);
// A thing to read at a map block (with the map's border offset) the player
// faces, its words in gStringVar4: a tower's grave, a shipwreck's bin, a
// power plant's terminal, a painting at home or in a POKéMON CENTER. NULL
// if there is none (src/rogue_dungeon.c).
const u8 *Rogue_GetThingScript(s16 x, s16 y);
const u8 *Rogue_GetHomeThingScript(s16 x, s16 y);
// The POKéMON CENTER's extra people: the child with the town's rhyme
// (LORE_WAY_RHYME) and the explorer, once led out of their dungeon.
bool32 Rogue_RhymeChildHere(u32 place);
bool32 Rogue_ExplorerThanksHere(u32 place);
bool32 Rogue_WildFits(u16 species, const struct RoguePlace *p);
bool32 Rogue_LineFitsLevel(u16 species, u32 level);
u16 Rogue_EvolveForLevel(u16 species, u32 level, bool32 anyMethod);
// The battle setup's hook: the stand-in trainer ROGUE_SLOT_DUNGEON_MASTER is
// the master of the dungeon the player is in.
u16 Rogue_ResolveTrainerId(u16 trainerId);
// The wild encounter check's hook: a dungeon floor's land encounter rate.
u32 Rogue_GetLandEncounterRate(u32 romRate);

// --- src/rogue_dungeon.c: dungeon Pokemon, bosses and rewards ---

// A dungeon master by kind: how they look and battle, their team's types
// (TYPE_NONE for any) and their lines.
struct RogueMasterDef
{
    u8 trainerClass;
    u16 pic;
    u16 gfx;
    u8 gender;
    u8 music;
    u8 types[2];
};

// Fills a dungeon floor's twelve land slots; returns its encounter rate, or
// 0 for none.
u32 RogueDungeon_FloorWild(u32 dungeon, u32 floor, struct WildPokemon *mons);
u32 RogueDungeon_WildRate(u32 dungeon, u32 floor);
// The guardian or legend of a dungeon (its boss Pokemon).
u16 RogueDungeon_BossSpecies(u32 dungeon);
const struct RogueMasterDef *RogueDungeon_MasterDef(u32 dungeon);
u32 RogueDungeon_MasterLevel(u32 dungeon);
// A master's line (RMASTER_INTRO, _DEFEAT, _AFTER) into dest, in the game's characters.
u8 *Rogue_BufferMasterLine(u32 dungeon, u32 field, u8 *dest, u32 size);
// A dungeon item ball's own item (the prize, a choice room's, a kind's find),
// or ITEM_NONE for the floor's ordinary finds.
bool32 RogueDungeon_IsChoice(u32 dungeon, u32 slot);
// For rumors: the region's legend and the place its dungeon is entered
// from. FALSE if the region has none.
bool32 Rogue_GetLegendRumor(u16 *species, u8 *place);
// Is a dungeon's legend still asleep: too few badges, or the villain leader
// who is after it not beaten yet.
bool32 Rogue_LegendGateOpen(u32 dungeon);
bool32 Rogue_IsSealOpened(u32 dungeon);
// The region's tunnel: is its barrier open now (src/rogue_dungeon.c).
bool32 Rogue_TunnelOpen(void);
bool32 Rogue_SealShown(u32 dungeon);
void Rogue_PrepareGymKey(u32 gym);
u16 Rogue_PrizeKey(u32 dungeon);
// The dungeon of this region whose boss is this legend (enum RogueLegend),
// or -1.
s32 Rogue_LegendDungeon(u32 legend);

// --- src/rogue_town.c: townsfolk, houses and gyms ---

u16 Rogue_TownsfolkGfx(u32 theme, bool32 summit, u32 slot);
u16 Rogue_PlaceTownsfolkGfx(u32 place, const struct RogueMap *m, u32 slot);
u16 Rogue_TownsfolkObjGfx(const struct RogueMap *m, u32 obj);
void Rogue_WarmText(u32 place);
// Header for an interior (Pokemon Center, Mart, house, gym) with its layout,
// people and the way back out for this run, or NULL for any other map.
const struct MapHeader *Rogue_GetInteriorHeader(u16 mapGroup, u16 mapNum);

// --- src/rogue_dungeon.c: dungeon names, notes, rumors ---

// A dungeon's name: its cave's, "<TOWN> TOWER", "<TOWN> WOODS"...
u8 *Rogue_CopyDungeonName(u8 *dest, u32 dungeon);
u8 *Rogue_CopyCaveDungeonName(u8 *dest, u32 place);
bool32 Rogue_CaveHasDungeonName(u32 place);
// Generator ASCII (src/rogue_gen_lore.h) in the game's characters, and a
// piece of a legend's lore (LORE_*) with a way's frame, at most size bytes.
u8 *Rogue_CopyLoreAscii(u8 *dest, const char *src);
// The text engine's game side (src/rogue_text.c).
struct RogueTextCtx;
struct RogueTextState;
char *Rogue_GameToAscii(char *dest, const u8 *src, u32 max);
void Rogue_TextState(struct RogueTextState *s);
void Rogue_TextCtx(struct RogueTextCtx *c, struct RogueTextState *st, s32 place, u8 speaker);
u8 *Rogue_TextToGame(u8 *dest);
u8 *Rogue_BufferText(u8 *dest, u32 size, const char *template, struct RogueTextCtx *c);
u8 *Rogue_BufferLore(u8 *dest, u32 dungeon, u32 piece, u32 way, u32 size);
u16 RogueDungeon_GuideGfx(u32 dungeon);
// The lost explorer of a dungeon: how they look (src/rogue_dungeon.c).
u16 RogueDungeon_ExplorerGfx(u32 dungeon);
// A fall through a ruins floor: sets the warp to the floor below (rogue.c).
bool32 Rogue_SetFallWarp(void);
// Script special: the item ball the player spoke to into VAR_0x8000 (item) and VAR_0x8001 (count).
void Rogue_PrepareItem(void);
// The name the floor popup shows on a dungeon floor; FALSE on other maps.
bool32 Rogue_GetFloorName(u8 *dest, const struct MapHeader *header);
// A dungeon's boss is beaten, caught or gone (this run).
bool32 Rogue_IsDungeonCleared(u32 dungeon);
u32 Rogue_CountDungeonsCleared(void);
u32 Rogue_CountDungeonBosses(void);
// The dungeon whose boss is this trainer, or -1.
s32 Rogue_DungeonOfBossTrainer(u16 trainerId);
// The region map strip's note on the dungeon a place holds, if any.
u8 *Rogue_AppendDungeonNote(u8 *str, u32 place, bool32 brief);
// A townsperson's line about the nearest dungeon into STR_VAR_1.
bool32 Rogue_BufferDungeonRumor(u32 place, u32 rng);

// The region map's dungeon marks (graphics/rogue/dungeon_marks.png, made by
// tools/roguemon/make_dungeon_marks.py): FRLG's dungeon icon, a small gray
// square until its way in is visited, then framed and filled with its
// kind's color. sDungeonMarks in src/rogue_ui.c gives each RDUN_* kind its
// style; a kind with no row there gets DMARK_GENERIC.
enum
{
    DMARK_GENERIC,
    DMARK_CAVE,
    DMARK_VICTORY,
    DMARK_FOREST,
    DMARK_TOWER,
    DMARK_HIDEOUT,
    DMARK_LAIR,
    DMARK_ICE,
    DMARK_VOLCANO,
    DMARK_RUINS,
    DMARK_SHIPWRECK,
    DMARK_PLANT,
    DMARK_TUNNEL,
    DMARK_COUNT,
};
// Marks beyond each dungeon's way in, such as a tunnel's far mouth once it
// is found: fills places[] and styles[] (DMARK_*), at most max, and returns
// how many. A place found is drawn framed; give DMARK_NOT_FOUND | style to
// show the small gray mark instead.
#define DMARK_NOT_FOUND 0x80
u32 Rogue_ExtraMapMarks(u8 *places, u8 *styles, u32 max);
// The name of the volcano the region map draws (RogueGen_VolcanoCell).
u8 *Rogue_CopyVolcanoName(u8 *dest);
bool32 Rogue_BufferCraterNote(u8 *str);

// --- src/rogue_run.c: Nuzlocke rules, run results and permanent unlocks ---

// Bytes for a bit per national dex number (NATIONAL_DEX_COUNT is 1025).
#define ROGUE_DEX_BYTES 130

// The permanent record beyond runs, wins and best (which are vars). It lives
// in the save block's Battle Frontier hall records, which nothing in a
// generated region uses, and is carried across New Game like the vars.
struct RogueRecord
{
    u32 magic;            // ROGUE_RECORD_MAGIC once set up
    u32 unlocks;          // earned, one bit per UNLOCK_*
    u32 disabled;         // switched off in the UNLOCKS menu
    u32 announced;        // unlocks the professor has already told about
    u32 runStart;         // unlocks earned before this run, for the summary
    u16 losses;
    u16 caves;            // caves explored, across runs
    u16 catches;          // Pokemon caught, across runs
    u8 shinies;           // shiny Pokemon met, across runs
    u8 rivalWins;
    u8 fossils;           // fossils revived
    u8 typeBadges[3];     // most badges won with each type's starter in the party
    u8 hardcoreWins;
    u8 fallenLevel;       // level of the fallen friend when the run ended
    u16 fallenSpecies;    // the fallen friend; ROGUE_CHAMPION_MON_SHINY if shiny
    u8 fallenName[POKEMON_NAME_LENGTH + 1];
    u8 lastPlace;         // where the last run ended, for the summary: a place, or ROGUE_LAST_DUNGEON | dungeon
    u8 lastBadges;
    u16 dungeons;         // dungeon bosses beaten, across runs (offset 52, read by the tests)
    // Added in v0.10. A record from before has no extMagic, and
    // Rogue_GetRecord clears the fields after it.
    u16 extMagic;         // ROGUE_RECORD_EXT_MAGIC once set up (offset 54)
    u32 legends;          // legends caught across runs, a bit per enum RogueLegend (offset 56)
    u32 legendsSeen;      // legends met, spoken to (offset 60)
    u16 feats;            // FEAT_* done (offset 64)
    u16 featsStart;       // feats done before this run, for the summary (offset 66)
    u32 runLegends;       // the legends of the run being played (offset 68)
    // Added in v0.13 for the starter rows. A record from before has no
    // ext2Magic, and Rogue_GetRecord sets these up (src/rogue_run.c).
    u16 ext2Magic;        // ROGUE_RECORD_EXT2_MAGIC once set up (offset 72)
    u16 lastAnySpecies;   // the last ANY POKEMON pick, where its list opens next time (74)
    u32 unlocksHi;        // unlocks 32 on, as unlocks to runStart above (76 to 88)
    u32 disabledHi;
    u32 announcedHi;
    u32 runStartHi;
    u8 typeBadgesAll[22]; // most badges won with a starter of each first type, by enum Type (92)
    u16 lastDexSpecies;   // the last POKEDEX PICK (114)
    u8 caught[ROGUE_DEX_BYTES];      // first forms caught across runs, a bit per national dex number (116)
    u8 caughtShiny[ROGUE_DEX_BYTES]; // the same, caught shiny (246)
};

#define ROGUE_LAST_DUNGEON 0x80
#define ROGUE_RECORD_MAGIC 0x524F4755 // "ROGU"
#define ROGUE_RECORD_EXT_MAGIC 0x4C47 // "GL"
#define ROGUE_RECORD_EXT2_MAGIC 0x5453 // "ST"

struct RogueRecord *Rogue_GetRecord(void);
// Earned and switched on in the UNLOCKS menu.
bool32 Rogue_IsUnlockOn(u32 unlock);
bool32 Rogue_IsUnlockEarned(u32 unlock);
void Rogue_SetUnlockEnabled(u32 unlock, bool32 enabled);
const u8 *Rogue_GetUnlockHint(u32 unlock);
const u8 *Rogue_GetUnlockDescription(u32 unlock);
// For the upgrades of a starter, by its first type (enum Type): TRUE if that
// type's upgrade is earned and on.
bool32 Rogue_HasTypeUpgrade(u32 unlock, u32 type);
// Most badges won with a starter of this first type still in the party.
u32 Rogue_GetTypeBadges(u32 type);
u32 Rogue_CountEarnedUnlocks(void);
// The POKeDEX mix chosen in UNLOCKS for the next run (ROGUE_MIX_CLASSIC,
// _NATIONAL or _GLOBAL; only an earned one), and choosing one.
u32 Rogue_GetDexChoice(void);
void Rogue_SetDexChoice(u32 mix);
// The next earned choice after mix, for the UNLOCKS row.
u32 Rogue_NextDexChoice(u32 mix);
const u8 *Rogue_GetDexChoiceName(u32 mix);
const u8 *Rogue_GetDexChoiceDescription(u32 mix);
// Unlocks the run that just ended earned, for the summary.
bool32 Rogue_IsSummaryUnlock(u32 unlock);
// The record of first forms caught across runs (POKEDEX PICK).
bool32 Rogue_WasCaught(u16 species, bool32 *shiny);
// Milestones that count toward unlocks. Each updates the record.
void Rogue_RecordCaveVisit(u32 place);
void Rogue_RecordCatch(void);
void Rogue_RecordShinySeen(void);
void Rogue_RecordRivalWin(void);
void Rogue_PrepareRivalExit(void);
extern u8 gRogueRivalExitMovement[];
void Rogue_RecordFossilRevived(void);
// A dungeon's boss is beaten or caught: marks it and counts it once per run.
void Rogue_RecordDungeonCleared(u32 dungeon);
// After a badge: counts it for the starter's type if the starter is still in
// the party.
void Rogue_RecordBadge(void);
// A legend (enum RogueLegend) is met, or caught, for the LEGEND LOG.
void Rogue_RecordLegendSeen(u32 legend);
void Rogue_RecordLegendCaught(u32 legend);
// The LEGEND LOG's rows: sets of legends (a trio, a pair), and a last row
// of lone ones. Each row lists up to four legends, RLEG_NONE after the last.
#define ROGUE_LEGEND_ROWS 10
#define ROGUE_LEGEND_SETS 9 // the rows that are sets
#define ROGUE_NUM_LEGENDS 29
extern const u8 gRogueLegendRows[ROGUE_LEGEND_ROWS][4];
const u8 *Rogue_GetLegendRowName(u32 row, bool32 full);
u32 Rogue_CountLegends(u32 bits);
u16 Rogue_LegendSpecies(u32 legend);
// The legends of this region (a bit per enum RogueLegend), and the ones whose
// boss is not beaten or caught yet.
u32 Rogue_RegionLegends(bool32 leftOnly);
// Feats: done, and their names, hints, rewards and progress.
bool32 Rogue_IsFeatDone(u32 feat);
const u8 *Rogue_GetFeatName(u32 feat);
const u8 *Rogue_GetFeatHint(u32 feat);
const u8 *Rogue_GetFeatReward(u32 feat);
bool32 Rogue_GetFeatProgress(u32 feat, u32 *have, u32 *need);
// The starter type of this run (ROGUE_STARTER_*), or -1 for any other choice.
s32 Rogue_GetRunStarterType(void);

// The villain team (src/rogue.c). TRUE once the hideout's admin is beaten
// and until the leader is: a legend's dungeon puts him (ROBJ_VILLAIN_LEADER)
// before the legend.
bool32 Rogue_VillainLeaderReady(void);
// The same for villain team t (0 the main team): its hideout cleared (the
// admin beaten), its leader beaten, and its leader waiting.
bool32 Rogue_TeamCleared(u32 t);
bool32 Rogue_TeamLeaderBeaten(u32 t);
bool32 Rogue_TeamLeaderReady(u32 t);
// ROGUE_KEY_VILLAIN once the blockade is gone, for reach checks.
u8 Rogue_VillainKeys(void);
// A line about the team for a townsperson of the hideout's town.
bool32 Rogue_BufferVillainTownTalk(u32 place, u32 order);
bool32 Rogue_BufferWandererPointer(u32 place, u32 order);
bool32 Rogue_BufferRuinsRiddleTalk(u32 place, bool32 teller);
bool32 Rogue_RuinsScholarHere(u32 place);
void Rogue_BufferRuinsScholar(void);

// TRUE if the saved run has ended and cannot be continued.
bool32 Rogue_IsRunOver(void);
void Rogue_OnWildBattleStart(void);
// Text to show instead of throwing a ball, or NULL if catching is allowed.
const u8 *Rogue_GetCatchBlockedText(void);
bool32 Rogue_IsFirstEncounter(void);
// A shiny the Shiny clause lets the player catch in a used area or as a dupe.
bool32 Rogue_IsShinyClauseCatch(void);
// The area's one catch is already used: the battle says so up front.
bool32 Rogue_IsAreaUsed(void);
// A dupe in an area still open: the battle says it won't use up the area.
bool32 Rogue_IsDupeEncounter(void);
void Rogue_OnBattleEnd(void);
// Farewell message for Pokemon that fainted, once the field is free again.
void Rogue_CheckFallenMessage(void);
// Script special (RUN 0, after a badge): the Director's line on the
// Pokemon that fainted in the leader's battle, now (gStringVar1, VAR_RESULT).
void Rogue_Run0FallenNow(void);
// Ends the run after the whole party has fainted.
void Rogue_LoseRun(void);
// Carries the permanent record across a new game. The stash lives on the
// caller's stack.
struct RogueRecordStash
{
    struct RogueRecord *record; // a copy on the heap, so the caller's stack stays small
    u16 vars[6];
};
void Rogue_StashRecord(struct RogueRecordStash *stash);
void Rogue_RestoreRecord(const struct RogueRecordStash *stash);
u64 Rogue_GetUnlocks(void);
// Updates the record after a milestone. Returns the unlocks just earned.
u64 Rogue_UpdateRecord(void);
const u8 *Rogue_GetUnlockName(u32 index);
// TRUE inside the region once the badge for a field move is won: the badge
// alone lets the player use it, with the party's lead Pokemon.
bool32 Rogue_HasFieldMoveBadge(u32 fieldMove);
bool32 Rogue_IsFieldMoveBadgeWon(u32 fieldMove);
u32 Rogue_WonGyms(void);
u32 Rogue_BadgeCount(void);
// Items (src/rogue_gen_items.h through src/rogue.c).
void Rogue_PrepareHiddenItem(void);
bool32 Rogue_GetTownGift(u32 town, bool32 folk);
void Rogue_UpdateProgress(void);

// --- src/rogue_ui.c: region map and run summary screens ---

void CB2_RogueMap(void);
// The professor shows a new region on its map: YES starts the run with it
// (going on to exitCallback), NO grows another, TYPE NAME visits a named one.
void Rogue_OpenRegionPreview(void (*exitCallback)(void), bool32 firstRun);
void CB2_RogueFlyMap(void);
void CB2_RogueRunSummary(void);
// The UNLOCKS screen from the main menu. Returns to the main menu.
void CB2_RogueUnlocks(void);

// RUN 0 (v1.0): the Director, the run kind, the run log.
#include "rogue_run0.h"

// RUN 0 in the game (src/rogue_run0_region.c; the region: src/rogue_gen_run0.h).
struct WarpEvent;
struct MapHeader;
struct RogueMap;
bool32 Rogue_Run0Active(void);
void Rogue_Run0Generated(u32 place);
bool32 Rogue_Run0Entering(u32 place);
void Rogue_Run0Entered(u32 place);
s32 Rogue_Run0IslandHere(void);
bool32 Rogue_Run0IslandDoorDest(const struct RogueMap *m, u32 house, struct WarpEvent *w);
bool32 Rogue_Run0WarpOut(s8 warpEventId);
void Rogue_Run0PatchHeader(struct MapHeader *h, u16 group, u16 num);
const u16 *Rogue_Run0Border(void);
void Rogue_Run0OnObjectsLoaded(void);
bool32 Rogue_Run0HiddenItem(void);
void Rogue_Run0Field(void);
void Rogue_ResetMusicPitch(void);
void Rogue_Run0TintPalette(u16 offset, u16 count, bool8 skipFaded);
u16 Rogue_Run0HookGfx(u32 hook);
void Rogue_Run0HookTalk(void);
void Rogue_Run0ThingHook(void);

#endif // GUARD_ROGUE_H
