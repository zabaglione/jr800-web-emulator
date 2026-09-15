// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {fixture} from '../common/poly3d-check.mjs';
const f=await fixture();
try {
    f.frame();assert.equal(f.read('phase'),0);await f.capture();
    let gameplay;
    if(f.mode==='test') {
        f.tap('return');assert.equal(f.read('phase'),1);assert.equal(f.read('tries'),6);
        const start=f.cycles.length;
        f.observePolling();
        await f.capture('playing');
        f.key('space',true);f.frame();assert.equal(f.read('tries'),5);
        for(let i=0;i<6;i++) f.frame();
        assert.equal(f.read('tries'),5,'Held check must not consume further tries');
        f.release();f.frame();await f.capture('no-match');
        for(let round=0;round<3;round++) {
            assert.equal(f.read('level'),round);
            const yaw=f.read('target_yaw'),pitch=f.read('target_pitch');
            // Only real key presses change the player; no state is written by this harness.
            for(let i=0;i<yaw/8;i++) f.tap('keypad-6');
            for(let i=0;i<pitch/8;i++) f.tap('keypad-8');
            assert.equal(f.read('player_yaw'),yaw);assert.equal(f.read('player_pitch'),pitch);
            const dots=f.machine.lcdPanel().dots;
            for(let y=16;y<56;y++) for(let x=30;x<74;x++)
                assert.equal(dots[y*192+x],dots[y*192+x+88],'Matching poses must draw identical solids');
            await f.capture('matched-'+(round+1));
            f.tap('space');
        }
        f.observePolling(false);
        gameplay=f.timing(start,f.cycles.length-2);
        assert.equal(f.read('phase'),2);assert.equal(f.read('level'),3);assert.equal(f.read('tries'),5);
        await f.capture('clear');
        f.tap('return');assert.equal(f.read('phase'),1);assert.equal(f.read('moves'),0);
        f.tap('keypad-4');assert.equal(f.read('player_yaw'),56);
        f.tap('keypad-6');assert.equal(f.read('player_yaw'),0);
        f.tap('keypad-2');assert.equal(f.read('player_pitch'),56);
        f.tap('keypad-8');assert.equal(f.read('player_pitch'),0);
        for(let i=0;i<6;i++) f.tap('space');
        assert.equal(f.read('phase'),3);assert.equal(f.read('tries'),0);await f.capture('game-over');
        f.tap('space');assert.equal(f.read('phase'),1);assert.equal(f.read('tries'),6);
    } else f.frame();
    await f.finish({gameplay,rounds:3,startTries:6,angleStep:8});
} finally {f.close();}
