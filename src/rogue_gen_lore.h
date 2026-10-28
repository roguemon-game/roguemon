// Legend lore and the ice caves' names. Included at the end of rogue_gen.c,
// so the host tools (gen_host, check_seeds.py) see the same words the game
// shows. Nothing here changes the region: it only reads the plan.
//
// Every legend has a short reason for being where it is, told in the world
// the way the real games tell theirs: the Sealed Chamber's braille, the
// Sinnoh lake myths in Canalave's library, the Tanoby tales, Ecruteak's
// burned tower told by an old man, Kanto's TV news of the birds, a gym
// leader handing over the Eon Ticket. Each legend has two pieces, told in
// different ways: a hint (a rumor, the town sign, a news report, a shrine by
// the lair) and the story (old words in its chamber, a book in the POKéMON
// CENTER, an old person at home, PROF. BIRCH's aide, the gym leader with
// the key, the villain leader after the battle). A region's legends are told
// in different ways where they can be. A set's stories share its myth (the
// three birds, the burned tower), told a little differently by each.
//
// Text is ASCII here, as the town names are: '@' is the e of POKéMON, '<'
// and '>' are the curly quotes, '|' starts a new box. RogueGen_LoreText
// fills the codes in and breaks the lines (of LORE_LINE characters, the
// box scrolling on past two); the game converts the result as it is.

#define LORE_LINE 30

static char *NameNumber(char *dest, u32 n); // src/rogue_gen_names.h


static const char *const sLoreNames[RLEG_COUNT] =
{
    [RLEG_NONE] = "a legend",
    [RLEG_REGIROCK] = "REGIROCK", [RLEG_REGICE] = "REGICE", [RLEG_REGISTEEL] = "REGISTEEL", [RLEG_REGIGIGAS] = "REGIGIGAS",
    [RLEG_ARTICUNO] = "ARTICUNO", [RLEG_ZAPDOS] = "ZAPDOS", [RLEG_MOLTRES] = "MOLTRES",
    [RLEG_RAIKOU] = "RAIKOU", [RLEG_ENTEI] = "ENTEI", [RLEG_SUICUNE] = "SUICUNE",
    [RLEG_UXIE] = "UXIE", [RLEG_MESPRIT] = "MESPRIT", [RLEG_AZELF] = "AZELF",
    [RLEG_COBALION] = "COBALION", [RLEG_TERRAKION] = "TERRAKION", [RLEG_VIRIZION] = "VIRIZION",
    [RLEG_TORNADUS] = "TORNADUS", [RLEG_THUNDURUS] = "THUNDURUS", [RLEG_LANDORUS] = "LANDORUS",
    [RLEG_LATIAS] = "LATIAS", [RLEG_LATIOS] = "LATIOS",
    [RLEG_HEATRAN] = "HEATRAN", [RLEG_CRESSELIA] = "CRESSELIA",
    [RLEG_MEWTWO] = "MEWTWO", [RLEG_LUGIA] = "LUGIA", [RLEG_HO_OH] = "HO-OH",
    [RLEG_KYOGRE] = "KYOGRE", [RLEG_GROUDON] = "GROUDON", [RLEG_RAYQUAZA] = "RAYQUAZA",
};

static const u8 sLegendElem[RLEG_COUNT] =
{
    [RLEG_REGIROCK] = LELEM_STONE, [RLEG_REGICE] = LELEM_ICE, [RLEG_REGISTEEL] = LELEM_STONE, [RLEG_REGIGIGAS] = LELEM_STONE,
    [RLEG_ARTICUNO] = LELEM_ICE, [RLEG_ZAPDOS] = LELEM_STORM, [RLEG_MOLTRES] = LELEM_FIRE,
    [RLEG_RAIKOU] = LELEM_STORM, [RLEG_ENTEI] = LELEM_FIRE, [RLEG_SUICUNE] = LELEM_WATER,
    [RLEG_UXIE] = LELEM_MIND, [RLEG_MESPRIT] = LELEM_MIND, [RLEG_AZELF] = LELEM_MIND,
    [RLEG_COBALION] = LELEM_STONE, [RLEG_TERRAKION] = LELEM_STONE, [RLEG_VIRIZION] = LELEM_GRASS,
    [RLEG_TORNADUS] = LELEM_SKY, [RLEG_THUNDURUS] = LELEM_STORM, [RLEG_LANDORUS] = LELEM_STONE,
    [RLEG_LATIAS] = LELEM_SKY, [RLEG_LATIOS] = LELEM_SKY,
    [RLEG_HEATRAN] = LELEM_FIRE, [RLEG_CRESSELIA] = LELEM_MIND,
    [RLEG_MEWTWO] = LELEM_MIND, [RLEG_LUGIA] = LELEM_WATER, [RLEG_HO_OH] = LELEM_SKY,
    [RLEG_KYOGRE] = LELEM_WATER, [RLEG_GROUDON] = LELEM_FIRE, [RLEG_RAYQUAZA] = LELEM_SKY,
};

// What the villains want it for (Team Aqua to fill the seas, Team Magma to
// raise the land): its power, in a word.
static const char *const sLorePowers[RLEG_COUNT] =
{
    [RLEG_NONE] = "power",
    [RLEG_REGIROCK] = "strength", [RLEG_REGICE] = "cold", [RLEG_REGISTEEL] = "strength", [RLEG_REGIGIGAS] = "strength",
    [RLEG_ARTICUNO] = "blizzards", [RLEG_ZAPDOS] = "storms", [RLEG_MOLTRES] = "fire",
    [RLEG_RAIKOU] = "thunder", [RLEG_ENTEI] = "fire", [RLEG_SUICUNE] = "water",
    [RLEG_UXIE] = "power", [RLEG_MESPRIT] = "power", [RLEG_AZELF] = "power",
    [RLEG_COBALION] = "strength", [RLEG_TERRAKION] = "strength", [RLEG_VIRIZION] = "strength",
    [RLEG_TORNADUS] = "winds", [RLEG_THUNDURUS] = "storms", [RLEG_LANDORUS] = "power",
    [RLEG_LATIAS] = "power", [RLEG_LATIOS] = "power",
    [RLEG_HEATRAN] = "fire", [RLEG_CRESSELIA] = "power",
    [RLEG_MEWTWO] = "power", [RLEG_LUGIA] = "storms", [RLEG_HO_OH] = "fire",
    [RLEG_KYOGRE] = "rain", [RLEG_GROUDON] = "sunlight", [RLEG_RAYQUAZA] = "power",
};

// ---------------------------------------------------------------------------
// Names

// The town a place is named after: the one nearest it by the roads, as
// Petalburg Woods is named after Petalburg. Routes are crossed by their
// links, caves by their ways in and out, so a side cave at the end of a side
// route finds the town that route hangs off (not the first town).
u8 RogueGen_TownNear(const struct RogueRegion *r, u8 place)
{
    u8 queue[ROGUE_MAX_PLACES];
    u64 seen;
    u32 head = 0, tail = 0, i;

    if (place >= r->nPlaces)
        return r->start;
    if (r->places[place].kind == RPLACE_TOWN)
        return place;
    seen = 1ull << place;
    queue[tail++] = place;
    while (head < tail)
    {
        const struct RoguePlace *p = &r->places[queue[head++]];
        u8 next[ROGUE_MAX_EXITS + 3];
        u32 n = 0;
        for (i = 0; i < p->nLinks; i++)
            next[n++] = p->links[i].target;
        next[n++] = p->caveIn;
        next[n++] = p->caveOut;
        next[n++] = p->mouth;
        for (i = 0; i < n; i++)
        {
            u32 t = next[i];
            if (t >= r->nPlaces || (seen >> t) & 1)
                continue;
            seen |= 1ull << t;
            if (r->places[t].kind == RPLACE_TOWN && r->places[t].type != RTOWN_SUMMIT)
                return t;
            queue[tail++] = t;
        }
    }
    return r->start;
}

static char *LoreCopy(char *dest, const char *src)
{
    while (*src)
        *dest++ = *src++;
    *dest = 0;
    return dest;
}

