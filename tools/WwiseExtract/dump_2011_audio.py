#!/usr/bin/env python3
"""Dump 2011 APB All Points Bulletin audio to WEM/WAV.

Sources under:
  2011 apb/.../APBGame/Content/Audio/

- SoundBanks/*_Media.bnk  → Banks/<name>/*.wem + wav/
- FilePackages/Music.pck  → Music/ (named from DefaultMusicLibrary)
- FilePackages/StreamedSFX.pck → StreamedSFX/
- FilePackages/Dialogue.pck → Dialogue/
- MusicStudio/**/*.wav already PCM — copied as-is

2011 AKPK entries are 24 bytes:
  u32 id, u32 flags, u32 size, u32 pad, u32 offset, u32 lang
"""
from __future__ import annotations

import argparse
import json
import re
import struct
import subprocess
import sys
import xml.etree.ElementTree as ET
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
AUDIO_2011 = ROOT / "2011 apb" / "APB All Points Bulletin" / "APB North America" / "APBGame" / "Content" / "Audio"
VGM = Path(__file__).resolve().parent / "vgmstream" / "vgmstream-cli.exe"
OUT_DEFAULT = ROOT / "Content" / "Extracted" / "Audio" / "2011"


def sanitize(name: str, max_len: int = 100) -> str:
    name = re.sub(r"[^\w\-.\(\)\[\] ]+", "_", name.strip())
    name = re.sub(r"\s+", "_", name)
    return (name or "unnamed")[:max_len]


def parse_media_txt_names(txt: Path) -> dict[int, str]:
    names: dict[int, str] = {}
    if not txt.is_file():
        return names
    for line in txt.read_text(errors="ignore").splitlines():
        parts = re.split(r"\t+", line.strip())
        # formats: "ID\tName\t..." or "\tID\tName\t..."
        nums = []
        for i, p in enumerate(parts):
            try:
                nums.append((i, int(p.strip())))
            except ValueError:
                continue
        if not nums:
            continue
        # prefer first integer that looks like a short-id / media id
        idx, wid = nums[0]
        if idx + 1 < len(parts):
            name = parts[idx + 1].strip()
            if name and not name.startswith("Z:") and name.lower() != "name":
                names[wid] = sanitize(name)
    return names


def parse_music_library(xml_path: Path) -> dict[int, str]:
    """LocalID -> Artist_TrackName from locallibrary.xml."""
    names: dict[int, str] = {}
    if not xml_path.is_file():
        return names
    try:
        root = ET.parse(xml_path).getroot()
    except ET.ParseError:
        return names
    for track in root.iter("Track"):
        try:
            lid = int(track.get("LocalID", "-1"))
        except ValueError:
            continue
        tname = track.get("TrackName") or track.get("TrackPath") or f"track_{lid}"
        artist = track.get("ArtistName") or ""
        label = f"{artist}_{tname}" if artist else tname
        names[lid] = sanitize(label)
    return names


def bnk_chunks(data: bytes) -> dict[str, bytes]:
    chunks: dict[str, bytes] = {}
    off = 0
    n = len(data)
    while off + 8 <= n:
        tag = data[off : off + 4]
        if not all(32 <= b < 127 for b in tag):
            break
        size = struct.unpack_from("<I", data, off + 4)[0]
        start = off + 8
        end = min(n, start + size)
        chunks[tag.decode("ascii", errors="replace")] = data[start:end]
        off = end
    return chunks


def extract_bnk_media(bnk: Path, out_dir: Path, name_map: dict[int, str]) -> int:
    if not bnk.is_file() or bnk.stat().st_size < 64:
        return 0
    raw = bnk.read_bytes()
    if raw[:4] != b"BKHD":
        return 0
    chunks = bnk_chunks(raw)
    didx, data = chunks.get("DIDX"), chunks.get("DATA")
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
        (out_dir / f"{wid}_{label}.wem").write_bytes(blob)
        count += 1
    return count


def parse_akpk_2011(data: bytes) -> list[tuple[int, int, int]]:
    """Return list of (id, size, offset) for 2011 24-byte AKPK tables."""
    if data[:4] != b"AKPK":
        raise ValueError("not AKPK")
    lang_size = struct.unpack_from("<I", data, 12)[0]
    table = 28 + lang_size
    count = struct.unpack_from("<I", data, table)[0]
    entries: list[tuple[int, int, int]] = []
    for i in range(count):
        # id, flags, size, pad, offset, lang
        wid, _flags, size, _pad, offset, _lang = struct.unpack_from(
            "<IIIIII", data, table + 4 + i * 24
        )
        if size > 0 and offset + size <= len(data):
            entries.append((wid, size, offset))
    return entries


