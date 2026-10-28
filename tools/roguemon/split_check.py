#!/usr/bin/env python3
"""Find trees, banks and cliffs that lost some of their pieces.

Trees are drawn from several metatiles that only ever appear together. This
learns from the vanilla Hoenn maps which neighbours each tree piece is always
seen with, and flags generated blocks whose neighbour was never seen beside
them: the top of a tree with no trunk under it, a left half without its
right half, tips floating over the wrong thing, and so on.

Buildings and decorations are cut whole from vanilla maps, so only their
outer edges meet new neighbours, and those edges sit on open ground. Only
tree pieces are judged. Neighbours across map connections are checked where
the player can see them, drawn with the tileset of the map the player stands
on, as the game does.

Long grass is judged too: each column must end in its lower edge, and the
edge must have long grass above it.

Usage:
  split_check.py [--seeds 20] [--first 1] [--verbose]
"""
import argparse
import json
import os
import re
import sys
from collections import Counter, defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rogue_emu as P  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
DIRS = ((0, -1), (1, 0), (0, 1), (-1, 0))  # N E S W
MIN_SEEN = 4       # times a metatile must appear in vanilla to be judged
MAX_PARTNERS = 4   # a piece has at most this many different neighbours on a side
# The tree pieces, by tileset.
TREES = {
    'general': {0x1C6, 0x1C7, 0x1CE, 0x1CF,           # tips over the ground
                0x1D4, 0x1D5, 0x1D6, 0x1D7,           # crowns
                0x1DC, 0x1DD, 0x1E4, 0x1E5, 0x1E6, 0x1E7,  # trunks
                0x0C6, 0x0C7, 0x00E, 0x00F, 0x016, 0x017,  # deep woods
                0x1EC, 0x1ED, 0x1F4, 0x1F5},  # where deep woods meet round trees (Route 104)
    'dewford': {0x239, 0x243, 0x23A},                 # island trees
    # Ash trees: on a town's pink dirt and on a route's grey ash. 0x206 0x207
    # 0x20E 0x20F (0x204 0x205 0x20C 0x20D) are the quarters of a lone tree.
    'fallarbor': {0x206, 0x207, 0x20E, 0x20F, 0x26B, 0x26C, 0x273, 0x274,
                  0x204, 0x205, 0x20C, 0x20D, 0x26D, 0x26E, 0x275, 0x276},
}
# Long grass and its lower edge, which only Fortree's tileset has: every
# column of long grass ends in the edge, and the edge has long grass above.
LONG_GRASS = 0x015
LONG_EDGES = {0x208, 0x281, 0x282, 0x283}
# Edges of water and rock, judged the same way: a bank that ends nowhere, a
# cliff with no face, a ledge cut short.
EDGES = {
    'general': {0x0B0, 0x0B1, 0x0B2, 0x0B8, 0x0BA,                       # pond banks
                0x188, 0x189, 0x18A, 0x178, 0x179, 0x190, 0x192, 0x198, 0x19A,  # sea banks on grass
                0x11B, 0x11D, 0x123, 0x125, 0x12B, 0x12C, 0x12D, 0x145, 0x146,  # sea banks on sand
                0x11C, 0x13D, 0x13E,                                      # sand shore
                0x0D5, 0x087, 0x0D6,                                      # ledges
                0x068, 0x069, 0x06A, 0x06B, 0x06C, 0x06D, 0x070, 0x072, 0x073, 0x074, 0x075,
                0x078, 0x079, 0x07A, 0x07B, 0x07C, 0x07D, 0x080, 0x081, 0x082, 0x088, 0x089,
                0x08A, 0x090, 0x091, 0x092, 0x0A9, 0x0BE, 0x0BF, 0x09F, 0x0A7},  # rock and cliffs
}
# Walkable pieces. The rest are solid.
WALKABLE_PIECES = {
    (0x1CE, 'general'), (0x1CF, 'general'), (0x1C6, 'general'), (0x1C7, 'general'),
    (0x239, 'dewford'), (0x206, 'fallarbor'), (0x207, 'fallarbor'),
    (0x204, 'fallarbor'), (0x205, 'fallarbor'),
}
# Pairs never seen in vanilla that were checked by eye and look right.
ACCEPTED = {
    # A cave mouth's top beside the mesa's rock.
    ((0x09F, 'general'), 3, (0x071, 'general')), ((0x071, 'general'), 1, (0x09F, 'general')),
    # The corners where sand meets the sea above, over a tree's tips, and
    # the sand shore beside the end of a row of trees.
    ((0x13D, 'general'), 2, (0x1CF, 'general')), ((0x13E, 'general'), 2, (0x1CE, 'general')),
    ((0x1CF, 'general'), 0, (0x13D, 'general')), ((0x1CE, 'general'), 0, (0x13E, 'general')),
    ((0x11C, 'general'), 1, (0x1D6, 'general')), ((0x11C, 'general'), 3, (0x1D7, 'general')),
    ((0x1D6, 'general'), 3, (0x11C, 'general')), ((0x1D7, 'general'), 1, (0x11C, 'general')),
    # Trees above a sea inlet: the bank is drawn in the water.
    ((0x146, 'general'), 0, (0x1E4, 'general')), ((0x146, 'general'), 0, (0x1E6, 'general')),
    ((0x146, 'general'), 0, (0x1CB, 'general')), ((0x146, 'general'), 0, (0x217, 'dewford')),
    ((0x146, 'general'), 0, (0x23E, 'dewford')),
}
# Tilesets whose tree sides are not judged. Ash trees are drawn whole now
# (lone trees and Route 113's lattice), so all are judged.
SIDES_FREE = set()
# Sea banks whose far side may be anything: the bank of an inlet is drawn in
# the water, whatever stands above it, and the corners where sand meets the
# sea above sit over whatever is below.
FREE_SIDE = {((0x146, 'general'), 0), ((0x13D, 'general'), 2), ((0x13E, 'general'), 2),
             ((0x11C, 'general'), 1), ((0x11C, 'general'), 3)}
