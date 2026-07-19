"""Parse decompressed APB MASTER packages for LevelStreaming PackageName + Location.

APB FileVersion 564 / Licensee 33. Requires PackageUnpack decompress first.
"""
from __future__ import annotations

import json
import struct
from dataclasses import dataclass
from pathlib import Path

SCRATCH = Path(r"C:\Users\Support\AppData\Local\Temp\grok-goal-9ca60165ac93\implementer")
UNPACKED = SCRATCH / "unpacked_maps"
OUT_DIR = Path(r"D:\APBReloaded\Content\Data\district_placements")
PKG_ROOT = Path(
    r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Content\Release\Packages"
)
IMPORTED = Path(r"D:\APBReloaded\Content\Imported\Districts")


class Reader:
    def __init__(self, data: bytes, pos: int = 0):
        self.data = data
        self.pos = pos

    def tell(self):
        return self.pos

    def seek(self, p):
        self.pos = p

    def remaining(self):
        return len(self.data) - self.pos

    def u8(self):
        v = self.data[self.pos]
        self.pos += 1
        return v

    def i32(self):
        v = struct.unpack_from("<i", self.data, self.pos)[0]
        self.pos += 4
        return v

    def u32(self):
        v = struct.unpack_from("<I", self.data, self.pos)[0]
        self.pos += 4
        return v

    def i64(self):
        v = struct.unpack_from("<q", self.data, self.pos)[0]
        self.pos += 8
        return v

    def f32(self):
        v = struct.unpack_from("<f", self.data, self.pos)[0]
        self.pos += 4
        return v

    def raw(self, n: int):
        b = self.data[self.pos : self.pos + n]
        self.pos += n
        return b

    def fname(self, names: list[str]) -> str:
        # FName: index int32, number int32
        idx = self.i32()
        num = self.i32()
        if 0 <= idx < len(names):
            base = names[idx]
        else:
            base = f"NAME_{idx}"
        if num > 0:
            return f"{base}_{num - 1}" if False else f"{base}_{num}"  # UE uses Number
        return base

    def fname_raw(self):
        return self.i32(), self.i32()


def read_names(r: Reader, count: int) -> list[str]:
    names = []
    for _ in range(count):
        # FString: length int32, then chars (positive = ansi, negative = unicode)
        n = r.i32()
        if n == 0:
            names.append("")
            continue
        if n > 0:
            raw = r.raw(n)
            s = raw.split(b"\x00")[0].decode("latin-1", "replace")
        else:
            raw = r.raw((-n) * 2)
            s = raw.decode("utf-16-le", "replace").split("\x00")[0]
        # Flags after name in UE3
        _flags = r.u32()
        names.append(s)
    return names


@dataclass
class Export:
    class_index: int
    super_index: int
    package_index: int
    object_name: str
    serial_size: int
    serial_offset: int


def resolve_class_name(class_index: int, names: list[str], imports: list, exports: list) -> str:
    # Package index: >0 export, <0 import, 0 null
    if class_index == 0:
        return "None"
    if class_index < 0:
        imp = imports[-class_index - 1]
        return imp
    return exports[class_index - 1].object_name if class_index - 1 < len(exports) else f"Export{class_index}"


