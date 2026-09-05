// SPDX-License-Identifier: MIT

import assert from "node:assert/strict";
import {pathToFileURL} from "node:url";

const [modulePath] = process.argv.slice(2);
if (!modulePath) throw new Error("Usage: node audio_output_test.mjs <audio-output.mjs>");
const moduleUrl = pathToFileURL(modulePath);
const {Jr800AudioOutput} = await import(moduleUrl);
const {Jr800AudioSignal} = await import(new URL("audio-signal.mjs", moduleUrl));

const frame = (startCycle, endCycle, transitions = [], clockHz = 1000) => ({
    clockHz, startCycle, endCycle, initialLevel: false, transitions,
});
function fakeContext() {
    return {
        currentTime: 1, sampleRate: 48000, state: "running", destination: {},
        sources: [], filters: [],
        async resume() { this.state = "running"; },
        createBuffer(channels, length, sampleRate) {
            return {duration: length / sampleRate, copyToChannel(samples) { this.samples = samples; }};
        },
        createBufferSource() {
            const node = {
                connect(target) { this.target = target; },
                start(time) { this.time = time; },
                stop() { this.stopped = true; },
                disconnect() { this.disconnected = true; },
                addEventListener() {},
            };
            this.sources.push(node);
            return node;
        },
        createBiquadFilter() {
            const node = {
                frequency: {value: 0}, Q: {value: 0},
                connect(target) { this.target = target; },
                disconnect() { this.disconnected = true; },
            };
            this.filters.push(node);
            return node;
        },
    };
}
const noAudio = new Jr800AudioOutput(() => null);
assert.equal(await noAudio.activate(), false);
noAudio.append(frame(0, 10));
assert.equal(noAudio.signal, null);
const context = fakeContext();
const output = new Jr800AudioOutput(() => context);
assert.equal(await output.activate(), true);
// A packet is scheduled immediately, without waiting for a CPU stop.
output.append(frame(0, 10, [{cycle: 2, level: true}]));
const first = context.sources[0];
assert.equal(first.time, 1.025);
context.currentTime += 0.01;
output.append(frame(10, 20, [{cycle: 12, level: false}]));
assert.equal(context.sources[1].time, first.time + first.buffer.duration);
assert.equal(context.filters.length, 1);
assert.equal(first.target.type, "highpass");
// Silent packets retain the sample timeline; repeated edges are not required.
context.currentTime += 0.01;
output.append(frame(20, 30));
assert.equal(context.filters.length, 1);
// A stalled host discards stale queued audio instead of replaying the backlog.
context.currentTime = 5;
output.append(frame(30, 40));
assert.equal(first.stopped, true);
assert.equal(first.disconnected, true);
assert.equal(context.sources.at(-1).time, 5.025);
assert.throws(() => output.append(frame(40, 50, [], 0)), /positive/);
assert.throws(() => output.append(frame(40, 50, [
    {cycle: 45, level: true}, {cycle: 44, level: false},
])), /ordered cycle levels/);
// Clock changes and missing packets reset only the host output mapping.
output.append(frame(0, 10, [], 2000));
assert.equal(context.sources.at(-1).time, 5.025);
output.append(frame(100, 110, [], 2000));
assert.equal(context.sources.at(-1).time, 5.025);
output.setEnabled(false);
assert.equal(output.signal, null);
assert.equal(context.sources.at(-1).stopped, true);
const count = context.sources.length;
output.append(frame(0, 10));
assert.equal(context.sources.length, count);
output.setEnabled(true);
context.state = "suspended";
assert.equal(await output.activate(), true);
output.reset(); output.reset();

// A continuous-time 20 kHz pulse train has a 40 kHz second harmonic.
// Naive 44.1 kHz sampling folds it to 4.1 kHz; filtering AFTER sampling cannot fix it.
const clock = 1_200_000;
const rate = 44100;
const end = 240000;
const transitions = [];
for (let cycle = 0; cycle < end; cycle += 60) {
    transitions.push({cycle, level: true}, {cycle: cycle + 20, level: false});
}
const signal = new Jr800AudioSignal(clock, rate, 0, false);
const whole = signal.render(frame(0, end, transitions, clock));
const splitSignal = new Jr800AudioSignal(clock, rate, 0, false);
const split = [];
let edge = 0;
for (let start = 0; start < end;) {
    const next = Math.min(end, start + 1001);
    const chunk = [];
    while (edge < transitions.length && transitions[edge].cycle < next) chunk.push(transitions[edge++]);
    split.push(...splitSignal.render(frame(start, next, chunk, clock)));
    start = next;
}
assert.deepEqual(new Float32Array(split), whole, "Packet boundaries must not change samples");
function amplitude(samples, hz) {
    let real = 0; let imaginary = 0;
    const start = 4410;
    for (let i = start; i < samples.length; ++i) {
        real += samples[i] * Math.cos(2 * Math.PI * hz * i / rate);
        imaginary += samples[i] * Math.sin(2 * Math.PI * hz * i / rate);
    }
    return 2 * Math.hypot(real, imaginary) / (samples.length - start);
}
const naive = Float32Array.from(whole, (_, index) =>
    (index * clock / rate) % 60 < 20 ? 0.12 : -0.12);
assert.ok(amplitude(naive, 4100) > 0.05);
assert.ok(amplitude(whole, 4100) < 0.0001, "Audible folded harmonics must be suppressed");
// Kernel normalization preserves a held logical level instead of slowly drifting.
const dc = new Jr800AudioSignal(1000, 48000, 0, false);
const step = dc.render(frame(0, 100, [{cycle: 0, level: true}]));
assert.ok(Math.abs(step.at(-1) - 0.24) < 1e-7);
