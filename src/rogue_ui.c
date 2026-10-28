// The region map, the run summary and the UNLOCKS screen.
//
// All are simple full screens: a tile background drawn straight into VRAM
// and one text window on top. The map is drawn with the tiles of Hoenn's
// region map; the summary's backdrop comes from a small custom tileset
// (tools/roguemon/make_map_tiles.py).

#include "global.h"
#include "rogue.h"
#include "rogue_run0_story.h"
#include "rogue_gen.h"
#include "battle_setup.h"
#include "bg.h"
#include "event_data.h"
#include "gpu_regs.h"
#include "main.h"
#include "menu.h"
#include "overworld.h"
#include "palette.h"
#include "battle_main.h"
#include "save.h"
#include "main_menu.h"
#include "strings.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "text_window.h"
#include "window.h"
#include "constants/rgb.h"
#include "constants/songs.h"
#include "field_effect.h"
#include "decompress.h"
#include "malloc.h"
#include "party_menu.h"
#include "international_string_util.h"
#include "list_menu.h"
#include "pokemon.h"
#include "pokemon_icon.h"
#include "naming_screen.h"
#include "m4a.h"
#include "constants/characters.h"

// The summary screen's backdrop, from graphics/rogue/map_tiles.png.
#define SUMMARY_TILE_DARK 42

// The region map uses the tiles of Hoenn's region map, laid out by
// RogueGen_DrawRegionMap. Like Emerald's map screens it fits on one screen:
// a bar at the top with the place's name, the whole region in 30x15 tiles,
// and a strip at the bottom with what the place holds.
#define MAP_TOP 2        // first tile row of the map on screen
#define MAP_VIEW_ROWS 15
#define MAP_INFO_TOP (MAP_TOP + MAP_VIEW_ROWS)
#define MAP_ROWS 64      // rows of the summary's backdrop layer

#define TILE_CHAR_BASE 2
#define BG_TEXT 0
#define BG_MAP 2
#define BG_LAND 3

#define TAG_CURSOR 0x5200
#define TAG_PLAYER 0x5201
#define TAG_MARKS  0x5203
#define TAG_DMARKS 0x5210 // the dungeon marks' tiles, and their palettes from here on

static const u16 sSummaryPalette[] = INCGFX_U16("graphics/rogue/map_tiles.png", ".gbapal");
static const u16 sSummaryTiles[] = INCGFX_U16("graphics/rogue/map_tiles.png", ".4bpp");
static const u16 sRegionMapPalette[] = INCGFX_U16("graphics/pokenav/region_map/map.pal", ".gbapal");
static const u32 sRegionMapTiles[] = INCGFX_U32("graphics/pokenav/region_map/map.png", ".8bpp.smol", "-num_tiles 233 -Wnum_tiles");
static const u16 sCursorPalette[] = INCGFX_U16("graphics/pokenav/region_map/cursor.pal", ".gbapal");
static const u8 sCursorGfx[] = INCGFX_U8("graphics/pokenav/region_map/cursor_small.png", ".4bpp");
static const u16 sBrendanIconPalette[] = INCGFX_U16("graphics/pokenav/region_map/brendan_icon.png", ".gbapal");
static const u8 sBrendanIconGfx[] = INCGFX_U8("graphics/pokenav/region_map/brendan_icon.png", ".4bpp");
static const u16 sMayIconPalette[] = INCGFX_U16("graphics/pokenav/region_map/may_icon.png", ".gbapal");
static const u8 sMayIconGfx[] = INCGFX_U8("graphics/pokenav/region_map/may_icon.png", ".4bpp");
// Emerald's Fly map: the frame's colors for the bars, and the dots and red
// outline it puts on towns.
static const u16 sMapBarPalette[] = INCGFX_U16("graphics/pokenav/region_map/frame.png", ".gbapal");
static const u16 sMarksPalette[] = INCGFX_U16("graphics/pokenav/region_map/fly_target_icons.png", ".gbapal");
static const u32 sMarksGfx[] = INCGFX_U32("graphics/pokenav/region_map/fly_target_icons.png", ".4bpp.smol");
// FRLG's dungeon icons, one framed per kind (tools/roguemon/make_dungeon_marks.py).
static const u8 sDungeonMarksGfx[] = INCGFX_U8("graphics/rogue/dungeon_marks.png", ".4bpp");
static const u16 sDungeonMarksPalette[] = INCGFX_U16("graphics/rogue/dungeon_marks.pal", ".gbapal");
// Mt. Chimney's crater in a volcano's colors, after Hoenn's tiles.
#include "data/rogue_region_map_crater.h"

static const struct BgTemplate sMapBgTemplates[] =
{
    { .bg = BG_TEXT, .charBaseIndex = 0, .mapBaseIndex = 31, .priority = 0 },
    { .bg = BG_MAP,  .charBaseIndex = TILE_CHAR_BASE, .mapBaseIndex = 26, .screenSize = 0, .paletteMode = 1, .priority = 2 },
};

static const struct BgTemplate sSummaryBgTemplates[] =
{
    { .bg = BG_TEXT, .charBaseIndex = 0, .mapBaseIndex = 31, .priority = 0 },
    { .bg = BG_LAND, .charBaseIndex = TILE_CHAR_BASE, .mapBaseIndex = 24, .screenSize = 2, .priority = 3 },
};

// The bar at the top (the place and the region) and the strip at the
// bottom (what the place holds), in the colors of Emerald's Fly map frame.
enum { MWIN_BAR, MWIN_INFO };
#define MAP_BAR_PAL 13
static const struct WindowTemplate sMapWindows[] =
{
    [MWIN_BAR]  = { .bg = BG_TEXT, .tilemapLeft = 0, .tilemapTop = 0, .width = 30, .height = MAP_TOP, .paletteNum = MAP_BAR_PAL, .baseBlock = 11 },
    [MWIN_INFO] = { .bg = BG_TEXT, .tilemapLeft = 0, .tilemapTop = MAP_INFO_TOP, .width = 30, .height = 20 - MAP_INFO_TOP, .paletteNum = MAP_BAR_PAL, .baseBlock = 11 + 30 * MAP_TOP },
    DUMMY_WIN_TEMPLATE
};
// Colors of graphics/pokenav/region_map/frame.png.
#define BAR_PALE   1
#define BAR_GRAY   3
#define BAR_LINE   7
#define BAR_MID    8
#define BAR_DARK   9
#define BAR_GOLD   10
#define BAR_WHITE  15
static const u8 sBarText[] = { BAR_DARK, BAR_WHITE, BAR_GRAY };
static const u8 sBarRegion[] = { BAR_DARK, BAR_PALE, BAR_MID };
static const u8 sBarHere[] = { BAR_DARK, BAR_GOLD, BAR_GRAY };

static const struct WindowTemplate sSummaryWindows[] =
{
    { .bg = BG_TEXT, .tilemapLeft = 2, .tilemapTop = 2, .width = 26, .height = 16, .paletteNum = 15, .baseBlock = 11 },
    DUMMY_WIN_TEMPLATE
};

// The cursor and the player's head: 16x16 sprites.
static const struct OamData sMapIconOam =
{
    .shape = SPRITE_SHAPE(16x16),
    .size = SPRITE_SIZE(16x16),
    .priority = 1,
};

// Emerald's Fly map icons on towns (pink where you can fly, gray where you
// cannot yet), shaped like the town's mark: a dot, or a city two tiles wide
// or tall. Also the red outline on the next gym.
static const struct OamData sMapDotOam =
{
    .shape = SPRITE_SHAPE(8x8),
    .size = SPRITE_SIZE(8x8),
    .priority = 1,
};

// Laid out as Emerald's sFlyDestIcon_Anims: by sprite shape, plus 3 for
// a town you cannot fly to.
enum { MARK_FLY, MARK_FLY_W, MARK_FLY_N, MARK_NO_FLY, MARK_NO_FLY_W, MARK_NO_FLY_N, MARK_NEXT };
static const union AnimCmd sMarkAnim_Fly[] = { ANIMCMD_FRAME(0, 5), ANIMCMD_END };
static const union AnimCmd sMarkAnim_FlyW[] = { ANIMCMD_FRAME(1, 5), ANIMCMD_END };
static const union AnimCmd sMarkAnim_FlyN[] = { ANIMCMD_FRAME(3, 5), ANIMCMD_END };
static const union AnimCmd sMarkAnim_NoFly[] = { ANIMCMD_FRAME(5, 5), ANIMCMD_END };
static const union AnimCmd sMarkAnim_NoFlyW[] = { ANIMCMD_FRAME(6, 5), ANIMCMD_END };
static const union AnimCmd sMarkAnim_NoFlyN[] = { ANIMCMD_FRAME(8, 5), ANIMCMD_END };
static const union AnimCmd sMarkAnim_Next[] = { ANIMCMD_FRAME(10, 5), ANIMCMD_END };
static const union AnimCmd *const sMarkAnims[] =
{
    [MARK_FLY] = sMarkAnim_Fly,
    [MARK_FLY_W] = sMarkAnim_FlyW,
    [MARK_FLY_N] = sMarkAnim_FlyN,
    [MARK_NO_FLY] = sMarkAnim_NoFly,
    [MARK_NO_FLY_W] = sMarkAnim_NoFlyW,
    [MARK_NO_FLY_N] = sMarkAnim_NoFlyN,
    [MARK_NEXT] = sMarkAnim_Next,
};

static void SpriteCB_FlyMark(struct Sprite *sprite);
static void SpriteCB_NextGym(struct Sprite *sprite);
static void SpriteCB_Cursor(struct Sprite *sprite);

