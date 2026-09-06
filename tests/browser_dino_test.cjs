// SPDX-License-Identifier: MIT
// Serve the built emulator first. Requires Playwright and Chrome.
const assert = require('node:assert/strict');
const {readFile, writeFile} = require('node:fs/promises');
const {resolve} = require('node:path');
const {chromium} = require('playwright');

const [url = 'http://127.0.0.1:8000/', output = 'build/sdk-lcd/06-dino', romPath] = process.argv.slice(2);
(async () => {
    const symbolText = await readFile(resolve(output, '06-dino.sym'), 'utf8');
    const symbols = Object.fromEntries([...symbolText.matchAll(/^ \$([0-9A-F]+) G (\S+)/gm)]
        .map(([, address, name]) => [name, parseInt(address, 16)]));
    const browser = await chromium.launch({channel: 'chrome', headless: true});
    const errors = [];
    try {
        const page = await browser.newPage({viewport: {width: 1440, height: 1000}});
        page.on('pageerror', error => errors.push(String(error)));
        page.on('dialog', dialog => dialog.accept());
        // Observe the actual app Worker. Only read-only snapshots are sent here;
        // all gameplay input goes through real browser keyboard events.
        await page.addInitScript(() => {
            const NativeWorker = window.Worker;
            window.Worker = class extends NativeWorker {
                constructor(...args) { super(...args); window.dinoTestWorker = this; }
            };
            let sequence = 1_000_000;
            window.dinoTestSnapshot = (address, length) => new Promise((resolve, reject) => {
                const worker = window.dinoTestWorker;
                const id = ++sequence;
                const timer = setTimeout(() => { worker.removeEventListener('message', receive); reject(new Error('Snapshot timeout')); }, 5000);
                const receive = ({data}) => {
                    if (data.id !== id || data.type !== 'response') return;
                    clearTimeout(timer);
                    worker.removeEventListener('message', receive);
                    if (data.ok) resolve(data.result);
                    else reject(new Error(data.error));
                };
                worker.addEventListener('message', receive);
                worker.postMessage({id, command: 'snapshot', view: {memoryAddress: address, memoryLength: length}});
            });
        });
        await page.goto(url);
        if (romPath) await page.locator('#jr8rom-file').setInputFiles(resolve(romPath));
        else {
            const rom = Buffer.alloc(32768, 1);
            rom.set([0x20, 0xfe]); rom[32766] = 0x80; rom[32767] = 0;
            await page.locator('#jr8rom-file').setInputFiles({name: 'dino-bootstrap.rom', mimeType: 'application/octet-stream', buffer: rom});
        }
        await page.locator('#browser-calendar-startup').uncheck();
        await page.locator('#boot-basic').click();
        await page.waitForTimeout(1500);
        await page.locator('#pause-basic').click();
        await page.locator('#hardware-program-file').setInputFiles(resolve(output, '06-dino.j8a'));
        await page.locator('#load-program').click();
        await page.locator('#lcd-panel').click();

        async function state() {
            const snapshot = await page.evaluate(address => window.dinoTestSnapshot(address, 22), symbols.phase);
            const get = name => snapshot.memory.bytes[symbols[name] - symbols.phase];
            const number = name => snapshot.memory.bytes.slice(symbols[name] - symbols.phase, symbols[name] - symbols.phase + 3)
                .reduce((value, digit) => value * 10 + digit, 0);
            return {phase: get('phase'), height: get('height'), x: get('obstacle_x'),
                speed: get('speed'), gap: get('gap_wait'), score: number('distance'), best: number('high_score')};
        }
        async function until(condition, limit = 10_000) {
            const end = Date.now() + limit;
            let current;
            while (Date.now() < end) {
                current = await state();
                if (condition(current)) return current;
                await page.waitForTimeout(25);
            }
            throw new Error(`Game state timeout: ${JSON.stringify(current)}`);
        }
        await until(s => s.phase === 0);
        await page.waitForTimeout(250);
        await page.locator('#lcd-panel-card').screenshot({path: resolve(output, 'browser-ready.png')});
        await page.keyboard.down('Space');
        await until(s => s.phase === 1 && s.height >= 18);
        await page.locator('#lcd-panel-card').screenshot({path: resolve(output, 'browser-jump.png')});
        await until(s => s.phase === 1 && s.height === 0);
        await page.waitForTimeout(300);
        assert.equal((await state()).height, 0, 'Held SPACE repeated jump');
        // Continue holding into a collision: it must not trigger an automatic retry.
        const loss = await until(s => s.phase === 2);
        await page.waitForTimeout(350);
        assert.equal((await state()).phase, 2);
        await page.locator('#lcd-panel-card').screenshot({path: resolve(output, 'browser-game-over.png')});
        await page.keyboard.up('Space');
        await page.waitForTimeout(150);
        await page.keyboard.press('Space', {delay: 90});
        const restarted = await until(s => s.phase === 1);
        assert.equal(restarted.best, loss.score);

        // Clear three successive obstacles under the normal real-time UI scheduler.
        let cleared = 0;
        while (cleared < 3) {
            const approach = await until(s => s.phase === 2 ||
                (s.height === 0 && s.gap === 0 && s.x >= 40 && s.x <= 35 + s.speed * 5));
            assert.equal(approach.phase, 1, 'Collision before the jump window');
            await page.keyboard.press('Space', {delay: 90});
            const airborne = await until(s => s.phase === 2 || s.height >= 14);
            assert.equal(airborne.phase, 1, 'SPACE did not clear the cactus');
            const passed = await until(s => s.phase === 2 || s.x < 17);
            assert.equal(passed.phase, 1, 'Collision while passing the cactus');
            cleared++;
            await until(s => s.phase === 2 || s.x > 100);
        }
        assert.equal((await state()).phase, 1);
        await page.locator('#pause-basic').click();
        assert.notEqual(await page.locator('#status').getAttribute('data-tone'), 'error');
        assert.deepEqual(errors, []);
        const result = {passed: true, cleared, input: 'browser SPACE',
            bootstrap: romPath ? 'owner-supplied' : 'project-authored', errors};
        await writeFile(resolve(output, 'browser-verification.json'), JSON.stringify(result, null, 2) + '\n');
        console.log(JSON.stringify(result));
    } finally { await browser.close(); }
})().catch(error => {console.error(error); process.exitCode = 1;});
