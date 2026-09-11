# SPDX-License-Identifier: MIT
"""Original reverse-pull puzzles; replay certificates use real movement rules."""
import sys,random,json,re
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from art import asm_bytes
from visual_art import title
out=Path(__file__).parent
from puzzle_assets import campaign, level_records, challenge_data
levels=campaign(out)
tiles=[
 [0]*8,[255,129,153,165,165,153,129,255],
 [0,0,24,36,36,24,0,0],[0]*8,
 [0,126,66,90,90,66,126,0],[0]*8,
 [0,126,90,102,102,90,126,0],[0]*8,
 [0,24,60,126,90,24,36,0],
]
from depth_art import block8
tiles[1]=block8('stone').bytes()
tiles[4]=block8('crate').bytes()
settled=block8('crate');settled.rect(1,3,4,4,1,True);settled.dot(2,4,0);settled.dot(3,5,0);tiles[6]=settled.bytes()
s='; SPDX-License-Identifier: MIT\n.equ STAGES,40\n.section .data, data\ngame_name: .byte "BOX SHIFT",0\naux1_label: .byte "UNDO ONE MOVE",0\naux2_label: .byte "HELP",0\n'
s+=asm_bytes('tiles',sum(tiles,[]))+asm_bytes('title_art',title('BOX SHIFT','box-shift'))
s+=level_records([x['initial']['board'] for x in levels])+asm_bytes('start_cells',[x['initial']['start'] for x in levels])+challenge_data(out,levels)
s=re.sub(r'\.byte "([^"\n]*)",0',lambda m:'.byte '+','.join(str(c) for c in m[1].encode())+',0 ; '+m[1],s)
(out/'assets.s').write_text(s)
