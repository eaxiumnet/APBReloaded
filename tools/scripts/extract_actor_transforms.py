"""Extract REAL actor transforms + mesh links from APB UE3 district packages.

Replaces the fabricated pipeline in extract_streamed_actors.py where rotations were
synthesized as ``(i*11)%360``, scale hardcoded ``[1,1,1]``, and mesh identity assigned
round-robin (``imported[i % len]``). This reads genuine UE3 tagged properties:

    * Location  -> StructProperty(Vector)        world position (unreal units)
    * Rotation  -> StructProperty(Rotator) URU   yaw/pitch/roll, converted to degrees
    * mesh      -> actor.StaticMeshComponent (export objref) -> component.StaticMesh
                   (import objref) -> import-table object name

Proven extraction chain (verified end-to-end against FinancialDistrict_Block09 family):
    decompress.exe -game=apb -out=<dir> <file.APB>   (LZO -> flat image)
    umodel_64.exe  -list -game=apb -path=<maps> STEM (export serial offsets, in_file)
    walk tagged props at the byte offset of each name-index tag.

A single logical "block" spans multiple packages (the shells in Block09.APB, plus the
props/art-props/design overlays). extract_block() aggregates that family.

UE3 Rotator units: 65536 URU == 360 degrees.
"""
from __future__ import annotations

import re
import struct
import subprocess
from dataclasses import dataclass, field
from pathlib import Path

UMODEL = Path(r"D:\APBReloaded\tools\UEViewer\umodel_64.exe")
DECOMPRESS = Path(r"D:\APBReloaded\tools\UEViewer\Tools\PackageUnpack\decompress.exe")
FLAT_CACHE = Path(r"C:\Users\Support\AppData\Local\Temp\opencode\apb_flatcache")
UE_TAG = 0x9E2A83C1

# Actor classes that carry a placeable mesh + transform. Roads/terrain tiles use
# cStreamedComponentSet (handled separately by location only if requested).
MESH_ACTOR_CLASSES = (
    "StaticMeshActor",
    "cStreamedLightingStaticMeshActor",
    "cProp",
    "cStreamedBuildingActor",  # Location only; mesh via component set (best-effort)
)


def uru_to_deg(uru: int) -> float:
    """Convert a UE3 Rotator axis (int32 URU) to degrees in [0, 360)."""
    d = (uru % 65536) * 360.0 / 65536.0
    return round(d, 3)


# --------------------------------------------------------------------------- IO

def decompress_package(stem: str, maps_dir: Path) -> Path:
    """Decompress <stem>.APB to a cached flat image; return the flat path."""
    FLAT_CACHE.mkdir(parents=True, exist_ok=True)
    flat = FLAT_CACHE / f"{stem}.APB"
    if flat.exists() and flat.stat().st_size > 0:
        return flat
    src = maps_dir / f"{stem}.APB"
    if not src.exists():
        raise FileNotFoundError(src)
    subprocess.run(
        [str(DECOMPRESS), "-game=apb", f"-out={FLAT_CACHE}", str(src)],
        capture_output=True, text=True, timeout=300, check=False,
    )
    if not flat.exists():
        raise RuntimeError(f"decompress produced no output for {stem} in {FLAT_CACHE}")
    return flat


def umodel_list(stem: str, maps_dir: Path) -> list[dict]:
    """Return export rows [{idx, off, size, cls, name}] using umodel -list serial offsets.

    Offsets are valid against the decompressed flat image (in_file layout preserved).
    """
    r = subprocess.run(
        [str(UMODEL), "-list", "-game=apb", f"-path={maps_dir}", stem],
        capture_output=True, text=True, timeout=240, errors="replace",
    )
    rows: list[dict] = []
    for line in (r.stdout or "").splitlines():
        parts = line.split()
        if len(parts) >= 5 and parts[0].isdigit():
            try:
                rows.append({
                    "idx": int(parts[0]),
                    "off": int(parts[1], 16),
                    "size": int(parts[2], 16),
                    "cls": parts[3],
                    "name": parts[4],
                })
            except ValueError:
                continue
    return rows


