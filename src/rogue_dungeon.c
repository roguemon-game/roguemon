// Dungeons on the game side.
//
// Names, notes and rumors: what a dungeon is called in the floor popup and
// on the region map, what the region map's strip says about it, what
// townsfolk and the professor say about it, and the record of dungeon
// bosses beaten across runs.
//
// Content: what makes a dungeon worth entering. Its wild Pokemon (their own
// tables, floor by floor), its boss (a guardian, a master or the region's
// legend), and its rewards (better finds deeper down, a prize beside the
// boss, a choice room and a camp). The floors themselves are generated in
// src/rogue_gen_dungeon.h; the map objects are built in src/rogue.c.

#include "global.h"
#include "rogue.h"
#include "rogue_run0_story.h"
#include "rogue_gen.h"
#include "event_data.h"
#include "string_util.h"
#include "constants/characters.h"
#include "constants/layouts.h"
#include "battle.h"
#include "battle_setup.h"
#include "event_object_movement.h"
#include "fieldmap.h"
#include "item.h"
#include "pokemon.h"
#include "script_pokemon_util.h"
#include "wild_encounter.h"
#include "constants/event_objects.h"
#include "constants/field_move.h"
#include "constants/items.h"
#include "constants/species.h"
#include "constants/trainers.h"
#include "map_name_popup.h"
#include "constants/map_groups.h"

// The longest name the map popup has room for, floor number included.
#define ROGUE_POPUP_NAME_MAX (MAP_POPUP_STRING_BUFFER_LENGTH - MAP_POPUP_PREFIX_BUFFER_LENGTH - 1)

// The vanilla room each lair is, for its name.
#define ROGUE_ROOMS_LAYOUTS_ONLY
#define ROGUE_ROOMS_STYLES_ONLY
#include "data/rogue_rooms.h"

static u16 LegendSpecies(const struct RogueRegion *r, u32 dungeon);
static const u16 sLegendSpecies[RLEG_COUNT];
static bool32 BufferGateRumor(u32 dungeon, u32 place, u32 rng);
static bool32 RuinsDoorOpen(u32 dungeon);
static void BufferRuinsRiddle(u32 dungeon, u32 rng);
static bool32 BufferTunnelRumor(u32 place);

// ---------------------------------------------------------------------------
// Text (src/rogue_text_dungeons.h): a context for a line about a dungeon,
// said at its way in, and the generator's ASCII made the game's characters.

static void DungeonCtx(struct RogueTextCtx *c, struct RogueTextState *st, s32 dungeon, u8 speaker)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    Rogue_TextCtx(c, st, dungeon >= 0 && dungeon < r->nDungeons ? r->dungeons[dungeon].host : -1, speaker);
    c->dungeon = dungeon >= 0 && dungeon < r->nDungeons ? dungeon : 0xFF;
}

static u8 *DungeonText(u8 *dest, u32 len)
{
    if (len == 0)
    {
        *dest = EOS;
        return dest;
    }
    return Rogue_TextToGame(dest);
}

// ---------------------------------------------------------------------------
// Names

// ASCII from the generator (town names, lore) in the game's characters:
// '@' is the e of POKéMON, '<' and '>' the curly quotes, '\n' a line break,
// '\v' a scroll and '\f' a new box (src/rogue_gen_lore.h).
u8 *Rogue_CopyLoreAscii(u8 *dest, const char *src)
{
    for (; *src; src++)
    {
        u8 c = *src;
        if (c >= 'A' && c <= 'Z')
            *dest++ = CHAR_A + (c - 'A');
        else if (c >= 'a' && c <= 'z')
            *dest++ = CHAR_a + (c - 'a');
        else if (c >= '0' && c <= '9')
            *dest++ = CHAR_0 + (c - '0');
        else
        {
            switch (c)
            {
            case '.':  c = CHAR_PERIOD; break;
            case ',':  c = CHAR_COMMA; break;
            case '!':  c = CHAR_EXCL_MARK; break;
            case '?':  c = CHAR_QUESTION_MARK; break;
            case '\'': c = CHAR_SGL_QUOTE_RIGHT; break;
            case '-':  c = CHAR_HYPHEN; break;
            case ':':  c = CHAR_COLON; break;
            case ';':  c = CHAR_SEMICOLON; break;
            case '/':  c = CHAR_SLASH; break;
            case '@':  c = CHAR_e_ACUTE; break;
            case '<':  c = CHAR_DBL_QUOTE_LEFT; break;
            case '>':  c = CHAR_DBL_QUOTE_RIGHT; break;
            case '^':  c = CHAR_MALE; break;
            case '`':  c = CHAR_FEMALE; break;
            case '&':  c = CHAR_AMPERSAND; break;
            case '(':  c = CHAR_LEFT_PAREN; break;
            case ')':  c = CHAR_RIGHT_PAREN; break;
            case '%':  c = CHAR_PERCENT; break;
            case '+':  c = CHAR_PLUS; break;
            case '\n': c = CHAR_NEWLINE; break;
            case '\f': c = CHAR_PROMPT_CLEAR; break;
            case '\v': c = CHAR_PROMPT_SCROLL; break;
            default:   c = CHAR_SPACE; break;
            }
            *dest++ = c;
        }
    }
    *dest = EOS;
    return dest;
}

// A piece of a legend's lore (src/rogue_gen_lore.h) in the game's
// characters, at most size bytes with its end: made in ASCII in dest
// itself, then converted where it lies (one character each).
u8 *Rogue_BufferLore(u8 *dest, u32 dungeon, u32 piece, u32 way, u32 size)
{
    u32 n = RogueGen_LoreText(Rogue_GetRegion(), dungeon, piece, way, (char *)dest, size);
    if (n == 0)
    {
        *dest = EOS;
        return dest;
    }
    return Rogue_CopyLoreAscii(dest, (const char *)dest);
}

// The town a dungeon is named after: the one nearest its way in by the
// roads, as Petalburg Woods is named after Petalburg. Routes are crossed by
// their links, caves by their ways in and out, so a side cave at the end of
// a side route finds the town that route hangs off (not the first town).
static u32 TownNear(u32 place)
{
    return RogueGen_TownNear(Rogue_GetRegion(), place);
}

// Dungeon names come from the generator (src/rogue_gen_names.h), so the
// host tools see the same ones.
static u8 *CopyChamberName(u8 *dest, u32 dungeon)
{
    char name[ROGUE_NAME_LEN + 16];
    RogueGen_ChamberName(Rogue_GetRegion(), dungeon, name);
    return Rogue_CopyLoreAscii(dest, name);
}

u8 *Rogue_CopyDungeonName(u8 *dest, u32 dungeon)
{
    char name[ROGUE_NAME_LEN + 16];
    RogueGen_DungeonName(Rogue_GetRegion(), dungeon, name);
    return Rogue_CopyLoreAscii(dest, name);
}

static s32 HostedDungeon(u32 place);

// A cave whose floors are ruins, an ice cave or a volcano goes by the
// dungeon's name, so the region map, the cave's popup and the floors all
// agree (BRIGHTTON RUINS, not SHADOW CAVE above BRIGHTTON RUINS B1F). NULL
// for any other cave.
bool32 Rogue_CaveHasDungeonName(u32 place)
{
    return RogueGen_CaveHasDungeonName(Rogue_GetRegion(), place);
}

u8 *Rogue_CopyCaveDungeonName(u8 *dest, u32 place)
{
    if (!Rogue_CaveHasDungeonName(place))
        return NULL;
    return Rogue_CopyDungeonName(dest, HostedDungeon(place));
}

// The dungeon whose floor the header is, or -1.
static s32 FloorDungeon(const struct MapHeader *header)
{
    if (header->mapLayoutId != LAYOUT_ROGUE_FLOOR)
        return -1;
    return RogueGen_DungeonAt(Rogue_GetRegion(), VarGet(VAR_ROGUE_TOWN));
}

// The floor popup: the dungeon's name, before the floor number. FALSE if the
// map is not a dungeon floor.
bool32 Rogue_GetFloorName(u8 *dest, const struct MapHeader *header)
{
    s32 dungeon = FloorDungeon(header);
    u8 name[48], *word;
    u32 room;
    if (dungeon < 0)
        return FALSE;
    // A lair's chamber has its own name (RogueGen_FloorPopup), the only
    // floor of a lair numbered 0.
    if (header->floorNumber == 0 && Rogue_GetRegion()->dungeons[dungeon].kind == RDUN_LAIR
     && RogueGen_HasChamber(&Rogue_GetRegion()->dungeons[dungeon]))
        CopyChamberName(name, dungeon);
    else
        Rogue_CopyDungeonName(name, dungeon);
    // The popup holds 20 characters with the floor (" B1F"): a long town's
    // name with its word (KESTHOLLOW SEA CAVE B1F) would run past the
    // buffer, so it keeps just the word (SEA CAVE B1F).
    room = header->floorNumber == 0 ? ROGUE_POPUP_NAME_MAX : ROGUE_POPUP_NAME_MAX - 4;
    word = name;
    while (StringLength(word) > room)
    {
        while (*word != EOS && *word != CHAR_SPACE)
            word++;
        if (*word == EOS)
        {
            word = name;
            name[room] = EOS;
            break;
        }
        word++;
    }
    StringCopy(dest, word);
    return TRUE;
}

// ---------------------------------------------------------------------------
// Bosses

static bool32 BossIsTrainer(const struct RogueDungeon *d)
{
    return d->boss == RBOSS_MASTER || d->boss == RBOSS_ADMIN;
}

// The trainer slot of a dungeon's boss trainer: the last of its trainers.
static s32 BossTrainerSlot(const struct RogueDungeon *d)
{
    if (!BossIsTrainer(d) || d->nTrainers == 0)
        return -1;
    return d->trainerBase + d->nTrainers - 1;
}

bool32 Rogue_IsDungeonCleared(u32 dungeon)
{
    const struct RogueDungeon *d = &Rogue_GetRegion()->dungeons[dungeon % ROGUE_MAX_DUNGEONS];
    s32 slot = BossTrainerSlot(d);
    if (FlagGet(FLAG_ROGUE_BOSS_0 + dungeon % ROGUE_MAX_DUNGEONS))
        return TRUE;
    return slot >= 0 && FlagGet(TRAINER_FLAGS_START + ROGUE_TRAINER(slot));
}

u32 Rogue_CountDungeonsCleared(void)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    u32 i, n = 0;
    for (i = 0; i < r->nDungeons; i++)
        if (r->dungeons[i].boss != RBOSS_NONE && Rogue_IsDungeonCleared(i))
            n++;
    return n;
}

// How many of the region's dungeons have a boss.
u32 Rogue_CountDungeonBosses(void)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    u32 i, n = 0;
    for (i = 0; i < r->nDungeons; i++)
        n += r->dungeons[i].boss != RBOSS_NONE;
    return n;
}

// The dungeon whose boss trainer this is, or -1.
s32 Rogue_DungeonOfBossTrainer(u16 trainerId)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    u32 i;
    for (i = 0; i < r->nDungeons; i++)
    {
        s32 slot = BossTrainerSlot(&r->dungeons[i]);
        if (slot >= 0 && trainerId == ROGUE_TRAINER(slot))
            return i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// The region map

// The dungeon a place holds (it is the way in), or -1.
static s32 HostedDungeon(u32 place)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    u32 i;
    for (i = 0; i < r->nDungeons; i++)
        if (r->dungeons[i].host == place)
            return i;
    return -1;
}

static const u8 *const sBossNotes[] =
{
    [RBOSS_NONE]     = NULL,
    [RBOSS_GUARDIAN] = COMPOUND_STRING(" Something waits."),
    [RBOSS_MASTER]   = COMPOUND_STRING(" A trainer waits."),
    [RBOSS_ADMIN]    = COMPOUND_STRING(" Shady folk inside."),
    [RBOSS_LEGEND]   = COMPOUND_STRING(" A legend waits."),
};

static bool32 SealCanOpen(u32 dungeon);
static s32 HintedLegend(bool32 leftOnly);

// Has the game told the player where a legend rests before they get there:
// the professor named its place when the run began ("They say one rests
// in ..."), or a leader or a prize ball gave its key ("A traveler left this
// EON TICKET with me").
static bool32 LegendToldOf(u32 dungeon)
{
    u32 kind = ROGUE_GATE_KIND(Rogue_GetRegion()->dungeons[dungeon].gate);
    if ((s32)dungeon == HintedLegend(FALSE))
        return TRUE;
    return (kind == RGATE_GIFT || kind == RGATE_FOUND) && SealCanOpen(dungeon);
}

static s32 NearestDungeon(u32 place);

// What a dungeon is, for the map's note, where its name does not say it:
// a ruins or a volcano by a town's name, a tower called MT. VIGIL.
static const u8 *const sKindWords[] =
{
    [RDUN_TOWER]   = COMPOUND_STRING("a tower"),
    [RDUN_LAIR]    = COMPOUND_STRING("a cave"),
    [RDUN_RUINS]   = COMPOUND_STRING("ruins"),
    [RDUN_ICE]     = COMPOUND_STRING("an ice cave"),
    [RDUN_VOLCANO] = COMPOUND_STRING("a volcano"),
    [RDUN_MIRAGE]  = COMPOUND_STRING("a tower"),
};
// The same, starting a sentence where the place's name is the dungeon's.
static const u8 *const sKindWordsCap[] =
{
    [RDUN_TOWER]   = COMPOUND_STRING("A tower"),
    [RDUN_LAIR]    = COMPOUND_STRING("A cave"),
    [RDUN_RUINS]   = COMPOUND_STRING("Ruins"),
    [RDUN_ICE]     = COMPOUND_STRING("An ice cave"),
    [RDUN_VOLCANO] = COMPOUND_STRING("A volcano"),
    [RDUN_MIRAGE]  = COMPOUND_STRING("A tower"),
};

// Is word (" TOWER") in name?
static bool32 NameHas(const u8 *name, const u8 *word)
{
    u32 n = StringLength(word);
    for (; *name != EOS; name++)
        if (StringCompareN(name, word, n) == 0)
            return TRUE;
    return FALSE;
}

// Has the game told the player of a dungeon before they get to its way in:
// they stood beside a cave's mouth (its route's sign names it), townsfolk of
// a town they have been to talk of it (the dungeon nearest that town, the
// ruins' riddle town, the towns by a tunnel), or they were told of its
// legend. Worked out from places visited, so nothing is saved for it.
static bool32 DungeonToldOf(u32 dungeon)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    const struct RogueDungeon *d = &r->dungeons[dungeon];
    const struct RoguePlace *host = &r->places[d->host];
    u32 i;
    if (Rogue_IsPlaceVisited(d->host))
        return TRUE;
    if (host->kind == RPLACE_CAVE && host->caveIn < r->nPlaces && Rogue_IsPlaceVisited(host->caveIn))
        return TRUE;
    if (d->boss == RBOSS_LEGEND && LegendToldOf(dungeon))
        return TRUE;
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        if (p->kind != RPLACE_TOWN || !Rogue_IsPlaceVisited(i))
            continue;
        if (NearestDungeon(i) == (s32)dungeon)
            return TRUE;
        if (d->kind == RDUN_RUINS && TownNear(d->host) == i)
            return TRUE;
        if (r->tunnel == dungeon && abs(host->gridX - p->gridX) + abs(host->gridY - p->gridY) <= 2)
            return TRUE;
    }
    return FALSE;
}

// Has the player been told of the tunnel's far mouth: they have been in it
// (its way in), or to a town whose townsfolk say where it comes out
// (BufferTunnelRumor's towns, near either end).
static bool32 TunnelToldOf(void)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    const struct RoguePlace *host, *exit;
    u32 i;
    if (r->tunnel >= r->nDungeons)
        return FALSE;
    if (Rogue_IsPlaceVisited(r->dungeons[r->tunnel].host))
        return TRUE;
    host = &r->places[r->dungeons[r->tunnel].host];
    exit = &r->places[r->dungeons[r->tunnel].exit];
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        if (p->kind == RPLACE_TOWN && Rogue_IsPlaceVisited(i)
         && (abs(host->gridX - p->gridX) + abs(host->gridY - p->gridY) <= 2
          || abs(exit->gridX - p->gridX) + abs(exit->gridY - p->gridY) <= 1))
            return TRUE;
    }
    return FALSE;
}

