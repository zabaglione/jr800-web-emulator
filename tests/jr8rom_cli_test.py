#!/usr/bin/env python3
# SPDX-License-Identifier: MIT

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import struct
import subprocess
import tempfile


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, check=False, capture_output=True, text=True)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def expected_container(segments: list[tuple[int, bytes]]) -> bytes:
    ordered = sorted(segments)
    material = bytearray(b"JR8ROM-INTEGRITY-V1\0")
    material.extend(struct.pack(">I", len(ordered)))
    records = bytearray()
    for address, data in ordered:
        record = struct.pack(">HI", address, len(data)) + data
        material.extend(record)
        records.extend(record)
    digest = hashlib.sha256(material).digest()
    return (
        b"JR8ROM\0\0"
        + struct.pack(">HHI", 1, 0, 0)
        + digest
        + struct.pack(">I", len(ordered))
        + records
    )


def create_command(
    tool: Path,
    output: Path,
    segments: list[tuple[str, Path]],
) -> list[str]:
    command = [str(tool), "create", "-o", str(output)]
    for address, path in segments:
        command.extend(["--segment", address, str(path)])
    return command


def combine_command(tool: Path, output: Path, inputs: list[Path]) -> list[str]:
    return [
        str(tool),
        "combine",
        "-o",
        str(output),
        *(str(path) for path in inputs),
    ]


