// The text engine: lines written as templates with named slots for the
// region's facts, filled, wrapped into Gen 3 boxes and spread so a run does
// not hear one line twice. Generalized from the legend lore
// (src/rogue_gen_lore.h). Included at the end of rogue_gen.c after the
// facts, so the host tools (gen_host --script, scripttest.py) see every line
// the game can show.
//
// Text is ASCII, as the lore's: '@' is the e of POK@MON, '<' and '>' the
// curly quotes, '\n' a line break, '\v' a scroll of the box ("\l" in the
// game's strings), '|' a new box ("\p"). The game converts the result as it
// is (Rogue_CopyLoreAscii). Lines are broken at TEXT_LINE characters (two to
// a box, the box scrolling on past two), so a template is written as one run
// of words; explicit breaks are kept, for lines migrated from the game.
//
// Slots are {NAME} (see sTextSlots below for all of them), {^NAME} with its
// first letter capitalized. A slot the context cannot fill makes the line
// unusable (RogueText_CanFill), and the fill writes {?NAME} so the checks
// find it. Values only the game knows (the player's name, the lead
// POK@MON, a wild species) come from RogueText_Runtime, which the game and
// the host tools each define.
//
// Adding text for a domain: write banks in your own header
// (src/rogue_text_<domain>.h, already included at the end of this file),
// with TEXT_BANK(sMyBank, "line", "line"...), pick with RogueText_Pick (one
// place's people in turn) or RogueText_PickRegion (spread over the whole
// region), fill with RogueText_Say, and add what your text can show to your
// header's TextDump_<Domain> so gen_host --script prints it. See
// rogue_text_towns.h (the town signs) for a complete example.

#define TEXT_LINE LORE_LINE


// The slots a template may use. What each is:
//  REGION      the region's name (NATEDEA)
//  TOWN, STEM  ctx.town's name (PINEWICK TOWN) and without TOWN/CITY (PINEWICK)
//  OTHER, OTHER_STEM  ctx.other, a second place (a town up the road)
//  NEXT        the next town along the road from ctx.place
//  ROUTE       ctx.route's name (ROUTE 4, SHADOW CAVE)
//  PLACE       ctx.place's own name
//  DUNGEON     ctx.dungeon's name (MT. CINDER, PINEWICK WOODS)
//  LEGEND      ctx.dungeon's legend (ZAPDOS)
//  LEADER, GYM_TOWN, TYPE, BADGE  ctx.gym's leader, town, type (ROCK) and badge
//  CLASS       ctx.slot's trainer class (BUG CATCHER)
//  TEAM, BOSS, ADMIN  ctx.team's name (TEAM AQUA, TEAM MAGMA, TEAM ROCKET),
//              its boss (ARCHIE, MAXIE, GIOVANNI) and its admin; with no
//              ctx.team, the team whose hideout is under ctx.place, else
//              the region's main team
//  TEAM2, BOSS2  ctx.team2's name and boss: the team they clash with (never
//              GIOVANNI: nobody names TEAM ROCKET's BOSS but himself)
//  VOLCANO     the volcano's name, where the map draws one
//  TRADE, TRADE_FOLK, GOODS  ctx.town's trade: fishing, fishers, fish
//  LANDMARK, PRIDE  ctx.town's landmark (only one on its map) and what it is proud of
//  NEIGHBOR, FEUD   ctx.town's rival town and what they argue over
//  LOCALMON    ctx.town's local POK@MON (a species of the route out)
//  PROBLEM_AT  the place or dungeon ctx.town's problem is about
//  FESTIVAL, FEST_ACT, FEST_TOWN  the region's festival, what people do, where
//  FOUNDING, OLDTOWN  how the region began ("Long ago, ..."), the oldest town
//  FIGURE, FIGURE_TITLE, FIGURE_DEED, FIGURE_TOWN  the famous figure
//  WANDERER, REPORTER, CAMERA, ELDER, EXPLORER  the recurring cast
//  FAMILY      the family of ctx.town's ctx.house (ASHBY)
//  PLAYER, RIVAL, LEAD, RIVAL_MON  the player, the rival, the lead POK@MON, the rival's starter
//  MON         the species in ctx.area's wild table at ctx.mon (a wild one: never
//              "my {MON}", it is seldom in the speaker's team)
//  OWN         the species trainer ctx.slot sends out first: what a trainer
//              says they caught, hooked or raised
//  BADGES      the badges won, as a word (no, one, two... eight)
//  N           ctx.number
//  NAME        ctx.name, a generated person's name
//  NICK, FEATURE, RESIDENT  ctx.route's local name ("the LONG FORD"), its
//              feature ("the DAY CARE") and the feature's person (route facts,
//              src/rogue_gen_features.h)
static const char *const sTextSlots[TS_COUNT] =
{
    "REGION", "TOWN", "STEM", "OTHER", "OTHER_STEM", "NEXT", "ROUTE", "PLACE", "DUNGEON", "LEGEND",
    "LEADER", "GYM_TOWN", "TYPE", "BADGE", "CLASS", "TEAM", "BOSS", "ADMIN", "VOLCANO",
    "TRADE", "TRADE_FOLK", "GOODS", "LANDMARK", "PRIDE", "NEIGHBOR", "FEUD", "LOCALMON", "PROBLEM_AT",
    "FESTIVAL", "FEST_ACT", "FEST_TOWN", "FOUNDING", "OLDTOWN", "FIGURE", "FIGURE_TITLE", "FIGURE_DEED",
    "FIGURE_TOWN", "WANDERER", "REPORTER", "CAMERA", "ELDER", "EXPLORER", "FAMILY",
    "PLAYER", "RIVAL", "LEAD", "MON", "RIVAL_MON", "BADGES", "N", "NAME",
    "NICK", "FEATURE", "RESIDENT", "TEAM2", "BOSS2", "OWN",
};