def parse_package(path: Path) -> dict:
    data = path.read_bytes()
    r = Reader(data)
    tag = r.u32()
    if tag != 0x9E2A83C1:
        raise ValueError(f"bad tag {tag:#x}")
    file_ver = struct.unpack_from("<H", data, 4)[0]
    lic_ver = struct.unpack_from("<H", data, 6)[0]
    r.seek(8)
    # TotalHeaderSize
    total_header = r.i32()
    # FolderName FString
    n = r.i32()
    if n > 0:
        r.raw(n)
    elif n < 0:
        r.raw((-n) * 2)
    package_flags = r.u32()
    name_count = r.i32()
    name_offset = r.i32()
    export_count = r.i32()
    export_offset = r.i32()
    # APB licensee >= 29 unk
    if lic_ver >= 29:
        r.i32()
    if lic_ver >= 28:
        for _ in range(5):
            r.f32()
    import_count = r.i32()
    import_offset = r.i32()
    depends_offset = r.i32()  # ver >= 415
    # APB pad 16 bytes
    if lic_ver >= 33:
        r.i32()
        r.i32()
        r.i32()
        r.i32()

    # Rest of header skipped — use offsets from summary we already have
    r.seek(name_offset)
    names = read_names(r, name_count)

    # Imports: ClassPackage, ClassName, PackageIndex, ObjectName
    r.seek(import_offset)
    imports = []
    for _ in range(import_count):
        # FName ClassPackage, ClassName; int PackageIndex; FName ObjectName
        cp = r.fname(names)
        cn = r.fname(names)
        pkg = r.i32()
        on = r.fname(names)
        imports.append(on)

    # Exports
    r.seek(export_offset)
    exports: list[Export] = []
    for _ in range(export_count):
        class_index = r.i32()
        super_index = r.i32()
        package_index = r.i32()
        obj_name = r.fname(names)
        # Archetype (ver >= 220)
        _archetype = r.i32()
        object_flags = r.u32()
        object_flags2 = r.u32()  # ver >= 195
        serial_size = r.i32()
        serial_offset = r.i32() if (serial_size or file_ver >= 249) else 0
        # ComponentMap + ExportFlags + NetObjectCount + Guid — varies
        # For cooked packages there is more data. Try to find next export by
        # scanning carefully — use residual size heuristic.
        # After SerialOffset: for UE3 ver>=249 often:
        #   ComponentMap (TMap) skipped differently per game
        # APB: try reading ExportFlags int32, then skip Guid (16), NetObjectCount array
        # Conservative: many APB exports are small LevelStreaming with size 0x50
        # We'll read ExportFlags + NetObjectCount count + Guid if present
        # From umodel list SerialSize was 0x50 for LevelStreaming — property data only
        # Export table entry continues with ComponentMap (empty for these)
        # Read: int32 ComponentMapCount? In UE3 FMap is serialized as count then pairs.
        # Standard UE3 after SerialOffset (ver < 543):
        #   if ver < 543: ComponentMap TMap<FName,int>
        #   ExportFlags
        #   if ver >= 247: NetObjectCount array + Guid + PackageGuid? 
        # Simplified approach used by many tools for APB:
        exp_flags = r.u32()
        # Generations of net objects: array of int
        # Actually structure from UnPackage3 after SerialOffset:
        # skip ComponentMap for ver < some: TMap count
        # Let's use known pattern from umodel: for LevelStreaming serial_size=0x50
        # We need correct serial_offset. If export table parse drifts, offsets wrong.
        # Store and continue with a safer full parse.

        # Re-read ComponentMap: TMap FName->int : count then for each FName+int
        # In many cooked APB, ComponentMap is empty (count=0)
        # After ObjectFlags2 SerialSize SerialOffset:
        #   if ArVer < 543: ComponentMap
        # Looking at unpackage3 after SerialOffset...
        exports.append(
            Export(class_index, super_index, package_index, obj_name, serial_size, serial_offset)
        )
        # Problem: we didn't consume remaining export fields. Need full skip.
        # Break and use alternate method: find exports via umodel list offsets
        # after decompress offsets should be valid in file.

    return {
        "path": str(path),
        "file_ver": file_ver,
        "lic_ver": lic_ver,
        "name_count": name_count,
        "export_count": export_count,
        "import_count": import_count,
        "names_sample": names[:30],
        "names": names,
        "imports": imports,
        "exports_partial": [e.__dict__ for e in exports[:5]],
        "data": data,
        "exports": exports,
    }


def parse_exports_full(data: bytes, names: list[str], export_count: int, export_offset: int, file_ver: int) -> list[Export]:
    """Full export table parse for APB 564 with ComponentMap skip."""
    r = Reader(data, export_offset)
    exports = []
    for i in range(export_count):
        start = r.tell()
        class_index = r.i32()
        super_index = r.i32()
        package_index = r.i32()
        obj_name = r.fname(names)
        archetype = r.i32()
        object_flags = r.u32()
        object_flags2 = r.u32()
        serial_size = r.i32()
        serial_offset = r.i32()
        # ComponentMap TMap - count then pairs (FName, int) for ArVer < 543; APB is 564 so maybe skipped
        # From UnPackage3.cpp after SerialOffset for high versions:
        if file_ver < 543:
            cmap_count = r.i32()
            if 0 <= cmap_count < 10000:
                for _ in range(cmap_count):
                    r.fname(names)
                    r.i32()
            else:
                # misaligned — abort
                raise RuntimeError(f"bad ComponentMap count {cmap_count} at export {i} off {start}")
        export_flags = r.u32()
        # NetObjectCount: TArray<int> — count then ints; for ver >= 247
        net_count = r.i32()
        if 0 <= net_count < 64:
            for _ in range(net_count):
                r.i32()
        else:
            # rewind and try without
            r.seek(r.tell() - 4)
            net_count = 0
        # Guid 16 bytes
        r.raw(16)
        # sometimes PackageGuid / U3unk
        # Some versions have more. If next export looks wrong, we'll detect.
        exports.append(
            Export(class_index, super_index, package_index, obj_name, serial_size, serial_offset)
        )
    return exports


