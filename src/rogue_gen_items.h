// Items: one catalogue that every source of items in a run draws from.
// Included by rogue_gen.c after rogue_gen_dungeon.h, so it is plain C the
// host tools build too (tools/roguemon/item_tally.py counts what a run hands
// out across many seeds).
//
// Each item that suits the game has a role (healing, a held item, a TM...),
// the stages of a run it can turn up in (badges won, 0 to 8), a weight within
// its role, and a cap: how many a run hands out at most. Each source (item
// balls on routes, side paths and in caves, hidden items, dungeon floors,
// prizes, choice rooms, masters, gift houses, townsfolk) has its own taste,
// a weight per role, and draws from the catalogue with it.
//
// What a run hands out is worked out in one fixed order from the seed alone
// (the ledger: gift houses, townsfolk, masters, every item ball, every hidden
// item), keeping count of each item, so caps hold across the whole region
// with no save data: no third Charcoal, no tenth Super Potion. Asking for
// one item replays the ledger up to it, which is cheap enough to do on the
// spot.
//
// Revives are left out on purpose: used in battle they would bring back a
// Pokemon that fainted before the battle ends, and the Nuzlocke rule removes
// it only at the end.

#include "constants/items.h"
#include "constants/pokemon.h"

struct RogueItemDef
{
    u16 item;
    u8 role;   // enum RogueItemRole
    u8 from;   // the first stage it turns up in (badges won)
    u8 until;  // the last
    u8 weight; // within its role
    u8 cap;    // at most this many in a run, from the ledger's sources
    u8 qty;    // how many come in one find
    u8 type;   // TMs and type items: the type they help
};

#define IT(item, role, from, until, w, cap, qty) { ITEM_##item, RIR_##role, from, until, w, cap, qty, 0 }
#define TM(move, type, from, w)                  { ITEM_TM_##move, RIR_TM, from, 8, w, 1, 1, TYPE_##type }
#define TY(item, type, w)                        { ITEM_##item, RIR_TYPE, 0, 8, w, 1, 1, TYPE_##type }

