#!/usr/bin/env python3
"""Build a uMod-compatible texture mod from 2011 APB menu art.

This script:
  1. Loads retail menu textures exported by UE Viewer (TGA).
  2. Computes the uMod/TexMod CRC32 hash for each one.
  3. Maps 2011 replacement textures to retail names.
  4. Renames the 2011 DDS files to <hash>.dds so uMod can load them.

uMod hashes the first (BitsPerPixel * Width * Height / 8) bytes of the
locked D3D9 surface.  For the ARGB menu textures in APB this is typically
32-bit BGRA data.
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


def compute_hash_from_tga(tga_path: Path) -> int:
    """Compute uMod hash from a TGA file, assuming 32-bit BGRA locked surface."""
    im = Image.open(tga_path)
    # Convert to BGRA (D3DFMT_A8R8G8B8 locked surface layout)
    if im.mode == "RGBA":
        r, g, b, a = im.split()
        bgra = Image.merge("RGBA", (b, g, r, a))
    elif im.mode == "RGB":
        r, g, b = im.split()
        bgra = Image.merge("RGB", (b, g, r))
    else:
        bgra = im.convert("RGBA")
        r, g, b, a = bgra.split()
        bgra = Image.merge("RGBA", (b, g, r, a))
    raw = bgra.tobytes()
    return umod_crc32(raw)


def main():
    retail_dir = Path(r"D:\APBReloaded\work\login_swap\mod\captures\retail_raw\APBMenus_Art_GameFlowScenes\Texture2D")
    replacement_dir = Path(r"D:\APBReloaded\work\login_swap\mod\textures\2011_dds")
    out_dir = Path(r"D:\APBReloaded\work\login_swap\mod\APB_2011_Menu_Mod_hashed")
    out_dir.mkdir(parents=True, exist_ok=True)

    # Map retail texture name (without extension) to 2011 replacement name
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
            h = compute_hash_from_tga(retail_path)
            hash_name = f"{h:08X}.dds"
            out_path = out_dir / hash_name
            # Copy replacement texture to hashed name
            out_path.write_bytes(replacement_path.read_bytes())
            report.append(f"{retail_name} -> {hash_name} (replacement: {replacement_name}.dds)")
        except Exception as e:
            report.append(f"[error] {retail_name}: {e}")

    (out_dir / "hash_report.txt").write_text("\n".join(report), encoding="utf-8")
    print("\n".join(report))
    print(f"\nHashed mod written to: {out_dir}")


if __name__ == "__main__":
    main()
