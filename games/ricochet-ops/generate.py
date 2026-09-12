# SPDX-License-Identifier: MIT
import sys,json,random,itertools,collections
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;dirs=[(0,-1),(1,-1),(1,0),(1,1),(0,1),(-1,1),(-1,0),(-1,-1)];slash=[2,1,0,7,6,5,4,3];back=[6,5,4,3,2,1,0,7]
def trace(board,col,angle):
 p=40+col;start=p;seen=set();path=[];reflections=0
 while True:
  dx,dy=dirs[angle];x=p%8+dx;y=p//8+dy
  if not(0<=x<8 and 0<=y<6):break
  p=y*8+x
  if board[p]==1 or p==start or (p,angle) in seen:break
  seen.add((p,angle));path.append(p)
  if board[p] in (2,3):angle=(slash if board[p]==2 else back)[angle];reflections+=1
 return path,reflections
from puzzle_assets import campaign,level_records,challenge_data
levels=campaign(root)
s=[Bitmap(16,8)]
b=Bitmap(16,8);b.rect(0,0,16,8);b.line(0,0,15,7);s.append(b)
b=Bitmap(16,8);b.line(1,7,14,0);b.line(2,7,15,0);s.append(b)
b=Bitmap(16,8);b.line(1,0,14,7);b.line(2,0,15,7);s.append(b)
b=Bitmap(16,8);b.rect(4,1,8,6);b.rect(6,3,4,2,1,True);s.append(b)
for dx,dy in dirs:
 b=Bitmap(16,8);b.rect(5,2,5,4,1,True);b.line(7,4,7+dx*6,4+dy*3);s.append(b)
b=Bitmap(16,8)
for x in (2,6,10,14):b.dot(x,4)
s.append(b)
b=Bitmap(16,8);b.rect(6,2,4,4,1,True);s.append(b)
assets(root,'RICOCHET OPS','ricochet-ops',8,6,2,1,s,40,'.section .runtime, code\n'+asm_bytes('rico_ammos',[s['initial']['ammo'] for s in levels])+'.section .data, data\n'+level_records([s['initial']['board'] for s in levels])+challenge_data(root,levels,8,2,1,6),aux=('UNDO SHOT','RESET'))

# Navigation is implemented by the cannon controls; no neighbor table is read.
p=root/'assets.s';source=p.read_text();start=source.index('neighbors:\n');end=source.index('.section .runtime',start);p.write_text(source[:start]+source[end:])
