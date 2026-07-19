#!/usr/bin/env python3
"""Convert a decompressed APB package from version 547/31 to 564/33.

UE3 APB added an extra FGuid (16 bytes) after the Generations array for
licensee version >= 32. This script inserts that Guid and updates the
version/licensee fields.
"""
import struct
import sys
from pathlib import Path


def read_fstring(data: bytes, pos: int):
    length, = struct.unpack_from('<i', data, pos)
    pos += 4
    if length == 0:
        return '', pos
    if length < 0:
        length = -length
        s = data[pos:pos + length * 2].decode('utf-16-le', errors='replace').rstrip('\x00')
        pos += length * 2
        return s, pos
    else:
        s = data[pos:pos + length].decode('latin-1', errors='replace').rstrip('\x00')
        pos += length
        return s, pos


def convert(src: Path, dst: Path, guid2: bytes = b'\x00' * 16) -> None:
    data = bytearray(src.read_bytes())
    pos = 0

    tag, = struct.unpack_from('<I', data, pos); pos += 4
    file_version, = struct.unpack_from('<H', data, pos); pos += 2
    licensee_version, = struct.unpack_from('<H', data, pos); pos += 2
    if tag != 0x9E2A83C1:
        raise ValueError(f"bad magic: 0x{tag:08X}")
    print(f"Original: version={file_version}, licensee={licensee_version}")

    headers_size, = struct.unpack_from('<I', data, pos); pos += 4
    # skip package group
    length, = struct.unpack_from('<i', data, pos)
    if length < 0:
        length = -length
        pos += 4 + length * 2
    elif length > 0:
        pos += 4 + length
    else:
        pos += 4

    pos += 4  # PackageFlags
    name_count, = struct.unpack_from('<I', data, pos); pos += 4
    name_offset, = struct.unpack_from('<I', data, pos); pos += 4
    export_count, = struct.unpack_from('<I', data, pos); pos += 4
    export_offset, = struct.unpack_from('<I', data, pos); pos += 4

    # APB extras
    if licensee_version >= 29:
        pos += 4
    if licensee_version >= 28:
        pos += 20

    import_count, = struct.unpack_from('<I', data, pos); pos += 4
    import_offset, = struct.unpack_from('<I', data, pos); pos += 4

    if file_version >= 415:
        pos += 4
    if file_version >= 584:
        pos += 4

    pos += 16  # Guid
    generation_count, = struct.unpack_from('<I', data, pos); pos += 4
    print(f"GenerationCount={generation_count}, header pos after generation_count={pos}")

    new_data = bytearray(data[:pos]) + bytearray(guid2) + bytearray(data[pos:])

    # Update version/licensee
    struct.pack_into('<H', new_data, 4, 564)
    struct.pack_into('<H', new_data, 6, 33)

    dst.write_bytes(new_data)
    print(f"Wrote {dst} ({len(new_data)} bytes)")


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("usage: convert_547_to_564.py <src> <dst> [guid2_hex]")
        sys.exit(1)
    guid2 = b'\x00' * 16
    if len(sys.argv) >= 4:
        guid2 = bytes.fromhex(sys.argv[3])
    convert(Path(sys.argv[1]), Path(sys.argv[2]), guid2)
