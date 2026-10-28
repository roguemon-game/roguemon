// Zones: where a route lies decides what kind of route it is (v1.1).
//
// Part of rogue_gen.c (included after AssignRouteTypes, whose helpers it
// uses). The region is split into areas on its grid: the coast along the
// open sea, the mountain or the volcano and the land round it, lake country
// round the lake, and inland forest, plains and now and then a dry side. A
// route takes a kind its area suits, so the road runs through areas (a
// stretch of beaches and ponds, then woods, then hills toward the volcano)
// instead of a shuffled bag, with a surprise now and then.
//
// The first three routes are gentle kinds of every sort (a beach, a lake, a
// river with a bridge, flowers, low hills, a field, a forest), and no three
// routes in a row are field, forest or deep woods, which look alike.
//
// Nothing here is kept: the zones are worked out while the region is
// planned (sLand is the planner's work buffer) and only the kinds stay.
// The plan has its own random stream, so the rest of the region (town
// names, trainers) draws as before.

enum { RZONE_NONE, RZONE_COAST, RZONE_MOUNTAIN, RZONE_FOREST, RZONE_DRY, RZONE_LAKE, RZONE_PLAIN, RZONE_VOLCANO, RZONE_COUNT };

#ifdef ROGUE_HOST
// The zone of each place as planned, for gen_host (variety.py).
static u8 gHostZone[ROGUE_MAX_PLACES];
#endif

// How much each zone wants each kind (out of about 100). Kinds left at 0
// still turn up now and then as a surprise (ZONE_SURPRISE).
static const u8 sZoneKinds[RZONE_COUNT][RROUTE_COUNT] =
{
    [RZONE_NONE]     = { [RROUTE_FIELD] = 30, [RROUTE_FOREST] = 25, [RROUTE_RIVER] = 15, [RROUTE_HILLS] = 15, [RROUTE_LAKE] = 15 },
    [RZONE_COAST]    = { [RROUTE_COAST] = 50, [RROUTE_RIVER] = 15, [RROUTE_LAKE] = 12, [RROUTE_FLOWERS] = 10, [RROUTE_FIELD] = 8,
                         [RROUTE_HILLS] = 5 },
    [RZONE_MOUNTAIN] = { [RROUTE_HILLS] = 50, [RROUTE_DESERT] = 12, [RROUTE_FOREST] = 10, [RROUTE_RIVER] = 10, [RROUTE_WOODS] = 8,
                         [RROUTE_LONGGRASS] = 5, [RROUTE_FIELD] = 5 },
    [RZONE_FOREST]   = { [RROUTE_FOREST] = 30, [RROUTE_WOODS] = 25, [RROUTE_LONGGRASS] = 15, [RROUTE_LAKE] = 12, [RROUTE_RIVER] = 10,
                         [RROUTE_FIELD] = 8 },
    [RZONE_DRY]      = { [RROUTE_DESERT] = 45, [RROUTE_HILLS] = 30, [RROUTE_FIELD] = 10, [RROUTE_RIVER] = 10, [RROUTE_FLOWERS] = 5 },
    [RZONE_LAKE]     = { [RROUTE_LAKE] = 35, [RROUTE_RIVER] = 30, [RROUTE_LONGGRASS] = 10, [RROUTE_FOREST] = 10, [RROUTE_FLOWERS] = 8,
                         [RROUTE_FIELD] = 7 },
    [RZONE_PLAIN]    = { [RROUTE_FIELD] = 30, [RROUTE_FLOWERS] = 25, [RROUTE_RIVER] = 18, [RROUTE_LAKE] = 10, [RROUTE_HILLS] = 10,
                         [RROUTE_FOREST] = 7 },
    [RZONE_VOLCANO]  = { [RROUTE_DESERT] = 45, [RROUTE_HILLS] = 40, [RROUTE_FIELD] = 8, [RROUTE_FOREST] = 7 },
};
#define ZONE_SURPRISE 3

