// Region and town facts, and the recurring cast. Included at the end of
// rogue_gen.c (after the names and the people), so the host tools see the
// very facts the game's text names.
//
// A region is a place with a past: a founding tale tied to one of its
// legends, a festival, a famous figure. Each town has a trade, a landmark,
// something it is proud of, a rivalry with a neighbour, a local POKéMON
// (from the real wild table of the route out of town) and a current
// problem drawn from what the region really holds (the villains' hideout, a
// blocked road, someone lost in a dungeon nearby, a legend stirring).
// The cast are people the player keeps meeting: the wanderer (the region's
// CHAMPION), a reporter and camera pair, the lore elder, the explorer lost
// in a dungeon, and a family name for every home.
//
// Everything is worked out from the seed and the plan when asked, with its
// own random streams (salts 40000 to 41999), so it changes no map, trainer,
// item or wild table, and nothing is saved. Words are ASCII, as the lore's.
//
// To extend: add a row to gRogueTrades or gRogueLandmarks (keep the
// RTRADE_* order in rogue_gen.h if you name one), a festival to sFestivals,
// a figure to sFigures, a problem to RPROB_* with its pick in TownProblem.
// Town signatures (the parallel proposal) will drive trade and landmark
// through FactsSignature below: until then it returns 0xFF and they are
// chosen from the theme and the surroundings.

#define FACTS_SALT 40000

// ---------------------------------------------------------------------------
// What lies round a town

// RFACT_NEAR_* bits: what a town has at hand, by its own map's theme and the
// places one step away (routes linked to it, its cave mouth, dungeons it
// hosts or that are named after it).
// The sea on a town's own map: a harbor's waterfront (the theme alone is
// not enough: an inland ISLAND village has no beach, and a coast or a sea
// route one step out of town may start on a beach of its own). Those count
// as water at hand (RFACT_NEAR_LAKE), not as the sea in town.
bool8 RogueGen_TownSeaOnMap(const struct RogueRegion *r, u8 place)
{
    const struct RoguePlace *p = &r->places[place];
    // (v1.1 towns2) A raft village is all sea.
    return (p->layout == RLAYOUT_HARBOR && p->seaSide != RDIR_NONE) || p->theme == RTHEME_RAFT;
}

// vx, vy: the volcano's cell (RogueGen_VolcanoCell), vx < 0 for none.
static u8 FactsNear(const struct RogueRegion *r, u8 place, s8 vx, s8 vy)
{
    const struct RoguePlace *p = &r->places[place];
    u8 near = 0;
    u32 i;

    if (RogueGen_TownSeaOnMap(r, place))
        near |= RFACT_NEAR_SEA;
    if (p->theme == RTHEME_TREETOP || p->theme == RTHEME_VERDANT)
        near |= RFACT_NEAR_WOODS;
    if (p->theme == RTHEME_STONE || p->theme == RTHEME_SPRING || p->theme == RTHEME_ASH)
        near |= RFACT_NEAR_HILLS;
    for (i = 0; i < p->nLinks; i++)
    {
        const struct RoguePlace *t = &r->places[p->links[i].target];
        if (t->kind != RPLACE_ROUTE)
            continue;
        switch (t->type)
        {
        case RROUTE_LAKE:
        case RROUTE_RIVER:
            near |= RFACT_NEAR_LAKE;
            break;
        case RROUTE_FOREST:
        case RROUTE_WOODS:
            near |= RFACT_NEAR_WOODS;
            break;
        case RROUTE_HILLS:
        case RROUTE_DESERT:
        case RROUTE_MOUNTAIN:
            near |= RFACT_NEAR_HILLS;
            break;
        case RROUTE_FIELD:
        case RROUTE_FLOWERS:
        case RROUTE_LONGGRASS:
            near |= RFACT_NEAR_FIELDS;
            break;
        case RROUTE_COAST:
        case RROUTE_SEA:
            near |= RFACT_NEAR_LAKE;
            break;
        }
        if (t->mouth != 0xFF)
            near |= RFACT_NEAR_CAVE;
    }
    if (p->mouth != 0xFF)
        near |= RFACT_NEAR_CAVE;
    for (i = 0; i < r->nDungeons; i++)
        if (RogueGen_TownNear(r, r->dungeons[i].host) == place && r->dungeons[i].kind != RDUN_HIDEOUT)
            near |= RFACT_NEAR_DUNGEON;
    // The volcano the map draws: a town of ash, or one within two cells.
    if (vx >= 0)
    {
        s32 dx = p->gridX - vx, dy = p->gridY - vy;
        if (p->theme == RTHEME_ASH || (dx >= -2 && dx <= 3 && dy >= -2 && dy <= 3))
            near |= RFACT_NEAR_VOLCANO;
    }
    return near;
}

u8 RogueGen_TownSurroundings(const struct RogueRegion *r, u8 place)
{
    s8 vx, vy;
    if (!RogueGen_VolcanoCell(r, &vx, &vy))
        vx = -1;
    return FactsNear(r, place, vx, vy);
}

// ---------------------------------------------------------------------------
// Trades and landmarks: one table each, easy to extend

#define TH(t) (1u << RTHEME_##t)
#define TH_SEA (TH(HARBOR) | TH(ISLAND) | TH(COVE) | TH(BEACH))
#define TH_ALL ((1u << RTHEME_COUNT) - 1)

