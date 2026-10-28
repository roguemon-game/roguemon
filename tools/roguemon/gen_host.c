// Host build of the region generator for previews and checks.
// Usage: gen_host SEED OUTDIR
// Writes OUTDIR/mapN.bin (one per place) and OUTDIR/region.txt describing the
// plan, each map's size and its events.
// gen_host --script SEED [OUTFILE] prints every line of text the region can
// show (src/rogue_gen_text.h), in three states of a run, for scripttest.py.
// ROGUE_TEXT_LONGEST=1 fills the game's values (ROGUE_TEXT_MARK=1 marks them) (the player's name, species)
// with the longest there are; ROGUE_WILD=FILE gives the region's wild
// species ("area slot NAME" lines, read from the ROM by scripttest.py --wild).
#define ROGUE_HOST
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/rogue_gen.c"

static struct RogueRegion sRegion;
static struct RogueMap sMap;
static u16 sBlocks[ROGUE_MAX_BLOCKS];
static FILE *sItemOut;
static u8 sHostVisit;
static u8 sTownLore[ROGUE_MAX_PLACES][5]; // townsfolk, a PC, the elder's home, a town sign, a dungeon sign

// Ruins shift on every visit: the visit to make their floors for is
// ROGUE_VISIT (0 by default), as the game counts them in VAR_ROGUE_SEALS.
u8 RogueGen_DungeonVisit(void)
{
    return sHostVisit;
}

// Every item the run hands out (src/rogue_gen_items.h), for item_tally.py.
static void ItemSink(u32 source, u32 index, u16 item, u8 qty, u32 stage, u32 taste)
{
    fprintf(sItemOut, "item src=%u index=%u item=%u qty=%u stage=%u taste=%d role=%u\n", source, index, item, qty, stage,
            taste == 0xFF ? -1 : (int)taste, RogueGen_ItemRole(item));
}

// Values only the game knows (RogueText_Runtime): samples to read, or the
// longest there are for the width checks.
static char sWildNames[ROGUE_MAX_AREAS][6][16];
static int sTextLongest;

char *RogueText_Runtime(u8 slot, u8 a, u8 b, char *dest)
{
    const char *s = NULL;
    char buf[32];
    // ROGUE_TEXT_MARK=1 (texttest.py): the values only the game's own state
    // gives (the player, the rival, the lead) as ~SLOT~, to match anything.
    if (getenv("ROGUE_TEXT_MARK") && slot != TS_MON)
    {
        snprintf(buf, sizeof(buf), "~%u~", slot);
        s = buf;
        while (*s)
            *dest++ = *s++;
        *dest = 0;
        return dest;
    }
    switch (slot)
    {
    case TS_PLAYER:    s = sTextLongest ? "MMMMMMM" : "BRENDAN"; break;  // 7 letters at most
    case TS_RIVAL:     s = sTextLongest ? "BRENDAN" : "MAY"; break;
    case TS_LEAD:      s = sTextLongest ? "CRABOMINABLE" : "MARSHTOMP"; break; // 12 letters at most
    case TS_RIVAL_MON: s = sTextLongest ? "CRABOMINABLE" : "TREECKO"; break;
    // A trainer's own lead: only the game builds teams.
    case TS_OWN:
        if (sTextLongest)
            s = "CRABOMINABLE";
        else
        {
            snprintf(buf, sizeof(buf), "OWNMON%u", a);
            s = buf;
        }
        break;
    case TS_MON:
        if (sTextLongest)
            s = "CRABOMINABLE";
        else if (a < ROGUE_MAX_AREAS && b < 6 && sWildNames[a][b][0])
            s = sWildNames[a][b];
        else
        {
            snprintf(buf, sizeof(buf), "WILDMON%u.%u", a, b);
            s = buf;
        }
        break;
    default:
        return NULL;
    }
    while (*s)
        *dest++ = *s++;
    *dest = 0;
    return dest;
}

static void LoadWild(void)
{
    const char *path = getenv("ROGUE_WILD");
    FILE *f;
    unsigned a, b;
    char name[32];
    if (path == NULL || (f = fopen(path, "r")) == NULL)
        return;
    while (fscanf(f, "%u %u %31s", &a, &b, name) == 3)
    {
        char *c;
        for (c = name; *c; c++)
            if (*c == '_')
                *c = ' ';
        if (a < ROGUE_MAX_AREAS && b < 6)
            snprintf(sWildNames[a][b], sizeof(sWildNames[a][b]), "%s", name);
    }
    fclose(f);
}

// Every line the region can show, at the start of a run, early on (a
// starter, no badges) and late (eight badges, the villains beaten).
static int DumpScript(u32 seed, const char *path)
{
    static struct RogueTextState early, late;
    FILE *f = path ? fopen(path, "w") : stdout;
    if (f == NULL)
        return 1;
    sTextLongest = getenv("ROGUE_TEXT_LONGEST") && atoi(getenv("ROGUE_TEXT_LONGEST"));
    LoadWild();
    RogueGen_BuildRegion(&sRegion, seed);
    early.flags = RTSTATE_STARTER;
    late.flags = RTSTATE_STARTER | RTSTATE_RIVAL1 | RTSTATE_HIDEOUT | RTSTATE_VLEADER;
    late.badges = 8;
    late.wonGyms = 0xFF;
    late.bosses = 0xFFFF;
    late.rescued = 0xFFFF;
    late.teamsCleared = 0xFF;
    late.teamsBeaten = 0xFF;
    {
        char name[ROGUE_REGION_NAME_LEN + 1];
        RogueGen_RegionName(seed, name);
        fprintf(f, "region\t%u\t%s\tlongest=%d\n", seed, name, sTextLongest);
    }
    RogueText_DumpRegion(&sRegion, f, NULL, "start");
    RogueText_DumpRegion(&sRegion, f, &early, "early");
    RogueText_DumpRegion(&sRegion, f, &late, "late");
    if (path)
        fclose(f);
    return 0;
}

