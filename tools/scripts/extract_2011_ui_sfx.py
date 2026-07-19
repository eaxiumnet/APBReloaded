#!/usr/bin/env python3
"""Extract 2011 frontend UI sound effects to WAV.

M3 task: frontend UI sounds (button click, tab, scene open, popups, HUD beeps)
from Basic_Media.bnk / Main_Media.bnk -> Content/Extracted/2011/UISfx/*.wav

The *_Media.txt sidecars in the 2011 SoundBanks folder list every embedded
sound as:  <id>\\t<name>\\t<source path>\\t<wwise path>. Entries whose Wwise
path starts with \\UISounds\\ are the UI set (event names such as
Play_UIDefaultButtonClick reference these sounds).

Media is reused from the verified full bank dump at
Content/Extracted/Audio/2011/Banks/<bank>/<id>_<name>.wem (produced by
tools/WwiseExtract/dump_2011_audio.py); any missing id is re-extracted from
the source .bnk with tools/WwiseExtract/extract_bnk_wem.py logic. Decode to
WAV via the bundled vgmstream-cli.

Rerunnable: existing WAVs are skipped unless --force.
"""
from __future__ import annotations

import argparse
import json
import re
import struct
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOUNDBANKS = (
    ROOT
    / "2011 apb"
    / "APB All Points Bulletin"
    / "APB North America"
    / "APBGame"
    / "Content"
    / "Audio"
    / "SoundBanks"
)
BANK_DUMP = ROOT / "Content" / "Extracted" / "Audio" / "2011" / "Banks"
VGM = ROOT / "tools" / "WwiseExtract" / "vgmstream" / "vgmstream-cli.exe"
OUT = ROOT / "Content" / "Extracted" / "2011" / "UISfx"
UI_PATH_TAG = "\\UISounds\\"
BANKS = ("Basic_Media", "Main_Media")


def sanitize(name: str, max_len: int = 80) -> str:
    name = re.sub(r"[^\w\-.\(\)\[\] ]+", "_", name.strip())
    name = re.sub(r"\s+", "_", name)
    return (name or "unnamed")[:max_len]


def parse_ui_entries(txt: Path) -> dict[int, tuple[str, str]]:
    """id -> (name, wwise_path) for entries under \\UISounds\\."""
    out: dict[int, tuple[str, str]] = {}
    for line in txt.read_text(errors="ignore").splitlines():
        if UI_PATH_TAG not in line:
            continue
        parts = re.split(r"\t+", line.strip("\t\r\n "))
        nums = [(i, p.strip()) for i, p in enumerate(parts) if p.strip().isdigit()]
        if not nums:
            continue
        idx, sid = nums[0]
        name = parts[idx + 1].strip() if idx + 1 < len(parts) else "unnamed"
        wwise_path = next((p.strip() for p in parts if p.strip().startswith(UI_PATH_TAG)), "")
        out[int(sid)] = (name, wwise_path)
    return out


def extract_wem_from_bnk(bnk: Path, want_id: int, dest: Path) -> bool:
    """Minimal DIDX/DATA pull for a single id (same format as extract_bnk_wem.py)."""
    raw = bnk.read_bytes()
    off, didx, data = 0, None, None
    while off + 8 <= len(raw):
        tag = raw[off : off + 4]
        if not all(32 <= b < 127 for b in tag):
            break
        size = struct.unpack_from("<I", raw, off + 4)[0]
        payload = raw[off + 8 : off + 8 + size]
        if tag == b"DIDX":
            didx = payload
        elif tag == b"DATA":
            data = payload
        off += 8 + size
        if didx is not None and data is not None:
            break
    if didx is None or data is None:
        return False
    for i in range(0, len(didx) - 11, 12):
        wid, woff, wsize = struct.unpack_from("<III", didx, i)
        if wid == want_id:
            dest.write_bytes(data[woff : woff + wsize])
            return True
    return False


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--force", action="store_true")
    args = ap.parse_args()

    if not VGM.is_file():
        print("missing vgmstream:", VGM)
        return 1

    OUT.mkdir(parents=True, exist_ok=True)
    t0 = time.time()
    entries: dict[int, dict] = {}
    for bank in BANKS:
        for sid, (name, wpath) in parse_ui_entries(SOUNDBANKS / f"{bank}.txt").items():
            e = entries.setdefault(sid, {"name": name, "path": wpath, "banks": []})
            e["banks"].append(bank)

    manifest: dict[str, dict] = {}
    ok = fail = skipped = 0
    seen_names: dict[str, int] = {}
    for sid, e in sorted(entries.items(), key=lambda kv: kv[1]["name"]):
        name = sanitize(e["name"])
        # different ids can share a sound name across banks (e.g. two "Button4")
        if name in seen_names and seen_names[name] != sid:
            name = f"{name}_{sid}"
        seen_names.setdefault(name, sid)
        wav = OUT / f"{name}.wav"
        rec = {"id": sid, "banks": e["banks"], "wwise_path": e["path"], "wav": wav.name}
        if wav.is_file() and not args.force:
            rec["status"] = "exists"
            skipped += 1
            manifest[name] = rec
            continue
        # locate wem in the existing bank dump, else re-extract from source bnk
        wem = None
        for bank in e["banks"]:
            cand = list((BANK_DUMP / bank).glob(f"{sid}_*.wem"))
            if cand:
                wem = cand[0]
                break
        tmp_wem = None
        if wem is None:
            for bank in e["banks"]:
                tmp = OUT / f"_tmp_{sid}.wem"
                if extract_wem_from_bnk(SOUNDBANKS / f"{bank}.bnk", sid, tmp):
                    wem = tmp_wem = tmp
                    break
        if wem is None:
            rec["status"] = "wem_missing"
            fail += 1
            manifest[name] = rec
            continue
        r = subprocess.run(
            [str(VGM), "-o", str(wav), str(wem)], capture_output=True, text=True
        )
        if tmp_wem is not None:
            tmp_wem.unlink(missing_ok=True)
        if r.returncode == 0 and wav.is_file() and wav.stat().st_size > 44:
            rec["status"] = "ok"
            rec["wav_bytes"] = wav.stat().st_size
            ok += 1
        else:
            rec["status"] = "decode_fail"
            rec["stderr"] = (r.stderr or "")[-200:]
            fail += 1
        manifest[name] = rec

    doc = {
        "source_banks": [str(SOUNDBANKS / f"{b}.bnk") for b in BANKS],
        "wem_source": str(BANK_DUMP),
        "out": str(OUT),
        "ui_sound_count": len(entries),
        "decoded": ok,
        "skipped_existing": skipped,
        "failed": fail,
        "elapsed_sec": round(time.time() - t0, 2),
        "sounds": manifest,
    }
    (OUT / "uisfx_manifest.json").write_text(json.dumps(doc, indent=2), encoding="utf-8")
    print(f"ui_sounds={len(entries)} decoded={ok} skipped={skipped} failed={fail} -> {OUT}")
    return 0 if fail == 0 else 2


if __name__ == "__main__":
    raise SystemExit(main())
