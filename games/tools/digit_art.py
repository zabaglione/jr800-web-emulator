# SPDX-License-Identifier: MIT
"""Original replacement digit strokes; existing HUD glyph slots and byte counts are reused."""
from art import FONT
SEGMENT={'step-strike','pocket-factory','number-rail','ricochet-ops','star-patrol','orbit-guard','target-range','rail-dispatch','beat-step','circuit-deck'}
BOOK={'knight-tour','reversi-mini','ace-stack','suit-run','micro-rogue','relay-quest','five-stones'}
SOFT={'peg-rescue','slide-nine','dice-hold','orchard-days','river-hop','line-four'}
BOOK_ROWS=[
['01110','11011','10001','10001','10001','11011','01110'],
['00100','01100','00100','00100','00100','00100','11111'],
['01110','10001','00001','00010','00100','01000','11111'],
['11110','00001','00001','01110','00001','00001','11110'],
['00010','00110','01010','10010','11111','00010','00111'],
['11111','10000','10000','11110','00001','00001','11110'],
['00110','01000','10000','11110','10001','10001','01110'],
['11111','10001','00010','00100','00100','00100','01110'],
['01110','10001','10001','01110','10001','10001','01110'],
['01110','10001','10001','01111','00001','00010','01100']]
SOFT_ROWS=[
['01110','10001','10001','10001','10001','10001','01110'],
['00100','01100','00100','00100','00100','00100','01110'],
['01110','10001','00001','00110','01000','10000','11111'],
['11110','00001','00001','01110','00001','00001','11110'],
['10010','10010','10010','11111','00010','00010','00010'],
['11111','10000','10000','11110','00001','00001','11110'],
['01110','10000','10000','11110','10001','10001','01110'],
['11111','00001','00010','00100','00100','00100','00100'],
['01110','10001','10001','01110','10001','10001','01110'],
['01110','10001','10001','01111','00001','00001','01110']]

def face(ident):return 'segment' if ident in SEGMENT else 'book' if ident in BOOK else 'soft' if ident in SOFT else 'standard'
def digits(ident):
    style=face(ident)
    if style=='standard':return [FONT[ord(c)-32] for c in '0123456789 ']
    if style=='segment':
        rows=[]
        for segments in ('abcdef','bc','abdeg','abcdg','bcfg','acdfg','acdefg','abc','abcdefg','abcdfg'):
            grid=[['0']*5 for _ in range(7)]
            for segment in segments:
                if segment in 'adg':
                    y={'a':0,'g':3,'d':6}[segment]
                    for x in range(1,4):grid[y][x]='1'
                else:
                    x=4 if segment in 'bc' else 0;begin=1 if segment in 'bf' else 4
                    for y in (begin,begin+1):grid[y][x]='1'
            rows.append([''.join(row) for row in grid])
    else:rows=BOOK_ROWS if style=='book' else SOFT_ROWS
    data=[[sum((rows[n][y][x]=='1')<<y for y in range(7)) for x in range(5)] for n in range(10)]+[[0]*5]
    assert len({tuple(g) for g in data})==11
    return data

def draw_digits(b,text,x,y,ident,sx=1,sy=1,c=1):
    glyphs=digits(ident)
    for ch in text:
        glyph=glyphs[int(ch)] if ch.isdigit() else glyphs[10]
        for xx,col in enumerate(glyph):
            for yy in range(7):
                if col>>yy&1:b.rect(x+xx*sx,y+yy*sy,sx,sy,c,True)
        x+=6*sx
