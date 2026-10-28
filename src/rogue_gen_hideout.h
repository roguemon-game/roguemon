// The villain teams: one, two or three of Team Aqua, Team Magma and Team
// Rocket per region (about six regions in ten one, three two, one three).
// Included by rogue_gen_dungeon.h.
//
// The main team (team 0) keeps a hideout under a town on the main road,
// between the second and the sixth gym. Its grunts stand in that town's way
// on, as Team Aqua's do in Slateport, until the admin at the bottom of the
// hideout is beaten. The hideout is entered from the town itself, so it is
// always reachable before the blockade. One or two grunts show up on a
// route of the stretch before, so the player has met the team by then.
//
// The other teams keep a hideout each under another town of that stretch,
// with no blockade (an optional dungeon, as the main team's is where no town
// can hold one), their own grunts on a route before it, and there a grunt
// of the team before them: where two teams meet, they quarrel.
//
// The blockade is a gate on the town's link onward (ROGUE_GATE_VILLAIN) and a
// block on the main road for GatingHolds, passed only with ROGUE_KEY_VILLAIN:
// a town is only taken if no shortcut or fork leads round it, even one that
// opens only past it (NoWayRound).

#include "constants/rogue.h"

// How many teams the seed asks for, and who they are in order (ids may be
// NULL). From the seed alone: the leaders' draw asks it too.
u8 RogueGen_TeamsWanted(u32 seed, u8 *ids)
{
    u32 rng = RogueRng_Hash(seed, 14010), roll = RogueRng_Range(&rng, 100);
    u8 order[ROGUE_VILLAIN_IDS] = { ROGUE_VILLAIN_AQUA, ROGUE_VILLAIN_MAGMA, ROGUE_VILLAIN_ROCKET }, i;
    for (i = ROGUE_VILLAIN_IDS - 1; i > 0; i--)
    {
        u8 j = RogueRng_Range(&rng, i + 1), t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    if (ids != NULL)
        for (i = 0; i < ROGUE_VILLAIN_IDS; i++)
            ids[i] = order[i];
    return roll < 57 ? 1 : roll < 88 ? 2 : 3;
}

u8 RogueGen_TeamCount(const struct RogueRegion *r)
{
    u8 n = 0, want = RogueGen_TeamsWanted(r->seed, NULL), i;
    for (i = 0; i < r->nDungeons; i++)
        n += r->dungeons[i].kind == RDUN_HIDEOUT;
    return n < want ? n : want;
}

u8 RogueGen_TeamId(const struct RogueRegion *r, u8 team)
{
    u8 ids[ROGUE_VILLAIN_IDS];
    RogueGen_TeamsWanted(r->seed, ids);
    return ids[team % ROGUE_VILLAIN_IDS];
}

u8 RogueGen_VillainTeam(const struct RogueRegion *r)
{
    return RogueGen_TeamId(r, 0);
}

s32 RogueGen_TeamHideout(const struct RogueRegion *r, u8 team)
{
    int i;
    if (team >= RogueGen_TeamsWanted(r->seed, NULL))
        return -1;
    for (i = 0; i < r->nDungeons; i++)
        if (r->dungeons[i].kind == RDUN_HIDEOUT && team-- == 0)
            return i;
    return -1;
}

s32 RogueGen_Hideout(const struct RogueRegion *r)
{
    return RogueGen_TeamHideout(r, 0);
}

u8 RogueGen_HideoutTeam(const struct RogueRegion *r, s32 dungeon)
{
    u8 t = 0;
    int i;
    if (dungeon < 0 || dungeon >= r->nDungeons || r->dungeons[dungeon].kind != RDUN_HIDEOUT)
        return 0xFF;
    for (i = 0; i < dungeon; i++)
        t += r->dungeons[i].kind == RDUN_HIDEOUT;
    return t < RogueGen_TeamsWanted(r->seed, NULL) ? t : 0xFF;
}

u8 RogueGen_TeamWithId(const struct RogueRegion *r, u8 id)
{
    u8 t, n = RogueGen_TeamCount(r);
    for (t = 0; t < n; t++)
        if (RogueGen_TeamId(r, t) == id)
            return t;
    return 0xFF;
}

// The trainer slots of each team's admin and leader: the main team's as
// before v0.13, the others' after the 240 slots.
static const u8 sTeamAdminSlots[ROGUE_MAX_TEAMS] = { ROGUE_SLOT_VILLAIN_ADMIN, ROGUE_SLOT_VILLAIN_ADMIN_1, ROGUE_SLOT_VILLAIN_ADMIN_2 };
static const u8 sTeamLeaderSlots[ROGUE_MAX_TEAMS] = { ROGUE_SLOT_VILLAIN_LEADER, ROGUE_SLOT_VILLAIN_LEADER_1, ROGUE_SLOT_VILLAIN_LEADER_2 };

u8 RogueGen_TeamAdminSlot(u8 team)
{
    return sTeamAdminSlots[team % ROGUE_MAX_TEAMS];
}

u8 RogueGen_TeamLeaderSlot(u8 team)
{
    return sTeamLeaderSlots[team % ROGUE_MAX_TEAMS];
}

// The route where team t's grunts first show up, before its hideout's
// town, or 0xFF; mask gets a bit for each of its trainers who is one of
// them. taken: trainers of routes another team already holds (their
// routes in takenAt). The main team's is as it was before v0.13.
static u8 TeamHint(const struct RogueRegion *r, u8 team, s32 h, const u8 *takenAt, const u8 *taken, u8 nTaken, u8 *mask)
{
    u8 cand[8];
    int i, n = 0, want, k;
    u32 rng;
    const struct RoguePlace *p;

    *mask = 0;
    if (h < 0)
        return 0xFF;
    // Main road routes back to the town before the hideout's, or past it
    // when that stretch is only the sea: the main road is the first places,
    // in order (BuildSpine).
    for (i = r->dungeons[h].host - 1, k = 0; i > 0 && n < 8; i--)
    {
        p = &r->places[i];
        if (p->kind == RPLACE_TOWN && (n > 0 || ++k > 1))
            break;
        if (p->kind == RPLACE_ROUTE && !p->side && !IsSeaRoute(p) && p->type != RROUTE_MOUNTAIN && p->nTrainers >= 2)
            cand[n++] = i;
    }
    rng = RogueRng_Hash(r->seed, team == 0 ? 14001 : 14030 + team);
    while (n > 0)
    {
        int c = RogueRng_Range(&rng, n);
        u8 busy = 0, j;
        i = cand[c];
        cand[c] = cand[--n];
        p = &r->places[i];
        for (j = 0; j < nTaken; j++)
            if (takenAt[j] == i)
                busy |= taken[j];
        // One or two, never the whole route, and never one the map puts by
        // the water (those are Fishermen and Swimmers).
        want = 1 + RogueRng_Range(&rng, 2);
        if (want > p->nTrainers - 1 - __builtin_popcount(busy))
            want = p->nTrainers - 1 - __builtin_popcount(busy);
        for (k = p->nTrainers - 1; k >= 0 && want > 0; k--)
        {
            if (RogueGen_TrainerByWater(r, i, k) || ((busy >> k) & 1))
                continue;
            *mask |= 1 << k;
            want--;
        }
        if (*mask)
            return i;
    }
    return 0xFF;
}

u8 RogueGen_VillainHint(const struct RogueRegion *r, u8 *mask)
{
    return TeamHint(r, 0, RogueGen_Hideout(r), NULL, NULL, 0, mask);
}

void RogueGen_Teams(const struct RogueRegion *r, struct RogueTeams *tm)
{
    u8 want = RogueGen_TeamsWanted(r->seed, tm->id), t, taken[ROGUE_MAX_TEAMS], k;
    int i;
    tm->n = 0;
    for (i = 0; i < r->nDungeons && tm->n < want; i++)
        if (r->dungeons[i].kind == RDUN_HIDEOUT)
            tm->hideout[tm->n++] = i;
    for (t = 0; t < ROGUE_MAX_TEAMS; t++)
    {
        tm->hint[t] = 0xFF;
        tm->mask[t] = tm->guest[t] = 0;
        if (t >= tm->n)
        {
            tm->hideout[t] = -1;
            continue;
        }
        tm->hint[t] = TeamHint(r, t, tm->hideout[t], tm->hint, taken, t, &tm->mask[t]);
        // A grunt of the team before on the same route, where they meet:
        // never the whole route, never one by the water.
        if (t > 0 && tm->hint[t] != 0xFF)
        {
            const struct RoguePlace *p = &r->places[tm->hint[t]];
            u8 busy = tm->mask[t];
            for (k = 0; k < t; k++)
                if (tm->hint[k] == tm->hint[t])
                    busy |= tm->mask[k] | tm->guest[k];
            for (k = 0; k < p->nTrainers && __builtin_popcount(busy) + 1 < p->nTrainers; k++)
            {
                if (((busy >> k) & 1) || RogueGen_TrainerByWater(r, tm->hint[t], k))
                    continue;
                tm->guest[t] = 1 << k;
                break;
            }
        }
        taken[t] = tm->mask[t] | tm->guest[t];
    }
}

u8 RogueGen_SlotTeam(const struct RogueRegion *r, const struct RogueTeams *tm, u32 slot, u8 *role, bool8 *clash)
{
    u8 t;
    *role = RVROLE_NONE;
    *clash = FALSE;
    for (t = 0; t < tm->n; t++)
    {
        if (slot == RogueGen_TeamAdminSlot(t))
            return *role = RVROLE_ADMIN, t;
        if (slot == RogueGen_TeamLeaderSlot(t))
            return *role = RVROLE_LEADER, t;
    }
    if (slot >= ROGUE_SLOT_GYM_0)
        return 0xFF;
    for (t = 0; t < tm->n; t++)
    {
        const struct RogueDungeon *d = &r->dungeons[tm->hideout[t]];
        if (d->nTrainers && slot >= d->trainerBase && slot < (u32)d->trainerBase + d->nTrainers)
        {
            // Where the BOSS waits at the bottom himself, his ADMIN is the
            // last of the hideout's trainers before him (the very last slot
            // is the boss's own, BossIsTrainer).
            if (d->nTrainers >= 2 && slot == (u32)d->trainerBase + d->nTrainers - 2 && RogueGen_TeamBossInHideout(r, t))
                return *role = RVROLE_ADMIN, t;
            return *role = RVROLE_GRUNT, t;
        }
        if (tm->hint[t] != 0xFF)
        {
            const struct RoguePlace *p = &r->places[tm->hint[t]];
            u32 k = slot - p->trainerBase;
            if (slot < p->trainerBase || k >= p->nTrainers)
                continue;
            if ((tm->mask[t] >> k) & 1)
                return *role = RVROLE_GRUNT, *clash = tm->guest[t] != 0, t;
            if ((tm->guest[t] >> k) & 1)
                return *role = RVROLE_GRUNT, *clash = TRUE, t - 1;
        }
    }
    return 0xFF;
}

// TEAM ROCKET's BOSS waits at the bottom of his hideout himself, as in
// FireRed's Rocket Hideout, where no legend's way is his to keep: then he,
// not an ADMIN, is the hideout's boss.
bool8 RogueGen_TeamBossInHideout(const struct RogueRegion *r, u8 team)
{
    return team < RogueGen_TeamCount(r) && RogueGen_TeamId(r, team) == ROGUE_VILLAIN_ROCKET
        && RogueGen_TeamLeaderDungeon(r, team) < 0;
}

s32 RogueGen_TeamLeaderDungeon(const struct RogueRegion *r, u8 team)
{
    int i;
    if (RogueGen_TeamHideout(r, team) < 0)
        return -1;
    for (i = 0; i < r->nDungeons; i++)
        if (r->dungeons[i].boss == RBOSS_LEGEND && ROGUE_GATE_KIND(r->dungeons[i].gate) == RGATE_VILLAIN
         && ROGUE_GATE_SRC(r->dungeons[i].gate) == team)
            return i;
    return -1;
}

// Two grunts on the exit's edge row, facing into the town; the gate's
// coord events stand on the row in front of them (AddGate).
static void AddBlockade(struct Gen *gen, u8 dir)
{
    int pos = gen->exitPos[dir] / 2, x, y, i;
    EdgeCell(gen, dir, pos, 0, &x, &y);
    for (i = 0; i < 2; i++)
    {
        int fx, fy;
        switch (dir)
        {
        case RDIR_NORTH: fx = x * 2 + i; fy = 0; break;
        case RDIR_SOUTH: fx = x * 2 + i; fy = gen->m->h - 1; break;
        case RDIR_WEST:  fx = 0; fy = y * 2 + i; break;
        default:         fx = gen->m->w - 1; fy = y * 2 + i; break;
        }
        AddObj(gen, ROBJ_GRUNT, fx, fy, Opposite(dir), i, 0);
    }
}

// The blockade as a block on the main road at step at, kept in order.
static void InsertVillainBlock(int at)
{
    int j = sNBlocks, k;
    while (j > 0 && sBlockAt[j - 1] > at)
        j--;
    for (k = sNBlocks; k > j; k--)
    {
        sBlockAt[k] = sBlockAt[k - 1];
        sBlockGym[k] = sBlockGym[k - 1];
        sBlockKind[k] = sBlockKind[k - 1];
        sBlockKeys[k] = sBlockKeys[k - 1];
        sBlockFork[k] = sBlockFork[k - 1];
    }
    sBlockAt[j] = at;
    sBlockGym[j] = 0xFF; // ends no chapter
    sBlockKind[j] = ROBST_NONE;
    sBlockKeys[j] = ROGUE_KEY_VILLAIN;
    sBlockFork[j] = ROBST_NONE;
    sNBlocks++;
}

static void RemoveVillainBlock(void)
{
    int j, k;
    for (j = 0; j < sNBlocks; j++)
    {
        if (sBlockKeys[j] != ROGUE_KEY_VILLAIN)
            continue;
        for (k = j; k + 1 < sNBlocks; k++)
        {
            sBlockAt[k] = sBlockAt[k + 1];
            sBlockGym[k] = sBlockGym[k + 1];
            sBlockKind[k] = sBlockKind[k + 1];
            sBlockKeys[k] = sBlockKeys[k + 1];
            sBlockFork[k] = sBlockFork[k + 1];
        }
        sNBlocks--;
        return;
    }
}

// Every region has its villain team, as every real region does: a hideout
// under a gym city or village of the main road from the second gym to the
// sixth, whose way on is a land route. Its grunts block that way on until
// the admin is beaten, where no shortcut or fork leads round them; where
// one does in every such town, the hideout is still there, under the first
// town tried, with no blockade: an optional dungeon (as Emerald's Team Aqua
// keeps Slateport's museum with nobody in the way), its grunts on the road
// before, the admin at the bottom and the leader later at the legend's.
// A town whose landmark would stand along its top, where a hideout's door
// would take its place (src/rogue_gen_signatures.h): the hot spring under a
// spring town's red rock, the Space Center, the Weather Institute, a mine.
static bool8 HideoutTakesLandmark(const struct RogueRegion *r, u8 place)
{
    const struct RoguePlace *p = &r->places[place];
    return p->theme == RTHEME_SPRING || (p->theme == RTHEME_ISLAND && p->type == RTOWN_GYM)
        || (p->theme == RTHEME_TREETOP && p->type != RTOWN_HAMLET) || (p->theme == RTHEME_STONE && p->mouth == 0xFF);
}

// TRUE if a hideout here has a way in of its own (HideoutEntrance in
// rogue_gen_rooms.h), not one of the town's homes: hideouts go under such
// towns where the road has one, as towers do (TowerTownOk).
static bool8 HideoutTownOk(const struct RogueRegion *r, u8 place)
{
    return HideoutEntrance(r, &r->places[place]) != RENTRY_HOUSE;
}

// Which of two towns the hideout with no blockade goes under: one with a
// way in of its own, then one keeping its landmark.
static int HideoutRank(const struct RogueRegion *r, u8 place)
{
    return (HideoutTownOk(r, place) ? 0 : 2) + (HideoutTakesLandmark(r, place) ? 1 : 0);
}

// The rooms team t's hideout is built from, each team its own look:
// Team Magma's volcanic halls (ROOM_STYLE_MAGMA), Team Aqua's halls round the
// pools and the submarine dock (Ruby's Aqua Hideout, ROOM_STYLE_AQUA), Team
// Rocket's plain indoor base (Emerald's Aqua Hideout with no water, or
// Ruby's store rooms, ROOM_STYLE_ROCKET). From the seed alone, so planning
// can ask before the hideout is there.
static u8 TeamStyle(const struct RogueRegion *r, u8 team)
{
    u8 ids[ROGUE_VILLAIN_IDS];
    RogueGen_TeamsWanted(r->seed, ids);
    switch (ids[team % ROGUE_VILLAIN_IDS])
    {
    case ROGUE_VILLAIN_AQUA:  return ROOM_STYLE_AQUA;
    case ROGUE_VILLAIN_MAGMA: return ROOM_STYLE_MAGMA;
    default:                  return ROOM_STYLE_ROCKET;
    }
}

static void PlanHideout(struct RogueRegion *r)
{
    u32 rng = RogueRng_Hash(r->seed, 14002);
    u8 spine[SPINE_MAX], cand[SPINE_MAX];
    int i, n = 0, nOk = 0, first = -1;
    // A region that had no team before v0.13 (this roll) keeps its towns'
    // landmarks: its hideout goes under another town where one fits.
    bool8 added = RogueRng_Range(&rng, 100) >= 85;
    for (i = 0; i < SPINE_LEN; i++)
        spine[i] = i;
    for (i = GymStep(1); i <= GymStep(5) && i + 1 < SPINE_LEN; i++)
    {
        const struct RoguePlace *p = &r->places[i], *q = &r->places[i + 1];
        const struct RogueLink *l = RogueGen_FindLink(p, i + 1);
        if (p->kind != RPLACE_TOWN || (p->type != RTOWN_GYM && p->type != RTOWN_VILLAGE) || TOWNS2_TOWN(p))
            continue;
        if (l == NULL || l->gate || q->kind != RPLACE_ROUTE || IsSeaRoute(q) || q->type == RROUTE_MOUNTAIN)
            continue;
        if (RogueGen_DungeonAt(r, i) >= 0)
            continue;
        cand[n++] = i;
    }
    if (added)
    {
        int keep = 0;
        for (i = 0; i < n; i++)
            if (!HideoutTakesLandmark(r, cand[i]))
                cand[keep++] = cand[i];
        if (keep > 0)
            n = keep;
    }
    // The towns where the hideout has a way in of its own are tried first
    // (cand[0] to cand[nOk - 1]).
    for (i = 0; i < n; i++)
    {
        if (HideoutTownOk(r, cand[i]))
        {
            u8 t = cand[i];
            cand[i] = cand[nOk];
            cand[nOk++] = t;
        }
    }
    while (n > 0)
    {
        int k = RogueRng_Range(&rng, nOk > 0 ? nOk : n), town = cand[k];
        struct RogueLink *l = (struct RogueLink *)RogueGen_FindLink(&r->places[town], town + 1);
        u8 floors = 2 + RogueRng_Range(&rng, RoomStyleFloors(TeamStyle(r, 0)) > 2 ? 2 : 1);
        if (nOk > 0)
        {
            cand[k] = cand[--nOk];
            cand[nOk] = cand[--n];
        }
        else
        {
            cand[k] = cand[--n];
        }
        if (first < 0 || HideoutRank(r, town) < HideoutRank(r, first & 0xFF))
            first = town | (floors << 8);
        if (sNBlocks >= MAX_BLOCKS)
            break;
        // Two or three floors, as the team's own rooms allow (Aqua's make two).
        if (!AddDungeon(r, RDUN_HIDEOUT, town, floors, RBOSS_ADMIN, TRUE))
            return;
        l->gate = ROGUE_GATE_VILLAIN;
        InsertVillainBlock(town + 1);
        if (GatingHolds(r, spine) && NoWayRound(r, spine, ROGUE_KEY_VILLAIN))
            return;
        // A shortcut or fork leads round it: try another town.
        RemoveVillainBlock();
        l->gate = 0;
        r->nDungeons--;
    }
    // No town holds a blockade: the hideout with none.
    if (first >= 0)
        AddDungeon(r, RDUN_HIDEOUT, first & 0xFF, first >> 8, RBOSS_ADMIN, FALSE);
}

// The other teams' hideouts, before the legends pick their homes (keeping
// two dungeons' room for their lairs; a wreck and a power plant take what
// is left after): each under a gym city or village of
// the main road from the second gym to the sixth with no dungeon of its own,
// as far from the other teams' towns as the road allows, keeping the
// towns' landmarks where it can. No blockade: an optional dungeon, as the
// main team's is where no town can hold one. A region with no room left
// (ten dungeons, or no such town) has fewer teams.
// Slots a region keeps for its legends' lairs.
#define TEAM_LEGEND_ROOM 2
static void PlanTeamHideouts(struct RogueRegion *r)
{
    u8 want = RogueGen_TeamsWanted(r->seed, NULL), t;
    u32 rng = RogueRng_Hash(r->seed, 14040);
    if (RogueGen_Hideout(r) < 0)
        return;
    for (t = 1; t < want && r->nDungeons < ROGUE_MAX_DUNGEONS - TEAM_LEGEND_ROOM; t++)
    {
        int i, best = -1, bestScore = -1000;
        u8 floors;
        for (i = GymStep(1); i <= GymStep(5) && i < SPINE_LEN; i++)
        {
            const struct RoguePlace *p = &r->places[i];
            int score, j, near = 99;
            if (p->kind != RPLACE_TOWN || (p->type != RTOWN_GYM && p->type != RTOWN_VILLAGE) || p->side || TOWNS2_TOWN(p))
                continue;
            if (RogueGen_DungeonAt(r, i) >= 0)
                continue;
            // Away from the other teams' towns along the road.
            for (j = 0; j < r->nDungeons; j++)
                if (r->dungeons[j].kind == RDUN_HIDEOUT && abs((int)r->dungeons[j].host - i) < near)
                    near = abs((int)r->dungeons[j].host - i);
            score = (near > 6 ? 6 : near) * 4 + RogueRng_Range(&rng, 8) - (HideoutTakesLandmark(r, i) ? 20 : 0)
                  - (HideoutTownOk(r, i) ? 0 : 40);
            if (score > bestScore)
                bestScore = score, best = i;
        }
        if (best < 0)
            break;
        floors = 2 + RogueRng_Range(&rng, RoomStyleFloors(TeamStyle(r, t)) > 2 ? 2 : 1);
        if (!AddDungeon(r, RDUN_HIDEOUT, best, floors, RBOSS_ADMIN, FALSE))
            break;
    }
}

// TRUE if the team's grunts block the hideout's town's way on (most
// regions); FALSE where the hideout stands with no blockade, or there is
// none.
bool8 RogueGen_VillainBlockade(const struct RogueRegion *r)
{
    s32 h = RogueGen_Hideout(r);
    return h >= 0 && r->dungeons[h].required;
}
