# SPDX-License-Identifier: MIT
import sys,json,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets, Bitmap, asm_bytes

from puzzle_assets import campaign, level_records, challenge_data
root=Path(__file__).parent
levels=campaign(root)
sprites=[]
for on in (0,1):
    b=Bitmap(16,8);b.rect(1,0,14,8);b.rect(4,2,8,4,on,True)
    if not on:b.dot(7,3);b.dot(8,4)
    sprites.append(b)
data=level_records([s['initial']['board'] for s in levels])+challenge_data(root,levels,5,2,1,5)
assets(root,'LAMP GRID','lamp-grid',5,5,2,1,sprites,40,data)
