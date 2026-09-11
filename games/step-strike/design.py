# SPDX-License-Identifier: MIT
"""Structured tactical rooms; independent state search verifies surviving routes."""
import sys,random,heapq,itertools
from collections import deque
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from puzzle_design import save
D=[1,16,-1,-16];names=['right','down','left','up','fire','wait']
def advance(board,state,action):
 p,face,guards,bullets,t=state;guards=list(guards);bullets=list(bullets)
 if action<4:
  q=p+D[action]
  if not 0<=q<112 or board[q]:return None
  p=q;face=action
  guards=[g for g in guards if g!=p]
 elif action==4:
  q=p
  for _ in range(4):
   q+=D[face]
   if not 0<=q<112 or board[q]:break
   if q in guards:guards.remove(q);break
 if any(q==p for q,d in bullets):return None
 after=[]
 for q,d in bullets:
  q+=D[d]
  if not 0<=q<112 or board[q]:continue
  if q==p:return None
  after.append((q,d))
 t=(t+1)%3
 if t==0:
  for g in guards:
   if p//16==g//16:d=0 if p>g else 2
   elif p%16==g%16:d=1 if p>g else 3
   else:continue
   q=g+D[d]
   while q!=p and 0<=q<112 and not board[q]:q+=D[d]
   if q==p and len(after)<8:after.append((g,d))
 return (p,face,tuple(guards),tuple(sorted(after)),t)

def solve(board,initial,marks=()):
    initial=(*initial,0);serial=itertools.count();heap=[(0,next(serial),0,initial,[])];seen={initial:0};need=(1<<len(marks))-1;work=0
    while heap and work<70000:
        _,_,cost,state,path=heapq.heappop(heap)
        if seen.get(state)!=cost:continue
        if not state[2]:
            if state[-1]==need:return path,work
            continue
        if cost>=90:continue
        work+=1
        for action in range(6):
            nxt=advance(board,state[:5],action)
            if nxt is None:continue
            bits=state[-1]
            for i,p in enumerate(marks):
                if nxt[0]==p:bits|=1<<i
            n=(*nxt,bits)
            if seen.get(n,999)<=cost+1:continue
            if not n[2] and bits!=need:continue
            seen[n]=cost+1
            targets=list(n[2])+[p for i,p in enumerate(marks) if not bits>>i&1]
            distance=min((abs(n[0]%16-p%16)+abs(n[0]//16-p//16) for p in targets),default=0)
            h=distance+len(n[2])*2+sum(not bits>>i&1 for i in range(len(marks)))*5
            heapq.heappush(heap,(cost+1+h*1.4,next(serial),cost+1,n,path+[action]))
    return None

def run():
    rng=random.Random(803114);stages=[]
    for i in range(40):
        for attempt in range(10000):
            board=[int(p//16 in (0,6) or p%16 in (0,15)) for p in range(112)]
            # Offset doors and partial cross walls produce cover, funnels and crossfire.
            for x in ([5] if i<10 else [5,10]):
                doors=rng.sample(range(1,6),2 if i<25 else 1)
                for y in range(1,6):
                    if y not in doors:board[y*16+x]=1
            if i>=10:
                for x in rng.sample([x for x in range(2,14) if x not in (5,10)],2+i//15):board[48+x]=1
            floors=[p for p in range(112) if not board[p]];player=rng.choice(floors)
            # A covered starting cell permits a safe three-turn wait cycle and explains the clock.
            candidates=[]
            for g in floors:
                if g==player:continue
                if g//16==player//16:d=1 if player>g else -1
                elif g%16==player%16:d=16 if player>g else -16
                else:candidates.append(g);continue
                q=g+d
                while q!=player and not board[q]:q+=d
                if q!=player:candidates.append(g)
            if len(candidates)<4:continue
            guards=tuple(sorted(rng.sample(candidates,2+i//15)));initial=(player,0,guards,(),0)
            found=solve(board,initial)
            if found is None:continue
            normal,work=found
            if len(normal)<(8,12,16,20)[i//10]:continue
            at=initial;visited={player}
            for action in normal:at=advance(board,at,action);visited.add(at[0])
            extras=[p for p in floors if p not in visited and p not in guards]
            if len(extras)<2:continue
            marks=rng.sample(extras,2);found=solve(board,initial,marks)
            if found is None:continue
            bonus,more=found
            if len(bonus)<len(normal)+3 or (i>=20 and 4 not in bonus):continue
            # Bounded weighted search supplies reproducible reference pars, not optimality claims.
            stages.append({'initial':{'board':board,'start':player,'guards':list(guards)},'normal':[names[a] for a in normal],
                           'bonus':[names[a] for a in bonus],'overpar':['wait']*3+[names[a] for a in bonus],
                           'bonus_cells':marks,'par':len(bonus),'metrics':{'guards':len(guards),'normal_reference':len(normal),'bonus_reference':len(bonus),'search_states':work+more,'shots':bonus.count(4),'waits':bonus.count(5)}})
            print(i+1,len(normal),len(bonus),bonus.count(4),flush=True);break
        else:raise RuntimeError(f'No tactical room {i+1}')
    save(__file__,stages)
if __name__=='__main__':run()
