# SPDX-License-Identifier: MIT
"""Original guard puzzles, solved with a bounded independent state search."""
import sys,random,json,re,heapq
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from art import asm_bytes
from visual_art import title
from puzzle_assets import campaign,level_records,challenge_data
out=Path(__file__).parent;levels=campaign(out)
tiles=[[0]*8,[255,129,189,165,165,189,129,255],[0,24,60,126,90,24,36,0],[0,24,60,126,90,24,36,0],[0,24,60,126,90,24,36,0],[0,24,60,126,90,24,36,0],[0,8,28,8,0,0,0,0],[0,0,0,28,8,0,0,0],[0,60,90,126,24,60,66,0]]
for d in range(4):
 x,y=3,3
 for _ in range(3):
  x+=[1,0,-1,0][d];y+=[0,1,0,-1][d]
  if 0<=x<8 and 0<=y<8:tiles[2+d][x]|=1<<y
from depth_art import block8
tiles[1]=block8('metal').bytes()
s='; SPDX-License-Identifier: MIT\n.equ STAGES,40\n.section .data, data\ngame_name: .byte "STEP STRIKE",0\naux1_label: .byte "WAIT ONE TURN",0\naux2_label: .byte "HELP",0\n'+asm_bytes('tiles',sum(tiles,[]))+asm_bytes('title_art',title('STEP STRIKE','step-strike'))+level_records([[s['initial']['start'],len(s['initial']['guards'])]+s['initial']['guards']+[255]*(4-len(s['initial']['guards']))+s['initial']['board'] for s in levels])+challenge_data(out,levels)
s=re.sub(r'\.byte "([^"\n]*)",0',lambda m:'.byte '+','.join(str(c) for c in m[1].encode())+',0 ; '+m[1],s)
(out/'assets.s').write_text(s)
