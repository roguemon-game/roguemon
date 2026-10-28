#include "global.h"
#include "battle.h"
#include "config/quickstart.h"
#include "quickstart.h"
#include "title_screen.h"
#include "rogue_run0_story.h"
#include "sprite.h"
#include "gba/m4a_internal.h"
#include "clear_save_data_menu.h"
#include "decompress.h"
#include "event_data.h"
#include "intro.h"
#include "m4a.h"
#include "main.h"
#include "main_menu.h"
#include "palette.h"
#include "reset_rtc_screen.h"
#include "berry_fix_program.h"
#include "sound.h"
#include "sprite.h"
#include "task.h"
#include "scanline_effect.h"
#include "gpu_regs.h"
#include "trig.h"
#include "graphics.h"
#include "malloc.h"
#include "pokemon.h"
#include "random.h"
#include "constants/rgb.h"
#include "constants/rogue.h"
#include "constants/songs.h"

enum {
    TAG_PRESS_START_COPYRIGHT = 1001,
    TAG_LOGO_SHINE,
    TAG_WINGULL,
    TAG_WINNER,
    TAG_WINNER_SHADOW,
};

// roguemon: the logo, PRESS START and the version line share one column
// centred at x 93, left of the hero on the cliff. The logo has clear space
// above it and to its left, like Emerald's. BG2 is affine: make_title.py puts
// the logo at the top of its layer, centred across it (x 128), and these
// scroll that layer so the logo's top is at LOGO_Y and its centre at x 93.
#define LOGO_X 35
#define LOGO_Y 16
#define LOGO_HEIGHT 52 // make_title.py checks this
#define LOGO_BG2X (LOGO_X * 256)
#define LOGO_BG2Y ((u32)(-LOGO_Y * 256))
// The shine passes along the logo's middle.
#define LOGO_SHINE_Y (LOGO_Y + LOGO_HEIGHT / 2)
// The banner's frames start 8 px into its art, so the banner sits 8 px right
// of the column. The version line sits where Emerald's copyright does.
#define START_BANNER_X 101
#define START_BANNER_Y 116
#define VERSION_LINE_Y 148

// The valley mist is BG1, added on top of the vista. Its tiles go past the
// logo's tiles and tilemap; tools/roguemon/make_title.py uses the same numbers.
#define MIST_FIRST_TILE 96
#define MIST_TOP 84
#define MIST_DRIFT_FRAMES 4 // it drifts one pixel left every 4 frames

// The lead of the latest win stands with its feet here, left of the hero.
// WINNER_FEET in make_title.py matches, as it bakes the shadow for this spot.
#define WINNER_FEET_X 167
#define WINNER_FEET_Y 105
// An affine sprite shrinks it to 3/4 size, so it is clearly smaller than the hero.
#define WINNER_SCALE 192
// Feet this wide or more (after shrinking) get the wide shadow.
#define WINNER_WIDE_FEET 20

// A small flock of Wingull crosses the valley now and then.
#define FLOCK_FIRST_DELAY 300
#define FLOCK_DELAY_MIN   (12 * 60)
#define FLOCK_DELAY_RANGE (8 * 60)

#define CLEAR_SAVE_BUTTON_COMBO (B_BUTTON | SELECT_BUTTON | DPAD_UP)
#define RESET_RTC_BUTTON_COMBO (B_BUTTON | SELECT_BUTTON | DPAD_LEFT)
#define BERRY_UPDATE_BUTTON_COMBO (B_BUTTON | SELECT_BUTTON)
#define A_B_START_SELECT (A_BUTTON | B_BUTTON | START_BUTTON | SELECT_BUTTON)

static void MainCB2(void);
static void Task_TitleScreenPhase1(u8);
static void Task_TitleScreenPhase2(u8);
static void Task_TitleScreenPhase3(u8);
static void CB2_GoToMainMenu(void);
static void CB2_GoToClearSaveDataScreen(void);
static void CB2_GoToResetRtcScreen(void);
static void CB2_GoToBerryFixScreen(void);
static void CB2_GoToCopyrightScreen(void);

static void Task_TitleScenery(u8);
static void SpriteCB_PressStartCopyrightBanner(struct Sprite *sprite);
static void SpriteCB_PokemonLogoShine(struct Sprite *sprite);

// const rom data
static const u16 sUnusedUnknownPal[] = INCGFX_U16("graphics/title_screen/unused.pal", ".gbapal");

// The region vista behind the logo. Built by tools/roguemon/make_title.py.
static const u32 sTitleScreenRayquazaGfx[] = INCGFX_U32("graphics/rogue/title/backdrop.png", ".4bpp.smol");
static const u32 sTitleScreenRayquazaTilemap[] = INCGFX_U32("graphics/rogue/title/backdrop.bin", ".smolTM");
static const u32 sTitleScreenLogoShineGfx[] = INCGFX_U32("graphics/title_screen/logo_shine.png", ".4bpp.smol");
static const u32 sTitleMistGfx[] = INCGFX_U32("graphics/rogue/title/mist.png", ".4bpp.smol");
static const u32 sTitleMistTilemap[] = INCGFX_U32("graphics/rogue/title/mist.bin", ".smolTM");
static const u32 sTitleWingullGfx[] = INCGFX_U32("graphics/rogue/title/wingull.png", ".4bpp", "-mwidth 2 -mheight 2");
static const u16 sTitleWingullPal[] = INCGFX_U16("graphics/rogue/title/wingull.png", ".gbapal");
static const u32 sTitleShadowGfx[] = INCGFX_U32("graphics/rogue/title/shadow.png", ".4bpp", "-mwidth 4 -mheight 1");
static const u16 sTitleShadowPal[] = INCGFX_U16("graphics/rogue/title/shadow.png", ".gbapal");