// The dungeon's name and what it is: "MT. SMOLDER, a volcano", or only
// "Ruins" where the place is named for it already (its bar says the name).
static u8 *AppendNameAndKind(u8 *str, u32 place, u32 dungeon)
{
    const struct RogueDungeon *d = &Rogue_GetRegion()->dungeons[dungeon];
    const u8 *word = d->kind < ARRAY_COUNT(sKindWords) ? sKindWords[d->kind] : NULL;
    u8 name[48], here[48];
    Rogue_CopyDungeonName(name, dungeon);
    Rogue_CopyPlaceName(here, place);
    if (word != NULL && (((d->kind == RDUN_TOWER || d->kind == RDUN_MIRAGE) && NameHas(name, COMPOUND_STRING("TOWER")))
                      || (d->kind == RDUN_ICE && NameHas(name, COMPOUND_STRING("ICE CAVE")))
                      || (d->kind == RDUN_LAIR && (NameHas(name, COMPOUND_STRING("CAVE")) || NameHas(name, COMPOUND_STRING("TUNNEL"))))))
        word = NULL;
    if (StringCompare(name, here) == 0 || d->kind == RDUN_CAVE || d->kind == RDUN_VICTORY)
        return word != NULL ? StringCopy(str, sKindWordsCap[d->kind]) : str;
    str = StringCopy(str, name);
    if (word != NULL)
        str = StringCopy(StringCopy(str, COMPOUND_STRING(", ")), word);
    return str;
}

// The region map's note on a dungeon held by a place: what it is and its
// name, how many floors, where a tunnel leads, and what waits (unless
// brief: a legend's note always stays), or that it is cleared. Before the
// player has been there, the name and kind of one they were told of, or
// only a legend they were told of.
u8 *Rogue_AppendDungeonNote(u8 *str, u32 place, bool32 brief)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    const struct RogueDungeon *d;
    s32 dungeon = HostedDungeon(place);
    u8 *start;

    // A tunnel's far mouth, once seen: where it leads. Told of it, that a
    // tunnel comes out here.
    if (dungeon < 0 && RogueGen_TunnelMouthAt(r, place))
    {
        if (Rogue_IsPlaceVisited(place))
        {
            str = StringCopy(str, COMPOUND_STRING("A tunnel to "));
            str = Rogue_CopyPlaceName(str, r->dungeons[r->tunnel].host);
            return StringCopy(str, COMPOUND_STRING(". "));
        }
        if (TunnelToldOf())
            return StringCopy(str, COMPOUND_STRING("A tunnel comes out here. "));
    }
    if (dungeon < 0)
        return str;
    d = &r->dungeons[dungeon];
    if (!Rogue_IsPlaceVisited(place))
    {
        bool32 legend = d->boss == RBOSS_LEGEND && !Rogue_IsDungeonCleared(dungeon);
        // Told of it: its name and kind.
        if (DungeonToldOf(dungeon))
        {
            start = str;
            str = AppendNameAndKind(str, place, dungeon);
            if (str != start)
                str = StringCopy(str, COMPOUND_STRING(". "));
        }
        if (!legend)
            return str;
        // The MARKING MAP shows where a legend sleeps before you get there.
        if (Rogue_IsUnlockOn(UNLOCK_MARKING_MAP))
        {
            str = StringCopy(str, GetSpeciesName(LegendSpecies(r, dungeon)));
            str = StringCopy(str, COMPOUND_STRING(" waits here. "));
        }
        else if (LegendToldOf(dungeon))
        {
            str = StringCopy(str, COMPOUND_STRING("A legend waits here. "));
        }
        return str;
    }
    start = str;
    str = AppendNameAndKind(str, place, dungeon);
    if (d->boss != RBOSS_NONE && Rogue_IsDungeonCleared(dungeon))
        return StringCopy(str, str == start ? COMPOUND_STRING("Cleared! ") : COMPOUND_STRING(": cleared! "));
    switch (d->kind)
    {
    case RDUN_FOREST:
        str = StringCopy(str, COMPOUND_STRING("."));
        break;
    case RDUN_LAIR:
        str = StringCopy(str, COMPOUND_STRING(": "));
        str = ConvertIntToDecimalStringN(str, d->nFloors - 1, STR_CONV_MODE_LEFT_ALIGN, 1);
        str = StringCopy(str, d->nFloors == 2 ? COMPOUND_STRING(" floor down.") : COMPOUND_STRING(" floors down."));
        break;
    case RDUN_CAVE:
    case RDUN_VICTORY:
        str = ConvertIntToDecimalStringN(str, d->nFloors, STR_CONV_MODE_LEFT_ALIGN, 1);
        str = StringCopy(str, d->nFloors == 1 ? COMPOUND_STRING(" floor below.") : COMPOUND_STRING(" floors below."));
        break;
    default:
        str = StringCopy(str, str == start ? COMPOUND_STRING("") : COMPOUND_STRING(": "));
        str = ConvertIntToDecimalStringN(str, d->nFloors, STR_CONV_MODE_LEFT_ALIGN, 1);
        str = StringCopy(str, d->nFloors == 1 ? COMPOUND_STRING(" floor") : COMPOUND_STRING(" floors"));
        str = StringCopy(str, d->kind == RDUN_TOWER ? COMPOUND_STRING(" up.") : COMPOUND_STRING(" down."));
        break;
    }
    // The main road goes on through the floors below, as through Granite
    // Cave's B1F: nothing else would say so.
    if (d->required && r->places[place].kind == RPLACE_CAVE && d->kind != RDUN_VICTORY)
        str = StringCopy(str, COMPOUND_STRING(" The way on is on B1F."));
    if (r->tunnel == dungeon && d->exit < r->nPlaces)
    {
        str = StringCopy(str, COMPOUND_STRING(" A tunnel to "));
        str = Rogue_CopyPlaceName(str, d->exit);
        str = StringCopy(str, COMPOUND_STRING("."));
    }
    if (d->boss == RBOSS_LEGEND && Rogue_IsUnlockOn(UNLOCK_MARKING_MAP))
    {
        // The MARKING MAP knows which legend.
        str = StringCopy(str, COMPOUND_STRING(" "));
        str = StringCopy(str, GetSpeciesName(LegendSpecies(r, dungeon)));
        str = StringCopy(str, Rogue_LegendGateOpen(dungeon) ? COMPOUND_STRING(" waits.") : COMPOUND_STRING(" waits, sealed."));
    }
    else if ((!brief || d->boss == RBOSS_LEGEND) && d->boss < ARRAY_COUNT(sBossNotes) && sBossNotes[d->boss])
        str = StringCopy(str, sBossNotes[d->boss]);
    return StringCopy(str, COMPOUND_STRING(" "));
}

// The region map's note on the volcano's crater (a cell with no place):
// where it is entered, once the player knows. FALSE if nothing to add.
bool32 Rogue_BufferCraterNote(u8 *str)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    u32 i;
    str = StringCopy(str, COMPOUND_STRING("Smoke rises from its crater."));
    for (i = 0; i < r->nDungeons; i++)
    {
        if (r->dungeons[i].kind != RDUN_VOLCANO || !DungeonToldOf(i))
            continue;
        if (r->dungeons[i].boss != RBOSS_NONE && Rogue_IsDungeonCleared(i))
            str = StringCopy(str, COMPOUND_STRING(" Cleared!"));
        str = StringCopy(str, COMPOUND_STRING(" Entered from "));
        str = Rogue_CopyPlaceName(str, r->dungeons[i].host);
        StringCopy(str, COMPOUND_STRING("."));
        return TRUE;
    }
    return FALSE;
}

// The volcano the region map draws (RogueGen_VolcanoCell), named as
// Hoenn's and Kanto's are (RogueGen_VolcanoName).
u8 *Rogue_CopyVolcanoName(u8 *dest)
{
    char name[ROGUE_NAME_LEN + 8];
    RogueGen_VolcanoName(Rogue_GetRegion(), name);
    return Rogue_CopyLoreAscii(dest, name);
}

// Marks the region map shows beside the dungeons' own (see rogue.h). A
// tunnel's far mouth (RogueGen_TunnelMouthAt) on the place it opens onto:
// framed once that place is visited (its mouth is in sight there), else
// the small gray mark, as a dungeon's way in not yet found.
u32 Rogue_ExtraMapMarks(u8 *places, u8 *styles, u32 max)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    u32 exit;
    if (max == 0 || r->tunnel >= r->nDungeons)
        return 0;
    exit = r->dungeons[r->tunnel].exit;
    places[0] = exit;
    styles[0] = DMARK_TUNNEL | (Rogue_IsPlaceVisited(exit) ? 0 : DMARK_NOT_FOUND);
    return 1;
}

// ---------------------------------------------------------------------------
// Rumors

// The dungeon nearest a place on the region's grid, cleared ones last.
static s32 NearestDungeon(u32 place)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    const struct RoguePlace *p = &r->places[place];
    s32 best = -1;
    u32 i, bestCost = 0xFFFF;
    for (i = 0; i < r->nDungeons; i++)
    {
        const struct RoguePlace *h = &r->places[r->dungeons[i].host];
        u32 cost = abs(h->gridX - p->gridX) + abs(h->gridY - p->gridY);
        if (r->dungeons[i].boss != RBOSS_NONE && Rogue_IsDungeonCleared(i))
            cost += 20;
        if (cost < bestCost)
        {
            best = i;
            bestCost = cost;
        }
    }
    return best;
}

// A townsperson's line about one of the dungeons round them (the nearest
// most often: RogueText_RumorDungeon), into STR_VAR_1. The tunnel, a
// legend's key and the ruins' riddle are told here; the rest by
// src/rogue_text_dungeons.h. FALSE if the region has none.
bool32 Rogue_BufferDungeonRumor(u32 place, u32 rng)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    const struct RogueDungeon *d;
    struct RogueTextCtx c;
    struct RogueTextState st;
    s32 dungeon;

    // Near a tunnel's ends, half the time, where it comes out.
    if ((rng & 0x80) && BufferTunnelRumor(place))
        return TRUE;
    Rogue_TextCtx(&c, &st, place, RSPK_FOLK);
    dungeon = RogueText_RumorDungeon(r, place, rng, st.bosses);
    if (dungeon < 0)
        return FALSE;
    d = &r->dungeons[dungeon];
    // Half the time a legend's rumor tells what opens its way.
    if (d->boss == RBOSS_LEGEND && !Rogue_IsDungeonCleared(dungeon) && (rng & 0x40) && !Rogue_LegendGateOpen(dungeon)
     && BufferGateRumor(dungeon, place, rng))
        return TRUE;
    // Ruins: the riddle of the vault's door.
    if (d->kind == RDUN_RUINS && !RuinsDoorOpen(dungeon))
    {
        BufferRuinsRiddle(dungeon, rng >> 2);
        return TRUE;
    }
    c.dungeon = dungeon;
    DungeonText(gStringVar1, RogueText_DungeonRumor(&c, rng, (char *)gStringVar1, sizeof(gStringVar1)));
    return gStringVar1[0] != EOS;
}

static const u8 *const sCountWords[] = {
    COMPOUND_STRING("a"), COMPOUND_STRING("a"), COMPOUND_STRING("two"), COMPOUND_STRING("three"),
    COMPOUND_STRING("four"), COMPOUND_STRING("five"), COMPOUND_STRING("six"),
};

// The legend the professor names: the first whose way opens (of those left,
// with leftOnly).
static s32 HintedLegend(bool32 leftOnly)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    u32 i;
    s32 first = -1;
    for (i = 0; i < r->nDungeons; i++)
    {
        if (r->dungeons[i].boss != RBOSS_LEGEND || (leftOnly && Rogue_IsDungeonCleared(i)))
            continue;
        if (first < 0 || RogueGen_LegendGateBadges(r, i) < RogueGen_LegendGateBadges(r, first))
            first = i;
    }
    return first;
}

// Script special: the professor's hint at the region's legends, into
// STR_VAR_1: how many there are in the region, and where the first to open
// rests. VAR_RESULT is FALSE when the region has none left.
void Rogue_BufferLegendHint(void)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    u32 i, n = 0;
    s32 first = HintedLegend(TRUE);
    u8 *str;

    gSpecialVar_Result = FALSE;
    for (i = 0; i < r->nDungeons; i++)
        if (r->dungeons[i].boss == RBOSS_LEGEND && !Rogue_IsDungeonCleared(i))
            n++;
    if (first < 0)
        return;
    str = StringCopy(gStringVar1, COMPOUND_STRING("Tales tell of "));
    str = StringCopy(str, sCountWords[n < ARRAY_COUNT(sCountWords) ? n : ARRAY_COUNT(sCountWords) - 1]);
    str = StringCopy(str, n == 1 ? COMPOUND_STRING(" legendary\nPOKéMON in this region.\pThey say it rests in\n")
                                 : COMPOUND_STRING(" legendary\nPOKéMON in this region.\pThey say one rests in\n"));
    str = Rogue_CopyDungeonName(str, first);
    StringCopy(str, COMPOUND_STRING("."));
    gSpecialVar_Result = TRUE;
}

// ---------------------------------------------------------------------------
// Helpers

static u32 Bst(u16 species)
{
    const struct SpeciesInfo *info = &gSpeciesInfo[species];
    return info->baseHP + info->baseAttack + info->baseDefense + info->baseSpeed + info->baseSpAttack + info->baseSpDefense;
}

static u16 PickFrom(u32 *rng, const u16 *list, u32 count)
{
    return list[RogueRng_Range(rng, count)];
}

static bool32 InList(const u16 *list, u32 count, u16 value)
{
    u32 i;
    for (i = 0; i < count; i++)
        if (list[i] == value)
            return TRUE;
    return FALSE;
}

// The badges a dungeon is measured by: those its way in opens with, or for
// an optional one (not the hideout) those held now if more, as its floors
// follow them (RogueGen_FloorLevel).
static u32 DungeonBadges(const struct RogueDungeon *d)
{
    u32 held = Rogue_BadgeCount();
    if (d->required || d->kind == RDUN_HIDEOUT || held <= d->badges)
        return d->badges;
    return held;
}

// The level of the next gym ace after a dungeon opens (the first Elite Four
// member's, past the eighth badge): what a boss is measured against.
static u32 NextAce(const struct RogueRegion *r, const struct RogueDungeon *d)
{
    u32 badges = DungeonBadges(d);
    if (badges >= ROGUE_NUM_GYMS)
        return r->e4Level[0];
    return r->aceLevel[badges];
}

// The level of a dungeon's first and deepest floors (RogueGen_FloorLevel).
static u32 FirstFloorLevel(const struct RogueRegion *r, const struct RogueDungeon *d)
{
    return RogueGen_FloorLevel(r, d - r->dungeons, 1);
}

static u32 LastFloorLevel(const struct RogueRegion *r, const struct RogueDungeon *d)
{
    return RogueGen_FloorLevel(r, d - r->dungeons, d->nFloors);
}

// ---------------------------------------------------------------------------
// Wild Pokemon
//
// Each floor has its own table from the dungeon kind's list, so a cave's
// floors are not its own map's Pokemon again, and each floor differs from the
// one above. Unlike the routes, dungeons have the evolved forms the level
// allows, as Emerald's Victory Road has Golbat and Hariyama. The floors share the
// dungeon's one-catch area (the cave's own, for floors below a cave).

static const u16 sCaveWild[] =
{
    SPECIES_ZUBAT, SPECIES_GEODUDE, SPECIES_ARON, SPECIES_SABLEYE, SPECIES_MAWILE, SPECIES_NOSEPASS,
    SPECIES_ROGGENROLA, SPECIES_WOOBAT, SPECIES_DRILBUR, SPECIES_ONIX, SPECIES_MACHOP, SPECIES_MAKUHITA,
    SPECIES_WHISMUR, SPECIES_DIGLETT, SPECIES_PARAS, SPECIES_CLEFAIRY, SPECIES_DUNSPARCE, SPECIES_ABRA,
    SPECIES_BRONZOR, SPECIES_SHUCKLE, SPECIES_TIMBURR, SPECIES_DWEBBLE, SPECIES_NOIBAT, SPECIES_CARBINK,
};
// Caves by a volcano, like Fiery Path and Mt. Chimney's.
static const u16 sVolcanoWild[] =
{
    SPECIES_SLUGMA, SPECIES_NUMEL, SPECIES_TORKOAL, SPECIES_HOUNDOUR, SPECIES_HEATMOR, SPECIES_GEODUDE,
    SPECIES_ZUBAT, SPECIES_MACHOP, SPECIES_GRIMER, SPECIES_KOFFING,
};
// Caves by the sea, like Seafoam Islands and Shoal Cave.
static const u16 sSeaCaveWild[] =
{
    SPECIES_ZUBAT, SPECIES_SEEL, SPECIES_SPHEAL, SPECIES_SHELLDER, SPECIES_KRABBY, SPECIES_PSYDUCK,
    SPECIES_SLOWPOKE, SPECIES_SNORUNT, SPECIES_SWINUB, SPECIES_GOLBAT,
};
static const u16 sVictoryWild[] =
{
    SPECIES_MACHOP, SPECIES_MAKUHITA, SPECIES_ARON, SPECIES_WHISMUR, SPECIES_MEDITITE, SPECIES_MAWILE,
    SPECIES_SABLEYE, SPECIES_ZUBAT, SPECIES_GEODUDE, SPECIES_ONIX, SPECIES_RHYHORN, SPECIES_DRUDDIGON,
    SPECIES_AXEW, SPECIES_SNEASEL, SPECIES_TEDDIURSA, SPECIES_GLIGAR,
};
static const u16 sForestWild[] =
{
    SPECIES_CATERPIE, SPECIES_WEEDLE, SPECIES_WURMPLE, SPECIES_VENONAT, SPECIES_PARAS, SPECIES_SHROOMISH,
    SPECIES_SEEDOT, SPECIES_ODDISH, SPECIES_PINECO, SPECIES_HOOTHOOT, SPECIES_SPINARAK, SPECIES_LEDYBA,
    SPECIES_SEWADDLE, SPECIES_VENIPEDE, SPECIES_FOONGUS, SPECIES_NINCADA, SPECIES_BELLSPROUT, SPECIES_KRICKETOT,
    SPECIES_BURMY, SPECIES_COMBEE, SPECIES_CHERUBI, SPECIES_PIKACHU, SPECIES_KARRABLAST, SPECIES_SHELMET,
    SPECIES_MORELULL, SPECIES_FOMANTIS,
};
static const u16 sTowerWild[] =
{
    SPECIES_GASTLY, SPECIES_SHUPPET, SPECIES_DUSKULL, SPECIES_MISDREAVUS, SPECIES_LITWICK, SPECIES_DRIFLOON,
    SPECIES_YAMASK, SPECIES_PHANTUMP, SPECIES_PUMPKABOO, SPECIES_HONEDGE, SPECIES_GOLETT, SPECIES_CUBONE,
    SPECIES_NATU, SPECIES_CHINGLING, SPECIES_ABRA, SPECIES_DROWZEE, SPECIES_MUNNA, SPECIES_ELGYEM,
};

