// SPDX-License-Identifier: MIT
import assert from "node:assert/strict";
import {pathToFileURL} from "node:url";
const {savedProgramFilename} = await import(pathToFileURL(process.argv[2]));
const bytes = (text) => Array.from(text, (c) => c.charCodeAt(0));
assert.equal(savedProgramFilename(bytes("MY DATA"), "j8a"), "MY DATA.j8a");
assert.equal(savedProgramFilename(bytes("DATA"), "wav"), "DATA.wav");
assert.equal(savedProgramFilename(bytes("../A:B\\C"), "wav"), "_A_B_C.wav");
assert.equal(savedProgramFilename(bytes(".. "), "wav"), "PROGRAM.wav");
assert.equal(savedProgramFilename(bytes("CON"), "j8a"), "_CON.j8a");
assert.equal(savedProgramFilename([0xC1,0xBA,0xBF,0x8A], "j8a"), `${String.fromCodePoint(0xFF81,0xFF7A,0xFF7F)}_8A.j8a`);
assert.throws(() => savedProgramFilename([], "html"));
