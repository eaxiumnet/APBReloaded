"""Build mesh-bind reports and *_bound.json from district_placements manifests.

Resolves mesh_id / ue_path against Content/Imported/Districts/*.uasset.
Emits:
  Content/Data/district_placements/{Base}_bound.json  (spawnable only)
  {SCRATCH}/bind_report.json
"""
from __future__ import annotations

import json
from pathlib import Path

PLACEMENTS = Path(r"D:\APBReloaded\Content\Data\district_placements")
IMPORTED = Path(r"D:\APBReloaded\Content\Imported\Districts")
import os
SCRATCH = Path(
    os.environ.get(
        "APB_SCRATCH",
        r"C:\Users\Support\AppData\Local\Temp\grok-goal-4ec7b7726483\implementer",
    )
)

MANIFESTS = [
    "Financial_Block09_realv2.json",
    "Waterfront_Block05.json",
    "Asylum_Block.json",
    "Beacon_Block.json",
    "Crate_Block.json",
    "Social_Block.json",
]


def collect_imported_stems() -> set[str]:
    stems: set[str] = set()
    if not IMPORTED.is_dir():
        return stems
    for p in IMPORTED.rglob("*.uasset"):
        stems.add(p.stem)
    for p in IMPORTED.rglob("*.obj"):
        stems.add(p.stem)
    return stems


def resolve_folder(district_id: str) -> str:
    d = district_id.lower()
    if "water" in d:
        return "Waterfront"
    if "asylum" in d:
        return "Asylum"
    if "beacon" in d:
        return "Beacon"
    if "crate" in d:
        return "Crate"
    if "social" in d:
        return "Social"
    return "Financial"


def bind_one(path: Path, imported: set[str]) -> dict:
    data = json.loads(path.read_text(encoding="utf-8"))
    placements = data.get("placements") or []
    total = data.get("source_visible_placement_count") or len(placements)
    bound = []
    missing_ids: set[str] = set()
    for pl in placements:
        mid = pl.get("mesh_id") or ""
        ue = pl.get("ue_path") or ""
        # stem from ue_path last segment
        stem = mid
        if ue:
            last = ue.rstrip("/").split("/")[-1]
            if "." in last:
                stem = last.split(".")[0]
            else:
                stem = last
        hit = mid in imported or stem in imported
        if hit:
            # normalize ue_path to imported asset
            folder = resolve_folder(data.get("district_id") or "")
            # prefer folder that actually has the asset
            for f in (folder, "Financial", "Waterfront", "Asylum", "Beacon", "Crate", "Social"):
                if (IMPORTED / f / f"{mid}.uasset").exists() or (IMPORTED / f / f"{mid}.obj").exists():
                    folder = f
                    break
            pl2 = dict(pl)
            pl2["mesh_id"] = mid if mid in imported else stem
            pl2["ue_path"] = f"/Game/Imported/Districts/{folder}/{pl2['mesh_id']}.{pl2['mesh_id']}"
            pl2["bound"] = True
            bound.append(pl2)
        else:
            if mid:
                missing_ids.add(mid)
    bcount = len(bound)
    rate = (bcount / total) if total else 0.0
    out = dict(data)
    out["placements"] = bound
    out["bound_count"] = bcount
    out["manifest_total"] = total
    out["hit_rate"] = round(rate, 4)
    out["layout_note"] = (
        (data.get("layout_note") or "")
        + f" | bind: {bcount}/{total} hit_rate={rate:.3f}"
    )
    bound_path = path.with_name(path.stem + "_bound.json")
    bound_path.write_text(json.dumps(out, indent=2), encoding="utf-8")
    return {
        "file": path.name,
        "bound_file": bound_path.name,
        "district_id": data.get("district_id"),
        "total": total,
        "bound": bcount,
        "hit_rate": round(rate, 4),
        "missing_mesh_ids": sorted(missing_ids)[:50],
        "missing_count": len(missing_ids),
    }


def main():
    SCRATCH.mkdir(parents=True, exist_ok=True)
    imported = collect_imported_stems()
    report = {"imported_stems": len(imported), "districts": []}
    for name in MANIFESTS:
        p = PLACEMENTS / name
        if not p.exists():
            report["districts"].append({"file": name, "error": "missing"})
            continue
        row = bind_one(p, imported)
        report["districts"].append(row)
        print(
            f"{name}: bound={row['bound']}/{row['total']} hit_rate={row['hit_rate']} missing_ids={row['missing_count']}"
        )
    # hero threshold
    fin = next((d for d in report["districts"] if d.get("file") == "Financial_Block09_realv2.json"), None)
    report["financial_hit_rate"] = fin["hit_rate"] if fin else 0.0
    report["financial_pass"] = bool(fin and fin["hit_rate"] >= 0.9)
    out = SCRATCH / "bind_report.json"
    out.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("wrote", out)
    print("financial_pass", report["financial_pass"], "rate", report["financial_hit_rate"])
    return 0 if report["financial_pass"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
