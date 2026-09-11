# SPDX-License-Identifier: MIT
"""Deterministic merge challenges with independent bounded beam-search certificates.
The par is a verified reference route, not a claim of an optimal 2048 solution.
"""
import random,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from puzzle_design import save
KEYS=('up','down','left','right');CORNERS=(0,3,12,15)
LINES=[[[step*4+line if d==0 else (3-step)*4+line if d==1 else line*4+step if d==2 else line*4+3-step for step in range(4)] for line in range(4)] for d in range(4)]
def move(board,d,seed):
    b=list(board)
    for line in LINES[d]:
        values=[board[p] for p in line if board[p]];out=[];j=0
        while j<len(values):
            if j+1<len(values) and values[j]==values[j+1] and values[j]<11:out.append(values[j]+1);j+=2
            else:out.append(values[j]);j+=1
        for p,v in zip(line,out+[0]*4):b[p]=v
    if tuple(b)==board:return None
    while True:
        seed=(seed>>1)^(0xb8 if seed&1 else 0);p=seed&15
        if not b[p]:break
    seed=(seed>>1)^(0xb8 if seed&1 else 0);b[p]=1 if seed&15 else 2
    return tuple(b),seed

def quality(b,corner):
    empty=b.count(0);rough=0;mono=0
    for line in LINES[0]+LINES[2]:
        vals=[b[p] for p in line];rough+=sum(abs(a-c) for a,c in zip(vals,vals[1:]) if a and c)
        mono+=min(sum(max(0,a-c) for a,c in zip(vals,vals[1:])),sum(max(0,c-a) for a,c in zip(vals,vals[1:])))
    return sum(3**p for p in b)+empty*220-rough*14-mono*60+b[corner]*90

def solve(board,seed,goal,corner,maxdepth):
    beam=[(tuple(board),seed,[])];seen={(tuple(board),seed)};normal=None;examined=0
    for depth in range(1,maxdepth+1):
        candidates=[]
        for b,seed,path in beam:
            for d in range(4):
                nxt=move(b,d,seed)
                if nxt is None or nxt in seen:continue
                seen.add(nxt);nb,ns=nxt;route=path+[KEYS[d]];examined+=1
                if max(nb)>=goal:
                    if nb[corner]>=goal:
                        if normal:return normal,route,examined
                    elif normal is None:normal=route
                    continue
                candidates.append((quality(nb,corner),nb,ns,route))
        candidates.sort(reverse=True,key=lambda item:item[0]);beam=[item[1:] for item in candidates[:500]]
        if not beam:break
    return None

def run():
    rng=random.Random(803111);stages=[]
    for i in range(40):
        goal=4+i*7//39;corner=CORNERS[i%4]
        for attempt in range(300):
            pieces=[goal]
            target=4+i*7//39
            while len(pieces)<target:
                options=[j for j,p in enumerate(pieces) if p>1]
                if not options:break
                j=max(options,key=lambda j:(pieces[j]+rng.random()*3));value=pieces.pop(j)-1;pieces.extend([value,value])
            occupied=min(15,len(pieces)+2+i//10)
            values=pieces+[rng.choice([1,1,2]) for _ in range(occupied-len(pieces))]+[0]*(16-occupied)
            rng.shuffle(values);seed=rng.randrange(1,256)
            found=solve(values,seed,goal,corner,55)
            if found is None:continue
            normal,bonus,nodes=found
            if len(normal)<(3,5,8,12)[i//10] or len(bonus)<len(normal):continue
            stages.append({'initial':{'board':values,'seed':seed,'goal':goal},'normal':normal,'bonus':bonus,'bonus_cells':[corner],
                           'par':len(bonus),'metrics':{'goal_value':1<<goal,'starting_tiles':occupied,'normal_reference':len(normal),'bonus_reference':len(bonus),'search_states':nodes}})
            print(i+1,1<<goal,len(normal),len(bonus),flush=True);break
        else:raise RuntimeError(f'No qualified merge challenge {i+1}')
    save(__file__,stages)
if __name__=='__main__':run()
