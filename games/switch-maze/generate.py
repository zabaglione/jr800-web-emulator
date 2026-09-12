# SPDX-License-Identifier: MIT
import sys,json,random
from collections import deque
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent
from puzzle_assets import campaign, level_records, challenge_data
levels=campaign(root)
sprites=[]
for n in range(12):
    b=Bitmap(8,8)
    if n==0:pass
    elif n==1:
        b.rect(0,0,8,8);b.line(0,3,7,3);b.line(3,0,3,3);b.line(5,4,5,7)
    elif n in (2,3):b.rect(1,0,4,4);b.line(4,3,4,7);b.line(4,6,6,6)
    elif n in (4,5):b.text('A' if n==4 else 'B',1,0);b.dot(0,7);b.dot(7,7)
    elif n in (6,7):
        b.rect(0,0,8,8,1,True)
        b.text('A' if n==6 else 'B',1,0,c=0)
    elif n==8:b.rect(1,0,6,8);b.text('E',2,0)
    elif n in (9,10):
        b.line(0,0,7,0);b.line(0,7,7,7);b.dot(0,3);b.dot(7,4)
    else:
        b=Bitmap.from_rows(['..##....','..##....','...#....','.#####..',
                            '...#....','..#.#...','..#.#...','........'])
        # grid.s applies the cursor palette to the actor. Store the inverse so
        # its visible head, arms and two legs are dark against the floor.
        b.p=[[1-pixel for pixel in row] for row in b.p]
    sprites.append(b)
data=level_records([s['initial']['board'] for s in levels])+asm_bytes('start_cells',[s['initial']['start'] for s in levels])+challenge_data(root,levels,14,1,1,7)
assets(root,'SWITCH MAZE','switch-maze',14,7,1,1,sprites,40,data,stat='KEYS',action='MOVE')
