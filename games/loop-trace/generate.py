# SPDX-License-Identifier: MIT
import sys,json,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;levels=[];paths=[];seen=set()
for stage in range(20):
 rng=random.Random(22000+stage);target=12+(stage//2)*2
 for attempt in range(10000):
  x,y=rng.randrange(5),rng.randrange(5);path=[(x,y),(x+1,y),(x+1,y+1),(x,y+1)]
  while len(path)<target:
   candidates=[]
   for i,p in enumerate(path):
    q=path[(i+1)%len(path)];dx,dy=q[0]-p[0],q[1]-p[1]
    for sign in (-1,1):
     r=(p[0]-dy*sign,p[1]+dx*sign);s=(q[0]-dy*sign,q[1]+dx*sign)
     if all(0<=X<6 and 0<=Y<6 for X,Y in (r,s)) and r not in path and s not in path:candidates.append((i,r,s))
   if not candidates:break
   i,r,s=rng.choice(candidates);path[i+1:i+1]=[r,s]
  if len(path)!=target:continue
  start=rng.randrange(target);path=path[start:]+path[:start];route=[y*6+x for x,y in path]
  board=[0]*36
  for p in route:board[p]=1
  board[route[0]]=5
  for n in (1,2,3):board[route[target*n//4]]=n+1
  signature=tuple(board)
  if signature not in seen:break
 else:raise RuntimeError('Could not construct a distinct original loop')
 seen.add(signature);levels.append(board);paths.append(route)
sprites=[]
for n in range(22):
 b=Bitmap(16,8)
 if n==0:
  for y in range(0,8,2):
   for x in range(y%4,16,4):b.dot(x,y)
 elif n==1:b.rect(6,2,4,4)
 elif n<=5:b.text({2:'A',3:'B',4:'C',5:'S'}[n],5,0)
 else:
  mask=n-6;b.rect(6,2,4,4,1,True)
  for bit,x,y in ((1,7,0),(2,7,7),(4,0,3),(8,15,3)):
   if mask&bit:b.line(7,3,x,y)
 sprites.append(b)
assets(root,'LOOP TRACE','loop-trace',6,6,2,1,sprites,20,asm_bytes('loop_levels',sum(levels,[])),stat='LEFT',action='SPACE',aux=('UNDO','RESET'))
(root/'solutions.json').write_text(json.dumps({'boards':levels,'paths':paths})+'\n')