// The kinds a route may be at all: not the sea, Victory Road or the bridge
// (placed on its own), and for the first routes only gentle ones.
static bool8 ZoneKindAllowed(u8 kind, u8 tier, bool8 opening)
{
    switch (kind)
    {
    case RROUTE_FIELD:
    case RROUTE_FOREST:
    case RROUTE_LAKE:
    case RROUTE_RIVER:
    case RROUTE_COAST:
    case RROUTE_FLOWERS:
    case RROUTE_HILLS:
        return TRUE;
    case RROUTE_WOODS:
        return !opening;
    case RROUTE_DESERT:
        return !opening && tier >= 1;
    case RROUTE_LONGGRASS:
        // Long grass hides the player and the trainers: mid game on.
        return !opening && tier >= 2;
    default:
        return FALSE;
    }
}

static bool8 LooksAlike(u8 kind)
{
    return kind == RROUTE_FIELD || kind == RROUTE_FOREST || kind == RROUTE_WOODS;
}

// Manhattan distance from a cell to the land feature of the given kind, or
// 99 if the land has none.
static int ZoneFeatureDist(int x, int y, u8 feature)
{
    const struct RogueLand *L = &sLand;
    int dx, dy;
    if (L->feature != feature)
        return 99;
    dx = x < L->fx ? L->fx - x : x >= L->fx + L->fw ? x - (L->fx + L->fw - 1) : 0;
    dy = y < L->fy ? L->fy - y : y >= L->fy + L->fh ? y - (L->fy + L->fh - 1) : 0;
    return dx + dy;
}

// Distance from a cell to the nearest town of a theme, or 99.
static int ZoneThemeDist(const struct RogueRegion *r, int x, int y, u8 theme)
{
    int i, best = 99;
    for (i = 0; i < r->nTowns; i++)
    {
        const struct RoguePlace *t = &r->places[r->towns[i]];
        int d = (t->gridX > x ? t->gridX - x : x - t->gridX) + (t->gridY > y ? t->gridY - y : y - t->gridY);
        if (t->theme == theme && t->type != RTOWN_SUMMIT && d < best)
            best = d;
    }
    return best;
}

// Distance from a cell to the open sea: the sea cells of the land plan and
// the edges the deep sea lies off.
static int ZoneSeaDist(int x, int y)
{
    int d, k;
    for (d = 1; d <= 3; d++)
    {
        for (k = 0; k < 4 * d; k++)
        {
            // The ring of cells d steps away.
            int s = k % d, q = k / d;
            int cx = x + (q == 0 ? s : q == 1 ? d - s : q == 2 ? -s : -(d - s));
            int cy = y + (q == 0 ? -(d - s) : q == 1 ? s : q == 2 ? d - s : -s);
            if (cx < 0 ? (sLand.seaSides & (1 << RDIR_WEST)) : cx >= RGRID_W ? (sLand.seaSides & (1 << RDIR_EAST))
              : cy < 0 ? (sLand.seaSides & (1 << RDIR_NORTH)) : cy >= RGRID_H ? (sLand.seaSides & (1 << RDIR_SOUTH))
              : sLand.cell[cy][cx] == CELL_SEA)
                return d;
        }
    }
    return 99;
}

// The zone of a cell. The land's own feature and the sea come first; the
// rest of the inland is split between two or three areas round seed cells
// (forest, plains, a dry side), each cell going to the nearest.
struct ZonePlan
{
    s8 x[3], y[3];
    u8 kind[3];
    u8 n;
};

