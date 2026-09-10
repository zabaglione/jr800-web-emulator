// SPDX-License-Identifier: MIT
// Exercises the deployed product through its ordinary UI. ROM bytes stay local.
const assert=require('node:assert/strict');
const fs=require('node:fs/promises');
const path=require('node:path');
const {createServer}=require('node:http');
const {chromium}=require('playwright');
(async()=>{
 const site=path.resolve(process.argv[2]),requests=[],errors=[];
 const server=createServer(async(req,res)=>{const pathname=new URL(req.url,'http://localhost').pathname;const f=path.resolve(site,'.'+pathname.replace(/^\/jr800\//,'/'));try{assert.ok(f.startsWith(site+path.sep)||f===site);const target=f===site?path.join(f,'index.html'):f;const b=await fs.readFile(target);res.setHeader('Content-Type',{'.html':'text/html','.mjs':'text/javascript','.wasm':'application/wasm','.json':'application/json','.css':'text/css'}[path.extname(target)]??'application/octet-stream');res.end(b);}catch{res.writeHead(404);res.end();}});
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const url=process.env.JR800_GAME_SITE_URL??`http://127.0.0.1:${server.address().port}/jr800/`;
 const catalog=JSON.parse(await fs.readFile('games/catalog.json','utf8'));
 const output=path.resolve('build/game-browser',process.env.JR800_GAME_SITE_URL?'public':'local');await fs.mkdir(output,{recursive:true});
 let browser;
 try{
  browser=await chromium.launch({channel:'chrome',headless:true});
  const context=await browser.newContext({locale:'en-US'});
  await context.addInitScript(()=>{
   window.audioTest={contexts:[],started:0};const Original=window.AudioContext;
   if(Original)window.AudioContext=class extends Original{constructor(options){super(options);window.audioTest.contexts.push(this);}createBufferSource(){const node=super.createBufferSource(),start=node.start.bind(node);node.start=(...args)=>{window.audioTest.started++;return start(...args);};return node;}};
  });
  const page=await context.newPage();page.on('pageerror',e=>errors.push(e.message));page.on('dialog',d=>d.accept());page.on('request',r=>requests.push({url:r.url(),method:r.method()}));
  await page.goto(url+'?program=box-shift');
  await page.waitForFunction(()=>document.querySelector('#program-launch-status').textContent.includes('Select your local BASIC ROM'));
  const synthetic=Buffer.alloc(32768,1);synthetic.set([0x20,0xfe]);synthetic[32766]=0x80;synthetic[32767]=0;
  const rom=process.env.JR800_GAME_ROM?await fs.readFile(process.env.JR800_GAME_ROM):synthetic;
  await page.locator('#browser-calendar-startup').uncheck();
  await page.locator('#jr8rom-file').setInputFiles({name:'owner.rom',mimeType:'application/octet-stream',buffer:rom});
  await page.locator('#boot-basic').click();
  await page.waitForFunction(()=>document.querySelector('#status').dataset.tone==='running');
  await page.waitForFunction(()=>Number(document.querySelector('#cycles').textContent)>600000&&document.querySelector('#saved-rom-status').textContent.includes('Remembered ROM:'));
  const screen=()=>page.locator('#lcd-panel').screenshot();
  for(const program of catalog.programs){
   await page.goto(url+'?program='+program.id);
   await page.waitForFunction(()=>document.querySelector('#status').dataset.tone==='running');
   await page.waitForFunction(()=>Number(document.querySelector('#cycles').textContent)>600000).catch(async e=>{throw new Error(JSON.stringify({program:program.id,url:page.url(),status:await page.locator('#status').textContent(),cycles:await page.locator('#cycles').textContent(),launch:await page.locator('#program-launch-status').textContent(),saved:await page.locator('#saved-rom-status').textContent(),errors})+' '+e.message);});
   assert.ok((await page.locator('#program-launch-status').textContent()).includes(program.title));
   const source=await fs.readFile(`games/${program.id}/assets.s`,'utf8');
   const art=source.split('title_art:')[1].split(/\n[A-Za-z_]\w*:/)[0].match(/\$[0-9A-F]{2}/g).map(x=>parseInt(x.slice(1),16));assert.equal(art.length,1536);
   const dots=await page.locator('#lcd-panel').evaluate(canvas=>{const {width:w,height:h}=canvas,data=canvas.getContext('2d').getImageData(0,0,w,h).data;return Array.from({length:12288},(_,i)=>data[(Math.floor((Math.floor(i/192)+.5)*h/64)*w+Math.floor((i%192+.5)*w/192))*4]<120?1:0);});
   assert.deepEqual(dots,Array.from({length:12288},(_,i)=>(art[(Math.floor(i/192)>>3)*192+i%192]>>(Math.floor(i/192)&7))&1),`${program.id} automatic title pixels`);
   await fs.writeFile(path.join(output,program.id+'-title.png'),await screen());
   await page.locator('#lcd-panel').click();await page.keyboard.down('Space');await page.waitForTimeout(80);await page.keyboard.up('Space');
   await page.waitForFunction(()=>{const c=document.querySelector('#lcd-panel'),p=c.getContext('2d').getImageData(0,0,c.width,c.height).data;for(let y=0;y<8;y++)for(let x=0;x<128;x++)if(p[(Math.floor((y+.5)*c.height/64)*c.width+Math.floor((x+.5)*c.width/192))*4]<120)return false;return true;});
   await page.waitForTimeout(180);
   const button=page.locator('[data-jr800-key="space"]');await button.scrollIntoViewIfNeeded();await button.click({delay:140});
   await page.waitForFunction(()=>{const c=document.querySelector('#lcd-panel'),p=c.getContext('2d').getImageData(0,0,c.width,c.height).data;let n=0;for(let y=0;y<8;y++)for(let x=0;x<128;x++)if(p[(Math.floor((y+.5)*c.height/64)*c.width+Math.floor((x+.5)*c.width/192))*4]<120)n++;return n>20;});
   await page.waitForFunction(()=>window.audioTest.contexts.some(c=>c.state==='running')&&window.audioTest.started>0);
   await fs.writeFile(path.join(output,program.id+'-play.png'),await screen());
  }
  await page.reload();await page.waitForFunction(()=>document.querySelector('#status').dataset.tone==='running');
  await page.goto(url);await page.waitForFunction(()=>document.querySelector('#status').dataset.tone==='ready');assert.equal(await page.locator('#cycles').textContent(),'0','Ordinary visit does not auto-boot');
  await page.goto(url+'?program=missing');await page.waitForFunction(()=>document.querySelector('#status').dataset.tone==='error');assert.match(await page.locator('#program-launch-status').textContent(),/Unknown program ID/);
  await page.route('**/box-shift.j8a',r=>r.fulfill({body:Buffer.of(1,2,3)}));
  await page.goto(url+'?program=box-shift');await page.waitForFunction(()=>document.querySelector('#status').dataset.tone==='error');assert.match(await page.locator('#program-launch-status').textContent(),/integrity/);
  assert.deepEqual(errors,[]);assert.ok(requests.every(r=>r.method==='GET'));assert.ok(!requests.some(r=>/\.(rom|j8r)(\?|$)/.test(r.url)));
  const result={passed:true,siteUrl:url,programs:catalog.programs.map(p=>p.id),ownerRom:Boolean(process.env.JR800_GAME_ROM),firstRomSetup:true,savedRomLaunch:true,titlePixels:true,physicalKeys:true,virtualKeys:true,audioScheduled:true,reload:true,ordinaryVisit:true,unknownId:true,integrityFailure:true,romStaysLocal:true};
  await fs.writeFile(path.join(output,'verification.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));
 }finally{await browser?.close();await new Promise(r=>server.close(r));}
})().catch(e=>{console.error(e);process.exitCode=1;});
