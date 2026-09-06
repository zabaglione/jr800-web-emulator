// SPDX-License-Identifier: MIT
// Actual Chrome input and IndexedDB lifecycle; no gameplay state writes.
const assert = require('node:assert/strict');
const {writeFile} = require('node:fs/promises');
const {resolve} = require('node:path');
const {chromium} = require('playwright');
const [url = 'http://127.0.0.1:8765/', output = 'build/sdk-lcd/07-relic-dive', romPath] = process.argv.slice(2);
(async () => {
    const browser = await chromium.launch({channel: 'chrome', headless: true});
    const errors = [];
    try {
        const context = await browser.newContext({viewport: {width: 1440, height: 1100}});
        await context.addInitScript(() => {
            const NativeWorker = window.Worker;
            window.Worker = class extends NativeWorker {
                constructor(...args) { super(...args); window.relicDiveWorker = this; }
            };
            let sequence = 1_000_000;
            window.relicDiveSnapshot = (address, length) => new Promise((resolve, reject) => {
                const worker = window.relicDiveWorker, id = ++sequence;
                const timer = setTimeout(() => reject(new Error('Snapshot timeout')), 5000);
                const receive = ({data}) => {
                    if (data.id !== id || data.type !== 'response') return;
                    clearTimeout(timer); worker.removeEventListener('message', receive);
                    if (data.ok) resolve(data.result); else reject(new Error(data.error));
                };
                worker.addEventListener('message', receive);
                worker.postMessage({id, command: 'snapshot', view: {memoryAddress: address, memoryLength: length}});
            });
        });
        const page = await context.newPage();
        page.on('pageerror', e => errors.push(String(e)));
        page.on('dialog', d => d.accept());
        await page.goto(url);
        if (romPath) await page.locator('#jr8rom-file').setInputFiles(resolve(romPath));
        else {
            const rom = Buffer.alloc(32768, 1); rom.set([32, 254]); rom[32766] = 128; rom[32767] = 0;
            await page.locator('#jr8rom-file').setInputFiles({name: 'relicDive-bootstrap.rom', mimeType: 'application/octet-stream', buffer: rom});
        }
        await page.locator('#browser-calendar-startup').uncheck();
        await page.locator('#boot-basic').click(); await page.waitForTimeout(1200);
        await page.locator('#pause-basic').click();
        await page.locator('#hardware-program-file').setInputFiles(resolve(output, '07-relic-dive.j8a'));
        await page.locator('#load-program').click();
        assert.equal(await page.locator('#relic-dive-controls').count(), 0);
        await page.locator('#lcd-panel').click();
        const state = async () => (await page.evaluate(() => window.relicDiveSnapshot(0x4800, 64))).memory.bytes;
        async function until(condition) {
            const end = Date.now() + 15000;
            let s;
            while (Date.now() < end) { s = await state(); if (condition(s)) return s; await page.waitForTimeout(30); }
            throw new Error(`State timeout: ${JSON.stringify(s)}; ${await page.locator('#status').textContent()}`);
        }
        async function key(code) {
            await page.keyboard.press(code, {delay: 60});
            await page.waitForTimeout(230);
        }
        await until(s => s[5] === 0); await page.waitForTimeout(200);
        await page.locator('#lcd-panel-card').screenshot({path: resolve(output, 'browser-title.png')});
        await key('Numpad8'); assert.equal((await state())[6], 0);
        await key('Numpad2'); assert.equal((await state())[6], 1);
        await key('Numpad2'); assert.equal((await state())[6], 2);
        await key('Numpad8'); await key('Numpad8'); await key('Enter');
        await until(s => s[5] === 1); assert.equal((await state())[58], 5);
        const before = await state();
        await key('Enter'); await until(s => s[5] === 2);
        await key('Enter'); await until(s => s[5] === 3);
        await key('Space'); await until(s => s[5] === 2);
        await key('Space'); await until(s => s[5] === 1);
        assert.deepEqual((await state()).slice(6, 27), before.slice(6, 27), 'Menus ticked time');
        // Waiting is valid for every generated starting map.
        for (let turn = 1; turn <= 4; ++turn) {
            await key('Numpad5'); await until(s => s[24] === turn);
        }
        await page.locator('#lcd-panel-card').screenshot({path: resolve(output, 'browser-dungeon.png')});
        await page.locator('#machine-state-save').click();
        await page.waitForFunction(() => document.querySelector('#machine-state-status').textContent.includes('saved in this browser'));
        const saved = await state();
        const savedSnapshot = await page.evaluate(() => window.relicDiveSnapshot(0x4800, 64));
        await page.locator('#resume-machine').click();
        await page.locator('#lcd-panel').click(); await key('Numpad5');
        await page.evaluate(() => {
            const original = IDBObjectStore.prototype.put;
            IDBObjectStore.prototype.put = function(...args) {
                if (this.name === 'state') { IDBObjectStore.prototype.put = original; throw new Error('Injected save failure'); }
                return original.apply(this,args);
            };
        });
        await page.locator('#machine-state-save').click();
        await page.waitForFunction(() => document.querySelector('#machine-state-status').textContent.includes('previous saved state is unchanged'));
        await page.reload();
        await page.waitForFunction(() => !document.querySelector('#boot-basic').disabled
            && document.querySelector('#jr8rom-file').files.length === 1);
        await page.locator('#boot-basic').click(); await page.waitForTimeout(500);
        await page.locator('#pause-basic').click();
        const rtcBeforeRestore = (await page.evaluate(() => window.relicDiveSnapshot(0x4800,64))).state.calendarAlarmTerminal;
        await page.locator('#machine-state-restore').click();
        await page.waitForFunction(() => document.querySelector('#machine-state-status').textContent.includes('Machine state restored'));
        assert.deepEqual(await state(), saved);
        const restoredSnapshot = await page.evaluate(() => window.relicDiveSnapshot(0x4800, 64));
        const {calendarAlarmTerminal: restoredRtc, ...restoredCpu} = restoredSnapshot.state;
        const {calendarAlarmTerminal: savedRtc, ...savedCpu} = savedSnapshot.state;
        assert.equal(restoredRtc, rtcBeforeRestore, 'RTC was rolled back');
        assert.deepEqual(restoredCpu, savedCpu);
        await page.locator('#resume-machine').click();
        await page.locator('#lcd-panel').click();
        await key('Numpad5');
        assert.equal((await state())[24], saved[24]+1);
        await page.locator('#pause-basic').click();
        assert.deepEqual(errors, []);
        await writeFile(resolve(output, 'browser-verification.json'), JSON.stringify({passed: true,
            checks: ['three difficulties', 'keypad/Enter/Space', 'menus freeze turns', 'reload/resume', 'CPU/RAM restore across reload', 'failed replacement retains prior state'], errors}, null, 2) + '\n');
        console.log('RELIC DIVE Chrome input, reload and full machine-state checks passed');
    } finally { await browser.close(); }
})().catch(e => { console.error(e); process.exitCode = 1; });
