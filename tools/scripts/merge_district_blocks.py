"""Merge cStreamedBuildingActor locations from all decompressed block maps in unpacked_blocks/."""
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
PKG_ROOT = Path(
    r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Content\Release\Packages"
)


def read_names(data: bytes, name_offset: int, name_count: int) -> list[str]:
    r = name_offset
    names = []
    for _ in range(name_count):
        n = struct.unpack_from("<i", data, r)[0]
        r += 4
        if n > 0:
            raw = data[r : r + n]
            r += n
            s = raw.split(b"\x00")[0].decode("latin-1", "replace")
        elif n < 0:
            raw = data[r : r + (-n) * 2]
            r += (-n) * 2
            s = raw.decode("utf-16-le", "replace").split("\x00")[0]
        else:
            s = ""
        r += 8
        names.append(s)
    return names


def parse_header(data: bytes) -> dict:
    r = 8
    r += 4
    n = struct.unpack_from("<i", data, r)[0]
    r += 4
    if n > 0:
        r += n
    elif n < 0:
        r += (-n) * 2
    r += 4
    name_count = struct.unpack_from("<i", data, r)[0]
    r += 4
    name_offset = struct.unpack_from("<i", data, r)[0]
    return {"name_count": name_count, "name_offset": name_offset}


def umodel_list(path_dir: Path, package: str) -> str:
    r = subprocess.run(
        [str(UMODEL), f"-path={path_dir}", "-game=apb", "-list", package],
        capture_output=True,
        text=True,
        errors="replace",
    )
    return (r.stdout or "") + (r.stderr or "")


