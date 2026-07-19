#!/usr/bin/env python3
"""Export all 2011 APB Interface UPKs (main menu / UI art) via umodel.

  python tools/scripts/export_2011_ui.py
  python tools/scripts/export_2011_ui.py --menu-only   # FrontEnd + GameFlow + *Art*
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
UMODEL = ROOT / "tools" / "UEViewer" / "umodel_64.exe"
if not UMODEL.is_file():
    UMODEL = ROOT / "tools" / "UEViewer" / "umodel.exe"

IFACE = (
    ROOT
    / "2011 apb"
    / "APB All Points Bulletin"
    / "APB North America"
    / "APBGame"
    / "Content"
    / "Interface"
)
OUT = ROOT / "Content" / "Extracted" / "2011" / "UI" / "umodel"


def is_menu_package(name: str) -> bool:
    n = name.lower()
    keys = (
        "frontend",
        "gameflow",
        "art",
        "login",
        "charactercustomisation",
        "charactermenu",
        "skins",
        "font",
        "accept",
        "faction",
        "district",
        "options",
        "video",
        "gecko",
    )
    return any(k in n for k in keys)


def already_exported(pkg_stem: str, out: Path) -> bool:
    """Skip if umodel already wrote a folder for this package with any TGA."""
    d = out / pkg_stem
    if d.is_dir() and any(d.rglob("*.tga")):
        return True
    # sometimes nested as out/Pkg/Pkg/...
    if any(out.glob(f"**/{pkg_stem}/**/*.tga")):
        return True
    return False


def export_package(pkg_stem: str, path_root: Path, out: Path, timeout: int = 300) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    cmd = [
        str(UMODEL),
        f"-path={path_root}",
        "-game=apb",
        "-export",
        f"-out={out}",
        pkg_stem,
    ]
    try:
        r = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout,
            cwd=str(UMODEL.parent),
        )
        ok = r.returncode == 0 or "Loading package" in (r.stdout + r.stderr)
        return {
            "package": pkg_stem,
            "ok": ok,
            "exit": r.returncode,
            "cmd": " ".join(cmd),
            "tail": ((r.stdout or "") + (r.stderr or ""))[-800:],
        }
    except subprocess.TimeoutExpired:
        return {"package": pkg_stem, "ok": False, "exit": -1, "cmd": " ".join(cmd), "tail": "timeout"}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--menu-only", action="store_true", help="Export menu/login art packages only")
    ap.add_argument("--out", type=Path, default=OUT)
    ap.add_argument("--limit", type=int, default=0, help="Max packages (0=all)")
    ap.add_argument("--no-skip", action="store_true", help="Re-export even if TGA already present")
    ap.add_argument(
        "--small-first",
        action="store_true",
        help="Smaller packages first (safer after a crash; default for menu-only)",
    )
    args = ap.parse_args()

    if not IFACE.is_dir():
        print("missing Interface:", IFACE, file=sys.stderr)
        return 1
    if not UMODEL.is_file():
        print("missing umodel:", UMODEL, file=sys.stderr)
        return 1

    # Prefer small-first after crash recovery to make progress without huge packages first
    reverse = not (args.small_first or args.menu_only)
    pkgs = sorted(IFACE.glob("*.upk"), key=lambda p: p.stat().st_size, reverse=reverse)
    if args.menu_only:
        pkgs = [p for p in pkgs if is_menu_package(p.stem)]
    if args.limit:
        pkgs = pkgs[: args.limit]

    print(f"exporting {len(pkgs)} packages -> {args.out}")
    t0 = time.time()
    results = []
    ok_n = fail_n = skip_n = 0
    for i, p in enumerate(pkgs, 1):
        if not args.no_skip and already_exported(p.stem, args.out):
            skip_n += 1
            print(f"[{i}/{len(pkgs)}] SKIP {p.name} (already has TGA)", flush=True)
            results.append({"package": p.stem, "ok": True, "exit": 0, "skipped": True})
            ok_n += 1
            continue
        # larger timeout for big art packs
        to = 600 if p.stat().st_size > 10_000_000 else 300
        print(f"[{i}/{len(pkgs)}] {p.name} ({p.stat().st_size // 1024} KB) ...", flush=True)
        res = export_package(p.stem, IFACE, args.out, timeout=to)
        results.append(res)
        if res["ok"]:
            ok_n += 1
            print(f"  OK exit={res['exit']}")
        else:
            fail_n += 1
            print(f"  FAIL exit={res['exit']} {res['tail'][:200]}")
        # brief pause so RAM can settle (crash-friendly)
        time.sleep(0.3)

    tga = list(args.out.rglob("*.tga"))
    psk = list(args.out.rglob("*.psk")) + list(args.out.rglob("*.pskx"))
    manifest = {
        "source": str(IFACE),
        "out": str(args.out),
        "packages_attempted": len(pkgs),
        "ok": ok_n,
        "fail": fail_n,
        "skipped": skip_n,
        "tga_count": len(tga),
        "mesh_count": len(psk),
        "elapsed_sec": round(time.time() - t0, 2),
        "results": results,
    }
    man = args.out / "export_manifest.json"
    args.out.mkdir(parents=True, exist_ok=True)
    man.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(
        f"done ok={ok_n} fail={fail_n} skipped={skip_n} "
        f"tga={len(tga)} meshes={len(psk)} -> {man}"
    )
    return 0 if ok_n else 2


if __name__ == "__main__":
    raise SystemExit(main())
