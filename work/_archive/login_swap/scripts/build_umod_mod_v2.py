#!/usr/bin/env python3
"""Build a uMod-compatible texture mod from 2011 APB menu art.

Computes uMod/TexMod CRC32 hashes for retail textures using the correct
locked-surface byte layout (BGRA for RGBA source, BGR for RGB source), and
also emits fallback hashes for other common layouts.
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


def main():
    retail_dir = Path(r"D:\APBReloaded\work\login_swap\mod\captures\retail_raw\APBMenus_Art_GameFlowScenes\Texture2D")
    replacement_dir = Path(r"D:\APBReloaded\work\login_swap\mod\textures\2011_dds")
    out_dir = Path(r"D:\APBReloaded\work\login_swap\mod\APB_2011_Menu_Mod_hashed_v2")
    out_dir.mkdir(parents=True, exist_ok=True)

    name_map = {
        "Constant_BG": "Constant_BG",
        "NewBackgroundImage": "Constant_BG",
        "LoadingScreen_APB": "LoadingScreen_APB",
        "LoadingScreen_APBFlame_Alpha": "LoadingScreen_APBFlame_Alpha",
        "CriminalFactionicon": "CriminalFactionicon",
        "CriminalFactionicon_Unselected": "CriminalFactionicon_Unselected",
        "EnforcerFactionicon": "EnforcerFactionicon",
        "EnforcerFactionicon_Unselected": "EnforcerFactionicon_Unselected",
        "factionheadericon": "factionheadericon",
        "FactionSelectbulletpoint": "FactionSelectbulletpoint",
        "FactionSelectbulletpoint_Unselected": "FactionSelectbulletpoint_Unselected",
        "factionselectbuttongrey": "factionselectbuttongrey",
        "frontendFooter": "frontendFooter",
        "JKICON_login_header_key": "JKICON_login_header_key",
        "LoadingArrows_BG": "LoadingArrows_BG",
        "LoadingArrows_Mask": "LoadingArrows_Mask",
        "LoadingArrows_Ring": "LoadingArrows_Ring",
        "LoadingIcon_MAIN": "LoadingIcon_MAIN",
        "newCriminalcon": "newCriminalcon",
        "newEnforcerIcon": "newEnforcerIcon",
        "splatter1": "splatter1",
        "worldselecticon": "worldselecticon",
        "ArcIcon64x64": "CharacterSelectIcon",
        "EpicGamesIcon64x64": "CharacterSelectIcon",
        "KongregateIcon64x64": "CharacterSelectIcon",
        "SteamIcon64x64": "CharacterSelectIcon",
        "KongregateLogo": "LoadingScreen_APB",
        "SteamLogo": "LoadingScreen_APB",
        "Login_APB_Logo": "LoadingScreen_APB",
    }

    report = []
    for retail_name, replacement_name in name_map.items():
        retail_path = retail_dir / (retail_name + ".tga")
        replacement_path = replacement_dir / (replacement_name + ".dds")
        if not retail_path.exists():
            report.append(f"[missing retail] {retail_name}")
            continue
        if not replacement_path.exists():
            report.append(f"[missing 2011] {replacement_name}")
            continue

        try:
            im = Image.open(retail_path)
            # Primary hash: BGRA/BGR depending on mode
            primary_hash = hash_bgra(im)
            # Fallback hashes
            fallback_hashes = {
                "RGBA": hash_rgba(im),
                "BGR": hash_bgr(im),
                "RGB": hash_rgb(im),
            }

            # Copy primary
            out_path = out_dir / f"{primary_hash:08X}.dds"
            out_path.write_bytes(replacement_path.read_bytes())
            report.append(f"{retail_name} -> {primary_hash:08X}.dds (mode={im.mode}, size={im.size}, replacement={replacement_name}.dds)")

            # Copy fallbacks with descriptive names (not loaded by uMod, but useful for debugging)
            for layout, h in fallback_hashes.items():
                if h != primary_hash:
                    fb_path = out_dir / f"{h:08X}_{retail_name}_{layout}.dds"
                    fb_path.write_bytes(replacement_path.read_bytes())

        except Exception as e:
            report.append(f"[error] {retail_name}: {e}")

    (out_dir / "hash_report.txt").write_text("\n".join(report), encoding="utf-8")
    print("\n".join(report))
    print(f"\nHashed mod written to: {out_dir}")


if __name__ == "__main__":
    main()
