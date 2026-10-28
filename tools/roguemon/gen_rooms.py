#!/usr/bin/env python3
"""Vanilla rooms for the floors of towers, hideouts and lairs: src/data/rogue_rooms.h.

Reads Hoenn's Mt. Pyre and Aqua and Magma Hideout maps, and the legends'
chambers (the Regis' ruins, Terra and Marine Cave's ends, Sky Pillar's top,
Navel Rock, Southern Island and others), and writes
each usable floor: its blocks, its warps by role and the spots its own objects
stand on, for RogueGen_GenerateFloor (src/rogue_gen_rooms.h).

Warp roles: BACK leads toward the way in (the floor before, or out to the
town on the ground floor), ON to the next floor, PAD to another warp pad of
the same room (Aqua Hideout's). Holes into the floor below are filled in: the
floors of a generated tower are not stacked as in Hoenn. (Sky Pillar was
tried: even its clean floors are only climbed by falling through cracks.)

Spots: the vanilla trainers', item balls' and people's places, the boss's
place on a last floor (the farthest walk from the way in, or the legend's or
leader's own), then a few open places spread round the room for when a floor
needs more. A lair (a legend's chamber, one room each) also has the place the
villain team's leader stands, on the walk to the legend, a narrow place if
there is one. No spot closes a way: every warp and spot stays reachable with
an object on every spot.

  gen_rooms.py            write the header
  gen_rooms.py --png DIR  also render every room with its warps and spots
"""
import argparse
import json
import os
import re
import struct
from collections import deque

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT = os.path.join(ROOT, 'src', 'data', 'rogue_rooms.h')
MAX_WARPS = 12  # ROGUE_MAX_WARPS
MAX_SPOTS = 16
MAX_BLOCKS, MAX_DATA = 1600, 3600  # ROGUE_MAX_BLOCKS, MAX_MAP_DATA_SIZE

# Each style is a dungeon of Hoenn: the maps in the order they are climbed or
# descended, and which of them serve as ground, middle and last floors.
STYLES = [
    # The top of a tower is Mt. Pyre's summit, out in the open by the orb
    # shrine, climbed onto from the floor below (6F, a dead end in Hoenn,
    # has no stairs on).
    ('PYRE', 'tower', ['MtPyre_1F', 'MtPyre_2F', 'MtPyre_3F', 'MtPyre_4F', 'MtPyre_5F', 'MtPyre_6F', 'MtPyre_Summit'],
     {'ground': ['MtPyre_1F'], 'middle': ['MtPyre_2F', 'MtPyre_3F', 'MtPyre_4F', 'MtPyre_5F'], 'last': ['MtPyre_Summit']}),
    # Team Aqua's own: Ruby's Aqua Hideout (left unused in Emerald), its
    # halls round the pools and the submarine dock, wet as the team is. Its
    # 1F was entered by Surf: here a warp panel by the pool is the way in
    # (SYNTH gives Ruby's maps the warps they lost).
    ('AQUA', 'hideout', ['AquaHideout_UnusedRubyMap1', 'AquaHideout_UnusedRubyMap3'],
     {'ground': ['AquaHideout_UnusedRubyMap1'], 'middle': [], 'last': ['AquaHideout_UnusedRubyMap3']}),
    # Team Magma's volcanic halls; a hideout may start on 1F or on 2F's
    # first room (down its stairs from the town's door), so they differ.
    ('MAGMA', 'hideout', ['MagmaHideout_1F', 'MagmaHideout_2F_1R', 'MagmaHideout_2F_2R', 'MagmaHideout_2F_3R',
                          'MagmaHideout_3F_1R', 'MagmaHideout_3F_2R', 'MagmaHideout_3F_3R', 'MagmaHideout_4F'],
     # (2F_3R and 3F_3R are bare ladder shafts, and 4F is too big.)
     {'ground': ['MagmaHideout_1F', 'MagmaHideout_2F_1R'], 'middle': ['MagmaHideout_2F_1R', 'MagmaHideout_3F_1R'],
      'last': ['MagmaHideout_2F_2R', 'MagmaHideout_3F_2R']}),
]
# The legends' lairs: one room each, its own style, so a legend picks its
# chamber (src/rogue_gen_legend.h).
LAIRS = [
    ('RUINS', 'DesertRuins'), ('ISLAND_CAVE', 'IslandCave'), ('TOMB', 'AncientTomb'),
    ('CHAMBER', 'SealedChamber_InnerRoom'), ('TERRA', 'TerraCave_End'), ('MARINE', 'MarineCave_End'),
    ('PEAK', 'NavelRock_Top'), ('DEPTHS', 'NavelRock_Bottom'), ('PILLAR', 'SkyPillar_Top'),
    ('ORIGIN', 'CaveOfOrigin_B1F'), ('HOLLOW', 'MeteorFalls_StevensCave'), ('DEN', 'GraniteCave_StevensRoom'),
    ('ISLE', 'SouthernIsland_Interior'), ('GLADE', 'FarawayIsland_Interior'),
]
STYLES += [(style, 'lair', [name], {'ground': [], 'middle': [], 'last': [name]}) for style, name in LAIRS]
# Ruins (RDUN_RUINS, src/rogue_gen_ruins.h), as Desert Ruins, the Mirage
# Tower and Sky Pillar: the Mirage Tower's sandy floors, in an order that
# changes on every visit, their cracked floors kept (sand pits: a fall drops
# the player a floor), and at the bottom a vault, a Regi chamber whose
# inner door stays shut until the riddle on its three tablets is solved.
STYLES += [
    ('SANDS', 'ruins', ['MirageTower_1F', 'MirageTower_2F', 'MirageTower_3F', 'MirageTower_4F'],
     {'ground': [], 'middle': ['MirageTower_1F', 'MirageTower_2F', 'MirageTower_3F'], 'last': []}),
    ('VAULT', 'ruins', ['DesertRuins', 'IslandCave', 'AncientTomb'],
     {'ground': [], 'middle': [], 'last': ['DesertRuins', 'IslandCave', 'AncientTomb']}),
]
# Team Rocket (who has no hideout in Hoenn): a plain indoor base, Emerald's
# Aqua Hideout B1F and B2F with no water, or Ruby's B1F, the store rooms
# and offices off the warp panels. Last in the list, so the other styles
# keep their numbers.
ROCKET_STYLE = ('ROCKET', 'hideout', ['AquaHideout_UnusedRubyMap2', 'AquaHideout_B1F', 'AquaHideout_B2F'],
                {'ground': ['AquaHideout_UnusedRubyMap2', 'AquaHideout_B1F'], 'middle': [], 'last': ['AquaHideout_B2F']})
# Ruby's Aqua Hideout maps kept their blocks but lost their warps: the way
# in (back), the ways on (on) are given here; the room's warp panels are
# paired up to join its parts (auto pads), the rest filled in, and stairs
# left over walled up like the wall beside them. 'pad' is a warp panel's
# block (Ruby's B1F's) put where the way in stands.
SYNTH = {
    'AquaHideout_UnusedRubyMap1': {'back': (13, 25), 'pad': 0x32DF, 'on': [(13, 1)]},
    'AquaHideout_UnusedRubyMap2': {'back': (3, 3), 'on': [(23, 1)]},
    'AquaHideout_UnusedRubyMap3': {'back': (18, 1), 'on': []},
}
# Rooms that two styles share are named apart in the header (and a room
# that is both a style's ground and middle floor, by its use).
CNAME_SUFFIX = {'VAULT': '_Vault', 'MIRAGE': '_Mirage'}
# A shipwreck (RDUN_SHIP) is Route 108's Abandoned Ship, climbed down from its
# deck: the deck and the captain's office, the corridors of 1F and B1F with
# the cabins off them, and the hidden deck at the bottom, where four cabins
# open one after another with keys found in each other (Hoenn's Rooms 1, 3,
# 4 and 6). Each floor is one map made of the ship's maps side by side
# (COMPOSE), their doors joined as warp pads, so every floor fits the
# floors' slots. Its middle floors keep their order (1F, then B1F).
# A power plant (RDUN_PLANT) is New Mauville's generator hall as it is, its
# blue and green switches raising and lowering barriers (SWITCHES).
STYLES += [
    ('SHIP', 'ship', ['Ship_Deck', 'Ship_1F', 'Ship_B1F', 'Ship_Hold'],
     {'ground': ['Ship_Deck'], 'middle': ['Ship_1F', 'Ship_B1F'], 'last': ['Ship_Hold']}),
    ('PLANT', 'plant', ['NewMauville_Inside'], {'ground': [], 'middle': [], 'last': ['NewMauville_Inside']}),
]
STYLES += [ROCKET_STYLE]
# (v1.1 peaks) A tower in the desert (RDUN_MIRAGE, src/rogue_gen_peaks.h):
# the Mirage Tower itself, climbed from its door in the sand up its ladders
# floor by floor to the top, where its fossils wait. Its cracked floor is
# kept as in ruins (sand pits: a fall drops the player a floor), and a fall
# lands near the ladder back up. Last, so the other styles keep their numbers.
STYLES += [
    ('MIRAGE', 'mirage', ['MirageTower_1F', 'MirageTower_2F', 'MirageTower_3F', 'MirageTower_4F'],
     {'ground': ['MirageTower_1F'], 'middle': ['MirageTower_2F', 'MirageTower_3F'], 'last': ['MirageTower_4F']}),
]
# The level of each map in its dungeon, for telling stairs on from stairs back
# (Magma Hideout's rooms side by side share a level).
LEVEL = {'MagmaHideout_1F': 0, 'MagmaHideout_2F_1R': 1, 'MagmaHideout_2F_2R': 1, 'MagmaHideout_2F_3R': 1,
         'MagmaHideout_3F_1R': 2, 'MagmaHideout_3F_2R': 2, 'MagmaHideout_3F_3R': 2, 'MagmaHideout_4F': 3,
         'AbandonedShip_Deck': 0, 'AbandonedShip_CaptainsOffice': 0,
         'AbandonedShip_Corridors_1F': 1, 'AbandonedShip_Rooms_1F': 1, 'AbandonedShip_Rooms2_1F': 1,
         'AbandonedShip_Corridors_B1F': 2, 'AbandonedShip_Rooms_B1F': 2, 'AbandonedShip_Rooms2_B1F': 2,
         'AbandonedShip_Room_B1F': 2, 'AbandonedShip_HiddenFloorCorridors': 3, 'AbandonedShip_HiddenFloorRooms': 3}
