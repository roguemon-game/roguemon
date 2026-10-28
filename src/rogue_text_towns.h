// Townsfolk, homes, signs, TVs, books, gifts and shops (the towns domain).
// Included at the end of src/rogue_gen_text.h. The game asks here and
// converts the result (src/rogue_town.c); gen_host --script prints all of it.
//
// What a town's people say comes from its facts (src/rogue_gen_facts.h):
// its trade, its landmark, what it is proud of, its rival town, its local
// POKeMON, its current trouble, the region's festival and famous figure. Each
// town's people take a list of topics in turn (TownAgenda), the town's own
// first, so two towns sound different from the first person you meet. Lines
// carry a tag for who may say them (a kid, a grown-up, an old one: the voice
// of the sprite the game gives them, RogueText_FolkVoice) and what the place
// must have (the sea, the volcano). Homes have families: a name on the sign,
// residents who talk to each other. TVs show the region's own shows and news,
// shelves hold its books, and the reporter pair (RogueCast) turn up in their
// towns and then on TV.
//
// A line's tag: '#' and letters, then a space, before the words:
//   k a kid, a a grown-up, o an old one (any of those given; none: anyone)
//   s the sea at hand, w water at hand (the sea, a lake or a river)
//   v the volcano on the region map, g goods you can carry in a basket,
//   f goods you can eat, b two BADGES or more, h a town of ash
//   a gift's kind (any one of those given, for the gift lines: what is
//   handed over, so nobody's POKeMON "outgrew" a BERRY): H something a
//   POKeMON holds, E something to eat or drink, D a TM, B POKe BALLS,
//   S a stone, T a treasure to sell, C candy, M medicine
// Town signatures (src/rogue_gen_signatures.h, v013-towns) add trades and
// landmarks: banks are looked up by the trade's and landmark's words, so a
// new one falls back on the general banks until it has its own.

// The old town sign mottos, by theme (v0.12.0): one of a sign's choices.
static const char *const sSlogans[RTHEME_COUNT + 1][2] =
{
    [RTHEME_MEADOW] = {
        "Where every journey begins.",
        "A quiet town in the clover.",
    },
    [RTHEME_VERDANT] = {
        "Green hills and fresh air.",
        "A town that heals the heart.",
    },
    [RTHEME_HARBOR] = {
        "Port of a thousand stories.",
        "Where the sea wind never rests.",
    },
    [RTHEME_ISLAND] = {
        "White stones under blue skies.",
        "Red roofs by the open sea.",
    },
    [RTHEME_STONE] = {
        "The city carved from stone.",
        "Solid as the mountain.",
    },
    [RTHEME_COVE] = {
        "Blue roofs by a quiet sea.",
        "Where the land meets the sea.",
    },
    [RTHEME_BEACH] = {
        "Sun, sand and sea spray.",
        "A town of fishers and shells.",
    },
    [RTHEME_ASH] = {
        "Farms in the volcano's shadow.",
        "Grey grass, sweet berries.",
    },
    [RTHEME_SPRING] = {
        "Hot springs at the mountain's foot.",
        "Soak your worries away.",
    },
    [RTHEME_TREETOP] = {
        "A town among the treetops.",
        "Where the forest is home.",
    },
    [RTHEME_RAFT] = {
        "A town afloat on the open sea.",
        "Where the rafts are tied together.",
    },
    [RTHEME_CRATER] = {
        "The city in the white cliffs.",
        "A town round a hidden lake.",
    },
    [RTHEME_COUNT] = {
        "The peak where champions rise.",
        "The last climb of the journey.",
    },
};

static const char *const sInlandSlogans[RTHEME_COUNT + 1] =
{
    [RTHEME_HARBOR] = "Where every road brings trade.",
    [RTHEME_ISLAND] = "White stones under blue skies.",
    [RTHEME_COVE] = "Blue roofs and quiet days.",
    [RTHEME_BEACH] = "Sun and sand all year.",
};

// The same for the old mottos (a bit per motto).
static const u8 sSeaSlogans[RTHEME_COUNT + 1] =
{
    [RTHEME_HARBOR] = 0x3, [RTHEME_ISLAND] = 0x2, [RTHEME_COVE] = 0x3, [RTHEME_BEACH] = 0x3,
    [RTHEME_ASH] = 0x1, [RTHEME_RAFT] = 0x3,
};

// ---------------------------------------------------------------------------
// Town signs: a motto from the town's facts

// A sign's motto, one or two lines under the town's name, in the voice of
// Emerald's ("The town of hot, sweet springs."). Each trade and landmark
// has its own (gRogueTrades, gRogueLandmarks); these banks are for the facts
// only some towns have, and a few that name the facts outright.
TEXT_BANK(sMottoLandmark,
    "Home of {LANDMARK}.",
);
TEXT_BANK(sMottoTrade,
    "A town of {TRADE_FOLK}.",
    "Famous for its {GOODS}.",
);
TEXT_BANK(sMottoPride,
    "Known for {PRIDE}.",
    "Proud of {PRIDE}.",
);
TEXT_BANK(sMottoOld,
    "The oldest town in {REGION}.",
    "Where {REGION} began.",
);
TEXT_BANK(sMottoFestival,
    "Where the {FESTIVAL} began.",
    "Come for the {FESTIVAL}!",
);
TEXT_BANK(sMottoFigure,
    "Hometown of {FIGURE} the {FIGURE_TITLE}.",
    "Where {FIGURE} grew up.",
);
TEXT_BANK(sMottoStart,
    "Where every journey begins.",
    "Every trainer's first step.",
);

// The region map shows a volcano, for the ash theme's lines.
static bool8 TownVolcano(const struct RogueRegion *r)
{
    s8 x, y;
    return RogueGen_FactsVolcano(r, &x, &y);
}

// The sea on the town's own map, as the townsfolk see it (the facts'
// RFACT_NEAR_SEA: a harbor's waterfront or the open sea setting out).
static bool8 TownSea(const struct RogueRegion *r, u8 place)
{
    return RogueGen_TownSeaOnMap(r, place);
}

// TRUE if a theme's sea lines (sSeaFlavor, sSeaSlogans) fit the place: it is
// by the sea, or for ash the region map shows a volcano.
static bool8 TownSeaLinesFit(const struct RogueRegion *r, u8 place, u8 theme)
{
    if (theme == RTHEME_ASH)
        return TownVolcano(r);
    return TownSea(r, place);
}

static u8 TownTheme(const struct RogueRegion *r, u8 place)
{
    const struct RoguePlace *p = &r->places[place];
    if (p->kind == RPLACE_TOWN && p->type == RTOWN_SUMMIT)
        return RTHEME_COUNT;
    return p->theme < RTHEME_COUNT ? p->theme : RTHEME_MEADOW;
}

// The old theme motto, as the sign said it in v0.12.0.
static const char *TownOldSlogan(const struct RogueRegion *r, u8 place)
{
    u8 theme = TownTheme(r, place), k = RogueRng_Hash(r->seed, 8600 + place) & 1;
    if (((sSeaSlogans[theme] >> k) & 1) && !TownSeaLinesFit(r, place, theme))
    {
        k ^= 1;
        if ((sSeaSlogans[theme] >> k) & 1)
            return sInlandSlogans[theme];
    }
    return sSlogans[theme][k];
}

// The template of a town's motto. The facts only this town has come
// first (the first town, the oldest, the festival's, the famous figure's);
// else its landmark's or trade's own motto, one that names its landmark,
// trade or pride, or now and then its theme's old one. Worked out for the
// towns in order, so no two signs of a region say the same.
static const char *TownMottoTemplate(struct RogueTextCtx *c, u8 place)
{
    const struct RogueRegion *r = c->r;
    struct RogueRegionFacts rf;
    // Only the trades and landmarks of the towns up to this one count (the
    // mottos' slots need no more), from one pass that stops here.
    struct FactsPass ps;
    struct FactsCore core[ROGUE_MAX_TOWNS];
    struct RogueTownFacts ft;
    const char *taken[ROGUE_MAX_TOWNS];
    u32 t, n = 0;
    RogueGen_RegionFacts(r, &rf);
    if (!RogueGen_FactsVolcano(r, &ps.vx, &ps.vy))
        ps.vx = -1;
    FactsPassTowns(r, &ps, place, core);
    memset(&ft, 0, sizeof(ft));
    for (t = 0; t < r->nTowns; t++)
    {
        u8 pl = r->towns[t], k;
        u32 rng = RogueRng_Hash(r->seed, FACTS_SALT + 500 + pl), i;
        const char *cand[8], *pick = NULL;
        u32 nc = 0;
        const struct RogueTownFacts *f = &ft;
        ft.place = pl;
        ft.trade = core[t].trade;
        ft.landmark = core[t].landmark;
        struct RogueTextCtx sub = *c;
        if (r->places[pl].type == RTOWN_SUMMIT)
        {
            if (pl == place)
                return TownOldSlogan(r, pl);
            continue;
        }
        sub.town = sub.place = pl;
        sub.facts = f;
        if (pl == r->start)
            cand[nc++] = RogueText_PickRegion(&sMottoStart, &sub, rng);
        else if (pl == rf.oldTown)
            cand[nc++] = RogueText_PickRegion(&sMottoOld, &sub, rng);
        if (pl == rf.festTown)
            cand[nc++] = RogueText_PickRegion(&sMottoFestival, &sub, rng);
        if (pl == rf.figureTown && f->landmark != RLANDMARK_STATUE)
            cand[nc++] = RogueText_PickRegion(&sMottoFigure, &sub, rng);
        if (nc == 0)
        {
            // A landmark's motto only where the map shows it.
            bool8 seen = RogueGen_SignatureLandmarkId(r, pl) == f->landmark;
            cand[nc++] = seen ? gRogueLandmarks[f->landmark].motto : NULL;
            cand[nc++] = gRogueTrades[f->trade].motto;
            cand[nc++] = seen ? gRogueLandmarks[f->landmark].motto : NULL;
            cand[nc++] = gRogueTrades[f->trade].motto;
            cand[nc++] = RogueText_PickRegion(&sMottoLandmark, &sub, rng >> 4);
            cand[nc++] = RogueText_PickRegion(&sMottoTrade, &sub, rng >> 4);
            cand[nc++] = RogueText_PickRegion(&sMottoPride, &sub, rng >> 4);
            if (pl != r->start)
                cand[nc++] = TownOldSlogan(r, pl);
        }
        // The first that fits and no earlier sign says, from a seeded start.
        for (k = 0; k < nc && pick == NULL; k++)
        {
            const char *m = cand[(rng + k) % nc];
            if (m == NULL || !RogueText_CanFill(m, &sub))
                continue;
            for (i = 0; i < n && taken[i] != m; i++)
                ;
            if (i == n)
                pick = m;
        }
        if (pick == NULL)
            pick = TownOldSlogan(r, pl);
        if (pl == place)
            return pick;
        taken[n++] = pick;
    }
    return TownOldSlogan(r, place);
}

// The motto of a town's sign (c->place), wrapped to sit under the town's
// name: its breaks scroll the box, as Rustboro's long motto does. The game
// puts it in curly quotes. A town a legend watches over says so instead
// (the lore, src/rogue_gen_lore.h). Returns the length.
u32 RogueText_TownMotto(struct RogueTextCtx *c, char *out, u32 size)
{
    const struct RogueRegion *r = c->r;
    s32 d = RogueGen_LoreAt(r, LORE_WAY_SIGN, c->place);
    u32 len;
    char *s;

    out[0] = 0;
    if (d >= 0)
        return RogueGen_LoreText(r, d, LORE_MOTTO, LORE_WAYS, out, size);
    c->town = c->place;
    {
        struct RogueTownFacts f;
        RogueGen_TownFacts(r, c->place, &f);
        c->facts = &f;
        len = RogueText_Fill(out, size, TownMottoTemplate(c, c->place), c);
        c->facts = NULL;
    }
    // Under the name, every break scrolls.
    RogueText_Wrap(out, TEXT_LINE - 2);
    for (s = out; *s; s++)
        if (*s == '\n')
            *s = '\v';
    return len;
}

// ---------------------------------------------------------------------------
// Who is talking

// The water a town has in and around it: the sea (its own sea front, or a
// coast or open sea next door), fresh water (a lake or river next door, or a
// pond in the town: m's blocks), or none. The game dresses its people by it
// (src/rogue_town.c: sailors only by the sea, fishermen only by water).
u8 RogueText_TownWater(const struct RogueRegion *r, u8 place, const struct RogueMap *m)
{
    const struct RoguePlace *p = &r->places[place];
    u32 i;
    u8 water = RWATER_NONE;
    if (p->layout == RLAYOUT_HARBOR && p->seaSide != RDIR_NONE)
        return RWATER_SEA;
    for (i = 0; i < p->nLinks; i++)
    {
        const struct RoguePlace *q = &r->places[p->links[i].target];
        if (q->kind != RPLACE_ROUTE)
            continue;
        if (q->type == RROUTE_COAST || q->type == RROUTE_SEA)
            return RWATER_SEA;
        if (q->type == RROUTE_LAKE || q->type == RROUTE_RIVER)
            water = RWATER_FRESH;
    }
    // A pond: its middle is pond water.
    if (water == RWATER_NONE && m != NULL && m->blocks != NULL)
    {
        for (i = 0; i < (u32)m->w * m->h; i++)
        {
            if ((i & 63) == 0)
                YIELD();
            if ((m->blocks[i] & 0x3FF) == 0x0A1)
            {
                water = RWATER_FRESH;
                break;
            }
        }
    }
    return water;
}

// How the townsfolk of each theme look (src/rogue_town.c sTownsfolkGfx, by
// slot), as voices: a kid, a grown-up, an old one. Keep the two in step.
#define VK RVOICE_KID
#define VA RVOICE_ADULT
#define VO RVOICE_OLD
static const u8 sFolkVoice[RTHEME_COUNT + 1][8] =
{
    [RTHEME_MEADOW]  = { VK, VK, VK, VO, VA, VA, VA, VK }, // boy, girl, little boy, old woman, man, woman, fat man, little girl
    [RTHEME_VERDANT] = { VK, VK, VA, VA, VA, VO, VK, VA }, // twin, girl, woman, man, cyclist, old man, boy, pokefan
    [RTHEME_HARBOR]  = { VA, VA, VA, VK, VA, VA, VO, VK }, // sailor, fisherman, beauty, tuber, man, woman, old man, boy
    [RTHEME_ISLAND]  = { VK, VO, VK, VA, VK, VA, VA, VA }, // tuber, old woman, girl, man, little girl, scientist, woman, fisherman
    [RTHEME_STONE]   = { VK, VA, VA, VA, VK, VK, VA, VA }, // school kid, scientist, gentleman, woman, boy, lass, devon man, man
    [RTHEME_COVE]    = { VA, VA, VA, VA, VK, VO, VK, VA }, // artist, woman, sailor, beauty, rich boy, old man, girl, man
    [RTHEME_BEACH]   = { VK, VK, VA, VA, VK, VO, VA, VK }, // tuber, girl, fisherman, sailor, tuber, old man, beauty, boy
    [RTHEME_ASH]     = { VA, VO, VK, VK, VA, VK, VA, VA }, // hiker, old woman, camper, picnicker, man, little boy, fat man, woman
    [RTHEME_SPRING]  = { VO, VO, VA, VO, VA, VK, VA, VA }, // old woman, old man, woman, expert, black belt, little girl, man, beauty
    [RTHEME_TREETOP] = { VA, VK, VK, VO, VK, VK, VK, VA }, // man, girl, bug catcher, old woman, picnicker, boy, ninja boy, woman
    [RTHEME_RAFT]    = { VA, VO, VK, VA, VA, VK, VO, VA }, // fisherman, old man, tuber, sailor, woman, little girl, old woman, swimmer
    [RTHEME_CRATER]  = { VA, VO, VK, VA, VO, VK, VA, VA }, // gentleman, old woman, boy, beauty, expert, lass, man, woman
    [RTHEME_COUNT]   = { VO, VO, VA, VA, VA, VA, VA, VA }, // the summit: experts, man, woman, black belt, hiker, gentleman, beauty
};
// Who stands in for sea folk away from the sea (sLandFolkGfx).
static const u8 sLandFolkVoice[8] = { VA, VA, VK, VK, VO, VA, VA, VK };
// The slots of sea folk (sailors, kids in swim rings) and of fishermen.
static const u8 sFolkSeaSlots[RTHEME_COUNT + 1] = { [RTHEME_HARBOR] = 0x09, [RTHEME_ISLAND] = 0x01, [RTHEME_COVE] = 0x04, [RTHEME_BEACH] = 0x19,
                                                   [RTHEME_RAFT] = 0x8D };
static const u8 sFolkFisherSlots[RTHEME_COUNT + 1] = { [RTHEME_HARBOR] = 0x02, [RTHEME_ISLAND] = 0x80, [RTHEME_BEACH] = 0x04,
                                                      [RTHEME_RAFT] = 0x01 };
#undef VK
#undef VA
#undef VO

static u8 TownTheme(const struct RogueRegion *r, u8 place);

// The voice of the townsperson the game dresses from slot (as
// Rogue_PlaceTownsfolkGfx does) in a town.
u8 RogueText_FolkVoice(const struct RogueRegion *r, u8 place, const struct RogueMap *m, u32 slot)
{
    u8 theme = TownTheme(r, place), s = slot % 8;
    bool8 sea = (sFolkSeaSlots[theme] >> s) & 1, fisher = (sFolkFisherSlots[theme] >> s) & 1;
    if (sea || fisher)
    {
        u8 water = RogueText_TownWater(r, place, m);
        if ((sea && water != RWATER_SEA) || (fisher && water == RWATER_NONE))
            return sLandFolkVoice[s];
    }
    return sFolkVoice[theme][s];
}

// The look (a townsfolk slot of the town's theme) of the k-th visitor in
// a town's POKeMON CENTER, and their voice.
// Grown-ups mostly, as in Hoenn's Centers: the first is one, the second
// anyone of another look.
u8 RogueText_CenterFolkSlot(const struct RogueRegion *r, u8 place, const struct RogueMap *m, u32 k)
{
    u32 h = RogueRng_Hash(r->seed, FACTS_SALT + 1700 + place), i, first = 8;
    u32 second = 8;
    for (i = 0; i < 8; i++)
    {
        u32 slot = (h + i * 3) & 7;
        u8 v = RogueText_FolkVoice(r, place, m, slot);
        if (first == 8 && (v == RVOICE_ADULT || i == 7))
            first = slot;
        else if (second == 8 && v != RVOICE_KID && slot != first)
            second = slot;
    }
    return k == 0 ? first : second != 8 ? second : (first + 3 + (h >> 8) % 5) & 7;
}

u8 RogueText_CenterFolkVoice(const struct RogueRegion *r, u8 place, const struct RogueMap *m, u32 k)
{
    return RogueText_FolkVoice(r, place, m, RogueText_CenterFolkSlot(r, place, m, k));
}

// What lines a speaker can take: their voice and what the place has.
struct TownWho
{
    u8 voice; // RVOICE_*
    u8 near;  // RFACT_NEAR_* of the town
    u8 goods; // 1: goods you can carry; 2: and eat
    u8 volcano, badges;
    u8 theme;
    u8 gift;  // TGIFT_* of what is handed over, 0 for none
    const char *avoid; // a line (past its tag) said by someone beside them, not to say again
    u8 local;          // over the region, this use's number in its town (0xFF: none): no line round again there
};

// What kind of thing a gift is, for the gift lines' kind tags.
enum { TGIFT_NONE, TGIFT_HELD, TGIFT_FOOD, TGIFT_TM, TGIFT_BALL, TGIFT_STONE, TGIFT_TREASURE, TGIFT_CANDY, TGIFT_MEDICINE };

static u8 TownGiftKind(u16 item)
{
    if (item == ITEM_NONE)
        return TGIFT_NONE;
    if ((item >= FIRST_BERRY_INDEX && item <= LAST_BERRY_INDEX) || item == ITEM_LAVA_COOKIE || item == ITEM_MOOMOO_MILK)
        return TGIFT_FOOD;
    switch (RogueGen_ItemRole(item))
    {
    case RIR_HELD: case RIR_TYPE: return TGIFT_HELD;
    case RIR_BERRY: return TGIFT_FOOD;
    case RIR_TM: return TGIFT_TM;
    case RIR_BALL: return TGIFT_BALL;
    case RIR_EVO: return TGIFT_STONE;
    case RIR_MONEY: return TGIFT_TREASURE;
    case RIR_CANDY: return TGIFT_CANDY;
    }
    return TGIFT_MEDICINE;
}

#define TAG_KID     (1 << 0)
#define TAG_ADULT   (1 << 1)
#define TAG_OLD     (1 << 2)
#define TAG_SEA     (1 << 3)
#define TAG_WATER   (1 << 4)
#define TAG_VOLCANO (1 << 5)
#define TAG_GOODS   (1 << 6)
#define TAG_FOOD    (1 << 7)
#define TAG_BADGES  (1 << 8)
#define TAG_ASH     (1 << 9)
#define TAG_GIFT(k) (1 << (9 + (k))) // TGIFT_HELD... (10 to 17)
#define TAG_GIFTS   (0xFF << 10)
#define TAG_VOICES  (TAG_KID | TAG_ADULT | TAG_OLD)

// A line's tag (see the top), and the line after it.
static u32 TownTags(const char **t)
{
    const char *s = *t;
    u32 tags = 0;
    if (*s != '#')
        return 0;
    for (s++; *s && *s != ' '; s++)
    {
        switch (*s)
        {
        case 'k': tags |= TAG_KID; break;
        case 'a': tags |= TAG_ADULT; break;
        case 'o': tags |= TAG_OLD; break;
        case 's': tags |= TAG_SEA; break;
        case 'w': tags |= TAG_WATER; break;
        case 'v': tags |= TAG_VOLCANO; break;
        case 'g': tags |= TAG_GOODS; break;
        case 'f': tags |= TAG_FOOD; break;
        case 'b': tags |= TAG_BADGES; break;
        case 'h': tags |= TAG_ASH; break;
        case 'H': tags |= TAG_GIFT(TGIFT_HELD); break;
        case 'E': tags |= TAG_GIFT(TGIFT_FOOD); break;
        case 'D': tags |= TAG_GIFT(TGIFT_TM); break;
        case 'B': tags |= TAG_GIFT(TGIFT_BALL); break;
        case 'S': tags |= TAG_GIFT(TGIFT_STONE); break;
        case 'T': tags |= TAG_GIFT(TGIFT_TREASURE); break;
        case 'C': tags |= TAG_GIFT(TGIFT_CANDY); break;
        case 'M': tags |= TAG_GIFT(TGIFT_MEDICINE); break;
        }
    }
    *t = *s ? s + 1 : s;
    return tags;
}

// TRUE if a speaker may say a line with these tags. anyVoice: whatever
// their voice (when a bank has nothing in it).
static bool8 TownTagsFit(u32 tags, const struct TownWho *w, bool8 anyVoice)
{
    static const u8 sVoiceTag[] = { [RVOICE_ANY] = 0, [RVOICE_KID] = TAG_KID, [RVOICE_ADULT] = TAG_ADULT, [RVOICE_OLD] = TAG_OLD };
    if (!anyVoice && (tags & TAG_VOICES) && w->voice != RVOICE_ANY && !(tags & sVoiceTag[w->voice % 4]))
        return FALSE;
    if ((tags & TAG_SEA) && !(w->near & RFACT_NEAR_SEA))
        return FALSE;
    if ((tags & TAG_WATER) && !(w->near & (RFACT_NEAR_SEA | RFACT_NEAR_LAKE)))
        return FALSE;
    if ((tags & TAG_VOLCANO) && (!w->volcano || !(w->near & RFACT_NEAR_VOLCANO)))
        return FALSE;
    if ((tags & TAG_GOODS) && w->goods == 0)
        return FALSE;
    if ((tags & TAG_FOOD) && w->goods < 2)
        return FALSE;
    if ((tags & TAG_BADGES) && w->badges < 2)
        return FALSE;
    if ((tags & TAG_ASH) && w->theme != RTHEME_ASH)
        return FALSE;
    // A line that names a kind of gift fits only that kind.
    if ((tags & TAG_GIFTS) && (w->gift == TGIFT_NONE || !(tags & TAG_GIFT(w->gift))))
        return FALSE;
    return TRUE;
}

// How a bank is taken (TownPick's spread): a place's own order (each place
// starting somewhere else in the bank), the same but never round again (a
// town's people each say a line of their own, or none: TOWN_ONCE), or over
// the whole region (any other value: k counts the region's uses of the
// bank, a town's number * per + its use there).
#define TOWN_LOCAL 0
#define TOWN_ONCE  1
#define TOWN_SPREAD(c, per) ((u32)(c)->r->nTowns * (per) + 2) // k = a town's number * per + its use

// The k-th line of a bank this speaker can say, the tag left off, or NULL
// if none fits. Locally k counts the lines that fit, so k and k + 1 never
// land on one line while two fit. Over the region (spread) k is a use of
// the bank: the region's every use takes its own line while the bank lasts,
// whoever says it (the lines that fit differ by voice, so counting them
// would deal two speakers one line).
static const char *TownPick(const struct RogueTextBank *b, struct RogueTextCtx *c, const struct TownWho *w, u32 k, u32 spread)
{
    u32 key, i, pass, fits, n;
    u8 fit[49];
    if (b == NULL)
        return NULL;
    key = TextBankKey(b) ^ (spread > TOWN_ONCE ? 0 : (((u32)c->place << 20) ^ ((u32)c->speaker << 26)));
    n = b->count < 48 ? b->count : 48;
    // Over the region a use's own line first (the k-th of the region's
    // order: every use of the region has its own while the bank lasts), if
    // this speaker can say it.
    if (spread > TOWN_ONCE && (w->local == 0xFF || w->local < n))
    {
        const char *t = b->lines[RogueText_Deal(c->r->seed, key, k % n, b->count)];
        if (TownTagsFit(TownTags(&t), w, FALSE) && t != w->avoid && RogueText_CanFill(t, c))
            return t;
    }
    for (pass = 0; pass < 2; pass++)
    {
        // The lines that fit, in the bank's order for this pick (each checked
        // once: a check fills the line's slots, the costly part).
        // The lines of the speaker's own voice come first and the lines
        // anyone may say after them, so two voices seldom meet on one.
        // Over the region the spare lines (past every use's own) only, while
        // any of them fits: the others are other uses' own.
        u32 spare = spread > TOWN_ONCE && spread < n ? spread : n, spareFits = 0;
        for (fits = 0, i = 0; i < 2 * n; i++)
        {
            u32 pos = spare < n ? (spare + i) % n : i % n;
            u32 at = RogueText_Deal(c->r->seed, key, pos, b->count);
            const char *t = b->lines[at];
            u32 tags = TownTags(&t);
            if (pass == 0 ? ((tags & TAG_VOICES) != 0) != (i < n) : i >= n)
                continue;
            if (TownTagsFit(tags, w, pass) && t != w->avoid && RogueText_CanFill(t, c))
            {
                if (spare < n && pos >= spare)
                {
                    // (the spare ones go to the front, in their order)
                    u32 j;
                    for (j = fits; j > spareFits; j--)
                        fit[j] = fit[j - 1];
                    fit[spareFits++] = at;
                    fits++;
                }
                else
                    fit[fits++] = at;
            }
        }
        if (spareFits && w->local == 0xFF)
            fits = spareFits;
        // (Over the region the order is the region's, so each use k takes
        // its own while the lines last.)
        if (fits && (spread != TOWN_ONCE || k < fits) && (spread <= TOWN_ONCE || w->local == 0xFF || w->local < fits))
        {
            const char *t = b->lines[fit[k % fits]];
            TownTags(&t);
            return t;
        }
        if (fits && (spread == TOWN_ONCE || w->local != 0xFF) && pass == 0)
            return NULL; // the voice's lines are all said here
    }
    return NULL;
}

// Says a picked line into out. FALSE if there was none.
static bool8 TownSayPick(struct RogueTextCtx *c, const struct RogueTextBank *b, const struct TownWho *w, u32 k, u32 spread,
                         char *out, u32 size)
{
    const char *t = TownPick(b, c, w, k, spread);
    if (t == NULL)
        return FALSE;
    RogueText_Say(out, size, t, c);
    return TRUE;
}

// ---------------------------------------------------------------------------
// The trades: what each town's people say about the work of the town. A
// STONE city of masons should not sound like one of fossil diggers.

