# SPDX-License-Identifier: MIT
"""Increasing peg counts, two certified endings, and a marked optional final hole."""
import random,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from puzzle_design import save
HOLES={y*7+x for y in range(7) for x in range(7) if (y in (2,3,4) or (y in (0,1,5) and x in (2,3,4)) or (y==6 and x==3))}
JUMPS=[]
for p in sorted(HOLES):
    for dx,dy in ((0,-1),(0,1),(-1,0),(1,0)):
        x,y=p%7,p//7;m=(y+dy)*7+x+dx;q=(y+dy*2)*7+x+dx*2
        if 0<=x+dx*2<7 and 0<=y+dy*2<7 and m in HOLES and q in HOLES:
            JUMPS.append((p,m,q,(1<<p)|(1<<m),1<<q))

def alternative(initial,goal,limit=220000):
    dead=set();path=[]
    ordered=sorted(JUMPS,key=lambda j:(abs(j[0]%7-3)+abs(j[0]//7-3),-abs(j[2]-goal)),reverse=True)
    def visit(state):
        if state&(state-1)==0:return state!=(1<<goal)
        if state in dead or len(dead)>=limit:return False
        for p,m,q,take,put in ordered:
            if state&take==take and not state&put:
                path.append([p,q])
                if visit(state^take^put):return True
                path.pop()
        dead.add(state);return False
    return (path[:] if visit(initial) else None),len(dead)

def validate(board,path):
    b=board[:]
    for p,q in path:
        m=(p+q)//2;assert b[p]==b[m]==1 and b[q]==0
        b[p]=b[m]=0;b[q]=1
    assert b.count(1)==1
    return b.index(1)

def run():
    rng=random.Random(803107);stages=[];seen=set()
    for i in range(40):
        count=6+i*23//39
        for attempt in range(15000):
            target=rng.choice(sorted(HOLES));state=1<<target;reverse=[]
            while state.bit_count()<count:
                choices=[j for j in JUMPS if not state&j[3] and state&j[4]]
                if not choices:break
                p,m,q,take,put=rng.choice(choices);state^=take^put;reverse.append([p,q])
            if state.bit_count()!=count or state in seen:continue
            normal,work=alternative(state,target)
            if not normal:continue
            board=[int(bool(state>>p&1)) if p in HOLES else 255 for p in range(49)];bonus=list(reversed(reverse))
            assert validate(board,normal)!=target and validate(board,bonus)==target
            stages.append({'initial':{'board':board},'normal':normal,'bonus':bonus,'bonus_cells':[target],
                           'par':count-1,'metrics':{'pegs':count,'required_jumps':count-1,'alternative_search_states':work}})
            seen.add(state);print(f'peg {i+1:02}: {count} pegs, two endings',flush=True);break
        else:raise RuntimeError(f'No qualified peg board {i+1}')
    save(__file__,stages)

if __name__=='__main__':run()