// What a town lives by. name: "fishing"; folk: who does it; goods: what
// it makes. themes: the town themes it suits. needs: RFACT_NEAR_* bits, any
// one of which it needs (0 for none). prides: two things the town is proud
// of, noun phrases ("the biggest catch in the region").
const struct RogueTrade gRogueTrades[RTRADE_COUNT] =
{
    [RTRADE_FISHING]   = { "fishing", "fishers", "fish", TH_SEA | TH(MEADOW) | TH(VERDANT), RFACT_NEAR_SEA | RFACT_NEAR_LAKE,
                           { "the freshest fish anywhere", "a MAGIKARP as long as a boat" }, RSHOP_NONE, "Where the nets come home full." },
    [RTRADE_BOATS]     = { "boatbuilding", "shipwrights", "boats", TH(HARBOR) | TH(COVE) | TH(BEACH), RFACT_NEAR_SEA,
                           { "boats that never leak", "the fastest boat on the coast" }, RSHOP_SEA, "Where boats are born." },
    [RTRADE_PEARLS]    = { "pearl diving", "divers", "pearls", TH(ISLAND) | TH(BEACH) | TH(COVE), RFACT_NEAR_SEA,
                           { "the biggest pearl ever found", "divers who hold their breath forever" }, RSHOP_SEA, "A town of divers and pearls." },
    [RTRADE_SALT]      = { "salt making", "salt makers", "sea salt", TH(BEACH) | TH(HARBOR), RFACT_NEAR_SEA,
                           { "the whitest salt in the region", "salt that makes any stew sing" }, RSHOP_NONE, "Salt of the sea, salt of the earth." },
    [RTRADE_MARKET]    = { "trade", "merchants", "goods from far away", TH(HARBOR) | TH(COVE), 0,
                           { "the busiest market in the region", "a shop for every BALL ever made" }, RSHOP_BALL, "Where every road brings trade." },
    [RTRADE_BERRIES]   = { "berry farming", "farmers", "BERRIES", TH(MEADOW) | TH(VERDANT) | TH(ASH), 0,
                           { "the sweetest BERRIES in the region", "a BERRY bush older than the town" }, RSHOP_HERB, "Sweet BERRIES, sweeter people." },
    [RTRADE_FLOWERS]   = { "flower growing", "gardeners", "flowers", TH(MEADOW) | TH(VERDANT) | TH(COVE), 0,
                           { "flowers in every color there is", "a garden even ROSELIA visit" }, RSHOP_NONE, "A town in full bloom." },
    [RTRADE_HERBS]     = { "herb medicine", "herbalists", "herbs", TH(VERDANT) | TH(SPRING) | TH(TREETOP), RFACT_NEAR_WOODS | RFACT_NEAR_HILLS,
                           { "an herb for every ache", "the bitterest medicine that works" }, RSHOP_HERB, "Where the hills grow medicine." },
    [RTRADE_MILK]      = { "dairy farming", "ranchers", "MOOMOO MILK", TH(MEADOW) | TH(VERDANT), RFACT_NEAR_FIELDS,
                           { "the creamiest MOOMOO MILK there is", "a MILTANK that won a ribbon" }, RSHOP_NONE, "Fresh MOOMOO MILK every morning." },
    [RTRADE_HONEY]     = { "beekeeping", "beekeepers", "honey", TH(VERDANT) | TH(TREETOP) | TH(MEADOW), RFACT_NEAR_WOODS | RFACT_NEAR_FIELDS,
                           { "the sweetest honey in the region", "COMBEE that know every flower" }, RSHOP_NONE, "A town as sweet as honey." },
    [RTRADE_MINING]    = { "mining", "miners", "ore", TH(STONE) | TH(ASH) | TH(SPRING), RFACT_NEAR_CAVE | RFACT_NEAR_HILLS,
                           { "the deepest mine in the region", "a NUGGET as big as a fist" }, RSHOP_STONE, "Built on what lies below." },
    [RTRADE_STONE]     = { "stonecutting", "masons", "cut stone", TH(STONE) | TH(ISLAND), RFACT_NEAR_HILLS | RFACT_NEAR_CAVE | RFACT_NEAR_SEA,
                           { "walls that have stood for ages", "the straightest stone streets" }, RSHOP_STONE, "Solid as the mountain." },
    [RTRADE_GLASS]     = { "glassblowing", "glassblowers", "glass", TH(ASH), RFACT_NEAR_VOLCANO,
                           { "glass made from the volcano's ash", "flutes of every color" }, RSHOP_HELD, "Where ash turns into glass." },
    [RTRADE_INNS]      = { "hot spring inns", "innkeepers", "hot spring eggs", TH(SPRING), 0,
                           { "the hottest spring in the region", "eggs boiled in the springs" }, RSHOP_HERB, "Soak your worries away." },
    [RTRADE_POTTERY]   = { "pottery", "potters", "pots", TH(ASH) | TH(SPRING) | TH(STONE) | TH(MEADOW), 0,
                           { "pots that never crack", "a kiln that never goes cold" }, RSHOP_NONE, "Shaped by hand and fire." },
    [RTRADE_WEAVING]   = { "weaving", "weavers", "cloth", TH(MEADOW) | TH(TREETOP) | TH(VERDANT) | TH(COVE), 0,
                           { "scarves soft as a SWABLU", "the brightest cloth in the region" }, RSHOP_NONE, "A town woven together." },
    [RTRADE_WOOD]      = { "woodcraft", "woodworkers", "furniture", TH(TREETOP) | TH(VERDANT), RFACT_NEAR_WOODS,
                           { "chairs that last a lifetime", "the finest carving in the region" }, RSHOP_TYPE, "Carved from the forest." },
    [RTRADE_BALLS]     = { "BALL making", "BALL makers", "POK@ BALLS", TH(STONE), 0,
                           { "BALLS that never miss a catch", "the first POK@ BALL workshop" }, RSHOP_BALL, "Where every BALL begins." },
    [RTRADE_STARS]     = { "stargazing", "stargazers", "star charts", TH(ISLAND) | TH(SPRING), 0,
                           { "the clearest night sky anywhere", "a falling star that landed here" }, RSHOP_STONE, "Closest to the stars." },
    [RTRADE_PAINTING]  = { "painting", "painters", "paintings", TH(COVE) | TH(ISLAND) | TH(MEADOW), 0,
                           { "a painter famous across the region", "the prettiest view to paint" }, RSHOP_TM, "A town worth painting." },
    [RTRADE_SHELLS]    = { "shell crafts", "shell crafters", "SHELL BELLS", TH(BEACH) | TH(COVE), RFACT_NEAR_SEA,
                           { "SHELL BELLS that ring the truest", "a shell as big as a SPHEAL" }, RSHOP_SEA, "Where the sea leaves its gifts." },
    [RTRADE_ROPES]     = { "rope making", "ropemakers", "rope bridges", TH(TREETOP), 0,
                           { "rope bridges no one has ever fallen from", "ropes strong as TANGELA vines" }, RSHOP_TYPE, "A town that walks the sky." },
    [RTRADE_FOSSILS]   = { "fossil digging", "diggers", "fossils", TH(STONE) | TH(ASH), RFACT_NEAR_HILLS | RFACT_NEAR_CAVE,
                           { "the oldest fossil ever dug up", "a whole wall of fossils" }, RSHOP_STONE, "Where the past is dug up." },
    [RTRADE_TEA]       = { "tea growing", "tea growers", "tea", TH(VERDANT) | TH(SPRING) | TH(MEADOW), RFACT_NEAR_HILLS | RFACT_NEAR_FIELDS,
                           { "tea that wakes up a SLAKOTH", "the greenest tea fields anywhere" }, RSHOP_HERB, "Stop a while for a cup." },
    [RTRADE_BAKING]    = { "baking", "bakers", "BERRY pies", TH(MEADOW) | TH(VERDANT) | TH(HARBOR), 0,
                           { "the best BERRY pie in the region", "bread that SNORLAX would wake for" }, RSHOP_NONE, "Follow the smell of fresh bread." },
    [RTRADE_LUMBER]    = { "logging", "loggers", "timber", TH(VERDANT) | TH(TREETOP) | TH(MEADOW), RFACT_NEAR_WOODS,
                           { "the tallest trees in the region", "logs that float down to the sea" }, RSHOP_NONE, "Tall trees and strong hands." },
    // A town signature's (themes 0: never chosen by theme).
    [RTRADE_SPACE]     = { "space research", "scientists", "rockets", 0, 0,
                           { "a rocket that will reach the stars", "the clearest night skies anywhere" }, RSHOP_STONE, "Reaching for the stars." },
    [RTRADE_WEATHER]   = { "weather watching", "forecasters", "forecasts", 0, 0,
                           { "forecasts that are never wrong", "the first word of every storm" }, RSHOP_TYPE, "Watching the sky for you." },
    [RTRADE_FIGHTING]  = { "martial arts", "fighters", "tough trainers", 0, 0,
                           { "the toughest trainers in the region", "a MASTER who once chopped a boulder" }, RSHOP_BATTLE, "Strong body, strong heart!" },
};

// Things a town is known by, seen or told of. A landmark with ' ' first takes
// the town's name: " LIGHTHOUSE" is PINEWICK LIGHTHOUSE; one with '*' takes
// the famous figure's ("the STATUE of *" in their town only). trade: the
// trade it suits best, or RTRADE_COUNT for any. onMap: the map shows it (the
// town signatures will set it; for now none, so lines must not send the
// player to look for it).
const struct RogueLandmark gRogueLandmarks[RLANDMARK_COUNT] =
{
    [RLANDMARK_LIGHTHOUSE] = { " LIGHTHOUSE",         TH_SEA,                               RFACT_NEAR_SEA,   RTRADE_BOATS, "Its light guides every boat home." },
    [RLANDMARK_PIER]       = { "the OLD PIER",        TH_SEA,                               RFACT_NEAR_SEA,   RTRADE_FISHING, "Where the boats come home." },
    [RLANDMARK_MARKET]     = { " MARKET",             TH(HARBOR) | TH(COVE) | TH(STONE),    0,                RTRADE_MARKET, "The market never sleeps." },
    [RLANDMARK_WINDMILL]   = { "the OLD WINDMILL",    TH(MEADOW) | TH(VERDANT) | TH(BEACH), 0,                RTRADE_BERRIES, "Where the old WINDMILL turns." },
    [RLANDMARK_GREAT_TREE] = { "the GREAT TREE",      TH(TREETOP) | TH(VERDANT) | TH(MEADOW), 0,              RTRADE_ROPES, "In the arms of the GREAT TREE." },
    [RLANDMARK_CLOCK]      = { "the CLOCK TOWER",     TH(STONE) | TH(HARBOR) | TH(COVE),    0,                RTRADE_COUNT, "Never late, never early." },
    [RLANDMARK_BRIDGE]     = { "the STONE BRIDGE",    TH_ALL,                               RFACT_NEAR_LAKE,  RTRADE_STONE, "A town on both banks." },
    [RLANDMARK_STATUE]     = { "the STATUE of *",     TH_ALL,                               0,                RTRADE_COUNT, "Where {FIGURE} still stands tall." },
    [RLANDMARK_MUSEUM]     = { " MUSEUM",             TH(STONE) | TH(HARBOR) | TH(COVE),    0,                RTRADE_FOSSILS, "Where the past is kept safe." },
    [RLANDMARK_TELESCOPE]  = { "the BIG TELESCOPE",   TH(ISLAND) | TH(SPRING) | TH(STONE),  0,                RTRADE_STARS, "Closer to the stars." },
    [RLANDMARK_BATHHOUSE]  = { "the OLD BATHHOUSE",   TH(SPRING),                           0,                RTRADE_INNS, "Warm water, warm hearts." },
    [RLANDMARK_KILN]       = { "the GLASS WORKSHOP",  TH(ASH),                              RFACT_NEAR_VOLCANO, RTRADE_GLASS, "Where ash becomes glass." },
    [RLANDMARK_GALLERY]    = { "the ART GALLERY",     TH(COVE) | TH(ISLAND),                0,                RTRADE_PAINTING, "A town of every color." },
    [RLANDMARK_FIELDS]     = { "the FLOWER FIELDS",   TH(MEADOW) | TH(VERDANT),             0,                RTRADE_FLOWERS, "A sea of flowers." },
    [RLANDMARK_ORCHARD]    = { "the BERRY ORCHARD",   TH(MEADOW) | TH(VERDANT) | TH(ASH),   0,                RTRADE_BERRIES, "Where BERRIES grow sweetest." },
    [RLANDMARK_WELL]       = { "the WISHING WELL",    TH(MEADOW) | TH(VERDANT) | TH(STONE) | TH(ASH) | TH(SPRING), 0, RTRADE_COUNT, "Make a wish and stay a while." },
    [RLANDMARK_MINE]       = { "the OLD MINE",        TH(STONE) | TH(ASH) | TH(SPRING),     RFACT_NEAR_HILLS | RFACT_NEAR_CAVE, RTRADE_MINING, "Built on what lies below." },
    [RLANDMARK_WATERWHEEL] = { "the WATERWHEEL",      TH(MEADOW) | TH(VERDANT) | TH(TREETOP), RFACT_NEAR_LAKE, RTRADE_MILK, "Where the water does the work." },
    [RLANDMARK_SHELL_HALL] = { "the SHELL HALL",      TH(BEACH) | TH(COVE),                 RFACT_NEAR_SEA,   RTRADE_SHELLS, "Where the sea leaves its gifts." },
    [RLANDMARK_ROPEWALK]   = { "the SKY WALK",        TH(TREETOP),                          0,                RTRADE_ROPES, "A town that walks the sky." },
    [RLANDMARK_TEA_HOUSE]  = { "the TEA HOUSE",       TH(VERDANT) | TH(SPRING) | TH(MEADOW), 0,               RTRADE_TEA, "Stop a while for a cup." },
    [RLANDMARK_BELL]       = { "the OLD BELL",        TH(STONE) | TH(SPRING) | TH(ISLAND) | TH(TREETOP), 0,  RTRADE_COUNT, "Where the old BELL rings." },
    [RLANDMARK_DOCKS]      = { "the SHIPYARD",        TH(HARBOR) | TH(COVE),                RFACT_NEAR_SEA,   RTRADE_BOATS, "Where boats are born." },
    [RLANDMARK_BAKERY]     = { "the BIG OVEN",        TH(MEADOW) | TH(VERDANT) | TH(HARBOR), 0,               RTRADE_BAKING, "Follow the smell of fresh bread." },
    // A town signature's, built on the map (themes 0: never chosen by theme).
    [RLANDMARK_HOT_SPRING] = { "the HOT SPRING",      0, 0, RTRADE_INNS,     "Soak away the road's aches." },
    [RLANDMARK_SPACE]      = { " SPACE CENTER",       0, 0, RTRADE_SPACE,    "Reaching for the stars." },
    [RLANDMARK_WEATHER]    = { "the WEATHER INSTITUTE", 0, 0, RTRADE_WEATHER, "Watching the sky for you." },
    [RLANDMARK_DOJO]       = { " DOJO",               0, 0, RTRADE_FIGHTING, "Strong body, strong heart!" },
    [RLANDMARK_RANCH]      = { "the RANCH",           0, 0, RTRADE_MILK,     "Home of the finest MOOMOO MILK." },
    [RLANDMARK_DIG]        = { "the FOSSIL DIG",      0, 0, RTRADE_FOSSILS,  "Dig with care. History inside!" },
    [RLANDMARK_POND]       = { "the FISHING POND",    0, 0, RTRADE_FISHING,  "Cast a line, catch a story." },
    [RLANDMARK_MINES]      = { " MINES",              0, 0, RTRADE_MINING,   "Built on what lies below." },
};