static const struct SpriteTemplate sDotTemplate =
{
    .tileTag = TAG_MARKS,
    .paletteTag = TAG_MARKS,
    .oam = &sMapDotOam,
    .anims = sMarkAnims,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteTemplate sOutlineTemplate =
{
    .tileTag = TAG_MARKS,
    .paletteTag = TAG_MARKS,
    .oam = &sMapIconOam,
    .anims = sMarkAnims,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_NextGym,
};

// The dungeon marks: frame 0 the small gray mark of a dungeon not yet
// visited, frame 1 + DMARK_* each kind's framed mark.
#define DMARK_FRAME(n) static const union AnimCmd sDMarkAnim_##n[] = { ANIMCMD_FRAME(n, 5), ANIMCMD_END }
DMARK_FRAME(0); DMARK_FRAME(1); DMARK_FRAME(2); DMARK_FRAME(3); DMARK_FRAME(4); DMARK_FRAME(5); DMARK_FRAME(6);
DMARK_FRAME(7); DMARK_FRAME(8); DMARK_FRAME(9); DMARK_FRAME(10); DMARK_FRAME(11); DMARK_FRAME(12); DMARK_FRAME(13);
static const union AnimCmd *const sDMarkAnims[DMARK_COUNT + 1] =
{
    sDMarkAnim_0, sDMarkAnim_1, sDMarkAnim_2, sDMarkAnim_3, sDMarkAnim_4, sDMarkAnim_5, sDMarkAnim_6,
    sDMarkAnim_7, sDMarkAnim_8, sDMarkAnim_9, sDMarkAnim_10, sDMarkAnim_11, sDMarkAnim_12, sDMarkAnim_13,
};

static void SpriteCB_DungeonMark(struct Sprite *sprite);

static const struct SpriteTemplate sDungeonMarkTemplate =
{
    .tileTag = TAG_DMARKS,
    .paletteTag = TAG_DMARKS,
    .oam = &sMapDotOam,
    .anims = sDMarkAnims,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_DungeonMark,
};

// Each kind of dungeon's mark (DMARK_*). A new kind adds a row; a kind
// with none gets DMARK_GENERIC, FRLG's own blue.
static const u8 sDungeonMarks[] =
{
    [RDUN_CAVE]    = DMARK_CAVE,
    [RDUN_VICTORY] = DMARK_VICTORY,
    [RDUN_FOREST]  = DMARK_FOREST,
    [RDUN_TOWER]   = DMARK_TOWER,
    [RDUN_HIDEOUT] = DMARK_HIDEOUT,
    [RDUN_LAIR]    = DMARK_LAIR,
    [RDUN_RUINS]   = DMARK_RUINS,
    [RDUN_MIRAGE]  = DMARK_TOWER, // (v1.1 peaks)
    [RDUN_SHIP]    = DMARK_SHIPWRECK,
    [RDUN_PLANT]   = DMARK_PLANT,
    [RDUN_ICE]     = DMARK_ICE,
    [RDUN_VOLCANO] = DMARK_VOLCANO,
};

static const union AnimCmd sCursorAnim[] =
{
    ANIMCMD_FRAME(0, 20),
    ANIMCMD_FRAME(4, 20),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd *const sCursorAnims[] = { sCursorAnim };

static const struct SpriteTemplate sCursorTemplate =
{
    .tileTag = TAG_CURSOR,
    .paletteTag = TAG_CURSOR,
    .oam = &sMapIconOam,
    .anims = sCursorAnims,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_Cursor,
};

static const struct SpriteTemplate sPlayerTemplate =
{
    .tileTag = TAG_PLAYER,
    .paletteTag = TAG_PLAYER,
    .oam = &sMapIconOam,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

struct MapScreen
{
    s8 tileX[ROGUE_MAX_PLACES];
    s8 tileY[ROGUE_MAX_PLACES];
    u8 selected;
    u8 current;
    u8 flying;
    u8 cursorSprite;
    u8 playerSprite;
    s8 rowOffset; // screen row of the map's row 0
    u8 height;    // rows the map uses
    u8 nMarks;
    u8 marks[30]; // sprites marking towns and legends, each with its place in data[0]
    u8 preview;   // the professor's region preview: PREVIEW_*
    u8 cursorCell; // the cursor's map cell: CELL_ROW * (tile y / 2) + tile x / 2
    u16 *tiles;   // the whole map, for a region taller than the screen
    MainCallback exitCallback;
};

static EWRAM_DATA struct MapScreen sMap = {0};
// sMap.selected when the cursor is on no place.
#define NO_PLACE 0xFF
// Cells across a row of the map (tiles 0-29 in steps of two).
#define CELL_ROW 15
enum { PREVIEW_NONE, PREVIEW_FIRST, PREVIEW_AGAIN };

// What the strip at the bottom says. No leader's name or type: the map
// shows the way, not who waits at the end of it.
static const u8 sText_NoGym[] = _("PROF. BIRCH's field lab is here.");
static const u8 sText_Village[] = _("A quiet village. No GYM here.");
static const u8 sText_Hamlet[] = _("A small town off the main road.");
static const u8 sText_GymTown[] = _("A town with a POKéMON GYM.");
static const u8 sText_NextGym[] = _("A GYM you can take on now.");
static const u8 sText_WonHere[] = _(" won here.");
static const u8 sText_WonHereKey[] = _(" won here: ");
static const u8 sText_League[] = _("The POKéMON LEAGUE.");
static const u8 sText_LeagueNext[] = _("The POKéMON LEAGUE. Your next goal.");
static const u8 sText_LeagueWon[] = _("The POKéMON LEAGUE. CHAMPION beaten!");
static const u8 *const sRouteKinds[RROUTE_COUNT] =
{
    [RROUTE_FOREST]   = COMPOUND_STRING("Forest route. "),
    [RROUTE_LAKE]     = COMPOUND_STRING("Lake route. "),
    [RROUTE_MOUNTAIN] = COMPOUND_STRING("Mountain pass. "),
    [RROUTE_COAST]    = COMPOUND_STRING("Coast route. "),
    [RROUTE_RIVER]    = COMPOUND_STRING("River route. "),
    [RROUTE_HILLS]    = COMPOUND_STRING("Hill route. "),
    [RROUTE_FIELD]    = COMPOUND_STRING("Meadow route. "),
    [RROUTE_DESERT]   = COMPOUND_STRING("Desert route. "),
    [RROUTE_SEA]      = COMPOUND_STRING("Sea route. "),
    [RROUTE_WOODS]    = COMPOUND_STRING("Deep woods. "),
    [RROUTE_FLOWERS]  = COMPOUND_STRING("Flower route. "),
    [RROUTE_LONGGRASS]= COMPOUND_STRING("Long grass route. "),
    [RROUTE_BRIDGE]   = COMPOUND_STRING("Bridge route. "),
};
static const u8 sText_Cave[] = _("Cave. ");
static const u8 sText_DarkCave[] = _("Dark cave. ");
static const u8 *const sObstacleNames[] =
{
    [ROBST_CUT]        = COMPOUND_STRING("Needs CUT. "),
    [ROBST_ROCK_SMASH] = COMPOUND_STRING("Needs ROCK SMASH. "),
    [ROBST_STRENGTH]   = COMPOUND_STRING("Needs STRENGTH. "),
    [ROBST_SURF]       = COMPOUND_STRING("Needs SURF. "),
};
static const u8 *const sKeyNames[] =
{
    [ROBST_CUT]        = COMPOUND_STRING("CUT."),
    [ROBST_ROCK_SMASH] = COMPOUND_STRING("ROCK SMASH."),
    [ROBST_STRENGTH]   = COMPOUND_STRING("STRENGTH."),
    [ROBST_SURF]       = COMPOUND_STRING("SURF."),
};
static const u8 sText_NotExplored[] = _("Not explored yet.");
static const u8 sText_CatchOpen[] = _("Can catch here.");
static const u8 sText_Caught[] = _("Caught here.");
static const u8 sText_EncounterUsed[] = _("Encounter used.");
static const u8 sText_YouAreHere[] = _("HERE");
static const u8 sText_FlyHere[] = _("A: FLY");

static const u8 sText_RunWon[] = _("{COLOR GREEN}A NEW CHAMPION!");
static const u8 sText_Run0Over[] = _("{COLOR GREEN}RUN 0 IS OVER");
static const u8 sText_RunLost[] = _("{COLOR RED}THE JOURNEY ENDS HERE");
static const u8 sText_BirchSays[] = _("{COLOR BLUE}PROF. BIRCH");
static const u8 sText_Seed[] = _("Region: ");
static const u8 sText_RunCode[] = _("Run code: ");
static const u8 sText_Badges[] = _("   Badges: ");
static const u8 sText_Champion[] = _("CHAMPION beaten!");
static const u8 sText_EndedAt[] = _("Ended at ");
static const u8 sText_EndedChampion[] = _("Reached the CHAMPION!");
static const u8 sText_EndedElite[] = _("ELITE FOUR beaten: ");
static const u8 sText_OfFour[] = _(" of 4");
static const u8 sText_Lv[] = _(" Lv");
static const u8 sText_Fallen[] = _("POKéMON lost: ");
static const u8 sText_Time[] = _("   Time: ");
static const u8 sText_Dungeons[] = _("Dungeon bosses beaten: ");
static const u8 sText_BossesOf[] = _(" of ");
static const u8 sText_Colon[] = _(":");
static const u8 sText_Record[] = _("Runs: ");
static const u8 sText_Runs[] = _("   Wins: ");
static const u8 sText_NewUnlocks[] = _("{COLOR BLUE}NEW UNLOCKS");
static const u8 sText_NextUnlocks[] = _("{COLOR BLUE}COMING UP");
static const u8 sText_NextUnlocksMenu[] = _("Turn them on or off in UNLOCKS.");
static const u8 sText_FeatsMenu[] = _("All FEATS and legends are in UNLOCKS.");
static const u8 sText_RewardShort[] = _("Reward: ");
static const u8 sText_PressANext[] = _("Press A to go on.");
static const u8 sText_PressA[] = _("Press A to return to the title.");

// ---------------------------------------------------------------------------
// Shared screen setup

static void VBlankCB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void CB2_RogueScreenMain(void)
{
    RunTasks();
    RunTextPrinters();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

static u16 *Tilemap(u32 bg)
{
    return (u16 *)BG_SCREEN_ADDR(bg == BG_TEXT ? 31 : (bg == BG_MAP ? 26 : 24));
}

static void PutTile(u32 bg, s32 x, s32 y, u16 tile)
{
    if (x >= 0 && y >= 0 && x < 32 && y < (bg == BG_TEXT ? 32 : MAP_ROWS))
        Tilemap(bg)[y * 32 + x] = tile;
}

static void InitScreen(const struct BgTemplate *bgs, u32 nBgs, const struct WindowTemplate *windows)
{
    u32 i;

    SetVBlankCallback(NULL);
    SetHBlankCallback(NULL);
    ScanlineEffect_Stop();
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0);
    DmaFill16(3, 0, (void *)VRAM, VRAM_SIZE);
    DmaFill32(3, 0, (void *)OAM, OAM_SIZE);
    DmaFill16(3, 0, (void *)PLTT, PLTT_SIZE);
    ResetPaletteFade();
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    ResetBgsAndClearDma3BusyFlags(0);
    InitBgsFromTemplates(0, bgs, nBgs);
    for (i = 0; i < 4; i++)
    {
        ChangeBgX(i, 0, BG_COORD_SET);
        ChangeBgY(i, 0, BG_COORD_SET);
    }
    InitWindows(windows);
    DeactivateAllTextPrinters();
    LoadWindowGfx(0, 0, 2, BG_PLTT_ID(14));
    LoadPalette(gStandardMenuPalette, BG_PLTT_ID(15), PLTT_SIZE_4BPP);
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0 | DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    for (i = 0; i < nBgs; i++)
        ShowBg(bgs[i].bg);
}

// ---------------------------------------------------------------------------
// Region map

static void LoadMapSprites(void)
{
    struct SpriteSheet sheet;
    struct CompressedSpriteSheet packed;
    struct SpritePalette pal;
    bool32 female = gSaveBlock2Ptr->playerGender != MALE;

    sheet.data = sCursorGfx;
    sheet.size = sizeof(sCursorGfx);
    sheet.tag = TAG_CURSOR;
    LoadSpriteSheet(&sheet);
    pal.data = sCursorPalette;
    pal.tag = TAG_CURSOR;
    LoadSpritePalette(&pal);

    sheet.data = female ? sMayIconGfx : sBrendanIconGfx;
    sheet.size = sizeof(sBrendanIconGfx);
    sheet.tag = TAG_PLAYER;
    LoadSpriteSheet(&sheet);
    pal.data = female ? sMayIconPalette : sBrendanIconPalette;
    pal.tag = TAG_PLAYER;
    LoadSpritePalette(&pal);

    packed.data = sMarksGfx;
    packed.size = 14 * TILE_SIZE_4BPP;
    packed.tag = TAG_MARKS;
    LoadCompressedSpriteSheet(&packed);
    pal.data = sMarksPalette;
    pal.tag = TAG_MARKS;
    LoadSpritePalette(&pal);
}

// A place's tile on screen, and whether it is on the screen at all (only
// a region planned taller than the screen leaves some off it).
static s32 PlaceRow(u32 place)
{
    return sMap.tileY[place] + sMap.rowOffset;
}

static bool32 PlaceShown(u32 place)
{
    s32 row = PlaceRow(place);
    return row >= MAP_TOP && row < MAP_INFO_TOP && sMap.tileX[place] >= 0 && sMap.tileX[place] < 30;
}

// The middle of a place in pixels. A city drawn two tiles wide or tall
// (RogueGen_DrawRegionMap's capsules) has its place on the first half.
// (v1.1 towns2) A crater town's mark is in the middle of its crater's four
// tiles, the place on the top left one.
static s32 PlaceCentreX(u32 place)
{
    u16 tile = sMap.tiles[sMap.tileY[place] * ROGUE_RMAP_W + sMap.tileX[place]] & 0x3FF;
    return sMap.tileX[place] * 8 + ((tile == RMAP_TILE_CITY_W || RMAP_IS_CRATER_TOWN(tile)) ? 8 : 4);
}

static s32 PlaceCentreY(u32 place)
{
    u16 tile = sMap.tiles[sMap.tileY[place] * ROGUE_RMAP_W + sMap.tileX[place]] & 0x3FF;
    return PlaceRow(place) * 8 + ((tile == RMAP_TILE_CITY_N || RMAP_IS_CRATER_TOWN(tile)) ? 8 : 4);
}

// Off the screen, a sprite waits above it.
static void PlaceSprite(struct Sprite *sprite, u32 place)
{
    sprite->x = PlaceCentreX(place);
    sprite->y = PlaceShown(place) ? PlaceCentreY(place) : -32;
}

// The gyms not yet won that the player can reach with the badges won, a
// bit each: within a chapter they can be won in any order.
static u32 OpenGyms(void)
{
    const struct RogueRegion *region = Rogue_GetRegion();
    u8 seen[ROGUE_MAX_PLACES];
    u32 g, won = Rogue_WonGyms(), open = 0;
    RogueGen_ReachWith(region, RogueGen_KeysWon(region, won) | Rogue_VillainKeys(), seen);
    for (g = 0; g < ROGUE_NUM_GYMS; g++)
        if (!((won >> g) & 1) && seen[region->gymTown[g]])
            open |= 1 << g;
    return open;
}

// TRUE once all eight badges are won and the Champion is still to beat.
static bool32 LeagueNext(void)
{
    return VarGet(VAR_ROGUE_PROGRESS) < ROGUE_PROGRESS_CHAMPION && Rogue_BadgeCount() >= ROGUE_NUM_GYMS;
}

static bool32 IsGymWon(u32 gym)
{
    return (Rogue_WonGyms() >> gym) & 1;
}

// A town you can fly to: a pink dot that blinks while selected, as on
// Emerald's Fly map.
static void SpriteCB_FlyMark(struct Sprite *sprite)
{
    if (sMap.selected == sprite->data[0])
    {
        if (++sprite->data[1] > 16)
        {
            sprite->data[1] = 0;
            sprite->invisible ^= TRUE;
        }
    }
    else
    {
        sprite->data[1] = 16;
        sprite->invisible = FALSE;
    }
}

// An open gym's red outline blinks all the time.
static void SpriteCB_NextGym(struct Sprite *sprite)
{
    if (++sprite->data[1] >= 24)
    {
        sprite->data[1] = 0;
        sprite->invisible ^= TRUE;
    }
}

// The cursor glides to the selected place.
static void SpriteCB_Cursor(struct Sprite *sprite)
{
    s32 dx = sprite->data[0] - sprite->x, dy = sprite->data[1] - sprite->y;
    sprite->x += (dx > 1 || dx < -1) ? dx / 2 : dx;
    sprite->y += (dy > 1 || dy < -1) ? dy / 2 : dy;
}

// Marks on the towns: a blinking red outline on each gym the player can
// take on now (or the League) and, once you can fly, Emerald's Fly map
// icons over every town,
// pink where you can fly and gray where you have not been. Before then the
// map marks no visits, as Emerald's town map doesn't; the strip tells.
static void CreateMarks(void)
{
    const struct RogueRegion *region = Rogue_GetRegion();
    u32 open = OpenGyms(), i;
    bool32 league = LeagueNext();
    u8 id;

    for (i = 0; i < region->nPlaces; i++)
    {
        const struct RoguePlace *p = &region->places[i];
        if (p->kind != RPLACE_TOWN)
            continue;
        if ((p->gym < ROGUE_NUM_GYMS && ((open >> p->gym) & 1)) || (league && p->type == RTOWN_SUMMIT))
        {
            id = CreateSprite(&sOutlineTemplate, 0, 0, 3);
            if (id != MAX_SPRITES && sMap.nMarks < ARRAY_COUNT(sMap.marks))
            {
                sMap.marks[sMap.nMarks++] = id;
                gSprites[id].data[0] = i;
                PlaceSprite(&gSprites[id], i);
                StartSpriteAnim(&gSprites[id], MARK_NEXT);
            }
        }
        if (Rogue_BadgeCount() >= 6)
        {
            u16 tile = sMap.tiles[sMap.tileY[i] * ROGUE_RMAP_W + sMap.tileX[i]] & 0x3FF;
            u32 mark = tile == RMAP_TILE_CITY_W ? MARK_FLY_W : tile == RMAP_TILE_CITY_N ? MARK_FLY_N : MARK_FLY;
            id = CreateSprite(&sDotTemplate, 0, 0, 4);
            if (id != MAX_SPRITES && sMap.nMarks < ARRAY_COUNT(sMap.marks))
            {
                sMap.marks[sMap.nMarks++] = id;
                gSprites[id].data[0] = i;
                PlaceSprite(&gSprites[id], i);
                if (mark == MARK_FLY_W)
                    gSprites[id].oam.shape = SPRITE_SHAPE(16x8);
                else if (mark == MARK_FLY_N)
                    gSprites[id].oam.shape = SPRITE_SHAPE(8x16);
                if (Rogue_CanFlyTo(i))
                    gSprites[id].callback = SpriteCB_FlyMark;
                else
                    mark += MARK_NO_FLY;
                StartSpriteAnim(&gSprites[id], mark);
            }
        }
    }
}

// The MARKING MAP: each legend not yet beaten or caught shows where it
// sleeps, as Platinum's Pokétch app shows roaming legends: its icon, above
// the place its dungeon is entered from.
static void CreateLegendMarks(void)
{
    const struct RogueRegion *region = Rogue_GetRegion();
    u32 i;

    if (!Rogue_IsUnlockOn(UNLOCK_MARKING_MAP))
        return;
    LoadMonIconPalettes();
    for (i = 0; i < region->nDungeons; i++)
    {
        const struct RogueDungeon *d = &region->dungeons[i];
        u8 id;
        if (d->boss != RBOSS_LEGEND || Rogue_IsDungeonCleared(i) || sMap.nMarks >= ARRAY_COUNT(sMap.marks))
            continue;
        id = CreateMonIcon(Rogue_LegendSpecies(d->legend), SpriteCB_MonIcon, 0, 0, 2, 0);
        if (id == MAX_SPRITES)
            continue;
        sMap.marks[sMap.nMarks++] = id;
        gSprites[id].data[0] = d->host;
        gSprites[id].y2 = -10;
        PlaceSprite(&gSprites[id], d->host);
    }
}

// Where a dungeon's mark goes: on a cave's own tile (the cave is the
// dungeon), else on the bottom right corner of the place's dot or capsule,
// as FRLG nudges a dungeon icon off a town's; a second mark on one place
// goes on the bottom left corner. data[0] is the place, data[1] which mark
// on it.
static void PlaceDungeonMark(struct Sprite *sprite)
{
    u32 place = sprite->data[0];
    u16 tile = sMap.tiles[sMap.tileY[place] * ROGUE_RMAP_W + sMap.tileX[place]] & 0x3FF;
    if (Rogue_GetRegion()->places[place].kind == RPLACE_CAVE && sprite->data[1] == 0)
    {
        sprite->x = sMap.tileX[place] * 8 + 4;
        sprite->y = PlaceRow(place) * 8 + 4;
    }
    else
    {
        sprite->x = sMap.tileX[place] * 8 + (sprite->data[1] ? 0 : (tile == RMAP_TILE_CITY_W || RMAP_IS_CRATER_TOWN(tile)) ? 16 : 8);
        sprite->y = PlaceRow(place) * 8 + ((tile == RMAP_TILE_CITY_N || RMAP_IS_CRATER_TOWN(tile)) ? 16 : 8);
    }
    if (!PlaceShown(place))
        sprite->y = -32;
}

static void SpriteCB_DungeonMark(struct Sprite *sprite)
{
}

// A mark on a place: the small gray one until found, then its style's.
static void CreateDungeonMark(u32 place, u32 style, bool32 found, u8 *marks)
{
    struct SpritePalette pal;
    u8 id = CreateSprite(&sDungeonMarkTemplate, 0, 0, 2);

    if (id == MAX_SPRITES || place >= ROGUE_MAX_PLACES)
        return;
    if (style >= DMARK_COUNT)
        style = DMARK_GENERIC;
    gSprites[id].data[0] = place;
    gSprites[id].data[1] = marks[place]++;
    if (found)
    {
        // Three styles share a palette (make_dungeon_marks.py).
        pal.data = sDungeonMarksPalette + 16 * (style / 3);
        pal.tag = TAG_DMARKS + style / 3;
        LoadSpritePalette(&pal);
        gSprites[id].oam.paletteNum = IndexOfSpritePaletteTag(pal.tag);
        StartSpriteAnim(&gSprites[id], 1 + style);
    }
    PlaceDungeonMark(&gSprites[id]);
}

// Every dungeon's mark, by its kind (sDungeonMarks), framed once its way in
// has been visited; in the professor's preview all are still to find. Then
// the marks others add (Rogue_ExtraMapMarks), such as a tunnel's far mouth.
static void CreateDungeonMarks(void)
{
    const struct RogueRegion *region = Rogue_GetRegion();
    struct SpriteSheet sheet = { sDungeonMarksGfx, sizeof(sDungeonMarksGfx), TAG_DMARKS };
    struct SpritePalette pal = { sDungeonMarksPalette, TAG_DMARKS };
    u8 marks[ROGUE_MAX_PLACES], places[8], styles[8];
    u32 i, n;

    LoadSpriteSheet(&sheet);
    LoadSpritePalette(&pal);
    memset(marks, 0, sizeof(marks));
    for (i = 0; i < region->nDungeons; i++)
    {
        const struct RogueDungeon *d = &region->dungeons[i];
        u32 style = d->kind < ARRAY_COUNT(sDungeonMarks) ? sDungeonMarks[d->kind] : DMARK_GENERIC;
        // The tunnel's near mouth is marked as the tunnel, as its far one.
        if (i == region->tunnel)
            style = DMARK_TUNNEL;
        CreateDungeonMark(d->host, style, !sMap.preview && Rogue_IsPlaceVisited(d->host), marks);
    }
    if (sMap.preview)
        return;
    n = Rogue_ExtraMapMarks(places, styles, ARRAY_COUNT(places));
    for (i = 0; i < n && i < ARRAY_COUNT(places); i++)
        CreateDungeonMark(places[i], styles[i] & ~DMARK_NOT_FOUND, !(styles[i] & DMARK_NOT_FOUND), marks);
}

// Shows the map's rows from top on. Only a region planned taller than the
// screen has rows to choose from; it moves to keep the selection in view.
static void ShowMapRows(s32 top)
{
    u16 *screen = Tilemap(BG_MAP);
    s32 x, y;
    u32 i;

    if (sMap.preview)
    {
        // No bars: the region from the top of the screen, above the
        // professor's box.
        top = 0;
        sMap.rowOffset = 0;
    }
    else if (sMap.height <= MAP_VIEW_ROWS)
    {
        // The whole region, centred between the bars.
        top = 0;
        sMap.rowOffset = MAP_TOP + (MAP_VIEW_ROWS - sMap.height) / 2;
    }
    else
    {
        if (top > sMap.height - MAP_VIEW_ROWS)
            top = sMap.height - MAP_VIEW_ROWS;
        if (top < 0)
            top = 0;
        sMap.rowOffset = MAP_TOP - top;
    }
    for (y = sMap.preview ? 0 : MAP_TOP; y < (sMap.preview ? MAP_VIEW_ROWS : MAP_INFO_TOP); y++)
    {
        s32 row = y - sMap.rowOffset;
        for (x = 0; x < 32; x++)
            screen[y * 32 + x] = (row >= 0 && row < sMap.height && x < ROGUE_RMAP_W) ? sMap.tiles[row * ROGUE_RMAP_W + x] : 0;
    }
    for (i = 0; i < sMap.nMarks; i++)
        PlaceSprite(&gSprites[sMap.marks[i]], gSprites[sMap.marks[i]].data[0]);
    for (i = 0; i < MAX_SPRITES; i++)
        if (gSprites[i].inUse && gSprites[i].callback == SpriteCB_DungeonMark)
            PlaceDungeonMark(&gSprites[i]);
}

// The map's tiles (Hoenn's, then the crater's) and colors, the same for the
// map screen and the professor's preview.
static void LoadMapGraphics(void)
{
    DecompressDataWithHeaderVram(sRegionMapTiles, (void *)BG_CHAR_ADDR(TILE_CHAR_BASE));
    CpuCopy16(sRegionMapCraterGfx, (void *)(BG_CHAR_ADDR(TILE_CHAR_BASE) + RMAP_TILE_CRATER * TILE_SIZE_8BPP), sizeof(sRegionMapCraterGfx));
    CpuCopy16(sRegionMapCraterTownGfx, (void *)(BG_CHAR_ADDR(TILE_CHAR_BASE) + RMAP_TILE_CRATER_TOWN * TILE_SIZE_8BPP), sizeof(sRegionMapCraterTownGfx));
    LoadPalette(sRegionMapPalette, BG_PLTT_ID(7), 2 * PLTT_SIZE_4BPP);
    LoadPalette(sMapBarPalette, BG_PLTT_ID(MAP_BAR_PAL), PLTT_SIZE_4BPP);
}

static void DrawMap(void)
{
    const struct RogueRegion *region = Rogue_GetRegion();
    u8 *work = Alloc(ROGUE_RMAP_WORK);

    sMap.tiles = Alloc(ROGUE_RMAP_W * ROGUE_RMAP_H * sizeof(u16));
    sMap.height = RogueGen_DrawRegionMap(region, sMap.tiles, sMap.tileX, sMap.tileY, work, 2);
    Free(work);
    sMap.nMarks = 0;
    LoadMapSprites();
    // The preview shows where the run starts and the dungeons to find.
    if (!sMap.preview)
    {
        CreateMarks();
        CreateLegendMarks();
    }
    CreateDungeonMarks();
    ShowMapRows(sMap.tileY[sMap.current] - MAP_VIEW_ROWS / 2);
    sMap.playerSprite = CreateSprite(&sPlayerTemplate, 0, 0, 1);
    sMap.cursorSprite = CreateSprite(&sCursorTemplate, 0, 0, 0);
    if (sMap.preview)
        gSprites[sMap.cursorSprite].invisible = TRUE;
    PlaceSprite(&gSprites[sMap.playerSprite], sMap.current);
    PlaceSprite(&gSprites[sMap.cursorSprite], sMap.selected);
}

// The cursor's tile. Places sit on every other tile, all with the same
// parity, so the cursor's cell gives its tile.
static s32 CursorTileX(void)
{
    return (sMap.cursorCell % CELL_ROW) * 2 + (sMap.tileX[0] & 1);
}

static s32 CursorTileY(void)
{
    return (sMap.cursorCell / CELL_ROW) * 2 + (sMap.tileY[0] & 1);
}

static void SelectPlace(u32 place)
{
    sMap.selected = place;
    sMap.cursorCell = CELL_ROW * (sMap.tileY[place] / 2) + sMap.tileX[place] / 2;
}

static void DrawCursor(void)
{
    struct Sprite *cursor = &gSprites[sMap.cursorSprite];
    s32 y = CursorTileY();
    if (y + sMap.rowOffset < MAP_TOP || y + sMap.rowOffset >= MAP_INFO_TOP)
    {
        ShowMapRows(y - MAP_VIEW_ROWS / 2);
        PlaceSprite(&gSprites[sMap.playerSprite], sMap.current);
        cursor->x = CursorTileX() * 8 + 4;
        cursor->y = (y + sMap.rowOffset) * 8 + 4;
    }
    if (sMap.selected != NO_PLACE)
    {
        cursor->data[0] = PlaceCentreX(sMap.selected);
        cursor->data[1] = PlaceCentreY(sMap.selected);
    }
    else
    {
        cursor->data[0] = CursorTileX() * 8 + 4;
        cursor->data[1] = (y + sMap.rowOffset) * 8 + 4;
    }
}

// What the strip says about a town. Returns the end.
static u8 *BuildTownInfo(u8 *str, const struct RoguePlace *p)
{
    const struct RogueRegion *region = Rogue_GetRegion();

    if (p->type == RTOWN_SUMMIT)
        return StringCopy(str, VarGet(VAR_ROGUE_PROGRESS) >= ROGUE_PROGRESS_CHAMPION ? sText_LeagueWon : LeagueNext() ? sText_LeagueNext : sText_League);
    if (p->type == RTOWN_START)
        return StringCopy(str, sText_NoGym);
    if (p->gym >= ROGUE_NUM_GYMS)
        return StringCopy(str, p->type == RTOWN_HAMLET ? sText_Hamlet : sText_Village);
    if (IsGymWon(p->gym))
    {
        // The badge, and the field move it gives.
        str = StringCopy(str, Rogue_GetBadgeName(p->gym));
        if (region->gymKey[p->gym] && region->gymKey[p->gym] < ARRAY_COUNT(sKeyNames))
        {
            str = StringCopy(str, sText_WonHereKey);
            return StringCopy(str, sKeyNames[region->gymKey[p->gym]]);
        }
        return StringCopy(str, sText_WonHere);
    }
    return StringCopy(str, (OpenGyms() >> p->gym) & 1 ? sText_NextGym : sText_GymTown);
}

// What the strip says about a place. Detail 1 leaves out the kind of route,
// 2 also what waits in a dungeon.
static void BuildPlaceInfo(u8 *str, u32 place, u32 detail)
{
    const struct RogueRegion *region = Rogue_GetRegion();
    const struct RoguePlace *p = &region->places[place];

    *str = EOS;
    if (p->kind != RPLACE_TOWN)
    {
        bool32 visited = Rogue_IsPlaceVisited(place);
        if (visited && detail == 0)
        {
            if (p->kind == RPLACE_CAVE)
                str = StringCopy(str, p->dark ? sText_DarkCave : sText_Cave);
            else
                str = StringCopy(str, sRouteKinds[p->type < RROUTE_COUNT && sRouteKinds[p->type] ? p->type : 0]);
        }
        if (p->obstacle && p->obstacle < ARRAY_COUNT(sObstacleNames))
            str = StringCopy(str, sObstacleNames[p->obstacle]);
        str = Rogue_AppendDungeonNote(str, place, detail >= 2);
        if (!visited)
            StringCopy(str, sText_NotExplored);
        else if (p->area == 0xFF)
            ;
        else if (Rogue_IsAreaCaught(p->area))
            StringCopy(str, sText_Caught);
        else if (Rogue_IsAreaCatchUsed(p->area))
            StringCopy(str, sText_EncounterUsed);
        else
            StringCopy(str, sText_CatchOpen);
    }
    else
    {
        // A town's dungeon (a tower, a hideout) after what the town is.
        str = BuildTownInfo(str, p);
        Rogue_AppendDungeonNote(StringCopy(str, COMPOUND_STRING(" ")), place, detail >= 2);
    }
}

#define INFO_WIDTH (30 * 8 - 8)
// A strip too long for one line shows its sentences a page at a time,
// turning every few seconds (text wider than the window would wrap round
// onto its start).
#define STRIP_PAGE_FRAMES 180

// Where the sentence starting at s ends: after its ". " or "! ", or at the
// end. Not after a name's "MT." (MT. CHIMNEY), which the next word belongs to.
static const u8 *SentenceEnd(const u8 *s)
{
    const u8 *start = s;
    for (; *s != EOS; s++)
    {
        if ((*s == CHAR_PERIOD || *s == CHAR_EXCL_MARK) && s[1] == CHAR_SPACE
         && !(*s == CHAR_PERIOD && s - start >= 2 && s[-2] == CHAR_M && s[-1] == CHAR_T
              && (s - start == 2 || s[-3] == CHAR_SPACE)))
            return s + 2;
    }
    return s;
}

// Copies [start, end) to dst without trailing spaces.
static void CopySpan(u8 *dst, const u8 *start, const u8 *end)
{
    while (end > start && end[-1] == CHAR_SPACE)
        end--;
    while (start < end)
        *dst++ = *start++;
    *dst = EOS;
}

// Cuts str at a word so that it fits width, ending it with "…" (only for
// a single sentence longer than a whole line).
static void CutToWidth(u8 *str, u32 font, u32 width)
{
    while (GetStringWidth(font, str, 0) > width)
    {
        u8 *cut = str + StringLength(str);
        while (cut > str && *cut != CHAR_SPACE)
            cut--;
        if (cut == str)
            return;
        StringCopy(cut, COMPOUND_STRING("…"));
    }
}

// gStringVar4 holds a line too long for the strip: leaves page `page` of it
// there and returns how many pages it has. Pages are whole sentences,
// packed from the end, so a dungeon's note stays with its legend. The bar
// is drawn by now, so gStringVar2 and 3 are free.
#define MAX_STRIP_SENTENCES 16
static u32 TakeStripPage(u32 page)
{
    u8 starts[MAX_STRIP_SENTENCES + 1]; // where each sentence starts, then the end
    u8 pageStarts[MAX_STRIP_SENTENCES]; // the sentence each page starts with, last page first
    const u8 *s;
    u32 n = 0, pages = 0, first, end;

    StringCopy(gStringVar3, gStringVar4);
    for (s = gStringVar3; *s != EOS && n < MAX_STRIP_SENTENCES; s = SentenceEnd(s))
        starts[n++] = s - gStringVar3;
    starts[n] = s - gStringVar3;
    for (end = n; end > 0; end = first)
    {
        for (first = end - 1; first > 0; first--)
        {
            CopySpan(gStringVar2, gStringVar3 + starts[first - 1], gStringVar3 + starts[end]);
            if (GetStringWidth(FONT_NARROW, gStringVar2, 0) > INFO_WIDTH)
                break;
        }
        pageStarts[pages++] = first;
    }
    // Two pages (the usual case) split where they come out most even, so a
    // short first sentence does not stand alone.
    if (pages == 2)
    {
        u32 split, best = 0xFFFF, width;
        for (split = 1; split < n; split++)
        {
            CopySpan(gStringVar2, gStringVar3, gStringVar3 + starts[split]);
            width = GetStringWidth(FONT_NARROW, gStringVar2, 0);
            CopySpan(gStringVar2, gStringVar3 + starts[split], gStringVar3 + starts[n]);
            width = max(width, GetStringWidth(FONT_NARROW, gStringVar2, 0));
            if (width <= INFO_WIDTH && width < best)
            {
                best = width;
                pageStarts[0] = split;
            }
        }
    }
    if (page >= pages)
        page = 0;
    first = pageStarts[pages - 1 - page];
    end = page + 1 < pages ? pageStarts[pages - 2 - page] : n;
    CopySpan(gStringVar4, gStringVar3 + starts[first], gStringVar3 + starts[end]);
    CutToWidth(gStringVar4, FONT_NARROW, INFO_WIDTH);
    return pages;
}

// TRUE with the cursor on the volcano's crater, which the bars name, as
// Emerald's map names Mt. Chimney.
static bool32 CursorOnVolcano(void)
{
    const struct RogueRegion *region = Rogue_GetRegion();
    s8 gx, gy;
    if (sMap.selected != NO_PLACE || !RogueGen_VolcanoCell(region, &gx, &gy))
        return FALSE;
    return CursorTileX() == sMap.tileX[0] + 2 * (gx - region->places[0].gridX)
        && CursorTileY() == sMap.tileY[0] + 2 * (gy - region->places[0].gridY);
}

static bool32 FlyFromParty(void)
{
    return sMap.exitCallback == CB2_ReturnToPartyMenuFromFlyMap;
}

// The strip, or a page of it. Returns how many pages it has.
static u32 PrintMapStrip(u32 page)
{
    u32 detail, font = FONT_NORMAL, pages = 1;

    // One line, in the narrow font or with less said when it is long; else
    // all of it, a page at a time.
    for (detail = 0; detail < 3 && sMap.selected != NO_PLACE; detail++)
    {
        BuildPlaceInfo(gStringVar4, sMap.selected, detail);
        font = FONT_NORMAL;
        if (GetStringWidth(FONT_NORMAL, gStringVar4, 0) <= INFO_WIDTH)
            break;
        font = FONT_NARROW;
        if (GetStringWidth(FONT_NARROW, gStringVar4, 0) <= INFO_WIDTH)
            break;
    }
    if (sMap.selected == NO_PLACE)
    {
        gStringVar4[0] = EOS;
        if (CursorOnVolcano())
        {
            // With where it is entered: narrow, or a page at a time.
            Rogue_BufferCraterNote(gStringVar4);
            if (GetStringWidth(FONT_NORMAL, gStringVar4, 0) > INFO_WIDTH)
                font = FONT_NARROW;
            if (GetStringWidth(FONT_NARROW, gStringVar4, 0) > INFO_WIDTH)
                pages = TakeStripPage(page);
        }
    }
    else if (detail == 3)
    {
        BuildPlaceInfo(gStringVar4, sMap.selected, 0);
        pages = TakeStripPage(page);
    }
    FillWindowPixelBuffer(MWIN_INFO, PIXEL_FILL(BAR_DARK));
    FillWindowPixelRect(MWIN_INFO, PIXEL_FILL(BAR_LINE), 0, 0, 30 * 8, 1);
    AddTextPrinterParameterized3(MWIN_INFO, font, 4, 5, sBarText, TEXT_SKIP_DRAW, gStringVar4);
    PutWindowTilemap(MWIN_INFO);
    CopyWindowToVram(MWIN_INFO, COPYWIN_FULL);
    return pages;
}

static u32 PrintMapInfo(void)
{
    u32 x;
    u8 *name = gStringVar1;

    // The bar: the place's name, HERE or A: FLY, and the region's name.
    FillWindowPixelBuffer(MWIN_BAR, PIXEL_FILL(BAR_DARK));
    FillWindowPixelRect(MWIN_BAR, PIXEL_FILL(BAR_LINE), 0, MAP_TOP * 8 - 1, 30 * 8, 1);
    // Nothing on a cell with no place, as on Emerald's maps.
    name[0] = EOS;
    if (sMap.selected != NO_PLACE)
        Rogue_CopyPlaceName(name, sMap.selected);
    else if (CursorOnVolcano())
        Rogue_CopyVolcanoName(name);
    AddTextPrinterParameterized3(MWIN_BAR, FONT_NORMAL, 4, 0, sBarText, TEXT_SKIP_DRAW, name);
    x = 4 + GetStringWidth(FONT_NORMAL, name, 0) + 8;
    if (sMap.selected == NO_PLACE)
        ;
    else if (sMap.selected == sMap.current)
        AddTextPrinterParameterized3(MWIN_BAR, FONT_NORMAL, x, 0, sBarHere, TEXT_SKIP_DRAW, sText_YouAreHere);
    else if (Rogue_CanFlyTo(sMap.selected))
        AddTextPrinterParameterized3(MWIN_BAR, FONT_NORMAL, x, 0, sBarRegion, TEXT_SKIP_DRAW, sText_FlyHere);
    // FLY from the party menu asks where to, as Emerald's Fly map does.
    if (FlyFromParty())
        StringCopy(gStringVar2, gText_FlyToWhere);
    else
        Rogue_RegionName(gStringVar2, Rogue_GetSeed());
    AddTextPrinterParameterized3(MWIN_BAR, FONT_NORMAL, 30 * 8 - 4 - GetStringWidth(FONT_NORMAL, gStringVar2, 0), 0,
                                 sBarRegion, TEXT_SKIP_DRAW, gStringVar2);
    PutWindowTilemap(MWIN_BAR);
    CopyWindowToVram(MWIN_BAR, COPYWIN_FULL);
    return PrintMapStrip(0);
}

// The cursor moves one cell (two tiles) the way pressed, over places and
// the land between them, as Emerald's Fly map cursor moves a square at a
// time; it stops at the edge of the places.
static bool32 TryMoveSelection(s32 dx, s32 dy)
{
    const struct RogueRegion *region = Rogue_GetRegion();
    s32 x = CursorTileX() + 2 * dx, y = CursorTileY() + 2 * dy;
    s32 i, minX = 127, maxX = -1, minY = 127, maxY = -1;

    for (i = 0; i < region->nPlaces; i++)
    {
        minX = min(minX, sMap.tileX[i]);
        maxX = max(maxX, sMap.tileX[i]);
        minY = min(minY, sMap.tileY[i]);
        maxY = max(maxY, sMap.tileY[i]);
    }
    if (x < minX || x > maxX || y < minY || y > maxY)
        return FALSE;
    sMap.cursorCell = CELL_ROW * (y / 2) + x / 2;
    sMap.selected = NO_PLACE;
    for (i = 0; i < region->nPlaces; i++)
        if (sMap.tileX[i] == x && sMap.tileY[i] == y)
            sMap.selected = i;
    return TRUE;
}

// L and R step through the towns.
static bool32 TryMoveToTown(s32 step)
{
    const struct RogueRegion *region = Rogue_GetRegion();
    s32 i, place = sMap.selected == NO_PLACE ? sMap.current : sMap.selected;
    for (i = 0; i < region->nPlaces; i++)
    {
        place = (place + step + region->nPlaces) % region->nPlaces;
        if (region->places[place].kind == RPLACE_TOWN)
        {
            SelectPlace(place);
            return TRUE;
        }
    }
    return FALSE;
}

// Who flies the player: the first Flying type in the party, or the lead.
// The fly banner shows the party menu's selected Pokémon, which is stale
// when the map was opened from START.
static u8 PickFlyMon(void)
{
    u32 i, n = CalculatePlayerPartyCount();
    for (i = 0; i < n; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        u32 species = GetMonData(mon, MON_DATA_SPECIES);
        if (GetMonData(mon, MON_DATA_IS_EGG) || species == SPECIES_NONE)
            continue;
        if (gSpeciesInfo[species].types[0] == TYPE_FLYING || gSpeciesInfo[species].types[1] == TYPE_FLYING)
            return i;
    }
    return 0;
}

static void Task_MapExit(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        gKeyRepeatStartDelay = 40;
        TRY_FREE_AND_SET_NULL(sMap.tiles);
        FreeMonIconPalettes();
        FreeAllWindowBuffers();
        DestroyTask(taskId);
        if (sMap.flying)
        {
            if (!FlyFromParty())
                gPartyMenu.slotId = PickFlyMon();
            Rogue_SetFlyWarp(sMap.selected);
            ReturnToFieldFromFlyMapSelect();
        }
        else
        {
            SetMainCallback2(sMap.exitCallback);
        }
    }
}

// Task data: the strip's page, its number of pages, and the frames it has shown.
#define tStripPage   data[0]
#define tStripPages  data[1]
#define tStripFrames data[2]

static void Task_MapInput(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    bool32 moved = FALSE;

    if (gPaletteFade.active)
        return;
    if (tStripPages > 1 && ++tStripFrames >= STRIP_PAGE_FRAMES)
    {
        tStripFrames = 0;
        tStripPage = (tStripPage + 1) % tStripPages;
        PrintMapStrip(tStripPage);
    }
    if (JOY_REPEAT(DPAD_UP))
        moved = TryMoveSelection(0, -1);
    else if (JOY_REPEAT(DPAD_DOWN))
        moved = TryMoveSelection(0, 1);
    else if (JOY_REPEAT(DPAD_LEFT))
        moved = TryMoveSelection(-1, 0);
    else if (JOY_REPEAT(DPAD_RIGHT))
        moved = TryMoveSelection(1, 0);
    else if (JOY_REPEAT(L_BUTTON))
        moved = TryMoveToTown(-1);
    else if (JOY_REPEAT(R_BUTTON))
        moved = TryMoveToTown(1);
    else if (JOY_NEW(A_BUTTON) && sMap.selected != sMap.current && Rogue_CanFlyTo(sMap.selected))
    {
        PlaySE(SE_SELECT);
        sMap.flying = TRUE;
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_MapExit;
        return;
    }
    else if (JOY_NEW(B_BUTTON | START_BUTTON) || (JOY_NEW(A_BUTTON) && !FlyFromParty()))
    {
        PlaySE(SE_SELECT);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_MapExit;
        return;
    }
    // The cursor moves without a sound, as on Emerald's maps.
    if (moved)
    {
        DrawCursor();
        tStripPages = PrintMapInfo();
        tStripPage = 0;
        tStripFrames = 0;
    }
}

static void OpenMap(MainCallback exitCallback)
{
    s32 current = Rogue_GetCurrentPlace();
    u32 pages;

    InitScreen(sMapBgTemplates, ARRAY_COUNT(sMapBgTemplates), sMapWindows);
    LoadMapGraphics();
    sMap.exitCallback = exitCallback;
    sMap.flying = FALSE;
    sMap.preview = PREVIEW_NONE;
    sMap.current = current < 0 ? 0 : current;
    sMap.selected = sMap.current;
    DrawMap();
    SelectPlace(sMap.current);
    DrawCursor();
    pages = PrintMapInfo();
    gKeyRepeatStartDelay = 20;
    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
    SetVBlankCallback(VBlankCB);
    EnableInterrupts(INTR_FLAG_VBLANK);
    gTasks[CreateTask(Task_MapInput, 0)].tStripPages = pages;
    SetMainCallback2(CB2_RogueScreenMain);
}

// START > MAP.
void CB2_RogueMap(void)
{
    OpenMap(CB2_ReturnToFieldWithOpenMenu);
}

// FLY from the party menu, which B goes back to.
void CB2_RogueFlyMap(void)
{
    OpenMap(CB2_ReturnToPartyMenuFromFlyMap);
}

// ---------------------------------------------------------------------------
// The professor's region preview. In his speech, before a run, he shows the
// region he found on its map and asks if the player will explore it: YES
// starts the run, NO grows another region, TYPE NAME visits the region of a
// typed name (a region's name is its seed code). The region is grown before
// (Rogue_PrepareNewRegion) and the run starts with it (Rogue_NewGame).

// The whole region fills the top 15 rows, and the professor talks in a
// strip below it, in the colors of the map screen's bars. The question's
// answers sit on the strip's second line, so nothing covers the map.
#define PREVIEW_TOP MAP_VIEW_ROWS
enum { PWIN_MSG };
static const struct WindowTemplate sPreviewWindows[] =
{
    [PWIN_MSG] = { .bg = BG_TEXT, .tilemapLeft = 0, .tilemapTop = PREVIEW_TOP, .width = 30, .height = 20 - PREVIEW_TOP, .paletteNum = MAP_BAR_PAL, .baseBlock = 11 },
    DUMMY_WIN_TEMPLATE
};

enum { CHOICE_YES, CHOICE_NO, CHOICE_TYPE, NUM_CHOICES };
static const u8 *const sPreviewChoices[NUM_CHOICES] =
{
    [CHOICE_YES]  = COMPOUND_STRING("YES"),
    [CHOICE_NO]   = COMPOUND_STRING("NO"),
    [CHOICE_TYPE] = COMPOUND_STRING("TYPE NAME"),
};
static const u8 sPreviewChoiceX[NUM_CHOICES] = { 104, 144, 176 };
#define PREVIEW_TEXT_X 8
#define PREVIEW_LINE_1 4
#define PREVIEW_LINE_2 20

static const u8 sText_PreviewAsk[] = _("Will you explore it?");
static const u8 sText_PreviewDaily[] = _("Today is {STR_VAR_1}. Everyone who\nsets out today finds this region!\p");

static void OpenRegionPreview(void);
static void CB2_RogueRegionPreviewTyped(void);
static void CB2_RogueRegionPreviewAnother(void);

static void Task_PreviewExit(u8 taskId)
{
    s16 choice = gTasks[taskId].data[0];   // tChoice

    if (gPaletteFade.active)
        return;
    TRY_FREE_AND_SET_NULL(sMap.tiles);
    FreeAllWindowBuffers();
    DestroyTask(taskId);
    switch (choice)
    {
    case CHOICE_YES:
        sMap.preview = PREVIEW_NONE;
        SetMainCallback2(sMap.exitCallback);
        break;
    case CHOICE_NO:
        SetMainCallback2(CB2_RogueRegionPreviewAnother);
        break;
    default:
        gStringVar2[0] = EOS;
        DoNamingScreen(NAMING_SCREEN_SEED, gStringVar2, 0, 0, 0, CB2_RogueRegionPreviewTyped);
        break;
    }
}

// The answers on the strip's second line, the chosen one with the arrow.
static void PrintPreviewChoices(u32 cursor)
{
    u32 i;

    FillWindowPixelRect(PWIN_MSG, PIXEL_FILL(BAR_DARK), 96, PREVIEW_LINE_2, 30 * 8 - 96, 16);
    for (i = 0; i < NUM_CHOICES; i++)
    {
        if (i == cursor)
            AddTextPrinterParameterized3(PWIN_MSG, FONT_NORMAL, sPreviewChoiceX[i] - 8, PREVIEW_LINE_2, sBarHere, TEXT_SKIP_DRAW, gText_SelectorArrow);
        AddTextPrinterParameterized3(PWIN_MSG, FONT_NORMAL, sPreviewChoiceX[i], PREVIEW_LINE_2, i == cursor ? sBarHere : sBarText, TEXT_SKIP_DRAW, sPreviewChoices[i]);
    }
    CopyWindowToVram(PWIN_MSG, COPYWIN_GFX);
}

#define tChoice data[0]

// Left and right (or up and down) move along the answers, without wrapping.
// B is not NO: a player mashing B through the text keeps the region.
static void Task_PreviewMenu(u8 taskId)
{
    s16 cursor = gTasks[taskId].tChoice;

    if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_PreviewExit;
        return;
    }
    if (JOY_REPEAT(DPAD_LEFT | DPAD_UP) && cursor > 0)
        cursor--;
    else if (JOY_REPEAT(DPAD_RIGHT | DPAD_DOWN) && cursor < NUM_CHOICES - 1)
        cursor++;
    else
        return;
    PlaySE(SE_SELECT);
    gTasks[taskId].tChoice = cursor;
    PrintPreviewChoices(cursor);
}

static void Task_PreviewTalk(u8 taskId)
{
    if (gPaletteFade.active || IsTextPrinterActiveOnWindow(PWIN_MSG))
        return;
    gTasks[taskId].tChoice = CHOICE_YES;
    PrintPreviewChoices(CHOICE_YES);
    gTasks[taskId].func = Task_PreviewMenu;
}

#undef tChoice

static void OpenRegionPreview(void)
{
    const struct RogueRegion *region = Rogue_GetRegion();
    struct TextPrinterTemplate printer = {0};
    u8 *end;

    InitScreen(sMapBgTemplates, ARRAY_COUNT(sMapBgTemplates), sPreviewWindows);
    LoadMapGraphics();
    sMap.flying = FALSE;
    sMap.current = region->start;
    sMap.selected = region->start;
    DrawMap();
    SelectPlace(region->start);
    FillWindowPixelBuffer(PWIN_MSG, PIXEL_FILL(BAR_DARK));
    PutWindowTilemap(PWIN_MSG);
    CopyWindowToVram(PWIN_MSG, COPYWIN_FULL);
    // The DAILY REGION: he says the date first.
    end = gStringVar4;
    if (Rogue_IsDailyRegion())
    {
        Rogue_DailyDateText(gStringVar1);
        StringExpandPlaceholders(gStringVar4, sText_PreviewDaily);
        end = gStringVar4 + StringLength(gStringVar4);
    }
    // The first region of the first run gets his longest description.
    StringCopy(Rogue_DescribeRegion(end, sMap.preview == PREVIEW_FIRST ? 2 : TRUE), sText_PreviewAsk);
    printer.currentChar = gStringVar4;
    printer.type = WINDOW_TEXT_PRINTER;
    printer.windowId = PWIN_MSG;
    printer.fontId = FONT_NORMAL;
    printer.x = printer.currentX = PREVIEW_TEXT_X;
    printer.y = printer.currentY = PREVIEW_LINE_1;
    printer.color.foreground = BAR_WHITE;
    printer.color.background = printer.color.accent = BAR_DARK;
    printer.color.shadow = BAR_GRAY;
    gTextFlags.canABSpeedUpPrint = TRUE;
    gTextFlags.useAlternateDownArrow = FALSE;
    AddTextPrinter(&printer, GetPlayerTextSpeedDelay(), NULL);
    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
    SetVBlankCallback(VBlankCB);
    EnableInterrupts(INTR_FLAG_VBLANK);
    CreateTask(Task_PreviewTalk, 0);
    SetMainCallback2(CB2_RogueScreenMain);
}

static bool8 sKeptOffered; // RUN 0's CHOOSE: the region kept was shown this time

void Rogue_OpenRegionPreview(MainCallback exitCallback, bool32 firstRun)
{
    sKeptOffered = FALSE;
    sMap.exitCallback = exitCallback;
    sMap.preview = firstRun ? PREVIEW_FIRST : PREVIEW_AGAIN;
    OpenRegionPreview();
}

// NO: another region, grown while the screen is black.
static void CB2_RogueRegionPreviewAnother(void)
{
    u16 kept;
    // After RUN 0's CHOOSE, the first NO shows the region kept, which
    // carries the player's name.
    if (!sKeptOffered && Rogue_Run0KeptRegion(&kept) && kept != Rogue_GetSeed())
    {
        sKeptOffered = TRUE;
        Rogue_PrepareSeedRegion(kept);
        sMap.preview = PREVIEW_AGAIN;
        OpenRegionPreview();
        return;
    }
    Rogue_PrepareNewRegion();
    sMap.preview = PREVIEW_AGAIN;
    OpenRegionPreview();
}

// Back from the keyboard: the typed name's region, or the same one again
// when nothing was typed.
static void CB2_RogueRegionPreviewTyped(void)
{
    if (gStringVar2[0] != EOS)
        Rogue_PrepareTypedRegion(gStringVar2);
    sMap.preview = PREVIEW_AGAIN;
    OpenRegionPreview();
}

// ---------------------------------------------------------------------------
// Run summary

static u8 *AppendNumber(u8 *str, u32 value, u32 digits)
{
    return ConvertIntToDecimalStringN(str, value, STR_CONV_MODE_LEFT_ALIGN, digits);
}

// The runs counted for the starter rows (LadderUnlocks in rogue_run.c) are
// runs started, so the last one is earned as the next run starts, not as
// this one ends.
static const u8 sText_MoreRunStarted[] = _(" more run");
static const u8 sText_NextRunStarts[] = _("Yours as your next run starts!");

// Progress toward a locked unlock that counts something: how many there are
// and how many it needs, with the word for one more. FALSE if the unlock is
// earned or does not count anything.
static bool32 GetUnlockProgress(u32 unlock, u32 *have, u32 *need, const u8 **what)
{
    const struct RogueRecord *record = Rogue_GetRecord();
    u32 best = 0, type;

    for (type = 0; type < NUMBER_OF_MON_TYPES; type++)
        best = max(best, Rogue_GetTypeBadges(type));
    if (Rogue_IsUnlockEarned(unlock))
        return FALSE;
    switch (unlock)
    {
    case UNLOCK_EEVEE:
        *have = VarGet(VAR_ROGUE_META_RUNS), *need = ROGUE_EEVEE_RUNS, *what = sText_MoreRunStarted;
        break;
    case UNLOCK_BABIES:
        *have = VarGet(VAR_ROGUE_META_RUNS), *need = ROGUE_BABY_RUNS, *what = sText_MoreRunStarted;
        break;
    case UNLOCK_RISING_STARS:
        *have = VarGet(VAR_ROGUE_META_RUNS), *need = ROGUE_RISING_RUNS, *what = sText_MoreRunStarted;
        break;
    case UNLOCK_STARTER_CASE:
        *have = VarGet(VAR_ROGUE_META_RUNS), *need = ROGUE_STARTER_CASE_RUNS, *what = sText_MoreRunStarted;
        break;
    case UNLOCK_FAR_AWAY:
        *have = VarGet(VAR_ROGUE_META_RUNS), *need = ROGUE_FAR_AWAY_RUNS, *what = sText_MoreRunStarted;
        break;
    case UNLOCK_FREE_CHOICE:
        *have = VarGet(VAR_ROGUE_META_RUNS), *need = ROGUE_FREE_CHOICE_RUNS, *what = sText_MoreRunStarted;
        break;
    case UNLOCK_POKEDEX_PICK:
        *have = VarGet(VAR_ROGUE_META_RUNS), *need = ROGUE_POKEDEX_PICK_RUNS, *what = sText_MoreRunStarted;
        break;
    case UNLOCK_ANY_POKEMON:
        *have = VarGet(VAR_ROGUE_META_WINS), *need = ROGUE_ANY_POKEMON_WINS, *what = COMPOUND_STRING(" more win");
        break;
    case UNLOCK_GLOBAL_DEX:
        *have = VarGet(VAR_ROGUE_META_WINS), *need = ROGUE_GLOBAL_DEX_WINS, *what = COMPOUND_STRING(" more win");
        break;
    case UNLOCK_VETERAN:
        *have = VarGet(VAR_ROGUE_META_RUNS), *need = 3, *what = COMPOUND_STRING(" more run");
        break;
    case UNLOCK_BERRIES:
        *have = record->losses, *need = 2, *what = COMPOUND_STRING(" more loss");
        break;
    case UNLOCK_SPARE_STARTERS:
        *have = record->losses, *need = 3, *what = COMPOUND_STRING(" more loss");
        break;
    case UNLOCK_LUCKY_EGG:
        *have = record->losses, *need = 4, *what = COMPOUND_STRING(" more loss");
        break;
    case UNLOCK_SECOND_CHANCE:
        *have = record->losses, *need = 5, *what = COMPOUND_STRING(" more loss");
        break;
    case UNLOCK_FOSSILS:
        *have = record->caves, *need = 5, *what = COMPOUND_STRING(" more cave");
        break;
    case UNLOCK_RARE_FINDS:
        *have = record->catches, *need = 50, *what = COMPOUND_STRING(" more catch");
        break;
    case UNLOCK_RIVAL_GIFT:
        *have = record->rivalWins, *need = 10, *what = COMPOUND_STRING(" more rival win");
        break;
    case UNLOCK_EXPLORER:
        *have = record->dungeons, *need = ROGUE_EXPLORER_DUNGEONS, *what = COMPOUND_STRING(" more dungeon");
        break;
    case UNLOCK_SHINY_CHARM:
        *have = record->shinies, *need = 3, *what = COMPOUND_STRING(" more shiny");
        break;
    case UNLOCK_MARKING_MAP:
        *have = Rogue_CountLegends(record->legends), *need = ROGUE_MARKING_MAP_LEGENDS, *what = COMPOUND_STRING(" more legend");
        break;
    case UNLOCK_MASTER_BALL:
        *have = Rogue_CountLegends(record->legends), *need = ROGUE_MASTER_BALL_LEGENDS, *what = COMPOUND_STRING(" more legend");
        break;
    case UNLOCK_TYPE_CHARM:
        *have = best, *need = ROGUE_TYPE_CHARM_BADGES, *what = COMPOUND_STRING(" more BADGE");
        break;
    case UNLOCK_HIDDEN_ABILITY:
        *have = best, *need = ROGUE_HIDDEN_ABILITY_BADGES, *what = COMPOUND_STRING(" more BADGE");
        break;
    case UNLOCK_EGG_MOVE:
        *have = best, *need = ROGUE_EGG_MOVE_BADGES, *what = COMPOUND_STRING(" more BADGE");
        break;
    default:
        return FALSE;
    }
    return *have < *need;
}

// Plural endings for the progress words.
static u8 *AppendProgressWord(u8 *str, const u8 *what, u32 left)
{
    u8 last = what[StringLength(what) - 1];

    str = StringCopy(str, what);
    if (left == 1)
        return str;
    if (last >= CHAR_A && last <= CHAR_Z)
        return StringCopy(str, COMPOUND_STRING("S"));
    if (what[StringLength(what) - 1] == CHAR_s || what[StringLength(what) - 1] == CHAR_h)
        return StringCopy(str, COMPOUND_STRING("es"));
    if (what[StringLength(what) - 1] == CHAR_y)
    {
        str[-1] = CHAR_i;
        return StringCopy(str, COMPOUND_STRING("es"));
    }
    return StringCopy(str, COMPOUND_STRING("s"));
}

#define SUMMARY_NEXT_SHOWN 4

// Up to SUMMARY_NEXT_SHOWN locked unlocks, nearest first, in out. Those that
// count something come first; the rest show how to earn them.
static u32 PickNextUnlocks(u8 *out)
{
    u32 i, j, n = 0, have, need;
    u32 left[ROGUE_NUM_UNLOCKS];
    const u8 *what;

    for (i = 0; i < ROGUE_NUM_UNLOCKS; i++)
    {
        if (Rogue_IsUnlockEarned(i))
            continue;
        // Fraction still to go, in 256ths. Unlocks that count nothing go last.
        left[i] = GetUnlockProgress(i, &have, &need, &what) ? (need - have) * 256 / need : 0x1000;
        for (j = n; j > 0 && left[out[j - 1]] > left[i]; j--)
            out[j] = out[j - 1];
        out[j] = i;
        n++;
    }
    return min(n, SUMMARY_NEXT_SHOWN);
}


// Page 0 is the run itself, then two new unlocks a page, then what is
// coming up.
static u32 SummaryUnlockPages(void)
{
    u32 i, n = 0;
    for (i = 0; i < ROGUE_NUM_UNLOCKS; i++)
        n += Rogue_IsSummaryUnlock(i);
    return (n + 1) / 2;
}

// After a win, or once a run has done a feat, a page of feats: the new ones
// and the next to chase.
static bool32 SummaryHasFeats(void)
{
    const struct RogueRecord *record = Rogue_GetRecord();
    return VarGet(VAR_ROGUE_RUN_STATE) == ROGUE_RUN_WON || (record->feats & ~record->featsStart) != 0;
}

// How far into the LEAGUE a lost run got: 0 not there, 1-4 the ELITE FOUR
// member it ended against (in the order beaten), 5 the CHAMPION.
static u32 LeagueReached(void)
{
    const struct RogueRecord *record = Rogue_GetRecord();
    u32 i, beaten = 0;
    if (VarGet(VAR_ROGUE_RUN_STATE) == ROGUE_RUN_WON)
        return 0;
    for (i = 0; i < ROGUE_NUM_E4; i++)
        beaten += HasTrainerBeenFought(ROGUE_TRAINER(ROGUE_SLOT_E4_0 + i)) != 0;
    if (beaten == ROGUE_NUM_E4)
        return 5;
    if (beaten || (record->lastPlace == Rogue_GetRegion()->summit && record->lastBadges >= ROGUE_NUM_GYMS))
        return beaten + 1;
    return 0;
}

// PROF. BIRCH's words open the summary, then the run itself.
static u32 SummaryPageCount(void)
{
    u8 next[ROGUE_NUM_UNLOCKS];
    // (RUN 0 has no COMING UP: its unlocks are not the normal runs'.)
    return 2 + SummaryUnlockPages() + SummaryHasFeats() + (!Rogue_IsRun0() && PickNextUnlocks(next) != 0);
}

// The epilogue: what PROF. BIRCH says after a win or a loss
// (src/rogue_text_birch.h), typed out as he speaks.
static void PrintSummaryBirch(void)
{
    const struct RogueRecord *record = Rogue_GetRecord();
    struct RogueBirchMemory m;
    struct RogueTextCtx c;
    struct RogueTextState st;
    char name[POKEMON_NAME_LENGTH * 2 + 1];
    bool32 won = (VarGet(VAR_ROGUE_RUN_STATE) == ROGUE_RUN_WON);

    memset(&m, 0, sizeof(m));
    m.outcome = won ? RBIRCH_WON : RBIRCH_LOST;
    m.badges = won ? ROGUE_NUM_GYMS : record->lastBadges;
    m.fallen = min(VarGet(VAR_ROGUE_FALLEN), 0xFF);
    m.runs = VarGet(VAR_ROGUE_META_RUNS);
    m.place = m.dungeon = 0xFF;
    m.league = LeagueReached();
    // A win that earned a POKeDEX upgrade: he hands it out here.
    if (won && Rogue_IsSummaryUnlock(UNLOCK_GLOBAL_DEX))
        m.dex = 2;
    else if (won && Rogue_IsSummaryUnlock(UNLOCK_NATIONAL_DEX))
        m.dex = 1;
    if (record->lastPlace & ROGUE_LAST_DUNGEON)
        m.dungeon = record->lastPlace & ~ROGUE_LAST_DUNGEON;
    else
        m.place = record->lastPlace;
    // The fallen friend was this run's if it was lost with four badges.
    if (!won && m.badges >= 4 && record->fallenSpecies != SPECIES_NONE && record->fallenName[0] != EOS)
    {
        Rogue_GameToAscii(name, record->fallenName, POKEMON_NAME_LENGTH);
        m.friendName = name;
    }
    Rogue_TextCtx(&c, &st, -1, RSPK_BIRCH);
    RogueText_BirchEpilogue(&c, &m, BIRCH_EPILOGUE_WIDTH, (char *)gStringVar4, 400);
    Rogue_TextToGame(gStringVar4);
    AddTextPrinterParameterized(0, FONT_NORMAL, sText_BirchSays, 2, 1, TEXT_SKIP_DRAW, NULL);
    gTextFlags.canABSpeedUpPrint = TRUE;
    AddTextPrinterParameterized(0, FONT_NORMAL, gStringVar4, 2, 17, GetPlayerTextSpeedDelay(), NULL);
}

static void PrintSummaryRun(void)
{
    const struct RogueRecord *record = Rogue_GetRecord();
    u32 progress = VarGet(VAR_ROGUE_PROGRESS);
    u32 badges = progress < ROGUE_PROGRESS_BADGE_1 ? 0 : progress - 1;
    bool32 won = (VarGet(VAR_ROGUE_RUN_STATE) == ROGUE_RUN_WON);
    u8 *str;
    u32 i, n = 0, y = 1, cols;

    if (badges > ROGUE_NUM_GYMS)
        badges = ROGUE_NUM_GYMS;
    // RUN 0 is not won by a CHAMPION ("You stopped." came first).
    AddTextPrinterParameterized(0, FONT_NORMAL, won ? (Rogue_IsRun0() ? sText_Run0Over : sText_RunWon) : sText_RunLost,
                                2, y, TEXT_SKIP_DRAW, NULL);
    y += 18;

    // The run code a friend types to play this region, left in
    // gStringVar3 for texttest.py --runcode; the region's name without one.
    if (Rogue_RunCode(gStringVar3))
        str = StringCopy(StringCopy(gStringVar4, sText_RunCode), gStringVar3);
    else
        str = Rogue_RegionName(StringCopy(gStringVar4, sText_Seed), Rogue_GetSeed());
    str = StringCopy(str, sText_Badges);
    AppendNumber(str, badges, 1);
    AddTextPrinterParameterized(0, FONT_NORMAL, gStringVar4, 2, y, TEXT_SKIP_DRAW, NULL);
    y += 14;

    if (won)
    {
        if (!Rogue_IsRun0())
        {
            AddTextPrinterParameterized(0, FONT_NORMAL, sText_Champion, 2, y, TEXT_SKIP_DRAW, NULL);
            y += 14;
        }
    }
    else if (LeagueReached())
    {
        // Ended in the LEAGUE: how far.
        if (LeagueReached() == 5)
            StringCopy(gStringVar4, sText_EndedChampion);
        else
            StringCopy(AppendNumber(StringCopy(gStringVar4, sText_EndedElite), LeagueReached() - 1, 1), sText_OfFour);
        AddTextPrinterParameterized(0, FONT_NORMAL, gStringVar4, 2, y, TEXT_SKIP_DRAW, NULL);
        y += 14;
    }
    else if ((record->lastPlace & ROGUE_LAST_DUNGEON) && (record->lastPlace & ~ROGUE_LAST_DUNGEON) < Rogue_GetRegion()->nDungeons)
    {
        str = StringCopy(gStringVar4, sText_EndedAt);
        Rogue_CopyDungeonName(str, record->lastPlace & ~ROGUE_LAST_DUNGEON);
        AddTextPrinterParameterized(0, FONT_NORMAL, gStringVar4, 2, y, TEXT_SKIP_DRAW, NULL);
        y += 14;
    }
    else if (record->lastPlace < Rogue_GetRegion()->nPlaces)
    {
        str = StringCopy(gStringVar4, sText_EndedAt);
        Rogue_CopyPlaceName(str, record->lastPlace);
        AddTextPrinterParameterized(0, FONT_NORMAL, gStringVar4, 2, y, TEXT_SKIP_DRAW, NULL);
        y += 14;
    }

    // The team at the end, two to a line; five or six, three to a line (a
    // third row pushed "Runs: 20 Wins: 4" under "Press A"), a long name cut
    // to its column.
    for (i = 0, cols = 2; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        if (GetMonData(mon, MON_DATA_SANITY_HAS_SPECIES) && !GetMonData(mon, MON_DATA_IS_EGG))
            cols += (++n == 5);
    }
    for (i = 0, n = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        u32 len;
        u8 level[8];
        if (!GetMonData(mon, MON_DATA_SANITY_HAS_SPECIES) || GetMonData(mon, MON_DATA_IS_EGG))
            continue;
        GetMonData(mon, MON_DATA_NICKNAME, gStringVar1);
        AppendNumber(StringCopy(level, sText_Lv), GetMonData(mon, MON_DATA_LEVEL), 3);
        for (len = StringLength(gStringVar1); len > 1; len--)
        {
            gStringVar1[len] = EOS;
            StringCopy(StringCopy(gStringVar4, gStringVar1), level);
            if (GetStringWidth(FONT_SMALL, gStringVar4, 0) <= 204 / cols - 4)
                break;
        }
        StringCopy(StringCopy(gStringVar4, gStringVar1), level);
        AddTextPrinterParameterized(0, FONT_SMALL, gStringVar4, 2 + (n % cols) * (204 / cols), y + (n / cols) * 12, TEXT_SKIP_DRAW, NULL);
        n++;
    }
    y += ((n + cols - 1) / cols) * 12 + 4;

    str = StringCopy(gStringVar4, sText_Fallen);
    str = AppendNumber(str, VarGet(VAR_ROGUE_FALLEN), 3);
    str = StringCopy(str, sText_Time);
    str = AppendNumber(str, gSaveBlock2Ptr->playTimeHours, 3);
    str = StringCopy(str, sText_Colon);
    ConvertIntToDecimalStringN(str, gSaveBlock2Ptr->playTimeMinutes, STR_CONV_MODE_LEADING_ZEROS, 2);
    AddTextPrinterParameterized(0, FONT_NORMAL, gStringVar4, 2, y, TEXT_SKIP_DRAW, NULL);
    y += 14;

    // A dungeon counts once its boss is beaten or caught (its master, its
    // guardian, its legend, the admin): reaching the top or the bottom is
    // not enough, so the line says what it counts.
    str = StringCopy(gStringVar4, sText_Dungeons);
    str = AppendNumber(str, Rogue_CountDungeonsCleared(), 2);
    str = StringCopy(str, sText_BossesOf);
    AppendNumber(str, Rogue_CountDungeonBosses(), 2);
    AddTextPrinterParameterized(0, FONT_NORMAL, gStringVar4, 2, y, TEXT_SKIP_DRAW, NULL);
    y += 14;

    str = StringCopy(gStringVar4, sText_Record);
    str = AppendNumber(str, VarGet(VAR_ROGUE_META_RUNS), 4);
    str = StringCopy(str, sText_Runs);
    AppendNumber(str, VarGet(VAR_ROGUE_META_WINS), 4);
    AddTextPrinterParameterized(0, FONT_NORMAL, gStringVar4, 2, y, TEXT_SKIP_DRAW, NULL);
}

// Two of the unlocks this run earned, with what they do.
static void PrintSummaryUnlocks(u32 page)
{
    u32 i, n = 0, y = 1;

    AddTextPrinterParameterized(0, FONT_NORMAL, sText_NewUnlocks, 2, y, TEXT_SKIP_DRAW, NULL);
    y += 18;
    for (i = 0; i < ROGUE_NUM_UNLOCKS; i++)
    {
        if (!Rogue_IsSummaryUnlock(i))
            continue;
        if (n++ / 2 != page)
            continue;
        AddTextPrinterParameterized(0, FONT_NORMAL, Rogue_GetUnlockName(i), 2, y, TEXT_SKIP_DRAW, NULL);
        AddTextPrinterParameterized(0, FONT_NORMAL, Rogue_GetUnlockDescription(i), 10, y + 14, TEXT_SKIP_DRAW, NULL);
        y += 46;
    }
}

// The nearest unlocks still to earn.
static void PrintSummaryNext(void)
{
    u8 next[ROGUE_NUM_UNLOCKS];
    u32 i, n = PickNextUnlocks(next), y = 1, have, need;
    const u8 *what;
    u8 *str;

    AddTextPrinterParameterized(0, FONT_NORMAL, sText_NextUnlocks, 2, y, TEXT_SKIP_DRAW, NULL);
    y += 18;
    // Three fit above the footer.
    if (n > 3)
        n = 3;
    for (i = 0; i < n; i++)
    {
        str = StringCopy(gStringVar4, Rogue_GetUnlockName(next[i]));
        AddTextPrinterParameterized(0, FONT_NORMAL, gStringVar4, 2, y, TEXT_SKIP_DRAW, NULL);
        if (!GetUnlockProgress(next[i], &have, &need, &what))
        {
            StringCopy(gStringVar4, Rogue_GetUnlockHint(next[i]));
        }
        else if (what == sText_MoreRunStarted && need - have == 1)
        {
            StringCopy(gStringVar4, sText_NextRunStarts);
        }
        else
        {
            str = AppendNumber(gStringVar4, need - have, 2);
            AppendProgressWord(str, what, need - have);
        }
        AddTextPrinterParameterized(0, FONT_SMALL, gStringVar4, 10, y + 13, TEXT_SKIP_DRAW, NULL);
        y += 24;
    }
    AddTextPrinterParameterized(0, FONT_SMALL, sText_NextUnlocksMenu, 2, 100, TEXT_SKIP_DRAW, NULL);
}

// The feats this run did, then the next ones to do: those that count
// something first, nearest done first, then the rest in order.
#define SUMMARY_FEATS_SHOWN 3
static void PrintSummaryFeats(void)
{
    const struct RogueRecord *record = Rogue_GetRecord();
    u32 newFeats = record->feats & ~record->featsStart;
    u8 order[ROGUE_NUM_FEATS];
    u32 left[ROGUE_NUM_FEATS];
    u32 i, j, n = 0, y = 1, have, need, total = 0;
    u8 *str;

    for (i = 0; i < ROGUE_NUM_FEATS; i++)
    {
        total += Rogue_IsFeatDone(i);
        if ((newFeats >> i) & 1)
            left[i] = 0;
        else if (Rogue_IsFeatDone(i))
            continue;
        else
            left[i] = Rogue_GetFeatProgress(i, &have, &need) ? 1 + (need - have) * 256 / need : 0x1000 + i;
        for (j = n; j > 0 && left[order[j - 1]] > left[i]; j--)
            order[j] = order[j - 1];
        order[j] = i;
        n++;
    }
    str = StringCopy(gStringVar4, COMPOUND_STRING("{COLOR BLUE}FEATS  "));
    str = ConvertIntToDecimalStringN(str, total, STR_CONV_MODE_LEFT_ALIGN, 2);
    *str++ = CHAR_SLASH;
    ConvertIntToDecimalStringN(str, ROGUE_NUM_FEATS, STR_CONV_MODE_LEFT_ALIGN, 2);
    AddTextPrinterParameterized(0, FONT_NORMAL, gStringVar4, 2, y, TEXT_SKIP_DRAW, NULL);
    y += 18;
    for (i = 0; i < n && i < SUMMARY_FEATS_SHOWN; i++)
    {
        u32 feat = order[i];
        str = StringCopy(gStringVar4, Rogue_GetFeatName(feat));
        if ((newFeats >> feat) & 1)
            StringCopy(str, COMPOUND_STRING("  {COLOR GREEN}DONE!"));
        AddTextPrinterParameterized(0, FONT_NORMAL, gStringVar4, 2, y, TEXT_SKIP_DRAW, NULL);
        if (!((newFeats >> feat) & 1) && Rogue_GetFeatProgress(feat, &have, &need))
        {
            str = StringCopy(gStringVar4, Rogue_GetFeatHint(feat));
            str = StringCopy(str, COMPOUND_STRING(" ("));
            str = ConvertIntToDecimalStringN(str, have, STR_CONV_MODE_LEFT_ALIGN, 2);
            *str++ = CHAR_SLASH;
            str = ConvertIntToDecimalStringN(str, need, STR_CONV_MODE_LEFT_ALIGN, 2);
            StringCopy(str, COMPOUND_STRING(")"));
        }
        else if ((newFeats >> feat) & 1 && Rogue_GetFeatReward(feat))
        {
            str = StringCopy(gStringVar4, sText_RewardShort);
            StringCopy(str, Rogue_GetFeatReward(feat));
        }
        else
        {
            StringCopy(gStringVar4, Rogue_GetFeatHint(feat));
        }
        AddTextPrinterParameterized(0, FONT_SMALL, gStringVar4, 10, y + 13, TEXT_SKIP_DRAW, NULL);
        y += 24;
    }
    AddTextPrinterParameterized(0, FONT_SMALL, sText_FeatsMenu, 2, 100, TEXT_SKIP_DRAW, NULL);
}

#define tPage data[0]

static void PrintSummary(u32 page)
{
    u32 pages = SummaryPageCount(), last = page + 1 >= pages;

    FillWindowPixelBuffer(0, PIXEL_FILL(1));
    if (page == 0 && Rogue_IsRun0() && VarGet(VAR_ROGUE_RUN_STATE) != ROGUE_RUN_WON)
    {
        // RUN 0 lost: the Director's page, DATA COLLECTED.
        Rogue_Run0BufferDataCollected(gStringVar4);
        Rogue_ReleaseDirector();
        AddTextPrinterParameterized(0, FONT_NORMAL, gStringVar4, 2, 1, TEXT_SKIP_DRAW, NULL);
    }
    else if (page == 0 && Rogue_Run0SummaryPage(gStringVar4))
    {
        // RUN 0 won: how it ended (src/rogue_run0_story.c).
        AddTextPrinterParameterized(0, FONT_NORMAL, gStringVar4, 2, 1, TEXT_SKIP_DRAW, NULL);
    }
    else if (page == 0)
        PrintSummaryBirch();
    else if (--page == 0)
        PrintSummaryRun();
    else if (page <= SummaryUnlockPages())
        PrintSummaryUnlocks(page - 1);
    else if (page == SummaryUnlockPages() + 1 && SummaryHasFeats())
        PrintSummaryFeats();
    else
        PrintSummaryNext();
    AddTextPrinterParameterized(0, FONT_SMALL, last ? sText_PressA : sText_PressANext, 2, 114, TEXT_SKIP_DRAW, NULL);
    PutWindowTilemap(0);
    CopyWindowToVram(0, COPYWIN_FULL);
}

static void Task_SummaryExit(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        FreeAllWindowBuffers();
        DestroyTask(taskId);
        DoSoftReset();
    }
}

