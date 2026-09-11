# SPDX-License-Identifier: MIT
"""Sliding mazes graded by exact action distance, with optional detours."""
import random,sys
from pathlib import Path
from collections import deque
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from puzzle_design import save
D=((-14,'up'),(14,'down'),(-1,'left'),(1,'right'))

def slide(board,p,keys,bonus,d,marks):
    crossed=[]
    while board[p+d]!=1:
        p+=d;crossed.append(p)
        if board[p] in (3,4):keys|=1<<(board[p]-3)
        for i,q in enumerate(marks):
            if p==q:bonus|=1<<i
        if board[p]==2 and keys==3:break
    return (p,keys,bonus),crossed

def solve(board,start,marks=()):
    initial=(start,0,0);q=deque([initial]);seen={initial:None};want=(1<<len(marks))-1
    while q:
        state=q.popleft();p,k,b=state
        if board[p]==2 and k==3:
            if b==want:
                path=[]
                while seen[state] is not None:state,key=seen[state];path.append(key)
                return list(reversed(path)),len(seen)
            continue
        for d,key in D:
            n,_=slide(board,p,k,b,d,marks)
            if n not in seen:seen[n]=(state,key);q.append(n)
    return None,len(seen)

def run():
    rng=random.Random(803102);levels=[]
    for i in range(40):
        low=4+i*11//39
        for attempt in range(60000):
            board=[1 if p%14 in (0,13) or p//14 in (0,6) or rng.random()<.24 else 0 for p in range(98)]
            free=[p for p,t in enumerate(board) if not t]
            if len(free)<16:continue
            start,end,k1,k2=rng.sample(free,4);board[end]=2;board[k1]=3;board[k2]=4
            normal,work=solve(board,start)
            if not normal or not low<=len(normal)<=low+5:continue
            state=(start,0,0);visited={start}
            for key in normal:
                state,path=slide(board,*state,dict((k,d) for d,k in D)[key],());visited.update(path)
            candidates=[p for p in free if p not in visited and board[p]==0]
            if len(candidates)<2:continue
            for _ in range(16):
                marks=rng.sample(candidates,2);bonus,bonus_work=solve(board,start,marks)
                if bonus and len(normal)+2<=len(bonus)<=48:break
            else:continue
            levels.append({'initial':{'start':start,'board':board},'normal':normal,'bonus':bonus,
                           'bonus_cells':marks,'par':len(bonus),'metrics':{'minimum_actions':len(normal),'bonus_minimum_actions':len(bonus),'search_states':work,'bonus_search_states':bonus_work}})
            break
        else:raise RuntimeError(f'No qualified ice stage {i+1}')
    save(__file__,levels)

if __name__=='__main__':run()