static const char *const sTextNumbers[] = { "no", "one", "two", "three", "four", "five", "six", "seven", "eight" };

void RogueText_Init(struct RogueTextCtx *c, const struct RogueRegion *r, u8 place, u8 speaker)
{
    c->r = r;
    c->state = NULL;
    c->place = place;
    c->speaker = speaker;
    c->town = place < r->nPlaces ? RogueGen_TownNear(r, place) : 0xFF;
    c->other = c->route = c->dungeon = c->gym = c->house = c->area = c->mon = c->slot = 0xFF;
    c->team = c->team2 = 0xFF;
    c->number = 0;
    c->name = NULL;
    c->facts = NULL;
    c->missing = 0;
}

// The teams' names, bosses and admins, by identity (ROGUE_VILLAIN_*).
const char *const gRogueTeamNames[ROGUE_VILLAIN_IDS] = { "TEAM AQUA", "TEAM MAGMA", "TEAM ROCKET" };
const char *const gRogueTeamBosses[ROGUE_VILLAIN_IDS] = { "ARCHIE", "MAXIE", "GIOVANNI" };

// Which of its two admins team t sends (0 or 1), as the game rolls it: the
// main team as before v0.13, the others on their own streams.
u8 RogueGen_TeamAdmin(const struct RogueRegion *r, u8 team)
{
    return RogueRng_Hash(r->seed, team == 0 ? 14003 : 14020 + team) & 1;
}

// The admin team t sends: MATT or SHELLY, TABITHA or COURTNEY, ARCHER or
// ARIANA.
const char *RogueGen_TeamAdminName(const struct RogueRegion *r, u8 team)
{
    static const char *const admins[ROGUE_VILLAIN_IDS][2] = { { "MATT", "SHELLY" }, { "TABITHA", "COURTNEY" }, { "ARCHER", "ARIANA" } };
    return admins[RogueGen_TeamId(r, team) % ROGUE_VILLAIN_IDS][RogueGen_TeamAdmin(r, team)];
}

const char *RogueGen_VillainAdminName(const struct RogueRegion *r)
{
    return RogueGen_TeamAdminName(r, 0);
}

// The team whose hideout is under a place, or 0xFF.
static u8 TextTeamAt(const struct RogueRegion *r, u8 place)
{
    s32 d = place < r->nPlaces ? RogueGen_DungeonAt(r, place) : -1;
    if (d >= 0 && r->dungeons[d].kind == RDUN_HIDEOUT && r->dungeons[d].host == place)
        return RogueGen_HideoutTeam(r, d);
    return 0xFF;
}

// The team a line speaks of ({TEAM}): ctx.team, else the team of the
// town's trouble (the hideout under it), else the team whose hideout is
// under the town or the place, else the main team. 0xFF: none.
static u8 TextTeam(const struct RogueTextCtx *c)
{
    const struct RogueRegion *r = c->r;
    u8 t;
    if (RogueGen_TeamCount(r) == 0)
        return 0xFF;
    if (c->team < RogueGen_TeamCount(r))
        return c->team;
    if (c->facts != NULL && c->facts->problem == RPROB_VILLAINS && RogueGen_HideoutTeam(r, c->facts->problemAt) != 0xFF)
        return RogueGen_HideoutTeam(r, c->facts->problemAt);
    if ((t = TextTeamAt(r, c->town)) != 0xFF || (t = TextTeamAt(r, c->place)) != 0xFF)
        return t;
    return 0;
}

