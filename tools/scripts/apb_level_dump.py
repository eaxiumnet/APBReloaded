"""Proper UE3 tagged-property walker for APB retail .APB level packages.

extract_actor_transforms.scan_prop() brute-force scans for a name index and takes the
first plausible hit -> retail resolution was 46/507. This walks the tagged-property
chain sequentially, the only correct way to read UE3 properties. It also fixes a cache
collision where flat images were keyed by package stem only, so identically named 2011
and retail packages overwrote each other.
"""
from __future__ import annotations

import hashlib
import struct
import subprocess
from pathlib import Path

UMODEL = Path(r"D:\APBReloaded\tools\_incoming\umodel_apb\umodel_64.exe")
DECOMPRESS = Path(r"D:\APBReloaded\tools\UEViewer\Tools\PackageUnpack\decompress.exe")
FLAT_ROOT = Path(r"C:\Users\Support\AppData\Local\Temp\opencode\apb_flat")
UE_TAG = 0x9E2A83C1


class R:
    def __init__(self, d: bytes, p: int = 0) -> None:
        self.d, self.p = d, p

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

    def fname(self, names: list[str]) -> str:
        i = self.i32()
        self.i32()
        return names[i] if 0 <= i < len(names) else f"?{i}"


def flat_path(stem: str, maps_dir: Path) -> Path:
    key = hashlib.sha1(str(maps_dir).lower().encode()).hexdigest()[:10]
    return FLAT_ROOT / key / f"{stem}.APB"


def decompress(stem: str, maps_dir: Path) -> Path:
    out = flat_path(stem, maps_dir)
    if out.exists() and out.stat().st_size > 0:
        return out
    src = next((maps_dir / f"{stem}{e}" for e in (".APB", ".apb")
                if (maps_dir / f"{stem}{e}").exists()), None)
    if src is None:
        raise FileNotFoundError(f"{stem} in {maps_dir}")
    out.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(DECOMPRESS), "-game=apb", f"-out={out.parent}", str(src)],
                   capture_output=True, text=True, timeout=600, check=False)
    produced = out.parent / src.name
    if produced != out and produced.exists():
        produced.replace(out)
    if not out.exists():
        raise RuntimeError(f"decompress produced nothing for {stem}")
    return out


def exports(stem: str, maps_dir: Path) -> list[dict]:
    """Export rows via umodel -list; offsets are valid against the flat image."""
    r = subprocess.run([str(UMODEL), "-list", "-game=apb", f"-path={maps_dir}", stem],
                       capture_output=True, text=True, timeout=600, errors="replace")
    rows: list[dict] = []
    for line in (r.stdout or "").splitlines():
        t = line.split()
        if len(t) >= 5 and t[0].isdigit():
            try:
                rows.append({"idx": int(t[0]), "off": int(t[1], 16),
                             "size": int(t[2], 16), "cls": t[3], "name": t[4]})
            except ValueError:
                continue
    return rows


class Pkg:
    def __init__(self, names: list[str], imports: list[dict]) -> None:
        self.names = names
        self.imports = imports
        self.ni = {n: i for i, n in enumerate(names)}


def parse_package(data: bytes) -> Pkg:
    r = R(data)
    if r.u32() != UE_TAG:
        raise ValueError("bad package tag")
    lic = struct.unpack_from("<H", data, 6)[0]
    r.p = 8
    r.i32()                                  # total header size
    n = r.i32()
    r.raw(n if n > 0 else (-n) * 2)           # folder name
    r.u32()                                  # package flags
    name_count, name_off = r.i32(), r.i32()
    r.i32(), r.i32()                         # export count / offset
    if lic >= 29:
        r.i32()
    if lic >= 28:
        for _ in range(5):
            r.f32()
    imp_count, imp_off = r.i32(), r.i32()

    r.p = name_off
    names: list[str] = []
    for _ in range(name_count):
        ln = r.i32()
        if ln > 0:
            s = r.raw(ln)[:-1].decode("latin-1", "replace")
        elif ln < 0:
            s = r.raw((-ln) * 2).decode("utf-16-le", "replace")[:-1]
        else:
            s = ""
        r.u32(), r.u32()                      # name flags
        names.append(s)

    r.p = imp_off
    imports: list[dict] = []
    for _ in range(imp_count):
        r.fname(names)                        # class package
        cls = r.fname(names)                  # class name
        r.i32()                               # outer
        imports.append({"cls": cls, "name": r.fname(names)})
    return Pkg(names, imports)


