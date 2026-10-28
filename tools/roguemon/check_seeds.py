#!/usr/bin/env python3
"""Generate many regions on the host and check that each one is completable.

For every place: all exits, building doors, cave mouths, trainers, items and
signs must be reachable on foot from where the player comes in. Ledges and
water count as walls, so a pass means nothing depends on a one-way jump.

A side route's obstacle (small trees, cracked rocks, a boulder or water)
must keep everything past it out of reach until it is cleared, and once it
is cleared everything must be reachable.

The plan must hang together: links match on both sides, every place can be
reached from the first town, eight gyms lie on the main road in order, and
every gate is on the way out of the first town, a gym town or the villains'
hideout town.

The region must keep strict badge gating: walking from the first town, the
badges needed to reach each place (gates, obstacles and Surf for the sea)
are exactly the badges the plan gives it, so gym k + 1's town needs k badges
and no shortcut or fork opens a way round a gym. Levels never drop sharply
on the way a place is first reached. The limits hold (42 places, 14 towns,
four exits, one a side), the region map fits one screen (30 x 15 tiles),
later gyms have one to three places between them, the sea comes after the
fifth badge, every gym has a catch area before it, no three main routes in a
row are of one kind and route 1 is never desert, gym towns never share a
theme, and a hamlet never copies the theme of the town it hangs off.

The region map's tiles join as on Hoenn's map (gen_region_map.check_map):
at least 92% of the seams between land, coast and sea tiles and between
road tiles are smooth (the same pixels as a seam Hoenn has, or at most one
pixel across it jumping from land to sea or by more than one shade), and
no more than 6 tiles are Hoenn's plain green. The end of the run prints the
rates.

Every region gives reasons to wander and come back: a shortcut back or a
fork, obstacles of two kinds or more, and a shortcut or a side way that
opens only with a badge won after the player first passes it.

No tree, bank or cliff may lose a piece inside a map (see split_check.py).
Tiling is held to the shapes of a real Gen 3 map (tile_polish.py): no
ledge cut short, no fence left open, no door patch from the wrong tileset,
and one-block-wide paths, broken trails, scraps of ground and stamps on the
wrong ground within their budgets over the run (printed as "tiling").
Lavaridge's red rock only has neighbours that Hoenn's maps drawn with
Lavaridge's tileset have: every pair of blocks side by side or one over the
other with red rock in it (the map's border included) occurs in one of them.
In a beach town no ordinary tree stands on bare sand, and ordinary and
island trees never touch.
Dewford's island trees are whole: every crown has a tip or another crown
above it and ends in a trunk (or runs on past the map's edge), every tip
has a tree under it, and each tree stands beside another, in a grove.

Route trainers come in a mix, as on Emerald's routes (TRAINER_MIX): some
nobody can walk past, more in view of the shortest walk, the rest off it, and
some turn or look around. The rival can never be walked past. Over 20 seeds
or more, a mix outside the bands fails the run.

Water and ledges on routes: no trainer, item or sign stands on the block a
ledge lands on or the block before it; open water never meets land or trees
across a connection without a bank (seam_check.py's 'water meets land'),
two sea routes meet in open water (no wall of land along their seam); a sea
route on the main road cannot be crossed on foot, without Surf; and a river
route's water (and a coast route's sea) comes on screen (within 7 blocks across and 5 up or down,
as the camera shows) from the shortest walk through it. The end of the run prints how many of each were found.

Houses: no town has two of one special interior (the Move Deleter, the
Name Rater, a fan club...), every sign beside a house can be read, every
town has its town sign, the first town has the professor's lab (the run
starts there), and the region has one Name Rater. The
end of the run prints how many special houses have no room for their sign.

Every warp stands on a metatile that works as one: a cave mouth (on a route
or in a town) and a cave's way out at the top are walk-in doors (behaviour
MB_NON_ANIMATED_DOOR, no collision) entered from below, a building door is
an animated door, and a cave's way back out at the bottom is a south arrow
warp. A mouth drawn as rock, or a door on the wrong metatile, cannot be
entered even though the cell in front of it can be reached.

Set REGION_HASHES=FILE to append a hash of each generated region to FILE,
to compare two builds' regions seed by seed.

Usage: check_seeds.py [COUNT] [FIRST_SEED]
"""
import hashlib
import json
import os
import struct
import sys
from collections import Counter, deque

sys.path.insert(0, os.path.dirname(__file__))
import rogue_emu as P  # noqa: E402
import gen_region_map  # noqa: E402
import seam_check  # noqa: E402
import split_check  # noqa: E402
import tile_polish  # noqa: E402
import gen_rooms  # noqa: E402

OBJ_TRAINER, OBJ_ITEM, OBJ_RIVAL = 1, 2, 6
ROUTE_SEA = 8
OBJ_CUT, OBJ_ROCK, OBJ_BOULDER = 7, 8, 9
OBSTACLES = {OBJ_CUT, OBJ_ROCK, OBJ_BOULDER}
HOUSE_WARPS = {P.WARP_HOUSE} | set(range(5, 10))
NUM_GYMS = 8
# Metatile behaviours (include/constants/metatile_behaviors.h).
MB_NON_ANIMATED_DOOR, MB_LADDER, MB_SOUTH_ARROW_WARP, MB_ANIMATED_DOOR = 0x60, 0x61, 0x65, 0x69
MB_NORTH_ARROW_WARP, MB_DEEP_SOUTH_WARP = 0x64, 0x6E
# A shipwreck's way in, Route 108's stairs on the shoal, and a power
# plant's, a cave mouth's metatile.
MB_STAIRS_OUTSIDE_ABANDONED_SHIP, MB_OCEAN_WATER, MB_SHALLOW_WATER = 0x1B, 0x15, 0x17
SHIP_STAIRS, B_MOUTH = 0x36A, 0x0A7
# The wreck's stairs are three blocks, each a way aboard (the side ones on
# the spare kinds RWARP_DUNGEON_WIDE and RWARP_DUNGEON_2_WIDE).
SHIP_STAIRS_ALL = (0x369, 0x36A, 0x36B)
# A forest's ways in on its route, two blocks wide.
FOREST_WARPS = {P.WARP_DUNGEON, P.WARP_DUNGEON_WIDE, P.WARP_DUNGEON_2, P.WARP_DUNGEON_2_WIDE}
MB_JUMP = {0x38: (1, 0), 0x39: (-1, 0), 0x3A: (0, -1), 0x3B: (0, 1)}
ROUTE_RIVER, ROUTE_COAST, ROUTE_MOUNTAIN = 4, 3, 2
water_stats = Counter()
NUM_PRIMARY = 512
_behaviours = {}
# Route trainers: nobody can walk past ('blocks'), in view of the shortest
# walk but avoidable ('beside'), off it ('off'); 'turns' turn or look around.
trainer_stats = Counter()
# The mix wanted, as on Emerald's routes: some block, more can be seen from
# the way, the rest stand off it. Shares of all route trainers.
TRAINER_MIX = {'blocks': (0.15, 0.40), 'in view': (0.50, 0.85), 'turns': (0.20, 0.50)}
# Special interiors (enum RogueInterior, RINT_HOME is 0) and the sign beside
# each (enum RogueSignKind); RSIGN_TUTOR and RSIGN_GIFT stand by homes, and
# RSIGN_HOUSE (a family plate) and RSIGN_LAB are house signs too.
INTERIOR_SIGNS = {2: 12, 3: 13, 4: 14, 5: 15, 6: 16}
HOUSE_SIGNS = set(range(12, 19)) | {6, 7}
MAX_HOUSE_SIGNS = 2  # a town (AddHouseSigns)
SIGN_TOWN, SIGN_NAME_RATER, SIGN_DUNGEON = 0, 15, 19
house_stats = Counter()
hidden_stats = Counter()


def reachable(grid, starts):
    """Where the player can stand, starting from starts. On an ice floor a
    step onto the ice slides on (grid.ice): they stand only where a slide
    stops."""
    seen = set(s for s in starts if grid.ok(*s))
    stack = list(seen)
    while stack:
        x, y = stack.pop()
        for dx, dy in P.KEY_DELTA.values():
            n = (x + dx, y + dy)
            if grid.ok(*n) and not grid.step((x, y), n):
                continue
            if grid.ice and n in grid.ice and grid.ok(*n):
                n = grid.slide(n, dx, dy)
            # (A current carries the player on: v1.1 sea.)
            if grid.current and n in grid.current and grid.ok(*n):
                n = grid.carry(n)
            if n not in seen and grid.ok(*n):
                seen.add(n)
                stack.append(n)
    return seen


def edge_cell(m, d, pos):
    return {0: (pos, 0), 2: (pos, m['h'] - 1), 3: (0, pos), 1: (m['w'] - 1, pos)}[d]


def below(cell):
    return (cell[0], cell[1] + 1)


def route_camp(maps):
    """RogueGen_RouteCamp: the main road route between the 6th and 7th gyms
    with the ROADSIDE CAMP camper (the rival's, if one has it), or None."""
    best = None
    for m in maps:
        if m['kind'] != 'route' or m['side'] or m['badges'] != 6 or m['type'] in (P.ROUTE_MOUNTAIN, ROUTE_SEA):
            continue
        if m.get('rival') == 3:
            return m['i']
        if best is None:
            best = m['i']
    return best


def touches(cell, seen):
    return any((cell[0] + dx, cell[1] + dy) in seen for dx, dy in P.KEY_DELTA.values())


def entrances(m, maps):
    """Cells where the player arrives in this place, by link direction or 'cave'."""
    out = {}
    for l in m['links']:
        out[l['dir']] = edge_cell(m, l['dir'], l['pos'])
    if m['kind'] == 'cave':
        out['in'] = m['warps'][P.CAVE_SOUTH]
        if P.CAVE_NORTH in m['warps']:
            door = m['warps'][P.CAVE_NORTH]
            out['out'] = below(door)
    elif P.WARP_CAVE in m['warps']:
        out['mouth'] = below(m['warps'][P.WARP_CAVE])
    if m['start']:
        out['start'] = m['start']
    return out


def forest_of(m, maps):
    """The forest dungeon route m hosts, or None."""
    return next((d for d in maps[0].get('dungeons', []) if d['kind'] == P.DUN_FOREST and d['host'] == m['i']), None)


def forest_front(m, kind):
    """The block the player walks into a forest's way in from: below a way
    walked north into, above one walked south into."""
    x, y = m['warps'][kind]
    mid = m['blocks'][y * m['w'] + x] & 0x3FF
    mb = behaviours(m['tileset'])
    return (x, y - 1) if mid < len(mb) and mb[mid] == MB_DEEP_SOUTH_WARP else (x, y + 1)


