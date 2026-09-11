# SPDX-License-Identifier: MIT
import sys,json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;b=Bitmap(128,56)
b.rect(14,2,102,20)
for x in range(20,115,8):
 for y in (8,14):b.dot(x,y)
b.line(4,30,124,30);b.line(4,30,4,55);b.line(124,30,124,55)
b.line(37,38,90,38);b.line(37,38,37,55);b.line(90,38,90,55)
b.rect(59,45,4,4,1,True);b.line(60,49,60,53);b.line(60,51,56,54);b.line(60,51,65,54)
sprites=[]
for row in range(7):
 for col in range(16):
  tile=Bitmap(8,8)
  for y in range(8):
   for x in range(8):tile.dot(x,y,b.p[row*8+y][col*8+x])
  sprites.append(tile)
xs=[[64+(target-64)*t//16 for t in range(17)] for target in (24,44,64,84,104)]
ys=[[60+(target-60)*t//16 for t in range(17)] for target in (24,14)]
assets(root,'PENALTY ARC','penalty-arc',16,7,1,1,sprites,3,asm_bytes('penalty_targets',[24,44,64,84,104])+asm_bytes('penalty_flight_x',sum(xs,[]))+asm_bytes('penalty_flight_y',sum(ys,[])),aux=('CANCEL','RESET'))
(root/'flight.json').write_text(json.dumps({'x':xs,'y':ys})+'\n')
