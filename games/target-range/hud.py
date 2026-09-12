# SPDX-License-Identifier: MIT
from copy import deepcopy
from hud_layouts import SPECS
from visual_art import tiny,poly

def decorate(b,slots):
    b.rect(0,0,192,8,1,True)
    tiny(b,'RANGE',2,1,0)
    for x in range(64,192,16):
        b.line(x,2,x+10,2,0)
        b.line(x+5,5,x+13,5,0)
        b.dot(x+2,5,0)
    b.rect(0,8,64,56,0,True)
    # Deep wooden uprights surround the score card, with recessed screw heads.
    for x in (0,60):
        b.rect(x,8,4,56,1,True)
        for y in range(12,61,9):b.line(x+1,y,x+1,y+4,0)
        for y in (9,61):b.dot(x+2,y,0)
    poly(b,[(7,8),(56,8),(59,11),(59,28),(56,31),(7,31),(4,28),(4,11),(7,8)],1)
    for text,y in [('SCORE',10),('ROUND',33),('TIME',41),('MISS',49)]:tiny(b,text,7,y)
    for y in (39,47,55):
        b.line(6,y,12,y)
        b.dot(57,y)
    b.line(4,63,59,63)

SPEC=deepcopy(SPECS['target-range'])
SPEC['decorate']=decorate