// Grouped by role, in the order of enum RogueItemRole.
static const struct RogueItemDef sItemCatalogue[] =
{
    // Healing: plain potions early, stronger ones later, and drinks between.
    IT(POTION,         HEAL, 0, 2, 10, 5, 1),
    IT(SUPER_POTION,   HEAL, 1, 5, 10, 5, 1),
    IT(HYPER_POTION,   HEAL, 4, 8, 9, 4, 1),
    IT(MAX_POTION,     HEAL, 6, 8, 5, 3, 1),
    IT(FULL_RESTORE,   HEAL, 7, 8, 3, 2, 1),
    IT(BERRY_JUICE,    HEAL, 0, 3, 3, 2, 1),
    IT(FRESH_WATER,    HEAL, 1, 4, 4, 2, 1),
    IT(SODA_POP,       HEAL, 2, 5, 3, 2, 1),
    IT(LEMONADE,       HEAL, 3, 6, 3, 2, 1),
    IT(MOOMOO_MILK,    HEAL, 4, 8, 3, 2, 1),
    IT(ENERGY_POWDER,  HEAL, 1, 5, 2, 2, 1),
    IT(ENERGY_ROOT,    HEAL, 4, 8, 2, 2, 1),
    // Status
    IT(ANTIDOTE,       STATUS, 0, 4, 4, 2, 1),
    IT(PARALYZE_HEAL,  STATUS, 0, 4, 4, 2, 1),
    IT(AWAKENING,      STATUS, 0, 5, 3, 2, 1),
    IT(BURN_HEAL,      STATUS, 0, 5, 3, 2, 1),
    IT(ICE_HEAL,       STATUS, 1, 6, 2, 2, 1),
    IT(FULL_HEAL,      STATUS, 3, 8, 5, 3, 1),
    IT(HEAL_POWDER,    STATUS, 2, 8, 2, 2, 1),
    IT(LAVA_COOKIE,    STATUS, 1, 6, 2, 2, 1),
    // PP
    IT(ETHER,          PP, 1, 8, 4, 3, 1),
    IT(MAX_ETHER,      PP, 4, 8, 4, 2, 1),
    IT(ELIXIR,         PP, 4, 8, 4, 2, 1),
    IT(MAX_ELIXIR,     PP, 6, 8, 1, 1, 1),
    // Balls: a pair of the plain ones, and the special balls for their uses.
    IT(POKE_BALL,      BALL, 0, 2, 6, 3, 2),
    IT(GREAT_BALL,     BALL, 1, 6, 6, 3, 2),
    IT(ULTRA_BALL,     BALL, 4, 8, 5, 3, 2),
    IT(NEST_BALL,      BALL, 0, 5, 2, 2, 1),
    IT(NET_BALL,       BALL, 1, 8, 2, 2, 1),
    IT(DIVE_BALL,      BALL, 3, 8, 2, 2, 1),
    IT(REPEAT_BALL,    BALL, 2, 8, 2, 2, 1),
    IT(TIMER_BALL,     BALL, 3, 8, 2, 2, 1),
    IT(QUICK_BALL,     BALL, 2, 8, 2, 2, 1),
    IT(DUSK_BALL,      BALL, 2, 8, 2, 2, 1),
    IT(LUXURY_BALL,    BALL, 1, 8, 1, 1, 1),
    IT(PREMIER_BALL,   BALL, 0, 8, 1, 1, 1),
    IT(HEAL_BALL,      BALL, 0, 4, 1, 2, 1),
    IT(LEVEL_BALL,     BALL, 2, 8, 1, 1, 1),
    IT(LURE_BALL,      BALL, 2, 8, 1, 1, 1),
    IT(MOON_BALL,      BALL, 2, 8, 1, 1, 1),
    IT(FRIEND_BALL,    BALL, 2, 8, 1, 1, 1),
    IT(FAST_BALL,      BALL, 2, 8, 1, 1, 1),
    IT(HEAVY_BALL,     BALL, 2, 8, 1, 1, 1),
    IT(MASTER_BALL,    BALL, 0, 8, 0, 1, 1), // only the hideout's prize
    // Battle items (none under the Hardcore rules)
    IT(X_ATTACK,       BATTLE, 0, 8, 3, 2, 1),
    IT(X_DEFENSE,      BATTLE, 0, 8, 2, 2, 1),
    IT(X_SP_ATK,       BATTLE, 0, 8, 3, 2, 1),
    IT(X_SP_DEF,       BATTLE, 0, 8, 2, 2, 1),
    IT(X_SPEED,        BATTLE, 0, 8, 3, 2, 1),
    IT(X_ACCURACY,     BATTLE, 0, 8, 1, 1, 1),
    IT(DIRE_HIT,       BATTLE, 0, 8, 2, 2, 1),
    IT(GUARD_SPEC,     BATTLE, 0, 8, 1, 1, 1),
    // Held items: one of each a run.
    IT(QUICK_CLAW,     HELD, 0, 8, 4, 1, 1),
    IT(SOOTHE_BELL,    HELD, 0, 8, 2, 1, 1),
    IT(CLEANSE_TAG,    HELD, 0, 8, 2, 1, 1),
    IT(SMOKE_BALL,     HELD, 0, 8, 2, 1, 1),
    IT(EVERSTONE,      HELD, 0, 8, 1, 1, 1),
    IT(AMULET_COIN,    HELD, 1, 8, 2, 1, 1),
    IT(LUCKY_EGG,      HELD, 2, 8, 2, 1, 1),
    IT(WHITE_HERB,     HELD, 2, 8, 2, 1, 1),
    IT(MENTAL_HERB,    HELD, 2, 8, 2, 1, 1),
    IT(POWER_HERB,     HELD, 3, 8, 1, 1, 1),
    IT(FOCUS_BAND,     HELD, 2, 8, 3, 1, 1),
    IT(KINGS_ROCK,     HELD, 2, 8, 3, 1, 1),
    IT(MUSCLE_BAND,    HELD, 2, 8, 3, 1, 1),
    IT(WISE_GLASSES,   HELD, 2, 8, 3, 1, 1),
    IT(BIG_ROOT,       HELD, 2, 8, 2, 1, 1),
    IT(WIDE_LENS,      HELD, 2, 8, 2, 1, 1),
    IT(SCOPE_LENS,     HELD, 3, 8, 3, 1, 1),
    IT(SHELL_BELL,     HELD, 3, 8, 3, 1, 1),
    IT(LEFTOVERS,      HELD, 3, 8, 3, 1, 1),
    IT(EVIOLITE,       HELD, 3, 8, 3, 1, 1),
    IT(RAZOR_CLAW,     HELD, 3, 8, 2, 1, 1),
    IT(RAZOR_FANG,     HELD, 3, 8, 2, 1, 1),
    IT(ZOOM_LENS,      HELD, 3, 8, 2, 1, 1),
    IT(LIGHT_CLAY,     HELD, 3, 8, 2, 1, 1),
    IT(AIR_BALLOON,    HELD, 3, 8, 2, 1, 1),
    IT(BRIGHT_POWDER,  HELD, 3, 8, 1, 1, 1),
    IT(LAX_INCENSE,    HELD, 3, 8, 1, 1, 1),
    IT(METRONOME,      HELD, 3, 8, 1, 1, 1),
    IT(BLACK_SLUDGE,   HELD, 3, 8, 1, 1, 1),
    IT(HEAT_ROCK,      HELD, 3, 8, 1, 1, 1),
    IT(DAMP_ROCK,      HELD, 3, 8, 1, 1, 1),
    IT(SMOOTH_ROCK,    HELD, 3, 8, 1, 1, 1),
    IT(ICY_ROCK,       HELD, 3, 8, 1, 1, 1),
    IT(EXPERT_BELT,    HELD, 4, 8, 3, 1, 1),
    IT(ROCKY_HELMET,   HELD, 4, 8, 2, 1, 1),
    IT(FOCUS_SASH,     HELD, 4, 8, 2, 1, 1),
    IT(FLAME_ORB,      HELD, 4, 8, 1, 1, 1),
    IT(TOXIC_ORB,      HELD, 4, 8, 1, 1, 1),
    IT(LIFE_ORB,       HELD, 5, 8, 2, 1, 1),
    IT(CELL_BATTERY,   HELD, 0, 8, 0, 1, 1), // only a power plant's
    IT(CHOICE_BAND,    HELD, 5, 8, 2, 1, 1),
    IT(CHOICE_SPECS,   HELD, 5, 8, 2, 1, 1),
    IT(CHOICE_SCARF,   HELD, 5, 8, 2, 1, 1),
    IT(ASSAULT_VEST,   HELD, 5, 8, 2, 1, 1),
    IT(WEAKNESS_POLICY, HELD, 5, 8, 1, 1, 1),
    // Type items: one of each a run.
    TY(SILK_SCARF,     NORMAL, 2),
    TY(BLACK_BELT,     FIGHTING, 2),
    TY(SHARP_BEAK,     FLYING, 2),
    TY(POISON_BARB,    POISON, 2),
    TY(SOFT_SAND,      GROUND, 2),
    TY(HARD_STONE,     ROCK, 2),
    TY(SILVER_POWDER,  BUG, 2),
    TY(SPELL_TAG,      GHOST, 2),
    TY(METAL_COAT,     STEEL, 2),
    TY(CHARCOAL,       FIRE, 2),
    TY(MYSTIC_WATER,   WATER, 2),
    TY(MIRACLE_SEED,   GRASS, 2),
    TY(MAGNET,         ELECTRIC, 2),
    TY(TWISTED_SPOON,  PSYCHIC, 2),
    TY(NEVER_MELT_ICE, ICE, 2),
    TY(DRAGON_FANG,    DRAGON, 2),
    TY(BLACK_GLASSES,  DARK, 2),
    TY(FAIRY_FEATHER,  FAIRY, 2),
    // Berries that work in battle.
    IT(ORAN_BERRY,     BERRY, 0, 3, 8, 4, 1),
    IT(SITRUS_BERRY,   BERRY, 1, 8, 8, 4, 1),
    IT(LUM_BERRY,      BERRY, 2, 8, 5, 3, 1),
    IT(CHERI_BERRY,    BERRY, 0, 6, 3, 2, 1),
    IT(CHESTO_BERRY,   BERRY, 0, 6, 3, 2, 1),
    IT(PECHA_BERRY,    BERRY, 0, 6, 3, 2, 1),
    IT(RAWST_BERRY,    BERRY, 0, 6, 3, 2, 1),
    IT(ASPEAR_BERRY,   BERRY, 0, 6, 2, 2, 1),
    IT(PERSIM_BERRY,   BERRY, 0, 6, 2, 2, 1),
    IT(LEPPA_BERRY,    BERRY, 1, 8, 3, 2, 1),
    IT(FIGY_BERRY,     BERRY, 2, 8, 1, 1, 1),
    IT(WIKI_BERRY,     BERRY, 2, 8, 1, 1, 1),
    IT(MAGO_BERRY,     BERRY, 2, 8, 1, 1, 1),
    IT(AGUAV_BERRY,    BERRY, 2, 8, 1, 1, 1),
    IT(IAPAPA_BERRY,   BERRY, 2, 8, 1, 1, 1),
    IT(LIECHI_BERRY,   BERRY, 4, 8, 2, 1, 1),
    IT(GANLON_BERRY,   BERRY, 4, 8, 2, 1, 1),
    IT(SALAC_BERRY,    BERRY, 4, 8, 2, 1, 1),
    IT(PETAYA_BERRY,   BERRY, 4, 8, 2, 1, 1),
    IT(APICOT_BERRY,   BERRY, 4, 8, 2, 1, 1),
    IT(CUSTAP_BERRY,   BERRY, 5, 8, 1, 1, 1),
    IT(LANSAT_BERRY,   BERRY, 7, 8, 1, 1, 1),
    IT(STARF_BERRY,    BERRY, 7, 8, 1, 1, 1),
    IT(OCCA_BERRY,     BERRY, 3, 8, 1, 1, 1),
    IT(PASSHO_BERRY,   BERRY, 3, 8, 1, 1, 1),
    IT(WACAN_BERRY,    BERRY, 3, 8, 1, 1, 1),
    IT(RINDO_BERRY,    BERRY, 3, 8, 1, 1, 1),
    IT(YACHE_BERRY,    BERRY, 3, 8, 1, 1, 1),
    IT(CHOPLE_BERRY,   BERRY, 3, 8, 1, 1, 1),
    IT(KEBIA_BERRY,    BERRY, 3, 8, 1, 1, 1),
    IT(SHUCA_BERRY,    BERRY, 3, 8, 1, 1, 1),
    IT(COBA_BERRY,     BERRY, 3, 8, 1, 1, 1),
    IT(PAYAPA_BERRY,   BERRY, 3, 8, 1, 1, 1),
    IT(TANGA_BERRY,    BERRY, 3, 8, 1, 1, 1),
    IT(CHARTI_BERRY,   BERRY, 3, 8, 1, 1, 1),
    IT(KASIB_BERRY,    BERRY, 3, 8, 1, 1, 1),
    IT(HABAN_BERRY,    BERRY, 3, 8, 1, 1, 1),
    IT(COLBUR_BERRY,   BERRY, 3, 8, 1, 1, 1),
    IT(BABIRI_BERRY,   BERRY, 3, 8, 1, 1, 1),
    IT(CHILAN_BERRY,   BERRY, 3, 8, 1, 1, 1),
    IT(ROSELI_BERRY,   BERRY, 3, 8, 1, 1, 1),
    // Evolution items. The trade evolution items are used from the bag
    // (I_USE_EVO_HELD_ITEMS_FROM_BAG), as there is no one to trade with.
    IT(FIRE_STONE,     EVO, 0, 8, 4, 1, 1),
    IT(WATER_STONE,    EVO, 0, 8, 4, 1, 1),
    IT(THUNDER_STONE,  EVO, 0, 8, 4, 1, 1),
    IT(LEAF_STONE,     EVO, 0, 8, 4, 1, 1),
    IT(MOON_STONE,     EVO, 0, 8, 4, 2, 1),
    IT(SUN_STONE,      EVO, 2, 8, 2, 1, 1),
    IT(SHINY_STONE,    EVO, 2, 8, 2, 1, 1),
    IT(DUSK_STONE,     EVO, 2, 8, 2, 1, 1),
    IT(DAWN_STONE,     EVO, 2, 8, 2, 1, 1),
    IT(ICE_STONE,      EVO, 2, 8, 2, 1, 1),
    IT(LINKING_CORD,   EVO, 2, 8, 3, 2, 1),
    IT(DRAGON_SCALE,   EVO, 3, 8, 1, 1, 1),
    IT(UPGRADE,        EVO, 3, 8, 1, 1, 1),
    IT(DUBIOUS_DISC,   EVO, 3, 8, 1, 1, 1),
    IT(PROTECTOR,      EVO, 3, 8, 1, 1, 1),
    IT(ELECTIRIZER,    EVO, 3, 8, 1, 1, 1),
    IT(MAGMARIZER,     EVO, 3, 8, 1, 1, 1),
    IT(REAPER_CLOTH,   EVO, 3, 8, 1, 1, 1),
    IT(PRISM_SCALE,    EVO, 3, 8, 1, 1, 1),
    IT(DEEP_SEA_TOOTH, EVO, 3, 8, 1, 1, 1),
    IT(DEEP_SEA_SCALE, EVO, 3, 8, 1, 1, 1),
    IT(OVAL_STONE,     EVO, 3, 8, 1, 1, 1),
    IT(SACHET,         EVO, 3, 8, 1, 1, 1),
    IT(WHIPPED_DREAM,  EVO, 3, 8, 1, 1, 1),
    // Levels: few Rare Candies, and not before the second badge.
    IT(EXP_CANDY_S,    CANDY, 0, 4, 4, 3, 2),
    IT(EXP_CANDY_M,    CANDY, 2, 7, 4, 3, 1),
    IT(EXP_CANDY_L,    CANDY, 5, 8, 2, 2, 1),
    IT(RARE_CANDY,     CANDY, 2, 8, 4, 4, 1),
    // Vitamins and the like.
    IT(HP_UP,          VITAMIN, 2, 8, 2, 2, 1),
    IT(PROTEIN,        VITAMIN, 2, 8, 2, 2, 1),
    IT(IRON,           VITAMIN, 2, 8, 2, 2, 1),
    IT(CALCIUM,        VITAMIN, 2, 8, 2, 2, 1),
    IT(ZINC,           VITAMIN, 2, 8, 2, 2, 1),
    IT(CARBOS,         VITAMIN, 2, 8, 2, 2, 1),
    IT(PP_UP,          VITAMIN, 1, 8, 3, 3, 1),
    IT(PP_MAX,         VITAMIN, 6, 8, 1, 1, 1),
    IT(ABILITY_CAPSULE, VITAMIN, 4, 8, 1, 1, 1),
    IT(ADAMANT_MINT,   VITAMIN, 3, 8, 1, 1, 1),
    IT(MODEST_MINT,    VITAMIN, 3, 8, 1, 1, 1),
    IT(JOLLY_MINT,     VITAMIN, 3, 8, 1, 1, 1),
    IT(TIMID_MINT,     VITAMIN, 3, 8, 1, 1, 1),
    IT(BOLD_MINT,      VITAMIN, 3, 8, 1, 1, 1),
    IT(CALM_MINT,      VITAMIN, 3, 8, 1, 1, 1),
    // TMs (single use, as in Emerald): the strong ones later.
    TM(WATER_PULSE,  WATER, 0, 2),
    TM(BULLET_SEED,  GRASS, 0, 2),
    TM(SHOCK_WAVE,   ELECTRIC, 0, 2),
    TM(ROCK_TOMB,    ROCK, 0, 2),
    TM(AERIAL_ACE,   FLYING, 0, 2),
    TM(SECRET_POWER, NORMAL, 0, 2),
    TM(THIEF,        DARK, 0, 1),
    TM(ROAR,         NORMAL, 0, 1),
    TM(TOXIC,        POISON, 1, 2),
    TM(PROTECT,      NORMAL, 1, 2),
    TM(FACADE,       NORMAL, 1, 2),
    TM(DIG,          GROUND, 1, 2),
    TM(STEEL_WING,   STEEL, 1, 2),
    TM(HIDDEN_POWER, NORMAL, 1, 1),
    TM(SUNNY_DAY,    FIRE, 1, 1),
    TM(RAIN_DANCE,   WATER, 1, 1),
    TM(REST,         PSYCHIC, 1, 1),
    TM(ATTRACT,      NORMAL, 1, 1),
    TM(DOUBLE_TEAM,  NORMAL, 1, 1),
    TM(FRUSTRATION,  NORMAL, 1, 1),
    TM(BULK_UP,      FIGHTING, 2, 2),
    TM(BRICK_BREAK,  FIGHTING, 2, 2),
    TM(GIGA_DRAIN,   GRASS, 2, 2),
    TM(RETURN,       NORMAL, 2, 2),
    TM(TAUNT,        DARK, 2, 1),
    TM(TORMENT,      DARK, 2, 1),
    TM(LIGHT_SCREEN, PSYCHIC, 2, 1),
    TM(REFLECT,      PSYCHIC, 2, 1),
    TM(SAFEGUARD,    NORMAL, 2, 1),
    TM(HAIL,         ICE, 2, 1),
    TM(SANDSTORM,    ROCK, 2, 1),
    TM(CALM_MIND,    PSYCHIC, 3, 2),
    TM(IRON_TAIL,    STEEL, 3, 2),
    TM(SKILL_SWAP,   PSYCHIC, 3, 1),
    TM(SNATCH,       DARK, 3, 1),
    TM(DRAGON_CLAW,  DRAGON, 4, 2),
    TM(SHADOW_BALL,  GHOST, 4, 2),
    TM(SLUDGE_BOMB,  POISON, 4, 2),
    TM(SOLAR_BEAM,   GRASS, 4, 1),
    TM(FOCUS_PUNCH,  FIGHTING, 4, 1),
    TM(OVERHEAT,     FIRE, 4, 1),
    TM(FLAMETHROWER, FIRE, 5, 1),
    TM(ICE_BEAM,     ICE, 5, 1),
    TM(THUNDERBOLT,  ELECTRIC, 5, 1),
    TM(PSYCHIC,      PSYCHIC, 5, 1),
    TM(EARTHQUAKE,   GROUND, 6, 1),
    TM(BLIZZARD,     ICE, 6, 1),
    TM(THUNDER,      ELECTRIC, 6, 1),
    TM(FIRE_BLAST,   FIRE, 6, 1),
    TM(HYPER_BEAM,   NORMAL, 7, 1),
    // Things to sell: a Nugget is worth a few hundred Potions.
    IT(TINY_MUSHROOM,  MONEY, 0, 4, 3, 3, 1),
    IT(PEARL,          MONEY, 0, 5, 3, 3, 1),
    IT(STARDUST,       MONEY, 0, 6, 3, 3, 1),
    IT(BIG_MUSHROOM,   MONEY, 2, 8, 2, 2, 1),
    IT(BIG_PEARL,      MONEY, 2, 8, 2, 2, 1),
    IT(STAR_PIECE,     MONEY, 2, 8, 2, 2, 1),
    IT(NUGGET,         MONEY, 2, 8, 3, 3, 1),
    IT(RARE_BONE,      MONEY, 4, 8, 1, 1, 1),
    IT(PEARL_STRING,   MONEY, 5, 8, 1, 1, 1),
    IT(BALM_MUSHROOM,  MONEY, 5, 8, 1, 1, 1),
    IT(BIG_NUGGET,     MONEY, 6, 8, 1, 1, 1),
    IT(COMET_SHARD,    MONEY, 6, 8, 1, 1, 1),
    // In the field
    IT(ESCAPE_ROPE,    FIELD, 0, 8, 3, 2, 1),
    IT(REPEL,          FIELD, 0, 3, 3, 2, 1),
    IT(SUPER_REPEL,    FIELD, 2, 6, 3, 2, 1),
    IT(MAX_REPEL,      FIELD, 5, 8, 3, 2, 1),
    IT(HONEY,          FIELD, 1, 8, 1, 1, 1),
    IT(POKE_DOLL,      FIELD, 0, 5, 1, 1, 1),
};

