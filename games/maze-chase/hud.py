# SPDX-License-Identifier: MIT
from copy import deepcopy
from hud_layouts import SPECS
from visual_art import tiny,poly

def decorate(b,slots):
    for x in (0,160):
        b.rect(x,8,32,56,1,True)
        # A recessed instrument rail with chamfered corners. Labels have a
        # full blank pixel above and below; ornament never crosses a glyph.
        poly(b,[(x+4,8),(x+28,8),(x+30,10),(x+30,60),(x+27,63),(x+4,63),(x+1,60),(x+1,11),(x+4,8)],0)
        for yy in (20,23,26,44,47):b.dot(x+2,yy,0)
    for text,x,y in [('FOOD',5,11),('LIVES',165,11),('POWER',5,35),('SCORE',165,35)]:
        tiny(b,text,x,y,0)
    tiny(b,'RETURN',163,57,0)

SPEC=deepcopy(SPECS['maze-chase'])
SPEC['decorate']=decorate