// A town's name without TOWN or CITY: PINEWICK.
static char *LoreTownStem(char *dest, const struct RogueRegion *r, u8 place)
{
    const char *name = (const char *)r->townNames[r->places[place].number % ROGUE_MAX_TOWNS];
    char *end = LoreCopy(dest, name), *p;
    for (p = end; p > dest; p--)
    {
        if (p[-1] == ' ')
        {
            p[-1] = 0;
            return p - 1;
        }
    }
    return end;
}

// The volcano the region map draws (RogueGen_VolcanoCell), named as Hoenn's
// and Kanto's are.
static const char *const sVolcanoNames[] = { "MT. CINDER", "MT. ASHEN", "MT. SCORIA", "MT. FLARE", "MT. KILN", "MT. SMOLDER" };

char *RogueGen_VolcanoName(const struct RogueRegion *r, char *dest)
{
    return LoreCopy(dest, sVolcanoNames[RogueRng_Hash(r->seed, 8500) % (sizeof(sVolcanoNames) / sizeof(sVolcanoNames[0]))]);
}

// The ice caves' names, as Seafoam Islands, Ice Path, Shoal Cave and the
// Snowpoint Temple are named: for the cold, the frost and the snow, or a
// town's ICE CAVE. Each fits the floor popup with its floor (" B1F").
static const char *const sIceNames[] =
{
    "FROSTFALL GROTTO", "RIME HOLLOW", "FROZEN DEEP", "GLACIER CAVERN", "HOARFROST CAVE", "WINTER'S MAW",
    "SNOWVEIL CAVE", "ICICLE CAVERN", "SLEET GROTTO", "SHIVER HOLLOW", "POLAR CAVERN", "ICEFALL CAVE",
    "FROST HOLLOW", "BOREAL GROTTO", "WHITEOUT CAVE", "FROSTBITE CAVE", "GLACIAL DEEP", "SNOWDRIFT CAVE",
    "NORTHWIND CAVE", "ICEBOUND CAVE", "HAILSTONE CAVE", "PALE HOLLOW", "FROZEN FALLS", "SILVERFROST CAVE",
    "DEEPFREEZE CAVE", "STILLFROST CAVE", "COLDHEART CAVE", "ICE MAZE", "CRYSTAL CAVERN", "WINTER HOLLOW",
};
// Where the cold meets the sea, as Seafoam and Shoal Cave do.
static const char *const sIceSeaNames[] = { "SEAFROST GROTTO", "FROZEN SHOAL", "TIDEFROST CAVE", "BRINE ICE CAVE", "SPRAYFROST CAVE" };
#define LORE_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

static bool8 LoreBySea(const struct RogueRegion *r, u8 place)
{
    const struct RoguePlace *p = &r->places[place];
    u32 i;
    if (p->theme == RTHEME_HARBOR || p->theme == RTHEME_ISLAND || p->theme == RTHEME_BEACH || p->theme == RTHEME_COVE
     || p->seaSide != RDIR_NONE)
        return TRUE;
    if (p->kind == RPLACE_ROUTE && (p->type == RROUTE_COAST || p->type == RROUTE_SEA))
        return TRUE;
    if (p->kind == RPLACE_CAVE)
    {
        for (i = 0; i < 2; i++)
        {
            u8 t = i ? p->caveOut : p->caveIn;
            if (t < r->nPlaces && r->places[t].kind == RPLACE_ROUTE
             && (r->places[t].type == RROUTE_COAST || r->places[t].type == RROUTE_SEA))
                return TRUE;
        }
    }
    return FALSE;
}

// One name a region per ice cave, the same on the map, the sign, the
// floors and in what people say: seeded, and never two alike in a region.
// A town with a short name may lend it ("PINEBURY ICE CAVE"), now and then.
char *RogueGen_IceCaveName(const struct RogueRegion *r, u8 dungeon, char *dest)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon % ROGUE_MAX_DUNGEONS];
    u32 rng = RogueRng_Hash(r->seed, 8450 + dungeon), i, nth = 0;
    u8 town = RogueGen_TownNear(r, d->host);
    char *start = dest;

    for (i = 0; i < dungeon && i < r->nDungeons; i++)
        nth += r->dungeons[i].kind == RDUN_ICE;
    if (nth == 0 && (rng & 3) == 0)
    {
        char *end = LoreTownStem(dest, r, town);
        if (end - start + 9 <= 16)
            return LoreCopy(end, " ICE CAVE");
    }
    rng >>= 2;
    if (LoreBySea(r, d->host) && (rng & 1))
        return LoreCopy(start, sIceSeaNames[((rng >> 1) + nth) % LORE_COUNT_OF(sIceSeaNames)]);
    return LoreCopy(start, sIceNames[((rng >> 1) + nth * 7) % LORE_COUNT_OF(sIceNames)]);
}

// ---------------------------------------------------------------------------
// Where a legend lives

// The kind of home, for the words that name it.
enum
{
    LHOME_VOLCANO_K, // the volcano's floors
    LHOME_ICE_K,     // an ice cave
    LHOME_WOODS_K,   // a grove
    LHOME_CAVE_K,    // below a cave
    LHOME_ISLE_K,    // a lair's isle or glade under the sky
    LHOME_PEAK_K,    // a lair's peak or pillar top
    LHOME_SEA_K,     // a lair's sea cave
    LHOME_DEPTHS_K,  // a lair's hot depths (Terra Cave)
    LHOME_CHAMBER_K, // a lair's old chamber (the Regis' rooms)
    LHOME_DEEP_K,    // a lair's deep cave
    LHOME_KINDS,
};

struct LoreHome
{
    u8 kind;  // LHOME_*_K
    u8 elem;  // LELEM_* of the home
    u8 town;  // the town it is told of in
    u8 sea, lake, hot; // the land round it
};

static void LegendHomeOf(const struct RogueRegion *r, u8 dungeon, struct LoreHome *h)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon];
    u8 room = RogueGen_FloorRoom(r, dungeon, d->nFloors);
    u8 style = room != 0xFF ? sRogueRooms[room].style : ROOM_STYLE_COUNT;
    const struct RoguePlace *host = &r->places[d->host];
    u8 land = DungeonLand(r, d);

    h->town = d->kind == RDUN_LAIR ? d->host : RogueGen_TownNear(r, d->host);
    h->sea = (land & LLAND_SEA) != 0 || LoreBySea(r, d->host);
    h->lake = (land & LLAND_LAKE) != 0;
    h->hot = host->theme == RTHEME_ASH || host->theme == RTHEME_SPRING
          || r->places[h->town].theme == RTHEME_ASH || r->places[h->town].theme == RTHEME_SPRING;
    switch (d->kind)
    {
    case RDUN_VOLCANO:
        h->kind = LHOME_VOLCANO_K, h->elem = LELEM_FIRE;
        return;
    case RDUN_ICE:
        h->kind = LHOME_ICE_K, h->elem = LELEM_ICE;
        return;
    case RDUN_FOREST:
        h->kind = LHOME_WOODS_K, h->elem = LELEM_GRASS;
        return;
    case RDUN_LAIR:
        break;
    default:
        h->kind = LHOME_CAVE_K;
        h->elem = h->hot ? LELEM_FIRE : (h->sea || h->lake) ? LELEM_WATER : LELEM_STONE;
        return;
    }
    switch (style)
    {
    case ROOM_STYLE_ISLE:
    case ROOM_STYLE_GLADE:
        h->kind = LHOME_ISLE_K, h->elem = style == ROOM_STYLE_GLADE ? LELEM_GRASS : (h->sea ? LELEM_WATER : LELEM_SKY);
        break;
    case ROOM_STYLE_PEAK:
    case ROOM_STYLE_PILLAR:
        h->kind = LHOME_PEAK_K, h->elem = LELEM_SKY;
        break;
    case ROOM_STYLE_MARINE:
    case ROOM_STYLE_DEPTHS:
    case ROOM_STYLE_ISLAND_CAVE:
        h->kind = LHOME_SEA_K, h->elem = LELEM_WATER;
        break;
    case ROOM_STYLE_TERRA:
        h->kind = LHOME_DEPTHS_K, h->elem = LELEM_FIRE;
        break;
    case ROOM_STYLE_RUINS:
    case ROOM_STYLE_TOMB:
    case ROOM_STYLE_CHAMBER:
        h->kind = LHOME_CHAMBER_K, h->elem = LELEM_STONE;
        break;
    default:
        h->kind = LHOME_DEEP_K, h->elem = h->hot ? LELEM_FIRE : LELEM_STONE;
        break;
    }
}

