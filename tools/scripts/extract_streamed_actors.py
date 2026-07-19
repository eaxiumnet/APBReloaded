"""Extract locations from cStreamedBuildingActor AND cStreamedComponentSet in unpacked maps.

Merges into Financial/Waterfront placement manifests with layer tags (building/road/prop).
Also assigns mesh_id from imported district assets preferring block-number match.
"""
from __future__ import annotations

import json
import re
import struct
import subprocess
from pathlib import Path

UMODEL = Path(r"D:\APBReloaded\Tools\UEViewer\umodel_64.exe")
BLOCKS = Path(r"C:\Users\Support\AppData\Local\Temp\grok-goal-9ca60165ac93\implementer\unpacked_blocks")
OUT = Path(r"D:\APBReloaded\Content\Data\district_placements")
IMPORTED = Path(r"D:\APBReloaded\Content\Imported\Districts")
SCRATCH = Path(r"C:\Users\Support\AppData\Local\Temp\grok-goal-9ca60165ac93\implementer")

ACTOR_CLASSES = (
    "cStreamedBuildingActor",
    "cStreamedComponentSet",
    "StaticMeshActor",
)


def umodel_list(path_dir: Path, package: str) -> str:
    r = subprocess.run(
        [str(UMODEL), f"-path={path_dir}", "-game=apb", "-list", package],
        capture_output=True,
        text=True,
        errors="replace",
    )
    return (r.stdout or "") + (r.stderr or "")


def extract_from_file(path: Path) -> list[dict]:
    data = path.read_bytes()
    text = umodel_list(path.parent, path.stem)
    actors = []
    layer = "building"
    name_l = path.name.lower()
    if "road" in name_l:
        layer = "road"
    elif "terrain" in name_l:
        layer = "terrain"
    elif "artprops" in name_l:
        layer = "artprops"
    elif "props" in name_l:
        layer = "props"
    for cls in ACTOR_CLASSES:
        for line in text.splitlines():
            m = re.match(
                rf"\s*\d+\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)\s+{re.escape(cls)}\s+(\S+)",
                line,
            )
            if not m:
                continue
            off = int(m.group(1), 16)
            size = int(m.group(2), 16)
            oname = m.group(3)
            if off + size > len(data):
                continue
            blob = data[off : off + size]
            loc = None
            for i in range(0, max(0, len(blob) - 12), 4):
                x, y, z = struct.unpack_from("<fff", blob, i)
                if max(abs(x), abs(y)) > 10000 and max(abs(x), abs(y), abs(z)) < 500000 and abs(z) < 15000:
                    loc = [x, y, z]
                    break
            if not loc:
                continue
            actors.append(
                {
                    "obj": oname,
                    "class": cls,
                    "location": loc,
                    "package": path.stem,
                    "layer": layer,
                }
            )
    return actors


