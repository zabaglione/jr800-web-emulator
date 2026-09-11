# SPDX-License-Identifier: MIT
from pathlib import Path
import sys,json
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from art import asm_bytes
from visual_art import title
out=Path(__file__).parent
# cost, damage, block, heal, poison, strength, energy, display magnitude
cards=[('STRIKE','HIT 6', [1,6,0,0,0,0,0,6]),('GUARD','BLOCK 6',[1,0,6,0,0,0,0,6]),('SPARK','HIT 2',[0,2,0,0,0,0,0,2]),('PIERCE','HIT 12',[2,12,0,0,0,0,0,12]),('WALL','BLOCK12',[2,0,12,0,0,0,0,12]),('HEAL','HEAL 5',[1,0,0,5,0,0,0,5]),('VENOM','TOXIN 3',[1,0,0,0,3,0,0,3]),('CHARGE','ENERGY1',[0,0,0,0,0,0,1,1]),('DRAIN','HIT+HP',[2,7,0,4,0,0,0,7]),('NOVA','HIT 20',[3,20,0,0,0,0,0,20]),('FOCUS','POWER 2',[1,0,0,0,0,2,0,2]),('ECHO','HIT+ARM',[1,4,4,0,0,0,0,4])]
strings={'game_name':'CIRCUIT DECK','aux1_label':'END TURN','aux2_label':'TOGGLE HELP','battle_label':'FIGHT','hp_label':'HP','energy_label':'EN','enemy_label':'ENEMY','shield_label':'S','block_label':'ARM','reward_label':'VICTORY - PICK A CARD','reward_help':'SPACE ADDS TO YOUR DECK','intent_attack':'NEXT: ATTACK','intent_defend':'NEXT: SHIELD','card_border':'+--------+','card_selected':'+========+','empty_card':'       ','card_cost_label':'COST','deck_help':'RETURN MENU: END TURN           '}
s='; SPDX-License-Identifier: MIT\n.equ STAGES,3\n.section .data, data\n'
for k,v in strings.items():s+=asm_bytes(k,list(v.encode())+[0])
s+=asm_bytes('enemy_health',[16,20,25,22,26,30,28,34,48])
s+=asm_bytes('card_names',sum([list(n.ljust(7).encode())+[0] for n,_,_ in cards],[]))
s+=asm_bytes('effect_names',sum([list(n.ljust(7).encode())+[0] for _,n,_ in cards],[]))
s+=asm_bytes('card_stats',sum([v for _,_,v in cards],[]))
s+=asm_bytes('tiles',[0]*8)+asm_bytes('title_art',title('CIRCUIT DECK','circuit-deck'))
(out/'assets.s').write_text(s)
(out/'cards.json').write_text(json.dumps([{'name':n,'effect':e,'stats':v} for n,e,v in cards],indent=2)+'\n')
