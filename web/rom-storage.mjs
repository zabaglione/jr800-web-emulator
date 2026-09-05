// SPDX-License-Identifier: MIT

import {JR800_LOGICAL_ROM_BYTES, MAX_JR8ROM_BYTES} from "./wasm-machine.mjs";

const STORE = "rom";

// GitHub Pages project paths share an origin. Keep each site's selection separate.
export function romStorageKey(pathname) {
    return pathname.replace(/\/index\.html$/, "/").replace(/\/$/, "") || "/";
}

function validate(record) {
    if (record === undefined) return null;
    const {file, ignoreUnsupportedIo, browserCalendarStartup} = record;
    const name = file?.name?.toLowerCase();
    if (!(file instanceof File)
        || !(name.endsWith(".rom") && file.size === JR800_LOGICAL_ROM_BYTES
            || name.endsWith(".j8r") && file.size > 0 && file.size <= MAX_JR8ROM_BYTES)
        || typeof ignoreUnsupportedIo !== "boolean"
        || typeof browserCalendarStartup !== "boolean") {
        throw new Error("Invalid saved ROM record");
    }
    return record;
}

async function access(pathname, mode, operation) {
    const db = await new Promise((resolve, reject) => {
        let blocked = false;
        const request = indexedDB.open("jr800-rom", 1);
        request.onupgradeneeded = () => request.result.createObjectStore(STORE);
        request.onerror = () => reject(request.error);
        request.onblocked = () => {
            blocked = true;
            reject(new Error("ROM storage is blocked"));
        };
        request.onsuccess = () => {
            if (blocked) request.result.close();
            else resolve(request.result);
        };
    });
    db.onversionchange = () => db.close();
    try {
        return await new Promise((resolve, reject) => {
            const transaction = db.transaction(STORE, mode);
            const request = operation(transaction.objectStore(STORE), romStorageKey(pathname));
            transaction.oncomplete = () => resolve(request.result);
            transaction.onabort = () => reject(transaction.error ?? new Error("ROM storage was aborted"));
            transaction.onerror = () => reject(transaction.error);
        });
    } finally {
        db.close();
    }
}

export async function readSavedRom(pathname) {
    return validate(await access(pathname, "readonly", (store, key) => store.get(key)));
}

export async function writeSavedRom(pathname, record) {
    validate(record);
    await access(pathname, "readwrite", (store, key) => store.put(record, key));
}

export async function deleteSavedRom(pathname) {
    await access(pathname, "readwrite", (store, key) => store.delete(key));
}
