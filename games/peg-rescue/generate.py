# SPDX-License-Identifier: MIT
import sys,json,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent
holes={y*7+x for y in range(7) for x in range(7) if (y in (2,3,4) or (y in (0,1,5) and x in (2,3,4)) or (y==6 and x==3))}
assert len(holes)==31
links=[];jumps=[]
for p in range(49):
 for dx,dy in ((0,-1),(0,1),(-1,0),(1,0)):
  x,y=p%7,p//7;m=(y+dy)*7+x+dx;q=(y+dy*2)*7+x+dx*2
  valid=p in holes and 0<=x+dx*2<7 and 0<=y+dy*2<7 and m in holes and q in holes
  links.extend([m,q] if valid else [255,255])
  if valid:jumps.append((p,m,q))
from puzzle_assets import campaign, level_records, challenge_data
levels=campaign(root)
sprites=[]
for n in range(5):
 b=Bitmap(16,8)
 if n!=2:
  for y in range(-3,4):
   for x in range(-3,4):
    r=x*x+y*y
    if r<=10 and (n in (1,3) or r>=6):b.dot(x+7,y+3)
 if n in (1,3):b.dot(6,1,0);b.dot(7,1,0);b.line(5,7,10,7)
 if n==3:b.line(1,0,1,7);b.line(13,0,13,7)
 if n==4:b.line(5,3,9,3);b.line(7,1,7,5)
 sprites.append(b)
data=asm_bytes('peg_links',links)+level_records([s['initial']['board'] for s in levels])+challenge_data(root,levels,7,2,1,7)
assets(root,'PEG RESCUE','peg-rescue',7,7,2,1,sprites,40,data,stat='LEFT',action='SPACE')
