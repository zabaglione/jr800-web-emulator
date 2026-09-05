// SPDX-License-Identifier: MIT

import assert from "node:assert/strict";
import {pathToFileURL} from "node:url";

const [modulePath] = process.argv.slice(2);
if (!modulePath) {
    throw new Error("Usage: node lcd_panel_view_test.mjs <lcd-panel-view.mjs>");
}

const {lcdPanelImage} = await import(pathToFileURL(modulePath));
const dotCount = 192 * 64;

const unavailable = lcdPanelImage(null);
assert.equal(unavailable.width, 192);
assert.equal(unavailable.height, 64);
assert.equal(unavailable.rgba.length, dotCount * 4);
assert.equal(
    unavailable.summary,
    "LCD experiment is not enabled for this session",
);
assert.deepEqual(Array.from(unavailable.rgba.slice(0, 4)), [173, 163, 139, 255]);

const dots = new Uint8Array(dotCount);
dots[0] = 1;
dots[1] = 2;
const mixed = lcdPanelImage({width: 192, height: 64, dots});
assert.equal(mixed.summary, `1 on; 1 off; ${dotCount - 2} unknown`);
assert.deepEqual(Array.from(mixed.rgba.slice(0, 4)), [177, 181, 168, 255]);
assert.deepEqual(Array.from(mixed.rgba.slice(4, 8)), [77, 84, 79, 255]);
assert.deepEqual(Array.from(mixed.rgba.slice(8, 12)), [173, 163, 139, 255]);
const translated = lcdPanelImage(
    {width: 192, height: 64, dots},
    (source, values = {}) => `translated:${source.replace(
        /\{([a-zA-Z][a-zA-Z0-9]*)\}/g,
        (match, name) => Object.hasOwn(values, name) ? values[name] : match,
    )}`,
);
assert.match(translated.summary, /^translated:1 on;/);
assert.match(translated.ariaLabel, /^translated:Provisional/);

assert.throws(
    () => lcdPanelImage({width: 191, height: 64, dots}),
    /Invalid LCD panel snapshot/,
);
assert.throws(
    () => lcdPanelImage({width: 192, height: 64, dots: dots.slice(1)}),
    /Invalid LCD panel snapshot/,
);
const invalidDots = dots.slice();
invalidDots[0] = 3;
assert.throws(
    () => lcdPanelImage({width: 192, height: 64, dots: invalidDots}),
    /Invalid LCD dot state/,
);
