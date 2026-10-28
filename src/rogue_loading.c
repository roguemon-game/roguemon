#include "global.h"
#include "rogue_loading.h"
#include "bg.h"
#include "event_object_movement.h"
#include "field_player_avatar.h"
#include "gpu_regs.h"
#include "international_string_util.h"
#include "main.h"
#include "menu.h"
#include "palette.h"
#include "scanline_effect.h"
#include "sprite.h"
#include "string_util.h"
#include "text.h"
#include "window.h"
#include "constants/event_object_movement.h"
#include "constants/rgb.h"

// The loading screen (include/rogue_loading.h).
//
// The game makes a region or a map in one long call, so the main loop and
// its tasks, fades and text printers stand still meanwhile. Everything that
// moves here runs from the VBlank interrupt instead: the bar is drawn
// straight into its window's tiles in VRAM, and the player's walking sprite
// is animated and its OAM built there. The main thread only says how far
// the work has come (Rogue_LoadingStage) and prints the line of where the
// player is headed, whose tiles the interrupt's DMA queue carries over.

enum
{
    LWIN_TITLE, // the region's name and where the player is headed
    LWIN_BAR,
    LWIN_SAY,   // who speaks, and their line
    LWIN_COUNT,
};

#define BAR_LEFT   5
#define BAR_TOP    10
#define BAR_TILES  20
#define BAR_PX     (BAR_TILES * 8)
#define FADE_STEPS 8
#define MIN_FRAMES 40 // shown at least this long, so it reads as a screen

static const struct BgTemplate sLoadingBg =
{
    .bg = 0, .charBaseIndex = 0, .mapBaseIndex = 31, .priority = 0,
};

static const struct WindowTemplate sLoadingWindows[] =
{
    [LWIN_TITLE] = { .bg = 0, .tilemapLeft = 1, .tilemapTop = 3, .width = 28, .height = 4, .paletteNum = 15, .baseBlock = 1 },
    [LWIN_BAR]   = { .bg = 0, .tilemapLeft = BAR_LEFT, .tilemapTop = BAR_TOP, .width = BAR_TILES, .height = 1, .paletteNum = 15, .baseBlock = 1 + 28 * 4 },
    [LWIN_SAY]   = { .bg = 0, .tilemapLeft = 1, .tilemapTop = 13, .width = 28, .height = 6, .paletteNum = 15, .baseBlock = 1 + 28 * 4 + BAR_TILES },
    DUMMY_WIN_TEMPLATE,
};

static const u8 sColor_Title[] = { TEXT_COLOR_TRANSPARENT, TEXT_COLOR_WHITE, TEXT_COLOR_DARK_GRAY };
static const u8 sColor_Line[] = { TEXT_COLOR_TRANSPARENT, TEXT_COLOR_LIGHT_GRAY, TEXT_COLOR_DARK_GRAY };
static const u8 sColor_Who[] = { TEXT_COLOR_TRANSPARENT, TEXT_COLOR_LIGHT_BLUE, TEXT_COLOR_BLUE };

static const u8 sText_Director[] = _("THE DIRECTOR");
static const u8 sText_Birch[] = _("PROF. BIRCH");

// Lines of two rows of about 30 characters. The professor's for a run of
// his, the Director's for RUN 0 and ENDLESS.
static const u8 *const sBirchLines[] =
{
    COMPOUND_STRING("No one has walked these roads\nbefore you. Take your time!"),
    COMPOUND_STRING("Every route hides something.\nLook around as you go!"),
    COMPOUND_STRING("A rested team goes further.\nStop at every POKéMON CENTER!"),
    COMPOUND_STRING("Talk to the people you meet.\nThey know their own land."),
    COMPOUND_STRING("The wild POKéMON here are new\nto me too. Tell me about them!"),
    COMPOUND_STRING("If a road is blocked, look\nfor another way around."),
    COMPOUND_STRING("Pack plenty of POKé BALLS.\nYou never know who you'll meet!"),
    COMPOUND_STRING("Keep your partners close.\nThey're counting on you!"),
};

static const u8 *const sDirectorLines[] =
{
    COMPOUND_STRING("Building your region now.\nPlease do not touch anything."),
    COMPOUND_STRING("Every tree out there was\nplaced by me. Just now."),
    COMPOUND_STRING("Another region, made for you.\nTry to keep this one."),
    COMPOUND_STRING("I have made this land before.\nIt was different then."),
    COMPOUND_STRING("Hold still. I am almost done\nwith the gyms."),
    COMPOUND_STRING("Loading. You may blink."),
};

// Kept in IWRAM: EWRAM is full, and the interrupt reads it every frame.
static struct
{
    bool8 active;
    bool8 fadingOut;
    u8 spriteId;
    u8 fade;        // 0 (black) to FADE_STEPS (shown)
    u8 eta;         // frames the current stage should take
    u8 ceiling;     // percent the bar creeps toward
    u8 drawnPx;     // bar pixels drawn, 0xFF for none yet
    u16 shown;      // the bar, percent * 256
    u32 start;      // vblankCounter1 when shown
} sLoad;

