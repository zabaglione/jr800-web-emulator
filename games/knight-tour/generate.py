# SPDX-License-Identifier: MIT
import sys,json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent
links=[];adj=[]
for p in range(36):
 row=[]
 for dx,dy in ((1,2),(2,1),(2,-1),(1,-2),(-1,-2),(-2,-1),(-2,1),(-1,2)):
    x,y=p%6+dx,p//6+dy;row.append(y*6+x if 0<=x<6 and 0<=y<6 else 255)
 links+=row;adj.append([q for q in row if q!=255])
from puzzle_assets import campaign, level_records, challenge_data
levels=campaign(root)
sprites=[]
for n in range(39):
 b=Bitmap(16,8);b.line(0,0,0,7);b.line(15,0,15,7);b.line(0,7,15,7)
 if 1<=n<=36:b.text(f'{n:02}',2,0)
 elif n==37:
    rows=[12,30,50,60,24,48,62]
    for y,row in enumerate(rows):
     for x in range(8):
        if row>>(7-x)&1:b.dot(4+x,y)
 elif n==38:b.line(5,3,10,3);b.line(7,1,7,5)
 sprites.append(b)
b=Bitmap(16,8)
for y in range(1,8,2):
    for x in range(y%4,16,4):b.dot(x,y)
sprites.append(b)
data=asm_bytes('knight_links',links)+asm_bytes('knight_starts',[s['initial']['start'] for s in levels])+level_records([s['initial']['board'] for s in levels])+challenge_data(root,levels,6,2,1,6)
assets(root,'KNIGHT TOUR','knight-tour',6,6,2,1,sprites,40,data,stat='LEFT',action='SPACE')
