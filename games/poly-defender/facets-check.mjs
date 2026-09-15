// SPDX-License-Identifier: MIT
// Independent pixel oracle for projected faces, opacity, byte shifts and clipping.
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {fixture} from '../../sdk/examples/lcd/common/poly3d-check.mjs';
const geometry=JSON.parse(await readFile(new URL('./facet-geometry.json',import.meta.url),'utf8'));
const f=await fixture({romPath:null});
const poses=[...geometry.ship,...geometry.scout,...geometry.rotor,...geometry.guardian];
const patterns=[[255,255],[85,170],[85,0],[0,0]];
function draw(pixels,faces,ox,oy,scale=1,wire=false) {
    for(const [shade,...v] of faces) {
        const p=[0,2,4].map(i=>[v[i],v[i+1]]);
        const first=Math.min(...p.map(v=>v[0]))*scale+ox,last=Math.min(191,(Math.max(...p.map(v=>v[0]))+1)*scale-1+ox);
        for(let x=Math.max(0,first);x<=last;x++){
            const column=Math.floor((x-ox)/scale);
            const crossings=[];
            for(let i=0;i<3;i++){
                let a=p[i],b=p[(i+1)%3];if(a[0]>b[0])[a,b]=[b,a];
                if(column<a[0] || column>b[0])continue;
                if(a[0]===b[0])crossings.push(a[1],b[1]);
                else {
                    const exact=(b[1]-a[1])*(column-a[0])/(b[0]-a[0]);
                    crossings.push(a[1]+Math.trunc(exact));
                }
            }
            const top=Math.max(8,Math.min(...crossings)*scale+oy),bottom=Math.min(55,Math.max(...crossings)*scale+oy);
            for(let y=top;y<=bottom;y++){
                assert.ok(y>=8 && y<=55,'Authored vertical viewport');
                const edge=x===first || x===last || y===top || y===bottom;
                pixels[y*192+x]=edge || (!wire && Boolean(patterns[shade][x&1] & (1<<(y&7))))?2:1;
            }
        }
    }
}
const lines=[[0,8,191,8],[191,55,0,55],[40,0,40,63],[100,63,100,0],
    [0,0,191,63],[191,63,0,0],[0,63,191,0],[191,0,0,63],
    [18,20,24,24],[24,24,18,20],[70,15,73,46],[73,46,70,15],
    [5,2,189,2],[5,60,189,60],[92,32,92,32],[20,8,22,9]];
function drawLine(pixels,[x0,y0,x1,y1]){
    const steps=Math.max(Math.abs(x1-x0),Math.abs(y1-y0));
    for(let i=0;i<=steps;i++){
        const x=x0+Math.sign(x1-x0)*Math.floor(Math.abs(x1-x0)*i/Math.max(1,steps));
        const y=y0+Math.sign(y1-y0)*Math.floor(Math.abs(y1-y0)*i/Math.max(1,steps));
        if(y>=8 && y<=55)pixels[y*192+x]=2;
    }
}
try {
    for(let frame=0;frame<161;frame++){
        f.frame();if(!frame)f.observePolling();
        const group=f.read('probe_group'),i=f.read('probe_case'),expected=new Uint8Array(12288).fill(1);
        if(group<5)draw(expected,poses[i],[16,93,187,16,110][group],[24,33,40,18,44][group]);
        else if(group<8)draw(expected,geometry.rotor[i],[214,152,64][group-5],32,2);
        else if(group<10)draw(expected,geometry.scout[i],152,group===8?16:48,2);
        else if(group===10)draw(expected,poses[i],93,33,1,true);
        else if(group===11)drawLine(expected,lines[i]);
        else if(i===1){draw(expected,geometry.ship[0],93,33);draw(expected,geometry.rotor[0],90,33);}
        const actual=f.machine.lcdPanel().dots;
        for(let p=0;p<actual.length;p++)assert.equal(actual[p],expected[p],`Polygon ${group}/${i} pixel ${p%192},${p/192|0}`);
    }
    await f.finish({oracleCases:161,geometryPoses:19,lineCases:16,verticalClipping:true,doubleScale:true,opaqueOverlap:true,erasedPreviousBounds:true});
} finally {f.close();}