#undef IT
#undef TM
#undef TY

#define NUM_CATALOGUE (sizeof(sItemCatalogue) / sizeof(sItemCatalogue[0]))

// How each source likes each role. Healing, status and PP weigh less with
// every badge (sHealScale), so they are common early and a minority later.
enum
{
    TASTE_ROUTE,  // item balls on the main road
    TASTE_SIDE,   // item balls on side paths, shortcuts and forks
    TASTE_CAVE,   // a cave's own map
    TASTE_HIDDEN, // hidden items, as Emerald's: mostly small things and things to sell
    TASTE_FLOOR,  // dungeon floors
    TASTE_PRIZE,  // the ball beside a dungeon's boss, when its kind's own prize is taken
    TASTE_CHOICE, // a choice room: pick one of two or three
    TASTE_MASTER, // a dungeon master's reward, when not a TM
    TASTE_COUNT,
};

static const u8 sTastes[TASTE_COUNT][RIR_COUNT] =
{
    //               HEAL STAT PP BALL BATL HELD TYPE BERRY EVO CANDY VIT TM MONEY FIELD
    [TASTE_ROUTE]  = { 26, 10,  3,  9,   4,   5,   5,  12,   6,  5,    4,  5,  7,   4 },
    [TASTE_SIDE]   = {  8,  3,  3,  6,   3,  12,   8,   8,  12, 10,   10, 10,  8,   2 },
    [TASTE_CAVE]   = { 10,  5,  4,  4,   3,   6,   6,   4,  10,  6,    6,  6, 16,   6 },
    [TASTE_HIDDEN] = { 18, 10,  6,  4,   5,   2,   2,  14,   3,  8,    6,  0, 20,   6 },
    [TASTE_FLOOR]  = {  8,  4,  5,  5,   3,  10,   8,   6,  10, 10,   10, 10, 10,   3 },
    [TASTE_PRIZE]  = {  0,  0,  0,  0,   0,  20,   6,   0,  10,  8,   10, 10,  0,   0 },
    [TASTE_CHOICE] = {  0,  0,  0,  0,   0,  30,   8,   0,  12,  0,    0,  8,  0,   0 },
    [TASTE_MASTER] = {  0,  0,  0,  0,   0,  20,   8,   0,   0,  0,    0,  0,  0,   0 },
};

// Twelfths of the healing weights by stage.
static const u8 sHealScale[9] = { 12, 11, 9, 8, 7, 6, 5, 4, 4 };

// A dungeon kind's own finds, signature prizes and master TMs.
static const u16 sFloorSignature[RDUN_COUNT][8] =
{
    [RDUN_CAVE]    = { ITEM_HARD_STONE, ITEM_EVERSTONE, ITEM_STAR_PIECE, ITEM_NUGGET, ITEM_MOON_STONE, ITEM_STARDUST, ITEM_SOFT_SAND, ITEM_RARE_BONE },
    // Ethers and Elixirs for the League ahead, where PP runs out (Marts
    // never sell them, as in Emerald).
    [RDUN_VICTORY] = { ITEM_FULL_RESTORE, ITEM_PP_UP, ITEM_MAX_ELIXIR, ITEM_ELIXIR, ITEM_MAX_ETHER, ITEM_RARE_CANDY, ITEM_DRAGON_FANG, ITEM_FULL_HEAL },
    [RDUN_FOREST]  = { ITEM_TINY_MUSHROOM, ITEM_BIG_MUSHROOM, ITEM_BALM_MUSHROOM, ITEM_MIRACLE_SEED, ITEM_LEAF_STONE, ITEM_BIG_ROOT, ITEM_SILVER_POWDER, ITEM_LUM_BERRY },
    [RDUN_TOWER]   = { ITEM_CLEANSE_TAG, ITEM_SPELL_TAG, ITEM_DUSK_STONE, ITEM_REAPER_CLOTH, ITEM_TWISTED_SPOON, ITEM_ETHER, ITEM_DUSK_BALL, ITEM_SHINY_STONE },
    [RDUN_HIDEOUT] = { ITEM_NUGGET, ITEM_ULTRA_BALL, ITEM_ESCAPE_ROPE, ITEM_FULL_HEAL, ITEM_BLACK_GLASSES, ITEM_POISON_BARB, ITEM_X_ATTACK, ITEM_MAX_REPEL },
    // A legend's chamber: old things from the stars and the deep.
    [RDUN_LAIR]    = { ITEM_STARDUST, ITEM_STAR_PIECE, ITEM_COMET_SHARD, ITEM_MOON_STONE, ITEM_SHINY_STONE, ITEM_DAWN_STONE, ITEM_PP_UP, ITEM_ELIXIR },
    // Ruins: old things in the sand.
    [RDUN_RUINS]   = { ITEM_STARDUST, ITEM_STAR_PIECE, ITEM_RARE_BONE, ITEM_NUGGET, ITEM_SUN_STONE, ITEM_MOON_STONE, ITEM_SMOOTH_ROCK, ITEM_HARD_STONE },
    // What the sea left in a wreck's cabins, as the Abandoned Ship's; what
    // lies about a power plant, as New Mauville's Thunder Stone.
    [RDUN_SHIP]    = { ITEM_PEARL, ITEM_BIG_PEARL, ITEM_MYSTIC_WATER, ITEM_SPELL_TAG, ITEM_DIVE_BALL, ITEM_DEEP_SEA_SCALE, ITEM_NUGGET, ITEM_STAR_PIECE },
    [RDUN_PLANT]   = { ITEM_MAGNET, ITEM_THUNDER_STONE, ITEM_CELL_BATTERY, ITEM_PARALYZE_HEAL, ITEM_ULTRA_BALL, ITEM_ESCAPE_ROPE, ITEM_ELECTIRIZER, ITEM_FULL_HEAL },
    // Shoal Cave's: ice and the sea's pearls and shells (its Shell Bell).
    [RDUN_ICE]     = { ITEM_NEVER_MELT_ICE, ITEM_ICE_STONE, ITEM_ICE_HEAL, ITEM_PEARL, ITEM_BIG_PEARL, ITEM_ICY_ROCK, ITEM_TM_HAIL, ITEM_SHELL_BELL },
    // Fiery Path's and Mt. Chimney's: fire, and Lavaridge's cookies.
    [RDUN_VOLCANO] = { ITEM_CHARCOAL, ITEM_FIRE_STONE, ITEM_BURN_HEAL, ITEM_LAVA_COOKIE, ITEM_HEAT_ROCK, ITEM_TM_SUNNY_DAY, ITEM_FLAME_ORB, ITEM_STARDUST },
    // (v1.1 peaks) The desert tower's: what the sand buries.
    [RDUN_MIRAGE]  = { ITEM_SOFT_SAND, ITEM_SMOOTH_ROCK, ITEM_HARD_STONE, ITEM_STARDUST, ITEM_STAR_PIECE, ITEM_SUN_STONE, ITEM_RARE_BONE, ITEM_TM_SANDSTORM },
};

