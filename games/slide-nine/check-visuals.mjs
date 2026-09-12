// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile, writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {Game} from '../tools/harness.mjs';

const [wasm, out] = process.argv.slice(2);
const stages = JSON.parse(await readFile(new URL('./challenges.json', import.meta.url))).stages;
const source = await readFile(new URL('./assets.s', import.meta.url), 'utf8');
const tiles = [...source.match(/^tiles:\n((?:\s*\.byte[^\n]*\n)+)/m)[1]
  .matchAll(/\$([0-9A-F]{2})/g)].map(m => parseInt(m[1], 16));
const indices = [...new Set([0, ...Array.from({length: 8}, (_, i) => stages.findIndex(s => s.bonus_tile === i + 1)), 39])].filter(i => i >= 0);
let frames = 0, playFrames = 0, stampedFrames = 0, emptyDestinations = 0, undoChecked = false;
const numbers = new Set();

function verify(g, stage) {
  if (g.read('phase') !== 2 && !g.read('clear_active')) return;
  const board = g.read('board', 9), target = stage.bonus_cells[0], done = Boolean(g.read('challenge_bonus'));
  const dots = g.machine.lcdPanel().dots;
  for (let cell = 0; cell < 9; cell++) for (let y = 0; y < 16; y++) for (let x = 0; x < 32; x++) {
    const ink = Boolean(tiles[8 + board[cell] * 64 + (y >> 3) * 32 + x] >> (y & 7) & 1);
    let expected = ink;
    if (board[cell] === stage.bonus_tile && x >= 8 && x < 24 && y >= 3 && y < 12) expected = x < 22 && !ink;
    if (cell === target) {
      const edge = x >= 1 && x <= 30 && y >= 1 && y <= 14 && ([1, 2, 29, 30].includes(x) || [1, 2, 13, 14].includes(y));
      if (edge) expected = done || (x + y) % 2 === 0;
    }
    const actual = dots[(8 + Math.floor(cell / 3) * 16 + y) * 192 + 80 + cell % 3 * 32 + x] === 2;
    assert.equal(actual, expected, `Cell ${cell}, number ${board[cell]}, pixel ${x},${y}: the number moves while the frame stays put`);
  }
  for (let x = 54; x < 60; x++) assert.equal(dots[55 * 192 + x], 1, 'STAMP has a blank row above the status text');
  assert.equal(dots[48 * 192 + 54], 2, 'HUD STAMP number has the matching inverse background');
  numbers.add(stage.bonus_tile); playFrames++;
  if (done) stampedFrames++;
  if (!board[target]) emptyDestinations++;
}

for (const index of indices) {
  const stage = stages[index], g = await Game.open(wasm, out, 'slide-nine');
  g.out = resolve(out, 'visual-check');
  try {
    const frame = g.frame.bind(g); g.frame = () => { const panel = frame(); verify(g, stage); return panel; };
    await g.start(index); await g.save(`stage-${index + 1}-initial`);
    g.frame(); assert.equal(g.word('dirty_bytes'), 0, 'Idle board performs no LCD writes');
    const panel = [...g.machine.lcdPanel().dots]; g.tap('return'); g.tap('return');
    assert.deepEqual([...g.machine.lcdPanel().dots], panel, 'Menu return restores both kinds of highlight');
    for (const key of stage.bonus) {
      const before = g.read('board', 9), wasStamped = g.read('challenge_bonus');
      g.tap(key);
      if (!undoChecked && !wasStamped && g.read('challenge_bonus') && g.read('phase') === 2) {
        await g.save('stamp-complete'); g.menu(1);
        assert.deepEqual(g.read('board', 9), before);
        assert.equal(g.read('challenge_bonus'), 0, 'Undo restores the patterned, pending destination');
        g.tap(key); assert.equal(g.read('challenge_bonus'), 1); undoChecked = true;
      }
    }
    assert.equal(g.read('phase'), 4); assert.equal(g.read('challenge_bonus'), 1);
    g.finishClear(); await g.clearSceneTask; frames += g.frames;
  } finally { g.destroy(); }
}
assert.ok(undoChecked && stampedFrames > 0 && emptyDestinations > 0);
assert.equal(numbers.size, new Set(stages.map(s => s.bonus_tile)).size);
const result = {passed: true, visualStages: indices.length, numbers: [...numbers].sort(), playFrames, stampedFrames, emptyDestinations, undoChecked, frames};
await writeFile(resolve(out, 'visual-verification.json'), JSON.stringify(result, null, 2) + '\n');
console.log(JSON.stringify(result));
