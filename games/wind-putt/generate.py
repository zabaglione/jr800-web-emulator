# SPDX-License-Identifier: MIT
import sys,json,heapq
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent
vectors=[(0,-256),(181,-181),(256,0),(181,181),(0,256),(-181,181),(-256,0),(-181,-181)]
def blocked(board,x,y):
 return x<1 or x>125 or y<9 or y>61 or any(board[((Y-8)//8)*16+X//8]==1 for X,Y in ((x,y),(x+1,y),(x,y+1),(x+1,y+1)))
def shot(level,start,angle,power):
 x,y=(v*256 for v in start);vx,vy=vectors[angle];vx+=level['wind']*32
 for left in range(power*8,0,-1):
  if left==8:vx//=2;vy//=2
  q=x+vx
  if blocked(level['board'],q//256,y//256):vx=-vx
  else:x=q
  q=y+vy
  if blocked(level['board'],x//256,q//256):vy=-vy
  else:y=q
  if abs(x//256-level['cup'][0])<=2 and abs(y//256-level['cup'][1])<=2:return (x//256,y//256),True
 return (x//256,y//256),False
levels=[];solutions=[]
for stage in range(12):
 board=[int(x in (0,15) or y in (0,6)) for y in range(7) for x in range(16)]
 for x,gap in ((5,1+stage%4),(10,1+(stage*3+2)%4)):
  for y in range(1,6):
   if y not in (gap,gap+1):board[y*16+x]=1
 start=[20,20+8*(stage%4)];cup=[108,20+8*((stage*3+1)%4)];board[(cup[1]-8)//8*16+cup[0]//8]=2
 level={'board':board,'start':start,'cup':cup,'wind':(stage%3)-1};levels.append(level)
 # Reverse pixel distance guides a bounded search, while the exact fixed-point model proves every stroke.
 dist={tuple(cup):0};queue=[tuple(cup)]
 for x,y in queue:
  for q in ((x-1,y),(x+1,y),(x,y-1),(x,y+1)):
   if q not in dist and not blocked(board,*q):dist[q]=dist[x,y]+1;queue.append(q)
 beam=[(tuple(start),[])];seen={tuple(start)};answer=None
 for depth in range(10):
  candidates=[]
  for pos,path in beam:
   for angle in range(8):
    for power in range(1,9):
     end,won=shot(level,pos,angle,power);new=path+[[angle,power]]
     if won:answer=new;break
     if end not in seen:seen.add(end);candidates.append((dist.get(end,9999),end,new))
    if answer:break
   if answer:break
  if answer:break
  beam=[(end,path) for _,end,path in heapq.nsmallest(96,candidates)]
 assert answer,stage
 solutions.append(answer)
 print('Hole',stage+1,'strokes',len(answer),flush=True)
sprites=[Bitmap(8,8),Bitmap(8,8),Bitmap(8,8)]
sprites[0].dot(1,6);sprites[1].rect(0,0,8,8);sprites[1].line(0,3,7,3);sprites[1].line(3,0,3,3);sprites[1].line(5,3,5,7)
sprites[2].rect(2,3,5,4);sprites[2].line(4,0,4,4);sprites[2].line(4,0,7,1)
raw=[]
for level in levels:raw.extend(level['board']+level['start']+level['cup']+[level['wind']&255])
def words(label,values):return asm_bytes(label,[b for v in values for b in ((v&65535)>>8,v&255)])
assets(root,'WIND PUTT','wind-putt',16,7,1,1,sprites,12,asm_bytes('putt_levels',raw)+words('putt_vectors',[v for vec in vectors for v in vec])+words('putt_angles',[n*45 for n in range(8)]),aux=('RETEE','RESET'))
(root/'levels.json').write_text(json.dumps(levels)+'\n');(root/'solutions.json').write_text(json.dumps(solutions)+'\n')
