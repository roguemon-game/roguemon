// RUN 0's trainers and levels (Roguémon v1.0). Part of src/rogue.c, included
// just before Rogue_GetTrainer, whose helpers it uses. Nothing here runs
// outside RUN 0 (Rogue_IsRun0), so normal regions are as they were.
//
// The band: the gyms climb from the party's median level (VAR_ROGUE_RUN0_BAND)
// to 88 by the eighth, the Elite Four stand at 90 to 95 and the Director at
// 100. Everything else (route, cave and gym trainers, villains, dungeon
// masters, wild Pokemon) is the region's own curve mapped onto the band, so
// a road still climbs toward its next gym as in any region.
//
// Teams: four to six Pokemon with held items and the smart battle AI, a
// double battle one time in five from the second act. The eight custom leaders bring teams of
// their type built for these levels. The Pokemon the Director keeps turn up
// on its trainers' teams with the player's nicknames. After a loss, the next
// attempt's trainers lean against the type the player led with most.

#define RUN0_AI (AI_FLAG_BASIC_TRAINER | AI_FLAG_SMART_SWITCHING | AI_FLAG_SMART_MON_CHOICES | AI_FLAG_ASSUME_STAB)
// Bosses switch and choose smartly but do not read the player's moves: that
// is RUN0_PATCH_OMNISCIENT's.
#define RUN0_BOSS_AI (AI_FLAG_BASIC_TRAINER | AI_FLAG_SMART_SWITCHING | AI_FLAG_SMART_MON_CHOICES | AI_FLAG_ASSUME_STAB \
                      | AI_FLAG_ACE_POKEMON)

// RUN 0, and an ENDLESS region a party was carried into (its band set by
// Rogue_Run0StoryNewRun): levels on a band, not the region's own.
bool32 Rogue_Run0Scaled(void)
{
    return Rogue_IsRun0() || (Rogue_IsEndless() && VarGet(VAR_ROGUE_RUN0_BAND) != 0);
}

static u32 Run0Base(void)
{
    u16 band = VarGet(VAR_ROGUE_RUN0_BAND);
    return band != 0 ? ROGUE_RUN0_BAND_BASE(band) : 55;
}

u32 Rogue_Run0AceFor(u32 base, u32 step)
{
    u32 top = base + 8 > 88 ? base + 8 : 88;
    if (top > MAX_LEVEL)
        top = MAX_LEVEL;
    if (step >= ROGUE_NUM_GYMS)
        step = ROGUE_NUM_GYMS - 1;
    return base + ((top - base) * (step + 1) + 4) / 8;
}

u32 Rogue_Run0GymAce(u32 step)
{
    return Rogue_Run0AceFor(Run0Base(), step);
}

// Where the party should stand now: the band's base before the first badge,
// then the ace level of the last gym won.
u32 Rogue_Run0BandLevel(void)
{
    u32 badges = Rogue_BadgeCount();
    return badges == 0 ? Run0Base() : Rogue_Run0GymAce(badges - 1);
}

// Four under it: wild Pokemon (the catches that replace losses) and
// Pokemon taken from the PC are at least this.
u32 Rogue_Run0BandFloor(void)
{
    u32 band = Rogue_Run0BandLevel();
    return band > RUN0_FLOOR_UNDER + 1 ? band - RUN0_FLOOR_UNDER : 2;
}

// The region's curve (its gym aces, then the Champion) onto the band: a
// level between two of its aces lands between the band's two. The first
// route starts ten under the party, as Route 101 is under a starter.
#define RUN0_FIRST_ROUTE_UNDER 12
u32 Rogue_Run0MapLevel(u32 level)
{
    u32 x0 = 0, y0 = Run0Base() - RUN0_FIRST_ROUTE_UNDER, s;
    EnsureRegion();
    for (s = 0; s <= ROGUE_NUM_GYMS; s++)
    {
        u32 x1 = s < ROGUE_NUM_GYMS ? sRegion.aceLevel[s] : sRegion.championLevel;
        u32 y1 = s < ROGUE_NUM_GYMS ? Rogue_Run0GymAce(s) : 92;
        if (x1 <= x0 || y1 < y0)
            continue;
        if (level <= x1)
            return y0 + ((y1 - y0) * (level - x0) + (x1 - x0) / 2) / (x1 - x0);
        x0 = x1;
        y0 = y1;
    }
    level = y0 + (level - x0);
    return level > MAX_LEVEL ? MAX_LEVEL : level;
}

// Wild Pokemon a few levels under the place's trainers, as in any region.
u32 Rogue_Run0WildLevel(u32 placeLevel, u32 normal)
{
    u32 level;
    if (!Rogue_Run0Scaled())
        return normal;
    level = Rogue_Run0MapLevel(placeLevel);
    return level > 6 ? level - 4 : 2;
}

// An area's first wild Pokemon (the catch the Nuzlocke rules allow) stands
// at the band's floor at least, grown for it: a catch can stand in for a
// loss (it came ten under the band). The rest of the area's stay under the
// place's trainers, so walking a route does not wear the party down.
bool32 Rogue_Run0WildCatch(u16 *species, u8 *level)
{
    s32 area;
    u32 floor;
    if (!Rogue_IsRun0())
        return FALSE;
    area = Rogue_GetAreaId();
    if (area < 0 || Rogue_IsAreaCatchUsed(area))
        return FALSE;
    floor = Rogue_Run0BandFloor();
    if (floor > MAX_LEVEL)
        floor = MAX_LEVEL;
    // A little under the floor: it is raised to the floor once caught
    // (Rogue_Run0OnCaught), and the battle for it is not a boss's.
    floor = floor > RUN0_CATCH_UNDER ? floor - RUN0_CATCH_UNDER : 1;
    if (*level < floor)
    {
        *level = floor;
        *species = EvolveForLevel(*species, floor, TRUE);
    }
    return TRUE;
}

// A wild table at RUN 0's levels: each species as far as its level evolves it.
static void Run0WildTable(struct WildPokemon *mons, u32 n)
{
    u32 i;
    for (i = 0; i < n; i++)
        mons[i].species = EvolveForLevel(mons[i].species, mons[i].minLevel, TRUE);
}

// Levels trainers get on top: the cost of the RARE CANDIES.
static u32 Run0TrainerBonus(void)
{
    return Rogue_Run0PatchOn(RUN0_PATCH_COST_CANDY) ? 3 : 0;
}

// The smart battle AI; switching smartly too from the third act.
static u32 Run0Ai(u32 badges)
{
    u32 ai = badges >= 4 ? RUN0_AI : (RUN0_AI & ~AI_FLAG_SMART_SWITCHING);
    return ai | (Rogue_Run0PatchOn(RUN0_PATCH_OMNISCIENT) ? AI_FLAG_OMNISCIENT : 0);
}

// The type a trainer's Pokemon is drawn from: now and then one that hits
// the type the player leads with hard (the last loss's, or the patch's).
static enum Type Run0Lean(u32 *rng, enum Type type)
{
    u16 band = VarGet(VAR_ROGUE_RUN0_BAND);
    u32 against = TYPE_NONE, roll = RogueRng_Range(rng, 100), t, n = 0;
    enum Type counters[NUMBER_OF_MON_TYPES];
    if (Rogue_Run0PatchOn(RUN0_PATCH_TYPE_ANSWERS) && ROGUE_RUN0_BAND_ANSWER(band) != TYPE_NONE && roll < 50)
        against = ROGUE_RUN0_BAND_ANSWER(band);
    else if (ROGUE_RUN0_BAND_LOSS(band) != TYPE_NONE && roll < 35)
        against = ROGUE_RUN0_BAND_LOSS(band);
    if (against == TYPE_NONE || against >= NUMBER_OF_MON_TYPES)
        return type;
    for (t = TYPE_NORMAL; t < NUMBER_OF_MON_TYPES; t++)
        if (t != TYPE_MYSTERY && t != TYPE_STELLAR && HitsHard(t, against))
            counters[n++] = t;
    return n ? counters[RogueRng_Range(rng, n)] : type;
}