_PRIMS = {"IntProperty": "<i", "FloatProperty": "<f", "ObjectProperty": "<i"}

UE3_PROP_TYPES = frozenset((
    "ByteProperty", "IntProperty", "BoolProperty", "FloatProperty", "ObjectProperty",
    "NameProperty", "DelegateProperty", "ClassProperty", "ArrayProperty",
    "StructProperty", "StrProperty", "MapProperty", "InterfaceProperty",
    "ComponentProperty", "FixedArrayProperty",
))

_NI_CACHE: dict[int, tuple[list[str], dict[str, int]]] = {}


def name_index(names: list[str]) -> dict[str, int]:
    """Cached name -> index map. Holds a reference to `names` so the id() key stays
    valid for the life of the cache entry."""
    hit = _NI_CACHE.get(id(names))
    if hit is not None and hit[0] is names:
        return hit[1]
    m = {n: i for i, n in enumerate(names)}
    _NI_CACHE[id(names)] = (names, m)
    return m


def walk_from(blob: bytes, start: int, names: list[str],
              decode: bool = True) -> tuple[list[dict], bool]:
    """Walk a UE3 tagged-property chain from `start`; returns (props, hit_None).

    Bails on the first entry whose type FName is not a real UE3 property type. That
    gate is what makes walk_best's anchor search unambiguous, and `decode=False`
    keeps the search cheap by skipping value decoding.
    """
    r = R(blob, start)
    out: list[dict] = []
    n = len(blob)
    while r.p + 8 <= n:
        try:
            pname = r.fname(names)
            if pname == "None":
                return out, True
            if r.p + 12 > n:
                break
            ptype = r.fname(names)
            if ptype not in UE3_PROP_TYPES:
                break
            size = r.i32()
            aidx = r.i32()
            if size < 0 or aidx < 0 or size > n:
                break
            sname, bval = None, None
            if ptype in ("StructProperty", "ByteProperty"):
                if r.p + 8 > n:
                    break
                sname = r.fname(names)
            elif ptype == "BoolProperty":
                if r.p + 1 > n:
                    break
                bval = r.raw(1)[0] != 0
            if r.p + size > n:
                break
            body = r.raw(size)
        except (struct.error, IndexError):
            break
        out.append({"name": pname, "idx": aidx, "type": ptype, "struct": sname,
                    "value": decode_prop(ptype, sname, body, bval, names) if decode
                    else None})
    return out, False


def walk_best(blob: bytes, names: list[str]) -> tuple[list[dict], int]:
    """Locate the real chain start and walk it.

    APB actor exports open with a variable-length binary header, so properties never
    start at offset 0 (components at 8, actors far deeper). Trying every 4-byte offset
    is O(n^2) and hangs on large exports, so anchor on the 4-byte encoding of a
    property-type FName index -- located at C speed with bytes.find -- and walk only
    those few candidates.

    The *earliest* 'None'-terminated chain wins, not the longest. The header always
    precedes the chain, so a later anchor necessarily lands inside the real chain's
    value bytes and yields a plausible-looking but bogus sub-chain. Measured on retail
    FinancialDistrict_Block09: earliest resolves cStreamedComponentSet.m_aComponents
    137/137 where longest resolves 12/137, and is identical on every other class.
    """
    ni = name_index(names)
    anchors: set[int] = set()
    for t in UE3_PROP_TYPES:
        ti = ni.get(t)
        if ti is None:
            continue
        pat = struct.pack("<i", ti)
        pos = blob.find(pat)
        while pos != -1:
            if pos >= 8:
                anchors.add(pos - 8)   # rewind over the property-name FName
            pos = blob.find(pat, pos + 1)   # chains are byte-packed, not aligned
    for off in sorted(anchors):
        props, done = walk_from(blob, off, names, decode=False)
        if done and props:
            return walk_from(blob, off, names)[0], off
    return [], -1


