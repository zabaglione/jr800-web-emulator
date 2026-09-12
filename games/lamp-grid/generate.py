# SPDX-License-Identifier: MIT
import sys,json,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets, Bitmap, asm_bytes

from puzzle_assets import campaign, level_records, challenge_data, pack
from hud import panel_art
root=Path(__file__).parent
levels=campaign(root)
sprites=[]
for target in (0,1):
    for on in (0,1):
        b=Bitmap(16,8)
        # Rounded glass lenses, with contrasting reflections or an inset jewel.
        for y,(left,right) in enumerate(((5,10),(3,12),(2,13),(1,14),(1,14),(2,13),(3,12),(5,10))):
            b.line(left,y,right,y)
        if not on:
            for y,(left,right) in enumerate(((5,10),(3,12),(2,13),(2,13),(3,12),(5,10)),1):
                b.line(left,y,right,y,0)
        if target:
            for y,(left,right) in enumerate(((7,8),(6,9),(5,10),(5,10),(6,9),(7,8)),1):
                b.line(left,y,right,y,1-on)
        else:
            b.dot(5,2,1-on);b.dot(4,3,1-on)
        sprites.append(b)
for face in sprites[:4]:
    # Half-width faces rotate about the lamp's vertical center line.
    b=Bitmap(16,8)
    for y in range(8):
        for x in range(8):b.dot(x+4,y,face.p[y][x*2+1 if x<4 else x*2])
    sprites.append(b)
b=Bitmap(16,8);b.rect(7,0,2,8,1,True);sprites.append(b)
cursor_offset=len(sprites)*2
for face in sprites[:]:
    b=Bitmap(16,8);b.p=[row[:] for row in face.p]
    for x,X in ((0,2),(15,13)):
        b.line(x,0,X,0);b.line(x,7,X,7)
        b.line(x,0,x,2);b.line(x,5,x,7)
    sprites.append(b)
data=level_records([s['initial']['board'] for s in levels])+challenge_data(root,levels,5,2,1,5)
data+=f'.equ LAMP_CURSOR_TILE_OFFSET,{cursor_offset}\n'
data+=asm_bytes('lamp_panel_art',pack(panel_art(sprites).bytes()))
assets(root,'LAMP GRID','lamp-grid',5,5,2,1,sprites,40,data)
