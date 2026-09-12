# SPDX-License-Identifier: MIT
import sys,json,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;directions=[(0,-1,1,2),(0,1,2,1),(-1,0,4,8),(1,0,8,4)]
rotate=lambda m:((m&1)<<3)|((m&8)>>2)|((m&2)<<1)|((m&4)>>2)
from puzzle_assets import campaign, level_records, challenge_data
from visual_art import tiny
levels=campaign(root)
ports=[1,2,4,8,5,6,9,10];sprites=[]
for kind in range(4):
 for m in (range(16) if kind<2 else ports):
  b=Bitmap(16,8)
  for bit,x,y,w,h in ((1,7,0,2,4),(2,7,4,2,4),(4,0,3,8,2),(8,8,3,8,2)):
   if m&bit:b.rect(x,y,w,h,1,True)
  if kind<2:
   b.rect(6,2,4,4,0,True)
   b.rect(6,2,4,4,1,kind==1)
  else:
   # A compact letter leaves both vertical ports visible. The renderer repeats
   # only the outer rows, giving every cell a ten-pixel vertical pitch.
   b.rect(6,1,5,5,0,True)
   tiny(b,'S' if kind==2 else 'E',7,1)
  sprites.append(b)
data=asm_bytes('pipe_rotation',[rotate(m) for m in range(16)])+asm_bytes('pipe_ports',[ports.index(m) if m in ports else 255 for m in range(16)])+level_records([[a*16+b for a,b in zip(s['initial']['board'][::2],s['initial']['board'][1::2])] for s in levels])+challenge_data(root,levels,6,2,1,6)
rows=[2+10*row for row in range(6)]
data+=asm_bytes('pipe_row_bands',[y//8 for y in rows])
data+=asm_bytes('pipe_row_shifts',[y%8 for y in rows])
data+=asm_bytes('pipe_row_clear_low',[(~(1023<<(y%8)))&255 for y in rows])
data+=asm_bytes('pipe_row_clear_high',[(~(1023<<(y%8))>>8)&255 for y in rows])
assets(root,'PIPE WEAVE','pipe-weave',6,6,2,1,sprites,40,data,stat='DRY')
