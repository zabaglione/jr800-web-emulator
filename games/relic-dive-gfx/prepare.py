# SPDX-License-Identifier: MIT
"""Compose a graphics edition from the unchanged original gameplay modules."""
import argparse,re,sys
from pathlib import Path
root=Path(__file__).resolve().parents[2];here=Path(__file__).parent;original=root/'sdk/examples/lcd/07-relic-dive'
sys.path.insert(0,str(root/'games/tools'))
from art import asm_bytes
from puzzle_assets import pack
p=argparse.ArgumentParser();p.add_argument('output',type=Path);a=p.parse_args()
def once(s,old,new):
 if s.count(old)!=1:raise ValueError('Original source boundary changed: '+old[:60])
 return s.replace(old,new)
main=(original/'main.s').read_text()
main=once(main,'    JSR init\n','    JSR init\n    JSR gfx_init\n')
main=once(main,'idle:\n    JSR poll_key','idle:\n    JSR poll_key\n    JSR gfx_tick\n    TSTA\n    BEQ gfx_idle_continue\n    JMP redraw\ngfx_idle_continue:')
main=once(main,'dispatch:\n    STAA G_KEY','dispatch:\n    TST gfx_clear_active\n    BEQ gfx_dispatch\n    CLR G_PENDING\n    JMP idle\ngfx_dispatch:\n    CMPA #5\n    BNE gfx_no_chirp\n    PSHA\n    JSR gfx_chirp\n    PULA\ngfx_no_chirp:\n    STAA G_KEY')
main=main.replace('    JSR present_begin','    JSR dirty_begin').replace('    JSR present_next','    JSR dirty_next')
main=once(main,'    CMPA #6\n    BEQ world_done','    CMPA #6\n    BEQ open_menu')
main=once(main,'poll_confirm:\n    LDAA $0F7F\n    BITA #64','poll_confirm:\n'+(here/'letter-input.s').read_text()+'    LDAA $0FEF\n    BITA #1')
main=once(main,'poll_back:\n    LDAA $0FEF\n    BITA #1','poll_back:\n    LDAA $0F7F\n    BITA #64')
main=main.replace('    BRA poll_sample','    JMP poll_sample')
ui=(original/'ui.s').read_text();ui=once(ui,'render_screen:\n    JSR clear','render_screen:\n    LDAA G_MODE\n    CMPA #9\n    BNE gfx_render_normal\n    TST gfx_clear_done\n    BNE gfx_render_normal\n    JMP gfx_win_scene\ngfx_render_normal:\n    JSR gfx_clear');start=ui.index('render_title:\n');end=ui.index('render_suspend:\n',start)
ui=ui[:start]+'render_title:\n    JMP gfx_title\n'+ui[end:]
ui=once(ui,'hud_done:\n    RTS','hud_done:\n    JMP gfx_hud_finish')
start=ui.index('draw_tile:\n');end=ui.index('; X -> pointer table',start)
ui=ui[:start]+'draw_tile:\n    JMP gfx_draw_tile\n\n'+ui[end:]
assets=(original/'assets.s').read_text();art=(here/'assets.s').read_text()
tiles=art.split('tiles:\n')[1].split('title_art:')[0]
assets=re.sub(r'tiles:\n.*?(?=difficulty_names:)',lambda _: 'tiles:\n'+tiles,assets,flags=re.S)
# Original rules/data stay shared. These are display strings only.
labels={'s_keys':'WASD / 2468 MOVE  SPACE START','s_menu_keys':'SPACE SELECT  RETURN BACK','s_resume':'SPACE TO CONTINUE','s_end':'SPACE FOR NEW GAME','help_lines_1':'RETURN MENU / BACK  SPACE OK','messages_0':'RETURN MENU - WASD OR 2468 MOVE'}
for label,text in labels.items():assets=re.sub(r'^'+label+r': \.byte[^\n]*',lambda _:label+': .byte '+','.join(map(str,text.encode()))+',0',assets,flags=re.M)
assets=re.sub(r'^s_(title|goal|keys):[^\n]*\n','',assets,flags=re.M)
pixels=[int(x,16) for x in re.findall(r'\$([0-9A-F]{2})',art.split('title_art:')[1])]
code=(original/'constants.inc').read_text()+main
for name in ['terrain.s','dungeon.s','turns.s','items.s']:code+=(original/name).read_text()
code+=ui+assets+(here/'graphics.s').read_text()+'\n.section .gfx_data, data\n'+asm_bytes('gfx_title_data',pack(pixels))
dirty=(root/'sdk/lib/lcd/dirty.s').read_text().replace('.section .text, code','.section .gfx_dirty, code').replace('.section .bss, bss','.section .gfx_state, bss')
code+=dirty
sound=(root/'sdk/lib/sound.s').read_text();tone=sound[sound.index('sound_tone:'):sound.index('; X = delay iterations; zero')]
code+='\n.global sound_tone\n.global sound_port\n.section .gfx_sound, code\n'+tone+'\n.section .gfx_state, bss\nsound_port: .space 1\nsound_period: .space 2\nsound_count: .space 2\n'
a.output.write_text(code)