// The ice cave's, as Shoal Cave's and Seafoam's (Snorunt, Spheal), Ice
// Path's (Swinub, Sneasel, Delibird, Jynx) and Mt. Coronet's snow (Snover).
static const u16 sIceWild[] =
{
    SPECIES_SNORUNT, SPECIES_SPHEAL, SPECIES_SNOVER, SPECIES_SWINUB, SPECIES_SNEASEL, SPECIES_SEEL,
    SPECIES_SHELLDER, SPECIES_DELIBIRD, SPECIES_SMOOCHUM, SPECIES_ZUBAT, SPECIES_BERGMITE, SPECIES_CUBCHOO,
};
// The volcano's, as Fiery Path's and Mt. Chimney's (Slugma, Numel,
// Torkoal), Mt. Ember's (Magmar, Ponyta) and Stark Mountain's.
static const u16 sVolcanoDungeonWild[] =
{
    SPECIES_SLUGMA, SPECIES_NUMEL, SPECIES_TORKOAL, SPECIES_MAGBY, SPECIES_HOUNDOUR, SPECIES_PONYTA,
    SPECIES_GEODUDE, SPECIES_KOFFING, SPECIES_GRIMER, SPECIES_HEATMOR,
};

// The rare slot (the two 1% slots), as Mt. Pyre's Absol or a Larvitar
// deep in a cave. Lines that are rare until late keep to their level
// (Rogue_WildFits).
static const u16 sCaveRare[]    = { SPECIES_LARVITAR, SPECIES_GIBLE, SPECIES_BELDUM, SPECIES_RIOLU, SPECIES_DITTO, SPECIES_LARVESTA };
static const u16 sForestRare[]  = { SPECIES_SCYTHER, SPECIES_PINSIR, SPECIES_HERACROSS, SPECIES_EEVEE, SPECIES_LARVESTA, SPECIES_SNORLAX };
static const u16 sTowerRare[]   = { SPECIES_ABSOL, SPECIES_SPIRITOMB, SPECIES_DITTO, SPECIES_RIOLU, SPECIES_SOLOSIS, SPECIES_GOTHITA };
// A shipwreck's ghosts and the sea's Pokemon that came aboard, as the
// Abandoned Ship's Tentacool; a power plant's electric Pokemon, as New
// Mauville's and the Power Plant's.
static const u16 sShipWild[] =
{
    SPECIES_TENTACOOL, SPECIES_WINGULL, SPECIES_SHUPPET, SPECIES_DUSKULL, SPECIES_FRILLISH, SPECIES_GASTLY,
    SPECIES_DRIFLOON, SPECIES_KRABBY, SPECIES_SHELLOS, SPECIES_CORPHISH, SPECIES_RATTATA, SPECIES_SABLEYE,
};
static const u16 sPlantWild[] =
{
    SPECIES_MAGNEMITE, SPECIES_VOLTORB, SPECIES_ELECTRIKE, SPECIES_PIKACHU, SPECIES_PICHU, SPECIES_GRIMER,
    SPECIES_KOFFING, SPECIES_SHINX, SPECIES_ELEKID, SPECIES_PLUSLE, SPECIES_MINUN, SPECIES_JOLTIK,
};
static const u16 sShipRare[]    = { SPECIES_LAPRAS, SPECIES_MANTYKE, SPECIES_SPIRITOMB, SPECIES_CHINCHOU, SPECIES_DHELMISE, SPECIES_RELICANTH };
static const u16 sPlantRare[]   = { SPECIES_ROTOM, SPECIES_PORYGON, SPECIES_DITTO, SPECIES_MAREEP, SPECIES_TYNAMO, SPECIES_ELEKID };
static const u16 sVictoryRare[] = { SPECIES_BAGON, SPECIES_LARVITAR, SPECIES_GIBLE, SPECIES_DEINO, SPECIES_RIOLU, SPECIES_BELDUM };
static const u16 sIceRare[]     = { SPECIES_LAPRAS, SPECIES_ABSOL, SPECIES_DELIBIRD, SPECIES_SNEASEL, SPECIES_LARVITAR, SPECIES_DITTO };
static const u16 sVolcanoRare[] = { SPECIES_LARVESTA, SPECIES_GROWLITHE, SPECIES_VULPIX, SPECIES_TURTONATOR, SPECIES_LARVITAR, SPECIES_DITTO };

// A lair's caves have Pokemon of its legend's kind, as Seafoam's ice
// for Articuno, the Power Plant's Magnemite for Zapdos, Mt. Ember's
// fire for Moltres, Sky Pillar's dragons.
static const u16 sIceCaveWild[] =
{
    SPECIES_SNORUNT, SPECIES_SPHEAL, SPECIES_SWINUB, SPECIES_SNEASEL, SPECIES_SEEL, SPECIES_SHELLDER,
    SPECIES_CUBCHOO, SPECIES_BERGMITE, SPECIES_VANILLITE, SPECIES_ZUBAT,
};
static const u16 sPowerCaveWild[] =
{
    SPECIES_MAGNEMITE, SPECIES_VOLTORB, SPECIES_ELECTRIKE, SPECIES_SHINX, SPECIES_BLITZLE, SPECIES_JOLTIK,
    SPECIES_TYNAMO, SPECIES_PIKACHU, SPECIES_ZUBAT, SPECIES_GEODUDE,
};
static const u16 sMindCaveWild[] =
{
    SPECIES_NATU, SPECIES_ABRA, SPECIES_MEDITITE, SPECIES_BRONZOR, SPECIES_SPOINK, SPECIES_WOOBAT,
    SPECIES_LUNATONE, SPECIES_SOLROCK, SPECIES_BALTOY, SPECIES_DROWZEE,
};
static const u16 sSkyCaveWild[] =
{
    SPECIES_SWABLU, SPECIES_NOIBAT, SPECIES_ZUBAT, SPECIES_TRAPINCH, SPECIES_AXEW, SPECIES_DRUDDIGON,
    SPECIES_WOOBAT, SPECIES_ARON, SPECIES_GLIGAR, SPECIES_SKARMORY,
};

// Ruins have Pokemon of old stone and sand, as the Desert Ruins' Baltoy,
// Unova's Relic Castle (Sigilyph, Yamask, Sandile) and Sinnoh's ruins
// (Bronzor). Each floor has one of the first three, and Unown, which lives
// nowhere else.
static const u16 sRuinsWild[] =
{
    SPECIES_BALTOY, SPECIES_SIGILYPH, SPECIES_BRONZOR, SPECIES_YAMASK, SPECIES_GOLETT, SPECIES_ELGYEM,
    SPECIES_NATU, SPECIES_SANDSHREW, SPECIES_TRAPINCH, SPECIES_CACNEA, SPECIES_SANDILE, SPECIES_ROGGENROLA,
    SPECIES_NOSEPASS, SPECIES_HIPPOPOTAS, SPECIES_LUNATONE, SPECIES_SOLROCK,
};
#define RUINS_SIGNATURE 3 // the first of sRuinsWild, one a floor
// (v1.1 peaks) The desert tower's: the Mirage Tower's Sandshrew and
// Trapinch, and what else lives in the sand and stone of a desert.
static const u16 sMirageWild[] =
{
    SPECIES_SANDSHREW, SPECIES_TRAPINCH, SPECIES_CACNEA, SPECIES_BALTOY, SPECIES_SANDILE, SPECIES_HIPPOPOTAS,
    SPECIES_SKORUPI, SPECIES_GLIGAR, SPECIES_DIGLETT, SPECIES_PHANPY, SPECIES_SILICOBRA, SPECIES_HELIOPTILE,
    SPECIES_DWEBBLE, SPECIES_NUMEL,
};
static const u16 sMirageRare[] = { SPECIES_GIBLE, SPECIES_LARVITAR, SPECIES_LARVESTA, SPECIES_AXEW, SPECIES_DRUDDIGON };
static const u16 sRuinsRare[] = { SPECIES_GIBLE, SPECIES_LARVESTA, SPECIES_BELDUM, SPECIES_LARVITAR, SPECIES_DITTO, SPECIES_DRUDDIGON };

static bool32 ThemeVolcanic(u32 theme)
{
    return theme == RTHEME_ASH || theme == RTHEME_SPRING;
}

static bool32 ThemeSea(u32 theme)
{
    return theme == RTHEME_HARBOR || theme == RTHEME_ISLAND || theme == RTHEME_BEACH || theme == RTHEME_COVE;
}

static void KindLists(const struct RogueRegion *r, const struct RogueDungeon *d, const u16 **list, u32 *n, const u16 **rare)
{
    u32 theme = r->places[d->host].theme;
    if (d->kind == RDUN_LAIR)
    {
        *rare = sCaveRare;
        switch (GetSpeciesType(sLegendSpecies[d->legend % RLEG_COUNT], 0))
        {
        case TYPE_ICE:      *list = sIceCaveWild, *n = ARRAY_COUNT(sIceCaveWild); return;
        case TYPE_ELECTRIC: *list = sPowerCaveWild, *n = ARRAY_COUNT(sPowerCaveWild); return;
        case TYPE_FIRE:     *list = sVolcanoWild, *n = ARRAY_COUNT(sVolcanoWild); return;
        case TYPE_WATER:    *list = sSeaCaveWild, *n = ARRAY_COUNT(sSeaCaveWild); return;
        case TYPE_PSYCHIC:  *list = sMindCaveWild, *n = ARRAY_COUNT(sMindCaveWild); return;
        case TYPE_DRAGON:
        case TYPE_FLYING:   *list = sSkyCaveWild, *n = ARRAY_COUNT(sSkyCaveWild), *rare = sVictoryRare; return;
        default:            break;
        }
    }
    switch (d->kind)
    {
    case RDUN_VICTORY:
        *list = sVictoryWild, *n = ARRAY_COUNT(sVictoryWild), *rare = sVictoryRare;
        break;
    case RDUN_FOREST:
        *list = sForestWild, *n = ARRAY_COUNT(sForestWild), *rare = sForestRare;
        break;
    case RDUN_TOWER:
        *list = sTowerWild, *n = ARRAY_COUNT(sTowerWild), *rare = sTowerRare;
        break;
    case RDUN_SHIP:
        *list = sShipWild, *n = ARRAY_COUNT(sShipWild), *rare = sShipRare;
        break;
    case RDUN_PLANT:
        *list = sPlantWild, *n = ARRAY_COUNT(sPlantWild), *rare = sPlantRare;
        break;
    case RDUN_RUINS:
        *list = sRuinsWild, *n = ARRAY_COUNT(sRuinsWild), *rare = sRuinsRare;
        break;
    case RDUN_MIRAGE:
        *list = sMirageWild, *n = ARRAY_COUNT(sMirageWild), *rare = sMirageRare;
        break;
    case RDUN_ICE:
        *list = sIceWild, *n = ARRAY_COUNT(sIceWild), *rare = sIceRare;
        break;
    case RDUN_VOLCANO:
        *list = sVolcanoDungeonWild, *n = ARRAY_COUNT(sVolcanoDungeonWild), *rare = sVolcanoRare;
        break;
    default:
        *rare = sCaveRare;
        if (ThemeVolcanic(theme))
            *list = sVolcanoWild, *n = ARRAY_COUNT(sVolcanoWild);
        else if (ThemeSea(theme) && RogueRng_Hash(r->seed, 13400 + (d - r->dungeons)) % 2)
            *list = sSeaCaveWild, *n = ARRAY_COUNT(sSeaCaveWild);
        else
            *list = sCaveWild, *n = ARRAY_COUNT(sCaveWild);
        break;
    }
}

// How often a step on a floor meets a Pokemon (RogueGen_FloorWildRate).
u32 RogueDungeon_WildRate(u32 dungeon, u32 floor)
{
    return RogueGen_FloorWildRate(Rogue_GetRegion(), dungeon, floor);
}

// The base stat total a dungeon's wild first stage may have at a level:
// like the routes', a little above (dungeons are where the tough ones live).
static u32 WildMaxBst(u32 level)
{
    if (level <= 12) return 330;
    if (level <= 20) return 380;
    if (level <= 30) return 430;
    if (level <= 40) return 470;
    return 510;
}

// Fills the twelve land slots of a dungeon floor (the vanilla 20/20/10/10/
// 10/10/5/5/4/4/1/1 odds, six species spread over them, a rare one in the
// last two). Returns the encounter rate, or 0 for none.
u32 RogueDungeon_FloorWild(u32 dungeon, u32 floor, struct WildPokemon *mons)
{
    static const u8 slotSpecies[12] = { 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5 };
    const struct RogueRegion *r = Rogue_GetRegion();
    const struct RogueDungeon *d;
    struct RoguePlace fp;
    const u16 *list, *rare;
    u16 base[6], species[6];
    u32 n, i, rng, level, cap, rate = RogueGen_FloorWildRate(r, dungeon, floor);

    // A legend's chamber has only the legend, as the Regis' have. Tower rooms
    // have their Pokemon, as Mt. Pyre's floors.
    if (rate == 0)
        return 0;
    d = &r->dungeons[dungeon];
    RogueGen_FloorPlace(r, dungeon, floor, &fp);
    level = RogueGen_WildLevel(fp.level);
    cap = WildMaxBst(level);
    // RUN 0: the band's levels, and what they evolve (the species are drawn as before).
    level = Rogue_Run0WildLevel(fp.level, level);
    KindLists(r, d, &list, &n, &rare);
    rng = RogueRng_Hash(r->seed, 13500 + dungeon * 8 + floor);
    for (i = 0; i < 6; i++)
    {
        u32 tries;
        u16 s = list[0];
        for (tries = 0; tries < 40; tries++)
        {
            s = PickFrom(&rng, list, n);
            if (!IsSpeciesEnabled(s) || InList(base, i, s))
                continue;
            if (tries < 30 && (!Rogue_WildFits(s, &fp) || Bst(s) > cap))
                continue;
            break;
        }
        // Ruins: one of Baltoy, Sigilyph and Bronzor on each floor, and
        // Unown in its own slots.
        if (d->kind == RDUN_RUINS && i == 0)
            s = sRuinsWild[(floor + dungeon) % RUINS_SIGNATURE];
        if (d->kind == RDUN_RUINS && i == 5 && IsSpeciesEnabled(SPECIES_UNOWN) && !InList(base, i, SPECIES_UNOWN))
            s = SPECIES_UNOWN;
        base[i] = s;
        // What the level allows has evolved, as Victory Road's Golbat and
        // Hariyama.
        species[i] = Rogue_EvolveForLevel(s, level, Rogue_IsRun0());
    }
    for (i = 0; i < 12; i++)
    {
        mons[i].species = species[slotSpecies[i]];
        mons[i].minLevel = level + (i % 2) + RogueGen_RareSlotBonus(i, level);
        mons[i].maxLevel = level + 2 + (i % 2) + RogueGen_RareSlotBonus(i, level);
    }
    // The rare slot, the same through the dungeon.
    rng = RogueRng_Hash(r->seed, 13600 + dungeon);
    for (i = 0; i < 12; i++)
    {
        u16 s = PickFrom(&rng, rare, 6);
        if (IsSpeciesEnabled(s) && Rogue_WildFits(s, &fp) && !InList(base, 6, s))
        {
            mons[10].species = mons[11].species = s;
            break;
        }
    }
    return rate;
}

