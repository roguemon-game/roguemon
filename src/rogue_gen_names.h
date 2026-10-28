// Names of places and dungeons, and the roads between towns, in ASCII.
// Included at the end of rogue_gen.c (after the lore), so the host tools see
// the very names the game shows: the game's Rogue_CopyPlaceName and
// Rogue_CopyDungeonName convert these. Nothing here changes the region.

char *RogueGen_PlaceName(const struct RogueRegion *r, u8 place, char *dest);

// A place's name without its last word, as a town's without TOWN or CITY:
// PINEWICK. Dungeons named after a town read "PINEWICK TOWER".
char *RogueGen_TownStem(const struct RogueRegion *r, u8 place, char *dest)
{
    char *end = RogueGen_PlaceName(r, place, dest), *p;
    // RUN 0's numbered towns keep their number: "PLACE 12", not "PLACE".
    if (dest[0] == 'P' && dest[1] == 'L' && dest[2] == 'A' && dest[3] == 'C' && dest[4] == 'E' && dest[5] == ' '
     && dest[6] >= '0' && dest[6] <= '9')
        return end;
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

// The dungeon whose host a place is, or -1.
s32 RogueGen_HostedDungeon(const struct RogueRegion *r, u8 place)
{
    u32 i;
    for (i = 0; i < r->nDungeons; i++)
        if (r->dungeons[i].host == place)
            return i;
    return -1;
}

// A cave whose floors are ruins, an ice cave or a volcano goes by the
// dungeon's name, so the region map, the cave's popup and the floors all
// agree (BRIGHTTON RUINS, not SHADOW CAVE above BRIGHTTON RUINS B1F).
bool8 RogueGen_CaveHasDungeonName(const struct RogueRegion *r, u8 place)
{
    s32 d = RogueGen_HostedDungeon(r, place);
    u8 kind;
    if (d < 0)
        return FALSE;
    kind = r->dungeons[d].kind;
    return kind == RDUN_RUINS || kind == RDUN_ICE || kind == RDUN_VOLCANO;
}

// Cave names. A cave takes one of the first six as it always has; one that
// another cave of the region has already taken steps on through the rest,
// so no two caves of a region share a name.
static const char *const sTunnelNames[] = {
    "ROCKY TUNNEL", "BOULDER PASS", "GRANITE TUNNEL", "ECHO TUNNEL", "IRONVEIN PASS", "MOLE TUNNEL",
    "FLINT TUNNEL", "PEBBLE PASS", "COPPER TUNNEL", "SLATE PASS", "RUMBLE TUNNEL", "CRAG PASS",
};
static const char *const sCavernNames[] = {
    "SHADOW CAVE", "GLOOM CAVERN", "DUSK CAVE", "MOONLESS CAVE", "WHISPER CAVE", "HOLLOW DEEP",
    "MURK CAVERN", "EMBERLESS CAVE", "DRIP CAVE", "SILENT GROTTO", "BAT CAVE", "STILL CAVERN",
};
#define CAVE_NAMES_FIRST 6

// The name a plain cave takes: side caves (caverns) and caves on the road
// (tunnels) each from their own list, never one an earlier cave took.
static u32 CaveNameIndex(const struct RogueRegion *r, u32 place)
{
    u32 taken = 0, i;
    for (i = 0; i <= place && i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        u32 rng = RogueRng_Hash(r->seed, 8300 + i), k, n;
        if (p->kind != RPLACE_CAVE || p->caveOut == r->summit || p->side != r->places[place].side
         || RogueGen_CaveHasDungeonName(r, i))
            continue;
        k = RogueRng_Range(&rng, CAVE_NAMES_FIRST);
        for (n = 0; n < LORE_COUNT_OF(sCavernNames) && ((taken >> k) & 1); n++)
            k = k < CAVE_NAMES_FIRST ? CAVE_NAMES_FIRST + (rng + n) % (LORE_COUNT_OF(sCavernNames) - CAVE_NAMES_FIRST)
                                     : (k + 1) % LORE_COUNT_OF(sCavernNames);
        if (i == place)
            return k;
        taken |= 1u << k;
    }
    return 0;
}

static char *NameNumber(char *dest, u32 n)
{
    if (n >= 10)
        *dest++ = '0' + (n / 10) % 10;
    *dest++ = '0' + n % 10;
    *dest = 0;
    return dest;
}

// A place's name as the map popup and the signs show it: the town's name,
// ROUTE 12, VICTORY ROAD, a cave's name (or its dungeon's).
char *RogueGen_PlaceName(const struct RogueRegion *r, u8 place, char *dest)
{
    const struct RoguePlace *p;
    if (place >= r->nPlaces)
        place = 0;
    p = &r->places[place];
    if (p->kind == RPLACE_TOWN)
        return LoreCopy(dest, (const char *)r->townNames[p->number % ROGUE_MAX_TOWNS]);
    if (p->kind == RPLACE_CAVE)
    {
        if (p->caveOut == r->summit)
            return LoreCopy(dest, "VICTORY CAVE");
        if (RogueGen_CaveHasDungeonName(r, place))
            return RogueGen_DungeonName(r, RogueGen_HostedDungeon(r, place), dest);
        if (p->side)
            return LoreCopy(dest, sCavernNames[CaveNameIndex(r, place)]);
        return LoreCopy(dest, sTunnelNames[CaveNameIndex(r, place)]);
    }
    if (p->type == RROUTE_MOUNTAIN)
        return LoreCopy(dest, "VICTORY ROAD");
    return NameNumber(LoreCopy(dest, "ROUTE "), p->number);
}

static const char *const sTowerNames[] =
{
    "MT. SOLACE", "MT. VIGIL", "MT. HALLOW", "ELDER TOWER", "BELL TOWER", "SKY TOWER",
};

// A lair is named after its chamber: Hoenn's own names for the rooms that
// are one of a kind, else the town's with a word for the place.
static const char *const sLairNames[ROOM_STYLE_COUNT] =
{
    [ROOM_STYLE_RUINS]       = " RUINS",
    [ROOM_STYLE_ISLAND_CAVE] = " GROTTO",
    [ROOM_STYLE_TOMB]        = "ANCIENT TOMB",
    [ROOM_STYLE_CHAMBER]     = "SEALED CHAMBER",
    [ROOM_STYLE_TERRA]       = "TERRA CAVE",
    [ROOM_STYLE_MARINE]      = "MARINE CAVE",
    [ROOM_STYLE_PEAK]        = " PEAK",
    [ROOM_STYLE_DEPTHS]      = " ABYSS",
    [ROOM_STYLE_PILLAR]      = "SKY PILLAR",
    [ROOM_STYLE_ORIGIN]      = "CAVE OF ORIGIN",
    [ROOM_STYLE_HOLLOW]      = " HOLLOW",
    [ROOM_STYLE_DEN]         = " DEN",
    [ROOM_STYLE_ISLE]        = " ISLE",
    [ROOM_STYLE_GLADE]       = " GLADE",
};

static u8 NamesRoomStyle(const struct RogueRegion *r, u8 dungeon)
{
    u8 room = RogueGen_FloorRoom(r, dungeon, r->dungeons[dungeon].nFloors);
    return room != 0xFF ? sRogueRooms[room].style : ROOM_STYLE_COUNT;
}

// A legend's chamber is named after its vanilla room: Hoenn's own name for
// the rooms that are one of a kind, else the nearest town's with a word for
// the place (PINEWICK RUINS).
char *RogueGen_ChamberName(const struct RogueRegion *r, u8 dungeon, char *dest)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon % ROGUE_MAX_DUNGEONS];
    u8 style = NamesRoomStyle(r, dungeon % ROGUE_MAX_DUNGEONS);
    const char *name = style < ROOM_STYLE_COUNT ? sLairNames[style] : NULL;
    // Hoenn's names that belong to one legend go with that legend only: a
    // player who reads MARINE CAVE expects Kyogre, not Articuno. Others
    // take the town's name with a word for the place.
    switch (style)
    {
    case ROOM_STYLE_TERRA:
        if (d->legend != RLEG_GROUDON)
            name = " CAVERN";
        break;
    case ROOM_STYLE_MARINE:
        if (d->legend != RLEG_KYOGRE)
            name = " SEA CAVE";
        break;
    case ROOM_STYLE_PILLAR:
        if (d->legend != RLEG_RAYQUAZA)
            name = " SPIRE";
        break;
    case ROOM_STYLE_ORIGIN:
        if (d->legend != RLEG_KYOGRE && d->legend != RLEG_GROUDON && d->legend != RLEG_RAYQUAZA)
            name = " CAVE";
        break;
    }
    if (name == NULL)
        name = " RUINS";
    if (name[0] != ' ')
        return LoreCopy(dest, name);
    return LoreCopy(RogueGen_TownStem(r, RogueGen_TownNear(r, d->host), dest), name);
}

