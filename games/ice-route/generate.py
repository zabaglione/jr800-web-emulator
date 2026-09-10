# SPDX-License-Identifier: MIT
import sys,json,random
from collections import deque
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent
steps=[(-14,'up'),(14,'down'),(-1,'left'),(1,'right')]
def travel(board,p,mask,d):
    while board[p+d]!=1:
        p+=d
        if board[p] in (3,4):mask|=1<<(board[p]-3)
        if board[p]==2 and mask==3:break
    return p,mask

def solve(board,start):
    q=deque([(start,0,[])]);seen={(start,0)}
    while q:
        p,m,path=q.popleft()
        if board[p]==2 and m==3:return path
        for d,key in steps:
            state=travel(board,p,m,d)
            if state not in seen:seen.add(state);q.append((*state,path+[key]))
r=random.Random(8009);levels=[];solutions=[];boards=[];starts=[]
for stage in range(20):
    while True:
        board=[1 if i%14 in (0,13) or i//14 in (0,6) or r.random()<.24 else 0 for i in range(98)]
        free=[i for i,c in enumerate(board) if c==0]
        if len(free)<4:continue
        start,exit,a,b=r.sample(free,4);board[exit]=2;board[a]=3;board[b]=4
        route=solve(board,start)
        if route and 5+stage//4<=len(route)<=24:break
    levels.extend([start]+board);solutions.append(route);boards.append(board);starts.append(start)
sprites=[]
for n in range(6):
    b=Bitmap(8,8)
    if n==0:b.dot(6,6)
    elif n==1:
        b.rect(0,0,8,8);b.line(1,6,6,1);b.dot(2,2);b.dot(5,5)
    elif n==2:b.rect(1,0,6,8);b.rect(3,2,2,4,1,True)
    elif n in (3,4):
        b.line(3,0,7,3);b.line(7,3,3,7);b.line(3,7,0,3);b.line(0,3,3,0)
        if n==4:b.line(3,0,3,7)
    else:b.rect(2,1,4,3,1,True);b.line(3,4,3,6);b.line(1,6,6,6)
    sprites.append(b)
assets(root,'ICE ROUTE','ice-route',14,7,1,1,sprites,20,asm_bytes('levels',levels),stat='GEMS',action='SLIDE')
(root/'solutions.json').write_text(json.dumps(solutions)+'\n')
(root/'levels.json').write_text(json.dumps([{'start':s,'board':b} for s,b in zip(starts,boards)])+'\n')
