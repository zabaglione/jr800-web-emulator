# SPDX-License-Identifier: MIT
import sys,json,random
from collections import deque
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent
steps=[(-14,'up'),(14,'down'),(-1,'left'),(1,'right')]
def solve(board,start):
    q=deque([(start,0,0,[])]);seen={(start,0,0)}
    while q:
        p,m,k,path=q.popleft()
        if board[p]==8 and k==3:return path
        for d,key in steps:
            t=p+d;c=board[t];M=m;K=k
            if c==1 or c in (6,7) and not (m>>(c-6)&1):continue
            if c in (2,3):K|=1<<(c-2)
            if c in (4,5):M^=1<<(c-4)
            state=(t,M,K)
            if state not in seen:seen.add(state);q.append((*state,path+[key]))
r=random.Random(8010);levels=[];solutions=[];meta=[]
rooms=[y*14+x for y in (1,3,5) for x in range(1,12,2)]
for stage in range(20):
    while True:
        board=[1]*98;board[rooms[0]]=0;stack=[rooms[0]]
        while stack:
            p=stack[-1];candidates=[q for q in rooms if board[q]==1 and (abs(q-p)==28 or abs(q-p)==2 and q//14==p//14)]
            if not candidates:stack.pop();continue
            q=r.choice(candidates);board[(p+q)//2]=board[q]=0;stack.append(q)
        corridors=[i for i,c in enumerate(board) if c==0 and i not in rooms]
        d1,d2=r.sample(corridors,2);board[d1]=6;board[d2]=7
        start,exit,k1,k2,s1,s2=r.sample(rooms,6)
        for p,c in ((exit,8),(k1,2),(k2,3),(s1,4),(s2,5)):board[p]=c
        route=solve(board,start)
        if not route or not 24+stage//2<=len(route)<=100:continue
        visited=set();p=start
        for key in route:p+=dict((k,d) for d,k in steps)[key];visited.add(p)
        if not {d1,d2,s1,s2}.issubset(visited):continue
        break
    levels.extend([start]+board);solutions.append(route);meta.append({'start':start,'board':board})
sprites=[]
for n in range(12):
    b=Bitmap(8,8)
    if n==0:b.dot(6,6)
    elif n==1:
        b.rect(0,0,8,8);b.line(0,3,7,3);b.line(3,0,3,3);b.line(5,4,5,7)
    elif n in (2,3):b.rect(1,0,4,4);b.line(4,3,4,7);b.line(4,6,6,6)
    elif n in (4,5):b.text('A' if n==4 else 'B',1,0);b.dot(0,7);b.dot(7,7)
    elif n in (6,7):
        b.rect(0,0,8,8)
        for x in ([2,5] if n==6 else [1,3,5]):b.line(x,1,x,6)
    elif n==8:b.rect(1,0,6,8);b.text('E',2,0)
    elif n in (9,10):
        b.line(0,0,7,0);b.line(0,7,7,7);b.dot(0,3);b.dot(7,4)
    else:b.rect(2,0,4,3,1,True);b.line(3,3,3,6);b.line(1,6,5,6)
    sprites.append(b)
assets(root,'SWITCH MAZE','switch-maze',14,7,1,1,sprites,20,asm_bytes('levels',levels),stat='KEYS',action='MOVE')
(root/'solutions.json').write_text(json.dumps(solutions)+'\n');(root/'levels.json').write_text(json.dumps(meta)+'\n')