def walk_props(blob: bytes, names: list[str]) -> list[dict]:
    return walk_best(blob, names)[0]


def props_map(blob: bytes, names: list[str]) -> dict:
    """walk_props as a {name: value} map (last write wins, array idx suffixed)."""
    m: dict = {}
    for p in walk_props(blob, names):
        m[p["name"] if p["idx"] == 0 else f"{p['name']}[{p['idx']}]"] = p["value"]
    return m


def decode_prop(ptype: str, sname: str | None, body: bytes,
                bval: bool | None, names: list[str]):
    if ptype == "BoolProperty":
        return bval
    if ptype in _PRIMS:
        return struct.unpack_from(_PRIMS[ptype], body)[0] if len(body) >= 4 else None
    if ptype == "NameProperty":
        if len(body) >= 8:
            i = struct.unpack_from("<i", body)[0]
            return names[i] if 0 <= i < len(names) else f"?{i}"
        return None
    if ptype == "StrProperty":
        if len(body) < 4:
            return None
        ln = struct.unpack_from("<i", body)[0]
        if ln > 0:
            return body[4:4 + ln].rstrip(b"\x00").decode("latin-1", "replace")
        if ln < 0:
            return body[4:4 + (-ln) * 2].decode("utf-16-le", "replace").rstrip("\x00")
        return ""
    if ptype == "ByteProperty":
        return {"enum": sname, "v": body[0] if body else None}
    if ptype == "ArrayProperty":
        if len(body) < 4:
            return None
        return {"count": struct.unpack_from("<i", body)[0], "raw": body[4:]}
    if ptype == "StructProperty":
        n = len(body)
        if sname in ("Vector", "Rotator") and n >= 12:
            return list(struct.unpack_from("<3f" if sname == "Vector" else "<3i", body))
        if sname in ("Vector4", "Quat", "LinearColor") and n >= 16:
            return list(struct.unpack_from("<4f", body))
        if sname == "Color" and n >= 4:
            return list(body[:4])
        if sname == "Guid" and n >= 16:
            return body[:16].hex()
        return {"struct": sname, "size": n, "raw": body[:48].hex()}
    return {"type": ptype, "size": len(body), "raw": body[:32].hex()}


def uru_to_deg(u: int) -> float:
    return round((u % 65536) * 360.0 / 65536.0, 3)


RETAIL_MAPS = Path(r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded"
                   r"\APBGame\Content\Release\Maps")


def load(stem: str, maps_dir: Path):
    data = decompress(stem, maps_dir).read_bytes()
    return data, parse_package(data), exports(stem, maps_dir)


def main(argv: list[str]) -> None:
    stem = argv[1] if len(argv) > 1 else "FinancialDistrict_Block09"
    maps = Path(argv[2]) if len(argv) > 2 else RETAIL_MAPS / "FinancialDistrict"
    want = [c.lower() for c in argv[3].split(",")] if len(argv) > 3 else []
    data, pkg, rows = load(stem, maps)
    print(f"# {stem} exports={len(rows)} names={len(pkg.names)} imports={len(pkg.imports)}")
    hist: dict = {}
    loc: dict = {}
    cache: dict = {}
    for row in rows:
        if row["off"] + row["size"] > len(data):
            continue
        m = props_map(data[row["off"]:row["off"] + row["size"]], pkg.names)
        cache[row["idx"]] = m
        hist[row["cls"]] = hist.get(row["cls"], 0) + 1
        if "Location" in m:
            loc[row["cls"]] = loc.get(row["cls"], 0) + 1
    print(f"{'# class':<38}{'n':>6}{'with_Location':>15}")
    for c, n in sorted(hist.items(), key=lambda kv: -kv[1]):
        print(f"  {c:<36}{n:>6}{loc.get(c, 0):>15}")
    for row in rows:
        if row["cls"].lower() in want:
            print(f"\n=== [{row['idx']}] {row['cls']} {row['name']} size={row['size']:#x}")
            for k, v in cache.get(row["idx"], {}).items():
                print(f"    {k:<30} = {v}")
            want.remove(row["cls"].lower())


if __name__ == "__main__":
    import sys
    main(sys.argv)