TEXT_BANK(sTradeFishing,
    "#a I've fished off {TOWN} since I could hold a rod. The fish still win most days.",
    "#k Dad's out on the water. He says he'll bring back a fish as big as me!",
    "#o Long ago we fished with nets we tied ourselves. Small nets, but we caught plenty.",
    "#a Half my catch is MAGIKARP. I throw them back. They come back. We're friends now.",
    "Nobody in {TOWN} asks how you are. They ask what's biting.",
    "#ka The whole town smells of fish. You stop noticing. Visitors from {NEIGHBOR} don't.",
    "#a The fishers of {TOWN} are up before the sun. The WINGULL are up before the fishers.",
    "#o Every fisher in {TOWN} has a story about the one that got away. Mine's true.",
);
TEXT_BANK(sTradeBoats,
    "#a A good boat takes a whole winter to build. A bad one takes an afternoon.",
    "#k I'm building my own boat! It's a bucket so far. But it floats!",
    "#o Every boat on this coast has a little of {TOWN} in it. We built most of them.",
    "We name every boat we launch. The last one was called WINGULL'S REVENGE.",
    "#a Saw and hammer from dawn to dusk. My WHISMUR thinks it's music.",
    "#a They buy our boats in {NEIGHBOR}, then tell everyone they built them. We know.",
    "#o I've launched forty boats from {TOWN}. I've cried at every one.",
);
TEXT_BANK(sTradePearls,
    "#a A diver from {TOWN} holds a breath longer than a WAILMER. Well, nearly.",
    "#k When I grow up I'm going to dive for pearls. I practice in the bath!",
    "#o I found a pearl as big as an egg once. Then I sneezed and lost it in the sand.",
    "The divers of {TOWN} go out at dawn, while the water's still. You can see right to the bottom.",
    "#a Pearls are pretty, sure. But the best part is the quiet down there.",
    "#a Every bride in {REGION} wants a pearl from {TOWN}. Every groom wants one cheaper.",
);
TEXT_BANK(sTradeSalt,
    "#a We let the sea dry in the pans and rake up the salt. The sun does most of the work.",
    "#k I licked a salt pan once. Don't do that.",
    "#o Salt keeps fish, salt keeps meat, and salt has kept {TOWN} going for a hundred years.",
    "Our sea salt goes in every kitchen in {REGION}. Taste any soup and you'll taste {TOWN}.",
    "#a Raking salt all day gives you arms like a MACHOKE.",
    "#a {NEIGHBOR} says our salt is too salty. That's what salt is for!",
);
TEXT_BANK(sTradeMarket,
    "#a Buy low, sell high, and never tell a customer what you paid. That's the whole trade.",
    "#k Mom sells things from places I've never heard of. I want to go to all of them.",
    "#o Everything in {REGION} passes through {TOWN} sooner or later. Even the gossip.",
    "Somebody sold my neighbor a rock that turns into a NUGGET. It hasn't yet.",
    "#a I traded a basket of eggs for a hat, and the hat for a bike. Tomorrow, a house!",
    "#a Merchants come all the way from {NEIGHBOR} to sell in {TOWN}. Then they complain about the prices.",
);
TEXT_BANK(sTradeBerries,
    "#a Up at dawn, BERRIES picked by noon, sore back by supper. Best life there is.",
    "#k I eat one BERRY for every ten I pick. Okay, one for every three.",
    "#o My mother planted the first BERRY trees in {TOWN}. Some of them are still giving.",
    "The farmers of {TOWN} can tell a ripe BERRY by the sound it makes when you tap it.",
    "#a Rain at night, sun all day. That's all a BERRY wants. Me too, really.",
    "#k The bird POK@MON eat the BERRIES before we can pick them. I chase them. They win.",
    "#a {LOCALMON} come down to the BERRY rows at dusk. We let them have a few.",
);
TEXT_BANK(sTradeFlowers,
    "#a Every color you see in {TOWN}, somebody planted by hand.",
    "#k I made a crown of flowers for my POK@MON. It ate it.",
    "#o A garden teaches patience. Mine has taught me a great deal of it.",
    "The gardeners of {TOWN} talk to their flowers. Some say the flowers talk back.",
    "#a When the wind blows from the gardens, the whole town smells like a present.",
    "#a We send flowers to every wedding in {REGION}. And a few to {NEIGHBOR}, to be polite.",
);
TEXT_BANK(sTradeHerbs,
    "#a That bitter smell? Medicine drying on the rooftops of {TOWN}. You get used to it.",
    "#k The herb folk gave me something bitter for my cold. It worked, but yuck!",
    "#o I've a root for every ache in {TOWN}. Except the ache of getting old.",
    "The herbalists here say the more bitter it tastes, the better it works. POK@MON disagree.",
    "#a We pick herbs at dawn, while the dew's still on them. They're grumpy after noon.",
    "#a Even the {LOCALMON} round {TOWN} know which leaves to chew for a sore tummy.",
);
TEXT_BANK(sTradeMilk,
    "#a The MILTANK wake me at five. I've stopped arguing with them.",
    "#k I named all the MILTANK. The big one is BUTTERCUP. She's my favorite.",
    "#o Warm MOOMOO MILK before bed, every night of my life. Never had a bad dream.",
    "Milk from {TOWN} is so fresh, it's still warm when it reaches the shops.",
    "#a A happy MILTANK gives the best milk. So we sing to them while we work.",
    "#a They drink our milk all the way to {NEIGHBOR}. They just don't say thank you.",
);
TEXT_BANK(sTradeHoney,
    "#a The COMBEE know every flower round {TOWN}. I just follow them around.",
    "#k I got stung on the nose once. It was worth it. Honey toast!",
    "#o Honey never spoils. There's a jar in my cupboard older than my children.",
    "Everyone in {TOWN} has sticky fingers. It's all the honey!",
    "#a Sweet as honey, that's {TOWN}. Sticky too, so mind your shoes.",
    "#k A {LOCALMON} put its whole face in our honey pot! It looked so happy.",
);
TEXT_BANK(sTradeMining,
    "#a I spend all day under {TOWN}. On my days off, the sun hurts my eyes.",
    "#k My dad's a miner. He comes home black as a GRIMER, and he still hugs me first!",
    "#o I dug the deepest shaft in {TOWN}. Found water, two ZUBAT and somebody's spoon.",
    "The miners whistle on the way up. If you hear a whistle, someone's coming home.",
    "#a Ore from {TOWN} goes into half the bikes in {REGION}. We're proud of that.",
    "#a The mines of {TOWN} are deeper than {NEIGHBOR}'s. That's not bragging. That's measuring.",
);
TEXT_BANK(sTradeStone,
    "#a Every wall in {TOWN} was cut by hand. Mine is the crooked one.",
    "#k I carved my name in a stone. It took all summer. It says JE.",
    "#o A good wall outlives the one who built it. I've got a few still standing.",
    "The masons of {TOWN} can split a boulder with one tap. They've had a lot of practice.",
    "#a Chip, chip, chip. That's the sound of {TOWN} waking up.",
    "#o Half the old walls in {NEIGHBOR} were cut right here. Don't let them tell you otherwise.",
);
TEXT_BANK(sTradeGlass,
    "#av The volcano makes the mess, and we make it pretty. Ash melts into the best glass.",
    "#k I blew a glass bubble once. It was supposed to be a vase. It's a bubble.",
    "#o Glass flutes from {TOWN} have played at every wedding in {REGION} for a hundred years.",
    "The workshops of {TOWN} run hotter than summer. Well, nearly.",
    "#a Hold our glass up to the light. See the gray swirl? That's the ash.",
    "#a A glass {LOCALMON}, a glass BERRY, a glass anything. Just ask. Just don't drop it.",
);
TEXT_BANK(sTradeInns,
    "#a Our inn's been full every night this week. Everyone's back aches the same.",
    "#k You can boil an egg in the spring! I boiled twelve. I'm not hungry anymore.",
    "#o I've soaked in these springs since I was small. Look at me. Not a day over seventy.",
    "Folks come from all over {REGION} to soak in {TOWN}. Then they eat. Then they soak again.",
    "#a Steam all day, every day. My glasses haven't been clear since I moved here.",
    "#a Half our guests come up from {NEIGHBOR}. They say they're here for the view. It's the eggs.",
);
TEXT_BANK(sTradePottery,
    "#a The kiln's been burning since before I was born. We're scared to let it go out now.",
    "#k I made a pot! It came out a bit flat. I'm calling it a plate.",
    "#o Every pot in my kitchen I made myself. The wobbly ones are my favorites.",
    "The potters of {TOWN} sign the bottom of every pot. Go on, check yours!",
    "#a Clay under the nails, clay in the hair. That's how you know a potter.",
    "#a A {LOCALMON} walked across my wet pots last spring. Best sellers I ever made.",
);
TEXT_BANK(sTradeWeaving,
    "#a The looms of {TOWN} click all night. It sounds like rain, if rain were very busy.",
    "#k I'm weaving a scarf for my POK@MON. It's three meters long. It's a small POK@MON.",
    "#o I wove the blanket on every bed in {TOWN}. My fingers know the pattern by heart.",
    "Cloth from {TOWN} is so bright, they say you can see it from {NEIGHBOR}.",
    "#a Every thread here is spun from fluff our POK@MON shed. Nothing goes to waste.",
);
TEXT_BANK(sTradeWood,
    "#a I made the bench you're standing by. Sit, if you like. It's sturdy.",
    "#k I'm carving a POK@MON out of wood. So far I've done the tail.",
    "#o A good chair lasts a lifetime. A great chair lasts three. {TOWN} makes great chairs.",
    "The woodworkers of {TOWN} never cut a tree that's still growing. They wait for the storms.",
    "#a Sawdust in my hair, sawdust in my boots. That's the job.",
    "#a Every table in {NEIGHBOR} came from {TOWN}. They eat off our work and never say so.",
);
TEXT_BANK(sTradeBalls,
    "#a Every POK@ BALL made in {TOWN} gets tested by hand. I've thrown about a million.",
    "#k I found a BALL on the road that won't open. I keep it under my pillow.",
    "#o I made POK@ BALLS from APRICORNS when I was young. Now it's all buttons and springs.",
    "The BALL makers here can hear a bad hinge from across the room. Just by the click.",
    "#a Red on top, white below. If it's the other way round, it came from {NEIGHBOR}.",
    "#a We test new BALLS on the {LOCALMON} out back. They've learned to duck.",
);
TEXT_BANK(sTradeStars,
    "#a I sleep all day and count stars all night. The neighbors think I'm a GHOST POK@MON.",
    "#k I saw a shooting star over {TOWN}! I made a wish. It's a secret!",
    "#o I've mapped every star you can see from {TOWN}. It took me all my life.",
    "On a clear night in {TOWN}, the sky's so full it looks like spilled sugar.",
    "#a The stargazers want every light out at night. Bring your own lantern.",
    "#a You can't see a single star from {NEIGHBOR}. Too many lights. We pity them.",
);
TEXT_BANK(sTradePainting,
    "#a I've painted this street a hundred times. It still won't sit still.",
    "#k I painted my POK@MON. Then it painted me back. With its tail.",
    "#o The light in {TOWN} is why we stay. Every painter says so, and they're right.",
    "There's paint on every door in {TOWN}. Some of it on purpose.",
    "#a Sell a painting, buy more paint. That's the whole economy here.",
    "#a I've painted {LOCALMON} a dozen times. It never sits still. Very modern art.",
);
TEXT_BANK(sTradeShells,
    "#a A SHELL BELL rings truest with a shell the sea has tumbled for years.",
    "#k I found a shell that sounds like the sea! Want to listen?",
    "#o I strung my first SHELL BELL at six. My hands still remember the knot.",
    "Walk the beach at low tide and the shell crafters of {TOWN} will race you for every shell.",
    "#a Hear that tinkling? That's every porch in {TOWN}, catching the wind.",
);
TEXT_BANK(sTradeRopes,
    "#a Every bridge in {TOWN} is rope we twisted ourselves. Don't look down. Well, look a little.",
    "#k I can cross the bridges without holding on! ...Don't tell Mom.",
    "#o I've made rope since I could tie my shoes. My knots will outlive my memory.",
    "The ropemakers of {TOWN} test every bridge by jumping on it. Then they test it again, harder.",
    "#a TANGELA vines make the strongest rope. The TANGELA don't love that.",
);
TEXT_BANK(sTradeFossils,
    "#a I dig all day for old bones. Most days I find old rocks instead.",
    "#k I found a fossil! Okay, it's a rock that looks like a fossil. Same thing!",
    "#o Every hill round {TOWN} was under the sea once. You can still find shells up on the ridges.",
    "The diggers of {TOWN} brush the dirt away one grain at a time. Very patient people.",
    "#a A fossil is just a POK@MON that waited a very long time to say hello.",
    "#a They dig in {NEIGHBOR} too. They find pebbles. We find history.",
);
TEXT_BANK(sTradeTea,
    "#a The tea bushes round {TOWN} get the morning mist. That's the whole secret.",
    "#k Tea tastes like leaves. Because it is leaves. I like juice.",
    "#o I've had tea every afternoon since I was a child. Never once missed it.",
    "Ask anyone in {TOWN} for a cup and you'll get three, a chair and a story.",
    "#a Picking tea is slow work. You get to know the person in the next row.",
);
TEXT_BANK(sTradeBaking,
    "#a The ovens of {TOWN} start at four in the morning. The smell wakes everyone by five.",
    "#k I'm allowed one BERRY pie a week. I've already had next week's.",
    "#o My pie crust recipe is a secret. Even from me. My hands just know it.",
    "Follow your nose in {TOWN}. It always ends up at a bakery.",
    "#a Flour on everything. My POK@MON look like they've seen a GHOST.",
    "#a The {LOCALMON} round here have learned what time the pies cool. Clever things.",
);
TEXT_BANK(sTradeLumber,
    "#a We cut one tree, we plant two. The woods round {TOWN} are bigger now than when I started.",
    "#k I counted the rings on a stump. It was two hundred years old!",
    "#o I felled trees all my life and never once spoiled a nest. That's the rule in {TOWN}.",
    "The loggers shout before a tree comes down. Then the POK@MON shout back.",
    "#a Strong arms, strong backs, early nights. That's logging.",
    "#a Half the houses in {NEIGHBOR} are built of {TOWN} timber. We knock on their walls to say hello.",
);
// The town signatures' own trades (src/rogue_gen_signatures.h).
TEXT_BANK(sTradeSpace,
    "#a {TOWN} launches a rocket every spring. Every spring it comes back down. Science!",
    "#k I'm going to be the first TRAINER on the moon. CLEFAIRY can come too.",
    "#o Stars were just for looking at, once. Now they want to go and visit them!",
    "The scientists of {TOWN} say the sky is only the beginning. They say it a lot.",
    "#a My whole job is counting down from ten. Very, very carefully.",
);
TEXT_BANK(sTradeWeather,
    "#a I said rain today. Look at that sun. Nobody's perfect.",
    "#k CASTFORM changed shape this morning! That means... something!",
    "#o My bad hip forecasts better than any machine. It's never wrong about rain.",
    "The forecasters of {TOWN} know a storm's coming two days early, and ring the bell.",
    "#a Watching clouds is real work. Ask my neck.",
);
TEXT_BANK(sTradeFighting,
    "#a One hundred push-ups before breakfast! Breakfast is the reward.",
    "#k I can break a board with my hand! A small board. A thin one.",
    "#o I trained all my life. Now I train the young ones. They're tougher than me.",
    "Everyone in {TOWN} trains. Even the shopkeepers. Don't haggle too hard.",
    "#a A strong body makes a strong heart. A strong heart makes a strong team.",
);
// Any trade without a bank of its own.
TEXT_BANK(sTradeAny,
    "#a Most folks here are {TRADE_FOLK}. Me too. Hard work, honest work.",
    "#k I'm going to be one of the {TRADE_FOLK} when I grow up. Or a CHAMPION. Maybe both!",
    "#o {TOWN} has lived by {TRADE} for as long as anyone remembers. Longer, even.",
    "Ask anyone in {TOWN} about {TRADE}. Go on. Then try to get away.",
);

// Which bank a trade's people use, by the trade's words (gRogueTrades.name),
// so new trades need no change here until they get their own.
static const struct { const char *trade; const struct RogueTextBank *bank; } sTradeBanks[] =
{
    { "fishing", &sTradeFishing }, { "boatbuilding", &sTradeBoats }, { "pearl diving", &sTradePearls },
    { "salt making", &sTradeSalt }, { "trade", &sTradeMarket }, { "berry farming", &sTradeBerries },
    { "flower growing", &sTradeFlowers }, { "herb medicine", &sTradeHerbs }, { "dairy farming", &sTradeMilk },
    { "beekeeping", &sTradeHoney }, { "mining", &sTradeMining }, { "stonecutting", &sTradeStone },
    { "glassblowing", &sTradeGlass }, { "hot spring inns", &sTradeInns }, { "pottery", &sTradePottery },
    { "weaving", &sTradeWeaving }, { "woodcraft", &sTradeWood }, { "BALL making", &sTradeBalls },
    { "stargazing", &sTradeStars }, { "painting", &sTradePainting }, { "shell crafts", &sTradeShells },
    { "rope making", &sTradeRopes }, { "fossil digging", &sTradeFossils }, { "tea growing", &sTradeTea },
    { "baking", &sTradeBaking }, { "logging", &sTradeLumber }, { "space research", &sTradeSpace },
    { "weather watching", &sTradeWeather }, { "martial arts", &sTradeFighting },
};

// Trades whose goods fit in a basket ("some {GOODS}"), and of those, the
// ones you can eat.
static const char *const sCarryTrades[] =
{
    "fishing", "salt making", "berry farming", "herb medicine", "dairy farming", "beekeeping", "hot spring inns",
    "tea growing", "baking", // food first: sFoodTrades is these
    "pearl diving", "flower growing", "glassblowing", "pottery", "weaving", "BALL making", "stargazing", "painting",
    "shell crafts",
};
#define FOOD_TRADES 9

static const struct RogueTextBank *TownTradeBank(u8 trade)
{
    u32 i;
    if (trade >= RTRADE_COUNT)
        return NULL;
    for (i = 0; i < LORE_COUNT_OF(sTradeBanks); i++)
        if (FactsSame(sTradeBanks[i].trade, gRogueTrades[trade].name))
            return sTradeBanks[i].bank;
    return &sTradeAny;
}

// 0: goods you can't carry, 1: you can, 2: and eat.
static u8 TownGoodsKind(u8 trade)
{
    u32 i;
    if (trade >= RTRADE_COUNT)
        return 0;
    for (i = 0; i < LORE_COUNT_OF(sCarryTrades); i++)
        if (FactsSame(sCarryTrades[i], gRogueTrades[trade].name))
            return i < FOOD_TRADES ? 2 : 1;
    return 0;
}

// ---------------------------------------------------------------------------
// The landmarks: part of people's lives. Until a town signature builds one
// on the map nobody sends the player to look for it.

TEXT_BANK(sLmLighthouse,
    "#o My father kept {LANDMARK} lit for forty years. Not one boat lost.",
    "#k I counted the stairs up {LANDMARK}. A hundred and twelve. Or a hundred and ten.",
    "On foggy nights the beam of {LANDMARK} sweeps the whole town. You can read by it.",
);
TEXT_BANK(sLmPier,
    "#a {^LANDMARK} creaks like an old man. It's held up longer than most of them.",
    "#k I fished off {LANDMARK} all day and caught a boot. A nice boot, though.",
    "#o I met my partner on {LANDMARK}. They pushed me in. I pushed back. We've been together ever since.",
);
TEXT_BANK(sLmMarket,
    "#a {^LANDMARK} opens early. By noon it's sold out of everything worth having.",
    "#k I got lost in {LANDMARK} once. A KECLEON helped me find the way out!",
    "You can buy anything at {LANDMARK}. Except a quiet afternoon.",
);
TEXT_BANK(sLmWindmill,
    "#o {^LANDMARK} has turned for a hundred years. It squeaks now, and so do my knees.",
    "#k When the wind's up, {LANDMARK} spins so fast it hums! I hum along.",
    "#a {^LANDMARK} grinds every grain in town. On still days we just wait.",
);
TEXT_BANK(sLmGreatTree,
    "#o {^LANDMARK} was old when {TOWN} was young. We built around it, politely.",
    "#k I climbed halfway up {LANDMARK} once. The other half is for next year.",
    "Every bird POK@MON for miles sleeps in {LANDMARK}. Mornings are loud.",
);
TEXT_BANK(sLmClock,
    "#a {^LANDMARK} is never late. The people of {TOWN} are, but never the clock.",
    "#k When {LANDMARK} rings at noon, I'm allowed lunch. I'm always ready early.",
    "#o I wound {LANDMARK} every week until my knees gave out. Now my son does it.",
);
TEXT_BANK(sLmBridge,
    "#o {^LANDMARK} was here first. The town came along to see what it was for.",
    "#k I dropped a stick off {LANDMARK} and raced it downstream. The stick won.",
    "#a Couples carve their names on {LANDMARK}. The masons sigh and sand them off.",
);
TEXT_BANK(sLmStatue,
    "#o {FIGURE} grew up two doors down from me. Now there's a statue. Funny old world.",
    "#k I climbed {LANDMARK} on a dare. I got as far as the knee.",
    "People come from all over to see {LANDMARK}. Then they ask where to eat.",
);
TEXT_BANK(sLmMuseum,
    "#a {^LANDMARK} has a fossil older than {REGION} itself. It doesn't look a day over a million.",
    "#k We go to {LANDMARK} every month on school trips. I know every room by heart!",
    "#o I gave {LANDMARK} my mother's old POK@ BALL. It's behind glass now.",
);
TEXT_BANK(sLmTelescope,
    "#a You can see the moon's spots through {LANDMARK}. And my neighbor's laundry.",
    "#k I looked through {LANDMARK} and saw a CLEFAIRY on the moon! ...It was a smudge.",
    "#o They built {LANDMARK} years ago. I'm still waiting for my turn to look.",
);
TEXT_BANK(sLmBathhouse,
    "#o I've soaked at {LANDMARK} since the day it opened. That was a long time ago.",
    "#k At {LANDMARK} you have to be quiet. I'm very bad at it.",
    "#a The water at {LANDMARK} comes straight up from under the mountain. Hot enough to cook in.",
);
TEXT_BANK(sLmKiln,
    "#a The fires at {LANDMARK} haven't gone out in a hundred years.",
    "#k I watched them make a glass SWABLU at {LANDMARK}. It took all morning!",
    "Every window in {TOWN} came out of {LANDMARK}. That's why they're a little gray.",
);
TEXT_BANK(sLmGallery,
    "#a {^LANDMARK} hangs one new painting a month. Mine has been next for six years.",
    "#k There's a painting at {LANDMARK} of a SMEARGLE painting a painting. My head hurts.",
    "#o I sat for a painter at {LANDMARK} as a child. I'm the one by the window.",
);
TEXT_BANK(sLmFields,
    "#k I made a crown in {LANDMARK}! A COMBEE wanted it more.",
    "#a In spring, {LANDMARK} goes on forever. People get lost in it on purpose.",
    "#o I tried to propose in {LANDMARK}. I sneezed through the whole thing, but they still said yes.",
);
TEXT_BANK(sLmOrchard,
    "#a {^LANDMARK} feeds half the town and all of the POK@MON.",
    "#k I'm not supposed to climb the trees in {LANDMARK}. The BERRIES taste better up there.",
    "#o I planted a row in {LANDMARK} the year I was married. Those trees are older than my kids.",
);
TEXT_BANK(sLmWell,
    "#k I wished on {LANDMARK} for a DRAGON. I got a cold. Close enough.",
    "#a There's a fortune in coins at the bottom of {LANDMARK}. Nobody's brave enough.",
    "#o Every wish I made at {LANDMARK} came true. I only ever wished for rain.",
);
TEXT_BANK(sLmMine,
    "#a {^LANDMARK} ran dry years ago. The ZUBAT moved in. They pay no rent.",
    "#k There's a ghost in {LANDMARK}! Or a GEODUDE. Something rolled at me.",
    "#o I worked {LANDMARK} all my life. I still wake up before the sun and reach for my lamp.",
);
TEXT_BANK(sLmMines,
    "#a The carts come up out of {LANDMARK} all day. Half of them are full of GEODUDE.",
    "#k My dad works in {LANDMARK}. He comes home all dusty and tells me about tunnels.",
    "#o I dug in {LANDMARK} for years. My MACHOP still lifts more than I do.",
);
TEXT_BANK(sLmWaterwheel,
    "#a {^LANDMARK} turns the millstones all day. On a busy day it turns my head, too.",
    "#k I rode {LANDMARK} once, all the way round! I got very wet.",
    "#o Shh. Listen. That creak is {LANDMARK}. I'd know it anywhere.",
);
TEXT_BANK(sLmShellHall,
    "#a {^LANDMARK} has a shell from every beach in {REGION}. Go and see!",
    "#k In {LANDMARK} there's a shell so big I could sleep in it. I asked. They said no.",
    "#o I gave {LANDMARK} its very first shell. Small and pink. It's still on the left.",
);
TEXT_BANK(sLmSkyWalk,
    "#a {^LANDMARK} runs from tree to tree across the town. Some days I never touch the ground.",
    "#k I ran the whole of {LANDMARK} without stopping! My legs still wobble.",
    "#o I tied the first knot of {LANDMARK}. It's never slipped.",
);
TEXT_BANK(sLmTeaHouse,
    "#o I've had the same seat at {LANDMARK} for years. Everyone knows not to take it.",
    "#a Every bit of gossip in {TOWN} gets poured out at {LANDMARK} first.",
    "#k They give kids milk at {LANDMARK}. Milk! I'm almost ten!",
);
TEXT_BANK(sLmBell,
    "#o {^LANDMARK} rang when every child here was born. It rang for me, too, they say.",
    "#k If you ring {LANDMARK} for no reason, the whole town comes running. I know that now.",
    "#a {^LANDMARK} rings at dawn and dusk. The bird POK@MON sing along.",
);
TEXT_BANK(sLmDocks,
    "#a {^LANDMARK} launches a boat every spring. The whole town comes down to cheer.",
    "#k I'm allowed into {LANDMARK} on my birthday. I get to hold a nail.",
    "#o I built hulls at {LANDMARK} all my working life. Never one that sank.",
);
TEXT_BANK(sLmBakery,
    "#a {^LANDMARK} has baked bread for the whole town since before anyone can remember.",
    "#k On holidays {LANDMARK} makes BERRY buns. I wait outside from six.",
    "#o In the cold years we all gathered round {LANDMARK} to keep warm. Best-smelling winters ever.",
);
// The town signatures' landmarks, built on the map.
TEXT_BANK(sLmHotSpring,
    "#o A soak in {LANDMARK} cures anything. Almost anything. My knees disagree.",
    "#k You can boil an egg in {LANDMARK}! Nobody believed me. Then I did it.",
    "#a {^LANDMARK} smells of eggs. You stop noticing after a year or two.",
);
TEXT_BANK(sLmSpace,
    "#a {^LANDMARK} counts every launch down out loud. The whole town counts along.",
    "#k I'm going to work at {LANDMARK}! I've already got a helmet. It's a pot.",
    "#o When {LANDMARK} launched its first rocket, I hid under the table. I'm still not sure about it.",
);
TEXT_BANK(sLmWeather,
    "#a {^LANDMARK} said sunny today. I brought an umbrella just in case.",
    "#k The people at {LANDMARK} have a CASTFORM. It turned into a raindrop! Then it rained!",
    "#o Before {LANDMARK}, we watched the WINGULL. They were right more often, honestly.",
);
TEXT_BANK(sLmDojo,
    "#a The shouting from {LANDMARK} starts early. You learn to sleep through it.",
    "#k I'm joining {LANDMARK} when I'm bigger. I practice kicks on my pillow.",
    "#o I trained at {LANDMARK} when I was a youngster. My back still remembers.",
);
TEXT_BANK(sLmRanch,
    "#a The MILTANK at {LANDMARK} give the creamiest milk around. Ask them. Well, they'll moo.",
    "#k I help at {LANDMARK} on weekends! A MAREEP zapped me. My hair's still fuzzy.",
    "#o I've watched {LANDMARK} grow from one MILTANK to a whole herd. She was a good one.",
);
TEXT_BANK(sLmDig,
    "#a They found a jaw at {LANDMARK} last month. Nobody knows what from. Exciting!",
    "#k I dug at {LANDMARK} all day! I found a button. It's probably ancient.",
    "#o {^LANDMARK} used to be my cow field. Then somebody found a bone in it.",
);
TEXT_BANK(sLmPond,
    "#a I fish {LANDMARK} after work. The same MAGIKARP keeps biting. We know each other now.",
    "#k I caught something in {LANDMARK}! It was my own hat.",
    "#o {^LANDMARK} is deeper than it looks. So are the people who fish it.",
);
TEXT_BANK(sLmAny,
    "#a Everyone in {TOWN} has a story about {LANDMARK}. I have three. Want one?",
    "#k When I grow up I'm going to live right next to {LANDMARK}!",
    "#o {^LANDMARK} was here before my time, and it'll be here after. That's a comfort.",
);

// Which bank a landmark's people use, by its name (gRogueLandmarks.name).
static const struct { const char *landmark; const struct RogueTextBank *bank; } sLandmarkBanks[] =
{
    { " LIGHTHOUSE", &sLmLighthouse }, { "the OLD PIER", &sLmPier }, { " MARKET", &sLmMarket },
    { "the OLD WINDMILL", &sLmWindmill }, { "the GREAT TREE", &sLmGreatTree }, { "the CLOCK TOWER", &sLmClock },
    { "the STONE BRIDGE", &sLmBridge }, { "the STATUE of *", &sLmStatue }, { " MUSEUM", &sLmMuseum },
    { "the BIG TELESCOPE", &sLmTelescope }, { "the OLD BATHHOUSE", &sLmBathhouse }, { "the GLASS WORKSHOP", &sLmKiln },
    { "the ART GALLERY", &sLmGallery }, { "the FLOWER FIELDS", &sLmFields }, { "the BERRY ORCHARD", &sLmOrchard },
    { "the WISHING WELL", &sLmWell }, { "the OLD MINE", &sLmMine }, { " MINES", &sLmMines }, { "the WATERWHEEL", &sLmWaterwheel },
    { "the SHELL HALL", &sLmShellHall }, { "the SKY WALK", &sLmSkyWalk }, { "the TEA HOUSE", &sLmTeaHouse },
    { "the OLD BELL", &sLmBell }, { "the SHIPYARD", &sLmDocks }, { "the BIG OVEN", &sLmBakery },
    { "the HOT SPRING", &sLmHotSpring }, { " SPACE CENTER", &sLmSpace }, { "the WEATHER INSTITUTE", &sLmWeather },
    { " DOJO", &sLmDojo }, { "the RANCH", &sLmRanch }, { "the FOSSIL DIG", &sLmDig }, { "the FISHING POND", &sLmPond },
};

