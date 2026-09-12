// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {readFile, writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {Game} from '../tools/harness.mjs';

const [wasm, out] = process.argv.slice(2);
const stages = JSON.parse(await readFile(new URL('./challenges.json', import.meta.url))).stages;
const font = (await readFile(new URL('../../sdk/lib/lcd/font.s', import.meta.url), 'utf8'))
  .split('\n').filter(line => line.includes('.byte'))
  .map(line => [...line.split(';')[0].matchAll(/\$([0-9A-F]{2})/g)].map(m => parseInt(m[1], 16)));
const adjacent = p => [p >= 6 ? p - 6 : -1, p < 30 ? p + 6 : -1,
  p % 6 ? p - 1 : -1, p % 6 < 5 ? p + 1 : -1].filter(q => q >= 0);
for (const stage of stages) {
  const board = stage.initial.board, seen = new Set([stage.initial.start]), pending = [...seen];
  for (const p of pending) for (const q of adjacent(p)) {
    if (board[q] && !seen.has(q)) { seen.add(q); pending.push(q); }
  }
  assert.equal(seen.size, board.filter(Boolean).length, 'Every displayed node connects to S');
  assert.equal(stage.metrics.open_squares, seen.size);
  assert.ok(stage.bonus_cells.every(p => board[p] === 1));
}

let frames = 0, states = 0, undoChecks = 0;
function verify(g, stage, visited) {
  const dots = g.machine.lcdPanel().dots;
  const pixel = (x, y) => dots[y * 192 + x] === 2;
  const cellPixel = (p, x, y) => pixel(16 + p % 6 * 16 + x, 8 + Math.floor(p / 6) * 8 + y);
  const board = stage.initial.board;
  const next = 1 + [...visited].filter(p => board[p] >= 2 && board[p] <= 4).length;
  assert.equal(g.read('loop_next'), next);
  for (const [i, letter] of [...'ABCS'].entries()) {
    const p = board.indexOf(i + 2), inverse = p === g.read('cursor');
    assert.equal(cellPixel(p, 3, 3), !inverse, 'Gate has a substantial, distinct frame');
    for (const [x, bits] of font[letter.charCodeAt(0) - 32].entries()) {
      for (let y = 0; y < 7; y++) if (bits >> y & 1) {
        assert.equal(cellPixel(p, x + 5, y), inverse, 'Gate letter remains visible along the route');
        if (i < 3) assert.equal(pixel(132 + i * 18 + x + 5, 32 + y), false,
          'The right legend uses the same gate lettering');
      }
    }
  }
  for (let i = 0; i < 3; i++) {
    const x = 132 + i * 18;
    if (i + 1 < next) {
      assert.ok(pixel(x + 12, 40) && pixel(x + 6, 46), 'Passed gate has a check mark');
    } else if (i + 1 === next) {
      assert.ok(pixel(x + 7, 41) && !pixel(x + 12, 40), 'Next gate has an upward arrow');
    } else {
      const ink = Array.from({length: 128}, (_, n) => pixel(x + n % 16, 40 + (n >> 4))).filter(Boolean).length;
      assert.equal(ink, 4, 'Later gates retain a quiet pending marker');
    }
  }
  let bonus = 0;
  stage.bonus_cells.forEach((p, i) => {
    const collected = visited.has(p), inverse = p === g.read('cursor'), x = [142, 168][i];
    if (collected) {
      bonus |= 1 << i;
      assert.ok(pixel(x + 12, 56) && pixel(x + 6, 62), 'Collected bonus becomes a check mark');
      assert.equal(cellPixel(p, 7, 3), !inverse, 'The route replaces the collected jewel');
    } else {
      const ink = Array.from({length: 128}, (_, n) => cellPixel(p, n % 16, n >> 4)).filter(Boolean).length;
      assert.ok(ink > 20, 'Uncollected bonus is a large central symbol');
      assert.ok(cellPixel(p, 7, 0) && !cellPixel(p, 7, 3), 'Bonus jewel has a contrasting inset');
      assert.ok(pixel(x + 7, 56) && !pixel(x + 7, 59), 'Uncollected bonus stays a jewel in the legend');
    }
  });
  assert.equal(g.read('challenge_bonus'), bonus);
  const digit = ['111/101/101/101/111', '010/110/010/010/111', '110/001/010/100/111']
    [stage.bonus_cells.filter(p => visited.has(p)).length].split('/');
  for (let y = 0; y < 5; y++) for (let x = 0; x < 3; x++) {
    assert.equal(pixel(171 + x, 48 + y), digit[y][x] === '1', 'Bonus counter follows the visible checks');
  }
  g.frame(); assert.equal(g.word('dirty_bytes'), 0, 'An idle legend performs no LCD writes');
  states++;
}

for (const index of [0, 30, 39]) {
  const g = await Game.open(wasm, out, 'loop-trace');
  g.out = resolve(out, 'visual-check');
  try {
    const stage = stages[index], visited = new Set([stage.initial.start]);
    await g.start(index); assert.deepEqual(g.read('board', 36), stage.initial.board);
    verify(g, stage, visited); await g.save(`stage-${index + 1}-initial`);
    let p = stage.initial.start, progressSaved = false;
    for (const q of stage.bonus) {
      if (q === 'close') {
        await g.save(`stage-${index + 1}-complete`);
        g.tap('space'); assert.equal(g.read('phase'), 4); break;
      }
      const key = q === p - 6 ? 'up' : q === p + 6 ? 'down' : q === p - 1 ? 'left' : 'right';
      g.tap(key); visited.add(q); verify(g, stage, visited);
      if (stage.initial.board[q] > 1 || stage.bonus_cells.includes(q)) {
        g.menu(1); visited.delete(q); assert.equal(g.read('cursor'), p); verify(g, stage, visited);
        g.tap(key); visited.add(q); verify(g, stage, visited); undoChecks++;
      }
      if (!progressSaved && g.read('loop_next') === 3) {
        g.tap('return'); g.tap('return'); verify(g, stage, visited);
        await g.save(`stage-${index + 1}-progress`); progressSaved = true;
      }
      p = q;
    }
    frames += g.frames;
  } finally { g.destroy(); }
}
const result = {passed: true, connectedStages: stages.length, visualStages: 3, frames, states, undoChecks};
await writeFile(resolve(out, 'visual-verification.json'), JSON.stringify(result, null, 2) + '\n');
console.log(JSON.stringify(result));
