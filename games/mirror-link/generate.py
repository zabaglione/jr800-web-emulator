# SPDX-License-Identifier: MIT
import sys,random,json,re
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from art import title,asm_bytes
out=Path(__file__).parent
levels=[];solutions=[]
for n in range(20):
 rng=random.Random(880+n);board=[1 if p//16 in (0,6) or p%16 in (0,15) else 0 for p in range(112)];paths=set();mirrors=[];sources=[]
 def segment(x,y,X,Y):
  dx=(X>x)-(X<x);dy=(Y>y)-(Y<y)
  while (x,y)!=(X,Y):paths.add(y*16+x);x+=dx;y+=dy
  paths.add(Y*16+X)
 def route(y,Y,x,end):
  sources.append(y*16+1);board[y*16+1]=5;segment(1,y,x,y);segment(x,y,x,Y)
  board[y*16+x]=2 if Y<y else 3;mirrors.append(y*16+x)
  if end is None:board[Y*16+x]=4
  else:
   board[Y*16+x]=2 if Y<y else 3;mirrors.append(Y*16+x)
   segment(x,Y,end,Y);board[Y*16+end]=4
 if n<5:route(4,1,4+n*2,None)
 elif n<10:route(4,1,3+(n-5),12+(n%2))
 else:
  route(2,1,3+n%5,11+n%3);route(4,5,4+n%4,12+n%2)
  if n>=15:board[3*16+1]=5;sources.append(49);board[3*16+13]=4;segment(1,3,13,3)
 for p in range(112):
  if p not in paths and board[p]==0 and rng.random()<.18:board[p]=1
 flips=mirrors if n%3==0 else rng.sample(mirrors,max(1,len(mirrors)-1))
 for p in flips:board[p]^=1
 levels.append([len(sources)]+sources+[0]*(3-len(sources))+board)
 solutions.append(flips)
tiles=[[0]*8,[255,129,129,153,153,129,129,255],[0,64,32,16,8,4,2,0],[0,2,4,8,16,32,64,0],[0,60,66,90,90,66,60,0],
[0,24,24,90,60,24,0,0],[0,8,24,62,62,24,8,0],[0,24,60,90,24,24,0,0],[0,16,24,124,124,24,16,0],
[0,60,126,102,102,126,60,0],[8]*8,[0,0,0,255,0,0,0,0],[8,8,8,255,8,8,8,8]]
s='; SPDX-License-Identifier: MIT\n.equ STAGES,20\n.section .data, data\ngame_name: .byte "MIRROR LINK",0\naux1_label: .byte "UNDO ROTATION",0\naux2_label: .byte "RESET MIRRORS",0\n'+asm_bytes('tiles',sum(tiles,[]))+asm_bytes('title_art',title('MIRROR LINK','mirror-link'))+asm_bytes('levels',sum(levels,[]))
s=re.sub(r'\.byte "([^"\n]*)",0',lambda m:'.byte '+','.join(str(c) for c in m[1].encode())+',0 ; '+m[1],s)
(out/'assets.s').write_text(s);(out/'solutions.json').write_text(json.dumps(solutions)+'\n')