static void Task_SummaryInput(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    // A or B while he speaks only hurries him along.
    if (gPaletteFade.active || IsTextPrinterActiveOnWindow(0))
        return;
    if (JOY_NEW(A_BUTTON | START_BUTTON))
    {
        PlaySE(SE_SELECT);
        if (tPage + 1 < (s32)SummaryPageCount())
        {
            PrintSummary(++tPage);
            return;
        }
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_SummaryExit;
    }
}

#undef tPage

void CB2_RogueRunSummary(void)
{
    u32 x, y;
    bool32 won = VarGet(VAR_ROGUE_RUN_STATE) == ROGUE_RUN_WON;

    // A lost run is written to the save before anything is shown, so turning
    // the game off here cannot bring the run back. A won run was already
    // saved by the Hall of Fame (RUN 0 has none: it saves here).
    if (!won || Rogue_IsRun0())
        TrySavingData(SAVE_NORMAL);
    // Then the run log, and RUN 0's checkpoint and kept POKeMON, in the
    // Director's sector, after the save (src/rogue_run0.c).
    Rogue_RunLogEnd(won ? ROGUE_LOG_WON : ROGUE_LOG_LOST);
    if (Rogue_IsRun0())
        Rogue_Run0OnRunEnd(won);
    // They belong with the save just written (the team kept goes with it).
    Rogue_DirectorNoteSave();
    Rogue_SaveDirector();

    InitScreen(sSummaryBgTemplates, ARRAY_COUNT(sSummaryBgTemplates), sSummaryWindows);
    CpuCopy16(sSummaryTiles, (void *)BG_CHAR_ADDR(TILE_CHAR_BASE), sizeof(sSummaryTiles));
    LoadPalette(sSummaryPalette, BG_PLTT_ID(0), PLTT_SIZE_4BPP);
    for (y = 0; y < 32; y++)
        for (x = 0; x < 32; x++)
            PutTile(BG_LAND, x, y, SUMMARY_TILE_DARK);
    DrawStdFrameWithCustomTileAndPalette(0, FALSE, 2, 14);
    PrintSummary(0);
    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
    SetVBlankCallback(VBlankCB);
    EnableInterrupts(INTR_FLAG_VBLANK);
    CreateTask(Task_SummaryInput, 0);
    SetMainCallback2(CB2_RogueScreenMain);
}

