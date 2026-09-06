// SPDX-License-Identifier: MIT
// One machine checkpoint per site. ROM bytes are never stored in this record.
export function validateMachineState(value) {
    if (!(value instanceof Uint8Array) || value.length < 68 || value.length > 131072
        || value[0] !== 74 || value[1] !== 56 || value[2] !== 83 || value[3] !== 1)
        throw new Error("Invalid or incompatible saved machine state");
    return value;
}
async function access(pathname, value) {
    const db = await new Promise((resolve, reject) => {
        const request = indexedDB.open("jr800-machine-state", 1);
        let blocked = false;
        request.onupgradeneeded = () => request.result.createObjectStore("state");
        request.onerror = () => reject(request.error);
        request.onblocked = () => { blocked = true; reject(new Error("Machine state storage is blocked")); };
        request.onsuccess = () => blocked ? request.result.close() : resolve(request.result);
    });
    db.onversionchange = () => db.close();
    try {
        return await new Promise((resolve, reject) => {
            const transaction = db.transaction("state", value === undefined ? "readonly" : "readwrite");
            const store = transaction.objectStore("state");
            const key = pathname.replace(/\/index\.html$/, "/").replace(/\/$/, "") || "/";
            const request = value === undefined ? store.get(key) : store.put(value, key);
            transaction.oncomplete = () => resolve(request.result);
            transaction.onabort = () => reject(transaction.error ?? new Error("Machine state save aborted"));
            transaction.onerror = () => reject(transaction.error);
        });
    } finally { db.close(); }
}
export async function readMachineState(pathname) {
    const value = await access(pathname);
    return value === undefined ? null : validateMachineState(value);
}
export async function writeMachineState(pathname, value) {
    await access(pathname, validateMachineState(value));
}
