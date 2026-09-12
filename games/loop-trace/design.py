# SPDX-License-Identifier: MIT
"""Simple ordered checkpoint loops with branches and optional detours; exact bounded search."""
import random,sys
from functools import lru_cache
from collections import deque
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from puzzle_design import save,grid_neighbors
ADJ=grid_neighbors(6,6)

def reachable_cells(board):
    start=board.index(5);reached={start};todo=[start]
    for p in todo:
        for q in ADJ[p]:
            if board[q] and q not in reached:reached.add(q);todo.append(q)
    return reached

def solve(board,marks=(),limit=36):
    start=board.index(5);check=[board.index(n) for n in (2,3,4)];open_cells={p for p,v in enumerate(board) if v}
    dist={}
    for p in open_cells:
        d={p:0};todo=[p]
        for q in todo:
            for r in ADJ[q]:
                if r in open_cells and r not in d:d[r]=d[q]+1;todo.append(r)
        dist[p]=d
    @lru_cache(None)
    def lower(p,n,mask):
        if n==3 and mask==(1<<len(marks))-1:return dist[p].get(start,99)
        options=[]
        if n<3:options.append(dist[p].get(check[n],99)+lower(check[n],n+1,mask))
        for i,q in enumerate(marks):
            if not mask>>i&1:options.append(dist[p].get(q,99)+lower(q,n,mask|1<<i))
        return min(options)
    nodes=0;path=[]
    def visit(p,n,mask,seen,budget):
        nonlocal nodes
        nodes+=1
        if nodes>250000:raise TimeoutError
        if lower(p,n,mask)>budget:return False
        if n==3 and mask==(1<<len(marks))-1 and start in ADJ[p]:return True
        if budget<2:return False
        candidates=[]
        for q in ADJ[p]:
            if q not in open_cells or seen>>q&1:continue
            nxt=n
            if q in check:
                if q!=check[n] if n<3 else True:continue
                nxt+=1
            bits=mask
            for i,t in enumerate(marks):
                if q==t:bits|=1<<i
            candidates.append((lower(q,nxt,bits),q,nxt,bits))
        for _,q,nxt,bits in sorted(candidates):
            path.append(q)
            if visit(q,nxt,bits,seen|1<<q,budget-1):return True
            path.pop()
        return False
    for bound in range(lower(start,0,0),limit+1):
        if visit(start,0,0,1<<start,bound):return path[:],nodes
    return None,nodes

def run():
    rng=random.Random(803110);stages=[];seen=set()
    for i in range(40):
        target=12+2*(i*10//39)
        for attempt in range(20000):
            x,y=rng.randrange(5),rng.randrange(5);path=[(x,y),(x+1,y),(x+1,y+1),(x,y+1)]
            while len(path)<target:
                candidates=[]
                for j,p in enumerate(path):
                    q=path[(j+1)%len(path)];dx,dy=q[0]-p[0],q[1]-p[1]
                    for sign in (-1,1):
                        r=(p[0]-dy*sign,p[1]+dx*sign);s=(q[0]-dy*sign,q[1]+dx*sign)
                        if all(0<=X<6 and 0<=Y<6 for X,Y in (r,s)) and r not in path and s not in path:candidates.append((j,r,s))
                if not candidates:break
                j,r,s=rng.choice(candidates);path[j+1:j+1]=[r,s]
            if len(path)!=target:continue
            off=rng.randrange(target);path=path[off:]+path[:off];cycle=[y*6+x for x,y in path]
            board=[int(p in cycle) for p in range(36)]
            for p in rng.sample([p for p in range(36) if not board[p]],min(1+i//10,36-target)):board[p]=1
            board[cycle[0]]=5
            for n in (1,2,3):board[cycle[target*n//4]]=n+1
            connected=reachable_cells(board)
            board=[v if p in connected else 0 for p,v in enumerate(board)]
            if tuple(board) in seen:continue
            try:
                normal,work=solve(board,limit=target)
                if normal is None or len(normal)+1<(8,12,14,18)[i//10]:continue
                extras=[p for p in cycle if p not in normal and board[p]==1]
                if len(extras)<2:continue
                marks=rng.sample(extras,2);bonus,more=solve(board,marks,limit=target)
            except TimeoutError:continue
            if bonus is None or len(bonus)<len(normal)+2:continue
            assert set(marks)<=set(bonus) and not set(marks)&set(normal)
            stages.append({'initial':{'board':board,'start':cycle[0]},'normal':normal+['close'],'bonus':bonus+['close'],
                           'bonus_cells':marks,'par':len(bonus)+1,'metrics':{'normal_optimal':len(normal)+1,'bonus_optimal':len(bonus)+1,'search_states':work+more,'open_squares':sum(v!=0 for v in board)}})
            seen.add(tuple(board));print(i+1,len(normal)+1,len(bonus)+1,flush=True);break
        else:raise RuntimeError(f'No qualified loop {i+1}')
    save(__file__,stages)
if __name__=='__main__':run()
