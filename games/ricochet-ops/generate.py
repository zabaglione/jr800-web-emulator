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
levels=[];solutions=[]
for stage in range(20):
 r=random.Random(38000+stage)
 for attempt in range(12000):
  board=[1 if y==0 or (x in (0,7) and y<5) else 0 for y in range(6) for x in range(8)]
  cells=[p for p in range(48) if 0<p//8<5 and 0<p%8<7];r.shuffle(cells)
  for p in cells[:7]:board[p]=r.choice((2,3))
  for p in cells[7:9]:board[p]=1
  shots=[]
  for c in range(1,7):
   for a in range(8):
    path,refs=trace(board,c,a);coverage=set(p for p in path if p//8<5 and board[p]==0)
    if coverage:shots.append((c,a,coverage,refs))
  reach=set().union(*(s[2] for s in shots))
  if len(reach)<3 or not any(s[3]>=2 for s in shots):continue
  targets=r.sample(sorted(reach),3);masks=[]
  for c,a,cover,refs in sorted(shots,key=lambda s:-s[3]):
   mask=sum(1<<i for i,p in enumerate(targets) if p in cover)
   if mask:masks.append((mask,c,a,refs))
  q=collections.deque([(0,[])]);seen={0};answer=None
  while q:
   mask,path=q.popleft()
   if mask==7:answer=path;break
   if len(path)==3:continue
   for hit,c,a,refs in masks:
    m=mask|hit
    if m not in seen:seen.add(m);q.append((m,path+[(c,a,refs)]))
  minimum=1 if stage<5 else 2 if stage<15 else 3
  if answer is None or len(answer)!=minimum or not any(s[2]>=2 for s in answer):continue
  for p in targets:board[p]=4
  levels.append(board);solutions.append([{'column':c,'angle':a} for c,a,_ in answer]);break
 else:raise RuntimeError(f'No certified layout for stage {stage+1}')
s=[Bitmap(16,8)]
b=Bitmap(16,8);b.rect(0,0,16,8);b.line(0,0,15,7);s.append(b)
b=Bitmap(16,8);b.line(1,7,14,0);b.line(2,7,15,0);s.append(b)
b=Bitmap(16,8);b.line(1,0,14,7);b.line(2,0,15,7);s.append(b)
b=Bitmap(16,8);b.rect(4,1,8,6);b.rect(6,3,4,2,1,True);s.append(b)
for dx,dy in dirs:
 b=Bitmap(16,8);b.rect(5,2,5,4,1,True);b.line(7,4,7+dx*6,4+dy*3);b.p=[[1-v for v in row] for row in b.p];s.append(b)
b=Bitmap(16,8)
for x in (2,6,10,14):b.dot(x,4)
s.append(b)
b=Bitmap(16,8);b.rect(6,2,4,4,1,True);s.append(b)
assets(root,'RICOCHET OPS','ricochet-ops',8,6,2,1,s,20,asm_bytes('rico_levels',sum(levels,[])),aux=('UNDO SHOT','RESET'))
(root/'levels.json').write_text(json.dumps(levels)+'\n');(root/'solutions.json').write_text(json.dumps(solutions)+'\n')
print('Certified 20 layouts: five one-shot, ten two-shot, five three-shot solutions')