# Floors made of several maps: each part is a map, the rectangle of it kept
# (x, y, w, h) and where it goes (x, y), with void between the parts, wide
# enough that a cabin shows nothing of the corridor beside it. A door whose
# other side is kept becomes a warp pad to it; one whose other side is left
# out is dropped, its blocks sealed ('seal', in the floor's own coordinates).
# 'role' turns a stair the other way, 'add' adds a stair, 'shut' marks a
# door that stays shut (a sign says so), 'cabins' are the corridor doors of
# cabins that can hold a key, and 'locks' the doors locked by one (the
# second value TRUE for a door always locked: B1F's storage room). On the
# hidden deck every cabin can be locked and the boss waits in the last one
# opened ('holds'). Values are whole blocks: metatile, collision, elevation.
SHIP_VOID = 0x0601
COMPOSE = {
    # The deck: up the stairs from the sea and in at the door ahead, as in
    # Hoenn (the far door and the captain's office are out of reach past
    # the holes in the deck).
    'Ship_Deck': {'parts': [('AbandonedShip_Deck', (0, 0, 23, 21), (0, 0))],
                  'void': 0x0170, 'drop': [(8, 9)]},
    # The corridor on the right (the left one leads only to its own cabins)
    # and the four cabins off it. The top right cabin is reached through the
    # one below it, as in Hoenn, so its own door is dropped (the warp slots)
    # and stays shut. The corridor's door down into that cabin is dropped
    # too, its arrow step sealed as the plain edge of the cabin's roof.
    'Ship_1F': {'parts': [('AbandonedShip_Corridors_1F', (8, 0, 10, 12), (0, 0)),
                          ('AbandonedShip_Rooms_1F', (0, 0, 18, 17), (18, 0))],
                'drop': [(6, 3), (31, 1)], 'shut': [(31, 1)], 'seal': {(6, 3): 0x3219}, 'void': SHIP_VOID},
    # The corridor, the three cabins below it (one flooded) and the storage
    # room, locked; the two cabins above are left out, their doors locked
    # for good. Its second stair leads on down to the hidden deck.
    'Ship_B1F': {'parts': [('AbandonedShip_Corridors_B1F', (0, 0, 13, 10), (0, 0)),
                           ('AbandonedShip_Rooms_B1F', (0, 0, 27, 8), (21, 0)),
                           ('AbandonedShip_Room_B1F', (0, 0, 9, 8), (21, 16))],
                 'seal': {(3, 4): 0x3633, (6, 3): 0x0623, (6, 4): 0x3633},
                 'role': {(8, 2): 'ON'}, 'cabins': [(5, 7), (8, 7), (11, 7), (11, 4)], 'locks': [(11, 4, True)],
                 'void': SHIP_VOID},
    # The hidden deck: its corridor and the three cabins below it (Rooms 4,
    # 5 and 6), each entered by a door in its front wall, a stair back up in
    # the corner, Corridors B1F's. The cabins above are left out: their doors
    # are only dips in the corridor's roof edge (Hoenn's DoorIndent tiles,
    # locked and open alike), which players walked past without seeing.
    'Ship_Hold': {'parts': [('AbandonedShip_HiddenFloorCorridors', (0, 0, 13, 11), (0, 0)),
                            ('AbandonedShip_HiddenFloorRooms', (0, 8, 15, 7), (21, 0)),
                            ('AbandonedShip_HiddenFloorRooms', (15, 8, 14, 7), (44, 0)),
                            ('AbandonedShip_HiddenFloorRooms', (29, 8, 15, 7), (66, 0))],
                  'seal': {(3, 3): 0x3234, (6, 3): 0x3234, (9, 3): 0x3234,
                           (11, 0): 0x0621, (12, 0): 0x0621, (11, 1): 0x0629, (12, 1): 0x069F,
                           (11, 2): 0x32A6, (12, 2): 0x06A7},
                  'add': [(11, 2, 'BACK')], 'cabins': [(3, 8), (6, 8), (9, 8)],
                  'locks': [(3, 8, False), (6, 8, False), (9, 8, False)], 'holds': True,
                  'void': SHIP_VOID},
}
# A floor has one way back (and on a ground floor one way out): a second
# door or stair back would only lead to the same place. 'seal' puts the wall
# or floor round such a door over it, with blocks from the room itself (or
# from the same rock rim elsewhere in the hideout), in place of the door.
OPTIONS = {
    # B1F's warp maze stands apart on the right: cut it off and join its two
    # ends straight, so the room fits the warp slots and the map width.
    'AquaHideout_B1F': {'crop': 34, 'relink': {9: 11, 11: 9}},
    # The left room's door back up, beside the stairs back: wall like the
    # rest of the room's top wall.
    'AquaHideout_B2F': {'boss': (23, 19), 'seal': {(2, 2): 0x06E0, (3, 2): 0x06E0, (4, 2): 0x06E0,
                                                   (2, 3): 0x0621, (3, 3): 0x0621, (4, 3): 0x0621,
                                                   (2, 4): 0x3229, (4, 4): 0x3229}},
    # Mt. Pyre's back door out to the mountain, at the end of the wing on
    # the left: the hall without its wing, as the floors above are.
    'MtPyre_1F': {'crop_x': 9, 'crop': 13},
    # Magma Hideout's doors over to the side rooms, which a floor here does
    # not have: rock wall, as on either side. The way in is the door from
    # the floor below in Hoenn, so the floor is crossed to the stairs on.
    # Ruby's dock room: the boss on the walk above the pool, clear of the
    # panels at either end of it.
    'AquaHideout_UnusedRubyMap3': {'boss': (21, 12)},
    # The pool below the way in, where Hoenn's players surfed in from the
    # sea: still water here (solid), so its arrow out is never reached.
    'AquaHideout_UnusedRubyMap1': {'seal': {(12, 26): 0x1580, (13, 26): 0x1589, (14, 26): 0x1581,
                                            (12, 27): 0x1590, (13, 27): 0x1667, (14, 27): 0x1592}},
    'MagmaHideout_2F_1R': {'seal': {(11, 22): 0x0674, (11, 23): 0x0674}},
    'MagmaHideout_3F_1R': {'seal': {(21, 8): 0x0674, (21, 9): 0x0674}},
    # A hole down off the ledge: plain floor and the rock rim (as in 3F_1R).
    'MagmaHideout_2F_2R': {'seal': {(36, 4): 0x03A2, (35, 5): 0x0669, (36, 5): 0x0669, (37, 5): 0x0669}},
    # The summit is wider than a floor may be: its middle, with the shrine
    # (where Maxie and Archie stood) for the master.
    'MtPyre_Summit': {'crop_x': 5, 'crop': 40, 'boss': (23, 6)},
    # Lairs whose legend stood in lava or water: the ground nearest it.
    'TerraCave_End': {'boss_near': (17, 22)},
    'MarineCave_End': {'boss_near': (9, 22)},
    'SealedChamber_InnerRoom': {'boss': (10, 5)},
    # Mew stood by the way in; the legend waits across the meadow, on the
    # open ground under the trees, where the long grass does not hide it
    # (or the statue that calls it).
    'FarawayIsland_Interior': {'boss_near': (14, 6)},
    # New Mauville whole (41x41: more blocks than a slot holds, so the game
    # draws it from ROM), the guardian by the generator.
    'NewMauville_Inside': {'switches': 'NewMauville_Inside', 'boss': (34, 5), 'big': True},
    # (v1.1 peaks) The Mirage Tower's top: the guardian on the raised floor
    # behind its two fossils, which are the choice row (one may be taken).
    'MirageTower_4F': {'boss': (6, 2), 'choice': [(5, 4), (7, 4)]},
}
# Objects that stand for a Pokemon disguised as an item ball (New
# Mauville's Voltorb): a DECOY spot.
DECOY_SCRIPT = re.compile(r'Voltorb\d')
# Objects that are no place for a person.
NOT_SPOTS = re.compile(r'BOULDER|SUBMARINE|GROUDON|KYOGRE|RAYQUAZA_STILL|CUTTABLE|BREAKABLE|DEOXYS|FOSSIL')
DIRS = {'UP': 0, 'RIGHT': 1, 'DOWN': 2, 'LEFT': 3}
TURNS, LOOKS = 0x10, 0x20
DELTA = [(0, -1), (1, 0), (0, 1), (-1, 0)]


