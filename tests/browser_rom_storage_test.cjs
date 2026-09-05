// SPDX-License-Identifier: MIT
// Optional browser test: node tests/browser_rom_storage_test.cjs build/wasm-release/web
// Requires Playwright and installed Chrome. Uses only a project-authored synthetic ROM.
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const {tmpdir} = require('node:os');
const path = require('node:path');
const {createServer} = require('node:http');
const {chromium} = require('playwright');

(async () => {
    const site = path.resolve(process.argv[2]);
    const profile = await fs.mkdtemp(path.join(tmpdir(), 'jr800-rom-storage-'));
    const requests = [], errors = [], dialogs = [];
    const server = createServer(async (request, response) => {
        const pathname = new URL(request.url, 'http://localhost').pathname;
        requests.push({pathname, method: request.method});
        const relative = pathname.replace(/^\/(jr800-test|another)\//, '');
        const file = path.resolve(site, relative || 'index.html');
        try {
            assert.ok(file.startsWith(site + path.sep));
            const bytes = await fs.readFile(file);
            response.setHeader('Content-Type', {'.html': 'text/html', '.mjs': 'text/javascript',
                '.wasm': 'application/wasm', '.css': 'text/css', '.json': 'application/json'}[path.extname(file)]);
            response.end(bytes);
        } catch { response.writeHead(404); response.end(); }
    });
    let context;
    try {
        await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
        const origin = `http://127.0.0.1:${server.address().port}`;
        const launch = () => chromium.launchPersistentContext(profile, {
            channel: 'chrome', headless: true, locale: 'en-US',
        });
        const open = async (suffix = '/jr800-test/') => {
            const page = await context.newPage();
            page.on('pageerror', error => errors.push(error.message));
            page.on('dialog', dialog => { dialogs.push(dialog.message()); return dialog.accept(); });
            await page.goto(origin + suffix);
            await page.waitForFunction(() => document.querySelector('#status').dataset.tone === 'ready');
            return page;
        };
        const boot = async page => {
            await page.locator('#boot-basic').click();
            await page.waitForFunction(() => Number(document.querySelector('#cycles').textContent) > 100000);
            await page.locator('#power-off').click();
            await page.waitForFunction(() => !document.querySelector('#boot-basic').disabled);
        };
        const rom = Buffer.alloc(32768, 1);
        rom.set([0x20, 0xfe]); // BRA-to-self, synthetic fixture only.
        rom[32766] = 0x80; rom[32767] = 0;
        context = await launch();
        let page = await open();
        await page.locator('#browser-calendar-startup').uncheck();
        await page.locator('#ignore-unsupported-io').uncheck();
        await page.locator('#jr8rom-file').setInputFiles({name: 'fixture.rom', mimeType: 'application/octet-stream', buffer: rom});
        await boot(page);
        assert.match(await page.locator('#saved-rom-status').textContent(), /Remembered ROM: fixture\.rom/);
        assert.match(await page.locator('#jr8rom-file').inputValue(), /fixture\.rom$/);
        assert.equal(dialogs.length, 1);
        await context.close(); context = await launch(); // Full browser restart, same disk profile.
        await context.addInitScript(() => {
            window.romReads = 0;
            const read = Blob.prototype.arrayBuffer;
            Blob.prototype.arrayBuffer = function () {
                ++window.romReads;
                return read.call(this);
            };
        });
        page = await open('/jr800-test/index.html');
        assert.match(await page.locator('#saved-rom-status').textContent(), /fixture\.rom/);
        assert.equal(await page.locator('#browser-calendar-startup').isChecked(), false);
        assert.equal(await page.locator('#ignore-unsupported-io').isChecked(), false);
        assert.equal(await page.locator('#boot-basic').isEnabled(), true);
        assert.equal(await page.locator('#resume-machine').isEnabled(), false);
        assert.equal(await page.locator('#cycles').textContent(), '0');
        assert.match(await page.locator('#jr8rom-file').inputValue(), /fixture\.rom$/);
        assert.deepEqual(await page.locator('#jr8rom-file').evaluate(input =>
            [...input.files].map(file => ({name: file.name, size: file.size}))),
            [{name: 'fixture.rom', size: rom.byteLength}]);
        assert.equal(await page.evaluate(() => window.romReads), 0);
        await boot(page); // No file chooser and no automatic boot on restoration.
        assert.equal(await page.evaluate(() => window.romReads), 1);
        assert.equal(dialogs.length, 1); // Restored RAW ROM was already approved.
        await page.locator('#jr8rom-file').setInputFiles({name: 'invalid.rom', mimeType: 'application/octet-stream', buffer: Buffer.of(1)});
        await page.locator('#boot-basic').click();
        await page.waitForFunction(() => document.querySelector('#status').dataset.tone === 'error');
        await page.reload();
        await page.waitForFunction(() => document.querySelector('#status').dataset.tone === 'ready');
        assert.match(await page.locator('#saved-rom-status').textContent(), /fixture\.rom/);
        assert.match(await page.locator('#jr8rom-file').inputValue(), /fixture\.rom$/);
        const other = await open('/another/');
        assert.match(await other.locator('#saved-rom-status').textContent(), /No ROM remembered/);
        await other.close();
        await page.locator('#jr8rom-file').setInputFiles({name: 'replacement.rom', mimeType: 'application/octet-stream', buffer: rom});
        await boot(page);
        await page.reload();
        await page.waitForFunction(() => document.querySelector('#status').dataset.tone === 'ready');
        assert.match(await page.locator('#jr8rom-file').inputValue(), /replacement\.rom$/);
        await boot(page);
        await page.locator('#forget-rom').click();
        await page.waitForFunction(() => document.querySelector('#saved-rom-status').textContent.includes('No ROM remembered'));
        assert.equal(await page.locator('#resume-machine').isEnabled(), true);
        assert.equal(await page.locator('#jr8rom-file').inputValue(), '');
        await page.reload();
        await page.waitForFunction(() => document.querySelector('#status').dataset.tone === 'ready');
        assert.match(await page.locator('#saved-rom-status').textContent(), /No ROM remembered/);
        await context.addInitScript(() => Object.defineProperty(window, 'indexedDB', {
            get() { throw new DOMException('Disabled for test', 'SecurityError'); },
        }));
        await page.reload();
        await page.waitForFunction(() => document.querySelector('#status').dataset.tone === 'ready');
        assert.match(await page.locator('#saved-rom-status').textContent(), /unavailable/);
        await page.locator('#browser-calendar-startup').uncheck();
        await page.locator('#jr8rom-file').setInputFiles({name: 'fixture.rom', mimeType: 'application/octet-stream', buffer: rom});
        await boot(page);
        assert.match(await page.locator('#saved-rom-status').textContent(), /could not be remembered/);
        assert.deepEqual(errors, []);
        assert.ok(requests.every(request => request.method === 'GET'));
        assert.ok(requests.every(request => !/\.(rom|j8r|wav|j8a)$/.test(request.pathname)));
        console.log('PASS: subpath assets, browser restart, explicit boot, preferences, invalid replacement, site isolation, deletion and unavailable storage');
    } finally {
        await context?.close();
        await new Promise(resolve => server.close(resolve));
        await fs.rm(profile, {recursive: true, force: true});
    }
})().catch(error => {console.error(error); process.exitCode = 1;});