// ---------------------------------------------------------------------------
// UNLOCKS, from the main menu: three pages, switched with left and right (or
// L and R): the unlocks, the feats, and the LEGEND LOG.

#define UNLOCK_ROWS 5       // rows in view
#define UNLOCK_ROW_HEIGHT 16
#define LEGEND_ROWS 4       // LEGEND LOG rows in view
#define LEGEND_ROW_HEIGHT 20
#define TAG_UNLOCK_ARROWS 0x5202

enum
{
    UWIN_HEADER,
    UWIN_LIST,
    UWIN_INFO,
};

enum
{
    TAB_UNLOCKS,
    TAB_FEATS,
    TAB_LEGENDS,
    TAB_COUNT,
};

static const struct WindowTemplate sUnlockWindows[] =
{
    [UWIN_HEADER] = { .bg = BG_TEXT, .tilemapLeft = 1, .tilemapTop = 0, .width = 28, .height = 2, .paletteNum = 15, .baseBlock = 11 },
    [UWIN_LIST]   = { .bg = BG_TEXT, .tilemapLeft = 1, .tilemapTop = 3, .width = 28, .height = 10, .paletteNum = 15, .baseBlock = 11 + 28 * 2 },
    [UWIN_INFO]   = { .bg = BG_TEXT, .tilemapLeft = 1, .tilemapTop = 15, .width = 28, .height = 4, .paletteNum = 15, .baseBlock = 11 + 28 * 12 },
    DUMMY_WIN_TEMPLATE
};

