# SPDX-License-Identifier: MIT
import sys,json,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;directions=[(0,-1,1,2),(0,1,2,1),(-1,0,4,8),(1,0,8,4)]
rotate=lambda m:((m&1)<<3)|((m&8)>>2)|((m&2)<<1)|((m&4)>>2)
from puzzle_assets import campaign, level_records, challenge_data
levels=campaign(root)
ports=[1,2,4,8,5,6,9,10];sprites=[]
for kind in range(4):
 for m in (range(16) if kind<2 else ports):
  b=Bitmap(16,8)
  for bit,x,y in ((1,7,0),(2,7,7),(4,0,3),(8,15,3)):
   if m&bit:b.line(7,3,x,y);b.line(8,4,min(x+1,15),min(y+1,7))
  b.rect(5,1,6,6,0,True)
  if kind<2:b.rect(5,1,6,6,1,kind==1)
  else:b.text('S' if kind==2 else 'E',5,0)
  sprites.append(b)
data=asm_bytes('pipe_rotation',[rotate(m) for m in range(16)])+asm_bytes('pipe_ports',[ports.index(m) if m in ports else 255 for m in range(16)])+level_records([[a*16+b for a,b in zip(s['initial']['board'][::2],s['initial']['board'][1::2])] for s in levels])+challenge_data(root,levels,6,2,1,6)
assets(root,'PIPE WEAVE','pipe-weave',6,6,2,1,sprites,40,data,stat='DRY')
