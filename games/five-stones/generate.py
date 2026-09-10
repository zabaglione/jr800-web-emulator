# SPDX-License-Identifier: MIT
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
sprites=[]
for n in range(3):
 b=Bitmap(8,8);b.line(0,3,7,3);b.line(3,0,3,7)
 if n:
    b.rect(0,0,7,7,0,True)
    for y in range(-3,4):
     for x in range(-3,4):
        if x*x+y*y<=10 and (n==2 or x*x+y*y>=5):b.dot(3+x,3+y)
 sprites.append(b)
links=[]
for dx,dy in ((1,0),(-1,0),(0,1),(0,-1),(1,1),(-1,-1),(-1,1),(1,-1)):
 for p in range(98):
    x,y=p%14+dx,p//14+dy;links.append(y*14+x if 0<=x<14 and 0<=y<7 else 255)
weights=[max(0,6-abs(p%14-6)-abs(p//14-3)) for p in range(98)]
assets(Path(__file__).parent,'FIVE STONES','five-stones',14,7,1,1,sprites,1,asm_bytes('line_links',links)+asm_bytes('five_weights',weights),stat='FREE',action='SPACE',aux=('UNDO TURN','RESET'))
