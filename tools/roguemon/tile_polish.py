#!/usr/bin/env python3
"""Find tiling that looks unfinished in generated towns and routes.

The bar is a real Gen 3 map: paths at least two blocks wide with their edges
on the outside, trees whole, ledges with both ends, fences and flower beds
closed, buildings on their own ground, no lone odd block. split_check.py and
seam_check.py judge tree, bank and cliff pieces against the Hoenn maps; this
judges shapes, from the generated metatile grids:

  path_edge     a block of a path, sand road, town road or ash patch whose
                edge pieces do not match the open ground beside it: an edge
                drawn inside the path, or a side left open against grass
  path_thin     a path block with open ground on both sides: a path one
                block wide or tall, which no edge set can draw
  path_scrap    a piece of path of four blocks or fewer, standing alone
  path_gap      a trail broken across its whole width by one or two blocks
                of grass between two pieces of it in a line
  ground_scrap  one to three blocks of one ground (grass, sand, ash) shut in
                by another
  ground_clash  a stamped building, rock or fence whose own ground (sand,
                grass, ash dirt, Rustboro's paving) is not the ground round it
  ledge_end     a ledge piece whose ends are missing or doubled
  ledge_short   a ledge of two blocks or fewer
  fence         a flower bed's fence that does not close
  door_mat      Petalburg's door patch (0x201) drawn in another tileset
  tip_floating  a tree's tips with no crown under them
  crown_cut     a tree's crown with no trunk or crown under it
  half_tree     half a tree without its other half beside it

Usage:
  tile_polish.py [--seeds 200] [--first 1] [--crops DIR] [--per 12] [--json OUT]

Prints each kind per map and exits 1 when one is over its budget (BUDGET,
per town or route); check_seeds.py checks the same budgets.
"""
import argparse
import json
import os
import sys
from collections import Counter, defaultdict
from concurrent.futures import ProcessPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rogue_emu as P  # noqa: E402

DIRS = ((0, -1), (1, 0), (0, 1), (-1, 0))  # N E S W

ROUTE_KINDS = ['forest', 'lake', 'mountain', 'coast', 'river', 'hills', 'field', 'desert', 'sea', 'woods', 'flowers', 'longgrass', 'bridge']
THEMES = ['meadow', 'verdant', 'harbor', 'island', 'stone', 'cove', 'beach', 'ash', 'spring', 'treetop', 'raft', 'crater']


def set3(tl, centre=None):
    """A 3x3 edge set: block id -> (row, col)."""
    out = {tl + r * 8 + c: (r, c) for r in range(3) for c in range(3)}
    if centre is not None:
        out[centre] = (1, 1)
    return out


# 3x3 sets drawn by which neighbours share the cover (PathBlock), by
# tileset (None: every tileset, General's own).
PATH_SETS = [
    ('path', None, set3(0x1D0)),
    ('sand', None, set3(0x118)),
    ('road', 'mauville', set3(0x100)),
    ('road', 'slateport', set3(0x208)),
    ('road', 'rustboro', set3(0x2E0, 0x2BB)),
    ('road', 'lilycove', set3(0x22A)),
    ('road', 'fallarbor', set3(0x2E8)),
    ('road', 'fortree', set3(0x258)),
    ('ash', 'fallarbor', set3(0x2D0)),
    ('pinkash', 'fallarbor', set3(0x2D3)),
]
# Blocks that count as more of a set's own ground: the plain inside a set
# is drawn with (open sand, ash, ash grass, trees standing on it).
ASH_INSIDE = {0x218, 0x20A, 0x204, 0x205, 0x20C, 0x20D, 0x26D, 0x26E, 0x275, 0x276}
WAYS = ('path', 'sand', 'road')
ASHES = ('ash', 'pinkash')
# Stone steps, which a path runs on to.
STEPS = {0x0AF, 0x0CF}
SAME_GROUND = {
    'path': STEPS,
    'road': STEPS,
    'sand': {0x124, 0x11C, 0x13D, 0x13E} | STEPS,
    'ash': ASH_INSIDE,
    'pinkash': ASH_INSIDE,
}