// The home, as a story names it ($H), and in short ($W).
static char *LoreHomeWords(char *dest, const struct RogueRegion *r, u8 dungeon, const struct LoreHome *h, bool8 brief)
{
    char stem[ROGUE_NAME_LEN + 1];
    LoreTownStem(stem, r, h->town);
    switch (h->kind)
    {
    case LHOME_VOLCANO_K:
        if (!brief)
            dest = LoreCopy(dest, "the heart of ");
        return RogueGen_VolcanoName(r, dest);
    case LHOME_ICE_K:
        if (!brief)
            dest = LoreCopy(dest, "the ice of ");
        return RogueGen_IceCaveName(r, dungeon, dest);
    case LHOME_WOODS_K:
        if (!brief)
            dest = LoreCopy(dest, "the depths of ");
        return LoreCopy(LoreCopy(dest, stem), " WOODS");
    case LHOME_CAVE_K:
        dest = LoreCopy(dest, h->hot ? "the hot caves near " : h->sea ? "the sea caves near " : h->lake ? "the caves by the lake at " : "the caves near ");
        return LoreCopy(dest, stem);
    case LHOME_ISLE_K:
        dest = LoreCopy(dest, brief ? "the cliffs of " : h->elem == LELEM_GRASS ? "a quiet glade past " : "a lonely isle past ");
        return LoreCopy(dest, stem);
    case LHOME_PEAK_K:
        return LoreCopy(LoreCopy(dest, "the peak above "), stem);
    case LHOME_SEA_K:
        return LoreCopy(LoreCopy(dest, brief ? "the sea caves of " : "the sea caves under "), stem);
    case LHOME_DEPTHS_K:
        return LoreCopy(LoreCopy(dest, brief ? "the rocks of " : "the hot depths under "), stem);
    case LHOME_CHAMBER_K:
        return LoreCopy(LoreCopy(dest, brief ? "the old rocks of " : "an old chamber under "), stem);
    default:
        return LoreCopy(LoreCopy(dest, brief ? "the rocks of " : h->hot ? "the hot caves under " : "the deep caves under "), stem);
    }
}

