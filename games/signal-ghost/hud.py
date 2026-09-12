# SPDX-License-Identifier: MIT
from copy import deepcopy
from hud_layouts import SPECS
from visual_art import tiny
SPEC=deepcopy(SPECS['signal-ghost'])
def decorate(bitmap,slots):
    for x in (0,160):bitmap.rect(x+1,8,2,56,1,True)
    for label,x,y in [('LINKS',2,9),('SCORE',2,33),('STEPS',162,9)]:tiny(bitmap,label,x,y,0)
SPEC['decorate']=decorate
