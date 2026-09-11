# SPDX-License-Identifier: MIT
"""Offline authoring support. Solvers run at build authoring time, never in the host."""
import json
from pathlib import Path

DIRECTIONS = ((0,-1,'up'),(0,1,'down'),(-1,0,'left'),(1,0,'right'))

def save(path, stages):
    root = Path(path).parent
    assert len(stages) == 40
    seen = set()
    for i, s in enumerate(stages):
        assert 0 < s['par'] <= 9999 and s['normal'] and s['bonus']
        signature = json.dumps(s['initial'], sort_keys=True)
        assert signature not in seen, f'Duplicate stage {i+1}'
        seen.add(signature)
        s['tier'] = ('INTRO', 'STANDARD', 'HARD', 'EXPERT')[i//10]
    doc = {'version': 1, 'game': root.name, 'stages': stages}
    root.joinpath('challenges.json').write_text(json.dumps(doc, separators=(',',':'))+'\n')
    print(f'{root.name}: 40 certified stages; par {min(s["par"] for s in stages)}..{max(s["par"] for s in stages)}')

def grid_neighbors(width, height):
    return [[(y+dy)*width+x+dx for dx,dy,_ in DIRECTIONS
             if 0<=x+dx<width and 0<=y+dy<height]
            for y in range(height) for x in range(width)]