static bool8 LoreFits(u8 legend, u8 home)
{
    u8 e = sLegendElem[legend];
    if (e == home)
        return TRUE;
    switch (e)
    {
    case LELEM_STORM: return home == LELEM_SKY;
    case LELEM_ICE:   return home == LELEM_WATER; // Articuno in Seafoam
    case LELEM_MIND:  return home == LELEM_WATER || (home == LELEM_STONE && legend == RLEG_MEWTWO)
                          || (home == LELEM_SKY && legend == RLEG_CRESSELIA);
    case LELEM_STONE: return home == LELEM_FIRE && legend == RLEG_LANDORUS;
    case LELEM_SKY:   return (home == LELEM_GRASS || home == LELEM_WATER) && (legend == RLEG_LATIAS || legend == RLEG_LATIOS);
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// The words

// A set's myth, told a little differently by each of its members, in the
// order they come in the set ($R the region).
static const char *const sLoreMyths[][3] =
{
    { "Long ago the people of $R sealed away three giants: REGIROCK, REGICE and REGISTEEL.",
      "Three giants were sealed in $R long ago, one of rock, one of ice and one of steel.",
      "The old people of $R feared three giants of rock, ice and steel, and sealed each one apart." },
    { "Three birds once ruled the sky of $R: ARTICUNO, ZAPDOS and MOLTRES.",
      "ARTICUNO brings the snow, ZAPDOS brings the storms, and MOLTRES brings the heat.",
      "The three birds of $R quarreled long ago and flew far apart." },
    { "Three POK@MON died when a tower burned. HO-OH brought them back as RAIKOU, ENTEI and SUICUNE.",
      "RAIKOU, ENTEI and SUICUNE rose from the ashes of a burned tower, long ago.",
      "The three beasts of the burned tower run where the wind takes them." },
    { "UXIE, MESPRIT and AZELF gave the people knowledge, feeling and will.",
      "Three beings came out of the lakes of $R long ago, to watch over its people.",
      "Look into the eyes of UXIE, MESPRIT or AZELF, and you will forget it all, they say." },
    { "COBALION, TERRAKION and VIRIZION fought to save POK@MON from a war long ago.",
      "Three swords of justice watch over the POK@MON of $R.",
      "When people harm POK@MON, the three swords of justice come." },
    { "TORNADUS and THUNDURUS raged over $R until LANDORUS calmed them.",
      "Two storms once tore at $R, wind and thunder. A third POK@MON made peace.",
      "TORNADUS brings the wind, THUNDURUS the lightning, and LANDORUS the harvest." },
};

// The myth of a legend outside the trios.
static const char *const sLoreSingleMyths[RLEG_COUNT] =
{
    [RLEG_REGIGIGAS] = "REGIGIGAS shaped REGIROCK, REGICE and REGISTEEL out of rock, ice and steel.",
    [RLEG_LATIAS]    = "LATIAS and her brother LATIOS fly as one, unseen by people.",
    [RLEG_LATIOS]    = "LATIOS and his sister LATIAS can share what they see, far apart.",
    [RLEG_HEATRAN]   = "HEATRAN was born in fire, deep under the mountains of $R.",
    [RLEG_CRESSELIA] = "CRESSELIA brings sweet dreams under the crescent moon.",
    [RLEG_MEWTWO]    = "MEWTWO was made by people, not born. It has hidden from them ever since.",
    [RLEG_LUGIA]     = "LUGIA calms the storms at sea, but its wings can raise them too.",
    [RLEG_HO_OH]     = "HO-OH flies across the sky on wings of seven colors.",
    [RLEG_KYOGRE]    = "KYOGRE filled the seas, and fought GROUDON for the world.",
    [RLEG_GROUDON]   = "GROUDON raised the land, and fought KYOGRE for the world.",
    [RLEG_RAYQUAZA]  = "RAYQUAZA lives high in the sky. It ended the fight of land and sea.",
};

// The myths again, naming no other legend, for a region without all of
// them (a set's myth told where the region has only some of the set,
// KYOGRE without GROUDON): a line must name only legends the region has.
static const char *const sLoreMythsUnnamed[] =
{
    "Long ago the people of $R sealed away three giants of rock, ice and steel.",
    "Three birds once ruled the sky of $R: one of ice, one of storms and one of fire.",
    "Three POK@MON were lost when a tower burned. They rose again as the three beasts.",
    "Three beings came out of the lakes of $R long ago, to give its people knowledge, feeling and will.",
    "Three swords of justice fought to save POK@MON from a war long ago.",
    "Two storms once tore at $R, wind and thunder, until a third POK@MON calmed them.",
};
static const char *const sLoreSingleUnnamed[RLEG_COUNT] =
{
    [RLEG_REGIGIGAS] = "REGIGIGAS shaped three giants out of rock, ice and steel, long ago.",
    [RLEG_LATIAS]    = "LATIAS flies unseen by people, and is never far from her brother.",
    [RLEG_LATIOS]    = "LATIOS can share what he sees with his sister, however far apart.",
    [RLEG_KYOGRE]    = "KYOGRE filled the seas, and fought a great POK@MON of the land for the world.",
    [RLEG_GROUDON]   = "GROUDON raised the land, and fought a great POK@MON of the sea for the world.",
};

// TRUE if every legend a text names lives in the region.
static bool8 LoreNamesOnlyRegion(const struct RogueRegion *r, const char *t)
{
    u32 l, i, k;
    for (l = 1; l < RLEG_COUNT; l++)
    {
        const char *name = sLoreNames[l];
        bool8 named = FALSE, have = FALSE;
        for (i = 0; t[i] && !named; i++)
        {
            for (k = 0; name[k] && t[i + k] == name[k]; k++)
                ;
            named = name[k] == 0 && (i == 0 || t[i - 1] < 'A' || t[i - 1] > 'Z')
                 && (t[i + k] < 'A' || t[i + k] > 'Z');
        }
        if (!named)
            continue;
        for (i = 0; i < r->nDungeons; i++)
            have |= r->dungeons[i].boss == RBOSS_LEGEND && r->dungeons[i].legend == l;
        if (!have)
            return FALSE;
    }
    return TRUE;
}

// Why a legend is where it fits ($H its home, $S the town).
static const char *const sLoreFitReasons[RLEG_COUNT] =
{
    [RLEG_REGIROCK]  = "REGIROCK was sealed $O $H. The rock there keeps it whole.",
    [RLEG_REGICE]    = "REGICE was sealed $O $H, where the cold never ends.",
    [RLEG_REGISTEEL] = "REGISTEEL was sealed $O $H. No pick can scratch its body.",
    [RLEG_REGIGIGAS] = "REGIGIGAS waits $O $H for the three it made.",
    [RLEG_ARTICUNO]  = "ARTICUNO made its nest $O $H. Its wings keep the cold there.",
    [RLEG_ZAPDOS]    = "ZAPDOS rests $O $H, where it can call the lightning down.",
    [RLEG_MOLTRES]   = "MOLTRES dove into $H to renew its flames.",
    [RLEG_RAIKOU]    = "RAIKOU came to $H with the thunder, and stayed.",
    [RLEG_ENTEI]     = "When the mountain last erupted, ENTEI came to $H. It sleeps there still.",
    [RLEG_SUICUNE]   = "SUICUNE came to $H to purify its waters.",
    [RLEG_UXIE]      = "UXIE sleeps $O $H. It wakes for no one.",
    [RLEG_MESPRIT]   = "MESPRIT wanders, but it always comes home to $H.",
    [RLEG_AZELF]     = "AZELF guards $H and the water beside it.",
    [RLEG_COBALION]  = "COBALION watches over $H from the rocks.",
    [RLEG_TERRAKION] = "TERRAKION once broke a castle wall to save POK@MON. It rests $O $H now.",
    [RLEG_VIRIZION]  = "VIRIZION moves through $H like the wind through the grass.",
    [RLEG_TORNADUS]  = "TORNADUS rests $O $H between its storms.",
    [RLEG_THUNDURUS] = "THUNDURUS hides $O $H between its storms.",
    [RLEG_LANDORUS]  = "LANDORUS made the fields around $S rich. It sleeps $O $H.",
    [RLEG_LATIAS]    = "LATIAS hides $O $H, where no one can see her.",
    [RLEG_LATIOS]    = "LATIOS keeps watch over $R from $H.",
    [RLEG_HEATRAN]   = "HEATRAN was born $O $H, and still crawls along its rocks.",
    [RLEG_CRESSELIA] = "CRESSELIA rests $O $H by day, and flies under the moon.",
    [RLEG_MEWTWO]    = "MEWTWO fled the people who made it and hid $O $H.",
    [RLEG_LUGIA]     = "LUGIA dives deep under $H, to keep its power from the world.",
    [RLEG_HO_OH]     = "HO-OH rests on $H, where the rainbow ends.",
    [RLEG_KYOGRE]    = "KYOGRE sleeps $O $H, dreaming of the deep sea.",
    [RLEG_GROUDON]   = "GROUDON sleeps $O $H, wrapped in its own heat.",
    [RLEG_RAYQUAZA]  = "RAYQUAZA comes down to $H to rest from the sky.",
};

// Why a legend is where it does not fit, by the home's kind and then the
// legend's: [home][0] for a legend of the sky or a storm, [1] fire, [2] the
// rest.
static const char *const sLoreClashReasons[LELEM_COUNT][3] =
{
    [LELEM_FIRE] = {
        "Long ago $L fled a great storm. It found warmth $O $H, and sleeps there still.",
        "",
        "$L came to $H to calm the fire below. It has slept there ever since.",
    },
    [LELEM_ICE] = {
        "$L fell from the sky in a blizzard. It sleeps $O $H still.",
        "Long ago $L burned too hot. It sleeps $O $H to cool its flames.",
        "$L came to $H long ago. The ice closed over it as it slept.",
    },
    [LELEM_WATER] = {
        "$L chased a storm out to sea. It rests now $O $H.",
        "$L lost a great battle. It hides $O $H, where nobody would look for fire.",
        "Fishers found $L asleep $O $H. Seasons come and go, but it never wakes.",
    },
    [LELEM_SKY] = {
        "",
        "$L was carried up to $H by a great wind, they say.",
        "$L climbed to $H to be nearer the sky, they say.",
    },
    [LELEM_STONE] = {
        "$L was caught in a rockfall $O $H, and has slept there ever since.",
        "$L sleeps $O $H, deep in the rock where its fire cannot spread.",
        "Long ago the old folk sealed $L $O $H, to keep its $P from the land.",
    },
    [LELEM_GRASS] = {
        "$L came to $H hurt, and the woods healed it. It never left.",
        "The woods of $H burned once. $L came, and they grew back thicker.",
        "$L came to $H hurt, and the woods healed it. It never left.",
    },
};

// TRUE if a legend's own tale says people shut it away or made it (the
// Regis' myth, MEWTWO's, a home whose reason is that the old folk sealed
// it there): the region's founding tale must not make it the settlers'
// friend (seed 909: "REGICE sheltered the first settlers" beside "the old
// people feared three giants... and sealed each one apart").
bool8 RogueGen_LegendFeared(const struct RogueRegion *r, u8 dungeon)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon % ROGUE_MAX_DUNGEONS];
    struct LoreHome h;
    const char *reason;
    u8 e, col;
    if (d->boss != RBOSS_LEGEND || d->legend >= RLEG_COUNT)
        return FALSE;
    if (d->legend == RLEG_REGIROCK || d->legend == RLEG_REGICE || d->legend == RLEG_REGISTEEL || d->legend == RLEG_MEWTWO)
        return TRUE;
    LegendHomeOf(r, dungeon % ROGUE_MAX_DUNGEONS, &h);
    if (LoreFits(d->legend, h.elem))
        return FALSE;
    e = sLegendElem[d->legend];
    col = (e == LELEM_STORM || e == LELEM_SKY) ? 0 : e == LELEM_FIRE ? 1 : 2;
    reason = sLoreClashReasons[h.elem][col];
    if (reason[0] == 0)
        reason = sLoreClashReasons[h.elem][2];
    return strstr(reason, "sealed") != NULL;
}

// The hint: what people notice where the legend sleeps ($W its home in
// short), with its name.
static const char *const sLoreHints[LELEM_COUNT] =
{
    [LELEM_FIRE]  = "Even in winter it is warm around $W. They say $L sleeps there.",
    [LELEM_ICE]   = "A cold wind blows out of $W, even in summer. Could it be $L?",
    [LELEM_WATER] = "The water by $W is always clear. They say $L keeps it so.",
    [LELEM_STORM] = "On clear nights, lightning flashes over $W. It must be $L!",
    [LELEM_SKY]   = "Strange winds sing around $W at dusk. They say it's $L.",
    [LELEM_STONE] = "The ground hums under $W at night. They say $L stirs below.",
    [LELEM_GRASS] = "Flowers bloom by $W all year round. They say it's $L.",
    [LELEM_MIND]  = "Folks who visit $W forget why they went. It's $L, they say.",
};

static const char *const sLoreMottos[] = { "Where $L sleeps.", "Watched over by $L.", "Home of the tale of $L." };