def decode_one(vgm: Path, wem: Path, wav: Path) -> bool:
    if wav.is_file() and wav.stat().st_size > 44:
        return True
    if not vgm.is_file():
        return False
    r = subprocess.run(
        [str(vgm), "-o", str(wav), str(wem)],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    return r.returncode == 0 and wav.is_file() and wav.stat().st_size > 44


def decode_tree(vgm: Path, root: Path, workers: int) -> tuple[int, int]:
    """Decode all .wem under root into sibling wav/ or same-dir .wav."""
    wems = list(root.rglob("*.wem"))
    if not wems:
        return 0, 0
    ok = 0
    fail = 0

    def job(wem: Path) -> bool:
        # prefer package/wav/ subfolder when wem is directly under a package dir
        if wem.parent.name != "wav":
            wav_dir = wem.parent / "wav"
            wav_dir.mkdir(exist_ok=True)
            wav = wav_dir / (wem.stem + ".wav")
        else:
            wav = wem.with_suffix(".wav")
        return decode_one(vgm, wem, wav)

    with ThreadPoolExecutor(max_workers=max(1, workers)) as ex:
        futs = {ex.submit(job, w): w for w in wems}
        for i, fut in enumerate(as_completed(futs), 1):
            if fut.result():
                ok += 1
            else:
                fail += 1
            if i % 200 == 0 or i == len(wems):
                print(f"  decode progress {i}/{len(wems)} ok={ok} fail={fail}")
    return ok, fail


def extract_pck(
    pck: Path,
    out_dir: Path,
    name_map: dict[int, str],
    default_label: str = "stream",
) -> int:
    if not pck.is_file():
        print(f"missing pck {pck}")
        return 0
    data = pck.read_bytes()
    entries = parse_akpk_2011(data)
    out_dir.mkdir(parents=True, exist_ok=True)
    n = 0
    for wid, size, offset in entries:
        blob = data[offset : offset + size]
        label = name_map.get(wid, default_label)
        # extension by magic
        magic = blob[:4]
        if magic == b"RIFF":
            ext = ".wem"
        elif magic == b"OggS":
            ext = ".ogg"
        elif magic[:3] == b"ID3" or blob[:2] in (b"\xff\xfb", b"\xff\xf3", b"\xff\xfa", b"\xff\xf2"):
            ext = ".mp3"
        else:
            ext = ".bin"
        path = out_dir / f"{wid}_{label}{ext}"
        path.write_bytes(blob)
        n += 1
    print(f"PCK {pck.name}: {n} files -> {out_dir}")
    return n


def copy_music_studio(src: Path, dst: Path) -> int:
    if not src.is_dir():
        return 0
    n = 0
    for wav in src.rglob("*.wav"):
        rel = wav.relative_to(src)
        target = dst / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.is_file() or target.stat().st_size != wav.stat().st_size:
            target.write_bytes(wav.read_bytes())
        n += 1
    print(f"MusicStudio: copied {n} wav -> {dst}")
    return n


def main() -> int:
    ap = argparse.ArgumentParser(description="Dump 2011 APB audio to WAV")
    ap.add_argument("--audio", type=Path, default=AUDIO_2011, help="2011 Audio root")
    ap.add_argument("--out", type=Path, default=OUT_DEFAULT, help="Output root")
    ap.add_argument("--workers", type=int, default=8, help="vgmstream decode threads")
    ap.add_argument("--skip-dialogue", action="store_true", help="Skip huge Dialogue.pck")
    ap.add_argument("--music-only", action="store_true", help="Only Music.pck + MusicStudio + media banks")
    ap.add_argument("--no-decode", action="store_true", help="Extract containers only, no WAV decode")
    args = ap.parse_args()

    audio: Path = args.audio
    out: Path = args.out
    if not audio.is_dir():
        print(f"Audio root missing: {audio}", file=sys.stderr)
        return 1
    if not VGM.is_file() and not args.no_decode:
        print(f"WARN: vgmstream missing at {VGM} — will extract only", file=sys.stderr)

    out.mkdir(parents=True, exist_ok=True)
    banks_dir = audio / "SoundBanks"
    name_map: dict[int, str] = {}
    for txt in banks_dir.rglob("*.txt"):
        name_map.update(parse_media_txt_names(txt))
    music_names = parse_music_library(audio / "DefaultMusicLibrary" / "locallibrary.xml")
    print(f"name_map={len(name_map)} music_tracks={len(music_names)}")

    manifest: dict = {
        "source": str(audio),
        "out": str(out),
        "banks": [],
        "packages": {},
        "totals": {},
    }

    total_extracted = 0

    # --- media banks ---
    for bnk in sorted(banks_dir.rglob("*.bnk")):
        if bnk.stat().st_size <= 64:
            continue
        # Prefer *_Media.bnk; also try non-media if they have DIDX
        rel = bnk.relative_to(banks_dir)
        dest = out / "Banks" / rel.with_suffix("")
        n = extract_bnk_media(bnk, dest, name_map)
        if n:
            total_extracted += n
            manifest["banks"].append({"path": str(bnk), "wem": n, "out": str(dest)})
            print(f"BNK {rel}: {n} wem")

    # --- packages ---
    pkgs = audio / "FilePackages"
    music_n = extract_pck(pkgs / "Music.pck", out / "Music", music_names, default_label="music")
    total_extracted += music_n
    manifest["packages"]["Music.pck"] = {"files": music_n, "out": str(out / "Music")}

    if not args.music_only:
        sfx_n = extract_pck(pkgs / "StreamedSFX.pck", out / "StreamedSFX", name_map)
        total_extracted += sfx_n
        manifest["packages"]["StreamedSFX.pck"] = {"files": sfx_n, "out": str(out / "StreamedSFX")}
        if not args.skip_dialogue:
            dlg_n = extract_pck(pkgs / "Dialogue.pck", out / "Dialogue", name_map, default_label="dialogue")
            total_extracted += dlg_n
            manifest["packages"]["Dialogue.pck"] = {"files": dlg_n, "out": str(out / "Dialogue")}

    # --- MusicStudio already-wav ---
    studio_n = copy_music_studio(audio / "MusicStudio", out / "MusicStudio")

    # --- decode to wav ---
    total_wav = 0
    total_fail = 0
    if not args.no_decode and VGM.is_file():
        # Music: mp3/ogg/wem → wav via vgmstream
        for sub in ["Music", "StreamedSFX", "Dialogue", "Banks"]:
            d = out / sub
            if not d.is_dir():
                continue
            # rename non-wem to .wem temp? vgmstream opens by content; use actual paths
            print(f"Decoding under {d} ...")
            # collect decodable media
            media = []
            for ext in ("*.wem", "*.mp3", "*.ogg", "*.bin"):
                media.extend(d.rglob(ext))
            # filter .bin that look like audio
            jobs = []
            for m in media:
                if m.suffix == ".bin":
                    head = m.read_bytes()[:4]
                    if head not in (b"RIFF", b"OggS") and head[:2] not in (
                        b"\xff\xfb",
                        b"\xff\xf3",
                        b"\xff\xfa",
                    ) and head[:3] != b"ID3":
                        continue
                wav_dir = m.parent / "wav"
                wav_dir.mkdir(exist_ok=True)
                jobs.append((m, wav_dir / (m.stem + ".wav")))

            def job(pair: tuple[Path, Path]) -> bool:
                return decode_one(VGM, pair[0], pair[1])

            ok = fail = 0
            with ThreadPoolExecutor(max_workers=max(1, args.workers)) as ex:
                futs = {ex.submit(job, p): p for p in jobs}
                for i, fut in enumerate(as_completed(futs), 1):
                    if fut.result():
                        ok += 1
                    else:
                        fail += 1
                    if i % 250 == 0 or i == len(jobs):
                        print(f"  {sub}: {i}/{len(jobs)} ok={ok} fail={fail}")
            total_wav += ok
            total_fail += fail
            print(f"{sub}: decoded {ok}, failed {fail}")

    manifest["totals"] = {
        "extracted_media_files": total_extracted,
        "music_studio_wav_copied": studio_n,
        "wav_decoded": total_wav,
        "wav_decode_failed": total_fail,
        "name_map_size": len(name_map),
        "music_library_tracks": len(music_names),
    }
    man_path = out / "audio_dump_manifest.json"
    man_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    # human summary
    summary = [
        "2011 APB audio dump",
        f"source={audio}",
        f"out={out}",
        f"extracted={total_extracted}",
        f"music_studio_wav={studio_n}",
        f"wav_decoded={total_wav}",
        f"wav_failed={total_fail}",
        f"music_named={len(music_names)}",
    ]
    (out / "audio_dump_manifest.txt").write_text("\n".join(summary) + "\n", encoding="utf-8")
    print("\n".join(summary))
    print("manifest", man_path)
    return 0 if total_extracted > 0 else 2


if __name__ == "__main__":
    raise SystemExit(main())
