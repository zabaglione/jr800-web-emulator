# SPDX-License-Identifier: MIT
from pathlib import Path
import sys,math,json
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from art import asm_bytes
from visual_art import title
out=Path(__file__).parent;terrains=[]
for n in range(6):
 h=[]
 for x in range(192):
  if n==0:y=48-int(13*math.sin(x*math.pi/191)**2)
  elif n==1:y=52-int(19*max(0,1-abs(x-95)/45))
  elif n==2:y=42+int(13*math.sin(x*math.pi/191)**2)
  elif n==3:y=45+int(7*math.sin(x/15))
  elif n==4:y=49-(17 if 70<x<120 else 0)
  else:y=51-int(16*abs(math.sin(x/30)))
  h.append(50 if x<25 or x>166 else y)
 terrains.append(h)
strings={'game_name':'ARC DUEL','aux1_label':'NEXT TERRAIN','aux2_label':'TOGGLE HELP','arc_hud':'A    P    W     HP    CPU  ','arc_aim':'AIM     ','arc_fly':'SHOT    ','arc_cpu':'CPU AIM','arc_round':'ROUND OVER - SPACE','arc_help1':'W/S ANGLE  A/D POWER','arc_help2':'SPACE FIRE  RETURN MENU','arc_blank':'                                ','arc_score':'WINS','arc_terrain':'LAND'}
s='; SPDX-License-Identifier: MIT\n.equ STAGES,3\n.section .data, data\n'
for k,v in strings.items():s+=asm_bytes(k,list(v.encode())+[0])
s+=asm_bytes('velocity_cos',[round(128*math.cos(math.radians(15+5*a))) for a in range(13)])
s+=asm_bytes('velocity_sin',[round(128*math.sin(math.radians(15+5*a))) for a in range(13)])
s+=asm_bytes('crater_depth',[round(math.sqrt(144-x*x)) for x in range(13)])
s+=asm_bytes('ground_masks',[255,254,252,248,240,224,192,128])+asm_bytes('pixel_masks',[1,2,4,8,16,32,64,128])
s+=asm_bytes('tiles',[0]*8)+asm_bytes('title_art',title('ARC DUEL','arc-duel'))+asm_bytes('terrain_data',sum(terrains,[]))
(out/'assets.s').write_text(s);(out/'terrains.json').write_text(json.dumps(terrains)+'\n')
