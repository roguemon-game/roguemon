#!/usr/bin/env python3
"""Find what looks cut off where two generated maps meet.

Across a connection the game draws the neighbour with the current map's
tileset, so the two edge rows sit side by side like any other blocks. Most
pairs are fine: grass meets trees, a path runs on, a tree line meets rock.
Three kinds look cut off:

  water meets land   sea, a river or a pond against land with no bank or
                     shore between them (the sea stops in a straight line)
  half tree          a piece of a 2x2 tree (or an ash or island tree) whose
                     other half is not across the seam, such as the left
                     half of a crown beside grass
  tree cut flat      tree crowns under rock or another solid block with no
                     tips above them (ash crowns under a route's cliff)
  rock cut           a cliff, rock edge or ledge piece beside something it
                     never stands beside in Hoenn

Each pair is judged against the Hoenn maps: a piece is flagged only when no
vanilla map ever puts that kind of neighbour on that side of it. Pieces of
two different tree kinds side by side (ordinary trees meeting a town's ash or
island trees) are whole trees, as are ash trees on both sides. Each kind of
cut is reported once per seam, at its first block, with how many blocks it
covers.

Usage:
  seam_check.py [--seeds 10] [--first 1] [--verbose]
"""
import argparse
import json
import os
import sys
from collections import Counter, defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rogue_emu as P  # noqa: E402
import split_check as S  # noqa: E402

DIRS = ((0, -1), (1, 0), (0, 1), (-1, 0))  # N E S W
ROCK = {0x068, 0x069, 0x06A, 0x06B, 0x06C, 0x06D, 0x070, 0x071, 0x072, 0x073, 0x074, 0x075,
        0x078, 0x079, 0x07A, 0x07B, 0x07C, 0x07D, 0x080, 0x081, 0x082, 0x088, 0x089,
        0x08A, 0x090, 0x091, 0x092, 0x0A9, 0x0BE, 0x0BF, 0x09F, 0x0A7, 0x0D5, 0x087, 0x0D6}
ASH = {0x206, 0x207, 0x20E, 0x20F, 0x26B, 0x26C, 0x273, 0x274}
MIN_SEEN = 4

_learned = None


def is_water(b):
    """Sea, river and pond blocks, banks included: walkable at elevation 1."""
    return (b & 0x0C00) == 0 and (b >> 12) == 1


def is_ground(b):
    return (b & 0x0C00) == 0 and (b >> 12) != 1


def kind(k):
    if S.is_tree(k):
        return 'tree'
    if k[1] == 'general' and k[0] in ROCK:
        return 'rock'
    return None


def learn():
    """For each metatile and side: the neighbours vanilla puts there, and how
    often that neighbour is water, ground, or neither."""
    global _learned
    if _learned is not None:
        return _learned
    layouts = json.load(open(os.path.join(S.ROOT, 'data', 'layouts', 'layouts.json')))['layouts']
    seen = Counter()
    sides = defaultdict(Counter)
    water = defaultdict(lambda: [0, 0, 0])  # (k, d) -> [water, ground, solid] neighbours
    for lay in layouts:
        if lay.get('primary_tileset') != 'gTileset_General' or 'blockdata_filepath' not in lay:
            continue
        if 'Rogue' in lay['name']:
            continue
        path = os.path.join(S.ROOT, lay['blockdata_filepath'])
        if not os.path.exists(path):
            continue
        ts = S.ts_name(lay['secondary_tileset'])
        w, h = lay['width'], lay['height']
        blocks = P.load_bin(path)
        for y in range(h):
            for x in range(w):
                b = blocks[y * w + x]
                k = S.key(b, ts)
                seen[k] += 1
                for d, (dx, dy) in enumerate(DIRS):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < w and 0 <= ny < h:
                        nb = blocks[ny * w + nx]
                        sides[(k, d)][S.key(nb, ts)] += 1
                        water[(k, d)][0 if is_water(nb) else (1 if is_ground(nb) else 2)] += 1
    _learned = (seen, sides, water)
    return _learned


