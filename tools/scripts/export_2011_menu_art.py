#!/usr/bin/env python3
"""Batch-export 2011 (pkg ver 547/31) menu art + fonts to PNG.

M3 task: APBMenus_Art*.upk, APBMenus_Font.upk, APBMenus_FrontEnd.upk,
APBMenus_Skins.upk (APBMenus_Art_Jonathan is inside the Art* glob)
  -> Content/Extracted/2011/MenuArt/<Package>/<ExportClass>/<name>.png

Strategy (rerunnable):
  1. Reuse the verified full Interface umodel dump at
     Content/Extracted/2011/UI/umodel/<Package> (TGA) when present.
  2. If a matching package has no TGA in the dump, run umodel -export fresh.
  3. Convert TGA -> PNG with Pillow, preserving folder structure.

Usage:
  python tools/scripts/export_2011_menu_art.py            # convert (skip existing PNG)
  python tools/scripts/export_2011_menu_art.py --force    # re-convert everything
"""
from __future__ import annotations

import argparse
import json
import subprocess
import time
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
IFACE = (
    ROOT
    / "2011 apb"
    / "APB All Points Bulletin"
    / "APB North America"
    / "APBGame"
    / "Content"
    / "Interface"
)
UMODEL = ROOT / "tools" / "UEViewer" / "umodel_64.exe"
if not UMODEL.is_file():
    UMODEL = ROOT / "tools" / "UEViewer" / "umodel.exe"
DUMP = ROOT / "Content" / "Extracted" / "2011" / "UI" / "umodel"
OUT = ROOT / "Content" / "Extracted" / "2011" / "MenuArt"


def wanted(pkg_stem: str) -> bool:
    n = pkg_stem.lower()
    return (
        n.startswith("apbmenus_art")  # covers APBMenus_Art + _Jonathan + family
        or n in ("apbmenus_font", "apbmenus_frontend", "apbmenus_skins")
    )


def ensure_tga(pkg_stem: str, log: list[str]) -> Path | None:
    """Return the dump folder containing TGAs for pkg, exporting if missing."""
    d = DUMP / pkg_stem
    if d.is_dir() and any(d.rglob("*.tga")):
        return d
    if not (IFACE / f"{pkg_stem}.upk").is_file():
        log.append(f"{pkg_stem}: no .upk in Interface, skipped")
        return None
    d.mkdir(parents=True, exist_ok=True)
    cmd = [
        str(UMODEL),
        f"-path={IFACE}",
        "-game=apb",
        "-export",
        f"-out={d}",
        pkg_stem,
    ]
    r = subprocess.run(
        cmd, capture_output=True, text=True, encoding="utf-8", errors="replace",
        timeout=600, cwd=str(UMODEL.parent),
    )
    log.append(f"{pkg_stem}: umodel exit={r.returncode}")
    return d if any(d.rglob("*.tga")) else None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--force", action="store_true", help="Overwrite existing PNGs")
    args = ap.parse_args()

    pkgs = sorted(p.name for p in DUMP.iterdir() if p.is_dir() and wanted(p.name))
    # also pick up Interface packages matching the filter that were never dumped
    for upk in sorted(IFACE.glob("*.upk")):
        if wanted(upk.stem) and upk.stem not in pkgs:
            pkgs.append(upk.stem)

    log: list[str] = []
    t0 = time.time()
    total_tga = total_png = skipped_png = failed = 0
    per_pkg: dict[str, dict] = {}
    for stem in pkgs:
        src = ensure_tga(stem, log)
        if src is None:
            per_pkg[stem] = {"tga": 0, "png": 0, "status": "no_textures"}
            continue
        tgas = sorted(src.rglob("*.tga"))
        png_n = 0
        for tga in tgas:
            total_tga += 1
            rel = tga.relative_to(src)
            # dump nests as <Pkg>/<Pkg>/<Class>/x.tga — collapse duplicate stem
            parts = [p for p in rel.parts if p != stem]
            dst = (OUT / stem / Path(*parts)).with_suffix(".png")
            if dst.is_file() and not args.force:
                skipped_png += 1
                png_n += 1
                continue
            dst.parent.mkdir(parents=True, exist_ok=True)
            try:
                with Image.open(tga) as im:
                    im.save(dst)
                png_n += 1
                total_png += 1
            except Exception as e:  # noqa: BLE001 - record and continue batch
                failed += 1
                log.append(f"{tga}: PNG convert failed: {e}")
        per_pkg[stem] = {"tga": len(tgas), "png": png_n, "status": "ok"}

    manifest = {
        "source_packages": str(IFACE),
        "tga_source": str(DUMP),
        "out": str(OUT),
        "packages": per_pkg,
        "package_count": len(pkgs),
        "tga_total": total_tga,
        "png_written": total_png,
        "png_skipped_existing": skipped_png,
        "convert_failures": failed,
        "elapsed_sec": round(time.time() - t0, 2),
        "log": log,
    }
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "menuart_manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(
        f"packages={len(pkgs)} tga={total_tga} png_written={total_png} "
        f"skipped={skipped_png} failures={failed} -> {OUT / 'menuart_manifest.json'}"
    )
    return 0 if failed == 0 else 2


if __name__ == "__main__":
    raise SystemExit(main())
