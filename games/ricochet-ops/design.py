# SPDX-License-Identifier: MIT
"""Exact shot-state search, including optional rays before the final target falls."""
import random,sys
from collections import deque
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from puzzle_design import save
DIRS=((0,-1),(1,-1),(1,0),(1,1),(0,1),(-1,1),(-1,0),(-1,-1))
SLASH=(2,1,0,7,6,5,4,3);BACK=(6,5,4,3,2,1,0,7)
def trace(board,col,angle):
    p=40+col;start=p;seen=set();path=[];refs=0
    while True:
        dx,dy=DIRS[angle];x=p%8+dx;y=p//8+dy
        if not(0<=x<8 and 0<=y<6):break
        p=y*8+x
        if board[p]==1 or p==start or (p,angle) in seen:break
        seen.add((p,angle));path.append(p)
        if board[p] in (2,3):angle=(SLASH if board[p]==2 else BACK)[angle];refs+=1
    return path,refs

def solve(shots,targets,marks=()):
    goal=(1<<len(targets))-1;need=(1<<len(marks))-1;todo=deque([(0,0,[])]);seen={(0,0)}
    while todo:
        hit,bits,route=todo.popleft()
        for col,angle,path,refs in shots:
            h,b=hit,bits
            for p in path:
                if p in marks:b|=1<<marks.index(p)
                if p in targets:h|=1<<targets.index(p)
                if h==goal:break
            if h==goal:
                if b==need:return route+[[col,angle]]
                continue
            if (h,b) not in seen:seen.add((h,b));todo.append((h,b,route+[[col,angle]]))
    return None

def simulate(shots,targets,route):
    hit=set();visited=set();refs=0
    for c,a in route:
        _,_,path,r=next(s for s in shots if s[:2]==(c,a));refs+=r
        for p in path:
            visited.add(p)
            if p in targets:hit.add(p)
            if len(hit)==len(targets):return visited,refs
    return visited,refs

def run():
    rng=random.Random(803113);stages=[]
    for i in range(40):
        count=3+i*4//39;mirrors=7+i*6//39
        for attempt in range(40000):
            board=[int(y==0 or(x in(0,7) and y<5)) for y in range(6) for x in range(8)]
            cells=[p for p in range(48) if 0<p//8<5 and 0<p%8<7];rng.shuffle(cells)
            for p in cells[:mirrors]:board[p]=rng.choice((2,3))
            if i<30:
                for p in cells[mirrors:mirrors+1]:board[p]=1
            shots=[(c,a,*trace(board,c,a)) for c in range(1,7) for a in range(8)]
            shots.sort(key=lambda s:-s[3]);reach=sorted({p for _,_,path,_ in shots for p in path if p<40 and board[p]==0})
            if len(reach)<count+2:continue
            targets=rng.sample(reach,count);normal=solve(shots,targets)
            if normal is None or len(normal)<(1,2,3,4)[i//10]:continue
            visited,refs=simulate(shots,targets,normal);extras=[p for p in reach if p not in visited and p not in targets]
            if len(extras)<2:continue
            marks=rng.sample(extras,2);bonus=solve(shots,targets,marks)
            if bonus is None or len(bonus)<=len(normal) or len(bonus)>8:continue
            if simulate(shots,targets,bonus)[1]<2:continue
            for p in targets:board[p]=4
            stages.append({'initial':{'board':board,'ammo':len(bonus)+4},'normal':normal,'bonus':bonus,'bonus_cells':marks,
                           'par':len(bonus),'metrics':{'targets':count,'mirrors':mirrors,'normal_optimal':len(normal),'bonus_optimal':len(bonus),'bonus_reflections':simulate(shots,targets,bonus)[1]}})
            print(i+1,count,len(normal),len(bonus),flush=True);break
        else:raise RuntimeError(f'No shot layout {i+1}')
    save(__file__,stages)
if __name__=='__main__':run()
