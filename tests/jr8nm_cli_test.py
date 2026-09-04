#!/usr/bin/env python3
# SPDX-License-Identifier: MIT

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile


SOURCE = """\
.global entry
.local loop
.extern external
.global constant
.equ constant, $42
.section .text, code
entry:
    NOP
loop:
    BRA loop
"""

EXPECTED = """\
JRO 1.0 target=hd6301v1
INDEX\tBINDING\tDEFINITION\tVALUE\tSIZE\tSECTION\tNAME
0\tglobal\tsection\t$00000000\t0\t".text"\t"entry"
1\tlocal\tsection\t$00000001\t0\t".text"\t"loop"
2\tglobal\tundefined\t-\t0\t-\t"external"
3\tglobal\tabsolute\t$00000042\t0\t-\t"constant"
"""


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, check=False, capture_output=True, text=True)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--assembler", type=Path, required=True)
    parser.add_argument("--tool", type=Path, required=True)
    parser.add_argument("--debug-fixture", type=Path, required=True)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="jr8nm-test-") as temporary:
        root = Path(temporary)
        source = root / "symbols.s"
        object_path = root / "symbols.jro"
        source.write_text(SOURCE, encoding="utf-8")
        assembled = run(
            [
                str(args.assembler),
                "--target",
                "hd6301v1",
                "-o",
                str(object_path),
                str(source),
            ]
        )
        require(assembled.returncode == 0, f"assembly failed: {assembled.stderr}")

        first = run([str(args.tool), str(object_path)])
        second = run([str(args.tool), str(object_path)])
        require(first.returncode == 0, f"jr8nm failed: {first.stderr}")
        require(first.stderr == "", f"jr8nm wrote diagnostics: {first.stderr}")
        require(first.stdout == EXPECTED, f"jr8nm output differs:\n{first.stdout}")
        require(second.stdout == first.stdout, "jr8nm output is nondeterministic")

        escaped = root / "escaped.jro"
        object_bytes = object_path.read_bytes()
        require(object_bytes.count(b"entry") == 1, "entry symbol encoding differs")
        escaped.write_bytes(object_bytes.replace(b"entry", b'e\t\n\x1b"'))
        escaped_result = run([str(args.tool), str(escaped)])
        require(escaped_result.returncode == 0, escaped_result.stderr)
        require(
            '\t"e\\t\\n\\x1B\\\""\n' in escaped_result.stdout,
            f"unsafe symbol text was not escaped: {escaped_result.stdout!r}",
        )
        require(
            escaped_result.stdout.count("\n") == EXPECTED.count("\n"),
            "escaped symbol text injected an output record",
        )

        bidirectional = root / "bidirectional.jro"
        bidirectional.write_bytes(
            object_bytes.replace(b"entry", b'\xe2\x80\xaea"')
        )
        bidirectional_result = run([str(args.tool), str(bidirectional)])
        require(bidirectional_result.returncode == 0, bidirectional_result.stderr)
        require(
            '\t"\\xE2\\x80\\xAEa\\\""\n' in bidirectional_result.stdout,
            "non-ASCII display control was not byte-escaped",
        )

        debug_path = root / "write-watch.j8d"
        debug_path.write_bytes(
            bytes.fromhex(args.debug_fixture.read_text(encoding="ascii"))
        )
        debug_result = run([str(args.tool), str(debug_path)])
        require(debug_result.returncode == 0, debug_result.stderr)
        require(
            debug_result.stdout.startswith(
                "JR8DBG 1.0 target=hd6301v1\n"
                "INDEX\tBINDING\tKIND\tVALUE\tSIZE\tSOURCE\tNAME\n"
            ),
            f"JR8DBG header differs: {debug_result.stdout}",
        )
        require(
            '\tglobal\taddress\t$0200\t0\t"main.s"\t"entry"\n'
            in debug_result.stdout,
            "JR8DBG global address symbol is missing",
        )
        require(
            '\tlocal\taddress\t$020A\t0\t"main.s"\t"loop"\n'
            in debug_result.stdout,
            "JR8DBG local address symbol is missing",
        )

        absolute_debug = root / "absolute.j8d"
        absolute_bytes = bytearray(debug_path.read_bytes())
        loop_record_marker = b"\x00\x00\x00\x04loop"
        require(
            absolute_bytes.count(loop_record_marker) == 1,
            "JR8DBG loop record encoding differs",
        )
        loop_record = absolute_bytes.index(loop_record_marker) + len(loop_record_marker)
        require(
            absolute_bytes[loop_record : loop_record + 2] == b"\x01\x01",
            "JR8DBG loop binding or kind differs",
        )
        absolute_bytes[loop_record + 1] = 2
        absolute_bytes[loop_record + 10 : loop_record + 14] = b"\xff" * 4
        absolute_debug.write_bytes(absolute_bytes)
        absolute_result = run([str(args.tool), str(absolute_debug)])
        require(absolute_result.returncode == 0, absolute_result.stderr)
        require(
            '\tlocal\tabsolute\t$020A\t0\t-\t"loop"\n'
            in absolute_result.stdout,
            "JR8DBG absolute symbol without source is missing",
        )

        truncated_debug = root / "truncated.j8d"
        truncated_debug.write_bytes(debug_path.read_bytes()[:-1])
        rejected_debug = run([str(args.tool), str(truncated_debug)])
        require(rejected_debug.returncode == 1, "truncated JR8DBG was accepted")
        require(rejected_debug.stdout == "", "invalid JR8DBG emitted partial symbols")
        require("jr8nm: invalid JR8DBG:" in rejected_debug.stderr, rejected_debug.stderr)
        require("at byte" in rejected_debug.stderr, "invalid JR8DBG omitted byte offset")

        truncated = root / "truncated.jro"
        truncated.write_bytes(object_path.read_bytes()[:-1])
        rejected = run([str(args.tool), str(truncated)])
        require(rejected.returncode == 1, "truncated JRO was accepted")
        require(rejected.stdout == "", "invalid JRO emitted partial symbols")
        require("jr8nm: invalid JRO:" in rejected.stderr, rejected.stderr)
        require("at byte" in rejected.stderr, "invalid JRO omitted its byte offset")

        missing = run([str(args.tool), str(root / "missing.jro")])
        require(missing.returncode == 2, "missing input exit status differs")
        require("jr8nm: cannot open input:" in missing.stderr, missing.stderr)

        multiple = run([str(args.tool), str(object_path), str(object_path)])
        require(multiple.returncode == 2, "multiple inputs were accepted")
        require(
            "exactly one JRO or JR8DBG input is required" in multiple.stderr,
            multiple.stderr,
        )

        unknown = run([str(args.tool), "--unknown"])
        require(unknown.returncode == 2, "unknown option was accepted")
        require("jr8nm: unknown option: --unknown" in unknown.stderr, unknown.stderr)

        unknown_format = root / "unknown.bin"
        unknown_format.write_bytes(b"not a supported format")
        unknown_format_result = run([str(args.tool), str(unknown_format)])
        require(unknown_format_result.returncode == 1, "unknown format was accepted")
        require(unknown_format_result.stdout == "", "unknown format emitted output")
        require(
            unknown_format_result.stderr == "jr8nm: unsupported input format\n",
            unknown_format_result.stderr,
        )

        help_result = run([str(args.tool), "--help"])
        require(help_result.returncode == 0, "--help failed")
        require(
            help_result.stdout == "Usage: jr8nm <input.jro|input.j8d>\n",
            "help differs",
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