// The ways the lore is told (LORE_WAY_*), and their frames: what comes
// before the piece. The museum's is LORE_WAY_MUSEUM's in a town with a
// museum; elsewhere the plaque's (sLorePlaqueFrame).
static const char *const sLoreFrames[LORE_WAYS] =
{
    [LORE_WAY_CARVING]  = "",
    [LORE_WAY_BOOK]     = "It's an old book: LEGENDS OF $R.|",
    [LORE_WAY_ELDER]    = "The old tale of $S? Sit down, dear.|",
    [LORE_WAY_LAB]      = "PROF. BIRCH is reading about $L!|",
    [LORE_WAY_LEADER]   = "There is an old tale about it.|",
    [LORE_WAY_VILLAIN]  = "Hmph. You don't even know the old tale, do you?|",
    [LORE_WAY_FOLK]     = "",
    [LORE_WAY_SIGN]     = "",
    [LORE_WAY_TV]       = "There's a news report on TV.|",
    [LORE_WAY_SHRINE]   = "Under the name, someone has added in an old hand:|",
    [LORE_WAY_DIARY]    = "A page torn from an old diary is folded under the ball.|",
    [LORE_WAY_GRAVE]    = "This grave is older than the rest. Its words are worn, but clear.|HERE RESTS $K, WHO KEPT THE TALE OF $L.|",
    [LORE_WAY_MUSEUM]   = "EXHIBIT: THE LEGEND OF $L.|",
    [LORE_WAY_RHYME]    = "",
    [LORE_WAY_PAINTING] = "",
};
static const char sLorePlaqueFrame[] = "A brass plaque hangs below the picture: THE LEGEND OF $L, AS TOLD IN $S.|";

// Who kept a legend's tale and lies in a tower's grave ($K).
static const char *const sLoreKeepers[] =
{
    "TOBIAS", "MAREN", "EDWIN", "HOLLIS", "PRUDENCE", "ANSEL", "MIRIAM", "CORIN", "BEATRIX", "OSRIC", "LENORE", "AMBROSE",
};

// How a diary page begins ($D a day of the journey, $S the town it was
// told in): the diarist heard the tale on the road.
static const char *const sLoreDiaryOpens[] =
{
    "<Day $D. An old fisher in $S told me a tale tonight. I wrote it down before I forgot.|",
    "<Day $D. Rain again, so I stayed in $S and listened to the old folk talk.|",
    "<Day $D. My boots are worn through. But in $S, I finally heard the tale!|",
    "<Day $D. The children of $S sing about it. Their grandparents told me the rest.|",
    "<Day $D. I traded a POTION in $S for a story. A good trade, I think.|",
    "<Day $D. Nobody in $S would talk about it, until the fire was lit.|",
};
static const char *const sLoreDiaryCloses[] =
{
    " I mean to see it for myself.>|The rest of the page is torn away.",
    " Tomorrow I go and look.>|The rest of the page is too faded to read.",
    " Could it be true?>|The page ends there.",
};

// A child's skipping rhyme, by the legend's nature: it names the legend and
// where it sleeps ($W), as children's rhymes keep old tales.
static const char *const sLoreRhymes[LELEM_COUNT] =
{
    [LELEM_FIRE]  = "<One, two, warm and bright, $L sleeps in $W tonight! Three, four, tiptoe slow, or $L wakes below!>",
    [LELEM_ICE]   = "<Snowflake, snowflake, who's that there? $L's breath in the winter air! Up from $W, cold and white, button your coat and hold on tight!>",
    [LELEM_WATER] = "<Splash and spin, splash and spin, $L keeps the water clean! Out by $W, deep and blue, $L is watching you!>",
    [LELEM_STORM] = "<Flash, crash, count to three, $L's over $W, see? Count to four, count to five, hide your kite, it's still alive!>",
    [LELEM_SKY]   = "<Wind in the willow, wind in the wheat, $L flies by on silent feet! Round $W, high and free, $L, $L, wait for me!>",
    [LELEM_STONE] = "<Stomp, stomp, the ground goes hum, under $W, here they come! Old $L, sleep, sleep sound, we'll keep quiet all year round!>",
    [LELEM_GRASS] = "<Daisy, clover, bluebell, rose, by $W the green wind blows! $L, $L, in the trees, bring us flowers, if you please!>",
    [LELEM_MIND]  = "<Close your eyes and count to ten, $L by $W again! What did I want? I forgot! $L knows, and I do not!>",
};
static const char *const sLoreRhymeFrames[] =
{
    "Wanna hear our skipping rhyme? Everybody in $S knows it!|",
    "My big sister taught me this one. It's a secret rhyme. Well, everyone knows it.|",
    "We sing this when we skip rope outside! Listen!|",
};

// A painting of the legend's home, with something more in it, by the
// legend's nature.
static const char *const sLorePaintings[LELEM_COUNT] =
{
    [LELEM_FIRE]  = "It's a painting of $W at night, glowing red.|Deep in the glow, someone painted two burning eyes. A card reads: $L, ASLEEP.",
    [LELEM_ICE]   = "It's a painting of $W in a snowstorm.|In the storm, faint as frost, are great wings. A card reads: $L.",
    [LELEM_WATER] = "It's a painting of the water by $W, very still and very clear.|Under the surface is a pale shape. A card reads: $L, KEEPER OF THE WATERS.",
    [LELEM_STORM] = "It's a painting of $W under a black sky.|In the lightning, someone painted a shape with jagged wings. A card reads: $L.",
    [LELEM_SKY]   = "It's a painting of $W at dusk, all gold and pink.|High in the clouds is a shape like a great bird. A card reads: $L PASSING BY.",
    [LELEM_STONE] = "It's a painting of $W. The rocks look almost alive.|In the stone is a great sleeping shape. A card reads: $L DREAMS.",
    [LELEM_GRASS] = "It's a painting of $W in spring, full of flowers.|Between the trees stands a slender shape. A card reads: $L WALKS HERE.",
    [LELEM_MIND]  = "It's a painting of $W, misty and soft.|There's something in the mist. Or was there? A card reads: $L.",
};

// ---------------------------------------------------------------------------
// How each legend is told

// Which piece each way can tell: the story (the myth and the reason) or
// the hint (what people notice, with its name). The lab tells either; the
// leader and the villain leader are given their legends, not picked.
static const u8 sLoreStoryWays[] = { LORE_WAY_CARVING, LORE_WAY_BOOK, LORE_WAY_ELDER, LORE_WAY_LAB, LORE_WAY_DIARY,
                                     LORE_WAY_GRAVE, LORE_WAY_MUSEUM };
static const u8 sLoreHintWays[] = { LORE_WAY_LAB, LORE_WAY_FOLK, LORE_WAY_SIGN, LORE_WAY_TV, LORE_WAY_SHRINE, LORE_WAY_RHYME,
                                    LORE_WAY_PAINTING };

// The town whose signature is the museum (src/rogue_gen_signatures.h) shows
// its legend's tale among the exhibits; elsewhere it is a plaque in the
// POKéMON CENTER.
bool8 RogueGen_TownHasMuseum(const struct RogueRegion *r, u8 place)
{
    return RogueGen_TownSignature(r, place) == RSIG_MUSEUM;
}

// A tower's floor whose graves keep the old keeper's: the first floor that
// has graves one can stand in front of (Mt. Pyre's, metatile 0x2EE), or 0.
#define LORE_GRAVE_METATILE 0x2EE
u8 RogueGen_GraveFloor(const struct RogueRegion *r, u8 dungeon)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon % ROGUE_MAX_DUNGEONS];
    u8 f;
    if (d->kind != RDUN_TOWER)
        return 0;
    for (f = 1; f <= d->nFloors; f++)
    {
        u8 id = RogueGen_FloorRoom(r, dungeon, f);
        const struct RogueRoom *room;
        u32 x, y;
        if (id == 0xFF)
            continue;
        room = &sRogueRooms[id];
        for (y = 0; y + 1 < room->h; y++)
            for (x = 0; x < room->w; x++)
                if ((room->blocks[y * room->w + x] & 0x3FF) == LORE_GRAVE_METATILE
                 && ((room->blocks[(y + 1) * room->w + x] >> 10) & 3) == 0)
                    return f;
    }
    return 0;
}

// TRUE if a dungeon's prize ball can hold a diary page: a prize that is no
// legend's key, out of the hideout.
static bool8 LoreDiaryCan(const struct RogueRegion *r, u8 j)
{
    struct RogueDungeonExtras e;
    u32 i;
    if (r->dungeons[j].kind == RDUN_HIDEOUT)
        return FALSE;
    RogueGen_DungeonExtras(r, j, &e);
    if (e.prize == 0xFF)
        return FALSE;
    for (i = 0; i < r->nDungeons; i++)
        if (r->dungeons[i].boss == RBOSS_LEGEND && ROGUE_GATE_KIND(r->dungeons[i].gate) == RGATE_FOUND
         && ROGUE_GATE_SRC(r->dungeons[i].gate) == j)
            return FALSE;
    return TRUE;
}

