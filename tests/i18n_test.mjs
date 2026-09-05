// SPDX-License-Identifier: MIT

import assert from "node:assert/strict";
import {readFile} from "node:fs/promises";
import {pathToFileURL} from "node:url";

const [modulePath, messagesPath] = process.argv.slice(2);
if (!modulePath || !messagesPath) {
    throw new Error("Usage: node i18n_test.mjs <i18n.mjs> <locale-ja.json>");
}

const {
    WebUiLanguage,
    createWebUiLocalizer,
    languageToggleLabel,
    preferredWebUiLanguage,
} = await import(pathToFileURL(modulePath));
const messages = JSON.parse(await readFile(messagesPath, "utf8"));

assert.equal(preferredWebUiLanguage(["ja-JP", "en-US"]), WebUiLanguage.japanese);
assert.equal(preferredWebUiLanguage(["fr-FR", "en-US"]), WebUiLanguage.english);
assert.equal(preferredWebUiLanguage(["fr-FR"]), WebUiLanguage.english);
assert.equal(preferredWebUiLanguage("ja"), WebUiLanguage.japanese);

const localizer = createWebUiLocalizer(messages, "ja-JP");
assert.equal(localizer.language, WebUiLanguage.japanese);
assert.equal(localizer.text("Boot BASIC experiment"), "BASIC\u306e\u8d77\u52d5");
assert.equal(
    localizer.text("Loaded {name}", {name: "sample.j8a"}),
    "sample.j8a\u3092\u8aad\u307f\u8fbc\u307f\u307e\u3057\u305f",
);
assert.equal(
    localizer.text(
        "Follow the documentation to convert the WAV file recorded from the physical machine to .j8r. Continue?",
    ),
    "\u30c9\u30ad\u30e5\u30e1\u30f3\u30c8\u306b\u5f93\u3063\u3066\u5b9f\u6a5f\u3067\u9332\u97f3\u3057\u305fWAV\u30d5\u30a1\u30a4\u30eb\u3092 .j8r \u306b\u5909\u63db\u3057\u3066\u304f\u3060\u3055\u3044\u3002\u51e6\u7406\u306f\u7d9a\u884c\u3057\u307e\u3059\u3002",
);
localizer.setLanguage("en-US");
assert.equal(localizer.text("Boot BASIC experiment"), "Boot BASIC experiment");
assert.equal(languageToggleLabel("en"), "\u65e5\u672c\u8a9e");
assert.equal(languageToggleLabel("ja"), "English");

const source = await readFile(modulePath, "utf8");
assert.doesNotMatch(source, /[\u3040-\u30ff\u3400-\u9fff]/u);
