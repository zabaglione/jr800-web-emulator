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
points=[(x,y) for y,row in enumerate((6,6,15,6,9,9)) for x in range(4) if row&(1<<x)]
assets(root,'TOWER LEAP','tower-leap',16,7,1,1,[a,b,c],12,asm_bytes('tower_levels',sum(levels,[]))+asm_bytes('tower_sprite_x',[p[0] for p in points])+asm_bytes('tower_sprite_y',[p[1] for p in points]),aux=('CHECKPOINT','RESET'))
(root/'levels.json').write_text(json.dumps(levels)+'\n')
