# SPDX-License-Identifier: MIT
"""Original LCD tile and title artwork for the genre library."""
from pathlib import Path
from art import Bitmap, asm_bytes

from visual_art import title

def assets(path,name,kind,w,h,sx,sy,sprites,stages,data='',stat='LEFT',action='SPACE',aux=('UNDO','RESET'),layout=None,navigation=None,cell_count=None):
    assert w*sx<=16 and h*sy<=7 and len(sprites)*sx*sy+1<=128
    count=cell_count if cell_count is not None else w*h
    cells=[];sub=[];ox=(16-w*sx)//2;oy=(7-h*sy)//2
    for y in range(7):
        for x in range(16):
            X,Y=x-ox,y-oy
            inside=0<=X<w*sx and 0<=Y<h*sy
            cells.append((Y//sy)*w+X//sx if inside else 255)
            sub.append((Y%sy)*sx+X%sx if inside else 0)
    if layout is not None:cells,sub=layout
    assert len(cells)==112 and len(sub)==112 and all(c==255 or 0<=c<count for c in cells)
    assert all(0<=s<sx*sy for s in sub)
    neighbors=list(navigation) if navigation is not None else []
    if navigation is None:
        assert count==w*h
        for dx,dy in ((0,-1),(0,1),(-1,0),(1,0)):
            for i in range(w*h):
                x,y=i%w+dx,i//w+dy
                neighbors.append(y*w+x if 0<=x<w and 0<=y<h else 255)
    assert len(neighbors)==4*count and all(n==255 or 0<=n<count for n in neighbors)
    tiles=[0]*8
    for sprite in sprites:
        raw=sprite.bytes()
        for y in range(sy):
            for x in range(sx):tiles.extend(raw[y*sx*8+x*8:y*sx*8+x*8+8])
    def txt(label,s):return asm_bytes(label,list(s.encode('ascii'))+[0])
    source='; SPDX-License-Identifier: MIT\n'+f'.equ STAGES,{stages}\n.equ CELLS,{count}\n.equ TILE_STRIDE,{sx*sy}\n.section .data, data\n'
    source+=txt('game_name',name)+txt('aux1_label',aux[0])+txt('aux2_label',aux[1])+txt('grid_stat_label',stat)+txt('grid_action_label',action)
    source+=asm_bytes('title_art',title(name,kind))+asm_bytes('tiles',tiles)+asm_bytes('view_cells',cells)+asm_bytes('view_subtiles',sub)+asm_bytes('neighbors',neighbors)+data
    Path(path,'assets.s').write_text(source)