// The dungeon nearest the legend's town (by the grid) that can hold its
// page or grave (can: a bit a dungeon) and holds no other's yet, or 0xFF.
static u8 LoreHolderFor(const struct RogueRegion *r, u8 legendDungeon, u8 town, u32 can, u32 used)
{
    u32 j, best = 0xFFFF;
    u8 pick = 0xFF;
    for (j = 0; j < r->nDungeons; j++)
    {
        const struct RoguePlace *h = &r->places[r->dungeons[j].host];
        u32 cost;
        if (j == legendDungeon || ((used >> j) & 1) || !((can >> j) & 1))
            continue;
        cost = abs(h->gridX - r->places[town].gridX) + abs(h->gridY - r->places[town].gridY);
        if (cost < best)
            best = cost, pick = j;
    }
    return pick;
}

// TRUE if a way may tell this legend: one of a kind in a town (the lab and
// the TV one in a region; a page or a grave one a dungeon), a shrine only
// by a lair's door, the lab not for a legend of the first town (its
// townsfolk tell it there), townsfolk only where they have the time. The
// POKéMON CENTER's picture shows one thing: the plaque of a town with no
// museum, or the painting of a town with no painting at home (never both).
static bool8 LoreWayFree(const struct RogueRegion *r, const u8 *ways, const u8 *towns, u32 i, u8 town, u8 w, u8 story)
{
    u32 j;
    if (w == LORE_WAY_SHRINE && r->dungeons[i].kind != RDUN_LAIR)
        return FALSE;
    if (w == LORE_WAY_LAB && town == r->start)
        return FALSE;
    // A hamlet has few people, and the hideout's town's talk of the team.
    if (w == LORE_WAY_FOLK && (r->places[town].type == RTOWN_HAMLET
     || (RogueGen_HideoutTeam(r, RogueGen_DungeonAt(r, town)) != 0xFF && r->dungeons[RogueGen_DungeonAt(r, town)].host == town)))
        return FALSE;
    if (w == LORE_WAY_PAINTING && story == LORE_WAY_MUSEUM && !RogueGen_TownHasMuseum(r, town))
        return FALSE;
    for (j = 0; j < i; j++)
    {
        u8 a, b;
        if (ways[j] == 0xFF || towns[j] != town)
            continue;
        a = ways[j] & 15, b = ways[j] >> 4;
        if (!RogueGen_TownHasMuseum(r, town)
         && ((w == LORE_WAY_MUSEUM && (a == LORE_WAY_PAINTING || b == LORE_WAY_PAINTING))
          || (w == LORE_WAY_PAINTING && (a == LORE_WAY_MUSEUM || b == LORE_WAY_MUSEUM))))
            return FALSE;
    }
    for (j = 0; j < i; j++)
        if (ways[j] != 0xFF && ((ways[j] & 15) == w || (ways[j] >> 4) == w)
         && (towns[j] == town || w == LORE_WAY_LAB || w == LORE_WAY_TV)
         && w != LORE_WAY_DIARY && w != LORE_WAY_GRAVE)
            return FALSE;
    return TRUE;
}

// The ways each legend of the region is told, worked out for all of them
// at once (in the order of the dungeons), so they spread over the ways: a
// way not used yet in the region comes first. ways[d] holds the story's way
// in its low nibble and the hint's in the high one, 0xFF for a dungeon
// without a legend; holders[d] (if given) the dungeon holding its diary
// page or grave, else 0xFF.
static void LoreWaysWork(const struct RogueRegion *r, u8 *ways, u8 *holders);

// The ways are asked for often (every sign, townsperson and thing read),
// and take a while to work out: the last region's are kept (a few bytes of
// IWRAM), keyed by its seed and a hash of its dungeons, so a region still
// being planned never hands its ways to the finished one.
static u32 LoreRegionKey(const struct RogueRegion *r)
{
    u32 h = r->seed * 31 + r->nDungeons * 7 + r->nPlaces, i;
    for (i = 0; i < r->nDungeons; i++)
        h = h * 131 + (r->dungeons[i].kind | (r->dungeons[i].host << 4) | (r->dungeons[i].legend << 12)
                       | ((u32)r->dungeons[i].gate << 18) | ((u32)r->dungeons[i].boss << 26) | ((u32)r->dungeons[i].nFloors << 29));
    return h;
}

static void LoreWays(const struct RogueRegion *r, u8 *ways, u8 *holders)
{
    static u32 sKey;
    static bool8 sValid;
    static u8 sWays[ROGUE_MAX_DUNGEONS], sHeld[ROGUE_MAX_DUNGEONS];
    u32 key = LoreRegionKey(r), i;
    if (!sValid || key != sKey)
    {
        LoreWaysWork(r, sWays, sHeld);
        sKey = key;
        sValid = TRUE;
    }
    for (i = 0; i < ROGUE_MAX_DUNGEONS; i++)
    {
        ways[i] = sWays[i];
        if (holders != NULL)
            holders[i] = sHeld[i];
    }
}

// TRUE if the region surely has a home TV that shows the legend news
// (src/rogue_text_towns.h RogueText_HouseLoreTV: the first TV at home in
// every other town). Worked out without drawing the towns: a town with no
// special interior to hold (no lab, no school or fan club..., no landmark
// you enter) has only homes, and always a first house, so its first home is
// house 0, drawn with RogueText_HomeRoom's room.
static bool8 LoreHasNewsTV(const struct RogueRegion *r)
{
    u32 i;
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        u8 sig;
        if (p->kind != RPLACE_TOWN || p->type == RTOWN_SUMMIT || p->type == RTOWN_START || (p->number & 1) || p->interiors)
            continue;
        sig = RogueGen_TownSignature(r, i);
        if (sig == RSIG_SPACE || sig == RSIG_WEATHER || sig == RSIG_MUSEUM || sig == RSIG_FOSSIL || sig == RSIG_DOJO)
            continue;
        if (RogueText_HomeFurniture(r, i, 0) & RFURN_TV)
            return TRUE;
    }
    return FALSE;
}

