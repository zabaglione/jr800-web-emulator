# SPDX-License-Identifier: MIT
"""Original 48x24 mechanical bosses; each has its own armor and silhouette."""
from art import Bitmap
from collections import deque
from visual_art import poly,circle
BOSSES=[('SPARK IMP','LET THE CURRENT BITE.'),('WIRE WOLF','I HUNT EVERY SIGNAL.'),('IRON MOTH','YOUR SPARK DRAWS ME IN.'),('COIL CRAB','MY SHELL WILL HOLD.'),('TWIN FANG','ONE STRIKE. TWO FANGS.'),('NOVA EYE','I SEE YOUR NEXT MOVE.'),('GEAR KING','BOW BEFORE THE GEARS.'),('VOID WARD','NO SIGNAL GETS PAST.'),('CORE ZERO','THIS CIRCUIT ENDS HERE.')]
def portrait(i):
 b=Bitmap(48,24)
 def plate(points,c=1,canvas=None):
  canvas=canvas or b
  for y in range(max(0,min(p[1] for p in points)),min(24,max(p[1] for p in points)+1)):
   cuts=[]
   for (x1,y1),(x2,y2) in zip(points,points[1:]+points[:1]):
    if min(y1,y2)<=y+.5<max(y1,y2):cuts.append(x1+(y+.5-y1)*(x2-x1)/(y2-y1))
   cuts.sort()
   for a,z in zip(cuts[::2],cuts[1::2]):
    for x in range(max(0,round(a)),min(48,round(z)+1)):canvas.dot(x,y,c)
  poly(canvas,points,c)
 def vent(x,y,n=3):
  for q in range(n):b.line(x,y+q*2,x+3,y+q*2,0)
 if i==0:
  plate([(8,0),(13,3),(16,8),(23,5),(29,9),(28,19),(23,23),(14,20),(10,12)])
  plate([(2,10),(9,8),(13,13),(10,20),(4,19),(0,15)])
  b.line(9,2,13,9,0);b.line(14,7,22,6,0);b.line(13,10,21,12,0)
  b.rect(15,12,6,2,0,True);b.dot(21,12);b.line(15,17,22,19,0)
  b.rect(18,19,2,3,0,True);vent(4,12);b.rect(7,19,2,3,1,True)
 elif i==1:
  plate([(9,0),(18,5),(23,4),(30,9),(27,18),(23,23),(12,19),(7,10)])
  plate([(1,12),(8,8),(11,14),(8,21),(1,19)])
  b.line(11,3,13,8,0);b.line(13,7,22,8,0);b.line(11,11,18,13,0)
  b.rect(16,11,5,2,0,True);plate([(17,16),(23,13),(27,16),(23,21)],0)
  b.rect(21,16,3,2,1,True);b.rect(15,17,2,3,0,True);vent(2,13)
 elif i==2:
  plate([(0,3),(9,4),(19,10),(17,18),(7,23),(5,15),(0,12)])
  plate([(18,5),(23,2),(29,5),(29,20),(23,23),(18,20)])
  b.line(2,5,16,12,0);b.line(4,8,14,14,0);b.line(7,17,15,16,0)
  b.rect(9,9,3,3,0,True);b.dot(10,10);b.line(20,5,15,0);b.line(19,6,13,2)
  for y in (8,12,16,20):b.line(21,y,24,y,0)
 elif i==3:
  plate([(0,2),(4,0),(5,7),(9,7),(10,1),(13,5),(12,14),(8,17),(2,12)])
  plate([(15,6),(23,4),(32,6),(33,16),(27,21),(15,18),(12,12)])
  b.line(2,4,3,10,0);b.line(3,10,8,14,0);b.rect(8,10,3,3,0,True)
  b.line(17,7,23,6,0);b.rect(17,10,5,2,0,True);vent(18,15,2)
  for y in (18,21):b.line(15,y,9,y-1);b.line(9,y-1,5,y+2);b.dot(4,y+2)
 elif i==4:
  plate([(3,1),(13,3),(18,10),(15,16),(9,14),(7,8),(2,6)])
  plate([(14,13),(18,12),(22,17),(23,20),(30,22),(20,23),(16,19)])
  b.line(5,3,12,5,0);b.rect(10,7,5,2,0,True);b.line(5,7,8,10,0)
  plate([(10,11),(13,10),(12,16)],0);b.line(17,15,21,21,0)
  b.rect(0,17,5,2,1,True);b.line(4,18,10,21);b.line(10,21,18,20)
 elif i==5:
  plate([(2,12),(11,5),(19,1),(27,1),(37,5),(45,12),(36,19),(27,23),(19,23),(11,19)])
  plate([(7,12),(15,7),(23,5),(32,7),(40,12),(32,17),(23,19),(15,17)],0)
  circle(b,23,12,7);b.rect(20,7,4,11,1,True);b.line(20,8,21,11,0)
  for x,y in [(5,9),(11,4),(15,2),(5,15),(11,20),(15,22)]:b.dot(x,y,0)
 elif i==6:
  plate([(6,0),(13,5),(16,0),(20,4),(23,0),(29,4),(34,0),(36,10),(10,10)])
  plate([(10,12),(23,10),(37,12),(35,20),(30,23),(15,23),(9,20)])
  b.line(10,7,23,7,0);b.rect(12,4,2,2,0,True);b.rect(20,5,2,2,0,True)
  b.rect(14,13,7,2,0,True);vent(13,18,2);b.rect(21,17,2,5,0,True)
  b.rect(3,13,4,8,1,True);b.line(3,15,6,15,0);b.line(3,19,6,19,0)
 elif i==7:
  plate([(8,3),(23,0),(38,3),(36,15),(30,21),(23,23),(15,21),(10,15)])
  poly(b,[(12,6),(23,3),(34,6),(32,14),(23,20),(14,14)],0)
  plate([(16,8),(23,6),(30,8),(28,14),(23,18),(18,14)],0)
  b.rect(20,9,4,7,1,True);b.line(21,10,22,12,0)
  for y in (5,10,15):b.rect(1,y,5,3,1,True);b.dot(1,y+1,0);b.line(5,y+1,9,y+1)
 else:
  plate([(9,4),(16,1),(23,0),(31,1),(39,4),(43,12),(39,20),(31,23),(16,23),(9,20),(5,12)])
  poly(b,[(12,5),(23,2),(35,5),(39,12),(35,19),(23,22),(12,19),(8,12)],0)
  plate([(15,7),(23,4),(32,7),(36,12),(32,17),(23,20),(15,17),(11,12)],0)
  plate([(19,8),(23,6),(28,8),(31,12),(28,16),(23,18),(19,16),(16,12)])
  b.line(20,11,23,8,0);b.line(20,13,23,16,0);b.rect(22,10,2,5,0,True)
  for y in (3,9,15,21):
   b.rect(0,y,5,2,1,True);b.line(4,y,8,y+1);b.dot(9,y+1,0)
  for x,y in [(12,4),(9,11),(12,18),(18,2),(18,21)]:b.rect(x,y,2,2,0,True)
 # Construct armor, then illuminate its upper-left faces. The right-facing
 # planes retain solid shadow; sparse intermediate pixels suggest curved metal.
 for y in range(24):
  for x in range(24):b.p[y][47-x]=b.p[y][x]
 original=[row[:] for row in b.p]
 # Keep a solid silhouette separately from the lit material. Enclosed openings
 # belong to the body, so a light-colored panel cannot erase the outer contour.
 outside=set();queue=deque((x,y) for y in range(24) for x in range(48) if (x in (0,47) or y in (0,23)) and not original[y][x])
 while queue:
  x,y=queue.popleft()
  if (x,y) in outside:continue
  outside.add((x,y))
  for X,Y in [(x-1,y),(x+1,y),(x,y-1),(x,y+1)]:
   if 0<=X<48 and 0<=Y<24 and not original[Y][X] and (X,Y) not in outside:queue.append((X,Y))
 silhouette=Bitmap(48,24)
 silhouette.p=[[int((x,y) not in outside) for x in range(48)] for y in range(24)]
 def cut(x,y,w,h):
  b.rect(x,y,w,h,0,True);silhouette.rect(x,y,w,h,0,True)
 for y in range(1,23):
  for x in range(1,47):
   if all(original[Y][X] for X,Y in [(x,y),(x-1,y),(x+1,y),(x,y-1),(x,y+1)]):
    light=x+2*y
    if light<36 and (x+y)%3!=0:b.dot(x,y,0)
    elif light<52 and x%2==0 and y%2==0:b.dot(x,y,0)
 # Each opponent also has a genuinely different contour or mechanism on its
 # two sides: worn horns, a torn wing, offset optics, or an unequal weapon.
 if i==0:
  plate([(14,7),(21,6),(20,9),(15,10)],0)
  cut(33,0,8,3);b.line(37,4,39,7,0)
  b.line(41,18,45,21);b.line(45,21,47,18);b.dot(47,17)
 elif i==1:
  plate([(11,4),(17,7),(13,8)],0)
  cut(34,0,8,3);b.line(30,6,34,12,0)
  b.rect(32,16,2,5,0,True);b.dot(35,20)
 elif i==2:
  plate([(2,6),(9,7),(15,12),(7,10)],0);b.line(6,8,13,11)
  plate([(40,19),(47,15),(47,23),(37,23)],0)
  plate([(40,19),(47,15),(47,23),(37,23)],0,silhouette)
  b.line(34,2,37,0);b.dot(39,2)
 elif i==3:
  plate([(2,3),(3,8),(6,11),(4,12),(1,9)],0)
  cut(40,0,8,4);b.rect(34,8,4,4,1,True);b.dot(35,9,0)
  b.line(31,21,38,23);cut(41,18,7,6)
 elif i==4:
  cut(29,0,19,16)
  plate([(39,1),(44,4),(47,10),(39,13),(34,10),(31,5)])
  b.line(36,4,42,5,0);b.rect(38,7,5,2,0,True)
  plate([(39,10),(42,10),(42,16)],0);b.line(34,11,36,16)
 elif i==5:
  plate([(14,9),(20,6),(29,7),(33,12),(29,17),(20,18),(14,15)],0)
  b.rect(18,8,5,9,1,True);b.rect(18,8,2,3,0,True)
  b.line(32,7,36,10);b.line(33,17,37,14);b.rect(39,10,8,3,1,True)
 elif i==6:
  cut(31,0,8,4);b.line(29,4,33,7,0)
  b.rect(11,12,3,6,0,True);circle(b,31,14,3);b.dot(31,14,0)
  b.line(40,17,46,20);b.line(46,20,43,23)
 elif i==7:
  plate([(12,6),(21,4),(18,7),(14,10)],0)
  cut(37,9,11,9);plate([(36,10),(43,9),(47,12),(47,16),(39,17),(36,14)])
  b.rect(43,12,3,3,0,True);b.line(39,11,43,10,0)
 else:
  plate([(13,6),(20,3),(22,4),(16,8),(14,12),(12,11)],0)
  b.rect(21,8,7,9,0,True);plate([(21,9),(25,10),(26,14),(22,16),(20,13)])
  b.line(21,10,23,11,0);b.dot(21,12,0)
  cut(39,1,9,6);b.line(37,5,44,2);b.rect(43,0,4,3,1,True)
  b.rect(40,16,8,6,1,True);b.line(42,18,47,18,0);b.dot(45,20,0)
 # Add the non-mirrored mechanisms, then restore a continuous black outer rim.
 for y in range(24):
  for x in range(48):
   if b.p[y][x]:silhouette.dot(x,y)
 for y in range(24):
  for x in range(48):
   if silhouette.p[y][x] and any(not (0<=X<48 and 0<=Y<24 and silhouette.p[Y][X]) for X,Y in [(x-1,y),(x+1,y),(x,y-1),(x,y+1)]):b.dot(x,y)
 assert any(b.p[y][x]!=b.p[y][47-x] for y in range(24) for x in range(24))
 # A restrained machinery backdrop sits behind a white separation line. The
 # black contour and all lit body pixels remain intact in the foreground.
 background=Bitmap(48,24)
 background.line(1,0,1,23);background.line(46,0,46,23)
 background.line(3,2,11,2);background.line(11,2,14,5)
 background.rect(3,4,5,3);background.dot(5,5)
 background.line(31,1,35,4);background.line(35,4,44,4)
 for y in (9,14,19):background.line(39,y,46,y);background.line(39,y,36,y+2)
 for x in range(3,45,7):background.line(x,22,min(47,x+3),23)
 for y in range(7,22):
  for x in range(30,46):
   if (x+2*y)%11==0:background.dot(x,y)
 for y in range(24):
  for x in range(48):
   if silhouette.p[y][x]:background.p[y][x]=b.p[y][x]
   elif any(0<=X<48 and 0<=Y<24 and silhouette.p[Y][X] for X in range(x-1,x+2) for Y in range(y-1,y+2)):background.dot(x,y,0)
 return background.bytes()
