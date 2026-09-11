# SPDX-License-Identifier: MIT
import sys,json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap,asm_bytes
root=Path(__file__).parent
patterns=[[0,1,2,3,2,1,0,3],[0,2,1,3,0,3,1,2,2,0,3,1],[0,3,1,2,3,0,2,1,0,2,3,1,3,1,0,2]]
charts=[{'interval':gap,'window':window,'notes':[pattern[n%len(pattern)] for n in range(count)]} for gap,window,count,pattern in zip([40,30,24],[8,6,5],[32,48,64],patterns)]
sprites=[Bitmap(8,8) for _ in range(4)]
sprites[1].line(0,0,0,7);sprites[2].dot(3,3);sprites[3].line(0,0,0,7);sprites[3].dot(3,3)
# Original 7x7 arrows, left/down/up/right; no imported image data.
arrows=[]
for lane in range(4):
 b=Bitmap(8,8)
 for y in range(7):
  for x in range(7):
   if (x>=3 and 2<=y<=4) or (x<=3 and abs(y-3)<=x):
    X,Y=(x,y) if lane==0 else (6-y,6-x) if lane==1 else (y,x) if lane==2 else (6-x,y)
    b.dot(X,Y)
 arrows.extend(b.bytes())
raw=[]
for c in charts:raw.extend([c['interval'],c['window'],len(c['notes'])]+c['notes']+[0]*(64-len(c['notes'])))
assets(root,'BEAT STEP','beat-step',16,7,1,1,sprites,3,asm_bytes('beat_charts',raw)+asm_bytes('beat_arrows',arrows),aux=('SOUND','RESET'))
(root/'charts.json').write_text(json.dumps(charts)+'\n')
