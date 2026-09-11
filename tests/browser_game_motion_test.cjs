// SPDX-License-Identifier: MIT
// Normal browser UI and canvas/audio only; gameplay stays in the JR-800 program.
const assert=require('node:assert/strict');
const fs=require('node:fs/promises');
const path=require('node:path');
const {createServer}=require('node:http');
const {pathToFileURL}=require('node:url');
const {chromium}=require('playwright');
(async()=>{
 const site=path.resolve(process.argv[2]??'build/wasm-release/web'),wasm=path.resolve('build/wasm-release/web-module');
 const out=path.resolve('build/motion-browser',process.env.JR800_GAME_SITE_URL?'public':'local');await fs.mkdir(out,{recursive:true});
 const {motionFixture}=await import(pathToFileURL(path.resolve('games/tools/motion_browser_fixture.mjs')));
 const fixtures=[];for(const id of ['step-strike','number-rail','pipe-weave','line-four'])fixtures.push(await motionFixture(wasm,path.resolve('build/games',id),id));
 const server=createServer(async(req,res)=>{try{let p=path.resolve(site,'.'+new URL(req.url,'http://local').pathname);assert.ok(p===site||p.startsWith(site+path.sep));if(p===site)p=path.join(p,'index.html');res.setHeader('Content-Type',{'.mjs':'text/javascript','.wasm':'application/wasm','.html':'text/html','.json':'application/json','.css':'text/css'}[path.extname(p)]??'application/octet-stream');res.end(await fs.readFile(p));}catch{res.writeHead(404);res.end();}});
 await new Promise(r=>server.listen(0,'127.0.0.1',r));const base=process.env.JR800_GAME_SITE_URL??`http://127.0.0.1:${server.address().port}/`;let browser;
 try{
  browser=await chromium.launch({channel:'chrome',headless:true});const context=await browser.newContext({locale:'en-US'}),errors=[],results=[];
  await context.addInitScript(()=>{
   window.motionAudio=[];const Original=window.AudioContext;
   window.AudioContext=class extends Original{createBufferSource(){const node=super.createBufferSource(),start=node.start.bind(node);node.start=(...args)=>{const a=node.buffer?.getChannelData(0);if(a)window.motionAudio.push({t:performance.now(),range:Math.max(...a)-Math.min(...a)});return start(...args);};return node;}};
  });
  const page=await context.newPage();page.on('pageerror',e=>errors.push(e.message));page.on('dialog',d=>d.accept());
  await page.goto(base+'?program=step-strike');await page.waitForFunction(()=>document.querySelector('#program-launch-status').textContent.includes('Select your local BASIC ROM'));
  const synthetic=Buffer.alloc(32768,1);synthetic.set([0x20,0xfe]);synthetic[32766]=0x80;synthetic[32767]=0;
  const rom=process.env.JR800_GAME_ROM?await fs.readFile(process.env.JR800_GAME_ROM):synthetic;
  await page.locator('#browser-calendar-startup').uncheck();await page.locator('#jr8rom-file').setInputFiles({name:'owner.rom',mimeType:'application/octet-stream',buffer:rom});await page.locator('#boot-basic').click();
  await page.waitForFunction(()=>document.querySelector('#saved-rom-status').textContent.includes('Remembered ROM:'));
  const tap=async key=>{const k={up:'w',down:'s',left:'a',right:'d',space:'Space',return:'Enter'}[key];await page.keyboard.down(k);await page.waitForTimeout(65);await page.keyboard.up(k);};
  const waitPixels=pixels=>page.waitForFunction(p=>window.motionFrames?.at(-1)?.bytes.every((v,i)=>v===p[i]),pixels,{timeout:15000});
  for(const fixture of fixtures){
   const {id,stage,initial,steps}=fixture;
   await page.goto(base+'?program='+id);await page.waitForFunction(()=>document.querySelector('#status').dataset.tone==='running'&&Number(document.querySelector('#cycles').textContent)>800000);await page.locator('#lcd-panel').click();
   await page.evaluate(()=>{
    window.motionFrames=[];window.motionAudio=[];
    const frame=()=>{const c=document.querySelector('#lcd-panel'),d=c.getContext('2d').getImageData(0,0,c.width,c.height).data,bytes=Array(1536).fill(0);for(let y=0;y<64;y++)for(let x=0;x<192;x++)if(d[(Math.floor((y+.5)*c.height/64)*c.width+Math.floor((x+.5)*c.width/192))*4]<120)bytes[(y>>3)*192+x]|=1<<(y&7);window.motionFrames.push({t:performance.now(),bytes});requestAnimationFrame(frame);};frame();
   });
   await tap('space');await page.waitForTimeout(250);for(let i=0;i<stage;i++){await tap('right');await page.waitForTimeout(100);}await tap('space');await waitPixels(initial);
   await fs.writeFile(path.join(out,id+'-ready.png'),await page.locator('#lcd-panel').screenshot());
   let intermediates=0;
   for(const step of steps){
    const before=await page.evaluate(()=>window.motionFrames.length);await tap(step.key);await waitPixels(step.end);
    const observed=await page.evaluate(n=>window.motionFrames.slice(n),before);
    intermediates+=step.intermediate.filter(p=>observed.some(f=>f.bytes.every((v,i)=>v===p[i]))).length;
    // Leave enough released time for the program's input gate before the next press.
    await page.waitForTimeout(90);
   }
   assert.ok(intermediates>=2,id+' displays intermediate motion frames');
   const clearStart=await page.evaluate(()=>performance.now());await fs.writeFile(path.join(out,id+'-clear-scene.png'),await page.locator('#lcd-panel').screenshot());
   await page.waitForTimeout(1900);
   const {audio,frames}=await page.evaluate(()=>({audio:window.motionAudio,frames:window.motionFrames})),target=steps.at(-1).end;
   const held=frames.filter(f=>f.t>=clearStart&&f.bytes.every((v,i)=>v===target[i]));
   assert.ok(held.length>10&&held.at(-1).t-held[0].t>850,id+' retains the completed playfield');
   const audible=audio.filter(a=>a.t>=clearStart&&a.range>0.1);
   assert.ok(audible.length>5&&audible.at(-1).t-audible[0].t>650,id+' plays an extended audible clear jingle');
   const result={id,passed:true,intermediateFrames:intermediates,heldSceneMs:Math.round(held.at(-1).t-held[0].t),clearAudioMs:Math.round(audible.at(-1).t-audible[0].t)};
   results.push(result);console.log(JSON.stringify(result));
  }
  assert.deepEqual(errors,[]);const result={passed:true,siteUrl:base,ownerRom:Boolean(process.env.JR800_GAME_ROM),programs:results};
  await fs.writeFile(path.join(out,'verification.json'),JSON.stringify(result,null,2)+'\n');
 }finally{await browser?.close();await new Promise(r=>server.close(r));}
})().catch(e=>{console.error(e);process.exitCode=1;});