#undef TH_ALL

// ---------------------------------------------------------------------------
// The region: founding tale, festival, famous figure

// A festival for each nature of legend (LELEM_*), named as the real games
// name theirs, with what people do at it.
struct FactsFestival
{
    const char *name;
    const char *act; // what people do, a clause: "people float lanterns on the water"
};
static const struct FactsFestival sFestivals[LELEM_COUNT][3] =
{
    [LELEM_FIRE]  = { { "EMBER FESTIVAL", "people light bonfires on the hills all night" },
                      { "LANTERN NIGHT", "everyone carries a lantern through the streets" },
                      { "SUN FESTIVAL", "people dance from sunrise to sunset" } },
    [LELEM_ICE]   = { { "FROST FAIR", "people carve statues of ice in the square" },
                      { "SNOW FESTIVAL", "kids build snow POK@MON on every corner" },
                      { "WINTER LIGHTS", "candles burn in every window" } },
    [LELEM_WATER] = { { "TIDE FESTIVAL", "people float paper boats out on the water" },
                      { "LANTERN FLOAT", "lanterns drift down the water at dusk" },
                      { "RAIN DANCE", "people dance in the first rain of the year" } },
    [LELEM_STORM] = { { "THUNDER FESTIVAL", "drums play loud enough to shake the windows" },
                      { "STORM DAY", "everyone flies a kite into the wind" },
                      { "FIRST LIGHT", "people stay up to watch the lightning" } },
    [LELEM_SKY]   = { { "KITE FESTIVAL", "the sky fills with kites shaped like birds" },
                      { "WIND FESTIVAL", "chimes hang from every roof" },
                      { "FEATHER DAY", "everyone wears a feather in their hat" } },
    [LELEM_STONE] = { { "STONE DAY", "people stack stones by the road for luck" },
                      { "HARVEST FAIR", "farmers bring their biggest crops to town" },
                      { "MOUNTAIN FAIR", "everyone climbs the nearest hill at dawn" } },
    [LELEM_GRASS] = { { "BLOOM FESTIVAL", "every door is hung with flowers" },
                      { "HARVEST FAIR", "farmers bring their biggest crops to town" },
                      { "GREEN DAY", "everyone plants a tree" } },
    [LELEM_MIND]  = { { "STAR FESTIVAL", "people make wishes on paper stars" },
                      { "DREAM NIGHT", "everyone stays up to watch the moon" },
                      { "MOON FESTIVAL", "people leave sweets out for the moon" } },
};

// How the region began, by its legend's nature: a clause after "Long ago,"
// with {LEGEND} and {OLDTOWN} (the oldest town) filled in by the text.
// (A region whose every legend was feared and shut away: the founding is
// that.)
#define FOUNDING_SEALED 0xFE
static const char *const sFoundings[LELEM_COUNT][2] =
{
    [LELEM_FIRE]  = { "the first settlers came for the warm ground {LEGEND} kept",
                      "{LEGEND} lit a fire on the hills, and people came to it" },
    [LELEM_ICE]   = { "{LEGEND} sheltered the first settlers through a long winter",
                      "people followed {LEGEND} across the frozen sea" },
    [LELEM_WATER] = { "{LEGEND} led the first boats safely to shore",
                      "{LEGEND} calmed a flood, and the first farms grew in the mud" },
    [LELEM_STORM] = { "{LEGEND}'s storm broke a drought, and people stayed",
                      "people saw lightning strike where {LEGEND} landed, and built there" },
    [LELEM_SKY]   = { "{LEGEND} showed the first people the way over the hills",
                      "{LEGEND} carried a lost child home, and the village grew round that house" },
    [LELEM_STONE] = { "people built on the rock where {LEGEND} sleeps",
                      "{LEGEND} moved the mountains aside so the first road could pass" },
    [LELEM_GRASS] = { "{LEGEND} made the land green for the first farms",
                      "the first seeds were a gift from {LEGEND}" },
    [LELEM_MIND]  = { "{LEGEND} taught the first people to read the stars",
                      "{LEGEND} sent the first settlers a dream of this land" },
};

// The famous figure: what they did, a clause after their name ("ROSALIND
// ..."), and the words for them. The region's first CHAMPION, an explorer,
// an inventor, a founder.
struct FactsFigure
{
    const char *title; // "explorer"
    const char *deed;  // "mapped every road in {REGION}"
};
#define FACTS_FIGURE_FOUNDER 3
static const struct FactsFigure sFigures[] =
{
    { "explorer",  "mapped every road in {REGION}" },
    { "champion",  "was {REGION}'s very first CHAMPION" },
    { "inventor",  "made the first POK@ BALL in {REGION}" },
    { "founder",   "built the first house in {FIGURE_TOWN}" },
    { "professor", "wrote the first POK@DEX of {REGION}" },
    { "breeder",   "raised over a hundred POK@MON" },
};

// Given names for the region's people: the figure, the cast. Up to eight
// letters; none a gym leader's, a trainer's (src/rogue.c sMaleNames,
// sFemaleNames) or a vanilla character's.
static const char *const sCastMale[] =
{
    "ALDRIC", "BARNABY", "CASPIAN", "DORIAN", "EDMUND", "FELIX", "GIDEON", "HOLLIS", "ISIDORE", "JASPER",
    "LORCAN", "MAGNUS", "NEVILLE", "OSRIC", "PERCIVAL", "QUENTIN", "ROLAND", "SILAS", "THADDEUS", "ULRIC",
    "VALE", "WARREN", "BASIL", "CORMAC", "EMRYS", "FINLEY", "HUXLEY", "LEOPOLD",
};
static const char *const sCastFemale[] =
{
    "ADELINE", "BEATRIX", "CELESTE", "DAPHNE", "ELOISE", "FLORA", "GRETA", "HELENA", "IMOGEN", "JOSEPHA",
    "LAVINIA", "MARGOT", "NOELLE", "OPHELIA", "PRISCA", "ROSALIND", "SYLVIE", "TAMSIN", "URSULA", "VIOLET",
    "WINIFRED", "AUGUSTA", "CLEMENCE", "DELPHINE", "ESTELLE", "FENELLA", "HONORA", "LUCASTA",
};
// BASIL is a gym leader's: kept out of the picks below.
#define CAST_NAME_AVOID "BASIL"

