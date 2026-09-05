// SPDX-License-Identifier: MIT

import assert from "node:assert/strict";
import {pathToFileURL} from "node:url";

const {readRomKeyboardGlyphs} = await import(pathToFileURL(process.argv[2]));
// Independent synthetic columns test orientation, stride and source ownership.
const memory = new Uint8Array(65536);
memory[0xf589 + (0x41 - 0x20) * 5 + 2] = 1;
memory[0xf769 + (0x96 - 0x80) * 6 + 5] = 0x80;
memory[0xf828 + (0xc1 - 0xa0) * 5 + 4] = 0x42;
const reads = [];
const read = (address, length) => {
    reads.push({address, length});
    return memory.slice(address, address + length);
};
const glyphs = readRomKeyboardGlyphs(read);
assert.equal(Object.keys(glyphs).length, 192);
assert.equal(glyphs[0x41], "M2 0h1v1h-1z");
assert.equal(glyphs[0x96], "M5 7h1v1h-1z");
assert.equal(glyphs[0xc1], "M4 1h1v1h-1zM4 6h1v1h-1z");
assert.equal(glyphs[0x20], "");
assert.equal(glyphs[0xe0], undefined);
assert.ok(reads.every(({address, length}) => address >= 0x8000 && address + length <= 0x10000));
// A replacement local ROM must supply its own glyphs, with no retained atlas.
memory.fill(0);
assert.equal(readRomKeyboardGlyphs(read)[0x41], "");
assert.equal(glyphs[0x41], "M2 0h1v1h-1z");
assert.throws(() => readRomKeyboardGlyphs(() => new Uint8Array(1)), /incomplete/);
