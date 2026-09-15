// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {fixture} from '../common/poly3d-check.mjs';
const f=await fixture();
try {
    f.frame();
    await f.capture();
    const rotation=[];
    if(f.mode==='test') {
        const seen=new Set();
        f.observePolling();
        for(let model=0;model<3;model++) {
            assert.equal(f.read('lab_model'),model);
            const start=f.cycles.length;
            for(let i=0;i<64;i++) {
                const {framebuffer}=f.frame();
                seen.add(framebuffer.slice(192,1344).join(','));
            }
            rotation.push({model:['tetra','octa','cube'][model],...f.timing(start)});
            await f.capture(['tetra','octa','cube'][model]);
            f.tap('space'); assert.equal(f.read('lab_wire'),1);
            await f.capture(['tetra-wire','octa-wire','cube-wire'][model]);
            f.tap('space'); assert.equal(f.read('lab_wire'),0);
            const before=f.read('lab_pitch');
            f.tap('keypad-8'); assert.equal(f.read('lab_pitch'),(before+4)&63);
            f.tap('keypad-2'); assert.equal(f.read('lab_pitch'),before);
            const yaw=f.read('lab_yaw');
            f.tap('keypad-4'); assert.equal(f.read('lab_yaw'),(yaw-4)&63);
            f.tap('keypad-6'); assert.equal(f.read('lab_yaw'),yaw);
            assert.equal(f.read('lab_auto'),0);
            f.tap('return');
        }
        f.observePolling(false);
        assert.equal(f.read('lab_model'),0);
        assert.ok(seen.size>130,'Rotation did not produce distinct solid views');
    } else f.frame();
    await f.finish({models:3,vertices:[4,6,8],triangles:[4,8,12],rotation});
} finally {f.close();}
