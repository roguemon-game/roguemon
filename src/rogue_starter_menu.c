// The starter menu in the professor's lab.
//
// A list of rows on the left, one per kind of starter (the Grass, Fire and
// Water starters, then the rows the unlocks add), the highlighted one's
// picture to the right with its types under it, and the message box below
// saying what kind of starter it is. Left and right go through a row's
// choices once the STARTER CASE or FREE CHOICE is earned (the box shows
// "< 3/9 >"). The POKEDEX PICK and ANY POKEMON rows open a list in name
// order, like the POKEDEX's: left and right jump to the next letter.
//
// Which species are on offer is src/rogue.c's business (Rogue_StarterRow*);
// this file only draws and reads the buttons. The state lives on the heap
// while the menu is open.

#include "global.h"
#include "rogue.h"
#include "battle_main.h"
#include "bg.h"
#include "decompress.h"
#include "event_data.h"
#include "graphics.h"
#include "international_string_util.h"
#include "list_menu.h"
#include "main.h"
#include "malloc.h"
#include "menu.h"
#include "palette.h"
#include "pokemon.h"
#include "pokemon_summary_screen.h"
#include "script.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "strings.h"
#include "task.h"
#include "text.h"
#include "trainer_pokemon_sprites.h"
#include "window.h"
#include "constants/songs.h"

#define ROWS_SHOWN 6
#define ROW_HEIGHT 16

// Tiles: the list, the picture, the types, and where on screen.
#define LIST_LEFT   2
#define LIST_TOP    1
#define LIST_WIDTH  11
#define PIC_LEFT    17
#define PIC_TOP     1
#define TYPES_TOP   11
#define BASE_BLOCK  100 // as the field's other menus (CreateWindowFromRect)

#define TAG_STARTER_PIC    0x5320
#define TAG_STARTER_TYPES  0x5321 // and the two after it: the type icons' three palettes
#define TAG_STARTER_ARROWS 0x5324

enum
{
    MODE_ROWS,
    MODE_LIST,
};

// The open list (POKEDEX PICK or ANY POKEMON) comes first: the tests read
// it (tools/roguemon/ruletest.py, test_starters).
struct StarterMenu
{
    u16 *list;
    u16 listCount;
    u16 listCursor;
    u16 listScroll;
    u8 mode;
    u8 nRows;
    u8 cursor;
    u8 listWindow, picWindow, typesWindow;
    u8 arrowTask;
    u8 typeIcons[2];
    u8 picPalette;     // the picture's palette slot, held while the menu is open
    u16 picSprite;
    u16 scroll;        // first row in view, read by the scroll arrows
    u8 kinds[ROGUE_MAX_STARTER_ROWS];
    u8 choice[ROGUE_MAX_STARTER_ROWS];  // the choice inside each row
    u8 choices[ROGUE_MAX_STARTER_ROWS]; // how many each row has
    u16 species[ROGUE_MAX_STARTER_ROWS];
    bool8 shiny[ROGUE_MAX_STARTER_ROWS];
    u8 text[64];
};

#define tMenu 0 // the state, a pointer in data[0] and data[1]
#define tReady data[2] // the A that closed the message before does not pick

static void Task_RogueStarterMenu(u8 taskId);

static struct StarterMenu *GetMenu(u8 taskId)
{
    return (struct StarterMenu *)GetWordTaskArg(taskId, tMenu);
}

static u16 CurrentSpecies(struct StarterMenu *menu, bool32 *shiny)
{
    if (menu->mode == MODE_LIST)
    {
        u16 species = menu->list[menu->listCursor];
        *shiny = Rogue_StarterListShiny(menu->kinds[menu->cursor], species);
        return species;
    }
    *shiny = menu->shiny[menu->cursor];
    return menu->species[menu->cursor];
}

// The tests read the species under the cursor and the mode.
static void UpdateTestVars(struct StarterMenu *menu)
{
    bool32 shiny;
    gSpecialVar_0x8006 = CurrentSpecies(menu, &shiny);
    gSpecialVar_0x8007 = menu->mode;
    gSpecialVar_0x8005 = menu->cursor;
}

// ---------------------------------------------------------------------------
// Drawing