static const char *TextFeud(const struct RogueTownFacts *f)
{
    switch (f->feud)
    {
    case RFEUD_SAME_TRADE: return "who has the best {GOODS}";
    case RFEUD_TRADES:     return "which town works harder";
    case RFEUD_GYMS:       return "whose GYM is tougher";
    case RFEUD_FESTIVAL:   return "who throws the better {FESTIVAL}";
    default:               return "the yearly race between our towns";
    }
}

static bool8 TextTown(const struct RogueTextCtx *c, u8 town)
{
    return town < c->r->nPlaces && c->r->places[town].kind == RPLACE_TOWN;
}

static char *TextFillTo(char *out, char *end, const char *t, struct RogueTextCtx *c, u8 depth);

// A slot's value at out (NULL: only checking it can be filled).
static char *TextPut(char *out, char *end, const char *s)
{
    if (out == NULL)
        return (char *)1;
    while (*s && out < end)
        *out++ = *s++;
    *out = 0;
    return out;
}

// The slots of a town's facts (kept out of TextSlot, so its frame stays small).
static __attribute__((noinline)) char *TextFactSlot(char *out, char *end, u8 slot, struct RogueTextCtx *c, u8 depth)
{
    const struct RogueRegion *r = c->r;
    char word[64];
    const char *s = NULL;
    word[0] = 0;
    do
    {
        struct RogueTownFacts tf;
        const struct RogueTownFacts *f = &tf;
        if (!TextTown(c, c->town) || r->places[c->town].type == RTOWN_SUMMIT) return NULL;
        // Only these few need the facts to say whether they can be filled.
        if (out == NULL && slot != TS_NEIGHBOR && slot != TS_FEUD && slot != TS_PROBLEM_AT && slot != TS_LOCALMON
         && slot != TS_LANDMARK)
            return (char *)1;
        if (c->facts != NULL && c->facts->place == c->town)
            f = c->facts;
        else
            RogueGen_TownFacts(r, c->town, &tf);
        switch (slot)
        {
        case TS_TRADE:      s = gRogueTrades[f->trade].name; break;
        case TS_TRADE_FOLK: s = gRogueTrades[f->trade].folk; break;
        case TS_GOODS:      s = gRogueTrades[f->trade].goods; break;
        case TS_LANDMARK:
            // Only a landmark the map shows (RogueGen_LandmarkOnMap).
            if (!RogueGen_LandmarkOnMap(r, f)) return NULL;
            if (out == NULL) return (char *)1;
            RogueGen_LandmarkName(r, f, word);
            break;
        case TS_PRIDE:      s = RogueGen_TownPride(f); break;
        case TS_NEIGHBOR:
            if (f->neighbor == 0xFF) return NULL;
            RogueGen_PlaceName(r, f->neighbor, word);
            break;
        case TS_FEUD:
            if (f->neighbor == 0xFF || depth > 2) return NULL;
            if (out == NULL)
                return TextFillTo(NULL, NULL, TextFeud(f), c, depth + 1) ? (char *)1 : NULL;
            return TextFillTo(out, end, TextFeud(f), c, depth + 1);
        case TS_PROBLEM_AT:
            if (f->problemAt == 0xFF) return NULL;
            if (f->problem == RPROB_VILLAIN_SIGHTED || f->problem == RPROB_BLOCKED)
                RogueGen_PlaceName(r, f->problemAt, word);
            else
                RogueGen_DungeonName(r, f->problemAt, word);
            break;
        case TS_LOCALMON:
            if (f->localArea == 0xFF) return NULL;
            if (out == NULL)
                return (char *)1;
            if (RogueText_Runtime(TS_MON, f->localArea, f->localSlot, word) == NULL) return NULL;
            break;
        }
        break;
    } while (0);
    return TextPut(out, end, s ? s : word);
}

