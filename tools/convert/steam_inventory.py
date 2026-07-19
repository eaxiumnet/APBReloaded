#!/usr/bin/env python3
"""Inventory Steam APB install + project extract trees for UE conversion spine.

Writes machine-readable catalog under project Content/Extracted/Convert and {scratch}.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

STEAM_ROOT = Path(r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded")
PROJECT = Path(r"D:\APBReloaded")
SCRATCH_DEFAULT = Path(r"C:\Users\Support\AppData\Local\Temp\grok-goal-e6df5b4a9676\implementer")


def count_files(root: Path, patterns: list[str], max_depth: int | None = None) -> int:
    if not root.is_dir():
        return 0
    n = 0
    if max_depth is None:
        for pat in patterns:
            n += sum(1 for _ in root.rglob(pat) if _.is_file())
        return n
    # limited walk
    root_depth = len(root.parts)
    for p in root.rglob("*"):
        if not p.is_file():
            continue
        if len(p.parts) - root_depth > max_depth:
            continue
        if any(p.match(pat) or p.suffix.lower() == pat.lstrip("*") for pat in patterns):
            n += 1
    return n


def count_glob(root: Path, pattern: str) -> int:
    if not root.is_dir():
        return 0
    return sum(1 for _ in root.rglob(pattern) if _.is_file())


def inventory(steam: Path = STEAM_ROOT, project: Path = PROJECT) -> dict:
    apbgame = steam / "APBGame"
    packages = apbgame / "Content" / "Release" / "Packages"
    audio = apbgame / "Content" / "Audio"
    extract = project / "Content" / "Extracted"
    data = project / "Content" / "Data"
    imported = project / "Content" / "Imported"

    stubs: list[dict] = []
    # Wwise stub media banks
    banks = audio / "SoundBanks"
    if banks.is_dir():
        for bnk in banks.rglob("*_Media.bnk"):
            if bnk.stat().st_size <= 64:
                stubs.append({"path": str(bnk), "size": bnk.stat().st_size, "kind": "empty_media_bank"})

    inv = {
        "steam_root": str(steam),
        "project_root": str(project),
        "buckets": {
            "upk_packages": {
                "path": str(packages),
                "count": count_glob(packages, "*.upk") if packages.is_dir() else 0,
                "exists": packages.is_dir(),
            },
            "script_user_build": {
                "path": str(apbgame / "ScriptUserBuild"),
                "count": count_glob(apbgame / "ScriptUserBuild", "*") if (apbgame / "ScriptUserBuild").is_dir() else 0,
                "exists": (apbgame / "ScriptUserBuild").is_dir(),
            },
            "audio_soundbanks": {
                "path": str(banks),
                "bnk_count": count_glob(banks, "*.bnk") if banks.is_dir() else 0,
                "txt_count": count_glob(banks, "*.txt") if banks.is_dir() else 0,
                "exists": banks.is_dir(),
            },
            "audio_filepackages": {
                "path": str(audio / "FilePackages"),
                "count": count_glob(audio / "FilePackages", "*") if (audio / "FilePackages").is_dir() else 0,
                "exists": (audio / "FilePackages").is_dir(),
            },
            "default_music_library": {
                "path": str(audio / "DefaultMusicLibrary"),
                "mp3_count": count_glob(audio / "DefaultMusicLibrary", "*.mp3")
                if (audio / "DefaultMusicLibrary").is_dir()
                else 0,
                "note": "radio_only_not_login_theme",
            },
            "steam_config": {
                "path": str(apbgame / "Config"),
                "count": count_glob(apbgame / "Config", "*") if (apbgame / "Config").is_dir() else 0,
            },
            "apb_private_server": {
                "path": str(steam / "ApbPrivateServer"),
                "cs_count": count_glob(steam / "ApbPrivateServer", "*.cs")
                if (steam / "ApbPrivateServer").is_dir()
                else 0,
                "exists": (steam / "ApbPrivateServer").is_dir(),
            },
            "project_umodel_export": {
                "path": str(extract / "UmodelExport"),
                "psk_count": count_glob(extract / "UmodelExport", "*.psk")
                + count_glob(extract / "UmodelExport", "*.pskx")
                if (extract / "UmodelExport").is_dir()
                else 0,
            },
            "project_audio_extract": {
                "path": str(extract / "Audio"),
                "wem_count": count_glob(extract / "Audio", "*.wem") if (extract / "Audio").is_dir() else 0,
                "wav_count": count_glob(extract / "Audio", "*.wav") if (extract / "Audio").is_dir() else 0,
                "main_media_wem": count_glob(extract / "Audio" / "Main_Media", "*.wem")
                if (extract / "Audio" / "Main_Media").is_dir()
                else 0,
                "scaleform_files": count_glob(extract / "Audio" / "Scaleform", "*")
                if (extract / "Audio" / "Scaleform").is_dir()
                else 0,
            },
            "project_district_placements": {
                "path": str(data / "district_placements"),
                "json_count": count_glob(data / "district_placements", "*.json")
                if (data / "district_placements").is_dir()
                else 0,
            },
            "project_catalogs": {
                "districts_json": (data / "districts.json").is_file(),
                "clothing_json": (data / "clothing.json").is_file(),
                "vehicles_json": (data / "vehicles.json").is_file() or (data / "vehicles_catalog.json").is_file(),
                "weapons_json": (data / "weapons.json").is_file() or (data / "weapons_catalog.json").is_file(),
            },
            "project_imported": {
                "path": str(imported),
                "uasset_count": count_glob(imported, "*.uasset") if imported.is_dir() else 0,
            },
            "login_theme": {
                "path": str(project / "Content" / "Audio" / "LoginTheme_APB_ThemePreMaster.wav"),
                "exists": (project / "Content" / "Audio" / "LoginTheme_APB_ThemePreMaster.wav").is_file(),
                "bytes": (project / "Content" / "Audio" / "LoginTheme_APB_ThemePreMaster.wav").stat().st_size
                if (project / "Content" / "Audio" / "LoginTheme_APB_ThemePreMaster.wav").is_file()
                else 0,
            },
        },
        "stubs": stubs,
        "conversion_spine": {
            "umodel": str(project / "tools" / "umodel"),
            "wwise_extract": str(project / "tools" / "WwiseExtract"),
            "apbdb": str(project / "tools" / "apbdb"),
            "asset_pipeline_md": str(project / "tools" / "ASSET_PIPELINE.md"),
            "model_viewer": str(project / "tools" / "model_viewer"),
            "convert_tools": str(project / "tools" / "convert"),
        },
    }

    # Primary counts for dual-run compare
    inv["primary_counts"] = {
        "upk": inv["buckets"]["upk_packages"]["count"],
        "psk": inv["buckets"]["project_umodel_export"]["psk_count"],
        "audio_wem": inv["buckets"]["project_audio_extract"]["wem_count"],
        "audio_wav": inv["buckets"]["project_audio_extract"]["wav_count"],
        "placements": inv["buckets"]["project_district_placements"]["json_count"],
        "privateserver_cs": inv["buckets"]["apb_private_server"]["cs_count"],
    }
    return inv


def write_outputs(inv: dict, scratch: Path, tag: str = "") -> None:
    scratch.mkdir(parents=True, exist_ok=True)
    out_dir = PROJECT / "Content" / "Extracted" / "Convert"
    out_dir.mkdir(parents=True, exist_ok=True)

    jpath = out_dir / "steam_convert_inventory.json"
    jpath.write_text(json.dumps(inv, indent=2), encoding="utf-8")
    (scratch / "steam_convert_inventory.json").write_text(json.dumps(inv, indent=2), encoding="utf-8")

    lines = [
        "APB Steam → UE conversion inventory",
        f"steam={inv['steam_root']}",
        f"project={inv['project_root']}",
    ]
    for k, v in inv["primary_counts"].items():
        lines.append(f"count_{k}={v}")
    for name, b in inv["buckets"].items():
        lines.append(f"bucket {name}={json.dumps(b, default=str)}")
    for s in inv["stubs"]:
        lines.append(f"STUB {s['path']} size={s['size']}")
    layout = scratch / "convert_run.log"
    layout.write_text("\n".join(lines) + "\n", encoding="utf-8")
    (out_dir / "steam_convert_inventory.md").write_text(
        "# Steam convert inventory\n\n"
        + "\n".join(f"- **{k}**: `{v}`" for k, v in inv["primary_counts"].items())
        + "\n\nStubs:\n"
        + "\n".join(f"- `{s['path']}` ({s['size']} B)" for s in inv["stubs"])
        + "\n",
        encoding="utf-8",
    )

    # dual-run primary line
    if tag:
        (scratch / f"inventory_{tag}.txt").write_text(
            "\n".join(f"{k}={v}" for k, v in sorted(inv["primary_counts"].items())) + "\n",
            encoding="utf-8",
        )


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--scratch", type=Path, default=SCRATCH_DEFAULT)
    ap.add_argument("--tag", default="")
    args = ap.parse_args()
    inv = inventory()
    write_outputs(inv, args.scratch, args.tag)
    pc = inv["primary_counts"]
    print("primary", pc)
    # Gate: non-zero models and audio and map
    ok = pc["psk"] > 0 and (pc["audio_wem"] > 0 or pc["audio_wav"] > 0) and pc["placements"] > 0
    return 0 if ok else 2


if __name__ == "__main__":
    raise SystemExit(main())
