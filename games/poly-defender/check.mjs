// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {fixture} from '../../sdk/examples/lcd/common/poly3d-check.mjs';
import {state,chooseInput,humanPolicy} from './play-policy.mjs';
import {png} from '../tools/harness.mjs';
const smoke=process.argv[5]==='smoke';
if(smoke)process.argv[5]='run';
const f=await fixture({romPath:process.env.JR800_GAME_ROM ?? null});
assert.equal(f.sample,'poly-defender');
// Test-owned key/frame recording lets the native core replay the same campaign.
const nativeKeys=['keypad-8','keypad-2','keypad-4','keypad-6','space','return','letter-w','letter-s','letter-a','letter-d',
    'keypad-7','keypad-9','keypad-1','keypad-3','keypad-5'];
const replay=[`${f.symbols.frame_ready} ${f.symbols.framebuffer} ${f.symbols.phase} ${f.symbols.app_update}`, 'EVENTS'];
const baseFrame=f.frame,baseKey=f.key,baseCapture=f.capture;
let replayFrames=0;
f.key=(name,held)=>{assert.ok(nativeKeys.includes(name));baseKey(name,held);replay.push(`K ${nativeKeys.indexOf(name)} ${Number(held)}`);};
f.frame=()=>{
    if(replayFrames++)replay.push('R 1');
    const result=baseFrame();let hash=2166136261;
    for(const byte of result.framebuffer)hash=Math.imul(hash^byte,16777619)>>>0;
    replay.push(`F ${hash} ${result.cycle} ${f.read('phase')}`);
    return result;
};
f.capture=async(name='screen')=>{
    await baseCapture(name);
    const dots=f.machine.lcdPanel().dots;
    const gallery={'boss-warning':'gameplay-1','boss-fight':'gameplay-2','death-hold':'gameplay-3'};
    for(const label of [name,gallery[name]].filter(Boolean)){
        await writeFile(resolve(process.argv[3],label+'.png'),png(dots));
        await writeFile(resolve(process.argv[3],label+'-1x.png'),png(dots,1));
    }
};
const coverage={phases:new Set(),kinds:new Set(),bossModes:new Set(),audio:new Set(),weapons:new Set(),
    entries:0,split:0,weaponPickups:0,shieldPickups:0,shieldBlocks:0,hits:0,bulletHits:0,bodyHits:0,
    downgrades:0,expiredItems:0,piercing:0,recoveryHits:0,aimedBolts:0,frozenFrames:0,invertedFrames:0,
    maxEnemies:0,maxShots:0,maxBolts:0,maxObjects:0,rage:false};
const stages=[[],[],[],[]],stress=[],held=new Set(),events=[],checkpoints=[];
let recording=false,startCycle,frameIndex=0;
const now=()=>Number(f.machine.state().cycleCount);
function key(k,v){if(held.has(k)===v)return;f.key(k,v);if(v)held.add(k);else held.delete(k);
    if(recording)events.push({cycle:now()-startCycle,key:k,down:v});}