// Held items for trainers who think ahead: their type's charm or one of
// these, the ace something to last with. No Quick Claw (a coin flip that
// decides a battle), and a Life Orb only for bosses and the strong few:
// a road's trainers are a toll, not one-hit KOs.
static const u16 sRun0Items[] =
{
    ITEM_SITRUS_BERRY, ITEM_LEFTOVERS, ITEM_LUM_BERRY, ITEM_ORAN_BERRY, ITEM_EXPERT_BELT,
    ITEM_SHELL_BELL, ITEM_MUSCLE_BAND, ITEM_WISE_GLASSES, ITEM_SCOPE_LENS,
};
static const u16 sRun0AceItems[] = { ITEM_SITRUS_BERRY, ITEM_LEFTOVERS, ITEM_LIFE_ORB };
#define RUN0_ROAD_ACE_ITEMS 2 // the first two of sRun0AceItems

static void Run0ItemsSome(u32 size, u32 *rng, bool32 all, bool32 modest)
{
    u32 i;
    for (i = 0; i < size; i++)
    {
        struct TrainerMon *mon = &sTrainerParty[i];
        enum Type t = GetSpeciesType(mon->species, 0);
        // In the first act, the ace and every other one.
        if (mon->heldItem != ITEM_NONE || (!all && i + 1 < size && (size - 1 - i) % 2))
            continue;
        if (i == size - 1)
            mon->heldItem = sRun0AceItems[RogueRng_Range(rng, modest ? RUN0_ROAD_ACE_ITEMS : ARRAY_COUNT(sRun0AceItems))];
        else if (RogueRng_Range(rng, 3) == 0 && t < NUMBER_OF_MON_TYPES && sTypeCharms[t] != ITEM_NONE)
            mon->heldItem = sTypeCharms[t];
        else
            mon->heldItem = sRun0Items[RogueRng_Range(rng, ARRAY_COUNT(sRun0Items))];
    }
}

static void Run0Items(u32 size, u32 *rng)
{
    Run0ItemsSome(size, rng, TRUE, FALSE);
}

// The best four moves of a Pokemon that was the player's (a Hall of Fame
// one, one the Director keeps), as the import gives them, at its level.
static void Run0PlayersMoves(struct TrainerMon *mon)
{
    u16 moves[MAX_MON_MOVES];
    if (Rogue_Run0BestMoves(mon->species, mon->lvl, moves) != 0)
        memcpy(mon->moves, moves, sizeof(mon->moves));
}

// One of the Pokemon the Director keeps, in place of a trainer's Pokemon,
// with the player's nickname (put on when the battle starts): one route or
// gym trainer in four at most, each kept Pokemon once an act, taken in turn
// (Rogue_Run0PickKept; shown when its battle starts), at the level of the
// Pokemon it stands in for (under the trainer's own).
static void Run0KeptOnTeam(u32 slot, u32 size)
{
    u32 k;
    u16 species;
    u32 at;
    if (size < 2 || RogueRng_Hash(Rogue_GetSeed() ^ 0x4B45, slot) % 4 != 0)
        return;
    if (!Rogue_Run0PickKept(&k, &species))
        return;
    at = size - 2;
    sTrainerParty[at].species = species;
    sTrainerParty[at].tags = RUN0_TAG(RUN0_TAG_KEPT, k, 0);
    sTrainerParty[at].heldItem = ITEM_NONE;
    Run0PlayersMoves(&sTrainerParty[at]);
}

// A trainer's battle starts (Rogue_Run0OnBattleStart, the Director loaded):
// the kept Pokemon on its team have been shown this act.
void Rogue_Run0NoteKeptShown(struct RogueDirector *d, u16 trainerId)
{
    const struct Trainer *t;
    s32 slot = TrainerSlot(trainerId);
    u32 i;
    if (!Rogue_IsRun0() || slot < 0 || slot >= ROGUE_SLOT_LEADER_0)
        return;
    t = Rogue_GetTrainer(trainerId);
    for (i = 0; i < t->partySize; i++)
        if ((t->party[i].tags >> RUN0_TAG_SHIFT) == RUN0_TAG_KEPT)
            Rogue_Run0MarkKeptShown(d, (t->party[i].tags >> 16) & 0xFF);
}

// Double battles: one trainer in five from the second act (never the gym's
// trainers: their leader is the fight), four Pokemon at most.
static bool32 Run0Doubles(u32 slot, u32 badges)
{
    return badges >= 2 && RogueRng_Hash(Rogue_GetSeed() ^ 0xD0B1, slot) % 5 == 0;
}

// modest: no Life Orb on the ace (a road's or a gym's trainer).
static void Run0Dress(u32 slot, u32 size, u32 *rng, bool32 kept, u32 badges, bool32 modest, bool32 doubles)
{
    if (kept)
        Run0KeptOnTeam(slot, size);
    Run0ItemsSome(size, rng, badges >= 2, modest);
    sTrainer.aiFlags = Run0Ai(badges);
    if (doubles && size >= 2)
        sTrainer.battleType = TRAINER_BATTLE_TYPE_DOUBLES;
}

// Pokemon whose base stats hide what they become in battle (PALAFIN's
// Hero form, WISHIWASHI's school, MINIOR's core, AEGISLASH's blade,
// SHEDINJA's one HP): a road's middling line that sweeps a party, or
// cannot be touched. Not on RUN 0's roads.
static bool32 Run0FormTrap(u16 species)
{
    switch (SpeciesToNationalPokedexNum(species))
    {
    case NATIONAL_DEX_PALAFIN:
    case NATIONAL_DEX_WISHIWASHI:
    case NATIONAL_DEX_MINIOR:
    case NATIONAL_DEX_AEGISLASH:
    case NATIONAL_DEX_SHEDINJA:
        return TRUE;
    default:
        return FALSE;
    }
}