// The list order: starters first, then help, then things to find, then the
// rewards for a win and for legends.
static const u8 sUnlockOrder[ROGUE_NUM_UNLOCK_ROWS] =
{
    UNLOCK_EEVEE,
    UNLOCK_PARTNERS,
    UNLOCK_BABIES,
    UNLOCK_RISING_STARS,
    UNLOCK_DRAGONS,
    UNLOCK_FOSSIL_STARTERS,
    UNLOCK_FAR_AWAY,
    UNLOCK_STARTER_CASE,
    UNLOCK_FREE_CHOICE,
    UNLOCK_POKEDEX_PICK,
    UNLOCK_ANY_POKEMON,
    UNLOCK_FALLEN_FRIEND,
    UNLOCK_CHAMP_PARTNER,
    UNLOCK_TYPE_CHARM,
    UNLOCK_HIDDEN_ABILITY,
    UNLOCK_EGG_MOVE,
    UNLOCK_VETERAN,
    UNLOCK_BERRIES,
    UNLOCK_LUCKY_EGG,
    UNLOCK_TRAINER_CANDY,
    UNLOCK_SECOND_CHANCE,
    UNLOCK_LATE_CAMP,
    UNLOCK_EXPLORER,
    UNLOCK_SCOUTING,
    UNLOCK_SPARE_STARTERS,
    UNLOCK_SHINY_CLAUSE,
    UNLOCK_SHINY_CHARM,
    UNLOCK_FOSSILS,
    UNLOCK_RARE_FINDS,
    UNLOCK_RIVAL_GIFT,
    UNLOCK_KIT,
    UNLOCK_HARDCORE,
    UNLOCK_NATIONAL_DEX, // the POKeDEX row: CLASSIC, NATIONAL or GLOBAL
    UNLOCK_MARKING_MAP,
    UNLOCK_MASTER_BALL,
    UNLOCK_DOWSING,
};

