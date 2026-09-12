// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile, writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {Game} from '../tools/harness.mjs';

const [wasm, out] = process.argv.slice(2);
const stages = JSON.parse(await readFile(new URL('./challenges.json', import.meta.url))).stages;
const visuals = await readFile(new URL('./visuals.s', import.meta.url), 'utf8');
const tinyFont = [...visuals.match(/^hud_font_tiny:\n((?:\s*\.byte[^\n]*\n)+)/m)[1]
  .matchAll(/\$([0-9A-F]{2})/g)].map(m => parseInt(m[1], 16));

// Search orientation states independently of the Python authoring code.
// Each retained emitter must make a unique contribution in at least one state.
function objectiveLight(board, source, objectives) {
  const seen = new Set(); let p = source, d = 0, light = 0;
  for (;;) {
    p += [1, 16, -1, -16][d];
    if (p < 0 || p >= 112 || board[p] === 1 || seen.has(p * 4 + d)) return light;
    seen.add(p * 4 + d);
    const i = objectives.indexOf(p);
    if (i >= 0) light |= 1 << i;
    if (board[p] === 2) d ^= 3;
    else if (board[p] === 3) d ^= 1;
    else if (board[p] >= 4) return light;
  }
}
let orientationChecks = 0, sourceChecks = 0;
for (const stage of stages) {
  const board = stage.initial.board.slice(), sources = stage.initial.sources;
  assert.ok(sources.length >= 1 && sources.length <= 3);
  assert.deepEqual(board.flatMap((tile, p) => tile === 5 ? [p] : []), sources.slice().sort((a, b) => a - b));
  const positions = board.flatMap((tile, p) => [2, 3].includes(tile) ? [p] : []);
  const objectives = board.flatMap((tile, p) => tile === 4 ? [p] : []).concat(stage.bonus_cells);
  const contributes = sources.map(() => false);
  for (let mask = 0; mask < 1 << positions.length; mask++) {
    positions.forEach((p, i) => { board[p] = 2 + (mask >> i & 1); });
    const light = sources.map(source => objectiveLight(board, source, objectives));
    light.forEach((bits, i) => {
      const others = light.reduce((all, value, j) => all | (j === i ? 0 : value), 0);
      if (bits & ~others) contributes[i] = true;
    });
    orientationChecks++;
    if (contributes.every(Boolean)) break;
  }
  assert.ok(contributes.every(Boolean), 'Every emitter can uniquely light a receiver or optional mark');
  sourceChecks += sources.length;
}

let frames = 0, states = 0, wallChecks = 0;
function verify(g, stage) {
  const board = g.read('board', 112), cursor = g.read('cursor');
  const dots = g.machine.lcdPanel().dots;
  const pixel = (x, y) => dots[y * 192 + x] === 2;
  const cell = (p, x, y) => pixel(32 + p % 16 * 8 + x, 8 + (p >> 4) * 8 + y) !== (p === cursor && Boolean(g.read('mirror_cursor_mask')));
  assert.equal(g.read('source_count'), stage.initial.sources.length);
  assert.deepEqual(g.read('source_cells', 3).slice(0, stage.initial.sources.length), stage.initial.sources);
  for (let p = 0; p < 112; p++) {
    if (board[p] === 1) {
      assert.ok(cell(p, 5, 5), 'A shielding panel has a solid dark core');
      if (p % 16 < 15 && board[p + 1] === 1) {
        for (let y = 3; y < 6; y++) assert.ok(cell(p, 7, y) && cell(p + 1, 0, y), 'Adjacent wall panels join horizontally');
      }
      if (p < 96 && board[p + 16] === 1) {
        for (let x = 3; x < 6; x++) assert.ok(cell(p, x, 7) && cell(p + 16, x, 0), 'Adjacent wall panels join vertically');
      }
      wallChecks++;
    }
    if (board[p] === 5) assert.ok(cell(p, 3, 1) && cell(p, 4, 2) && cell(p, 5, 3), 'Useful emitters retain their right-pointing arrow');
  }
  for (const side of [0, 160]) for (const band of [1, 4, 7]) {
    for (let y = band * 8; y < band * 8 + 8; y++) {
      assert.equal(pixel(side + 2, y), false, 'One full blank column separates the text from the rail');
      assert.ok(pixel(side, y) && pixel(side + 31, y), 'HUD borders remain intact');
    }
  }
  for (const [text, x, y] of [['LIT', 3, 9], ['GOALS', 163, 9], ['USED', 3, 33],
    ['PAR', 163, 33], ['ROTATE', 3, 56], ['RETURN', 164, 57]]) {
    [...text].forEach((ch, i) => {
      for (let col = 0; col < 3; col++) for (let row = 0; row < 5; row++) {
        assert.equal(pixel(x + i * 4 + col, y + row), Boolean(tinyFont[(ch.charCodeAt(0) - 32) * 4 + col] >> row & 1), `HUD ${text}: ${ch} column ${col}, row ${row} stays legible after the one-pixel inset`);
      }
    });
  }
  g.frame(); assert.equal(g.word('dirty_bytes'), 0, 'An idle screen performs no LCD writes');
  states++;
}

function aim(g, target) {
  while ((g.read('cursor') >> 4) < (target >> 4)) g.tap('down');
  while ((g.read('cursor') >> 4) > (target >> 4)) g.tap('up');
  while (g.read('cursor') % 16 < target % 16) g.tap('right');
  while (g.read('cursor') % 16 > target % 16) g.tap('left');
}
for (const index of [0, 4, 10, 30, 39]) {
  const g = await Game.open(wasm, out, 'mirror-link');
  g.out = resolve(out, 'visual-check');
  try {
    const stage = stages[index];
    await g.start(index); verify(g, stage); await g.save(`stage-${index + 1}-initial`);
    for (const target of stage.bonus.slice(0, 2)) {
      aim(g, target); g.tap('space'); verify(g, stage);
      const before = g.read('board', 112), panel = [...g.machine.lcdPanel().dots];
      g.tap('return'); g.tap('return');
      assert.deepEqual(g.read('board', 112), before);
      assert.deepEqual([...g.machine.lcdPanel().dots], panel, 'Menu return redraws the same walls and HUD without another shift');
      verify(g, stage);
    }
    frames += g.frames;
  } finally { g.destroy(); }
}
const result = {passed: true, stages: stages.length, sourceChecks, orientationChecks,
  visualStages: 5, states, wallChecks, frames};
await writeFile(resolve(out, 'visual-verification.json'), JSON.stringify(result, null, 2) + '\n');
console.log(JSON.stringify(result));
