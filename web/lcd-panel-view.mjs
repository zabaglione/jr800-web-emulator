// SPDX-License-Identifier: MIT

import {
    JR800_LCD_PANEL_DOT_COUNT,
    JR800_LCD_PANEL_HEIGHT,
    JR800_LCD_PANEL_WIDTH,
    Jr800LcdDotState,
} from "./wasm-machine.mjs";

export const Jr800LcdAppearanceDefaults = Object.freeze({
    contrast: 75,
    brightness: 60,
    tint: 35,
    dotGap: 18,
    inactiveDots: 12,
});

export function normalizeLcdAppearance(value) {
    const settings = {...Jr800LcdAppearanceDefaults};
    if (value === null || typeof value !== "object" || Array.isArray(value)) {
        return settings;
    }
    for (const key of Object.keys(settings)) {
        if (typeof value[key] === "number" && Number.isFinite(value[key])) {
            settings[key] = Math.round(Math.max(0,
                Math.min(key === "dotGap" ? 40 : 100, value[key])));
        }
    }
    return settings;
}

const mix = (first, second, amount) => first.map((channel, index) =>
    Math.round(channel + (second[index] - channel) * amount));

export function lcdPalette(appearance) {
    const settings = normalizeLcdAppearance(appearance);
    const light = 136 + settings.brightness * 0.82;
    const surface = mix([light, light + 3, light - 3],
        [light + 5, light + 9, light - 19], settings.tint / 100);
    const ink = mix([47, 57, 61], [52, 61, 42], settings.tint / 100);
    const density = 0.2 + settings.contrast * 0.008;
    return {
        surface,
        [Jr800LcdDotState.unknown]: [...mix(surface, [146, 114, 69], 0.35), 255],
        [Jr800LcdDotState.off]: [...mix(surface, ink, density * settings.inactiveDots * 0.0072), 255],
        [Jr800LcdDotState.on]: [...mix(surface, ink, density), 255],
    };
}

function defaultTranslate(source, values = {}) {
    return source.replace(/\{([a-zA-Z][a-zA-Z0-9]*)\}/g, (match, name) => (
        Object.hasOwn(values, name) ? String(values[name]) : match
    ));
}

export function lcdPanelImage(panel, translate = defaultTranslate, appearance) {
    const colors = lcdPalette(appearance);
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
        ? translate("{on} on; {off} off; {unknown} unknown", counts)
        : translate("LCD experiment is not enabled for this session");
    return {
        width: JR800_LCD_PANEL_WIDTH,
        height: JR800_LCD_PANEL_HEIGHT,
        rgba,
        summary,
        ariaLabel: translate(
            "Provisional JR-800 LCD matrix. {summary}",
            {summary},
        ),
    };
}

// Keep the logical 192 x 64 matrix separate from the visual dot spacing.
// A larger backing canvas keeps sub-dot gaps visible on high-density displays.
const DOT_SCALE = 8;
export class Jr800LcdPanelRenderer {
    constructor(canvas) {
        this.canvas = canvas;
        this.context = canvas.getContext("2d");
        this.dotsCanvas = canvas.ownerDocument.createElement("canvas");
        this.dotsCanvas.width = JR800_LCD_PANEL_WIDTH;
        this.dotsCanvas.height = JR800_LCD_PANEL_HEIGHT;
        this.dotsContext = this.dotsCanvas.getContext("2d");
        this.gridKey = null;
        this.gridPattern = null;
        canvas.width = JR800_LCD_PANEL_WIDTH * DOT_SCALE;
        canvas.height = JR800_LCD_PANEL_HEIGHT * DOT_SCALE;
    }

    draw(view, appearance) {
        if (this.context === null || this.dotsContext === null) return false;
        const settings = normalizeLcdAppearance(appearance);
        const surface = lcdPalette(settings).surface;
        const gridKey = `${surface.join(",")}:${settings.dotGap}`;
        if (gridKey !== this.gridKey) {
            const tile = this.canvas.ownerDocument.createElement("canvas");
            tile.width = DOT_SCALE;
            tile.height = DOT_SCALE;
            const context = tile.getContext("2d");
            const gap = settings.dotGap / 100 * DOT_SCALE;
            context.fillStyle = `rgb(${surface.join(",")})`;
            context.fillRect(0, 0, gap / 2, DOT_SCALE);
            context.fillRect(DOT_SCALE - gap / 2, 0, gap / 2, DOT_SCALE);
            context.fillRect(gap / 2, 0, DOT_SCALE - gap, gap / 2);
            context.fillRect(gap / 2, DOT_SCALE - gap / 2, DOT_SCALE - gap, gap / 2);
            this.gridPattern = this.context.createPattern(tile, "repeat");
            this.gridKey = gridKey;
        }
        const image = this.dotsContext.createImageData(view.width, view.height);
        image.data.set(view.rgba);
        this.dotsContext.putImageData(image, 0, 0);
        this.context.imageSmoothingEnabled = false;
        this.context.drawImage(this.dotsCanvas, 0, 0, this.canvas.width, this.canvas.height);
        this.context.fillStyle = this.gridPattern;
        this.context.fillRect(0, 0, this.canvas.width, this.canvas.height);
        return true;
    }
}
