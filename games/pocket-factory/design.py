# SPDX-License-Identifier: MIT
"""Disjoint production routes, alternating bends and optional processed-item sensors."""
import random,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from puzzle_design import save

def expansions(path,rows,last_column):
    out=[];used=set(path)
    for i,(p,q) in enumerate(zip(path,path[1:])):
        if i<2:continue # Source and first press always have an east-facing output.
        dx,dy=q%16-p%16,q//16-p//16
        for sign in (-1,1):
            pair=[]
            for at in (p,q):
                x=at%16-dy*sign;y=at//16+dx*sign
                if not(1<=x<=last_column and y in rows):break
                pair.append(y*16+x)
            if len(pair)==2 and not used.intersection(pair):out.append((i,pair))
    return out

def plan(path,lane,press):
    build=[]
    for p,q in zip(path[1:-1],path[2:]):
        tile={1:6,16:7,-1:8,-16:9}[q-p]
        if p==press:assert tile==6;tile=10+lane
        build.append([p,tile])
    return build

def run():
    rng=random.Random(803115);stages=[]
    for i in range(40):
        for attempt in range(10000):
            board=[5]*112;normal=[];bonus=[];marks=[];paths=[];all_used=set();valid=True
            tall=i%2;zones=([1,2,3],[4,5]) if tall==0 else ([1,2],[3,4,5])
            width=8+i*6//39
            for lane,rows in enumerate(zones):
                y=rng.choice(rows);path=[y*16+x for x in range(1,width+1)]
                target=width+2*(i//10+(1 if len(rows)==3 and i>=20 else 0))
                while len(path)<target:
                    choices=expansions(path,rows,width)
                    if not choices:break
                    at,pair=rng.choice(choices);path[at+1:at+1]=pair
                if len(path)!=target:valid=False;break
                choices=expansions(path,rows,width)
                if not choices:valid=False;break
                at,pair=rng.choice(choices);extra=path[:at+1]+pair+path[at+1:]
                press=path[1];assert path[2]-press==1
                normal+=plan(path,lane,press);bonus+=plan(extra,lane,press)
                marks.append(pair[rng.randrange(2)]);paths.append({'normal':path,'bonus':extra,'press':press})
                used=set(extra)|set(path);assert not used&all_used;all_used|=used
                for p in used:board[p]=0
                board[path[0]]=lane+1;board[path[-1]]=lane+3
            if not valid:continue
            # Some open decoys and gaps keep the belt direction/processor choice meaningful.
            decoys=[p for p in range(112) if 1<=p//16<=5 and 1<=p%16<=14 and p not in all_used]
            for p in rng.sample(decoys,len(decoys)//(3 if i<20 else 2)):board[p]=0
            if any(board==s['initial']['board'] for s in stages):continue
            stages.append({'initial':{'board':board,'targets':[3+i//15,3+i//20]},'normal':normal,'bonus':bonus,
                           'bonus_cells':marks,'par':len(bonus),'paths':paths,
                           'metrics':{'normal_placements':len(normal),'bonus_placements':len(bonus),'bends':sum(sum(b-a!=c-b for a,b,c in zip(p['bonus'],p['bonus'][1:],p['bonus'][2:])) for p in paths)}})
            print(i+1,len(normal),len(bonus),flush=True);break
        else:raise RuntimeError(f'No factory plan {i+1}')
    save(__file__,stages)
if __name__=='__main__':run()
