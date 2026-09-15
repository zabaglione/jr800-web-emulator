// SPDX-License-Identifier: MIT
// Verification observes state but drives the unmodified JR-800 program with keys.
export function state(f) {
    const bytes=(name,n)=>[...f.read(name,n)],e=bytes('enemies',20),b=bytes('bolts',16),s=bytes('shots',8);
    return {
        phase:f.read('phase'),stage:f.read('stage'),score:f.read('score'),hull:f.read('hull'),
        weapon:f.read('weapon'),shield:f.read('shield'),x:f.read('ship_x'),y:f.read('ship_y'),
        tick:f.read('def_tick'),firing:f.read('fire_enabled'),invulnerable:f.read('invulnerable'),
        pause:f.read('pause_ticks'),cause:f.read('hit_cause'),inverted:f.read('screen_inverted'),
        audio:f.read('audio_id'),spawnLeft:f.read('spawn_left'),item:bytes('item',4),
        enemies:[e.slice(0,10),e.slice(10)],shots:[s.slice(0,4),s.slice(4)],
        bolts:[b.slice(0,8),b.slice(8)].filter(b=>b[0]).map(b=>({life:b[0],x:b[1]*256+b[2],y:b[3]*256+b[4],
            dx:(b[5]*256+b[6])<<16>>16,dy:b[7]<<24>>24})),
    };
}
const toward=(a,b,n)=>a+Math.sign(b-a)*Math.min(n,Math.abs(b-a));
// Full-precision test policy. The demonstration uses the delayed policy below.
export function chooseInput(s,{collect=true,evade=true,avoidItems=false}={}) {
    if(s.phase!==1)return null;
    const enemies=s.enemies.filter(e=>e[0]),target=enemies.slice().sort((a,b)=>a[2]-b[2])[0];
    const seeking=collect && s.item[0] && s.item[1]<90;
    const goal=seeking?s.item[2]:(avoidItems && s.item[0]?(s.item[2]<32?48:16):(target?.[3]??32));
    let best={value:Infinity,y:s.y};
    for(let dest=16;dest<=48;dest+=2) {
        let value=Math.abs(dest-goal)*(seeking?.7:.3)+Math.abs(dest-s.y)*.025;
        for(let t=1;t<=20;t++) {
            const y=toward(s.y,dest,2*t);
            if(!evade || s.invulnerable>=t)continue;
            for(const b of s.bolts) {
                if(b.life<=t)continue;
                const bx=(b.x+b.dx*t)/16,by=(b.y+b.dy*t)/16;
                if(bx>=16 && bx<=32 && Math.abs(y-by)<=7)value+=1000/(t+1);
            }
            for(const e of enemies) {
                if(e[4]&128)continue;
                let ex=e[2],ey=e[3];
                if(e[0]===2){const n=Math.max(0,e[4]+t-23)-Math.max(0,e[4]-23);ex-=3*n;ey=toward(ey,e[5],2*n);}
                if(e[0]===4){ex-=t;ey+=e[6]<<24>>24;}
                if(e[0]===5)for(let n=1;n<=t;n++) {
                    const a=e[4]+n;
                    if(a>=40 && a<48){ex-=15;ey=toward(ey,e[5],2);}
                    else if(a>=48 && a<64)ex=Math.min(152,ex+8);
                }
                if(ex>=8 && ex<=39 && Math.abs(ey-y)<=10)value+=1000/(t+1);
            }
        }
        if(value<best.value)best={value,y:dest};
    }
    return best.y<s.y?'keypad-8':best.y>s.y?'keypad-2':null;
}

// Coarse visual decisions every 0.3 seconds, applied after 0.2 seconds.
// No hidden HP, future spawn tables, projectile velocity or invulnerability checks.
// The recording is automated key playback with these human-like limits.
export function humanPolicy() {
    let tick=0,goal=32,pending=32,react=0;
    return s=>{
        tick++;
        if(s.phase!==1)return null;
        if(tick%3===0){
            const enemies=s.enemies.filter(e=>e[0] && e[2]<188);
            const target=enemies.slice().sort((a,b)=>a[2]-b[2])[0];
            pending=s.item[0] && s.item[1]<88?s.item[2]:(target?.[3]??32);
            // React to nearby visible hazards, committing to a broad empty lane.
            const danger=s.bolts.find(b=>b.x/16<90 && b.x/16>15 && Math.abs(b.y/16-s.y)<10);
            const charger=enemies.find(e=>e[2]<106 && Math.abs(e[3]-s.y)<13 && e[0]!==3);
            if(danger || charger){const y=danger?danger.y/16:charger[3];pending=y<32?46:18;}
            react=3;
        }
        if(react>0 && --react===0)goal=Math.max(16,Math.min(48,pending));
        // Two-pixel tolerance prevents frame-by-frame perfect tracking.
        return goal<s.y-2?'keypad-8':goal>s.y+2?'keypad-2':null;
    };
}
