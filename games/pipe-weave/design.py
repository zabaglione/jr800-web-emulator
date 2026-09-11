# SPDX-License-Identifier: MIT
"""Original branching networks; normal outlet and completely sealed certificates."""
import random,sys
from collections import deque
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from puzzle_design import save
DIR=((0,-1,1,2),(0,1,2,1),(-1,0,4,8),(1,0,8,4))
def rotate(m):return ((m&1)<<3)|((m&8)>>2)|((m&2)<<1)|((m&4)>>2)
def trace(board):
    wet={0};todo=[0];leaks=0
    for p,m in enumerate(board):
        for dx,dy,bit,opp in DIR:
            if not m&bit:continue
            x,y=p%6+dx,p//6+dy;q=y*6+x
            if not (0<=x<6 and 0<=y<6) or not board[q]&opp:leaks+=1
    for p in todo:
        for dx,dy,bit,opp in DIR:
            x,y=p%6+dx,p//6+dy;q=y*6+x
            if 0<=x<6 and 0<=y<6 and board[p]&bit and board[q]&opp and q not in wet:wet.add(q);todo.append(q)
    return wet,leaks

def route(board,solved,cells):
    out=[];b=board[:]
    for p in cells:
        while b[p]!=solved[p]:
            b[p]=rotate(b[p]);out.append(p)
            if 35 in trace(b)[0]:return out,b
    return out,b

def run():
    rng=random.Random(803109);stages=[];seen=set()
    for i in range(40):
        for attempt in range(20000):
            m=[0]*36;visited={0};stack=[0];parent={}
            while stack:
                p=stack[-1];edges=[]
                for dx,dy,bit,opp in DIR:
                    x,y=p%6+dx,p//6+dy;q=y*6+x
                    if 0<=x<6 and 0<=y<6 and q not in visited:edges.append((q,bit,opp))
                if not edges:stack.pop();continue
                q,bit,opp=rng.choice(edges);m[p]|=bit;m[q]|=opp;parent[q]=p;visited.add(q)
                if q!=35:stack.append(q)
            if len(visited)!=36:continue
            path=[35]
            while path[-1]:path.append(parent[path[-1]])
            path.reverse()
            if len(path)<(11,15,19,23)[i//10]:continue
            # Later levels add branches joining into genuine cycles.
            for _ in range(i//10):
                p=rng.randrange(35);dx,dy,bit,opp=rng.choice(DIR);x,y=p%6+dx,p//6+dy;q=y*6+x
                if 0<=x<6 and 0<=y<6 and q!=35:m[p]|=bit;m[q]|=opp
            b=m[:];scramble=rng.sample(range(35),min(35,4+i))
            for p in scramble:
                for _ in range(rng.randrange(1,4)):b[p]=rotate(b[p])
            b[35]=rotate(m[35])
            if tuple(b) in seen or 35 in trace(b)[0]:continue
            order=[p for p in range(35) if p not in path]+path[:-1]+[35]
            normal,nb=route(b,m,path);bonus,bb=route(b,m,order)
            wet,leaks=trace(nb)
            if not normal or 35 not in wet or (len(wet)==36 and not leaks):continue
            if bb!=m or len(bonus)<=len(normal)+2:continue
            assert trace(bb)==(set(range(36)),0)
            stages.append({'initial':{'board':b},'normal':normal,'bonus':bonus,'bonus_cells':[],
                           'par':len(bonus),'metrics':{'outlet_path_cells':len(path),'sealed_turns':len(bonus),'normal_turns':len(normal),'branch_junctions':sum(x.bit_count()>=3 for x in m)}})
            seen.add(tuple(b));break
        else:raise RuntimeError(f'No network {i+1}')
    save(__file__,stages)
if __name__=='__main__':run()