# ----------------------------------------------------------------- header parse

@dataclass
class Package:
    names: list[str] = field(default_factory=list)
    imports: list[dict] = field(default_factory=list)
    name_index: dict[str, int] = field(default_factory=dict)


class _Reader:
    def __init__(self, data: bytes, pos: int = 0):
        self.d = data
        self.p = pos

    def i32(self) -> int:
        v = struct.unpack_from("<i", self.d, self.p)[0]
        self.p += 4
        return v

    def u32(self) -> int:
        v = struct.unpack_from("<I", self.d, self.p)[0]
        self.p += 4
        return v

    def f32(self) -> float:
        v = struct.unpack_from("<f", self.d, self.p)[0]
        self.p += 4
        return v

    def raw(self, n: int) -> bytes:
        b = self.d[self.p : self.p + n]
        self.p += n
        return b

    def seek(self, p: int) -> None:
        self.p = p

    def fname(self, names: list[str]) -> str:
        idx = self.i32()
        self.i32()  # number
        return names[idx] if 0 <= idx < len(names) else f"?{idx}"


def parse_package(data: bytes) -> Package:
    r = _Reader(data)
    if r.u32() != UE_TAG:
        raise ValueError("bad package tag")
    _file_ver, lic_ver = struct.unpack_from("<HH", data, 4)
    r.seek(8)
    r.i32()  # total header size
    n = r.i32()
    r.raw(n if n > 0 else (-n) * 2)  # folder name
    r.u32()  # package flags
    name_count = r.i32()
    name_offset = r.i32()
    r.i32()  # export_count
    r.i32()  # export_offset
    if lic_ver >= 29:
        r.i32()
    if lic_ver >= 28:
        for _ in range(5):
            r.f32()
    import_count = r.i32()
    import_offset = r.i32()

    r.seek(name_offset)
    names: list[str] = []
    for _ in range(name_count):
        ln = r.i32()
        if ln > 0:
            s = r.raw(ln)[:-1].decode("latin-1", "replace")
        elif ln < 0:
            s = r.raw((-ln) * 2).decode("utf-16-le", "replace")[:-1]
        else:
            s = ""
        r.u32(); r.u32()  # name flags (2x u32)
        names.append(s)

    r.seek(import_offset)
    imports: list[dict] = []
    for _ in range(import_count):
        r.fname(names)                 # ClassPackage
        cls = r.fname(names)           # ClassName
        r.i32()                        # PackageIndex (outer)
        obj = r.fname(names)           # ObjectName
        imports.append({"cls": cls, "name": obj})

    return Package(names=names, imports=imports,
                   name_index={nm: i for i, nm in enumerate(names)})


def resolve_object(idx: int, pkg: Package, rows: list[dict]) -> str | None:
    """Resolve a UE object index: <0 import, >0 export, 0 none."""
    if idx < 0:
        i = -idx - 1
        if 0 <= i < len(pkg.imports):
            return pkg.imports[i]["name"]
    elif idx > 0:
        e = idx - 1
        if 0 <= e < len(rows):
            return rows[e]["name"]
    return None


# ----------------------------------------------------------- tagged-prop access

