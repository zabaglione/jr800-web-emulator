# SPDX-License-Identifier: MIT
"""Original guard puzzles, solved with a bounded independent state search."""
import sys,random,json,re,heapq
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from art import title,asm_bytes
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
def solve(board,initial):
 heap=[(0,0,initial,[])];seen={initial:0};serial=0
 while heap and serial<60000:
  _,cost,s,path=heapq.heappop(heap)
  if not s[2]:return path
  if cost!=seen[s]:continue
  for action in range(6):
   n=advance(board,s,action)
   if n is None or seen.get(n,999)<=cost+1:continue
   seen[n]=cost+1;serial+=1
   dist=min(abs(n[0]%16-g%16)+abs(n[0]//16-g//16) for g in n[2]) if n[2] else 0
   heapq.heappush(heap,(cost+1+dist+len(n[2])*2,cost+1,n,path+[action]))
 return None
out=Path(__file__).parent;levels=[];solutions=[]
for n in range(20):
 rng=random.Random(991+n)
 while True:
  board=[int(p//16 in (0,6) or p%16 in (0,12,13,14,15)) for p in range(112)]
  floor=[p for p in range(112) if not board[p]]
  for p in rng.sample(floor,4+n//4):board[p]=1
  floor=[p for p in floor if not board[p]]
  player=rng.choice(floor);guards=tuple(sorted(rng.sample([p for p in floor if p!=player],2+n//7)))
  path=solve(board,(player,0,guards,(),0))
  if path and 8<=len(path)<=55:break
 levels.append([player,len(guards)]+list(guards)+[255]*(4-len(guards))+board)
 solutions.append([names[a] for a in path])
tiles=[[0]*8,[255,129,189,165,165,189,129,255],[0,24,60,126,90,24,36,0],[0,24,60,126,90,24,36,0],[0,24,60,126,90,24,36,0],[0,24,60,126,90,24,36,0],[0,8,28,8,0,0,0,0],[0,0,0,28,8,0,0,0],[0,60,90,126,24,60,66,0]]
for d in range(4):
 x,y=3,3
 for _ in range(3):
  x+=[1,0,-1,0][d];y+=[0,1,0,-1][d]
  if 0<=x<8 and 0<=y<8:tiles[2+d][x]|=1<<y
s='; SPDX-License-Identifier: MIT\n.equ STAGES,20\n.section .data, data\ngame_name: .byte "STEP STRIKE",0\naux1_label: .byte "WAIT ONE TURN",0\naux2_label: .byte "HELP",0\n'+asm_bytes('tiles',sum(tiles,[]))+asm_bytes('title_art',title('STEP STRIKE','step-strike'))+asm_bytes('levels',sum(levels,[]))
s=re.sub(r'\.byte "([^"\n]*)",0',lambda m:'.byte '+','.join(str(c) for c in m[1].encode())+',0 ; '+m[1],s)
(out/'assets.s').write_text(s);(out/'solutions.json').write_text(json.dumps(solutions)+'\n')