static char *RuinsName(const struct RogueRegion *r, u8 dungeon, char *dest)
{
    char *end = RogueGen_TownStem(r, RogueGen_TownNear(r, r->dungeons[dungeon].host), dest);
    char other[48];
    u32 i;
    LoreCopy(end, " RUINS");
    for (i = 0; i < r->nDungeons; i++)
    {
        if (i == dungeon || !RogueGen_HasChamber(&r->dungeons[i]))
            continue;
        RogueGen_ChamberName(r, i, other);
        if (strcmp(other, dest) == 0)
            return LoreCopy(end, " TEMPLE");
    }
    return LoreCopy(end, " RUINS");
}

// A dungeon's name, by its kind: a cave keeps its own name, a tower is its
// town's or a mountain's, a forest is its town's woods, a hideout is the
// team's.
char *RogueGen_DungeonName(const struct RogueRegion *r, u8 dungeon, char *dest)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon % ROGUE_MAX_DUNGEONS];
    u32 rng = RogueRng_Hash(r->seed, 8400 + dungeon);

    dungeon %= ROGUE_MAX_DUNGEONS;
    switch (d->kind)
    {
    case RDUN_TOWER:
        if (RogueRng_Range(&rng, 2) == 0)
            return LoreCopy(dest, sTowerNames[RogueRng_Range(&rng, LORE_COUNT_OF(sTowerNames))]);
        return LoreCopy(RogueGen_TownStem(r, d->host, dest), " TOWER");
    case RDUN_FOREST:
        return LoreCopy(RogueGen_TownStem(r, RogueGen_TownNear(r, d->host), dest), " WOODS");
    case RDUN_LAIR:
    {
        // Named after its chamber, but a cave floor that is a tunnel, or
        // leads to a chamber under the sky (Southern Island, a glade), is the
        // town's: FERNSTEAD TUNNEL B1F, not FERNSTEAD ISLE B1F. The chamber
        // keeps its own name in its popup.
        u8 style = NamesRoomStyle(r, dungeon);
        if (r->tunnel == dungeon)
            return LoreCopy(RogueGen_TownStem(r, RogueGen_TownNear(r, d->host), dest), " TUNNEL");
        if (style == ROOM_STYLE_ISLE || style == ROOM_STYLE_GLADE || style == ROOM_STYLE_PEAK)
            return LoreCopy(RogueGen_TownStem(r, RogueGen_TownNear(r, d->host), dest), " CAVE");
        return RogueGen_ChamberName(r, dungeon, dest);
    }
    case RDUN_RUINS:
        // The nearest town's RUINS, or its TEMPLE where a legend's chamber
        // is that town's RUINS already.
        return RuinsName(r, dungeon, dest);
    case RDUN_VICTORY:
        // Its floors are Emerald's VICTORY ROAD B1F and B2F.
        return LoreCopy(dest, "VICTORY ROAD");
    case RDUN_SHIP:
        // Hoenn's own name for it: a wreck nobody has a name for.
        return LoreCopy(dest, "ABANDONED SHIP");
    case RDUN_PLANT:
        // Kanto's, for a hall of generators nobody runs any more.
        return LoreCopy(dest, "POWER PLANT");
    case RDUN_MIRAGE:
        // (v1.1 peaks) Hoenn's own, for a tower the sandstorm hides.
        return LoreCopy(dest, "MIRAGE TOWER");
    case RDUN_ICE:
        // Its own name, made up for the cold (FROSTFALL GROTTO, WINTER'S
        // MAW) or a town's ICE CAVE, short enough for the floor popup with
        // its floor.
        return RogueGen_IceCaveName(r, dungeon, dest);
    case RDUN_VOLCANO:
        // The crater the region map draws and names, as MT. CHIMNEY: one
        // name on the map, the sign and the floors.
        return RogueGen_VolcanoName(r, dest);
    case RDUN_HIDEOUT:
        // The team's own, as Emerald's AQUA HIDEOUT.
        {
            static const char *const names[ROGUE_VILLAIN_IDS] = { "AQUA HIDEOUT", "MAGMA HIDEOUT", "ROCKET HIDEOUT" };
            u8 t = RogueGen_HideoutTeam(r, dungeon);
            return LoreCopy(dest, names[RogueGen_TeamId(r, t == 0xFF ? 0 : t) % ROGUE_VILLAIN_IDS]);
        }
    default:
        return RogueGen_PlaceName(r, d->host, dest);
    }
}

