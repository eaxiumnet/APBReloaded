#!/usr/bin/env python3
"""Dump extractable Wwise media from current Steam APB install.

- StreamedSFX.pck: all AKPK sound entries (WEM + decode WAV when possible)
- All *_Media.bnk with DIDX+DATA: extract WEMs
- Named map from bank *_Media.txt indexes
- Manifest with counts, stubs, login theme path
"""
from __future__ import annotations

import json
import re
import struct
import subprocess
import sys
from pathlib import Path

STEAM_AUDIO = Path(
    r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Content\Audio"
)
VGM = Path(__file__).resolve().parent / "vgmstream" / "vgmstream-cli.exe"
OUT_ROOT = Path(r"D:\APBReloaded\Content\Extracted\Audio")
OUT_CONTENT = Path(r"D:\APBReloaded\Content\Audio")
SCRATCH_DEFAULT = Path(r"C:\Users\Support\AppData\Local\Temp\grok-goal-5afeb5477713\implementer")

# Known important names (login + SF district)
KNOWN = {
    540780953: "APB_ThemePreMaster",
    43619621: "Beach_Music",
    411783285: "Beltane_Music",
    631057005: "SkatePark_Music",
    877752351: "SkatePark_Music02",
    947106994: "CrimScene_Music",
}


def parse_media_txt_names(txt: Path) -> dict[int, str]:
    names: dict[int, str] = {}
    if not txt.is_file():
        return names
    for line in txt.read_text(errors="ignore").splitlines():
        # id \t name \t ...
        parts = re.split(r"\t+", line.strip())
        if len(parts) < 2:
            continue
        try:
            wid = int(parts[0].strip())
        except ValueError:
            continue
        name = parts[1].strip()
        if name:
            names[wid] = re.sub(r"[^\w\-.]+", "_", name)[:80]
    return names


def bnk_chunks(data: bytes) -> list[tuple[str, bytes]]:
    chunks = []
    off = 0
    n = len(data)
    while off + 8 <= n:
        tag = data[off : off + 4]
        if not all(32 <= b < 127 for b in tag):
            break
        size = struct.unpack_from("<I", data, off + 4)[0]
        start = off + 8
        end = min(n, start + size)
        chunks.append((tag.decode("ascii", errors="replace"), data[start:end]))
        off = end
    return chunks


def extract_bnk_media(bnk: Path, out_dir: Path, name_map: dict[int, str]) -> int:
    if not bnk.is_file() or bnk.stat().st_size < 64:
        return 0
    raw = bnk.read_bytes()
    if raw[:4] != b"BKHD":
        return 0
    chunks = dict(bnk_chunks(raw))
    didx = chunks.get("DIDX")
    data = chunks.get("DATA")
    if not didx or not data:
        return 0
    out_dir.mkdir(parents=True, exist_ok=True)
    count = 0
    for i in range(0, len(didx), 12):
        if i + 12 > len(didx):
            break
        wid, woff, wsize = struct.unpack_from("<III", didx, i)
        blob = data[woff : woff + wsize]
        if len(blob) != wsize or wsize == 0:
            continue
        label = name_map.get(wid, "unnamed")
        path = out_dir / f"{wid}_{label}.wem"
        path.write_bytes(blob)
        count += 1
    return count


def parse_pck_entries(data: bytes) -> list[tuple[int, int, int, int, int]]:
    assert data[:4] == b"AKPK"
    lang_size, banks_size, sounds_size, _ext = struct.unpack_from("<IIII", data, 12)
    off = 28 + lang_size + banks_size
    count = struct.unpack_from("<I", data, off)[0]
    base = off + 4
    entries = []
    for i in range(count):
        entries.append(struct.unpack_from("<IIIII", data, base + i * 20))
    return entries


