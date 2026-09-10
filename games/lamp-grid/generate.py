# SPDX-License-Identifier: MIT
import sys,json,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets, Bitmap, asm_bytes

def flip(board,p):
    for q in [p]+[y*5+x for x,y in ((p%5-1,p//5),(p%5+1,p//5),(p%5,p//5-1),(p%5,p//5+1)) if 0<=x<5 and 0<=y<5]:board[q]^=1

def solve(board):
    answers=[]
    for mask in range(32):
        a=board[:];route=[]
        for x in range(5):
            if mask>>x&1:flip(a,x);route.append(x)
        for y in range(1,5):
            for x in range(5):
                if a[(y-1)*5+x]:flip(a,y*5+x);route.append(y*5+x)
        if not any(a):answers.append(route)
    return min(answers,key=len) if answers else None
r=random.Random(8007);levels=[];solutions=[]
for stage in range(20):
    while True:
        a=[0]*25
        for p in r.sample(range(25),3+stage//2):flip(a,p)
        sol=solve(a)
        if a not in levels and sol and len(sol)>=min(3+stage//3,9):break
    levels.append(a);solutions.append(sol)
sprites=[]
for on in (0,1):
    b=Bitmap(16,8);b.rect(1,0,14,8);b.rect(4,2,8,4,on,True)
    if not on:b.dot(7,3);b.dot(8,4)
    sprites.append(b)
assets(Path(__file__).parent,'LAMP GRID','lamp-grid',5,5,2,1,sprites,20,asm_bytes('levels',sum(levels,[])))
Path(__file__).with_name('solutions.json').write_text(json.dumps(solutions)+'\n')
