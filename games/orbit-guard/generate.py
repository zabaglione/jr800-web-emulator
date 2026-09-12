# SPDX-License-Identifier: MIT
import sys,json,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent
rings=[[(7,2),(8,2),(9,3),(8,4),(7,4),(6,4),(5,3),(6,2)],[(7,1),(10,1),(11,3),(10,5),(7,5),(4,5),(3,3),(4,1)],[(7,0),(12,0),(13,3),(12,6),(7,6),(2,6),(1,3),(2,0)]]
bg=Bitmap(128,56)
for points in rings:
 for (x,y),(X,Y) in zip(points,points[1:]+points[:1]):bg.line(x*8+4,y*8+4,X*8+4,Y*8+4)
for y in range(56):
 for x in range(128):
  if (x+y)%3==0:bg.dot(x,y,0)
s=[];unique={};back=[];raw=bg.bytes()
for cell in range(112):
 x,y=cell%16,cell//16;t=tuple(raw[y*128+x*8:y*128+x*8+8])
 if t not in unique:
  b=Bitmap(8,8)
  for X,v in enumerate(t):
   for Y in range(8):b.p[Y][X]=(v>>Y)&1
  unique[t]=len(s);s.append(b)
 back.append(unique[t])
enemy_base=len(s)
for hp in (1,2):
 b=Bitmap(8,8);b.rect(1,1,6,6,1,hp==1);b.line(2,2,5,5);b.line(2,5,5,2);s.append(b)
warning=len(s);b=Bitmap(8,8);b.rect(1,1,6,6);b.line(3,2,3,4);b.dot(3,6);s.append(b)
beam=len(s);b=Bitmap(8,8);b.line(0,3,7,3);b.line(3,0,3,7);b.rect(2,2,3,3);s.append(b)
player=len(s)
for dx,dy in ((0,-1),(1,-1),(1,0),(1,1),(0,1),(-1,1),(-1,0),(-1,-1)):
 b=Bitmap(8,8);b.rect(2,2,4,4,1,True);b.line(4,4,4+dx*3,4+dy*3);b.p=[[1-v for v in row] for row in b.p];s.append(b)
slots=[255]*112
for r,ring in enumerate(rings):
 for a,(x,y) in enumerate(ring):slots[y*16+x]=r*8+a
levels=[]
for stage in range(12):
 r=random.Random(36000+stage);wave=[];last=-1
 for i in range(12+stage):
  a=r.choice([n for n in range(8) if n!=last]);wave.append(a+(8 if stage>=4 and i%3==0 else 0));last=a
 levels.append(wave)
data=f'.equ ORBIT_WARNING,{warning}\n.equ ORBIT_ENEMY,{enemy_base}\n.equ ORBIT_BEAM,{beam}\n.equ ORBIT_PLAYER,{player}\n'+asm_bytes('orbit_x',[x*8 for ring in rings for x,y in ring])+asm_bytes('orbit_y',[y*8+8 for ring in rings for x,y in ring])+asm_bytes('orbit_background',back)+asm_bytes('orbit_slots',slots)+asm_bytes('orbit_levels',sum([v+[255]*(24-len(v)) for v in levels],[]))
assets(root,'ORBIT GUARD','orbit-guard',16,7,1,1,s,12,data,aux=('PULSE','RESET'))
(root/'levels.json').write_text(json.dumps(levels)+'\n')