static void PlanZones(const struct RogueRegion *r, u32 *rng, struct ZonePlan *z)
{
    static const u8 sInland[][3] =
    {
        { RZONE_FOREST, RZONE_PLAIN, RZONE_DRY },
        { RZONE_FOREST, RZONE_PLAIN, RZONE_FOREST },
        { RZONE_PLAIN, RZONE_FOREST, RZONE_DRY },
        { RZONE_FOREST, RZONE_PLAIN, RZONE_PLAIN },
    };
    int i, k, tries;
    const u8 *pick = sInland[RogueRng_Range(rng, ARRAY_COUNT_GEN(sInland))];
    z->n = 2 + RogueRng_Range(rng, 2);
    for (i = 0; i < z->n; i++)
    {
        int bx = 0, by = 0, bestD = -1;
        // Seed cells far apart: the best of a few random land cells.
        for (tries = 0; tries < 8; tries++)
        {
            int x = sLand.x0 + RogueRng_Range(rng, sLand.x1 - sLand.x0 + 1), y = sLand.y0 + RogueRng_Range(rng, sLand.y1 - sLand.y0 + 1);
            int d = 99;
            if (sLand.cell[y][x] == CELL_SEA)
                continue;
            for (k = 0; k < i; k++)
            {
                int dk = (x > z->x[k] ? x - z->x[k] : z->x[k] - x) + (y > z->y[k] ? y - z->y[k] : z->y[k] - y);
                if (dk < d)
                    d = dk;
            }
            // A dry side lies away from the sea.
            if (pick[i] == RZONE_DRY)
                d += ZoneSeaDist(x, y) >= 3 ? 3 : 0;
            if (d > bestD)
                bestD = d, bx = x, by = y;
        }
        z->x[i] = bx;
        z->y[i] = by;
        z->kind[i] = pick[i];
    }
}

static u8 ZoneAt(const struct RogueRegion *r, const struct ZonePlan *z, int x, int y)
{
    int i, best = 99;
    u8 zone = RZONE_PLAIN;
    // The volcano: round the land's own, else round the ash towns, beside
    // one of which the map draws it.
    if (ZoneFeatureDist(x, y, FEATURE_VOLCANO) <= 2 || ZoneThemeDist(r, x, y, RTHEME_ASH) <= (sLand.feature == FEATURE_VOLCANO ? 1 : 2))
        return RZONE_VOLCANO;
    if (ZoneFeatureDist(x, y, FEATURE_MOUNTAIN) <= 2)
        return RZONE_MOUNTAIN;
    if (ZoneFeatureDist(x, y, FEATURE_LAKE) <= 1)
        return RZONE_LAKE;
    if (ZoneSeaDist(x, y) <= 1)
        return RZONE_COAST;
    for (i = 0; i < z->n; i++)
    {
        // A little unevenness, so the areas are not cut on straight lines.
        int d = 2 * ((x > z->x[i] ? x - z->x[i] : z->x[i] - x) + (y > z->y[i] ? y - z->y[i] : z->y[i] - y))
              + (RogueRng_Hash(r->seed, 52100 + y * RGRID_W + x + i * 128) & 1);
        if (d < best)
            best = d, zone = z->kind[i];
    }
    return zone;
}

// The weight of a kind for route p in its zone, given the routes before it
// on the road (last, last2; 0xFF for none), the next route's kind if it is
// fixed already, how many look-alike routes touch it in a row on either
// side (alike), and what the road has had so far.
static u32 ZoneWeight(const struct RoguePlace *p, u8 zone, u8 kind, u8 sea, bool8 opening,
                      u8 last, u8 last2, u8 next, u8 alike, u16 seen, u16 recent)
{
    u32 w;
    if (!ZoneKindAllowed(kind, p->tier, opening))
        return 0;
    if (kind == RROUTE_COAST && !sea)
        return 0;
    // Never three of a kind, nor three look-alikes, in a row.
    if (kind == last && (kind == last2 || kind == next))
        return 0;
    if (LooksAlike(kind) && alike >= 2)
        return 0;
    w = sZoneKinds[zone][kind] + ZONE_SURPRISE;
    // A beach wherever the sea is, in any zone, though most by the coast.
    if (kind == RROUTE_COAST)
        w += 25;
    if (opening)
    {
        // The first routes: every gentle kind about as likely, the zone's
        // own a little more, a beach whenever the sea is there.
        w = 10 + sZoneKinds[zone][kind] / 3 + (kind == RROUTE_COAST ? 10 : 0);
        // Hills after the first route.
        if (kind == RROUTE_HILLS && p->number <= 1)
            return 0;
    }
    if (kind == last)
        w = w / 2;
    if (LooksAlike(kind) && last != 0xFF && LooksAlike(last))
        w = w / 3;
    if (recent & (1 << kind))
        w = w / 2;
    // The first stretch shows as many kinds as it can.
    if (p->number <= 6 && (seen & (1 << kind)))
        w = w / 3;
    return w;
}

