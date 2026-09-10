# SPDX-License-Identifier: MIT
from pathlib import Path
import sys,json,random
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from art import title,asm_bytes,Bitmap
out=Path(__file__).parent;levels=[];solutions=[]
for n in range(12):
 board=[5 if p//16 in (0,6) or p%16 in (0,12,13,14,15) else 0 for p in range(112)]
 routes=[];build=[]
 for lane in range(2):
  y0=(1 if n<6 else 2)+lane*3;y1=(2 if n<6 else 1)+lane*3;turn=3+(n+lane*3)%6
  route=[y0*16+x for x in range(1,turn+1)]+[y1*16+x for x in range(turn,12)]
  routes+=route;board[route[0]]=1+lane;board[route[-1]]=3+lane
  for i,p in enumerate(route[1:-1],1):
   delta=route[i+1]-p;tile={1:6,16:7,-1:8,-16:9}[delta]
   if p==y1*16+turn+1:tile=10+lane
   build.append([p,tile])
 rng=random.Random(800+n)
 for p in rng.sample([p for p in range(112) if board[p]==0 and p not in routes],5+n//2):board[p]=5
 levels.append([3+n//3,2+n//2]+board);solutions.append(build)
base=[[0]*8,[255,129,153,165,165,153,129,255],[255,129,189,165,165,189,129,255],[255,129,145,169,169,145,129,255],[255,129,185,169,169,185,129,255],[255,129,189,165,165,189,129,255]]
for d in range(4):
 b=Bitmap(8,8);b.rect(0,0,8,8)
 if d==0:b.line(2,3,5,3);b.line(3,1,5,3);b.line(3,5,5,3)
 if d==1:b.line(3,2,3,5);b.line(1,3,3,5);b.line(5,3,3,5)
 if d==2:b.line(2,3,5,3);b.line(2,3,4,1);b.line(2,3,4,5)
 if d==3:b.line(3,2,3,5);b.line(1,4,3,2);b.line(5,4,3,2)
 base.append(b.bytes())
base += [[255,129,219,165,165,219,129,255],[255,129,189,195,195,189,129,255]]
tiles=[]
for item in range(5):
 for tile in base:
  a=tile.copy()
  if item:
   for x in range(2,6):a[x]&=~60
   icon=[[0,24,24,0],[24,36,36,24],[60,60,60,60],[60,36,36,60]][item-1]
   for x,v in enumerate(icon,2):a[x]|=v
  tiles+=a
strings={'game_name':'POCKET FACTORY','aux1_label':'SELECT TOOL','aux2_label':'RUN / PAUSE','factory_heading':'FACTORY','build_label':'BUILD   ','run_label':'RUNNING ','tool_label':'TOOL    ','choose_label':'CHOOSE  ','shipment_label':'SHIP/GOAL','factory_return':'RETURN'}
s='; SPDX-License-Identifier: MIT\n.equ STAGES,12\n.section .data, data\n'
for k,v in strings.items():s+=asm_bytes(k,list(v.encode())+[0])
s+=asm_bytes('tool_names',sum([list(x.ljust(8).encode())+[0] for x in ['BELT >','BELT V','BELT <','BELT ^','PRESS A','PRESS B','ERASE']],[]))
s+=asm_bytes('flow_deltas',[0,1,1,0,0,0,1,16,255,240,1,1])+asm_bytes('tiles',tiles)+asm_bytes('title_art',title('POCKET FACTORY','pocket-factory'))+asm_bytes('levels',sum(levels,[]))
(out/'assets.s').write_text(s);(out/'solutions.json').write_text(json.dumps(solutions)+'\n')