// The edge of the cliff, as (line, x) points from the top of the mist down.
// The mist is cut off there, so it stays in the valley.
static const u8 sCliffEdge[][2] =
{
    {MIST_TOP, 172}, {96, 170}, {98, 156}, {102, 152}, {138, 150}, {144, 140}, {150, 132}, {DISPLAY_HEIGHT, 132},
};

// Used to blend "Emerald Version" as it passes over over the Pokémon banner.
// Also used by the intro to blend the Game Freak name/logo in and out as they appear and disappear
const u16 gTitleScreenAlphaBlend[64] =
{
    BLDALPHA_BLEND(16, 0),
    BLDALPHA_BLEND(16, 1),
    BLDALPHA_BLEND(16, 2),
    BLDALPHA_BLEND(16, 3),
    BLDALPHA_BLEND(16, 4),
    BLDALPHA_BLEND(16, 5),
    BLDALPHA_BLEND(16, 6),
    BLDALPHA_BLEND(16, 7),
    BLDALPHA_BLEND(16, 8),
    BLDALPHA_BLEND(16, 9),
    BLDALPHA_BLEND(16, 10),
    BLDALPHA_BLEND(16, 11),
    BLDALPHA_BLEND(16, 12),
    BLDALPHA_BLEND(16, 13),
    BLDALPHA_BLEND(16, 14),
    BLDALPHA_BLEND(16, 15),
    BLDALPHA_BLEND(15, 16),
    BLDALPHA_BLEND(14, 16),
    BLDALPHA_BLEND(13, 16),
    BLDALPHA_BLEND(12, 16),
    BLDALPHA_BLEND(11, 16),
    BLDALPHA_BLEND(10, 16),
    BLDALPHA_BLEND(9, 16),
    BLDALPHA_BLEND(8, 16),
    BLDALPHA_BLEND(7, 16),
    BLDALPHA_BLEND(6, 16),
    BLDALPHA_BLEND(5, 16),
    BLDALPHA_BLEND(4, 16),
    BLDALPHA_BLEND(3, 16),
    BLDALPHA_BLEND(2, 16),
    BLDALPHA_BLEND(1, 16),
    BLDALPHA_BLEND(0, 16),
    [32 ... 63] = BLDALPHA_BLEND(0, 16)
};

static const struct OamData sOamData_CopyrightBanner =
{
    .y = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(32x8),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(32x8),
    .tileNum = 0,
    .priority = 0,
    .paletteNum = 0,
    .affineParam = 0,
};

static const union AnimCmd sAnim_PressStart_0[] =
{
    ANIMCMD_FRAME(1, 4),
    ANIMCMD_END,
};
static const union AnimCmd sAnim_PressStart_1[] =
{
    ANIMCMD_FRAME(5, 4),
    ANIMCMD_END,
};
static const union AnimCmd sAnim_PressStart_2[] =
{
    ANIMCMD_FRAME(9, 4),
    ANIMCMD_END,
};
static const union AnimCmd sAnim_PressStart_3[] =
{
    ANIMCMD_FRAME(13, 4),
    ANIMCMD_END,
};
static const union AnimCmd sAnim_PressStart_4[] =
{
    ANIMCMD_FRAME(17, 4),
    ANIMCMD_END,
};
static const union AnimCmd sAnim_Copyright_0[] =
{
    ANIMCMD_FRAME(21, 4),
    ANIMCMD_END,
};
static const union AnimCmd sAnim_Copyright_1[] =
{
    ANIMCMD_FRAME(25, 4),
    ANIMCMD_END,
};
static const union AnimCmd sAnim_Copyright_2[] =
{
    ANIMCMD_FRAME(29, 4),
    ANIMCMD_END,
};
static const union AnimCmd sAnim_Copyright_3[] =
{
    ANIMCMD_FRAME(33, 4),
    ANIMCMD_END,
};
static const union AnimCmd sAnim_Copyright_4[] =
{
    ANIMCMD_FRAME(37, 4),
    ANIMCMD_END,
};

// The "Press Start" and copyright graphics are each 5 32x8 segments long
#define NUM_PRESS_START_FRAMES 5
#define NUM_COPYRIGHT_FRAMES 5

static const union AnimCmd *const sStartCopyrightBannerAnimTable[NUM_PRESS_START_FRAMES + NUM_COPYRIGHT_FRAMES] =
{
    sAnim_PressStart_0,
    sAnim_PressStart_1,
    sAnim_PressStart_2,
    sAnim_PressStart_3,
    sAnim_PressStart_4,
    [NUM_PRESS_START_FRAMES] =
    sAnim_Copyright_0,
    sAnim_Copyright_1,
    sAnim_Copyright_2,
    sAnim_Copyright_3,
    sAnim_Copyright_4,
};

static const struct SpriteTemplate sStartCopyrightBannerSpriteTemplate =
{
    .tileTag = TAG_PRESS_START_COPYRIGHT,
    .paletteTag = TAG_PRESS_START_COPYRIGHT,
    .oam = &sOamData_CopyrightBanner,
    .anims = sStartCopyrightBannerAnimTable,
    .callback = SpriteCB_PressStartCopyrightBanner,
};

static const struct CompressedSpriteSheet sSpriteSheet_PressStart[] =
{
    {
        .data = gTitleScreenPressStartGfx,
        .size = 0x520,
        .tag = TAG_PRESS_START_COPYRIGHT
    },
    {},
};

