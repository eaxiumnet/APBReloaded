#!/usr/bin/env python3
"""Document 2011 APB login/main-menu rendering pipeline from real on-disk artifacts.

Reads engine INI, verifies key packages/maps/scripts, optionally scans a Launch.log
for runtime UI markers, writes JSON + markdown summary.

  python tools/scripts/document_2011_login_pipeline.py
  python tools/scripts/document_2011_login_pipeline.py --log path/to/Launch.log --out-dir DIR
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
APB = ROOT / "2011 apb" / "APB All Points Bulletin" / "APB North America"
APBGAME = APB / "APBGame"
DEFAULT_OUT = ROOT / "Content" / "Extracted" / "2011"
DEFAULT_SCRATCH = Path(r"C:\Users\Support\AppData\Local\Temp\grok-goal-9234a14c05ca\implementer")

REQUIRED_PATHS = {
    "exe": APB / "Binaries" / "APB.exe",
    "login_map": APBGAME / "Content" / "Maps" / "APBLoginLevel.apb",
    "user_ui_script": APBGAME / "ScriptUserBuild" / "APBUserInterface.u",
    "game_script": APBGAME / "ScriptUserBuild" / "APBGame.u",
    "frontend_upk": APBGAME / "Content" / "Interface" / "APBMenus_FrontEnd.upk",
    "gameflow_upk": APBGAME / "Content" / "Interface" / "APBMenus_GameFlowScenes.upk",
    "gameflow_art_upk": APBGAME / "Content" / "Interface" / "APBMenus_Art_GameFlowScenes.upk",
    "engine_ini": APBGAME / "Config" / "APBEngine.ini",
    "splash_bik": APBGAME / "Movies" / "SplashScreen.bik",
    "intro_bik": APBGAME / "Movies" / "IntroTitles.bik",
}


def parse_engine_maps(ini: Path) -> list[str]:
    text = ini.read_text(errors="ignore")
    return re.findall(r"^(?:Local)?Map\s*=\s*.+$", text, re.M)


def scan_log_markers(log_path: Path) -> list[str]:
    if not log_path.is_file():
        return []
    pats = re.compile(
        r"(LoadMap:|Browse:|GameFlow|Login_Scene|IntroMovie|SplashScreen|IntroTitles|"
        r"TextureMovie|UcUIDataStore_GameFlowManager|LaunchGameFlowScene|"
        r"TriggerSceneOpenedPopupDialog|WorldInfo\.StreamingLevels|BINK|Playing movie)",
        re.I,
    )
    hits = []
    for line in log_path.read_text(errors="ignore").splitlines():
        if pats.search(line):
            hits.append(line.strip())
    return hits


def uidistrict_present() -> list[str]:
    content = APBGAME / "Content"
    if not content.is_dir():
        return []
    return [str(p.relative_to(APBGAME)) for p in content.rglob("*UIDistrict*")]


def mine_script_signals(path: Path) -> list[str]:
    data = path.read_bytes()
    idents = set(
        m.group().decode("ascii", "ignore")
        for m in re.finditer(rb"[A-Za-z_][A-Za-z0-9_]{5,64}", data)
    )
    keys = ("GameFlow", "FrontEnd", "Login", "OpenMenu", "UIScene", "Scene", "Matinee")
    return sorted(i for i in idents if any(k.lower() in i.lower() for k in keys))


def build_pipeline(log_path: Path | None) -> dict:
    paths = {k: str(v) for k, v in REQUIRED_PATHS.items()}
    exists = {k: Path(p).is_file() for k, p in paths.items()}
    maps = parse_engine_maps(REQUIRED_PATHS["engine_ini"]) if exists["engine_ini"] else []
    log_hits = scan_log_markers(log_path) if log_path else []
    ui_sigs = (
        mine_script_signals(REQUIRED_PATHS["user_ui_script"])
        if exists["user_ui_script"]
        else []
    )
    # ordered pipeline stages (documented)
    stages = [
        {
            "order": 1,
            "stage": "process_entry",
            "detail": "Binaries/APB.exe cwd=Binaries; loads APBEngine/APBGame/APBUI configs",
            "evidence": paths["exe"],
        },
        {
            "order": 2,
            "stage": "default_map",
            "detail": "Engine Map=/LocalMap=APBLoginLevel.apb",
            "evidence": maps,
        },
        {
            "order": 3,
            "stage": "load_map_shell",
            "detail": "LoadMap APBLoginLevel — tiny shell (streaming slots + Kismet openmenu)",
            "evidence": paths["login_map"],
        },
        {
            "order": 4,
            "stage": "ui_script_gameflow_manager",
            "detail": "APBUserInterface: UcUIDataStore_GameFlowManager::InitBaseScene / LaunchGameFlowScene",
            "evidence": paths["user_ui_script"],
        },
        {
            "order": 5,
            "stage": "ui_packages",
            "detail": "UIScene packages APBMenus_GameFlowScenes + APBMenus_FrontEnd; art APBMenus_Art_GameFlowScenes (TextureMovie BGs)",
            "evidence": [
                paths["gameflow_upk"],
                paths["frontend_upk"],
                paths["gameflow_art_upk"],
            ],
        },
        {
            "order": 6,
            "stage": "fullscreen_movies",
            "detail": "FFullScreenMovieBink: SplashScreen then IntroTitles (and failed optional APBtest)",
            "evidence": [paths["splash_bik"], paths["intro_bik"]],
        },
        {
            "order": 7,
            "stage": "scene_stack",
            "detail": "GameFlowBase_Scene → IntroMovieScene → Login_Scene (UIScene stack, not UIDistrict 3D)",
            "evidence": "runtime APB_UI TriggerSceneOpenedPopupDialog lines",
        },
        {
            "order": 8,
            "stage": "not_present",
            "detail": "No UIDistrict_* / Login01 posed-character city stage in this 2011 Content tree",
            "evidence": uidistrict_present() or ["(none)"],
        },
    ]
    return {
        "source_root": str(APBGAME),
        "paths": paths,
        "exists": exists,
        "engine_map_lines": maps,
        "uidistrict_paths": uidistrict_present(),
        "script_signals_APBUserInterface": ui_sigs[:80],
        "runtime_log": str(log_path) if log_path else None,
        "runtime_markers": log_hits[:120],
        "pipeline": stages,
    }


def write_markdown(doc: dict, md_path: Path) -> None:
    lines = [
        "# 2011 APB login / main-menu rendering pipeline",
        "",
        "Evidence from live `APB.exe` launch logs + on-disk INI/scripts/packages (not Steam UIDistrict).",
        "",
        "## Ordered pipeline",
        "",
    ]
    for s in doc["pipeline"]:
        lines.append(f"### {s['order']}. {s['stage']}")
        lines.append("")
        lines.append(s["detail"])
        lines.append("")
        lines.append(f"- Evidence: `{s['evidence']}`")
        lines.append("")
    lines += [
        "## Engine map config",
        "",
    ]
    for m in doc.get("engine_map_lines") or []:
        lines.append(f"- `{m}`")
    lines += [
        "",
        "## Runtime markers (if log provided)",
        "",
    ]
    for h in (doc.get("runtime_markers") or [])[:40]:
        lines.append(f"- `{h}`")
    if not doc.get("runtime_markers"):
        lines.append("- *(no log markers)*")
    lines += [
        "",
        "## UIDistrict in 2011 tree",
        "",
        f"- Hits: `{doc.get('uidistrict_paths')}`",
        "",
        "## Key script signals (APBUserInterface.u)",
        "",
    ]
    for s in (doc.get("script_signals_APBUserInterface") or [])[:40]:
        lines.append(f"- `{s}`")
    lines += [
        "",
        "## What is rendered",
        "",
        "1. **3D world**: almost empty `APBLoginLevel` (2 streaming level slots; no UIDistrict).",
        "2. **Fullscreen Bink**: SplashScreen / IntroTitles via `FFullScreenMovieBink`.",
        "3. **Unreal UIScenes** from `APBMenus_GameFlowScenes` / FrontEnd (Login_Scene, GameFlowBase_Scene, …).",
        "4. **TextureMovie backgrounds** in `APBMenus_Art_GameFlowScenes` (Login_BG, Generic_BG, faction select BGs, Character_Select_BG).",
        "",
        "The multi-character graffiti city stage is **not** loaded by this 2011 client.",
        "",
    ]
    md_path.write_text("\n".join(lines), encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--log", type=Path, default=None, help="Launch.log or backup to scan")
    ap.add_argument("--out-dir", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--scratch", type=Path, default=DEFAULT_SCRATCH)
    args = ap.parse_args(argv)

    log = args.log
    if log is None:
        # prefer live Launch.log then newest backup under APBGame/Logs
        logs = APBGAME / "Logs"
        candidates = []
        if (logs / "Launch.log").is_file() and (logs / "Launch.log").stat().st_size > 0:
            candidates.append(logs / "Launch.log")
        candidates.extend(sorted(logs.glob("Launch-backup-*.log"), key=lambda p: -p.stat().st_mtime))
        # also scratch copies
        if args.scratch.is_dir():
            candidates.extend(sorted(args.scratch.glob("Launch*.log"), key=lambda p: -p.stat().st_mtime))
        for c in candidates:
            if c.is_file() and c.stat().st_size > 1000:
                log = c
                break

    doc = build_pipeline(log)
    args.out_dir.mkdir(parents=True, exist_ok=True)
    args.scratch.mkdir(parents=True, exist_ok=True)
    json_path = args.out_dir / "login_menu_rendering_pipeline.json"
    md_path = args.out_dir / "LOGIN_MENU_RENDERING_PIPELINE.md"
    json_path.write_text(json.dumps(doc, indent=2), encoding="utf-8")
    write_markdown(doc, md_path)
    # mirror to scratch
    (args.scratch / "login_menu_rendering_pipeline.json").write_text(
        json.dumps(doc, indent=2), encoding="utf-8"
    )
    (args.scratch / "LOGIN_MENU_RENDERING_PIPELINE.md").write_text(
        md_path.read_text(encoding="utf-8"), encoding="utf-8"
    )
    print("wrote", md_path)
    print("wrote", json_path)
    print("exists", doc["exists"])
    print("map_lines", doc["engine_map_lines"])
    print("uidistrict", doc["uidistrict_paths"])
    print("runtime_markers", len(doc["runtime_markers"] or []))
    print("log", doc["runtime_log"])
    # fail if core files missing
    if not all(doc["exists"].get(k) for k in ("exe", "login_map", "user_ui_script", "gameflow_upk")):
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