// ---------------------------------------------------------------------------
// Guardians and legends

static const u16 sCaveGuardians[] =
{
    SPECIES_ONIX, SPECIES_ARON, SPECIES_GEODUDE, SPECIES_LARVITAR, SPECIES_SABLEYE, SPECIES_MAWILE,
    SPECIES_RHYHORN, SPECIES_MACHOP, SPECIES_MAKUHITA, SPECIES_NOSEPASS, SPECIES_ROGGENROLA, SPECIES_DRILBUR,
    SPECIES_DRUDDIGON, SPECIES_SHUCKLE, SPECIES_ZUBAT, SPECIES_GLIGAR,
};
static const u16 sVolcanoGuardians[] =
{
    SPECIES_SLUGMA, SPECIES_NUMEL, SPECIES_TORKOAL, SPECIES_HOUNDOUR, SPECIES_MAGBY, SPECIES_HEATMOR,
    SPECIES_ONIX, SPECIES_GEODUDE,
};
static const u16 sSeaCaveGuardians[] =
{
    SPECIES_LAPRAS, SPECIES_SEEL, SPECIES_SPHEAL, SPECIES_SHELLDER, SPECIES_SNORUNT, SPECIES_SWINUB, SPECIES_ONIX,
};
static const u16 sForestGuardians[] =
{
    SPECIES_HERACROSS, SPECIES_PINSIR, SPECIES_SHROOMISH, SPECIES_SEEDOT, SPECIES_SCYTHER, SPECIES_PINECO,
    SPECIES_VENONAT, SPECIES_PARAS, SPECIES_ODDISH, SPECIES_TANGELA, SPECIES_FOONGUS, SPECIES_KARRABLAST,
    SPECIES_SEWADDLE, SPECIES_SNORLAX, SPECIES_SUDOWOODO,
};
static const u16 sTowerGuardians[] =
{
    SPECIES_GASTLY, SPECIES_DUSKULL, SPECIES_SHUPPET, SPECIES_MISDREAVUS, SPECIES_LITWICK, SPECIES_DRIFLOON,
    SPECIES_YAMASK, SPECIES_PHANTUMP, SPECIES_HONEDGE, SPECIES_GOLETT, SPECIES_ABRA, SPECIES_NATU,
    SPECIES_DROWZEE, SPECIES_SPIRITOMB, SPECIES_CUBONE,
};
static const u16 sVictoryGuardians[] =
{
    SPECIES_MACHOP, SPECIES_MAKUHITA, SPECIES_ONIX, SPECIES_ARON, SPECIES_GIBLE, SPECIES_AXEW,
    SPECIES_DRUDDIGON, SPECIES_BAGON, SPECIES_LARVITAR,
};
// The ice cave's, Glalie and Walrein most of all (Shoal Cave's Snorunt and
// Spheal grown up), now and then Mamoswine or Abomasnow: always the grown
// form, however early, as the other kinds' guardians stand out.
static const u16 sIceGuardians[] =
{
    SPECIES_GLALIE, SPECIES_WALREIN, SPECIES_GLALIE, SPECIES_WALREIN, SPECIES_MAMOSWINE, SPECIES_ABOMASNOW,
};
// The volcano's: Torkoal, Camerupt, Magcargo, Magmar, Houndoom.
static const u16 sVolcanoDungeonGuardians[] =
{
    SPECIES_TORKOAL, SPECIES_CAMERUPT, SPECIES_MAGCARGO, SPECIES_MAGMAR, SPECIES_HOUNDOOM, SPECIES_TORKOAL,
};
// What haunts a wreck's hold, as the Abandoned Ship's ghost stories; what
// guards a power plant's generator, as the Power Plant's Zapdos and New
// Mauville's Electrode.
// Both are fully grown already: a ghost in the hold, an Electrode or a
// Magneton by the generator.
static const u16 sShipGuardians[] =
{
    SPECIES_DUSCLOPS, SPECIES_BANETTE, SPECIES_JELLICENT, SPECIES_DHELMISE, SPECIES_DRIFBLIM, SPECIES_GENGAR,
};
static const u16 sPlantGuardians[] =
{
    SPECIES_ELECTRODE, SPECIES_MAGNETON, SPECIES_ELECTRODE, SPECIES_MAGNEZONE, SPECIES_MAGNETON, SPECIES_MANECTRIC,
};
// A ruins vault's guardian is of the old kind, as the Regis are: clay,
// bronze and stone golems, a mummy, a sentinel bird.
static const u16 sRuinsGuardians[] =
{
    SPECIES_BALTOY, SPECIES_BRONZOR, SPECIES_GOLETT, SPECIES_SIGILYPH, SPECIES_YAMASK, SPECIES_NOSEPASS,
    SPECIES_LUNATONE, SPECIES_SOLROCK,
};
// (v1.1 peaks) The desert tower's: Flygon, Sandslash, Cacturne, Krookodile,
// Hippowdon, Drapion, Gliscor, Sandaconda as the level allows.
static const u16 sMirageGuardians[] =
{
    SPECIES_TRAPINCH, SPECIES_SANDSHREW, SPECIES_CACNEA, SPECIES_SANDILE, SPECIES_HIPPOPOTAS, SPECIES_SKORUPI,
    SPECIES_GLIGAR, SPECIES_SILICOBRA,
};
static const u16 sHideoutGuardians[] =
{
    SPECIES_KOFFING, SPECIES_GRIMER, SPECIES_MAGNEMITE, SPECIES_VOLTORB, SPECIES_HOUNDOUR, SPECIES_SNEASEL,
};

// The legends' species (enum RogueLegend; the region's set and where each
// sleeps are planned in src/rogue_gen_legend.h).
static const u16 sLegendSpecies[RLEG_COUNT] =
{
    [RLEG_NONE]      = SPECIES_REGIROCK,
    [RLEG_REGIROCK]  = SPECIES_REGIROCK,  [RLEG_REGICE]    = SPECIES_REGICE,    [RLEG_REGISTEEL] = SPECIES_REGISTEEL,
    [RLEG_REGIGIGAS] = SPECIES_REGIGIGAS,
    [RLEG_ARTICUNO]  = SPECIES_ARTICUNO,  [RLEG_ZAPDOS]    = SPECIES_ZAPDOS,    [RLEG_MOLTRES]   = SPECIES_MOLTRES,
    [RLEG_RAIKOU]    = SPECIES_RAIKOU,    [RLEG_ENTEI]     = SPECIES_ENTEI,     [RLEG_SUICUNE]   = SPECIES_SUICUNE,
    [RLEG_UXIE]      = SPECIES_UXIE,      [RLEG_MESPRIT]   = SPECIES_MESPRIT,   [RLEG_AZELF]     = SPECIES_AZELF,
    [RLEG_COBALION]  = SPECIES_COBALION,  [RLEG_TERRAKION] = SPECIES_TERRAKION, [RLEG_VIRIZION]  = SPECIES_VIRIZION,
    [RLEG_TORNADUS]  = SPECIES_TORNADUS,  [RLEG_THUNDURUS] = SPECIES_THUNDURUS, [RLEG_LANDORUS]  = SPECIES_LANDORUS,
    [RLEG_LATIAS]    = SPECIES_LATIAS,    [RLEG_LATIOS]    = SPECIES_LATIOS,
    [RLEG_HEATRAN]   = SPECIES_HEATRAN,   [RLEG_CRESSELIA] = SPECIES_CRESSELIA,
    [RLEG_MEWTWO]    = SPECIES_MEWTWO,    [RLEG_LUGIA]     = SPECIES_LUGIA,     [RLEG_HO_OH]     = SPECIES_HO_OH,
    [RLEG_KYOGRE]    = SPECIES_KYOGRE,    [RLEG_GROUDON]   = SPECIES_GROUDON,   [RLEG_RAYQUAZA]  = SPECIES_RAYQUAZA,
};

static u16 LegendSpecies(const struct RogueRegion *r, u32 dungeon)
{
    return sLegendSpecies[r->dungeons[dungeon].legend % RLEG_COUNT];
}

u16 Rogue_LegendSpecies(u32 legend)
{
    return sLegendSpecies[legend % RLEG_COUNT];
}

s32 Rogue_LegendDungeon(u32 legend)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    u32 i;
    for (i = 0; i < r->nDungeons; i++)
        if (r->dungeons[i].boss == RBOSS_LEGEND && r->dungeons[i].legend == legend)
            return i;
    return -1;
}

u32 Rogue_RegionLegends(bool32 leftOnly)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    u32 i, bits = 0;
    for (i = 0; i < r->nDungeons; i++)
        if (r->dungeons[i].boss == RBOSS_LEGEND && r->dungeons[i].legend < RLEG_COUNT
         && !(leftOnly && Rogue_IsDungeonCleared(i)))
            bits |= 1u << r->dungeons[i].legend;
    return bits;
}

// A legend is as strong as the gym ace of the badges held when the player
// meets it, plus a little: Emerald's Regis and Latis are 40, near its seventh
// ace. A box-art legend a few more. It is set when met, not when its way
// opens, so a key won early never leaves it far under a late team; never
// under the badges its way opens with.
static u32 LegendLevel(const struct RogueRegion *r, const struct RogueDungeon *d)
{
    u32 badges = RogueGen_LegendGateBadges(r, d - r->dungeons), level;
    if (badges < Rogue_BadgeCount())
        badges = Rogue_BadgeCount();
    if (badges > ROGUE_NUM_GYMS)
        badges = ROGUE_NUM_GYMS;
    level = r->aceLevel[badges - 1] + (d->legend >= RLEG_MEWTWO ? 6 : 2);
    if (level < LastFloorLevel(r, d) + 3)
        level = LastFloorLevel(r, d) + 3;
    return level;
}

// A guardian is the strongest thing in its dungeon, but no wall: about
// three levels over the dungeon's first floor, never under its deepest
// floor's trainers' level (Emerald's statics, Kecleon, Electrode, and FRLG's
// Snorlax, stand at the level of the place around them), and never more
// than two over the next gym ace. Its totem boost does the rest.
static u32 GuardianLevel(const struct RogueRegion *r, const struct RogueDungeon *d)
{
    u32 ace = NextAce(r, d), level = FirstFloorLevel(r, d) + 3;
    if (level < LastFloorLevel(r, d))
        level = LastFloorLevel(r, d);
    // An optional dungeon off the first towns, before a team has had time
    // to grow (no badge or one): the guardian a level over the last
    // floor's, as FRLG's early woods and caves stay near their routes.
    if (!d->required && DungeonBadges(d) <= 1 && level > LastFloorLevel(r, d) + 1)
        level = LastFloorLevel(r, d) + 1;
    if (level > ace + 2)
        level = ace + 2;
    return level;
}

static void GuardianList(const struct RogueRegion *r, const struct RogueDungeon *d, const u16 **list, u32 *n)
{
    u32 theme = r->places[d->host].theme;
    switch (d->kind)
    {
    case RDUN_VICTORY: *list = sVictoryGuardians, *n = ARRAY_COUNT(sVictoryGuardians); break;
    case RDUN_FOREST:  *list = sForestGuardians, *n = ARRAY_COUNT(sForestGuardians); break;
    case RDUN_TOWER:   *list = sTowerGuardians, *n = ARRAY_COUNT(sTowerGuardians); break;
    case RDUN_HIDEOUT: *list = sHideoutGuardians, *n = ARRAY_COUNT(sHideoutGuardians); break;
    case RDUN_RUINS:   *list = sRuinsGuardians, *n = ARRAY_COUNT(sRuinsGuardians); break;
    case RDUN_MIRAGE:  *list = sMirageGuardians, *n = ARRAY_COUNT(sMirageGuardians); break;
    case RDUN_SHIP:    *list = sShipGuardians, *n = ARRAY_COUNT(sShipGuardians); break;
    case RDUN_PLANT:   *list = sPlantGuardians, *n = ARRAY_COUNT(sPlantGuardians); break;
    case RDUN_ICE:     *list = sIceGuardians, *n = ARRAY_COUNT(sIceGuardians); break;
    case RDUN_VOLCANO: *list = sVolcanoDungeonGuardians, *n = ARRAY_COUNT(sVolcanoDungeonGuardians); break;
    default:
        if (ThemeVolcanic(theme))
            *list = sVolcanoGuardians, *n = ARRAY_COUNT(sVolcanoGuardians);
        else if (ThemeSea(theme) && RogueRng_Hash(r->seed, 13400 + (d - r->dungeons)) % 2)
            *list = sSeaCaveGuardians, *n = ARRAY_COUNT(sSeaCaveGuardians);
        else
            *list = sCaveGuardians, *n = ARRAY_COUNT(sCaveGuardians);
        break;
    }
}

static u16 GuardianSpecies(const struct RogueRegion *r, u32 dungeon, u32 level)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon];
    const u16 *list;
    u32 n, tries, rng = RogueRng_Hash(r->seed, 13000 + dungeon), hi;
    u16 species = SPECIES_ONIX, evolved = SPECIES_ONIX;

    GuardianList(r, d, &list, &n);
    hi = level <= 15 ? 410 : level <= 22 ? 460 : level <= 30 ? 500 : level <= 40 ? 540 : 600;
    for (tries = 0; tries < 60; tries++)
    {
        u32 bst;
        species = PickFrom(&rng, list, n);
        if (!IsSpeciesEnabled(species))
            continue;
        // Evolved a little ahead of its level, as a totem Pokemon is.
        evolved = Rogue_EvolveForLevel(species, level + 8, TRUE);
        bst = Bst(evolved);
        // Something that stands out: evolved as far as the level allows,
        // within the band, and not one of the late lines too early. A
        // wreck's ghost and a power plant's guardian are their lists' own
        // grown forms, as New Mauville's Electrode, whatever the band.
        if (d->kind == RDUN_SHIP || d->kind == RDUN_PLANT)
            bst = hi;
        if (tries < 50 && (bst > hi || bst + 150 < hi))
            continue;
        if (tries < 40 && !Rogue_LineFitsLevel(species, level + 5))
            continue;
        return evolved;
    }
    return evolved;
}

// The item a guardian holds, its type's: catching it brings the item along.
static u16 TypeItem(u16 species)
{
    switch (GetSpeciesType(species, 0))
    {
    case TYPE_ROCK:     return ITEM_HARD_STONE;
    case TYPE_GROUND:   return ITEM_SOFT_SAND;
    case TYPE_STEEL:    return ITEM_METAL_COAT;
    case TYPE_DARK:     return ITEM_BLACK_GLASSES;
    case TYPE_POISON:   return ITEM_POISON_BARB;
    case TYPE_FIGHTING: return ITEM_BLACK_BELT;
    case TYPE_GHOST:    return ITEM_SPELL_TAG;
    case TYPE_BUG:      return ITEM_SILVER_POWDER;
    case TYPE_GRASS:    return ITEM_MIRACLE_SEED;
    case TYPE_FIRE:     return ITEM_CHARCOAL;
    case TYPE_WATER:    return ITEM_MYSTIC_WATER;
    case TYPE_ELECTRIC: return ITEM_MAGNET;
    case TYPE_PSYCHIC:  return ITEM_TWISTED_SPOON;
    case TYPE_ICE:      return ITEM_NEVER_MELT_ICE;
    case TYPE_DRAGON:   return ITEM_DRAGON_FANG;
    case TYPE_FLYING:   return ITEM_SHARP_BEAK;
    case TYPE_FAIRY:    return ITEM_FAIRY_FEATHER;
    default:            return ITEM_SILK_SCARF;
    }
}

// A dungeon's boss Pokemon (a guardian or the legend): species, level and
// held item.
static void BossMon(u32 dungeon, u16 *species, u8 *level, u16 *item)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    const struct RogueDungeon *d = &r->dungeons[dungeon % ROGUE_MAX_DUNGEONS];
    if (d->boss == RBOSS_LEGEND)
    {
        *species = LegendSpecies(r, dungeon % ROGUE_MAX_DUNGEONS);
        *level = LegendLevel(r, d);
        if (Rogue_Run0Scaled())
            *level = Rogue_Run0MapLevel(*level) + 2;
        *item = ITEM_NONE;
        return;
    }
    *level = GuardianLevel(r, d);
    *species = GuardianSpecies(r, dungeon % ROGUE_MAX_DUNGEONS, *level);
    *item = TypeItem(*species);
    // RUN 0 (and ENDLESS with a carried party): at the band's level, as far
    // as it evolves there.
    if (Rogue_Run0Scaled())
    {
        *level = Rogue_Run0MapLevel(*level) + 2;
        *species = Rogue_EvolveForLevel(*species, *level, TRUE);
    }
}

