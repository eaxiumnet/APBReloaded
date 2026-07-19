#!/usr/bin/env python3
"""Parallel umodel export of ALL 2011 .apb levels + related package UPKs.

Designed for high-core CPUs (e.g. Ryzen 7800X3D). Strategy:
  1. Discover every .apb
  2. Resolve related building packages (dedupe)
  3. Parallel-export unique packages + each .apb's baked textures
  4. Write per-level manifests pointing at shared package dirs

  python tools/scripts/export_apb_level_parallel.py
  python tools/scripts/export_apb_level_parallel.py --workers 14
  python tools/scripts/export_apb_level_parallel.py --workers 12 --skip-apb-textures
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
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
SHARED = OUT_ROOT / "_shared_packages"


def list_apbs() -> list[Path]:
    seen: set[str] = set()
    out: list[Path] = []
    for p in CONTENT.rglob("*"):
        if not p.is_file() or p.suffix.lower() != ".apb":
            continue
        k = str(p.resolve()).lower()
        if k in seen:
            continue
        seen.add(k)
        out.append(p)
    out.sort(key=lambda p: (-p.stat().st_size, str(p)))
    return out


def related_packages(apb: Path) -> list[Path]:
    stem = apb.stem
    found: list[Path] = []
    try:
        parts = apb.relative_to(CONTENT).parts
    except ValueError:
        return []
    if parts:
        district = CONTENT / parts[0]
        pkg_root = district / "Packages"
        if pkg_root.is_dir():
            m = re.search(r"Block(\d+)", stem, re.I)
            if m:
                bn = m.group(1)
                for p in pkg_root.rglob("*.upk"):
                    if re.search(rf"Block0*{bn}\b", p.stem, re.I) or f"Block{bn}" in p.stem:
                        found.append(p)
            for key in ("ArtProps", "MASTER", "Minimap", "MiniMap"):
                if key.lower() in stem.lower():
                    for p in pkg_root.rglob("*.upk"):
                        if key.lower() in p.stem.lower():
                            found.append(p)
    if not found:
        low = stem.lower().replace("_", "")
        for p in CONTENT.rglob("*.upk"):
            if low in p.stem.lower().replace("_", "") or stem.lower() in p.stem.lower():
                found.append(p)
    uniq: dict[str, Path] = {}
    for p in found:
        uniq[str(p.resolve()).lower()] = p
    return sorted(uniq.values(), key=lambda p: -p.stat().st_size)


def already_done(out_dir: Path) -> bool:
    if not out_dir.is_dir():
        return False
    # any mesh or at least one tga counts as done
    for pat in ("*.pskx", "*.psk", "*.tga"):
        if any(out_dir.rglob(pat)):
            return True
    return False


def export_one(job: dict) -> dict:
    """Worker: run one umodel export. job keys: stem, out, kind, src_hint."""
    stem = job["stem"]
    out = Path(job["out"])
    kind = job.get("kind", "pkg")
    if already_done(out):
        return {
            "stem": stem,
            "out": str(out),
            "kind": kind,
            "ok": True,
            "skipped": True,
            "exit": 0,
            "secs": 0.0,
        }
    out.mkdir(parents=True, exist_ok=True)
    umodel = job.get("umodel") or str(UMODEL)
    content = job.get("content") or str(CONTENT)
    cmd = [
        umodel,
        f"-path={content}",
        "-game=apb",
        "-export",
        f"-out={out}",
        stem,
    ]
    t0 = time.time()
    try:
        r = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=job.get("timeout", 600),
            cwd=str(Path(umodel).parent),
        )
        text = (r.stdout or "") + (r.stderr or "")
        ok = r.returncode == 0 or "Exported" in text or "Loading package" in text
        return {
            "stem": stem,
            "out": str(out),
            "kind": kind,
            "ok": ok,
            "skipped": False,
            "exit": r.returncode,
            "secs": round(time.time() - t0, 2),
            "tail": text[-400:] if not ok else "",
        }
    except subprocess.TimeoutExpired:
        return {
            "stem": stem,
            "out": str(out),
            "kind": kind,
            "ok": False,
            "skipped": False,
            "exit": -1,
            "secs": round(time.time() - t0, 2),
            "tail": "timeout",
        }
    except Exception as e:
        return {
            "stem": stem,
            "out": str(out),
            "kind": kind,
            "ok": False,
            "skipped": False,
            "exit": -2,
            "secs": round(time.time() - t0, 2),
            "tail": str(e),
        }


def index_upks() -> dict[str, Path]:
    """stem -> path for all Content UPKs (first wins if dup names)."""
    idx: dict[str, Path] = {}
    for p in CONTENT.rglob("*.upk"):
        idx.setdefault(p.stem, p)
    return idx


def related_packages_fast(apb: Path, upk_index: dict[str, Path]) -> list[Path]:
    stem = apb.stem
    found: list[Path] = []
    try:
        parts = apb.relative_to(CONTENT).parts
    except ValueError:
        return []
    district_name = parts[0] if parts else ""
    m = re.search(r"Block(\d+)", stem, re.I)
    block = m.group(1) if m else None

    for ustem, path in upk_index.items():
        # prefer same district
        try:
            rel = path.relative_to(CONTENT)
            same_district = rel.parts and rel.parts[0] == district_name
        except ValueError:
            same_district = False
        if block and re.search(rf"Block0*{int(block)}\b", ustem, re.I):
            if same_district or "Package" in ustem:
                found.append(path)
                continue
        for key in ("ArtProps", "MASTER", "Minimap", "MiniMap"):
            if key.lower() in stem.lower() and key.lower() in ustem.lower() and same_district:
                found.append(path)
    if not found:
        low = stem.lower().replace("_", "")
        for ustem, path in upk_index.items():
            if low in ustem.lower().replace("_", "") or stem.lower() in ustem.lower():
                found.append(path)
    uniq = {str(p.resolve()).lower(): p for p in found}
    return sorted(uniq.values(), key=lambda p: -p.stat().st_size)


def build_jobs(skip_apb_textures: bool) -> tuple[list[dict], dict[str, list[str]]]:
    """Returns (jobs, level_stem -> list of shared package stems)."""
    apbs = list_apbs()
    print(f"  indexing UPKs...", flush=True)
    upk_index = index_upks()
    print(f"  upks={len(upk_index)} apbs={len(apbs)}", flush=True)

    level_pkgs: dict[str, list[str]] = {}
    pkg_jobs: dict[str, dict] = {}  # stem -> job
    apb_jobs: list[dict] = []
    sizes: dict[str, int] = {}

    SHARED.mkdir(parents=True, exist_ok=True)
    OUT_ROOT.mkdir(parents=True, exist_ok=True)

    for apb in apbs:
        pkgs = related_packages_fast(apb, upk_index)
        stems = []
        for pkg in pkgs:
            stems.append(pkg.stem)
            if pkg.stem not in pkg_jobs:
                pkg_jobs[pkg.stem] = {
                    "stem": pkg.stem,
                    "out": str(SHARED / pkg.stem),
                    "kind": "package",
                    "umodel": str(UMODEL),
                    "content": str(CONTENT),
                    "timeout": 600,
                }
                sizes[pkg.stem] = pkg.stat().st_size
        level_pkgs[apb.stem] = stems
        if not skip_apb_textures:
            apb_jobs.append(
                {
                    "stem": apb.stem,
                    "out": str(OUT_ROOT / apb.stem / "apb_textures"),
                    "kind": "apb_tex",
                    "umodel": str(UMODEL),
                    "content": str(CONTENT),
                    "timeout": 300,
                }
            )
            sizes[apb.stem] = apb.stat().st_size

    jobs = list(pkg_jobs.values()) + apb_jobs
    jobs.sort(key=lambda j: sizes.get(j["stem"], 0), reverse=True)
    return jobs, level_pkgs


def write_level_manifests(level_pkgs: dict[str, list[str]], job_results: dict[str, dict]) -> None:
    for level_stem, pkg_stems in level_pkgs.items():
        out = OUT_ROOT / level_stem
        out.mkdir(parents=True, exist_ok=True)
        # pointer files for shared packages
        for ps in pkg_stems:
            shared = SHARED / ps
            dest = out / "packages" / ps
            dest.mkdir(parents=True, exist_ok=True)
            (dest / "SHARED_EXPORT.txt").write_text(str(shared.resolve()), encoding="utf-8")
        # counts
        mesh = 0
        tga = 0
        for ps in pkg_stems:
            sp = SHARED / ps
            if sp.is_dir():
                mesh += sum(1 for _ in sp.rglob("*.pskx")) + sum(1 for _ in sp.rglob("*.psk"))
                tga += sum(1 for _ in sp.rglob("*.tga"))
        apb_tex = out / "apb_textures"
        if apb_tex.is_dir():
            tga += sum(1 for _ in apb_tex.rglob("*.tga"))
        man = {
            "level": level_stem,
            "out": str(out),
            "shared_packages": [str(SHARED / p) for p in pkg_stems],
            "mesh_count": mesh,
            "tga_count": tga,
            "view_shared": f'python tools/model_viewer/view_models.py --root "{SHARED}"',
            "view_level_tex": f'python tools/model_viewer/view_ui.py --root "{apb_tex}"'
            if apb_tex.is_dir()
            else None,
        }
        (out / "export_manifest.json").write_text(json.dumps(man, indent=2), encoding="utf-8")


def run_parallel(workers: int, skip_apb_textures: bool, limit_jobs: int = 0) -> int:
    if not UMODEL.is_file():
        print("missing umodel", UMODEL, file=sys.stderr)
        return 1
    if not CONTENT.is_dir():
        print("missing content", CONTENT, file=sys.stderr)
        return 1

    print(f"CPU workers={workers} umodel={UMODEL}")
    print("Building job list...")
    t0 = time.time()
    jobs, level_pkgs = build_jobs(skip_apb_textures=skip_apb_textures)
    if limit_jobs:
        jobs = jobs[:limit_jobs]
    n_pkg = sum(1 for j in jobs if j["kind"] == "package")
    n_apb = sum(1 for j in jobs if j["kind"] == "apb_tex")
    print(f"levels={len(level_pkgs)} unique_packages={n_pkg} apb_texture_jobs={n_apb} total_jobs={len(jobs)}")
    print(f"shared_out={SHARED}")

    # Filter already done for progress estimate
    pending = [j for j in jobs if not already_done(Path(j["out"]))]
    print(f"pending={len(pending)} already_done={len(jobs) - len(pending)}")

    results: dict[str, dict] = {}
    ok = fail = skipped = 0
    done = 0
    total = len(jobs)

    # ProcessPoolExecutor needs picklable worker at module level (export_one is)
    with ProcessPoolExecutor(max_workers=max(1, workers)) as ex:
        futs = {ex.submit(export_one, j): j for j in jobs}
        for fut in as_completed(futs):
            j = futs[fut]
            try:
                r = fut.result()
            except Exception as e:
                r = {
                    "stem": j["stem"],
                    "out": j["out"],
                    "kind": j.get("kind"),
                    "ok": False,
                    "skipped": False,
                    "exit": -3,
                    "secs": 0,
                    "tail": str(e),
                }
            results[f"{r['kind']}:{r['stem']}"] = r
            done += 1
            if r.get("skipped"):
                skipped += 1
            elif r.get("ok"):
                ok += 1
            else:
                fail += 1
            if done % 10 == 0 or done == total or not r.get("ok"):
                status = "skip" if r.get("skipped") else ("ok" if r.get("ok") else "FAIL")
                print(
                    f"[{done}/{total}] {status} {r.get('kind')} {r.get('stem')} "
                    f"{r.get('secs')}s  (ok={ok} fail={fail} skip={skipped})",
                    flush=True,
                )
                if not r.get("ok") and r.get("tail"):
                    print(f"    {r['tail'][:200]}", flush=True)

    print("Writing per-level manifests...")
    write_level_manifests(level_pkgs, results)

    disk_mesh = sum(1 for _ in OUT_ROOT.rglob("*.pskx")) + sum(1 for _ in OUT_ROOT.rglob("*.psk"))
    disk_tga = sum(1 for _ in OUT_ROOT.rglob("*.tga"))
    elapsed = round(time.time() - t0, 1)
    report = {
        "workers": workers,
        "jobs": len(jobs),
        "ok": ok,
        "fail": fail,
        "skipped": skipped,
        "levels": len(level_pkgs),
        "disk_mesh": disk_mesh,
        "disk_tga": disk_tga,
        "elapsed_sec": elapsed,
        "shared": str(SHARED),
        "results": list(results.values()),
    }
    (OUT_ROOT / "export_all_parallel_report.json").write_text(
        json.dumps(report, indent=2), encoding="utf-8"
    )
    (OUT_ROOT / "export_all_parallel_report.txt").write_text(
        f"workers={workers} jobs={len(jobs)} ok={ok} fail={fail} skip={skipped}\n"
        f"levels={len(level_pkgs)} disk_mesh={disk_mesh} disk_tga={disk_tga}\n"
        f"elapsed_sec={elapsed}\n"
        f"view: python tools/model_viewer/view_models.py --root \"{SHARED}\"\n",
        encoding="utf-8",
    )
    print(
        f"\nDONE workers={workers} ok={ok} fail={fail} skip={skipped} "
        f"mesh={disk_mesh} tga={disk_tga} elapsed={elapsed}s"
    )
    print(f"VIEW: python tools/model_viewer/view_models.py --root \"{SHARED}\"")
    return 0 if fail == 0 else 1


def main() -> int:
    # Default workers: leave 2 cores free on high-end CPUs
    default_workers = max(4, min(14, (os.cpu_count() or 8) - 2))
    ap = argparse.ArgumentParser()
    ap.add_argument("--workers", type=int, default=default_workers)
    ap.add_argument("--skip-apb-textures", action="store_true", help="Only export package UPKs (faster)")
    ap.add_argument("--limit-jobs", type=int, default=0, help="Debug: only N jobs")
    args = ap.parse_args()
    return run_parallel(args.workers, args.skip_apb_textures, args.limit_jobs)


if __name__ == "__main__":
    # Windows process pool needs freeze_support
    from multiprocessing import freeze_support

    freeze_support()
    raise SystemExit(main())
