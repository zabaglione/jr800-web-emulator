// SPDX-License-Identifier: MIT
// node tests/browser_expansion_ram_test.cjs build/wasm-release/web
// Requires Playwright and Chrome. JR800_GAME_ROM optionally checks owner-local BASIC.
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const path = require('node:path');
const {createServer} = require('node:http');
const {chromium} = require('playwright');

(async () => {
    const site = path.resolve(process.argv[2]);
    const output = path.resolve('build/expansion-ram-qa');
    await fs.mkdir(output, {recursive: true});
    const errors = [];
    const server = createServer(async (request, response) => {
        const pathname = new URL(request.url, 'http://localhost').pathname;
        const file = path.resolve(site, '.' + (pathname === '/' ? '/index.html' : pathname));
        try {
            assert.ok(file.startsWith(site + path.sep));
            const bytes = await fs.readFile(file);
            response.setHeader('Content-Type', {'.html': 'text/html', '.mjs': 'text/javascript',
                '.wasm': 'application/wasm', '.css': 'text/css', '.json': 'application/json'}[path.extname(file)]
                ?? 'application/octet-stream');
            response.end(bytes);
        } catch { response.writeHead(404); response.end(); }
    });
    let browser;
    try {
        await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
        const url = `http://127.0.0.1:${server.address().port}/`;
        browser = await chromium.launch({channel: 'chrome', headless: true});
        const page = await browser.newPage({locale: 'en-US'});
        page.on('pageerror', error => errors.push(error.message));
        page.on('dialog', dialog => dialog.accept());
        await page.goto(url);
        await page.waitForFunction(() => document.querySelector('#status').dataset.tone === 'ready');
        const expansion = page.locator('#expansion-ram-enabled');
        assert.equal(await expansion.isChecked(), false);
        assert.equal(await expansion.isEnabled(), true, 'Expansion is selectable before first boot');
        assert.equal(await page.locator('#standard-ram-enabled').isChecked(), true);
        await page.locator('#browser-calendar-startup').uncheck();
        await page.locator('#ignore-unsupported-io').uncheck();
        const rom = Buffer.alloc(32768, 1);
        // LDAA #$3C; STAA $6000/$7FFF/$5FFF; BRA-to-self. Original fixture.
        rom.set([0x86, 0x3c, 0xb7, 0x60, 0, 0xb7, 0x7f, 0xff, 0xb7, 0x5f, 0xff, 0x20, 0xfe]);
        rom[32766] = 0x80;
        rom[32767] = 0;
        await page.locator('#jr8rom-file').setInputFiles({
            name: 'expansion-probe.rom', mimeType: 'application/octet-stream', buffer: rom,
        });
        const boot = async (cycles = 200000) => {
            await page.locator('#boot-basic').click();
            await page.waitForFunction(limit =>
                document.querySelector('#status').textContent === 'BASIC running'
                && Number(document.querySelector('#cycles').textContent) > limit, cycles);
            await page.locator('#pause-basic').click();
            await page.waitForFunction(() => !document.querySelector('#boot-basic').disabled);
            assert.notEqual(await page.locator('#status').getAttribute('data-tone'), 'error');
        };
        const memory = async address => {
            await page.locator('#memory-address').fill(address);
            await page.locator('#memory-length').fill('2');
            await page.locator('#refresh-memory').click();
            await page.waitForFunction(value => document.querySelector('#memory').textContent.startsWith(value), address);
            return page.locator('#memory').textContent();
        };
        await boot();
        assert.equal(await expansion.isChecked(), false, 'BASIC must not attach expansion implicitly');
        await page.locator('#debugger-menu > summary').click();
        assert.match(await memory('$6000'), /\$6000\s+FF FF/);
        assert.match(await memory('$5FFF'), /\$5FFF\s+3C FF/);
        await page.locator('.configuration > summary').click();
        await expansion.check();
        await page.locator('#expansion-ram-value').fill('$5A');
        await boot();
        assert.equal(await expansion.isChecked(), true);
        assert.equal(await page.locator('#expansion-ram-value').inputValue(), '$5A');
        assert.match(await memory('$6000'), /\$6000\s+3C 5A/);
        assert.match(await memory('$7FFF'), /\$7FFF\s+3C 86/);
        await expansion.uncheck();
        await boot();
        assert.equal(await expansion.isChecked(), false);
        assert.match(await memory('$6000'), /\$6000\s+FF FF/);
        await page.goto(url + '?program=box-shift');
        await page.waitForFunction(() => document.querySelector('#status').dataset.tone === 'running'
            && Number(document.querySelector('#cycles').textContent) > 600000);
        assert.equal(await expansion.isChecked(), false, 'Catalog startup must use standard RAM');

        if (process.env.JR800_GAME_ROM) {
            await page.goto(url);
            await page.waitForFunction(() => document.querySelector('#status').dataset.tone === 'ready');
            await page.locator('#ignore-unsupported-io').check();
            await page.locator('#jr8rom-file').setInputFiles(process.env.JR800_GAME_ROM);
            await page.locator('.configuration > summary').click();
            let standardOutput;
            for (const enabled of [false, true]) {
                await expansion.setChecked(enabled);
                await boot(6000000);
                assert.equal(await expansion.isChecked(), enabled);
                await page.locator('#resume-machine').click();
                await page.locator('#lcd-panel').click();
                for (const key of ['KeyP', 'KeyR', 'KeyI', 'KeyN', 'KeyT', 'Space', 'Digit7', 'Enter']) {
                    await page.keyboard.press(key, {delay: 100});
                    await page.waitForTimeout(120);
                }
                await page.locator('#pause-basic').click();
                await page.waitForFunction(() => !document.querySelector('#boot-basic').disabled);
                const ink = await page.locator('#lcd-panel').evaluate(canvas => {
                    const pixels = canvas.getContext('2d').getImageData(0, 0, canvas.width, canvas.height).data;
                    let count = 0;
                    for (let index = 0; index < pixels.length; index += 4) if (pixels[index] < 120) count++;
                    return count;
                });
                assert.ok(ink > 100, 'BASIC must finish startup and render its prompt');
                const basicText = await page.locator('#lcd-panel').evaluate(canvas => {
                    // First six text rows include PRINT 7 and its result; exclude the blinking cursor row.
                    const pixels = canvas.getContext('2d').getImageData(0, 0, canvas.width, canvas.height).data;
                    return Array.from({length: 192 * 48}, (_, index) => {
                        const x = Math.floor((index % 192 + 0.5) * canvas.width / 192);
                        const y = Math.floor((Math.floor(index / 192) + 0.5) * canvas.height / 64);
                        return pixels[(y * canvas.width + x) * 4] < 120;
                    });
                });
                await page.locator('#lcd-panel').screenshot({
                    path: path.join(output, enabled ? 'basic-24kb.png' : 'basic-16kb.png'),
                });
                if (enabled) assert.ok(basicText.every((dot, index) => dot === standardOutput[index]),
                    'BASIC PRINT 7 result differs between RAM configurations');
                else standardOutput = basicText;
            }
        }
        assert.deepEqual(errors, []);
        console.log('PASS: default 16 KB, optional 8 KB, initial byte, disable again, catalog startup'
            + (process.env.JR800_GAME_ROM ? ', owner-local BASIC boot with both RAM options' : ''));
    } finally {
        await browser?.close();
        await new Promise(resolve => server.close(resolve));
    }
})().catch(error => { console.error(error); process.exitCode = 1; });