# Signs stand beside and above tree tips like any other ground.
SIGN = (0x003, 'general')
EDGES_ON = os.environ.get('SPLIT_EDGES', '1') == '1'


def is_tree(k):
    return k[0] in TREES.get(k[1], ())


def is_edge(k):
    return k[0] in EDGES.get(k[1], ())


# Fallarbor's ash trees on pink dirt and on grey ash, piece for piece. Where
# a route's pink dirt by an ash town meets its grey ash, the two forests
# join as on Route 113 west of Fallarbor; a pink piece passes where the
# grey one would (and the other way round), since in a forest only a corner
# of ground shows in either.
ASH_PINK = (0x206, 0x207, 0x20E, 0x20F, 0x26B, 0x274, 0x26C, 0x273)
ASH_GREY = (0x204, 0x205, 0x20C, 0x20D, 0x26D, 0x276, 0x26E, 0x275)
ASH_OTHER = {(p, 'fallarbor'): (g, 'fallarbor') for p, g in zip(ASH_PINK, ASH_GREY)}
ASH_OTHER.update({v: k for k, v in list(ASH_OTHER.items())})


def ash_mix(nk, allowed):
    return nk in ASH_OTHER and ASH_OTHER[nk] in allowed


# Trees of two styles stacked with no join, which no Hoenn map draws: a
# column of deep woods ending on a round crown (Route 104 puts the woods'
# trunks on it, 0x1F4 0x1F5), the woods' crowns over an ash tree, and an ash
# tree standing on a round crown (its trunk cut off, the crown with no tips).
# Judged inside a map; (piece above, piece below).
WOODS_CROWNS = {(0x0C6, 'general'), (0x0C7, 'general')}
ROUND_CROWNS = {(t, 'general') for t in (0x1D4, 0x1D5, 0x1D6, 0x1D7)}
ASH_TREES = {(t, 'fallarbor') for t in ASH_PINK + ASH_GREY}
ASH_LOWER = {(t, 'fallarbor') for t in (0x20C, 0x20D, 0x20E, 0x20F, 0x26B, 0x26C, 0x26D, 0x26E,
                                         0x273, 0x274, 0x275, 0x276)}


