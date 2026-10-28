// Game-side glue for the generated region.
//
// The plan of the region and each place's map come from src/rogue_gen.c. This
// file turns them into the engine's own structures (map headers, layouts,
// events, connections), which are served through small hooks in the engine's
// lookups. A place's blocks and events are only generated when the game needs
// them, into a small cache, so the region can be much larger than the memory
// it would take to hold every map at once.
//
// It also generates wild encounter tables, trainer teams, gym leaders, the
// Elite Four, starters and berry trees from the seed.

#include "global.h"
#include "rogue.h"
#include "rogue_run0_story.h"
#include "battle_util.h"
#include "rogue_gen.h"
#include "rogue_loading.h"
#include "battle_main.h"
#include "battle_setup.h"
#include "berry.h"
#include "data.h"
#include "event_data.h"
#include "event_scripts.h"
#include "field_screen_effect.h"
#include "fieldmap.h"
#include "field_player_avatar.h"
#include "item.h"
#include "event_object_movement.h"
#include "main.h"
#include "malloc.h"
#include "metatile_behavior.h"
#include "naming_screen.h"
#include "overworld.h"
#include "pokedex.h"
#include "pokemon.h"
#include "random.h"
#include "script.h"
#include "script_pokemon_util.h"
#include "battle.h"
#include "string_util.h"
#include "rtc.h"
#include "text.h"
#include "tilesets.h"
#include "wild_encounter.h"
#include "constants/characters.h"
#include "constants/battle_ai.h"
#include "constants/berry.h"
#include "constants/event_bg.h"
#include "constants/event_object_movement.h"
#include "constants/event_objects.h"
#include "constants/items.h"
#include "constants/layouts.h"
#include "constants/map_groups.h"
#include "constants/map_types.h"
#include "constants/metatile_labels.h"
#include "constants/moves.h"
#include "constants/songs.h"
#include "constants/opponents.h"
#include "constants/pokeball.h"
#include "constants/region_map_sections.h"
#include "constants/trainer_types.h"
#include "constants/trainers.h"
#include "constants/weather.h"

// The vanilla layouts and map types of the rooms towers and hideouts are
// built from (the rooms themselves are in the generator).
#define ROGUE_ROOMS_LAYOUTS_ONLY
#include "data/rogue_rooms.h"

#define ROGUE_GROUP MAP_GROUP(MAP_ROGUE_PLACE_0)

#define POOL_MAX 640
#define WILD_SPECIES_PER_AREA 6
#define FISH_SPECIES 3
#define SURF_SPECIES 4
#define NUM_TIERS 11

extern const struct MapLayout *const gMapLayouts[];
extern const struct MapHeader *const *const gMapGroups[];

// Scripts in data/scripts/rogue.inc
extern const u8 Rogue_EventScript_FindItem[];
extern const u8 Rogue_EventScript_DungeonBoss[];
extern const u8 Rogue_EventScript_LegendSeal[];
extern const u8 Rogue_EventScript_TunnelGuard[];
extern const u8 Rogue_EventScript_DungeonMaster[];
extern const u8 Rogue_EventScript_ChoiceItem[];
extern const u8 Rogue_EventScript_PrizeItem[];
extern const u8 Rogue_EventScript_ChoiceGuide[];
extern const u8 Rogue_EventScript_Camp[];
extern const u8 Rogue_EventScript_DungeonSign[];
extern const u8 Rogue_EventScript_ForestSign[];
extern const u8 Rogue_EventScript_LegendCarving[];
extern const u8 Rogue_EventScript_RuinsTablet[];
extern const u8 Rogue_EventScript_SigFolk[];
extern const u8 Rogue_EventScript_LandmarkSign[];
extern const u8 Rogue_EventScript_FeatureSign[];
extern const u8 Rogue_EventScript_FeatFolk[];
extern const u8 Rogue_EventScript_Snorlax[];
extern const u8 *const gRoguePairScripts[];
extern const u8 Rogue_EventScript_Explorer[];
extern const u8 Rogue_RuinsFloor_MapScripts[];
extern const u8 Rogue_RaftVillage_MapScripts[];
extern const u8 Rogue_Sandstorm_MapScripts[]; // (v1.1 peaks)
extern const u8 Rogue_EventScript_TownSign[];
extern const u8 Rogue_EventScript_RouteSign[];
extern const u8 Rogue_EventScript_GymSign[];
extern const u8 Rogue_EventScript_LockedDoor[];
extern const u8 Rogue_EventScript_ShipDoor[];
extern const u8 Rogue_EventScript_Decoy[];
extern const u8 Rogue_ShipFloor_MapScripts[];
extern const u8 Rogue_PlantFloor_MapScripts[];
extern const u8 NewMauville_Inside_EventScript_BlueButton[];
extern const u8 NewMauville_Inside_EventScript_GreenButton[];
extern const u8 Rogue_EventScript_HouseSign[];
extern const u8 Rogue_EventScript_LabSign[];
extern const u8 Rogue_EventScript_GameCornerSign[];
extern const u8 Rogue_EventScript_BattleTentSign[];
extern const u8 Rogue_EventScript_MotelSign[];
extern const u8 Rogue_EventScript_RestSign[];
extern const u8 Rogue_EventScript_SchoolSign[];
extern const u8 Rogue_EventScript_FanClubSign[];
extern const u8 Rogue_EventScript_BikeShopSign[];
extern const u8 Rogue_EventScript_NameRaterSign[];
extern const u8 Rogue_EventScript_MoveDeleterSign[];
extern const u8 Rogue_EventScript_TutorSign[];
extern const u8 Rogue_EventScript_GiftHouseSign[];
extern const u8 Rogue_EventScript_GateNorth[];
extern const u8 Rogue_EventScript_GateEast[];
extern const u8 Rogue_EventScript_GateSouth[];
extern const u8 Rogue_EventScript_GateWest[];
extern const u8 Rogue_EventScript_BadgeGateNorth[];
extern const u8 Rogue_EventScript_BadgeGateEast[];
extern const u8 Rogue_EventScript_BadgeGateSouth[];
extern const u8 Rogue_EventScript_BadgeGateWest[];
extern const u8 Rogue_EventScript_Townsperson[];
extern const u8 Rogue_EventScript_Wanderer[];
extern const u8 Rogue_EventScript_TownMon[];
extern const u8 Rogue_EventScript_CutTree[];
extern const u8 Rogue_EventScript_RockSmash[];
extern const u8 Rogue_EventScript_Boulder[];
extern const u8 *const gRogueTrainerScripts[];
extern const u8 *const gRogueRivalScripts[];
extern const u8 Rogue_EventScript_VillainGrunt[];
extern const u8 Rogue_EventScript_VillainAdmin[];
extern const u8 Rogue_EventScript_VillainLeader[];
extern const u8 Rogue_EventScript_VillainGateNorth[];
extern const u8 Rogue_EventScript_VillainGateEast[];
extern const u8 Rogue_EventScript_VillainGateSouth[];
extern const u8 Rogue_EventScript_VillainGateWest[];

// ---------------------------------------------------------------------------
// Gym leaders and the Elite Four
//
// Each run draws eight gym leaders and four Elite Four members from one pool:
// Roguémon's own eight leaders, Emerald's eight gym leaders and Elite Four,
// and FireRed and LeafGreen's eight gym leaders and Elite Four. No two of
// them share a type, so every run has twelve.

struct LeaderDef
{
    enum Type type;
    u8 name[TRAINER_NAME_LENGTH + 1];
    enum TrainerPicID pic;
    u16 gfx;
    u8 gender;
    u8 doubles;        // battles two at a time (Tate and Liza)
    // Their lines (intro, defeat, after, gossip, and the Elite Four's intro
    // and after) are in the generator (src/rogue_text_leaders.h).
    u8 badge[16];      // the badge they give as a gym leader
    // Optional (zero for none): the signature ace's line, first stage first,
    // as far as it may evolve (Onix to Steelix, Rhyhorn to Rhydon but not
    // Rhyperior), and first stages the leader likes on the rest of the team.
    u16 ace[3];
    u16 team[3];
};

// Roguémon's own leaders have their own battle sprites (generated with Retro
// Diffusion from Emerald references; sources in the project's art/leaders3
// folder) and overworld sprites recoloured from vanilla ones to match
// (tools/roguemon/recolor_leaders.py). Then Emerald's gym leaders and Elite
// Four, and FireRed and LeafGreen's (their badges are Kanto's; the Elite
// Four's, for a gym, Johto's and Sinnoh's of the same type). In the order of
// gRogueGenLeaders (src/rogue_gen_people.h).
static const struct LeaderDef sLeaders[] =
{
    { TYPE_ROCK,     _("CRAG"),     TRAINER_PIC_ROGUE_LEADER_ROCK,      OBJ_EVENT_GFX_ROGUE_LEADER_ROCK,     MALE,   FALSE, _("BEDROCK BADGE") },
    { TYPE_WATER,    _("MARINA"),   TRAINER_PIC_ROGUE_LEADER_WATER,     OBJ_EVENT_GFX_ROGUE_LEADER_WATER,    FEMALE, FALSE, _("TIDE BADGE") },
    { TYPE_GRASS,    _("BASIL"),    TRAINER_PIC_ROGUE_LEADER_GRASS,     OBJ_EVENT_GFX_ROGUE_LEADER_GRASS,    MALE,   FALSE, _("BLOSSOM BADGE") },
    { TYPE_FIRE,     _("CINDER"),   TRAINER_PIC_ROGUE_LEADER_FIRE,      OBJ_EVENT_GFX_ROGUE_LEADER_FIRE,     MALE,   FALSE, _("FORGE BADGE") },
    { TYPE_ELECTRIC, _("VOLTZ"),    TRAINER_PIC_ROGUE_LEADER_ELECTRIC,  OBJ_EVENT_GFX_ROGUE_LEADER_ELECTRIC, MALE,   FALSE, _("VOLT BADGE") },
    { TYPE_FIGHTING, _("MAKO"),     TRAINER_PIC_ROGUE_LEADER_FIGHTING,  OBJ_EVENT_GFX_ROGUE_LEADER_FIGHTING, FEMALE, FALSE, _("FIST BADGE") },
    { TYPE_PSYCHIC,  _("LUNA"),     TRAINER_PIC_ROGUE_LEADER_PSYCHIC,   OBJ_EVENT_GFX_ROGUE_LEADER_PSYCHIC,  FEMALE, FALSE, _("DREAM BADGE") },
    { TYPE_FLYING,   _("ACE"),      TRAINER_PIC_ROGUE_LEADER_FLYING,    OBJ_EVENT_GFX_ROGUE_LEADER_FLYING,   MALE,   FALSE, _("GALE BADGE") },
    { TYPE_ROCK,     _("ROXANNE"),  TRAINER_PIC_LEADER_ROXANNE,         OBJ_EVENT_GFX_ROXANNE,               FEMALE, FALSE, _("STONE BADGE") },
    { TYPE_FIGHTING, _("BRAWLY"),   TRAINER_PIC_LEADER_BRAWLY,          OBJ_EVENT_GFX_BRAWLY,                MALE,   FALSE, _("KNUCKLE BADGE") },
    { TYPE_ELECTRIC, _("WATTSON"),  TRAINER_PIC_LEADER_WATTSON,         OBJ_EVENT_GFX_WATTSON,               MALE,   FALSE, _("DYNAMO BADGE") },
    { TYPE_FIRE,     _("FLANNERY"), TRAINER_PIC_LEADER_FLANNERY,        OBJ_EVENT_GFX_FLANNERY,              FEMALE, FALSE, _("HEAT BADGE") },
    { TYPE_NORMAL,   _("NORMAN"),   TRAINER_PIC_LEADER_NORMAN,          OBJ_EVENT_GFX_NORMAN,                MALE,   FALSE, _("BALANCE BADGE") },
    { TYPE_FLYING,   _("WINONA"),   TRAINER_PIC_LEADER_WINONA,          OBJ_EVENT_GFX_WINONA,                FEMALE, FALSE, _("FEATHER BADGE") },
    { TYPE_PSYCHIC,  _("TATE&LIZA"),TRAINER_PIC_LEADER_TATE_AND_LIZA,   OBJ_EVENT_GFX_TATE,                  MALE,   TRUE, _("MIND BADGE") },
    { TYPE_WATER,    _("JUAN"),     TRAINER_PIC_LEADER_JUAN,            OBJ_EVENT_GFX_JUAN,                  MALE,   FALSE, _("RAIN BADGE") },
    { TYPE_DARK,     _("SIDNEY"),   TRAINER_PIC_ELITE_FOUR_SIDNEY,      OBJ_EVENT_GFX_SIDNEY,                MALE,   FALSE, _("DUSK BADGE") },
    { TYPE_GHOST,    _("PHOEBE"),   TRAINER_PIC_ELITE_FOUR_PHOEBE,      OBJ_EVENT_GFX_PHOEBE,                FEMALE, FALSE, _("SPIRIT BADGE") },
    { TYPE_ICE,      _("GLACIA"),   TRAINER_PIC_ELITE_FOUR_GLACIA,      OBJ_EVENT_GFX_GLACIA,                FEMALE, FALSE, _("GLACIER BADGE") },
    { TYPE_DRAGON,   _("DRAKE"),    TRAINER_PIC_ELITE_FOUR_DRAKE,       OBJ_EVENT_GFX_DRAKE,                 MALE,   FALSE, _("WYRM BADGE") },
    // FireRed and LeafGreen.
    { TYPE_ROCK,     _("BROCK"),    TRAINER_PIC_LEADER_BROCK_FRLG,      OBJ_EVENT_GFX_BROCK,                 MALE,   FALSE, _("BOULDER BADGE"),    { SPECIES_ONIX, SPECIES_STEELIX },                       { SPECIES_GEODUDE, SPECIES_RHYHORN, SPECIES_KABUTO } },
    { TYPE_WATER,    _("MISTY"),    TRAINER_PIC_LEADER_MISTY_FRLG,      OBJ_EVENT_GFX_MISTY,                 FEMALE, FALSE, _("CASCADE BADGE"),    { SPECIES_STARYU, SPECIES_STARMIE },                     { SPECIES_PSYDUCK, SPECIES_HORSEA, SPECIES_GOLDEEN } },
    { TYPE_ELECTRIC, _("LT. SURGE"),TRAINER_PIC_LEADER_LT_SURGE_FRLG,   OBJ_EVENT_GFX_LT_SURGE,              MALE,   FALSE, _("THUNDER BADGE"),    { SPECIES_PIKACHU, SPECIES_RAICHU },                     { SPECIES_VOLTORB, SPECIES_MAGNEMITE, SPECIES_ELEKID } },
    { TYPE_GRASS,    _("ERIKA"),    TRAINER_PIC_LEADER_ERIKA_FRLG,      OBJ_EVENT_GFX_ERIKA,                 FEMALE, FALSE, _("RAINBOW BADGE"),    { SPECIES_ODDISH, SPECIES_GLOOM, SPECIES_VILEPLUME },    { SPECIES_BELLSPROUT, SPECIES_TANGELA, SPECIES_EXEGGCUTE } },
    { TYPE_POISON,   _("KOGA"),     TRAINER_PIC_LEADER_KOGA_FRLG,       OBJ_EVENT_GFX_KOGA,                  MALE,   FALSE, _("SOUL BADGE"),     { SPECIES_KOFFING, SPECIES_WEEZING },                    { SPECIES_GRIMER, SPECIES_VENONAT, SPECIES_SPINARAK } },
    { TYPE_PSYCHIC,  _("SABRINA"),  TRAINER_PIC_LEADER_SABRINA_FRLG,    OBJ_EVENT_GFX_SABRINA,               FEMALE, FALSE, _("MARSH BADGE"),  { SPECIES_ABRA, SPECIES_KADABRA, SPECIES_ALAKAZAM },     { SPECIES_DROWZEE, SPECIES_MIME_JR, SPECIES_SLOWPOKE } },
    { TYPE_FIRE,     _("BLAINE"),   TRAINER_PIC_LEADER_BLAINE_FRLG,     OBJ_EVENT_GFX_BLAINE,                MALE,   FALSE, _("VOLCANO BADGE"),   { SPECIES_GROWLITHE, SPECIES_ARCANINE },                 { SPECIES_PONYTA, SPECIES_VULPIX, SPECIES_MAGBY } },
    { TYPE_GROUND,   _("GIOVANNI"), TRAINER_PIC_LEADER_GIOVANNI_FRLG,   OBJ_EVENT_GFX_GIOVANNI,              MALE,   FALSE, _("EARTH BADGE"), { SPECIES_RHYHORN, SPECIES_RHYDON },                     { SPECIES_DIGLETT, SPECIES_CUBONE, SPECIES_SANDSHREW } },
    { TYPE_ICE,      _("LORELEI"),  TRAINER_PIC_ELITE_FOUR_LORELEI_FRLG, OBJ_EVENT_GFX_LORELEI,              FEMALE, FALSE, _("ICICLE BADGE"),  { SPECIES_LAPRAS },                                      { SPECIES_SEEL, SPECIES_SHELLDER, SPECIES_SMOOCHUM } },
    { TYPE_FIGHTING, _("BRUNO"),    TRAINER_PIC_ELITE_FOUR_BRUNO_FRLG,  OBJ_EVENT_GFX_BRUNO,                 MALE,   FALSE, _("STORM BADGE"),    { SPECIES_MACHOP, SPECIES_MACHOKE, SPECIES_MACHAMP },    { SPECIES_TYROGUE, SPECIES_MANKEY, SPECIES_POLIWAG } },
    { TYPE_GHOST,    _("AGATHA"),   TRAINER_PIC_ELITE_FOUR_AGATHA_FRLG, OBJ_EVENT_GFX_AGATHA,                FEMALE, FALSE, _("FOG BADGE"),   { SPECIES_GASTLY, SPECIES_HAUNTER, SPECIES_GENGAR },     { SPECIES_MISDREAVUS } },
    { TYPE_DRAGON,   _("LANCE"),    TRAINER_PIC_ELITE_FOUR_LANCE_FRLG,  OBJ_EVENT_GFX_LANCE,                 MALE,   FALSE, _("RISING BADGE"),    { SPECIES_DRATINI, SPECIES_DRAGONAIR, SPECIES_DRAGONITE }, { SPECIES_HORSEA } },
};

// The CHAMPION is the wanderer (RogueGen_Cast): their look gives their
// sprites, and their team follows what they love (src/rogue_text_leaders.h
// has their words).
struct ChampionLook
{
    u16 pic, gfx;
    u8 music;
    u16 aces[3];         // the ace, one of these by the seed
    const u16 *team;     // the rest of the team, five of these: as STEVEN's
    u8 teamCount;        // and WALLACE's teams fit their love of stones and water
};

// What each look's CHAMPION keeps besides their ace (the aces may appear
// here too: never twice in one team).
static const u16 sChampionTeamSteven[] =
{
    SPECIES_SKARMORY, SPECIES_CLAYDOL, SPECIES_CRADILY, SPECIES_ARMALDO, SPECIES_AGGRON, SPECIES_GOLEM,
    SPECIES_PROBOPASS, SPECIES_RHYPERIOR, SPECIES_EXCADRILL, SPECIES_AERODACTYL, SPECIES_BRONZONG,
    SPECIES_GIGALITH, SPECIES_BASTIODON, SPECIES_RAMPARDOS, SPECIES_TYRANITAR, SPECIES_METAGROSS,
};
static const u16 sChampionTeamWallace[] =
{
    SPECIES_WAILORD, SPECIES_TENTACRUEL, SPECIES_LUDICOLO, SPECIES_WHISCASH, SPECIES_GYARADOS, SPECIES_MILOTIC,
    SPECIES_KINGDRA, SPECIES_LAPRAS, SPECIES_STARMIE, SPECIES_VAPOREON, SPECIES_LANTURN, SPECIES_DEWGONG,
    SPECIES_ALTARIA, SPECIES_LUMINEON, SPECIES_SEAKING,
};
static const u16 sChampionTeamAce[] =
{
    SPECIES_ARCANINE, SPECIES_STARMIE, SPECIES_JOLTEON, SPECIES_BRELOOM, SPECIES_GARDEVOIR, SPECIES_STARAPTOR,
    SPECIES_SCIZOR, SPECIES_TOGEKISS, SPECIES_ROSERADE, SPECIES_MAGNEZONE, SPECIES_WEAVILE, SPECIES_HERACROSS,
    SPECIES_LUCARIO, SPECIES_GARCHOMP, SPECIES_SALAMENCE,
};
static const u16 sChampionTeamVeteran[] =
{
    SPECIES_GENGAR, SPECIES_DUSKNOIR, SPECIES_SNORLAX, SPECIES_URSARING, SPECIES_KANGASKHAN, SPECIES_HITMONLEE,
    SPECIES_HITMONCHAN, SPECIES_POLIWRATH, SPECIES_XATU, SPECIES_SLOWKING, SPECIES_NOCTOWL, SPECIES_MISMAGIUS,
    SPECIES_BANETTE, SPECIES_MEDICHAM, SPECIES_HARIYAMA, SPECIES_ALAKAZAM, SPECIES_MACHAMP, SPECIES_DRAGONITE,
};
static const struct ChampionLook sChampionLooks[RLOOK_COUNT] =
{
    // STEVEN's: stones and caves.
    [RLOOK_STEVEN]  = { TRAINER_PIC_STEVEN, OBJ_EVENT_GFX_STEVEN, TRAINER_ENCOUNTER_MUSIC_MALE,
                        { SPECIES_METAGROSS, SPECIES_AGGRON, SPECIES_TYRANITAR },
                        sChampionTeamSteven, ARRAY_COUNT(sChampionTeamSteven) },
    // Champion WALLACE's: water and grace.
    [RLOOK_WALLACE] = { TRAINER_PIC_CHAMPION_WALLACE, OBJ_EVENT_GFX_WALLACE, TRAINER_ENCOUNTER_MUSIC_MALE,
                        { SPECIES_MILOTIC, SPECIES_GYARADOS, SPECIES_KINGDRA },
                        sChampionTeamWallace, ARRAY_COUNT(sChampionTeamWallace) },
    // A COOLTRAINER's: a team for every matchup.
    [RLOOK_ACE]     = { TRAINER_PIC_COOLTRAINER_F, OBJ_EVENT_GFX_WOMAN_5, TRAINER_ENCOUNTER_MUSIC_COOL,
                        { SPECIES_SALAMENCE, SPECIES_GARCHOMP, SPECIES_LUCARIO },
                        sChampionTeamAce, ARRAY_COUNT(sChampionTeamAce) },
    // An EXPERT's: old roads and old tales.
    [RLOOK_VETERAN] = { TRAINER_PIC_EXPERT_F, OBJ_EVENT_GFX_EXPERT_F, TRAINER_ENCOUNTER_MUSIC_INTENSE,
                        { SPECIES_DRAGONITE, SPECIES_ALAKAZAM, SPECIES_MACHAMP },
                        sChampionTeamVeteran, ARRAY_COUNT(sChampionTeamVeteran) },
};

STATIC_ASSERT(ARRAY_COUNT(sLeaders) == ROGUE_LEADER_COUNT, GenLeadersMatchLeaders);

struct ClassDef
{
    u8 trainerClass;
    enum TrainerPicID pic;
    u16 gfx;
    u8 gender;
    u8 music;
    enum Type type;  // the team's type on routes, TYPE_NONE for any
    u8 firstRoute;   // earliest route this class appears on
    u8 talk;         // enum TrainerTalk: what kind of trainer, for where it stands
};

enum TrainerTalk
{
    TALK_KID,
    TALK_BUGS,
    TALK_HIKER,
    TALK_OUTDOORS,
    TALK_FIGHTER,
    TALK_WATER,
    TALK_FANCY,
    TALK_MYSTIC,
    TALK_BIRDS,
    TALK_FIRE,
    TALK_MUSIC,
    TALK_ACE,
    TALK_SAILOR,
    TALK_COUNT,
};

static const struct ClassDef sClasses[] =
{
    { TRAINER_CLASS_YOUNGSTER,   TRAINER_PIC_YOUNGSTER,     OBJ_EVENT_GFX_YOUNGSTER,    MALE,   TRAINER_ENCOUNTER_MUSIC_MALE,       TYPE_NONE,     0, TALK_KID },
    { TRAINER_CLASS_LASS,        TRAINER_PIC_LASS,          OBJ_EVENT_GFX_LASS,         FEMALE, TRAINER_ENCOUNTER_MUSIC_FEMALE,     TYPE_NONE,     0, TALK_KID },
    { TRAINER_CLASS_BUG_CATCHER, TRAINER_PIC_BUG_CATCHER,   OBJ_EVENT_GFX_BUG_CATCHER,  MALE,   TRAINER_ENCOUNTER_MUSIC_MALE,       TYPE_BUG,      0, TALK_BUGS },
    { TRAINER_CLASS_CAMPER,      TRAINER_PIC_CAMPER,        OBJ_EVENT_GFX_CAMPER,       MALE,   TRAINER_ENCOUNTER_MUSIC_MALE,       TYPE_NONE,     0, TALK_OUTDOORS },
    { TRAINER_CLASS_PICNICKER,   TRAINER_PIC_PICNICKER,     OBJ_EVENT_GFX_PICNICKER,    FEMALE, TRAINER_ENCOUNTER_MUSIC_FEMALE,     TYPE_GRASS,    1, TALK_OUTDOORS },
    { TRAINER_CLASS_FISHERMAN,   TRAINER_PIC_FISHERMAN,     OBJ_EVENT_GFX_FISHERMAN,    MALE,   TRAINER_ENCOUNTER_MUSIC_HIKER,      TYPE_WATER,    1, TALK_WATER },
    { TRAINER_CLASS_BIRD_KEEPER, TRAINER_PIC_BIRD_KEEPER,   OBJ_EVENT_GFX_MAN_5,        MALE,   TRAINER_ENCOUNTER_MUSIC_COOL,       TYPE_FLYING,   0, TALK_BIRDS },
    { TRAINER_CLASS_TUBER_F,     TRAINER_PIC_TUBER_F,       OBJ_EVENT_GFX_TUBER_F,      FEMALE, TRAINER_ENCOUNTER_MUSIC_GIRL,       TYPE_WATER,    1, TALK_KID },
    { TRAINER_CLASS_SCHOOL_KID,  TRAINER_PIC_SCHOOL_KID_M,  OBJ_EVENT_GFX_SCHOOL_KID_M, MALE,   TRAINER_ENCOUNTER_MUSIC_MALE,       TYPE_NONE,     1, TALK_KID },
    { TRAINER_CLASS_HIKER,       TRAINER_PIC_HIKER,         OBJ_EVENT_GFX_HIKER,        MALE,   TRAINER_ENCOUNTER_MUSIC_HIKER,      TYPE_ROCK,     1, TALK_HIKER },
    { TRAINER_CLASS_RUIN_MANIAC, TRAINER_PIC_RUIN_MANIAC,   OBJ_EVENT_GFX_HIKER,        MALE,   TRAINER_ENCOUNTER_MUSIC_HIKER,      TYPE_GROUND,   1, TALK_HIKER },
    { TRAINER_CLASS_NINJA_BOY,   TRAINER_PIC_NINJA_BOY,     OBJ_EVENT_GFX_NINJA_BOY,    MALE,   TRAINER_ENCOUNTER_MUSIC_MALE,       TYPE_POISON,   1, TALK_KID },
    { TRAINER_CLASS_BLACK_BELT,  TRAINER_PIC_BLACK_BELT,    OBJ_EVENT_GFX_BLACK_BELT,   MALE,   TRAINER_ENCOUNTER_MUSIC_INTENSE,    TYPE_FIGHTING, 1, TALK_FIGHTER },
    { TRAINER_CLASS_BATTLE_GIRL, TRAINER_PIC_BATTLE_GIRL,   OBJ_EVENT_GFX_GIRL_3,       FEMALE, TRAINER_ENCOUNTER_MUSIC_INTENSE,    TYPE_FIGHTING, 1, TALK_FIGHTER },
    { TRAINER_CLASS_KINDLER,     TRAINER_PIC_KINDLER,       OBJ_EVENT_GFX_MAN_5,        MALE,   TRAINER_ENCOUNTER_MUSIC_HIKER,      TYPE_FIRE,     1, TALK_FIRE },
    { TRAINER_CLASS_AROMA_LADY,  TRAINER_PIC_AROMA_LADY,    OBJ_EVENT_GFX_WOMAN_2,      FEMALE, TRAINER_ENCOUNTER_MUSIC_FEMALE,     TYPE_GRASS,    1, TALK_FANCY },
    { TRAINER_CLASS_BEAUTY,      TRAINER_PIC_BEAUTY,        OBJ_EVENT_GFX_BEAUTY,       FEMALE, TRAINER_ENCOUNTER_MUSIC_FEMALE,     TYPE_FAIRY,    1, TALK_FANCY },
    { TRAINER_CLASS_GUITARIST,   TRAINER_PIC_GUITARIST,     OBJ_EVENT_GFX_MAN_5,        MALE,   TRAINER_ENCOUNTER_MUSIC_INTENSE,    TYPE_ELECTRIC, 1, TALK_MUSIC },
    { TRAINER_CLASS_SAILOR,      TRAINER_PIC_SAILOR,        OBJ_EVENT_GFX_SAILOR,       MALE,   TRAINER_ENCOUNTER_MUSIC_MALE,       TYPE_WATER,    1, TALK_SAILOR },
    { TRAINER_CLASS_POKEFAN,     TRAINER_PIC_POKEFAN_F,     OBJ_EVENT_GFX_POKEFAN_F,    FEMALE, TRAINER_ENCOUNTER_MUSIC_TWINS,      TYPE_NONE,     1, TALK_FANCY },
    { TRAINER_CLASS_PSYCHIC,     TRAINER_PIC_PSYCHIC_M,     OBJ_EVENT_GFX_PSYCHIC_M,    MALE,   TRAINER_ENCOUNTER_MUSIC_INTENSE,    TYPE_PSYCHIC,  2, TALK_MYSTIC },
    { TRAINER_CLASS_HEX_MANIAC,  TRAINER_PIC_HEX_MANIAC,    OBJ_EVENT_GFX_HEX_MANIAC,   FEMALE, TRAINER_ENCOUNTER_MUSIC_SUSPICIOUS, TYPE_GHOST,    2, TALK_MYSTIC },
    { TRAINER_CLASS_GENTLEMAN,   TRAINER_PIC_GENTLEMAN,     OBJ_EVENT_GFX_GENTLEMAN,    MALE,   TRAINER_ENCOUNTER_MUSIC_RICH,       TYPE_NONE,     2, TALK_FANCY },
    { TRAINER_CLASS_COOLTRAINER, TRAINER_PIC_COOLTRAINER_M, OBJ_EVENT_GFX_MAN_3,        MALE,   TRAINER_ENCOUNTER_MUSIC_COOL,       TYPE_NONE,     3, TALK_ACE },
    { TRAINER_CLASS_COOLTRAINER, TRAINER_PIC_COOLTRAINER_F, OBJ_EVENT_GFX_WOMAN_5,      FEMALE, TRAINER_ENCOUNTER_MUSIC_COOL,       TYPE_NONE,     3, TALK_ACE },
    { TRAINER_CLASS_EXPERT,      TRAINER_PIC_EXPERT_M,      OBJ_EVENT_GFX_EXPERT_M,     MALE,   TRAINER_ENCOUNTER_MUSIC_INTENSE,    TYPE_NONE,     3, TALK_ACE },
    // v1.1 sea: out on the water of a maze or the shallows (RCLASS_SWIMMER_M).
    { TRAINER_CLASS_SWIMMER_M,   TRAINER_PIC_SWIMMER_M,     OBJ_EVENT_GFX_SWIMMER_M,    MALE,   TRAINER_ENCOUNTER_MUSIC_SWIMMER,    TYPE_WATER,    1, TALK_WATER },
    { TRAINER_CLASS_SWIMMER_F,   TRAINER_PIC_SWIMMER_F,     OBJ_EVENT_GFX_SWIMMER_F,    FEMALE, TRAINER_ENCOUNTER_MUSIC_SWIMMER,    TYPE_WATER,    1, TALK_WATER },
};

STATIC_ASSERT(ARRAY_COUNT(sClasses) == ROGUE_CLASS_COUNT, ClassesMatchGenClasses);

#define CLASS_HIKER      9
#define CLASS_BLACK_BELT 12
#define CLASS_RUIN       10
#define CLASS_PSYCHIC    20
#define CLASS_HEX_MANIAC 21

// Trainer lines by class live in the generator (src/rogue_text_trainers.h).

// Names for generated trainers, enough that no two in a region share one
// (GenerateContent): a region has up to about 140 men and 65 women. None is
// a gym leader's, the cast's (the reporters, the elders, the explorers,
// src/rogue_gen_facts.h), a family's or a vanilla character's.
static const u8 sMaleNames[][TRAINER_NAME_LENGTH + 1] =
{
    _("ALDO"), _("COLE"), _("DREW"), _("EVAN"), _("FINN"), _("HUGO"), _("IVAN"), _("JOEL"), _("KIRK"), _("LIAM"),
    _("MILO"), _("NOAH"), _("OWEN"), _("REED"), _("SETH"), _("TOBY"), _("VIC"), _("WADE"), _("ABEL"), _("BEN"),
    _("CARL"), _("DEAN"), _("ELI"), _("FRED"), _("GLEN"), _("HANK"), _("JAKE"), _("KYLE"), _("LUKE"), _("MARK"),
    _("OTTO"), _("PAUL"), _("SAM"), _("TED"), _("ROSS"), _("NICK"), _("MAX"), _("LEO"), _("JON"), _("IAN"),
    _("DALE"), _("CODY"), _("ROY"), _("ZANE"), _("ALAN"), _("ARLO"), _("BART"), _("BERT"), _("BOYD"), _("BRETT"),
    _("BURT"), _("CARY"), _("CHAD"), _("CHET"), _("CLAY"), _("CLIFF"), _("CLYDE"), _("CRAIG"), _("CURT"), _("DARYL"),
    _("DEREK"), _("DIRK"), _("DOUG"), _("DUKE"), _("EARL"), _("ELLIS"), _("EMIL"), _("ERIC"), _("ERNIE"), _("FORD"),
    _("FRANK"), _("GENE"), _("GRANT"), _("GREG"), _("GUY"), _("HANS"), _("HARVEY"), _("HECTOR"), _("HERB"), _("HOMER"),
    _("HOWIE"), _("HUGH"), _("IKE"), _("IRA"), _("JACK"), _("JEFF"), _("JESSE"), _("JIM"), _("JOE"), _("KEITH"),
    _("KEN"), _("KENT"), _("KURT"), _("LANE"), _("LARS"), _("LEN"), _("LLOYD"), _("LYLE"), _("MAC"), _("MEL"),
    _("MITCH"), _("MONTY"), _("MORT"), _("NATE"), _("NED"), _("NEIL"), _("NILS"), _("OLLIE"), _("OREN"), _("OSCAR"),
    _("PETE"), _("PHIL"), _("QUINN"), _("RALPH"), _("RANDY"), _("REX"), _("RHYS"), _("RICK"), _("ROD"), _("RON"),
    _("RUSS"), _("RUDY"), _("SHANE"), _("SKIP"), _("STAN"), _("STU"), _("TIM"), _("TONY"), _("TRENT"), _("TROY"),
    _("VERN"), _("WALT"), _("WES"), _("WILL"), _("WYATT"), _("ZACH"), _("ALVIN"), _("AMOS"), _("ANGUS"), _("BEAU"),
    _("BRAD"), _("BRYCE"), _("CALVIN"), _("CASEY"), _("COLIN"), _("DAMON"), _("DENNY"), _("EMMET"), _("GIL"), _("GORDY"),
    _("JONAH"), _("JUDE"), _("KEVIN"), _("LEWIS"), _("MARTY"), _("MURRAY"), _("NORRIS"), _("ORSON"), _("PERRY"), _("REID"),
    _("RORY"), _("RUBEN"), _("SAUL"), _("SEAN"), _("SIMON"), _("TERRY"), _("THEO"), _("WAYNE"), _("ZEKE"),
};

static const u8 sFemaleNames[][TRAINER_NAME_LENGTH + 1] =
{
    _("ANNA"), _("BETH"), _("CORA"), _("DANA"), _("ELLA"), _("FAYE"), _("GWEN"), _("HOPE"), _("JUNE"), _("KATE"),
    _("LILA"), _("MAYA"), _("NINA"), _("OPAL"), _("RUBY"), _("SARA"), _("TESS"), _("UMA"), _("VERA"), _("ADA"),
    _("BREE"), _("CLEO"), _("DEB"), _("EVE"), _("FERN"), _("GAIL"), _("HEDY"), _("JADE"), _("LUCY"), _("MIA"),
    _("AMY"), _("BELL"), _("CARA"), _("DORA"), _("FLO"), _("GINA"), _("ISLA"), _("JILL"), _("KIM"), _("LENA"),
    _("MIRA"), _("OLGA"), _("RITA"), _("SUE"), _("ABBY"), _("ALMA"), _("APRIL"), _("BELLA"), _("CARLA"), _("CLARA"),
    _("DAISY"), _("DELLA"), _("DINA"), _("ELSA"), _("EMMA"), _("ESME"), _("GRACE"), _("HOLLY"), _("IDA"), _("JANE"),
    _("JOY"), _("JUNO"), _("LIZ"), _("LOIS"), _("LORNA"), _("MABEL"), _("MAUD"), _("NORA"), _("OLIVE"), _("PAM"),
    _("POLLY"), _("RAE"), _("ROSIE"), _("SADIE"), _("STELLA"), _("TINA"), _("TRUDY"), _("VAL"), _("VIV"), _("WANDA"),
    _("ZARA"), _("AGNES"), _("BONNIE"), _("CINDY"), _("ELLIE"), _("ETTA"), _("FIONA"), _("HATTIE"), _("JOAN"), _("KITTY"),
    _("LEAH"), _("MINA"), _("RUTH"), _("SALLY"), _("UNA"),
};
#define NAME_WORDS ((ARRAY_COUNT(sMaleNames) + 31) / 32)
STATIC_ASSERT(ARRAY_COUNT(sMaleNames) < 256 && ARRAY_COUNT(sFemaleNames) <= ARRAY_COUNT(sMaleNames), TrainerNamesFitAByte);
// The dojo's three keep their names in sTrainerNames' spare entries, below
// the route features' slots (a region uses at most 168 route slots).
#define DOJO_NAME_SLOT (ROGUE_SLOT_FEATURE_0 - ROGUE_DOJO_TRAINERS)

// Grass, Fire and Water starters from every generation.
static const u16 sStarters[3][9] =
{
    { SPECIES_BULBASAUR, SPECIES_CHIKORITA, SPECIES_TREECKO, SPECIES_TURTWIG, SPECIES_SNIVY, SPECIES_CHESPIN, SPECIES_ROWLET, SPECIES_GROOKEY, SPECIES_SPRIGATITO },
    { SPECIES_CHARMANDER, SPECIES_CYNDAQUIL, SPECIES_TORCHIC, SPECIES_CHIMCHAR, SPECIES_TEPIG, SPECIES_FENNEKIN, SPECIES_LITTEN, SPECIES_SCORBUNNY, SPECIES_FUECOCO },
    { SPECIES_SQUIRTLE, SPECIES_TOTODILE, SPECIES_MUDKIP, SPECIES_PIPLUP, SPECIES_OSHAWOTT, SPECIES_FROAKIE, SPECIES_POPPLIO, SPECIES_SOBBLE, SPECIES_QUAXLY },
};

// Extra starter choices earned as permanent unlocks.
static const u16 sPartnerStarters[] =
{
    SPECIES_PIKACHU, SPECIES_EEVEE, SPECIES_RIOLU, SPECIES_RALTS, SPECIES_SHINX, SPECIES_GROWLITHE,
    SPECIES_ABRA, SPECIES_GASTLY, SPECIES_MACHOP, SPECIES_TOGEPI, SPECIES_ZORUA, SPECIES_ROCKRUFF,
};

// Young dragons only, as the menu says YOUNG DRAGON: no Larvitar or Beldum,
// pseudo-legendaries but not Dragon types.
static const u16 sDragonStarters[] =
{
    SPECIES_DRATINI, SPECIES_AXEW, SPECIES_BAGON, SPECIES_GIBLE, SPECIES_DEINO,
    SPECIES_GOOMY, SPECIES_JANGMO_O, SPECIES_DREEPY, SPECIES_FRIGIBAX,
};

static const u16 sFossilStarters[] =
{
    SPECIES_OMANYTE, SPECIES_KABUTO, SPECIES_LILEEP, SPECIES_ANORITH, SPECIES_CRANIDOS,
    SPECIES_SHIELDON, SPECIES_TIRTOUGA, SPECIES_ARCHEN, SPECIES_TYRUNT, SPECIES_AMAURA,
};

// The starter rows of v0.13. Each region rolls its own from a stream of its
// own (as the fossil row does), so adding a row never changes a region.
// Babies: hatched from eggs in the real games, they grow up by friendship.
static const u16 sBabyStarters[] =
{
    SPECIES_PICHU, SPECIES_CLEFFA, SPECIES_IGGLYBUFF, SPECIES_TOGEPI, SPECIES_AZURILL, SPECIES_WYNAUT,
    SPECIES_RIOLU, SPECIES_MUNCHLAX, SPECIES_HAPPINY, SPECIES_BONSLY, SPECIES_MIME_JR, SPECIES_BUDEW,
    SPECIES_CHINGLING, SPECIES_ELEKID, SPECIES_MAGBY, SPECIES_SMOOCHUM, SPECIES_TYROGUE, SPECIES_MANTYKE,
};

// Rising stars: first forms of three-stage lines that are not starters,
// about as strong as a starter at level 5 (base stats 240 to 330; not
// Ralts, at 198, which the partners have).
static const u16 sRisingStarters[] =
{
    SPECIES_ABRA, SPECIES_MACHOP, SPECIES_GEODUDE, SPECIES_GASTLY, SPECIES_MAREEP,
    SPECIES_ARON, SPECIES_SHINX, SPECIES_STARLY, SPECIES_PIDOVE, SPECIES_ROGGENROLA, SPECIES_TIMBURR,
    SPECIES_LITWICK, SPECIES_FLETCHLING, SPECIES_ROOKIDEE, SPECIES_ROLYCOLY, SPECIES_TINKATINK,
    SPECIES_NIDORAN_F, SPECIES_NIDORAN_M, SPECIES_ODDISH, SPECIES_POLIWAG, SPECIES_BELLSPROUT,
    SPECIES_SEWADDLE, SPECIES_KLINK, SPECIES_HONEDGE,
};

// From far away: regional forms that are first forms. Not Paldean Tauros
// or Galarian Stunfisk, which never evolve and start out much stronger.
static const u16 sRegionalStarters[] =
{
    SPECIES_RATTATA_ALOLA, SPECIES_SANDSHREW_ALOLA, SPECIES_VULPIX_ALOLA, SPECIES_DIGLETT_ALOLA,
    SPECIES_MEOWTH_ALOLA, SPECIES_GEODUDE_ALOLA, SPECIES_GRIMER_ALOLA, SPECIES_MEOWTH_GALAR,
    SPECIES_PONYTA_GALAR, SPECIES_SLOWPOKE_GALAR, SPECIES_FARFETCHD_GALAR, SPECIES_CORSOLA_GALAR,
    SPECIES_ZIGZAGOON_GALAR, SPECIES_DARUMAKA_GALAR, SPECIES_YAMASK_GALAR, SPECIES_GROWLITHE_HISUI,
    SPECIES_VOLTORB_HISUI, SPECIES_QWILFISH_HISUI, SPECIES_SNEASEL_HISUI, SPECIES_ZORUA_HISUI,
    SPECIES_WOOPER_PALDEA,
};

static const u16 sEeveeStarter[] = { SPECIES_EEVEE };

STATIC_ASSERT(NUM_SPECIES <= ROGUE_STARTER_SPECIES_MASK + 1, RogueStarterSpeciesFits);
STATIC_ASSERT(ROGUE_STARTER_KIND_COUNT <= 16, RogueStarterKindFits);

// The held item a starter gets with the TYPE CHARM, by its first type.
static const u16 sTypeCharms[NUMBER_OF_MON_TYPES] =
{
    [TYPE_NORMAL] = ITEM_SILK_SCARF, [TYPE_FIGHTING] = ITEM_BLACK_BELT, [TYPE_FLYING] = ITEM_SHARP_BEAK,
    [TYPE_POISON] = ITEM_POISON_BARB, [TYPE_GROUND] = ITEM_SOFT_SAND, [TYPE_ROCK] = ITEM_HARD_STONE,
    [TYPE_BUG] = ITEM_SILVER_POWDER, [TYPE_GHOST] = ITEM_SPELL_TAG, [TYPE_STEEL] = ITEM_METAL_COAT,
    [TYPE_FIRE] = ITEM_CHARCOAL, [TYPE_WATER] = ITEM_MYSTIC_WATER, [TYPE_GRASS] = ITEM_MIRACLE_SEED,
    [TYPE_ELECTRIC] = ITEM_MAGNET, [TYPE_PSYCHIC] = ITEM_TWISTED_SPOON, [TYPE_ICE] = ITEM_NEVER_MELT_ICE,
    [TYPE_DRAGON] = ITEM_DRAGON_FANG, [TYPE_DARK] = ITEM_BLACK_GLASSES, [TYPE_FAIRY] = ITEM_FAIRY_FEATHER,
};

static const u16 sBerries[] =
{
    ITEM_ORAN_BERRY, ITEM_PECHA_BERRY, ITEM_CHERI_BERRY, ITEM_CHESTO_BERRY,
    ITEM_RAWST_BERRY, ITEM_ASPEAR_BERRY, ITEM_PERSIM_BERRY, ITEM_LEPPA_BERRY, ITEM_SITRUS_BERRY,
};

// Levels come from the region's curve (src/rogue_gen.c): each place has the
// level of its trainers, and wild Pokemon are a little lower, as in Emerald.
// Wild species are first stages within these base stat totals, by tier.
static const u16 sWildMaxBst[NUM_TIERS]     = { 300, 330, 360, 400, 430, 460, 480, 500, 500, 500, 500 };
static const u16 sWildMinBst[NUM_TIERS]     = { 0, 0, 200, 230, 260, 280, 300, 300, 320, 340, 350 };
// Team sizes of Emerald's leaders.
// The first leader brings one fewer, for a team that may have no answer to
// their type yet.
static const u8 sLeaderTeamSize[ROGUE_NUM_GYMS]  = { 2, 3, 4, 4, 4, 5, 4, 5 };
static const u16 sLeaderMaxBst[ROGUE_NUM_GYMS]   = { 360, 400, 430, 460, 480, 500, 500, 500 };
// Gyms field two trainers at first and three from the third gym on.
static const u8 sGymTrainers[ROGUE_NUM_GYMS]     = { 3, 2, 3, 3, 3, 3, 3, 3 };
// The Champion's team below his ace, as Wallace's is.
static const s8 sChampionLevelOffsets[] = { -3, -2, -2, -2, -1, 0 };
static const enum Type sCaveTypes[] = { TYPE_ROCK, TYPE_GROUND, TYPE_DARK, TYPE_POISON, TYPE_FIGHTING, TYPE_STEEL, TYPE_GHOST };

// Types that suit each kind of route, for about half its wild Pokemon.
static const enum Type sRouteTypes[RROUTE_COUNT][4] =
{
    [RROUTE_FOREST]   = { TYPE_BUG, TYPE_GRASS, TYPE_NORMAL, TYPE_FLYING },
    [RROUTE_LAKE]     = { TYPE_WATER, TYPE_BUG, TYPE_GRASS, TYPE_NORMAL },
    [RROUTE_MOUNTAIN] = { TYPE_ROCK, TYPE_FIGHTING, TYPE_DRAGON, TYPE_STEEL },
    [RROUTE_COAST]    = { TYPE_WATER, TYPE_FLYING, TYPE_WATER, TYPE_NORMAL },
    [RROUTE_RIVER]    = { TYPE_WATER, TYPE_GRASS, TYPE_BUG, TYPE_ELECTRIC },
    [RROUTE_HILLS]    = { TYPE_ROCK, TYPE_GROUND, TYPE_FIGHTING, TYPE_FLYING },
    [RROUTE_FIELD]    = { TYPE_NORMAL, TYPE_GRASS, TYPE_BUG, TYPE_FAIRY },
    [RROUTE_DESERT]   = { TYPE_GROUND, TYPE_ROCK, TYPE_FIRE, TYPE_BUG },
    [RROUTE_SEA]      = { TYPE_WATER, TYPE_FLYING, TYPE_WATER, TYPE_ICE },
    [RROUTE_WOODS]    = { TYPE_BUG, TYPE_GRASS, TYPE_GHOST, TYPE_POISON },
    [RROUTE_FLOWERS]  = { TYPE_GRASS, TYPE_BUG, TYPE_FAIRY, TYPE_NORMAL },
    [RROUTE_LONGGRASS]= { TYPE_GRASS, TYPE_BUG, TYPE_POISON, TYPE_NORMAL },
    [RROUTE_BRIDGE]   = { TYPE_WATER, TYPE_ELECTRIC, TYPE_FLYING, TYPE_NORMAL },
};

// Region state. Rebuilt from the seed whenever it does not match the save.
static EWRAM_DATA struct RogueRegion sRegion = {0};
static EWRAM_DATA bool8 sRegionReady = FALSE;
static EWRAM_DATA u8 sGymLeaders[ROGUE_NUM_GYMS] = {0};
static EWRAM_DATA u8 sE4[ROGUE_NUM_E4] = {0};
static EWRAM_DATA u16 sStarterBase[3] = {0};
static EWRAM_DATA u16 sStarterExtra[2] = {0};
static EWRAM_DATA u16 sWildSpecies[ROGUE_MAX_AREAS][WILD_SPECIES_PER_AREA] = {0};
static EWRAM_DATA u16 sFishSpecies[2][FISH_SPECIES] = {0}; // before and after the fifth gym
static EWRAM_DATA u16 sSurfSpecies[2][SURF_SPECIES] = {0};
static EWRAM_DATA u16 sPool[POOL_MAX] = {0};
static EWRAM_DATA u16 sPoolCount = 0;
// The POKeDEX mix the region's species are drawn with (ROGUE_MIX_*).
static EWRAM_DATA u8 sRegionMix = 0;
static EWRAM_DATA u8 sHasPreEvo[(NUM_SPECIES + 7) / 8] = {0};
// Name of each generated trainer, as an index into the male or female list.
static EWRAM_DATA u8 sTrainerNames[ROGUE_SLOT_LEADER_0] = {0};

// Engine structures for every place. Layout sizes and connections are always
// valid. The blocks and events live in a cache slot while the place is loaded
// or next door, and the other places point at empty ones.
// The extra header and layout, after the places', serve the dungeon floor the
// player is on (FLOOR_SLOT).
#define FLOOR_SLOT  ROGUE_MAX_PLACES       // the floor the player is on
#define FLOOR_AHEAD (ROGUE_MAX_PLACES + 1) // a floor generated ahead, usually the next one down
static EWRAM_DATA struct MapHeader sHeaders[ROGUE_MAX_PLACES + 2] = {0};
static EWRAM_DATA struct MapLayout sLayouts[ROGUE_MAX_PLACES + 2] = {0};
// What a place on the sea shows beyond its edge where its edge is land
// (Rogue_LayoutBorder).
static EWRAM_DATA const u16 *sLandBorder[ROGUE_MAX_PLACES] = {0};
static EWRAM_DATA struct MapConnections sConnections[ROGUE_MAX_PLACES] = {0};
static EWRAM_DATA struct MapConnection sConnectionList[ROGUE_MAX_PLACES][ROGUE_MAX_EXITS] = {0};

#define CACHE_SLOTS 6
struct MapSlot
{
    u8 place; // 0xFF: empty
    u8 age;
    struct RogueMap map;
    u16 blocks[ROGUE_MAX_BLOCKS];
    struct MapEvents events;
    struct ObjectEventTemplate objects[ROGUE_MAX_OBJS];
    struct WarpEvent warps[ROGUE_MAX_WARPS];
    struct CoordEvent coords[ROGUE_MAX_GATES];
    struct BgEvent bgs[ROGUE_MAX_SIGNS];
};
static EWRAM_DATA struct MapSlot sSlots[CACHE_SLOTS] = {0};
static EWRAM_DATA u8 sSlotOf[ROGUE_MAX_PLACES + 2] = {0}; // slot + 1, 0: not cached; then the two floors
// A place made up for a dungeon floor (RogueGen_FloorPlace), for the code
// that looks up a trainer's or an area's place.
static EWRAM_DATA struct RoguePlace sFloorPlace = {0};

static void EnsureRegion(void);
static u16 BaseForm(u16 species);
static bool32 FloorToGenerate(u32 *dungeon, u32 *floor);
static bool32 IsFloorMap(u32 mapNum);
static u16 MapObjectOfKind(u32 kind);
static s32 CurrentDungeon(void);
static bool32 InRegion(void);
static u8 ItemRules(void);
static EWRAM_DATA u8 sAge = 0;
static EWRAM_DATA u8 sCurrentPlace = 0, sPreviousPlace = 0; // set in BuildHeaders
static const struct MapEvents sEmptyEvents = {0};
static const u16 sEmptyBlocks[4] = { 0x3001, 0x3001, 0x3001, 0x3001 };

// Test hook: the automated playtest writes a level here to get a starter strong
// enough to finish a run by mashing A. It is always zero in normal play.
EWRAM_DATA u8 gRogueTestStarterLevel = 0;
// Test hook for tuning leaders (tools/roguemon/playtest.py --leader-states):
// [0] on, [1] ace level change, [2] IVs (0 keeps), [3] bits: 1 no held item,
// 2 no bag items, 4 the level change for the whole team, 8 the first gym as
// before v0.12 (no first-gym move rule, Water Bubble and the like allowed),
// 16 no badge TM for the first leader's ace.
EWRAM_DATA s8 gRogueTestLeader[4] = {0};
// Test hook: a warp the tests ask for, to skip long walks: map number + 1,
// x, y, and the outdoor place + 1 it counts as being entered from
// (VAR_ROGUE_TOWN, for dungeon floors; set when the floor loads, as the map
// left resets it on the way out). Always zero in normal play.
EWRAM_DATA u8 gRogueTestWarp[4] = {0};
// Test hook: a fixed seed for the next new game, so test runs can be repeated.
EWRAM_DATA u32 gRogueTestSeed = 0;
// Test hook: frames spent generating the last map, and how many maps had to
// be generated on the spot (not ahead), for the playtest's numbers.
EWRAM_DATA u16 gRogueGenFrames = 0;
EWRAM_DATA u16 gRogueSyncGens = 0;
// Test hook: set by the playtest to time every place once (frames per place
// into gRogueGenProfile) when the region is built.
EWRAM_DATA u8 gRogueGenProfile[ROGUE_MAX_PLACES] = {0};
EWRAM_DATA bool8 gRogueTestProfile = FALSE;
// Test hook: a trainer id the tests ask to have built (sTrainer and
// sTrainerParty), for the POKeDEX mix's numbers (dexmixtest.py). Always zero
// in normal play.
EWRAM_DATA u16 gRogueTestTrainer = 0;

static EWRAM_DATA struct Trainer sTrainer = {0};
static EWRAM_DATA struct TrainerMon sTrainerParty[PARTY_SIZE] = {0};
static EWRAM_DATA u16 sTrainerCachedId = 0;
static void Run0TestCopy(void); // src/rogue_run0_trainers.h
static void Run0TestCatch(void);
static void Run0TestParty(void);
static EWRAM_DATA struct WildPokemon sWildMons[12] = {0};
static EWRAM_DATA struct WildPokemonInfo sWildInfo = {0};

// ---------------------------------------------------------------------------
// Seed

u32 Rogue_GetSeed(void)
{
    return VarGet(VAR_ROGUE_SEED_LO) | (VarGet(VAR_ROGUE_SEED_HI) << 16);
}

static void SetSavedSeed(u32 seed)
{
    VarSet(VAR_ROGUE_SEED_LO, seed & 0xFFFF);
    VarSet(VAR_ROGUE_SEED_HI, seed >> 16);
}

// A region's name is its seed code (see RogueGen_RegionName).
bool8 RogueGen_KeptRegionName(u32 seed, char *out)
{
    u16 kept;
    if (!Rogue_Run0KeptRegion(&kept) || kept != seed)
        return FALSE;
    *Rogue_GameToAscii(out, gSaveBlock2Ptr->playerName, PLAYER_NAME_LENGTH) = 0;
    return TRUE;
}

u8 *Rogue_RegionName(u8 *dest, u32 seed)
{
    char name[ROGUE_REGION_NAME_LEN + 1];
    u16 kept;
    // RUN 0's CHOOSE: the region kept carries the player's name.
    if (Rogue_Run0KeptRegion(&kept) && kept == seed)
        return StringCopyN(dest, gSaveBlock2Ptr->playerName, PLAYER_NAME_LENGTH);
    RogueGen_RegionName(seed, name);
    return Rogue_CopyAscii(dest, (const u8 *)name);
}

// A run's code: the region's name, a dash and the letter of its
// POKeDEX mix, KALONA-N, shown at the end of the run. Typed on the
// professor's map (TYPE NAME) it gives a friend the same region with the
// same POKeMON. Always the region's own name, never RUN 0's kept one (the
// player's name). RUN 0 and runs from before v0.14 (LEGACY) have none.
static const char sMixLetters[ROGUE_MIX_COUNT] = { [ROGUE_MIX_CLASSIC] = 'C', [ROGUE_MIX_NATIONAL] = 'N', [ROGUE_MIX_GLOBAL] = 'G' };

bool32 Rogue_RunCode(u8 *dest)
{
    char name[ROGUE_REGION_NAME_LEN + 1];
    u32 seed = Rogue_GetSeed(), mix = VarGet(VAR_ROGUE_DEX_MIX);
    if (seed >= ROGUE_REGION_COUNT || mix >= ROGUE_MIX_COUNT || sMixLetters[mix] == 0)
        return FALSE;
    RogueGen_RegionName(seed, name);
    dest = Rogue_CopyAscii(dest, (const u8 *)name);
    *dest++ = CHAR_HYPHEN;
    *dest++ = CHAR_A + (sMixLetters[mix] - 'A');
    *dest = EOS;
    return TRUE;
}

// The seed for a typed name. A region's name leads to that region, a run
// code to its region and sets *mix to its POKeDEX mix (ROGUE_MIX_COUNT for
// a plain name: the player's own choice). Any other word leads somewhere
// too, always the same place.
static u32 CodeToSeed(const u8 *code, u32 *mix)
{
    char ascii[ROGUE_RUN_CODE_LEN + 2];
    u32 i, m, hash = 0;
    s32 seed;
    *mix = ROGUE_MIX_COUNT;
    for (i = 0; i < ROGUE_RUN_CODE_LEN + 1 && code[i] != EOS; i++)
    {
        if (code[i] >= CHAR_A && code[i] <= CHAR_Z)
            ascii[i] = 'A' + (code[i] - CHAR_A);
        else if (code[i] >= CHAR_a && code[i] <= CHAR_z)
            ascii[i] = 'A' + (code[i] - CHAR_a);
        else
            ascii[i] = '?';
        hash = RogueRng_Hash(hash, ascii[i]);
    }
    ascii[i] = 0;
    seed = RogueGen_ParseRegionName(ascii);
    if (seed >= 0)
        return seed;
    // A run code: the name, the mix's letter, and between them a dash, a
    // dot, a space or nothing.
    for (m = 0; i >= 2 && m < ROGUE_MIX_COUNT; m++)
    {
        if (sMixLetters[m] == 0 || ascii[i - 1] != sMixLetters[m])
            continue;
        ascii[i - 1] = 0;
        if (ascii[i - 2] == '?')
            ascii[i - 2] = 0;
        seed = RogueGen_ParseRegionName(ascii);
        if (seed >= 0)
        {
            *mix = m;
            return seed;
        }
        break;
    }
    return hash % ROGUE_REGION_COUNT;
}

// ---------------------------------------------------------------------------
// Species pools

static u32 SpeciesBst(u16 species)
{
    const struct SpeciesInfo *info = &gSpeciesInfo[species];
    return info->baseHP + info->baseAttack + info->baseDefense + info->baseSpeed + info->baseSpAttack + info->baseSpDefense;
}

static bool32 SpeciesHasType(u16 species, enum Type type)
{
    return GetSpeciesType(species, 0) == type || GetSpeciesType(species, 1) == type;
}

// The pool holds every ordinary first-stage species: no alternate forms, no
// legendaries, and nothing that evolves from another species.
static void BuildPool(void)
{
    u32 s, i;

    memset(sHasPreEvo, 0, sizeof(sHasPreEvo));
    for (s = 1; s < NUM_SPECIES; s++)
    {
        const struct Evolution *evos;
        // The engine asserts on lookups for species that are compiled out.
        if (!IsSpeciesEnabled(s))
            continue;
        evos = GetSpeciesEvolutions(s);
        if (evos == NULL)
            continue;
        for (i = 0; evos[i].method != EVOLUTIONS_END; i++)
        {
            u32 target = evos[i].targetSpecies;
            if (target < NUM_SPECIES)
                sHasPreEvo[target / 8] |= 1 << (target % 8);
        }
    }

    sPoolCount = 0;
    for (s = 1; s < NUM_SPECIES && sPoolCount < POOL_MAX; s++)
    {
        const struct SpeciesInfo *info = &gSpeciesInfo[s];
        if (!IsSpeciesEnabled(s) || GET_BASE_SPECIES_ID(s) != s)
            continue;
        if (info->isRestrictedLegendary || info->isSubLegendary || info->isMythical || info->isUltraBeast
         || info->isParadox || info->isTotem || info->isMegaEvolution || info->isPrimalReversion)
            continue;
        if (sHasPreEvo[s / 8] & (1 << (s % 8)))
            continue;
        if (SpeciesBst(s) == 0 || SpeciesBst(s) > 500)
            continue;
        sPool[sPoolCount++] = s;
    }
    // In national dex order, for the generations (PoolGenStart). Species
    // numbers mostly are already. (A run from before v0.14 keeps its draw.)
    for (s = 1; s < sPoolCount && sRegionMix != ROGUE_MIX_LEGACY; s++)
    {
        u16 species = sPool[s];
        u32 dex = SpeciesToNationalPokedexNum(species);
        for (i = s; i > 0 && SpeciesToNationalPokedexNum(sPool[i - 1]) > dex; i--)
            sPool[i] = sPool[i - 1];
        sPool[i] = species;
    }
}

// Pick a pool species within a base stat range. type may be TYPE_NONE.
// A run from before v0.14 (ROGUE_MIX_LEGACY) draws as it always did, so its
// region stays the same.
static u16 PickSpeciesLegacy(u32 *rng, enum Type type, u32 minBst, u32 maxBst)
{
    u32 tries;
    for (tries = 0; tries < 400; tries++)
    {
        u16 species = sPool[RogueRng_Range(rng, sPoolCount)];
        u32 bst = SpeciesBst(species);
        // Relax the stat limits if nothing fits after many tries.
        if (tries < 300 && (bst < minBst || bst > maxBst))
            continue;
        if (type != TYPE_NONE && !SpeciesHasType(species, type))
            continue;
        return species;
    }
    return sPool[RogueRng_Range(rng, sPoolCount)];
}

// ---------------------------------------------------------------------------
// The POKeDEX mix: a generation first, by the mix's weights, then a species
// of that generation as before. The pool is in national dex order.

#define NUM_GENS 9
static const u16 sGenFirstDex[NUM_GENS + 1] = { 1, 152, 252, 387, 494, 650, 722, 810, 906, NATIONAL_DEX_COUNT + 1 };
// In thousandths. CLASSIC is 65% Gens 1 to 3, 15% Gen 4 and 20% Gens 5 to
// 9, each group split evenly as the design table has it (22, 22, 22, 15, 5, 5,
// 3, 3, 3), not by pool size: Gen 2 has the fewest first forms and would
// be the rarest again.
static const u16 sGenWeights[ROGUE_MIX_COUNT][NUM_GENS] =
{
    [ROGUE_MIX_CLASSIC]  = { 217, 217, 216, 150, 50, 50, 34, 33, 33 },
    [ROGUE_MIX_NATIONAL] = { 150, 150, 150, 150, 125, 125, 50, 50, 50 },
    [ROGUE_MIX_GLOBAL]   = { 111, 111, 111, 111, 111, 111, 111, 111, 112 },
};

// What a slot asks of its species, beyond the type and the stat range: a
// wild area's rules (WildFits), or a trainer's (the evolved form's stats at
// its level, and the lines rare until late).
struct PickRules
{
    enum Type type;
    u16 minBst, maxBst;
    const struct RoguePlace *wild;
    u16 evoLo, evoHi; // a trainer's band; evoHi 0: none
    u8 level;
    bool8 anyMethod;
};

static bool32 WildFits(u16 species, const struct RoguePlace *p);
static bool32 LineFitsLevel(u16 species, u32 level);
static u16 EvolveForLevel(u16 species, u32 level, bool32 anyMethod);

// How strictly: 0 every rule; 1 the stat range relaxed (as the draw always
// relaxed it); 2 the slot's own rules relaxed too; 3 the type as well.
static bool32 PickFits(u16 species, const struct PickRules *r, u32 relax)
{
    u32 bst;
    if (relax >= 3)
        return TRUE;
    if (r->type != TYPE_NONE && !SpeciesHasType(species, r->type))
        return FALSE;
    bst = SpeciesBst(species);
    if (relax < 1 && (bst < r->minBst || bst > r->maxBst))
        return FALSE;
    if (relax >= 2)
        return TRUE;
    if (r->wild != NULL && !WildFits(species, r->wild))
        return FALSE;
    if (r->evoHi != 0)
    {
        u32 evo = SpeciesBst(EvolveForLevel(species, r->level, r->anyMethod));
        if (evo < r->evoLo || evo > r->evoHi || !LineFitsLevel(species, r->level))
            return FALSE;
    }
    return TRUE;
}

// The first pool index of a generation (0 to NUM_GENS).
static u32 PoolGenStart(u32 gen)
{
    u32 lo = 0, hi = sPoolCount, dex = sGenFirstDex[gen];
    while (lo < hi)
    {
        u32 mid = (lo + hi) / 2;
        if (SpeciesToNationalPokedexNum(sPool[mid]) < dex)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

// A species of one generation that fits, or SPECIES_NONE if none does. A few
// draws find one most of the time; else a count of the generation decides.
static u16 PickInGen(u32 *rng, u32 gen, const struct PickRules *r, u32 relax)
{
    u32 start = PoolGenStart(gen), n = PoolGenStart(gen + 1) - start, i, count = 0;
    if (n == 0)
        return SPECIES_NONE;
    for (i = 0; i < 12; i++)
    {
        u16 species = sPool[start + RogueRng_Range(rng, n)];
        if (PickFits(species, r, relax))
            return species;
    }
    for (i = 0; i < n; i++)
        count += PickFits(sPool[start + i], r, relax);
    if (count == 0)
        return SPECIES_NONE;
    count = RogueRng_Range(rng, count);
    for (i = 0; i < n; i++)
        if (PickFits(sPool[start + i], r, relax) && count-- == 0)
            return sPool[start + i];
    return SPECIES_NONE;
}

static u16 PickByRules(u32 *rng, const struct PickRules *r)
{
    const u16 *weights = sGenWeights[sRegionMix < ROGUE_MIX_COUNT ? sRegionMix : ROGUE_MIX_CLASSIC];
    u32 roll, gen, total = 0, relax, d;
    for (gen = 0; gen < NUM_GENS; gen++)
        total += weights[gen];
    roll = RogueRng_Range(rng, total);
    for (gen = 0; gen + 1 < NUM_GENS && roll >= weights[gen]; gen++)
        roll -= weights[gen];
    // A generation with nothing that fits gives way to the nearest one that
    // has (the older first), before any rule is relaxed.
    for (relax = 0; relax < 4; relax++)
    {
        for (d = 0; d < NUM_GENS; d++)
        {
            u16 species;
            if (gen >= d && (species = PickInGen(rng, gen - d, r, relax)) != SPECIES_NONE)
                return species;
            if (d != 0 && gen + d < NUM_GENS && (species = PickInGen(rng, gen + d, r, relax)) != SPECIES_NONE)
                return species;
        }
    }
    return sPool[RogueRng_Range(rng, sPoolCount)];
}

// A wild, trainer or leader species. place: a wild area's rules apply too.
static u16 PickSpeciesFor(u32 *rng, enum Type type, u32 minBst, u32 maxBst, const struct RoguePlace *place)
{
    struct PickRules r = {0};
    if (sRegionMix == ROGUE_MIX_LEGACY)
        return PickSpeciesLegacy(rng, type, minBst, maxBst);
    r.type = type;
    r.minBst = minBst;
    r.maxBst = maxBst;
    r.wild = place;
    return PickByRules(rng, &r);
}

static u16 PickSpecies(u32 *rng, enum Type type, u32 minBst, u32 maxBst)
{
    return PickSpeciesFor(rng, type, minBst, maxBst, NULL);
}

// The highest base stat total in a species' line.
static u32 LineMaxBst(u16 species)
{
    const struct Evolution *evos = GetSpeciesEvolutions(species);
    u32 best = SpeciesBst(species), i, depth;
    for (depth = 0; depth < 2 && evos != NULL; depth++)
    {
        u16 next = SPECIES_NONE;
        for (i = 0; evos[i].method != EVOLUTIONS_END; i++)
        {
            u16 t = evos[i].targetSpecies;
            if (t < NUM_SPECIES && IsSpeciesEnabled(t))
            {
                if (SpeciesBst(t) > best)
                    best = SpeciesBst(t);
                next = t;
            }
        }
        if (next == SPECIES_NONE)
            break;
        evos = GetSpeciesEvolutions(next);
    }
    return best;
}

static bool32 IsStarterLine(u16 species)
{
    u32 i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < ARRAY_COUNT(sStarters[0]); j++)
            if (sStarters[i][j] == species)
                return TRUE;
    return FALSE;
}

// The real games keep the starters to the professor, and lines like Dratini,
// Bagon and Beldum for late or hidden places: here, from level 35 on.
static bool32 LineFitsLevel(u16 species, u32 level)
{
    if (IsStarterLine(species))
        return FALSE;
    if (LineMaxBst(species) >= 580 && level < 35)
        return FALSE;
    return TRUE;
}

static bool32 SpeciesHasAbility(u16 species, enum Ability ability)
{
    // Wild and trainer Pokemon here use the first two slots, not the hidden one.
    return gSpeciesInfo[species].abilities[0] == ability || gSpeciesInfo[species].abilities[1] == ability;
}

// Wild Pokemon that suit a place. Nothing that traps the player's team
// (Wynaut is on Mirage Island in Emerald, not a route), and none of the
// abilities that swing a battle on their own until the third badge.
// A baby (Azurill, Igglybuff, Happiny...): hatched from an egg, never met
// in the wild of a real region, and it only grows up by friendship. As a
// first catch it is a weak partner for a long time.
static bool32 IsBaby(u16 species)
{
    return gSpeciesInfo[species].eggGroups[0] == EGG_GROUP_NO_EGGS_DISCOVERED
        && GetSpeciesEvolutions(species) != NULL;
}

static bool32 WildFits(u16 species, const struct RoguePlace *p)
{
    if (!LineFitsLevel(species, p->level) || IsBaby(species))
        return FALSE;
    // Unown lives only in ruins (the Ruins of Alph, Tanoby), never in the
    // grass of a route or a cave.
    if (species == SPECIES_UNOWN)
        return FALSE;
    if (SpeciesHasAbility(species, ABILITY_SHADOW_TAG))
        return FALSE;
    if (p->badges < 3 && (SpeciesHasAbility(species, ABILITY_ARENA_TRAP) || SpeciesHasAbility(species, ABILITY_ADAPTABILITY)
                       || SpeciesHasAbility(species, ABILITY_MOODY) || SpeciesHasAbility(species, ABILITY_WONDER_GUARD)))
        return FALSE;
    return TRUE;
}

static bool32 HitsHard(enum Type atk, enum Type def)
{
    return GetTypeModifier(atk, def) >= UQ_4_12(2.0);
}

static bool32 CountersType(u16 species, enum Type type)
{
    return HitsHard(GetSpeciesType(species, 0), type) || HitsHard(GetSpeciesType(species, 1), type);
}

// Trainers use the form a species would have reached by this level. With
// anyMethod, stone, trade and friendship evolutions count too, which gives
// late trainers fully evolved teams.
static bool32 EvoReached(const struct Evolution *evo, u32 level, bool32 anyMethod, u32 stage)
{
    bool32 byLevel = (evo->method == EVO_LEVEL && evo->param != 0 && evo->param <= level);
    // Friendship and the other conditions (Riolu, Golbat) are a level
    // evolution with no level: they count as other methods.
    bool32 byOther = (anyMethod && (evo->method != EVO_LEVEL || evo->param == 0) && level >= 26 + stage * 6);
    return (byLevel || byOther) && evo->targetSpecies < NUM_SPECIES && IsSpeciesEnabled(evo->targetSpecies);
}

static u16 EvolveForLevel(u16 species, u32 level, bool32 anyMethod)
{
    u32 stage, i;
    for (stage = 0; stage < 2; stage++)
    {
        const struct Evolution *evos = GetSpeciesEvolutions(species);
        bool32 evolved = FALSE;
        if (evos == NULL)
            break;
        for (i = 0; evos[i].method != EVOLUTIONS_END; i++)
        {
            if (EvoReached(&evos[i], level, anyMethod, stage))
            {
                species = evos[i].targetSpecies;
                evolved = TRUE;
                break;
            }
        }
        if (!evolved)
            break;
    }
    return species;
}

static bool32 InList(const u16 *list, u32 count, u16 value)
{
    u32 i;
    for (i = 0; i < count; i++)
        if (list[i] == value)
            return TRUE;
    return FALSE;
}

static u16 PickEnabled(u32 *rng, const u16 *list, u32 count)
{
    u16 species;
    do
    {
        species = list[RogueRng_Range(rng, count)];
    } while (!IsSpeciesEnabled(species));
    return species;
}

static const struct ClassDef *TrainerClass(u32 slot);
static bool32 IsFeatureSlot(u32 slot);
static s32 MasterDungeon(u32 slot);
static const struct ClassDef *VillainClass(u32 slot);
static u16 TeamGruntGfx(u32 team);
static u32 BuildVillainTrainer(u32 slot, u32 *rng);
static const u8 *VillainLine(u32 slot, u32 kind, u8 *dest, u32 size);
static void BuildVillainObject(struct ObjectEventTemplate *t, u32 kind, u32 slot, u32 place);
static void UpdateVillainObjects(struct MapSlot *s);
static void UpdateCastObjects(struct MapSlot *s);
static u8 WandererMet(void);

// Place of each wild area, found once per region.
static u8 AreaPlace(u32 area)
{
    u32 i;
    for (i = 0; i < sRegion.nPlaces; i++)
        if (sRegion.places[i].area == area)
            return i;
    return 0;
}

// The place of a wild area. A dungeon's own area (one not shared with its
// cave) stands as its first floor.
static const struct RoguePlace *AreaPlaceOf(u32 area)
{
    u32 i;
    for (i = 0; i < sRegion.nPlaces; i++)
        if (sRegion.places[i].area == area)
            return &sRegion.places[i];
    for (i = 0; i < sRegion.nDungeons; i++)
    {
        if (sRegion.dungeons[i].area == area)
        {
            RogueGen_FloorPlace(&sRegion, i, 1, &sFloorPlace);
            return &sFloorPlace;
        }
    }
    return &sRegion.places[0];
}

static void EnsureGymCounter(u32 *rng, u32 gym)
{
    enum Type leaderType = sLeaders[sGymLeaders[gym]].type, counters[NUMBER_OF_MON_TYPES];
    u8 areas[ROGUE_MAX_AREAS];
    u32 nAreas = 0, nCounters = 0, i, j, tries;
    // Before the first two gyms the answer is common, as Emerald's Lotad
    // and Shroomish are on the way to Roxanne: one of the two species that
    // take a quarter of the encounters each (slots 0 and 1). Later one in a
    // middle slot is enough.
    u32 common = gym < 2 ? 2 : WILD_SPECIES_PER_AREA;
    for (i = 0; i < sRegion.nPlaces; i++)
    {
        const struct RoguePlace *p = &sRegion.places[i];
        if (p->kind == RPLACE_TOWN || p->area == 0xFF || p->badges != gym)
            continue;
        for (j = 0; j < common; j++)
            if (CountersType(sWildSpecies[p->area][j], leaderType))
                return;
        areas[nAreas++] = p->area;
    }
    if (nAreas == 0)
        return;
    for (i = TYPE_NORMAL; i < NUMBER_OF_MON_TYPES; i++)
        if (i != TYPE_MYSTERY && i != TYPE_STELLAR && HitsHard(i, leaderType))
            counters[nCounters++] = i;
    if (nCounters == 0)
        return;
    for (tries = 0; tries < 20; tries++)
    {
        u32 area = areas[RogueRng_Range(rng, nAreas)];
        const struct RoguePlace *p = AreaPlaceOf(area);
        u32 tier = p->tier < NUM_TIERS ? p->tier : NUM_TIERS - 1;
        u16 species = PickSpeciesFor(rng, counters[RogueRng_Range(rng, nCounters)], sWildMinBst[tier], sWildMaxBst[tier], p);
        if (!CountersType(species, leaderType) || InList(sWildSpecies[area], WILD_SPECIES_PER_AREA, species)
         || (tries < 15 && !WildFits(species, p)))
            continue;
        // One of the middle slots: met now and then, not every other step.
        sWildSpecies[area][gym < 2 ? RogueRng_Range(rng, 2) : 2 + RogueRng_Range(rng, 4)] = species;
        return;
    }
}

static void GenerateContent(u32 seed)
{
    u32 rng = RogueRng_Hash(seed, 7);
    u32 i, j;

    BuildPool();

    // Twelve leaders of twelve different types: eight gyms and the Elite
    // Four (src/rogue_gen_people.h, so the host tools know them too). The
    // draw has its own stream, and the content stream steps as it did for
    // the pool of 20 before the Kanto leaders came, so the starters and wild
    // Pokemon of a region stay as they were.
    for (i = 0; i < 19; i++)
        RogueRng_Next(&rng);
    RogueGen_RegionLeaders(&sRegion, sGymLeaders, sE4);

    for (i = 0; i < 3; i++)
        sStarterBase[i] = PickEnabled(&rng, sStarters[i], ARRAY_COUNT(sStarters[0]));
    sStarterExtra[0] = PickEnabled(&rng, sPartnerStarters, ARRAY_COUNT(sPartnerStarters));
    sStarterExtra[1] = PickEnabled(&rng, sDragonStarters, ARRAY_COUNT(sDragonStarters));

    for (i = 0; i < sRegion.nAreas; i++)
    {
        const struct RoguePlace *p = AreaPlaceOf(i);
        u32 tier = p->tier < NUM_TIERS ? p->tier : NUM_TIERS - 1;
        if (p->kind == RPLACE_TOWN)
            continue; // towns only have fishing
        for (j = 0; j < WILD_SPECIES_PER_AREA; j++)
        {
            u32 tries = 0;
            do
            {
                enum Type type = TYPE_NONE;
                if (p->kind == RPLACE_CAVE)
                    type = sCaveTypes[RogueRng_Range(&rng, ARRAY_COUNT(sCaveTypes))];
                else if (p->type < RROUTE_COUNT && RogueRng_Range(&rng, 2))
                {
                    type = sRouteTypes[p->type][RogueRng_Range(&rng, 4)];
                    // Ash fields are volcanic.
                    if (p->type == RROUTE_DESERT && p->theme == RTHEME_ASH && type == TYPE_ROCK)
                        type = TYPE_FIRE;
                }
                sWildSpecies[i][j] = PickSpeciesFor(&rng, type, sWildMinBst[tier], sWildMaxBst[tier], p);
            } while ((InList(sWildSpecies[i], j, sWildSpecies[i][j]) || (tries < 40 && !WildFits(sWildSpecies[i][j], p)))
                     && ++tries < 50);
        }
    }
    // Before each gym, at least one wild Pokemon whose type hits the
    // leader's hard, the way Emerald puts Lotad, Seedot and Shroomish on the
    // routes before Roxanne.
    for (i = 0; i < ROGUE_NUM_GYMS; i++)
        EnsureGymCounter(&rng, i);
    for (i = 0; i < 2; i++)
    {
        for (j = 0; j < FISH_SPECIES; j++)
        {
            u32 tries = 0;
            do
            {
                sFishSpecies[i][j] = PickSpecies(&rng, TYPE_WATER, 0, i ? 500 : 420);
            } while (InList(sFishSpecies[i], j, sFishSpecies[i][j]) && ++tries < 50);
        }
        for (j = 0; j < SURF_SPECIES; j++)
        {
            u32 tries = 0;
            do
            {
                sSurfSpecies[i][j] = PickSpecies(&rng, TYPE_WATER, i ? 300 : 0, i ? 500 : 430);
            } while ((InList(sSurfSpecies[i], j, sSurfSpecies[i][j]) || InList(sFishSpecies[i], FISH_SPECIES, sSurfSpecies[i][j])) && ++tries < 50);
        }
    }

    // A different name for every generated trainer in the run: route,
    // cave and dungeon trainers, gym trainers, the battling family and the
    // Seashore House's, and the dojo's (the villains are GRUNTs, the pairs
    // have their own). The lists are long enough for any region; were one
    // used up, its names would start again.
    {
        u32 usedMale[NAME_WORDS] = {0}, usedFemale[NAME_WORDS] = {0};
        // (Slots with no name of their own keep none from the last region.)
        memset(sTrainerNames, 0, sizeof(sTrainerNames));
        for (i = 0; i < ROGUE_SLOT_LEADER_0 + ROGUE_DOJO_TRAINERS; i++)
        {
            u32 slot = i < ROGUE_SLOT_LEADER_0 ? i : ROGUE_SLOT_DOJO_0 + i - ROGUE_SLOT_LEADER_0;
            u32 *used, n, k, tries, store = slot;
            bool32 female;
            if (slot >= ROGUE_SLOT_DOJO_0)
            {
                if (sRegion.nTrainerSlots > DOJO_NAME_SLOT)
                    continue;
                store = DOJO_NAME_SLOT + slot - ROGUE_SLOT_DOJO_0;
            }
            else if (slot < sRegion.nTrainerSlots)
            {
                if (RogueGen_TrainerClass(&sRegion, slot) == ROGUE_CLASS_VILLAIN)
                    continue;
            }
            else if (slot < ROGUE_SLOT_FAMILY_0 || (slot < ROGUE_SLOT_GYM_0 && !IsFeatureSlot(slot)))
            {
                continue;
            }
            female = (TrainerClass(slot)->gender == FEMALE);
            used = female ? usedFemale : usedMale;
            n = female ? ARRAY_COUNT(sFemaleNames) : ARRAY_COUNT(sMaleNames);
            k = RogueRng_Range(&rng, n);
            for (tries = 0; tries < n && ((used[k / 32] >> (k % 32)) & 1); tries++)
                k = (k + 1) % n;
            if (tries == n)
                memset(used, 0, NAME_WORDS * sizeof(u32));
            used[k / 32] |= 1u << (k % 32);
            sTrainerNames[store] = k;
        }
    }
}

// ---------------------------------------------------------------------------
// Engine structures

static const u8 sFacingMovement[] =
{
    [RDIR_NORTH] = MOVEMENT_TYPE_FACE_UP,
    [RDIR_EAST]  = MOVEMENT_TYPE_FACE_RIGHT,
    [RDIR_SOUTH] = MOVEMENT_TYPE_FACE_DOWN,
    [RDIR_WEST]  = MOVEMENT_TYPE_FACE_LEFT,
};

// Trainers that turn between two ways, by their two facings.
static const u8 sTurnMovement[4][4] =
{
    [RDIR_NORTH] = { [RDIR_NORTH] = MOVEMENT_TYPE_FACE_UP, [RDIR_EAST] = MOVEMENT_TYPE_FACE_UP_AND_RIGHT,
                     [RDIR_SOUTH] = MOVEMENT_TYPE_FACE_DOWN_AND_UP, [RDIR_WEST] = MOVEMENT_TYPE_FACE_UP_AND_LEFT },
    [RDIR_EAST]  = { [RDIR_NORTH] = MOVEMENT_TYPE_FACE_UP_AND_RIGHT, [RDIR_EAST] = MOVEMENT_TYPE_FACE_RIGHT,
                     [RDIR_SOUTH] = MOVEMENT_TYPE_FACE_DOWN_AND_RIGHT, [RDIR_WEST] = MOVEMENT_TYPE_FACE_LEFT_AND_RIGHT },
    [RDIR_SOUTH] = { [RDIR_NORTH] = MOVEMENT_TYPE_FACE_DOWN_AND_UP, [RDIR_EAST] = MOVEMENT_TYPE_FACE_DOWN_AND_RIGHT,
                     [RDIR_SOUTH] = MOVEMENT_TYPE_FACE_DOWN, [RDIR_WEST] = MOVEMENT_TYPE_FACE_DOWN_AND_LEFT },
    [RDIR_WEST]  = { [RDIR_NORTH] = MOVEMENT_TYPE_FACE_UP_AND_LEFT, [RDIR_EAST] = MOVEMENT_TYPE_FACE_LEFT_AND_RIGHT,
                     [RDIR_SOUTH] = MOVEMENT_TYPE_FACE_DOWN_AND_LEFT, [RDIR_WEST] = MOVEMENT_TYPE_FACE_LEFT },
};

static const u8 sConnectionDirs[] =
{
    [RDIR_NORTH] = CONNECTION_NORTH,
    [RDIR_EAST]  = CONNECTION_EAST,
    [RDIR_SOUTH] = CONNECTION_SOUTH,
    [RDIR_WEST]  = CONNECTION_WEST,
};

static const u8 *const sGateScripts[] =
{
    [RDIR_NORTH] = Rogue_EventScript_GateNorth,
    [RDIR_EAST]  = Rogue_EventScript_GateEast,
    [RDIR_SOUTH] = Rogue_EventScript_GateSouth,
    [RDIR_WEST]  = Rogue_EventScript_GateWest,
};

static const u8 *const sBadgeGateScripts[] =
{
    [RDIR_NORTH] = Rogue_EventScript_BadgeGateNorth,
    [RDIR_EAST]  = Rogue_EventScript_BadgeGateEast,
    [RDIR_SOUTH] = Rogue_EventScript_BadgeGateSouth,
    [RDIR_WEST]  = Rogue_EventScript_BadgeGateWest,
};

bool32 Rogue_AllowRivalDouble(void)
{
    return RogueRng_Hash(Rogue_GetSeed(), 31000) % 1000 == 0;
}

static u32 TrainerSlotRng(u32 slot)
{
    return RogueRng_Hash(sRegion.seed, 5000 + slot);
}

// The dungeon a trainer slot belongs to, or -1.
static s32 SlotDungeon(u32 slot)
{
    u32 i;
    for (i = 0; i < sRegion.nDungeons; i++)
    {
        const struct RogueDungeon *d = &sRegion.dungeons[i];
        if (d->nTrainers && slot >= d->trainerBase && slot < d->trainerBase + d->nTrainers)
            return i;
    }
    return -1;
}

// The place a route, cave or dungeon trainer slot belongs to. A dungeon
// trainer's is the floor they stand on.
static const struct RoguePlace *SlotPlace(u32 slot)
{
    u32 i;
    s32 d;
    for (i = 0; i < sRegion.nPlaces; i++)
    {
        const struct RoguePlace *p = &sRegion.places[i];
        if (p->nTrainers && slot >= p->trainerBase && slot < p->trainerBase + p->nTrainers)
            return p;
    }
    d = SlotDungeon(slot);
    if (d >= 0)
    {
        RogueGen_FloorPlace(&sRegion, d, RogueGen_FloorOfTrainer(&sRegion, d, slot), &sFloorPlace);
        return &sFloorPlace;
    }
    return &sRegion.places[0];
}

// ---------------------------------------------------------------------------
// Route features' trainers (src/rogue_gen_features.h): the double battle
// pairs (TWINS, couples...), the battling family and the Seashore House's
// three, in slots after every route, cave and dungeon trainer.

static const struct ClassDef sPairClasses[RPAIR_COUNT] =
{
    [RPAIR_TWINS]        = { TRAINER_CLASS_TWINS, TRAINER_PIC_TWINS, OBJ_EVENT_GFX_TWIN, FEMALE, TRAINER_ENCOUNTER_MUSIC_TWINS, TYPE_NONE, 0, TALK_KID },
    [RPAIR_YOUNG_COUPLE] = { TRAINER_CLASS_YOUNG_COUPLE, TRAINER_PIC_YOUNG_COUPLE, OBJ_EVENT_GFX_MAN_3, MALE, TRAINER_ENCOUNTER_MUSIC_GIRL, TYPE_NONE, 0, TALK_FANCY },
    [RPAIR_OLD_COUPLE]   = { TRAINER_CLASS_OLD_COUPLE, TRAINER_PIC_OLD_COUPLE, OBJ_EVENT_GFX_OLD_MAN, MALE, TRAINER_ENCOUNTER_MUSIC_INTENSE, TYPE_NONE, 0, TALK_FANCY },
    [RPAIR_SIS_AND_BRO]  = { TRAINER_CLASS_SIS_AND_BRO, TRAINER_PIC_SIS_AND_BRO, OBJ_EVENT_GFX_TUBER_F, FEMALE, TRAINER_ENCOUNTER_MUSIC_SWIMMER, TYPE_NONE, 0, TALK_KID },
    [RPAIR_SR_AND_JR]    = { TRAINER_CLASS_SR_AND_JR, TRAINER_PIC_SR_AND_JR, OBJ_EVENT_GFX_WOMAN_4, FEMALE, TRAINER_ENCOUNTER_MUSIC_TWINS, TYPE_NONE, 0, TALK_FANCY },
};
// The second of each pair, as Emerald dresses them.
static const u16 sPairSecondGfx[RPAIR_COUNT] =
{
    OBJ_EVENT_GFX_TWIN, OBJ_EVENT_GFX_WOMAN_5, OBJ_EVENT_GFX_OLD_WOMAN, OBJ_EVENT_GFX_TUBER_M, OBJ_EVENT_GFX_LITTLE_GIRL,
};
// Their names, two to a pair, as Emerald's AMY & LIV.
static const u8 sPairNames[RPAIR_COUNT][4][TRAINER_NAME_LENGTH + 1] =
{
    [RPAIR_TWINS]        = { _("ZOE & ZIA"), _("PIA & PEG"), _("KAY & KIM"), _("LEA & LOU") },
    [RPAIR_YOUNG_COUPLE] = { _("JAY & ELL"), _("ROB & ADA"), _("TED & FAY"), _("SAM & IVY") },
    [RPAIR_OLD_COUPLE]   = { _("HAL & MAE"), _("OTIS & BEA"), _("GUS & ROSE"), _("ABE & NAN") },
    [RPAIR_SIS_AND_BRO]  = { _("MIA & MAX"), _("UMA & ARI"), _("ELLA & BO"), _("TIA & TOM") },
    [RPAIR_SR_AND_JR]    = { _("JO & JEN"), _("ANN & AMY"), _("MEG & MOLL"), _("DEB & DOT") },
};
// The battling family: a parent at the door, the other parent, their child
// and the grandparent who closes.
static const struct ClassDef sFamilyClasses[ROGUE_FAMILY_TRAINERS] =
{
    { TRAINER_CLASS_POKEFAN, TRAINER_PIC_POKEFAN_M, OBJ_EVENT_GFX_POKEFAN_M, MALE, TRAINER_ENCOUNTER_MUSIC_TWINS, TYPE_NONE, 0, TALK_FANCY },
    { TRAINER_CLASS_POKEFAN, TRAINER_PIC_POKEFAN_F, OBJ_EVENT_GFX_POKEFAN_F, FEMALE, TRAINER_ENCOUNTER_MUSIC_TWINS, TYPE_NONE, 0, TALK_FANCY },
    { TRAINER_CLASS_LASS, TRAINER_PIC_LASS, OBJ_EVENT_GFX_LASS, FEMALE, TRAINER_ENCOUNTER_MUSIC_FEMALE, TYPE_NONE, 0, TALK_KID },
    { TRAINER_CLASS_EXPERT, TRAINER_PIC_EXPERT_F, OBJ_EVENT_GFX_EXPERT_F, FEMALE, TRAINER_ENCOUNTER_MUSIC_INTENSE, TYPE_NONE, 0, TALK_ACE },
};
// The Seashore House's: a Tuber, a Beauty and a Sailor (sClasses).
static const u8 sSeashoreClasses[ROGUE_SEASHORE_TRAINERS] = { 7, 16, 18 };

static bool32 IsFeatureSlot(u32 slot)
{
    return slot >= ROGUE_SLOT_FEATURE_0 && slot < ROGUE_SLOT_FEATURE_0 + ROGUE_FEATURE_TRAINERS
        && RogueGen_FeatureSlotPlace(&sRegion, slot) != 0xFF;
}

static const struct ClassDef *FeatureClass(u32 slot)
{
    if (slot < ROGUE_SLOT_FAMILY_0)
        return &sPairClasses[RogueGen_PairKind(&sRegion, slot - ROGUE_SLOT_PAIR_0) % RPAIR_COUNT];
    if (slot < ROGUE_SLOT_SEASHORE_0)
        return &sFamilyClasses[slot - ROGUE_SLOT_FAMILY_0];
    return &sClasses[sSeashoreClasses[(slot - ROGUE_SLOT_SEASHORE_0) % ROGUE_SEASHORE_TRAINERS]];
}

u16 Rogue_FeatureTrainerGfx(u32 slot)
{
    EnsureRegion();
    return FeatureClass(slot)->gfx;
}

u16 Rogue_PairGfx(u32 pair, bool32 second)
{
    u32 kind = RogueGen_PairKind(&sRegion, pair) % RPAIR_COUNT;
    return second ? sPairSecondGfx[kind] : sPairClasses[kind].gfx;
}

// A trainer's class comes from the generator (RogueGen_TrainerClass in
// src/rogue_gen_people.h), so the host tools know who says what.
static const struct ClassDef *TrainerClass(u32 slot)
{
    u32 c;
    // A dojo's disciples and master (src/rogue_town.c): a Black Belt, a
    // Battle Girl, an Expert.
    if (slot >= ROGUE_SLOT_DOJO_0 && slot < ROGUE_SLOT_DOJO_0 + ROGUE_DOJO_TRAINERS)
        return &sClasses[slot == ROGUE_SLOT_DOJO_0 ? CLASS_BLACK_BELT : (slot == ROGUE_SLOT_DOJO_0 + 1 ? 13 : 25)];
    if (IsFeatureSlot(slot))
        return FeatureClass(slot);
    c = RogueGen_TrainerClass(&sRegion, slot);
    if (c == ROGUE_CLASS_VILLAIN)
        return VillainClass(slot);
    return &sClasses[c % ARRAY_COUNT(sClasses)];
}

// ---------------------------------------------------------------------------
// Dungeon bosses (the Pokemon, the masters' teams and lines: src/rogue_dungeon.c)

// The dungeon whose master (RBOSS_MASTER) a trainer slot is, or -1.
static s32 MasterDungeon(u32 slot)
{
    s32 d = SlotDungeon(slot);
    if (d < 0 || sRegion.dungeons[d].boss != RBOSS_MASTER
     || slot != (u32)sRegion.dungeons[d].trainerBase + sRegion.dungeons[d].nTrainers - 1)
        return -1;
    return d;
}

// The master's script battles the stand-in slot ROGUE_SLOT_DUNGEON_MASTER,
// the villains' scripts their admin's and leader's (Rogue_ResolveVillainId):
// the battle setup asks here for the slot of the dungeon the player is in.
u16 Rogue_ResolveVillainId(u16 trainerId);

u16 Rogue_ResolveTrainerId(u16 trainerId)
{
    s32 d;
    if (!InRegion())
        return trainerId;
    if (trainerId == ROGUE_TRAINER(ROGUE_SLOT_VILLAIN_ADMIN) || trainerId == ROGUE_TRAINER(ROGUE_SLOT_VILLAIN_LEADER))
    {
        EnsureRegion();
        return Rogue_ResolveVillainId(trainerId);
    }
    if (trainerId != ROGUE_TRAINER(ROGUE_SLOT_DUNGEON_MASTER))
        return trainerId;
    EnsureRegion();
    d = CurrentDungeon();
    if (d < 0 || sRegion.dungeons[d].boss != RBOSS_MASTER || sRegion.dungeons[d].nTrainers == 0)
        return trainerId;
    return ROGUE_TRAINER(sRegion.dungeons[d].trainerBase + sRegion.dungeons[d].nTrainers - 1);
}

// Fills in the object of a dungeon's boss: a trainer, or a Pokemon standing
// in the way, gone once beaten or caught.
static void BuildBossObject(struct ObjectEventTemplate *t, u32 dungeon, u32 slot)
{
    const struct RogueDungeon *d = &sRegion.dungeons[dungeon % ROGUE_MAX_DUNGEONS];
    t->flagId = FLAG_ROGUE_BOSS_0 + (dungeon % ROGUE_MAX_DUNGEONS);
    if (d->boss == RBOSS_ADMIN && RogueGen_HideoutTeam(&sRegion, dungeon % ROGUE_MAX_DUNGEONS) != 0xFF)
    {
        BuildVillainObject(t, ROBJ_BOSS, dungeon % ROGUE_MAX_DUNGEONS, 0xFF);
        return;
    }
    if (d->boss == RBOSS_MASTER)
    {
        // A master waits to be spoken to, as a gym leader does.
        t->graphicsId = RogueDungeon_MasterDef(dungeon)->gfx;
        t->script = Rogue_EventScript_DungeonMaster;
        t->flagId = 0;
        return;
    }
    if (d->boss == RBOSS_ADMIN)
    {
        // A trainer faces the way the map put them (Victory Road's master
        // looks along the step in front of the way out).
        t->graphicsId = TrainerClass(slot)->gfx;
        t->trainerType = TRAINER_TYPE_NORMAL;
        t->trainerRange_berryTreeId = 4;
        t->script = gRogueTrainerScripts[slot];
        t->flagId = 0;
        return;
    }
    t->movementType = MOVEMENT_TYPE_FACE_DOWN;
    t->graphicsId = OBJ_EVENT_MON + RogueDungeon_BossSpecies(dungeon);
    t->script = Rogue_EventScript_DungeonBoss;
}

s32 Rogue_GetCurrentDungeon(void)
{
    EnsureRegion();
    return CurrentDungeon();
}

s32 Rogue_GetFloorDungeon(void)
{
    if (!InRegion() || !IsFloorMap(gSaveBlock1Ptr->location.mapNum))
        return -1;
    EnsureRegion();
    return CurrentDungeon();
}

bool32 Rogue_WildFits(u16 species, const struct RoguePlace *p)
{
    return WildFits(species, p);
}

bool32 Rogue_LineFitsLevel(u16 species, u32 level)
{
    return LineFitsLevel(species, level);
}

u16 Rogue_EvolveForLevel(u16 species, u32 level, bool32 anyMethod)
{
    return EvolveForLevel(species, level, anyMethod);
}

// People out walking on the routes.
static const u16 sRouteFolkGfx[] =
{
    OBJ_EVENT_GFX_HIKER, OBJ_EVENT_GFX_FISHERMAN, OBJ_EVENT_GFX_CAMPER, OBJ_EVENT_GFX_PICNICKER,
    OBJ_EVENT_GFX_LITTLE_BOY, OBJ_EVENT_GFX_OLD_MAN, OBJ_EVENT_GFX_WOMAN_1, OBJ_EVENT_GFX_BOY_1,
};

// The route out of a town along the main road, or the one in, for the
// Pokemon townsfolk walk and the wild lines.
u8 Rogue_RouteNear(u8 place)
{
    return RogueGen_RouteNear(&sRegion, place);
}

// A townsperson's Pokemon is one that lives on the route nearby, so the
// town hints at what is ahead.
static u16 TownMonSpecies(const struct RoguePlace *p, const struct RogueObj *o)
{
    u32 rng = RogueRng_Hash(sRegion.seed, 8500 + p->number * 16 + o->slot);
    const struct RoguePlace *route = &sRegion.places[Rogue_RouteNear(p - sRegion.places)];
    return sWildSpecies[route->area % ROGUE_MAX_AREAS][RogueRng_Range(&rng, WILD_SPECIES_PER_AREA)];
}

static const u16 sObstacleGfx[] = { OBJ_EVENT_GFX_CUTTABLE_TREE, OBJ_EVENT_GFX_BREAKABLE_ROCK, OBJ_EVENT_GFX_PUSHABLE_BOULDER };

// What keeps a legend's chamber shut (enum RogueLegendGate): a cracked
// rock or a boulder, an old statue (a rite's, one with a hollow for its
// key, or one bearing the marks of the legend's set), a keeper who lets
// the holder of a ticket by, or the villain team's lookout. Shown while
// shut (UpdateVillainObjects); taken away by Rogue_EventScript_LegendSeal.
static void BuildSealObject(struct ObjectEventTemplate *t, u32 dungeon)
{
    const struct RogueDungeon *d = &sRegion.dungeons[dungeon % ROGUE_MAX_DUNGEONS];
    t->script = Rogue_EventScript_LegendSeal;
    t->flagId = FLAG_TEMP_15;
    switch (ROGUE_GATE_KIND(d->gate))
    {
    case RGATE_FIELD:
        t->graphicsId = ROGUE_GATE_SRC(d->gate) == ROBST_STRENGTH ? OBJ_EVENT_GFX_PUSHABLE_BOULDER : OBJ_EVENT_GFX_BREAKABLE_ROCK;
        t->movementType = MOVEMENT_TYPE_LOOK_AROUND;
        break;
    case RGATE_GIFT:
        // A sailor for the tickets to the islands, else an old woman.
        t->graphicsId = RogueGen_LegendKey(d->legend) == ITEM_REVEAL_GLASS ? OBJ_EVENT_GFX_OLD_WOMAN : OBJ_EVENT_GFX_SAILOR;
        break;
    case RGATE_VILLAIN:
        // The lookout of the team whose leader is coming.
        t->graphicsId = TeamGruntGfx(ROGUE_GATE_SRC(d->gate));
        break;
    default:
        // An old statue, as the ruins' (Trick House's, tall enough to
        // stand out of the long grass of a glade).
        t->graphicsId = OBJ_EVENT_GFX_TRICK_HOUSE_STATUE;
        t->movementType = MOVEMENT_TYPE_FACE_DOWN;
        break;
    }
}

// The place a generated map is filled from: the place itself, or for a
// dungeon floor the place made up for that floor.
static const struct RoguePlace *MapPlace(const struct RogueMap *m)
{
    if (m->floor == 0)
        return &sRegion.places[m->place];
    RogueGen_FloorPlace(&sRegion, m->dungeon, m->floor, &sFloorPlace);
    return &sFloorPlace;
}

extern const u8 Rogue_EventScript_Run0Thing[];

// The moving truck after RUN 0's STOP ending, in the first town of every
// region: beside the lab's door, the first spot of these where the whole
// truck stands on open ground, so it is in the same place for everyone who
// plays the region. Nothing is under it.
static s32 LabHouse(const struct RogueMap *town);
static void ParkTruck(struct MapSlot *s)
{
    s32 x, y;
    u32 n = s->map.nObjs;
    struct ObjectEventTemplate *t;
    // The spot is the generator's (src/rogue_gen_run0.h), so gen_host can
    // check it for every region.
    if (!RogueGen_TruckSpot(&s->map, &x, &y))
        return;
    t = &s->objects[n];
    memset(t, 0, sizeof(*t));
    t->localId = n + 1;
    t->kind = OBJ_KIND_NORMAL;
    t->graphicsId = OBJ_EVENT_GFX_TRUCK;
    t->x = x;
    t->y = y;
    t->elevation = 3;
    t->movementType = MOVEMENT_TYPE_FACE_DOWN;
    s->events.objectEventCount = n + 1;
}

static void BuildObjects(struct MapSlot *s)
{
    const struct RogueMap *m = &s->map;
    const struct RoguePlace *p = MapPlace(m);
    u32 i;

    for (i = 0; i < m->nObjs; i++)
    {
        const struct RogueObj *o = &m->objs[i];
        struct ObjectEventTemplate *t = &s->objects[i];

        memset(t, 0, sizeof(*t));
        t->localId = i + 1;
        t->kind = OBJ_KIND_NORMAL;
        t->x = o->x;
        t->y = o->y;
        t->elevation = 3;
        // A vanilla room's floors stand at other heights (Magma Hideout's),
        // and a bridge route's road is raised (4), with people on it.
        {
            u32 e = m->blocks[o->y * m->w + o->x] >> 12;
            // (v1.1 sea: a Swimmer treads water, at the sea's height.)
            if ((m->floor && e != 0 && e != 15) || e == 4 || (!m->floor && e == 1))
                t->elevation = e;
        }
        t->movementType = sFacingMovement[o->facing & 3];
        switch (o->kind)
        {
        case ROBJ_TRAINER:
        {
            u32 slot = p->trainerBase + o->slot;
            t->graphicsId = TrainerClass(slot)->gfx;
            t->trainerType = TRAINER_TYPE_NORMAL;
            t->trainerRange_berryTreeId = o->range;
            t->script = gRogueTrainerScripts[slot];
            // Which of their lines they say: worked out here, as the map is
            // built (often in the background), not as they speak.
            t->unused = 1 + RogueText_TrainerSetIndex(&sRegion, slot);
            if (o->facing & ROBJ_LOOKS)
                t->movementType = MOVEMENT_TYPE_LOOK_AROUND;
            else if (o->facing & ROBJ_TURNS)
                t->movementType = sTurnMovement[o->facing & 3][(o->facing >> 2) & 3];
            break;
        }
        case ROBJ_ITEM:
            t->graphicsId = OBJ_EVENT_GFX_ITEM_BALL;
            t->movementType = MOVEMENT_TYPE_FACE_DOWN;
            t->script = Rogue_EventScript_FindItem;
            t->flagId = FLAG_ROGUE_ITEM_0 + p->itemBase + o->slot;
            break;
        case ROBJ_BERRY:
            t->graphicsId = OBJ_EVENT_GFX_BERRY_TREE;
            t->movementType = MOVEMENT_TYPE_BERRY_TREE_GROWTH;
            // A berry farm's trees have their own ids, after the routes'.
            // (And a BERRY MASTER's garden's, after the farm's; v1.1: a
            // berry farm route's field's after those, src/rogue_gen_berry.h.)
            t->trainerRange_berryTreeId = 1 + (p->kind == RPLACE_TOWN ? ROGUE_FARM_BERRY_BASE + o->slot
                                             : (o->slot & 0x80) ? ROGUE_GARDEN_BERRY_BASE + (o->slot & 0x7F)
                                             : (o->slot & 0x40) ? ROGUE_BFARM_BERRY_BASE + (o->slot & 0x3F) : p->berryBase + o->slot);
            t->script = BerryTreeScript;
            break;
        case ROBJ_RIVAL:
            t->graphicsId = gSaveBlock2Ptr->playerGender == MALE ? OBJ_EVENT_GFX_RIVAL_MAY_NORMAL : OBJ_EVENT_GFX_RIVAL_BRENDAN_NORMAL;
            // RUN 0: the Previous Player, in the old games' clothes.
            if (Rogue_Run0RivalGfx())
                t->graphicsId = Rogue_Run0RivalGfx();
            t->trainerType = TRAINER_TYPE_NORMAL;
            t->trainerRange_berryTreeId = o->range;
            t->script = gRogueRivalScripts[o->slot & 3];
            t->flagId = FLAG_ROGUE_RIVAL_1 + (o->slot & 3);
            break;
        case ROBJ_SIGFOLK:
            // The town's signature people (src/rogue_town.c).
            t->graphicsId = Rogue_SigFolkGfx(m->place, o->slot);
            t->script = Rogue_EventScript_SigFolk;
            break;
        case ROBJ_FEATFOLK:
            // A route feature's people (src/rogue_town.c).
            t->graphicsId = Rogue_FeatFolkGfx(m->place, o->slot);
            t->script = Rogue_EventScript_FeatFolk;
            break;
        case ROBJ_FEATMON:
            t->graphicsId = OBJ_EVENT_MON + Rogue_FeatMonSpecies(m->place, o->slot);
            t->movementType = MOVEMENT_TYPE_WANDER_AROUND;
            t->movementRangeX = 1;
            t->movementRangeY = 1;
            t->script = Rogue_EventScript_TownMon;
            break;
        case ROBJ_PAIR:
        {
            // A double battle pair: both say the pair's script.
            u32 k = o->slot & 3;
            t->graphicsId = Rogue_PairGfx(k, (o->slot & 4) != 0);
            t->trainerType = TRAINER_TYPE_NORMAL;
            t->trainerRange_berryTreeId = o->range;
            t->script = gRoguePairScripts[k];
            break;
        }
        case ROBJ_SNORLAX:
            // Asleep in the way until the POKe FLUTE wakes it, once a run.
            t->graphicsId = OBJ_EVENT_MON + SPECIES_SNORLAX;
            t->movementType = MOVEMENT_TYPE_FACE_DOWN;
            t->script = Rogue_EventScript_Snorlax;
            t->flagId = FLAG_ROGUE_FEAT_0 + RFEAT_SNORLAX;
            break;
        case ROBJ_SIGMON:
            t->graphicsId = OBJ_EVENT_MON + Rogue_SigMonSpecies(m->place, o->slot);
            t->movementType = MOVEMENT_TYPE_WANDER_AROUND;
            t->movementRangeX = 1;
            t->movementRangeY = 1;
            t->script = Rogue_EventScript_TownMon;
            break;
        case ROBJ_MON:
            t->graphicsId = OBJ_EVENT_MON + TownMonSpecies(p, o);
            t->movementType = MOVEMENT_TYPE_WANDER_AROUND;
            t->movementRangeX = 1;
            t->movementRangeY = 1;
            t->script = Rogue_EventScript_TownMon;
            break;
        case ROBJ_BOSS:
            BuildBossObject(t, m->dungeon, p->trainerBase + o->slot);
            // A trainer boss sees as far as the map says: Victory Road's
            // master only the step in front of the way out, so they never
            // walk up onto it and stand in the way.
            if (t->trainerType != TRAINER_TYPE_NONE && o->range)
                t->trainerRange_berryTreeId = o->range;
            break;
        case ROBJ_PRIZE:
        case ROBJ_CHOICE:
            // Dungeon extras: slot is the item flag itself.
            t->graphicsId = OBJ_EVENT_GFX_ITEM_BALL;
            t->movementType = MOVEMENT_TYPE_FACE_DOWN;
            t->script = o->kind == ROBJ_CHOICE ? Rogue_EventScript_ChoiceItem : Rogue_EventScript_PrizeItem;
            t->flagId = FLAG_ROGUE_ITEM_0 + o->slot;
            break;
        case ROBJ_GUIDE:
            // On a floor, beside a choice room; in a town, by a tower's door.
            t->graphicsId = m->floor ? RogueDungeon_GuideGfx(m->dungeon) : OBJ_EVENT_GFX_HEX_MANIAC;
            t->movementType = sFacingMovement[o->facing & 3];
            t->script = Rogue_EventScript_ChoiceGuide;
            break;
        case ROBJ_CAMP:
            // A tower's is the old caretaker, as Mt. Pyre has.
            t->graphicsId = m->floor && sRegion.dungeons[m->dungeon % ROGUE_MAX_DUNGEONS].kind == RDUN_TOWER
                          ? OBJ_EVENT_GFX_OLD_WOMAN : OBJ_EVENT_GFX_CAMPER;
            // LATE CAMP's camper by the road shows only from the run after
            // it is earned (hidden by a flag every run has set by then).
            // Victory Road's is always there.
            if (!m->floor && m->place != sRegion.victoryRoad && !Rogue_IsLateCampOn())
                t->flagId = FLAG_SYS_POKEMON_GET;
            t->movementType = MOVEMENT_TYPE_FACE_DOWN;
            t->script = Rogue_EventScript_Camp;
            break;
        case ROBJ_GRUNT:
        case ROBJ_VILLAIN_LEADER:
            BuildVillainObject(t, o->kind, o->slot, m->place);
            break;
        case ROBJ_SEAL:
            BuildSealObject(t, m->dungeon);
            break;
        case ROBJ_KEY:
            // A shipwreck's cabin key, found like any item (Rogue_PrepareItem).
            t->graphicsId = OBJ_EVENT_GFX_ITEM_BALL;
            t->movementType = MOVEMENT_TYPE_FACE_DOWN;
            t->script = Rogue_EventScript_FindItem;
            t->flagId = FLAG_ROGUE_ITEM_0 + o->slot;
            break;
        case ROBJ_DECOY:
            // New Mauville's Voltorb, posing as an item ball: gone once met.
            t->graphicsId = OBJ_EVENT_GFX_ITEM_BALL;
            t->movementType = MOVEMENT_TYPE_LOOK_AROUND;
            t->script = Rogue_EventScript_Decoy;
            t->flagId = FLAG_ROGUE_ITEM_0 + o->slot;
            break;
        case ROBJ_EXPLORER:
            // Gone once led out (the item flag of the reward).
            t->graphicsId = RogueDungeon_ExplorerGfx(m->dungeon);
            t->movementType = MOVEMENT_TYPE_LOOK_AROUND;
            t->script = Rogue_EventScript_Explorer;
            t->flagId = FLAG_ROGUE_ITEM_0 + o->slot;
            break;
        case ROBJ_TUNNEL_GUARD:
            // A Hiker digging, or a villain grunt, in a tunnel's narrow way.
            t->graphicsId = o->slot == 0 ? OBJ_EVENT_GFX_HIKER : TeamGruntGfx(0);
            t->movementType = sFacingMovement[o->facing & 3];
            t->script = Rogue_EventScript_TunnelGuard;
            t->flagId = FLAG_SYS_POKEDEX_GET; // see UpdateVillainObjects
            break;
        case ROBJ_RUN0:
            // One of RUN 0's story things (include/rogue_run0_region.h): the hook
            // goes in trainerRange_berryTreeId for Rogue_Run0ThingHook.
            t->graphicsId = Rogue_Run0HookGfx(o->slot);
            t->trainerRange_berryTreeId = o->slot;
            t->script = Rogue_EventScript_Run0Thing;
            break;
        case ROBJ_CUT_TREE:
        case ROBJ_ROCK:
        case ROBJ_BOULDER:
            // A cleared obstacle stays gone until the map is loaded again.
            t->graphicsId = sObstacleGfx[o->kind - ROBJ_CUT_TREE];
            t->movementType = MOVEMENT_TYPE_LOOK_AROUND;
            t->flagId = FLAG_TEMP_11 + (o->slot & 3);
            // RUN 0, act 3: TERRY clears the shortcut's (gone for the attempt).
            if (Rogue_IsRun0() && (s32)(p - sRegion.places) == RogueGen_Run0ShortcutPlace(&sRegion))
                t->flagId = RUN0_FLAG_SHORTCUT;
            t->script = o->kind == ROBJ_CUT_TREE ? Rogue_EventScript_CutTree
                      : o->kind == ROBJ_ROCK ? Rogue_EventScript_RockSmash : Rogue_EventScript_Boulder;
            if (o->kind == ROBJ_BOULDER)
                t->movementType = MOVEMENT_TYPE_LOOK_AROUND;
            break;
        default:
            if (p->kind == RPLACE_TOWN)
                t->graphicsId = Rogue_TownsfolkObjGfx(m, i); // the reporters in their towns
            else
                t->graphicsId = sRouteFolkGfx[o->slot % ARRAY_COUNT(sRouteFolkGfx)];
            // Some townsfolk stroll about, the rest look around (the reporter
            // pair stay side by side).
            if (o->slot % 3 == 0 && t->graphicsId != OBJ_EVENT_GFX_REPORTER_F && t->graphicsId != OBJ_EVENT_GFX_CAMERAMAN)
            {
                t->movementType = MOVEMENT_TYPE_WANDER_AROUND;
                t->movementRangeX = 1;
                t->movementRangeY = 1;
            }
            else
            {
                t->movementType = MOVEMENT_TYPE_LOOK_AROUND;
            }
            t->script = Rogue_EventScript_Townsperson;
            break;
        }
    }
}

// Where a warp out of a dungeon floor leads: the floor above or below, the
// host place, or the dungeon's exit.
static void FloorWarpDest(const struct RogueMap *m, u32 kind, struct WarpEvent *w)
{
    const struct RogueDungeon *d = &sRegion.dungeons[m->dungeon];
    bool32 caveHost = sRegion.places[d->host].kind == RPLACE_CAVE;
    // A tower's or hideout's room: its stairs and pads (the warp kind is
    // the room's warp index).
    if (RogueGen_FloorRoom(&sRegion, m->dungeon, m->floor) != 0xFF)
    {
        u8 toFloor, toWarp;
        RogueGen_RoomWarpDest(&sRegion, m->dungeon, m->floor, kind, &toFloor, &toWarp);
        w->mapNum = toFloor ? MAP_NUM(MAP_ROGUE_FLOOR_1) + toFloor - 1 : d->host;
        // (Ruins below a cave lead back up its ladder.)
        w->warpId = toFloor ? toWarp : caveHost ? RCAVE_DOWN : RWARP_DUNGEON;
        return;
    }
    if (kind >= RFLOOR_WARPS)
    {
        // The second block of a way two blocks wide (a forest's) leads to
        // the second block at the other end.
        FloorWarpDest(m, kind - RFLOOR_WARPS, w);
        if (w->mapNum < ROGUE_MAX_PLACES)
            w->warpId = w->warpId == RWARP_DUNGEON ? RWARP_DUNGEON_WIDE : RWARP_DUNGEON_2_WIDE;
        else if (RogueGen_FloorRoom(&sRegion, m->dungeon, w->mapNum - MAP_NUM(MAP_ROGUE_FLOOR_1) + 1) == 0xFF)
            w->warpId += RFLOOR_WARPS; // (a legend's chamber has one way in)
        return;
    }
    switch (kind)
    {
    case RFLOOR_UP:
    case RFLOOR_UP_2:
        if (m->floor > 1)
        {
            w->mapNum = MAP_NUM(MAP_ROGUE_FLOOR_1) + m->floor - 2;
            w->warpId = kind == RFLOOR_UP ? RFLOOR_DOWN : RFLOOR_DOWN_2;
            // (v1.1 peaks) Down from a tower's mountainside onto the stairs
            // on of the room below.
            if (RogueGen_FloorRoom(&sRegion, m->dungeon, m->floor - 1) != 0xFF)
                w->warpId = RogueGen_RoomOnWarp(&sRegion, m->dungeon, m->floor - 1);
        }
        else
        {
            w->mapNum = d->host;
            if (caveHost)
                w->warpId = kind == RFLOOR_UP ? RCAVE_DOWN : RCAVE_DOWN_2;
            else
                w->warpId = kind == RFLOOR_UP ? RWARP_DUNGEON : RWARP_DUNGEON_2;
        }
        break;
    case RFLOOR_DOWN:
    case RFLOOR_DOWN_2:
        w->mapNum = MAP_NUM(MAP_ROGUE_FLOOR_1) + m->floor;
        w->warpId = kind == RFLOOR_DOWN ? RFLOOR_UP : RFLOOR_UP_2;
        break;
    case RFLOOR_EXIT:
        w->mapNum = d->exit < sRegion.nPlaces ? d->exit : d->host;
        w->warpId = RWARP_DUNGEON_2;
        break;
    }
}

// Where a warp into a dungeon leads from its host or its exit.
static void DungeonEntryDest(u32 place, u32 kind, struct WarpEvent *w)
{
    s32 di = RogueGen_DungeonAt(&sRegion, place);
    const struct RogueDungeon *d;
    if (di < 0)
        return;
    d = &sRegion.dungeons[di];
    // A forest the road goes through comes out on both halves of its host:
    // the second way in leads to the last floor's way out.
    if ((place == d->exit && d->exit != d->host)
     || (d->exit == d->host && (kind == RWARP_DUNGEON_2 || kind == RWARP_DUNGEON_2_WIDE)))
    {
        w->mapNum = MAP_NUM(MAP_ROGUE_FLOOR_1) + RogueGen_ExitFloor(&sRegion, di) - 1;
        w->warpId = kind == RWARP_DUNGEON_2_WIDE ? RFLOOR_EXIT + RFLOOR_WARPS : RFLOOR_EXIT;
        return;
    }
    w->mapNum = MAP_NUM(MAP_ROGUE_FLOOR_1);
    // A wreck's side stairs lead aboard as its middle ones do: a room's
    // way in is one warp.
    if ((kind == RWARP_DUNGEON_WIDE || kind == RWARP_DUNGEON_2_WIDE) && RogueGen_FloorRoom(&sRegion, di, 1) != 0xFF)
        w->warpId = RFLOOR_UP;
    else if (kind == RWARP_DUNGEON_WIDE)
        w->warpId = RFLOOR_UP + RFLOOR_WARPS;
    else
        w->warpId = (kind == RWARP_DUNGEON_2 || kind == RCAVE_DOWN_2) ? RFLOOR_UP_2 : RFLOOR_UP;
}

static void BuildWarps(struct MapSlot *s)
{
    const struct RogueMap *m = &s->map;
    const struct RoguePlace *p = &sRegion.places[m->place];
    u32 i;

    // One warp per kind so other maps can always warp back to a fixed id.
    // Kinds the map does not have sit off the map and can never be used.
    for (i = 0; i < ROGUE_MAX_WARPS; i++)
    {
        struct WarpEvent *w = &s->warps[i];
        w->x = -1;
        w->y = -1;
        w->elevation = 0;
        w->warpId = 0;
        w->mapGroup = ROGUE_GROUP;
        w->mapNum = m->place;
    }
    for (i = 0; i < m->nWarps; i++)
    {
        struct WarpEvent *w = &s->warps[m->warps[i].kind];
        w->x = m->warps[i].x;
        w->y = m->warps[i].y;
        if (m->floor)
        {
            FloorWarpDest(m, m->warps[i].kind, w);
            continue;
        }
        if (p->kind == RPLACE_CAVE && (m->warps[i].kind == RCAVE_DOWN || m->warps[i].kind == RCAVE_DOWN_2))
        {
            DungeonEntryDest(m->place, m->warps[i].kind, w);
            continue;
        }
        if (m->warps[i].kind == RWARP_DUNGEON || m->warps[i].kind == RWARP_DUNGEON_2
         || (p->kind == RPLACE_ROUTE && (m->warps[i].kind == RWARP_DUNGEON_WIDE || m->warps[i].kind == RWARP_DUNGEON_2_WIDE)))
        {
            DungeonEntryDest(m->place, m->warps[i].kind, w);
            continue;
        }
        if (p->kind == RPLACE_CAVE)
        {
            // A cave joins the cave mouths of the places at either end.
            w->warpId = RWARP_CAVE;
            w->mapNum = m->warps[i].kind == RCAVE_SOUTH ? p->caveIn : p->caveOut;
            continue;
        }
        switch (m->warps[i].kind)
        {
        case RWARP_PC:
            w->mapNum = MAP_NUM(MAP_ROGUE_POKEMON_CENTER);
            break;
        case RWARP_MART:
            w->mapNum = MAP_NUM(MAP_ROGUE_MART);
            break;
        case RWARP_HOUSE:
            w->mapNum = MAP_NUM(MAP_ROGUE_HOUSE);
            Rogue_Run0IslandDoorDest(m, 0, w);
            break;
        default:
            // The other houses share the house map. The warp id says
            // which house it is.
            w->mapNum = MAP_NUM(MAP_ROGUE_HOUSE);
            w->warpId = m->warps[i].kind - RWARP_CAVE;
            // RUN 0: a home whose door opens onto a real Emerald map.
            Rogue_Run0IslandDoorDest(m, w->warpId, w);
            break;
        case RWARP_GYM:
            w->mapNum = p->type == RTOWN_SUMMIT ? MAP_NUM(MAP_ROGUE_LEAGUE) : MAP_NUM(MAP_ROGUE_GYM);
            break;
        case RWARP_CAVE:
        {
            const struct RoguePlace *c = &sRegion.places[p->mouth];
            w->mapNum = p->mouth;
            w->warpId = (c->caveIn == m->place) ? RCAVE_SOUTH : RCAVE_NORTH;
            break;
        }
        }
    }
}

// RUN 0's signs: the PROMPT LOG notes, and real signs moved onto
// generated routes (Emerald's own scripts).
extern const u8 Rogue_EventScript_Run0Prompt0[], Rogue_EventScript_Run0Prompt1[], Rogue_EventScript_Run0Prompt2[],
                Rogue_EventScript_Run0Prompt3[], Rogue_EventScript_Run0Prompt4[], Rogue_EventScript_Run0Prompt5[],
                Rogue_EventScript_Run0Prompt6[], Rogue_EventScript_Run0Prompt7[];
extern const u8 Route101_EventScript_RouteSign[], LittlerootTown_EventScript_TownSign[], OldaleTown_EventScript_TownSign[],
                PetalburgCity_EventScript_CitySign[];
static const u8 *const sRun0PromptScripts[RUN0_HOOK_PROMPTS] =
{
    Rogue_EventScript_Run0Prompt0, Rogue_EventScript_Run0Prompt1, Rogue_EventScript_Run0Prompt2, Rogue_EventScript_Run0Prompt3,
    Rogue_EventScript_Run0Prompt4, Rogue_EventScript_Run0Prompt5, Rogue_EventScript_Run0Prompt6, Rogue_EventScript_Run0Prompt7,
};
static const u8 *const sRun0RealSigns[RUN0_SIGN_COUNT] =
{
    [RUN0_SIGN_ROUTE101]   = Route101_EventScript_RouteSign,
    [RUN0_SIGN_LITTLEROOT] = LittlerootTown_EventScript_TownSign,
    [RUN0_SIGN_OLDALE]     = OldaleTown_EventScript_TownSign,
    [RUN0_SIGN_PETALBURG]  = PetalburgCity_EventScript_CitySign,
};

static void BuildSignsAndGates(struct MapSlot *s)
{
    const struct RogueMap *m = &s->map;
    u32 i;

    for (i = 0; i < m->nSigns; i++)
    {
        struct BgEvent *b = &s->bgs[i];
        memset(b, 0, sizeof(*b));
        b->x = m->signs[i].x;
        b->y = m->signs[i].y;
        b->elevation = 0;
        b->kind = BG_EVENT_PLAYER_FACING_NORTH;
        switch (m->signs[i].kind)
        {
        case RSIGN_TOWN:
            b->kind = BG_EVENT_PLAYER_FACING_ANY;
            b->bgUnion.script = Rogue_EventScript_TownSign;
            break;
        case RSIGN_ROUTE:
            b->kind = BG_EVENT_PLAYER_FACING_ANY;
            b->bgUnion.script = Rogue_EventScript_RouteSign;
            break;
        case RSIGN_HOUSE:
            b->kind = BG_EVENT_PLAYER_FACING_ANY;
            b->bgUnion.script = Rogue_EventScript_HouseSign;
            break;
        case RSIGN_LAB:
            b->kind = BG_EVENT_PLAYER_FACING_ANY;
            b->bgUnion.script = Rogue_EventScript_LabSign;
            break;
        case RSIGN_PC:
            b->bgUnion.script = Common_EventScript_ShowPokemonCenterSign;
            break;
        case RSIGN_MART:
            b->bgUnion.script = Common_EventScript_ShowPokemartSign;
            break;
        case RSIGN_GYM:
            b->bgUnion.script = Rogue_EventScript_GymSign;
            break;
        case RSIGN_GAME_CORNER:
            b->bgUnion.script = Rogue_EventScript_GameCornerSign;
            break;
        case RSIGN_BATTLE_TENT:
            b->bgUnion.script = Rogue_EventScript_BattleTentSign;
            break;
        case RSIGN_MOTEL:
            b->bgUnion.script = Rogue_EventScript_MotelSign;
            break;
        case RSIGN_REST:
            b->kind = BG_EVENT_PLAYER_FACING_ANY;
            b->bgUnion.script = Rogue_EventScript_RestSign;
            break;
        case RSIGN_DUNGEON:
            b->kind = BG_EVENT_PLAYER_FACING_ANY;
            b->bgUnion.script = Rogue_EventScript_DungeonSign;
            break;
        case RSIGN_FOREST:
            b->kind = BG_EVENT_PLAYER_FACING_ANY;
            b->bgUnion.script = Rogue_EventScript_ForestSign;
            break;
        case RSIGN_CARVING:
            b->kind = BG_EVENT_PLAYER_FACING_ANY;
            b->bgUnion.script = Rogue_EventScript_LegendCarving;
            break;
        case RSIGN_TABLET:
            b->bgUnion.script = Rogue_EventScript_RuinsTablet;
            break;
        case RSIGN_LANDMARK:
            b->kind = BG_EVENT_PLAYER_FACING_ANY;
            b->bgUnion.script = Rogue_EventScript_LandmarkSign;
            break;
        case RSIGN_FEATURE:
            b->kind = BG_EVENT_PLAYER_FACING_ANY;
            b->bgUnion.script = Rogue_EventScript_FeatureSign;
            break;
        case RSIGN_SCHOOL:
        case RSIGN_FAN_CLUB:
        case RSIGN_BIKE_SHOP:
        case RSIGN_NAME_RATER:
        case RSIGN_MOVE_DELETER:
        case RSIGN_TUTOR:
        case RSIGN_GIFT:
        {
            static const u8 *const sHouseSigns[] =
            {
                [RSIGN_SCHOOL - RSIGN_SCHOOL]       = Rogue_EventScript_SchoolSign,
                [RSIGN_FAN_CLUB - RSIGN_SCHOOL]     = Rogue_EventScript_FanClubSign,
                [RSIGN_BIKE_SHOP - RSIGN_SCHOOL]    = Rogue_EventScript_BikeShopSign,
                [RSIGN_NAME_RATER - RSIGN_SCHOOL]   = Rogue_EventScript_NameRaterSign,
                [RSIGN_MOVE_DELETER - RSIGN_SCHOOL] = Rogue_EventScript_MoveDeleterSign,
                [RSIGN_TUTOR - RSIGN_SCHOOL]        = Rogue_EventScript_TutorSign,
                [RSIGN_GIFT - RSIGN_SCHOOL]         = Rogue_EventScript_GiftHouseSign,
            };
            // Signs beside houses can stand where they are read from the side.
            b->kind = BG_EVENT_PLAYER_FACING_ANY;
            b->bgUnion.script = sHouseSigns[m->signs[i].kind - RSIGN_SCHOOL];
            break;
        }
        default:
            if (m->signs[i].kind >= RSIGN_RUN0 && m->signs[i].kind < RSIGN_RUN0 + RUN0_HOOK_PROMPTS)
            {
                // RUN 0: a PROMPT LOG note.
                b->kind = BG_EVENT_PLAYER_FACING_ANY;
                b->bgUnion.script = sRun0PromptScripts[m->signs[i].kind - RSIGN_RUN0];
                break;
            }
            if (m->signs[i].kind >= RSIGN_RUN0_REAL && m->signs[i].kind < RSIGN_RUN0_REAL + RUN0_SIGN_COUNT)
            {
                // RUN 0: a real sign moved here, reading Emerald's own words.
                b->kind = BG_EVENT_PLAYER_FACING_ANY;
                b->bgUnion.script = sRun0RealSigns[m->signs[i].kind - RSIGN_RUN0_REAL];
                break;
            }
            if (m->signs[i].kind >= RSIGN_LOCK && m->signs[i].kind < RSIGN_HIDDEN && m->floor)
            {
                // A shipwreck's locked cabin door (Rogue_CheckShipDoor).
                b->kind = BG_EVENT_PLAYER_FACING_ANY;
                b->bgUnion.script = Rogue_EventScript_ShipDoor;
                break;
            }
            if (m->signs[i].kind >= RSIGN_HIDDEN && m->floor == 0)
            {
                // A hidden item. What it is comes once it is found
                // (Rogue_PrepareHiddenItem), so the map needs no ledger.
                b->kind = BG_EVENT_HIDDEN_ITEM;
                b->bgUnion.hiddenItem.item = ITEM_POTION;
                b->bgUnion.hiddenItem.hiddenItemId = FLAG_ROGUE_HIDDEN_0 - FLAG_HIDDEN_ITEMS_START
                                                   + (m->signs[i].kind == RSIGN_HIDDEN_TUNNEL ? RogueGen_TunnelHidden(&sRegion)
                                                      : RogueGen_HiddenBase(&sRegion, m->place) + m->signs[i].kind - RSIGN_HIDDEN);
                b->bgUnion.hiddenItem.quantity = 1;
                break;
            }
            b->bgUnion.script = Rogue_EventScript_LockedDoor;
            break;
        }
    }
    for (i = 0; i < m->nGates; i++)
    {
        struct CoordEvent *c = &s->coords[i];
        memset(c, 0, sizeof(*c));
        c->x = m->gates[i].x;
        c->y = m->gates[i].y;
        c->elevation = 0;
        // The first town's blocks while there is no starter yet. The way to
        // Victory Road checks for eight badges each visit until it opens,
        // and the villains' blockade until their admin is beaten.
        if (m->gates[i].need == ROGUE_GATE_VILLAIN)
        {
            static const u8 *const sVillainGateScripts[] =
            {
                [RDIR_NORTH] = Rogue_EventScript_VillainGateNorth,
                [RDIR_EAST]  = Rogue_EventScript_VillainGateEast,
                [RDIR_SOUTH] = Rogue_EventScript_VillainGateSouth,
                [RDIR_WEST]  = Rogue_EventScript_VillainGateWest,
            };
            c->trigger = VAR_ROGUE_GATE_OPEN;
            c->index = 0;
            c->script = sVillainGateScripts[m->gates[i].dir & 3];
        }
        else if (m->gates[i].need > 1)
        {
            c->trigger = VAR_ROGUE_GATE_OPEN;
            c->index = 0;
            c->script = sBadgeGateScripts[m->gates[i].dir & 3];
        }
        else
        {
            c->trigger = VAR_ROGUE_PROGRESS;
            c->index = ROGUE_PROGRESS_NO_STARTER;
            c->script = sGateScripts[m->gates[i].dir & 3];
        }
    }
}

extern const struct Tileset gTileset_Lavaridge;
extern const struct Tileset gTileset_Fortree;
extern const struct Tileset gTileset_Pacifidlog;
extern const struct Tileset gTileset_Cave;
extern const struct Tileset gTileset_Facility; // (v1.1 peaks: Mt. Pyre's outdoor tileset)

// Secondary tileset and music of each town theme (enum RogueTheme).
static const struct Tileset *const sThemeTilesets[RTHEME_COUNT + 1] =
{
    [RTHEME_MEADOW]  = &gTileset_Petalburg,
    [RTHEME_VERDANT] = &gTileset_Mauville,
    [RTHEME_HARBOR]  = &gTileset_Slateport,
    [RTHEME_ISLAND]  = &gTileset_Mossdeep,
    [RTHEME_STONE]   = &gTileset_Rustboro,
    [RTHEME_COVE]    = &gTileset_Lilycove,
    [RTHEME_BEACH]   = &gTileset_Dewford,
    [RTHEME_ASH]     = &gTileset_Fallarbor,
    [RTHEME_SPRING]  = &gTileset_Lavaridge,
    [RTHEME_TREETOP] = &gTileset_Fortree,
    [RTHEME_RAFT]    = &gTileset_Pacifidlog, // v1.1 towns2
    [RTHEME_CRATER]  = &gTileset_Sootopolis,
    [RTHEME_CURRENTS] = &gTileset_Pacifidlog, // v1.1 sea: a sea route's currents
};

static const u16 sThemeMusic[RTHEME_COUNT] =
{
    [RTHEME_MEADOW]  = MUS_PETALBURG,
    [RTHEME_VERDANT] = MUS_VERDANTURF,
    [RTHEME_HARBOR]  = MUS_SLATEPORT,
    [RTHEME_ISLAND]  = MUS_DEWFORD,
    [RTHEME_STONE]   = MUS_RUSTBORO,
    [RTHEME_COVE]    = MUS_LILYCOVE,
    [RTHEME_BEACH]   = MUS_DEWFORD,
    [RTHEME_ASH]     = MUS_FALLARBOR,
    [RTHEME_SPRING]  = MUS_OLDALE,
    [RTHEME_TREETOP] = MUS_FORTREE,
    [RTHEME_RAFT]    = MUS_LILYCOVE, // Pacifidlog's own
    [RTHEME_CRATER]  = MUS_SOOTOPOLIS,
};

static const u16 sBorderSea[4] = { 0x0170, 0x0170, 0x0170, 0x0170 };
static const u16 sBorderWoods[4] = { 0x04C7, 0x04C6, 0x04C6, 0x04C7 };
static const u16 sBorderRock[4] = { 0x0471, 0x0471, 0x0471, 0x0471 };
// Beyond the edge of a town on ash or sand: its own trees, and in a cave the
// cave's rock.
// Laid out as the ash forest's lattice falls on even and odd blocks (see
// sAshTreePink in rogue_gen.c): even row, then odd row.
static const u16 sBorderAshTrees[4] = { 0x066B, 0x066C, 0x0673, 0x0674 };
static const u16 sBorderIslandTrees[4] = { 0x0643, 0x0643, 0x0643, 0x0643 };
static const u16 sBorderCave[4] = { 0x0611, 0x0611, 0x0611, 0x0611 };
static const u16 sBorderIce[4] = { 0x0781, 0x0781, 0x0781, 0x0781 };   // Shoal Cave's ice room's rock
static const u16 sBorderFiery[4] = { 0x0671, 0x0671, 0x0671, 0x0671 }; // Fiery Path's
// v1.1 towns2: Sootopolis's white rock round a crater town.
static const u16 sBorderCrater[4] = { 0x06D9, 0x06D9, 0x06D9, 0x06D9 };

// Route music by tier, the way the routes of Hoenn get grander.
static const u16 sRouteMusic[] = { MUS_ROUTE101, MUS_ROUTE104, MUS_ROUTE110, MUS_ROUTE113, MUS_ROUTE119, MUS_ROUTE120 };

// Header, layout and connections of every place. Called once per region.
static void BuildHeaders(void)
{
    u32 i, j;

    memset(sSlotOf, 0, sizeof(sSlotOf));
    for (i = 0; i < CACHE_SLOTS; i++)
        sSlots[i].place = 0xFF;
    sCurrentPlace = sPreviousPlace = 0xFF;
    for (i = 0; i < ROGUE_MAX_PLACES; i++)
    {
        const struct RoguePlace *p = &sRegion.places[i < sRegion.nPlaces ? i : 0];
        const struct MapHeader *romHeader = gMapGroups[ROGUE_GROUP][i];

        sLayouts[i] = *gMapLayouts[romHeader->mapLayoutId - 1];
        sLayouts[i].width = p->w;
        sLayouts[i].height = p->h;
        sLayouts[i].map = sEmptyBlocks;
        if (p->kind == RPLACE_CAVE)
            sLayouts[i].secondaryTileset = &gTileset_Cave;
        else if (p->kind == RPLACE_TOWN && p->type == RTOWN_SUMMIT)
            sLayouts[i].secondaryTileset = &gTileset_EverGrande;
        else
            sLayouts[i].secondaryTileset = sThemeTilesets[p->theme <= RTHEME_CURRENTS ? p->theme : 0];

        for (j = 0; j < p->nLinks; j++)
        {
            sConnectionList[i][j].direction = sConnectionDirs[p->links[j].dir & 3];
            sConnectionList[i][j].offset = p->links[j].offset;
            sConnectionList[i][j].mapGroup = ROGUE_GROUP;
            sConnectionList[i][j].mapNum = p->links[j].target;
        }
        sConnections[i].count = p->nLinks;
        sConnections[i].connections = sConnectionList[i];

        sHeaders[i] = *romHeader;
        sHeaders[i].mapLayout = &sLayouts[i];
        sHeaders[i].events = &sEmptyEvents;
        sHeaders[i].connections = &sConnections[i];
        sHeaders[i].mapLayoutId = LAYOUT_ROGUE_SLOT_0 + i;
        if (p->kind == RPLACE_TOWN)
        {
            sHeaders[i].mapType = MAP_TYPE_TOWN;
            if (p->type != RTOWN_SUMMIT)
                sHeaders[i].music = sThemeMusic[p->theme < RTHEME_COUNT ? p->theme : 0];
            else
                sHeaders[i].music = MUS_EVER_GRANDE;
            if (p->type == RTOWN_GYM)
                sHeaders[i].mapType = MAP_TYPE_CITY;
            sHeaders[i].weather = WEATHER_SUNNY;
            // (v1.1 towns2) A raft village's logs sink under each step.
            if (p->theme == RTHEME_RAFT && i != 0)
                sHeaders[i].mapScripts = Rogue_RaftVillage_MapScripts;
            // The League stands on a mountain top, cut off by the map's
            // edge like Ever Grande's.
            if (p->type == RTOWN_SUMMIT)
                sLayouts[i].border = sBorderRock;
            else if (p->theme == RTHEME_ASH)
                sLayouts[i].border = sBorderAshTrees;
            else if (p->theme == RTHEME_BEACH)
                sLayouts[i].border = sBorderIslandTrees;
            else if (p->theme == RTHEME_CRATER) // v1.1 towns2
                sLayouts[i].border = sBorderCrater;
            // A town on the sea looks out over the sea beyond its edge, as
            // Slateport and Lilycove do, and Fortree's woods go on and on.
            sLandBorder[i] = sLayouts[i].border;
            if ((p->layout == RLAYOUT_HARBOR && p->seaSide != RDIR_NONE) || p->theme == RTHEME_RAFT)
                sLayouts[i].border = sBorderSea;
            else if (p->theme == RTHEME_TREETOP && p->type != RTOWN_SUMMIT)
                sLayouts[i].border = sBorderWoods;
        }
        else if (p->kind == RPLACE_CAVE)
        {
            sHeaders[i].mapType = MAP_TYPE_UNDERGROUND;
            sLayouts[i].border = sBorderCave;
            sHeaders[i].music = p->side ? MUS_CAVE_OF_ORIGIN : (p->caveOut == sRegion.summit ? MUS_VICTORY_ROAD : MUS_PETALBURG_WOODS);
            sHeaders[i].weather = WEATHER_NONE;
            sHeaders[i].showMapName = FALSE;
            // A dark cave needs Flash (the second badge) to see by.
            sHeaders[i].cave = p->dark;
            sHeaders[i].allowEscaping = TRUE;
        }
        else
        {
            static const u8 weathers[] = { [RWEATHER_NONE] = WEATHER_NONE, [RWEATHER_SUN] = WEATHER_SUNNY,
                                           [RWEATHER_SHADE] = WEATHER_SHADE, [RWEATHER_RAIN] = WEATHER_RAIN,
                                           [RWEATHER_SAND] = WEATHER_SANDSTORM, [RWEATHER_ASH] = WEATHER_VOLCANIC_ASH,
                                           [RWEATHER_STORM] = WEATHER_RAIN_THUNDERSTORM, [RWEATHER_FOG] = WEATHER_FOG_HORIZONTAL };
            sHeaders[i].mapType = MAP_TYPE_ROUTE;
            sHeaders[i].music = p->type == RROUTE_MOUNTAIN ? MUS_VICTORY_ROAD : sRouteMusic[(p->tier * ARRAY_COUNT(sRouteMusic)) / NUM_TIERS];
            // Some routes are rainy, lake country most of all, and a region
            // has a route known for rain or storms and one for mist.
            // Deserts have sandstorms and ash fields falling ash; the
            // woods are shady.
            // The first two routes stay clear: weather carries into battle,
            // and a new starter has enough to deal with. The generator
            // decides (RogueGen_RouteWeather), so trainers can talk of it.
            sHeaders[i].weather = weathers[RogueGen_RouteWeather(&sRegion, i)];
            // (v1.1 peaks) Into a sandstorm, a hiker hands over GO-GOGGLES
            // (data/scripts/rogue_dungeon.inc).
            // (Not in RUN 0's story region, which stays as it was.)
            if (RogueGen_RouteWeather(&sRegion, i) == RWEATHER_SAND && !RogueGen_IsRun0(&sRegion))
                sHeaders[i].mapScripts = Rogue_Sandstorm_MapScripts;
            // Beyond the edge of the map: the sea off a coast, rock around a
            // desert or the hills, small trees around the woods.
            sLandBorder[i] = sLayouts[i].border;
            if (p->type == RROUTE_COAST || p->type == RROUTE_SEA)
                sLayouts[i].border = sBorderSea;
            else if (p->type == RROUTE_WOODS || p->type == RROUTE_LONGGRASS)
                sLayouts[i].border = sBorderWoods;
            else if (p->type == RROUTE_MOUNTAIN || p->type == RROUTE_HILLS || (p->type == RROUTE_DESERT && p->theme != RTHEME_ASH))
                sLayouts[i].border = sBorderRock;
        }
    }
}

// TRUE if place is joined to near by a map edge: it is drawn at near's edge,
// so its blocks are needed whenever near loads.
static bool32 EdgeJoined(u32 near, u32 place)
{
    return RogueGen_FindLink(&sRegion.places[near], place) != NULL;
}

// The places one step from place: those at its edges, and those its warps
// lead to without a map in between (its cave, or a cave's two ends). Up to
// ROGUE_MAX_EXITS + 2 of them into out; returns how many.
static u32 PlacesNear(u32 place, u8 *out)
{
    const struct RoguePlace *p = &sRegion.places[place];
    u32 i, n = 0;
    for (i = 0; i < p->nLinks; i++)
        out[n++] = p->links[i].target;
    if (p->mouth != 0xFF)
        out[n++] = p->mouth;
    if (p->kind == RPLACE_CAVE)
    {
        if (p->caveIn < sRegion.nPlaces)
            out[n++] = p->caveIn;
        if (p->caveOut < sRegion.nPlaces && p->caveOut != p->caveIn)
            out[n++] = p->caveOut;
    }
    return n;
}

// The place the maps around are generated for: the current place, or on a
// dungeon floor the place it was entered from, which the player comes back
// out to. Indoors before any place has loaded (a run starts in the lab), the
// town outside: Route 1 used to be started only on leaving the lab, and
// finished on the spot a few steps later, as the player came near its edge
// (seed 11: 103 frames). 0xFF if none.
static u32 HomePlace(void)
{
    if (sCurrentPlace < sRegion.nPlaces)
        return sCurrentPlace;
    if (sCurrentPlace == FLOOR_SLOT && sPreviousPlace < sRegion.nPlaces)
        return sPreviousPlace;
    if (sCurrentPlace == 0xFF && VarGet(VAR_ROGUE_TOWN) < sRegion.nPlaces)
        return VarGet(VAR_ROGUE_TOWN);
    return 0xFF;
}

static bool32 WantedFloor(u32 *dungeon, u32 *floor);
static struct MapSlot *CachedFloor(u32 tag, u32 dungeon, u32 floor);

// How far the player stands from the edge of the current place that joins
// place near, in blocks (0 if it does not join it).
static u32 EdgeDistance(u32 near)
{
    const struct RoguePlace *p = &sRegion.places[sCurrentPlace];
    const struct RogueLink *l = RogueGen_FindLink(p, near);
    s32 x = gSaveBlock1Ptr->pos.x, y = gSaveBlock1Ptr->pos.y, d;
    if (l == NULL)
        return 0;
    switch (l->dir)
    {
    case RDIR_NORTH: d = y; break;
    case RDIR_SOUTH: d = p->h - 1 - y; break;
    case RDIR_WEST:  d = x; break;
    default:         d = p->w - 1 - x; break;
    }
    return d < 0 ? 0 : (d > 200 ? 200 : d);
}

// How far the player stands from the nearest way down into a dungeon in the
// current place, in blocks, or 200 if it has none in view.
static u32 DungeonWayDistance(void)
{
    const struct RoguePlace *p;
    const struct RogueMap *m;
    u32 i, best = 200;
    if (sCurrentPlace >= sRegion.nPlaces || !sSlotOf[sCurrentPlace])
        return best;
    p = &sRegion.places[sCurrentPlace];
    m = &sSlots[sSlotOf[sCurrentPlace] - 1].map;
    for (i = 0; i < m->nWarps; i++)
    {
        u8 k = m->warps[i].kind;
        if ((p->kind == RPLACE_CAVE && (k == RCAVE_DOWN || k == RCAVE_DOWN_2))
         || (p->kind != RPLACE_CAVE && (k == RWARP_DUNGEON || k == RWARP_DUNGEON_2
                                     || (p->kind == RPLACE_ROUTE && (k == RWARP_DUNGEON_WIDE || k == RWARP_DUNGEON_2_WIDE)))))
        {
            s32 d = abs(m->warps[i].x - gSaveBlock1Ptr->pos.x) + abs(m->warps[i].y - gSaveBlock1Ptr->pos.y);
            if ((u32)d < best)
                best = d;
        }
    }
    return best;
}

// How much the cache wants a place (or a floor's tag), lowest first: 0 for
// the maps the screen may show now (the current and previous places, the
// places next to the current one, the floor the player is on); then the
// next map's neighbours, generated ahead so crossing into it never waits,
// those beyond the edge the player is nearest first; and the dungeon floor
// the player may walk onto next, by how near its way in is. WANT_NONE for a
// map not worth keeping. Six slots cannot always hold all of them: a place
// with three neighbours can want seven, so the nearer ones go first.
// From the main road, the maps beyond a side branch come SIDE_LATER blocks
// later: the player mostly walks on along the road, and before v1.1.2 a
// big route beyond a side branch the player stood nearer (often one they
// could not reach yet) took the whole walk to make, so the road's next
// route was made on the spot (seed 10394: 109 frames).
#define WANT_NONE 0xFFFF
#define SIDE_LATER 32
static u32 Want(u32 place)
{
    u32 i, n, home, d, f, best = WANT_NONE;
    u8 near[ROGUE_MAX_EXITS + 2];
    if (place == sCurrentPlace || place == sPreviousPlace)
        return 0;
    if (place == FLOOR_SLOT || place == FLOOR_AHEAD)
    {
        if (!WantedFloor(&d, &f) || CachedFloor(place, d, f) == NULL)
            return WANT_NONE;
        return 1 + (sCurrentPlace == FLOOR_SLOT ? 0 : DungeonWayDistance());
    }
    home = HomePlace();
    if (home == 0xFF)
        return WANT_NONE;
    if (place == home)
        return 0;
    n = PlacesNear(home, near);
    for (i = 0; i < n; i++)
    {
        if (near[i] == place)
            return 0;
        // On a floor only the way back out matters.
        if (home == sCurrentPlace && EdgeJoined(near[i], place))
        {
            d = 1 + EdgeDistance(near[i]);
            if (sRegion.places[near[i]].side && !sRegion.places[home].side)
                d += SIDE_LATER;
            if (d < best)
                best = d;
        }
    }
    return best;
}

// A slot to generate into now: an empty one, or the least recently used one
// that is not the current or previous place. 0xFF if there is none.
static u32 PickSlot(void)
{
    u32 i, best = 0xFF;
    u8 oldest = 0;
    for (i = 0; i < CACHE_SLOTS; i++)
    {
        u8 age;
        if (sSlots[i].place == 0xFF)
            return i;
        if (sSlots[i].place == sCurrentPlace || sSlots[i].place == sPreviousPlace)
            continue;
        age = sAge - sSlots[i].age;
        if (best == 0xFF || age > oldest)
        {
            best = i;
            oldest = age;
        }
    }
    return best;
}

// A slot to generate a map wanted this much into ahead of time: an empty
// one, or the one holding the least wanted map if that is wanted less.
static u32 PickSlotFor(u32 want)
{
    u32 i, best = 0xFF, worst = want;
    for (i = 0; i < CACHE_SLOTS; i++)
    {
        u32 w;
        if (sSlots[i].place == 0xFF)
            return i;
        if (sSlots[i].place == 0xFE)
            continue;
        w = Want(sSlots[i].place);
        if (w > worst || (w == worst && best != 0xFF && (u8)(sAge - sSlots[i].age) > (u8)(sAge - sSlots[best].age)))
        {
            best = i;
            worst = w;
        }
    }
    return best;
}

static void ClearSlot(struct MapSlot *s)
{
    if (s->place != 0xFF && sSlotOf[s->place] == (s - sSlots) + 1)
    {
        sSlotOf[s->place] = 0;
        sHeaders[s->place].events = &sEmptyEvents;
        sLayouts[s->place].map = sEmptyBlocks;
    }
    s->place = 0xFF;
}

// Generates place into slot s and builds its engine structures.
// The dungeon floor the floor slot is to hold next (see EnsureFloor).
static EWRAM_DATA u8 sWantDungeon = 0, sWantFloor = 0;

#ifdef ROGUE_YIELD_PROFILE
// Marks for the harness's YIELD profile (see EnsureRegion).
extern bool8 gRogueGenBackground;
void RogueThread_Yield(void);
void __attribute__((noipa)) RogueGen_YieldPoint(u32 line, const char *file)
{
    if (gRogueGenBackground && ROGUE_LATE_IN_FRAME())
        RogueThread_Yield();
}
void ProfileMark(u32 line, const char *file);
void __attribute__((noipa)) ProfileMark(u32 line, const char *file) { asm volatile("" : : "r"(line), "r"(file)); }
#define PROFILE_MARK() ProfileMark(__LINE__, __FILE__)
#define PROFILE_END() ProfileMark(0, __FILE__)
#else
#define PROFILE_MARK()
#define PROFILE_END()
#endif

// New Mauville's blue and green switches, where its map has them: a blue
// one works while VAR_TEMP_1 is 0, a green one while VAR_TEMP_2 is, and
// its own scripts raise and lower the barriers.
#define PLANT_SWITCH(px, py, var, s) { .x = px, .y = py, .elevation = 3, .trigger = var, .index = 0, .script = s }
static const struct CoordEvent sPlantSwitches[] =
{
    PLANT_SWITCH(30, 38, VAR_TEMP_1, NewMauville_Inside_EventScript_BlueButton),
    PLANT_SWITCH(18, 36, VAR_TEMP_2, NewMauville_Inside_EventScript_GreenButton),
    PLANT_SWITCH(4, 26, VAR_TEMP_1, NewMauville_Inside_EventScript_BlueButton),
    PLANT_SWITCH(16, 22, VAR_TEMP_1, NewMauville_Inside_EventScript_BlueButton),
    PLANT_SWITCH(25, 18, VAR_TEMP_2, NewMauville_Inside_EventScript_GreenButton),
    PLANT_SWITCH(2, 11, VAR_TEMP_2, NewMauville_Inside_EventScript_GreenButton),
    PLANT_SWITCH(6, 11, VAR_TEMP_1, NewMauville_Inside_EventScript_BlueButton),
    PLANT_SWITCH(13, 10, VAR_TEMP_1, NewMauville_Inside_EventScript_BlueButton),
    PLANT_SWITCH(17, 10, VAR_TEMP_2, NewMauville_Inside_EventScript_GreenButton),
};

static void FillSlot(struct MapSlot *s, u32 place)
{
    PROFILE_MARK();
    if (place == FLOOR_SLOT || place == FLOOR_AHEAD)
        RogueGen_GenerateFloor(&sRegion, sWantDungeon, sWantFloor, &s->map, s->blocks);
    else
    {
        RogueGen_GenerateMap(&sRegion, place, &s->map, s->blocks);
        Rogue_Run0Generated(place);
    }

    BuildObjects(s);
    BuildWarps(s);
    BuildSignsAndGates(s);
    s->events.objectEventCount = s->map.nObjs;
    // After RUN 0's STOP: the moving truck, parked in every first town.
    // (Asked of the Director, not a flag: the professor's preview makes the
    // first town before the run's flags are set.)
    if (place == sRegion.start && s->map.nObjs < ROGUE_MAX_OBJS && Rogue_Run0TruckParked())
        ParkTruck(s);
    s->events.warpCount = ROGUE_MAX_WARPS;
    s->events.coordEventCount = s->map.nGates;
    s->events.bgEventCount = s->map.nSigns;
    s->events.objectEvents = s->objects;
    s->events.warps = s->warps;
    s->events.coordEvents = s->coords;
    s->events.bgEvents = s->bgs;
    // New Mauville's switches, its own coord events (a floor has no gates).
    if (s->map.floor && RogueGen_FloorRoom(&sRegion, s->map.dungeon, s->map.floor) != 0xFF
     && sRogueRoomStyles[RogueGen_FloorRoom(&sRegion, s->map.dungeon, s->map.floor)] == ROOM_STYLE_PLANT)
    {
        s->events.coordEvents = sPlantSwitches;
        s->events.coordEventCount = ARRAY_COUNT(sPlantSwitches);
    }
    PROFILE_END();
}

// The border to draw at (x, y), a block past the edge of the map in map
// blocks. A map has one border pattern, so a coast with the sea on one side
// and woods on another showed open sea past its trees (or, with a land
// border, trees standing in its sea). On a map that looks out to sea, past
// the edge the sea goes on where the map's nearest edge block is sea, and
// its land border (trees, rock) where it is land.
const u16 *Rogue_LayoutBorder(const struct MapLayout *layout, int x, int y)
{
    u32 place = layout - sLayouts;
    u16 b;
    // RUN 0's real maps end in black where nothing joins them.
    const u16 *run0 = Rogue_Run0Border();
    if (run0 != NULL)
        return run0;
    if (layout->border != sBorderSea || place >= ROGUE_MAX_PLACES || sLandBorder[place] == NULL || layout->map == sEmptyBlocks)
        return layout->border;
    x = x < 0 ? 0 : (x >= layout->width ? layout->width - 1 : x);
    y = y < 0 ? 0 : (y >= layout->height ? layout->height - 1 : y);
    b = layout->map[y * layout->width + x];
    // The sea, or water at the sea's height (rocks and banks in it).
    if ((b & 0x3FF) == 0x170 || (b >> 12) == 1)
        return sBorderSea;
    return sLandBorder[place];
}

static void PublishSlot(struct MapSlot *s, u32 place)
{
    s->place = place;
    s->age = ++sAge;
    sSlotOf[place] = (s - sSlots) + 1;
    sHeaders[place].events = &s->events;
    sLayouts[place].map = s->blocks;
}

// ---------------------------------------------------------------------------
// Generating ahead
//
// The generator can run as a second thread with its own stack. Each frame in
// the overworld, once the frame's own work is done, Rogue_BackgroundStep
// resumes it, and it hands control back (RogueThread_Yield) when the screen
// is about to refresh. So the next map's neighbours are usually ready before
// the player walks into it, and crossing a map edge never waits for them.

#define GEN_STACK_WORDS 640
static EWRAM_DATA u32 sGenStack[GEN_STACK_WORDS] = {0};
static EWRAM_DATA u32 sMainSp = 0, sGenSp = 0;
static EWRAM_DATA u8 sJobPlace = 0, sJobSlot = 0;
static EWRAM_DATA bool8 sJobActive = FALSE, sThreadReady = FALSE;
// The current map's connections whose edges are left for later (bit i for
// connection i; see Rogue_EnsureMapBlocks).
static EWRAM_DATA u8 sLateEdges = 0;
static void FillLateEdges(void);
EWRAM_DATA bool8 gRogueGenBackground = FALSE;

void RogueThread_Switch(u32 *saveSp, u32 newSp);

void RogueThread_Yield(void)
{
    RogueThread_Switch(&sGenSp, sMainSp);
}

// For game code the generator thread runs (building a map's people): hands
// the frame back if the screen is about to refresh, as the generator's
// YIELD does.
void Rogue_YieldIfLate(void)
{
#ifdef ROGUE_YIELD_PROFILE
    RogueGen_YieldPoint(__LINE__, __FILE__);
#else
    if (gRogueGenBackground && ROGUE_LATE_IN_FRAME())
        RogueThread_Yield();
#endif
}

static void GenThread(void)
{
    for (;;)
    {
        FillSlot(&sSlots[sJobSlot], sJobPlace);
        sJobActive = FALSE;
        RogueThread_Switch(&sGenSp, sMainSp);
    }
}

static void ResumeJob(bool32 background)
{
    gRogueGenBackground = background;
    RogueThread_Switch(&sMainSp, sGenSp);
    gRogueGenBackground = FALSE;
}

// Runs the background job to the end, without handing frames back.
static void FinishJob(void)
{
    while (sJobActive)
        ResumeJob(FALSE);
    if (sSlots[sJobSlot].place == 0xFE)
        PublishSlot(&sSlots[sJobSlot], sJobPlace);
}

// Drops the job (when the region itself is replaced, or another map is
// needed now). The thread starts afresh next time.
static void AbandonJob(void)
{
    if (sJobActive)
        sSlots[sJobSlot].place = 0xFF;
    sJobActive = FALSE;
    sThreadReady = FALSE;
    RogueGen_DropJob();
}

static void StartJob(u32 place, u32 slot)
{
    if (!sThreadReady)
    {
        // A new stack whose first switch "returns" into GenThread: eight
        // saved registers, then the return address.
        u32 *sp = &sGenStack[GEN_STACK_WORDS - 9];
        u32 i;
        // Marked, so tests can see how deep the stack has gone.
        for (i = 0; i < GEN_STACK_WORDS - 9; i++)
            sGenStack[i] = 0xA5A5A5A5;
        for (i = 0; i < 8; i++)
            sp[i] = 0;
        sp[8] = (u32)GenThread;
        sGenSp = (u32)sp;
        sThreadReady = TRUE;
    }
    ClearSlot(&sSlots[slot]);
    sSlots[slot].place = 0xFE; // being generated
    sJobPlace = place;
    sJobSlot = slot;
    sJobActive = TRUE;
}

// Whether place is a better pick to generate ahead than best: wanted more,
// or as much and further along the main road.
static bool32 BetterPick(u32 place, u32 want, u32 best, u32 bestWant)
{
    if (sSlotOf[place] || want == WANT_NONE)
        return FALSE;
    return want < bestWant || (want == bestWant && place > best && !sRegion.places[place].side);
}

// The next map to generate ahead: the most wanted one not in the cache (see
// Want), or 0xFF. The candidates are the places near the current one and
// those at their edges. FLOOR_AHEAD for a dungeon floor, which goes into
// sWantDungeon and sWantFloor.
static u32 NextToGenerate(u32 *want)
{
    u32 i, j, n = 0, w, home = HomePlace(), best = 0xFF, bestWant = WANT_NONE, dungeon, floor;
    u8 near[ROGUE_MAX_EXITS + 2];
    if (home != 0xFF)
        n = PlacesNear(home, near);
    for (i = 0; i < n; i++)
    {
        const struct RoguePlace *p = &sRegion.places[near[i]];
        if (BetterPick(near[i], 0, best, bestWant))
        {
            best = near[i];
            bestWant = 0;
        }
        for (j = 0; j < p->nLinks && home == sCurrentPlace; j++)
        {
            u32 t = p->links[j].target;
            w = Want(t);
            if (BetterPick(t, w, best, bestWant))
            {
                best = t;
                bestWant = w;
            }
        }
    }
    if (FloorToGenerate(&dungeon, &floor))
    {
        w = 1 + (sCurrentPlace == FLOOR_SLOT ? 0 : DungeonWayDistance());
        if (w < bestWant)
        {
            best = FLOOR_AHEAD;
            bestWant = w;
            sWantDungeon = dungeon;
            sWantFloor = floor;
        }
    }
    *want = bestWant;
    return best;
}

// Called every overworld frame once its work is done.
void Rogue_BackgroundStep(void)
{
#ifdef ROGUE_YIELD_PROFILE
    // The profile times the generator alone: no maps are made ahead.
    return;
#endif
    if (!sRegionReady || gSaveBlock1Ptr->location.mapGroup != ROGUE_GROUP)
        return;
    if (sLateEdges)
        FillLateEdges();
    // run0test.py: the kept Pokemon of the trainer built last are shown
    // (0xFFFD, as its battle's start would), a badge's kept Pokemon given
    // back (0xFFFC), an area's catch made (0xFFFB), the Copy's party made
    // (0xFFFA), a trainer's party made (0xFFF9).
    if (gRogueTestTrainer >= 0xFFF9 && gRogueTestTrainer <= 0xFFFD)
    {
        if (gRogueTestTrainer == 0xFFFB)
            Run0TestCatch();
        else if (gRogueTestTrainer == 0xFFF9)
            Run0TestParty();
        else if (gRogueTestTrainer == 0xFFFD)
        {
            struct RogueDirector *d = Rogue_Director();
            if (d != NULL)
            {
                Rogue_Run0NoteKeptShown(d, sTrainerCachedId);
                Rogue_SaveDirector();
            }
        }
        else if (gRogueTestTrainer == 0xFFFC)
        {
            Rogue_Run0ReturnKept();
        }
        else if (gRogueTestTrainer == 0xFFFA)
        {
            Run0TestCopy();
        }
        gRogueTestTrainer = 0;
        return;
    }
    // run0test.py: a badge's patch (0xFFFF) and its box (0xFFFE).
    if (gRogueTestTrainer == 0xFFFF || gRogueTestTrainer == 0xFFFE)
    {
        if (gRogueTestTrainer == 0xFFFF)
            Rogue_Run0PatchAfterBadge();
        else
            Rogue_Run0BufferPatch();
        gRogueTestTrainer = 0;
        return;
    }
    if (gRogueTestTrainer != 0)
    {
        Rogue_GetTrainer(gRogueTestTrainer);
        gRogueTestTrainer = 0;
        return;
    }
    if (gRogueTestWarp[0] && !ArePlayerFieldControlsLocked())
    {
        SetWarpDestination(ROGUE_GROUP, gRogueTestWarp[0] - 1, WARP_ID_NONE, gRogueTestWarp[1], gRogueTestWarp[2]);
        gRogueTestWarp[0] = 0;
        LockPlayerFieldControls();
        DoWarp();
        ResetInitialPlayerAvatarState();
        return;
    }
    if (!sJobActive)
    {
        u32 want, slot, place;
        // Looking for work costs time too: a few times a second is enough.
        if (gMain.vblankCounter1 & 7)
            return;
        place = NextToGenerate(&want);
        if (place == 0xFF)
            return;
        slot = (place == FLOOR_AHEAD && sSlotOf[FLOOR_AHEAD]) ? sSlotOf[FLOOR_AHEAD] - 1 : PickSlotFor(want);
        if (slot == 0xFF)
            return;
        StartJob(place, slot);
    }
    if (ROGUE_LATE_IN_FRAME())
        return; // no time left in this frame
    ResumeJob(TRUE);
    if (!sJobActive)
    {
        PublishSlot(&sSlots[sJobSlot], sJobPlace);
        if (sLateEdges)
            FillLateEdges();
    }
}

// Generates a place into the cache now, if it is not there already. The
// current and previous places are never evicted.
static void DropPlace(u32 place);
static struct MapSlot *EnsurePlace(u32 place)
{
    struct MapSlot *s;
    u32 slot, start;

    if (place >= sRegion.nPlaces)
        return NULL;
    // A place in the cache is never the job's slot (StartJob cleared that
    // one), so the job can go on in the background. Finishing it here froze
    // the game for seconds whenever the engine looked up the current map's
    // header (every door) while a big neighbour was being generated.
    if (sSlotOf[place])
    {
        s = &sSlots[sSlotOf[place] - 1];
        s->age = ++sAge;
        return s;
    }
    // A job making another map is dropped, to start again later: finishing
    // it first would only make the wait for this one longer.
    if (sJobActive && sJobPlace != place)
        AbandonJob();
    else if (sJobActive || (sThreadReady && sSlots[sJobSlot].place == 0xFE))
        FinishJob();
    // The job may have just made this one.
    if (sSlotOf[place])
        return &sSlots[sSlotOf[place] - 1];
    slot = PickSlot();
    s = &sSlots[slot];
    ClearSlot(s);
    start = gMain.vblankCounter1;
    FillSlot(s, place);
    gRogueGenFrames = gMain.vblankCounter1 - start;
    gRogueSyncGens++;
    PublishSlot(s, place);
    return s;
}

// Forgets a place's map (and its job), so it is made again when next needed.
static void DropPlace(u32 place)
{
    if (sJobActive && sJobPlace == place)
        AbandonJob();
    if (place < ROGUE_MAX_PLACES && sSlotOf[place])
        ClearSlot(&sSlots[sSlotOf[place] - 1]);
}

// The slot under tag (FLOOR_SLOT or FLOOR_AHEAD) if it holds this floor.
static struct MapSlot *CachedFloor(u32 tag, u32 dungeon, u32 floor)
{
    struct MapSlot *s;
    if (!sSlotOf[tag])
        return NULL;
    s = &sSlots[sSlotOf[tag] - 1];
    return (s->map.dungeon == dungeon && s->map.floor == floor) ? s : NULL;
}

// The floor generated ahead becomes the current one, and the other way round.
static void SwapFloorSlots(void)
{
    u8 cur = sSlotOf[FLOOR_SLOT], ahead = sSlotOf[FLOOR_AHEAD];
    sSlotOf[FLOOR_SLOT] = ahead;
    sSlotOf[FLOOR_AHEAD] = cur;
    if (ahead)
        sSlots[ahead - 1].place = FLOOR_SLOT;
    if (cur)
        sSlots[cur - 1].place = FLOOR_AHEAD;
}

// The slot of a dungeon floor, as the floor the player is on: the one
// generated ahead if it is that floor, or generated now. Floors are made on
// the generator's own stack, as the maps ahead are: this runs while a map
// loads, deep in the main stack, where a cave's searches and a VBlank on top
// would run out of room.
static struct MapSlot *EnsureFloor(u32 dungeon, u32 floor)
{
    struct MapSlot *s;
    u32 start;

    if (dungeon >= sRegion.nDungeons)
        return NULL;
    if (floor < 1)
        floor = 1;
    if (floor > sRegion.dungeons[dungeon].nFloors)
        floor = sRegion.dungeons[dungeon].nFloors;
    // A job making anything but this floor is dropped (see EnsurePlace).
    if (sJobActive && !(sJobPlace == FLOOR_AHEAD && sWantDungeon == dungeon && sWantFloor == floor))
        AbandonJob();
    else if (sJobActive || (sThreadReady && sSlots[sJobSlot].place == 0xFE))
        FinishJob();
    if ((s = CachedFloor(FLOOR_SLOT, dungeon, floor)) == NULL && CachedFloor(FLOOR_AHEAD, dungeon, floor))
    {
        SwapFloorSlots();
        s = &sSlots[sSlotOf[FLOOR_SLOT] - 1];
    }
    if (s != NULL)
    {
        s->age = ++sAge;
        return s;
    }
    s = sSlotOf[FLOOR_SLOT] ? &sSlots[sSlotOf[FLOOR_SLOT] - 1] : &sSlots[PickSlot()];
    sWantDungeon = dungeon;
    sWantFloor = floor;
    start = gMain.vblankCounter1;
    StartJob(FLOOR_SLOT, s - sSlots);
    FinishJob();
    gRogueGenFrames = gMain.vblankCounter1 - start;
    gRogueSyncGens++;
    return s;
}

// The dungeon floor worth having ready, if any: the next one down from the
// floor the player is on (from the bottom floor, the one above, which the way
// through a dungeon climbs back to), or the first floor from the place it is
// entered from (the last, from its exit).
static bool32 WantedFloor(u32 *dungeon, u32 *floor)
{
    s32 d;
    u32 f;
    if (sCurrentPlace == FLOOR_SLOT && sSlotOf[FLOOR_SLOT])
    {
        const struct MapSlot *s = &sSlots[sSlotOf[FLOOR_SLOT] - 1];
        d = s->map.dungeon;
        f = s->map.floor + 1;
        if (d >= sRegion.nDungeons)
            return FALSE;
        if (f > sRegion.dungeons[d].nFloors)
            f = s->map.floor - 1;
        if (f < 1)
            return FALSE;
    }
    else if (sCurrentPlace < sRegion.nPlaces)
    {
        d = RogueGen_DungeonAt(&sRegion, sCurrentPlace);
        if (d < 0)
            return FALSE;
        f = (sRegion.dungeons[d].exit == sCurrentPlace && sRegion.dungeons[d].exit != sRegion.dungeons[d].host)
          ? RogueGen_ExitFloor(&sRegion, d) : 1;
    }
    else
    {
        return FALSE;
    }
    *dungeon = d;
    *floor = f;
    return TRUE;
}

// The wanted floor, if it is not generated yet.
static bool32 FloorToGenerate(u32 *dungeon, u32 *floor)
{
    if (!WantedFloor(dungeon, floor))
        return FALSE;
    return !CachedFloor(FLOOR_SLOT, *dungeon, *floor) && !CachedFloor(FLOOR_AHEAD, *dungeon, *floor);
}

static void LoadingSayWhere(void);
static bool32 RegionReadyFor(u32 seed);
static void ForgetMaps(void);
static void PrewarmSavedMap(void);

static void EnsureRegion(void)
{
    u32 seed = Rogue_GetSeed();
    u32 mix = VarGet(VAR_ROGUE_DEX_MIX);
    if (mix >= ROGUE_MIX_COUNT)
        mix = ROGUE_MIX_CLASSIC;
    if (sRegionReady && sRegion.seed == seed && sRegionMix == mix)
        return;
    AbandonJob();
    // (Behind the loading screen, when it is up: src/rogue_loading.c.)
    Rogue_LoadingStage(70, 140);
    RogueGen_BuildRegion(&sRegion, seed);
    Rogue_LoadingStage(85, 20);
    // The mix is part of what defines the run's region: the same name and
    // POKeDEX choice give the same POKeMON.
    sRegionMix = mix;
    GenerateContent(seed);
    BuildHeaders();
    sTrainerCachedId = 0;
    sRegionReady = TRUE;
    LoadingSayWhere();
    if (gRogueTestProfile)
    {
        // Each place, then each dungeon floor, once. A YIELD-profiling build
        // (ROGUE_YIELD_PROFILE) lets the harness time the work between the
        // generator's yields as it goes.
        u32 i, j;
        for (i = 0; i < sRegion.nPlaces; i++)
        {
            EnsurePlace(i);
            gRogueGenProfile[i] = gRogueGenFrames > 255 ? 255 : gRogueGenFrames;
        }
        for (i = 0; i < sRegion.nDungeons; i++)
            for (j = 1; j <= sRegion.dungeons[i].nFloors; j++)
                EnsureFloor(i, j);
    }
}

const struct RogueRegion *Rogue_GetRegion(void)
{
    EnsureRegion();
    return &sRegion;
}

// The generated map of a place, generating it if needed.
const struct RogueMap *Rogue_GetPlaceMap(u32 place)
{
    struct MapSlot *s;
    EnsureRegion();
    s = EnsurePlace(place);
    return s ? &s->map : NULL;
}

// ---------------------------------------------------------------------------
// Where the player is

static bool32 InRegion(void)
{
    return gSaveBlock1Ptr->location.mapGroup == ROGUE_GROUP;
}

static bool32 IsLeagueMap(u32 mapNum)
{
    return mapNum >= MAP_NUM(MAP_ROGUE_LEAGUE) && mapNum <= MAP_NUM(MAP_ROGUE_HALL_OF_FAME);
}

// The place the player is in. Interiors count as their town, the League
// rooms as the summit.
s32 Rogue_GetCurrentPlace(void)
{
    u32 mapNum = gSaveBlock1Ptr->location.mapNum;
    if (!InRegion())
        return -1;
    EnsureRegion();
    if (mapNum < ROGUE_MAX_PLACES)
        return mapNum < sRegion.nPlaces ? (s32)mapNum : 0;
    if (IsLeagueMap(mapNum))
        return sRegion.summit;
    return VarGet(VAR_ROGUE_TOWN) < sRegion.nPlaces ? VarGet(VAR_ROGUE_TOWN) : 0;
}

// Town number of the current map. Interiors count as their town; routes and
// caves as town 0.
u32 Rogue_GetCurrentTown(void)
{
    s32 place = Rogue_GetCurrentPlace();
    if (place < 0 || sRegion.places[place].kind != RPLACE_TOWN)
        return 0;
    return sRegion.places[place].number;
}

// The place of the town the player is in, or VAR_ROGUE_TOWN for interiors.
u32 Rogue_GetCurrentTownPlace(void)
{
    s32 place = Rogue_GetCurrentPlace();
    if (place < 0 || sRegion.places[place].kind != RPLACE_TOWN)
        return sRegion.start;
    return place;
}

s32 Rogue_GetAreaId(void)
{
    u32 mapNum = gSaveBlock1Ptr->location.mapNum;
    if (InRegion() && IsFloorMap(mapNum))
    {
        s32 d;
        EnsureRegion();
        d = CurrentDungeon();
        return (d >= 0 && sRegion.dungeons[d].area != 0xFF) ? sRegion.dungeons[d].area : -1;
    }
    if (!InRegion() || mapNum >= ROGUE_MAX_PLACES)
        return -1;
    EnsureRegion();
    if (mapNum >= sRegion.nPlaces || sRegion.places[mapNum].area == 0xFF)
        return -1;
    return sRegion.places[mapNum].area;
}

bool32 Rogue_IsAreaCatchUsed(u32 area)
{
    u16 bits = VarGet(area < 16 ? VAR_ROGUE_ENCOUNTERS : area < 32 ? VAR_ROGUE_ENCOUNTERS_2 : VAR_ROGUE_ENCOUNTERS_3);
    return (bits >> (area & 15)) & 1;
}

void Rogue_SetAreaCatchUsed(u32 area)
{
    u16 var = area < 16 ? VAR_ROGUE_ENCOUNTERS : area < 32 ? VAR_ROGUE_ENCOUNTERS_2 : VAR_ROGUE_ENCOUNTERS_3;
    VarSet(var, VarGet(var) | (1 << (area & 15)));
}

void Rogue_ClearAreaCatchUsed(u32 area)
{
    u16 var = area < 16 ? VAR_ROGUE_ENCOUNTERS : area < 32 ? VAR_ROGUE_ENCOUNTERS_2 : VAR_ROGUE_ENCOUNTERS_3;
    VarSet(var, VarGet(var) & ~(1 << (area & 15)));
}

bool32 Rogue_IsAreaCaught(u32 area)
{
    return area < 48 && FlagGet(FLAG_ROGUE_CAUGHT_0 + area);
}

void Rogue_SetAreaCaught(u32 area)
{
    if (area < 48)
        FlagSet(FLAG_ROGUE_CAUGHT_0 + area);
}

// A place the player has been to this run. Towns also count from Fly's
// record, and areas whose catch is used, for saves made before the record.
bool32 Rogue_IsPlaceVisited(u32 place)
{
    const struct RoguePlace *p;
    EnsureRegion();
    if (place >= sRegion.nPlaces)
        return FALSE;
    p = &sRegion.places[place];
    if (FlagGet(FLAG_ROGUE_VISITED_0 + place) || (s32)place == Rogue_GetCurrentPlace())
        return TRUE;
    if (p->kind == RPLACE_TOWN && (VarGet(VAR_ROGUE_VISITED) & (1 << p->number)))
        return TRUE;
    return p->area != 0xFF && Rogue_IsAreaCatchUsed(p->area);
}

// The map's record of the run, cleared with each new region.
static void ClearPlaceRecords(void)
{
    u32 i;
    VarSet(VAR_ROGUE_VISITED, 0);
    for (i = 0; i < 48; i++)
    {
        FlagClear(FLAG_ROGUE_VISITED_0 + i);
        FlagClear(FLAG_ROGUE_CAUGHT_0 + i);
    }
}

// ---------------------------------------------------------------------------
// Engine hooks

static bool32 IsFloorMap(u32 mapNum)
{
    return mapNum >= MAP_NUM(MAP_ROGUE_FLOOR_1) && mapNum < MAP_NUM(MAP_ROGUE_FLOOR_1) + ROGUE_MAX_FLOORS;
}

// The dungeon the player is in or by: the one entered from the last place.
static s32 CurrentDungeon(void)
{
    return RogueGen_DungeonAt(&sRegion, VarGet(VAR_ROGUE_TOWN));
}

static const u16 sDungeonMusic[RDUN_COUNT] =
{
    [RDUN_CAVE]    = MUS_CAVE_OF_ORIGIN,
    [RDUN_VICTORY] = MUS_VICTORY_ROAD,
    [RDUN_FOREST]  = MUS_PETALBURG_WOODS,
    [RDUN_TOWER]   = MUS_MT_PYRE,
    [RDUN_HIDEOUT] = MUS_AQUA_MAGMA_HIDEOUT,
    [RDUN_LAIR]    = MUS_SEALED_CHAMBER,
    [RDUN_RUINS]   = MUS_SEALED_CHAMBER, // as the Desert Ruins and the Ancient Tomb
    [RDUN_SHIP]    = MUS_ABANDONED_SHIP,
    [RDUN_PLANT]   = MUS_MT_PYRE, // New Mauville's own
    [RDUN_ICE]     = MUS_RG_MT_MOON,  // Seafoam Islands'
    [RDUN_VOLCANO] = MUS_MT_CHIMNEY,
    [RDUN_MIRAGE]  = MUS_MT_CHIMNEY, // (v1.1 peaks) the Mirage Tower's own
};

// A forest floor is outdoors, drawn as Petalburg Woods is: General and
// Rustboro's tilesets, small trees beyond the edge, shade, and the woods'
// own music. Its popup names the route it lies off, without a floor.
static void SetForestHeader(struct MapHeader *h, struct MapLayout *layout)
{
    layout->secondaryTileset = &gTileset_Rustboro;
    layout->border = sBorderWoods;
    h->mapType = MAP_TYPE_ROUTE;
    h->weather = WEATHER_SHADE;
    h->cave = FALSE;
    h->floorNumber = 0;
}

// The header of the dungeon floor the player is on (or arriving on): the
// floor of the dungeon entered from the last place. Built only for that map.
static const struct MapHeader *GetFloorHeader(u16 mapNum)
{
    const struct MapHeader *rom = gMapGroups[ROGUE_GROUP][mapNum];
    const struct RogueDungeon *d;
    struct MapHeader *h = &sHeaders[FLOOR_SLOT];
    struct MapSlot *slot;
    s32 dungeon;
    u32 floor, room;

    if (gSaveBlock1Ptr->location.mapGroup != ROGUE_GROUP || gSaveBlock1Ptr->location.mapNum != mapNum)
        return rom;
    EnsureRegion();
    if (gRogueTestWarp[3])
    {
        VarSet(VAR_ROGUE_TOWN, gRogueTestWarp[3] - 1);
        gRogueTestWarp[3] = 0;
    }
    dungeon = CurrentDungeon();
    if (dungeon < 0)
        return rom;
    d = &sRegion.dungeons[dungeon];
    slot = EnsureFloor(dungeon, mapNum - MAP_NUM(MAP_ROGUE_FLOOR_1) + 1);
    if (slot == NULL)
        return rom;
    UpdateVillainObjects(slot);
    if (sCurrentPlace != FLOOR_SLOT)
    {
        sPreviousPlace = sCurrentPlace;
        sCurrentPlace = FLOOR_SLOT;
    }
    floor = mapNum - MAP_NUM(MAP_ROGUE_FLOOR_1) + 1;
    room = RogueGen_FloorRoom(&sRegion, dungeon, floor);
    // A room keeps its vanilla tilesets and border.
    if (room != 0xFF)
    {
        sLayouts[FLOOR_SLOT] = *gMapLayouts[sRogueRoomLayouts[room] - 1];
    }
    else
    {
        sLayouts[FLOOR_SLOT] = *gMapLayouts[LAYOUT_ROGUE_FLOOR - 1];
        sLayouts[FLOOR_SLOT].secondaryTileset = &gTileset_Cave;
        sLayouts[FLOOR_SLOT].border = sBorderCave;
        // An ice cave's floors are Shoal Cave's ice room, a volcano's Fiery
        // Path (src/rogue_gen_ice.h, rogue_gen_volcano.h).
        if (d->kind == RDUN_ICE)
            sLayouts[FLOOR_SLOT].border = sBorderIce;
        if (d->kind == RDUN_VOLCANO)
        {
            sLayouts[FLOOR_SLOT].secondaryTileset = &gTileset_Lavaridge;
            sLayouts[FLOOR_SLOT].border = sBorderFiery;
        }
    }
    sLayouts[FLOOR_SLOT].width = slot->map.w;
    sLayouts[FLOOR_SLOT].height = slot->map.h;
    sLayouts[FLOOR_SLOT].map = slot->map.blocks; // (New Mauville's straight from ROM)
    *h = *rom;
    h->mapLayout = &sLayouts[FLOOR_SLOT];
    h->mapLayoutId = LAYOUT_ROGUE_FLOOR;
    h->events = &slot->events;
    h->connections = NULL;
    h->regionMapSectionId = MAPSEC_ROGUE_PLACE_0 + d->host;
    // A legend's chamber has the Sealed Chamber's music; a lair's caves a
    // cave's.
    h->music = sDungeonMusic[d->kind < RDUN_COUNT ? d->kind : 0];
    // Team Rocket's hideout has FireRed's.
    if (d->kind == RDUN_HIDEOUT && RogueGen_TeamId(&sRegion, RogueGen_HideoutTeam(&sRegion, d - sRegion.dungeons)) == ROGUE_VILLAIN_ROCKET
     && RogueGen_HideoutTeam(&sRegion, d - sRegion.dungeons) != 0xFF)
        h->music = MUS_RG_ROCKET_HIDEOUT;
    if (RogueGen_HasChamber(d))
        h->music = room != 0xFF ? MUS_SEALED_CHAMBER : d->kind == RDUN_LAIR ? sDungeonMusic[RDUN_CAVE] : h->music;
    h->mapType = room != 0xFF ? sRogueRoomMapTypes[room] : MAP_TYPE_UNDERGROUND;
    h->weather = WEATHER_NONE;
    // (An ice cave's floors are lit as Shoal Cave's are, a volcano's by its
    // lava; both open late.)
    h->cave = room == 0xFF && sRegion.places[d->host].kind == RPLACE_CAVE && sRegion.places[d->host].dark
           && d->kind != RDUN_ICE && d->kind != RDUN_VOLCANO;
    h->allowEscaping = TRUE;
    h->showMapName = TRUE;
    // The popup names the floor (RogueGen_FloorPopup): B1F down in a cave
    // or a hideout (always entered from the town above it), 1F to 5F up a
    // tower, a lair's chamber by its own name alone.
    {
        char name[ROGUE_NAME_LEN + 16];
        h->floorNumber = RogueGen_FloorPopup(&sRegion, dungeon, floor, name);
    }
    // A shipwreck's locks and a power plant's barriers, set as the floor
    // loads (Rogue_ApplyFloorState).
    if (d->kind == RDUN_SHIP)
        h->mapScripts = Rogue_ShipFloor_MapScripts;
    else if (d->kind == RDUN_PLANT)
        h->mapScripts = Rogue_PlantFloor_MapScripts;
    if (d->kind == RDUN_FOREST && room == 0xFF)
        SetForestHeader(h, &sLayouts[FLOOR_SLOT]);
    // Ruins: their sand pits (the Mirage Tower's cracked floor) give way at
    // a step, and the vault's riddle door is shut until solved.
    if (d->kind == RDUN_RUINS)
        h->mapScripts = Rogue_RuinsFloor_MapScripts;
    // A tower's top is Mt. Pyre's summit, out in the mist.
    if (room != 0xFF && d->kind == RDUN_TOWER && sRogueRoomMapTypes[room] == MAP_TYPE_ROUTE)
    {
        h->weather = WEATHER_FOG_HORIZONTAL;
        h->music = MUS_MT_PYRE_EXTERIOR;
    }
    // (v1.1 peaks) The misty mountainside below a tower's summit: Mt. Pyre's
    // exterior, General and Facility's tilesets, rock beyond its edges.
    if (room == 0xFF && d->kind == RDUN_TOWER)
    {
        sLayouts[FLOOR_SLOT].secondaryTileset = &gTileset_Facility;
        sLayouts[FLOOR_SLOT].border = sBorderRock;
        h->mapType = MAP_TYPE_ROUTE;
        h->weather = WEATHER_FOG_HORIZONTAL;
        h->music = MUS_MT_PYRE_EXTERIOR;
        h->cave = FALSE;
    }
    // (v1.1 peaks) The desert tower's cracked floor gives way, as the ruins'.
    if (d->kind == RDUN_MIRAGE)
        h->mapScripts = Rogue_RuinsFloor_MapScripts;
    return h;
}

// The motto under a dungeon sign's name, in curly quotes, into STR_VAR_2.
static void BufferDungeonMotto(s32 dungeon, bool32 roadThrough)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    u8 *str = StringCopy(gStringVar2, COMPOUND_STRING("“"));
    Rogue_TextCtx(&c, &st, dungeon >= 0 ? sRegion.dungeons[dungeon].host : -1, RSPK_SIGN);
    c.dungeon = dungeon >= 0 ? dungeon : 0xFF;
    c.number = roadThrough;
    if (RogueText_DungeonSign(&c, (char *)str, sizeof(gStringVar2) - 8) == 0)
        StringCopy(str, COMPOUND_STRING("Enter at your own risk."));
    else
        str = Rogue_TextToGame(str);
    StringAppend(gStringVar2, COMPOUND_STRING("”"));
}

// Script special for the sign by a town's tower, hideout or lair: its name
// into STR_VAR_1, and VAR_RESULT TRUE for a hideout, 2 where a legend
// sleeps.
void Rogue_BufferDungeonSign(void)
{
    s32 dungeon;
    EnsureRegion();
    dungeon = RogueGen_DungeonAt(&sRegion, Rogue_GetCurrentTownPlace());
    gSpecialVar_Result = dungeon >= 0 && sRegion.dungeons[dungeon].kind == RDUN_HIDEOUT;
    if (dungeon >= 0 && sRegion.dungeons[dungeon].boss == RBOSS_LEGEND)
    {
        // A lair's carving tells of a legend within, and fresh prints tell
        // the villain leader went in ahead.
        gSpecialVar_Result = 2;
        StringCopy(gStringVar3, ROGUE_GATE_KIND(sRegion.dungeons[dungeon].gate) == RGATE_VILLAIN
                                && Rogue_TeamLeaderReady(ROGUE_GATE_SRC(sRegion.dungeons[dungeon].gate))
                                ? COMPOUND_STRING("\pFresh boot prints lead inside...") : COMPOUND_STRING(""));
    }
    if (dungeon < 0)
        StringCopy(gStringVar1, COMPOUND_STRING("TOWER"));
    else
        Rogue_CopyDungeonName(gStringVar1, dungeon);
    if (gSpecialVar_Result == 2)
    {
        // Words added under the name, where the lore puts its hint on the
        // lair's sign (LORE_WAY_SHRINE, src/rogue_gen_lore.h).
        struct RogueLore lore;
        RogueGen_LegendLore(&sRegion, dungeon, &lore);
        if (lore.hint == LORE_WAY_SHRINE)
        {
            u8 *str = StringCopy(StringCopy(gStringVar4, gStringVar1), COMPOUND_STRING("\p"));
            str = Rogue_BufferLore(str, dungeon, LORE_HINT, LORE_WAY_SHRINE, 600);
            StringCopy(str, gStringVar3);
            gSpecialVar_Result = 3;
        }
    }
    // What lies within, by its kind: a tower's master, a volcano's heat
    // (src/rogue_text_dungeons.h).
    BufferDungeonMotto(dungeon, FALSE);
}

// Script special for the sign by a route's way into the woods: their name
// into STR_VAR_1, and VAR_RESULT TRUE where the road goes on through them.
void Rogue_BufferForestSign(void)
{
    s32 place = Rogue_GetCurrentPlace(), dungeon;
    EnsureRegion();
    dungeon = place < 0 ? -1 : RogueGen_DungeonAt(&sRegion, place);
    gSpecialVar_Result = dungeon >= 0 && sRegion.dungeons[dungeon].exit != 0xFF;
    if (dungeon < 0)
        StringCopy(gStringVar1, COMPOUND_STRING("WOODS"));
    else
        Rogue_CopyDungeonName(gStringVar1, dungeon);
    BufferDungeonMotto(dungeon, gSpecialVar_Result);
}

// Ruins shift with every visit (src/rogue_gen_ruins.h): the visits are
// counted in VAR_ROGUE_SEALS' high bits (a ruins dungeon has no seal of its
// own), so a saved game comes back to the same floors.
u8 RogueGen_DungeonVisit(void)
{
    return VarGet(VAR_ROGUE_SEALS) >> ROGUE_SEALS_VISIT_SHIFT;
}

// Out of a dungeon's floors and back in the open (by the way in, a rope, a
// faint): ruins count the visit, and forget the floors made for it.
static void LeftFloors(void)
{
    s32 d = CurrentDungeon();
    u32 tag, v;
    if (d < 0 || sRegion.dungeons[d].kind != RDUN_RUINS)
        return;
    v = (RogueGen_DungeonVisit() + 1) & 63;
    VarSet(VAR_ROGUE_SEALS, (VarGet(VAR_ROGUE_SEALS) & ((1 << ROGUE_SEALS_VISIT_SHIFT) - 1)) | (v << ROGUE_SEALS_VISIT_SHIFT));
    if (sJobActive && sJobPlace == FLOOR_AHEAD && sWantDungeon == d)
        AbandonJob();
    for (tag = FLOOR_SLOT; tag <= FLOOR_AHEAD; tag++)
        if (sSlotOf[tag] && sSlots[sSlotOf[tag] - 1].map.dungeon == d)
            sSlots[sSlotOf[tag] - 1].map.dungeon = 0xFF;
}

// A fall through a ruins floor's sand pit (warphole, from the Mirage
// Tower's cracked floor) lands on the floor below, where its room says.
bool32 Rogue_SetFallWarp(void)
{
    s32 d;
    u32 floor;
    u8 land;
    if (!InRegion() || !IsFloorMap(gSaveBlock1Ptr->location.mapNum))
        return FALSE;
    EnsureRegion();
    d = CurrentDungeon();
    floor = gSaveBlock1Ptr->location.mapNum - MAP_NUM(MAP_ROGUE_FLOOR_1) + 1;
    // (v1.1 peaks) The desert tower is climbed: a fall lands on the floor
    // below, near its ladder back up.
    if (d >= 0 && sRegion.dungeons[d].kind == RDUN_MIRAGE)
    {
        if (floor < 2 || (land = RogueGen_RuinsLandWarp(&sRegion, d, floor - 1)) == 0xFF)
            return FALSE;
        SetWarpDestination(ROGUE_GROUP, MAP_NUM(MAP_ROGUE_FLOOR_1) + floor - 2, land, -1, -1);
        return TRUE;
    }
    if (d < 0 || floor >= sRegion.dungeons[d].nFloors)
        return FALSE;
    land = RogueGen_RuinsLandWarp(&sRegion, d, floor + 1);
    if (land == 0xFF)
        return FALSE;
    SetWarpDestination(ROGUE_GROUP, MAP_NUM(MAP_ROGUE_FLOOR_1) + floor, land, -1, -1);
    return TRUE;
}

// Script special: the lost explorer leads the player out (their ESCAPE
// ROPE), to the dungeon's way in. Their flag (the reward's) is set.
void Rogue_ExplorerEscape(void)
{
    s32 d = Rogue_GetFloorDungeon();
    const struct RogueDungeon *dn;
    if (d < 0)
        return;
    dn = &sRegion.dungeons[d];
    FlagSet(GetObjectEventFlagIdByLocalIdAndMap(gSpecialVar_LastTalked, gSaveBlock1Ptr->location.mapNum,
                                                gSaveBlock1Ptr->location.mapGroup));
    SetWarpDestination(ROGUE_GROUP, dn->host, sRegion.places[dn->host].kind == RPLACE_CAVE ? RCAVE_DOWN : RWARP_DUNGEON, -1, -1);
    DoWarp();
    ResetInitialPlayerAvatarState();
}

const struct MapHeader *Rogue_GetMapHeader(u16 mapGroup, u16 mapNum)
{
    if (mapGroup != ROGUE_GROUP)
        return NULL;
    if (IsFloorMap(mapNum))
        return GetFloorHeader(mapNum);
    if (mapNum >= ROGUE_MAX_PLACES)
        return Rogue_GetInteriorHeader(mapGroup, mapNum);
    EnsureRegion();
    if (mapNum >= sRegion.nPlaces)
        mapNum = 0;
    // The map the player is in (or is arriving in) is generated now, and
    // becomes the current place. The one they left stays cached, since its
    // people may still be on screen.
    if (gSaveBlock1Ptr->location.mapGroup == mapGroup && gSaveBlock1Ptr->location.mapNum == mapNum)
    {
        if (sCurrentPlace != mapNum)
        {
            if (sCurrentPlace == FLOOR_SLOT)
                LeftFloors();
            sPreviousPlace = sCurrentPlace;
            sCurrentPlace = mapNum;
            // RUN 0: a map that changes on each visit is made for this one
            // (src/rogue_run0_region.c), once the old one is dropped.
            if (Rogue_Run0Entering(mapNum))
                DropPlace(mapNum);
            EnsurePlace(mapNum);
            Rogue_Run0Entered(mapNum);
        }
        UpdateVillainObjects(EnsurePlace(mapNum));
        UpdateCastObjects(EnsurePlace(mapNum));
        // Interiors lead back out to the last outdoor place.
        VarSet(VAR_ROGUE_TOWN, mapNum);
        if (sRegion.places[mapNum].kind == RPLACE_TOWN)
            VarSet(VAR_ROGUE_VISITED, VarGet(VAR_ROGUE_VISITED) | (1 << sRegion.places[mapNum].number));
        FlagSet(FLAG_ROGUE_VISITED_0 + mapNum);
    }
    return &sHeaders[mapNum];
}

const struct MapLayout *Rogue_GetMapLayout(u16 mapLayoutId)
{
    u32 place;
    if (mapLayoutId == LAYOUT_ROGUE_FLOOR)
    {
        if (!IsFloorMap(gSaveBlock1Ptr->location.mapNum) || gSaveBlock1Ptr->location.mapGroup != ROGUE_GROUP)
            return NULL;
        GetFloorHeader(gSaveBlock1Ptr->location.mapNum);
        return &sLayouts[FLOOR_SLOT];
    }
    if (mapLayoutId < LAYOUT_ROGUE_SLOT_0 || mapLayoutId >= LAYOUT_ROGUE_SLOT_0 + ROGUE_MAX_PLACES)
        return NULL;
    EnsureRegion();
    place = mapLayoutId - LAYOUT_ROGUE_SLOT_0;
    if (place >= sRegion.nPlaces)
        place = 0;
    EnsurePlace(place);
    return &sLayouts[place];
}

// How near (in blocks) the player comes to an edge left for later before
// its map is made on the spot. The screen's map buffer reaches about 10
// blocks past the player, and the edge's own blocks lie beyond the edge.
#define LATE_EDGE_BLOCKS 12

void Rogue_NewMapLayout(void)
{
    sLateEdges = 0;
}

// Called by the engine before it copies the edge of connection i of the map
// it is loading. Walking into a map made every map at its edges on the
// spot if it was not ready: a town or a big route at the far side cost 50
// to 110 frames as the player crossed (seeds 10390 and 10394). Since
// v1.1.2 a map whose edge is far from the player is left for later
// (FALSE): the background makes it next, and its edge is copied in once it
// is ready, or made then if the player comes near that edge first.
bool32 Rogue_EnsureMapBlocks(u16 mapGroup, u16 mapNum, u32 i)
{
    if (mapGroup != ROGUE_GROUP || mapNum >= ROGUE_MAX_PLACES)
        return TRUE;
    EnsureRegion();
    if (mapNum < sRegion.nPlaces && !sSlotOf[mapNum] && i < 8 && sCurrentPlace < sRegion.nPlaces
     && gSaveBlock1Ptr->location.mapGroup == ROGUE_GROUP && gSaveBlock1Ptr->location.mapNum == sCurrentPlace
     && EdgeDistance(mapNum) > LATE_EDGE_BLOCKS)
    {
        sLateEdges |= 1 << i;
        // A job making a map not wanted now gives way.
        if (sJobActive && Want(sJobPlace) != 0)
            AbandonJob();
        return FALSE;
    }
    EnsurePlace(mapNum);
    return TRUE;
}

// Copies in the edges left for later whose maps are ready, and makes the
// map of one the player has come near now.
static void FillLateEdges(void)
{
    const struct MapConnection *c;
    u32 i;
    if (gSaveBlock1Ptr->location.mapNum != sCurrentPlace || gMapHeader.connections == NULL)
    {
        sLateEdges = 0;
        return;
    }
    c = gMapHeader.connections->connections;
    for (i = 0; i < (u32)gMapHeader.connections->count; i++, c++)
    {
        if (!(sLateEdges & (1 << i)) || (!sSlotOf[c->mapNum] && EdgeDistance(c->mapNum) > LATE_EDGE_BLOCKS))
            continue;
        EnsurePlace(c->mapNum);
        sLateEdges &= ~(1 << i);
        FillMapConnection(i);
    }
}

u8 *Rogue_CopyAscii(u8 *dest, const u8 *src)
{
    for (; *src; src++)
    {
        if (*src >= 'A' && *src <= 'Z')
            *dest++ = CHAR_A + (*src - 'A');
        else if (*src >= '0' && *src <= '9')
            *dest++ = CHAR_0 + (*src - '0');
        else if (*src == '.')
            *dest++ = CHAR_PERIOD;
        else
            *dest++ = CHAR_SPACE;
    }
    *dest = EOS;
    return dest;
}

// Place names come from the generator (RogueGen_PlaceName in
// src/rogue_gen_names.h), so the host tools see the same ones.
u8 *Rogue_CopyPlaceName(u8 *dest, u32 place)
{
    char name[ROGUE_NAME_LEN + 16];
    EnsureRegion();
    RogueGen_PlaceName(&sRegion, place < sRegion.nPlaces ? place : 0, name);
    return Rogue_CopyLoreAscii(dest, name);
}


bool32 Rogue_GetMapName(u8 *dest, u16 mapSecId)
{
    if (mapSecId >= MAPSEC_ROGUE_PLACE_0 && mapSecId < MAPSEC_ROGUE_PLACE_0 + ROGUE_MAX_PLACES)
    {
        Rogue_CopyPlaceName(dest, mapSecId - MAPSEC_ROGUE_PLACE_0);
        return TRUE;
    }
    return FALSE;
}

u16 Rogue_GetWildSpecies(u32 area, u32 i)
{
    EnsureRegion();
    return sWildSpecies[area % ROGUE_MAX_AREAS][i % WILD_SPECIES_PER_AREA];
}

u32 Rogue_GetGymLeaderIndex(u32 gym)
{
    EnsureRegion();
    return sGymLeaders[gym % ROGUE_NUM_GYMS];
}

u32 Rogue_GetE4Index(u32 member)
{
    EnsureRegion();
    return sE4[member % ROGUE_NUM_E4];
}

// Overworld sprites in a gym: 0 the leader, 1 to 3 the gym trainers.
u16 Rogue_GetGymGfx(u32 gym, u32 which)
{
    EnsureRegion();
    gym %= ROGUE_NUM_GYMS;
    if (which == 0)
        return sLeaders[sGymLeaders[gym]].gfx;
    return TrainerClass(ROGUE_SLOT_GYM_0 + gym * ROGUE_GYM_TRAINERS + which - 1)->gfx;
}

u32 Rogue_GetGymTrainerCount(u32 gym)
{
    return sGymTrainers[gym % ROGUE_NUM_GYMS];
}

const u8 *Rogue_GetGymLeaderName(u32 gym)
{
    EnsureRegion();
    return sLeaders[sGymLeaders[gym % ROGUE_NUM_GYMS]].name;
}

u32 Rogue_GetGymLeaderType(u32 gym)
{
    EnsureRegion();
    return sLeaders[sGymLeaders[gym % ROGUE_NUM_GYMS]].type;
}

const u8 *Rogue_GetLeaderName(u32 leader)
{
    return sLeaders[leader % ARRAY_COUNT(sLeaders)].name;
}

u32 Rogue_GetLeaderType(u32 leader)
{
    return sLeaders[leader % ARRAY_COUNT(sLeaders)].type;
}

u16 Rogue_GetLeaderGfx(u32 leader)
{
    return sLeaders[leader % ARRAY_COUNT(sLeaders)].gfx;
}

// ---------------------------------------------------------------------------
// Wild encounters

// A dungeon's own area (a tower's, a grove's) stands as its first floor
// (AreaPlaceOf), so its water is at its level, not the first town's.
static u32 AreaTier(s32 area)
{
    const struct RoguePlace *p = AreaPlaceOf(area);
    return p->tier < NUM_TIERS ? p->tier : NUM_TIERS - 1;
}

// Wild Pokemon run under the place's trainers, as in Emerald
// (RogueGen_WildLevel).
static void Run0WildTable(struct WildPokemon *mons, u32 n); // src/rogue_run0_trainers.h

static u32 AreaWildLevel(s32 area)
{
    u32 level = AreaPlaceOf(area)->level;
    // RUN 0: the band's (src/rogue_run0_trainers.h).
    return Rogue_Run0WildLevel(level, RogueGen_WildLevel(level));
}

// ---------------------------------------------------------------------------
// Rare finds

// Pokemon the real games keep rare: a Safari Zone's Chansey, Mt. Pyre's
// Absol, an Eevee in a back corner. Dragons only where Dratini's line may
// appear (LineFitsLevel).
static const u16 sRareFinds[] =
{
    SPECIES_EEVEE, SPECIES_TOGEPI, SPECIES_CHANSEY, SPECIES_LARVESTA, SPECIES_ABSOL,
    SPECIES_MUNCHLAX, SPECIES_RIOLU, SPECIES_SCYTHER, SPECIES_DRATINI, SPECIES_BAGON,
};

// TRUE if a place may hold the rare find: a route or cave off the main road,
// at the deep end of a side path if there is one.
static bool32 RareFindPlace(u32 i, bool32 deep)
{
    const struct RoguePlace *p = &sRegion.places[i];
    if (!p->side || p->kind == RPLACE_TOWN || p->area == 0xFF)
        return FALSE;
    return !deep || (p->branch == RBRANCH_SIDE && (p->kind == RPLACE_CAVE || p->nLinks == 1));
}

// The side area with the rare find. Returns its area, or -1.
static s32 RareFindArea(void)
{
    u32 i, n = 0, pick;
    bool32 deep = TRUE;
    for (i = 0; i < sRegion.nPlaces; i++)
        n += RareFindPlace(i, deep);
    if (n == 0)
    {
        deep = FALSE;
        for (i = 0; i < sRegion.nPlaces; i++)
            n += RareFindPlace(i, deep);
    }
    if (n == 0)
        return -1;
    pick = RogueRng_Hash(sRegion.seed, 9410) % n;
    for (i = 0; i < sRegion.nPlaces; i++)
        if (RareFindPlace(i, deep) && pick-- == 0)
            return sRegion.places[i].area;
    return -1;
}

static u16 RareFindSpecies(s32 area)
{
    const struct RoguePlace *p = &sRegion.places[AreaPlace(area)];
    u32 rng = RogueRng_Hash(sRegion.seed, 9420), tries;
    u16 species = SPECIES_EEVEE;
    for (tries = 0; tries < 20; tries++)
    {
        species = sRareFinds[RogueRng_Range(&rng, ARRAY_COUNT(sRareFinds))];
        if (LineFitsLevel(species, p->level) && !InList(sWildSpecies[area], WILD_SPECIES_PER_AREA, species))
            break;
    }
    return species;
}

// RARE FINDS: the rare find's species and area, or SPECIES_NONE when off.
static u16 RareFind(s32 *area)
{
    *area = -1;
    if (!Rogue_IsUnlockOn(UNLOCK_RARE_FINDS))
        return SPECIES_NONE;
    *area = RareFindArea();
    return *area < 0 ? SPECIES_NONE : RareFindSpecies(*area);
}

// Script special: the professor's hint about the rare find, in STR_VAR_1.
// VAR_RESULT is FALSE when there is none.
void Rogue_BufferRareFindHint(void)
{
    s32 area;
    u8 *str;

    EnsureRegion();
    gSpecialVar_Result = FALSE;
    if (RareFind(&area) == SPECIES_NONE)
        return;
    str = StringCopy(gStringVar1, COMPOUND_STRING("People say a rare POKéMON lives\noff the main road, at "));
    str = Rogue_CopyPlaceName(str, AreaPlace(area));
    StringCopy(str, COMPOUND_STRING("."));
    gSpecialVar_Result = TRUE;
}

// TRUE if a species can be met at a place: in its grass or cave, or on its
// water (fishing and surfing) for the routes and towns that have water.
bool32 Rogue_PlaceHasWildSpecies(u32 place, u16 species)
{
    const struct RoguePlace *p;
    u32 i, late;
    bool32 water;

    EnsureRegion();
    if (place >= sRegion.nPlaces || sRegion.places[place].area == 0xFF || species == SPECIES_NONE)
        return FALSE;
    p = &sRegion.places[place];
    if (p->kind != RPLACE_TOWN)
    {
        s32 rareArea;
        for (i = 0; i < WILD_SPECIES_PER_AREA; i++)
            if (sWildSpecies[p->area][i] == species)
                return TRUE;
        if (RareFind(&rareArea) == species && rareArea == p->area)
            return TRUE;
        if (RogueGen_IslandRare(&sRegion, place) == species)
            return TRUE;
    }
    water = (p->kind == RPLACE_ROUTE && (p->type == RROUTE_LAKE || p->type == RROUTE_RIVER
                                       || p->type == RROUTE_COAST || p->type == RROUTE_SEA))
         || (p->kind == RPLACE_TOWN && p->layout == RLAYOUT_HARBOR);
    if (!water)
        return FALSE;
    late = AreaTier(p->area) >= 5;
    for (i = 0; i < SURF_SPECIES; i++)
        if (sSurfSpecies[late][i] == species)
            return TRUE;
    for (i = 0; i < FISH_SPECIES; i++)
        if (sFishSpecies[late][i] == species)
            return TRUE;
    return FALSE;
}

// Emerald's caves' land encounter rate (Granite Cave, Victory Road).
#define CAVE_WILD_RATE 10

const struct WildPokemonInfo *Rogue_GetLandMonsInfo(const struct WildPokemonInfo *original)
{
    // Slot to species, following the vanilla 20/20/10/10/10/10/5/5/4/4/1/1 odds.
    // Land slots are 20, 20, 10, 10, 10, 10, 5, 5, 4, 4, 1, 1 percent. Spread
    // the species over them so no one Pokemon fills the route.
    static const u8 slotSpecies[12] = { 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5 };
    s32 area = Rogue_GetAreaId();
    u32 i, level, spread;

    // Dungeon floors have tables of their own (src/rogue_dungeon.c), never
    // the slot map's placeholder or their town's. A floor with none (a
    // legend's chamber) meets nothing on foot (Rogue_GetLandEncounterRate);
    // Sweet Scent there finds the floor above's.
    if (IsFloorMap(gSaveBlock1Ptr->location.mapNum) && CurrentDungeon() >= 0)
    {
        u32 floor = gSaveBlock1Ptr->location.mapNum - MAP_NUM(MAP_ROGUE_FLOOR_1) + 1;
        sWildInfo.encounterRate = RogueDungeon_FloorWild(CurrentDungeon(), floor, sWildMons);
        if (sWildInfo.encounterRate != 0
         || (floor > 1 && RogueDungeon_FloorWild(CurrentDungeon(), floor - 1, sWildMons) != 0))
        {
            sWildInfo.wildPokemon = sWildMons;
            return &sWildInfo;
        }
    }
    if (area < 0 || sRegion.places[AreaPlace(area)].kind == RPLACE_TOWN)
        return original;
    level = AreaWildLevel(area);
    // The first routes keep close together: Route 101 is levels 2 and 3.
    spread = level < 5 ? 1 : 2;
    for (i = 0; i < 12; i++)
    {
        sWildMons[i].species = sWildSpecies[area][slotSpecies[i]];
        sWildMons[i].minLevel = level + (i % 2) + RogueGen_RareSlotBonus(i, level);
        sWildMons[i].maxLevel = level + spread + (i % 2) + RogueGen_RareSlotBonus(i, level);
    }
    {
        // The rare find takes the two 1% slots.
        s32 rareArea;
        u16 rare = RareFind(&rareArea);
        if (rare != SPECIES_NONE && rareArea == area)
            sWildMons[10].species = sWildMons[11].species = rare;
    }
    // v1.1 sea: an island's rare POKeMON takes a 10% slot.
    if (RogueGen_IslandRare(&sRegion, AreaPlace(area)) != SPECIES_NONE)
        sWildMons[5].species = RogueGen_IslandRare(&sRegion, AreaPlace(area));
    if (Rogue_Run0Scaled())
        Run0WildTable(sWildMons, 12);
    sWildInfo.encounterRate = sRegion.places[AreaPlace(area)].kind == RPLACE_CAVE ? CAVE_WILD_RATE : 20;
    sWildInfo.wildPokemon = sWildMons;
    return &sWildInfo;
}

// The chance of meeting a Pokemon on a land step, for the map's own rate
// romRate (the slot maps' 20, Emerald's routes'): a cave meets them half as
// often, as Emerald's caves (10) do, and a dungeon floor's kind sets its own
// (none in a hideout or a legend's chamber).
u32 Rogue_GetLandEncounterRate(u32 romRate)
{
    s32 d;
    u32 mapNum = gSaveBlock1Ptr->location.mapNum;
    if (!InRegion())
        return romRate;
    EnsureRegion();
    if (!IsFloorMap(mapNum))
        return (mapNum < sRegion.nPlaces && sRegion.places[mapNum].kind == RPLACE_CAVE) ? CAVE_WILD_RATE : romRate;
    d = CurrentDungeon();
    return d >= 0 ? RogueDungeon_WildRate(d, gSaveBlock1Ptr->location.mapNum - MAP_NUM(MAP_ROGUE_FLOOR_1) + 1) : romRate;
}

const struct WildPokemonInfo *Rogue_GetWaterMonsInfo(const struct WildPokemonInfo *original)
{
    static const u8 slotSpecies[5] = { 0, 1, 2, 0, 3 };
    s32 area = Rogue_GetAreaId();
    u32 i, tier, late;

    if (area < 0)
        return original;
    tier = AreaTier(area);
    late = tier >= 5;
    for (i = 0; i < 5; i++)
    {
        sWildMons[i].species = sSurfSpecies[late][slotSpecies[i]];
        sWildMons[i].minLevel = AreaWildLevel(area);
        sWildMons[i].maxLevel = AreaWildLevel(area) + 4;
    }
    if (Rogue_Run0Scaled())
        Run0WildTable(sWildMons, 5);
    sWildInfo.encounterRate = 4;
    sWildInfo.wildPokemon = sWildMons;
    return &sWildInfo;
}

const struct WildPokemonInfo *Rogue_GetFishingMonsInfo(const struct WildPokemonInfo *original)
{
    // The ten fishing slots are two for the Old Rod, three for the Good Rod
    // and five for the Super Rod.
    static const u8 slotSpecies[10] = { 0, 1, 0, 1, 2, 1, 2, 2, 0, 1 };
    s32 area = Rogue_GetAreaId();
    u32 i, level, tier;

    if (area < 0)
        return original;
    tier = AreaTier(area);
    level = AreaWildLevel(area);
    for (i = 0; i < 10; i++)
    {
        sWildMons[i].species = sFishSpecies[tier >= 5][slotSpecies[i]];
        sWildMons[i].minLevel = level;
        sWildMons[i].maxLevel = level + 3;
    }
    if (Rogue_Run0Scaled())
        Run0WildTable(sWildMons, 10);
    sWildInfo.encounterRate = 30;
    sWildInfo.wildPokemon = sWildMons;
    return &sWildInfo;
}

// ---------------------------------------------------------------------------
// Trainers

static s32 TrainerSlot(u16 trainerId)
{
    if (trainerId < ROGUE_TRAINER_FIRST || trainerId >= ROGUE_TRAINER_FIRST + ROGUE_TRAINER_SLOTS)
        return -1;
    return trainerId - ROGUE_TRAINER_FIRST;
}

bool32 Rogue_IsRogueTrainer(u16 trainerId)
{
    // The slots only stand in for generated trainers inside a generated region.
    if (!InRegion())
        return FALSE;
    return TrainerSlot(trainerId) >= 0;
}

// TRUE if an earlier party slot already has this species.
static bool32 PartyHasSpecies(u32 n, u16 species)
{
    u32 i;
    for (i = 0; i < n; i++)
        if (sTrainerParty[i].species == species)
            return TRUE;
    return FALSE;
}

// TRUE if slot n shares its first type with an earlier slot.
// The CHAMPION's team so far has two of party member n's first type.
static bool32 ChampionTypeCrowded(u32 n)
{
    u32 i, same = 0;
    enum Type t = GetSpeciesType(sTrainerParty[n].species, 0);
    for (i = 0; i < n; i++)
        same += GetSpeciesType(sTrainerParty[i].species, 0) == t;
    return same >= 2;
}

// Early trainers: the last four level-up moves, but no sleep, no accuracy or
// evasion tricks, one status move at most, and nothing too strong for the level (modern learnsets
// give some species Fire Fang and the like at level 1). In a Nuzlocke one
// long sleep or a crit from a strong move ends a young run.
static bool32 EarlyMoveOk(enum Move move, u32 level)
{
    switch (move)
    {
    case MOVE_SAND_ATTACK:
    case MOVE_SMOKESCREEN:
    case MOVE_FLASH:
    case MOVE_KINESIS:
    case MOVE_DOUBLE_TEAM:
    case MOVE_MINIMIZE:
    case MOVE_PROTECT:
    case MOVE_DETECT:
        return FALSE;
    default:
        break;
    }
    if (GetMoveNonVolatileStatus(move) == MOVE_EFFECT_SLEEP)
        return FALSE;
    // No setting up: Growth five times in a row sweeps a young team.
    if (GetMoveCategory(move) == DAMAGE_CATEGORY_STATUS && GetMoveTarget(move) == TARGET_USER)
        return FALSE;
    // Basic attacks like Tackle (40) are fine at any level.
    if (GetMoveCategory(move) != DAMAGE_CATEGORY_STATUS && GetMovePower(move) > max(40, level * 3 + 30))
        return FALSE;
    return TRUE;
}

// Abilities that set weather or terrain on the field, which early on turn
// a fair fight into a lost one.
static bool32 FieldAbility(enum Ability ability)
{
    switch (ability)
    {
    case ABILITY_DRIZZLE:
    case ABILITY_DROUGHT:
    case ABILITY_SAND_STREAM:
    case ABILITY_SNOW_WARNING:
    case ABILITY_SAND_SPIT:
    case ABILITY_SEED_SOWER:
    case ABILITY_ELECTRIC_SURGE:
    case ABILITY_PSYCHIC_SURGE:
    case ABILITY_MISTY_SURGE:
    case ABILITY_GRASSY_SURGE:
        return TRUE;
    default:
        return FALSE;
    }
}

// Abilities that double a young Pokemon's damage: Dewpider's Water Bubble
// turns Bubble Beam into a one-hit knockout at the first gym.
static bool32 DoublingAbility(enum Ability ability)
{
    switch (ability)
    {
    case ABILITY_WATER_BUBBLE:
    case ABILITY_HUGE_POWER:
    case ABILITY_PURE_POWER:
    case ABILITY_ADAPTABILITY:
        return TRUE;
    default:
        return FALSE;
    }
}

// An early trainer's Pokemon gets the first ability it has that is not one
// of those, when it has one.
static void SetEarlyAbility(struct TrainerMon *mon)
{
    u32 i;
    for (i = 0; i < NUM_ABILITY_SLOTS; i++)
    {
        enum Ability ability = GetSpeciesAbility(mon->species, i);
        // (gRogueTestLeader bit 8 keeps the old rule, for leaderbench.)
        if (ability != ABILITY_NONE && !FieldAbility(ability)
         && !(DoublingAbility(ability) && !(gRogueTestLeader[0] && (gRogueTestLeader[3] & 8))))
        {
            mon->ability = ability;
            return;
        }
    }
}

// An attack stronger than Tackle or Water Gun.
static bool32 StrongAttack(enum Move move)
{
    return move != MOVE_NONE && GetMoveCategory(move) != DAMAGE_CATEGORY_STATUS && GetMovePower(move) > 40;
}

// The first leader's Pokemon as Roxanne's are: one attack of the gym's type
// up to Rock Tomb's 60 power, the rest Tackle and Harden. A strong attack of
// another type (Dewpider's Bug Bite) would hit both starters that beat the
// gym's type.
#define FIRST_GYM_MAX_POWER 60

static bool32 FirstGymAttackOk(enum Move move, enum Type gymType)
{
    return GetMoveType(move) == gymType && GetMovePower(move) <= FIRST_GYM_MAX_POWER;
}

static void DropMoveSlot(struct TrainerMon *mon, u32 slot, u32 *n)
{
    u32 j;
    for (j = slot; j + 1 < *n; j++)
        mon->moves[j] = mon->moves[j + 1];
    mon->moves[--*n] = MOVE_NONE;
}

// gymType is the first leader's type for their team (TYPE_NONE otherwise).
static void SetEarlyMoves(struct TrainerMon *mon, u32 level, enum Type gymType)
{
    const struct LevelUpMove *learnset = GetSpeciesLevelUpLearnset(mon->species);
    u32 i, j, n = 0;
    bool32 attack = FALSE;

    for (i = 0; i < MAX_MON_MOVES; i++)
        mon->moves[i] = MOVE_NONE;
    for (i = 0; learnset[i].move != LEVEL_UP_MOVE_END && learnset[i].level <= level; i++)
    {
        enum Move move = learnset[i].move;
        bool32 dup = FALSE;
        if (!EarlyMoveOk(move, level))
            continue;
        for (j = 0; j < n; j++)
            if (mon->moves[j] == move)
                dup = TRUE;
        if (dup)
            continue;
        if (gymType != TYPE_NONE && StrongAttack(move))
        {
            if (!FirstGymAttackOk(move, gymType))
                continue;
            // The newest strong attack replaces the one before.
            for (j = 0; j < n; j++)
            {
                if (StrongAttack(mon->moves[j]))
                {
                    DropMoveSlot(mon, j, &n);
                    break;
                }
            }
        }
        // One status move at most, so a leader's ace cannot set up for five
        // turns and sweep.
        if (GetMoveCategory(move) == DAMAGE_CATEGORY_STATUS)
        {
            bool32 hasStatus = FALSE;
            for (j = 0; j < n; j++)
                if (GetMoveCategory(mon->moves[j]) == DAMAGE_CATEGORY_STATUS)
                    hasStatus = TRUE;
            if (hasStatus)
                continue;
        }
        if (n == MAX_MON_MOVES)
        {
            for (j = 0; j < MAX_MON_MOVES - 1; j++)
                mon->moves[j] = mon->moves[j + 1];
            n--;
        }
        mon->moves[n++] = move;
    }
    for (i = 0; i < n; i++)
        if (GetMoveCategory(mon->moves[i]) != DAMAGE_CATEGORY_STATUS)
            attack = TRUE;
    // Something to hit with, so no battle is a stall.
    if (!attack)
    {
        if (n == MAX_MON_MOVES)
            n--;
        mon->moves[n++] = MOVE_TACKLE;
    }
}

static void SetPartyMon(u32 i, u16 species, u32 level, u32 iv, bool32 anyMethod)
{
    struct TrainerMon *mon = &sTrainerParty[i];
    memset(mon, 0, sizeof(*mon));
    mon->species = EvolveForLevel(species, level, anyMethod);
    mon->lvl = level;
    mon->iv = TRAINER_PARTY_IVS(iv, iv, iv, iv, iv, iv);
    mon->ball = BALL_POKE;
    mon->gender = TRAINER_MON_RANDOM_GENDER;
    if (level < 20)
    {
        SetEarlyMoves(mon, level, TYPE_NONE);
        SetEarlyAbility(mon);
    }
}

static const u8 sText_May[] = _("MAY");
static const u8 sText_Brendan[] = _("BRENDAN");

// Does an attacking type hit a species hard, both its types counted.
static bool32 TypeBeatsSpecies(enum Type atk, u16 species)
{
    enum Type t1 = GetSpeciesType(species, 0), t2 = GetSpeciesType(species, 1);
    u32 mod = GetTypeModifier(atk, t1);
    if (t2 != t1)
        mod = mod * GetTypeModifier(atk, t2) / UQ_4_12(1.0);
    return mod >= UQ_4_12(2.0);
}

// Against a starter from another row, the rival takes a first form whose
// type beats it, as the base starters beat each other: one of its types
// hits the player's starter hard, and the starter's own first type does not
// hit it hard. From the starter rows' pools, the lines nearest the
// starter's in strength (final forms within 20 base stats of the nearest),
// by the region's roll: a baby meets a partner's equal, a legend a
// pseudo-legend.
static u16 RivalCounter(u16 player, u32 *rng, u32 maxBst)
{
    static const u16 *const pools[] = { sStarters[0], sStarters[1], sStarters[2], sPartnerStarters, sRisingStarters,
                                        sDragonStarters, sFossilStarters, sRegionalStarters };
    static const u8 sizes[] = { 9, 9, 9, ARRAY_COUNT(sPartnerStarters), ARRAY_COUNT(sRisingStarters),
                                ARRAY_COUNT(sDragonStarters), ARRAY_COUNT(sFossilStarters), ARRAY_COUNT(sRegionalStarters) };
    u32 pass, p, i, nearest = 0xFFFF, n = 0, pick = 0, target = LineMaxBst(player);
    enum Type own = GetSpeciesType(player, 0);

    for (pass = 0; pass < 3; pass++)
    {
        for (p = 0; p < ARRAY_COUNT(pools); p++)
        {
            for (i = 0; i < sizes[p]; i++)
            {
                u16 c = pools[p][i];
                u32 bst, gap;
                if (!IsSpeciesEnabled(c) || BaseForm(c) == BaseForm(player) || TypeBeatsSpecies(own, c))
                    continue;
                if (!TypeBeatsSpecies(GetSpeciesType(c, 0), player) && !TypeBeatsSpecies(GetSpeciesType(c, 1), player))
                    continue;
                bst = LineMaxBst(c);
                if (bst > maxBst)
                    continue;
                gap = bst > target ? bst - target : target - bst;
                if (pass == 0 && gap < nearest)
                    nearest = gap;
                else if (pass == 1 && gap <= nearest + 20)
                    n++;
                else if (pass == 2 && gap <= nearest + 20 && pick-- == 0)
                    return c;
            }
        }
        if (pass == 0 && nearest == 0xFFFF)
            break;
        if (pass == 1)
            pick = RogueRng_Range(rng, n);
    }
    return sStarterBase[RogueRng_Range(rng, 3)];
}

// The first rival battle, as in the real games: the region's Grass, Fire
// or Water starter whose type beats the player's (one of the region's
// three, by the region's roll), or else a counter no stronger than a
// starter's line.
static u16 FirstRivalCounter(u16 player, u32 *rng)
{
    enum Type own = GetSpeciesType(player, 0);
    u32 i, n = 0, pick, best = 0;
    for (i = 0; i < 3; i++)
    {
        u16 c = sStarterBase[i];
        best = max(best, LineMaxBst(c));
        if (!TypeBeatsSpecies(own, c) && (TypeBeatsSpecies(GetSpeciesType(c, 0), player) || TypeBeatsSpecies(GetSpeciesType(c, 1), player)))
            n++;
    }
    if (n == 0)
        return RivalCounter(player, rng, best);
    pick = RogueRng_Range(rng, n);
    for (i = 0; i < 3; i++)
    {
        u16 c = sStarterBase[i];
        if (!TypeBeatsSpecies(own, c) && (TypeBeatsSpecies(GetSpeciesType(c, 0), player) || TypeBeatsSpecies(GetSpeciesType(c, 1), player))
         && pick-- == 0)
            return c;
    }
    return sStarterBase[0];
}

// The rival's partner, chosen once for the run and kept for all four
// battles: the base starter that beats yours, Fire against Grass, Water
// against Fire, Grass against Water (whichever Grass starter you took).
// Against a starter from another row, the region's base starter whose type
// beats it (FirstRivalCounter). A run started before v0.13 with another
// row: one at random, as then.
static u16 RivalStarter(void)
{
    u32 value = VarGet(VAR_ROGUE_STARTER);
    u32 rng = RogueRng_Hash(sRegion.seed, 77);
    u32 kind = (value & ROGUE_STARTER_NEW) ? (value >> ROGUE_STARTER_KIND_SHIFT) & 0xF : value;
    if (kind <= ROGUE_STARTER_WATER)
        return sStarterBase[(kind + 1) % 3];
    if (!(value & ROGUE_STARTER_NEW))
        return sStarterBase[RogueRng_Range(&rng, 3)];
    return FirstRivalCounter(value & ROGUE_STARTER_SPECIES_MASK, &rng);
}

// At the foot of VICTORY ROAD, against a starter from another row, the
// rival also brings a counter of about that starter's strength
// (RivalCounter), so a strong starter meets a strong answer. SPECIES_NONE
// for a classic pick or when it would be the partner's line.
static u16 RivalSecondCounter(void)
{
    u32 value = VarGet(VAR_ROGUE_STARTER);
    u32 rng = RogueRng_Hash(sRegion.seed, 78);
    u32 kind = (value >> ROGUE_STARTER_KIND_SHIFT) & 0xF;
    u16 counter;
    if (!(value & ROGUE_STARTER_NEW) || kind <= ROGUE_STARTER_WATER)
        return SPECIES_NONE;
    counter = RivalCounter(value & ROGUE_STARTER_SPECIES_MASK, &rng, 0xFFFF);
    return BaseForm(counter) == BaseForm(RivalStarter()) ? SPECIES_NONE : counter;
}

u16 Rogue_GetRivalStarter(void)
{
    EnsureRegion();
    return RivalStarter();
}

// A species from the wild area of the place with the given tier, or close.
static u16 WildOfTier(u32 *rng, u32 tier)
{
    u32 i, best = 0;
    for (i = 0; i < sRegion.nPlaces; i++)
    {
        const struct RoguePlace *p = &sRegion.places[i];
        if (p->kind == RPLACE_ROUTE && !p->side && p->area != 0xFF && p->tier <= tier)
            best = p->area;
    }
    return sWildSpecies[best][RogueRng_Range(rng, WILD_SPECIES_PER_AREA)];
}

// Sizes of the rival's four battles, as in Emerald: the first is the
// starter alone. Levels are in the region's curve.
static const u8 sRivalSizes[4] = { 1, 3, 4, 5 };

// The rival's partner's gender, the same at every battle of a run: drawn
// once from the region's seed, as its species' ratio allows.
static u8 RivalPartnerGender(u16 species)
{
    u8 ratio = gSpeciesInfo[species].genderRatio;
    if (ratio == MON_GENDERLESS)
        return TRAINER_MON_RANDOM_GENDER;
    if (ratio == MON_MALE)
        return TRAINER_MON_MALE;
    if (ratio == MON_FEMALE)
        return TRAINER_MON_FEMALE;
    return RogueRng_Hash(sRegion.seed, 41900) % MON_FEMALE < ratio ? TRAINER_MON_FEMALE : TRAINER_MON_MALE;
}

// How strong a route or gym trainer's Pokemon may be, by level, as base
// stat totals of the evolved form. Around Emerald's: a median of about 240
// before the first gym, 305 by the third, 330 midgame and 405 before the
// League.
static void TrainerBstBand(u32 level, u32 *lo, u32 *hi)
{
    if (level <= 10)      { *lo = 180; *hi = 300; }
    else if (level <= 20) { *lo = 200; *hi = 385; }
    else if (level <= 30) { *lo = 230; *hi = 420; }
    else if (level <= 38) { *lo = 250; *hi = 460; }
    else                  { *lo = 300; *hi = 500; }
}

// A first stage for a trainer whose evolved form at this level falls in the
// band, and not one of the lines that are rare in the real games until late.
static u16 PickTrainerSpecies(u32 *rng, enum Type type, u32 level, u32 lo, u32 hi, u32 maxBst, bool32 anyMethod)
{
    u16 species = SPECIES_NONE;
    u32 tries;
    for (tries = 0; tries < 40; tries++)
    {
        u32 bst;
        if (sRegionMix == ROGUE_MIX_LEGACY)
        {
            species = PickSpeciesLegacy(rng, type, 0, maxBst < hi ? maxBst : hi);
        }
        else
        {
            struct PickRules r = {0};
            r.type = type;
            r.maxBst = maxBst < hi ? maxBst : hi;
            r.evoLo = lo;
            r.evoHi = hi;
            r.level = level;
            r.anyMethod = anyMethod;
            species = PickByRules(rng, &r);
        }
        bst = SpeciesBst(EvolveForLevel(species, level, anyMethod));
        if (tries < 30 && (bst < lo || bst > hi))
            continue;
        if (tries < 36 && !LineFitsLevel(species, level))
            continue;
        break;
    }
    return species;
}

// A leader's or Elite Four member's team: type specialists, the ace last.
// The ace evolves by stone, trade or friendship too once it is old enough
// (Sinistea at 27 is Polteageist, Skitty at 32 Delcatty), as no leader's ace
// in the games is a Pokemon that stopped short of its last form.
// A leader's signature ace (Misty's Starmie, Lance's Dragonite) at this
// level: the furthest stage of its line the level reaches by the builder's
// rules, or none if its line is too strong for the gym (a little over the
// gym's limit is fine: Brock's Onix is Roxanne's Nosepass).
static u16 SignatureAce(const struct LeaderDef *leader, u32 level, u32 maxBst)
{
    u16 species = leader->ace[0];
    u32 stage, i;
    if (species == SPECIES_NONE || !IsSpeciesEnabled(species) || SpeciesBst(species) > maxBst + 40
     || !LineFitsLevel(species, level))
        return SPECIES_NONE;
    for (stage = 0; stage + 1 < ARRAY_COUNT(leader->ace) && leader->ace[stage + 1] != SPECIES_NONE; stage++)
    {
        const struct Evolution *evos = GetSpeciesEvolutions(species);
        bool32 evolved = FALSE;
        for (i = 0; evos != NULL && evos[i].method != EVOLUTIONS_END; i++)
        {
            if (evos[i].targetSpecies == leader->ace[stage + 1] && EvoReached(&evos[i], level, TRUE, stage))
            {
                species = evos[i].targetSpecies;
                evolved = TRUE;
                break;
            }
        }
        if (!evolved)
            break;
    }
    return species;
}

// Now and then one of the leader's favourites (Koga's Grimer, Sabrina's
// Mr. Mime) instead of a random one of the type, when it fits the gym and
// is of the type at this level.
static u16 LeaderFavorite(const struct LeaderDef *leader, u32 *rng, u32 slot, u32 level, u32 maxBst, bool32 anyMethod)
{
    u16 fits[ARRAY_COUNT(leader->team)];
    u32 i, n = 0;
    for (i = 0; i < ARRAY_COUNT(leader->team) && leader->team[i] != SPECIES_NONE; i++)
    {
        u16 species = leader->team[i], evolved;
        if (!IsSpeciesEnabled(species) || SpeciesBst(species) > maxBst || !LineFitsLevel(species, level))
            continue;
        evolved = EvolveForLevel(species, level, anyMethod);
        if ((IsBaby(species) && evolved == species) || !SpeciesHasType(evolved, leader->type)
         || PartyHasSpecies(slot, evolved))
            continue;
        fits[n++] = species;
    }
    if (n == 0 || RogueRng_Range(rng, 2) != 0)
        return SPECIES_NONE;
    return fits[RogueRng_Range(rng, n)];
}

static void BuildLeaderTeam(const struct LeaderDef *leader, u32 *rng, u32 size, u32 ace, u32 maxBst, bool32 anyMethod, u32 iv)
{
    u32 i;
    for (i = 0; i < size; i++)
    {
        // The ace stands out, most of all in the early gyms: Roxanne's
        // team is 12, 12 and 15.
        u32 level = (i == size - 1) ? ace : ace - (ace < 30 ? 3 : 2) - (size - 2 - i) / 2;
        u32 tries = 0;
        u16 species = (i == size - 1) ? SignatureAce(leader, level, maxBst)
                                      : LeaderFavorite(leader, rng, i, level, maxBst, anyMethod);
        if (species != SPECIES_NONE && !PartyHasSpecies(i, species))
        {
            // The ace's stage is already chosen: no further evolution.
            SetPartyMon(i, species, level, iv, i != size - 1 && anyMethod);
            continue;
        }
        do
        {
            SetPartyMon(i, PickSpecies(rng, leader->type, 250, maxBst), level, iv, anyMethod || i == size - 1);
        } while (++tries < 30 && PartyHasSpecies(i, sTrainerParty[i].species));
    }
}

// The first leader's team: Roxanne's moves (see FirstGymAttackOk), and the
// ace knows the TM the badge brings when it is such an attack, as Nosepass
// knows Rock Tomb.
static void TameFirstLeader(u32 size, enum Type type, u16 tm)
{
    enum Move tmMove = GetItemTMHMMoveId(tm);
    struct TrainerMon *ace = &sTrainerParty[size - 1];
    u32 i, slot = MAX_MON_MOVES;

    for (i = 0; i < size; i++)
        if (sTrainerParty[i].lvl < 20)
            SetEarlyMoves(&sTrainerParty[i], sTrainerParty[i].lvl, type);
    if (tmMove == MOVE_NONE || !StrongAttack(tmMove) || !FirstGymAttackOk(tmMove, type)
     || !CanLearnTeachableMove(ace->species, tmMove))
        return;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (ace->moves[i] == tmMove)
            return;
        if (slot == MAX_MON_MOVES && (ace->moves[i] == MOVE_NONE || StrongAttack(ace->moves[i])))
            slot = i;
    }
    // Over its strong attack or into a free slot, else over its weakest
    // attack.
    if (slot == MAX_MON_MOVES)
    {
        for (i = 0; i < MAX_MON_MOVES; i++)
            if (GetMoveCategory(ace->moves[i]) != DAMAGE_CATEGORY_STATUS
             && (slot == MAX_MON_MOVES || GetMovePower(ace->moves[i]) < GetMovePower(ace->moves[slot])))
                slot = i;
        if (slot == MAX_MON_MOVES)
            slot = MAX_MON_MOVES - 1;
    }
    ace->moves[slot] = tmMove;
}

// Badges won so far, as the step of the next one: 0 for the first gym
// fought, up to 7.
static u32 BadgeStep(void)
{
    u32 n = Rogue_BadgeCount();
    return n < ROGUE_NUM_GYMS ? n : ROGUE_NUM_GYMS - 1;
}

// Script special, after a badge: progress counts the badges won, and the
// gyms' trainers, built for the badges held, are built again.
void Rogue_UpdateProgress(void)
{
    VarSet(VAR_ROGUE_PROGRESS, ROGUE_PROGRESS_STARTER + Rogue_BadgeCount());
    sTrainerCachedId = 0;
}

static const struct ChampionLook *ChampionLook(void)
{
    struct RogueCast cast;
    RogueGen_Cast(&sRegion, &cast);
    return &sChampionLooks[cast.wandererLook % RLOOK_COUNT];
}

// The CHAMPION's name, in the game's characters.
static u8 *CopyChampionName(u8 *dest)
{
    struct RogueCast cast;
    RogueGen_Cast(&sRegion, &cast);
    return Rogue_CopyAscii(dest, (const u8 *)RogueGen_WandererName(&cast));
}

// ---------------------------------------------------------------------------
// The dojo (a town signature, src/rogue_town.c): two disciples, a Black Belt
// and a Battle Girl, and their master, an Expert, who fight once per run
// (their trainer flags). Fighting types, at the strength of the road ahead
// when the player comes, as gym trainers are.

#define CLASS_BATTLE_GIRL 13
#define CLASS_EXPERT      25

static const u8 sDojoClasses[ROGUE_DOJO_TRAINERS] = { CLASS_BLACK_BELT, CLASS_BATTLE_GIRL, CLASS_EXPERT };

// What they say comes from the text engine (src/rogue_text_signatures.h),
// into buf in the game's characters.
static const u8 *DojoLine(u32 slot, u32 field, u8 *buf, u32 size)
{
    char ascii[200];
    u32 place = RogueGen_SignatureTown(&sRegion, RSIG_DOJO);
    if (place >= sRegion.nPlaces)
        place = sRegion.start;
    RogueText_SigLine(&sRegion, place, RSIGTEXT_DOJO, (slot - ROGUE_SLOT_DOJO_0) * 3 + field, ascii, sizeof(ascii));
    // (Each line is two boxes at most, well within buf.)
    (void)size;
    Rogue_CopyLoreAscii(buf, ascii);
    return buf;
}

static bool32 IsDojoSlot(u32 slot)
{
    return slot >= ROGUE_SLOT_DOJO_0 && slot < ROGUE_SLOT_DOJO_0 + ROGUE_DOJO_TRAINERS;
}

u16 Rogue_GetDojoGfx(u32 which)
{
    return sClasses[sDojoClasses[which % ROGUE_DOJO_TRAINERS]].gfx;
}

static u32 BuildDojoTrainer(u32 slot, u32 *rng)
{
    u32 which = slot - ROGUE_SLOT_DOJO_0, place = RogueGen_SignatureTown(&sRegion, RSIG_DOJO);
    const struct RoguePlace *p = &sRegion.places[place < sRegion.nPlaces ? place : 0];
    const struct ClassDef *class = &sClasses[sDojoClasses[which]];
    u32 level = sRegion.aceLevel[BadgeStep()] > 8 ? sRegion.aceLevel[BadgeStep()] - 8 : 2, size = which == 2 ? 3 : 2;
    u32 tier = p->tier < NUM_TIERS ? p->tier : NUM_TIERS - 1, i, lo, hi;
    u32 name = RogueRng_Hash(sRegion.seed, 23300 + which);
    if (level < p->level)
        level = p->level;
    level += which + (which == 2);
    TrainerBstBand(level, &lo, &hi);
    for (i = 0; i < size; i++)
    {
        u32 monLevel = level - (i + 1 < size), tries = 0;
        do
        {
            u16 species = PickTrainerSpecies(rng, TYPE_FIGHTING, monLevel, lo, hi + (which == 2 ? 40 : 0), 500, tier >= 6);
            SetPartyMon(i, species, monLevel, 8 + tier, tier >= 6);
        } while (++tries < 20 && PartyHasSpecies(i, sTrainerParty[i].species));
    }
    if (which == 2)
    {
        sTrainer.items[0] = level < 30 ? ITEM_SUPER_POTION : ITEM_HYPER_POTION;
        sTrainer.aiFlags = AI_FLAG_BASIC_TRAINER | AI_FLAG_SMART_MON_CHOICES;
    }
    sTrainer.trainerClass = class->trainerClass;
    sTrainer.encounterMusic = class->music;
    sTrainer.gender = class->gender;
    sTrainer.trainerPic = class->pic;
    // Their own names among the region's (GenerateContent).
    if (sRegion.nTrainerSlots <= DOJO_NAME_SLOT)
        name = sTrainerNames[DOJO_NAME_SLOT + which];
    if (class->gender == FEMALE)
        StringCopy(sTrainer.trainerName, sFemaleNames[name % ARRAY_COUNT(sFemaleNames)]);
    else
        StringCopy(sTrainer.trainerName, sMaleNames[name % ARRAY_COUNT(sMaleNames)]);
    return size;
}

// A route feature's trainer, at its route's level: a pair of two weak
// POKeMON on the first routes (as Emerald's twins on Route 103) and more
// later; the family a little stronger each, the grandparent strongest;
// the Seashore House's like the route's own.
static u32 BuildFeatureTrainer(u32 slot, u32 *rng)
{
    u8 place = RogueGen_FeatureSlotPlace(&sRegion, slot);
    const struct RoguePlace *p = &sRegion.places[place < sRegion.nPlaces ? place : 0];
    const struct ClassDef *class = FeatureClass(slot);
    u32 tier = p->tier < NUM_TIERS ? p->tier : NUM_TIERS - 1, level = p->level, size, i, lo, hi;
    u32 name = RogueRng_Hash(sRegion.seed, 42600 + slot);
    if (slot < ROGUE_SLOT_FAMILY_0)
    {
        size = tier <= 3 ? 2 : tier <= 6 ? 3 : 4;
        level = level > 3 ? level - 1 : 2;
    }
    else if (slot < ROGUE_SLOT_SEASHORE_0)
    {
        u32 which = slot - ROGUE_SLOT_FAMILY_0;
        size = 2 + (which >= 2);
        level += which / 2;
    }
    else
    {
        size = 1 + (RogueRng_Range(rng, 100) < 60);
    }
    TrainerBstBand(level, &lo, &hi);
    for (i = 0; i < size; i++)
    {
        u32 monLevel = level - (i + 1 < size && level > 2), tries = 0;
        u16 species;
        do
        {
            species = PickTrainerSpecies(rng, class->type, monLevel, lo, hi, 500, tier >= 6);
        } while (++tries < 20 && (PartyHasSpecies(i, species)
                 || (tier <= 3 && SpeciesHasType(species, TYPE_GHOST))
                 || (tier == 0 && (SpeciesHasType(species, TYPE_STEEL) || SpeciesHasType(species, TYPE_GRASS)
                                || SpeciesHasType(species, TYPE_FIRE) || SpeciesHasType(species, TYPE_WATER)))));
        SetPartyMon(i, species, monLevel, tier, tier >= 6);
    }
    sTrainer.trainerClass = class->trainerClass;
    sTrainer.encounterMusic = class->music;
    sTrainer.gender = class->gender;
    sTrainer.trainerPic = class->pic;
    if (slot < ROGUE_SLOT_FAMILY_0)
    {
        sTrainer.battleType = TRAINER_BATTLE_TYPE_DOUBLES;
        StringCopy(sTrainer.trainerName, sPairNames[RogueGen_PairKind(&sRegion, slot - ROGUE_SLOT_PAIR_0) % RPAIR_COUNT][name % 4]);
    }
    else if (class->gender == FEMALE)
        StringCopy(sTrainer.trainerName, sFemaleNames[sTrainerNames[slot] % ARRAY_COUNT(sFemaleNames)]);
    else
        StringCopy(sTrainer.trainerName, sMaleNames[sTrainerNames[slot] % ARRAY_COUNT(sMaleNames)]);
    (void)name;
    if (slot == ROGUE_SLOT_FAMILY_0 + ROGUE_FAMILY_TRAINERS - 1)
        sTrainer.items[0] = level < 30 ? ITEM_SUPER_POTION : ITEM_HYPER_POTION;
    return size;
}

// What a feature trainer says, from src/rogue_text_features.h, into buf.
static const u8 *FeatureLine(u32 slot, u32 field, u8 *buf)
{
    char ascii[300];
    if (RogueText_FeatTrainerLine(&sRegion, slot, field, ascii, sizeof(ascii)) == 0)
        return NULL;
    Rogue_CopyLoreAscii(buf, ascii);
    return buf;
}


#include "rogue_run0_trainers.h"

// Every party built here passes trainer_util.c's checks: a gender of 0
// (a TrainerMon cleared with memset) stopped CHAMPION TERRY's battle on a
// debug screen ("unknown trainer mon gender value 0").
static void SettleTrainerParty(u32 size)
{
    u32 i;
    for (i = 0; i < size && i < PARTY_SIZE; i++)
    {
        if (sTrainerParty[i].gender < TRAINER_MON_MALE || sTrainerParty[i].gender > TRAINER_MON_RANDOM_GENDER)
            sTrainerParty[i].gender = TRAINER_MON_RANDOM_GENDER;
    }
}

const struct Trainer *Rogue_GetTrainer(u16 trainerId)
{
    s32 slot = TrainerSlot(trainerId), master;
    u32 rng, i, size;
    bool32 run0 = Rogue_IsRun0(), run0Built = FALSE;
    u16 key = trainerId;

    EnsureRegion();
    // RUN 0's Director is a second team once beaten (src/rogue_run0_trainers.h).
    if (run0 && slot == ROGUE_SLOT_CHAMPION && Rogue_Run0DirectorPhase() == 2)
        key |= 0x8000;
    if (sTrainerCachedId == key)
        return &sTrainer;

    rng = TrainerSlotRng(slot);
    memset(&sTrainer, 0, sizeof(sTrainer));
    sTrainer.party = sTrainerParty;
    sTrainer.aiFlags = AI_FLAG_BASIC_TRAINER;
    sTrainer.battleType = TRAINER_BATTLE_TYPE_SINGLES;

    // RUN 0's own: CHAMPION TERRY (src/rogue_run0_story.c); the rest of
    // the story's teams are the challenge's (src/rogue_run0_trainers.h).
    if ((size = Rogue_Run0BuildTrainer(slot, &sTrainer, sTrainerParty)) != 0)
    {
        SettleTrainerParty(size);
        sTrainer.partySize = size;
        sTrainerCachedId = trainerId;
        return &sTrainer;
    }

    if (run0 && (size = Run0BuildTrainer(slot, &rng)) != 0)
    {
        run0Built = TRUE;
    }
    else if (VillainClass(slot) != NULL)
    {
        size = BuildVillainTrainer(slot, &rng);
    }
    else if (slot == ROGUE_SLOT_CHAMPION)
    {
        const struct ChampionLook *look = ChampionLook();
        u16 ace = look->aces[RogueRng_Range(&rng, ARRAY_COUNT(look->aces))];
        size = ARRAY_COUNT(sChampionLevelOffsets);
        for (i = 0; i + 1 < size; i++)
        {
            u16 species;
            u32 tries = 0;
            // Five different species from their look's team (STEVEN's
            // stones, WALLACE's water...), never the ace, and no three of
            // one first type.
            do
            {
                species = look->team[RogueRng_Range(&rng, look->teamCount)];
                if (IsSpeciesEnabled(species))
                {
                    SetPartyMon(i, species, 60, 24, TRUE);
                    sTrainerParty[i].lvl = sRegion.championLevel + sChampionLevelOffsets[i];
                }
            } while (++tries < 60 && (!IsSpeciesEnabled(species) || PartyHasSpecies(i, sTrainerParty[i].species)
                                      || sTrainerParty[i].species == ace || ChampionTypeCrowded(i)));
        }
        // Their ace last, as Steven's Metagross and Wallace's Milotic, and
        // sent out last.
        SetPartyMon(size - 1, ace, 60, 24, TRUE);
        sTrainerParty[size - 1].lvl = sRegion.championLevel + sChampionLevelOffsets[size - 1];
        sTrainerParty[size - 1].heldItem = ITEM_SITRUS_BERRY;
        sTrainer.items[0] = ITEM_FULL_RESTORE;
        sTrainer.items[1] = ITEM_HYPER_POTION;
        sTrainer.aiFlags = AI_FLAG_BASIC_TRAINER | AI_FLAG_SMART_MON_CHOICES | AI_FLAG_ACE_POKEMON;
        sTrainer.trainerClass = TRAINER_CLASS_CHAMPION;
        sTrainer.encounterMusic = look->music;
        sTrainer.gender = look->pic == TRAINER_PIC_COOLTRAINER_F || look->pic == TRAINER_PIC_EXPERT_F ? FEMALE : MALE;
        sTrainer.trainerPic = look->pic;
        CopyChampionName(sTrainer.trainerName);
    }
    else if (IsFeatureSlot(slot))
    {
        size = BuildFeatureTrainer(slot, &rng);
    }
    else if (IsDojoSlot(slot))
    {
        size = BuildDojoTrainer(slot, &rng);
    }
    else if (slot >= ROGUE_SLOT_RIVAL_0)
    {
        u32 battle = (slot - ROGUE_SLOT_RIVAL_0) & 3;
        u32 level = sRegion.rivalLevel[battle];
        u16 starter = RivalStarter(), second = battle == 3 ? RivalSecondCounter() : SPECIES_NONE;
        size = sRivalSizes[battle];
        for (i = 0; i + 1 < size; i++)
        {
            u32 tries = 0;
            do
            {
                // Catches from the routes so far, and a fish or two.
                u16 species = (i == 1 && battle >= 1) ? sFishSpecies[battle >= 2][RogueRng_Range(&rng, FISH_SPECIES)]
                                                       : WildOfTier(&rng, battle * 3 + i);
                // IVs as Emerald's rival teams have them, low for the
                // catches and rising with each battle.
                // The third battle's catches stand two levels lower still,
                // as Emerald's Route 119 rival is a soft fight.
                SetPartyMon(i, species, level - (battle == 2 ? 5 : 3) + i / 2, battle * 6, battle >= 2);
            } while (++tries < 20 && PartyHasSpecies(i, sTrainerParty[i].species));
        }
        if (second != SPECIES_NONE && size >= 2)
            SetPartyMon(0, second, level - 2, 6 + battle * 6, FALSE);
        SetPartyMon(size - 1, starter, level, 6 + battle * 6, FALSE);
        // The partner is the same POKeMON every battle: one gender for the
        // run (as its species allows), and it comes out last.
        sTrainerParty[size - 1].gender = RivalPartnerGender(sTrainerParty[size - 1].species);
        sTrainer.aiFlags |= AI_FLAG_ACE_POKEMON;
        // The first battle's starter knows only what it hatched with, as in
        // Emerald, so its type does not decide the fight.
        if (battle == 0)
        {
            const struct LevelUpMove *learnset = GetSpeciesLevelUpLearnset(sTrainerParty[size - 1].species);
            u32 k, n = 0;
            for (k = 0; k < MAX_MON_MOVES; k++)
                sTrainerParty[size - 1].moves[k] = MOVE_NONE;
            for (k = 0; learnset[k].move != LEVEL_UP_MOVE_END && learnset[k].level <= 1 && n < MAX_MON_MOVES; k++)
                sTrainerParty[size - 1].moves[n++] = learnset[k].move;
        }
        // The second battle is Emerald's Route 110 rival: no items. From
        // the third on the rival thinks ahead and has a Potion; only the
        // last one's starter holds a Sitrus Berry, with a Hyper Potion.
        if (battle >= 2)
        {
            if (battle >= 3)
                sTrainerParty[size - 1].heldItem = ITEM_SITRUS_BERRY;
            sTrainer.items[0] = battle >= 3 ? ITEM_HYPER_POTION : ITEM_SUPER_POTION;
            sTrainer.aiFlags = AI_FLAG_BASIC_TRAINER | AI_FLAG_SMART_MON_CHOICES | AI_FLAG_ACE_POKEMON;
        }
        sTrainer.trainerClass = TRAINER_CLASS_RIVAL;
        sTrainer.encounterMusic = TRAINER_ENCOUNTER_MUSIC_MALE;
        if (gSaveBlock2Ptr->playerGender == MALE)
        {
            sTrainer.gender = FEMALE;
            sTrainer.trainerPic = TRAINER_PIC_MAY;
            StringCopy(sTrainer.trainerName, sText_May);
        }
        else
        {
            sTrainer.gender = MALE;
            sTrainer.trainerPic = TRAINER_PIC_BRENDAN;
            StringCopy(sTrainer.trainerName, sText_Brendan);
        }
    }
    else if (slot >= ROGUE_SLOT_E4_0)
    {
        u32 member = slot - ROGUE_SLOT_E4_0;
        const struct LeaderDef *leader = &sLeaders[sE4[member]];
        size = 5;
        BuildLeaderTeam(leader, &rng, size, sRegion.e4Level[member], 500, TRUE, 24);
        sTrainerParty[size - 1].heldItem = ITEM_SITRUS_BERRY;
        sTrainer.items[0] = ITEM_FULL_RESTORE;
        sTrainer.items[1] = ITEM_HYPER_POTION;
        sTrainer.aiFlags = AI_FLAG_BASIC_TRAINER | AI_FLAG_SMART_MON_CHOICES | AI_FLAG_ACE_POKEMON;
        sTrainer.trainerClass = TRAINER_CLASS_ELITE_FOUR;
        sTrainer.encounterMusic = TRAINER_ENCOUNTER_MUSIC_ELITE_FOUR;
        sTrainer.gender = leader->gender;
        sTrainer.trainerPic = leader->pic;
        if (leader->doubles)
            sTrainer.battleType = TRAINER_BATTLE_TYPE_DOUBLES;
        StringCopy(sTrainer.trainerName, leader->name);
    }
    else if (slot >= ROGUE_SLOT_LEADER_0)
    {
        // The leader of the town, at the strength of the next badge: the
        // first gym fought is a first gym, wherever it stands.
        u32 gym = slot - ROGUE_SLOT_LEADER_0, step = BadgeStep();
        const struct LeaderDef *leader = &sLeaders[sGymLeaders[gym]];
        size = sLeaderTeamSize[step];
        if (leader->doubles && step >= 5 && size < 2)
            size = 2;
        // The first two leaders' IVs as Roxanne's and Brawly's are (12 and
        // a little more), later ones a well-raised 20, but 16 for the sixth,
        // the first double battle and the wall of most strong runs. The first three
        // leaders' teams stand two levels under the region's ace levels
        // (the level the road prepares a team for), so a team that arrives
        // there wins most of the time, as against Roxanne, Brawly and
        // Wattson; one that lost its catches on the way still struggles.
        BuildLeaderTeam(leader, &rng, size, sRegion.aceLevel[step] - (step < 3 ? 2 : 0), sLeaderMaxBst[step], step >= 5,
                        step == 0 ? 12 : (step == 1 || step == 5 ? 16 : 20));
        if (step == 0 && !(gRogueTestLeader[0] && (gRogueTestLeader[3] & 8)))
            TameFirstLeader(size, leader->type, (gRogueTestLeader[0] && (gRogueTestLeader[3] & 16)) ? ITEM_NONE
                                                : RogueGen_GymTM(&sRegion, gym, leader->type));
        sTrainerParty[size - 1].heldItem = (step == 0) ? ITEM_ORAN_BERRY : ITEM_SITRUS_BERRY;
        sTrainer.items[0] = (step == 0) ? ITEM_POTION : (step < 5 ? ITEM_SUPER_POTION : ITEM_HYPER_POTION);
        if (step >= 3)
            sTrainer.aiFlags = AI_FLAG_BASIC_TRAINER | AI_FLAG_SMART_MON_CHOICES;
        // The ace comes out last, as the gym guide's scouting says.
        sTrainer.aiFlags |= AI_FLAG_ACE_POKEMON;
        if (gRogueTestLeader[0])
        {
            for (i = 0; i < size; i++)
            {
                if (i == size - 1 || (gRogueTestLeader[3] & 4))
                    sTrainerParty[i].lvl += gRogueTestLeader[1];
                if (gRogueTestLeader[2])
                    sTrainerParty[i].iv = TRAINER_PARTY_IVS(gRogueTestLeader[2], gRogueTestLeader[2], gRogueTestLeader[2],
                                                            gRogueTestLeader[2], gRogueTestLeader[2], gRogueTestLeader[2]);
            }
            if (gRogueTestLeader[3] & 1)
                sTrainerParty[size - 1].heldItem = ITEM_NONE;
            if (gRogueTestLeader[3] & 2)
                sTrainer.items[0] = sTrainer.items[1] = ITEM_NONE;
        }
        sTrainer.trainerClass = TRAINER_CLASS_LEADER;
        sTrainer.encounterMusic = (leader->gender == FEMALE) ? TRAINER_ENCOUNTER_MUSIC_FEMALE : TRAINER_ENCOUNTER_MUSIC_MALE;
        sTrainer.gender = leader->gender;
        sTrainer.trainerPic = leader->pic;
        // A double battle only from the sixth badge on, as Tate and Liza
        // are Emerald's seventh.
        if (leader->doubles && step >= 5)
            sTrainer.battleType = TRAINER_BATTLE_TYPE_DOUBLES;
        StringCopy(sTrainer.trainerName, leader->name);
    }
    else if ((master = MasterDungeon(slot)) >= 0)
    {
        // A dungeon master: three or four strong Pokemon of the dungeon's
        // kind, the ace near the next gym ace's level.
        const struct RogueMasterDef *def = RogueDungeon_MasterDef(master);
        u32 level = RogueDungeon_MasterLevel(master), lo, hi;
        size = level < 30 ? 3 : 4;
        TrainerBstBand(level, &lo, &hi);
        hi += 40;
        for (i = 0; i < size; i++)
        {
            u32 monLevel = (i == size - 1) ? level : level - 2 - (size - 2 - i) / 2, tries = 0;
            do
            {
                u16 species = PickTrainerSpecies(&rng, def->types[i & 1], monLevel, lo, hi, 500, monLevel >= 26);
                SetPartyMon(i, species, monLevel, 16, monLevel >= 26);
            } while (++tries < 20 && PartyHasSpecies(i, sTrainerParty[i].species));
        }
        sTrainerParty[size - 1].heldItem = ITEM_SITRUS_BERRY;
        sTrainer.items[0] = level < 30 ? ITEM_SUPER_POTION : ITEM_HYPER_POTION;
        sTrainer.aiFlags = AI_FLAG_BASIC_TRAINER | AI_FLAG_SMART_MON_CHOICES | AI_FLAG_ACE_POKEMON;
        sTrainer.trainerClass = def->trainerClass;
        sTrainer.encounterMusic = def->music;
        sTrainer.gender = def->gender;
        sTrainer.trainerPic = def->pic;
        if (def->gender == FEMALE)
            StringCopy(sTrainer.trainerName, sFemaleNames[sTrainerNames[slot] % ARRAY_COUNT(sFemaleNames)]);
        else
            StringCopy(sTrainer.trainerName, sMaleNames[sTrainerNames[slot] % ARRAY_COUNT(sMaleNames)]);
    }
    else
    {
        const struct ClassDef *class = TrainerClass(slot);
        enum Type type = TYPE_NONE;
        u32 level, maxBst, tier;

        RogueRng_Next(&rng); // keep in step with TrainerClass
        u32 lo, hi, roll;
        if (slot >= ROGUE_SLOT_GYM_0)
        {
            u32 gym = (slot - ROGUE_SLOT_GYM_0) / ROGUE_GYM_TRAINERS;
            u32 which = (slot - ROGUE_SLOT_GYM_0) % ROGUE_GYM_TRAINERS;
            u32 step = BadgeStep();
            type = sLeaders[sGymLeaders[gym]].type;
            // Gym trainers a few levels under the leader, nearer as you go
            // in, at the strength of the next badge as the leader is.
            level = sRegion.aceLevel[step] - 7 + which;
            maxBst = 340 + step * 30;
            size = 1 + (step > 1) + (step > 3);
            tier = sRegion.places[sRegion.gymTown[step]].tier;
        }
        else
        {
            const struct RoguePlace *p = SlotPlace(slot);
            tier = p->tier < NUM_TIERS ? p->tier : NUM_TIERS - 1;
            level = p->level + (slot - p->trainerBase) % 3;
            level = level > 3 ? level - 1 : 2;
            maxBst = 500;
            // Party sizes like Emerald's, about two on average: mostly one or
            // two early, now and then three later.
            roll = RogueRng_Range(&rng, 100);
            if (p->number == 1 && p->kind == RPLACE_ROUTE && !p->side)
                size = 1;
            else if (level <= 10)
                size = roll < 55 ? 1 : 2;
            else if (level <= 20)
                size = roll < 35 ? 1 : 2; // Emerald's three-Pokemon trainers come later
            else if (level <= 30)
                size = roll < 30 ? 1 : (roll < 80 ? 2 : 3);
            else
                size = roll < 20 ? 1 : (roll < 70 ? 2 : 3);
            // Route 2 often comes straight after route 1 with no heal stop.
            // Emerald's Route 102 has one Pokemon at 5 or two at 3 to 4.
            if (p->number == 2 && p->kind == RPLACE_ROUTE && !p->side)
            {
                u32 cap = size == 1 ? 5 : 4;
                if (level > cap)
                    level = cap;
            }
            // A tower's floors count as a cave's, but its Hex Maniacs and
            // Psychics keep to their own type, as Mt. Pyre's do.
            // (So do a wreck's and a power plant's trainers.)
            if (p->kind == RPLACE_CAVE && !(p == &sFloorPlace && SlotDungeon(slot) >= 0
                                            && (sRegion.dungeons[SlotDungeon(slot)].kind == RDUN_TOWER
                                             || sRegion.dungeons[SlotDungeon(slot)].kind == RDUN_SHIP
                                             || sRegion.dungeons[SlotDungeon(slot)].kind == RDUN_PLANT)))
            {
                // Underground, a class with a type of its own keeps it, as
                // Granite Cave's Black Belts bring Machop and Makuhita;
                // Hikers and Ruin Maniacs bring ROCK or GROUND. The rest
                // bring what lives in caves.
                type = sCaveTypes[RogueRng_Range(&rng, ARRAY_COUNT(sCaveTypes))];
                if (class->type == TYPE_ROCK || class->type == TYPE_GROUND)
                    type = (type == TYPE_ROCK || type == TYPE_GROUND) ? type : class->type;
                else if (class->type != TYPE_NONE)
                    type = class->type;
            }
            else
            {
                type = class->type;
            }
        }
        if (size > PARTY_SIZE)
            size = PARTY_SIZE;
        TrainerBstBand(level, &lo, &hi);
        for (i = 0; i < size; i++)
        {
            // Its level: the last one at the trainer's level, the others
            // one under, as Emerald's trainers mostly are.
            u32 monLevel = level - (i + 1 < size && level > 2), tries;
            u16 species = PickTrainerSpecies(&rng, type, monLevel, lo, hi, maxBst, tier >= 6);
            // Nothing a starter's Normal move cannot touch, and no walls
            // that resist everything, this early.
            // Nor Grass, Fire or Water types, which would beat one of
            // the three starters before it has a backup.
            // Ghosts stay off the roads until about level 18 too: a young
            // team's Normal moves cannot touch them, and Emerald keeps its
            // trainers' ghosts for Mt. Pyre.
            for (tries = 0; tries < 20 && slot < ROGUE_SLOT_GYM_0
                 && ((tier <= 3 && SpeciesHasType(species, TYPE_GHOST))
                  || (tier == 0 && (SpeciesHasType(species, TYPE_STEEL)
                  || SpeciesHasType(species, TYPE_GRASS) || SpeciesHasType(species, TYPE_FIRE)
                  || SpeciesHasType(species, TYPE_WATER)))); tries++)
                species = PickTrainerSpecies(&rng, type, monLevel, lo, hi, maxBst, tier >= 6);
            // Emerald's route trainers have low IVs; a gym's own trainers
            // are better raised.
            SetPartyMon(i, species, monLevel, slot >= ROGUE_SLOT_GYM_0 ? 8 + tier : tier, tier >= 6);
        }
        sTrainer.trainerClass = class->trainerClass;
        sTrainer.encounterMusic = class->music;
        sTrainer.gender = class->gender;
        sTrainer.trainerPic = class->pic;
        if (class->gender == FEMALE)
            StringCopy(sTrainer.trainerName, sFemaleNames[sTrainerNames[slot] % ARRAY_COUNT(sFemaleNames)]);
        else
            StringCopy(sTrainer.trainerName, sMaleNames[sTrainerNames[slot] % ARRAY_COUNT(sMaleNames)]);
    }
    // RUN 0: the normal builder's teams, at RUN 0's levels. ENDLESS with a
    // carried party: the same, on its own band.
    if ((run0 && !run0Built) || (!run0 && Rogue_Run0Scaled()))
        size = Run0Raise(slot, size, &rng);
    SettleTrainerParty(size);
    sTrainer.partySize = size;
    sTrainerCachedId = key;
    return &sTrainer;
}

// The species trainer slot sends out first, for their lines ({OWN}): what
// they say they caught is in their team. Builds the trainer (it is the one
// about to battle or just beaten, so the cache mostly has it already).
u16 Rogue_TrainerLeadSpecies(u32 slot)
{
    const struct Trainer *t;
    if (slot >= ROGUE_TRAINER_SLOTS)
        return SPECIES_NONE;
    t = Rogue_GetTrainer(ROGUE_TRAINER(slot));
    return t->partySize > 0 ? t->party[0].species : SPECIES_NONE;
}

// ---------------------------------------------------------------------------
// Trainer lines

// The rival's lines come from src/rogue_text_leaders.h (RogueText_RivalLine).
static const u8 *RivalLine(u32 battle, u32 field, u8 *dest, u32 size)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    Rogue_TextCtx(&c, &st, -1, RSPK_RIVAL);
    Rogue_BufferText(dest, size, RogueText_RivalLine(&c, battle, field), &c);
    return dest;
}

// A class trainer's line (RTRAINER_INTRO, _DEFEAT, _AFTER) from the
// generator (src/rogue_text_trainers.h), in dest in the game's characters.
static const u8 *ClassLine(u32 slot, u32 field, u8 *dest, u32 size)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    const struct MapEvents *events = gMapHeader.events;
    s32 index = -2;
    u32 i;
    // The set the map's builder kept on the trainer's object (BuildObjects),
    // else worked out now (a gym's trainers).
    for (i = 0; events != NULL && i < events->objectEventCount; i++)
        if (events->objectEvents[i].script == gRogueTrainerScripts[slot] && events->objectEvents[i].unused)
            index = events->objectEvents[i].unused - 1;
    if (index == -2)
        index = RogueText_TrainerSetIndex(&sRegion, slot);
    Rogue_TextCtx(&c, &st, RogueText_TrainerPlace(&sRegion, slot), RSPK_TRAINER);
    RogueText_TrainerCtx(&c, slot);
    return Rogue_BufferText(dest, size, RogueText_TrainerTemplate(&c, slot, index, field), &c), dest;
}

// A gym leader's or Elite Four member's line (RLEADER_*) from the generator
// (src/rogue_text_leaders.h), in dest in the game's characters. gym: their
// gym (0-7) or Elite Four seat (8-11), for the slots.
static const u8 *LeaderLine(u32 leader, u32 field, u32 gym, u8 *dest, u32 size)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    // RUN 0: the eight custom leaders know what made them.
    if (Rogue_Run0LeaderLine(leader, field, dest, size))
        return dest;
    Rogue_TextCtx(&c, &st, gym < ROGUE_NUM_GYMS ? sRegion.gymTown[gym] : sRegion.summit, gym < ROGUE_NUM_GYMS ? RSPK_LEADER : RSPK_E4);
    c.gym = gym;
    Rogue_BufferText(dest, size, RogueText_LeaderLineIn(&c, leader, field), &c);
    return dest;
}

// Intro and defeat lines for a generated trainer, or NULL to use the
// script's own. Called by the battle setup.
const u8 *Rogue_GetTrainerText(u16 trainerId, u32 kind)
{
    s32 slot;
    if (!Rogue_IsRogueTrainer(trainerId))
        return NULL;
    EnsureRegion();
    slot = TrainerSlot(trainerId);
    // RUN 0's story: the Director's words for its trainers.
    {
        const u8 *run0 = Rogue_Run0TrainerText(slot, kind);
        if (run0 != NULL)
            return run0;
    }
    if (VillainClass(slot) != NULL)
        return VillainLine(slot, kind, gStringVar4, sizeof(gStringVar4));
    if (IsDojoSlot(slot))
        return DojoLine(slot, kind == ROGUE_TEXT_INTRO ? RTRAINER_INTRO : RTRAINER_DEFEAT, gStringVar4, sizeof(gStringVar4));
    if (IsFeatureSlot(slot))
        return FeatureLine(slot, kind == ROGUE_TEXT_INTRO ? RTRAINER_INTRO : RTRAINER_DEFEAT, gStringVar4);
    if (slot == ROGUE_SLOT_CHAMPION)
    {
        struct RogueTextCtx c;
        struct RogueTextState st;
        if (kind == ROGUE_TEXT_INTRO)
            return NULL;
        Rogue_TextCtx(&c, &st, sRegion.summit, RSPK_CHAMPION);
        return Rogue_BufferText(gStringVar4, sizeof(gStringVar4), RogueText_ChampionLine(&c, RCHAMP_DEFEAT, WandererMet()), &c), gStringVar4;
    }
    if (slot >= ROGUE_SLOT_RIVAL_0)
        return RivalLine(slot - ROGUE_SLOT_RIVAL_0, kind == ROGUE_TEXT_INTRO ? RRIVAL_INTRO : RRIVAL_DEFEAT, gStringVar4, sizeof(gStringVar4));
    if (slot >= ROGUE_SLOT_E4_0)
    {
        return LeaderLine(sE4[slot - ROGUE_SLOT_E4_0], kind == ROGUE_TEXT_INTRO ? RLEADER_E4_INTRO : RLEADER_DEFEAT,
                          ROGUE_NUM_GYMS + slot - ROGUE_SLOT_E4_0, gStringVar4, sizeof(gStringVar4));
    }
    if (slot >= ROGUE_SLOT_LEADER_0)
    {
        return LeaderLine(sGymLeaders[slot - ROGUE_SLOT_LEADER_0], kind == ROGUE_TEXT_INTRO ? RLEADER_INTRO : RLEADER_DEFEAT,
                          slot - ROGUE_SLOT_LEADER_0, gStringVar4, sizeof(gStringVar4));
    }
    if (MasterDungeon(slot) >= 0)
        return Rogue_BufferMasterLine(MasterDungeon(slot), kind == ROGUE_TEXT_INTRO ? RMASTER_INTRO : RMASTER_DEFEAT,
                                      gStringVar4, sizeof(gStringVar4));
    // Shown at once (copied into gStringVar4 by the caller, where it is
    // made: no placeholders, so the copy leaves it as it is).
    return ClassLine(slot, kind == ROGUE_TEXT_INTRO ? RTRAINER_INTRO : RTRAINER_DEFEAT, gStringVar4, sizeof(gStringVar4));
}

// Script special: what trainer slot VAR_0x8004 says after the battle, into
// STR_VAR_1.
void Rogue_BufferTrainerAfter(void)
{
    u32 slot = gSpecialVar_0x8004;
    EnsureRegion();
    if (VillainClass(slot) != NULL)
    {
        VillainLine(slot, 2, gStringVar1, sizeof(gStringVar1));
        return;
    }
    if (IsDojoSlot(slot))
    {
        DojoLine(slot, RTRAINER_AFTER, gStringVar1, sizeof(gStringVar1));
        return;
    }
    if (IsFeatureSlot(slot))
    {
        if (FeatureLine(slot, RTRAINER_AFTER, gStringVar1) == NULL)
            StringCopy(gStringVar1, COMPOUND_STRING("That was a good battle."));
        return;
    }
    // On the gauntlet's crossing, how many of its trainers are left.
    if (slot < sRegion.nTrainerSlots)
    {
        u8 place = RogueGen_SlotPlace(&sRegion, slot);
        if (place != 0xFF && RogueGen_RouteFeature(&sRegion, place) == RFEAT_GAUNTLET)
        {
            char ascii[300];
            if (RogueText_GauntletLine(&sRegion, place, Rogue_GauntletLeft(place), slot, ascii, sizeof(ascii)))
            {
                Rogue_CopyLoreAscii(gStringVar1, ascii);
                return;
            }
        }
    }
    if (slot >= ROGUE_SLOT_LEADER_0)
        slot = 0;
    ClassLine(slot, RTRAINER_AFTER, gStringVar1, sizeof(gStringVar1));
}

// ---------------------------------------------------------------------------
// The villain teams
//
// One, two or three of Team Aqua, Team Magma and Team Rocket
// (src/rogue_gen_hideout.h), as in Emerald and FireRed: grunts on a route
// before each hideout's town and in the hideout, the main team's grunts
// blocking its town's way on until the admin at the bottom of its hideout is
// beaten (FLAG_ROGUE_HIDEOUT_CLEARED), and each leader, met later where a
// legend's dungeon puts him (ROBJ_VILLAIN_LEADER, Rogue_TeamLeaderReady).
// Team t's admin and leader are trainer slots of their own
// (RogueGen_TeamAdminSlot, RogueGen_TeamLeaderSlot), whose defeat flags say
// the hideout is cleared and the leader beaten.

enum { VCLASS_GRUNT_M, VCLASS_GRUNT_F, VCLASS_ADMIN, VCLASS_ADMIN_2, VCLASS_LEADER, VCLASS_COUNT };

static const struct ClassDef sVillainClasses[ROGUE_VILLAIN_IDS][VCLASS_COUNT] =
{
    [ROGUE_VILLAIN_AQUA] =
    {
        { TRAINER_CLASS_TEAM_AQUA,   TRAINER_PIC_AQUA_GRUNT_M,        OBJ_EVENT_GFX_AQUA_MEMBER_M, MALE,   TRAINER_ENCOUNTER_MUSIC_AQUA, TYPE_NONE, 0, TALK_KID },
        { TRAINER_CLASS_TEAM_AQUA,   TRAINER_PIC_AQUA_GRUNT_F,        OBJ_EVENT_GFX_AQUA_MEMBER_F, FEMALE, TRAINER_ENCOUNTER_MUSIC_AQUA, TYPE_NONE, 0, TALK_KID },
        { TRAINER_CLASS_AQUA_ADMIN,  TRAINER_PIC_AQUA_ADMIN_M,        OBJ_EVENT_GFX_AQUA_MEMBER_M, MALE,   TRAINER_ENCOUNTER_MUSIC_AQUA, TYPE_NONE, 0, TALK_KID },
        { TRAINER_CLASS_AQUA_ADMIN,  TRAINER_PIC_AQUA_ADMIN_F,        OBJ_EVENT_GFX_AQUA_MEMBER_F, FEMALE, TRAINER_ENCOUNTER_MUSIC_AQUA, TYPE_NONE, 0, TALK_KID },
        { TRAINER_CLASS_AQUA_LEADER, TRAINER_PIC_AQUA_LEADER_ARCHIE,  OBJ_EVENT_GFX_ARCHIE,        MALE,   TRAINER_ENCOUNTER_MUSIC_AQUA, TYPE_NONE, 0, TALK_KID },
    },
    [ROGUE_VILLAIN_MAGMA] =
    {
        { TRAINER_CLASS_TEAM_MAGMA,   TRAINER_PIC_MAGMA_GRUNT_M,       OBJ_EVENT_GFX_MAGMA_MEMBER_M, MALE,   TRAINER_ENCOUNTER_MUSIC_MAGMA, TYPE_NONE, 0, TALK_KID },
        { TRAINER_CLASS_TEAM_MAGMA,   TRAINER_PIC_MAGMA_GRUNT_F,       OBJ_EVENT_GFX_MAGMA_MEMBER_F, FEMALE, TRAINER_ENCOUNTER_MUSIC_MAGMA, TYPE_NONE, 0, TALK_KID },
        { TRAINER_CLASS_MAGMA_ADMIN,  TRAINER_PIC_MAGMA_ADMIN,         OBJ_EVENT_GFX_MAGMA_MEMBER_M, MALE,   TRAINER_ENCOUNTER_MUSIC_MAGMA, TYPE_NONE, 0, TALK_KID },
        // COURTNEY has no Gen 3 sprite (she is from Omega Ruby and Alpha
        // Sapphire): she wears the female grunt's, with the admin's class.
        { TRAINER_CLASS_MAGMA_ADMIN,  TRAINER_PIC_MAGMA_GRUNT_F,       OBJ_EVENT_GFX_MAGMA_MEMBER_F, FEMALE, TRAINER_ENCOUNTER_MUSIC_MAGMA, TYPE_NONE, 0, TALK_KID },
        { TRAINER_CLASS_MAGMA_LEADER, TRAINER_PIC_MAGMA_LEADER_MAXIE,  OBJ_EVENT_GFX_MAXIE,          MALE,   TRAINER_ENCOUNTER_MUSIC_MAGMA, TYPE_NONE, 0, TALK_KID },
    },
    // FireRed's grunts. Gen 3 has no named Rocket admins: ARCHER and
    // ARIANA (HeartGold and SoulSilver's) wear the grunts' uniforms, as
    // the Sevii Islands' ADMINs do, with a ROCKET ADMIN class; GIOVANNI is
    // the BOSS, in FireRed's sprite.
    [ROGUE_VILLAIN_ROCKET] =
    {
        { TRAINER_CLASS_TEAM_ROCKET_FRLG, TRAINER_PIC_ROCKET_GRUNT_M_FRLG,  OBJ_EVENT_GFX_ROCKET_M, MALE,   TRAINER_ENCOUNTER_MUSIC_ROCKET, TYPE_NONE, 0, TALK_KID },
        { TRAINER_CLASS_TEAM_ROCKET_FRLG, TRAINER_PIC_ROCKET_GRUNT_F_FRLG,  OBJ_EVENT_GFX_ROCKET_F, FEMALE, TRAINER_ENCOUNTER_MUSIC_ROCKET, TYPE_NONE, 0, TALK_KID },
        { TRAINER_CLASS_ROCKET_ADMIN,     TRAINER_PIC_ROCKET_GRUNT_M_FRLG,  OBJ_EVENT_GFX_ROCKET_M, MALE,   TRAINER_ENCOUNTER_MUSIC_ROCKET, TYPE_NONE, 0, TALK_KID },
        { TRAINER_CLASS_ROCKET_ADMIN,     TRAINER_PIC_ROCKET_GRUNT_F_FRLG,  OBJ_EVENT_GFX_ROCKET_F, FEMALE, TRAINER_ENCOUNTER_MUSIC_ROCKET, TYPE_NONE, 0, TALK_KID },
        { TRAINER_CLASS_BOSS_FRLG,        TRAINER_PIC_LEADER_GIOVANNI_FRLG, OBJ_EVENT_GFX_GIOVANNI, MALE,   TRAINER_ENCOUNTER_MUSIC_ROCKET, TYPE_NONE, 0, TALK_KID },
    },
};

static const u8 sVillainNames[ROGUE_VILLAIN_IDS][VCLASS_COUNT][TRAINER_NAME_LENGTH + 1] =
{
    [ROGUE_VILLAIN_AQUA]   = { _("GRUNT"), _("GRUNT"), _("MATT"), _("SHELLY"), _("ARCHIE") },
    [ROGUE_VILLAIN_MAGMA]  = { _("GRUNT"), _("GRUNT"), _("TABITHA"), _("COURTNEY"), _("MAXIE") },
    [ROGUE_VILLAIN_ROCKET] = { _("GRUNT"), _("GRUNT"), _("ARCHER"), _("ARIANA"), _("GIOVANNI") },
};

// Their Pokemon, as in Emerald and FireRed. Grunts pick from the first
// list; the admin from it too, and the leader from the second (GIOVANNI's
// own, as at Silph Co.), each ending with an ace of their own: Sharpedo
// and Camerupt as in Emerald, Houndoom for Team Rocket's ADMINs (as the
// Sevii Islands' and ARCHER's) and Rhydon for GIOVANNI.
static const u16 sVillainMons[ROGUE_VILLAIN_IDS][5] =
{
    [ROGUE_VILLAIN_AQUA]   = { SPECIES_POOCHYENA, SPECIES_CARVANHA, SPECIES_ZUBAT, SPECIES_WAILMER, SPECIES_TENTACOOL },
    [ROGUE_VILLAIN_MAGMA]  = { SPECIES_POOCHYENA, SPECIES_NUMEL, SPECIES_ZUBAT, SPECIES_KOFFING, SPECIES_SLUGMA },
    [ROGUE_VILLAIN_ROCKET] = { SPECIES_RATTATA, SPECIES_ZUBAT, SPECIES_EKANS, SPECIES_KOFFING, SPECIES_GRIMER },
};
static const u16 sVillainBossMons[ROGUE_VILLAIN_IDS][5] =
{
    [ROGUE_VILLAIN_AQUA]   = { SPECIES_POOCHYENA, SPECIES_CARVANHA, SPECIES_ZUBAT, SPECIES_WAILMER, SPECIES_TENTACOOL },
    [ROGUE_VILLAIN_MAGMA]  = { SPECIES_POOCHYENA, SPECIES_NUMEL, SPECIES_ZUBAT, SPECIES_KOFFING, SPECIES_SLUGMA },
    [ROGUE_VILLAIN_ROCKET] = { SPECIES_NIDORINO, SPECIES_NIDORINA, SPECIES_KANGASKHAN, SPECIES_ONIX, SPECIES_MEOWTH },
};
static const u16 sVillainAce[ROGUE_VILLAIN_IDS] = { SPECIES_CARVANHA, SPECIES_NUMEL, SPECIES_HOUNDOUR };
static const u16 sVillainBossAce[ROGUE_VILLAIN_IDS] = { SPECIES_CARVANHA, SPECIES_NUMEL, SPECIES_RHYHORN };

// Their lines come from src/rogue_text_leaders.h (RogueText_VillainLine):
// each team in its own voice, after the region's own legends.

// A slot's team (0xFF for none) and role (RVROLE_*); clash TRUE for a grunt
// on a route two teams share, other then the team it quarrels with.
static u8 VillainSlotTeam(u32 slot, u8 *role, bool8 *clash, u8 *other)
{
    struct RogueTeams tm;
    u8 t, u;
    RogueGen_Teams(&sRegion, &tm);
    t = RogueGen_SlotTeam(&sRegion, &tm, slot, role, clash);
    *other = 0xFF;
    if (*clash)
    {
        // The route's own team quarrels with the team before it; that
        // team's grunt there with the route's.
        for (u = 1; u < tm.n; u++)
            if (tm.hint[u] == RogueGen_SlotPlace(&sRegion, slot))
                *other = t == u ? u - 1 : u;
    }
    return t;
}

static u32 VillainRole(u32 slot)
{
    u8 role, other;
    bool8 clash;
    VillainSlotTeam(slot, &role, &clash, &other);
    return role;
}

// The identity (ROGUE_VILLAIN_*) of team t.
static u32 TeamId(u32 team)
{
    return RogueGen_TeamId(&sRegion, team) % ROGUE_VILLAIN_IDS;
}

// A male grunt of team t, as he stands about.
static u16 TeamGruntGfx(u32 team)
{
    return sVillainClasses[TeamId(team)][VCLASS_GRUNT_M].gfx;
}

// The class of a villain trainer slot, or NULL for everyone else.
static const struct ClassDef *VillainClass(u32 slot)
{
    u8 role, other, t;
    bool8 clash;
    t = VillainSlotTeam(slot, &role, &clash, &other);
    switch (role)
    {
    case RVROLE_GRUNT:
        // A third of the grunts are women, as in Emerald's hideouts.
        return &sVillainClasses[TeamId(t)][RogueRng_Hash(sRegion.seed, 14100 + slot) % 3 == 0 ? VCLASS_GRUNT_F : VCLASS_GRUNT_M];
    case RVROLE_ADMIN:
        return &sVillainClasses[TeamId(t)][VCLASS_ADMIN + RogueGen_TeamAdmin(&sRegion, t)];
    case RVROLE_LEADER:
        return &sVillainClasses[TeamId(t)][VCLASS_LEADER];
    }
    return NULL;
}

// Gyms before a place on the main road: the next gym's ace is the one the
// hideout's admin is near.
static u32 GymsBefore(u32 place)
{
    u32 g, n = 0;
    for (g = 0; g < ROGUE_NUM_GYMS; g++)
        n += sRegion.gymTown[g] < place;
    return n < ROGUE_NUM_GYMS ? n : ROGUE_NUM_GYMS - 1;
}

// A grunt's, admin's or leader's team and name. Grunts follow the route
// trainers' levels and strength bands; the admin brings three or four a
// little under the next gym's ace, the leader four or five just under it.
static u32 BuildVillainTrainer(u32 slot, u32 *rng)
{
    u8 role, other, t;
    bool8 clash;
    u32 team, size, i, ace;
    const struct ClassDef *class = VillainClass(slot);
    const u16 *mons;

    t = VillainSlotTeam(slot, &role, &clash, &other);
    team = TeamId(t);
    mons = sVillainMons[team];
    if (role == RVROLE_GRUNT)
    {
        const struct RoguePlace *p = SlotPlace(slot);
        u32 level = p->level + (slot - p->trainerBase) % 3, roll = RogueRng_Range(rng, 100);
        level = level > 3 ? level - 1 : 2;
        if (level <= 12)
            size = roll < 60 ? 1 : 2;
        else if (level <= 30)
            size = roll < 30 ? 1 : 2;
        else
            size = roll < 50 ? 2 : 3;
        for (i = 0; i < size; i++)
        {
            u32 monLevel = level - (i + 1 < size && level > 2), lo, hi, tries;
            TrainerBstBand(monLevel, &lo, &hi);
            for (tries = 0; tries < 30; tries++)
            {
                u16 species = mons[RogueRng_Range(rng, ARRAY_COUNT(sVillainMons[0]))];
                if (tries < 20 && SpeciesBst(EvolveForLevel(species, monLevel, FALSE)) > hi)
                    continue;
                SetPartyMon(i, species, monLevel, 8 + p->tier, FALSE);
                if (!PartyHasSpecies(i, sTrainerParty[i].species))
                    break;
            }
        }
    }
    else
    {
        s32 h = RogueGen_TeamHideout(&sRegion, t);
        const struct RogueDungeon *d = &sRegion.dungeons[h < 0 ? 0 : h];
        bool32 leader = (role == RVROLE_LEADER);
        // (TEAM ROCKET's BOSS at the bottom of his hideout stands where an
        // admin would, a level over.)
        bool32 below = leader && RogueGen_TeamBossInHideout(&sRegion, t);
        // (An optional hideout entered later is manned for the badges held.)
        u32 step = leader && !below ? BadgeStep() : GymsBefore(d->host);
        if (!leader || below)
        {
            if (!d->required && BadgeStep() > step)
                step = BadgeStep();
        }
        u16 aceSpecies = leader ? sVillainBossAce[team] : sVillainAce[team];
        if (leader)
            mons = sVillainBossMons[team];
        // Under the next gym's ace, as Emerald's Maxie (Mt. Chimney, 24
        // before Flannery's 29) and the hideout admins are.
        ace = sRegion.aceLevel[step] - (leader ? 3 : 4);
        // But never under the hideout's grunts, who stand at their floor's level
        // (up to a level over it): the ace two over the last floor's, where
        // the admin waits, as Emerald's Matt and Tabitha stand over their
        // hideouts' grunts. A hideout under a late town is that town's level.
        if ((!leader || below) && h >= 0)
        {
            RogueGen_FloorPlace(&sRegion, h, d->nFloors, &sFloorPlace);
            if (ace < sFloorPlace.level + 2u + below)
                ace = sFloorPlace.level + 2 + below;
        }
        size = leader && !below ? (step >= 6 ? 5 : 4) : (step >= 3 ? 4 : 3);
        for (i = 0; i + 1 < size; i++)
        {
            u32 tries = 0;
            u16 species;
            do
            {
                species = mons[RogueRng_Range(rng, ARRAY_COUNT(sVillainMons[0]))];
                SetPartyMon(i, species, ace - 3 + i / 2, leader ? 20 : 14, leader);
            } while (++tries < 30 && (species == aceSpecies || PartyHasSpecies(i, sTrainerParty[i].species)));
        }
        SetPartyMon(size - 1, aceSpecies, ace, leader ? 24 : 16, leader);
        if (leader)
            sTrainerParty[size - 1].heldItem = ITEM_SITRUS_BERRY;
        sTrainer.items[0] = ace >= 35 ? ITEM_HYPER_POTION : ITEM_SUPER_POTION;
        if (leader)
            sTrainer.items[1] = sTrainer.items[0];
        sTrainer.aiFlags = AI_FLAG_BASIC_TRAINER | AI_FLAG_SMART_MON_CHOICES | AI_FLAG_ACE_POKEMON;
    }
    sTrainer.trainerClass = class->trainerClass;
    sTrainer.encounterMusic = class->music;
    sTrainer.gender = class->gender;
    sTrainer.trainerPic = class->pic;
    StringCopy(sTrainer.trainerName, sVillainNames[team][class - sVillainClasses[team]]);
    return size;
}

// A grunt's turn: in a hideout, among its trainers; on the road, among its
// team's grunts there, those on the route the team shares counted after
// its own (so a team's lines spread over the region: RogueText_PickRegion).
static u32 VillainTurn(u32 slot)
{
    struct RogueTeams tm;
    u8 t;
    u32 i;
    RogueGen_Teams(&sRegion, &tm);
    for (t = 0; t < tm.n; t++)
    {
        const struct RoguePlace *p;
        u32 k;
        if (SlotDungeon(slot) == tm.hideout[t])
            return slot - sRegion.dungeons[tm.hideout[t]].trainerBase;
        if (tm.hint[t] == 0xFF)
            continue;
        p = &sRegion.places[tm.hint[t]];
        if (slot < p->trainerBase || slot >= (u32)p->trainerBase + p->nTrainers)
            continue;
        k = slot - p->trainerBase;
        if ((tm.mask[t] >> k) & 1)
        {
            u32 n = 0;
            for (i = 0; i < k; i++)
                n += (tm.mask[t] >> i) & 1;
            return n;
        }
        if ((tm.guest[t] >> k) & 1)
            return __builtin_popcount(tm.mask[t - 1]);
    }
    return 0;
}

// A villain's line: intro, defeat (ROGUE_TEXT_*) or 2 for after, into dest.
static const u8 *VillainLine(u32 slot, u32 kind, u8 *dest, u32 size)
{
    u8 role, other, t, vrole, place = RogueGen_SlotPlace(&sRegion, slot);
    bool8 clash;
    struct RogueTextCtx c;
    struct RogueTextState st;
    s32 h, lair;
    t = VillainSlotTeam(slot, &role, &clash, &other);
    h = RogueGen_TeamHideout(&sRegion, t);
    if (kind > 3)
        kind = 2;
    if (role == RVROLE_ADMIN)
        vrole = RVILLAIN_ADMIN, place = h >= 0 ? sRegion.dungeons[h].host : 0xFF;
    else if (role == RVROLE_LEADER)
        vrole = RVILLAIN_LEADER, place = 0xFF;
    else if (h >= 0 && SlotDungeon(slot) == h)
    {
        // (On the bottom floor nobody sends the player further down.)
        vrole = RogueGen_FloorOfTrainer(&sRegion, h, slot) >= sRegion.dungeons[h].nFloors ? RVILLAIN_HIDEOUT_LAST : RVILLAIN_HIDEOUT;
        place = sRegion.dungeons[h].host;
    }
    else
        vrole = RVILLAIN_GRUNT;
    lair = RogueGen_TeamLeaderDungeon(&sRegion, t);
    if (vrole == RVILLAIN_LEADER && lair >= 0)
        place = sRegion.dungeons[lair].host;
    else if (vrole == RVILLAIN_LEADER && h >= 0)
        place = sRegion.dungeons[h].host; // at the bottom of his hideout
    Rogue_TextCtx(&c, &st, place == 0xFF ? -1 : place, RSPK_VILLAIN);
    c.team = t;
    c.team2 = other;
    return Rogue_BufferText(dest, size, RogueText_VillainLine(&c, vrole, kind, VillainTurn(slot)), &c), dest;
}

// A villain line that needs no trainer (the blockade, a lookout, the
// town's people) into STR_VAR_1, of team t.
static void VillainSay(u32 role, u32 kind, u32 k, s32 place, u8 t)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    Rogue_TextCtx(&c, &st, place, role == RVILLAIN_TOWN ? RSPK_FOLK : RSPK_VILLAIN);
    c.team = t;
    Rogue_BufferText(gStringVar1, sizeof(gStringVar1), RogueText_VillainLine(&c, role, kind, k), &c);
}

// The trainer at the bottom of team t's hideout: its admin, or TEAM
// ROCKET's BOSS himself where no legend's way is his (as in FireRed).
static u32 HideoutBossSlot(u32 t)
{
    return RogueGen_TeamBossInHideout(&sRegion, t) ? RogueGen_TeamLeaderSlot(t) : RogueGen_TeamAdminSlot(t);
}

// The flags that say team t's hideout is cleared (its admin beaten) and
// its leader beaten.
static u32 TeamClearedFlag(u32 t)
{
    return t == 0 ? FLAG_ROGUE_HIDEOUT_CLEARED : TRAINER_FLAGS_START + ROGUE_TRAINER(RogueGen_TeamAdminSlot(t));
}

static u32 TeamBeatenFlag(u32 t)
{
    return TRAINER_FLAGS_START + ROGUE_TRAINER(RogueGen_TeamLeaderSlot(t));
}

bool32 Rogue_TeamCleared(u32 t)
{
    EnsureRegion();
    return RogueGen_TeamHideout(&sRegion, t) >= 0 && FlagGet(TeamClearedFlag(t));
}

bool32 Rogue_TeamLeaderBeaten(u32 t)
{
    EnsureRegion();
    return RogueGen_TeamHideout(&sRegion, t) >= 0 && FlagGet(TeamBeatenFlag(t));
}

// The team of the hideout under a place (0xFF for none).
static u8 PlaceTeam(u32 place)
{
    s32 d = place < sRegion.nPlaces ? RogueGen_DungeonAt(&sRegion, place) : -1;
    if (d < 0 || sRegion.dungeons[d].host != place)
        return 0xFF;
    return RogueGen_HideoutTeam(&sRegion, d);
}

// The object of a blockade grunt, a hideout's lookout or admin, or a
// leader (slot is the team).
static void BuildVillainObject(struct ObjectEventTemplate *t, u32 kind, u32 slot, u32 place)
{
    u8 team = 0;
    switch (kind)
    {
    case ROBJ_GRUNT:
        // The main team's blockade, or the lookout of the hideout here.
        if (slot >= 2 && PlaceTeam(place) != 0xFF)
            team = PlaceTeam(place);
        t->graphicsId = sVillainClasses[TeamId(team)][slot & 1 ? VCLASS_GRUNT_F : VCLASS_GRUNT_M].gfx;
        t->script = Rogue_EventScript_VillainGrunt;
        t->flagId = TeamClearedFlag(team);
        break;
    case ROBJ_BOSS:
        // Gone once beaten: he leads the grunts out. (slot: the hideout.)
        team = RogueGen_HideoutTeam(&sRegion, slot);
        if (team == 0xFF)
            team = 0;
        t->graphicsId = VillainClass(HideoutBossSlot(team))->gfx;
        t->movementType = MOVEMENT_TYPE_FACE_DOWN;
        t->trainerType = TRAINER_TYPE_NORMAL;
        t->trainerRange_berryTreeId = 4;
        t->script = Rogue_EventScript_VillainAdmin;
        t->flagId = TeamClearedFlag(team);
        break;
    case ROBJ_VILLAIN_LEADER:
        team = slot < ROGUE_MAX_TEAMS ? slot : 0;
        t->graphicsId = sVillainClasses[TeamId(team)][VCLASS_LEADER].gfx;
        t->trainerType = TRAINER_TYPE_NORMAL;
        t->trainerRange_berryTreeId = 3;
        t->script = Rogue_EventScript_VillainLeader;
        t->flagId = FLAG_SYS_POKEDEX_GET; // see UpdateVillainObjects
        break;
    }
}

// As a map loads: the leader shows only after the hideout and until he is
// beaten. Maps are built ahead and kept, so this is set each time. Before,
// a flag that is always set in a run hides him.
static void UpdateVillainObjects(struct MapSlot *s)
{
    u32 i;
    if (s == NULL)
        return;
    for (i = 0; i < s->map.nObjs; i++)
    {
        const struct RogueObj *o = &s->map.objs[i];
        if (o->kind == ROBJ_VILLAIN_LEADER)
            s->objects[i].flagId = Rogue_TeamLeaderReady(o->slot) ? TeamBeatenFlag(o->slot) : FLAG_SYS_POKEDEX_GET;
        // A legend's seal while shut; the legend, where it appears once
        // the seal opens, only then.
        else if (o->kind == ROBJ_SEAL)
            s->objects[i].flagId = Rogue_SealShown(s->map.dungeon) ? FLAG_TEMP_15 : FLAG_SYS_POKEDEX_GET;
        // Whoever stands in a tunnel's way, until its key is held.
        else if (o->kind == ROBJ_TUNNEL_GUARD)
            s->objects[i].flagId = Rogue_TunnelOpen() ? FLAG_SYS_POKEDEX_GET : FLAG_TEMP_16;
        else if (o->kind == ROBJ_BOSS && s->map.floor && sRegion.dungeons[s->map.dungeon].boss == RBOSS_LEGEND)
            s->objects[i].flagId = RogueGen_SealAtLegend(&sRegion, s->map.dungeon) == TRUE && !Rogue_IsSealOpened(s->map.dungeon)
                                 ? FLAG_SYS_POKEDEX_GET : FLAG_ROGUE_BOSS_0 + s->map.dungeon;
    }
}

// Script special after a seal opens where the legend appears: the legend
// may show now. VAR_0x8006 is its local id, for addobject.
void Rogue_RevealLegend(void)
{
    u32 num = gSaveBlock1Ptr->location.mapNum, i;
    gSpecialVar_0x8006 = 0;
    if (!IsFloorMap(num) || !sSlotOf[FLOOR_SLOT])
        return;
    UpdateVillainObjects(&sSlots[sSlotOf[FLOOR_SLOT] - 1]);
    gSpecialVar_0x8006 = MapObjectOfKind(ROBJ_BOSS);
}

// ---------------------------------------------------------------------------
// Shipwrecks and power plants (RDUN_SHIP, RDUN_PLANT): the floor the player
// is on, its cabin doors and its Voltorb balls.

static const struct RogueMap *CurrentFloorMap(void)
{
    if (!IsFloorMap(gSaveBlock1Ptr->location.mapNum) || !sSlotOf[FLOOR_SLOT])
        return NULL;
    return &sSlots[sSlotOf[FLOOR_SLOT] - 1].map;
}

// The floor of a dungeon the player is on (1 the first), or -1.
s32 Rogue_GetFloorNumber(void)
{
    const struct RogueMap *m = CurrentFloorMap();
    return m == NULL ? -1 : m->floor;
}

// A cabin door's metatile, locked or open: the Abandoned Ship's door in a
// cabin's front wall (solid, walked into) or its door set in a corridor's
// roof. The floor's own blocks hold it open.
static void SetShipDoor(const struct RogueMap *m, u32 x, u32 y, bool32 open)
{
    u32 base = m->blocks[y * m->w + x] & MAPGRID_METATILE_ID_MASK;
    if (base == METATILE_InsideShip_IntactDoor_Bottom_Unlocked)
        MapGridSetMetatileIdAt(x + MAP_OFFSET, y + MAP_OFFSET, MAPGRID_COLLISION_MASK
                               | (open ? METATILE_InsideShip_IntactDoor_Bottom_Unlocked : METATILE_InsideShip_IntactDoor_Bottom_Locked));
    else if (base == METATILE_InsideShip_DoorIndent_Unlocked)
        // (Solid while locked, so the door is faced, not stepped on.)
        MapGridSetMetatileIdAt(x + MAP_OFFSET, y + MAP_OFFSET,
                               open ? METATILE_InsideShip_DoorIndent_Unlocked
                                    : METATILE_InsideShip_DoorIndent_Locked | MAPGRID_COLLISION_MASK);
}

// The key of a locked cabin door (RSIGN_LOCK + k): its item flag, and the
// key item it is.
static u32 ShipDoorKey(const struct RogueMap *m, u32 sign, u16 *item)
{
    struct RogueDungeonExtras e;
    RogueGen_DungeonExtras(&sRegion, m->dungeon, &e);
    *item = RogueGen_KeyItem(&sRegion, m->dungeon, e.keys + m->signs[sign].kind - RSIGN_LOCK);
    return FLAG_ROGUE_ITEM_0 + e.keys + m->signs[sign].kind - RSIGN_LOCK;
}

// Map script special as a shipwreck's floor loads (after a battle too): a
// cabin door stays locked until its key is used on it (Rogue_CheckShipDoor),
// which uses the key up, as Hoenn's are: so a door is open once its key ball
// is picked up and the key is gone from the bag. Nothing else is saved.
void Rogue_ApplyFloorState(void)
{
    const struct RogueMap *m;
    u32 i;
    EnsureRegion();
    if ((m = CurrentFloorMap()) == NULL)
        return;
    for (i = 0; i < m->nSigns; i++)
    {
        u16 item;
        u32 flag;
        if (m->signs[i].kind < RSIGN_LOCK || m->signs[i].kind >= RSIGN_HIDDEN)
            continue;
        flag = ShipDoorKey(m, i, &item);
        SetShipDoor(m, m->signs[i].x, m->signs[i].y, FlagGet(flag) && !CheckBagHasItem(item, 1));
    }
}

// Script special for a cabin door the player faces: VAR_RESULT 1 if its key
// was just found (the door opens, the key is used up), 2 if it is open, 0 if
// still locked. STR_VAR_1 is the key's name.
void Rogue_CheckShipDoor(void)
{
    const struct RogueMap *m;
    s16 x, y;
    u32 i;
    gSpecialVar_Result = 2;
    EnsureRegion();
    if ((m = CurrentFloorMap()) == NULL)
        return;
    GetXYCoordsOneStepInFrontOfPlayer(&x, &y);
    x -= MAP_OFFSET;
    y -= MAP_OFFSET;
    for (i = 0; i < m->nSigns; i++)
    {
        u16 item;
        u32 flag;
        if (m->signs[i].kind < RSIGN_LOCK || m->signs[i].kind >= RSIGN_HIDDEN || m->signs[i].x != x || m->signs[i].y != y)
            continue;
        flag = ShipDoorKey(m, i, &item);
        CopyItemName(item, gStringVar1);
        if (!FlagGet(flag))
        {
            gSpecialVar_Result = 0;
            return;
        }
        if (!CheckBagHasItem(item, 1))
            return;
        SetShipDoor(m, x, y, TRUE);
        RemoveBagItem(item, 1);
        gSpecialVar_Result = 1;
        return;
    }
}

// Script special for a power plant's Voltorb ball: a Voltorb, or an
// Electrode as the level allows, a little over the floor's wild Pokemon,
// as New Mauville's are. Creates it for dowildbattle.
void Rogue_PrepareDecoy(void)
{
    struct RoguePlace fp;
    s32 d;
    u32 level;
    u16 species;
    EnsureRegion();
    d = CurrentDungeon();
    if (d < 0 || !IsFloorMap(gSaveBlock1Ptr->location.mapNum))
        return;
    RogueGen_FloorPlace(&sRegion, d, gSaveBlock1Ptr->location.mapNum - MAP_NUM(MAP_ROGUE_FLOOR_1) + 1, &fp);
    level = RogueGen_WildLevel(fp.level) + 3;
    species = Rogue_EvolveForLevel(SPECIES_VOLTORB, level, FALSE);
    gSpecialVar_0x8004 = species;
    CreateScriptedWildMon(species, level, ITEM_NONE);
}

// Team t's leader waits before his legend once the team's hideout is
// cleared, until he is beaten.
bool32 Rogue_TeamLeaderReady(u32 t)
{
    return Rogue_TeamCleared(t) && !Rogue_TeamLeaderBeaten(t);
}

bool32 Rogue_VillainLeaderReady(void)
{
    return Rogue_TeamLeaderReady(0);
}

// The local id of the first object of a kind on the map the player is on,
// or the last one talked to.
static u16 MapObjectOfKind(u32 kind)
{
    const struct RogueMap *m = NULL;
    u32 num = gSaveBlock1Ptr->location.mapNum, i;
    if (IsFloorMap(num) && sSlotOf[FLOOR_SLOT])
        m = &sSlots[sSlotOf[FLOOR_SLOT] - 1].map;
    else if (num < sRegion.nPlaces)
        m = Rogue_GetPlaceMap(num);
    for (i = 0; m != NULL && i < m->nObjs; i++)
        if (m->objs[i].kind == kind)
            return i + 1;
    return gSpecialVar_LastTalked;
}

// Script special: the blockade's line, into STR_VAR_1, or a lookout's by
// a hideout's door (of the team under the town).
void Rogue_BufferVillainBlock(void)
{
    u32 num = gSaveBlock1Ptr->location.mapNum, i = gSpecialVar_LastTalked - 1;
    const struct RogueMap *m;
    EnsureRegion();
    m = num < sRegion.nPlaces ? Rogue_GetPlaceMap(num) : NULL;
    if (m != NULL && i < m->nObjs && m->objs[i].kind == ROBJ_GRUNT && m->objs[i].slot >= 2)
        VillainSay(RVILLAIN_LOOKOUT, 0, 0, num, PlaceTeam(num) == 0xFF ? 0 : PlaceTeam(num));
    else
        VillainSay(RVILLAIN_BLOCK, 0, m != NULL && i < m->nObjs ? m->objs[i].slot & 1 : 0, -1, 0);
}

// The stand-ins the scripts battle: the admin (ROGUE_SLOT_VILLAIN_ADMIN) of
// the hideout the player is in, and the leader (ROGUE_SLOT_VILLAIN_LEADER)
// before the legend whose way his team keeps. Other ids as they are.
u16 Rogue_ResolveVillainId(u16 trainerId)
{
    s32 d;
    u8 t;
    if (trainerId != ROGUE_TRAINER(ROGUE_SLOT_VILLAIN_ADMIN) && trainerId != ROGUE_TRAINER(ROGUE_SLOT_VILLAIN_LEADER))
        return trainerId;
    d = CurrentDungeon();
    if (d < 0)
        return trainerId;
    if (trainerId == ROGUE_TRAINER(ROGUE_SLOT_VILLAIN_ADMIN))
    {
        t = RogueGen_HideoutTeam(&sRegion, d);
        return t == 0xFF ? trainerId : ROGUE_TRAINER(HideoutBossSlot(t));
    }
    if (sRegion.dungeons[d].boss != RBOSS_LEGEND || ROGUE_GATE_KIND(sRegion.dungeons[d].gate) != RGATE_VILLAIN)
        return trainerId;
    t = ROGUE_GATE_SRC(sRegion.dungeons[d].gate);
    return t < RogueGen_TeamCount(&sRegion) ? ROGUE_TRAINER(RogueGen_TeamLeaderSlot(t)) : trainerId;
}

// After any trainer battle won: beating a hideout's boss clears it (the
// main team's blockade goes), beating a team's leader marks him beaten,
// whatever battle it was (the boss's own script, or two trainers at once).
void Rogue_VillainBattleWon(u16 trainerId)
{
    u8 t;
    if (!InRegion())
        return;
    EnsureRegion();
    for (t = 0; t < RogueGen_TeamCount(&sRegion); t++)
    {
        if (trainerId == ROGUE_TRAINER(HideoutBossSlot(t)))
        {
            s32 h = RogueGen_TeamHideout(&sRegion, t);
            if (t == 0)
                FlagSet(FLAG_ROGUE_HIDEOUT_CLEARED);
            FlagSet(TeamClearedFlag(t));
            if (RogueGen_TeamBossInHideout(&sRegion, t))
                FlagSet(TeamBeatenFlag(t));
            if (h >= 0)
                Rogue_RecordDungeonCleared(h);
        }
        if (trainerId == ROGUE_TRAINER(RogueGen_TeamLeaderSlot(t)))
            FlagSet(TeamBeatenFlag(t));
    }
}

// A trainer who battles only alone, never pulled into two trainers seeing
// the player at once: a team's admin or leader (their scripts' stand-ins,
// or their own slots).
bool32 Rogue_IsSoloTrainer(u16 trainerId)
{
    u8 role, other;
    bool8 clash;
    if (!InRegion())
        return FALSE;
    EnsureRegion();
    if (trainerId == ROGUE_TRAINER(ROGUE_SLOT_VILLAIN_ADMIN) || trainerId == ROGUE_TRAINER(ROGUE_SLOT_VILLAIN_LEADER))
        return TRUE;
    if (TrainerSlot(trainerId) < 0)
        return FALSE;
    VillainSlotTeam(TrainerSlot(trainerId), &role, &clash, &other);
    return role == RVROLE_ADMIN || role == RVROLE_LEADER;
}

// Script special, after an admin's battle: the hideout is cleared (the
// main team's blockade gone for good), the boss is marked beaten, the
// admin's parting line goes to STR_VAR_1 and his object to VAR_0x8004.
void Rogue_VillainAdminBeaten(void)
{
    s32 h;
    u8 t;
    EnsureRegion();
    h = CurrentDungeon();
    t = RogueGen_HideoutTeam(&sRegion, h);
    if (t == 0xFF)
        t = 0, h = RogueGen_Hideout(&sRegion);
    if (t == 0)
        FlagSet(FLAG_ROGUE_HIDEOUT_CLEARED);
    FlagSet(TeamClearedFlag(t)); // (the battle set it already)
    if (h >= 0)
        Rogue_RecordDungeonCleared(h); // a dungeon boss beaten, for the record too
    if (RogueGen_TeamBossInHideout(&sRegion, t))
        FlagSet(TeamBeatenFlag(t)); // (the battle set it already)
    gSpecialVar_0x8004 = MapObjectOfKind(ROBJ_BOSS);
    // The boss's parting words (an admin's with where the leader went),
    // then, once he has gone, the field says the team has left.
    {
        u8 *str = (u8 *)VillainLine(HideoutBossSlot(t), 2, gStringVar4, sizeof(gStringVar4));
        struct RogueTextCtx c;
        struct RogueTextState st;
        if (!RogueGen_TeamBossInHideout(&sRegion, t))
        {
            str = StringCopy(str + StringLength(str), COMPOUND_STRING("\p"));
            VillainLine(RogueGen_TeamAdminSlot(t), 3, str, gStringVar4 + sizeof(gStringVar4) - str);
        }
        Rogue_TextCtx(&c, &st, -1, RSPK_NONE);
        c.team = t;
        Rogue_BufferText(gStringVar1, sizeof(gStringVar1), RogueText_HideoutLeft(&c), &c);
    }
}

// Script special, after a leader's battle: his parting line and object.
void Rogue_VillainLeaderBeaten(void)
{
    u8 *str = gStringVar4;
    s32 dungeon;
    u8 t = 0;
    EnsureRegion();
    dungeon = CurrentDungeon();
    gSpecialVar_0x8004 = MapObjectOfKind(ROBJ_VILLAIN_LEADER);
    if (dungeon >= 0 && ROGUE_GATE_KIND(sRegion.dungeons[dungeon].gate) == RGATE_VILLAIN)
        t = ROGUE_GATE_SRC(sRegion.dungeons[dungeon].gate);
    FlagSet(TeamBeatenFlag(t)); // (the battle set it already)
    // He says what he wanted the legend for, and tells its tale.
    if (dungeon >= 0 && RogueGen_LoreAt(&sRegion, LORE_WAY_VILLAIN, dungeon) == dungeon
     && dungeon == RogueGen_TeamLeaderDungeon(&sRegion, t))
    {
        str = Rogue_BufferLore(str, dungeon, LORE_VILLAIN_WHY, LORE_WAYS, 120);
        str = Rogue_BufferLore(str, dungeon, LORE_STORY, LORE_WAY_VILLAIN, 500);
        str = StringCopy(str, COMPOUND_STRING("\p"));
    }
    VillainLine(RogueGen_TeamLeaderSlot(t), 2, str, gStringVar4 + sizeof(gStringVar4) - str);
}

// What the people of a hideout's town say about its team: TRUE with the
// line in STR_VAR_1 for the first two (one once the grunts are gone). In a
// region of two or three teams, the second tells of the feud.
bool32 Rogue_BufferVillainTownTalk(u32 place, u32 order)
{
    u8 t;
    bool32 cleared;
    EnsureRegion();
    t = PlaceTeam(place);
    if (t == 0xFF)
        return FALSE;
    cleared = Rogue_TeamCleared(t);
    if (order >= (cleared ? 1 : 2))
        return FALSE;
    VillainSay(RVILLAIN_TOWN, cleared ? 2 : (order == 1 && RogueGen_TeamCount(&sRegion) > 1 ? 3 : order), 0, place, t);
    return TRUE;
}

// The keys the main team's blockade adds once their admin is beaten.
u8 Rogue_VillainKeys(void)
{
    return FlagGet(FLAG_ROGUE_HIDEOUT_CLEARED) ? ROGUE_KEY_VILLAIN : 0;
}

static u32 CurrentGym(void)
{
    const struct RoguePlace *p = &sRegion.places[Rogue_GetCurrentTownPlace()];
    return p->gym < ROGUE_NUM_GYMS ? p->gym : 0;
}

// Script special: the gym leader's words after the battle, into STR_VAR_1:
// as the badge is won (VAR_0x8004 FALSE), or talked to again (TRUE).
void Rogue_BufferLeaderAfter(void)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    EnsureRegion();
    // RUN 0: the leaders' own after line, after the badge and when talked
    // to again (the normal one talks about their town and region).
    if (Rogue_Run0LeaderLine(sGymLeaders[CurrentGym()], RLEADER_AFTER, gStringVar1, sizeof(gStringVar1)))
        return;
    Rogue_TextCtx(&c, &st, sRegion.gymTown[CurrentGym()], RSPK_LEADER);
    c.gym = CurrentGym();
    Rogue_BufferText(gStringVar1, sizeof(gStringVar1), RogueText_LeaderAfter(&c, sGymLeaders[CurrentGym()], gSpecialVar_0x8004 != 0), &c);
}

// Script special: the badge of the current town's gym into STR_VAR_3.
void Rogue_BufferBadge(void)
{
    EnsureRegion();
    StringCopy(gStringVar3, sLeaders[sGymLeaders[CurrentGym()]].badge);
}

const u8 *Rogue_GetBadgeName(u32 gym)
{
    EnsureRegion();
    return sLeaders[sGymLeaders[gym % ROGUE_NUM_GYMS]].badge;
}

// What a badge lets the player do, said by the leader after the battle:
// the obstacle its field move clears, if any, then what the count of badges
// opens (Flash with two, Fly with six, Victory Road with eight).
static const u8 *const sKeyInfo[] =
{
    [ROBST_NONE]       = COMPOUND_STRING("That BADGE proves you beat me.\nWear it with pride!"),
    [ROBST_CUT]        = COMPOUND_STRING("With that BADGE, your POKéMON can\nCUT small trees out of the way."),
    [ROBST_ROCK_SMASH] = COMPOUND_STRING("With that BADGE, your POKéMON can\nuse ROCK SMASH on cracked rocks."),
    [ROBST_STRENGTH]   = COMPOUND_STRING("With that BADGE, your POKéMON can\nuse STRENGTH to push boulders."),
    [ROBST_SURF]       = COMPOUND_STRING("With that BADGE, your POKéMON can\nSURF across water. Face it and\lpress A."),
};

// Script special: the TM a gym leader gives with the badge, a move of the
// leader's type that suits how far along the gym is, into VAR_0x8000
// (RogueGen_GymTM).
void Rogue_PrepareLeaderTM(void)
{
    EnsureRegion();
    gSpecialVar_0x8000 = RogueGen_GymTM(&sRegion, CurrentGym(), sLeaders[sGymLeaders[CurrentGym()]].type);
}

// Script special after the leader's TM: the key the leader keeps for a
// legend's way (Rogue_PrepareGymKey), if any.
void Rogue_PrepareLeaderKey(void)
{
    EnsureRegion();
    Rogue_PrepareGymKey(CurrentGym());
}

// Script special: what the badge just won lets the player do, into
// gStringVar4.
void Rogue_BufferBadgeInfo(void)
{
    u32 key, n = Rogue_BadgeCount();
    u8 *str;
    EnsureRegion();
    key = sRegion.gymKey[CurrentGym()];
    str = StringCopy(gStringVar4, sKeyInfo[key < ARRAY_COUNT(sKeyInfo) ? key : ROBST_NONE]);
    if (n == 2)
        StringCopy(str, COMPOUND_STRING("\pWith two BADGES, FLASH lights up\ndark caves for you, too."));
    else if (n == 6)
        StringCopy(str, COMPOUND_STRING("\pWith six BADGES, your POKéMON can\nFLY to any town you have visited.\pOpen the MAP and press A on a town."));
    else if (n == ROGUE_NUM_GYMS)
        StringCopy(str, COMPOUND_STRING("\pThat's every BADGE in the region!\nVICTORY ROAD is open to you now.\pThe ELITE FOUR and the CHAMPION\nwait at the summit."));
}

// Elite Four member number VAR_0x8004 (0 to 3): name into STR_VAR_1, intro
// into gStringVar4, and the sprite into VAR_OBJ_GFX_ID_0.
void Rogue_BufferE4(void)
{
    const struct LeaderDef *leader;
    EnsureRegion();
    if (Rogue_Run0BufferE4(gSpecialVar_0x8004 % ROGUE_NUM_E4, FALSE))
        return;
    leader = &sLeaders[sE4[gSpecialVar_0x8004 % ROGUE_NUM_E4]];
    StringCopy(gStringVar1, leader->name);
    LeaderLine(sE4[gSpecialVar_0x8004 % ROGUE_NUM_E4], RLEADER_E4_INTRO, ROGUE_NUM_GYMS + gSpecialVar_0x8004 % ROGUE_NUM_E4,
               gStringVar4, sizeof(gStringVar4));
}

void Rogue_BufferE4After(void)
{
    u8 *str;
    EnsureRegion();
    if (Rogue_Run0BufferE4(gSpecialVar_0x8004 % ROGUE_NUM_E4, TRUE))
        return;
    LeaderLine(sE4[gSpecialVar_0x8004 % ROGUE_NUM_E4], RLEADER_E4_AFTER, ROGUE_NUM_GYMS + gSpecialVar_0x8004 % ROGUE_NUM_E4,
               gStringVar4, sizeof(gStringVar4));
    // The last of them hints that the CHAMPION is someone the player knows.
    if (gSpecialVar_0x8004 % ROGUE_NUM_E4 == ROGUE_NUM_E4 - 1)
    {
        struct RogueTextCtx c;
        struct RogueTextState st;
        str = gStringVar4 + StringLength(gStringVar4);
        str = StringCopy(str, COMPOUND_STRING("\p"));
        Rogue_TextCtx(&c, &st, sRegion.summit, RSPK_E4);
        Rogue_BufferText(str, gStringVar4 + sizeof(gStringVar4) - str, RogueText_E4LastHint(&c, WandererMet()), &c);
    }
}

// Script special: the gym guide's words for the current town's gym, into
// gStringVar4.
void Rogue_BufferGymGuide(void)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    EnsureRegion();
    Rogue_TextCtx(&c, &st, sRegion.gymTown[CurrentGym()], RSPK_FOLK);
    c.gym = CurrentGym();
    Rogue_BufferText(gStringVar4, sizeof(gStringVar4), RogueText_GymGuide(&c), &c);
}

// Script special: what the leader says as they hand over their TM
// (VAR_0x8004 FALSE) and after it (TRUE), into gStringVar4.
void Rogue_BufferLeaderTM(void)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    EnsureRegion();
    Rogue_TextCtx(&c, &st, sRegion.gymTown[CurrentGym()], RSPK_LEADER);
    c.gym = CurrentGym();
    Rogue_BufferText(gStringVar4, sizeof(gStringVar4), RogueText_LeaderTM(&c, gSpecialVar_0x8004 != 0), &c);
}

// Script special after a rival battle (VAR_0x8004 the battle, 1 to 4): what
// the rival says, into gStringVar4.
void Rogue_BufferRivalAfter(void)
{
    EnsureRegion();
    if (Rogue_Run0BufferRivalAfter())
        return;
    RivalLine((gSpecialVar_0x8004 - 1) & 3, RRIVAL_AFTER, gStringVar4, sizeof(gStringVar4));
}

// The rival's partner as it stands at the next (or current) rival battle:
// the starter, evolved for that battle's level, as their team builds it.
u16 Rogue_GetRivalPartner(void)
{
    u32 battle, level;
    EnsureRegion();
    for (battle = 0; battle < 3 && FlagGet(FLAG_ROGUE_RIVAL_1 + battle); battle++)
        ;
    level = sRegion.rivalLevel[battle];
    return EvolveForLevel(RivalStarter(), level, FALSE);
}

// ---------------------------------------------------------------------------
// The CHAMPION's room and the HALL OF FAME: the wanderer.

// Script special on entering either room: the CHAMPION's sprite.
void Rogue_SetChampionGraphics(void)
{
    EnsureRegion();
    VarSet(VAR_OBJ_GFX_ID_0, ChampionLook()->gfx);
}

// Script special: the CHAMPION's words (VAR_0x8004: RCHAMP_INTRO, _AFTER
// or _HOF) into gStringVar4.
void Rogue_BufferChampion(void)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    EnsureRegion();
    Rogue_TextCtx(&c, &st, sRegion.summit, RSPK_CHAMPION);
    Rogue_BufferText(gStringVar4, sizeof(gStringVar4), RogueText_ChampionLine(&c, gSpecialVar_0x8004, WandererMet()), &c);
}

// ---------------------------------------------------------------------------
// The wanderer on the road: in a town of their meetings (RogueCast), one of
// the townsfolk stands aside and the wanderer stands there, while the
// player holds the badges that meeting waits for and not yet the next
// one's. FLAG_ROGUE_WANDERER_0 + meeting marks a meeting had.

// The gifts at the second meeting, by look.
static const u16 sWandererGifts[RLOOK_COUNT] =
{
    [RLOOK_STEVEN] = ITEM_TM_STEEL_WING, // as Steven gives at the Granite Cave
    [RLOOK_WALLACE] = ITEM_SHELL_BELL,
    [RLOOK_ACE] = ITEM_QUICK_CLAW,
    [RLOOK_VETERAN] = ITEM_LUCKY_EGG,
};

// The meetings with the wanderer had this run, a bit each.
static u8 WandererMet(void)
{
    u32 k;
    u8 met = 0;
    for (k = 0; k < 4; k++)
        if (FlagGet(FLAG_ROGUE_WANDERER_0 + k))
            met |= 1 << k;
    return met;
}

// The meeting (0-3) the wanderer stands at now, in place, or -1.
static s32 WandererMeetingAt(u32 place)
{
    struct RogueCast cast;
    s32 m;
    u32 badges = Rogue_BadgeCount();
    RogueGen_Cast(&sRegion, &cast);
    m = RogueGen_WandererAt(&cast, place);
    if (m < 0 || badges < cast.wandererBadges[m])
        return -1;
    if (m < 3 && cast.wandererTowns[m + 1] != 0xFF && badges >= cast.wandererBadges[m + 1])
        return -1;
    return m;
}

// As a town loads: the wanderer, or the townsperson in their place.
static void UpdateCastObjects(struct MapSlot *s)
{
    s32 i, m;
    struct ObjectEventTemplate *t;
    if (s == NULL || s->map.floor || s->map.place >= sRegion.nPlaces || sRegion.places[s->map.place].kind != RPLACE_TOWN)
        return;
    // The wanderer stands in for a townsperson nobody special is (never the
    // reporters); outside a meeting's window that one is as BuildObjects
    // made them (a cached map may still show the wanderer).
    i = RogueText_WandererObj(&sRegion, &s->map);
    if (i < 0)
        return;
    t = &s->objects[i];
    m = WandererMeetingAt(s->map.place);
    if (m < 0)
    {
        t->x = s->map.objs[i].x;
        t->y = s->map.objs[i].y;
        t->graphicsId = Rogue_TownsfolkObjGfx(&s->map, i);
        if (s->map.objs[i].slot % 3 == 0)
        {
            t->movementType = MOVEMENT_TYPE_WANDER_AROUND;
            t->movementRangeX = t->movementRangeY = 1;
        }
        else
        {
            t->movementType = MOVEMENT_TYPE_LOOK_AROUND;
            t->movementRangeX = t->movementRangeY = 0;
        }
        t->script = Rogue_EventScript_Townsperson;
        return;
    }
    // They wait by the POKeMON CENTER's door, where nobody can miss them.
    {
        u8 x, y;
        if (RogueText_WandererSpot(&s->map, i, &x, &y))
        {
            t->x = x;
            t->y = y;
        }
    }
    t->graphicsId = ChampionLook()->gfx;
    t->movementType = MOVEMENT_TYPE_FACE_DOWN;
    t->movementRangeX = t->movementRangeY = 0;
    t->script = Rogue_EventScript_Wanderer;
}

// What a townsperson says while the wanderer waits in town, not yet met
// at this meeting: where to find them. TRUE with the line in STR_VAR_1 for
// the townsperson whose turn it is (RogueText_WandererPointerTurn).
bool32 Rogue_BufferWandererPointer(u32 place, u32 order)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    s32 m;
    EnsureRegion();
    if (place >= sRegion.nPlaces || sRegion.places[place].kind != RPLACE_TOWN)
        return FALSE;
    // The first townsperson talked to on each visit while the meeting
    // waits (a temporary flag: it clears as the map loads).
    m = WandererMeetingAt(place);
    if (m < 0 || FlagGet(FLAG_ROGUE_WANDERER_0 + m) || FlagGet(FLAG_ROGUE_POINTED))
        return FALSE;
    (void)order;
    FlagSet(FLAG_ROGUE_POINTED);
    Rogue_TextCtx(&c, &st, place, RSPK_FOLK);
    Rogue_BufferText(gStringVar1, sizeof(gStringVar1), RogueText_WandererPointer(&c, m, (WandererMet() & ((1 << m) - 1)) != 0), &c);
    return TRUE;
}

// Script special: what the wanderer says, into gStringVar4; the gift (if
// any, the first time at the second meeting) into VAR_0x8000.
void Rogue_BufferWanderer(void)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    struct RogueCast cast;
    s32 place = Rogue_GetCurrentPlace(), m;
    u8 met;
    bool8 gift;
    EnsureRegion();
    gSpecialVar_0x8000 = ITEM_NONE;
    m = place >= 0 ? WandererMeetingAt(place) : -1;
    if (m < 0)
        m = 0;
    RogueGen_Cast(&sRegion, &cast);
    Rogue_TextCtx(&c, &st, place, RSPK_WANDERER);
    if (FlagGet(FLAG_ROGUE_WANDERER_0 + m))
    {
        RogueText_Wanderer(&c, m, RWANDER_REPEAT, TRUE, FALSE, (char *)gStringVar4, sizeof(gStringVar4));
        Rogue_TextToGame(gStringVar4);
        return;
    }
    met = WandererMet() & ((1 << m) - 1);
    gift = (m == 1);
    FlagSet(FLAG_ROGUE_WANDERER_0 + m);
    RogueText_Wanderer(&c, m, RWANDER_TALK, met, gift, (char *)gStringVar4, sizeof(gStringVar4));
    Rogue_TextToGame(gStringVar4);
    if (gift)
        gSpecialVar_0x8000 = sWandererGifts[cast.wandererLook % RLOOK_COUNT];
}

// Script special after the gift: the wanderer's farewell, into gStringVar4.
void Rogue_BufferWandererBye(void)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    s32 place = Rogue_GetCurrentPlace(), m;
    EnsureRegion();
    m = place >= 0 ? WandererMeetingAt(place) : -1;
    Rogue_TextCtx(&c, &st, place, RSPK_WANDERER);
    RogueText_Wanderer(&c, m < 0 ? 0 : m, RWANDER_BYE, TRUE, FALSE, (char *)gStringVar4, sizeof(gStringVar4));
    Rogue_TextToGame(gStringVar4);
}

// Sets the Elite Four member's sprite before room VAR_0x8004 loads.
void Rogue_SetE4Graphics(void)
{
    EnsureRegion();
    VarSet(VAR_OBJ_GFX_ID_0, sLeaders[sE4[gSpecialVar_0x8004 % ROGUE_NUM_E4]].gfx);
    // RUN 0: the Fallen, the Previous Player, PROF. BIRCH and the Copy.
    if (Rogue_Run0E4Gfx(gSpecialVar_0x8004 % ROGUE_NUM_E4))
        VarSet(VAR_OBJ_GFX_ID_0, Rogue_Run0E4Gfx(gSpecialVar_0x8004 % ROGUE_NUM_E4));
}

// ---------------------------------------------------------------------------
// New game and script specials

// (v1.1) Every tree id must fit the save's berry trees.
STATIC_ASSERT(1 + ROGUE_BFARM_BERRY_BASE + ROGUE_BFARM_BERRIES <= BERRY_TREES_COUNT, RogueBerryTreeIdsFit);

void Rogue_PlantBerries(void)
{
    u32 i;
    EnsureRegion();
    for (i = 0; i < sRegion.nBerries; i++)
    {
        u32 rng = RogueRng_Hash(Rogue_GetSeed(), 9500 + i);
        u16 item = sBerries[RogueRng_Range(&rng, ARRAY_COUNT(sBerries))];
        PlantBerryTree(1 + i, ItemIdToBerryType(item), BERRY_STAGE_BERRIES, FALSE);
    }
    // A berry farm's trees (src/rogue_gen_signatures.h), in fruit.
    for (i = 0; i < ROGUE_FARM_BERRIES; i++)
        PlantBerryTree(1 + ROGUE_FARM_BERRY_BASE + i, ItemIdToBerryType(Rogue_FarmBerry(i)), BERRY_STAGE_BERRIES, FALSE);
    // A BERRY MASTER's garden (src/rogue_gen_features.h): the good ones.
    for (i = 0; i < ROGUE_GARDEN_BERRIES; i++)
    {
        static const u16 sGarden[ROGUE_GARDEN_BERRIES] = { ITEM_SITRUS_BERRY, ITEM_LUM_BERRY, ITEM_LEPPA_BERRY, ITEM_PERSIM_BERRY };
        PlantBerryTree(1 + ROGUE_GARDEN_BERRY_BASE + i, ItemIdToBerryType(sGarden[i]), BERRY_STAGE_BERRIES, FALSE);
    }
    // (v1.1) A berry farm route's field (src/rogue_gen_berry.h): a crop to
    // a row. Planted in every run; only a farm's map shows them.
    for (i = 0; i < ROGUE_BFARM_BERRIES; i++)
        PlantBerryTree(1 + ROGUE_BFARM_BERRY_BASE + i, ItemIdToBerryType(RogueGen_FarmRowBerry(Rogue_GetSeed(), i / ROGUE_BFARM_ROW)),
                       BERRY_STAGE_BERRIES, FALSE);
}

// The run starts in the professor's lab in the first town, in front of him
// (sSpecialInteriors[RINT_LAB] in rogue_town.c: he stands at 6,4), as a
// real game's first partner is chosen in the lab. Its house number is the
// warp id the house map is entered by.
#define LAB_START_X 6
#define LAB_START_Y 5

static s32 LabHouse(const struct RogueMap *town)
{
    u32 i;
    for (i = 0; i < town->nHouses; i++)
        if (town->interiors[i] == RINT_LAB)
            return i;
    return -1;
}

static void WarpToStart(void)
{
    const struct RogueMap *town = Rogue_GetPlaceMap(sRegion.start);
    s32 lab = LabHouse(town);
    if (lab >= 0)
        SetWarpDestination(ROGUE_GROUP, MAP_NUM(MAP_ROGUE_HOUSE), lab, -1, -1);
    else
        SetWarpDestination(ROGUE_GROUP, sRegion.start, WARP_ID_NONE, town->startX, town->startY);
    // Never fall back to a Hoenn map.
    gSaveBlock1Ptr->lastHealLocation.mapGroup = ROGUE_GROUP;
    gSaveBlock1Ptr->lastHealLocation.mapNum = sRegion.start;
    gSaveBlock1Ptr->lastHealLocation.warpId = WARP_ID_NONE;
    gSaveBlock1Ptr->lastHealLocation.x = town->startX;
    gSaveBlock1Ptr->lastHealLocation.y = town->startY;
    VarSet(VAR_ROGUE_TOWN, sRegion.start);
}

// The region the professor described in the introduction, picked before the
// game starts and used by Rogue_NewGame. Once the run starts it moves to
// sDescribedSeed, so the professor does not describe it twice. Both hold the
// seed plus one, and 0 for none.
static EWRAM_DATA u16 sPendingSeed = 0;
static EWRAM_DATA u16 sDescribedSeed = 0;
// A typed run code's POKeDEX mix for the pending region, plus one (0: the
// choice in UNLOCKS).
static EWRAM_DATA u8 sPendingMix = 0;

static u32 PickNewSeed(void)
{
    u32 seed;
    if (gRogueTestSeed != 0)
        return gRogueTestSeed;
    do
        seed = (Random32() ^ (gMain.vblankCounter1 << 8)) % ROGUE_REGION_COUNT;
    while (RogueGen_IsAwkwardRegionName(seed));
    return seed;
}

// The DAILY REGION's date (year from 2000, month, day), set while its region
// is the one the professor shows; 0 otherwise. NO or TYPE NAME on his map
// leave it.
static EWRAM_DATA u8 sDailyDate[3] = {0};

// mix: a run code's (or the DAILY REGION's) POKeDEX mix, or ROGUE_MIX_COUNT
// for the choice.
static void PrepareRegion(u32 seed, u32 mix)
{
    // A normal run, unless the menu asks for RUN 0 or ENDLESS after this.
    VarSet(VAR_ROGUE_RUN_KIND, ROGUE_RUN_KIND_NORMAL);
    sDailyDate[0] = sDailyDate[1] = sDailyDate[2] = 0;
    sPendingSeed = seed + 1;
    sPendingMix = mix < ROGUE_MIX_COUNT ? mix + 1 : 0;
    SetSavedSeed(seed);
    // The POKeDEX chosen in UNLOCKS (or the run code's), for the region he
    // shows.
    VarSet(VAR_ROGUE_DEX_MIX, mix < ROGUE_MIX_COUNT ? mix : Rogue_GetDexChoice());
    EnsureRegion();
}

static u32 PickAnotherSeed(void)
{
    u32 seed, tries = 0;
    do
        seed = PickNewSeed();
    while (sPendingSeed == seed + 1 && gRogueTestSeed == 0 && ++tries < 8);
    return seed;
}

// Called in the professor's speech, while the screen is black, so he can show
// the region before the run starts. The save is about to be cleared, so its
// seed vars are free to use. Another call grows another region.
void Rogue_PrepareNewRegion(void)
{
    PrepareRegion(PickAnotherSeed(), ROGUE_MIX_COUNT);
}

// The region of a typed name or run code (see CodeToSeed).
void Rogue_PrepareTypedRegion(const u8 *code)
{
    u32 mix, seed = CodeToSeed(code, &mix);
    PrepareRegion(seed, mix);
}

// The DAILY REGION (NEW RUN's left and right on the main menu): the
// region of the cartridge's clock's date (on an emulator, the device's),
// so everyone who plays that day gets the same one, offline. It is grown
// with the default POKeDEX mix, whatever UNLOCKS says, so its POKeMON are
// the same for everyone too.
bool32 Rogue_DailyAvailable(void)
{
    return !(RtcGetErrorStatus() & RTC_ERR_FLAG_MASK);
}

// A date's region: a hash of the date, skipping awkward names as new runs
// do. tools/roguemon/dailytest.py pins a few dates' regions.
u32 Rogue_DailySeed(u32 year, u32 month, u32 day)
{
    u32 seed = RogueRng_Hash(year * 10000 + month * 100 + day, 0xDA1F) % ROGUE_REGION_COUNT;
    while (RogueGen_IsAwkwardRegionName(seed))
        seed = (seed + 1) % ROGUE_REGION_COUNT;
    return seed;
}

void Rogue_PrepareDailyRegion(void)
{
    struct SiiRtcInfo rtc;
    u32 year, month, day;

    if (!Rogue_DailyAvailable())
    {
        Rogue_PrepareNewRegion();
        return;
    }
    RtcGetInfo(&rtc);
    year = ConvertBcdToBinary(rtc.year);
    month = ConvertBcdToBinary(rtc.month);
    day = ConvertBcdToBinary(rtc.day);
    PrepareRegion(Rogue_DailySeed(2000 + year, month, day), ROGUE_MIX_CLASSIC);
    sDailyDate[0] = year;
    sDailyDate[1] = month;
    sDailyDate[2] = day;
}

bool32 Rogue_IsDailyRegion(void)
{
    return sDailyDate[1] != 0 && sPendingSeed != 0;
}

// The DAILY REGION's date, as "OCT 20, 2026".
u8 *Rogue_DailyDateText(u8 *dest)
{
    static const u8 sMonths[12][4] =
    {
        _("JAN"), _("FEB"), _("MAR"), _("APR"), _("MAY"), _("JUN"),
        _("JUL"), _("AUG"), _("SEP"), _("OCT"), _("NOV"), _("DEC"),
    };
    dest = StringCopy(dest, sMonths[(sDailyDate[1] + 11) % 12]);
    *dest++ = CHAR_SPACE;
    dest = ConvertIntToDecimalStringN(dest, sDailyDate[2], STR_CONV_MODE_LEFT_ALIGN, 2);
    *dest++ = CHAR_COMMA;
    *dest++ = CHAR_SPACE;
    return ConvertIntToDecimalStringN(dest, 2000 + sDailyDate[0], STR_CONV_MODE_LEFT_ALIGN, 4);
}

// The main menu's RUN 0 row: the next new game is RUN 0, its own seed. The
// kind is carried through the new game with the record (Rogue_StashRecord).
void Rogue_PrepareRun0(void)
{
    // Not built here: nothing shows it before the run, and the new game
    // builds it again with the global mix (built twice, the black before
    // the Director's line was 5 to 6 seconds, not 2).
    sPendingSeed = ROGUE_RUN0_SEED + 1;
    sPendingMix = 0;
    SetSavedSeed(ROGUE_RUN0_SEED);
    VarSet(VAR_ROGUE_RUN_KIND, ROGUE_RUN_KIND_RUN0);
    VarSet(VAR_ROGUE_DEX_MIX, ROGUE_MIX_GLOBAL);
}

// ENDLESS (after MORE): a stub for the story. A new region of the ordinary
// kind, marked ENDLESS.
// Not built here, as RUN 0's is not: nothing shows it before the run, and
// the new game builds it behind the loading screen.
void Rogue_PrepareEndlessRun(void)
{
    u32 seed = PickAnotherSeed();
    sPendingSeed = seed + 1;
    sPendingMix = 0;
    SetSavedSeed(seed);
    VarSet(VAR_ROGUE_DEX_MIX, Rogue_GetDexChoice());
    VarSet(VAR_ROGUE_RUN_KIND, ROGUE_RUN_KIND_ENDLESS);
}

void Rogue_NewGame(void)
{
    u32 seed, kind, mix = 0;

    // The DAILY REGION keeps the mix it was shown with (sPendingMix).
    sDailyDate[1] = 0;
    if (sPendingSeed != 0)
    {
        seed = sPendingSeed - 1;
        sDescribedSeed = sPendingSeed;
        sPendingSeed = 0;
        mix = sPendingMix;
    }
    else
    {
        seed = PickNewSeed();
        sDescribedSeed = 0;
    }
    sPendingMix = 0;

    SetSavedSeed(seed);
    // The kind of run (v1.0), from the menu: RUN 0 is its seed, and only it.
    kind = VarGet(VAR_ROGUE_RUN_KIND);
    if (seed == ROGUE_RUN0_SEED)
        kind = ROGUE_RUN_KIND_RUN0;
    else if (kind == ROGUE_RUN_KIND_RUN0 || kind > ROGUE_RUN_KIND_ENDLESS)
        kind = ROGUE_RUN_KIND_NORMAL;
    VarSet(VAR_ROGUE_RUN_KIND, kind);
    VarSet(VAR_ROGUE_RUN0_ACT, 0);
    // The POKeDEX mix holds for the whole run, as the Hardcore rules do.
    // RUN 0 draws from every generation; a typed run code or the DAILY
    // REGION brings its own.
    if (kind == ROGUE_RUN_KIND_RUN0)
        VarSet(VAR_ROGUE_DEX_MIX, ROGUE_MIX_GLOBAL);
    else
        VarSet(VAR_ROGUE_DEX_MIX, mix != 0 ? mix - 1 : Rogue_GetDexChoice());
    VarSet(VAR_ROGUE_PROGRESS, ROGUE_PROGRESS_NO_STARTER);
    VarSet(VAR_ROGUE_ENCOUNTERS, 0);
    VarSet(VAR_ROGUE_ENCOUNTERS_2, 0);
    VarSet(VAR_ROGUE_ENCOUNTERS_3, 0);
    VarSet(VAR_ROGUE_SECOND_CHANCES, 0);
    VarSet(VAR_ROGUE_FALLEN, 0);
    ClearPlaceRecords();
    VarSet(VAR_ROGUE_E4, 0);
    VarSet(VAR_ROGUE_SEALS, 0);
    VarSet(VAR_ROGUE_RUN_STATE, ROGUE_RUN_ACTIVE);
    // Hardcore rules are chosen in the UNLOCKS menu and hold for the run.
    // RUN 0 has rules of its own.
    if (Rogue_IsUnlockOn(UNLOCK_HARDCORE) && kind != ROGUE_RUN_KIND_RUN0)
    {
        FlagSet(FLAG_ROGUE_HARDCORE);
        gSaveBlock2Ptr->optionsBattleStyle = OPTIONS_BATTLE_STYLE_SET;
    }
    // The professor's speech built this region to show it on his map
    // (Rogue_PrepareNewRegion): it is kept when the seed and the POKeDEX mix
    // match, as a build gives the same region every time (dexmixtest.py's
    // "built again"). That saved about 3 seconds of black after the
    // player shrank. The maps made for it are dropped all the same: they
    // were made under the old save's flags. RUN 0 and ENDLESS build here,
    // behind the loading screen (Rogue_LoadingBegin).
    if (RegionReadyFor(seed))
        ForgetMaps();
    EnsureRegion();
    // (The run log's flash write below takes most of what is left.)
    Rogue_LoadingStage(80, 45);
    // The LEGEND LOG marks this run's legends without building the region.
    Rogue_GetRecord()->runLegends = Rogue_RegionLegends(FALSE);

    // The player starts with running shoes and a National Pokedex.
    FlagSet(FLAG_SYS_B_DASH);
    FlagSet(FLAG_SYS_POKEDEX_GET);
    EnableNationalPokedex();
    // RUN 0 brings the Hall of Fame's teams and Terry's; every run is logged
    // in the Director's sector (src/rogue_run0.c).
    if (kind == ROGUE_RUN_KIND_RUN0)
        Rogue_Run0NewGame();
    else
        Rogue_RunLogStart();
    // ENDLESS keeps the party it was given (src/rogue_run0_story.c).
    Rogue_Run0StoryNewRun();
    WarpToStart();
    WarpIntoMap();
    // The warp id names the lab, so the player arrives at its door: step
    // up to the professor instead.
    if (gSaveBlock1Ptr->location.mapNum == MAP_NUM(MAP_ROGUE_HOUSE))
    {
        gSaveBlock1Ptr->pos.x = LAB_START_X;
        gSaveBlock1Ptr->pos.y = LAB_START_Y;
    }
    // The first map is made now, behind the loading screen, not in the
    // black after it.
    PrewarmSavedMap();
}

// ---------------------------------------------------------------------------
// The loading screen (src/rogue_loading.c), over the work that would
// otherwise freeze the screen: a new run, and CONTINUE.

static u8 sLoadingKind; // ROGUE_LOAD_*, while the screen is up (IWRAM)
// (Arrays, not compound literals: a 4-byte compound literal here was linked
// to the wrong address, so "..." came out as "BRN" less its first letter.)
static const u8 sText_SettingOutFor[] = _("Setting out for ");
static const u8 sText_BackTo[] = _("Back to ");
static const u8 sText_OnTo[] = _("On to ");
static const u8 sText_Dots[] = _("...");
// Until the region is made and the place known.
static const u8 sText_PickingUp[] = _("Picking up where you left off...");
static const u8 sText_Building[] = _("Building the region...");

static bool32 RegionReadyFor(u32 seed)
{
    u32 mix = VarGet(VAR_ROGUE_DEX_MIX);
    if (mix >= ROGUE_MIX_COUNT)
        mix = ROGUE_MIX_CLASSIC;
    return sRegionReady && sRegion.seed == seed && sRegionMix == mix;
}

// Drops the maps made for the region and the job making one, keeping the
// region itself.
static void ForgetMaps(void)
{
    AbandonJob();
    BuildHeaders();
    sTrainerCachedId = 0;
}

// Makes the map the save is in, and works out what its people say, as the
// field's load would.
static void PrewarmSavedMap(void)
{
    u32 mapNum = gSaveBlock1Ptr->location.mapNum, i;
    s32 place;
    if (!InRegion())
        return;
    Rogue_LoadingStage(90, 50);
    Rogue_GetMapHeader(gSaveBlock1Ptr->location.mapGroup, mapNum);
    // Out of doors, the maps across its edges too: the field copies their
    // edges when it loads (Rogue_EnsureMapBlocks).
    if (mapNum < sRegion.nPlaces)
    {
        const struct RoguePlace *p = &sRegion.places[mapNum];
        for (i = 0; i < p->nLinks; i++)
        {
            Rogue_LoadingStage(90 + 8 * (i + 1) / p->nLinks, 40);
            EnsurePlace(p->links[i].target);
        }
    }
    place = Rogue_GetCurrentPlace();
    if (place >= 0 && !IsFloorMap(mapNum))
        Rogue_WarmText(place);
}

// Where the player is headed, on the loading screen.
static void LoadingSayWhere(void)
{
    u8 line[64], *end;
    s32 place;
    if (!Rogue_LoadingActive())
        return;
    switch (sLoadingKind)
    {
    case ROGUE_LOAD_CONTINUE:
        place = Rogue_GetCurrentPlace();
        end = StringCopy(line, sText_BackTo);
        break;
    case ROGUE_LOAD_ENDLESS:
        place = sRegion.start;
        end = StringCopy(line, sText_OnTo);
        break;
    default:
        place = sRegion.start;
        end = StringCopy(line, sText_SettingOutFor);
        break;
    }
    if (place < 0 || place >= sRegion.nPlaces)
        return;
    end = Rogue_CopyPlaceName(end, place);
    StringCopy(end, sText_Dots);
    Rogue_LoadingSetLine(line);
}

// Puts the loading screen up for a new run (CB2_NewGame, before the save is
// cleared) or CONTINUE (CB2_ContinueSavedGame, the save loaded).
static void LoadingBegin(bool32 continuing)
{
    u32 seed = Rogue_GetSeed(), kind = VarGet(VAR_ROGUE_RUN_KIND);
    u8 title[ROGUE_REGION_NAME_LEN + PLAYER_NAME_LENGTH + 2];
    if (kind == ROGUE_RUN_KIND_RUN0 || seed == ROGUE_RUN0_SEED)
        sLoadingKind = ROGUE_LOAD_RUN0;
    else if (kind == ROGUE_RUN_KIND_ENDLESS)
        sLoadingKind = ROGUE_LOAD_ENDLESS;
    else
        sLoadingKind = continuing ? ROGUE_LOAD_CONTINUE : ROGUE_LOAD_NEW;
    Rogue_RegionName(title, seed);
    Rogue_LoadingShow(sLoadingKind, title, seed);
    if (RegionReadyFor(seed))
        LoadingSayWhere();
    else
        Rogue_LoadingSetLine(continuing ? sText_PickingUp : sText_Building);
}

void Rogue_LoadingNewGame(void)
{
    LoadingBegin(FALSE);
}

// CONTINUE: the region and the map the save is in are made behind the
// screen, then the field loads them from the cache.
void Rogue_LoadingContinue(void)
{
    if (!InRegion())
        return;
    LoadingBegin(TRUE);
    EnsureRegion();
    PrewarmSavedMap();
    Rogue_LoadingHide();
}

// Town name into STR_VAR_1.
void Rogue_BufferTownName(void)
{
    Rogue_CopyPlaceName(gStringVar1, Rogue_GetCurrentTownPlace());
}

// The next town along the main road from a place (the summit at the end),
// following links and caves toward higher place numbers.
u32 Rogue_NextTownFrom(u32 place)
{
    EnsureRegion();
    return RogueGen_NextTownFrom(&sRegion, place);
}

// Route name into STR_VAR_1 and the town it leads to into STR_VAR_2.
void Rogue_BufferRouteSign(void)
{
    s32 place = Rogue_GetCurrentPlace();
    if (place < 0)
        place = 0;
    {
        // A route's local name beside its number: ROUTE 12 THE LONG FORD.
        char nick[32];
        u8 *e = Rogue_CopyPlaceName(gStringVar1, place);
        if (RogueGen_RouteNick(&sRegion, place, nick))
        {
            u32 i;
            for (i = 0; nick[i]; i++)
                if (nick[i] >= 'a' && nick[i] <= 'z')
                    nick[i] -= 'a' - 'A';
            e = StringCopy(e, COMPOUND_STRING(": "));
            Rogue_CopyLoreAscii(e, nick);
        }
    }
    {
        u8 *end = Rogue_CopyPlaceName(gStringVar2, Rogue_NextTownFrom(place));
        u32 cave = sRegion.places[place].mouth;
        s32 d = cave < sRegion.nPlaces ? RogueGen_DungeonAt(&sRegion, cave) : -1;
        // The road goes on through the floors below the cave ahead, as
        // through Granite Cave's B1F: the sign says so.
        if (d >= 0 && sRegion.dungeons[d].host == cave && sRegion.dungeons[d].required
         && sRegion.dungeons[d].kind != RDUN_VICTORY && sRegion.places[cave].caveIn == place)
        {
            end = StringCopy(end, COMPOUND_STRING("\pThe way through "));
            end = Rogue_CopyPlaceName(end, cave);
            end = StringCopy(end, COMPOUND_STRING("\ngoes down its ladder."));
        }
        // (v1.1 towns2) A raft village or a crater town ahead: the sign says
        // what kind of town it is.
        {
            const struct RoguePlace *next = &sRegion.places[Rogue_NextTownFrom(place)];
            if (RogueGen_IsRaftTown(next))
                end = StringCopy(end, COMPOUND_STRING("\pThe town ahead floats on the sea,\nits houses on rafts."));
            else if (RogueGen_IsCraterTown(next))
                end = StringCopy(end, COMPOUND_STRING("\pThe town ahead lies in a crater\nringed by white cliffs."));
        }
        // v1.1: the weather the route is known for.
        switch (RogueGen_KnownWeather(&sRegion, place))
        {
        case RWEATHER_RAIN:
            StringCopy(end, COMPOUND_STRING("\pRain falls here most days."));
            break;
        case RWEATHER_STORM:
            StringCopy(end, COMPOUND_STRING("\pStorms often pass this way.\nMind the thunder!"));
            break;
        case RWEATHER_FOG:
            StringCopy(end, COMPOUND_STRING("\pA thick mist hangs here\nall year round."));
            break;
        }
    }
}

// Town into STR_VAR_1, leader into STR_VAR_2 and type into STR_VAR_3. At the
// summit there is no gym, and VAR_RESULT is FALSE.
void Rogue_BufferGymInfo(void)
{
    u32 place = Rogue_GetCurrentTownPlace();
    const struct RoguePlace *p = &sRegion.places[place];
    Rogue_CopyPlaceName(gStringVar1, place);
    if (p->gym >= ROGUE_NUM_GYMS)
    {
        CopyChampionName(gStringVar2);
        gSpecialVar_Result = FALSE;
        return;
    }
    StringCopy(gStringVar2, Rogue_GetGymLeaderName(p->gym));
    StringCopy(gStringVar3, gTypesInfo[Rogue_GetGymLeaderType(p->gym)].name);
    gSpecialVar_Result = TRUE;
}

// GYM SCOUTING: the gym guide's tip about the leader's team, in gStringVar4.
// VAR_RESULT is FALSE when the unlock is off.
void Rogue_BufferGymScouting(void)
{
    u32 place = Rogue_GetCurrentTownPlace();
    const struct RoguePlace *p = &sRegion.places[place];
    const struct Trainer *trainer;
    u8 *str = gStringVar4;
    u32 i, n;

    gSpecialVar_Result = FALSE;
    // Once the gym is won the guide only cheers.
    if (!Rogue_IsUnlockOn(UNLOCK_SCOUTING) || p->gym >= ROGUE_NUM_GYMS || FlagGet(FLAG_BADGE01_GET + p->gym))
        return;
    trainer = Rogue_GetTrainer(ROGUE_TRAINER(ROGUE_SLOT_LEADER_0 + p->gym));
    n = trainer->partySize;
    str = StringCopy(str, COMPOUND_STRING("I scouted the LEADER's team!\n"));
    for (i = 0; i < n; i++)
    {
        if (i > 0)
            str = StringCopy(str, i + 1 == n ? COMPOUND_STRING(" and ") : COMPOUND_STRING(", "));
        // Two names to a line.
        if (i == 2 || i == 4)
            str = StringCopy(str, COMPOUND_STRING("\l"));
        str = StringCopy(str, GetSpeciesName(trainer->party[i].species));
    }
    str = StringCopy(str, COMPOUND_STRING(".\pThe ace is "));
    str = StringCopy(str, GetSpeciesName(trainer->party[n - 1].species));
    str = StringCopy(str, COMPOUND_STRING(", at\nlevel "));
    str = ConvertIntToDecimalStringN(str, trainer->party[n - 1].lvl, STR_CONV_MODE_LEFT_ALIGN, 3);
    StringCopy(str, COMPOUND_STRING(". Train up first!"));
    gSpecialVar_Result = TRUE;
}

// The region's name into STR_VAR_1 and the build version into STR_VAR_2.
// STR_VAR_3: for a friend, the run's POKeDEX when it is not CLASSIC (a name
// gives the same land to all; the POKeMON come with the POKeDEX chosen).
void Rogue_BufferSeed(void)
{
    u32 mix = VarGet(VAR_ROGUE_DEX_MIX);
    Rogue_RegionName(gStringVar1, Rogue_GetSeed());
    Rogue_CopyAscii(gStringVar2, (const u8 *)ROGUEMON_VERSION);
    gStringVar3[0] = EOS;
    if (mix == ROGUE_MIX_NATIONAL || mix == ROGUE_MIX_GLOBAL)
    {
        u8 *str = StringCopy(gStringVar3, COMPOUND_STRING("\pIt's a "));
        str = StringCopy(str, Rogue_GetDexChoiceName(mix));
        StringCopy(str, COMPOUND_STRING(" DEX journey,\nso they should pick that too!"));
    }
}

// ---------------------------------------------------------------------------
// PROF. BIRCH's words (src/rogue_text_birch.h): what he says about a region,
// his lines in the lab, and his welcome back.

// Writes what the professor says about the region to dest. With full, as he
// shows it on the map in his speech, each box ending in a pause (full 2:
// the first run's first region, at length); without, for the lab when the
// map did not show it. Returns the end.
u8 *Rogue_DescribeRegion(u8 *dest, u32 full)
{
    struct RogueTextCtx c;
    struct RogueTextState st;

    EnsureRegion();
    Rogue_TextCtx(&c, &st, sRegion.start, RSPK_BIRCH);
    RogueText_BirchRegion(&c, full == 2 ? RBIRCH_DESC_MAP_FIRST : full ? RBIRCH_DESC_MAP : RBIRCH_DESC_LAB,
                          (char *)dest, full ? 800 : 0x100 - 1);
    dest = Rogue_TextToGame(dest);
    if (full)
        *dest++ = CHAR_PROMPT_CLEAR;
    *dest = EOS;
    return dest;
}

STATIC_ASSERT(BIRCH_LINE_WELCOME == RBIRCH_LAB_WELCOME && BIRCH_LINE_CHOOSE == RBIRCH_LAB_CHOOSE
           && BIRCH_LINE_GOOD_LUCK == RBIRCH_LAB_GOODLUCK && BIRCH_LINE_AFTER == RBIRCH_LAB_AFTER
           && BIRCH_LINE_NEWS == RBIRCH_LAB_NEWS, BirchLines);

// Script special: the professor's line VAR_0x8006 (BIRCH_LINE_*) in his lab,
// into gStringVar4. VAR_RESULT is FALSE if he has none.
void Rogue_BufferProfessor(void)
{
    struct RogueTextCtx c;
    struct RogueTextState st;

    EnsureRegion();
    Rogue_TextCtx(&c, &st, sRegion.start, RSPK_BIRCH);
    gSpecialVar_Result = RogueText_BirchLab(&c, gSpecialVar_0x8006, (char *)gStringVar4, sizeof(gStringVar4)) != 0;
    Rogue_TextToGame(gStringVar4);
}

// Script special: what the CHAMPION says walking the player into the HALL
// OF FAME, into gStringVar4.
void Rogue_BufferHallOfFame(void)
{
    struct RogueTextCtx c;
    struct RogueTextState st;

    EnsureRegion();
    Rogue_TextCtx(&c, &st, -1, RSPK_CHAMPION);
    RogueText_HallOfFame(&c, (char *)gStringVar4, sizeof(gStringVar4));
    Rogue_TextToGame(gStringVar4);
}

// The professor's welcome back on his platform, into dest (size bytes): it
// remembers the saved run, so it is said before a new region replaces it
// (in the speech, before Rogue_PrepareNewRegion). Only the saved region's
// name is needed, not its plan: planning it again took three seconds of
// black screen, so where the run ended is left to the epilogue. sRegion is
// grown again right after, and holds only that name meanwhile. FALSE if
// there is no run to remember.
bool32 Rogue_BirchWelcomeBack(u8 *dest, u32 size)
{
    const struct RogueRecord *record = Rogue_GetRecord();
    struct RogueBirchMemory m;
    struct RogueTextCtx c;
    char name[POKEMON_NAME_LENGTH * 2 + 1];
    u32 state = VarGet(VAR_ROGUE_RUN_STATE), seed = Rogue_GetSeed();

    // Back from RUN 0: the ending is all he remembers of it.
    if (seed == ROGUE_RUN0_SEED)
        return Rogue_Run0BirchEndingWelcome(dest, size);
    if (VarGet(VAR_ROGUE_META_RUNS) == 0 || seed >= ROGUE_REGION_COUNT)
        return FALSE;
    AbandonJob();
    sRegionReady = FALSE;
    sRegion.seed = seed;
    sRegion.nPlaces = 0;
    sRegion.nDungeons = 0;
    sRegion.start = 0;
    memset(&m, 0, sizeof(m));
    m.place = m.dungeon = 0xFF;
    m.runs = VarGet(VAR_ROGUE_META_RUNS);
    m.fallen = min(VarGet(VAR_ROGUE_FALLEN), 0xFF);
    // (A land played again, as a fixed seed does: he says so.)
    m.same = gRogueTestSeed != 0 && gRogueTestSeed == seed;
    if (state == ROGUE_RUN_ACTIVE)
    {
        // Left behind for a new land.
        m.outcome = RBIRCH_LEFT;
        m.badges = Rogue_BadgeCount();
    }
    else
    {
        m.outcome = state == ROGUE_RUN_WON ? RBIRCH_WON : RBIRCH_LOST;
        m.badges = record->lastBadges;
        // The fallen friend is this run's when it was lost with four badges.
        if (state == ROGUE_RUN_LOST && m.badges >= 4 && record->fallenSpecies != SPECIES_NONE && record->fallenName[0] != EOS)
        {
            Rogue_GameToAscii(name, record->fallenName, POKEMON_NAME_LENGTH);
            m.friendName = name;
        }
    }
    // The third win's teaser: once, he was "given" the new land (RUN 0).
    m.given = Rogue_TeaseGiven();
    // After a RUN 0 ending, once: his welcome remembers it.
    m.endingLine = Rogue_Run0BirchEndingLine();
    RogueText_Init(&c, &sRegion, 0xFF, RSPK_BIRCH);
    RogueText_BirchWelcomeBack(&c, &m, (char *)dest, size);
    Rogue_TextToGame(dest);
    return dest[0] != EOS;
}

// The short description into STR_VAR_1, unless the professor already gave
// it in the introduction. VAR_RESULT says whether there is one.
void Rogue_BufferRegionHints(void)
{
    gSpecialVar_Result = Rogue_GetSeed() + 1 != sDescribedSeed;
    if (gSpecialVar_Result)
        Rogue_DescribeRegion(gStringVar1, FALSE);
}

u16 Rogue_FirstForm(u16 species)
{
    return BaseForm(species);
}

// A region's roll from a list, from a stream of its own, skipping one
// species (another row's) when it can.
static u16 RollOwn(u32 salt, const u16 *list, u32 count, u16 skip)
{
    u32 rng = RogueRng_Hash(Rogue_GetSeed(), salt), tries;
    u16 species = list[0];
    for (tries = 0; tries < 32; tries++)
    {
        species = list[RogueRng_Range(&rng, count)];
        if (IsSpeciesEnabled(species) && species != skip)
            break;
    }
    return species;
}

// The species pool a row's left and right go through, and how long it is.
static const u16 *StarterPool(u32 kind, u32 *count)
{
    switch (kind)
    {
    case ROGUE_STARTER_GRASS:
    case ROGUE_STARTER_FIRE:
    case ROGUE_STARTER_WATER:
        *count = ARRAY_COUNT(sStarters[0]);
        return sStarters[kind];
    case ROGUE_STARTER_KIND_PARTNER:  *count = ARRAY_COUNT(sPartnerStarters); return sPartnerStarters;
    case ROGUE_STARTER_KIND_DRAGON:   *count = ARRAY_COUNT(sDragonStarters); return sDragonStarters;
    case ROGUE_STARTER_KIND_FOSSIL:   *count = ARRAY_COUNT(sFossilStarters); return sFossilStarters;
    case ROGUE_STARTER_KIND_EEVEE:    *count = ARRAY_COUNT(sEeveeStarter); return sEeveeStarter;
    case ROGUE_STARTER_KIND_BABY:     *count = ARRAY_COUNT(sBabyStarters); return sBabyStarters;
    case ROGUE_STARTER_KIND_RISING:   *count = ARRAY_COUNT(sRisingStarters); return sRisingStarters;
    case ROGUE_STARTER_KIND_REGIONAL: *count = ARRAY_COUNT(sRegionalStarters); return sRegionalStarters;
    }
    *count = 0;
    return NULL;
}

// The partner the region rolled. With the EEVEE row on, a region whose
// partner is Eevee offers another one instead, so no two rows are the same.
static u16 RegionPartner(void)
{
    if (sStarterExtra[0] == SPECIES_EEVEE && Rogue_IsUnlockOn(UNLOCK_EEVEE))
        return RollOwn(33400, sPartnerStarters, ARRAY_COUNT(sPartnerStarters), SPECIES_EEVEE);
    return sStarterExtra[0];
}

// A row's own pick for this region: its choice 0.
static u16 StarterRowDefault(u32 kind, bool32 *shiny)
{
    u16 value = SPECIES_NONE;

    *shiny = FALSE;
    switch (kind)
    {
    case ROGUE_STARTER_GRASS:
    case ROGUE_STARTER_FIRE:
    case ROGUE_STARTER_WATER:
        return sStarterBase[kind];
    case ROGUE_STARTER_KIND_PARTNER:
        return RegionPartner();
    case ROGUE_STARTER_KIND_DRAGON:
        return sStarterExtra[1];
    case ROGUE_STARTER_KIND_FOSSIL:
        // Its own stream, so earning it leaves the rest of the region as it was.
        return RollOwn(61, sFossilStarters, ARRAY_COUNT(sFossilStarters), SPECIES_NONE);
    case ROGUE_STARTER_KIND_EEVEE:
        return SPECIES_EEVEE;
    case ROGUE_STARTER_KIND_BABY:
        return RollOwn(33100, sBabyStarters, ARRAY_COUNT(sBabyStarters), RegionPartner());
    case ROGUE_STARTER_KIND_RISING:
        return RollOwn(33200, sRisingStarters, ARRAY_COUNT(sRisingStarters), RegionPartner());
    case ROGUE_STARTER_KIND_REGIONAL:
        return RollOwn(33300, sRegionalStarters, ARRAY_COUNT(sRegionalStarters), SPECIES_NONE);
    case ROGUE_STARTER_KIND_DEX:
    case ROGUE_STARTER_KIND_ANY:
        value = Rogue_StarterListStart(kind);
        *shiny = Rogue_StarterListShiny(kind, value);
        return value;
    case ROGUE_STARTER_KIND_FALLEN:
        value = Rogue_GetRecord()->fallenSpecies;
        break;
    case ROGUE_STARTER_KIND_CHAMPION:
        value = VarGet(VAR_ROGUE_META_CHAMPION_MON);
        break;
    }
    // The fallen friend and the Champion's partner come back in their first
    // form, and shiny if they were.
    *shiny = (value & ROGUE_CHAMPION_MON_SHINY) != 0;
    value &= ~ROGUE_CHAMPION_MON_SHINY;
    if (value == SPECIES_NONE || value >= NUM_SPECIES)
    {
        *shiny = FALSE;
        return sStarterBase[0];
    }
    return BaseForm(value);
}

// Left and right inside a row: the Grass, Fire and Water rows with the
// STARTER CASE, and every row of a pool with FREE CHOICE.
static bool32 StarterRowCycles(u32 kind)
{
    if (kind <= ROGUE_STARTER_WATER)
        return Rogue_IsUnlockOn(UNLOCK_STARTER_CASE) || Rogue_IsUnlockOn(UNLOCK_FREE_CHOICE);
    switch (kind)
    {
    case ROGUE_STARTER_KIND_PARTNER:
    case ROGUE_STARTER_KIND_DRAGON:
    case ROGUE_STARTER_KIND_FOSSIL:
    case ROGUE_STARTER_KIND_BABY:
    case ROGUE_STARTER_KIND_RISING:
    case ROGUE_STARTER_KIND_REGIONAL:
        return Rogue_IsUnlockOn(UNLOCK_FREE_CHOICE);
    }
    return FALSE;
}

u32 Rogue_StarterRowChoices(u32 kind)
{
    const u16 *pool;
    u32 count, i, n = 0;

    if (!StarterRowCycles(kind) || (pool = StarterPool(kind, &count)) == NULL)
        return 1;
    for (i = 0; i < count; i++)
        n += IsSpeciesEnabled(pool[i]);
    return n != 0 ? n : 1;
}

// A row's choice: 0 is the region's own pick, then the rest of the pool in
// order from it.
u16 Rogue_StarterRowSpecies(u32 kind, u32 choice, bool32 *shiny)
{
    const u16 *pool;
    u32 count, i, start = 0, k = 0;
    u16 first = StarterRowDefault(kind, shiny);

    if (choice == 0 || !StarterRowCycles(kind) || (pool = StarterPool(kind, &count)) == NULL)
        return first;
    for (i = 0; i < count; i++)
        if (pool[i] == first)
            start = i;
    for (i = 0; i < count; i++)
    {
        u16 species = pool[(start + i) % count];
        if (IsSpeciesEnabled(species) && k++ == choice)
            return species;
    }
    return first;
}

bool32 Rogue_StarterRowIsList(u32 kind)
{
    return kind == ROGUE_STARTER_KIND_DEX || kind == ROGUE_STARTER_KIND_ANY;
}

// The legend of the LEGEND LOG a species is, or RLEG_NONE.
static u32 LegendOfSpecies(u16 species)
{
    u32 legend;
    for (legend = RLEG_NONE + 1; legend < RLEG_COUNT; legend++)
        if (Rogue_LegendSpecies(legend) == species)
            return legend;
    return RLEG_NONE;
}

// Does a species belong in a list row: a first form of its line (a
// regional form too), never a Mega or other battle form. Legends only in
// ANY POKEMON and only once caught (the LEGEND LOG); mythicals, Ultra
// Beasts and paradoxes never. POKEDEX PICK has what was ever caught.
static bool32 InStarterList(u32 kind, u16 species, bool32 *shiny)
{
    const struct SpeciesInfo *info;
    bool32 regional;

    *shiny = FALSE;
    if (species == SPECIES_NONE || species >= NUM_SPECIES || !IsSpeciesEnabled(species))
        return FALSE;
    info = &gSpeciesInfo[species];
    regional = info->isAlolanForm || info->isGalarianForm || info->isHisuianForm || info->isPaldeanForm;
    // A regional form with form changes is a battle form (Galarian Zen
    // Mode), not a Pokemon of its own.
    if (GET_BASE_SPECIES_ID(species) != species && (!regional || info->formChangeTable != NULL))
        return FALSE;
    if (sHasPreEvo[species / 8] & (1 << (species % 8)))
        return FALSE;
    if (info->isTotem || info->isMegaEvolution || info->isPrimalReversion || info->isUltraBurst
     || info->isGigantamax || info->isTeraForm || SpeciesBst(species) == 0)
        return FALSE;
    if (info->isMythical || info->isUltraBeast || info->isParadox)
        return FALSE;
    if (info->isRestrictedLegendary || info->isSubLegendary)
    {
        u32 legend = LegendOfSpecies(species);
        if (kind != ROGUE_STARTER_KIND_ANY || legend == RLEG_NONE)
            return FALSE;
        return (Rogue_GetRecord()->legends >> legend) & 1;
    }
    if (kind == ROGUE_STARTER_KIND_DEX)
        return Rogue_WasCaught(species, shiny);
    return TRUE;
}

bool32 Rogue_StarterListShiny(u32 kind, u16 species)
{
    bool32 shiny;
    InStarterList(kind, species, &shiny);
    return shiny && kind == ROGUE_STARTER_KIND_DEX;
}

static bool32 NameBefore(u16 a, u16 b)
{
    s32 diff = StringCompare(GetSpeciesName(a), GetSpeciesName(b));
    return diff < 0 || (diff == 0 && a < b);
}

u16 *Rogue_BuildStarterList(u32 kind, u32 *count)
{
    u16 *list;
    u32 n = 0, s, lo, hi;
    bool32 shiny;

    EnsureRegion();
    for (s = 1; s < NUM_SPECIES; s++)
        n += InStarterList(kind, s, &shiny);
    *count = 0;
    list = Alloc((n + 1) * sizeof(u16));
    if (list == NULL)
        return NULL;
    // Insertion by name: a binary search for the place, then a shift.
    for (s = 1; s < NUM_SPECIES; s++)
    {
        if (!InStarterList(kind, s, &shiny))
            continue;
        lo = 0, hi = *count;
        while (lo < hi)
        {
            u32 mid = (lo + hi) / 2;
            if (NameBefore(list[mid], s))
                lo = mid + 1;
            else
                hi = mid;
        }
        memmove(&list[lo + 1], &list[lo], (*count - lo) * sizeof(u16));
        list[lo] = s;
        (*count)++;
    }
    return list;
}

// Where a list row starts: the last pick from it, or else the first by name.
u16 Rogue_StarterListStart(u32 kind)
{
    const struct RogueRecord *record = Rogue_GetRecord();
    u16 last = kind == ROGUE_STARTER_KIND_DEX ? record->lastDexSpecies : record->lastAnySpecies, best = SPECIES_NONE;
    u32 s;
    bool32 shiny;

    EnsureRegion();
    if (InStarterList(kind, last, &shiny))
        return last;
    for (s = 1; s < NUM_SPECIES; s++)
        if (InStarterList(kind, s, &shiny) && (best == SPECIES_NONE || NameBefore(s, best)))
            best = s;
    return best != SPECIES_NONE ? best : sStarterBase[0];
}

// POKEDEX PICK shows only once something it can offer was caught.
static bool32 AnyCaught(void)
{
    const struct RogueRecord *record = Rogue_GetRecord();
    u32 i, s;
    bool32 shiny;
    for (i = 0; i < ROGUE_DEX_BYTES && record->caught[i] == 0; i++)
        ;
    if (i == ROGUE_DEX_BYTES)
        return FALSE;
    for (s = 1; s < NUM_SPECIES; s++)
        if (InStarterList(ROGUE_STARTER_KIND_DEX, s, &shiny))
            return TRUE;
    return FALSE;
}

// The kinds of the starter menu's rows, by the unlocks that are on. Returns
// the number of rows.
u32 Rogue_GetStarterRows(u8 *kinds)
{
    u32 i, n = 0;
    for (i = 0; i < 3; i++)
        kinds[n++] = i;
    if (Rogue_IsUnlockOn(UNLOCK_EEVEE))
        kinds[n++] = ROGUE_STARTER_KIND_EEVEE;
    if (Rogue_IsUnlockOn(UNLOCK_PARTNERS))
        kinds[n++] = ROGUE_STARTER_KIND_PARTNER;
    if (Rogue_IsUnlockOn(UNLOCK_BABIES))
        kinds[n++] = ROGUE_STARTER_KIND_BABY;
    if (Rogue_IsUnlockOn(UNLOCK_RISING_STARS))
        kinds[n++] = ROGUE_STARTER_KIND_RISING;
    if (Rogue_IsUnlockOn(UNLOCK_DRAGONS))
        kinds[n++] = ROGUE_STARTER_KIND_DRAGON;
    if (Rogue_IsUnlockOn(UNLOCK_FOSSIL_STARTERS))
        kinds[n++] = ROGUE_STARTER_KIND_FOSSIL;
    if (Rogue_IsUnlockOn(UNLOCK_FAR_AWAY))
        kinds[n++] = ROGUE_STARTER_KIND_REGIONAL;
    if (Rogue_IsUnlockOn(UNLOCK_FALLEN_FRIEND) && Rogue_GetRecord()->fallenSpecies != SPECIES_NONE)
        kinds[n++] = ROGUE_STARTER_KIND_FALLEN;
    if (Rogue_IsUnlockOn(UNLOCK_CHAMP_PARTNER) && VarGet(VAR_ROGUE_META_CHAMPION_MON) != SPECIES_NONE)
        kinds[n++] = ROGUE_STARTER_KIND_CHAMPION;
    if (Rogue_IsUnlockOn(UNLOCK_POKEDEX_PICK) && AnyCaught())
        kinds[n++] = ROGUE_STARTER_KIND_DEX;
    if (Rogue_IsUnlockOn(UNLOCK_ANY_POKEMON))
        kinds[n++] = ROGUE_STARTER_KIND_ANY;
    return n;
}

u32 Rogue_GetStarterCount(void)
{
    u8 kinds[ROGUE_MAX_STARTER_ROWS];
    return Rogue_GetStarterRows(kinds);
}

void Rogue_StarterRowName(u8 *dest, u32 kind, u16 species)
{
    if (kind == ROGUE_STARTER_KIND_FALLEN)
        StringCopyN(dest, Rogue_GetRecord()->fallenName, POKEMON_NAME_LENGTH);
    else
        StringCopyN(dest, GetSpeciesName(species), POKEMON_NAME_LENGTH);
    dest[POKEMON_NAME_LENGTH] = EOS;
}

// What kind of starter a row is, shown with its picture. A regional form
// says where it is from.
const u8 *Rogue_StarterKindLabel(u32 kind, u16 species)
{
    static const u8 *const labels[ROGUE_STARTER_KIND_COUNT] =
    {
        [ROGUE_STARTER_GRASS]          = COMPOUND_STRING("GRASS STARTER"),
        [ROGUE_STARTER_FIRE]           = COMPOUND_STRING("FIRE STARTER"),
        [ROGUE_STARTER_WATER]          = COMPOUND_STRING("WATER STARTER"),
        [ROGUE_STARTER_KIND_PARTNER]   = COMPOUND_STRING("PARTNER"),
        [ROGUE_STARTER_KIND_DRAGON]    = COMPOUND_STRING("YOUNG DRAGON"),
        [ROGUE_STARTER_KIND_FOSSIL]    = COMPOUND_STRING("FOSSIL"),
        [ROGUE_STARTER_KIND_FALLEN]    = COMPOUND_STRING("OLD FRIEND"),
        [ROGUE_STARTER_KIND_CHAMPION]  = COMPOUND_STRING("YOUR CHAMPION"),
        [ROGUE_STARTER_KIND_EEVEE]     = COMPOUND_STRING("EEVEE"),
        [ROGUE_STARTER_KIND_BABY]      = COMPOUND_STRING("BABY POKéMON"),
        [ROGUE_STARTER_KIND_RISING]    = COMPOUND_STRING("RISING STAR"),
        [ROGUE_STARTER_KIND_REGIONAL]  = COMPOUND_STRING("FROM FAR AWAY"),
        [ROGUE_STARTER_KIND_DEX]       = COMPOUND_STRING("POKéDEX PICK"),
        [ROGUE_STARTER_KIND_ANY]       = COMPOUND_STRING("ANY POKéMON"),
    };
    if (species < NUM_SPECIES && kind != ROGUE_STARTER_KIND_ANY && kind != ROGUE_STARTER_KIND_DEX)
    {
        if (gSpeciesInfo[species].isAlolanForm)
            return COMPOUND_STRING("FROM ALOLA");
        if (gSpeciesInfo[species].isGalarianForm)
            return COMPOUND_STRING("FROM GALAR");
        if (gSpeciesInfo[species].isHisuianForm)
            return COMPOUND_STRING("FROM HISUI");
        if (gSpeciesInfo[species].isPaldeanForm)
            return COMPOUND_STRING("FROM PALDEA");
    }
    return labels[kind < ROGUE_STARTER_KIND_COUNT ? kind : 0];
}

// Sets up the starter menu. The rows' names go to gStringVar4, one every
// POKEMON_NAME_LENGTH + 1 bytes (the tests read them); the number of rows
// goes to VAR_0x8004. The fallen friend shows its nickname.
void Rogue_PrepareStarters(void)
{
    u8 kinds[ROGUE_MAX_STARTER_ROWS];
    u32 i, n;
    bool32 shiny;

    EnsureRegion();
    n = Rogue_GetStarterRows(kinds);
    for (i = 0; i < n; i++)
        Rogue_StarterRowName(gStringVar4 + i * (POKEMON_NAME_LENGTH + 1), kinds[i], Rogue_StarterRowSpecies(kinds[i], 0, &shiny));
    gSpecialVar_0x8004 = n;
}

// The run's starter, from VAR_ROGUE_STARTER (ROGUE_STARTER_VALUE). A run
// started before v0.13 kept only the row: 0 to 2 are the region's Grass,
// Fire and Water starters, and any other row is unknown (0xFF).
static u32 RunStarterKind(void)
{
    u32 value = VarGet(VAR_ROGUE_STARTER);
    if (value & ROGUE_STARTER_NEW)
        return (value >> ROGUE_STARTER_KIND_SHIFT) & 0xF;
    return value <= ROGUE_STARTER_WATER ? value : 0xFF;
}

u16 Rogue_GetRunStarterSpecies(void)
{
    u32 value = VarGet(VAR_ROGUE_STARTER);
    if (!FlagGet(FLAG_SYS_POKEMON_GET))
        return SPECIES_NONE;
    if (value & ROGUE_STARTER_NEW)
        return value & ROGUE_STARTER_SPECIES_MASK;
    if (value <= ROGUE_STARTER_WATER)
    {
        EnsureRegion();
        return sStarterBase[value];
    }
    return SPECIES_NONE;
}

u32 Rogue_GetRunStarterFirstType(void)
{
    u16 species = Rogue_GetRunStarterSpecies();
    return species != SPECIES_NONE && species < NUM_SPECIES ? GetSpeciesType(species, 0) : TYPE_NONE;
}

// The Grass, Fire or Water row this run's starter came from, or -1.
s32 Rogue_GetRunStarterType(void)
{
    u32 kind = RunStarterKind();
    if (!FlagGet(FLAG_SYS_POKEMON_GET) || kind > ROGUE_STARTER_WATER)
        return -1;
    return kind;
}

static u16 BaseForm(u16 species)
{
    u32 i;
    for (i = 0; i < 3; i++)
    {
        u16 pre = GetSpeciesPreEvolution(species);
        if (pre == SPECIES_NONE)
            break;
        species = pre;
    }
    return species;
}

// TRUE if the run's starter, or what it evolved into, is still in the party.
bool32 Rogue_PartyHasStarter(void)
{
    u16 starter = Rogue_GetRunStarterSpecies();
    u32 i;

    if (starter == SPECIES_NONE)
        return FALSE;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        if (!GetMonData(mon, MON_DATA_SANITY_HAS_SPECIES) || GetMonData(mon, MON_DATA_IS_EGG))
            continue;
        if (BaseForm(GetMonData(mon, MON_DATA_SPECIES)) == BaseForm(starter))
            return TRUE;
    }
    return FALSE;
}

// Appends a phrase to a message, breaking lines so they fit a text box:
// two lines to a page.
static u8 *AppendWrapped(u8 *msg, u8 *end, const u8 *phrase, u32 *lines)
{
    u8 *lineStart = end;
    while (lineStart > msg && lineStart[-1] != CHAR_NEWLINE && lineStart[-1] != CHAR_PROMPT_SCROLL
        && lineStart[-1] != CHAR_PROMPT_CLEAR)
        lineStart--;
    if (end > lineStart && GetStringWidth(FONT_NORMAL, lineStart, 0) + GetStringWidth(FONT_NORMAL, phrase, 0) > 200)
    {
        if (end[-1] == CHAR_SPACE)
            end--;
        *end++ = (++*lines % 2) ? CHAR_NEWLINE : CHAR_PROMPT_CLEAR;
        *end = EOS;
    }
    return StringCopy(end, phrase);
}

// The kit's contents as one message.
static void BufferKitText(const u8 *const *items, u32 count)
{
    u32 i, lines = 0;
    u8 *end = StringCopy(gStringVar2, COMPOUND_STRING("In the kit: "));
    for (i = 0; i < count; i++)
    {
        if (i + 1 == count && count > 1)
            end = AppendWrapped(gStringVar2, end, COMPOUND_STRING("and "), &lines);
        end = AppendWrapped(gStringVar2, end, items[i], &lines);
        end = StringCopy(end, i + 1 == count ? COMPOUND_STRING(".") : (i + 2 < count ? COMPOUND_STRING(", ") : COMPOUND_STRING(" ")));
    }
}

// The most useful egg move: the strongest reliable attack, or else the
// first one that is not an attack.
static u16 PickEggMove(u16 species)
{
    const u16 *moves = GetSpeciesEggMoves(species);
    u32 i, bestPower = 0;
    u16 best = MOVE_NONE, firstStatus = MOVE_NONE;

    for (i = 0; moves[i] != MOVE_UNAVAILABLE; i++)
    {
        u32 power = GetMovePower(moves[i]);
        if (GetMoveCategory(moves[i]) == DAMAGE_CATEGORY_STATUS)
        {
            if (firstStatus == MOVE_NONE)
                firstStatus = moves[i];
        }
        else if (power <= 90 && GetMoveAccuracy(moves[i]) >= 85 && power > bestPower)
        {
            best = moves[i];
            bestPower = power;
        }
    }
    return best != MOVE_NONE ? best : firstStatus;
}

// Teaches the egg move in an empty slot, or over the weakest move.
static void TeachEggMove(struct Pokemon *mon, u16 move)
{
    u32 i, slot = 0, weakest = 0xFFFF;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        u16 known = GetMonData(mon, MON_DATA_MOVE1 + i);
        u32 power;
        if (known == move)
            return;
        if (known == MOVE_NONE)
        {
            slot = i;
            break;
        }
        power = GetMoveCategory(known) == DAMAGE_CATEGORY_STATUS ? 0 : GetMovePower(known);
        if (power < weakest)
        {
            slot = i;
            weakest = power;
        }
    }
    SetMonMoveSlot(mon, move, slot);
}

// Is a species one of a row's choices (the menu's pick, checked).
static bool32 IsStarterChoice(u32 kind, u16 species, bool32 *shiny)
{
    u32 i, n;
    if (Rogue_StarterRowIsList(kind))
    {
        if (!InStarterList(kind, species, shiny))
            return FALSE;
        *shiny = Rogue_StarterListShiny(kind, species);
        return TRUE;
    }
    n = Rogue_StarterRowChoices(kind);
    for (i = 0; i < n; i++)
        if (Rogue_StarterRowSpecies(kind, i, shiny) == species)
            return TRUE;
    return FALSE;
}

// Gives the starter chosen in the menu (the row in VAR_RESULT, the species
// in VAR_0x8006) and the starting kit, and buffers the starter's name in
// STR_VAR_1 and the kit in STR_VAR_2. If the starter came with something
// special, STR_VAR_3 says so and VAR_0x8005 is TRUE. The type upgrades go
// by the starter's first type, whatever its row.
void Rogue_GiveStarter(void)
{
    u64 unlocks = Rogue_GetUnlocks();
    u32 choice = gSpecialVar_Result;
    u32 kind, type, nItems = 0, lines = 0;
    u16 species, item = ITEM_NONE, eggMove = MOVE_NONE;
    bool32 shiny, hidden = FALSE;
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][0];
    const u8 *items[14];
    u8 kinds[ROGUE_MAX_STARTER_ROWS];
    u8 *end;

    EnsureRegion();
    if (choice >= Rogue_GetStarterRows(kinds))
        choice = 0;
    kind = kinds[choice];
    species = gSpecialVar_0x8006;
    if (!IsStarterChoice(kind, species, &shiny))
        species = Rogue_StarterRowSpecies(kind, 0, &shiny);
    VarSet(VAR_ROGUE_STARTER, ROGUE_STARTER_VALUE(kind, species));
    if (kind == ROGUE_STARTER_KIND_ANY)
        Rogue_GetRecord()->lastAnySpecies = species;
    if (kind == ROGUE_STARTER_KIND_DEX)
        Rogue_GetRecord()->lastDexSpecies = species;
    type = GetSpeciesType(species, 0);
    if (Rogue_HasTypeUpgrade(UNLOCK_TYPE_CHARM, type) && type < NUMBER_OF_MON_TYPES)
        item = sTypeCharms[type];
    hidden = Rogue_HasTypeUpgrade(UNLOCK_HIDDEN_ABILITY, type) && gSpeciesInfo[species].abilities[2] != ABILITY_NONE;
    if (Rogue_HasTypeUpgrade(UNLOCK_EGG_MOVE, type))
        eggMove = PickEggMove(species);
    ScriptGiveMon(species, gRogueTestStarterLevel ? gRogueTestStarterLevel : 5, item);
    if (shiny)
        SetMonData(mon, MON_DATA_IS_SHINY, &shiny);
    if (kind == ROGUE_STARTER_KIND_FALLEN)
        SetMonData(mon, MON_DATA_NICKNAME, Rogue_GetRecord()->fallenName);
    if (hidden)
    {
        u8 abilityNum = 2;
        SetMonData(mon, MON_DATA_ABILITY_NUM, &abilityNum);
    }
    if (eggMove != MOVE_NONE)
        TeachEggMove(mon, eggMove);
    if (gRogueTestStarterLevel != 0)
    {
        // Attacks of types that nothing is immune to, so the playtest bot can
        // win any battle by pressing A.
        u8 ppBonuses = 0xFF;
        SetMonMoveSlot(mon, MOVE_BITE, 0);
        SetMonMoveSlot(mon, MOVE_WING_ATTACK, 1);
        SetMonMoveSlot(mon, MOVE_POWDER_SNOW, 2);
        SetMonMoveSlot(mon, MOVE_AERIAL_ACE, 3);
        SetMonData(mon, MON_DATA_PP_BONUSES, &ppBonuses);
        MonRestorePP(mon);
    }
    StringCopy(gStringVar1, GetSpeciesName(species));

    // What the starter came with, for the message after the kit.
    gSpecialVar_0x8005 = (kind == ROGUE_STARTER_KIND_FALLEN || item != ITEM_NONE || hidden || eggMove != MOVE_NONE);
    end = gStringVar3;
    *end = EOS;
    if (kind == ROGUE_STARTER_KIND_FALLEN)
    {
        u8 name[POKEMON_NAME_LENGTH + 1];
        StringCopyN(name, Rogue_GetRecord()->fallenName, POKEMON_NAME_LENGTH);
        name[POKEMON_NAME_LENGTH] = EOS;
        end = StringCopy(end, COMPOUND_STRING("It carries the name of your old\nfriend, "));
        end = StringCopy(end, name);
        end = StringCopy(end, COMPOUND_STRING(". "));
        lines = 1;
    }
    if (item != ITEM_NONE)
    {
        end = AppendWrapped(gStringVar3, end, COMPOUND_STRING("It holds "), &lines);
        end = AppendWrapped(gStringVar3, end, GetItemName(item), &lines);
        end = StringCopy(end, COMPOUND_STRING(". "));
    }
    if (hidden)
    {
        end = AppendWrapped(gStringVar3, end, COMPOUND_STRING("Its ability is "), &lines);
        end = AppendWrapped(gStringVar3, end, gAbilitiesInfo[gSpeciesInfo[species].abilities[2]].name, &lines);
        end = StringCopy(end, COMPOUND_STRING(". "));
    }
    if (eggMove != MOVE_NONE)
    {
        end = AppendWrapped(gStringVar3, end, COMPOUND_STRING("It knows "), &lines);
        end = AppendWrapped(gStringVar3, end, GetMoveName(eggMove), &lines);
        end = StringCopy(end, COMPOUND_STRING(". "));
    }
    if (end > gStringVar3 && end[-1] == CHAR_SPACE)
        end[-1] = EOS;

    AddBagItem(ITEM_POKE_BALL, 5);
    AddBagItem(ITEM_POTION, 5);
    FlagSet(FLAG_ROGUE_EXP_SHARE);
    AddBagItem(ITEM_OLD_ROD, 1);
    items[nItems++] = COMPOUND_STRING("5 POKé BALLS");
    if (unlocks & ROGUE_UNLOCK_KIT)
    {
        AddBagItem(ITEM_GREAT_BALL, 5);
        AddBagItem(ITEM_SUPER_POTION, 3);
        items[nItems++] = COMPOUND_STRING("5 GREAT BALLS");
    }
    items[nItems++] = COMPOUND_STRING("5 POTIONS");
    if (unlocks & ROGUE_UNLOCK_KIT)
        items[nItems++] = COMPOUND_STRING("3 SUPER POTIONS");
    if (Rogue_IsUnlockOn(UNLOCK_LUCKY_EGG))
    {
        AddBagItem(ITEM_LUCKY_EGG, 1);
        items[nItems++] = COMPOUND_STRING("a LUCKY EGG");
    }
    if (Rogue_IsUnlockOn(UNLOCK_BERRIES))
    {
        AddBagItem(ITEM_ORAN_BERRY, 3);
        AddBagItem(ITEM_SITRUS_BERRY, 2);
        items[nItems++] = COMPOUND_STRING("3 ORAN BERRIES");
        items[nItems++] = COMPOUND_STRING("2 SITRUS BERRIES");
    }
    if (unlocks & ROGUE_UNLOCK_VETERAN)
    {
        AddBagItem(ITEM_AMULET_COIN, 1);
        items[nItems++] = COMPOUND_STRING("an AMULET COIN");
    }
    if (Rogue_IsUnlockOn(UNLOCK_SHINY_CHARM))
    {
        AddBagItem(ITEM_SHINY_CHARM, 1);
        items[nItems++] = COMPOUND_STRING("a SHINY CHARM");
    }
    if (Rogue_IsUnlockOn(UNLOCK_EXPLORER))
    {
        AddBagItem(ITEM_ESCAPE_ROPE, 3);
        AddBagItem(ITEM_MAX_REPEL, 1);
        items[nItems++] = COMPOUND_STRING("3 ESCAPE ROPES");
        items[nItems++] = COMPOUND_STRING("a MAX REPEL");
    }
    if (Rogue_IsUnlockOn(UNLOCK_MASTER_BALL))
    {
        AddBagItem(ITEM_MASTER_BALL, 1);
        items[nItems++] = COMPOUND_STRING("a MASTER BALL");
    }
    if (Rogue_IsUnlockOn(UNLOCK_DOWSING))
    {
        AddBagItem(ITEM_DOWSING_MACHINE, 1);
        items[nItems++] = COMPOUND_STRING("a DOWSING MACHINE");
    }
    items[nItems++] = COMPOUND_STRING("an OLD ROD");
    BufferKitText(items, nItems);
}

// ---------------------------------------------------------------------------
// Spare starters

// With SPARE STARTERS on, the two starters the player did not pick wait in
// the first and fourth gym towns, as Yellow hands out Kanto's three. The
// first gym town gets the one that hits its leader hard, if only one does,
// so a player who keeps losing there gets the answer to that gym. Returns 0
// or 1 for the gift a town holds, or -1.
s32 Rogue_GetSpareGift(u32 place)
{
    EnsureRegion();
    // RUN 0: the aide is gone (and a Lv. 5 starter is no help there).
    if (Rogue_IsRun0())
        return -1;
    if (!Rogue_IsUnlockOn(UNLOCK_SPARE_STARTERS) || !FlagGet(FLAG_SYS_POKEMON_GET) || place >= sRegion.nPlaces)
        return -1;
    if (sRegion.places[place].kind != RPLACE_TOWN)
        return -1;
    if (sRegion.places[place].gym == 0)
        return 0;
    if (sRegion.places[place].gym == 3)
        return 1;
    return -1;
}

// The region's Grass, Fire and Water starters the player did not pick. For
// a starter from another row, two of the three: not one of its own first
// type, else not the one the first gym hits hard, else not Water.
static u16 SpareStarter(u32 which)
{
    s32 picked = Rogue_GetRunStarterType();
    enum Type gym1 = Rogue_GetGymLeaderType(0);
    u16 first, second;

    if (picked < 0)
    {
        u32 i, drop = 2, type = Rogue_GetRunStarterFirstType();
        for (i = 3; i-- > 0;)
            if (HitsHard(gym1, GetSpeciesType(sStarterBase[i], 0)))
                drop = i;
        for (i = 0; i < 3; i++)
            if (GetSpeciesType(sStarterBase[i], 0) == type)
                drop = i;
        picked = drop;
    }
    first = sStarterBase[(picked + 1) % 3], second = sStarterBase[(picked + 2) % 3];

    if (CountersType(second, gym1) && !CountersType(first, gym1))
    {
        u16 t = first;
        first = second;
        second = t;
    }
    return which == 0 ? first : second;
}

// Script special: the spare starter of the town the player is in. Sets
// VAR_RESULT to TRUE if it is still waiting, with its species in VAR_0x8000,
// its level in VAR_0x8001 and its name in STR_VAR_1.
void Rogue_PrepareSpareStarter(void)
{
    u32 place = Rogue_GetCurrentTownPlace();
    s32 which = Rogue_GetSpareGift(place);
    u32 level;

    gSpecialVar_Result = FALSE;
    if (which < 0 || FlagGet(FLAG_ROGUE_SPARE_1 + which))
        return;
    // About as strong as what lives on the routes nearby.
    level = sRegion.places[place].level > 8 ? sRegion.places[place].level - 3 : 5;
    gSpecialVar_0x8000 = SpareStarter(which);
    gSpecialVar_0x8001 = level;
    StringCopy(gStringVar1, GetSpeciesName(gSpecialVar_0x8000));
    gSpecialVar_Result = TRUE;
}

void Rogue_MarkSpareStarterGiven(void)
{
    s32 which = Rogue_GetSpareGift(Rogue_GetCurrentTownPlace());
    if (which >= 0)
        FlagSet(FLAG_ROGUE_SPARE_1 + which);
}

// ---------------------------------------------------------------------------
// Hardcore rules

bool32 Rogue_IsHardcore(void)
{
    return FlagGet(FLAG_ROGUE_HARDCORE);
}

// The level cap: the next gym leader's ace, then each Elite Four member's
// and the Champion's.
u32 Rogue_GetLevelCap(void)
{
    u32 progress = VarGet(VAR_ROGUE_PROGRESS);
    u32 badges = progress < ROGUE_PROGRESS_BADGE_1 ? 0 : progress - ROGUE_PROGRESS_BADGE_1 + 1;
    u32 e4 = VarGet(VAR_ROGUE_E4);

    EnsureRegion();
    if (badges < ROGUE_NUM_GYMS)
        return sRegion.aceLevel[badges];
    if (e4 < ROGUE_NUM_E4)
        return sRegion.e4Level[e4];
    return sRegion.championLevel;
}

// Script special: the Hardcore rules and the first cap in STR_VAR_1.
// VAR_RESULT is FALSE when the run is not Hardcore.
void Rogue_BufferHardcoreRules(void)
{
    gSpecialVar_Result = Rogue_IsHardcore();
    ConvertIntToDecimalStringN(gStringVar1, Rogue_GetLevelCap(), STR_CONV_MODE_LEFT_ALIGN, 3);
}

// The rival's way off the map after a battle, as a movement script for
// applymovement: fast steps, then the end.
#define RIVAL_EXIT_STEPS 20
EWRAM_DATA u8 gRogueRivalExitMovement[RIVAL_EXIT_STEPS + 1] = {0};

static const u8 sWalkFast[4] =
{
    [RDIR_NORTH] = MOVEMENT_ACTION_WALK_FAST_UP,
    [RDIR_EAST]  = MOVEMENT_ACTION_WALK_FAST_RIGHT,
    [RDIR_SOUTH] = MOVEMENT_ACTION_WALK_FAST_DOWN,
    [RDIR_WEST]  = MOVEMENT_ACTION_WALK_FAST_LEFT,
};
static const s8 sExitDX[4] = { 0, 1, 0, -1 };
static const s8 sExitDY[4] = { -1, 0, 1, 0 };

// Ground the rival can walk on: no wall, water, ledge or anyone standing there.
static bool32 RivalCanStep(s32 x, s32 y)
{
    u32 elevation;
    if (MapGridGetCollisionAt(x + MAP_OFFSET, y + MAP_OFFSET))
        return FALSE;
    elevation = MapGridGetElevationAt(x + MAP_OFFSET, y + MAP_OFFSET);
    if (elevation != 0 && elevation != 3 && elevation != 4)
        return FALSE;
    if (MetatileBehavior_IsJumpSouth(MapGridGetMetatileBehaviorAt(x + MAP_OFFSET, y + MAP_OFFSET)))
        return FALSE;
    return GetObjectEventIdByXY(x + MAP_OFFSET, y + MAP_OFFSET) == OBJECT_EVENTS_COUNT;
}

// Script special after a rival battle. Like May after Emerald's battles, the
// rival walks off: on along the main road (VAR_0x8005 FALSE) or back the way
// the player came (TRUE). Sets VAR_0x8004 to the rival's local id, and
// fills gRogueRivalExitMovement with the steps until she is off the screen
// (or reaches the way out). VAR_RESULT is FALSE when there is no clear way,
// and the script fades her out instead.
//
// The id cannot come from VAR_LAST_TALKED: when a trainer walks up to the
// player, the expansion only sets it from the trainerbattle's own local id,
// which trainerbattle_single leaves as LOCALID_NONE.
void Rogue_PrepareRivalExit(void)
{
    const struct RoguePlace *p;
    const struct RogueMap *m;
    struct ObjectEvent *rival = NULL, *player = &gObjectEvents[gPlayerAvatar.objectEventId];
    u32 i, w, h, place = sCurrentPlace, head = 0, tail = 0, n;
    s32 rx, ry, tx = -1, ty = -1, px, py, x, y;
    bool32 back = gSpecialVar_0x8005;
    u8 *from;
    u16 *queue;

    gSpecialVar_Result = FALSE;
    gSpecialVar_0x8004 = LOCALID_NONE;
    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
    {
        struct ObjectEvent *o = &gObjectEvents[i];
        if (o->active && !o->isPlayer
         && (o->graphicsId == OBJ_EVENT_GFX_RIVAL_MAY_NORMAL || o->graphicsId == OBJ_EVENT_GFX_RIVAL_BRENDAN_NORMAL))
        {
            rival = o;
            break;
        }
    }
    if (rival == NULL)
        return;
    gSpecialVar_0x8004 = rival->localId;
    if (!InRegion() || place >= sRegion.nPlaces)
        return;
    p = &sRegion.places[place];
    m = Rogue_GetPlaceMap(place);
    if (m == NULL)
        return;
    w = m->w;
    h = m->h;
    // Where she heads: the way on along the main road (a link to a later
    // place, or the cave mouth), or back to an earlier place.
    for (i = 0; i < p->nLinks; i++)
    {
        const struct RogueLink *l = &p->links[i];
        bool32 later = l->target > place && !sRegion.places[l->target].side;
        if (back ? l->target < place : later)
        {
            tx = l->dir == RDIR_WEST ? 0 : l->dir == RDIR_EAST ? (s32)w - 1 : l->pos;
            ty = l->dir == RDIR_NORTH ? 0 : l->dir == RDIR_SOUTH ? (s32)h - 1 : l->pos;
            break;
        }
    }
    if (tx < 0 && !back)
    {
        for (i = 0; i < m->nWarps; i++)
        {
            if (m->warps[i].kind == RWARP_CAVE)
            {
                tx = m->warps[i].x;
                ty = m->warps[i].y;
            }
        }
    }
    if (tx < 0)
        return;
    rx = rival->currentCoords.x - MAP_OFFSET;
    ry = rival->currentCoords.y - MAP_OFFSET;
    px = player->currentCoords.x - MAP_OFFSET;
    py = player->currentCoords.y - MAP_OFFSET;
    // A search back from the target, so each block knows its next step.
    from = AllocZeroed(w * h);
    queue = Alloc(w * h * sizeof(u16));
    if (from == NULL || queue == NULL)
    {
        Free(from);
        Free(queue);
        return;
    }
    from[ty * w + tx] = 5;
    queue[tail++] = ty * w + tx;
    while (head < tail && !from[ry * w + rx])
    {
        u32 cur = queue[head++], k;
        s32 cx = cur % w, cy = cur / w;
        for (k = 0; k < 4; k++)
        {
            s32 nx = cx + sExitDX[k], ny = cy + sExitDY[k];
            if (nx < 0 || ny < 0 || nx >= (s32)w || ny >= (s32)h || from[ny * w + nx])
                continue;
            if (!(nx == rx && ny == ry) && !RivalCanStep(nx, ny))
                continue;
            from[ny * w + nx] = 1 + ((k + 2) & 3); // the step back toward cur
            queue[tail++] = ny * w + nx;
        }
    }
    // Follow it until she is off the screen (seven blocks across, five up or
    // down from the player) or at the way out.
    n = 0;
    x = rx;
    y = ry;
    if (from[ry * w + rx])
    {
        while (n < RIVAL_EXIT_STEPS && from[y * w + x] != 5)
        {
            u32 k = from[y * w + x] - 1;
            gRogueRivalExitMovement[n++] = sWalkFast[k];
            x += sExitDX[k];
            y += sExitDY[k];
            if (x - px > 8 || px - x > 8 || y - py > 6 || py - y > 6)
                break;
        }
    }
    Free(from);
    Free(queue);
    gRogueRivalExitMovement[n] = MOVEMENT_ACTION_STEP_END;
    gSpecialVar_Result = n >= 2;
}

// RIVAL'S GIFT: the held item the rival gives at the foot of Victory Road,
// in VAR_0x8000. VAR_RESULT is FALSE when there is none to give.
void Rogue_PrepareRivalGift(void)
{
    EnsureRegion();
    // A strong held item the run has not handed out elsewhere.
    gSpecialVar_0x8000 = RogueGen_ItemFor(&sRegion, RITEMS_RIVAL, 0, NULL, ItemRules());
    if (gSpecialVar_0x8000 == ITEM_NONE)
        gSpecialVar_0x8000 = ITEM_LEFTOVERS;
    gSpecialVar_Result = Rogue_IsUnlockOn(UNLOCK_RIVAL_GIFT) && !FlagGet(FLAG_ROGUE_RIVAL_GIFT);
    // RUN 0's Previous Player only says "...", and gives nothing.
    if (Rogue_IsRun0())
        gSpecialVar_Result = FALSE;
}

// ---------------------------------------------------------------------------
// Fossils

// The fossils of every generation, and what each one brings back.
static const u16 sFossils[][2] =
{
    { ITEM_HELIX_FOSSIL, SPECIES_OMANYTE },
    { ITEM_DOME_FOSSIL,  SPECIES_KABUTO },
    { ITEM_ROOT_FOSSIL,  SPECIES_LILEEP },
    { ITEM_CLAW_FOSSIL,  SPECIES_ANORITH },
    { ITEM_SKULL_FOSSIL, SPECIES_CRANIDOS },
    { ITEM_ARMOR_FOSSIL, SPECIES_SHIELDON },
    { ITEM_COVER_FOSSIL, SPECIES_TIRTOUGA },
    { ITEM_PLUME_FOSSIL, SPECIES_ARCHEN },
    { ITEM_JAW_FOSSIL,   SPECIES_TYRUNT },
    { ITEM_SAIL_FOSSIL,  SPECIES_AMAURA },
};

static u32 RegionFossil(void)
{
    return RogueRng_Hash(sRegion.seed, 9400) % ARRAY_COUNT(sFossils);
}

// The other fossil of the fossil cave's choice room.
static u32 SecondFossil(void)
{
    return (RegionFossil() + 1 + RogueRng_Hash(sRegion.seed, 9401) % (ARRAY_COUNT(sFossils) - 1)) % ARRAY_COUNT(sFossils);
}

// The cave on the main road before the fourth gym holds the fossil.
static s32 FossilCave(void)
{
    return RogueGen_FossilCave(&sRegion);
}

// TRUE if the FOSSILS unlock fills dungeon d's choice room with two fossils:
// the fossil cave's floors have one, as Mt. Moon has, and then its first
// item ball holds no fossil.
static bool32 FossilChoice(s32 d)
{
    struct RogueDungeonExtras e;
    // (v1.1 peaks) The desert tower's top always holds two, as the Mirage
    // Tower's: the region's fossil and the other, one to take.
    if (d >= 0 && sRegion.dungeons[d].kind == RDUN_MIRAGE)
        return TRUE;
    if (!Rogue_IsUnlockOn(UNLOCK_FOSSILS) || d < 0 || FossilCave() < 0 || sRegion.dungeons[d].host != FossilCave())
        return FALSE;
    RogueGen_DungeonExtras(&sRegion, d, &e);
    return e.nChoice != 0;
}

// Script special for the scientist in the fourth gym town. VAR_RESULT: 0 if
// fossils are not unlocked, 1 if the fossil is not found yet, 2 if the player
// carries it (VAR_0x8000 the item, VAR_0x8001 the species, VAR_0x8002 the
// level), 3 once it is revived. The fossil's name goes to STR_VAR_1 and the
// species' to STR_VAR_2.
void Rogue_PrepareFossil(void)
{
    u32 fossil;

    EnsureRegion();
    fossil = RegionFossil();
    // The player may carry the other fossil of the choice room instead.
    if (!CheckBagHasItem(sFossils[fossil][0], 1) && CheckBagHasItem(sFossils[SecondFossil()][0], 1))
        fossil = SecondFossil();
    gSpecialVar_0x8000 = sFossils[fossil][0];
    gSpecialVar_0x8001 = sFossils[fossil][1];
    gSpecialVar_0x8002 = sRegion.places[sRegion.gymTown[3]].level > 6 ? sRegion.places[sRegion.gymTown[3]].level - 3 : 5;
    CopyItemName(gSpecialVar_0x8000, gStringVar1);
    StringCopy(gStringVar2, GetSpeciesName(gSpecialVar_0x8001));
    if (!Rogue_IsUnlockOn(UNLOCK_FOSSILS) && !FlagGet(FLAG_ROGUE_FOSSIL_FOUND))
        gSpecialVar_Result = 0;
    else if (FlagGet(FLAG_ROGUE_FOSSIL_REVIVED))
        gSpecialVar_Result = 3;
    else if (CheckBagHasItem(gSpecialVar_0x8000, 1))
        gSpecialVar_Result = 2;
    else
        gSpecialVar_Result = 1;
}

// The fossil came back to life.
void Rogue_FossilRevived(void)
{
    RemoveBagItem(gSpecialVar_0x8000, 1);
    FlagSet(FLAG_ROGUE_FOSSIL_REVIVED);
    Rogue_RecordFossilRevived();
}

// TRUE if the scientist who revives fossils visits this town.
bool32 Rogue_IsFossilTown(u32 place)
{
    EnsureRegion();
    return (Rogue_IsUnlockOn(UNLOCK_FOSSILS) || FlagGet(FLAG_ROGUE_FOSSIL_FOUND)) && place == sRegion.gymTown[3];
}

// VAR_RESULT: TRUE in the town where the fossil scientist visits, for the
// sign outside the house he visits.
void Rogue_CheckFossilTown(void)
{
    gSpecialVar_Result = Rogue_IsFossilTown(Rogue_GetCurrentTownPlace());
}

// What the item catalogue's ledger leaves out under this run's rules: no
// battle items with the Hardcore rules.
static u8 ItemRules(void)
{
    return FlagGet(FLAG_ROGUE_HARDCORE) ? ROGUE_ITEMS_HARDCORE : 0;
}

// Sets up the item for the item ball the player is facing: VAR_0x8000 and
// how many in VAR_0x8001, from the run's ledger (src/rogue_gen_items.h).
void Rogue_PrepareItem(void)
{
    s32 place = Rogue_GetCurrentPlace();
    u32 slot = GetObjectEventFlagIdByLocalIdAndMap(gSpecialVar_LastTalked, gSaveBlock1Ptr->location.mapNum, gSaveBlock1Ptr->location.mapGroup)
             - FLAG_ROGUE_ITEM_0;
    u16 item;
    u8 qty;

    gSpecialVar_0x8000 = ITEM_POTION;
    gSpecialVar_0x8001 = 1;
    if (place < 0)
        return;
    EnsureRegion();
    if (IsFloorMap(gSaveBlock1Ptr->location.mapNum) && CurrentDungeon() >= 0)
    {
        // FOSSILS: the fossil cave's choice room holds two fossils.
        s32 d = CurrentDungeon();
        if (RogueDungeon_IsChoice(d, slot) && FossilChoice(d))
        {
            struct RogueDungeonExtras e;
            RogueGen_DungeonExtras(&sRegion, d, &e);
            gSpecialVar_0x8000 = sFossils[slot == e.choice ? RegionFossil() : SecondFossil()][0];
            FlagSet(FLAG_ROGUE_FOSSIL_FOUND);
            return;
        }
    }
    else if (Rogue_IsUnlockOn(UNLOCK_FOSSILS) && place == FossilCave() && !FossilChoice(RogueGen_DungeonAt(&sRegion, place))
          && slot == sRegion.places[place].itemBase)
    {
        // The first item ball of the fossil cave holds the region's fossil.
        gSpecialVar_0x8000 = sFossils[RegionFossil()][0];
        FlagSet(FLAG_ROGUE_FOSSIL_FOUND);
        return;
    }
    if (slot >= ROGUE_MAX_ITEM_FLAGS)
        return;
    // A legend's key, deep in another dungeon (Mt. Pyre's orbs): its prize.
    if (IsFloorMap(gSaveBlock1Ptr->location.mapNum) && CurrentDungeon() >= 0)
    {
        struct RogueDungeonExtras e;
        RogueGen_DungeonExtras(&sRegion, CurrentDungeon(), &e);
        if (slot == e.prize && Rogue_PrizeKey(CurrentDungeon()) != ITEM_NONE)
        {
            gSpecialVar_0x8000 = Rogue_PrizeKey(CurrentDungeon());
            return;
        }
        // A shipwreck's cabin key.
        if (RogueGen_KeyItem(&sRegion, CurrentDungeon(), slot) != ITEM_NONE)
        {
            gSpecialVar_0x8000 = RogueGen_KeyItem(&sRegion, CurrentDungeon(), slot);
            return;
        }
    }
    item = RogueGen_ItemFor(&sRegion, RITEMS_BALL, slot, &qty, ItemRules());
    if (item != ITEM_NONE)
    {
        gSpecialVar_0x8000 = item;
        gSpecialVar_0x8001 = qty;
    }
}

// Called when the player finds a hidden item, before it is picked up: what
// it is comes from the run's ledger. VAR_0x8004 is its flag; sets
// VAR_0x8005 to the item and VAR_0x8009 to how many.
void Rogue_PrepareHiddenItem(void)
{
    u8 qty;
    u16 item;
    if (!InRegion() || gSpecialVar_0x8004 < FLAG_ROGUE_HIDDEN_0 || gSpecialVar_0x8004 >= FLAG_ROGUE_HIDDEN_0 + ROGUE_MAX_HIDDEN)
        return;
    EnsureRegion();
    item = RogueGen_ItemFor(&sRegion, RITEMS_HIDDEN, gSpecialVar_0x8004 - FLAG_ROGUE_HIDDEN_0, &qty, ItemRules());
    if (item != ITEM_NONE)
    {
        gSpecialVar_0x8005 = item;
        gSpecialVar_0x8009 = qty;
    }
}

// A gift from a town (its gift house, or a townsperson when folk is TRUE),
// into VAR_0x8000 and VAR_0x8001. FALSE if the ledger has none left.
bool32 Rogue_GetTownGift(u32 town, bool32 folk)
{
    u8 qty;
    u16 item;
    EnsureRegion();
    item = RogueGen_ItemFor(&sRegion, folk ? RITEMS_FOLK : RITEMS_GIFT, town, &qty, ItemRules());
    gSpecialVar_0x8000 = item;
    gSpecialVar_0x8001 = qty;
    return item != ITEM_NONE;
}

// Sets the gym's leader and trainer sprites before the map loads.
void Rogue_SetGymGraphics(void)
{
    u32 gym;
    EnsureRegion();
    gym = CurrentGym();
    VarSet(VAR_OBJ_GFX_ID_0, sLeaders[sGymLeaders[gym]].gfx);
    VarSet(VAR_OBJ_GFX_ID_1, TrainerClass(ROGUE_SLOT_GYM_0 + gym * ROGUE_GYM_TRAINERS)->gfx);
    VarSet(VAR_OBJ_GFX_ID_2, TrainerClass(ROGUE_SLOT_GYM_0 + gym * ROGUE_GYM_TRAINERS + 1)->gfx);
    VarSet(VAR_OBJ_GFX_ID_3, TrainerClass(ROGUE_SLOT_GYM_0 + gym * ROGUE_GYM_TRAINERS + 2)->gfx);
}

// The Mart's stock by badges won.
static u32 MartTier(void)
{
    u32 progress = VarGet(VAR_ROGUE_PROGRESS);
    if (Rogue_GetCurrentPlace() == sRegion.summit)
        return 5;
    if (progress < ROGUE_PROGRESS_BADGE_1)
        return 0;
    return (progress - ROGUE_PROGRESS_BADGE_1 + 2) / 2 > 5 ? 5 : (progress - ROGUE_PROGRESS_BADGE_1 + 2) / 2;
}

u32 Rogue_GetMartTier(void)
{
    return Rogue_Run0MartTier(MartTier());
}

// A dark cave is lit once two badges are won: they alone let the player use
// Flash.
void Rogue_OnMapLoad(void)
{
    s32 place = Rogue_GetCurrentPlace();
    // A Director copy left loaded is dropped (see Rogue_Director).
    Rogue_ReleaseDirector();
    // RUN 0's scenes that wait for a place (src/rogue_run0_story.c).
    Rogue_Run0OnMapLoad();
    if (place >= 0 && IsFloorMap(gSaveBlock1Ptr->location.mapNum))
    {
        if (gMapHeader.cave && Rogue_BadgeCount() >= 2)
            FlagSet(FLAG_SYS_USE_FLASH);
        return;
    }
    // What people here will say needs the town's facts: worked out now,
    // behind the fade, not as someone speaks.
    if (place >= 0)
        Rogue_WarmText(place);
    if (place < 0 || gSaveBlock1Ptr->location.mapNum >= ROGUE_MAX_PLACES)
        return;
    Rogue_RecordCaveVisit(place);
    if (sRegion.places[place].kind == RPLACE_CAVE && sRegion.places[place].dark && Rogue_BadgeCount() >= 2)
        FlagSet(FLAG_SYS_USE_FLASH);
}

// ---------------------------------------------------------------------------
// Fly

// TRUE if the player may fly, and to the town at place.
bool32 Rogue_CanFlyTo(u32 place)
{
    EnsureRegion();
    if (Rogue_BadgeCount() < 6 || place >= sRegion.nPlaces || sRegion.places[place].kind != RPLACE_TOWN)
        return FALSE;
    if (!(VarGet(VAR_ROGUE_VISITED) & (1 << sRegion.places[place].number)))
        return FALSE;
    // Not out of the League once inside, nor into a town while in a gym.
    return Rogue_GetCurrentPlace() >= 0 && gSaveBlock1Ptr->location.mapNum < ROGUE_MAX_PLACES;
}

// Sets the warp for flying to a town: in front of its Pokemon Center.
void Rogue_SetFlyWarp(u32 place)
{
    const struct RogueMap *m = Rogue_GetPlaceMap(place);
    u32 i, x = m->startX, y = m->startY;
    for (i = 0; i < m->nWarps; i++)
    {
        if (m->warps[i].kind == RWARP_PC || (m->warps[i].kind == RWARP_CAVE && x == 0))
        {
            x = m->warps[i].x;
            y = m->warps[i].y + 1;
        }
    }
    SetWarpDestination(ROGUE_GROUP, place, WARP_ID_NONE, x, y);
}

// ---------------------------------------------------------------------------
// For RUN 0's story (src/rogue_run0_story.c)

// The Grass, Fire and Water starters a region's seed rolls, as
// GenerateContent rolls them.
void Rogue_RegionStarters(u32 seed, u16 *out)
{
    u32 rng = RogueRng_Hash(seed, 7), i;
    for (i = 0; i < 19; i++)
        RogueRng_Next(&rng);
    for (i = 0; i < 3; i++)
        out[i] = PickEnabled(&rng, sStarters[i], ARRAY_COUNT(sStarters[0]));
}

u16 Rogue_EvolveTo(u16 species, u32 level)
{
    return EvolveForLevel(species, level, TRUE);
}

// A trainer's team is made again next time (the Director's second team).
void Rogue_InvalidateTrainer(void)
{
    sTrainerCachedId = 0;
}

// The region of a seed, for PROF. BIRCH to show (the region kept in CHOOSE).
void Rogue_PrepareSeedRegion(u32 seed)
{
    PrepareRegion(seed, ROGUE_MIX_COUNT);
}
