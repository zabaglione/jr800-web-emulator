# SPDX-License-Identifier: MIT
import sys,random,json,re
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from art import Bitmap,asm_bytes
from visual_art import title
from design import useful_sources
out=Path(__file__).parent
from puzzle_assets import campaign, level_records, challenge_data
levels=campaign(out)
for index,level in enumerate(levels,1):
    initial=level['initial']
    assert useful_sources(initial['board'],initial['sources'],level['bonus_cells'])==initial['sources'],f'Stage {index}: purposeless emitter'

def wall(mask):
    # Joined dark shielding panels, with a light bevel on exposed upper/left edges.
    b=Bitmap(8,8);b.rect(0,0,8,8,1,True)
    if not mask&1:b.line(0,0,0,7,0);b.line(2,2,2,5,0)
    if not mask&2:b.line(7,0,7,7,0)
    if not mask&4:b.line(0,0,7,0,0);b.line(2,2,5,2,0)
    if not mask&8:b.line(0,7,7,7,0)
    return b.bytes()

tiles=[[0]*8,wall(0),[0,64,32,16,8,4,2,0],[0,2,4,8,16,32,64,0],[0,60,66,90,90,66,60,0],
[0,24,24,90,60,24,0,0],[0,8,24,62,62,24,8,0],[0,24,60,90,24,24,0,0],[0,16,24,124,124,24,16,0],
[0,60,126,102,102,126,60,0],[8]*8,[0,0,0,255,0,0,0,0],[8,8,8,255,8,8,8,8]]
tiles.extend(wall(mask) for mask in range(1,16))

s='; SPDX-License-Identifier: MIT\n.equ STAGES,40\n.section .data, data\ngame_name: .byte "MIRROR LINK",0\naux1_label: .byte "UNDO ROTATION",0\naux2_label: .byte "RESET MIRRORS",0\n'+asm_bytes('tiles',sum(tiles,[]))+asm_bytes('title_art',title('MIRROR LINK','mirror-link'))+level_records([[len(x['initial']['sources'])]+x['initial']['sources']+[0]*(3-len(x['initial']['sources']))+x['initial']['board'] for x in levels])+challenge_data(out,levels)
s=re.sub(r'\.byte "([^"\n]*)",0',lambda m:'.byte '+','.join(str(c) for c in m[1].encode())+',0 ; '+m[1],s)
(out/'assets.s').write_text(s)