static const u16 sPrizeSignature[RDUN_COUNT][6] =
{
    [RDUN_CAVE]    = { ITEM_MOON_STONE, ITEM_DUSK_STONE, ITEM_METAL_COAT, ITEM_HARD_STONE, ITEM_PP_MAX, ITEM_ICE_STONE },
    [RDUN_VICTORY] = { ITEM_PP_MAX, ITEM_MAX_ELIXIR, ITEM_DRAGON_FANG, ITEM_LIFE_ORB, ITEM_RARE_CANDY, ITEM_FULL_RESTORE },
    [RDUN_FOREST]  = { ITEM_LEAF_STONE, ITEM_MIRACLE_SEED, ITEM_SILVER_POWDER, ITEM_BIG_ROOT, ITEM_SUN_STONE, ITEM_BALM_MUSHROOM },
    [RDUN_TOWER]   = { ITEM_SPELL_TAG, ITEM_DUSK_STONE, ITEM_CLEANSE_TAG, ITEM_TWISTED_SPOON, ITEM_REAPER_CLOTH, ITEM_SHINY_STONE },
    // Emerald hides its Master Ball in Team Aqua's hideout.
    [RDUN_HIDEOUT] = { ITEM_MASTER_BALL, ITEM_MASTER_BALL, ITEM_MASTER_BALL, ITEM_MASTER_BALL, ITEM_MASTER_BALL, ITEM_MASTER_BALL },
    [RDUN_LAIR]    = { ITEM_COMET_SHARD, ITEM_SHINY_STONE, ITEM_DAWN_STONE, ITEM_PP_MAX, ITEM_STAR_PIECE, ITEM_MOON_STONE },
    // The hold's treasure, as the Scanner the Abandoned Ship hides: the
    // sea's evolution items, or a TM of the water or the dead.
    [RDUN_SHIP]    = { ITEM_DEEP_SEA_TOOTH, ITEM_DEEP_SEA_SCALE, ITEM_TM_SHADOW_BALL, ITEM_TM_WATER_PULSE, ITEM_BIG_PEARL, ITEM_REAPER_CLOTH },
    [RDUN_PLANT]   = { ITEM_TM_THUNDERBOLT, ITEM_TM_SHOCK_WAVE, ITEM_MAGNET, ITEM_THUNDER_STONE, ITEM_CELL_BATTERY, ITEM_TM_THUNDER },
    // Ruins: what the guardian kept is worth the riddle (the stones are the
    // vault's choice beside it): an old one's held item, or a TM of the
    // mind or the dead, never a sell-only shard.
    [RDUN_RUINS]   = { ITEM_EVIOLITE, ITEM_LEFTOVERS, ITEM_TM_PSYCHIC, ITEM_TM_CALM_MIND, ITEM_TM_SHADOW_BALL, ITEM_PP_MAX },
    [RDUN_ICE]     = { ITEM_ICE_STONE, ITEM_NEVER_MELT_ICE, ITEM_TM_ICE_BEAM, ITEM_TM_BLIZZARD, ITEM_ICY_ROCK, ITEM_SHELL_BELL },
    [RDUN_VOLCANO] = { ITEM_FIRE_STONE, ITEM_CHARCOAL, ITEM_HEAT_ROCK, ITEM_TM_FLAMETHROWER, ITEM_TM_FIRE_BLAST, ITEM_TM_OVERHEAT },
    [RDUN_MIRAGE]  = { ITEM_TM_EARTHQUAKE, ITEM_TM_DIG, ITEM_SOFT_SAND, ITEM_SMOOTH_ROCK, ITEM_SUN_STONE, ITEM_TM_ROCK_TOMB },
};

// What a wreck's storage room keeps, as the Abandoned Ship's keeps TM13
// Ice Beam: TMs of the sea.
static const u16 sShipStorage[4] = { ITEM_TM_ICE_BEAM, ITEM_TM_RAIN_DANCE, ITEM_TM_BLIZZARD, ITEM_TM_WATER_PULSE };

// A ruins vault's treasure: a choice of the old stones, one to take.
static const u16 sRuinsChoice[8] =
{
    ITEM_SUN_STONE, ITEM_MOON_STONE, ITEM_DAWN_STONE, ITEM_DUSK_STONE, ITEM_SHINY_STONE, ITEM_FIRE_STONE,
    ITEM_WATER_STONE, ITEM_THUNDER_STONE,
};

static const u16 sMasterTMs[RDUN_COUNT][6] =
{
    [RDUN_CAVE]    = { ITEM_TM_DIG, ITEM_TM_ROCK_TOMB, ITEM_TM_EARTHQUAKE, ITEM_TM_IRON_TAIL, ITEM_TM_SANDSTORM, ITEM_TM_BULK_UP },
    [RDUN_VICTORY] = { ITEM_TM_EARTHQUAKE, ITEM_TM_DRAGON_CLAW, ITEM_TM_FOCUS_PUNCH, ITEM_TM_HYPER_BEAM, ITEM_TM_OVERHEAT, ITEM_TM_ICE_BEAM },
    [RDUN_FOREST]  = { ITEM_TM_GIGA_DRAIN, ITEM_TM_BULLET_SEED, ITEM_TM_SLUDGE_BOMB, ITEM_TM_SOLAR_BEAM, ITEM_TM_TOXIC, ITEM_TM_SUNNY_DAY },
    [RDUN_TOWER]   = { ITEM_TM_SHADOW_BALL, ITEM_TM_CALM_MIND, ITEM_TM_PSYCHIC, ITEM_TM_SKILL_SWAP, ITEM_TM_LIGHT_SCREEN, ITEM_TM_REFLECT },
    [RDUN_HIDEOUT] = { ITEM_TM_SLUDGE_BOMB, ITEM_TM_TAUNT, ITEM_TM_THIEF, ITEM_TM_TORMENT, ITEM_TM_SNATCH, ITEM_TM_TOXIC },
    [RDUN_LAIR]    = { ITEM_TM_CALM_MIND, ITEM_TM_PSYCHIC, ITEM_TM_ICE_BEAM, ITEM_TM_THUNDERBOLT, ITEM_TM_FLAMETHROWER, ITEM_TM_HYPER_BEAM },
    [RDUN_RUINS]   = { ITEM_TM_ROCK_TOMB, ITEM_TM_SANDSTORM, ITEM_TM_PSYCHIC, ITEM_TM_CALM_MIND, ITEM_TM_EARTHQUAKE, ITEM_TM_SHADOW_BALL },
    [RDUN_ICE]     = { ITEM_TM_ICE_BEAM, ITEM_TM_BLIZZARD, ITEM_TM_HAIL, ITEM_TM_WATER_PULSE, ITEM_TM_RAIN_DANCE, ITEM_TM_ICE_BEAM },
    [RDUN_VOLCANO] = { ITEM_TM_FLAMETHROWER, ITEM_TM_FIRE_BLAST, ITEM_TM_OVERHEAT, ITEM_TM_SUNNY_DAY, ITEM_TM_EARTHQUAKE, ITEM_TM_ROCK_TOMB },
    [RDUN_MIRAGE]  = { ITEM_TM_EARTHQUAKE, ITEM_TM_SANDSTORM, ITEM_TM_DIG, ITEM_TM_ROCK_TOMB, ITEM_TM_IRON_TAIL, ITEM_TM_BULK_UP },
};

// ---------------------------------------------------------------------------
// Towns: a gift house's gift and a townsperson's, by the town's theme, like
// Dewford's Silk Scarf, Rustboro's Quick Claw or Lavaridge's Charcoal.
// small: a townsperson may hand it out too.

struct RogueGiftDef
{
    u16 item;
    u8 qty, from, small;
};

