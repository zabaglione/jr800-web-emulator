# SPDX-License-Identifier: MIT
from pathlib import Path
import sys,json,random
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from art import asm_bytes
from visual_art import title,Bitmap
from puzzle_assets import campaign,level_records,challenge_data
out=Path(__file__).parent;levels=campaign(out)
base=[[0]*8,[255,129,153,165,165,153,129,255],[255,129,189,165,165,189,129,255],[255,129,145,169,169,145,129,255],[255,129,185,169,169,185,129,255],[255,129,189,165,165,189,129,255]]
for d in range(4):
 b=Bitmap(8,8);b.rect(0,0,8,8)
 if d==0:b.line(2,3,5,3);b.line(3,1,5,3);b.line(3,5,5,3)
 if d==1:b.line(3,2,3,5);b.line(1,3,3,5);b.line(5,3,3,5)
 if d==2:b.line(2,3,5,3);b.line(2,3,4,1);b.line(2,3,4,5)
 if d==3:b.line(3,2,3,5);b.line(1,4,3,2);b.line(5,4,3,2)
 base.append(b.bytes())
base += [[255,129,219,165,165,219,129,255],[255,129,189,195,195,189,129,255]]
from depth_art import block8
base[5]=block8('metal').bytes()
base[10]=block8('metal').bytes()
base[11]=block8('stone').bytes()
# Retain the original belt arrows; a shaded bottom/right edge seats each belt.
for tile in range(6,10):base[tile][7]|=0xFE;base[tile][6]|=0x80
tiles=[]
for item in range(5):
 for tile in base:
  a=tile.copy()
  if item:
   for x in range(2,6):a[x]&=~60
   icon=[[0,24,24,0],[24,36,36,24],[60,60,60,60],[60,36,36,60]][item-1]
   for x,v in enumerate(icon,2):a[x]|=v
  tiles+=a
strings={'game_name':'POCKET FACTORY','aux1_label':'SELECT TOOL','aux2_label':'RUN / PAUSE','factory_heading':'FACTORY','build_label':'BUILD   ','run_label':'RUNNING ','tool_label':'TOOL    ','choose_label':'CHOOSE  ','shipment_label':'SHIP/GOAL','factory_return':'RETURN'}
s='; SPDX-License-Identifier: MIT\n.equ STAGES,40\n.section .data, data\n'
for k,v in strings.items():s+=asm_bytes(k,list(v.encode())+[0])
s+=asm_bytes('tool_names',sum([list(x.ljust(8).encode())+[0] for x in ['BELT >','BELT V','BELT <','BELT ^','PRESS A','PRESS B','ERASE']],[]))
s+=asm_bytes('flow_deltas',[0,1,1,0,0,0,1,16,255,240,1,1])+asm_bytes('tiles',tiles)+asm_bytes('title_art',title('POCKET FACTORY','pocket-factory'))+level_records([s['initial']['targets']+[s['initial']['board'].index(t) for t in (1,2,3,4)]+[sum(1<<bit for bit in range(8) if s['initial']['board'][byte*8+bit]==5) for byte in range(14)] for s in levels])+challenge_data(out,levels)
(out/'assets.s').write_text(s)