static void LoreWaysWork(const struct RogueRegion *r, u8 *ways, u8 *holders)
{
    bool8 newsTV = LoreHasNewsTV(r);
    u32 used = 0, pages = 0, graves = 0, canPage = 0, canGrave = 0, i, j, k;
    u8 towns[ROGUE_MAX_DUNGEONS], held[ROGUE_MAX_DUNGEONS];
    for (i = 0; i < ROGUE_MAX_DUNGEONS; i++)
        ways[i] = held[i] = 0xFF;
    // Which dungeons can hold a page or a grave, worked out once (only if
    // the region has a legend to tell).
    if (RogueGen_LegendDungeon(r) >= 0)
        for (i = 0; i < r->nDungeons; i++)
        {
            if (LoreDiaryCan(r, i))
                canPage |= 1u << i;
            if (r->dungeons[i].kind == RDUN_TOWER && RogueGen_GraveFloor(r, i))
                canGrave |= 1u << i;
        }
    for (i = 0; i < r->nDungeons; i++)
    {
        const struct RogueDungeon *d = &r->dungeons[i];
        struct LoreHome h;
        u8 kind = ROGUE_GATE_KIND(d->gate), story = 0xFF, hint = LORE_WAY_SIGN, w, page, grave;
        u32 rng = RogueRng_Hash(r->seed, 8460 + i), score, best;
        if (d->boss != RBOSS_LEGEND)
            continue;
        LegendHomeOf(r, i, &h);
        towns[i] = h.town;
        page = LoreHolderFor(r, i, h.town, canPage, pages);
        grave = LoreHolderFor(r, i, h.town, canGrave, graves);
        // The leader who hands over the key tells it, as the one who gives
        // the Eon Ticket would; the villain leader says why he wanted it.
        if (kind == RGATE_GIFT)
        {
            // One key a leader (the Eon Ticket opens both Latis' ways): the
            // second legend is told another way.
            story = LORE_WAY_LEADER;
            for (j = 0; j < i; j++)
                if (ways[j] != 0xFF && (ways[j] & 15) == LORE_WAY_LEADER && ROGUE_GATE_SRC(r->dungeons[j].gate) == ROGUE_GATE_SRC(d->gate))
                    story = 0xFF;
        }
        else if (kind == RGATE_VILLAIN && RogueGen_TeamLeaderDungeon(r, ROGUE_GATE_SRC(d->gate)) == (s32)i)
            story = LORE_WAY_VILLAIN;
        if (story == 0xFF)
        {
            story = LORE_WAY_CARVING;
            best = 0;
            for (k = 0; k < LORE_COUNT_OF(sLoreStoryWays); k++)
            {
                w = sLoreStoryWays[k];
                if ((w == LORE_WAY_DIARY && page == 0xFF) || (w == LORE_WAY_GRAVE && grave == 0xFF)
                 || !LoreWayFree(r, ways, towns, i, h.town, w, 0xFF))
                    continue;
                score = (((used >> w) & 1) ? 1 : 9) + ((rng >> (w * 2)) & 3);
                // A legend of the museum's town is told there, as a rule.
                if (w == LORE_WAY_MUSEUM && RogueGen_TownHasMuseum(r, h.town))
                    score += 6;
                if (score > best)
                    story = w, best = score;
            }
        }
        // The hint: a rumor, the town's sign, the TV, a shrine by a lair's
        // door, a rhyme, a painting, or the lab once those are taken.
        best = 0;
        for (k = 0; k < LORE_COUNT_OF(sLoreHintWays); k++)
        {
            w = sLoreHintWays[k];
            if (w == story || (w == LORE_WAY_TV && !newsTV) || !LoreWayFree(r, ways, towns, i, h.town, w, story))
                continue;
            score = (((used >> w) & 1) ? 1 : 9) + ((rng >> ((w * 2 + 1) & 31)) & 3) + (w == LORE_WAY_SHRINE ? 3 : 0);
            if (w == LORE_WAY_LAB)
                score = score > 6 ? score - 6 : 1;
            if (score > best)
                hint = w, best = score;
        }
        used |= (1u << story) | (1u << hint);
        ways[i] = story | (hint << 4);
        if (story == LORE_WAY_DIARY)
            held[i] = page, pages |= 1u << page;
        else if (story == LORE_WAY_GRAVE)
            held[i] = grave, graves |= 1u << grave;
    }
    if (holders != NULL)
        for (i = 0; i < ROGUE_MAX_DUNGEONS; i++)
            holders[i] = held[i];
}

void RogueGen_LegendLore(const struct RogueRegion *r, u8 dungeon, struct RogueLore *lore)
{
    u8 ways[ROGUE_MAX_DUNGEONS];
    struct LoreHome h;
    dungeon %= ROGUE_MAX_DUNGEONS;
    LoreWays(r, ways, NULL);
    lore->story = ways[dungeon] == 0xFF ? 0xFF : ways[dungeon] & 15;
    lore->hint = ways[dungeon] == 0xFF ? 0xFF : ways[dungeon] >> 4;
    if (ways[dungeon] == 0xFF)
    {
        lore->town = 0xFF;
        lore->elem = LELEM_STONE;
        return;
    }
    LegendHomeOf(r, dungeon, &h);
    lore->town = h.town;
    lore->home = h.kind;
    lore->homeElem = h.elem;
    lore->elem = sLegendElem[r->dungeons[dungeon].legend];
    lore->fits = LoreFits(r->dungeons[dungeon].legend, h.elem);
}

u8 RogueGen_LoreHolder(const struct RogueRegion *r, u8 dungeon)
{
    u8 ways[ROGUE_MAX_DUNGEONS], holders[ROGUE_MAX_DUNGEONS];
    LoreWays(r, ways, holders);
    return holders[dungeon % ROGUE_MAX_DUNGEONS];
}

s32 RogueGen_LoreAt(const struct RogueRegion *r, u8 way, u8 place)
{
    u8 ways[ROGUE_MAX_DUNGEONS], holders[ROGUE_MAX_DUNGEONS];
    u32 i;
    LoreWays(r, ways, holders);
    for (i = 0; i < r->nDungeons; i++)
    {
        struct LoreHome h;
        if (ways[i] == 0xFF || ((ways[i] & 15) != way && (ways[i] >> 4) != way))
            continue;
        if (way == LORE_WAY_LAB || way == LORE_WAY_TV)
            return i;
        if (way == LORE_WAY_LEADER)
        {
            if (ROGUE_GATE_SRC(r->dungeons[i].gate) == place)
                return i;
            continue;
        }
        if (way == LORE_WAY_DIARY || way == LORE_WAY_GRAVE)
        {
            if (holders[i] == place)
                return i;
            continue;
        }
        // The villain leader's and the carving's: place is the dungeon
        // (0xFF: the first).
        if (way == LORE_WAY_VILLAIN || way == LORE_WAY_CARVING)
        {
            if (place == 0xFF || place == i || way == LORE_WAY_CARVING)
                return i;
            continue;
        }
        LegendHomeOf(r, i, &h);
        if (h.town == place)
            return i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Paintings at home

// The rooms of src/rogue_town.c's sHomes with a framed painting on the
// wall (HOUSE1's and the diving treasure hunters' house's, the Building
// tileset's 0x207 over 0x20F; the others hang windows): keep in step with
// that list.
#define LORE_PAINTING_ROOMS ((1u << 0) | (1u << 15))
#define LORE_NUM_HOME_ROOMS 16

u8 RogueGen_HomeRoom(const struct RogueRegion *r, u8 place, u8 house)
{
    return (RogueRng_Hash(r->seed, 9690) + r->places[place % ROGUE_MAX_PLACES].number * ROGUE_MAX_HOUSES + house) % LORE_NUM_HOME_ROOMS;
}

bool8 RogueGen_HomeRoomHasPainting(u8 room)
{
    return room < LORE_NUM_HOME_ROOMS && ((LORE_PAINTING_ROOMS >> room) & 1);
}

s32 RogueGen_PaintingHouse(const struct RogueRegion *r, u8 place, const u8 *interiors, u8 nHouses)
{
    u32 i;
    for (i = 0; i < nHouses && i < ROGUE_MAX_HOUSES; i++)
        if (interiors[i] == RINT_HOME && RogueGen_HomeRoomHasPainting(RogueGen_HomeRoom(r, place, i)))
            return i;
    return -1;
}

// The legends of a legend's set in the region before it, for its myth.
static u8 LoreSetIndex(const struct RogueRegion *r, u8 dungeon, u8 *set)
{
    u8 legend = r->dungeons[dungeon].legend, n = 0;
    u32 i;
    *set = 0xFF;
    for (i = 0; i < LEGEND_COUNT_OF(sLegendTrios); i++)
        if (sLegendTrios[i][0] == legend || sLegendTrios[i][1] == legend || sLegendTrios[i][2] == legend)
            *set = i;
    if (*set == 0xFF)
        return 0;
    for (i = 0; i < dungeon; i++)
    {
        u8 l = r->dungeons[i].legend;
        if (r->dungeons[i].boss == RBOSS_LEGEND
         && (sLegendTrios[*set][0] == l || sLegendTrios[*set][1] == l || sLegendTrios[*set][2] == l))
            n++;
    }
    return n < 3 ? n : 2;
}

// Fills a template's codes in.
static char *LoreFill(char *out, char *end, const char *t, const struct RogueRegion *r, u8 dungeon, const struct LoreHome *h)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon];
    char word[48];
    for (; *t && out < end; t++)
    {
        const char *s = word;
        if (*t != '$')
        {
            *out++ = *t;
            continue;
        }
        word[0] = 0;
        switch (*++t)
        {
        case 'L': s = sLoreNames[d->legend < RLEG_COUNT ? d->legend : 0]; break;
        case 'P': s = sLorePowers[d->legend < RLEG_COUNT ? d->legend : 0]; break;
        case 'R': if (!RogueGen_KeptRegionName(r->seed, word)) RogueGen_RegionName(r->seed, word); break;
        case 'T': s = (const char *)r->townNames[r->places[h->town].number % ROGUE_MAX_TOWNS]; break;
        case 'S': LoreTownStem(word, r, h->town); break;
        case 'H': LoreHomeWords(word, r, dungeon, h, FALSE); break;
        case 'W': LoreHomeWords(word, r, dungeon, h, TRUE); break;
        case 'O': s = (h->kind == LHOME_ISLE_K || h->kind == LHOME_PEAK_K) ? "on" : "in"; break;
        case 'K': s = sLoreKeepers[RogueRng_Hash(r->seed, 8480 + dungeon) % LORE_COUNT_OF(sLoreKeepers)]; break;
        case 'D': NameNumber(word, 3 + RogueRng_Hash(r->seed, 8481 + dungeon) % 58); break;
        case 'N':
            // The badges its way opens with: "one BADGE", "6 BADGES".
            if (RogueGen_LegendGateBadges(r, dungeon) <= 1)
                s = "one BADGE";
            else
                word[0] = '0' + RogueGen_LegendGateBadges(r, dungeon) % 10, LoreCopy(word + 1, " BADGES");
            break;
        default: t--; s = "$"; break;
        }
        while (*s && out < end)
            *out++ = *s++;
    }
    *out = 0;
    return out;
}