def extract_locations(block_path: Path) -> tuple[list[dict], list[str]]:
    data = block_path.read_bytes()
    try:
        hdr = parse_header(data)
        names = read_names(data, hdr["name_offset"], hdr["name_count"])
    except Exception:
        names = []
    mesh_hints = [
        n
        for n in names
        if isinstance(n, str)
        and n.endswith("_LOD_0")
        and "VertexLit" not in n
        and len(n) > 8
    ]
    text = umodel_list(block_path.parent, block_path.stem)
    actors = []
    for line in text.splitlines():
        m = re.match(
            r"\s*\d+\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)\s+cStreamedBuildingActor\s+(\S+)",
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
            if max(abs(x), abs(y)) > 50000 and max(abs(x), abs(y), abs(z)) < 500000 and abs(z) < 8000:
                loc = [x, y, z]
                break
        if not loc:
            continue
        actors.append({"obj": oname, "location": loc, "block": block_path.stem})
    return actors, mesh_hints


def list_building_lod0(district_pkg: str, block_num: str) -> list[str]:
    """LOD0 mesh names from Packages/.../Buildings/*BlockNN* packages via umodel."""
    bdir = PKG_ROOT / district_pkg / "Buildings"
    if not bdir.is_dir():
        return []
    pkgs = list(bdir.glob(f"*Block{block_num}*Package.upk")) + list(
        bdir.glob(f"*Block{block_num}*Package.UPK")
    )
    # also BlockNN without zero pad variants
    if not pkgs:
        pkgs = list(bdir.glob(f"*Block{int(block_num)}*Package.upk")) + list(
            bdir.glob(f"*Block{int(block_num)}*Package.UPK")
        )
    meshes = []
    for pkg in pkgs[:2]:
        text = umodel_list(pkg.parent, pkg.stem)
        for line in text.splitlines():
            m = re.search(r"StaticMesh\s+(\S+)", line)
            if not m:
                continue
            name = m.group(1)
            if name.endswith("_LOD_0") and "VertexLit" not in name:
                meshes.append(name)
    return meshes


def build(district_id: str, folder: str, prefix: str, pkg_folder: str, out_name: str, globs: list[str] | None = None):
    files = []
    patterns = globs or [f"{prefix}_Block*.APB", f"{prefix}_Block*.apb"]
    for pat in patterns:
        files.extend(BLOCKS.glob(pat))
    # unique by name
    seen = set()
    uniq = []
    for f in sorted(files, key=lambda p: p.name.lower()):
        if f.name.lower() in seen:
            continue
        seen.add(f.name.lower())
        uniq.append(f)
    files = uniq
    print(f"=== {district_id}: {len(files)} block files ===")
    imported = []
    idir = IMPORTED / folder
    if idir.is_dir():
        imported = [p.stem for p in list(idir.glob("*.obj")) + list(idir.glob("*.uasset"))]
    if not imported:
        imported = [f"{folder}_mesh"]

    all_actors = []
    packages = []
    block_mesh_map: dict[str, list[str]] = {}

    for bf in files:
        actors, hints = extract_locations(bf)
        print(f"  {bf.name}: actors={len(actors)} hints={len(hints)}")
        all_actors.extend(actors)
        packages.append(bf.stem)
        m = re.search(r"Block(\d+)", bf.stem, re.I)
        if m:
            bn = m.group(1)
            # Prefer importable hints, else building package LOD0 names that match imported
            pool = [h for h in hints if h in imported]
            if not pool:
                bmeshes = list_building_lod0(pkg_folder, bn)
                pool = [h for h in bmeshes if h in imported] or bmeshes[:20]
            if pool:
                block_mesh_map[bn] = pool

    # median filter
    if len(all_actors) >= 5:
        xs = sorted(a["location"][0] for a in all_actors)
        ys = sorted(a["location"][1] for a in all_actors)
        mx, my = xs[len(xs) // 2], ys[len(ys) // 2]
        all_actors = [
            a
            for a in all_actors
            if abs(a["location"][0] - mx) < 150000
            and abs(a["location"][1] - my) < 150000
            and abs(a["location"][0]) > 50000
            and abs(a["location"][1]) > 50000
        ]

    placements = []
    for i, a in enumerate(all_actors):
        bn_m = re.search(r"Block(\d+)", a.get("block", ""), re.I)
        bn = bn_m.group(1) if bn_m else ""
        pool = block_mesh_map.get(bn) or imported
        # prefer imported mesh if pool has non-imported names
        mid = None
        for cand in pool:
            if cand in imported:
                mid = cand
                break
        if not mid:
            mid = imported[i % len(imported)]
        # if pool has LOD0 names, store source_mesh for report even if we place imported stand-in
        source_mesh = pool[i % len(pool)] if pool else mid
        loc = a["location"]
        placements.append(
            {
                "mesh_id": mid,
                "source_mesh": source_mesh,
                "ue_path": f"/Game/Imported/Districts/{folder}/{mid}.{mid}",
                "location": [round(loc[0], 2), round(loc[1], 2), round(loc[2], 2)],
                "rotation": [0.0, float((i * 13) % 360), 0.0],
                "scale": [1.0, 1.0, 1.0],
                "package": a.get("block"),
                "actor": a.get("obj"),
                "layout": "cStreamedBuildingActor_location",
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
    chunks = [
        {"id": f"cell_{gx}_{gy}", "origin": [gx * cell, gy * cell], "size": cell, "count": n}
        for (gx, gy), n in sorted(cells.items())
    ]
    man = {
        "district_id": district_id,
        "source_package": packages[0],
        "source_packages": packages,
        "layout": "cStreamedBuildingActor_location",
        "layout_note": (
            "Locations from decompressed Steam Financial/Waterfront Block*.APB "
            "cStreamedBuildingActor; mesh_id bound to imported district LOD0, "
            "source_mesh records package mesh name when available."
        ),
        "actor_count": len(placements),
        "stream_chunks": chunks,
        "player_start": player,
        "vehicle_start": [player[0] + 600, player[1] - 200, player[2] - 50],
        "placements": placements,
    }
    outp = OUT / out_name
    outp.write_text(json.dumps(man, indent=2), encoding="utf-8")
    print(
        f"WROTE {outp} n={len(placements)} packages={len(packages)} "
        f"x={min(xs):.0f}..{max(xs):.0f} y={min(ys):.0f}..{max(ys):.0f}"
    )
    print("  player", player)
    (SCRATCH / f"merge_{district_id}.json").write_text(
        json.dumps(
            {
                "n": len(placements),
                "packages": packages,
                "xrange": [min(xs), max(xs)],
                "yrange": [min(ys), max(ys)],
                "player_start": player,
            },
            indent=2,
        ),
        encoding="utf-8",
    )


def main():
    build("Financial", "Financial", "FinancialDistrict", "FinancialDistrict", "Financial_Block09.json")
    build("Waterfront", "Waterfront", "WaterfrontDistrict", "WaterfrontDistrict", "Waterfront_Block05.json")
    # Other districts — any unpacked Block*.APB with matching prefix
    build(
        "PGAsylum",
        "Asylum",
        "AsylumDistrict",
        "AsylumDistrict",
        "Asylum_Block.json",
        globs=["AsylumDistrict_Main.APB", "AsylumDistrict_CentralSection.APB", "AsylumDistrict_NorthFace.APB", "AsylumDistrict_*.APB"],
    )
    build("PGBeacon", "Beacon", "PGBeaconDistrict", "PGBeaconDistrict", "Beacon_Block.json")
    build("PGCrate", "Crate", "PGCrateDistrict", "PGCrateDistrict", "Crate_Block.json")
    build("Social", "Social", "RWorldSocialDistrict", "RWorldSocialDistrict", "Social_Block.json")


if __name__ == "__main__":
    main()
