# SPDX-License-Identifier: MIT
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
sprites=[]
for n in range(4):
 b=Bitmap(16,8);b.rect(0,0,16,8)
 if n in (1,2):
    b.line(5,1,10,1);b.line(5,6,10,6);b.line(3,3,3,4);b.line(12,3,12,4)
    for x,y in [(4,2),(11,2),(4,5),(11,5)]:b.dot(x,y)
    if n==2:b.rect(4,3,8,2,1,True);b.line(5,2,10,2);b.line(5,5,10,5)
 elif n==3:b.line(6,3,9,3);b.line(7,2,7,5)
 sprites.append(b)
links=[]
for dy in (-1,0,1):
 for dx in (-1,0,1):
    if not (dx or dy):continue
    for p in range(36):
        x,y=p%6+dx,p//6+dy;links.append(y*6+x if 0<=x<6 and 0<=y<6 else 255)
weights=[]
for y in range(6):
 for x in range(6):
    a,b=min(x,5-x),min(y,5-y)
    weights.append((60 if a==b==0 else -40 if a==b==1 else -24 if min(a,b)==0 and max(a,b)==1 else 16 if min(a,b)==0 else 0)&255)
assets(Path(__file__).parent,'REVERSI MINI','reversi-mini',6,6,2,1,sprites,1,asm_bytes('rev_links',links)+asm_bytes('rev_weights',weights),stat='YOU',action='SPACE',aux=('UNDO TURN','RESET'))