// The slots of the region's facts.
static __attribute__((noinline)) char *TextRegionSlot(char *out, char *end, u8 slot, struct RogueTextCtx *c, u8 depth)
{
    const struct RogueRegion *r = c->r;
    char word[64];
    const char *s = NULL;
    // Every region has these: a check needs no words (the tales hold slots
    // of their own, checked below).
    if (out == NULL && slot != TS_FOUNDING && slot != TS_FIGURE_DEED)
        return (char *)1;
    word[0] = 0;
    do
    {
        struct RogueRegionFacts f;
        RogueGen_RegionFacts(r, &f);
        switch (slot)
        {
        case TS_FESTIVAL:     s = RogueGen_FestivalName(&f); break;
        case TS_FEST_ACT:     s = RogueGen_FestivalAct(&f); break;
        case TS_FEST_TOWN:    RogueGen_PlaceName(r, f.festTown, word); break;
        case TS_OLDTOWN:      RogueGen_PlaceName(r, f.oldTown, word); break;
        case TS_FIGURE:       s = RogueGen_FigureName(r, &f); break;
        case TS_FIGURE_TITLE: s = RogueGen_FigureTitle(&f); break;
        case TS_FIGURE_TOWN:  RogueGen_PlaceName(r, f.figureTown, word); break;
        case TS_FOUNDING:
        case TS_FIGURE_DEED:
        {
            // These hold slots of their own: the legend, the region.
            struct RogueTextCtx sub = *c;
            const char *t = slot == TS_FOUNDING ? RogueGen_FoundingTale(&f) : RogueGen_FigureDeed(&f);
            char *e;
            if (depth > 2) return NULL;
            sub.dungeon = f.legend;
            if (slot == TS_FIGURE_DEED)
                sub.town = f.figureTown;
            e = TextFillTo(out, end, t, &sub, depth + 1);
            c->missing += sub.missing - c->missing;
            return out == NULL ? (e ? (char *)1 : NULL) : e;
        }
        }
        break;
    } while (0);
    return TextPut(out, end, s ? s : word);
}

// The slots of the cast.
static __attribute__((noinline)) char *TextCastSlot(char *out, char *end, u8 slot, struct RogueTextCtx *c, u8 depth)
{
    const struct RogueRegion *r = c->r;
    char word[64];
    const char *s = NULL;
    if (out == NULL && slot != TS_EXPLORER)
        return (char *)1;
    word[0] = 0;
    do
    {
        struct RogueCast cast;
        RogueGen_Cast(r, &cast);
        switch (slot)
        {
        case TS_WANDERER: s = RogueGen_WandererName(&cast); break;
        case TS_REPORTER: s = RogueGen_ReporterName(&cast, FALSE); break;
        case TS_CAMERA:   s = RogueGen_ReporterName(&cast, TRUE); break;
        case TS_ELDER:    RogueGen_ElderName(&cast, word); break;
        case TS_EXPLORER:
            if (cast.explorerDungeon == 0xFF) return NULL;
            s = RogueGen_ExplorerName(&cast);
            break;
        }
        break;
    } while (0);
    return TextPut(out, end, s ? s : word);
}


