# SPDX-License-Identifier: MIT
import sys,json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;levels=[]
for stage in range(12):
 # Gradually fill the wall, then increase its durability. Later boards never
 # lose a quarter of their bricks merely because their motif changes.
 count=min(24,18+stage)
 order=sorted(range(24),key=lambda p:((p%8*5+p//8*3+stage*7)%24,p))
 present=set(order[:count]);armored=set(order[:max(0,stage-2)])
 levels.append([0 if p not in present else 2 if p in armored else 1 for p in range(24)])
from depth_art import raised
sprites=[Bitmap(16,8)]
for strength in (1,2):
 b=Bitmap(16,8);raised(b,1,1,15,7)
 if strength==2:b.line(3,3,12,3);b.line(3,4,12,4)
 sprites.append(b)
assets(root,'WALL BREAK','wall-break',8,3,2,1,sprites,12,asm_bytes('wall_levels',sum(levels,[])),aux=('RELAUNCH','RESET'))
(root/'levels.json').write_text(json.dumps(levels)+'\n')
