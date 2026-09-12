# SPDX-License-Identifier: MIT
from hud_layouts import b, w, n, setup, status

# Values retain the shared layout; the game paints its own control-panel art.
SPEC = setup(0, 'panel', 'LIGHTS', [
    n('LIT', b('grid_stat')),
    n('USED', w('challenge_moves'), 4),
    n('PAR', w('challenge_par'), 4),
], status(
    'LDAB lamp_view_bonus\nCMPB challenge_need\nBEQ @full\n'
    'CLRB\nBRA @done\n@full:\nLDAB #1\n@done:\nCLRA',
    ['BONUS: MISSING', 'BONUS: OK'],
))


def panel_art(faces):
    from art import Bitmap
    from visual_art import tiny, poly
    art=Bitmap()
    art.rect(0,0,192,8,1,True)
    tiny(art,'LAMP GRID',45,1,0)
    tiny(art,SPEC['brand'],130,1,0)
    # Header wiring and a tiny lamp emblem.
    poly(art,[(8,1),(11,1),(13,3),(11,5),(8,5),(6,3),(8,1)],0)
    art.line(8,6,11,6,0)
    for y in (2,5):
        art.line(18,y,34,y,0);art.line(91,y,116,y,0)
    art.line(32,2,36,6,0);art.line(91,2,87,6,0)
    # Chamfered board housing, mounting screws and traces into each row.
    poly(art,[(9,9),(118,9),(123,14),(123,58),(119,62),(8,62),(4,58),(4,14),(9,9)])
    for x in (9,118):
        for y in (13,58):
            art.rect(x-1,y-1,3,3);art.dot(x,y)
    for row in range(5):
        y=20+row*8
        art.line(19,y,23,y);art.rect(16,y-1,3,3)
        art.line(104,y,108,y);art.rect(109,y-1,3,3)
    art.line(13,20,13,52);art.line(115,20,115,52)
    for y in (20,36,52):
        art.line(13,y,16,y);art.line(111,y,115,y)
    # Small connector pins and ventilation slots fill the side and lower rails.
    for y in range(24,50,4):
        art.line(7,y,10,y);art.line(118,y,121,y)
    for x in range(24,102,5):art.line(x,59,x+2,59)
    for x in (27,43,59,75,91):
        art.line(x,12,x+6,12);art.dot(x+7,13)
    # Meter brackets leave clear space around the count and its LIT label.
    art.line(128,8,128,63);art.line(191,8,191,63)
    tiny(art,SPEC['fields'][0]['label'],133,9)
    art.line(151,11,184,11);art.line(184,11,188,15)
    for y,length in ((18,3),(22,6),(26,3)):
        art.line(133,y,133+length,y);art.line(186-length,y,186,y)
    for i,field in enumerate(SPEC['fields'][1:],4):
        tiny(art,field['label'],133,i*8+1)
        art.line(133,i*8+7,151,i*8+7);art.line(154,i*8+7,158,i*8+7)
    # Match both appearances of a bonus lamp, with a blank row above and below.
    for left,face in ((132,faces[2]),(148,faces[3])):
        for y,source_y in enumerate((0,1,3,4,6,7),49):
            for x,pixel in enumerate(face.p[source_y]):art.dot(left+x,y,pixel)
    tiny(art,'BONUS',168,49)
    # Dynamic fields and all playable tiles must remain free of static artwork.
    for y in range(16,56):
        assert not any(art.p[y][24:104])
    for x,y,width,height in ((143,16,33,16),(171,32,16,8),(171,40,16,8),(132,56,56,8)):
        assert not any(art.p[Y][X] for Y in range(y,y+height) for X in range(x,x+width))
    return art
