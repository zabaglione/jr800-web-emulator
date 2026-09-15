// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {fixture} from '../../sdk/examples/lcd/common/poly3d-check.mjs';
import {state,chooseInput} from './play-policy.mjs';
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
    const gallery={'maze-2':'gameplay-1','reactor-obstacles':'gameplay-2','laser-3':'gameplay-3'};
    for(const label of [name,gallery[name]].filter(Boolean)){
        await writeFile(resolve(process.argv[3],label+'.png'),png(dots));
        await writeFile(resolve(process.argv[3],label+'-1x.png'),png(dots,1));
    }
};

const coverage={phases:new Set(),sectors:new Set(),kinds:new Set(),audio:new Set(),bossModes:new Set(),
    shieldBroken:0,hits:0,laserFrames:0,closedCore:0,burstHits:0,chargeShots:0,gateBlocks:0,
    shieldPickups:0,weaponPickups:0,forwardFrames:0,backwardFrames:0,maxScouts:0,movingGates:0,
    frozenFrames:0,entries:0,scoreCarry:false};
const measures=Array.from({length:3},()=>Array.from({length:4},()=>[]));
const held=new Set(),events=[],checkpoints=[],captures=new Set();
let frameIndex=0,recording=false,startCycle=0;
const now=()=>Number(f.machine.state().cycleCount);
const immutable=Uint8Array.from(f.machine.memory(0x5200,0x600));
const geometryGuard=Uint8Array.from(f.machine.memory(0x5900,0x398));
function keys(wanted=[]) {
    const next=new Set(wanted);
    // Same-row multi-key electrical behavior is unresolved. Use the existing S alias
    // when down and horizontal motion overlap; never change the emulator response.
    if(next.has("keypad-2") && (next.has("keypad-4") || next.has("keypad-6"))){next.delete("keypad-2");next.add("letter-s");}
    for(const key of new Set([...held,...next]))if(held.has(key)!==next.has(key)){
        f.key(key,next.has(key));if(recording)events.push({cycle:now()-startCycle,key,down:next.has(key)});
    }
    held.clear();for(const key of next)held.add(key);
}
function step(wanted=[],benchmark=false){
    keys(wanted);const p=state(f),panel=f.machine.lcdPanel().dots;f.frame();const s=state(f);frameIndex++;
    assert.deepEqual(f.machine.memory(0x5200,0x600),immutable,'Immutable extension program');
    assert.deepEqual(f.machine.memory(0x5900,0x398),geometryGuard,'Immutable polygon geometry');
    coverage.phases.add(s.phase);coverage.audio.add(s.audio);
    if(s.phase!==0){
        coverage.sectors.add(s.sector);assert.ok(s.sector<=2);
        assert.ok(s.x>=16 && s.x<=112 && s.x%2===0);
        assert.ok(s.y>=16 && s.y<=48 && s.y%2===0);
        assert.ok(s.hull>=0 && s.hull<=3 && s.weapon>=1 && s.weapon<=3 && s.shield<=1);
        assert.ok(s.score<=9999);assert.ok(s.charge<=18);
        coverage.scoreCarry ||= s.score>=100;
        assert.ok(s.enemies.filter(e=>e[0]).length<=2 && s.bolts.length<=2 && s.swarm.length<=6 && s.gates.length<=2);
        coverage.maxScouts=Math.max(coverage.maxScouts,s.swarm.length);
        for(const e of s.enemies.filter(e=>e[0])){
            coverage.kinds.add(e[0]);assert.ok(e[1]>0 && e[2]>=9 && e[2]<=(s.sector===2 && e[0]===5?214:188) && e[3]>=16 && e[3]<=48);
            if(s.sector===2 && e[0]===5)assert.equal(e[3],32,'Large core stays inside the vertical viewport');
            if(s.sector===2 && e[0]>=6)assert.ok(e[3]>=20 && e[3]<=44,'Factory hull stays inside the vertical viewport');
            if(e[0]===5 && e[4]<128){
                coverage.bossModes.add(`${s.sector}:${f.read('boss_mode')}`);
                if(s.sector && e[4]>=28 && e[4]<=32)coverage.laserFrames++;
            }
        }
    }
    if(p.phase===1){
        if(s.x>p.x)coverage.forwardFrames++;
        if(s.x<p.x)coverage.backwardFrames++;
        assert.ok(Math.abs(s.x-p.x)<=2 && Math.abs(s.y-p.y)<=2,'Movement step');
        if(s.weapon>p.weapon)coverage.weaponPickups++;
        if(s.shield>p.shield)coverage.shieldPickups++;
        if(p.shield && !s.shield){
            assert.equal(s.hull,p.hull);assert.ok(s.message>0);coverage.shieldBroken++;
            const font=f.read('font',320),fb=f.read('framebuffer',1536);
            for(const [i,c] of [...'SHIELD BROKEN'].entries())for(let x=0;x<5;x++)
                assert.equal(fb[1344+6+i*6+x],font[(c.charCodeAt(0)-32)*5+x],'Shield feedback');
        }
        if(s.hull<p.hull){assert.equal(s.hull,p.hull-1);assert.ok([6,7].includes(s.phase));assert.ok(s.inverted);coverage.hits++;}
        for(let i=0;i<2;i++){
            const a=p.enemies[i],b=s.enemies[i];
            if(a[0] && b[0]===a[0] && a[4]&128 && s.phase===1){
                assert.equal(b[2],Math.max(152,a[2]-3));assert.equal(b[1],a[1]);
                if(!(b[4]&128))coverage.entries++;
            }
            const x=p.shots[i],y=s.shots[i];
            if(y[0] && y[3]&64 && (!x[0] || y[1]===s.x+8))coverage.chargeShots++;
            if(x[0] && x[3]&64 && b[0] && b[0]===a[0] && b[1]<a[1])coverage.burstHits++;
        }
        if(p.stage===1 && s.stage===1){
            for(let i=0;i<2;i++){
                const shot=p.shots[i];
                if(shot[0] && s.gates.some(g=>Math.abs(g[0]-shot[1])<=13 && (s.sector===2?Math.abs(g[1]-shot[2])<=11:Math.abs(g[1]-shot[2])>=12+Math.floor(Math.max(0,Math.abs(g[0]-shot[1])-4)/2)))){
                    assert.ok(!s.shots[i][0] || s.shots[i][1]===s.x+8,'Solid must stop every shot');coverage.gateBlocks++;
                }
            }
        }
        const a=p.enemies[0],b=s.enemies[0];
        if(p.sector===2 && s.stage===3 && a[0]===5 && b[0]===5 && a[4]<128 && b[4]<48 &&
            p.shots.every(shot=>!(shot[0] && (shot[3]&64))) &&
            p.shots.some(shot=>shot[0] && !(shot[3]&1) && Math.abs(shot[1]+8-b[2])<=7 && Math.abs(shot[2]-b[3])<=7)){
            assert.equal(b[1],a[1],'Closed core must stop normal shots');coverage.closedCore++;
        }
        if(s.sector && a[0]===5 && b[0]===5 && a[4]>=8 && b[4]<=32 && b[4]>a[4])assert.equal(b[5],a[5],'Laser aim must remain fixed through the entire beam');
        for(let i=0;i<Math.min(p.gates.length,s.gates.length);i++)if(p.gates[i][1]!==s.gates[i][1])coverage.movingGates++;
        if(benchmark && s.phase===1 && p.sector===s.sector && p.stage===s.stage)measures[p.sector][p.stage].push(f.cycles.at(-1));
    }
    if([6,7].includes(p.phase) && s.phase===p.phase){
        for(const k of ['enemies','bolts','shots','item','gates','swarm','tick','x','y','hull','weapon','shield'])assert.deepEqual(s[k],p[k],'Frozen impact '+k);
        coverage.frozenFrames++;
        if(s.inverted!==p.inverted){const dots=f.machine.lcdPanel().dots;for(let i=0;i<dots.length;i++)assert.equal(dots[i],3-panel[i]);}
    }
    return s;
}
async function capture(name){
    if(captures.has(name))return;captures.add(name);await f.capture(name);
    if(recording)checkpoints.push({label:name,cycle:now()-startCycle,phase:f.read('phase')});
}
const timing=a=>{assert.ok(a.length);const mean=a.reduce((x,y)=>x+y,0)/a.length;
    return {frames:a.length,meanCycles:mean,minCycles:Math.min(...a),maxCycles:Math.max(...a),nominalFps:1228800/mean};};
