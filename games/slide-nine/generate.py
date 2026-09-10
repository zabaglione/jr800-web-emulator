# SPDX-License-Identifier: MIT
import sys,json,random
from collections import deque
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent
# Breadth-first distances from the goal prove solvability and optimal lengths.
goal=tuple(range(1,9))+(0,);paths={goal:[]};q=deque([goal]);layers={0:[goal]}
while q:
    a=q.popleft();route=paths[a]
    if len(route)>=23:continue
    p=a.index(0)
    for dx,dy,key,opposite in ((0,-1,'up','down'),(0,1,'down','up'),(-1,0,'left','right'),(1,0,'right','left')):
        x,y=p%3+dx,p//3+dy
        if not 0<=x<3 or not 0<=y<3:continue
        t=y*3+x;b=list(a);b[p],b[t]=b[t],b[p];b=tuple(b)
        if b not in paths:
            paths[b]=[opposite]+route;q.append(b);layers.setdefault(len(route)+1,[]).append(b)
r=random.Random(8008);levels=[r.choice(layers[4+i]) for i in range(20)]
sprites=[]
for n in range(9):
    b=Bitmap(32,16);b.rect(0,0,31,16)
    if n:b.text(str(n),11,1,2,2)
    else:b.rect(2,2,27,12,1,True)
    sprites.append(b)
assets(root,'SLIDE NINE','slide-nine',3,3,4,2,sprites,20,asm_bytes('levels',sum((list(x) for x in levels),[])),stat='LEFT',action='MOVE')
(root/'solutions.json').write_text(json.dumps([paths[a] for a in levels])+'\n')
