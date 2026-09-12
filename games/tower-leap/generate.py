# SPDX-License-Identifier: MIT
import sys,json,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;levels=[]
for stage in range(12):
 r=random.Random(3100+stage);xs=[6]
 for i in range(11):
  options=[x for x in (xs[-1]-3,xs[-1]-2,xs[-1]+2,xs[-1]+3) if 1<=x<=12]
  xs.append(r.choice(options))
 levels.append(xs)
a=Bitmap(8,8);a.line(0,7,7,7);a.dot(0,6);a.dot(7,6)
b=Bitmap(8,8);b.line(0,7,7,7);b.line(0,6,7,6)
c=Bitmap(8,8);c.line(0,7,7,7);c.line(3,3,3,7);c.line(0,3,7,3)
moving=Bitmap(8,8);moving.line(0,7,7,7);moving.line(1,4,6,4);moving.line(1,4,3,2);moving.line(6,4,4,6)
crumble=Bitmap(8,8);crumble.line(0,7,2,7);crumble.line(4,7,7,7);crumble.line(3,3,4,4);crumble.line(4,4,3,5);crumble.line(3,5,4,6)
warning=Bitmap(8,8);warning.line(0,7,1,7);warning.line(4,7,5,7);warning.line(3,4,3,6)
kinds=[]
for stage in range(12):
 k=[0]*12
 for floor in (2,6):k[floor]=1
 for floor in (3,7):k[floor]=2
 if stage>=4:k[9]=1
 if stage>=8:k[10]=2
 kinds.append(k)
points=[(x,y) for y,row in enumerate((6,6,15,6,9,9)) for x in range(4) if row&(1<<x)]
assets(root,'TOWER LEAP','tower-leap',16,7,1,1,[a,b,c,moving,crumble,warning],12,asm_bytes('tower_levels',sum(levels,[]))+asm_bytes('tower_kinds',sum(kinds,[]))+asm_bytes('tower_sprite_x',[p[0] for p in points])+asm_bytes('tower_sprite_y',[p[1] for p in points]),aux=('CHECKPOINT','RESET'))
(root/'levels.json').write_text(json.dumps(levels)+'\n')
(root/'gimmicks.json').write_text(json.dumps({'kinds':kinds,'movingTicks':12,'crumbleTicks':[max(6,14-stage) for stage in range(12)],'enemyFloor':[5+stage%3 for stage in range(12)]})+'\n')
