#!/usr/bin/env python3
"""Render a whole generated region to one PNG.

Usage: preview_region.py SEED OUT.png [--scale S]
Uses the host build of the generator (tools/bin/gen_host, or $GEN_HOST).
Places joined by map edges are drawn where the game joins them. A place
reached through a cave is drawn above the place whose mouth leads into the
cave, and the caves themselves along the right edge.
"""
import argparse
import os
import sys
from collections import deque

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(__file__))
from render_map import Tilesets, render  # noqa: E402
from rogue_emu import load_region  # noqa: E402

# obj kind: professor, trainer, item, villager, berry, mon, rival, cut tree, rock, boulder
OBJ_COLORS = {0: (255, 255, 0), 1: (255, 60, 60), 2: (60, 160, 255), 3: (255, 255, 255), 6: (255, 0, 200),
              7: (0, 200, 0), 8: (150, 100, 50), 9: (120, 120, 120)}


def layout(maps):
    pos = {0: (0, 0)}
    q = deque([0])
    while q:
        i = q.popleft()
        m = maps[i]
        ax, ay = pos[i]
        for l in m['links']:
            t = l['target']
            if t in pos:
                continue
            b = maps[t]
            d, off = l['dir'], l['offset']
            if d == 0:
                pos[t] = (ax + off, ay - b['h'])
            elif d == 2:
                pos[t] = (ax + off, ay + m['h'])
            elif d == 1:
                pos[t] = (ax + m['w'], ay + off)
            else:
                pos[t] = (ax - b['w'], ay + off)
            q.append(t)
        # Through a cave: the far side goes above this place.
        if m.get('mouth', -1) >= 0:
            cave = maps[m['mouth']]
            out = cave.get('caveOut', -1)
            if cave.get('caveIn') == i and out >= 0 and out not in pos:
                pos[out] = (ax, ay - maps[out]['h'] - 4)
                q.append(out)
    right = max(pos[k][0] + maps[k]['w'] for k in pos)
    top = min(p[1] for p in pos.values())
    for m in maps:
        if m['i'] not in pos:
            pos[m['i']] = (right + 3, top)
            top += m['h'] + 3
    return pos


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('seed')
    ap.add_argument('out')
    ap.add_argument('--scale', type=float, default=0)
    a = ap.parse_args()
    maps = load_region(int(a.seed))
    pos = layout(maps)
    minx = min(p[0] for p in pos.values())
    miny = min(p[1] for p in pos.values())
    maxx = max(pos[m['i']][0] + m['w'] for m in maps)
    maxy = max(pos[m['i']][1] + m['h'] for m in maps)
    world = Image.new('RGB', ((maxx - minx) * 16, (maxy - miny) * 16), (20, 20, 30))
    sets = {}
    for m in maps:
        name = m['tileset']
        if name not in sets:
            sets[name] = Tilesets('general', name)
        img = render(m['blocks'], m['w'], sets[name])
        d = ImageDraw.Draw(img)
        for o in m['objs']:
            x, y = o['x'] * 16, o['y'] * 16
            d.rectangle([x + 3, y + 3, x + 12, y + 12], fill=OBJ_COLORS.get(o['kind'], (0, 255, 255)), outline=(0, 0, 0))
        for w in m['warps'].values():
            x, y = w[0] * 16, w[1] * 16
            d.rectangle([x, y, x + 15, y + 15], outline=(255, 0, 255), width=2)
        for g in m['gates']:
            x, y = g['x'] * 16, g['y'] * 16
            d.line([x, y, x + 15, y + 15], fill=(255, 128, 0), width=2)
        d.text((4, 4), '%d %s %s' % (m['i'], m['kind'], m['name'] if m['kind'] == 'town' else m['index']), fill=(255, 255, 255))
        px, py = pos[m['i']]
        world.paste(img, ((px - minx) * 16, (py - miny) * 16))
    scale = a.scale or (0.5 if world.width > 2000 else 1)
    if scale != 1:
        world = world.resize((int(world.width * scale), int(world.height * scale)), Image.LANCZOS)
    world.save(a.out)


if __name__ == '__main__':
    main()
