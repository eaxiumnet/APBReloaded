#!/usr/bin/env python3
"""Structural checks for classic frontend + audio dump (shipped sources / artifacts)."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(r"D:\APBReloaded")
CPP = ROOT / "Source" / "APBReloaded" / "Systems" / "APBFrontendWidget.cpp"
HDR = ROOT / "Source" / "APBReloaded" / "Systems" / "APBFrontendWidget.h"
THEME = ROOT / "Content" / "Audio" / "841514482_APBTheme1.wav"
THEME_LEGACY = ROOT / "Content" / "Audio" / "LoginTheme_APB_ThemePreMaster.wav"
EXTRACT = ROOT / "Content" / "Extracted" / "Audio"
SCRATCH = Path(r"C:\Users\Support\AppData\Local\Temp\grok-goal-05e2326e6a85\implementer")


def main() -> int:
    errs: list[str] = []
    src = CPP.read_text(encoding="utf-8", errors="ignore")
    hdr = HDR.read_text(encoding="utf-8", errors="ignore")

    for needle in (
        "UpdateMotionBackground",
        "UpdateViewportScale",
        "841514482_APBTheme1",
        "APB_LoadPcmWavProcedural",
        "CharacterSelect",
        "PanelSizeBox",
        "BodyScroll",
        "BgFar",
        "BgNear",
        "Constant_BG",
        "Login_Scene_Preview",
        "SetStage(EAPBFrontendStage::CharacterSelect)",
    ):
        if needle not in src and needle not in hdr:
            errs.append(f"missing_in_source:{needle}")

    # Must not LOAD DefaultMusicLibrary / ChapterOne for login music
    if re.search(r'LoadObject[^\n]*ChapterOne|LoadObject[^\n]*DefaultMusicLibrary', src):
        errs.append("login_loads_radio_library")
    if "841514482_APBTheme1" not in src:
        errs.append("missing_apbtheme1_load_path")

    if not THEME.is_file() or THEME.stat().st_size < 100_000:
        errs.append(f"theme_missing_or_tiny:{THEME}")
    ui_bg = ROOT / "Content" / "UI" / "Frontend" / "2011" / "Constant_BG.tga"
    if not ui_bg.is_file() or ui_bg.stat().st_size < 100_000:
        errs.append(f"ui_2011_bg_missing:{ui_bg}")

    wem_count = sum(1 for _ in EXTRACT.rglob("*.wem")) if EXTRACT.is_dir() else 0
    wav_count = sum(1 for _ in EXTRACT.rglob("*.wav")) if EXTRACT.is_dir() else 0
    if wem_count < 500:
        errs.append(f"audio_dump_too_small_wem={wem_count}")
    if wav_count < 100:
        errs.append(f"audio_dump_too_small_wav={wav_count}")

    # Optional manifest from full audio dump jobs
    man = SCRATCH / "audio_dump_manifest.txt"
    if man.is_file():
        text = man.read_text(encoding="utf-8", errors="ignore")
        if "APBTheme1" not in text and "ThemePreMaster" not in text and "841514482" not in text:
            errs.append("manifest_missing_theme")

    out = SCRATCH / "static_verify.txt"
    lines = [
        f"ok={0 if errs else 1}",
        f"wem_count={wem_count}",
        f"wav_count={wav_count}",
        f"theme_bytes={THEME.stat().st_size if THEME.is_file() else 0}",
        f"errors={errs}",
    ]
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))
    return 1 if errs else 0


if __name__ == "__main__":
    raise SystemExit(main())
