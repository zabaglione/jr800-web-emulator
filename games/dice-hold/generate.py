# SPDX-License-Identifier: MIT
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;sprites=[Bitmap(24,16)]
pips=[[],[(11,7)],[(6,3),(16,11)],[(6,3),(11,7),(16,11)],[(6,3),(16,3),(6,11),(16,11)],[(6,3),(16,3),(11,7),(6,11),(16,11)],[(6,3),(16,3),(6,7),(16,7),(6,11),(16,11)]]
for held in range(2):
 for value in range(1,7):
  b=Bitmap(24,16);b.rect(1,2,20,14);b.line(1,2,3,0);b.line(3,0,23,0);b.line(20,2,23,0);b.line(23,0,23,13);b.line(20,15,23,13);b.line(22,3,22,13)
  # Two-pixel pips leave a clear row below the upper rim, even on sixes.
  for x,y in pips[value]:b.rect(x-1,max(4,y),2,2,1,True)
  if held:b.rect(2,3,18,12)
  sprites.append(b)
cells=[255]*112;sub=[0]*112
for i in range(5):
 for y in range(2):
  for x in range(3):p=(y+1)*16+i*3+x;cells[p]=i;sub[p]=y*3+x
labels=['ONES','TWOS','THREES','FOURS','FIVES','SIXES','3-KIND','4-KIND','HOUSE','SMALL','LARGE','5-KIND','CHANCE']
data=asm_bytes('dice_goals',[0,150,0,200,0,250])+asm_bytes('dice_labels',sum([list(n.ljust(6).encode())+[0] for n in labels],[]))
assets(root,'DICE HOLD','dice-hold',5,1,3,2,sprites,3,data,aux=('ROLL','SCORE'),layout=(cells,sub))