static const struct RogueTextBank *TownLandmarkBank(u8 landmark)
{
    u32 i;
    if (landmark >= RLANDMARK_COUNT)
        return NULL;
    for (i = 0; i < LORE_COUNT_OF(sLandmarkBanks); i++)
        if (FactsSame(sLandmarkBanks[i].landmark, gRogueLandmarks[landmark].name))
            return sLandmarkBanks[i].bank;
    return &sLmAny;
}

// ---------------------------------------------------------------------------
// What a town is proud of, its rival town, its local POKeMON

TEXT_BANK(sTownPride,
    "#a Everybody says {TOWN} has {PRIDE}. Well, everybody here does.",
    "#k My teacher says we have {PRIDE}. I'm going to check.",
    "#o When I was young, nobody had heard of {TOWN}. Now we're known for {PRIDE}.",
    "Ask anyone in {TOWN} what we're proud of. They'll say {PRIDE}. Then they won't stop.",
    "#a We've got {PRIDE}. {NEIGHBOR} says otherwise. {NEIGHBOR} is wrong.",
    "#ka Put it on the sign, I say: {PRIDE}. They never listen to me.",
    "#o {PRIDE}. That's {TOWN} for you. Has been for a long, long time.",
    "Visitors always ask about {PRIDE}. We're happy to tell them all about it.",
);
TEXT_BANK(sTownRival,
    "{NEIGHBOR}? We've argued with them about {FEUD} for years. We're right, of course.",
    "#o My father argued with {NEIGHBOR} about {FEUD}. So did his. It's tradition.",
    "#k The kids from {NEIGHBOR} say they're better than us. They're not. Tell them.",
    "#a Heading to {NEIGHBOR}? Tell them {TOWN} says hello. Loudly. They hate that.",
    "#a The people in {NEIGHBOR} are nice enough. Wrong about everything, but nice.",
    "Every year {TOWN} and {NEIGHBOR} squabble over {FEUD}. Every year we both say we won.",
    "#a My partner's from {NEIGHBOR}. We don't talk about {FEUD} at the dinner table.",
    "#k I have a pen pal in {NEIGHBOR}. We agree on everything except {FEUD}.",
);
TEXT_BANK(sTownLocalMon,
    "#a {LOCALMON} keep getting into my garden. I've named three of them.",
    "#k I saw {LOCALMON} by the road this morning! It looked right at me!",
    "#o {LOCALMON} lived round here long before people did. We're the guests, really.",
    "#a My kid wants {LOCALMON} for a birthday. I said go and catch one yourself.",
    "#k {LOCALMON} took my ball and ran off! I'm not even mad. It looked so happy.",
    "If you hear rustling by the road, it's {LOCALMON}. It's always {LOCALMON}.",
    "#o I used to leave crumbs out for {LOCALMON}. Now their young ones come too.",
    "#a We had {LOCALMON} on our doorstep all winter. Best guard in {TOWN}.",
    "#a At dusk, {LOCALMON} come out by the road, like they're waiting for someone.",
);
// The species on the road out of town (ctx.area, ctx.mon), and the road.
TEXT_BANK(sTownWild,
    "I see {MON} on {ROUTE} all the time. It's practically a neighbor.",
    "#k {MON} chased me all the way down {ROUTE}! I was only a little scared.",
    "#a My kid wants {MON} from {ROUTE} so badly. Every day it's {MON} this, {MON} that.",
    "#ka I saw {MON} on {ROUTE} this morning. So cute!",
    "#o My father caught {MON} on {ROUTE} when he was a boy. Or so he always said.",
    "#a {MON} on {ROUTE} took an apple right out of my hand. Fair enough. It was a good apple.",
    "Keep an eye out on {ROUTE}. {MON} likes to jump out and say hello.",
    "#k There's {MON} on {ROUTE} that's bigger than me! Well, bigger than my brother.",
    "#a Off down {ROUTE}? Wave at {MON} for me. We go way back.",
    "{MON} sleeps in the shade on {ROUTE} some afternoons. It snores.",
);

// ---------------------------------------------------------------------------
// What troubles a town now (RogueTownFacts.problem), and how it changes
// with what the player does: one bank before, one after.

TEXT_BANK(sProbVillains,
    "#a Those {TEAM} types moved in at the edge of town. They don't buy anything. They just stand there.",
    "#k I'm not supposed to talk to the people in the funny uniforms. I waved. They waved back!",
    "#o In all my years nobody's dug a hideout under our town. {TEAM}... Hmph. Rude.",
);
TEXT_BANK(sProbVillainsAfter,
    "#a {TEAM} packed up and left. The road's ours again. Thank you, TRAINER.",
    "#k Did you chase off {TEAM}? Everyone says it was a kid like me!",
    "#o Quiet again. I'd almost forgotten what that sounded like.",
);
TEXT_BANK(sProbSighted,
    "#a People in odd uniforms were poking about on {PROBLEM_AT}. {TEAM}, they called themselves.",
    "#k I saw {TEAM} on {PROBLEM_AT}! They were digging for something!",
    "#o Strangers on {PROBLEM_AT}, asking about the old stories. I told them nothing. Hah!",
);
TEXT_BANK(sProbSightedAfter,
    "#a Haven't seen those {TEAM} people on {PROBLEM_AT} in days. Good riddance.",
    "Somebody sent {TEAM} packing, I hear. {PROBLEM_AT} is quiet again.",
);
TEXT_BANK(sProbLost,
    "#a {EXPLORER} went into {PROBLEM_AT} with a map and a lantern. That was days ago.",
    "#k {EXPLORER} promised me a rock from {PROBLEM_AT}. They haven't come back yet.",
    "#o {PROBLEM_AT} goes deeper than anyone's map. {EXPLORER} should know that. Still not home.",
);
TEXT_BANK(sProbLostAfter,
    "#a {EXPLORER}'s home! Says a TRAINER walked them out of {PROBLEM_AT}. Was that you?",
    "#k {EXPLORER} brought me a rock from {PROBLEM_AT}! It's just a rock. I love it.",
);
TEXT_BANK(sProbLegend,
    "#a At night you can feel the ground hum near {PROBLEM_AT}. Something in there is awake.",
    "#k The grown-ups say stay away from {PROBLEM_AT}. Something big lives there. I want to see it!",
    "#o {PROBLEM_AT} has been restless lately. It was restless when I was a child, too. Then it slept.",
);
TEXT_BANK(sProbLegendAfter,
    "#a The humming from {PROBLEM_AT} has stopped. Whatever was in there is at peace now.",
    "#o Quiet nights again. Whoever went into {PROBLEM_AT}, thank you.",
);
TEXT_BANK(sProbBlocked,
    "#a The way onto {PROBLEM_AT} is blocked. Nobody in {TOWN} has a POK@MON that can clear it.",
    "#k I can see {PROBLEM_AT} from here, but I can't get to it. Not fair!",
    "#o The way to {PROBLEM_AT} closed up long ago. A TRAINER with the right BADGE could open it.",
);
TEXT_BANK(sProbBlockedAfter,
    "#a The way to {PROBLEM_AT} is open again! Our deliveries can get through.",
    "#k Somebody opened the way to {PROBLEM_AT}! I'm going exploring there tomorrow!",
);
TEXT_BANK(sProbPower,
    "#a The lights keep flickering since {PROBLEM_AT} shut down. I eat supper by candlelight.",
    "#k {PROBLEM_AT} makes a scary humming sound. Everyone says it's just the wires. I'm not so sure.",
    "#o When {PROBLEM_AT} was running, {TOWN} glowed all night. Now we all go to bed early.",
);
TEXT_BANK(sProbPowerAfter,
    "#a Somebody calmed whatever was loose in {PROBLEM_AT}. The lights have stopped flickering!",
    "#k I can play after dark again! The lights don't flicker anymore.",
);
TEXT_BANK(sProbWreck,
    "#a {PROBLEM_AT} ran aground off the coast years ago. Folks still hear bumps from it at night.",
    "#k I dared my friend to swim out to {PROBLEM_AT}. We both chickened out.",
    "#o I remember the night {PROBLEM_AT} ran aground. Nobody aboard, thank goodness. Just crates, and something that moans.",
);
TEXT_BANK(sProbWreckAfter,
    "#a No more bumps from {PROBLEM_AT} at night. Someone must have gone in there.",
    "#k Did you go inside {PROBLEM_AT}? Was it scary? Was it SO scary?",
);
TEXT_BANK(sProbVolcano,
    "#a {VOLCANO} has been rumbling all week. The plates rattle on my shelf.",
    "#k I felt the ground shake! I bet it's {VOLCANO} snoring.",
    "#o {VOLCANO} rumbles every few years, then settles down again. Nothing to fear.",
);
TEXT_BANK(sProbSea,
    "#a The sea off {TOWN} has been too rough for the boats all week. Everyone's grumpy.",
    "#k The waves were taller than our house! Well, taller than the shed.",
    "#o I've seen the sea this angry before. It always calms down. Give it a week.",
);
TEXT_BANK(sProbPests,
    "#a {LOCALMON} got into the gardens again. Not a carrot left in {TOWN}.",
    "#k {LOCALMON} are eating Mom's flowers. I'm on guard duty. I fell asleep.",
    "#o We put up fences. {LOCALMON} dig under them. We put up deeper fences. They dig deeper.",
);
TEXT_BANK(sProbPet,
    "#a Half of {TOWN} is out looking for a lost POK@MON. It always turns up at suppertime.",
    "#k My POK@MON didn't come home last night. Maybe it's on an adventure, like you!",
    "#o Pets always come home to {TOWN}. Mine once came back after a month, with a {LOCALMON} for a friend.",
);
TEXT_BANK(sProbFestival,
    "#a The {FESTIVAL} is coming and nothing's ready. Same as every year.",
    "#k I'm helping get ready for the {FESTIVAL}! I'm in charge of not touching anything.",
    "#o Every year we say we'll start on the {FESTIVAL} early. Every year we don't.",
);
// By RPROB_*: before, and after (NULL: it does not change).
static const struct RogueTextBank *const sProblemBanks[RPROB_COUNT][2] =
{
    [RPROB_VILLAINS]        = { &sProbVillains, &sProbVillainsAfter },
    [RPROB_VILLAIN_SIGHTED] = { &sProbSighted, &sProbSightedAfter },
    [RPROB_LOST]            = { &sProbLost, &sProbLostAfter },
    [RPROB_LEGEND]          = { &sProbLegend, &sProbLegendAfter },
    [RPROB_BLOCKED]         = { &sProbBlocked, &sProbBlockedAfter },
    [RPROB_POWER]           = { &sProbPower, &sProbPowerAfter },
    [RPROB_WRECK]           = { &sProbWreck, &sProbWreckAfter },
    [RPROB_VOLCANO]         = { &sProbVolcano, NULL },
    [RPROB_ROUGH_SEA]       = { &sProbSea, NULL },
    [RPROB_PESTS]           = { &sProbPests, NULL },
    [RPROB_PET_LOST]        = { &sProbPet, NULL },
    [RPROB_FESTIVAL]        = { &sProbFestival, NULL },
};

// ---------------------------------------------------------------------------
// The region's festival, its famous figure, its founding

TEXT_BANK(sTownFestival,
    "Going to the {FESTIVAL} in {FEST_TOWN}? {^FEST_ACT}. You have to see it.",
    "#k The {FESTIVAL} is my favorite day of the year! Better than my birthday!",
    "#o I've not missed a {FESTIVAL} in seventy years. The first one I don't remember. I was a baby.",
    "#a At the {FESTIVAL}, {FEST_ACT}. It's silly. I cry every year.",
    "#a {FEST_TOWN} throws the {FESTIVAL} every year. They never let the rest of us forget it.",
    "#k Last {FESTIVAL} I stayed up all night! Then I slept the whole next day.",
    "#a I'm saving up for the {FESTIVAL} in {FEST_TOWN}. The food there is the best!",
);
// In the festival's own town.
TEXT_BANK(sTownFestivalHere,
    "We hold the {FESTIVAL} right here in {TOWN}! {^FEST_ACT}. Come back for it!",
    "#a The {FESTIVAL} brings people from all over {REGION}. Then they all want a bed at my place.",
    "#o I've helped with the {FESTIVAL} since I could walk. Now I give orders from a chair.",
    "#k This year I get to carry the banner at the {FESTIVAL}! It's heavier than me!",
);
TEXT_BANK(sTownFigureHere,
    "#k {FIGURE} {FIGURE_DEED}! And they grew up in {TOWN}, just like me!",
    "#a Every kid in {TOWN} wants to grow up like {FIGURE}. Mine wants to grow up like our POK@MON.",
    "Did you know {FIGURE} the {FIGURE_TITLE} came from right here? We mention it a lot.",
    "#o I knew {FIGURE}'s family. Good people. Always off somewhere, that one. Like you.",
);
TEXT_BANK(sTownFigure,
    "#a {FIGURE} {FIGURE_DEED}. Can you imagine? I can't even find my keys.",
    "#k When I grow up I want to be like {FIGURE}! Or a DRAGON TAMER!",
    "#o I met {FIGURE} once, long ago, in {FIGURE_TOWN}. Very polite. Shorter than you'd think.",
    "They say {FIGURE} {FIGURE_DEED}. The people in {FIGURE_TOWN} never stop telling you.",
);
// The oldest town's old ones.
TEXT_BANK(sTownFounding,
    "#o Long ago, {FOUNDING}. That's how {TOWN} began. Ask anyone old enough.",
    "#o My mother used to say that long ago, {FOUNDING}. She said it every night.",
    "#ka {TOWN} is the oldest town in {REGION}. Older than the roads. We like to mention it.",
);

// ---------------------------------------------------------------------------
// Gyms, the League and the CHAMPION (gossip about the town's own leader is
// the leaders domain's: RogueText_LeaderGossip)

TEXT_BANK(sTownNextGym,
    "Heading to {GYM_TOWN}? {LEADER} runs the GYM there. {TYPE} POK@MON, every one.",
    "#k {LEADER} in {GYM_TOWN} is so cool! I want {TYPE} POK@MON just like {LEADER}'s!",
    "#a I tried {LEADER}'s GYM in {GYM_TOWN} once. Lasted about a minute. A good minute, though.",
    "#o {LEADER} of {GYM_TOWN}... I remember when {LEADER} was just a kid with big ideas.",
    "The next GYM is in {GYM_TOWN}. They say {LEADER} hasn't lost in weeks.",
    "#a After {LEADER}'s BADGE? Bring something that stands up to {TYPE} moves. That's all I'll say.",
);
TEXT_BANK(sTownGymWon,
    "#a You beat our {LEADER}? Well! Nobody's done that in a long while.",
    "#k You beat {LEADER}! Can I see the BADGE? Just for a second? Ooh, shiny.",
    "#o {LEADER} was smiling at the shops this morning. Losing to you did {LEADER} good.",
    "#a The whole town's talking about your battle with {LEADER}. Some of it's even true.",
    "#k I watched your battle with {LEADER} through the GYM window! Well, the bit I could see.",
    "#a {LEADER} came out of the GYM and bought everyone a drink of MOOMOO MILK. Said it was for you.",
    "#o Beat {LEADER}, did you? Then {TOWN} owes you a good night's sleep. Go on, rest.",
    "#a I lost to {LEADER} three times last year. You did it in one. I'm not jealous. I'm a bit jealous.",
    "#k Everyone at school is pretending to be you now. I'm {LEADER}. I lose every time.",
    "#a {LEADER} has been training hard since you came through. The GYM's never been so loud.",
);
TEXT_BANK(sTownElite,
    "Past here it's VICTORY ROAD, then the ELITE FOUR. They say {LEADER} is one of them.",
    "#a The ELITE FOUR wait at the top. {LEADER} among them, I hear. {TYPE} POK@MON.",
    "#o {LEADER} of the ELITE FOUR passed through once. Bought a scarf. Very good taste.",
);
TEXT_BANK(sTownChampion,
    "Nobody here has seen the CHAMPION up close. They say the CHAMPION walks the roads like anyone else.",
    "#o The CHAMPION came through {TOWN} once, years ago. Asked about our {GOODS}. Lovely manners.",
    "#a The CHAMPION waits at the very top. Eight BADGES get you in the door.",
);

// ---------------------------------------------------------------------------
// The town up the road, as people here see it (said with ctx.town the next
// town: its trade, its landmark, what it is proud of)

TEXT_BANK(sTownNext,
    "Follow {ROUTE} and you'll come to {TOWN}. It's all {TRADE_FOLK} up there.",
    "#ag {TOWN}'s next up the road. Bring me back some {GOODS} if you go.",
    "Up {ROUTE} is {TOWN}. Ask anyone there about {LANDMARK}!",
    "#o I was born in {TOWN}, up {ROUTE}. Moved here for love. Stayed for the quiet.",
    "#k My cousin lives in {TOWN}! They're all {TRADE_FOLK} there. Except my cousin. He's six.",
    "#a The people in {TOWN} say they have {PRIDE}. You'll hear about it before you hear hello.",
    "#a {ROUTE} takes you to {TOWN}. Lovely place. Slightly too proud of {LANDMARK}.",
);
TEXT_BANK(sTownNextSummit,
    "Past {ROUTE} is {TOWN}. Only the strongest TRAINERS ever go up there.",
    "#o {ROUTE} leads up to {TOWN} and the POK@MON LEAGUE. I got halfway, once. Then it rained.",
);

// ---------------------------------------------------------------------------
// The town's own character, by its theme ('s': lines of the sea, 'v': of the
// volcano), and the summit and hamlets

TEXT_BANK(sThemeMeadow,
    "#a {TOWN} is small, but we know every POK@MON in it by name.",
    "#k Nothing ever happens here. Then you showed up! Is something happening?",
    "#o I've lived here all my life. I went to the next town once. Didn't care for it.",
    "The breeze in {TOWN} smells of clover. You don't get that in the big cities.",
    "#a I planted those flowers along the road myself. Don't step on them!",
    "#k I know every path in {TOWN}! There are four. Five if you count the one through the garden.",
);
TEXT_BANK(sThemeVerdant,
    "The air's so clean here, sick POK@MON come to rest and go home skipping.",
    "#a The log cabins stay warm all winter. I haven't been cold in years.",
    "#k I ride my bike round town before school! It's good for my legs.",
    "#o Green as far as you can see from {TOWN}. I stopped counting the trees. There were too many.",
    "Pink roofs! Somebody insisted, long ago. Nobody remembers who.",
    "#a Folks come to {TOWN} for the air and stay for the quiet. I came for a week. That was years ago.",
);
TEXT_BANK(sThemeHarbor,
    "#as Ships bring sailors, stories and strange POK@MON from all over.",
    "#s Smell that? Salt, fish and adventure!",
    "#ks My dad's a sailor. He says the sea here goes on forever.",
    "#as The docks never sleep. Neither do I, with all that noise.",
    "#a Traders come up the road with crates and stories. Busy, busy!",
    "#o {TOWN} was a sleepy little place once. Then everyone found out about it.",
    "#as Ships from all over tie up at {TOWN}. I came on one and never left.",
);
TEXT_BANK(sThemeIsland,
    "#a {TOWN} is a long way from anywhere. That's how we like it.",
    "#o Those white rocks were here before anyone built a house. They'll be here after.",
    "#s I watch the seabirds from my window all day. It never gets old.",
    "#k At night you can see a million stars here. I counted to two hundred, then fell asleep.",
    "Round trees, red roofs, blue sky. Best view in {REGION}, if you ask me.",
    "#o On {TOWN}, everybody knows everybody, and everybody's business too.",
);
TEXT_BANK(sThemeStone,
    "#a {TOWN} was carved out of the mountain, one block at a time.",
    "#k My school makes us learn every type matchup. I still mix up GHOST and DARK.",
    "Stone streets, iron railings. {TOWN} is a serious place for serious people.",
    "#a Everyone here works hard. Even the POK@MON.",
    "#o The stones in this street are older than anyone in {TOWN}.",
    "#k The stairs in {TOWN} go up forever! I counted them once. Then I sat down.",
);
TEXT_BANK(sThemeCove,
    "#s Blue roofs and sea breezes. People come here to slow down.",
    "#a The inns fill up every summer. TRAINERS from all over!",
    "#as I paint the cove every day. The light changes by the hour.",
    "#s At night the sea here glows blue. It's like magic.",
    "#k I sell my drawings to the visitors. One lady bought three!",
    "#a I came to {TOWN} on holiday ten years ago. I never went home.",
);
TEXT_BANK(sThemeBeach,
    "#s Sand in my shoes, sand in my bed. I wouldn't live anywhere else.",
    "#as The old-timers here can read the weather off the waves.",
    "#s Low tide's the best time to find shells. And sleepy POK@MON.",
    "Blue tile roofs keep the houses of {TOWN} cool on the hottest days.",
    "#k I'm learning to swim! I'm on step one. Getting wet.",
    "#o I've lived in {TOWN} since I was born. I still find sand in my shoes every day.",
);
TEXT_BANK(sThemeAsh,
    "#hv The volcano dusts us with ash most days. It's good for the crops!",
    "#ahv We sweep the roofs every week, or they'd cave in under the ash.",
    "#a The farmers here grow the sweetest crops in gray soil. The ash feeds them.",
    "#k Everything's gray here! Except the BERRIES. They're bright red!",
    "#ohv See that crater outside town? A rock fell from the sky there long ago.",
    "Gray roofs, gray paths, gray grass. The {LOCALMON} round {TOWN} just shake it off.",
    "#a Nothing in {TOWN} stays clean for long. We've stopped minding.",
);
TEXT_BANK(sThemeSpring,
    "#o A soak in the hot springs cures anything. Well, almost anything.",
    "#a The ground in {TOWN} is warm. Even in winter the snow never stays.",
    "#k Old folks come from all over for the springs. I come for the eggs.",
    "#a Smell that? Sulfur. You get used to it in a few years.",
    "#o I came here for one week of soaking. That was thirty years ago.",
    "#k Everyone in {TOWN} smells like eggs! Even the mayor. Especially the mayor.",
);
TEXT_BANK(sThemeTreetop,
    "We live up in the trees here, with bird POK@MON for neighbors.",
    "#a Mind the ladders! Every house here is a climb.",
    "#o The forest keeps {TOWN} cool in summer and dry when it rains.",
    "#k From my treehouse I can see three routes at once!",
    "#a Dropped my keys from the walkway again. A bird POK@MON brought them back. Show-off.",
    "#k In {TOWN} you can walk across the whole town without touching the ground. I've tried!",
);
TEXT_BANK(sThemeSummit,
    "#a Every TRAINER here is heading for the CHAMPION. Most turn back.",
    "The air's thin up here. So is the line of challengers.",
    "#a I made it this far once. Then I went home and trained for a year.",
    "#o Nobody's seen the CHAMPION up here in ages. Out walking the roads, they say.",
    "#a The ELITE FOUR wait through that door. They've been expecting you, I bet.",
);
TEXT_BANK(sThemeHamlet,
    "Not many travelers find their way out here. Welcome!",
    "#a We like it quiet out here. The main road can keep its noise.",
    "#o I've lived at the end of this path for sixty years. You're the third visitor. Welcome.",
    "#k Nobody my age lives out here. Want to be friends? Just for today?",
);
// v1.1 towns2: the raft village out on the sea, as Pacifidlog, and the
// crater town in its ring of white cliffs, as Sootopolis.
TEXT_BANK(sThemeRaft,
    "#s The whole town floats. When a big wave comes, we all sway together.",
    "#as We tie the rafts tighter before every storm. Nobody's drifted off yet.",
    "#ks I can swim from my house to the POK@MON CENTER faster than I can walk it!",
    "#os My grandfather lashed the first logs together here. The sea has kept us since.",
    "#s Mind the logs. They sink a little when you step on them. That's normal.",
    "#as WAILMER pass right under the rafts at dawn. You can feel them go by.",
);
TEXT_BANK(sThemeCrater,
    "#a The cliffs keep the wind out of {TOWN}. The lake never freezes.",
    "#o Long ago a great fire hollowed this place out. Now we live in the hole it left.",
    "#k The stairs in the white rock go up and up! I live at the very top.",
    "Every house in {TOWN} is cut into the cliff. Cool in summer, warm in winter.",
    "#a The lake in the middle is deeper than anyone has dived. We like it that way.",
    "#o From the rim you can see the whole town at once. It's worth the climb.",
);
static const struct RogueTextBank *const sThemeBanks[RTHEME_COUNT + 1] =
{
    [RTHEME_MEADOW] = &sThemeMeadow, [RTHEME_VERDANT] = &sThemeVerdant, [RTHEME_HARBOR] = &sThemeHarbor,
    [RTHEME_ISLAND] = &sThemeIsland, [RTHEME_STONE] = &sThemeStone, [RTHEME_COVE] = &sThemeCove,
    [RTHEME_BEACH] = &sThemeBeach, [RTHEME_ASH] = &sThemeAsh, [RTHEME_SPRING] = &sThemeSpring,
    [RTHEME_TREETOP] = &sThemeTreetop, [RTHEME_RAFT] = &sThemeRaft, [RTHEME_CRATER] = &sThemeCrater,
    [RTHEME_COUNT] = &sThemeSummit,
};

// ---------------------------------------------------------------------------
// Everyday life, tips, the rival passing through, the player's lead

TEXT_BANK(sTownLife,
    "#k My POK@MON and I are the same age! We're both very grown-up.",
    "#a I'm late for work. I've been late for work for eleven years. They've stopped noticing.",
    "#o I battled for years. My POK@MON retired first. Then I did. Then we argued about it.",
    "#k I'm going on a journey like you when I'm older! I've packed already!",
    "#a My POK@MON sleeps on my pillow. I sleep on the floor. We're both happy.",
    "#a Lovely day, isn't it? Even if it isn't, I've decided it is.",
    "#o Young people walk so fast. Where's the fire? ...Oh, a journey. Off you go, then.",
    "#k I'm hiding from my sister. If she asks, you haven't seen me.",
    "#a I keep a POTION in every pocket. My POK@MON is a little clumsy.",
    "#ka I tried to catch a POK@MON with my hat. The POK@MON kept the hat.",
    "#o Have you eaten? You look thin. TRAINERS never eat enough.",
    "#a I was born in {TOWN} and I'll grow old here too. Sounds lovely, honestly.",
    "#k Have you been to {NEIGHBOR}? Is it nice? Is it nicer than here? Don't answer that.",
    "#o I've lived in {TOWN} my whole life and I still get lost on the way to the shops.",
    "#a Everyone in {TOWN} knows my POK@MON's name. Not many know mine. That's fine.",
);
TEXT_BANK(sTownTip,
    "#a My mom always said, heal up at the POK@MON CENTER before you leave town. She was right about most things.",
    "#o The first wild POK@MON you meet in a new place is the one to catch. We all learned that the hard way.",
    "#a A POK@MON that faints out there is gone for good. Keep yours safe, TRAINER.",
    "#k I hopped down a ledge once and couldn't climb back up. I had to walk all the way round!",
    "#a A GYM BADGE lets your POK@MON clear the way. Small trees, cracked rocks, boulders, even water.",
    "#o BERRIES grow back on the soft soil by the road. Help yourself. The trees don't mind.",
    "#a Fishing counts as meeting a POK@MON too, they say. An OLD ROD is worth having.",
    "#a If the first POK@MON you meet is one you already have, the place isn't used up. Lucky, huh?",
    "#k When my POK@MON is tired, I let it rest at the POK@MON CENTER. The nurse gives it a sticker!",
    "If your POK@MON look worn out, there's no shame in heading back to {TOWN}'s POK@MON CENTER. Everyone does.",
);
TEXT_BANK(sTownRivalKid,
    "A kid with {RIVAL_MON} ran through here. PROF. BIRCH's kid, I think.|Said they'd beat you to the first GYM!",
    "#k {RIVAL} was here! They showed me {RIVAL_MON}! It sneezed on me. It was great.",
    "#a PROF. BIRCH's kid went by in a hurry. Something about not losing to you.",
    "#o {RIVAL} stopped to help me with my bags, then ran off shouting your name. Nice kid.",
);
TEXT_BANK(sTownLead,
    "#k Is that {LEAD}? Can I pet it? I'll be gentle!",
    "#a That's a fine {LEAD} you've got. Kind eyes. You can tell a lot from the eyes.",
    "#o {LEAD}... I had one just like it, long ago. Look after it.",
    "#a Your {LEAD} keeps looking at my lunch. It's welcome to half.",
    "#k My friend says {LEAD} is the coolest POK@MON ever. I say it depends who's holding the BALL.",
);

// ---------------------------------------------------------------------------
// People out on a route: the road itself, by its kind, and where they are
// going ({TOWN} the town nearby, {NEXT} the next one on)