static void PrintRows(struct StarterMenu *menu)
{
    u32 i, y;
    u8 name[POKEMON_NAME_LENGTH + 1];

    FillWindowPixelBuffer(menu->listWindow, PIXEL_FILL(1));
    for (i = 0; i < ROWS_SHOWN; i++)
    {
        y = i * ROW_HEIGHT + 1;
        if (menu->mode == MODE_LIST)
        {
            u32 at = menu->listScroll + i;
            if (at >= menu->listCount)
                break;
            Rogue_StarterRowName(name, ROGUE_STARTER_KIND_ANY, menu->list[at]);
            if (at == menu->listCursor)
                AddTextPrinterParameterized(menu->listWindow, FONT_NORMAL, gText_SelectorArrow3, 0, y, TEXT_SKIP_DRAW, NULL);
        }
        else
        {
            u32 row = menu->scroll + i;
            if (row >= menu->nRows)
                break;
            Rogue_StarterRowName(name, menu->kinds[row], menu->species[row]);
            if (row == menu->cursor)
                AddTextPrinterParameterized(menu->listWindow, FONT_NORMAL, gText_SelectorArrow3, 0, y, TEXT_SKIP_DRAW, NULL);
        }
        AddTextPrinterParameterized(menu->listWindow, FONT_NORMAL, name, 8, y, TEXT_SKIP_DRAW, NULL);
    }
    CopyWindowToVram(menu->listWindow, COPYWIN_GFX);
}

// "The SEED POKEMON."
static u8 *AppendCategory(u8 *str, u16 species)
{
    str = StringCopy(str, COMPOUND_STRING("The "));
    str = StringCopyUppercase(str, GetSpeciesCategory(species));
    return StringCopy(str, COMPOUND_STRING(" POKéMON."));
}