#define GIFT(item, qty, from, small) { ITEM_##item, qty, from, small }
#define MAX_THEME_GIFTS 9

static const struct RogueGiftDef sThemeGifts[RTHEME_COUNT][MAX_THEME_GIFTS] =
{
    [RTHEME_MEADOW]  = { GIFT(QUICK_CLAW, 1, 0, 1), GIFT(EXP_CANDY_S, 3, 0, 0), GIFT(ORAN_BERRY, 3, 0, 1), GIFT(SILK_SCARF, 1, 0, 1),
                         GIFT(SOOTHE_BELL, 1, 0, 1), GIFT(GREAT_BALL, 5, 1, 0), GIFT(AMULET_COIN, 1, 1, 0), GIFT(LUCKY_EGG, 1, 3, 0),
                         GIFT(SITRUS_BERRY, 2, 1, 1) },
    [RTHEME_VERDANT] = { GIFT(MIRACLE_SEED, 1, 0, 1), GIFT(LEAF_STONE, 1, 0, 0), GIFT(MAGNET, 1, 0, 1), GIFT(THUNDER_STONE, 1, 0, 0),
                         GIFT(BIG_ROOT, 1, 2, 1), GIFT(SITRUS_BERRY, 3, 1, 1), GIFT(LUM_BERRY, 2, 2, 1), GIFT(EXP_CANDY_M, 2, 2, 0),
                         GIFT(TM_GIGA_DRAIN, 1, 2, 0) },
    [RTHEME_HARBOR]  = { GIFT(MYSTIC_WATER, 1, 0, 1), GIFT(DIVE_BALL, 5, 0, 0), GIFT(NET_BALL, 5, 0, 0), GIFT(WATER_STONE, 1, 0, 0),
                         GIFT(SOOTHE_BELL, 1, 0, 1), GIFT(BIG_PEARL, 1, 2, 1), GIFT(SHELL_BELL, 1, 3, 0), GIFT(TM_WATER_PULSE, 1, 0, 0),
                         GIFT(DEEP_SEA_SCALE, 1, 3, 1) },
    [RTHEME_ISLAND]  = { GIFT(TWISTED_SPOON, 1, 0, 1), GIFT(SUN_STONE, 1, 0, 0), GIFT(MOON_STONE, 1, 0, 0), GIFT(STAR_PIECE, 1, 2, 1),
                         GIFT(DAWN_STONE, 1, 2, 0), GIFT(LIGHT_CLAY, 1, 3, 1), GIFT(TM_CALM_MIND, 1, 3, 0), GIFT(STARDUST, 2, 0, 1),
                         GIFT(COMET_SHARD, 1, 6, 0) },
    [RTHEME_STONE]   = { GIFT(HARD_STONE, 1, 0, 1), GIFT(QUICK_CLAW, 1, 0, 1), GIFT(EVERSTONE, 1, 0, 1), GIFT(REPEAT_BALL, 5, 1, 0),
                         GIFT(TIMER_BALL, 5, 2, 0), GIFT(EXP_CANDY_M, 2, 2, 0), GIFT(METAL_COAT, 1, 2, 1), GIFT(TM_ROCK_TOMB, 1, 0, 0),
                         GIFT(LINKING_CORD, 1, 2, 0) },
    [RTHEME_COVE]    = { GIFT(LUXURY_BALL, 3, 0, 0), GIFT(BLACK_GLASSES, 1, 0, 1), GIFT(WISE_GLASSES, 1, 2, 1), GIFT(MUSCLE_BAND, 1, 2, 1),
                         GIFT(EXP_CANDY_L, 1, 5, 0), GIFT(MAX_REPEL, 3, 4, 1), GIFT(LUCKY_EGG, 1, 3, 0), GIFT(TM_RETURN, 1, 2, 0),
                         GIFT(PP_UP, 1, 3, 0) },
    [RTHEME_BEACH]   = { GIFT(SILK_SCARF, 1, 0, 1), GIFT(BLACK_BELT, 1, 0, 1), GIFT(SHELL_BELL, 1, 3, 0), GIFT(SOFT_SAND, 1, 0, 1),
                         GIFT(NET_BALL, 5, 0, 0), GIFT(BIG_PEARL, 1, 2, 1), GIFT(KINGS_ROCK, 1, 2, 0), GIFT(TM_BULK_UP, 1, 2, 0),
                         GIFT(PEARL, 2, 0, 1) },
    [RTHEME_ASH]     = { GIFT(SOFT_SAND, 1, 0, 1), GIFT(MOON_STONE, 1, 0, 0), GIFT(STARDUST, 2, 0, 1), GIFT(STAR_PIECE, 1, 2, 1),
                         GIFT(POISON_BARB, 1, 0, 1), GIFT(SMOOTH_ROCK, 1, 3, 1), GIFT(SHINY_STONE, 1, 2, 0), GIFT(TM_SANDSTORM, 1, 2, 0),
                         GIFT(TM_DIG, 1, 1, 0) },
    [RTHEME_SPRING]  = { GIFT(CHARCOAL, 1, 0, 1), GIFT(FIRE_STONE, 1, 0, 0), GIFT(LAVA_COOKIE, 3, 0, 1), GIFT(MOOMOO_MILK, 3, 2, 1),
                         GIFT(HEAT_ROCK, 1, 3, 1), GIFT(RAWST_BERRY, 3, 0, 1), GIFT(EXP_CANDY_M, 2, 2, 0), GIFT(TM_SUNNY_DAY, 1, 1, 0),
                         GIFT(MAGMARIZER, 1, 3, 0) },
    [RTHEME_TREETOP] = { GIFT(SHARP_BEAK, 1, 0, 1), GIFT(SILVER_POWDER, 1, 0, 1), GIFT(LEAF_STONE, 1, 0, 0), GIFT(MENTAL_HERB, 1, 2, 1),
                         GIFT(WHITE_HERB, 1, 2, 1), GIFT(SALAC_BERRY, 1, 4, 1), GIFT(QUICK_BALL, 5, 2, 0), GIFT(WIDE_LENS, 1, 2, 0),
                         GIFT(TM_AERIAL_ACE, 1, 0, 0) },
    // v1.1 towns2: the raft village (sea gifts) and the crater town.
    [RTHEME_RAFT]    = { GIFT(MYSTIC_WATER, 1, 0, 1), GIFT(NET_BALL, 5, 0, 0), GIFT(DIVE_BALL, 5, 0, 0), GIFT(SHELL_BELL, 1, 3, 0),
                         GIFT(PEARL, 2, 0, 1), GIFT(BIG_PEARL, 1, 2, 1), GIFT(WATER_STONE, 1, 0, 0), GIFT(DAMP_ROCK, 1, 3, 1),
                         GIFT(TM_WATER_PULSE, 1, 0, 0) },
    [RTHEME_CRATER]  = { GIFT(MYSTIC_WATER, 1, 0, 1), GIFT(HARD_STONE, 1, 0, 1), GIFT(STAR_PIECE, 1, 2, 1), GIFT(WATER_STONE, 1, 0, 0),
                         GIFT(DAWN_STONE, 1, 2, 0), GIFT(SMOOTH_ROCK, 1, 3, 1), GIFT(EXP_CANDY_M, 2, 2, 0), GIFT(TM_CALM_MIND, 1, 3, 0),
                         GIFT(PP_UP, 1, 3, 0) },
};

#undef GIFT

// ---------------------------------------------------------------------------
// The ledger

struct ItemLedger
{
    u8 count[(NUM_CATALOGUE + 1) / 2]; // a nibble per catalogue entry
    u16 roleFirst[RIR_COUNT + 1];
    u8 flags;
};

static u32 LedgerCount(const struct ItemLedger *l, u32 e)
{
    return (l->count[e >> 1] >> ((e & 1) * 4)) & 0xF;
}

static void LedgerAdd(struct ItemLedger *l, u32 e)
{
    if (LedgerCount(l, e) < 15)
        l->count[e >> 1] += 1 << ((e & 1) * 4);
}

static void LedgerInit(struct ItemLedger *l, u8 flags)
{
    u32 i, role = 0;
    memset(l, 0, sizeof(*l));
    l->flags = flags;
    for (i = 0; i < NUM_CATALOGUE; i++)
        while (role <= sItemCatalogue[i].role)
            l->roleFirst[role++] = i;
    while (role <= RIR_COUNT)
        l->roleFirst[role++] = NUM_CATALOGUE;
}

static s32 CatalogueIndex(u16 item)
{
    u32 i;
    for (i = 0; i < NUM_CATALOGUE; i++)
        if (sItemCatalogue[i].item == item)
            return i;
    return -1;
}

static bool8 Eligible(const struct ItemLedger *l, u32 e, u32 stage)
{
    const struct RogueItemDef *d = &sItemCatalogue[e];
    if (d->role == RIR_BATTLE && (l->flags & ROGUE_ITEMS_HARDCORE))
        return FALSE;
    return d->from <= stage && stage <= d->until && LedgerCount(l, e) < d->cap;
}

// An entry of one role, by weight; type (if not TYPE_NONE) keeps to items of
// that type. -1 if none is left.
static s32 PickInRole(const struct ItemLedger *l, u32 role, u32 stage, u32 type, u32 *rng)
{
    u32 i, total = 0, pick;
    for (i = l->roleFirst[role]; i < l->roleFirst[role + 1]; i++)
        if (Eligible(l, i, stage) && (type == TYPE_NONE || sItemCatalogue[i].type == type))
            total += sItemCatalogue[i].weight;
    if (total == 0)
        return -1;
    pick = RogueRng_Range(rng, total);
    for (i = l->roleFirst[role]; i < l->roleFirst[role + 1]; i++)
    {
        if (!Eligible(l, i, stage) || (type != TYPE_NONE && sItemCatalogue[i].type != type))
            continue;
        if (pick < sItemCatalogue[i].weight)
            return i;
        pick -= sItemCatalogue[i].weight;
    }
    return -1;
}