// Names for the wanderer: grand, as champions' are.
static const char *const sWandererMale[] = { "DORIAN", "CASPIAN", "LEOPOLD", "THADDEUS", "PERCIVAL", "MAGNUS", "ALDRIC", "ROLAND" };
static const char *const sWandererFemale[] = { "ROSALIND", "CELESTE", "AUGUSTA", "OPHELIA", "ESTELLE", "HELENA", "DELPHINE", "IMOGEN" };

// The reporter pairs, as GABBY and TY: a reporter and a camera operator.
static const char *const sReporterPairs[][2] =
{
    { "PENNY", "TOD" }, { "DOT", "BRAM" }, { "TRIXIE", "MO" }, { "PIP", "OZ" }, { "MOLLY", "GUS" },
    { "ROXY", "JED" }, { "KIKI", "BO" }, { "BUNNY", "RAY" }, { "LULU", "TEX" }, { "NIA", "ZED" },
};
static const u8 sReporterFemale[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };

// The lore elder: GRANNY or GRAMPS and a short name.
static const char *const sElderNames[] = { "MAE", "ODELL", "HETTY", "BRAM", "ROSE", "IVO", "NELL", "ABE", "EDIE", "SOL" };
static const u8 sElderFemale[] = { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };

// The explorer lost in a dungeon, thanked later.
static const char *const sExplorerNames[] = { "MILES", "ROSA", "DEXTER", "IRIS", "BARRETT", "WREN", "OSWALD", "TILLY" };
static const u8 sExplorerFemale[] = { 0, 1, 0, 1, 0, 1, 0, 1 };

// Family names for the homes ("THE ASHBY HOUSE"), never two alike in a
// region: one for each town number and house (ROGUE_MAX_TOWNS x 7).
static const char *const sFamilyNames[] =
{
    "ASHBY", "BARLOW", "CARVER", "DALTON", "ELLERY", "FENWICK", "GARLAND", "HADLEY", "INGRAM", "JARVIS",
    "KENDALL", "LANGLEY", "MERRIL", "NORWOOD", "OAKLEY", "PRESCOTT", "QUINLAN", "RADCLIFF", "SAWYER", "THORNE",
    "UPTON", "VAUGHN", "WHITLOCK", "YARDLEY", "ABBOTT", "BECKETT", "CROFT", "DUNMORE", "EASTON", "FAIRLEY",
    "GOODWIN", "HARTLEY", "IRVING", "JUDD", "KIPLING", "LOCKE", "MARLOW", "NASH", "OSGOOD", "PEMBROKE",
    "REDFERN", "SHELBY", "TALBOT", "UNDERHILL", "VANCE", "WINSLOW", "ALCOTT", "BRAMBLE", "CALLOW", "DRAPER",
    "ELWOOD", "FINCH", "GRAYSON", "HOLLOWAY", "IVERSON", "KEMP", "LARK", "MAYHEW", "NEWBURY", "ORMOND",
    "PIPER", "QUILL", "ROOKWOOD", "SPARROW", "TINDALL", "VARLEY", "WREN", "ACKLEY", "BELLAMY", "COBB",
    "DEVLIN", "EVERLY", "FOXLEY", "GALE", "HEATH", "JOLLY", "KNOX", "LINDLEY", "MOSS", "NOBLE",
    "OLDHAM", "PARRISH", "RAINE", "STOKES", "TUCKER", "VOSS", "WARDELL", "BUNCE", "CHALK", "DIMMOCK",
    "FROST", "HAZEL", "LACEY", "MILLER", "PEABODY", "ROWE", "SPINK", "TOLLY", "WEBB", "YOUNGER",
};

// ---------------------------------------------------------------------------
// Picks

// The k-th of a run of picks from n under a key: consecutive k give
// different entries until all n are used, each key in its own order.
u32 RogueText_Deal(u32 seed, u32 key, u32 k, u32 n)
{
    u32 h = RogueRng_Hash(seed, key), step, a, b, t;
    if (n <= 1)
        return 0;
    // A step coprime to n walks every entry once before repeating.
    step = 1 + (h >> 8) % (n - 1);
    a = step, b = n;
    while (b)
        t = a % b, a = b, b = t;
    while (a != 1)
    {
        step = step % (n - 1) + 1;
        a = step, b = n;
        while (b)
            t = a % b, a = b, b = t;
    }
    return ((h & 0xFF) + k * step) % n;
}

static bool8 FactsSame(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

static const char *FactsName(u32 seed, u32 salt, u8 female, u32 k)
{
    const char *const *list = female ? sCastFemale : sCastMale;
    u32 n = female ? LORE_COUNT_OF(sCastFemale) : LORE_COUNT_OF(sCastMale);
    const char *name = list[RogueText_Deal(seed, salt, k, n)];
    if (FactsSame(name, CAST_NAME_AVOID))
        name = list[RogueText_Deal(seed, salt, k + 1, n)];
    return name;
}

// The legend the region's tale is about: the first on the main road's side,
// the earliest dungeon with one.
static s32 FactsLegend(const struct RogueRegion *r)
{
    return RogueGen_LegendDungeon(r);
}

static u8 FactsLegendElem(const struct RogueRegion *r)
{
    s32 d = FactsLegend(r);
    if (d < 0 || r->dungeons[d].legend >= RLEG_COUNT)
        return LELEM_STONE;
    return sLegendElem[r->dungeons[d].legend];
}

// Towns of the main road in order (not the summit, not a hamlet off it).
static u32 FactsRoadTowns(const struct RogueRegion *r, u8 *out)
{
    u32 n = 0, i;
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        if (p->kind == RPLACE_TOWN && !p->side && p->type != RTOWN_SUMMIT && p->type != RTOWN_HAMLET)
            out[n++] = i;
    }
    return n;
}

// The facts most text asks for again and again (the region's, the cast,
// the town the player is in) take a frame or two each on the GBA: they are
// worked out once and kept here, keyed by the region's seed (and the town's
// place), so the host and the game say the very same things. 48 bytes. The
// volcano's cell (the region map's land, most of a frame) rides along. Each
// is worked out into a local first and stored whole, as the generator may
// yield to the game in between.
#define FACTS_HAVE_REGION  1
#define FACTS_HAVE_CAST    2
#define FACTS_HAVE_TOWN    4
#define FACTS_HAVE_VOLCANO 8
static ROGUE_EWRAM struct
{
    u32 seed;
    u8 have, townPlace;
    s8 vx, vy; // here, not last: the structs below are word aligned
    struct RogueRegionFacts region;
    struct RogueCast cast;
    struct RogueTownFacts town;
} sFactsCache = {0};

static void FactsForget(void)
{
    sFactsCache.have = 0;
}

static void FactsCacheFor(const struct RogueRegion *r)
{
    if (sFactsCache.seed != r->seed)
    {
        sFactsCache.seed = r->seed;
        sFactsCache.have = 0;
    }
}

static __attribute__((noinline)) void FactsRegion(const struct RogueRegion *r, struct RogueRegionFacts *f);

void RogueGen_RegionFacts(const struct RogueRegion *r, struct RogueRegionFacts *f)
{
    FactsCacheFor(r);
    if (!(sFactsCache.have & FACTS_HAVE_REGION))
    {
        FactsRegion(r, f);
        FactsCacheFor(r);
        sFactsCache.region = *f;
        sFactsCache.have |= FACTS_HAVE_REGION;
        return;
    }
    *f = sFactsCache.region;
}

// The volcano's cell (RogueGen_VolcanoCell), for the text: kept once worked
// out. FALSE if the region map draws none.
bool8 RogueGen_FactsVolcano(const struct RogueRegion *r, s8 *x, s8 *y)
{
    FactsCacheFor(r);
    if (!(sFactsCache.have & FACTS_HAVE_VOLCANO))
    {
        s8 vx, vy;
        if (!RogueGen_VolcanoCell(r, &vx, &vy))
            vx = vy = -1;
        FactsCacheFor(r);
        sFactsCache.vx = vx, sFactsCache.vy = vy;
        sFactsCache.have |= FACTS_HAVE_VOLCANO;
    }
    *x = sFactsCache.vx, *y = sFactsCache.vy;
    return sFactsCache.vx >= 0;
}

