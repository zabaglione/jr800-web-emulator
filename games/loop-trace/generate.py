# SPDX-License-Identifier: MIT
import sys,json,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent
from puzzle_assets import campaign, level_records, challenge_data
levels=campaign(root)
sprites=[]
for n in range(22):
 b=Bitmap(16,8)
 if n==0:
  for y in range(0,8,2):
   for x in range(y%4,16,4):b.dot(x,y)
 elif n==1:b.rect(6,2,4,4)
 elif n<=5:b.text({2:'A',3:'B',4:'C',5:'S'}[n],5,0)
 else:
  mask=n-6;b.rect(6,2,4,4,1,True)
  for bit,x,y in ((1,7,0),(2,7,7),(4,0,3),(8,15,3)):
   if mask&bit:b.line(7,3,x,y)
 sprites.append(b)
assets(root,'LOOP TRACE','loop-trace',6,6,2,1,sprites,40,level_records([s['initial']['board'] for s in levels])+challenge_data(root,levels,6,2,1,6),stat='LEFT',action='SPACE',aux=('UNDO','RESET'))
