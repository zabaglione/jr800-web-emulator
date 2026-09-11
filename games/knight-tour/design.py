# SPDX-License-Identifier: MIT
"""Forty shaped knight boards with certified alternative final squares."""
import random,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from puzzle_design import save
ADJ=[]
for p in range(36):
    ADJ.append([y*6+x for x,y in ((p%6+dx,p//6+dy) for dx,dy in ((1,2),(2,1),(2,-1),(1,-2),(-1,-2),(-2,-1),(-2,1),(-1,2))) if 0<=x<6 and 0<=y<6])
MASK=[sum(1<<q for q in row) for row in ADJ]

def alternative(allowed,start,goal):
    dead=set();path=[start]
    def visit(p,left):
        if not left:return p!=goal
        if (p,left) in dead or len(dead)>80000:return False
        candidates=[q for q in ADJ[p] if left>>q&1]
        for q in sorted(candidates,key=lambda q:((MASK[q]&left).bit_count(),q)):
            remaining=left^(1<<q)
            # A disconnected unvisited component cannot be repaired by a knight later.
            connected=1<<q;front=connected
            while front:
                at=(front&-front).bit_length()-1;front&=front-1
                more=MASK[at]&(remaining|(1<<q))&~connected;connected|=more;front|=more
            if connected!=(remaining|(1<<q)):continue
            path.append(q)
            if visit(q,remaining):return True
            path.pop()
        dead.add((p,left));return False
    return (path[:] if visit(start,allowed^(1<<start)) else None),len(dead)

def run():
    rng=random.Random(803108);stages=[];seen=set()
    for i in range(40):
        count=8+i*28//39
        for attempt in range(20000):
            start=rng.randrange(36);path=[start];used={start}
            while len(path)<count:
                near=[q for q in ADJ[path[-1]] if q not in used]
                if not near:break
                near.sort(key=lambda q:(sum(t not in used for t in ADJ[q]),rng.random()))
                q=rng.choice(near[:1 if count>=30 else min(3,len(near))]);path.append(q);used.add(q)
            if len(path)!=count:continue
            mask=sum(1<<p for p in path);signature=(mask,start)
            if signature in seen:continue
            goal=path[-1];normal,work=alternative(mask,start,goal)
            if normal is None:continue
            assert normal[0]==path[0] and normal[-1]!=goal and set(normal)==used
            board=[0 if p in used else 255 for p in range(36)]
            stages.append({'initial':{'board':board,'start':start},'normal':normal[1:],'bonus':path[1:],
                           'bonus_cells':[goal],'par':count-1,'metrics':{'squares':count,'required_jumps':count-1,'alternative_search_states':work}})
            seen.add(signature);break
        else:raise RuntimeError(f'No qualified knight board {i+1}')
    save(__file__,stages)

if __name__=='__main__':run()