def parse_tagged_props(data: bytes, offset: int, size: int, names: list[str]) -> dict:
    """Parse UE3 tagged properties until None."""
    r = Reader(data, offset)
    end = offset + size
    props = {}
    while r.tell() < end - 8:
        name_idx, name_num = r.fname_raw()
        if name_idx < 0 or name_idx >= len(names):
            break
        name = names[name_idx]
        if name_num:
            name = f"{name}_{name_num}"
        if name == "None":
            break
        type_idx, type_num = r.fname_raw()
        if type_idx < 0 or type_idx >= len(names):
            break
        ptype = names[type_idx]
        size = r.i32()
        array_index = r.i32()
        if size < 0 or size > end - r.tell():
            break
        start = r.tell()
        val = None
        if ptype == "NameProperty" and size >= 8:
            val = r.fname(names)
        elif ptype == "StrProperty":
            n = r.i32()
            if n > 0:
                val = r.raw(n).split(b"\x00")[0].decode("latin-1", "replace")
            elif n < 0:
                val = r.raw((-n) * 2).decode("utf-16-le", "replace").split("\x00")[0]
            else:
                val = ""
        elif ptype == "FloatProperty" and size >= 4:
            val = r.f32()
        elif ptype == "IntProperty" and size >= 4:
            val = r.i32()
        elif ptype == "BoolProperty":
            # APB bool often 1 byte
            val = r.u8() != 0
            # size may be 0 for bool in some versions
            if size > 1:
                r.seek(start + size)
        elif ptype == "StructProperty":
            struct_name = r.fname(names)
            payload_size = size - 8  # minus struct name? actually size is full payload after arrayindex; struct name is part of header in UE3
            # In UE3: after ArrayIndex comes StructName FName, then data of size
            # Actually size includes only data after ArrayIndex; StructName is BEFORE data and not in size?
            # Standard: Type, Size, ArrayIndex, [StructName if Struct], data
            # Size is size of data after optional struct name
            # We already read StructName above — data is `size` bytes starting after struct name
            # Wait we read size before struct name in this code — wrong for UE3.
            # Correct order: Name, Type, Size, ArrayIndex, (if Struct: StructName), Data[Size]
            data_start = r.tell()
            if struct_name in ("Vector", "Vector3f", "VectorProperty") or "Vector" in struct_name:
                if size >= 12:
                    val = [r.f32(), r.f32(), r.f32()]
                else:
                    val = {"struct": struct_name, "raw": r.raw(size).hex()}
            elif struct_name in ("Rotator",):
                if size >= 12:
                    val = [r.i32(), r.i32(), r.i32()]
                else:
                    val = {"struct": struct_name, "raw": r.raw(size).hex()}
            else:
                val = {"struct": struct_name, "raw_len": size}
                r.seek(data_start + size)
        elif ptype == "ObjectProperty" and size >= 4:
            val = r.i32()
        elif ptype == "ByteProperty":
            # EnumName then byte for APB
            if size >= 8:
                ename = r.fname(names)
                b = r.u8() if r.tell() < start + size else 0
                val = {"enum": ename, "value": b}
            else:
                val = r.raw(size).hex()
        else:
            val = r.raw(size).hex() if size <= 64 else f"<{size} bytes>"
        # Ensure we're at start+size
        r.seek(start + size)
        props[name] = {"type": ptype, "value": val, "array_index": array_index}
    return props