static bool8 IsHealRole(u32 role)
{
    return role == RIR_HEAL || role == RIR_STATUS || role == RIR_PP;
}

// A draw with a source's taste: a role by weight, then an item of it.
static s32 DrawTaste(const struct ItemLedger *l, u32 taste, u32 stage, u32 *rng)
{
    u32 w[RIR_COUNT], total = 0, role, tries;
    if (stage > 8)
        stage = 8;
    for (role = 0; role < RIR_COUNT; role++)
    {
        w[role] = sTastes[taste][role] * 12;
        if (IsHealRole(role))
            w[role] = sTastes[taste][role] * sHealScale[stage];
        if (role == RIR_BATTLE && (l->flags & ROGUE_ITEMS_HARDCORE))
            w[role] = 0;
        total += w[role];
    }
    for (tries = 0; tries < RIR_COUNT && total != 0; tries++)
    {
        u32 pick = RogueRng_Range(rng, total);
        s32 e;
        for (role = 0; role < RIR_COUNT - 1 && pick >= w[role]; role++)
            pick -= w[role];
        e = PickInRole(l, role, stage, TYPE_NONE, rng);
        if (e >= 0)
            return e;
        // Nothing of that role is left: another role.
        total -= w[role];
        w[role] = 0;
    }
    return -1;
}

// One of a list of items, if any is still to give at this stage (a list's
// items ignore their last stage: a kind's own find is always welcome).
static s32 DrawList(const struct ItemLedger *l, const u16 *list, u32 n, u32 stage, u32 *rng)
{
    s32 ok[8];
    u32 i, count = 0;
    for (i = 0; i < n && count < 8; i++)
    {
        s32 e = CatalogueIndex(list[i]);
        if (e >= 0 && sItemCatalogue[e].from <= stage && LedgerCount(l, e) < sItemCatalogue[e].cap
         && !(sItemCatalogue[e].role == RIR_BATTLE && (l->flags & ROGUE_ITEMS_HARDCORE)))
            ok[count++] = e;
    }
    if (count == 0)
        return -1;
    return ok[RogueRng_Range(rng, count)];
}

// The gift of a town's gift house (small == FALSE) or townsperson (TRUE).
static s32 DrawGift(const struct ItemLedger *l, u32 theme, u32 stage, bool8 small, u32 *rng, u8 *qty)
{
    const struct RogueGiftDef *g = sThemeGifts[theme < RTHEME_COUNT ? theme : 0];
    s32 ok[MAX_THEME_GIFTS];
    u32 i, n = 0, k;
    for (i = 0; i < MAX_THEME_GIFTS; i++)
    {
        s32 e = CatalogueIndex(g[i].item);
        if (g[i].item == ITEM_NONE || g[i].from > stage || (small && !g[i].small))
            continue;
        if (e >= 0 && LedgerCount(l, e) >= sItemCatalogue[e].cap)
            continue;
        ok[n++] = i;
    }
    if (n == 0)
        return -1;
    k = ok[RogueRng_Range(rng, n)];
    *qty = g[k].qty;
    return k;
}

// Stage of a place: the badges won when its way in first opens.
static u32 PlaceStage(const struct RogueRegion *r, u32 place)
{
    u32 s = r->places[place].badges;
    if (place == r->victoryRoad || place == r->summit)
        s = 8;
    return s > 8 ? 8 : s;
}

// Towns with a townsperson who hands out a small gift: about two in five,
// never the first town or the League.
bool8 RogueGen_HasFolkGift(const struct RogueRegion *r, u8 town)
{
    u32 place;
    if (town >= r->nTowns)
        return FALSE;
    place = r->towns[town];
    if (place == r->start || place == r->summit)
        return FALSE;
    return RogueRng_Hash(r->seed, 17100 + town) % 100 < 40;
}

// The master's types by dungeon kind, as in src/rogue_dungeon.c.
static bool8 DungeonHasMaster(const struct RogueDungeon *d)
{
    return d->boss == RBOSS_MASTER;
}

// Hidden items: how many a place hides, and the first one's index among
// the region's (FLAG_ROGUE_HIDDEN_0 + index). Routes hide one or two (one
// more off the main road), caves and sea routes one; towns none.
u8 RogueGen_HiddenCount(const struct RogueRegion *r, u8 place)
{
    const struct RoguePlace *p = &r->places[place];
    u32 h = RogueRng_Hash(r->seed, 17200 + place), n;
    if (p->kind == RPLACE_TOWN)
        return 0;
    if (p->kind == RPLACE_CAVE || p->type == RROUTE_SEA)
        n = 1;
    else
        n = 1 + (h % 100 < 50);
    if (p->side && n < ROGUE_HIDDEN_PER_PLACE)
        n++;
    return n;
}

u8 RogueGen_HiddenBase(const struct RogueRegion *r, u8 place)
{
    u32 i, base = 0;
    for (i = 0; i < place && i < r->nPlaces; i++)
    {
        YIELD();
        base += RogueGen_HiddenCount(r, i);
    }
    return base > ROGUE_MAX_HIDDEN ? ROGUE_MAX_HIDDEN : base;
}

// The floor of a dungeon item flag (an index into the region's item flags).
// The item flag of a wreck's storage find: the first find of the floor
// with the storage room (GenRoomFloor puts it there), or 0xFF.
static u32 ShipStorageItem(const struct RogueRegion *r, u32 dungeon)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon];
    u8 f, first, count;
    for (f = 1; f <= d->nFloors && d->kind == RDUN_SHIP; f++)
    {
        if (StorageSpot(r, dungeon, f) == NULL)
            continue;
        FloorSlots(r, dungeon, f, TRUE, &first, &count);
        return count ? d->itemBase + first : 0xFF;
    }
    return 0xFF;
}

static u32 FloorOfItem(const struct RogueRegion *r, u32 dungeon, u32 flag)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon];
    u8 f, first, count;
    for (f = 1; f <= d->nFloors; f++)
    {
        FloorSlots(r, dungeon, f, TRUE, &first, &count);
        if (flag >= d->itemBase + first && flag < d->itemBase + first + count)
            return f;
    }
    return d->nFloors;
}

struct LedgerQuery
{
    u8 source, index;
    u16 item;
    u8 qty;
    bool8 found;
    RogueItemSink sink; // the host's tally gets every draw
};

static void Hand(struct ItemLedger *l, struct LedgerQuery *q, u32 source, u32 index, s32 e, u16 item, u8 qty, u32 stage, u32 taste)
{
    if (e >= 0)
    {
        LedgerAdd(l, e);
        item = sItemCatalogue[e].item;
        if (qty == 0)
            qty = sItemCatalogue[e].qty;
    }
    if (qty == 0)
        qty = 1;
    if (q->sink)
        q->sink(source, index, item, qty, stage, taste);
    if (source == q->source && index == q->index)
    {
        q->item = item;
        q->qty = qty;
        q->found = TRUE;
    }
}

// The balls a legend's dungeon keeps for it on the way in, as Emerald's
// Timer Balls wait for the Regis' braille: Dusk Balls, which work three
// times as well underground, or Timer Balls for a chamber in the open air
// (Sky Pillar's top, Southern and Faraway Island), which work better the
// longer the battle. A legend has a catch rate of 3: a Dusk Ball, or a
// Timer Ball after ten turns, catches it about one time in thirty-five
// below three quarters of its HP, one in sixteen asleep. The first two finds
// hold five each, so ten give about one chance in four awake and one in two
// asleep, and the Marts sell more.
#define LEGEND_BALLS 5
#define LEGEND_BALL_FINDS 2
static u16 LegendBall(const struct RogueRegion *r, u32 dungeon)
{
    u8 room = RogueGen_FloorRoom(r, dungeon, r->dungeons[dungeon].nFloors);
    u8 style = room < NUM_ROOMS ? sRogueRooms[room].style : ROOM_STYLE_CHAMBER;
    return style == ROOM_STYLE_PILLAR || style == ROOM_STYLE_ISLE || style == ROOM_STYLE_GLADE ? ITEM_TIMER_BALL : ITEM_DUSK_BALL;
}

