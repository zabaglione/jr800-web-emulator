# SPDX-License-Identifier: MIT
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
sprites=[]
for p in range(12):
 b=Bitmap(32,8);b.line(0,0,0,7);b.line(31,0,31,7);b.line(0,7,31,7)
 if p:
  s=str(1<<p);b.text(s,(32-len(s)*6+1)//2,0)
 sprites.append(b)
lines=[]
for direction in range(4):
 for line in range(4):
  for step in range(4):
   x,y=(line,step) if direction==0 else (line,3-step) if direction==1 else (step,line) if direction==2 else (3-step,line)
   lines.append(y*4+x)
data=asm_bytes('rail_lines',lines)+'rail_values: .word '+','.join(str(1<<n if n else 0) for n in range(12))+'\n'+asm_bytes('rail_goals',[7,9,11])
assets(Path(__file__).parent,'NUMBER RAIL','number-rail',4,4,4,1,sprites,3,data,stat='TOP',action='     ',aux=('UNDO','RESET'))
