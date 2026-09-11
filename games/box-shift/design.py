# SPDX-License-Identifier: MIT
"""Original warehouses, reverse-generated then independently solved by move-optimal A*."""
import random,sys,heapq,itertools
from pathlib import Path
from collections import deque
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from puzzle_design import save
D=((-16,'up'),(16,'down'),(-1,'left'),(1,'right'))
KEY=dict((k,d) for d,k in D)

def solve(floor,goals,player,boxes,marks=(),limit=45000):
    goal_dist=[]
    for goal in sorted(goals):
        dist={goal:0};q=deque([goal])
        while q:
            p=q.popleft()
            for d,_ in D:
                n=p-d
                if n in floor and n-d in floor and n not in dist:dist[n]=dist[p]+1;q.append(n)
        goal_dist.append(dist)
    viable=set().union(*(set(d) for d in goal_dist));cache={}
    def lower(boxes,p,mask):
        if boxes not in cache:
            cache[boxes]=min(sum(goal_dist[i].get(b,10000) for i,b in enumerate(order)) for order in itertools.permutations(boxes))
        remaining=[q for i,q in enumerate(marks) if not mask>>i&1]
        def manhattan(a,b):return abs(a%16-b%16)+abs(a//16-b//16)
        bonus=min((sum(manhattan(a,b) for a,b in zip((p,)+order,order)) for order in itertools.permutations(remaining)),default=0)
        return max(cache[boxes],bonus)
    start=(player,tuple(sorted(boxes)),0);costs={start:0};parent={};serial=0
    heap=[(lower(start[1],player,0),0,0,start)];want=(1<<len(marks))-1
    while heap and len(costs)<limit:
        _,cost,_,state=heapq.heappop(heap);p,boxes,mask=state
        if cost!=costs[state]:continue
        if set(boxes)==goals:
            if mask==want:
                chunks=[]
                while state!=start:state,path=parent[state];chunks.append(path)
                return [key for path in reversed(chunks) for key in path],len(costs)
            continue
        # Reachable walking positions also track optional pickups; cursor walking is real cost.
        occupied=set(boxes);walk={(p,mask):[]};q=deque([(p,mask)])
        while q:
            cell,m=q.popleft()
            for d,key in D:
                to=cell+d
                if to not in floor or to in occupied:continue
                M=m
                for i,mark in enumerate(marks):
                    if to==mark:M|=1<<i
                if (to,M) not in walk:walk[to,M]=walk[cell,m]+[key];q.append((to,M))
        for box in boxes:
            for d,key in D:
                dest=box+d;behind=box-d
                if dest not in viable or dest in occupied:continue
                newboxes=tuple(sorted((occupied-{box})|{dest}))
                for m in range(want+1):
                    path=walk.get((behind,m))
                    if path is None:continue
                    M=m
                    for i,mark in enumerate(marks):
                        if box==mark:M|=1<<i
                    n=(box,newboxes,M);c=cost+len(path)+1
                    if c>=costs.get(n,100000):continue
                    h=lower(newboxes,box,M)
                    if h>=10000:continue
                    costs[n]=c;parent[n]=(state,path+[key]);serial+=1
                    heapq.heappush(heap,(c+h,c,serial,n))
    return None,len(costs)

def replay(floor,goals,p,boxes,path,marks=()):
    boxes=set(boxes);visited={p};pushes=0;mask=0
    for i,key in enumerate(path):
        d=KEY[key];q=p+d;assert q in floor
        if q in boxes:
            assert q+d in floor-boxes;boxes.remove(q);boxes.add(q+d);pushes+=1
        p=q;visited.add(p)
        for n,mark in enumerate(marks):
            if p==mark:mask|=1<<n
        assert boxes!=goals or i==len(path)-1
    assert boxes==goals
    return visited,pushes,mask

def room(rng,index):
    floor={y*16+x for y in range(1,6) for x in range(1,10)}
    # Bays, central piers and staggered doors create distinct push-order constraints.
    patterns=[{(4,2),(4,3),(7,3),(7,4)},
              {(5,y) for y in (1,2,4,5)},
              {(3,2),(3,3),(6,3),(6,4)},
              {(x,3) for x in (2,3,6,7,8)},
              {(3,2),(7,2),(3,4),(7,4)}]
    floor-={y*16+x for x,y in patterns[index%len(patterns)]}
    for p in rng.sample(sorted(floor),rng.randint(2,6)):floor.remove(p)
    parts=[];unseen=set(floor)
    while unseen:
        part={unseen.pop()};todo=list(part)
        for p in todo:
            for d,_ in D:
                if p+d in unseen:unseen.remove(p+d);part.add(p+d);todo.append(p+d)
        parts.append(part)
    return max(parts,key=len)

def run():
    rng=random.Random(803105);stages=[];seen=set()
    for i in range(40):
        count=2 if i<12 else 3 if i<32 else 4
        low=8+i
        for attempt in range(12000):
            floor=room(rng,i+attempt%5)
            if len(floor)<29:continue
            goals=set(rng.sample(sorted(floor),count));boxes=set(goals);player=rng.choice(sorted(floor-boxes))
            for _ in range(220+i*9):
                options=[d for d,_ in D if player+d in floor-boxes]
                if not options:break
                d=rng.choice([d for d in options for _ in range(4 if player-d in boxes else 1)])
                if player-d in boxes:boxes.remove(player-d);boxes.add(player)
                player+=d
            if len(boxes-goals)<2:continue
            normal,work=solve(floor,goals,player,boxes,limit=18000)
            if not normal or not low<=len(normal)<=140:continue
            visited,pushes,_=replay(floor,goals,player,boxes,normal)
            if pushes<3+i//4:continue
            signature=(tuple(sorted(floor)),tuple(sorted(goals)),player,tuple(sorted(boxes)))
            if signature in seen:continue
            free=sorted(floor-set(boxes)-goals-visited)
            if len(free)<2:continue
            for _ in range(6):
                marks=rng.sample(free,2);bonus,bonus_work=solve(floor,goals,player,boxes,marks,limit=35000)
                if bonus and len(normal)+2<=len(bonus)<=180:break
            else:continue
            _,bonus_pushes,mask=replay(floor,goals,player,boxes,bonus,marks);assert mask==3
            board=[1 if p not in floor else (2 if p in goals else 0) for p in range(112)]
            for p in boxes:board[p]|=4
            stages.append({'initial':{'start':player,'board':board},'normal':normal,'bonus':bonus,
                           'bonus_cells':marks,'par':len(bonus),'metrics':{'minimum_actions':len(normal),'bonus_minimum_actions':len(bonus),'boxes':count,'solution_pushes':pushes,'bonus_pushes':bonus_pushes,'search_states':work,'bonus_search_states':bonus_work}})
            seen.add(signature);print(f'box {i+1:02}: {len(normal)}/{len(bonus)} actions, {pushes} pushes',flush=True);break
        else:raise RuntimeError(f'No qualified warehouse {i+1}')
    save(__file__,stages)

if __name__=='__main__':run()
