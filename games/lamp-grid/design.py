# SPDX-License-Identifier: MIT
"""Forty increasing Lights Out ranks, with a different valid bonus solution."""
import random
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from puzzle_design import save

CROSSES = []
for p in range(25):
    cells = [p]+[y*5+x for x,y in ((p%5-1,p//5),(p%5+1,p//5),(p%5,p//5-1),(p%5,p//5+1)) if 0<=x<5 and 0<=y<5]
    CROSSES.append(sum(1<<q for q in cells))

def answers(board):
    result = []
    for top in range(32):
        b=board; route=[]
        for x in range(5):
            if top>>x&1: b^=CROSSES[x]; route.append(x)
        for y in range(1,5):
            for x in range(5):
                if b>>((y-1)*5+x)&1: b^=CROSSES[y*5+x]; route.append(y*5+x)
        if not b: result.append(route)
    return sorted(result,key=lambda a:(len(a),a))

def canonical(board):
    variants=[]
    for transpose in (False,True):
        for flipx in (False,True):
            for flipy in (False,True):
                v=0
                for p in range(25):
                    x,y=p%5,p//5
                    if transpose:x,y=y,x
                    if flipx:x=4-x
                    if flipy:y=4-y
                    v|=((board>>p)&1)<<(y*5+x)
                variants.append(v)
    return min(variants)

def run():
    rng=random.Random(803101); stages=[]; seen=set()
    for i in range(40):
        low,high=((2,5),(6,8),(9,11),(12,15))[i//10]
        for attempt in range(30000):
            board=0
            for p in rng.sample(range(25),rng.randint(low, min(24,high+6))):board^=CROSSES[p]
            solutions=answers(board)
            if not solutions or not low<=len(solutions[0])<=high or canonical(board) in seen:continue
            normal=solutions[0]; choices=[]
            for a in solutions[1:]:
                extra=sorted(set(a)-set(normal))
                if not extra or not 2<=len(a)-len(normal)<=10:continue
                marks=rng.sample(extra,min(2,len(extra)))
                valid=[s for s in solutions if set(marks)<=set(s)]
                best=min(valid,key=len)
                if len(best)>len(normal):choices.append((len(best),marks,best))
            if not choices:continue
            par,marks,bonus=min(choices,key=lambda q:q[0])
            # Distinct presses with no solved proper prefix: the last action really clears.
            for path in (normal,bonus):
                state=board
                for n,p in enumerate(path):
                    state^=CROSSES[p]
                    assert bool(state)==(n<len(path)-1)
            stages.append({'initial':{'board':[(board>>p)&1 for p in range(25)]},
                           'normal':normal,'bonus':bonus,'bonus_cells':marks,'par':par,
                           'metrics':{'minimum_actions':len(normal),'bonus_minimum_actions':par,'valid_solutions':len(solutions)}})
            seen.add(canonical(board));break
        else:raise RuntimeError(f'No qualified lamp stage {i+1}')
    save(__file__,stages)

if __name__=='__main__':run()