def scan_prop(blob: bytes, names: list[str], target_idx: int) -> tuple[str, object] | None:
    """Find the first tagged property whose name-index == target_idx, verifying its
    type field is a known property type, then decode it.
    """
    if target_idx is None:
        return None
    limit = len(blob) - 24
    for p in range(0, max(0, limit)):
        if struct.unpack_from("<i", blob, p)[0] != target_idx:
            continue
        ti = struct.unpack_from("<i", blob, p + 8)[0]
        if not (0 <= ti < len(names)):
            continue
        ty = names[ti]
        if ty not in ("StructProperty", "ObjectProperty", "IntProperty", "FloatProperty"):
            continue
        size = struct.unpack_from("<i", blob, p + 16)[0]
        body = p + 24
        if ty == "StructProperty":
            if body + 8 > len(blob):
                continue
            si = struct.unpack_from("<i", blob, body)[0]
            sname = names[si] if 0 <= si < len(names) else None
            payload = body + 8
            if sname == "Vector" and size >= 12 and payload + 12 <= len(blob):
                return ("Vector", list(struct.unpack_from("<3f", blob, payload)))
            if sname == "Rotator" and size >= 12 and payload + 12 <= len(blob):
                return ("Rotator", list(struct.unpack_from("<3i", blob, payload)))
            continue
        if ty == "ObjectProperty" and body + 4 <= len(blob):
            return ("Object", struct.unpack_from("<i", blob, body)[0])
        if ty in ("IntProperty", "FloatProperty") and body + 4 <= len(blob):
            fmt = "<i" if ty == "IntProperty" else "<f"
            return (ty, struct.unpack_from(fmt, blob, body)[0])
    return None


def resolve_mesh(actor_blob: bytes, pkg: Package, rows: list[dict], data: bytes) -> tuple[str | None, list[float] | None, str]:
    """Return ``(mesh_name, scale3d, source)``; source in
    {component, direct, compset, prefab, none}. ``prefab`` means the mesh ref lives
    in a separate prefab package and is not resolvable in-package.
    """
    ni = pkg.name_index

    def component_mesh_scale(obj_idx: int) -> tuple[str | None, list[float] | None]:
        if not (obj_idx > 0 and obj_idx - 1 < len(rows)):
            return None, None
        crow = rows[obj_idx - 1]
        cblob = data[crow["off"] : crow["off"] + crow["size"]]
        sc = scan_prop(cblob, pkg.names, ni.get("Scale3D"))
        scale = sc[1] if sc and sc[0] == "Vector" else None
        sm = scan_prop(cblob, pkg.names, ni.get("StaticMesh"))
        if sm and sm[0] == "Object":
            return resolve_object(sm[1], pkg, rows), scale
        return None, scale

    smc = scan_prop(actor_blob, pkg.names, ni.get("StaticMeshComponent"))
    if smc and smc[0] == "Object" and smc[1] > 0:
        nm, scale = component_mesh_scale(smc[1])
        if nm:
            return nm, scale, "component"
        _kept_scale = scale
    else:
        _kept_scale = None

    for key in ("StaticMesh", "Mesh"):
        d = scan_prop(actor_blob, pkg.names, ni.get(key))
        if d and d[0] == "Object":
            nm = resolve_object(d[1], pkg, rows)
            if nm:
                return nm, _kept_scale, "direct"

    mcs = scan_prop(actor_blob, pkg.names, ni.get("m_ComponentSet"))
    if mcs and mcs[0] == "Object" and mcs[1] > 0 and mcs[1] - 1 < len(rows):
        csrow = rows[mcs[1] - 1]
        csblob = data[csrow["off"] : csrow["off"] + csrow["size"]]
        for member in _array_objrefs(csblob, pkg.names, ni.get("m_aComponents")):
            if member > 0 and member - 1 < len(rows) and rows[member - 1]["cls"] == "StaticMeshComponent":
                nm, scale = component_mesh_scale(member)
                if nm:
                    return nm, scale or _kept_scale, "compset"

    if smc and smc[0] == "Object":
        return None, _kept_scale, "prefab"
    return None, _kept_scale, "none"


def _array_objrefs(blob: bytes, names: list[str], target_idx: int | None) -> list[int]:
    if target_idx is None:
        return []
    for p in range(0, len(blob) - 24):
        if struct.unpack_from("<i", blob, p)[0] != target_idx:
            continue
        ti = struct.unpack_from("<i", blob, p + 8)[0]
        if not (0 <= ti < len(names)) or names[ti] != "ArrayProperty":
            continue
        body = p + 24
        cnt = struct.unpack_from("<i", blob, body)[0]
        if not (0 <= cnt < 1024) or body + 4 + cnt * 4 > len(blob):
            return []
        return [struct.unpack_from("<i", blob, body + 4 + k * 4)[0] for k in range(cnt)]
    return []


