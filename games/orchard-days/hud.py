# SPDX-License-Identifier: MIT
from art import Bitmap
from visual_art import tiny,poly
from hud_layouts import n,t,b as byte,w,inc

def render(compile_layout):
 b=Bitmap();slots=[];regions=[]
 def field(f,x,band,font=0,chars=3,dark=False):
  f=dict(f,x=x,band=band,font=font,chars=chars,inverse=128 if dark else 0);slots.append(f);regions.append((x,band*8,chars*[4,6,6,11,1,6][font],16 if font in (2,3) else 8))
 b.rect(0,0,192,8,1,True)
 for text,x in [('DAY',2),('/',28),('ACT',89),('ORCHARD',131)]:tiny(b,text,x,1,0)
 field(n('',inc('orchard_day')),17,0,chars=2,dark=True);field(n('',byte('orchard_limit')),34,0,chars=2,dark=True)
 field(t('',byte('orchard_rain'),['SUN','RAIN']),52,0,chars=4,dark=True);field(n('',byte('orchard_actions')),104,0,chars=1,dark=True);field(n('',inc('stage')),178,0,chars=3,dark=True)
 for text,x,y in [('COINS',133,10),('GOAL',133,18),('SEED',135,26),('COST',137,34),('GROW',137,42),('D',184,42),('SELL',137,50)]:tiny(b,text,x,y)
 field(n('',w('orchard_coins')),164,1,1,4);field(n('',byte('orchard_goal')),170,2,1,3)
 field(t('','LDAB orchard_seed\nDECB\nCLRA',['BEAN','BERRY']),163,3,chars=5)
 for var,band in [('orchard_costs',4),('orchard_durations',5),('orchard_prices',6)]:
  field(n('',f'LDAB orchard_seed\nLDX #{var}\nABX\nLDAB 0,X\nCLRA'),174,band,chars=2)
 # Seed packet: a folded top, clear values, and the selected crop's name.
 poly(b,[(156,24),(133,24),(132,27),(132,61),(190,61),(190,27),(187,24),(185,24)])
 b.line(134,31,188,31);tiny(b,'SEED TO PLANT',136,56)
 for x in (0,64):
  b.rect(x,56,64,8,1,True);b.line(x+1,57,x+1,61,0);b.dot(x+62,62,0)
 tiny(b,'NEXT DAY',12,57,0);tiny(b,'SWAP SEED',76,57,0)
 for button,x in [(1,4),(2,68)]:
  code=f'CLRB\nLDAA orchard_button\nCMPA #{button}\nBNE @done\nTST cursor_blink_mask\nBEQ @done\nINCB\n@done:\nCLRA'
  field(t('',code,['','>']),x,7,chars=1,dark=True)
 raw=b.bytes();spans=[(band,x,raw[band*192+x:band*192+x+64]) for band in (0,7) for x in (0,64,128)]+[(band,128,raw[band*192+128:band*192+192]) for band in range(1,7)]
 for i,(x,y,W,H) in enumerate(regions):
  assert x+W<=192 and y+H<=64
  for X,Y,w0,h0 in regions[:i]:assert x+W<=X or X+w0<=x or y+H<=Y or Y+h0<=y
 return compile_layout('orchard-days',dict(view=0,theme='seed-packet'),b,slots,[],regions,spans)
SPEC={'render':render}
