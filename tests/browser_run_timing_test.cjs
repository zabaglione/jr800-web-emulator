// SPDX-License-Identifier: MIT
// Optional real-browser check. Requires Playwright and an installed Chrome.
// Serve build/wasm-release/web first; pass its URL as the first argument.
const assert = require('node:assert/strict');
const {chromium} = require('playwright');

(async () => {
    const browser = await chromium.launch({channel: 'chrome', headless: true});
    try {
        const page = await browser.newPage();
        await page.goto(process.argv[2] ?? 'http://localhost:8000/');
        const results = await page.evaluate(async () => {
            const entry = document.querySelector('script[type="module"][src]').src;
            const {WasmMachine} = await import(new URL('./wasm-machine.mjs', entry));
            const {Jr800BasicRunSlice} = await import(new URL('./basic-boot-profile.mjs', entry));
            const url = new URL('./jr800_wasm.mjs', entry).href;
            const rom = new Uint8Array(32768).fill(1);
            // Project-authored BRA-to-self fixture: three E cycles/instruction.
            rom.set([0x20, 0xfe]);
            rom[32766] = 0x80;
            rom[32767] = 0;
            const machine = await WasmMachine.createJr800(url);
            machine.loadLogicalRom(rom);
            const begin = performance.now();
            machine.run(250_000);
            const directMs = performance.now() - begin;
            machine.destroy();

            const worker = new Worker(new URL('./jr800-worker.mjs', entry), {type: 'module'});
            let id = 0;
            const pending = new Map();
            let stopped;
            const progressCycles = [];
            worker.onmessage = ({data: message}) => {
                if (message.type === 'response') {
                    const request = pending.get(message.id);
                    pending.delete(message.id);
                    clearTimeout(request.timeout);
                    if (message.ok) request.resolve(message.result);
                    else request.reject(new Error(message.error));
                } else if (message.event === 'progress') {
                    progressCycles.push(Number(message.snapshot.state.cycleCount));
                } else if (message.event === 'stopped') {
                    stopped(message);
                }
            };
            const request = (command, fields = {}) => new Promise((resolve, reject) => {
                const requestId = ++id;
                const timeout = setTimeout(() => reject(new Error(command)), 15_000);
                pending.set(requestId, {resolve, reject, timeout});
                worker.postMessage({id: requestId, command, ...fields});
            });
            const nextStop = () => new Promise((resolve, reject) => {
                const timeout = setTimeout(() => reject(new Error('Stop timeout')), 15_000);
                stopped = message => { clearTimeout(timeout); resolve(message); };
            });
            const run = async fields => {
                const before = Number((await request('snapshot')).state.cycleCount);
                const start = performance.now();
                const done = nextStop();
                await request('run', {instructionLimit: 250_000, ...fields});
                const result = await done;
                return {
                    ms: performance.now() - start,
                    cycles: Number(result.snapshot.state.cycleCount) - before,
                    reason: result.stop.reason,
                };
            };
            await request('load-jr800-raw', {moduleUrl: url, logicalRom: rom});
            const unpaced = await run({});
            const realtime = await run({realtime: true});
            const soundRun = run({realtime: true});
            await new Promise(resolve => setTimeout(resolve, 40));
            await request('set-audio-enabled', {enabled: true});
            await new Promise(resolve => setTimeout(resolve, 40));
            await request('set-audio-enabled', {enabled: false});
            const realtimeWithAudio = await soundRun;
            const continuousStart = performance.now();
            let continuousCycles = 0;
            for (let frame = 0; frame < 40; ++frame) {
                const result = await run(Jr800BasicRunSlice);
                continuousCycles += result.cycles;
                // Exercise the normal UI's sound toggles without losing pacing.
                if (frame === 10) await request('set-audio-enabled', {enabled: true});
                if (frame === 20) await request('set-audio-enabled', {enabled: false});
            }
            const continuous = {
                ms: performance.now() - continuousStart, cycles: continuousCycles,
            };
            const paused = nextStop();
            await request('run', {instructionLimit: 10_000_000, realtime: true});
            await new Promise(resolve => setTimeout(resolve, 40));
            const pauseStart = performance.now();
            await request('pause');
            const pauseResult = await paused;
            const pause = {ms: performance.now() - pauseStart, reason: pauseResult.stop.reason};
            await request('reset');
            // SLP cycles must consume the same paced time as executed instructions.
            const sleepingRom = rom.slice();
            sleepingRom[0] = 0x1a;
            await request('load-jr800-raw', {moduleUrl: url, logicalRom: sleepingRom});
            progressCycles.length = 0;
            const sleeping = await run({realtime: true, suspendedCycleLimit: 750_000});
            sleeping.progressEvents = progressCycles.length;
            sleeping.progressMonotonic = progressCycles.every((cycle, index) =>
                index === 0 || cycle > progressCycles[index - 1]);
            worker.terminate();
            return {directMs, unpaced, realtime, realtimeWithAudio, continuous, pause, sleeping};
        });
        assert.equal(results.unpaced.cycles, 750_000);
        assert.equal(results.realtime.cycles, 750_000);
        assert.equal(results.realtime.reason, 'instruction-limit');
        assert.ok(results.unpaced.ms < 610, 'Timer-per-1000-instructions slowdown returned');
        for (const result of [results.realtime, results.realtimeWithAudio, results.continuous, results.sleeping]) {
            const ratio = result.cycles / (result.ms * 1228.8);
            assert.ok(ratio > 0.9 && ratio < 1.1, `Clock ratio out of range: ${ratio}`);
        }
        assert.equal(results.sleeping.reason, 'sleeping');
        assert.ok(results.sleeping.progressEvents >= 3, 'SLP must refresh the display before stopping');
        assert.equal(results.sleeping.progressMonotonic, true);
        assert.equal(results.pause.reason, 'paused');
        assert.ok(results.pause.ms < 200, 'Worker did not yield promptly for pause');

        // Check the actual BASIC continuous-run UI path, including DOM rendering.
        const rom = Buffer.alloc(32768, 1);
        // Set a nonzero RAM sentinel before entering the three-cycle loop.
        rom.set([0x86, 0x5a, 0xb7, 0x20, 0x00, 0x20, 0xfe]);
        rom[32766] = 0x80; rom[32767] = 0;
        page.on('dialog', dialog => dialog.accept());
        await page.locator('#jr8rom-file').setInputFiles({
            name: 'synthetic-speed.rom', mimeType: 'application/octet-stream', buffer: rom,
        });
        await page.locator('#boot-basic').click();
        await page.waitForFunction(() => Number(document.querySelector('#cycles').textContent) > 0);
        const ui = await page.evaluate(async () => {
            const startCycles = Number(document.querySelector('#cycles').textContent);
            const start = performance.now();
            await new Promise(resolve => setTimeout(resolve, 2000));
            return {
                ms: performance.now() - start,
                cycles: Number(document.querySelector('#cycles').textContent) - startCycles,
            };
        });
        const uiRatio = ui.cycles / (ui.ms * 1228.8);
        assert.ok(uiRatio > 0.9 && uiRatio < 1.1, `UI clock ratio out of range: ${uiRatio}`);
        await page.locator('#pause-basic').click();
        await page.waitForFunction(() => !document.querySelector('#resume-machine').disabled);
        // The ordinary Run button must also pace JR-800 keyboard scans.
        await page.locator('#debugger-menu > summary').click();
        await page.locator('#instruction-limit').fill('250000');
        const beforeRun = Number(await page.locator('#cycles').textContent());
        const runStart = performance.now();
        await page.locator('#run').click();
        await page.waitForFunction(before =>
            Number(document.querySelector('#cycles').textContent) > before, beforeRun);
        await page.waitForFunction(() => !document.querySelector('#resume-machine').disabled);
        const ordinaryRun = {
            ms: performance.now() - runStart,
            cycles: Number(await page.locator('#cycles').textContent()) - beforeRun,
        };
        assert.equal(ordinaryRun.cycles, 750_000);
        assert.ok(ordinaryRun.ms >= 580 && ordinaryRun.ms < 1000,
            'Ordinary JR-800 Run bypassed pacing');
        await page.locator('#memory-address').fill('$2000');
        await page.locator('#refresh-memory').click();
        await page.waitForFunction(() => document.querySelector('#memory').textContent.startsWith('$2000'));
        const beforePower = await page.locator('#memory').textContent();
        assert.match(beforePower, /\$2000\s+5A/);
        await page.locator('#power-on').click();
        await page.waitForFunction(() => !document.querySelector('#power-off').disabled);
        await page.locator('#power-off').click();
        await page.waitForFunction(() => !document.querySelector('#power-on').disabled);
        const suspendedCycles = await page.locator('#cycles').textContent();
        await page.waitForTimeout(200);
        assert.equal(await page.locator('#cycles').textContent(), suspendedCycles);
        assert.equal(await page.locator('#memory').textContent(), beforePower);
        await page.locator('#power-on').click();
        await page.waitForFunction(before =>
            document.querySelector('#cycles').textContent !== before, suspendedCycles);
        assert.ok(Number(await page.locator('#cycles').textContent()) > Number(suspendedCycles),
            'POWER ON reset the CPU instead of continuing the retained session');
        await page.locator('#power-off').click();
        await page.waitForFunction(() => !document.querySelector('#power-on').disabled);
        assert.equal(await page.locator('#memory').textContent(), beforePower);
        const inputPage = await browser.newPage();
        await inputPage.goto(process.argv[2] ?? 'http://localhost:8000/');
        inputPage.on('dialog', dialog => dialog.accept());
        const inputRom = Buffer.alloc(32768, 1);
        // Repeatedly read the A/B keyboard row into A and RAM.
        inputRom.set([0xb6, 0x0f, 0xef, 0xb7, 0x20, 0x00, 0x20, 0xf8]);
        inputRom[32766] = 0x80; inputRom[32767] = 0;
        await inputPage.locator('#jr8rom-file').setInputFiles({
            name: 'synthetic-keyboard.rom', mimeType: 'application/octet-stream', buffer: inputRom,
        });
        await inputPage.locator('#boot-basic').click();
        await inputPage.waitForFunction(() => Number(document.querySelector('#cycles').textContent) > 0);
        await inputPage.locator('#lcd-panel').click();
        await inputPage.keyboard.down('KeyA');
        await inputPage.waitForTimeout(20);
        await inputPage.keyboard.down('KeyB');
        await inputPage.waitForFunction(() => document.querySelector('#register-a').textContent === '$FB');
        await inputPage.keyboard.up('KeyA');
        await inputPage.waitForTimeout(70);
        assert.equal(await inputPage.locator('#register-a').textContent(), '$FB');
        await inputPage.keyboard.up('KeyB');
        await inputPage.waitForFunction(() => document.querySelector('#register-a').textContent === '$FF');
        assert.ok(await inputPage.locator('#power-off').isEnabled());
        assert.doesNotMatch(await inputPage.locator('#status').textContent(), /cpu-fault/);
        await inputPage.locator('#power-off').click();

        const sleepPage = await browser.newPage();
        // Shrink only the host batch quota: test many SLP continuations without
        // spending 52 seconds on each boundary. The CPU clock is unchanged.
        await sleepPage.route('**/basic-boot-profile.mjs', async route => {
            const response = await route.fetch();
            const source = await response.text();
            assert.ok(source.includes('suspendedCycleLimit: 64_000_000'));
            await route.fulfill({response, body: source.replace(
                'suspendedCycleLimit: 64_000_000', 'suspendedCycleLimit: 60_000',
            )});
        });
        await sleepPage.goto(process.argv[2] ?? 'http://localhost:8000/');
        sleepPage.on('dialog', dialog => dialog.accept());
        const sleepRom = Buffer.alloc(32768, 1);
        sleepRom[0] = 0x1a; sleepRom[32766] = 0x80; sleepRom[32767] = 0;
        await sleepPage.locator('#jr8rom-file').setInputFiles({
            name: 'synthetic-sleep.rom', mimeType: 'application/octet-stream', buffer: sleepRom,
        });
        await sleepPage.locator('#boot-basic').click();
        await sleepPage.waitForFunction(() =>
            Number(document.querySelector('#cycles').textContent) > 1_228_800,
            null, {timeout: 5000});
        assert.ok(await sleepPage.locator('#power-off').isEnabled());
        assert.equal(await sleepPage.locator('#power-on').isEnabled(), false);
        await sleepPage.locator('#power-off').click();
        await sleepPage.waitForFunction(() => !document.querySelector('#power-on').disabled);
        const sleepPaused = await sleepPage.locator('#cycles').textContent();
        await sleepPage.waitForTimeout(100);
        assert.equal(await sleepPage.locator('#cycles').textContent(), sleepPaused);
        console.log(JSON.stringify({
            ...results, ui, ordinaryRun, retainedPowerResume: true,
            overlappingKeys: true, continuousSleepBoundaries: true,
        }, null, 2));
    } finally {
        await browser.close();
    }
})().catch(error => { console.error(error); process.exitCode = 1; });
