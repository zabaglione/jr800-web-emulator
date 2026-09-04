#!/usr/bin/env python3
# SPDX-License-Identifier: MIT

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--snapshot", type=Path, required=True)
    parser.add_argument("--expected", type=Path, required=True)
    parser.add_argument("arguments", nargs="*")
    args = parser.parse_args()

    completed = subprocess.run(
        [str(args.snapshot), *args.arguments],
        check=False,
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        raise AssertionError(
            f"native snapshot failed:\n{completed.stderr}\n{completed.stdout}"
        )

    actual = json.loads(completed.stdout)
    expected = json.loads(args.expected.read_text(encoding="utf-8"))
    if actual != expected:
        raise AssertionError(
            "native snapshot differs from the shared parity fixture:\n"
            f"actual={json.dumps(actual, sort_keys=True)}\n"
            f"expected={json.dumps(expected, sort_keys=True)}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
