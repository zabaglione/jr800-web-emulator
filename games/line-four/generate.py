# SPDX-License-Identifier: MIT
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
sprites=[]
for n in range(3):
    b=Bitmap(16,8);b.line(0,0,0,7);b.line(15,0,15,7)
    if not n:b.dot(7,6);b.dot(8,6)
    else:
        b.line(5,0,10,0);b.line(5,6,10,6);b.line(3,2,3,4);b.line(12,2,12,4)
        for x,y in [(4,1),(11,1),(4,5),(11,5)]:b.dot(x,y)
        if n==2:b.rect(4,2,8,3,1,True);b.line(5,1,10,1);b.line(5,5,10,5)
    sprites.append(b)
links=[]
for dx,dy in ((1,0),(-1,0),(0,1),(0,-1),(1,1),(-1,-1),(-1,1),(1,-1)):
 for p in range(42):
    x,y=p%7+dx,p//7+dy;links.append(y*7+x if 0<=x<7 and 0<=y<6 else 255)
assets(Path(__file__).parent,'LINE FOUR','line-four',7,6,2,1,sprites,3,asm_bytes('line_links',links)+asm_bytes('four_weights',[3,4,5,6,5,4,3]),stat='FREE',action='SPACE',aux=('UNDO TURN','RESET'))
