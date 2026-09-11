# SPDX-License-Identifier: MIT
import sys,json,random,heapq
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;D=[-14,14,-1,1];K=['up','down','left','right'];levels=[];solutions=[]
def apply(s,key):
 b=s['board'];p=s['p'];turn=False
 if key=='heal':
  if s['pot'] and s['hp']<9:s['pot']-=1;s['hp']=min(9,s['hp']+3);turn=True
 elif key=='space':
  q=p+D[s['aim']];v=b[q]
  if v>=7:
   b[q]=v-2 if v==9 else 0
   if not b[q]:s['score']+=25
   turn=True
 else:
  d=K.index(key);s['aim']=d;q=p+D[d];v=b[q]
  if v==1 or v>=7 or (v==3 and not s['keys']):return
  s['p']=q;turn=True
  if v==2:s['keys']+=1;b[q]=0
  if v==3:s['keys']-=1;b[q]=0
  if v==4:s['pot']+=1;b[q]=0
  if v==5:s['left']-=1;s['score']+=100;b[q]=0
  if v==6 and not s['left']:s['phase']=4;s['score']+=s['hp']*10;return
 if turn:
  s['turns']=min(255,s['turns']+1)
  s['hp']=max(0,s['hp']-sum(b[s['p']+d]>=7 for d in D))
  if not s['hp']:s['phase']=5

def route(s,goal):
 q=[(0,s['p'],[])];seen=set()
 while q:
  cost,p,path=heapq.heappop(q)
  if p in seen:continue
  seen.add(p)
  if p==goal:return path
  for d,key in zip(D,K):
   n=p+d;v=s['board'][n]
   if v==1 or v>=7 or (v==3 and not s['keys']):continue
   risk=sum(s['board'][n+dd]>=7 for dd in D)
   heapq.heappush(q,(cost+1+30*risk,n,path+[key]))
 return None
for stage in range(12):
 r=random.Random(39000+stage)
 for attempt in range(300):
  y1=r.randrange(1,6);y2=r.randrange(1,6)
  board=[1 if y in (0,6) or x in (0,13,5,10) else 0 for y in range(7) for x in range(14)]
  board[y1*14+5]=3;board[y2*14+10]=3
  room1=[y*14+x for y in range(1,6) for x in range(1,5) if y*14+x!=15]
  room2=[y*14+x for y in range(1,6) for x in range(6,10) if y*14+x!=y1*14+6]
  room3=[y*14+x for y in range(1,6) for x in (11,12) if y*14+x!=y2*14+11]
  r.shuffle(room1);r.shuffle(room2);r.shuffle(room3)
  key1,pot1=room1[:2];key2,pot2,relay1=room2[:3];relay2,exit=room3[:2]
  for p,v in [(key1,2),(pot1,4),(key2,2),(pot2,4),(relay1,5),(relay2,5),(exit,6),(y1*14+6,9),(y2*14+11,9)]:board[p]=v
  for room in (room1[2:],room2[3:]):
   for p in room[:2]:board[p]=1
  s={'board':board[:],'p':15,'aim':3,'hp':9,'keys':0,'pot':0,'left':2,'score':0,'turns':0,'phase':2};path=[]
  def do(key):path.append(key);apply(s,key)
  def walk(goal):
   way=route(s,goal)
   if way is None:raise ValueError()
   for key in way:
    if s['hp']<=5 and s['pot']:do('heal')
    do(key)
    if s['phase']==5:raise ValueError()
  try:
   walk(key1);walk(pot1);walk(y1*14+5);do('right');do('space');do('space')
   walk(key2);walk(pot2);walk(relay1);walk(y2*14+10);do('right');do('space');do('space')
   walk(relay2);walk(exit)
   if s['phase']!=4:continue
  except (ValueError,IndexError):continue
  levels.append(board);solutions.append(path);break
 else:raise RuntimeError('No solvable ruin')
s=[Bitmap(8,8)]
b=Bitmap(8,8);b.rect(0,0,8,8);b.line(0,3,7,3);b.line(3,0,3,3);b.line(5,4,5,7);s.append(b)
b=Bitmap(8,8);b.rect(0,1,4,4);b.line(3,4,7,4);b.line(6,4,6,6);s.append(b)
b=Bitmap(8,8);b.rect(0,0,8,8);b.rect(2,2,4,4,1,True);b.dot(4,3,0);s.append(b)
b=Bitmap(8,8);b.rect(2,0,4,2,1,True);b.rect(1,2,6,6);b.line(2,5,5,5);b.line(3,4,3,6);s.append(b)
b=Bitmap(8,8);b.rect(1,1,6,6);b.line(2,5,4,2);b.line(4,2,5,5);s.append(b)
b=Bitmap(8,8);b.rect(0,0,8,8);b.line(2,2,5,2);b.line(2,4,5,4);b.line(2,6,5,6);s.append(b)
for hp in range(1,4):
 b=Bitmap(8,8);b.rect(1,2,6,4,1,True);b.dot(2,3,0);b.dot(5,3,0);b.dot(1,1);b.dot(6,1)
 for i in range(hp):b.dot(1+i*2,7)
 s.append(b)
for dx,dy in ((0,-1),(0,1),(-1,0),(1,0)):
 b=Bitmap(8,8);b.rect(2,2,4,4,1,True);b.line(3,3,3+dx*3,3+dy*3);b.p=[[1-v for v in row] for row in b.p];s.append(b)
from depth_art import block8
s[1]=block8('stone')
assets(root,'RELAY QUEST','relay-quest',14,7,1,1,s,12,asm_bytes('quest_levels',sum(levels,[])),aux=('USE TONIC','RESET'))
(root/'levels.json').write_text(json.dumps(levels)+'\n');(root/'solutions.json').write_text(json.dumps(solutions)+'\n')
print('Certified 12 resource and combat routes:',','.join(str(len(p)) for p in solutions))
