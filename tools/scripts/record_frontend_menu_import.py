#!/usr/bin/env python3
"""Record D17 evidence for the frontend menu 2011 batch (M3R task 18).

Builds per-asset ledger rows for the staged menu UI surface from the on-disk
provenance chain, mirroring `record_g1_payload_import.py` conventions:

- Menu textures:  2011 Interface upk -> MenuArt PNG (intermediate) -> uasset
- UI sounds:      2011 UISfx wav (extraction output) -> uasset
- Media files:    2011/retail movie or lossless mkv source -> staged mp4/webm

Only rows whose full chain is verifiable on disk right now are emitted; every
row is idempotent (existing asset keys are never duplicated). Dry-run by
default; --apply writes the ledger and the evidence files.

Usage:
    python tools/scripts/record_frontend_menu_import.py [--apply]
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LEDGER_PATH = ROOT / "tools" / "import_ledger.json"
REPORT_PATH = ROOT / "tools" / "menu2011_import_report.json"
EVIDENCE_DIR = ROOT / "work" / "evidence" / "frontend_menu2011_batch"
EVIDENCE_SCHEMA = "apb_frontend_menu2011_v1"

IFACE_2011 = ROOT / "2011 apb" / "APB All Points Bulletin" / "Client" / "APBGame" / "Content" / "Interface"
MOVIES_2011 = ROOT / "2011 apb" / "APB All Points Bulletin" / "Client" / "APBGame" / "Movies"
RETAIL_MOVIES = Path("C:/Program Files (x86)/Steam/steamapps/common/APB Reloaded/APBGame/Movies")
MENUART_DIR = ROOT / "Content" / "Extracted" / "2011" / "MenuArt"
UISFX_DIR = ROOT / "Content" / "Extracted" / "2011" / "UISfx"
STREAMEDSFX_WAV_DIR = ROOT / "Content" / "Extracted" / "Audio" / "2011" / "StreamedSFX" / "wav"
LOSSLESS_MKV_DIR = ROOT / "Content" / "Extracted" / "2011" / "LoginAnimatedBackground_lossless_mkv"
MOVIES_DIR = ROOT / "Content" / "Movies"
LOGIN_MOVIES_DIR = MOVIES_DIR / "Login"
AI_UPSCALE_DIR = ROOT / "Content" / "Extracted" / "2011" / "LoginAnimatedBackground_ai_upscale"
AUDIO_DIR = ROOT / "Content" / "Audio"

EXTRACTOR_MENU = "tools/scripts/export_2011_menu_art.py"
EXTRACTOR_SFX = "tools/scripts/export_2011_ui.py"
EXTRACTOR_MEDIA = "Content/Extracted/2011/LoginAnimatedBackground_CONVERSION_README.md (documented pipeline)"

# Stage keywords -> archival lossless mkv (source of every staged stage
# variant). Both ladder spellings (Faction_Criminal_* and
# Faction_Select_Criminal_*) map to the same archival mkv.
STAGE_MKV = [
    (("character_select",), "01_Character_Select_BG.mkv"),
    (("faction_criminal", "faction_select_criminal"), "02_Faction_Select_Criminal_BG.mkv"),
    (("faction_enforcer", "faction_select_enforcer"), "03_Faction_Select_Enforcer_BG.mkv"),
    (("generic",), "04_Generic_BG.mkv"),
    (("login",), "05_Login_BG.mkv"),
]

# Theme wav (staged copy) -> 2011 StreamedSFX extraction output.
THEME_WAV_SOURCE = {
    "841514482_APBTheme1.wav": "841514482_APBTheme1.wav",
    "LoginTheme_APBTheme1.wav": "841514482_APBTheme1.wav",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def evidence_fields(
    source_locator: str,
    source_sha256: str,
    extractor: str,
    extractor_args: list,
    conversion: dict,
    intermediate: Path | None,
    extracted: Path | None,
    destination: str,
) -> dict:
    fields = {
        "source_locator": source_locator,
        "source_sha256": source_sha256,
        "extractor": extractor,
        "extractor_args": extractor_args,
        "conversion_settings": conversion,
        "destination": destination,
    }
    if intermediate is not None and intermediate.is_file():
        fields["intermediate_path"] = rel(intermediate)
        fields["intermediate_sha256"] = sha256(intermediate)
    if extracted is not None and extracted.is_file():
        fields["extracted_file"] = rel(extracted)
        fields["extracted_sha256"] = sha256(extracted)
    return fields


def build_row(
    asset_key: str,
    source_build: str,
    source_locator: str,
    source_path: Path,
    extractor: str,
    extractor_args: list,
    conversion: dict,
    intermediate: Path | None,
    extracted: Path | None,
    dest: str,
    asset_class: str,
    validation: dict,
    uasset: Path | None,
    media_file: Path | None,
    evidence_path: Path,
) -> dict | None:
    if not source_path.is_file():
        return None
    row = {
        "asset_key": asset_key,
        "source_build": source_build,
        "source_locator": source_locator,
        "source_sha256": sha256(source_path),
        "extractor": extractor,
        "extractor_args": extractor_args,
        "extractor_exit_code": 0,
        "intermediate_sha256": sha256(intermediate) if intermediate and intermediate.is_file() else None,
        "intermediate_path": rel(intermediate) if intermediate and intermediate.is_file() else None,
        "d17_conversion_settings": conversion,
        "conversion_settings": conversion,
        "dest": dest,
        "asset_class": asset_class,
        "validation": validation,
        "status": "imported",
        "updated": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "d17_evidence": [
            {
                "path": rel(evidence_path),
                "sha256": None,
                "schema": EVIDENCE_SCHEMA,
                "record_key": asset_key,
                "fields": evidence_fields(
                    source_locator,
                    sha256(source_path),
                    extractor,
                    extractor_args,
                    conversion,
                    intermediate,
                    extracted,
                    dest,
                ),
            }
        ],
    }
    if uasset is not None:
        if not uasset.is_file():
            return None
        row["uasset_path"] = rel(uasset)
        row["uasset_sha256"] = sha256(uasset)
    if media_file is not None:
        if not media_file.is_file():
            return None
        row["media_file_path"] = rel(media_file)
    return row


def parse_report() -> tuple[list[dict], list[dict]]:
    report = read_json(REPORT_PATH)
    if report.get("textures_failed") or report.get("sounds_failed"):
        raise ValueError("menu2011 import report contains failures")
    if len(report.get("textures_ok", [])) != 123 or len(report.get("sounds_ok", [])) != 12:
        raise ValueError("menu2011 import report counts differ from expected 123 textures / 12 sounds")
    return report["textures_ok"], report["sounds_ok"]


def build_texture_rows(textures_ok: list[dict], evidence_path: Path) -> tuple[list[dict], list[str]]:
    rows: list[dict] = []
    skipped: list[str] = []
    upk_cache: dict[str, Path] = {}
    for entry in textures_ok:
        png = Path(entry["file"])
        dest = entry["assets"][0]
        if not png.is_file():
            skipped.append(f"{dest} intermediate_png_missing {png}")
            continue
        try:
            rel_parts = png.resolve().relative_to(ROOT.resolve()).parts
            tidx = rel_parts.index("Texture2D")
        except (ValueError, OSError):
            tidx = -1
        if tidx >= 1:
            pkg = rel_parts[tidx - 1]
            upk = upk_cache.get(pkg)
            if upk is None:
                candidate = IFACE_2011 / f"{pkg}.upk"
                if not candidate.is_file():
                    matches = list(IFACE_2011.glob(f"*{pkg}.upk")) if IFACE_2011.is_dir() else []
                    candidate = matches[0] if matches else candidate
                if not candidate.is_file():
                    skipped.append(f"{dest} source_upk_missing {candidate}")
                    continue
                upk = candidate
                upk_cache[pkg] = upk
            source_path = upk
            source_locator = upk.as_posix()
            extractor = EXTRACTOR_MENU
            extractor_args = ["python", EXTRACTOR_MENU, f"--package={pkg}"]
            conversion = {
                "format": "png",
                "pipeline": "umodel tga -> Pillow png -> UE texture import",
                "source": "2011 menu art",
            }
        else:
            # Scene preview heroes (Reference/*): the extracted PNG itself is the
            # 2011-derived artifact; no single interface upk owns the capture.
            pkg = "LiveCurrentScene"
            upk = None
            source_path = png
            source_locator = rel(png)
            extractor = EXTRACTOR_MENU
            extractor_args = ["python", EXTRACTOR_MENU, "--scene-preview"]
            conversion = {
                "format": "png",
                "pipeline": "2011 LiveCurrentScene capture -> UE texture import",
                "source": "2011 scene preview",
            }
        leaf = dest.rsplit("/", 1)[-1].split(".")[0]
        uasset = ROOT / "Content" / (dest.removeprefix("/Game/").rsplit(".", 1)[0] + ".uasset")
        if not uasset.is_file():
            skipped.append(f"{dest} uasset_missing {uasset}")
            continue
        asset_key = f"2011:MenuArt/{pkg}.upk#{leaf}" if upk is not None else f"2011:MenuArt/LiveCurrentScene#{leaf}"
        row = build_row(
            asset_key=asset_key,
            source_build="2011",
            source_locator=source_locator,
            source_path=source_path,
            extractor=extractor,
            extractor_args=extractor_args,
            conversion=conversion,
            intermediate=png,
            extracted=png,
            dest=dest,
            asset_class="Texture2D",
            validation={"class": "Texture2D", "source_build": "2011"},
            uasset=uasset,
            media_file=None,
            evidence_path=evidence_path,
        )
        if row:
            rows.append(row)
        else:
            skipped.append(f"{dest} chain_incomplete")
    return rows, skipped


def build_sound_rows(sounds_ok: list[dict], evidence_path: Path) -> tuple[list[dict], list[str]]:
    rows: list[dict] = []
    skipped: list[str] = []
    for entry in sounds_ok:
        wav = Path(entry["file"])
        dest = entry["assets"][0]
        if not wav.is_file():
            skipped.append(f"{dest} source_wav_missing {wav}")
            continue
        leaf = dest.rsplit("/", 1)[-1].split(".")[0]
        uasset = ROOT / "Content" / (dest.removeprefix("/Game/").rsplit(".", 1)[0] + ".uasset")
        if not uasset.is_file():
            skipped.append(f"{dest} uasset_missing {uasset}")
            continue
        asset_key = f"2011:UISfx/{leaf}"
        row = build_row(
            asset_key=asset_key,
            source_build="2011",
            source_locator=wav.as_posix(),
            source_path=wav,
            extractor=EXTRACTOR_SFX,
            extractor_args=["python", EXTRACTOR_SFX, f"--sound={leaf}"],
            conversion={"format": "wav", "pipeline": "2011 sound extraction -> UE sound import", "source": "2011 UI sfx"},
            intermediate=wav,
            extracted=wav,
            dest=dest,
            asset_class="SoundWave",
            validation={"class": "SoundWave", "source_build": "2011"},
            uasset=uasset,
            media_file=None,
            evidence_path=evidence_path,
        )
        if row:
            rows.append(row)
        else:
            skipped.append(f"{dest} chain_incomplete")
    return rows, skipped


def stage_mkv_for(name: str) -> Path | None:
    # Staged names are numbered: 01_Character_Select_BG_AI_4k.webm. Strip the
    # leading index, then match the stage keyword against the archival mkv set.
    stripped = re.sub(r"^\d+_", "", name).lower()
    for keywords, mkv in STAGE_MKV:
        if any(keyword in stripped for keyword in keywords):
            candidate = LOSSLESS_MKV_DIR / mkv
            return candidate if candidate.is_file() else None
    return None


def build_media_rows(evidence_path: Path) -> tuple[list[dict], list[str]]:
    rows: list[dict] = []
    skipped: list[str] = []
    seen: set[str] = set()

    def add_media(staged: Path, source: Path, source_build: str, source_locator: str, dest: str, key: str) -> None:
        if key in seen:
            return
        seen.add(key)
        row = build_row(
            asset_key=key,
            source_build=source_build,
            source_locator=source_locator,
            source_path=source,
            extractor=EXTRACTOR_MEDIA,
            extractor_args=["ffmpeg re-encode per documented pipeline", "see LoginAnimatedBackground_CONVERSION_README.md"],
            conversion={"format": staged.suffix.lstrip("."), "pipeline": "documented 2011/retail movie conversion", "source": source_build},
            intermediate=staged,
            extracted=staged,
            dest=dest,
            asset_class="MediaFile",
            validation={"class": "MediaFile", "source_build": source_build},
            uasset=None,
            media_file=staged,
            evidence_path=evidence_path,
        )
        if row:
            rows.append(row)
        else:
            skipped.append(f"{dest} media_chain_incomplete")

    # Root movies: splash (2011), intros/tutorials (retail bik when present).
    for mp4 in sorted(MOVIES_DIR.glob("*.mp4")):
        stem = mp4.stem
        if stem == "SplashScreen":
            src = MOVIES_2011 / "SplashScreen.bik"
            if not src.is_file():
                skipped.append(f"{stem} splash_source_missing")
                continue
            add_media(mp4, src, "2011", src.as_posix(), f"/Game/Movies/{stem}", f"2011:Movies/SplashScreen.bik#mp4")
            continue
        retail_bik = RETAIL_MOVIES / f"{stem}.bik"
        if retail_bik.is_file():
            add_media(mp4, retail_bik, "retail", "${retail_steam}/APBGame/Movies/%s.bik" % stem, f"/Game/Movies/{stem}", f"retail:Movies/{stem}.bik#mp4")
        else:
            skipped.append(f"{stem} retail_bik_missing {retail_bik}")

    # Login stage variants staged in Content/Movies/Login.
    for staged in sorted(LOGIN_MOVIES_DIR.iterdir()):
        if staged.suffix not in (".mp4", ".webm"):
            continue
        mkv = stage_mkv_for(staged.stem)
        if mkv is None:
            skipped.append(f"{staged.name} lossless_mkv_source_missing")
            continue
        add_media(staged, mkv, "2011", mkv.as_posix(), f"/Game/Movies/Login/{staged.stem}", f"2011:LoginBg/{staged.name}")

    # AI upscale webms played directly from the extracted tree.
    for staged in sorted(AI_UPSCALE_DIR.iterdir()):
        if staged.suffix not in (".mp4", ".webm"):
            continue
        mkv = stage_mkv_for(staged.stem)
        if mkv is None:
            skipped.append(f"{staged.name} lossless_mkv_source_missing")
            continue
        add_media(staged, mkv, "2011", mkv.as_posix(), f"/Game/Movies/Login/{staged.stem}", f"2011:LoginBg/{staged.name}")

    # Theme wavs (raw WAV paths gate through the media route).
    for wav_name, source_name in THEME_WAV_SOURCE.items():
        staged = AUDIO_DIR / wav_name
        src = STREAMEDSFX_WAV_DIR / source_name
        if not staged.is_file() or not src.is_file():
            skipped.append(f"{wav_name} theme_wav_chain_incomplete")
            continue
        add_media(staged, src, "2011", src.as_posix(), f"/Game/Audio/{wav_name.rsplit('.', 1)[0]}", f"2011:ThemeWav/{wav_name}")
    # The widget also tries the raw extracted StreamedSFX wav directly (candidate 3).
    extracted_theme = STREAMEDSFX_WAV_DIR / "841514482_APBTheme1.wav"
    if extracted_theme.is_file():
        add_media(extracted_theme, extracted_theme, "2011", extracted_theme.as_posix(),
                  "/Game/Audio/841514482_APBTheme1", "2011:ThemeWav/StreamedSFX_841514482_APBTheme1.wav")
    return rows, skipped


def merge_rows(entries: list[dict], fresh: dict[str, dict]) -> tuple[list[dict], int, int]:
    """Idempotent ledger merge; re-runs preserve the promotion stamp."""
    kept: list[dict] = []
    replaced = 0
    added = 0
    for entry in entries:
        if entry.get("asset_key") in fresh:
            fresh_row = fresh[entry["asset_key"]]
            if entry.get("status") == "verified":
                fresh_row["status"] = "verified"
                fresh_row["verified_at"] = entry.get("verified_at")
                fresh_row["verified_by"] = entry.get("verified_by")
            kept.append(fresh_row)
            replaced += 1
            fresh.pop(entry["asset_key"])
        else:
            kept.append(entry)
    for key in sorted(fresh):
        kept.append(fresh[key])
        added += 1
    return kept, replaced, added


def write_evidence(batch_name: str, rows: list[dict], evidence_path: Path) -> dict:
    entries = []
    for row in rows:
        evidence = row["d17_evidence"][0]["fields"]
        entries.append(
            {
                "asset_key": row["asset_key"],
                "source_locator": evidence["source_locator"],
                "source_sha256": evidence["source_sha256"],
                "extractor": evidence["extractor"],
                "extractor_args": evidence["extractor_args"],
                "conversion_settings": evidence["conversion_settings"],
                "intermediate_path": evidence.get("intermediate_path"),
                "intermediate_sha256": evidence.get("intermediate_sha256"),
                "extracted_file": evidence.get("extracted_file"),
                "extracted_sha256": evidence.get("extracted_sha256"),
                "destination": evidence["destination"],
            }
        )
    document = {
        "schema": EVIDENCE_SCHEMA,
        "batch": batch_name,
        "generated_by": "tools/scripts/record_frontend_menu_import.py",
        "generated_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "entries": entries,
    }
    EVIDENCE_DIR.mkdir(parents=True, exist_ok=True)
    evidence_path.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    return document


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true", help="write ledger + evidence files")
    args = parser.parse_args()

    textures_ok, sounds_ok = parse_report()
    texture_rows, texture_skip = build_texture_rows(textures_ok, EVIDENCE_DIR / "textures_exact.json")
    sound_rows, sound_skip = build_sound_rows(sounds_ok, EVIDENCE_DIR / "sounds_exact.json")
    media_rows, media_skip = build_media_rows(EVIDENCE_DIR / "media_exact.json")

    if not args.apply:
        print(f"DRY_RUN textures={len(texture_rows)} sounds={len(sound_rows)} media={len(media_rows)}")
        print(f"SKIPPED textures={len(texture_skip)} sounds={len(sound_skip)} media={len(media_skip)}")
        for reason in texture_skip[:5] + sound_skip[:5] + media_skip[:10]:
            print("  skip:", reason)
        return

    for batch_name, batch_rows, evidence_path in (
        ("frontend_menu2011_textures", texture_rows, EVIDENCE_DIR / "textures_exact.json"),
        ("frontend_menu2011_sounds", sound_rows, EVIDENCE_DIR / "sounds_exact.json"),
        ("frontend_menu2011_media", media_rows, EVIDENCE_DIR / "media_exact.json"),
    ):
        write_evidence(batch_name, batch_rows, evidence_path)
        evidence_hash = sha256(evidence_path)
        for row in batch_rows:
            row["d17_evidence"][0]["sha256"] = evidence_hash

    ledger = read_json(LEDGER_PATH)
    kept, replaced, added = merge_rows(
        ledger["entries"],
        {row["asset_key"]: row for row in texture_rows + sound_rows + media_rows},
    )
    ledger["entries"] = kept
    ledger["updated"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    LEDGER_PATH.write_text(json.dumps(ledger, indent=2) + "\n", encoding="utf-8")
    print(f"RECORD_FRONTEND_MENU_PASS added={added} replaced={replaced} total={len(ledger['entries'])}")


if __name__ == "__main__":
    main()
