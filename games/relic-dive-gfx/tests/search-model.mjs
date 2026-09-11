// SPDX-License-Identifier: MIT
// Search-only model of project-owned game assembly. Never establishes victory:
// every selected action must be checked against the original machine program.
export const directions = [[0,-1,1],[0,1,2],[-1,0,3],[1,0,4],[-1,-1,7],[1,-1,8],[-1,1,9],[1,1,10]];
const attacks=[2,3,4,3,2,2,4,5,2,2,2,4], defenses=[0,0,1,0,0,0,1,1,0,0,1,0];
export function createSearchModel(terrain) {
    const visionCache=new Map();
    function vision(px,py) {
        const pos=py*24+px;
        if(visionCache.has(pos))return visionCache.get(pos);
        const cells=[];
        for(let ty=Math.max(0,py-5);ty<=Math.min(15,py+5);ty++)for(let tx=Math.max(0,px-5);tx<=Math.min(23,px+5);tx++){
            const dx=Math.abs(tx-px),dy=Math.abs(ty-py);if(dx+dy>5)continue;
            let x=px,y=py,error=dx-dy,visible=true;
            while(x!==tx||y!==ty){
                if(!(terrain[y*24+x]&127)){visible=false;break;}
                const twice=error*2;
                if(twice>-dy){error-=dy;x+=tx<px?-1:1;}
                if(twice<dx){error+=dx;y+=ty<py?-1:1;}
            }
            if(visible)cells.push(ty*24+tx);
        }
        const mask=new Uint8Array(384);for(const p of cells)mask[p]=1;
        const result={cells,mask};visionCache.set(pos,result);return result;
    }
    const walk=(m,x,y)=>x>=0&&x<24&&y>=0&&y<16&&(m[y*24+x]&127)!==0;
    const enemy=(m,x,y)=>{for(let i=384;i<640;i+=8)if(m[i+3]&&m[i]===x&&m[i+1]===y)return i;return -1;};
    const itemAt=(m,x,y)=>{for(let i=640;i<736;i+=3)if(m[i+2]&&m[i]===x&&m[i+1]===y)return i;return -1;};
    const word=(s,o)=>s[o]*256+s[o+1];
    const putWord=(s,o,v)=>{s[o]=v>>8;s[o+1]=v;};
    const random=s=>{putWord(s,25,(word(s,25)*25173+13849)&65535);return s[25];};
    function step(parent,action) {
        const s=parent.state.slice(),m=parent.map.slice();
        if(s[5]!==1)return null;
        if(action.slot!==undefined){
            const slot=30+action.slot,item=s[slot];if(!item)return null;
            const old=item>=10?s[43]:s[42];
            if(action.discard||item>=7&&old){
                if(itemAt(m,s[8],s[9])>=0)return null;
                let free=-1;for(let i=640;i<736;i+=3)if(!m[i+2]){free=i;break;}
                if(free<0)return null;
                m[free]=s[8];m[free+1]=s[9];m[free+2]=action.discard?item:old;
            }
            if(!action.discard){
                if(item===1)s[12]=Math.min(255,s[12]+[80,60,45][s[6]]);
                else if(item>=7){if(item>=10){s[43]=item;s[14]=item-9;}else{s[42]=item;s[13]=item-3;}}
                else {
                    const effect=s[37+item-2];s[36]|=1<<(item-2);
                    if(effect===0){s[59]=s[60]=0;s[10]=Math.min(s[11],s[10]+[12,8,6][s[6]]);}
                    if(effect===1)s[10]=Math.max(0,s[10]-3);
                    if(effect===2)s[19]=13;
                    if(effect===3)for(let i=0;i<384;i++)m[i]|=128;
                    if(effect===4)outer:for(let pos=0;pos<384;pos++){
                        const x=pos%24,y=Math.floor(pos/24);
                        if(m[y*24+x]!==129||itemAt(m,x,y)>=0||enemy(m,x,y)>=0)continue;
                        for(let i=384;i<640;i+=8)if(m[i+3]&&Math.abs(m[i]-x)+Math.abs(m[i+1]-y)<=1)continue outer;
                        s[8]=x;s[9]=y;break outer;
                    }
                }
            }
            s[slot]=0;
        }else if(action.key!==11){
            const d=directions.find(d=>d[2]===action.key);if(!d)return null;
            const [dx,dy]=d,x=s[8]+dx,y=s[9]+dy;
            if(!walk(m,x,y)||dx&&dy&&(!walk(m,s[8]+dx,s[9])||!walk(m,s[8],s[9]+dy)))return null;
            const e=enemy(m,x,y);
            if(e>=0){
                const damage=Math.max(1,s[13]+(s[19]?1:0)-defenses[m[e+2]-1]);
                if(m[e+3]>damage)m[e+3]-=damage;
                else {m[e+3]=0;s[16]++;if(s[15]<32&&s[16]>=3){s[16]=0;s[15]++;s[10]+=2;s[11]+=2;}}
            }else{
                s[8]=x;s[9]=y;
                if((m[y*24+x]&127)===4){s[20]=1;s[5]=9;m[y*24+x]=129;}
                const i=itemAt(m,x,y);
                if(i>=0){
                    if(m[i+2]===13){putWord(s,21,Math.min(65535,word(s,21)+10));m[i+2]=0;}
                    else for(let j=30;j<36;j++)if(!s[j]){s[j]=m[i+2];m[i+2]=0;break;}
                }
            }
        }
        putWord(s,23,(word(s,23)+1)&65535);
        if(s[5]===9)return {state:s,map:m};
        if(!s[10]){s[5]=8;return {state:s,map:m};}
        if(s[19])s[19]--;
        if(s[59]){s[59]--;s[60]++;if(!(s[60]&1))s[10]--;}
        if(!s[10]){s[5]=8;return {state:s,map:m};}
        if(s[12]){s[12]--;s[18]=0;s[17]++;if(s[17]>=12){s[17]=0;if(s[10]<s[11])s[10]++;}}
        else{s[17]=0;s[18]++;if(!(s[18]&1)&&s[10])s[10]--;}
        const v=vision(s[8],s[9]);for(const p of v.cells)m[p]|=128;
        function strike(i){
            const kind=m[i+2];s[10]=Math.max(0,s[10]-Math.max(1,attacks[kind-1]+s[6]-1-s[14]));
            if(kind===10){s[59]=6;s[60]=0;}
            if(kind===11&&s[14]){s[14]--;s[43]=s[14]?s[43]-1:0;}
            if(kind===9&&!m[i+6]){m[i+6]=1;putWord(s,21,Math.max(0,word(s,21)-10));}
            if(kind===4)s[12]=Math.max(0,s[12]-3);
        }
        function enemyStep(i,x,y){
            if(!walk(m,x,y))return false;
            if(x===s[8]&&y===s[9]){strike(i);return true;}
            if(enemy(m,x,y)>=0)return false;
            m[i]=x;m[i+1]=y;return true;
        }
        for(let i=384;i<640&&s[10];i+=8){
            if(!m[i+3])continue;
            const kind=m[i+2];
            if(kind===8&&!(s[24]&3)&&m[i+3]<12)m[i+3]++;
            if(kind===1&&(s[24]&1))continue;
            if(v.mask[m[i+1]*24+m[i]]){
                if(kind===12&&(m[i]===s[8]||m[i+1]===s[9])){strike(i);continue;}
                m[i+4]=s[8];m[i+5]=s[9];
            }
            if(m[i+4]===255)continue;
            if(kind===9&&m[i+6]){m[i+4]=m[i]>s[8]?23:0;m[i+5]=m[i+1]>s[9]?15:0;}
            if(kind===6&&!(random(s)&3)){const d=directions[random(s)&3];enemyStep(i,m[i]+d[0],m[i+1]+d[1]);continue;}
            if(m[i]!==m[i+4]&&enemyStep(i,m[i]+(m[i]<m[i+4]?1:-1),m[i+1]))continue;
            if(m[i+1]!==m[i+5])enemyStep(i,m[i],m[i+1]+(m[i+1]<m[i+5]?1:-1));
            else {m[i+4]=255;m[i+5]=255;}
        }
        if(!s[10])s[5]=8;
        return {state:s,map:m};
    }
    return {step,vision};
}