u16 RogueDungeon_BossSpecies(u32 dungeon)
{
    u16 species, item;
    u8 level;
    BossMon(dungeon, &species, &level, &item);
    return species;
}

// ---------------------------------------------------------------------------
// The ways to the legends (enum RogueLegendGate)
//
// What keeps a chamber shut stands at its narrow place, or where the legend
// appears (ROBJ_SEAL). Once opened it stays open for the run
// (VAR_ROGUE_SEALS, a bit a dungeon); the villain leader's way opens when
// he is beaten.

// The team whose leader keeps a legend's way (its gate's src).
static u32 GateTeam(const struct RogueDungeon *d)
{
    return ROGUE_GATE_SRC(d->gate);
}

static const u8 *const sTeamNames[ROGUE_VILLAIN_IDS] =
{
    COMPOUND_STRING("TEAM AQUA"), COMPOUND_STRING("TEAM MAGMA"), COMPOUND_STRING("TEAM ROCKET"),
};

static const u8 *TeamName(const struct RogueRegion *r, u32 team)
{
    return sTeamNames[RogueGen_TeamId(r, team) % ROGUE_VILLAIN_IDS];
}

bool32 Rogue_IsSealOpened(u32 dungeon)
{
    return (VarGet(VAR_ROGUE_SEALS) >> (dungeon % ROGUE_MAX_DUNGEONS)) & 1;
}

// Is the way to a legend open: nothing in the way, its seal opened, or the
// villain leader before it beaten.
bool32 Rogue_LegendGateOpen(u32 dungeon)
{
    const struct RogueDungeon *d = &Rogue_GetRegion()->dungeons[dungeon % ROGUE_MAX_DUNGEONS];
    switch (ROGUE_GATE_KIND(d->gate))
    {
    case RGATE_OPEN:
        return TRUE;
    case RGATE_VILLAIN:
        return Rogue_TeamLeaderBeaten(GateTeam(d));
    }
    return Rogue_IsSealOpened(dungeon);
}

// Is the seal's villain lookout there: until his leader comes.
bool32 Rogue_SealShown(u32 dungeon)
{
    const struct RogueDungeon *d = &Rogue_GetRegion()->dungeons[dungeon % ROGUE_MAX_DUNGEONS];
    if (ROGUE_GATE_KIND(d->gate) == RGATE_VILLAIN)
        return !Rogue_TeamLeaderReady(GateTeam(d)) && !Rogue_TeamLeaderBeaten(GateTeam(d));
    return !Rogue_IsSealOpened(dungeon);
}

// The other legends of a set, met or not yet, for RGATE_AFTER: their names
// into dest ("REGIROCK, REGICE and REGISTEEL"), unless dest is NULL; with
// unmetOnly, only those not beaten or caught. Returns how many.
static u32 CopySetNames(u8 *dest, u32 dungeon, bool32 unmetOnly)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    const struct RogueDungeon *d = &r->dungeons[dungeon];
    u32 i, n = 0, total = 0;
    s32 picks[4];
    for (i = 0; i < r->nDungeons && total < 4; i++)
    {
        const struct RogueDungeon *o = &r->dungeons[i];
        if (i == dungeon || o->boss != RBOSS_LEGEND || ROGUE_GATE_KIND(o->gate) == RGATE_AFTER)
            continue;
        if (RogueGen_LegendSetOf(o->legend) != RogueGen_LegendSetOf(d->legend))
            continue;
        if (unmetOnly && Rogue_IsDungeonCleared(i))
            continue;
        picks[total++] = i;
    }
    if (dest == NULL)
        return total;
    *dest = EOS;
    for (n = 0; n < total; n++)
    {
        if (n > 0)
            dest = StringCopy(dest, n + 1 == total ? COMPOUND_STRING(" and ") : COMPOUND_STRING(", "));
        dest = StringCopy(dest, GetSpeciesName(LegendSpecies(r, picks[n])));
    }
    return total;
}

// Can the seal open now: the field move's badge, the rite's, the key in
// hand (the gym won that gives it, the prize ball taken), or the rest of
// the set met.
static bool32 SealCanOpen(u32 dungeon)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    const struct RogueDungeon *d = &r->dungeons[dungeon];
    u32 src = ROGUE_GATE_SRC(d->gate);
    switch (ROGUE_GATE_KIND(d->gate))
    {
    case RGATE_FIELD:
        return Rogue_IsFieldMoveBadgeWon(src == ROBST_STRENGTH ? FIELD_MOVE_STRENGTH : FIELD_MOVE_ROCK_SMASH);
    case RGATE_RITUAL:
        return Rogue_IsFieldMoveBadgeWon(src == RRITUAL_FLY ? FIELD_MOVE_FLY : src == RRITUAL_DIVE ? FIELD_MOVE_DIVE : FIELD_MOVE_WATERFALL);
    case RGATE_GIFT:
        return FlagGet(FLAG_BADGE01_GET + src);
    case RGATE_FOUND:
    {
        struct RogueDungeonExtras e;
        RogueGen_DungeonExtras(r, src, &e);
        return e.prize != 0xFF && FlagGet(FLAG_ROGUE_ITEM_0 + e.prize);
    }
    case RGATE_AFTER:
        return CopySetNames(NULL, dungeon, TRUE) == 0;
    }
    return FALSE;
}

static const u8 *const sRitualMoves[] = { COMPOUND_STRING("FLY"), COMPOUND_STRING("DIVE"), COMPOUND_STRING("WATERFALL") };
// The old words on a rite's stone, as the Regis' braille: what to do.
static const u8 *const sRitualWords[] =
{
    COMPOUND_STRING("“Let one who soars through the sky\nstand here, and it will come.”"),
    COMPOUND_STRING("“Let one who goes down into the\nsea stand here, and it will come.”"),
    COMPOUND_STRING("“Let one who climbs the falls\nstand here, and it will come.”"),
};

// The lead's name into STR_VAR_3, for "used ROCK SMASH!".
static void BufferLeadName(void)
{
    u32 i;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_SPECIES) != SPECIES_NONE && !GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_IS_EGG))
        {
            GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_NICKNAME, gStringVar3);
            StringGet_Nickname(gStringVar3);
            return;
        }
    }
    StringCopy(gStringVar3, COMPOUND_STRING("Your POKéMON"));
}

// The place a gate's key comes from into dest: the gym's town, or the
// dungeon whose prize holds it.
static void CopyKeyPlace(u8 *dest, const struct RogueDungeon *d)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    u32 src = ROGUE_GATE_SRC(d->gate);
    if (ROGUE_GATE_KIND(d->gate) == RGATE_GIFT)
        Rogue_CopyPlaceName(dest, r->gymTown[src % ROGUE_NUM_GYMS]);
    else
        Rogue_CopyDungeonName(dest, src);
}

// Script special for a seal (ROBJ_SEAL): what it is and whether it opens.
// STR_VAR_1 the legend, gStringVar4 what is seen or done. VAR_RESULT TRUE if
// it opens now (Rogue_OpenSeal follows); VAR_0x8005 TRUE if the legend then
// appears where the seal stood.
void Rogue_CheckSeal(void)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    s32 dungeon = Rogue_GetFloorDungeon();
    const struct RogueDungeon *d;
    u32 src;
    bool32 open;
    const u8 *text = NULL;

    gSpecialVar_Result = FALSE;
    gSpecialVar_0x8005 = FALSE;
    if (dungeon < 0)
        return;
    d = &r->dungeons[dungeon];
    src = ROGUE_GATE_SRC(d->gate);
    open = SealCanOpen(dungeon);
    gSpecialVar_0x8005 = RogueGen_SealAtLegend(r, dungeon) == TRUE;
    gSpecialVar_0x8004 = LegendSpecies(r, dungeon);
    // Statue and rock rumble as they go; a keeper just steps aside.
    gSpecialVar_0x8007 = ROGUE_GATE_KIND(d->gate) != RGATE_GIFT && ROGUE_GATE_KIND(d->gate) != RGATE_VILLAIN;
    StringCopy(gStringVar1, GetSpeciesName(LegendSpecies(r, dungeon)));
    BufferLeadName();
    switch (ROGUE_GATE_KIND(d->gate))
    {
    case RGATE_FIELD:
        if (src == ROBST_STRENGTH)
            text = open ? COMPOUND_STRING("A huge boulder blocks the way.\p{STR_VAR_3} used STRENGTH!\nThe boulder slid aside!")
                        : COMPOUND_STRING("A huge boulder blocks the way.\nA strong POKéMON could move it.\pA GYM BADGE would let your\nPOKéMON use STRENGTH here.");
        else
            text = open ? COMPOUND_STRING("A cracked rock blocks the way.\p{STR_VAR_3} used ROCK SMASH!\nThe rock crumbled away!")
                        : COMPOUND_STRING("A cracked rock blocks the way.\nIt looks like it could be smashed.\pA GYM BADGE would let your\nPOKéMON use ROCK SMASH here.");
        break;
    case RGATE_RITUAL:
    {
        u8 *end;
        StringCopy(gStringVar2, sRitualMoves[src < 3 ? src : 2]);
        end = StringExpandPlaceholders(gStringVar4, COMPOUND_STRING("An old statue stands here, with\nwords carved on its base:\p"));
        end = StringExpandPlaceholders(end, sRitualWords[src < 3 ? src : 2]);
        if (open)
            StringExpandPlaceholders(end, COMPOUND_STRING("\p{STR_VAR_3} used {STR_VAR_2}!\p...\nThe statue began to glow!"));
        break;
    }
    case RGATE_GIFT:
    case RGATE_FOUND:
        CopyItemName(RogueGen_LegendKey(d->legend), gStringVar2);
        if (open)
            text = ROGUE_GATE_KIND(d->gate) == RGATE_GIFT
                 ? COMPOUND_STRING("Only those with the {STR_VAR_2}\nmay pass here.\pOh! You have it!\nGo on, then.")
                 : COMPOUND_STRING("An old statue blocks the way.\nIts base has a hollow in it.\p{PLAYER} held up the {STR_VAR_2}!\pThe statue sank into the\nground!");
        else
        {
            CopyKeyPlace(gStringVar3, d);
            text = ROGUE_GATE_KIND(d->gate) == RGATE_GIFT
                 ? COMPOUND_STRING("Only those with the {STR_VAR_2}\nmay pass here.\pThey say the GYM LEADER of\n{STR_VAR_3} keeps one.")
                 : COMPOUND_STRING("An old statue blocks the way.\nIts base has a hollow in it.\pOld words: “The {STR_VAR_2} lies\ndeep in {STR_VAR_3}.”");
        }
        break;
    case RGATE_VILLAIN:
        StringCopy(gStringVar2, TeamName(r, src));
        text = RogueGen_TeamId(r, src) == ROGUE_VILLAIN_ROCKET
             ? COMPOUND_STRING("Beat it, kid! {STR_VAR_2} has\nclaimed this place.\pThe BOSS is coming for the\n{STR_VAR_1}. It's worth a fortune!")
             : COMPOUND_STRING("Get lost, kid! {STR_VAR_2} has\nclaimed this place.\pOur boss is coming for the\n{STR_VAR_1}. Scram!");
        open = FALSE;
        break;
    case RGATE_AFTER:
        if (open)
            text = COMPOUND_STRING("An old statue stands here. The\nmarks on its base have gone dark.\p...\nThe statue began to glow!");
        else
        {
            CopySetNames(gStringVar3, dungeon, TRUE);
            text = COMPOUND_STRING("An old statue stands here. Marks\nglow on its base:\p{STR_VAR_3}.\pIt seems to wait for them\nto be met.");
        }
        break;
    }
    if (text != NULL)
        StringExpandPlaceholders(gStringVar4, text);
    gSpecialVar_Result = open;
}

// Script special after Rogue_CheckSeal opened it: it stays open.
void Rogue_OpenSeal(void)
{
    s32 dungeon = Rogue_GetFloorDungeon();
    if (dungeon >= 0)
        VarSet(VAR_ROGUE_SEALS, VarGet(VAR_ROGUE_SEALS) | (1 << dungeon));
}

// What the townsfolk say opens a legend's way, into STR_VAR_1. FALSE if
// there is nothing to tell (an open way).
static bool32 BufferGateRumor(u32 dungeon, u32 place, u32 rng)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    Rogue_TextCtx(&c, &st, place, RSPK_FOLK);
    c.dungeon = dungeon;
    DungeonText(gStringVar1, RogueText_GateRumor(&c, rng, (char *)gStringVar1, sizeof(gStringVar1)));
    return gStringVar1[0] != EOS;
}

// Script special after a gym leader's TM: the key the leader keeps for a
// legend's way, as the Eon Ticket. VAR_RESULT TRUE if there is one, the
// item in VAR_0x8000 and the place it opens in STR_VAR_1.
void Rogue_PrepareGymKey(u32 gym)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    u32 i;
    gSpecialVar_Result = FALSE;
    for (i = 0; i < r->nDungeons; i++)
    {
        const struct RogueDungeon *d = &r->dungeons[i];
        if (d->boss == RBOSS_LEGEND && ROGUE_GATE_KIND(d->gate) == RGATE_GIFT && ROGUE_GATE_SRC(d->gate) == gym
         && RogueGen_LegendKey(d->legend) != ITEM_NONE && !CheckBagHasItem(RogueGen_LegendKey(d->legend), 1))
        {
            struct RogueLore lore;
            gSpecialVar_0x8000 = RogueGen_LegendKey(d->legend);
            Rogue_CopyDungeonName(gStringVar1, i);
            CopyItemName(gSpecialVar_0x8000, gStringVar2);
            // The leader tells why it rests there, as the one who keeps its
            // key would know.
            gStringVar3[0] = EOS;
            RogueGen_LegendLore(r, i, &lore);
            if (lore.story == LORE_WAY_LEADER)
                Rogue_BufferLore(StringCopy(gStringVar3, COMPOUND_STRING("\p")), i, LORE_STORY, LORE_WAY_LEADER, 250);
            gSpecialVar_Result = TRUE;
            return;
        }
    }
}

// The key a dungeon's prize ball holds instead of its prize, or ITEM_NONE.
u16 Rogue_PrizeKey(u32 dungeon)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    u32 i;
    for (i = 0; i < r->nDungeons; i++)
        if (r->dungeons[i].boss == RBOSS_LEGEND && ROGUE_GATE_KIND(r->dungeons[i].gate) == RGATE_FOUND
         && ROGUE_GATE_SRC(r->dungeons[i].gate) == dungeon)
            return RogueGen_LegendKey(r->dungeons[i].legend);
    return ITEM_NONE;
}

// The first legend and the place its dungeon is entered from, for rumors
// (the professor, townsfolk). FALSE if this region has none.
bool32 Rogue_GetLegendRumor(u16 *species, u8 *place)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    s32 dungeon = RogueGen_LegendDungeon(r);
    if (dungeon < 0)
        return FALSE;
    *species = LegendSpecies(r, dungeon);
    *place = r->dungeons[dungeon].host;
    return TRUE;
}

// How a legend shows itself, by its nature (LELEM_*).
static const u8 *const sLegendOmens[LELEM_COUNT] =
{
    [LELEM_FIRE]  = COMPOUND_STRING("The air shimmers with heat...\pThe legendary {STR_VAR_1} appeared!"),
    [LELEM_ICE]   = COMPOUND_STRING("A freezing wind howls...\pThe legendary {STR_VAR_1} appeared!"),
    [LELEM_WATER] = COMPOUND_STRING("A cool mist rises from the ground...\pThe legendary {STR_VAR_1} appeared!"),
    [LELEM_STORM] = COMPOUND_STRING("Thunder rumbles... The air crackles!\pThe legendary {STR_VAR_1} appeared!"),
    [LELEM_SKY]   = COMPOUND_STRING("A great gust of wind sweeps by...\pThe legendary {STR_VAR_1} appeared!"),
    [LELEM_STONE] = COMPOUND_STRING("The ground begins to shake...\pThe legendary {STR_VAR_1} appeared!"),
    [LELEM_GRASS] = COMPOUND_STRING("A sweet scent fills the air...\pThe legendary {STR_VAR_1} appeared!"),
    [LELEM_MIND]  = COMPOUND_STRING("The air grows still...\pThe legendary {STR_VAR_1} appeared!"),
};