TEXT_BANK(sRouteWoods,
    "#a I come out to {ROUTE} to listen to the trees. And to get away from my family.",
    "#k I got lost in the trees on {ROUTE} once. I followed the birds out!",
    "The bug POK@MON on {ROUTE} sing all evening. It's loud, but I like it.",
    "#a I come to {ROUTE} to pick mushrooms. I've picked three. Two were rocks.",
    "#o These trees on {ROUTE} were saplings when I moved here. Now we're both a bit bent.",
    "#k I built a fort in the woods on {ROUTE}! It's a secret. You can visit, though.",
    "The light comes through the leaves on {ROUTE} all green. Like walking underwater.",
);
TEXT_BANK(sRouteLake,
    "#a The ponds on {ROUTE} are so still you can see two of everything.",
    "#k I skipped a stone four times on {ROUTE}! It hit a LOTAD. Sorry, LOTAD.",
    "#o I've fished these ponds since I was your age. They've never once given me a fish.",
    "#a I paint the lily pads on {ROUTE}. The LOTAD keep moving. I paint faster.",
    "#k There's a POK@MON in the pond on {ROUTE} that winks at me. I wink back.",
    "Mind the mud by the water on {ROUTE}. It's taken three of my boots.",
    "#a On a still morning the ponds on {ROUTE} are like mirrors. Then a MAGIKARP jumps.",
);
TEXT_BANK(sRouteCoast,
    "#a Nothing like a walk on the beach on {ROUTE}. Sand in my shoes all week, though.",
    "#k I'm looking for shells on {ROUTE}. I've found two and a crab. The crab found me.",
    "#o The tide comes up {ROUTE} each morning. I come down to meet it.",
    "#a I come down to {ROUTE} to watch the boats. I don't own a boat. I just like them.",
    "The sea wind on {ROUTE} takes your hat if you let it. Hold on tight!",
    "#k I buried my brother in the sand on {ROUTE}! Just up to his neck. He's fine.",
    "#o I've walked this beach each day since I retired. The sea never gets boring.",
);
TEXT_BANK(sRouteRiver,
    "#a I walk along the river on {ROUTE} every day. I like how it sings over the stones.",
    "#k If you drop a leaf in the river on {ROUTE}, it races you. It always wins.",
    "#a The river on {ROUTE} is cold all year. My POK@MON swim in it every day, cold or not.",
    "#o This river had no bridge in the old days. We waded across with our boots on our heads.",
    "#k I'm counting the fish in the river on {ROUTE}. They won't hold still!",
    "The bridge on {ROUTE} creaks. It's always creaked, and it's never fallen down.",
);
TEXT_BANK(sRouteBridge,
    "#a I cross the bridge on {ROUTE} every morning. The wind up there wakes you better than any alarm.",
    "#k From the top of the bridge on {ROUTE} you can see the fish! Well, their shadows.",
    "#o They built the road on {ROUTE} up high so boats could pass under. Nobody's brought a boat yet.",
    "#a The trainers on the bridge on {ROUTE} stand right in the way. Up there, there's no going round them.",
    "#k I counted the pillars under the bridge on {ROUTE}. I lost count. They all look the same!",
    "The fishing's better down by the water on {ROUTE} than up on the bridge. The fish can't hear you there.",
    "#o Before the bridge, we rowed across the water on {ROUTE}. My arms still remember it.",
);
TEXT_BANK(sRouteHills,
    "#a All these ledges on {ROUTE}! You can hop down, but never back up. Plan ahead.",
    "#o I climb {ROUTE} each day. My doctor says it's good for me. My doctor doesn't climb it.",
    "#k From the top of {ROUTE} you can see for miles!",
    "#a I fly kites on the hills of {ROUTE}. The wind is good. The kites disagree.",
    "#k I rolled all the way down a hill on {ROUTE}! I'm still dizzy. It was great.",
    "Every ledge on {ROUTE} looks small from the bottom. They're bigger up close.",
);
TEXT_BANK(sRouteField,
    "#a The grass on {ROUTE} comes up past my knees. Who knows what's in there!",
    "#k I lay down in the grass on {ROUTE} and a POK@MON lay down next to me!",
    "#o I used to make hay on {ROUTE}. Now I just lie in it.",
    "#a The wind makes waves in the grass on {ROUTE}. Like a sea you can walk on.",
    "#k I'm looking for a four-leaf clover on {ROUTE}. I've found a lot of three-leaf ones.",
    "#a I lost a hat in the grass on {ROUTE} last spring. Something in there is wearing it now.",
    "The flowers on {ROUTE} come up a different color every year. I wonder what's next.",
);
// v1.1 land: the flower route (Route 117's beds) and the long grass (Routes
// 119 and 120).
TEXT_BANK(sRouteFlowers,
    "#a Somebody plants the beds on {ROUTE} every spring. I've never once seen who.",
    "#k The flowers on {ROUTE} smell so sweet! The bug POK@MON think so too.",
    "#o I come to {ROUTE} to paint the flower beds. My paintings never come out this bright.",
    "#a Don't pick the flowers on {ROUTE}. The fence is there for a reason, you know.",
    "#k I counted every flower in one bed on {ROUTE}! Then the wind blew and I lost count.",
    "The bees on {ROUTE} are very busy. And so are the POK@MON that follow them.",
);
TEXT_BANK(sRouteLongGrass,
    "#a The grass on {ROUTE} grows over your head. Trainers hide in it and jump out!",
    "#k I got lost in the long grass on {ROUTE}. I just walked until I hit a tree.",
    "#o Walk the gaps on {ROUTE} if you're tired of battles. The grass lets you pass there.",
    "#a You can't see a thing in the grass on {ROUTE}. I keep hearing rustling.",
    "#k Something tall moved in the grass on {ROUTE}! It was a tree. I think.",
    "The grass on {ROUTE} is taller than I am. The POK@MON in it must be huge.",
);
TEXT_BANK(sRouteDesert,
    "#a The wind on {ROUTE} gets into everything. Even my pockets are full of grit.",
    "#a Bring water if you're crossing {ROUTE}, and a hat for the sun.",
    "#o The sand on {ROUTE} sings when the wind is right. Listen.",
    "#k I found a shiny stone in the sand on {ROUTE}! It's a bottle cap. Still shiny.",
    "Watch your step on {ROUTE}. The sand moves when you're not looking.",
);
TEXT_BANK(sRouteMountain,
    "#a {ROUTE} is where TRAINERS find out if they're ready. Most find out they're not.",
    "#o I've watched TRAINERS climb {ROUTE} for years. The ones who make it all look tired.",
    "#a The air up here on {ROUTE} is thin. So are the excuses. Keep climbing.",
    "Every TRAINER who reaches the top of {ROUTE} leaves a stone on the pile. It's a big pile.",
);
TEXT_BANK(sRouteSea,
    "#a I swam out here and now I'm too tired to swim back. Nice view, though.",
    "#k The water out here is so clear! I can see my toes. And something else's toes.",
    "#a I'm resting on this rock before I swim back. It's a very comfortable rock.",
    "#o I've sailed these waters most of my life. I still learn something every trip.",
);
// v1.1: on a route known for its weather (RogueGen_KnownWeather), people
// talk of that.
TEXT_BANK(sRouteRain,
    "#a It rains on {ROUTE} most days. The grass loves it. My boots don't.",
    "#k I've got my raincoat on! It's always raining on {ROUTE}. Always!",
    "#o It's rained on {ROUTE} as long as I can remember. The water POK@MON like it.",
    "Folks in {TOWN} call this stretch {NICK}. You can guess why.",
    "#a I never dry my umbrella anymore. What's the point, on {ROUTE}?",
    "#k When it rains on {ROUTE} the frogs sing! So they sing every day.",
);
TEXT_BANK(sRouteStorm,
    "#a Storms roll over {ROUTE} all the time. Count after the flash. That's how far it is.",
    "#k The thunder on {ROUTE} makes my POK@MON hide behind me. I hide behind it.",
    "#o They call it {NICK} in {TOWN}. Mind the lightning, and the puddles.",
    "#a The ELECTRIC POK@MON love the storms on {ROUTE}. They come out to watch.",
    "#k Rain AND thunder! {ROUTE} is the best place for a scary story.",
);
TEXT_BANK(sRouteFog,
    "#a The mist on {ROUTE} never lifts. Keep to the path, and keep talking.",
    "#k I can't see my own feet in this fog! Are they still there?",
    "#o Folks in {TOWN} call it {NICK}. The mist has been here longer than the town.",
    "#a Something big walked past me in the mist on {ROUTE}. I think it was a POK@MON.",
    "#k I lost my friend in the fog on {ROUTE}! Oh, there you are. Wait, who are you?",
);
static const struct RogueTextBank *const sRouteBanks[RROUTE_COUNT] =
{
    [RROUTE_FOREST] = &sRouteWoods, [RROUTE_WOODS] = &sRouteWoods, [RROUTE_LAKE] = &sRouteLake,
    [RROUTE_MOUNTAIN] = &sRouteMountain, [RROUTE_COAST] = &sRouteCoast, [RROUTE_RIVER] = &sRouteRiver,
    [RROUTE_HILLS] = &sRouteHills, [RROUTE_FIELD] = &sRouteField, [RROUTE_DESERT] = &sRouteDesert, [RROUTE_SEA] = &sRouteSea,
    [RROUTE_FLOWERS] = &sRouteFlowers, [RROUTE_LONGGRASS] = &sRouteLongGrass, [RROUTE_BRIDGE] = &sRouteBridge,
};
TEXT_BANK(sRouteTravel,
    "#ag I'm taking a basket of {GOODS} from {TOWN} to sell. My arms are already tired.",
    "#a I walk from {TOWN} to {NEXT} and back every day. I know every pebble by name.",
    "#k I'm walking to {NEXT} all by myself! Well, my POK@MON's with me. So not by myself.",
    "#o I've walked {ROUTE} every day of my life. It's gotten longer, I'm sure of it.",
    "#a Off to the {FESTIVAL} in {FEST_TOWN}! I'm walking there now. Only a few days early.",
    "#a The {TRADE_FOLK} of {TOWN} send me out on errands. I like the walk better than the work.",
);

// ---------------------------------------------------------------------------
// Homes: families ({FAMILY} with ctx.house), and the people who live there

// The plate beside an ordinary home's door. Signs are official: no jokes,
// just the family and a word of welcome.
TEXT_BANK(sHouseSign,
    "THE {FAMILY} HOUSE",
    "THE {FAMILY} HOUSE\nPlease knock.",
    "THE {FAMILY} FAMILY\nVisitors welcome.",
    "THE {FAMILY} HOUSE\nWipe your feet, please.",
    "THE {FAMILY} HOUSE\nHome of {TRADE_FOLK}.",
    "THE {FAMILY} HOUSE\nMind the POK@MON.",
    "THE {FAMILY} FAMILY\nPlease close the gate.",
    "THE {FAMILY} HOUSE",
);
// The gift house's plate: the family, and a welcome for TRAINERS (the gift
// inside is a surprise).
TEXT_BANK(sGiftHouseSign,
    "THE {FAMILY} HOUSE\nTRAINERS welcome!",
    "THE {FAMILY} FAMILY\nTravelers, do knock!",
    "THE {FAMILY} HOUSE\nAll who pass are welcome.",
    "THE {FAMILY} HOUSE\nTRAINERS, come in and rest.",
    "THE {FAMILY} FAMILY\nThe door is always open.",
);

// A glass cabinet at home (RogueText_Cabinet).
TEXT_BANK(sHomeCabinet,
    "A glass cabinet. The {FAMILY} family's best plates are inside. They have never been used.",
    "A glass cabinet full of ribbons from the {FESTIVAL}. Every single one says SECOND PLACE.",
    "A glass cabinet. A POK@ BALL sits inside, polished till it shines. A card says: OUR FIRST.",
    "A glass cabinet of old tools of the {TRADE_FOLK}. They look heavier than the new ones.",
    "A glass cabinet. There's a photo of {LANDMARK} on the top shelf, and a smaller one of the family in front of it.",
    "A glass cabinet. A letter from {NEIGHBOR} is framed inside. It's a complaint.",
    "A glass cabinet of tiny POK@MON figures. There's a gap where one is missing.",
    "A glass cabinet. Wedding photos, a baby's first shoe, and a very old POK@ BALL.",
    "A glass cabinet. A sign inside says PLEASE DO NOT TAP THE GLASS. Somebody has tapped the glass.",
    "A glass cabinet full of trophies. The biggest one is for {TRADE}. The smallest is for being on time.",
    "#s A glass cabinet full of seashells, each with a date written on it in tiny letters.",
    "#v A glass cabinet. A lump of black glass from {VOLCANO} rests on a velvet cushion.",
    "#g A glass cabinet. The family's finest {GOODS} sit behind the glass, too good to use.",
    "A glass cabinet. A child's drawing of {LOCALMON} has been framed and given the middle shelf.",
    "A glass cabinet with a map of {REGION} inside. {TOWN} is drawn much bigger than it should be.",
    "A glass cabinet. Inside are a cracked cup and a note: DO NOT THROW AWAY. IT WAS MOM'S.",
    "A glass cabinet of POK@MON cards. They're sorted by how much the {FAMILY} children like them.",
    "A glass cabinet. A medal from the {FESTIVAL} sits in a velvet box. The ribbon is a little faded.",
);

// Visitors resting in a town's POKeMON CENTER (RogueText_CenterFolk), as
// Hoenn's Centers have a TRAINER or two on the benches.
TEXT_BANK(sCenterFolk,
    "#ao My POK@MON are with the nurse. I'm not worried. I'm a little worried.",
    "#k I come here every day to hear the healing machine go ding. Ding!",
    "#ao I walked here from {NEIGHBOR} with a tired team. The nurse didn't ask a thing. She just smiled.",
    "#o I'm not hurt. I just like the chairs. Best chairs in {TOWN}.",
    "#ao They say every POK@MON CENTER looks the same inside. I've been to four. They're right.",
    "I'm waiting for the PC. Someone has been storing BERRIES on it all morning.",
    "#ao I battled my way down {ROUTE}. My team's in the back now, being fussed over.",
    "#k When I'm a TRAINER, I'll come here every day. Even if nobody's hurt.",
    "#ao The nurse here knows every POK@MON in {TOWN} by name. Most of the TRAINERS, too.",
    "#o I've been coming to this CENTER since before it had a roof. It rained a lot.",
    "#ao Everyone who battles {LEADER} in {GYM_TOWN} ends up in a CENTER. Ask me how I know.",
    "#ao I'm writing to my family in {NEIGHBOR}. Dear everyone, I lost again. Love, me.",
    "#k My mom works with the {TRADE_FOLK}. I wait here after school. The nurse gives me stickers.",
    "#ao Rested team, full BAG, and the road to {NEXT}. Today's a good day.",
    "#ao I heal here, train on {ROUTE} and come back. That's nine times today.",
    "#ao A {LOCALMON} chased me all the way here. It's waiting outside. I'll wait a bit longer.",
    "#ao That healing tune! I hum it all day now. My partner has asked me to stop.",
    "#ao I'm not a TRAINER. I just come in when it's cold. Nobody minds.",
    "#ao Everyone in {TOWN} has a story about this CENTER. Mine has a very lost {LOCALMON} in it.",
    "#b {^BADGES} BADGES? Then you've seen more CENTERS than I have. How does this one rate?",
    "#o The secret to a long life is a good sleep and a POK@MON CENTER nearby. I have both.",
    "#ao I came in to heal and got talking. That was this morning.",
    "#k The nurse let me hold a POTION once. Once! Best day of my life.",
    "#ao The {FESTIVAL} always fills this place. Too much dancing, not enough stretching.",
    "#ao My team's ready for {NEXT}. I'm the one who needs a rest.",
    "#o I keep a seat by the door. You see everyone who comes through {TOWN} from here.",
    "#k I'm going to be a nurse. Or a TRAINER. Or the machine that goes ding.",
    "#ao I lost my way on {ROUTE} and walked in circles. My POK@MON knew the way. It let me walk.",
    "#k My big brother is getting his POK@MON healed. He says I can't come in the back. I can see it from here.",
    "#k I counted the tiles on the floor. Two hundred and six! Waiting is boring.",
    "#k The nurse's CHANSEY waved at me! Well, I think it waved.",
    "#k I want a POK@MON so badly. I practice by talking to the plant.",
    "#k I'm not allowed on {ROUTE} by myself. I'm allowed in here. It's almost the same.",
    "#k Is that a real TRAINER BAG? Can I look inside? Just a peek!",
    "#o I used to be a nurse here, you know. Now I just come and tell her how to do it.",
    "#o My POK@MON is older than this building. It still likes the healing tune.",
    "#o Before CENTERS, we healed POK@MON with BERRIES and patience. This is much faster.",
    "#o I've watched TRAINERS come through {TOWN} for ages. You've got a good, steady walk.",
    "#ao I always sit here. The nurse calls it my chair. It isn't. It is a bit.",
    "#ao My POK@MON fainted on {ROUTE}. It's fine now. I'm the one who needs a lie down.",
    "#ao I swap stories with every TRAINER who comes in. Yours is next. No rush.",
    "#ao The PC here holds every POK@MON I've ever caught. Some of them must wonder where I went.",
    "#ao I came to {TOWN} for the {TRADE_FOLK}. I stayed for the POK@MON CENTER. Such comfy chairs!",
    "#ao My partner's POK@MON and mine are being healed together. They're best friends. We're getting there.",
    "#ao A TRAINER from {NEIGHBOR} told me {TOWN}'s CENTER is the best. I'm checking. So far, yes.",
    "#ao I'm meeting a friend here. They're late. Their POK@MON is probably the reason.",
    "#ao Healed, rested, and still nervous about {LEADER}. Is that normal? Tell me it's normal.",
    "#ao Every TRAINER I've met in here was kind. Something about the place.",
    "#k I drew a picture of the nurse. She put it on the wall behind the counter!",
    "#k My POK@MON is getting a check-up. It's very brave. I'm the one holding on to the chair.",
    "#o I knew the nurse here when she was a girl. She was bossy then, too. Good at it now.",
);
// The same in the LEAGUE's lobby.
TEXT_BANK(sCenterSummit,
    "#a Last rest before the ELITE FOUR. My hands won't stop shaking. My POK@MON are fine.",
    "#ao I made it this far once. I come back every year to sit here and remember it.",
    "#a They say the CHAMPION walks the roads like anyone else. I keep looking at the door.",
    "#a Eight BADGES, a full BAG, and a team I trust. Now I just need to stand up.",
);

// One person at home, by voice.
TEXT_BANK(sHomeKid,
    "#k Mom says I can go on a journey when I'm older. Older than what, she won't say.",
    "#k I'm drawing a map of {TOWN}! Our house is the biggest. It isn't, but it's my map.",
    "#k Shh! I'm hiding from bedtime.",
    "#k When I grow up I'm going to be one of the {TRADE_FOLK}. Or a CHAMPION. Maybe both!",
    "#k My POK@MON sleeps on my bed. It snores like you wouldn't believe.",
    "#k Want to see my rock collection? This one's a rock. This one's also a rock.",
    "#k I'm not allowed past {ROUTE} on my own yet. You're so lucky.",
    "#k I wrote a letter to {LEAD}. Can you give it to it? It's all drawings.",
    "#k I want to go to {NEIGHBOR} someday. Everyone says it's boring. I want to see how boring.",
    "#k {TOWN} used to be tiny, they say. It's still tiny. I walked all of it!",
);
TEXT_BANK(sHomeAdult,
    "#a Oh! A visitor! Make yourself at home. Mind the POK@MON hair.",
    "#a Long day with the other {TRADE_FOLK}. Don't mind the mess.",
    "#a Staying for dinner? No? Well, good luck out there. Come back safe.",
    "#a The kids want to be TRAINERS. I want them to be {TRADE_FOLK}. We'll see who wins.",
    "#a I'm baking. Come back later and maybe there'll be some left.",
    "#a My POK@MON and I like to watch the rain from this window.",
    "#a I keep telling the kids not to bother TRAINERS passing through {TOWN}. Then I bother them myself. Hello!",
    "#a I worked in {NEIGHBOR} for a year once. Came home to {TOWN} and never left again.",
    "#a We moved here from {NEIGHBOR}. Best thing we ever did. We still visit, though.",
    "#a The {TRADE_FOLK} start early in {TOWN}. I'm only home for a bite to eat. Then back out.",
);
TEXT_BANK(sHomeOld,
    "#o I once beat a GYM LEADER, you know. Nobody believes me. I don't blame them.",
    "#o {TOWN} was three houses and a well once. Now look at it. Crowded!",
    "#o Sit, sit. Have a BERRY. You're too thin. All TRAINERS are too thin.",
    "#o I've been to every {FESTIVAL} since I was a baby. I still cry at the end.",
    "#o My knees can tell when it'll rain. They say tomorrow. Take a coat.",
    "#o This house has stood since {OLDTOWN} was a hamlet. We've fixed the roof a few times.",
    "#o I used to be one of the {TRADE_FOLK} myself. Now I give advice. Nobody takes it.",
    "#o So you're a TRAINER. You've got the look, I can tell. You'll go far.",
    "#o When {LANDMARK} was new, everybody hated it. Now everybody loves it. Funny how that goes.",
);
// What the family says of itself (one person of a home, now and then).
TEXT_BANK(sHomeFamily,
    "The {FAMILY} family has lived in this house for four generations. Same creaky stairs.",
    "#a There's been a {FAMILY} in {TOWN} as long as there's been a {TOWN}.",
    "#o I'm the oldest {FAMILY} still going. Still going strong, mind you.",
    "#k Everyone in this house is called {FAMILY}! It gets confusing at dinner.",
    "#a The {FAMILY} family has always been {TRADE_FOLK}. Before that, more {TRADE_FOLK}.",
    "Welcome to the {FAMILY} house! Shoes off, please. The rug's older than I am.",
);
// Two people at home, talking to each other: the first line is the first
// resident's, the second the other's. Each side has its own tag.
static const char *const sHomeDuets[][2] =
{
    { "#a My partner says the roof will last another year.", "#ao It won't." },
    { "#k Can I go on a journey too? Please? Please?", "#ao When you can keep your room tidy, you can keep a BAG tidy. Then we'll talk." },
    { "#o Back then we walked to the next town uphill. Both ways.", "#ka That's not even possible!" },
    { "#ao We've been {TRADE_FOLK} for generations in this family.", "#k I'm going to be a TRAINER. Sorry." },
    { "#k Is that a real TRAINER? A real one!", "#ao Don't shout. ...Hello! Sorry. Ignore us." },
    { "#a Did you lock the door?", "#ao We don't have a lock. We've never had a lock." },
    { "#ao The {FESTIVAL} is coming! We should start planning.", "#ao We said that last year." },
    { "#k I want a POK@MON just like {LEAD}!", "#ao Look after the potted plant for a month first. Then we'll see." },
    { "#ao Supper's nearly ready. Will you stay?", "#k Say yes. There's always too much." },
    { "#o The {TRADE_FOLK} worked harder in the old days.", "#ka You say that every day." },
    { "#k Our POK@MON learned a new trick! It sits!", "#ao It was already sitting. It's always sitting." },
    { "#a Shh! The baby's sleeping.", "#k That's not the baby. That's the POK@MON." },
    { "#ao Have you heard? {NEIGHBOR} says they have {PRIDE}.", "#ao They what? Oh, they did NOT." },
    { "#k I saw {LOCALMON} on the way home!", "#ao That's nice, dear. Now go and wash your hands." },
    { "#o Long ago, {FOUNDING}.", "#ka You tell everyone that story." },
    { "#ao Is that the TRAINER everyone's talking about?", "#ka Don't stare. ...Okay, it might be." },
    { "#ao The {TRADE_FOLK} in {NEIGHBOR} are working on a holiday again.", "#ao Let them. We'll still be better at it." },
    { "#k When can I go out on {ROUTE} by myself?", "#ao When {LOCALMON} stop being bigger than you." },
    { "#a Did anyone feed the POK@MON?", "#ka It says nobody did. It always says nobody did." },
    { "#o I remember when {TOWN} had no POK@MON CENTER at all.", "#ka We know. You've told us. Every week." },
    { "#ao Shall we go to the {FESTIVAL} this year?", "#ka We go every year!" },
    { "#k Can we keep {LOCALMON} as a pet?", "#ao One lives in the garden already. It just doesn't know it's ours." },
    { "#a A visitor! Quick, hide the laundry!", "#ka Too late. They've seen the laundry." },
    { "#ao That TRAINER has a good look about them.", "#ao Their POK@MON have a better one." },
};

// The lore elder of the cast (RogueCast.elder), at home in the oldest town:
// slow, kind, in the past tense, and a little muddled about names.
TEXT_BANK(sElderIntro,
    "Come in, dear. I'm {ELDER}. I've lived in {TOWN} longer than the roads have.|Long ago, {FOUNDING}. That's how it all began.",
);
TEXT_BANK(sElderLines,
    "Once, we made our own lanterns for the {FESTIVAL}. Now they buy them from {NEIGHBOR}. Hmph.",
    "{FIGURE}? I met {FIGURE} once, at the {FESTIVAL}. Knee-high, and already set on leaving {FIGURE_TOWN}. Like you, dear.",
    "Long ago, {FOUNDING}. My mother told me that, and her mother told her. Now I've told you.",
    "I walked to {NEIGHBOR} and back every week as a child. My legs still remember it, dear.",
    "#b {BADGES} BADGES already? My, my. You remind me of myself at your age.",
    "Young one, the old tales are true. Most of them. The rest are better than true.",
    "This house is older than me, and I'm older than everything else in {TOWN}. Sit, sit.",
);

// A POKeMON at home ({NAME} its species, {FAMILY} its family's).
TEXT_BANK(sPetHome,
    "The {NAME} is curled up by the table. It looks cozy.",
    "{NAME}: ...!|It wants you to play with it.",
    "The {FAMILY} family's {NAME} sniffs your shoes and decides you're all right.",
    "The {NAME} is guarding the kitchen. Nobody gets a snack without asking it first.",
    "The {NAME} is asleep on somebody's homework.",
    "The {NAME} brings you a sock. It seems very proud of it.",
    "The {NAME} yawns at you. It's had a busy day of sleeping.",
    "The {NAME} watches the door. It's waiting for someone. Not you, but you'll do.",
    "The {NAME} is curled up in the laundry basket.",
    "The {NAME} is staring at the TV. It seems to like the news.",
    "The {NAME} has knocked something off a shelf. It looks at you as if you did it.",
    "The {NAME} is snoring in a patch of sunlight.",
    "The {NAME} is sitting in the best chair. Nobody else is allowed in the best chair.",
    "The {NAME} has taken one of your shoelaces. It's not giving it back.",
    "The {NAME} is pretending to be asleep. One eye is open.",
    "The {NAME} is chewing a slipper. The slipper has seen better days.",
    "The {NAME} pads over, sits on your foot, and stays there.",
    "The {NAME} is watching the stove. It hopes dinner is soon.",
    "The {NAME} has made a nest of the {FAMILY} family's coats.",
    "The {NAME} sniffs your BAG very seriously, then sneezes.",
    "The {NAME} is staring out of the window, waiting for the {FAMILY} children to come home.",
    "The {NAME} rolls onto its back. It would like its tummy rubbed. Now, please.",
);
// One out for a walk.
TEXT_BANK(sPetOut,
    "The {NAME} is out for a walk. It seems very friendly!",
    "{NAME}: ...!|It wants to play.",
    "The {NAME} is sniffing around happily. Someone takes good care of it.",
    "The {NAME} trots past with something in its mouth. It's not telling what.",
    "The {NAME} is sunning itself. It has no plans to move today.",
    "The {NAME} follows you a few steps, then remembers it lives here.",
    "The {NAME} is chasing its own tail. It's winning.",
    "The {NAME} is watching the clouds. It looks very thoughtful.",
    "The {NAME} sniffs your hand, then takes a long sniff at your BAG.",
    "The {NAME} is guarding a puddle very seriously.",
    "The {NAME} has found a stick. It's clearly the best stick.",
    "The {NAME} rolls over in the dust and looks very pleased with itself.",
    "The {NAME} is waiting by a door. Its owner will be out any minute. Any minute now.",
    "The {NAME} gives you a long look, then decides you're all right.",
    "The {NAME} is counting the people going past. It has lost count.",
    "The {NAME} has found a sunny step and is not sharing it.",
    "The {NAME} is trying to catch a falling leaf. It keeps missing.",
    "The {NAME} sneezes, looks around to see who heard, and walks off with great dignity.",
    "The {NAME} is following a butterfly very slowly, so the butterfly won't notice.",
    "The {NAME} yawns so wide it nearly falls over.",
    "The {NAME} is sitting in a flower box. The flowers are not happy about it.",
    "The {NAME} watches you go by, then goes back to watching nothing in particular.",
);

// ---------------------------------------------------------------------------
// Gifts: the gift house (one gift per town), and a townsperson's

