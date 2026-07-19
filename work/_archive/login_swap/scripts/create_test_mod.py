#!/usr/bin/env python3
"""Create a test mod that replaces a 2011 texture with a solid color.

This proves the uMod hash computation works end-to-end in the 2011 client.
"""
from PIL import Image
from pathlib import Path


def umod_crc32(data: bytes) -> int:
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


def main():
    # Use a small 2011 texture
    src_path = Path(r"D:\APBReloaded\work\login_swap\textures\2011\APBMenus_Art_GameFlowScenes\Texture2D\CharacterSelectIcon.tga")
    im = Image.open(src_path)
    print(f"Source: {src_path.name} {im.size} {im.mode}")

    # Compute hash assuming BGRA locked surface
    if im.mode == "RGBA":
        r, g, b, a = im.split()
        bgra = Image.merge("RGBA", (b, g, r, a))
    elif im.mode == "RGB":
        r, g, b = im.split()
        bgra = Image.merge("RGB", (b, g, r))
    else:
        im2 = im.convert("RGBA")
        r, g, b, a = im2.split()
        bgra = Image.merge("RGBA", (b, g, r, a))

    data = bgra.tobytes()
    h = umod_crc32(data)
    print(f"Computed hash (BGRA): {h:08X}")

    # Create a bright red replacement texture of the same size
    red = Image.new("RGBA", im.size, (255, 0, 0, 255))
    out_dir = Path(r"D:\APBReloaded\work\login_swap\mod\test_mod")
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / f"{h:08X}.dds"
    red.save(out_path, "DDS")
    print(f"Test mod written to: {out_path}")
    print("Load this file into uMod and the CharacterSelectIcon should turn bright red.")


if __name__ == "__main__":
    main()
