# SPDX-License-Identifier: MIT
import sys,json,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent
coords=[(1,1),(2,1),(3,1),(4,1),(4,2),(1,5),(2,5),(3,5),(4,5),(4,4),(4,3),(5,3),(6,3),(7,3),(8,3),(8,2),(8,1),(9,1),(10,1),(11,1),(12,1),(8,4),(8,5),(9,5),(10,5),(11,5),(12,5)]
nexts=[1,2,3,4,10,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,255,22,23,24,25,26,255]
board=[0]*112
for a,dest in enumerate(nexts):
 for b in ([15,21] if a==14 else [dest]):
  if b==255:continue
  x,y=coords[a];xx,yy=coords[b];dx,dy=xx-x,yy-y
  bit={(0,-1):1,(1,0):2,(0,1):4,(-1,0):8}[(dx,dy)];opposite={1:4,2:8,4:1,8:2}[bit];board[y*16+x]|=bit;board[yy*16+xx]|=opposite
s=[]
for mask in range(16):
 b=Bitmap(8,8)
 for bit,x,y in ((1,3,0),(2,7,3),(4,3,7),(8,0,3)):
  if mask&bit:b.line(3,3,x,y);b.line(4,4,min(7,x+1),min(7,y+1))
 s.append(b)
for letter in ('A','B'):
 b=Bitmap(8,8);b.text(letter,1,0);b.line(0,7,7,7);s.append(b)
b=Bitmap(8,8);b.line(0,3,7,3);b.line(2,1,6,5);b.line(2,5,6,1);s.append(b)
b=Bitmap(8,8);b.line(0,3,7,3);b.rect(3,1,2,5);s.append(b)
levels=[]
for stage in range(12):
 r=random.Random(43000+stage);count=6+(stage//4)*2;trains=[];arrival=1
 for n in range(count):
  if n:arrival+=r.choice([2,3,4])
  entry=(n+(stage%2))%2 if stage<4 else r.randrange(2);dest=r.randrange(2);trains.append([arrival,entry,dest])
 levels.append(trains)
raw=[]
for l in levels:raw+=[len(l)]+sum(l,[])+[255]*(30-len(l)*3)
extra=asm_bytes('dispatch_track',board)+asm_bytes('dispatch_node_cells',[y*16+x for x,y in coords])+asm_bytes('dispatch_next_nodes',nexts)+asm_bytes('dispatch_schedules',raw)
assets(root,'RAIL DISPATCH','rail-dispatch',16,7,1,1,s,12,extra,aux=('RUN / PAUSE','RESET'))
(root/'levels.json').write_text(json.dumps(levels)+'\n');(root/'network.json').write_text(json.dumps({'coords':coords,'next':nexts,'track':board})+'\n');print('Built 12 timetables and a 27-node two-entry railway')
