#!/usr/bin/env python3
"""Extract APB login theme (APB_ThemePreMaster) and Scaleform district music from StreamedSFX.pck."""
from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

STEAM_PCK = Path(
    r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded"
    r"\APBGame\Content\Audio\FilePackages\StreamedSFX.pck"
)
VGM = Path(__file__).resolve().parent / "vgmstream" / "vgmstream-cli.exe"
OUT_CONTENT = Path(r"D:\APBReloaded\Content\Audio")
OUT_EXTRACT = Path(r"D:\APBReloaded\Content\Extracted\Audio")

# short_id -> name (from Main_Media / Scaleform_Media txt indexes)
TARGETS = {
    540780953: "APB_ThemePreMaster",  # Play_NewThemeMusic / LoginBackgroundSound
    43619621: "Beach_Music",
    411783285: "Beltane_Music",
    631057005: "SkatePark_Music",
    877752351: "SkatePark_Music02",
    947106994: "CrimScene_Music",
}


def parse_sounds_lut(data: bytes) -> list[tuple[int, int, int, int, int]]:
    assert data[:4] == b"AKPK"
    lang_size, banks_size, sounds_size, _ext = struct.unpack_from("<IIII", data, 12)
    off = 28 + lang_size + banks_size
    count = struct.unpack_from("<I", data, off)[0]
    base = off + 4
    entries = []
    for i in range(count):
        e = base + i * 20
        entries.append(struct.unpack_from("<IIIII", data, e))
    return entries


def main() -> int:
    if not STEAM_PCK.is_file():
        print("missing", STEAM_PCK, file=sys.stderr)
        return 1
    if not VGM.is_file():
        print("missing vgmstream", VGM, file=sys.stderr)
        return 1

    data = STEAM_PCK.read_bytes()
    OUT_CONTENT.mkdir(parents=True, exist_ok=True)
    OUT_EXTRACT.mkdir(parents=True, exist_ok=True)

    found = {e[0]: e for e in parse_sounds_lut(data)}
    for wid, name in TARGETS.items():
        if wid not in found:
            print("MISSING id", wid, name)
            continue
        _id, _f1, size, offset, _f4 = found[wid]
        blob = data[offset : offset + size]
        wem = OUT_EXTRACT / f"{wid}_{name}.wem"
        wem.write_bytes(blob)
        wav = OUT_EXTRACT / f"{name}.wav"
        r = subprocess.run([str(VGM), "-o", str(wav), str(wem)], capture_output=True, text=True)
        if r.returncode != 0 or not wav.is_file():
            print("decode fail", name, r.stderr[-300:])
            continue
        print(f"OK {name}: {wav.stat().st_size} bytes")
        if name == "APB_ThemePreMaster":
            dest = OUT_CONTENT / "LoginTheme_APB_ThemePreMaster.wav"
            dest.write_bytes(wav.read_bytes())
            print("  ->", dest)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
