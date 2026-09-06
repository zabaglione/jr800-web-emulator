# SPDX-License-Identifier: MIT
"""Render the actual 16x16 PCG bytes for the player guide."""
from pathlib import Path
import re

root = Path(__file__).resolve().parent
source = (root / 'pcg.s').read_text()
(root / 'images').mkdir(exist_ok=True)
for name in ('dino_run_a', 'dino_run_b', 'dino_air', 'dino_dead', 'cactus_a', 'cactus_b', 'rock', 'crate'):
    body = source.split(name + ':', 1)[1].split('.global', 1)[0]
    values = [int(token[1:], 16) for line in body.splitlines() if '.byte' in line
              for token in re.findall(r'\$[0-9A-Fa-f]+', line.split(';')[0])][:32]
    assert len(values) == 32
    points = []
    for x in range(16):
        for y in range(16):
            if values[x * 2 + y // 8] & (1 << (y % 8)):
                points.append(f'M{x} {y}h1v1h-1z')
    width = 16
    (root / 'images' / (name + '.svg')).write_text(
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width*4}" height="64" '
        f'viewBox="0 0 {width} 16" shape-rendering="crispEdges"><title>{name}</title>'
        f'<rect width="{width}" height="16" fill="#b1b5a8"/>'
        f'<path fill="#4d544f" d="{"".join(points)}"/></svg>\n')

# Matches draw_pit: 24 empty ground columns, with a vertical lip at each edge.
(root / 'images' / 'pit.svg').write_text(
    '<svg xmlns="http://www.w3.org/2000/svg" width="144" height="64" '
    'viewBox="0 0 36 16" shape-rendering="crispEdges"><title>Pit</title>'
    '<rect width="36" height="16" fill="#b1b5a8"/>'
    '<path fill="#4d544f" d="M0 8h6v1H0zM30 8h6v1h-6zM5 8h1v8H5zM30 8h1v8h-1z"/></svg>\n')