def judge(b, nb, ts, d, why=None):
    """Why block b (drawn with tileset ts) looks cut off with nb on side d, or None."""
    seen, sides, water = learn()
    k, nk = S.key(b, ts), S.key(nb, ts)
    if seen[k] < MIN_SEEN:
        return None
    counts = water[(k, d)]
    pair_seen = nk in sides[(k, d)]
    # Water against land, judged from both sides: vanilla must have put
    # these two blocks side by side this way (a bank in the water facing the
    # land, or a shore on the land facing the water).
    if is_water(b) != is_water(nb):
        return None if pair_seen else 'water meets land'
    kd = kind(k)
    if kd is None or pair_seen:
        return None
    nkd = kind(nk)
    # Trees meeting trees across a seam read as one forest, even a block out
    # of step, and rock masses run on into rock or a tree line.
    if nkd == kd or (kd == 'rock' and nkd == 'tree'):
        return None
    # A piece that stands beside open ground in Hoenn may stand beside any
    # open ground: towns lay sand and ash where vanilla has grass.
    if is_ground(nb):
        if counts[1]:
            return None
    # Trees stand against rock, buildings and water on their sides and
    # below; only a crown with rock over it is cut flat.
    elif kd == 'tree' and d != 0:
        return None
    return ('tree cut flat' if d == 0 and not is_ground(nb) else 'half tree') if kd == 'tree' else 'rock cut'


def seam_pairs(a, b, l):
    """Edge block pairs (block of a, block of b) along the connection l of a."""
    d, off = l['dir'], l['offset']
    pairs = []
    if d in (0, 2):
        ya = 0 if d == 0 else a['h'] - 1
        yb = b['h'] - 1 if d == 0 else 0
        for x in range(a['w']):
            xb = x - off
            if 0 <= xb < b['w']:
                pairs.append(((x, ya), (xb, yb)))
    else:
        xa = a['w'] - 1 if d == 1 else 0
        xb = 0 if d == 1 else b['w'] - 1
        for y in range(a['h']):
            yb = y - off
            if 0 <= yb < b['h']:
                pairs.append(((xa, y), (xb, yb)))
    return pairs


def check_region(maps):
    """Warnings for every seam of a region: one per run of cut pairs."""
    by_i = {m['i']: m for m in maps}
    out = []
    for a in maps:
        if a['kind'] == 'cave':
            continue
        for l in a['links']:
            b = by_i[l['target']]
            if b['i'] < a['i']:
                continue
            d = l['dir']
            found = {}
            for pa, pb in seam_pairs(a, b, l):
                ba = a['blocks'][pa[1] * a['w'] + pa[0]]
                bb = b['blocks'][pb[1] * b['w'] + pb[0]]
                # Each map sees the other drawn with its own tileset.
                why = judge(ba, bb, a['tileset'], d) or judge(bb, ba, b['tileset'], (d + 2) % 4)
                if not why:
                    continue
                if why in found:
                    found[why]['len'] += 1
                    continue
                found[why] = {'check': 'seam', 'reason': why, 'map': a['i'], 'x': pa[0], 'y': pa[1],
                              'other': b['i'], 'len': 1, 'name': a['name'], 'side': 'NESW'[d]}
                out.append(found[why])
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--seeds', type=int, default=10)
    ap.add_argument('--first', type=int, default=1)
    ap.add_argument('--verbose', action='store_true')
    args = ap.parse_args()
    total = Counter()
    for seed in range(args.first, args.first + args.seeds):
        for w in check_region(P.load_region(seed)):
            total[w['reason']] += 1
            if args.verbose:
                print('seed %d map %d (%s) %s edge from %d,%d, %d blocks, toward map %d: %s'
                      % (seed, w['map'], w['name'], w['side'], w['x'], w['y'], w['len'], w['other'], w['reason']))
    print('%d seam warnings in %d seeds: %s' % (sum(total.values()), args.seeds, dict(total)))


if __name__ == '__main__':
    main()
