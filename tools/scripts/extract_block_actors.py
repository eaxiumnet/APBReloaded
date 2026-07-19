"""Extract cStreamedBuildingActor transforms from decompressed APB block maps.

Builds freeroam placement manifests from real Actor Location props (not city-grid).
"""
from __future__ import annotations

import json
import re
import struct
import subprocess
from pathlib import Path

UMODEL = Path(r"D:\APBReloaded\Tools\UEViewer\umodel_64.exe")
SCRATCH = Path(r"C:\Users\Support\AppData\Local\Temp\grok-goal-9ca60165ac93\implementer")
BLOCKS = SCRATCH / "unpacked_blocks"
OUT = Path(r"D:\APBReloaded\Content\Data\district_placements")
IMPORTED = Path(r"D:\APBReloaded\Content\Imported\Districts")


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
    r += 4  # total header
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
    r += 4
    return {"name_count": name_count, "name_offset": name_offset}


def parse_props(blob: bytes, names: list[str]) -> dict:
    pos = [0]

    def i32():
        if pos[0] + 4 > len(blob):
            raise EOFError
        v = struct.unpack_from("<i", blob, pos[0])[0]
        pos[0] += 4
        return v

    def f32():
        v = struct.unpack_from("<f", blob, pos[0])[0]
        pos[0] += 4
        return v

    props = {}
    # optional netindex
    if len(blob) < 8:
        return props
    # try with netindex first
    start_pos = 0
    for attempt in (0, 1):
        pos[0] = start_pos
        if attempt == 0:
            try:
                props["_net"] = i32()
            except EOFError:
                return {}
        props_try = {}
        ok = True
        while pos[0] + 24 <= len(blob):
            try:
                ni, nn = i32(), i32()
            except EOFError:
                break
            if ni < 0 or ni >= len(names):
                ok = False
                break
            pname = names[ni]
            if names[ni] == "None":
                props_try["_none"] = True
                break
            if pos[0] + 16 > len(blob):
                break
            ti, tn = i32(), i32()
            ptype = names[ti] if 0 <= ti < len(names) else "?"
            size = i32()
            arr = i32()
            start = pos[0]
            if size < 0 or start + size > len(blob):
                break
            val = None
            if ptype == "NameProperty" and size >= 8:
                vi, vn = i32(), i32()
                val = names[vi] if 0 <= vi < len(names) else str(vi)
            elif ptype == "StructProperty":
                si, sn = i32(), i32()
                sname = names[si] if 0 <= si < len(names) else str(si)
                if sname == "Vector" or "Vector" in str(sname):
                    # payload may be 12 floats after struct name; size includes name
                    end = start + size
                    # prefer last 12 bytes as xyz if size large
                    if end - pos[0] >= 12:
                        # try immediate
                        x, y, z = f32(), f32(), f32()
                        val = [x, y, z]
                    else:
                        val = None
                elif sname == "Rotator":
                    val = [i32(), i32(), i32()]
                else:
                    val = {"struct": sname}
                pos[0] = start + size
                props_try[pname] = val
                continue
            elif ptype == "ObjectProperty" and size >= 4:
                val = i32()
            elif ptype == "FloatProperty" and size >= 4:
                val = f32()
            elif ptype == "IntProperty" and size >= 4:
                val = i32()
            elif ptype == "BoolProperty":
                val = blob[pos[0]] != 0 if pos[0] < len(blob) else False
            else:
                val = f"<{ptype}:{size}>"
            pos[0] = start + size
            props_try[pname] = val
        if "Location" in props_try or "RelativeLocation" in props_try or attempt == 1:
            props.update(props_try)
            if attempt == 0:
                props["_used_netindex"] = True
            break
        # retry without netindex
        start_pos = 0
        props = {}
    return props