function release(){for(const k of [...held])key(k,false);}
function check(p,s) {
    coverage.phases.add(s.phase);if(s.audio)coverage.audio.add(s.audio);
    if(s.phase===0)return;
    if(s.phase===5){
        const font=f.read('font',320),fb=f.read('framebuffer',1536);
        for(const [i,c] of [...'! BOSS APPROACHING !'].entries())for(let x=0;x<5;x++)
            assert.equal(fb[576+39+i*6+x],font[(c.charCodeAt(0)-32)*5+x],'Warning text was erased by moving shutters');
    }
    assert.ok(s.hull>=0 && s.hull<=3 && s.weapon>=1 && s.weapon<=3);
    assert.ok(s.shield===0 || s.shield===1);assert.equal(s.x,24);
    assert.ok(s.y>=16 && s.y<=48 && s.y%2===0);assert.ok(s.score<=99);
    const live=s.enemies.filter(e=>e[0]),shots=s.shots.filter(e=>e[0]);
    coverage.maxEnemies=Math.max(coverage.maxEnemies,live.length);
    coverage.maxShots=Math.max(coverage.maxShots,shots.length);
    coverage.maxBolts=Math.max(coverage.maxBolts,s.bolts.length);
    coverage.maxObjects=Math.max(coverage.maxObjects,live.length+shots.length+s.bolts.length+Number(!!s.item[0]));
    coverage.weapons.add(s.weapon);
    assert.ok(live.length<=2 && shots.length<=2 && s.bolts.length<=2);
    for(const e of live){
        coverage.kinds.add(e[0]);assert.ok(e[2]>=9 && e[2]<=188 && e[3]>=16 && e[3]<=48);
        assert.ok(e[1]>0);
        if(e[0]===3 || e[0]===5)assert.equal(live.length,1);
    }
    for(const e of shots)assert.ok(e[1]>=32 && e[1]<=184 && e[2]>=16 && e[2]<=48);
    if(s.phase===1 && s.stage===3 && !(s.enemies[0][4]&128))coverage.bossModes.add(f.read('boss_mode'));
    if(p.phase!==1)return;
    for(let i=0;i<2;i++){
        const a=p.enemies[i],b=s.enemies[i];
        if(a[0] && b[0]===a[0] && a[4]&128 && s.phase===1){
            assert.equal(b[2],Math.max(152,a[2]-3),'Entrance must move smoothly');
            assert.equal(b[1],a[1],'Enemy cannot be hit before entering');
            if(!(b[4]&128))coverage.entries++;
        }
        const x=p.shots[i],y=s.shots[i];
        if(x[0] && y[0] && y[1]!==32 && !(a[0]===3 && b[0]===4)){
            assert.equal(y[1],x[1]+8);assert.equal(y[2],x[2],'Shot must keep its launch lane');
        }
        if(y[0] && y[3]&128 && (y[3]&3)&~(x[3]&3))coverage.piercing++;
    }
    for(const b of s.bolts) {
        assert.ok(b.x>=0 && b.x<3072 && b.y>=144 && b.y<864);
        const previous=p.bolts.find(a=>a.life===b.life+1 && a.dx===b.dx && a.dy===b.dy &&
            a.x+a.dx===b.x && a.y+a.dy===b.y);
        const frozen=p.bolts.some(a=>JSON.stringify(a)===JSON.stringify(b));
        if(b.life===37 || b.life===38){
            const steps=38-b.life,ox=b.x-b.dx*steps,oy=b.y-b.dy*steps;
            assert.equal(ox%16,0);assert.equal(oy%16,0);
            assert.ok(Math.abs((ox+b.dx*32)/16-24)<=1,'Bolt must target the captured player column');
            assert.ok(s.enemies.some(e=>Math.abs((oy+b.dy*32)/16-e[5])<=1),'Bolt target lane');
            coverage.aimedBolts++;
        }else assert.ok(previous || (s.phase!==1 && frozen),'Bolt changed fixed velocity');
    }
    if(p.enemies[0][0]===3 && s.enemies[0][0]===4){
        assert.deepEqual(s.enemies.map(e=>e[0]),[4,4]);assert.equal(shots.length,0);coverage.split++;
    }
    if(s.weapon>p.weapon)coverage.weaponPickups++;
    if(s.shield>p.shield)coverage.shieldPickups++;
    if(p.item[0] && !s.item[0] && s.item[1]<=12 && p.score===s.score)coverage.expiredItems++;
    if(p.shield && !s.shield){assert.equal(s.hull,p.hull);assert.ok(s.weapon>=p.weapon);coverage.shieldBlocks++;}
    if(s.hull<p.hull){
        assert.equal(s.hull,p.hull-1);assert.equal(s.weapon,Math.max(1,p.weapon-1));
        assert.equal(s.phase,s.hull?6:7);assert.ok(s.inverted);coverage.hits++;
        if(s.cause===1)coverage.bulletHits++;else if(s.cause===2)coverage.bodyHits++;
        if(s.weapon<p.weapon)coverage.downgrades++;
    }
    const a=p.enemies[0],b=s.enemies[0];
    if(a[0]===5 && b[0]===5 && b[1]<a[1] && b[4]>=48)coverage.recoveryHits++;
    if(b[0]===5 && b[1]<=9 && b[4]===20 && s.bolts.length===2)coverage.rage=true;
}
function frame({benchmark=false,stressRun=false}={}){
    const p=state(f),panel=f.machine.lcdPanel().dots;f.frame();const s=state(f);frameIndex++;
    check(p,s);
    if([6,7].includes(p.phase) && s.phase===p.phase){
        for(const k of ['enemies','bolts','shots','item','tick','y','hull','weapon','shield'])assert.deepEqual(s[k],p[k],'Frozen impact '+k);
        coverage.frozenFrames++;
        if(s.inverted!==p.inverted){
            const dots=f.machine.lcdPanel().dots;
            for(let i=0;i<dots.length;i++)assert.equal(dots[i],3-panel[i],'Whole-screen inverse at '+i);
        }
    }
    if(s.inverted)coverage.invertedFrames++;
    if(p.phase===1 && s.phase===1 && p.stage===s.stage){
        if(benchmark)stages[p.stage].push(f.cycles.at(-1));
        if(stressRun)stress.push(f.cycles.at(-1));
    }
    return s;
}
function move(k,options={}){key('keypad-8',k==='keypad-8');key('keypad-2',k==='keypad-2');return frame(options);}
function steer(options={},timing={}){return move(chooseInput(state(f),options),timing);}
function tap(k){key(k,true);frame();key(k,false);return frame();}
function until(test,limit=2400,options={},timing={}){
    for(let i=0;i<limit;i++){if(test(state(f)))return;assert.ok(![2,3].includes(f.read('phase')),'Unexpected result');steer(options,timing);}
    assert.fail('Replay exceeded update budget');
}
async function capture(label){await f.capture(label);if(recording)checkpoints.push({label,cycle:now()-startCycle,phase:f.read('phase')});}
const timing=a=>{assert.ok(a.length);const mean=a.reduce((x,y)=>x+y,0)/a.length;
    return {frames:a.length,meanCycles:mean,minCycles:Math.min(...a),maxCycles:Math.max(...a),nominalFps:1228800/mean};};
