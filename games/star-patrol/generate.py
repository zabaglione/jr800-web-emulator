# SPDX-License-Identifier: MIT
import sys,json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;levels=[]
for stage in range(12):
 levels.append([0 if stage%4 and (i+stage)%7==0 else 2 if stage>=4 and (i*3+stage)%4==0 else 1 for i in range(20)])
s=[Bitmap(8,8)]
for hp in (1,2):
 b=Bitmap(8,8);b.line(2,1,5,1);b.rect(1,2,6,3,1,True);b.dot(2,3,0);b.dot(5,3,0);b.line(0,5,0,6);b.line(7,5,7,6);b.dot(2,6);b.dot(5,6)
 if hp==2:b.line(1,0,6,0);b.line(0,1,0,4);b.line(7,1,7,4)
 s.append(b)
for hp in (1,2):
 b=Bitmap(8,8);b.line(1,2,6,2);b.line(0,3,7,3);b.line(0,4,7,4);b.line(0,5,7,5)
 if hp==1:b.rect(3,2,2,2,0,True);b.dot(6,5,0)
 s.append(b)
b=Bitmap(8,8);b.line(3,1,3,6);b.line(4,1,4,6);s.append(b)
b=Bitmap(8,8);b.line(2,0,4,2);b.line(4,2,2,4);b.line(2,4,4,6);s.append(b)
b=Bitmap(8,8);b.line(3,0,3,5);b.line(4,0,4,5);b.rect(1,4,6,3,1,True);b.line(0,6,7,6);b.p=[[1-v for v in row] for row in b.p];s.append(b)
assets(root,'STAR PATROL','star-patrol',16,7,1,1,s,12,asm_bytes('star_levels',sum(levels,[])),aux=('PAUSE','RESET'))
(root/'levels.json').write_text(json.dumps(levels)+'\n')
