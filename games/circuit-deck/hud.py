# SPDX-License-Identifier: MIT
"""The game's full-width battle dashboard and directly selectable turn button."""
from pathlib import Path
import sys,re
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
sys.path.insert(0,str(Path(__file__).resolve().parent))
from art import Bitmap,asm_bytes
from visual_art import tiny
from hud_layouts import n,t,b as byte,inc
from hud_assets import compile_layout
from bosses import BOSSES
from puzzle_assets import pack
art=Bitmap();slots=[];regions=[]
def field(code,x,band,chars,font=0,values=None,dark=False):
 f=(t('',code,values) if values is not None else n('',code,chars))
 f.update(x=x,band=band,chars=chars,font=font,inverse=128 if dark else 0)
 w=chars*[4,6,6,11,1,6][font];r=(x,band*8,w,8)
 assert x+w<=192 and band<8
 for X,Y,W,H in regions:assert x+w<=X or X+W<=x or band*8+8<=Y or Y+H<=band*8,(r,(X,Y,W,H))
 slots.append(f);regions.append(r)
def label(text,x,y,dark=False):tiny(art,text,x,y,0 if dark else 1)
def diamond(b,x,y,r=2,c=1):
 for dy in range(-r,r+1):b.line(x-r+abs(dy),y+dy,x+r-abs(dy),y+dy,c)
def rail(b,x,X,y,c=1):
 b.line(x,y,X,y,c);b.line(x,y+3,X,y+3,c)
 for q in (x+4,X-4):
  b.rect(q-1,y-1,3,6,c,True);b.dot(q,y+1,1-c)
def panel(b,x,y,w,h):
 b.line(x+2,y,x+w-3,y);b.line(x+2,y+h-1,x+w-3,y+h-1)
 for q in (x,x+w-1):b.line(q,y+2,q,y+h-3)
 for X,Y,dx,dy in ((x,y,1,1),(x+w-1,y,-1,1),(x,y+h-1,1,-1),(x+w-1,y+h-1,-1,-1)):
  b.line(X+dx*2,Y,X,Y+dy*2)
art.rect(0,0,192,8,1,True)
label('CIRCUIT DECK',2,1,True);label('BOSS',113,1,True);label('/9',144,1,True);label('EN',162,1,True)
rail(art,56,105,2,0)
# Mechanical dividers frame the portrait and keep clear of every text column.
art.line(52,9,52,22);art.line(54,10,54,21);art.line(52,25,52,29)
art.rect(52,22,3,3);art.line(125,9,125,21);art.line(127,10,127,21)
art.rect(54,8,67,8,1,True);art.line(121,8,124,11)
art.line(184,10,188,10);art.line(184,13,188,13);art.rect(189,9,2,6,1,True)
art.line(156,20,181,20);diamond(art,185,20,2)
art.rect(54,24,138,8,1,True)
field(inc('battle'),135,0,1,dark=True)
field(byte('deck_hp'),169,1,2,1)
field(byte('deck_block'),144,2,2)
field(byte('energy'),175,0,2,dark=True)
field(byte('deck_enemy_hp'),68,2,2,1)
field(byte('deck_enemy_block'),108,2,2)
label('YOU HP',130,9);label('ARM',130,17);label('HP',56,17);label('SHD',92,17)
field(byte('battle'),56,1,10,values=[b[0] for b in BOSSES],dark=True)
intent='LDAA intent\nCMPA #1\nBEQ @shield\nLDAB battle\nADDB #4\nADDB stage\nTSTA\nBEQ @amount\nADDB #3\n@amount:\nCLRA\nBRA @done\n@shield:\nLDD #6\n@done:'
field(intent,173,3,2,dark=True)
slots[-1]['when']=('deck_reward_view',False)
field('LDD #0\nTST deck_reward_view\nBEQ @combat\nLDAB #2\nBRA @done\n@combat:\nLDAA intent\nCMPA #1\nBNE @done\nINCB\n@done:\nCLRA',56,3,26,values=['NEXT: ATTACK','NEXT: SHIELD','BOSS DEFEATED'],dark=True)
field(byte('deck_message'),2,7,22,values=['DOWN:END  SPACE:PLAY','BLOCKED BY SHIELD','SHIELD TOOK THE HIT','ENEMY ATTACK','YOUR SHIELD HELD','ENEMY SHIELD +6','POISON DAMAGE','PICK CARD / NEXT BOSS','NOT ENOUGH ENERGY','ALL NINE BOSSES DEFEATED','STRIKE - ENEMY HIT','GUARD - SHIELD UP','SPARK - ENEMY HIT','PIERCE - HEAVY HIT','WALL - SHIELD UP','HEAL - RECOVER HP','VENOM - POISON ADDED','CHARGE - ENERGY UP','DRAIN - HIT AND HEAL','NOVA - MASSIVE HIT','FOCUS - ATTACK POWER UP','ECHO - HIT AND SHIELD','EN PAYS COST / DOWN END'],dark=True)
names=['STRIKE','GUARD','SPARK','PIERCE','WALL','HEAL','VENOM','CHARGE','DRAIN','NOVA','FOCUS','ECHO','--']
effects=['HIT 6','ARM +6','HIT 2','HIT 12','ARM +12','HP +5','POISON3','EN +1','HIT7 HP4','HIT 20','POWER+2','HIT4 ARM4','']
for i in range(3):
 x=i*48;panel(art,x,32,47,24);art.rect(x+2,32,43,8,1,True);label('C',x+29,48)
 diamond(art,x+39,35,2,0)
 art.line(x+43,42,x+43,50);art.dot(x+43,52)
 art.line(x+25,54,x+32,54);art.line(x+25,52,x+27,50)
 card=f'LDAB hand + {i}\nTST deck_reward_view\nBEQ @hand\nLDAB rewards + {i}\n@hand:\nCMPB #255\nBNE @valid\nLDAB #12\n@valid:\nCLRA'
 field(card,x+3,4,7,values=names,dark=True)
 field(card,x+3,5,9,values=effects)
 cost=card+'\nCMPB #12\nBEQ @empty\nASLB\nASLB\nASLB\nLDX #card_stats\nABX\nLDAB 0,X\nCLRA\nBRA @done\n@empty:\nLDD #0\n@done:'
 field(cost,x+37,6,1)
 chosen=f'LDAB selected\nCMPB #{i}\nBEQ @yes\nLDD #0\nBRA @done\n@yes:\nLDD #0\nTST cursor_blink_mask\nBEQ @done\nLDD #1\n@done:'
 field(chosen,x+3,6,5,values=['','PLAY'])