# Island trees stand on sand, so a sand road runs on under their edge.
ISLAND_TREES = {0x239, 0x23A, 0x243}
ISLAND_TREE_SETS = {'dewford'}
# Pieces of a plaza stamped beside a road (Rustboro's lamps and fountain on
# their paving, the League's steps): either side may look right.
ON_ROAD = {'rustboro': {0x212, 0x21A, 0x338, 0x339, 0x33A, 0x340, 0x341, 0x342, 0x348, 0x349, 0x34A},
           'ever_grande': {0x224, 0x225},
           # Slateport's market stalls, crates and awnings on its cobbles (a
           # town signature's market, cut whole from Slateport).
           'slateport': {0x205, 0x207, 0x20C, 0x20D, 0x20E, 0x20F, 0x21B, 0x225, 0x227, 0x22B, 0x22D, 0x22F, 0x232,
                         0x233, 0x235, 0x237, 0x23D, 0x23F, 0x26B, 0x26C, 0x285, 0x28E, 0x2F8, 0x2F9, 0x2FA, 0x32E,
                         0x32F, 0x336, 0x33B, 0x33E, 0x33F, 0x344}}
# Water and its banks (walkable at elevation 1), where a path ends.
WATERISH = set(range(0x170, 0x1A0)) | set(range(0x11B, 0x130)) - {0x124, 0x11C} | {0x145, 0x146, 0x0B0, 0x0B1, 0x0B2, 0x0B8, 0x0BA, 0x0A1}
# Grass, tall grass, flowers and tree tips over them: open grass.
GRASSY = {0x001, 0x002, 0x004, 0x00D, 0x00E, 0x00F, 0x015, 0x1CE, 0x1CF, 0x1C6, 0x1C7}

# Ledges: left end, middle, right end.
LEDGE_L, LEDGE_M, LEDGE_R = 0x0D5, 0x087, 0x0D6
# Ordinary trees (General).
TIPS = {0x1CE: 0, 0x1CF: 1, 0x1C6: 0, 0x1C7: 1}       # over grass / over tall grass
CROWNS = {0x1D4: 0, 0x1D5: 1, 0x1D6: 0, 0x1D7: 1}
TRUNKS = {0x1DC: 0, 0x1DD: 1, 0x1E4: 0, 0x1E5: 1, 0x1E6: 0, 0x1E7: 1,
          0x1EC: 0, 0x1ED: 1}  # (a trunk among the crowns of deep woods below, Route 104)

# Most a kind may average per town or route over many seeds (v0.13.0 is at
# about half of each; v0.12.0 had path_edge 11, path_thin 2.9, ground_clash
# 0.8). Kinds not listed must not occur at all.
BUDGET = {'path_edge': 0.3, 'path_thin': 0.15, 'ground_clash': 0.25, 'ground_scrap': 0.08, 'path_gap': 0.08,
          'path_scrap': 0.02, 'half_tree': 0.005, 'crown_cut': 0.005}


def path_family(m):
    """For this map's tileset: block id -> (family name, (row, col))."""
    fam = {}
    for name, ts, s in PATH_SETS:
        if ts is not None and ts != m['tileset']:
            continue
        for t, rc in s.items():
            fam.setdefault(t, (name, rc))
    return fam


class Map:
    def __init__(self, m):
        self.m = m
        self.w, self.h, self.ts = m['w'], m['h'], m['tileset']
        self.t = [b & 0x3FF for b in m['blocks']]
        self.solid = [(b >> 10) & 3 for b in m['blocks']]

    def at(self, x, y):
        if 0 <= x < self.w and 0 <= y < self.h:
            return self.t[y * self.w + x]
        return None

    def inside(self, x, y):
        return 0 <= x < self.w and 0 <= y < self.h