// One 8x8 tile of the bar, 4bpp, in the menu palette's colors: a white
// outline, the fill (green, like an HP bar) up to px pixels from the tile's
// left and black after it.
static void DrawBarTile(u32 *dst, u32 tile, u32 px)
{
    u32 y, x;
    for (y = 0; y < 8; y++)
    {
        u32 row = 0;
        for (x = 0; x < 8; x++)
        {
            u32 c;
            if (y == 0 || y == 7 || (tile == 0 && x == 0) || (tile == BAR_TILES - 1 && x == 7))
                c = TEXT_COLOR_WHITE;
            else if (x < px)
                c = y == 6 ? TEXT_COLOR_GREEN : TEXT_COLOR_LIGHT_GREEN;
            else
                c = TEXT_COLOR_TRANSPARENT;
            row |= c << (x * 4);
        }
        dst[y] = row;
    }
}

// Draws the tiles of the bar from tile first to last with px pixels filled.
// (Only the tiles that changed: the interrupt's time is the generator's.)
static void DrawBar(u32 px, u32 first, u32 last)
{
    u32 *tiles = (u32 *)(BG_CHAR_ADDR(sLoadingBg.charBaseIndex) + sLoadingWindows[LWIN_BAR].baseBlock * TILE_SIZE_4BPP);
    u32 t;
    for (t = first; t <= last && t < BAR_TILES; t++)
    {
        u32 in = px > t * 8 ? px - t * 8 : 0;
        DrawBarTile(tiles + t * 8, t, in > 8 ? 8 : in);
    }
}

static void VBlankCB_Loading(void)
{
    u32 px;
    // The bar creeps toward the stage's ceiling, quickly at first.
    if (sLoad.shown < sLoad.ceiling * 256)
    {
        u32 step = (sLoad.ceiling * 256 - sLoad.shown) / (sLoad.eta ? sLoad.eta : 1);
        if (step < 16)
            step = 16;
        sLoad.shown = min(sLoad.shown + step, sLoad.ceiling * 256);
    }
    px = (sLoad.shown >> 8) * BAR_PX / 100;
    if (sLoad.drawnPx == 0xFF)
        DrawBar(px, 0, BAR_TILES - 1);
    else if (px != sLoad.drawnPx)
        DrawBar(px, min(px, sLoad.drawnPx) / 8, max(px, sLoad.drawnPx) / 8);
    sLoad.drawnPx = px;
    // The player walks along the bar's front.
    if (sLoad.spriteId != MAX_SPRITES)
        gSprites[sLoad.spriteId].x = BAR_LEFT * 8 + 4 + (px > BAR_PX - 8 ? BAR_PX - 8 : px);
    AnimateSprites();
    BuildOamBuffer();
    LoadOam();
    ProcessSpriteCopyRequests();
    // Fading in from black, or out to it.
    if (sLoad.fadingOut ? sLoad.fade > 0 : sLoad.fade < FADE_STEPS)
        sLoad.fade += sLoad.fadingOut ? -1 : 1;
    REG_BLDY = (FADE_STEPS - sLoad.fade) * 16 / FADE_STEPS;
}

