#!/usr/bin/env python3
# SPDX-License-Identifier: MIT

from __future__ import annotations

import argparse
from pathlib import Path
import re
import subprocess
import tempfile


SOURCE = """\
.global entry
.local loop
.extern external
.section .text, code
entry:
    NOP
    LDAA #$7F
    STAA $80
    BRA loop
loop:
    LDX #$1234
    .word external
    .byte $02, $CE, $12
.section .data, data
    .byte $41, $00, $7F, $FF
.section .bss, bss
    .space 4
"""

LINKED_SOURCE = """\
.global constant
.equ constant, $42
.global entry
.local loop
.section .text, code
entry:
    LDAA #$42
    STAA $80
loop:
    BRA loop
    .byte $02, $CE, $12
.section .bss, bss
    .space 2
"""

LINK_SCRIPT = """\
target hd6301v1
entry entry
region ZP $0000 $0100
region CODE $0200 $0100
place .text CODE
place .bss ZP
"""

ENTRY_INSIDE_SOURCE = """\
.global entry
.section .text, code
    .byte $86
entry:
    .byte $42, $01
"""


def run(
    command: list[str],
    cwd: Path | None = None,
    timeout: float | None = None,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        check=False,
        capture_output=True,
        text=True,
        cwd=cwd,
        timeout=timeout,
    )


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def assemble(assembler: Path, source: Path, output: Path, target: str) -> None:
    result = run(
        [
            str(assembler),
            "--target",
            target,
            "-o",
            str(output),
            source.name,
        ],
        cwd=source.parent,
    )
    require(result.returncode == 0, f"assembly failed: {result.stderr}")


