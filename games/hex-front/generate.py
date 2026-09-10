# SPDX-License-Identifier: MIT
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
sprites=[]
for n in range(3):
 b=Bitmap(16,8)
 for x,y in ((0,3),(15,3),(7,0),(7,7),(15,0),(0,7)):b.line(7,3,x,y)
 if n:
    b.rect(4,0,7,7,0,True)
    for y in range(-3,4):
     for x in range(-3,4):
        if x*x+y*y<=10 and (n==2 or x*x+y*y>=5):b.dot(7+x,3+y)
 sprites.append(b)
links=[]
for p in range(36):
 for dx,dy in ((0,-1),(0,1),(-1,0),(1,0),(1,-1),(-1,1)):
    x,y=p%6+dx,p//6+dy;links.append(y*6+x if 0<=x<6 and 0<=y<6 else 255)
sources=[int((p//6==0) if edge==0 else (p//6==5) if edge==1 else (p%6==0) if edge==2 else (p%6==5)) for edge in range(4) for p in range(36)]
weights=[max(0,5-abs(p%6-2)-abs(p//6-2)) for p in range(36)]
assets(Path(__file__).parent,'HEX FRONT','hex-front',6,6,2,1,sprites,1,asm_bytes('hex_links',links)+asm_bytes('hex_sources',sources)+asm_bytes('hex_weights',weights),stat='NEED',action='SPACE',aux=('RELAY','UNDO TURN'))