static __attribute__((noinline)) void FactsRegion(const struct RogueRegion *r, struct RogueRegionFacts *f)
{
    u32 rng = RogueRng_Hash(r->seed, FACTS_SALT + 1);
    u8 towns[ROGUE_MAX_PLACES], elem = FactsLegendElem(r);
    u32 n = FactsRoadTowns(r, towns);
    s32 d = FactsLegend(r);

    // The tale is about one of the region's legends: one the settlers could
    // have been glad of, not one whose own tale says the old folk sealed it
    // away (RogueGen_LegendFeared); if every legend is one of those, the
    // founding is the sealing.
    bool8 feared = FALSE;
    {
        u8 ds[ROGUE_MAX_DUNGEONS], nd = 0, i;
        for (i = 0; i < r->nDungeons; i++)
            if (r->dungeons[i].boss == RBOSS_LEGEND && r->dungeons[i].legend < RLEG_COUNT && !RogueGen_LegendFeared(r, i))
                ds[nd++] = i;
        if (nd == 0)
            for (i = 0; i < r->nDungeons; i++)
                if (r->dungeons[i].boss == RBOSS_LEGEND && r->dungeons[i].legend < RLEG_COUNT)
                    ds[nd++] = i, feared = TRUE;
        if (nd)
        {
            d = ds[RogueRng_Range(&rng, nd)];
            elem = sLegendElem[r->dungeons[d].legend];
        }
    }
    f->legend = d < 0 ? 0xFF : d;
    f->founding = elem * 2 + RogueRng_Range(&rng, 2);
    if (feared)
        f->founding = FOUNDING_SEALED;
    // The oldest town: where the legend's tale is told, if on the road;
    // else the first town, where every journey starts.
    f->oldTown = r->start;
    if (d >= 0)
    {
        struct RogueLore lore;
        RogueGen_LegendLore(r, d, &lore);
        if (lore.town < r->nPlaces && !r->places[lore.town].side && r->places[lore.town].type != RTOWN_SUMMIT)
            f->oldTown = lore.town;
    }
    // The festival honors one of the region's legends, any of them.
    {
        u8 legends[ROGUE_MAX_DUNGEONS], nl = 0, i, fe = elem;
        for (i = 0; i < r->nDungeons; i++)
            if (r->dungeons[i].boss == RBOSS_LEGEND && r->dungeons[i].legend < RLEG_COUNT)
                legends[nl++] = r->dungeons[i].legend;
        if (nl)
            fe = sLegendElem[legends[RogueRng_Range(&rng, nl)]];
        f->festival = fe * 3 + RogueRng_Range(&rng, 3);
    }
    // The festival is held in a gym city of the middle of the road.
    f->festTown = n ? towns[n / 3 + RogueRng_Range(&rng, (n + 2) / 3)] : r->start;
    f->figure = RogueRng_Range(&rng, LORE_COUNT_OF(sFigures));
    f->figureFemale = RogueRng_Range(&rng, 2);
    f->figureTown = n > 1 ? towns[1 + RogueRng_Range(&rng, n - 1)] : r->start;
    // A town's founder is not the first town's: that one grew round the lab.
    if (f->figure == FACTS_FIGURE_FOUNDER && f->figureTown == r->start)
        f->figureTown = n > 1 ? towns[n - 1] : r->start;
}

const char *RogueGen_FestivalName(const struct RogueRegionFacts *f)
{
    return sFestivals[f->festival / 3 % LELEM_COUNT][f->festival % 3].name;
}

const char *RogueGen_FestivalAct(const struct RogueRegionFacts *f)
{
    return sFestivals[f->festival / 3 % LELEM_COUNT][f->festival % 3].act;
}

const char *RogueGen_FoundingTale(const struct RogueRegionFacts *f)
{
    if (f->founding == FOUNDING_SEALED)
        return "the first settlers sealed {LEGEND} away and built over it";
    return sFoundings[f->founding / 2 % LELEM_COUNT][f->founding & 1];
}

const char *RogueGen_FigureName(const struct RogueRegion *r, const struct RogueRegionFacts *f)
{
    return FactsName(r->seed, FACTS_SALT + 2, f->figureFemale, 0);
}

const char *RogueGen_FigureTitle(const struct RogueRegionFacts *f)
{
    return sFigures[f->figure % LORE_COUNT_OF(sFigures)].title;
}

const char *RogueGen_FigureDeed(const struct RogueRegionFacts *f)
{
    return sFigures[f->figure % LORE_COUNT_OF(sFigures)].deed;
}

// ---------------------------------------------------------------------------
// Towns

// A town signature's trade and landmark (the parallel proposal), or 0xFF
// for none yet: then they are chosen below.
static u8 FactsSignature(const struct RogueRegion *r, u8 place, u8 *landmark)
{
    *landmark = RogueGen_SignatureLandmarkId(r, place);
    return RogueGen_SignatureTrade(r, place);
}

// The nearest other town by the roads (not the summit), for a rivalry.
static u8 FactsNeighbor(const struct RogueRegion *r, u8 place)
{
    u8 queue[ROGUE_MAX_PLACES];
    u64 seen = 1ull << place;
    u32 head = 0, tail = 0, i;
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
    return 0xFF;
}

static bool8 FactsTradeFits(u8 trade, u8 theme, u8 near)
{
    const struct RogueTrade *t = &gRogueTrades[trade];
    if (!((t->themes >> theme) & 1))
        return FALSE;
    return t->needs == 0 || (t->needs & near) != 0;
}

static bool8 FactsLandmarkFits(u8 lm, u8 theme, u8 near)
{
    const struct RogueLandmark *l = &gRogueLandmarks[lm];
    if (!((l->themes >> theme) & 1))
        return FALSE;
    return l->needs == 0 || (l->needs & near) != 0;
}

// What a town has that depends on the towns before it: its trade and
// landmark (a region spreads them: one taken is passed over while another
// fits) and its problem (each of a town's own troubles once a region).
struct FactsCore
{
    u8 trade, landmark, problem, problemAt;
};

// What one pass over the towns keeps.
struct FactsPass
{
    struct RogueRegionFacts rf;
    s8 vx, vy;
    u32 usedTrades, usedLandmarks, usedProblems;
    u8 lost[ROGUE_MAX_DUNGEONS]; // the town a dungeon with someone lost in it is named after, else 0xFF
};

static void FactsTradeOf(const struct RogueRegion *r, struct FactsPass *ps, u8 pl, struct FactsCore *core)
{
    u8 theme = r->places[pl].theme, near = FactsNear(r, pl, ps->vx, ps->vy), sigLm, k;
    u32 rng = RogueRng_Hash(r->seed, FACTS_SALT + 100 + pl), best = 0, score;
    u8 tr = FactsSignature(r, pl, &sigLm), lm = sigLm;
    if (r->places[pl].type == RTOWN_SUMMIT)
    {
        core->trade = RTRADE_COUNT, core->landmark = RLANDMARK_COUNT;
        return;
    }
    if (theme >= RTHEME_COUNT)
        theme = RTHEME_MEADOW;
    if (tr == 0xFF)
    {
        tr = RTRADE_BERRIES;
        for (k = 0; k < RTRADE_COUNT; k++)
        {
            if (!FactsTradeFits(k, theme, near))
                continue;
            score = (((ps->usedTrades >> k) & 1) ? 1 : 16) + RogueRng_Range(&rng, 12)
                  + ((gRogueTrades[k].needs & near) ? 4 : 0);
            if (score > best)
                best = score, tr = k;
        }
    }
    if (lm == 0xFF)
    {
        lm = RLANDMARK_WELL;
        best = 0;
        for (k = 0; k < RLANDMARK_COUNT; k++)
        {
            if (!FactsLandmarkFits(k, theme, near))
                continue;
            // The statue stands only in the famous figure's town.
            if (k == RLANDMARK_STATUE && pl != ps->rf.figureTown)
                continue;
            score = (((ps->usedLandmarks >> k) & 1) ? 1 : 16) + RogueRng_Range(&rng, 10)
                  + (gRogueLandmarks[k].trade == tr ? 10 : 0) + (k == RLANDMARK_STATUE ? 6 : 0);
            if (score > best)
                best = score, lm = k;
        }
    }
    ps->usedTrades |= 1u << tr;
    ps->usedLandmarks |= 1u << lm;
    core->trade = tr, core->landmark = lm;
}

// A blocked road out of town: a link to a route whose obstacle guards the
// side facing this town (the badge clears it). Returns the route or 0xFF.
static u8 FactsBlockedRoad(const struct RogueRegion *r, u8 place)
{
    const struct RoguePlace *p = &r->places[place];
    u32 i;
    for (i = 0; i < p->nLinks; i++)
    {
        const struct RoguePlace *t = &r->places[p->links[i].target];
        const struct RogueLink *back = RogueGen_FindLink(t, place);
        if (t->kind == RPLACE_ROUTE && t->obstacle != ROBST_NONE && back != NULL && back->dir == t->guardDir)
            return p->links[i].target;
    }
    return 0xFF;
}

