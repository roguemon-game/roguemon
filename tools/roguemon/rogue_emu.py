#!/usr/bin/env python3
"""Shared core for every tool that plays Roguémon in the emulator.

Everything that talks to the headless mGBA harness or reads the game lives
here: the Game class (buttons, frames, screenshots, save states, memory),
game state helpers (map and position, vars and flags, party, battle menus),
and the generated region as the host generator sees it (load_region, Grid).

The emulator side needs the headless harness, which is not published; the
host side (load_region, Grid and the constants) is what check_seeds.py and
preview_region.py use.
"""
import json
import os
import re
import struct
import subprocess
import tempfile
import fcntl
import shutil
from collections import deque

TOOLS = os.environ.get('ROGUE_TOOLS', os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
# Headers and interior layouts come from here. Point ROGUE_SRC at a copy of a
# release's include/ and data/layouts to match a ROM snapshot in ROGUE_BUILD.
SRC = os.environ.get('ROGUE_SRC', ROOT)
NM = os.environ.get('ROGUE_NM') or shutil.which('arm-none-eabi-nm') or 'arm-none-eabi-nm'


def toolchain_cached(build, name, compute, pack=lambda v: v, unpack=lambda v: v):
    """A result that needs the ARM toolchain (the ELF's symbols, struct offsets
    and probes compiled against the headers), kept in BUILD/toolcache when
    BUILD is a snapshot (ROGUE_BUILD, not the repo), so a snapshot copied to a
    machine without the toolchain still plays. The first run on the snapshot
    where the toolchain is fills the cache."""
    snapshot = build and os.path.abspath(build) != ROOT
    path = os.path.join(build, 'toolcache', name + '.json') if snapshot else None
    if path and os.path.exists(path):
        with open(path) as f:
            return unpack(json.load(f))
    if not shutil.which(NM):
        raise RuntimeError('no ARM toolchain at %s and no %s in %s: run a tool on this snapshot where the toolchain is '
                           '(it fills toolcache), or set ROGUE_NM' % (NM, name, os.path.dirname(path or '?')))
    value = compute()
    if path:
        # Several lanes fill a fresh snapshot's cache at once: write a temp file
        # and rename it, so a reader never sees a half-written one.
        os.makedirs(os.path.dirname(path), exist_ok=True)
        fd, tmp = tempfile.mkstemp(dir=os.path.dirname(path), prefix='.' + name + '.', suffix='.tmp')
        try:
            with os.fdopen(fd, 'w') as f:
                json.dump(pack(value), f)
            os.replace(tmp, path)
        except BaseException:
            if os.path.exists(tmp):
                os.unlink(tmp)
            raise
    return value

LTO_SUFFIX = re.compile(r'\.lto_priv\.\d+$')


def symbol_name(name):
    """A symbol's name in the source. The release build links with LTO,
    which renames file-local symbols (sRegion.lto_priv.0)."""
    return LTO_SUFFIX.sub('', name)


DIRS = {0: ('UP', 0, -1), 1: ('RIGHT', 1, 0), 2: ('DOWN', 0, 1), 3: ('LEFT', -1, 0)}
KEY_DELTA = {'UP': (0, -1), 'RIGHT': (1, 0), 'DOWN': (0, 1), 'LEFT': (-1, 0)}
GROUP = 75
# Map numbers in the Rogue map group (data/maps/map_groups.json): one slot
# per place, then the shared interiors and the League.
MAX_PLACES = 42
PC, MART, HOUSE, GYM, LEAGUE = MAX_PLACES, MAX_PLACES + 1, MAX_PLACES + 2, MAX_PLACES + 3, MAX_PLACES + 4
E4_1, CHAMPION_ROOM, HALL_OF_FAME = MAX_PLACES + 5, MAX_PLACES + 9, MAX_PLACES + 10
NUM_GYMS = 8
WARP_PC, WARP_MART, WARP_HOUSE, WARP_GYM, WARP_CAVE = 0, 1, 2, 3, 4
CAVE_SOUTH, CAVE_NORTH, CAVE_DOWN, CAVE_DOWN_2 = 0, 1, 2, 3
WARP_DUNGEON, WARP_DUNGEON_2 = 10, 11
# On a route: the second block of a forest's way in, two blocks wide.
WARP_DUNGEON_WIDE, WARP_DUNGEON_2_WIDE = 8, 9
# Warps on a dungeon floor (enum RogueFloorWarp); FLOOR_WARPS + one of them
# is the second block of a way two blocks wide.
FLOOR_UP, FLOOR_DOWN, FLOOR_UP_2, FLOOR_DOWN_2, FLOOR_EXIT, FLOOR_WARPS = 0, 1, 2, 3, 4, 5
# enum RogueDungeonKind
DUN_CAVE, DUN_VICTORY, DUN_FOREST, DUN_TOWER, DUN_HIDEOUT, DUN_LAIR = 0, 1, 2, 3, 4, 5
DUN_RUINS = 6  # (src/rogue_gen_ruins.h)
DUN_SHIP, DUN_PLANT = 7, 8
DUN_ICE, DUN_VOLCANO = 9, 10
DUN_MIRAGE = 11  # (v1.1 peaks: the desert tower, src/rogue_gen_peaks.h)
# An ice cave floor's ice (Shoal Cave's, MB_ICE in gTileset_Cave): stepped
# onto, it slides the player on until something stops them.
ICE_METATILE = 0x38D


# The professor's lab (enum RogueInterior RINT_LAB), where a run starts, in
# front of him (WarpToStart in src/rogue.c), and its door inside.
INT_LAB = 1
LAB_START = (6, 5)
LAB_DOOR = (6, 12)


def lab_warp(town):
    """The warp kind of the first town's lab door (ROGUE_HOUSE_WARP), or None."""
    if INT_LAB not in town['houses']:
        return None
    k = town['houses'].index(INT_LAB)
    return WARP_HOUSE if k == 0 else WARP_CAVE + k


def floor_map(floor):
    """The map number of a dungeon's floor (1 based): RogueFloor1 and on."""
    import json
    groups = json.load(open(os.path.join(SRC, 'data', 'maps', 'map_groups.json')))
    return groups['gMapGroup_Rogue'].index('RogueFloor1') + floor - 1
OBJ_BOSS = 10
OBJ_PRIZE, OBJ_CHOICE, OBJ_CAMP = 11, 12, 13
# The villain team (src/rogue_gen_hideout.h): blockade grunts and the
# leader's spot, the hideout dungeon kind, its admin boss, the gate on the
# hideout town's way on, and the flag set once the admin is beaten.
OBJ_GRUNT, OBJ_VILLAIN_LEADER = 14, 15
OBJ_GUIDE = 16
# What keeps a legend's chamber shut (ROBJ_SEAL), and how it opens (enum RogueLegendGate).
OBJ_SEAL = 17
# Someone lost deep in a dungeon, led out for a reward (slot: the item flag).
OBJ_EXPLORER = 18
# A ruins vault's riddle tablets (enum RogueSignKind RSIGN_TABLET).
SIGN_TABLET = 22
# A shipwreck's cabin key ball and a power plant's Voltorb ball (ROBJ_KEY,
# ROBJ_DECOY); a locked cabin door is sign SIGN_LOCK + its key's number.
OBJ_KEY, OBJ_DECOY = 19, 20
SIGN_LOCK = 0xE0
OBJ_TUNNEL_GUARD = 21  # in a tunnel's narrow way until its key is held: slot 0 a Hiker, 1 a grunt
# Route features (src/rogue_gen_features.h): a feature's people (slot
# FEATROLE_*: 0 talk, 1 host, 2 the child by SNORLAX, 3 the gauntlet's prize),
# its Pokemon (the Day Care's in their yard, the old sailor's Wingull), the
# two of a double battle pair (slot the pair, + 4 the second) and SNORLAX
# asleep across a pocket's way in (gone once woken).
OBJ_FEATFOLK, OBJ_FEATMON, OBJ_PAIR, OBJ_SNORLAX = 24, 25, 26, 27
GATE_OPEN, GATE_FIELD, GATE_RITUAL, GATE_GIFT, GATE_FOUND, GATE_VILLAIN_LEADER, GATE_AFTER = range(7)
GATE_KIND_NAMES = ["open", "field", "ritual", "gift", "found", "villain", "after"]
DUNGEON_HIDEOUT, BOSS_ADMIN = 4, 3
GATE_LEAGUE, GATE_VILLAIN = 9, 10
FLAG_ROGUE_HIDEOUT_CLEARED = 0x4D
# The villain teams (0 Aqua, 1 Magma, 2 Rocket by identity): team t's admin
# and leader trainer slots (RogueGen_TeamAdminSlot, RogueGen_TeamLeaderSlot),
# whose defeat flags say its hideout is cleared and its leader beaten.
TEAM_NAMES = ['AQUA', 'MAGMA', 'ROCKET']
TEAM_ADMIN_SLOTS = (233, 240, 242)
TEAM_LEADER_SLOTS = (234, 241, 243)
TRAINER_FLAG_0 = 0x500 + 589  # TRAINER_FLAGS_START + ROGUE_TRAINER_FIRST


def team_cleared_flag(t):
    """The flag that says team t's hideout is cleared (its admin beaten)."""
    return FLAG_ROGUE_HIDEOUT_CLEARED if t == 0 else TRAINER_FLAG_0 + TEAM_ADMIN_SLOTS[t]


def team_beaten_flag(t):
    """The flag that says team t's leader is beaten."""
    return TRAINER_FLAG_0 + TEAM_LEADER_SLOTS[t]
SLOT_VILLAIN_ADMIN, SLOT_VILLAIN_LEADER = 233, 234
# From include/constants/vars.h, flags.h and rogue.h.
VAR_ROGUE_PROGRESS = 0x40F9
VAR_ROGUE_ENCOUNTERS = 0x40FA
VAR_ROGUE_ENCOUNTERS_2 = 0x404E
VAR_ROGUE_ENCOUNTERS_3 = 0x409B
VAR_ROGUE_TOWN = 0x4083
VAR_ROGUE_VISITED = 0x408B
VAR_ROGUE_FALLEN = 0x40FB
VAR_ROGUE_STARTER = 0x40FE
VAR_ROGUE_DEX_MIX = 0x40B8   # the run's POKeDEX mix (ROGUE_MIX_*), v0.14
VAR_ROGUE_RUN_STATE = 0x40FC
VAR_ROGUE_META_UNLOCKS = 0x40FD
VAR_ROGUE_META_RUNS = 0x40DB
VAR_ROGUE_META_WINS = 0x40DC
VAR_ROGUE_META_BEST = 0x40E5
VAR_0x8004 = 0x8004
RUN_ACTIVE, RUN_LOST, RUN_WON = 0, 1, 2
TALL_GRASS = 0x0D  # metatile id in the General tileset
ASH_GRASS = 0x20A  # ash-covered grass with encounters (Fallarbor)
LONG_GRASS = 0x15  # field routes; wild Pokémon live here too
# Secondary tileset of each town theme (enum RogueTheme in include/rogue_gen.h).
THEME_TILESETS = ['petalburg', 'mauville', 'slateport', 'mossdeep', 'rustboro', 'lilycove',
                  'dewford', 'fallarbor', 'lavaridge', 'fortree', 'pacifidlog', 'sootopolis',
                  'pacifidlog']  # RTHEME_CURRENTS: a sea route's currents (v1.1 sea)
# enum RoguePlaceKind, RogueTownRole, RogueRouteType and RogueObstacle.
TOWN_START, TOWN_VILLAGE, TOWN_GYM, TOWN_HAMLET, TOWN_SUMMIT = 0, 1, 2, 3, 4
ROUTE_MOUNTAIN = 2
OBST_NONE, OBST_CUT, OBST_ROCK_SMASH, OBST_STRENGTH, OBST_SURF = 0, 1, 2, 3, 4
def gen_host_sources(root=ROOT):
    """The files the host generator is built from: when any is newer than a
    gen_host, that gen_host draws another region than this tree's ROM (the
    v0.13 playtests' shared tools/bin one named the first town JUNIFORD and
    put the doors elsewhere)."""
    import glob
    files = [os.path.join(root, 'tools', 'roguemon', 'gen_host.c'), os.path.join(root, 'src', 'rogue_gen.c'),
             os.path.join(root, 'include', 'rogue_gen.h'), os.path.join(root, 'include', 'constants', 'rogue.h')]
    for pattern in ('src/rogue_gen_*.h', 'src/rogue_text_*.h', 'src/data/rogue_*.h'):
        files += glob.glob(os.path.join(root, pattern))
    return [f for f in files if os.path.exists(f)]


def gen_host_stale(path, root=ROOT):
    """The source files newer than the gen_host at path (all of them if it
    is missing)."""
    if not os.path.exists(path):
        return gen_host_sources(root)
    built = os.path.getmtime(path)
    return [f for f in gen_host_sources(root) if os.path.getmtime(f) > built]


_GEN_HOST_CHECKED = []


def gen_host_path():
    """The host generator to use: $GEN_HOST if set (a frozen release's,
    with ROGUE_BUILD and ROGUE_SRC: then it is only checked against this
    tree when ROGUE_SRC is unset, with a warning), else this tree's own,
    tools/bin/gen_host, built or rebuilt here when its sources changed."""
    import sys
    if _GEN_HOST_CHECKED:
        return _GEN_HOST_CHECKED[0]
    path = os.environ.get('GEN_HOST')
    _GEN_HOST_CHECKED.append(path or os.path.join(ROOT, 'tools', 'bin', 'gen_host'))
    if path:
        stale = [] if os.environ.get('ROGUE_SRC') else gen_host_stale(path)
        if stale:
            sys.stderr.write('warning: GEN_HOST %s is older than %d of this tree\'s sources (%s...): it may draw '
                             'another region than the ROM. Rebuild it, or unset GEN_HOST to use this tree\'s.\n'
                             % (path, len(stale), os.path.relpath(stale[0], ROOT)))
        return path
    path = os.path.join(ROOT, 'tools', 'bin', 'gen_host')
    stale = gen_host_stale(path)
    if stale:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        # Use a lock file to avoid race conditions when several playtest.py
        # processes start at once and all see that gen_host needs rebuilding.
        # One process builds it under the lock; the others wait and reuse it.
        lock_path = os.path.join(os.path.dirname(path), '.gen_host.lock')
        with open(lock_path, 'w') as lock_file:
            fcntl.flock(lock_file.fileno(), fcntl.LOCK_EX)
            try:
                # Re-check staleness after acquiring the lock: another process
                # may have built it while we waited for the lock.
                if not os.path.exists(path) or gen_host_stale(path):
                    sys.stderr.write('building this tree\'s gen_host (%d sources newer) into %s\n' % (len(stale), path))
                    subprocess.run(['clang', '-O1', '-g', '-I', os.path.join(ROOT, 'include'), '-o', path,
                                    os.path.join(ROOT, 'tools', 'roguemon', 'gen_host.c')], check=True,
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                else:
                    sys.stderr.write('gen_host already built by another process\n')
            finally:
                fcntl.flock(lock_file.fileno(), fcntl.LOCK_UN)
    return path


# Resolved lazily (gen_host() below): importing this module never builds or
# checks anything. live.py's commands import it in a fresh process without
# the session's GEN_HOST, and an eager check rebuilt the worktree's
# tools/bin/gen_host in the middle of a session (seed 909 playtest).
GEN_HOST = os.environ.get('GEN_HOST') or os.path.join(ROOT, 'tools', 'bin', 'gen_host')


def gen_host():
    """The gen_host to run: one a tool set (GEN_HOST here, or $GEN_HOST:
    used as it is, with a warning if older than the tree), else this tree's,
    built when stale (gen_host_path)."""
    if not os.environ.get('GEN_HOST') and GEN_HOST != os.path.join(ROOT, 'tools', 'bin', 'gen_host'):
        return GEN_HOST
    return gen_host_path()
FLAG_SYS_GAME_CLEAR = 0x864
FLAG_BADGE01_GET = 0x867
FLAG_SYS_USE_FLASH = 0x888
FLAG_SYS_USE_STRENGTH = 0x889


# ---------------------------------------------------------------------------
# Game data: moves, species, the type chart, decrypted party, bag and money.
# Merged from the retired smartbot.py balance bot.

# Constants from the project headers.
TYPE_NONE, TYPE_MYSTERY, NUM_TYPES = 0, 10, 21
CAT_STATUS = 3
ITEM_POKE_BALL, ITEM_GREAT_BALL, ITEM_ULTRA_BALL = 1, 2, 3
ITEM_POTION, ITEM_SUPER_POTION, ITEM_HYPER_POTION, ITEM_MAX_POTION, ITEM_FULL_RESTORE = 28, 29, 30, 31, 32
BALLS = (ITEM_ULTRA_BALL, ITEM_GREAT_BALL, ITEM_POKE_BALL)
HEALS = {ITEM_POTION: 20, ITEM_SUPER_POTION: 60, ITEM_HYPER_POTION: 120, ITEM_MAX_POTION: 999, ITEM_FULL_RESTORE: 999}
POCKET_ITEMS, POCKET_POKE_BALLS = 0, 1
B_OUTCOME_WON, B_OUTCOME_LOST, B_OUTCOME_CAUGHT = 1, 2, 7
EVOSTATE_REPLACE_MOVE = 22
MVSTATE_HANDLE_YES_NO, MVSTATE_INTRO_MSG_1 = 4, 0
TASK_SIZE, NUM_TASKS = 40, 16
MENU_ACTION_POKEMON, MENU_SWITCH = 1, 1

CHARMAP = {0x00: ' ', 0xAD: '.', 0xAE: '-', 0xB4: "'", 0xB5: 'm', 0xB6: 'f', 0x1B: 'e'}
# The battle text puts a non-breaking space (0x39) for the spaces in the names and
# trainer lines it fills in, and a zero-width space (0x3A) after a hyphen it breaks at.
CHARMAP.update({0x39: ' ', 0x3A: ''})
CHARMAP.update({0xA1 + i: str(i) for i in range(10)})
CHARMAP.update({0xBB + i: chr(ord('A') + i) for i in range(26)})
CHARMAP.update({0xD5 + i: chr(ord('a') + i) for i in range(26)})

SUBSTRUCT_ORDER = [  # sSubstructOffsets in src/pokemon.c
    [0, 0, 0, 0, 0, 0, 1, 1, 2, 3, 2, 3, 1, 1, 2, 3, 2, 3, 1, 1, 2, 3, 2, 3],
    [1, 1, 2, 3, 2, 3, 0, 0, 0, 0, 0, 0, 2, 3, 1, 1, 3, 2, 2, 3, 1, 1, 3, 2],
    [2, 3, 1, 1, 3, 2, 2, 3, 1, 1, 3, 2, 0, 0, 0, 0, 0, 0, 3, 2, 3, 2, 1, 1],
    [3, 2, 3, 2, 1, 1, 3, 2, 3, 2, 1, 1, 3, 2, 3, 2, 1, 1, 0, 0, 0, 0, 0, 0],
]




class GameData:
    """Mixin for Game: batched reads and views of game data in RAM and ROM."""

    def peeks(self, reqs):
        """Reads many (size, addr) values with one round trip."""
        for i, (size, addr) in enumerate(reqs):
            self.cmd('peek%d %X v%d' % (size, addr, i))
        vals = [None] * len(reqs)
        for line in self.sync():
            m = re.match(r'v(\d+) [0-9A-F]+ = (\d+)', line)
            if m:
                vals[int(m.group(1))] = int(m.group(2))
        return vals

    def read(self, addr, n):
        """Reads n bytes (n a multiple of 4) from memory."""
        words = self.peeks([(32, addr + i) for i in range(0, n, 4)])
        return struct.pack('<%dI' % len(words), *words)

    def rom_bytes(self, addr, n):
        self.load_tables()
        off = addr - 0x08000000
        return self.rom[off:off + n]

    def compile_probe(self, body):
        """Compiles body against the project headers and returns (symbols, rodata)."""
        import base64
        import hashlib
        key = 'probe-%s' % hashlib.sha1(body.encode()).hexdigest()[:12]
        cached = toolchain_cached(getattr(self, 'build', None), key, lambda: self._compile_probe(body),
                                  lambda v: {'syms': v[0], 'rodata': base64.b64encode(v[1]).decode()},
                                  lambda d: (d['syms'], base64.b64decode(d['rodata'])))
        return cached

    def _compile_probe(self, body):
        tmp = tempfile.mkdtemp()
        src = os.path.join(tmp, 'probe.c')
        with open(src, 'w') as f:
            f.write('#include "global.h"\n#include "pokemon.h"\n#include "battle.h"\n#include "move.h"\n'
                    '#include "item.h"\n#include "item_menu.h"\n#include "party_menu.h"\n'
                    '#include "data.h"\n#include "constants/trainers.h"\n' + body)
        obj = os.path.join(tmp, 'probe.o')
        gcc = NM.replace('-nm', '-gcc')
        subprocess.run([gcc, '-c', '-mthumb', '-mthumb-interwork', '-mabi=apcs-gnu', '-march=armv4t',
                        '-iquote', 'include', '-DMODERN=1', '-DEMERALD', '-DTESTING=0', '-std=gnu17', '-w',
                        '-o', obj, src], check=True, cwd=SRC)
        out = subprocess.run([NM, '-S', obj], capture_output=True, text=True).stdout
        syms = {}
        for line in out.splitlines():
            parts = line.split()
            if len(parts) == 4:
                syms[parts[3]] = (int(parts[0], 16), int(parts[1], 16), parts[2])
        binf = os.path.join(tmp, 'rodata.bin')
        subprocess.run([NM.replace('-nm', '-objcopy'), '-O', 'binary', '-j', '.rodata', obj, binf], check=True)
        return syms, open(binf, 'rb').read()

    def load_tables(self):
        """Struct offsets, move bit fields, the type chart and the battle script
        command table, read from the project headers and the ROM. Called on
        first use by the data views below."""
        if getattr(self, '_tables', False):
            return
        self._tables = True
        build = getattr(self, 'build', None) or os.environ.get('ROGUE_BUILD', ROOT)
        self.rom = open(os.path.join(build, 'pokeemerald.gba'), 'rb').read()
        self._moves = {}
        fields = {
            'bmSpecies': 'offsetof(struct BattlePokemon, species)',
            'bmMoves': 'offsetof(struct BattlePokemon, moves)',
            'bmTypes': 'offsetof(struct BattlePokemon, types)',
            'bmPp': 'offsetof(struct BattlePokemon, pp)',
            'bmHp': 'offsetof(struct BattlePokemon, hp)',
            'bmLevel': 'offsetof(struct BattlePokemon, level)',
            'bmMaxHp': 'offsetof(struct BattlePokemon, maxHP)',
            'bmSize': 'sizeof(struct BattlePokemon)',
            'bmAttack': 'offsetof(struct BattlePokemon, attack)',
            'bmDefense': 'offsetof(struct BattlePokemon, defense)',
            'bmSpAttack': 'offsetof(struct BattlePokemon, spAttack)',
            'bmSpDefense': 'offsetof(struct BattlePokemon, spDefense)',
            'bmSpeed': 'offsetof(struct BattlePokemon, speed)',
            'bmStatus1': 'offsetof(struct BattlePokemon, status1)',
            'bmStages': 'offsetof(struct BattlePokemon, statStages)',
            'bsExpGetter': 'offsetof(struct BattleStruct, expGetterMonId)',
            'sb1Bag': 'offsetof(struct SaveBlock1, bag)',
            'bagItems': 'offsetof(struct Bag, items)',
            'bagBalls': 'offsetof(struct Bag, pokeBalls)',
            'bagBallsEnd': 'offsetof(struct Bag, TMsHMs)',
            'bagKeyItems': 'offsetof(struct Bag, keyItems)',
            'bagBerries': 'offsetof(struct Bag, berries)',
            'bagSize': 'sizeof(struct Bag)',
            'speciesBaseHp': 'offsetof(struct SpeciesInfo, baseHP)',
            'sb2Key': 'offsetof(struct SaveBlock2, encryptionKey)',
            'sb1Money': 'offsetof(struct SaveBlock1, money)',
            'boxPersonality': 'offsetof(struct BoxPokemon, personality)',
            'boxOtId': 'offsetof(struct BoxPokemon, otId)',
            'boxNick': 'offsetof(struct BoxPokemon, nickname)',
            'boxSecure': 'offsetof(struct BoxPokemon, secure)',
            'monLevel': 'offsetof(struct Pokemon, level)',
            'monHp': 'offsetof(struct Pokemon, hp)',
            'monMaxHp': 'offsetof(struct Pokemon, maxHP)',
            'monStatus': 'offsetof(struct Pokemon, status)',
            'monSize': 'sizeof(struct Pokemon)',
            'bagPocket': 'offsetof(struct BagPosition, pocket)',
            'bagCursor': 'offsetof(struct BagPosition, cursorPosition)',
            'bagScroll': 'offsetof(struct BagPosition, scrollPosition)',
            'pmSlot': 'offsetof(struct PartyMenu, slotId)',
            'pmSlot2': 'offsetof(struct PartyMenu, slotId2)',
            'moveInfoSize': 'sizeof(struct MoveInfo)',
            'speciesName': 'offsetof(struct SpeciesInfo, speciesName)',
            'speciesSize': 'sizeof(struct SpeciesInfo)',
            'speciesTypes': 'offsetof(struct SpeciesInfo, types)',
            'speciesLearnset': 'offsetof(struct SpeciesInfo, levelUpLearnset)',
            'lumSize': 'sizeof(struct LevelUpMove)',
            'lumLevel': 'offsetof(struct LevelUpMove, level)',
            'itemPrice': 'offsetof(struct ItemInfo, price)',
            'itemSize': 'sizeof(struct ItemInfo)',
            'scrLearnState': 'offsetof(struct BattleScripting, learnMoveState)',
            'trName': 'offsetof(struct Trainer, trainerName)',
            'trClass': 'offsetof(struct Trainer, trainerClass)',
            'clsRival': 'TRAINER_CLASS_RIVAL',
            'clsLeader': 'TRAINER_CLASS_LEADER',
            'clsElite': 'TRAINER_CLASS_ELITE_FOUR',
            'clsChampion': 'TRAINER_CLASS_CHAMPION',
            'clsAquaAdmin': 'TRAINER_CLASS_AQUA_ADMIN',
            'clsMagmaAdmin': 'TRAINER_CLASS_MAGMA_ADMIN',
            'clsAquaLeader': 'TRAINER_CLASS_AQUA_LEADER',
            'clsMagmaLeader': 'TRAINER_CLASS_MAGMA_LEADER',
            'clsRocketAdmin': 'TRAINER_CLASS_ROCKET_ADMIN',
            'clsBoss': 'TRAINER_CLASS_BOSS_FRLG',
        }
        body = ''.join('char %s[%s + 1];\n' % (k, v) for k, v in fields.items())
        # Bit fields cannot go through offsetof, so a probe table sets one
        # field at a time to all ones and the bytes show where it lives.
        probes = ['type', 'category', 'power', 'accuracy', 'pp', 'explosion', 'strikeCount']
        body += 'const struct MoveInfo movesProbe[] = {%s};\n' % ', '.join(
            '{.%s = -1}' % p if p != 'type' and p != 'category' else '{.%s = %d}' % (p, 31 if p == 'type' else 3)
            for p in probes)
        # And some of a battler's volatile bit fields, set to all ones.
        volatiles = {'perishSong': 1, 'destinyBond': 3, 'wrapped': 1, 'escapePrevention': 1}
        for v, ones in volatiles.items():
            body += 'const struct BattlePokemon bmProbe_%s = {.volatiles = {.%s = %d}};\n' % (v, v, ones)
        syms, rodata = self.compile_probe(body)
        self.off = {k: syms[k][1] - 1 for k in fields}
        size = self.off['moveInfoSize']
        base = syms['movesProbe'][0]
        self.move_fields = {}
        for i, p in enumerate(probes):
            blob = rodata[base + i * size: base + (i + 1) * size]
            bits = int.from_bytes(blob, 'little')
            lo = (bits & -bits).bit_length() - 1
            width = bin(bits).count('1')
            self.move_fields[p] = (lo, width)
        # Each as (byte offset, mask) pairs within struct BattlePokemon.
        self.bm_volatile = {}
        for v in volatiles:
            base = syms['bmProbe_' + v][0]
            blob = rodata[base: base + self.off['bmSize']]
            self.bm_volatile[v] = [(i, b) for i, b in enumerate(blob) if b]
        # The type chart, u32 entries in 4.12 fixed point.
        addr = self.sym['gTypeEffectivenessTable']
        raw = self.rom_bytes(addr, NUM_TYPES * NUM_TYPES * 4)
        vals = struct.unpack('<%dI' % (NUM_TYPES * NUM_TYPES), raw)
        self.chart = [[vals[a * NUM_TYPES + d] / 4096.0 for d in range(NUM_TYPES)] for a in range(NUM_TYPES)]
        table = self.sym['gBattleScriptingCommandsTable']
        n = 0x3EC // 4
        self.cmd_table = struct.unpack('<%dI' % n, self.rom_bytes(table, n * 4))

    def move(self, move_id):
        self.load_tables()
        if move_id not in self._moves:
            size = self.off['moveInfoSize']
            blob = self.rom_bytes(self.sym['gMovesInfo'] + move_id * size, size)
            bits = int.from_bytes(blob, 'little')
            info = {k: (bits >> lo) & ((1 << w) - 1) for k, (lo, w) in self.move_fields.items()}
            name_ptr = struct.unpack('<I', blob[:4])[0]
            info['name'] = decode_text(self.rom_bytes(name_ptr, 20)) if 0x08000000 <= name_ptr < 0x0A000000 else '?'
            self._moves[move_id] = info
        return self._moves[move_id]

    def price(self, item):
        self.load_tables()
        addr = self.sym['gItemsInfo'] + item * self.off['itemSize'] + self.off['itemPrice']
        return struct.unpack('<I', self.rom_bytes(addr, 4))[0]

    def mart_items(self, tier):
        """What the Mart sells at a stock tier, in list order, read from the
        ROM (Rogue_MartN_Items in data/scripts/rogue.inc)."""
        self.load_tables()
        raw = self.rom_bytes(self.sym['Rogue_Mart%d_Items' % tier], 64)
        items = []
        for i in range(0, 64, 2):
            item = struct.unpack_from('<H', raw, i)[0]
            if item == 0:
                break
            items.append(item)
        return items

    def species_types(self, species):
        self.load_tables()
        addr = self.sym['gSpeciesInfo'] + species * self.off['speciesSize'] + self.off['speciesTypes']
        return list(self.rom_bytes(addr, 2))

    def species_bst(self, species):
        """Base stat total (HP, Attack, Defense, Speed, Sp. Atk, Sp. Def)."""
        self.load_tables()
        addr = self.sym['gSpeciesInfo'] + species * self.off['speciesSize'] + self.off['speciesBaseHp']
        return sum(self.rom_bytes(addr, 6))

    def initial_moves(self, species, level):
        """The moves a wild Pokemon of this species and level knows, as
        GiveBoxMonInitialMoveset gives them: the last four level-up moves up
        to its level, skipping evolution moves (level 0) and repeats."""
        self.load_tables()
        addr = self.sym['gSpeciesInfo'] + species * self.off['speciesSize'] + self.off['speciesLearnset']
        ptr = struct.unpack('<I', self.rom_bytes(addr, 4))[0]
        size, lvl_off = self.off['lumSize'], self.off['lumLevel']
        moves = []
        while True:
            blob = self.rom_bytes(ptr, size)
            move = int.from_bytes(blob[:lvl_off], 'little')
            lvl = struct.unpack_from('<H', blob, lvl_off)[0]
            ptr += size
            if move == 0xFFFF or lvl > level:
                break
            if lvl == 0 or move in moves:
                continue
            moves = (moves + [move])[-4:]
        return moves

    def species_name(self, species):
        self.load_tables()
        addr = self.sym['gSpeciesInfo'] + species * self.off['speciesSize'] + self.off['speciesName']
        return decode_text(self.rom_bytes(addr, 13))

    def effectiveness(self, move_type, def_types):
        self.load_tables()
        mult = 1.0
        seen = set()
        for t in def_types:
            if t in (TYPE_NONE, TYPE_MYSTERY) or t in seen or t >= NUM_TYPES:
                continue
            seen.add(t)
            mult *= self.chart[move_type][t]
        return mult

    # Memory views ---------------------------------------------------------
    def battle_mon(self, battler):
        self.load_tables()
        o = self.off
        base = self.sym['gBattleMons'] + battler * o['bmSize']
        raw = self.read(base, (o['bmSize'] + 3) & ~3)
        moves = list(struct.unpack_from('<4H', raw, o['bmMoves']))
        return {
            'species': struct.unpack_from('<H', raw, o['bmSpecies'])[0],
            'moves': moves,
            'pp': list(raw[o['bmPp']:o['bmPp'] + 4]),
            'types': list(raw[o['bmTypes']:o['bmTypes'] + 3]),
            'hp': struct.unpack_from('<H', raw, o['bmHp'])[0],
            'maxhp': struct.unpack_from('<H', raw, o['bmMaxHp'])[0],
            'level': raw[o['bmLevel']],
            'attack': struct.unpack_from('<H', raw, o['bmAttack'])[0],
            'defense': struct.unpack_from('<H', raw, o['bmDefense'])[0],
            'spattack': struct.unpack_from('<H', raw, o['bmSpAttack'])[0],
            'spdefense': struct.unpack_from('<H', raw, o['bmSpDefense'])[0],
            'speed': struct.unpack_from('<H', raw, o['bmSpeed'])[0],
            'status': struct.unpack_from('<I', raw, o['bmStatus1'])[0],
            'stages': [b - 256 if b >= 128 else b for b in raw[o['bmStages']:o['bmStages'] + 8]],
            # Bound (Wrap, Sand Tomb...) or held by Mean Look and the like:
            # it cannot run or (held) switch out.
            'trapped': any(raw[i] & m for v in ('wrapped', 'escapePrevention') for i, m in self.bm_volatile[v]),
        }

    def foe_label(self, battler):
        """How the game names a foe in battle: its nickname, which is what
        every battle line says, with the species after it when they differ
        (a RUN 0 DRAFT is "DRAFT 047 (CLOYSTER)", a kept Pokemon on a
        trainer "LINDA (ARCANINE)"). The win-terry playtest read a DRAFT's
        species from the state line and took the game's faint line for a
        bug."""
        self.load_tables()
        o = self.off
        species = self.battle_mon(battler)['species']
        slot = self.peek(16, self.sym['gBattlerPartyIndexes'] + 2 * battler)
        side = 6 if battler & 1 else 0
        raw = self.read(self.sym['gParties'] + (side + slot) * o['monSize'] + o['boxNick'], 12)
        nick = decode_text(raw[:10])
        name = self.species_name(species)
        return name if not nick or nick.upper() == name.upper() else '%s (%s)' % (nick, name)

    def enemy_species(self):
        """The species of the opponent's party (gParties[B_TRAINER_OPPONENT_A])."""
        self.load_tables()
        o = self.off
        out = []
        for slot in range(6):
            base = self.sym['gParties'] + (6 + slot) * o['monSize']
            raw = self.read(base, (o['monSize'] + 3) & ~3)
            pers, otid = struct.unpack_from('<II', raw, o['boxPersonality'])
            words = struct.unpack_from('<12I', raw, o['boxSecure'])
            plain = struct.pack('<12I', *[w ^ pers ^ otid for w in words])
            idx = SUBSTRUCT_ORDER[0][pers % 24]
            species = struct.unpack_from('<H', plain, idx * 12)[0] & 0x7FF
            if species:
                out.append(species)
        return out

    def party(self):
        """The player's party, decrypted."""
        self.load_tables()
        o = self.off
        count = self.party_count()
        mons = []
        for slot in range(count):
            base = self.sym['gParties'] + slot * o['monSize']
            raw = self.read(base, (o['monSize'] + 3) & ~3)
            pers, otid = struct.unpack_from('<II', raw, o['boxPersonality'])
            sec = o['boxSecure']
            words = struct.unpack_from('<12I', raw, sec)
            plain = struct.pack('<12I', *[w ^ pers ^ otid for w in words])

            def sub(kind):
                idx = SUBSTRUCT_ORDER[kind][pers % 24]
                return plain[idx * 12:(idx + 1) * 12]
            s0, s1 = sub(0), sub(1)
            w = struct.unpack('<6H', s1)
            mons.append({
                'slot': slot,
                'pid': pers,
                'species': struct.unpack_from('<H', s0, 0)[0] & 0x7FF,
                'item': struct.unpack_from('<H', s0, 2)[0] & 0x3FF,
                'moves': [w[0] & 0x7FF, w[1] & 0x7FF, w[2] & 0x7FF, w[3] & 0x7FF],
                'pp': list(s1[8:12]),
                'name': decode_text(raw[o['boxNick']:o['boxNick'] + 10]),
                'level': raw[o['monLevel']],
                'exp': struct.unpack_from('<I', s0, 4)[0] & 0x1FFFFF,
                'hp': struct.unpack_from('<H', raw, o['monHp'])[0],
                'maxhp': struct.unpack_from('<H', raw, o['monMaxHp'])[0],
            })
        return mons

    def bag_pocket(self, which):
        """[(item, quantity)] in a bag pocket ('items', 'balls' or 'all')."""
        self.load_tables()
        o = self.off
        sb1 = self.peek(32, self.sym['gSaveBlock1Ptr'])
        sb2 = self.peek(32, self.sym['gSaveBlock2Ptr'])
        key = self.peek(32, sb2 + o['sb2Key']) & 0xFFFF
        if which == 'balls':
            start, end = o['bagBalls'], o['bagBallsEnd']
        elif which == 'berries':
            start, end = o['bagBerries'], o['bagSize']
        elif which == 'all':
            # Every pocket: items, key items, balls, TMs and berries in a row.
            start, end = o['bagItems'], o['bagItems'] + 4 * (30 + 30 + 16 + 64 + 46)
        else:
            start, end = o['bagItems'], o['bagKeyItems']
        raw = self.read(sb1 + o['sb1Bag'] + start, end - start)
        out = []
        for i in range(0, end - start, 4):
            item, qty = struct.unpack_from('<HH', raw, i)
            if item:
                out.append((item, qty ^ key))
        return out

    def count_item(self, which, items):
        return sum(q for i, q in self.bag_pocket(which) if i in items)

    def money(self):
        self.load_tables()
        sb1 = self.peek(32, self.sym['gSaveBlock1Ptr'])
        sb2 = self.peek(32, self.sym['gSaveBlock2Ptr'])
        key = self.peek(32, sb2 + self.off['sb2Key'])
        return self.peek(32, sb1 + self.off['sb1Money']) ^ key

    def controller(self):
        return self.peek(32, self.sym['gBattlerControllerFuncs']) & ~1

    def is_func(self, value, name):
        return (value & ~1) in self.all_syms.get(name, ())

    def script_cmd(self):
        """Name of the battle script command that is running, if known."""
        self.load_tables()
        instr = self.peek(32, self.sym['gBattlescriptCurrInstr'])
        if not 0x08000000 <= instr < 0x0A000000:
            return None
        op = self.rom[instr - 0x08000000]
        if op >= len(self.cmd_table):
            return None
        func = self.cmd_table[op] & ~1
        for name in ('Cmd_yesnobox', 'Cmd_yesnoboxlearnmove', 'Cmd_yesnoboxstoplearningmove'):
            if func in self.all_syms.get(name, ()):
                return name
        return None

    def find_task(self, name):
        funcs = self.all_syms.get(name, ())
        base = self.sym['gTasks']
        vals = self.peeks([(32, base + i * TASK_SIZE) for i in range(NUM_TASKS)] +
                          [(8, base + i * TASK_SIZE + 4) for i in range(NUM_TASKS)])
        for i in range(NUM_TASKS):
            if vals[NUM_TASKS + i] and (vals[i] & ~1) in funcs:
                return base + i * TASK_SIZE
        return None

    def task_data(self, task, i):
        v = self.peek(16, task + 8 + 2 * i)
        return v - 0x10000 if v >= 0x8000 else v


class Stuck(Exception):
    pass


class Game(GameData):
    def __init__(self, shots=None, build=None, save_file=None):
        self.shots = shots
        self.nshots = 0
        self.sym = {}
        self.all_syms = {}
        # ROGUE_BUILD points at a copy of the ROM and ELF, so a test can keep
        # running on a snapshot while the ROM in the repo is rebuilt.
        build = build or os.environ.get('ROGUE_BUILD', ROOT)
        self.build = build
        out = toolchain_cached(build, 'symbols', lambda: subprocess.run(
            [NM, os.path.join(build, 'pokeemerald.elf')], capture_output=True, text=True).stdout)
        for line in out.splitlines():
            parts = line.split()
            if len(parts) == 3:
                name = symbol_name(parts[2])
                self.sym[name] = int(parts[0], 16)
                # Static functions can share a name across files.
                self.all_syms.setdefault(name, set()).add(int(parts[0], 16) & ~1)
        # ROGUE_SAVE names a battery save file to play on (the harness reads
        # and writes it), for tests that carry a save from one build to another.
        args = [os.environ.get('HARNESS', os.path.join(TOOLS, 'bin', 'harness')), os.path.join(build, 'pokeemerald.gba'), '-']
        if save_file or os.environ.get('ROGUE_SAVE'):
            args.append(save_file or os.environ['ROGUE_SAVE'])
        self.p = subprocess.Popen(args,
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
        self.frames = 0

    def cmd(self, line):
        self.p.stdin.write(line + '\n')

    def sync(self):
        self.cmd('sync')
        self.p.stdin.flush()
        lines = []
        while True:
            line = self.p.stdout.readline()
            if not line:
                raise RuntimeError('harness exited')
            line = line.strip()
            if line == 'ok':
                return lines
            lines.append(line)

    def peek(self, size, addr):
        self.cmd('peek%d %X v' % (size, addr))
        for line in self.sync():
            m = re.match(r'v [0-9A-F]+ = (\d+)', line)
            if m:
                return int(m.group(1))
        raise RuntimeError('peek failed')

    def poke(self, size, addr, value):
        self.cmd('poke%d %X %X' % (size, addr, value))

    def wait(self, n):
        self.cmd('wait %d' % n)
        self.frames += n

    def hold(self, keys, n):
        self.cmd('hold %s %d' % (keys, n))
        self.frames += n

    def press(self, keys, n=1):
        self.cmd('press %s %d' % (keys, n))
        self.frames += 16 * n

    def shot(self, name):
        if self.shots:
            self.nshots += 1
            self.cmd('shot %s/%03d_%s.png' % (self.shots, self.nshots, name))

    # Game state ----------------------------------------------------------
    def state(self):
        sb1 = self.peek(32, self.sym['gSaveBlock1Ptr'])
        x = self.peek(16, sb1)
        y = self.peek(16, sb1 + 2)
        group = self.peek(8, sb1 + 4)
        num = self.peek(8, sb1 + 5)
        return group, num, x, y

    def seed(self):
        return self.peek(32, self.sym['sRegion'])

    def catch_used(self, area):
        var = VAR_ROGUE_ENCOUNTERS if area < 16 else VAR_ROGUE_ENCOUNTERS_2 if area < 32 else VAR_ROGUE_ENCOUNTERS_3
        return (self.var(var) >> (area & 15)) & 1

    def callback2(self):
        return self.peek(32, self.sym['gMain'] + 4) & ~1

    def in_callback(self, name):
        return self.callback2() == (self.sym[name] & ~1)

    def party_count(self):
        return self.peek(8, self.sym['gPartiesCount'])

    def in_battle(self):
        return (self.peek(8, self.sym['gMain'] + 0x439) & 2) != 0

    def offsets(self):
        """Struct offsets and sizes, taken from the project headers."""
        if hasattr(self, '_offsets'):
            return self._offsets
        self._offsets = toolchain_cached(self.build, 'offsets', self._offsets_from_headers)
        return self._offsets

    def _offsets_from_headers(self):
        tmp = tempfile.mkdtemp()
        src = os.path.join(tmp, 'off.c')
        with open(src, 'w') as f:
            f.write('#include "global.h"\n#include "pokemon.h"\n'
                    'char vars[offsetof(struct SaveBlock1, vars)];\n'
                    'char flags[offsetof(struct SaveBlock1, flags)];\n'
                    'char registeredItem[offsetof(struct SaveBlock1, registeredItem)];\n'
                    'char monSize[sizeof(struct Pokemon)];\n'
                    'char monHp[offsetof(struct Pokemon, hp)];\n'
                    'char monAttack[offsetof(struct Pokemon, attack)];\n'
                    'char monSpeed[offsetof(struct Pokemon, speed)];\n'
                    'char monSpAttack[offsetof(struct Pokemon, spAttack)];\n'
                    'char battleMonSize[sizeof(struct BattlePokemon)];\n'
                    'char battleMonHp[offsetof(struct BattlePokemon, hp)];\n'
                    'char battleMonMaxHp[offsetof(struct BattlePokemon, maxHP)];\n'
                    'char battleMonAttack[offsetof(struct BattlePokemon, attack)];\n'
                    'char battleMonSpeed[offsetof(struct BattlePokemon, speed)];\n'
                    'char battleMonSpAttack[offsetof(struct BattlePokemon, spAttack)];\n'
                    'char battleMonStatus1[offsetof(struct BattlePokemon, status1)];\n'
                    'char objSize[sizeof(struct ObjectEvent)];\n'
                    'char objLocalId[offsetof(struct ObjectEvent, localId)];\n'
                    'char objCoords[offsetof(struct ObjectEvent, currentCoords)];\n')
        obj = os.path.join(tmp, 'off.o')
        gcc = NM.replace('-nm', '-gcc')
        subprocess.run([gcc, '-c', '-mthumb', '-mthumb-interwork', '-mabi=apcs-gnu', '-march=armv4t',
                        '-iquote', 'include', '-DMODERN=1', '-DEMERALD', '-DTESTING=0', '-std=gnu17', '-w',
                        '-o', obj, src], check=True, cwd=SRC)
        out = subprocess.run([NM, '-S', obj], capture_output=True, text=True).stdout
        return {line.split()[3]: int(line.split()[1], 16) for line in out.splitlines() if len(line.split()) == 4}

    def objects(self):
        """Active object events other than the player: (localId, x, y)."""
        o = self.offsets()
        base = self.sym['gObjectEvents']
        out = []
        for i in range(16):
            a = base + i * o['objSize']
            if not (self.peek(32, a) & 1):
                continue
            local = self.peek(8, a + o['objLocalId'])
            if local == 0xFF:  # the player
                continue
            x = self.peek(16, a + o['objCoords']) - 7
            y = self.peek(16, a + o['objCoords'] + 2) - 7
            out.append((local, x, y))
        return out

    def templates(self):
        """Object templates of the loaded map, spawned or not: (localId, x, y)."""
        header = self.sym['gMapHeader']
        events = self.peek(32, header + 4)
        count = self.peek(8, events)
        ptr = self.peek(32, events + 4)
        out = []
        for i in range(count):
            a = ptr + i * 24
            out.append((self.peek(8, a), self.peek(16, a + 4), self.peek(16, a + 6)))
        return out

    def live_grid(self):
        """Walkable grid of the loaded map, read from RAM, with objects blocked."""
        base = self.sym['gBackupMapLayout']
        w = self.peek(32, base)
        h = self.peek(32, base + 4)
        ptr = self.peek(32, base + 8)
        raw = self.read(ptr, (w * h * 2 + 3) // 4 * 4)[:w * h * 2]
        blocks = list(struct.unpack('<%dH' % (w * h), raw))
        # The backup layout has a border of 7 blocks (15 across, 14 down).
        inner = [blocks[(y + 7) * w + x + 7] for y in range(h - 14) for x in range(w - 15)]
        grid = Grid(inner, w - 15)
        spawned = self.objects()
        ids = {local for local, _, _ in spawned}
        grid.blocked.update((x, y) for _, x, y in spawned)
        # Objects not spawned yet (off screen) stand where the map puts them.
        grid.blocked.update((x, y) for local, x, y in self.templates() if local not in ids)
        return grid

    def save_offsets(self):
        o = self.offsets()
        return o['vars'], o['flags']

    def party_mon(self, slot):
        return self.sym['gParties'] + slot * self.offsets()['monSize']

    def var(self, var_id):
        if var_id >= 0x8000:
            return self.peek(16, self.sym['gSpecialVar_0x%04X' % var_id])
        sb1 = self.peek(32, self.sym['gSaveBlock1Ptr'])
        return self.peek(16, sb1 + self.save_offsets()[0] + 2 * (var_id - 0x4000))

    def flag(self, flag_id):
        sb1 = self.peek(32, self.sym['gSaveBlock1Ptr'])
        return (self.peek(8, sb1 + self.save_offsets()[1] + flag_id // 8) >> (flag_id % 8)) & 1

    def set_flag(self, flag_id, on=True):
        sb1 = self.peek(32, self.sym['gSaveBlock1Ptr'])
        addr = sb1 + self.save_offsets()[1] + flag_id // 8
        v = self.peek(8, addr)
        v = (v | (1 << (flag_id % 8))) if on else (v & ~(1 << (flag_id % 8)))
        self.poke(8, addr, v)

    def teleport(self, map_num, x, y, town=None):
        """Warps straight to a spot on a map of the region (test hook
        gRogueTestWarp), as if entered from the outdoor place town (for
        dungeon floors). Returns once the player stands there."""
        base = self.sym['gRogueTestWarp']
        # Right after crossing a map edge the first warp asked for can be
        # taken and come to nothing: ask again.
        for attempt in range(3):
            self.poke(8, base + 1, x)
            self.poke(8, base + 2, y)
            self.poke(8, base + 3, 0 if town is None else town + 1)
            self.poke(8, base, map_num + 1)
            before = self.state()
            # (A map whose neighbour is slow to make, made on the spot as
            # it loads, can take a long while: seed 41's lake route 23,
            # about 2000 frames, for town 24.)
            for n in range(300):
                self.wait(10)
                if self.state()[1:] == (map_num, x, y) and self.in_callback('CB2_Overworld'):
                    self.wait(30)
                    return
                if n >= 60 and self.state()[1:] != (map_num, x, y):
                    break
                if n >= 12 and self.peek(8, base) == 0 and self.state() == before:
                    break
        raise Stuck('teleport to %d %d,%d failed (at %s)' % (map_num, x, y, self.state()))

    def set_var(self, var_id, value):
        sb1 = self.peek(32, self.sym['gSaveBlock1Ptr'])
        self.poke(16, sb1 + self.save_offsets()[0] + 2 * (var_id - 0x4000), value)


class Grid:
    def __init__(self, blocks, w, ground_only=False):
        self.w = w
        self.h = len(blocks) // w
        # Generated maps put all walkable ground at elevation 3 (doors are 0).
        # Water has no collision bit but sits at elevation 1. A treehouse
        # platform is at 4, reached up a ladder at 0; a log bridge's land
        # ends are at 4 and its planks at 15, stepped up to from grass at 0.
        if ground_only:
            self.walk = [(b & 0x0C00) == 0 and (b >> 12) in (0, 3, 4, 15) for b in blocks]
        else:
            self.walk = [(b & 0x0C00) == 0 for b in blocks]
        # The game's elevation rule, as it matters on generated maps: no step
        # between ground (3) and a raised block (4). They join only through
        # a block at 0, such as a log bridge's step tile or a ladder.
        self.elev = [b >> 12 for b in blocks]
        self.blocked = set()
        # Ice (an ice cave floor's): a step onto it slides on (path plans it).
        self.ice = set()
        # Currents (a sea maze's, v1.1): a step onto one carries the player
        # on its way until something stops them or the current ends.
        self.current = {}

    def ok(self, x, y):
        return 0 <= x < self.w and 0 <= y < self.h and self.walk[y * self.w + x] and (x, y) not in self.blocked

    def step(self, a, b):
        """Whether the elevations let the player step from a to b (both on
        the map)."""
        return {self.elev[a[1] * self.w + a[0]], self.elev[b[1] * self.w + b[0]]} != {3, 4}

    def slide(self, cell, dx, dy):
        """Where a step from cell ends: on ice the player slides on until
        the next block stops them or they leave the ice."""
        while cell in self.ice and self.ok(cell[0] + dx, cell[1] + dy):
            cell = (cell[0] + dx, cell[1] + dy)
        return cell

    def carry(self, cell):
        """Where a current carries the player who steps onto cell: on in its
        way, block by block, until the next block stops them or the current
        ends (the game's forced movement)."""
        for _ in range(64):
            d = self.current.get(cell)
            if d is None or not self.ok(cell[0] + d[0], cell[1] + d[1]):
                break
            cell = (cell[0] + d[0], cell[1] + d[1])
        return cell

    def path(self, start, goal):
        if start == goal:
            return []
        prev = {start: None}
        q = deque([start])
        while q:
            cur = q.popleft()
            for key, (dx, dy) in KEY_DELTA.items():
                nxt = (cur[0] + dx, cur[1] + dy)
                if 0 <= nxt[0] < self.w and 0 <= nxt[1] < self.h and not self.step(cur, nxt):
                    continue
                if nxt != goal and self.ok(*nxt) and nxt in self.ice:
                    nxt = self.slide(nxt, dx, dy)
                if nxt != goal and self.ok(*nxt) and nxt in self.current:
                    nxt = self.carry(nxt)
                if nxt in prev:
                    continue
                if nxt != goal and not self.ok(*nxt):
                    continue
                if nxt == goal and not (0 <= nxt[0] < self.w and 0 <= nxt[1] < self.h and self.walk[nxt[1] * self.w + nxt[0]]):
                    continue
                prev[nxt] = (cur, key)
                if nxt == goal:
                    keys = []
                    while prev[nxt]:
                        nxt, k = prev[nxt]
                        keys.append(k)
                    return keys[::-1]
                q.append(nxt)
        return None


def load_bin(path):
    data = open(path, 'rb').read()
    return list(struct.unpack('<%dH' % (len(data) // 2), data))


def region_name(seed):
    """The region's name, which is also its seed code."""
    with tempfile.TemporaryDirectory(prefix='rogue-region-') as tmp:
        subprocess.run([gen_host(), str(seed), tmp], check=True)
        for line in open(os.path.join(tmp, 'region.txt')):
            m = re.match(r'region name=(\w+)', line)
            if m:
                return m[1]
    raise ValueError('no region name for seed %d' % seed)


_rooms = None


def room_library():
    """The vanilla rooms of towers and hideouts, in the order of
    src/data/rogue_rooms.h (gen_rooms.py)."""
    global _rooms
    if _rooms is None:
        import gen_rooms
        _rooms = gen_rooms.build()
    return _rooms



def room_is(f):
    """A dungeon floor built from a vanilla room (a tower's or hideout's)."""
    return f.get('room', 255) != 255


def room_way(f, to_floor):
    """A room floor's first warp to another floor (0: out), and the warp it
    arrives by there."""
    return next((i, w) for i, (t, w) in sorted(f['dests'].items()) if t == to_floor)


class RoomGrid:
    """Paths on a floor built from a vanilla room: the game's rules for
    heights (gen_rooms.walk), warp tiles only as the goal, and warp pads
    carrying the player across (walk_to plans again after each pad)."""
    def __init__(self, f):
        self.f = f
        self.room = room_library()[f['room']]
        self.w, self.h = f['w'], f['h']
        self.blocks = f['blocks']
        import gen_rooms
        self.mb = gen_rooms.behaviours(f['tileset'])
        self.warps = {(x, y): i for i, (x, y, _, _) in enumerate(self.room.warps)}
        self.pads = {(x, y): self.room.warps[pair][:2] for x, y, role, pair in self.room.warps if role == 'PAD'}
        # Trainers, items, the boss, the extras (prize, choice balls, camp)
        # and the villain leader (until beaten).
        # A shipwreck's key balls and a power plant's Voltorb balls too.
        self.blocked = {(o['x'], o['y']) for o in f['objs'] if o['kind'] in (1, 2, 10, 11, 12, 13, OBJ_VILLAIN_LEADER, OBJ_SEAL,
                                                                             OBJ_EXPLORER, OBJ_KEY, OBJ_DECOY)}
        # A ruins floor's sand pits (cracked floor) drop the player a floor.
        self.pits = {(x, y) for y in range(self.h) for x in range(self.w)
                     if self.mb[self.at(x, y) & 0x3FF] == gen_rooms.CRACK_MB}
        self.blocked |= self.pits
        # A ruins floor's landing spot is plain floor, not a warp.
        self.warps = {xy: i for xy, i in self.warps.items() if self.room.warps[i][2] != 'LAND'}

    def at(self, x, y):
        return self.blocks[y * self.w + x]

    def inside(self, x, y):
        return 0 <= x < self.w and 0 <= y < self.h

    def ok(self, x, y):
        return (self.inside(x, y) and not self.at(x, y) & 0x0C00 and (x, y) not in self.warps
                and (x, y) not in self.blocked)

    def path(self, start, goal):
        """Keys from start to goal, the first leg if pads are on the way."""
        e0 = self.at(*start) >> 12
        prev = {(start, e0): None}
        q = deque([(start, e0)])
        while q:
            cell, e = q.popleft()
            for key, (dx, dy) in KEY_DELTA.items():
                n = (cell[0] + dx, cell[1] + dy)
                if not self.inside(*n) or (n in self.blocked and n != goal):
                    continue
                b = self.at(*n)
                # (A door is walked into though it is solid: the ship's.)
                if b & 0x0C00 and n not in self.warps:
                    continue
                te = b >> 12
                if e not in (0, 15) and te not in (0, 15) and te != e:
                    continue
                ne = e if te == 15 or (self.at(*cell) >> 12) == 15 else te
                if (n, ne) in prev:
                    continue
                prev[(n, ne)] = ((cell, e), key)
                if n == goal or n in self.pads:
                    keys, s = [], (n, ne)
                    while prev[s]:
                        s, k = prev[s]
                        keys.append(k)
                    if n == goal:
                        return keys[::-1]
                    # Across a pad: remember the way, go on from its partner.
                    t = self.pads[n]
                    te2 = self.at(*t) >> 12
                    if (t, te2) not in prev:
                        prev[(t, te2)] = prev[(n, ne)]
                        q.append((t, te2))
                    continue
                if n in self.warps:
                    continue
                q.append((n, ne))
        return None


# Dungeons: a cave's own map (level 0) and its floors (1, 2...), joined by
# ladders. On the cave map CAVE_DOWN and CAVE_DOWN_2 lead to the first
# floor's FLOOR_UP and FLOOR_UP_2; each floor's FLOOR_DOWN and FLOOR_DOWN_2
# to the next floor's FLOOR_UP and FLOOR_UP_2.
OBSTACLE_OBJS = (7, 8, 9)


def dungeon_of(maps, place):
    """The dungeon whose host is place, or None."""
    return next((d for d in maps[0].get('dungeons', []) if d['host'] == place), None)


def dungeon_levels(maps, d):
    """{0: the host's map, 1: floor 1, ...} of dungeon d."""
    out = {0: maps[d['host']]}
    for f in maps[0].get('floors', []):
        if f['dungeon'] == d['i']:
            out[f['floor']] = f
    return out


def ladder_partner(level, kind):
    """Where the warp kind on level leads: (level, kind) or None."""
    if level == 0:
        return {CAVE_DOWN: (1, FLOOR_UP), CAVE_DOWN_2: (1, FLOOR_UP_2)}.get(kind)
    up = {FLOOR_UP: CAVE_DOWN, FLOOR_UP_2: CAVE_DOWN_2} if level == 1 else {FLOOR_UP: FLOOR_DOWN, FLOOR_UP_2: FLOOR_DOWN_2}
    if kind in up:
        return (level - 1, up[kind])
    return {FLOOR_DOWN: (level + 1, FLOOR_UP), FLOOR_DOWN_2: (level + 1, FLOOR_UP_2)}.get(kind)


def level_grid(m, cleared=True, live=None):
    """A map's grid for walking across a dungeon: warps are walls (stepping
    on one takes the player away), obstacles too unless cleared. live: the
    blocks people stand on now, if known."""
    g = Grid(m['blocks'], m['w'], ground_only=True)
    g.ice = m['grid'].ice
    g.blocked = set(m['grid'].blocked)
    if cleared:
        g.blocked -= {(o['x'], o['y']) for o in m['objs'] if o['kind'] in OBSTACLE_OBJS}
    if live is not None:
        # Trainers who saw the player walked up to them: where they stand
        # now counts, not where they started.
        g.blocked -= {(o['x'], o['y']) for o in m['objs'] if o['kind'] == 1}
        g.blocked |= live
    g.blocked |= set(m['warps'].values())
    return g


def dungeon_route(maps, d, level, start, goal_level, goal, cleared=True):
    """The way from cell start on level to cell goal on goal_level across the
    dungeon's ladders: a list of (level, from cell, ladder kind or None for
    the goal), or None. With cleared, boulders and cracked rocks count as
    gone (the badges are held)."""
    import heapq
    levels = dungeon_levels(maps, d)
    grids = {k: level_grid(m, cleared) for k, m in levels.items()}
    best = {(level, start): 0}
    prev = {}
    q = [(0, level, start)]
    while q:
        c, lv, cell = heapq.heappop(q)
        if c > best.get((lv, cell), 1 << 30):
            continue
        if (lv, cell) == (goal_level, goal):
            out, node = [], (lv, cell)
            while node in prev:
                node, step = prev[node]
                out.append(step)
            return out[::-1]
        m, g = levels[lv], grids[lv]
        targets = [(kind, xy) for kind, xy in m['warps'].items() if ladder_partner(lv, kind) and xy != cell]
        if lv == goal_level:
            targets.append((None, goal))
        for kind, xy in targets:
            g.blocked.discard(xy)
            keys = g.path(cell, xy)
            g.blocked.add(xy) if kind is not None else None
            if keys is None:
                continue
            if kind is None:
                nxt, cost = (lv, xy), c + len(keys)
            else:
                pl, pk = ladder_partner(lv, kind)
                if pl not in levels or pk not in levels[pl]['warps']:
                    continue
                nxt, cost = (pl, levels[pl]['warps'][pk]), c + len(keys) + 1
            if cost < best.get(nxt, 1 << 30):
                best[nxt] = cost
                prev[nxt] = ((lv, cell), (lv, cell, kind))
                heapq.heappush(q, (cost, nxt[0], nxt[1]))
    return None


def cave_ends(m):
    """The cells a cave's way in and way out are walked to from inside: above
    the mat, and below the door (or None)."""
    mat = m['warps'][CAVE_SOUTH]
    door = m['warps'].get(CAVE_NORTH)
    return (mat[0], mat[1] - 1), (door[0], door[1] + 1) if door else None


def load_region(seed, visit=0):
    """The generated region as the host generator sees it: a list of places
    (towns, routes and caves), each with its plan and its generated map.
    visit: the visit to the ruins to make their floors for (they shift with
    each; the game counts them in VAR_ROGUE_SEALS' high bits)."""
    with tempfile.TemporaryDirectory(prefix='rogue-region-') as tmp:
        return _load_region(seed, tmp, visit)


def _load_region(seed, tmp, visit=0):
    subprocess.run([gen_host(), str(seed), tmp], check=True, env=dict(os.environ, ROGUE_VISIT=str(visit)))
    maps, cur, curve, gyms, villain, tunnel = [], None, {}, [], {}, None
    teams, leaders = [], []
    dungeons, floors, items, folk, traits = [], [], [], [], {}
    for line in open(os.path.join(tmp, 'region.txt')):
        if line.startswith('traits '):
            # What the professor says (enum RogueTrait) and the land the map
            # draws for it: the volcano's crater cell (-1 if none), lake cells.
            traits = {k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', line)}
            traits['volcano'] = tuple(int(v) for v in re.search(r'volcano=(-?\d+),(-?\d+)', line).groups())
            # Where the volcano was planned, before the map moved the crater
            # to its town's door (MapCraterByDoor).
            m = re.search(r'planned=(-?\d+),(-?\d+)', line)
            traits['planned'] = tuple(int(v) for v in m.groups()) if m else traits['volcano']
            continue
        if line.startswith('item '):
            # What the run hands out (src/rogue_gen_items.h): src, index, item, qty.
            items.append({k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', line)})
            continue
        if line.startswith('folk '):
            folk.append(int(re.search(r'town=(\d+)', line)[1]))
            continue
        if line.startswith('villain '):
            villain = {k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', line)}
            continue
        if line.startswith('team '):
            # A villain team: index, identity, hideout, its route of grunts
            # (hint, mask), the team before's grunt there (guest), its
            # leader's legend dungeon (-1) and its admin.
            t = {k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)\b', line)}
            t['admin'] = re.search(r'admin=(\S+)', line).group(1)
            teams.append(t)
            continue
        if line.startswith('leaders '):
            leaders = line[len('leaders '):].strip()
            continue
        if line.startswith('tunnel '):
            # The tunnel: its dungeon, host and exit places, barrier (gate
            # kind and src), the badges it opens with and its exit floor.
            tunnel = {k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', line)}
            continue
        if line.startswith('  stretch') and tunnel is not None:
            # The level its floors follow with b badges held: stretch[b].
            tunnel['stretch'] = [int(v) for v in re.findall(r'b\d=(\d+)', line)]
            continue
        if line.startswith('dungeon '):
            kv = {k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', line)}
            kv['i'] = int(line.split()[1])
            dungeons.append(kv)
            cur = None
            continue
        if line.startswith('  extras ') and dungeons:
            # The dungeon's prize, choice room and camp (RogueGen_DungeonExtras).
            dungeons[-1]['extras'] = {k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', line)}
            continue
        if line.startswith('  riddle ') and dungeons:
            # A ruins vault's riddle: the tablets' words and the order to read them.
            dungeons[-1]['riddle'] = {k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', line)}
            continue
        if line.startswith('  levels ') and cur is not None and cur.get('kind') == 'floor':
            # The floor's level with each count of badges held (an optional
            # dungeon follows them: RogueGen_FloorLevel).
            cur['levels'] = [int(v) for v in re.findall(r'b\d=(\d+)', line)]
            continue
        m = re.match(r'floor (\d+) (\d+) (\d+) (\d+)', line)
        if m:
            # A dungeon floor: like a cave, kept apart from the places.
            # A forest is drawn with Petalburg Woods' tilesets.
            kind = next((d['kind'] for d in dungeons if d['i'] == int(m[1])), DUN_CAVE)
            cur = {'dungeon': int(m[1]), 'floor': int(m[2]), 'w': int(m[3]), 'h': int(m[4]), 'kind': 'floor',
                   'objs': [], 'warps': {}, 'signs': [],
                   'tileset': 'rustboro' if kind == DUN_FOREST else 'lavaridge' if kind == DUN_VOLCANO
                   # (v1.1 peaks) A tower's misty mountainside: Mt. Pyre's outdoor tileset.
                   else 'facility' if kind == DUN_TOWER else 'cave'}
            cur.update({k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', line)})
            if cur.get('room', 255) != 255:
                cur['tileset'] = room_library()[cur['room']].tileset
            cur['blocks'] = load_bin(os.path.join(tmp, 'floor%d_%d.bin' % (cur['dungeon'], cur['floor'])))
            cur['grid'] = Grid(cur['blocks'], cur['w'], ground_only=True)
            if kind == DUN_ICE and cur.get('room', 255) == 255:
                cur['grid'].ice = {(i % cur['w'], i // cur['w']) for i, b in enumerate(cur['blocks'])
                                   if (b & 0x3FF) == ICE_METATILE}
            floors.append(cur)
            continue
        if line.startswith('curve'):
            curve = {k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', line)}
            continue
        if line.startswith('gym '):
            # Each gym's town, the obstacle its badge clears and its chapter.
            gyms.append({k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', line)})
            continue
        m = re.match(r'map (\d+) (\w+) (\d+) (\d+) (\d+) name=(.*)', line)
        if m:
            cur = {'i': int(m[1]), 'kind': m[2], 'index': int(m[3]), 'w': int(m[4]), 'h': int(m[5]), 'name': m[6],
                   'links': [], 'objs': [], 'warps': {}, 'exits': {}, 'start': None, 'gates': [], 'houses': []}
            cur['blocks'] = load_bin(os.path.join(tmp, 'map%d.bin' % cur['i']))
            cur['grid'] = Grid(cur['blocks'], cur['w'], ground_only=True)
            maps.append(cur)
            continue
        if cur is None:
            continue
        kv = {k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', line)}
        word = line.split()[0] if line.split() else ''
        if word == 'place':
            cur.update(kv)
            cur['grid_pos'] = (kv['x'], kv['y'])
            cur['tileset'] = THEME_TILESETS[kv['theme']]
            if cur['kind'] == 'town' and kv['type'] == TOWN_SUMMIT:
                cur['tileset'] = 'ever_grande'  # the summit, for the League
            if cur['kind'] == 'cave':
                cur['tileset'] = 'cave'
        elif word == 'seakind':
            # v1.1 sea: what kind of sea a sea route is (SEA_OPEN, SEA_MAZE,
            # SEA_SHALLOWS: src/rogue_gen_sea.h).
            cur['sea'] = kv['sea']
        elif word == 'sig':
            # The town's signature (src/rogue_gen_signatures.h): its id,
            # whether its landmark stands, and its gift Pokemon.
            cur['sigid'] = kv['id']
            cur['sigBuilt'] = kv['built']
            cur['sigGift'] = kv['gift']
            cur['sigGiftLevel'] = kv['giftLevel']
        elif word == 'wanderer':
            # A town of the wanderer's meetings: the townsperson they stand
            # in for (an index of objs) and where they wait.
            cur['wanderer'] = kv
        elif word == 'feat':
            # The route's feature (src/rogue_gen_features.h), whether its lot
            # was laid, its double battle pair and that pair's kind.
            cur['featid'] = kv['id']
            cur['featLot'] = kv['lot']
            cur['pair'] = kv['pair']
            cur['pairKind'] = kv['pairKind']
            cur['nick'] = re.search(r'nick=(\S+)', line)[1].replace('_', ' ')
            for k in ('gift', 'giftLevel', 'item', 'qty', 'crop0', 'crop1'):
                if k in kv:
                    cur['feat' + k[0].upper() + k[1:]] = kv[k]
        elif word == 'zone':
            # v1.1: the route's zone (src/rogue_gen_zones.h), how much the
            # zone wants its kind, its weather and the weather it is known
            # for (RWEATHER_*; 0 none).
            cur.update(kv)
        elif word == 'link':
            cur['links'].append(kv)
            cur['exits'][kv['dir']] = kv['pos']
        elif word == 'obj':
            cur['objs'].append(kv)
            # The blockade's grunts leave once their admin is beaten: walks
            # that need the way past them clear the hideout first. (The
            # lookout by the hideout's door, slot 2, stands aside.)
            # So does whoever stands in a tunnel's way, once its key is held.
            if (kv['kind'] != OBJ_GRUNT or kv['slot'] >= 2) and kv['kind'] not in (OBJ_TUNNEL_GUARD, OBJ_SNORLAX):
                cur['grid'].blocked.add((kv['x'], kv['y']))
        elif word == 'warp':
            cur['warps'][kv['kind']] = (kv['x'], kv['y'])
            if kv.get('toFloor', -1) >= 0:
                # A room floor's warp: where it leads.
                cur.setdefault('dests', {})[kv['kind']] = (kv['toFloor'], kv['toWarp'])
        elif word == 'sign':
            cur.setdefault('signs', []).append(kv)
        elif word == 'shop':
            # The Mart's specialty counter: its kind and stock by badges.
            cur['shopkind'] = kv['kind']
            stock = re.search(r'items=([\d,]*)', line)[1]
            cur.setdefault('shopstock', {})[kv['badges']] = [int(x) for x in stock.split(',') if x]
        elif word == 'hidden':
            # A hidden item: k-th of the place, id its index in the region.
            cur.setdefault('hidden', []).append(kv)
        elif word == 'gate':
            cur['gates'].append(kv)
        elif word == 'house':
            cur['houses'].append(kv['interior'])
        elif word == 'start':
            cur['start'] = (kv['x'], kv['y'])
        elif word == 'truck':
            # After RUN 0's STOP: where the moving truck parks in the first
            # town (RogueGen_TruckSpot), and whether all of it stands clear.
            cur['truck'] = kv
    # The region's level curve (gym, Elite Four, Champion and rival aces)
    # rides along on the first place.
    if maps:
        maps[0]['curve'] = curve
        maps[0]['gyms'] = gyms
        # Dungeons and their floors ride along too.
        maps[0]['dungeons'] = dungeons
        maps[0]['floors'] = floors
        # The villain team: team (0 Aqua, 1 Magma), hideout dungeon (-1:
        # none), the route of its first grunts and their trainer bits.
        maps[0]['villain'] = villain
        # Every villain team (src/rogue_gen_hideout.h), team 0 first.
        maps[0]['teams'] = teams
        maps[0]['leaders'] = leaders
        # The tunnel (src/rogue_gen_tunnel.h), or None.
        maps[0]['tunnel'] = tunnel
        # Every item the run hands out, and the towns with a townsperson's gift.
        maps[0]['items'] = items
        maps[0]['folk'] = folk
        maps[0]['traits'] = traits
        # The region map screen: its height in tiles, what it covers, and
        # each place's tile.
        rm = os.path.join(tmp, 'region_map.txt')
        if os.path.exists(rm):
            info = {'pos': {}}
            for line in open(rm):
                kv = {k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', line)}
                if line.startswith('height'):
                    info['height'] = int(line.split()[1])
                elif line.startswith('extent'):
                    info['extent'] = kv
                elif line.startswith('place'):
                    info['pos'][int(line.split()[1])] = (kv['x'], kv['y'])
            # The tiles of the visible map, one row a list.
            tiles = load_bin(os.path.join(tmp, 'region_map.bin'))
            info['tiles'] = [[tiles[y * 32 + x] & 0x3FF for x in range(30)] for y in range(info.get('height', 0))]
            maps[0]['rmap'] = info
    for m in maps:
        if m.get('tileset') == 'pacifidlog':
            m['grid'].current = currents(m['blocks'], m['w'], m['tileset'])
    return maps


# The way each kind of current carries the player (MB_*_CURRENT).
CURRENT_DELTA = {'MB_NORTHWARD_CURRENT': (0, -1), 'MB_EASTWARD_CURRENT': (1, 0),
                 'MB_SOUTHWARD_CURRENT': (0, 1), 'MB_WESTWARD_CURRENT': (-1, 0)}


def currents(blocks, w, tileset):
    """The blocks of a map that carry the player along, and which way:
    {(x, y): (dx, dy)} (Grid.current)."""
    import gen_rooms
    mb = gen_rooms.behaviours(tileset)
    way = {gen_rooms.MB[k]: d for k, d in CURRENT_DELTA.items()}
    out = {}
    for i, b in enumerate(blocks):
        d = way.get(mb[b & 0x3FF] if (b & 0x3FF) < len(mb) else None)
        if d:
            out[(i % w, i // w)] = d
    return out


# Keys: what opens a way (ROGUE_KEY in include/rogue_gen.h). A badge gives
# at most one of the four obstacle moves; the League's gate needs all eight.
KEY_LEAGUE = 1 << 5
KEY_VILLAIN = 1 << 6
KEY_TUNNEL = 1 << 7  # the tunnel's barrier when a gym leader's badge opens it
KEYS_ALL = 0x7E
OBST_NAMES = {OBST_CUT: 'CUT', OBST_ROCK_SMASH: 'ROCK SMASH', OBST_STRENGTH: 'STRENGTH', OBST_SURF: 'SURF'}


def key(obstacle):
    return 1 << obstacle


def keys_won(maps, won):
    """The keys the won gyms (a bit per gym) give."""
    keys = 0
    for g in maps[0]['gyms']:
        if (won >> g['n']) & 1 and g['key']:
            keys |= key(g['key'])
    t = maps[0].get('tunnel')
    if t and t['gate'] == GATE_GIFT and (won >> t['src']) & 1:
        keys |= KEY_TUNNEL
    return keys | (KEY_LEAGUE if won == 0xFF else 0)


def tunnel_keys(maps):
    """The keys that open the region's tunnel (0 if it has none)."""
    t = maps[0].get('tunnel')
    if not t:
        return 0
    return key(t['src']) if t['gate'] == GATE_FIELD else KEY_VILLAIN if t['gate'] == GATE_VILLAIN_LEADER else KEY_TUNNEL


_FLOOR_1 = []


def floor_map(n):
    """Map number of dungeon floor n (1 based): RogueFloor1 to 4."""
    if not _FLOOR_1:
        groups = json.load(open(os.path.join(SRC, 'data', 'maps', 'map_groups.json')))
        _FLOOR_1.append(groups['gMapGroup_Rogue'].index('RogueFloor1'))
    return _FLOOR_1[0] + n - 1


def hideout(maps, team=0):
    """A villain team's hideout dungeon (the main team's by default), or
    None."""
    teams = maps[0].get('teams') or []
    h = teams[team]['hideout'] if team < len(teams) else (maps[0].get('villain', {}).get('hideout', -1) if team == 0 else -1)
    return next((d for d in maps[0].get('dungeons', []) if d['i'] == h), None) if h >= 0 else None


def team_of_dungeon(maps, d):
    """The villain team whose hideout d is, or whose leader waits before
    its legend, or None."""
    for t in maps[0].get('teams') or []:
        if t['hideout'] == d['i'] or t['leader'] == d['i']:
            return t['t']
    return None


def keys_held(maps, won):
    """The keys a player holds with these gyms won, who clears the villains'
    hideout as soon as its town is reached (it is entered from that town,
    before the blockade)."""
    keys = keys_won(maps, won)
    h = hideout(maps)
    if h is not None and h['host'] in reach_with(maps, keys):
        keys |= KEY_VILLAIN
    return keys


def key_gym(maps, obstacle):
    """The gym whose badge clears an obstacle, or None."""
    return next((g['n'] for g in maps[0]['gyms'] if g['key'] == obstacle), None)


def link_keys(by, m, l):
    """Keys needed to walk from m through link l: the League's gate, an
    obstacle on the way in or out past it from behind, Surf for the sea."""
    t = by[l['target']]
    keys = KEY_LEAGUE if l['gate'] == GATE_LEAGUE else KEY_VILLAIN if l['gate'] == GATE_VILLAIN else 0
    if t.get('obstacle') and t['guardDir'] == (l['dir'] + 2) % 4:
        keys |= key(t['obstacle'])
    if m.get('obstacle') and m['guardDir'] == l['dir']:
        keys |= key(m['obstacle'])
    if t['kind'] == 'route' and t['type'] == 8:
        keys |= key(OBST_SURF)
    return keys


def reach_with(maps, keys):
    """Places reachable from the first town with these keys, each with the
    place it is first reached from."""
    by = {m['i']: m for m in maps}
    came = {0: None}
    q = [0]
    while q:
        m = by[q.pop(0)]
        nxt = [l['target'] for l in m['links'] if link_keys(by, m, l) & keys == link_keys(by, m, l)]
        if m.get('mouth', -1) >= 0:
            nxt.append(m['mouth'])
        if m['kind'] == 'cave':
            nxt += [p for p in (m['caveIn'], m['caveOut']) if p >= 0]
        t = maps[0].get('tunnel')
        if t and keys & tunnel_keys(maps) == tunnel_keys(maps) and m['i'] in (t['host'], t['exit']):
            nxt.append(t['exit'] if m['i'] == t['host'] else t['host'])
        for n in nxt:
            if n not in came:
                came[n] = m['i']
                q.append(n)
    return came


def interior_grid(layout, w, objects):
    g = Grid(load_bin(os.path.join(SRC, 'data', 'layouts', layout, 'map.bin')), w)
    g.blocked.update(objects)
    return g


# ---------------------------------------------------------------------------
# Battle and field helpers shared by the bots and the tests

CATCH_FREE, CATCH_FIRST, CATCH_DUPE, CATCH_USED = 0, 1, 2, 3
BATTLE_TYPE_TRAINER = 1 << 3


def controller(g):
    """The player's battle controller function, without the thumb bit."""
    return g.peek(32, g.sym['gBattlerControllerFuncs']) & ~1


def at_action_menu(g):
    """True while the battle waits at "What will X do?"."""
    return controller(g) in g.all_syms['HandleInputChooseAction']


def at_move_menu(g):
    return controller(g) in g.all_syms['HandleInputChooseMove']


def catch_state(g):
    """gRogueCatchState: CATCH_FREE, CATCH_FIRST, CATCH_DUPE or CATCH_USED."""
    return g.peek(8, g.sym['gRogueCatchState'])


def trainer_battle(g):
    return g.in_battle() and (g.peek(32, g.sym['gBattleTypeFlags']) & BATTLE_TYPE_TRAINER) != 0


def fishing_step(g):
    """The step of the fishing task, or None when nobody is fishing."""
    funcs = g.all_syms['Task_Fishing']
    for i in range(16):
        task = g.sym['gTasks'] + i * 40
        if g.peek(8, task + 4) and (g.peek(32, task) & ~1) in funcs:
            return g.peek(16, task + 8)
    return None


# Arguments taken by each FC text control code.
_CTRL_ARGS = {0x01: 1, 0x02: 1, 0x03: 1, 0x04: 3, 0x05: 1, 0x06: 1, 0x08: 1, 0x0B: 2, 0x0C: 1, 0x0D: 1, 0x0E: 1,
              0x10: 2, 0x11: 1, 0x12: 1, 0x13: 1, 0x14: 1}
_PUNCT = {0xAB: '!', 0xAC: '?', 0xAD: '.', 0xAE: '-', 0xB0: '...', 0xB1: '"', 0xB2: '"', 0xB3: "'", 0xB4: "'",
          0xB8: ',', 0xBA: '/', 0xF0: ':', 0x1B: 'e', 0x5C: '(', 0x5D: ')'}


def skip_naming(g):
    """The nickname screen the professor offers for the starter: leave it
    with the name as it was (every letter rubbed out, then OK)."""
    g.wait(60)
    for _ in range(12):
        g.press('B')
        g.wait(4)
    g.press('START')
    g.wait(10)
    g.press('A')
    g.wait(120)


def decode_message(raw):
    """A message buffer (gStringVar4, gDisplayedStringBattle) to plain text.
    New lines become spaces and new paragraphs ' | '."""
    out, i = [], 0
    while i < len(raw):
        b = raw[i]
        i += 1
        if b == 0xFF:
            break
        if b in (0xFE, 0xFA):
            out.append(' ')
        elif b == 0xFB:
            out.append(' | ')
        elif b == 0xFC and i < len(raw):
            i += 1 + _CTRL_ARGS.get(raw[i], 0)
        elif b == 0xFD:
            i += 1
        elif b in _PUNCT:
            out.append(_PUNCT[b])
        else:
            out.append(CHARMAP.get(b, ''))
    return ' '.join(''.join(out).split())


def map_strip(g):
    """The region map's strip as raw text, one entry per page: a line too
    long for the strip shows a few sentences at a time, turning every few
    seconds (Task_MapInput's data: page, pages, frames)."""
    task = g.find_task('Task_MapInput')
    pages = max(1, g.task_data(task, 1)) if task is not None else 1
    out = []
    for k in range(pages):
        for _ in range(20):
            if task is None or g.task_data(task, 0) == k:
                break
            g.wait(15)
        raw = g.read(g.sym['gStringVar4'], 256)
        out.append(raw[:raw.index(0xFF)] if 0xFF in raw else raw)
    return out


def map_strip_text(g):
    """The region map's strip as plain text, all its pages."""
    return ' '.join(decode_message(raw + b'\xff') for raw in map_strip(g))


def decode_text(raw):
    """Game text bytes to ASCII, for names."""
    out = ''
    for b in raw:
        if b == 0xFF:
            break
        out += CHARMAP.get(b, '?')
    return out