def main() -> int:
    scratch = Path(sys.argv[1]) if len(sys.argv) > 1 else SCRATCH_DEFAULT
    scratch.mkdir(parents=True, exist_ok=True)
    OUT_ROOT.mkdir(parents=True, exist_ok=True)

    name_map: dict[int, str] = dict(KNOWN)
    banks_dir = STEAM_AUDIO / "SoundBanks"
    for txt in banks_dir.rglob("*_Media.txt"):
        name_map.update(parse_media_txt_names(txt))

    manifest: dict = {
        "steam_audio": str(STEAM_AUDIO),
        "out_root": str(OUT_ROOT),
        "banks": [],
        "streamed_sfx": {},
        "login_theme": None,
        "stubs": [],
        "totals": {},
    }

    total_wem = 0
    total_wav = 0

    # Media banks
    for bnk in sorted(banks_dir.rglob("*_Media.bnk")):
        sz = bnk.stat().st_size
        if sz <= 64:
            manifest["stubs"].append({"path": str(bnk), "size": sz, "note": "empty/stub media bank"})
            continue
        rel = bnk.relative_to(banks_dir)
        out = OUT_ROOT / "Banks" / rel.with_suffix("")
        n = extract_bnk_media(bnk, out, name_map)
        total_wem += n
        manifest["banks"].append({"path": str(bnk), "size": sz, "extracted_wem": n, "out": str(out)})
        print(f"BNK {rel}: {n} wems")

    # StreamedSFX.pck full dump
    pck = STEAM_AUDIO / "FilePackages" / "StreamedSFX.pck"
    pck_out = OUT_ROOT / "StreamedSFX"
    pck_out.mkdir(parents=True, exist_ok=True)
    decode_dir = pck_out / "wav"
    decode_dir.mkdir(exist_ok=True)
    pck_count = 0
    decode_ok = 0
    if pck.is_file():
        data = pck.read_bytes()
        entries = parse_pck_entries(data)
        # Prefer decoding known + large music-like (>150KB RIFF) first; then all WEM dump
        for wid, f1, size, offset, f4 in entries:
            blob = data[offset : offset + size]
            if len(blob) != size:
                continue
            label = name_map.get(wid, "stream")
            wem_path = pck_out / f"{wid}_{label}.wem"
            wem_path.write_bytes(blob)
            pck_count += 1
            total_wem += 1
            # Decode all RIFF Wwise (music often RIFF); skip if vgm missing
            if VGM.is_file() and blob[:4] == b"RIFF":
                wav_path = decode_dir / f"{wid}_{label}.wav"
                if not wav_path.is_file():
                    r = subprocess.run(
                        [str(VGM), "-o", str(wav_path), str(wem_path)],
                        capture_output=True,
                        text=True,
                    )
                    if r.returncode == 0 and wav_path.is_file():
                        decode_ok += 1
                        total_wav += 1
                        if wid == 540780953 or label == "APB_ThemePreMaster":
                            dest = OUT_CONTENT / "LoginTheme_APB_ThemePreMaster.wav"
                            dest.write_bytes(wav_path.read_bytes())
                            manifest["login_theme"] = str(dest)
                elif wid == 540780953:
                    dest = OUT_CONTENT / "LoginTheme_APB_ThemePreMaster.wav"
                    dest.write_bytes(wav_path.read_bytes())
                    manifest["login_theme"] = str(dest)
                    total_wav += 1
                    decode_ok += 1
        manifest["streamed_sfx"] = {
            "path": str(pck),
            "entries": len(entries),
            "extracted_wem": pck_count,
            "decoded_wav": decode_ok,
            "out": str(pck_out),
        }
        print(f"StreamedSFX: {pck_count} wem, {decode_ok} wav")
    else:
        manifest["streamed_sfx"] = {"error": "missing pck"}

    # Ensure theme path recorded
    theme = OUT_CONTENT / "LoginTheme_APB_ThemePreMaster.wav"
    if theme.is_file():
        manifest["login_theme"] = str(theme)

    # Copy SF named wavs to Scaleform folder for convenience
    sf = OUT_ROOT / "Scaleform"
    sf.mkdir(exist_ok=True)
    for wid, name in KNOWN.items():
        src = decode_dir / f"{wid}_{name}.wav"
        if not src.is_file():
            # alternate name patterns
            for p in decode_dir.glob(f"{wid}_*.wav"):
                src = p
                break
        if src.is_file():
            (sf / f"{name}.wav").write_bytes(src.read_bytes())

    manifest["totals"] = {
        "wem_files": total_wem,
        "wav_decoded": total_wav,
        "named_index_entries": len(name_map),
    }

    man_path = scratch / "audio_dump_manifest.txt"
    lines = [
        "APB Steam audio dump manifest",
        f"steam={STEAM_AUDIO}",
        f"out={OUT_ROOT}",
        f"login_theme={manifest.get('login_theme')}",
        f"total_wem={total_wem}",
        f"total_wav_decoded={total_wav}",
        f"name_map_size={len(name_map)}",
        f"streamed_sfx_entries={manifest['streamed_sfx'].get('entries')}",
        f"streamed_sfx_wem={manifest['streamed_sfx'].get('extracted_wem')}",
        f"streamed_sfx_wav={manifest['streamed_sfx'].get('decoded_wav')}",
        "stubs:",
    ]
    for s in manifest["stubs"]:
        lines.append(f"  STUB {s['path']} size={s['size']}")
    lines.append("banks:")
    for b in manifest["banks"]:
        lines.append(f"  {b['path']} wem={b['extracted_wem']} size={b['size']}")
    lines.append("known_login_not_DefaultMusicLibrary=APB_ThemePreMaster")
    man_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    (scratch / "audio_dump_manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print("manifest", man_path)
    print("totals", manifest["totals"])
    return 0 if total_wem > 100 else 2


if __name__ == "__main__":
    raise SystemExit(main())