// The other slots.
static __attribute__((noinline)) char *TextPlainSlot(char *out, char *end, u8 slot, struct RogueTextCtx *c, u8 depth)
{
    const struct RogueRegion *r = c->r;
    char word[64];
    const char *s = NULL;
    word[0] = 0;
    switch (slot)
    {
    case TS_REGION:
        if (out == NULL) return (char *)1;
        if (!RogueGen_KeptRegionName(r->seed, word))
            RogueGen_RegionName(r->seed, word);
        break;
    case TS_TOWN:
        if (!TextTown(c, c->town)) return NULL;
        if (out == NULL) return (char *)1;
        RogueGen_PlaceName(r, c->town, word);
        break;
    case TS_STEM:
        if (!TextTown(c, c->town)) return NULL;
        if (out == NULL) return (char *)1;
        RogueGen_TownStem(r, c->town, word);
        break;
    case TS_OTHER:
        if (c->other >= r->nPlaces) return NULL;
        if (out == NULL) return (char *)1;
        RogueGen_PlaceName(r, c->other, word);
        break;
    case TS_OTHER_STEM:
        if (!TextTown(c, c->other)) return NULL;
        RogueGen_TownStem(r, c->other, word);
        break;
    case TS_NEXT:
    {
        u8 next;
        if (c->place >= r->nPlaces) return NULL;
        next = RogueGen_NextTownFrom(r, c->place);
        if (next == c->place) return NULL;
        if (out == NULL) return (char *)1;
        RogueGen_PlaceName(r, next, word);
        break;
    }
    case TS_ROUTE:
        if (c->route >= r->nPlaces) return NULL;
        if (out == NULL) return (char *)1;
        RogueGen_PlaceName(r, c->route, word);
        break;
    case TS_PLACE:
        if (c->place >= r->nPlaces) return NULL;
        if (out == NULL) return (char *)1;
        RogueGen_PlaceName(r, c->place, word);
        break;
    case TS_DUNGEON:
        if (c->dungeon >= r->nDungeons) return NULL;
        if (out == NULL) return (char *)1;
        RogueGen_DungeonName(r, c->dungeon, word);
        break;
    case TS_LEGEND:
        if (c->dungeon >= r->nDungeons || r->dungeons[c->dungeon].boss != RBOSS_LEGEND) return NULL;
        s = sLoreNames[r->dungeons[c->dungeon].legend % RLEG_COUNT];
        break;
    case TS_LEADER:
        if (c->gym >= ROGUE_NUM_GYMS + ROGUE_NUM_E4) return NULL;
        s = RogueGen_GymLeader(r, c->gym)->name;
        break;
    case TS_GYM_TOWN:
        if (c->gym >= ROGUE_NUM_GYMS) return NULL;
        if (out == NULL) return (char *)1;
        RogueGen_PlaceName(r, r->gymTown[c->gym], word);
        break;
    case TS_TYPE:
        if (c->gym >= ROGUE_NUM_GYMS + ROGUE_NUM_E4) return NULL;
        s = gRogueTypeNames[RogueGen_GymLeader(r, c->gym)->type % RTYPE_COUNT];
        break;
    case TS_BADGE:
        if (c->gym >= ROGUE_NUM_GYMS) return NULL;
        s = RogueGen_GymLeader(r, c->gym)->badge;
        break;
    case TS_CLASS:
    {
        u8 cls;
        if (c->slot == 0xFF) return NULL;
        cls = RogueGen_TrainerClass(r, c->slot);
        if (cls >= ROGUE_CLASS_COUNT) return NULL;
        s = gRogueGenClasses[cls].name;
        break;
    }
    case TS_TEAM:
    case TS_BOSS:
    case TS_ADMIN:
    {
        u8 t = TextTeam(c);
        if (t == 0xFF) return NULL;
        s = slot == TS_TEAM ? gRogueTeamNames[RogueGen_TeamId(r, t) % ROGUE_VILLAIN_IDS]
          : slot == TS_BOSS ? gRogueTeamBosses[RogueGen_TeamId(r, t) % ROGUE_VILLAIN_IDS]
          : RogueGen_TeamAdminName(r, t);
        break;
    }
    case TS_TEAM2:
    case TS_BOSS2:
        if (c->team2 >= RogueGen_TeamCount(r)) return NULL;
        // Nobody names TEAM ROCKET's BOSS but himself: he may run a gym.
        if (slot == TS_BOSS2 && RogueGen_TeamId(r, c->team2) == ROGUE_VILLAIN_ROCKET) return NULL;
        s = (slot == TS_TEAM2 ? gRogueTeamNames : gRogueTeamBosses)[RogueGen_TeamId(r, c->team2) % ROGUE_VILLAIN_IDS];
        break;
    case TS_VOLCANO:
    {
        s8 vx, vy;
        if (!RogueGen_FactsVolcano(r, &vx, &vy)) return NULL;
        if (out == NULL) return (char *)1;
        RogueGen_VolcanoName(r, word);
        break;
    }
    case TS_FAMILY:
        if (!TextTown(c, c->town) || c->house == 0xFF) return NULL;
        s = RogueGen_FamilyName(r, c->town, c->house);
        break;
    case TS_MON:
        if (c->area == 0xFF) return NULL;
        if (out == NULL)
            return (char *)1;
        if (RogueText_Runtime(TS_MON, c->area, c->mon, word) == NULL) return NULL;
        break;
    case TS_OWN:
        if (c->slot == 0xFF) return NULL;
        if (out == NULL)
            return (char *)1;
        if (RogueText_Runtime(TS_OWN, c->slot, 0, word) == NULL) return NULL;
        break;
    case TS_PLAYER: case TS_RIVAL: case TS_LEAD: case TS_RIVAL_MON:
        if (out == NULL)
            return (char *)1;
        if (RogueText_Runtime(slot, 0, 0, word) == NULL) return NULL;
        break;
    case TS_BADGES:
        s = sTextNumbers[c->state ? c->state->badges % 9 : 0];
        break;
    case TS_N:
        NameNumber(word, c->number);
        break;
    case TS_NAME:
        if (c->name == NULL) return NULL;
        s = c->name;
        break;
    case TS_NICK:
    case TS_FEATURE:
    case TS_RESIDENT:
    {
        u8 rt = c->route < r->nPlaces ? c->route : c->place;
        bool8 ok;
        if (rt >= r->nPlaces) return NULL;
        ok = slot == TS_NICK ? RogueGen_RouteNick(r, rt, word) : slot == TS_FEATURE ? RogueGen_FeatureName(r, rt, word)
           : RogueGen_ResidentName(r, rt, word);
        if (!ok) return NULL;
        if (out == NULL) return (char *)1;
        break;
    }
    default:
        return NULL;
    }
    return TextPut(out, end, s ? s : word);
}

