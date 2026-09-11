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
starts=[0,5,14,15,30,35];solutions=[]
for start in starts:
 path=[start];seen={start}
 def visit(p):
    if len(path)==36:return True
    for q in sorted((q for q in adj[p] if q not in seen),key=lambda q:(sum(t not in seen for t in adj[q]),q)):
        seen.add(q);path.append(q)
        if visit(q):return True
        path.pop();seen.remove(q)
    return False
 assert visit(start);solutions.append(path[:])
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
assets(root,'KNIGHT TOUR','knight-tour',6,6,2,1,sprites,6,asm_bytes('knight_links',links)+asm_bytes('knight_starts',starts),stat='LEFT',action='SPACE')
(root/'solutions.json').write_text(json.dumps(solutions)+'\n')
