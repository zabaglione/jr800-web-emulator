# SPDX-License-Identifier: MIT
from pathlib import Path
import sys,json,textwrap
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from art import asm_bytes
from visual_art import title
from bosses import BOSSES,portrait
from puzzle_assets import pack
out=Path(__file__).parent
# cost, damage, block, heal, poison, strength, energy, display magnitude
cards=[('STRIKE','HIT 6', [1,6,0,0,0,0,0,6]),('GUARD','BLOCK 6',[1,0,6,0,0,0,0,6]),('SPARK','HIT 2',[0,2,0,0,0,0,0,2]),('PIERCE','HIT 12',[2,12,0,0,0,0,0,12]),('WALL','BLOCK12',[2,0,12,0,0,0,0,12]),('HEAL','HEAL 5',[1,0,0,5,0,0,0,5]),('VENOM','TOXIN 3',[1,0,0,0,3,0,0,3]),('CHARGE','ENERGY1',[0,0,0,0,0,0,1,1]),('DRAIN','HIT+HP',[2,7,0,4,0,0,0,7]),('NOVA','HIT 20',[3,20,0,0,0,0,0,20]),('FOCUS','POWER 2',[1,0,0,0,0,2,0,2]),('ECHO','HIT+ARM',[1,4,4,0,0,0,0,4])]
strings={'game_name':'CIRCUIT DECK','aux1_label':'TURN END','aux2_label':'TOGGLE HELP'}
s='; SPDX-License-Identifier: MIT\n.equ STAGES,3\n.global boss_portraits\n.section .data, data\n'
for k,v in strings.items():s+=asm_bytes(k,list(v.encode())+[0])
s+=asm_bytes('enemy_health',[16,20,25,22,26,30,28,34,48])
s+=asm_bytes('card_stats',sum([v for _,_,v in cards],[]))
s+=asm_bytes('boss_portraits',sum([portrait(i) for i in range(9)],[]))
s+='\n.section .runtime, data\n'
s+=asm_bytes('boss_names',sum([list(n.ljust(10).encode())+[0] for n,_ in BOSSES],[]))
quote_lines=[textwrap.wrap(q,18) for _,q in BOSSES]
assert all(len(lines)<=2 for lines in quote_lines)
s+=asm_bytes('boss_quotes',sum([list(line.ljust(18).encode())+[0] for lines in quote_lines for line in (lines+['']*2)[:2]],[]))
s+='\n.section .data, data\n'
s+=asm_bytes('tiles',[0]*8)+asm_bytes('title_art',pack(title('CIRCUIT DECK','circuit-deck')))
(out/'assets.s').write_text(s)
(out/'cards.json').write_text(json.dumps([{'name':n,'effect':e,'stats':v} for n,e,v in cards],indent=2)+'\n')
