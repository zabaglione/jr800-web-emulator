#!/usr/bin/env python3
# SPDX-License-Identifier: MIT

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile


MAIN_SOURCE = """\
.section .text, code
.global entry
.extern helper
entry:
    LDAA #$2A
    BSR helper
"""

LIBRARY_SOURCE = """\
.section .text, code
.global helper
helper:
    RTS
"""


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, check=False, capture_output=True, text=True)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--assembler", type=Path, required=True)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="jr8as-test-") as temporary:
        root = Path(temporary)
        main_source = root / "main.s"
        library_source = root / "lib.s"
        invalid_source = root / "invalid.s"
        main_source.write_text(MAIN_SOURCE, encoding="utf-8")
        library_source.write_text(LIBRARY_SOURCE, encoding="utf-8")
        invalid_source.write_text(
            ".section .text, code\nSTAA missing\n",
            encoding="utf-8",
        )

        first_object = root / "main-first.jro"
        second_object = root / "main-second.jro"
        listing = root / "main.lst"
        first = run(
            [
                str(args.assembler),
                "--target",
                "hd6301v1",
                "-o",
                str(first_object),
                "--listing",
                str(listing),
                str(main_source),
            ]
        )
        require(first.returncode == 0, f"first assembly failed: {first.stderr}")
        second = run(
            [
                str(args.assembler),
                "--target",
                "hd6301v1",
                "-o",
                str(second_object),
                str(main_source),
            ]
        )
        require(second.returncode == 0, f"second assembly failed: {second.stderr}")
        require(first_object.read_bytes() == second_object.read_bytes(), "JRO bytes differ")
        require(first_object.read_bytes().startswith(b"JRO\0"), "JRO magic is missing")
        listing_text = listing.read_text(encoding="utf-8")
        require("Target: hd6301v1" in listing_text, "listing target is missing")
        require("86 2A" in listing_text, "listing instruction bytes are missing")

        library_object = root / "lib.jro"
        library = run(
            [
                str(args.assembler),
                "--target",
                "hd6301v1",
                "-o",
                str(library_object),
                str(library_source),
            ]
        )
        require(library.returncode == 0, f"library assembly failed: {library.stderr}")
        require(library_object.read_bytes().startswith(b"JRO\0"), "library JRO is missing")

        invalid_object = root / "invalid.jro"
        invalid = run(
            [
                str(args.assembler),
                "--target",
                "hd6301v1",
                "-o",
                str(invalid_object),
                str(invalid_source),
            ]
        )
        require(invalid.returncode == 1, "invalid source exit status mismatch")
        require(
            f"{invalid_source}:2:6: error[E3307]" in invalid.stderr,
            f"diagnostic location mismatch: {invalid.stderr}",
        )
        require(not invalid_object.exists(), "failed assembly emitted an object")

        original_source = main_source.read_bytes()
        same_path = run(
            [
                str(args.assembler),
                "--target",
                "hd6301v1",
                "-o",
                str(main_source),
                str(main_source),
            ]
        )
        require(same_path.returncode == 2, "input/output collision was accepted")
        require(main_source.read_bytes() == original_source, "input/output collision changed source")

        shared_output = root / "shared-output"
        output_listing_collision = run(
            [
                str(args.assembler),
                "--target",
                "hd6301v1",
                "-o",
                str(shared_output),
                "--listing",
                str(shared_output),
                str(main_source),
            ]
        )
        require(
            output_listing_collision.returncode == 2,
            "object/listing collision was accepted",
        )
        require(not shared_output.exists(), "colliding output was created")

        source_alias = root / "main-alias.s"
        source_alias.symlink_to(main_source)
        alias_collision = run(
            [
                str(args.assembler),
                "--target",
                "hd6301v1",
                "-o",
                str(source_alias),
                str(main_source),
            ]
        )
        require(alias_collision.returncode == 2, "symlinked input/output collision was accepted")
        require(main_source.read_bytes() == original_source, "symlink collision changed source")

        dangling_target = root / "dangling-output.jro"
        dangling_alias = root / "dangling-output-alias"
        dangling_alias.symlink_to(dangling_target.name)
        dangling_collision = run(
            [
                str(args.assembler),
                "--target",
                "hd6301v1",
                "-o",
                str(dangling_target),
                "--listing",
                str(dangling_alias),
                str(main_source),
            ]
        )
        require(
            dangling_collision.returncode == 2,
            "dangling symlink output collision was accepted",
        )
        require(not dangling_target.exists(), "dangling symlink collision created output")

        case_collision = run(
            [
                str(args.assembler),
                "--target",
                "hd6301v1",
                "-o",
                str(root / "CaseOutput"),
                "--listing",
                str(root / "caseoutput"),
                str(main_source),
            ]
        )
        require(case_collision.returncode == 2, "case-folded output collision was accepted")
        require(not (root / "CaseOutput").exists(), "case-folded collision created output")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
