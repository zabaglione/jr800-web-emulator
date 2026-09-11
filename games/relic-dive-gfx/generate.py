# SPDX-License-Identifier: MIT
"""New art for the complete RELIC DIVE ruleset. The original game is unchanged."""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from art import Bitmap,asm_bytes
from depth_art import block8
out=Path(__file__).parent
# All 25 indices retain the original engine's meaning.
s=[Bitmap(8,8) for _ in range(25)]
s[0].rect(0,0,8,8,1,True)
s[1]=block8('stone')
# Seen floor is clear; unexplored space remains dark.
s[3].line(1,6,6,6);s[3].line(2,4,6,4);s[3].line(3,2,6,2);s[3].line(6,1,6,6)
s[4].line(1,2,6,2);s[4].line(2,4,6,4);s[4].line(3,6,6,6);s[4].line(6,1,6,6)
s[5].line(3,0,7,3);s[5].line(7,3,3,7);s[5].line(3,7,0,3);s[5].line(0,3,3,0);s[5].rect(2,2,3,3,1,True)
s[6]=Bitmap.from_rows(['..####..','..#..#..','...##...','.######.','.#.##.#.','...##...','..#..#..','.##..##.'])
s[7].rect(1,2,6,5);s[7].line(2,1,5,1);s[7].dot(2,4);s[7].dot(4,3);s[7].dot(5,5)
s[8].rect(3,0,2,2,1,True);s[8].rect(1,2,6,6);s[8].line(2,5,5,5);s[8].line(3,4,3,6)
s[9].rect(1,1,5,6);s[9].line(2,2,4,2);s[9].line(2,4,4,4);s[9].line(2,7,7,7);s[9].dot(7,6)
s[10].line(1,7,6,1);s[10].line(2,7,7,1);s[10].line(0,4,4,7)
s[11].line(1,1,6,1);s[11].line(1,1,1,5);s[11].line(6,1,6,5);s[11].line(1,5,3,7);s[11].line(6,5,3,7);s[11].line(3,2,3,6)
s[12].rect(1,1,6,6);s[12].rect(3,3,2,2,1,True)
rows=[
 ['........','........','..####..','.######.','.#.##.#.','.######.','########','........'], # slime
 ['.#....#.','..####..','.#.##.#.','..####..','.######.','.#.##.#.','..#..#..','.##..##.'], # goblin
 ['.######.','##.##.##','.######.','..####..','########','##.##.##','..#..#..','.##..##.'], # orc
 ['..####..','.######.','.#.##.#.','.######.','..####..','..####..','.#.##.#.','#.#..#.#'], # wraith
 ['........','.#..#...','..####..','..#.###.','..####..','##..#...','#...#...','........'], # rat
 ['#......#','##.##.##','########','.######.','..#..#..','...##...','........','........'], # bat
 ['..####..','.#.##.#.','..#..#..','...##...','.######.','...##...','..#..#..','..#..#..'], # skeleton
 ['..####..','.######.','##.##.##','.######.','########','##.##.##','..####..','.##..##.'], # troll
 ['..####..','.######.','.#....#.','..####..','...##...','.######.','..#..#..','.##..##.'], # thief
 ['....###.','...#.#.#','...####.','...#....','..###...','.#...#..','.#...#..','..###...'], # snake
 ['.#....#.','..####..','##.##.##','.######.','.#.##.#.','########','.#.##.#.','.#....#.'], # rust beast
 ['..##....','.#..#.#.','..##...#','.####..#','#####..#','##.####.','.#..#...','.#..#...'], # centaur
]
for i,r in enumerate(rows,13):s[i]=Bitmap.from_rows(r)
t=Bitmap();t.rect(0,0,192,64)
t.text('RELIC DIVE',7,1,3,4)
for x in [151,187]:
 t.line(x,32,x,52);t.line(x-2,32,x+2,32);t.line(x-2,52,x+2,52)
for i in range(4):t.line(158+i*4,51-i*4,182,51-i*4)
for i,label in enumerate(['EASY - 5 FLOORS','NORMAL - 10 FLOORS','HARD - 20 FLOORS']):t.text(label,18,32+i*8)
for x in [3,4]:
 for y in range(1,6):t.dot(x,40+y)
t.text('SPACE TO START',54,56);t.text('GFX',164,56)
(out/'assets.s').write_text('; SPDX-License-Identifier: MIT\n'+asm_bytes('tiles',sum((b.bytes() for b in s),[]))+asm_bytes('title_art',t.bytes()))
