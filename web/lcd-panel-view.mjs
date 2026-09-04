// SPDX-License-Identifier: MIT

import {
    JR800_LCD_PANEL_DOT_COUNT,
    JR800_LCD_PANEL_HEIGHT,
    JR800_LCD_PANEL_WIDTH,
    Jr800LcdDotState,
} from "./wasm-machine.mjs";

const colors = Object.freeze({
    [Jr800LcdDotState.unknown]: Object.freeze([67, 57, 40, 255]),
    [Jr800LcdDotState.off]: Object.freeze([17, 42, 34, 255]),
    [Jr800LcdDotState.on]: Object.freeze([132, 235, 194, 255]),
});

export function lcdPanelImage(panel) {
    const available = panel !== null && panel !== undefined;
    const dots = panel?.dots
        ?? new Uint8Array(JR800_LCD_PANEL_DOT_COUNT);
    if (available && (
        panel.width !== JR800_LCD_PANEL_WIDTH
        || panel.height !== JR800_LCD_PANEL_HEIGHT
        || dots.length !== JR800_LCD_PANEL_DOT_COUNT
    )) {
        throw new RangeError("Invalid LCD panel snapshot");
    }

    const rgba = new Uint8ClampedArray(JR800_LCD_PANEL_DOT_COUNT * 4);
    const counts = {unknown: 0, off: 0, on: 0};
    for (let index = 0; index < dots.length; ++index) {
        const state = dots[index];
        const color = colors[state];
        if (color === undefined) {
            throw new RangeError("Invalid LCD dot state");
        }
        if (state === Jr800LcdDotState.unknown) {
            ++counts.unknown;
        } else if (state === Jr800LcdDotState.off) {
            ++counts.off;
        } else {
            ++counts.on;
        }
        rgba.set(color, index * 4);
    }

    const summary = available
        ? `${counts.on} on; ${counts.off} off; ${counts.unknown} unknown`
        : "LCD experiment is not enabled for this session";
    return {
        width: JR800_LCD_PANEL_WIDTH,
        height: JR800_LCD_PANEL_HEIGHT,
        rgba,
        summary,
        ariaLabel: `Provisional JR-800 LCD matrix. ${summary}`,
    };
}
