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
def asm_bytes(label,data):
    return label+':\n'+''.join('    .byte '+','.join(f'${x:02X}' for x in data[i:i+24])+'\n' for i in range(0,len(data),24))
