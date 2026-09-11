# SPDX-License-Identifier: MIT
"""Braided switch mazes; key, door and optional-mark states are searched together."""
import random,sys
from pathlib import Path
from collections import deque
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from puzzle_design import save
D=((-14,'up'),(14,'down'),(-1,'left'),(1,'right'))

def solve(board,start,marks=()):
    initial=(start,0,0,0);q=deque([initial]);seen={initial:None};want=(1<<len(marks))-1
    while q:
        state=q.popleft();p,gates,keys,bonus=state
        if board[p]==8 and keys==3:
            if bonus==want:
                path=[]
                while seen[state] is not None:state,key=seen[state];path.append(key)
                return list(reversed(path)),len(seen)
            continue
        for d,key in D:
            t=p+d;c=board[t];G=gates;K=keys;B=bonus
            if c==1 or c in (6,7) and not(gates>>(c-6)&1):continue
            if c in (2,3):K|=1<<(c-2)
            if c in (4,5):G^=1<<(c-4)
            for i,mark in enumerate(marks):
                if t==mark:B|=1<<i
            n=(t,G,K,B)
            if n not in seen:seen[n]=(state,key);q.append(n)
    return None,len(seen)

def run():
    rng=random.Random(803103);levels=[]
    rooms=[y*14+x for y in (1,3,5) for x in range(1,12,2)]
    for i in range(40):
        low=16+i*48//39
        for attempt in range(20000):
            board=[1]*98;board[rooms[0]]=0;stack=[rooms[0]]
            while stack:
                p=stack[-1];near=[q for q in rooms if board[q]==1 and (abs(q-p)==28 or abs(q-p)==2 and q//14==p//14)]
                if not near:stack.pop();continue
                q=rng.choice(near);board[(p+q)//2]=board[q]=0;stack.append(q)
            # Alternate between trees with deep branches and mazes with several loops.
            walls=[p for p in range(98) if board[p]==1 and p%14 not in (0,13) and p//14 not in (0,6)
                   and ((board[p-1]==board[p+1]==0) or (board[p-14]==board[p+14]==0))]
            for p in rng.sample(walls,min(len(walls),i%3)):board[p]=0
            corridors=[p for p,c in enumerate(board) if not c and p not in rooms]
            doors=rng.sample(corridors,2 if i<20 else 4)
            for n,p in enumerate(doors):board[p]=6+n%2
            start,end,k1,k2,s1,s2=rng.sample(rooms,6)
            for p,c in ((end,8),(k1,2),(k2,3),(s1,4),(s2,5)):board[p]=c
            normal,work=solve(board,start)
            if not normal or not low<=len(normal)<=low+20:continue
            p=start;visited={start};toggles=0
            for key in normal:
                p+=dict((k,d) for d,k in D)[key];visited.add(p);toggles+=board[p] in (4,5)
            if not {s1,s2,*doors}.issubset(visited) or (i>=30 and toggles<3):continue
            free=[p for p,c in enumerate(board) if c==0 and p not in visited]
            if len(free)<2:continue
            for _ in range(12):
                marks=rng.sample(free,2);bonus,bonus_work=solve(board,start,marks)
                if bonus and len(normal)+4<=len(bonus)<=180:break
            else:continue
            levels.append({'initial':{'start':start,'board':board},'normal':normal,'bonus':bonus,
                           'bonus_cells':marks,'par':len(bonus),'metrics':{'minimum_actions':len(normal),'bonus_minimum_actions':len(bonus),'switch_visits':toggles,'doors':len(doors),'search_states':work,'bonus_search_states':bonus_work}})
            break
        else:raise RuntimeError(f'No qualified switch stage {i+1}')
    save(__file__,levels)

if __name__=='__main__':run()