// The message box: what kind of starter it is and where left and right
// go, then what the POKEDEX calls it, or how to open a list.
static void PrintInfo(struct StarterMenu *menu)
{
    u32 kind = menu->kinds[menu->cursor];
    bool32 shiny;
    u16 species = CurrentSpecies(menu, &shiny);
    u8 *str;

    FillWindowPixelBuffer(0, PIXEL_FILL(1));
    AddTextPrinterParameterized(0, FONT_NORMAL, Rogue_StarterKindLabel(kind, species), 0, 1, TEXT_SKIP_DRAW, NULL);
    str = menu->text;
    *str = EOS;
    if (menu->mode == MODE_LIST)
    {
        *str++ = CHAR_LEFT_ARROW;
        *str++ = CHAR_SPACE;
        *str++ = GetSpeciesName(species)[0];
        *str++ = CHAR_SPACE;
        *str++ = CHAR_RIGHT_ARROW;
        *str = EOS;
    }
    else if (menu->choices[menu->cursor] > 1)
    {
        *str++ = CHAR_LEFT_ARROW;
        *str++ = CHAR_SPACE;
        str = ConvertIntToDecimalStringN(str, menu->choice[menu->cursor] + 1, STR_CONV_MODE_LEFT_ALIGN, 2);
        *str++ = CHAR_SLASH;
        str = ConvertIntToDecimalStringN(str, menu->choices[menu->cursor], STR_CONV_MODE_LEFT_ALIGN, 2);
        *str++ = CHAR_SPACE;
        *str++ = CHAR_RIGHT_ARROW;
        *str = EOS;
    }
    if (menu->text[0] != EOS)
        AddTextPrinterParameterized(0, FONT_NORMAL, menu->text, GetStringRightAlignXOffset(FONT_NORMAL, menu->text, 27 * 8) - 8, 1, TEXT_SKIP_DRAW, NULL);
    if (menu->mode == MODE_ROWS && Rogue_StarterRowIsList(kind))
        StringCopy(menu->text, kind == ROGUE_STARTER_KIND_DEX ? COMPOUND_STRING("Press A to see all you caught.")
                                                              : COMPOUND_STRING("Press A to see them all."));
    else
        AppendCategory(menu->text, species);
    AddTextPrinterParameterized(0, FONT_NORMAL, menu->text, 0, 17, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(0, COPYWIN_GFX);
}

static void ShowTypeIcon(struct StarterMenu *menu, u32 i, u32 type, s32 x)
{
    struct Sprite *sprite;
    if (menu->typeIcons[i] == MAX_SPRITES)
        return;
    sprite = &gSprites[menu->typeIcons[i]];
    if (type == TYPE_NONE)
    {
        sprite->invisible = TRUE;
        return;
    }
    StartSpriteAnim(sprite, type);
    sprite->oam.paletteNum = IndexOfSpritePaletteTag(TAG_STARTER_TYPES + gTypesInfo[type].palette - 13);
    sprite->x = x + 16;
    sprite->y = TYPES_TOP * 8 + 8;
    sprite->invisible = FALSE;
}

// The picture and the types of the species under the cursor.
static void ShowPicture(struct StarterMenu *menu)
{
    bool32 shiny;
    u16 species = CurrentSpecies(menu, &shiny);
    u32 t1 = GetSpeciesType(species, 0), t2 = GetSpeciesType(species, 1);
    s32 x = PIC_LEFT * 8;

    if (menu->picSprite != MAX_SPRITES)
        FreeAndDestroyMonPicSprite(menu->picSprite);
    // Into the slot held for it: with a tag, the engine would load the
    // palette under the species' tag and leave it behind.
    menu->picSprite = MAX_SPRITES;
    if (menu->picPalette < 16)
        menu->picSprite = CreateMonPicSprite(species, shiny, 0x8000, TRUE, PIC_LEFT * 8 + 32, PIC_TOP * 8 + 32, menu->picPalette, TAG_NONE);
    if (menu->picSprite == 0xFFFF || menu->picSprite == MAX_SPRITES)
    {
        menu->picSprite = MAX_SPRITES;
    }
    else
    {
        gSprites[menu->picSprite].callback = SpriteCallbackDummy;
        gSprites[menu->picSprite].oam.priority = 0;
    }
    if (t2 == t1)
    {
        ShowTypeIcon(menu, 0, t1, x + 16);
        ShowTypeIcon(menu, 1, TYPE_NONE, x);
    }
    else
    {
        ShowTypeIcon(menu, 0, t1, x);
        ShowTypeIcon(menu, 1, t2, x + 32);
    }
}

static void Redraw(struct StarterMenu *menu)
{
    PrintRows(menu);
    PrintInfo(menu);
    ShowPicture(menu);
    UpdateTestVars(menu);
}

static void AddArrows(struct StarterMenu *menu)
{
    u32 total = menu->mode == MODE_LIST ? menu->listCount : menu->nRows;
    u16 *scroll = menu->mode == MODE_LIST ? &menu->listScroll : &menu->scroll;
    if (total <= ROWS_SHOWN)
        return;
    menu->arrowTask = AddScrollIndicatorArrowPairParameterized(SCROLL_ARROW_UP, LIST_LEFT * 8 + LIST_WIDTH * 4,
                                                               LIST_TOP * 8 - 4, (LIST_TOP + ROWS_SHOWN * 2) * 8 + 4,
                                                               total - ROWS_SHOWN, TAG_STARTER_ARROWS, TAG_STARTER_ARROWS,
                                                               scroll);
}

static void RemoveArrows(struct StarterMenu *menu)
{
    if (menu->arrowTask != TASK_NONE)
        RemoveScrollIndicatorArrowPair(menu->arrowTask);
    menu->arrowTask = TASK_NONE;
}

// ---------------------------------------------------------------------------
// Opening and closing

static u8 AddFramedWindow(u32 left, u32 top, u32 width, u32 height, u32 baseBlock)
{
    struct WindowTemplate template = CreateWindowTemplate(0, left, top, width, height, 15, baseBlock);
    u8 windowId = AddWindow(&template);
    PutWindowTilemap(windowId);
    SetStandardWindowBorderStyle(windowId, FALSE);
    FillWindowPixelBuffer(windowId, PIXEL_FILL(1));
    CopyWindowToVram(windowId, COPYWIN_FULL);
    return windowId;
}

// No room on the heap for the menu: the first row's own pick, once the
// script waits.
static void Task_NoStarterMenu(u8 taskId)
{
    gSpecialVar_Result = 0;
    gSpecialVar_0x8006 = SPECIES_NONE;
    DestroyTask(taskId);
    ScriptContext_Enable();
}

void Rogue_ChooseStarter(void)
{
    struct StarterMenu *menu = AllocZeroed(sizeof(*menu));
    struct SpritePalette palette;
    struct SpriteTemplate typeTemplate;
    u32 i, base = BASE_BLOCK;
    u8 taskId;

    Rogue_PrepareStarters();
    if (menu == NULL)
    {
        CreateTask(Task_NoStarterMenu, 80);
        return;
    }
    menu->nRows = Rogue_GetStarterRows(menu->kinds);
    for (i = 0; i < menu->nRows; i++)
    {
        bool32 shiny;
        menu->choices[i] = Rogue_StarterRowChoices(menu->kinds[i]);
        menu->species[i] = Rogue_StarterRowSpecies(menu->kinds[i], 0, &shiny);
        menu->shiny[i] = shiny;
    }
    menu->arrowTask = TASK_NONE;
    menu->picSprite = MAX_SPRITES;

    LoadMessageBoxAndBorderGfx();
    menu->listWindow = AddFramedWindow(LIST_LEFT, LIST_TOP, LIST_WIDTH, ROWS_SHOWN * 2, base);
    base += LIST_WIDTH * ROWS_SHOWN * 2;
    menu->picWindow = AddFramedWindow(PIC_LEFT, PIC_TOP, 8, 8, base);
    base += 8 * 8;
    menu->typesWindow = AddFramedWindow(PIC_LEFT, TYPES_TOP, 8, 2, base);
    ScheduleBgCopyTilemapToVram(0);

    // The type icons of the summary screen, with their three palettes.
    menu->picPalette = AllocSpritePalette(TAG_STARTER_PIC);
    LoadCompressedSpriteSheet(&gSpriteSheet_MoveTypes);
    for (i = 0; i < 3; i++)
    {
        palette.data = gMoveTypes_Pal + 16 * i;
        palette.tag = TAG_STARTER_TYPES + i;
        LoadSpritePalette(&palette);
    }
    typeTemplate = gSpriteTemplate_MoveTypes;
    typeTemplate.paletteTag = TAG_STARTER_TYPES;
    for (i = 0; i < 2; i++)
    {
        menu->typeIcons[i] = CreateSprite(&typeTemplate, 0, 0, 0);
        if (menu->typeIcons[i] != MAX_SPRITES)
        {
            gSprites[menu->typeIcons[i]].oam.priority = 0;
            gSprites[menu->typeIcons[i]].invisible = TRUE;
        }
    }

    // Back from the professor's question with NO: on the row and the choice
    // the player had (VAR_0x8008 the row, 0xFFFF the first time; VAR_0x8006
    // the species).
    if (gSpecialVar_0x8008 < menu->nRows)
    {
        u32 row = gSpecialVar_0x8008;
        menu->cursor = row;
        menu->scroll = row >= ROWS_SHOWN ? row - ROWS_SHOWN + 1 : 0;
        if (!Rogue_StarterRowIsList(menu->kinds[row]))
            for (i = 0; i < menu->choices[row]; i++)
            {
                bool32 shiny;
                if (Rogue_StarterRowSpecies(menu->kinds[row], i, &shiny) == gSpecialVar_0x8006)
                {
                    menu->choice[row] = i;
                    menu->species[row] = gSpecialVar_0x8006;
                    menu->shiny[row] = shiny;
                    break;
                }
            }
    }

    taskId = CreateTask(Task_RogueStarterMenu, 80);
    SetWordTaskArg(taskId, tMenu, (u32)menu);
    AddArrows(menu);
    Redraw(menu);
}

static void CloseMenu(u8 taskId)
{
    struct StarterMenu *menu = GetMenu(taskId);
    u32 i;

    RemoveArrows(menu);
    if (menu->picSprite != MAX_SPRITES)
        FreeAndDestroyMonPicSprite(menu->picSprite);
    for (i = 0; i < 2; i++)
        if (menu->typeIcons[i] != MAX_SPRITES)
            DestroySprite(&gSprites[menu->typeIcons[i]]);
    FreeSpriteTilesByTag(gSpriteSheet_MoveTypes.tag);
    for (i = 0; i < 3; i++)
        FreeSpritePaletteByTag(TAG_STARTER_TYPES + i);
    FreeSpritePaletteByTag(TAG_STARTER_PIC);
    ClearStdWindowAndFrame(menu->typesWindow, FALSE);
    ClearStdWindowAndFrame(menu->picWindow, FALSE);
    ClearStdWindowAndFrame(menu->listWindow, TRUE);
    RemoveWindow(menu->typesWindow);
    RemoveWindow(menu->picWindow);
    RemoveWindow(menu->listWindow);
    Free(menu->list);
    Free(menu);
    DestroyTask(taskId);
    ScriptContext_Enable();
}

static void Choose(u8 taskId, u16 species)
{
    struct StarterMenu *menu = GetMenu(taskId);
    PlaySE(SE_SELECT);
    gSpecialVar_Result = menu->cursor;
    gSpecialVar_0x8006 = species;
    CloseMenu(taskId);
}

// ---------------------------------------------------------------------------
// Input

static void OpenList(struct StarterMenu *menu)
{
    u32 count, i;
    u16 *list = Rogue_BuildStarterList(menu->kinds[menu->cursor], &count);

    if (list == NULL || count == 0)
    {
        Free(list);
        PlaySE(SE_FAILURE);
        return;
    }
    PlaySE(SE_SELECT);
    menu->list = list;
    menu->listCount = count;
    menu->listCursor = 0;
    for (i = 0; i < count; i++)
        if (list[i] == menu->species[menu->cursor])
            menu->listCursor = i;
    menu->listScroll = menu->listCursor > ROWS_SHOWN / 2 ? menu->listCursor - ROWS_SHOWN / 2 : 0;
    if (menu->listScroll + ROWS_SHOWN > count)
        menu->listScroll = count > ROWS_SHOWN ? count - ROWS_SHOWN : 0;
    RemoveArrows(menu);
    menu->mode = MODE_LIST;
    AddArrows(menu);
    Redraw(menu);
}

static void CloseList(struct StarterMenu *menu)
{
    RemoveArrows(menu);
    Free(menu->list);
    menu->list = NULL;
    menu->mode = MODE_ROWS;
    AddArrows(menu);
    Redraw(menu);
}

// Moves a cursor and keeps it in view. FALSE if it did not move.
static bool32 MoveCursor(u16 *cursor, u16 *scroll, u32 count, s32 delta)
{
    s32 to = *cursor + delta;
    if (to < 0)
        to = 0;
    if (to >= (s32)count)
        to = count - 1;
    if (to == *cursor)
        return FALSE;
    *cursor = to;
    if (*cursor < *scroll)
        *scroll = *cursor;
    if (*cursor >= *scroll + ROWS_SHOWN)
        *scroll = *cursor - ROWS_SHOWN + 1;
    return TRUE;
}

// The next species in the list whose name starts with another letter, or
// back to the first of the letter before.
static u32 LetterJump(struct StarterMenu *menu, s32 dir)
{
    u32 i = menu->listCursor, n = menu->listCount;
    u8 letter = GetSpeciesName(menu->list[i])[0];

    if (dir > 0)
    {
        while (i + 1 < n && GetSpeciesName(menu->list[i])[0] == letter)
            i++;
        return GetSpeciesName(menu->list[i])[0] == letter ? 0 : i;
    }
    // Back to the start of this letter, then to the start of the one before.
    while (i > 0 && GetSpeciesName(menu->list[i - 1])[0] == letter)
        i--;
    if (i == menu->listCursor)
    {
        if (i == 0)
            i = n;
        letter = GetSpeciesName(menu->list[i - 1])[0];
        i--;
        while (i > 0 && GetSpeciesName(menu->list[i - 1])[0] == letter)
            i--;
    }
    return i;
}

static void HandleRows(u8 taskId, struct StarterMenu *menu)
{
    u32 row = menu->cursor;
    u16 cursor = menu->cursor;

    if (JOY_NEW(A_BUTTON))
    {
        if (Rogue_StarterRowIsList(menu->kinds[row]))
            OpenList(menu);
        else
            Choose(taskId, menu->species[row]);
        return;
    }
    if (JOY_REPEAT(DPAD_UP) || JOY_REPEAT(DPAD_DOWN))
    {
        if (MoveCursor(&cursor, &menu->scroll, menu->nRows, JOY_REPEAT(DPAD_UP) ? -1 : 1))
        {
            menu->cursor = cursor;
            PlaySE(SE_SELECT);
            Redraw(menu);
        }
        return;
    }
    if ((JOY_REPEAT(DPAD_LEFT) || JOY_REPEAT(DPAD_RIGHT)) && menu->choices[row] > 1)
    {
        bool32 shiny;
        u32 n = menu->choices[row];
        menu->choice[row] = (menu->choice[row] + (JOY_REPEAT(DPAD_LEFT) ? n - 1 : 1)) % n;
        menu->species[row] = Rogue_StarterRowSpecies(menu->kinds[row], menu->choice[row], &shiny);
        menu->shiny[row] = shiny;
        // The tests read the rows' names.
        Rogue_StarterRowName(gStringVar4 + row * (POKEMON_NAME_LENGTH + 1), menu->kinds[row], menu->species[row]);
        PlaySE(SE_SELECT);
        Redraw(menu);
    }
}

static void HandleList(u8 taskId, struct StarterMenu *menu)
{
    s32 delta = 0;

    if (JOY_NEW(A_BUTTON))
    {
        Choose(taskId, menu->list[menu->listCursor]);
        return;
    }
    if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        CloseList(menu);
        return;
    }
    if (JOY_REPEAT(DPAD_UP))
        delta = -1;
    else if (JOY_REPEAT(DPAD_DOWN))
        delta = 1;
    else if (JOY_REPEAT(L_BUTTON))
        delta = -ROWS_SHOWN;
    else if (JOY_REPEAT(R_BUTTON))
        delta = ROWS_SHOWN;
    else if (JOY_REPEAT(DPAD_LEFT) || JOY_REPEAT(DPAD_RIGHT))
        delta = (s32)LetterJump(menu, JOY_REPEAT(DPAD_LEFT) ? -1 : 1) - menu->listCursor;
    if (delta != 0 && MoveCursor(&menu->listCursor, &menu->listScroll, menu->listCount, delta))
    {
        // A letter jump puts the letter at the top.
        if (JOY_REPEAT(DPAD_LEFT | DPAD_RIGHT))
        {
            menu->listScroll = menu->listCursor;
            if (menu->listScroll + ROWS_SHOWN > menu->listCount)
                menu->listScroll = menu->listCount > ROWS_SHOWN ? menu->listCount - ROWS_SHOWN : 0;
        }
        PlaySE(SE_SELECT);
        Redraw(menu);
    }
}