def behaviour_ids():
    src = open(os.path.join(ROOT, 'include', 'constants', 'metatile_behaviors.h')).read()
    body = src.split('enum {', 1)[1].split('}', 1)[0]
    ids, n = {}, 0
    for line in body.split('\n'):
        m = re.match(r'\s*(MB_\w+)(\s*=\s*(\w+))?', line)
        if m:
            if m[3]:
                n = int(m[3], 0)
            ids[m[1]] = n
            n += 1
    return ids


MB = behaviour_ids()
WARP_MB = {MB[k] for k in ('MB_NON_ANIMATED_DOOR', 'MB_LADDER', 'MB_EAST_ARROW_WARP', 'MB_WEST_ARROW_WARP',
                          'MB_NORTH_ARROW_WARP', 'MB_SOUTH_ARROW_WARP', 'MB_AQUA_HIDEOUT_WARP', 'MB_ANIMATED_DOOR',
                          'MB_LAVARIDGE_GYM_1F_WARP', 'MB_LAVARIDGE_GYM_B1F_WARP', 'MB_UP_ESCALATOR', 'MB_DOWN_ESCALATOR',
                          'MB_UP_RIGHT_STAIR_WARP', 'MB_UP_LEFT_STAIR_WARP', 'MB_DOWN_RIGHT_STAIR_WARP',
                          'MB_DOWN_LEFT_STAIR_WARP')}
HOLE_MB = {MB['MB_MT_PYRE_HOLE'], MB['MB_CRACKED_FLOOR_HOLE']}
CRACK_MB = MB['MB_CRACKED_FLOOR']
PAD_MB = {MB['MB_AQUA_HIDEOUT_WARP'], MB['MB_LAVARIDGE_GYM_1F_WARP'], MB['MB_LAVARIDGE_GYM_B1F_WARP']}
JUMP = {MB['MB_JUMP_EAST']: (1, 0), MB['MB_JUMP_WEST']: (-1, 0), MB['MB_JUMP_NORTH']: (0, -1), MB['MB_JUMP_SOUTH']: (0, 1)}


def load_layouts():
    data = json.load(open(os.path.join(ROOT, 'data', 'layouts', 'layouts.json')))
    return {l['name'][:-len('_Layout')]: l for l in data['layouts'] if 'name' in l}


def tileset_dir(sym):
    # gTileset_Facility -> facility, gTileset_InsideShip -> inside_ship
    return re.sub(r'(?<!^)(?=[A-Z])', '_', sym[len('gTileset_'):]).lower()


_attr_cache = {}