// A route, cave or gym trainer at RUN 0's strength. The roads are a toll
// on the way to the next leader, not a wall: their aces a few levels under
// where the party should be, three or four Pokemon in the first act and
// four or five after, six only for the strong few (COOLTRAINERS and
// EXPERTS, who look it), the rest two under the ace.
#define RUN0_ROAD_UNDER 2  // under the region's curve mapped onto the band
#define RUN0_ROAD_GAP   5  // the ace at most this far under the next gym's
static u32 Run0RouteTrainer(u32 slot, u32 *rng)
{
    const struct ClassDef *class = TrainerClass(slot);
    enum Type type;
    u32 level, size, iv, i, badges, lo, hi;
    bool32 road = slot < ROGUE_SLOT_GYM_0, strong = FALSE, doubles = FALSE;

    RogueRng_Next(rng); // as the normal builder steps it
    if (!road)
    {
        u32 gym = (slot - ROGUE_SLOT_GYM_0) / ROGUE_GYM_TRAINERS;
        u32 which = (slot - ROGUE_SLOT_GYM_0) % ROGUE_GYM_TRAINERS;
        u32 step = BadgeStep();
        type = sLeaders[sGymLeaders[gym]].type;
        // Three or four each, ten to eight under the leader's ace: the
        // leader is the fight, the trainers a toll on the way.
        level = Rogue_Run0GymAce(step) - 10 + which;
        size = step < 2 ? 3 : 4;
        iv = 16 + step;
        badges = step;
    }
    else
    {
        const struct RoguePlace *p = SlotPlace(slot);
        u32 cap;
        badges = p->badges < ROGUE_NUM_GYMS ? p->badges : ROGUE_NUM_GYMS;
        level = Rogue_Run0MapLevel(p->level + (slot - p->trainerBase) % 3);
        level = level > RUN0_ROAD_UNDER + 2 ? level - RUN0_ROAD_UNDER : 2;
        cap = Rogue_Run0GymAce(badges < ROGUE_NUM_GYMS ? badges : ROGUE_NUM_GYMS - 1);
        cap = cap > RUN0_ROAD_GAP + 2 ? cap - RUN0_ROAD_GAP : 2;
        if (level > cap)
            level = cap;
        type = class->type;
        if (p->kind == RPLACE_CAVE && class->type == TYPE_NONE)
            type = sCaveTypes[RogueRng_Range(rng, ARRAY_COUNT(sCaveTypes))];
        strong = class->talk == TALK_ACE && badges >= 2;
        if (strong)
            size = PARTY_SIZE;
        else
            size = (badges < 2 ? 3 : 4) + RogueRng_Range(rng, 2);
        doubles = Run0Doubles(slot, badges);
        if (doubles && size > 4)
            size = 4;
        iv = 8 + 2 * badges;
    }
    level += Run0TrainerBonus();
    // Stronger lines as the acts go on: the first act's are a League's
    // middling Pokemon, the last act's its best.
    lo = badges < 2 ? 300 : (badges < 4 ? 360 : 420);
    hi = badges < 2 ? 460 : (badges < 4 ? 520 : 600);
    for (i = 0; i < size; i++)
    {
        u32 monLevel = level - (i + 1 < size ? 2 : 0), tries = 0;
        do
        {
            u16 species = PickTrainerSpecies(rng, Run0Lean(rng, type), monLevel, lo, hi, 600, TRUE);
            SetPartyMon(i, species, monLevel, iv, TRUE);
        } while (++tries < 20 && (PartyHasSpecies(i, sTrainerParty[i].species) || Run0FormTrap(sTrainerParty[i].species)));
    }
    // A Life Orb on the ace only for the strong few.
    Run0Dress(slot, size, rng, TRUE, badges, !strong, doubles);
    sTrainer.trainerClass = class->trainerClass;
    sTrainer.encounterMusic = class->music;
    sTrainer.gender = class->gender;
    sTrainer.trainerPic = class->pic;
    if (class->gender == FEMALE)
        StringCopy(sTrainer.trainerName, sFemaleNames[sTrainerNames[slot] % ARRAY_COUNT(sFemaleNames)]);
    else
        StringCopy(sTrainer.trainerName, sMaleNames[sTrainerNames[slot] % ARRAY_COUNT(sMaleNames)]);
    return size;
}

// A team built by the normal builder (villains, dungeon masters, the dojo,
// route features) brought up to RUN 0: its levels mapped, each Pokemon as
// far as its new level evolves it with the moves it knows there, at least
// four of them.
static u32 Run0Raise(u32 slot, u32 size, u32 *rng)
{
    u32 i, bonus = Run0TrainerBonus();
    for (i = 0; i < size; i++)
    {
        struct TrainerMon *mon = &sTrainerParty[i];
        mon->lvl = Rogue_Run0MapLevel(mon->lvl) + bonus;
        if (mon->lvl > MAX_LEVEL)
            mon->lvl = MAX_LEVEL;
        mon->species = EvolveForLevel(mon->species, mon->lvl, TRUE);
        memset(mon->moves, 0, sizeof(mon->moves));
        mon->ability = ABILITY_NONE;
    }
    for (; size < 4 && size > 0; size++)
    {
        u32 level = sTrainerParty[size - 1].lvl, tries = 0;
        enum Type type = GetSpeciesType(sTrainerParty[0].species, 0);
        // Before the ace, which stays last.
        sTrainerParty[size] = sTrainerParty[size - 1];
        do
        {
            SetPartyMon(size - 1, PickTrainerSpecies(rng, Run0Lean(rng, type), level - 1, 380, 600, 600, TRUE), level - 1, 16, TRUE);
        } while (++tries < 20 && (PartyHasSpecies(size - 1, sTrainerParty[size - 1].species)
                                  || sTrainerParty[size - 1].species == sTrainerParty[size].species));
    }
    Run0Dress(slot, size, rng, FALSE, Rogue_BadgeCount(), FALSE, Run0Doubles(slot, Rogue_BadgeCount()));
    return size;
}

// What a boss may hold: one Focus Sash at most, no Quick Claw, and one
// FULL RESTORE in the bag at most (a player beats the team, not the bag).
static void Run0BossLimits(u32 size)
{
    u32 i, sashes = 0, restores = 0;
    for (i = 0; i < size; i++)
    {
        struct TrainerMon *mon = &sTrainerParty[size - 1 - i];
        if (mon->heldItem == ITEM_FOCUS_SASH && sashes++ > 0)
            mon->heldItem = ITEM_SITRUS_BERRY;
        if (mon->heldItem == ITEM_QUICK_CLAW)
            mon->heldItem = ITEM_LEFTOVERS;
    }
    for (i = 0; i < ARRAY_COUNT(sTrainer.items); i++)
        if (sTrainer.items[i] == ITEM_FULL_RESTORE && restores++ > 0)
            sTrainer.items[i] = ITEM_NONE;
}

// ---------------------------------------------------------------------------
// The eight custom leaders: the Director's own children, with teams of
// their type built for RUN 0. Ace last; a team of four, five or six by the
// badges held (the last four, five or six of these).

struct Run0LeaderMon
{
    u16 species;
    u16 item;
    u8 nature;
    u8 under; // levels under the ace
    u16 moves[MAX_MON_MOVES];
};