// Script special for the boss Pokemon the player is facing: VAR_0x8004 its
// species, VAR_0x8005 its level, VAR_0x8006 its boss flag, VAR_0x8007 its
// held item; VAR_RESULT TRUE for the legend. The intro line goes to
// gStringVar4 and the species name to STR_VAR_1.
void Rogue_PrepareBoss(void)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    s32 dungeon = Rogue_GetCurrentDungeon();
    const struct RogueDungeon *d;
    u16 species, item;
    u8 level;

    if (dungeon < 0)
        dungeon = 0;
    d = &r->dungeons[dungeon];
    BossMon(dungeon, &species, &level, &item);
    gSpecialVar_0x8004 = species;
    gSpecialVar_0x8005 = level;
    gSpecialVar_0x8006 = FLAG_ROGUE_BOSS_0 + dungeon;
    gSpecialVar_0x8007 = item;
    gSpecialVar_Result = d->boss == RBOSS_LEGEND;
    StringCopy(gStringVar1, GetSpeciesName(species));
    if (d->boss == RBOSS_LEGEND)
        Rogue_RecordLegendSeen(d->legend);
    // The way is shut but the legend was reached all the same (the villain
    // leader not beaten yet, if the room let the player round him): it
    // keeps its distance.
    if (d->boss == RBOSS_LEGEND && !Rogue_LegendGateOpen(dungeon))
    {
        gSpecialVar_Result = 2;
        StringCopy(gStringVar2, TeamName(r, GateTeam(d)));
        StringExpandPlaceholders(gStringVar4, ROGUE_GATE_KIND(d->gate) == RGATE_VILLAIN
            ? COMPOUND_STRING("The {STR_VAR_1} eyes you warily.\p{STR_VAR_2}'s leader is after it.\nIt won't let anyone near.")
            : COMPOUND_STRING("The {STR_VAR_1} eyes you warily.\nIt won't let anyone near yet."));
    }
    else if (d->boss == RBOSS_LEGEND)
    {
        // A sign of what it is before it shows itself: a cold wind, the
        // ground shaking.
        struct RogueLore lore;
        RogueGen_LegendLore(r, dungeon, &lore);
        StringExpandPlaceholders(gStringVar4, sLegendOmens[lore.elem < LELEM_COUNT ? lore.elem : LELEM_STONE]);
    }
    else
    {
        // How the guardian shows itself, by the dungeon's kind
        // (src/rogue_text_dungeons.h).
        struct RogueTextCtx c;
        struct RogueTextState st;
        char name[POKEMON_NAME_LENGTH + 4];
        DungeonCtx(&c, &st, dungeon, RSPK_NONE);
        Rogue_GameToAscii(name, GetSpeciesName(species), POKEMON_NAME_LENGTH);
        c.name = name;
        DungeonText(gStringVar4, RogueText_GuardianAppears(&c, (char *)gStringVar4, sizeof(gStringVar4)));
    }
}

// After the battle: the line for a boss beaten (not caught), in gStringVar4.
void Rogue_BufferBossGone(void)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    char name[POKEMON_NAME_LENGTH + 4];
    DungeonCtx(&c, &st, Rogue_GetCurrentDungeon(), RSPK_NONE);
    Rogue_GameToAscii(name, gStringVar1, POKEMON_NAME_LENGTH);
    c.name = name;
    DungeonText(gStringVar4, RogueText_BossGone(&c, gSpecialVar_Result != 0, (char *)gStringVar4, sizeof(gStringVar4)));
}

// Creates the boss Pokemon set up by Rogue_PrepareBoss. A guardian is boosted
// as a totem Pokemon is: it starts with its best stats raised. A legend needs
// no help.
// A boss knows an attack of each of its types, as the legends of the real
// games do when met. Its last four level-up moves may have none: Zapdos at
// 34 knows Pluck, Agility, Ancient Power and Charge, Articuno at 29 no
// Flying move. It then learns the latest attack of that type it would know
// a little past its level (Discharge at 45, Gust), over a move that does no
// damage, or else one of no type of its own.
static void GiveTypeAttacks(struct Pokemon *mon)
{
    u16 species = GetMonData(mon, MON_DATA_SPECIES);
    u32 level = GetMonData(mon, MON_DATA_LEVEL), t, k;
    const struct LevelUpMove *learnset = GetSpeciesLevelUpLearnset(species);
    enum Type types[2] = { GetSpeciesType(species, 0), GetSpeciesType(species, 1) };

    for (t = 0; t < 2; t++)
    {
        u16 moves[MAX_MON_MOVES], best = MOVE_NONE;
        s32 slot = -1;
        if (t == 1 && types[1] == types[0])
            break;
        for (k = 0; k < MAX_MON_MOVES; k++)
            moves[k] = GetMonData(mon, MON_DATA_MOVE1 + k);
        for (k = 0; k < MAX_MON_MOVES; k++)
            if (moves[k] != MOVE_NONE && GetMovePower(moves[k]) > 0 && GetMoveType(moves[k]) == types[t])
                break;
        if (k < MAX_MON_MOVES)
            continue;
        for (k = 0; learnset[k].move != LEVEL_UP_MOVE_END && learnset[k].level <= level + 11; k++)
            if (GetMoveType(learnset[k].move) == types[t] && GetMovePower(learnset[k].move) > 0)
                best = learnset[k].move;
        if (best == MOVE_NONE)
            continue;
        for (k = 0; k < MAX_MON_MOVES && slot < 0; k++)
            if (moves[k] == MOVE_NONE || GetMovePower(moves[k]) == 0)
                slot = k;
        for (k = 0; k < MAX_MON_MOVES && slot < 0; k++)
            if (GetMoveType(moves[k]) != types[0] && GetMoveType(moves[k]) != types[1])
                slot = k;
        if (slot >= 0)
            SetMonMoveSlot(mon, best, slot);
    }
}

// A move that costs its user HP or faints it: recoil, Explosion, Memento.
static bool32 HurtsUser(u16 move)
{
    switch (GetMoveEffect(move))
    {
    case EFFECT_RECOIL:
    case EFFECT_RECOIL_IF_MISS:
    case EFFECT_MAX_HP_50_RECOIL:
    case EFFECT_CHLOROBLAST:
    case EFFECT_MEMENTO:
    case EFFECT_FINAL_GAMBIT:
    case EFFECT_HEALING_WISH:
    case EFFECT_LUNAR_DANCE:
        return TRUE;
    default:
        return IsExplosionMove(move);
    }
}

// A legend is there to be caught: a move that would wear it down or faint
// it on its own (Cobalion's Take Down) gives way to the latest other move it
// learns by its level, so only the player's attacks bring it low.
static void DropSelfHarm(struct Pokemon *mon)
{
    const struct LevelUpMove *learnset = GetSpeciesLevelUpLearnset(GetMonData(mon, MON_DATA_SPECIES));
    u32 level = GetMonData(mon, MON_DATA_LEVEL), slot;
    s32 k;

    for (slot = 0; slot < MAX_MON_MOVES; slot++)
    {
        u16 move = GetMonData(mon, MON_DATA_MOVE1 + slot);
        if (move == MOVE_NONE || !HurtsUser(move))
            continue;
        for (k = 0; learnset[k].move != LEVEL_UP_MOVE_END && learnset[k].level <= level; k++)
            ;
        for (k--; k >= 0; k--)
        {
            u32 j;
            for (j = 0; j < MAX_MON_MOVES && GetMonData(mon, MON_DATA_MOVE1 + j) != learnset[k].move; j++)
                ;
            if (j == MAX_MON_MOVES && !HurtsUser(learnset[k].move))
                break;
        }
        if (k >= 0)
            SetMonMoveSlot(mon, learnset[k].move, slot);
    }
}

void Rogue_CreateBossMon(void)
{
    const struct SpeciesInfo *info = &gSpeciesInfo[gSpecialVar_0x8004];
    u32 physical = info->baseAttack >= info->baseSpAttack;
    CreateScriptedWildMon(gSpecialVar_0x8004, gSpecialVar_0x8005, gSpecialVar_0x8007);
    GiveTypeAttacks(&gParties[B_TRAINER_OPPONENT_A][0]);
    memset(&gQueuedStatBoosts[B_POSITION_OPPONENT_LEFT], 0, sizeof(gQueuedStatBoosts[0]));
    if (gSpecialVar_Result)
    {
        DropSelfHarm(&gParties[B_TRAINER_OPPONENT_A][0]);
        return;
    }
    gQueuedStatBoosts[B_POSITION_OPPONENT_LEFT].stats = (1 << (physical ? STAT_ATK : STAT_SPATK)) | (1 << STAT_DEF) | (1 << STAT_SPDEF);
    gQueuedStatBoosts[B_POSITION_OPPONENT_LEFT].statChanges[physical ? STAT_ATK : STAT_SPATK] = 1;
    gQueuedStatBoosts[B_POSITION_OPPONENT_LEFT].statChanges[STAT_DEF] = 1;
    gQueuedStatBoosts[B_POSITION_OPPONENT_LEFT].statChanges[STAT_SPDEF] = 1;
}

// Starts the boss battle: the Regis' own transition and music, the other
// legends FRLG's legend theme, a guardian Emerald's Regi battle. These
// battles skip the one-catch rule (Rogue_OnWildBattleStart), so the ball is
// always free and the area stays open.
void Rogue_StartBossBattle(void)
{
    u16 species = gSpecialVar_0x8004;
    if (gSpecialVar_Result && species != SPECIES_REGIROCK && species != SPECIES_REGICE && species != SPECIES_REGISTEEL)
        BattleSetup_StartLegendaryBattle();
    else
        StartRegiBattle();
}

// The boss the player faced is beaten or caught, and stays gone.
void Rogue_BossBeaten(void)
{
    s32 dungeon = Rogue_GetCurrentDungeon();
    if (dungeon < 0)
        return;
    Rogue_RecordDungeonCleared(dungeon);
    // A legend caught goes into the LEGEND LOG, across runs.
    if (gBattleOutcome == B_OUTCOME_CAUGHT && Rogue_GetRegion()->dungeons[dungeon].boss == RBOSS_LEGEND)
        Rogue_RecordLegendCaught(Rogue_GetRegion()->dungeons[dungeon].legend);
}

// ---------------------------------------------------------------------------
// Dungeon masters

// A master by dungeon kind: the class (an index into src/rogue.c's
// sClasses, for the slot's lines and sprite there), how they look and what
// they say, and the types of their team.
static const struct RogueMasterDef sMasters[RDUN_COUNT] =
{
    [RDUN_CAVE] = {
        .trainerClass = TRAINER_CLASS_HIKER, .pic = TRAINER_PIC_HIKER, .gfx = OBJ_EVENT_GFX_HIKER, .gender = MALE,
        .music = TRAINER_ENCOUNTER_MUSIC_HIKER, .types = { TYPE_ROCK, TYPE_GROUND },
    },
    [RDUN_VICTORY] = {
        .trainerClass = TRAINER_CLASS_COOLTRAINER, .pic = TRAINER_PIC_COOLTRAINER_F, .gfx = OBJ_EVENT_GFX_WOMAN_5, .gender = FEMALE,
        .music = TRAINER_ENCOUNTER_MUSIC_COOL, .types = { TYPE_NONE, TYPE_DRAGON },
    },
    [RDUN_FOREST] = {
        .trainerClass = TRAINER_CLASS_BUG_MANIAC, .pic = TRAINER_PIC_BUG_MANIAC, .gfx = OBJ_EVENT_GFX_MANIAC, .gender = MALE,
        .music = TRAINER_ENCOUNTER_MUSIC_SUSPICIOUS, .types = { TYPE_BUG, TYPE_GRASS },
    },
    [RDUN_TOWER] = {
        .trainerClass = TRAINER_CLASS_HEX_MANIAC, .pic = TRAINER_PIC_HEX_MANIAC, .gfx = OBJ_EVENT_GFX_HEX_MANIAC, .gender = FEMALE,
        .music = TRAINER_ENCOUNTER_MUSIC_SUSPICIOUS, .types = { TYPE_GHOST, TYPE_PSYCHIC },
    },
    [RDUN_HIDEOUT] = {
        .trainerClass = TRAINER_CLASS_EXPERT, .pic = TRAINER_PIC_EXPERT_M, .gfx = OBJ_EVENT_GFX_EXPERT_M, .gender = MALE,
        .music = TRAINER_ENCOUNTER_MUSIC_INTENSE, .types = { TYPE_POISON, TYPE_DARK },
    },
};

const struct RogueMasterDef *RogueDungeon_MasterDef(u32 dungeon)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    u32 kind = r->dungeons[dungeon % ROGUE_MAX_DUNGEONS].kind;
    // A kind with no master of its own (an ice cave's, a volcano's) has the
    // cave's Hiker.
    return &sMasters[kind < RDUN_COUNT && sMasters[kind].gfx != 0 ? kind : RDUN_CAVE];
}

// The level of a master's ace, near the next gym ace's (or the Elite Four's,
// at Victory Road).
u32 RogueDungeon_MasterLevel(u32 dungeon)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    const struct RogueDungeon *d = &r->dungeons[dungeon % ROGUE_MAX_DUNGEONS];
    u32 level = DungeonBadges(d) >= ROGUE_NUM_GYMS ? r->e4Level[0] - 3 : NextAce(r, d) - 1;
    if (level < LastFloorLevel(r, d) + 2)
        level = LastFloorLevel(r, d) + 2;
    return level;
}

// Script special, after beating a dungeon master: the reward in VAR_0x8000,
// a TM of their kind or a good held item (RogueGen_ItemFor).
void Rogue_PrepareMasterReward(void)
{
    s32 dungeon = Rogue_GetCurrentDungeon();
    gSpecialVar_0x8000 = RogueGen_ItemFor(Rogue_GetRegion(), RITEMS_MASTER, dungeon < 0 ? 0 : dungeon, NULL,
                                          FlagGet(FLAG_ROGUE_HARDCORE) ? ROGUE_ITEMS_HARDCORE : 0);
    if (gSpecialVar_0x8000 == ITEM_NONE)
        gSpecialVar_0x8000 = ITEM_TM_FACADE;
}

// A master's line (RMASTER_INTRO, _DEFEAT, _AFTER: src/rogue_text_dungeons.h)
// into dest, in the game's characters.
u8 *Rogue_BufferMasterLine(u32 dungeon, u32 field, u8 *dest, u32 size)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    DungeonCtx(&c, &st, dungeon, RSPK_TRAINER);
    return DungeonText(dest, RogueText_MasterLine(&c, field, (char *)dest, size));
}

// Script special: what the master says once beaten, into gStringVar4.
void Rogue_BufferMasterAfter(void)
{
    s32 dungeon = Rogue_GetCurrentDungeon();
    Rogue_BufferMasterLine(dungeon < 0 ? 0 : dungeon, RMASTER_AFTER, gStringVar4, sizeof(gStringVar4));
}

// ---------------------------------------------------------------------------
// Rewards

// What dungeon balls hold (the prize, the choice room's and the floors') comes
// from the item catalogue (src/rogue_gen_items.h), through Rogue_PrepareItem.

// TRUE if slot is one of the dungeon's choice room balls.
bool32 RogueDungeon_IsChoice(u32 dungeon, u32 slot)
{
    struct RogueDungeonExtras e;
    RogueGen_DungeonExtras(Rogue_GetRegion(), dungeon % ROGUE_MAX_DUNGEONS, &e);
    return e.nChoice && slot >= e.choice && slot < e.choice + e.nChoice;
}

// Script special, after a choice room's ball is picked up: the others
// vanish, as Mt. Moon's second fossil does. VAR_RESULT is TRUE if they did
// (FALSE when the ball stayed, the bag being full).
void Rogue_ChoiceTaken(void)
{
    const struct MapEvents *events = gMapHeader.events;
    s32 dungeon = Rogue_GetCurrentDungeon();
    u16 taken = GetObjectEventFlagIdByLocalIdAndMap(gSpecialVar_LastTalked, gSaveBlock1Ptr->location.mapNum, gSaveBlock1Ptr->location.mapGroup);
    struct RogueDungeonExtras e;
    u32 i;

    gSpecialVar_Result = FALSE;
    if (dungeon < 0 || !FlagGet(taken))
        return;
    RogueGen_DungeonExtras(Rogue_GetRegion(), dungeon, &e);
    for (i = 0; i < events->objectEventCount; i++)
    {
        const struct ObjectEventTemplate *t = &events->objectEvents[i];
        if (t->flagId != taken && t->flagId >= FLAG_ROGUE_ITEM_0 + e.choice && t->flagId < FLAG_ROGUE_ITEM_0 + e.choice + e.nChoice)
        {
            RemoveObjectEventByLocalIdAndMap(t->localId, gSaveBlock1Ptr->location.mapNum, gSaveBlock1Ptr->location.mapGroup);
            FlagSet(t->flagId);
            gSpecialVar_Result = TRUE;
        }
    }
    // Into the grass in the woods, across the ice, between the graves.
    {
        struct RogueTextCtx c;
        struct RogueTextState st;
        DungeonCtx(&c, &st, dungeon, RSPK_NONE);
        DungeonText(gStringVar4, RogueText_ChoiceGone(&c, e.nChoice > 2, (char *)gStringVar4, sizeof(gStringVar4)));
    }
}

