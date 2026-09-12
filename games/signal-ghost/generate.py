# SPDX-License-Identifier: MIT
import sys,json,random,collections
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;dirs=[(0,-1),(1,-1),(1,0),(1,1),(0,1),(-1,1),(-1,0),(-1,-1)];steps=[-14,14,-1,1];keys=['up','down','left','right'];levels=[];solutions=[]
def vision(b,cameras,bases,turn,disabled):
 lit=set()
 for actor,p in enumerate(cameras):
  if disabled&(2 if actor==1 else 1):continue
  for ray in (-1,0,1):
   dx,dy=dirs[((bases[actor]+turn)*2+ray)%8];x=p%14;y=p//14
   for distance in range(4):
    x+=dx;y+=dy
    if not(0<=x<14 and 0<=y<7):break
    q=y*14+x
    if b[q]==1 or q in cameras:break
    lit.add(q)
 return lit
for stage in range(20):
 r=random.Random(42000+stage)
 for attempt in range(5000):
  b=[1 if y in (0,6) or x in (0,13) else 0 for y in range(7) for x in range(14)]
  if stage==0:
   # A protected entry room teaches walking onto a terminal before hacking.
   for y in range(1,5):b[y*14+5]=1
   cameras=[35,39,63];terminals=[31,76];bases=[2,2,0]
   b[terminals[0]]=2;b[terminals[1]]=3;b[82]=4
  else:
   for x in (5,9):
    openings=r.sample(range(1,6),2)
    for y in range(1,6):
     if y not in openings:b[y*14+x]=1
   floors=[p for p in range(98) if b[p]==0 and p not in (15,82)];r.shuffle(floors);cameras=floors[:3];terminals=floors[3:5];bases=[r.randrange(4) for _ in range(3)];b[terminals[0]]=2;b[terminals[1]]=3;b[82]=4
  views={(t,m):vision(b,cameras,bases,t,m) for t in range(4) for m in range(4)}
  if 15 in views[(0,0)]:continue
  # Keep the common first move/action safe for all start screens.
  p=16 if b[16]!=1 and 16 not in cameras else 15;t=1 if p==16 else 0
  if p in views[(t,0)] or p in views[((t+1)%4,0)]:continue
  start=(15,0,0);q=collections.deque([(start,[])]);seen={start};answer=None;waited=False
  while q:
   (p,t,m),path=q.popleft()
   if p==82 and m==3:answer=path;break
   for action in range(5):
    n=p if action==4 else p+steps[action]
    if n<0 or n>=98 or b[n]==1 or n in cameras:continue
    mask=m
    if action==4 and b[p] in (2,3):mask|=1<<(b[p]-2)
    turn=(t+1)%4
    if n in views[(turn,mask)]:continue
    state=(n,turn,mask)
    if state not in seen:seen.add(state);q.append((state,path+[keys[action] if action<4 else 'space']))
  if answer and (stage==0 or len(answer)>=20 and answer.count('space')>=3):
   if stage==0:
    assert len(answer)==17 and answer.count('space')==2
    assert all(not ({15,16,17,31}&views[(t,0)]) for t in range(4))
   levels.append({'board':b,'cameras':cameras,'bases':bases});solutions.append(answer);break
 else:raise RuntimeError('No stealth route')
s=[Bitmap(8,8)]
b=Bitmap(8,8);b.rect(0,0,8,8);b.line(0,4,7,4);b.line(3,0,3,4);b.line(5,4,5,7);s.append(b)
for rows in [('010','101','111','101','101'),('110','101','110','101','110')]:
 b=Bitmap(8,8);b.rect(0,0,8,8)
 for y,row in enumerate(rows):
  for x,v in enumerate(row):
   if v=='1':b.dot(x+2,y+1)
 s.append(b)
b=Bitmap(8,8);b.rect(0,0,8,8);b.line(1,1,6,6);b.line(1,6,6,1);s.append(b)
b=Bitmap(8,8);b.rect(0,0,8,8);b.line(2,2,5,2);b.line(2,4,5,4);b.line(2,6,5,6);s.append(b)
b=Bitmap(8,8)
for x,y in ((2,2),(6,6)):b.dot(x,y)
s.append(b)
for dx,dy in ((0,-1),(1,0),(0,1),(-1,0)):
 b=Bitmap(8,8);b.rect(1,1,6,6);b.rect(2,2,4,4,1,True);b.line(3,3,3+dx*3,3+dy*3,0);s.append(b)
b=Bitmap(8,8);b.rect(2,1,4,3,1,True);b.rect(1,4,6,2,1,True);b.dot(2,7);b.dot(5,7);b.p=[[1-v for v in row] for row in b.p];s.append(b)
b=Bitmap(8,8);b.rect(1,1,6,6);b.line(2,2,5,5);s.append(b)
raw=[]
for l in levels:raw += [(l['board'][i]<<4)|l['board'][i+1] for i in range(0,98,2)]+l['cameras']+l['bases']
assets(root,'SIGNAL GHOST','signal-ghost',14,7,1,1,s,20,'.section .extra, data\n'+asm_bytes('signal_levels',raw),aux=('WAIT','RESET'))
(root/'levels.json').write_text(json.dumps(levels)+'\n');(root/'solutions.json').write_text(json.dumps(solutions)+'\n');print('Certified 20 timed stealth routes:',','.join(str(len(p)) for p in solutions))