static const struct Run0LeaderMon sRun0Leaders[8][PARTY_SIZE] =
{
    [0] = { // CRAG, rock
        { SPECIES_CARRACOSTA,       ITEM_WHITE_HERB,   NATURE_ADAMANT, 4, { MOVE_SHELL_SMASH, MOVE_STONE_EDGE, MOVE_AQUA_JET, MOVE_LIQUIDATION } },
        { SPECIES_LYCANROC_MIDDAY,  ITEM_LIFE_ORB,     NATURE_JOLLY,   3, { MOVE_STONE_EDGE, MOVE_CLOSE_COMBAT, MOVE_CRUNCH, MOVE_SWORDS_DANCE } },
        { SPECIES_GIGALITH,         ITEM_LEFTOVERS,    NATURE_ADAMANT, 3, { MOVE_STONE_EDGE, MOVE_EARTHQUAKE, MOVE_HEAVY_SLAM, MOVE_STEALTH_ROCK } },
        { SPECIES_AERODACTYL,       ITEM_FOCUS_SASH,   NATURE_JOLLY,   2, { MOVE_STONE_EDGE, MOVE_EARTHQUAKE, MOVE_CRUNCH, MOVE_ICE_FANG } },
        { SPECIES_RHYPERIOR,        ITEM_EXPERT_BELT,  NATURE_ADAMANT, 2, { MOVE_STONE_EDGE, MOVE_EARTHQUAKE, MOVE_MEGAHORN, MOVE_ICE_PUNCH } },
        { SPECIES_TYRANITAR,        ITEM_CHOPLE_BERRY, NATURE_ADAMANT, 0, { MOVE_STONE_EDGE, MOVE_CRUNCH, MOVE_EARTHQUAKE, MOVE_DRAGON_DANCE } },
    },
    [1] = { // MARINA, water
        { SPECIES_PELIPPER,         ITEM_DAMP_ROCK,    NATURE_MODEST,  4, { MOVE_HURRICANE, MOVE_SCALD, MOVE_ROOST, MOVE_U_TURN } },
        { SPECIES_STARMIE,          ITEM_LIFE_ORB,     NATURE_TIMID,   3, { MOVE_SURF, MOVE_PSYCHIC, MOVE_THUNDERBOLT, MOVE_ICE_BEAM } },
        { SPECIES_SWAMPERT,         ITEM_LEFTOVERS,    NATURE_ADAMANT, 3, { MOVE_WATERFALL, MOVE_EARTHQUAKE, MOVE_ICE_PUNCH, MOVE_STEALTH_ROCK } },
        { SPECIES_KINGDRA,          ITEM_WISE_GLASSES, NATURE_MODEST,  2, { MOVE_SURF, MOVE_DRACO_METEOR, MOVE_ICE_BEAM, MOVE_RAIN_DANCE } },
        { SPECIES_GYARADOS,         ITEM_LUM_BERRY,    NATURE_JOLLY,   2, { MOVE_WATERFALL, MOVE_EARTHQUAKE, MOVE_ICE_FANG, MOVE_DRAGON_DANCE } },
        { SPECIES_MILOTIC,          ITEM_LEFTOVERS,    NATURE_BOLD,    0, { MOVE_SURF, MOVE_ICE_BEAM, MOVE_RECOVER, MOVE_DRAGON_PULSE } },
    },
    [2] = { // BASIL, grass
        { SPECIES_FERROTHORN,       ITEM_LEFTOVERS,    NATURE_RELAXED, 4, { MOVE_POWER_WHIP, MOVE_GYRO_BALL, MOVE_LEECH_SEED, MOVE_STEALTH_ROCK } },
        { SPECIES_BRELOOM,          ITEM_FOCUS_SASH,   NATURE_JOLLY,   3, { MOVE_SPORE, MOVE_SEED_BOMB, MOVE_MACH_PUNCH, MOVE_ROCK_TOMB } },
        { SPECIES_TANGROWTH,        ITEM_SITRUS_BERRY, NATURE_ADAMANT, 3, { MOVE_POWER_WHIP, MOVE_EARTHQUAKE, MOVE_ROCK_SLIDE, MOVE_KNOCK_OFF } },
        { SPECIES_ROSERADE,         ITEM_LIFE_ORB,     NATURE_TIMID,   2, { MOVE_ENERGY_BALL, MOVE_SLUDGE_BOMB, MOVE_SHADOW_BALL, MOVE_SLEEP_POWDER } },
        { SPECIES_DECIDUEYE,        ITEM_EXPERT_BELT,  NATURE_ADAMANT, 2, { MOVE_LEAF_BLADE, MOVE_SHADOW_SNEAK, MOVE_BRAVE_BIRD, MOVE_SWORDS_DANCE } },
        { SPECIES_VENUSAUR,         ITEM_BLACK_SLUDGE, NATURE_MODEST,  0, { MOVE_GIGA_DRAIN, MOVE_SLUDGE_BOMB, MOVE_EARTH_POWER, MOVE_SLEEP_POWDER } },
    },
    [3] = { // CINDER, fire
        { SPECIES_TORKOAL,          ITEM_HEAT_ROCK,    NATURE_BOLD,    4, { MOVE_LAVA_PLUME, MOVE_EARTH_POWER, MOVE_STEALTH_ROCK, MOVE_YAWN } },
        { SPECIES_HOUNDOOM,         ITEM_LIFE_ORB,     NATURE_TIMID,   3, { MOVE_FLAMETHROWER, MOVE_DARK_PULSE, MOVE_SLUDGE_BOMB, MOVE_NASTY_PLOT } },
        { SPECIES_INFERNAPE,        ITEM_FOCUS_SASH,   NATURE_NAIVE,   3, { MOVE_FLARE_BLITZ, MOVE_CLOSE_COMBAT, MOVE_MACH_PUNCH, MOVE_U_TURN } },
        { SPECIES_CHANDELURE,       ITEM_WISE_GLASSES, NATURE_MODEST,  2, { MOVE_FLAMETHROWER, MOVE_SHADOW_BALL, MOVE_ENERGY_BALL, MOVE_CALM_MIND } },
        { SPECIES_VOLCARONA,        ITEM_LUM_BERRY,    NATURE_TIMID,   2, { MOVE_QUIVER_DANCE, MOVE_FIERY_DANCE, MOVE_BUG_BUZZ, MOVE_GIGA_DRAIN } },
        { SPECIES_CHARIZARD,        ITEM_SITRUS_BERRY, NATURE_TIMID,   0, { MOVE_FLAMETHROWER, MOVE_AIR_SLASH, MOVE_DRAGON_PULSE, MOVE_FOCUS_BLAST } },
    },
    [4] = { // VOLTZ, electric
        { SPECIES_ELECTRODE,        ITEM_LIFE_ORB,     NATURE_TIMID,   4, { MOVE_THUNDERBOLT, MOVE_SIGNAL_BEAM, MOVE_CHARGE_BEAM, MOVE_SWIFT } },
        { SPECIES_JOLTEON,          ITEM_LIFE_ORB,     NATURE_TIMID,   3, { MOVE_THUNDERBOLT, MOVE_SHADOW_BALL, MOVE_VOLT_SWITCH, MOVE_SIGNAL_BEAM } },
        { SPECIES_MAGNEZONE,        ITEM_AIR_BALLOON,  NATURE_MODEST,  3, { MOVE_THUNDERBOLT, MOVE_FLASH_CANNON, MOVE_VOLT_SWITCH, MOVE_BODY_PRESS } },
        { SPECIES_LUXRAY,           ITEM_EXPERT_BELT,  NATURE_ADAMANT, 2, { MOVE_WILD_CHARGE, MOVE_CRUNCH, MOVE_ICE_FANG, MOVE_SUPERPOWER } },
        { SPECIES_AMPHAROS,         ITEM_LEFTOVERS,    NATURE_MODEST,  2, { MOVE_THUNDERBOLT, MOVE_DRAGON_PULSE, MOVE_FOCUS_BLAST, MOVE_THUNDER_WAVE } },
        { SPECIES_ELECTIVIRE,       ITEM_SITRUS_BERRY, NATURE_ADAMANT, 0, { MOVE_WILD_CHARGE, MOVE_ICE_PUNCH, MOVE_EARTHQUAKE, MOVE_CROSS_CHOP } },
    },
    [5] = { // MAKO, fighting
        { SPECIES_HITMONTOP,        ITEM_SITRUS_BERRY, NATURE_ADAMANT, 4, { MOVE_CLOSE_COMBAT, MOVE_SUCKER_PUNCH, MOVE_RAPID_SPIN, MOVE_MACH_PUNCH } },
        { SPECIES_HERACROSS,        ITEM_FOCUS_SASH,   NATURE_JOLLY,   3, { MOVE_CLOSE_COMBAT, MOVE_MEGAHORN, MOVE_STONE_EDGE, MOVE_EARTHQUAKE } },
        { SPECIES_CONKELDURR,       ITEM_LEFTOVERS,    NATURE_ADAMANT, 3, { MOVE_DRAIN_PUNCH, MOVE_MACH_PUNCH, MOVE_KNOCK_OFF, MOVE_ICE_PUNCH } },
        { SPECIES_GALLADE,          ITEM_LIFE_ORB,     NATURE_JOLLY,   2, { MOVE_CLOSE_COMBAT, MOVE_PSYCHO_CUT, MOVE_LEAF_BLADE, MOVE_SWORDS_DANCE } },
        { SPECIES_MACHAMP,          ITEM_EXPERT_BELT,  NATURE_ADAMANT, 2, { MOVE_DYNAMIC_PUNCH, MOVE_KNOCK_OFF, MOVE_BULLET_PUNCH, MOVE_ICE_PUNCH } },
        { SPECIES_LUCARIO,          ITEM_LIFE_ORB,     NATURE_TIMID,   0, { MOVE_AURA_SPHERE, MOVE_FLASH_CANNON, MOVE_DARK_PULSE, MOVE_NASTY_PLOT } },
    },
    [6] = { // LUNA, psychic
        { SPECIES_XATU,             ITEM_SITRUS_BERRY, NATURE_TIMID,   4, { MOVE_PSYCHIC, MOVE_AIR_SLASH, MOVE_THUNDER_WAVE, MOVE_ROOST } },
        { SPECIES_SLOWBRO,          ITEM_LEFTOVERS,    NATURE_BOLD,    3, { MOVE_SCALD, MOVE_PSYCHIC, MOVE_SLACK_OFF, MOVE_ICE_BEAM } },
        { SPECIES_ESPEON,           ITEM_LIFE_ORB,     NATURE_TIMID,   3, { MOVE_PSYCHIC, MOVE_SHADOW_BALL, MOVE_DAZZLING_GLEAM, MOVE_CALM_MIND } },
        { SPECIES_REUNICLUS,        ITEM_SITRUS_BERRY, NATURE_MODEST,  2, { MOVE_PSYCHIC, MOVE_FOCUS_BLAST, MOVE_SHADOW_BALL, MOVE_CALM_MIND } },
        { SPECIES_METAGROSS,        ITEM_EXPERT_BELT,  NATURE_ADAMANT, 2, { MOVE_METEOR_MASH, MOVE_ZEN_HEADBUTT, MOVE_EARTHQUAKE, MOVE_BULLET_PUNCH } },
        { SPECIES_ALAKAZAM,         ITEM_FOCUS_SASH,   NATURE_TIMID,   0, { MOVE_PSYCHIC, MOVE_SHADOW_BALL, MOVE_FOCUS_BLAST, MOVE_CALM_MIND } },
    },
    [7] = { // ACE, flying
        { SPECIES_SKARMORY,         ITEM_LEFTOVERS,    NATURE_IMPISH,  4, { MOVE_BRAVE_BIRD, MOVE_IRON_HEAD, MOVE_ROOST, MOVE_SPIKES } },
        { SPECIES_CROBAT,           ITEM_SITRUS_BERRY, NATURE_JOLLY,   3, { MOVE_BRAVE_BIRD, MOVE_CROSS_POISON, MOVE_U_TURN, MOVE_ROOST } },
        { SPECIES_HONCHKROW,        ITEM_LIFE_ORB,     NATURE_ADAMANT, 3, { MOVE_BRAVE_BIRD, MOVE_SUCKER_PUNCH, MOVE_NIGHT_SLASH, MOVE_HEAT_WAVE } },
        { SPECIES_TOGEKISS,         ITEM_LEFTOVERS,    NATURE_TIMID,   2, { MOVE_AIR_SLASH, MOVE_DAZZLING_GLEAM, MOVE_AURA_SPHERE, MOVE_ROOST } },
        { SPECIES_STARAPTOR,        ITEM_EXPERT_BELT,  NATURE_JOLLY,   2, { MOVE_BRAVE_BIRD, MOVE_CLOSE_COMBAT, MOVE_DOUBLE_EDGE, MOVE_U_TURN } },
        { SPECIES_SALAMENCE,        ITEM_LUM_BERRY,    NATURE_JOLLY,   0, { MOVE_DRAGON_CLAW, MOVE_EARTHQUAKE, MOVE_FIRE_FANG, MOVE_DRAGON_DANCE } },
    },
};

