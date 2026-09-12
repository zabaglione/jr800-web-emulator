# SPDX-License-Identifier: MIT
from copy import deepcopy
from hud_layouts import SPECS
from visual_art import tiny

def decorate(b,slots):
    b.rect(4,48,35,8,1,True)
    rows=['........','.####...','.#..##..','.#.###..','.#...#..','.###.#..','.#####..','........']
    for y,row in enumerate(rows):
        for x,ch in enumerate(row):
            if ch=='#':b.dot(5+x,48+y,0)
    tiny(b,'INTEL',16,49,0)

SPEC=deepcopy(SPECS['step-strike'])
SPEC['decorate']=decorate
