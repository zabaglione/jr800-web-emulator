// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {deflateSync} from 'node:zlib';
export const directions={up:'keypad-8',down:'keypad-2',left:'keypad-4',right:'keypad-6'};
export const letters={up:'letter-w',down:'letter-s',left:'letter-a',right:'letter-d'};
function crc32(data){let c=0xffffffff;for(const b of data){c^=b;for(let k=0;k<8;k++)c=(c>>>1)^((c&1)?0xedb88320:0);}return (c^0xffffffff)>>>0;}
export function png(dots,scale=4){
 const w=192*scale,h=64*scale,raw=Buffer.alloc((w*3+1)*h),palette=[[175,184,153],[31,44,35]];
 for(let y=0;y<h;y++)for(let x=0;x<w;x++){const c=palette[dots[(y/scale|0)*192+(x/scale|0)]===2?1:0];raw.set(c,y*(w*3+1)+1+x*3);}
 const chunk=(name,data)=>{const n=Buffer.from(name),a=Buffer.alloc(4),c=Buffer.alloc(4);a.writeUInt32BE(data.length);c.writeUInt32BE(crc32(Buffer.concat([n,data])));return Buffer.concat([a,n,data,c]);};
 const hdr=Buffer.alloc(13);hdr.writeUInt32BE(w);hdr.writeUInt32BE(h,4);hdr[8]=8;hdr[9]=2;
 return Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]),chunk('IHDR',hdr),chunk('IDAT',deflateSync(raw)),chunk('IEND',Buffer.alloc(0))]);
}
// Result dialogs occupy this rectangle; physics checks still inspect every exposed pixel.
export function playPixelVisible(g,x,y){return !([4,5].includes(g.read('phase'))&&x+g.read('view_origin')>=36&&x+g.read('view_origin')<156&&y>=16&&y<48);}
export class Game {
 static async open(wasm,out,id){
  const url=n=>pathToFileURL(resolve(wasm,n)).href;
  const {WasmMachine}=await import(url('wasm-machine.mjs'));
  const {jr800BasicBootExperimentConfiguration}=await import(url('basic-boot-profile.mjs'));
  const g=new Game();g.out=out;g.id=id;g.frames=0;g.cycles=[];g.transfers=[];g.trace=[];g.keys=0;
  g.symbols=Object.fromEntries([... (await readFile(resolve(out,`${id}.sym`),'utf8')).matchAll(/^ \$([0-9A-F]+) G (\S+)/gm)].map(([,a,n])=>[n,parseInt(a,16)]));
  const config={...jr800BasicBootExperimentConfiguration(),ignoreUnsupportedIo:Boolean(process.env.JR800_GAME_ROM)};if(!process.env.JR800_GAME_ROM)delete config.expansionRamInitialValue;
  g.machine=await WasmMachine.createJr800(url('jr800_wasm.mjs'),config);
  if(process.env.JR800_GAME_ROM){const p=process.env.JR800_GAME_ROM,d=await readFile(p);if(p.endsWith('.j8r'))g.machine.loadJr8rom(d);else g.machine.loadLogicalRom(d);const stop=g.machine.run(1000000);assert.ok(['instruction-limit','sleeping'].includes(stop.reason),JSON.stringify(stop));}
  else {const rom=new Uint8Array(32768).fill(1);rom.set([0x20,0xfe]);rom[32766]=0x80;rom[32767]=0;g.machine.loadLogicalRom(rom);}
  g.initialIgnored=g.machine.state().ignoredIoAccessCount;
  g.app=await readFile(resolve(out,`${id}.j8a`));g.machine.loadProgram(g.app);g.frame();return g;
 }
 read(n,len=1){assert.ok(this.symbols[n],`Missing symbol ${n}`);const b=this.machine.memory(this.symbols[n],len);return len===1?b[0]:[...b];}
 word(n){const b=this.read(n,2);return b[0]*256+b[1];}
 frame(){assert.equal(this.machine.state().ignoredIoAccessCount,this.initialIgnored,'Game introduces no ignored I/O');if(this.frames)this.machine.step();const before=Number(this.machine.state().cycleCount);const stop=this.machine.runTo(this.symbols.frame_ready,1000000);assert.equal(stop.reason,'address-reached',JSON.stringify(stop));assert.equal(this.machine.state().sp,0x5fff);const panel=this.machine.lcdPanel();const fb=this.machine.memory(this.symbols.framebuffer,1536);for(let y=0;y<64;y++)for(let x=0;x<192;x++)assert.equal(panel.dots[y*192+x],1+((fb[(y>>3)*192+x]>>(y&7))&1),`LCD mismatch ${x},${y}`);this.cycles.push(Number(this.machine.state().cycleCount)-before);this.transfers.push(this.word('dirty_bytes'));let hash=2166136261;for(const b of fb)hash=Math.imul(hash^b,16777619)>>>0;this.trace.push([this.keys,hash,Number(this.machine.state().cycleCount),this.word('dirty_bytes'),this.read('phase')].join(' '));this.frames++;return panel;}
 hold(key,on){key=directions[key]??key;const i=['keypad-8','keypad-2','keypad-4','keypad-6','space','return','letter-w','letter-s','letter-a','letter-d'].indexOf(key);assert.ok(i>=0);this.keys=on?this.keys|(1<<i):this.keys&~(1<<i);this.machine.setKeyboardKeyState(key,on);}
 tap(key){this.hold(key,true);this.frame();this.hold(key,false);this.frame();}
 async save(name){await mkdir(this.out,{recursive:true});await writeFile(resolve(this.out,`${name}.png`),png(this.machine.lcdPanel().dots));await writeFile(resolve(this.out,`${name}-1x.png`),png(this.machine.lcdPanel().dots,1));}
 async start(stage=0){this.tap('space');assert.equal(this.read('phase'),1);for(let i=0;i<stage;i++)this.tap('right');this.tap('space');assert.equal(this.read('phase'),2);}
 menu(choice){this.tap('return');assert.equal(this.read('phase'),3);for(let i=0;i<choice;i++)this.tap('down');this.tap('space');}
 destroy(){this.machine.destroy();}
}