// A Pokemon with its moves, nature, item and IVs as given.
static void Run0SetMon(u32 i, u16 species, u32 level, u32 iv, u16 item, u32 nature, const u16 *moves)
{
    struct TrainerMon *mon = &sTrainerParty[i];
    memset(mon, 0, sizeof(*mon));
    mon->species = species;
    mon->lvl = level > MAX_LEVEL ? MAX_LEVEL : level;
    mon->iv = TRAINER_PARTY_IVS(iv, iv, iv, iv, iv, iv);
    mon->ball = BALL_ULTRA;
    mon->gender = TRAINER_MON_RANDOM_GENDER;
    mon->heldItem = item;
    mon->nature = nature;
    if (moves != NULL)
        memcpy(mon->moves, moves, sizeof(mon->moves));
}

static bool32 Run0Knows(const struct TrainerMon *mon, u16 move)
{
    u32 k;
    for (k = 0; k < MAX_MON_MOVES; k++)
        if (mon->moves[k] == move)
            return TRUE;
    return FALSE;
}

// The first three gyms fought hold back: no setting up (a Shell Smash or a
// Dragon Dance on the first leader decides the battle by itself), an attack
// of the Pokemon's own type instead.
static const u16 sRun0SetupMoves[] =
{
    MOVE_SHELL_SMASH, MOVE_SWORDS_DANCE, MOVE_DRAGON_DANCE, MOVE_QUIVER_DANCE, MOVE_CALM_MIND, MOVE_NASTY_PLOT,
};

// An attack for a Pokemon in place of a move, one it does not know already
// (v1.0.0 gave CINDER's HOUNDOOM a second Dark Pulse and LUNA's ESPEON a
// second Psychic): its own types' strong attack, then the best attack the
// import would give it (Rogue_Run0BestMoves) that it does not know; MOVE_NONE
// if none is left.
static u16 Run0FreshAttack(const struct TrainerMon *mon)
{
    u16 best[MAX_MON_MOVES], move;
    u32 k, n;
    for (k = 0; k < 2; k++)
    {
        move = Rogue_Run0AnswerMove(GetSpeciesType(mon->species, k));
        if (move != MOVE_NONE && !Run0Knows(mon, move))
            return move;
    }
    n = Rogue_Run0BestMoves(mon->species, mon->lvl, best);
    for (k = 0; k < n; k++)
        if (GetMoveCategory(best[k]) != DAMAGE_CATEGORY_STATUS && !Run0Knows(mon, best[k])
         && !InList(sRun0SetupMoves, ARRAY_COUNT(sRun0SetupMoves), best[k]))
            return best[k];
    return MOVE_NONE;
}

// The answer to the player's type on a boss's ace, if it can learn it and
// does not know it already (then it needs no other).
static void Run0AceAnswer(u32 size)
{
    u16 band = VarGet(VAR_ROGUE_RUN0_BAND);
    u32 against = Rogue_Run0PatchOn(RUN0_PATCH_TYPE_ANSWERS) ? ROGUE_RUN0_BAND_ANSWER(band) : ROGUE_RUN0_BAND_LOSS(band), t;
    struct TrainerMon *ace = &sTrainerParty[size - 1];
    if (against == TYPE_NONE || against >= NUMBER_OF_MON_TYPES || ace->moves[0] == MOVE_NONE)
        return;
    for (t = TYPE_NORMAL; t < NUMBER_OF_MON_TYPES; t++)
    {
        u16 move = Rogue_Run0AnswerMove(t);
        if (move != MOVE_NONE && HitsHard(t, against) && Run0Knows(ace, move))
            return;
    }
    for (t = TYPE_NORMAL; t < NUMBER_OF_MON_TYPES; t++)
    {
        u16 move = Rogue_Run0AnswerMove(t);
        if (move != MOVE_NONE && HitsHard(t, against) && CanLearnTeachableMove(ace->species, move))
        {
            ace->moves[MAX_MON_MOVES - 1] = move;
            return;
        }
    }
}

static void Run0NoSetup(u32 size)
{
    u32 i, k;
    for (i = 0; i < size; i++)
    {
        struct TrainerMon *mon = &sTrainerParty[i];
        for (k = 0; k < MAX_MON_MOVES; k++)
            if (InList(sRun0SetupMoves, ARRAY_COUNT(sRun0SetupMoves), mon->moves[k]))
                mon->moves[k] = Run0FreshAttack(mon);
    }
}

// A boss's team never knows a move twice: a second copy becomes another
// attack (or nothing, which the battle skips).
static void Run0NoDuplicateMoves(u32 size)
{
    u32 i, j, k;
    for (i = 0; i < size; i++)
    {
        struct TrainerMon *mon = &sTrainerParty[i];
        for (k = 1; k < MAX_MON_MOVES; k++)
        {
            for (j = 0; j < k; j++)
                if (mon->moves[k] != MOVE_NONE && mon->moves[j] == mon->moves[k])
                    break;
            if (j < k)
            {
                mon->moves[k] = MOVE_NONE;
                mon->moves[k] = Run0FreshAttack(mon);
            }
        }
    }
}