def style_break(k, nk):
    """Is piece k over piece nk two tree styles cut into each other?"""
    return ((k in WOODS_CROWNS and (nk in ROUND_CROWNS or nk in ASH_TREES))
            or (k in ASH_LOWER and nk in ROUND_CROWNS))


def tree_meets_tree(k, nk):
    """Ordinary trees within view of a connection meet a town's own trees
    (and a sand town's bushes) side by side, as the two forests touch."""
    return is_tree(k) and is_tree(nk) and k[1] != nk[1]

# Neighbours that pass for any piece: map edges in vanilla run into the
# border, so a generated map edge is judged only through its connection.
_rules = None


def ts_name(sym):
    """gTileset_EverGrande -> ever_grande"""
    s = sym.replace('gTileset_', '')
    return re.sub(r'(?<!^)(?=[A-Z])', '_', s).lower()


def key(b, ts):
    t = b & 0x3FF
    return (t, 'general' if t < 0x200 else ts)


def learn():
    """For each metatile and side, the neighbours vanilla puts there."""
    global _rules
    if _rules is not None:
        return _rules
    layouts = json.load(open(os.path.join(ROOT, 'data', 'layouts', 'layouts.json')))['layouts']
    seen = Counter()
    solid = Counter()
    sides = defaultdict(Counter)
    for lay in layouts:
        if lay.get('primary_tileset') != 'gTileset_General' or 'blockdata_filepath' not in lay:
            continue
        if 'Rogue' in lay['name']:
            continue
        path = os.path.join(ROOT, lay['blockdata_filepath'])
        if not os.path.exists(path):
            continue
        ts = ts_name(lay['secondary_tileset'])
        w, h = lay['width'], lay['height']
        blocks = P.load_bin(path)
        for y in range(h):
            for x in range(w):
                k = key(blocks[y * w + x], ts)
                seen[k] += 1
                if (blocks[y * w + x] >> 10) & 3:
                    solid[k] += 1
                for d, (dx, dy) in enumerate(DIRS):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < w and 0 <= ny < h:
                        sides[(k, d)][key(blocks[ny * w + nx], ts)] += 1
    def ground(k):
        return solid[k] * 2 <= seen[k] and k not in WALKABLE_PIECES

    rules = {}
    for (k, d), partners in sides.items():
        piece = (not ground(k) and is_tree(k)) or (is_edge(k) and EDGES_ON)
        if piece and seen[k] >= MIN_SEEN and len(partners) <= MAX_PARTNERS:
            # An object whose side meets open ground in vanilla may meet
            # any open ground here: towns lay other ground than vanilla.
            rules[(k, d)] = (set(partners), any(ground(p) for p in partners))
    _rules = (rules, seen)
    return _rules


def neighbour(maps_by_i, m, x, y):
    """The block at x, y of map m, looking across its connections."""
    if 0 <= x < m['w'] and 0 <= y < m['h']:
        return m['blocks'][y * m['w'] + x]
    for l in m['links']:
        b = maps_by_i[l['target']]
        d, off = l['dir'], l['offset']
        if d == 0 and y < 0:
            bx, by = x - off, y + b['h']
        elif d == 2 and y >= m['h']:
            bx, by = x - off, y - m['h']
        elif d == 1 and x >= m['w']:
            bx, by = x - m['w'], y - off
        elif d == 3 and x < 0:
            bx, by = x + b['w'], y - off
        else:
            continue
        if 0 <= bx < b['w'] and 0 <= by < b['h']:
            return b['blocks'][by * b['w'] + bx]
    return None


def in_view(m, x, y, side):
    """Can a player standing across the connection on this side see x, y?"""
    pos = m['exits'].get(side)
    if pos is None:
        return False
    depth = {0: y, 2: m['h'] - 1 - y, 3: x, 1: m['w'] - 1 - x}[side]
    lateral = abs((x if side in (0, 2) else y) - pos)
    # The screen shows 7 blocks each side of the player and about 5 above
    # and below; the player can stand up to 3 blocks off the exit line.
    return depth <= (5 if side in (0, 2) else 7) and lateral <= (10 if side in (0, 2) else 8)


