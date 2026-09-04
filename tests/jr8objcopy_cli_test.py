#!/usr/bin/env python3
# SPDX-License-Identifier: MIT

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile


SOURCE = """\
.global entry
.section .text, code
entry:
    LDAA #$42
    STAA $80
    BRA entry
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

CONTIGUOUS_LINK_SCRIPT = """\
target hd6301v1
entry entry
region IMAGE $0200 $0100
place .text IMAGE
place .bss IMAGE
"""

EXPECTED_DATA = bytes([0x86, 0x42, 0x97, 0x80, 0x20, 0xFA])

SECTION_SOURCE = """\
.extern external
.section .payload, data
    .byte $00, $7F, $80, $FF
.section .pending, code
    .word external
.section .scratch, bss
    .space 3
"""

EXPECTED_SECTION = bytes([0x00, 0x7F, 0x80, 0xFF])


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, check=False, capture_output=True, text=True)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def assemble_source(
    assembler: Path, source: Path, object_path: Path
) -> None:
    assembled = run(
        [
            str(assembler),
            "--target",
            "hd6301v1",
            "-o",
            str(object_path),
            str(source),
        ]
    )
    require(assembled.returncode == 0, f"assembly failed: {assembled.stderr}")


def build_application(
    assembler: Path,
    linker: Path,
    root: Path,
    stem_name: str = "input",
    link_script: str = LINK_SCRIPT,
) -> tuple[Path, Path]:
    source = root / f"{stem_name}.s"
    object_path = root / f"{stem_name}.jro"
    script = root / f"{stem_name}.j8l"
    application = root / f"{stem_name}.j8a"
    source.write_text(SOURCE, encoding="utf-8")
    script.write_text(link_script, encoding="utf-8")

    assemble_source(assembler, source, object_path)

    stem = root / stem_name
    linked = run(
        [
            str(linker),
            "--script",
            str(script),
            "-o",
            str(application),
            "--debug",
            str(stem.with_suffix(".j8d")),
            "--map",
            str(stem.with_suffix(".map")),
            "--symbols",
            str(stem.with_suffix(".sym")),
            str(object_path),
        ]
    )
    require(linked.returncode == 0, f"link failed: {linked.stderr}")
    return application, object_path


def extract_command(
    tool: Path, application: Path, output: Path, index: str
) -> list[str]:
    return [
        str(tool),
        "--segment",
        index,
        "-o",
        str(output),
        str(application),
    ]


def extract_section_command(
    tool: Path, object_path: Path, output: Path, section: str
) -> list[str]:
    return [
        str(tool),
        "--section",
        section,
        "-o",
        str(output),
        str(object_path),
    ]


def extract_image_command(
    tool: Path,
    application: Path,
    output: Path,
    address: str,
    size: str,
) -> list[str]:
    return [
        str(tool),
        "--image",
        address,
        "--size",
        size,
        "-o",
        str(output),
        str(application),
    ]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--assembler", type=Path, required=True)
    parser.add_argument("--linker", type=Path, required=True)
    parser.add_argument("--tool", type=Path, required=True)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="jr8objcopy-test-") as temporary:
        root = Path(temporary)
        application, object_path = build_application(
            args.assembler, args.linker, root
        )
        contiguous_application, _ = build_application(
            args.assembler,
            args.linker,
            root,
            "contiguous",
            CONTIGUOUS_LINK_SCRIPT,
        )

        section_source = root / "sections.s"
        section_object = root / "sections.jro"
        section_source.write_text(SECTION_SOURCE, encoding="utf-8")
        assemble_source(args.assembler, section_source, section_object)

        first_output = root / "first.bin"
        second_output = root / "second.bin"
        first = run(extract_command(args.tool, application, first_output, "1"))
        second = run(extract_command(args.tool, application, second_output, "1"))
        require(first.returncode == 0, first.stderr)
        require(first.stdout == "" and first.stderr == "", "success was not silent")
        require(second.returncode == 0, second.stderr)
        require(first_output.read_bytes() == EXPECTED_DATA, "data bytes differ")
        require(
            second_output.read_bytes() == first_output.read_bytes(),
            "extraction is nondeterministic",
        )

        first_section_output = root / "first-section.bin"
        second_section_output = root / "second-section.bin"
        first_section = run(
            extract_section_command(
                args.tool, section_object, first_section_output, ".payload"
            )
        )
        second_section = run(
            extract_section_command(
                args.tool, section_object, second_section_output, ".payload"
            )
        )
        require(first_section.returncode == 0, first_section.stderr)
        require(
            first_section.stdout == "" and first_section.stderr == "",
            "section success was not silent",
        )
        require(second_section.returncode == 0, second_section.stderr)
        require(
            first_section_output.read_bytes() == EXPECTED_SECTION,
            "section bytes differ",
        )
        require(
            second_section_output.read_bytes() == first_section_output.read_bytes(),
            "section extraction is nondeterministic",
        )

        image_output = root / "image.bin"
        repeated_image_output = root / "repeated-image.bin"
        image = run(
            extract_image_command(
                args.tool,
                contiguous_application,
                image_output,
                "0x0200",
                "8",
            )
        )
        repeated_image = run(
            extract_image_command(
                args.tool,
                contiguous_application,
                repeated_image_output,
                "512",
                "0x8",
            )
        )
        require(image.returncode == 0, image.stderr)
        require(image.stdout == "" and image.stderr == "", "image success was not silent")
        require(repeated_image.returncode == 0, repeated_image.stderr)
        require(
            image_output.read_bytes() == EXPECTED_DATA + bytes(2),
            "contiguous image bytes differ",
        )
        require(
            repeated_image_output.read_bytes() == image_output.read_bytes(),
            "image extraction is nondeterministic",
        )

        boundary_output = root / "boundary.bin"
        boundary = run(
            extract_image_command(
                args.tool,
                contiguous_application,
                boundary_output,
                "0X0204",
                "4",
            )
        )
        require(boundary.returncode == 0, boundary.stderr)
        require(
            boundary_output.read_bytes() == bytes([0x20, 0xFA, 0x00, 0x00]),
            "DATA/ZERO_FILL boundary image differs",
        )

        zero_image_output = root / "zero-image.bin"
        zero_image = run(
            extract_image_command(
                args.tool, application, zero_image_output, "0", "2"
            )
        )
        require(zero_image.returncode == 0, zero_image.stderr)
        require(
            zero_image_output.read_bytes() == bytes(2),
            "explicit image did not materialize ZERO_FILL",
        )

        protected_output = root / "protected.bin"
        protected_output.write_bytes(b"preserve-me")
        zero_fill = run(
            extract_command(args.tool, application, protected_output, "0")
        )
        require(zero_fill.returncode == 1, "ZERO_FILL was materialized")
        require(zero_fill.stdout == "", "ZERO_FILL rejection emitted stdout")
        require(
            zero_fill.stderr
            == "jr8objcopy: segment 0 is ZERO_FILL and has no stored bytes\n",
            zero_fill.stderr,
        )
        require(
            protected_output.read_bytes() == b"preserve-me",
            "ZERO_FILL rejection modified output",
        )

        out_of_range = run(
            extract_command(args.tool, application, protected_output, "2")
        )
        require(out_of_range.returncode == 1, "out-of-range segment was accepted")
        require(out_of_range.stdout == "", "range rejection emitted stdout")
        require(
            out_of_range.stderr
            == "jr8objcopy: segment index is out of range: 2\n",
            out_of_range.stderr,
        )
        require(
            protected_output.read_bytes() == b"preserve-me",
            "range rejection modified output",
        )

        unloaded_image = run(
            extract_image_command(
                args.tool,
                application,
                protected_output,
                "0",
                "0x201",
            )
        )
        require(unloaded_image.returncode == 1, "unloaded gap was filled")
        require(unloaded_image.stdout == "", "gap rejection emitted stdout")
        require(
            unloaded_image.stderr
            == "jr8objcopy: image range includes unloaded addresses\n",
            unloaded_image.stderr,
        )
        require(
            protected_output.read_bytes() == b"preserve-me",
            "gap rejection modified output",
        )

        wrong_case_section = run(
            extract_section_command(
                args.tool, section_object, protected_output, ".PAYLOAD"
            )
        )
        require(
            wrong_case_section.returncode == 1,
            "case-mismatched section was accepted",
        )
        require(
            wrong_case_section.stdout == "",
            "missing section emitted stdout",
        )
        require(
            wrong_case_section.stderr == "jr8objcopy: section was not found\n",
            wrong_case_section.stderr,
        )
        require(
            protected_output.read_bytes() == b"preserve-me",
            "missing section modified output",
        )

        no_bits = run(
            extract_section_command(
                args.tool, section_object, protected_output, ".scratch"
            )
        )
        require(no_bits.returncode == 1, "NO_BITS section was materialized")
        require(no_bits.stdout == "", "NO_BITS rejection emitted stdout")
        require(
            no_bits.stderr
            == "jr8objcopy: selected section is NO_BITS and has no stored bytes\n",
            no_bits.stderr,
        )
        require(
            protected_output.read_bytes() == b"preserve-me",
            "NO_BITS rejection modified output",
        )

        unresolved_section = run(
            extract_section_command(
                args.tool, section_object, protected_output, ".pending"
            )
        )
        require(
            unresolved_section.returncode == 1,
            "section with relocation was extracted",
        )
        require(
            unresolved_section.stdout == "",
            "relocation rejection emitted stdout",
        )
        require(
            unresolved_section.stderr
            == "jr8objcopy: selected section has unresolved relocations\n",
            unresolved_section.stderr,
        )
        require(
            protected_output.read_bytes() == b"preserve-me",
            "relocation rejection modified output",
        )

        damaged_application = root / "damaged.j8a"
        damaged_bytes = bytearray(application.read_bytes())
        damaged_bytes[-1] ^= 0x01
        damaged_application.write_bytes(damaged_bytes)
        damaged = run(
            extract_command(args.tool, damaged_application, protected_output, "1")
        )
        require(damaged.returncode == 1, "damaged JR8APP was accepted")
        require(damaged.stdout == "", "damaged JR8APP emitted stdout")
        require("jr8objcopy: invalid JR8APP:" in damaged.stderr, damaged.stderr)
        require("integrity SHA-256 mismatch" in damaged.stderr, damaged.stderr)
        require(
            protected_output.read_bytes() == b"preserve-me",
            "damaged input modified output",
        )

        damaged_object = root / "damaged.jro"
        damaged_object.write_bytes(section_object.read_bytes()[:-1])
        damaged_section = run(
            extract_section_command(
                args.tool, damaged_object, protected_output, ".payload"
            )
        )
        require(damaged_section.returncode == 1, "damaged JRO was accepted")
        require(damaged_section.stdout == "", "damaged JRO emitted stdout")
        require(
            "jr8objcopy: invalid JRO:" in damaged_section.stderr,
            damaged_section.stderr,
        )
        require("at byte" in damaged_section.stderr, damaged_section.stderr)
        require(
            protected_output.read_bytes() == b"preserve-me",
            "damaged JRO modified output",
        )

        wrong_format = run(
            extract_command(args.tool, object_path, protected_output, "0")
        )
        require(wrong_format.returncode == 1, "JRO was accepted as JR8APP")
        require(wrong_format.stdout == "", "wrong format emitted stdout")
        require(
            "invalid JR8APP: invalid JR8APP magic" in wrong_format.stderr,
            wrong_format.stderr,
        )
        require(
            protected_output.read_bytes() == b"preserve-me",
            "wrong format modified output",
        )

        application_as_object = run(
            extract_section_command(
                args.tool, application, protected_output, ".payload"
            )
        )
        require(
            application_as_object.returncode == 1,
            "JR8APP was accepted as JRO",
        )
        require(
            application_as_object.stdout == "",
            "JR8APP section mode emitted stdout",
        )
        require(
            "invalid JRO: invalid JRO magic" in application_as_object.stderr,
            application_as_object.stderr,
        )
        require(
            protected_output.read_bytes() == b"preserve-me",
            "JR8APP section mode modified output",
        )

        same_path = run(
            extract_command(args.tool, application, application, "1")
        )
        require(same_path.returncode == 2, "same input/output path was accepted")
        require(
            "input and output paths must be distinct" in same_path.stderr,
            same_path.stderr,
        )
        require(
            application.read_bytes() != EXPECTED_DATA,
            "same-path rejection replaced the application",
        )

        alias = root / "application-alias"
        alias.symlink_to(application)
        aliased_output = run(extract_command(args.tool, application, alias, "1"))
        require(aliased_output.returncode == 2, "symlink alias was accepted")
        require(
            "input and output paths must be distinct" in aliased_output.stderr,
            aliased_output.stderr,
        )

        missing_input = run(
            extract_command(
                args.tool,
                root / "missing.j8a",
                root / "unused.bin",
                "0",
            )
        )
        require(missing_input.returncode == 2, "missing input status differs")
        require(
            "jr8objcopy: cannot open input:" in missing_input.stderr,
            missing_input.stderr,
        )
        require(not (root / "unused.bin").exists(), "missing input created output")

        bad_output = run(
            extract_command(
                args.tool,
                application,
                root / "missing-parent" / "output.bin",
                "1",
            )
        )
        require(bad_output.returncode == 2, "unwritable output status differs")
        require("cannot open output:" in bad_output.stderr, bad_output.stderr)

        invalid_indexes = ("-1", "+1", "1x", "4294967296")
        for invalid_index in invalid_indexes:
            result = run(
                extract_command(
                    args.tool,
                    application,
                    root / f"invalid-{invalid_index.replace('+', 'p')}.bin",
                    invalid_index,
                )
            )
            require(result.returncode == 2, f"accepted index {invalid_index}")
            require(not result.stdout, f"index {invalid_index} emitted stdout")
            require(
                "invalid segment index:" in result.stderr
                or "missing value for --segment" in result.stderr,
                result.stderr,
            )

        invalid_image_values = (
            ("0x10000", "1", "invalid image address:"),
            ("0xGG", "1", "invalid image address:"),
            ("0", "0", "invalid image size:"),
            ("0", "0x10001", "invalid image size:"),
            ("0xFFFF", "2", "image range exceeds the 16-bit address space"),
        )
        for address, size, diagnostic in invalid_image_values:
            invalid_output = root / f"invalid-image-{len(address)}-{len(size)}.bin"
            result = run(
                extract_image_command(
                    args.tool,
                    application,
                    invalid_output,
                    address,
                    size,
                )
            )
            require(result.returncode == 2, f"accepted image {address}:{size}")
            require(result.stdout == "", "invalid image emitted stdout")
            require(diagnostic in result.stderr, result.stderr)
            require(not invalid_output.exists(), "invalid image created output")

        duplicate_segment = run(
            [
                str(args.tool),
                "--segment",
                "1",
                "--segment",
                "1",
                "-o",
                str(root / "duplicate.bin"),
                str(application),
            ]
        )
        require(duplicate_segment.returncode == 2, "duplicate segment was accepted")
        require(
            "--segment may be specified only once" in duplicate_segment.stderr,
            duplicate_segment.stderr,
        )

        duplicate_output = run(
            [
                str(args.tool),
                "--segment",
                "1",
                "-o",
                str(root / "a.bin"),
                "-o",
                str(root / "b.bin"),
                str(application),
            ]
        )
        require(duplicate_output.returncode == 2, "duplicate output was accepted")
        require(
            "-o may be specified only once" in duplicate_output.stderr,
            duplicate_output.stderr,
        )

        duplicate_section = run(
            [
                str(args.tool),
                "--section",
                ".payload",
                "--section",
                ".payload",
                "-o",
                str(root / "duplicate-section.bin"),
                str(section_object),
            ]
        )
        require(duplicate_section.returncode == 2, "duplicate section was accepted")
        require(
            "--section may be specified only once" in duplicate_section.stderr,
            duplicate_section.stderr,
        )

        conflicting_modes = run(
            [
                str(args.tool),
                "--segment",
                "1",
                "--section",
                ".payload",
                "-o",
                str(root / "conflicting.bin"),
                str(application),
            ]
        )
        require(conflicting_modes.returncode == 2, "both modes were accepted")
        require(
            "exactly one of --segment, --section, or --image with --size"
            in conflicting_modes.stderr,
            conflicting_modes.stderr,
        )

        incomplete_image = run(
            [
                str(args.tool),
                "--image",
                "0x0200",
                "-o",
                str(root / "incomplete.bin"),
                str(application),
            ]
        )
        require(incomplete_image.returncode == 2, "image without size passed")
        require(
            "--image and --size must be specified together" in incomplete_image.stderr,
            incomplete_image.stderr,
        )

        size_without_image = run(
            [
                str(args.tool),
                "--size",
                "1",
                "-o",
                str(root / "size-only.bin"),
                str(application),
            ]
        )
        require(size_without_image.returncode == 2, "size without image passed")
        require(
            "--image and --size must be specified together"
            in size_without_image.stderr,
            size_without_image.stderr,
        )

        multiple_inputs = run(
            [
                str(args.tool),
                "--section",
                ".payload",
                "-o",
                str(root / "multiple.bin"),
                str(section_object),
                str(section_object),
            ]
        )
        require(multiple_inputs.returncode == 2, "multiple inputs were accepted")
        require(
            "exactly one input is required" in multiple_inputs.stderr,
            multiple_inputs.stderr,
        )

        unknown = run([str(args.tool), "--unknown"])
        require(unknown.returncode == 2, "unknown option was accepted")
        require("unknown option: --unknown" in unknown.stderr, unknown.stderr)

        missing_required = run([str(args.tool), str(application)])
        require(missing_required.returncode == 2, "missing options were accepted")
        require(
            "exactly one of --segment, --section, or --image with --size"
            in missing_required.stderr,
            missing_required.stderr,
        )

        help_result = run([str(args.tool), "--help"])
        require(help_result.returncode == 0, "--help failed")
        require(
            help_result.stdout
            == "Usage: jr8objcopy --segment <index> -o <output.bin> "
            "<input.j8a>\n"
            "       jr8objcopy --section <name> -o <output.bin> <input.jro>\n"
            "       jr8objcopy --image <address> --size <bytes> "
            "-o <output.bin> <input.j8a>\n",
            help_result.stdout,
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
