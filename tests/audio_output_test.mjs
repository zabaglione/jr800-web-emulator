// SPDX-License-Identifier: MIT

import assert from "node:assert/strict";
import {pathToFileURL} from "node:url";

const [modulePath] = process.argv.slice(2);
if (!modulePath) {
    throw new Error("Usage: node audio_output_test.mjs <audio-output.mjs>");
}

const {Jr800AudioOutput, renderAudioTransitions} = await import(
    pathToFileURL(modulePath)
);

const samples = renderAudioTransitions([
    {cycle: 0, level: true},
    {cycle: 10, level: false},
    {cycle: 20, level: true},
], 1000, 1000, 0.25);
assert.ok(samples.length > 20);
assert.equal(samples[0], 0.25);
assert.equal(samples[10], -0.25);
assert.equal(samples[19], -0.25);
assert.ok(Math.abs(samples.at(-1)) < 0.000001);
assert.equal(renderAudioTransitions([], 1000, 1000).length, 0);
assert.throws(
    () => renderAudioTransitions([
        {cycle: 2, level: true},
        {cycle: 1, level: false},
    ], 1000, 1000),
    /ordered cycle levels/,
);

const output = new Jr800AudioOutput(() => null);
assert.equal(await output.activate(), false);
output.append({
    clockHz: 1_228_800,
    transitions: [{cycle: 1, level: true}, {cycle: 2, level: false}],
});
assert.equal(output.flush(), false);
output.setEnabled(false);
assert.equal(output.enabled, false);
