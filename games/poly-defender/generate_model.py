#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Emit original live meshes and projected polygon boundaries for POLY DEFENDER."""
import argparse
import math
import json
import importlib.util
from pathlib import Path

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('poly_mesh_author', root / 'sdk/lib/poly3d/generate_models.py')
author = importlib.util.module_from_spec(spec)
spec.loader.exec_module(author)
author.MODELS = {'tetra':author.MODELS['tetra'],'octa':author.MODELS['octa'],'prism': {
    'vertices': [(-24, -10, -12), (24, -10, -12), (0, -10, 22), (0, 18, 0), (0, -22, 0)],
    'faces': [(0, 1, 3), (1, 2, 3), (2, 0, 3), (1, 0, 4), (2, 1, 4), (0, 2, 4)],
}}
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--check', action='store_true')
args = parser.parse_args()
# All game meshes use geometry-only poses; shading and composition run on the CPU.
author.MODELS['fighter'] = {
    'vertices': [(12,0,0),(-9,6,0),(-9,-6,0),(-5,0,4),(-5,0,-4)],
    'faces': [(0,1,3),(1,2,3),(2,0,3),(1,0,4),(2,1,4),(0,2,4)],
}
author.MODELS['scout'] = {
    'vertices': [(-18,-10,-12),(18,-10,-12),(0,-10,24),(0,20,0)],
    'faces': [(0,1,2),(0,3,1),(1,3,2),(2,3,0)],
}
author.MODELS['rotor'] = {'vertices': [(0,22,0),(0,-22,0),(-20,0,0),(20,0,0),(0,0,20),(0,0,-20)],'faces': [(0,2,4),(0,4,3),(0,3,5),(0,5,2),(1,4,2),(1,3,4),(1,5,3),(1,2,5)]}
models=author.meshes()
def project(name,yaw,pitch,scale,roll=0):
    cy,sy=math.cos(yaw),math.sin(yaw);cp,sp=math.cos(pitch),math.sin(pitch)
    def rotate(v):
        x,y,z=v;x,z=x*cy+z*sy,z*cy-x*sy
        y,z=y*cp-z*sp,y*sp+z*cp
        return x*math.cos(roll)-y*math.sin(roll),x*math.sin(roll)+y*math.cos(roll),z
    vertices=[(round(rotate(v)[0]*scale),round(-rotate(v)[1]*scale)) for v in models[name]['vertices']]
    faces=[]
    for face in models[name]['faces']:
        x,y,z=rotate(face[3:6])
        if z<=0:continue
        light=-.3*x+.6*y+.7*z
        shade=3 if light>60 else 2 if light>28 else 1 if light>0 else 0
        xy=[vertices[i] for i in face[:3]]
        if (xy[1][0]-xy[0][0])*(xy[2][1]-xy[0][1])==(xy[2][0]-xy[0][0])*(xy[1][1]-xy[0][1]):continue
        faces.append([shade,*[c for v in xy for c in v]])
    assert faces
    return faces
poses={'ship':[project('fighter',0,p,1) for p in (0,-.48,.48)],
       'scout':[project('scout',i*math.tau/8,.6,.35) for i in range(8)],
       'rotor':[project('rotor',i*math.pi/8,.35,.5) for i in range(4)],
       'guardian':[project('prism',i*math.tau/4,.35,.5) for i in range(4)],
       'aim':[project('scout',0,.6,.3,math.pi/2)],
       'core':[project('rotor',math.pi/4,0,.15)]}
def columns(face):
    shade,*xy=face;points=list(zip(xy[::2],xy[1::2]));left=min(x for x,y in points);right=max(x for x,y in points)
    edges={x:[] for x in range(left,right+1)}
    for a,b in zip(points,points[1:]+points[:1]):
        if a[0]>b[0]:a,b=b,a
        if a[0]==b[0]:edges[a[0]] += [a[1],b[1]];continue
        dx=b[0]-a[0];dy=b[1]-a[1]
        for x in range(a[0],b[0]+1):edges[x].append(a[1]+(1 if dy>=0 else -1)*(abs(dy)*(x-a[0])//dx))
    return [shade,left,right-left+1,min(y for x,y in points),max(y for x,y in points),*[v for values in edges.values() for v in (min(values),max(values))]]
lines=['; SPDX-License-Identifier: MIT','; Polygon column boundaries and shades, no stored pixel colors.']
for kind,frames in poses.items():
    section='.cues' if kind in ('aim','core') else '.geometry' if kind in ('rotor','guardian') else '.data'
    lines+=['.section '+section+', data']
    lines+=['.global facet_'+kind+'_poses','facet_'+kind+'_poses: .word '+','.join('facet_'+kind+'_'+str(i) for i in range(len(frames)))]
    for i,faces in enumerate(frames):
        lines+=['facet_'+kind+'_'+str(i)+': .byte '+str(len(faces))]
        lines+=['    .byte '+','.join(str(c&255) for c in columns(f)) for f in faces]
content='\n'.join(lines)+'\n'
path=Path(__file__).with_name('facet-models.s')
if args.check:
    if path.read_text()!=content:raise SystemExit('Stale projected geometry')
else:path.write_text(content)
path=Path(__file__).with_name('facet-geometry.json')
content=json.dumps(poses,indent=2)+'\n'
if args.check:
    if path.read_text()!=content:raise SystemExit('Stale projected face oracle')
else:path.write_text(content)
print(('Verified' if args.check else 'Generated')+' '+str(sum(map(len,poses.values())))+' original geometry poses.')
