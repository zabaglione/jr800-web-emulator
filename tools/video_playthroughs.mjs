// SPDX-License-Identifier: MIT
// Human-paced keyboard demonstrations, kept outside the emulator and game code.
import {pawnMoves} from '../games/tools/pawn_check.mjs';
import {area,neighbor} from '../games/grid-claim/check.mjs';
const directions=['up','down','left','right'];
function walk(p,q,w){return q===p-w?'up':q===p+w?'down':q===p-1?'left':'right';}
function path(start,target,neighbors){
 const todo=[[start,[]]],seen=new Set([String(start)]);
 for(const [p,route] of todo){if(String(p)===String(target))return route;
  for(const [q,key] of neighbors(p)){if(seen.has(String(q)))continue;seen.add(String(q));todo.push([q,[...route,key]]);}
 }throw new Error('No keyboard route');
}
function gridPath(v,target,w,n,allowed=()=>true){
 const route=path(v.read('cursor'),target,p=>directions.map((key,i)=>[p+[-w,w,-1,1][i],key]).filter(([q])=>q>=0&&q<n&&Math.abs(q%w-p%w)<=1&&allowed(q)));
 for(const key of route)v.tap(key,0.19);
}
function puzzle(v){
 while(true){const stage=v.data('challenges').stages[v.read('stage')], id=v.id;
  const route=['lamp-grid','mirror-link','slide-nine','pipe-weave','loop-trace','mine-field'].includes(id)?stage.bonus:stage.normal;
  for(const a of route){
   v.settle();
   if(['box-shift','slide-nine','ice-route','switch-maze','number-rail','step-strike'].includes(id)){
    if(a==='wait')v.menu(1);else v.tap(a==='fire'?'space':a,id==='number-rail'?0.9:0.38);
    if(id==='ice-route')v.until(()=>v.read('ice_mode')===1||v.read('ice_mode')===2,8);
   }else if(['lamp-grid','mirror-link','pipe-weave','knight-tour'].includes(id)){
    v.aim(a,{ 'lamp-grid':5,'mirror-link':16,'pipe-weave':6,'knight-tour':6}[id]);v.wait(0.25);v.tap('space',0.7);
   }else if(id==='loop-trace')v.tap(a==='close'?'space':walk(v.read('cursor'),a,6),0.5);
   else if(id==='peg-rescue'){
    for(const p of a){gridPath(v,p,7,49,q=>v.read('board',49)[q]!==255);v.tap('space',0.6);}
   }else if(id==='mine-field'){
    const [kind,p]=a,flag=kind==='flag';
    if(!!v.read('selection_active')!==flag){
     while(!v.read('mine_control'))v.tap('down',0.19);v.tap('space',0.5);
    }v.aim(p,14);v.tap('space',0.6);
   }else if(id==='ricochet-ops'){
    v.aim(40+a[0],8);while(v.read('rico_aim')!==a[1])v.tap('up');v.tap('space',0.5);v.settle();v.wait(0.5);
   }else if(id==='pocket-factory'){
    const tool=a[1]?a[1]-6:6;
    if(v.read('tool')!==tool){
     while(!v.read('selection_active'))v.tap('left',0.16);while(v.read('tool')!==tool)v.tap('right',0.19);v.tap('space',0.35);
    }v.aim(a[0],16);v.tap('space',0.38);
   }
  }
  if(id==='pocket-factory'){v.menu(2);v.until(()=>v.read('phase')===2,25);}
  v.next();
 }
}
function board(v){
 const id=v.id;let turn=0;
 while(true){
  v.settle();if(v.read('phase')!==2){v.next();turn=0;continue;}v.wait(0.45);
  if(id==='line-four'){
   const cols=[3,2,4,3,1,5,2,4,0,6],b=v.read('board',42);let c=cols[turn++%cols.length];if(b[c])c=b.slice(0,7).indexOf(0);
   v.aim(c,7);v.tap('space',1.05);
  }else if(id==='reversi-mini'){
   const moves=v.read('rev_legal',36).map((flips,p)=>({p,flips})).filter(a=>a.flips);
   const weight=p=>[0,5,30,35].includes(p)?30:0;moves.sort((a,b)=>weight(b.p)+b.flips-weight(a.p)-a.flips);
   if(!moves.length){v.wait(0.3);continue;}v.aim(moves[0].p,6);v.tap('space',0.5);v.settle();v.wait(0.5);
  }else if(id==='five-stones'||id==='hex-front'){
   const action=v.data('replays')[1][turn++];if(action===undefined){v.wait(1);continue;}
   const p=typeof action==='number'?action:action.p;
   if(action.relay){v.menu(1);}v.aim(p,id==='five-stones'?14:6);v.tap('space',1.0);
  }else if(id==='pawn-race'){
   const b=v.read('board',36),moves=pawnMoves(b,1).sort((a,c)=>(b[c[1]]?15:0)-(b[a[1]]?15:0)+Math.floor(a[1]/6)-Math.floor(c[1]/6));
   for(const p of moves[0]){v.aim(p,6);v.tap('space',0.55);}v.wait(0.45);
  }else if(id==='dot-claim'){
   const edges=[];for(let y=0;y<7;y++)for(let x=0;x<9;x++)if((x+y)%2)edges.push([x,y]);
   const b=v.read('board',43),target=b.slice(0,31).findIndex(n=>!n);
   const queue=[[v.read('dot_cursor'),v.read('dot_column'),[]]],seen=new Set();let route;
   for(const [e,col,keys] of queue){if(e===target){route=keys;break;}for(const key of directions){let [x,y]=edges[e],c=col;
    if(key==='up'||key==='down')y=Math.max(0,Math.min(6,y+(key==='up'?-1:1)));
    else {x+=key==='left'?-2:2;if(x<0||x>8)continue;c=x;}
    const n=edges.map(([X,Y],i)=>({i,Y,d:Math.abs(X-c)})).filter(q=>q.Y===y).sort((a,b)=>a.d-b.d||a.i-b.i)[0].i;
    const tag=n+':'+c;if(!seen.has(tag)){seen.add(tag);queue.push([n,c,[...keys,key]]);}
   }}for(const key of route)v.tap(key,0.2);v.tap('space',0.55);v.settle();
  }
 }
}
function card(v){
 const id=v.id;
 if(id==='ace-stack'||id==='suit-run'){
  const positions=[];for(let row=0;row<7;row++)for(let col=0;col<=row;col++)positions.push([7-row+col*2,row]);positions.push([18,3]);
  const nav=positions.map(([x,y])=>[[0,-1],[0,1],[-1,0],[1,0]].map(([dx,dy],d)=>{
   const picks=positions.map(([X,Y],p)=>({p,along:(X-x)*dx+(Y-y)*dy,score:2*((X-x)*dx+(Y-y)*dy)+3*Math.abs((X-x)*dy-(Y-y)*dx)})).filter(a=>a.along>0).sort((a,b)=>a.score-b.score||a.p-b.p);return picks[0]?[picks[0].p,directions[d]]:null;
  }).filter(Boolean));
  while(true){for(const a of v.data('solutions').steps[v.read('stage')]){
   if(a[0]==='draw'){
    if(id==='suit-run'){while(!v.read('suit_action'))v.tap('down',0.2);v.tap('space',0.7);v.tap('up');}
    else v.menu(1);
   }else for(const p of a.slice(1)){
    if(id==='suit-run')v.aim(p,7);else for(const key of path(v.read('cursor'),p,n=>nav[n]))v.tap(key,0.22);
    v.tap('space',0.65);
   }
  }v.next();}
 }
 if(id==='dice-hold'){
  while(true){
   if(v.read('phase')!==2){v.next();continue;}
   const dice=v.read('board',5),counts=Array(7).fill(0);for(const d of dice)counts[d]++;const keep=counts.indexOf(Math.max(...counts));
   for(let p=0;p<5;p++)if(dice[p]===keep&&!v.read('dice_holds',5)[p]){v.aim(p,5);v.tap('space',0.4);}
   v.tap('down');if(v.read('dice_action')!==1)v.tap('left');v.tap('space',0.9);
   v.tap('right');v.tap('space',0.8);
   const scores=v.read('dice_scores',13),used=v.read('dice_used',13),best=scores.map((n,p)=>({p,n:used[p]?-1:n})).sort((a,b)=>b.n-a.n)[0].p;
   while(v.read('dice_category')!==best)v.tap('down',0.24);v.wait(0.45);v.tap('space',0.85);
   if(v.read('dice_action'))v.tap('up');
  }
 }
 if(id==='push-luck'){
  while(true){if(v.read('phase')!==2){v.next();continue;}if(v.read('luck_side')){v.wait(0.2);continue;}
   v.until(()=>v.read('luck_rolling')!==0,10);v.wait(0.5);
   const bank=v.read('luck_pot')>=8;
   if(v.read('luck_action')!==Number(bank))v.tap(bank?'right':'left');v.tap('space',1.2);
  }
 }
 if(id==='circuit-deck'){
  const cards=v.data('cards');
  while(true){
   if(v.read('phase')!==2){v.next();continue;}
   if(v.read('battle_mode')){v.wait(0.6);v.tap('space',1);continue;}
   const hand=v.read('hand',3),en=v.read('energy'),options=hand.map((id,i)=>({i,id,v:id===255||cards[id].stats[0]>en?-1:cards[id].stats[1]+cards[id].stats[4]*4+Math.min(40-v.read('hp'),cards[id].stats[3])+cards[id].stats[2]*0.7})).sort((a,b)=>b.v-a.v);
   const pick=options[0].v<0?3:options[0].i;
   while(v.read('selected')!==pick)v.tap('right');v.wait(0.35);v.tap('space',1.1);
  }
 }
}
function simulation(v){
 if(v.id==='orchard-days'){
  function button(choice){while(!v.read('orchard_button'))v.tap('down',0.19);if(v.read('orchard_button')!==choice)v.tap('right');v.tap('space',0.75);if(v.read('phase')===2)v.tap('up');}
  // Show both seed packets, then return to the first contract's planting plan.
  button(2);v.wait(0.5);button(2);
  while(true){for(const item of v.data('solutions')[v.read('stage')]){
   if(item==='next')button(1);
   else{if(item.seed&&v.read('orchard_seed')!==item.seed)button(2);v.aim(item.cell,6);v.tap('space',0.55);}
  }v.next();}
 }
 if(v.id==='market-harbor'){
  while(true){for(const a of v.data('solutions')[v.read('stage')]){
   while(v.read('market_good')!==a.good)v.tap('down');v.tap('left');for(let i=0;i<a.count;i++)v.tap('space',0.28);
   while(v.read('market_good')!==3)v.tap('down');v.tap('space',0.6);
   while(v.read('market_destination')!==a.dest)v.tap('down');v.wait(0.4);v.tap('space',0.85);
   while(v.read('market_good')!==a.good)v.tap('down');v.tap('right');for(let i=0;i<a.count&&v.read('phase')===2;i++)v.tap('space',0.3);
  }v.next();}
 }
 if(v.id==='rail-dispatch'){
  function running(value){if(!!v.read('dispatch_running')===value)return;v.tap('return',0.35);v.tap('down',0.2);v.tap('space',0.02);}
  function set(index,value){if((index===2?v.read('dispatch_switch'):v.read('dispatch_gates',2)[index])===value)return;while(v.read('dispatch_selected')!==index)v.tap('right',0.2);v.tap('space',0.25);}
  while(true){
   if(v.read('phase')!==2){v.next();continue;}
   running(false);set(0,0);set(1,0);running(true);
   v.until(()=>!v.read('dispatch_positions',4).some(p=>p===2||p===7),12);running(false);
   const positions=v.read('dispatch_positions',4),slot=positions.findIndex(p=>p===2||p===7),gate=positions[slot]===2?0:1,completed=v.read('dispatch_done');
   set(2,v.read('dispatch_targets',4)[slot]);set(gate,1);running(true);
   v.until(()=>v.read('dispatch_positions',4)[slot]===positions[slot],5);running(false);set(gate,0);running(true);
   v.until(()=>v.read('dispatch_done')===completed&&v.read('phase')===2,12);
  }
 }
}
function sports(v){
 if(v.id==='wind-putt'){
  while(true){for(const [angle,power] of v.data('solutions')[v.read('stage')]){
   while(v.read('putt_aim')!==angle)v.tap('right');while(v.read('putt_power')!==power)v.tap(v.read('putt_power')<power?'up':'down');v.wait(0.4);v.tap('space',0.5);v.until(()=>v.read('putt_left')>0,12);v.wait(0.5);
  }v.next();}
 }
 if(v.id==='rally-return'){
  while(true){if(v.read('phase')!==2){v.next();continue;}if(!v.read('rally_active')){v.release();v.tap('space',0.2);}
   const wanted=v.read('rally_y')-7,p=v.read('rally_player');v.hold('up',p>wanted+2);v.hold('down',p<wanted-2);v.wait(0.17);
  }
 }
 if(v.id==='penalty-arc'){
  while(true){if(v.read('phase')!==2){v.next();continue;}if(v.read('penalty_mode')===3)v.tap('space',0.5);
   const goal=v.read('penalty_keeper')<64?4:0;while(v.read('penalty_target')!==goal)v.tap('right',0.2);v.tap('down');v.tap('space',0.05);
   v.until(()=>v.read('penalty_power')!==6,5);v.tap('space',0.1);v.until(()=>v.read('penalty_mode')===2,8);v.wait(1);
  }
 }
 if(v.id==='beat-step'){
  const lane=['left','down','up','right'];
  while(true){if(v.read('phase')!==2){v.next();continue;}if(!v.read('beat_active'))v.tap('space',0.1);
   const chart=v.data('charts')[v.read('stage')],index=v.read('beat_index');
   v.until(()=>v.word('beat_time')<v.word('beat_due')-2,4);v.tap(lane[chart.notes[index]],0.08);
  }
 }
 if(v.id==='balance-dock'){
  while(true){if(v.read('phase')!==2){v.next();continue;}
   const mode=v.read('dock_mode');if(mode===0||mode===3){v.tap('space',0.18);continue;}
   if(mode===1&&Math.abs(v.read('dock_x')-v.read('dock_left'))<=4)v.tap('space',0.3);else v.wait(0.09);
  }
 }
 if(v.id==='arc-duel'){
  function predict(h,w,a,p){let x=16*256,y=(h[16]-7)*256,vx=Math.round(128*Math.cos((15+a*5)*Math.PI/180))*p,vy=-Math.round(128*Math.sin((15+a*5)*Math.PI/180))*p;
   for(let t=1;t<160;t++){x+=vx;y+=vy;vx+=w;vy+=32;const X=Math.floor(x/256),Y=Math.floor(y/256);if(X<0||X>=192)return null;if(Y>=h[X])return {x:X,y:Math.min(63,Y)};}return null;
  }
  while(true){if(v.read('phase')!==2){v.next();continue;}if(v.read('arc_mode')===4)v.tap('space',0.65);
   if(v.read('arc_mode')!==0){v.wait(0.15);continue;}
   const h=v.read('heights',192),wind=v.read('wind'),w=wind>127?wind-256:wind;let best;
   for(let a=0;a<13;a++)for(let p=2;p<=9;p++){const hit=predict(h,w,a,p);if(!hit)continue;const score=Math.abs(hit.x-175)+Math.abs(hit.y-h[175]+3);if(!best||score<best.score)best={a,p,score};}
   while(v.read('angle')!==best.a)v.tap(v.read('angle')<best.a?'up':'down',0.22);while(v.read('power')!==best.p)v.tap(v.read('power')<best.p?'right':'left',0.22);v.wait(0.5);v.tap('space',0.5);
  }
 }
}
function action(v){
 if(v.id==='target-range'){
  while(true){if(v.read('phase')!==2){v.next();continue;}if(!v.read('range_mode'))v.tap('space',0.2);
   if(v.read('range_mode')!==1||v.read('range_target')===255){v.wait(0.2);continue;}
   v.wait(0.2);v.aim(v.read('range_target'),3);v.tap('space',0.35);
  }
 }
 if(v.id==='orbit-guard'){
  while(true){if(v.read('phase')!==2){v.next();continue;}if(!v.read('orbit_running'))v.tap('space',0.2);
   const enemy=v.read('orbit_enemies',24).findIndex(Boolean);if(enemy<0){v.wait(0.18);continue;}
   const a=enemy%8,delta=(a-v.read('orbit_aim')+8)%8;
   v.tap(!delta?'space':a===0?'up':a===4?'down':delta<=4?'right':'left',0.19);
  }
 }
 if(v.id==='tail-trail'){
  while(true){if(v.read('phase')!==2){v.next();continue;}if(!v.read('tail_running'))v.tap('space',0.05);
   const p=v.read('cursor'),occupied=new Set(v.read('tail_body',v.read('tail_length')).slice(0,-1));
   const route=path(p,v.read('tail_food'),n=>directions.map((key,d)=>[n+[-14,14,-1,1][d],key,d]).filter(([q,key,d])=>q>=0&&q<98&&Math.abs(q%14-n%14)<=1&&!occupied.has(q)&&(n!==p||d!==(v.read('tail_direction')^1))));
   v.release();v.hold(route[0],true);v.until(()=>v.read('cursor')===p&&v.read('phase')===2,3);
  }
 }
 if(v.id==='maze-chase'){
  while(true){if(v.read('phase')!==2){v.next();continue;}if(!v.read('maze_running'))v.tap('space',0.1);
   const p=v.read('cursor'),b=v.read('board',105),targets=b.map((t,p)=>({t,p})).filter(a=>a.t>=2);
   const routes=targets.map(q=>path(p,q.p,n=>directions.map((key,d)=>[n+[-15,15,-1,1][d],key]).filter(([q])=>q>=0&&q<105&&b[q]!==1))).sort((a,b)=>a.length-b.length);
   if(!routes.length){v.wait(0.2);continue;}v.release();v.hold(routes[0][0],true);v.until(()=>v.read('cursor')===p&&v.read('phase')===2,3);
  }
 }
 if(v.id==='tower-leap'){
  let target=1;
  while(true){if(v.read('phase')!==2){v.next();continue;}
   if(v.read('tower_grounded')){target=Math.min(11,Math.floor(v.read('tower_height')/16)+1);v.release();v.tap('space',0.03);}
   const x=v.read('tower_x'),p=v.read('tower_platforms',12)[target]*8+10;v.hold('left',x>p+2);v.hold('right',x<p-2);v.wait(0.11);
  }
 }
 if(v.id==='gravity-run'){
  function route(course,row,direction,from){let states=[{row,direction,score:0,path:[]}];for(let d=from+1;d<=64;d++){const next=new Map();for(const s of states)for(const direction of [-1,1]){const row=Math.max(1,Math.min(5,s.row+direction)),code=course[d+3];if(code&(1<<(row-1)))continue;const n={row,direction,score:s.score+Number(code>>5===row),path:[...s.path,direction]},key=row+':'+direction;if(!next.has(key)||next.get(key).score<n.score)next.set(key,n);}states=[...next.values()];}return states.sort((a,b)=>b.score-a.score)[0]?.path;}
  let stage=-1,steps;
  while(true){if(v.read('phase')!==2){v.next();stage=-1;continue;}
   if(stage!==v.read('stage')||!v.read('gravity_running')){stage=v.read('stage');steps=route(v.data('levels')[stage],v.read('gravity_row'),v.read('gravity_direction'),v.read('gravity_distance'));if(!v.read('gravity_running'))v.tap('space',0.02);}
   const p=v.read('gravity_distance'),d=steps?.shift()??1;v.hold('up',d<0);v.hold('down',d>0);v.until(()=>v.read('gravity_distance')===p&&v.read('gravity_running')&&v.read('phase')===2,3);
  }
 }
 if(v.id==='grid-claim'){
  while(true){if(v.read('phase')!==2){v.next();continue;}if(v.read('claim_mode')!==1){v.release();v.wait(1);v.tap('space',0.3);continue;}
   const b=v.read('board',112),p=v.read('cursor'),dir=v.read('claim_dir'),choices=directions.map((key,d)=>({key,d,n:neighbor(p,d)})).filter(a=>a.d!==[1,0,3,2][dir]&&a.n!==255&&!b[a.n]);
   choices.sort((a,c)=>area(b,c.n)-area(b,a.n));v.release();v.hold(choices[0]?.key??'up',true);const s=v.read('claim_steps');v.until(()=>v.read('claim_steps')===s&&v.read('claim_mode')===1&&v.read('phase')===2,4);
  }
 }
 if(v.id==='star-patrol'){
  while(true){if(v.read('phase')!==2){v.next();continue;}if(!v.read('star_running'))v.tap('space',0.15);
   const p=v.read('cursor')%16,shot=v.read('star_shot'),enemies=v.read('star_enemies',20),offset=v.read('star_offset'),columns=enemies.map((n,i)=>n?i%10+offset:-1).filter(n=>n>=0&&n<16);
   const target=columns.sort((a,b)=>Math.abs(a-p)-Math.abs(b-p))[0]??p;
   if(shot===255&&target===p)v.tap('space',0.18);else if(target!==p)v.tap(target>p?'right':'left',0.18);else v.wait(0.15);
  }
 }
 if(v.id==='river-hop'){
  while(true){if(v.read('phase')!==2){v.next();continue;}
   const p=v.read('cursor'),x=p%14,y=Math.floor(p/14),lanes=v.read('river_lanes',56),homes=v.read('river_homes',5),goal=[1,4,7,10,12].filter((_,i)=>!homes[i]).sort((a,b)=>Math.abs(a-x)-Math.abs(b-x))[0];
   const safe=q=>{const X=q%14,Y=Math.floor(q/14),lane=[1,2,4,5].indexOf(Y);if(X<0||X>=14||q<0||q>=98)return false;if(Y===0)return X===goal;return lane<0||!!lanes[lane*14+X]===(lane<2);};
   let key;if(y===3&&x!==goal)key=x<goal?'right':'left';else if(safe(p-14))key='up';else if(safe(p+(x<goal?1:-1)))key=x<goal?'right':'left';
   if(key)v.tap(key,0.22);else v.wait(0.18);
  }
 }
 if(v.id==='bomb-vault'){
  function route(board,p,goal,crates=false){const q=[[0,p,[]]],cost=new Map([[p,0]]);while(q.length){q.sort((a,b)=>b[0]-a[0]);const [c,p,keys]=q.pop();if(goal(p))return keys;for(let d=0;d<4;d++){const n=p+[-15,15,-1,1][d],t=board[n];if(n<0||n>=105||t===1||(!crates&&[2,4].includes(t)))continue;const score=c+1+([2,4].includes(t)?15:0);if(score<(cost.get(n)??Infinity)){cost.set(n,score);q.push([score,n,[...keys,directions[d]]]);}}}}
  while(true){if(v.read('phase')!==2){v.next();continue;}const b=v.read('board',105),p=v.read('cursor'),targets=b.map((t,p)=>({t,p})).filter(a=>[4,5].includes(a.t)),goal=targets[0]?.p??88;
   const keys=route(b,p,q=>q===goal,true);if(!keys?.length){v.wait(0.3);continue;}const key=keys[0],next=p+[-15,15,-1,1][directions.indexOf(key)];
   if([2,4].includes(b[next])){
    const blast=new Set([p]);for(const d of [-15,15,-1,1])for(let n=1;n<=3;n++){const q=p+n*d;if(b[q]===1)break;blast.add(q);if([2,4].includes(b[q]))break;}
    const retreat=route(b,p,q=>!blast.has(q));if(retreat){v.tap('space',0.12);for(const k of retreat)v.tap(k,0.19);v.until(()=>v.read('bomb_fuse')||v.read('bomb_flame_time'),10);v.wait(0.4);}else v.tap('space',0.3);
   }else v.tap(key,0.3);
  }
 }
 throw new Error('No playthrough for '+v.id);
}
function relic(v){
 v.wait(0.75);v.tap('up',0.5);v.tap('space',1.3);
 while(true){
  if(v.read('G_MODE')!==1){v.wait(1.2);v.tap('space',1);continue;}
  const map=v.read('FLOORS',768),p=v.read('G_Y')*24+v.read('G_X'),goal=map.slice(0,384).findIndex(n=>[3,4].includes(n&127));
  if(p===goal){v.tap('space',0.5);v.tap('space',1.2);continue;}
  const bag=v.read('G_BAG',6),identities=v.read('G_IDENTITIES',5);
  const slot=bag.findIndex(item=>item===1&&v.read('G_FOOD')<160||item>=2&&item<=4&&identities[item-2]===0&&v.read('G_HP')<=v.read('G_MAX_HP')-10||item>=7&&item<=9&&item>v.read('G_WEAPON')||item>=10&&item<=12&&item>v.read('G_ARMOR'));
  if(slot>=0){v.tap('space',0.5);v.tap('space',0.5);for(let i=0;i<slot;i++)v.tap('down');v.tap('space',0.55);v.tap('space',0.7);continue;}
  const routes=[];if(bag.includes(0))for(let offset=640;offset<736;offset+=3){const item=map[offset+2],q=map[offset]+map[offset+1]*24;if(q!==p&&(item===1||item>=7&&item<=12))routes.push(path(p,q,n=>directions.map((key,d)=>[n+[-24,24,-1,1][d],key]).filter(([q])=>q>=0&&q<384&&(map[q]&127))));}
  if(!routes.length)routes.push(path(p,goal,n=>directions.map((key,d)=>[n+[-24,24,-1,1][d],key]).filter(([q])=>q>=0&&q<384&&(map[q]&127))));
  routes.sort((a,b)=>a.length-b.length);v.tap(routes[0][0],0.4);
 }
}
export function play(v){
 if(v.id==='relic-dive-gfx')return relic(v);
 v.start();
 if(['box-shift','mirror-link','step-strike','pocket-factory','lamp-grid','slide-nine','ice-route','switch-maze','knight-tour','peg-rescue','pipe-weave','number-rail','mine-field','loop-trace','ricochet-ops'].includes(v.id))return puzzle(v);
 if(['line-four','reversi-mini','five-stones','hex-front','pawn-race','dot-claim'].includes(v.id))return board(v);
 if(['ace-stack','suit-run','dice-hold','push-luck','circuit-deck'].includes(v.id))return card(v);
 if(['signal-ghost','relay-quest','echo-cavern'].includes(v.id)){
  while(true){for(const key of v.data('solutions')[v.read('stage')]){if(key==='heal')v.menu(1);else v.tap(key,key==='space'?0.6:0.34);}v.next();}
 }
 if(v.id==='micro-rogue'){
  while(true){
   if(v.read('phase')!==2){v.next();continue;}
   const b=v.read('board',98),p=v.read('cursor'),health=v.read('rogue_health',3),enemies=v.read('rogue_positions',3).filter((_,i)=>health[i]);
   if(v.read('rogue_pot')&&v.read('rogue_hp')<=v.read('rogue_max_hp')-8){v.tap('space',0.65);continue;}
   const adjacent=enemies.find(q=>[-14,14,-1,1].includes(q-p));if(adjacent!==undefined){v.tap(walk(p,adjacent,14),0.5);continue;}
   let targets=b.map((t,p)=>({t,p})).filter(a=>a.t===2&&v.read('rogue_sword')<5||a.t===3&&v.read('rogue_armor')<2||a.t===4&&v.read('rogue_pot')<2).map(a=>a.p);
   if(!targets.length)targets=enemies;if(!targets.length)targets=b.map((t,p)=>({t,p})).filter(a=>a.t===(v.read('rogue_relic')?6:5)).map(a=>a.p);
   const routes=targets.map(q=>path(p,q,n=>directions.map((key,d)=>[n+[-14,14,-1,1][d],key]).filter(([q])=>q>=0&&q<98&&b[q]!==1))).sort((a,b)=>a.length-b.length);
   v.tap(routes[0][0],0.4);
  }
 }
 if(v.id==='wall-break'){
  while(true){
   if(v.read('phase')!==2){v.next();continue;}
   if(!v.read('wall_active')){v.release();v.tap('space',0.1);}
   const target=Math.max(0,Math.min(106,v.read('wall_x')-10)),p=v.read('wall_paddle');
   v.hold('left',p>target+3);v.hold('right',p<target-3);v.wait(0.18);
  }
 }
 if(['orchard-days','market-harbor','rail-dispatch'].includes(v.id))return simulation(v);
 if(['wind-putt','rally-return','penalty-arc','beat-step','balance-dock','arc-duel'].includes(v.id))return sports(v);
 return action(v);
}
