#ifndef GUARD_POKEDEX_AREA_REGION_MAP_H
#define GUARD_POKEDEX_AREA_REGION_MAP_H

void LoadPokedexAreaMapGfx();
bool32 TryShowPokedexAreaMap(void);
void PokedexAreaMapChangeBgY(u32 move);

// roguemon: where each place sits on the AREA page's map.
extern s8 gRogueDexPlaceX[];
extern s8 gRogueDexPlaceY[];

#endif // GUARD_POKEDEX_AREA_REGION_MAP_H
