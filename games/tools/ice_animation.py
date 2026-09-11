# SPDX-License-Identifier: MIT
"""Export the browser test's actual LCD frames as a timed, pixel-exact GIF."""
import argparse,json,re
from pathlib import Path
from PIL import Image
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('recording',type=Path);p.add_argument('output',type=Path)
a=p.parse_args();root=Path(__file__).resolve().parents[2]
frames=json.loads(a.recording.read_text())['frames']
board=json.loads((root/'games/ice-route/challenges.json').read_text())['stages'][0]['initial']['board']
tiles=[int(s,16) for s in re.findall(r'\$([0-9A-F]{2})',(root/'games/ice-route/assets.s').read_text().split('tiles:')[1].split('view_cells:')[0])]
def tile(f,cell):
    at=(1+cell//14)*192+40+(cell%14)*8
    return f['bytes'][at:at+8]
# Discard partial LCD transfers during the transition from stage selection.
start=next(i for i,f in enumerate(frames) if all(tile(f,c)==tiles[(v+1)*8:(v+2)*8] if v in (1,2) else tile(f,c)==[0]*8 for c,v in enumerate(board)))
frames=frames[start:];images=[];durations=[]
for i in range(0,len(frames),3):
    f=frames[i];img=Image.new('P',(192,64));img.putpalette([175,184,153,31,44,35]+[0]*762)
    img.putdata([(f['bytes'][(y//8)*192+x]>>(y%8))&1 for y in range(64) for x in range(192)])
    images.append(img.resize((768,256),Image.Resampling.NEAREST))
    end=frames[min(i+3,len(frames)-1)]['t'];durations.append(max(20,round((end-f['t'])/10)*10))
durations[-1]=900
a.output.parent.mkdir(parents=True,exist_ok=True)
images[0].save(a.output,save_all=True,append_images=images[1:],duration=durations,loop=0,optimize=True,disposal=2)
print(f'Exported {len(images)} LCD frames, {sum(durations)} ms')