static u32 Run0Leader(u32 slot, u32 *rng)
{
    u32 gym = slot - ROGUE_SLOT_LEADER_0, step = BadgeStep(), leaderId = sGymLeaders[gym];
    const struct LeaderDef *leader = &sLeaders[leaderId];
    bool32 remember = Rogue_Run0PatchOn(RUN0_PATCH_LEADERS);
    // The first three stand two levels under the band's aces, as the
    // first three leaders of any region do.
    u32 size = step == 0 ? 4 : (step < 4 ? 5 : PARTY_SIZE);
    // The fifth to seventh gyms fought stand two under the band too: the
    // fair bot's numbers showed a wall there (ROGUEMON.md, RUN 0's challenge).
    u32 ace = Rogue_Run0GymAce(step) - (step < 3 || (step >= 4 && step <= 6) ? 2 : 0) + Run0TrainerBonus() + (remember ? 2 : 0);
    u32 iv = remember ? 31 : 14 + step * 2, i;

    if (leaderId < ARRAY_COUNT(sRun0Leaders))
    {
        // The first of the team (the weaker), then the ace.
        for (i = 0; i < size; i++)
        {
            const struct Run0LeaderMon *m = &sRun0Leaders[leaderId][i + 1 < size ? i : PARTY_SIZE - 1];
            // The rest two under the table's own steps: the ace stands out.
            Run0SetMon(i, IsSpeciesEnabled(m->species) ? m->species : SPECIES_SNORLAX, ace - m->under - (m->under ? 2 : 0), iv, m->item, m->nature,
                       IsSpeciesEnabled(m->species) ? m->moves : NULL);
        }
        Run0AceAnswer(size);
        if (step < 3)
            Run0NoSetup(size);
        Run0NoDuplicateMoves(size);
    }
    else
    {
        // Not one of the eight (only if the region's draw changes): a strong
        // team of the leader's type all the same.
        BuildLeaderTeam(leader, rng, size, ace, 600, TRUE, iv);
        Run0Items(size, rng);
    }
    sTrainer.items[0] = step < 4 ? ITEM_HYPER_POTION : ITEM_FULL_RESTORE;
    if (step >= 6)
        sTrainer.items[1] = ITEM_HYPER_POTION;
    Run0BossLimits(size);
    sTrainer.aiFlags = RUN0_BOSS_AI | (Rogue_Run0PatchOn(RUN0_PATCH_OMNISCIENT) ? AI_FLAG_OMNISCIENT : 0);
    sTrainer.trainerClass = TRAINER_CLASS_LEADER;
    sTrainer.encounterMusic = (leader->gender == FEMALE) ? TRAINER_ENCOUNTER_MUSIC_FEMALE : TRAINER_ENCOUNTER_MUSIC_MALE;
    sTrainer.gender = leader->gender;
    sTrainer.trainerPic = leader->pic;
    // The sixth and eighth gyms fought are double battles.
    if (step == 5 || step == 7 || leader->doubles)
        sTrainer.battleType = TRAINER_BATTLE_TYPE_DOUBLES;
    StringCopy(sTrainer.trainerName, leader->name);
    return size;
}

// ---------------------------------------------------------------------------
// The player's own past, as the Director fields it

// The player's name and picture, for the Previous Player, the Copy and the
// Director (which has no picture of its own: it shows yours, darkened).
// The Previous Player wears Ruby and Sapphire's picture (an older you).
static void Run0AsPlayer(u32 class)
{
    bool32 previous = class == TRAINER_CLASS_PKMN_TRAINER_1;
    sTrainer.trainerClass = class;
    sTrainer.gender = gSaveBlock2Ptr->playerGender;
    if (gSaveBlock2Ptr->playerGender == MALE)
        sTrainer.trainerPic = previous ? TRAINER_PIC_RS_BRENDAN : TRAINER_PIC_BRENDAN;
    else
        sTrainer.trainerPic = previous ? TRAINER_PIC_RS_MAY : TRAINER_PIC_MAY;
    sTrainer.encounterMusic = TRAINER_ENCOUNTER_MUSIC_MALE;
    StringCopy(sTrainer.trainerName, gSaveBlock2Ptr->playerName);
}

static void Run0EvolvedMon(u32 i, u16 species, u32 level, u32 iv, u32 tags)
{
    u32 kind = tags >> RUN0_TAG_SHIFT;
    Run0SetMon(i, EvolveForLevel(species, level, TRUE), level, iv, ITEM_NONE, 0, NULL);
    sTrainerParty[i].tags = tags;
    // The player's own Pokemon fight with the four a player would keep (not
    // the last four learned: Double Team, Roar).
    if (kind == RUN0_TAG_HOF || kind == RUN0_TAG_KEPT || kind == RUN0_TAG_RUNLOG)
        Run0PlayersMoves(&sTrainerParty[i]);
}

// The first player, for a player with no Hall of Fame (20 losses): not a
// mirror of TEST TEAM but somebody's first champion team from the old games,
// kept by the Director. None of Terry's, CHAMPION TERRY's or the starters
// PROF. BIRCH brings. The ace last.
static const struct Run0LeaderMon sFirstPlayer[PARTY_SIZE] =
{
    // A Gen 3 player's team: the moves and items of the old games, no
    // setting up (the fair bot's League runs all ended here at 95 with
    // Spore, Dragon Dance and Swords Dance on it).
    // No Roar, no Quick Claws, no Fly (a turn in the air): its four attacks.
    { SPECIES_MANECTRIC, ITEM_MAGNET,       NATURE_TIMID,   0, { MOVE_THUNDERBOLT, MOVE_CRUNCH, MOVE_QUICK_ATTACK, MOVE_FLAMETHROWER } },
    { SPECIES_BRELOOM,   ITEM_SCOPE_LENS,   NATURE_ADAMANT, 0, { MOVE_SKY_UPPERCUT, MOVE_SEED_BOMB, MOVE_MACH_PUNCH, MOVE_HEADBUTT } },
    { SPECIES_ALTARIA,   ITEM_SHELL_BELL,   NATURE_ADAMANT, 0, { MOVE_DRAGON_CLAW, MOVE_AERIAL_ACE, MOVE_EARTHQUAKE, MOVE_FLAMETHROWER } },
    { SPECIES_ABSOL,     ITEM_SCOPE_LENS,   NATURE_JOLLY,   0, { MOVE_NIGHT_SLASH, MOVE_PSYCHO_CUT, MOVE_ROCK_SLIDE, MOVE_QUICK_ATTACK } },
    { SPECIES_AGGRON,    ITEM_LEFTOVERS,    NATURE_ADAMANT, 0, { MOVE_IRON_TAIL, MOVE_EARTHQUAKE, MOVE_ROCK_SLIDE, MOVE_TAKE_DOWN } },
    { SPECIES_MILOTIC,   ITEM_SITRUS_BERRY, NATURE_BOLD,    0, { MOVE_SURF, MOVE_ICE_BEAM, MOVE_RECOVER, MOVE_TWISTER } },
};

// The Hall of Fame's first team (the Previous Player's), as many as size,
// at level; the team's ace (its highest level) last. With no Hall of Fame,
// the first player's (sFirstPlayer).
static u32 Run0FirstTeam(u32 size, u32 level, u32 iv)
{
    u16 species[PARTY_SIZE];
    u8 levels[PARTY_SIZE];
    u32 n = Rogue_Run0HofTeam(0, species, levels), i, ace = 0;
    if (n != 0)
    {
        for (i = 1; i < n; i++)
            if (levels[i] > levels[ace])
                ace = i;
        if (size > n)
            size = n;
        // The ace and the first others of the team.
        for (i = 0; i + 1 < size; i++)
        {
            u32 k = i < ace ? i : i + 1;
            Run0EvolvedMon(i, species[k], level - 1, iv, RUN0_TAG(RUN0_TAG_HOF, 0, k));
        }
        Run0EvolvedMon(size - 1, species[ace], level, iv, RUN0_TAG(RUN0_TAG_HOF, 0, ace));
        return size;
    }
    if (size > PARTY_SIZE)
        size = PARTY_SIZE;
    for (i = 0; i < size; i++)
    {
        const struct Run0LeaderMon *m = &sFirstPlayer[i + 1 < size ? i : PARTY_SIZE - 1];
        Run0SetMon(i, m->species, level - (i + 1 < size), iv, m->item, m->nature, m->moves);
    }
    return size;
}