def behaviours(tileset):
    """Behaviour of every metatile id with the General primary tileset and
    tileset as the secondary one, as the game loads a generated map."""
    if tileset not in _behaviours:
        out = []
        for kind, name, n in (('primary', 'general', NUM_PRIMARY), ('secondary', tileset, None)):
            data = open(os.path.join(P.SRC, 'data', 'tilesets', kind, name, 'metatile_attributes.bin'), 'rb').read()
            attrs = [a & 0xFF for a in struct.unpack('<%dH' % (len(data) // 2), data)]
            out += (attrs + [0] * NUM_PRIMARY)[:n] if n else attrs
        _behaviours[tileset] = out
    return _behaviours[tileset]


def check_warps(m):
    """Each warp on a metatile the player can actually use."""
    problems = []
    mb = behaviours(m['tileset'])
    for kind, (x, y) in m['warps'].items():
        block = m['blocks'][y * m['w'] + x]
        mid, solid = block & 0x3FF, (block >> 10) & 3
        b = mb[mid] if mid < len(mb) else None
        if m['kind'] == 'cave' and kind in (P.CAVE_DOWN, P.CAVE_DOWN_2):
            ok, what = b == MB_LADDER and not solid, 'ladder down'
        elif m['kind'] == 'route' and kind in FOREST_WARPS and mid in SHIP_STAIRS_ALL:
            # A shipwreck's stairs (Route 108's), stepped onto from the
            # shoal or the water below and climbed north.
            ok, what = b == MB_STAIRS_OUTSIDE_ABANDONED_SHIP and not solid, 'shipwreck stairs'
            below = m['blocks'][(y + 1) * m['w'] + x] & 0x3FF if y + 1 < m['h'] else None
            ok = ok and below is not None and mb[below] in (MB_OCEAN_WATER, MB_SHALLOW_WATER)
        elif m['kind'] == 'route' and kind == P.WARP_DUNGEON and mid == B_MOUTH:
            # A power plant's door in the rock, as a cave mouth.
            ok, what = b == MB_NON_ANIMATED_DOOR and not solid, 'power plant door'
            ok = ok and y + 1 < m['grid'].h and m['grid'].walk[(y + 1) * m['w'] + x]
        elif kind == P.WARP_DUNGEON and mid == 0x25E and m['tileset'] == 'lavaridge':
            # (v1.1) Fiery Path's doorway in the volcano's own mountain, a
            # spring town's or the foot route's, walked into from its
            # terrace just below.
            ok, what = b == MB_NON_ANIMATED_DOOR and not solid, 'doorway into the volcano'
            ok = ok and y + 1 < m['grid'].h and m['grid'].walk[(y + 1) * m['w'] + x]
        elif m['kind'] == 'route' and kind in FOREST_WARPS:
            # Route 104's way into Petalburg Woods, walked north or south into.
            ok, what = b in (MB_NON_ANIMATED_DOOR, MB_DEEP_SOUTH_WARP) and not solid, 'way into the woods %d' % kind
            fx, fy = forest_front(m, kind)
            ok = ok and 0 <= fy < m['grid'].h and m['grid'].walk[fy * m['w'] + fx]
        elif m['kind'] == 'cave' and kind == P.CAVE_SOUTH:
            ok, what = b == MB_SOUTH_ARROW_WARP and not solid, 'way out'
        elif kind == P.WARP_CAVE or (m['kind'] == 'cave' and kind == P.CAVE_NORTH):
            ok, what = b == MB_NON_ANIMATED_DOOR and not solid, 'cave mouth'
            # Walked into from the ground just below.
            ok = ok and y + 1 < m['grid'].h and m['grid'].walk[(y + 1) * m['w'] + x]
        else:
            ok, what = b == MB_ANIMATED_DOOR or (b == MB_NON_ANIMATED_DOOR and not solid), 'door %d' % kind
        if not ok:
            problems.append('map %d: %s at %d,%d on metatile 0x%03X (behaviour %s%s)'
                            % (m['i'], what, x, y, mid, 'none' if b is None else '0x%02X' % b,
                               ', solid' if solid else ''))
    return problems


def check_plan(maps):
    problems = []
    by = {m['i']: m for m in maps}
    for m in maps:
        for l in m['links']:
            t = by[l['target']]
            if t['i'] == m['i']:
                # RUN 0's loop: the opposite edge leads back in, at the same
                # place along it.
                other = [b for b in m['links'] if b['target'] == m['i'] and b['dir'] == (l['dir'] + 2) % 4]
                if len(other) != 1 or other[0]['pos'] != l['pos'] or l['offset'] != 0:
                    problems.append('loop %d does not meet itself' % m['i'])
                continue
            back = [b for b in t['links'] if b['target'] == m['i']]
            if len(back) != 1 or back[0]['dir'] != (l['dir'] + 2) % 4 or back[0]['offset'] != -l['offset']:
                problems.append('link %d->%d does not match' % (m['i'], t['i']))
            dx, dy = {0: (0, -1), 1: (1, 0), 2: (0, 1), 3: (-1, 0)}[l['dir']]
            if (m['x'] + dx, m['y'] + dy) != (t['x'], t['y']):
                problems.append('link %d->%d not on the grid' % (m['i'], t['i']))
    cells = [(m['x'], m['y']) for m in maps]
    if len(set(cells)) != len(cells):
        problems.append('two places share a grid cell')
    # Everything reachable from the first town through links and caves.
    seen, q = {0}, deque([0])
    while q:
        m = by[q.popleft()]
        nxt = [l['target'] for l in m['links']]
        if m.get('mouth', -1) >= 0:
            nxt.append(m['mouth'])
        if m['kind'] == 'cave':
            nxt += [p for p in (m['caveIn'], m['caveOut']) if p >= 0]
        for n in nxt:
            if n not in seen:
                seen.add(n)
                q.append(n)
    if len(seen) != len(maps):
        problems.append('%d places cannot be reached' % (len(maps) - len(seen)))
    gyms = sorted((m['gym'], m['i']) for m in maps if m['kind'] == 'town' and m['gym'] >= 0)
    if [g for g, _ in gyms] != list(range(NUM_GYMS)):
        problems.append('gyms %s' % [g for g, _ in gyms])
    # Gates only on the way out of the first town (a starter), out of the
    # last gym town to Victory Road (eight badges), and on the way on out of
    # the villains' hideout town (check_villain).
    h = P.hideout(maps)
    for m in maps:
        for l in m['links']:
            if not l['gate']:
                continue
            if h is not None and m['i'] == h['host'] and l['gate'] == P.GATE_VILLAIN:
                ok = l['target'] == m['i'] + 1
            elif m['kind'] == 'town' and m['type'] == P.TOWN_START:
                ok = l['gate'] == 1
            else:
                t = by[l['target']]
                ok = m['kind'] == 'town' and m['gym'] == NUM_GYMS - 1 and t['kind'] == 'route' and t['type'] == P.ROUTE_MOUNTAIN \
                    and l['gate'] == 1 + NUM_GYMS
            if not ok:
                problems.append('gate %d out of map %d' % (l['gate'], m['i']))
    return problems


ROUTE_KINDS = ['forest', 'lake', 'mountain', 'coast', 'river', 'hills', 'field', 'desert', 'sea', 'woods', 'flowers', 'longgrass', 'bridge']
ROUTE_DESERT = 7
RMAP_VIEW_W, RMAP_VIEW_H = 30, 15
# The region map's seams: the share that must be smooth (see
# gen_region_map.check_map), and how many plain green tiles it may have.
RMAP_SMOOTH = 0.92
RMAP_PLAIN = 6
rmap_stats = Counter()
tile_stats = Counter()  # tile_polish kinds found, and 'maps' checked
# RUN 0 (v1.0): the region of ROGUE_RUN0_SEED (ROGUE_REGION_COUNT, the first
# seed past the names) is corrupted on purpose (src/rogue_gen_run0.h): its
# broken trees, cliffs and copied buildings are counted apart, not held to
# the tiling budgets, and its loop route's links to itself are checked as a
# loop. Everything else is checked as for any region. ROGUE_RUN0_ACT picks
# the act its maps are made for (1 by default).
RUN0_SEED = 16384
run0_tiles = Counter()
region_stats = {'before_gym1': [], 'shortcuts': 0, 'forks': 0, 'both': 0, 'hamlet': 0, 'hamlet_early': 0, 'kinds': {},
                'crossroads': [], 'sea': 0, 'second sea': 0}
chapter_stats = {'split': Counter(), 'first': Counter(), 'locks': Counter(), 'spot': Counter(), 'two ways': 0,
                 'hm order': Counter(), 'surf gym': Counter(), 'hm gym': Counter(),
                 'optional': Counter(), 'states': []}


def stages(maps):
    """When each place opens if the gyms are won in order along the road:
    the gyms won by then, and the place it is first reached from."""
    need, came = {}, {}
    for g in range(NUM_GYMS + 1):
        for i, c in P.reach_with(maps, P.keys_held(maps, (1 << g) - 1)).items():
            if i not in need:
                need[i], came[i] = g, c
    return need, came


def road_of(maps):
    """The main road in order, from the first town to the summit."""
    by = {m['i']: m for m in maps}
    road = [0]
    while not (by[road[-1]]['kind'] == 'town' and by[road[-1]]['type'] == P.TOWN_SUMMIT):
        m = by[road[-1]]
        nxt = [l['target'] for l in m['links'] if l['target'] > m['i'] and not by[l['target']]['side']]
        if m.get('mouth', -1) >= 0 and by[m['mouth']]['caveIn'] == m['i']:
            nxt.append(m['mouth'])
        if m['kind'] == 'cave':
            nxt.append(m['caveOut'])
        road.append(min(nxt))
    return road


def side_ends(by, start):
    """The main road places a chain of side routes from start joins."""
    seen, q, ends = {start}, [start], set()
    while q:
        m = by[q.pop()]
        for l in m['links']:
            t = by[l['target']]
            if not t['side']:
                ends.add(t['i'])
            elif t['i'] not in seen:
                seen.add(t['i'])
                q.append(t['i'])
    return ends


def check_chapters(maps):
    """Chapters and keys: blocks on the main road end each chapter, each
    passed only with the keys of its chapter's badges (or the sea with Surf,
    the League with all eight); with any set of keys the road is open
    exactly up to the first block they do not pass; and no order of winning
    the gyms leaves the player stuck."""
    problems = []
    by = {m['i']: m for m in maps}
    gyms = maps[0]['gyms']
    road = road_of(maps)
    step = {p: i for i, p in enumerate(road)}
    town = {g['n']: g['place'] for g in gyms}
    chapter = {g['n']: g['chapter'] for g in gyms}
    # Blocks along the road: a guarded main route (its way in from the place
    # before), the first sea route of a run, the League's gate.
    blocks = []
    for i, p in enumerate(road):
        m = by[p]
        prev = by[road[i - 1]] if i else None
        # The villains' blockade may stand at the way into a blocked route.
        if prev is not None and any(l['target'] == p and l['gate'] == P.GATE_VILLAIN for l in prev['links']):
            blocks.append({'at': i, 'kind': 'villain', 'keys': P.KEY_VILLAIN})
        if m['kind'] == 'route' and m.get('obstacle') and not m['side']:
            l = next((l for l in m['links'] if l['target'] == road[i - 1]), None)
            if l is None or l['dir'] != m['guardDir']:
                problems.append('block on map %d does not face the way in' % p)
            blocks.append({'at': i, 'kind': m['obstacle'], 'keys': P.key(m['obstacle'])})
        elif m['kind'] == 'route' and m['type'] == ROUTE_SEA and not any(b['kind'] == 'sea' for b in blocks):
            blocks.append({'at': i, 'kind': 'sea', 'keys': P.key(P.OBST_SURF)})
        elif prev is not None and any(l['target'] == p and l['gate'] == P.GATE_LEAGUE for l in prev['links']):
            blocks.append({'at': i, 'kind': 'league', 'keys': P.KEY_LEAGUE})
    zone = lambda i: sum(1 for b in blocks if b['at'] <= i)
    # Chapters end at the blocks a badge passes, not at the villains'.
    chapter_zone = lambda i: sum(1 for b in blocks if b['at'] <= i and b['kind'] != 'villain')
    if not blocks or blocks[-1]['kind'] != 'league' or sum(b['kind'] == 'league' for b in blocks) != 1:
        problems.append('no League gate at the end of the road')
    # A chapter's second way on: a fork whose ends lie on either side of
    # one block, behind another obstacle.
    for m in maps:
        if m.get('branch') == 3 and m.get('obstacle'):
            ends = sorted(step[e] for e in side_ends(by, m['i']) if e in step)
            if len(ends) == 2 and zone(ends[1]) == zone(ends[0]) + 1 and blocks[zone(ends[0])]['kind'] == 'villain':
                problems.append('fork %d leads round the villains\' blockade' % m['i'])
            elif len(ends) == 2 and zone(ends[1]) == zone(ends[0]) + 1:
                b = blocks[zone(ends[0])]
                b['keys'] |= P.key(m['obstacle'])
                b['fork'] = m['obstacle']
            elif len(ends) == 2 and zone(ends[1]) != zone(ends[0]):
                problems.append('fork %d spans %d blocks' % (m['i'], zone(ends[1]) - zone(ends[0])))
    # Chapters: the gyms between two blocks, in order, one to three of them.
    gym_zone = {g: chapter_zone(step[town[g]]) for g in town}
    if [gym_zone[g] for g in range(NUM_GYMS)] != [chapter[g] for g in range(NUM_GYMS)]:
        problems.append('chapters %s, blocks put gyms in %s' % ([chapter[g] for g in range(NUM_GYMS)],
                                                                [gym_zone[g] for g in range(NUM_GYMS)]))
    sizes = Counter(chapter.values())
    if any(not 1 <= n <= 3 for n in sizes.values()):
        problems.append('chapter sizes %s' % sorted(sizes.items()))
    # Each block's keys come from gyms of the chapter it ends.
    for z, b in enumerate(b for b in blocks if b['kind'] != 'villain'):
        if b['kind'] == 'league':
            continue
        for o in [b['kind'] if b['kind'] != 'sea' else P.OBST_SURF] + ([b['fork']] if 'fork' in b else []):
            g = P.key_gym(maps, o)
            if g is None:
                problems.append('no gym gives the key to block %d' % z)
            elif b['kind'] == 'sea' and gym_zone[g] > z:
                problems.append('SURF comes from gym %d, past the sea' % (g + 1))
            elif b['kind'] != 'sea' and gym_zone[g] != z:
                problems.append('the key to block %d comes from gym %d in chapter %d' % (z, g + 1, gym_zone[g]))
    # No softlock: from every set of won gyms the player can reach, some gym
    # not yet won can be reached, until all eight are won and the League
    # opens. And for each such set, exactly the chapters its keys open: no
    # shortcut, fork, tunnel or ring road leads round a block (as
    # GatingHolds in src/rogue_gen.c). A way may cross several blocks when
    # the badge that opens it cannot be held before they are passed.
    states, todo = {0}, [0]
    while todo:
        won = todo.pop()
        keys = P.keys_held(maps, won)
        seen = P.reach_with(maps, keys)
        for i, p in enumerate(road):
            want = all(blocks[j]['keys'] & keys for j in range(zone(i)))
            if (p in seen) != want:
                problems.append('with gyms %s won the road at map %d is %s' % (
                    [g + 1 for g in range(NUM_GYMS) if (won >> g) & 1], p, 'open' if p in seen else 'shut'))
                break
        if won == 0xFF and len(seen) != len(maps):
            problems.append('%d places never reachable' % (len(maps) - len(seen)))
        if won == 0xFF:
            if maps and next(m['i'] for m in maps if m['kind'] == 'town' and m['type'] == P.TOWN_SUMMIT) not in seen:
                problems.append('the League shut with eight badges')
            continue
        open_gyms = [g for g in range(NUM_GYMS) if not (won >> g) & 1 and town[g] in seen]
        if not open_gyms:
            problems.append('stuck with gyms %s won' % [g + 1 for g in range(NUM_GYMS) if (won >> g) & 1])
            break
        for g in open_gyms:
            if won | (1 << g) not in states:
                states.add(won | (1 << g))
                todo.append(won | (1 << g))
    chapter_stats['states'].append(len(states))
    # Variety numbers.
    names = {P.OBST_CUT: 'CUT', P.OBST_ROCK_SMASH: 'ROCK SMASH', P.OBST_STRENGTH: 'STRENGTH', P.OBST_SURF: 'SURF', 'sea': 'sea'}
    split, last = [], None
    for g in range(NUM_GYMS):
        if chapter[g] != last:
            split.append(0)
            last = chapter[g]
        split[-1] += 1
    text = ''
    ends = [b for b in blocks if b['kind'] != 'villain']
    for z, n in enumerate(split):
        text += ('' if z == 0 else (' | sea | ' if ends[z - 1]['kind'] == 'sea' else ' | ')) + str(n)
    chapter_stats['split'][text] += 1
    chapter_stats['first'][split[0]] += 1
    for b in blocks:
        if b['kind'] in ('league', 'villain'):
            continue
        chapter_stats['locks'][names[b['kind']] if b['kind'] in names else b['kind']] += 1
        if b['kind'] != 'sea':
            prev = by[road[b['at'] - 1]]
            chapter_stats['spot']['out of a gym town' if prev['kind'] == 'town' and prev['gym'] >= 0
                                  else 'out of a village' if prev['kind'] == 'town' else 'along the road'] += 1
        if 'fork' in b:
            chapter_stats['two ways'] += 1
    order = sorted((P.key_gym(maps, o), names[o]) for o in (P.OBST_CUT, P.OBST_ROCK_SMASH, P.OBST_STRENGTH))
    chapter_stats['hm order'][' < '.join(n for _, n in order)] += 1
    chapter_stats['surf gym'][P.key_gym(maps, P.OBST_SURF) + 1] += 1
    for o in (P.OBST_CUT, P.OBST_ROCK_SMASH, P.OBST_STRENGTH):
        chapter_stats['hm gym'][(names[o], P.key_gym(maps, o) + 1)] += 1
    # Gyms the player never needs to win to move on (only for the League).
    needed = {P.key_gym(maps, b['kind'] if b['kind'] != 'sea' else P.OBST_SURF) for b in blocks if b['kind'] not in ('league', 'villain')}
    chapter_stats['optional'][NUM_GYMS - len(needed)] += 1
    return problems


TRAIT_NAMES = ['lakes', 'sea', 'woods', 'rock', 'volcano', 'meadows']
TRAIT_LAKES, TRAIT_ASH = 0, 4
trait_stats = Counter()


def check_traits(maps, rm):
    """What the professor says the region is like shows on its map: a
    volcano (ash) has its crater, lakes and rivers a lake. The crater is
    four tiles, two by two, on the volcano's cell, inland."""
    problems = []
    tr = maps[0].get('traits')
    if not tr:
        return problems
    tiles = rm['tiles']
    first = gen_region_map.NUM_TILES
    crater = [(x, y) for y, row in enumerate(tiles) for x, t in enumerate(row) if t == first]
    vx, vy = tr['volcano']
    trait_stats['regions'] += 1
    trait_stats['volcano drawn'] += bool(crater)
    trait_stats['lake drawn'] += tr['lakecells'] > 0
    for t in (tr['first'], tr['second']):
        trait_stats['says ' + TRAIT_NAMES[t]] += 1
    named = (tr['first'], tr['second'])
    if (vx >= 0) != bool(crater) or len(crater) > 1:
        problems.append('volcano at %d,%d but %d craters drawn' % (vx, vy, len(crater)))
    for x, y in crater:
        block = [tiles[y + j][x + i] if y + j < len(tiles) and x + i < len(tiles[0]) else -1 for j in (0, 1) for i in (0, 1)]
        if block != [first, first + 1, first + 2, first + 3]:
            problems.append('crater at %d,%d drawn as %s' % (x, y, block))
        # Cells are two tiles apart: place 0's tile and cell give the rest.
        (px, py), (gx, gy) = rm['pos'][0], maps[0]['grid_pos']
        if (x, y) != (px + 2 * (vx - gx), py + 2 * (vy - gy)):
            problems.append('crater at tile %d,%d is not on cell %d,%d' % (x, y, vx, vy))
    if TRAIT_ASH in named and not crater:
        problems.append('the professor speaks of a volcano the map does not show')
    if TRAIT_LAKES in named and tr['lakecells'] == 0:
        problems.append('the professor speaks of lakes the map does not show')
    return problems


def check_region(maps):
    """The region invariants: strict badge gating, levels, limits, the map
    fitting one screen, and the shape rules."""
    problems = []
    by = {m['i']: m for m in maps}
    need, came = stages(maps)
    problems += check_chapters(maps)
    # Every place reachable. Main road places count the gyms passed along
    # it; side places the gyms won, in order along the road, when they open.
    road = road_of(maps)
    passed = 0
    for p in road:
        if by[p]['badges'] != passed:
            problems.append('road place %d planned after %d gyms, passed %d' % (p, by[p]['badges'], passed))
        passed += by[p]['kind'] == 'town' and by[p]['gym'] >= 0
    for m in maps:
        if m['i'] not in need:
            problems.append('place %d never reachable' % m['i'])
        elif m['side'] and need[m['i']] != m['badges']:
            problems.append('place %d opens after %d gyms, planned %d' % (m['i'], need[m['i']], m['badges']))
    for m in maps:
        if m['kind'] == 'town' and m['gym'] >= 0 and need.get(m['i'], 99) > m['gym']:
            problems.append('gym %d town reached after %s gyms' % (m['gym'] + 1, need.get(m['i'])))
    # Levels never drop sharply on the way a side place is first reached.
    # (The main road keeps its levels: a shortcut into a chapter may come
    # down onto it from a later stretch. A tunnel comes later than the
    # levels, which follow the way each place is reached without it; its
    # floors follow the badges held.)
    tun = maps[0].get('tunnel')
    if tun:
        maps[0]['tunnel'] = None
        _, came = stages(maps)
        maps[0]['tunnel'] = tun
    for m in maps:
        c = came.get(m['i'])
        if c is not None and m['side'] and m['kind'] != 'town' and by[c]['kind'] != 'town' and m['level'] < by[c]['level'] - 4:
            problems.append('level drops from %d (map %d) to %d (map %d)' % (by[c]['level'], c, m['level'], m['i']))
    # Limits.
    if len(maps) > 42:
        problems.append('%d places' % len(maps))
    if sum(1 for m in maps if m['kind'] == 'town') > 14:
        problems.append('too many towns')
    for m in maps:
        dirs = [l['dir'] for l in m['links']]
        if len(dirs) > 4 or len(set(dirs)) != len(dirs):
            problems.append('map %d exits %s' % (m['i'], dirs))
        if m.get('mouth', -1) >= 0 and 0 in dirs:
            problems.append('map %d has a cave mouth and a north exit' % m['i'])
        # (A side route out to an island, v1.1 sea, is a dead end.)
        if m['kind'] == 'route' and m['type'] == ROUTE_SEA and len(m['links']) != (1 if m['side'] else 2):
            problems.append('sea route %d has %d exits' % (m['i'], len(m['links'])))
    if sum(m['nTrainers'] for m in maps) > 240 - 40:
        problems.append('too many trainers')
    # The region map fits one screen.
    rm = maps[0].get('rmap')
    if rm:
        e = rm.get('extent', {'x1': 0, 'y1': 0})
        if rm['height'] > RMAP_VIEW_H or e['x1'] >= RMAP_VIEW_W or e['y1'] >= RMAP_VIEW_H:
            problems.append('region map %s is bigger than %dx%d' % (e, RMAP_VIEW_W, RMAP_VIEW_H))
        for i, (x, y) in rm['pos'].items():
            if not (0 <= x < RMAP_VIEW_W and 0 <= y < RMAP_VIEW_H):
                problems.append('place %d drawn at %d,%d' % (i, x, y))
        if len(set(rm['pos'].values())) != len(rm['pos']):
            problems.append('two places drawn on one tile')
        # The tiles join as on Hoenn's map: smooth seams, hardly any plain
        # green.
        seams, rough = gen_region_map.check_map(rm['tiles'])
        rmap_stats.update(seams)
        rmap_stats['maps'] += 1
        if seams['smooth'] < RMAP_SMOOTH * seams['seams']:
            problems.append('region map: %d of %d seams rough, first at %d,%d' % (len(rough), seams['seams'], rough[0][0], rough[0][1]))
        if seams['plain'] > RMAP_PLAIN:
            problems.append('region map: %d plain green tiles' % seams['plain'])
        problems += check_traits(maps, rm)
    # The main road: gyms in order with one to three places between later
    # ones (the fourth gym's cave on top), the sea after the fifth badge.
    main = [m for m in maps if not m['side']]
    gyms = sorted(m['i'] for m in main if m['kind'] == 'town' and m['gym'] >= 0)
    for a, b in zip(gyms[1:], gyms[2:]):
        between = [m for m in main if a < m['i'] < b and m['kind'] != 'cave']
        if not 1 <= len(between) <= 4:
            problems.append('%d places between gyms at %d and %d' % (len(between), a, b))
    sea = [m for m in main if m['kind'] == 'route' and m['type'] == ROUTE_SEA]
    region_stats['sea'] += bool(sea)
    # v1.1 sea: the first stretch after the fifth badge, a second one (in
    # some regions) after the sixth or seventh, both crossed with Surf, which
    # a gym before the first gives.
    stretches = sorted({m['badges'] for m in sea})
    region_stats['second sea'] += len(stretches) > 1
    if stretches and (stretches[0] != 5 or len(stretches) > 2 or any(b not in (6, 7) for b in stretches[1:])):
        problems.append('sea routes after %s badges' % stretches)
    surf = P.key_gym(maps, P.OBST_SURF) if sea else None
    if sea and (surf is None or surf >= 5):
        problems.append('SURF from gym %s, not before the sea' % surf)
    # A wild counter can be put before every gym: a catch area with that
    # many badges. Catch areas before the first gym.
    for g in range(NUM_GYMS):
        if not any(m['kind'] != 'town' and m['badges'] == g for m in maps):
            problems.append('no catch area before gym %d' % (g + 1))
    region_stats['before_gym1'].append(sum(1 for m in maps if m['kind'] != 'town' and m['badges'] == 0))
    # Route kinds: never three of a kind in a row, the first never ash.
    routes = sorted((m for m in main if m['kind'] == 'route' and m['type'] != P.ROUTE_MOUNTAIN), key=lambda m: m['index'])
    for a, b, c in zip(routes, routes[1:], routes[2:]):
        # (Three sea routes in a row are a long crossing, as Routes 105 to
        # 109 and 124 to 127 are.)
        if a['type'] == b['type'] == c['type'] != ROUTE_SEA:
            problems.append('routes %d-%d all %s' % (a['index'], c['index'], ROUTE_KINDS[a['type']]))
    if routes and routes[0]['type'] == ROUTE_DESERT:
        problems.append('route 1 is desert')
    # Themes: gym towns all differ; a hamlet differs from the town it hangs
    # off.
    themes = [m['theme'] for m in maps if m['kind'] == 'town' and m['type'] == P.TOWN_GYM]
    if len(set(themes)) != len(themes):
        problems.append('gym towns share a theme')
    for m in maps:
        if m['kind'] == 'town' and m['type'] == P.TOWN_HAMLET:
            at = m['i']
            while by[at]['side'] and came.get(at) is not None:
                at = came[at]
            nxt = at
            while by[nxt]['kind'] != 'town':
                ahead = [l['target'] for l in by[nxt]['links'] if l['target'] > nxt and not by[l['target']]['side']]
                if by[nxt].get('mouth', -1) >= 0 and by[by[nxt]['mouth']]['caveIn'] == nxt:
                    ahead.append(by[by[nxt]['mouth']]['caveOut'])
                if by[nxt]['kind'] == 'cave':
                    ahead.append(by[nxt]['caveOut'])
                ahead = [a for a in ahead if a is not None and a >= 0]
                if not ahead:
                    break
                nxt = min(ahead)
            if by[nxt]['kind'] == 'town' and by[nxt]['theme'] == m['theme']:
                problems.append('hamlet %d copies the theme of town %d' % (m['i'], nxt))
    # Reasons to wander and come back: a shortcut back or a fork, obstacles
    # of two kinds or more, and a shortcut or a side way that opens only
    # with a badge won after the player first passes it.
    shortcut = any(m.get('branch') == 2 for m in maps)
    fork = any(m.get('branch') == 3 for m in maps)
    kinds = {m['obstacle'] for m in maps if m.get('obstacle')}
    later = any(m.get('obstacle') and m['side'] and l['dir'] == m['guardDir']
                and P.key_gym(maps, m['obstacle']) + 1 > need.get(l['target'], 0)
                for m in maps for l in m['links'])
    hamlets = [need.get(m['i'], 99) for m in maps if m['kind'] == 'town' and m['type'] == P.TOWN_HAMLET]
    if not (shortcut or fork):
        problems.append('no shortcut and no fork')
    if len(kinds) < 2:
        problems.append('obstacles of only %d kind' % len(kinds))
    if not (shortcut or later):
        problems.append('nothing opens with a later badge')
    region_stats['shortcuts'] += shortcut
    region_stats['forks'] += fork
    region_stats['both'] += shortcut and fork
    region_stats['hamlet'] += bool(hamlets)
    region_stats['hamlet_early'] += any(b < 6 for b in hamlets)
    region_stats['kinds'][len(kinds)] = region_stats['kinds'].get(len(kinds), 0) + 1
    crossroads = sum(1 for m in main if m['kind'] == 'town' and len(m['links']) + (m.get('mouth', -1) >= 0) >= 3)
    region_stats['crossroads'].append(crossroads)
    if not 1 <= crossroads <= 2:
        problems.append('%d crossroads towns' % crossroads)
    return problems


def obstacle_outside(m, maps):
    """A side route or a block on the main road whose obstacle (not water)
    stands in a lane of the route beside it, on the near side of the seam
    (ObstacleOutside in rogue_gen.c)."""
    if m.get('obstacle', 0) in (P.OBST_NONE, P.OBST_SURF):
        return False
    t = next((maps[l['target']] for l in m['links'] if l['dir'] == m['guardDir']), None)
    return t is not None and t['kind'] == 'route' and t['type'] not in (ROUTE_SEA, ROUTE_MOUNTAIN)


def gate_lanes(m, maps):
    """Directions of this map's exits whose side place keeps its obstacle
    in a lane on this side."""
    return [l['dir'] for l in m['links']
            if obstacle_outside(maps[l['target']], maps) and maps[l['target']]['guardDir'] == (l['dir'] + 2) % 4]


def bridge_steps(m):
    """A log bridge's step tiles: walkable blocks at elevation 0 beside a
    land end (4, beside the planks at 15), the only way between the
    ground's 3 and the 4 (Grid.step)."""
    w, h, blocks = m['w'], m['h'], m['blocks']

    def elev(x, y):
        return blocks[y * w + x] >> 12 if 0 <= x < w and 0 <= y < h else -1

    out = set()
    for y in range(h):
        for x in range(w):
            if blocks[y * w + x] >> 12 or blocks[y * w + x] & 0x0C00:
                continue
            for dx, dy in P.KEY_DELTA.values():
                ex, ey = x + dx, y + dy
                if elev(ex, ey) == 4 and any(elev(ex + ddx, ey + ddy) == 15 for ddx, ddy in P.KEY_DELTA.values()):
                    out.add((x, y))
    return out


def is_raft(m):
    """A raft village out on the sea (v1.1, src/rogue_gen_towns2.h): crossed
    by Surf like a sea route, with no Mart, as Pacifidlog."""
    return m['kind'] == 'town' and m.get('tileset') == 'pacifidlog'


def check_map(m, maps):
    problems = []
    name = 'map %d' % m['i']
    grid = m['grid']
    if (m['kind'] == 'route' and m['type'] == ROUTE_SEA) or is_raft(m):
        # Crossed by Surf: water counts as ground.
        grid = P.Grid(m['blocks'], m['w'], ground_only=False)
        grid.blocked = set(m['grid'].blocked)
        grid.current = m['grid'].current
    for o in m['objs']:
        if o['kind'] == OBJ_RIVAL:
            grid.blocked.discard((o['x'], o['y']))
    ents = entrances(m, maps)
    guard = m.get('obstacle', 0) and not obstacle_outside(m, maps)
    lanes = gate_lanes(m, maps)
    blocked = None
    gone = set()  # a guard's objects, cleared before looking at the lanes
    if guard:
        first = ents.get(m['guardDir'])
        if first is None:
            return ['%s: no way in on the guarded side' % name]
        outside = reachable(grid, [first])
        # Clear the obstacle: objects go, water becomes walkable.
        cleared = P.Grid(m['blocks'], m['w'], ground_only=False)
        cleared.blocked = set(grid.blocked) - {(o['x'], o['y']) for o in m['objs'] if o['kind'] in OBSTACLES}
        seen = reachable(cleared, [first])
        for k, c in ents.items():
            if k != m['guardDir'] and c in outside:
                problems.append('%s: %s reachable without clearing the obstacle' % (name, k))
        objs_past = [o for o in m['objs'] if o['kind'] in (OBJ_TRAINER, OBJ_ITEM)]
        for o in objs_past:
            if touches((o['x'], o['y']), outside):
                problems.append('%s: object at %d,%d reachable without clearing' % (name, o['x'], o['y']))
        gone = {(o['x'], o['y']) for o in m['objs'] if o['kind'] in OBSTACLES and touches((o['x'], o['y']), outside)}
        if not gone and m['obstacle'] != P.OBST_SURF:
            problems.append('%s: obstacle missing' % name)
    if lanes:
        # Each lane's obstacle keeps its exit out of reach until cleared,
        # and nothing else may need it.
        starts = [ents[m['guardDir']]] if guard else [c for k, c in ents.items() if k not in lanes]
        lane_grid = P.Grid(m['blocks'], m['w'], ground_only=False)
        lane_grid.walk = grid.walk if not (guard and m['obstacle'] == P.OBST_SURF) else lane_grid.walk
        lane_grid.blocked = set(grid.blocked) - gone
        blocked = reachable(lane_grid, starts[:1])
        cleared = P.Grid(m['blocks'], m['w'], ground_only=False)
        cleared.walk = lane_grid.walk
        cleared.blocked = set(grid.blocked) - {(o['x'], o['y']) for o in m['objs'] if o['kind'] in OBSTACLES}
        seen = reachable(cleared, starts[:1])
        for d in lanes:
            if ents[d] in blocked:
                problems.append('%s: exit %d reachable without clearing the obstacle' % (name, d))
            if ents[d] not in seen:
                problems.append('%s: exit %d unreachable with the obstacle cleared' % (name, d))
            if not [o for o in m['objs'] if o['kind'] in OBSTACLES and touches((o['x'], o['y']), blocked)]:
                problems.append('%s: no obstacle in the lane to exit %d' % (name, d))
        seen = blocked
    if not lanes and not guard and m['kind'] == 'cave' and P.CAVE_DOWN_2 in m['warps'] and 'out' in ents:
        # The cave of a dungeon on the way: two parts that do not join, the
        # way through goes by the floors below (check_dungeons).
        seen = reachable(grid, [ents['in']])
        if ents['out'] in seen:
            problems.append('%s: the two parts of the cave join' % name)
        if not touches(m['warps'][P.CAVE_DOWN], seen):
            problems.append('%s: no ladder down from the way in' % name)
        far = reachable(grid, [ents['out']])
        if not touches(m['warps'][P.CAVE_DOWN_2], far):
            problems.append('%s: no ladder up to the way out' % name)
        seen |= far
    elif not lanes and not guard:
        starts = [c for c in ents.values()]
        if not starts:
            return ['%s: no way in' % name]
        start = ents.get('start') or ents.get('in') or ents.get('mouth') or starts[0]
        if not grid.ok(*start):
            return ['%s: start %s blocked' % (name, start)]
        seen = reachable(grid, [start])
    forest = forest_of(m, maps) if m['kind'] == 'route' else None
    split = forest and forest['exit'] == m['i']
    if split:
        # The road goes through the forest: its two ways in join the halves.
        if not {P.WARP_DUNGEON, P.WARP_DUNGEON_2} <= set(m['warps']):
            return problems + ['%s: forest across the road without both ways in' % name]
        fa, fb = forest_front(m, P.WARP_DUNGEON), forest_front(m, P.WARP_DUNGEON_2)
        if fb in seen:
            problems.append('%s: the way on is reached without going through the forest' % name)
        if fa in seen:
            seen |= reachable(grid, [fb])
        elif fb in seen:
            seen |= reachable(grid, [fa])
    elif forest and P.WARP_DUNGEON not in m['warps']:
        problems.append('%s: no way into its forest' % name)
    for k, c in ents.items():
        if k not in lanes and c not in seen:
            problems.append('%s: entrance %s unreachable' % (name, k))
    if grid.current:
        # v1.1 sea: a current runs one way, so every way in must reach every
        # other, every item be reached with a way back to a way in, and every
        # hidden item be reached.
        for k, c in ents.items():
            back = reachable(grid, [c])
            for k2, c2 in ents.items():
                if c2 not in back:
                    problems.append('%s: entrance %s unreachable from %s (currents)' % (name, k2, k))
        for o in m['objs']:
            if o['kind'] != OBJ_ITEM:
                continue
            near = [n for n in ((o['x'] + dx, o['y'] + dy) for dx, dy in P.KEY_DELTA.values()) if n in seen]
            if not near or not any(set(ents.values()) & reachable(grid, [n]) for n in near):
                problems.append('%s: no way back from the item at %d,%d (currents)' % (name, o['x'], o['y']))
        for h in m.get('hidden', []):
            if not touches((h['x'], h['y']), seen):
                problems.append('%s: hidden item at %d,%d unreachable (currents)' % (name, h['x'], h['y']))
    # Hidden items lie on ground that can be reached (by Surf on the sea).
    for h in m.get('hidden', []):
        if not (m['kind'] == 'route' and m['type'] == ROUTE_SEA) and not touches((h['x'], h['y']), seen):
            problems.append('%s: hidden item at %d,%d unreachable' % (name, h['x'], h['y']))
        hidden_stats['n'] += 1
    if m['kind'] == 'town':
        for kind, door in m['warps'].items():
            if below(door) not in seen:
                problems.append('%s: door %d unreachable' % (name, kind))
        want = {P.WARP_PC, P.WARP_MART}
        if m['type'] == P.TOWN_HAMLET or is_raft(m):
            want = {P.WARP_PC}
        if m['gym'] >= 0 or m['type'] == P.TOWN_SUMMIT:
            want.add(P.WARP_GYM)
        if not want <= set(m['warps']):
            problems.append('%s: warps %s' % (name, sorted(m['warps'])))
        # (A cramped town may give its one house up as the way into its
        # hideout or tower.)
        if m['type'] != P.TOWN_SUMMIT and P.WARP_HOUSE not in m['warps'] and P.WARP_DUNGEON not in m['warps']:
            problems.append('%s: no house' % name)
        # The run starts in the professor's lab (WarpToStart): the first town
        # always has it, and its door can be reached (checked above).
        if m['type'] == P.TOWN_START and P.lab_warp(m) not in m['warps']:
            problems.append('%s: no lab' % name)
        # Each special interior at most once a town, each with its sign
        # (counted: a house with no room beside it may go without), and
        # every sign beside a house can be read.
        kinds = [sg['kind'] for sg in m.get('signs', [])]
        for k, n in Counter(m['houses']).items():
            if k != 0 and n > 1:
                problems.append('%s: %d houses with interior %d' % (name, n, k))
        for k in m['houses']:
            if k in INTERIOR_SIGNS:
                house_stats['special'] += 1
                house_stats['unsigned'] += INTERIOR_SIGNS[k] not in kinds
        if sum(k in HOUSE_SIGNS for k in kinds) > MAX_HOUSE_SIGNS:
            problems.append('%s: %d house signs' % (name, sum(k in HOUSE_SIGNS for k in kinds)))
        for sg in m.get('signs', []):
            if sg['kind'] in HOUSE_SIGNS and not touches((sg['x'], sg['y']), seen):
                problems.append('%s: house sign at %d,%d cannot be read' % (name, sg['x'], sg['y']))
    else:
        for kind, door in m['warps'].items():
            front = forest_front(m, kind) if m['kind'] == 'route' and kind in FOREST_WARPS else below(door)
            if kind != P.WARP_CAVE and kind not in (P.CAVE_SOUTH, P.CAVE_NORTH) and front not in seen:
                problems.append('%s: door %d unreachable' % (name, kind))
        trainers = [o for o in m['objs'] if o['kind'] == OBJ_TRAINER]
        items = [o for o in m['objs'] if o['kind'] == OBJ_ITEM]
        if len(trainers) != m['nTrainers'] or len(items) != m['nItems']:
            problems.append('%s: %d trainers, %d items' % (name, len(trainers), len(items)))
        rivals = [o for o in m['objs'] if o['kind'] == OBJ_RIVAL]
        if len(rivals) != (1 if m['rival'] else 0):
            problems.append('%s: %d rivals' % (name, len(rivals)))
        camps = [o for o in m['objs'] if o['kind'] == P.OBJ_CAMP]
        # LATE CAMP's camper, and Victory Road's past the rival.
        if len(camps) != (1 if m['i'] == route_camp(maps) or m.get('rival') == 4 else 0):
            problems.append('%s: %d roadside camps' % (name, len(camps)))
        for o in camps:
            if not touches((o['x'], o['y']), seen):
                problems.append('%s: roadside camp at %d,%d cannot be reached' % (name, o['x'], o['y']))
        # Does the shortest walk through pass in front of each trainer?
        # The main road through: in from the lower numbered place, on to
        # the higher one or the cave mouth.
        by = {x['i']: x for x in maps}
        main = [l for l in m['links'] if not by[l['target']].get('side')]
        a = end = None
        for l in main:
            if l['target'] < m['i']:
                a = ents[l['dir']]
            else:
                end = ents[l['dir']]
        end = ents.get('mouth', end)
        # Where the forest cuts the route, the walk through is in two parts:
        # to the forest, and from its other end on.
        legs = [(a, end)]
        if split and a and end:
            legs = [(a, forest_front(m, P.WARP_DUNGEON)), (forest_front(m, P.WARP_DUNGEON_2), end)]
        for a, end in (legs if m['kind'] == 'route' and a and end and not guard else []):
            # A block on the road ahead in a lane here is walked through
            # once cleared.
            grid.blocked = set(grid.blocked) - {(o['x'], o['y']) for o in m['objs'] if o['kind'] in OBSTACLES}
            walk = grid.path(a, end) or []
            cells, cur = set(), a
            for key in walk:
                dx, dy = P.KEY_DELTA[key]
                cur = (cur[0] + dx, cur[1] + dy)
                cells.add(cur)
            lines = []
            steps = bridge_steps(m)
            near = reachable(grid, [a]) if len(legs) > 1 else None
            for o in trainers + rivals:
                if near is not None and not touches((o['x'], o['y']), near):
                    continue
                # A trainer that turns or looks around sees every way it faces.
                turn = o.get('turn', -1)
                faces = {o['facing']} if turn < 0 else {0, 1, 2, 3} if turn == 4 else {o['facing'], turn}
                view, stops = set(), set()
                for f in faces:
                    dx, dy = {0: (0, -1), 1: (1, 0), 2: (0, 1), 3: (-1, 0)}[f]
                    for k in range(1, o.get('range', 4) + 1):
                        c = (o['x'] + dx * k, o['y'] + dy * k)
                        if not grid.ok(*c) and c != end:
                            break
                        # A log bridge's land end (4) or planks (15): a
                        # trainer on the ground at 3 cannot walk there.
                        if 0 <= c[0] < m['w'] and 0 <= c[1] < m['h'] and (m['blocks'][c[1] * m['w'] + c[0]] >> 12) in (4, 15):
                            break
                        view.add(c)
                        if k < o.get('range', 4):
                            stops.add(c)  # where they may stop, beside the player
                # A trainer who sees the player walks up and stays where they
                # stop, on any block of their line: that must never close the
                # way on, alone or with another trainer whose line meets theirs.
                lines.append((o, view))
                for c in stops:
                    if c == end:
                        continue
                    if c in steps:
                        problems.append('%s: trainer at %d,%d may stop on the bridge step %d,%d'
                                        % (name, o['x'], o['y'], c[0], c[1]))
                    saved = set(grid.blocked)
                    grid.blocked.discard((o['x'], o['y']))
                    grid.blocked.add(c)
                    if grid.path(a, end) is None:
                        problems.append('%s: trainer at %d,%d would close the way after walking to %d,%d'
                                        % (name, o['x'], o['y'], c[0], c[1]))
                    grid.blocked = saved
                if o['kind'] == OBJ_TRAINER:
                    saved = set(grid.blocked)
                    grid.blocked |= view
                    cut = end in view or grid.path(a, end) is None
                    grid.blocked = saved
                    trainer_stats['all'] += 1
                    trainer_stats['turns'] += turn >= 0
                    trainer_stats['blocks' if cut else 'beside' if view & cells else 'off'] += 1
            # (Open sea is wide enough for swimmers' lines to cross.)
            for i, (o1, v1) in enumerate(lines if m['type'] != ROUTE_SEA else []):
                for o2, v2 in lines[i + 1:]:
                    if v1 & v2:
                        problems.append('%s: the lines of the trainers at %d,%d and %d,%d cross'
                                        % (name, o1['x'], o1['y'], o2['x'], o2['y']))
                if o['kind'] == OBJ_RIVAL:
                    saved = set(grid.blocked)
                    grid.blocked |= view | {(o['x'], o['y'])}
                    # (As the game sees it: the player steps into view on
                    # the way in, too.)
                    caught = (o['x'], o['y']) == end or end in view or a in view
                    # Only the way on along the main road matters.
                    if not caught and grid.path(a, end) is not None and m['rival']:
                        problems.append('%s: rival can be walked past' % name)
                    grid.blocked = saved
    steps = bridge_steps(m)
    for o in m['objs']:
        if o['kind'] in OBSTACLES:
            continue
        if (o['x'], o['y']) in steps:
            problems.append('%s: object %d stands on the bridge step %d,%d' % (name, o['kind'], o['x'], o['y']))
        # The Day Care's Pokemon play in their fenced yard.
        if o['kind'] == P.OBJ_FEATMON:
            continue
        if not touches((o['x'], o['y']), seen):
            problems.append('%s: object %d at %d,%d unreachable' % (name, o['kind'], o['x'], o['y']))
    return problems


# People and things in view from across a connection pop in on crossing,
# since the game shows only the objects of the map the player is on. Counts
# by kind of those standing in that view, and of all objects.
seam_stats = Counter()
seam_all = Counter()
OBJ_NAMES = {1: 'trainer', 2: 'item', 3: 'villager', 4: 'berry', 5: 'mon', 6: 'rival', 7: 'cut tree', 8: 'rock',
             9: 'boulder'}
# Share of people and things allowed in that view (Hoenn has about 7% near
# its connected edges; the generator keeps clear of it but for the odd map
# that runs out of room).
SEAM_VIEW_MAX = 0.02


def in_seam_view(m, x, y):
    """Seen from just across a connection: within about half a screen of
    an opening and as deep as the camera shows past the seam (5 blocks up or
    down, 7 across), or right by a connected edge (SeenFromNextDoor)."""
    for l in m['links']:
        d, p = l['dir'], l['pos']
        depth = {0: y, 2: m['h'] - 1 - y, 3: x, 1: m['w'] - 1 - x}[d]
        along = (x if d in (0, 2) else y) - (p & ~1)
        if depth <= 2:
            return True
        if d in (0, 2) and depth <= 4 and -7 <= along <= 8:
            return True
        if d in (1, 3) and depth <= 6 and -5 <= along <= 6:
            return True
    return False


def check_seam_view(m):
    problems = []
    if m['kind'] == 'cave':
        return problems
    for o in m['objs']:
        if o['kind'] not in OBJ_NAMES:
            continue
        what = OBJ_NAMES[o['kind']]
        seam_all[what] += 1
        if in_seam_view(m, o['x'], o['y']):
            seam_stats[what] += 1
            if o['kind'] in OBSTACLES:
                problems.append('map %d: %s at %d,%d in view across a seam' % (m['i'], what, o['x'], o['y']))
    return problems


def is_water(b):
    return (b & 0x0C00) == 0 and (b >> 12) == 1


ROUTE_BRIDGE = 12


def check_heights(m):
    """Ground (elevation 3) never meets a raised road or platform (4) where
    both can be walked: the game keeps a walker to their height there, and
    these checks would let them step across. (Stairs at elevation 0 join
    them.) On a bridge route, Mauville's pieces stay out of view across a
    connection, where the map next door draws this one with its own tileset
    (src/rogue_gen_bridge.h BridgeInView)."""
    problems = []
    # (A river's or pond's log bridge has its land ends at 4 beside ground
    # at 3 on purpose: nobody steps onto them from the side. Only a bridge
    # route's raised road is checked.)
    if m['kind'] != 'route' or m['type'] != ROUTE_BRIDGE:
        return problems
    w, h, bl = m['w'], m['h'], m['blocks']
    def walk(b):
        return (b & 0x0C00) == 0 and (b >> 12) in (0, 3, 4)
    for y in range(h):
        for x in range(w):
            b = bl[y * w + x]
            if not walk(b) or (b >> 12) not in (3, 4):
                continue
            for dx, dy in ((1, 0), (0, 1)):
                if x + dx >= w or y + dy >= h:
                    continue
                nb = bl[(y + dy) * w + x + dx]
                if walk(nb) and {b >> 12, nb >> 12} == {3, 4}:
                    problems.append('map %d: ground beside a raised road at %d,%d' % (m['i'], x, y))
    if m['kind'] == 'route' and m['type'] == ROUTE_BRIDGE:
        for l in m['links']:
            d, p = l['dir'], l['pos']
            box = {0: (p - 10, 0, p + 11, 6), 2: (p - 10, h - 7, p + 11, h - 1),
                   3: (0, p - 7, 8, p + 8), 1: (w - 9, p - 7, w - 1, p + 8)}[d]
            for y in range(max(0, box[1]), min(h, box[3] + 1)):
                for x in range(max(0, box[0]), min(w, box[2] + 1)):
                    if (bl[y * w + x] & 0x3FF) >= 0x200:
                        problems.append('map %d: bridge piece at %d,%d in view across a seam' % (m['i'], x, y))
                        break
    return problems


def main_walk(m, maps, grid):
    """Blocks of the shortest walk through a route: in from the lower
    numbered place, on to the higher one or the cave mouth; for a dead end,
    to the block furthest from the way in."""
    ents = entrances(m, maps)
    by = {x['i']: x for x in maps}
    a = end = None
    if m.get('side') and m.get('guardDir') in ents:
        a = ents[m['guardDir']]
    for l in m['links']:
        if by[l['target']].get('side') and not m.get('side'):
            continue
        if l['target'] < m['i'] and a is None:
            a = ents[l['dir']]
        elif ents[l['dir']] != a:
            end = ents[l['dir']]
    end = ents.get('mouth', end)
    if a is None:
        return None, None, []
    if end is None:
        dist = {a: 0}
        q = deque([a])
        while q:
            cur = q.popleft()
            for dx, dy in P.KEY_DELTA.values():
                n = (cur[0] + dx, cur[1] + dy)
                if n not in dist and grid.ok(*n) and grid.step(cur, n):
                    dist[n] = dist[cur] + 1
                    q.append(n)
        end = max(dist, key=dist.get)
    if not m.get('side'):
        # Past a block on the road ahead, once cleared.
        clear = P.Grid(m['blocks'], m['w'], ground_only=True)
        clear.walk = grid.walk
        clear.blocked = set(grid.blocked) - {(o['x'], o['y']) for o in m['objs'] if o['kind'] in OBSTACLES}
        grid = clear
    walk = grid.path(a, end)
    cells, cur = [a], a
    for key in walk or []:
        dx, dy = P.KEY_DELTA[key]
        cur = (cur[0] + dx, cur[1] + dy)
        cells.append(cur)
    return a, end, cells if walk is not None else []


victory_stats = Counter()


def check_victory_camp(m, maps):
    """On Victory Road the walk meets the rival first (the last gym town's
    Pokemon Center is behind), then passes the camper, then the trainers:
    nobody fights the rival and a trainer back to back without a chance to
    heal (PlaceVictoryCamp)."""
    if m['kind'] != 'route' or m.get('rival') != 4:
        return []
    name = 'map %d (Victory Road)' % m['i']
    grid = P.Grid(m['blocks'], m['w'], ground_only=True)
    grid.walk = m['grid'].walk
    grid.blocked = set(m['grid'].blocked)
    for o in m['objs']:
        if o['kind'] == OBJ_RIVAL:
            grid.blocked.discard((o['x'], o['y']))
    a, end, cells = main_walk(m, maps, grid)
    if not cells:
        return ['%s: no walk through' % name]
    step = {}
    for i, c in enumerate(cells):
        step.setdefault(c, i)

    def seen(o):
        turn = o.get('turn', -1)
        faces = {o['facing']} if turn < 0 else {0, 1, 2, 3} if turn == 4 else {o['facing'], turn}
        hits = []
        for f in faces:
            dx, dy = {0: (0, -1), 1: (1, 0), 2: (0, 1), 3: (-1, 0)}[f]
            for k in range(1, o.get('range', 4) + 1):
                c = (o['x'] + dx * k, o['y'] + dy * k)
                if not grid.ok(*c) and c not in step:
                    break
                if c in step:
                    hits.append(step[c])
        return min(hits) if hits else None
    rival = next((o for o in m['objs'] if o['kind'] == OBJ_RIVAL), None)
    camp = next((o for o in m['objs'] if o['kind'] == P.OBJ_CAMP), None)
    if rival is None or camp is None:
        return ['%s: rival or camper missing' % name]
    r = seen(rival)
    near = [step[c] for c in step if abs(c[0] - camp['x']) + abs(c[1] - camp['y']) == 1]
    c = min(near) if near else None
    ts = [s for s in (seen(o) for o in m['objs'] if o['kind'] == OBJ_TRAINER) if s is not None]
    problems = []
    if r is None:
        problems.append('%s: the walk does not meet the rival' % name)
    elif c is None or c <= r:
        problems.append('%s: the camper is not beside the walk past the rival' % name)
    elif ts and min(ts) <= c:
        problems.append('%s: a trainer at step %d before the camper at %d (rival at %d)' % (name, min(ts), c, r))
    else:
        victory_stats['roads'] += 1
        victory_stats['rival to camper'] += c - r
        victory_stats['camper to trainer'] += (min(ts) - c) if ts else 0
    return problems


def check_water(m, maps):
    """Ledge landings, sea routes that need Surf, rivers in view."""
    problems = []
    if m['kind'] != 'route':
        return problems
    name = 'map %d' % m['i']
    w, h, blocks = m['w'], m['h'], m['blocks']
    mb = behaviours(m['tileset'])
    spots = {}
    for o in m['objs']:
        if o['kind'] not in OBSTACLES:
            spots[(o['x'], o['y'])] = 'object %d' % o['kind']
    for sg in m.get('signs', []):
        spots[(sg['x'], sg['y'])] = 'sign'
    for (x, y), what in spots.items():
        for (nx, ny, sign) in ((x, y - 1, 1), (x, y + 1, -1), (x - 1, y, 1), (x + 1, y, -1)):
            if not (0 <= nx < w and 0 <= ny < h):
                continue
            mid = blocks[ny * w + nx] & 0x3FF
            d = MB_JUMP.get(mb[mid] if mid < len(mb) else None)
            # The ledge at (nx, ny) jumps toward d: it lands on (nx, ny) + d
            # and is jumped from (nx, ny) - d.
            if d and ((nx + d[0], ny + d[1]) == (x, y) or (nx - d[0], ny - d[1]) == (x, y)):
                water_stats['ledge'] += 1
                problems.append('%s: %s at %d,%d by the ledge at %d,%d' % (name, what, x, y, nx, ny))
                break
    if m['type'] == ROUTE_SEA and not m.get('side'):
        a, end, walk = main_walk(m, maps, m['grid'])
        if a and end and walk:
            water_stats['sea on foot'] += 1
            problems.append('%s: sea route crossed without Surf' % name)
    elif m['type'] == ROUTE_SEA:
        # v1.1 sea: a side route out to an island needs Surf for its finds.
        ents = entrances(m, maps)
        on_foot = reachable(m['grid'], list(ents.values()))
        for o in m['objs']:
            if o['kind'] == OBJ_ITEM and touches((o['x'], o['y']), on_foot):
                water_stats['island on foot'] += 1
                problems.append('%s: the island\'s item at %d,%d reached without Surf' % (name, o['x'], o['y']))
    if m['type'] == ROUTE_RIVER or (m['type'] == ROUTE_COAST and m.get('seaSide', -1) >= 0):
        a, end, walk = main_walk(m, maps, m['grid'])
        seen = any(is_water(blocks[yy * w + xx])
                   for (x, y) in walk for yy in range(max(0, y - 5), min(h, y + 6))
                   for xx in range(max(0, x - 7), min(w, x + 8)))
        if not seen:
            what = 'river' if m['type'] == ROUTE_RIVER else 'coast sea'
            water_stats[what + ' out of view'] += 1
            problems.append('%s: %s not in view of the walk' % (name, what))
    return problems


ISLAND_TIP, ISLAND_CROWN, ISLAND_FOOT = 0x239, 0x243, 0x23A


def check_island_trees(m):
    """Island trees (Dewford's tileset) with no tip or no trunk, tips with no
    tree under them, and trees standing alone. At the map's edges the trees
    run on into the border's crowns."""
    if m.get('tileset') != 'dewford':
        return []
    w, h, blocks = m['w'], m['h'], m['blocks']

    def at(x, y):
        return blocks[y * w + x] & 0x3FF if 0 <= x < w and 0 <= y < h else None
    trunk = {ISLAND_CROWN, ISLAND_FOOT}
    problems, tree = [], set()
    for y in range(h):
        for x in range(w):
            t = at(x, y)
            if t in trunk:
                tree.add((x, y))
                if y > 0 and at(x, y - 1) not in (ISLAND_TIP, ISLAND_CROWN):
                    problems.append('map %d: island tree with no top at %d,%d' % (m['i'], x, y))
                if t == ISLAND_CROWN and y < h - 1 and at(x, y + 1) not in trunk:
                    problems.append('map %d: island tree with no trunk at %d,%d' % (m['i'], x, y))
            elif t == ISLAND_TIP and y < h - 1 and at(x, y + 1) not in trunk:
                problems.append('map %d: island tree tip with no tree at %d,%d' % (m['i'], x, y))
            elif t == ISLAND_TIP:
                tree.add((x, y))
    for x, y in tree:
        if (x, y - 1) in tree:
            continue
        rows = range(y, next(yy for yy in range(y, h + 1) if (x, yy) not in tree))
        if not any((x + dx, yy) in tree for dx in (-1, 1) for yy in rows):
            problems.append('map %d: island tree standing alone at %d,%d' % (m['i'], x, y))
    return problems


# Lavaridge's red rock (and its spring's rim, plank walk and Fiery Path's
# mouth): every pair of blocks side by side or one over the other with one
# of them in it must occur in a Hoenn map drawn with Lavaridge's tileset.
# Grass and tall grass count as one.
RED_ROCK = set(range(0x256, 0x280)) | set(range(0x299, 0x29F)) | {0x2AE, 0x316, 0x31C, 0x317, 0x318, 0x319, 0x2DC, 0x2DD}
GRASS_LIKE = {0x001, 0x00D}
SLOT_BORDER = (0x1D4, 0x1D5, 0x1DC, 0x1DD)  # data/layouts/RogueSlot/border.bin
_red_pairs = []
red_stats = Counter()


def red_pairs():
    if not _red_pairs:
        import json
        hp, vp = set(), set()
        root = os.path.join(os.path.dirname(__file__), '..', '..')
        for l in json.load(open(os.path.join(root, 'data/layouts/layouts.json')))['layouts']:
            if not l.get('secondary_tileset', '').endswith('Lavaridge') or not l.get('primary_tileset', '').endswith('General'):
                continue
            data = open(os.path.join(root, l['blockdata_filepath']), 'rb').read()
            v = [t & 0x3FF for t in struct.unpack('<%dH' % (len(data) // 2), data)]
            v = [0x001 if t in GRASS_LIKE else t for t in v]
            w = l['width']
            for i, t in enumerate(v):
                if i % w + 1 < w:
                    hp.add((t, v[i + 1]))
                if i + w < len(v):
                    vp.add((t, v[i + w]))
        _red_pairs.extend((hp, vp))
    return _red_pairs


def check_red_rock(m):
    """Neighbours in and around Lavaridge's red rock that Hoenn never has."""
    if m.get('tileset') != 'lavaridge':
        return []
    if m['kind'] == 'town':
        red_stats['towns'] += 1
    hp, vp = red_pairs()
    w, h, blocks = m['w'], m['h'], m['blocks']
    linked = {l['dir'] for l in m['links']}

    def at(x, y):
        if 0 <= x < w and 0 <= y < h:
            t = blocks[y * w + x] & 0x3FF
            return 0x001 if t in GRASS_LIKE else t
        side = 0 if y < 0 else 2 if y >= h else 3 if x < 0 else 1
        return None if side in linked else SLOT_BORDER[(x & 1) + (y & 1) * 2]
    bad, seen = [], False
    # A hot spring town (src/rogue_gen_signatures.h) has Lavaridge's own top
    # left corner, cliffs, spring and all, at blocks 2-12: its pairs are
    # Lavaridge's, and its edges meet the town's trees and grass, as
    # Lavaridge's own map meets its border.
    corner = m.get('sigid') == 2 and m.get('sigBuilt')
    # (v1.1) The volcano's own mountain (gen_volcano_hill.py) runs on up
    # past the top of the map, where the border's trees stand over it: out
    # of view from anywhere the player can stand (its stair, three rows
    # below its doorway in row 6, is the highest ground), as Route 112's
    # and Jagged Pass's rock runs off their maps. Its ends are closed (its
    # caps: a west face beside round trees, an east face beside grass), so
    # every pair beside it is checked.
    mountain = any(k == P.WARP_DUNGEON and xy[1] == 6 for k, xy in m['warps'].items()) \
        and m['blocks'][6 * w + m['warps'][P.WARP_DUNGEON][0]] & 0x3FF == 0x25E
    for y in range(0 if mountain else -1, h):
        for x in range(-1, w):
            if corner and 1 <= x <= 12 and 1 <= y <= 12:
                continue
            a = at(x, y)
            for dx, dy, pairs in ((1, 0, hp), (0, 1, vp)):
                b = at(x + dx, y + dy)
                if a is None or b is None or (a not in RED_ROCK and b not in RED_ROCK):
                    continue
                seen = True
                red_stats['pairs'] += 1
                if (a, b) not in pairs:
                    bad.append((x, y, a, b, 'beside' if dx else 'over'))
    if seen and m['kind'] == 'town':
        red_stats['with rock'] += 1
    red_stats['bad'] += len(bad)
    if bad:
        x, y, a, b, how = bad[0]
        return ['map %d: %d pairs of red rock Hoenn never has, as %03X %s %03X at %d,%d' % (m['i'], len(bad), a, how, b, x, y)]
    return []


# Ordinary (round) trees of the general tileset, and the plain sand they
# must not stand on (sand's edge pieces have grass on their outer side).
ORDINARY_TREE = {0x1C6, 0x1C7, 0x1CE, 0x1CF, 0x1D4, 0x1D5, 0x1D6, 0x1D7,
                 0x1DC, 0x1DD, 0x1DE, 0x1DF, 0x1E4, 0x1E5, 0x1E6, 0x1E7}
ORDINARY_TIPS = {0x1C6, 0x1C7, 0x1CE, 0x1CF}
PLAIN_SAND = {0x124, 0x121}
ISLAND_TREE = {0x239, 0x243, 0x23A}
beach_stats = Counter()


truck_stats = {'towns': 0, 'clear': 0, 'old clear': 0, 'moved': 0, 'top over': []}

def check_beach_trees(m):
    """In a beach town (Dewford's tileset), no ordinary tree stands on bare
    sand (it would show a square of grass), and ordinary and island trees
    never touch."""
    if m.get('tileset') != 'dewford' or m['kind'] != 'town':
        return []
    w, h, blocks = m['w'], m['h'], m['blocks']
    problems = []
    sand, mixed = set(), set()
    for y in range(h):
        for x in range(w):
            if blocks[y * w + x] & 0x3FF not in ORDINARY_TREE:
                continue
            for dx, dy in ((0, -1), (1, 0), (0, 1), (-1, 0)):
                # (A tree's tips may meet a sand road above them, as in
                # Hoenn's towns.)
                if dy < 0 and blocks[y * w + x] & 0x3FF in ORDINARY_TIPS:
                    continue
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and 0 <= ny < h and blocks[ny * w + nx] & 0x3FF in PLAIN_SAND:
                    sand.add((x, y))
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < w and 0 <= ny < h and blocks[ny * w + nx] & 0x3FF in ISLAND_TREE:
                        mixed.add((x, y))
    beach_stats['towns'] += 1
    beach_stats['ordinary on sand'] += len(sand)
    beach_stats['kinds touching'] += len(mixed)
    if sand:
        x, y = min(sand)
        problems.append('map %d: %d ordinary tree blocks on sand, as at %d,%d' % (m['i'], len(sand), x, y))
    if mixed:
        x, y = min(mixed)
        problems.append('map %d: ordinary and island trees touch at %d,%d' % (m['i'], x, y))
    return problems


def check_sea_seams(maps):
    """Open water against land across a connection, and land walls between
    two sea routes."""
    problems = []
    by = {m['i']: m for m in maps}
    for f in seam_check.check_region(maps):
        if f['reason'] != 'water meets land':
            continue
        a, b = by[f['map']], by[f['other']]
        if a['kind'] != 'route' and b['kind'] != 'route':
            continue
        water_stats['water meets land'] += 1
        problems.append('map %d %s edge at %d,%d toward map %d: water meets land (%d blocks)'
                        % (f['map'], f['side'], f['x'], f['y'], f['other'], f['len']))
    for a in maps:
        if a['kind'] != 'route' or a['type'] != ROUTE_SEA:
            continue
        for l in a['links']:
            b = by[l['target']]
            if b['i'] < a['i'] or b['kind'] != 'route' or b['type'] != ROUTE_SEA:
                continue
            # Land on both sides away from the landing and from each map's
            # corners (where shores along the other sides come down to it).
            along = 0 if l['dir'] in (0, 2) else 1
            land = 0
            for pa, pb in seam_check.seam_pairs(a, b, l):
                if abs(pa[along] - l['pos']) <= 4:
                    continue
                if min(pa[along], (a['w'], a['h'])[along] - 1 - pa[along]) < 8:
                    continue
                if min(pb[along], (b['w'], b['h'])[along] - 1 - pb[along]) < 8:
                    continue
                if not is_water(a['blocks'][pa[1] * a['w'] + pa[0]]) and not is_water(b['blocks'][pb[1] * b['w'] + pb[0]]):
                    land += 1
            if land > 4:
                water_stats['sea wall'] += 1
                problems.append('maps %d and %d: %d blocks of land along a seam between sea routes'
                                % (a['i'], b['i'], land))
    return problems


def check_town_signs(maps):
    """Every town has its town sign, and the region one Name Rater (his
    house or a home with his sign)."""
    problems = []
    raters = 0
    for m in maps:
        if m['kind'] != 'town':
            continue
        kinds = [sg['kind'] for sg in m.get('signs', [])]
        if SIGN_TOWN not in kinds:
            problems.append('map %d: no town sign' % m['i'])
        raters += kinds.count(SIGN_NAME_RATER)
    if raters != 1:
        problems.append('%d Name Raters in the region' % raters)
    return problems


dungeon_stats = Counter()
# Trainers and items: route and dungeon trainers stay below the gym slots,
# item balls within their flags.
MAX_ROUTE_TRAINERS, MAX_ITEMS = 192, 160


BOSS_LEGEND = 4
# enum RogueLegend (include/rogue_gen.h)
LEGEND_NAMES = ['-', 'Regirock', 'Regice', 'Registeel', 'Regigigas', 'Articuno', 'Zapdos', 'Moltres',
                'Raikou', 'Entei', 'Suicune', 'Uxie', 'Mesprit', 'Azelf', 'Cobalion', 'Terrakion', 'Virizion',
                'Tornadus', 'Thundurus', 'Landorus', 'Latias', 'Latios', 'Heatran', 'Cresselia',
                'Mewtwo', 'Lugia', 'Ho-Oh', 'Kyogre', 'Groudon', 'Rayquaza']
FIRST_COVER = LEGEND_NAMES.index('Mewtwo')
DUN_KIND_NAMES = ['cave', 'victory', 'forest', 'tower', 'hideout', 'lair', 'ruins', 'ship', 'plant', 'ice', 'volcano', 'mirage']
legend_sets = Counter()


class RoomView:
    """A generated room floor, as gen_rooms.walk sees a room (with the
    room's switches, if it has any: New Mauville's)."""
    def __init__(self, f):
        self.blocks, self.w, self.h = f['blocks'], f['w'], f['h']
        self.mb = gen_rooms.behaviours(f['tileset'])
        room = P.room_library()[f['room']] if f.get('room', 255) != 255 else None
        if room is not None and getattr(room, 'states', None) and list(room.blocks) == list(self.blocks):
            self.states, self.triggers = room.states, room.triggers
        # A ruins floor's sand pits (cracked floor): no walk counts on them.
        self.avoid = frozenset((x, y) for y in range(self.h) for x in range(self.w)
                               if self.mb[self.at(x, y) & 0x3FF] == gen_rooms.CRACK_MB)

    def at(self, x, y):
        return self.blocks[y * self.w + x]

    def beh(self, x, y):
        return self.mb[self.at(x, y) & 0x3FF]

    def inside(self, x, y):
        return 0 <= x < self.w and 0 <= y < self.h


DUN_NAMES = {P.DUN_CAVE: 'cave', P.DUN_VICTORY: 'victory', P.DUN_FOREST: 'forest', P.DUN_TOWER: 'tower',
             P.DUN_HIDEOUT: 'hideout', P.DUN_RUINS: 'ruins', P.DUN_SHIP: 'ship', P.DUN_PLANT: 'plant', P.DUN_ICE: 'ice', P.DUN_VOLCANO: 'volcano',
             P.DUN_MIRAGE: 'mirage'}
EXTRA_NAMES = {P.OBJ_PRIZE: 'prize', P.OBJ_CHOICE: 'choice', P.OBJ_CAMP: 'camp', P.OBJ_EXPLORER: 'explorer'}


def check_extras_plan(d, maps):
    """The plan's extras follow PlanExtras' rules for the dungeon's kind."""
    problems = []
    ex = d.get('extras', {})
    if not ex or d['nItems'] == 0:
        return problems
    kind, name = d['kind'], 'dungeon %d (%s)' % (d['i'], DUN_NAMES.get(d['kind'], d['kind']))
    prize, choice, camp = ex['prize'] >= 0, ex['nChoice'] > 0, ex['camp'] >= 0
    door_master = kind in (P.DUN_CAVE, P.DUN_VICTORY, P.DUN_VOLCANO) and d['required'] and d['boss'] in (2, 3)
    if prize and (not d['boss'] or door_master):
        problems.append('%s: a prize with no boss on a floor to guard it' % name)
    if not prize and d['boss'] and not door_master and kind not in (P.DUN_TOWER, P.DUN_HIDEOUT):
        problems.append('%s: no prize beside the boss' % name)
    fossil = kind in (P.DUN_CAVE, P.DUN_VOLCANO) and d['host'] == next((m['i'] for m in maps if m['kind'] == 'cave' and not m['side']
                                                         and m['badges'] == 3 and m['nItems']), -1)
    if choice and not fossil and (d['required'] or kind in (P.DUN_VICTORY, P.DUN_HIDEOUT)):
        problems.append('%s: a choice room on the way on or in the hideout' % name)
    if choice and not (2 <= ex['nChoice'] <= 3 and 1 <= ex['choiceFloor'] <= d['floors']):
        problems.append('%s: choice room of %d balls on floor %d' % (name, ex['nChoice'], ex['choiceFloor']))
    deep = {P.DUN_CAVE: 3, P.DUN_FOREST: 2, P.DUN_TOWER: 4, P.DUN_ICE: 3, P.DUN_VOLCANO: 3}.get(kind)
    # (A legend's chamber is no floor to camp on.)
    floors = d['floors'] - (1 if d['boss'] == BOSS_LEGEND else 0)
    # A cave on the way on has a camp on its first floor, Victory Road on its deepest.
    way_camp = d['required'] and kind in (P.DUN_CAVE, P.DUN_VICTORY, P.DUN_VOLCANO)
    if way_camp and not (camp and ex['campFloor'] == (d['floors'] if kind == P.DUN_VICTORY else 1)):
        problems.append('%s: no camp on the first floor of a cave on the way on' % name)
    if camp and not way_camp and (d['required'] or deep is None or floors < deep):
        problems.append('%s: a camp in a dungeon of %d floors' % (name, floors))
    if not camp and not d['required'] and deep is not None and floors >= deep and kind != P.DUN_TOWER:
        problems.append('%s: no camp in a dungeon of %d floors' % (name, floors))
    if camp and not 1 <= ex['campFloor'] <= d['floors']:
        problems.append('%s: camp on floor %d' % (name, ex['campFloor']))
    explorer = ex.get('explorer', -1) >= 0
    if d['kind'] == P.DUN_SHIP and not ex.get('nKeys'):
        problems.append('%s: a shipwreck with no cabin keys' % name)
    if d['kind'] == P.DUN_PLANT and not ex.get('nDecoys'):
        problems.append('%s: a power plant with no Voltorb balls' % name)
    if ex['total'] != prize + ex['nChoice'] + camp + explorer + ex.get('nKeys', 0) + ex.get('nDecoys', 0):
        problems.append('%s: extras total %d' % (name, ex['total']))
    if explorer and (kind == P.DUN_HIDEOUT or not 2 <= ex['explorerFloor'] <= d['floors']):
        problems.append('%s: someone lost on floor %d' % (name, ex['explorerFloor']))
    if kind == P.DUN_RUINS and not (prize and choice and not camp and ex['choiceFloor'] == d['floors']):
        problems.append('%s: ruins without their prize and vault treasure' % name)
    return problems


def check_floor_extras(d, f, name):
    """The extras stand where the plan says, with their own item flags: the
    prize beside the boss, the choice room's balls in a row two steps apart,
    and nothing of theirs on a warp."""
    problems = []
    ex = d.get('extras', {})
    last = f['floor'] == d['floors']
    want = {P.OBJ_PRIZE: 1 if last and d['boss'] and ex.get('prize', -1) >= 0 else 0,
            P.OBJ_CHOICE: ex.get('nChoice', 0) if ex.get('choiceFloor') == f['floor'] else 0,
            P.OBJ_CAMP: 1 if ex.get('camp', -1) >= 0 and ex.get('campFloor') == f['floor'] else 0,
            P.OBJ_EXPLORER: 1 if ex.get('explorer', -1) >= 0 and ex.get('explorerFloor') == f['floor'] else 0}
    warps = set(f['warps'].values())
    for kind, n in want.items():
        got = [o for o in f['objs'] if o['kind'] == kind]
        if len(got) != n:
            problems.append('%s: %d objects of kind %d, want %d' % (name, len(got), kind, n))
        for o in got:
            if not d['itemBase'] + d['nItems'] - ex.get('total', 0) <= o['slot'] < d['itemBase'] + d['nItems']:
                problems.append('%s: extra at %d,%d has item flag %d' % (name, o['x'], o['y'], o['slot']))
            if (o['x'], o['y']) in warps:
                problems.append('%s: extra at %d,%d on a warp' % (name, o['x'], o['y']))
        dungeon_stats['extra %d' % kind] += len(got)
        dungeon_stats['%s %s' % (EXTRA_NAMES[kind], DUN_NAMES.get(d['kind'], d['kind']))] += len(got) and 1
    balls = sorted((o for o in f['objs'] if o['kind'] == P.OBJ_CHOICE), key=lambda o: o['x'])
    if balls and any(b['y'] != balls[0]['y'] or b['x'] != balls[0]['x'] + 2 * k for k, b in enumerate(balls)):
        problems.append('%s: the choice balls are not in a row' % name)
    if balls and sorted(b['slot'] for b in balls) != list(range(ex['choice'], ex['choice'] + ex['nChoice'])):
        problems.append('%s: choice balls have flags %s' % (name, [b['slot'] for b in balls]))
    boss = next((o for o in f['objs'] if o['kind'] == P.OBJ_BOSS), None)
    for o in f['objs']:
        # (A legend's chamber has its own place for it, near the legend
        # where the room allows.)
        if o['kind'] == P.OBJ_PRIZE and (boss is None or abs(o['x'] - boss['x']) > 2 or abs(o['y'] - boss['y']) > 2) \
           and not (d['boss'] == BOSS_LEGEND and f.get('room', 255) != 255):
            problems.append('%s: the prize at %d,%d is not beside the boss' % (name, o['x'], o['y']))
    return problems


DOOR_MB = {gen_rooms.MB['MB_ANIMATED_DOOR'], gen_rooms.MB['MB_NON_ANIMATED_DOOR']}


def check_ship_locks(d, f, view, warps, standing, name):
    """A shipwreck's locked cabins open: walking from the way in with the
    locked doors shut, every key in reach is picked up and opens its door,
    until all are open, as on the Abandoned Ship's hidden deck. Each lock's
    key lies on its floor, each key opens a lock, and the key balls are the
    dungeon's key flags."""
    problems = []
    ex = d.get('extras', {})
    locks = {sg['kind'] - P.SIGN_LOCK: (sg['x'], sg['y']) for sg in f.get('signs', [])
             if P.SIGN_LOCK <= sg['kind'] < P.SIGN_LOCK + 16}
    keys = {o['slot'] - ex.get('keys', 0): (o['x'], o['y']) for o in f['objs'] if o['kind'] == P.OBJ_KEY}
    if d['kind'] != P.DUN_SHIP:
        if locks or keys:
            problems.append('%s: locks or keys outside a shipwreck' % name)
        return problems
    if set(locks) != set(keys):
        problems.append('%s: locks %s but keys %s' % (name, sorted(locks), sorted(keys)))
    for k in keys:
        if not 0 <= k < ex.get('nKeys', 0):
            problems.append('%s: key ball %d is no key flag' % (name, k))
    doors = {(x, y) for x, y, _, _ in warps}
    for k, cell in locks.items():
        if cell not in doors:
            problems.append('%s: lock %d at %d,%d is on no door' % (name, k, *cell))
    got, opened = set(), set()
    while True:
        shut = {locks[k] for k in locks if k not in opened}
        # (A key ball picked up is gone.)
        gone = {keys[k] for k in got}
        seen = gen_rooms.walk(view, [warps[0][:2]], warps, (standing - gone) | shut)
        new = {k for k in keys if k not in got and gen_rooms.touches(keys[k], seen)}
        opening = {k for k in locks if k not in opened and (k in got or k in new) and gen_rooms.touches(locks[k], seen)}
        if not new and not opening:
            break
        got |= new
        opened |= opening
    if opened != set(locks):
        problems.append('%s: locks %s never open' % (name, sorted(set(locks) - opened)))
    dungeon_stats['ship locks'] += len(locks)
    return problems


def check_room_floor(d, f, floors, host, name):
    """A tower's or hideout's floor, built from a vanilla room: its warps
    are the room's on warp metatiles, each leads to a warp that exists, and
    from the way in (and from every warp the player arrives by) every warp,
    trainer, item and the boss can be reached past the trainers standing
    there, warp pads included."""
    problems = []
    room = P.room_library()[f['room']]
    view = RoomView(f)
    warps = [(x, y, role, pair) for x, y, role, pair in room.warps]
    if sorted(f['warps']) != list(range(len(warps))) or any(f['warps'][i] != warps[i][:2] for i in range(len(warps))):
        problems.append('%s: warps differ from room %s' % (name, room.name))
        return problems
    for i, (x, y, role, _) in enumerate(warps):
        block = view.at(x, y)
        if role == 'LAND':
            # Where a fall from the floor above lands: open floor, never a
            # pit, reached on foot (below).
            if view.mb[block & 0x3FF] in gen_rooms.WARP_MB | {gen_rooms.CRACK_MB} or block & 0x0C00:
                problems.append('%s: landing %d at %d,%d is not open floor' % (name, i, x, y))
            continue
        # (A ship's doors are solid, walked into as Hoenn's are.)
        if view.mb[block & 0x3FF] not in gen_rooms.WARP_MB or (block & 0x0C00 and view.mb[block & 0x3FF] not in DOOR_MB):
            problems.append('%s: warp %d at %d,%d is not a warp metatile' % (name, i, x, y))
    by_floor = {g['floor']: g for g in floors}
    for i, (to_floor, to_warp) in f.get('dests', {}).items():
        if to_floor == 0:
            if f['floor'] != 1 or (P.CAVE_DOWN if host['kind'] == 'cave' else P.WARP_DUNGEON) not in host['warps']:
                problems.append('%s: warp %d leads out from floor %d' % (name, i, f['floor']))
        elif to_floor not in by_floor or to_warp not in by_floor[to_floor]['warps']:
            problems.append('%s: warp %d leads to floor %d warp %d, which is not there' % (name, i, to_floor, to_warp))
        elif abs(to_floor - f['floor']) > 1:
            problems.append('%s: warp %d skips to floor %d' % (name, i, to_floor))
    last = f['floor'] == d['floors']
    if not last and not any(t == f['floor'] + 1 for t, _ in f.get('dests', {}).values()):
        problems.append('%s: no way on' % name)
    # Everyone and everything on the floor stands in the way (item balls,
    # the extras too).
    # Everything stands in the way but the villain leader, who leaves once beaten.
    # (A legend's seal opens: check_legend_gate checks the way back past it.)
    standing = {(o['x'], o['y']) for o in f['objs'] if o['kind'] not in (P.OBJ_VILLAIN_LEADER, P.OBJ_SEAL)}
    # A ball on one of New Mauville's switches is taken (or fought) first.
    switches = set(getattr(view, 'triggers', {}))
    standing -= {(o['x'], o['y']) for o in f['objs'] if o['kind'] in (OBJ_ITEM, P.OBJ_DECOY) and (o['x'], o['y']) in switches}
    if switches and any((o['x'], o['y']) in switches for o in f['objs'] if o['kind'] not in (OBJ_ITEM, P.OBJ_DECOY)):
        problems.append('%s: someone stands on a switch' % name)
    problems += check_ship_locks(d, f, view, warps, standing, name)
    for o in f['objs']:
        if (o['x'], o['y']) in view.avoid:
            problems.append('%s: object kind %d stands on a sand pit at %d,%d' % (name, o['kind'], o['x'], o['y']))
    seen = gen_rooms.walk(view, [warps[0][:2]], warps, standing)
    for i, (x, y, _, _) in enumerate(warps):
        if not gen_rooms.touches((x, y), seen):
            problems.append('%s: warp %d cannot be reached' % (name, i))
    for o in f['objs']:
        if not gen_rooms.touches((o['x'], o['y']), seen):
            problems.append('%s: object kind %d at %d,%d cannot be reached' % (name, o['kind'], o['x'], o['y']))
    # Arriving by any warp, the way back is open.
    arrivals = {w for g in floors for (t, w) in g.get('dests', {}).values() if t == f['floor']}
    for i in arrivals:
        back = gen_rooms.walk(view, [warps[i][:2]], warps, standing)
        if not gen_rooms.touches(warps[0][:2], back):
            problems.append('%s: arriving by warp %d there is no way back' % (name, i))
    bosses = [o for o in f['objs'] if o['kind'] == P.OBJ_BOSS]
    if last and d['boss'] and len(bosses) != 1:
        problems.append('%s: %d bosses' % (name, len(bosses)))
    # A guardian or legend boss stands there too, but is not a trainer.
    want_t = f['nTrainers'] + (1 if last and d['boss'] and d['boss'] not in (2, 3) else 0)
    if sum(1 for o in f['objs'] if o['kind'] in (OBJ_TRAINER, P.OBJ_BOSS)) != want_t:
        problems.append('%s: %d trainers placed of %d' % (name, sum(1 for o in f['objs'] if o['kind'] in (OBJ_TRAINER, P.OBJ_BOSS)), want_t))
    if sum(1 for o in f['objs'] if o['kind'] == OBJ_ITEM) != f['nItems']:
        problems.append('%s: items placed short' % name)
    dungeon_stats['trainers'] += sum(1 for o in f['objs'] if o['kind'] in (OBJ_TRAINER, P.OBJ_BOSS))
    dungeon_stats['items'] += sum(1 for o in f['objs'] if o['kind'] == OBJ_ITEM)
    dungeon_stats['room floors'] += 1
    return problems


def floor_dest(d, f, kind):
    """Where warp kind of a floor leads, as FloorWarpDest (src/rogue.c) sends
    it: (floor, warp) with floor 0 for the host, or ('exit', warp)."""
    if f.get('room', 255) != 255:
        return f.get('dests', {}).get(kind)
    if kind >= P.FLOOR_WARPS:
        dest = floor_dest(d, f, kind - P.FLOOR_WARPS)
        return dest and (dest[0], dest[1] + P.FLOOR_WARPS)
    if kind == P.FLOOR_EXIT:
        return ('exit', P.WARP_DUNGEON_2)
    if kind in (P.FLOOR_UP, P.FLOOR_UP_2) and f['floor'] == 1:
        return (0, kind)
    partner = P.ladder_partner(f['floor'], kind)
    return partner


def warp_groups(cells):
    """The warps of a map grouped into ways: warp blocks side by side (a
    double door, a wide stair) are one way. {kind: group number}."""
    group, n = {}, 0
    for kind in sorted(cells):
        if kind in group:
            continue
        group[kind] = n
        todo = [kind]
        while todo:
            k = todo.pop()
            x, y = cells[k]
            for other, (ox, oy) in cells.items():
                if other not in group and abs(ox - x) + abs(oy - y) == 1:
                    group[other] = n
                    todo.append(other)
        n += 1
    return group


def check_floor_exits(d, f, floors, name):
    """Each way off a floor leads somewhere of its own: no two doors or
    stairs that lead to the same place (Mt. Pyre's back door beside its
    front door), and none that leads back onto its own floor but a warp
    pad to its partner."""
    problems = []
    by_floor = {g['floor']: g for g in floors}
    mine = warp_groups(f['warps'])
    seen = {}
    for kind in sorted(f['warps']):
        dest = floor_dest(d, f, kind)
        if dest is None:
            continue
        to, warp = dest
        if to == f['floor'] and warp == kind:
            continue  # a ruins floor's landing spot: no way anywhere
        if to == f['floor']:
            # A warp pad: to another pad of the room, not one beside it.
            if warp not in f['warps'] or mine[warp] == mine[kind]:
                problems.append('%s: warp %d is a pad that leads nowhere else' % (name, kind))
            continue
        if to == 0 or to == 'exit':
            where = (to, warp)
        elif to in by_floor and warp in by_floor[to]['warps']:
            where = (to, warp_groups(by_floor[to]['warps'])[warp])
        else:
            continue  # (a warp that leads nowhere is reported elsewhere)
        if where in seen and seen[where] != mine[kind]:
            problems.append('%s: warps %d and %d are two ways to the same place' % (name, min(
                k for k in f['warps'] if mine[k] == seen[where]), kind))
        seen.setdefault(where, mine[kind])
    dungeon_stats['floor exits'] += len(set(mine.values()))
    return problems


def legend_set(legend):
    """The trio a legend belongs to (Regigigas with the Regis), as
    LegendSet in src/rogue_gen_legend.h."""
    names = LEGEND_NAMES
    trios = [('Regirock', 'Regice', 'Registeel', 'Regigigas'), ('Articuno', 'Zapdos', 'Moltres'),
             ('Raikou', 'Entei', 'Suicune'), ('Uxie', 'Mesprit', 'Azelf'), ('Cobalion', 'Terrakion', 'Virizion'),
             ('Tornadus', 'Thundurus', 'Landorus')]
    return next((k for k, t in enumerate(trios) if names[legend] in t), None)


def check_legend_gate(d, mine, maps, dungeons):
    """A legend's dungeon ends in its chamber, a vanilla legend's room, past
    floors of cave or woods; what keeps the chamber shut stands at the room's
    narrow place (no walk to the legend passes it by) or where the legend
    appears; the way back out stays open with it there; and the way it opens
    can be had before it, from outside it (no loops): the field move's gym,
    the gym that gives the key, a prize ball in a dungeon without a legend,
    the villain leader after a hideout, or the rest of its set, all with
    gates of their own."""
    problems = []
    name = 'dungeon %d (%s)' % (d['i'], LEGEND_NAMES[d['legend']])
    kind, src = d['gate'], d['gateSrc']
    dungeon_stats['legend gate %s' % P.GATE_KIND_NAMES[kind]] += 1
    dungeon_stats['legend opens at %d' % d['gateBadges']] += 1
    if not 1 <= d['gateBadges'] <= 8:
        problems.append('%s: opens at %d badges' % (name, d['gateBadges']))
    if not mine or mine[-1].get('room', 255) == 255 or P.room_library()[mine[-1]['room']].kind != 'lair':
        return problems + ['%s: does not end in a chamber' % name]
    if any(f.get('room', 255) != 255 for f in mine[:-1]):
        problems.append('%s: a room before the chamber' % name)
    if len(mine) < 2:
        problems.append('%s: no dungeon before the chamber' % name)
    last = mine[-1]
    seals = [o for o in last['objs'] if o['kind'] == P.OBJ_SEAL]
    boss = next((o for o in last['objs'] if o['kind'] == P.OBJ_BOSS), None)
    if boss is None:
        return problems + ['%s: no legend in the chamber' % name]
    if d['sealAtLegend'] < 0:
        if seals:
            problems.append('%s: a seal on an open way' % name)
    elif len(seals) != 1:
        problems.append('%s: %d seals' % (name, len(seals)))
    elif d['sealAtLegend'] == 1:
        if (seals[0]['x'], seals[0]['y']) != (boss['x'], boss['y']):
            problems.append('%s: the seal is not where the legend appears' % name)
    else:
        room = P.room_library()[last['room']]
        view = RoomView(last)
        warps = [(x, y, role, pair) for x, y, role, pair in room.warps]
        seal = (seals[0]['x'], seals[0]['y'])
        goal = {(boss['x'] + dx, boss['y'] + dy) for dx, dy in gen_rooms.DELTA}
        shut = gen_rooms.walk(view, [warps[0][:2]], warps, {seal, (boss['x'], boss['y'])})
        if goal & shut and kind != P.GATE_VILLAIN_LEADER:
            problems.append('%s: the legend can be reached past the seal' % name)
        if not gen_rooms.touches(seal, shut):
            problems.append('%s: the seal cannot be reached' % name)
        dungeon_stats['seals at a narrow place'] += 1
    by = {x['i']: x for x in dungeons}
    gyms = maps[0]['gyms']
    if kind == P.GATE_FIELD:
        if src not in (2, 3) or not any(g['key'] == src for g in gyms):
            problems.append('%s: field move %d no gym gives' % (name, src))
    elif kind == P.GATE_RITUAL:
        if src not in (0, 1, 2):
            problems.append('%s: rite %d' % (name, src))
    elif kind == P.GATE_GIFT:
        if not 0 <= src < 8:
            problems.append('%s: key from gym %d' % (name, src))
        dungeon_stats['key from gym %d' % (src + 1)] += 1
    elif kind == P.GATE_FOUND:
        s = by.get(src)
        if s is None or s['boss'] == BOSS_LEGEND or s['kind'] == P.DUN_HIDEOUT or s['nItems'] == 0 \
           or s.get('extras', {}).get('prize', -1) < 0:
            problems.append('%s: key in dungeon %d, which cannot hold it' % (name, src))
        else:
            prize = s['extras']['prize']
            floors = [f for f in maps[0]['floors'] if f['dungeon'] == src]
            if not any(o['kind'] == P.OBJ_PRIZE and o['slot'] == prize for f in floors for o in f['objs']):
                problems.append('%s: the key\'s ball is not in dungeon %d' % (name, src))
            if sum(1 for x in dungeons if x['boss'] == BOSS_LEGEND and x['gate'] == P.GATE_FOUND and x['gateSrc'] == src) > 1:
                problems.append('%s: two keys in dungeon %d' % (name, src))
    elif kind == P.GATE_VILLAIN_LEADER:
        if P.hideout(maps) is None or not d.get('leader'):
            problems.append('%s: waits behind a villain leader the region lacks' % name)
    elif kind == P.GATE_AFTER:
        others = [x for x in dungeons if x['boss'] == BOSS_LEGEND and x['i'] != d['i']
                  and legend_set(x['legend']) == legend_set(d['legend'])]
        if len(others) < 2 or any(x['gate'] == P.GATE_AFTER for x in others):
            problems.append('%s: waits for %s' % (name, [LEGEND_NAMES[x['legend']] for x in others]))
    return problems


def check_floor_wild(d, f, host, name):
    """A floor's wild Pokémon: none in the villains' hideout, a legend's
    chamber and the ruins' vault (as the Desert Ruins), and every other floor (tower rooms too, as Mt. Pyre's) has its
    own table (RogueGen_FloorWildRate, RogueDungeon_FloorWild) at the floor's
    level, which climbs from the dungeon's and stays near its host's. A floor
    whose game rate and table disagree met the slot map's placeholder
    (Poochyena at level 2) in v0.10."""
    problems = []
    chamber = d['boss'] == BOSS_LEGEND and d['kind'] in (P.DUN_LAIR, P.DUN_CAVE, P.DUN_FOREST, P.DUN_ICE, P.DUN_VOLCANO) \
        and f['floor'] == d['floors']
    vault = d['kind'] == P.DUN_RUINS and f['floor'] == d['floors']
    want_none = d['kind'] == P.DUN_HIDEOUT or chamber or vault
    rate = f.get('wild', -1)
    if want_none and rate != 0:
        problems.append('%s: wild rate %d where none should live' % (name, rate))
    if not want_none and not 4 <= rate <= 20:
        problems.append('%s: wild rate %d' % (name, rate))
    if chamber and f.get('room', 255) == 255:
        problems.append('%s: a legend\'s last floor that is no chamber' % name)
    if not want_none:
        # The table's level (RogueDungeon_FloorWild, RogueGen_WildLevel: a few
        # under the floor's, more late).
        wild = f['wildLevel']
        if not f['level'] - 12 <= wild <= f['level'] - 2 and f['level'] > 4:
            problems.append('%s: wild level %d on a floor of level %d' % (name, wild, f['level']))
        if not d['level'] <= f['level'] <= d['level'] + 2 * (f['floor'] - 1):
            problems.append('%s: level %d, the dungeon\'s is %d' % (name, f['level'], d['level']))
        if wild < 2 or abs(f['level'] - host['level']) > 12:
            problems.append('%s: wild level %d by a host of level %d' % (name, wild, host['level']))
        dungeon_stats['wild floors'] += 1
    return problems


ice_stats = Counter()
ice_moves_stats = Counter()  # (floor, 2 for the floors below; least moves on the ice)
ice_prize_stats = Counter()  # moves on the ice to reach a find out on it
STRENGTH = 3  # enum RogueObstacle: ROBST_STRENGTH


def ice_moves(grid, cell):
    """The stands one move (a step, or a slide across ice) from cell."""
    out = []
    for dx, dy in P.KEY_DELTA.values():
        n = (cell[0] + dx, cell[1] + dy)
        if not grid.ok(*n):
            continue
        out.append(grid.slide(n, dx, dy) if n in grid.ice else n)
    return out


def ice_room(f, grid, c):
    """Ice, or a patch of snow on it (B_ICE_SNOW, 0x28F): the puzzle's cells."""
    return c in grid.ice or f['blocks'][c[1] * f['w'] + c[0]] & 0x3FF == 0x28F


def ice_way(f, grid, start):
    """The least moves on the ice from start to every stand: each slide, or
    step onto or off the ice, is one; walking is free."""
    dist, dq = {start: 0}, deque([start])
    while dq:
        c = dq.popleft()
        for dx, dy in P.KEY_DELTA.values():
            n = (c[0] + dx, c[1] + dy)
            if not grid.ok(*n):
                continue
            t = grid.slide(n, dx, dy) if n in grid.ice else n
            cost = 1 if ice_room(f, grid, c) or ice_room(f, grid, n) else 0
            if dist[c] + cost < dist.get(t, 1 << 20):
                dist[t] = dist[c] + cost
                (dq.append if cost else dq.appendleft)(t)
    return dist


def ice_stuck(grid, start):
    """Stands reached from start that never lead back to it."""
    seen = reachable(grid, [start])
    back = {}
    for c in seen:
        for n in ice_moves(grid, c):
            back.setdefault(n, []).append(c)
    home, stack = {start}, [start]
    while stack:
        c = stack.pop()
        for p in back.get(c, []):
            if p not in home:
                home.add(p)
                stack.append(p)
    return [c for c in seen if c not in home]


def check_ice(d, f, grid, name):
    """An ice cave floor (RDUN_ICE): its sliding puzzle, slide by slide as
    Emerald's MB_ICE moves the player. From the way in every ladder and
    object can be reached (the general check does that with sliding); here,
    nowhere the player can slide to leaves them stuck: from every stand
    reached from the way in, the way in can be reached again. A find out on
    the ice is reached, and nobody is stuck once it is taken. And the
    puzzle's moves: the least moves on the ice to the furthest of the ways
    on and the boss (0 where the ice can be walked round), at least two
    where it must be crossed."""
    problems = []
    if not grid.ice:
        ice_stats['floors without ice'] += 1
        return problems
    ice_stats['floors with ice'] += 1
    up = f['warps'][P.FLOOR_UP]
    start = (up[0], up[1] + 1)
    stuck = ice_stuck(grid, start)
    if stuck:
        problems.append('%s: stuck on the ice at %s' % (name, sorted(stuck)[:3]))
    dist = ice_way(f, grid, start)
    goals = [c for k, c in f['warps'].items() if k != P.FLOOR_UP] + [(o['x'], o['y']) for o in f['objs'] if o['kind'] == P.OBJ_BOSS]
    moves = max((min(dist.get((g[0] + dx, g[1] + dy), 1 << 20) for dx, dy in [(0, 0)] + list(P.KEY_DELTA.values()))
                 for g in goals), default=0)
    if moves >= 1 << 20:
        problems.append('%s: the way on cannot be reached across the ice' % name)
    elif moves == 1:
        problems.append('%s: the ice is crossed in 1 move' % name)
    else:
        ice_moves_stats[(min(f['floor'], 2), moves)] += 1
    for o in f['objs']:
        c = (o['x'], o['y'])
        if o['kind'] != OBJ_ITEM or not ice_room(f, grid, c):
            continue
        near = min(dist.get((c[0] + dx, c[1] + dy), 1 << 20) for dx, dy in P.KEY_DELTA.values())
        grid.blocked.discard(c)
        after = ice_stuck(grid, start)
        grid.blocked.add(c)
        if near >= 1 << 20 or after:
            problems.append('%s: the find on the ice at %s reached in %s, stuck after taking it at %s'
                            % (name, c, near, sorted(after)[:3]))
        else:
            ice_prize_stats[near] += 1
    return problems


def keys_by(maps, badges):
    """The obstacles (enum RogueObstacle) the first badges gyms' badges clear."""
    gyms = maps[0].get('gyms', [])
    return {g['key'] for g in gyms if g['n'] < badges}


def check_volcano_place(maps, dungeons):
    """The volcano (RDUN_VOLCANO) only where the region map draws its crater,
    entered from a place beside it (two cells at most), one at most. A town's
    door into it is in the rock along the top of the town, so the crater is
    drawn north of that town (MapCraterByDoor), never beside or below it;
    so is the foot route's (v1.1), which is drawn with Lavaridge's tileset."""
    problems = []
    tr = maps[0].get('traits') or {}
    vx, vy = tr.get('volcano', (-1, -1))
    vols = [d for d in dungeons if d['kind'] == P.DUN_VOLCANO]
    ice_stats['volcano regions'] += bool(vols)
    ice_stats['crater regions'] += vx >= 0
    ice_stats['ice regions'] += any(d['kind'] == P.DUN_ICE for d in dungeons)
    if len(vols) > 1:
        problems.append('%d volcanoes' % len(vols))
    for d in vols:
        host = maps[d['host']]
        gx, gy = host['grid_pos']
        if vx < 0:
            problems.append('dungeon %d: a volcano with no crater on the map' % d['i'])
        elif max(abs(gx - vx), abs(gy - vy)) > 2:
            problems.append('dungeon %d: the volcano is entered %d,%d from the crater' % (d['i'], gx - vx, gy - vy))
        elif host['kind'] in ('town', 'route') and vy >= gy:
            problems.append('dungeon %d: the door into the volcano is at the top of %s, the crater %d,%d off'
                            % (d['i'], host['name'], vx - gx, vy - gy))
        # (v1.1) A foot route is drawn with Lavaridge's tileset, for the
        # volcano's own mountain along its top.
        if host['kind'] == 'route' and host['tileset'] != 'lavaridge':
            problems.append('dungeon %d: the volcano\'s foot route is drawn with %s' % (d['i'], host['tileset']))
        ice_stats['volcano by %s' % host['kind']] += 1
        if host['kind'] != 'cave' and host['tileset'] == 'lavaridge':
            ice_stats['volcano by its own mountain'] += 1
        if host['kind'] == 'town' and tr.get('planned', (vx, vy)) != (vx, vy):
            ice_stats['crater moved over its town'] += 1
    for d in dungeons:
        if d['kind'] == P.DUN_ICE and d['badges'] < 4:
            problems.append('dungeon %d: an ice cave at %d badges' % (d['i'], d['badges']))
    return problems


def exit_floor(maps, d):
    """The floor whose way out leads to the dungeon's exit place
    (RogueGen_ExitFloor): the tunnel's, else the last."""
    t = maps[0].get('tunnel')
    return t['floor'] if t and t['dungeon'] == d['i'] else d['floors']


def is_tunnel_floor(maps, d, f):
    t = maps[0].get('tunnel')
    return bool(t) and t['dungeon'] == d['i'] and f['floor'] == t['floor']


tunnel_stats = Counter()


def check_tunnel_floor(maps, d, f, name):
    """A tunnel's exit floor: its way out (RFLOOR_EXIT) lies past the
    barrier, a pair of rocks or boulders, or someone beside a rock, that
    alone keeps it from the rest of the floor. Each side can be walked up to
    the barrier, and with it cleared every warp is reached from the way in."""
    problems = []
    t = maps[0]['tunnel']
    barrier = [o for o in f['objs'] if o['kind'] in OBSTACLES | {P.OBJ_TUNNEL_GUARD}]
    want = {P.GATE_FIELD: 2}.get(t['gate'], 1)
    if len(barrier) != want:
        return ['%s: tunnel barrier of %d objects, wanted %d' % (name, len(barrier), want)]
    kinds = {o['kind'] for o in barrier}
    expect = {P.OBST_ROCK_SMASH: OBJ_ROCK, P.OBST_STRENGTH: OBJ_BOULDER, P.OBST_CUT: OBJ_CUT}.get(t['src']) \
        if t['gate'] == P.GATE_FIELD else P.OBJ_TUNNEL_GUARD
    if kinds != {expect}:
        problems.append('%s: tunnel barrier %s, wanted %s' % (name, sorted(kinds), expect))
    if P.FLOOR_EXIT not in f['warps'] or P.FLOOR_UP not in f['warps']:
        return problems + ['%s: tunnel floor without its ways' % name]
    def side(start, cleared):
        grid = P.Grid(f['blocks'], f['w'], ground_only=True)
        grid.ice = f['grid'].ice
        grid.blocked = set(f['grid'].blocked) - {(o['x'], o['y']) for o in barrier}
        if not cleared:
            grid.blocked |= {(o['x'], o['y']) for o in barrier}
        x, y = f['warps'][start]
        return reachable(grid, [(x, y)] + [(x + dx, y + dy) for dx, dy in P.KEY_DELTA.values()])
    near, far, open_ = side(P.FLOOR_UP, False), side(P.FLOOR_EXIT, False), side(P.FLOOR_UP, True)
    if touches(f['warps'][P.FLOOR_EXIT], near) or f['warps'][P.FLOOR_EXIT] in near:
        problems.append('%s: the tunnel\'s way out can be reached past its barrier' % name)
    if not touches(f['warps'][P.FLOOR_EXIT], open_) and f['warps'][P.FLOOR_EXIT] not in open_:
        problems.append('%s: the tunnel\'s way out cannot be reached with its barrier cleared' % name)
    if not any(touches((o['x'], o['y']), near) for o in barrier) or not any(touches((o['x'], o['y']), far) for o in barrier):
        problems.append('%s: the tunnel\'s barrier cannot be reached from both sides' % name)
    for kind in f['warps']:
        if kind != P.FLOOR_EXIT and not touches(f['warps'][kind], near) and f['warps'][kind] not in near:
            problems.append('%s: warp %d lies past the tunnel\'s barrier' % (name, kind))
    tunnel_stats['floors'] += 1
    return problems


def check_dungeons(maps):
    """Every dungeon's floors hang together: each floor's ladders stand on
    ladder metatiles, every ladder, trainer and item of a floor can be
    reached from one of its ladders (with boulders and cracked rocks
    cleared), each dungeon has one boss, and the host has the way in. A
    dungeon on the way on (required) can be crossed from the cave's way in
    to its way out across the floors, and only that way: the cave's two
    parts do not join. Boulders and rocks only stand where the badges held
    when the dungeon opens clear them."""
    problems = []
    dungeons, floors = maps[0].get('dungeons', []), maps[0].get('floors', [])
    by = {m['i']: m for m in maps}
    trainers = max([m.get('trainerBase', 0) + m.get('nTrainers', 0) for m in maps]
                   + [d['trainerBase'] + d['nTrainers'] for d in dungeons] + [0])
    items = max([m.get('itemBase', 0) + m.get('nItems', 0) for m in maps]
                + [d['itemBase'] + d['nItems'] for d in dungeons] + [0])
    if trainers > MAX_ROUTE_TRAINERS:
        problems.append('%d route and dungeon trainers' % trainers)
    if items > MAX_ITEMS:
        problems.append('%d item balls' % items)
    dungeon_stats['slots'] += trainers
    areas = [d['area'] for d in dungeons if d['area'] >= 0]
    if max(areas + [0]) >= 48:
        problems.append('dungeon area %d' % max(areas))
    dungeon_stats['regions'] += 1
    mb = behaviours('cave')
    for d in dungeons:
        dungeon_stats['dungeons'] += 1
        dungeon_stats['kind %d' % d['kind']] += 1
        dungeon_stats['floors'] += d['floors']
        host = by[d['host']]
        if host['kind'] == 'cave' and P.CAVE_DOWN not in host['warps']:
            problems.append('dungeon %d: cave %d has no ladder down' % (d['i'], d['host']))
        mine = sorted((f for f in floors if f['dungeon'] == d['i']), key=lambda f: f['floor'])
        if len(mine) != d['floors']:
            problems.append('dungeon %d: %d floors of %d' % (d['i'], len(mine), d['floors']))
        for f in mine:
            both = boss_sight_shared(f, d['boss'] in (2, P.BOSS_ADMIN))  # a master or an admin
            if both:
                problems.append('dungeon %d floor %d: %d,%d is seen by the boss and another trainer at once'
                                % (d['i'], f['floor'], both[0], both[1]))
        bosses = [o for o in host['objs'] if o['kind'] == P.OBJ_BOSS]
        if d['kind'] == P.DUN_FOREST:
            dungeon_stats['forests'] += 1
            dungeon_stats['forests across the road'] += d['exit'] == d['host']
        if host['kind'] == 'town':
            # A town's tower or hideout: a door into it, and a sign naming it
            # that can be read.
            if P.WARP_DUNGEON not in host['warps']:
                problems.append('dungeon %d: town %d has no way in' % (d['i'], d['host']))
            elif d['kind'] == P.DUN_TOWER:
                # A tower is entered by a door in a building, never a cave
                # mouth in the rock (B_MOUTH).
                x, y = host['warps'][P.WARP_DUNGEON]
                door = host['blocks'][y * host['w'] + x] & 0x3FF
                if door == 0x0A7:
                    problems.append('dungeon %d: the tower in town %d is entered by a cave mouth' % (d['i'], d['host']))
                dungeon_stats['tower door %03X' % door] += 1
            signs = [sg for sg in host.get('signs', []) if sg['kind'] == SIGN_DUNGEON]
            if not signs:
                problems.append('dungeon %d: town %d has no sign for it' % (d['i'], d['host']))
            else:
                dungeon_stats['signed'] += 1
        for f in mine:
            name = 'dungeon %d floor %d' % (d['i'], f['floor'])
            problems += check_floor_wild(d, f, host, name)
            problems += check_floor_exits(d, f, mine, name)
            if f.get('room', 255) != 255:
                problems += check_room_floor(d, f, mine, host, name)
                problems += check_floor_extras(d, f, name)
                bosses += [o for o in f['objs'] if o['kind'] == P.OBJ_BOSS]
                continue
            mb = behaviours(f['tileset'])
            for kind, (x, y) in f['warps'].items():
                block = f['blocks'][y * f['w'] + x]
                if d['kind'] == P.DUN_FOREST:
                    # Petalburg Woods' ends: arrow warps across a lane out to
                    # the edge, walked onto from inside.
                    b = mb[block & 0x3FF]
                    inside = y - 1 if b == MB_SOUTH_ARROW_WARP else y + 1
                    if b not in (MB_SOUTH_ARROW_WARP, MB_NORTH_ARROW_WARP) or (block >> 10) & 3                             or not f['grid'].ok(x, inside):
                        problems.append('%s: warp %d at %d,%d is no way out of the woods' % (name, kind, x, y))
                    if kind >= P.FLOOR_WARPS and f['warps'].get(kind - P.FLOOR_WARPS) != (x - 1, y):
                        problems.append('%s: warp %d at %d,%d has no first block' % (name, kind, x, y))
                elif d['kind'] == P.DUN_TOWER:
                    # (v1.1 peaks) A tower's misty mountainside: Mt. Pyre's
                    # doors in the rock (0x0A7), walked into from below.
                    if block & 0x3FF != 0x0A7 or (block >> 10) & 3 or not f['grid'].ok(x, y + 1):
                        problems.append('%s: warp %d at %d,%d is not a door in the rock' % (name, kind, x, y))
                elif d['kind'] == P.DUN_VOLCANO:
                    # The Magma Hideout's doorways, walked into from below.
                    if mb[block & 0x3FF] != MB_NON_ANIMATED_DOOR or (block >> 10) & 3 or not f['grid'].ok(x, y + 1):
                        problems.append('%s: warp %d at %d,%d is not a doorway' % (name, kind, x, y))
                elif mb[block & 0x3FF] != MB_LADDER or (block >> 10) & 3:
                    problems.append('%s: warp %d at %d,%d is not a ladder' % (name, kind, x, y))
                if d['kind'] != P.DUN_FOREST and P.ladder_partner(f['floor'], kind) is None \
                   and not (kind == P.FLOOR_EXIT and is_tunnel_floor(maps, d, f)):
                    problems.append('%s: warp %d leads nowhere' % (name, kind))
            if P.FLOOR_UP not in f['warps']:
                problems.append('%s: no way up' % name)
                continue
            last = f['floor'] == d['floors']
            if not last and P.FLOOR_DOWN not in f['warps']:
                problems.append('%s: no way down' % name)
            if f['floor'] == exit_floor(maps, d) and d['exit'] >= 0 and P.FLOOR_EXIT not in f['warps']:
                problems.append('%s: no way out' % name)
            grid = P.Grid(f['blocks'], f['w'], ground_only=True)
            grid.ice = f['grid'].ice
            grid.blocked = set(f['grid'].blocked) - {(o['x'], o['y']) for o in f['objs'] if o['kind'] in OBSTACLES | {P.OBJ_TUNNEL_GUARD}}
            if d['kind'] == P.DUN_ICE:
                problems += check_ice(d, f, grid, name)
            if d['kind'] == P.DUN_VOLCANO and all(g.get('room', 255) != 255 for g in mine if g['floor'] > f['floor']):
                # Its heart, the deepest floor: a lava lake (0x2BD), mostly.
                ice_stats['volcano heart with lava'] += any(b & 0x3FF == 0x2BD for b in f['blocks'])
                ice_stats['volcano hearts'] += 1
            seen = set()
            for (x, y) in f['warps'].values():
                seen |= reachable(grid, [(x, y)] + [(x + dx, y + dy) for dx, dy in P.KEY_DELTA.values()])
            for kind, cell in f['warps'].items():
                if cell not in seen and not touches(cell, seen):
                    problems.append('%s: warp %d cannot be reached' % (name, kind))
            for o in f['objs']:
                if not touches((o['x'], o['y']), seen):
                    problems.append('%s: object kind %d at %d,%d cannot be reached' % (name, o['kind'], o['x'], o['y']))
            bosses += [o for o in f['objs'] if o['kind'] == P.OBJ_BOSS]
            gates = [o for o in f['objs'] if o['kind'] in OBSTACLES]
            if is_tunnel_floor(maps, d, f):
                problems += check_tunnel_floor(maps, d, f, name)
                gates = []
            if d['kind'] in (P.DUN_ICE, P.DUN_VOLCANO):
                # Strength boulders, as in Fiery Path and Seafoam: before a
                # side chamber's find, or across the way only if Strength is
                # held when the dungeon opens.
                if any(o['kind'] != OBJ_BOULDER for o in gates):
                    problems.append('%s: an obstacle other than a boulder' % name)
                if gates and STRENGTH not in keys_by(maps, d['badges']):
                    open_grid = P.Grid(f['blocks'], f['w'], ground_only=True)
                    open_grid.ice = f['grid'].ice
                    open_grid.blocked = set(f['grid'].blocked)
                    up = f['warps'][P.FLOOR_UP]
                    shut = reachable(open_grid, [(up[0], up[1] + 1)])
                    for kind, cell in f['warps'].items():
                        if not touches(cell, shut) and cell not in shut:
                            problems.append('%s: boulders across the way before STRENGTH' % name)
                    for o in f['objs']:
                        if o['kind'] in (P.OBJ_BOSS, OBJ_TRAINER) and not touches((o['x'], o['y']), shut):
                            problems.append('%s: boulders before the boss or a trainer before STRENGTH' % name)
                ice_stats['boulder pairs'] += len(gates) // 2
            elif gates and (d['kind'] != 1 or d['badges'] < 8):
                problems.append('%s: %d boulders or rocks before all badges' % (name, len(gates)))
            dungeon_stats['gates'] += len(gates) // 2
            dungeon_stats['trainers'] += sum(1 for o in f['objs'] if o['kind'] == OBJ_TRAINER)
            dungeon_stats['items'] += sum(1 for o in f['objs'] if o['kind'] == OBJ_ITEM)
            problems += check_floor_extras(d, f, name)
        if d['boss'] and len(bosses) != 1:
            problems.append('dungeon %d: %d bosses' % (d['i'], len(bosses)))
        problems += check_extras_plan(d, maps)
        dungeon_stats['bosses'] += len(bosses)
        if d['required'] and host['kind'] == 'cave':
            dungeon_stats['required'] += 1
            way_in, way_out = P.cave_ends(host)
            if way_out is None:
                problems.append('dungeon %d: required, but its cave is a dead end' % d['i'])
                continue
            route = P.dungeon_route(maps, d, 0, host['warps'][P.CAVE_SOUTH], 0, way_out)
            if route is None:
                problems.append('dungeon %d: no way through from the way in to the way out' % d['i'])
                continue
            deepest = max(lv for lv, _, _ in route)
            dungeon_stats['through floors %d' % deepest] += 1
            if deepest == 0:
                problems.append('dungeon %d: the way through stays on the cave' % d['i'])
            back = P.dungeon_route(maps, d, 0, (way_out[0], way_out[1] - 1), 0, way_in)
            if back is None:
                problems.append('dungeon %d: no way back from the way out' % d['i'])
            if P.dungeon_route(maps, d, 0, host['warps'][P.CAVE_SOUTH], 0, way_out, cleared=False) is None:
                dungeon_stats['gated'] += 1
        if d['boss'] == BOSS_LEGEND:
            dungeon_stats['legends'] += 1
            dungeon_stats['legend home %s' % DUN_KIND_NAMES[d['kind']]] += 1
            if not 1 <= d['legend'] < len(LEGEND_NAMES):
                problems.append('dungeon %d: legend %d' % (d['i'], d['legend']))
            problems += check_legend_gate(d, mine, maps, dungeons)
        elif d.get('legend'):
            problems.append('dungeon %d: a legend %d but boss %d' % (d['i'], d['legend'], d['boss']))
        if d['kind'] == P.DUN_LAIR:
            # A lair: a door in its town's rock face, a cave floor or two
            # with its trainers and items, sharing the town's catch area,
            # and the legend's chamber.
            if host['kind'] != 'town' or d['boss'] != BOSS_LEGEND or not 2 <= d['floors'] <= 3 \
               or d['area'] != host.get('area', -1):
                problems.append('dungeon %d: a lair hosted by %s, boss %d, %d floors, area %d'
                                % (d['i'], host['kind'], d['boss'], d['floors'], d['area']))
            dungeon_stats['lair room %s' % P.room_library()[mine[-1]['room']].name] += 1 if mine else 0
        leaders = [o for f in mine for o in f['objs'] if o['kind'] == P.OBJ_VILLAIN_LEADER]
        if d.get('leader'):
            dungeon_stats['leader before legend'] += 1
            last = mine[-1] if mine else None
            if len(leaders) != 1 or d['gate'] != P.GATE_VILLAIN_LEADER or (last and leaders[0] not in last['objs']):
                problems.append('dungeon %d: the villain leader is not before its legend' % d['i'])
            elif last:
                # He stands on the walk to the legend: with him there, the
                # legend can be reached only past him (or he is in sight).
                boss = next(o for o in last['objs'] if o['kind'] == P.OBJ_BOSS)
                lo = leaders[0]
                if abs(lo['x'] - boss['x']) + abs(lo['y'] - boss['y']) > 8:
                    problems.append('dungeon %d: the villain leader stands far from the legend' % d['i'])
        elif leaders:
            problems.append('dungeon %d: a villain leader but not his legend' % d['i'])
    problems += check_volcano_place(maps, dungeons)
    legends = [d for d in dungeons if d['boss'] == BOSS_LEGEND]
    dungeon_stats['legend count %d' % len(legends)] += 1
    if not legends:
        problems.append('no legend')
    if len({d['legend'] for d in legends}) != len(legends):
        problems.append('a legend twice')
    if P.hideout(maps) is not None and not any(d.get('leader') for d in dungeons):
        dungeon_stats['hideout without the leader'] += 1
    legend_sets[' '.join(LEGEND_NAMES[d['legend']] + '@%d' % d['gateBadges'] for d in sorted(legends, key=lambda d: d['gateBadges']))] += 1
    late = sum(1 for d in legends if d['gateBadges'] >= 5)
    dungeon_stats['legend late %d of %d' % (late, len(legends))] += 1
    return problems


def sight_cells(f, o, rng):
    """The blocks a trainer on floor f sees (SeparateBossSight's way): its
    directions up to rng, stopped by a wall or anyone standing."""
    w, h = f['w'], len(f['blocks']) // f['w']
    turn = o.get('turn', -1)
    faces = {o['facing']} if turn < 0 else {0, 1, 2, 3} if turn == 4 else {o['facing'], turn}
    taken = {(q['x'], q['y']) for q in f['objs']}
    out = set()
    for fc in faces:
        dx, dy = {0: (0, -1), 1: (1, 0), 2: (0, 1), 3: (-1, 0)}[fc]
        for k in range(1, rng + 1):
            c = (o['x'] + dx * k, o['y'] + dy * k)
            if not (0 <= c[0] < w and 0 <= c[1] < h) or f['blocks'][c[1] * w + c[0]] & 0x0C00 or c in taken:
                break
            out.add(c)
    return out


def boss_sight_shared(f, boss_trainer):
    """A block seen by the floor's boss (a trainer) or a team's leader and
    by another trainer at once, or None: a player there would draw a boss
    into a battle of two trainers."""
    bosses = [o for o in f['objs'] if (o['kind'] == P.OBJ_BOSS and boss_trainer) or o['kind'] == P.OBJ_VILLAIN_LEADER]
    if not bosses:
        return None
    seen = set()
    for o in bosses:
        seen |= sight_cells(f, o, max(o.get('range', 4), 4))
    for o in f['objs']:
        if o['kind'] == OBJ_TRAINER:
            shared = sight_cells(f, o, o.get('range', 3)) & seen
            if shared:
                return min(shared)
    return None


villain_stats = Counter()


def check_villain(maps):
    """The villain team's hideout: every region has one, a dungeon under a
    gym city or village of the main road from the second gym to the sixth,
    its admin at the bottom; two grunts and a gate in the town's way on, and
    the way in to the hideout (when the town has one) reachable without
    passing them; and grunts on a main route before the town. Where no
    town can hold a blockade (a way round it), the hideout is optional,
    with no grunts or gate in the way on."""
    problems = []
    v = maps[0].get('villain', {})
    h = P.hideout(maps)
    villain_stats['regions'] += 1
    if h is None:
        problems.append('no villain team')
        if v.get('hint', -1) >= 0:
            problems.append('villain grunts on map %d with no hideout' % v['hint'])
        return problems
    by = {m['i']: m for m in maps}
    host = by[h['host']]
    villain_stats['hideouts'] += 1
    villain_stats['main team %s' % ('Aqua', 'Magma', 'Rocket')[v['team']]] += 1
    villain_stats['after %d gyms' % host['badges']] += 1
    villain_stats['floors %d' % h['floors']] += 1
    name = 'hideout in map %d' % host['i']
    if h['kind'] != P.DUNGEON_HIDEOUT or h['boss'] != P.BOSS_ADMIN or not 2 <= h['floors'] <= 3:
        problems.append('%s: kind %d boss %d required %d floors %d' % (name, h['kind'], h['boss'], h['required'], h['floors']))
    if host['kind'] != 'town' or host['side'] or host['type'] not in (P.TOWN_GYM, P.TOWN_VILLAGE) or not 1 <= host['badges'] <= 5:
        problems.append('%s: not a main road town from the second gym to the sixth' % name)
    gates = [l for l in host['links'] if l['gate'] == P.GATE_VILLAIN]
    if not h['required']:
        # No blockade: no gate, no grunts in the way on, the way in reachable.
        villain_stats['no blockade'] += 1
        if gates or any(o['kind'] == P.OBJ_GRUNT and o['slot'] < 2 for o in host['objs']):
            problems.append('%s: optional hideout with a blockade' % name)
        door = host['warps'].get(P.WARP_DUNGEON)
        ents = entrances(host, maps)
        start = next(iter(ents.values()), None)
        if door is not None and start is not None:
            grid = P.Grid(host['blocks'], host['w'], ground_only=True)
            grid.blocked = set(host['grid'].blocked)
            if below(door) not in reachable(grid, [start]):
                problems.append('%s: the way into the hideout cannot be reached' % name)
        hint = v.get('hint', -1)
        if hint >= 0:
            r = by[hint]
            if r['kind'] != 'route' or r['side'] or hint >= host['i'] or v['mask'] >> r['nTrainers'] or v['mask'] == (1 << r['nTrainers']) - 1:
                problems.append('villain grunts on map %d (mask %x)' % (hint, v['mask']))
        return problems
    if len(gates) != 1 or by[gates[0]['target']]['kind'] != 'route' or by[gates[0]['target']]['side']:
        return problems + ['%s: blockade gates %s' % (name, gates)]
    l = gates[0]
    grunts = [o for o in host['objs'] if o['kind'] == P.OBJ_GRUNT and o['slot'] < 2]
    exit_cells = {edge_cell(host, l['dir'], (l['pos'] & ~1) + k) if l['dir'] in (0, 2) else
                  edge_cell(host, l['dir'], (l['pos'] & ~1) + k) for k in (0, 1)}
    if {(o['x'], o['y']) for o in grunts} != exit_cells:
        problems.append('%s: grunts at %s, exit %s' % (name, [(o['x'], o['y']) for o in grunts], sorted(exit_cells)))
    cells = {(gt['x'], gt['y']) for gt in host['gates'] if gt['need'] == P.GATE_VILLAIN}
    if len(cells) != 2:
        problems.append('%s: %d blockade gate cells' % (name, len(cells)))
    # From the way in, the player walks up to the gate in front of the
    # grunts, and reaches the hideout's way in without passing it.
    ents = entrances(host, maps)
    start = next((c for d, c in ents.items() if d != l['dir']), None)
    grid = P.Grid(host['blocks'], host['w'], ground_only=True)
    grid.blocked = set(host['grid'].blocked) | exit_cells | cells
    seen = reachable(grid, [start]) if start else set()
    if not any(touches(c, seen) for c in cells):
        problems.append('%s: the blockade cannot be reached' % name)
    door = host['warps'].get(P.WARP_DUNGEON)
    if door is not None and below(door) not in seen:
        problems.append('%s: the way into the hideout is past the blockade' % name)
    villain_stats['entrance in town' if door is not None else 'no entrance yet'] += 1
    hint = v.get('hint', -1)
    if hint >= 0:
        r = by[hint]
        villain_stats['hint grunts %d' % bin(v['mask']).count('1')] += 1
        if r['kind'] != 'route' or r['side'] or hint >= host['i'] or v['mask'] >> r['nTrainers'] or v['mask'] == (1 << r['nTrainers']) - 1:
            problems.append('villain grunts on map %d (mask %x)' % (hint, v['mask']))
    else:
        villain_stats['no hint route'] += 1
    return problems


team_stats = Counter()


def check_teams(maps):
    """The villain teams (src/rogue_gen_hideout.h): one to three of AQUA,
    MAGMA and ROCKET, each once, team 0 the main team (check_villain). The
    others' hideouts: under a gym city or village of the main road from the
    second gym to the sixth, two or three floors, the admin at the bottom,
    no blockade (not required, no gate, no grunts in the way on) and a
    lookout by the door; their grunts on a main route before the town
    (hint, mask), and there a grunt of the team before (guest, where two
    teams clash), never the whole route, never two teams' grunts on one
    trainer. Each leader before a legend of his own, its gate his team's.
    GIOVANNI runs no Elite Four seat where TEAM ROCKET is, and his gym is
    the last where he runs one."""
    problems = []
    teams = maps[0].get('teams') or []
    by = {m['i']: m for m in maps}
    dungeons = {d['i']: d for d in maps[0].get('dungeons', [])}
    team_stats['regions'] += 1
    team_stats['teams %d' % len(teams)] += 1
    if not teams:
        return ['no villain teams']
    if len({t['id'] for t in teams}) != len(teams):
        problems.append('a team twice: %s' % [t['id'] for t in teams])
    hideouts = [d['i'] for d in maps[0].get('dungeons', []) if d['kind'] == P.DUNGEON_HIDEOUT]
    if [t['hideout'] for t in teams] != hideouts:
        problems.append('team hideouts %s, hideouts %s' % ([t['hideout'] for t in teams], hideouts))
    used = Counter()
    for t in teams:
        name = '%s (team %d)' % (P.TEAM_NAMES[t['id']], t['t'])
        team_stats['id %s' % P.TEAM_NAMES[t['id']]] += 1
        h = dungeons.get(t['hideout'])
        if h is None:
            problems.append('%s: no hideout' % name)
            continue
        host = by[h['host']]
        if t['t'] > 0:
            if h['kind'] != P.DUNGEON_HIDEOUT or h['boss'] != P.BOSS_ADMIN or not 2 <= h['floors'] <= 3 or h['required']:
                problems.append('%s: hideout kind %d boss %d required %d floors %d' % (name, h['kind'], h['boss'], h['required'], h['floors']))
            if host['kind'] != 'town' or host['side'] or host['type'] not in (P.TOWN_GYM, P.TOWN_VILLAGE) or not 1 <= host['badges'] <= 5:
                problems.append('%s: hideout not under a main road town from the second gym to the sixth' % name)
            if any(l['gate'] == P.GATE_VILLAIN for l in host['links']) or any(o['kind'] == P.OBJ_GRUNT and o['slot'] < 2 for o in host['objs']):
                problems.append('%s: a blockade' % name)
        # (A lookout stands by the door where the town has room.)
        if not any(o['kind'] == P.OBJ_GRUNT and o['slot'] == 2 for o in host['objs']):
            team_stats['no lookout'] += 1
        if t['hint'] >= 0:
            r = by[t['hint']]
            if r['kind'] != 'route' or r['side'] or t['hint'] >= host['i'] or t['mask'] & t['guest'] or (t['mask'] | t['guest']) >> r['nTrainers']:
                problems.append('%s: grunts on map %d (mask %x guest %x)' % (name, t['hint'], t['mask'], t['guest']))
            if t['guest'] and t['t'] == 0:
                problems.append('%s: a guest on the main team\'s route' % name)
            used[t['hint']] |= 0
            for k in range(8):
                if ((t['mask'] | t['guest']) >> k) & 1:
                    if used[(t['hint'], k)]:
                        problems.append('%s: trainer %d of map %d is two teams\' grunt' % (name, k, t['hint']))
                    used[(t['hint'], k)] = 1
            team_stats['clash routes'] += 1 if t['guest'] else 0
        else:
            team_stats['no grunts on the road'] += 1
        if t['leader'] >= 0:
            d = dungeons[t['leader']]
            team_stats['leaders'] += 1
            if d['gate'] != P.GATE_VILLAIN_LEADER or d['gateSrc'] != t['t'] or d.get('leader') != t['t'] + 1:
                problems.append('%s: leader before dungeon %d with gate %d src %d' % (name, d['i'], d['gate'], d['gateSrc']))
    for t in teams:
        hint = by.get(t['hint'])
        if hint is not None:
            busy = sum(1 for k in range(hint['nTrainers']) if used[(t['hint'], k)])
            if busy >= hint['nTrainers']:
                problems.append('every trainer of map %d is a grunt' % t['hint'])
    for d in maps[0].get('dungeons', []):
        if d['gate'] == P.GATE_VILLAIN_LEADER and (d['gateSrc'] >= len(teams) or teams[d['gateSrc']]['leader'] != d['i']):
            problems.append('dungeon %d: kept by team %d, which waits elsewhere' % (d['i'], d['gateSrc']))
    names = maps[0].get('leaders', '')
    if 2 in [t['id'] for t in teams]:
        team_stats['rocket'] += 1
        rocket = next(t for t in teams if t['id'] == 2)
        gyms = [w for w in names.replace('LT. SURGE', 'LT.SURGE').split()]
        if 'GIOVANNI' in gyms:
            k = gyms.index('GIOVANNI')
            if k >= 8:
                problems.append('GIOVANNI is in the Elite Four where TEAM ROCKET is')
            team_stats['giovanni gym %d' % (k + 1)] += 1
            team_stats['giovanni twist (BOSS at a legend)' if rocket['leader'] >= 0 else 'giovanni twist (BOSS in his hideout)'] += 1
        team_stats['rocket BOSS at a legend' if rocket['leader'] >= 0 else 'rocket BOSS in his hideout'] += 1
        if rocket['leader'] < 0 and not rocket.get('bossInHideout'):
            problems.append('TEAM ROCKET\'s BOSS is met nowhere')
    return problems


wanderer_stats = Counter()


def check_wanderer(maps):
    """The wanderer waits by the POKeMON CENTER's door in the towns of their
    meetings (RogueText_WandererSpot): beside the way out of the door or a
    step below it, on plain ground, not on a door, a sign or the block in
    front of one, and nothing the player could reach from the town's ways
    in is cut off by them standing there."""
    problems = []
    for m in maps:
        w = m.get('wanderer')
        if w is None:
            continue
        wanderer_stats['meetings'] += 1
        if not w['spot']:
            wanderer_stats['no spot by the CENTER'] += 1
            continue
        spot = (w['x'], w['y'])
        pc = m['warps'].get(P.WARP_PC)
        name = 'wanderer in map %d at %s' % (m['i'], spot)
        if pc is None or not (abs(spot[0] - pc[0]) <= 2 and 1 <= spot[1] - pc[1] <= 2) or spot == below(pc):
            problems.append('%s: not beside the POKeMON CENTER door %s' % (name, pc))
        if any((x, y) == spot or (x, y + 1) == spot for x, y in m['warps'].values()):
            problems.append('%s: on a door or the block in front of one' % name)
        others = {(o['x'], o['y']) for k, o in enumerate(m['objs']) if k != w['obj']}
        if spot in others:
            problems.append('%s: on someone else' % name)
        grid = P.Grid(m['blocks'], m['w'], ground_only=True)
        # The townsperson the wanderer stands in for leaves their own spot
        # (the game moves that very object to the CENTER's door).
        me = m['objs'][w['obj']] if 0 <= w['obj'] < len(m['objs']) else None
        grid.blocked = set(m['grid'].blocked) - ({(me['x'], me['y'])} if me else set())
        if not grid.ok(*spot):
            problems.append('%s: not on open ground' % name)
            continue
        starts = list(entrances(m, maps).values())
        seen = reachable(grid, starts)
        grid.blocked = grid.blocked | {spot}
        if reachable(grid, starts) != seen - {spot}:
            problems.append('%s: closes part of the town' % name)
        wanderer_stats['by the CENTER'] += 1
    return problems


def region_hash(maps):
    """A hash of everything generated for a region: its plan, maps, events,
    dungeons and floors, to show a change leaves regions as they were."""
    return hashlib.sha256(json.dumps(maps, sort_keys=True, default=lambda o: None).encode()).hexdigest()  # Grids come from the blocks


loop_stats = {'regions': 0, 'wrap': 0, 'tunnel': 0, 'ring': 0, 'opens': Counter(), 'saved': [], 'span3': 0,
              'road': 0, 'road on loop': 0, 'places': [], 'by5': 0, 'early tunnel': 0, 'early side': 0}

# A loop may lead at most this many stretches past the badges it opens with
# (won in order): LOOP_AHEAD in src/rogue_gen.c.
LOOP_AHEAD = 1


def place_graph(maps, tunnel=True):
    """Places joined by links, cave mouths and (with tunnel) the tunnel."""
    adj = {m['i']: set() for m in maps}
    for m in maps:
        for l in m['links']:
            adj[m['i']].add(l['target'])
            adj[l['target']].add(m['i'])
        if m.get('mouth', -1) >= 0:
            adj[m['i']].add(m['mouth'])
            adj[m['mouth']].add(m['i'])
        if m['kind'] == 'cave':
            for c in (m['caveIn'], m['caveOut']):
                if c >= 0:
                    adj[m['i']].add(c)
                    adj[c].add(m['i'])
    tun = maps[0].get('tunnel')
    if tunnel and tun:
        adj[tun['host']].add(tun['exit'])
        adj[tun['exit']].add(tun['host'])
    return adj


def bfs_dist(adj, s, banned=()):
    dist, q = {s: 0}, deque([s])
    while q:
        u = q.popleft()
        for v in adj[u]:
            if v not in dist and v not in banned:
                dist[v] = dist[u] + 1
                q.append(v)
    return dist


def way_open(maps, by, a, b, comp, keys):
    """TRUE if a side way can be walked from end a to end b through its own
    places with these keys (its obstacle cleared)."""
    inside, seen, q = set(comp) | {a, b}, {a}, [a]
    while q:
        m = by[q.pop()]
        for l in m['links']:
            need = P.link_keys(by, m, l)
            if l['target'] in inside and l['target'] not in seen and need & keys == need:
                seen.add(l['target'])
                q.append(l['target'])
    return b in seen


def loop_metrics(maps):
    """Region loops: wraps (a tunnel, or
    a side way joining main road places three or more gyms apart: a ring
    road or a long shortcut), the badges each opens with (won in order),
    places it saves on the walk between its ends, loops spanning 3+ gyms,
    and the share of main road places on any loop. A tunnel whose far end
    lies more than LOOP_AHEAD stretches past the badges it opens with is a
    problem (returned); side ways that do are counted (shortcuts may)."""
    problems = []
    by = {m['i']: m for m in maps}
    adj = place_graph(maps)
    road = road_of(maps)
    # Side groups with two main road ends: loops of their own.
    loops, seen = [], set()
    for m in maps:
        if not m['side'] or m['i'] in seen:
            continue
        comp, q = [], [m['i']]
        seen.add(m['i'])
        while q:
            u = q.pop()
            comp.append(u)
            for v in adj[u]:
                if by[v]['side'] and v not in seen:
                    seen.add(v)
                    q.append(v)
        ends = sorted({v for u in comp for v in adj[u] if not by[v]['side']})
        if len(ends) == 2:
            loops.append((ends[0], ends[1], set(comp), False))
    tun = maps[0].get('tunnel')
    if tun:
        # The road place the host hangs off, for a side cave.
        a = tun['host']
        if by[a]['side']:
            d0 = bfs_dist(adj, a, banned={tun['exit']})
            a = min((p for p in road if p in d0), key=lambda p: d0[p], default=None)
        if a is not None:
            loops.append((a, tun['exit'], {tun['host']}, True))
    wraps = []
    for a, b, comp, is_tunnel in loops:
        span = abs(by[a]['badges'] - by[b]['badges'])
        if span < 3:
            continue
        # Badges (won in order) when the way can first be walked.
        opens = None
        for g in range(NUM_GYMS + 1):
            reach = P.reach_with(maps, P.keys_held(maps, (1 << g) - 1))
            if is_tunnel:
                ok = g >= tun['opens']
            else:
                ok = (a in reach or b in reach) and way_open(maps, by, a, b, comp, P.keys_held(maps, (1 << g) - 1))
            if ok:
                opens = g
                break
        far = max(by[a]['badges'], by[b]['badges'])
        if opens is not None and far > opens + LOOP_AHEAD:
            loop_stats['early tunnel' if is_tunnel else 'early side'] += 1
            if is_tunnel:
                problems.append('the tunnel opens with %d badges onto stretch %d' % (opens, far))
        without = bfs_dist(place_graph(maps, tunnel=not is_tunnel), a, banned=set() if is_tunnel else comp).get(b)
        steps = 1 if is_tunnel else len(comp) + 1
        wraps.append((span, opens, (without - steps) if without else 0, is_tunnel))
    loop_stats['regions'] += 1
    loop_stats['places'].append(len(maps))
    if wraps:
        best = min(wraps, key=lambda w: (w[1] if w[1] is not None else 99, -w[2]))
        loop_stats['wrap'] += 1
        loop_stats['tunnel'] += any(w[3] for w in wraps)
        loop_stats['ring'] += any(not w[3] for w in wraps)
        loop_stats['opens'][best[1]] += 1
        loop_stats['by5'] += best[1] is not None and best[1] <= 5
        loop_stats['saved'].append(best[2])
        loop_stats['span3'] += 1
    # Main road places on any loop: not cut off by a bridge.
    tin, low, bridges, clock = {}, {}, set(), [0]
    sys.setrecursionlimit(10000)

    def dfs(u, parent):
        tin[u] = low[u] = clock[0]
        clock[0] += 1
        for v in adj[u]:
            if v == parent:
                continue
            if v in tin:
                low[u] = min(low[u], tin[v])
            else:
                dfs(v, u)
                low[u] = min(low[u], low[v])
                if low[v] > tin[u]:
                    bridges.add(frozenset((u, v)))
    dfs(0, -1)
    on = {u for u in adj for v in adj[u] if frozenset((u, v)) not in bridges}
    loop_stats['road'] += len(road)
    loop_stats['road on loop'] += sum(1 for p in road if p in on)
    return problems


ruins_stats = Counter()


def ruins_layout(floors, d):
    """What a ruins dungeon's floors are on one visit: each floor's room and
    where its trainers, items and lost explorer stand."""
    return [(f['room'], sorted((o['kind'], o['x'], o['y']) for o in f['objs'])) for f in floors if f['dungeon'] == d['i']]


def ruins_slots(floors, d):
    """Which trainer slots and item flags a ruins dungeon's floors hold, by
    floor: the same on every visit, so nothing beaten or taken comes back."""
    return [(f['floor'], f['trainerBase'], f['nTrainers'], f['itemBase'], f['nItems'],
             sorted(o['slot'] for o in f['objs'] if o['kind'] in (OBJ_TRAINER, OBJ_ITEM, P.OBJ_PRIZE, P.OBJ_CHOICE, P.OBJ_EXPLORER)))
            for f in floors if f['dungeon'] == d['i']]


peaks_stats = Counter()


def check_peaks(seed, maps):
    """(v1.1 peaks, src/rogue_gen_peaks.h) The desert tower: on a sand
    desert in a sandstorm, drawn with Mauville's tileset, the Mirage Tower
    standing on its sand with its door at the dungeon's way in; its four
    floors the Mirage Tower's, in order; a landing for a fall on every floor
    but the top, from which the ladder back up is walked to; no sand pit at
    the top, where the guardian stands with its prize and the two fossils.
    A tower's misty mountainside: a generated floor below the summit, on
    Mt. Pyre's outdoor tileset, its graves and steps counted."""
    problems = []
    by = {m['i']: m for m in maps}
    for d in maps[0].get('dungeons', []):
        floors = sorted((f for f in maps[0].get('floors', []) if f['dungeon'] == d['i']), key=lambda f: f['floor'])
        if d['kind'] == P.DUN_TOWER:
            peaks_stats['towers'] += 1
            for f in floors:
                if f.get('room', 255) != 255:
                    continue
                peaks_stats['slopes'] += 1
                name = 'dungeon %d floor %d (mountainside)' % (d['i'], f['floor'])
                if f['floor'] != d['floors'] - 1 or f['tileset'] != 'facility':
                    problems.append('%s: floor %d of %d, tileset %s' % (name, f['floor'], d['floors'], f['tileset']))
                graves = sum(1 for b in f['blocks'] if b & 0x3FF in (0x324, 0x325))
                stairs = sum(1 for b in f['blocks'] if b & 0x3FF == 0x0AF)
                peaks_stats['slope graves'] += graves
                peaks_stats['slope stair blocks'] += stairs
                peaks_stats['slopes with steps'] += stairs > 0
                if graves == 0:
                    problems.append('%s: no graves' % name)
                # Both colours of grave, and stairs that climb tall faces
                # (each flight three rows or more), as Mt. Pyre's.
                kinds = {b & 0x3FF for b in f['blocks']} & {0x324, 0x325}
                if graves and len(kinds) < 2:
                    problems.append('%s: graves of one colour' % name)
                fw, blocks = f['w'], [b & 0x3FF for b in f['blocks']]
                fh = len(blocks) // fw
                for x in range(fw):
                    for y in range(fh):
                        if blocks[y * fw + x] == 0x0AF and (y == 0 or blocks[(y - 1) * fw + x] != 0x0AF):
                            run = 0
                            while y + run < fh and blocks[(y + run) * fw + x] == 0x0AF:
                                run += 1
                            peaks_stats['slope flights'] += 1
                            if run < 3:
                                problems.append('%s: a flight of %d rows at %d,%d' % (name, run, x, y))
            continue
        if d['kind'] != P.DUN_MIRAGE:
            continue
        name = 'desert tower %d' % d['i']
        peaks_stats['mirage'] += 1
        host = by[d['host']]
        peaks_stats['mirage on a side route' if host['side'] else 'mirage on the main road'] += 1
        if host['kind'] != 'route' or host['type'] != 7 or host['theme'] != 1 or host.get('weather') != 4:
            problems.append('%s: on map %d (%s type %d theme %d weather %s), not a sandstorm desert on Mauville tiles'
                            % (name, d['host'], host['kind'], host.get('type', -1), host.get('theme', -1), host.get('weather')))
        door = host['warps'].get(P.WARP_DUNGEON)
        if door is None:
            problems.append('%s: no way in on map %d' % (name, d['host']))
        else:
            x, y = door
            stamp = [host['blocks'][(y - 5 + r) * host['w'] + x - 1 + c] & 0x3FF for r in range(6) for c in range(3)]
            want = [0x3D8, 0x3D9, 0x3DA, 0x3E0, 0x3E1, 0x3E2, 0x3E8, 0x3E9, 0x3EA, 0x3F0, 0x3F1, 0x3F2,
                    0x3DB, 0x3DC, 0x3DD, 0x3E3, 0x3E4, 0x3E5]
            if stamp != want:
                problems.append('%s: the tower at %d,%d is not drawn whole' % (name, x, y))
            if not host['grid'].ok(x, y + 1):
                problems.append('%s: nowhere to stand at the door' % name)
        if d['floors'] != 4 or d['boss'] != 1 or len(floors) != 4:
            problems.append('%s: %d floors, boss %d' % (name, d['floors'], d['boss']))
            continue
        ex = d.get('extras', {})
        if ex.get('nChoice') != 2 or ex.get('choiceFloor') != 4 or ex.get('prize', -1) < 0:
            problems.append('%s: no prize and two fossils at the top' % name)
        for f in floors:
            fname = '%s floor %d' % (name, f['floor'])
            room = P.room_library()[f['room']] if f.get('room', 255) != 255 else None
            if room is None or room.style != 'MIRAGE' or room.name != 'MirageTower_%dF' % f['floor']:
                problems.append('%s: room %s' % (fname, room.name if room else None))
                continue
            view = RoomView(f)
            last = f['floor'] == d['floors']
            if last:
                if view.avoid:
                    problems.append('%s: sand pits at the top' % fname)
                continue
            peaks_stats['mirage pit floors'] += bool(view.avoid)
            land = [i for i, w in enumerate(room.warps) if w[2] == 'LAND']
            on = [i for i, w in enumerate(room.warps) if w[2] == 'ON']
            if not land or not on:
                problems.append('%s: nowhere for a fall to land' % fname)
                continue
            # From where a fall lands, the ladder back up and the way down.
            standing = {(o['x'], o['y']) for o in f['objs']}
            seen = gen_rooms.walk(view, [room.warps[land[0]][:2]], room.warps, standing)
            for i, w in enumerate(room.warps):
                if w[2] != 'LAND' and not gen_rooms.touches(w[:2], seen):
                    problems.append('%s: warp %d cannot be reached after a fall' % (fname, i))
    return problems


def check_ruins(seed, maps):
    """Ruins (src/rogue_gen_ruins.h): below a dead-end side cave, past the
    first badge and away from a volcano; the Mirage Tower's floors, then a
    vault (a Regi chamber) last, never a sand pit there; every floor has a
    landing spot for a fall from above, the vault three tablets at its door
    and the guardian shut behind it. The floors shift with the visit (shown
    for visits 0 to 3): another order of rooms or other spots, but always
    the same trainer slots and item flags on each floor, and the same
    riddle."""
    problems = []
    by = {m['i']: m for m in maps}
    for d in maps[0].get('dungeons', []):
        if d['kind'] != P.DUN_RUINS:
            continue
        name = 'ruins %d' % d['i']
        ruins_stats['ruins'] += 1
        ruins_stats['floors %d' % d['floors']] += 1
        host = by[d['host']]
        if host['kind'] != 'cave' or not host['side'] or host['caveOut'] != -1 or d['required'] or d['badges'] < 1:
            problems.append('%s: under map %d (%s), not a dead-end side cave' % (name, d['host'], host['kind']))
        if host['caveIn'] >= 0 and by[host['caveIn']]['theme'] in (7, 8):
            problems.append('%s: by a volcano' % name)
        if d['boss'] != 1 or not 3 <= d['floors'] <= 4:
            problems.append('%s: boss %d, %d floors' % (name, d['boss'], d['floors']))
        rid = d.get('riddle', {})
        words = [rid.get('w%d' % k) for k in range(3)]
        order = [rid.get('o%d' % k) for k in range(3)]
        if len(set(words)) != 3 or sorted(order) != [0, 1, 2] or order == [0, 1, 2]:
            problems.append('%s: riddle %s read %s' % (name, words, order))
        layouts, slots = [], []
        for visit in range(4):
            vmaps = maps if visit == 0 else P.load_region(seed, visit)
            vd = next(x for x in vmaps[0]['dungeons'] if x['i'] == d['i'])
            floors = sorted((f for f in vmaps[0]['floors'] if f['dungeon'] == d['i']), key=lambda f: f['floor'])
            if {k: v for k, v in vd.get('riddle', {}).items() if k != 'visit'} != {k: v for k, v in rid.items() if k != 'visit'}:
                problems.append('%s: the riddle changes on visit %d' % (name, visit))
            for f in floors:
                room = P.room_library()[f['room']]
                fname = '%s floor %d visit %d' % (name, f['floor'], visit)
                last = f['floor'] == d['floors']
                if room.style != ('VAULT' if last else 'SANDS'):
                    problems.append('%s: room %s' % (fname, room.name))
                if f.get('land', -1) < 0 or room.warps[f['land']][2] != 'LAND':
                    problems.append('%s: nowhere for a fall to land' % fname)
                view = RoomView(f)
                if last and view.avoid:
                    problems.append('%s: sand pits on the last floor' % fname)
                ruins_stats['pit floors'] += bool(view.avoid) and visit == 0
                if visit:
                    # The other visits' floors hang together as the first's do.
                    problems += check_room_floor(vd, f, floors, host, fname)
                    problems += check_floor_extras(vd, f, fname)
                if not last:
                    continue
                tablets = sorted((sg['x'], sg['y']) for sg in f.get('signs', []) if sg['kind'] == P.SIGN_TABLET)
                if len(tablets) != 3 or len({y for _, y in tablets}) != 1 or tablets[2][0] - tablets[0][0] != 2:
                    problems.append('%s: tablets %s' % (fname, tablets))
                    continue
                # The guardian and the treasure are shut in until the door
                # (the middle tablet, a warp pad) opens.
                warps = list(room.warps)
                door = tablets[1]
                shut = gen_rooms.walk(view, [warps[0][:2]], warps, {door})
                inner = [o for o in f['objs'] if o['kind'] in (P.OBJ_BOSS, P.OBJ_PRIZE, P.OBJ_CHOICE)]
                if len(inner) < 4 or any(gen_rooms.touches((o['x'], o['y']), shut) for o in inner):
                    problems.append('%s: the guardian or its treasure can be reached past the shut door' % fname)
            layouts.append(ruins_layout(vmaps[0]['floors'], d))
            slots.append(ruins_slots(vmaps[0]['floors'], d))
        if any(sl != slots[0] for sl in slots):
            problems.append('%s: trainer slots or item flags change between visits' % name)
        if len({repr(l) for l in layouts}) < 2:
            problems.append('%s: the floors are the same on four visits' % name)
        ruins_stats['different layouts'] += len({repr(l) for l in layouts})
    return problems


def check(seed):
    maps = P.load_region(seed)
    loop_problems = loop_metrics(maps)
    if os.environ.get('REGION_HASHES'):
        with open(os.environ['REGION_HASHES'], 'a') as f:
            f.write('%d %s\n' % (seed, region_hash(maps)))
    problems = check_ruins(seed, maps) + check_peaks(seed, maps) + loop_problems
    problems += check_plan(maps) + check_region(maps) + check_sea_seams(maps) + check_town_signs(maps) + check_dungeons(maps) + check_villain(maps) + check_teams(maps) + check_wanderer(maps)
    for m in maps:
        problems += check_map(m, maps) + check_victory_camp(m, maps) + check_warps(m) + check_water(m, maps) + check_island_trees(m) + check_red_rock(m) + check_beach_trees(m) + check_seam_view(m)
        problems += check_heights(m)
    run0 = seed == RUN0_SEED
    for f in split_check.check_region(maps):
        if run0:
            run0_tiles['split ' + str(f['kind'])] += 1
        elif not f['seam']:
            problems.append('%s: split tree at %d,%d (%s with %s to the %s)'
                            % (f['name'], f['x'], f['y'], f['tile'], f['other'], f['side']))
    # Tiling polish (tile_polish.py): kinds with no budget never occur, the
    # rest are counted and held to their budget over the run (main).
    for f in tile_polish.check_region(maps):
        if run0:
            run0_tiles[f['kind']] += 1
            continue
        tile_stats[f['kind']] += 1
        if f['kind'] not in tile_polish.BUDGET:
            problems.append('map %d: %s at %d,%d (%s)' % (f['map'], f['kind'], f['x'], f['y'], f['detail']))
    if not run0:
        tile_stats['maps'] += sum(1 for m in maps if m['kind'] != 'cave')
        # After RUN 0's STOP the moving truck parks in the first town
        # (RogueGen_TruckSpot): somewhere, and with all of it clear.
        truck = next((m['truck'] for m in maps if 'truck' in m), None)
        if truck is None or not truck['found']:
            problems.append('no spot for the moving truck in the first town')
        else:
            truck_stats['towns'] += 1
            truck_stats['clear'] += truck['clear']
            truck_stats['old clear'] += truck['oldClear']
            truck_stats['moved'] += (truck['x'], truck['y']) != (truck['oldX'], truck['oldY'])
            if not truck['clear']:
                truck_stats['top over'].append(seed)
    return problems


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    first = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    bad = 0
    for seed in range(first, first + count):
        problems = check(seed)
        if problems:
            bad += 1
            print('seed %d: %s' % (seed, '; '.join(problems[:4])))
    print('%d of %d seeds OK' % (count - bad, count))
    if run0_tiles:
        print('RUN 0 corruption (on purpose, not budgeted): ' + ', '.join('%s %d' % kv for kv in sorted(run0_tiles.items())))
    if tile_stats['maps']:
        nmaps = tile_stats.pop('maps')
        print('tiling: %d towns and routes; %s a map' % (nmaps, ', '.join('%s %.3f' % (k, v / nmaps)
                                                                     for k, v in sorted(tile_stats.items()))))
        over = tile_polish.over_budget(tile_stats, nmaps)
        if over:
            print('tiling over budget: ' + '; '.join(over))
            bad += 1
    n = len(region_stats['before_gym1'])
    if n:
        print('catch areas before gym 1: %d to %d, mean %.1f' % (min(region_stats['before_gym1']), max(region_stats['before_gym1']),
                                                              sum(region_stats['before_gym1']) / n))
        print('regions with a shortcut: %d, a fork: %d, the sea: %d of %d (a second stretch: %d); crossroads towns %.1f a region'
              % (region_stats['shortcuts'], region_stats['forks'], region_stats['sea'], n, region_stats['second sea'],
                 sum(region_stats['crossroads']) / n))
        print('regions with both: %d, a hamlet: %d, a hamlet before the sixth badge: %d; obstacle kinds %s'
              % (region_stats['both'], region_stats['hamlet'], region_stats['hamlet_early'],
                 ', '.join('%d: %d' % kv for kv in sorted(region_stats['kinds'].items()))))
    if victory_stats['roads']:
        n = victory_stats['roads']
        print('victory road: rival, camper, trainers in that order on %d; steps rival to camper %.1f, camper to first trainer %.1f'
              % (n, victory_stats['rival to camper'] / n, victory_stats['camper to trainer'] / n))
    n = trainer_stats['all']
    if n:
        share = {'blocks': trainer_stats['blocks'] / n, 'in view': (trainer_stats['blocks'] + trainer_stats['beside']) / n,
                 'turns': trainer_stats['turns'] / n}
        print('route trainers: %d; nobody can walk past %.0f%%, in view of the shortest walk but avoidable %.0f%%, '
              'off it %.0f%%; turn or look around %.0f%%'
              % (n, 100.0 * share['blocks'], 100.0 * trainer_stats['beside'] / n, 100.0 * trainer_stats['off'] / n,
                 100.0 * share['turns']))
        if count >= 20:
            for k, (lo, hi) in TRAINER_MIX.items():
                if not lo <= share[k] <= hi:
                    print('trainer mix: %s at %.0f%%, wanted %.0f to %.0f%%' % (k, 100 * share[k], 100 * lo, 100 * hi))
                    bad += 1
    n = len(chapter_stats['states'])
    if n:
        top = lambda c, k=12: ', '.join('%s: %d' % kv for kv in c.most_common(k))
        print('chapters: %s' % top(chapter_stats['split'], 20))
        print('first chapter gyms: %s' % top(chapter_stats['first']))
        print('blocks on the road: %s; placed %s; chapters with two ways on: %d'
              % (top(chapter_stats['locks']), top(chapter_stats['spot']), chapter_stats['two ways']))
        print('HM unlock order (Cut, Rock Smash, Strength by gym): %s' % top(chapter_stats['hm order']))
        print('gym giving each: %s' % ', '.join('%s %s' % (o, ' '.join('%d:%d' % (g, chapter_stats['hm gym'][(o, g)])
                                                                         for g in range(1, 9)))
                                                for o in ('CUT', 'ROCK SMASH', 'STRENGTH')))
        print('SURF from gym: %s' % ', '.join('%d: %d' % kv for kv in sorted(chapter_stats['surf gym'].items())))
        print('gyms not needed to move on: %s; badge sets a player can hold: %d to %d'
              % (', '.join('%d: %d' % kv for kv in sorted(chapter_stats['optional'].items())),
                 min(chapter_stats['states']), max(chapter_stats['states'])))
    n = rmap_stats['seams']
    if n:
        print('region map seams: %d, as a tile pair Hoenn has %.1f%%, the same pixels as a seam Hoenn has %.1f%%, '
              'smooth %.2f%%; plain green tiles %.1f a map' % (n, 100.0 * rmap_stats['pairs in hoenn'] / n,
              100.0 * rmap_stats['seams in hoenn'] / n, 100.0 * rmap_stats['smooth'] / n, rmap_stats['plain'] / rmap_stats['maps']))
    n = trait_stats['regions']
    if n:
        print('region map land: volcano drawn %d, a lake %d of %d; the professor speaks of %s'
              % (trait_stats['volcano drawn'], trait_stats['lake drawn'], n,
                 ', '.join('%s %d' % (k[5:], v) for k, v in sorted(trait_stats.items()) if k.startswith('says '))))
    print('water and ledge problems: %s' % (dict(water_stats) or 'none'))
    n = sum(seam_all.values())
    if n:
        k = sum(seam_stats.values())
        print('in view across a seam: %d of %d people and things (%.1f%%): %s'
              % (k, n, 100.0 * k / n, ', '.join('%s %d of %d' % (w, seam_stats[w], seam_all[w]) for w in sorted(seam_all))))
        if count >= 20 and k > SEAM_VIEW_MAX * n:
            print('in view across a seam: %.1f%%, wanted at most %.0f%%' % (100.0 * k / n, 100 * SEAM_VIEW_MAX))
            bad += 1
    n = dungeon_stats['regions']
    if n:
        print('dungeons: %.1f a region, %.1f floors each; %s; trainers %.1f, items %.1f a region'
              % (dungeon_stats['dungeons'] / n, dungeon_stats['floors'] / max(1, dungeon_stats['dungeons']),
                 ', '.join('%s: %d' % (k, v) for k, v in sorted(dungeon_stats.items()) if k.startswith('kind')),
                 dungeon_stats['trainers'] / n, dungeon_stats['items'] / n))
        print('dungeon floor exits: %d (each to a place of its own); tower doors: %s'
              % (dungeon_stats['floor exits'], ', '.join('%s %d' % (k[11:], v) for k, v in sorted(dungeon_stats.items())
                                                          if k.startswith('tower door '))))
        print('forests: %d in %d regions, %d of them across the road'
              % (dungeon_stats['forests'], n, dungeon_stats['forests across the road']))
        print('dungeon extras: prizes %d, choice balls %d, camps %d, lost explorers %d'
              % (dungeon_stats['extra %d' % P.OBJ_PRIZE], dungeon_stats['extra %d' % P.OBJ_CHOICE],
                 dungeon_stats['extra %d' % P.OBJ_CAMP], dungeon_stats['extra %d' % P.OBJ_EXPLORER]))
        print('peaks: %s' % ', '.join('%s %d' % kv for kv in sorted(peaks_stats.items())))
        print('ruins: %d in %d regions; %s; distinct layouts in four visits %.1f a ruins'
              % (ruins_stats['ruins'], n, ', '.join('%s %d' % kv for kv in sorted(ruins_stats.items())
                                                      if kv[0].startswith(('floors', 'pit'))),
                 ruins_stats['different layouts'] / max(1, ruins_stats['ruins'])))
        print('legends: %d in %d regions, %.1f a region; %s' % (
            dungeon_stats['legends'], n, dungeon_stats['legends'] / n,
            ', '.join('%s: %d' % (k[7:], v) for k, v in sorted(dungeon_stats.items()) if k.startswith('legend '))))
        few = sum(v for k, v in dungeon_stats.items() if k in ('legend count 1', 'legend count 2'))
        kinds = {k for k in dungeon_stats if k.startswith('legend gate ')}
        print('regions with fewer than three legends: %d of %d; gate kinds seen: %d' % (few, n, len(kinds)))
        # Most regions have three to five, and over many regions every
        # kind of gate turns up.
        if n >= 50 and (few * 10 > n or len(kinds) < 7):
            print('FAIL: too many regions with few legends, or a kind of gate never used')
            bad += 1
        print('legend lairs: %s; leader before a legend in %d regions, hideout without him %d' % (
            ', '.join('%s: %d' % (k[10:], v) for k, v in sorted(dungeon_stats.items()) if k.startswith('lair room ')),
            dungeon_stats['leader before legend'], dungeon_stats['hideout without the leader']))
        if os.environ.get('LEGEND_SETS'):
            for k, v in legend_sets.most_common():
                print('  %3d  %s' % (v, k))
        trios = Counter()
        for k, v in legend_sets.items():
            names = [x.split('@')[0] for x in k.split()]
            for t in (('Regirock', 'Regice', 'Registeel'), ('Articuno', 'Zapdos', 'Moltres'), ('Raikou', 'Entei', 'Suicune'),
                      ('Uxie', 'Mesprit', 'Azelf'), ('Cobalion', 'Terrakion', 'Virizion'), ('Tornadus', 'Thundurus', 'Landorus')):
                got = sum(x in names for x in t)
                if got:
                    trios['%s %d of 3' % (t[0][:4], got)] += v
            for x in names:
                if x in ('Latias', 'Latios', 'Heatran', 'Cresselia', 'Regigigas') or LEGEND_NAMES.index(x) >= FIRST_COVER:
                    trios[x] += v
        print('legend sets: %s' % ', '.join('%s: %d' % kv for kv in sorted(trios.items())))
        print('dungeon extras by kind (dungeons with one): %s' % ', '.join(
            '%s %d' % (k, dungeon_stats[k]) for k in sorted(dungeon_stats)
            if k.split(' ')[0] in EXTRA_NAMES.values() and k.split(' ')[1] in DUN_NAMES.values()))
    if team_stats['regions']:
        n = team_stats['regions']
        print('teams: %s' % ', '.join('%s %d (%.0f%%)' % (k, team_stats[k], 100.0 * team_stats[k] / n) for k in ('teams 1', 'teams 2', 'teams 3'))
              + '; ' + ', '.join('%s: %d' % kv for kv in sorted(team_stats.items()) if kv[0] not in ('regions', 'teams 1', 'teams 2', 'teams 3')))
    n = villain_stats['regions']
    if n:
        print('wanderer: %s' % ', '.join('%s: %d' % kv for kv in sorted(wanderer_stats.items())))
        print('villains: hideouts in %d of %d regions; %s' % (villain_stats['hideouts'], n,
              ', '.join('%s: %d' % kv for kv in sorted(villain_stats.items()) if kv[0] not in ('regions', 'hideouts'))))
        print('dungeons on the way: %d, deepest floor of the way %s; needing boulders or rocks cleared: %d; '
              'gates (pairs) %.1f a region; trainer slots used %.0f a region (max %d)'
              % (dungeon_stats['required'], ', '.join('B%sF: %d' % (k.split()[-1], v) for k, v in sorted(dungeon_stats.items())
                                                       if k.startswith('through')),
                 dungeon_stats['gated'], dungeon_stats['gates'] / n, dungeon_stats['slots'] / n, MAX_ROUTE_TRAINERS))
    n = loop_stats['regions']
    if n:
        s = sorted(loop_stats['saved'])
        print('region loops: regions with a wrap (a loop spanning 3+ gyms) %d of %d (%.0f%%): tunnels %d, ring roads or long '
              'shortcuts %d; opens with badges %s, by 5 badges %d; places saved median %s mean %.1f; main road places on a '
              'loop %.0f%%; places used mean %.1f (42: %d); tunnel floors checked %d; loops 3+ gyms long opening more than %d '
              'stretch before their far end: tunnels %d, side ways %d'
              % (loop_stats['wrap'], n, 100.0 * loop_stats['wrap'] / n, loop_stats['tunnel'], loop_stats['ring'],
                 ', '.join('%s: %d' % kv for kv in sorted(loop_stats['opens'].items(), key=lambda kv: (kv[0] is None, kv[0]))),
                 loop_stats['by5'], s[len(s) // 2] if s else '-', sum(s) / max(1, len(s)),
                 100.0 * loop_stats['road on loop'] / max(1, loop_stats['road']),
                 sum(loop_stats['places']) / n, loop_stats['places'].count(42), tunnel_stats['floors'],
                 LOOP_AHEAD, loop_stats['early tunnel'], loop_stats['early side']))
    print('ice and volcano: %s' % ', '.join('%s: %d' % kv for kv in sorted(ice_stats.items())))
    for fl, label in ((1, 'first floor'), (2, 'floors below')):
        print('ice puzzle moves, %s: %s' % (label, ' '.join('%d:%d' % (m, ice_moves_stats[(fl, m)])
                                                         for m in sorted(k[1] for k in ice_moves_stats if k[0] == fl))))
    print('finds out on the ice: %d; moves to reach: %s' % (sum(ice_prize_stats.values()),
                                                          ' '.join('%d:%d' % kv for kv in sorted(ice_prize_stats.items()))))
    print('special houses: %d, without a sign: %d' % (house_stats['special'], house_stats['unsigned']))
    print('hidden items: %d (%.1f a region)' % (hidden_stats['n'], hidden_stats['n'] / max(1, count)))
    print('spring towns: %d, with red rock: %d; red rock pairs %d, never in Hoenn %d'
          % (red_stats['towns'], red_stats['with rock'], red_stats['pairs'], red_stats['bad']))
    print('beach towns: %d; ordinary tree blocks on sand: %d; blocks where the two kinds of tree touch: %d'
          % (beach_stats['towns'], beach_stats['ordinary on sand'], beach_stats['kinds touching']))
    if truck_stats['towns']:
        print('moving truck (after STOP): %d first towns, all of it clear in %d (the first rule: %d), moved in %d%s'
              % (truck_stats['towns'], truck_stats['clear'], truck_stats['old clear'], truck_stats['moved'],
                 '; its top over something in seeds %s' % truck_stats['top over'][:20] if truck_stats['top over'] else ''))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
