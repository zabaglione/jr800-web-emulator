# SPDX-License-Identifier: MIT
import sys,json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;arenas=[]
patterns=[[],[7,23,87,103],[35,36,75,76],[6,9,22,25,86,89,102,105],[17,18,19,92,93,94],[38,41,70,73]]
for cells in patterns:
 b=[0]*112
 for p in cells:b[p]=3
 assert b[50]==b[61]==0;arenas.append(b)
s=[Bitmap(8,8)]
b=Bitmap(8,8);b.rect(1,2,6,4,1,True);s.append(b)
b=Bitmap(8,8)
for y in range(1,7):
 for x in range(1,7):
  if (x+y)%2==0:b.dot(x,y)
s.append(b)
b=Bitmap(8,8);b.rect(0,0,8,8);b.line(1,1,6,6);b.line(1,6,6,1);s.append(b)
b=Bitmap(8,8);b.rect(1,1,6,6,1,True);b.dot(3,3,0);b.dot(4,3,0);b.p=[[1-v for v in row] for row in b.p];s.append(b)
b=Bitmap(8,8);b.line(3,0,7,4);b.line(7,4,3,7);b.line(3,7,0,4);b.line(0,4,3,0);b.rect(2,2,3,3);s.append(b)
assets(root,'GRID CLAIM','grid-claim',16,7,1,1,s,3,asm_bytes('claim_arenas',sum(arenas,[])),aux=('PAUSE','RESET'))
(root/'levels.json').write_text(json.dumps(arenas)+'\n')
