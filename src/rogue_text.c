// The game's side of the text engine (src/rogue_gen_text.h): the values
// only the game knows (the player's name, the lead POKéMON, a wild species),
// what the run has done so far, and the step from the generator's ASCII to
// the game's characters.
//
// To show a line: set up a context where it is said, fill its state, say
// the template into a buffer and convert it where it lies:
//
//     struct RogueTextCtx c;
//     struct RogueTextState st;
//     Rogue_TextCtx(&c, &st, place, RSPK_FOLK);
//     RogueText_Say((char *)gStringVar1, sizeof(gStringVar1), template, &c);
//     Rogue_TextToGame(gStringVar1);
//
// Rogue_BufferText does all of that for a template.

#include "global.h"
#include "rogue.h"
#include "rogue_gen.h"
#include "event_data.h"
#include "battle.h"
#include "pokemon.h"
#include "string_util.h"
#include "constants/characters.h"
#include "constants/flags.h"
#include "constants/trainers.h"
#include "constants/rogue.h"

// The game's characters in ASCII, for names the text fills in (the
// generator's own codes: '@' the e of POKéMON, '^' and '`' the male and
// female signs, as Rogue_CopyLoreAscii turns them back).
char *Rogue_GameToAscii(char *dest, const u8 *src, u32 max)
{
    u32 n = 0;
    for (; *src != EOS && n < max; src++, n++)
    {
        u8 c = *src;
        if (c >= CHAR_A && c <= CHAR_Z)
            *dest++ = 'A' + (c - CHAR_A);
        else if (c >= CHAR_a && c <= CHAR_z)
            *dest++ = 'a' + (c - CHAR_a);
        else if (c >= CHAR_0 && c <= CHAR_9)
            *dest++ = '0' + (c - CHAR_0);
        else
        {
            switch (c)
            {
            case CHAR_PERIOD:          *dest++ = '.'; break;
            case CHAR_COMMA:           *dest++ = ','; break;
            case CHAR_EXCL_MARK:       *dest++ = '!'; break;
            case CHAR_QUESTION_MARK:   *dest++ = '?'; break;
            case CHAR_SGL_QUOTE_LEFT:
            case CHAR_SGL_QUOTE_RIGHT: *dest++ = '\''; break;
            case CHAR_HYPHEN:          *dest++ = '-'; break;
            case CHAR_COLON:           *dest++ = ':'; break;
            case CHAR_e_ACUTE:         *dest++ = '@'; break;
            case CHAR_MALE:            *dest++ = '^'; break;
            case CHAR_FEMALE:          *dest++ = '`'; break;
            case CHAR_AMPERSAND:       *dest++ = '&'; break;
            case CHAR_LEFT_PAREN:      *dest++ = '('; break;
            case CHAR_RIGHT_PAREN:     *dest++ = ')'; break;
            case CHAR_SPACE:           *dest++ = ' '; break;
            default: break;
            }
        }
    }
    *dest = 0;
    return dest;
}

// The values only the game knows, for the generator's text.
char *RogueText_Runtime(u8 slot, u8 a, u8 b, char *dest)
{
    u16 species;
    switch (slot)
    {
    case TS_PLAYER:
        return Rogue_GameToAscii(dest, gSaveBlock2Ptr->playerName, PLAYER_NAME_LENGTH);
    case TS_RIVAL:
        return Rogue_GameToAscii(dest, gSaveBlock2Ptr->playerGender == MALE ? COMPOUND_STRING("MAY") : COMPOUND_STRING("BRENDAN"), 8);
    case TS_LEAD:
        species = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES);
        break;
    case TS_RIVAL_MON:
        species = Rogue_GetRivalPartner();
        break;
    case TS_MON:
        species = Rogue_GetWildSpecies(a, b);
        break;
    case TS_OWN:
        species = Rogue_TrainerLeadSpecies(a);
        break;
    default:
        return NULL;
    }
    if (species == SPECIES_NONE || species >= NUM_SPECIES)
        return NULL;
    return Rogue_GameToAscii(dest, GetSpeciesName(species), POKEMON_NAME_LENGTH);
}

// What the run has done so far.
void Rogue_TextState(struct RogueTextState *s)
{
    u32 i;
    memset(s, 0, sizeof(*s));
    s->badges = Rogue_BadgeCount();
    for (i = 0; i < ROGUE_NUM_GYMS; i++)
        if (FlagGet(FLAG_BADGE01_GET + i))
            s->wonGyms |= 1 << i;
    for (i = 0; i < ROGUE_MAX_DUNGEONS; i++)
        if (FlagGet(FLAG_ROGUE_BOSS_0 + i))
            s->bosses |= 1 << i;
    s->fallen = VarGet(VAR_ROGUE_FALLEN);
    if (FlagGet(FLAG_SYS_POKEMON_GET))
        s->flags |= RTSTATE_STARTER;
    if (FlagGet(FLAG_ROGUE_RIVAL_1))
        s->flags |= RTSTATE_RIVAL1;
    if (FlagGet(FLAG_ROGUE_HIDEOUT_CLEARED))
        s->flags |= RTSTATE_HIDEOUT;
    if (FlagGet(TRAINER_FLAGS_START + ROGUE_TRAINER(ROGUE_SLOT_VILLAIN_LEADER)))
        s->flags |= RTSTATE_VLEADER;
    for (i = 0; i < ROGUE_MAX_TEAMS; i++)
    {
        if (Rogue_TeamCleared(i))
            s->teamsCleared |= 1 << i;
        if (Rogue_TeamLeaderBeaten(i))
            s->teamsBeaten |= 1 << i;
    }
    if (Rogue_IsHardcore())
        s->flags |= RTSTATE_HARDCORE;
    if (s->flags & RTSTATE_STARTER)
        s->leadType = gSpeciesInfo[GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES)].types[0];
    s->runs = VarGet(VAR_ROGUE_META_RUNS);
    s->wins = VarGet(VAR_ROGUE_META_WINS);
    // The cast's lost explorer led out (the item flag, RogueDungeonExtras).
    {
        struct RogueCast cast;
        RogueGen_Cast(Rogue_GetRegion(), &cast);
        if (cast.explorerDungeon < 16)
        {
            struct RogueDungeonExtras e;
            RogueGen_DungeonExtras(Rogue_GetRegion(), cast.explorerDungeon, &e);
            if (e.explorer != 0xFF && FlagGet(FLAG_ROGUE_ITEM_0 + e.explorer))
                s->rescued |= 1 << cast.explorerDungeon;
        }
    }
}

// A context for a line said at a place (a town, route or cave; -1 for the
// current one), with the run's state in st.
void Rogue_TextCtx(struct RogueTextCtx *c, struct RogueTextState *st, s32 place, u8 speaker)
{
    if (place < 0)
        place = Rogue_GetCurrentPlace();
    if (place < 0)
        place = Rogue_GetRegion()->start;
    RogueText_Init(c, Rogue_GetRegion(), place, speaker);
    Rogue_TextState(st);
    c->state = st;
}

// The generator's ASCII in dest turned into the game's characters where it
// lies (one character each).
u8 *Rogue_TextToGame(u8 *dest)
{
    return Rogue_CopyLoreAscii(dest, (const char *)dest);
}

// A template said in c into dest (size bytes with its end), in the game's
// characters. Returns the end.
u8 *Rogue_BufferText(u8 *dest, u32 size, const char *template, struct RogueTextCtx *c)
{
    RogueText_Say((char *)dest, size, template, c);
    return Rogue_TextToGame(dest);
}
