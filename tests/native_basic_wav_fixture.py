# SPDX-License-Identifier: MIT
"""Project-authored waveform fixtures from the documented native block contract."""
import array
import sys
import wave


def basic_blocks(source: bytes, binary: bool = False, name: bytes = b"BASIC") -> list[bytes]:
    header = bytearray(32)
    header[0] = 2 if binary else 3
    header[1:1 + len(name)] = name
    if binary:
        header[18:20] = len(source).to_bytes(2, "big")
        header[20:22] = (0x2800).to_bytes(2, "big")
        return [bytes(header), source]
    blocks = [bytes(header)]
    payload = source + b"\x1a"
    count = len(payload) // 256 + 1
    for index in range(count):
        blocks.append(bytes([int(index == count - 1)]) + payload[index * 256:(index + 1) * 256].ljust(256, b"\0"))
    return blocks


def write_blocks(path, blocks: list[bytes], bad_checksum: int | None = None):
    signal = array.array("h", [0] * 4800)
    def cycle(long):
        size = 21 if long else 11
        signal.extend([12000] * size)
        signal.extend([-12000] * size)
    for index, body in enumerate(blocks):
        block = body + ((sum(body) + int(index == bad_checksum)) & 65535).to_bytes(2, "big")
        for _ in range(4000): cycle(False)
        sync = 40 if index == 0 else 20
        for _ in range(sync): cycle(True)
        for _ in range(sync): cycle(False)
        cycle(True); cycle(True)
        for value in block:
            for bit in range(7,-1,-1): cycle(bool(value & (1 << bit)))
            cycle(True)
        signal.extend([0] * 4800)
    if sys.byteorder != "little": signal.byteswap()
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(48000)
        output.writeframes(signal.tobytes())
