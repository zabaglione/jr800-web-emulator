# SPDX-License-Identifier: MIT
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;sprites=[Bitmap(24,16)]
pips=[[],[(11,7)],[(6,3),(16,11)],[(6,3),(11,7),(16,11)],[(6,3),(16,3),(6,11),(16,11)],[(6,3),(16,3),(11,7),(6,11),(16,11)],[(6,3),(16,3),(6,7),(16,7),(6,11),(16,11)]]
for n in range(1,7):
 b=Bitmap(24,16);b.rect(1,0,22,16)
 for x,y in pips[n]:b.rect(x-1,y-1,3,3,1,True)
 sprites.append(b)
assets(root,'PUSH LUCK','push-luck',5,1,3,2,sprites,3,asm_bytes('luck_limits',[12,18,24]),aux=('BANK','RESET'))
