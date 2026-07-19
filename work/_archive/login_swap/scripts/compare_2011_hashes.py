#!/usr/bin/env python3
"""Compare computed 2011 texture hashes to live uMod log hashes."""
from PIL import Image
from pathlib import Path

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


def hash_bgra(im: Image.Image) -> int:
    if im.mode == "RGBA":
        r, g, b, a = im.split()
        data = Image.merge("RGBA", (b, g, r, a)).tobytes()
    elif im.mode == "RGB":
        r, g, b = im.split()
        data = Image.merge("RGB", (b, g, r)).tobytes()
    else:
        im2 = im.convert("RGBA")
        r, g, b, a = im2.split()
        data = Image.merge("RGBA", (b, g, r, a)).tobytes()
    return umod_crc32(data)


def hash_rgba(im: Image.Image) -> int:
    return umod_crc32(im.convert("RGBA").tobytes())


def hash_bgr(im: Image.Image) -> int:
    if im.mode == "RGB":
        r, g, b = im.split()
        data = Image.merge("RGB", (b, g, r)).tobytes()
    else:
        im2 = im.convert("RGB")
        r, g, b = im2.split()
        data = Image.merge("RGB", (b, g, r)).tobytes()
    return umod_crc32(data)


def hash_rgb(im: Image.Image) -> int:
    return umod_crc32(im.convert("RGB").tobytes())


# Live hashes from uMod log for 2011 client
live_hashes = {
    "32x32_1": 0xC86E4F8A1C7C18E,  # lower 32 bits: 0x01C7C18E
    "512x512": 0x88F8B22114352471,  # lower 32 bits: 0x43552471
    "512x256": 0x16FC15C170EB8B7C,  # lower 32 bits: 0x70EB8B7C
    "32x32_2": 0xB14D41474375001A,  # lower 32 bits: 0x4375001A
}

src_dir = Path(r"D:\APBReloaded\work\login_swap\textures\2011\APBMenus_Art_GameFlowScenes\Texture2D")

print("Computed hashes for 2011 textures:")
for tga in src_dir.glob("*.tga"):
    im = Image.open(tga)
    if im.size not in [(32, 32), (512, 512), (512, 256)]:
        continue
    h_bgra = hash_bgra(im)
    h_rgba = hash_rgba(im)
    h_bgr = hash_bgr(im)
    h_rgb = hash_rgb(im)
    print(f"{tga.name} {im.size} {im.mode}: BGRA={h_bgra:08X} RGBA={h_rgba:08X} BGR={h_bgr:08X} RGB={h_rgb:08X}")

print("\nLive hashes (lower 32 bits):")
for name, h in live_hashes.items():
    print(f"  {name}: {h:016X} -> {h & 0xFFFFFFFF:08X}")