TEXT_BANK(sGiftOffer,
    "Oh, a TRAINER on a journey! My kid set out from {TOWN} just like you.|Here, take this. It should help you on your way.",
    "#ao We {TRADE_FOLK} don't have much use for this. A TRAINER does.|Take it, please.",
    "#o Every year I put something by for a TRAINER passing through {TOWN}. This year it's you.|Here.",
    "The {FAMILY} family always helps a traveler. That's how we were raised.|This is for you.",
    "#o I was a TRAINER once, long ago, before I settled in {TOWN}. I kept this for someone like you.|Go on, take it.",
    "#aoH My POK@MON outgrew this. Yours won't, I bet.|Here!",
    "#s The tide washes up the strangest things on the beaches of {TOWN}. Look what I found!|It's yours, TRAINER.",
    "#hv The ash buries everything here. I dug this up in the yard!|Take it with you.",
    "A traveler left this with us after the {FESTIVAL}. Never came back for it.|Better a TRAINER uses it!",
    "#ao I bought too much at the shops in {NEIGHBOR} again.|Here, you take this!",
    "#ao My partner found this on the road to {NEXT}. We've no use for it.|You take it!",
    "#k Mom said I could give the TRAINER present this year! Here! I picked it myself!",
    "#ao Your POK@MON have come a long way. Ours had a road like yours once.|This helped us. Maybe it'll help you.",
    "#aoE We grew these ourselves. Well, the garden did. We watched.|Take some for the road!",
    "#E There's always extra after the {FESTIVAL}. We've been eating them for weeks.|Please, take some. Please.",
    "#aoD My old teacher gave me this disc and said, find someone who needs it.|I think that's you.",
    "#oD I never did learn how to use one of these. Machines, honestly.|You'll know what to do with it.",
    "#aoB We sell a few of these to TRAINERS passing through. You look like you need them more than you need a bill.|No charge!",
    "#aoS I found this stone up on {ROUTE} years ago. It makes certain POK@MON very excited.|Ours just yawned. Maybe yours won't.",
    "#T Look at this! It's worth a lot at the shops. I'd rather it bought something for a TRAINER.|Go on, take it.",
    "#aoC My kid used to sneak these to our POK@MON. It got very strong. And very round.|Take them, before the kid finds them.",
    "#o Sit down a moment. You've walked a long way to {TOWN}.|There. Now, take this with you. No arguing.",
    "We had a bet: would a TRAINER knock today? I won. So you get the prize.|Here you are!",
    "#ao I lost a battle on {ROUTE} once and a stranger gave me one of these. I've waited years to pass it on.|Your turn.",
    "#k I've been watching the road all morning for a TRAINER! And you came!|This is for you. I picked it out myself!",
    "#o Our {TRADE_FOLK} have always looked after travelers. My mother did, and her mother.|Take this, from all of us.",
    "Oh! You're the TRAINER they're talking about in {NEIGHBOR}! Well, they're wrong about you. You're taller.|Here, take this.",
    "#ao You remind me of my sister when she left {TOWN}. She came back a GYM LEADER. Well, almost.|Take this. For luck.",
);
TEXT_BANK(sGiftAfter,
    "Look after every one of them. There are no second chances out there.",
    "Come back and tell us how it went! We love a good story in {TOWN}.",
    "Use it well. And visit again. The {FAMILY} door is always open.",
    "Go on, then. {NEXT} won't come to you.",
    "I hope it helps. The {FAMILY} family likes to be useful.",
    "#o Mind how you go. The road to {NEXT} has always been rough.",
    "Safe travels! Say hello to {NEIGHBOR} for us. Or don't. We're not speaking this week.",
    "#ao The kids will want to hear all about you. Come back to {TOWN} someday!",
    "#o Here, have a BERRY too. Oh, you're full? Next time, then.",
    "Every TRAINER we've helped has written to us. Well, one did. Once. From {NEIGHBOR}.",
    "#ao Don't be a stranger in {TOWN}! We'll remember you.",
    "#k Mom gave you our best one! She said so herself.",
    "#ao If anyone in {NEXT} asks where you got it, say the {FAMILY} family. We like to be talked about.",
    "#o Come back when you've won. Or when you haven't. My door's always open.",
    "#k When I'm big I'm going to give TRAINERS presents too!",
    "#ao Your POK@MON looked at me just now as if to say thank you. You're welcome, little one.",
    "We'll be watching for you on the news! {REPORTER} and {CAMERA} cover everything, you know.",
    "#aoH Let your POK@MON hold it in a tough battle. You'll see.",
    "#E Eat one when the road gets long. Or let your POK@MON have it. They'll pretend to share.",
    "#D Teach it to a POK@MON you trust. A move is a promise, my teacher used to say.",
    "#B Every one of those has a POK@MON waiting for it somewhere. Go and find them.",
    "#ao The house feels lighter now. Don't take that the wrong way.",
    "#o I gave one of those to a TRAINER years ago. They became the CHAMPION. No pressure.",
    "#k Bye! Wave when you go past the window! ...I'll be at the window.",
);
TEXT_BANK(sFolkGift,
    "Oh! You're a TRAINER, aren't you?|I found this, but I have no use for it. You can have it!",
    "#H My POK@MON outgrew this. I bet yours could use it!",
    "#k Take this! Everyone in {TOWN} says a TRAINER should have one.",
    "#ao I've carried this around {TOWN} for weeks waiting for the right person. Turns out it's you!",
    "#k I found this on {ROUTE}! Finders keepers. But I'm giving it to you.",
    "#o Here, young one. I'm too old to be carrying this about. Take it.",
    "#ao We {TRADE_FOLK} get paid in odd things sometimes. Like this! You have it.",
    "#ao A TRAINER from {NEIGHBOR} passed through and left this behind. You look like you need it more.",
    "#k I was saving this for my birthday. But you need it more!",
    "#o When I was your age, a stranger in {TOWN} gave me one of these. Now it's your turn.",
    "You look like you've come a long way to {TOWN}.|Here. This should help a bit.",
    "#ao My shop had one too many of these. Lucky you!",
    "#k You battled on {ROUTE}, didn't you? I saw! Here, a present for winning!",
    "#ao I'm clearing out my pockets before the {FESTIVAL}. You just got lucky.",
    "#o My POK@MON found this under the porch and won't give it back to anyone but a TRAINER. Here.",
    "#E I picked too many this morning. Take some, before they go soft.",
    "#k Shh! I've got a secret present. And it's for you!",
    "#ao Wait! You dropped this. ...No? Well, now you've found it. Keep it.",
    "#S I've no POK@MON that this stone does anything for. Maybe one of yours will light up.",
    "#ao A {LOCALMON} left this on my step. I think it's a thank you. I'm passing the thanks on.",
    "#o I've been standing here so long I forgot why. Oh, yes! To give this to someone. Here.",
    "#k My big sister is a TRAINER too. She'd want you to have this. I'll tell her later.",
);
TEXT_BANK(sFolkGiftAfter,
    "Good luck out there!",
    "#k Tell me if it works!",
    "#o Mind how you go, now. {ROUTE} can be rough.",
    "Use it well! Or at least use it.",
    "#ao Go get them, TRAINER! {TOWN} is cheering for you.",
    "#k Don't lose it! I almost did.",
    "#ao That's one less thing in my pockets. Thanks!",
    "#o Off you go. And eat something before {NEXT}.",
    "Come back to {TOWN} and show me how you did!",
    "#ao My POK@MON says good luck too!",
    "#k Now we're friends! That's how it works in {TOWN}.",
    "#ao If {LEADER} asks, you got it from me. Name-drop me. Please.",
    "#o There. Now I can go home and put my feet up.",
    "#k I'm going to tell everyone I gave a present to a real TRAINER!",
    "#ao Don't thank me. Thank whoever left it on {ROUTE}.",
    "#o I used to be the one going places. Now I give things to the ones who are. It's nice too.",
    "#k Next time you come, bring me something back from {NEXT}!",
    "#ao Off to {NEXT}, then? Say hello to the road for me.",
    "#o Look at that, my pockets are lighter and my heart is too.",
    "#ao Lucky day for both of us. You got a present, and I got to see a TRAINER smile.",
);

// ---------------------------------------------------------------------------
// The specialty counter's clerk, by shop (RSHOP_COUNT: the Hardcore berry
// stall)

TEXT_BANK(sShopBattle,
    "Welcome! X items for battle, the tough TRAINER's way!",
    "An X ATTACK at the right moment beats a good speech. Every TRAINER in {TOWN} knows that.",
    "Everyone heading out of {TOWN} for a GYM stops here first. Have a look!",
    "Battle supplies! Half of {TOWN} is out training. The other half is shopping here.",
);
TEXT_BANK(sShopVitamin,
    "I'm the ENERGY GURU of {TOWN}! Vitamins make POK@MON strong!",
    "Vitamins! For POK@MON who want to be just a little bit more.",
    "Good food, good sleep, good vitamins. That's the secret of every strong POK@MON in {TOWN}!",
);
TEXT_BANK(sShopStone,
    "Evolution stones! Dug up round {TOWN}, and some from very far away.",
    "A FIRE STONE, a WATER STONE... Every POK@MON has its moment. Take a look.",
    "Stones that make POK@MON grow up! Not that you'd want to rush them.",
    "The finest stones in {REGION}, right here in {TOWN}. {NEIGHBOR} sells pebbles.",
);
TEXT_BANK(sShopBall,
    "A BALL for every catch! The best selection in {TOWN}. Well, the only one.",
    "We've a BALL for every POK@MON that ever got away. Have a look!",
    "Pick a BALL, any BALL. Well, not any. Those ones cost more.",
    "TRAINERS come all the way from {NEIGHBOR} for our BALLS. Have a look!",
);
TEXT_BANK(sShopTm,
    "This is the TM corner of {TOWN}. Teach your POK@MON new moves!",
    "TMs! One new move can turn a whole battle around.",
    "Every TM here was tested on my own POK@MON. They're very well-educated now.",
);
TEXT_BANK(sShopSea,
    "#s Cold drinks and sea gear! All you need for the beaches of {TOWN}.",
    "Gear for the water! Everything a TRAINER needs by the sea or the shore.",
    "Nets, goggles, the works. Water is hard on a BAG.",
    "#a The {TRADE_FOLK} of {TOWN} buy their gear here. Now you can too!",
);
TEXT_BANK(sShopHeld,
    "Hold items, bought and sold here in {TOWN}. See anything you like?",
    "Something for your POK@MON to hold onto. Every little bit helps!",
    "A good hold item is like a lucky charm. Except it works.",
);
TEXT_BANK(sShopHerb,
    "Welcome to the HERB SHOP of {TOWN}! Bitter, but they work wonders.",
    "Herbs from round {TOWN}. Bitter as anything. Work like anything.",
    "My herbs taste awful. That's how you know they're working.",
);
TEXT_BANK(sShopType,
    "Charms that power up the moves of one type. Every one made in {TOWN}!",
    "Pick a type, any type. I've a charm to make it stronger.",
    "A little charm for your favorite POK@MON. It'll thank you in battle.",
);
TEXT_BANK(sShopBerries,
    "BERRIES for battle! A POK@MON eats them on its own.",
    "Fresh BERRIES from round {TOWN}! Your POK@MON knows just when to eat them. Smarter than me.",
    "BERRIES picked this morning! Well, yesterday. Still good!",
);
static const struct RogueTextBank *const sShopBanks[RSHOP_COUNT + 1] =
{
    [RSHOP_BATTLE] = &sShopBattle, [RSHOP_VITAMIN] = &sShopVitamin, [RSHOP_STONE] = &sShopStone, [RSHOP_BALL] = &sShopBall,
    [RSHOP_TM] = &sShopTm, [RSHOP_SEA] = &sShopSea, [RSHOP_HELD] = &sShopHeld, [RSHOP_HERB] = &sShopHerb,
    [RSHOP_TYPE] = &sShopType, [RSHOP_COUNT] = &sShopBerries,
};

// ---------------------------------------------------------------------------
// The professor's aide, the school, the fan club

// Tired, precise, a little dry; opinions about the professor's filing.
TEXT_BANK(sLabAide,
    "I've filed every note the PROF. has written about {REGION}. He writes on napkins. Napkins!",
    "Your partner's POK@ BALL came from this lab. Please take good care of it. We keep records.",
    "The PROF. calls this a lab. I call it a shed with a microscope. But it's our shed!",
    "If the PROF. tells you about the {LOCALMON} on {ROUTE}, just nod. It's a long story.",
    "I sort the POK@MON files by number. The PROF. sorts them by how much he likes them. It's a system.",
    "The PROF. hasn't slept since he started on {REGION}. Neither have I. I'm fine. I'm fine.",
    "Field research means mud on the floor. Every single day. Wipe your feet, please.",
    "Please don't touch anything. Especially the things the PROF. says you can touch.",
);
TEXT_BANK(sTeacherIntro,
    "Welcome to the POK@MON SCHOOL! Today's lesson is about the GYM.|",
    "Class, we have a visitor! Today, a word about the GYM LEADER.|",
    "Ah, a TRAINER! Do sit in. We're studying the GYM today.|",
);
TEXT_BANK(sStudent,
    "FIRE beats GRASS, GRASS beats WATER, WATER beats FIRE. Easy!",
    "Did you know GHOST moves can't touch NORMAL types? And NORMAL moves can't touch GHOSTS!",
    "ELECTRIC moves do nothing to GROUND types. I learned that today. It's in my notes.",
    "Teacher says real battles teach more than books. Then gives us more books.",
    "I'm writing a report on {LOCALMON}. So far it says: they're cute.",
    "In history today: {FIGURE} {FIGURE_DEED}. There's a test on it tomorrow.",
    "Psst. What's the answer to number four? ...You don't know either?",
    "When the bell rings, I'm going to go and catch {LOCALMON}. Teacher says that's not homework.",
);
TEXT_BANK(sFanClub,
    "I adore POK@MON! And your {LEAD} looks simply splendid!|Treat it well. A partner like that is a friend for life.",
    "Oh my! Is that {LEAD}? The coat! The posture! Splendid!",
    "Our club meets every day. We talk about POK@MON. Then we talk about POK@MON some more.",
    "The finest POK@MON in {REGION}? Why, mine, of course. And yours. And everyone's, really.",
    "We have members from every town in {REGION}. Even {NEIGHBOR}. We try not to hold it against them.",
    "Darling, your {LEAD} looks a little tired. Let it rest, won't you? For me?",
);

// ---------------------------------------------------------------------------
// The reporter pair (RogueCast.reporters), as GABBY and TY: met in their
// three towns (reporterTowns), then on TV. {REPORTER} gushes and is always
// on air; {CAMERA} says almost nothing.

// The first meeting, the second ({OTHER} the town they met before), the
// third.
// The reporter pair's meetings: the first one (wherever it is), the
// second (they remember you), the third (you're famous), and talking to them
// again where you met them already.
TEXT_BANK(sReporterMeet1,
    "Oh! A TRAINER! {REPORTER} here, live from {TOWN} for {REGION} TV!|We're doing a story on {LANDMARK}. How do you like {TOWN}? ...You like it? Wonderful!|{CAMERA}, did you get that?",
    "Oh! A TRAINER, here of all places! Okay, {CAMERA}, roll camera!|{REPORTER} here, in {TOWN}, home of the {TRADE_FOLK}! Tell our viewers, what's in your BAG? ...Ooh, a POTION!",
    "Excuse me! You, with the POK@ BALLS! {REPORTER}, {REGION} TV!|We're asking everyone in {TOWN} one thing: what makes a good TRAINER? ...Being nice to your POK@MON? {CAMERA}, write that down!",
);
TEXT_BANK(sReporterMeet2,
    "It's you again! {CAMERA}, it's that TRAINER from {OTHER}!|{REPORTER} here, live from {TOWN}! They say it has {PRIDE}. Our viewers loved you. Say something! ...Perfect!",
    "Oh! Hi! Do you remember us from {OTHER}? Now we're doing a story on {LANDMARK}!|You're in it now. Smile! {CAMERA}, keep rolling!",
    "Well, look who it is! The TRAINER from {OTHER}! {CAMERA}, get over here!|We're in {TOWN} this week. The {TRADE_FOLK} are wonderful. You're wonderful. Everyone's wonderful!",
);
TEXT_BANK(sReporterMeet3,
    "{PLAYER}! You're famous, you know! Everyone in {REGION} watches our show.|Today it's {TOWN} and {LANDMARK}. Want to be in it? You already are!",
    "Oh! It's {PLAYER}! {CAMERA}, it's {PLAYER}!|{REPORTER} here, live from {TOWN}, land of the {TRADE_FOLK}! Tell our viewers, where to next? ...The LEAGUE? Ooh!",
    "{PLAYER}! Third time! People will say we're following you. We are, a little.|{REPORTER} here in {TOWN}! One last question: was it all worth it? ...{CAMERA}, I'm not crying. You're crying.",
);
TEXT_BANK(sReporterAgain,
    "We've got you on tape already! But thank you!|{CAMERA}, one more shot of {TOWN}. The light's perfect!",
    "Back for more? You're a natural!|We'll be in {TOWN} a while yet. Wave to the viewers on your way!",
    "Sorry, {CAMERA} is changing the tape.|...It takes a while. {CAMERA} is very careful.",
);
TEXT_BANK(sCameraLines,
    "{CAMERA}: ...|{CAMERA} gives you a thumbs-up, then films a long, slow shot of {TOWN}.",
    "{CAMERA}: ...!|{CAMERA} points the camera at you, then at {REPORTER}, then at the sky.",
    "{CAMERA}: ...Hm.|{CAMERA} adjusts the lens. It looks like very serious work.",
    "{CAMERA}: ...|{CAMERA} nods at you, then at {TOWN}, then at the sky. Very professional.",
    "{CAMERA}: ...?|{CAMERA} is checking the tape. You're on it. You look good.",
    "{CAMERA}: ...|{CAMERA} wipes the lens, waves, and goes back to filming {REPORTER}.",
);

// ---------------------------------------------------------------------------
// TV: the region's own shows and news, in place of Hoenn's. A show is its
// title card, then what is on. Which show a set has on goes round with the
// house and the badges won, so the TVs of a town differ and change as the
// run goes on. A legend's news (LORE_WAY_TV) comes first, where the region
// has it (src/rogue_town.c).

// The news: what troubles a town ({TOWN}), before and after.
TEXT_BANK(sNewsVillains,
    "Reports say {TEAM} has set up camp in {TOWN}. People there are asked to stay calm.",
);
TEXT_BANK(sNewsVillainsAfter,
    "{TEAM} has left {TOWN}! Locals say a young TRAINER sent them packing.",
);
TEXT_BANK(sNewsSighted,
    "{TEAM} members were seen on {PROBLEM_AT}, near {TOWN}. Nobody knows what they're after.",
);
TEXT_BANK(sNewsSightedAfter,
    "{TEAM} hasn't been seen near {TOWN} since their hideout was cleared out. Hooray!",
);
TEXT_BANK(sNewsLost,
    "{EXPLORER} of {TOWN} is missing in {PROBLEM_AT}. If you see {EXPLORER}, please call in!",
);
TEXT_BANK(sNewsLostAfter,
    "Good news! {EXPLORER} is home safe from {PROBLEM_AT}, led out by a passing TRAINER.",
);
TEXT_BANK(sNewsLegend,
    "Strange tremors near {PROBLEM_AT}, outside {TOWN}. Scientists say it's nothing. Locals say it's something.",
);
TEXT_BANK(sNewsLegendAfter,
    "The tremors at {PROBLEM_AT} have stopped. Whatever stirred there has gone quiet.",
);
TEXT_BANK(sNewsBlocked,
    "The way from {TOWN} onto {PROBLEM_AT} is still blocked. Deliveries are late again.",
);
TEXT_BANK(sNewsBlockedAfter,
    "The way to {PROBLEM_AT} is open again! Deliveries to {TOWN} are back on time.",
);
TEXT_BANK(sNewsPower,
    "{PROBLEM_AT} is still dark, and the lights in {TOWN} keep flickering.",
);
TEXT_BANK(sNewsPowerAfter,
    "The lights are steady in {TOWN} again! Whatever ran loose in {PROBLEM_AT} has calmed down.",
);
TEXT_BANK(sNewsWreck,
    "Folks in {TOWN} hear noises from {PROBLEM_AT} at night. Spooky!",
);
TEXT_BANK(sNewsWreckAfter,
    "No more noises from {PROBLEM_AT}! {TOWN} sleeps easy tonight.",
);
TEXT_BANK(sNewsVolcano,
    "{VOLCANO} rumbled again near {TOWN}. Experts say there's no need to worry.",
);
TEXT_BANK(sNewsSea,
    "Rough seas keep the boats of {TOWN} in harbor for another week.",
);
TEXT_BANK(sNewsPests,
    "{LOCALMON} raided the gardens of {TOWN} again. Residents are building taller fences.",
);
TEXT_BANK(sNewsPet,
    "In {TOWN}, a pet POK@MON has wandered off. It was last seen looking very pleased with itself.",
);
TEXT_BANK(sNewsFestival,
    "{TOWN} is getting ready for the {FESTIVAL}! Organizers say everything is fine. Everything is not fine.",
);
static const struct RogueTextBank *const sNewsBanks[RPROB_COUNT][2] =
{
    [RPROB_VILLAINS]        = { &sNewsVillains, &sNewsVillainsAfter },
    [RPROB_VILLAIN_SIGHTED] = { &sNewsSighted, &sNewsSightedAfter },
    [RPROB_LOST]            = { &sNewsLost, &sNewsLostAfter },
    [RPROB_LEGEND]          = { &sNewsLegend, &sNewsLegendAfter },
    [RPROB_BLOCKED]         = { &sNewsBlocked, &sNewsBlockedAfter },
    [RPROB_POWER]           = { &sNewsPower, &sNewsPowerAfter },
    [RPROB_WRECK]           = { &sNewsWreck, &sNewsWreckAfter },
    [RPROB_VOLCANO]         = { &sNewsVolcano, NULL },
    [RPROB_ROUGH_SEA]       = { &sNewsSea, NULL },
    [RPROB_PESTS]           = { &sNewsPests, NULL },
    [RPROB_PET_LOST]        = { &sNewsPet, NULL },
    [RPROB_FESTIVAL]        = { &sNewsFestival, NULL },
};
// The sports at the end of the news: the player's BADGES.
TEXT_BANK(sNewsBadge1,
    "And in sports: a new TRAINER named {PLAYER} has won a first BADGE! One to watch!",
);
TEXT_BANK(sNewsBadges,
    "And in sports: {PLAYER} has won {BADGES} BADGES now! Could this be a new CHAMPION?",
    "And in sports: {BADGES} BADGES for {PLAYER}! GYM LEADERS all over {REGION} are nervous.",
    "And in sports: GYM LEADERS report a TRAINER called {PLAYER} with {BADGES} BADGES. Stay tuned!",
);

// The shows. Each starts with its title card.
TEXT_BANK(sTvRoad,
    "It's {REPORTER} and {CAMERA}'s ROAD SHOW!|{REPORTER}: This week we're in {TOWN}! The friendliest {TRADE_FOLK} in {REGION}!",
    "It's {REPORTER} and {CAMERA}'s ROAD SHOW!|{REPORTER}: Here we are at {LANDMARK}, in lovely {TOWN}! {CAMERA}, get a close-up!",
    "It's {REPORTER} and {CAMERA}'s ROAD SHOW!|{REPORTER}: In {TOWN} they say they have {PRIDE}. Let's find out!",
    "It's {REPORTER} and {CAMERA}'s ROAD SHOW!|{REPORTER}: We're on {ROUTE} today! The grass is tall and so are the stories!",
    "It's {REPORTER} and {CAMERA}'s ROAD SHOW!|{REPORTER}: {TOWN} or {NEIGHBOR}? This week, we settle it. Or start a fight. One of those.",
);
// When the player has been through the town (the interview airs).
TEXT_BANK(sTvRoadPlayer,
    "It's {REPORTER} and {CAMERA}'s ROAD SHOW!|{REPORTER}: In {TOWN} we met a TRAINER named {PLAYER}! Watch this, viewers!",
);
TEXT_BANK(sTvMon,
    "It's POK@MON CORNER!|Today we follow {LOCALMON} round {TOWN} for a whole day. It slept. It was wonderful.",
    "It's POK@MON CORNER!|Our guest today is {LOCALMON}, all the way from {TOWN}. It ate the microphone.",
    "It's POK@MON CORNER!|Why do {LOCALMON} gather at dusk near {TOWN}? Our experts have no idea. Stay tuned!",
    "It's POK@MON CORNER!|Viewers, send us your stories about {LOCALMON}! This week's winner: <it stole my lunch.>",
    "It's POK@MON CORNER!|A {LOCALMON} has been seen on {ROUTE} wearing a hat. We are looking into it.",
    "It's POK@MON CORNER!|How do you brush a {LOCALMON}? Very carefully, says our expert, from behind a chair.",
);
TEXT_BANK(sTvLeague,
    "It's LEAGUE WATCH!|{LEADER} of {GYM_TOWN} is taking on all challengers. Our host is not one of them.",
    "It's LEAGUE WATCH!|Who can beat {LEADER}'s {TYPE} POK@MON in {GYM_TOWN}? Not our host. Our host tried.",
    "It's LEAGUE WATCH!|Tonight: the GYM in {GYM_TOWN}. We asked {LEADER} for a secret. We got a stern look.",
    "It's LEAGUE WATCH!|Bring {TYPE}-proof POK@MON to {GYM_TOWN}, the experts say. The experts have never beaten {LEADER}.",
);
TEXT_BANK(sTvLeagueWon,
    "It's LEAGUE WATCH!|{LEADER} of {GYM_TOWN} lost the BADGE to a TRAINER named {PLAYER}! What a battle!",
    "It's LEAGUE WATCH!|Big news from {GYM_TOWN}! {PLAYER} has beaten {LEADER}! {LEADER} says, <Come back any time.>",
);
TEXT_BANK(sTvFestival,
    "It's a {FESTIVAL} SPECIAL!|In {FEST_TOWN}, {FEST_ACT}. See you there!",
    "It's FESTIVAL COUNTDOWN!|Only a few weeks to the {FESTIVAL} in {FEST_TOWN}! Have you got your outfit?",
    "It's a {FESTIVAL} SPECIAL!|We asked the people of {FEST_TOWN} what the {FESTIVAL} means to them. Most of them said <snacks.>",
);
TEXT_BANK(sTvKitchen,
    "#f It's KITCHEN TIME!|Today's recipe uses {GOODS} from {TOWN}. Step one: eat some. Step two: there is no step two.",
    "#f It's KITCHEN TIME!|Our chef made a pie with {GOODS} from {TOWN}. Our chef's POK@MON ate it. Back after this.",
    "It's KITCHEN TIME!|Today: soup for a tired TRAINER. You will need BERRIES, a pot, and a warm blanket.",
    "It's KITCHEN TIME!|Our chef is making POK@MON treats. The POK@MON are making a mess. Everyone is happy.",
);
TEXT_BANK(sTvDrama,
    "It's TALES OF {REGION}!|Tonight: {FIGURE} the {FIGURE_TITLE}. In this episode, {FIGURE} {FIGURE_DEED}. Gasp!",
    "It's TALES OF {REGION}!|Long ago, {FOUNDING}. Tonight's episode: the very first winter in {OLDTOWN}.",
    "It's TALES OF {REGION}!|Tonight: the first {FESTIVAL}. Nobody knew what to do, so they danced. It caught on.",
    "It's TALES OF {REGION}!|Tonight: young {FIGURE} leaves {FIGURE_TOWN} with one POK@MON and a map. To be continued!",
);
TEXT_BANK(sTvWeather,
    "It's the WEATHER!|Sunny over {TOWN}, clouds over {NEIGHBOR}, and a good chance of {LOCALMON} everywhere.",
    "It's the WEATHER!|A breeze over {TOWN} today. Hang out your washing. Hold onto your hat.",
    "It's the WEATHER!|Rain on {ROUTE} this afternoon. Pack an umbrella. Your POK@MON may enjoy a puddle.",
    "It's the WEATHER!|Clear skies tonight over {REGION}. A good night to count stars.",
    "It's the WEATHER!|Our forecaster says sun. Our forecaster's CASTFORM says rain. We'll find out together.",
);

// ---------------------------------------------------------------------------
// Books: on a home's shelves, its picture-book shelf, and the POKeMON
// CENTER's shelf (where no legend's book is kept: LORE_WAY_BOOK)

