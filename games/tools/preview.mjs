// SPDX-License-Identifier: MIT
// Capture the actual JR-800 title and initial play screen for visual review.
import {readFile,mkdir} from 'node:fs/promises';
import {resolve} from 'node:path';
import {Game} from './harness.mjs';
const [wasm='build/wasm-release/web-module',output='build/visual-redesign',...ids]=process.argv.slice(2);
const programs=JSON.parse(await readFile(new URL('../catalog.json',import.meta.url),'utf8')).programs;
for(const p of programs.filter(p=>!ids.length||ids.includes(p.id))){
 const g=await Game.open(wasm,resolve('build/games',p.id),p.id);
 try {g.out=resolve(output,p.id);await mkdir(g.out,{recursive:true});await g.save('title');await g.start();await g.save('initial');process.stdout.write(p.id+'\n');}
 finally {g.destroy();}
}
