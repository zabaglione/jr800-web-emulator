# SPDX-License-Identifier: MIT
"""Original reverse-pull puzzles; replay certificates use real movement rules."""
import sys,random,json,re
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from art import asm_bytes
from visual_art import title
out=Path(__file__).parent
levels=[];solutions=[]
for level in range(20):
    rng=random.Random(80300+level)
    while True:
        # Irregular small rooms, embedded in the 16x7 cell canvas.
        floor={y*16+x for y in range(1,6) for x in range(1,10+level%3)}
        for p in rng.sample(sorted(floor),4+level%5):floor.remove(p)
        goals=set(rng.sample(sorted(floor),2 if level<10 else 3))
        boxes=set(goals);player=rng.choice(sorted(floor-boxes));reverse=[];pulls=0
        for _ in range(180+level*8):
            moves=[]
            for d in (-16,16,-1,1):
                if player+d in floor-boxes:moves.append(d)
            if not moves:break
            d=rng.choice(moves)
            if player-d in boxes:
                boxes.remove(player-d);boxes.add(player);pulls+=1
            player+=d;reverse.append(d)
        if pulls>=6 and len(boxes-goals)>=2:break
    board=[1 if p not in floor else (2 if p in goals else 0) for p in range(112)]
    for p in boxes:board[p]|=4
    # Replay the inverse walk, including reverse steps that merely repositioned.
    p=player;b=set(boxes);sol=[]
    for d in reversed(reverse):
        d=-d;q=p+d
        assert q in floor
        if q in b:
            assert q+d in floor-b
            b.remove(q);b.add(q+d)
        p=q;sol.append({-16:'up',16:'down',-1:'left',1:'right'}[d])
    assert b==goals
    levels.append([player]+board);solutions.append(sol)
tiles=[
 [0]*8,[255,129,153,165,165,153,129,255],
 [0,0,24,36,36,24,0,0],[0]*8,
 [0,126,66,90,90,66,126,0],[0]*8,
 [0,126,90,102,102,90,126,0],[0]*8,
 [0,24,60,126,90,24,36,0],
]
s='; SPDX-License-Identifier: MIT\n.equ STAGES,20\n.section .data, data\ngame_name: .byte "BOX SHIFT",0\naux1_label: .byte "UNDO ONE MOVE",0\naux2_label: .byte "HELP",0\n'
s+=asm_bytes('tiles',sum(tiles,[]))+asm_bytes('title_art',title('BOX SHIFT','box-shift'))
s+=asm_bytes('levels',sum(levels,[]))
s=re.sub(r'\.byte "([^"\n]*)",0',lambda m:'.byte '+','.join(str(c) for c in m[1].encode())+',0 ; '+m[1],s)
(out/'assets.s').write_text(s)
(out/'solutions.json').write_text(json.dumps(solutions)+'\n')