// What troubles a town now, from what the region holds: the villains in
// town or seen next door, someone lost in a dungeon named after it, a
// legend stirring in its lair, a blocked road, the dark power plant, a
// wreck; else a trouble of its own (the volcano, a rough sea, POK@MON in the
// gardens, a lost pet, the festival not ready), each once in a region, so
// the towns' talk differs. used: RPROB_* bits taken by earlier towns.
static void TownProblem(const struct RogueRegion *r, u8 place, u8 near, u8 festTown, u32 used, const u8 *lost, u8 *prob, u8 *at)
{
    s32 h = RogueGen_DungeonAt(r, place), i;
    u8 hint, mask, blocked, own[5], n = 0;
    u32 rng = RogueRng_Hash(r->seed, FACTS_SALT + 300 + place);
    *at = 0xFF;
    if (r->places[place].type == RTOWN_SUMMIT)
    {
        *prob = RPROB_NONE;
        return;
    }
    // A villain team's hideout under the town (any team's).
    if (h >= 0 && r->dungeons[h].host == place && r->dungeons[h].kind == RDUN_HIDEOUT)
    {
        *prob = RPROB_VILLAINS, *at = h;
        return;
    }
    hint = RogueGen_VillainHint(r, &mask);
    if (hint != 0xFF && RogueGen_FindLink(&r->places[place], hint) != NULL && !((used >> RPROB_VILLAIN_SIGHTED) & 1))
    {
        *prob = RPROB_VILLAIN_SIGHTED, *at = hint;
        return;
    }
    for (i = 0; i < r->nDungeons; i++)
    {
        if (lost[i] == place)
        {
            *prob = RPROB_LOST, *at = i;
            return;
        }
    }
    for (i = 0; i < r->nDungeons; i++)
    {
        if (r->dungeons[i].boss == RBOSS_LEGEND && r->dungeons[i].kind == RDUN_LAIR && r->dungeons[i].host == place
         && r->tunnel != i)
        {
            *prob = RPROB_LEGEND, *at = i;
            return;
        }
    }
    blocked = FactsBlockedRoad(r, place);
    if (blocked != 0xFF && (RogueRng_Range(&rng, 3) || !((used >> RPROB_BLOCKED) & 1)))
    {
        *prob = RPROB_BLOCKED, *at = blocked;
        return;
    }
    for (i = 0; i < r->nDungeons; i++)
    {
        if (RogueGen_TownNear(r, r->dungeons[i].host) != place)
            continue;
        if (r->dungeons[i].kind == RDUN_PLANT && !((used >> RPROB_POWER) & 1))
        {
            *prob = RPROB_POWER, *at = i;
            return;
        }
        if (r->dungeons[i].kind == RDUN_SHIP && !((used >> RPROB_WRECK) & 1))
        {
            *prob = RPROB_WRECK, *at = i;
            return;
        }
    }
    // A trouble of its own, one not heard yet in the region.
    if (place == festTown)
        own[n++] = RPROB_FESTIVAL;
    if (near & RFACT_NEAR_VOLCANO)
        own[n++] = RPROB_VOLCANO;
    if (near & RFACT_NEAR_SEA)
        own[n++] = RPROB_ROUGH_SEA;
    own[n++] = RPROB_PESTS;
    own[n++] = RPROB_PET_LOST;
    h = place == festTown ? 0 : RogueRng_Range(&rng, n); // the festival's town worries about it first
    for (i = 0; i < n; i++)
    {
        u8 pr = own[(i + h) % n];
        if (!((used >> pr) & 1))
        {
            *prob = pr;
            return;
        }
    }
    *prob = RPROB_NONE;
}

// One pass over the towns in order, up to and including place (or all of
// them for 0xFF): each town's core into core[town number].
// ps->vx, ps->vy: the volcano's cell, set by the caller (RogueGen_VolcanoCell
// works out the land: its frame is better not stacked on this one's).
static __attribute__((noinline)) void FactsPassTowns(const struct RogueRegion *r, struct FactsPass *ps, u8 place, struct FactsCore *core)
{
    u32 t, i;
    RogueGen_RegionFacts(r, &ps->rf);
    ps->usedTrades = ps->usedLandmarks = ps->usedProblems = 0;
    for (i = 0; i < ROGUE_MAX_DUNGEONS; i++)
    {
        struct RogueDungeonExtras e;
        const struct RogueDungeon *d = &r->dungeons[i];
        ps->lost[i] = 0xFF;
        if (i >= r->nDungeons || d->kind == RDUN_HIDEOUT || d->kind == RDUN_VICTORY)
            continue;
        RogueGen_DungeonExtras(r, i, &e);
        if (e.explorer != 0xFF)
            ps->lost[i] = RogueGen_TownNear(r, d->host);
    }
    for (t = 0; t < r->nTowns; t++)
    {
        u8 pl = r->towns[t];
        struct FactsCore *c = &core[t];
        FactsTradeOf(r, ps, pl, c);
        TownProblem(r, pl, FactsNear(r, pl, ps->vx, ps->vy), ps->rf.festTown, ps->usedProblems, ps->lost, &c->problem, &c->problemAt);
        ps->usedProblems |= 1u << c->problem;
        if (pl == place)
            return;
    }
}

// A town's facts from a pass that covered it.
static void FactsOf(const struct RogueRegion *r, const struct FactsPass *ps, const struct FactsCore *core, u8 place,
                    struct RogueTownFacts *f)
{
    const struct RoguePlace *p = &r->places[place % ROGUE_MAX_PLACES];
    u32 rng = RogueRng_Hash(r->seed, FACTS_SALT + 200 + place);
    const struct FactsCore *c = &core[p->number % ROGUE_MAX_TOWNS];

    place %= ROGUE_MAX_PLACES;
    f->place = place;
    f->near = FactsNear(r, place, ps->vx, ps->vy);
    f->trade = c->trade;
    f->landmark = c->landmark;
    f->problem = c->problem;
    f->problemAt = c->problemAt;
    f->pride = RogueRng_Range(&rng, 2);
    // The rival town, and what they argue over: the same trade's goods, or
    // their gyms, or the yearly race.
    f->neighbor = FactsNeighbor(r, place);
    f->feud = RFEUD_RACE;
    if (f->neighbor != 0xFF)
    {
        if (core[r->places[f->neighbor].number % ROGUE_MAX_TOWNS].trade == f->trade)
            f->feud = RFEUD_SAME_TRADE;
        else if (p->gym != 0xFF && r->places[f->neighbor].gym != 0xFF)
            f->feud = RogueRng_Range(&rng, 2) ? RFEUD_GYMS : RFEUD_TRADES;
        else
            f->feud = RogueRng_Range(&rng, 3) ? RFEUD_TRADES : RogueRng_Range(&rng, 2) ? RFEUD_RACE : RFEUD_FESTIVAL;
    }
    // The local POK@MON: one of the two commonest on the route out of town.
    f->localRoute = RogueGen_RouteNear(r, place);
    f->localArea = f->localRoute < r->nPlaces ? r->places[f->localRoute].area : 0xFF;
    f->localSlot = RogueRng_Range(&rng, 2);
    if (p->type == RTOWN_SUMMIT)
        f->localArea = 0xFF;
}

// Every town's facts at once (by town number), for whoever needs many.
void RogueGen_AllTownFacts(const struct RogueRegion *r, struct RogueTownFacts *f)
{
    struct FactsPass ps;
    struct FactsCore core[ROGUE_MAX_TOWNS];
    u32 t;
    if (!RogueGen_FactsVolcano(r, &ps.vx, &ps.vy))
        ps.vx = -1;
    FactsPassTowns(r, &ps, 0xFF, core);
    for (t = 0; t < r->nTowns; t++)
        FactsOf(r, &ps, core, r->towns[t], &f[t]);
}

static __attribute__((noinline)) void FactsTown(const struct RogueRegion *r, u8 place, struct RogueTownFacts *f);

void RogueGen_TownFacts(const struct RogueRegion *r, u8 place, struct RogueTownFacts *f)
{
    FactsCacheFor(r);
    if (!(sFactsCache.have & FACTS_HAVE_TOWN) || sFactsCache.townPlace != place)
    {
        FactsTown(r, place, f);
        FactsCacheFor(r);
        sFactsCache.town = *f;
        sFactsCache.townPlace = place;
        sFactsCache.have |= FACTS_HAVE_TOWN;
        return;
    }
    *f = sFactsCache.town;
}

static __attribute__((noinline)) void FactsTown(const struct RogueRegion *r, u8 place, struct RogueTownFacts *f)
{
    struct FactsPass ps;
    struct FactsCore core[ROGUE_MAX_TOWNS];
    if (!RogueGen_FactsVolcano(r, &ps.vx, &ps.vy))
        ps.vx = -1;
    // The neighbour's trade may come later in the order: the pass goes on.
    FactsPassTowns(r, &ps, 0xFF, core);
    FactsOf(r, &ps, core, place, f);
}

