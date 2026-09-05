// SPDX-License-Identifier: MIT

import {Jr800KeyboardKey} from "./wasm-machine.mjs";

export const Jr800VirtualLatchKeys = Object.freeze([
    "shift",
    "control",
]);

const latchKeys = new Set(Jr800VirtualLatchKeys);

function requireKey(key) {
    if (typeof key !== "string" || !Object.hasOwn(Jr800KeyboardKey, key)) {
        throw new TypeError("Virtual keyboard key is unsupported");
    }
    return key;
}

function requireSource(source) {
    if (typeof source !== "string" || source.length === 0) {
        throw new TypeError("Virtual keyboard source must be a nonempty string");
    }
    return source;
}

export function isJr800VirtualLatchKey(key) {
    return latchKeys.has(requireKey(key));
}

export class Jr800VirtualKeyboardState {
    #sources = new Map();
    #sourceCounts = new Map();
    #latched = new Set();

    press(source, key) {
        const checkedSource = requireSource(source);
        const checkedKey = requireKey(key);
        const existing = this.#sources.get(checkedSource);
        if (existing !== undefined) {
            if (existing !== checkedKey) {
                throw new Error("Virtual keyboard source is already active");
            }
            return null;
        }

        this.#sources.set(checkedSource, checkedKey);
        return this.#increment(checkedKey);
    }

    release(source) {
        const checkedSource = requireSource(source);
        const key = this.#sources.get(checkedSource);
        if (key === undefined) {
            return null;
        }
        this.#sources.delete(checkedSource);
        return this.#decrement(key);
    }

    toggleLatch(key) {
        const checkedKey = requireKey(key);
        if (!latchKeys.has(checkedKey)) {
            throw new TypeError("Virtual keyboard key cannot be latched");
        }

        if (this.#latched.delete(checkedKey)) {
            return this.#decrement(checkedKey);
        }
        this.#latched.add(checkedKey);
        return this.#increment(checkedKey);
    }

    isPressed(key) {
        const checkedKey = requireKey(key);
        return this.#sourceCounts.has(checkedKey);
    }

    isLatched(key) {
        const checkedKey = requireKey(key);
        return this.#latched.has(checkedKey);
    }

    releaseAll() {
        const releases = Array.from(this.#sourceCounts.keys())
            .sort((left, right) => (
                Jr800KeyboardKey[left] - Jr800KeyboardKey[right]
            ))
            .map((key) => ({key, pressed: false}));
        this.#sources.clear();
        this.#sourceCounts.clear();
        this.#latched.clear();
        return releases;
    }

    #increment(key) {
        const count = this.#sourceCounts.get(key) ?? 0;
        this.#sourceCounts.set(key, count + 1);
        return count === 0 ? {key, pressed: true} : null;
    }

    #decrement(key) {
        const count = this.#sourceCounts.get(key);
        if (count === 1) {
            this.#sourceCounts.delete(key);
            return {key, pressed: false};
        }
        this.#sourceCounts.set(key, count - 1);
        return null;
    }
}

// Host typing policy: do not turn overlapping ordinary presses into an
// unsupported physical matrix chord. Modifier behavior stays separate.
export class Jr800TypingRollover {
    #activeKey = null;

    transition({key, pressed}) {
        requireKey(key);
        if (typeof pressed !== "boolean") {
            throw new TypeError("Keyboard pressed state must be boolean");
        }
        if (latchKeys.has(key)) {
            return [{key, pressed}];
        }
        if (pressed) {
            if (this.#activeKey === key) {
                return [];
            }
            const previous = this.#activeKey;
            this.#activeKey = key;
            return [
                ...(previous === null ? [] : [{key: previous, pressed: false}]),
                {key, pressed: true},
            ];
        }
        if (this.#activeKey !== key) {
            return [];
        }
        this.#activeKey = null;
        return [{key, pressed: false}];
    }

    reset() {
        this.#activeKey = null;
    }
}