// The old words on a chamber's wall: who rests there and how its way
// opens, as the Sealed Chamber's braille.
static const u8 *const sCarvings[] =
{
    [RGATE_OPEN]    = COMPOUND_STRING("“Here rests {STR_VAR_1}.\nCome, if you are strong.”"),
    [RGATE_FIELD]   = COMPOUND_STRING("“Here rests {STR_VAR_1}.\nIts way is shut with stone.”"),
    [RGATE_RITUAL]  = COMPOUND_STRING("“Here rests {STR_VAR_1}.\nIt comes to one who uses\p{STR_VAR_2} before its stone.”"),
    [RGATE_GIFT]    = COMPOUND_STRING("“Here rests {STR_VAR_1}.\nOnly the {STR_VAR_2} opens\pits way.”"),
    [RGATE_FOUND]   = COMPOUND_STRING("“Here rests {STR_VAR_1}.\nOnly the {STR_VAR_2} opens\pits way.”"),
    [RGATE_VILLAIN] = COMPOUND_STRING("“Here rests {STR_VAR_1}.\nKeep it from those who would\puse its power.”"),
    [RGATE_AFTER]   = COMPOUND_STRING("“Here rests {STR_VAR_1}.\nIt comes once {STR_VAR_2}\phave been met.”"),
};

static const u8 sCarvingIntro[] = _("Old words are carved in the wall.\p");

// Script special for the old words on a chamber's wall, into gStringVar4.
void Rogue_BufferLegendCarving(void)
{
    s32 dungeon = Rogue_GetFloorDungeon();
    const struct RogueRegion *r = Rogue_GetRegion();
    const struct RogueDungeon *d;
    struct RogueLore lore;
    u32 kind;
    u8 *str;
    if (dungeon < 0 || r->dungeons[dungeon].boss != RBOSS_LEGEND)
    {
        StringCopy(gStringVar1, COMPOUND_STRING("an ancient POKéMON"));
        StringExpandPlaceholders(StringCopy(gStringVar4, sCarvingIntro), sCarvings[RGATE_OPEN]);
        return;
    }
    d = &r->dungeons[dungeon];
    kind = ROGUE_GATE_KIND(d->gate);
    StringCopy(gStringVar1, GetSpeciesName(LegendSpecies(r, dungeon)));
    if (kind == RGATE_RITUAL)
        StringCopy(gStringVar2, sRitualMoves[ROGUE_GATE_SRC(d->gate) < 3 ? ROGUE_GATE_SRC(d->gate) : 2]);
    else if (kind == RGATE_GIFT || kind == RGATE_FOUND)
        CopyItemName(RogueGen_LegendKey(d->legend), gStringVar2);
    else if (kind == RGATE_AFTER)
        CopySetNames(gStringVar2, dungeon, FALSE);
    str = StringCopy(gStringVar4, sCarvingIntro);
    // Where the walls tell its story, why it rests here comes first.
    RogueGen_LegendLore(r, dungeon, &lore);
    if (lore.story == LORE_WAY_CARVING)
        str = StringCopy(Rogue_BufferLore(str, dungeon, LORE_STORY, LORE_WAY_CARVING, 480), COMPOUND_STRING("\p"));
    StringExpandPlaceholders(str, sCarvings[kind < ARRAY_COUNT(sCarvings) ? kind : RGATE_OPEN]);
}

// Script special, before a choice room's ball is taken: its item's name
// into STR_VAR_1, for the question, as Kanto's starter balls and Mt. Moon's
// fossils ask. Nothing is given yet.
void Rogue_BufferChoiceName(void)
{
    bool32 fossilFound = FlagGet(FLAG_ROGUE_FOSSIL_FOUND);
    Rogue_PrepareItem();
    if (!fossilFound)
        FlagClear(FLAG_ROGUE_FOSSIL_FOUND);
    CopyItemName(gSpecialVar_0x8000, gStringVar1);
}

// The person beside a choice room's balls: a fossil maniac in a cave, as
// Mt. Moon's, an old man in the woods, an old woman in a tower.
u16 RogueDungeon_GuideGfx(u32 dungeon)
{
    switch (Rogue_GetRegion()->dungeons[dungeon % ROGUE_MAX_DUNGEONS].kind)
    {
    case RDUN_FOREST: return OBJ_EVENT_GFX_OLD_MAN;
    case RDUN_TOWER:  return OBJ_EVENT_GFX_OLD_WOMAN;
    case RDUN_RUINS:  return OBJ_EVENT_GFX_EXPERT_M;
    case RDUN_MIRAGE: return OBJ_EVENT_GFX_HIKER; // (v1.1 peaks) a hiker by the fossils
    default:          return OBJ_EVENT_GFX_SCIENTIST_1;
    }
}

// Script special: what the guide says into gStringVar4; out in a town,
// the keeper by the tower's door.
void Rogue_BufferChoiceGuide(void)
{
    s32 dungeon = Rogue_GetFloorDungeon();
    struct RogueDungeonExtras e;
    struct RogueTextCtx c;
    struct RogueTextState st;
    u32 i, taken = FALSE;
    if (dungeon < 0)
    {
        dungeon = Rogue_GetCurrentDungeon();
        if (dungeon < 0)
            dungeon = 0;
        Rogue_CopyDungeonName(gStringVar1, dungeon);
        StringExpandPlaceholders(gStringVar4, COMPOUND_STRING("This house is the way into\n{STR_VAR_1}.\pThe spirits of POKéMON rest\nthere. A master waits at the top."));
        return;
    }
    RogueGen_DungeonExtras(Rogue_GetRegion(), dungeon, &e);
    for (i = 0; i < e.nChoice; i++)
        if (FlagGet(FLAG_ROGUE_ITEM_0 + e.choice + i))
            taken = TRUE;
    DungeonCtx(&c, &st, dungeon, RSPK_FOLK);
    DungeonText(gStringVar4, RogueText_GuideLine(&c, taken, (char *)gStringVar4, sizeof(gStringVar4)));
}

// The camp's flag: an item flag of the dungeon's, set once the camper has
// healed the team.
static u16 CampFlag(void)
{
    s32 dungeon = Rogue_GetFloorDungeon();
    struct RogueDungeonExtras e;
    if (dungeon < 0)
    {
        // A camper by the road: Victory Road's or LATE CAMP's.
        s32 flag = Rogue_GetCurrentPlace() < 0 ? -1 : RogueGen_RoadCampFlag(Rogue_GetRegion(), Rogue_GetCurrentPlace());
        return flag < 0 ? 0 : FLAG_ROGUE_ITEM_0 + flag;
    }
    RogueGen_DungeonExtras(Rogue_GetRegion(), dungeon, &e);
    return e.camp == 0xFF ? 0 : FLAG_ROGUE_ITEM_0 + e.camp;
}

// Script special: VAR_RESULT TRUE if the camper may still heal the team.
void Rogue_CheckCamp(void)
{
    u16 flag = CampFlag();
    gSpecialVar_Result = flag != 0 && !FlagGet(flag);
}

void Rogue_UseCamp(void)
{
    u16 flag = CampFlag();
    if (flag)
        FlagSet(flag);
}

// Script special: the camper's line VAR_0x8004 (RCAMP_*) into gStringVar4:
// a camper in a cave, one by a pond in the woods, the old caretaker of a
// tower, one by the road (src/rogue_text_dungeons.h).
void Rogue_BufferCampText(void)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    DungeonCtx(&c, &st, Rogue_GetFloorDungeon(), RSPK_FOLK);
    DungeonText(gStringVar4, RogueText_CampLine(&c, gSpecialVar_0x8004 <= RCAMP_USED ? gSpecialVar_0x8004 : RCAMP_OFFER,
                                                (char *)gStringVar4, sizeof(gStringVar4)));
}

// ---------------------------------------------------------------------------
// Ruins: the vault's riddle door (src/rogue_gen_ruins.h)
//
// The vault's inner door is shut as the Sealed Chamber's is: Emerald's own
// cover over the doorway and braille on the door (Rogue_ShutRuinsDoor, as
// the floor loads). The three tablets are the door and the wall either
// side, each with an old word. Read in the order the riddle gives (the
// townsfolk tell it), the door opens with the Sealed Chamber's rumble, and
// stays open for the run (the dungeon's VAR_ROGUE_SEALS bit: ruins have no
// legend's seal). Beaten or caught, the guardian leaves it open too.

static const u8 *const sRuinsWords[RUINS_WORDS] =
{
    COMPOUND_STRING("SUN"), COMPOUND_STRING("MOON"), COMPOUND_STRING("STAR"), COMPOUND_STRING("SAND"),
    COMPOUND_STRING("STONE"), COMPOUND_STRING("SKY"), COMPOUND_STRING("SEA"), COMPOUND_STRING("WIND"),
};

#include "fieldmap.h"
#include "field_camera.h"
#include "field_player_avatar.h"
#include "constants/metatile_labels.h"

extern const u8 Rogue_EventScript_RuinsTablet[];

#define VAR_RUINS_READS VAR_TEMP_4 // tablets read since the floor loaded
#define VAR_RUINS_WRONG VAR_TEMP_5 // one was out of order; RUINS_DARK after a wrong try
// A wrong order puts the tablets out until the player leaves the floor (the
// temp vars clear as a map loads): a small cost, so the six orders are not
// simply tried one after another, and a reason to hear the riddle.
#define RUINS_DARK 2

// The ruins dungeon whose vault the player is in, or -1.
static s32 RuinsVaultHere(void)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    s32 d = Rogue_GetFloorDungeon();
    if (d < 0 || r->dungeons[d].kind != RDUN_RUINS
     || gSaveBlock1Ptr->location.mapNum - MAP_NUM(MAP_ROGUE_FLOOR_1) + 1 != r->dungeons[d].nFloors)
        return -1;
    return d;
}

static bool32 RuinsDoorOpen(u32 dungeon)
{
    return Rogue_IsSealOpened(dungeon) || FlagGet(FLAG_ROGUE_BOSS_0 + dungeon);
}

// The tablets of the loaded map, left to right. Returns how many.
static u32 RuinsTablets(s16 *xs, s16 *ys)
{
    const struct MapEvents *events = gMapHeader.events;
    u32 i, n = 0, k;
    for (i = 0; i < events->bgEventCount && n < 3; i++)
    {
        if (events->bgEvents[i].bgUnion.script != Rogue_EventScript_RuinsTablet)
            continue;
        for (k = n; k > 0 && xs[k - 1] > events->bgEvents[i].x; k--)
        {
            xs[k] = xs[k - 1];
            ys[k] = ys[k - 1];
        }
        xs[k] = events->bgEvents[i].x;
        ys[k] = events->bgEvents[i].y;
        n++;
    }
    return n;
}

// Map script, as a vault floor loads: its door is shut until opened.
void Rogue_ShutRuinsDoor(void)
{
    s16 xs[3], ys[3];
    s32 d = RuinsVaultHere();
    u32 i, n;
    if (d < 0 || RuinsDoorOpen(d))
        return;
    n = RuinsTablets(xs, ys);
    for (i = 0; i < n; i++)
    {
        MapGridSetMetatileIdAt(xs[i] + MAP_OFFSET, ys[i] - 1 + MAP_OFFSET, METATILE_Cave_EntranceCover | MAPGRID_COLLISION_MASK);
        MapGridSetMetatileIdAt(xs[i] + MAP_OFFSET, ys[i] + MAP_OFFSET, METATILE_Cave_SealedChamberBraille_Mid | MAPGRID_COLLISION_MASK);
    }
}

// Script special, after the riddle is solved: the door as the room has it.
void Rogue_OpenRuinsDoor(void)
{
    const struct MapLayout *layout = gMapHeader.mapLayout;
    s16 xs[3], ys[3];
    s32 d = RuinsVaultHere();
    u32 i, n;
    if (d < 0)
        return;
    VarSet(VAR_ROGUE_SEALS, VarGet(VAR_ROGUE_SEALS) | (1 << d));
    n = RuinsTablets(xs, ys);
    for (i = 0; i < n; i++)
    {
        MapGridSetMetatileEntryAt(xs[i] + MAP_OFFSET, ys[i] - 1 + MAP_OFFSET, layout->map[(ys[i] - 1) * layout->width + xs[i]]);
        MapGridSetMetatileEntryAt(xs[i] + MAP_OFFSET, ys[i] + MAP_OFFSET, layout->map[ys[i] * layout->width + xs[i]]);
    }
    DrawWholeMapView();
}

// Script special for a tablet: its word into gStringVar4; VAR_RESULT TRUE
// when this was the third read in the riddle's order (the door opens).
void Rogue_ReadRuinsTablet(void)
{
    s16 xs[3], ys[3], x, y;
    u8 words[3], order[3], *str;
    s32 d = RuinsVaultHere();
    u32 n, k, reads;

    gSpecialVar_Result = FALSE;
    if (d < 0 || (n = RuinsTablets(xs, ys)) < 3)
    {
        StringCopy(gStringVar4, COMPOUND_STRING("Old letters are carved here.\nThey are too worn to read."));
        return;
    }
    GetXYCoordsOneStepInFrontOfPlayer(&x, &y);
    x -= MAP_OFFSET;
    for (k = 0; k < 2 && xs[k] != x; k++)
        ;
    RogueGen_RuinsRiddle(Rogue_GetRegion(), d, words, order);
    str = StringCopy(gStringVar4, COMPOUND_STRING("Old letters are carved here:\n“"));
    str = StringCopy(str, sRuinsWords[words[k] % RUINS_WORDS]);
    str = StringCopy(str, COMPOUND_STRING(".”"));
    if (RuinsDoorOpen(d))
        return;
    if (VarGet(VAR_RUINS_WRONG) == RUINS_DARK)
    {
        StringCopy(str, COMPOUND_STRING("\pThe tablets have gone dark.\nThey may wake once you leave."));
        return;
    }
    reads = VarGet(VAR_RUINS_READS);
    if (reads > 2)
        reads = 0;
    if (order[reads] != k)
        VarSet(VAR_RUINS_WRONG, TRUE);
    VarSet(VAR_RUINS_READS, ++reads);
    if (reads < 3)
    {
        StringCopy(str, COMPOUND_STRING("\pThe letters glow faintly..."));
        return;
    }
    VarSet(VAR_RUINS_READS, 0);
    if (VarGet(VAR_RUINS_WRONG))
    {
        // Out of order: the tablets go dark, and small words under them say
        // where the order is told (Rogue_BufferRuinsRiddleTalk).
        VarSet(VAR_RUINS_WRONG, RUINS_DARK);
        str = StringCopy(str, COMPOUND_STRING("\pThe glow faded from all three\ntablets. Nothing happened.\pSmall words are cut below:\n“Ask in "));
        str = Rogue_CopyPlaceName(str, TownNear(Rogue_GetRegion()->dungeons[d].host));
        StringCopy(str, COMPOUND_STRING(".”"));
        return;
    }
    StringCopy(str, COMPOUND_STRING("\pAll three tablets are glowing!"));
    gSpecialVar_Result = TRUE;
}

// The riddle into gStringVar1, one of three ways the townsfolk tell it.
static void BufferRuinsRiddle(u32 dungeon, u32 rng)
{
    u8 words[3], order[3], *str = gStringVar1;
    const u8 *w[3];
    u32 i;
    RogueGen_RuinsRiddle(Rogue_GetRegion(), dungeon, words, order);
    for (i = 0; i < 3; i++)
        w[i] = sRuinsWords[words[order[i]] % RUINS_WORDS];
    Rogue_CopyDungeonName(gStringVar2, dungeon);
    switch (rng % 3)
    {
    case 0:
        str = StringCopy(str, COMPOUND_STRING("There's an old door deep in\n"));
        str = StringCopy(str, gStringVar2);
        str = StringCopy(str, COMPOUND_STRING(".\pIt opens for one who reads\n"));
        str = StringCopy(str, w[0]);
        str = StringCopy(str, COMPOUND_STRING(", "));
        str = StringCopy(str, w[1]);
        str = StringCopy(str, COMPOUND_STRING(", then "));
        str = StringCopy(str, w[2]);
        StringCopy(str, COMPOUND_STRING(", they say."));
        break;
    case 1:
        str = StringCopy(str, COMPOUND_STRING("Grandma sang a rhyme about\n"));
        str = StringCopy(str, gStringVar2);
        str = StringCopy(str, COMPOUND_STRING(":\p“First the "));
        str = StringCopy(str, w[0]);
        str = StringCopy(str, COMPOUND_STRING(", then the "));
        str = StringCopy(str, w[1]);
        str = StringCopy(str, COMPOUND_STRING(",\nlast of all the "));
        str = StringCopy(str, w[2]);
        StringCopy(str, COMPOUND_STRING(".”"));
        break;
    default:
        str = StringCopy(str, COMPOUND_STRING("Ruin maniacs say a vault in\n"));
        str = StringCopy(str, gStringVar2);
        str = StringCopy(str, COMPOUND_STRING(" has three tablets.\pRead "));
        str = StringCopy(str, w[0]);
        str = StringCopy(str, COMPOUND_STRING(", "));
        str = StringCopy(str, w[1]);
        str = StringCopy(str, COMPOUND_STRING(" and "));
        str = StringCopy(str, w[2]);
        StringCopy(str, COMPOUND_STRING(" in\nthat order, and it opens!"));
        break;
    }
}

