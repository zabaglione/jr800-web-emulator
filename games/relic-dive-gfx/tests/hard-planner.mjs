// SPDX-License-Identifier: MIT
// Only the unmodified emulator can validate a selected search path.
import assert from "node:assert/strict";
import {createSearchModel, directions} from "./search-model.mjs";
export function chooseHardAction({target,recentPositions=[],initialNode,quiet=false,progressWeight=48,width=256,perCell=12,diverse=true}) {
    const initial=initialNode.state,map=initialNode.map,model=createSearchModel(map);
    const distances=new Map([[target,0]]),queue=[target];
    for(let i=0;i<queue.length;i++)for(const [dx,dy]of directions){
        const p=queue[i],x=p%24,y=Math.floor(p/24),n=(y+dy)*24+x+dx;
        if(n<0||n>=384||!(map[n]&127)||!(map[y*24+x+dx]&127)||!(map[(y+dy)*24+x]&127)||distances.has(n))continue;
        distances.set(n,distances.get(p)+1);queue.push(n);
    }
    const distance0=distances.get(initial[9]*24+initial[8]);
    let targetItem=-1;for(let i=640;i<736;i+=3)if(map[i+2]&&map[i]+map[i+1]*24===target){targetItem=i;break;}
    const enemyHp=m=>{let hp=0,n=0;for(let i=387;i<640;i+=8){hp+=m[i];if(m[i])n++;}return [hp,n];};
    const [hp0,n0]=enemyHp(map);
    const result=node=>{const steps=[];for(let p=node;p.parent;p=p.parent)steps.push({...p.action,expected:p.state.slice(6,27),expectedBag:p.state.slice(30,44),expectedPoison:p.state.slice(59,61),expectedMap:p.map,expectedState:p.state});return steps.reverse();};
    let beam=[{state:initial,map,score:0}],best=null,finishAt=Infinity,nodes=0;
    const start=performance.now();
    for(let depth=0;depth<96;depth++){
        const next=[],seen=new Set();
        for(const parent of beam){
            const actions=directions.map(d=>({key:d[2]}));actions.push({key:11});
            const s=parent.state, seenItems=new Set();
            for(let slot=0;slot<6;slot++){
                const item=s[30+slot],effect=s[37+item-2];
                if(seenItems.has(item))continue;seenItems.add(item);
                if(item===1&&s[12]<160||item>=2&&item<=4&&(effect===0&&(s[10]<=s[11]-5||s[59])||effect===2&&!s[19])
                    ||item>=5&&item<=6&&effect===4||item>=7&&item<=9&&item>s[42]||item>=10&&item<=12&&item>s[43])actions.push({slot});
                if(!s.slice(30,36).includes(0)&&((item>=7&&item<=9&&item<=s[42]||item>=10&&item<=s[43]&&(item<12||s.slice(30,36).filter(x=>x===12).length>1))||item>=2&&item<=6&&[1,3].includes(effect)||item>=5&&item<=6&&effect===4&&s.slice(30,36).filter(x=>x===item).length>1))actions.push({slot,discard:true});
            }
            for(const action of actions){
                const child=model.step(parent,action);nodes++;
                if(!child||child.state[5]===8)continue;
                const {state:s,map:m}=child,pos=s[9]*24+s[8];
                const id=Buffer.concat([s.slice(6,27),s.slice(30,44),s.slice(59,61),m]).toString('base64');
                if(seen.has(id))continue;seen.add(id);
                const [hp,n]=enemyHp(m);
                let score=(s[10]-initial[10])*40+(s[13]-initial[13])*200+(s[14]-initial[14])*250
                    +(s[12]-initial[12])*16+(hp0-hp)*(initial[15]<32?12:2)+(s[15]<32?(s[16]-initial[16])*26:0)
                    +(distance0-(distances.get(pos)??distance0))*progressWeight-Math.max(0,s[59]-initial[59])*5-depth;
                for(let item=1;item<=12;item++){
                    const gain=[...s.slice(30,36)].filter(x=>x===item).length-[...initial.slice(30,36)].filter(x=>x===item).length;
                    if(item>=7&&item<=9&&item>initial[42]||item>=10&&item<=12&&item>initial[43])score+=gain*50;
                    if(item===12)score+=(s.slice(30,36).includes(12)?100:0)-(initial.slice(30,36).includes(12)?100:0);
                    if(item===1)score+=gain*(initial[12]<70?40:10);
                    if((item>=7&&item<=9&&item<=initial[42]||item>=10&&item<=initial[43]&&item<12)||item>=2&&item<=6&&[1,3].includes(initial[37+item-2]))score-=gain*30;
                }
                score-=Math.min(30,recentPositions.filter(p=>p===pos).length*5);
                assert.ok(Number.isFinite(score),`Nonfinite search score: distance=${distance0}, pos=${pos}`);
                const node={...child,parent,action,score,remainingHp:hp};
                const stairCost=(s[59]&&!(s[60]+1&1)?1:0)+(!s[12]&&!(s[18]+1&1)?1:0);
                const acquired=targetItem<0||m[targetItem+2]!==map[targetItem+2];
                if(s[5]===9||pos===target&&acquired&&((m[pos]&127)!==3||s[10]>stairCost)){
                    if(s[5]===9)return result(node);
                    if(!best||node.score>best.score)best=node;
                    finishAt=Math.min(finishAt,depth+3);
                }
                if(s[5]===1)next.push(node);
            }
        }
        if(depth>=finishAt||!next.length)break;
        next.sort((a,b)=>b.score-a.score);
        if(!diverse){
            const counts=new Map();beam=[];
            for(const node of next){const pos=node.state[9]*24+node.state[8],n=counts.get(pos)??0;if(n>=perCell)continue;counts.set(pos,n+1);beam.push(node);if(beam.length>=width)break;}
            continue;
        }
        const groups=new Map();beam=[];
        for(const node of next){const pos=node.state[9]*24+node.state[8];if(!groups.has(pos))groups.set(pos,[]);groups.get(pos).push(node);}
        const selected=new Set();
        for(let pass=0;pass<perCell&&beam.length<width;pass++)for(const group of groups.values()){
            const node=pass===1?group.reduce((a,b)=>b.remainingHp<a.remainingHp?b:a):group[pass===0?0:pass-1];
            if(!node||selected.has(node))continue;selected.add(node);beam.push(node);if(beam.length>=width)break;
        }
    }
    if(!quiet)console.log({searchTarget:target,nodes,ms:Math.round(performance.now()-start),steps:best?result(best).length:0});
    return best?result(best):[];
}
export function chooseFloorPlan({initialNode,width=256,perCell=12,waypoints=false,diverse=true}) {
    const initial={state:initialNode.state,map:initialNode.map,steps:[],visited:new Set()};
    const goal=initial.map.slice(0,384).findIndex(t=>[3,4].includes(t&127));
    const value=node=>{
        const s=node.state;let food=s[12];for(const item of s.slice(30,36))if(item===1)food+=45;
        let score=s[10]*40+s[11]*4+Math.min(food,200)*16+Math.max(0,food-200)*2+s[13]*250+s[14]*600-s[59]*10-node.steps.length;
        for(const item of s.slice(30,36)){

            if(item>=2&&item<=4&&s[37+item-2]===0)score+=160;
            if(item>=5&&item<=6&&s[37+item-2]===4)score+=30;

        }
        if(s.slice(30,36).includes(12))score+=250;
        return score;
    };
    let beam=[initial],best=null,searches=0;
    const exits=[], routeCache=new Map();
    const start=performance.now();
    for(let stage=0;stage<6;stage++){
        const next=[];
        for(const parent of beam){
            const s=parent.state,m=parent.map,targets=[goal];
            if(waypoints)for(const pos of [108,116,300,308])if(pos!==s[9]*24+s[8]&&!parent.visited.has(pos))targets.push(pos);
            for(let i=640;i<736;i+=3){
                const item=m[i+2];
                if(item===1&&s[12]<170||item>=7&&item<=9&&item>s[42]||item>=10&&item<=12&&item>s[43]
                    ||item===12&&!s.slice(30,36).includes(12)||item>=2&&item<=4&&s[37+item-2]===0){
                    const pos=m[i+1]*24+m[i];if(pos!==s[9]*24+s[8])targets.push(pos);
                }
            }
            for(const target of [...new Set(targets)]){
                const routeKey=target+":"+Buffer.concat([s,m]).toString("base64");
                let searched=routeCache.get(routeKey);
                if(!searched){searched=chooseHardAction({initialNode:parent,target,quiet:true,width,perCell,diverse});routeCache.set(routeKey,searched);searches++;}
                const candidates=[searched,directRoute(parent,target),directRoute(parent,target,true),tacticalRoute(parent,target)];
                for(const plan of candidates){
                if(!plan.length)continue;
                const last=plan.at(-1),node={state:last.expectedState,map:last.expectedMap,steps:[...parent.steps,...plan],visited:new Set([...parent.visited,target])};
                node.score=value(node);
                if(node.state[5]===9)return node.steps;
                if(node.state[5]===9||node.state[9]*24+node.state[8]===goal){exits.push(node);if(!best||node.score>best.score)best=node;}
                else next.push(node);
                }
            }
        }
        next.sort((a,b)=>b.score-a.score);beam=[];
        const seenNodes=new Set();
        for(const node of next){const key=Buffer.concat([node.state,node.map]).toString("base64")+":"+[...node.visited].sort((a,b)=>a-b).join(",");if(seenNodes.has(key))continue;seenNodes.add(key);beam.push(node);if(beam.length===4)break;}
        if(!beam.length)break;
    }
    console.log({floorSearches:searches,ms:Math.round(performance.now()-start),steps:best?.steps.length??0,exitHp:best?.state[10],exitFood:best?.state[12],exitDefense:best?.state[14]});
    if(!best&&width===256)return chooseFloorPlan({initialNode:initial,width:1024,perCell:40,waypoints,diverse});
    const result=best?.steps??[];
    exits.sort((a,b)=>b.score-a.score);
    const rngs=new Set();result.alternatives=[];
    for(const node of exits){const rng=node.state[25]*256+node.state[26];if(rngs.has(rng))continue;rngs.add(rng);result.alternatives.push(node.steps);if(result.alternatives.length>=4)break;}
    return result;
}

