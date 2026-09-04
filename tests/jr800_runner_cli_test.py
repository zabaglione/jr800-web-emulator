#!/usr/bin/env python3
# SPDX-License-Identifier: MIT

from __future__ import annotations

import argparse
import hashlib
import struct
import subprocess
import tempfile
from pathlib import Path


ROM_SIZE = 32_768


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, capture_output=True, check=False, text=True)


def write_container(path: Path, segments: list[tuple[int, bytes]]) -> None:
    ordered = sorted(segments)
    integrity_material = bytearray(b"JR8ROM-INTEGRITY-V1\0")
    integrity_material.extend(struct.pack(">I", len(ordered)))
    records = bytearray()
    for address, data in ordered:
        record = struct.pack(">HI", address, len(data)) + data
        integrity_material.extend(record)
        records.extend(record)
    path.write_bytes(
        b"JR8ROM\0\0"
        + struct.pack(">HHI", 1, 0, 0)
        + hashlib.sha256(integrity_material).digest()
        + struct.pack(">I", len(ordered))
        + records
    )


def rom_image(program: bytes) -> bytes:
    image = bytearray([0x01] * ROM_SIZE)
    image[: len(program)] = program
    image[-2:] = b"\x80\x00"
    return bytes(image)


def make_rom(path: Path, program: bytes) -> None:
    write_container(path, [(0x8000, rom_image(program))])


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runner", type=Path, required=True)
    args = parser.parse_args()

    help_result = run([str(args.runner), "jr800", "--help"])
    require(help_result.returncode == 0, "JR-800 help failed")
    require(
        "--basic-boot-experiment" in help_result.stdout
        and "--reset-sp" in help_result.stdout
        and "--reset-cc" in help_result.stdout
        and "--internal-ram-initial" in help_result.stdout
        and "--standard-ram-initial" in help_result.stdout
        and "--calendar-address-source" in help_result.stdout
        and "--calendar-cpu-cycle-ratio" in help_result.stdout
        and "--keyboard-response" in help_result.stdout
        and "<rom.j8r>" in help_result.stdout,
        "JR-800 help omitted explicit experiment inputs",
    )

    with tempfile.TemporaryDirectory() as temporary_directory:
        temporary = Path(temporary_directory)
        nop_rom = temporary / "nop.j8r"
        make_rom(nop_rom, b"\x01")

        nop_result = run(
            [
                str(args.runner),
                "jr800",
                "--max-instructions",
                "3",
                str(nop_rom),
            ]
        )
        require(nop_result.returncode == 0, "Bounded NOP run failed")
        require(
            "stop=instruction-limit" in nop_result.stdout
            and "instructions=3" in nop_result.stdout
            and "execution-cycles=3" in nop_result.stdout
            and "timer-input-capture-interrupts=0" in nop_result.stdout
            and "timer-output-compare-interrupts=0" in nop_result.stdout
            and "timer-overflow-interrupts=0" in nop_result.stdout
            and "serial-interrupts=0" in nop_result.stdout
            and "instructions-after-last-interrupt=0" in nop_result.stdout
            and "keyboard-read-attempts=0" in nop_result.stdout
            and "keyboard-distinct-addresses=0" in nop_result.stdout
            and "calendar-alarm-terminal=disconnected" in nop_result.stdout
            and "port2-timer-output=disabled" in nop_result.stdout
            and "port2-timer-output-observed=disabled" in nop_result.stdout
            and "cpu-fault=none" in nop_result.stdout,
            "Bounded NOP summary differs",
        )
        require(
            "lcd-unknown-dots=" not in nop_result.stdout
            and "lcd-off-dots=" not in nop_result.stdout
            and "lcd-on-dots=" not in nop_result.stdout,
            "Disconnected LCD unexpectedly produced aggregate output",
        )

        configured_result = run(
            [
                str(args.runner),
                "jr800",
                "--max-instructions",
                "1",
                "--reset-sp",
                "0x2345",
                "--reset-x",
                "0x3456",
                "--reset-a",
                "0x67",
                "--reset-b",
                "0x89",
                "--reset-cc",
                "0x25:0x2f",
                "--internal-ram-initial",
                "0xa5",
                "--standard-ram-initial",
                "0",
                "--expansion-ram-initial",
                "0xff",
                "--lcd-unknown-data",
                "0xde",
                "--calendar-address-source",
                "a0-a3",
                "--calendar-upper-read",
                "zero",
                "--calendar-cpu-cycle-ratio",
                "e030-nominal-1.2288mhz",
                "--port1-pins",
                "0:0xff",
                "--port2-pins",
                "8:0x1f",
                "--ram-standby",
                "invalid",
                "--keyboard-window-value",
                "0",
                str(nop_rom),
            ]
        )
        require(configured_result.returncode == 0, "Explicit input run failed")
        require(
            "lcd-substituted-reads=0" in configured_result.stdout
            and "lcd-unknown-dots=0" in configured_result.stdout
            and "lcd-off-dots=12288" in configured_result.stdout
            and "lcd-on-dots=0" in configured_result.stdout
            and "calendar-alarm-terminal=released" in configured_result.stdout,
            "Explicit LCD experiment was not attached",
        )

        basic_boot_rom = temporary / "basic-boot-profile.j8r"
        basic_boot_program = bytearray()
        failure_branches: list[int] = []

        def append_checked_read(instruction: bytes, expected: int) -> None:
            basic_boot_program.extend(instruction)
            basic_boot_program.extend((0x81, expected, 0x26, 0x00))
            failure_branches.append(len(basic_boot_program) - 1)

        append_checked_read(b"\x96\x80", 0x00)
        append_checked_read(b"\xb6\x20\x00", 0x00)
        append_checked_read(b"\xb6\x60\x00", 0x00)
        append_checked_read(b"\xb6\x00\x02", 0xFF)
        append_checked_read(b"\xb6\x00\x03", 0xDE)
        append_checked_read(b"\xb6\x06\x00", 0x00)
        append_checked_read(b"\xb6\x0c\x00", 0xFF)
        basic_boot_program.extend(b"\x86\x00\xb7\x0a\x01")
        append_checked_read(b"\xb6\x0b\x01", 0x00)
        basic_boot_program.append(0x01)
        failure_opcode = len(basic_boot_program)
        basic_boot_program.append(0x00)
        for displacement_index in failure_branches:
            displacement = failure_opcode - (displacement_index + 1)
            require(displacement <= 0x7F, "BASIC boot test branch overflow")
            basic_boot_program[displacement_index] = displacement
        make_rom(basic_boot_rom, bytes(basic_boot_program))
        basic_boot_result = run(
            [
                str(args.runner),
                "jr800",
                "--max-instructions",
                "27",
                "--basic-boot-experiment",
                str(basic_boot_rom),
            ]
        )
        require(
            basic_boot_result.returncode == 0
            and "stop=instruction-limit" in basic_boot_result.stdout
            and "instructions=27" in basic_boot_result.stdout
            and "keyboard-read-attempts=1" in basic_boot_result.stdout
            and "keyboard-distinct-addresses=1" in basic_boot_result.stdout
            and "calendar-alarm-terminal=released"
            in basic_boot_result.stdout
            and "lcd-substituted-reads=1" in basic_boot_result.stdout
            and "lcd-unknown-dots=0" in basic_boot_result.stdout,
            "Named BASIC boot experiment did not apply every machine input: "
            f"stdout={basic_boot_result.stdout!r} "
            f"stderr={basic_boot_result.stderr!r}",
        )

        conflicting_basic_boot_result = run(
            [
                str(args.runner),
                "jr800",
                "--basic-boot-experiment",
                "--standard-ram-initial",
                "0",
                str(nop_rom),
            ]
        )
        require(
            conflicting_basic_boot_result.returncode == 2
            and "cannot be combined with explicit machine inputs"
            in conflicting_basic_boot_result.stderr,
            "Named BASIC boot experiment accepted an overriding input",
        )

        alarm_rom = temporary / "alarm.j8r"
        make_rom(alarm_rom, b"\x86\x04\xb7\x06\x0d")
        alarm_result = run(
            [
                str(args.runner),
                "jr800",
                "--max-instructions",
                "2",
                "--calendar-address-source",
                "a0-a3",
                "--calendar-upper-read",
                "zero",
                str(alarm_rom),
            ]
        )
        require(
            alarm_result.returncode == 0
            and "calendar-alarm-terminal=pull-low" in alarm_result.stdout,
            "Calendar ALARM terminal drive was missing from the summary",
        )

        timer_output_rom = temporary / "timer-output.j8r"
        make_rom(
            timer_output_rom,
            b"\x86\x01\x97\x08\x86\x02\x97\x01\xcc\xff\xfc\xdd\x09",
        )
        timer_output_result = run(
            [
                str(args.runner),
                "jr800",
                "--max-instructions",
                "6",
                str(timer_output_rom),
            ]
        )
        require(
            timer_output_result.returncode == 0
            and "port2-timer-output=high" in timer_output_result.stdout
            and "port2-timer-output-observed=disabled,unknown,high"
            in timer_output_result.stdout,
            "Timer-output endpoint was missing from the summary",
        )

        unknown_timer_output_rom = temporary / "unknown-timer-output.j8r"
        make_rom(unknown_timer_output_rom, b"\x86\x02\x97\x01")
        unknown_timer_output_result = run(
            [
                str(args.runner),
                "jr800",
                "--max-instructions",
                "2",
                str(unknown_timer_output_rom),
            ]
        )
        require(
            unknown_timer_output_result.returncode == 0
            and "port2-timer-output=unknown"
            in unknown_timer_output_result.stdout
            and "port2-timer-output-observed=disabled,unknown"
            in unknown_timer_output_result.stdout,
            "Unknown timer-output endpoint was collapsed",
        )

        low_timer_output_rom = temporary / "low-timer-output.j8r"
        make_rom(
            low_timer_output_rom,
            b"\x86\x02\x97\x01\xcc\xff\xfc\xdd\x09",
        )
        low_timer_output_result = run(
            [
                str(args.runner),
                "jr800",
                "--max-instructions",
                "4",
                str(low_timer_output_rom),
            ]
        )
        require(
            low_timer_output_result.returncode == 0
            and "port2-timer-output=low" in low_timer_output_result.stdout
            and "port2-timer-output-observed=disabled,unknown,low"
            in low_timer_output_result.stdout,
            "Low timer-output endpoint was collapsed",
        )

        keyboard_rom = temporary / "keyboard.j8r"
        make_rom(
            keyboard_rom,
            b"\xb6\x0c\x00\xb6\x0c\x01\xb6\x0c\x00",
        )
        keyboard_result = run(
            [
                str(args.runner),
                "jr800",
                "--max-instructions",
                "3",
                "--keyboard-window-value",
                "0xff",
                str(keyboard_rom),
            ]
        )
        require(keyboard_result.returncode == 0, "Keyboard scan run failed")
        require(
            "keyboard-read-attempts=3" in keyboard_result.stdout
            and "keyboard-distinct-addresses=2" in keyboard_result.stdout,
            "Privacy-bounded keyboard summary differs",
        )

        keyboard_specific_result = run(
            [
                str(args.runner),
                "jr800",
                "--max-instructions",
                "3",
                "--keyboard-response",
                "0x0c00:0x11",
                "--keyboard-response",
                "0x0c01:0x22",
                str(keyboard_rom),
            ]
        )
        require(
            keyboard_specific_result.returncode == 0
            and "keyboard-read-attempts=3"
            in keyboard_specific_result.stdout
            and "keyboard-distinct-addresses=2"
            in keyboard_specific_result.stdout,
            "Address-specific keyboard responses were not applied",
        )

        keyboard_value_rom = temporary / "keyboard-value.j8r"
        make_rom(
            keyboard_value_rom,
            b"\xb6\x0c\x23\x81\x5a\x26\x01\x01\x00",
        )
        keyboard_override_result = run(
            [
                str(args.runner),
                "jr800",
                "--max-instructions",
                "4",
                "--keyboard-response",
                "$0C23:0x5A",
                "--keyboard-window-value",
                "0xff",
                str(keyboard_value_rom),
            ]
        )
        require(
            keyboard_override_result.returncode == 0
            and "keyboard-read-attempts=1"
            in keyboard_override_result.stdout
            and "keyboard-distinct-addresses=1"
            in keyboard_override_result.stdout,
            "Address-specific response did not override the uniform value",
        )

        duplicate_keyboard_response = run(
            [
                str(args.runner),
                "jr800",
                "--keyboard-response",
                "0x0c23:0x11",
                "--keyboard-response",
                "$0C23:0x22",
                str(nop_rom),
            ]
        )
        require(
            duplicate_keyboard_response.returncode == 2
            and "duplicate keyboard response"
            in duplicate_keyboard_response.stderr
            and "0x0c23:0x11" not in duplicate_keyboard_response.stderr
            and "$0C23:0x22" not in duplicate_keyboard_response.stderr,
            "Duplicate keyboard response was accepted or disclosed",
        )

        for invalid_response in (
            "0x0bff:0",
            "0x1000:0",
            "0x0c00:0x100",
            "0x0c00",
            "0x0c00:",
        ):
            invalid_keyboard_response = run(
                [
                    str(args.runner),
                    "jr800",
                    "--keyboard-response",
                    invalid_response,
                    str(nop_rom),
                ]
            )
            require(
                invalid_keyboard_response.returncode == 2
                and "invalid keyboard response"
                in invalid_keyboard_response.stderr
                and invalid_response not in invalid_keyboard_response.stderr,
                "Invalid keyboard response was accepted or disclosed",
            )

        read_rom = temporary / "read.j8r"
        make_rom(read_rom, b"\xb6\x20\x00")
        uninitialized_result = run(
            [
                str(args.runner),
                "jr800",
                "--max-instructions",
                "1",
                str(read_rom),
            ]
        )
        require(
            uninitialized_result.returncode == 1,
            "Uninitialized RAM read unexpectedly succeeded",
        )
        require(
            "stop=cpu-fault" in uninitialized_result.stdout
            and "cpu-fault=bus-access" in uninitialized_result.stdout
            and "bus-fault=uninitialized-read" in uninitialized_result.stdout
            and "fault-access=data-read" in uninitialized_result.stdout
            and "fault-region=standard-ram" in uninitialized_result.stdout,
            "Coarse RAM fault summary differs",
        )
        require(
            "fault-address" not in uninitialized_result.stdout
            and "opcode=" not in uninitialized_result.stdout
            and " pc=" not in uninitialized_result.stdout,
            "Content-bearing fault detail escaped the default summary",
        )

        initialized_result = run(
            [
                str(args.runner),
                "jr800",
                "--max-instructions",
                "1",
                "--standard-ram-initial",
                "0x42",
                str(read_rom),
            ]
        )
        require(
            initialized_result.returncode == 0
            and "instructions=1" in initialized_result.stdout,
            "Explicit standard RAM initialization was not applied",
        )

        internal_read_rom = temporary / "internal-read.j8r"
        make_rom(internal_read_rom, b"\x96\x80")
        unknown_internal_result = run(
            [
                str(args.runner),
                "jr800",
                "--max-instructions",
                "1",
                str(internal_read_rom),
            ]
        )
        require(
            unknown_internal_result.returncode == 1
            and "bus-fault=uninitialized-read"
            in unknown_internal_result.stdout
            and "fault-region=cpu-internal-ram"
            in unknown_internal_result.stdout,
            "Unknown internal RAM did not fail closed",
        )
        initialized_internal_result = run(
            [
                str(args.runner),
                "jr800",
                "--max-instructions",
                "1",
                "--internal-ram-initial",
                "0x42",
                str(internal_read_rom),
            ]
        )
        require(
            initialized_internal_result.returncode == 0
            and "instructions=1" in initialized_internal_result.stdout,
            "Explicit internal RAM initialization was not applied",
        )

        indexed_read_rom = temporary / "indexed-read.j8r"
        make_rom(indexed_read_rom, b"\xa6\x00")
        unknown_index_result = run(
            [
                str(args.runner),
                "jr800",
                "--max-instructions",
                "1",
                str(indexed_read_rom),
            ]
        )
        require(
            unknown_index_result.returncode == 1
            and "cpu-fault=unknown-state" in unknown_index_result.stdout
            and "state-fault=index-register" in unknown_index_result.stdout,
            "Unknown reset index register did not fail closed",
        )
        known_index_result = run(
            [
                str(args.runner),
                "jr800",
                "--max-instructions",
                "1",
                "--reset-x",
                "0x8000",
                str(indexed_read_rom),
            ]
        )
        require(
            known_index_result.returncode == 0
            and "instructions=1" in known_index_result.stdout,
            "Explicit reset index register was not applied",
        )

        invalid_reset_cc = run(
            [
                str(args.runner),
                "jr800",
                "--reset-cc",
                "0x40:0x40",
                str(nop_rom),
            ]
        )
        require(
            invalid_reset_cc.returncode == 2
            and "invalid reset condition code" in invalid_reset_cc.stderr,
            "Fixed reset condition-code bit was accepted as an override",
        )

        incomplete_calendar = run(
            [
                str(args.runner),
                "jr800",
                "--calendar-address-source",
                "a0-a3",
                str(nop_rom),
            ]
        )
        require(
            incomplete_calendar.returncode == 2
            and "both calendar options are required"
            in incomplete_calendar.stderr,
            "Incomplete calendar hypothesis was accepted",
        )

        ratio_without_calendar = run(
            [
                str(args.runner),
                "jr800",
                "--calendar-cpu-cycle-ratio",
                "e030-nominal-1.2288mhz",
                str(nop_rom),
            ]
        )
        require(
            ratio_without_calendar.returncode == 2
            and "ratio requires calendar options"
            in ratio_without_calendar.stderr,
            "Calendar CPU-cycle ratio was accepted without a calendar",
        )

        unknown_calendar_ratio = run(
            [
                str(args.runner),
                "jr800",
                "--calendar-cpu-cycle-ratio",
                "measured-1.2288mhz",
                str(nop_rom),
            ]
        )
        require(
            unknown_calendar_ratio.returncode == 2
            and "invalid calendar CPU-cycle ratio"
            in unknown_calendar_ratio.stderr,
            "Unknown calendar CPU-cycle ratio was accepted",
        )

        obsolete_calendar_ratio = run(
            [
                str(args.runner),
                "jr800",
                "--calendar-address-source",
                "a0-a3",
                "--calendar-upper-read",
                "zero",
                "--calendar-cpu-cycle-ratio",
                "e030-assumed-1.2288mhz",
                str(nop_rom),
            ]
        )
        require(
            obsolete_calendar_ratio.returncode == 2
            and "invalid calendar CPU-cycle ratio"
            in obsolete_calendar_ratio.stderr,
            "Obsolete assumed calendar CPU-cycle ratio was accepted",
        )

        expansion_without_standard = run(
            [
                str(args.runner),
                "jr800",
                "--expansion-ram-initial",
                "0",
                str(nop_rom),
            ]
        )
        require(
            expansion_without_standard.returncode == 2
            and "requires explicit standard RAM"
            in expansion_without_standard.stderr,
            "Expansion RAM was accepted without standard RAM state",
        )

        invalid_port2 = run(
            [
                str(args.runner),
                "jr800",
                "--port2-pins",
                "0x20:0x1f",
                str(nop_rom),
            ]
        )
        require(
            invalid_port2.returncode == 2
            and "invalid pin state" in invalid_port2.stderr,
            "Nonexistent Port 2 pins were accepted",
        )

        split_rom = temporary / "split.j8r"
        split_image = rom_image(b"\x01")
        write_container(
            split_rom,
            [
                (0xC000, split_image[0x4000:]),
                (0x8000, split_image[:0x4000]),
            ],
        )
        split_result = run(
            [
                str(args.runner),
                "jr800",
                "--max-instructions",
                "1",
                str(split_rom),
            ]
        )
        require(
            split_result.returncode == 0
            and "instructions=1" in split_result.stdout,
            "Adjacent JR8ROM segments were not loaded as one logical range",
        )

        incomplete_rom = temporary / "incomplete.j8r"
        write_container(incomplete_rom, [(0x8000, split_image[:-1])])
        incomplete_result = run(
            [str(args.runner), "jr800", str(incomplete_rom)]
        )
        require(
            incomplete_result.returncode == 1
            and "does not completely cover logical ROM range"
            in incomplete_result.stderr,
            "Incomplete JR8ROM coverage was accepted",
        )

        damaged_rom = temporary / "damaged.j8r"
        damaged_bytes = bytearray(nop_rom.read_bytes())
        damaged_bytes[-1] ^= 0x01
        damaged_rom.write_bytes(damaged_bytes)
        damaged_result = run([str(args.runner), "jr800", str(damaged_rom)])
        require(
            damaged_result.returncode == 1
            and "integrity SHA-256 mismatch" in damaged_result.stderr,
            "Damaged JR8ROM was accepted",
        )

        raw_rom = temporary / "raw.bin"
        raw_rom.write_bytes(split_image)
        raw_result = run([str(args.runner), "jr800", str(raw_rom)])
        require(
            raw_result.returncode == 1
            and "invalid JR8ROM magic" in raw_result.stderr,
            "Obsolete raw logical-ROM input was accepted",
        )

        missing_result = run(
            [str(args.runner), "jr800", str(temporary / "missing.j8r")]
        )
        require(
            missing_result.returncode == 2
            and "cannot inspect JR8ROM input" in missing_result.stderr,
            "Missing JR8ROM input did not report an I/O error",
        )


if __name__ == "__main__":
    main()
