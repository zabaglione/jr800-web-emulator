// SPDX-License-Identifier: MIT
// Normal browser UI only: visible sliding, staged pickups and audible celebration.
const assert=require('node:assert/strict');
const fs=require('node:fs/promises');
const path=require('node:path');
const {createServer}=require('node:http');
const {chromium}=require('playwright');
(async()=>{
 const site=path.resolve(process.argv[2]??'build/wasm-release/web');
 const out=path.resolve('build/ice-browser',process.env.JR800_GAME_SITE_URL?'public':'local');await fs.mkdir(out,{recursive:true});
 const server=createServer(async(req,res)=>{try{const p=path.resolve(site,'.'+new URL(req.url,'http://local').pathname);assert.ok(p===site||p.startsWith(site+path.sep));const f=p===site?path.join(p,'index.html'):p;res.setHeader('Content-Type',{'.mjs':'text/javascript','.wasm':'application/wasm','.html':'text/html','.json':'application/json','.css':'text/css'}[path.extname(f)]??'application/octet-stream');res.end(await fs.readFile(f));}catch{res.writeHead(404);res.end();}});
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const base=process.env.JR800_GAME_SITE_URL??`http://127.0.0.1:${server.address().port}/`;
 let browser;
 try{
  browser=await chromium.launch({channel:'chrome',headless:true});const context=await browser.newContext({locale:'en-US'}),errors=[];
  await context.addInitScript(()=>{
   window.iceAudio=[];const Original=window.AudioContext;
   window.AudioContext=class extends Original{createBufferSource(){const node=super.createBufferSource(),start=node.start.bind(node);node.start=(...args)=>{const samples=node.buffer?.getChannelData(0);if(samples)window.iceAudio.push({t:performance.now(),samples:Array.from(samples),rate:node.buffer.sampleRate});return start(...args);};return node;}};
  });
  const page=await context.newPage();page.on('pageerror',e=>errors.push(e.message));page.on('dialog',d=>d.accept());
  await page.goto(base+'?program=ice-route');await page.waitForFunction(()=>document.querySelector('#program-launch-status').textContent.includes('Select your local BASIC ROM'));
  const synthetic=Buffer.alloc(32768,1);synthetic.set([0x20,0xfe]);synthetic[32766]=0x80;synthetic[32767]=0;
  const rom=process.env.JR800_GAME_ROM?await fs.readFile(process.env.JR800_GAME_ROM):synthetic;
  await page.locator('#browser-calendar-startup').uncheck();await page.locator('#jr8rom-file').setInputFiles({name:'owner.rom',mimeType:'application/octet-stream',buffer:rom});await page.locator('#boot-basic').click();
  await page.waitForFunction(()=>document.querySelector('#saved-rom-status').textContent.includes('Remembered ROM:'));
  await page.goto(base+'?program=ice-route');await page.waitForFunction(()=>document.querySelector('#status').dataset.tone==='running'&&Number(document.querySelector('#cycles').textContent)>800000);
  await page.locator('#lcd-panel').click();
  const tap=async key=>{await page.keyboard.down(key);await page.waitForTimeout(65);await page.keyboard.up(key);};
  await tap('Space');await page.waitForTimeout(300);
  await page.evaluate(()=>{
   window.iceFrames=[];window.iceRecording=true;window.iceAudio=[];
   const frame=()=>{if(!window.iceRecording)return;const c=document.querySelector('#lcd-panel'),d=c.getContext('2d').getImageData(0,0,c.width,c.height).data,bytes=Array(1536).fill(0);for(let y=0;y<64;y++)for(let x=0;x<192;x++)if(d[(Math.floor((y+.5)*c.height/64)*c.width+Math.floor((x+.5)*c.width/192))*4]<120)bytes[(y>>3)*192+x]|=1<<(y&7);window.iceFrames.push({t:performance.now(),bytes});requestAnimationFrame(frame);};frame();
  });
  const stage=JSON.parse(await fs.readFile('games/ice-route/challenges.json','utf8')).stages[0];
  const source=await fs.readFile('games/ice-route/assets.s','utf8');
  const tiles=source.split('tiles:')[1].split('view_cells:')[0].match(/\$[0-9A-F]{2}/g).map(x=>parseInt(x.slice(1),16));
  const skater=tiles.slice(48,56),gem=tiles.slice(32,40),star=[0,0x14,8,0x3e,8,0x14,0,0];
  const waitTile=async(cell,pattern)=>page.waitForFunction(({cell,pattern})=>{const f=window.iceFrames.at(-1);if(!f)return false;const at=(1+Math.floor(cell/14))*192+40+(cell%14)*8;return pattern.every((v,i)=>v===f.bytes[at+i]);},{cell,pattern});
  await tap('Space');await waitTile(stage.initial.start,skater);await page.waitForTimeout(1000);
  await fs.writeFile(path.join(out,'ready.png'),await page.locator('#lcd-panel').screenshot());
  let p=stage.initial.start,board=[...stage.initial.board],left=2;const traversed=[];
  for(const key of stage.normal){
   const d={left:-1,right:1,up:-14,down:14}[key],cells=[];
   do{p+=d;cells.push(p);if([3,4].includes(board[p])){board[p]=0;left--;}if(board[p]===2&&!left)break;}while(board[p+d]!==1);
   const time=await page.evaluate(()=>performance.now());await tap({left:'a',right:'d',up:'w',down:'s'}[key]);
   if(board[p]!==2||left){await waitTile(p,skater);await page.waitForTimeout(200);}else await page.waitForTimeout(2400);
   traversed.push({time,cells});
  }
  await page.evaluate(()=>window.iceRecording=false);
  const {frames,audio}=await page.evaluate(()=>({frames:window.iceFrames,audio:window.iceAudio}));
  const tile=(f,cell)=>f.bytes.slice((1+Math.floor(cell/14))*192+40+(cell%14)*8,(1+Math.floor(cell/14))*192+48+(cell%14)*8);
  const same=(a,b)=>a.every((v,i)=>v===b[i]);
  const first=(cell,shape)=>frames.findIndex(f=>same(tile(f,cell),shape));
  const gem1=first(stage.initial.board.indexOf(3),gem),gem2=first(stage.initial.board.indexOf(4),gem),mark1=first(stage.bonus_cells[0],star),mark2=first(stage.bonus_cells[1],star),player=first(stage.initial.start,skater);
  assert.ok(gem1>=0&&gem1<gem2&&gem2<mark1&&mark1<mark2&&mark2<player,'Gem, gem, star, star, player appear in order');
  const blink=frames.slice(player).filter(f=>f.t<traversed[0].time).map(f=>same(tile(f,stage.initial.start),skater));assert.ok(blink.some(x=>!x)&&blink.at(-1),'Player blinks and then stays visible');
  for(const step of traversed)for(const cell of step.cells)assert.ok(frames.some(f=>f.t>=step.time&&(same(tile(f,cell),skater)||(cell===p&&(same(tile(f,cell),tiles.slice(56,64))||same(tile(f,cell),tiles.slice(64,72)))))),`Visible intermediate cell ${cell}`);
  const last=traversed.at(-1).time,clearAudio=audio.filter(a=>a.t>last+400);
  const active=clearAudio.filter(a=>Math.max(...a.samples)-Math.min(...a.samples)>0.1);
  assert.ok(active.length>5&&active.at(-1).t-active[0].t>700,'An audible extended clear jingle is scheduled');
  assert.deepEqual(errors,[]);
  // Capture actual browser LCD frames and PCM for optional local review/export.
  await fs.writeFile(path.join(out,'recording.json'),JSON.stringify({frames,audio}));
  await fs.writeFile(path.join(out,'clear.png'),await page.locator('#lcd-panel').screenshot());
  const result={passed:true,siteUrl:base,ownerRom:Boolean(process.env.JR800_GAME_ROM),orderedItems:true,playerBlink:true,visibleSlideCells:traversed.reduce((n,s)=>n+s.cells.length,0),clearAudioMs:Math.round(active.at(-1).t-active[0].t),frames:frames.length};
  await fs.writeFile(path.join(out,'verification.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));
 }finally{await browser?.close();await new Promise(r=>server.close(r));}
})().catch(e=>{console.error(e);process.exitCode=1;});
