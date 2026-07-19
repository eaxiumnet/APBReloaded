#!/usr/bin/env python3
"""Compute uMod hashes for a texture under multiple D3D9 locked-surface assumptions.

uMod hashes the first (BitsPerPixel * Width * Height / 8) bytes of the locked
surface.  The exact byte layout depends on the D3D9 format and pitch.  This
script generates hashes for every plausible layout so the user can try them
all if the first one fails.
"""
import struct
import sys
from pathlib import Path
from PIL import Image


CRC32POLY = 0xEDB88320


def umod_crc32(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for byte in data:
        data_val = byte
        for _ in range(8):
            bit = (crc ^ data_val) & 1
            crc >>= 1
            if bit:
                crc ^= CRC32POLY
            data_val >>= 1
    return crc


def raw_bgra(im: Image.Image) -> bytes:
    r, g, b, a = im.split()
    return Image.merge("RGBA", (b, g, r, a)).tobytes()


def raw_rgba(im: Image.Image) -> bytes:
    return im.convert("RGBA").tobytes()


def raw_bgr(im: Image.Image) -> bytes:
    rgb = im.convert("RGB")
    r, g, b = rgb.split()
    return Image.merge("RGB", (b, g, r)).tobytes()


def raw_rgb(im: Image.Image) -> bytes:
    return im.convert("RGB").tobytes()


def raw_argb(im: Image.Image) -> bytes:
    r, g, b, a = im.split()
    return Image.merge("RGBA", (a, r, g, b)).tobytes()


def raw_abgr(im: Image.Image) -> bytes:
    r, g, b, a = im.split()
    return Image.merge("RGBA", (a, b, g, r)).tobytes()


def compute_hashes(tga_path: Path) -> dict:
    im = Image.open(tga_path)
    w, h = im.size
    layouts = {
        "BGRA": raw_bgra,
        "RGBA": raw_rgba,
        "BGR": raw_bgr,
        "RGB": raw_rgb,
        "ARGB": raw_argb,
        "ABGR": raw_abgr,
    }
    results = {}
    for name, fn in layouts.items():
        try:
            data = fn(im)
            results[name] = umod_crc32(data)
        except Exception as e:
            results[name] = f"error: {e}"
    return w, h, results


def main():
    if len(sys.argv) < 2:
        print("usage: compute_all_umod_hashes.py <tga_file>")
        sys.exit(1)
    path = Path(sys.argv[1])
    w, h, hashes = compute_hashes(path)
    print(f"{path.name}: {w}x{h}")
    for name, hsh in hashes.items():
        if isinstance(hsh, int):
            print(f"  {name}: {hsh:08X}")
        else:
            print(f"  {name}: {hsh}")


if __name__ == "__main__":
    main()
