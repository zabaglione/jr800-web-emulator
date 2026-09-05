// SPDX-License-Identifier: MIT

// E-420: JR-HuBASIC 1.0/2.0 character layout. Only addresses and dimensions
// belong in source. Read the owner's loaded ROM through bus inspection.
const characterRanges = Object.freeze([
    {first: 0x20, count: 96, address: 0xf589, columns: 5},
    {first: 0x80, count: 32, address: 0xf769, columns: 6},
    {first: 0xa0, count: 64, address: 0xf828, columns: 5},
]);

export function readRomKeyboardGlyphs(readMemory) {
    const glyphs = {};
    for (const {first, count, address, columns} of characterRanges) {
        const bytes = readMemory(address, count * columns);
        if (!(bytes instanceof Uint8Array) || bytes.length !== count * columns) {
            throw new TypeError("ROM character data is incomplete");
        }
        for (let index = 0; index < count; ++index) {
            const dots = [];
            for (let x = 0; x < columns; ++x) {
                const column = bytes[index * columns + x];
                for (let y = 0; y < 8; ++y) {
                    if ((column & (1 << y)) !== 0) {
                        dots.push(`M${x} ${y}h1v1h-1z`);
                    }
                }
            }
            // Send drawing paths to the page, not a ROM or a font file.
            glyphs[first + index] = dots.join("");
        }
    }
    return glyphs;
}
