#!/usr/bin/env python3
"""Learn how Hoenn's region map joins its tiles, for the region map screen.

Reads graphics/pokenav/region_map/map.png (the tiles) and map.bin (Hoenn's
map) and writes src/data/rogue_region_map.h:

- how dark each tile's land is at its corners, and its edges: every tile
  edge is a column (or row) of 8 pixels;
- the seams: for each pair of edges that can meet, a cost. 0 means the two
  edges meet somewhere on Hoenn's map; otherwise the cost grows with how far
  apart the pixels across the seam are (land against sea, or a light green
  against a dark one);
- the tiles to try for land, coast (by which corners are land) and roads,
  how well each fits, and for each edge the tiles that meet it in a seam
  Hoenn has, so the search on the GBA can try those first.

RogueGen_DrawRegionMap fills the land, the coast and the roads with the
cheapest tiles, so the coast runs on unbroken and the shading flows across
tiles as on Hoenn's map. check_seeds.py uses check_map here to check the
drawn maps.

It also writes src/data/rogue_region_map_crater.h: Mt. Chimney's crater
(four tiles) with its ring and middle in the map's own orange and red, as
FRLG's map draws Mt. Ember, so the volcano reads as one. The map screen puts
them after Hoenn's tiles (RMAP_TILE_CRATER).

Run it again only if map.png or map.bin change.
"""
import os
import random
from collections import Counter

from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
GFX = os.path.join(ROOT, 'graphics', 'pokenav', 'region_map')
OUT = os.path.join(ROOT, 'src', 'data', 'rogue_region_map.h')
OUT_CRATER = os.path.join(ROOT, 'src', 'data', 'rogue_region_map_crater.h')
NUM_TILES = 233
# Mt. Chimney's crater on Hoenn's map (top left, top right, bottom left,
# bottom right); the recoloured copies follow Hoenn's tiles, from NUM_TILES
# on (RMAP_TILE_CRATER in include/rogue_gen.h).
CRATER = [0x1D, 0x1E, 0x2D, 0x2E]
CRATER_RING = 125           # the dark green ring round the crater
CRATER_RING_TO = 134        # dark orange
CRATER_INSIDE_TO = {124: 132, 123: 136, 122: 137}  # gold, orange red, red
MAP_W, MAP_H = 32, 20  # Hoenn's part of map.bin (64 x 64, one byte a tile)

# Palette indices of map.png by what they show. Greens and road yellows are
# listed light to dark.
SEA = (113, 114, 115)
SEA_ROUTE = (116, 117, 118, 119, 120, 121)
GREENS = (122, 123, 124, 125, 126)
SAND = (127, 130)
ROADS = (131, 133, 132, 135, 134)

KIND_OTHER, KIND_SEA, KIND_LAND, KIND_COAST, KIND_ROAD = range(5)
PLAIN = 0x33       # the plain green tile, which Hoenn's map hardly uses
PLAIN_COST = 12
SAND_ROUTE = [0x59, 0x56, 0x2F, 0x67]  # light road tiles for desert routes


def pixel_class(p):
    if p in SEA:
        return 's'
    if p in GREENS:
        return 'g'
    if p in ROADS:
        return 'r'
    if p in SEA_ROUTE:
        return 'b'
    if p in SAND:
        return 'y'
    return 'm'


def pixel_shade(p):
    """Darkness 0 (lightest) to 4 (darkest) of a green or road pixel."""
    if p in GREENS:
        return GREENS.index(p)
    if p in ROADS:
        return min(ROADS.index(p), 3) * 4 / 3
    return 0


