// SPDX-License-Identifier: MIT
// Observe intermediate LCD frames and actual sound-port writes without changing RAM.
import assert from 'node:assert/strict';
import {mkdir, readFile, writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {Game, png} from '../tools/harness.mjs';
const [wasm, out] = process.argv.slice(2);
const stages = JSON.parse(await readFile(new URL('./challenges.json', import.meta.url))).stages;
const human = ['..##....','..##....','...#....','.#####..','...#....','..#.#...','..#.#...','........'];
const captures = [], sequences = []; let audioEdges = 0, frames = 0;
for (const index of [0, 10, 30, 39]) {
  const g = await Game.open(wasm, out, 'switch-maze'), stage = stages[index];
  const machine = g.machine, run = machine.runTo.bind(machine), symbol = g.symbols.switch_intro_frame;
  const observations = [], writes = [];
  let active = false, from = 0;
  const start = stage.initial.start, exit = stage.initial.board.indexOf(8);
  const viewCell = p => Math.floor(p / 14) * 16 + p % 14 + 1;
  machine.setExecutionBreakpoint(symbol, true); machine.setMemoryWatchpoint(2, 'write', true);
  machine.runTo = (address, limit) => {
    for (let i = 0; i < 10000; i++) {
      const stop = run(address, limit);
      if (stop.reason === 'memory-watchpoint') {
        if (active) {
          const edge = machine.accesses({firstAddress: 2, lastAddress: 2}).at(-1);
          writes.push({cycle: Number(edge.instructionCycle), value: edge.value & 16});
        }
        machine.step(); continue;
      }
      if (stop.reason !== 'execution-breakpoint') return stop;
      assert.equal(machine.state().pc, symbol);
      const target = g.read('switch_intro_target'), step = g.read('switch_intro_step');
      if (!observations.length) { active = true; from = Number(machine.state().cycleCount); }
      assert.equal(g.read('cursor'), start, 'The player stays at the start throughout both cues');
      assert.equal(g.word('challenge_moves'), 0); assert.equal(g.read('gates'), 0);
      assert.deepEqual(g.read('board', 98), stage.initial.board, 'Intro changes no maze state');
      const panel = machine.lcdPanel();
      const sx = 8 + start % 14 * 8, sy = 8 + Math.floor(start / 14) * 8;
      const inverted = target === viewCell(start) && step % 2 === 0;
      for (let y = 0; y < 8; y++) for (let x = 0; x < 8; x++) {
        assert.equal(panel.dots[(sy + y) * 192 + sx + x] === 2, (human[y][x] === '#') !== inverted, 'Visible player has a head, arms and separate feet in the correct palette');
      }
      observations.push({target, step, cycle: Number(machine.state().cycleCount), pixels: [...panel.dots]});
      if (index === 0) captures.push({target, step, cycle: Number(machine.state().cycleCount), png: png(panel.dots)});
      machine.step();
    }
    throw new Error('Intro instrumentation exceeded its bound');
  };
  try {
    await g.start(index); active = false;
    assert.deepEqual(observations.map(f => [f.target, f.step]), [start, exit].flatMap(p => [0, 1, 2, 3].map(step => [viewCell(p), step])), 'Two start blinks finish before two exit blinks');
    for (let i = 1; i < observations.length; i++) {
      assert.ok(observations[i].cycle - observations[i - 1].cycle >= 120000, 'Each blink remains visible for at least 0.09 seconds');
      if (i % 4) {
        const before = observations[i - 1], now = observations[i], cell = now.target;
        for (let y = 0; y < 64; y++) for (let x = 0; x < 192; x++) {
          const inside = x >= (cell % 16) * 8 && x < (cell % 16 + 1) * 8 && y >= 8 + Math.floor(cell / 16) * 8 && y < 16 + Math.floor(cell / 16) * 8;
          assert.equal(now.pixels[y * 192 + x], inside ? 3 - before.pixels[y * 192 + x] : before.pixels[y * 192 + x], 'Only the indicated start or exit tile blinks');
        }
      }
    }
    assert.equal(writes.length, 80, 'Exactly one 20-period cue sounds for each location');
    writes.forEach((edge, i) => assert.equal(edge.value, i % 2 ? 0 : 16));
    assert.ok(writes[40].cycle - writes[0].cycle > 500000, 'Start and goal cues are audibly separate');
    audioEdges += writes.length;
    const panel = [...machine.lcdPanel().dots]; g.frame(); assert.equal(g.word('dirty_bytes'), 0);
    g.tap('return'); g.tap('return');
    assert.deepEqual([...machine.lcdPanel().dots], panel, 'Menu return preserves the normal player palette');
    assert.equal(observations.length, 8, 'Opening the menu does not replay the introduction');
    sequences.push({stage: index + 1, start, exit, durationCycles: Number(machine.state().cycleCount) - from, observations: observations.map(({pixels, ...frame}) => frame)});
    frames += g.frames;
  } finally { g.destroy(); }
}
const directory = resolve(out, 'intro'); await mkdir(directory, {recursive: true});
for (const [i, frame] of captures.entries()) await writeFile(resolve(directory, String(i).padStart(2, '0') + '.png'), frame.png);
await writeFile(resolve(directory, 'timing.json'), JSON.stringify({clockHz: 1228800, frames: captures.map(({png, ...frame}) => frame), sequences}, null, 2) + '\n');
const result = {passed: true, visualStages: 4, blinkFrames: 32, audioEdges, sequentialCues: true, playerOrientation: true, frames, physicalDevice: false};
await writeFile(resolve(out, 'intro-verification.json'), JSON.stringify(result, null, 2) + '\n'); console.log(JSON.stringify(result));
