# SPDX-License-Identifier: MIT
import sys,json,random
from collections import deque
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent
from puzzle_assets import campaign, level_records, challenge_data
levels=campaign(root)
from depth_art import raised
from digit_art import draw_digits
sprites=[]
for n in range(9):
    b=Bitmap(32,16)
    if n:
        raised(b);draw_digits(b,str(n),10,4,'slide-nine',2,1)
    else:
        b.rect(1,1,30,14,1,True);b.line(2,13,29,13,0);b.line(29,3,29,13,0)
    sprites.append(b)
data=level_records([s['initial']['board'] for s in levels])+asm_bytes('slide_tokens',[s['bonus_tile'] for s in levels])+challenge_data(root,levels,3,4,2,3)
assets(root,'SLIDE NINE','slide-nine',3,3,4,2,sprites,40,data,stat='LEFT',action='MOVE')
