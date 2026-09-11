# SPDX-License-Identifier: MIT
import sys,json,random,collections
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;rooms=[]
for room in range(12):
 r=random.Random(41000+room);b=[1 if y in (0,6) or x in (0,13,5,9) else 0 for y in range(7) for x in range(14)]
 for x in (5,9):
  for y in r.sample(range(1,6),2):b[y*14+x]=0
 d={15:0};q=collections.deque([15])
 while q:
  p=q.popleft()
  for n in (p-14,p+14,p-1,p+1):
   if b[n]==0 and n not in d:d[n]=d[p]+1;q.append(n)
 assert len(d)==sum(v==0 for v in b)
 b[82]=6
 near=[p for p in d if p!=15 and d[p]<=5];r.shuffle(near)
 for p,v in zip(near[:3],(2,3,4)):b[p]=v
 far=[p for p in d if b[p]==0 and d[p]>=9];r.shuffle(far)
 for p,v in zip(far[:7],(7,7,7,5,4,9,9)):b[p]=v
 assert b.count(7)==3 and b.count(5)==1
 rooms.append(b)
s=[Bitmap(8,8)]
b=Bitmap(8,8);b.rect(0,0,8,8);b.line(0,4,7,4);b.line(3,0,3,4);b.line(5,4,5,7);s.append(b)
b=Bitmap(8,8);b.line(1,7,6,1);b.line(2,7,7,1);b.line(0,4,4,7);s.append(b)
b=Bitmap(8,8);b.line(1,1,6,1);b.line(1,1,1,5);b.line(6,1,6,5);b.line(1,5,3,7);b.line(6,5,3,7);b.line(3,2,3,6);s.append(b)
b=Bitmap(8,8);b.rect(3,0,2,2,1,True);b.rect(1,2,6,6);b.line(2,5,5,5);b.line(3,4,3,6);s.append(b)
b=Bitmap(8,8);b.line(3,0,7,3);b.line(7,3,3,7);b.line(3,7,0,3);b.line(0,3,3,0);b.rect(2,2,3,3,1,True);s.append(b)
b=Bitmap(8,8);b.rect(0,0,8,8);b.line(1,1,6,6);b.line(1,6,6,1);s.append(b)
for n in range(3):
 b=Bitmap(8,8);b.rect(1,1,6,5,1,True);b.dot(2,2,0);b.dot(5,2,0);b.line(2,4,5,4,0)
 for x in range(n+1):b.dot(1+x*2,7)
 s.append(b)
b=Bitmap(8,8);b.rect(1,1,6,6);b.rect(3,3,2,2,1,True);s.append(b)
b=Bitmap(8,8);b.rect(2,0,4,3,1,True);b.rect(1,3,6,3,1,True);b.dot(2,7);b.dot(5,7);b.p=[[1-v for v in row] for row in b.p];s.append(b)
b=Bitmap(8,8);b.rect(0,0,8,8);b.line(2,2,5,2);b.line(2,4,5,4);b.line(2,6,5,6);s.append(b)
assets(root,'MICRO ROGUE','micro-rogue',14,7,1,1,s,3,asm_bytes('rogue_rooms',sum(rooms,[])),aux=('WAIT','RESET'))
(root/'rooms.json').write_text(json.dumps(rooms)+'\n');print('Verified 12 connected rooms with gear, medicine, relic and three enemy spawns')
