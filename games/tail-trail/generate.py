# SPDX-License-Identifier: MIT
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;sprites=[Bitmap(8,8)]
# Body, food, then four directional heads.
b=Bitmap(8,8);b.rect(1,1,6,6);b.rect(3,3,2,2,1,True);sprites.append(b)
b=Bitmap(8,8)
for p,q in [((3,0),(7,4)),((7,4),(3,7)),((3,7),(0,4)),((0,4),(3,0))]:b.line(*p,*q)
b.dot(3,3);b.dot(4,3);sprites.append(b)
for d in range(4):
 b=Bitmap(8,8);b.rect(0,0,8,8,1,True)
 eyes=[[(2,1),(5,1)],[(2,5),(5,5)],[(1,2),(1,5)],[(5,2),(5,5)]][d]
 for x,y in eyes:b.rect(x,y,2,2,0,True)
 # grid.s inverts the cursor tile; pre-invert heads for a solid visible head.
 b.p=[[1-v for v in row] for row in b.p]
 sprites.append(b)
assets(root,'TAIL TRAIL','tail-trail',14,7,1,1,sprites,3,asm_bytes('tail_goals',[10,20,30])+asm_bytes('tail_speeds',[24,18,12]),aux=('PAUSE','RESET'))
