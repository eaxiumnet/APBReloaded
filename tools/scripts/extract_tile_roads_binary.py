"""Binary float-scan TILE ROADS/TERRAIN APB maps for world locations.

umodel -list returns 0 cStreamed actors on FinancialDistrict_TILE_*ROADS*.APB.
This scans decompressed (or cooked) map bytes for plausible San Paro XYZ triples
and emits a road-layer placement overlay merged into Financial_Block09.json.
"""
from __future__ import annotations

import json
import re
import struct
from collections import Counter
from pathlib import Path

MAPS = Path(
    r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Content\Release\Maps\FinancialDistrict"
)
UNPACKED = Path(
    r"C:\Users\Support\AppData\Local\Temp\grok-goal-9ca60165ac93\implementer\unpacked_blocks"
)
OUT = Path(r"D:\APBReloaded\Content\Data\district_placements")
IMPORTED = Path(r"D:\APBReloaded\Content\Imported\Districts\Financial")
SCRATCH = Path(r"C:\Users\Support\AppData\Local\Temp\grok-goal-9ca60165ac93\implementer")

# San Paro Financial-ish AABB from existing building placements
X_MIN, X_MAX = 60000.0, 200000.0
Y_MIN, Y_MAX = 10000.0, 200000.0
Z_MIN, Z_MAX = -500.0, 5000.0


def scan_file(path: Path, max_hits: int = 400) -> list[dict]:
    data = path.read_bytes()
    hits = []
    # 4-byte aligned float triples
    n = len(data) - 12
    step = 4
    seen = set()
    for i in range(0, n, step):
        x, y, z = struct.unpack_from("<fff", data, i)
        if not (X_MIN <= x <= X_MAX and Y_MIN <= y <= Y_MAX and Z_MIN <= z <= Z_MAX):
            continue
        # reject NaN/inf
        if any(v != v or abs(v) > 1e8 for v in (x, y, z)):
            continue
        # quantize to reduce duplicates
        key = (round(x / 50) * 50, round(y / 50) * 50, round(z / 25) * 25)
        if key in seen:
            continue
        seen.add(key)
        layer = "road" if "ROAD" in path.name.upper() else "terrain"
        hits.append(
            {
                "obj": f"{path.stem}_{len(hits)}",
                "class": "TileBinaryFloatScan",
                "location": [float(x), float(y), float(z)],
                "package": path.stem,
                "layer": layer,
            }
        )
        if len(hits) >= max_hits:
            break
    return hits


def collect_road_mesh_ids() -> list[str]:
    stems = []
    for p in IMPORTED.glob("*.uasset"):
        s = p.stem.lower()
        if "road" in s or "lane" in s or "asphalt" in s or s.startswith("mt"):
            stems.append(p.stem)
    # prefer real road surfaces if present
    preferred = [s for s in stems if "road" in s.lower() or "lane" in s.lower()]
    return preferred or stems or ["FinancialDistrict_Block09_Generic_0001_LOD_0"]


def main() -> None:
    files: list[Path] = []
    for root in (UNPACKED, MAPS):
        if not root.is_dir():
            continue
        files.extend(root.glob("*ROADS*.APB"))
        files.extend(root.glob("*ROADS*.apb"))
        files.extend(root.glob("*TERRAIN*.APB"))
    # unique by name
    by_name = {f.name: f for f in files}
    files = sorted(by_name.values(), key=lambda p: p.name)
    print(f"scanning {len(files)} TILE road/terrain files")

    all_hits: list[dict] = []
    per_file = Counter()
    for f in files:
        h = scan_file(f, max_hits=80 if "ROAD" in f.name.upper() else 20)
        per_file[f.name] = len(h)
        all_hits.extend(h)
        print(f"  {f.name}: {len(h)}")

    print(f"total raw hits={len(all_hits)}")
    # densify-filter: keep roads preferred
    roads = [h for h in all_hits if h["layer"] == "road"]
    terrain = [h for h in all_hits if h["layer"] == "terrain"]
    # cap totals
    roads = roads[:800]
    terrain = terrain[:200]
    selected = roads + terrain
    print(f"selected roads={len(roads)} terrain={len(terrain)}")

    mesh_pool = collect_road_mesh_ids()
    print(f"mesh_pool={len(mesh_pool)} sample={mesh_pool[:5]}")

    # Load existing Financial placements
    fin_path = OUT / "Financial_Block09.json"
    man = json.loads(fin_path.read_text(encoding="utf-8"))
    placements = list(man.get("placements") or [])
    # drop previous binary tile entries
    placements = [p for p in placements if p.get("layout") != "tile_binary_float_scan"]

    for i, h in enumerate(selected):
        mid = mesh_pool[i % len(mesh_pool)]
        loc = h["location"]
        placements.append(
            {
                "mesh_id": mid,
                "ue_path": f"/Game/Imported/Districts/Financial/{mid}.{mid}",
                "location": loc,
                "rotation": [0.0, 0.0, 0.0],
                "scale": [1.0, 1.0, 1.0],
                "layer": h["layer"],
                "package": h["package"],
                "layout": "tile_binary_float_scan",
                "layout_note": "XYZ from float-scan of TILE ROADS/TERRAIN APB (umodel list empty)",
            }
        )

    man["placements"] = placements
    man["layout_note"] = (
        (man.get("layout_note") or "")
        + f" | tile_binary roads={len(roads)} terrain={len(terrain)} pool={len(mesh_pool)}"
    )
    fin_path.write_text(json.dumps(man, indent=2), encoding="utf-8")
    print(f"WROTE {fin_path} total_placements={len(placements)}")

    report = {
        "files_scanned": len(files),
        "per_file": dict(per_file.most_common(40)),
        "roads": len(roads),
        "terrain": len(terrain),
        "mesh_pool": len(mesh_pool),
        "total_placements": len(placements),
    }
    (SCRATCH / "tile_road_extract.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