// A forced shortest-route candidate prevents the beam from discarding a
// necessary, temporarily costly fight at a corridor entrance.
export function directRoute(initial,target,avoidRust=false) {
    const model=createSearchModel(initial.map),distances=new Map([[target,0]]),q=[target],map=initial.map;
    for(let i=0;i<q.length;i++)for(const [dx,dy]of directions){
        const p=q[i],x=p%24,y=Math.floor(p/24),n=(y+dy)*24+x+dx;
        if(n<0||n>=384||!(map[n]&127)||!(map[y*24+x+dx]&127)||!(map[(y+dy)*24+x]&127)||distances.has(n))continue;
        distances.set(n,distances.get(p)+1);q.push(n);
    }
    let node=initial;const steps=[];
    const act=action=>{
        const next=model.step(node,action);if(!next||next.state[5]===8)return false;
        node=next;const s=node.state;steps.push({...action,expected:s.slice(6,27),expectedBag:s.slice(30,44),expectedPoison:s.slice(59,61),expectedMap:node.map,expectedState:s});return true;
    };
    for(let turn=0;turn<180;turn++){
        const s=node.state,pos=s[9]*24+s[8];let used=false;
        for(let slot=0;slot<6;slot++){
            const item=s[30+slot],effect=s[37+item-2];
            if(item===1&&s[12]<150||item>=2&&item<=4&&effect===0&&(s[10]<=s[11]-5||s[59])
                ||item>=2&&item<=4&&effect===2&&!s[19]&&Array.from({length:32},(_,i)=>384+i*8).some(i=>node.map[i+2]===8&&node.map[i+3]&&Math.abs(node.map[i]-s[8])+Math.abs(node.map[i+1]-s[9])<=4)
                ||item>=7&&item<=9&&item>s[42]||item>=10&&item<=12&&item>s[43]){
                if(act({slot})){used=true;break;}
            }
        }
        if(used)continue;
        if(pos===target){
            for(let i=640;i<736;i+=3)if(initial.map[i+2]&&initial.map[i]+initial.map[i+1]*24===target&&node.map[i+2]===initial.map[i+2])return [];
            const cost=(s[59]&&!(s[60]+1&1)?1:0)+(!s[12]&&!(s[18]+1&1)?1:0);
            return (node.map[pos]&127)===3&&s[10]<=cost?[]:steps;
        }
        if(!s.slice(30,36).includes(0)){
            for(let slot=0;slot<6;slot++){
                const item=s[30+slot];
                if((item>=7&&item<=9&&item<=s[42]||item>=10&&item<=s[43]&&(item<12||s.slice(30,36).filter(x=>x===12).length>1))||item>=2&&item<=6&&[1,3].includes(s[37+item-2])||item>=5&&item<=6&&s[37+item-2]===4&&s.slice(30,36).filter(x=>x===item).length>1){
                    if(act({slot,discard:true})){used=true;break;}
                }
            }
        }
        if(used)continue;
        const d=directions.find(([dx,dy])=>(node.map[s[9]*24+s[8]+dx]&127)&&(node.map[(s[9]+dy)*24+s[8]]&127)&&distances.get((s[9]+dy)*24+s[8]+dx)===distances.get(pos)-1);
        if(!d)return [];
        let key=d[2];const ahead=model.step(node,{key});
        if(avoidRust&&ahead&&ahead.state[14]<s[14]){
            let bestSafe=null;
            for(const [, ,candidate]of directions){
                const safe=model.step(node,{key:candidate});if(!safe||safe.state[5]===8||safe.state[14]<s[14])continue;
                let score=-(distances.get(safe.state[9]*24+safe.state[8])??100)+(safe.state[10]-s[10])*50;
                for(let i=384;i<640;i+=8)if(node.map[i+2]===11&&node.map[i+3]){
                    score+=(node.map[i+3]-safe.map[i+3])*100;
                    if(!safe.map[i+3])score+=1000;
                    else if(Math.abs(safe.map[i]-safe.state[8])===1&&Math.abs(safe.map[i+1]-safe.state[9])===1)score+=500;
                }
                if(!bestSafe||score>bestSafe.score)bestSafe={key:candidate,score};
            }
            if(bestSafe)key=bestSafe.key;
        }
        if(!act({key}))return [];
    }
    return [];
}

