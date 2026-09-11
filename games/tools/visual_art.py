# SPDX-License-Identifier: MIT
"""Original, individually composed 192x64 one-bit title artwork."""
import math
from art import Bitmap, FONT

TINY={
'A':['010','101','111','101','101'],'B':['110','101','110','101','110'],'C':['011','100','100','100','011'],
'D':['110','101','101','101','110'],'E':['111','100','110','100','111'],'F':['111','100','110','100','100'],
'G':['011','100','101','101','011'],'H':['101','101','111','101','101'],'I':['111','010','010','010','111'],
'J':['001','001','001','101','010'],'K':['101','101','110','101','101'],'L':['100','100','100','100','111'],
'M':['101','111','111','101','101'],'N':['101','111','111','111','101'],'O':['010','101','101','101','010'],
'P':['110','101','110','100','100'],'Q':['010','101','101','111','011'],'R':['110','101','110','101','101'],
'S':['011','100','010','001','110'],'T':['111','010','010','010','010'],'U':['101','101','101','101','111'],
'V':['101','101','101','101','010'],'W':['101','101','111','111','101'],'X':['101','101','010','101','101'],
'Y':['101','101','010','010','010'],'Z':['111','001','010','100','111'],
'0':['111','101','101','101','111'],'1':['010','110','010','010','111'],'2':['110','001','010','100','111'],
'3':['110','001','010','001','110'],'4':['101','101','111','001','001'],'5':['111','100','110','001','110'],
'6':['011','100','111','101','111'],'7':['111','001','010','010','010'],'8':['111','101','111','101','111'],
'9':['111','101','111','001','110'],'-':['000','000','111','000','000'],'/':['001','001','010','100','100'],
':':['000','010','000','010','000'],'.':['000','000','000','000','010'],'+':['000','010','111','010','000'],
'<':['001','010','100','010','001'],'>':['100','010','001','010','100'],' ':['000']*5,
'?':['110','001','010','000','010'],'!':['010','010','010','000','010']}

def tiny(b,text,x,y,c=1):
    for char in text:
        for yy,row in enumerate(TINY.get(char,TINY['?'])):
            for xx,v in enumerate(row):
                if v=='1':b.dot(x+xx,y+yy,c)
        x+=4

def circle(b,x,y,r,c=1,fill=False):
    for yy in range(-r,r+1):
        for xx in range(-r,r+1):
            q=xx*xx+yy*yy
            if q<=r*r and (fill or q>=(r-1)*(r-1)):b.dot(x+xx,y+yy,c)

def poly(b,pts,c=1,fill=False):
    if fill:
        for y in range(max(0,min(p[1] for p in pts)),min(b.h,max(p[1] for p in pts)+1)):
            cuts=[]
            for (x1,y1),(x2,y2) in zip(pts,pts[1:]+pts[:1]):
                if min(y1,y2)<=y<max(y1,y2):cuts.append(x1+(y-y1)*(x2-x1)/(y2-y1))
            cuts.sort()
            for x1,x2 in zip(cuts[::2],cuts[1::2]):b.line(math.ceil(x1),y,math.floor(x2),y,c)
    for p,q in zip(pts,pts[1:]+pts[:1]):b.line(*p,*q,c)

def hatch(b,x,y,w,h,step=4,c=1):
    for yy in range(y,y+h):
        for xx in range(x,x+w):
            if (xx+yy)%step==0:b.dot(xx,yy,c)

def width(text,sx=2,font='block',gap=1):
    n=3 if font=='condensed' else 5
    return sum((3 if ch==' ' else n)*sx+gap for ch in text)-gap

