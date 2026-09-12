# SPDX-License-Identifier: MIT
"""Use the standard shell with packed screens and the game's used font sizes."""
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from compose import compose
root=Path(__file__).resolve().parent
output,*sources=sys.argv[1:]
s=compose('circuit-deck',sources)
start=s.index('\nshow_title:\n')+1
end=s.index('\nupdate_menu:\n',start)
assert s[start:end].rstrip().endswith('    JMP fanfare')
s=s[:start]+(root/'title.s').read_text()+s[end:]
# This dashboard uses tiny text, normal digits and normal text (fonts 0, 1, 5).
# Remove the unused double-height font paths as well as their bitmap tables.
start=s.index('hud_glyph_large:\n')
end=s.index('hud_glyph_tiny:\n',start)
s=s[:start]+s[end:]
s=s.replace('    BNE hud_glyph_large\n','    BNE hud_glyph_dimensions\n')
start=s.index('    LDAB hud_font\n',s.index('hud_source_digit:\n'))
end=s.index('hud_source_normal:\n',start)
s=s[:start]+s[end:]
Path(output).write_text(s)