static const struct SpritePalette sSpritePalette_PressStart[] =
{
    {
        .data = gTitleScreenPressStartPal,
        .tag = TAG_PRESS_START_COPYRIGHT
    },
    {},
};

static const struct OamData sPokemonLogoShineOamData =
{
    .y = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(64x64),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(64x64),
    .tileNum = 0,
    .priority = 0,
    .paletteNum = 0,
    .affineParam = 0,
};

static const union AnimCmd sPokemonLogoShineAnimSequence[] =
{
    ANIMCMD_FRAME(0, 4),
    ANIMCMD_END,
};

static const union AnimCmd *const sPokemonLogoShineAnimTable[] =
{
    sPokemonLogoShineAnimSequence,
};

static const struct SpriteTemplate sPokemonLogoShineSpriteTemplate =
{
    .tileTag = TAG_LOGO_SHINE,
    .paletteTag = TAG_PRESS_START_COPYRIGHT,
    .oam = &sPokemonLogoShineOamData,
    .anims = sPokemonLogoShineAnimTable,
    .callback = SpriteCB_PokemonLogoShine,
};

static const struct CompressedSpriteSheet sPokemonLogoShineSpriteSheet[] =
{
    {
        .data = sTitleScreenLogoShineGfx,
        .size = 0x800,
        .tag = TAG_LOGO_SHINE
    },
    {},
};

// Emerald's small overworld Wingull, flying left with its side frames.
static const struct OamData sOamData_Wingull =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(16x16),
    .size = SPRITE_SIZE(16x16),
    .priority = 2,
};

