#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Render README sprites directly from the game's column-oriented PCG data."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent
NAMES = {1: 'wall', 2: 'floor', 4: 'stairs', 5: 'relic', 6: 'player',
         7: 'food', 8: 'potion', 9: 'scroll', 10: 'weapon', 11: 'armor', 12: 'gold',
         13: 'slime', 14: 'goblin', 15: 'orc', 16: 'wraith', 17: 'rat', 18: 'bat',
         19: 'skeleton', 20: 'troll', 21: 'thief', 22: 'snake', 23: 'rust-beast', 24: 'centaur'}

def render():
    source = (ROOT / 'assets.s').read_text()
    block = source.split('tiles:\n', 1)[1].split('difficulty_names:', 1)[0]
    patterns = [[int(n) for n in row.split(',')] for row in re.findall(r'\.byte ([\d,]+)', block)]
    assert len(patterns) == 25 and all(len(row) == 8 for row in patterns)
    target = ROOT / 'images'
    target.mkdir(exist_ok=True)
    for index, name in NAMES.items():
        pixels = ''.join(f'M{x+1} {y+1}h1v1h-1z' for x, column in enumerate(patterns[index])
                         for y in range(8) if column & (1 << y))
        svg = ('<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" '
               'viewBox="0 0 10 10" shape-rendering="crispEdges">'
               f'<title>{name}</title><rect width="10" height="10" fill="#b0b4a6"/>'
               f'<path fill="#3d433d" d="{pixels}"/></svg>\n')
        (target / f'{name}.svg').write_text(svg)
    print(f'Rendered {len(NAMES)} PCG sprites from assets.s')

if __name__ == '__main__':
    render()