// One slot's value at out (NULL to only check it can be filled). Returns
// the end, or NULL if the context cannot fill it. The kinds of slot keep
// their work in functions of their own, so the stack stays shallow.
static char *TextSlot(char *out, char *end, u8 slot, struct RogueTextCtx *c, u8 depth)
{
    switch (slot)
    {
    case TS_TRADE: case TS_TRADE_FOLK: case TS_GOODS: case TS_LANDMARK: case TS_PRIDE: case TS_NEIGHBOR:
    case TS_FEUD: case TS_PROBLEM_AT: case TS_LOCALMON:
        return TextFactSlot(out, end, slot, c, depth);
    case TS_FESTIVAL: case TS_FEST_ACT: case TS_FEST_TOWN: case TS_FOUNDING: case TS_OLDTOWN: case TS_FIGURE:
    case TS_FIGURE_TITLE: case TS_FIGURE_DEED: case TS_FIGURE_TOWN:
        return TextRegionSlot(out, end, slot, c, depth);
    case TS_WANDERER: case TS_REPORTER: case TS_CAMERA: case TS_ELDER: case TS_EXPLORER:
        return TextCastSlot(out, end, slot, c, depth);
    default:
        return TextPlainSlot(out, end, slot, c, depth);
    }
}

// Fills t's slots into out (NULL: only checks). A slot that cannot be
// filled is written as {?NAME} and counted in c->missing.
static char *TextFillTo(char *out, char *end, const char *t, struct RogueTextCtx *c, u8 depth)
{
    bool8 ok = TRUE;
    for (; *t; t++)
    {
        u8 slot, len;
        bool8 cap = FALSE;
        char *start;
        if (*t != '{')
        {
            if (out != NULL && out < end)
                *out++ = *t;
            continue;
        }
        t++;
        if (*t == '^')
            cap = TRUE, t++;
        for (len = 0; t[len] && t[len] != '}'; len++)
            ;
        for (slot = 0; slot < TS_COUNT; slot++)
        {
            const char *n = sTextSlots[slot];
            u8 i;
            if (n[0] != t[0])
                continue;
            for (i = 0; i < len && n[i] == t[i]; i++)
                ;
            if (i == len && n[i] == 0)
                break;
        }
        start = out;
        if (slot >= TS_COUNT || (out = TextSlot(out, end, slot, c, depth)) == NULL)
        {
            ok = FALSE;
            c->missing++;
            out = start;
            if (out != NULL)
            {
                const char *bad = "{?";
                while (*bad && out < end)
                    *out++ = *bad++;
                for (slot = 0; slot < len && out < end; slot++)
                    *out++ = t[slot];
                if (out < end)
                    *out++ = '}';
            }
        }
        else if (cap && start != NULL && *start >= 'a' && *start <= 'z')
            *start -= 'a' - 'A';
        if (start == NULL)
            out = NULL;
        t += len;
        if (*t == 0)
            break;
    }
    if (out != NULL)
        *out = 0;
    if (out == NULL)
        return ok ? (char *)1 : NULL;
    return out;
}

// TRUE if a template names a slot that needs the town's facts worked out.
static bool8 TextNeedsFacts(const char *t)
{
    for (; *t; t++)
    {
        if (*t != '{')
            continue;
        t++;
        if (*t == '^')
            t++;
        // TRADE, TRADE_FOLK, GOODS, LANDMARK, LOCALMON, PRIDE, PROBLEM_AT,
        // NEIGHBOR (and FEUD, which needs it); not TOWN, TYPE, LEADER, NEXT,
        // PLACE and the like, which would work the facts out for nothing
        // (it takes the best part of a frame on the GBA).
        if ((t[0] == 'T' && t[1] == 'R') || (t[0] == 'G' && t[1] == 'O') || (t[0] == 'L' && (t[1] == 'A' || t[1] == 'O'))
         || (t[0] == 'P' && t[1] == 'R') || (t[0] == 'N' && t[1] == 'E' && t[2] == 'I') || (t[0] == 'F' && t[1] == 'E' && t[2] == 'U'))
            return TRUE;
    }
    return FALSE;
}

bool8 RogueText_CanFill(const char *t, struct RogueTextCtx *c)
{
    u8 missing = c->missing;
    bool8 ok = TextFillTo(NULL, NULL, t, c, 0) != NULL;
    c->missing = missing;
    return ok;
}