def filter_actors(actors: list[dict]) -> list[dict]:
    if len(actors) < 5:
        return [
            a
            for a in actors
            if abs(a["location"][0]) > 10000 and abs(a["location"][1]) > 1000
        ]
    xs = sorted(a["location"][0] for a in actors)
    ys = sorted(a["location"][1] for a in actors)
    mx, my = xs[len(xs) // 2], ys[len(ys) // 2]
    out = []
    for a in actors:
        x, y, z = a["location"]
        if abs(x - mx) < 200000 and abs(y - my) < 200000 and abs(x) > 10000 and abs(y) > 1000 and abs(z) < 15000:
            out.append(a)
    return out


def build_district(district_id: str, folder: str, file_globs: list[str], out_name: str):
    files = []
    for g in file_globs:
        files.extend(BLOCKS.glob(g))
    seen = set()
    uniq = []
    for f in sorted(files, key=lambda p: p.name.lower()):
        k = f.name.lower()
        if k in seen:
            continue
        seen.add(k)
        uniq.append(f)
    print(f"=== {district_id}: scanning {len(uniq)} files ===")
    imported = []
    idir = IMPORTED / folder
    if idir.is_dir():
        imported = [p.stem for p in list(idir.glob("*.obj")) + list(idir.glob("*.uasset"))]
    if not imported:
        imported = [f"{folder}_mesh"]

    all_a = []
    for f in uniq:
        a = extract_from_file(f)
        print(f"  {f.name}: {len(a)}")
        all_a.extend(a)
    all_a = filter_actors(all_a)

    placements = []
    for i, a in enumerate(all_a):
        mid = imported[i % len(imported)]
        # Prefer mesh name containing block number
        bn = re.search(r"Block(\d+)", a.get("package", ""), re.I)
        if bn:
            cands = [m for m in imported if bn.group(1) in m or f"B{int(bn.group(1)):02d}" in m or f"_B{bn.group(1)}_" in m]
            if cands:
                mid = cands[i % len(cands)]
        loc = a["location"]
        placements.append(
            {
                "mesh_id": mid,
                "ue_path": f"/Game/Imported/Districts/{folder}/{mid}.{mid}",
                "location": [round(loc[0], 2), round(loc[1], 2), round(loc[2], 2)],
                "rotation": [0.0, float((i * 11) % 360), 0.0],
                "scale": [1.0, 1.0, 1.0],
                "package": a.get("package"),
                "actor": a.get("obj"),
                "actor_class": a.get("class"),
                "layer": a.get("layer"),
                "layout": "cStreamed_multi_class_location",
            }
        )

    if not placements:
        print("NO placements", district_id)
        return

    xs = [p["location"][0] for p in placements]
    ys = [p["location"][1] for p in placements]
    zs = [p["location"][2] for p in placements]
    player = [sum(xs) / len(xs), sum(ys) / len(ys), sum(zs) / len(zs) + 150.0]
    cell = 15000.0
    cells = {}
    for p in placements:
        key = (int(p["location"][0] // cell), int(p["location"][1] // cell))
        cells[key] = cells.get(key, 0) + 1
    packages = sorted({p["package"] for p in placements if p.get("package")})
    layers = {}
    for p in placements:
        layers[p.get("layer", "?")] = layers.get(p.get("layer", "?"), 0) + 1
    man = {
        "district_id": district_id,
        "source_package": packages[0] if packages else "",
        "source_packages": packages,
        "layout": "cStreamed_multi_class_location",
        "layout_note": "Locations from cStreamedBuildingActor + cStreamedComponentSet (roads) + props maps",
        "layer_counts": layers,
        "actor_count": len(placements),
        "stream_chunks": [
            {"id": f"cell_{gx}_{gy}", "origin": [gx * cell, gy * cell], "size": cell, "count": n}
            for (gx, gy), n in sorted(cells.items())
        ],
        "player_start": player,
        "vehicle_start": [player[0] + 600, player[1] - 200, player[2] - 50],
        "placements": placements,
    }
    outp = OUT / out_name
    outp.write_text(json.dumps(man, indent=2), encoding="utf-8")
    print(
        f"WROTE {outp} n={len(placements)} packages={len(packages)} layers={layers} "
        f"x={min(xs):.0f}..{max(xs):.0f} y={min(ys):.0f}..{max(ys):.0f}"
    )


def main():
    build_district(
        "Financial",
        "Financial",
        [
            "FinancialDistrict_Block*.APB",
            "FinancialDistrict_Block*.apb",
            "FinancialDistrict_TILE_*ROADS*.APB",
            "FinancialDistrict_TILE_*TERRAIN*.APB",
            "FinancialDistrict_Props_*.APB",
            "FinancialDistrict_ArtProps_*.APB",
        ],
        "Financial_Block09.json",
    )
    build_district(
        "Waterfront",
        "Waterfront",
        [
            "WaterfrontDistrict_Block*.APB",
            "WaterfrontDistrict_Block*.apb",
            "WaterfrontDistrict_Props_*.APB",
            "WaterfrontDistrict_TILE_*",
        ],
        "Waterfront_Block05.json",
    )
    build_district(
        "Social",
        "Social",
        ["RWorldSocialDistrict_Block*.APB", "RWorldSocialDistrict_Block*.apb"],
        "Social_Block.json",
    )
    build_district(
        "PGBeacon",
        "Beacon",
        ["PGBeaconDistrict_Block*.APB"],
        "Beacon_Block.json",
    )
    build_district(
        "PGCrate",
        "Crate",
        ["PGCrateDistrict_Block*.APB"],
        "Crate_Block.json",
    )
    build_district(
        "PGAsylum",
        "Asylum",
        ["AsylumDistrict_*.APB", "AsylumDistrict_*.apb"],
        "Asylum_Block.json",
    )


if __name__ == "__main__":
    main()
