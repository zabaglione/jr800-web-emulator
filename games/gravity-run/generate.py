# SPDX-License-Identifier: MIT
import sys,json,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;levels=[];solutions=[]
def solve(c):
 states={(5,1):(0,[])}
 for distance in range(1,65):
  new={}
  for (y,d),(score,path) in states.items():
   for flip in (0,1):
    nd=-d if flip else d;ny=max(1,min(5,y+nd));v=c[distance+3]
    if v&(1<<(ny-1)):continue
    item=(score+int(v>>5==ny),path+[flip]);key=(ny,nd)
    if key not in new or item[0]>new[key][0]:new[key]=item
  states=new
  if not states:return None
 return max(states.values(),key=lambda item:item[0])
for stage in range(12):
 for attempt in range(100):
  r=random.Random(34000+stage*100+attempt);c=[0]*80
  for x in range(8,64,6):
   if 30<=x<=38:continue
   row=5 if (x==8 and stage==0) else r.choice((1,5))
   for col in range(x,min(x+r.choice((2,3)),64)):c[col]|=1<<(row-1)
   if stage>=4 and x%12==8:c[x+1]|=1<<(3 if row==5 else 1)
   if stage>=8 and x%18==14:c[x+2]|=4
  for x in range(12,64,7):
   if not c[x]&4:c[x]|=3<<5
  answer=solve(c)
  if answer:break
 else:raise RuntimeError('No generated route')
 levels.append(c);solutions.append({'stars':answer[0],'flips':answer[1]})
s=[Bitmap(8,8)]
b=Bitmap(8,8);b.line(0,0,7,0);b.line(0,7,7,7);b.line(3,0,3,7);s.append(b)
b=Bitmap(8,8);b.line(0,7,3,0);b.line(3,0,7,7);b.line(0,7,7,7);b.line(3,2,3,6);s.append(b)
b=Bitmap(8,8);b.line(0,3,7,3);b.line(3,0,3,7);b.line(1,1,6,6);b.line(1,6,6,1);s.append(b)
for up in (False,True):
 b=Bitmap(8,8);b.rect(1,1,6,6,1,True)
 for x,y in ((3,2 if up else 5),(2,3 if up else 4),(4,3 if up else 4)):b.dot(x,y,0)
 b.p=[[1-v for v in row] for row in b.p];s.append(b)
assets(root,'GRAVITY RUN','gravity-run',16,7,1,1,s,12,asm_bytes('gravity_levels',sum(levels,[])),aux=('PAUSE','RESET'))
(root/'levels.json').write_text(json.dumps(levels)+'\n');(root/'solutions.json').write_text(json.dumps(solutions)+'\n')