static const union AnimCmd sAnim_Wingull_0[] =
{
    ANIMCMD_FRAME(16, 8),
    ANIMCMD_FRAME(20, 8),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd sAnim_Wingull_1[] =
{
    ANIMCMD_FRAME(20, 8),
    ANIMCMD_FRAME(16, 8),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd *const sAnims_Wingull[] =
{
    sAnim_Wingull_0,
    sAnim_Wingull_1,
};

static void SpriteCB_Wingull(struct Sprite *sprite);

static const struct SpriteTemplate sWingullSpriteTemplate =
{
    .tileTag = TAG_WINGULL,
    .paletteTag = TAG_WINGULL,
    .oam = &sOamData_Wingull,
    .anims = sAnims_Wingull,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_Wingull,
};

// Each bird's place in the flock (x, y) and when its bob starts.
static const s8 sWingullFlock[][3] =
{
    { 0,  0,  0},
    {14, -7,  5},
    {16,  8, 11},
};

// The winner: a front sprite shrunk by an affine sprite. Frame 0 stands
// still; anim 1 shows the second front frame for a moment.
static const struct OamData sOamData_Winner =
{
    .affineMode = ST_OAM_AFFINE_NORMAL,
    .objMode = ST_OAM_OBJ_NORMAL,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(64x64),
    .size = SPRITE_SIZE(64x64),
    .priority = 2,
};

static const union AnimCmd sAnim_Winner_Still[] =
{
    ANIMCMD_FRAME(0, 1),
    ANIMCMD_END,
};

static const union AnimCmd sAnim_Winner_Move[] =
{
    ANIMCMD_FRAME(64, 24),
    ANIMCMD_FRAME(0, 1),
    ANIMCMD_END,
};

static const union AnimCmd *const sAnims_Winner[] =
{
    sAnim_Winner_Still,
    sAnim_Winner_Move,
};

static const union AffineAnimCmd sAffineAnim_Winner[] =
{
    AFFINEANIMCMD_FRAME(WINNER_SCALE, WINNER_SCALE, 0, 0),
    AFFINEANIMCMD_END,
};

static const union AffineAnimCmd *const sAffineAnims_Winner[] =
{
    sAffineAnim_Winner,
};

static const struct SpriteTemplate sWinnerSpriteTemplate =
{
    .tileTag = TAG_WINNER,
    .paletteTag = TAG_WINNER,
    .oam = &sOamData_Winner,
    .anims = sAnims_Winner,
    .images = NULL,
    .affineAnims = sAffineAnims_Winner,
    .callback = SpriteCallbackDummy,
};

// The shadow under the winner's feet: frame 0 wide, frame 1 narrow.
static const struct OamData sOamData_WinnerShadow =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(32x8),
    .size = SPRITE_SIZE(32x8),
    .priority = 2,
};

static const union AnimCmd sAnim_WinnerShadow_Wide[] =
{
    ANIMCMD_FRAME(0, 1),
    ANIMCMD_END,
};

static const union AnimCmd sAnim_WinnerShadow_Narrow[] =
{
    ANIMCMD_FRAME(4, 1),
    ANIMCMD_END,
};

static const union AnimCmd *const sAnims_WinnerShadow[] =
{
    sAnim_WinnerShadow_Wide,
    sAnim_WinnerShadow_Narrow,
};

static const struct SpriteTemplate sWinnerShadowSpriteTemplate =
{
    .tileTag = TAG_WINNER_SHADOW,
    .paletteTag = TAG_WINNER_SHADOW,
    .oam = &sOamData_WinnerShadow,
    .anims = sAnims_WinnerShadow,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sTitleSceneSpriteSheets[] =
{
    {sTitleWingullGfx, sizeof(sTitleWingullGfx), TAG_WINGULL},
    {sTitleShadowGfx, sizeof(sTitleShadowGfx), TAG_WINNER_SHADOW},
    {},
};

static const struct SpritePalette sTitleSceneSpritePalettes[] =
{
    {sTitleWingullPal, TAG_WINGULL},
    {sTitleShadowPal, TAG_WINNER_SHADOW},
    {},
};

// Task data for the main title screen tasks (Task_TitleScreenPhase#)
#define tCounter    data[0]
#define tSkipToNext data[1]
#define tPointless  data[2] // Incremented but never used to do anything.
#define tBg2Y       data[3]

// Sprite data for SpriteCB_PressStartCopyrightBanner
#define sAnimate data[0]
#define sTimer   data[1]

static void SpriteCB_PressStartCopyrightBanner(struct Sprite *sprite)
{
    if (sprite->sAnimate == TRUE)
    {
        // Alternate between hidden and shown every 16th frame
        if (++sprite->sTimer & 16)
            sprite->invisible = FALSE;
        else
            sprite->invisible = TRUE;
    }
    else
    {
        sprite->invisible = FALSE;
    }
}

static void CreatePressStartBanner(s16 x, s16 y)
{
    u8 i;
    u8 spriteId;

    x -= 64;
    for (i = 0; i < NUM_PRESS_START_FRAMES; i++, x += 32)
    {
        spriteId = CreateSprite(&sStartCopyrightBannerSpriteTemplate, x, y, 0);
        StartSpriteAnim(&gSprites[spriteId], i);
        gSprites[spriteId].sAnimate = TRUE;
    }
}

static void CreateCopyrightBanner(s16 x, s16 y)
{
    u8 i;
    u8 spriteId;

    x -= 64;
    for (i = 0; i < NUM_COPYRIGHT_FRAMES; i++, x += 32)
    {
        spriteId = CreateSprite(&sStartCopyrightBannerSpriteTemplate, x, y, 0);
        StartSpriteAnim(&gSprites[spriteId], i + NUM_PRESS_START_FRAMES);
    }
}

#undef sAnimate
#undef sTimer

// Defines for SpriteCB_PokemonLogoShine
enum {
    SHINE_MODE_SINGLE_NO_BG_COLOR,
    SHINE_MODE_DOUBLE,
    SHINE_MODE_SINGLE,
};

#define SHINE_SPEED  4

#define sMode     data[0]
#define sBgColor  data[1]

static void SpriteCB_PokemonLogoShine(struct Sprite *sprite)
{
    if (sprite->x < DISPLAY_WIDTH + 32)
    {
        // In any mode except SHINE_MODE_SINGLE_NO_BG_COLOR the background
        // color will change, in addition to the shine sprite moving.
        if (sprite->sMode != SHINE_MODE_SINGLE_NO_BG_COLOR)
        {
            u16 backgroundColor;

            if (sprite->x < DISPLAY_WIDTH / 2)
            {
                // Brighten background color
                if (sprite->sBgColor < 31)
                    sprite->sBgColor++;
                if (sprite->sBgColor < 31)
                    sprite->sBgColor++;
            }
            else
            {
                // Darken background color
                if (sprite->sBgColor != 0)
                    sprite->sBgColor--;
                if (sprite->sBgColor != 0)
                    sprite->sBgColor--;
            }

            backgroundColor = _RGB(sprite->sBgColor, sprite->sBgColor, sprite->sBgColor);

            // Flash the background green for 4 frames of movement.
            // Otherwise use the updating color.
            if (sprite->x == DISPLAY_WIDTH / 2 + (3 * SHINE_SPEED)
             || sprite->x == DISPLAY_WIDTH / 2 + (4 * SHINE_SPEED)
             || sprite->x == DISPLAY_WIDTH / 2 + (5 * SHINE_SPEED)
             || sprite->x == DISPLAY_WIDTH / 2 + (6 * SHINE_SPEED))
                gPlttBufferFaded[0] = RGB(24, 31, 12);
            else
                gPlttBufferFaded[0] = backgroundColor;
        }

        sprite->x += SHINE_SPEED;
    }
    else
    {
        // Sprite has moved fully offscreen
        gPlttBufferFaded[0] = RGB_BLACK;
        DestroySprite(sprite);
    }
}

static void SpriteCB_PokemonLogoShine_Fast(struct Sprite *sprite)
{
    if (sprite->x < DISPLAY_WIDTH + 32)
        sprite->x += SHINE_SPEED * 2;
    else
        DestroySprite(sprite);
}

static void StartPokemonLogoShine(u8 mode)
{
    u8 spriteId;

    switch (mode)
    {
    case SHINE_MODE_SINGLE_NO_BG_COLOR:
    case SHINE_MODE_SINGLE:
        // Create one regular shine sprite.
        // If mode is SHINE_MODE_SINGLE it will also change the background color.
        spriteId = CreateSprite(&sPokemonLogoShineSpriteTemplate, 0, LOGO_SHINE_Y, 0);
        gSprites[spriteId].oam.objMode = ST_OAM_OBJ_WINDOW;
        gSprites[spriteId].sMode = mode;
        break;
    case SHINE_MODE_DOUBLE:
        // Create an invisible sprite with mode set to update the background color
        spriteId = CreateSprite(&sPokemonLogoShineSpriteTemplate, 0, LOGO_SHINE_Y, 0);
        gSprites[spriteId].oam.objMode = ST_OAM_OBJ_WINDOW;
        gSprites[spriteId].sMode = mode;
        gSprites[spriteId].invisible = TRUE;

        // Create two faster shine sprites
        spriteId = CreateSprite(&sPokemonLogoShineSpriteTemplate, 0, LOGO_SHINE_Y, 0);
        gSprites[spriteId].callback = SpriteCB_PokemonLogoShine_Fast;
        gSprites[spriteId].oam.objMode = ST_OAM_OBJ_WINDOW;

        spriteId = CreateSprite(&sPokemonLogoShineSpriteTemplate, -80, LOGO_SHINE_Y, 0);
        gSprites[spriteId].callback = SpriteCB_PokemonLogoShine_Fast;
        gSprites[spriteId].oam.objMode = ST_OAM_OBJ_WINDOW;
        break;
    }
}

#undef sMode
#undef sBgColor

// Task data for Task_TitleScenery
#define tMistX      data[0]
#define tMistTimer  data[1]
#define tFlockTimer data[2]
#define tIdleTimer  data[3]
#define tWinner     data[4] // sprite id, or MAX_SPRITES with no win yet
#define tSpecies    data[5]

// Sprite data for SpriteCB_Wingull
#define sTime  data[0]
#define sDx    data[1]
#define sDy    data[2]
#define sPhase data[3]

// Flies up from the cliff face across the valley, bobbing a little. It
// passes just below the winner's feet.
static void SpriteCB_Wingull(struct Sprite *sprite)
{
    s32 t = sprite->sTime++;

    sprite->x = DISPLAY_WIDTH + 12 + sprite->sDx - t * 6 / 5;
    sprite->y = 132 + sprite->sDy - t * 17 / 100 + Sin(((t + 4 * sprite->sPhase) * 4) & 0xFF, 2);
    if (sprite->x < -8)
        DestroySprite(sprite);
}

static void CreateWingullFlock(void)
{
    u32 i;

    for (i = 0; i < ARRAY_COUNT(sWingullFlock); i++)
    {
        u8 spriteId = CreateSprite(&sWingullSpriteTemplate, 0, 0, 0);
        if (spriteId == MAX_SPRITES)
            break;
        gSprites[spriteId].sDx = sWingullFlock[i][0];
        gSprites[spriteId].sDy = sWingullFlock[i][1];
        gSprites[spriteId].sPhase = sWingullFlock[i][2];
        StartSpriteAnim(&gSprites[spriteId], i & 1);
        SpriteCB_Wingull(&gSprites[spriteId]);
    }
}

#undef sTime
#undef sDx
#undef sDy
#undef sPhase

// Whether pixel (x, y) of a 4bpp Pokémon picture is drawn.
static bool32 IsPicPixelDrawn(const u8 *pic, u32 x, u32 y)
{
    u32 pair = pic[((y / 8) * 8 + x / 8) * TILE_SIZE_4BPP + (y % 8) * 4 + (x % 8) / 2];

    return ((x & 1) ? (pair >> 4) : (pair & 0xF)) != 0;
}

// After a win, the lead of that run stands on the cliff beside the hero: its
// front sprite, shrunk, with its feet on the ground and a soft shadow under
// them. Returns the sprite id, or MAX_SPRITES with no win yet.
static u8 CreateWinner(void)
{
    u32 value = VarGet(VAR_ROGUE_META_CHAMPION_MON);
    u32 species = value & ~ROGUE_CHAMPION_MON_SHINY;
    struct SpriteSheet sheet;
    struct SpritePalette palette;
    s32 x, y, left = MON_PIC_WIDTH, right = -1, bottom = -1;
    u8 *pic;
    u8 spriteId;

    if (species == SPECIES_NONE || species >= NUM_SPECIES || !IsSpeciesEnabled(species))
        return MAX_SPRITES;
    pic = Alloc(MON_PIC_SIZE * MAX_MON_PIC_FRAMES);
    if (pic == NULL)
        return MAX_SPRITES;
    // A personality of 0xFF picks the male look where the two differ.
    LoadSpecialPokePic(pic, species, 0xFF, TRUE);

    // The feet are the lowest drawn row of the first frame. Centre the
    // bottom few rows on the spot.
    for (y = MON_PIC_HEIGHT - 1; y >= 0 && bottom < 0; y--)
    {
        for (x = 0; x < MON_PIC_WIDTH; x++)
        {
            if (IsPicPixelDrawn(pic, x, y))
            {
                bottom = y;
                break;
            }
        }
    }
    if (bottom < 0)
    {
        Free(pic);
        return MAX_SPRITES;
    }
    for (y = max(0, bottom - 5); y <= bottom; y++)
    {
        for (x = 0; x < MON_PIC_WIDTH; x++)
        {
            if (IsPicPixelDrawn(pic, x, y))
            {
                left = min(left, x);
                right = max(right, x);
            }
        }
    }

    sheet.data = pic;
    sheet.size = MON_PIC_SIZE * MAX_MON_PIC_FRAMES;
    sheet.tag = TAG_WINNER;
    LoadSpriteSheet(&sheet);
    Free(pic);
    palette.data = GetMonSpritePalFromSpeciesAndPersonality(species, (value & ROGUE_CHAMPION_MON_SHINY) != 0, 0xFF);
    palette.tag = TAG_WINNER;
    LoadSpritePalette(&palette);

    // The sprite shrinks about its centre, so scale the feet's offset from it.
    x = WINNER_FEET_X - ((left + right + 1) / 2 - MON_PIC_WIDTH / 2) * WINNER_SCALE / 256;
    y = WINNER_FEET_Y - (bottom + 1 - MON_PIC_HEIGHT / 2) * WINNER_SCALE / 256;
    spriteId = CreateSprite(&sWinnerSpriteTemplate, x, y, 1);
    if (spriteId == MAX_SPRITES)
        return MAX_SPRITES;
    x = CreateSprite(&sWinnerShadowSpriteTemplate, WINNER_FEET_X, WINNER_FEET_Y - 1, 3);
    if (x != MAX_SPRITES && (right - left + 1) * WINNER_SCALE / 256 < WINNER_WIDE_FEET)
        StartSpriteAnim(&gSprites[x], 1);
    return spriteId;
}

// Drifts the mist, sends the Wingull now and then, and lets the winner move.
static void Task_TitleScenery(u8 taskId)
{
    struct Task *task = &gTasks[taskId];

    if (++task->tMistTimer >= MIST_DRIFT_FRAMES)
    {
        task->tMistTimer = 0;
        task->tMistX = (task->tMistX + 1) & 0xFF;
        SetGpuReg(REG_OFFSET_BG1HOFS, task->tMistX);
    }
    if (--task->tFlockTimer <= 0)
    {
        CreateWingullFlock();
        task->tFlockTimer = FLOCK_DELAY_MIN + Random2() % FLOCK_DELAY_RANGE;
    }
    if (task->tWinner != MAX_SPRITES && --task->tIdleTimer <= 0)
    {
        StartSpriteAnim(&gSprites[task->tWinner], 1);
        task->tIdleTimer = 300 + Random2() % 240;
    }
}

static void CreateTitleScenery(void)
{
    u8 taskId = CreateTask(Task_TitleScenery, 1);

    gTasks[taskId].tFlockTimer = FLOCK_FIRST_DELAY;
    gTasks[taskId].tIdleTimer = 180;
    gTasks[taskId].tWinner = CreateWinner();
    gTasks[taskId].tSpecies = VarGet(VAR_ROGUE_META_CHAMPION_MON) & ~ROGUE_CHAMPION_MON_SHINY;
}

// The winner moves and cries as the player presses START.
static void PlayWinnerCry(void)
{
    u8 taskId = FindTaskIdByFunc(Task_TitleScenery);

    if (taskId == TASK_NONE || gTasks[taskId].tWinner == MAX_SPRITES)
        return;
    StartSpriteAnim(&gSprites[gTasks[taskId].tWinner], 1);
    PlayCry_NormalNoDucking(gTasks[taskId].tSpecies, 0, CRY_VOLUME_RS, CRY_PRIORITY_NORMAL);
}

// The mist stops at the cliff edge through window 0, whose right edge an
// HBlank DMA sets line by line.
static void InitMistEdge(void)
{
    u32 i, y;
    struct ScanlineEffectParams params = {&REG_WIN0H, SCANLINE_EFFECT_DMACNT_16BIT, 1};

    ScanlineEffect_Clear();
    for (y = 0; y < DISPLAY_HEIGHT; y++)
    {
        u32 x = sCliffEdge[0][1];
        for (i = 0; i + 1 < ARRAY_COUNT(sCliffEdge); i++)
        {
            if (y >= sCliffEdge[i][0] && y <= sCliffEdge[i + 1][0])
            {
                x = sCliffEdge[i][1] + (sCliffEdge[i + 1][1] - sCliffEdge[i][1]) * (s32)(y - sCliffEdge[i][0])
                                     / max(1, sCliffEdge[i + 1][0] - sCliffEdge[i][0]);
                break;
            }
        }
        gScanlineEffectRegBuffers[0][y] = WIN_RANGE(0, x);
        gScanlineEffectRegBuffers[1][y] = WIN_RANGE(0, x);
    }
    ScanlineEffect_SetParams(params);
}

// Stops the mist's HBlank DMA and clears the windows and blending before
// the next screen sets up its own.
static void StopTitleEffects(void)
{
    ScanlineEffect_Stop();
    SetGpuReg(REG_OFFSET_WIN0H, 0);
    SetGpuReg(REG_OFFSET_WIN0V, 0);
    SetGpuReg(REG_OFFSET_WININ, 0);
    SetGpuReg(REG_OFFSET_WINOUT, 0);
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
    SetGpuReg(REG_OFFSET_BLDALPHA, 0);
}

static void VBlankCB(void)
{
    ScanlineEffect_InitHBlankDmaTransfer();
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

void CB2_InitTitleScreen(void)
{
    if (IS_FRLG)
    {
        CB2_InitTitleScreenFrlg();
        return;
    }
    switch (gMain.state)
    {
    default:
    case 0:
        SetVBlankCallback(NULL);
        SetGpuReg(REG_OFFSET_BLDCNT, 0);
        SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        SetGpuReg(REG_OFFSET_BLDY, 0);
        *((u16 *)PLTT) = RGB_WHITE;
        SetGpuReg(REG_OFFSET_DISPCNT, 0);
        SetGpuReg(REG_OFFSET_BG2CNT, 0);
        SetGpuReg(REG_OFFSET_BG1CNT, 0);
        SetGpuReg(REG_OFFSET_BG0CNT, 0);
        SetGpuReg(REG_OFFSET_BG2HOFS, 0);
        SetGpuReg(REG_OFFSET_BG2VOFS, 0);
        SetGpuReg(REG_OFFSET_BG1HOFS, 0);
        SetGpuReg(REG_OFFSET_BG1VOFS, 0);
        SetGpuReg(REG_OFFSET_BG0HOFS, 0);
        SetGpuReg(REG_OFFSET_BG0VOFS, 0);
        DmaFill16(3, 0, (void *)VRAM, VRAM_SIZE);
        DmaFill32(3, 0, (void *)OAM, OAM_SIZE);
        DmaFill16(3, 0, (void *)(PLTT + 2), PLTT_SIZE - 2);
        ResetPaletteFade();
        gMain.state = 1;
        break;
    case 1:
        // bg2
        DecompressDataWithHeaderVram(gTitleScreenPokemonLogoGfx, (void *)(BG_CHAR_ADDR(0)));
        DecompressDataWithHeaderVram(gTitleScreenPokemonLogoTilemap, (void *)(BG_SCREEN_ADDR(9)));
        LoadPalette(gTitleScreenBgPalettes, BG_PLTT_ID(0), 15 * PLTT_SIZE_4BPP);
        // bg3
        DecompressDataWithHeaderVram(sTitleScreenRayquazaGfx, (void *)(BG_CHAR_ADDR(2)));
        DecompressDataWithHeaderVram(sTitleScreenRayquazaTilemap, (void *)(BG_SCREEN_ADDR(26)));
        // roguemon: vanilla's clouds are gone, so the vista's tiles may run on
        // into their space, up to the vista's tilemap at block 26. BG1 is the
        // valley mist instead, with its tiles in char base 1 past the logo's.
        DecompressDataWithHeaderVram(sTitleMistGfx, (void *)(BG_CHAR_ADDR(1) + MIST_FIRST_TILE * TILE_SIZE_4BPP));
        DecompressDataWithHeaderVram(sTitleMistTilemap, (void *)(BG_SCREEN_ADDR(27)));
        ScanlineEffect_Stop();
        InitMistEdge();
        ResetTasks();
        ResetSpriteData();
        FreeAllSpritePalettes();
        gReservedSpritePaletteCount = 9;
        LoadCompressedSpriteSheet(&sSpriteSheet_PressStart[0]);
        LoadCompressedSpriteSheet(&sPokemonLogoShineSpriteSheet[0]);
        LoadSpriteSheets(sTitleSceneSpriteSheets);
        LoadSpritePalette(&sSpritePalette_PressStart[0]);
        LoadSpritePalettes(sTitleSceneSpritePalettes);
        gMain.state = 2;
        break;
    case 2:
    {
        u8 taskId = CreateTask(Task_TitleScreenPhase1, 0);

        // The logo sits at the top from the start and the whole sequence
        // is short, so the player reaches PRESS START quickly.
        gTasks[taskId].tCounter = 80;
        gTasks[taskId].tSkipToNext = FALSE;
        gTasks[taskId].tPointless = -16;
        gTasks[taskId].tBg2Y = 0;
        CreateTitleScenery();
        gMain.state = 3;
        break;
    }
    case 3:
        BeginNormalPaletteFade(PALETTES_ALL, 1, 16, 0, RGB_WHITEALPHA);
        SetVBlankCallback(VBlankCB);
        gMain.state = 4;
        break;
    case 4:
        PanFadeAndZoomScreen(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, 0x100, 0);
        SetGpuReg(REG_OFFSET_BG2X_L, LOGO_BG2X & 0xFFFF);
        SetGpuReg(REG_OFFSET_BG2X_H, LOGO_BG2X >> 16);
        SetGpuReg(REG_OFFSET_BG2Y_L, LOGO_BG2Y & 0xFFFF);
        SetGpuReg(REG_OFFSET_BG2Y_H, LOGO_BG2Y >> 16);
        // roguemon: window 0 holds the mist (its right edge is set per line),
        // and the shine's OBJ window lights up the logo. Both use the same
        // blend, so it never has to change during a frame: the mist and the
        // logo are each added on top of the vista. The rest shows plainly.
        SetGpuReg(REG_OFFSET_WIN0V, WIN_RANGE(MIST_TOP, DISPLAY_HEIGHT));
        SetGpuReg(REG_OFFSET_WIN1H, 0);
        SetGpuReg(REG_OFFSET_WIN1V, 0);
        SetGpuReg(REG_OFFSET_WININ, WININ_WIN0_ALL);
        SetGpuReg(REG_OFFSET_WINOUT, WINOUT_WIN01_BG0 | WINOUT_WIN01_BG2 | WINOUT_WIN01_OBJ
                                   | WINOUT_WINOBJ_BG0 | WINOUT_WINOBJ_BG2 | WINOUT_WINOBJ_OBJ | WINOUT_WINOBJ_CLR);
        SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_BG1 | BLDCNT_TGT1_BG2 | BLDCNT_EFFECT_BLEND | BLDCNT_TGT2_BG0);
        SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(16, 16));
        SetGpuReg(REG_OFFSET_BLDY, 0);
        SetGpuReg(REG_OFFSET_BG0CNT, BGCNT_PRIORITY(3) | BGCNT_CHARBASE(2) | BGCNT_SCREENBASE(26) | BGCNT_16COLOR | BGCNT_TXT256x256);
        SetGpuReg(REG_OFFSET_BG1CNT, BGCNT_PRIORITY(2) | BGCNT_CHARBASE(1) | BGCNT_SCREENBASE(27) | BGCNT_16COLOR | BGCNT_TXT256x256);
        SetGpuReg(REG_OFFSET_BG2CNT, BGCNT_PRIORITY(1) | BGCNT_CHARBASE(0) | BGCNT_SCREENBASE(9) | BGCNT_256COLOR | BGCNT_AFF256x256);
        EnableInterrupts(INTR_FLAG_VBLANK);
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_1
                                    | DISPCNT_OBJ_1D_MAP
                                    | DISPCNT_BG0_ON
                                    | DISPCNT_BG1_ON
                                    | DISPCNT_BG2_ON
                                    | DISPCNT_OBJ_ON
                                    | DISPCNT_WIN0_ON
                                    | DISPCNT_OBJWIN_ON);
        // Coming back from the main menu, the theme is still playing.
        m4aSongNumStartOrChange(MUS_ABANDONED_SHIP);
        gMain.state = 5;
        break;
    case 5:
        // Show the winner while the screen fades in.
        AnimateSprites();
        BuildOamBuffer();
        if (!UpdatePaletteFade())
        {
            StartPokemonLogoShine(SHINE_MODE_SINGLE_NO_BG_COLOR);
            SetMainCallback2(MainCB2);
        }
        break;
    }
}

