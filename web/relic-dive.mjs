// SPDX-License-Identifier: MIT
// Browser conveniences only: every game rule executes in the HD6301 program.
export const RELIC_DIVE_STATE_ADDRESS = 0x4800;
export const RELIC_DIVE_STATE_LENGTH = 57;
export function relicDiveHeader(bytes) {
    if (bytes.length !== RELIC_DIVE_STATE_LENGTH
        || bytes[0] !== 82 || bytes[1] !== 68 || bytes[2] !== 48 || bytes[3] !== 49) {
        throw new Error("This is not a compatible RELIC DIVE adventure (version 1 required)");
    }
    const readyAddress = bytes[55] * 256 + bytes[56];
    if (readyAddress < 0x2800 || readyAddress >= 0x4800) {
        throw new Error("Invalid RELIC DIVE checkpoint address");
    }
    return {suspended: bytes[4] === 1 && bytes[5] === 7, mode: bytes[5], readyAddress};
}

// Results are read-only observations of the machine program, never RAM writes.
export function relicDiveResult(bytes) {
    const {mode} = relicDiveHeader(bytes);
    if (![8, 9].includes(mode) || bytes[6] > 2) throw new Error("Finish an adventure before recording GOLD");
    return {difficulty: bytes[6], gold: bytes[21] * 256 + bytes[22], cleared: mode === 9};
}
export function recordRelicDiveGold(storage, pathname, result = null) {
    const key = "jr800-relic-dive-gold:" + (pathname.replace(/\/index\.html$/, "/").replace(/\/$/, "") || "/");
    const previous = JSON.parse(storage.getItem(key) ?? "[0,0,0]");
    if (!Array.isArray(previous) || previous.length !== 3 || previous.some(n => !Number.isInteger(n) || n < 0 || n > 65535)) {
        throw new Error("Invalid RELIC DIVE GOLD records");
    }
    if (result !== null) {
        previous[result.difficulty] = Math.max(previous[result.difficulty], result.gold);
        storage.setItem(key, JSON.stringify(previous));
    }
    return previous;
}
