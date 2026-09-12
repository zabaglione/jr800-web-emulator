# SPDX-License-Identifier: MIT
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
edges=[(x,y) for y in range(7) for x in range(9) if (x+y)%2]
assert len(edges)==31
edgeid={p:i for i,p in enumerate(edges)};nodes=[];boxes=[];types=[];ids=[]
for y in range(7):
 for x in range(9):
  if not x%2 and not y%2:types.append(0);ids.append(len(nodes));nodes.append((x,y))
  elif (x+y)%2:types.append(1 if x%2 else 2);ids.append(edgeid[x,y])
  else:types.append(3);ids.append(len(boxes));boxes.append((x,y))
boxedges=[[edgeid[x,y-1],edgeid[x,y+1],edgeid[x-1,y],edgeid[x+1,y]] for x,y in boxes]
edgeboxes=[]
for i in range(31):
 adj=[b for b,e in enumerate(boxedges) if i in e];edgeboxes+=adj+[255]*(2-len(adj))
nodelinks=[edgeid.get((x+dx,y+dy),255) for x,y in nodes for dx,dy in ((0,-1),(0,1),(-1,0),(1,0))]
# Each row alternates horizontal and vertical edges. Retain the aimed column
# across vertical moves, choosing the left edge on an equal-distance tie.
targets=[min((abs(X-x),j) for j,(X,Y) in enumerate(edges) if Y==y)[1] for y in range(7) for x in range(9)]
sprites=[]
for n in range(23):
 b=Bitmap(8,8)
 if n<16:
  b.rect(3,3,2,2,1,True)
  for bit,(x,y) in enumerate(((3,0),(3,7),(0,3),(7,3))):
   if n&(1<<bit):b.line(3,3,x,y)
 elif n==16:b.dot(1,3);b.dot(6,3)
 elif n==17:b.line(0,3,7,3)
 elif n==18:b.dot(3,1);b.dot(3,6)
 elif n==19:b.line(3,0,3,7)
 elif n in (21,22):b.rect(1,1,6,6,1,n==22)
 sprites.append(b)
data=asm_bytes('dot_types',types)+asm_bytes('dot_ids',ids)+asm_bytes('dot_cells',[y*9+x for x,y in edges])+asm_bytes('dot_rows',[y for x,y in edges])+asm_bytes('dot_columns',[x for x,y in edges])+asm_bytes('dot_targets',targets)+asm_bytes('dot_node_edges',nodelinks)+asm_bytes('dot_box_edges',sum(boxedges,[]))+asm_bytes('dot_edge_boxes',edgeboxes)
assets(Path(__file__).parent,'DOT CLAIM','dot-claim',9,7,1,1,sprites,3,data,stat='LEFT',aux=('UNDO TURN','RESET'))