TEXT_BANK(sBookHome,
    "It's a book: TALES OF {REGION}.|Long ago, {FOUNDING}. The first houses went up in {OLDTOWN}.",
    "It's a book: THE {FESTIVAL}.|Every year in {FEST_TOWN}, {FEST_ACT}. Nobody remembers who started it.",
    "It's a book: {FIGURE}, A LIFE.|{FIGURE} the {FIGURE_TITLE} grew up in {FIGURE_TOWN}, and {FIGURE_DEED}.",
    "It's a well-thumbed book about {TRADE}.|The {FAMILY} family's name is written inside the cover.",
    "It's a field guide: POK@MON OF {TOWN}.|{LOCALMON} gets a whole chapter. Somebody has drawn a mustache on it.",
    "#f It's a recipe book.|Every recipe uses {GOODS}. Even the one for cake.",
    "It's a book of maps of {REGION}.|Somebody has circled {NEXT} and written <SOMEDAY!> beside it.",
    "It's a diary.|...You shouldn't read someone's diary. You put it back.",
    "It's a book: THE STORY OF {TOWN}.|Most of it is about {LANDMARK}. Chapter one is just a drawing of it.",
    "It's crammed full of books about {TRADE}.|Somebody here takes it very seriously.",
    "It's an album of old photos of {TOWN}.|Everyone in them is squinting at the sun.",
    "It's a book: TRAINING YOUR FIRST POK@MON.|Chapter one: be nice to it. That's most of the book.",
    "It's a book of poems about {TOWN}.|Every single poem ends with the word <home.>",
    "It's a book about the {TRADE_FOLK} of long ago.|There's a drawing of a very serious person in a very large hat.",
    "It's a book about raising POK@MON.|Somebody has underlined <patience> in three colors.",
    "It's a book: A HISTORY OF {REGION}'S GYMS.|The chapter on {LEADER} has a lot of exclamation marks.",
    "It's a book of letters.|They're all from {NEIGHBOR}. They all start <You'll never believe this.>",
    "It's a cookbook from {NEIGHBOR}.|Someone has crossed out every recipe and written {TOWN}'S WAY IS BETTER.",
    "It's a notebook of POK@MON seen from the window.|{LOCALMON} is on nearly every page.",
    "It's a book of old {TOWN} sayings.|<Slow and steady wins the race. Unless it's a POK@MON race.>",
);
TEXT_BANK(sBookPicture,
    "It's a picture book: {LOCALMON} GOES TO {TOWN}.|On the last page, {LOCALMON} falls asleep. The end.",
    "It's a picture book about the {FESTIVAL}.|Someone has colored it in, well outside the lines.",
    "It's a picture book: GOODNIGHT, {REGION}.|Goodnight, {TOWN}. Goodnight, moon. Goodnight, POK@MON everywhere.",
    "It's a picture book about POK@MON types.|FIRE is colored red. So is WATER, for some reason.",
    "It's a picture book: WHERE DO POK@MON SLEEP?|In trees. In caves. On your bed. Under the {FAMILY} family's stairs, sometimes.",
    "It's a picture book: {FIGURE} AND THE LONG ROAD.|On every page, {FIGURE} waves. The end.",
    "It's a picture book about a lost POK@ BALL that finds its way home to {TOWN}.|Somebody has written THE END in crayon.",
    "It's a counting book: ONE {LOCALMON}, TWO {LOCALMON}...|The last page has been nibbled by a POK@MON.",
    "It's a picture book: {LOCALMON} CAN'T SLEEP.|By the last page, {LOCALMON} is asleep. So is whoever was reading it.",
    "It's a picture book: THE LITTLE POK@ BALL.|The POK@ BALL rolls all over {REGION}, then home to bed.",
    "It's a picture book about {TOWN}.|Every house has a smiling face. Even the GYM.",
    "It's a picture book: THE NURSE WHO NEVER SLEPT.|On the last page she sleeps. Everyone cheers.",
    "It's a picture book about the {TRADE_FOLK} of {TOWN}.|Somebody has drawn themselves on every page.",
    "It's a picture book: I CAN COUNT TO TEN BADGES.|There are only eight. The last two pages are blank.",
    "It's a picture book: A RAINY DAY FOR {LOCALMON}.|{LOCALMON} splashes in every puddle in the book.",
    "It's a picture book about the {FESTIVAL}.|The pages are sticky. Someone read it while eating.",
    "It's a picture book: THE BRAVE LITTLE TRAINER.|The TRAINER is scared on every page, and keeps going.",
    "It's a picture book: {LOCALMON}'S BIG DAY OUT.|It goes all the way to {NEIGHBOR}, and back for supper.",
    "It's a picture book with flaps. Under every flap is a POK@MON.|Under the last flap is another flap.",
    "It's a picture book: THE MOON AND THE CLEFAIRY.|The pages are dark blue and covered in tiny stars.",
    "It's a picture book: WHO LIVES IN {TOWN}?|There's a drawing of every house. The {FAMILY} family's has the biggest smile.",
);
TEXT_BANK(sBookCenter,
    "It's a magazine: TRAINER WEEKLY.|This week: <{LEADER} of {GYM_TOWN}: what's the secret?> The page is torn out.",
    "It's a guide: SIGHTS OF {REGION}.|{TOWN}: famous for {PRIDE}. Ask anyone about {TRADE}.",
    "It's a book about POK@MON CENTERS.|It says they're all alike inside. You look around {TOWN}'s. It's right.",
    "It's the guest book.|TRAINERS from all over {REGION} have signed it. Someone drew {LOCALMON} next to their name.",
    "It's a POK@MON journal.|An article calls {LOCALMON} <misunderstood.> There are teeth marks on the page.",
    "It's a map of {REGION}.|Somebody has drawn a little star on {TOWN}. And a smaller one on {NEIGHBOR}.",
    "It's a puzzle book from {NEIGHBOR}.|Every puzzle has been done. In pen. Wrong.",
    "It's a magazine: POK@MON FANCY.|This month: <The best-groomed {LOCALMON} in {REGION}!>",
    "It's a guide to the {FESTIVAL}.|It says {FEST_ACT}. Somebody has added <AND SNACKS!>",
    "It's the lost and found book of {TOWN}.|Lost: one hat, one POK@ BALL, one GYM BADGE (fake). Found: one hat.",
    "It's a pamphlet: KEEP YOUR POK@MON HAPPY.|Step one: say hello. Step two: say hello again.",
    "It's a magazine: {REGION} TRAVELER.|This month: <Ten reasons to visit {TOWN}.> Reason one is the POK@MON CENTER.",
    "It's a book of letters from TRAINERS to the nurses of {REGION}.|Almost every letter says thank you. One says sorry about the mess.",
    "It's a guide: THE ROAD TO {NEXT}.|Most of it says <keep walking.> The last page says <you're there!>",
    "It's a picture book about a CHANSEY who works in a POK@MON CENTER.|It's been read so often the pages are soft.",
    "It's a notebook of TRAINERS' tips. Somebody's tip is <never give up.> Under it, someone else wrote <and eat lunch.>",
    "It's a thick book about the {TRADE_FOLK} of {TOWN}.|Somebody has bookmarked the page with the pictures.",
    "It's a magazine about the {FESTIVAL}.|A whole page is about the food. Another whole page is about the food.",
    "It's a POK@MON CENTER visitors' book.|A TRAINER from {NEIGHBOR} wrote: <Nice chairs. Better than ours.>",
    "It's a book of maps of the roads round {TOWN}.|{ROUTE} has a little drawing of {LOCALMON} on it.",
    "It's an old copy of TRAINER WEEKLY.|On the cover: <Can anyone beat {LEADER}?> Somebody has written <YES> in pen.",
    "It's a book called WAITING ROOM PUZZLES.|Every puzzle has been done. One of them, correctly.",
);

// ---------------------------------------------------------------------------
// Townsfolk: who says what

// A lost explorer's town that is not the cast's explorer (two dungeons with
// someone lost): said without a name.
TEXT_BANK(sProbLostOther,
    "#a Somebody from {TOWN} went into {PROBLEM_AT} days ago and hasn't come out.",
    "#k Grown-ups keep looking at {PROBLEM_AT} and whispering. Someone's lost in there.",
    "#o {PROBLEM_AT} swallowed a traveler again. Well, lost one. Someone should go and look.",
);
TEXT_BANK(sProbLostOtherAfter,
    "#a The traveler lost in {PROBLEM_AT} is home safe! Somebody led them out.",
    "#o Everyone's home from {PROBLEM_AT} now. That's how it should be.",
);

enum
{
    TT_TRADE, TT_LANDMARK, TT_PROBLEM, TT_GYM, TT_DUNGEON, TT_NEIGHBOR, TT_LOCALMON, TT_FESTIVAL, TT_FIGURE,
    TT_FOUNDING, TT_THEME, TT_NEXT, TT_PRIDE, TT_WILD, TT_LIFE, TT_TIP, TT_RIVAL, TT_LEAD, TT_ROUTE, TT_TRAVEL,
    TT_COUNT,
};

// Route folk (src/rogue.c sRouteFolkGfx): hiker, fisherman, camper,
// picnicker, little boy, old man, woman, boy.
static const u8 sRouteFolkVoice[8] = { RVOICE_ADULT, RVOICE_ADULT, RVOICE_KID, RVOICE_KID, RVOICE_KID, RVOICE_OLD, RVOICE_ADULT, RVOICE_KID };

// The next gym at or after this place along the main road, or ROGUE_NUM_GYMS.
u8 RogueText_NextGym(const struct RogueRegion *r, u8 place)
{
    u32 i;
    if (r->places[place].gym < ROGUE_NUM_GYMS)
        return r->places[place].gym;
    for (i = 0; i < 4; i++)
    {
        place = RogueGen_NextTownFrom(r, place);
        if (r->places[place].gym < ROGUE_NUM_GYMS)
            return r->places[place].gym;
        if (r->places[place].type == RTOWN_SUMMIT)
            break;
    }
    return ROGUE_NUM_GYMS;
}

// The voice of someone out on a route, by slot.
u8 RogueText_RouteFolkVoice(u32 slot)
{
    return sRouteFolkVoice[slot % 8];
}

#ifdef ROGUE_HOST
// The voice of anyone the game puts on a place's map from slot.
static u8 TownSlotVoice(const struct RogueRegion *r, u8 place, const struct RogueMap *m, u32 slot)
{
    if (r->places[place].kind != RPLACE_TOWN)
        return sRouteFolkVoice[slot % 8];
    return RogueText_FolkVoice(r, place, m, slot);
}
#endif

static bool8 TownHasFacts(const struct RogueTextCtx *c)
{
    return TextTown(c, c->town) && c->r->places[c->town].type != RTOWN_SUMMIT;
}

static void TownWhoOf(const struct RogueTextCtx *c, const struct RogueTownFacts *f, u8 voice, struct TownWho *w)
{
    w->voice = voice;
    w->near = f ? f->near : 0;
    w->goods = f ? TownGoodsKind(f->trade) : 0;
    // A town by the volcano (RFACT_NEAR_VOLCANO) has one on the map.
    w->volcano = (w->near & RFACT_NEAR_VOLCANO) != 0;
    w->badges = c->state ? c->state->badges : 0;
    w->theme = TextTown(c, c->town) ? TownTheme(c->r, c->town) : RTHEME_COUNT;
    w->gift = TGIFT_NONE;
    w->avoid = NULL;
    w->local = 0xFF;
}

// TRUE once the player has dealt with a town's trouble, where the game
// knows it: the villains' hideout cleared, the lost explorer led out, the
// dungeon's boss beaten, a badge that clears the blocked road won.
static bool8 TownProblemSolved(const struct RogueTextCtx *c, const struct RogueTownFacts *f)
{
    const struct RogueTextState *s = c->state;
    const struct RogueRegion *r = c->r;
    u32 g;
    if (s == NULL || f->problemAt == 0xFF)
        return FALSE;
    switch (f->problem)
    {
    case RPROB_VILLAINS:
    {
        // The team of the hideout under the town.
        u8 t = RogueGen_HideoutTeam(r, f->problemAt);
        return t != 0xFF && ((s->teamsCleared >> t) & 1);
    }
    case RPROB_VILLAIN_SIGHTED:
        return (s->flags & RTSTATE_HIDEOUT) != 0;
    case RPROB_LOST:
    {
        // The cast's explorer (the one the game follows: Rogue_TextState).
        struct RogueCast cast;
        RogueGen_Cast(r, &cast);
        return f->problemAt == cast.explorerDungeon && f->problemAt < 16 && ((s->rescued >> f->problemAt) & 1);
    }
    case RPROB_LEGEND:
    case RPROB_POWER:
    case RPROB_WRECK:
        return f->problemAt < 16 && ((s->bosses >> f->problemAt) & 1);
    case RPROB_BLOCKED:
        for (g = 0; g < ROGUE_NUM_GYMS; g++)
            if (((s->wonGyms >> g) & 1) && r->gymKey[g] == r->places[f->problemAt].obstacle)
                return TRUE;
        return FALSE;
    }
    return FALSE;
}

// The bank for a town's trouble now, or NULL.
static const struct RogueTextBank *TownProblemBank(const struct RogueTextCtx *c, const struct RogueTownFacts *f,
                                                   const struct RogueTextBank *const (*banks)[2])
{
    bool8 solved = TownProblemSolved(c, f);
    const struct RogueTextBank *const *pair;
    if (f->problem == RPROB_NONE || f->problem >= RPROB_COUNT)
        return NULL;
    pair = banks[f->problem];
    if (f->problem == RPROB_LOST && banks == sProblemBanks)
    {
        struct RogueCast cast;
        RogueGen_Cast(c->r, &cast);
        if (cast.explorerDungeon != f->problemAt)
            return solved ? &sProbLostOtherAfter : &sProbLostOther;
    }
    if (solved && pair[1] != NULL)
        return pair[1];
    return pair[0];
}

// A town's number, for picks spread over the region.
static u32 TownNumber(const struct RogueTextCtx *c)
{
    return c->town < c->r->nPlaces ? c->r->places[c->town].number : 0;
}

static bool8 TownRivalFits(const struct RogueTextCtx *c)
{
    const struct RogueTextState *s = c->state;
    return c->r->places[c->place].tier <= 1 && s != NULL && !(s->flags & RTSTATE_RIVAL1) && (s->flags & RTSTATE_STARTER);
}

// The topics a town's people take in turn: the town's own first (its trade,
// landmark, trouble, gym, the dungeon nearby, and the festival, famous
// figure or founding tale where they belong), in an order of the town's
// own; then the rest. Returns how many.
static u32 TownAgenda(const struct RogueTextCtx *c, const struct RogueTownFacts *f, bool8 game, u8 *out)
{
    const struct RogueRegion *r = c->r;
    const struct RoguePlace *p = &r->places[c->place];
    struct RogueRegionFacts rf;
    u8 core[8], more[16];
    u32 nc = 0, nm = 0, n = 0, i, key = FACTS_SALT + 1600 + c->place, start;

    if (p->kind == RPLACE_TOWN && p->type == RTOWN_SUMMIT)
    {
        static const u8 sSummit[] = { TT_THEME, TT_GYM, TT_LEAD, TT_THEME, TT_LIFE, TT_GYM };
        for (i = 0; i < LORE_COUNT_OF(sSummit); i++)
            if (sSummit[i] != TT_LEAD || (c->state && (c->state->flags & RTSTATE_STARTER)))
                out[n++] = sSummit[i];
        return n;
    }
    if (p->kind != RPLACE_TOWN)
    {
        // Out on a route: the road, its POKeMON, where people are going.
        static const u8 sRoute[] = { TT_ROUTE, TT_WILD, TT_TRAVEL, TT_DUNGEON, TT_TIP, TT_FESTIVAL };
        for (i = 0; i < LORE_COUNT_OF(sRoute); i++)
        {
            if (sRoute[i] == TT_WILD && p->area == 0xFF)
                continue;
            if (sRoute[i] == TT_DUNGEON && (!game || r->nDungeons == 0))
                continue;
            out[n++] = sRoute[i];
        }
        return n;
    }
    RogueGen_RegionFacts(r, &rf);
    if (p->type == RTOWN_HAMLET)
        out[n++] = TT_THEME;
    core[nc++] = TT_TRADE;
    if (RogueGen_LandmarkOnMap(r, f))
        core[nc++] = TT_LANDMARK;
    if (f->problem != RPROB_NONE)
        core[nc++] = TT_PROBLEM;
    core[nc++] = TT_GYM;
    if (game && r->nDungeons)
        core[nc++] = TT_DUNGEON;
    if (c->place == rf.festTown)
        core[nc++] = TT_FESTIVAL;
    if (c->place == rf.figureTown)
        core[nc++] = TT_FIGURE;
    if (c->place == rf.oldTown)
        core[nc++] = TT_FOUNDING;
    for (i = 0; i < nc; i++)
        out[n++] = core[RogueText_Deal(r->seed, key, i, nc)];
    if (f->neighbor != 0xFF)
        more[nm++] = TT_NEIGHBOR;
    if (f->localArea != 0xFF)
        more[nm++] = TT_LOCALMON;
    if (p->type != RTOWN_HAMLET)
        more[nm++] = TT_THEME;
    if (RogueGen_NextTownFrom(r, c->place) != c->place)
        more[nm++] = TT_NEXT;
    more[nm++] = TT_PRIDE;
    if (c->place != rf.festTown)
        more[nm++] = TT_FESTIVAL;
    if (r->places[RogueGen_RouteNear(r, c->place)].area != 0xFF)
        more[nm++] = TT_WILD;
    if (TownRivalFits(c))
        more[nm++] = TT_RIVAL;
    if (c->place != rf.figureTown)
        more[nm++] = TT_FIGURE;
    if (c->state && (c->state->flags & RTSTATE_STARTER))
        more[nm++] = TT_LEAD;
    more[nm++] = TT_LIFE;
    more[nm++] = TT_TIP;
    // The rival passing through is news: right after the town's own.
    start = RogueRng_Hash(r->seed, key + 1) % 3;
    for (i = 0; i < nm; i++)
        out[n++] = more[(start + i) % nm];
    return n;
}

static bool8 TownGameTopic(struct RogueTextCtx *c, RogueGameTopic gameTopic, u32 rng)
{
    return gameTopic != NULL && gameTopic(TT_DUNGEON, c->place, rng);
}

// One topic's line into out. k: which use of the topic in this town, its
// own for each speaker (TownUse), so no two people of a town say one line:
// a bank of the town's own is never taken round again (TOWN_ONCE), a
// region's gives each use its own line. FALSE if the topic has nothing (more)
// to say here.
static bool8 TownTopicLine(struct RogueTextCtx *c, const struct RogueTownFacts *f, const struct TownWho *w0, u8 topic, u32 k,
                           u32 rng, char *out, u32 size)
{
    struct TownWho wl = *w0, *w = &wl;
    const struct RogueRegion *r = c->r;
    const struct RoguePlace *p = &r->places[c->place];
    u32 kr = TownNumber(c) * 16 + k;
    wl.local = k < 16 ? k : 15; // (the town's own banks never go round again: TOWN_ONCE)
    struct RogueRegionFacts rf;
    switch (topic)
    {
    case TT_TRADE:
        return f && TownSayPick(c, TownTradeBank(f->trade), w, k, TOWN_ONCE, out, size);
    case TT_LANDMARK:
        return f && RogueGen_LandmarkOnMap(r, f) && TownSayPick(c, TownLandmarkBank(f->landmark), w, k, TOWN_ONCE, out, size);
    case TT_PRIDE:
        return f && TownSayPick(c, &sTownPride, w, kr, TOWN_SPREAD(c, 16), out, size);
    case TT_PROBLEM:
        return f && TownSayPick(c, TownProblemBank(c, f, sProblemBanks), w, k, TOWN_ONCE, out, size);
    case TT_NEIGHBOR:
        return f && TownSayPick(c, &sTownRival, w, kr, TOWN_SPREAD(c, 16), out, size);
    case TT_LOCALMON:
        return f && TownSayPick(c, &sTownLocalMon, w, kr, TOWN_SPREAD(c, 16), out, size);
    case TT_FESTIVAL:
        RogueGen_RegionFacts(r, &rf);
        if (c->place == rf.festTown)
            return TownSayPick(c, &sTownFestivalHere, w, k, TOWN_ONCE, out, size);
        return TownSayPick(c, &sTownFestival, w, kr, TOWN_SPREAD(c, 16), out, size);
    case TT_FIGURE:
        RogueGen_RegionFacts(r, &rf);
        if (c->place == rf.figureTown)
            return TownSayPick(c, &sTownFigureHere, w, k, TOWN_ONCE, out, size);
        return TownSayPick(c, &sTownFigure, w, kr, TOWN_SPREAD(c, 16), out, size);
    case TT_FOUNDING:
        return TownSayPick(c, &sTownFounding, w, k, TOWN_ONCE, out, size);
    case TT_THEME:
        if (p->kind == RPLACE_TOWN && p->type == RTOWN_HAMLET)
            return TownSayPick(c, &sThemeHamlet, w, k, TOWN_ONCE, out, size);
        return TownSayPick(c, sThemeBanks[TownTheme(r, c->place)], w, k, TOWN_ONCE, out, size);
    case TT_LIFE:
        return TownSayPick(c, &sTownLife, w, kr, TOWN_SPREAD(c, 16), out, size);
    case TT_TIP:
        return TownSayPick(c, &sTownTip, w, kr, TOWN_SPREAD(c, 16), out, size);
    case TT_LEAD:
        return TownSayPick(c, &sTownLead, w, kr, TOWN_SPREAD(c, 16), out, size);
    case TT_RIVAL:
        return TownRivalFits(c) && TownSayPick(c, &sTownRivalKid, w, kr, TOWN_SPREAD(c, 16), out, size);
    case TT_GYM:
    {
        u8 gym = RogueText_NextGym(r, c->place);
        if (gym >= ROGUE_NUM_GYMS)
        {
            // Past the last gym: the Elite Four, or the CHAMPION.
            if (!(rng & 1) && TownSayPick(c, &sTownChampion, w, kr, TOWN_SPREAD(c, 16), out, size))
                return TRUE;
            c->gym = ROGUE_NUM_GYMS + (rng >> 3) % ROGUE_NUM_E4;
            return TownSayPick(c, &sTownElite, w, kr, TOWN_SPREAD(c, 16), out, size);
        }
        c->gym = gym;
        if (p->gym == gym)
        {
            if (c->state && ((c->state->wonGyms >> gym) & 1))
                return TownSayPick(c, &sTownGymWon, w, kr, TOWN_SPREAD(c, 16), out, size);
            if (k > 0)
                return FALSE; // the gossip is the town's one line about its LEADER
            RogueText_Say(out, size, RogueText_LeaderGossip(r, gym), c);
            return TRUE;
        }
        return TownSayPick(c, &sTownNextGym, w, kr, TOWN_SPREAD(c, 16), out, size);
    }
    case TT_WILD:
    {
        u8 route = p->kind == RPLACE_TOWN ? RogueGen_RouteNear(r, c->place) : c->place;
        if (r->places[route].area == 0xFF)
            return FALSE;
        c->area = r->places[route].area;
        c->mon = rng % 6;
        c->route = route;
        return TownSayPick(c, &sTownWild, w, kr, TOWN_SPREAD(c, 16), out, size);
    }
    case TT_NEXT:
    {
        u8 next = RogueGen_NextTownFrom(r, c->place), town = c->town;
        const struct RogueTownFacts *facts = c->facts;
        struct RogueTownFacts nf;
        struct TownWho w2 = *w;
        bool8 ok;
        if (next == c->place)
            return FALSE;
        c->route = p->kind == RPLACE_TOWN ? RogueGen_RouteNear(r, c->place) : c->place;
        c->town = next;
        if (r->places[next].type == RTOWN_SUMMIT)
        {
            c->facts = NULL;
            ok = TownSayPick(c, &sTownNextSummit, w, k, TOWN_ONCE, out, size);
        }
        else
        {
            FactsTown(r, next, &nf); // not kept: this town's stay
            c->facts = &nf;
            w2.goods = TownGoodsKind(nf.trade);
            ok = TownSayPick(c, &sTownNext, &w2, kr, TOWN_SPREAD(c, 16), out, size);
        }
        c->town = town;
        c->facts = facts;
        return ok;
    }
    case TT_ROUTE:
    {
        // v1.1: the weather a route is known for, else its kind.
        u8 known = RogueGen_KnownWeather(r, c->place);
        const struct RogueTextBank *bank = known == RWEATHER_RAIN ? &sRouteRain : known == RWEATHER_STORM ? &sRouteStorm
                                         : known == RWEATHER_FOG ? &sRouteFog : p->type < RROUTE_COUNT ? sRouteBanks[p->type] : NULL;
        c->route = c->place;
        return bank != NULL && TownSayPick(c, bank, w, p->number, (u32)r->nPlaces + 2, out, size);
    }
    case TT_TRAVEL:
        c->route = c->place;
        return TownSayPick(c, &sRouteTravel, w, p->number, (u32)r->nPlaces + 2, out, size);
    }
    return FALSE;
}

// Where TEAM ROCKET's BOSS is a gym's LEADER too (GIOVANNI, the leaders
// domain's GiovanniGym): the town under whose streets the ROCKET hideout
// was, once it is cleared, and GIOVANNI's own town, once the hideout is
// cleared (his GYM gone quiet) and once his BADGE is won with the BOSS
// beaten (the town learns who he was). One person a town, its third.
TEXT_BANK(sRocketHideoutLeft,
    "#a A man in a fine suit walked out of {TOWN} the night TEAM ROCKET left. Nobody saw his face. Everybody saw his suit.",
    "#o TEAM ROCKET's BOSS left {TOWN} in a big black car. He didn't even look back. Good riddance, I say.",
    "#k I saw TEAM ROCKET's BOSS! He had a cat POK@MON and a really nice suit. Mom says I didn't see anything.",
);
TEXT_BANK(sRocketGymDark,
    "#a {LEADER}'s GYM has been dark since TEAM ROCKET left. Funny timing, isn't it?",
    "#o The GYM's been shut up since TEAM ROCKET cleared out. {LEADER} hasn't been seen at the shops either.",
    "#k The GYM lights are off! {LEADER} is away on business, they say. What business?",
);
TEXT_BANK(sRocketGymTwist,
    "#a Our LEADER, a crook? {LEADER} shook my hand at the {FESTIVAL}! I'm going to wash that hand.",
    "#o The LEAGUE has sent someone to run the GYM till they find a new LEADER. Somebody honest, I hope.",
    "#k {LEADER} was TEAM ROCKET's BOSS all along? I had his picture on my wall! ...I still do. I'm thinking about it.",
);

// The ROCKET reaction a town's third person has now, or NULL (see above).
static const struct RogueTextBank *TownRocketReaction(struct RogueTextCtx *c)
{
    const struct RogueRegion *r = c->r;
    const struct RogueTextState *st = c->state;
    u8 gym = GiovanniGym(r), t = RogueGen_TeamWithId(r, ROGUE_VILLAIN_ROCKET);
    s32 h;
    if (gym == 0xFF || t == 0xFF || st == NULL || !((st->teamsCleared >> t) & 1))
        return NULL;
    if (r->gymTown[gym] == c->place)
    {
        c->gym = gym;
        if ((st->wonGyms >> gym) & 1)
            return GiovanniBossBeaten(c) ? &sRocketGymTwist : NULL;
        return &sRocketGymDark;
    }
    h = RogueGen_TeamHideout(r, t);
    if (h >= 0 && RogueGen_TownNear(r, r->dungeons[h].host) == c->place)
        return &sRocketHideoutLeft;
    return NULL;
}

// The order the townsfolk start the topics at: the person's own turn (the
// town's agenda does the rest).
u32 RogueText_TownTalkOrder(const struct RogueRegion *r, u8 place, u32 order)
{
    (void)r, (void)place;
    return order;
}

// A townsperson's own hash (by their local id on the map).
u32 RogueText_TownTalkRng(const struct RogueRegion *r, u8 place, u32 localId)
{
    return RogueRng_Hash(r->seed, 8800 + place * 32 + localId);
}

// What a townsperson (or someone out on a route) says once the game's
// special people are done: c->place where they are, order their turn among
// the place's people, rng their own hash, voice how they look (RVOICE_*).
// The dungeon rumors are the game's (gameTopic writes the line itself and
// returns TRUE). Returns RTALK_TEXT with the line in out, or RTALK_GAME.
u8 RogueText_TownTalk(struct RogueTextCtx *c, u32 order, u32 rng, bool8 onRoute, u8 voice, RogueGameTopic gameTopic,
                      char *out, u32 size)
{
    struct RogueTownFacts tf;
    const struct RogueTownFacts *f = NULL;
    struct TownWho w;
    u8 agenda[24];
    u32 n, i;
    (void)onRoute;
    if (TownHasFacts(c))
    {
        RogueGen_TownFacts(c->r, c->town, &tf);
        c->facts = f = &tf;
    }
    TownWhoOf(c, f, voice, &w);
    // Out on a route, what people call it and what stands on it come first
    // (route facts: src/rogue_text_features.h).
    if (c->r->places[c->place].kind == RPLACE_ROUTE && RogueText_RouteFactLine(c, rng, out, size))
    {
        c->facts = NULL;
        return RTALK_TEXT;
    }
    // TEAM ROCKET's BOSS found out, where the town knows of it.
    if (order == 2 && c->r->places[c->place].kind == RPLACE_TOWN)
    {
        const struct RogueTextBank *b = TownRocketReaction(c);
        if (b != NULL && TownSayPick(c, b, &w, TownNumber(c), 4, out, size))
        {
            c->facts = NULL;
            return RTALK_TEXT;
        }
    }
    n = TownAgenda(c, f, gameTopic != NULL, agenda);
    // Out on the routes, each route's first person starts somewhere else.
    if (c->r->places[c->place].kind != RPLACE_TOWN)
        order += c->r->places[c->place].number;
    for (i = 0; i < n; i++)
    {
        u32 at = (order + i) % n, j, again = 0;
        u8 topic = agenda[at];
        if (topic == TT_DUNGEON)
        {
            if (TownGameTopic(c, gameTopic, rng))
            {
                c->facts = NULL;
                return RTALK_GAME;
            }
            continue;
        }
        u32 cnt = 0;
        // Each use of a topic in a town takes a line of its own: the street's
        // in the agenda's turn (a topic the agenda holds twice takes its next
        // line the second time, and round again the next after that), one
        // passed on to here by those before (whose own topic had nothing to
        // say) another, the homes' others (3 k + 2: RogueText_HomeTalk).
        for (j = 0; j < n; j++)
        {
            again += j < at && agenda[j] == topic;
            cnt += agenda[j] == topic;
        }
        again += (order + i) / n * cnt;
        if (TownTopicLine(c, f, &w, topic, i == 0 ? 3 * again : 3 * (again + i - 1) + 1, rng, out, size))
        {
            c->facts = NULL;
            return RTALK_TEXT;
        }
    }
    TownSayPick(c, sThemeBanks[TownTheme(c->r, c->place)], &w, order, TOWN_LOCAL, out, size);
    c->facts = NULL;
    return RTALK_TEXT;
}

// ---------------------------------------------------------------------------
// The people the game shows itself in a town's street, by their turn: the
// gift, a legend's tale, a hamlet's rare find, the villains, a ruins'
// riddle. The reporters take two others. (Mirrors Rogue_BufferTownTalk in
// src/rogue_town.c; the villains' first two, before their hideout is
// cleared.)
static u32 TownSpecialTurns(const struct RogueRegion *r, u8 place, u32 nFolk)
{
    const struct RoguePlace *p = &r->places[place];
    u32 taken = 0;
    s32 lore, h = RogueGen_DungeonAt(r, place), ruins = RogueGen_RuinsDungeon(r);
    bool8 gift = p->kind == RPLACE_TOWN && RogueGen_HasFolkGift(r, p->number);
    if (nFolk == 0)
        return 0;
    if (gift)
        taken |= 1u << (nFolk - 1);
    lore = (s32)nFolk - 1 - gift;
    if (lore < 0)
        lore = nFolk - 1;
    if (RogueGen_LoreAt(r, LORE_WAY_FOLK, place) >= 0)
        taken |= 1u << lore;
    if (p->kind == RPLACE_TOWN && p->type == RTOWN_HAMLET)
        taken |= 1;
    if (h >= 0 && r->dungeons[h].host == place && r->dungeons[h].kind == RDUN_HIDEOUT)
        taken |= 3;
    if (ruins >= 0 && p->kind == RPLACE_TOWN && RogueGen_TownNear(r, r->dungeons[ruins].host) == place)
        taken |= 1u << (nFolk >= 2 ? 1 : 0);
    return taken;
}

// Which of the reporter pair (RTEXT_REPORTER, RTEXT_CAMERA) the turn-th
// townsperson of a town is, or 0: in their towns, the first two side by
// side in the turns that the game's special people leave.
u8 RogueText_ReporterRole(const struct RogueRegion *r, u8 place, u32 nFolk, u32 turn)
{
    struct RogueCast cast;
    u32 taken, i;
    RogueGen_Cast(r, &cast);
    for (i = 0; i < 3 && cast.reporterTowns[i] != place; i++)
        ;
    if (i == 3 || place >= r->nPlaces || r->places[place].kind != RPLACE_TOWN)
        return 0;
    taken = TownSpecialTurns(r, place, nFolk);
    for (i = 0; i + 1 < nFolk; i++)
    {
        if (((taken >> i) & 3) == 0)
            return turn == i ? RTEXT_REPORTER : turn == i + 1 ? RTEXT_CAMERA : 0;
    }
    return 0;
}

