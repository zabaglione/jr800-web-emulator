# SPDX-License-Identifier: MIT
"""The market ledger and port-selection screen, owned by MARKET HARBOR."""
from art import Bitmap
from visual_art import tiny
from hud_layouts import b as byte,w,n,t,inc,flag

def render(compile_layout):
    ident='market-harbor'
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
    theme='harbor-ledger';alt=Bitmap()
    for canvas in (art,alt):
        canvas.rect(0,0,192,8,1,True);canvas.rect(0,16,192,8,1,True);canvas.rect(0,56,192,8,1,True)
        for x in (1,59,101,143,190):canvas.line(x,24,x,55)
    label('HARBOR',3,1,True);label('SAIL FROM',3,1,True,alt)
    for canvas in (art,alt):label('DAY',99,1,True,canvas);label('/',132,1,True,canvas);label('JOB',160,1,True,canvas)
    field(t('',byte('market_port'),['NORTH','EAST','SOUTH','WEST']),48,0,5,5,True)
    field(n('',inc('market_day'),2),115,0,0,2,True);field(n('',byte('market_limit'),2),139,0,0,2,True);field(n('',inc('stage'),2),179,0,0,2,True)
    label('CASH',3,9);label('GOAL',81,9);label('LOAD',4,49);label('/',74,49)
    field(n('',w('market_cash'),5),23,1,1,5,when=False);field(n('',w('market_goal'),5),107,1,1,5,when=False)
    field(t('',byte('market_mode'),['BUY','SELL']),158,1,5,4,when=False)
    for text,x in [('GOODS',11),('PRICE',69),('HOLD',111),('ACTION',151)]:label(text,x,17,True)
    for i,name in enumerate(['RICE','ORE','SPICE']):
        y=25+i*8;label(name,12,y)
        field(t('',selected('market_good',i),['','>']),4,3+i,0,1,when=False)
        field(n('',byte(f'market_prices + {i}')),72,3+i,1,3,when=False)
        field(n('',byte(f'market_cargo + {i}')),114,3+i,1,3,when=False)
        field(t('',selected('market_good',i),['','SPACE']),147,3+i,0,9,when=False)
    field(n('',byte('market_load'),2),56,6,0,2,when=False);field(n('',byte('market_capacity'),2),82,6,0,2,when=False)
    art.line(143,47,143,55,0)
    label('SAIL',117,49)
    field(t('',selected('market_good',3),['','>']),110,6,0,1,when=False)
    field(t('',selected('market_good',3),['','SPACE']),147,6,0,9,when=False)
    field(t('',byte('market_reason'),['A/D: BUY/SELL  SPACE: TRADE','TIME LIMIT','OUT OF CASH']),4,7,0,46,True,False)
    for text,x in [('DAYS',3),('FARE',61),('CASH',121)]:label(text,x,9,on=alt)
    field(n('',byte('market_trip_days'),2),27,1,1,2,when=True)
    field(n('',byte('market_trip_days')+'\nASLB',2),85,1,1,2,when=True)
    field(n('',w('market_cash'),5),145,1,0,5,when=True)
    for text,x in [('PORT',11),('RICE',69),('ORE',111),('SPICE',151)]:label(text,x,17,True,alt)
    for i,name in enumerate(['NORTH','EAST','SOUTH','WEST']):
        label(name,12,25+i*8,on=alt)
        field(t('',selected('market_destination',i),['','>']),4,3+i,0,1,when=True)
        for j,x in enumerate((72,114,156)):field(n('',byte(f'market_quotes + {i*3+j}')),x,3+i,1,3,when=True)
    field(t('',flag('market_message'),['SPACE: SAIL   RETURN: BACK','NOT ENOUGH CASH FOR FARE']),4,7,0,46,True,True)
    bands=range(8)
    def spans(canvas):
        raw=canvas.bytes();return [(y,x,raw[y*192+x:y*192+x+64]) for y in bands for x in range(0,192,64)]
    return compile_layout(ident,dict(view=0,theme=theme),art,slots,[],regions,spans(art),spans(alt) if alt else None)

SPEC={"render":render}
