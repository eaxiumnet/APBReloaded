#!/usr/bin/env python3
"""Task-19 Phases B+C: per-district mesh evidence + ledger rows.

For each district placement set:
1. Collect distinct mesh_path (package.object) whose ue_path leaf resolves to an
   imported uasset under Content/Imported/Districts/<district>.
2. Run build_missing_district_meshes.py to convert pskx -> obj and emit D17
   evidence records (retail upk hash, pskx hash, obj hash).
3. Run record_district_mesh_import.py to write ledger rows (d17_evidence wired).

Dry-run prints per-district counts; --apply executes the two tools.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PLACEMENTS = ROOT / "Content" / "Data" / "district_placements"
IMPORTED = ROOT / "Content" / "Imported" / "Districts"
ASSET_ROOT = ROOT / "Content" / "Extracted" / "Task19_obj"
EXTRACTED_ROOT = ROOT / "Content" / "Extracted" / "Task19"
RETAIL = Path("C:/Program Files (x86)/Steam/steamapps/common/APB Reloaded/APBGame/Content/Release/Packages")
EVIDENCE_DIR = ROOT / "work" / "evidence" / "district19"
BUILD = ROOT / "tools" / "scripts" / "build_missing_district_meshes.py"
RECORD = ROOT / "tools" / "scripts" / "record_district_mesh_import.py"

DISTRICTS = ["Financial", "Waterfront", "Asylum", "Beacon", "Crate", "Social"]


def district_manifests(district: str) -> list[Path]:
    return sorted(p for p in PLACEMENTS.glob("*.json") if p.stem.lower().startswith(district.lower()))


def canonical_manifest(district: str) -> Path:
    files = district_manifests(district)
    best = files[0]
    best_score = 0
    for f in files:
        try:
            d = json.loads(f.read_text(encoding="utf-8"))
        except Exception:
            continue
        if not isinstance(d, dict):
            continue
        score = sum(1 for r in d.get("placements", []) if r.get("ue_path"))
        if score > best_score:
            best, best_score = f, score
    return best


def pskx_index() -> dict[str, Path]:
    """stem(lower) -> pskx path for Task19 extractions (unique stems only)."""
    index: dict[str, list[Path]] = {}
    for root in (EXTRACTED_ROOT,):
        for extension in (".pskx", ".psk"):
            for candidate in root.rglob(f"*{extension}"):
                index.setdefault(candidate.stem.lower(), []).append(candidate.resolve())
    unique: dict[str, Path] = {}
    for stem, paths in index.items():
        if len(paths) == 1:
            unique[stem] = paths[0]
    return unique


def mesh_obligations(district: str) -> dict[str, str]:
    """mesh_path -> ue_leaf for placements whose uasset exists on disk.

    Rows with a package-qualified mesh_path use it directly. Rows where the
    placement only carries a resolved ue_path (mesh_path null) are bound by
    leaf: the Task19 pskx with the same stem identifies the source package.
    """
    oblig: dict[str, str] = {}
    pskx = pskx_index()
    for f in district_manifests(district):
        try:
            d = json.loads(f.read_text(encoding="utf-8"))
        except Exception:
            continue
        if not isinstance(d, dict):
            continue
        for r in d.get("placements", []):
            mp = r.get("mesh_path")
            ue = r.get("ue_path")
            if not isinstance(ue, str):
                continue
            leaf = ue.rsplit("/", 1)[-1].split(".", 1)[0]
            if not leaf:
                continue
            uasset = IMPORTED / district / f"{leaf}.uasset"
            if not uasset.is_file():
                continue
            if isinstance(mp, str) and "." in mp:
                oblig[mp] = leaf
                continue
            # mesh_path null: synthesize package from the unique pskx stem.
            # umodel layout is <task19>/<pkgdir>/<ExportDir>/StaticMesh3/<file>.pskx
            # where ExportDir is the upk stem (e.g. `2mScaffold` or
            # `FinancialDistrict_FinancialDistrict_Block01_Package`).
            psk = pskx.get(leaf.lower())
            if psk is None or len(psk.parts) < 3:
                continue
            package = psk.parts[-3]
            oblig[f"{package}.{leaf}"] = leaf
    return oblig


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--district", choices=DISTRICTS + ["all"], default="all")
    args = parser.parse_args()

    districts = DISTRICTS if args.district == "all" else [args.district]
    EVIDENCE_DIR.mkdir(parents=True, exist_ok=True)
    total_rows = 0
    for district in districts:
        oblig = mesh_obligations(district)
        canonical = canonical_manifest(district)
        evidence = EVIDENCE_DIR / f"{district.lower()}.json"
        print(f"DISTRICT {district}: obligations={len(oblig)} canonical={canonical.name}")
        if not args.apply:
            total_rows += len(oblig)
            continue
        mesh_paths = sorted(oblig.keys())
        list_file = EVIDENCE_DIR / f"{district.lower()}_mesh_paths.txt"
        list_file.write_text("\n".join(mesh_paths) + "\n", encoding="utf-8")
        build_cmd = [
            sys.executable, str(BUILD), "--district", district,
            "--manifest", str(canonical),
            "--asset-root", str(ASSET_ROOT / district),
            "--extracted-root", str(EXTRACTED_ROOT),
            "--retail-packages-root", str(RETAIL),
            "--evidence", str(evidence),
            "--mesh-list-file", str(list_file),
        ]
        proc = subprocess.run(build_cmd, capture_output=True, text=True)
        if proc.returncode != 0:
            print(f"  BUILD_FAIL {district} rc={proc.returncode}\n{proc.stderr[-1200:]}")
            return 1
        print(f"  build: {proc.stdout.strip().splitlines()[-1]}")
        record_cmd = [
            sys.executable, str(RECORD),
            "--evidence", str(evidence),
            "--manifest", str(canonical),
        ]
        proc = subprocess.run(record_cmd, capture_output=True, text=True)
        if proc.returncode != 0:
            print(f"  RECORD_FAIL {district} rc={proc.returncode}\n{proc.stderr[-1200:]}")
            return 1
        print(f"  record: {proc.stdout.strip().splitlines()[-1]}")
        total_rows += len(oblig)
    print(f"TOTAL obligations (dry run) / rows written (apply): {total_rows}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
