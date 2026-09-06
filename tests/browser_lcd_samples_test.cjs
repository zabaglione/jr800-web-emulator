// SPDX-License-Identifier: MIT
// Optional browser acceptance: serve build/wasm-release/web first.
// Requires Playwright and installed Chrome. Manufacturer ROM is optional.
const assert = require('node:assert/strict');
const {mkdir, writeFile} = require('node:fs/promises');
const {resolve} = require('node:path');
const {chromium} = require('playwright');

const [url = 'http://127.0.0.1:8000/', output = 'build/sdk-lcd', romPath] = process.argv.slice(2);
const samples = ['01-hello', '02-checkerboard', '03-counter', '04-bounce', '05-keypad'];

async function spriteLeft(page) {
    return page.locator('#lcd-panel').evaluate(canvas => {
        const {width, height} = canvas;
        const pixels = canvas.getContext('2d').getImageData(0, 0, width, height).data;
        let left = width;
        for (let y = Math.ceil(height / 2); y < Math.floor(height * 40 / 64); y++) {
            for (let x = 0; x < width; x++) {
                const offset = (y * width + x) * 4;
                if (pixels[offset + 1] < 110) left = Math.min(left, x);
            }
        }
        if (left === width) throw new Error('Sprite not visible');
        return left / width * 192;
    });
}

(async () => {
    const browser = await chromium.launch({channel: 'chrome', headless: true});
    const errors = [];
    const results = [];
    try {
        const page = await browser.newPage({viewport: {width: 1440, height: 1000}});
        page.on('pageerror', error => errors.push(String(error)));
        page.on('dialog', dialog => dialog.accept());
        await page.goto(url);
        if (romPath) {
            await page.locator('#jr8rom-file').setInputFiles(resolve(romPath));
        } else {
            // Project-authored BRA-to-self bootstrap; no owner ROM or font.
            const rom = Buffer.alloc(32768, 1);
            rom.set([0x20, 0xfe]);
            rom[32766] = 0x80;
            rom[32767] = 0;
            await page.locator('#jr8rom-file').setInputFiles({
                name: 'sample-bootstrap.rom', mimeType: 'application/octet-stream', buffer: rom,
            });
        }
        await page.locator('#browser-calendar-startup').uncheck();
        await page.locator('#boot-basic').click();
        await page.waitForTimeout(1500);
        await page.locator('#pause-basic').click();
        for (const sample of samples) {
            const directory = resolve(output, sample);
            await page.locator('#hardware-program-file').setInputFiles(resolve(directory, `${sample}.j8a`));
            await page.locator('#load-program').click();
            await page.waitForTimeout(800);
            if (sample === '03-counter' || sample === '04-bounce') {
                const before = await page.locator('#lcd-panel').evaluate(canvas => canvas.toDataURL());
                await page.waitForTimeout(350);
                const after = await page.locator('#lcd-panel').evaluate(canvas => canvas.toDataURL());
                assert.notEqual(after, before, `${sample}: display did not animate`);
            }
            if (sample === '05-keypad') {
                const initial = await spriteLeft(page);
                for (const [keyName, sign] of [['keypad-4', -1], ['keypad-6', 1]]) {
                    const before = await spriteLeft(page);
                    const key = page.locator(`[data-jr800-key="${keyName}"]`);
                    await key.scrollIntoViewIfNeeded();
                    const box = await key.boundingBox();
                    await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
                    await page.mouse.down();
                    await page.waitForTimeout(500);
                    await page.mouse.up();
                    await page.waitForTimeout(250);
                    const after = await spriteLeft(page);
                    assert.ok((after - before) * sign > 0, `${keyName}: wrong movement`);
                    await page.waitForTimeout(300);
                    assert.equal(await spriteLeft(page), after, `${keyName}: release did not stop movement`);
                }
                assert.ok(initial >= 91 && initial <= 93, 'Cursor did not start at center');
            }
            await page.locator('#pause-basic').click();
            assert.notEqual(await page.locator('#status').getAttribute('data-tone'), 'error', sample);
            const summary = await page.locator('#lcd-summary').textContent();
            assert.match(summary, /(?:unknown|不明)\s*0|0 unknown/, 'Unknown LCD dots remain');
            await mkdir(directory, {recursive: true});
            // Only the sample LCD is captured; no ROM-backed keyboard glyphs.
            await page.locator('#lcd-panel-card').screenshot({path: resolve(directory, 'browser.png')});
            results.push({sample, passed: true});
        }
        assert.deepEqual(errors, []);
        await writeFile(resolve(output, 'browser-verification.json'),
            JSON.stringify({bootstrap: romPath ? 'owner-supplied' : 'project-authored', results, errors}, null, 2) + '\n');
        console.log(JSON.stringify({samples: results.length, passed: true, errors}));
    } finally {
        await browser.close();
    }
})().catch(error => {console.error(error); process.exitCode = 1;});
