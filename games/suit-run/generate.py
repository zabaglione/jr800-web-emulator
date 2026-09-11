# SPDX-License-Identifier: MIT
import sys,json,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;deals=[];solutions=[]
for stage in range(20):
 rng=random.Random(24000+stage)
 for attempt in range(1000):
  counts=[0]+[4]*13;waste=rng.randint(1,13);counts[waste]-=1;initial=waste;board=[0]*35;removed=set();stock=[];steps=[]
  while len(removed)<35:
   choices=[(waste-2)%13+1,waste%13+1];choices=[n for n in choices if counts[n]]
   if not choices or (len(stock)<12 and rng.random()<.15):
    choices=[n for n in range(1,14) if counts[n]]
    if len(stock)>=16 or not choices:break
    waste=rng.choice(choices);counts[waste]-=1;stock.append(waste);steps.append(['draw']);continue
   exposed=[p for p in range(35) if p not in removed and (p>=28 or p+7 in removed)]
   p=rng.choice(exposed);waste=rng.choice(choices);counts[waste]-=1;board[p]=waste;removed.add(p);steps.append(['take',p])
  if len(removed)==35 and len(stock)>=3:break
 else:raise RuntimeError('Could not construct a solvable deck')
 unused=[n for n in range(1,14) for _ in range(counts[n])];rng.shuffle(unused);stock+=unused
 deal=board+[initial]+stock;assert len(deal)==52 and all(deal.count(n)==4 for n in range(1,14))
 deals.append(deal);solutions.append(steps)
sprites=[Bitmap(16,8)]
for available in range(2):
 for rank in range(1,14):
  b=Bitmap(16,8);b.text(' A23456789TJQK'[rank],5,0);b.line(1,7,14,7)
  if available:b.line(1,0,1,7);b.line(14,0,14,7)
  else:b.dot(1,0);b.dot(14,0)
  sprites.append(b)
assets(root,'SUIT RUN','suit-run',7,5,2,1,sprites,20,asm_bytes('suit_deals',sum(deals,[])),aux=('DRAW','UNDO'))
(root/'solutions.json').write_text(json.dumps({'deals':deals,'steps':solutions})+'\n')
