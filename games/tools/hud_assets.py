# SPDX-License-Identifier: MIT
"""Compile each game's HUD artwork, numeric typography and state bindings."""
import sys,json,re
from pathlib import Path
from art import Bitmap,FONT,asm_bytes
from visual_art import TINY,tiny,circle,hatch,poly
from hud_layouts import SPECS,n,inc
ROOT=Path(__file__).resolve().parents[2]
DARK={'terminal','radar','arcade','ticker','equalizer','sonar','vector'}

def make(ident):
    if ident in ('arc-duel','circuit-deck','market-harbor'):
        from hud_custom import make_custom
        return make_custom(ident,compile_layout)
    spec=SPECS[ident];view=spec['view'];theme=spec['theme'];dark=theme in DARK
    b=Bitmap();head_dark=theme not in {'folio','ink','ice','garden','water','golf','tile','pegboard'}
    b.rect(0,0,192,8,int(head_dark),True)
    sides=[(0,32),(160,32)] if view==32 else [(128 if view==0 else 0,64)]
    for x,span in sides:
        b.rect(x,8,span,56,int(dark),True)
        c=1-int(dark)
        if theme in {'terminal','vector','sonar','radar','optics'}:
            b.line(x,8,x,63,c);b.line(x+span-1,8,x+span-1,63,c)
            for y in range(9,63,5):b.line(x,y,x+2,y,c)
        elif theme in {'ticket','timetable','card'}:
            for y in range(10,64,4):b.dot(x+1,y,c);b.dot(x+span-2,y,c)
            b.line(x+4,32,x+span-5,32,c)
        elif theme in {'folio','stone','ink'}:
            b.line(x+1,9,x+1,63,c);b.line(x+3,9,x+3,63,c)
            for y in (10,53):poly(b,[(x+span-7,y),(x+span-4,y+3),(x+span-7,y+6),(x+span-10,y+3)],c)
        elif theme in {'scoreboard','stadium','arcade','equalizer'}:
            b.rect(x+1,9,span-2,23,c)
            for xx in range(x+4,x+span-3,5):b.dot(xx,34,c)
        elif theme in {'hazard','circuit','plumbing','switchboard','tower','cargo'}:
            b.rect(x,8,span,56,c)
            for yy in (9,61):b.rect(x+2,yy,2,2,c,True);b.rect(x+span-4,yy,2,2,c,True)
        elif theme in {'water','ice','golf','garden'}:
            for yy in (31,54):
                for xx in range(x+2,x+span-3,6):b.line(xx,yy,xx+3,yy+1,c)
        else:
            b.line(x,8,x,63,c);b.line(x+span-1,8,x+span-1,63,c)
    slots=[]
    def slot(field,x,band,font=0,chars=None,inverse=None):
        f=dict(field);f.update(x=x,band=band,font=font,chars=chars or max(len(v) for v in f['values']) if 'values' in f else chars or f.get('digits',3),inverse=int(dark)*128 if inverse is None else inverse)
        slots.append(f);return f
    # The title is on the title screen. In play, a short badge leaves room for useful values.
    badge_x=2 if view in (32,64) else 130
    tiny(b,spec['brand'][:6 if view==32 else 9],badge_x,1,1-int(head_dark))
    stage_x=178 if view==32 else badge_x+46
    slot(n('STAGE',inc('stage')),stage_x,0,chars=3,inverse=int(head_dark)*128)
    fields=spec['fields']
    for i,f in enumerate(fields):
        if i<4:
            if view==32:
                side=0 if i%2==0 else 160
                label_y=9 if i<2 else 33
                tiny(b,f['label'][:7],side+2,label_y,1-int(dark))
                if 'values' in f:slot(f,side+3,2 if i<2 else 5,0,chars=min(7,max(map(len,f['values']))))
                else:
                    font=2 if i<2 else 1
                    digits=f.get('digits',3);slot(f,side+(32-6*digits)//2,2 if i<2 else 5,font,chars=digits)
            else:
                side=sides[0][0]
                if i==0:
                    tiny(b,f['label'][:12],side+5,9,1-int(dark))
                    digits=f.get('digits',3)
                    font=1 if spec['meter'] else 3
                    fw=6 if font==1 else 11
                    slot(f,side+max(3,(64-fw*digits)//2),2,font,chars=digits)
                    if spec['meter']:
                        meter=dict(f,label='',limit=spec['meter']);slot(meter,side+5,3,4,chars=54)
                else:
                    band=i+3;tiny(b,f['label'][:8],side+5,band*8+1,1-int(dark))
                    count=min(6,max(map(len,f['values']))) if 'values' in f else f.get('digits',3)
                    # Right alignment accommodates counts up to 65535 without moving labels.
                    slot(f,side+59-count*4,band,0,chars=count)
        else:
            j=i-4;assert j<3,(ident,'Too many secondary values')
            x=view+j*42;tiny(b,f['label'][:3],x,1,1-int(head_dark))
            count=min(6,max(map(len,f['values']))) if 'values' in f else f.get('digits',3)
            slot(f,x+14,0,0,chars=count,inverse=int(head_dark)*128)
    state=dict(spec['status']);state['values']=[v[:7 if view==32 else 14] for v in state['values']]
    if view==32:
        slot(state,2,7,0,chars=7);tiny(b,'RETURN',163,57,1-int(dark))
    else:slot(state,sides[0][0]+4,7,0,chars=14)
    # Validate authoring bounds and field overlap independently of assembly drawing.
    regions=[]
    for f in slots:
        fw=[4,6,6,11,1][f['font']];height=16 if f['font'] in (2,3) else 8
        x,y,w=f['x'],f['band']*8,f['chars']*fw
        assert 0<=x and x+w<=192 and y+height<=64,(ident,f,'bounds')
        assert y==0 or x+w<=view or x>=view+128,(ident,f,'field overlaps play area')
        for X,Y,W,H in regions:assert x+w<=X or X+W<=x or y+height<=Y or Y+H<=y,(ident,f,'overlapping values')
        regions.append((x,y,w,height))
    return compile_layout(ident,spec,b,slots,sides,regions)

def compile_layout(ident,spec,b,slots,sides,regions,custom_spans=None,alternate_spans=None):
    view=spec['view'];theme=spec['theme']
    raw=b.bytes();spans=[]
    for x in range(0,192,64):spans.append((0,x,raw[x:x+64]))
    for band in range(1,8):
        for x,span in sides:spans.append((band,x,raw[band*192+x:band*192+x+span]))
    if custom_spans is not None:spans=custom_spans
    asm=['; SPDX-License-Identifier: MIT',f'; {ident}: {theme}, play origin {view}',f'.equ VIEW_X,{view}',f'.equ HUD_FIELDS,{len(slots)}','.section .text, code','visual_hud:','    JSR hud_begin']
    for i,f in enumerate(slots):
        code=f['code'].replace('@',f'hud_expr_{i}_')
        if 'when' in f:asm.extend(['    TST '+f['when'][0],'    '+('BEQ' if f['when'][1] else 'BNE')+f' hud_skip_{i}'])
        if f['font']==4 and isinstance(f['limit'],str):asm.extend(['    LDAB '+f['limit'],f'    STAB hud_field_{i} + 5'])
        asm.extend(('' if line.endswith(':') else '    ')+line for line in code.splitlines())
        asm.extend([f'    LDX #hud_field_{i}','    JSR '+('hud_choice' if 'values' in f else 'hud_number')])
        if 'when' in f:asm.append(f'hud_skip_{i}:')
    asm.extend(['    RTS','hud_select_background:','    LDX #hud_span_table'])
    if alternate_spans is not None:asm.extend(['    TST selection_active','    BEQ hud_background_selected','    LDX #hud_span_alternate','hud_background_selected:'])
    asm.extend(['    RTS','.section .data, data',f'view_origin: .byte {view}'])
    for i,f in enumerate(slots):
        limit=f.get('limit',0);limit=1 if isinstance(limit,str) else limit
        if 'values' in f:
            limit=len(f['values'])
            assert all(32<=ord(ch)<=(90 if f['font']==0 else 95) for v in f['values'] for ch in v),(ident,'Unsupported HUD glyph',f)
        asm.extend([f'hud_field_{i}:',f'    .byte {f["x"]},{f["band"]},{f["chars"]},{f["font"]},{f["inverse"]},{limit}',f'    .word hud_cache + {i*3},'+(f'hud_choices_{i}' if 'values' in f else '0')])
        if 'values' in f:
            data=''.join(v[:f['chars']].ljust(f['chars']) for v in f['values']).encode();asm.append(asm_bytes(f'hud_choices_{i}',data))
    patterns={}
    for label,group in [('hud_span_table',spans),('hud_span_alternate',alternate_spans)]:
        if group is None:continue
        asm.append(label+':')
        for band,x,data in group:
            key=tuple(data)
            if key not in patterns:patterns[key]=len(patterns)
            asm.extend([f'    .word hud_pixels_{patterns[key]}',f'    .byte {band},{x},{len(data)}'])
        asm.extend(['    .word 0','    .byte 0,0,0'])
    for data,i in patterns.items():asm.append(asm_bytes(f'hud_pixels_{i}',data))
    asm.append('.section .ui_fonts, data')
    tinyfont=[]
    for v in range(32,91):
        rows=TINY.get(chr(v),TINY['?']);tinyfont.extend([sum((row[x]=='1')<<y for y,row in enumerate(rows)) for x in range(3)]+[0])
    asm.append(asm_bytes('hud_font_tiny',tinyfont))
    for label,sx,sy in [('normal',1,1),('tall',1,2),('wide',2,2)]:
        data=[]
        for ch in '0123456789 ':
            glyph=Bitmap(5*sx+1,8*sy)
            for x,v in enumerate(FONT[ord(ch)-32]):
                for y in range(7):
                    if v>>y&1:glyph.rect(x*sx,y*sy,sx,sy,1,True)
            data.extend(glyph.bytes())
        asm.append(asm_bytes('hud_font_'+label,data))
    return '\n'.join(asm).rstrip()+'\n',dict(id=ident,theme=theme,view=view,fields=slots,regions=regions)

if __name__=='__main__':
    ident=sys.argv[1]
    if ident not in SPECS and ident not in ('arc-duel','circuit-deck','market-harbor'):raise SystemExit('Missing HUD design: '+ident)
    source,layout=make(ident)
    (ROOT/'games'/ident/'visuals.s').write_text(source)
    out=ROOT/'build/visual-redesign/layouts';out.mkdir(parents=True,exist_ok=True)
    (out/(ident+'.json')).write_text(json.dumps(layout,indent=2)+'\n')