def word(b,text,x,y,sx=2,sy=3,font='block',ink=1,gap=1):
    """Authoring-time rasterization: no font scaling is done on the JR-800."""
    mask=set(); start=x
    for k,ch in enumerate(text):
        if ch==' ':x+=3*sx+gap;continue
        cols=3 if font=='condensed' else 5
        rows=5 if font=='condensed' else 7
        for a in range(cols):
            for r in range(rows):
                on=TINY.get(ch,TINY['?'])[r][a]=='1' if font=='condensed' else bool(FONT[ord(ch)-32][a]>>r&1)
                if not on:continue
                for yy in range(sy):
                    for xx in range(sx):
                        skew=((rows*sy-1-r*sy-yy)//5 if font in ('sport','slant') else 0)
                        jitter=((k+r)%3==0) if font=='brush' else 0
                        mask.add((x+a*sx+xx+skew+jitter,y+r*sy+yy))
        x+=cols*sx+gap
    if font=='serif':
        for xx,yy in list(mask):
            if yy in (y,y+7*sy-1):mask.update(((xx-1,yy),(xx+1,yy)))
    if font in ('outline','neon','wire'):
        mask={p for p in mask if any((p[0]+dx,p[1]+dy) not in mask for dx,dy in ((1,0),(-1,0),(0,1),(0,-1)))}
    if font=='stencil':mask={p for p in mask if p[1]!=y+sy*3}
    if font=='dots':mask={p for p in mask if (p[0]-start)%sx!=sx-1 and (p[1]-y)%sy!=sy-1}
    if font=='brush':mask={p for p in mask if (p[0]*19+p[1]*7)%47!=0}
    if font=='sport':
        for xx,yy in mask:b.dot(xx+1,yy+2,ink)
    for xx,yy in mask:b.dot(xx,yy,ink)

def label(b,text,x,y,w=None,ink=1):
    w=w or 4*len(text)+6;b.rect(x,y,w,9,ink,True);tiny(b,text,x+3,y+2,1-ink)

def crate(b,x,y,s=24,c=1):
    d=4;front=s-d
    poly(b,[(x,y+d),(x+d,y),(x+s-1,y),(x+s-1,y+front-1),(x+front-1,y+s-1),(x,y+s-1)],c,True)
    b.rect(x+1,y+d+1,front-2,front-2,1-c,True)
    poly(b,[(x+2,y+d-1),(x+d+1,y+1),(x+s-3,y+1),(x+front-2,y+d-1)],1-c,True)
    b.line(x+3,y+d+3,x+front-4,y+s-4,c);b.line(x+front-4,y+d+3,x+3,y+s-4,c)
    b.line(x+front,y+d+1,x+s-2,y+2,1-c);b.line(x+front+1,y+d+4,x+front+1,y+s-7,1-c)

def star(b,x,y,r=3,c=1):
    b.line(x-r,y,x+r,y,c);b.line(x,y-r,x,y+r,c);b.dot(x-1,y-1,c);b.dot(x+1,y+1,c)

def person(b,x,y,c=1,pose=0):
    circle(b,x,y,3,c,True);b.rect(x-2,y+5,5,10,c,True)
    b.line(x,y+14,x-5,y+22,c);b.line(x,y+14,x+6,y+21,c)
    b.line(x,y+7,x+9,y+3+pose,c);b.line(x,y+7,x-7,y+11,c)

def card(b,x,y,w=22,h=31,mark='A',c=1):
    b.rect(x+2,y+2,w-2,h-2,c,True);b.rect(x,y,w-2,h-2,c,True)
    b.rect(x+1,y+1,w-4,h-4,1-c,True);tiny(b,mark,x+3,y+3,c)
    poly(b,[(x+(w-2)//2,y+10),(x+w-5,y+16),(x+(w-2)//2,y+22),(x+4,y+16)],c,True)

def mountains(b,y=40,ink=1):
    poly(b,[(0,63),(0,y+9),(20,y-9),(36,y+3),(64,y-18),(92,y+8),(122,y-6),(150,y+5),(174,y-12),(191,y+6),(191,63)],ink,True)

def title(name,kind):
    b=Bitmap()
    if kind=='box-shift':
        # A loading dock: asymmetric timber illustration and a shipping stamp.
        hatch(b,0,0,59,64,5);crate(b,3,30,28);crate(b,27,9,27);crate(b,31,38,24)
        b.line(58,0,58,63);word(b,'BOX',72,4,3,3,'stencil');word(b,'SHIFT',68,29,3,3,'stencil')
        tiny(b,'40 ROOMS',64,56);label(b,'SPACE TO START',103,55,85)
    elif kind=='mirror-link':
        b.rect(0,0,192,64,1,True)
        for pts in [[(0,48),(22,48),(63,7),(177,7),(177,48),(191,48)],[(0,13),(15,13),(57,55),(163,55),(191,27)]]:
            for p,q in zip(pts,pts[1:]):b.line(*p,*q,0)
        for x,y in [(22,48),(63,7),(177,7),(177,48)]:b.line(x-4,y+4,x+4,y-4,0)
        b.rect(20,15,96,16,1,True);word(b,'MIRROR',21,16,2,2,'neon',0,3);word(b,'LINK',92,33,2,2,'neon',0,4)
        tiny(b,'SPACE TO START',70,57,0)
    elif kind=='step-strike':
        poly(b,[(0,0),(151,0),(115,55),(0,55)],1,True);person(b,164,29,1,0)
        for y in (12,22,38):b.line(130,y,187,y-7)
        word(b,'STEP',9,4,3,3,'sport',0);word(b,'STRIKE',7,30,2,3,'slant',0)
        tiny(b,'TIME MOVES WHEN YOU DO',4,57);tiny(b,'SPACE TO START',134,57)
    elif kind=='circuit-deck':
        for x,y in [(4,30),(24,22),(145,25),(165,33)]:card(b,x,y,23,29,'3')
        for y in (4,8,52):
            b.line(0,y,191,y)
            for x in (9,39,152,181):circle(b,x,y,2)
        b.rect(37,10,118,43,1,True);word(b,'CIRCUIT',40,12,2,2,'wire',0);word(b,'DECK',48,29,4,3,'stencil',0)
        label(b,'SPACE TO START',66,55,60)
    elif kind=='pocket-factory':
        for x,h in ((0,24),(17,37),(40,17),(153,34),(178,46)):
            poly(b,[(x,56-h),(x+3,53-h),(x+11,53-h),(x+11,50),(x+8,53),(x,53)],1,True)
            b.line(x+8,56-h,x+8,50,0);b.line(x+1,56-h,x+8,56-h,0)
            b.rect(x+2,58-h,3,4,0,True);b.rect(x+2,50,3,2,0,True)
        for x in range(2,192,9):circle(b,x,52,3)
        for x,y in ((181,3),(160,10),(21,6)):circle(b,x,y,4)
        b.rect(30,12,128,35,0,True);word(b,'POCKET',45,11,2,2,'block',1,3);word(b,'FACTORY',35,28,3,2,'stencil')
        tiny(b,'SPACE TO START',69,58)
    elif kind=='arc-duel':
        mountains(b,49);word(b,'ARC DUEL',8,3,3,3,'sport');b.line(5,27,187,27)
        for x in range(11,185,3):b.dot(x,30+((x-96)**2//360))
        for x in (7,162):b.rect(x,49,22,8,0,True);b.line(x+8,49,x+16,42,0)
        label(b,'SPACE TO START',65,55,62,0)
    elif kind=='lamp-grid':
        # Bauhaus bulbs / alternating solid and outline letter blocks.
        for x,y,r in ((20,18,15),(177,45,15),(25,53,6)):
            circle(b,x,y,r,1,True);b.rect(x-6,y+r-2,12,5);b.line(x-4,y-6,x+4,y+6,0)
        word(b,'LAMP',53,4,4,3,'outline');word(b,'GRID',53,29,4,3,'block')
        label(b,'SPACE TO START',51,54,92,0)
    elif kind=='slide-nine':
        for x,y,n in ((5,3,'1'),(34,8,'2'),(10,33,'3')):
            b.rect(x+2,y+2,24,24,1,True);b.rect(x,y,24,24,1,True);b.rect(x+1,y+1,22,22,0,True)
            word(b,n,x+6,y+2,3,3,'block')
        word(b,'SLIDE',77,5,3,3,'block');word(b,'NINE',90,31,3,3,'outline')
        b.line(65,8,65,53);poly(b,[(62,49),(65,54),(68,49)],1,True);tiny(b,'SPACE TO START',93,57)
    elif kind=='ice-route':
        mountains(b,64)
        for x,y,r in ((14,17,11),(170,18,14),(62,49,5)):
            for a in range(0,180,60):
                dx=int(math.cos(math.radians(a))*r);dy=int(math.sin(math.radians(a))*r);b.line(x-dx,y-dy,x+dx,y+dy)
        word(b,'ICE',36,2,4,3,'outline');word(b,'ROUTE',46,27,3,3,'slant')
        label(b,'SPACE TO START',92,55,99,0)
    elif kind=='switch-maze':
        for x,y,w,h in ((0,0,192,64),(3,3,47,49),(8,8,37,24),(13,13,27,14)):
            b.rect(x,y,w,h)
        b.rect(0,36,43,13,0,True);b.line(10,39,24,28);circle(b,10,39,4,1,True)
        word(b,'SWITCH',58,6,3,2,'stencil');word(b,'MAZE',64,28,4,3,'outline')
        tiny(b,'SPACE TO START',65,55)
    elif kind=='line-four':
        b.rect(0,0,72,64,1,True)
        b.line(69,0,69,60,0);b.line(0,61,68,61,0)
        for x in (10,27,44,61):
            for y in (10,27,44):circle(b,x,y,6,0,True)
        for x,y in ((10,44),(27,27),(44,10)):circle(b,x,y,4)
        word(b,'LINE',83,2,3,3,'block');word(b,'FOUR',83,27,3,3,'outline')
        tiny(b,'SPACE TO START',87,57)
    elif kind=='reversi-mini':
        # Restrained board-game box, white space and large offset disks.
        circle(b,158,27,29,1,True);circle(b,158,24,27,0);circle(b,142,44,19,1,True)
        circle(b,140,41,19,0,True);circle(b,140,41,19)
        word(b,'REVERSI',8,8,2,3,'serif');word(b,'MINI',11,35,2,2,'serif',1,3)
        tiny(b,'SPACE TO START',9,56);b.line(9,52,113,52)
    elif kind=='five-stones':
        for x in range(116,192,12):b.line(x,0,x,63)
        for y in range(5,64,12):b.line(116,y,191,y)
        for x,y in ((128,17),(140,29),(152,41),(164,53)):circle(b,x,y,5,1,True)
        word(b,'FIVE',5,4,4,3,'brush');word(b,'STONES',5,30,3,3,'brush')
        label(b,'SPACE TO START',5,55,98,0)
    elif kind=='hex-front':
        b.rect(0,0,192,64,1,True)
        for x,y in ((15,14),(15,43),(177,15),(177,43),(33,28),(159,28)):
            poly(b,[(x-8,y-5),(x,y-10),(x+8,y-5),(x+8,y+5),(x,y+10),(x-8,y+5)],0)
        word(b,'HEX',62,5,4,3,'stencil',0);word(b,'FRONT',48,31,3,3,'wire',0)
        tiny(b,'SPACE TO START',71,57,0)
    elif kind=='knight-tour':
        for y in range(8):
            for x in range(8):
                if (x+y)%2:
                    def point(col,row):return (round(12-row*1.5+col*(40+row*3)/8),round(5+row*7))
                    poly(b,[point(x,y),point(x+1,y),point(x+1,y+1),point(x,y+1)],1,True)
        b.line(0,62,64,62)
        poly(b,[(13,53),(52,53),(47,44),(32,44),(45,33),(48,16),(33,4),(28,17),(17,24),(19,34),(28,29),(25,43)],0,True)
        word(b,'KNIGHT',73,7,3,2,'serif');word(b,'TOUR',88,29,3,3,'serif')
        tiny(b,'SPACE TO START',85,55)
    elif kind=='peg-rescue':
        for x,y in ((21,17),(42,17),(21,38),(42,38),(63,38)):circle(b,x,y,8,1,True);circle(b,x-2,y-2,3,0)
        b.line(15,52,57,52);poly(b,[(51,48),(58,52),(51,56)],1,True)
        word(b,'PEG',91,3,4,3,'block');word(b,'RESCUE',76,28,3,3,'condensed')
        tiny(b,'SPACE TO START',95,55)
    elif kind=='pawn-race':
        for x in range(0,192,12):b.rect(x,52,6,6,1,True);b.rect(x+6,58,6,6,1,True)
        circle(b,29,16,8,1,True);poly(b,[(25,24),(33,24),(41,43),(18,43)],1,True);b.rect(13,44,34,6,1,True)
        word(b,'PAWN',67,4,3,3,'sport');word(b,'RACE',74,29,3,3,'slant')
        label(b,'SPACE TO START',92,54,97,0)
    elif kind=='dot-claim':
        for x,y in ((8,8),(43,8),(8,41),(43,41),(176,11),(176,45)):circle(b,x,y,3,1,True)
        b.line(8,8,43,8);b.line(43,8,43,41);b.line(8,41,43,41);hatch(b,11,11,28,27,5)
        word(b,'DOT',76,2,4,3,'dots');word(b,'CLAIM',69,28,3,3,'block')
        tiny(b,'SPACE TO START',75,56)
    elif kind=='pipe-weave':
        for d in range(5):
            for p,q in zip([(0,8+d),(29,8+d),(29,46+d),(165,46+d),(165,5+d),(191,5+d)],[(29,8+d),(29,46+d),(165,46+d),(165,5+d),(191,5+d)]):b.line(*p,*q)
        for x,y in ((27,15),(163,29)):b.rect(x-3,y,10,5)
        word(b,'PIPE',62,2,3,3,'outline');word(b,'WEAVE',45,25,4,3,'condensed')
        tiny(b,'SPACE TO START',72,57)
    elif kind=='number-rail':
        for y in (0,3,48,51):b.line(0,y,191,y)
        for x in range(0,192,10):b.line(x,47,x+4,54)
        word(b,'NUMBER',7,8,3,2,'stencil');word(b,'RAIL',105,28,3,2,'block')
        for x,n in ((5,'2'),(30,'4'),(55,'8')):b.rect(x,25,21,20);word(b,n,x+4,27,2,2,'block')
        tiny(b,'SPACE TO START',70,57)
    elif kind=='mine-field':
        for x in range(-60,210,12):poly(b,[(x,0),(x+6,0),(x+20,14),(x+14,14)],1,True)
        for x in range(-60,210,12):poly(b,[(x,50),(x+6,50),(x+20,63),(x+14,63)],1,True)
        circle(b,22,32,10,1,True)
        for a in range(0,360,45):b.line(22,32,22+int(16*math.cos(math.radians(a))),32+int(16*math.sin(math.radians(a))));
        word(b,'MINE FIELD',50,19,2,3,'stencil');label(b,'SPACE TO START',69,53,91,0)
    elif kind=='loop-trace':
        for i in range(4):
            b.rect(2+i*5,2+i*5,49-i*10,49-i*10);b.rect(17+i*5,42-i*5,7,10,0,True)
        word(b,'LOOP',65,5,4,3,'wire');word(b,'TRACE',68,31,3,3,'outline')
        b.line(3,59,59,59);circle(b,59,59,2);tiny(b,'SPACE TO START',82,56)
    elif kind=='ace-stack':
        for x,y in ((5,20),(15,12),(25,4)):card(b,x,y,37,49,'A')
        word(b,'ACE',88,3,4,3,'serif');word(b,'STACK',73,28,3,3,'serif')
        tiny(b,'SPACE TO START',94,55)
    elif kind=='suit-run':
        poly(b,[(0,0),(192,0),(149,64),(0,64)],1,True)
        for x,y in ((153,2),(168,18),(157,34)):card(b,x,y,22,29,'2',1)
        word(b,'SUIT',8,5,4,3,'slant',0);word(b,'RUN',51,30,4,3,'sport',0)
        tiny(b,'SPACE TO START',8,56,0)
    elif kind=='dice-hold':
        for x,y,s in ((6,6,27),(28,30,29),(161,4,25)):
            poly(b,[(x,y+4),(x+4,y),(x+s-1,y),(x+s-1,y+s-5),(x+s-5,y+s-1),(x,y+s-1)],1,True)
            b.rect(x+1,y+5,s-6,s-6,0,True);b.line(x+3,y+3,x+s-7,y+3,0)
            for dx,dy in ((5,9),(s-10,s-7),((s-4)//2,(s+4)//2)):circle(b,x+dx,y+dy,2,1,True)
        word(b,'DICE',61,5,3,3,'dots');word(b,'HOLD',66,31,3,3,'block')
        tiny(b,'SPACE TO START',76,57)
    elif kind=='push-luck':
        for y in (41,32,23):
            b.rect(4,y,51,14,1,True);b.rect(6,y+2,47,10,0,True);b.line(8,y+6,51,y+6)
        star(b,29,10,7);word(b,'PUSH',71,5,3,3,'serif');word(b,'LUCK',70,30,3,3,'outline')
        tiny(b,'SPACE TO START',72,57)
    elif kind=='wall-break':
        for y in (0,9,18):
            for x in range(0,192,24):b.rect(x+(y%2)*12,y,22,7,1,True)
        poly(b,[(13,16),(171,12),(166,48),(17,53)],0,True)
        word(b,'WALL BREAK',25,24,3,4,'condensed');b.line(4,62,32,38);circle(b,33,37,3,1,True)
        label(b,'SPACE TO START',91,55,98)
    elif kind=='tail-trail':
        path=[(0,9),(29,9),(29,47),(10,47),(10,29),(47,29),(47,59),(158,59),(158,40),(189,40),(189,11),(158,11)]
        for p,q in zip(path,path[1:]):
            for d in range(5):b.line(p[0],p[1]+d,q[0],q[1]+d)
        b.rect(151,5,16,15,1,True);b.rect(155,7,3,3,0,True);b.rect(163,7,3,3,0,True)
        word(b,'TAIL',57,3,3,3,'block');word(b,'TRAIL',57,28,3,3,'block');label(b,'SPACE TO START',60,55,88,0)
    elif kind=='maze-chase':
        b.rect(0,0,192,64,1,True)
        for y in (2,7,50,55):b.line(2,y,188,y,0)
        for x in (2,7,183,188):b.line(x,2,x,55,0)
        circle(b,27,30,13,0,True);poly(b,[(27,30),(43,20),(43,40)],1,True)
        word(b,'MAZE',56,10,3,2,'block',0);word(b,'CHASE',54,29,3,2,'outline',0)
        for x in (47,159,169,179):b.rect(x,28,2,2,0,True)
        tiny(b,'SPACE TO START',75,58,0)
    elif kind=='river-hop':
        for y in (5,18,43,56):
            for x in range(0,192,24):b.line(x,y,x+8,y+3);b.line(x+8,y+3,x+17,y)
        b.rect(37,9,123,41,0,True);word(b,'RIVER',42,10,4,3,'condensed');word(b,'HOP',90,31,4,3,'condensed')
        circle(b,20,36,9,1,True);circle(b,14,25,4,1,True);circle(b,26,25,4,1,True);b.dot(14,24,0);b.dot(26,24,0)
        label(b,'SPACE TO START',72,55,89,0)
    elif kind=='tower-leap':
        for x,y,h in ((0,5,58),(22,19,44),(44,35,28),(167,20,43)):
            poly(b,[(x,y+3),(x+4,y),(x+17,y),(x+17,y+h-4),(x+13,y+h-1),(x,y+h-1)],1,True)
            b.line(x+13,y+4,x+13,y+h-3,0);b.line(x+1,y+3,x+13,y+3,0)
            for yy in range(y+5,62,8):b.rect(x+5,yy,4,3,0,True)
        word(b,'TOWER',68,3,3,3,'condensed');word(b,'LEAP',84,29,3,3,'sport')
        poly(b,[(49,24),(57,10),(65,24)],1,True);label(b,'SPACE TO START',73,55,99,0)
    elif kind=='bomb-vault':
        b.rect(0,0,192,64,1,True);circle(b,38,36,24,0);circle(b,38,36,20,0);b.line(37,12,47,0,0);star(b,49,3,4,0)
        word(b,'BOMB',78,6,3,3,'stencil',0);word(b,'VAULT',73,32,3,3,'stencil',0)
        tiny(b,'SPACE TO START',85,57,0)
    elif kind=='grid-claim':
        for x,y,w,h in ((0,0,47,35),(145,0,47,27),(0,43,42,21),(133,38,59,26)):
            hatch(b,x,y,w,h,3);b.rect(x,y,w,h)
        word(b,'GRID',57,6,3,3,'outline');word(b,'CLAIM',51,31,3,3,'stencil')
        label(b,'SPACE TO START',53,55,84,0)
    elif kind=='gravity-run':
        b.rect(0,0,192,7,1,True);b.rect(0,49,192,7,1,True)
        for x in range(8,192,18):poly(b,[(x,7),(x+5,15),(x+10,7)],1,True);poly(b,[(x,49),(x+5,41),(x+10,49)],1,True)
        word(b,'GRAVITY RUN',14,20,3,3,'condensed');person(b,175,20,1,3)
        tiny(b,'SPACE TO START',70,58)
    elif kind=='star-patrol':
        b.rect(0,0,192,64,1,True)
        for i in range(47):b.dot((i*47+11)%192,(i*29+7)%64,0)
        poly(b,[(13,46),(31,7),(49,46),(31,38)],0,True);b.line(31,16,31,36)
        word(b,'STAR',67,7,3,3,'outline',0);word(b,'PATROL',59,32,3,3,'condensed',0)
        tiny(b,'SPACE TO START',84,57,0)
    elif kind=='orbit-guard':
        b.rect(0,0,192,64,1,True);circle(b,40,32,24,0);circle(b,40,32,14,0,True)
        for x in range(4,77):b.dot(x,18+(x-4)//3,0)
        star(b,64,12,4,0);word(b,'ORBIT',86,5,3,3,'wire',0);word(b,'GUARD',78,30,3,3,'condensed',0)
        tiny(b,'SPACE TO START',88,56,0)
    elif kind=='target-range':
        for r in (10,20,30):circle(b,39,32,r)
        b.line(0,32,77,32);b.line(39,0,39,63)
        word(b,'TARGET',83,6,3,3,'condensed');word(b,'RANGE',83,30,3,3,'stencil')
        tiny(b,'SPACE TO START',92,57)
    elif kind=='ricochet-ops':
        pts=[(0,2),(34,46),(81,4),(141,48),(189,5)]
        for p,q in zip(pts,pts[1:]):b.line(*p,*q)
        for x,y in pts[1:-1]:b.line(x-6,y,x+6,y)
        b.rect(10,15,173,25,1,True);word(b,'RICOCHET OPS',15,19,3,3,'condensed',0)
        tiny(b,'SPACE TO START',72,57)
    elif kind=='relay-quest':
        b.rect(0,0,192,64);poly(b,[(1,1),(47,1),(40,14),(44,32),(38,52),(1,62)],1,True)
        for x,y in ((12,11),(24,31),(10,51)):b.rect(x,y,16,4,0,True)
        word(b,'RELAY',57,5,3,3,'serif');word(b,'QUEST',57,31,3,3,'serif')
        label(b,'SPACE TO START',83,55,91,0)
    elif kind=='echo-cavern':
        b.rect(0,0,192,64,1,True)
        poly(b,[(0,48),(13,18),(22,28),(32,5),(45,23),(61,13),(72,47)],0,True)
        for r in (8,16,24):
            for a in range(-70,71):b.dot(41+int(r*math.cos(math.radians(a))),36+int(r*math.sin(math.radians(a))),1)
        word(b,'ECHO',84,7,3,3,'brush',0);word(b,'CAVERN',81,33,3,3,'condensed',0)
        tiny(b,'SPACE TO START',85,57,0)
    elif kind=='micro-rogue':
        # A book cover, large small-type subtitle and a lurking creature.
        b.rect(0,0,192,64);b.line(5,1,5,62);b.line(9,1,9,62)
        word(b,'MICRO',19,6,2,2,'serif',1,3);word(b,'ROGUE',16,25,4,3,'condensed')
        poly(b,[(133,55),(128,31),(140,9),(149,18),(162,4),(176,28),(181,56)],1,True)
        b.rect(140,30,8,3,0,True);b.rect(162,30,8,3,0,True)
        tiny(b,'SPACE TO START',18,55)
    elif kind=='signal-ghost':
        b.rect(0,0,192,64,1,True)
        for y in range(0,64,3):
            for x in range(132,192):
                if (x*7+y*11)%9<2:b.dot(x,y,0)
        word(b,'SIGNAL',8,6,3,3,'wire',0);word(b,'GHOST',19,31,3,3,'stencil',0)
        for p,q in zip([(155,53),(155,32),(167,21),(179,32),(179,53),(173,48),(167,54),(161,48)],[(155,32),(167,21),(179,32),(179,53),(173,48),(167,54),(161,48),(155,53)]):b.line(*p,*q,0)
        tiny(b,'SPACE TO START',10,57,0)
    elif kind=='rail-dispatch':
        for y in (3,8,47,52):b.line(0,y,191,y)
        for x in range(2,191,9):b.line(x,2,x,9);b.line(x,46,x,53)
        label(b,'CONTROL ROOM',3,10,60)
        word(b,'RAIL DISPATCH',7,23,3,4,'condensed')
        tiny(b,'SPACE TO START',3,57);tiny(b,'12 TIMETABLES',132,57)
    elif kind=='orchard-days':
        for x,y,r in ((17,23,13),(42,15,12),(169,27,15),(145,17,9)):
            b.line(x,54,x+12,50);b.line(x+2,54,x+14,50)
            circle(b,x,y,r,1,True);b.line(x,y,x,54)
            for dx,dy in ((-4,0),(4,5),(1,-6)):circle(b,x+dx,y+dy,2,0,True)
        b.line(0,54,191,54);b.rect(52,7,83,44,0,True)
        word(b,'ORCHARD',55,8,3,3,'condensed');word(b,'DAYS',66,32,3,3,'serif')
        tiny(b,'SPACE TO START',72,58)
    elif kind=='market-harbor':
        for y in (40,46,52):
            for x in range(0,192,18):b.line(x,y,x+6,y+2);b.line(x+6,y+2,x+12,y)
        poly(b,[(0,25),(62,25),(51,40),(13,40)],1,True);b.line(33,2,33,25);poly(b,[(35,3),(59,22),(35,22)],1,True)
        b.line(5,28,57,28,0);b.line(14,37,49,37,0);b.line(49,37,56,30,0)
        word(b,'MARKET',74,3,3,3,'condensed');word(b,'HARBOR',73,24,3,3,'condensed')
        label(b,'SPACE TO START',99,54,91,0)
    elif kind=='wind-putt':
        poly(b,[(0,45),(32,32),(70,45),(100,29),(133,44),(162,36),(191,48),(191,64),(0,64)],1,True)
        b.line(25,6,25,39);poly(b,[(26,6),(47,12),(26,18)],1,True)
        word(b,'WIND PUTT',62,5,3,4,'condensed');b.line(67,29,182,29)
        for i in range(7):b.dot(51+i*16,39-(i*(6-i)))
        circle(b,49,42,2);tiny(b,'SPACE TO START',104,56,0)
    elif kind=='rally-return':
        b.rect(0,0,192,64,1,True)
        b.rect(7,21,4,28,0,True);b.rect(180,7,4,28,0,True)
        for y in range(0,64,6):b.rect(95,y,1,3,0,True)
        word(b,'RALLY',26,5,3,3,'sport',0);word(b,'RETURN',39,32,3,3,'condensed',0)
        b.rect(20,13,4,4,0,True);tiny(b,'SPACE TO START',67,57,0)
    elif kind=='penalty-arc':
        b.rect(10,1,60,47)
        for x in range(12,69,9):b.line(x,3,x,46)
        for y in range(3,47,8):b.line(12,y,68,y)
        b.line(1,9,61,9);b.line(1,9,1,56);b.line(61,9,61,56)
        for p,q in [((1,9),(10,1)),((61,9),(69,1)),((1,56),(10,47)),((61,56),(69,47))]:b.line(*p,*q)
        person(b,36,22);circle(b,55,48,6,0,True);circle(b,55,48,6);poly(b,[(54,44),(58,47),(57,51),(52,51),(51,47)],1,True)
        word(b,'PENALTY',77,4,3,3,'condensed');word(b,'ARC',101,27,4,3,'sport')
        tiny(b,'SPACE TO START',91,57)
    elif kind=='beat-step':
        b.rect(0,0,192,64,1,True)
        for x,h in ((3,21),(11,34),(19,18),(165,26),(173,45),(181,33)):
            for y in range(52-h,53,4):b.rect(x,y,5,2,0,True)
        word(b,'BEAT',40,5,4,3,'outline',0);word(b,'STEP',49,30,4,3,'sport',0)
        for x in (31,146):poly(b,[(x,53),(x+5,48),(x+10,53),(x+7,53),(x+7,58),(x+3,58),(x+3,53)],0,True)
        tiny(b,'SPACE TO START',76,58,0)
    elif kind=='balance-dock':
        for x,y,w in ((0,46,47),(8,32,41),(15,18,34)):
            poly(b,[(x,y+3),(x+3,y),(x+w-1,y),(x+w-1,y+8),(x+w-4,y+11),(x,y+11)],1,True)
            b.rect(x+1,y+4,w-5,7,0,True);hatch(b,x+2,y+4,w-7,6,6)
            b.line(x+4,y+1,x+w-4,y+1,0)
        b.line(4,0,182,0);b.line(174,0,174,11);b.line(169,11,174,16);b.line(174,16,179,11)
        word(b,'BALANCE',59,6,3,3,'condensed');word(b,'DOCK',79,29,3,3,'stencil')
        tiny(b,'SPACE TO START',84,56)
    else:raise ValueError('Missing original title composition: '+kind)
    return b.bytes()