// Breaks the text into lines (spaces become a line break, then scrolls of
// the box, '\v'); '|' starts a new box. Returns FALSE if a word is too long
// for a line.
static bool8 LoreWrap(char *s)
{
    u32 col = 0, line = 0;
    char *space = NULL;
    bool8 ok = TRUE;
    for (; *s; s++)
    {
        if (*s == '|')
        {
            *s = '\f';
            col = line = 0;
            space = NULL;
            continue;
        }
        if (*s == ' ')
            space = s;
        if (++col > LORE_LINE)
        {
            if (space == NULL)
            {
                ok = FALSE;
                continue;
            }
            // The first break is a new line; the rest scroll the box on,
            // as Emerald's long lines do, so no box holds a lone word.
            *space = line == 0 ? '\n' : '\v';
            line = 1;
            col = s - space;
            space = NULL;
        }
    }
    return ok;
}

u32 RogueGen_LoreText(const struct RogueRegion *r, u8 dungeon, u8 piece, u8 way, char *out, u32 size)
{
    const struct RogueDungeon *d;
    struct LoreHome h;
    char *p = out, *end = out + size - 1;
    u32 rng;
    u8 set, n;
    dungeon %= ROGUE_MAX_DUNGEONS;
    d = &r->dungeons[dungeon];
    out[0] = 0;
    if (d->boss != RBOSS_LEGEND || d->legend >= RLEG_COUNT)
        return 0;
    LegendHomeOf(r, dungeon, &h);
    rng = RogueRng_Hash(r->seed, 8470 + dungeon);
    if (way == LORE_WAY_MUSEUM && !RogueGen_TownHasMuseum(r, h.town))
        p = LoreFill(p, end, sLorePlaqueFrame, r, dungeon, &h);
    else if (way < LORE_WAYS)
        p = LoreFill(p, end, sLoreFrames[way], r, dungeon, &h);
    switch (piece)
    {
    case LORE_STORY:
    {
        const char *reason;
        n = LoreSetIndex(r, dungeon, &set);
        if (way == LORE_WAY_DIARY)
            p = LoreFill(p, end, sLoreDiaryOpens[(rng >> 8) % LORE_COUNT_OF(sLoreDiaryOpens)], r, dungeon, &h);
        if (way == LORE_WAY_CARVING || way == LORE_WAY_GRAVE)
            p = LoreFill(p, end, "<", r, dungeon, &h);
        {
            // A myth that names a legend the region lacks is told without
            // the names.
            char *start = p;
            p = LoreFill(p, end, set != 0xFF ? sLoreMyths[set][n] : sLoreSingleMyths[d->legend], r, dungeon, &h);
            if (!LoreNamesOnlyRegion(r, start))
            {
                const char *alt = set != 0xFF ? sLoreMythsUnnamed[set] : sLoreSingleUnnamed[d->legend];
                if (alt != NULL)
                    p = LoreFill(start, end, alt, r, dungeon, &h);
            }
        }
        p = LoreFill(p, end, "|", r, dungeon, &h);
        if (LoreFits(d->legend, h.elem))
            reason = sLoreFitReasons[d->legend];
        else
        {
            u8 e = sLegendElem[d->legend];
            u8 col = (e == LELEM_STORM || e == LELEM_SKY) ? 0 : e == LELEM_FIRE ? 1 : 2;
            reason = sLoreClashReasons[h.elem][col];
            if (reason[0] == 0)
                reason = sLoreClashReasons[h.elem][2];
        }
        p = LoreFill(p, end, reason, r, dungeon, &h);
        if (way == LORE_WAY_CARVING || way == LORE_WAY_GRAVE)
            p = LoreFill(p, end, ">", r, dungeon, &h);
        else if (way == LORE_WAY_DIARY)
            p = LoreFill(p, end, sLoreDiaryCloses[(rng >> 12) % LORE_COUNT_OF(sLoreDiaryCloses)], r, dungeon, &h);
        break;
    }
    case LORE_HINT:
    {
        const char *hint = sLoreHints[sLegendElem[d->legend]];
        if (d->legend == RLEG_SUICUNE)
            hint = "The water by $W is always clear. They say SUICUNE purifies it.";
        else if (d->legend == RLEG_CRESSELIA)
            hint = "Sleepers near $W have the sweetest dreams, thanks to $L.";
        else if (d->legend == RLEG_MEWTWO)
            hint = "Scientists came to $S looking for $L. They left in a hurry.";
        if (way == LORE_WAY_RHYME)
        {
            p = LoreFill(p, end, sLoreRhymeFrames[(rng >> 8) % LORE_COUNT_OF(sLoreRhymeFrames)], r, dungeon, &h);
            hint = sLoreRhymes[sLegendElem[d->legend]];
        }
        else if (way == LORE_WAY_PAINTING)
            hint = sLorePaintings[sLegendElem[d->legend]];
        if (way == LORE_WAY_FOLK)
        {
            // Sailors by the sea, fishers by a lake or a river.
            const struct RoguePlace *t = &r->places[h.town];
            bool8 lake = FALSE;
            u32 i;
            for (i = 0; i < t->nLinks; i++)
                if (t->links[i].target < r->nPlaces && IsWaterRoutePlace(&r->places[t->links[i].target]))
                    lake = TRUE;
            p = LoreFill(p, end, LoreBySea(r, h.town) ? "The sailors here tell a tale.|"
                               : lake ? "The fishers here tell a tale.|" : "My granny tells an old tale.|", r, dungeon, &h);
        }
        p = LoreFill(p, end, hint, r, dungeon, &h);
        break;
    }
    case LORE_MOTTO:
        p = LoreFill(p, end, sLoreMottos[rng % LORE_COUNT_OF(sLoreMottos)], r, dungeon, &h);
        return p - out; // one line, never broken
    case LORE_LATER:
        // The old one's tale waits until the player could go and see.
        p = LoreFill(p, end, "The tale of $L? You're too young for it, dear. Come back with $N.", r, dungeon, &h);
        break;
    case LORE_VILLAIN_WHY:
    {
        // In the voice of the team whose leader waits before it.
        static const char *const why[ROGUE_VILLAIN_IDS] = {
            "We needed $L's $P to fill the seas!|", "We needed $L's $P to raise the land!|",
            "$L would have made TEAM ROCKET a fortune!|",
        };
        u8 t = ROGUE_GATE_KIND(r->dungeons[dungeon].gate) == RGATE_VILLAIN ? ROGUE_GATE_SRC(r->dungeons[dungeon].gate) : 0;
        p = LoreFill(p, end, why[RogueGen_TeamId(r, t) % ROGUE_VILLAIN_IDS], r, dungeon, &h);
        break;
    }
    }
    LoreWrap(out);
    return p - out;
}