const char *RogueGen_TownPride(const struct RogueTownFacts *f)
{
    if (f->trade >= RTRADE_COUNT)
        return "the strongest trainers in the region";
    return gRogueTrades[f->trade].prides[f->pride & 1];
}

// TRUE if the town's landmark stands on its map: the one its signature
// builds. A town whose trade is told only in words has its landmark chosen
// too (from its theme), but nothing of it is drawn, so people do not name
// it: players look for what they hear of.
bool8 RogueGen_LandmarkOnMap(const struct RogueRegion *r, const struct RogueTownFacts *f)
{
    return f->landmark < RLANDMARK_COUNT && RogueGen_SignatureLandmarkId(r, f->place) == f->landmark;
}

// A landmark's name, as said: PINEWICK LIGHTHOUSE, the STATUE of ROSALIND.
char *RogueGen_LandmarkName(const struct RogueRegion *r, const struct RogueTownFacts *f, char *dest)
{
    const char *s;
    if (f->landmark >= RLANDMARK_COUNT)
        return LoreCopy(dest, "the POK@MON LEAGUE");
    s = gRogueLandmarks[f->landmark].name;
    if (s[0] == ' ')
        dest = RogueGen_TownStem(r, f->place, dest);
    for (; *s; s++)
    {
        if (*s == '*')
        {
            struct RogueRegionFacts rf;
            RogueGen_RegionFacts(r, &rf);
            dest = LoreCopy(dest, RogueGen_FigureName(r, &rf));
            continue;
        }
        *dest++ = *s;
    }
    *dest = 0;
    return dest;
}

// ---------------------------------------------------------------------------
// The cast

// Four looks for the wanderer from vanilla trainer sprites (the game maps
// them, src/rogue.c): STEVEN's (a gentleman explorer who collects stones),
// Champion WALLACE's (an elegant one who loves water and art), a COOL
// TRAINER's (an ace who trains everywhere), an EXPERT's (a veteran who has
// walked every road).
static const u8 sLookFemale[RLOOK_COUNT] = { [RLOOK_STEVEN] = 0, [RLOOK_WALLACE] = 0, [RLOOK_ACE] = 1, [RLOOK_VETERAN] = 1 };

static __attribute__((noinline)) void FactsCast(const struct RogueRegion *r, struct RogueCast *c);

void RogueGen_Cast(const struct RogueRegion *r, struct RogueCast *c)
{
    FactsCacheFor(r);
    if (!(sFactsCache.have & FACTS_HAVE_CAST))
    {
        FactsCast(r, c);
        FactsCacheFor(r);
        sFactsCache.cast = *c;
        sFactsCache.have |= FACTS_HAVE_CAST;
        return;
    }
    *c = sFactsCache.cast;
}

static __attribute__((noinline)) void FactsCast(const struct RogueRegion *r, struct RogueCast *c)
{
    u32 rng = RogueRng_Hash(r->seed, FACTS_SALT + 1000), i, k;
    u8 towns[ROGUE_MAX_PLACES];
    u32 n = FactsRoadTowns(r, towns);
    s32 d;

    // The wanderer: met three or four times on the road before the
    // League, as Steven is in Granite Cave, Mossdeep and the Seafloor
    // Cavern: once early, twice in the middle, once late.
    c->wandererLook = RogueRng_Range(&rng, RLOOK_COUNT);
    c->wandererFemale = sLookFemale[c->wandererLook];
    c->wandererName = RogueRng_Range(&rng, LORE_COUNT_OF(sWandererMale));
    for (i = 0; i < 4; i++)
        c->wandererTowns[i] = c->wandererBadges[i] = 0xFF;
    {
        // By the badges passed on the road to reach the town.
        static const u8 lo[4] = { 1, 3, 5, 7 }, hi[4] = { 2, 4, 6, 7 };
        u32 m = 0;
        for (i = 0; i < 4; i++)
        {
            u8 pick[ROGUE_MAX_PLACES];
            u32 np = 0;
            for (k = 0; k < n; k++)
            {
                const struct RoguePlace *p = &r->places[towns[k]];
                if (towns[k] == r->start || p->badges < lo[i] || p->badges > hi[i])
                    continue;
                // Not where the villains hold the town.
                if (RogueGen_HideoutTeam(r, RogueGen_DungeonAt(r, towns[k])) != 0xFF
                 && r->dungeons[RogueGen_DungeonAt(r, towns[k])].host == towns[k])
                    continue;
                if (m > 0 && c->wandererTowns[m - 1] == towns[k])
                    continue;
                pick[np++] = towns[k];
            }
            if (np == 0)
                continue;
            c->wandererTowns[m] = pick[RogueRng_Range(&rng, np)];
            c->wandererBadges[m] = r->places[c->wandererTowns[m]].badges;
            m++;
        }
    }
    // The reporters: in three towns, early, middle and late, as GABBY and
    // TY turn up along the routes.
    c->reporters = RogueRng_Range(&rng, LORE_COUNT_OF(sReporterPairs));
    for (i = 0; i < 3; i++)
    {
        c->reporterTowns[i] = 0xFF;
        if (n > 3)
        {
            u32 lo = 1 + (n - 1) * i / 3, hi = 1 + (n - 1) * (i + 1) / 3;
            c->reporterTowns[i] = towns[lo + RogueRng_Range(&rng, hi > lo ? hi - lo : 1)];
        }
    }
    // The elder lives in the oldest town, where the region's tale is told.
    {
        struct RogueRegionFacts rf;
        RogueGen_RegionFacts(r, &rf);
        c->elderTown = rf.oldTown;
    }
    c->elder = RogueRng_Range(&rng, LORE_COUNT_OF(sElderNames));
    // The explorer: someone lost deep in a dungeon (RogueGen_DungeonExtras),
    // thanked later in the town the dungeon is named after.
    c->explorer = RogueRng_Range(&rng, LORE_COUNT_OF(sExplorerNames));
    c->explorerDungeon = c->explorerTown = 0xFF;
    for (d = 0; d < r->nDungeons; d++)
    {
        struct RogueDungeonExtras e;
        RogueGen_DungeonExtras(r, d, &e);
        if (e.explorer != 0xFF)
        {
            c->explorerDungeon = d;
            c->explorerTown = RogueGen_TownNear(r, r->dungeons[d].host);
            break;
        }
    }
}

const char *RogueGen_WandererName(const struct RogueCast *c)
{
    return c->wandererFemale ? sWandererFemale[c->wandererName % LORE_COUNT_OF(sWandererFemale)]
                             : sWandererMale[c->wandererName % LORE_COUNT_OF(sWandererMale)];
}

const char *RogueGen_ReporterName(const struct RogueCast *c, u8 camera)
{
    return sReporterPairs[c->reporters % LORE_COUNT_OF(sReporterPairs)][camera ? 1 : 0];
}

bool8 RogueGen_ReporterFemale(const struct RogueCast *c, u8 camera)
{
    return camera ? FALSE : sReporterFemale[c->reporters % LORE_COUNT_OF(sReporterPairs)];
}

// GRANNY MAE, GRAMPS ODELL.
char *RogueGen_ElderName(const struct RogueCast *c, char *dest)
{
    u8 i = c->elder % LORE_COUNT_OF(sElderNames);
    return LoreCopy(LoreCopy(dest, sElderFemale[i] ? "GRANNY " : "GRAMPS "), sElderNames[i]);
}

bool8 RogueGen_ElderFemale(const struct RogueCast *c)
{
    return sElderFemale[c->elder % LORE_COUNT_OF(sElderNames)];
}

const char *RogueGen_ExplorerName(const struct RogueCast *c)
{
    return sExplorerNames[c->explorer % LORE_COUNT_OF(sExplorerNames)];
}

bool8 RogueGen_ExplorerFemale(const struct RogueCast *c)
{
    return sExplorerFemale[c->explorer % LORE_COUNT_OF(sExplorerNames)];
}

// The family of a town's house-th house: one name each, never two alike in
// a region.
const char *RogueGen_FamilyName(const struct RogueRegion *r, u8 place, u8 house)
{
    u32 k = (r->places[place % ROGUE_MAX_PLACES].number % ROGUE_MAX_TOWNS) * 7 + house % 7;
    return sFamilyNames[RogueText_Deal(r->seed, FACTS_SALT + 1100, k, LORE_COUNT_OF(sFamilyNames))];
}

// Which meeting with the wanderer (0-3) is in this town, or -1.
s32 RogueGen_WandererAt(const struct RogueCast *c, u8 place)
{
    u32 i;
    for (i = 0; i < 4; i++)
        if (c->wandererTowns[i] == place)
            return i;
    return -1;
}

#undef TH
#undef TH_SEA

// ---------------------------------------------------------------------------
// The dump (host only): the facts in words, for gen_host --script.