static void Task_RogueStarterMenu(u8 taskId)
{
    struct StarterMenu *menu = GetMenu(taskId);
    if (!gTasks[taskId].tReady)
    {
        gTasks[taskId].tReady = TRUE;
        return;
    }
    if (menu->mode == MODE_LIST)
        HandleList(taskId, menu);
    else
        HandleRows(taskId, menu);
}

// ---------------------------------------------------------------------------
// The professor's question

// Script special after the menu: "So you want X, the Y POKéMON?" into
// gStringVar4, as FRLG's professor asks, before the starter is given. The
// row picked (VAR_RESULT) is kept in VAR_0x8008 through the YES/NO, for
// Rogue_GiveStarter and for the menu if the answer is NO.
void Rogue_BufferStarterConfirm(void)
{
    u16 species = gSpecialVar_0x8006;
    u8 *str;
    const u8 *category;
    gSpecialVar_0x8008 = gSpecialVar_Result;
    if (species == SPECIES_NONE || species >= NUM_SPECIES)
    {
        u8 kinds[ROGUE_MAX_STARTER_ROWS];
        bool32 shiny;
        u32 n = Rogue_GetStarterRows(kinds);
        species = Rogue_StarterRowSpecies(kinds[gSpecialVar_Result < n ? gSpecialVar_Result : 0], 0, &shiny);
    }
    str = StringCopy(gStringVar4, COMPOUND_STRING("So you want "));
    str = StringCopy(str, GetSpeciesName(species));
    str = StringCopy(str, COMPOUND_STRING(",\nthe "));
    // The category in capitals, as the POKéDEX prints it (SEED POKéMON).
    for (category = GetSpeciesCategory(species); *category != EOS; category++)
        *str++ = (*category >= CHAR_a && *category <= CHAR_z) ? *category - CHAR_a + CHAR_A : *category;
    StringCopy(str, COMPOUND_STRING(" POKéMON?"));
}

// Script special after the starter and its kit: VAR_RESULT TRUE if the
// professor offers a nickname (not for a starter that carries a fallen
// friend's name), the species in STR_VAR_1.
void Rogue_BufferStarterNickname(void)
{
    u32 value = VarGet(VAR_ROGUE_STARTER);
    StringCopy(gStringVar1, GetSpeciesName(value & ROGUE_STARTER_SPECIES_MASK));
    gSpecialVar_Result = ((value >> ROGUE_STARTER_KIND_SHIFT) & 0xF) != ROGUE_STARTER_KIND_FALLEN;
}
