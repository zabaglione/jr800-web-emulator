# SPDX-License-Identifier: MIT
"""Artillery instruments for the full-width battle view."""
from art import Bitmap
from visual_art import tiny,poly
from hud_layouts import b as byte,w,n,t,c,inc,flag

def make_custom(ident,compile_layout):
    art=Bitmap();slots=[];regions=[];alt=None
    def field(f,x,band,font=0,chars=None,dark=False,when=None):
        f=dict(f,x=x,band=band,font=font,chars=chars or (max(map(len,f['values'])) if 'values' in f else f.get('digits',3)),inverse=128 if dark else 0)
        if when is not None:f['when']=('selection_active',when)
        width=f['chars']*[4,6,6,11,1,6][font];height=16 if font in (2,3) else 8
        assert 0<=x and x+width<=192 and band*8+height<=64,(ident,f)
        for other,rect in zip(slots,regions):
            if other.get('when')!=f.get('when') and 'when' in other and 'when' in f:continue
            X,Y,W,H=rect
            assert x+width<=X or X+W<=x or band*8+height<=Y or Y+H<=band*8,(ident,f,other)
        slots.append(f);regions.append((x,band*8,width,height))
    def label(text,x,y,dark=False,on=None):tiny(on or art,text,x,y,0 if dark else 1)
    def selected(var,index):return byte(var)+f'\nCMPB #{index}\nBEQ @yes\nLDD #0\nBRA @done\n@yes:\nLDD #1\n@done:'
    if ident=='arc-duel':
        theme='artillery-instruments';art.rect(0,0,192,8,1,True)
        for name,x in [('HP',1),('A',33),('P',65),('W',96),('CPU',130),('LV',166)]:label(name,x,1,True)
        field(n('',byte('health')),11,0,1,3,True)
        field(n('','LDAA angle\nLDAB #5\nMUL\nADDD #15'),40,0,1,3,True)
        field(n('',byte('power')),73,0,1,3,True)
        field(t('','CLRB\nTST wind\nBEQ @done\nBMI @west\nLDAB #2\nBRA @done\n@west:\nLDAB #1\n@done:\nCLRA',['-','<','>']),102,0,0,1,True)
        field(n('','LDAB wind\nBPL @done\nNEGB\n@done:\nCLRA'),108,0,1,2,True)
        field(n('',byte('cpu_health')),144,0,1,3,True)
        field(n('',inc('stage'),1),180,0,1,1,True)
        label('ROUND',2,9);label('/',34,9);label('LAND',51,9);label('RETURN',166,9)
        field(n('',byte('wins'),1),26,1,0,1);field(n('',byte('cpu_wins'),1),41,1,0,1)
        field(n('',inc('terrain'),1),71,1,0,1)
        field(t('',byte('arc_mode'),['AIM / FIRE','FLIGHT','FLIGHT','CPU AIM','SPACE: NEXT']),87,1,0,15)
        bands=range(2)
    def spans(canvas):
        raw=canvas.bytes();return [(y,x,raw[y*192+x:y*192+x+64]) for y in bands for x in range(0,192,64)]
    return compile_layout(ident,dict(view=0,theme=theme),art,slots,[],regions,spans(art),spans(alt) if alt else None)