void Rogue_LoadingShow(u8 kind, const u8 *title, u32 seed)
{
    const u8 *who, *line;
    u16 gfx;
    u32 i;

    SetVBlankCallback(NULL);
    SetHBlankCallback(NULL);
    ScanlineEffect_Stop();
    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    DmaFill16(3, 0, (void *)VRAM, VRAM_SIZE);
    DmaFill32(3, 0, (void *)OAM, OAM_SIZE);
    DmaFill16(3, 0, (void *)PLTT, PLTT_SIZE);
    ResetPaletteFade();
    ResetSpriteData();
    FreeAllSpritePalettes();
    FreeAllWindowBuffers();
    ResetBgsAndClearDma3BusyFlags(0);
    InitBgsFromTemplates(0, &sLoadingBg, 1);
    for (i = 0; i < 4; i++)
    {
        ChangeBgX(i, 0, BG_COORD_SET);
        ChangeBgY(i, 0, BG_COORD_SET);
    }
    InitWindows(sLoadingWindows);
    DeactivateAllTextPrinters();
    LoadPalette(gStandardMenuPalette, BG_PLTT_ID(15), PLTT_SIZE_4BPP);
    gPlttBufferUnfaded[0] = gPlttBufferFaded[0] = RGB_BLACK;

    FillWindowPixelBuffer(LWIN_TITLE, PIXEL_FILL(0));
    AddTextPrinterParameterized4(LWIN_TITLE, FONT_NORMAL, GetStringCenterAlignXOffset(FONT_NORMAL, title, 28 * 8), 0,
                                 0, 0, sColor_Title, TEXT_SKIP_DRAW, title);
    PutWindowTilemap(LWIN_TITLE);
    CopyWindowToVram(LWIN_TITLE, COPYWIN_FULL);

    if (kind == ROGUE_LOAD_RUN0 || kind == ROGUE_LOAD_ENDLESS)
    {
        who = sText_Director;
        line = sDirectorLines[seed % ARRAY_COUNT(sDirectorLines)];
    }
    else
    {
        who = sText_Birch;
        line = sBirchLines[seed % ARRAY_COUNT(sBirchLines)];
    }
    FillWindowPixelBuffer(LWIN_SAY, PIXEL_FILL(0));
    AddTextPrinterParameterized4(LWIN_SAY, FONT_NORMAL, 8, 0, 0, 0, sColor_Who, TEXT_SKIP_DRAW, who);
    AddTextPrinterParameterized4(LWIN_SAY, FONT_NORMAL, 8, 16, 0, 0, sColor_Line, TEXT_SKIP_DRAW, line);
    PutWindowTilemap(LWIN_SAY);
    CopyWindowToVram(LWIN_SAY, COPYWIN_FULL);

    // The bar's tiles are drawn by the interrupt, straight into VRAM.
    PutWindowTilemap(LWIN_BAR);
    CopyBgTilemapBufferToVram(0);

    gfx = GetPlayerAvatarGraphicsIdByStateIdAndGender(PLAYER_AVATAR_STATE_NORMAL, gSaveBlock2Ptr->playerGender);
    sLoad.spriteId = CreateObjectGraphicsSprite(gfx, SpriteCallbackDummy, BAR_LEFT * 8 + 4, BAR_TOP * 8 - 18, 0);
    if (sLoad.spriteId != MAX_SPRITES)
    {
        gSprites[sLoad.spriteId].oam.priority = 0;
        StartSpriteAnim(&gSprites[sLoad.spriteId], ANIM_STD_GO_EAST);
    }
    // The palettes stay as they are: copied once, not every frame.
    CpuCopy16(gPlttBufferFaded, (void *)PLTT, PLTT_SIZE);

    sLoad.fadingOut = FALSE;
    sLoad.fade = 0;
    sLoad.shown = 0;
    sLoad.ceiling = 10;
    sLoad.eta = 8;
    sLoad.drawnPx = 0xFF;
    sLoad.start = gMain.vblankCounter1;
    SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_BG0 | BLDCNT_TGT1_OBJ | BLDCNT_TGT1_BD | BLDCNT_EFFECT_DARKEN);
    SetGpuReg(REG_OFFSET_BLDY, 16);
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0 | DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    ShowBg(0);
    sLoad.active = TRUE;
    SetVBlankCallback(VBlankCB_Loading);
}

void Rogue_LoadingSetLine(const u8 *str)
{
    if (!sLoad.active)
        return;
    FillWindowPixelRect(LWIN_TITLE, PIXEL_FILL(0), 0, 16, 28 * 8, 16);
    AddTextPrinterParameterized4(LWIN_TITLE, FONT_NORMAL, GetStringCenterAlignXOffset(FONT_NORMAL, str, 28 * 8), 16,
                                 0, 0, sColor_Line, TEXT_SKIP_DRAW, str);
    CopyWindowToVram(LWIN_TITLE, COPYWIN_GFX);
}

void Rogue_LoadingStage(u8 ceiling, u8 frames)
{
    if (!sLoad.active)
        return;
    sLoad.eta = frames;
    sLoad.ceiling = ceiling > 100 ? 100 : ceiling;
}

bool8 Rogue_LoadingActive(void)
{
    return sLoad.active;
}

static void WaitFrames(u32 n)
{
    u32 i;
    for (i = 0; i < n; i++)
        VBlankIntrWait();
}

void Rogue_LoadingHide(void)
{
    u32 shown;
    if (!sLoad.active)
        return;
    // Long enough to read, the bar full a moment, then to black.
    shown = gMain.vblankCounter1 - sLoad.start;
    if (shown < MIN_FRAMES)
        WaitFrames(MIN_FRAMES - shown);
    Rogue_LoadingStage(100, 1);
    WaitFrames(6);
    sLoad.fadingOut = TRUE;
    WaitFrames(FADE_STEPS + 1);
    SetVBlankCallback(NULL);
    sLoad.active = FALSE;
    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
    SetGpuReg(REG_OFFSET_BLDY, 0);
    if (sLoad.spriteId != MAX_SPRITES)
        DestroySpriteAndFreeResources(&gSprites[sLoad.spriteId]);
    sLoad.spriteId = MAX_SPRITES;
    ResetSpriteData();
    FreeAllSpritePalettes();
    FreeAllWindowBuffers();
    DmaFill32(3, 0, (void *)OAM, OAM_SIZE);
    // Black, for the field's fade from black.
    CpuFill16(RGB_BLACK, gPlttBufferUnfaded, PLTT_SIZE);
    CpuFill16(RGB_BLACK, gPlttBufferFaded, PLTT_SIZE);
    DmaFill16(3, RGB_BLACK, (void *)PLTT, PLTT_SIZE);
}
