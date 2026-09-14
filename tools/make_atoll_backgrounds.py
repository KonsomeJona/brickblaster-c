#!/usr/bin/env python3
"""Derive the atoll world backgrounds (sprites/02_01..08.png) from the space
set (00_01..08.png): the night sky becomes deep water, the stars become
drifting plankton, and the warm planets turn into coral.

usage: python3 tools/make_atoll_backgrounds.py   (from the repository root)
"""
from PIL import Image

for i in range(1, 9):
    src = Image.open(f"assets/sprites/00_{i:02d}.png").convert("RGBA")
    px = src.load()
    out = Image.new("RGBA", src.size)
    dst = out.load()
    for y in range(src.height):
        # Light falls from the surface: the top of the screen is brighter.
        depth = 1.0 - 0.35 * y / src.height
        for x in range(src.width):
            r, g, b, a = px[x, y]
            lum = 0.30 * r + 0.59 * g + 0.11 * b
            warm = max(0.0, min(1.0, (r - b) / 90.0))
            water = (lum * 0.20, lum * 0.80 + 18 * depth, lum * 0.95 + 38 * depth)
            coral = (lum * 1.25 + 20, lum * 0.62, lum * 0.55)
            c = [w * (1 - warm) + k * warm for w, k in zip(water, coral)]
            dst[x, y] = tuple(max(0, min(255, int(v * depth + 0.5))) for v in c) + (a,)
    out.save(f"assets/sprites/02_{i:02d}.png", optimize=True)
    print(f"assets/sprites/02_{i:02d}.png")
