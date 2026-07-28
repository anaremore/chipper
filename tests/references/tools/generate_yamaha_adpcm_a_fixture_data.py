#!/usr/bin/env python3
"""Generate original synthetic Yamaha ADPCM-A reference inputs.

The byte patterns are authored for Chipper's validation suite. They contain no
music, game, ROM, or third-party sample data.
"""

from __future__ import annotations

import argparse
from pathlib import Path


OPNA_SIZE = 0x2000
OPNB_SIZE = 0x300


def xorshift32(value: int) -> int:
    value ^= (value << 13) & 0xFFFFFFFF
    value ^= value >> 17
    value ^= (value << 5) & 0xFFFFFFFF
    return value & 0xFFFFFFFF


def make_page(seed: int, variant: int) -> bytes:
    data = bytearray(0x100)
    nibble_walk = bytes((index << 4) | ((index + variant) & 0x0F) for index in range(16))
    data[0:64] = nibble_walk * 4
    data[64:96] = bytes([0x77]) * 32
    data[96:128] = bytes([0xFF]) * 32
    stress = (0x70, 0xF0, 0x0F, 0x87, 0x08, 0x80, 0x17, 0xE9)
    data[128:160] = bytes(stress[(index + variant) % len(stress)] for index in range(32))

    state = seed & 0xFFFFFFFF
    for index in range(160, len(data)):
        state = xorshift32(state)
        data[index] = ((state >> 24) ^ (index * (17 + variant))) & 0xFF
    return bytes(data)


def make_opna() -> bytes:
    data = bytearray(OPNA_SIZE)
    for page in range(OPNA_SIZE // 0x100):
        data[page * 0x100 : (page + 1) * 0x100] = make_page(
            0x26080000 ^ (page * 0x9E3779B9),
            page & 0x0F,
        )
    return bytes(data)


def make_opnb() -> bytes:
    page0 = make_page(0x261000A0, 1)
    page1 = bytes([0x88]) * 0x100
    page2 = make_page(0x261000A2, 11)
    return page0 + page1 + page2


def write_c_header(path: Path, data: bytes) -> None:
    physical_rom = bytes(((value & 0x0F) << 4) | (value >> 4) for value in data)
    lines = [
        "/* Generated first-party Chipper test bytes; not an upstream ROM.",
        " * YM2608-LLE exposes the internal ROM's physical low-nibble-first wiring,",
        " * so canonical high-nibble-first fixture bytes are swapped at this boundary. */",
        "static unsigned char rss_rom[8192] = {",
    ]
    for offset in range(0, len(physical_rom), 16):
        row = ", ".join(f"0x{value:02x}" for value in physical_rom[offset : offset + 16])
        lines.append(f"    {row},")
    lines.append("};")
    path.write_text("\n".join(lines) + "\n", encoding="ascii")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--opna", type=Path, required=True)
    parser.add_argument("--opnb", type=Path, required=True)
    parser.add_argument("--opna-header", type=Path)
    args = parser.parse_args()

    opna = make_opna()
    opnb = make_opnb()
    args.opna.parent.mkdir(parents=True, exist_ok=True)
    args.opnb.parent.mkdir(parents=True, exist_ok=True)
    args.opna.write_bytes(opna)
    args.opnb.write_bytes(opnb)
    if args.opna_header is not None:
        args.opna_header.parent.mkdir(parents=True, exist_ok=True)
        write_c_header(args.opna_header, opna)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
