"""Tests for the psk -> glTF 2.0 (.glb) exporter, validating binary structure."""

from __future__ import annotations

import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from gltf_export import mesh_to_glb  # noqa: E402
from psk import parse_psk_file  # noqa: E402
from texture_resolver import find_default_textures  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[4]
MAGNUM = (
    REPO_ROOT
    / "Content/Extracted/WeaponsBase/Weapon_Armas_Magnum/Weapon_Armas_Magnum"
    / "SkeletalMesh3/Crm_Magnum_Clip_mk3_LOD0.psk"
)
TOMMY_GUN = (
    REPO_ROOT
    / "Content/Extracted/WeaponsBase/Weapon_TommyGun/Weapon_TommyGun"
    / "SkeletalMesh3/Weapon_TommyGun_LOD0.psk"
)

_GLB_MAGIC = 0x46546C67
_CHUNK_JSON = 0x4E4F534A
_CHUNK_BIN = 0x004E4942


def _parse_glb(blob: bytes):
    """Return (gltf_dict, bin_bytes) from a .glb, asserting the container is well-formed."""
    assert len(blob) >= 12, "glb too short"
    magic, version, total = struct.unpack_from("<III", blob, 0)
    assert magic == _GLB_MAGIC, f"bad magic {magic:#x}"
    assert version == 2, f"bad version {version}"
    assert total == len(blob), f"declared total {total} != actual {len(blob)}"

    pos = 12
    gltf = None
    bindata = b""
    while pos < len(blob):
        clen, ctype = struct.unpack_from("<II", blob, pos)
        pos += 8
        chunk = blob[pos : pos + clen]
        assert len(chunk) == clen, "truncated chunk"
        assert clen % 4 == 0, "chunk not 4-byte aligned"
        if ctype == _CHUNK_JSON:
            gltf = json.loads(chunk.decode("utf-8"))
        elif ctype == _CHUNK_BIN:
            bindata = chunk
        pos += clen
    assert gltf is not None, "no JSON chunk"
    return gltf, bindata


def test_glb_container_valid() -> None:
    mesh = parse_psk_file(MAGNUM)
    gltf, bindata = _parse_glb(mesh_to_glb(mesh))
    assert gltf["asset"]["version"] == "2.0"
    assert gltf["buffers"][0]["byteLength"] == len(bindata)


def test_accessor_counts_match_mesh() -> None:
    mesh = parse_psk_file(MAGNUM)
    gltf, _ = _parse_glb(mesh_to_glb(mesh))

    prims = gltf["meshes"][0]["primitives"]
    assert len(prims) == 1, f"expected 1 primitive (1 material), got {len(prims)}"

    prim = prims[0]
    pos_acc = gltf["accessors"][prim["attributes"]["POSITION"]]
    uv_acc = gltf["accessors"][prim["attributes"]["TEXCOORD_0"]]
    idx_acc = gltf["accessors"][prim["indices"]]

    assert pos_acc["count"] == len(mesh.wedges) == 24, pos_acc["count"]
    assert pos_acc["type"] == "VEC3"
    assert uv_acc["count"] == len(mesh.wedges) == 24
    assert uv_acc["type"] == "VEC2"
    assert idx_acc["count"] == len(mesh.faces) * 3 == 36, idx_acc["count"]
    assert idx_acc["type"] == "SCALAR"


def test_position_bounds_present_and_sane() -> None:
    mesh = parse_psk_file(MAGNUM)
    gltf, _ = _parse_glb(mesh_to_glb(mesh))
    pos_acc = gltf["accessors"][0]
    assert "min" in pos_acc and "max" in pos_acc
    for lo, hi in zip(pos_acc["min"], pos_acc["max"]):
        assert lo <= hi, f"min {lo} > max {hi}"


def test_material_name_carried() -> None:
    mesh = parse_psk_file(MAGNUM)
    gltf, _ = _parse_glb(mesh_to_glb(mesh))
    names = [m.get("name") for m in gltf.get("materials", [])]
    assert "Crm_Magnum_Mk3_MAT" in names, names


def test_glb_has_embedded_texture() -> None:
    mesh = parse_psk_file(TOMMY_GUN)
    textures = find_default_textures(TOMMY_GUN)

    gltf, _ = _parse_glb(mesh_to_glb(mesh, textures=textures))

    assert gltf["images"]
    base_color_index = gltf["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"]["index"]
    assert gltf["textures"][base_color_index]["source"] == 0
    assert gltf["images"][0]["mimeType"] == "image/png"
    assert gltf["textures"]
    assert gltf["samplers"]
    assert gltf["materials"][0]["normalTexture"]["index"] == 1
    assert "target" not in gltf["bufferViews"][gltf["images"][0]["bufferView"]]


def test_glb_has_normals() -> None:
    mesh = parse_psk_file(TOMMY_GUN)
    gltf, _ = _parse_glb(mesh_to_glb(mesh))

    primitive = gltf["meshes"][0]["primitives"][0]
    normal_accessor = gltf["accessors"][primitive["attributes"]["NORMAL"]]

    assert normal_accessor["componentType"] == 5126
    assert normal_accessor["type"] == "VEC3"
    assert normal_accessor["count"] == len(mesh.wedges)


def test_glb_without_texture_still_valid() -> None:
    mesh = parse_psk_file(MAGNUM)

    gltf, bindata = _parse_glb(mesh_to_glb(mesh))

    assert gltf["asset"]["version"] == "2.0"
    assert gltf["buffers"][0]["byteLength"] == len(bindata)
    assert "baseColorTexture" not in gltf["materials"][0]["pbrMetallicRoughness"]


# --- standalone runner (repo FAILS=N convention; no pytest required) ---
if __name__ == "__main__":
    fails = 0
    for name, fn in sorted(globals().items()):
        if name.startswith("test_") and callable(fn):
            try:
                fn()
                print(f"PASS: {name}")
            except Exception as exc:  # noqa: BLE001
                fails += 1
                print(f"FAIL: {name}: {exc}")
    print(f"FAILS={fails}")
    sys.exit(1 if fails else 0)
