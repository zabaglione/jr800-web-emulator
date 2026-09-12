# SPDX-License-Identifier: MIT
import sys,json,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
from visual_art import poly
from design import reachable_cells
from hud import panel_art
root=Path(__file__).parent
from puzzle_assets import campaign, level_records, challenge_data, pack
levels=campaign(root)
for stage in levels:
 assert reachable_cells(stage['initial']['board'])=={p for p,v in enumerate(stage['initial']['board']) if v}

def route(b,mask):
 b.rect(6,2,4,4,1,True)
 for bit,x,y in ((1,7,0),(2,7,7),(4,0,3),(8,15,3)):
  if mask&bit:b.line(7,3,x,y)

def gate(b,letter,mask=0):
 if mask:route(b,mask)
 poly(b,[(5,0),(10,0),(13,3),(13,4),(10,7),(5,7),(2,4),(2,3)],1,True)
 b.text(letter,5,0,c=0)

sprites=[]
for n in range(22):
 b=Bitmap(16,8)
 if n==0:
  for y in range(0,8,2):
   for x in range(y%4,16,4):b.dot(x,y)
 elif n==1:b.rect(6,2,4,4)
 elif n<=5:gate(b,{2:'A',3:'B',4:'C',5:'S'}[n])
 else:
  route(b,n-6)
 sprites.append(b)
# Large, centrally placed bonus jewel, collected when the route reaches it.
bonus_sprite=len(sprites)
b=Bitmap(16,8);poly(b,[(7,0),(12,3),(8,7),(3,4)],1,True)
b.rect(7,2,2,3,0,True);sprites.append(b)

# Keep gate lettering visible after a turn, including both links at the start.
gate_sprites=[]
sprite_ids={tuple(sprite.bytes()):i for i,sprite in enumerate(sprites)}
for value,letter in enumerate('ABCS',2):
 for mask in range(16):
  if 1<=mask.bit_count()<=2:
   b=Bitmap(16,8);gate(b,letter,mask);key=tuple(b.bytes())
   if key not in sprite_ids:sprite_ids[key]=len(sprites);sprites.append(b)
   gate_sprites.append(sprite_ids[key])
  else:gate_sprites.append(value)
data=level_records([s['initial']['board'] for s in levels])+challenge_data(root,levels,6,2,1,6)
data+=f'.equ LOOP_BONUS_SPRITE,{bonus_sprite}\n'+asm_bytes('loop_gate_sprites',gate_sprites)
states=[]
for state in range(3):
 b=Bitmap(16,8)
 if state==0:b.line(6,3,9,3)
 elif state==1:
  poly(b,[(7,1),(11,5),(4,5)],1,True)
 else:
  for dx in (0,1):
   b.line(3+dx,3,6+dx,6);b.line(6+dx,6,12+dx,0)
 states+=b.bytes()
data+=asm_bytes('loop_state_art',states)+asm_bytes('loop_panel_art',pack(panel_art().bytes()))
data+=asm_bytes('loop_draw_label',list(b'DRAW        \0'))+asm_bytes('loop_close_label',list(b'SPACE: CLOSE\0'))
assets(root,'LOOP TRACE','loop-trace',6,6,2,1,sprites,40,data,stat='LEFT',action='SPACE',aux=('UNDO','RESET'))
