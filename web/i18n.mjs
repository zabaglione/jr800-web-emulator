// SPDX-License-Identifier: MIT

export const WebUiLanguage = Object.freeze({
    english: "en",
    japanese: "ja",
});

const translatableAttributes = Object.freeze([
    "aria-label",
    "placeholder",
    "title",
]);

function normalizeLanguage(value) {
    if (typeof value !== "string") {
        return null;
    }
    const primary = value.trim().toLowerCase().split(/[-_]/, 1)[0];
    return primary === WebUiLanguage.japanese
        ? WebUiLanguage.japanese
        : primary === WebUiLanguage.english
            ? WebUiLanguage.english
            : null;
}

export function preferredWebUiLanguage(languages) {
    const candidates = typeof languages === "string" ? [languages] : languages;
    if (candidates !== null && candidates !== undefined) {
        for (const candidate of candidates) {
            const language = normalizeLanguage(candidate);
            if (language !== null) {
                return language;
            }
        }
    }
    return WebUiLanguage.english;
}

export async function loadJapaneseMessages(url, fetchFunction = globalThis.fetch) {
    if (typeof fetchFunction !== "function") {
        throw new TypeError("A fetch function is required to load translations");
    }
    const response = await fetchFunction(url);
    if (!response.ok) {
        throw new Error(`Japanese translation request failed: ${response.status}`);
    }
    const messages = await response.json();
    if (messages === null || typeof messages !== "object" || Array.isArray(messages)) {
        throw new TypeError("Japanese translations must be an object");
    }
    for (const [source, translation] of Object.entries(messages)) {
        if (source.length === 0 || typeof translation !== "string"
            || translation.length === 0) {
            throw new TypeError("Japanese translations must map nonempty strings");
        }
    }
    return Object.freeze({...messages});
}

function formatMessage(template, values) {
    return template.replace(/\{([a-zA-Z][a-zA-Z0-9]*)\}/g, (match, name) => (
        Object.hasOwn(values, name) ? String(values[name]) : match
    ));
}

export function createWebUiLocalizer(japaneseMessages, initialLanguage) {
    let language = normalizeLanguage(initialLanguage) ?? WebUiLanguage.english;
    const textBindings = [];
    const attributeBindings = [];
    let boundDocument = null;

    function text(source, values = {}) {
        const template = language === WebUiLanguage.japanese
            ? japaneseMessages[source] ?? source
            : source;
        return formatMessage(template, values);
    }

    function collectDocumentBindings(documentObject) {
        const walker = documentObject.createTreeWalker(documentObject, 4);
        let node = walker.nextNode();
        while (node !== null) {
            const parentName = node.parentElement?.tagName?.toLowerCase();
            const source = node.nodeValue.trim();
            if (parentName !== "script" && parentName !== "style"
                && source.length !== 0
                && Object.hasOwn(japaneseMessages, source)) {
                const start = node.nodeValue.indexOf(source);
                textBindings.push({
                    node,
                    source,
                    prefix: node.nodeValue.slice(0, start),
                    suffix: node.nodeValue.slice(start + source.length),
                });
            }
            node = walker.nextNode();
        }

        for (const element of documentObject.querySelectorAll("*")) {
            for (const attribute of translatableAttributes) {
                const source = element.getAttribute(attribute);
                if (source !== null && Object.hasOwn(japaneseMessages, source)) {
                    attributeBindings.push({element, attribute, source});
                }
            }
        }
    }

    function applyBindings() {
        for (const binding of textBindings) {
            binding.node.nodeValue = binding.prefix
                + text(binding.source)
                + binding.suffix;
        }
        for (const binding of attributeBindings) {
            binding.element.setAttribute(binding.attribute, text(binding.source));
        }
        if (boundDocument !== null) {
            boundDocument.documentElement.lang = language;
        }
    }

    function bindDocument(documentObject) {
        if (boundDocument !== null) {
            throw new Error("The localizer is already bound to a document");
        }
        boundDocument = documentObject;
        collectDocumentBindings(documentObject);
        applyBindings();
    }

    function setLanguage(nextLanguage) {
        const normalized = normalizeLanguage(nextLanguage);
        if (normalized === null) {
            throw new RangeError(`Unsupported Web UI language: ${nextLanguage}`);
        }
        language = normalized;
        applyBindings();
    }

    return Object.freeze({
        bindDocument,
        get language() {
            return language;
        },
        setLanguage,
        text,
    });
}

export function languageToggleLabel(language) {
    return normalizeLanguage(language) === WebUiLanguage.japanese
        ? "English"
        : "\u65e5\u672c\u8a9e";
}