// Which of the reporter pair's meetings (0-2, RogueCast.reporterTowns) is
// in this town, or -1.
s32 RogueText_ReporterMeeting(const struct RogueRegion *r, u8 place)
{
    struct RogueCast cast;
    u32 i;
    RogueGen_Cast(r, &cast);
    for (i = 0; i < 3; i++)
        if (cast.reporterTowns[i] == place)
            return i;
    return -1;
}

// What the reporter or the camera operator says in their town. met: the
// meetings had so far (a bit for each, FLAG_ROGUE_REPORTER_0 on), so the
// meetings go on from the last one the player had, and a talk where they
// met already is short.
void RogueText_Reporter(struct RogueTextCtx *c, u8 role, u8 met, char *out, u32 size)
{
    static const struct RogueTextBank *const sMeets[3] = { &sReporterMeet1, &sReporterMeet2, &sReporterMeet3 };
    struct RogueCast cast;
    struct TownWho w;
    s32 i = RogueText_ReporterMeeting(c->r, c->place), j;
    u32 before = 0;
    RogueGen_Cast(c->r, &cast);
    if (i < 0)
        i = 0;
    c->town = c->place;
    TownWhoOf(c, NULL, RVOICE_ANY, &w);
    for (j = 0; j < 3; j++)
        if (j != i && ((met >> j) & 1))
        {
            before++;
            c->other = cast.reporterTowns[j]; // the last place they met
        }
    if (role == RTEXT_CAMERA)
        TownSayPick(c, &sCameraLines, &w, i, 4, out, size); // (the same each talk: the camera rolls on)
    else if ((met >> i) & 1)
        TownSayPick(c, &sReporterAgain, &w, i, 4, out, size);
    else if (!TownSayPick(c, sMeets[before > 2 ? 2 : before], &w, i, 4, out, size))
        TownSayPick(c, &sReporterMeet1, &w, i, 4, out, size);
}

// ---------------------------------------------------------------------------
// Homes and special houses

u32 RogueText_HouseRng(const struct RogueRegion *r, u8 place, u8 house, u32 salt)
{
    return RogueRng_Hash(r->seed, 9700 + place * 64 + house * 8 + salt);
}

// The home whose old resident tells a legend's tale (LORE_WAY_ELDER): the
// town's first ordinary home of chat, or -1.
static s32 TownLoreElderHouse(const struct RogueRegion *r, u8 place, const struct RogueMap *m)
{
    u32 i;
    if (RogueGen_LoreAt(r, LORE_WAY_ELDER, place) < 0)
        return -1;
    for (i = 0; i < m->nHouses; i++)
        if (m->interiors[i] == RINT_HOME && RogueGen_HomeRole(r, place, m->interiors, m->nHouses, i) == RHOME_CHAT)
            return i;
    return -1;
}

// The home of the cast's lore elder (RogueCast.elder) in the oldest town:
// its first ordinary home of chat that is not the legend teller's, or -1.
s32 RogueText_CastElderHouse(const struct RogueRegion *r, u8 place, const struct RogueMap *m)
{
    struct RogueCast cast;
    s32 lore;
    u32 i;
    RogueGen_Cast(r, &cast);
    if (cast.elderTown != place || m == NULL)
        return -1;
    lore = TownLoreElderHouse(r, place, m);
    for (i = 0; i < m->nHouses; i++)
        if ((s32)i != lore && m->interiors[i] == RINT_HOME && RogueGen_HomeRole(r, place, m->interiors, m->nHouses, i) == RHOME_CHAT)
            return i;
    return -1;
}

// The voices of a home's people as the game dresses them (src/rogue_town.c
// BuildHouse): the host, and the second resident or RVOICE_NONE. An old one
// tells the tales.
void RogueText_HomeVoices(const struct RogueRegion *r, u8 place, const struct RogueMap *m, u8 house, u8 *v1, u8 *v2)
{
    u32 rng = RogueText_HouseRng(r, place, house, 0);
    u8 role = RogueGen_HomeRole(r, place, m->interiors, m->nHouses, house);
    *v1 = RogueText_FolkVoice(r, place, m, rng >> 8);
    if ((s32)house == TownLoreElderHouse(r, place, m) || (s32)house == RogueText_CastElderHouse(r, place, m)
     || role == RHOME_RELEARNER)
        *v1 = RVOICE_OLD;
    *v2 = RVOICE_NONE;
    if (role != RHOME_RELEARNER && role != RHOME_NAME_RATER && (rng >> 12) % 100 < 75)
        *v2 = RogueText_FolkVoice(r, place, m, (rng >> 8) + 3);
}

static bool8 TownDuetFits(struct RogueTextCtx *c, const char *t, const struct TownWho *w)
{
    u32 tags = TownTags(&t);
    return TownTagsFit(tags, w, FALSE) && RogueText_CanFill(t, c);
}

// Someone at home (localId 1 the host, 2 the other) who only chats: the
// cast's elder in theirs; two people talking to each other; one about
// themselves or their family; or the town's news, as the street has it.
static bool8 TownStreetLine(struct RogueTextCtx *c, const struct RogueTownFacts *f, const struct RogueMap *m, u8 topic,
                            char *out, u32 size);

// The duet (an index of sHomeDuets) the two people of a home talk in, or -1:
// homes of two who chat, now and then, not where the host is an elder, who
// says their own (so the other's reply never answers what was not said).
// The town's homes take them in turn, each the first in the region's order
// from its own place that fits its two and no home before it has, so no two
// homes of a town have one conversation.
static s32 TownDuet(struct RogueTextCtx *c, const struct RogueTownFacts *f, const struct RogueMap *m, u8 house)
{
    const struct RogueRegion *r = c->r;
    u32 used = 0, h, i, n = LORE_COUNT_OF(sHomeDuets);
    s32 elder = RogueText_CastElderHouse(r, c->place, m), lore = TownLoreElderHouse(r, c->place, m), pick = -1;
    struct TownWho w, w2;
    u8 savedHouse = c->house;
    for (h = 0; h <= house && h < m->nHouses; h++)
    {
        u8 v1, v2;
        u32 r2 = RogueRng_Hash(r->seed, FACTS_SALT + 1500 + c->place * 8 + h), kh = TownNumber(c) * ROGUE_MAX_HOUSES + h;
        pick = -1;
        if (m->interiors[h] != RINT_HOME || RogueGen_HomeRole(r, c->place, m->interiors, m->nHouses, h) != RHOME_CHAT
         || (r2 & 3) == 0 || (s32)h == elder || (s32)h == lore)
            continue;
        RogueText_HomeVoices(r, c->place, m, h, &v1, &v2);
        if (v2 == RVOICE_NONE)
            continue;
        c->house = h;
        TownWhoOf(c, f, v1, &w);
        TownWhoOf(c, f, v2, &w2);
        for (i = 0; i < n && pick < 0; i++)
        {
            u32 at = RogueText_Deal(r->seed, FACTS_SALT + 1510, kh + i, n);
            if (!((used >> at) & 1) && TownDuetFits(c, sHomeDuets[at][0], &w) && TownDuetFits(c, sHomeDuets[at][1], &w2))
                pick = at;
        }
        if (pick >= 0)
            used |= 1u << pick;
    }
    c->house = savedHouse;
    return pick;
}

u8 RogueText_HomeTalk(struct RogueTextCtx *c, const struct RogueMap *m, u8 house, u32 localId, RogueGameTopic gameTopic,
                      char *out, u32 size)
{
    static const u8 sHomeTopics[] = { TT_PROBLEM, TT_NEIGHBOR, TT_FESTIVAL, TT_LOCALMON, TT_PRIDE, TT_FIGURE, TT_NEXT, TT_LIFE };
    static const struct RogueTextBank *const sVoiceBanks[] = { &sHomeAdult, &sHomeKid, &sHomeAdult, &sHomeOld };
    const struct RogueRegion *r = c->r;
    struct RogueTownFacts tf;
    struct TownWho w;
    u8 v1, v2, me, role;
    u32 r2 = RogueRng_Hash(r->seed, FACTS_SALT + 1500 + c->place * 8 + house), kh, choice;
    bool8 ok = FALSE;

    c->house = house;
    c->town = c->place;
    RogueText_HomeVoices(r, c->place, m, house, &v1, &v2);
    role = RogueGen_HomeRole(r, c->place, m->interiors, m->nHouses, house);
    me = localId == 1 ? v1 : v2;
    if (me == RVOICE_NONE)
        me = RVOICE_ADULT;
    kh = TownNumber(c) * ROGUE_MAX_HOUSES + house;
    RogueGen_TownFacts(r, c->place, &tf);
    c->facts = &tf;
    c->route = RogueGen_RouteNear(r, c->place);
    TownWhoOf(c, &tf, me, &w);
    // The elder.
    if (localId == 1 && (s32)house == RogueText_CastElderHouse(r, c->place, m))
    {
        // Who they are first; then a new tale with each BADGE.
        u32 badges = c->state ? c->state->badges : 0;
        c->speaker = RSPK_ELDER; // the game asks as RSPK_HOME, the dump as the elder: one pick
        w.voice = RVOICE_OLD;
        ok = badges == 0 ? TownSayPick(c, &sElderIntro, &w, 0, TOWN_LOCAL, out, size)
                         : TownSayPick(c, &sElderLines, &w, badges - 1, TOWN_LOCAL, out, size);
    }
    // Two people, one conversation (TownDuet: each home of a town its own).
    if (!ok && role == RHOME_CHAT && v2 != RVOICE_NONE)
    {
        s32 d = TownDuet(c, &tf, m, house);
        if (d >= 0)
        {
            const char *t = sHomeDuets[d][localId == 1 ? 0 : 1];
            TownTags(&t);
            RogueText_Say(out, size, t, c);
            ok = TRUE;
        }
    }
    // One person: the family, themselves, or the news.
    choice = (r2 >> (4 + localId * 3)) % 8;
    if (!ok && choice < 2 && (localId == 1 || v1 == RVOICE_NONE || role != RHOME_CHAT))
        ok = TownSayPick(c, &sHomeFamily, &w, kh, TOWN_SPREAD(c, ROGUE_MAX_HOUSES), out, size);
    if (!ok && choice >= 5)
    {
        u8 topic = sHomeTopics[(house * 2 + localId + (r2 >> 12)) % LORE_COUNT_OF(sHomeTopics)];
        // Its own use of the topic in the town: the homes before this one
        // (and the host before the other) that may take it too.
        u32 h, id, use = 0;
        for (h = 0; h <= house; h++)
            for (id = 1; id <= 2 && (h < house || id < localId); id++)
            {
                u32 rh = RogueRng_Hash(r->seed, FACTS_SALT + 1500 + c->place * 8 + h);
                use += (rh >> (4 + id * 3)) % 8 >= 5 && sHomeTopics[(h * 2 + id + (rh >> 12)) % LORE_COUNT_OF(sHomeTopics)] == topic;
            }
        ok = TownTopicLine(c, &tf, &w, topic, 3 * use + 2, r2 >> 16, out, size);
        // Never what the street says of it (its bank's lines for another
        // voice can meet this one's): a line past every home's own instead.
        if (ok)
        {
            char street[200];
            if (TownStreetLine(c, &tf, m, topic, street, sizeof(street)) && strcmp(street, out) == 0)
                ok = TownTopicLine(c, &tf, &w, topic, 3 * (use + ROGUE_MAX_HOUSES * 2) + 2, r2 >> 16, out, size) && strcmp(street, out) != 0;
        }
    }
    if (!ok)
        ok = TownSayPick(c, sVoiceBanks[me % 4], &w, kh * 2 + localId, TOWN_SPREAD(c, 2 * ROGUE_MAX_HOUSES), out, size);
    if (!ok)
        TownSayPick(c, &sHomeAdult, &w, kh * 2 + localId, TOWN_SPREAD(c, 2 * ROGUE_MAX_HOUSES), out, size);
    (void)gameTopic;
    c->facts = NULL;
    return RTALK_TEXT;
}

// What the first of a town's street people whose turn is a topic says of
// it (as RogueText_TownTalk has them: by the agenda, k 0), into out; FALSE
// if nobody in the street takes it.
static bool8 TownStreetLine(struct RogueTextCtx *c, const struct RogueTownFacts *f, const struct RogueMap *m, u8 topic,
                            char *out, u32 size)
{
    struct RogueTextCtx keep = *c;
    struct TownWho w;
    u8 agenda[24];
    u32 n = TownAgenda(c, f, TRUE, agenda), i, order = 0;
    bool8 ok = FALSE;
    for (i = 0; i < m->nObjs && n; i++)
    {
        if (m->objs[i].kind != ROBJ_VILLAGER)
            continue;
        if (agenda[order % n] == topic)
        {
            TownWhoOf(c, f, RogueText_FolkVoice(c->r, c->place, m, m->objs[i].slot), &w);
            ok = TownTopicLine(c, f, &w, topic, order / n * 3, RogueText_TownTalkRng(c->r, c->place, i + 1), out, size);
            break;
        }
        order++;
    }
    *c = keep;
    return ok;
}

// The plate beside a home: its family's name (the gift house's too, with a
// welcome for TRAINERS: gift TRUE).
void RogueText_HouseSign(struct RogueTextCtx *c, u8 house, bool8 gift, char *out, u32 size)
{
    struct TownWho w;
    c->house = house;
    c->town = c->place;
    TownWhoOf(c, NULL, RVOICE_ANY, &w);
    if (!TownSayPick(c, gift ? &sGiftHouseSign : &sHouseSign, &w, TownNumber(c) * ROGUE_MAX_HOUSES + house,
                     TOWN_SPREAD(c, ROGUE_MAX_HOUSES), out, size))
        RogueText_Say(out, size, "THE {FAMILY} HOUSE", c);
}

// A glass cabinet in a home (its lower half, where the player reads it):
// what the family keeps behind the glass. Players take it for a TV, so it
// has its say.
// Which of a town's homes with this furniture (RFURN_*) a house is, from
// the first: the region's uses of a furniture's bank are town * 3 + this,
// so a town's shelves never share a book while the bank lasts.
static u32 TownFurnitureRank(const struct RogueRegion *r, u8 place, u8 house, u8 furn)
{
    u32 h, rank = 0;
    for (h = 0; h < house && h < ROGUE_MAX_HOUSES; h++)
        rank += (RogueText_HomeFurniture(r, place, h) & furn) != 0;
    return rank;
}

void RogueText_Cabinet(struct RogueTextCtx *c, u8 house, char *out, u32 size)
{
    struct RogueTownFacts tf;
    struct TownWho w;
    c->house = house;
    c->town = c->place;
    RogueGen_TownFacts(c->r, c->place, &tf);
    c->facts = &tf;
    c->route = RogueGen_RouteNear(c->r, c->place);
    TownWhoOf(c, &tf, RVOICE_ANY, &w);
    w.local = TownFurnitureRank(c->r, c->place, house, RFURN_CABINET);
    if (!TownSayPick(c, &sHomeCabinet, &w, TownNumber(c) * 3 + w.local, TOWN_SPREAD(c, 3), out, size))
        RogueText_Say(out, size, "A glass cabinet. The {FAMILY} family's best plates are inside.", c);
    c->facts = NULL;
}

// The k-th visitor (0 or 1) resting in a town's POKeMON CENTER (m the
// town's map, for how they look), never saying what the other says.
void RogueText_CenterFolk(struct RogueTextCtx *c, const struct RogueMap *m, u32 k, char *out, u32 size)
{
    struct RogueTownFacts tf;
    struct TownWho w;
    const struct RogueTextBank *b = &sCenterFolk;
    const char *other = NULL;
    u8 gym;
    c->town = RogueGen_TownNear(c->r, c->place);
    c->route = RogueGen_RouteNear(c->r, c->place);
    gym = RogueText_NextGym(c->r, c->place);
    c->gym = gym < ROGUE_NUM_GYMS ? gym : 0xFF;
    if (TownHasFacts(c))
    {
        RogueGen_TownFacts(c->r, c->town, &tf);
        c->facts = &tf;
    }
    if (c->r->places[c->town].type == RTOWN_SUMMIT)
        b = &sCenterSummit;
    if (k == 1)
    {
        TownWhoOf(c, c->facts, RogueText_CenterFolkVoice(c->r, c->place, m, 0), &w);
        other = TownPick(b, c, &w, TownNumber(c) * 2, TOWN_SPREAD(c, 2));
    }
    TownWhoOf(c, c->facts, RogueText_CenterFolkVoice(c->r, c->place, m, k), &w);
    w.avoid = other;
    if (!TownSayPick(c, b, &w, TownNumber(c) * 2 + k, TOWN_SPREAD(c, 2), out, size))
        RogueText_Say(out, size, "My POK@MON are with the nurse. I'm not worried. I'm a little worried.", c);
    c->facts = NULL;
}

// The house a sign at x, y names: the house whose door is nearest on its
// row (the plate stands beside the door), or -1, also when two doors are
// as near (a plate between two houses; the generator puts none there,
// PutHouseSign).
s32 RogueText_SignHouse(const struct RogueMap *m, u8 x, u8 y)
{
    s32 best = -1, bestD = 99;
    bool8 tie = FALSE;
    u32 i;
    for (i = 0; i < m->nWarps; i++)
    {
        const struct RogueWarp *wp = &m->warps[i];
        s32 house = wp->kind == RWARP_HOUSE ? 0 : (wp->kind > RWARP_CAVE && wp->kind < RWARP_CAVE + ROGUE_MAX_HOUSES) ? wp->kind - RWARP_CAVE : -1;
        // A plate stands beside the door, on its row, or a step or two
        // down by the path in front (AddHouseSign); a treehouse's at the
        // foot of its stairs.
        s32 d = (wp->x > x ? wp->x - x : x - wp->x) + 2 * (y - wp->y);
        if (house < 0 || house >= m->nHouses || wp->y > y || wp->y + 4 < y)
            continue;
        if (d < bestD)
            best = house, bestD = d, tie = FALSE;
        else if (d == bestD)
            tie = TRUE;
    }
    return tie ? -1 : best;
}

// The professor's aide, the school's students and teacher, the fan club.
void RogueText_HouseRole(struct RogueTextCtx *c, u8 role, u8 house, u32 localId, char *out, u32 size)
{
    const struct RogueRegion *r = c->r;
    struct TownWho w;
    u8 gym;
    u32 n, badges = c->state ? c->state->badges : 0;
    c->town = c->place;
    c->house = house;
    c->route = RogueGen_RouteNear(r, c->place);
    TownWhoOf(c, NULL, RVOICE_ANY, &w);
    out[0] = 0;
    switch (role)
    {
    case RTEXT_ROLE_LAB:
        TownSayPick(c, &sLabAide, &w, badges, TOWN_LOCAL, out, size);
        break;
    case RTEXT_ROLE_STUDENT:
        TownSayPick(c, &sStudent, &w, localId + badges * 2, TOWN_LOCAL, out, size);
        break;
    case RTEXT_ROLE_TEACHER:
        gym = RogueText_NextGym(r, c->place);
        c->gym = gym < ROGUE_NUM_GYMS ? gym : 0;
        n = 0;
        {
            const char *t = TownPick(&sTeacherIntro, c, &w, badges, TOWN_LOCAL);
            if (t != NULL)
                n = RogueText_Say(out, size, t, c);
        }
        RogueText_Say(out + n, size - n, RogueText_LeaderGossip(r, c->gym), c);
        break;
    case RTEXT_ROLE_FAN:
        TownSayPick(c, &sFanClub, &w, localId + badges, TOWN_LOCAL, out, size);
        break;
    }
}

// The gift house's offer (house its number; m the town's map, for who
// gives it).
void RogueText_GiftOffer(struct RogueTextCtx *c, const struct RogueMap *m, u8 house, u16 item, char *out, u32 size)
{
    struct RogueTownFacts tf;
    struct TownWho w;
    u8 v1 = RVOICE_ADULT, v2;
    c->town = c->place;
    c->house = house;
    if (m != NULL && house < m->nHouses)
        RogueText_HomeVoices(c->r, c->place, m, house, &v1, &v2);
    RogueGen_TownFacts(c->r, c->place, &tf);
    c->facts = &tf;
    TownWhoOf(c, &tf, v1, &w);
    w.gift = TownGiftKind(item);
    TownSayPick(c, &sGiftOffer, &w, TownNumber(c), TOWN_SPREAD(c, 1), out, size);
    c->facts = NULL;
}

// What the gift house says once its gift is given.
void RogueText_GiftAfter(struct RogueTextCtx *c, const struct RogueMap *m, u8 house, u16 item, char *out, u32 size)
{
    struct TownWho w;
    u8 v1 = RVOICE_ADULT, v2;
    c->town = c->place;
    c->house = house;
    if (m != NULL && house < m->nHouses)
        RogueText_HomeVoices(c->r, c->place, m, house, &v1, &v2);
    TownWhoOf(c, NULL, v1, &w);
    w.gift = TownGiftKind(item);
    TownSayPick(c, &sGiftAfter, &w, TownNumber(c), TOWN_SPREAD(c, 1), out, size);
}

// A townsperson's small gift (the town's last person), and their word
// after.
void RogueText_FolkGift(struct RogueTextCtx *c, u8 voice, bool8 after, u16 item, char *out, u32 size)
{
    struct RogueTownFacts tf;
    struct TownWho w;
    c->town = c->place;
    c->route = RogueGen_RouteNear(c->r, c->place);
    RogueGen_TownFacts(c->r, c->place, &tf);
    c->facts = &tf;
    TownWhoOf(c, &tf, voice, &w);
    w.gift = TownGiftKind(item);
    TownSayPick(c, after ? &sFolkGiftAfter : &sFolkGift, &w, TownNumber(c), TOWN_SPREAD(c, 1), out, size);
    c->facts = NULL;
}

// The specialty counter's greeting (RSHOP_COUNT: the Hardcore berry stall).
void RogueText_ShopGreeting(struct RogueTextCtx *c, u8 shop, char *out, u32 size)
{
    struct TownWho w;
    c->town = c->place;
    TownWhoOf(c, NULL, RVOICE_ANY, &w);
    if (shop > RSHOP_COUNT || sShopBanks[shop] == NULL || !TownSayPick(c, sShopBanks[shop], &w, 0, TOWN_LOCAL, out, size))
        RogueText_Say(out, size, "Welcome!", c);
}

// A POKeMON out for a walk or at home (c->house its house): c->name is its
// species.
void RogueText_MonLine(struct RogueTextCtx *c, bool8 home, u32 localId, char *out, u32 size)
{
    struct TownWho w;
    u32 town = c->place < c->r->nPlaces ? c->r->places[RogueGen_TownNear(c->r, c->place)].number : 0;
    // A route's rest house pet: past every town's, by the route.
    if (c->place < c->r->nPlaces && c->r->places[c->place].kind == RPLACE_ROUTE)
        town = c->r->nTowns + c->r->places[c->place].number;
    TownWhoOf(c, NULL, RVOICE_ANY, &w);
    TownSayPick(c, home ? &sPetHome : &sPetOut, &w, home ? town * ROGUE_MAX_HOUSES + (c->house % ROGUE_MAX_HOUSES) : town * 3 + localId,
                TOWN_SPREAD(c, home ? ROGUE_MAX_HOUSES : 3), out, size);
}

// ---------------------------------------------------------------------------
// Rest houses on the routes: the keeper (who lets the team rest), a
// traveler resting, the sign. Each route's its own (k: the route's number).

TEXT_BANK(sRestSign,
    "REST HOUSE\nTravellers welcome. Take a break!",
    "REST HOUSE\nTired feet welcome. Muddy ones too.",
    "REST HOUSE\nA chair, a cup and a warm fire.",
    "REST HOUSE\nStop a while. The road will wait.",
    "REST HOUSE\nTRAINERS and POK@MON welcome.",
    "REST HOUSE\nRest here, then on your way!",
    "REST HOUSE\nKnock, and the kettle goes on.",
    "REST HOUSE\nHalfway to everywhere.",
    "REST HOUSE\nBoots off, feet up.",
    "REST HOUSE\nThe road can wait a while.",
    "REST HOUSE\nOpen every day, all day.",
);
// The offer when the team has been knocked about...
TEXT_BANK(sRestOfferHurt,
    "You look worn out, and so do your POK@MON. Would you like to rest?",
    "Oh dear, you've been in the wars on {ROUTE}. Sit down! Would you like to rest a while?",
    "Your POK@MON look like they could use a lie down. Will you stay and rest?",
    "Come in, come in. Battles on {ROUTE} again? Rest here as long as you like. Will you?",
    "My, what a state. Nobody leaves my house tired. Will you rest?",
    "You've had a long road from {TOWN}, I can tell. Would you like a rest?",
    "Bumps and bruises, the lot of you! A rest will fix that. Shall I make up the beds?",
    "Travelers always come in looking like that. Will you rest a while?",
    "You're limping, and so is your team. In you come! Will you rest?",
    "Battles on the road? I can tell. Sit by the fire. Would you like to rest?",
);
// ...and when it has not.
TEXT_BANK(sRestOfferFine,
    "You look fit as a fiddle! Still, the road's long. Would you like a rest?",
    "Oh, a visitor! Your team looks well. Will you sit and rest a while all the same?",
    "Nobody passes my door without a rest. Even the healthy ones. Will you stay?",
    "Fresh as a morning on {ROUTE}, you lot! Still, a rest never hurt. Rest a while?",
    "You don't look tired at all. Lucky you! Would you like to rest anyway?",
    "Come in! You're the first TRAINER today who isn't limping. Will you rest?",
    "Your POK@MON look happy! Even so, sit a while. Would you like to rest?",
    "Not a scratch on you! Well done. Still, will you rest a bit?",
    "Come in! It's not every day a TRAINER comes by looking so well. Rest a while?",
    "No bruises? Lucky! The road ahead may be harder. Care for a rest?",
);
TEXT_BANK(sRestDone,
    "There. You all look much better. Take care on the road!",
    "Rested and ready! Go on, {NEXT} isn't getting any closer.",
    "Good as new. Come back whenever {ROUTE} gets the better of you.",
    "There we are. Mind the long grass on the way out!",
    "All better! My POK@MON fluffed the pillows for yours.",
    "Up you get. You snored, by the way. So did your POK@MON.",
    "That's the stuff. A rest and a cup, and the road looks shorter.",
    "Off you go, then! Say hello to {NEXT} for me.",
    "Everyone's bright-eyed again! Go on, the road won't walk itself.",
    "Rested? Good. My POK@MON wanted to battle yours, but they're all asleep now.",
    "There. Nobody leaves my house tired. That's the rule.",
);
TEXT_BANK(sRestDeclined,
    "Suit yourself. The door's always open.",
    "All right! The chair will be here when you need it.",
    "Off you go, then. Don't push them too hard out there.",
    "No? Well, I'll keep the kettle warm for next time.",
    "Brave! Or stubborn. Come back if {ROUTE} changes your mind.",
    "Off you go, then! Wave when you pass the window.",
    "Another time, then. The door doesn't lock.",
    "Fair enough. I'll save the good pillow for next time.",
    "Not tired? Oh, to be young. Off you go!",
    "All right. My POK@MON will be disappointed. It made the beds.",
);
// A traveler resting there.
TEXT_BANK(sRestTraveler,
    "I'm walking from {TOWN} to {NEXT}. This is my third rest today. It's a good house.",
    "I came in for one cup and stayed for four. The keeper won't let you leave thirsty.",
    "My POK@MON is asleep on my BAG. I'll go when it wakes up.",
    "Every rest house on every road is a little different. This one has the best chair.",
    "I got lost in the long grass on {ROUTE}. The keeper found me and fed me supper.",
    "I'm a TRAINER on my day off. Battles are fine, but so is sitting down.",
    "They say you can see {NEXT} from the window on a clear day. It's never clear.",
    "I'm drying my socks by the fire. Don't look. Nobody look.",
    "The keeper has run this house for years. Never once closed the door.",
    "I'm writing a guide to the roads of {REGION}. This house gets five stars. The pie gets six.",
    "I was on my way to the {FESTIVAL}. I'm still on my way. It's a very comfortable way.",
    "Rest here, battle there, rest here again. That's how you cross {REGION}.",
    "My team beat everyone on {ROUTE}. Then I tripped on the step outside. Now I'm resting.",
    "I've been to {TOWN} and back three times this week. My boots have holes in the holes.",
    "Shh. My POK@MON just fell asleep. If it wakes up, it'll want to battle you.",
    "I heard {LEADER} in {GYM_TOWN} is tough. I'm resting before I go and find out how tough.",
);

// The rest house on a route (c->place): the sign (RREST_SIGN), the keeper's
// offer (with the team hurt or not), what the keeper says once rested or
// turned down, and the traveler resting there.
void RogueText_RestHouse(struct RogueTextCtx *c, u8 what, char *out, u32 size)
{
    static const struct RogueTextBank *const sBanks[RREST_COUNT] =
    {
        [RREST_SIGN] = &sRestSign, [RREST_OFFER_HURT] = &sRestOfferHurt, [RREST_OFFER_FINE] = &sRestOfferFine,
        [RREST_DONE] = &sRestDone, [RREST_DECLINED] = &sRestDeclined, [RREST_TRAVELER] = &sRestTraveler,
    };
    struct TownWho w;
    u32 i, rank = 0;
    u8 gym;
    // The region's rest houses in turn, each its own lines.
    for (i = 0; i < c->place && i < c->r->nPlaces; i++)
        rank += c->r->places[i].kind == RPLACE_ROUTE && (c->r->places[i].interiors & (1 << RINT_REST));
    c->route = c->place;
    c->town = RogueGen_TownNear(c->r, c->place);
    gym = RogueText_NextGym(c->r, c->town);
    c->gym = gym < ROGUE_NUM_GYMS ? gym : 0xFF;
    TownWhoOf(c, NULL, RVOICE_ANY, &w);
    out[0] = 0;
    if (what >= RREST_COUNT || !TownSayPick(c, sBanks[what], &w, rank, 18, out, size))
        TownSayPick(c, sBanks[what < RREST_COUNT ? what : 0], &w, 0, TOWN_LOCAL, out, size);
}