try {
    f.frame();await f.capture();await f.capture('title');
    if(f.mode!=='test'){
        tap('return');for(let i=0;i<32;i++)frame();
        assert.equal(f.read('phase'),1);await f.finish();
    }else{
        f.observePolling();recording=true;startCycle=now();
        for(let i=0;i<16;i++)frame();tap('return');
        // First attempt: small, hesitant manual-style adjustments miss aimed attacks.
        let age=0;const captured=new Set();
        for(let i=0;i<800 && f.read('phase')!==3;i++){
            const s=state(f);
            let k=null;
            if(s.phase===1){age++;const n=age%30;if(n>=18 && n<20)k='keypad-8';if(n>=22 && n<24)k='keypad-2';}
            move(k);
            if([6,7].includes(f.read('phase')) && !captured.has(f.read('phase'))){
                captured.add(f.read('phase'));await capture(f.read('phase')===7?'death-inverted':'hurt');
            }
            if(f.read('phase')===7 && !f.read('screen_inverted') && !captured.has('death-hold')){
                captured.add('death-hold');await capture('death-hold');
            }
        }
        release();assert.equal(f.read('phase'),3);await capture('game-over');
        for(let i=0;i<28;i++)frame();assert.equal(f.read('audio_id'),0,'Game-over cue must end');
        tap('return');const player=humanPolicy();
        let warningCaptured=false,bossEntryCaptured=false;
        for(let i=0;i<2300 && ![2,3].includes(f.read('phase'));i++){
            const s=move(player(state(f)),{benchmark:true});
            if(s.phase===5 && !warningCaptured){await capture('boss-warning');warningCaptured=true;}
            if(s.stage===3 && s.phase===1 && s.enemies[0][2]===176 && !bossEntryCaptured){
                await capture('boss-entry');bossEntryCaptured=true;
            }
            if(s.stage===3 && s.phase===1 && s.enemies[0][4]===36 && !captured.has('boss-fight')){
                captured.add('boss-fight');await capture('boss-fight');
            }
            if(s.phase===8 && !captured.has('boss-destroyed')){captured.add('boss-destroyed');await capture('boss-destroyed');}
        }
        release();assert.equal(f.read('phase'),2);await capture('clear');
        for(let i=0;i<32;i++)frame();assert.equal(f.read('audio_id'),0,'Clear cue must end');
        const outcome=Object.fromEntries(['phase','score','hull','weapon','shield'].map(n=>[n,f.read(n)]));
        await writeFile(resolve(process.argv[3],'horizontal-replay.json'),JSON.stringify({clockHz:1228800,
            startCycle,durationCycles:now()-startCycle,outcome,events,checkpoints,
            inputStyle:'Automated keys: hesitant first attempt, 0.3s observations / 0.2s delayed decisions on retry'},null,2)+'\n');
        recording=false;
        // Test precise controls, hold behavior and independent stress/escape scenarios.
        tap('return');until(s=>s.phase===1);
        assert.equal(f.read('hull'),3);assert.equal(f.read('weapon'),1);assert.equal(f.read('score'),0);
        key('space',true);frame();for(let i=0;i<12;i++)frame();assert.equal(f.read('fire_enabled'),0);
        key('space',false);frame();key('keypad-8',true);for(let i=0;i<12;i++)frame();
        assert.equal(f.read('ship_y'),16);release();key('keypad-2',true);for(let i=0;i<20;i++)frame();
        assert.equal(f.read('ship_y'),48);release();tap('space');
        until(s=>s.stage===2);
        release();tap('space');for(let i=0;i<100;i++)steer({}, {stressRun:true});
        release();tap('space');until(s=>s.stage===3,1500,{}, {stressRun:true});
        until(s=>s.phase===1 && s.enemies[0][4]<128);
        until(s=>s.enemies[0][1]<=9,1300,{collect:false});
        release();tap('space');for(let i=0;i<135;i++)steer({collect:false,avoidItems:true});
        release();
        // Stay in the lane locked at age 32 to verify a body-impact freeze too.
        for(let i=0;i<400 && !coverage.bodyHits;i++){
            const s=state(f),e=s.enemies[0];
            assert.notEqual(s.phase,3,'Body-impact scenario ran out of hull');
            const k=s.phase===1 && e[4]>=32 && e[4]<48?
                (e[5]<s.y?'keypad-8':e[5]>s.y?'keypad-2':null):chooseInput(s,{collect:false});
            move(k);
        }
        release();
        f.observePolling(false);
        const measures=stages.map(timing),load=timing(stress),serializable=Object.fromEntries(
            Object.entries(coverage).map(([k,v])=>[k,v instanceof Set?[...v].sort((a,b)=>a-b):v]));
        await writeFile(resolve(process.argv[3],'scenario-diagnostics.json'),JSON.stringify({stages:measures,stress:load,coverage:serializable},null,2)+'\n');
        for(const p of [0,1,2,3,4,5,6,7,8])assert.ok(coverage.phases.has(p),'Phase '+p);
        for(const k of [1,2,3,4,5])assert.ok(coverage.kinds.has(k),'Enemy '+k);
        for(const k of [0,1,2,3])assert.ok(coverage.bossModes.has(k),'Boss mode '+k);
        for(const k of [1,3,4,5,7,8,9,10,11,12,13])assert.ok(coverage.audio.has(k),'Audio cue '+k);
        for(const k of ['entries','split','weaponPickups','shieldPickups','hits','bulletHits','bodyHits','downgrades',
            'expiredItems','piercing','recoveryHits','aimedBolts','frozenFrames','invertedFrames'])assert.ok(coverage[k]>0,'Missing '+k);
        assert.ok(coverage.rage,'Low HP boss attack');
        for(const m of [...measures,load]){
            assert.ok(m.meanCycles<=123624,'Mean slowdown: '+JSON.stringify(m));
            assert.ok(m.maxCycles<=188366,'Frame spike: '+JSON.stringify(m));
        }
        await f.finish({gameplay:timing(stages.flat()),stages:measures,stress:load,coverage:serializable,
            controls:'normal keypad 8/2, W/S, SPACE, RETURN; key replay only',
            presentation:'entry, warning, frozen/inverted impacts, boss destruction, jingles and SE',
            baseline:{meanCycles:123623.63679245283,maxCycles:188366}});
    }
    if(!process.env.JR800_GAME_ROM)
        await writeFile(resolve(process.argv[3],(smoke?'smoke-':'')+'replay.txt'),replay.join('\n')+'\n');
}finally{f.close();}
