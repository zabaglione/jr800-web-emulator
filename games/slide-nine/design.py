# SPDX-License-Identifier: MIT
"""Exact 8-puzzle distances and tile-specific optional stamps."""
import random,sys,heapq
from pathlib import Path
from collections import deque
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from puzzle_design import save
GOAL=tuple(range(1,9))+(0,)
NEAR=[]
for p in range(9):
    NEAR.append([(y*3+x,key,opposite) for x,y,key,opposite in
                 ((p%3,p//3-1,'up','down'),(p%3,p//3+1,'down','up'),(p%3-1,p//3,'left','right'),(p%3+1,p//3,'right','left')) if 0<=x<3 and 0<=y<3])

def swap(board,p,q):
    b=list(board);b[p],b[q]=b[q],b[p];return tuple(b)

def database():
    distance={GOAL:0};toward={};layers={0:[GOAL]};queue=deque([GOAL])
    while queue:
        board=queue.popleft();p=board.index(0);d=distance[board]
        for q,key,opposite in NEAR[p]:
            b=swap(board,p,q)
            if b not in distance:
                distance[b]=d+1;toward[b]=opposite;layers.setdefault(d+1,[]).append(b);queue.append(b)
    assert len(distance)==181440 and max(layers)==31
    return distance,toward,layers

def route(board,toward):
    actions=[];stamps=set()
    while board!=GOAL:
        p=board.index(0);key=toward[board];q=next(q for q,k,_ in NEAR[p] if k==key)
        stamps.add((board[q],p));actions.append(key);board=swap(board,p,q)
    return actions,stamps

def stamped_route(board,tile,cell,distance,limit):
    start=(board,False);heap=[(distance[board],0,0,start)];seen={start:0};parent={};serial=0
    while heap and serial<120000:
        _,cost,_,state=heapq.heappop(heap);b,got=state
        if cost!=seen[state]:continue
        if b==GOAL:
            if got:
                path=[]
                while state!=start:state,key=parent[state];path.append(key)
                return list(reversed(path)),len(seen)
            continue
        p=b.index(0)
        for q,key,_ in NEAR[p]:
            n=(swap(b,p,q),got or (b[q]==tile and p==cell));c=cost+1
            if c+distance[n[0]]>limit or c>=seen.get(n,999):continue
            seen[n]=c;parent[n]=(state,key);serial+=1
            heapq.heappush(heap,(c+distance[n[0]],c,serial,n))
    return None,len(seen)

def run():
    distances,toward,layers=database();rng=random.Random(803104);stages=[];used=set()
    for i in range(40):
        depth=3+i if i<10 else 13+(i-10)*8//9 if i<20 else 22+(i-20)*4//9 if i<30 else 27+(i-30)*4//9
        for attempt in range(300):
            board=rng.choice(layers[depth])
            if board in used:continue
            normal,seen=route(board,toward)
            options=[(tile,cell) for tile in range(1,9) for cell in range(9)
                     if cell!=tile-1 and board[cell]!=tile and (tile,cell) not in seen]
            rng.shuffle(options)
            for tile,cell in options[:16]:
                bonus,work=stamped_route(board,tile,cell,distances,depth+8)
                if bonus and depth+2<=len(bonus)<=depth+8:break
            else:continue
            stages.append({'initial':{'board':list(board)},'normal':normal,'bonus':bonus,
                           'bonus_cells':[cell],'bonus_tile':tile,'bonus_label':f'STAMP TILE {tile}',
                           'par':len(bonus),'metrics':{'minimum_actions':depth,'bonus_minimum_actions':len(bonus),'bonus_search_states':work}})
            used.add(board);break
        else:raise RuntimeError(f'No qualified sliding stage {i+1}')
    save(__file__,stages)

if __name__=='__main__':run()
