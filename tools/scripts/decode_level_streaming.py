"""Decode LevelStreamingKismet PackageName + Location from decompressed APB MASTER maps."""
from __future__ import annotations

import json
import re
import struct
import subprocess
from pathlib import Path

UMODEL = Path(r"D:\APBReloaded\Tools\UEViewer\umodel_64.exe")
SCRATCH = Path(r"C:\Users\Support\AppData\Local\Temp\grok-goal-9ca60165ac93\implementer")
UNPACKED = SCRATCH / "unpacked_maps"
OUT = Path(r"D:\APBReloaded\Content\Data\district_placements")
IMPORTED = Path(r"D:\APBReloaded\Content\Imported\Districts")
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
        r += 8  # APB 564 name flags (8 bytes)
        names.append(s)
    return names


def parse_header(data: bytes) -> dict:
    # Tag + versions
    file_ver = struct.unpack_from("<H", data, 4)[0]
    lic_ver = struct.unpack_from("<H", data, 6)[0]
    r = 8
    total_header = struct.unpack_from("<i", data, r)[0]
    r += 4
    n = struct.unpack_from("<i", data, r)[0]
    r += 4
    if n > 0:
        r += n
    elif n < 0:
        r += (-n) * 2
    r += 4  # flags
    name_count = struct.unpack_from("<i", data, r)[0]
    r += 4
    name_offset = struct.unpack_from("<i", data, r)[0]
    r += 4
    export_count = struct.unpack_from("<i", data, r)[0]
    r += 4
    export_offset = struct.unpack_from("<i", data, r)[0]
    r += 4
    return {
        "file_ver": file_ver,
        "lic_ver": lic_ver,
        "name_count": name_count,
        "name_offset": name_offset,
        "export_count": export_count,
        "export_offset": export_offset,
    }


def umodel_list(path_dir: Path, package: str) -> str:
    r = subprocess.run(
        [str(UMODEL), f"-path={path_dir}", "-game=apb", "-list", package],
        capture_output=True,
        text=True,
        errors="replace",
    )
    return (r.stdout or "") + (r.stderr or "")


def parse_props(blob: bytes, names: list[str]) -> dict:
    pos = [0]

    def i32():
        v = struct.unpack_from("<i", blob, pos[0])[0]
        pos[0] += 4
        return v

    def f32():
        v = struct.unpack_from("<f", blob, pos[0])[0]
        pos[0] += 4
        return v

    if len(blob) < 4:
        return {}
    net = i32()
    props = {"_netindex": net}
    while pos[0] + 24 <= len(blob):
        ni = i32()
        nn = i32()
        if ni < 0 or ni >= len(names):
            break
        pname = names[ni]
        if nn:
            pname = f"{pname}_{nn}"
        if pname == "None" or names[ni] == "None":
            props["_none"] = True
            break
        if pos[0] + 16 > len(blob):
            break
        ti = i32()
        tn = i32()
        ptype = names[ti] if 0 <= ti < len(names) else str(ti)
        size = i32()
        arr = i32()
        start = pos[0]
        if size < 0 or start + size > len(blob):
            break
        val = None
        if ptype == "NameProperty":
            vi, vn = i32(), i32()
            val = names[vi] if 0 <= vi < len(names) else str(vi)
            if vn:
                val = f"{val}_{vn}"
        elif ptype == "StructProperty":
            si, sn = i32(), i32()
            sname = names[si] if 0 <= si < len(names) else str(si)
            # Data size is `size` bytes after ArrayIndex; StructName is part of stream before data
            # In UE3 Size is the size of the data following StructName
            data_start = pos[0]
            # Some builds include StructName inside size — try both heuristics
            remaining = size
            # We already read StructName (8 bytes). If size >= 12 and looks like vector:
            if remaining >= 12 and (sname == "Vector" or "Vector" in str(sname)):
                val = [f32(), f32(), f32()]
            elif remaining >= 12 and sname == "Rotator":
                val = [i32(), i32(), i32()]
            else:
                # try reading vector anyway if 12 bytes of floats look reasonable
                if remaining >= 12:
                    trial = struct.unpack_from("<fff", blob, data_start)
                    if all(abs(x) < 1e6 for x in trial) and max(abs(x) for x in trial) > 1:
                        val = list(trial)
                        pos[0] = data_start + 12
                    else:
                        val = {"struct": sname, "raw": blob[data_start : data_start + max(0, remaining - 8)].hex()}
                        pos[0] = data_start + max(0, remaining - 8)
                else:
                    val = {"struct": sname}
            # Align to start+size for struct where size counts from after ArrayIndex including StructName
            # UE3: Size = size of data after ArrayIndex, including StructName FName (8) + payload
            # So end = start + size, and StructName was first 8 of that
            # We read StructName already from start, so payload ends at start+size
            pos[0] = start + size
            props[pname] = val
            continue
        elif ptype == "BoolProperty":
            val = blob[pos[0]] != 0 if pos[0] < len(blob) else False
        elif ptype == "IntProperty":
            val = i32()
        elif ptype == "FloatProperty":
            val = f32()
        elif ptype == "ObjectProperty":
            val = i32()
        elif ptype == "StrProperty":
            n = i32()
            if n > 0:
                raw = blob[pos[0] : pos[0] + n]
                pos[0] += n
                val = raw.split(b"\x00")[0].decode("latin-1", "replace")
            elif n < 0:
                raw = blob[pos[0] : pos[0] + (-n) * 2]
                pos[0] += (-n) * 2
                val = raw.decode("utf-16-le", "replace").split("\x00")[0]
            else:
                val = ""
        else:
            val = blob[start : start + size].hex()
        pos[0] = start + max(size, 0)
        props[pname] = val
    return props


