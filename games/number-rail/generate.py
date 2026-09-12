# SPDX-License-Identifier: MIT
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent
from puzzle_assets import campaign,level_records,challenge_data
levels=campaign(root)
from digit_art import draw_digits
from visual_art import tiny
sprites=[]
for p in range(12):
 b=Bitmap(32,8)
 # Uniform flat rails leave all seven number rows intact. No displaced
 # corners or shadows: an 8-dot row has no room for an additional top face.
 b.line(0,0,0,7);b.line(31,0,31,7);b.line(0,7,31,7)
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
# Keep the goal cue outside the seven-row digits, even when displaying 2048.
pointer = ['...#....', '#######.', '.#####..', '..###...',
           '...#....', '........', '........', '........']
check = ['........', '......#.', '.....##.', '.#..##..',
         '.####...', '..##....', '........', '........']
pending = ['........', '.#####..', '.#...#..', '.#...#..',
           '.#...#..', '.#####..', '........', '........']
for label, rows in [('rail_hint_down', pointer), ('rail_hint_up', pointer[::-1]),
                    ('rail_hint_check', check), ('rail_hint_pending', pending)]:
 data += asm_bytes(label, Bitmap.from_rows(rows).bytes())
legend=Bitmap(20,8);tiny(legend,'BONUS',0,1)
data+=asm_bytes('rail_bonus_label',legend.bytes())
assets(Path(__file__).parent,'NUMBER RAIL','number-rail',4,4,4,1,sprites,40,data,stat='TOP',action='     ',aux=('UNDO','RESET'))
