# SPDX-License-Identifier: MIT
import sys,json,random
from collections import deque
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent
from puzzle_assets import campaign, level_records, challenge_data
levels=campaign(root)
from digit_art import draw_digits
from visual_art import tiny
sprites=[]
for n in range(9):
    b=Bitmap(32,16)
    # Flat, separated frames distinguish the stationary destination from the
    # numbered tiles. The blank is a recessed central well, without a cursor.
    b.rect(1,1,30,14)
    if n:
        draw_digits(b,str(n),10,4,'slide-nine',2,1)
    else:
        b.rect(9,3,14,10,1,True)
    sprites.append(b)
# Pack two inverse number faces per sprite. Only the middle four subtiles are
# used, keeping all ordinary, target and number artwork within 128 tile IDs.
token_tiles=[]
for pair in range(4):
    b=Bitmap(32,16)
    for side in range(2):
        n=pair*2+side+1; face=Bitmap(16,16)
        face.line(0,1,15,1);face.line(0,14,15,14)
        face.rect(0,3,14,9,1,True)
        digit=Bitmap(32,16);draw_digits(digit,str(n),10,4,'slide-nine',2,1)
        for y in range(16):
            for x in range(16):
                if digit.p[y][x+8]:face.dot(x,y,0)
                b.dot(side*16+x,y,face.p[y][x])
        first=73+pair*8+side*2
        token_tiles.extend([0,first,first+1,0,0,first+4,first+5,0])
    sprites.append(b)
for complete in (False,True):
    b=Bitmap(32,16)
    for y in range(1,15):
        for x in range(1,31):
            if x in (1,2,29,30) or y in (1,2,13,14):
                b.dot(x,y,int(complete or (x+y)%2==0))
    sprites.append(b)
data=level_records([s['initial']['board'] for s in levels])+asm_bytes('slide_tokens',[s['bonus_tile'] for s in levels])+challenge_data(root,levels,3,4,2,3)
data+=asm_bytes('slide_token_tiles',token_tiles)
data+=asm_bytes('slide_frame_origins',[v for row in range(3) for col in range(3) for v in (88+col*32,1+row*2)])
stamp=[]
for n in range(1,9):
    b=Bitmap(6,8);b.rect(0,0,6,7,1,True);tiny(b,str(n),1,1,0);stamp+=b.bytes()
data+=asm_bytes('slide_stamp_faces',stamp)
assets(root,'SLIDE NINE','slide-nine',3,3,4,2,sprites,40,data,stat='LEFT',action='MOVE')
