// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile, writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {Game} from '../tools/harness.mjs';
import {aim} from '../tools/puzzle_check.mjs';

const [wasm, out] = process.argv.slice(2);
const stages = JSON.parse(await readFile(new URL('./challenges.json', import.meta.url))).stages;
const source = await readFile(new URL('./assets.s', import.meta.url), 'utf8');
const tiles = [...source.match(/^tiles:\n((?:\s*\.byte[^\n]*\n)+)/m)[1]
  .matchAll(/\$([0-9A-F]{2})/g)].map(m => parseInt(m[1], 16));
const ports = [1, 2, 4, 8, 5, 6, 9, 10], rows = [0, 0, 1, 2, 3, 4, 5, 6, 7, 7];
let playFrames = 0, flowFrames = 0, verticalLinks = 0, endpointPorts = 0, frames = 0;
const endpointDirections = new Set();

function verify(g) {
  if (g.read('phase') !== 2 && !g.read('clear_active')) return;
  const board = g.read('board', 36), wet = g.read('pipe_wet', 36), cursor = g.read('cursor');
  const dots = g.machine.lcdPanel().dots;
  const pixel = (x, y) => dots[y * 192 + x] === 2;
  const cellPixel = (cell, x, y) => pixel(16 + cell % 6 * 16 + x, 2 + Math.floor(cell / 6) * 10 + y) !== (cell === cursor && g.read('pipe_cursor_mask') !== 0);
  // Validate every pixel against the unscaled original artwork. The two LCD
  // pages share neighboring cells, so cursor/flow redraws must preserve both.
  for (let cell = 0; cell < 36; cell++) {
    const face = cell === 0 ? 32 + ports.indexOf(board[cell]) : cell === 35 ? 40 + ports.indexOf(board[cell]) : board[cell] + (wet[cell] ? 16 : 0);
    for (let y = 0; y < 10; y++) for (let x = 0; x < 16; x++) {
      assert.equal(cellPixel(cell, x, y), Boolean(tiles[8 + face * 16 + x] >> rows[y] & 1),
        `Cell ${cell}, port mask ${board[cell]}, pixel ${x},${y}`);
    }
    if (cell < 30 && (board[cell] & 2) && (board[cell + 6] & 1)) {
      for (const [p, ys] of [[cell, [8, 9]], [cell + 6, [0, 1]]]) {
        for (const x of [7, 8]) for (const y of ys) assert.ok(cellPixel(p, x, y), 'Vertical connections stay two pixels wide across cell/page boundaries');
      }
      verticalLinks++;
    }
  }
  for (const cell of [0, 35]) {
    for (const [bit, points] of [[1, [[7, 0], [8, 0], [7, 1], [8, 1]]],
      [2, [[7, 8], [8, 8], [7, 9], [8, 9]]], [4, [[0, 4], [0, 5]]], [8, [[15, 4], [15, 5]]]]) {
      for (const [x, y] of points) assert.equal(cellPixel(cell, x, y), Boolean(board[cell] & bit), 'S/E letters expose exactly their current ports');
      if (board[cell] & bit) { endpointDirections.add(bit); endpointPorts++; }
    }
  }
  for (const y of [0, 1, 62, 63]) for (let x = 0; x < 128; x++) assert.equal(pixel(x, y), false, 'The taller board retains a two-pixel outer margin');
  for (const x of [15, 112, 127]) for (let y = 0; y < 64; y++) assert.equal(pixel(x, y), false, 'Board drawing stays clear of the HUD');
  playFrames++; if (g.read('pipe_running')) flowFrames++;
}

function settle(g) {
  for (let i = 0; g.read('pipe_running') && i < 300; i++) g.frame();
  assert.equal(g.read('pipe_running'), 0);
}

for (const index of [0, 10, 30, 39]) {
  const g = await Game.open(wasm, out, 'pipe-weave'), stage = stages[index];
  g.out = resolve(out, 'visual-check');
  try {
    const frame = g.frame.bind(g);
    g.frame = () => { const panel = frame(); verify(g); return panel; };
    await g.start(index); settle(g); await g.save(`stage-${index + 1}-initial`);
    g.frame(); assert.equal(g.word('dirty_bytes'), 0, 'An idle board performs no LCD writes');
    const panel = [...g.machine.lcdPanel().dots], board = g.read('board', 36);
    g.tap('return'); g.tap('return');
    assert.deepEqual([...g.machine.lcdPanel().dots], panel, 'Menu return restores the tall grid and cursor');
    aim(g, stage.bonus[0], 6); g.tap('space');
    assert.equal(g.read('pipe_wet'), 1, 'Rotation restarts water from S, even when its first visit ends immediately');
    g.menu(1); settle(g);
    assert.deepEqual(g.read('board', 36), board, 'Undo during flowing water restores pipe orientation');
    g.menu(3); settle(g);
    assert.deepEqual([...g.machine.lcdPanel().dots], panel, 'Retry restores the same layout and flow');
    for (const cell of stage.bonus) { aim(g, cell, 6); g.tap('space'); settle(g); }
    assert.equal(g.read('phase'), 4); assert.equal(g.read('challenge_bonus'), 1);
    await g.save(`stage-${index + 1}-complete`, false);
    g.finishClear(); await g.clearSceneTask; frames += g.frames;
  } finally { g.destroy(); }
}
assert.deepEqual([...endpointDirections].sort((a, b) => a - b), [1, 2, 4, 8]);
assert.ok(verticalLinks > 0 && flowFrames > 0);
const result = {passed: true, visualStages: 4, playFrames, flowFrames, verticalLinks, endpointPorts, frames};
await writeFile(resolve(out, 'visual-verification.json'), JSON.stringify(result, null, 2) + '\n');
console.log(JSON.stringify(result));