def check_paths(M, out):
    fam = path_family(M.m)
    w, h = M.w, M.h

    def name_at(x, y):
        t = M.at(x, y)
        if t in fam:
            return fam[t][0]
        return None

    def same_at(x, y, name):
        t = M.at(x, y)
        if name == 'sand' and M.ts in ISLAND_TREE_SETS and t in ISLAND_TREES:
            return True
        if t in fam and name in ASHES and fam[t][0] in ASHES:
            return True  # grey ash edged with grass or with pink dirt
        if t in fam and name in WAYS and fam[t][0] in WAYS:
            return True  # a path, a sand road and a town's road join edge to edge
        if name in WAYS and t in SAME_GROUND['sand'] - STEPS:
            return True  # and run on into the sand of a beach or desert
        return (t in fam and fam[t][0] == name) or t in SAME_GROUND.get(name, ())

    # Edges.
    for y in range(h):
        for x in range(w):
            t = M.t[y * w + x]
            if t not in fam:
                continue
            name, (r, c) = fam[t]
            want = {0: r != 0, 2: r != 2, 3: c != 0, 1: c != 2}  # side -> neighbour is the same path
            for d, (dx, dy) in enumerate(DIRS):
                if not M.inside(x + dx, y + dy):
                    continue
                if M.at(x + dx, y + dy) in ON_ROAD.get(M.ts, ()):
                    continue
                same = same_at(x + dx, y + dy, name)
                # Against trees, rock, water and buildings either way can
                # look right (sand runs under a desert's rocks, a path stops
                # at a door): only open ground is judged.
                if not same and M.solid[(y + dy) * w + x + dx]:
                    continue
                if not same and M.at(x + dx, y + dy) in WATERISH:
                    continue
                if same != want[d]:
                    out.append(('path_edge', x, y, '%s %03X %s' % (name, t, 'NESW'[d])))
                    break
    # One block wide or tall: open ground (grass, a tree's canopy) on both
    # sides of a piece of path, where no edge set can draw it.
    def open_other(x, y, name):
        t = M.at(x, y)
        return t is not None and not same_at(x, y, name) and not M.solid[y * w + x] and t not in WATERISH \
            and t not in ON_ROAD.get(M.ts, ())

    for y in range(h):
        for x in range(w):
            t = M.t[y * w + x]
            if t not in fam:
                continue
            name = fam[t][0]
            if (open_other(x, y - 1, name) and open_other(x, y + 1, name)) \
                    or (open_other(x - 1, y, name) and open_other(x + 1, y, name)):
                out.append(('path_thin', x, y, '%s %03X' % (name, t)))

    # Gaps: the trail broken across its whole width by a block or two of
    # grass between two pieces of the same path in a line.
    # A gap under a tree's canopy (a squeeze between trees) cannot hold a
    # path; one round an obstacle (a small tree, a rock) is bare ground.
    near_gate = set()
    for o in M.m['objs']:
        if o['kind'] in (7, 8, 9):
            near_gate |= {(o['x'] + dx, o['y'] + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)}

    def gap_at(x, y, dx, dy):
        if name_at(x, y) != 'path' or not M.inside(x + dx, y + dy) or name_at(x + dx, y + dy) == 'path':
            return 0
        for gap in (1, 2):
            gx, gy = x + dx * (gap + 1), y + dy * (gap + 1)
            if not M.inside(gx, gy):
                return 0
            if name_at(gx, gy) == 'path':
                cells = [(x + dx * k, y + dy * k) for k in range(1, gap + 1)]
                return gap if all(M.at(cx, cy) in GRASSY - set(TIPS) and (cx, cy) not in near_gate
                                  for cx, cy in cells) else 0
        return 0

    paths = [(i % w, i // w) for i, t in enumerate(M.t) if t in fam and fam[t][0] == 'path']
    for x, y in paths:
        for dx, dy in ((1, 0), (0, 1)):
            g = gap_at(x, y, dx, dy)
            # Across the whole width: the row (or column) beside is cut
            # the same way, and this is the first of the two.
            if g and gap_at(x + dy, y + dx, dx, dy) == g and gap_at(x - dy, y - dx, dx, dy) != g:
                out.append(('path_gap', x + dx, y + dy, '%d' % g))


def material(M, fam, x, y):
    """The ground a block shows, or None for anything that is not open ground."""
    t = M.at(x, y)
    if t is None or t in TIPS or t in STEPS or t in (0x002, 0x00E, 0x00F):
        return None  # a tree's canopy over the grass (part of the tree), steps
    if t in fam:
        name = fam[t][0]
        return 'ash' if name == 'pinkash' else name
    for name, same in SAME_GROUND.items():
        if t in same and (name == 'sand' or M.ts == 'fallarbor'):
            return 'ash' if name == 'pinkash' else name
    if t in GRASSY or (M.ts == 'fortree' and t in (0x208, 0x281, 0x282, 0x283)):
        return 'grass'
    if M.ts == 'fallarbor' and t in (0x279, 0x29F):
        return 'pink'
    return None


def check_scraps(M, out):
    """Small pieces of one ground inside another: a block or three of grass
    left in the sand, sand in the grass, and so on."""
    fam = path_family(M.m)
    w, h = M.w, M.h
    mat = [[material(M, fam, x, y) for x in range(w)] for y in range(h)]
    seen = set()
    for y in range(h):
        for x in range(w):
            k = mat[y][x]
            if k is None or (x, y) in seen:
                continue
            comp, stack, edge, border = [], [(x, y)], False, set()
            seen.add((x, y))
            while stack and len(comp) <= 8:
                cx, cy = stack.pop()
                comp.append((cx, cy))
                for dx, dy in DIRS:
                    nx, ny = cx + dx, cy + dy
                    if not M.inside(nx, ny):
                        edge = True
                    elif mat[ny][nx] == k:
                        if (nx, ny) not in seen:
                            seen.add((nx, ny))
                            stack.append((nx, ny))
                    else:
                        border.add((nx, ny))
            if stack:
                # Too big to be a scrap: mark the rest as seen.
                while stack:
                    cx, cy = stack.pop()
                    for dx, dy in DIRS:
                        nx, ny = cx + dx, cy + dy
                        if M.inside(nx, ny) and mat[ny][nx] == k and (nx, ny) not in seen:
                            seen.add((nx, ny))
                            stack.append((nx, ny))
                continue
            if edge:
                continue
            other = sum(1 for bx, by in border if mat[by][bx] is not None)
            limit = 4 if k in ('path', 'road') else 3
            if len(comp) <= limit and other * 4 >= len(border) * 3:
                xs, ys = [p[0] for p in comp], [p[1] for p in comp]
                kind = 'path_scrap' if k in ('path', 'road') else 'ground_scrap'
                out.append((kind, min(xs), min(ys), '%s %d blocks in %s' % (
                    k, len(comp), Counter(mat[by][bx] for bx, by in border).most_common(1)[0][0])))


def check_ledges(M, out):
    w, h = M.w, M.h
    L = (LEDGE_L, LEDGE_M, LEDGE_R)
    for y in range(h):
        for x in range(w):
            t = M.t[y * w + x]
            if t not in L:
                continue
            le, ri = M.at(x - 1, y), M.at(x + 1, y)
            if t == LEDGE_L:
                bad = le in (LEDGE_M, LEDGE_L) or (ri not in (LEDGE_M, LEDGE_R) and ri is not None)
            elif t == LEDGE_R:
                bad = ri in (LEDGE_M, LEDGE_R) or (le not in (LEDGE_M, LEDGE_L) and le is not None)
            else:
                bad = (le is not None and le not in (LEDGE_M, LEDGE_L)) or (ri is not None and ri not in (LEDGE_M, LEDGE_R))
            if bad:
                out.append(('ledge_end', x, y, '%03X' % t))
            # A ledge of two blocks or fewer (both ends and nothing between)
            # reads as a stray stone, not a ledge.
            if t == LEDGE_L:
                n = 1
                while M.at(x + n, y) in (LEDGE_M, LEDGE_R):
                    n += 1
                    if M.at(x + n - 1, y) == LEDGE_R:
                        break
                if n <= 2:
                    out.append(('ledge_short', x, y, '%d blocks' % n))


F_TL, F_H, F_TR, F_L, F_R, F_BL, F_BR = 0x138, 0x149, 0x13A, 0x140, 0x142, 0x148, 0x14A
FENCE_RULES = {  # piece -> {side: pieces allowed there}
    F_H: {1: {F_H, F_TR, F_BR}, 3: {F_H, F_TL, F_BL}},
    F_TL: {1: {F_H, F_TR}, 2: {F_L, F_BL}},
    F_TR: {3: {F_H, F_TL}, 2: {F_R, F_BR}},
    F_L: {0: {F_L, F_TL}, 2: {F_L, F_BL}},
    F_R: {0: {F_R, F_TR}, 2: {F_R, F_BR}},
    F_BL: {1: {F_H, F_BR}},
    F_BR: {3: {F_H, F_BL}},
}


def check_fences(M, out):
    for y in range(M.h):
        for x in range(M.w):
            rule = FENCE_RULES.get(M.t[y * M.w + x])
            if not rule:
                continue
            for d, ok in rule.items():
                dx, dy = DIRS[d]
                nb = M.at(x + dx, y + dy)
                if nb is not None and nb not in ok:
                    out.append(('fence', x, y, '%03X %s %03X' % (M.t[y * M.w + x], 'NESW'[d], nb)))
                    break


def check_trees(M, out):
    w, h = M.w, M.h
    for y in range(h):
        for x in range(w):
            t = M.t[y * w + x]
            if t in TIPS:
                below = M.at(x, y + 1)
                if below is not None and below not in CROWNS:
                    out.append(('tip_floating', x, y, '%03X over %03X' % (t, below)))
            if t in CROWNS:
                below = M.at(x, y + 1)
                if below is not None and below not in TRUNKS and below not in CROWNS:
                    out.append(('crown_cut', x, y, '%03X over %03X' % (t, below)))
            for group in (TIPS, CROWNS, TRUNKS):
                if t in group:
                    side = group[t]
                    nx = x + 1 if side == 0 else x - 1
                    nb = M.at(nx, y)
                    if nb is not None and (nb not in group or group[nb] == side):
                        out.append(('half_tree', x, y, '%03X beside %03X' % (t, nb)))


def check_doors(M, out):
    """The patch in front of a door (V_MAT) is Petalburg's metatile 0x201;
    in any other tileset that number is something else entirely."""
    if M.ts == 'petalburg':
        return
    for y in range(M.h):
        for x in range(M.w):
            if M.m['blocks'][y * M.w + x] == 0x3201:
                out.append(('door_mat', x, y, 'tileset %s' % M.ts))


# The ground under a metatile, from the tiles of its bottom layer: grass
# (General's), sand (General's, as Dewford lays it) or Fallarbor's ash dirt.
GROUND_TILES = {'grass': {0x002, 0x003}, 'sand': {0x108, 0x118}}
ASH_DIRT_TILES = {0x205, 0x215}
# Rustboro's paving (0x2BB), the ground its buildings were cut standing on.
PAVING_TILES = {0x2E0, 0x2E1, 0x2E2}
PLAIN = {0x001: 'grass', 0x124: 'sand'}
_tilesets = {}


def under(ts, t):
    """What ground shows under metatile t of tileset ts (None: none or mixed)."""
    key_ = (ts, t)
    if key_ in _tilesets:
        return _tilesets[key_]
    from render_map import Tilesets
    if ts not in _tilesets:
        _tilesets[ts] = Tilesets('general', ts)
    T = _tilesets[ts]
    mt = T.metatiles[t] if t < len(T.metatiles) else None
    got = set()
    if mt:
        for ref in mt[:4]:
            tile = ref & 0x3FF
            for g, tiles in GROUND_TILES.items():
                if tile in tiles:
                    got.add(g)
            if ts == 'fallarbor' and tile in ASH_DIRT_TILES:
                got.add('ash')
            if ts == 'rustboro' and tile in PAVING_TILES:
                got.add('paving')
    _tilesets[key_] = got.pop() if len(got) == 1 else None
    return _tilesets[key_]


def check_ground_clash(M, out):
    """A building, rock or fence cut from a map with other ground under it:
    sand corners round a house on the grass, a square of sand round a rock."""
    if M.m['kind'] != 'town':
        return
    fam = path_family(M.m)
    plain = dict(PLAIN)
    if M.ts == 'fallarbor':
        plain[0x279] = 'ash'
    if M.ts == 'rustboro':
        plain[0x2BB] = 'paving'
    for y in range(M.h):
        for x in range(M.w):
            t = M.t[y * M.w + x]
            if t in plain or t in fam or t in TIPS or t in GRASSY:
                continue
            g = under(M.ts, t)
            if g is None:
                continue
            for dx, dy in DIRS:
                o = plain.get(M.at(x + dx, y + dy))
                if o is not None and o != g:
                    out.append(('ground_clash', x, y, '%03X on %s by %s' % (t, g, o)))
                    break


CHECKS = (check_paths, check_scraps, check_ledges, check_fences, check_trees, check_ground_clash, check_doors)


def check_region(maps):
    found = []
    for m in maps:
        if m['kind'] == 'cave':
            continue
        M = Map(m)
        out = []
        for c in CHECKS:
            c(M, out)
        for kind, x, y, detail in out:
            found.append({'kind': kind, 'map': m['i'], 'place': m['kind'], 'x': x, 'y': y, 'detail': detail,
                          'type': ROUTE_KINDS[m['type']] if m['kind'] == 'route' else THEMES[m['theme']]})
    return found


def over_budget(total, nmaps):
    """Kinds over their budget, as text, for total counts over nmaps towns and routes."""
    return ['%s %.3f a map (budget %s)' % (k, c / nmaps, BUDGET.get(k, 0))
            for k, c in sorted(total.items()) if c / nmaps > BUDGET.get(k, 0)]


def one(seed):
    maps = P.load_region(seed)
    n = Counter(m['kind'] for m in maps if m['kind'] != 'cave')
    return seed, check_region(maps), n


def crops(found_by_seed, out_dir, per):
    from PIL import Image, ImageDraw
    from render_map import Tilesets, render
    os.makedirs(out_dir, exist_ok=True)
    picked = defaultdict(list)
    for seed, found in found_by_seed:
        for f in found:
            k = f['kind']
            if len(picked[k]) < per and all(g[0] != seed or g[1]['map'] != f['map'] for g in picked[k][-2:]):
                picked[k].append((seed, f))
    sets = {}
    cache = {}
    for k, lst in picked.items():
        for n, (seed, f) in enumerate(lst):
            if seed not in cache:
                cache.clear()
                cache[seed] = {m['i']: m for m in P.load_region(seed)}
            m = cache[seed][f['map']]
            ts = sets.setdefault(m['tileset'], Tilesets('general', m['tileset']))
            x0, y0 = max(0, f['x'] - 6), max(0, f['y'] - 4)
            x1, y1 = min(m['w'], f['x'] + 7), min(m['h'], f['y'] + 5)
            blocks = [m['blocks'][y * m['w'] + x] for y in range(y0, y1) for x in range(x0, x1)]
            img = render(blocks, x1 - x0, ts).resize(((x1 - x0) * 48, (y1 - y0) * 48), Image.NEAREST)
            d = ImageDraw.Draw(img)
            bx, by = (f['x'] - x0) * 48, (f['y'] - y0) * 48
            d.rectangle([bx, by, bx + 47, by + 47], outline=(255, 0, 0), width=2)
            img.save(os.path.join(out_dir, '%s_%02d_s%d_m%d_%d_%d.png' % (k, n, seed, f['map'], f['x'], f['y'])))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--seeds', type=int, default=200)
    ap.add_argument('--first', type=int, default=1)
    ap.add_argument('--crops')
    ap.add_argument('--per', type=int, default=12)
    ap.add_argument('--json')
    ap.add_argument('--jobs', type=int, default=8)
    a = ap.parse_args()
    results = []
    with ProcessPoolExecutor(a.jobs) as ex:
        for r in ex.map(one, range(a.first, a.first + a.seeds)):
            results.append(r)
    total = Counter()
    by_type = defaultdict(Counter)
    places = Counter()
    for seed, found, n in results:
        places.update(n)
        for f in found:
            total[f['kind']] += 1
            by_type[f['kind']][f['place'] + ':' + f['type']] += 1
    nmaps = sum(places.values())
    print('%d seeds, %d towns, %d routes' % (a.seeds, places['town'], places['route']))
    for k, c in total.most_common():
        print('%-14s %7d  %6.2f per map   %s' % (k, c, c / nmaps, ', '.join('%s %d' % kv for kv in by_type[k].most_common(5))))
    if a.json:
        json.dump({'seeds': a.seeds, 'first': a.first, 'maps': nmaps, 'total': dict(total),
                   'by_type': {k: dict(v) for k, v in by_type.items()},
                   'per_seed': {str(s): Counter(f['kind'] for f in found) for s, found, n in results}},
                  open(a.json, 'w'), indent=1)
    if a.crops:
        crops([(s, f) for s, f, n in results], a.crops, a.per)
    over = over_budget(total, nmaps)
    if over:
        print('over budget: ' + '; '.join(over))
    sys.exit(1 if over else 0)


if __name__ == '__main__':
    main()
