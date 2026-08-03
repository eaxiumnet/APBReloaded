#!/usr/bin/env python3
"""Task-19 Phase A: extract pskx for placement source packages via umodel.

Collects distinct (package, object) pairs from all district placement JSONs,
maps each package to its retail .upk (by stem substring match), and runs
umodel -export for every package whose pskx output is not already present
under Content/Extracted (excluding the 2011 build tree).

Output: Content/Extracted/Task19/<package>/... (umodel's layout). A run log
lists extracted vs skipped packages. No ledger or allowlist mutation here.
"""
from __future__ import annotations

import json
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RETAIL = Path("C:/Program Files (x86)/Steam/steamapps/common/APB Reloaded/APBGame/Content/Release")
UMODEL = ROOT / "tools" / "UEViewer" / "umodel_64.exe"
OUT = ROOT / "Content" / "Extracted" / "Task19"
EXTRACTED = ROOT / "Content" / "Extracted"
LOG = ROOT / ".omo" / "task19_extract.log"


def collect_obligations() -> dict[str, set[str]]:
    obligations: dict[str, set[str]] = {}
    for f in sorted((ROOT / "Content" / "Data" / "district_placements").glob("*.json")):
        try:
            d = json.loads(f.read_text(encoding="utf-8"))
        except Exception:
            continue
        if not isinstance(d, dict):
            continue
        for row in d.get("placements", []):
            mp = row.get("mesh_path")
            if not isinstance(mp, str) or "." not in mp:
                continue
            pkg, obj = mp.split(".", 1)
            obligations.setdefault(pkg, set()).add(obj)
    return obligations


def find_upk(package: str) -> Path | None:
    low = package.lower()
    best: Path | None = None
    for upk in RETAIL.rglob("*.upk"):
        stem = upk.stem.lower()
        if stem == low:
            return upk
        if low in stem:
            if best is None or len(stem) < len(best.stem):
                best = upk
    return best


def pskx_present(package: str, objects: set[str]) -> bool:
    for root in (EXTRACTED / "Task19", EXTRACTED / "G1PayloadBatch2Exact"):
        if not root.is_dir():
            continue
        found = 0
        for psk in root.rglob("*.pskx"):
            if psk.stem in objects:
                found += 1
        if found >= len(objects):
            return True
    return False


def main() -> int:
    obligations = collect_obligations()
    print(f"distinct packages: {len(obligations)} objects: {sum(len(v) for v in obligations.values())}", flush=True)
    OUT.mkdir(parents=True, exist_ok=True)
    log_lines: list[str] = []
    extracted = 0
    skipped = 0
    failed: list[str] = []
    t0 = time.time()
    for package, objects in sorted(obligations.items()):
        if pskx_present(package, objects):
            log_lines.append(f"SKIP {package} (pskx present)")
            skipped += 1
            continue
        upk = find_upk(package)
        if upk is None:
            log_lines.append(f"MISS_UPK {package}")
            failed.append(package)
            continue
        out = OUT / package
        out.mkdir(parents=True, exist_ok=True)
        cmd = [
            str(UMODEL), "-export", "-game=apb",
            f"-path={RETAIL}", f"-out={out}", upk.stem,
        ]
        try:
            proc = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
            if proc.returncode != 0:
                log_lines.append(f"FAIL {package} rc={proc.returncode} {proc.stderr[-200:]}")
                failed.append(package)
                continue
            produced = list(out.rglob("*.pskx"))
            log_lines.append(f"OK {package} pskx={len(produced)} ({time.time() - t0:.1f}s)")
            extracted += 1
        except Exception as exc:
            log_lines.append(f"ERR {package} {exc}")
            failed.append(package)
    LOG.parent.mkdir(parents=True, exist_ok=True)
    LOG.write_text("\n".join(log_lines) + f"\nSUMMARY extracted={extracted} skipped={skipped} failed={len(failed)}\n", encoding="utf-8")
    print(f"SUMMARY extracted={extracted} skipped={skipped} failed={len(failed)} elapsed={time.time() - t0:.0f}s")
    print(f"failed: {failed[:10]}")
    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())
