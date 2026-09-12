# SPDX-License-Identifier: MIT
import sys,json,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent;levels=[]
for stage in range(12):
 r=random.Random(32000+stage);b=[1 if x in (0,14) or y in (0,6) or (x%2==0 and y%2==0) else 0 for y in range(7) for x in range(15)]
 keycols=[5+2*(stage%3),5+2*((stage+1)%4),7+2*((stage+2)%3)]
 for y,x in zip((1,3,5),keycols):b[y*15+x]=4
 candidates=[p for p in range(105) if b[p]==0 and p%15>3 and p not in (49,77,88)];r.shuffle(candidates)
 for p in candidates[:8+stage%5]:b[p]=2
 b[88]=3;assert b.count(4)==3;levels.append(b)
# Each silhouette is authored for the real 8x8 LCD cell: light falls from above left.
rows=[
 ['........','........','........','........','........','........','.#......','........'],
 ['########','#......#','#.######','#.######','#.######','#.######','#.######','########'],
 ['.######.','#......#','#.####.#','#.#..#.#','#..##..#','#.####.#','#......#','.######.'],
 ['.######.','.#....#.','.#.##.#.','.#.##.#.','.#.##.#.','.#.##.#.','.#....#.','########'],
 ['..###...','.#...#..','.#.#.#..','..###...','...#....','...###..','...#....','........'],
 ['........','..###...','.#.#.#..','..###...','...#....','...###..','........','........'],
 ['.....#.#','....#.#.','...##...','..####..','.#.####.','.#.####.','..####..','...##...'],
 ['...#....','.#.##.#.','..####..','###..###','.##..##.','..####..','.#.##.#.','....#...'],
 ['...###..','..#####.','.#..#.##','.#######','..###.#.','..####..','.#..##..','.#...#..'],
 ['..###...','..#.#...','..###...','.####...','.#.###..','...##.#.','..##.#..','..#..#..'],
]
s=[Bitmap.from_rows(r) for r in rows]
# The grid compositor highlights the actor; pre-invert its face to retain
# a dark, upright figure instead of a solid selection square during play.
s[9].p=[[1-v for v in row] for row in s[9].p]
# Continuous wall blocks: bright exposed top/left edges, heavy lower shadows.
for mask in range(16):
 b=Bitmap(8,8);b.rect(0,0,8,8,1,True)
 if not mask&1:b.line(1,1,6,1,0)
 if not mask&4:b.line(1,1,1,6,0)
 if not mask&2:
  b.dot(2,6,0);b.dot(4,6,0)
 if not mask&8:b.dot(6,3,0);b.dot(6,5,0)
 if not mask&5:b.dot(0,0,0)
 if not mask&10:b.dot(7,7,0)
 b.dot(3,3,0);b.dot(4,3,0)
 s.append(b)
assets(root,'BOMB VAULT','bomb-vault',15,7,1,1,s,12,asm_bytes('bomb_levels',sum(levels,[])),aux=('PAUSE','RESET'))
(root/'levels.json').write_text(json.dumps(levels)+'\n')
