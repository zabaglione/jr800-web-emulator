// SPDX-License-Identifier: MIT
export function savedProgramFilename(nameBytes, extension) {
    if (!["j8a", "wav"].includes(extension)) throw new TypeError("Unsupported save extension");
    let name = nameBytes.map((code) => {
        if (code >= 32 && code < 127) return String.fromCharCode(code);
        if (code >= 0xa1 && code <= 0xdf) return String.fromCodePoint(0xff61 + code - 0xa1);
        return `_${code.toString(16).padStart(2, "0").toUpperCase()}`;
    }).join("").replace(/[<>:"/\\|?*\u0000-\u001f\u007f]/g, "_").replace(/^[. ]+|[. ]+$/g, "");
    if (!name) name = "PROGRAM";
    if (/^(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])(?:\.|$)/i.test(name)) name = `_${name}`;
    return `${name}.${extension}`;
}
