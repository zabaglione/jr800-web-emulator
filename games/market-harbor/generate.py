# SPDX-License-Identifier: MIT
import sys,json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import logo
from art import asm_bytes
root=Path(__file__).parent;bases=[[4,14,18],[10,5,15],[8,12,6],[12,16,22]];season=[[0,-2,1,2,-1,0,1,-1],[0,2,-1,3,-2,1,-3,0],[0,-3,2,4,-2,1,-4,3]]
def price(port,day,offset,good):return max(1,bases[port][good]+season[good][(day+offset)%8])
def trip(a,b):return min((a-b)%4,(b-a)%4)
levels=[];solutions=[]
for stage in range(12):
 cap=6+2*(stage//4);cash=24+4*(stage%3);goal=80+20*stage;limit=8+(stage//4)*2;offset=stage*3%8
 states={(0,0):(cash,[])};winner=None
 for day in range(limit):
  for port in range(4):
   if (day,port) not in states:continue
   money,path=states[(day,port)]
   if money>=goal:winner=path;break
   for dest in range(4):
    days=trip(port,dest);fee=2*days
    if not days or day+days>=limit or money<fee:continue
    for good in range(3):
     buy=price(port,day,offset,good);sell=price(dest,day+days,offset,good);count=min(cap,(money-fee)//buy) if sell>buy else 0;new=money-fee+count*(sell-buy)
     key=(day+days,dest);newPath=path+[{'good':good,'count':count,'dest':dest}]
     if key not in states or states[key][0]<new:states[key]=(new,newPath)
  if winner is not None:break
 if winner is None:raise RuntimeError(f'No trade route for contract {stage+1}')
 levels.append({'capacity':cap,'cash':cash,'goal':goal,'limit':limit,'offset':offset});solutions.append(winner)
source='; SPDX-License-Identifier: MIT\n.equ STAGES,12\n.section .data, data\n'
for label,text in [('game_name','MARKET HARBOR'),('aux1_label','SAIL'),('aux2_label','SELL ALL')]:source+=asm_bytes(label,list(text.encode())+[0])
source+=asm_bytes('title_art',logo('MARKET HARBOR','market-harbor'))+asm_bytes('market_bases',sum(bases,[]))+asm_bytes('market_seasons',[n&255 for row in season for n in row])
raw=[]
for l in levels:raw += [l['capacity'],l['cash'],l['goal']>>8,l['goal']&255,l['limit'],l['offset']]
source+=asm_bytes('market_levels',raw)
# The shared shell links a tile table even though this game renders ledgers as text.
source+=asm_bytes('tiles',[0]*8)
(root/'assets.s').write_text(source);(root/'levels.json').write_text(json.dumps(levels)+'\n');(root/'solutions.json').write_text(json.dumps(solutions)+'\n');(root/'prices.json').write_text(json.dumps({'bases':bases,'season':season})+'\n');print('Certified 12 trading contracts:',','.join(str(len(p)) for p in solutions))
