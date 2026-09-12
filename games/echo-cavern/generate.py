# SPDX-License-Identifier: MIT
import sys,json,random,heapq,itertools
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;D=[-14,14,-1,1];K=['up','down','left','right'];levels=[];solutions=[]
def route(b,start,end):
 q=[(0,start,[])];seen=set()
 while q:
  cost,p,path=heapq.heappop(q)
  if p in seen:continue
  seen.add(p)
  if p==end:return path
  for d,k in zip(D,K):
   n=p+d
   if b[n]!=1:heapq.heappush(q,(cost+(3 if b[n]==4 else 1),n,path+[k]))
for stage in range(12):
 r=random.Random(40000+stage)
 for attempt in range(100):
  b=[1]*98;seen={0};stack=[0];b[15]=0
  while stack:
   c=stack[-1];x=c%6;y=c//6;adj=[(y+dy)*6+x+dx for dx,dy in ((0,-1),(0,1),(-1,0),(1,0)) if 0<=x+dx<6 and 0<=y+dy<3 and (y+dy)*6+x+dx not in seen]
   if not adj:stack.pop();continue
   n=r.choice(adj);seen.add(n);stack.append(n);p=(1+2*y)*14+1+2*x;q=(1+2*(n//6))*14+1+2*(n%6);b[q]=b[(p+q)//2]=0
  walls=[p for p in range(98) if 0<p//14<6 and 0<p%14<12 and b[p]==1 and ((b[p-1]==0 and b[p+1]==0) or (b[p-14]==0 and b[p+14]==0))];r.shuffle(walls)
  for p in walls[:3]:b[p]=0
  floors=[p for p in range(98) if b[p]==0 and p!=15];far=sorted(floors,key=lambda p:len(route(b,15,p)),reverse=True);exit=far[0];b[exit]=5
  candidates=[p for p in floors if p!=exit];r.shuffle(candidates);gems=candidates[:3];tanks=candidates[3:5];spikes=candidates[5:8]
  for p in gems:b[p]=2
  for p in tanks:b[p]=3
  for p in spikes:b[p]=4
  best=None
  for order in itertools.permutations(gems+tanks):
   board=b[:];p=15;air=64-(stage//4)*4;left=3;path=[];since=0;okay=True
   for goal in (*order,exit):
    for k in route(board,p,goal):
     if since==6:path.append('space');air-=2;since=0
     p+=D[K.index(k)];path.append(k);since+=1;air-=3 if board[p]==4 else 1
     if air<=0:okay=False;break
     if board[p]==3:air=min(99,air+24);board[p]=0
     if board[p]==2:left-=1;board[p]=0
    if not okay:break
   if okay and not left and (best is None or len(path)<len(best)):best=path
  if best:levels.append(b);solutions.append(best);break
 else:raise RuntimeError('No oxygen-safe expedition')
s=[Bitmap(8,8)]
b=Bitmap(8,8);b.rect(0,0,8,8);b.line(0,6,4,1);b.line(4,1,7,5);s.append(b)
b=Bitmap(8,8);b.line(3,0,7,3);b.line(7,3,3,7);b.line(3,7,0,3);b.line(0,3,3,0);b.line(3,1,3,6);s.append(b)
b=Bitmap(8,8);b.rect(2,1,4,7);b.rect(3,0,2,2,1,True);b.line(3,3,4,3);b.line(3,5,4,5);s.append(b)
b=Bitmap(8,8)
for x in (0,4):b.line(x,7,x+2,2);b.line(x+2,2,x+3,7)
s.append(b)
b=Bitmap.from_rows(['..####..','.######.','.##...#.','.##..##.','.##.#.#.','.##.#.#.','.##..##.','.######.']);s.append(b)
b=Bitmap(8,8)
for y in (1,5):
 for x in (1,5):b.dot(x,y);b.dot((x+2)%8,(y+2)%8)
s.append(b)
b=Bitmap(8,8);b.rect(2,1,4,3,1,True);b.line(3,3,3,6);b.line(1,5,5,5);b.dot(2,7);b.dot(5,7);b.p=[[1-v for v in row] for row in b.p];s.append(b)
s.append(Bitmap.from_rows(['..####..','.######.','.##..##.','.##..##.','.##.#.#.','.##..##.','.##..##.','.######.']))
assets(root,'ECHO CAVERN','echo-cavern',14,7,1,1,s,12,asm_bytes('echo_levels',sum(levels,[])),aux=('SONAR','RESET'))
(root/'levels.json').write_text(json.dumps(levels)+'\n');(root/'solutions.json').write_text(json.dumps(solutions)+'\n')
print('Certified 12 oxygen-safe routes:',','.join(str(len(p)) for p in solutions))