// Runs the region's ledger, stopping once the query is answered (or never,
// with a sink).
static void RunLedger(const struct RogueRegion *r, struct LedgerQuery *q, u8 flags)
{
    struct ItemLedger l;
    u32 i, k, nHidden;
    LedgerInit(&l, flags);

    // Gift houses, then townsfolk.
    for (i = 0; i < r->nTowns; i++)
    {
        u32 place = r->towns[i], rng = RogueRng_Hash(r->seed, 17300 + i);
        u8 qty = 1;
        s32 g;
        if (place == r->summit)
            continue;
        g = DrawGift(&l, r->places[place].theme, PlaceStage(r, place), FALSE, &rng, &qty);
        if (g >= 0)
        {
            u16 item = sThemeGifts[r->places[place].theme][g].item;
            Hand(&l, q, RITEMS_GIFT, i, CatalogueIndex(item), item, qty, PlaceStage(r, place), 0xFF);
        }
        if (q->found && !q->sink)
            return;
    }
    for (i = 0; i < r->nTowns; i++)
    {
        u32 place = r->towns[i], rng = RogueRng_Hash(r->seed, 17400 + i);
        u8 qty = 1;
        s32 g;
        if (!RogueGen_HasFolkGift(r, i))
            continue;
        g = DrawGift(&l, r->places[place].theme, PlaceStage(r, place), TRUE, &rng, &qty);
        if (g >= 0)
        {
            u16 item = sThemeGifts[r->places[place].theme][g].item;
            Hand(&l, q, RITEMS_FOLK, i, CatalogueIndex(item), item, qty, PlaceStage(r, place), 0xFF);
        }
        if (q->found && !q->sink)
            return;
    }
    // Masters: a TM of their kind, or a good held item.
    for (i = 0; i < r->nDungeons; i++)
    {
        const struct RogueDungeon *d = &r->dungeons[i];
        u32 kind = d->kind < RDUN_COUNT ? d->kind : RDUN_CAVE, rng = RogueRng_Hash(r->seed, 13900 + i);
        u32 stage = d->badges > 8 ? 8 : d->badges;
        s32 e = -1;
        if (!DungeonHasMaster(d))
            continue;
        if (RogueRng_Range(&rng, 3) != 0)
            e = DrawList(&l, sMasterTMs[kind], 6, stage + 1, &rng);
        if (e < 0)
            e = DrawTaste(&l, TASTE_MASTER, stage + 1, &rng);
        Hand(&l, q, RITEMS_MASTER, i, e, ITEM_TM_FACADE, 0, stage, TASTE_MASTER);
        if (q->found && !q->sink)
            return;
    }
    // Item balls: the places', then the dungeons'.
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        u32 stage = PlaceStage(r, i);
        u32 taste = p->kind == RPLACE_CAVE ? TASTE_CAVE : p->side ? TASTE_SIDE : TASTE_ROUTE;
        if (p->side && p->kind == RPLACE_CAVE)
            taste = TASTE_SIDE;
        for (k = 0; k < p->nItems; k++)
        {
            u32 rng = RogueRng_Hash(r->seed, 17500 + p->itemBase + k);
            Hand(&l, q, RITEMS_BALL, p->itemBase + k, DrawTaste(&l, taste, stage, &rng), ITEM_POTION, 0, stage, taste);
            if (q->found && !q->sink)
                return;
        }
    }
    for (i = 0; i < r->nDungeons; i++)
    {
        const struct RogueDungeon *d = &r->dungeons[i];
        u32 kind = d->kind < RDUN_COUNT ? d->kind : RDUN_CAVE;
        struct RogueDungeonExtras e;
        u16 picked[3] = { ITEM_NONE, ITEM_NONE, ITEM_NONE };
        u8 balls = d->boss == RBOSS_LEGEND ? LEGEND_BALL_FINDS : 0;
        u32 storage = ShipStorageItem(r, i);
        RogueGen_DungeonExtras(r, i, &e);
        for (k = d->itemBase; k < (u32)d->itemBase + d->nItems; k++)
        {
            u32 rng = RogueRng_Hash(r->seed, 17500 + k), floor, stage, taste = TASTE_FLOOR;
            s32 en = -1;
            u8 qty = 0;
            // (A camp is no item; a cabin key and a Voltorb ball neither.)
            if (k == e.camp || (e.nKeys && k >= e.keys && k < (u32)e.keys + e.nKeys)
             || (e.nDecoys && k >= e.decoys && k < (u32)e.decoys + e.nDecoys))
                continue;
            floor = FloorOfItem(r, i, k);
            // Deeper is better: a stage more a floor down.
            stage = d->badges + floor - 1;
            if (stage > 8)
                stage = 8;
            if (k == e.prize)
            {
                taste = TASTE_PRIZE;
                en = DrawList(&l, sPrizeSignature[kind], 6, 8, &rng);
                if (en < 0)
                    en = DrawTaste(&l, TASTE_PRIZE, stage + 1, &rng);
            }
            else if (k == storage)
            {
                // Behind the always locked door: worth the key.
                taste = TASTE_PRIZE;
                en = DrawList(&l, sShipStorage, 4, 8, &rng);
                if (en < 0)
                    en = DrawTaste(&l, TASTE_PRIZE, stage + 1, &rng);
            }
            else if (e.nChoice && k >= e.choice && k < (u32)e.choice + e.nChoice)
            {
                // Two or three different things to choose from.
                u32 n = k - e.choice, tries;
                taste = TASTE_CHOICE;
                for (tries = 0; tries < 8; tries++)
                {
                    u32 j;
                    en = kind == RDUN_RUINS ? DrawList(&l, sRuinsChoice, 8, 8, &rng) : -1;
                    if (en < 0)
                        en = DrawTaste(&l, TASTE_CHOICE, stage + 1, &rng);
                    for (j = 0; j < n && j < 3; j++)
                        if (en >= 0 && picked[j] == sItemCatalogue[en].item)
                            break;
                    if (j == n || j == 3)
                        break;
                }
                if (n < 3)
                    picked[n] = en >= 0 ? sItemCatalogue[en].item : ITEM_NONE;
            }
            else if (k == e.explorer)
            {
                // The lost explorer's thanks: something good.
                taste = TASTE_PRIZE;
                en = DrawTaste(&l, TASTE_PRIZE, stage + 1, &rng);
            }
            else if (balls)
            {
                // The first finds of a legend's dungeon: balls for it.
                en = CatalogueIndex(LegendBall(r, i));
                qty = LEGEND_BALLS;
                balls--;
            }
            else
            {
                // A kind's own finds now and then.
                if (RogueRng_Range(&rng, 3) == 0)
                    en = DrawList(&l, sFloorSignature[kind], 8, stage, &rng);
                if (en < 0)
                    en = DrawTaste(&l, TASTE_FLOOR, stage, &rng);
            }
            Hand(&l, q, RITEMS_BALL, k, en, ITEM_ULTRA_BALL, qty, stage, taste);
            if (q->found && !q->sink)
                return;
        }
    }
    // Hidden items.
    nHidden = 0;
    for (i = 0; i < r->nPlaces; i++)
    {
        u32 n = RogueGen_HiddenCount(r, i), stage = PlaceStage(r, i);
        for (k = 0; k < n && nHidden < ROGUE_MAX_HIDDEN; k++, nHidden++)
        {
            u32 rng = RogueRng_Hash(r->seed, 17700 + nHidden);
            Hand(&l, q, RITEMS_HIDDEN, nHidden, DrawTaste(&l, TASTE_HIDDEN, stage, &rng), ITEM_POTION, 0, stage, TASTE_HIDDEN);
            if (q->found && !q->sink)
                return;
        }
    }
    // The rival's gift (RIVAL'S GIFT), last so it never changes the rest: a
    // strong held item not handed out yet.
    {
        static const u16 sRivalGifts[] = { ITEM_LEFTOVERS, ITEM_LIFE_ORB, ITEM_CHOICE_SCARF, ITEM_EXPERT_BELT, ITEM_SHELL_BELL,
                                           ITEM_SCOPE_LENS, ITEM_QUICK_CLAW, ITEM_FOCUS_BAND, ITEM_ASSAULT_VEST, ITEM_LUCKY_EGG };
        u32 rng = RogueRng_Hash(r->seed, 9430);
        s32 en = DrawList(&l, sRivalGifts, 8, 8, &rng);
        if (en < 0)
            en = DrawList(&l, sRivalGifts + 8, 2, 8, &rng);
        Hand(&l, q, RITEMS_RIVAL, 0, en, ITEM_LEFTOVERS, 1, 8, 0xFF);
    }
    // The tunnel's hidden item by its far mouth, after everything else so
    // nothing else changes: a find a stretch ahead of the far end's.
    if (RogueGen_TunnelHidden(r) < ROGUE_MAX_HIDDEN)
    {
        u32 rng = RogueRng_Hash(r->seed, 9440), stage = PlaceStage(r, r->dungeons[r->tunnel].exit) + 1;
        if (stage > 8)
            stage = 8;
        Hand(&l, q, RITEMS_HIDDEN, RogueGen_TunnelHidden(r), DrawTaste(&l, TASTE_HIDDEN, stage, &rng), ITEM_POTION, 0, stage, TASTE_HIDDEN);
    }
    // Town signatures' small gifts (src/rogue_gen_signatures.h), last of all
    // so they move nothing else: a LAVA COOKIE at the hot spring, MOOMOO
    // MILK at the ranch.
    for (i = 0; i < r->nTowns; i++)
    {
        u8 qty;
        u16 item = RogueGen_SignatureItem(r, r->towns[i], &qty);
        if (item != ITEM_NONE)
            Hand(&l, q, RITEMS_SIGNATURE, i, CatalogueIndex(item), item, qty, PlaceStage(r, r->towns[i]), 0xFF);
    }
    // Route features' rewards (src/rogue_gen_features.h), after everything
    // else so they move nothing: the BERRY MASTER's berries, the glassblower's
    // flute, the Seashore House's SODA POP, the family's MACHO BRACE, the
    // gauntlet's NUGGET, the fishing guru's LURE BALLS.
    for (i = 1; i < RFEAT_COUNT; i++)
    {
        u8 qty;
        u16 item = RogueGen_FeatureItem(r, i, &qty);
        if (item != ITEM_NONE)
            Hand(&l, q, RITEMS_FEATURE, i, CatalogueIndex(item), item, qty, PlaceStage(r, RogueGen_FeatureRoute(r, i)), 0xFF);
    }
}

u16 RogueGen_ItemFor(const struct RogueRegion *r, u8 source, u8 index, u8 *qty, u8 flags)
{
    struct LedgerQuery q;
    memset(&q, 0, sizeof(q));
    q.source = source;
    q.index = index;
    RunLedger(r, &q, flags);
    if (qty)
        *qty = q.found ? q.qty : 1;
    return q.found ? q.item : ITEM_NONE;
}

void RogueGen_TallyItems(const struct RogueRegion *r, u8 flags, RogueItemSink sink)
{
    struct LedgerQuery q;
    memset(&q, 0, sizeof(q));
    q.source = 0xFF;
    q.sink = sink;
    RunLedger(r, &q, flags);
}

u8 RogueGen_ItemRole(u16 item)
{
    s32 e = CatalogueIndex(item);
    return e >= 0 ? sItemCatalogue[e].role : RIR_COUNT;
}

