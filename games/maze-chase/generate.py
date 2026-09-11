# SPDX-License-Identifier: MIT
import sys,json,random,collections
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;levels=[]
for stage in range(12):
 rng=random.Random(29000+stage);board=[1]*105;todo=[(1,1)];board[16]=2
 while todo:
  x,y=todo[-1];choices=[(x+dx,y+dy) for dx,dy in ((0,-2),(0,2),(-2,0),(2,0)) if 1<=x+dx<=13 and 1<=y+dy<=5 and board[(y+dy)*15+x+dx]==1]
  if not choices:todo.pop();continue
  X,Y=rng.choice(choices);board[((y+Y)//2)*15+(x+X)//2]=2;board[Y*15+X]=2;todo.append((X,Y))
 # Open three loops, with vertical and horizontal routes still inside the border.
 walls=[p for p in range(105) if board[p]==1 and 0<p%15<14 and 0<p//15<6 and ((board[p-1]!=1 and board[p+1]!=1) or (board[p-15]!=1 and board[p+15]!=1))]
 rng.shuffle(walls)
 for p in walls[:3]:board[p]=2
 for p in (16,28,76,88,52):board[p]=3
 # All five power cells are original maze junctions.
 assert all(board[p]!=1 for p in (16,28,76,88,52))
 levels.append(board)
sprites=[Bitmap(8,8)]
b=Bitmap(8,8);b.rect(0,0,8,8);b.line(0,0,7,7);sprites.append(b)
b=Bitmap(8,8);b.rect(3,3,2,2,1,True);sprites.append(b)
b=Bitmap(8,8);b.rect(1,1,6,6);b.rect(3,3,2,2,1,True);sprites.append(b)
# Player is pre-inverted because the grid cursor is inverted at draw time.
b=Bitmap(8,8);b.rect(1,1,6,6,1,True);b.dot(3,2,0);b.dot(5,2,0);b.p=[[1-v for v in row] for row in b.p];sprites.append(b)
for frightened in (False,True):
 b=Bitmap(8,8);b.rect(1,1,6,6,1,not frightened);b.dot(2,7);b.dot(5,7)
 if not frightened:b.dot(2,3,0);b.dot(5,3,0)
 else:b.line(2,3,5,3)
 sprites.append(b)
data=asm_bytes('maze_levels',sum(levels,[]))
assets(root,'MAZE CHASE','maze-chase',15,7,1,1,sprites,12,data,aux=('PAUSE','RESET'))
(root/'levels.json').write_text(json.dumps(levels)+'\n')
