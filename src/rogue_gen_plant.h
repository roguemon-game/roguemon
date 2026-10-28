// Power plants (RDUN_PLANT): New Mauville at the end of a side route.
// Included by rogue_gen_dungeon.h.
//
// As New Mauville is entered from Route 110 by a door in a mound of rock, a
// power plant is a dead-end side route that ends at a rock face with a door
// in it, as a route up to a cave does (GenRoute's mesa), the door leading
// down into one floor: New Mauville's generator hall as it is
// (src/rogue_gen_rooms.h, gen_rooms.py), where blue and green switches
// raise and lower barriers, Voltorb pose as item balls, and an electric
// guardian waits by the generator. It takes the place of the grove off such
// a route where there is one, so the dungeon count stays as it was; else it
// is added while there is room under the cap.

// The power plant at the end of this route, or -1.
static s32 PlantOf(struct Gen *gen)
{
    s32 di;
    if (sGenFloor)
        return -1;
    di = RogueGen_DungeonAt(gen->r, (u8)(gen->p - gen->r->places));
    return (di >= 0 && gen->r->dungeons[di].kind == RDUN_PLANT) ? di : -1;
}

// A side route with one way in, not from the north (the rock face stands
// along the top), nothing at its end, and nothing in it but a grove that
// may give way.
static bool8 PlantRouteOk(const struct RogueRegion *r, u8 place)
{
    const struct RoguePlace *p = &r->places[place];
    s32 di = RogueGen_DungeonAt(r, place);
    if (p->kind != RPLACE_ROUTE || !p->side || p->nLinks != 1 || p->mouth != 0xFF || p->links[0].dir == RDIR_NORTH
     || p->type == RROUTE_SEA || p->type == RROUTE_MOUNTAIN)
        return FALSE;
    if (di < 0)
        return r->nDungeons < ROGUE_MAX_DUNGEONS;
    return r->dungeons[di].kind == RDUN_FOREST && r->dungeons[di].host == place && r->dungeons[di].exit == 0xFF
        && r->dungeons[di].boss != RBOSS_LEGEND && !r->dungeons[di].legend;
}

// About two regions in three with such a route have one.
static void PlanPlantDungeons(struct RogueRegion *r)
{
    u32 rng = RogueRng_Hash(r->seed, 11950), total = 0;
    u8 i, cand[ROGUE_MAX_PLACES], host;
    s32 di;

    if (RogueRng_Range(&rng, 100) >= 70)
        return;
    for (i = 0; i < r->nPlaces; i++)
        if (PlantRouteOk(r, i))
            cand[total++] = i;
    if (total == 0)
        return;
    host = cand[RogueRng_Range(&rng, total)];
    di = RogueGen_DungeonAt(r, host);
    if (di < 0)
    {
        AddDungeon(r, RDUN_PLANT, host, 1, RBOSS_GUARDIAN, FALSE);
        return;
    }
    r->dungeons[di].kind = RDUN_PLANT;
    r->dungeons[di].nFloors = 1;
    r->dungeons[di].boss = RBOSS_GUARDIAN;
}