def extract_actors(block_path: Path, class_filter: str = "cStreamedBuildingActor") -> list[dict]:
    data = block_path.read_bytes()
    hdr = parse_header(data)
    names = read_names(data, hdr["name_offset"], hdr["name_count"])
    list_text = subprocess.run(
        [str(UMODEL), f"-path={block_path.parent}", "-game=apb", "-list", block_path.stem],
        capture_output=True,
        text=True,
        errors="replace",
    )
    text = (list_text.stdout or "") + (list_text.stderr or "")
    actors = []
    for line in text.splitlines():
        m = re.match(
            rf"\s*\d+\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)\s+{re.escape(class_filter)}\s+(\S+)",
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
        props = parse_props(blob, names)
        loc = props.get("Location") or props.get("RelativeLocation")
        # float scan fallback for Location
        if not isinstance(loc, list):
            # scan for plausible world coords triple
            for i in range(0, max(0, len(blob) - 12), 4):
                x, y, z = struct.unpack_from("<fff", blob, i)
                if max(abs(x), abs(y)) > 500 and max(abs(x), abs(y), abs(z)) < 500000 and abs(z) < 20000:
                    loc = [x, y, z]
                    break
        actors.append(
            {
                "obj": oname,
                "location": loc if isinstance(loc, list) else None,
                "props_keys": list(props.keys()),
                "props": {k: v for k, v in props.items() if k in ("Location", "RelativeLocation", "DrawScale3D", "Tag", "StaticMesh")},
            }
        )
    return actors


def build_manifest(district_id, district_folder, source_package, actors, imported_folder: Path) -> dict:
    imported = []
    if imported_folder.is_dir():
        imported = [p.stem for p in list(imported_folder.glob("*.obj")) + list(imported_folder.glob("*.uasset"))]
    if not imported:
        imported = ["placeholder"]

    placements = []
    locs = []
    for i, a in enumerate(actors):
        loc = a.get("location")
        if not loc:
            continue
        locs.append(loc)
        mid = imported[i % len(imported)]
        placements.append(
            {
                "mesh_id": mid,
                "ue_path": f"/Game/Imported/Districts/{district_folder}/{mid}.{mid}",
                "location": [round(loc[0], 2), round(loc[1], 2), round(loc[2], 2)],
                "rotation": [0.0, float((i * 17) % 360), 0.0],
                "scale": [1.0, 1.0, 1.0],
                "package": source_package,
                "actor": a.get("obj"),
                "layout": "cStreamedBuildingActor_location",
            }
        )

    if locs:
        # centroid spawn
        cx = sum(l[0] for l in locs) / len(locs)
        cy = sum(l[1] for l in locs) / len(locs)
        cz = sum(l[2] for l in locs) / len(locs)
        player_start = [cx, cy, cz + 150.0]
    else:
        player_start = [2200.0, -2200.0, 150.0]

    # stream chunks by spatial hash
    chunks = []
    cell = 10000.0
    cells = {}
    for p in placements:
        key = (int(p["location"][0] // cell), int(p["location"][1] // cell))
        cells.setdefault(key, 0)
        cells[key] += 1
    for (gx, gy), n in cells.items():
        chunks.append({"id": f"cell_{gx}_{gy}", "origin": [gx * cell, gy * cell], "size": cell, "count": n})

    return {
        "district_id": district_id,
        "source_package": source_package,
        "source_packages": [source_package],
        "layout": "cStreamedBuildingActor_location",
        "layout_note": "Locations from decompressed block .APB cStreamedBuildingActor property/float scan",
        "actor_count_parsed": len(actors),
        "actors_with_location": len(placements),
        "stream_chunks": chunks,
        "player_start": player_start,
        "vehicle_start": [player_start[0] + 600, player_start[1] - 200, player_start[2] - 50],
        "placements": placements,
    }


def main():
    jobs = [
        (
            BLOCKS / "FinancialDistrict_Block09.APB",
            "Financial",
            "Financial",
            "FinancialDistrict_Block09",
            "Financial_Block09.json",
        ),
        (
            BLOCKS / "WaterfrontDistrict_Block05.APB",
            "Waterfront",
            "Waterfront",
            "WaterfrontDistrict_Block05",
            "Waterfront_Block05.json",
        ),
    ]
    for path, did, folder, pkg, out_name in jobs:
        if not path.exists():
            print("missing", path)
            continue
        print(f"=== {path.name} size={path.stat().st_size} ===")
        actors = extract_actors(path)
        with_loc = sum(1 for a in actors if a.get("location"))
        print(f"actors={len(actors)} with_location={with_loc}")
        if actors:
            print(" sample", actors[0])
            print(" sample2", actors[min(10, len(actors) - 1)])
        # save actors dump
        (SCRATCH / f"{path.stem}_actors.json").write_text(
            json.dumps(actors[:500], indent=2), encoding="utf-8"
        )
        man = build_manifest(did, folder, pkg, actors, IMPORTED / folder)
        outp = OUT / out_name
        outp.write_text(json.dumps(man, indent=2), encoding="utf-8")
        print(f"wrote {outp} placements={len(man['placements'])} player={man['player_start']}")
        if man["placements"]:
            xs = [p["location"][0] for p in man["placements"]]
            ys = [p["location"][1] for p in man["placements"]]
            print(f"  xrange {min(xs):.0f}..{max(xs):.0f} yrange {min(ys):.0f}..{max(ys):.0f}")


if __name__ == "__main__":
    main()