def dump_streaming_from_list(master_path: Path, list_path: Path | None = None) -> list[dict]:
    """Use umodel-list serial offsets on DECOMPRESSED file."""
    data = master_path.read_bytes()
    # Run umodel -list on unpacked file's directory
    import subprocess
    import re

    umodel = Path(r"D:\APBReloaded\Tools\UEViewer\umodel_64.exe")
    r = subprocess.run(
        [str(umodel), f"-path={master_path.parent}", "-game=apb", "-list", master_path.stem],
        capture_output=True,
        text=True,
        errors="replace",
    )
    text = (r.stdout or "") + (r.stderr or "")
    # Parse names from package ourselves for property dump
    pkg = parse_package_header_names(data)

    rows = []
    for line in text.splitlines():
        m = re.match(
            r"\s*\d+\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)\s+LevelStreamingKismet\s+(\S+)",
            line,
        )
        if not m:
            continue
        off = int(m.group(1), 16)
        size = int(m.group(2), 16)
        name = m.group(3)
        if off + size > len(data):
            rows.append({"name": name, "error": "oob", "off": off, "size": size, "file_size": len(data)})
            continue
        blob = data[off : off + size]
        props = parse_tagged_props(data, off, size, pkg["names"])
        # Also raw float scan
        floats = [struct.unpack_from("<f", blob, i)[0] for i in range(0, max(0, len(blob) - 3), 4)]
        rows.append(
            {
                "name": name,
                "off": off,
                "size": size,
                "props": props,
                "package_name": (props.get("PackageName") or {}).get("value"),
                "location": (props.get("Location") or {}).get("value"),
                "floats": [round(f, 2) for f in floats if abs(f) > 1 and abs(f) < 500000],
                "hex": blob.hex(),
            }
        )
    return rows


def parse_package_header_names(data: bytes) -> dict:
    r = Reader(data)
    tag = r.u32()
    file_ver = struct.unpack_from("<H", data, 4)[0]
    lic_ver = struct.unpack_from("<H", data, 6)[0]
    r.seek(8)
    r.i32()  # total header
    n = r.i32()
    if n > 0:
        r.raw(n)
    elif n < 0:
        r.raw((-n) * 2)
    r.u32()  # flags
    name_count = r.i32()
    name_offset = r.i32()
    export_count = r.i32()
    export_offset = r.i32()
    if lic_ver >= 29:
        r.i32()
    if lic_ver >= 28:
        for _ in range(5):
            r.f32()
    import_count = r.i32()
    import_offset = r.i32()
    r.seek(name_offset)
    names = read_names(r, name_count)
    return {
        "names": names,
        "file_ver": file_ver,
        "lic_ver": lic_ver,
        "export_count": export_count,
        "export_offset": export_offset,
        "import_count": import_count,
        "import_offset": import_offset,
        "name_count": name_count,
        "name_offset": name_offset,
    }


def main():
    results = {}
    for master in [
        UNPACKED / "FinancialDistrict_MASTER.APB",
        UNPACKED / "WaterfrontDistrict_MASTER.APB",
    ]:
        if not master.exists():
            print("missing", master)
            continue
        print(f"=== {master.name} size={master.stat().st_size} ===")
        rows = dump_streaming_from_list(master)
        ok = [r for r in rows if not r.get("error")]
        with_pkg = [r for r in ok if r.get("package_name")]
        with_loc = [r for r in ok if r.get("location")]
        print(f"streaming={len(rows)} ok={len(ok)} with_pkg={len(with_pkg)} with_loc={len(with_loc)}")
        if ok:
            print("sample props keys", list(ok[0].get("props", {}).keys())[:20])
            print("sample", json.dumps({k: ok[0][k] for k in ("name", "package_name", "location", "props") if k in ok[0]}, indent=2)[:800])
        # Save
        outp = SCRATCH / f"{master.stem}_streaming_decoded.json"
        # shrink hex for storage
        slim = []
        for r in rows:
            slim.append(
                {
                    "name": r.get("name"),
                    "package_name": r.get("package_name"),
                    "location": r.get("location"),
                    "props": {k: v.get("value") for k, v in (r.get("props") or {}).items()},
                    "error": r.get("error"),
                    "floats": r.get("floats", [])[:12],
                }
            )
        outp.write_text(json.dumps(slim, indent=2), encoding="utf-8")
        print("wrote", outp)
        results[master.stem] = slim

    (SCRATCH / "streaming_decode_summary.json").write_text(
        json.dumps({k: len(v) for k, v in results.items()}, indent=2), encoding="utf-8"
    )


if __name__ == "__main__":
    main()