def decode_master(master_path: Path) -> list[dict]:
    data = master_path.read_bytes()
    hdr = parse_header(data)
    names = read_names(data, hdr["name_offset"], hdr["name_count"])
    list_text = umodel_list(master_path.parent, master_path.stem)
    rows = []
    for line in list_text.splitlines():
        m = re.match(
            r"\s*\d+\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)\s+LevelStreamingKismet\s+(\S+)",
            line,
        )
        if not m:
            continue
        off = int(m.group(1), 16)
        size = int(m.group(2), 16)
        oname = m.group(3)
        if off + size > len(data):
            rows.append({"obj": oname, "error": "oob"})
            continue
        blob = data[off : off + size]
        props = parse_props(blob, names)
        rows.append(
            {
                "obj": oname,
                "package_name": props.get("PackageName"),
                "location": props.get("Location"),
                "props": {k: v for k, v in props.items() if not str(k).startswith("_")},
            }
        )
    return rows


def find_block_meshes(package_name: str, district_key: str) -> list[str]:
    """Find LOD0 mesh names via umodel list on matching building package."""
    if not package_name or not isinstance(package_name, str):
        return []
    # Search packages tree for matching upk stem containing package_name
    candidates = list(PKG_ROOT.rglob(f"*{package_name}*.upk")) + list(
        PKG_ROOT.rglob(f"*{package_name}*.UPK")
    )
    if not candidates:
        # try loose match BlockNN
        m = re.search(r"Block(\d+)", package_name, re.I)
        if m and district_key:
            candidates = list((PKG_ROOT / district_key).rglob(f"*Block{m.group(1)}*_Package.upk"))
            candidates += list((PKG_ROOT / district_key).rglob(f"*Block{m.group(1)}*_Package.UPK"))
    if not candidates:
        return []
    pkg = candidates[0]
    text = umodel_list(pkg.parent, pkg.stem)
    meshes = []
    for line in text.splitlines():
        m = re.search(r"StaticMesh\s+(\S+)", line)
        if not m:
            continue
        name = m.group(1)
        if name.endswith("_LOD_0") and "VertexLit" not in name:
            meshes.append(name)
    return meshes[:12]