// Route kinds by zone. Replaces AssignRouteTypes (kept for RUN 0's region).
static void AssignZoneRouteTypes(struct RogueRegion *r, const u8 *spinePlace)
{
    u32 rng = RogueRng_Hash(r->seed, 52000);
    struct ZonePlan zp;
    u8 road[SPINE_MAX], fixed[SPINE_MAX];
    int i, nRoad = 0;
    u16 seen = 0, recent[4] = { 0 };
    u8 last = 0xFF, last2 = 0xFF;

    PlanZones(r, &rng, &zp);
    for (i = 0; i < r->nPlaces; i++)
    {
        struct RoguePlace *p = &r->places[i];
#ifdef ROGUE_HOST
        gHostZone[i] = p->kind == RPLACE_ROUTE ? ZoneAt(r, &zp, p->gridX, p->gridY) : RZONE_NONE;
#endif
    }
    for (i = 0; i < (int)SPINE_LEN; i++)
    {
        const struct RoguePlace *p = &r->places[spinePlace[i]];
        if (p->kind == RPLACE_ROUTE && p->type != RROUTE_MOUNTAIN && p->type != RROUTE_SEA)
            road[nRoad++] = spinePlace[i];
    }

    // First the routes whose kind is fixed by what stands on them: the route
    // up to a cave climbs hills, water to cross is a lake, and a block on
    // the route itself stands at the end of a walled way in through green
    // country (GuardType).
    for (i = 0; i < nRoad; i++)
    {
        struct RoguePlace *p = &r->places[road[i]];
        fixed[i] = 0xFF;
        if (p->mouth != 0xFF)
            fixed[i] = RROUTE_HILLS;
        else if (p->obstacle == ROBST_SURF)
            fixed[i] = RROUTE_LAKE;
        else if (p->obstacle && !ObstacleOutside(r, p))
        {
            u8 zone = ZoneAt(r, &zp, p->gridX, p->gridY);
            u32 f = sZoneKinds[zone][RROUTE_FOREST] + 10, w = (p->tier ? sZoneKinds[zone][RROUTE_WOODS] + 5 : 0),
                g = sZoneKinds[zone][RROUTE_FIELD] + 10, k = RogueRng_Range(&rng, f + w + g);
            fixed[i] = k < f ? RROUTE_FOREST : k < f + w ? RROUTE_WOODS : RROUTE_FIELD;
            // Not the same as a fixed route just before.
            if (i > 0 && fixed[i - 1] == fixed[i])
                fixed[i] = fixed[i] == RROUTE_FIELD ? RROUTE_FOREST : RROUTE_FIELD;
        }
    }

    for (i = 0; i < nRoad; i++)
    {
        struct RoguePlace *p = &r->places[road[i]];
        u8 sea = FreeSidesFacing(p, CELL_SEA), zone = ZoneAt(r, &zp, p->gridX, p->gridY), next, alike;
        bool8 opening = p->number <= 3 || p->tier == 0;
        u16 rec = recent[0] | recent[1] | recent[2] | recent[3];
        u32 w[RROUTE_COUNT], total = 0, k;
        u8 t;
        // No coast where the main road turns a corner and a side path takes
        // the way on, which leaves the sea far from the walk.
        if (p->nLinks >= 3 && MainTurns(r, p))
            sea = 0;
        next = i + 1 < nRoad ? fixed[i + 1] : 0xFF;
        alike = 0;
        if (last != 0xFF && LooksAlike(last))
            alike += (last2 != 0xFF && LooksAlike(last2)) ? 2 : 1;
        if (next != 0xFF && LooksAlike(next))
            alike += (i + 2 < nRoad && fixed[i + 2] != 0xFF && LooksAlike(fixed[i + 2])) ? 2 : 1;
        if (fixed[i] != 0xFF)
        {
            p->type = fixed[i];
        }
        else
        {
            for (t = 0; t < RROUTE_COUNT; t++)
            {
                w[t] = ZoneWeight(p, zone, t, sea, opening, last, last2, next, alike, seen, rec);
                total += w[t];
            }
            if (total == 0)
            {
                // Nothing fits the rules: any kind allowed here but the last.
                for (t = 0; t < RROUTE_COUNT; t++)
                {
                    w[t] = (ZoneKindAllowed(t, p->tier, opening) && t != last && !(t == RROUTE_COAST && !sea)
                         && !(LooksAlike(t) && alike)) ? 1 : 0;
                    total += w[t];
                }
            }
            k = total ? RogueRng_Range(&rng, total) : 0;
            for (t = 0; t + 1 < RROUTE_COUNT && k >= w[t]; t++)
                k -= w[t];
            p->type = total ? t : RROUTE_RIVER;
        }
        // Beside an ash town, ash country runs on along the route, but never
        // on the first ones.
        if (p->theme == RTHEME_ASH && p->type == RROUTE_HILLS && fixed[i] == 0xFF && !opening && last != RROUTE_DESERT
         && RogueRng_Range(&rng, 100) < 40)
            p->type = RROUTE_DESERT;
        seen |= 1 << p->type;
        recent[i & 3] = 1 << p->type;
        last2 = last;
        last = p->type;
        // A rest house on some of the longer routes from the third on, not
        // where a block stands on the route: past its narrow way in there
        // is no room for one.
        if (p->tier >= 2 && p->type != RROUTE_HILLS && !(p->obstacle && !ObstacleOutside(r, p)) && RogueRng_Range(&rng, 100) < 45)
            p->interiors |= 1 << RINT_REST;
    }

    // Side routes: by their zone too, but a block on the way still needs
    // green country with a narrow way to block (or one beside it, in the
    // lane of the route next door), and water to cross is a lake.
    for (i = 0; i < r->nPlaces; i++)
    {
        struct RoguePlace *p = &r->places[i];
        u8 zone;
        // (v1.1 sea: a side route out to an island stays sea.)
        if (p->kind != RPLACE_ROUTE || !p->side || p->type == RROUTE_SEA)
            continue;
        zone = ZoneAt(r, &zp, p->gridX, p->gridY);
        if (p->obstacle == ROBST_SURF)
            p->type = RROUTE_LAKE;
        else if (p->obstacle)
        {
            u32 f = sZoneKinds[zone][RROUTE_FOREST] + 15, w = sZoneKinds[zone][RROUTE_WOODS] + 10,
                l = sZoneKinds[zone][RROUTE_LAKE] + 10, k = RogueRng_Range(&rng, f + w + l);
            p->type = k < f ? RROUTE_FOREST : k < f + w ? RROUTE_WOODS : RROUTE_LAKE;
        }
        else
        {
            u32 w[RROUTE_COUNT], total = 0, k;
            u8 t;
            for (t = 0; t < RROUTE_COUNT; t++)
            {
                // Not a beach or a river: a side path's double battle and
                // its feature's house need room they do not leave.
                w[t] = ZoneKindAllowed(t, p->tier, FALSE) && t != RROUTE_COAST && t != RROUTE_RIVER ? sZoneKinds[zone][t] + ZONE_SURPRISE : 0;
                total += w[t];
            }
            k = RogueRng_Range(&rng, total);
            for (t = 0; t + 1 < RROUTE_COUNT && k >= w[t]; t++)
                k -= w[t];
            p->type = t;
        }
    }

    // A coast has the sea along a side with no way out, facing the sea.
    for (i = 0; i < r->nPlaces; i++)
    {
        struct RoguePlace *p = &r->places[i];
        if (p->kind != RPLACE_ROUTE || p->type != RROUTE_COAST)
            continue;
        p->seaSide = PickSide(&rng, FreeSidesFacing(p, CELL_SEA));
        if (p->seaSide == RDIR_NONE || (!p->side && p->nLinks >= 3 && MainTurns(r, p)))
        {
            p->type = RROUTE_RIVER;
            p->seaSide = RDIR_NONE;
        }
    }
}

