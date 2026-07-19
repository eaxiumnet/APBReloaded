#!/usr/bin/env python3
"""Export viewable meshes for a 2011 APB .apb level.

APB cooked levels are placement + lightmaps. Static meshes live in sibling
district package UPKs (e.g. FinancialDistrict/Packages/Buildings/*_Block09_Package.UPK).

  python tools/scripts/export_apb_level.py FinancialDistrict_Block09
  python tools/scripts/export_apb_level.py --list
  python tools/scripts/export_apb_level.py AssigningVFXPrefabs-testLevelMASTER
  python tools/scripts/export_apb_level.py --all-financial   # all block packages (heavy)

Then view:
  python tools/model_viewer/view_models.py --root Content/Extracted/2011/Levels/<name>
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CONTENT = (
    ROOT
    / "2011 apb"
    / "APB All Points Bulletin"
    / "APB North America"
    / "APBGame"
    / "Content"
)
UMODEL = ROOT / "tools" / "UEViewer" / "umodel_64.exe"
if not UMODEL.is_file():
    UMODEL = ROOT / "tools" / "UEViewer" / "umodel.exe"
OUT_ROOT = ROOT / "Content" / "Extracted" / "2011" / "Levels"


def find_apb(name: str) -> Path | None:
    stem = name.replace(".apb", "").replace(".APB", "")
    for p in CONTENT.rglob("*"):
        if p.is_file() and p.suffix.lower() == ".apb" and p.stem.lower() == stem.lower():
            return p
    return None


def list_levels() -> list[Path]:
    return sorted(CONTENT.rglob("*.apb"), key=lambda p: (-p.stat().st_size, str(p)))


def related_packages(apb: Path) -> list[Path]:
    """Find UPK packages that hold meshes for this level."""
    stem = apb.stem  # FinancialDistrict_Block09
    found: list[Path] = []
    # 1) Same district Packages folder
    # Maps/.../FinancialDistrict/Maps/X.apb -> FinancialDistrict/Packages/**
    parts = apb.relative_to(CONTENT).parts
    if len(parts) >= 2 and parts[0].endswith("District") or "District" in parts[0]:
        district = CONTENT / parts[0]
        pkg_root = district / "Packages"
        if pkg_root.is_dir():
            # Block number
            m = re.search(r"Block(\d+)", stem, re.I)
            if m:
                bn = m.group(1)
                for p in pkg_root.rglob("*.upk"):
                    if f"Block{bn}" in p.stem or f"Block{bn.zfill(2)}" in p.stem:
                        found.append(p)
            # master / art props
            for key in ("ArtProps", "MASTER", "Minimap", "Minimap"):
                if key.lower() in stem.lower():
                    for p in pkg_root.rglob("*.upk"):
                        if key.lower() in p.stem.lower():
                            found.append(p)

    # 2) Name substring match under Content/**/Packages
    if not found:
        for p in CONTENT.rglob("*.upk"):
            if stem.lower() in p.stem.lower() or stem.replace("_", "").lower() in p.stem.replace("_", "").lower():
                found.append(p)

    # 3) Content/Maps levels with no packages — only self (often empty mesh)
    # Prefer building packages
    uniq = []
    seen = set()
    for p in found:
        k = str(p.resolve()).lower()
        if k not in seen:
            seen.add(k)
            uniq.append(p)
    # Prefer larger packages first
    uniq.sort(key=lambda p: -p.stat().st_size)
    return uniq


# Global cache: package stem -> out dir already exported (shared across --all)
_PKG_CACHE: dict[str, Path] = {}


def package_already_exported(out_dir: Path) -> bool:
    if not out_dir.is_dir():
        return False
    return any(out_dir.rglob("*.pskx")) or any(out_dir.rglob("*.psk")) or any(out_dir.rglob("*.tga"))


def umodel_export(path_root: Path, package_stem: str, out: Path, meshes_only: bool = False) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    if package_already_exported(out):
        return {
            "cmd": f"(skip) {package_stem}",
            "ok": True,
            "exit": 0,
            "tail": "skipped existing export",
            "skipped": True,
        }
    cmd = [
        str(UMODEL),
        f"-path={path_root}",
        "-game=apb",
        "-export",
        f"-out={out}",
    ]
    if meshes_only:
        cmd.append("-meshes")
    cmd.append(package_stem)
    try:
        r = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=600,
            cwd=str(UMODEL.parent),
        )
        text = (r.stdout or "") + (r.stderr or "")
        ok = r.returncode == 0 or "Exported" in text or "Loading package" in text
        return {"cmd": " ".join(cmd), "ok": ok, "exit": r.returncode, "tail": text[-1500:], "skipped": False}
    except subprocess.TimeoutExpired:
        return {"cmd": " ".join(cmd), "ok": False, "exit": -1, "tail": "timeout", "skipped": False}


def export_level(
    name: str,
    include_apb_textures: bool = True,
    shared_pkg_root: Path | None = None,
) -> dict:
    """Export one level. Returns summary dict (not exit code)."""
    if isinstance(name, Path):
        apb = name
    else:
        apb = find_apb(name)
        if not apb:
            return {"level": name, "ok": False, "error": "not found"}
    if not UMODEL.is_file():
        return {"level": str(apb), "ok": False, "error": f"umodel missing {UMODEL}"}

    out = OUT_ROOT / apb.stem
    out.mkdir(parents=True, exist_ok=True)
    man_path = out / "export_manifest.json"
    # Resume: if manifest exists and has any mesh/tga, skip full redo unless empty
    if man_path.is_file():
        try:
            old = json.loads(man_path.read_text(encoding="utf-8"))
            if (old.get("mesh_count") or 0) + (old.get("tga_count") or 0) > 0:
                psk_n = len(list(out.rglob("*.psk"))) + len(list(out.rglob("*.pskx")))
                tga_n = len(list(out.rglob("*.tga")))
                if psk_n + tga_n > 0:
                    return {
                        "level": str(apb),
                        "out": str(out),
                        "ok": True,
                        "skipped": True,
                        "mesh_count": psk_n,
                        "tga_count": tga_n,
                    }
        except json.JSONDecodeError:
            pass

    pkgs = related_packages(apb)
    results = []
    pkg_out_root = shared_pkg_root if shared_pkg_root else (out / "packages")

    for pkg in pkgs:
        # Shared package cache by stem under Levels/_shared_packages
        cache_key = pkg.stem.lower()
        if cache_key in _PKG_CACHE and package_already_exported(_PKG_CACHE[cache_key]):
            dest = out / "packages" / pkg.stem
            # pointer file for viewer roots that expect packages under level
            dest.mkdir(parents=True, exist_ok=True)
            ptr = dest / "SHARED_EXPORT.txt"
            ptr.write_text(str(_PKG_CACHE[cache_key]), encoding="utf-8")
            results.append(
                {
                    "package": pkg.name,
                    "ok": True,
                    "skipped": True,
                    "shared": str(_PKG_CACHE[cache_key]),
                }
            )
            continue

        dest = pkg_out_root / pkg.stem
        print(f"  package {pkg.name} ...", flush=True)
        r = umodel_export(CONTENT, pkg.stem, dest, meshes_only=False)
        results.append({"package": pkg.name, **r})
        if r["ok"]:
            _PKG_CACHE[cache_key] = dest
            # also link under level out
            if shared_pkg_root is not None:
                level_pkg = out / "packages" / pkg.stem
                level_pkg.mkdir(parents=True, exist_ok=True)
                (level_pkg / "SHARED_EXPORT.txt").write_text(str(dest), encoding="utf-8")
        print("    OK" if r["ok"] else f"    FAIL {r.get('tail', '')[:100]}")
        time.sleep(0.15)

    if include_apb_textures:
        print(f"  apb textures {apb.name} ...", flush=True)
        r = umodel_export(CONTENT, apb.stem, out / "apb_textures", meshes_only=False)
        results.append({"package": apb.name, **r})
        print("    OK" if r["ok"] else f"    FAIL {r.get('tail', '')[:100]}")
        time.sleep(0.1)

    # Count meshes: level tree + shared package paths referenced
    psk = list(out.rglob("*.psk")) + list(out.rglob("*.pskx"))
    tga = list(out.rglob("*.tga"))
    for r in results:
        shared = r.get("shared")
        if shared:
            sp = Path(shared)
            if sp.is_dir():
                psk += list(sp.rglob("*.psk")) + list(sp.rglob("*.pskx"))
                tga += list(sp.rglob("*.tga"))
    # unique by path
    psk = list({str(p): p for p in psk}.values())
    tga = list({str(p): p for p in tga}.values())

    man = {
        "level": str(apb),
        "out": str(out),
        "packages": [str(p) for p in pkgs],
        "mesh_count": len(psk),
        "tga_count": len(tga),
        "results": results,
        "view": f'python tools/model_viewer/view_models.py --root "{out}"',
    }
    man_path.write_text(json.dumps(man, indent=2), encoding="utf-8")
    print(f"  DONE meshes={len(psk)} tga={len(tga)}")
    return {
        "level": str(apb),
        "out": str(out),
        "ok": True,
        "skipped": False,
        "mesh_count": len(psk),
        "tga_count": len(tga),
        "package_count": len(pkgs),
    }


def export_all_apb(limit: int = 0) -> int:
    levels = list_levels()
    if limit:
        levels = levels[:limit]
    shared = OUT_ROOT / "_shared_packages"
    shared.mkdir(parents=True, exist_ok=True)
    # Seed cache from existing shared exports
    if shared.is_dir():
        for d in shared.iterdir():
            if d.is_dir() and package_already_exported(d):
                _PKG_CACHE[d.name.lower()] = d

    print(f"Exporting {len(levels)} .apb levels -> {OUT_ROOT}")
    print(f"Shared packages: {shared}")
    t0 = time.time()
    summaries = []
    ok = fail = skip = 0
    for i, apb in enumerate(levels, 1):
        print(f"\n[{i}/{len(levels)}] {apb.relative_to(CONTENT)} ({apb.stat().st_size // 1024} KB)", flush=True)
        s = export_level(apb, include_apb_textures=True, shared_pkg_root=shared)
        summaries.append(s)
        if s.get("skipped"):
            skip += 1
            print("  (resume skip)")
        elif s.get("ok"):
            ok += 1
        else:
            fail += 1
            print("  FAIL", s.get("error"))

    elapsed = round(time.time() - t0, 1)
    total_mesh = sum(s.get("mesh_count") or 0 for s in summaries)
    total_tga = sum(s.get("tga_count") or 0 for s in summaries)
    # unique meshes on disk under Levels
    disk_mesh = len(list(OUT_ROOT.rglob("*.pskx"))) + len(list(OUT_ROOT.rglob("*.psk")))
    disk_tga = len(list(OUT_ROOT.rglob("*.tga")))
    report = {
        "levels_total": len(levels),
        "ok": ok,
        "fail": fail,
        "skipped_resume": skip,
        "sum_mesh_counts": total_mesh,
        "sum_tga_counts": total_tga,
        "disk_mesh_files": disk_mesh,
        "disk_tga_files": disk_tga,
        "elapsed_sec": elapsed,
        "out_root": str(OUT_ROOT),
        "levels": summaries,
    }
    rep_path = OUT_ROOT / "export_all_apb_report.json"
    rep_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    (OUT_ROOT / "export_all_apb_report.txt").write_text(
        "\n".join(
            [
                f"levels={len(levels)} ok={ok} fail={fail} skipped={skip}",
                f"disk_mesh={disk_mesh} disk_tga={disk_tga}",
                f"elapsed_sec={elapsed}",
                f"out={OUT_ROOT}",
            ]
        )
        + "\n",
        encoding="utf-8",
    )
    print(f"\n==== ALL DONE ====")
    print(f"levels={len(levels)} ok={ok} fail={fail} skipped={skip}")
    print(f"disk meshes={disk_mesh} tga={disk_tga} elapsed={elapsed}s")
    print(f"report {rep_path}")
    return 0 if fail == 0 else 1


def main() -> int:
    ap = argparse.ArgumentParser(description="Export 2011 APB .apb level meshes for viewing")
    ap.add_argument("level", nargs="?", help="Level name e.g. FinancialDistrict_Block09")
    ap.add_argument("--list", action="store_true", help="List all .apb under Content")
    ap.add_argument("--all", action="store_true", help="Export ALL .apb levels under Content")
    ap.add_argument("--limit", type=int, default=0, help="With --all, only first N levels")
    ap.add_argument("--all-financial", action="store_true", help="Export all FD building packages")
    ap.add_argument("--all-waterfront", action="store_true")
    args = ap.parse_args()

    if args.list:
        for p in list_levels():
            rel = p.relative_to(CONTENT)
            print(f"{p.stat().st_size // 1024:6} KB  {rel}")
        return 0

    if args.all:
        return export_all_apb(limit=args.limit)

    if args.all_financial or args.all_waterfront:
        district = "FinancialDistrict" if args.all_financial else "WaterfrontDistrict"
        pkg_dir = CONTENT / district / "Packages" / "Buildings"
        if not pkg_dir.is_dir():
            print("missing", pkg_dir)
            return 1
        out = OUT_ROOT / f"{district}_AllBuildings"
        for i, pkg in enumerate(sorted(pkg_dir.glob("*.upk")), 1):
            print(f"[{i}] {pkg.name}")
            umodel_export(CONTENT, pkg.stem, out / pkg.stem)
            time.sleep(0.15)
        psk = list(out.rglob("*.pskx")) + list(out.rglob("*.psk"))
        print(f"total meshes {len(psk)} -> {out}")
        return 0

    if not args.level:
        ap.print_help()
        print("\nExamples:")
        print("  python tools/scripts/export_apb_level.py --list")
        print("  python tools/scripts/export_apb_level.py FinancialDistrict_Block09")
        print("  python tools/scripts/export_apb_level.py --all")
        return 1

    s = export_level(args.level)
    if not s.get("ok"):
        print(s.get("error"), file=sys.stderr)
        return 1
    print(f"\nDONE meshes={s.get('mesh_count')} tga={s.get('tga_count')} -> {s.get('out')}")
    print(f"VIEW: python tools/model_viewer/view_models.py --root \"{s.get('out')}\"")
    return 0 if (s.get("mesh_count") or 0) + (s.get("tga_count") or 0) > 0 else 2


if __name__ == "__main__":
    raise SystemExit(main())