#ifdef ROGUE_HOST
static const char *const sProblemNames[RPROB_COUNT] =
{
    "none", "villains hold the town", "villains seen next door", "someone lost in a dungeon", "a legend stirs",
    "the road is blocked", "the lights flicker", "a wreck off the coast", "the volcano rumbles", "the sea is rough",
    "the local POK@MON raid the gardens", "a pet POK@MON wandered off", "the festival isn't ready",
};
static const char *const sFeudNames[RFEUD_COUNT] = { "the yearly race", "the same goods", "whose trade", "whose gym", "the festival" };
static const char *const sLookNames[RLOOK_COUNT] = { "STEVEN", "WALLACE", "COOLTRAINER_F", "EXPERT_F" };

static void TextDump_Facts(struct TextDump *d)
{
    const struct RogueRegion *r = d->r;
    struct RogueRegionFacts rf;
    struct RogueCast cast;
    char a[64], b[64], c[64];
    u32 i;
    if (d->state != NULL)
        return; // the facts do not change with the run
    RogueGen_RegionFacts(r, &rf);
    RogueGen_RegionName(r->seed, a);
    fprintf(d->f, "fact\tregion\t%s\n", a);
    {
        struct RogueTextCtx c;
        char t[200];
        RogueText_Init(&c, r, rf.oldTown, RSPK_NONE);
        RogueText_Fill(t, sizeof(t), "Long ago, {FOUNDING}.", &c);
        fprintf(d->f, "fact\tfounding\t%s\n", t);
        RogueText_Fill(t, sizeof(t), "{FIGURE} the {FIGURE_TITLE} of {FIGURE_TOWN}, who {FIGURE_DEED}", &c);
        fprintf(d->f, "fact\tfigure\t%s\n", t);
    }
    RogueGen_PlaceName(r, rf.oldTown, a);
    fprintf(d->f, "fact\toldtown\t%s\n", a);
    RogueGen_PlaceName(r, rf.festTown, a);
    fprintf(d->f, "fact\tfestival\t%s in %s: %s\n", RogueGen_FestivalName(&rf), a, RogueGen_FestivalAct(&rf));
    for (i = 0; i < r->nPlaces; i++)
    {
        struct RogueTownFacts f;
        if (r->places[i].kind != RPLACE_TOWN || r->places[i].type == RTOWN_SUMMIT)
            continue;
        RogueGen_TownFacts(r, i, &f);
        RogueGen_PlaceName(r, i, a);
        RogueGen_LandmarkName(r, &f, b);
        if (f.neighbor != 0xFF)
            RogueGen_PlaceName(r, f.neighbor, c);
        else
            LoreCopy(c, "-");
        fprintf(d->f, "fact\ttown\t%s\tplace=%u theme=%u near=0x%02x\ttrade=%s (%s, %s)\tlandmark=%s\tpride=%s\tneighbor=%s over %s",
                a, i, r->places[i].theme, f.near, gRogueTrades[f.trade].name, gRogueTrades[f.trade].folk, gRogueTrades[f.trade].goods,
                b, RogueGen_TownPride(&f), c, sFeudNames[f.feud % RFEUD_COUNT]);
        if (f.localArea != 0xFF)
        {
            RogueGen_PlaceName(r, f.localRoute, b);
            RogueText_Runtime(TS_MON, f.localArea, f.localSlot, c);
            fprintf(d->f, "\tlocalmon=%s (area %u slot %u, %s)", c, f.localArea, f.localSlot, b);
        }
        fprintf(d->f, "\tproblem=%s", sProblemNames[f.problem % RPROB_COUNT]);
        if (f.problemAt != 0xFF)
        {
            if (f.problem == RPROB_VILLAIN_SIGHTED || f.problem == RPROB_BLOCKED)
                RogueGen_PlaceName(r, f.problemAt, b);
            else
                RogueGen_DungeonName(r, f.problemAt, b);
            fprintf(d->f, " (%s)", b);
        }
        fprintf(d->f, "\n");
    }
    // The facts must be true to the region (scripttest.py counts these).
    {
        struct RogueTownFacts all[ROGUE_MAX_TOWNS];
        u32 t, meets = 0;
        RogueGen_AllTownFacts(r, all);
        for (t = 0; t < r->nTowns; t++)
        {
            const struct RogueTownFacts *f = &all[t];
            const struct RoguePlace *p = &r->places[f->place];
            if (p->type == RTOWN_SUMMIT)
                continue;
            RogueGen_PlaceName(r, f->place, a);
            // A trade or landmark the town's signature gives (src/rogue_gen_signatures.h)
            // stands on its map: sigtest.py checks where those fit.
            bool8 sigTrade = RogueGen_SignatureTrade(r, f->place) == f->trade;
            bool8 sigLandmark = RogueGen_SignatureLandmarkId(r, f->place) == f->landmark;
            if (!sigTrade && gRogueTrades[f->trade].needs && !(gRogueTrades[f->trade].needs & f->near))
                fprintf(d->f, "fact\terror\t%s: trade %s needs what the town lacks\n", a, gRogueTrades[f->trade].name);
            if (!sigTrade && !((gRogueTrades[f->trade].themes >> p->theme) & 1))
                fprintf(d->f, "fact\terror\t%s: trade %s does not suit its theme\n", a, gRogueTrades[f->trade].name);
            if (!sigLandmark && gRogueLandmarks[f->landmark].needs && !(gRogueLandmarks[f->landmark].needs & f->near))
                fprintf(d->f, "fact\terror\t%s: landmark %s needs what the town lacks\n", a, gRogueLandmarks[f->landmark].name);
            if (f->landmark == RLANDMARK_STATUE && f->place != rf.figureTown)
                fprintf(d->f, "fact\terror\t%s: a statue away from the figure's town\n", a);
            if ((f->problem == RPROB_ROUGH_SEA && !(f->near & RFACT_NEAR_SEA))
             || (f->problem == RPROB_VOLCANO && !(f->near & RFACT_NEAR_VOLCANO))
             || (f->problem == RPROB_FESTIVAL && f->place != rf.festTown))
                fprintf(d->f, "fact\terror\t%s: problem %u does not fit the town\n", a, f->problem);
            if (f->neighbor == f->place)
                fprintf(d->f, "fact\terror\t%s: its own rival\n", a);
        }
        RogueGen_Cast(r, &cast);
        for (t = 0; t < 4; t++)
        {
            if (cast.wandererTowns[t] == 0xFF)
                continue;
            meets++;
            if (cast.wandererTowns[t] == r->start || r->places[cast.wandererTowns[t]].kind != RPLACE_TOWN
             || r->places[cast.wandererTowns[t]].type == RTOWN_SUMMIT)
                fprintf(d->f, "fact\terror\tthe wanderer is met in the wrong place (%u)\n", cast.wandererTowns[t]);
        }
        if (meets < 3)
            fprintf(d->f, "fact\terror\tthe wanderer is met only %u times\n", meets);
    }
    fprintf(d->f, "fact\twanderer\t%s (%s), look %s, met in", RogueGen_WandererName(&cast), cast.wandererFemale ? "she" : "he",
            sLookNames[cast.wandererLook % RLOOK_COUNT]);
    for (i = 0; i < 4; i++)
    {
        if (cast.wandererTowns[i] == 0xFF)
            continue;
        RogueGen_PlaceName(r, cast.wandererTowns[i], a);
        fprintf(d->f, " %s (%u badges)", a, cast.wandererBadges[i]);
    }
    fprintf(d->f, "\n");
    fprintf(d->f, "fact\treporters\t%s and %s, in", RogueGen_ReporterName(&cast, FALSE), RogueGen_ReporterName(&cast, TRUE));
    for (i = 0; i < 3; i++)
        if (cast.reporterTowns[i] != 0xFF)
        {
            RogueGen_PlaceName(r, cast.reporterTowns[i], a);
            fprintf(d->f, " %s", a);
        }
    fprintf(d->f, "\n");
    RogueGen_ElderName(&cast, a);
    RogueGen_PlaceName(r, cast.elderTown, b);
    fprintf(d->f, "fact\telder\t%s of %s\n", a, b);
    if (cast.explorerDungeon != 0xFF)
    {
        RogueGen_DungeonName(r, cast.explorerDungeon, a);
        RogueGen_PlaceName(r, cast.explorerTown, b);
        fprintf(d->f, "fact\texplorer\t%s, lost in %s, thanks you in %s\n", RogueGen_ExplorerName(&cast), a, b);
    }
    else
        fprintf(d->f, "fact\texplorer\tnone lost in this region\n");
    for (i = 0; i < r->nTowns; i++)
    {
        u32 h;
        RogueGen_PlaceName(r, r->towns[i], a);
        fprintf(d->f, "fact\tfamilies\t%s:", a);
        for (h = 0; h < 7; h++)
            fprintf(d->f, " %s", RogueGen_FamilyName(r, r->towns[i], h));
        fprintf(d->f, "\n");
    }
}
#endif