// The town's facts are worked out once for a fill, unless the caller has
// them already (c->facts).
u32 RogueText_Fill(char *out, u32 size, const char *t, struct RogueTextCtx *c)
{
    struct RogueTownFacts tf;
    u32 n;
    bool8 own = FALSE;
    if (c->facts == NULL && TextTown(c, c->town) && TextNeedsFacts(t))
    {
        RogueGen_TownFacts(c->r, c->town, &tf);
        c->facts = &tf;
        own = TRUE;
    }
    n = TextFillTo(out, out + size - 1, t, c, 0) - out;
    if (own)
        c->facts = NULL;
    return n;
}

// Breaks the text into lines of width characters: a space becomes a line
// break, then scrolls of the box ('\v'); '|' starts a new box ('\f').
// Breaks already there are kept. Returns FALSE if a word is too long.
bool8 RogueText_Wrap(char *s, u32 width)
{
    u32 col = 0, line = 0;
    char *space = NULL;
    bool8 ok = TRUE;
    for (; *s; s++)
    {
        if (*s == '|' || *s == '\f')
        {
            *s = '\f';
            col = line = 0;
            space = NULL;
            continue;
        }
        if (*s == '\n' || *s == '\v')
        {
            col = 0;
            line = 1;
            space = NULL;
            continue;
        }
        if (*s == ' ')
            space = s;
        if (++col > width)
        {
            if (space == NULL)
            {
                ok = FALSE;
                continue;
            }
            *space = line == 0 ? '\n' : '\v';
            line = 1;
            col = s - space;
            space = NULL;
        }
    }
    return ok;
}

// A template filled and wrapped into out. Returns the length, 0 if a slot
// could not be filled (out then shows {?NAME}).
u32 RogueText_Say(char *out, u32 size, const char *t, struct RogueTextCtx *c)
{
    u8 missing = c->missing;
    u32 n = RogueText_Fill(out, size, t, c);
    const char *s;
    // A template with breaks of its own (lines moved from the game) keeps
    // them; the rest are broken here.
    for (s = t; *s && *s != '\n' && *s != '\v'; s++)
        ;
    RogueText_Wrap(out, *s ? 0xFF : TEXT_LINE);
    return c->missing != missing ? 0 : n;
}

// A bank's key: its name, hashed, so picks stay put across builds.
static u32 TextBankKey(const struct RogueTextBank *b)
{
    const char *s = b->name;
    u32 h = 41000;
    while (*s)
        h = h * 31 + (u8)*s++;
    return h;
}

// The k-th line of a bank for one place's speaker: a place's people take
// the lines in turn (k = their order there), each place starting somewhere
// else in the bank. Lines whose slots cannot be filled are passed over.
const char *RogueText_Pick(const struct RogueTextBank *b, struct RogueTextCtx *c, u32 k)
{
    u32 key = TextBankKey(b) ^ ((u32)c->place << 20) ^ ((u32)c->speaker << 26), i;
    for (i = 0; i < b->count; i++)
    {
        const char *t = b->lines[RogueText_Deal(c->r->seed, key, k + i, b->count)];
        if (RogueText_CanFill(t, c))
            return t;
    }
    return NULL;
}

// The k-th line of a bank over the whole region: k counts the uses across
// the region (a town's number and the person's turn there, say), so the
// region hears every line before any twice.
const char *RogueText_PickRegion(const struct RogueTextBank *b, struct RogueTextCtx *c, u32 k)
{
    u32 key = TextBankKey(b), i;
    for (i = 0; i < b->count; i++)
    {
        const char *t = b->lines[RogueText_Deal(c->r->seed, key, k + i, b->count)];
        if (RogueText_CanFill(t, c))
            return t;
    }
    return NULL;
}

// Frames: what comes before a line said a certain way (a TV report, a book).
// Optional; a domain may keep its own.
static const char *const sTextFrames[RSPK_COUNT] =
{
    [RSPK_TV]   = "There's a show on TV.|",
    [RSPK_BOOK] = "It's a book: TALES OF {REGION}.|",
};

const char *RogueText_Frame(u8 speaker)
{
    return speaker < RSPK_COUNT && sTextFrames[speaker] ? sTextFrames[speaker] : "";
}

// ---------------------------------------------------------------------------
// The dump (host only): every line a region can show, for gen_host --script
// and tools/roguemon/scripttest.py.

