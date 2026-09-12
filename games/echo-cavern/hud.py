# SPDX-License-Identifier: MIT
from copy import deepcopy
from hud_layouts import SPECS
from art import Bitmap
SPEC=deepcopy(SPECS['echo-cavern'])
SPEC['status']['code']='LDD #0\nTST echo_left\nBNE @done\nINCB\n@done:'
SPEC['status']['values']=['EXIT LOCKED','EXIT OPEN']
def decorate(bitmap,slots):
    slots[-1]['x']=142
    slots[-1]['chars']=11
    door=Bitmap.from_rows(['..####..','.######.','.##..##.','.##..##.','.##.#.#.','.##..##.','.##..##.','.######.'])
    for y,row in enumerate(door.p):
        for x,pixel in enumerate(row):bitmap.dot(132+x,56+y,1-pixel)
SPEC['decorate']=decorate
