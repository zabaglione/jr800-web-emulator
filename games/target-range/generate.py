# SPDX-License-Identifier: MIT
import sys,json,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;levels=[]
for stage in range(3):
 r=random.Random(37000+stage);levels.append([([255,*r.sample(range(9),2)] if n%5==4 else [*r.sample(range(9),2),255]) for n in range(20)])
s=[]
for kind in range(5):
 b=Bitmap(24,16);b.rect(1,1,22,14)
 if kind==1:
  for y in range(-6,7):
   for x in range(-6,7):
    if x*x+y*y in range(24,38) or x*x+y*y<=5:b.dot(12+x,8+y)
 if kind==2:
  for d in (0,1):b.line(6,3+d,18,12+d);b.line(6,12+d,18,3+d)
 if kind==3:b.line(6,8,10,12);b.line(10,12,18,4);b.line(6,7,10,11);b.line(10,11,18,3)
 if kind==4:b.rect(6,4,13,8);b.line(6,4,18,11);b.line(6,11,18,4)
 s.append(b)
assets(root,'TARGET RANGE','target-range',3,3,3,2,s,3,asm_bytes('range_levels',[v for stage in levels for row in stage for v in row]),aux=('RETRY','RESET'))
(root/'levels.json').write_text(json.dumps(levels)+'\n')