def behaviours(secondary):
    if secondary not in _attr_cache:
        out = []
        for kind, name, n in (('primary', 'general', 512), ('secondary', secondary, None)):
            data = open(os.path.join(ROOT, 'data', 'tilesets', kind, name, 'metatile_attributes.bin'), 'rb').read()
            attrs = [a & 0xFF for a in struct.unpack('<%dH' % (len(data) // 2), data)]
            out += (attrs + [0] * 512)[:n] if n else attrs
        _attr_cache[secondary] = out
    return _attr_cache[secondary]


def layout_blocks(lay):
    data = open(os.path.join(ROOT, lay['blockdata_filepath']), 'rb').read()
    return list(struct.unpack('<%dH' % (len(data) // 2), data))


_labels = None


def metatile_label(name):
    global _labels
    if _labels is None:
        src = open(os.path.join(ROOT, 'include', 'constants', 'metatile_labels.h')).read()
        _labels = {m[1]: int(m[2], 0) for m in re.finditer(r'#define (METATILE_\w+)\s+(0x[0-9A-Fa-f]+)', src)}
    return _labels[name]


# The ship's locked doors (Abandoned Ship's): a door in a cabin's front wall
# and a door set in the roof of the corridor, each locked and unlocked.
DOOR_UNLOCKED = {0x233: 0x22B, 0x234: 0x21A}


class Room:
    def __init__(self, style, use, name, layouts):
        self.style, self.use, self.name = style, use, name
        self.cname = name + CNAME_SUFFIX.get(style, '')
        uses = next(u for st, _, _, u in STYLES if st == style)
        if sum(name in v for v in uses.values()) > 1:
            self.cname += '_' + use.capitalize()
        if style == 'ROCKET' and name.startswith('AquaHideout_B'):
            self.cname += '_Rocket'
        # Cells no walk may count on: a ruins room's cracked floor, which
        # gives way under a second step (the player falls a floor).
        self.avoid = frozenset()
        self.parts = None
        if name in COMPOSE:
            self.compose(layouts)
            return
        opts = OPTIONS.get(name, {})
        self.map = json.load(open(os.path.join(ROOT, 'data', 'maps', name, 'map.json')))
        lay = layouts[opts.get('layout', name)]
        self.layout_name = opts.get('layout', name)
        self.layout_id = lay['id']
        self.tileset = tileset_dir(lay['secondary_tileset'])
        self.map_type = self.map['map_type']
        w, h = lay['width'], lay['height']
        data = open(os.path.join(ROOT, lay['blockdata_filepath']), 'rb').read()
        blocks = list(struct.unpack('<%dH' % (len(data) // 2), data))
        cw = opts.get('crop', w)
        x0 = opts.get('crop_x', 0)
        self.w, self.h = cw, h
        self.blocks = [blocks[y * w + x0 + x] for y in range(h) for x in range(cw)]
        # Cropped from the left: everything on the map moves with it.
        for e in self.map.get('warp_events', []) + self.map.get('object_events', []):
            e['x'] -= x0
        if 'boss' in opts:
            opts = dict(opts, boss=(opts['boss'][0] - x0, opts['boss'][1]))
        for (x, y), block in opts.get('seal', {}).items():
            self.blocks[y * cw + x - x0] = block
        self.mb = behaviours(self.tileset)
        self.opts = opts
        if opts.get('switches'):
            self.switch_states(opts['switches'])
        if name in SYNTH:
            self.synth(SYNTH[name], next(o for st, _, o, _ in STYLES if st == style))

    def synth(self, sy, order):
        """Warps for a map that lost them (SYNTH): the way in, the ways on,
        and warp panels paired to join the room's parts into one, from the
        way in outwards (each part reached by the nearest free panel of the
        parts already joined); panels left over are filled in, stairs left
        over walled up."""
        me = order.index(self.name)
        nxt = order[me + 1] if me + 1 < len(order) else None
        bx, by = sy['back']
        if sy.get('pad'):
            self.blocks[by * self.w + bx] = sy['pad']
        ev = [{'x': bx, 'y': by, 'dest_map': 'MAP_NONE', 'dest_warp_id': '0'}]
        for x, y in sy.get('on', []):
            ev.append({'x': x, 'y': y, 'dest_map': map_id(nxt), 'dest_warp_id': '0'})
        used = {(e['x'], e['y']) for e in ev}
        pads = [(x, y) for y in range(self.h) for x in range(self.w)
                if self.beh(x, y) in PAD_MB and (x, y) not in used]
        stairs = [(x, y) for y in range(self.h) for x in range(self.w)
                  if self.beh(x, y) in WARP_MB and self.beh(x, y) not in PAD_MB and (x, y) not in used]
        for x, y in stairs:
            self.blocks[y * self.w + x] = self.at(x - 1, y) if x > 0 else self.at(x + 1, y)
        part = {}
        for c in pads + [(bx, by)]:
            if c not in part:
                cells = walk(self, [c], [])
                k = len(set(part.values()))
                part[c] = k
                for p in pads + [(bx, by)]:
                    if p not in part and touches(p, cells):
                        part[p] = k
        joined, free, pairs = {part[(bx, by)]}, set(pads), []
        while True:
            best = None
            for a in sorted(free):
                if part[a] not in joined:
                    continue
                for b in sorted(free):
                    if part[b] in joined:
                        continue
                    d = abs(a[0] - b[0]) + abs(a[1] - b[1])
                    if best is None or d < best[0]:
                        best = (d, a, b)
            if best is None:
                break
            _, a, b = best
            free -= {a, b}
            joined.add(part[b])
            pairs.append((a, b))
        for a, b in pairs:
            i = len(ev)
            ev.append({'x': a[0], 'y': a[1], 'dest_map': map_id(self.name), 'dest_warp_id': str(i + 1)})
            ev.append({'x': b[0], 'y': b[1], 'dest_map': map_id(self.name), 'dest_warp_id': str(i)})
        for x, y in free:
            fill_hole(self, x, y)
        self.map['warp_events'] = ev

    def compose(self, layouts):
        """A floor of several maps side by side (COMPOSE)."""
        c = COMPOSE[self.name]
        self.opts = dict(c)
        first = c['parts'][0][0]
        lay = layouts[first]
        self.layout_name, self.layout_id = first, lay['id']
        self.tileset = tileset_dir(lay['secondary_tileset'])
        self.map_type = json.load(open(os.path.join(ROOT, 'data', 'maps', first, 'map.json')))['map_type']
        self.w = max(dx + pw for _, (_, _, pw, _), (dx, _) in c['parts'])
        self.h = max(dy + ph for _, (_, _, _, ph), (_, dy) in c['parts'])
        self.blocks = [c['void']] * (self.w * self.h)
        self.parts, objects = [], []
        for name, (x0, y0, pw, ph), (dx, dy) in c['parts']:
            lay = layouts[name]
            full = layout_blocks(lay)
            for y in range(ph):
                for x in range(pw):
                    self.blocks[(dy + y) * self.w + dx + x] = full[(y0 + y) * lay['width'] + x0 + x]
            m = json.load(open(os.path.join(ROOT, 'data', 'maps', name, 'map.json')))
            for o in m.get('object_events', []):
                if x0 <= o['x'] < x0 + pw and y0 <= o['y'] < y0 + ph:
                    objects.append(dict(o, x=o['x'] - x0 + dx, y=o['y'] - y0 + dy))
            self.parts.append((name, x0, y0, pw, ph, dx, dy, m))
        self.map = {'object_events': objects}
        for (x, y), block in c.get('seal', {}).items():
            self.blocks[y * self.w + x] = block
        # A door that may be locked is drawn open: the game locks it while
        # its key is still to find (Rogue_ApplyFloorState).
        for x, y, _ in c.get('locks', []):
            b = self.at(x, y)
            if b & 0x3FF in DOOR_UNLOCKED:
                self.blocks[y * self.w + x] = (b & ~0x3FF) | DOOR_UNLOCKED[b & 0x3FF]
        self.mb = behaviours(self.tileset)

    def switch_states(self, name):
        """New Mauville's barriers: the blocks with no switch pressed (as the
        map is), after a blue one and after a green one (its own scripts'
        setmetatiles), and which switch leads to which state. A blue switch
        works while VAR_TEMP_1 is 0 (not just after a blue one), a green
        one while VAR_TEMP_2 is."""
        src = open(os.path.join(ROOT, 'data', 'maps', name, 'scripts.inc')).read()

        def state(label):
            body = src.split('%s_EventScript_%s::' % (name, label), 1)[1].split('\treturn', 1)[0]
            b = list(self.blocks)
            for x, y, m, c in re.findall(r'setmetatile (\d+), (\d+), (METATILE_\w+), (TRUE|FALSE)', body):
                i = int(y) * self.w + int(x)
                b[i] = (b[i] & 0xF000) | metatile_label(m) | (0x0C00 if c == 'TRUE' else 0)
            return b
        self.states = [list(self.blocks), state('SetBarrierStateBlueButton'), state('SetBarrierStateGreenButton')]
        self.triggers = {}
        for e in json.load(open(os.path.join(ROOT, 'data', 'maps', name, 'map.json')))['coord_events']:
            if e['var'] == 'VAR_TEMP_1':
                self.triggers[(e['x'], e['y'])] = ((0, 2), 1)
            elif e['var'] == 'VAR_TEMP_2':
                self.triggers[(e['x'], e['y'])] = ((0, 1), 2)

    def at(self, x, y):
        return self.blocks[y * self.w + x]

    def beh(self, x, y):
        return self.mb[self.at(x, y) & 0x3FF]

    def inside(self, x, y):
        return 0 <= x < self.w and 0 <= y < self.h


def map_id(name):
    return json.load(open(os.path.join(ROOT, 'data', 'maps', name, 'map.json')))['id']


def classify(room, order):
    """The room's warps by role, holes filled in. Back warps come first (the
    way in at index 0), then the ways on, then warp pads."""
    me = LEVEL.get(room.name, order.index(room.name))
    warps = room.map['warp_events']
    relink = room.opts.get('relink', {})
    back, on, pads, dropped = [], [], [], []
    for i, w in enumerate(warps):
        x, y = w['x'], w['y']
        if not room.inside(x, y):
            dropped.append(i)
            continue
        b = room.beh(x, y)
        target = next((n for n in order if map_id(n) == w['dest_map']), None)
        if b in HOLE_MB:
            fill_hole(room, x, y)
            dropped.append(i)
            continue
        if b not in WARP_MB:
            dropped.append(i)  # where a fall from above lands
            continue
        if target == room.name:
            pair = relink.get(i, int(w['dest_warp_id']))
            pads.append((i, x, y, pair))
        elif target is None:
            back.append((i, x, y, 'out'))
        elif LEVEL.get(target, order.index(target)) > me:
            on.append((i, x, y, None))
        else:
            back.append((i, x, y, 'back'))
    # The way in: the main door (lowest on the map) on a ground floor, else
    # the stairs that lead back the vanilla way.
    back.sort(key=lambda t: (t[3] != 'out', -t[2] if t[3] == 'out' else 0, t[0]))
    keep_pads = [p for p in pads if p[0] in relink or any(q[0] == p[3] for q in pads)]
    keep_pads = [p for p in keep_pads if any(q[0] == p[3] for q in keep_pads)]
    out = [(x, y, 'BACK', 0) for _, x, y, _ in back] + [(x, y, 'ON', 0) for _, x, y, _ in on]
    index = {}
    for k, (i, x, y, pair) in enumerate(keep_pads):
        index[i] = len(out) + k
    for i, x, y, pair in keep_pads:
        out.append((x, y, 'PAD', pair))
    out = [(x, y, role, index[pair] if role == 'PAD' else 0) for x, y, role, pair in out]
    # Pads with no partner left are dead tiles: fill them in.
    for i, x, y, pair in pads:
        if i not in index:
            fill_hole(room, x, y)
    return out


def classify_composed(room):
    """A composed floor's warps by role: a door whose other side is on the
    floor is a pad to it, a stair to another level BACK or ON (Route 108's
    stairs down to the sea lead out), a door to a map left out is dropped."""
    c = COMPOSE[room.name]
    level = LEVEL[room.parts[0][0]]
    names = {p[0] for p in room.parts}
    ids = {json.load(open(os.path.join(ROOT, 'data', 'maps', n, 'map.json')))['id']: n for n in LEVEL}
    where, raw = {}, []
    for name, x0, y0, pw, ph, dx, dy, m in room.parts:
        for i, w in enumerate(m.get('warp_events', [])):
            if x0 <= w['x'] < x0 + pw and y0 <= w['y'] < y0 + ph:
                pos = (w['x'] - x0 + dx, w['y'] - y0 + dy)
                where[(name, i)] = pos
                raw.append((pos, w))
    drop = set(c.get('drop', []))
    back, on, pads = [], [], []
    for pos, w in raw:
        if pos in drop:
            continue
        dest = ids.get(w['dest_map'])
        if dest in names:
            target = where.get((dest, int(w['dest_warp_id'])))
            if target is not None and target not in drop:
                pads.append((pos, target))
            continue
        role = c.get('role', {}).get(pos)
        if role is None and dest is None:
            role = 'BACK'  # out of the ship
        elif role is None and LEVEL[dest] != level:
            role = 'ON' if LEVEL[dest] > level else 'BACK'
        if role:
            (on if role == 'ON' else back).append(pos)
    for x, y, role in c.get('add', []):
        (on if role == 'ON' else back).append((x, y))
    # The way in first: the lowest of the ways back, the leftmost.
    back.sort(key=lambda p: (-p[1], p[0]))
    out = [(x, y, 'BACK', 0) for x, y in back] + [(x, y, 'ON', 0) for x, y in on]
    index = {}
    for k, (pos, _) in enumerate(pads):
        index[pos] = len(out) + k
    for pos, target in pads:
        out.append((pos[0], pos[1], 'PAD', target))
    return [(x, y, role, index[pair] if role == 'PAD' else 0) for x, y, role, pair in out]


# Magma Hideout's ways between floors are gaps in the rock rim below a
# floor block (0x20D-0x20F under a south arrow warp), which read as a rock
# on a floor going down. A way on gets Mt. Chimney's metal stairs down to
# Jagged Pass instead (0x216, a south arrow warp set in the rim, as there),
# the warp moved onto them; the gap's sides take the rim beside them.
RIM_GAP = (0x20D, 0x20E, 0x20F)
STAIRS_DOWN = 0x216


def stairs_down(room):
    if room.tileset != 'lavaridge':
        return
    for k, (x, y, role, pair) in enumerate(room.warps):
        if role != 'ON' or not room.inside(x - 2, y + 1) or not room.inside(x + 1, y + 1) \
                or tuple(room.at(x + dx, y + 1) & 0x3FF for dx in (-1, 0, 1)) != RIM_GAP:
            continue
        warp = room.at(x, y)
        rim = room.at(x - 2, y + 1) & 0x3FF
        for dx in (-1, 1):
            room.blocks[(y + 1) * room.w + x + dx] = (room.at(x + dx, y + 1) & ~0x3FF) | rim
        room.blocks[(y + 1) * room.w + x] = (warp & 0xF000) | STAIRS_DOWN
        fill_hole(room, x, y)
        room.warps[k] = (x, y + 1, role, pair)


def fill_hole(room, x, y):
    """The commonest walkable block round (x, y), in place of a hole."""
    counts = {}
    for dx, dy in DELTA:
        nx, ny = x + dx, y + dy
        if room.inside(nx, ny):
            b = room.at(nx, ny)
            if not b & 0x0C00 and room.mb[b & 0x3FF] not in WARP_MB | HOLE_MB:
                counts[b] = counts.get(b, 0) + 1
    if counts:
        room.blocks[y * room.w + x] = max(counts, key=counts.get)


def walk(room, starts, warps, blocked=frozenset()):
    """Cells the player can reach from starts, with the elevation rules of
    the game (a step onto a block of another height is refused unless either
    is 0 or 15), ledges one way, and warp pads carrying across. Warp tiles are
    reached but not walked through (a door is walked into though it is
    solid). A room with switches (New Mauville's) is walked in each state
    of its barriers, a switch stepped on changing the state."""
    pads = {(x, y): (warps[pair][0], warps[pair][1]) for x, y, role, pair in warps if role == 'PAD'}
    # A landing spot (LAND) is plain floor, not a warp.
    warp_cells = {(x, y) for x, y, role, _ in warps if role != 'LAND'}
    blocked = blocked | getattr(room, 'avoid', frozenset())
    by_state = getattr(room, 'states', None)
    triggers = getattr(room, 'triggers', {})

    def block(x, y, s):
        return by_state[s][y * room.w + x] if by_state else room.at(x, y)
    seen, states = set(), set()
    q = deque()
    for x, y in starts:
        e = room.at(x, y) >> 12
        q.append((x, y, e, True, 0))
        states.add((x, y, e, 1, 0))
        seen.add((x, y))
    while q:
        x, y, e, arrived, s = q.popleft()
        # Stepping onto a warp takes the player away; arriving on one (a pad's
        # far end, or the way in) leaves them free to step off.
        if (x, y) in warp_cells and not arrived:
            if (x, y) in pads:
                tx, ty = pads[(x, y)]
                te = block(tx, ty, s) >> 12
                if (tx, ty, te, 1, s) not in states:
                    states.add((tx, ty, te, 1, s))
                    seen.add((tx, ty))
                    q.append((tx, ty, te, True, s))
            continue
        for dx, dy in DELTA:
            nx, ny = x + dx, y + dy
            here = room.beh(x, y)
            if here in JUMP and JUMP[here] == (dx, dy):
                nx, ny = x + 2 * dx, y + 2 * dy
            if not room.inside(nx, ny) or (nx, ny) in blocked:
                continue
            b = block(nx, ny, s)
            nb = room.mb[b & 0x3FF]
            if nb in JUMP and JUMP[nb] != (dx, dy):
                continue
            if b & 0x0C00 and nb not in JUMP and (nx, ny) not in warp_cells:
                continue
            te = b >> 12
            if e not in (0, 15) and te not in (0, 15) and te != e:
                continue
            ne = e if te == 15 or (block(x, y, s) >> 12) == 15 else te
            ns = s
            t = triggers.get((nx, ny))
            if t and s in t[0]:
                ns = t[1]
            if (nx, ny, ne, 0, ns) in states:
                continue
            states.add((nx, ny, ne, 0, ns))
            seen.add((nx, ny))
            q.append((nx, ny, ne, False, ns))
    return seen


def touches(cell, seen):
    return cell in seen or any((cell[0] + dx, cell[1] + dy) in seen for dx, dy in DELTA)


def facing_of(o):
    mt = o['movement_type'][len('MOVEMENT_TYPE_'):]
    m = re.match(r'FACE_(UP|DOWN|LEFT|RIGHT)$', mt)
    if m:
        return DIRS[m[1]]
    m = re.match(r'FACE_(UP|DOWN|LEFT|RIGHT)_AND_(UP|DOWN|LEFT|RIGHT)$', mt)
    if m:
        return DIRS[m[1]] | TURNS | (DIRS[m[2]] << 2)
    if 'LOOK_AROUND' in mt:
        return DIRS['DOWN'] | LOOKS
    return DIRS['DOWN']


def spots_of(room, warps, entry):
    """Spots from the room's own objects, the boss's, then open places."""
    base = walk(room, [entry], warps)
    needed = [(x, y) for x, y, _, _ in warps]
    spots = []
    taken = set()
    near_warp = {(x + dx, y + dy) for x, y, _, _ in warps for dx, dy in DELTA + [(0, 0)]}

    # A ball on a switch (New Mauville's) is taken before the switch is
    # needed, so it closes no way; no one else stands on one.
    switches = set(getattr(room, 'triggers', {}))

    def ok(cell, extra=()):
        if cell in taken or cell in near_warp or not room.inside(*cell):
            return False
        b = room.at(*cell)
        if b & 0x0C00 or room.mb[b & 0x3FF] in WARP_MB or not touches(cell, base):
            return False
        blocked = (taken | {cell} | set(extra)) - switches
        seen = walk(room, [entry], warps, blocked)
        return all(touches(c, seen) for c in needed) and all(touches(s, seen) for s in taken)

    if room.kind == 'ruins' and room.use == 'last':
        # The riddle door's three tablets: the door and the wall either side,
        # as the Sealed Chamber's braille stands where its door opens (the
        # game shuts the door with Emerald's own sealed blocks).
        door = outer_door(warps)
        for dx in (-1, 0, 1):
            spots.append((door[0] + dx, door[1], 'TABLET', DIRS['UP'], 0))
    objs = sorted(room.map.get('object_events', []), key=lambda o: (
        0 if o.get('trainer_type') == 'TRAINER_TYPE_NORMAL' else 1 if 'ITEM_BALL' in o['graphics_id'] else 2))
    for o in objs:
        if NOT_SPOTS.search(o['graphics_id']):
            continue
        cell = (o['x'], o['y'])
        if not room.inside(*cell) or not ok(cell):
            continue
        kind = 'TRAINER' if o.get('trainer_type') == 'TRAINER_TYPE_NORMAL' else 'ITEM' if 'ITEM_BALL' in o['graphics_id'] else 'PERSON'
        if kind == 'ITEM' and DECOY_SCRIPT.search(o.get('script', '')):
            kind = 'DECOY'
        rng = int(o.get('trainer_sight_or_berry_tree_id', '0') or 0) if kind == 'TRAINER' else 0
        spots.append((cell[0], cell[1], kind, facing_of(o), max(rng, 3) if kind == 'TRAINER' else 0))
        taken.add(cell)
    if room.parts:
        cabin_spots(room, warps, entry, spots, taken, ok)
    if room.use == 'last' and not room.opts.get('holds'):
        dist = distances(room, entry, warps)
        boss = room.opts.get('boss')
        if room.opts.get('boss_near'):
            nx, ny = room.opts['boss_near']
            boss = min((c for c in base if ok(c) and open_round(room, c)),
                       key=lambda c: (abs(c[0] - nx) + abs(c[1] - ny), c[1], c[0]))
        if boss and ok(boss):
            spots.append((boss[0], boss[1], 'BOSS', DIRS['DOWN'], 4))
            taken.add(boss)
        else:
            # The farthest person or trainer from the way in turns boss.
            people = [s for s in spots if s[2] in ('TRAINER', 'PERSON')]
            if people:
                far = max(people, key=lambda s: dist.get((s[0], s[1]), 0))
                spots.remove(far)
                spots.append((far[0], far[1], 'BOSS', far[3] & 3, 4))
            else:
                cand = [c for c in sorted(base, key=lambda c: -dist.get(c, 0)) if ok(c)]
                if cand:
                    spots.append((cand[0][0], cand[0][1], 'BOSS', DIRS['DOWN'], 4))
                    taken.add(cand[0])
    if room.kind == 'lair':
        leader = leader_spot(room, warps, entry, next(s for s in spots if s[2] == 'BOSS'))
        spots.append(leader)
        taken.add(leader[:2])
        carving = carving_spot(room, warps, entry, taken)
        if carving:
            spots.append(carving)
        boss = next(s for s in spots if s[2] == 'BOSS')
        gate = gate_spot(room, warps, entry, boss)
        if gate:
            spots.append(gate)
        prize = lair_prize_spot(room, warps, entry, boss, taken, {gate[:2]} if gate else set())
        if prize:
            spots.append(prize)
            taken.add(prize[:2])
        return spots
    # Open places, spread out: each the farthest from those already taken.
    open_base = base
    if room.kind == 'ruins' and room.use == 'last':
        # A vault's own finds lie in its outer room; the inner one keeps
        # the guardian and the treasure.
        open_base = walk(room, [entry], warps, {outer_door(warps)[:2]})
    # (A ruins floor is a ring of corridors round the tower's hollow middle:
    # anywhere that closes no way will do, off the cracked floor.)
    cand = sorted(c for c in open_base if all(touches(n, base) for n in [c]) and c not in switches
                  and (open_round(room, c) if room.kind not in ('ruins', 'mirage') else c not in room.avoid
                       and not any((c[0] + dx, c[1] + dy) in room.avoid for dx, dy in DELTA)))
    while sum(1 for s in spots if s[2] not in CABIN_SPOTS) < MAX_SPOTS and cand:
        best, bestd = None, -1
        for c in cand:
            d = min([abs(c[0] - t[0]) + abs(c[1] - t[1]) for t in taken] + [abs(c[0] - entry[0]) + abs(c[1] - entry[1])])
            if d > bestd:
                best, bestd = c, d
        cand.remove(best)
        if bestd < 3:
            break
        if not ok(best):
            continue
        spots.append((best[0], best[1], 'OPEN', open_facing(room, best), 3))
        taken.add(best)
    # The extras last, so the rooms keep the spots they had.
    extra_spots(room, warps, entry, base, spots, taken, ok)
    assert len(spots) <= 32, room.name  # a u32 of spots taken (TakeRoomSpot)
    return spots


# Spots of a composed floor's cabins, beside the room's own.
CABIN_SPOTS = ('KEY', 'LOCK', 'HOLD', 'HOLD_PRIZE', 'SHUT')


def cabin_spots(room, warps, entry, spots, taken, ok):
    """A composed floor's cabins (COMPOSE 'cabins'): in each, a KEY spot for
    a key to another cabin, as far in as it goes; on the hidden deck also a
    HOLD where the boss waits if the cabin is the last one opened, and a
    HOLD_PRIZE beside it. A LOCK on each door that can be locked (its range
    the cabin, + 0x80 if always locked), and SHUT on each door that stays
    shut (signs, not places to stand). Cabins are numbered in the order
    COMPOSE lists them."""
    c = room.opts
    index = {(x, y): i for i, (x, y, _, _) in enumerate(warps)}
    plain = [w for w in warps if w[2] != 'PAD']
    for k, door in enumerate(c.get('cabins', [])):
        partner = warps[warps[index[door]][3]][:2]
        cells = walk(room, [partner], plain)
        dist = distances(room, partner, plain)
        near = {(x + dx, y + dy) for x, y, _, _ in warps for dx, dy in DELTA + [(0, 0)]}
        cand = sorted((p for p in cells if p in dist and p not in near and open_round(room, p)),
                      key=lambda p: (-dist[p], p))
        want = ['HOLD', 'KEY'] if c.get('holds') else ['KEY']
        placed = {}
        for kind in want:
            for p in cand:
                if any(abs(p[0] - q[0]) + abs(p[1] - q[1]) < 3 for q in placed.values()) or not ok(p):
                    continue
                placed[kind] = p
                spots.append((p[0], p[1], kind, DIRS['DOWN'], k))
                taken.add(p)
                break
            assert kind in placed, '%s: no %s in cabin %d' % (room.name, kind, k)
        if 'HOLD' in placed:
            hx, hy = placed['HOLD']
            # Beside the boss, not behind it: a Pokemon's overworld sprite is
            # two blocks tall, and a ball just behind it sits on its head
            # (seed 6047's Banette looked like something else).
            for dx, dy in ((1, 0), (-1, 0), (2, 0), (-2, 0), (1, -1), (-1, -1), (0, 1), (0, -1)):
                p = (hx + dx, hy + dy)
                if p in cells and p not in near and ok(p):
                    spots.append((p[0], p[1], 'HOLD_PRIZE', DIRS['DOWN'], k))
                    taken.add(p)
                    break
            else:
                assert False, '%s: no prize beside the hold of cabin %d' % (room.name, k)
    for x, y, always in c.get('locks', []):
        k = c['cabins'].index((x, y))
        spots.append((x, y, 'LOCK', DIRS['DOWN'], k | (0x80 if always else 0)))
    for x, y in c.get('shut', []):
        spots.append((x, y, 'SHUT', DIRS['DOWN'], 0))


def extra_spots(room, warps, entry, base, spots, taken, ok):
    """Places for the dungeon extras (RogueGen_DungeonExtras), taken like the
    others so none closes a way: the PRIZE beside the boss of a last floor
    (beside or two behind them, never in front); in a tower, the CHOICE row of
    two or three balls two steps apart, in a nook off the way (between the
    graves, as offerings), and on a middle floor the CAMP, where the old
    caretaker sits off the way."""
    boss = next((s for s in spots if s[2] == 'BOSS'), None)
    if boss:
        fx, fy = DELTA[boss[3] & 3]
        sx, sy = fy, fx  # across the way the boss faces
        # Beside first: a Pokemon boss's sprite is two blocks tall and would
        # stand over a ball just behind it.
        for dx, dy in ((sx, sy), (-sx, -sy), (2 * sx, 2 * sy), (-2 * sx, -2 * sy), (-2 * fx, -2 * fy),
                       (sx - fx, sy - fy), (-sx - fx, -sy - fy), (-fx, -fy)):
            cell = (boss[0] + dx, boss[1] + dy)
            if ok(cell):
                spots.append((cell[0], cell[1], 'PRIZE', DIRS['DOWN'], 0))
                taken.add(cell)
                break
    vault = room.kind == 'ruins' and room.use == 'last'
    if room.kind not in ('tower', 'mirage') and not vault:
        return
    # Off the walk up to the master too, on the top floor.
    way = way_cells(room, warps, entry, [boss[:2]] if boss else [])
    if vault:
        # A ruins vault's row is the treasure behind the riddle door: in the
        # inner room with the guardian, off the walk from the door to it.
        inner = inner_door(warps)
        base = walk(room, [inner[:2]], warps, {outer_door(warps)[:2]}) - {inner[:2]}
        way = way_cells(room, [inner], inner[:2], [boss[:2]] if boss else [])

    def walls(c):
        return sum(1 for dx, dy in DELTA if not room.inside(c[0] + dx, c[1] + dy)
                   or room.at(c[0] + dx, c[1] + dy) & 0x0C00)

    def apart(c):
        # From the other spots, and well clear of the stairs.
        return min([abs(c[0] - t[0]) + abs(c[1] - t[1]) for t in taken]
                   + [abs(c[0] - w[0]) + abs(c[1] - w[1]) - 1 for w in warps] + [99])

    best, best_score = None, None
    for c in sorted(base):
        for n in (3, 2):
            row = [(c[0] + 2 * k, c[1]) for k in range(n)]
            if any(r in way or apart(r) < 2 or not ok(r, row) for r in row):
                continue
            # Each ball picked up from beside, with the others standing.
            seen_row = walk(room, [entry], warps, taken | set(row))
            if not all(touches(r, seen_row) for r in row):
                continue
            score = (n, sum(walls(r) for r in row), min(abs(r[0] - entry[0]) + abs(r[1] - entry[1]) for r in row))
            if best_score is None or score > best_score:
                best, best_score = row, score
            break
    # (A room may say where its row stands: the Mirage Tower's fossils.)
    if room.opts.get('choice'):
        best = [c for c in room.opts['choice'] if ok(c, room.opts['choice'])]
        best = best if len(best) == len(room.opts['choice']) else None
        assert best, '%s: the choice row is shut in' % room.name
    row = best
    if best:
        for r in best:
            spots.append((r[0], r[1], 'CHOICE', DIRS['DOWN'], 0))
            taken.add(r)
    if room.use == 'middle' and room.kind == 'tower':
        best, best_score = None, None
        for c in sorted(base):
            if c in way or apart(c) < 2 or not ok(c):
                continue
            score = (walls(c) >= 2, min(abs(c[0] - w[0]) + abs(c[1] - w[1]) for w in way) if way else 0)
            if best_score is None or score > best_score:
                best, best_score = c, score
        if best:
            spots.append((best[0], best[1], 'CAMP', DIRS['DOWN'], 0))
            taken.add(best)
    if row:
        # Someone beside the row says only one may be taken, as Mt. Moon's
        # fossil maniac does: at an end of the row, else close by, never
        # between the balls.
        def gap(c):
            return c[1] == row[0][1] and row[0][0] < c[0] < row[-1][0]

        def dist(c):
            return min(abs(c[0] - r[0]) + abs(c[1] - r[1]) for r in row)

        ends = [(row[0][0] - 2, row[0][1]), (row[-1][0] + 2, row[-1][1])]
        near = sorted((c for c in base if c not in way and c not in taken and not gap(c) and dist(c) <= 3),
                      key=lambda c: (dist(c), c))
        for c in [e for e in ends if e in base and e not in way] + near:
            # Spoken to from beside, with the balls standing.
            if ok(c) and touches(c, walk(room, [entry], warps, taken | {c})):
                face = DIRS['LEFT'] if c[0] > row[-1][0] else DIRS['RIGHT'] if c[0] < row[0][0] else DIRS['DOWN']
                spots.append((c[0], c[1], 'GUIDE', face, 0))
                taken.add(c)
                break


def outer_door(warps):
    """A ruins vault's riddle door: of its two warp pads (the door and where
    it lets out inside), the one on the outer room's side, lower down."""
    return max((w for w in warps if w[2] == 'PAD'), key=lambda w: w[1])


def inner_door(warps):
    return min((w for w in warps if w[2] == 'PAD'), key=lambda w: w[1])


def ruins_setup(room):
    """A ruins room's cracked floor (cells no walk may count on: stepped on
    twice, it drops the player a floor), and where a fall from the floor
    above lands (LAND, a plain warp tile-less warp the game sends falls to):
    open floor a few steps from the stairs back up (in a vault, in its outer
    room), so a player who falls always sees the way back at once and is
    never left wondering how to get out."""
    # In Hoenn the Mirage Tower's cracked floor is crossed on the Mach Bike:
    # a walker falls at the first step onto it. The fewest cracks that keep
    # every warp in reach on foot become sand floor (the floor beside them),
    # and the rest stay as pits beside the way.
    fill_crack_path(room)
    room.avoid = frozenset((x, y) for y in range(room.h) for x in range(room.w)
                           if room.beh(x, y) == CRACK_MB)
    assert not (room.use == 'last' and room.avoid), '%s: cracked floor on a last floor' % room.name
    entry = room.warps[0][:2]
    # (v1.1 peaks) A tower is climbed: a fall from the floor above lands
    # near the ladder back up to it (ON), not near the way in.
    if room.kind == 'mirage':
        if room.use == 'last':
            return
        near_to = next(w for w in room.warps if w[2] == 'ON')[:2]
    else:
        near_to = entry
    pads = {w[:2] for w in room.warps if w[2] == 'PAD'}
    near = {(x + dx, y + dy) for x, y, _, _ in room.warps for dx, dy in DELTA + [(0, 0)]}
    outer = walk(room, [entry], room.warps, pads)
    cand = [c for c in outer if c not in near and c not in room.avoid and open_round(room, c)
            and not any((c[0] + dx, c[1] + dy) in room.avoid for dx, dy in DELTA)]
    assert cand, '%s: nowhere to land' % room.name
    # Steps from the stairs up, on the floor walked without the cracks.
    dist, q = {near_to: 0}, deque([near_to])
    while q:
        c = q.popleft()
        for dx, dy in DELTA:
            n = (c[0] + dx, c[1] + dy)
            if n in outer and n not in dist:
                dist[n] = dist[c] + 1
                q.append(n)
    land = min(cand, key=lambda c: (dist.get(c, 1 << 20) < 3, dist.get(c, 1 << 20), c))
    room.warps.append((land[0], land[1], 'LAND', 0))


SAND_FLOOR = 0x211  # the Mirage Tower's plain sand floor


def fill_crack_path(room):
    """Fills in the cracked floor on the cheapest walk (fewest cracks) from
    the way in to each other warp, so all of them are reached on foot."""
    import heapq
    entry = room.warps[0][:2]

    def cost(c):
        return 1 if room.beh(*c) == CRACK_MB else 0
    dist, prev = {entry: 0}, {entry: None}
    q = [(0, entry)]
    while q:
        d, c = heapq.heappop(q)
        if d > dist[c]:
            continue
        for dx, dy in DELTA:
            n = (c[0] + dx, c[1] + dy)
            if not room.inside(*n) or room.at(*n) & 0x0C00:
                continue
            nd = d + cost(n)
            if nd < dist.get(n, 1 << 30):
                dist[n], prev[n] = nd, c
                heapq.heappush(q, (nd, n))
    for x, y, role, _ in room.warps[1:]:
        c = (x, y)
        while c is not None and c in prev:
            if room.beh(*c) == CRACK_MB:
                room.blocks[c[1] * room.w + c[0]] = (room.at(*c) & 0xF000) | SAND_FLOOR
            c = prev[c]


def way_cells(room, warps, entry, goals=()):
    """The cells on a shortest walk from the way in to each other warp (and
    to each of goals, such as the boss of a last floor)."""
    def bfs(start):
        dist = {start: 0}
        q = deque([start])
        while q:
            c = q.popleft()
            for dx, dy in DELTA:
                n = (c[0] + dx, c[1] + dy)
                if n not in dist and room.inside(*n) and not room.at(*n) & 0x0C00:
                    dist[n] = dist[c] + 1
                    q.append(n)
        return dist
    a = bfs(entry)
    cells = set()
    for x, y in [w[:2] for w in warps[1:]] + list(goals):
        b = bfs((x, y))
        if (x, y) in a:
            cells |= {c for c in a if c in b and a[c] + b[c] == a[(x, y)]}
    return cells


def carving_spot(room, warps, entry, taken):
    """Old words carved in a wall a few steps in from the way in, as the
    Sealed Chamber's braille: a wall block beside open floor, read from that
    floor (best facing north, as signs are). Not a place anyone stands."""
    dist = distances(room, entry, warps)
    warp_cells = {(x, y) for x, y, _, _ in warps}
    near_warp = {(x + dx, y + dy) for x, y in warp_cells for dx, dy in DELTA + [(0, 0)]}
    best, best_score = None, None
    for c, d in dist.items():
        if c in near_warp or c in taken or room.at(*c) & 0x0C00:
            continue
        for k, (dx, dy) in enumerate(DELTA):
            w = (c[0] + dx, c[1] + dy)
            if not room.inside(*w) or not room.at(*w) & 0x0C00 or w in taken:
                continue
            score = (k != DIRS['UP'], abs(d - 4), c[1], c[0])
            if best_score is None or score < best_score:
                best, best_score = (w[0], w[1], 'CARVING', k, 0), score
    return best


def gate_spot(room, warps, entry, boss):
    """Where what keeps a lair shut stands (ROBJ_SEAL: a cracked rock, a
    boulder, a keeper): a block every walk from the way in to the legend
    crosses, nearest the legend, off the warps. None if the room is open
    all round (the seal then stands where the legend appears)."""
    bx, by = boss[0], boss[1]
    goal = {(bx + dx, by + dy) for dx, dy in DELTA}
    seen = walk(room, [entry], warps, {(bx, by)})
    warp_cells = {(x, y) for x, y, _, _ in warps}
    dist = distances(room, entry, warps)
    best = None
    for c in sorted(seen):
        if c in warp_cells or c in goal or room.at(*c) & 0x0C00 or c not in dist:
            continue
        if goal & walk(room, [entry], warps, {(bx, by), c}):
            continue
        if best is None or dist[c] > dist[best]:
            best = c
    if best is None:
        return None
    # It faces the way the player comes.
    k = min(range(4), key=lambda k: dist.get((best[0] + DELTA[k][0], best[1] + DELTA[k][1]), 999))
    return (best[0], best[1], 'GATE', k, 0)


def lair_prize_spot(room, warps, entry, boss, taken, gate):
    """The prize ball beside a lair's legend: two or three steps from it,
    behind it first, closing no way to it."""
    bx, by = boss[0], boss[1]
    goal = {(bx + dx, by + dy) for dx, dy in DELTA}
    seen = walk(room, [entry], warps, {(bx, by)})
    near_warp = {(x + dx, y + dy) for x, y, _, _ in warps for dx, dy in DELTA + [(0, 0)]}
    cand = [c for c in seen if 2 <= abs(c[0] - bx) + abs(c[1] - by) and c not in near_warp
            and c not in taken and c not in gate and not room.at(*c) & 0x0C00]
    cand.sort(key=lambda c: (abs(c[0] - bx) + abs(c[1] - by) > 3, c[1] >= by, abs(c[0] - bx) + abs(c[1] - by), c))
    for c in cand:
        after = walk(room, [entry], warps, {(bx, by), c})
        if goal & after and touches(c, after):
            return (c[0], c[1], 'PRIZE', DIRS['DOWN'], 0)
    return None


def leader_spot(room, warps, entry, boss):
    """Where the villain leader waits: on the walk from the way in to the
    legend, at a narrow place nearest the legend if there is one (a block
    every walk to it crosses), else two steps short of it. He faces the way
    the player comes."""
    bx, by = boss[0], boss[1]
    goal = {(bx + dx, by + dy) for dx, dy in DELTA}
    seen = walk(room, [entry], warps, {(bx, by)})
    goal &= seen
    # The walk back from the legend, by a search from its side.
    prev = {}
    q = deque(sorted(goal))
    for c in goal:
        prev[c] = None
    while q:
        c = q.popleft()
        for dx, dy in DELTA:
            n = (c[0] + dx, c[1] + dy)
            if n in prev or n not in seen or n == (bx, by):
                continue
            prev[n] = c
            q.append(n)
    # The path from the way in (or where the last warp pad lands) to the legend.
    starts = [entry] + [(x, y) for x, y, role, _ in warps if role == 'PAD']
    start = min((s for s in starts if s in prev), key=lambda s: len(chain(prev, s)))
    path = chain(prev, start)
    warp_cells = {(x, y) for x, y, _, _ in warps}
    near_warp = {(x + dx, y + dy) for x, y in warp_cells for dx, dy in DELTA + [(0, 0)]}
    cand = [c for c in path[:-1] if c not in near_warp and c not in goal and not room.at(*c) & 0x0C00]
    if not cand:
        # A short walk: anywhere on it but the warps.
        cand = [c for c in path[:-1] if c not in warp_cells and c not in goal and not room.at(*c) & 0x0C00]
    # Nearest the legend first.
    cand.reverse()
    for c in cand[:8]:
        cut = walk(room, [entry], warps, {(bx, by), c})
        if not (goal & cut):
            pick = c
            break
    else:
        pick = cand[1] if len(cand) > 1 else cand[0]
    i = path.index(pick)
    toward = path[i - 1] if i > 0 else start
    dx, dy = toward[0] - pick[0], toward[1] - pick[1]
    facing = DELTA.index((max(-1, min(1, dx)), max(-1, min(1, dy)))) if (dx, dy) != (0, 0) else DIRS['DOWN']
    return (pick[0], pick[1], 'LEADER', facing, 3)


def chain(prev, c):
    out = [c]
    while prev[out[-1]] is not None:
        out.append(prev[out[-1]])
    return out


def open_round(room, c):
    """Open floor all round: a place to stand in a room, not in a doorway."""
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            x, y = c[0] + dx, c[1] + dy
            if not room.inside(x, y) or room.at(x, y) & 0x0C00:
                return False
    return True


def spots_of_all(room, warps, entry):
    room.spots_all = spots_of(room, warps, entry)
    return room.spots_all


def safe_sight(room, warps, spot, spots):
    """The spot with its sight cut short where a trainer standing there,
    walking up to a player it saw, would stop in a gap that shuts the player
    off from the stairs: in Emerald a trainer stays where they stopped until
    the map is entered again. (Seed 29's ruins had one stop in the one-wide
    way down the side of a floor, leaving the player in a dead end.) Other
    spots block the sight line but not the walk: few hold anyone at once."""
    x, y, kind, facing, rng = spot
    if rng < 2:
        return spot
    if facing & LOOKS:
        dirs = [0, 1, 2, 3]
    elif facing & TURNS:
        dirs = [facing & 3, (facing >> 2) & 3]
    else:
        dirs = [facing & 3]
    others = {(s[0], s[1]) for s in spots if (s[0], s[1]) != (x, y)}
    warp_cells = {(wx, wy) for wx, wy, role, _ in warps if role != 'LAND'}
    needed = [(wx, wy) for wx, wy, _, _ in warps]
    best = rng
    for d in dirs:
        dx, dy = DELTA[d]
        for n in range(1, best + 1):
            cell = (x + dx * n, y + dy * n)
            if (not room.inside(*cell) or room.at(*cell) & 0x0C00 or cell in others or cell in warp_cells
                    or cell in getattr(room, 'avoid', ())):
                break
            if n == 1:
                continue
            stop = (x + dx * (n - 1), y + dy * (n - 1))
            seen = walk(room, [cell], warps, {stop})
            if not all(touches(c, seen) for c in needed):
                best = n - 1
                break
    return (x, y, kind, facing, best)


def open_facing(room, c):
    """Face the longest clear line."""
    best, bestn = DIRS['DOWN'], -1
    for d, (dx, dy) in enumerate(DELTA):
        n, x, y = 0, c[0] + dx, c[1] + dy
        while room.inside(x, y) and not room.at(x, y) & 0x0C00 and n < 6:
            n, x, y = n + 1, x + dx, y + dy
        if n > bestn:
            best, bestn = d, n
    return best


def distances(room, entry, warps):
    dist = {entry: 0}
    q = deque([entry])
    while q:
        c = q.popleft()
        for dx, dy in DELTA:
            n = (c[0] + dx, c[1] + dy)
            if n in dist or not room.inside(*n) or room.at(*n) & 0x0C00:
                continue
            dist[n] = dist[c] + 1
            q.append(n)
    return dist


def build():
    layouts = load_layouts()
    rooms = []
    for style, kind, order, uses in STYLES:
        for use in ('ground', 'middle', 'last'):
            for name in uses[use]:
                room = Room(style, use, name, layouts)
                room.kind = kind
                room.warps = classify_composed(room) if room.parts else classify(room, order)
                # Warps only reached by a fall in Hoenn are left out.
                seen = walk(room, [room.warps[0][:2]], room.warps)
                keep = [i for i, w in enumerate(room.warps) if touches(w[:2], seen)]
                remap = {i: k for k, i in enumerate(keep)}
                room.warps = [(x, y, role, remap.get(pair, 0) if role == 'PAD' else 0)
                              for x, y, role, pair in (room.warps[i] for i in keep)
                              if role != 'PAD' or pair in remap]
                stairs_down(room)
                if kind in ('ruins', 'mirage'):
                    ruins_setup(room)
                n_back = sum(1 for w in room.warps if w[2] == 'BACK')
                # One way back: its warps side by side, the way in among them.
                group, grew = {room.warps[0][:2]}, True
                while grew:
                    grew = False
                    for x, y, role, _ in room.warps:
                        if role == 'BACK' and (x, y) not in group and touches((x, y), group):
                            group.add((x, y))
                            grew = True
                assert len(group) == n_back, '%s: %d ways back' % (name, n_back - len(group) + 1)
                n_on = sum(1 for w in room.warps if w[2] == 'ON')
                assert n_back, name
                assert (n_on > 0) == (use != 'last'), (name, n_on)
                assert len(room.warps) <= MAX_WARPS, (name, len(room.warps))
                # (A big room is drawn straight from ROM: GenRoomFloor.)
                assert (room.w * room.h <= MAX_BLOCKS or room.opts.get('big')) \
                    and (room.w + 15) * (room.h + 14) <= MAX_DATA, name
                entry = room.warps[0][:2]
                seen = walk(room, [entry], room.warps)
                for x, y, role, _ in room.warps:
                    assert touches((x, y), seen), '%s: warp %s at %d,%d cannot be reached' % (name, role, x, y)
                room.spots = [safe_sight(room, room.warps, s, room.spots_all) if s[2] in ('TRAINER', 'OPEN', 'BOSS') else s
                              for s in spots_of_all(room, room.warps, entry)]
                # A hideout's boss can be walked up to with every other spot
                # taken (the prize beside it, trainers, items): nothing shuts
                # it in. (A vault's guardian waits behind its riddle door.)
                boss = next((s for s in room.spots if s[2] == 'BOSS'), None)
                if boss is not None and kind in ('hideout', 'tower', 'mirage'):
                    others = {(s[0], s[1]) for s in room.spots if s[2] not in ('BOSS', 'OPEN')}
                    seen = walk(room, [entry], room.warps, others)
                    assert touches(boss[:2], seen), '%s: the boss at %d,%d is shut in' % (name, boss[0], boss[1])
                rooms.append(room)
    return rooms


def write(rooms):
    out = ['// Generated by tools/roguemon/gen_rooms.py from Hoenn\'s Mt. Pyre, Aqua and Magma',
           '// Hideout and legends\' chamber maps. Do not edit.', '',
           '// The vanilla dungeon a room comes from.',
           'enum { %s, ROOM_STYLE_COUNT };' % ', '.join('ROOM_STYLE_%s' % s[0] for s in STYLES), '',
           '#ifndef ROGUE_ROOMS_LAYOUTS_ONLY', '']
    for k, r in enumerate(rooms):
        out.append('// %s (%s)' % (r.name, r.layout_name))
        out.append('static const u16 sRoomBlocks_%s[%d * %d] = {' % (r.cname, r.w, r.h))
        for y in range(r.h):
            out.append('    ' + ', '.join('0x%04X' % v for v in r.blocks[y * r.w:(y + 1) * r.w]) + ',')
        out.append('};')
        out.append('static const struct RogueRoomWarp sRoomWarps_%s[] = {' % r.cname)
        for x, y, role, pair in r.warps:
            out.append('    { %d, %d, ROOM_%s, %d },' % (x, y, role, pair))
        out.append('};')
        out.append('static const struct RogueRoomSpot sRoomSpots_%s[] = {' % r.cname)
        for x, y, kind, facing, rng in r.spots:
            out.append('    { %d, %d, ROOM_SPOT_%s, 0x%02X, %d },' % (x, y, kind, facing, rng))
        out.append('};')
        out.append('')
    out.append('static const struct RogueRoom sRogueRooms[] = {')
    for r in rooms:
        out.append('    { sRoomBlocks_%s, sRoomWarps_%s, sRoomSpots_%s, %d, %d, %d, %d, ROOM_STYLE_%s, ROOM_USE_%s },'
                   % (r.cname, r.cname, r.cname, r.w, r.h, len(r.warps), len(r.spots), r.style, r.use.upper()))
    out.append('};')
    out.append('')
    out.append('#else')
    out.append('')
    out.append('// The vanilla dungeon each room comes from.')
    out.append('static const u8 sRogueRoomStyles[] = {')
    for r in rooms:
        out.append('    ROOM_STYLE_%s,' % r.style)
    out.append('};')
    out.append('')
    out.append('#ifndef ROGUE_ROOMS_STYLES_ONLY')
    out.append('')
    out.append('// The vanilla layout of each room, for its tilesets and border, and its map type.')
    out.append('static const u16 sRogueRoomLayouts[] = {')
    for r in rooms:
        out.append('    %s,' % r.layout_id)
    out.append('};')
    out.append('static const u8 sRogueRoomMapTypes[] = {')
    for r in rooms:
        out.append('    %s,' % r.map_type)
    out.append('};')
    out.append('')
    out.append('#endif // ROGUE_ROOMS_STYLES_ONLY')
    out.append('')
    out.append('#endif')
    out.append('')
    open(OUT, 'w').write('\n'.join(out))


def render(rooms, d):
    import subprocess
    import sys
    from PIL import Image, ImageDraw
    os.makedirs(d, exist_ok=True)
    py = sys.executable
    colors = {'BACK': (0, 160, 255), 'ON': (255, 60, 60), 'PAD': (255, 200, 0), 'LAND': (255, 255, 255)}
    scolors = {'TRAINER': (255, 0, 255), 'ITEM': (0, 255, 0), 'PERSON': (0, 255, 255), 'BOSS': (255, 128, 0),
               'OPEN': (255, 255, 255), 'PRIZE': (255, 255, 0), 'CHOICE': (0, 0, 255), 'CAMP': (128, 64, 0), 'GUIDE': (0, 200, 200),
               'LEADER': (255, 0, 0), 'GATE': (160, 0, 255), 'CARVING': (128, 128, 128), 'TABLET': (90, 60, 30),
               'KEY': (255, 160, 0), 'LOCK': (255, 80, 160), 'DECOY': (255, 255, 128), 'HOLD': (160, 255, 0),
               'HOLD_PRIZE': (255, 255, 0), 'SHUT': (90, 90, 90)}
    for r in rooms:
        binp = os.path.join(d, r.cname + '.bin')
        open(binp, 'wb').write(struct.pack('<%dH' % len(r.blocks), *r.blocks))
        png = os.path.join(d, r.cname + '.png')
        subprocess.run([py, os.path.join(ROOT, 'tools', 'roguemon', 'render_map.py'), binp, str(r.w), png,
                        '--primary', 'general', '--secondary', r.tileset], check=True)
        im = Image.open(png).convert('RGB')
        dr = ImageDraw.Draw(im)
        for i, (x, y, role, pair) in enumerate(r.warps):
            dr.rectangle([x * 16, y * 16, x * 16 + 15, y * 16 + 15], outline=colors[role], width=2)
            dr.text((x * 16 + 2, y * 16 + 2), str(i), fill=colors[role])
        for x, y, kind, facing, rng in r.spots:
            dr.ellipse([x * 16 + 3, y * 16 + 3, x * 16 + 12, y * 16 + 12], outline=scolors[kind], width=2)
        im.save(png)
        os.remove(binp)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--png', help='render every room into this directory')
    a = ap.parse_args()
    rooms = build()
    write(rooms)
    for r in rooms:
        kinds = {}
        for s in r.spots:
            kinds[s[2]] = kinds.get(s[2], 0) + 1
        print('%-22s %-7s %-6s %2dx%-2d warps %2d (%s) spots %s' % (
            r.name, r.style, r.use, r.w, r.h, len(r.warps), ''.join(w[2][0] for w in r.warps),
            ' '.join('%s %d' % kv for kv in sorted(kinds.items()))))
    if a.png:
        render(rooms, a.png)


if __name__ == '__main__':
    main()