class Hoenn:
    def __init__(self):
        img = Image.open(os.path.join(GFX, 'map.png'))
        px = img.load()
        self.pix = [[[px[(t % 16) * 8 + i, (t // 16) * 8 + j] for i in range(8)] for j in range(8)]
                    for t in range(NUM_TILES)]
        data = open(os.path.join(GFX, 'map.bin'), 'rb').read()
        self.map = [[data[y * 64 + x] for x in range(MAP_W)] for y in range(MAP_H)]
        self.kind = [self._kind(t) for t in range(NUM_TILES)]
        self.seams_h = set()
        self.seams_v = set()
        self.pairs_h = set()
        self.pairs_v = set()
        for y in range(MAP_H):
            for x in range(MAP_W):
                a = self.map[y][x]
                if x + 1 < MAP_W:
                    b = self.map[y][x + 1]
                    self.seams_h.add((self.right(a), self.left(b)))
                    self.pairs_h.add((a, b))
                if y + 1 < MAP_H:
                    b = self.map[y + 1][x]
                    self.seams_v.add((self.bottom(a), self.top(b)))
                    self.pairs_v.add((a, b))

    def _kind(self, t):
        c = Counter(pixel_class(p) for row in self.pix[t] for p in row)
        keys = set(c)
        if keys == {'s'}:
            return KIND_SEA
        if keys == {'g'}:
            return KIND_LAND
        if keys == {'g', 's'}:
            return KIND_COAST
        if keys == {'r'}:
            return KIND_ROAD
        return KIND_OTHER

    def left(self, t):
        return tuple(self.pix[t][j][0] for j in range(8))

    def right(self, t):
        return tuple(self.pix[t][j][7] for j in range(8))

    def top(self, t):
        return tuple(self.pix[t][0])

    def bottom(self, t):
        return tuple(self.pix[t][7])

    def seam_h_in_hoenn(self, a, b):
        return (self.right(a), self.left(b)) in self.seams_h

    def seam_v_in_hoenn(self, a, b):
        return (self.bottom(a), self.top(b)) in self.seams_v

    def masks(self, t):
        """Which corners are land (top left 8, top right 4, bottom left 2,
        bottom right 1), judged two ways: by the quarters of the tile and by
        the 3x3 pixels at each corner. A coast tile fits a corner pattern
        that either matches."""
        out = []
        for size in (4, 3):
            m = 0
            for q, (qx, qy) in enumerate([(0, 0), (8 - size, 0), (0, 8 - size), (8 - size, 8 - size)]):
                land = sum(1 for j in range(size) for i in range(size) if pixel_class(self.pix[t][qy + j][qx + i]) == 'g')
                if 2 * land > size * size:
                    m |= 8 >> q
            out.append(m)
        return out

    def corner_shades(self, t):
        """Darkness of the land (or road) at each corner, 0 to 16."""
        out = []
        for qx, qy in [(0, 0), (5, 0), (0, 5), (5, 5)]:
            v = [pixel_shade(self.pix[t][qy + j][qx + i]) for j in range(3) for i in range(3)
                 if pixel_class(self.pix[t][qy + j][qx + i]) in 'gr']
            # An all sea corner counts as dark: it is only ever wanted on the
            # coast, where the land is darkest.
            out.append(round(sum(v) / len(v) * 4) if v else 16)
        return out


def crater_tiles(h):
    """The crater's four tiles recoloured: the ring dark orange and what it
    holds gold to red. The green round it stays, so it keeps its round
    shape on the land. Returns four lists of 64 palette indices."""
    blk = [[h.pix[CRATER[(y >> 3) * 2 + (x >> 3)]][y & 7][x & 7] for x in range(16)] for y in range(16)]
    outside = set()
    stack = [(x, y) for x in range(16) for y in (0, 15)] + [(x, y) for y in range(16) for x in (0, 15)]
    while stack:
        x, y = stack.pop()
        if (x, y) in outside or not (0 <= x < 16 and 0 <= y < 16) or blk[y][x] == CRATER_RING:
            continue
        outside.add((x, y))
        stack += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    inside = {(x, y) for x in range(16) for y in range(16) if blk[y][x] != CRATER_RING and (x, y) not in outside}
    out = [row[:] for row in blk]
    for y in range(16):
        for x in range(16):
            if (x, y) in inside:
                out[y][x] = CRATER_INSIDE_TO.get(blk[y][x], blk[y][x])
            elif blk[y][x] == CRATER_RING and any((x + dx, y + dy) in inside for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
                out[y][x] = CRATER_RING_TO
    return [[out[(k >> 1) * 8 + j][(k & 1) * 8 + i] for j in range(8) for i in range(8)] for k in range(4)]


# (v1.1 towns2) A crater town's mark: a city's or a town's, in the middle of
# Mt. Chimney's crater as Hoenn's map draws it (green, as Sootopolis's ring of
# rock is), four tiles each, after the volcano's at RMAP_TILE_CRATER_TOWN.
CRATER_TOWN_MARKS = [0x14, 0x0D]  # a city's, a town's


def crater_town_tiles(h):
    out = []
    for mark in CRATER_TOWN_MARKS:
        blk = [[h.pix[CRATER[(y >> 3) * 2 + (x >> 3)]][y & 7][x & 7] for x in range(16)] for y in range(16)]
        for j in range(8):
            for i in range(8):
                blk[4 + j][4 + i] = h.pix[mark][j][i]
        out += [[blk[(k >> 1) * 8 + j][(k & 1) * 8 + i] for j in range(8) for i in range(8)] for k in range(4)]
    return out


def write_crater(h):
    tiles = crater_tiles(h)
    out = ['// Generated by tools/roguemon/gen_region_map.py from Hoenn\'s region map', '// (graphics/pokenav/region_map). Do not edit.', '',
           '// Mt. Chimney\'s crater (tiles 0x%02X, 0x%02X, 0x%02X, 0x%02X) in orange and red, as 8bpp' % tuple(CRATER),
           '// tiles in the map\'s palette, loaded after Hoenn\'s at RMAP_TILE_CRATER.',
           'static const u8 ALIGNED(4) sRegionMapCraterGfx[4 * 64] =', '{']
    for t in tiles:
        for j in range(8):
            out.append('    ' + ''.join('%d,' % v for v in t[j * 8:j * 8 + 8]))
    out.append('};')
    out += ['', '// (v1.1 towns2) A crater town: a city\'s mark (0x%02X), then a town\'s (0x%02X), in the middle of' % tuple(CRATER_TOWN_MARKS),
            '// Mt. Chimney\'s green crater, four tiles each, loaded at RMAP_TILE_CRATER_TOWN.',
            'static const u8 ALIGNED(4) sRegionMapCraterTownGfx[8 * 64] =', '{']
    for t in crater_town_tiles(h):
        for j in range(8):
            out.append('    ' + ''.join('%d,' % v for v in t[j * 8:j * 8 + 8]))
    out.append('};')
    open(OUT_CRATER, 'w').write('\n'.join(out) + '\n')


def seam_pixels_cost(ea, eb):
    """How far apart two edges that meet are, pixel by pixel."""
    cost = 0
    for p, q in zip(ea, eb):
        cp, cq = pixel_class(p), pixel_class(q)
        if cp != cq:
            cost += 6
        else:
            d = abs(pixel_shade(p) - pixel_shade(q))
            cost += 0 if d == 0 else 1 if d <= 1 else round(4 * d)
    return cost


def seam_ok(ea, eb):
    """A smooth seam: at most one pixel across it jumps from land to sea or
    by more than one shade."""
    jumps = 0
    for p, q in zip(ea, eb):
        cp, cq = pixel_class(p), pixel_class(q)
        if cp != cq or abs(pixel_shade(p) - pixel_shade(q)) > 1.5:
            jumps += 1
    return jumps <= 1


_hoenn = None


def check_map(tiles):
    """How a drawn region map's tiles join. tiles is a list of rows of tile
    numbers. Counts the seams between land, coast and sea tiles, and between
    road tiles (places and sea routes sit on top with edges of their own, as
    on Hoenn's map): how many are tile pairs found on Hoenn's map, how many
    are seams found there (the same pixels meeting), and how many are smooth
    (found there, or seam_ok). Also counts plain green tiles. Returns the
    counts and the rough seams as (x, y, 'east' or 'south', tile, other)."""
    global _hoenn
    if _hoenn is None:
        _hoenn = Hoenn()
    h = _hoenn
    road = {KIND_ROAD}
    terrain = {KIND_SEA, KIND_LAND, KIND_COAST}
    out = Counter()
    rough = []
    # The crater's recoloured tiles join as Hoenn's own.
    # (A crater town's crater too, v1.1 towns2.)
    tiles = [[CRATER[(t - NUM_TILES) % 4] if NUM_TILES <= t < NUM_TILES + 12 else t for t in row] for row in tiles]
    for y, row in enumerate(tiles):
        for x, a in enumerate(row):
            for dx, dy in ((1, 0), (0, 1)):
                if y + dy >= len(tiles) or x + dx >= len(row):
                    continue
                b = tiles[y + dy][x + dx]
                ka, kb = h.kind[a], h.kind[b]
                if not ((ka in terrain and kb in terrain) or (ka in road and kb in road)) or a == b == 0:
                    continue
                if dx:
                    pair, seam, ea, eb = (a, b) in h.pairs_h, h.seam_h_in_hoenn(a, b), h.right(a), h.left(b)
                else:
                    pair, seam, ea, eb = (a, b) in h.pairs_v, h.seam_v_in_hoenn(a, b), h.bottom(a), h.top(b)
                out['seams'] += 1
                out['pairs in hoenn'] += pair
                out['seams in hoenn'] += seam
                if seam or seam_ok(ea, eb):
                    out['smooth'] += 1
                else:
                    rough.append((x, y, 'east' if dx else 'south', a, b))
    out['plain'] = sum(row.count(0x33) for row in tiles)
    return out, rough


def main():
    h = Hoenn()
    used = [t for t in range(NUM_TILES) if h.kind[t] != KIND_OTHER]
    edges = {}
    for name, fn in (('L', h.left), ('R', h.right), ('T', h.top), ('B', h.bottom)):
        ids = {}
        for t in used:
            ids.setdefault(fn(t), len(ids))
        edges[name] = ids
    assert all(len(ids) < 255 for ids in edges.values())

    def table(first, second, seams):
        rows = []
        for ea in first:
            row = []
            for eb in second:
                row.append(0 if (ea, eb) in seams else min(255, 3 + seam_pixels_cost(ea, eb)))
            rows.append(row)
        return rows

    seam_h = table(list(edges['R']), list(edges['L']), h.seams_h)
    seam_v = table(list(edges['B']), list(edges['T']), h.seams_v)

    def off(t, m):
        return min(bin(k ^ m).count('1') for k in h.masks(t))

    land_tiles = [t for t in used if h.kind[t] == KIND_LAND]
    coast = [t for t in used if h.kind[t] == KIND_COAST]
    road_tiles = [t for t in used if h.kind[t] == KIND_ROAD]

    # Tiles shaded evenly (corners within a shade of each other), for a place
    # with no neighbour to join; the plain green is left out.
    def even(tiles):
        return [t for t in tiles if max(h.corner_shades(t)) - min(h.corner_shades(t)) <= 4 and t != PLAIN]

    def evens_first(tiles):
        return even(tiles) + [t for t in tiles if t not in even(tiles)]

    # The tiles to try for each kind of tile. By the land corners wanted:
    # the coast tiles that fit them exactly, then those one corner off (either
    # way of judging them), or for land by the coast the land tiles. Then
    # inland only land tiles, roads, and for desert routes the light sand of
    # Hoenn's desert.
    exact_fit = [[t for t in coast if off(t, m) == 0] for m in range(16)]
    lists = [exact_fit[m] + ([t for t in coast if off(t, m) == 1] if m != 15 else evens_first(land_tiles))
             for m in range(16)]
    lists += [evens_first(land_tiles), evens_first(road_tiles), SAND_ROUTE]
    cores = [len(l) for l in exact_fit] + [0, 0, 0]
    frees = cores[:15] + [cores[15] + len(even(land_tiles)), len(even(land_tiles)), len(even(road_tiles)), len(SAND_ROUTE)]
    names = ['%s' % format(m, '04b') for m in range(16)] + ['inland', 'road', 'sand road']

    def fit(t, m):
        bits = bin(m).count('1')
        land = sum(1 for row in h.pix[t] for p in row if pixel_class(p) in 'gr')
        return min(255, 24 * off(t, m) + abs(land - 16 * bits) + (PLAIN_COST if t == PLAIN else 0))

    out = ['// Generated by tools/roguemon/gen_region_map.py from Hoenn\'s region map', '// (graphics/pokenav/region_map). Do not edit.', '']
    out.append('// Each tile: its edges\' numbers in the seam tables (left, right, top and')
    out.append('// bottom, a byte each from the lowest) and how dark its land is at each')
    out.append('// corner, 0 to 16 (top left, top right, bottom left, bottom right).')
    out.append('struct RegionMapTile')
    out.append('{')
    out.append('    u32 edges;')
    out.append('    u32 shade;')
    out.append('};')
    out.append('')
    out.append('static const struct RegionMapTile sRegionMapTileInfo[%d] =' % NUM_TILES)
    out.append('{')
    for t in used:
        e = [edges['L'][h.left(t)], edges['R'][h.right(t)], edges['T'][h.top(t)], edges['B'][h.bottom(t)]]
        sh = h.corner_shades(t)
        out.append('    [0x%02X] = { 0x%08X, 0x%08X },' % (
            t, sum(v << (8 * i) for i, v in enumerate(e)), sum(v << (8 * i) for i, v in enumerate(sh))))
    out.append('};')
    out.append('')
    out.append('#define RMAP_LIST_INLAND 16')
    out.append('#define RMAP_LIST_ROAD   17')
    out.append('#define RMAP_LIST_SAND   18')
    out.append('')
    out.append('// The tiles to try, list n from sRegionMapListStart[n] to [n + 1]: 0 to 15')
    out.append('// by the land corners wanted (top left 8, top right 4, bottom left 2, bottom')
    out.append('// right 1), then inland, roads and desert roads.')
    starts = [0]
    for l in lists:
        starts.append(starts[-1] + len(l))
    out.append('static const u16 sRegionMapListStart[%d] = { %s };' % (len(starts), ', '.join(str(v) for v in starts)))
    out.append('// How many at the start of each list to try first: the coast tiles that fit')
    out.append('// the corners exactly (Core); where no neighbour is known, those and the')
    out.append('// tiles shaded evenly (Free).')
    out.append('static const u8 sRegionMapListCore[%d] = { %s };' % (len(cores), ', '.join(str(v) for v in cores)))
    out.append('static const u8 sRegionMapListFree[%d] = { %s };' % (len(frees), ', '.join(str(v) for v in frees)))
    out.append('static const u8 sRegionMapLists[] =')
    out.append('{')
    for name, l in zip(names, lists):
        out.append('    // %s' % name)
        for i in range(0, len(l), 16):
            out.append('    ' + ', '.join('0x%02X' % t for t in l[i:i + 16]) + ',')
    out.append('};')
    out.append('')
    out.append('// [list][tile]: how badly a tile on the list fits, 255 when it is not on')
    out.append('// the list. For land and coast: corners off, too much or too little land')
    out.append('// (a coast frayed into specks), and the plain green that Hoenn hardly uses.')
    out.append('static const u8 sRegionMapFit[%d][%d] =' % (len(lists), NUM_TILES))
    out.append('{')
    for n, l in enumerate(lists):
        row = [255] * NUM_TILES
        for t in l:
            row[t] = fit(t, n) if n < 16 else fit(t, 15) if n == 16 else 0
        out.append('    {' + ','.join(str(v) for v in row) + '},')
    out.append('};')
    out.append('')
    # For each edge, the tiles that meet it in a seam Hoenn's map has: land
    # and coast tiles for land and coast, road tiles for roads.
    groups = {'Land': land_tiles, 'Coast': coast, 'Road': road_tiles}
    what = {'H': 'east of a right edge', 'HT': 'west of a left edge', 'V': 'south of a bottom edge', 'VT': 'north of a top edge'}
    for g, tiles in groups.items():
        exact = {
            'H': [[c for c in tiles if (e, h.left(c)) in h.seams_h] for e in edges['R']],
            'HT': [[c for c in tiles if (h.right(c), e) in h.seams_h] for e in edges['L']],
            'V': [[c for c in tiles if (e, h.top(c)) in h.seams_v] for e in edges['B']],
            'VT': [[c for c in tiles if (h.bottom(c), e) in h.seams_v] for e in edges['T']],
        }
        for k, lists_k in exact.items():
            starts = [0]
            for l in lists_k:
                starts.append(starts[-1] + len(l))
            out.append('// The %s tiles that join %s in a seam Hoenn has, from' % (g.lower(), what[k]))
            out.append('// sRegionMapExact%s%sStart[edge] to [edge + 1].' % (g, k))
            out.append('static const u16 sRegionMapExact%s%sStart[%d] = { %s };' % (g, k, len(starts), ', '.join(str(v) for v in starts)))
            out.append('static const u8 sRegionMapExact%s%s[] =' % (g, k))
            out.append('{')
            for l in lists_k:
                if l:
                    out.append('    ' + ''.join('%d,' % t for t in l))
            out.append('};')
            out.append('')

    # A little noise for the search, 0 to 7, so like places do not all get
    # like tiles; cheaper on the GBA than hashing each tile tried.
    rng = random.Random(233)
    out.append('static const u8 sRegionMapJitter[256] =')
    out.append('{')
    noise = [rng.randrange(8) for _ in range(256)]
    for i in range(0, 256, 32):
        out.append('    ' + ''.join('%d,' % v for v in noise[i:i + 32]))
    out.append('};')
    out.append('')

    def emit(name, rows, comment):
        out.append('// ' + comment)
        out.append('static const u8 sRegionMap%s[%d][%d] =' % (name, len(rows), len(rows[0])))
        out.append('{')
        for row in rows:
            out.append('    {' + ','.join(str(v) for v in row) + '},')
        out.append('};')
        out.append('')
    emit('SeamH', seam_h, '[right edge of the west tile][left edge of the east tile]: 0 where Hoenn has the seam')
    emit('SeamHT', [list(r) for r in zip(*seam_h)], 'The same, [left edge][right edge]')
    emit('SeamV', seam_v, '[bottom edge of the north tile][top edge of the south tile]')
    emit('SeamVT', [list(r) for r in zip(*seam_v)], 'The same, [top edge][bottom edge]')
    open(OUT, 'w').write('\n'.join(out))
    write_crater(h)
    print('wrote %s: %d tiles, %d x %d and %d x %d seams' % (
        os.path.relpath(OUT, ROOT), len(used), len(seam_h), len(seam_h[0]), len(seam_v), len(seam_v[0])))


if __name__ == '__main__':
    main()