// What a dungeon floor's popup shows: its name into dest, and the floor
// number after it (0 none, 1 1F, -1 B1F). A lair is named after its
// chamber, and the chamber keeps that name alone (TERRA CAVE, PINEWICK
// GROTTO). Anywhere else a legend's chamber is the place's deepest floor,
// as Articuno's is SEAFOAM ISLANDS B4F and Mewtwo's CERULEAN CAVE B1F: a
// volcano's lava floor is MT. ASHEN B3F, never a cave's name. Floors go
// down in a cave, a hideout, an ice cave or a volcano (always entered from
// above), up a tower; a power plant and the woods have one level; a
// shipwreck's are the deck, 1F, B1F and B2F.
s8 RogueGen_FloorPopup(const struct RogueRegion *r, u8 dungeon, u8 floor, char *dest)
{
    const struct RogueDungeon *d = &r->dungeons[dungeon % ROGUE_MAX_DUNGEONS];
    bool8 chamber = RogueGen_HasChamber(d) && RogueGen_FloorRoom(r, dungeon, floor) != 0xFF;
    if (chamber && d->kind == RDUN_LAIR)
    {
        RogueGen_ChamberName(r, dungeon, dest);
        return 0;
    }
    RogueGen_DungeonName(r, dungeon, dest);
    switch (d->kind)
    {
    case RDUN_TOWER:  return floor;
    case RDUN_MIRAGE: return floor;
    case RDUN_PLANT:  return 0;
    case RDUN_FOREST: return 0;
    case RDUN_SHIP:   return floor == 1 ? 0 : floor == 2 ? 1 : -(s8)(floor - 2);
    default:          return -(s8)floor;
    }
}

