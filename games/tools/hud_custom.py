# SPDX-License-Identifier: MIT
"""Full-width interfaces for the card table, trading ledger and artillery."""
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
    elif ident=='circuit-deck':
        theme='card-table';art.rect(0,0,192,8,1,True);art.rect(0,16,192,8,1,True)
        for text,x in [('FIGHT',1),('HP',48),('ARM',96),('EN',149)]:label(text,x,1,True)
        for var,x in [('battle',26),('hp',62),('block',115),('energy',164)]:field(n('',inc(var) if var=='battle' else byte(var)),x,0,1,3,True)
        label('ENEMY',2,9);label('ARMOR',76,9);label('INTENT',133,9)
        field(n('',byte('enemy_hp')),37,1,1,3);field(n('',byte('enemy_block')),105,1,1,3)
        intent='LDAB #6\nLDAA intent\nCMPA #1\nBEQ @done\nLDAB battle\nADDB #4\nADDB stage\nTSTA\nBEQ @done\nADDB #3\n@done:\nCLRA'
        field(n('',intent),164,1,1,3)
        state='LDAB intent\nCMPB #1\nBEQ @shield\nCLRB\nBRA @mode\n@shield:\nLDAB #1\n@mode:\nTST battle_mode\nBEQ @help\nLDAB #2\n@help:\nTST card_help\nBEQ @done\nLDAB #3\n@done:\nCLRA'
        field(t('',state,['NEXT: ENEMY ATTACK','NEXT: ENEMY SHIELD','REWARD: CHOOSE ONE','RETURN MENU: END TURN']),6,2,0,44,True)
        names=['STRIKE','GUARD','SPARK','PIERCE','WALL','HEAL','VENOM','CHARGE','DRAIN','NOVA','FOCUS','ECHO','--']
        effects=['HIT 6','BLOCK 6','HIT 2','HIT 12','BLK 12','HEAL 5','TOXIN 3','ENERGY1','HIT+HP','HIT 20','POWER 2','HIT+ARM','']
        for i in range(3):
            x=i*64;art.rect(x,24,64,40,1);art.rect(x+1,24,62,8,1,True);art.rect(x+1,56,62,8,1,True)
            label('COST',x+4,25,True)
            card=f'LDAB hand + {i}\nTST battle_mode\nBEQ @hand\nLDAB rewards + {i}\n@hand:\nCMPB #255\nBNE @valid\nLDAB #12\n@valid:\nCLRA'
            cost=card+'\nCMPB #12\nBEQ @empty\nASLB\nASLB\nASLB\nLDX #card_stats\nABX\nLDAB 0,X\nCLRA\nBRA @costdone\n@empty:\nLDD #0\n@costdone:'
            field(n('',cost),x+44,3,1,2,True)
            field(t('',card,names),x+5,4,5,8)
            field(t('',card,effects),x+5,5,5,8)
            label('SPACE: PLAY' if i==1 else ('< SELECT' if i==0 else 'SELECT >'),x+6,49)
            field(t('',selected('selected',i),['','> SELECT <']),x+4,7,0,14,True)
        bands=range(8)
    else:
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
        label('RETURN: SAIL',122,49)
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