// The ruins whose riddle is told in this town (the town nearest their way
// in, which they are named after), while their door is shut, or -1.
static s32 RiddleRuinsOf(u32 place)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    s32 d = RogueGen_RuinsDungeon(r);
    if (d < 0 || RuinsDoorOpen(d) || TownNear(r->dungeons[d].host) != place)
        return -1;
    return d;
}

// A townsperson of the town the ruins are named after tells the riddle
// (teller: the second one met, or the only one). FALSE if not here.
bool32 Rogue_BufferRuinsRiddleTalk(u32 place, bool32 teller)
{
    s32 d = RiddleRuinsOf(place);
    if (d < 0 || !teller)
        return FALSE;
    BufferRuinsRiddle(d, RogueRng_Hash(Rogue_GetRegion()->seed, 8900 + place));
    return TRUE;
}

// Is the ruin maniac who studies the ruins resting in this town's POKéMON
// CENTER, as Emerald's Centers have their visitors: a teller of the riddle
// every town has room for, though its streets may be empty.
bool32 Rogue_RuinsScholarHere(u32 place)
{
    return RiddleRuinsOf(place) >= 0;
}

// Script special: what he says, into STR_VAR_1.
void Rogue_BufferRuinsScholar(void)
{
    s32 d = RiddleRuinsOf(Rogue_GetCurrentTownPlace());
    u8 words[3], order[3], *str = gStringVar1;
    u32 i;
    if (d < 0)
    {
        StringCopy(str, COMPOUND_STRING("Old ruins are my life's work.\nThis region is full of them!"));
        return;
    }
    RogueGen_RuinsRiddle(Rogue_GetRegion(), d, words, order);
    str = StringCopy(str, COMPOUND_STRING("I study the ruins near here,\n"));
    str = Rogue_CopyDungeonName(str, d);
    str = StringCopy(str, COMPOUND_STRING(".\pThe vault door has three\ntablets. Read them in order:\p"));
    for (i = 0; i < 3; i++)
    {
        str = StringCopy(str, sRuinsWords[words[order[i]] % RUINS_WORDS]);
        str = StringCopy(str, i == 0 ? COMPOUND_STRING(", ") : i == 1 ? COMPOUND_STRING(", then ") : COMPOUND_STRING("!"));
    }
}

// ---------------------------------------------------------------------------
// The lost explorer (ROBJ_EXPLORER)

// The explorer is the region's own ({EXPLORER}): a hiker's look, or a
// picnicker's for a woman, in any kind of dungeon.
u16 RogueDungeon_ExplorerGfx(u32 dungeon)
{
    struct RogueCast cast;
    (void)dungeon;
    RogueGen_Cast(Rogue_GetRegion(), &cast);
    return RogueGen_ExplorerFemale(&cast) ? OBJ_EVENT_GFX_PICNICKER : OBJ_EVENT_GFX_HIKER;
}

// Script special: the explorer's line VAR_0x8004 into gStringVar4: asking,
// the thanks with the reward, leaving by ESCAPE ROPE, staying, a full BAG
// (REXPLORER_*).
void Rogue_BufferExplorerText(void)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    DungeonCtx(&c, &st, Rogue_GetFloorDungeon(), RSPK_EXPLORER);
    DungeonText(gStringVar4, RogueText_ExplorerLine(&c, gSpecialVar_0x8004 <= REXPLORER_FULL ? gSpecialVar_0x8004 : REXPLORER_ASK,
                                                    (char *)gStringVar4, sizeof(gStringVar4)));
}

// ---------------------------------------------------------------------------
// The region's tunnel (src/rogue_gen_tunnel.h): a dungeon with a second way
// out onto the road a few gyms on, behind a barrier on its floor.

// Is the tunnel's barrier open now: the field move's badge, the leader's
// badge, or the villains' hideout cleared. FALSE if there is no tunnel.
bool32 Rogue_TunnelOpen(void)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    u32 src = ROGUE_GATE_SRC(r->tunnelGate);
    if (r->tunnel >= r->nDungeons)
        return FALSE;
    switch (ROGUE_GATE_KIND(r->tunnelGate))
    {
    case RGATE_FIELD:
        return Rogue_IsFieldMoveBadgeWon(src == ROBST_STRENGTH ? FIELD_MOVE_STRENGTH : src == ROBST_CUT ? FIELD_MOVE_CUT : FIELD_MOVE_ROCK_SMASH);
    case RGATE_GIFT:
        return FlagGet(FLAG_BADGE01_GET + src);
    case RGATE_VILLAIN:
        return FlagGet(FLAG_ROGUE_HIDEOUT_CLEARED);
    }
    return FALSE;
}

// Script special for whoever stands in the tunnel's narrow way
// (ROBJ_TUNNEL_GUARD): what they say into gStringVar4, VAR_RESULT TRUE if
// they step aside now.
void Rogue_CheckTunnelGuard(void)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    bool32 open = Rogue_TunnelOpen();
    const u8 *text;
    gSpecialVar_Result = FALSE;
    if (r->tunnel >= r->nDungeons)
        return;
    Rogue_CopyPlaceName(gStringVar1, r->dungeons[r->tunnel].exit);
    if (ROGUE_GATE_KIND(r->tunnelGate) == RGATE_VILLAIN)
    {
        StringCopy(gStringVar3, TeamName(r, 0));
        text = COMPOUND_STRING("{STR_VAR_3} is digging a\ntunnel through here. Scram!\pOur boss wants it shut tight,\nso nobody gets by!");
        open = FALSE;
    }
    else
    {
        Rogue_CopyPlaceName(gStringVar2, r->gymTown[ROGUE_GATE_SRC(r->tunnelGate) % ROGUE_NUM_GYMS]);
        text = open ? COMPOUND_STRING("That's the BADGE from\n{STR_VAR_2}!\pThis tunnel comes out at\n{STR_VAR_1}. Go on through!")
                    : COMPOUND_STRING("This tunnel comes out at\n{STR_VAR_1}.\pOnly trainers with the BADGE\nfrom {STR_VAR_2} may pass.");
    }
    StringExpandPlaceholders(gStringVar4, text);
    gSpecialVar_Result = open;
}

// A townsperson's line about the tunnel, into STR_VAR_1, near either end:
// where it comes out, and what stands in the way. FALSE if there is none
// here, or it is open already.
static bool32 BufferTunnelRumor(u32 place)
{
    const struct RogueRegion *r = Rogue_GetRegion();
    const struct RogueDungeon *d;
    const struct RoguePlace *p = &r->places[place], *host, *exit;
    const u8 *line, *barrier;
    if (r->tunnel >= r->nDungeons || Rogue_TunnelOpen())
        return FALSE;
    d = &r->dungeons[r->tunnel];
    host = &r->places[d->host];
    exit = &r->places[d->exit];
    if (abs(host->gridX - p->gridX) + abs(host->gridY - p->gridY) <= 2)
    {
        Rogue_CopyDungeonName(gStringVar2, r->tunnel);
        Rogue_CopyPlaceName(gStringVar3, d->exit);
        line = COMPOUND_STRING("They say {STR_VAR_2}\ncomes out at {STR_VAR_3}!\p");
    }
    else if (abs(exit->gridX - p->gridX) + abs(exit->gridY - p->gridY) <= 1)
    {
        Rogue_CopyPlaceName(gStringVar2, d->exit);
        Rogue_CopyPlaceName(gStringVar3, d->host);
        line = COMPOUND_STRING("The cave in the rock at\n{STR_VAR_2} leads back\pto {STR_VAR_3}, they say.\p");
    }
    else
    {
        return FALSE;
    }
    switch (ROGUE_GATE_KIND(r->tunnelGate))
    {
    case RGATE_FIELD:
        barrier = ROGUE_GATE_SRC(r->tunnelGate) == ROBST_STRENGTH ? COMPOUND_STRING("But huge boulders block the\nway in the middle.")
                                                                 : COMPOUND_STRING("But cracked rocks block the\nway in the middle.");
        break;
    case RGATE_VILLAIN:
        barrier = COMPOUND_STRING("But shady types in uniforms\nhave shut it.");
        break;
    default:
        barrier = COMPOUND_STRING("The diggers there let only\nGYM BADGE holders through.");
        break;
    }
    // Not a shortcut to sell: the far side is a later stretch of the road,
    // so near the way in they warn of its trainers, as Route 111's
    // townsfolk warn of the desert.
    if (place != d->exit && abs(host->gridX - p->gridX) + abs(host->gridY - p->gridY) <= 2
     && exit->level >= p->level + 5)
        StringCopy(StringCopy(StringExpandPlaceholders(gStringVar1, line), barrier),
                   COMPOUND_STRING("\pThe trainers out that way are\nmuch tougher than around here!"));
    else
        StringCopy(StringExpandPlaceholders(gStringVar1, line), barrier);
    return TRUE;
}

// ---------------------------------------------------------------------------
// Words to read (src/rogue_text_dungeons.h): the prize ball's line and its
// diary page, things on a floor, paintings, the POKéMON CENTER's child
// with the town's rhyme, the explorer's thanks, a museum's exhibits.

extern const u8 Rogue_EventScript_ReadThing[];

// Script special before a dungeon's prize ball is picked up: its line (and
// a diary page under it, LORE_WAY_DIARY) into gStringVar4. VAR_RESULT FALSE
// for a ball that is no prize.
void Rogue_BufferPrizeText(void)
{
    s32 dungeon = Rogue_GetFloorDungeon();
    struct RogueDungeonExtras e;
    struct RogueTextCtx c;
    struct RogueTextState st;
    u16 flag;
    gSpecialVar_Result = FALSE;
    if (dungeon < 0)
        return;
    RogueGen_DungeonExtras(Rogue_GetRegion(), dungeon, &e);
    flag = GetObjectEventFlagIdByLocalIdAndMap(gSpecialVar_LastTalked, gSaveBlock1Ptr->location.mapNum, gSaveBlock1Ptr->location.mapGroup);
    if (e.prize == 0xFF || flag != FLAG_ROGUE_ITEM_0 + e.prize)
        return;
    DungeonCtx(&c, &st, dungeon, RSPK_NONE);
    DungeonText(gStringVar4, RogueText_PrizeLine(&c, (char *)gStringVar4, sizeof(gStringVar4)));
    gSpecialVar_Result = gStringVar4[0] != EOS;
}

// The POKéMON CENTER's picture (the pokemon_center tileset's, top right,
// beside the storage PC, which keeps its own script) and a home's painting
// (the Building tileset's 0x207 over 0x20F).
static bool32 IsCenterPicture(u32 mt)
{
    return mt == 0x20E || mt == 0x20F || mt == 0x216 || mt == 0x217;
}

static bool32 IsHomePainting(u32 mt)
{
    return mt == 0x207 || mt == 0x20F;
}

const u8 *Rogue_GetThingScript(s16 x, s16 y)
{
    const struct RogueRegion *r;
    struct RogueTextCtx c;
    struct RogueTextState st;
    s32 dungeon, place;
    u32 mt, len = 0, mapNum = gSaveBlock1Ptr->location.mapNum;

    if (gSaveBlock1Ptr->location.mapGroup != MAP_GROUP(MAP_ROGUE_PLACE_0) || (place = Rogue_GetCurrentPlace()) < 0)
        return NULL;
    // RUN 0: the graveyard's graves from the run log, the library's shelves.
    {
        const u8 *run0 = Rogue_Run0ThingScript(x, y);
        if (run0 != NULL)
            return run0;
    }
    r = Rogue_GetRegion();
    mt = MapGridGetMetatileIdAt(x, y);
    dungeon = Rogue_GetFloorDungeon();
    if (dungeon >= 0)
    {
        s32 floor = Rogue_GetFloorNumber();
        u8 k, thing;
        bool8 lore;
        if (floor < 0 || x < MAP_OFFSET || y < MAP_OFFSET)
            return NULL;
        thing = RogueGen_FloorThingAt(r, dungeon, floor, x - MAP_OFFSET, y - MAP_OFFSET, &k, &lore);
        if (thing == RTHING_NONE)
            return NULL;
        DungeonCtx(&c, &st, dungeon, RSPK_SIGN);
        len = RogueText_FloorThing(&c, floor, thing, k, lore, (char *)gStringVar4, sizeof(gStringVar4));
    }
    else if (mapNum == MAP_NUM(MAP_ROGUE_HOUSE) && RogueGen_TownSignature(r, place) == RSIG_MUSEUM
          && gSaveBlock1Ptr->location.warpId < Rogue_GetPlaceMap(place)->nHouses
          && Rogue_GetPlaceMap(place)->interiors[gSaveBlock1Ptr->location.warpId] == RINT_SIGNATURE)
    {
        // The museum's display cases beside its two exhibits (only in the
        // museum: the town's homes keep their paintings).
        s32 k;
        if (x < MAP_OFFSET || y < MAP_OFFSET
         || (k = RogueGen_MuseumCase(r->places[place].theme == RTHEME_COVE, x - MAP_OFFSET, y - MAP_OFFSET)) < 0)
            return NULL;
        Rogue_TextCtx(&c, &st, place, RSPK_SIGN);
        len = RogueText_MuseumExhibit(&c, k, (char *)gStringVar4, sizeof(gStringVar4));
    }
    else if ((mapNum == MAP_NUM(MAP_ROGUE_POKEMON_CENTER) && IsCenterPicture(mt))
          || (mapNum == MAP_NUM(MAP_ROGUE_HOUSE) && IsHomePainting(mt)))
    {
        const struct RogueMap *m = Rogue_GetPlaceMap(place);
        u32 house = mapNum == MAP_NUM(MAP_ROGUE_HOUSE) ? gSaveBlock1Ptr->location.warpId : 0xFF;
        if (house != 0xFF && (house >= m->nHouses || m->interiors[house] != RINT_HOME))
            return NULL;
        Rogue_TextCtx(&c, &st, place, RSPK_SIGN);
        len = RogueText_Painting(&c, house, RogueGen_PaintingHouse(r, place, m->interiors, m->nHouses),
                                 (char *)gStringVar4, sizeof(gStringVar4));
    }
    if (len == 0)
        return NULL;
    DungeonText(gStringVar4, len);
    return Rogue_EventScript_ReadThing;
}

bool32 Rogue_RhymeChildHere(u32 place)
{
    return RogueGen_LoreAt(Rogue_GetRegion(), LORE_WAY_RHYME, place) >= 0;
}

// Script special: the child's rhyme, into gStringVar4.
void Rogue_BufferRhyme(void)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    Rogue_TextCtx(&c, &st, Rogue_GetCurrentTownPlace(), RSPK_FOLK);
    DungeonText(gStringVar4, RogueText_Rhyme(&c, (char *)gStringVar4, sizeof(gStringVar4)));
}

bool32 Rogue_ExplorerThanksHere(u32 place)
{
    struct RogueCast cast;
    struct RogueDungeonExtras e;
    RogueGen_Cast(Rogue_GetRegion(), &cast);
    if (cast.explorerDungeon == 0xFF || cast.explorerTown != place)
        return FALSE;
    RogueGen_DungeonExtras(Rogue_GetRegion(), cast.explorerDungeon, &e);
    return e.explorer != 0xFF && FlagGet(FLAG_ROGUE_ITEM_0 + e.explorer);
}

// Script special: the explorer's thanks, into gStringVar4; the words turn
// over with the badges won.
void Rogue_BufferExplorerThanks(void)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    Rogue_TextCtx(&c, &st, Rogue_GetCurrentTownPlace(), RSPK_EXPLORER);
    c.number = st.badges;
    DungeonText(gStringVar4, RogueText_ExplorerLine(&c, REXPLORER_TOWN, (char *)gStringVar4, sizeof(gStringVar4)));
}

// Script special for a museum's exhibit VAR_0x8004 (0 the first), into
// gStringVar4; VAR_RESULT FALSE past the last. For the town signatures'
// museum (Rogue_EventScript_MuseumExhibit).
void Rogue_BufferMuseumExhibit(void)
{
    struct RogueTextCtx c;
    struct RogueTextState st;
    Rogue_TextCtx(&c, &st, Rogue_GetCurrentTownPlace(), RSPK_SIGN);
    DungeonText(gStringVar4, RogueText_MuseumExhibit(&c, gSpecialVar_0x8004, (char *)gStringVar4, sizeof(gStringVar4)));
    gSpecialVar_Result = gStringVar4[0] != EOS;
}
