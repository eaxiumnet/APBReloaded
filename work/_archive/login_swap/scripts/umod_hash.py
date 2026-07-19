#!/usr/bin/env python3
"""Compute uMod/TexMod CRC32 texture hashes.

uMod hashes BytesPerPixel * Width * Height bytes from the start of the
locked D3D9 texture surface.  The CRC32 polynomial is 0xEDB88320 and the
initial value is 0xFFFFFFFF.
"""
import struct
import sys
from pathlib import Path


def umod_crc32(data: bytes) -> int:
    """Return the uMod CRC32 of the given byte string."""
    crc = 0xFFFFFFFF
    for byte in data:
        data_val = byte
        for _ in range(8):
            bit = (crc ^ data_val) & 1
            crc >>= 1
            if bit:
                crc ^= 0xEDB88320
            data_val >>= 1
    return crc


def bits_per_pixel_from_d3dformat(fmt: str) -> int:
    """Return bits per pixel for a D3D9 format name (approximate uMod logic)."""
    fmt = fmt.upper()
    if fmt in ("D3DFMT_A1",):
        return 1
    if fmt in ("D3DFMT_R3G3B2", "D3DFMT_A8", "D3DFMT_A8P8", "D3DFMT_P8",
               "D3DFMT_L8", "D3DFMT_A4L4", "D3DFMT_FORCE_DWORD",
               "D3DFMT_S8_LOCKABLE"):
        return 8
    if fmt in ("D3DFMT_D16_LOCKABLE", "D3DFMT_D15S1", "D3DFMT_L6V5U5",
               "D3DFMT_V8U8", "D3DFMT_CxV8U8", "D3DFMT_R5G6B5",
               "D3DFMT_X1R5G5B5", "D3DFMT_A1R5G5B5", "D3DFMT_A4R4G4B4",
               "D3DFMT_A8R3G3B2", "D3DFMT_X4R4G4B4", "D3DFMT_L16",
               "D3DFMT_R16F", "D3DFMT_A8L8", "D3DFMT_D16", "D3DFMT_INDEX16",
               "D3DFMT_G8R8_G8B8", "D3DFMT_R8G8_B8G8", "D3DFMT_UYVY",
               "D3DFMT_YUY2"):
        return 16
    if fmt == "D3DFMT_R8G8B8":
        return 24
    if fmt in ("D3DFMT_R32F", "D3DFMT_X8L8V8U8", "D3DFMT_A2W10V10U10",
               "D3DFMT_Q8W8V8U8", "D3DFMT_V16U16", "D3DFMT_A8R8G8B8",
               "D3DFMT_X8R8G8B8", "D3DFMT_A2B10G10R10", "D3DFMT_A8B8G8R8",
               "D3DFMT_X8B8G8R8", "D3DFMT_G16R16", "D3DFMT_G16R16F",
               "D3DFMT_A2R10G10B10", "D3DFMT_D32", "D3DFMT_D24S8",
               "D3DFMT_D24X8", "D3DFMT_D24X4S4", "D3DFMT_D32F_LOCKABLE",
               "D3DFMT_D24FS8", "D3DFMT_D32_LOCKABLE", "D3DFMT_INDEX32"):
        return 32
    if fmt in ("D3DFMT_G32R32F", "D3DFMT_Q16W16V16U16", "D3DFMT_A16B16G16R16",
               "D3DFMT_A16B16G16R16F"):
        return 64
    if fmt == "D3DFMT_A32B32G32R32F":
        return 128
    if fmt in ("D3DFMT_DXT2", "D3DFMT_DXT3", "D3DFMT_DXT4", "D3DFMT_DXT5"):
        return 8
    if fmt == "D3DFMT_DXT1":
        return 4
    # default / compressed
    return 4


def hash_texture(data: bytes, width: int, height: int, bits_per_pixel: int) -> int:
    """Compute uMod hash for a texture given raw locked data."""
    byte_count = (bits_per_pixel * width * height) // 8
    if byte_count > len(data):
        raise ValueError(f"Need {byte_count} bytes, got {len(data)}")
    return umod_crc32(data[:byte_count])


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("usage: umod_hash.py <file> [width] [height] [bits_per_pixel]")
        sys.exit(1)
    path = Path(sys.argv[1])
    data = path.read_bytes()
    if len(sys.argv) >= 5:
        width = int(sys.argv[2])
        height = int(sys.argv[3])
        bpp = int(sys.argv[4])
        h = hash_texture(data, width, height, bpp)
        print(f"{h:08X}")
    else:
        h = umod_crc32(data)
        print(f"{h:08X}")
