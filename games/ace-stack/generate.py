# SPDX-License-Identifier: MIT
import sys,json,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;children=[];positions=[];cells=[255]*112;sub=[0]*112
for row in range(7):
 for col in range(row+1):
  p=row*(row+1)//2+col;children.append([(row+1)*(row+2)//2+col,(row+1)*(row+2)//2+col+1] if row<6 else [255,255]);x=7-row+col*2;positions.append((x, row))
  for s in range(2):cells[row*16+x+s]=p;sub[row*16+x+s]=s
positions.append((18,3));navigation=[]
for dx,dy in ((0,-1),(0,1),(-1,0),(1,0)):
 for i,(x,y) in enumerate(positions):
  candidates=[(2*((X-x)*dx+(Y-y)*dy)+3*abs((X-x)*dy-(Y-y)*dx),j) for j,(X,Y) in enumerate(positions) if (X-x)*dx+(Y-y)*dy>0]
  navigation.append(min(candidates)[1] if candidates else 255)
deals=[];solutions=[]
for stage in range(20):
 rng=random.Random(23000+stage)
 for attempt in range(1000):
  remaining=[0]+[4]*13;removed=set();board=[0]*28;stock=[];steps=[]
  while len(removed)<28:
   exposed=[p for p in range(28) if p not in removed and all(c==255 or c in removed for c in children[p])]
   pairs=[n for n in range(1,13) if remaining[n] and remaining[13-n]]
   if remaining[13] and (rng.random()<.15 or not pairs or (len(exposed)==1 and len(stock)>=18)):
    p=rng.choice(exposed);board[p]=13;remaining[13]-=1;removed.add(p);steps.append(['king',p]);continue
   if not pairs or (len(exposed)==1 and len(stock)>=18):break
   a=rng.choice(pairs);b=13-a;remaining[a]-=1;remaining[b]-=1
   p=rng.choice(exposed);board[p]=a;removed.add(p)
   if len(exposed)>=2 and (rng.random()<.60 or len(stock)>=18):
    q=rng.choice([q for q in exposed if q!=p]);board[q]=b;removed.add(q);steps.append(['pair',p,q])
   else:stock.append(b);steps.extend([['draw'],['pair',p,28]])
  if len(removed)==28 and len(stock)>=4:break
 else:raise RuntimeError('Could not construct a solvable standard deck')
 unused=[n for n in range(1,14) for _ in range(remaining[n])];rng.shuffle(unused);stock+=unused
 assert len(stock)==24 and all((board+stock).count(n)==4 for n in range(1,14))
 deals.append(board+stock);solutions.append(steps)
sprites=[Bitmap(16,8)]
for kind in range(3):
 for rank in range(1,14):
  b=Bitmap(16,8);b.text(' A23456789TJQK'[rank],5,0);b.line(1,7,14,7)
  if kind:
   b.line(1,0,1,7);b.line(14,0,14,7)
  else:b.dot(1,0);b.dot(14,0)
  if kind==2:b.line(3,0,3,6);b.line(12,0,12,6)
  sprites.append(b)
data=asm_bytes('ace_children',sum(children,[]))+asm_bytes('ace_deals',sum(deals,[]))
assets(root,'ACE STACK','ace-stack',7,7,2,1,sprites,20,data,aux=('DRAW','UNDO'),layout=(cells,sub),navigation=navigation,cell_count=29)
(root/'solutions.json').write_text(json.dumps({'deals':deals,'steps':solutions})+'\n')