#ifdef ROGUE_HOST
static const char *const sSpeakerNames[RSPK_COUNT] =
{
    [RSPK_NONE] = "-", [RSPK_SIGN] = "sign", [RSPK_FOLK] = "folk", [RSPK_HOME] = "home", [RSPK_TRAINER] = "trainer",
    [RSPK_GYM] = "gym", [RSPK_LEADER] = "leader", [RSPK_E4] = "e4", [RSPK_CHAMPION] = "champion",
    [RSPK_RIVAL] = "rival", [RSPK_VILLAIN] = "villain", [RSPK_WANDERER] = "wanderer", [RSPK_REPORTER] = "reporter",
    [RSPK_ELDER] = "elder", [RSPK_EXPLORER] = "explorer", [RSPK_BIRCH] = "birch", [RSPK_AIDE] = "aide",
    [RSPK_TV] = "tv", [RSPK_BOOK] = "book", [RSPK_CLERK] = "clerk", [RSPK_NURSE] = "nurse", [RSPK_PET] = "pet",
};

// One line: its domain, the state it is shown in, where (a place or 255),
// who says it, a label (which person, which line), the bank and index it came
// from ("-" for none), and the text as shown ('/' a line break, '\\' a
// scroll, '|' a new box).
void TextDump_Line(struct TextDump *d, u8 place, u8 speaker, const char *who, const char *bank, s32 index, const char *text)
{
    char name[48];
    const char *c;
    if (place < d->r->nPlaces)
        RogueGen_PlaceName(d->r, place, name);
    else
        LoreCopy(name, "-");
    fprintf(d->f, "line\t%s\t%s\t%d\t%s\t%s\t%s\t%s:%d\t", d->domain, d->stateName, place < d->r->nPlaces ? place : -1, name,
            speaker < RSPK_COUNT && sSpeakerNames[speaker] ? sSpeakerNames[speaker] : "?", who ? who : "-", bank ? bank : "-", (int)index);
    // (0x5C is the backslash: tools/scaninc reads a quoted backslash as an
    // open quote, and the build then missed the domains' headers below.)
    for (c = text; *c; c++)
        fputc(*c == '\n' ? '/' : *c == '\f' ? '|' : *c == '\v' ? 0x5C : *c == '\t' ? ' ' : *c, d->f);
    fputc('\n', d->f);
}

// A template said in a context, dumped (wrapped as the game shows it).
void TextDump_Say(struct TextDump *d, struct RogueTextCtx *c, const char *who, const char *bank, s32 index, const char *t)
{
    char out[1000];
    if (t == NULL)
        return;
    c->state = d->state;
    RogueText_Say(out, sizeof(out), t, c);
    TextDump_Line(d, c->place, c->speaker, who, bank, index, out);
}

// A whole bank as one speaker could say it here, each line once (for
// reading the bank, not for the repetition numbers).
void TextDump_Bank(struct TextDump *d, struct RogueTextCtx *c, const char *who, const struct RogueTextBank *b)
{
    u32 i;
    for (i = 0; i < b->count; i++)
        TextDump_Say(d, c, who, b->name, i, b->lines[i]);
}
#endif

// ---------------------------------------------------------------------------
// The domains. Each owns its header; this list does not change.

#include "rogue_text_leaders.h"  // gym leaders, Elite Four, rival, villains, the CHAMPION (the wanderer)
#include "rogue_text_trainers.h" // route and dungeon trainers
#include "rogue_text_towns.h"    // townsfolk, homes, signs, TVs, books, gifts, shops
#include "rogue_text_signatures.h" // town signatures' people, gifts, signs and exhibits
#include "rogue_text_dungeons.h" // dungeon flavor, prizes, the lore ways
#include "rogue_text_birch.h"    // PROF. BIRCH, his aide, run memory, the epilogue
#include "rogue_text_features.h" // route features' people, pairs, gauntlet, route walkers' route facts

#ifdef ROGUE_HOST
typedef void (*TextDumpFunc)(struct TextDump *d);
static const struct { const char *name; TextDumpFunc dump; } sTextDomains[] =
{
    { "facts",    TextDump_Facts },
    { "leaders",  TextDump_Leaders },
    { "trainers", TextDump_Trainers },
    { "towns",    TextDump_Towns },
    { "signatures", TextDump_Signatures },
    { "dungeons", TextDump_Dungeons },
    { "birch",    TextDump_Birch },
    { "features", TextDump_Features },
};

// Every domain's lines for a region in one state.
void RogueText_DumpRegion(const struct RogueRegion *r, FILE *f, const struct RogueTextState *state, const char *stateName)
{
    struct TextDump d;
    u32 i;
    d.r = r;
    d.f = f;
    d.state = state;
    d.stateName = stateName;
    for (i = 0; i < LORE_COUNT_OF(sTextDomains); i++)
    {
        d.domain = sTextDomains[i].name;
        sTextDomains[i].dump(&d);
    }
}
#endif