static void MainCB2(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

// Shine the Pokémon logo two more times
static void Task_TitleScreenPhase1(u8 taskId)
{
    // Skip to next phase when A, B, Start, or Select is pressed
    if (JOY_NEW(A_B_START_SELECT) || gTasks[taskId].tSkipToNext)
    {
        gTasks[taskId].tSkipToNext = TRUE;
        gTasks[taskId].tCounter = 0;
    }

    if (gTasks[taskId].tCounter != 0)
    {
        u16 frameNum = gTasks[taskId].tCounter;
        if (frameNum == 176)
            StartPokemonLogoShine(SHINE_MODE_DOUBLE);
        else if (frameNum == 64)
            StartPokemonLogoShine(SHINE_MODE_SINGLE);

        gTasks[taskId].tCounter--;
    }
    else
    {
        // roguemon: no version banner, and the windows and blending stay
        // as they are for the mist.
        gTasks[taskId].tCounter = 40;
        gTasks[taskId].func = Task_TitleScreenPhase2;
    }
}

// Create "Press Start" and copyright banners, and slide Pokémon logo up
static void Task_TitleScreenPhase2(u8 taskId)
{
    u32 yPos;

    // Skip to next phase when A, B, Start, or Select is pressed
    if (JOY_NEW(A_B_START_SELECT) || gTasks[taskId].tSkipToNext)
    {
        gTasks[taskId].tSkipToNext = TRUE;
        gTasks[taskId].tCounter = 0;
    }

    if (gTasks[taskId].tCounter != 0)
    {
        gTasks[taskId].tCounter--;
    }
    else
    {
        gTasks[taskId].tSkipToNext = TRUE;
        CreatePressStartBanner(START_BANNER_X, START_BANNER_Y);
        CreateCopyrightBanner(START_BANNER_X, VERSION_LINE_Y);
        if (QUICKSTART && QUICKSTART_HUD)
            CreateQuickstartHud();
        gTasks[taskId].func = Task_TitleScreenPhase3;
    }

    if (!(gTasks[taskId].tCounter & 3) && gTasks[taskId].tPointless != 0)
        gTasks[taskId].tPointless++;
    if (!(gTasks[taskId].tCounter & 1) && gTasks[taskId].tBg2Y != 0)
        gTasks[taskId].tBg2Y++;

    // Slide Pokémon logo up
    yPos = gTasks[taskId].tBg2Y * 256 + LOGO_BG2Y;
    SetGpuReg(REG_OFFSET_BG2Y_L, yPos);
    SetGpuReg(REG_OFFSET_BG2Y_H, yPos / 0x10000);

    gTasks[taskId].data[5] = 15; // Unused
    gTasks[taskId].data[6] = 6;  // Unused
}

// Show Rayquaza silhouette and process main title screen input
static void Task_TitleScreenPhase3(u8 taskId)
{
    if (QUICKSTART && JOY_NEW(SELECT_BUTTON))
        Quickstart();

    if (JOY_NEW(A_BUTTON) || JOY_NEW(START_BUTTON))
    {
        // The theme plays on through the main menu.
        PlayWinnerCry();
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_WHITEALPHA);
        SetMainCallback2(CB2_GoToMainMenu);
    }
    // roguemon: RUN 0's fake boot only has its menu.
    else if (Rogue_Run0FakeBoot())
    {
    }
    else if (JOY_HELD(CLEAR_SAVE_BUTTON_COMBO) == CLEAR_SAVE_BUTTON_COMBO)
    {
        SetMainCallback2(CB2_GoToClearSaveDataScreen);
    }
    else if (JOY_HELD(RESET_RTC_BUTTON_COMBO) == RESET_RTC_BUTTON_COMBO
      && CanResetRTC() == TRUE)
    {
        FadeOutBGM(4);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        SetMainCallback2(CB2_GoToResetRtcScreen);
    }
    else if (JOY_HELD(BERRY_UPDATE_BUTTON_COMBO) == BERRY_UPDATE_BUTTON_COMBO)
    {
        FadeOutBGM(4);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        SetMainCallback2(CB2_GoToBerryFixScreen);
    }
    else
    {
        SetGpuReg(REG_OFFSET_BG2Y_L, LOGO_BG2Y & 0xFFFF);
        SetGpuReg(REG_OFFSET_BG2Y_H, LOGO_BG2Y >> 16);
        if ((gMPlayInfo_BGM.status & 0xFFFF) == 0)
        {
            BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_WHITEALPHA);
            SetMainCallback2(CB2_GoToCopyrightScreen);
        }
    }
}

static void CB2_GoToMainMenu(void)
{
    if (!UpdatePaletteFade())
    {
        StopTitleEffects();
        SetMainCallback2(CB2_InitMainMenu);
    }
}

static void CB2_GoToCopyrightScreen(void)
{
    if (!UpdatePaletteFade())
    {
        StopTitleEffects();
        SetMainCallback2(CB2_InitCopyrightScreenAfterTitleScreen);
    }
}

static void CB2_GoToClearSaveDataScreen(void)
{
    if (!UpdatePaletteFade())
    {
        StopTitleEffects();
        SetMainCallback2(CB2_InitClearSaveDataScreen);
    }
}

static void CB2_GoToResetRtcScreen(void)
{
    if (!UpdatePaletteFade())
    {
        StopTitleEffects();
        SetMainCallback2(CB2_InitResetRtcScreen);
    }
}

static void CB2_GoToBerryFixScreen(void)
{
    if (!UpdatePaletteFade())
    {
        m4aMPlayAllStop();
        StopTitleEffects();
        SetMainCallback2(CB2_InitBerryFixProgram);
    }
}