def link(linker: Path, script: Path, object_path: Path, stem: Path) -> None:
    result = run(
        [
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
            str(object_path),
        ]
    )
    require(result.returncode == 0, f"link failed: {result.stderr}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--assembler", type=Path, required=True)
    parser.add_argument("--linker", type=Path, required=True)
    parser.add_argument("--tool", type=Path, required=True)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="jr8objdump-test-") as temporary:
        root = Path(temporary)
        source = root / "mixed.s"
        object_path = root / "mixed.jro"
        source.write_text(SOURCE, encoding="utf-8")
        assemble(args.assembler, source, object_path, "hd6301v1")

        first = run([str(args.tool), str(object_path)])
        second = run([str(args.tool), str(object_path)])
        require(first.returncode == 0, first.stderr)
        require(first.stderr == "", first.stderr)
        require(first.stdout == second.stdout, "output is nondeterministic")
        require(first.stdout.startswith("JRO 1.0 target=hd6301v1\n"), first.stdout)
        require(
            'SECTION 0 name=".text" type=PROGRAM_BITS attributes=AX '
            "placement=relocatable alignment=1 size=$0000000F\n"
            in first.stdout,
            first.stdout,
        )
        require(
            "$00000000\t01\tNOP\tdecoded\n" in first.stdout,
            "implied instruction is missing",
        )
        require(
            "$00000001\t86 7F\tLDAA #$7F\tdecoded\n" in first.stdout,
            "immediate instruction is missing",
        )
        require(
            "$00000003\t97 80\tSTAA $80\tdecoded\n" in first.stdout,
            "direct instruction is missing",
        )
        require(
            "$00000005\t20 00\tBRA $0007\tdecoded+relocation\n"
            in first.stdout,
            "relocated branch is not marked",
        )
        require(
            "$00000007\tCE 12 34\tLDX #$1234\tdecoded\n" in first.stdout,
            "word instruction is missing",
        )
        require(
            "$0000000A\t00\t.byte $00\tunknown-opcode+relocation\n"
            in first.stdout,
            "first byte of word relocation is not marked",
        )
        require(
            "$0000000B\t00\t.byte $00\tunknown-opcode+relocation\n"
            in first.stdout,
            "second byte of word relocation is not marked",
        )
        require(
            "$0000000C\t02\t.byte $02\tunknown-opcode\n" in first.stdout,
            "unknown opcode is not explicit",
        )
        require(
            "$0000000D\tCE\t.byte $CE\ttruncated-instruction\n" in first.stdout,
            "truncated instruction is not explicit",
        )
        require(
            "$0000000E\t12\t.byte $12\tunknown-opcode\n" in first.stdout,
            "byte after truncated instruction was lost",
        )
        require(
            'SECTION 1 name=".data" type=PROGRAM_BITS attributes=AW '
            "placement=relocatable alignment=1 size=$00000004\n"
            "CONTENTS\nOFFSET\tBYTES\n$00000000\t41 00 7F FF\n"
            in first.stdout,
            "data section contents differ",
        )
        require(
            'SECTION 2 name=".bss" type=NO_BITS attributes=AW '
            "placement=relocatable alignment=1 size=$00000004\n"
            in first.stdout,
            "BSS section header is missing",
        )
        require(first.stdout.count("DISASSEMBLY\n") == 0, "legacy heading appeared")
        require(
            first.stdout.count("DISASSEMBLY stored-byte-decode\n") == 1,
            "non-code section was disassembled",
        )
        require(
            first.stdout.count("CONTENTS\n") == 1,
            "BSS acquired contents or data contents are missing",
        )
        require(
            "RELOCATIONS 2\n"
            "INDEX\tSECTION\tOFFSET\tTYPE\tSYMBOL\tADDEND\n"
            '0\t0\t$00000006\tREL8\t"loop"\t0\n'
            '1\t0\t$0000000A\tABS16_BE\t"external"\t0\n'
            in first.stdout,
            "relocation table differs",
        )

        hostile = root / "hostile.jro"
        object_bytes = object_path.read_bytes()
        require(object_bytes.count(b".text") == 1, "section encoding differs")
        hostile.write_bytes(object_bytes.replace(b".text", b'e\t\n\x1b"'))
        hostile_result = run([str(args.tool), str(hostile)])
        require(hostile_result.returncode == 0, hostile_result.stderr)
        require(
            'name="e\\t\\n\\x1B\\\""' in hostile_result.stdout,
            "hostile section name was not escaped",
        )
        require(
            hostile_result.stdout.count("\n") == first.stdout.count("\n"),
            "hostile section name injected an output record",
        )

        hostile_symbol = root / "hostile-symbol.jro"
        require(object_bytes.count(b"external") == 1, "symbol encoding differs")
        hostile_symbol.write_bytes(
            object_bytes.replace(b"external", b'\xe2\x80\xaea"\\xy')
        )
        hostile_symbol_result = run([str(args.tool), str(hostile_symbol)])
        require(hostile_symbol_result.returncode == 0, hostile_symbol_result.stderr)
        require(
            '"\\xE2\\x80\\xAEa\\\"\\\\xy"' in hostile_symbol_result.stdout,
            "hostile symbol name was not byte-escaped",
        )
        require(
            hostile_symbol_result.stdout.count("\n") == first.stdout.count("\n"),
            "hostile symbol name injected an output record",
        )

        unknown_profile = root / "unknown-profile.jro"
        unknown_profile.write_bytes(object_bytes.replace(b"hd6301v1", b"othercpu"))
        unknown_profile_result = run([str(args.tool), str(unknown_profile)])
        require(unknown_profile_result.returncode == 1, "unknown profile was accepted")
        require(unknown_profile_result.stdout == "", "unknown profile emitted output")
        require(
            unknown_profile_result.stderr
            == "jr8objdump: unsupported target profile: othercpu\n",
            unknown_profile_result.stderr,
        )

        unresolved_object = root / "unresolved.jro"
        encoded_profile = b"\x00\x00\x00\x08hd6301v1"
        require(
            object_bytes.count(encoded_profile) == 1,
            "target profile encoding differs",
        )
        unresolved_name = b"jr800_unresolved"
        unresolved_object.write_bytes(
            object_bytes.replace(
                encoded_profile,
                len(unresolved_name).to_bytes(4, "big") + unresolved_name,
            )
        )
        unresolved = run([str(args.tool), str(unresolved_object)])
        require(unresolved.returncode == 1, "unresolved profile was accepted")
        require(unresolved.stdout == "", "unresolved profile emitted partial output")
        require(
            unresolved.stderr
            == "jr8objdump: unsupported target profile: jr800_unresolved\n",
            unresolved.stderr,
        )

        linked_source = root / "linked.s"
        linked_object = root / "linked.jro"
        link_script = root / "memory.j8l"
        linked_stem = root / "linked"
        linked_source.write_text(LINKED_SOURCE, encoding="utf-8")
        link_script.write_text(LINK_SCRIPT, encoding="utf-8")
        assemble(args.assembler, linked_source, linked_object, "hd6301v1")
        link(args.linker, link_script, linked_object, linked_stem)

        application_path = linked_stem.with_suffix(".j8a")
        application = run([str(args.tool), str(application_path)])
        repeated_application = run([str(args.tool), str(application_path)])
        require(application.returncode == 0, application.stderr)
        require(application.stderr == "", application.stderr)
        require(
            application.stdout == repeated_application.stdout,
            "JR8APP output is nondeterministic",
        )
        application_lines = application.stdout.splitlines()
        require(
            len(application_lines[0].split(" integrity=")) == 2
            and application_lines[0].startswith(
                "JR8APP 1.0 target=hd6301v1 entry=$0200 integrity="
            )
            and re.fullmatch(
                r"[0-9A-F]{64}",
                application_lines[0].split(" integrity=")[1],
            )
            is not None,
            application.stdout,
        )
        require(
            "SEGMENT 0 kind=ZERO_FILL address=$0000 size=$00000002 "
            "entry-offset=-\n"
            in application.stdout,
            "zero-fill segment differs",
        )
        require(
            "SEGMENT 1 kind=DATA address=$0200 size=$00000009 "
            "entry-offset=$00000000\n"
            in application.stdout,
            "data segment differs",
        )
        require(
            "DISASSEMBLY linear-stored-byte-decode\n"
            "ADDRESS\tBYTES\tTEXT\tSTATUS\n"
            "$0200\t86 42\tLDAA #$42\tdecoded+entry\n"
            "$0202\t97 80\tSTAA $80\tdecoded\n"
            "$0204\t20 FE\tBRA $0204\tdecoded\n"
            "$0206\t02\t.byte $02\tunknown-opcode\n"
            "$0207\tCE\t.byte $CE\ttruncated-instruction\n"
            "$0208\t12\t.byte $12\tunknown-opcode\n"
            in application.stdout,
            "linked-address disassembly differs",
        )
        require(
            application.stdout.count("DISASSEMBLY linear-stored-byte-decode\n")
            == 1,
            "zero-fill segment was disassembled",
        )

        debug_path = linked_stem.with_suffix(".j8d")
        annotated = run(
            [str(args.tool), "--debug", str(debug_path), str(application_path)]
        )
        repeated_annotated = run(
            [str(args.tool), "--debug", str(debug_path), str(application_path)]
        )
        require(annotated.returncode == 0, annotated.stderr)
        require(annotated.stderr == "", annotated.stderr)
        require(
            annotated.stdout == repeated_annotated.stdout,
            "annotated JR8APP output is nondeterministic",
        )
        require(
            "DEBUG JR8DBG 1.0 matched sources=1 symbols=3 lines=5\n"
            in annotated.stdout,
            annotated.stdout,
        )
        require(
            "ADDRESS\tBYTES\tTEXT\tSTATUS\tSYMBOLS\tSOURCE\n"
            "$0200\t86 42\tLDAA #$42\tdecoded+entry\t"
            '@+0:global:"entry"\t@+0:"linked.s":7:5\n'
            "$0202\t97 80\tSTAA $80\tdecoded\t-\t"
            '@+0:"linked.s":8:5\n'
            "$0204\t20 FE\tBRA $0204\tdecoded\t"
            '@+0:local:"loop"\t@+0:"linked.s":10:5\n'
            "$0206\t02\t.byte $02\tunknown-opcode\t-\t"
            '@+0:"linked.s":11:5\n'
            "$0207\tCE\t.byte $CE\ttruncated-instruction\t-\t"
            '@+0:"linked.s":11:5\n'
            "$0208\t12\t.byte $12\tunknown-opcode\t-\t"
            '@+0:"linked.s":11:5\n'
            in annotated.stdout,
            "JR8DBG symbol or source annotations differ",
        )
        require(
            'constant' not in annotated.stdout,
            "absolute symbol was presented as an address annotation",
        )

        malformed_debug = root / "malformed.j8d"
        malformed_debug.write_bytes(debug_path.read_bytes()[:-1])
        malformed_debug_result = run(
            [
                str(args.tool),
                "--debug",
                str(malformed_debug),
                str(application_path),
            ]
        )
        require(
            malformed_debug_result.returncode == 1,
            "truncated JR8DBG was accepted",
        )
        require(
            malformed_debug_result.stdout == "",
            "invalid JR8DBG emitted partial output",
        )
        require(
            "jr8objdump: invalid JR8DBG:" in malformed_debug_result.stderr
            and "at byte" in malformed_debug_result.stderr,
            malformed_debug_result.stderr,
        )

        missing_debug_file = run(
            [
                str(args.tool),
                "--debug",
                str(root / "does-not-exist.j8d"),
                str(application_path),
            ]
        )
        require(missing_debug_file.returncode == 2, "missing JR8DBG was accepted")
        require(missing_debug_file.stdout == "", "missing JR8DBG emitted output")
        require(
            "jr8objdump: cannot open input:" in missing_debug_file.stderr,
            missing_debug_file.stderr,
        )

        mismatched_target_debug = root / "mismatched-target.j8d"
        debug_bytes = debug_path.read_bytes()
        require(
            debug_bytes.count(encoded_profile) == 1,
            "JR8DBG target profile encoding differs",
        )
        replacement_profile = b"mc6801"
        mismatched_target_debug.write_bytes(
            debug_bytes.replace(
                encoded_profile,
                len(replacement_profile).to_bytes(4, "big")
                + replacement_profile,
            )
        )
        mismatched_target = run(
            [
                str(args.tool),
                "--debug",
                str(mismatched_target_debug),
                str(application_path),
            ]
        )
        require(mismatched_target.returncode == 1, "target mismatch was accepted")
        require(mismatched_target.stdout == "", "target mismatch emitted output")
        require(
            mismatched_target.stderr
            == "jr8objdump: JR8DBG target profile does not match JR8APP\n",
            mismatched_target.stderr,
        )

        hostile_debug = root / "hostile.j8d"
        require(debug_bytes.count(b"linked.s") == 1, "source path encoding differs")
        require(debug_bytes.count(b"entry") == 1, "debug symbol encoding differs")
        hostile_debug.write_bytes(
            debug_bytes.replace(b"linked.s", b'\xe2\x80\xaea"\\xy').replace(
                b"entry", b'e\t\n\x1b"'
            )
        )
        hostile_debug_result = run(
            [
                str(args.tool),
                "--debug",
                str(hostile_debug),
                str(application_path),
            ]
        )
        require(hostile_debug_result.returncode == 0, hostile_debug_result.stderr)
        require(
            '@+0:global:"e\\t\\n\\x1B\\\""' in hostile_debug_result.stdout,
            "hostile debug symbol was not escaped",
        )
        require(
            '"\\xE2\\x80\\xAEa\\\"\\\\xy":7:5'
            in hostile_debug_result.stdout,
            "hostile source path was not escaped",
        )
        require(
            hostile_debug_result.stdout.count("\n") == annotated.stdout.count("\n"),
            "hostile debug text injected an output record",
        )

        damaged_application = root / "damaged.j8a"
        damaged_bytes = bytearray(application_path.read_bytes())
        damaged_bytes[-1] ^= 0x01
        damaged_application.write_bytes(damaged_bytes)
        damaged = run([str(args.tool), str(damaged_application)])
        require(damaged.returncode == 1, "damaged JR8APP was accepted")
        require(damaged.stdout == "", "damaged JR8APP emitted partial output")
        require("jr8objdump: invalid JR8APP:" in damaged.stderr, damaged.stderr)
        require("integrity SHA-256 mismatch" in damaged.stderr, damaged.stderr)

        inside_source = root / "entry-inside.s"
        inside_object = root / "entry-inside.jro"
        inside_stem = root / "entry-inside"
        inside_source.write_text(ENTRY_INSIDE_SOURCE, encoding="utf-8")
        assemble(args.assembler, inside_source, inside_object, "hd6301v1")
        link(args.linker, link_script, inside_object, inside_stem)
        inside = run([str(args.tool), str(inside_stem.with_suffix(".j8a"))])
        require(inside.returncode == 0, inside.stderr)
        require(
            "SEGMENT 0 kind=DATA address=$0200 size=$00000003 "
            "entry-offset=$00000001\n"
            in inside.stdout,
            "entry-inside segment offset differs",
        )
        require(
            "$0200\t86 42\tLDAA #$42\tdecoded+entry-inside\n"
            in inside.stdout,
            "entry inside a decoded instruction was hidden",
        )

        annotated_inside = run(
            [
                str(args.tool),
                "--debug",
                str(inside_stem.with_suffix(".j8d")),
                str(inside_stem.with_suffix(".j8a")),
            ]
        )
        require(annotated_inside.returncode == 0, annotated_inside.stderr)
        require(
            "$0200\t86 42\tLDAA #$42\tdecoded+entry-inside\t"
            '@+1:global:"entry"\t'
            '@+0:"entry-inside.s":3:5,@+1:"entry-inside.s":5:5\n'
            in annotated_inside.stdout,
            "metadata beginning inside an instruction row was hidden",
        )

        mismatched_integrity = run(
            [
                str(args.tool),
                "--debug",
                str(debug_path),
                str(inside_stem.with_suffix(".j8a")),
            ]
        )
        require(
            mismatched_integrity.returncode == 1,
            "JR8DBG integrity mismatch was accepted",
        )
        require(
            mismatched_integrity.stdout == "",
            "JR8DBG integrity mismatch emitted output",
        )
        require(
            mismatched_integrity.stderr
            == "jr8objdump: JR8DBG application integrity does not match JR8APP\n",
            mismatched_integrity.stderr,
        )

        unsupported_path = root / "unsupported.bin"
        unsupported_path.write_bytes(b"not a supported format")
        unsupported = run([str(args.tool), str(unsupported_path)])
        require(unsupported.returncode == 1, "unknown format was accepted")
        require(unsupported.stdout == "", "unknown format emitted output")
        require(
            unsupported.stderr == "jr8objdump: unsupported input format\n",
            unsupported.stderr,
        )

        truncated = root / "truncated.jro"
        truncated.write_bytes(object_bytes[:-1])
        rejected = run([str(args.tool), str(truncated)])
        require(rejected.returncode == 1, "truncated JRO was accepted")
        require(rejected.stdout == "", "invalid JRO emitted partial output")
        require("jr8objdump: invalid JRO:" in rejected.stderr, rejected.stderr)
        require("at byte" in rejected.stderr, "invalid JRO omitted byte offset")

        missing = run([str(args.tool), str(root / "missing.jro")])
        require(missing.returncode == 2, "missing input exit status differs")
        require("jr8objdump: cannot open input:" in missing.stderr, missing.stderr)

        multiple = run([str(args.tool), str(object_path), str(object_path)])
        require(multiple.returncode == 2, "multiple inputs were accepted")
        require(
            "exactly one JRO or JR8APP input is required" in multiple.stderr,
            multiple.stderr,
        )

        unknown = run([str(args.tool), "--unknown"])
        require(unknown.returncode == 2, "unknown option was accepted")
        require("jr8objdump: unknown option: --unknown" in unknown.stderr, unknown.stderr)

        debug_with_object = run(
            [
                str(args.tool),
                "--debug",
                str(root / "does-not-exist.j8d"),
                str(object_path),
            ]
        )
        require(debug_with_object.returncode == 2, "JRO accepted --debug")
        require(debug_with_object.stdout == "", "JRO --debug emitted output")
        require(
            debug_with_object.stderr
            == "jr8objdump: --debug is only valid with JR8APP input\n",
            debug_with_object.stderr,
        )

        duplicate_debug = run(
            [
                str(args.tool),
                "--debug",
                str(debug_path),
                "--debug",
                str(debug_path),
                str(application_path),
            ]
        )
        require(duplicate_debug.returncode == 2, "duplicate --debug was accepted")
        require(
            "--debug may be specified only once" in duplicate_debug.stderr,
            duplicate_debug.stderr,
        )

        missing_debug_value = run([str(args.tool), "--debug"])
        require(missing_debug_value.returncode == 2, "missing --debug value passed")
        require(
            "missing value for --debug" in missing_debug_value.stderr,
            missing_debug_value.stderr,
        )

        option_as_debug_value = run(
            [str(args.tool), "--debug", "--unknown", str(application_path)]
        )
        require(
            option_as_debug_value.returncode == 2,
            "option-looking --debug value was accepted",
        )
        require(
            "missing value for --debug" in option_as_debug_value.stderr,
            option_as_debug_value.stderr,
        )

        annotation_count = 16_384
        large_source = root / "large.s"
        large_object = root / "large.jro"
        large_script = root / "large.j8l"
        large_stem = root / "large"
        large_lines = [".global entry", ".section .text, code", "entry:"]
        for index in range(annotation_count):
            large_lines.extend(
                [f"label_{index:05d}:", "    .byte $02"]
            )
        large_source.write_text("\n".join(large_lines) + "\n", encoding="utf-8")
        large_script.write_text(
            "target hd6301v1\n"
            "entry entry\n"
            "region CODE $0000 $4000\n"
            "place .text CODE\n",
            encoding="utf-8",
        )
        assemble(args.assembler, large_source, large_object, "hd6301v1")
        link(args.linker, large_script, large_object, large_stem)
        large = run(
            [
                str(args.tool),
                "--debug",
                str(large_stem.with_suffix(".j8d")),
                str(large_stem.with_suffix(".j8a")),
            ],
            timeout=15.0,
        )
        require(large.returncode == 0, large.stderr)
        require(large.stderr == "", large.stderr)
        require(
            large.stdout.count('@+0:local:"label_') == annotation_count,
            "large symbol index lost or repeated annotations",
        )
        require(
            large.stdout.count('@+0:"large.s":') == annotation_count,
            "large line index lost or repeated annotations",
        )
        require(
            '@+0:global:"entry",@+0:local:"label_00000"'
            in large.stdout,
            "first large annotation differs",
        )
        require(
            '@+0:local:"label_16383"' in large.stdout,
            "last large annotation differs",
        )

        help_result = run([str(args.tool), "--help"])
        require(help_result.returncode == 0, "--help failed")
        require(
            help_result.stdout
            == "Usage: jr8objdump [--debug <input.j8d>] "
            "<input.jro|input.j8a>\n",
            "help differs",
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