// ---------------------------------------------------------------------------
// TV and books

enum { TV_NEWS, TV_ROAD, TV_MON, TV_LEAGUE, TV_FESTIVAL, TV_KITCHEN, TV_DRAMA, TV_WEATHER, TV_COUNT };

// The news: a town's trouble, the k-th town with one from here, then the
// sports if the player has BADGES. FALSE if no town has news.
static bool8 TvNews(struct RogueTextCtx *c, const struct TownWho *w0, u32 k, char *out, u32 size)
{
    const struct RogueRegion *r = c->r;
    struct RogueCast cast;
    struct RogueTownFacts all[ROGUE_MAX_TOWNS];
    u32 t, n;
    RogueGen_Cast(r, &cast);
    // Every town's facts in one pass (one by one would take a frame each).
    RogueGen_AllTownFacts(r, all);
    for (t = 0; t < r->nTowns; t++)
    {
        u32 tn = (TownNumber(c) + k + t) % r->nTowns;
        u8 pl = r->towns[tn];
        struct RogueTownFacts tf = all[tn];
        const struct RogueTextBank *b;
        struct TownWho w = *w0;
        u8 town = c->town;
        const char *line;
        if (r->places[pl].type == RTOWN_SUMMIT)
            continue;
        if (tf.problem == RPROB_LOST && tf.problemAt != cast.explorerDungeon)
            continue;
        b = TownProblemBank(c, &tf, sNewsBanks);
        if (b == NULL)
            continue;
        c->town = pl;
        c->facts = &tf;
        w.near = tf.near;
        line = TownPick(b, c, &w, 0, TOWN_SPREAD(c, 1));
        if (line == NULL)
        {
            c->town = town;
            c->facts = NULL;
            continue;
        }
        n = RogueText_Fill(out, size, "It's the {REGION} NEWS!|", c);
        n += RogueText_Fill(out + n, size - n, line, c);
        c->town = town;
        c->facts = NULL;
        if (c->state && c->state->badges)
        {
            line = TownPick(c->state->badges == 1 ? &sNewsBadge1 : &sNewsBadges, c, w0, k, TOWN_SPREAD(c, 8));
            if (line != NULL)
            {
                n += RogueText_Fill(out + n, size - n, "|", c);
                RogueText_Fill(out + n, size - n, line, c);
            }
        }
        RogueText_Wrap(out, TEXT_LINE);
        return TRUE;
    }
    return FALSE;
}

static bool8 TvShow(struct RogueTextCtx *c, const struct TownWho *w, u8 show, u32 k, char *out, u32 size)
{
    const struct RogueRegion *r = c->r;
    struct RogueCast cast;
    struct RogueTownFacts tf;
    bool8 ok = FALSE;
    u8 town = c->town;
    switch (show)
    {
    case TV_NEWS:
        return TvNews(c, w, k, out, size);
    case TV_ROAD:
    {
        u8 rt;
        RogueGen_Cast(r, &cast);
        rt = cast.reporterTowns[k % 3];
        if (rt >= r->nPlaces)
            return FALSE;
        c->town = rt;
        FactsTown(r, rt, &tf); // not kept: this town's stay
        c->facts = &tf;
        // Once the player has come that far, the interview airs.
        if (c->state && c->state->badges >= r->places[rt].badges && rt != c->place && (c->state->flags & RTSTATE_STARTER))
            ok = TownSayPick(c, &sTvRoadPlayer, w, k, TOWN_SPREAD(c, 8), out, size);
        if (!ok)
            ok = TownSayPick(c, &sTvRoad, w, k, TOWN_SPREAD(c, 8), out, size);
        break;
    }
    case TV_MON:
    case TV_KITCHEN:
    case TV_WEATHER:
        if (!TownHasFacts(c))
            return FALSE;
        RogueGen_TownFacts(r, c->town, &tf);
        c->facts = &tf;
        {
            struct TownWho w2 = *w;
            w2.goods = TownGoodsKind(tf.trade);
            ok = TownSayPick(c, show == TV_MON ? &sTvMon : show == TV_KITCHEN ? &sTvKitchen : &sTvWeather, &w2, k, TOWN_SPREAD(c, 8), out, size);
        }
        break;
    case TV_LEAGUE:
    {
        u8 gym = RogueText_NextGym(r, c->place), g;
        // A gym the player has won, else the next one.
        for (g = 0; c->state && g < ROGUE_NUM_GYMS; g++)
        {
            if ((c->state->wonGyms >> ((g + k) % ROGUE_NUM_GYMS)) & 1)
            {
                c->gym = (g + k) % ROGUE_NUM_GYMS;
                return TownSayPick(c, &sTvLeagueWon, w, k, TOWN_SPREAD(c, 8), out, size);
            }
        }
        if (gym >= ROGUE_NUM_GYMS)
            return FALSE;
        c->gym = gym;
        return TownSayPick(c, &sTvLeague, w, k, TOWN_SPREAD(c, 8), out, size);
    }
    case TV_FESTIVAL:
        return TownSayPick(c, &sTvFestival, w, k, TOWN_SPREAD(c, 8), out, size);
    case TV_DRAMA:
        return TownSayPick(c, &sTvDrama, w, k, TOWN_SPREAD(c, 8), out, size);
    }
    c->town = town;
    c->facts = NULL;
    return ok;
}

// What a TV in a house shows (house its number in the town).
void RogueText_TV(struct RogueTextCtx *c, u8 house, char *out, u32 size)
{
    struct TownWho w;
    u32 badges = c->state ? c->state->badges : 0, k = TownNumber(c) * 5 + house * 3 + badges, i;
    c->town = RogueGen_TownNear(c->r, c->place);
    c->house = house;
    TownWhoOf(c, NULL, RVOICE_ANY, &w);
    for (i = 0; i < TV_COUNT; i++)
        if (TvShow(c, &w, (k + i) % TV_COUNT, k / TV_COUNT + i, out, size))
            return;
    RogueText_Say(out, size, "It's the {REGION} NEWS!|Nothing much happened today. What a nice day.", c);
}

// A book on a shelf: RBOOK_HOME a home's shelf (house its number), RBOOK_PICTURE
// a picture-book shelf, RBOOK_CENTER the POKeMON CENTER's.
void RogueText_Book(struct RogueTextCtx *c, u8 kind, u8 house, char *out, u32 size)
{
    static const struct RogueTextBank *const sBooks[] = { &sBookHome, &sBookPicture, &sBookCenter };
    struct RogueTownFacts tf;
    struct TownWho w;
    u8 gym;
    c->town = RogueGen_TownNear(c->r, c->place);
    c->house = house;
    if (TownHasFacts(c))
    {
        RogueGen_TownFacts(c->r, c->town, &tf);
        c->facts = &tf;
    }
    TownWhoOf(c, c->facts, RVOICE_ANY, &w);
    gym = RogueText_NextGym(c->r, c->place);
    c->gym = gym < ROGUE_NUM_GYMS ? gym : 0xFF;
    if (kind != RBOOK_CENTER && house < ROGUE_MAX_HOUSES)
        w.local = TownFurnitureRank(c->r, c->town, house, kind == RBOOK_PICTURE ? RFURN_PICTURES : RFURN_BOOKS);
    if (!TownSayPick(c, sBooks[kind % 3], &w, kind == RBOOK_CENTER ? TownNumber(c) : TownNumber(c) * 3 + (w.local == 0xFF ? 0 : w.local),
                     TOWN_SPREAD(c, kind == RBOOK_CENTER ? 1 : 3), out, size))
        RogueText_Say(out, size, "It's crammed full of books.", c);
    c->facts = NULL;
}

// What furniture each of the game's home rooms has (src/rogue_town.c sHomes,
// in its order: keep the two in step): RFURN_* bits, from the layouts'
// metatile behaviors.
#define FC RFURN_CABINET
static const u8 sHomeFurniture[] =
{
    FC, RFURN_TV | FC, RFURN_TV | RFURN_BOOKS | RFURN_PICTURES | FC, FC, RFURN_BOOKS | RFURN_PICTURES, FC, 0, RFURN_TV, 0, 0,
    RFURN_TV | RFURN_BOOKS | RFURN_PICTURES, RFURN_BOOKS, FC, RFURN_BOOKS | RFURN_PICTURES, RFURN_PICTURES | FC, FC,
};
#undef FC

// The home room the game builds for a town's house-th house (an index of
// src/rogue_town.c sHomes), and what furniture it has.
u8 RogueText_HomeRoom(const struct RogueRegion *r, u8 place, u8 house)
{
    return (RogueRng_Hash(r->seed, 9690) + r->places[place].number * ROGUE_MAX_HOUSES + house) % LORE_COUNT_OF(sHomeFurniture);
}

u8 RogueText_HomeFurniture(const struct RogueRegion *r, u8 place, u8 house)
{
    return sHomeFurniture[RogueText_HomeRoom(r, place, house)];
}

// TRUE if a house's TV has the news of a legend (LORE_WAY_TV, the
// region's): the first TV of every other town, as Kanto's TVs carry the
// news of the birds; the rest show the region's own shows.
bool8 RogueText_HouseLoreTV(const struct RogueRegion *r, u8 place, u8 house)
{
    u32 i;
    if (RogueGen_LoreAt(r, LORE_WAY_TV, 0) < 0 || (r->places[place].number & 1))
        return FALSE;
    for (i = 0; i < house; i++)
        if (RogueText_HomeFurniture(r, place, i) & RFURN_TV)
            return FALSE;
    return TRUE;
}

// ---------------------------------------------------------------------------
// The dump: every line the towns domain can show in a region, as the game
// takes them (src/rogue_town.c)

#ifdef ROGUE_HOST
// The dungeon rumor the game tells (src/rogue_text_dungeons.h), kept for
// DumpTalk.
static u8 sDumpRumorPlace;
static u32 sDumpRumorRng;
static bool8 DumpGameTopic(u8 topic, u8 place, u32 rng)
{
    (void)topic;
    sDumpRumorPlace = place;
    sDumpRumorRng = rng;
    return TRUE;
}

static struct RogueMap sDumpMap;
static u16 sDumpBlocks[ROGUE_MAX_BLOCKS];

// The pets that live in houses (src/rogue_town.c sPets), for reading.
static const char *const sDumpPets[] =
{
    "Skitty", "Zigzagoon", "Meowth", "Purrloin", "Lillipup", "Poochyena", "Eevee", "Pikachu", "Azurill", "Whismur",
    "Glameow", "Buneary",
};

// One townsperson's or resident's line through RogueText_TownTalk or
// RogueText_HomeTalk: the game's topics show as a note.
static void DumpTalk(struct TextDump *d, struct RogueTextCtx *c, const struct RogueMap *m, const char *who, u8 kind, u32 order,
                     u32 rng, bool8 onRoute, u8 voice, u8 house, u32 localId)
{
    char out[600];
    u8 res;
    c->state = d->state;
    if (kind == RSPK_HOME)
        res = RogueText_HomeTalk(c, m, house, localId, DumpGameTopic, out, sizeof(out));
    else
        res = RogueText_TownTalk(c, order, rng, onRoute, voice, DumpGameTopic, out, sizeof(out));
    if (res == RTALK_GAME && RogueText_DumpRumor(c->r, sDumpRumorPlace, sDumpRumorRng, d->state, out, sizeof(out)))
        TextDump_Line(d, c->place, c->speaker, who, "rumor", 0, out);
    else if (res == RTALK_GAME)
        TextDump_Line(d, c->place, c->speaker, who, "game", 0, "(a dungeon rumor, told by the game)");
    else
        TextDump_Line(d, c->place, c->speaker, who, "towntalk", 0, out);
}

// A town's gift (its gift house's, or folk: its townsperson's), as the
// game draws it with the usual rules.
static u16 DumpGift(const struct RogueRegion *r, u8 place, bool8 folk)
{
    u8 qty;
    return RogueGen_ItemFor(r, folk ? RITEMS_FOLK : RITEMS_GIFT, r->places[place].number, &qty, 0);
}

static const char sVoiceLetter[] = { [RVOICE_ANY] = '-', [RVOICE_KID] = 'k', [RVOICE_ADULT] = 'a', [RVOICE_OLD] = 'o' };

// The street: the sign, the townsfolk in their turns (the game's special
// people as notes), the POKeMON out for a walk.
static void DumpStreet(struct TextDump *d, u8 place, struct RogueMap *m)
{
    const struct RogueRegion *r = d->r;
    const struct RoguePlace *p = &r->places[place];
    struct RogueTextCtx c;
    char out[600], who[64];
    u32 nFolk = 0, order = 0, i, nMons = 0;
    s32 loreOrder;
    bool8 gift = p->kind == RPLACE_TOWN && RogueGen_HasFolkGift(r, p->number);
    for (i = 0; i < m->nObjs; i++)
        nFolk += m->objs[i].kind == ROBJ_VILLAGER;
    if (p->kind == RPLACE_TOWN)
    {
        char line[640];
        RogueText_Init(&c, r, place, RSPK_SIGN);
        c.state = d->state;
        RogueText_TownMotto(&c, out, sizeof(out));
        // ROGUE_TEXT_BEFORE=1: the signs as v0.12.0 had them, for the
        // before numbers.
        if (getenv("ROGUE_TEXT_BEFORE") && RogueGen_LoreAt(r, LORE_WAY_SIGN, place) < 0)
            LoreCopy(out, TownOldSlogan(r, place));
        RogueGen_PlaceName(r, place, line);
        snprintf(line + strlen(line), sizeof(line) - strlen(line), "\n<%s>", out);
        TextDump_Line(d, place, RSPK_SIGN, "town sign", "motto", 0, line);
    }
    loreOrder = (s32)nFolk - 1 - gift;
    if (loreOrder < 0)
        loreOrder = (s32)nFolk - 1;
    for (i = 0; i < m->nObjs; i++)
    {
        const struct RogueObj *o = &m->objs[i];
        u8 voice, reporter;
        if (o->kind == ROBJ_MON && nMons++ < 2)
        {
            RogueText_Init(&c, r, place, RSPK_PET);
            c.state = d->state;
            {
                static char sMonName[16];
                u32 mrng = RogueRng_Hash(r->seed, 8500 + p->number * 16 + o->slot);
                const struct RoguePlace *route = &r->places[RogueGen_RouteNear(r, place)];
                if (RogueText_Runtime(TS_MON, route->area, RogueRng_Range(&mrng, 6), sMonName) == NULL)
                    LoreCopy(sMonName, "Zigzagoon");
                c.name = sMonName;
            }
            RogueText_MonLine(&c, FALSE, i + 1, out, sizeof(out));
            TextDump_Line(d, place, RSPK_PET, "a POKeMON out for a walk", "sPetOut", 0, out);
        }
        if (o->kind != ROBJ_VILLAGER)
            continue;
        voice = TownSlotVoice(r, place, m, o->slot);
        snprintf(who, sizeof(who), "person %u", order + 1);
        RogueText_Init(&c, r, place, RSPK_FOLK);
        c.state = d->state;
        reporter = RogueText_ReporterRole(r, place, nFolk, order);
        if (gift && order == nFolk - 1)
        {
            RogueText_FolkGift(&c, voice, FALSE, DumpGift(r, place, TRUE), out, sizeof(out));
            TextDump_Line(d, place, RSPK_FOLK, "person with a gift", "sFolkGift", 0, out);
            RogueText_Init(&c, r, place, RSPK_FOLK);
            c.state = d->state;
            RogueText_FolkGift(&c, voice, TRUE, DumpGift(r, place, TRUE), out, sizeof(out));
            TextDump_Line(d, place, RSPK_FOLK, "person with a gift, after", "sFolkGiftAfter", 0, out);
            RogueText_Init(&c, r, place, RSPK_FOLK);
            c.state = d->state;
        }
        if (reporter)
        {
            // As met in the road's order (every meeting before had), then
            // as a first meeting and as a talk again (texttest.py takes the
            // one its walk makes).
            s32 meeting = RogueText_ReporterMeeting(r, place);
            u8 before = meeting > 0 ? (1 << meeting) - 1 : 0, here = meeting >= 0 ? 1 << meeting : 0, v;
            static const char *const sHow[3] = { "", " (first meeting)", " (again)" };
            for (v = 0; v < 3; v++)
            {
                if (v > 0 && (reporter != RTEXT_REPORTER || (v == 1 && before == 0)))
                    continue;
                RogueText_Init(&c, r, place, RSPK_REPORTER);
                c.state = d->state;
                RogueText_Reporter(&c, reporter, v == 0 ? before : v == 1 ? 0 : before | here, out, sizeof(out));
                snprintf(who, sizeof(who), "person %u, %s%s", order + 1, reporter == RTEXT_REPORTER ? "the reporter" : "the camera operator",
                         sHow[v]);
                TextDump_Line(d, place, RSPK_REPORTER, who, "reporter", v, out);
            }
        }
        else if (loreOrder >= 2 && order == (u32)loreOrder && RogueGen_LoreAt(r, LORE_WAY_FOLK, place) >= 0)
        {
            RogueGen_LoreText(r, RogueGen_LoreAt(r, LORE_WAY_FOLK, place), LORE_HINT, LORE_WAY_FOLK, out, sizeof(out));
            TextDump_Line(d, place, RSPK_FOLK, who, "lore", 0, out);
        }
        else if (order == 0 && p->type == RTOWN_HAMLET && p->kind == RPLACE_TOWN)
            TextDump_Line(d, place, RSPK_FOLK, who, "game", 0, "(where a rare POKeMON lives, told by the game)");
        else if (RogueGen_HideoutTeam(r, RogueGen_DungeonAt(r, place)) != 0xFF
              && r->dungeons[RogueGen_DungeonAt(r, place)].host == place
              && order < ((d->state && ((d->state->teamsCleared >> RogueGen_HideoutTeam(r, RogueGen_DungeonAt(r, place))) & 1)) ? 1u : 2u))
            TextDump_Line(d, place, RSPK_FOLK, who, "game", 0, "(the villains in town, told by the game)");
        else if (RogueGen_RuinsDungeon(r) >= 0 && RogueGen_TownNear(r, r->dungeons[RogueGen_RuinsDungeon(r)].host) == place
              && p->kind == RPLACE_TOWN && order == (nFolk >= 2 ? 1u : 0u))
            TextDump_Line(d, place, RSPK_FOLK, who, "game", 0, "(the ruins' riddle, told by the game)");
        else if (loreOrder >= 0 && order == (u32)loreOrder && RogueGen_LoreAt(r, LORE_WAY_FOLK, place) >= 0)
        {
            RogueGen_LoreText(r, RogueGen_LoreAt(r, LORE_WAY_FOLK, place), LORE_HINT, LORE_WAY_FOLK, out, sizeof(out));
            TextDump_Line(d, place, RSPK_FOLK, who, "lore", 0, out);
        }
        else
        {
            snprintf(who, sizeof(who), "person %u (%c)", order + 1, sVoiceLetter[voice % 4]);
            DumpTalk(d, &c, m, who, RSPK_FOLK, RogueText_TownTalkOrder(r, place, order),
                     RogueText_TownTalkRng(r, place, i + 1), p->kind != RPLACE_TOWN, voice, 0, i + 1);
        }
        order++;
    }
}

// The town's houses: the plates by the doors, the people, pets, TVs and
// books inside, the gift, the special houses.
static void DumpHouses(struct TextDump *d, u8 place, struct RogueMap *m)
{
    const struct RogueRegion *r = d->r;
    struct RogueTextCtx c;
    char out[1000], who[64];
    u32 i, j;
    for (i = 0; i < m->nSigns; i++)
    {
        s32 house;
        if (m->signs[i].kind != RSIGN_HOUSE && m->signs[i].kind != RSIGN_GIFT)
            continue;
        house = RogueText_SignHouse(m, m->signs[i].x, m->signs[i].y);
        if (house < 0)
            continue;
        RogueText_Init(&c, r, place, RSPK_SIGN);
        c.state = d->state;
        RogueText_HouseSign(&c, house, m->signs[i].kind == RSIGN_GIFT, out, sizeof(out));
        snprintf(who, sizeof(who), "house %d sign", (int)house);
        TextDump_Line(d, place, RSPK_SIGN, who, "sHouseSign", 0, out);
    }
    for (j = 0; j < m->nHouses; j++)
    {
        u32 rng = RogueText_HouseRng(r, place, j, 0);
        RogueText_Init(&c, r, place, RSPK_HOME);
        c.state = d->state;
        c.house = j;
        switch (m->interiors[j])
        {
        case RINT_HOME:
        {
            u8 role = RogueGen_HomeRole(r, place, m->interiors, m->nHouses, j), v1, v2;
            RogueText_HomeVoices(r, place, m, j, &v1, &v2);
            if (role == RHOME_GIFT)
            {
                RogueText_GiftOffer(&c, m, j, DumpGift(r, place, FALSE), out, sizeof(out));
                snprintf(who, sizeof(who), "house %u gift", j);
                TextDump_Line(d, place, RSPK_HOME, who, "sGiftOffer", 0, out);
                RogueText_Init(&c, r, place, RSPK_HOME);
                c.state = d->state;
                RogueText_GiftAfter(&c, m, j, DumpGift(r, place, FALSE), out, sizeof(out));
                snprintf(who, sizeof(who), "house %u gift, after", j);
                TextDump_Line(d, place, RSPK_HOME, who, "sGiftAfter", 0, out);
            }
            if (role == RHOME_CHAT)
            {
                snprintf(who, sizeof(who), "house %u person 1 (%c)", j, sVoiceLetter[v1 % 4]);
                if (RogueGen_LoreAt(r, LORE_WAY_ELDER, place) >= 0 && (s32)j == TownLoreElderHouse(r, place, m))
                {
                    // The tale once the player has the badges its way opens
                    // with, else to come back then (src/rogue_town.c BufferElder).
                    s32 lore = RogueGen_LoreAt(r, LORE_WAY_ELDER, place);
                    if ((d->state ? d->state->badges : 0) >= RogueGen_LegendGateBadges(r, lore))
                        RogueGen_LoreText(r, lore, LORE_STORY, LORE_WAY_ELDER, out, sizeof(out));
                    else
                        RogueGen_LoreText(r, lore, LORE_LATER, LORE_WAYS, out, sizeof(out));
                    TextDump_Line(d, place, RSPK_ELDER, who, "lore", 0, out);
                }
                else
                {
                    RogueText_Init(&c, r, place, (s32)j == RogueText_CastElderHouse(r, place, m) ? RSPK_ELDER : RSPK_HOME);
                    DumpTalk(d, &c, m, who, RSPK_HOME, 0, 0, FALSE, v1, j, 1);
                }
            }
            if (v2 != RVOICE_NONE)
            {
                RogueText_Init(&c, r, place, RSPK_HOME);
                snprintf(who, sizeof(who), "house %u person 2 (%c)", j, sVoiceLetter[v2 % 4]);
                DumpTalk(d, &c, m, who, RSPK_HOME, 0, 0, FALSE, v2, j, 2);
            }
            if ((rng >> 20) % 100 < 45)
            {
                RogueText_Init(&c, r, place, RSPK_PET);
                c.state = d->state;
                c.house = j;
                c.name = sDumpPets[(rng >> 24) % LORE_COUNT_OF(sDumpPets)];
                RogueText_MonLine(&c, TRUE, 3, out, sizeof(out));
                snprintf(who, sizeof(who), "house %u pet", j);
                TextDump_Line(d, place, RSPK_PET, who, "sPetHome", 0, out);
            }
            // Its TV and its shelves, where the room has them.
            if (RogueText_HomeFurniture(r, place, j) & RFURN_TV)
            {
                RogueText_Init(&c, r, place, RSPK_TV);
                c.state = d->state;
                if (RogueText_HouseLoreTV(r, place, j))
                    RogueGen_LoreText(r, RogueGen_LoreAt(r, LORE_WAY_TV, 0), LORE_HINT, LORE_WAY_TV, out, sizeof(out));
                else
                    RogueText_TV(&c, j, out, sizeof(out));
                snprintf(who, sizeof(who), "house %u TV", j);
                TextDump_Line(d, place, RSPK_TV, who, "tv", 0, out);
            }
            if (RogueText_HomeFurniture(r, place, j) & RFURN_CABINET)
            {
                RogueText_Init(&c, r, place, RSPK_SIGN);
                c.state = d->state;
                RogueText_Cabinet(&c, j, out, sizeof(out));
                snprintf(who, sizeof(who), "house %u cabinet", j);
                TextDump_Line(d, place, RSPK_SIGN, who, "sHomeCabinet", 0, out);
            }
            for (i = 0; i < 2; i++)
            {
                if (!(RogueText_HomeFurniture(r, place, j) & (i ? RFURN_PICTURES : RFURN_BOOKS)))
                    continue;
                RogueText_Init(&c, r, place, RSPK_BOOK);
                c.state = d->state;
                RogueText_Book(&c, i ? RBOOK_PICTURE : RBOOK_HOME, j, out, sizeof(out));
                snprintf(who, sizeof(who), i ? "house %u picture books" : "house %u bookshelf", j);
                TextDump_Line(d, place, RSPK_BOOK, who, i ? "sBookPicture" : "sBookHome", 0, out);
            }
            break;
        }
        case RINT_LAB:
            RogueText_HouseRole(&c, RTEXT_ROLE_LAB, j, 2, out, sizeof(out));
            TextDump_Line(d, place, RSPK_AIDE, "lab aide", "sLabAide", 0, out);
            break;
        case RINT_SCHOOL:
            RogueText_HouseRole(&c, RTEXT_ROLE_TEACHER, j, 1, out, sizeof(out));
            TextDump_Line(d, place, RSPK_HOME, "school teacher", "teacher", 0, out);
            for (i = 2; i <= 3; i++)
            {
                snprintf(who, sizeof(who), "school student %u", i - 1);
                RogueText_HouseRole(&c, RTEXT_ROLE_STUDENT, j, i, out, sizeof(out));
                TextDump_Line(d, place, RSPK_HOME, who, "sStudent", 0, out);
            }
            break;
        case RINT_FAN_CLUB:
            for (i = 1; i <= 2; i++)
            {
                snprintf(who, sizeof(who), "fan club %u", i);
                RogueText_HouseRole(&c, RTEXT_ROLE_FAN, j, i, out, sizeof(out));
                TextDump_Line(d, place, RSPK_HOME, who, "sFanClub", 0, out);
            }
            break;
        }
    }
    // The POKeMON CENTER's visitors, where there is room (src/rogue_town.c
    // BuildRoomWithWarps: after the game's own people, so a CENTER full of
    // them may have one or none).
    for (i = 0; i < 2; i++)
    {
        RogueText_Init(&c, r, place, RSPK_FOLK);
        c.state = d->state;
        RogueText_CenterFolk(&c, m, i, out, sizeof(out));
        snprintf(who, sizeof(who), "POKeMON CENTER visitor %u", i + 1);
        TextDump_Line(d, place, RSPK_FOLK, who, "sCenterFolk", 0, out);
    }
    // The POKeMON CENTER's shelf.
    if (RogueGen_LoreAt(r, LORE_WAY_BOOK, place) < 0)
    {
        RogueText_Init(&c, r, place, RSPK_BOOK);
        c.state = d->state;
        RogueText_Book(&c, RBOOK_CENTER, 0, out, sizeof(out));
        TextDump_Line(d, place, RSPK_BOOK, "POKeMON CENTER bookshelf", "sBookCenter", 0, out);
    }
    if (RogueGen_SpecialtyShop(r, place))
    {
        RogueText_Init(&c, r, place, RSPK_CLERK);
        c.state = d->state;
        RogueText_ShopGreeting(&c, RogueGen_SpecialtyShop(r, place), out, sizeof(out));
        TextDump_Line(d, place, RSPK_CLERK, "specialty clerk", "shop", 0, out);
    }
}

static void TextDump_Towns(struct TextDump *d)
{
    const struct RogueRegion *r = d->r;
    u32 place;
    for (place = 0; place < r->nPlaces; place++)
    {
        const struct RoguePlace *p = &r->places[place];
        struct RogueMap *m = &sDumpMap;
        if (p->kind == RPLACE_CAVE)
            continue;
        RogueGen_GenerateMap(r, place, m, sDumpBlocks);
        DumpStreet(d, place, m);
        if (p->kind == RPLACE_TOWN)
            DumpHouses(d, place, m);
        // A route's rest house (src/rogue_town.c).
        if (p->kind == RPLACE_ROUTE && (p->interiors & (1 << RINT_REST)))
        {
            static const char *const sRestWho[RREST_COUNT] = { "rest house sign", "rest house keeper", "rest house keeper (team well)",
                                                               "rest house keeper, rested", "rest house keeper, turned down",
                                                               "rest house traveler" };
            u32 what;
            char out[400];
            for (what = 0; what < RREST_COUNT; what++)
            {
                struct RogueTextCtx c;
                RogueText_Init(&c, r, place, what == RREST_SIGN ? RSPK_SIGN : RSPK_HOME);
                c.state = d->state;
                RogueText_RestHouse(&c, what, out, sizeof(out));
                TextDump_Line(d, place, what == RREST_SIGN ? RSPK_SIGN : RSPK_HOME, sRestWho[what], "rest", what, out);
            }
        }
    }
}
#endif
