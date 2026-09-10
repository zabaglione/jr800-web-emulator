# SPDX-License-Identifier: MIT
"""Project-authored one-bit artwork. No ROM glyphs or external artwork."""
import re
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
FONT = []
for line in (ROOT/'sdk/lib/lcd/font.s').read_text().splitlines():
    if '.byte' in line:
        FONT.append([int(x,16) for x in re.findall(r'\$([0-9A-F]{2})',line.split(';')[0])])
class Bitmap:
    def __init__(self,w=192,h=64):
        self.w,self.h=w,h
        self.p=[[0]*w for _ in range(h)]
    def dot(self,x,y,c=1):
        if 0<=x<self.w and 0<=y<self.h:self.p[y][x]=c
    def line(self,x,y,X,Y,c=1):
        dx,dy=abs(X-x),-abs(Y-y);sx=1 if x<X else -1;sy=1 if y<Y else -1;e=dx+dy
        while True:
            self.dot(x,y,c)
            if (x,y)==(X,Y):break
            e2=2*e
            if e2>=dy:e+=dy;x+=sx
            if e2<=dx:e+=dx;y+=sy
    def rect(self,x,y,w,h,c=1,fill=False):
        for Y in range(y,y+h):
            for X in range(x,x+w):
                if fill or X in (x,x+w-1) or Y in (y,y+h-1):self.dot(X,Y,c)
    def text(self,text,x,y,sx=1,sy=1,c=1):
        for ch in text:
            glyph=FONT[ord(ch)-32] if 32<=ord(ch)<=90 else [0]*5
            for a,bits in enumerate(glyph):
                for b in range(7):
                    if bits>>b&1:self.rect(x+a*sx,y+b*sy,sx,sy,c,True)
            x+=6*sx
    def bytes(self):
        return [sum(self.p[y+b][x]<<b for b in range(8)) for y in range(0,self.h,8) for x in range(self.w)]
def title(name,kind):
    b=Bitmap();b.rect(0,0,192,64);b.rect(2,2,188,60)
    if kind=='box-shift':
        for x,y in [(5,5),(20,22),(4,37),(166,4),(175,22),(158,37)]:
            b.rect(x,y,15,15);b.rect(x+2,y+2,11,11);b.line(x+2,y+2,x+12,y+12);b.line(x+12,y+2,x+2,y+12)
        for x in range(8,190,16):b.line(96,10,x,54)
    elif kind=='mirror-link':
        points=[(3,17),(25,17),(25,46),(170,46),(170,10),(4,10)]
        for (x,y),(X,Y) in zip(points,points[1:]):b.line(x,y,X,Y)
        for x,y in points[1:-1]:b.line(x-5,y+5,x+5,y-5);b.line(x-4,y+5,x+6,y-5)
        for x in range(8,190,12):b.dot(x,53)
    elif kind=='step-strike':
        for y in range(6,52,9):b.line(3,y,187,y-4)
        for x,y in [(9,30),(173,32)]:
            b.rect(x,y-8,7,7,1,True);b.rect(x+1,y,5,12,1,True);b.line(x+2,y+11,x-2,y+18);b.line(x+4,y+11,x+9,y+18);b.line(x+4,y+3,x+13,y-1)
        for x in range(30,163,17):b.rect(x,16,4,2,1,True)
    elif kind=='circuit-deck':
        for x in (5,16,161,172):b.rect(x,7+(x%3)*5,14,31);b.rect(x+3,13+(x%3)*5,8,9)
        for y in (5,20,38,50):b.line(3,y,189,y);b.rect(8,y-2,5,5);b.rect(179,y-2,5,5)
    elif kind=='pocket-factory':
        for x in range(5,190,19):b.rect(x,44,15,8);b.rect(x+3,46,4,4)
        for x in (5,167):
            b.rect(x,20,18,22);b.rect(x+2,11,4,13);b.rect(x+10,7,4,17)
            for y in range(5,42,7):b.line(x,y,x+17,y+3)
        b.line(3,54,188,54)
    else:
        for x in range(3,189):
            y=44+((x*13//19)%8);b.line(x,y,x,54)
        for x in (8,165):b.rect(x,38,18,9,1,True);b.line(x+8,38,x+16,31);b.rect(x+2,47,14,3)
        for x in range(12,181,4):b.dot(x,4+int((x-96)**2/320))
    for row,word in enumerate(name.split()):
        sx=3;sy=3;x=(192-(len(word)*6-1)*sx)//2;y=5+row*24
        mask=Bitmap();mask.text(word,x,y,sx,sy)
        pts=[(X,Y) for Y in range(64) for X in range(192) if mask.p[Y][X]]
        # Knock-out outline and a displaced hard shadow remain legible on motifs.
        for X,Y in pts:b.rect(X-2,Y-2,6,6,0,True)
        for X,Y in pts:b.dot(X+2,Y+2)
        for X,Y in pts:b.dot(X,Y)
        # Tiny cuts distinguish the logo from an enlarged ordinary text row.
        for X,Y in pts:
            if Y==y+10 and (X-x)%18 in (1,2):b.dot(X,Y,0)
    b.rect(48,54,96,8,0,True);b.text('SPACE TO START',57,55)
    return b.bytes()
def asm_bytes(label,data):
    return label+':\n'+''.join('    .byte '+','.join(f'${x:02X}' for x in data[i:i+24])+'\n' for i in range(0,len(data),24))