function tacticalRoute(initial,target) {
    const model=createSearchModel(initial.map),distances=new Map([[target,0]]),q=[target],map=initial.map;
    for(let i=0;i<q.length;i++)for(const [dx,dy]of directions){
        const p=q[i],x=p%24,y=Math.floor(p/24),n=(y+dy)*24+x+dx;
        if(n<0||n>=384||!(map[n]&127)||!(map[y*24+x+dx]&127)||!(map[(y+dy)*24+x]&127)||distances.has(n))continue;
        distances.set(n,distances.get(p)+1);q.push(n);
    }
    let node=initial;const steps=[],visits=new Map();
    const act=action=>{
        const next=model.step(node,action);if(!next||next.state[5]===8)return false;
        node=next;const s=node.state;steps.push({...action,expected:s.slice(6,27),expectedBag:s.slice(30,44),expectedPoison:s.slice(59,61),expectedMap:node.map,expectedState:s});return true;
    };
    for(let turn=0;turn<180;turn++){
        const s=node.state,pos=s[9]*24+s[8];let used=false;
        for(let slot=0;slot<6;slot++){
            const item=s[30+slot],effect=s[37+item-2];
            if(item===1&&s[12]<150||item>=2&&item<=4&&effect===0&&(s[10]<=s[11]-5||s[59])
                ||item>=2&&item<=4&&effect===2&&!s[19]&&Array.from({length:32},(_,i)=>384+i*8).some(i=>node.map[i+2]===8&&node.map[i+3]&&Math.abs(node.map[i]-s[8])+Math.abs(node.map[i+1]-s[9])<=4)
                ||item>=7&&item<=9&&item>s[42]||item>=10&&item<=12&&item>s[43]){
                if(act({slot})){used=true;break;}
            }
        }
        if(used)continue;
        if(pos===target){
            for(let i=640;i<736;i+=3)if(initial.map[i+2]&&initial.map[i]+initial.map[i+1]*24===target&&node.map[i+2]===initial.map[i+2])return [];
            const cost=(s[59]&&!(s[60]+1&1)?1:0)+(!s[12]&&!(s[18]+1&1)?1:0);
            return (node.map[pos]&127)===3&&s[10]<=cost?[]:steps;
        }
        if(!s.slice(30,36).includes(0)){
            for(let slot=0;slot<6;slot++){
                const item=s[30+slot];
                if((item>=7&&item<=9&&item<=s[42]||item>=10&&item<=s[43]&&(item<12||s.slice(30,36).filter(x=>x===12).length>1))||item>=2&&item<=6&&[1,3].includes(s[37+item-2])||item>=5&&item<=6&&s[37+item-2]===4&&s.slice(30,36).filter(x=>x===item).length>1){
                    if(act({slot,discard:true})){used=true;break;}
                }
            }
        }
        if(used)continue;
        const d=directions.find(([dx,dy])=>(node.map[s[9]*24+s[8]+dx]&127)&&(node.map[(s[9]+dy)*24+s[8]]&127)&&distances.get((s[9]+dy)*24+s[8]+dx)===distances.get(pos)-1);
        if(!d)return [];
        let key=d[2];
        let baseHp=0;for(let i=387;i<640;i+=8)baseHp+=node.map[i];
        const score=n=>{const t=n.state;let hp=0;for(let i=387;i<640;i+=8)hp+=n.map[i];
            return (t[10]-s[10])*40+(t[14]-s[14])*600+(t[12]-s[12])*12
                +(distances.get(pos)-(distances.get(t[9]*24+t[8])??100))*48+(baseHp-hp)*6;};
        let beam=[{node,first:key}];
        for(let horizon=0;horizon<4;horizon++){
            const next=[];
            for(const parent of beam){
                if(parent.done){next.push(parent);continue;}
                for(const candidate of [...directions.map(d=>d[2]),11]){
                const n=model.step(parent.node,{key:candidate});if(!n||n.state[5]===8)continue;
                next.push({node:n,first:horizon===0?candidate:parent.first,score:score(n),done:n.state[5]===9||n.state[8]+n.state[9]*24===target});
            }}
            next.sort((a,b)=>b.score-a.score);beam=next.slice(0,6);if(!beam.length)break;
        }
        if(beam.length)key=beam[0].first;
        const visitKey=pos+":"+baseHp,count=(visits.get(visitKey)??0)+1;visits.set(visitKey,count);if(count>2)key=d[2];
        if(!act({key}))return [];
    }
    return [];
}
