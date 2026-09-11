# SPDX-License-Identifier: MIT
"""Original LCD tile and title artwork for the genre library."""
from pathlib import Path
from art import Bitmap, asm_bytes

def logo(name,kind):
    b=Bitmap(); b.rect(0,0,192,64); b.rect(2,2,188,60)
    if kind=='lamp-grid':
        for x in (10,29,160,179):
            for y in (11,34):
                b.rect(x-4,y-4,9,9);b.rect(x-2,y+5,5,3,1,True)
                for dx,dy in ((-7,0),(7,0),(0,-7)):
                    b.line(x+dx,y+dy,x+dx*4//3,y+dy*4//3)
        for y in (23,47):b.line(3,y,188,y)
    elif kind=='slide-nine':
        for i,(x,y) in enumerate(((4,4),(20,21),(4,38),(173,3),(157,21),(172,38))):
            b.rect(x,y,15,15);b.text(str(i+1),x+5,y+4)
        b.line(3,53,188,53)
    elif kind=='ice-route':
        for x,y in ((12,12),(26,37),(178,14),(166,40)):
            b.line(x,y-9,x+7,y);b.line(x+7,y,x,y+9);b.line(x,y+9,x-7,y);b.line(x-7,y,x,y-9);b.line(x,y-9,x,y+9)
        for y in (5,24,50):b.line(3,y,188,y+3)
    elif kind=='switch-maze':
        for x in (5,171):
            b.rect(x,6,15,16);b.line(x+3,18,x+11,10);b.line(x+7,22,x+7,50);b.line(x+7,50,96,50)
            b.rect(x,29,15,16)
            for xx in range(x+3,x+14,4):b.line(xx,30,xx,43)
    elif kind=='line-four':
        for x in (4,20,158,174):
            for y in (7,22,37):
                b.rect(x,y,13,13);b.rect(x+3,y+3,7,7,1,(x+y)%3==0)
        b.line(3,53,188,53);b.line(28,3,165,51)
    elif kind=='reversi-mini':
        for col,x in enumerate((8,23,38,154,169,184)):
            for row,y in enumerate((10,27,44)):
                for Y in range(-5,6):
                    for X in range(-5,6):
                        if X*X+Y*Y<=25 and ((row+col)%2 or X*X+Y*Y>=13):b.dot(x+X,y+Y)
        b.line(30,51,161,51);b.line(157,48,161,51);b.line(157,54,161,51)
    elif kind=='five-stones':
        for y in (8,20,32,44):b.line(3,y,188,y)
        for x in (9,21,33,159,171,183):b.line(x,3,x,52)
        for x,y in ((9,8),(21,20),(33,32),(159,44),(171,32),(183,20)):
            b.rect(x-4,y-4,9,9,0,True)
            for Y in range(-3,4):
                for X in range(-3,4):
                    if X*X+Y*Y<=10:b.dot(x+X,y+Y)
    elif kind=='hex-front':
        for x in (10,27,164,181):
            for y in (10,28,46):
                points=[(x-7,y),(x-3,y-6),(x+3,y-6),(x+7,y),(x+3,y+6),(x-3,y+6),(x-7,y)]
                for p,q in zip(points,points[1:]):b.line(*p,*q)
                b.line(x-3,y,x+3,y);b.line(x,y-3,x,y+3)
        b.line(8,52,184,52);b.line(91,49,96,52);b.line(91,55,96,52)
    elif kind=='knight-tour':
        for x in (5,162):
            points=[(x,47),(x+24,47),(x+24,42),(x+11,42),(x+14,35),(x+24,33),(x+20,19),(x+13,15),(x+11,22),(x+5,26),(x+7,33),(x+13,31),(x+9,42),(x,42),(x,47)]
            for p,q in zip(points,points[1:]):b.line(*p,*q)
            b.dot(x+17,25);b.dot(x+17,26)
        for x in (37,70,103,136):b.line(x,51,x+15,51);b.line(x+15,51,x+15,46)
    elif kind=='peg-rescue':
        for x,y in ((11,12),(27,35),(176,13),(163,36)):
            for Y in range(-6,7):
                for X in range(-6,7):
                    if X*X+Y*Y<=36 and (x%2 or X*X+Y*Y>=19):b.dot(x+X,y+Y)
            b.line(x-7,y+9,x+7,y+9)
        for x in (30,70,110,150):
            b.line(x,51,x+20,48);b.line(x+20,48,x+16,46);b.line(x+20,48,x+17,52)
    elif kind=='pawn-race':
        for x in (6,165):
            b.rect(x,42,20,6,1,True);b.line(x+3,40,x+7,27);b.line(x+16,40,x+12,27)
            for Y in range(-5,6):
                for X in range(-5,6):
                    if X*X+Y*Y<=25:b.dot(x+10+X,20+Y)
            b.line(x+5,9,x+15,9);b.line(x+5,9,x+10,4);b.line(x+15,9,x+10,4)
        for x in range(31,158,8):b.rect(x,49,4,4,1,True)
    elif kind=='dot-claim':
        for x in (5,167):
            for yy in range(3):
                for xx in range(2):b.rect(x+xx*16,6+yy*19,4,4,1,True)
            b.line(x+2,8,x+18,8);b.line(x+18,8,x+18,46);b.line(x+2,27,x+2,46);b.line(x+2,46,x+18,46)
            b.rect(x+6,31,9,10,1,x==5)
        for x in range(35,159,15):b.rect(x,50,3,3,1,True)
    else:
        raise ValueError('A distinct title motif is required: '+kind)
    for row,word in enumerate(name.split()):
        sx=3; x=(192-(len(word)*6-1)*sx)//2; y=5+row*24
        mask=Bitmap();mask.text(word,x,y,3,3)
        pts=[(X,Y) for Y in range(64) for X in range(192) if mask.p[Y][X]]
        for X,Y in pts:b.rect(X-2,Y-2,6,6,0,True)
        for X,Y in pts:b.dot(X+2,Y+2)
        for X,Y in pts:b.dot(X,Y)
        for X,Y in pts:
            if Y==y+10 and (X-x)%18 in (1,2):b.dot(X,Y,0)
    b.rect(48,54,96,8,0,True);b.text('SPACE TO START',57,55)
    return b.bytes()

def assets(path,name,kind,w,h,sx,sy,sprites,stages,data='',stat='LEFT',action='SPACE',aux=('UNDO','RESET')):
    assert w*sx<=16 and h*sy<=7 and len(sprites)*sx*sy+1<=128
    cells=[];sub=[];ox=(16-w*sx)//2;oy=(7-h*sy)//2
    for y in range(7):
        for x in range(16):
            X,Y=x-ox,y-oy
            inside=0<=X<w*sx and 0<=Y<h*sy
            cells.append((Y//sy)*w+X//sx if inside else 255)
            sub.append((Y%sy)*sx+X%sx if inside else 0)
    neighbors=[]
    for dx,dy in ((0,-1),(0,1),(-1,0),(1,0)):
        for i in range(w*h):
            x,y=i%w+dx,i//w+dy
            neighbors.append(y*w+x if 0<=x<w and 0<=y<h else 255)
    tiles=[0]*8
    for sprite in sprites:
        raw=sprite.bytes()
        for y in range(sy):
            for x in range(sx):tiles.extend(raw[y*sx*8+x*8:y*sx*8+x*8+8])
    def txt(label,s):return asm_bytes(label,list(s.encode('ascii'))+[0])
    source='; SPDX-License-Identifier: MIT\n'+f'.equ STAGES,{stages}\n.equ CELLS,{w*h}\n.equ TILE_STRIDE,{sx*sy}\n.section .data, data\n'
    source+=txt('game_name',name)+txt('aux1_label',aux[0])+txt('aux2_label',aux[1])+txt('grid_stat_label',stat)+txt('grid_action_label',action)
    source+=asm_bytes('title_art',logo(name,kind))+asm_bytes('tiles',tiles)+asm_bytes('view_cells',cells)+asm_bytes('view_subtiles',sub)+asm_bytes('neighbors',neighbors)+data
    Path(path,'assets.s').write_text(source)
