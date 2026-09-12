# SPDX-License-Identifier: MIT
import sys,json,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;levels=[]
for stage in range(3):
 r=random.Random(37000+stage);levels.append([([255,*r.sample(range(9),2)] if n%5==4 else [*r.sample(range(9),2),255]) for n in range(20)])
# A wooden shooting gallery with recessed, hinged enamel target plates.
def panel(kind,width=26):
 b=Bitmap(32,16)
 # Grain, scarf joints, and the two dark rails continue between panels.
 for x in range(32):
  b.dot(x,0);b.dot(x,15)
 for x,y in [(1,3),(30,11),(2,10),(29,4)]:b.line(x,y,x+1,y)
 b.line(0,7,31,7)
 left=(32-width)//2;right=left+width-1
 b.rect(left,1,width,14,1,True)
 if width>3:
  b.rect(left+1,2,width-2,11,0,True)
  b.line(left+2,13,right-1,13)
  b.dot(left+1,2);b.dot(right-1,2)
  b.line(left-2,6,left-1,8);b.line(right+1,6,right+2,8)
  # Render the face at full size first; the foreshortened face is a sampled
  # projection of the same original artwork, not a different symbol.
  face=Bitmap(24,16)
  if kind==0:
   for y in (4,8,11):face.line(3,y,20,y)
   face.line(6,3,6,12);face.line(17,3,17,12)
  elif kind==1:
   for y in range(-5,6):
    for x in range(-5,6):
     if 15<=x*x+y*y<=28 or x*x+y*y<=2:face.dot(12+x,7+y)
  elif kind==2:
   for y in range(3,12):
    for d in (0,1):face.dot(7+y-3+d,y);face.dot(17-y+3-d,y)
  elif kind==3:
   for y in range(-5,6):
    for x in range(-5,6):
     if 14<=x*x+y*y<=29:face.dot(12+x,7+y)
   face.dot(3,5);face.dot(21,9)
  elif kind==4:
   for d in (0,1):face.line(7+d,3,16+d,11);face.line(7+d,11,16+d,3)
  raw=face.bytes()
  for y in range(3,12):
   for x in range(width-4):
    source=x*24//(width-4)
    if raw[(y//8)*24+source]>>(y%8)&1:b.dot(left+2+x,y)
 return b
s=[panel(k) for k in range(5)]+[panel(0,14),panel(0,2)]+[panel(k,14) for k in range(1,5)]
assets(root,'TARGET RANGE','target-range',3,3,4,2,s,3,asm_bytes('range_levels',[v for stage in levels for row in stage for v in row]),aux=('RETRY','RESET'))
# Continuous timber uprights and a sign below the target bank.
from visual_art import tiny
b=Bitmap(128,56)
for base in (0,112):
 b.rect(base,0,16,56,1,True)
 for yy in range(48):
  for x in (3,7,11):
   q=x+(1 if (yy//4)%4==1 else 0)-(1 if (yy//4)%4==3 else 0)
   b.dot(base+q,yy,0)
 for yy in (3,43):
  b.rect(base+5,yy,6,5,0,True);b.rect(base+6,yy+1,4,3);b.line(base+7,yy+2,base+8,yy+2)
b.rect(0,48,128,8,1,True)
tiny(b,'SHOOTING GALLERY',35,50,0)
for x in (3,107):b.line(x,51,x+18,51,0);b.line(x+4,54,x+12,54,0)
raw=b.bytes();patterns={};lookup=[];first=1+len(s)*8
for row in range(7):
 for col in range(16):
  if row<6 and 2<=col<14:lookup.append(0);continue
  tile=tuple(raw[row*128+col*8:row*128+col*8+8])
  if tile not in patterns:patterns[tile]=first+len(patterns)
  lookup.append(patterns[tile])
assert first+len(patterns)<=128
p=root/'assets.s';source=p.read_text();at=source.index('view_cells:');source=source[:at]+asm_bytes('range_wood_tiles',sum((list(t) for t in patterns),[]))+source[at:]+asm_bytes('range_wood_lookup',lookup);p.write_text(source)
(root/'levels.json').write_text(json.dumps(levels)+'\n')