// ---------------------------------------------------------------------------
// Weather routes: a region's rainy route (now and then a stormy one, as
// Routes 119 and 120 are) and its misty one, known for it: the route sign
// says so, it has a local name for it, and the people out on it talk of it.
// Worked out from the plan when asked; nothing is kept.

// How well each kind suits rain, and mist.
static const u8 sRainKinds[RROUTE_COUNT] =
{
    [RROUTE_LAKE] = 4, [RROUTE_RIVER] = 4, [RROUTE_LONGGRASS] = 6, [RROUTE_FOREST] = 2, [RROUTE_COAST] = 2,
    [RROUTE_FIELD] = 1, [RROUTE_FLOWERS] = 1, [RROUTE_HILLS] = 1,
};
static const u8 sFogKinds[RROUTE_COUNT] =
{
    [RROUTE_HILLS] = 4, [RROUTE_LAKE] = 3, [RROUTE_FOREST] = 2, [RROUTE_COAST] = 2, [RROUTE_RIVER] = 1,
    [RROUTE_LONGGRASS] = 1, [RROUTE_FIELD] = 1,
};

// The route known for rain (which 0) or mist (which 1), or 0xFF. About
// four regions in five have a rainy one and half a misty one, from the
// third tier on, as rain is today.
static u8 KnownWeatherRoute(const struct RogueRegion *r, u8 which)
{
    u32 total = 0, k, h = RogueRng_Hash(r->seed, 52200 + which);
    u8 rain = which ? KnownWeatherRoute(r, 0) : 0xFF;
    int i;
    if (h % 100 >= (which ? 50u : 80u))
        return 0xFF;
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        if (p->kind == RPLACE_ROUTE && p->tier >= 2 && p->type < RROUTE_COUNT && i != rain)
            total += which ? sFogKinds[p->type] : sRainKinds[p->type];
    }
    if (total == 0)
        return 0xFF;
    k = (h >> 8) % total;
    for (i = 0; i < r->nPlaces; i++)
    {
        const struct RoguePlace *p = &r->places[i];
        u32 w;
        if (p->kind != RPLACE_ROUTE || p->tier < 2 || p->type >= RROUTE_COUNT || i == rain)
            continue;
        w = which ? sFogKinds[p->type] : sRainKinds[p->type];
        if (k < w)
            return i;
        k -= w;
    }
    return 0xFF;
}