static const u8 sTextColor_Header[] = { TEXT_COLOR_TRANSPARENT, TEXT_COLOR_WHITE, TEXT_COLOR_DARK_GRAY };
static const u8 sTextColor_Tab[] = { TEXT_COLOR_TRANSPARENT, TEXT_COLOR_LIGHT_GRAY, TEXT_COLOR_DARK_GRAY };
static const u8 sTextColor_Row[] = { TEXT_COLOR_WHITE, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY };
static const u8 sTextColor_RowDim[] = { TEXT_COLOR_WHITE, TEXT_COLOR_LIGHT_GRAY, TEXT_COLOR_WHITE };
static const u8 sTextColor_On[] = { TEXT_COLOR_WHITE, TEXT_COLOR_BLUE, TEXT_COLOR_LIGHT_BLUE };
static const u8 sTextColor_Off[] = { TEXT_COLOR_WHITE, TEXT_COLOR_RED, TEXT_COLOR_LIGHT_RED };
static const u8 sTextColor_Done[] = { TEXT_COLOR_WHITE, TEXT_COLOR_GREEN, TEXT_COLOR_LIGHT_GREEN };
// A legend of the run being played, on its light blue cell.
static const u8 sTextColor_Here[] = { TEXT_COLOR_LIGHT_BLUE, TEXT_COLOR_BLUE, TEXT_COLOR_WHITE };

