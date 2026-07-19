#!/usr/bin/env python3
"""Machine-readable inventory of the 2011 RTW APB client tree (login-era focus)."""
from __future__ import annotations

import csv
import json
import re
import time
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(r"D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America")
OUT_DIR = Path(r"D:\APBReloaded\Content\Extracted\2011_rtw")
LOGIN_HINT = re.compile(
    r"(login|splash|frontend|gameflow|title|theme|music|menu|ui_|interface|account|password|username|intro|loading)",
    re.I,
)

# Relative roots to walk fully (everything under APB North America for catalog)
FOCUS_REL = [
    "APBGame/Content/Interface",
    "APBGame/Content/Audio",
    "APBGame/Movies",
    "APBGame/Config",
    "APBGame/Localization/INT",
    "APBGame/Splash",
    "APBGame/ScriptUserBuild",
    "Binaries",
    "Launcher",
    "Engine/Config",
    "Engine/Content/UI",
]


def rel_posix(p: Path) -> str:
    return p.relative_to(ROOT).as_posix()


def main() -> int:
    if not ROOT.is_dir():
        print("missing root", ROOT)
        return 1
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    t0 = time.time()

    all_rows: list[dict] = []
    by_ext: Counter[str] = Counter()
    by_top: defaultdict[str, dict] = defaultdict(lambda: {"files": 0, "bytes": 0})
    login_hits: list[dict] = []

    # Full tree shallow counts + focus deep inventory
    for dirpath, dirnames, filenames in __import__("os").walk(ROOT):
        # skip huge trash if any
        base = Path(dirpath)
        try:
            rel_base = rel_posix(base)
        except ValueError:
            continue
        for name in filenames:
            p = base / name
            try:
                st = p.stat()
            except OSError:
                continue
            size = st.st_size
            ext = p.suffix.lower() or "(none)"
            by_ext[ext] += 1
            top = rel_base.split("/", 1)[0] if rel_base else name
            by_top[top]["files"] += 1
            by_top[top]["bytes"] += size

            # Detailed rows only under focus roots or login-named files
            under_focus = any(rel_base == fr or rel_base.startswith(fr + "/") for fr in FOCUS_REL)
            name_login = bool(LOGIN_HINT.search(name) or LOGIN_HINT.search(rel_base))
            if under_focus or name_login:
                row = {
                    "rel_path": f"{rel_base}/{name}" if rel_base else name,
                    "size": size,
                    "ext": ext,
                    "login_hint": name_login,
                }
                all_rows.append(row)
                if name_login:
                    login_hits.append(row)

    # Key login packages explicitly listed
    key_paths = [
        "APBGame/Content/Interface/APBMenus_FrontEnd.upk",
        "APBGame/Content/Interface/APBMenus_Art_GameFlowScenes.upk",
        "APBGame/Content/Interface/APBMenus_GameFlowScenes.upk",
        "APBGame/Content/Interface/APBMenus_Art_BG_Materials.upk",
        "APBGame/Content/Audio/FilePackages/Music.pck",
        "APBGame/Content/Audio/FilePackages/StreamedSFX.pck",
        "APBGame/Content/Audio/FilePackages/Dialogue.pck",
        "APBGame/Content/Audio/SoundBanks/Main_Media.bnk",
        "APBGame/Content/Audio/SoundBanks/Main.bnk",
        "APBGame/Content/Audio/SoundBanks/Init.bnk",
        "APBGame/Movies/SplashScreen.bik",
        "APBGame/Movies/IntroTitles.bik",
        "APBGame/Movies/LoadingMovieV1.bik",
        "APBGame/Splash/PC/Splash.bmp",
        "APBGame/Config/APBCompat_APBLoginLevel.ini",
        "APBGame/Config/APBEngine.ini",
        "APBGame/Config/APBGame.ini",
        "APBGame/Config/APBUI.ini",
        "Binaries/APB.exe",
        "Binaries/client.config",
        "Launcher/APBLauncher.exe",
        "Launcher/launcher.config",
    ]
    key_assets = []
    for kp in key_paths:
        fp = ROOT / kp.replace("/", "\\")
        key_assets.append(
            {
                "rel_path": kp,
                "exists": fp.is_file(),
                "size": fp.stat().st_size if fp.is_file() else 0,
            }
        )

    # Interface package index
    iface = ROOT / "APBGame" / "Content" / "Interface"
    iface_pkgs = []
    if iface.is_dir():
        for p in sorted(iface.glob("*.upk")):
            iface_pkgs.append({"name": p.name, "size": p.stat().st_size})

    # Movies
    movies = []
    mov = ROOT / "APBGame" / "Movies"
    if mov.is_dir():
        for p in sorted(mov.iterdir()):
            if p.is_file():
                movies.append({"name": p.name, "size": p.stat().st_size})

    # Config login-related
    cfg_login = []
    cfg = ROOT / "APBGame" / "Config"
    if cfg.is_dir():
        for p in sorted(cfg.glob("*.ini")):
            if LOGIN_HINT.search(p.name) or "Login" in p.name or "UI" in p.name:
                cfg_login.append({"name": p.name, "size": p.stat().st_size})

    # Localization strings sample (login-related lines)
    loc_samples: list[dict] = []
    loc = ROOT / "APBGame" / "Localization" / "INT"
    if loc.is_dir():
        for p in sorted(loc.glob("*.int")) + sorted(loc.glob("*.INT")):
            try:
                text = p.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                continue
            hits = []
            for i, line in enumerate(text.splitlines(), 1):
                if re.search(r"Login|Password|Username|Sign.?in|Account", line, re.I):
                    hits.append({"line": i, "text": line.strip()[:200]})
                    if len(hits) >= 8:
                        break
            if hits:
                loc_samples.append({"file": p.name, "hits": hits})

    summary = {
        "source_root": str(ROOT),
        "generated_unix": int(time.time()),
        "elapsed_sec": round(time.time() - t0, 2),
        "total_files_walked_top": {k: v for k, v in sorted(by_top.items())},
        "extension_counts_full_tree": dict(by_ext.most_common(40)),
        "focus_detail_rows": len(all_rows),
        "login_hint_rows": len(login_hits),
        "key_login_assets": key_assets,
        "interface_package_count": len(iface_pkgs),
        "movies": movies,
        "config_login_related": cfg_login,
        "localization_login_sample_files": len(loc_samples),
    }

    (OUT_DIR / "inventory_2011_summary.json").write_text(
        json.dumps(summary, indent=2), encoding="utf-8"
    )
    (OUT_DIR / "inventory_2011_login_hits.json").write_text(
        json.dumps(login_hits, indent=2), encoding="utf-8"
    )
    (OUT_DIR / "inventory_2011_key_assets.json").write_text(
        json.dumps(key_assets, indent=2), encoding="utf-8"
    )
    (OUT_DIR / "inventory_2011_interface_packages.json").write_text(
        json.dumps(iface_pkgs, indent=2), encoding="utf-8"
    )
    (OUT_DIR / "inventory_2011_loc_login_strings.json").write_text(
        json.dumps(loc_samples, indent=2), encoding="utf-8"
    )

    with (OUT_DIR / "inventory_2011_focus.csv").open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=["rel_path", "size", "ext", "login_hint"])
        w.writeheader()
        w.writerows(all_rows)

    # Human-readable listing for verification capture
    lines = [
        f"ROOT={ROOT}",
        f"focus_rows={len(all_rows)} login_hints={len(login_hits)}",
        "KEY_ASSETS:",
    ]
    for ka in key_assets:
        lines.append(f"  {'OK' if ka['exists'] else 'MISSING'} {ka['size']:12d}  {ka['rel_path']}")
    lines.append("INTERFACE_LOGINISH:")
    for pkg in iface_pkgs:
        if LOGIN_HINT.search(pkg["name"]):
            lines.append(f"  {pkg['size']:12d}  {pkg['name']}")
    lines.append("MOVIES:")
    for m in movies:
        lines.append(f"  {m['size']:12d}  {m['name']}")
    listing = "\n".join(lines) + "\n"
    (OUT_DIR / "inventory_2011_listing.txt").write_text(listing, encoding="utf-8")
    print(listing)
    print("wrote", OUT_DIR)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