try {
    f.frame();await capture('title');
    if(f.mode!=='test'){
        step(['return']);step();for(let i=0;i<32;i++)step();assert.equal(f.read('phase'),1);await f.finish();
    }else{
        f.observePolling();recording=true;startCycle=now();
        // An unattended first attempt demonstrates readable impacts and game over.
        step(['return']);step();
        for(let i=0;i<900 && f.read('phase')!==3;i++){
            const s=step();
            if(s.phase===7)await capture(s.inverted?'death-inverted':'death-hold');
        }
        assert.equal(f.read('phase'),3,'Idle attempt must lose');await capture('game-over');
        for(let i=0;i<28;i++)step();
        step(['return']);step();
        let shieldWait=0;
        for(let i=0;i<6500 && ![2,3].includes(f.read('phase'));i++){
            const s=state(f);let input=chooseInput(s);
            if(s.phase===1 && s.shield && !coverage.shieldBroken && shieldWait++<150){input=[];}
            // Release SPACE between the two taps. Charging sacrifices sustained fire.
            if(s.phase===1 && s.firing && i%74===16)input.push('space');
            if(s.phase===1 && !s.firing && s.charge===18)input.push('space');
            const n=step(input,true);
            if(n.sector!==s.sector)console.log(JSON.stringify({sector:n.sector,frame:frameIndex,hull:n.hull,score:n.score}));
            if(n.gates.some(g=>g[0]<110))await capture(`maze-${n.sector+1}`);
            if(n.sector===2 && n.gates.some(g=>g[0]<110))await capture('reactor-obstacles');
            if(n.swarm.length>=4)await capture(`rush-${n.sector+1}`);
            if(n.message)await capture('shield-broken');
            if(n.phase===5)await capture('boss-warning');
            if(n.stage===3 && n.enemies[0][4]===36)await capture('boss-fight');
            if(n.stage===3 && n.sector && n.enemies[0][4]===29)await capture(`laser-${n.sector+1}`);
            if(n.phase===8)await capture(`boss-down-${n.sector+1}`);
            if(n.phase===7)await capture('unexpected-death');
        }
        keys();
        const serializable=Object.fromEntries(Object.entries(coverage).map(([k,v])=>[k,v instanceof Set?[...v]:v]));
        const measured=measures.map(a=>a.map(b=>b.length?timing(b):null));
        await writeFile(resolve(process.argv[3],'campaign-diagnostics.json'),JSON.stringify({state:state(f),measures:measured,coverage:serializable},null,2)+'\n');
        assert.equal(f.read('phase'),2,'Campaign input replay must defeat all three bosses');await capture('clear');
        const outcome=state(f);
        for(let i=0;i<32;i++)step();assert.equal(f.read('audio_id'),0);
        await writeFile(resolve(process.argv[3],'campaign-replay.json'),JSON.stringify({clockHz:1228800,startCycle,durationCycles:now()-startCycle,
            outcome,events,checkpoints,inputStyle:'Automated coverage keys with deterministic hazard forecast; no RAM writes'},null,2)+'\n');
        for(const p of [0,1,2,3,4,5,6,7,8])if(p)assert.ok(coverage.phases.has(p),'Phase '+p);
        for(const k of [1,2,3,4,5,6,7])assert.ok(coverage.kinds.has(k),'Enemy '+k);
        for(const k of ['shieldBroken','hits','weaponPickups','shieldPickups','chargeShots','burstHits','laserFrames','movingGates','forwardFrames','backwardFrames','frozenFrames','entries','closedCore','gateBlocks'])assert.ok(coverage[k]>0,'Coverage '+k);
        assert.equal(coverage.maxScouts,6);assert.ok(coverage.scoreCarry);
        // Preserve the preceding campaign's limits, including all six-scout rushes.
        for(const area of measured)for(const m of area){
            assert.ok(m);assert.ok(m.meanCycles<=123624,'Mean slowdown: '+JSON.stringify(m));
            assert.ok(m.maxCycles<=188366,'Frame spike: '+JSON.stringify(m));
        }
        recording=false;
        step(['return']);step();
        for(let i=0;i<40 && f.read('phase')!==1;i++)step();
        for(let i=0;i<8;i++)step(['keypad-4']);
        assert.equal(f.read('ship_x'),16,'Left movement boundary');
        step();step(['letter-d']);assert.equal(f.read('ship_x'),18,'D moves forward');
        step();step(['letter-a']);assert.equal(f.read('ship_x'),16,'A moves backward');
        step();for(let i=0;i<24;i++)step(['space']);
        assert.equal(f.read('fire_enabled'),0,'Holding SPACE must not toggle repeatedly');
        assert.equal(f.read('charge'),18,'Charging must stop at ready');
        step();step(['space']);step();
        for(let i=0;i<8;i++)step(['keypad-8']);
        for(let i=0;i<110 && f.read('ship_x')!==112;i++)step(['keypad-6']);
        assert.equal(f.read('ship_x'),112,'Right movement boundary');
        for(let i=0;i<6;i++)step(['keypad-6']);assert.equal(f.read('ship_x'),112,'Right clamp');
        step();for(let i=0;i<32 && f.read('phase')!==1;i++)step();
        assert.equal(f.read('phase'),1,'Controls resume after impact pause');
        step(['keypad-4']);assert.equal(f.read('ship_x'),110,'Backward movement after clamp');
        keys();
        await f.finish({gameplay:timing(measures.flat(2)),stages:measured,coverage:serializable,
            additionalProgramBytes:[...(await readFile(resolve(process.argv[3],'poly-defender.map'),'utf8')).matchAll(/^  \$([0-9A-F]+)-\$([0-9A-F]+)  (?:EXTENSION|GEOMETRY)  /gm)].reduce((n,m)=>n+parseInt(m[2],16)-parseInt(m[1],16)+1,0),controls:'Normal keypad 8/2/4/6, SPACE, RETURN; key replay only',
            memory:'Standard 16KB RAM; immutable program extension and relocated BASIC return workspace',
            budgets:{meanCycles:123624,maxCycles:188366}});
    }
    if(!process.env.JR800_GAME_ROM)await writeFile(resolve(process.argv[3],(smoke?'smoke-':'')+'replay.txt'),replay.join('\n')+'\n');
}finally{f.close();}
