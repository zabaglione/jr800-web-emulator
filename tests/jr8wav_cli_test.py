#!/usr/bin/env python3
# SPDX-License-Identifier: MIT

from __future__ import annotations

import argparse
import array
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import wave


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, check=False, capture_output=True, text=True)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def single_segment_jr8rom(address: int, payload: bytes) -> bytes:
    count = (1).to_bytes(4, "big")
    record = address.to_bytes(2, "big") + len(payload).to_bytes(4, "big") + payload
    integrity = hashlib.sha256(b"JR8ROM-INTEGRITY-V1\0" + count + record).digest()
    return (
        b"JR8ROM\0\0"
        + (1).to_bytes(2, "big")
        + (0).to_bytes(2, "big")
        + (0).to_bytes(4, "big")
        + integrity
        + count
        + record
    )


def native_block(
    payload: bytes,
    *,
    header: bool,
    address: int = 0x8000,
    execution_address: int = 0x8123,
    byte_order: str = "little",
    reserved_byte_before_fields: bool = False,
) -> bytes:
    if header:
        body = bytearray(32)
        body[0] = 1
        body[1:10] = b"SYNTHETIC"
        offset = 18 if reserved_byte_before_fields else 17
        body[offset : offset + 2] = (len(payload)).to_bytes(2, byte_order)
        body[offset + 2 : offset + 4] = address.to_bytes(2, byte_order)
        body[offset + 4 : offset + 6] = execution_address.to_bytes(2, byte_order)
        return bytes(body) + (sum(body) & 0xFFFF).to_bytes(2, "big")
    return payload + (sum(payload) & 0xFFFF).to_bytes(2, "big")