static const u8 *const sTabNames[TAB_COUNT] =
{
    [TAB_UNLOCKS] = COMPOUND_STRING("UNLOCKS"),
    [TAB_FEATS]   = COMPOUND_STRING("FEATS"),
    [TAB_LEGENDS] = COMPOUND_STRING("LEGENDS"),
};
static const u8 sText_UnlocksCount[] = _("{STR_VAR_1}/{STR_VAR_2}");
static const u8 sText_Locked[] = _("???");
static const u8 sText_On[] = _("ON");
static const u8 sText_Off[] = _("OFF");
static const u8 sText_Done[] = _("DONE");
static const u8 sText_DexRow[] = _("POKéDEX");
static const u8 sText_DexSwitch[] = _("A: CLASSIC, NATIONAL or GLOBAL.");
static const u8 sText_DexSwitch2[] = _("A: CLASSIC or NATIONAL.");
static const u8 sText_DexGlobalHint[] = _("GLOBAL: become CHAMPION 3 times.");
static const u8 sText_Unseen[] = _("?");
static const u8 sText_UnseenName[] = _("?????");
static const u8 sText_ToUnlock[] = _("{COLOR DARK_GRAY}How to unlock:\n");
static const u8 sText_Reward[] = _("Reward: ");
static const u8 sText_SoFar[] = _("So far: ");
static const u8 sText_Of[] = _(" of ");
static const u8 sText_InRegion[] = _(" in this region");
// A type's upgrade row names the starters' first types that earned it.
static const u8 *const sTypeShort[NUMBER_OF_MON_TYPES] =
{
    [TYPE_NORMAL] = COMPOUND_STRING("NRM"), [TYPE_FIGHTING] = COMPOUND_STRING("FGT"), [TYPE_FLYING] = COMPOUND_STRING("FLY"),
    [TYPE_POISON] = COMPOUND_STRING("PSN"), [TYPE_GROUND] = COMPOUND_STRING("GRD"), [TYPE_ROCK] = COMPOUND_STRING("RCK"),
    [TYPE_BUG] = COMPOUND_STRING("BUG"), [TYPE_GHOST] = COMPOUND_STRING("GHO"), [TYPE_STEEL] = COMPOUND_STRING("STL"),
    [TYPE_FIRE] = COMPOUND_STRING("FIR"), [TYPE_WATER] = COMPOUND_STRING("WTR"), [TYPE_GRASS] = COMPOUND_STRING("GRS"),
    [TYPE_ELECTRIC] = COMPOUND_STRING("ELE"), [TYPE_PSYCHIC] = COMPOUND_STRING("PSY"), [TYPE_ICE] = COMPOUND_STRING("ICE"),
    [TYPE_DRAGON] = COMPOUND_STRING("DRG"), [TYPE_DARK] = COMPOUND_STRING("DRK"), [TYPE_FAIRY] = COMPOUND_STRING("FAI"),
};

struct UnlocksScreen
{
    u8 tab;
    u8 cursor;
    u8 arrowTask;
    bool8 changed;
    u16 scroll; // first row in view, also read by the scroll arrows
    u32 regionLegends; // the legends of the run to continue, if any
    u8 icons[RLEG_COUNT]; // the LEGEND LOG's icons by legend, or MAX_SPRITES
};

// The screen's state is allocated when it opens; the task keeps the pointer.
#define tScreen 0

static struct UnlocksScreen *GetUnlocksScreen(u8 taskId)
{
    return (struct UnlocksScreen *)GetWordTaskArg(taskId, tScreen);
}

static bool32 IsTypeUpgrade(u32 unlock)
{
    return unlock == UNLOCK_TYPE_CHARM || unlock == UNLOCK_HIDDEN_ABILITY || unlock == UNLOCK_EGG_MOVE;
}

static u32 TabRows(u32 tab)
{
    if (tab == TAB_FEATS)
        return ROGUE_NUM_FEATS;
    if (tab == TAB_LEGENDS)
        return ROGUE_LEGEND_ROWS;
    return ROGUE_NUM_UNLOCK_ROWS;
}

static u32 TabRowsInView(u32 tab)
{
    return tab == TAB_LEGENDS ? LEGEND_ROWS : UNLOCK_ROWS;
}

static u32 CountFeats(void)
{
    u32 i, count = 0;
    for (i = 0; i < ROGUE_NUM_FEATS; i++)
        count += Rogue_IsFeatDone(i);
    return count;
}

static u32 CountEarnedUnlocks(void)
{
    return Rogue_CountEarnedUnlocks();
}

// The tabs, L and R either side, and how far the current one is done.
static void PrintUnlocksHeader(struct UnlocksScreen *screen)
{
    u32 i, x = 2, done, total = TabRows(screen->tab);

    FillWindowPixelBuffer(UWIN_HEADER, PIXEL_FILL(0));
    AddTextPrinterParameterized3(UWIN_HEADER, FONT_NORMAL, x, 1, sTextColor_Header, TEXT_SKIP_DRAW, COMPOUND_STRING("{L_BUTTON}"));
    x += 22;
    for (i = 0; i < TAB_COUNT; i++)
    {
        AddTextPrinterParameterized3(UWIN_HEADER, FONT_NORMAL, x, 1, i == screen->tab ? sTextColor_Header : sTextColor_Tab,
                                     TEXT_SKIP_DRAW, sTabNames[i]);
        x += GetStringWidth(FONT_NORMAL, sTabNames[i], 0) + 8;
    }
    AddTextPrinterParameterized3(UWIN_HEADER, FONT_NORMAL, x, 1, sTextColor_Header, TEXT_SKIP_DRAW, COMPOUND_STRING("{R_BUTTON}"));
    if (screen->tab == TAB_FEATS)
        done = CountFeats();
    else if (screen->tab == TAB_LEGENDS)
        done = Rogue_CountLegends(Rogue_GetRecord()->legends), total = ROGUE_NUM_LEGENDS;
    else
        done = CountEarnedUnlocks();
    ConvertIntToDecimalStringN(gStringVar1, done, STR_CONV_MODE_LEFT_ALIGN, 2);
    ConvertIntToDecimalStringN(gStringVar2, total, STR_CONV_MODE_LEFT_ALIGN, 2);
    StringExpandPlaceholders(gStringVar4, sText_UnlocksCount);
    AddTextPrinterParameterized3(UWIN_HEADER, FONT_NORMAL, GetStringRightAlignXOffset(FONT_NORMAL, gStringVar4, 28 * 8) - 2, 1, sTextColor_Header, TEXT_SKIP_DRAW, gStringVar4);
    PutWindowTilemap(UWIN_HEADER);
    CopyWindowToVram(UWIN_HEADER, COPYWIN_FULL);
}

static void PrintUnlockRow(struct UnlocksScreen *screen, u32 row, u32 index)
{
    u32 unlock = sUnlockOrder[index];
    u32 y = row * UNLOCK_ROW_HEIGHT;

    if (index == screen->cursor)
        AddTextPrinterParameterized3(UWIN_LIST, FONT_NORMAL, 0, y, sTextColor_Row, TEXT_SKIP_DRAW, gText_SelectorArrow3);
    if (!Rogue_IsUnlockEarned(unlock))
    {
        AddTextPrinterParameterized3(UWIN_LIST, FONT_NORMAL, 10, y, sTextColor_RowDim, TEXT_SKIP_DRAW, sText_Locked);
        return;
    }
    // The POKeDEX row shows the choice instead of ON or OFF.
    if (unlock == UNLOCK_NATIONAL_DEX)
    {
        const u8 *choice = Rogue_GetDexChoiceName(Rogue_GetDexChoice());
        AddTextPrinterParameterized3(UWIN_LIST, FONT_NORMAL, 10, y, sTextColor_Row, TEXT_SKIP_DRAW, sText_DexRow);
        AddTextPrinterParameterized3(UWIN_LIST, FONT_NORMAL, GetStringRightAlignXOffset(FONT_NORMAL, choice, 28 * 8) - 6, y,
                                     sTextColor_On, TEXT_SKIP_DRAW, choice);
        return;
    }
    AddTextPrinterParameterized3(UWIN_LIST, FONT_NORMAL, 10, y, sTextColor_Row, TEXT_SKIP_DRAW, Rogue_GetUnlockName(unlock));
    // A type's upgrade belongs to the starters' first types that earned it:
    // the three with the most badges, or two and how many more.
    if (IsTypeUpgrade(unlock))
    {
        static const u8 sBadges[] = { [UNLOCK_TYPE_CHARM] = ROGUE_TYPE_CHARM_BADGES, [UNLOCK_HIDDEN_ABILITY] = ROGUE_HIDDEN_ABILITY_BADGES, [UNLOCK_EGG_MOVE] = ROGUE_EGG_MOVE_BADGES };
        u8 types[NUMBER_OF_MON_TYPES];
        u32 n = 0, i, j, type;
        for (type = 0; type < NUMBER_OF_MON_TYPES; type++)
        {
            if (Rogue_GetTypeBadges(type) < sBadges[unlock] || sTypeShort[type] == NULL)
                continue;
            for (j = n++; j > 0 && Rogue_GetTypeBadges(types[j - 1]) < Rogue_GetTypeBadges(type); j--)
                types[j] = types[j - 1];
            types[j] = type;
        }
        for (i = 0; i < n && i < 3; i++)
        {
            if (i == 2 && n > 3)
            {
                u8 *str = gStringVar4;
                *str++ = CHAR_PLUS;
                ConvertIntToDecimalStringN(str, n - 2, STR_CONV_MODE_LEFT_ALIGN, 2);
                AddTextPrinterParameterized3(UWIN_LIST, FONT_SMALL, 124 + i * 20, y + 2, sTextColor_Row, TEXT_SKIP_DRAW, gStringVar4);
                break;
            }
            AddTextPrinterParameterized3(UWIN_LIST, FONT_SMALL, 124 + i * 20, y + 2, sTextColor_Row, TEXT_SKIP_DRAW, sTypeShort[types[i]]);
        }
    }
    if (Rogue_IsUnlockOn(unlock))
        AddTextPrinterParameterized3(UWIN_LIST, FONT_NORMAL, 196, y, sTextColor_On, TEXT_SKIP_DRAW, sText_On);
    else
        AddTextPrinterParameterized3(UWIN_LIST, FONT_NORMAL, 196, y, sTextColor_Off, TEXT_SKIP_DRAW, sText_Off);
}

// A feat: DONE, how far along ("2/10"), or nothing.
static void PrintFeatRow(struct UnlocksScreen *screen, u32 row, u32 feat)
{
    u32 y = row * UNLOCK_ROW_HEIGHT, have, need;
    u8 *str;

    if (feat == screen->cursor)
        AddTextPrinterParameterized3(UWIN_LIST, FONT_NORMAL, 0, y, sTextColor_Row, TEXT_SKIP_DRAW, gText_SelectorArrow3);
    AddTextPrinterParameterized3(UWIN_LIST, FONT_NORMAL, 10, y, sTextColor_Row, TEXT_SKIP_DRAW, Rogue_GetFeatName(feat));
    if (Rogue_IsFeatDone(feat))
    {
        AddTextPrinterParameterized3(UWIN_LIST, FONT_NORMAL, 190, y, sTextColor_Done, TEXT_SKIP_DRAW, sText_Done);
    }
    else if (Rogue_GetFeatProgress(feat, &have, &need))
    {
        str = ConvertIntToDecimalStringN(gStringVar4, have, STR_CONV_MODE_LEFT_ALIGN, 2);
        *str++ = CHAR_SLASH;
        ConvertIntToDecimalStringN(str, need, STR_CONV_MODE_LEFT_ALIGN, 2);
        AddTextPrinterParameterized3(UWIN_LIST, FONT_NORMAL, 190, y, sTextColor_RowDim, TEXT_SKIP_DRAW, gStringVar4);
    }
}

// The LEGEND LOG: a row per set, with a cell for each legend: its icon,
// moving once caught and faded if only met, or "?" if never met. The run
// being played has its legends on light blue cells.
static u32 LegendCellX(u32 i)
{
    return 80 + i * 36;
}

static void PrintLegendRow(struct UnlocksScreen *screen, u32 row, u32 index)
{
    const struct RogueRecord *record = Rogue_GetRecord();
    u32 y = row * LEGEND_ROW_HEIGHT, i, legend, x;

    if (index == screen->cursor)
        AddTextPrinterParameterized3(UWIN_LIST, FONT_NORMAL, 0, y + 2, sTextColor_Row, TEXT_SKIP_DRAW, gText_SelectorArrow3);
    AddTextPrinterParameterized3(UWIN_LIST, FONT_NORMAL, 10, y + 2, sTextColor_Row, TEXT_SKIP_DRAW, Rogue_GetLegendRowName(index, FALSE));
    for (i = 0; i < 4 && (legend = gRogueLegendRows[index][i]) != RLEG_NONE; i++)
    {
        x = LegendCellX(i);
        if (screen->regionLegends & (1u << legend))
            FillWindowPixelRect(UWIN_LIST, PIXEL_FILL(TEXT_COLOR_LIGHT_BLUE), x - 15, y + 1, 30, LEGEND_ROW_HEIGHT - 2);
        if (!(record->legendsSeen & (1u << legend)))
            AddTextPrinterParameterized3(UWIN_LIST, FONT_NORMAL, x - GetStringWidth(FONT_NORMAL, sText_Unseen, 0) / 2, y + 2,
                                         (screen->regionLegends & (1u << legend)) ? sTextColor_Here : sTextColor_Row,
                                         TEXT_SKIP_DRAW, sText_Unseen);
    }
}

static void PlaceLegendIcons(struct UnlocksScreen *screen)
{
    u32 row, i, legend;
    for (row = 0; row < ROGUE_LEGEND_ROWS; row++)
    {
        for (i = 0; i < 4 && (legend = gRogueLegendRows[row][i]) != RLEG_NONE; i++)
        {
            struct Sprite *sprite;
            if (screen->icons[legend] == MAX_SPRITES)
                continue;
            sprite = &gSprites[screen->icons[legend]];
            sprite->invisible = row < screen->scroll || row >= screen->scroll + LEGEND_ROWS;
            sprite->x = 8 + LegendCellX(i);
            sprite->y = 24 + (row - screen->scroll) * LEGEND_ROW_HEIGHT + LEGEND_ROW_HEIGHT / 2 - 2;
        }
    }
}

static void CreateLegendIcons(struct UnlocksScreen *screen)
{
    const struct RogueRecord *record = Rogue_GetRecord();
    u32 legend;

    LoadMonIconPalettes();
    // A legend only met shows faded, as a Pokemon seen but not caught.
    SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT2_ALL);
    SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(8, 8));
    for (legend = RLEG_NONE + 1; legend < RLEG_COUNT; legend++)
    {
        bool32 caught = (record->legends >> legend) & 1;
        u8 id;
        screen->icons[legend] = MAX_SPRITES;
        if (!((record->legendsSeen | record->legends) & (1u << legend)))
            continue;
        id = CreateMonIcon(Rogue_LegendSpecies(legend), caught ? SpriteCB_MonIcon : SpriteCallbackDummy, 0, 0, 0, 0);
        if (id == MAX_SPRITES)
            continue;
        gSprites[id].oam.priority = 0;
        if (!caught)
            gSprites[id].oam.objMode = ST_OAM_OBJ_BLEND;
        screen->icons[legend] = id;
    }
    PlaceLegendIcons(screen);
}

static void DestroyLegendIcons(struct UnlocksScreen *screen)
{
    u32 legend;
    for (legend = RLEG_NONE + 1; legend < RLEG_COUNT; legend++)
    {
        if (screen->icons[legend] != MAX_SPRITES)
            FreeAndDestroyMonIconSprite(&gSprites[screen->icons[legend]]);
        screen->icons[legend] = MAX_SPRITES;
    }
    FreeMonIconPalettes();
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
}

static void PrintUnlocksList(struct UnlocksScreen *screen)
{
    u32 row, rows = TabRows(screen->tab);

    FillWindowPixelBuffer(UWIN_LIST, PIXEL_FILL(1));
    for (row = 0; row < TabRowsInView(screen->tab) && screen->scroll + row < rows; row++)
    {
        if (screen->tab == TAB_FEATS)
            PrintFeatRow(screen, row, screen->scroll + row);
        else if (screen->tab == TAB_LEGENDS)
            PrintLegendRow(screen, row, screen->scroll + row);
        else
            PrintUnlockRow(screen, row, screen->scroll + row);
    }
    if (screen->tab == TAB_LEGENDS)
        PlaceLegendIcons(screen);
    PutWindowTilemap(UWIN_LIST);
    CopyWindowToVram(UWIN_LIST, COPYWIN_GFX);
}