def build_manifest_from_streaming(
    district_id: str,
    district_folder: str,
    package_folder: str,
    rows: list[dict],
    imported_folder: Path,
) -> dict:
    imported = []
    if imported_folder.is_dir():
        imported = [p.stem for p in list(imported_folder.glob("*.obj")) + list(imported_folder.glob("*.uasset"))]

    placements = []
    chunks = []
    source_packages = []
    used_pkg = set()

    for r in rows:
        pkg = r.get("package_name")
        loc = r.get("location")
        if not pkg or not isinstance(pkg, str):
            continue
        if not loc or not isinstance(loc, list) or len(loc) < 3:
            continue
        # Filter junk package names
        if pkg in ("None", "Package", "PackageName"):
            continue
        if pkg not in used_pkg:
            used_pkg.add(pkg)
            source_packages.append(pkg)
            chunks.append(
                {
                    "id": pkg,
                    "origin": [loc[0], loc[1]],
                    "size": 15000,
                    "package": pkg,
                    "location": loc,
                }
            )
        meshes = find_block_meshes(pkg, package_folder)
        if not meshes and imported:
            meshes = imported[:6]
        for mi, mesh in enumerate(meshes[:8]):
            mid = mesh if (not imported or mesh in imported) else imported[mi % len(imported)]
            # Local offset within block so multiple meshes not stacked
            ox = (mi % 3) * 1500.0
            oy = (mi // 3) * 1500.0
            placements.append(
                {
                    "mesh_id": mid,
                    "ue_path": f"/Game/Imported/Districts/{district_folder}/{mid}.{mid}",
                    "location": [round(loc[0] + ox, 1), round(loc[1] + oy, 1), round(loc[2], 1)],
                    "rotation": [0.0, float(mi * 15), 0.0],
                    "scale": [1.0, 1.0, 1.0],
                    "package": pkg,
                    "stream_obj": r.get("obj"),
                    "layout": "master_levelstreaming_location",
                }
            )

    # PlayerStart near first chunk with valid location
    if chunks:
        c0 = chunks[0]["location"]
        player_start = [c0[0] + 2000.0, c0[1] + 2000.0, c0[2] + 150.0]
    else:
        player_start = [2200.0, -2200.0, 150.0]

    return {
        "district_id": district_id,
        "source_package": source_packages[0] if source_packages else "",
        "source_packages": source_packages,
        "layout": "master_levelstreaming_location",
        "layout_note": "PackageName+Location decoded from decompressed MASTER.APB LevelStreamingKismet exports",
        "stream_chunks": chunks,
        "player_start": player_start,
        "vehicle_start": [player_start[0] + 600, player_start[1] - 200, player_start[2] - 50],
        "placements": placements,
    }


def main():
    # Decode Financial + Waterfront
    for master_name, district_id, folder, pkg_folder, out_name in [
        ("FinancialDistrict_MASTER.APB", "Financial", "Financial", "FinancialDistrict", "Financial_Block09.json"),
        ("WaterfrontDistrict_MASTER.APB", "Waterfront", "Waterfront", "WaterfrontDistrict", "Waterfront_Block05.json"),
    ]:
        master = UNPACKED / master_name
        if not master.exists():
            print("missing", master)
            continue
        rows = decode_master(master)
        with_pkg = sum(1 for r in rows if r.get("package_name"))
        with_loc = sum(1 for r in rows if isinstance(r.get("location"), list))
        print(f"{master_name}: streaming={len(rows)} package_name={with_pkg} location={with_loc}")
        # show samples
        for r in rows[:5]:
            print(" ", r.get("obj"), r.get("package_name"), r.get("location"))
        (SCRATCH / f"{master.stem}_streaming_decoded.json").write_text(
            json.dumps(rows, indent=2), encoding="utf-8"
        )
        man = build_manifest_from_streaming(
            district_id, folder, pkg_folder, rows, IMPORTED / folder
        )
        outp = OUT / out_name
        outp.write_text(json.dumps(man, indent=2), encoding="utf-8")
        print(
            f"  wrote {outp} placements={len(man['placements'])} packages={len(man['source_packages'])} layout={man['layout']}"
        )
        if man["source_packages"]:
            print("  first packages:", man["source_packages"][:8])
            print("  player_start", man["player_start"])


if __name__ == "__main__":
    main()