def long_grass(by_i, m, x, y, k):
    """Long grass with no lower edge under it, or an edge with no long
    grass over it."""
    ts = m['tileset']
    if k[0] == LONG_GRASS:
        d, ok = 2, ({LONG_GRASS} | LONG_EDGES) if ts == 'fortree' else set()
    else:
        d, ok = 0, {LONG_GRASS}
    dx, dy = DIRS[d]
    nb = neighbour(by_i, m, x + dx, y + dy)
    if nb is None or (nb & 0x3FF) in ok:
        return []
    seam = not (0 <= x + dx < m['w'] and 0 <= y + dy < m['h'])
    return [{'map': m['i'], 'name': m['name'], 'kind': m['kind'], 'x': x, 'y': y, 'tile': '%03X' % k[0],
             'side': 'NESW'[d], 'other': '%03X' % (nb & 0x3FF), 'seam': seam}]


def check_region(maps):
    rules, _ = learn()
    by_i = {m['i']: m for m in maps}
    out = []
    for m in maps:
        if m['kind'] == 'cave':
            continue
        ts = m['tileset']
        w, h = m['w'], m['h']
        for y in range(h):
            for x in range(w):
                k = key(m['blocks'][y * w + x], ts)
                if k[0] == LONG_GRASS or (k[1] == 'fortree' and k[0] in LONG_EDGES):
                    out.extend(long_grass(by_i, m, x, y, k))
                    continue
                if y + 1 < h and style_break(k, key(m['blocks'][(y + 1) * w + x], ts)):
                    nk = key(m['blocks'][(y + 1) * w + x], ts)
                    out.append({'map': m['i'], 'name': m['name'], 'kind': m['kind'], 'x': x, 'y': y,
                                'tile': '%03X' % k[0], 'side': 'S', 'other': '%03X' % nk[0], 'seam': False})
                for d, (dx, dy) in enumerate(DIRS):
                    if (d in (1, 3) and k[1] in SIDES_FREE) or (k, d) in FREE_SIDE:
                        continue
                    rule = rules.get((k, d))
                    if rule is None:
                        continue
                    allowed, ground_ok = rule
                    nb = neighbour(by_i, m, x + dx, y + dy)
                    if nb is None:
                        continue
                    seam = not (0 <= x + dx < w and 0 <= y + dy < h)
                    if seam and not in_view(m, x, y, d):
                        continue
                    nk = key(nb, ts)
                    if ground_ok and not (nb >> 10) & 3 and nk not in WALKABLE_PIECES:
                        continue
                    # A bank or cliff beside a building, fence or other
                    # solid scenery: those stand where they like.
                    if is_edge(k) and not is_tree(nk) and not is_edge(nk) and (nb >> 10) & 3:
                        continue
                    if nk == SIGN and (k in WALKABLE_PIECES or d == 0):
                        continue
                    if nk not in allowed and (k, d, nk) not in ACCEPTED and not tree_meets_tree(k, nk) \
                            and not ash_mix(nk, allowed):
                        out.append({'map': m['i'], 'name': m['name'], 'kind': m['kind'], 'x': x, 'y': y,
                                    'tile': '%03X' % k[0], 'side': 'NESW'[d], 'other': '%03X' % nk[0],
                                    'seam': seam})
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--seeds', type=int, default=20)
    ap.add_argument('--first', type=int, default=1)
    ap.add_argument('--verbose', action='store_true')
    args = ap.parse_args()
    total = Counter()
    bad = 0
    for seed in range(args.first, args.first + args.seeds):
        found = check_region(P.load_region(seed))
        if any(not f['seam'] for f in found):
            bad += 1
        for f in found:
            total[(f['tile'], f['side'], f['other'], f['kind'], f['seam'])] += 1
            if args.verbose:
                print('seed %d map %d (%s) %d,%d: %s with %s to the %s%s'
                      % (seed, f['map'], f['name'], f['x'], f['y'], f['tile'], f['other'], f['side'],
                         ' across a seam' if f['seam'] else ''))
    for (t, side, other, kind, seam), n in total.most_common(60):
        print('%5d  %s %s-of %s  %s%s' % (n, other, side, t, kind, ' seam' if seam else ''))
    # Across a seam the map next door was made on its own; a mismatch there
    # is a warning.
    print('%d of %d seeds without split trees' % (args.seeds - bad, args.seeds))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
