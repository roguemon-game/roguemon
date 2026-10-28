#!/usr/bin/env python3
"""Render Gen 3 map block data to a PNG using the real tileset graphics.

Usage:
  render_map.py MAP.bin WIDTH OUT.png [--primary general] [--secondary petalburg] [--grid]
  render_map.py --metatiles OUT.png [--primary general] [--secondary petalburg]

Map data is little-endian u16 per block: metatile id (bits 0-9),
collision (10-11), elevation (12-15). --grid overlays metatile ids and marks
impassable blocks, which helps when picking stamps out of existing maps.
"""
import argparse
import os
import struct

from PIL import Image, ImageDraw

ROOT = os.path.join(os.path.dirname(__file__), '..', '..')
NUM_PRIMARY = 512
NUM_PALS_PRIMARY = 6


def load_pal(path):
    lines = open(path).read().split('\n')[3:]
    cols = []
    for line in lines[:16]:
        parts = line.split()
        if len(parts) == 3:
            cols.append(tuple(int(p) for p in parts))
    return cols


def tileset_dir(name, secondary):
    return os.path.join(ROOT, 'data', 'tilesets', 'secondary' if secondary else 'primary', name)


class Tilesets:
    def __init__(self, primary, secondary):
        pdir = tileset_dir(primary, False)
        sdir = tileset_dir(secondary, True)
        self.tiles = self._tiles(pdir) + [None] * 0
        self.tiles = self._pad(self._tiles(pdir), NUM_PRIMARY) + self._tiles(sdir)
        self.metatiles = self._pad(self._metatiles(pdir), NUM_PRIMARY) + self._metatiles(sdir)
        self.attrs = self._pad(self._attrs(pdir), NUM_PRIMARY, 0) + self._attrs(sdir)
        self.pals = []
        for i in range(16):
            src = pdir if i < NUM_PALS_PRIMARY else sdir
            p = os.path.join(src, 'palettes', '%02d.pal' % i)
            self.pals.append(load_pal(p) if os.path.exists(p) else [(0, 0, 0)] * 16)
        self.cache = {}

    @staticmethod
    def _pad(lst, n, fill=None):
        return (lst + [fill] * n)[:n]

    @staticmethod
    def _tiles(d):
        img = Image.open(os.path.join(d, 'tiles.png'))
        w, h = img.size
        px = img.load()
        tiles = []
        for ty in range(h // 8):
            for tx in range(w // 8):
                tiles.append([[px[tx * 8 + x, ty * 8 + y] & 0xF for x in range(8)] for y in range(8)])
        return tiles

    @staticmethod
    def _metatiles(d):
        data = open(os.path.join(d, 'metatiles.bin'), 'rb').read()
        n = len(data) // 16
        return [struct.unpack_from('<8H', data, i * 16) for i in range(n)]

    @staticmethod
    def _attrs(d):
        data = open(os.path.join(d, 'metatile_attributes.bin'), 'rb').read()
        return list(struct.unpack('<%dH' % (len(data) // 2), data))

    def metatile_image(self, mid):
        if mid in self.cache:
            return self.cache[mid]
        img = Image.new('RGB', (16, 16), (255, 0, 255))
        mt = self.metatiles[mid] if mid < len(self.metatiles) else None
        if mt:
            px = img.load()
            for layer in range(2):
                for q in range(4):
                    e = mt[layer * 4 + q]
                    t = e & 0x3FF
                    hf, vf, pal = (e >> 10) & 1, (e >> 11) & 1, e >> 12
                    tile = self.tiles[t] if t < len(self.tiles) else None
                    if tile is None:
                        continue
                    ox, oy = (q % 2) * 8, (q // 2) * 8
                    for y in range(8):
                        for x in range(8):
                            c = tile[7 - y if vf else y][7 - x if hf else x]
                            if c == 0 and layer == 1:
                                continue
                            if c == 0 and layer == 0:
                                c = 0
                            px[ox + x, oy + y] = self.pals[pal][c] if c < len(self.pals[pal]) else (0, 0, 0)
        self.cache[mid] = img
        return img


def render(blocks, width, ts, grid=False):
    height = len(blocks) // width
    img = Image.new('RGB', (width * 16, height * 16))
    for i, b in enumerate(blocks):
        img.paste(ts.metatile_image(b & 0x3FF), ((i % width) * 16, (i // width) * 16))
    if grid:
        big = img.resize((img.width * 2, img.height * 2), Image.NEAREST)
        d = ImageDraw.Draw(big)
        for i, b in enumerate(blocks):
            x, y = (i % width) * 32, (i // width) * 32
            d.rectangle([x, y, x + 31, y + 31], outline=(0, 0, 0))
            d.text((x + 2, y + 2), '%X' % (b & 0x3FF), fill=(255, 255, 255))
            if b & 0x0C00:
                d.line([x, y, x + 31, y + 31], fill=(255, 0, 0))
        return big
    return img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('args', nargs='*')
    ap.add_argument('--primary', default='general')
    ap.add_argument('--secondary', default='petalburg')
    ap.add_argument('--grid', action='store_true')
    ap.add_argument('--metatiles', action='store_true')
    a = ap.parse_args()
    ts = Tilesets(a.primary, a.secondary)
    if a.metatiles:
        ids = [i for i in range(len(ts.metatiles)) if ts.metatiles[i]]
        blocks = [i for i in range(max(ids) + 1)]
        render(blocks, 16, ts, grid=True).save(a.args[0])
        return
    path, width, out = a.args
    data = open(path, 'rb').read()
    blocks = list(struct.unpack('<%dH' % (len(data) // 2), data))
    render(blocks, int(width), ts, a.grid).save(out)


if __name__ == '__main__':
    main()
