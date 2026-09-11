# SPDX-License-Identifier: MIT
import sys,json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;levels=[]
for stage in range(12):
 lanes=[]
 for lane in range(4):
  width=[4,5,2,3][lane]-(1 if stage>=8 and lane==1 else 0)
  shift=(stage*(lane+1)+lane*2)%7
  lanes.append([int((x+shift)%7<width) for x in range(14)])
 periods=[3+stage%2,4-stage%2,3+(stage//2)%2,4-(stage//2)%2]
 levels.append({'lanes':lanes,'periods':periods})
assert len({str(x) for x in levels})==12
sprites=[Bitmap(8,8)]
# 1 grass,2 water,3 road,4 home,5 reached home,6 log,7 car,8 player.
b=Bitmap(8,8);b.dot(1,6);b.dot(5,2);sprites.append(b)
b=Bitmap(8,8);b.line(0,2,3,2);b.line(4,6,7,6);sprites.append(b)
b=Bitmap(8,8);b.line(1,7,3,7);sprites.append(b)
b=Bitmap(8,8);b.rect(0,0,8,8);b.line(2,5,4,2);b.line(4,2,6,5);sprites.append(b)
b=Bitmap(8,8);b.rect(0,0,8,8);b.rect(2,2,4,4,1,True);sprites.append(b)
b=Bitmap(8,8);b.rect(0,1,8,6);b.line(1,3,6,3);sprites.append(b)
b=Bitmap(8,8);b.rect(0,1,8,5,1,True);b.dot(1,6);b.dot(6,6);b.dot(2,2,0);b.dot(5,2,0);sprites.append(b)
b=Bitmap(8,8);b.rect(2,1,4,6,1,True);b.dot(0,0);b.dot(7,0);b.line(0,3,2,3);b.line(5,3,7,3);b.dot(0,7);b.dot(7,7);b.p=[[1-v for v in row] for row in b.p];sprites.append(b)
data=asm_bytes('river_levels',sum([sum(g['lanes'],[])+g['periods'] for g in levels],[]))+asm_bytes('river_rows',[1,2,4,5])+asm_bytes('river_directions',[1,255,255,1])+asm_bytes('river_home_columns',[1,4,7,10,12])
assets(root,'RIVER HOP','river-hop',14,7,1,1,sprites,12,data,aux=('PAUSE','RESET'))
(root/'levels.json').write_text(json.dumps(levels)+'\n')
