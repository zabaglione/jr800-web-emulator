#!/usr/bin/env python3
# SPDX-License-Identifier: MIT

import argparse
import re
from pathlib import Path


def require_version(text: str, pattern: str, source: str) -> int:
    match = re.search(pattern, text, re.MULTILINE)
    if match is None:
        raise AssertionError(f"ABI version was not found in {source}")
    return int(match.group(1))


def require_c_keyboard_keys(text: str) -> dict[str, int]:
    enum_match = re.search(
        r"typedef enum jr800_key \{(?P<body>.*?)\n\} jr800_key;",
        text,
        re.DOTALL,
    )
    if enum_match is None:
        raise AssertionError("jr800_key was not found in the C header")
    keys: dict[str, int] = {}
    for name, value in re.findall(
        r"^\s*JR800_KEY_([A-Z0-9_]+)\s*=\s*(\d+),?$",
        enum_match.group("body"),
        re.MULTILINE,
    ):
        keys[name.lower().replace("_", "-")] = int(value)
    return keys


def require_javascript_keyboard_keys(text: str) -> dict[str, int]:
    object_match = re.search(
        r"export const Jr800KeyboardKey = Object\.freeze\(\{"
        r"(?P<body>.*?)\n\}\);",
        text,
        re.DOTALL,
    )
    if object_match is None:
        raise AssertionError("Jr800KeyboardKey was not found in the adapter")
    keys: dict[str, int] = {}
    for quoted, bare, value in re.findall(
        r'^\s*(?:"([a-z0-9-]+)"|([a-z][a-z0-9-]*)):\s*(\d+),$',
        object_match.group("body"),
        re.MULTILINE,
    ):
        keys[quoted or bare] = int(value)
    return keys


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--header", required=True, type=Path)
    parser.add_argument("--adapter", required=True, type=Path)
    parser.add_argument("--document", required=True, type=Path)
    arguments = parser.parse_args()

    header = arguments.header.read_text(encoding="utf-8")
    adapter = arguments.adapter.read_text(encoding="utf-8")
    document = arguments.document.read_text(encoding="utf-8")

    versions = {
        "C header": require_version(
            header,
            r"^#define JR800_WASM_ABI_VERSION (\d+)U$",
            str(arguments.header),
        ),
        "JavaScript adapter": require_version(
            adapter,
            r"^const WASM_ABI_VERSION = (\d+);$",
            str(arguments.adapter),
        ),
        "document title": require_version(
            document,
            r"^# JR-800 WASM C ABI (\d+)$",
            str(arguments.document),
        ),
        "document versioning section": require_version(
            document,
            r"`abi_version` word both\s+return (\d+)\.",
            str(arguments.document),
        ),
    }

    expected = versions["C header"]
    mismatches = {
        source: version
        for source, version in versions.items()
        if version != expected
    }
    if mismatches:
        details = ", ".join(
            f"{source}={version}" for source, version in mismatches.items()
        )
        raise AssertionError(f"ABI versions differ from C header {expected}: {details}")

    c_keyboard_keys = require_c_keyboard_keys(header)
    javascript_keyboard_keys = require_javascript_keyboard_keys(adapter)
    if c_keyboard_keys != javascript_keyboard_keys:
        raise AssertionError(
            "C and JavaScript structured keyboard-key values differ"
        )
    if sorted(c_keyboard_keys.values()) != list(range(77)):
        raise AssertionError(
            "Structured keyboard-key values must cover contiguous range 0-76"
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