def write_native_msave_wav(
    path: Path,
    payload: bytes,
    *,
    leading_silence: int,
    address: int = 0x8000,
    execution_address: int = 0x8123,
    byte_order: str = "little",
    continuous_blocks: bool = False,
    reserved_byte_before_fields: bool = False,
) -> None:
    sample_rate = 48_000
    amplitude = 12_000
    signal = array.array("h", [0]) * leading_silence

    def append_cycle(long_period: bool) -> None:
        half_period = 21 if long_period else 11
        signal.extend([amplitude] * half_period)
        signal.extend([-amplitude] * half_period)

    def append_cycles(long_period: bool, count: int) -> None:
        for _ in range(count):
            append_cycle(long_period)

    def append_byte(value: int) -> None:
        for bit in range(7, -1, -1):
            append_cycle(bool((value >> bit) & 1))
        append_cycle(True)

    def append_block(block: bytes, sync_cycles: int) -> None:
        append_cycles(False, 4_000)
        append_cycles(True, sync_cycles)
        append_cycles(False, sync_cycles)
        append_cycles(True, 2)
        for value in block:
            append_byte(value)

    append_block(
        native_block(
            payload,
            header=True,
            address=address,
            execution_address=execution_address,
            byte_order=byte_order,
            reserved_byte_before_fields=reserved_byte_before_fields,
        ),
        40,
    )
    if continuous_blocks:
        signal.extend([-amplitude] * 720)
    else:
        signal.extend([0] * (sample_rate // 10))
    append_block(native_block(payload, header=False), 20)
    signal.extend([0] * (sample_rate // 10))

    interleaved = array.array("h")
    for sample in signal:
        interleaved.append(sample)
        interleaved.append(0)
    if sys.byteorder != "little":
        interleaved.byteswap()
    with wave.open(str(path), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        output.writeframes(interleaved.tobytes())


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tool", type=Path, required=True)
    parser.add_argument("--jr8rom-tool", type=Path, required=True)
    parser.add_argument("--runner", type=Path, required=True)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="jr8wav-test-") as temporary:
        root = Path(temporary)
        usage = run([str(args.tool)])
        require(usage.returncode == 1, "missing command status differs")
        require("<output.j8r>" in usage.stderr, usage.stderr)
        require(
            "decode <input.wav> <output.bin>" not in usage.stderr,
            "obsolete raw decode output remains in usage",
        )

        source = root / "synthetic.bin"
        first_wav = root / "first.wav"
        second_wav = root / "second.wav"
        decoded = root / "decoded.j8r"
        payload = bytes((index * 37 + 11) & 0xFF for index in range(700))
        source.write_bytes(payload)
        expected_hash = hashlib.sha256(payload).hexdigest()

        encode_command = [
            str(args.tool),
            "encode",
            "--address",
            "0x8000",
            "--block-size",
            "128",
            str(source),
            str(first_wav),
        ]
        first = run(encode_command)
        require(first.returncode == 0, f"encode failed: {first.stderr}")
        require("address=$8000" in first.stdout, first.stdout)
        require(f"sha256={expected_hash}" in first.stdout, first.stdout)
        wav_bytes = first_wav.read_bytes()
        require(wav_bytes.startswith(b"RIFF"), "RIFF magic is missing")
        require(wav_bytes[8:12] == b"WAVE", "WAVE magic is missing")

        second_command = [*encode_command[:-1], str(second_wav)]
        second = run(second_command)
        require(second.returncode == 0, f"second encode failed: {second.stderr}")
        require(first_wav.read_bytes() == second_wav.read_bytes(), "WAV output differs")

        decode = run([str(args.tool), "decode", str(first_wav), str(decoded)])
        require(decode.returncode == 0, f"decode failed: {decode.stderr}")
        require(
            decoded.read_bytes() == single_segment_jr8rom(0x8000, payload),
            "decoded JR8ROM differs",
        )
        require("address=$8000" in decode.stdout, decode.stdout)
        require(f"sha256={expected_hash}" in decode.stdout, decode.stdout)

        truncated_wav = root / "truncated.wav"
        truncated_wav.write_bytes(wav_bytes[:-1_000])
        refused_output = root / "refused.j8r"
        refused_output.write_bytes(b"unchanged")
        refused = run(
            [str(args.tool), "decode", str(truncated_wav), str(refused_output)]
        )
        require(refused.returncode == 2, f"truncated status differs: {refused.stderr}")
        require(
            "refusing incomplete or unverified output" in refused.stderr,
            refused.stderr,
        )
        require(refused_output.read_bytes() == b"unchanged", "failed decode changed output")

        overflowing_wav = root / "overflow.wav"
        overflow = run(
            [
                str(args.tool),
                "encode",
                "--address",
                "0xFF00",
                str(source),
                str(overflowing_wav),
            ]
        )
        require(overflow.returncode == 1, "address overflow was accepted")
        require(not overflowing_wav.exists(), "failed encode emitted WAV output")

        original_source = source.read_bytes()
        encode_collision = run(
            [str(args.tool), "encode", "--address", "0x8000", str(source), str(source)]
        )
        require(encode_collision.returncode == 1, "encode path collision was accepted")
        require(source.read_bytes() == original_source, "encode collision changed input")

        original_wav = first_wav.read_bytes()
        decode_collision = run(
            [str(args.tool), "decode", str(first_wav), str(first_wav)]
        )
        require(decode_collision.returncode == 1, "decode path collision was accepted")
        require(first_wav.read_bytes() == original_wav, "decode collision changed input")

        native_payload_bytes = bytearray(
            (index * 19 + 7) & 0xFF for index in range(256)
        )
        native_payload_bytes[0] = 0x01
        native_payload = bytes(native_payload_bytes)
        native_hash = hashlib.sha256(native_payload).hexdigest()
        native_first = root / "native-first.wav"
        native_second = root / "native-second.wav"
        write_native_msave_wav(
            native_first,
            native_payload,
            leading_silence=4_800,
        )
        write_native_msave_wav(
            native_second,
            native_payload,
            leading_silence=5_200,
        )
        require(
            native_first.read_bytes() != native_second.read_bytes(),
            "independent native recordings must differ at the WAV level",
        )

        native_decoded = root / "native-decoded.j8r"
        native_decode = run(
            [
                str(args.tool),
                "decode-native-msave",
                str(native_first),
                str(native_decoded),
            ]
        )
        require(native_decode.returncode == 0, native_decode.stderr)
        expected_native_jr8rom = single_segment_jr8rom(0x8000, native_payload)
        require(
            native_decoded.read_bytes() == expected_native_jr8rom,
            "native decode JR8ROM differs",
        )
        require("address=$8000" in native_decode.stdout, native_decode.stdout)
        require(f"sha256={native_hash}" in native_decode.stdout, native_decode.stdout)

        program_payload = bytes([0x86, 0x42, 0x20, 0xFE])
        program_wav = root / "native-program.wav"
        program_application = root / "native-program.j8a"
        write_native_msave_wav(
            program_wav,
            program_payload,
            leading_silence=4_800,
            address=0x2800,
            execution_address=0x2800,
            byte_order="big",
            continuous_blocks=True,
            reserved_byte_before_fields=True,
        )
        program_decode = run(
            [
                str(args.tool),
                "decode-native-program",
                str(program_wav),
                str(program_application),
            ]
        )
        require(program_decode.returncode == 0, program_decode.stderr)
        require("address=$2800" in program_decode.stdout, program_decode.stdout)
        require("execution=$2800" in program_decode.stdout, program_decode.stdout)
        require(
            "header-byte-order=big-endian" in program_decode.stdout,
            program_decode.stdout,
        )
        require(
            "header-layout=reserved-byte-before-fields" in program_decode.stdout,
            program_decode.stdout,
        )
        program_run = run(
            [
                str(args.runner),
                "--max-instructions",
                "1",
                "--expect-stop",
                "instruction-limit",
                "--expect",
                "PC == 0x2802 && A == 0x42",
                str(program_application),
            ]
        )
        require(program_run.returncode == 0, program_run.stderr)

        outside_entry_wav = root / "outside-entry.wav"
        outside_entry_output = root / "outside-entry.j8a"
        outside_entry_output.write_bytes(b"unchanged")
        write_native_msave_wav(
            outside_entry_wav,
            program_payload,
            leading_silence=4_800,
            address=0x2800,
            execution_address=0x3000,
            byte_order="big",
            continuous_blocks=True,
            reserved_byte_before_fields=True,
        )
        outside_entry = run(
            [
                str(args.tool),
                "decode-native-program",
                str(outside_entry_wav),
                str(outside_entry_output),
            ]
        )
        require(outside_entry.returncode == 2, outside_entry.stderr)
        require(
            "invalid-program-range" in outside_entry.stderr,
            outside_entry.stderr,
        )
        require(
            outside_entry_output.read_bytes() == b"unchanged",
            "invalid program conversion changed output",
        )

        native_verified = root / "native-verified.j8r"
        native_verify = run(
            [
                str(args.tool),
                "verify-native-msave",
                str(native_first),
                str(native_second),
                str(native_verified),
            ]
        )
        require(native_verify.returncode == 0, native_verify.stderr)
        require(
            native_verified.read_bytes() == expected_native_jr8rom,
            "native verify JR8ROM differs",
        )
        require("recordings=2" in native_verify.stdout, native_verify.stdout)
        require(f"sha256={native_hash}" in native_verify.stdout, native_verify.stdout)

        verified_container = run(
            [str(args.jr8rom_tool), "verify", str(native_verified)]
        )
        require(verified_container.returncode == 0, verified_container.stderr)
        inspected_container = run(
            [str(args.jr8rom_tool), "inspect", str(native_verified)]
        )
        require(inspected_container.returncode == 0, inspected_container.stderr)
        require(
            inspected_container.stdout
            == (
                "JR8ROM 1.0 segments=1\n"
                "0\taddress=$8000\tlength=256\tend=$80FF\n"
            ),
            inspected_container.stdout,
        )

        filler_bytes = bytearray([0x01] * (32_768 - len(native_payload)))
        filler_bytes[-2:] = b"\x80\x00"
        filler_source = root / "filler.bin"
        filler_source.write_bytes(filler_bytes)
        filler_container = root / "filler.j8r"
        filler_result = run(
            [
                str(args.jr8rom_tool),
                "create",
                "-o",
                str(filler_container),
                "--segment",
                "0x8100",
                str(filler_source),
            ]
        )
        require(filler_result.returncode == 0, filler_result.stderr)
        runnable_container = root / "runnable.j8r"
        combined_result = run(
            [
                str(args.jr8rom_tool),
                "combine",
                "-o",
                str(runnable_container),
                str(native_verified),
                str(filler_container),
            ]
        )
        require(combined_result.returncode == 0, combined_result.stderr)
        runnable = run(
            [
                str(args.runner),
                "jr800",
                "--max-instructions",
                "1",
                str(runnable_container),
            ]
        )
        require(runnable.returncode == 0, runnable.stderr)
        require("stop=instruction-limit" in runnable.stdout, runnable.stdout)
        require("instructions=1" in runnable.stdout, runnable.stdout)
        require("cpu-fault=none" in runnable.stdout, runnable.stdout)

        different_native = root / "native-different.wav"
        different_payload = bytes([native_payload[0] ^ 1]) + native_payload[1:]
        write_native_msave_wav(
            different_native,
            different_payload,
            leading_silence=4_800,
        )
        mismatch_output = root / "native-mismatch.j8r"
        mismatch_output.write_bytes(b"unchanged")
        mismatch = run(
            [
                str(args.tool),
                "verify-native-msave",
                str(native_first),
                str(different_native),
                str(mismatch_output),
            ]
        )
        require(mismatch.returncode == 2, mismatch.stderr)
        require("recordings differ" in mismatch.stderr, mismatch.stderr)
        require(mismatch_output.read_bytes() == b"unchanged", "mismatch changed output")

        collision_output = root / "native-collision.j8r"
        collision_output.write_bytes(b"unchanged")
        native_collision = run(
            [
                str(args.tool),
                "verify-native-msave",
                str(native_first),
                str(native_first),
                str(collision_output),
            ]
        )
        require(native_collision.returncode == 1, "native collision was accepted")
        require(
            collision_output.read_bytes() == b"unchanged",
            "native collision changed output",
        )

        duplicate_native = root / "native-duplicate.wav"
        duplicate_native.write_bytes(native_first.read_bytes())
        duplicate_output = root / "native-duplicate.j8r"
        duplicate_output.write_bytes(b"unchanged")
        duplicate = run(
            [
                str(args.tool),
                "verify-native-msave",
                str(native_first),
                str(duplicate_native),
                str(duplicate_output),
            ]
        )
        require(duplicate.returncode == 2, "duplicate recording was accepted")
        require("files are identical" in duplicate.stderr, duplicate.stderr)
        require(
            duplicate_output.read_bytes() == b"unchanged",
            "duplicate recording changed output",
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