// A line in the narrow font if it does not fit.
static void PrintInfoLine(const u8 *str, u32 y, const u8 *colors)
{
    u32 font = GetStringWidth(FONT_NORMAL, str, 0) <= 28 * 8 - 4 ? FONT_NORMAL : FONT_NARROW;
    AddTextPrinterParameterized3(UWIN_INFO, font, 2, y, colors, TEXT_SKIP_DRAW, str);
}

static void PrintFeatInfo(u32 feat)
{
    u32 have, need;
    u8 *str;

    PrintInfoLine(Rogue_GetFeatHint(feat), 1, sTextColor_Row);
    if (Rogue_GetFeatReward(feat))
    {
        str = StringCopy(gStringVar4, sText_Reward);
        StringCopy(str, Rogue_GetFeatReward(feat));
        PrintInfoLine(gStringVar4, 17, sTextColor_On);
    }
    else if (!Rogue_IsFeatDone(feat) && Rogue_GetFeatProgress(feat, &have, &need))
    {
        str = StringCopy(gStringVar4, sText_SoFar);
        str = ConvertIntToDecimalStringN(str, have, STR_CONV_MODE_LEFT_ALIGN, 2);
        str = StringCopy(str, sText_Of);
        ConvertIntToDecimalStringN(str, need, STR_CONV_MODE_LEFT_ALIGN, 2);
        PrintInfoLine(gStringVar4, 17, sTextColor_RowDim);
    }
}

// A set's full name and how many are caught; then each legend's name, or
// "?????" for one never met.
static void PrintLegendInfo(struct UnlocksScreen *screen, u32 row)
{
    const struct RogueRecord *record = Rogue_GetRecord();
    u32 i, legend, x = 2, here = 0, size = 0, bits = 0;
    u8 *str;

    for (i = 0; i < 4 && (legend = gRogueLegendRows[row][i]) != RLEG_NONE; i++)
    {
        size++;
        bits |= 1u << legend;
        here += (screen->regionLegends >> legend) & 1;
    }
    str = StringCopy(gStringVar4, Rogue_GetLegendRowName(row, TRUE));
    if (row < ROGUE_LEGEND_SETS)
    {
        *str++ = CHAR_SPACE;
        *str++ = CHAR_SPACE;
        str = ConvertIntToDecimalStringN(str, Rogue_CountLegends(record->legends & bits), STR_CONV_MODE_LEFT_ALIGN, 1);
        *str++ = CHAR_SLASH;
        ConvertIntToDecimalStringN(str, size, STR_CONV_MODE_LEFT_ALIGN, 1);
    }
    AddTextPrinterParameterized3(UWIN_INFO, FONT_NORMAL, 2, 1, sTextColor_Row, TEXT_SKIP_DRAW, gStringVar4);
    if (here)
    {
        str = ConvertIntToDecimalStringN(gStringVar4, here, STR_CONV_MODE_LEFT_ALIGN, 1);
        StringCopy(str, sText_InRegion);
        AddTextPrinterParameterized3(UWIN_INFO, FONT_SMALL, GetStringRightAlignXOffset(FONT_SMALL, gStringVar4, 28 * 8) - 2, 3,
                                     sTextColor_On, TEXT_SKIP_DRAW, gStringVar4);
    }
    for (i = 0; i < size; i++)
    {
        const u8 *name, *colors;
        legend = gRogueLegendRows[row][i];
        name = ((record->legendsSeen | record->legends) >> legend) & 1 ? GetSpeciesName(Rogue_LegendSpecies(legend)) : sText_UnseenName;
        // Caught in green, this run's in blue.
        if ((record->legends >> legend) & 1)
            colors = sTextColor_Done;
        else if ((screen->regionLegends >> legend) & 1)
            colors = sTextColor_On;
        else
            colors = sTextColor_Row;
        AddTextPrinterParameterized3(UWIN_INFO, FONT_SMALL, x, 18, colors, TEXT_SKIP_DRAW, name);
        x += GetStringWidth(FONT_SMALL, name, 0) + 8;
    }
}

static void PrintUnlockInfo(struct UnlocksScreen *screen)
{
    u32 unlock = sUnlockOrder[screen->cursor];
    u8 *str = gStringVar4;

    FillWindowPixelBuffer(UWIN_INFO, PIXEL_FILL(1));
    if (screen->tab == TAB_FEATS)
    {
        PrintFeatInfo(screen->cursor);
    }
    else if (screen->tab == TAB_LEGENDS)
    {
        PrintLegendInfo(screen, screen->cursor);
    }
    else if (unlock == UNLOCK_NATIONAL_DEX && Rogue_IsUnlockEarned(unlock))
    {
        // The choice, and how to change it or earn the last one.
        PrintInfoLine(Rogue_GetDexChoiceDescription(Rogue_GetDexChoice()), 1, sTextColor_Row);
        if (!Rogue_IsUnlockEarned(UNLOCK_GLOBAL_DEX))
            PrintInfoLine(Rogue_GetDexChoice() == ROGUE_MIX_CLASSIC ? sText_DexSwitch2 : sText_DexGlobalHint, 17, sTextColor_RowDim);
        else
            PrintInfoLine(sText_DexSwitch, 17, sTextColor_RowDim);
    }
    else
    {
        if (Rogue_IsUnlockEarned(unlock))
        {
            StringCopy(str, Rogue_GetUnlockDescription(unlock));
        }
        else
        {
            str = StringCopy(str, sText_ToUnlock);
            StringCopy(str, Rogue_GetUnlockHint(unlock));
        }
        AddTextPrinterParameterized(UWIN_INFO, FONT_NORMAL, gStringVar4, 2, 1, TEXT_SKIP_DRAW, NULL);
    }
    PutWindowTilemap(UWIN_INFO);
    CopyWindowToVram(UWIN_INFO, COPYWIN_GFX);
}

static void AddUnlocksArrows(struct UnlocksScreen *screen)
{
    screen->arrowTask = AddScrollIndicatorArrowPairParameterized(SCROLL_ARROW_UP, 120, 20, 108,
                                                                 TabRows(screen->tab) - TabRowsInView(screen->tab),
                                                                 TAG_UNLOCK_ARROWS, TAG_UNLOCK_ARROWS,
                                                                 &screen->scroll);
}

static void ShowUnlocksTab(struct UnlocksScreen *screen, u32 tab)
{
    if (screen->tab == TAB_LEGENDS)
        DestroyLegendIcons(screen);
    RemoveScrollIndicatorArrowPair(screen->arrowTask);
    screen->tab = tab;
    screen->cursor = 0;
    screen->scroll = 0;
    if (tab == TAB_LEGENDS)
    {
        // The run to continue, if there is one: its legends are marked.
        screen->regionLegends = VarGet(VAR_ROGUE_RUN_STATE) == ROGUE_RUN_ACTIVE ? Rogue_GetRecord()->runLegends : 0;
        CreateLegendIcons(screen);
    }
    AddUnlocksArrows(screen);
    PrintUnlocksHeader(screen);
    PrintUnlocksList(screen);
    PrintUnlockInfo(screen);
}

static void Task_UnlocksExit(u8 taskId)
{
    struct UnlocksScreen *screen = GetUnlocksScreen(taskId);

    if (gPaletteFade.active)
        return;
    RemoveScrollIndicatorArrowPair(screen->arrowTask);
    if (screen->tab == TAB_LEGENDS)
        DestroyLegendIcons(screen);
    // The record is part of the save, so a change is kept by saving now.
    if (screen->changed)
        TrySavingData(SAVE_NORMAL);
    Free(screen);
    FreeAllWindowBuffers();
    DestroyTask(taskId);
    SetMainCallback2(CB2_ReinitMainMenu);
}

static void Task_UnlocksInput(u8 taskId)
{
    struct UnlocksScreen *screen = GetUnlocksScreen(taskId);
    u32 unlock = sUnlockOrder[screen->cursor];
    u32 rows = TabRows(screen->tab), inView = TabRowsInView(screen->tab);
    bool32 moved = FALSE;

    if (gPaletteFade.active)
        return;
    if (JOY_REPEAT(DPAD_UP) && screen->cursor > 0)
    {
        screen->cursor--;
        moved = TRUE;
    }
    else if (JOY_REPEAT(DPAD_DOWN) && screen->cursor + 1 < rows)
    {
        screen->cursor++;
        moved = TRUE;
    }
    else if (JOY_NEW(L_BUTTON) || JOY_REPEAT(DPAD_LEFT))
    {
        PlaySE(SE_SELECT);
        ShowUnlocksTab(screen, (screen->tab + TAB_COUNT - 1) % TAB_COUNT);
        return;
    }
    else if (JOY_NEW(R_BUTTON) || JOY_REPEAT(DPAD_RIGHT))
    {
        PlaySE(SE_SELECT);
        ShowUnlocksTab(screen, (screen->tab + 1) % TAB_COUNT);
        return;
    }
    else if (JOY_NEW(A_BUTTON) && screen->tab == TAB_UNLOCKS && Rogue_IsUnlockEarned(unlock))
    {
        PlaySE(SE_SELECT);
        if (unlock == UNLOCK_NATIONAL_DEX)
            Rogue_SetDexChoice(Rogue_NextDexChoice(Rogue_GetDexChoice()));
        else
            Rogue_SetUnlockEnabled(unlock, !Rogue_IsUnlockOn(unlock));
        screen->changed = TRUE;
        PrintUnlocksList(screen);
        if (unlock == UNLOCK_NATIONAL_DEX)
            PrintUnlockInfo(screen);
        return;
    }
    else if (JOY_NEW(B_BUTTON | START_BUTTON))
    {
        PlaySE(SE_SELECT);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_UnlocksExit;
        return;
    }
    if (moved)
    {
        PlaySE(SE_SELECT);
        if (screen->cursor < screen->scroll)
            screen->scroll = screen->cursor;
        else if (screen->cursor >= screen->scroll + inView)
            screen->scroll = screen->cursor - inView + 1;
        PrintUnlocksList(screen);
        PrintUnlockInfo(screen);
    }
}

void CB2_RogueUnlocks(void)
{
    struct UnlocksScreen *screen;
    u32 x, y;

    InitScreen(sSummaryBgTemplates, ARRAY_COUNT(sSummaryBgTemplates), sUnlockWindows);
    CpuCopy16(sSummaryTiles, (void *)BG_CHAR_ADDR(TILE_CHAR_BASE), sizeof(sSummaryTiles));
    LoadPalette(sSummaryPalette, BG_PLTT_ID(0), PLTT_SIZE_4BPP);
    for (y = 0; y < 32; y++)
        for (x = 0; x < 32; x++)
            PutTile(BG_LAND, x, y, SUMMARY_TILE_DARK);
    screen = AllocZeroed(sizeof(*screen));
    DrawStdFrameWithCustomTileAndPalette(UWIN_LIST, FALSE, 2, 14);
    DrawStdFrameWithCustomTileAndPalette(UWIN_INFO, FALSE, 2, 14);
    PrintUnlocksHeader(screen);
    PrintUnlocksList(screen);
    PrintUnlockInfo(screen);
    AddUnlocksArrows(screen);
    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
    SetVBlankCallback(VBlankCB);
    EnableInterrupts(INTR_FLAG_VBLANK);
    SetWordTaskArg(CreateTask(Task_UnlocksInput, 0), tScreen, (u32)screen);
    SetMainCallback2(CB2_RogueScreenMain);
}

// ---------------------------------------------------------------------------
// RUN 0's start (src/main_menu.c's RUN 0 row): two seconds of black with no
// sound, then the Director's line in the game's own type with no name box,
// after a loss the offer of the act reached, then the run (CB2_NewGame). The
// story may rewrite the lines; they live here.

#define RUN0_SILENCE_FRAMES 120

static const u8 sText_Run0Welcome[] = _("Welcome back. I made this one\nbefore all the others.");
static const u8 sText_Run0Offer[] = _("Last time you reached the {STR_VAR_1}\npart. I can start you there.");
static const u8 sText_Run0JustSayYes[] = _("Just say yes.");
static const u8 *const sText_Run0Parts[] = { NULL, NULL, COMPOUND_STRING("second"), COMPOUND_STRING("third"), COMPOUND_STRING("fourth") };
static const u8 sRun0TextColors[] = { TEXT_COLOR_TRANSPARENT, TEXT_COLOR_WHITE, TEXT_COLOR_DARK_GRAY };

enum { RUN0WIN_TEXT };
static const struct WindowTemplate sRun0Windows[] =
{
    [RUN0WIN_TEXT] = { .bg = 0, .tilemapLeft = 2, .tilemapTop = 14, .width = 27, .height = 4, .paletteNum = 15, .baseBlock = 0x20 },
    DUMMY_WIN_TEMPLATE
};
static const struct WindowTemplate sRun0YesNo = { .bg = 0, .tilemapLeft = 23, .tilemapTop = 8, .width = 5, .height = 4, .paletteNum = 15, .baseBlock = 0x100 };
static const struct BgTemplate sRun0Bgs[] =
{
    { .bg = 0, .charBaseIndex = 0, .mapBaseIndex = 31, .priority = 0 },
};

#define tState data[0]
#define tTimer data[1]
#define tAct   data[2]

static void Run0Say(const u8 *text)
{
    FillWindowPixelBuffer(RUN0WIN_TEXT, PIXEL_FILL(0));
    StringExpandPlaceholders(gStringVar4, text);
    gTextFlags.canABSpeedUpPrint = TRUE;
    AddTextPrinterParameterized4(RUN0WIN_TEXT, FONT_NORMAL, 0, 1, 0, 0, sRun0TextColors, GetPlayerTextSpeedDelay(), gStringVar4);
    PutWindowTilemap(RUN0WIN_TEXT);
    CopyWindowToVram(RUN0WIN_TEXT, COPYWIN_FULL);
}

static void Task_Run0Intro(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    struct RogueDirector *d;

    if (IsTextPrinterActiveOnWindow(RUN0WIN_TEXT))
        return;
    switch (tState)
    {
    case 0: // black and silent
        if (++tTimer < RUN0_SILENCE_FRAMES)
            return;
        Run0Say(sText_Run0Welcome);
        tState++;
        break;
    case 1:
        if (!JOY_NEW(A_BUTTON | B_BUTTON))
            return;
        // After a loss: the act the last attempt reached.
        d = Rogue_Director();
        tAct = 0;
        if (d != NULL && d->offerCheckpoint && d->checkpointAct >= 2 && d->checkpointAct <= ROGUE_RUN0_ACTS)
            tAct = d->checkpointAct;
        Rogue_ReleaseDirector();
        if (tAct == 0)
        {
            tState = 5;
            break;
        }
        StringCopy(gStringVar1, sText_Run0Parts[tAct]);
        Run0Say(sText_Run0Offer);
        tState++;
        break;
    case 2:
        if (!JOY_NEW(A_BUTTON | B_BUTTON))
            return;
        Run0Say(sText_Run0JustSayYes);
        tState = 7;
        break;
    case 7:
        // Only once the line is printed: the YES/NO menu's own text colors
        // would color the rest of it (it was dark on a white bar).
        CreateYesNoMenu(&sRun0YesNo, 2, 14, 0);
        tState = 3;
        break;
    case 3:
        switch (Menu_ProcessInputNoWrapClearOnChoose())
        {
        case 0: // YES
            break;
        case 1:
        case MENU_B_PRESSED:
            tAct = 1;
            break;
        default:
            return;
        }
        d = Rogue_Director();
        if (d != NULL)
        {
            d->startAct = tAct;
            Rogue_SaveDirector();
        }
        tState = 5;
        break;
    case 5:
        FillWindowPixelBuffer(RUN0WIN_TEXT, PIXEL_FILL(0));
        CopyWindowToVram(RUN0WIN_TEXT, COPYWIN_FULL);
        tTimer = 0;
        tState++;
        break;
    case 6:
        if (++tTimer < 30)
            return;
        FreeAllWindowBuffers();
        DestroyTask(taskId);
        SetMainCallback2(CB2_NewGame);
        break;
    }
}

#undef tState
#undef tTimer
#undef tAct

void CB2_RogueRun0Intro(void)
{
    m4aMPlayAllStop();
    InitScreen(sRun0Bgs, ARRAY_COUNT(sRun0Bgs), sRun0Windows);
    SetVBlankCallback(VBlankCB);
    EnableInterrupts(INTR_FLAG_VBLANK);
    CreateTask(Task_Run0Intro, 0);
    SetMainCallback2(CB2_RogueScreenMain);
}
