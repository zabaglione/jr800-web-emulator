# SPDX-License-Identifier: MIT
import sys,json,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets, Bitmap, asm_bytes

from puzzle_assets import campaign, level_records, challenge_data
root=Path(__file__).parent
levels=campaign(root)
sprites=[]
for on in (0,1):
    b=Bitmap(16,8);b.rect(1,0,14,8);b.rect(4,2,8,4,on,True)
    sprites.append(b)
for on in (0,1):
    # Retain the rectangular lamp: filled when on, outlined when off.
    # The inset diamond contrasts with the lamp in either state.
    b=Bitmap(16,8);b.rect(1,0,14,8,1,bool(on))
    for y,(left,right) in enumerate(((7,8),(6,9),(5,10),(5,10),(6,9),(7,8)),1):
        b.line(left,y,right,y,1-on)
    sprites.append(b)
for face in sprites[:4]:
    # Half-width faces rotate about the lamp's vertical center line.
    b=Bitmap(16,8)
    for y in range(8):
        for x in range(8):b.dot(x+4,y,face.p[y][x*2+1 if x<4 else x*2])
    sprites.append(b)
b=Bitmap(16,8);b.rect(7,0,2,8,1,True);sprites.append(b)
data=level_records([s['initial']['board'] for s in levels])+challenge_data(root,levels,5,2,1,5)
assets(root,'LAMP GRID','lamp-grid',5,5,2,1,sprites,40,data)
