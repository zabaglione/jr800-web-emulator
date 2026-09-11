# SPDX-License-Identifier: MIT
import sys,json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;levels=[]
for stage in range(12):
 cells=[]
 for y in range(3):
  for x in range(8):
   present=stage%4==0 or (x+y+stage)%4!=0
   cells.append((2 if stage>=4 and (x*3+y+stage)%5==0 else 1) if present else 0)
 levels.append(cells)
sprites=[Bitmap(16,8)]
for strength in (1,2):
 b=Bitmap(16,8);b.rect(1,1,14,6)
 if strength==2:b.line(3,3,12,3);b.line(3,4,12,4)
 sprites.append(b)
assets(root,'WALL BREAK','wall-break',8,3,2,1,sprites,12,asm_bytes('wall_levels',sum(levels,[])),aux=('RELAUNCH','RESET'))
(root/'levels.json').write_text(json.dumps(levels)+'\n')
