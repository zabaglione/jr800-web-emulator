// SPDX-License-Identifier: MIT
// Test/recording input only. No game logic is implemented by the browser host.
export function state(f) {
    const bytes=(name,n)=>[...f.read(name,n)],e=bytes('enemies',20),b=bytes('bolts',16),s=bytes('shots',8);
    const split=(v,n)=>Array.from({length:v.length/n},(_,i)=>v.slice(i*n,(i+1)*n));
    return {
        phase:f.read('phase'),stage:f.read('stage'),sector:f.read('sector'),score:f.read('score')+100*f.read('score_hi'),hull:f.read('hull'),
        weapon:f.read('weapon'),shield:f.read('shield'),x:f.read('ship_x'),y:f.read('ship_y'),
        tick:f.read('def_tick'),firing:f.read('fire_enabled'),charge:f.read('charge'),message:f.read('message_ticks'),invulnerable:f.read('invulnerable'),
        pause:f.read('pause_ticks'),cause:f.read('hit_cause'),inverted:f.read('screen_inverted'),
        audio:f.read('audio_id'),spawnLeft:f.read('spawn_left'),item:bytes('item',4),
        enemies:split(e,10),shots:split(s,4),gates:split(bytes('gates',6),3).filter(g=>g[0]),
        swarm:split(bytes('swarm',24),4).filter(e=>e[0]),
        bolts:split(b,8).filter(b=>b[0]).map(b=>({life:b[0],x:b[1]*256+b[2],y:b[3]*256+b[4],
            dx:(b[5]*256+b[6])<<16>>16,dy:b[7]<<24>>24})),
    };
}
const toward=(a,b,n)=>a+Math.sign(b-a)*Math.min(n,Math.abs(b-a));
const reflect=y=>y<18?36-y:y>46?92-y:y;
// Deterministic collision forecast used for coverage. Gameplay itself never calls this.
export function chooseInput(s,{collect=true,evade=true,front=true}={}) {
    if(s.phase!==1)return [];
    const enemies=s.enemies.filter(e=>e[0]),target=enemies.slice().sort((a,b)=>a[2]-b[2])[0];
    const seeking=collect && s.item[0] && s.item[1]<150;
    const goalY=seeking?s.item[2]:(target?.[3]??s.swarm.find(e=>e[0]>s.x+16)?.[1]??32);
    const goalX=seeking?Math.min(96,Math.max(24,s.item[1]-12)):(front?72:24);
    let best={value:Infinity,x:s.x,y:s.y};
    for(const destX of [16,24,40,56,72,88,104,112])for(let destY=16;destY<=48;destY+=2) {
        let value=Math.abs(destY-goalY)*.4+Math.abs(destX-goalX)*.08+Math.abs(destY-s.y)*.018+Math.abs(destX-s.x)*.012;
        for(let t=1;t<=22;t++) {
            const x=toward(s.x,destX,2*t),y=toward(s.y,destY,2*t);
            if(!evade || s.invulnerable>=t)continue;
            const penalty=5000/(t+1);
            for(const g of s.gates) {
                const gx=g[0]-2*t,gy=g[1]+(s.sector===2?(g[2]<<24>>24)*Math.floor(t/8):0);
                if(Math.abs(gx-x)<=16 && (s.sector===2?Math.abs(y-gy)<=14:Math.abs(y-gy)>=7))value+=penalty;
            }
            for(const b of s.bolts) {
                if(b.life<=t)continue;
                if(Math.abs((b.x+b.dx*t)/16-x)<=9 && Math.abs(y-(b.y+b.dy*t)/16)<=7)value+=penalty;
            }
            for(const e of s.swarm) {
                const ex=e[0]-e[3]*t,ey=reflect(e[1]+(e[2]<<24>>24)*t);
                if(Math.abs(ex-x)<=13 && Math.abs(ey-y)<=8)value+=penalty;
            }
            for(const e of enemies) {
                if(e[4]&128)continue;
                let ex=e[2],ey=e[3];
                if(e[0]===2)for(let n=1;n<=t;n++){const a=e[4]+n;if(a>=8 && a<16)ex++;else if(a>=16 && a<40)ex-=4;else if(a>=48)ex-=3;}
                if(e[0]===4){ex-=t;ey=reflect(ey+(e[6]<<24>>24)*t);}
                if(e[0]>=6 && !(s.sector===2 && e[4]<20)){ex-=(e[0]===6?2:4)*t;ey=reflect(ey+(e[6]<<24>>24)*t);}
                if(e[0]===5){
                    if(s.sector && e[4]>=20 && e[4]+t>=28 && e[4]+t<=32 && x<ex && Math.abs(y-e[5])<=8)value+=penalty;
                    for(let n=1;n<=t;n++){
                        const a=e[4]+n;
                        if(s.sector===1 && a>=48 && a<72)ey+=a<60?-1:1;
                        else if(a>=40 && a<48){ex++;if(s.sector===0)ey=toward(ey,e[4]>=40?e[5]:s.y,4);}
                        else if(a>=48 && a<56)ex-=s.sector===2?11:15;
                        else if(a>=56 && a<72)ex=Math.min(152,ex+8);
                    }
                }
                if(Math.abs(ex-x)<=(e[0]===5 && s.sector===2?26:18) && Math.abs(ey-y)<=(e[0]===5 && s.sector===2?23:12))value+=penalty;
            }
        }
        if(value<best.value)best={value,x:destX,y:destY};
    }
    return [best.y<s.y?'keypad-8':best.y>s.y?'keypad-2':null,best.x<s.x?'keypad-4':best.x>s.x?'keypad-6':null].filter(Boolean);
}
// The demonstration observes every third update and holds each coarse decision.
// Unlike the coverage policy it uses only visible positions and warning cues.
export function humanPolicy() {
    let tick=0,goalX=40,goalY=32;
    return s=>{
        if(s.phase!==1)return [];
        if(++tick%3===0){
            const target=s.enemies.find(e=>e[0]);goalY=s.item[0] && s.item[1]<110?s.item[2]:target?.[3]??32;
            goalX=s.item[0] && s.item[1]<110?72:56;
            const wall=s.gates.find(g=>g[0]>s.x-14 && g[0]<s.x+60);
            if(wall){goalY=s.sector===2?(wall[1]<32?48:16):wall[1];goalX=24;}
            const bolt=s.bolts.find(b=>b.x/16<s.x+80 && b.x/16>s.x-12 && (Math.abs(b.y/16-s.y)<12 || Math.abs(b.y/16-goalY)<12));
            const body=[...s.enemies.filter(e=>e[0]).map(e=>[e[2],e[3]]),...s.swarm].find(e=>e[0]<s.x+50 && e[0]>s.x-14 && Math.abs(e[1]-s.y)<12);
            if(!wall && (bolt || body)){goalY=(bolt?bolt.y/16:body[1])<32?46:18;goalX=24;}
            if(target?.[0]===5 && ((s.sector && target[4]>=20 && target[4]<33) || (s.sector!==1 && target[4]>=40 && target[4]<56))){goalY=target[5]<32?46:18;goalX=16;}
        }
        return [goalY<s.y-2?'keypad-8':goalY>s.y+2?'keypad-2':null,goalX<s.x-2?'keypad-4':goalX>s.x+2?'keypad-6':null].filter(Boolean);
    };
}
