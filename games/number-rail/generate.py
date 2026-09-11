# SPDX-License-Identifier: MIT
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent
from puzzle_assets import campaign,level_records,challenge_data
levels=campaign(root)
from digit_art import draw_digits
sprites=[]
for p in range(12):
 b=Bitmap(32,8)
 if p:
  # Seven rows belong to the number; the lower lip and side never cross its strokes.
  b.line(1,7,31,7);b.line(0,2,0,6);b.line(31,1,31,7);b.dot(1,1);b.dot(30,0)
 else:b.line(0,7,31,7)
 if p:
  s=str(1<<p);draw_digits(b,s,(32-len(s)*6+1)//2,0,'number-rail')
 sprites.append(b)
lines=[]
for direction in range(4):
 for line in range(4):
  for step in range(4):
   x,y=(line,step) if direction==0 else (line,3-step) if direction==1 else (step,line) if direction==2 else (3-step,line)
   lines.append(y*4+x)
data=asm_bytes('rail_lines',lines)+'rail_values: .word '+','.join(str(1<<n if n else 0) for n in range(12))+'\n'+asm_bytes('rail_goals',[s['initial']['goal'] for s in levels])+asm_bytes('rail_seeds',[s['initial']['seed'] for s in levels])+level_records([[a*16+b for a,b in zip(s['initial']['board'][::2],s['initial']['board'][1::2])] for s in levels])+challenge_data(root,levels,4,4,1,4)
assets(Path(__file__).parent,'NUMBER RAIL','number-rail',4,4,4,1,sprites,40,data,stat='TOP',action='     ',aux=('UNDO','RESET'))