// The Previous Player as the rival: the first champion team, a few more
// each battle, at the band's level for the battle.
static u32 Run0Rival(u32 slot, u32 *rng)
{
    u32 battle = (slot - ROGUE_SLOT_RIVAL_0) & 3;
    u32 level = Rogue_Run0MapLevel(sRegion.rivalLevel[battle]) + Run0TrainerBonus();
    u32 size = Run0FirstTeam(3 + battle, level, 16 + battle * 4);
    Run0Items(size, rng);
    Run0BossLimits(size);
    sTrainer.aiFlags = RUN0_BOSS_AI | (Rogue_Run0PatchOn(RUN0_PATCH_OMNISCIENT) ? AI_FLAG_OMNISCIENT : 0);
    Run0AsPlayer(TRAINER_CLASS_PKMN_TRAINER_1);
    return size;
}

// The three starters of a region (as GenerateContent draws them).
static void Run0RegionStarters(u32 seed, u16 *out)
{
    u32 rng = RogueRng_Hash(seed, 7), i;
    for (i = 0; i < 19; i++)
        RogueRng_Next(&rng);
    for (i = 0; i < 3; i++)
        out[i] = PickEnabled(&rng, sStarters[i], ARRAY_COUNT(sStarters[0]));
}

// The Elite Four: the Fallen, the Previous Player, PROF. BIRCH, the Copy.
static u32 Run0EliteFour(u32 slot, u32 *rng)
{
    u32 member = slot - ROGUE_SLOT_E4_0, size = 0, i, bonus = Run0TrainerBonus();
    u16 species[ROGUE_KEPT_MAX];
    u8 levels[ROGUE_KEPT_MAX], a[ROGUE_KEPT_MAX], b[ROGUE_KEPT_MAX];

    sTrainer.aiFlags = RUN0_BOSS_AI | (Rogue_Run0PatchOn(RUN0_PATCH_OMNISCIENT) ? AI_FLAG_OMNISCIENT : 0);
    sTrainer.encounterMusic = TRAINER_ENCOUNTER_MUSIC_ELITE_FOUR;
    switch (member)
    {
    case 0:
    {
        // The Fallen: what the Director keeps (Terry's first), then the
        // Pokemon lost in past runs, with their nicknames. "I kept them."
        u32 n = Rogue_Run0KeptMons(species, levels, a, PARTY_SIZE);
        for (i = 0; i < n && size < PARTY_SIZE; i++)
            Run0EvolvedMon(size++, species[i], RUN0_LEVEL_FALLEN + bonus, 24, RUN0_TAG(RUN0_TAG_KEPT, a[i], 0));
        if (size < PARTY_SIZE)
        {
            n = Rogue_Run0LostMons(species, levels, a, b, PARTY_SIZE - size);
            for (i = 0; i < n && size < PARTY_SIZE; i++)
                Run0EvolvedMon(size++, species[i], RUN0_LEVEL_FALLEN + bonus, 24, RUN0_TAG(RUN0_TAG_RUNLOG, a[i], b[i]));
        }
        // Nothing lost yet: the Pokemon it lost itself, the ones that
        // never made it out of a draft.
        for (i = 0; size < 4; i++)
        {
            static const u16 sDrafts[] = { SPECIES_DUSKNOIR, SPECIES_BANETTE, SPECIES_SPIRITOMB, SPECIES_COFAGRIGUS };
            Run0EvolvedMon(size++, sDrafts[i % ARRAY_COUNT(sDrafts)], RUN0_LEVEL_FALLEN + bonus, 24, 0);
        }
        sTrainer.trainerClass = TRAINER_CLASS_ELITE_FOUR;
        sTrainer.gender = FEMALE;
        sTrainer.trainerPic = TRAINER_PIC_HEX_MANIAC;
        StringCopy(sTrainer.trainerName, COMPOUND_STRING("FALLEN"));
        break;
    }
    case 1:
        // The Previous Player, last time: the first champion team at 95.
        size = Run0FirstTeam(PARTY_SIZE, RUN0_LEVEL_PREVIOUS + bonus, 28);
        Run0AsPlayer(TRAINER_CLASS_PKMN_TRAINER_1);
        sTrainer.encounterMusic = TRAINER_ENCOUNTER_MUSIC_ELITE_FOUR;
        break;
    case 2:
    {
        // PROF. BIRCH with the three starters of the first run known.
        u16 starters[3] = { SPECIES_TREECKO, SPECIES_TORCHIC, SPECIES_MUDKIP };
        s32 seed = Rogue_Run0FirstRunSeed();
        if (seed >= 0)
            Run0RegionStarters(seed, starters);
        for (i = 0; i < 3; i++)
        {
            Run0EvolvedMon(i, starters[i], RUN0_LEVEL_BIRCH + bonus - (i < 2), 31, 0);
            sTrainerParty[i].heldItem = i == 2 ? ITEM_SITRUS_BERRY : ITEM_LEFTOVERS;
        }
        size = 3;
        sTrainer.items[0] = ITEM_FULL_RESTORE;
        sTrainer.items[1] = ITEM_HYPER_POTION;
        sTrainer.trainerClass = TRAINER_CLASS_PKMN_PROF_FRLG;
        sTrainer.gender = MALE;
        // He never had a battle picture: a scientist's.
        sTrainer.trainerPic = TRAINER_PIC_SCIENTIST_FRLG;
        StringCopy(sTrainer.trainerName, COMPOUND_STRING("BIRCH"));
        break;
    }
    default:
        // The Copy: the player's party, exactly, made when the battle
        // starts (Rogue_Run0FinishTrainerParty); these stand in until then.
        for (i = 0; i < PARTY_SIZE; i++)
        {
            struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
            // As fit as the party: one that has fainted is not copied.
            if (!GetMonData(mon, MON_DATA_SANITY_HAS_SPECIES) || GetMonData(mon, MON_DATA_IS_EGG) || GetMonData(mon, MON_DATA_HP) == 0)
                continue;
            Run0SetMon(size, GetMonData(mon, MON_DATA_SPECIES), GetMonData(mon, MON_DATA_LEVEL), 31, ITEM_NONE, 0, NULL);
            sTrainerParty[size].tags = RUN0_TAG(RUN0_TAG_COPY, i, 0);
            size++;
        }
        if (size == 0)
            Run0SetMon(size++, SPECIES_DITTO, RUN0_LEVEL_FALLEN, 31, ITEM_NONE, 0, NULL);
        Run0AsPlayer(TRAINER_CLASS_ELITE_FOUR);
        sTrainer.aiFlags = AI_FLAG_SMART_TRAINER;
        sTrainer.encounterMusic = TRAINER_ENCOUNTER_MUSIC_ELITE_FOUR;
        return size;
    }
    if (member != 2)
        Run0Items(size, rng);
    if (member != 2)
        sTrainer.items[0] = ITEM_FULL_RESTORE;
    Run0BossLimits(size);
    return size;
}

void Rogue_Run0FreshTrainer(u16 trainerId)
{
    s32 slot = TrainerSlot(trainerId);
    if (Rogue_IsRun0() && (slot == ROGUE_SLOT_CHAMPION || slot == ROGUE_SLOT_E4_0 + 3))
        sTrainerCachedId = 0;
}

u32 Rogue_Run0DirectorPhase(void)
{
    return FlagGet(TRAINER_FLAGS_START + ROGUE_TRAINER(ROGUE_SLOT_CHAMPION)) ? 2 : 1;
}