// The next town along the road from a place, or the place itself at the
// end: the lowest-numbered place on, off the side paths (a side path's own
// way on).
u8 RogueGen_NextTownFrom(const struct RogueRegion *r, u8 place)
{
    u32 guard;
    for (guard = 0; guard < ROGUE_MAX_PLACES; guard++)
    {
        const struct RoguePlace *p = &r->places[place];
        u32 i, next = 0xFF;
        for (i = 0; i < p->nLinks; i++)
            if (p->links[i].target > place && !r->places[p->links[i].target].side && (next == 0xFF || p->links[i].target < next))
                next = p->links[i].target;
        if (p->side)
        {
            for (i = 0; i < p->nLinks; i++)
                if (p->links[i].target > place)
                    next = p->links[i].target;
        }
        if (next == 0xFF && p->mouth != 0xFF && r->places[p->mouth].caveIn == place)
            next = p->mouth;
        if (next == 0xFF && p->kind == RPLACE_CAVE && p->caveOut != 0xFF)
            next = p->caveOut;
        if (next == 0xFF)
            return place;
        place = next;
        if (r->places[place].kind == RPLACE_TOWN)
            return place;
    }
    return place;
}

// The route out of a town along the main road, or the one in, for the
// Pokemon townsfolk walk and the wild lines.
u8 RogueGen_RouteNear(const struct RogueRegion *r, u8 place)
{
    const struct RoguePlace *p = &r->places[place];
    u32 i, best = 0xFF;
    for (i = 0; i < p->nLinks; i++)
    {
        const struct RoguePlace *t = &r->places[p->links[i].target];
        // The main road's way on first: not a side path, shortcut or fork.
        if (t->kind == RPLACE_ROUTE && t->area != 0xFF
         && (best == 0xFF || (r->places[best].side && !t->side) || (r->places[best].side == t->side && p->links[i].target > best)))
            best = p->links[i].target;
    }
    if (best == 0xFF && p->mouth != 0xFF)
    {
        const struct RoguePlace *c = &r->places[p->mouth];
        best = c->caveIn != place ? c->caveIn : p->mouth;
    }
    return best == 0xFF ? 1 : best;
}
