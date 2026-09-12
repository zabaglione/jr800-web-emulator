# SPDX-License-Identifier: MIT
from art import Bitmap
from visual_art import tiny, poly


def panel_art():
    b=Bitmap()
    tiny(b,'SLIDER',2,1)
    b.line(2,7,44,7)
    b.line(0,8,0,63);b.line(63,8,63,63)
    for label,y in [('LEFT',9),('USED',33),('PAR',41),('STAMP',49)]:tiny(b,label,5,y)
    # Small scales frame the large counter without touching its digit area.
    for y,length in [(18,3),(22,5),(26,3),(30,5)]:
        b.line(4,y,4+length,y);b.line(59-length,y,59,y)
    for y in (39,47):
        b.line(5,y,24,y);b.line(27,y,31,y)
    # A fitted case, side runners, screws and vents surround the sliding tiles.
    poly(b,[(69,0),(186,0),(190,4),(190,59),(186,63),(69,63),(65,59),(65,4),(69,0)])
    tiny(b,'SLIDE NINE',108,1)
    b.line(77,3,103,3);b.line(152,3,180,3)
    for x in (70,185):
        for y in (5,58):
            b.rect(x-1,y-1,3,3);b.dot(x,y)
    for x in (68,187):b.line(x,12,x,51)
    for y in (16,32,48):
        b.line(70,y,76,y);b.line(179,y,185,y)
        poly(b,[(73,y-2),(75,y),(73,y+2)])
        poly(b,[(182,y-2),(180,y),(182,y+2)])
    for x in (71,184):
        for y in range(22,46,4):b.line(x,y,x+2,y)
    b.line(81,60,174,60)
    for x in range(84,173,6):b.rect(x,57,3,2,1,True)
    # Static decorations may not overlap a dynamic number or playable tile.
    for x,y,w,h in [(48,0,12,8),(15,16,33,16),(43,32,16,8),
                    (43,40,16,8),(54,48,6,8),(4,56,56,8),(80,8,96,48)]:
        assert not any(b.p[Y][X] for Y in range(y,y+h) for X in range(x,x+w)), (x,y,w,h)
    return b
