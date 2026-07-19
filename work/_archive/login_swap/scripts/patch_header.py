#!/usr/bin/env python3
"""Patch a decompressed UE3 APB package header to retail version/licensee."""
import struct
import sys
from pathlib import Path


def patch_header(src: Path, dst: Path, new_version: int, new_licensee: int) -> None:
    data = bytearray(src.read_bytes())
    if len(data) < 8:
        raise ValueError("file too small")

    tag, = struct.unpack_from('<I', data, 0)
    if tag != 0x9E2A83C1:
        raise ValueError(f"bad magic: 0x{tag:08X}")

    old_version, = struct.unpack_from('<H', data, 4)
    old_licensee, = struct.unpack_from('<H', data, 6)
    print(f"Original: version={old_version}, licensee={old_licensee}")

    # Patch version/licensee
    struct.pack_into('<H', data, 4, new_version)
    struct.pack_into('<H', data, 6, new_licensee)

    dst.write_bytes(data)
    print(f"Wrote patched header to {dst}")
    print(f"New: version={new_version}, licensee={new_licensee}")


if __name__ == '__main__':
    if len(sys.argv) < 5:
        print("usage: patch_header.py <src> <dst> <version> <licensee>")
        sys.exit(1)
    patch_header(Path(sys.argv[1]), Path(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]))
