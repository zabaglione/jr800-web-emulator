# SPDX-License-Identifier: MIT
import sys,json,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;duration=[0,3,5];cost=[0,2,3];price=[0,7,12];slots=[0,7,14,3,10,17];levels=[];solutions=[]
def key(coins,plots):return (coins,tuple(sorted(plots)))
def rank(coins,plots,remaining,rain):
 v=coins
 for c,g,w,d in plots:
  if c and duration[c]-g<remaining:
   value=price[c]*(g+1)/(duration[c]+1)
   if d and not w and not rain:value*=.25
   v+=value
 return v
for stage in range(12):
 days=[8,8,9,9,10,10,11,11,12,12,14,14][stage];goal=[18,22,25,28,32,36,40,44,48,52,56,60][stage];r=random.Random(44000+stage);rain=[int((day+stage)%4==2 or (stage>=6 and day%5==1)) for day in range(days)];start=(10,((0,0,0,0),)*6,[]);beam=[start];winner=None
 for day in range(days):
  candidates=beam[:];front=beam
  for action in range(3):
   unique={}
   for coins,plots,path in front:
    seenPlots=set()
    for cell,(c,g,w,d) in enumerate(plots):
     if (c,g,w,d) in seenPlots:continue
     seenPlots.add((c,g,w,d))
     choices=[(0,c,g,w,d,coins)]
     if c==0:choices=[(seed,seed,0,1,0,coins-cost[seed]) for seed in (1,2) if coins>=cost[seed]]
     elif g==duration[c]:choices=[(0,0,0,0,0,coins+price[c])]
     elif not w:choices=[(0,c,g,1,0,coins)]
     else:choices=[]
     for seed,C,G,W,DR,newCoins in choices:
      newPlots=list(plots);newPlots[cell]=(C,G,W,DR);newPlots=tuple(newPlots);newPath=path+[{'cell':slots[cell],'seed':seed}];node=(newCoins,newPlots,newPath);unique.setdefault(key(newCoins,newPlots),node)
      if newCoins>=goal:winner=newPath;break
     if winner:break
    if winner:break
   if winner:break
   front=sorted(unique.values(),key=lambda n:rank(n[0],n[1],days-day,rain[day]),reverse=True)[:240];candidates+=front
  if winner:break
  if day==days-1:break
  unique={}
  for coins,plots,path in candidates:
   out=[]
   for c,g,w,d in plots:
    if c and g<duration[c]:
     if w or rain[day]:g+=1;d=0
     else:d+=1
     if d==2:c=g=d=0
    out.append((c,g,0,d))
   node=(coins,tuple(out),path+['next']);unique.setdefault(key(coins,out),node)
  beam=sorted(unique.values(),key=lambda n:rank(n[0],n[1],days-day-1,rain[day+1]),reverse=True)[:320]
 if not winner:raise RuntimeError(f'Uncertified goal {stage+1}: best cash {max(n[0] for n in beam)}')
 levels.append({'days':days,'goal':goal,'rain':rain});solutions.append(winner)
s=[Bitmap(16,16)]
for x in range(2,15,4):s[0].line(x,10,x+2,13)
s[0].rect(0,0,16,16)
for water in range(3):
 for crop in (1,2):
  for age in range(3):
   b=Bitmap(16,16);b.rect(0,0,16,16)
   for x in range(2,15,4):b.line(x,11,x+2,13)
   if age==0:b.line(7,10,7,7);b.line(7,8,4,6);b.line(7,9,10,6)
   else:
    b.line(7,11,7,3);b.line(7,6,3,3);b.line(7,8,12,5);b.line(7,9,3,7)
    if age==2:
     for x,y in ((3,3),(11,5),(3,7)):
      if crop==1:b.rect(x,y,2,5,1,True)
      else:b.rect(x,y,3,3,1,True);b.dot(x+1,y+1,0)
   if water==1:
    for x in (2,6,10):b.line(x,14,x+2,14)
   elif water==2:b.line(11,1,14,4);b.line(11,4,14,1)
   s.append(b)
raw=[]
for l in levels:raw += [l['days'],l['goal']]+l['rain']+[0]*(14-l['days'])
assets(root,'ORCHARD DAYS','orchard-days',6,3,2,2,s,12,asm_bytes('orchard_levels',raw),aux=('NEXT DAY','SWAP SEED'))
(root/'levels.json').write_text(json.dumps(levels)+'\n');(root/'solutions.json').write_text(json.dumps(solutions)+'\n');print('Certified 12 farming plans:',','.join(str(len(p)) for p in solutions))
