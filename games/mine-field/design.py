# SPDX-License-Identifier: MIT
"""Fixed mine boards certified by visible-clue and subset deduction, without guessing."""
import random,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from puzzle_design import save
LINKS=[]
for p in range(98):
    LINKS.append([y*14+x for dy in (-1,0,1) for dx in (-1,0,1) if dx or dy
                  for x,y in [(p%14+dx,p//14+dy)] if 0<=x<14 and 0<=y<7])

def solve(mines,start):
    numbers=[16 if p in mines else sum(q in mines for q in LINKS[p]) for p in range(98)]
    opened=set();flags=set();actions=[];subsets=0;rounds=0
    def reveal(p):
        assert p not in mines
        if p in opened:return
        opened.add(p);todo=[p]
        for q in todo:
            if numbers[q]:continue
            for r in LINKS[q]:
                if r not in opened:assert r not in mines;opened.add(r);todo.append(r)
    reveal(start)
    while len(opened)<98-len(mines):
        rounds+=1;constraints={}
        for p in sorted(opened):
            unknown=frozenset(q for q in LINKS[p] if q not in opened and q not in flags)
            value=numbers[p]-sum(q in flags for q in LINKS[p])
            if unknown:constraints[unknown]=value
            else:assert value==0
        safe=set();bombs=set()
        for cells,n in constraints.items():
            assert 0<=n<=len(cells)
            if not n:safe|=cells
            elif n==len(cells):bombs|=cells
        if not safe and not bombs:
            items=list(constraints.items())
            for a,na in items:
                for b,nb in items:
                    if a<b:
                        cells=b-a;n=nb-na
                        if not n:safe|=cells
                        elif n==len(cells):bombs|=cells
            if safe or bombs:subsets+=1
        if not safe and not bombs:return None
        for p in sorted(bombs-flags):
            assert p in mines;flags.add(p);actions.append(['flag',p])
        # One deduction at a time exposes the next useful clue before more flags are inferred.
        if safe:
            p=min(safe-opened,key=lambda p:(numbers[p]!=0,p));actions.append(['open',p]);reveal(p)
    if flags!=mines:return None
    normal=[a for a in actions if a[0]=='open']
    if not normal:return None
    return numbers,normal,actions,subsets,rounds

def run():
    rng=random.Random(803112);stages=[]
    for i in range(40):
        count=10+i*15//39
        for attempt in range(50000):
            start=rng.randrange(98);excluded=set(LINKS[start])|{start}
            mines=set(rng.sample([p for p in range(98) if p not in excluded],count));found=solve(mines,start)
            if found is None:continue
            board,normal,bonus,subsets,rounds=found
            if len(normal)<(3,8,13,18)[i//10] or (i>=20 and subsets==0):continue
            stages.append({'initial':{'board':board,'start':start,'mines':sorted(mines)},'normal':normal,'bonus':bonus,'bonus_cells':[],
                           'par':len(bonus),'metrics':{'mines':count,'deduction_rounds':rounds,'subset_deductions':subsets,'safe_open_actions':len(normal),'bonus_reference':len(bonus)}})
            print(i+1,count,len(normal),subsets,flush=True);break
        else:raise RuntimeError(f'No logical minefield {i+1}')
    save(__file__,stages)
if __name__=='__main__':run()
