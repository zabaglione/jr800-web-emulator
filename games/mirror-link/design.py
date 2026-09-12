# SPDX-License-Identifier: MIT
"""Search complete mirror orientation graphs, including cumulative optional light marks."""
import random,sys
from pathlib import Path
from collections import deque
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from puzzle_design import save
DELTAS=(1,16,-1,-16)

def trace(board,sources):
    rays=[];lit=set();all_seen=0
    for source in sources:
        p=source;d=0;seen=set();free=[];reflections=0
        while True:
            p+=DELTAS[d]
            if not 0<=p<112 or board[p]==1 or (p,d) in seen:break
            seen.add((p,d));all_seen|=1<<p
            tile=board[p]
            if tile==2:d^=3;reflections+=1
            elif tile==3:d^=1;reflections+=1
            elif tile==4:lit.add(p);break
            elif tile>=5:break
            else:free.append(p)
        rays.append((reflections,free))
    return all_seen,lit,rays

def orientation(board,positions,mask):
    b=board[:]
    for i,p in enumerate(positions):b[p]=2+((mask>>i)&1)
    return b

def useful_sources(board,sources,marks):
    """Keep emitters that can reach a receiver or optional mark in any orientation."""
    positions=[p for p,tile in enumerate(board) if tile in (2,3)]
    objectives=sum(1<<p for p in set(marks)|{p for p,tile in enumerate(board) if tile==4})
    useful=set()
    for mask in range(1<<len(positions)):
        b=orientation(board,positions,mask)
        for source in sources:
            if source not in useful and trace(b,[source])[0]&objectives:useful.add(source)
        if len(useful)==len(sources):break
    return [source for source in sources if source in useful]

def prune_sources(board,sources,marks):
    """Remove purposeless beams while preserving every objective-lighting state."""
    kept=useful_sources(board,sources,marks)
    removed=set(sources)-set(kept)
    clean=board[:]
    for source in removed:clean[source]=0
    if removed:
        positions=[p for p,tile in enumerate(board) if tile in (2,3)]
        objectives=sum(1<<p for p in set(marks)|{p for p,tile in enumerate(board) if tile==4})
        for mask in range(1<<len(positions)):
            before=trace(orientation(board,positions,mask),sources)[0]&objectives
            after=trace(orientation(clean,positions,mask),kept)[0]&objectives
            if before!=after:
                # An emitter also blocks incoming light. Preserve that barrier
                # if an empty cell would open a new route to an objective.
                for source in removed:clean[source]=1
                break
    return clean,kept

def bonus_route(initial,positions,lit,won,marks,limit):
    wanted=(1<<len(marks))-1
    pickup=[sum((1<<i) for i,p in enumerate(marks) if bits>>p&1) for bits in lit]
    start=(initial,pickup[initial]);queue=deque([start]);parents={start:None};depth={start:0}
    while queue:
        state=queue.popleft();mask,got=state
        if won[mask]:
            if got==wanted:
                route=[]
                while parents[state] is not None:state,i=parents[state];route.append(positions[i])
                return list(reversed(route)),len(parents)
            continue
        if depth[state]>=limit:continue
        for i in range(len(positions)):
            m=mask^(1<<i);n=(m,got|pickup[m])
            if n not in parents:parents[n]=(state,i);depth[n]=depth[state]+1;queue.append(n)
    return None,len(parents)

def run():
    rng=random.Random(803106);stages=[]
    for index in range(40):
        columns=3+index//10;required=1 if index<10 else 2;low=2+index//8
        for attempt in range(8000):
            xs=sorted(rng.sample(range(2,14),columns));ys=rng.choice(((1,5),(1,4),(2,5)))
            positions=[y*16+x for x in xs for y in ys]
            middle=[y*16+x for x in xs for y in range(ys[0]+1,ys[1])]
            positions+=rng.sample(middle,min(len(middle),0 if index<10 else 2));positions=sorted(positions)
            source_rows=[ys[0],ys[1],rng.choice([y for y in range(1,6) if y not in ys])]
            sources=[y*16+1 for y in source_rows]
            # A displaced third source makes independent beams interact at different columns.
            if index>=20:sources[2]+=rng.choice(xs[:-1])
            if any(p in positions for p in sources):continue
            board=[int(p//16 in (0,6) or p%16 in (0,15)) for p in range(112)]
            for p in sources:board[p]=5
            candidate=None
            for _ in range(80):
                config=rng.randrange(1<<len(positions));b=orientation(board,positions,config);seen,_,rays=trace(b,sources)
                choices=sorted((r for r in rays if r[0]>=2 and r[1]),reverse=True)
                if len(choices)<required:continue
                goals=[r[1][-1] for r in choices[:required]]
                if len(set(goals))!=required:continue
                trial=b[:]
                for p in goals:trial[p]=4
                if len(trace(trial,sources)[1])==required:candidate=(config,goals,seen);break
            if candidate is None:continue
            _,goals,protected=candidate
            for p in goals:board[p]=4
            for p in range(112):
                if not board[p] and p not in positions and not(protected>>p&1) and rng.random()<.1:board[p]=1
            lights=[];won=[];winning=[]
            for mask in range(1<<len(positions)):
                seen,targets,_=trace(orientation(board,positions,mask),sources);lights.append(seen);won.append(len(targets)==required)
                if won[-1]:winning.append(mask)
            if not winning:continue
            options=[]
            for _ in range(80):
                initial=rng.randrange(1<<len(positions));goal=min(winning,key=lambda v:(v^initial).bit_count())
                distance=(initial^goal).bit_count()
                if low<=distance<=low+4:options.append((distance,initial,goal))
            if not options:continue
            distance,initial,goal=max(options)
            flipped=[i for i in range(len(positions)) if (initial^goal)>>i&1]
            rng.shuffle(flipped);normal=[positions[i] for i in flipped];m=initial;normal_light=lights[m]
            for i in flipped:m^=1<<i;normal_light|=lights[m]
            all_light=0
            for light in lights:all_light|=light
            free=[p for p in range(112) if board[p]==0 and p not in positions and not(normal_light>>p&1) and all_light>>p&1]
            if len(free)<2:continue
            for _ in range(8):
                marks=rng.sample(free,2);bonus,work=bonus_route(initial,positions,lights,won,marks,distance+8)
                if bonus and len(bonus)>=distance+2:break
            else:continue
            b,active_sources=prune_sources(orientation(board,positions,initial),sources,marks)
            stages.append({'initial':{'board':b,'sources':active_sources},'normal':normal,'bonus':bonus,'bonus_cells':marks,
                           'par':len(bonus),'metrics':{'minimum_actions':distance,'bonus_minimum_actions':len(bonus),'mirrors':len(positions),'targets':required,'winning_orientations':len(winning),'bonus_search_states':work}})
            print(f'mirror {index+1:02}: {distance}/{len(bonus)} rotations, {len(positions)} mirrors',flush=True);break
        else:raise RuntimeError(f'No qualified mirror stage {index+1}')
    save(__file__,stages)

if __name__=='__main__':run()