def extract_command(
    tool: Path,
    input_path: Path,
    output: Path,
    address: str,
    size: str,
) -> list[str]:
    return [
        str(tool),
        "extract",
        "--address",
        address,
        "--size",
        size,
        "-o",
        str(output),
        str(input_path),
    ]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tool", type=Path, required=True)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="jr8rom-test-") as temporary:
        root = Path(temporary)
        low = root / "low.bin"
        high = root / "high.bin"
        low_data = bytes([0x11, 0x12])
        high_data = bytes([0x31, 0x32])
        low.write_bytes(low_data)
        high.write_bytes(high_data)

        first_output = root / "first.j8r"
        second_output = root / "second.j8r"
        first = run(
            create_command(
                args.tool,
                first_output,
                [("0xF000", high), ("32768", low)],
            )
        )
        second = run(
            create_command(
                args.tool,
                second_output,
                [("$8000", low), ("61440", high)],
            )
        )
        require(first.returncode == 0, first.stderr)
        require(second.returncode == 0, second.stderr)
        require(first.stdout == "" and first.stderr == "", "create was not silent")
        require(second.stdout == "" and second.stderr == "", "repeat was not silent")
        expected = expected_container([(0x8000, low_data), (0xF000, high_data)])
        require(first_output.read_bytes() == expected, "JR8ROM bytes differ")
        require(second_output.read_bytes() == expected, "create is nondeterministic")

        verified = run([str(args.tool), "verify", str(first_output)])
        require(verified.returncode == 0, verified.stderr)
        require(
            verified.stdout == "" and verified.stderr == "",
            "verify success was not silent",
        )

        combine_low = root / "combine-low.j8r"
        combine_high = root / "combine-high.j8r"
        combine_low.write_bytes(expected_container([(0x8000, low_data)]))
        combine_high.write_bytes(expected_container([(0x8002, high_data)]))
        combined = root / "combined.j8r"
        reversed_combined = root / "combined-reversed.j8r"
        combined_result = run(
            combine_command(args.tool, combined, [combine_high, combine_low])
        )
        reversed_result = run(
            combine_command(
                args.tool,
                reversed_combined,
                [combine_low, combine_high],
            )
        )
        require(combined_result.returncode == 0, combined_result.stderr)
        require(reversed_result.returncode == 0, reversed_result.stderr)
        require(
            combined_result.stdout == "" and combined_result.stderr == "",
            "combine success was not silent",
        )
        combined_expected = expected_container(
            [(0x8000, low_data), (0x8002, high_data)]
        )
        require(combined.read_bytes() == combined_expected, "combined bytes differ")
        require(
            reversed_combined.read_bytes() == combined_expected,
            "combine depends on input order",
        )
        inspected = run([str(args.tool), "inspect", str(combined)])
        require(inspected.returncode == 0, inspected.stderr)
        require(inspected.stderr == "", "inspect wrote diagnostics")
        require(
            inspected.stdout
            == (
                "JR8ROM 1.0 segments=2\n"
                "0\taddress=$8000\tlength=2\tend=$8001\n"
                "1\taddress=$8002\tlength=2\tend=$8003\n"
            ),
            inspected.stdout,
        )
        require("sha256" not in inspected.stdout.lower(), "inspect exposed a digest")
        require(str(combined) not in inspected.stdout, "inspect exposed an input path")

        partial_output = root / "partial.bin"
        partial = run(
            extract_command(
                args.tool,
                first_output,
                partial_output,
                "$8001",
                "1",
            )
        )
        require(partial.returncode == 0, partial.stderr)
        require(partial.stdout == "" and partial.stderr == "", "extract was not silent")
        require(partial_output.read_bytes() == low_data[1:], "partial range differs")

        adjacent_container = root / "adjacent.j8r"
        adjacent_create = run(
            create_command(
                args.tool,
                adjacent_container,
                [("0x8002", high), ("0x8000", low)],
            )
        )
        require(adjacent_create.returncode == 0, adjacent_create.stderr)
        adjacent_output = root / "adjacent.bin"
        adjacent_extract = run(
            extract_command(
                args.tool,
                adjacent_container,
                adjacent_output,
                "32769",
                "0x3",
            )
        )
        require(adjacent_extract.returncode == 0, adjacent_extract.stderr)
        require(
            adjacent_output.read_bytes() == low_data[1:] + high_data,
            "extract did not cross adjacent segments",
        )

        gap_output = root / "gap.bin"
        gap_output.write_bytes(b"preserve")
        rejected_gap = run(
            extract_command(
                args.tool,
                first_output,
                gap_output,
                "0x8001",
                "2",
            )
        )
        require(rejected_gap.returncode == 1, "unstored gap was filled")
        require(
            "extraction range includes unstored addresses" in rejected_gap.stderr,
            rejected_gap.stderr,
        )
        require(gap_output.read_bytes() == b"preserve", "gap changed output")

        duplicate_source_output = root / "duplicate-source.j8r"
        duplicate_source = run(
            create_command(
                args.tool,
                duplicate_source_output,
                [("0x8000", low), ("0x9000", low)],
            )
        )
        require(duplicate_source.returncode == 0, duplicate_source.stderr)
        require(
            duplicate_source_output.read_bytes()
            == expected_container([(0x8000, low_data), (0x9000, low_data)]),
            "explicit duplicate source input was not preserved",
        )

        damaged = root / "damaged.j8r"
        damaged_bytes = bytearray(expected)
        damaged_bytes[-1] ^= 0x01
        damaged.write_bytes(damaged_bytes)
        rejected_damage = run([str(args.tool), "verify", str(damaged)])
        require(rejected_damage.returncode == 1, "damaged JR8ROM was accepted")
        require(rejected_damage.stdout == "", "damaged JR8ROM emitted output")
        require(
            "jr8rom: invalid JR8ROM: JR8ROM integrity SHA-256 mismatch"
            in rejected_damage.stderr,
            rejected_damage.stderr,
        )
        rejected_damage_inspect = run(
            [str(args.tool), "inspect", str(damaged)]
        )
        require(
            rejected_damage_inspect.returncode == 1,
            "damaged JR8ROM was inspected",
        )
        require(
            rejected_damage_inspect.stdout == "",
            "damaged inspect emitted partial metadata",
        )
        damaged_extract_output = root / "damaged-extract.bin"
        damaged_extract_output.write_bytes(b"preserve")
        rejected_damaged_extract = run(
            extract_command(
                args.tool,
                damaged,
                damaged_extract_output,
                "0x8000",
                "1",
            )
        )
        require(
            rejected_damaged_extract.returncode == 1,
            "damaged JR8ROM was extracted",
        )
        require(
            damaged_extract_output.read_bytes() == b"preserve",
            "damaged extraction changed output",
        )
        damaged_combine_output = root / "damaged-combine.j8r"
        damaged_combine_output.write_bytes(b"preserve")
        rejected_damaged_combine = run(
            combine_command(
                args.tool,
                damaged_combine_output,
                [combine_low, damaged],
            )
        )
        require(
            rejected_damaged_combine.returncode == 1,
            "damaged JR8ROM was combined",
        )
        require(
            "invalid JR8ROM input" in rejected_damaged_combine.stderr,
            rejected_damaged_combine.stderr,
        )
        require(
            damaged_combine_output.read_bytes() == b"preserve",
            "damaged combine changed output",
        )

        truncated = root / "truncated.j8r"
        truncated.write_bytes(expected[:-1])
        rejected_truncation = run([str(args.tool), "verify", str(truncated)])
        require(rejected_truncation.returncode == 1, "truncated JR8ROM was accepted")
        require("at byte" in rejected_truncation.stderr, rejected_truncation.stderr)

        oversized_container = root / "oversized-container.j8r"
        maximum_encoded_size = 52 + 65_535 * 6 + 65_536
        oversized_container.write_bytes(bytes(maximum_encoded_size + 1))
        rejected_container_size = run(
            [str(args.tool), "verify", str(oversized_container)]
        )
        require(
            rejected_container_size.returncode == 2,
            "oversized container reached the parser",
        )
        require(
            "exceeds the size limit" in rejected_container_size.stderr,
            rejected_container_size.stderr,
        )

        noncanonical = root / "noncanonical.j8r"
        noncanonical_bytes = bytearray(expected)
        first_record = noncanonical_bytes[52:60]
        second_record = noncanonical_bytes[60:68]
        noncanonical_bytes[52:68] = second_record + first_record
        noncanonical.write_bytes(noncanonical_bytes)
        rejected_order = run([str(args.tool), "verify", str(noncanonical)])
        require(rejected_order.returncode == 1, "noncanonical JR8ROM was accepted")
        require("canonical address order" in rejected_order.stderr, rejected_order.stderr)

        preserved = root / "preserved.j8r"
        preserved.write_bytes(b"preserve")
        overlap = run(
            create_command(
                args.tool,
                preserved,
                [("0x8000", low), ("0x8001", high)],
            )
        )
        require(overlap.returncode == 1, "overlapping segments were accepted")
        require("segments must not overlap" in overlap.stderr, overlap.stderr)
        require(preserved.read_bytes() == b"preserve", "invalid create changed output")

        combine_overlap = root / "combine-overlap.j8r"
        combine_overlap.write_bytes(expected_container([(0x8001, high_data)]))
        overlap_output = root / "overlap-combine.j8r"
        overlap_output.write_bytes(b"preserve")
        rejected_combine_overlap = run(
            combine_command(
                args.tool,
                overlap_output,
                [combine_low, combine_overlap],
            )
        )
        require(
            rejected_combine_overlap.returncode == 1,
            "overlapping containers were combined",
        )
        require(
            "segments must not overlap" in rejected_combine_overlap.stderr,
            rejected_combine_overlap.stderr,
        )
        require(
            overlap_output.read_bytes() == b"preserve",
            "overlapping combine changed output",
        )

        empty = root / "empty.bin"
        empty.write_bytes(b"")
        empty_output = root / "empty.j8r"
        rejected_empty = run(
            create_command(args.tool, empty_output, [("0x8000", empty)])
        )
        require(rejected_empty.returncode == 2, "empty segment was accepted")
        require("must not be empty" in rejected_empty.stderr, rejected_empty.stderr)
        require(not empty_output.exists(), "empty segment created output")

        oversized = root / "oversized.bin"
        oversized.write_bytes(bytes(65_537))
        oversized_output = root / "oversized.j8r"
        rejected_oversized = run(
            create_command(args.tool, oversized_output, [("0", oversized)])
        )
        require(rejected_oversized.returncode == 2, "oversized segment was accepted")
        require("exceeds the size limit" in rejected_oversized.stderr, rejected_oversized.stderr)

        full = root / "full.bin"
        full.write_bytes(bytes([0xA5]) * 65_536)
        full_output = root / "full.j8r"
        full_result = run(
            create_command(args.tool, full_output, [("0", full)])
        )
        require(full_result.returncode == 0, full_result.stderr)
        require(
            run([str(args.tool), "verify", str(full_output)]).returncode == 0,
            "full-address-space container did not verify",
        )
        full_extract_output = root / "full-extract.bin"
        full_extract = run(
            extract_command(
                args.tool,
                full_output,
                full_extract_output,
                "0",
                "65536",
            )
        )
        require(full_extract.returncode == 0, full_extract.stderr)
        require(
            full_extract_output.read_bytes() == full.read_bytes(),
            "full-address-space extraction differs",
        )

        exact_alias = run(
            create_command(args.tool, low, [("0x8000", low)])
        )
        require(exact_alias.returncode == 2, "exact input/output alias was accepted")
        require(low.read_bytes() == low_data, "exact alias changed input")

        alias = root / "low-alias.j8r"
        alias.symlink_to(low)
        symlink_alias = run(
            create_command(args.tool, alias, [("0x8000", low)])
        )
        require(symlink_alias.returncode == 2, "symlink alias was accepted")
        require(low.read_bytes() == low_data, "symlink alias changed input")

        extract_exact_alias = run(
            extract_command(
                args.tool,
                first_output,
                first_output,
                "0x8000",
                "1",
            )
        )
        require(
            extract_exact_alias.returncode == 2,
            "extract accepted an exact input/output alias",
        )
        require(first_output.read_bytes() == expected, "extract alias changed input")

        container_alias = root / "container-alias.bin"
        container_alias.symlink_to(first_output)
        extract_symlink_alias = run(
            extract_command(
                args.tool,
                first_output,
                container_alias,
                "0x8000",
                "1",
            )
        )
        require(
            extract_symlink_alias.returncode == 2,
            "extract accepted a symlink input/output alias",
        )
        require(first_output.read_bytes() == expected, "extract symlink changed input")

        combine_exact_alias = run(
            combine_command(
                args.tool,
                combine_low,
                [combine_low, combine_high],
            )
        )
        require(
            combine_exact_alias.returncode == 2,
            "combine accepted an exact input/output alias",
        )
        require(
            combine_low.read_bytes() == expected_container([(0x8000, low_data)]),
            "combine alias changed input",
        )
        combine_symlink_alias = root / "combine-alias.j8r"
        combine_symlink_alias.symlink_to(combine_low)
        rejected_combine_symlink = run(
            combine_command(
                args.tool,
                combine_symlink_alias,
                [combine_low, combine_high],
            )
        )
        require(
            rejected_combine_symlink.returncode == 2,
            "combine accepted a symlink input/output alias",
        )
        require(
            combine_low.read_bytes() == expected_container([(0x8000, low_data)]),
            "combine symlink alias changed input",
        )

        missing = run(
            create_command(
                args.tool,
                root / "missing-output.j8r",
                [("0x8000", root / "missing.bin")],
            )
        )
        require(missing.returncode == 2, "missing segment input was accepted")
        require("cannot inspect input" in missing.stderr, missing.stderr)

        missing_verify = run(
            [str(args.tool), "verify", str(root / "missing.j8r")]
        )
        require(missing_verify.returncode == 2, "missing container was accepted")
        require("cannot inspect input" in missing_verify.stderr, missing_verify.stderr)

        missing_extract = run(
            extract_command(
                args.tool,
                root / "missing.j8r",
                root / "missing-extract.bin",
                "0x8000",
                "1",
            )
        )
        require(missing_extract.returncode == 2, "missing container was extracted")
        require("cannot inspect input" in missing_extract.stderr, missing_extract.stderr)

        missing_combine = run(
            combine_command(
                args.tool,
                root / "missing-combine-output.j8r",
                [combine_low, root / "missing.j8r"],
            )
        )
        require(missing_combine.returncode == 2, "missing container was combined")
        require("cannot inspect input" in missing_combine.stderr, missing_combine.stderr)

        output_failure = run(
            create_command(args.tool, root, [("0x8000", low)])
        )
        require(output_failure.returncode == 2, "directory output was accepted")
        require("cannot open output" in output_failure.stderr, output_failure.stderr)

        for invalid_address in ["0x10000", "-1", "0x", "12junk"]:
            invalid = run(
                create_command(
                    args.tool,
                    root / f"invalid-{invalid_address.replace('/', '_')}.j8r",
                    [(invalid_address, low)],
                )
            )
            require(invalid.returncode == 2, f"invalid address accepted: {invalid_address}")
            require("invalid segment address" in invalid.stderr, invalid.stderr)

        argument_cases = [
            ([str(args.tool)], "a command is required"),
            ([str(args.tool), "unknown"], "unknown command"),
            ([str(args.tool), "verify"], "verify requires exactly one input"),
            (
                [str(args.tool), "verify", str(first_output), str(second_output)],
                "verify requires exactly one input",
            ),
            ([str(args.tool), "inspect"], "inspect requires exactly one input"),
            (
                [str(args.tool), "inspect", str(first_output), str(second_output)],
                "inspect requires exactly one input",
            ),
            ([str(args.tool), "create", "--unknown"], "unknown option"),
            ([str(args.tool), "create", "extra"], "unexpected argument"),
            ([str(args.tool), "create", "-o", str(root / "none")], "at least one"),
            (
                [str(args.tool), "create", "--segment", "0x8000"],
                "requires an address and input",
            ),
            (
                [
                    str(args.tool),
                    "create",
                    "-o",
                    str(root / "a"),
                    "-o",
                    str(root / "b"),
                    "--segment",
                    "0x8000",
                    str(low),
                ],
                "may be specified only once",
            ),
            ([str(args.tool), "combine"], "combine requires"),
            (
                [
                    str(args.tool),
                    "combine",
                    "-o",
                    str(root / "one-input.j8r"),
                    str(combine_low),
                ],
                "at least two inputs",
            ),
            (
                [
                    str(args.tool),
                    "combine",
                    "--unknown",
                    str(combine_low),
                    str(combine_high),
                ],
                "unknown option",
            ),
            ([str(args.tool), "extract"], "extract requires"),
            (
                extract_command(
                    args.tool,
                    first_output,
                    root / "zero.bin",
                    "0x8000",
                    "0",
                ),
                "invalid extraction size",
            ),
            (
                extract_command(
                    args.tool,
                    first_output,
                    root / "overflow.bin",
                    "0xFFFF",
                    "2",
                ),
                "extraction range exceeds",
            ),
            (
                extract_command(
                    args.tool,
                    first_output,
                    root / "bad-address.bin",
                    "0x10000",
                    "1",
                ),
                "invalid extraction address",
            ),
            (
                [
                    str(args.tool),
                    "extract",
                    "--address",
                    "0x8000",
                    "--address",
                    "0x8001",
                    "--size",
                    "1",
                    "-o",
                    str(root / "duplicate-address.bin"),
                    str(first_output),
                ],
                "--address may be specified only once",
            ),
        ]
        for command, diagnostic in argument_cases:
            result = run(command)
            require(result.returncode == 2, f"invalid arguments accepted: {command}")
            require(diagnostic in result.stderr, result.stderr)

        help_result = run([str(args.tool), "--help"])
        require(help_result.returncode == 0, help_result.stderr)
        require(help_result.stderr == "", "help wrote diagnostics")
        require("jr8rom create" in help_result.stdout, "help omitted create")
        require("jr8rom combine" in help_result.stdout, "help omitted combine")
        require("jr8rom extract" in help_result.stdout, "help omitted extract")
        require("jr8rom inspect" in help_result.stdout, "help omitted inspect")
        require("jr8rom verify" in help_result.stdout, "help omitted verify")

        version = run([str(args.tool), "--version"])
        require(version.returncode == 0, version.stderr)
        require(version.stdout == "jr8rom 0.1.0\n", version.stdout)
        require(version.stderr == "", "version wrote diagnostics")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