// The Director: PORYGON-Z, the man-made Pokemon whose upgrade made it
// unstable, and the Pokemon that beat the player's party, all at 100.
// Phase 2 ("PATCH 1.0.1: Fixed a bug where the player could win."): the ace
// of every Hall of Fame team.
static const u16 sPorygonZMoves[MAX_MON_MOVES] = { MOVE_TRI_ATTACK, MOVE_THUNDERBOLT, MOVE_ICE_BEAM, MOVE_NASTY_PLOT };

static u32 Run0Director(u32 *rng)
{
    u32 size = 0, i;
    if (Rogue_Run0DirectorPhase() == 2)
    {
        u16 species[PARTY_SIZE];
        u8 team[PARTY_SIZE], mon[PARTY_SIZE];
        u32 n = Rogue_Run0HofAces(species, team, mon, PARTY_SIZE);
        for (i = 0; i < n; i++)
            Run0EvolvedMon(size++, species[i], RUN0_LEVEL_DIRECTOR, 31, RUN0_TAG(RUN0_TAG_HOF, team[i], mon[i]));
        // No champion teams: Terry's, which know the way.
        for (i = 0; size < 3; i++)
        {
            u8 lv;
            Run0EvolvedMon(size++, Rogue_Run0TerrySpecies(i, &lv), RUN0_LEVEL_DIRECTOR, 31, RUN0_TAG(RUN0_TAG_TERRY, i, 0));
        }
        Run0Items(size, rng);
    }
    else
    {
        // Against each of the player's Pokemon, the strongest first, one that
        // hits it hard.
        u32 order[PARTY_SIZE], n = 0, j;
        for (i = 0; i < PARTY_SIZE; i++)
        {
            struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
            if (!GetMonData(mon, MON_DATA_SANITY_HAS_SPECIES) || GetMonData(mon, MON_DATA_IS_EGG))
                continue;
            for (j = n; j > 0 && GetMonData(&gParties[B_TRAINER_PLAYER][order[j - 1]], MON_DATA_LEVEL) < GetMonData(mon, MON_DATA_LEVEL); j--)
                order[j] = order[j - 1];
            order[j] = i;
            n++;
        }
        for (i = 0; i < n && size < PARTY_SIZE - 1; i++)
        {
            u16 target = GetMonData(&gParties[B_TRAINER_PLAYER][order[i]], MON_DATA_SPECIES);
            u32 type = Rogue_Run0AnswerType(target, RogueRng_Next(rng)), tries = 0;
            do
            {
                u16 s = PickTrainerSpecies(rng, type, RUN0_LEVEL_DIRECTOR, 470, 600, 600, TRUE);
                Run0EvolvedMon(size, s, RUN0_LEVEL_DIRECTOR, 31, 0);
            } while (++tries < 20 && (PartyHasSpecies(size, sTrainerParty[size].species) || sTrainerParty[size].species == SPECIES_PORYGON_Z));
            size++;
        }
        Run0Items(size, rng);
        Run0SetMon(size++, SPECIES_PORYGON_Z, RUN0_LEVEL_DIRECTOR, 31, ITEM_LIFE_ORB, NATURE_MODEST, sPorygonZMoves);
    }
    sTrainer.items[0] = ITEM_FULL_RESTORE;
    sTrainer.items[1] = ITEM_HYPER_POTION;
    Run0BossLimits(size);
    sTrainer.aiFlags = RUN0_BOSS_AI | (Rogue_Run0PatchOn(RUN0_PATCH_OMNISCIENT) ? AI_FLAG_OMNISCIENT : 0);
    Run0AsPlayer(TRAINER_CLASS_CHAMPION);
    sTrainer.encounterMusic = TRAINER_ENCOUNTER_MUSIC_MALE;
    StringCopy(sTrainer.trainerName, COMPOUND_STRING("DIRECTOR"));
    return size;
}

// RUN 0's own teams; 0 for a trainer the normal builder makes (then
// raised by Run0Raise).
static u32 Run0BuildTrainer(s32 slot, u32 *rng)
{
    if (slot < 0 || VillainClass(slot) != NULL || IsFeatureSlot(slot) || IsDojoSlot(slot) || MasterDungeon(slot) >= 0)
        return 0;
    if (slot == ROGUE_SLOT_CHAMPION)
        return Run0Director(rng);
    if (slot >= ROGUE_SLOT_RIVAL_0)
        return slot < ROGUE_SLOT_RIVAL_0 + 4 ? Run0Rival(slot, rng) : 0;
    if (slot >= ROGUE_SLOT_E4_0)
        return Run0EliteFour(slot, rng);
    if (slot >= ROGUE_SLOT_LEADER_0)
        return Run0Leader(slot, rng);
    return Run0RouteTrainer(slot, rng);
}

// run0test.py (gRogueTestTrainer 0xFFFA): the Copy's party made as its
// battle makes it. VAR_0x8004 the Pokemon made, VAR_RESULT those that could
// Dynamax or Terastallize or whose HP is not the party's.
static void Run0TestCopy(void)
{
    u16 id = ROGUE_TRAINER(ROGUE_SLOT_E4_0 + 3);
    struct Pokemon *foes = gParties[B_TRAINER_OPPONENT_A];
    const struct Trainer *t;
    u32 i, bad = 0;
    Rogue_Run0FreshTrainer(id);
    t = Rogue_GetTrainer(id);
    CreateNPCTrainerPartyFromTrainer(foes, t);
    for (i = 0; i < t->partySize; i++)
    {
        u32 slot = (t->party[i].tags >> 16) & 0xFF;
        if (GetMonData(&foes[i], MON_DATA_DYNAMAX_LEVEL) != BLOCK_AI_DYNAMAX
         || GetMonData(&foes[i], MON_DATA_TERA_TYPE) != TYPE_MYSTERY
         || slot >= PARTY_SIZE
         || GetMonData(&foes[i], MON_DATA_HP) != GetMonData(&gParties[B_TRAINER_PLAYER][slot], MON_DATA_HP))
            bad++;
    }
    gSpecialVar_0x8004 = t->partySize;
    gSpecialVar_Result = bad;
}

// run0test.py: an area's catch of species VAR_0x8004 (0xFFFB), made as
// CreateWildMon makes one at the band's floor, its moves read back.
static void Run0TestCatch(void)
{
    struct Pokemon *mon = &gParties[B_TRAINER_OPPONENT_A][0];
    u32 floor = Rogue_Run0BandFloor();
    ZeroEnemyPartyMons();
    CreateMon(mon, gSpecialVar_0x8004, floor > MAX_LEVEL ? MAX_LEVEL : floor, 0x12345, OTID_STRUCT_PLAYER_ID);
    GiveMonInitialMoveset(mon);
    Rogue_Run0OnWildMon(mon, TRUE);
}

// run0test.py: trainer VAR_0x8004's party made (0xFFF9), as its battle
// makes it (the player's Pokemon on it finished). VAR_RESULT: the Pokemon
// that could Dynamax, Terastallize, Mega Evolve or use a Z-Move.
static void Run0TestParty(void)
{
    struct Pokemon *foes = gParties[B_TRAINER_OPPONENT_A];
    const struct Trainer *t = Rogue_GetTrainer(gSpecialVar_0x8004);
    u32 i, bad = 0;
    ZeroEnemyPartyMons();
    CreateNPCTrainerPartyFromTrainer(foes, t);
    for (i = 0; i < t->partySize && i < PARTY_SIZE; i++)
    {
        enum HoldEffect hold = GetItemHoldEffect(GetMonData(&foes[i], MON_DATA_HELD_ITEM));
        if (GetMonData(&foes[i], MON_DATA_DYNAMAX_LEVEL) != BLOCK_AI_DYNAMAX
         || GetMonData(&foes[i], MON_DATA_TERA_TYPE) != TYPE_MYSTERY
         || hold == HOLD_EFFECT_MEGA_STONE || hold == HOLD_EFFECT_Z_CRYSTAL)
            bad++;
    }
    gSpecialVar_Result = bad;
}
