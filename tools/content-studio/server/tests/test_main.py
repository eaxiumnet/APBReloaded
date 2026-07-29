from __future__ import annotations

import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from main import mesh_glb  # noqa: E402

MESH = "Weapon_AssaultRifle/Weapon_AssaultRifle/SkeletalMesh3/Weapon_AssaultRifle_LOD0.psk"
SKIN = "Weapon_AssaultRifle/MaterialInstanceConstant/Weapon_AssaultRifle_Bloodrose_MAT_INST.props.txt"


def test_mesh_glb_with_missing_skin_material_degrades_to_base_render() -> None:
    response = mesh_glb(path=MESH, skin="Weapon_AssaultRifle/Missing_MAT_INST.props.txt")
    assert response.status_code == 200
    assert response.body[:4] == b"glTF"


def test_mesh_glb_with_bloodrose_composites_texture_without_factor_darkening() -> None:
    # Given / When
    response = mesh_glb(path=MESH, skin=SKIN)
    # Then
    assert response.status_code == 200
    json_length = struct.unpack_from("<I", response.body, 12)[0]
    gltf = json.loads(response.body[20 : 20 + json_length].decode("utf-8"))
    material = gltf["materials"][0]["pbrMetallicRoughness"]
    assert material["baseColorFactor"] == [1.0, 1.0, 1.0, 1.0]
    assert "baseColorTexture" in material