// The weather a route is known for (RWEATHER_RAIN, _STORM or _FOG), or
// RWEATHER_NONE.
u8 RogueGen_KnownWeather(const struct RogueRegion *r, u8 place)
{
    if (RogueGen_IsRun0(r) || place >= r->nPlaces)
        return RWEATHER_NONE;
    if (KnownWeatherRoute(r, 0) == place)
        // A storm on later routes, one in three.
        return r->places[place].tier >= 4 && RogueRng_Hash(r->seed, 52210) % 3 == 0 ? RWEATHER_STORM : RWEATHER_RAIN;
    if (KnownWeatherRoute(r, 1) == place)
        return RWEATHER_FOG;
    return RWEATHER_NONE;
}

// The local name of a weather route, or NULL.
static const char *KnownWeatherNick(const struct RogueRegion *r, u8 place)
{
    static const char *const sRain[] = { "the RAINY REACH", "DRIZZLE LANE", "the WET WAY" };
    static const char *const sStorm[] = { "THUNDER ROAD", "the STORMY WAY", "RUMBLE RIDGE" };
    static const char *const sFog[] = { "the MISTY MOOR", "FOGGY HOLLOW", "the GRAY VEIL" };
    u32 h = RogueRng_Hash(r->seed, 52220) % 3;
    switch (RogueGen_KnownWeather(r, place))
    {
    case RWEATHER_RAIN:  return sRain[h];
    case RWEATHER_STORM: return sStorm[h];
    case RWEATHER_FOG:   return sFog[h];
    }
    return NULL;
}
