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
const check = ['........', '......#.', '.....##.', '.#..##..',
  '.####...', '..##....', '........', '........'];
const corners = new Set(), completedCorners = new Set(), targetNumbers = new Set();
let frames = 0, playFrames = 0, movingFrames = 0, emptyTargets = 0;

function verify(g, stage) {
  if (g.read('phase') !== 2 && !g.read('clear_active')) return;
  const board = g.read('board', 16), target = stage.bonus_cells[0];
  const done = !g.read('rail_active') && board[target] >= stage.initial.goal;
  const dots = g.machine.lcdPanel().dots;
  const pixel = (x, y) => dots[y * 192 + x] === 2;
  assert.equal(g.read('challenge_bonus'), Number(done), 'The check appears only after the move settles at the goal');
  // Every digit and rail survives the inverse highlight, including all four
  // digits of 2048. No small overlay can damage a number or move with a tile.
  for (let cell = 0; cell < 16; cell++) {
    const left = 64 + cell % 4 * 32, top = 16 + (cell >> 2) * 8;
    for (let x = 0; x < 32; x++) for (let y = 0; y < 8; y++) {
      const ink = Boolean(tiles[8 + board[cell] * 32 + x] >> y & 1);
      assert.equal(pixel(left + x, top + y), ink !== (cell === target),
        `Cell ${cell}, number ${board[cell] ? 2 ** board[cell] : 0}, pixel ${x},${y}`);
    }
  }
  // The legend uses the same complete, readable goal face as its destination.
  for (let x = 0; x < 32; x++) for (let y = 0; y < 8; y++) {
    const ink = Boolean(tiles[8 + stage.initial.goal * 32 + x] >> y & 1);
    assert.equal(pixel(97 + x, 56 + y), !ink, 'Legend shows the actual goal value');
  }
  const hintX = 76 + target % 4 * 32, hintY = target < 4 ? 8 : 48;
  if (done) {
    for (const [left, top] of [[hintX, hintY], [136, 56]]) {
      for (let y = 0; y < 8; y++) for (let x = 0; x < 8; x++) {
        assert.equal(pixel(left + x, top + y), check[y][x] === '#', 'Both completion checks remain visible');
      }
    }
    completedCorners.add(target);
  } else {
    assert.ok(pixel(hintX + 3, hintY + (target < 4 ? 4 : 3)), 'Pointer aims toward the selected corner');
    assert.equal(pixel(139, 59), false, 'Pending checkbox is unfilled');
  }
  // A full blank row separates either outside pointer from the tile numbers.
  for (let x = 64; x < 192; x++) assert.equal(pixel(x, target < 4 ? 15 : 48), false);
  corners.add(target); targetNumbers.add(board[target]);
  if (!board[target]) emptyTargets++;
  if (g.read('rail_active')) movingFrames++;
  playFrames++;
}

function settle(g) {
  for (let i = 0; g.read('rail_active') && i < 300; i++) g.frame();
  assert.equal(g.read('rail_active'), 0);
}

for (const index of [0, 1, 2, 3, 36, 37, 38, 39]) {
  const g = await Game.open(wasm, out, 'number-rail'), stage = stages[index];
  g.out = resolve(out, 'visual-check');
  try {
    const frame = g.frame.bind(g);
    g.frame = () => { const panel = frame(); verify(g, stage); return panel; };
    await g.start(index);
    await g.save(`stage-${index + 1}-initial`);
    g.frame(); assert.equal(g.word('dirty_bytes'), 0, 'Idle board and legend write no LCD data');
    const before = g.read('board', 16), panel = [...g.machine.lcdPanel().dots];
    g.tap('return'); g.tap('return');
    assert.deepEqual([...g.machine.lcdPanel().dots], panel, 'Menu return restores every goal cue');
    g.tap(stage.bonus[0]); assert.equal(g.read('rail_active'), 1);
    g.menu(1);
    assert.deepEqual(g.read('board', 16), before, 'Undo during a slide restores the initial tiles');
    assert.equal(g.read('rail_active'), 0);
    g.menu(3);
    assert.deepEqual([...g.machine.lcdPanel().dots], panel, 'Retry restores the target and clears the check');
    // Normal clears must leave the bonus unchecked; the certified bonus route
    // then checks both cues without changing the original scoring condition.
    for (const key of stage.normal) { g.tap(key); settle(g); }
    assert.equal(g.read('phase'), 4); assert.equal(g.read('challenge_bonus'), 0);
    assert.equal(g.read('challenge_rating'), 2);
    g.tap('return'); g.tap('space');
    for (const key of stage.bonus) { g.tap(key); settle(g); }
    assert.equal(g.read('phase'), 4); assert.equal(g.read('challenge_bonus'), 1);
    assert.equal(g.read('challenge_rating'), 3);
    await g.save(`stage-${index + 1}-complete`, false);
    g.finishClear(); await g.clearSceneTask;
    frames += g.frames;
  } finally { g.destroy(); }
}
assert.deepEqual([...corners].sort((a, b) => a - b), [0, 3, 12, 15]);
assert.equal(completedCorners.size, 4);
assert.ok(targetNumbers.has(11), '2048 is tested in its marked destination');
assert.ok(emptyTargets > 0 && movingFrames > 0);
const result = {passed: true, visualStages: 8, corners: 4, playFrames, movingFrames,
  emptyTargets, targetValues: [...targetNumbers].sort((a, b) => a - b).map(n => n ? 2 ** n : 0), frames};
await writeFile(resolve(out, 'visual-verification.json'), JSON.stringify(result, null, 2) + '\n');
console.log(JSON.stringify(result));