panel(art,147,32,45,24);art.rect(149,32,41,16,1,True)
button='LDD #0\nTST deck_reward_view\nBNE @reward\nLDAB selected\nCMPB #3\nBNE @plain\nTST cursor_blink_mask\nBEQ @plain\nLDD #1\nBRA @done\n@plain:\nLDD #0\nBRA @done\n@reward:\nLDD #2\n@done:'
field(button,149,5,10,values=[' TURN END ','>TURN END<','PICK CARD '],dark=True)
field(byte('deck_reward_view'),157,4,6,values=['ACTION','REWARD'],dark=True)
field(byte('deck_reward_view'),157,6,6,values=[' SPACE','HP +12'])
art.rect(0,56,192,8,1,True);rail(art,101,190,58,0)
s,layout=compile_layout('circuit-deck',dict(view=0,theme='boss-battle'),art,slots,[],regions,[])
# No double-height fields are used. Their dispatch paths are removed by compose.py.
s=s[:s.index('hud_font_tall:')]
assert {f['font'] for f in slots}=={0,1}
# Shared card names/effects occupy one table each, irrespective of hand slot.
seen={}
for m in list(re.finditer(r'^(hud_choices_\d+):\n((?:    \.byte[^\n]*\n)+)',s,re.M)):
 data=m[2]
 if data in seen:s=s.replace(m[0],'').replace(m[1],seen[data])
 else:seen[data]=m[1]
arrival=Bitmap()
arrival.rect(0,0,192,8,1,True);arrival.text('BOSS INCOMING',57,0,c=0)
rail(arrival,2,47,2,0);rail(arrival,145,190,2,0)
panel(arrival,99,8,93,48);arrival.line(101,12,101,51)
tiny(arrival,'SIGNAL DETECTED',106,10)
rail(arrival,105,185,26)
arrival.line(188,32,188,47);arrival.rect(187,48,3,3)
arrival.rect(0,56,192,8,1,True);rail(arrival,2,94,58,0)
tiny(arrival,'TRANSMISSION',114,57,0)
result=Bitmap()
result.rect(0,0,192,8,1,True);result.text('CIRCUIT DECK',60,0,c=0)
rail(result,2,51,2,0);rail(result,140,190,2,0)
result.rect(0,56,192,8,1,True);rail(result,2,190,58,0)
panel(result,32,14,128,34)
for flip in (False,True):
 def px(x):return 191-x if flip else x
 for x,y,w,h in ((4,19,16,27),(14,23,16,27)):
  for Y in range(y,y+h):
   for X in range(x,x+w):result.dot(px(X),Y, int(X in (x,x+w-1) or Y in (y,y+h-1)))
  for d in range(-3,4):
   for X in range(x+8-3+abs(d),x+8+4-abs(d)):result.dot(px(X),y+15+d)
  for Y in range(y+3,y+6):result.dot(px(x+3),Y)
 for y in (11,53):
  result.line(px(2),y,px(24),y);result.line(px(24),y,px(28),y+(3 if y==11 else -3))
s+='\n.section .data, data\n'
for name,b in [('battle',art),('arrival',arrival),('result',result)]:s+=asm_bytes('deck_'+name+'_art',pack(b.bytes()))
SPEC={'render':lambda compile_layout: (s,layout)}
if __name__=='__main__':Path(__file__).with_name('visuals.s').write_text(s)
