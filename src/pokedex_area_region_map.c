#include "global.h"
#include "main.h"
#include "menu.h"
#include "bg.h"
#include "malloc.h"
#include "palette.h"
#include "pokedex_area_region_map.h"
#include "regions.h"
#include "region_map.h"
#include "rogue.h"
#include "rogue_gen.h"

#define POKEDEX_AREA_MAP_BG 3

// roguemon: the AREA page shows the region's own map, one tile a place, as
// small as Hoenn's. Where each place sits on it, for the area glow.
EWRAM_DATA s8 gRogueDexPlaceX[ROGUE_MAX_PLACES] = {0};
EWRAM_DATA s8 gRogueDexPlaceY[ROGUE_MAX_PLACES] = {0};

static const u16 sRogueMapPal[] = INCGFX_U16("graphics/pokenav/region_map/map.pal", ".gbapal");
static const u32 sRogueMapGfx[] = INCGFX_U32("graphics/pokenav/region_map/map.png", ".8bpp.smol", "-num_tiles 233 -Wnum_tiles");

#define DEX_MAP_ROWS 20

static void LoadRogueAreaMap(void)
{
    u16 *tiles = Alloc(ROGUE_RMAP_W * ROGUE_RMAP_H * 2);
    u8 *work = Alloc(ROGUE_RMAP_WORK);
    u16 *vram = (u16 *)BG_SCREEN_ADDR(GetBgAttribute(POKEDEX_AREA_MAP_BG, BG_ATTR_MAPBASEINDEX));
    s32 height, top, i;

    height = RogueGen_DrawRegionMap(Rogue_GetRegion(), tiles, gRogueDexPlaceX, gRogueDexPlaceY, work, 1);
    // A region taller than the screen shows the part around the player,
    // or its middle outside the region.
    top = 0;
    if (height > DEX_MAP_ROWS)
    {
        s32 here = Rogue_GetCurrentPlace();
        top = (height - DEX_MAP_ROWS) / 2;
        if (here >= 0 && here < ROGUE_MAX_PLACES)
            top = gRogueDexPlaceY[here] - DEX_MAP_ROWS / 2;
        if (top < 0)
            top = 0;
        if (top > height - DEX_MAP_ROWS)
            top = height - DEX_MAP_ROWS;
    }
    for (i = 0; i < ROGUE_MAX_PLACES; i++)
        gRogueDexPlaceY[i] -= top;
    for (i = 0; i < 32 * 32; i++)
        vram[i] = (top + i / 32 < ROGUE_RMAP_H) ? tiles[(top + i / 32) * ROGUE_RMAP_W + i % 32] : 0;
    Free(work);
    Free(tiles);
    SetBgAttribute(POKEDEX_AREA_MAP_BG, BG_ATTR_METRIC, 0);
    DecompressAndCopyTileDataToVram(POKEDEX_AREA_MAP_BG, sRogueMapGfx, 0, 0, 0);
    ChangeBgX(POKEDEX_AREA_MAP_BG, 0, BG_COORD_SET);
    ChangeBgY(POKEDEX_AREA_MAP_BG, 0, BG_COORD_SET);
    SetBgAttribute(POKEDEX_AREA_MAP_BG, BG_ATTR_PALETTEMODE, 1);
    CpuCopy16(sRogueMapPal, &gPlttBufferUnfaded[BG_PLTT_ID(7)], 2 * PLTT_SIZE_4BPP);
}

// roguemon: the region's own map instead of Hoenn's.
void LoadPokedexAreaMapGfx(void)
{
    LoadRogueAreaMap();
}

bool32 TryShowPokedexAreaMap(void)
{
    if (!FreeTempTileDataBuffersIfPossible())
    {
        ShowBg(POKEDEX_AREA_MAP_BG);
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}

void PokedexAreaMapChangeBgY(u32 move)
{
    ChangeBgY(POKEDEX_AREA_MAP_BG, move * 0x100, BG_COORD_SET);
}