// Independent generation model; the test suite compares all bytes and RNG
// against generate_world for 1,000 seeds before it is used by offline search.
export function generateFloor(state) {
    const s=state.slice(),m=new Uint8Array(768);
    function random(){const n=((s[25]*256+s[26])*25173+13849)&65535;s[25]=n>>8;s[26]=n;return s[25];}
    const centers=[];
    const bounded=n=>{let v;do{v=random()&7;}while(v>=n);return v;};
    const rects=[[1,1,22,14]];
    let target;do{target=random()&3;}while(target===3);target+=4;
    while(rects.length<target){
        let big=0,index=0,axis=0;
        for(let i=0;i<rects.length;i++)for(let k=0;k<2;k++)if(rects[i][k+2]>big){big=rects[i][k+2];index=i;axis=k;}
        if(big<11)break;
        let cut;do{cut=(random()&31)+5;}while(cut+6>big);
        const next=rects[index].slice();rects[index][axis+2]=cut;next[axis]+=cut+1;next[axis+2]=big-cut-1;rects.push(next);
    }
    const count=rects.length;let omitted=0;
    for(let i=0;i<count-4;i++)if(random()&1){let r;do{r=bounded(count);}while(omitted&(1<<r));omitted|=1<<r;}
    for(let room=0;room<count;room++){
        const [rx,ry,rw,rh]=rects[room];let w,h,ox,oy;
        do{w=(random()&31)+4;}while(w>rw);
        do{ox=random()&31;}while(ox+w>rw);
        do{h=(random()&15)+4;}while(h>rh);
        do{oy=random()&15;}while(oy+h>rh);
        const x=rx+ox,y=ry+oy;rects[room]=[x,y,w,h];centers.push([x+(w>>1),y+(h>>1)]);
        if(!(omitted&(1<<room)))for(let yy=y;yy<y+h;yy++)for(let xx=x;xx<x+w;xx++)m[yy*24+xx]=1;
    }
    let visited=1<<bounded(count);const links=Array(count).fill(0);
    const edge=()=>{const a=bounded(count);let b;do{b=bounded(count);}while(a===b);return [a,b];};
    function connect(a,b){
        links[a]|=1<<b;links[b]|=1<<a;
        let [x,y]=centers[a];const [tx,ty]=centers[b];
        const horizontal=()=>{m[y*24+x]=1;while(x!==tx){x+=Math.sign(tx-x);m[y*24+x]=1;}};
        const vertical=()=>{m[y*24+x]=1;while(y!==ty){y+=Math.sign(ty-y);m[y*24+x]=1;}};
        if(random()&1){vertical();horizontal();}else{horizontal();vertical();}
    }
    while(visited!==(1<<count)-1){const [a,b]=edge(),mask=(1<<a)|(1<<b);if((visited&mask)===0||(visited&mask)===mask)continue;visited|=mask;connect(a,b);}
    if(random()&1){let a,b;do{[a,b]=edge();}while(links[a]&(1<<b));connect(a,b);}
    let start;do{start=bounded(count);}while(omitted&(1<<start));
    const [sx,sy]=centers[start];s[8]=sx;s[9]=sy;
    let distance=0,goal;
    for(let i=0;i<count;i++)if(i!==start&&!(omitted&(1<<i))){const [rx,ry,w,h]=rects[i],x=centers[i][0]>=sx?rx+w-1:rx,y=centers[i][1]>=sy?ry+h-1:ry,d=Math.abs(x-sx)+Math.abs(y-sy);if(d>distance){distance=d;goal=y*24+x;}}
    m[goal]=s[7]+1===s[58]?4:3;
    const occupied=new Set([sy*24+sx]);
    function free(){for(;;){const x=random()&31;if(x===0||x>=23)continue;const y=random()&15;if(y===0||y>=15)continue;const p=y*24+x;if(m[p]!==1||occupied.has(p))continue;return [x,y,p];}}
    const tier=Math.min(2,s[7]>>1),items=[...(s[6]===0?[1,1]:[1]),2,3,4,5,6,7+tier,10+tier,13,13,13];
    for(let i=0;i<items.length;i++){const [x,y,p]=free();m.set([x,y,items[i]],640+i*3);occupied.add(p);}
    s[57]=Math.min(32,s[6]*2+s[7]+4);
    const distribution=[5,1,6,2,3,10,4,9,11,8,7,12],hp=[4,5,9,6,3,4,7,12,5,5,8,10];
    for(let i=0;i<s[57];i++){
        let x,y,p;do{[x,y,p]=free();}while(Math.abs(x-sx)+Math.abs(y-sy)<6);
        occupied.add(p);let kind;do{kind=random()&15;}while(kind>Math.min(12,s[7]*2+4)-1);
        kind=distribution[kind];m.set([x,y,kind,hp[kind-1],255,255,0,0],384+i*8);
    }
    return {state:s,map:m};
}
export function descend(node) {
    const state=node.state.slice();state[7]++;
    const next=generateFloor(state);next.state[5]=1;
    return createSearchModel(next.map).step(next,{key:11});
}

export function newAdventure(template,seed) {
    const s=template.state.slice();s[25]=s[44]=seed>>8;s[26]=s[45]=seed;
    s.set([0,1,2,3,4],37);
    function random(){const n=((s[25]*256+s[26])*25173+13849)&65535;s[25]=n>>8;s[26]=n;return s[25];}
    if(random()&1)s.set([1,0],37);
    if(random()&1)[s[38],s[39]]=[s[39],s[38]];
    if(random()&1)[s[40],s[41]]=[s[41],s[40]];
    const node=generateFloor(s);for(const p of createSearchModel(node.map).vision(node.state[8],node.state[9]).cells)node.map[p]|=128;
    return node;
}
