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
# Twelve distinct motifs: combs, staggered shelves, slalom gates and stars
# through the central gap. A guaranteed corridor connects stage boundaries.
for stage in range(12):
 for attempt in range(100):
  r=random.Random(34000+stage*100+attempt);c=[0]*80
  for n,x in enumerate(range(10,58,6)):
   if 28<=x<=38:continue
   family=stage%4;side=(n+stage)%2
   if family==0:
    row=5 if side==0 else 1
    for col in range(x,x+2):c[col]|=1<<(row-1)
    if n%3==1:c[x+1]|=1<<(3 if row==5 else 1)
   elif family==1:
    # Low and high shelves require a deliberate gravity reversal.
    mask=24 if side==0 else 3
    c[x]=c[x+1]=mask
   elif family==2:
    # A central barrier alternates with split gates on both outer rails.
    c[x]=4 if side==0 else 17
    c[x+1]=4 if side==0 else 17
   else:
    # Three-step teeth narrow from alternating sides.
    for j in range(3):c[x+j]|=((1<<(j+1))-1)<<(4-j) if side==0 else (1<<(j+1))-1
   if stage>=4 and n%3==2:c[x+3]|=1<<(0 if side else 4)
  for x in range(12,58,5):
   free=[y for y in range(1,6) if not c[x]&(1<<(y-1))]
   c[x]|=r.choice(free)<<5
  answer=solve(c)
  if answer:break
 else:raise RuntimeError('No generated route')
 levels.append(c);solutions.append({'stars':answer[0],'flips':answer[1]})
# The viewport at distance 64 is exactly the next stage's first 16 columns.
for stage,c in enumerate(levels):c[64:80]=levels[(stage+1)%12][:16]
solutions=[{'stars':a[0],'flips':a[1]} for a in map(solve,levels)]
s=[Bitmap(8,8)]
b=Bitmap(8,8);b.line(0,6,7,6);b.line(0,7,7,7);s.append(b)
b=Bitmap(8,8);b.line(0,7,3,0);b.line(3,0,7,7);b.line(0,7,7,7);b.line(3,2,3,6);s.append(b)
b=Bitmap(8,8);b.line(0,3,7,3);b.line(3,0,3,7);b.line(1,1,6,6);b.line(1,6,6,1);s.append(b)
for up in (False,True):
 b=Bitmap(8,8);b.rect(1,1,6,6,1,True)
 for x,y in ((3,2 if up else 5),(2,3 if up else 4),(4,3 if up else 4)):b.dot(x,y,0)
 b.p=[[1-v for v in row] for row in b.p];s.append(b)
# Four staggered mechanical rail panels for each surface, without box grids.
for floor in (False,True):
 for phase in range(4):
  b=Bitmap(8,8)
  for y in range(8):
   yy=y if floor else 7-y
   for x in range(8):
    on=yy in (0,1,6,7) or (yy in (3,4) and (x+phase*8-yy)%13<4)
    if on:b.dot(x,y)
  s.append(b)
assets(root,'GRAVITY RUN','gravity-run',16,7,1,1,s,12,asm_bytes('gravity_levels',sum(levels,[])),aux=('PAUSE','RESET'))
(root/'levels.json').write_text(json.dumps(levels)+'\n');(root/'solutions.json').write_text(json.dumps(solutions)+'\n')
