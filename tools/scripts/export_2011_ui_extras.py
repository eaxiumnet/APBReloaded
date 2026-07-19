#!/usr/bin/env python3
"""Export remaining 2011 2D/UI sources outside the menu-only pass.

- All Interface/*.upk not yet exported (HUD, marketplace, wardrobe, etc.)
- Minimap UPKs, HUDMaterials, FX_HUDMarkers
- Copy Splash BMPs + Movies index into Content/Extracted/2011/UI/

Safe to re-run (skips packages that already have TGA).
"""
from __future__ import annotations

import json
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "scripts"))
from export_2011_ui import (  # noqa: E402
    IFACE,
    UMODEL,
    already_exported,
    export_package,
)

OUT_UI = ROOT / "Content" / "Extracted" / "2011" / "UI" / "umodel"
OUT_EXTRA = ROOT / "Content" / "Extracted" / "2011" / "UI" / "extra"
OUT_LOOSE = ROOT / "Content" / "Extracted" / "2011" / "UI" / "loose"
CONTENT = (
    ROOT
    / "2011 apb"
    / "APB All Points Bulletin"
    / "APB North America"
    / "APBGame"
    / "Content"
)
APBGAME = CONTENT.parent


def export_at(path_root: Path, pkg: Path, out: Path) -> dict:
    if already_exported(pkg.stem, out):
        return {"package": pkg.stem, "ok": True, "skipped": True, "path": str(pkg)}
    to = 900 if pkg.stat().st_size > 5_000_000 else 400
    print(f"  export {pkg.name} ({pkg.stat().st_size // 1024} KB) path={path_root}", flush=True)
    r = export_package(pkg.stem, path_root, out, timeout=to)
    r["path"] = str(pkg)
    time.sleep(0.4)
    return r


def main() -> int:
    results: list[dict] = []
    OUT_UI.mkdir(parents=True, exist_ok=True)
    OUT_EXTRA.mkdir(parents=True, exist_ok=True)
    OUT_LOOSE.mkdir(parents=True, exist_ok=True)

    # 1) Remaining Interface packages
    print("=== Interface remaining ===")
    iface_pkgs = sorted(IFACE.glob("*.upk"), key=lambda p: p.stat().st_size)
    ok = fail = skip = 0
    for i, p in enumerate(iface_pkgs, 1):
        if already_exported(p.stem, OUT_UI):
            skip += 1
            if i % 20 == 0:
                print(f"[{i}/{len(iface_pkgs)}] skip so far {skip}")
            continue
        print(f"[{i}/{len(iface_pkgs)}] {p.name}", flush=True)
        r = export_package(p.stem, IFACE, OUT_UI, timeout=900 if p.stat().st_size > 1_000_000 else 400)
        results.append({"batch": "interface", **r})
        if r["ok"]:
            ok += 1
            print("  OK")
        else:
            fail += 1
            print("  FAIL", r.get("tail", "")[:150])
        time.sleep(0.35)
    print(f"Interface done ok={ok} fail={fail} skipped_existing={skip}")

    # 2) Extra art packages
    print("=== Extra 2D packages ===")
    extras: list[tuple[Path, Path]] = []
    for p in CONTENT.rglob("*Minimap*.upk"):
        extras.append((p.parent, p))
    for name in ("HUDMaterials.upk",):
        p = CONTENT / "DesignObjects" / name
        if p.is_file():
            extras.append((p.parent, p))
    fx = CONTENT / "Packages" / "FX_HUDMarkers.upk"
    if fx.is_file():
        extras.append((fx.parent, fx))
    pre = CONTENT / "Packages" / "PreProductionTestMinimapAssets.upk"
    if pre.is_file():
        extras.append((pre.parent, pre))

    for path_root, pkg in extras:
        r = export_at(path_root, pkg, OUT_EXTRA)
        results.append({"batch": "extra", **r})
        print(" ", r.get("package"), "skip" if r.get("skipped") else ("OK" if r.get("ok") else "FAIL"))

    # 3) Loose splash + movie listing
    print("=== Loose splash / movies ===")
    splash = APBGAME / "Splash" / "PC"
    for bmp in splash.glob("*.bmp"):
        dest = OUT_LOOSE / "Splash" / bmp.name
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(bmp, dest)
        print("  copied", dest)
    movies = APBGAME / "Movies"
    movie_list = []
    for bik in sorted(movies.glob("*.bik")):
        movie_list.append({"name": bik.name, "bytes": bik.stat().st_size, "path": str(bik)})
    (OUT_LOOSE / "movies_index.json").write_text(json.dumps(movie_list, indent=2), encoding="utf-8")
    (OUT_LOOSE / "README_movies.txt").write_text(
        "Bink movies are NOT TGA. Open with VLC:\n"
        + "\n".join(m["path"] for m in movie_list)
        + "\n",
        encoding="utf-8",
    )

    tga_ui = sum(1 for _ in OUT_UI.rglob("*.tga"))
    tga_ex = sum(1 for _ in OUT_EXTRA.rglob("*.tga")) if OUT_EXTRA.is_dir() else 0
    summary = {
        "interface_ok_new": ok,
        "interface_fail": fail,
        "interface_skipped_existing": skip,
        "tga_in_umodel": tga_ui,
        "tga_in_extra": tga_ex,
        "results": results[-50:],  # tail only in summary file full below
        "movies": movie_list,
    }
    full = {
        **summary,
        "results": results,
        "tga_total_ui_tree": tga_ui + tga_ex,
    }
    out_man = ROOT / "Content" / "Extracted" / "2011" / "UI" / "export_extras_manifest.json"
    out_man.write_text(json.dumps(full, indent=2), encoding="utf-8")
    print(f"TOTAL tga umodel={tga_ui} extra={tga_ex} manifest={out_man}")
    return 0 if fail == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
