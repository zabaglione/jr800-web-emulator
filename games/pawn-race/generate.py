# SPDX-License-Identifier: MIT
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
from hud_layouts import SPECS
from visual_art import tiny
links=[]
for side in (1,2):
 for p in range(36):
  for dx in (0,-1,1):
   x,y=p%6+dx,p//6+(-1 if side==1 else 1)
   links.append(y*6+x if 0<=x<6 and 0<=y<6 else 255)
sprites=[]
for n in range(6):
 b=Bitmap(16,8);b.line(0,0,15,0);b.line(0,7,15,7);b.line(0,0,0,7)
 if n in (1,2,3,5):
  b.rect(5,1,5,3,1,n in (2,5));b.rect(4,4,7,3,1,n in (2,5))
 if n==3:b.line(13,1,13,6)
 if n in (4,5):b.line(11,3,14,3);b.line(12,2,12,5)
 sprites.append(b)
# Keep the frame at y=9 and inset its label to y=11, within the same LCD band.
foes=Bitmap(16,8)
foes.line(0,1,15,1)
tiny(foes,SPECS['pawn-race']['fields'][0]['label'],0,3)
turns=Bitmap(64,8)
tiny(turns,SPECS['pawn-race']['fields'][1]['label'],5,1)
data=asm_bytes('pawn_links',links)+asm_bytes('pawn_foes_label',foes.bytes())+asm_bytes('pawn_turns_label',turns.bytes())
assets(Path(__file__).parent,'PAWN RACE','pawn-race',6,6,2,1,sprites,3,data,stat='FOES',aux=('UNDO TURN','RESET'))
