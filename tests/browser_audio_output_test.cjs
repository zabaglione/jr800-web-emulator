// SPDX-License-Identifier: MIT
// Optional browser waveform check. Uses only project-authored signals, no ROM.
// Serve build/wasm-release/web and pass its URL; requires Playwright and Chrome.
const assert = require('node:assert/strict');
const {chromium} = require('playwright');

(async () => {
    const browser = await chromium.launch({channel: 'chrome', headless: true});
    try {
        const page = await browser.newPage();
        await page.goto(process.argv[2] ?? 'http://localhost:8000/');
        const results = await page.evaluate(async () => {
            const entry = document.querySelector('script[type="module"][src]').src;
            const {Jr800AudioOutput} = await import(new URL('./audio-output.mjs', entry));
            const results = [];
            for (const sampleRate of [44100, 48000]) {
                const offline = new OfflineAudioContext(1, sampleRate, sampleRate);
                let arrivalTime = 0;
                let filterCount = 0;
                const context = {
                    state: 'running', sampleRate,
                    get currentTime() { return arrivalTime; },
                    destination: offline.destination,
                    createBuffer: (...args) => offline.createBuffer(...args),
                    createBufferSource: () => offline.createBufferSource(),
                    createBiquadFilter: () => { ++filterCount; return offline.createBiquadFilter(); },
                };
                const output = new Jr800AudioOutput(() => context);
                await output.activate();
                const clockHz = 1_200_000;
                const transitions = [];
                for (const {start, end, frequency, duty} of [
                    {start: 0, end: 0.05, frequency: 1600, duty: 0.5},
                    {start: 0.4, end: 0.6, frequency: 440, duty: 0.5},
                    {start: 0.7, end: 0.9, frequency: 20000, duty: 1 / 3},
                ]) {
                    for (let index = 0; index < Math.round((end - start) * frequency); ++index) {
                        transitions.push(
                            {cycle: Math.round((start + index / frequency) * clockHz), level: true},
                            {cycle: Math.round((start + (index + duty) / frequency) * clockHz), level: false},
                        );
                    }
                }
                let edge = 0;
                for (let startCycle = 0; startCycle < clockHz;) {
                    const endCycle = Math.min(clockHz, startCycle + 9600);
                    const chunk = [];
                    while (edge < transitions.length && transitions[edge].cycle < endCycle) chunk.push(transitions[edge++]);
                    arrivalTime = startCycle / clockHz;
                    output.append({clockHz, startCycle, endCycle, initialLevel: false, transitions: chunk});
                    startCycle = endCycle;
                }
                const buffer = await offline.startRendering();
                const samples = buffer.getChannelData(0);
                function rms(start, end) {
                    const part = samples.slice(Math.round(start * sampleRate), Math.round(end * sampleRate));
                    return Math.sqrt(part.reduce((sum, x) => sum + x * x, 0) / part.length);
                }
                function frequency(start, end) {
                    const rising = [];
                    for (let i = Math.round(start * sampleRate); i < end * sampleRate; ++i) {
                        if (samples[i] <= 0 && samples[i + 1] > 0) rising.push(i);
                    }
                    return (rising.length - 1) * sampleRate / (rising.at(-1) - rising[0]);
                }
                function amplitude(hz) {
                    let real = 0; let imaginary = 0;
                    const start = Math.round(0.76 * sampleRate);
                    const end = start + Math.round(0.1 * sampleRate);
                    for (let i = start; i < end; ++i) {
                        real += samples[i] * Math.cos(2 * Math.PI * hz * i / sampleRate);
                        imaginary += samples[i] * Math.sin(2 * Math.PI * hz * i / sampleRate);
                    }
                    return 2 * Math.hypot(real, imaginary) / (end - start);
                }
                results.push({
                    sampleRate,
                    onsetMs: samples.findIndex(x => Math.abs(x) > 0.001) / sampleRate * 1000,
                    keyToneHz: frequency(0.035, 0.07),
                    aToneHz: frequency(0.45, 0.61),
                    silentRms: rms(0.2, 0.4),
                    activeRms: rms(0.035, 0.07),
                    aliasAmplitude: amplitude(Math.abs(sampleRate - 40000)),
                    filterCount,
                });
            }
            return results;
        });
        for (const result of results) {
            assert.ok(result.onsetMs >= 25 && result.onsetMs <= 27, JSON.stringify(result));
            assert.ok(Math.abs(result.keyToneHz - 1600) < 5, JSON.stringify(result));
            assert.ok(Math.abs(result.aToneHz - 440) < 2, JSON.stringify(result));
            assert.ok(result.silentRms < 0.0001, JSON.stringify(result));
            assert.ok(result.activeRms > 0.1, JSON.stringify(result));
            assert.ok(result.aliasAmplitude < 0.0001, JSON.stringify(result));
            assert.equal(result.filterCount, 1);
        }
        console.log(JSON.stringify(results));
    } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
