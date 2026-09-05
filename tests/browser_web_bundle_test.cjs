// SPDX-License-Identifier: MIT
// Optional real-browser regression; requires Playwright and installed Chrome.
// Usage: node tests/browser_web_bundle_test.cjs build/wasm-release/web
const assert = require('node:assert/strict');
const {readFile, readdir} = require('node:fs/promises');
const {createServer} = require('node:http');
const path = require('node:path');
const {chromium} = require('playwright');

(async () => {
    const site = path.resolve(process.argv[2]);
    const index = await readFile(path.join(site, 'index.html'), 'utf8');
    const entry = index.match(/<script type="module" src="([^"]+)"/)[1];
    const prefix = path.posix.dirname(entry).slice(1) + '/';
    assert.match(prefix, /^\/assets\/[0-9a-f]{64}\/$/);
    const directory = path.join(site, prefix);
    const assets = new Map(await Promise.all((await readdir(directory)).map(async name =>
        [name, await readFile(path.join(directory, name))])));
    const flatIndex = index.replaceAll(`".${prefix}`, '"./');

    // A project-authored WASM fixture exports only the obsolete ABI number.
    const exportName = Buffer.from('jr800_machine_abi_version');
    const staleWasm = Buffer.from([
        0, 97, 115, 109, 1, 0, 0, 0,
        1, 5, 1, 0x60, 0, 1, 0x7f,
        3, 2, 1, 0,
        7, exportName.length + 4, 1, exportName.length, ...exportName, 0, 0,
        10, 6, 1, 4, 0, 0x41, 36, 0x0b,
    ]);
    assert.equal((await WebAssembly.instantiate(staleWasm)).instance.exports.jr800_machine_abi_version(), 36);
    const staleFactory = `export default async function () {
        const {instance} = await WebAssembly.instantiateStreaming(
            fetch(new URL('./jr800_wasm.wasm', import.meta.url)));
        return {cwrap: name => instance.exports[name] ?? (() => 0)};
    }`;

    let phase = 'stale';
    const requests = [];
    const types = {'.mjs': 'text/javascript', '.wasm': 'application/wasm',
        '.css': 'text/css', '.json': 'application/json'};
    const server = createServer((request, response) => {
        const pathname = new URL(request.url, 'http://localhost').pathname;
        requests.push({phase, pathname});
        if (pathname === '/' || pathname === '/index.html') {
            response.writeHead(200, {'Content-Type': 'text/html', 'Cache-Control': 'no-store'});
            response.end(phase === 'published' ? index : flatIndex);
            return;
        }
        const name = phase === 'published'
            ? (pathname.startsWith(prefix) ? pathname.slice(prefix.length) : '')
            : pathname.slice(1);
        let content = assets.get(name);
        if (phase === 'stale' && name === 'jr800_wasm.mjs') content = staleFactory;
        if (phase === 'stale' && name === 'jr800_wasm.wasm') content = staleWasm;
        if (content === undefined) {
            response.writeHead(404); response.end(); return;
        }
        response.writeHead(200, {'Content-Type': types[path.extname(name)],
            'Cache-Control': 'public, max-age=31536000, immutable'});
        response.end(content);
    });
    let browser;
    try {
        await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
        const url = `http://127.0.0.1:${server.address().port}/`;
        browser = await chromium.launch({channel: 'chrome', headless: true});
        const context = await browser.newContext();
        const expectOldAbi = async page => {
            await page.goto(url);
            await page.waitForFunction(() => document.querySelector('#status').dataset.tone === 'error');
            assert.match(await page.locator('#status').textContent(), /Unsupported WASM ABI version: 36/);
        };
        const warm = await context.newPage();
        await expectOldAbi(warm);
        await warm.close();

        // Even replacing the unversioned server files leaves a fresh cache stale.
        phase = 'flat-update';
        const page = await context.newPage();
        await expectOldAbi(page);
        assert.ok(!requests.some(item => item.phase === phase && item.pathname === '/jr800_wasm.wasm'));

        // An ordinary reload now loads the coherent revision, without clearing cache.
        phase = 'published';
        const pageErrors = [];
        page.on('pageerror', error => pageErrors.push(error.message));
        await page.reload();
        await page.waitForFunction(() => document.querySelector('#status').dataset.tone === 'ready');
        assert.match(await page.locator('#status').textContent(), /ABI 37/);
        assert.equal(await page.locator('#ignore-unsupported-io').isChecked(), true);
        const rom = Buffer.alloc(32768, 1);
        // LDAA #0; STAA unmapped $0300; BRA-to-self. No owner ROM is used.
        rom.set([0x86, 0, 0xb7, 3, 0, 0x20, 0xfe]);
        rom[32766] = 0x80; rom[32767] = 0;
        page.on('dialog', dialog => dialog.accept());
        await page.locator('#jr8rom-file').setInputFiles({
            name: 'synthetic-io.rom', mimeType: 'application/octet-stream', buffer: rom,
        });
        await page.locator('#boot-basic').click();
        await page.waitForFunction(() => Number(document.querySelector('#cycles').textContent) > 200000);
        assert.equal(await page.locator('#ignored-io-access-count').textContent(), '1');
        await page.locator('#power-off').click();
        assert.deepEqual(pageErrors, []);
        for (const name of ['app.mjs', 'jr800-worker.mjs', 'wasm-machine.mjs', 'jr800_wasm.mjs', 'jr800_wasm.wasm', 'locale-ja.json']) {
            assert.ok(requests.some(item => item.phase === 'published' && item.pathname === prefix + name), name);
        }
        console.log('Reproduced cached ABI 36; ordinary reload loaded ABI 37 and continued past synthetic I/O');
    } finally {
        if (browser) await browser.close();
        await new Promise(resolve => server.close(resolve));
    }
})().catch(error => {console.error(error); process.exitCode = 1;});