# --------------------------------------------------------------- extraction API

def extract_package(stem: str, maps_dir: Path) -> list[dict]:
    """Extract every mesh-bearing actor's real transform + mesh name from one package."""
    flat = decompress_package(stem, maps_dir)
    data = flat.read_bytes()
    pkg = parse_package(data)
    rows = umodel_list(stem, maps_dir)
    ni = pkg.name_index
    loc_i, rot_i = ni.get("Location"), ni.get("Rotation")

    out: list[dict] = []
    for row in rows:
        if row["cls"] not in MESH_ACTOR_CLASSES:
            continue
        if row["off"] + row["size"] > len(data):
            continue
        blob = data[row["off"] : row["off"] + row["size"]]
        loc = scan_prop(blob, pkg.names, loc_i)
        if not (loc and loc[0] == "Vector"):
            continue  # no world position => not a placed instance
        location = [round(v, 3) for v in loc[1]]

        rot = scan_prop(blob, pkg.names, rot_i)
        if rot and rot[0] == "Rotator":
            pitch, yaw, roll = rot[1]
            rotation = [uru_to_deg(pitch), uru_to_deg(yaw), uru_to_deg(roll)]
        else:
            rotation = [0.0, 0.0, 0.0]  # UE3 default: identity when Rotation absent

        mesh_name, scale3d, source = resolve_mesh(blob, pkg, rows, data)
        scale = [round(v, 4) for v in scale3d] if scale3d else [1.0, 1.0, 1.0]
        out.append({
            "actor": row["name"],
            "actor_class": row["cls"],
            "package": stem,
            "location": location,
            "rotation": rotation,
            "scale": scale,
            "mesh_name": mesh_name,
            "mesh_source": source,
        })
    return out


def _block_family(block_stem: str, maps_dir: Path) -> list[str]:
    """Resolve the package family for a block: the block shell package plus its
    Props/ArtProps/Design overlays that share the same district + block number.
    """
    m = re.match(r"(?P<district>.+?)_Block(?P<num>\d+)$", block_stem)
    if not m:
        return [block_stem]
    district, num = m.group("district"), m.group("num")
    wanted_suffixes = (
        f"_Block{num}",
        f"_Props_Block{num}",
        f"_ArtProps_Block{num}",
        f"_Block{num}_Design",
    )
    fam: list[str] = []
    for suf in wanted_suffixes:
        p = maps_dir / f"{district}{suf}.APB"
        if p.exists():
            fam.append(f"{district}{suf}")
    return fam or [block_stem]


def extract_block(block_stem: str, maps_dir: Path) -> list[dict]:
    """Aggregate real transforms across a block's whole package family."""
    rows: list[dict] = []
    for stem in _block_family(block_stem, maps_dir):
        try:
            rows.extend(extract_package(stem, maps_dir))
        except (FileNotFoundError, RuntimeError, ValueError):
            continue
    return rows


if __name__ == "__main__":
    import collections
    import json
    import sys

    stem = sys.argv[1] if len(sys.argv) > 1 else "FinancialDistrict_Block09"
    maps = Path(sys.argv[2]) if len(sys.argv) > 2 else Path(
        r"D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America"
        r"\APBGame\Content\FinancialDistrict\Maps"
    )
    recs = extract_block(stem, maps)
    yaws = {r["rotation"][1] for r in recs}
    meshed = sum(1 for r in recs if r["mesh_name"])
    scaled = sum(1 for r in recs if r["scale"] != [1.0, 1.0, 1.0])
    src = collections.Counter(r["mesh_source"] for r in recs)
    print(f"actors={len(recs)} distinct_yaws={len(yaws)} meshed={meshed}/{len(recs)} "
          f"nonunit_scale={scaled} sources={dict(src)}")
    for r in recs[:5]:
        print(json.dumps(r))
