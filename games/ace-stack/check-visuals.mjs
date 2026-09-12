// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile, writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {Game} from '../tools/harness.mjs';
const [wasm, out] = process.argv.slice(2);
const art = await readFile(new URL('./assets.s', import.meta.url), 'utf8');
const tiles = [...art.match(/^tiles:\n((?:\s*\.byte[^\n]*\n)+)/m)[1].matchAll(/\$([0-9A-F]{2})/g)].map(m => parseInt(m[1], 16));
for (let rank = 1; rank <= 13; rank++) {
  const face = tiles.slice(8 + rank * 16, 8 + (rank + 1) * 16);
  assert.equal(face[1] & 1, 0); assert.equal(face[14] & 1, 0);
  assert.equal(face[1] & 128, 0); assert.equal(face[14] & 128, 0);
  for (let x = 2; x < 14; x++) assert.ok(face[x] & 128, 'A continuous bottom edge identifies a covered card');
}
let states = 0, frames = 0;
function verify(g) {
  const dots = g.machine.lcdPanel().dots;
  for (let x = 4; x < 60; x++) assert.equal(dots[31 * 192 + x], 2, 'Rule is one pixel higher');
  for (let x = 4; x < 25; x++) assert.equal(dots[32 * 192 + x], 1, 'One clear row separates the rule from WASTE');
  g.frame(); assert.equal(g.word('dirty_bytes'), 0, 'Idle screen sends no LCD data'); states++;
}
for (const index of [0, 9, 19]) {
  const g = await Game.open(wasm, out, 'ace-stack');
  try {
    await g.start(index); verify(g);
    for (let draw = 0; draw < 3; draw++) {
      g.menu(1); verify(g);
      const panel = [...g.machine.lcdPanel().dots]; g.tap('return'); g.tap('return');
      assert.deepEqual([...g.machine.lcdPanel().dots], panel, 'Menu return preserves the shifted rule and complete waste rank');
      g.menu(2); verify(g);
      g.menu(1);
    }
    frames += g.frames;
  } finally { g.destroy(); }
}
const result = {passed: true, visualStages: 3, ranks: 13, states, frames};
await writeFile(resolve(out, 'visual-verification.json'), JSON.stringify(result, null, 2) + '\n'); console.log(JSON.stringify(result));
