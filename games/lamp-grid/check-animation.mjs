// SPDX-License-Identifier: MIT
// Observe intermediate LCD states and sound-port writes through real key input.
import assert from 'node:assert/strict';
import {mkdir, readFile, writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {Game, png} from '../tools/harness.mjs';
import {aim} from '../tools/puzzle_check.mjs';

const [wasm, out] = process.argv.slice(2);
const g = await Game.open(wasm, out, 'lamp-grid');
const symbols = Object.fromEntries([...(await readFile(resolve(out, 'lamp-grid.sym'), 'utf8')).matchAll(/^ \$([0-9A-F]+) [GL] (\S+)/gm)].map(([, a, n]) => [n, parseInt(a, 16)]));
const machine = g.machine, run = machine.runTo.bind(machine);
const marker = symbols.lamp_animation_frame, sound = symbols.lamp_flip_sound;
const totals = {frames: 0, actions: 0, lamps: 0, cues: 0};
const captures = [], writes = [];
let current, callback, tone, recording = false;

function cross(p) {
  return [p, ...(p >= 5 ? [p - 5] : []), ...(p < 20 ? [p + 5] : []),
    ...(p % 5 ? [p - 1] : []), ...(p % 5 < 4 ? [p + 1] : [])];
}
function prepare(cell, undo = false) {
  const before = g.read('board', 25), after = [...before], order = cross(cell);
  for (const p of order) after[p] ^= 1;
  const goals = [...machine.memory(symbols.challenge_cells, 2)];
  const beforeBonus = g.read('challenge_bonus');
  const afterBonus = undo ? machine.memory(symbols.challenge_undo_bonus, 1)[0]
    : goals.reduce((bits, p, i) => bits | (p === cell ? 1 << i : 0), beforeBonus);
  current = {before, after, order, goals, beforeBonus, afterBonus, seen: [], cues: 0};
  totals.actions++;
}
function face(value, cell, bonus) {
  const i = current.goals.indexOf(cell);
  return value + (i >= 0 && !(bonus & (1 << i)) ? 2 : 0);
}
function observe() {
  assert.ok(current, 'An observed animation belongs to a requested action');
  const s = {phase: g.read('lamp_anim_phase'), cell: g.read('lamp_anim_cell'),
    index: g.read('lamp_queue_index'), cycle: Number(machine.state().cycleCount)};
  assert.deepEqual(g.read('lamp_queue', 5).slice(0, g.read('lamp_queue_count')), current.order,
    'Only the pressed lamp and its bounded orthogonal neighbors flip');
  assert.equal(s.cell, current.order[s.index]);
  assert.deepEqual(g.read('board', 25), current.after);
  const visible = [...current.before];
  for (const cell of current.order.slice(0, s.index + Number(s.phase === 4))) visible[cell] ^= 1;
  assert.deepEqual(g.read('lamp_display', 25), visible, 'One lamp changes at the full new face');
  assert.equal(g.read('grid_stat'), visible.reduce((sum, on) => sum + on, 0));
  assert.equal(g.read('lamp_view_bonus'), s.index || s.phase === 4 ? current.afterBonus : current.beforeBonus,
    'The target marker and BONUS status change after the center lamp finishes');
  assert.equal(g.read('phase'), 2, 'Clear waits for the final lamp');
  assert.equal(g.read('cursor'), 255, 'The selection does not obscure rotating faces');
  const oldFace = face(current.before[s.cell], s.cell, current.beforeBonus);
  const newFace = face(current.after[s.cell], s.cell, current.afterBonus);
  const sprite = [0, oldFace + 4, 8, newFace + 4, newFace][s.phase];
  assert.equal(g.read('lamp_sprite'), sprite);
  const panel = machine.lcdPanel(), tile = machine.memory(symbols.tiles + 8 + sprite * 16, 16);
  const left = 24 + s.cell % 5 * 16, top = 16 + Math.floor(s.cell / 5) * 8;
  for (let y = 0; y < 8; y++) for (let x = 0; x < 16; x++)
    assert.equal(panel.dots[(top + y) * 192 + left + x], 1 + ((tile[x] >> y) & 1));
  const fb = machine.memory(symbols.framebuffer, 1536);
  for (let y = 0; y < 64; y++) for (let x = 0; x < 192; x++)
    assert.equal(panel.dots[y * 192 + x], 1 + ((fb[(y >> 3) * 192 + x] >> (y & 7)) & 1));
  const key = `${s.index}:${s.phase}`;
  if (current.seen.at(-1)?.key !== key) {
    const previous = current.seen.at(-1);
    if (previous) assert.ok(s.cycle - previous.cycle >= 20000, 'Faces remain visible for an emulated tick or longer');
    current.seen.push({key, cycle: s.cycle});
    if (s.phase === 4) totals.lamps++;
  }
  if (tone) {
    assert.equal(tone.length, 24, 'A lamp cue has twelve complete sound periods');
    tone.forEach((edge, i) => assert.equal(edge.value & 16, i % 2 ? 0 : 16));
    tone = undefined;
  }
  totals.frames++;
  if (recording) captures.push({cycle: s.cycle, phase: s.phase, cell: s.cell, png: png(panel.dots)});
  callback?.(s);
}
function completed() {
  assert.equal(g.read('lamp_active'), 0);
  assert.deepEqual(g.read('board', 25), current.after);
  assert.deepEqual(current.seen.map(s => s.key), current.order.flatMap((_, i) => [1, 2, 3, 4].map(p => `${i}:${p}`)));
  assert.equal(current.cues, current.order.length, 'Exactly one cue accompanies each lamp');
  assert.equal(g.read('lamp_view_bonus'), current.afterBonus);
  if (g.read('phase') === 2) {
    const p = g.read('cursor'), base = face(current.after[p], p, current.afterBonus);
    const tile = machine.memory(symbols.tiles + 8 + base * 16, 16), panel = machine.lcdPanel();
    const left = 24 + p % 5 * 16, top = 16 + Math.floor(p / 5) * 8;
    for (let y = 0; y < 8; y++) for (let x = 3; x < 13; x++)
      assert.equal(panel.dots[(top + y) * 192 + left + x], 1 + ((tile[x] >> y) & 1),
        'The cursor preserves the lamp and bonus colors');
    for (const x of [0,15]) for (const y of [0,7])
      assert.equal(panel.dots[(top + y) * 192 + left + x], 2, 'Corner brackets identify the selected lamp');
  }
}

machine.setExecutionBreakpoint(marker, true);
machine.setExecutionBreakpoint(sound, true);
machine.setMemoryWatchpoint(2, 'write', true);
machine.runTo = (address, limit) => {
  for (let stops = 0; stops < 10000; stops++) {
    const stop = run(address, limit);
    if (stop.reason === 'memory-watchpoint') {
      const edge = machine.accesses({firstAddress: 2, lastAddress: 2}).at(-1);
      tone?.push(edge);
      if (recording) writes.push({cycle: Number(edge.instructionCycle), value: edge.value});
      machine.step(); continue;
    }
    if (stop.reason !== 'execution-breakpoint') return stop;
    if (machine.state().pc === sound) {
      current.cues++; totals.cues++; tone = []; machine.step(); continue;
    }
    assert.equal(machine.state().pc, marker);
    machine.step();
    assert.equal(run(marker + 3, limit).reason, 'address-reached');
    observe();
  }
  throw new Error('Unbounded animation instrumentation');
};

try {
  await g.start();
  const initial = g.read('board', 25);
  // Every cell covers the corners, borders, interior, both lamp states and goals.
  for (let p = 0; p < 25; p++) {
    g.menu(2); aim(g, p, 5); prepare(p); g.tap('space'); completed();
    assert.equal(g.read('cursor'), p);
    aim(g, (p + 1) % 25, 5); const cursor = g.read('cursor');
    prepare(p, true); g.menu(1); completed();
    assert.deepEqual(g.read('board', 25), initial);
    assert.equal(g.read('cursor'), cursor); assert.equal(g.word('challenge_moves'), 2);
    assert.equal(g.read('challenge_bonus'), 0);
  }
  // Pause on different faces; freezing, resume, undo and reset share the shell.
  for (const [part, action] of [[1, 'resume'], [2, 'undo'], [3, 'reset']]) {
    g.menu(2); aim(g, 0, 5); prepare(0); let paused = false;
    callback = s => {
      if (!paused && s.index === 1 && s.phase === part) {paused = true; g.hold('return', true);}
    };
    g.tap('space'); assert.ok(paused); assert.equal(g.read('phase'), 3);
    callback = undefined; g.hold('return', false); g.frame();
    const display = g.read('lamp_display', 25), cues = totals.cues;
    for (let i = 0; i < 180; i++) g.frame();
    assert.deepEqual(g.read('lamp_display', 25), display); assert.equal(totals.cues, cues);
    if (action === 'resume') {g.tap('return'); completed();}
    else {
      for (let i = 0; i < (action === 'undo' ? 1 : 2); i++) g.tap('down');
      g.tap('space'); assert.deepEqual(g.read('board', 25), initial);
      assert.equal(g.read('challenge_bonus'), 0); assert.equal(g.read('lamp_view_bonus'), 0);
    }
    assert.equal(g.read('lamp_active'), 0); assert.equal(machine.state().sp, 0x5fff);
  }
  // Holding SPACE and a direction during the turn cannot queue another move.
  g.menu(2); aim(g, 12, 5); prepare(12);
  callback = () => g.hold('right', true);
  g.hold('space', true); g.frame(); completed(); callback = undefined;
  const moves = g.word('challenge_moves');
  for (let i = 0; i < 10; i++) g.frame();
  assert.equal(g.word('challenge_moves'), moves); assert.equal(g.read('cursor'), 12);
  g.hold('space', false); g.hold('right', false); g.frame();
  // The final solved face survives a pause before the result celebration.
  const stages = JSON.parse(await readFile(new URL('challenges.json', import.meta.url)));
  const route = stages.stages[0].normal;
  assert.ok(route, 'Stage one supplies an independently certified solution');
  g.menu(2);
  for (let i = 0; i < route.length; i++) {
    const p = route[i]; aim(g, p, 5); prepare(p); let paused = false;
    if (i === route.length - 1) callback = s => {
      if (!paused && s.index === current.order.length - 1 && s.phase === 3) {paused = true; g.hold('return', true);}
    };
    g.tap('space'); callback = undefined;
    if (i === route.length - 1) {
      assert.ok(paused); assert.equal(g.read('phase'), 3);
      g.hold('return', false); g.frame(); g.tap('return');
      assert.equal(g.read('phase'), 4); assert.equal(g.read('clear_active'), 1);
    }
    completed();
  }
  g.finishClear();
  // Capture a target pickup and a five-lamp turn at the real emulated timing.
  g.tap('return'); g.tap('space');
  const still = () => captures.push({cycle: Number(machine.state().cycleCount), phase: 0,
    cell: g.read('cursor'), png: png(machine.lcdPanel().dots)});
  for (const p of [0, 12]) {
    aim(g, p, 5); prepare(p); still(); recording = true; g.tap('space'); recording = false; completed(); still();
  }
  const directory = resolve(out, 'animation'); await mkdir(directory, {recursive: true});
  for (const [i, frame] of captures.entries()) await writeFile(resolve(directory, `${String(i).padStart(3, '0')}.png`), frame.png);
  await writeFile(resolve(directory, 'timing.json'), JSON.stringify({clockHz: 1228800,
    frames: captures.map(({png, ...frame}) => frame), writes}, null, 2) + '\n');
  const result = {passed: true, ...totals, allCells: 25, pausedCases: 4,
    undoAnimated: true, resumedWin: true, heldInput: true, actualPortWrites: true, physicalDevice: false};
  await writeFile(resolve(out, 'animation-verification.json'), JSON.stringify(result, null, 2) + '\n');
  console.log(JSON.stringify(result));
} finally {g.destroy();}