// ---------------------------------------------------------------------------
// Gym leaders' TMs: one of the leader's type, from those that suit how far
// along the gym is (Emerald's leaders give Rock Tomb first and Water Pulse
// last; strong attacks come from the fifth gym on). A few types have only
// one TM, so they borrow a fitting one.

static const u16 sGymExtraTMs[][3] =
{
    { TYPE_FLYING, ITEM_TM_STEEL_WING, ITEM_TM_AERIAL_ACE },
    { TYPE_BUG,    ITEM_TM_GIGA_DRAIN, ITEM_TM_PROTECT },
    { TYPE_FAIRY,  ITEM_TM_CALM_MIND, ITEM_TM_ATTRACT },
    { TYPE_GHOST,  ITEM_TM_SHADOW_BALL, ITEM_TM_TORMENT },
    { TYPE_DRAGON, ITEM_TM_DRAGON_CLAW, ITEM_TM_SAFEGUARD },
};

u16 RogueGen_GymTM(const struct RogueRegion *r, u8 gym, u8 type)
{
    u16 ok[12];
    u32 i, n = 0, rng = RogueRng_Hash(r->seed, 17800 + gym);
    s32 lowest = -1;
    for (i = 0; i < NUM_CATALOGUE && n < 12; i++)
    {
        const struct RogueItemDef *d = &sItemCatalogue[i];
        if (d->role != RIR_TM || d->type != type)
            continue;
        if (d->from <= gym + 1)
            ok[n++] = d->item;
        if (lowest < 0 || d->from < sItemCatalogue[lowest].from)
            lowest = i;
    }
    for (i = 0; i < sizeof(sGymExtraTMs) / sizeof(sGymExtraTMs[0]) && n < 11; i++)
        if (sGymExtraTMs[i][0] == type)
        {
            ok[n++] = sGymExtraTMs[i][1];
            ok[n++] = sGymExtraTMs[i][2];
        }
    if (n == 0)
        return lowest >= 0 ? sItemCatalogue[lowest].item : ITEM_TM_FACADE;
    return ok[RogueRng_Range(&rng, n)];
}

// ---------------------------------------------------------------------------
// Specialty counters: a second clerk in the Mart of a themed town, like
// Lavaridge's herb shop, Slateport's Energy Guru or Lilycove's department
// store.

static const u8 sThemeShop[RTHEME_COUNT] =
{
    [RTHEME_MEADOW]  = RSHOP_NONE,
    [RTHEME_VERDANT] = RSHOP_BATTLE,
    [RTHEME_HARBOR]  = RSHOP_VITAMIN,
    [RTHEME_ISLAND]  = RSHOP_STONE,
    [RTHEME_STONE]   = RSHOP_BALL,
    [RTHEME_COVE]    = RSHOP_TM,
    [RTHEME_BEACH]   = RSHOP_SEA,
    [RTHEME_ASH]     = RSHOP_HELD,
    [RTHEME_SPRING]  = RSHOP_HERB,
    [RTHEME_TREETOP] = RSHOP_TYPE,
    [RTHEME_RAFT]    = RSHOP_SEA,
    [RTHEME_CRATER]  = RSHOP_STONE,
};

struct ShopDef
{
    u16 item;
    u8 from;
};

static const struct ShopDef sShopFixed[][12] =
{
    [RSHOP_BATTLE]  = { { ITEM_X_ATTACK, 0 }, { ITEM_X_DEFENSE, 0 }, { ITEM_X_SP_ATK, 0 }, { ITEM_X_SP_DEF, 0 }, { ITEM_X_SPEED, 0 },
                        { ITEM_X_ACCURACY, 0 }, { ITEM_DIRE_HIT, 0 }, { ITEM_GUARD_SPEC, 0 } },
    [RSHOP_VITAMIN] = { { ITEM_HP_UP, 0 }, { ITEM_PROTEIN, 0 }, { ITEM_IRON, 0 }, { ITEM_CALCIUM, 0 }, { ITEM_ZINC, 0 },
                        { ITEM_CARBOS, 0 }, { ITEM_PP_UP, 4 } },
    [RSHOP_STONE]   = { { ITEM_FIRE_STONE, 0 }, { ITEM_WATER_STONE, 0 }, { ITEM_THUNDER_STONE, 0 }, { ITEM_LEAF_STONE, 0 },
                        { ITEM_MOON_STONE, 0 }, { ITEM_SUN_STONE, 3 }, { ITEM_SHINY_STONE, 4 }, { ITEM_DUSK_STONE, 4 },
                        { ITEM_DAWN_STONE, 5 }, { ITEM_ICE_STONE, 5 }, { ITEM_LINKING_CORD, 6 } },
    [RSHOP_BALL]    = { { ITEM_NEST_BALL, 0 }, { ITEM_NET_BALL, 0 }, { ITEM_REPEAT_BALL, 1 }, { ITEM_TIMER_BALL, 2 },
                        { ITEM_DIVE_BALL, 2 }, { ITEM_QUICK_BALL, 3 }, { ITEM_DUSK_BALL, 3 }, { ITEM_LUXURY_BALL, 1 },
                        { ITEM_PREMIER_BALL, 0 } },
    [RSHOP_SEA]     = { { ITEM_FRESH_WATER, 0 }, { ITEM_SODA_POP, 0 }, { ITEM_LEMONADE, 0 }, { ITEM_NET_BALL, 0 },
                        { ITEM_DIVE_BALL, 0 }, { ITEM_MOOMOO_MILK, 3 }, { ITEM_SUPER_REPEL, 2 } },
    // The battle counter under the Hardcore rules, where X items do nothing:
    // berries a Pokemon eats in battle by itself.
    [RSHOP_COUNT]   = { { ITEM_ORAN_BERRY, 0 }, { ITEM_CHERI_BERRY, 0 }, { ITEM_CHESTO_BERRY, 0 }, { ITEM_PECHA_BERRY, 0 },
                        { ITEM_RAWST_BERRY, 0 }, { ITEM_ASPEAR_BERRY, 0 }, { ITEM_PERSIM_BERRY, 0 }, { ITEM_SITRUS_BERRY, 2 },
                        { ITEM_LUM_BERRY, 4 } },
    [RSHOP_HERB]    = { { ITEM_ENERGY_POWDER, 0 }, { ITEM_ENERGY_ROOT, 0 }, { ITEM_HEAL_POWDER, 0 }, { ITEM_LAVA_COOKIE, 0 },
                        { ITEM_CHARCOAL, 0 }, { ITEM_MENTAL_HERB, 2 }, { ITEM_WHITE_HERB, 3 }, { ITEM_POWER_HERB, 5 } },
};

// The specialty counter of a town, or RSHOP_NONE: gym cities only, the first
// of each theme along the road, so a region has a handful, as Hoenn has.
u8 RogueGen_SpecialtyShop(const struct RogueRegion *r, u8 place)
{
    const struct RoguePlace *p = &r->places[place];
    u32 i;
    if (p->kind != RPLACE_TOWN || p->type != RTOWN_GYM || p->theme >= RTHEME_COUNT)
        return RSHOP_NONE;
    for (i = 0; i < p->number && i < r->nTowns; i++)
    {
        const struct RoguePlace *q = &r->places[r->towns[i]];
        if (q->type == RTOWN_GYM && q->theme == p->theme)
            return RSHOP_NONE;
    }
    return sThemeShop[p->theme];
}

// What the counter sells with this many badges, into out (at most max, with
// ITEM_NONE after): the fixed shops grow with the badges; the TM counter,
// the held item exchange and the type item stall pick their stock per run
// and town from the catalogue.
u32 RogueGen_SpecialtyStock(const struct RogueRegion *r, u8 place, u8 badges, u16 *out, u32 max, u8 flags)
{
    u32 shop = RogueGen_SpecialtyShop(r, place), n = 0, i;
    if (shop == RSHOP_NONE || max == 0)
        return 0;
    if (shop == RSHOP_BATTLE && (flags & ROGUE_ITEMS_HARDCORE))
        shop = RSHOP_COUNT;
    if (shop == RSHOP_TM || shop == RSHOP_HELD || shop == RSHOP_TYPE)
    {
        u32 role = shop == RSHOP_TM ? RIR_TM : shop == RSHOP_HELD ? RIR_HELD : RIR_TYPE;
        u32 want = shop == RSHOP_TYPE ? 6 : 7, rng = RogueRng_Hash(r->seed, 17900 + place);
        u32 stage = badges > 8 ? 8 : badges;
        u8 taken[NUM_CATALOGUE / 8 + 1];
        memset(taken, 0, sizeof(taken));
        for (i = 0; i < 400 && n < want && n + 1 < max; i++)
        {
            u32 e = RogueRng_Range(&rng, NUM_CATALOGUE);
            // Not the most powerful held items: those are found, not bought.
            if (sItemCatalogue[e].role != role || sItemCatalogue[e].from > stage || ((taken[e / 8] >> (e % 8)) & 1)
             || (role == RIR_HELD && sItemCatalogue[e].from >= 5))
                continue;
            taken[e / 8] |= 1 << (e % 8);
            out[n++] = sItemCatalogue[e].item;
        }
        // Keep the catalogue's order, so the list reads tidily.
        for (i = 1; i < n; i++)
        {
            u32 j = i;
            u16 v = out[i];
            while (j > 0 && CatalogueIndex(out[j - 1]) > CatalogueIndex(v))
            {
                out[j] = out[j - 1];
                j--;
            }
            out[j] = v;
        }
    }
    else
    {
        for (i = 0; i < 12 && n + 1 < max && sShopFixed[shop][i].item != ITEM_NONE; i++)
            if (sShopFixed[shop][i].from <= badges)
                out[n++] = sShopFixed[shop][i].item;
    }
    out[n] = ITEM_NONE;
    return n;
}
