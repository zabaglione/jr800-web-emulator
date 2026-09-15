// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {fixture} from '../common/poly3d-check.mjs';
const f=await fixture();
try {
    f.frame();
    assert.equal(f.read('phase'),0);
    await f.capture();
    let gameplay;
    if(f.mode==='test') {
        f.tap('return');
        assert.equal(f.read('phase'),1); assert.equal(f.read('hull'),3);
        f.tap('space'); assert.equal(f.read('flight_paused'),1);
        const age=f.read('gate_age'), ship=f.read('ship_x'), frozen=f.frame().framebuffer;
        f.key('keypad-4',true);
        for(let i=0;i<5;i++) assert.deepEqual(f.frame().framebuffer,frozen,'Paused scene changed');
        f.release(); assert.equal(f.read('ship_x'),ship); assert.equal(f.read('gate_age'),age);
        f.tap('space'); assert.equal(f.read('flight_paused'),0);
        const start=f.cycles.length;
        f.observePolling();
        let near=false;
        for(let i=0;i<500 && f.read('phase')===1;i++) {
            const x=f.read('ship_x'), target=[53,96,139][f.read('gate_lane')];
            f.key('keypad-4',x>target+3); f.key('keypad-6',x<target-3);
            f.frame();
            if(!near && f.read('gate_age')===32) {await f.capture('playing');near=true;}
        }
        f.release(); f.observePolling(false);
        gameplay=f.timing(start,f.cycles.length-1);
        assert.equal(f.read('phase'),2);assert.equal(f.read('score'),8);assert.equal(f.read('hull'),3);
        await f.capture('clear');
        f.tap('return'); assert.equal(f.read('phase'),1); assert.equal(f.read('score'),0);
        f.key('keypad-4',true);
        for(let i=0;i<20;i++) f.frame();
        assert.equal(f.read('ship_x'),40,'Left movement must clamp');
        // Separate observed rows; same-row chords have unresolved hardware behavior.
        f.key('letter-d',true);
        f.frame(); const neutral=f.read('ship_x');
        f.frame(); assert.equal(f.read('ship_x'),neutral,'Opposite directions must neutralize');
        f.key('keypad-4',false);
        for(let i=0;i<30;i++) f.frame();
        assert.equal(f.read('ship_x'),152,'Right movement must clamp');
        f.release();
        f.until(()=>f.read('phase')===3,500);
        assert.equal(f.read('hull'),0); await f.capture('game-over');
        f.tap('space');assert.equal(f.read('phase'),1);assert.equal(f.read('hull'),3);
    } else f.frame();
    await f.finish({gameplay,winScore:8,startHull:3});
} finally {f.close();}
