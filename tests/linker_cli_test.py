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
.global buffer
.extern helper
entry:
    LDAA #$2A
    STAA buffer
    BSR helper
.section .bss, bss
buffer:
    .space 1
"""

LIBRARY_SOURCE = """\
.section .text, code
.global helper
.extern buffer
helper:
    AIM #$F0, buffer
    RTS
"""

LINK_SCRIPT = """\
target hd6301v1
entry entry
region ZP $0000 $0100
region CODE $0200 $0100
place .text CODE
place .bss ZP
"""


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, check=False, capture_output=True, text=True)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def assemble(assembler: Path, source: Path, output: Path) -> None:
    result = run(
        [
            str(assembler),
            "--target",
            "hd6301v1",
            "-o",
            str(output),
            str(source),
        ]
    )
    require(result.returncode == 0, f"assembly failed: {result.stderr}")


def link_command(
    linker: Path,
    script: Path,
    stem: Path,
    inputs: list[Path],
) -> list[str]:
    return [
        str(linker),
        "--script",
        str(script),
        "-o",
        str(stem.with_suffix(".j8a")),
        "--debug",
        str(stem.with_suffix(".j8d")),
        "--map",
        str(stem.with_suffix(".map")),
        "--symbols",
        str(stem.with_suffix(".sym")),
        *[str(path) for path in inputs],
    ]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--assembler", type=Path, required=True)
    parser.add_argument("--linker", type=Path, required=True)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="jr8ld-test-") as temporary:
        root = Path(temporary)
        main_source = root / "main.s"
        library_source = root / "lib.s"
        script = root / "memory.j8l"
        main_object = root / "main.jro"
        library_object = root / "lib.jro"
        main_source.write_text(MAIN_SOURCE, encoding="utf-8")
        library_source.write_text(LIBRARY_SOURCE, encoding="utf-8")
        script.write_text(LINK_SCRIPT, encoding="utf-8")
        assemble(args.assembler, main_source, main_object)
        assemble(args.assembler, library_source, library_object)

        first_stem = root / "first"
        second_stem = root / "second"
        first = run(
            link_command(
                args.linker,
                script,
                first_stem,
                [main_object, library_object],
            )
        )
        require(first.returncode == 0, f"first link failed: {first.stderr}")
        second = run(
            link_command(
                args.linker,
                script,
                second_stem,
                [main_object, library_object],
            )
        )
        require(second.returncode == 0, f"second link failed: {second.stderr}")

        for suffix in (".j8a", ".j8d", ".map", ".sym"):
            first_bytes = first_stem.with_suffix(suffix).read_bytes()
            second_bytes = second_stem.with_suffix(suffix).read_bytes()
            require(first_bytes == second_bytes, f"nondeterministic output: {suffix}")
        require(
            first_stem.with_suffix(".j8a").read_bytes().startswith(b"JR8APP\0\0"),
            "JR8APP magic is missing",
        )
        require(
            first_stem.with_suffix(".j8d").read_bytes().startswith(b"JR8DBG\0\0"),
            "JR8DBG magic is missing",
        )
        require(
            "Entry: entry = $0200" in first_stem.with_suffix(".map").read_text(),
            "link map entry is missing",
        )
        require(
            "helper" in first_stem.with_suffix(".sym").read_text(),
            "symbol output is missing helper",
        )

        failed_stem = root / "failed"
        failed = run(
            link_command(args.linker, script, failed_stem, [main_object])
        )
        require(failed.returncode == 1, "undefined-symbol exit status mismatch")
        require("error[L2301]: undefined symbol: helper" in failed.stderr, failed.stderr)
        for suffix in (".j8a", ".j8d", ".map", ".sym"):
            require(
                not failed_stem.with_suffix(suffix).exists(),
                f"failed link emitted {suffix}",
            )

        object_alias = root / "main-alias.jro"
        object_alias.symlink_to(main_object)
        collision_command = link_command(
            args.linker,
            script,
            root / "collision",
            [main_object, library_object],
        )
        collision_command[collision_command.index("--map") + 1] = str(object_alias)
        original_object = main_object.read_bytes()
        collision = run(collision_command)
        require(collision.returncode == 2, "symlinked input/output collision was accepted")
        require(main_object.read_bytes() == original_object, "link collision changed input object")

        dangling_application = root / "dangling.j8a"
        dangling_debug = root / "dangling.j8d"
        dangling_debug.symlink_to(dangling_application.name)
        dangling_command = link_command(
            args.linker,
            script,
            root / "dangling",
            [main_object, library_object],
        )
        dangling_command[dangling_command.index("--debug") + 1] = str(dangling_debug)
        dangling = run(dangling_command)
        require(
            dangling.returncode == 2,
            "dangling symlink output collision was accepted",
        )
        require(not dangling_application.exists(), "dangling link collision created output")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