int main(int argc, char **argv)
{
    // --run0 stands for RUN 0's seed (include/rogue_run0_region.h); ROGUE_RUN0_ACT,
    // ROGUE_RUN0_VISIT and ROGUE_RUN0_STATE give the story's state.
    u32 seed = argc > 1 ? (strcmp(argv[1], "--run0") == 0 ? ROGUE_RUN0_SEED : strtoul(argv[1], NULL, 0)) : 1;
    const char *dir = argc > 2 ? argv[2] : ".";
    char path[512];
    FILE *f;
    int i, j;

    if (argc > 2 && strcmp(argv[1], "--script") == 0)
        return DumpScript(strtoul(argv[2], NULL, 0), argc > 3 ? argv[3] : NULL);
    if (argc > 2 && strcmp(argv[1], "--trainers") == 0)
    {
        // Every class trainer of a region (route, cave, dungeon and gym
        // slots, no villains): slot, class (RCLASS_*), female. For
        // leadertest.py's check of their names in the ROM.
        RogueGen_BuildRegion(&sRegion, strtoul(argv[2], NULL, 0));
        for (i = 0; i < ROGUE_SLOT_LEADER_0; i++)
        {
            u8 c;
            if (i >= sRegion.nTrainerSlots && i < ROGUE_SLOT_GYM_0)
                continue;
            c = RogueGen_TrainerClass(&sRegion, i);
            if (c < ROGUE_CLASS_COUNT)
                printf("trainer slot=%d class=%d female=%d\n", i, c, gRogueGenClasses[c].female);
        }
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "--roles") == 0)
    {
        // The item catalogue's roles, for item_tally.py.
        for (i = 1; i < 2048; i++)
            if (RogueGen_ItemRole(i) != RIR_COUNT)
                printf("role item=%d role=%d\n", i, RogueGen_ItemRole(i));
        return 0;
    }
    if (getenv("ROGUE_VISIT"))
        sHostVisit = atoi(getenv("ROGUE_VISIT"));
    RogueGen_BuildRegion(&sRegion, seed);
    snprintf(path, sizeof(path), "%s/region.txt", dir);
    f = fopen(path, "w");
    fprintf(f, "seed %u places=%d towns=%d areas=%d trainers=%d items=%d berries=%d start=%d summit=%d victory=%d\n",
            seed, sRegion.nPlaces, sRegion.nTowns, sRegion.nAreas, sRegion.nTrainerSlots, sRegion.nItems,
            sRegion.nBerries, sRegion.start, sRegion.summit, sRegion.victoryRoad);
    {
        char name[ROGUE_REGION_NAME_LEN + 1];
        RogueGen_RegionName(seed, name);
        fprintf(f, "region name=%s awkward=%d\n", name, RogueGen_IsAwkwardRegionName(seed));
    }
    {
        // What the professor says (enum RogueTrait) and what the map draws for it.
        struct RogueLand land;
        u8 first, second;
        s8 vx, vy;
        int x, y, lakes = 0;
        RogueGen_RegionTraits(&sRegion, &first, &second);
        s8 px, py;
        MapLand(&sRegion, &land, &vx, &vy);
        // Where the volcano was planned, and where the map draws it.
        px = vx;
        py = vy;
        if (vx >= 0)
            MapCraterByDoor(&sRegion, &land, &vx, &vy);
        for (y = 0; y < RGRID_H; y++)
            for (x = 0; x < RGRID_W; x++)
                lakes += land.cell[y][x] == CELL_LAKE;
        fprintf(f, "traits first=%d second=%d feature=%d volcano=%d,%d planned=%d,%d lakecells=%d\n", first, second, land.feature,
                vx, vy, px, py, lakes);
    }
    fprintf(f, "curve");
    for (i = 0; i < ROGUE_NUM_GYMS; i++)
        fprintf(f, " gym%d=%d", i, sRegion.aceLevel[i]);
    for (i = 0; i < ROGUE_NUM_E4; i++)
        fprintf(f, " e4%d=%d", i, sRegion.e4Level[i]);
    fprintf(f, " champion=%d", sRegion.championLevel);
    for (i = 0; i < 4; i++)
        fprintf(f, " rival%d=%d", i, sRegion.rivalLevel[i]);
    fprintf(f, "\n");
    for (i = 0; i < sRegion.nTowns; i++)
        fprintf(f, "town n=%d place=%d name=%s\n", i, sRegion.towns[i], sRegion.townNames[i]);
    for (i = 0; i < ROGUE_NUM_GYMS; i++)
        fprintf(f, "gym n=%d place=%d key=%d chapter=%d\n", i, sRegion.gymTown[i], sRegion.gymKey[i], sRegion.gymChapter[i]);
    for (i = 0; i < sRegion.nPlaces; i++)
    {
        const struct RoguePlace *p = &sRegion.places[i];
        struct RogueMap *m = &sMap;
        FILE *b;
        static const char *const kinds[] = { "town", "route", "cave" };

        gHostLandmark = 0;
        gHostFeatLot = 0;
        RogueGen_GenerateMap(&sRegion, i, m, sBlocks);
        fprintf(f, "map %d %s %d %d %d name=%s\n", i, kinds[p->kind], p->number, p->w, p->h,
                p->kind == RPLACE_TOWN ? (char *)sRegion.townNames[p->number] : "-");
        if (i == sRegion.start)
        {
            // After RUN 0's STOP: where the moving truck parks (and whether
            // its whole picture stands clear, or only its wheels and body).
            // (old: the spot v1.0's first build chose, its top unchecked.)
            s32 tx = -1, ty = -1, ox = -1, oy = -1;
            u8 found = RogueGen_TruckSpot(m, &tx, &ty), oldFound = Run0TruckRing(m, FALSE, &ox, &oy);
            fprintf(f, "truck place=%d x=%d y=%d found=%d clear=%d oldX=%d oldY=%d oldClear=%d\n", i, tx, ty, found,
                    found && Run0TruckFits(m, tx, ty, TRUE), ox, oy, oldFound && Run0TruckFits(m, ox, oy, TRUE));
        }
        fprintf(f, "  place type=%d theme=%d x=%d y=%d gym=%d tier=%d level=%d badges=%d area=%d trainerBase=%d nTrainers=%d itemBase=%d nItems=%d"
                   " berryBase=%d nBerries=%d rival=%d side=%d obstacle=%d guardDir=%d mouth=%d caveIn=%d caveOut=%d dark=%d interiors=%d layout=%d seaSide=%d branch=%d wildLevel=%d\n",
                p->type, p->theme, p->gridX, p->gridY, p->gym == 0xFF ? -1 : p->gym, p->tier, p->level, p->badges, p->area == 0xFF ? -1 : p->area,
                p->trainerBase, p->nTrainers, p->itemBase, p->nItems, p->berryBase, p->nBerries, p->rival, p->side,
                p->obstacle, p->guardDir == RDIR_NONE ? -1 : p->guardDir, p->mouth == 0xFF ? -1 : p->mouth,
                p->caveIn == 0xFF ? -1 : p->caveIn, p->caveOut == 0xFF ? -1 : p->caveOut, p->dark, p->interiors,
                p->layout, p->seaSide == RDIR_NONE ? -1 : p->seaSide, p->branch, RogueGen_WildLevel(p->level));
        if (p->kind == RPLACE_ROUTE)
            // v1.1: the route's zone (src/rogue_gen_zones.h) and weather.
            fprintf(f, "  zone zone=%d fit=%d weather=%d known=%d\n", gHostZone[i], p->type < RROUTE_COUNT ? sZoneKinds[gHostZone[i]][p->type] : 0,
                    RogueGen_RouteWeather(&sRegion, i), RogueGen_KnownWeather(&sRegion, i));
        if (p->kind == RPLACE_ROUTE && p->type == RROUTE_SEA)
            // v1.1 sea: the sea's kind (src/rogue_gen_sea.h).
            fprintf(f, "  seakind sea=%d\n", RogueGen_SeaKind(&sRegion, i));
        if (p->kind == RPLACE_TOWN)
        {
            // The town's signature (src/rogue_gen_signatures.h), its gift
            // Pokemon and whether GenTown built its landmark.
            char lm[40];
            u8 lv = 0;
            u16 gift = RogueGen_SignatureGift(&sRegion, i, &lv);
            RogueGen_SignatureLandmark(&sRegion, i, lm);
            fprintf(f, "  sig id=%d built=%d gift=%d giftLevel=%d landmark=%s\n", RogueGen_TownSignature(&sRegion, i),
                    gHostLandmark, gift, lv, lm);
            // A town of the wanderer's meetings: the townsperson they stand
            // in for and where they wait (by the POKeMON CENTER's door).
            {
                struct RogueCast cast;
                s32 standIn;
                u8 wx = 0, wy = 0;
                RogueGen_Cast(&sRegion, &cast);
                if (RogueGen_WandererAt(&cast, i) >= 0 && (standIn = RogueText_WandererObj(&sRegion, m)) >= 0)
                {
                    bool8 spot = RogueText_WandererSpot(m, standIn, &wx, &wy);
                    fprintf(f, "  wanderer meeting=%d obj=%d spot=%d x=%d y=%d pointer=%d\n", RogueGen_WandererAt(&cast, i), standIn,
                            spot, spot ? wx : m->objs[standIn].x, spot ? wy : m->objs[standIn].y,
                            RogueText_WandererPointerTurn(&sRegion, m));
                }
            }
        }
        if (p->kind == RPLACE_ROUTE)
        {
            // The route's feature (src/rogue_gen_features.h), whether its lot
            // was laid, its pair and the route's local name.
            char nick[32];
            s8 pair = RogueGen_RoutePair(&sRegion, i);
            if (!RogueGen_RouteNick(&sRegion, i, nick))
                strcpy(nick, "-");
            for (j = 0; nick[j]; j++)
                if (nick[j] == ' ')
                    nick[j] = '_';
            {
                u8 feat = RogueGen_RouteFeature(&sRegion, i), lv = 0, qty = 0;
                u16 gift = feat ? RogueGen_FeatureGift(&sRegion, feat, &lv) : 0;
                u16 item = feat ? RogueGen_FeatureItem(&sRegion, feat, &qty) : 0;
                // (lot=2: a berry farm's, v1.1, and crop0/crop1 its rows' berries.)
                fprintf(f, "  feat id=%d lot=%d pair=%d pairKind=%d gift=%d giftLevel=%d item=%d qty=%d crop0=%d crop1=%d nick=%s\n", feat,
                        gHostFeatLot, pair, pair >= 0 ? RogueGen_PairKind(&sRegion, pair) : -1, gift, lv, item, qty,
                        RogueGen_FarmRowBerry(sRegion.seed, 0), RogueGen_FarmRowBerry(sRegion.seed, 1), nick);
            }
        }
        for (j = 0; j < p->nLinks; j++)
            fprintf(f, "  link dir=%d target=%d pos=%d gate=%d offset=%d\n", p->links[j].dir, p->links[j].target,
                    p->links[j].pos, p->links[j].gate, p->links[j].offset);
        for (j = 0; j < m->nObjs; j++)
            // turn: -1 a fixed facing, 0-3 the second way they turn to, 4 all round.
            fprintf(f, "  obj kind=%d x=%d y=%d facing=%d slot=%d range=%d wet=%d turn=%d\n", m->objs[j].kind, m->objs[j].x, m->objs[j].y,
                    m->objs[j].facing & 3, m->objs[j].slot, m->objs[j].range,
                    m->objs[j].kind == ROBJ_TRAINER && RogueGen_TrainerByWater(&sRegion, i, m->objs[j].slot),
                    (m->objs[j].kind == ROBJ_TRAINER && (m->objs[j].facing & ROBJ_LOOKS)) ? 4
                    : (m->objs[j].kind == ROBJ_TRAINER && (m->objs[j].facing & ROBJ_TURNS)) ? (m->objs[j].facing >> 2) & 3 : -1);
        for (j = 0; j < m->nWarps; j++)
            fprintf(f, "  warp kind=%d x=%d y=%d\n", m->warps[j].kind, m->warps[j].x, m->warps[j].y);
        for (j = 0; j < m->nSigns; j++)
        {
            // Hidden items are kept with the signs (they are bg events too).
            if (m->signs[j].kind == RSIGN_HIDDEN_TUNNEL)
                fprintf(f, "  hidden k=-1 id=%d x=%d y=%d\n", RogueGen_TunnelHidden(&sRegion), m->signs[j].x, m->signs[j].y);
            else if (m->signs[j].kind >= RSIGN_HIDDEN)
                fprintf(f, "  hidden k=%d id=%d x=%d y=%d\n", m->signs[j].kind - RSIGN_HIDDEN,
                        RogueGen_HiddenBase(&sRegion, i) + m->signs[j].kind - RSIGN_HIDDEN, m->signs[j].x, m->signs[j].y);
            else
                fprintf(f, "  sign kind=%d x=%d y=%d\n", m->signs[j].kind, m->signs[j].x, m->signs[j].y);
        }
        if (RogueGen_HiddenCount(&sRegion, i))
            fprintf(f, "  hiddenplan base=%d count=%d\n", RogueGen_HiddenBase(&sRegion, i), RogueGen_HiddenCount(&sRegion, i));
        if (RogueGen_SpecialtyShop(&sRegion, i))
        {
            u16 stock[16];
            int b, n;
            for (b = 0; b <= 8; b++)
            {
                n = RogueGen_SpecialtyStock(&sRegion, i, b, stock, 16, 0);
                fprintf(f, "  shop kind=%d badges=%d items=", RogueGen_SpecialtyShop(&sRegion, i), b);
                for (j = 0; j < n; j++)
                    fprintf(f, "%s%d", j ? "," : "", stock[j]);
                fprintf(f, "\n");
            }
        }
        for (j = 0; j < m->nGates; j++)
            fprintf(f, "  gate x=%d y=%d dir=%d need=%d\n", m->gates[j].x, m->gates[j].y, m->gates[j].dir, m->gates[j].need);
        for (j = 0; j < m->nHouses; j++)
            fprintf(f, "  house n=%d interior=%d\n", j, m->interiors[j]);
        if (p->kind == RPLACE_TOWN)
        {
            // What the town's lore needs (loretest.py): townsfolk, a PC
            // (its bookshelf), an ordinary home of chat (the elder's), signs.
            u8 *t = sTownLore[i];
            t[0] = t[1] = t[3] = t[4] = 0;
            t[2] = 0xFF;
            for (j = 0; j < m->nObjs; j++)
                t[0] += m->objs[j].kind == ROBJ_VILLAGER;
            for (j = 0; j < m->nWarps; j++)
                t[1] |= m->warps[j].kind == RWARP_PC;
            for (j = 0; j < m->nHouses && t[2] == 0xFF; j++)
                if (m->interiors[j] == RINT_HOME
                 && RogueGen_HomeRole(&sRegion, i, m->interiors, m->nHouses, j) == RHOME_CHAT)
                    t[2] = j;
            for (j = 0; j < m->nSigns; j++)
            {
                t[3] |= m->signs[j].kind == RSIGN_TOWN;
                t[4] |= m->signs[j].kind == RSIGN_DUNGEON;
            }
        }
        if (p->kind == RPLACE_TOWN && p->type == RTOWN_START)
            fprintf(f, "  start x=%d y=%d\n", m->startX, m->startY);
        if (p->kind == RPLACE_TOWN)
        {
            u32 objs, said;
            RogueText_CountMuseum(&sRegion, i, &objs, &said);
            if (objs)
                fprintf(f, "  museum objects=%u said=%u\n", objs, said);
        }
        snprintf(path, sizeof(path), "%s/map%d.bin", dir, i);
        b = fopen(path, "wb");
        fwrite(m->blocks, 2, m->w * m->h, b);
        fclose(b);
    }
    // Dungeons and their floors: OUTDIR/floorD_F.bin.
    for (i = 0; i < sRegion.nDungeons; i++)
    {
        const struct RogueDungeon *d = &sRegion.dungeons[i];
        int fl;
        fprintf(f, "dungeon %d kind=%d host=%d exit=%d floors=%d boss=%d required=%d level=%d badges=%d area=%d"
                   " trainerBase=%d nTrainers=%d itemBase=%d nItems=%d legend=%d gate=%d gateSrc=%d gateBadges=%d sealAtLegend=%d leader=%d\n",
                i, d->kind, d->host, d->exit == 0xFF ? -1 : d->exit, d->nFloors, d->boss, d->required, d->level, d->badges,
                d->area == 0xFF ? -1 : d->area, d->trainerBase, d->nTrainers, d->itemBase, d->nItems, d->legend,
                ROGUE_GATE_KIND(d->gate), ROGUE_GATE_SRC(d->gate), d->boss == RBOSS_LEGEND ? RogueGen_LegendGateBadges(&sRegion, i) : 0,
                RogueGen_SealAtLegend(&sRegion, i) == 0xFF ? -1 : RogueGen_SealAtLegend(&sRegion, i),
                ROGUE_GATE_KIND(d->gate) == RGATE_VILLAIN && d->boss == RBOSS_LEGEND
                && RogueGen_TeamLeaderDungeon(&sRegion, ROGUE_GATE_SRC(d->gate)) == i ? ROGUE_GATE_SRC(d->gate) + 1 : 0);
        {
            struct RogueDungeonExtras e;
            RogueGen_DungeonExtras(&sRegion, i, &e);
            fprintf(f, "  extras prize=%d choice=%d nChoice=%d choiceFloor=%d camp=%d campFloor=%d explorer=%d explorerFloor=%d total=%d"
                       " keys=%d nKeys=%d decoys=%d nDecoys=%d\n",
                    e.prize == 0xFF ? -1 : e.prize, e.choice == 0xFF ? -1 : e.choice, e.nChoice, e.choiceFloor,
                    e.camp == 0xFF ? -1 : e.camp, e.campFloor, e.explorer == 0xFF ? -1 : e.explorer, e.explorerFloor, e.total,
                    e.keys == 0xFF ? -1 : e.keys, e.nKeys, e.decoys == 0xFF ? -1 : e.decoys, e.nDecoys);
            for (fl = 0; fl < e.nKeys; fl++)
                fprintf(f, "  key slot=%d item=%d\n", e.keys + fl, RogueGen_KeyItem(&sRegion, i, e.keys + fl));
        }
        if (d->kind == RDUN_RUINS)
        {
            // The vault's riddle: each tablet's word (left to right) and the
            // order they are read in.
            u8 words[3], order[3];
            RogueGen_RuinsRiddle(&sRegion, i, words, order);
            fprintf(f, "  riddle visit=%d w0=%d w1=%d w2=%d o0=%d o1=%d o2=%d\n", sHostVisit, words[0], words[1], words[2],
                    order[0], order[1], order[2]);
        }
        for (fl = 1; fl <= d->nFloors; fl++)
        {
            struct RogueMap *m = &sMap;
            struct RoguePlace fp;
            FILE *b;
            RogueGen_FloorPlace(&sRegion, i, fl, &fp);
            RogueGen_GenerateFloor(&sRegion, i, fl, m, sBlocks);
            fprintf(f, "floor %d %d %d %d level=%d trainerBase=%d nTrainers=%d itemBase=%d nItems=%d dark=%d room=%d wild=%d wildLevel=%d land=%d\n", i, fl, m->w, m->h,
                    fp.level, fp.trainerBase, fp.nTrainers, fp.itemBase, fp.nItems, fp.dark, RogueGen_FloorRoom(&sRegion, i, fl), RogueGen_FloorWildRate(&sRegion, i, fl),
                    RogueGen_WildLevel(fp.level), RogueGen_RuinsLandWarp(&sRegion, i, fl) == 0xFF ? -1 : RogueGen_RuinsLandWarp(&sRegion, i, fl));
            {
                // What its popup says (RogueGen_FloorPopup).
                char popup[ROGUE_NAME_LEN + 16];
                int num = RogueGen_FloorPopup(&sRegion, i, fl, popup);
                u32 graves, read;
                RogueText_CountGraves(&sRegion, i, fl, &graves, &read);
                if (graves)
                    fprintf(f, "  graves %u read=%u\n", graves, read);
                if (num == 0)
                    fprintf(f, "  popup %s\n", popup);
                else
                    fprintf(f, "  popup %s %s%dF\n", popup, num < 0 ? "B" : "", num < 0 ? -num : num);
            }
            // The floor's level in the game with each count of badges held
            // (RogueGen_FloorLevel: optional dungeons follow the badges).
            fprintf(f, "  levels");
            for (j = 0; j <= ROGUE_NUM_GYMS; j++)
            {
                sHostBadges = j;
                fprintf(f, " b%d=%d", j, RogueGen_FloorLevel(&sRegion, i, fl));
            }
            sHostBadges = 0;
            fprintf(f, "\n");
            for (j = 0; j < m->nObjs; j++)
                fprintf(f, "  obj kind=%d x=%d y=%d facing=%d slot=%d range=%d wet=0 turn=%d\n", m->objs[j].kind, m->objs[j].x, m->objs[j].y,
                        m->objs[j].facing & 3, m->objs[j].slot, m->objs[j].range,
                        (m->objs[j].kind == ROBJ_TRAINER && (m->objs[j].facing & ROBJ_LOOKS)) ? 4
                        : (m->objs[j].kind == ROBJ_TRAINER && (m->objs[j].facing & ROBJ_TURNS)) ? (m->objs[j].facing >> 2) & 3 : -1);
            for (j = 0; j < m->nWarps; j++)
            {
                // A room floor's warps: where each leads (floor 0: out).
                u8 toFloor = 0xFF, toWarp = 0;
                if (RogueGen_FloorRoom(&sRegion, i, fl) != 0xFF)
                    RogueGen_RoomWarpDest(&sRegion, i, fl, m->warps[j].kind, &toFloor, &toWarp);
                fprintf(f, "  warp kind=%d x=%d y=%d toFloor=%d toWarp=%d\n", m->warps[j].kind, m->warps[j].x, m->warps[j].y,
                        toFloor == 0xFF ? -1 : toFloor, toWarp);
            }
            for (j = 0; j < m->nSigns; j++)
                fprintf(f, "  sign kind=%d x=%d y=%d\n", m->signs[j].kind, m->signs[j].x, m->signs[j].y);
            snprintf(path, sizeof(path), "%s/floor%d_%d.bin", dir, i, fl);
            b = fopen(path, "wb");
            fwrite(m->blocks, 2, m->w * m->h, b);
            fclose(b);
        }
    }
    {
        // The villain team: its hideout dungeon, and the route where its
        // grunts first show up, with a bit for each grunt trainer there.
        u8 mask;
        u8 hint = RogueGen_VillainHint(&sRegion, &mask);
        struct RogueTeams tm;
        u8 t;
        fprintf(f, "villain team=%d hideout=%d hint=%d mask=%d\n", RogueGen_VillainTeam(&sRegion), RogueGen_Hideout(&sRegion),
                hint == 0xFF ? -1 : hint, mask);
        // Every team (src/rogue_gen_hideout.h): its identity (0 Aqua, 1
        // Magma, 2 Rocket), hideout, the route of its first grunts with
        // their trainer bits, the bit of the team before's grunt there
        // (guest), its leader's legend dungeon and its admin.
        RogueGen_Teams(&sRegion, &tm);
        fprintf(f, "teams n=%d wanted=%d\n", tm.n, RogueGen_TeamsWanted(sRegion.seed, NULL));
        {
            // The region's gym leaders and Elite Four (gRogueGenLeaders).
            u8 gyms[ROGUE_NUM_GYMS], e4[ROGUE_NUM_E4], g;
            RogueGen_RegionLeaders(&sRegion, gyms, e4);
            fprintf(f, "leaders");
            for (g = 0; g < ROGUE_NUM_GYMS + ROGUE_NUM_E4; g++)
                fprintf(f, " %s", gRogueGenLeaders[g < ROGUE_NUM_GYMS ? gyms[g] : e4[g - ROGUE_NUM_GYMS]].name);
            fprintf(f, "\n");
        }
        for (t = 0; t < tm.n; t++)
            fprintf(f, "team t=%d id=%d hideout=%d hint=%d mask=%d guest=%d leader=%d bossInHideout=%d admin=%s\n", t, tm.id[t], tm.hideout[t],
                    tm.hint[t] == 0xFF ? -1 : tm.hint[t], tm.mask[t], tm.guest[t], RogueGen_TeamLeaderDungeon(&sRegion, t),
                    RogueGen_TeamBossInHideout(&sRegion, t), RogueGen_TeamAdminName(&sRegion, t));
    }
    if (sRegion.tunnel < sRegion.nDungeons)
    {
        // The tunnel (src/rogue_gen_tunnel.h): its dungeon, far end, barrier
        // and the badges it opens with, winning the gyms in order.
        const struct RogueDungeon *d = &sRegion.dungeons[sRegion.tunnel];
        fprintf(f, "tunnel dungeon=%d host=%d exit=%d gate=%d src=%d opens=%d floor=%d\n", sRegion.tunnel, d->host, d->exit,
                ROGUE_GATE_KIND(sRegion.tunnelGate), ROGUE_GATE_SRC(sRegion.tunnelGate), RogueGen_TunnelOpensAt(&sRegion),
                RogueGen_ExitFloor(&sRegion, sRegion.tunnel));
        // The main road's level where each stretch begins, which the
        // tunnel's floors follow in the game (the badges held).
        fprintf(f, "  stretch");
        for (i = 0; i <= ROGUE_NUM_GYMS; i++)
            fprintf(f, " b%d=%d", i, RogueGen_StretchLevel(&sRegion, i));
        fprintf(f, "\n");
    }
    {
        // Items: every draw of the ledger, folk gift towns and gym TMs by type.
        int t;
        sItemOut = f;
        RogueGen_TallyItems(&sRegion, 0, ItemSink);
        for (i = 0; i < sRegion.nTowns; i++)
            if (RogueGen_HasFolkGift(&sRegion, i))
                fprintf(f, "folk town=%d\n", i);
        for (i = 0; i < ROGUE_NUM_GYMS; i++)
            for (t = 1; t < 20; t++)
                if (t != 10)
                    fprintf(f, "gymtm gym=%d type=%d item=%d\n", i, t, RogueGen_GymTM(&sRegion, i, t));
    }
    fclose(f);
    {
        // Legend lore and dungeon names (src/rogue_gen_lore.h), for
        // loretest.py: each piece as the game shows it, '\n' as '/', a
        // scroll as '\\' and a new box as '|'.
        static const char *const ways[LORE_WAYS] = { "carving", "book", "elder", "lab", "leader", "villain", "folk", "sign", "tv", "shrine",
                                                     "diary", "grave", "museum", "rhyme", "painting" };
        char text[1000], *c;
        snprintf(path, sizeof(path), "%s/lore.txt", dir);
        f = fopen(path, "w");
        RogueGen_RegionName(seed, text);
        fprintf(f, "region name=%s start=%d\n", text, sRegion.start);
        RogueGen_VolcanoName(&sRegion, text);
        fprintf(f, "volcano name=%s\n", text);
        {
            // The region's founding tale and the legend it is about.
            struct RogueRegionFacts rf;
            struct RogueTextCtx tc;
            RogueGen_RegionFacts(&sRegion, &rf);
            RogueText_Init(&tc, &sRegion, rf.oldTown, RSPK_NONE);
            RogueText_Fill(text, sizeof(text), "Long ago, {FOUNDING}.", &tc);
            fprintf(f, "founding d=%d text=%s\n", rf.legend == 0xFF ? -1 : rf.legend, text);
        }
        for (i = 0; i < sRegion.nPlaces; i++)
            if (sRegion.places[i].kind == RPLACE_TOWN)
                fprintf(f, "town place=%d type=%d theme=%d folk=%d pc=%d chathome=%d townsign=%d dungeonsign=%d gift=%d museum=%d riddle=%d\n", i,
                        sRegion.places[i].type, sRegion.places[i].theme, sTownLore[i][0], sTownLore[i][1],
                        sTownLore[i][2] == 0xFF ? -1 : sTownLore[i][2], sTownLore[i][3], sTownLore[i][4],
                        RogueGen_HasFolkGift(&sRegion, sRegion.places[i].number),
                        RogueGen_TownHasMuseum(&sRegion, i),
                        RogueGen_RuinsDungeon(&sRegion) >= 0
                        && RogueGen_TownNear(&sRegion, sRegion.dungeons[RogueGen_RuinsDungeon(&sRegion)].host) == i);
        for (i = 0; i < sRegion.nDungeons; i++)
        {
            const struct RogueDungeon *d = &sRegion.dungeons[i];
            struct RogueLore lore;
            static const u8 pieces[] = { LORE_STORY, LORE_HINT, LORE_MOTTO, LORE_LATER, LORE_VILLAIN_WHY };
            static const char *const pieceNames[] = { "story", "hint", "motto", "later", "why" };
            int k;
            {
                struct RogueDungeonExtras e;
                RogueGen_DungeonExtras(&sRegion, i, &e);
                fprintf(f, "dun d=%d kind=%d prize=%d graveFloor=%d\n", i, d->kind, e.prize == 0xFF ? -1 : e.prize,
                        RogueGen_GraveFloor(&sRegion, i));
            }
            if (d->kind == RDUN_ICE)
            {
                RogueGen_IceCaveName(&sRegion, i, text);
                fprintf(f, "ice d=%d host=%d name=%s\n", i, d->host, text);
            }
            if (d->boss != RBOSS_LEGEND)
                continue;
            RogueGen_LegendLore(&sRegion, i, &lore);
            fprintf(f, "lore d=%d legend=%d name=%s kind=%d host=%d gate=%d gateSrc=%d home=%d homeElem=%d elem=%d fits=%d town=%d"
                       " holder=%d graveFloor=%d townName=%s story=%s hint=%s\n", i, d->legend, sLoreNames[d->legend], d->kind, d->host,
                    ROGUE_GATE_KIND(d->gate), ROGUE_GATE_SRC(d->gate), lore.home, lore.homeElem, lore.elem, lore.fits, lore.town,
                    RogueGen_LoreHolder(&sRegion, i) == 0xFF ? -1 : RogueGen_LoreHolder(&sRegion, i),
                    RogueGen_LoreHolder(&sRegion, i) == 0xFF ? 0 : RogueGen_GraveFloor(&sRegion, RogueGen_LoreHolder(&sRegion, i)),
                    sRegion.townNames[sRegion.places[lore.town].number], ways[lore.story], ways[lore.hint]);
            for (k = 0; k < 5; k++)
            {
                u8 way = pieces[k] == LORE_STORY ? lore.story : pieces[k] == LORE_HINT ? lore.hint
                       : pieces[k] == LORE_LATER ? LORE_WAYS : pieces[k] == LORE_VILLAIN_WHY ? LORE_WAYS : LORE_WAYS;
                RogueGen_LoreText(&sRegion, i, pieces[k], way, text, sizeof(text));
                for (c = text; *c; c++)
                    if (*c == '\n')
                        *c = '/';
                    else if (*c == '\f')
                        *c = '|';
                    else if (*c == '\v')
                        *c = '\\';
                fprintf(f, "  %s %s\n", pieceNames[k], text);
            }
        }
        fclose(f);
    }
    if (RogueGen_IsRun0(&sRegion))
    {
        // RUN 0: where the story's hooks, the islands' doors and the
        // changing maps are (src/rogue_gen_run0.h).
        static const char *const hookNames[RUN0_HOOK_COUNT] =
        {
            "LIBRARY", "TERRY_HOUSE", "BELIEVER_0", "BELIEVER_1", "BELIEVER_2", "GIFT_BOX", "CHAMPION_TERRY",
            "PROMPT_0", "PROMPT_1", "PROMPT_2", "PROMPT_3", "PROMPT_4", "PROMPT_5", "PROMPT_6", "PROMPT_7", "GRAVEYARD",
        };
        snprintf(path, sizeof(path), "%s/run0.txt", dir);
        f = fopen(path, "w");
        fprintf(f, "run0 act=%d relay=%d copy=%d loop=%d graveyard=%d victory=%d shortcut=%d\n", RogueGen_Run0Act(),
                RogueGen_Run0RelayTown(&sRegion), RogueGen_Run0CopyRoute(&sRegion), RogueGen_Run0LoopRoute(&sRegion),
                RogueGen_Run0Graveyard(&sRegion), sRegion.victoryRoad, RogueGen_Run0ShortcutPlace(&sRegion));
        {
            s32 gym, houses;
            RogueGen_Run0CutPlaces(&sRegion, &gym, &houses);
            fprintf(f, "cuts gym=%d houses=%d\n", gym, houses);
        }
        for (i = 0; i < RUN0_ISLAND_COUNT; i++)
            fprintf(f, "island n=%d town=%d\n", i, RogueGen_Run0IslandTown(&sRegion, i));
        for (i = 0; i < RUN0_HOOK_COUNT; i++)
            fprintf(f, "hook n=%d name=%s place=%d\n", i, hookNames[i], RogueGen_Run0HookPlace(&sRegion, i));
        for (i = 0; i < sRegion.nPlaces; i++)
        {
            RogueGen_GenerateMap(&sRegion, i, &sMap, sBlocks);
            for (j = 0; j < sMap.nHouses; j++)
                if (RogueGen_Run0HouseHook(&sRegion, &sMap, j) >= 0)
                    fprintf(f, "house place=%d house=%d what=%d\n", i, j, RogueGen_Run0HouseHook(&sRegion, &sMap, j));
            for (j = 0; j < sMap.nObjs; j++)
                if (sMap.objs[j].kind == ROBJ_RUN0)
                    fprintf(f, "thing place=%d hook=%d x=%d y=%d\n", i, sMap.objs[j].slot, sMap.objs[j].x, sMap.objs[j].y);
            for (j = 0; j < sMap.nSigns; j++)
                if (sMap.signs[j].kind >= RSIGN_RUN0 && sMap.signs[j].kind < RSIGN_LOCK)
                    fprintf(f, "sign place=%d kind=%d x=%d y=%d\n", i, sMap.signs[j].kind, sMap.signs[j].x, sMap.signs[j].y);
        }
        fclose(f);
    }
    {
        // The region map: its tilemap, and a line per place with its tile.
        static u16 tiles[ROGUE_RMAP_W * ROGUE_RMAP_H];
        static u8 work[ROGUE_RMAP_WORK];
        s8 px[ROGUE_MAX_PLACES], py[ROGUE_MAX_PLACES];
        u32 h = RogueGen_DrawRegionMap(&sRegion, tiles, px, py, work, argc > 3 ? atoi(argv[3]) : 2);
        snprintf(path, sizeof(path), "%s/region_map.bin", dir);
        f = fopen(path, "wb");
        fwrite(tiles, 2, ROGUE_RMAP_W * ROGUE_RMAP_H, f);
        fclose(f);
        snprintf(path, sizeof(path), "%s/region_map.txt", dir);
        f = fopen(path, "w");
        fprintf(f, "height %u\n", h);
        // Columns and rows holding anything but open sea.
        {
            int x, y, x0 = 99, x1 = -1, y0 = 99, y1 = -1;
            for (y = 0; y < ROGUE_RMAP_H; y++)
                for (x = 0; x < ROGUE_RMAP_W; x++)
                    if (tiles[y * ROGUE_RMAP_W + x] != 0)
                    {
                        if (x < x0) x0 = x;
                        if (x > x1) x1 = x;
                        if (y < y0) y0 = y;
                        if (y > y1) y1 = y;
                    }
            fprintf(f, "extent x0=%d x1=%d y0=%d y1=%d\n", x0, x1, y0, y1);
        }
        for (i = 0; i < sRegion.nPlaces; i++)
            fprintf(f, "place %d x=%d y=%d\n", i, px[i], py[i]);
        fclose(f);
    }
    return 0;
}
