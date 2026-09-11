# SPDX-License-Identifier: MIT
import sys,json,random
from collections import deque
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent
from puzzle_assets import campaign, level_records, challenge_data
levels=campaign(root)
from depth_art import block8
sprites=[]
for n in range(8):
    b=Bitmap(8,8)
    if n==0:pass  # Clear ice: no dots resembling small collectible marks.
    elif n==1:
        b=block8('ice')
    elif n==2:b.rect(1,0,6,8);b.rect(3,2,2,4,1,True)
    elif n in (3,4):
        b.line(3,1,6,3);b.line(6,3,3,6);b.line(3,6,1,3);b.line(1,3,3,1)
    else:
        # Solid hood and two skates; hollow diamonds remain visually distinct.
        b.rect(2,0,4,3,1,True);b.dot(4,1,0)
        b.rect(2,3,4,3,1,True)
        b.line(2,5,1,7);b.line(5,5,6,7)
        b.line(0,7,2,7);b.line(5,7,7,7)
        if n==5:b.line(0,4,7,4)
        elif n==6:b.line(0,1,2,4);b.line(5,4,7,1)
        else:b.line(0,3,2,4);b.line(5,4,7,3)
    sprites.append(b)
data=level_records([s['initial']['board'] for s in levels])+asm_bytes('start_cells',[s['initial']['start'] for s in levels])+challenge_data(root,levels,14,1,1,7)
assets(root,'ICE ROUTE','ice-route',14,7,1,1,sprites,40,data,stat='GEMS',action='SLIDE')
