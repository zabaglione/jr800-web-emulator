# SPDX-License-Identifier: MIT
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
links=[]
for p in range(98):
 for dy in (-1,0,1):
  for dx in (-1,0,1):
   if not(dx or dy):continue
   x,y=p%14+dx,p//14+dy;links.append(y*14+x if 0<=x<14 and 0<=y<7 else 255)
digits=[[7,5,5,5,7],[2,6,2,2,7],[7,1,7,4,7],[7,1,7,1,7],[5,5,7,1,1],[7,4,7,1,7],[7,4,7,5,7],[7,1,2,2,2],[7,5,7,5,7]]
sprites=[]
for n in range(15):
 b=Bitmap(8,8)
 if n<=8:
  b.dot(7,7)
  if n:
   for y,row in enumerate(digits[n]):
    for x in range(3):
     if row&(4>>x):b.dot(2+x,1+y)
 elif n==10:
  b.rect(0,0,7,7);b.dot(2,2);b.dot(4,4)
 elif n==11:
  b.line(2,1,2,6);b.line(0,6,5,6);b.line(2,1,5,2);b.line(5,2,2,3)
 elif n in (12,13):
  for dx,dy in ((3,0),(0,3),(2,2),(2,-2)):b.line(3-dx,3-dy,3+dx,3+dy)
  b.rect(2,2,3,3,1,True)
  if n==13:b.rect(0,0,8,8)
 elif n==14:b.line(0,0,6,6);b.line(6,0,0,6)
 sprites.append(b)
assets(Path(__file__).parent,'MINE FIELD','mine-field',14,7,1,1,sprites,3,asm_bytes('mine_links',links)+asm_bytes('mine_totals',[10,15,20]),stat='LEFT',aux=('FLAG MODE','RESET'))
