# SPDX-License-Identifier: MIT
import sys,json,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;levels=[]
for stage in range(12):
 r=random.Random(32000+stage);b=[1 if x in (0,14) or y in (0,6) or (x%2==0 and y%2==0) else 0 for y in range(7) for x in range(15)]
 keycols=[5+2*(stage%3),5+2*((stage+1)%4),7+2*((stage+2)%3)]
 for y,x in zip((1,3,5),keycols):b[y*15+x]=4
 candidates=[p for p in range(105) if b[p]==0 and p%15>3 and p not in (49,77,88)];r.shuffle(candidates)
 for p in candidates[:8+stage%5]:b[p]=2
 b[88]=3;assert b.count(4)==3;levels.append(b)
s=[Bitmap(8,8)]
b=Bitmap(8,8);b.rect(0,0,8,8);b.line(0,0,7,7);s.append(b)
b=Bitmap(8,8);b.rect(1,1,6,6);b.line(1,1,6,6);b.line(1,6,6,1);s.append(b)
b=Bitmap(8,8);b.rect(1,0,6,8);b.line(3,2,5,4);b.line(5,4,3,6);s.append(b)
b=Bitmap(8,8);b.rect(0,0,8,8);b.line(2,1,2,6);b.line(2,4,5,1);b.line(2,3,5,6);s.append(b)
b=Bitmap(8,8);b.rect(1,1,3,3);b.line(3,3,6,6);b.dot(6,4);s.append(b)
b=Bitmap(8,8);b.rect(2,2,4,5,1,True);b.line(4,1,6,0);s.append(b)
b=Bitmap(8,8);b.line(0,3,7,3);b.line(3,0,3,7);b.line(1,1,6,6);b.line(1,6,6,1);s.append(b)
b=Bitmap(8,8);b.rect(1,1,6,6,1,True);b.dot(2,3,0);b.dot(5,3,0);b.line(1,7,2,6);b.line(5,6,6,7);s.append(b)
b=Bitmap(8,8);b.rect(2,0,4,3,1,True);b.line(1,4,6,4);b.line(3,3,3,6);b.dot(2,7);b.dot(5,7);b.p=[[1-v for v in row] for row in b.p];s.append(b)
assets(root,'BOMB VAULT','bomb-vault',15,7,1,1,s,12,asm_bytes('bomb_levels',sum(levels,[])),aux=('PAUSE','RESET'))
(root/'levels.json').write_text(json.dumps(levels)+'\n')
