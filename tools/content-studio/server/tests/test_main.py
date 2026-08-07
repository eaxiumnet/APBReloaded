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


def test_clothing_mesh_glb_uses_retail_main_texture_names() -> None:
    from main import clothing_mesh_glb

    response = clothing_mesh_glb(item="F_Armpads_Armoured")
    assert response.status_code == 200
    json_length = struct.unpack_from("<I", response.body, 12)[0]
    gltf = json.loads(response.body[20 : 20 + json_length].decode("utf-8"))
    names = [material["name"] for material in gltf["materials"]]
    assert "baseColorTexture" in gltf["materials"][0]["pbrMetallicRoughness"]
    assert "F_Hair_Eyelashes_Mat" in names


def test_clothing_mesh_glb_body_item_composites_skin_atlas() -> None:
    from io import BytesIO

    from PIL import Image

    from main import clothing_mesh_glb

    response = clothing_mesh_glb(item="F_Armpads_Armoured")
    assert response.status_code == 200
    json_length = struct.unpack_from("<I", response.body, 12)[0]
    gltf = json.loads(response.body[20 : 20 + json_length].decode("utf-8"))
    base_index = gltf["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"]["index"]
    view = gltf["images"][base_index]["bufferView"]
    binary = response.body[20 + json_length + 8 :]
    offset = gltf["bufferViews"][view].get("byteOffset", 0)
    length = gltf["bufferViews"][view]["byteLength"]
    with Image.open(BytesIO(binary[offset : offset + length])) as texture:
        assert texture.size == (1024, 1024)


def test_clothing_mesh_glb_arm_pads_use_a_separate_overlay_material() -> None:
    from main import clothing_mesh_glb

    response = clothing_mesh_glb(item="F_Armpads_Armoured")
    assert response.status_code == 200
    json_length = struct.unpack_from("<I", response.body, 12)[0]
    gltf = json.loads(response.body[20 : 20 + json_length].decode("utf-8"))
    names = [material["name"] for material in gltf["materials"]]
    assert "__clothing_layer" in names
    assert len(gltf["meshes"][0]["primitives"]) == 4


def test_clothing_mesh_glb_layer_only_item_uses_canonical_body_mesh() -> None:
    from main import clothing_mesh_glb

    response = clothing_mesh_glb(item="F_Top_Shortsleeved_VNeck_Tee")
    assert response.status_code == 200
    assert response.body[:4] == b"glTF"


ANIM_MESH = "F_Body_Base/F_Body_Base/SkeletalMesh3/F_Body_Base.psk"


def test_animation_catalog_lists_real_animsets() -> None:
    from main import catalog_animations

    response = catalog_animations()
    assert response["count"] > 0
    first = response["animsets"][0]
    assert "relpath" in first and first["relpath"].endswith(".psa")
    assert first["bone_count"] > 0
    assert len(first["clips"]) > 0
    clip = first["clips"][0]
    assert clip["frames"] > 0 and clip["duration"] > 0


def test_animation_glb_is_skinned_and_animated() -> None:
    from main import animation_glb, catalog_animations

    catalog = catalog_animations()["animsets"]
    animset = next(
        (entry for entry in catalog if entry["bone_count"] >= 100 and entry["clips"]),
        catalog[0],
    )
    clip = animset["clips"][0]
    response = animation_glb(mesh=ANIM_MESH, animset=animset["relpath"], clip=clip["name"])
    assert response.status_code == 200
    assert response.body[:4] == b"glTF"
    json_length = struct.unpack_from("<I", response.body, 12)[0]
    gltf = json.loads(response.body[20 : 20 + json_length].decode("utf-8"))
    assert len(gltf["skins"]) == 1
    assert len(gltf["animations"]) == 1
    assert gltf["animations"][0]["name"] == clip["name"]
    # JOINTS_0/WEIGHTS_0 must exist on the primitive
    attributes = gltf["meshes"][0]["primitives"][0]["attributes"]
    assert "JOINTS_0" in attributes and "WEIGHTS_0" in attributes
    # skin joints == REFSKELT bones of the body
    assert len(gltf["skins"][0]["joints"]) >= 100


def test_catalog_characters_lists_bodies_and_clothing() -> None:
    from main import catalog_characters

    response = catalog_characters()
    assert response["count"] > 0
    characters = response["characters"]
    categories = {character["category"] for character in characters}
    assert "body" in categories
    assert "clothing" in categories
    for character in characters:
        assert character["relpath"].endswith(".psk")
        assert character["psk"]
        assert "/" in character["relpath"]
        assert character["id"]
    # The body base the Animations tab defaults to must be in the catalog.
    assert any(
        character["category"] == "body" and character["name"] == "F_Body_Base"
        for character in characters
    )


def test_animation_glb_with_clothing_item_mesh() -> None:
    from main import animation_glb, catalog_animations, catalog_characters

    catalog = catalog_animations()["animsets"]
    animset = next(
        (entry for entry in catalog if entry["bone_count"] >= 100 and entry["clips"]),
        catalog[0],
    )
    clip = animset["clips"][0]
    characters = catalog_characters()["characters"]
    clothing = next(
        (entry for entry in characters if entry["category"] == "clothing"),
        None,
    )
    assert clothing is not None
    response = animation_glb(
        mesh=clothing["relpath"], animset=animset["relpath"], clip=clip["name"])
    assert response.status_code == 200
    assert response.body[:4] == b"glTF"
    json_length = struct.unpack_from("<I", response.body, 12)[0]
    gltf = json.loads(response.body[20 : 20 + json_length].decode("utf-8"))
    assert len(gltf["skins"]) == 1
    assert len(gltf["animations"]) == 1
    assert len(gltf["skins"][0]["joints"]) >= 100


def test_vehicle_animation_glb_is_skinned_with_wheel_spin() -> None:
    from main import catalog_vehicles, vehicle_animation_glb

    catalog = catalog_vehicles()["vehicles"]
    assert len(catalog) > 0
    vehicle = catalog[0]
    response = vehicle_animation_glb(path=vehicle["primary"])
    assert response.status_code == 200
    assert response.body[:4] == b"glTF"
    json_length = struct.unpack_from("<I", response.body, 12)[0]
    gltf = json.loads(response.body[20 : 20 + json_length].decode("utf-8"))
    assert len(gltf["skins"]) == 1
    assert len(gltf["animations"]) == 1
    animation = gltf["animations"][0]
    assert animation["name"] == "Wheel Spin"
    attributes = gltf["meshes"][0]["primitives"][0]["attributes"]
    assert "JOINTS_0" in attributes and "WEIGHTS_0" in attributes
    # The wheel-spin clip must target the wheel bones (WheelDamage/Main),
    # not just the body root.
    node_names = [node.get("name", "") for node in gltf["nodes"]]
    targeted_nodes = {channel["target"]["node"] for channel in animation["channels"]}
    targeted_names = {node_names[node] for node in targeted_nodes}
    assert any("Wheel" in name for name in targeted_names)


def test_animation_clips_rebased_onto_reconstructed_bind() -> None:
    """Frame 0 of every rebased clip must equal the reconstructed bind locals.

    Character PSK REFSKELTs are the animation reference skeleton, 50-160cm from
    the geometry; playing raw keys stretches the mesh into ribbons. The bind is
    re-derived from the weights and the keys are re-anchored so frame 0 rests on
    the authored geometry (skinned == rest) while later frames keep the clip's
    relative motion. This test locks that invariant so the ribbon bug cannot
    silently recur.
    """
    import main
    from psk import (
        parse_psk_file,
        parse_skeleton,
        parse_weights,
        skin_weights_by_point,
        reconstruct_bind_skeleton,
    )
    from psa import parse_psa_file
    from animations import (
        resolve_animset,
        align_clips_to_skeleton,
        rebase_clips_to_skeleton,
    )

    mesh_path = main.CHARACTERS_BULK / ANIM_MESH
    mesh = parse_psk_file(mesh_path)
    raw = parse_skeleton(mesh_path.read_bytes())
    weights = skin_weights_by_point(parse_weights(mesh_path.read_bytes()), len(mesh.points))
    bind = reconstruct_bind_skeleton(mesh.points, weights, raw)

    catalog = main.catalog_animations()["animsets"]
    locomotion = next(
        (entry for entry in catalog if "locomotion" in entry["display"].casefold()),
        None,
    )
    assert locomotion is not None
    psa = parse_psa_file(resolve_animset(main.ANIMATIONS_ROOT, locomotion["relpath"]))
    clips = align_clips_to_skeleton(psa, [bone.name for bone in bind])
    clip = next(
        (c for c in clips if "walk" in c["name"].casefold()),
        clips[0],
    )
    rebased = rebase_clips_to_skeleton([clip], bind)[0]

    def close(a, b):
        return all(abs(x - y) < 1e-4 for x, y in zip(a, b))

    moving = 0
    for i, bone in enumerate(bind):
        track = rebased["tracks"][i]
        if not track:
            continue
        # frame 0 sits exactly on the bind local (skinned == rest at start)
        assert close(track[0].position, bone.position), f"bone {bone.name} pos"
        assert close(track[0].quat, bone.quat), f"bone {bone.name} quat"
        # later frames keep the clip's relative motion
        for key in track[1:]:
            if not close(key.quat, track[0].quat):
                moving += 1
                break
    assert moving >= 10, "rebased clip lost all motion"


def _glb_parts(body: bytes) -> tuple[dict, bytes]:
    """Split a .glb response into (gltf json, binary chunk)."""
    json_length = struct.unpack_from("<I", body, 12)[0]
    gltf = json.loads(body[20 : 20 + json_length].decode("utf-8"))
    binary = body[20 + json_length + 8 :]
    return gltf, binary


def _read_accessor(gltf: dict, binary: bytes, index: int, fmt: str, per_element: int) -> list[tuple]:
    """Decode one accessor's raw component values from the GLB binary chunk."""
    accessor = gltf["accessors"][index]
    view = gltf["bufferViews"][accessor["bufferView"]]
    offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    return [
        struct.unpack_from("<" + fmt, binary, offset + i * per_element)
        for i in range(accessor["count"])
    ]


def _skinned_at_key(gltf: dict, binary: bytes, key_index: int = 0) -> tuple[list, list]:
    """Skin every vertex at one animation key; returns (skinned, rest) tuples.

    Skinning mirrors what three.js does per frame: skinned = sum(w * world * IBM * pos).
    The pose is the key_index-th key of every animated channel (0 = frame 0).
    The rebase anchors key 0 to the bind, so a fixed pipeline lands on the
    authored geometry; the old raw-keys pipeline sampled the animation-reference
    rig pose (50-160cm off), the exact distortion this skinning guards.
    Samplers are LINEAR with input times starting at 0, so reading the raw key
    is exact. Unweighted vertices are returned at their rest positions.
    """
    primitive = gltf["meshes"][0]["primitives"][0]
    attrs = primitive["attributes"]
    rest = _read_accessor(gltf, binary, attrs["POSITION"], "fff", 12)
    joints = _read_accessor(gltf, binary, attrs["JOINTS_0"], "HHHH", 8)
    weights = _read_accessor(gltf, binary, attrs["WEIGHTS_0"], "ffff", 16)

    skin = gltf["skins"][0]
    joint_nodes = skin["joints"]
    ibm_rows = _read_accessor(gltf, binary, skin["inverseBindMatrices"], "f" * 16, 64)

    # pose TRS per node: key_index-th key of each animated channel, else the
    # static bind TRS (untracked bones, root/mesh nodes).
    frame_trs: dict[int, tuple] = {}
    for node_index, node in enumerate(gltf["nodes"]):
        frame_trs[node_index] = (
            node.get("translation", (0.0, 0.0, 0.0)),
            node.get("rotation", (0.0, 0.0, 0.0, 1.0)),
        )
    animation = gltf["animations"][0]
    for channel in animation["channels"]:
        node_index = channel["target"]["node"]
        path = channel["target"]["path"]
        sampler = animation["samplers"][channel["sampler"]]
        if path == "translation":
            values = _read_accessor(gltf, binary, sampler["output"], "fff", 12)[key_index]
            _, rotation = frame_trs[node_index]
            frame_trs[node_index] = (values, rotation)
        elif path == "rotation":
            values = _read_accessor(gltf, binary, sampler["output"], "ffff", 16)[key_index]
            translation, _ = frame_trs[node_index]
            frame_trs[node_index] = (translation, values)

    # node world matrices: world = parent_world * local
    children_of: dict[int, int] = {}
    for node_index, node in enumerate(gltf["nodes"]):
        for child in node.get("children", []):
            children_of[child] = node_index

    def local_matrix(node_index: int) -> list[float]:
        translation, quat = frame_trs[node_index]
        qx, qy, qz, qw = quat
        xx, yy, zz = qx * qx, qy * qy, qz * qz
        xy, xz, yz = qx * qy, qx * qz, qy * qz
        wx, wy, wz = qw * qx, qw * qy, qw * qz
        m = [
            1 - 2 * (yy + zz), 2 * (xy + wz), 2 * (xz - wy), 0.0,
            2 * (xy - wz), 1 - 2 * (xx + zz), 2 * (yz + wx), 0.0,
            2 * (xz + wy), 2 * (yz - wx), 1 - 2 * (xx + yy), 0.0,
        ]
        m.extend(translation)
        m.append(1.0)
        return m

    def mat_mul(a: list[float], b: list[float]) -> list[float]:
        return [
            sum(a[r + 4 * k] * b[k + 4 * c] for k in range(4))
            for c in range(4)
            for r in range(4)
        ]

    def mat_vec(m: list[float], v: tuple) -> tuple:
        return (
            m[0] * v[0] + m[4] * v[1] + m[8] * v[2] + m[12],
            m[1] * v[0] + m[5] * v[1] + m[9] * v[2] + m[13],
            m[2] * v[0] + m[6] * v[1] + m[10] * v[2] + m[14],
        )

    identity = [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0]
    node_count = len(gltf["nodes"])
    world = [None] * node_count
    resolved = [False] * node_count
    for node_index in range(node_count):
        if node_index not in children_of:
            world[node_index] = identity
            resolved[node_index] = True
    remaining = [i for i in range(node_count) if not resolved[i]]
    while remaining:
        progressed = False
        still = []
        for node_index in remaining:
            parent = children_of[node_index]
            if not resolved[parent]:
                still.append(node_index)
                continue
            world[node_index] = mat_mul(world[parent], local_matrix(node_index))
            resolved[node_index] = True
            progressed = True
        if not progressed:
            break
        remaining = still

    joint_world_ibm: list[list[float]] = [
        mat_mul(world[joint_node], list(ibm))
        for joint_node, ibm in zip(joint_nodes, ibm_rows)
    ]

    skinned: list = []
    for vertex_index, rp in enumerate(rest):
        total = sum(weights[vertex_index])
        if total < 1e-3:
            skinned.append(rp)
            continue
        out = [0.0, 0.0, 0.0]
        for influence in range(4):
            weight = weights[vertex_index][influence] / total
            if weight == 0.0:
                continue
            transformed = mat_vec(joint_world_ibm[joints[vertex_index][influence]], rp)
            out[0] += weight * transformed[0]
            out[1] += weight * transformed[1]
            out[2] += weight * transformed[2]
        skinned.append(tuple(out))
    return skinned, rest


def _frame0_skinning_displacement(gltf: dict, binary: bytes) -> list[float]:
    """Per-vertex displacement (cm) between frame-0 skinning and rest geometry."""
    skinned, rest = _skinned_at_key(gltf, binary, 0)
    return [
        max(abs(s[i] - r[i]) for i in range(3))
        for s, r in zip(skinned, rest)
    ]


def test_animation_glb_frame0_skinned_positions_match_rest_geometry() -> None:
    """Skinning the served animation GLB at frame 0 reproduces the rest mesh.

    Regression guard for the ribbon bug: composing clip keys through the raw
    animation-reference skeleton moved vertices hundreds of cm off the authored
    geometry (edges stretched up to 7.9x). The fix re-derives a mesh-aligned
    bind skeleton from the weights and rebases clip keys so frame 0 rests on
    the bind. This test reads the exact GLB the client renders (POSITION,
    JOINTS_0/WEIGHTS_0, inverse bind matrices, node TRS), skins every vertex
    at frame 0, and asserts the result matches the rest geometry to sub-cm.
    """
    import main

    catalog = main.catalog_animations()["animsets"]
    locomotion = next(
        (entry for entry in catalog if "locomotion" in entry["display"].casefold()),
        None,
    )
    assert locomotion is not None
    clip = next(
        (c for c in locomotion["clips"] if "walk" in c["name"].casefold()),
        locomotion["clips"][0],
    )
    response = main.animation_glb(mesh=ANIM_MESH, animset=locomotion["relpath"], clip=clip["name"])
    assert response.status_code == 200
    gltf, binary = _glb_parts(response.body)

    displacements = _frame0_skinning_displacement(gltf, binary)
    assert displacements, "no weighted vertices to skin"
    max_error = max(displacements)
    median_error = sorted(displacements)[len(displacements) // 2]
    assert max_error < 0.5, (
        f"frame-0 skinning diverges from rest geometry: "
        f"max={max_error:.3f}cm median={median_error:.4f}cm "
        f"over {len(displacements)} vertices"
    )


def test_animation_glb_frame0_skinned_across_body_and_clothing_meshes() -> None:
    """The frame-0 invariant holds across body types and clothing items.

    The Characters lane serves every body/clothing/crowd mesh through the same
    animation_glb endpoint, so the ribbon fix must hold for all of them, not
    just the F_Body_Base regression fixture. Exercises female + male bodies and
    a backpack accessory; each must skin exactly onto its rest geometry at
    frame 0 (displacement < 0.5cm).
    """
    import main

    characters = main.catalog_characters()["characters"]

    def find(predicate):
        return next((character for character in characters if predicate(character)), None)

    targets = [
        find(lambda c: c["name"] == "F_Body_Base"),
        find(lambda c: c["name"] == "M_Body_Base"),
        find(lambda c: c["category"] == "clothing" and "backpack" in c["name"].casefold()),
    ]
    targets = [target for target in targets if target is not None]
    assert len(targets) >= 2, "need at least two catalog targets (bodies/backpack)"

    animset = next(
        (entry for entry in main.catalog_animations()["animsets"]
         if "locomotion" in entry["display"].casefold()),
        None,
    )
    assert animset is not None
    clip = next(
        (c for c in animset["clips"] if "walk" in c["name"].casefold()),
        animset["clips"][0],
    )
    for target in targets:
        response = main.animation_glb(
            mesh=target["relpath"], animset=animset["relpath"], clip=clip["name"])
        assert response.status_code == 200, f"{target['name']}: {response.body[:120]!r}"
        gltf, binary = _glb_parts(response.body)
        displacements = _frame0_skinning_displacement(gltf, binary)
        assert displacements, f"{target['name']}: no weighted vertices"
        assert max(displacements) < 0.5, (
            f"{target['name']}: frame-0 skinning diverges {max(displacements):.3f}cm"
        )


def test_prop_animation_glb_frame0_skinned_positions_match_rest_geometry() -> None:
    """Every animated prop skins onto its rest geometry at frame 0.

    Prop PSA keys are authored against the animation reference rig, not the
    mesh bind (measured 9-230cm frame-0 displacement before the fix). The prop
    endpoint now rebases keys onto the bind, reconstructing the bind first for
    character-rig props (>=100 bones) while mechanical props keep their
    authored hinge REFSKELT. Every indexed prop must rest exactly on its
    authored geometry at frame 0.
    """
    import main
    from inventory import _prop_anim_index

    md = main.REPO_ROOT / "Content" / "Extracted" / "MaterialDatabase"
    props = _prop_anim_index(md)
    assert props, "no animated props indexed"
    for package, relpath in sorted(props.items()):

        response = main.prop_animation_glb(path=relpath)
        assert response.status_code == 200, f"{package}: {response.body[:120]!r}"
        gltf, binary = _glb_parts(response.body)
        if not gltf.get("animations"):
            continue  # no animated channels: mesh renders at bind == rest
        displacements = _frame0_skinning_displacement(gltf, binary)
        assert displacements, f"{package}: no weighted vertices"
        assert max(displacements) < 0.5, (
            f"{package}: frame-0 skinning diverges {max(displacements):.3f}cm"
        )

def test_prop_breakable_doors_swings_on_its_hinge_edge() -> None:
    """BreakableDoors opens on its hinge edge, not flattened in its plane.

    The clip rotates each door ~90deg around local Z, but the exported REFSKELT
    carried a 90deg-around-Y orientation that mapped the swing onto the door's
    normal (world X), flipping the panels flat in their own plane (hinge edge
    flew 215cm, z-extent 2.5x at full open). The mechanical-prop branch zeroes
    joint orientation, so the clip's local-Z swing is upright in viewer space:
    the hinge edge stays put (5.6cm) and the panel keeps its height (z ratio
    1.09) while the free edge travels 140cm.
    """
    import main
    from inventory import _prop_anim_index

    md = main.REPO_ROOT / "Content" / "Extracted" / "MaterialDatabase"
    rel = _prop_anim_index(md).get("breakabledoors")
    assert rel, "BreakableDoors not indexed"

    response = main.prop_animation_glb(path=rel)
    assert response.status_code == 200, response.body[:120]
    gltf, binary = _glb_parts(response.body)
    assert gltf.get("animations"), "door clip missing"

    skin = gltf["skins"][0]
    door_joints = [
        i for i, node in enumerate(skin["joints"])
        if gltf["nodes"][node].get("name", "").startswith("Bone:Door")
    ]
    assert door_joints, "no Bone:Door joints in skin"

    animation = gltf["animations"][0]
    counts = [gltf["accessors"][s["output"]]["count"] for s in animation["samplers"]]
    assert min(counts) >= 2, "linear sampler needs >= 2 keys"
    last_key = min(counts) - 1
    skinned, rest = _skinned_at_key(gltf, binary, last_key)

    attributes = gltf["meshes"][0]["primitives"][0]["attributes"]
    joints = _read_accessor(gltf, binary, attributes["JOINTS_0"], "HHHH", 8)
    weights = _read_accessor(gltf, binary, attributes["WEIGHTS_0"], "ffff", 16)

    def max_joint(vertex: int) -> int:
        slot = max(range(4), key=lambda k: weights[vertex][k])
        return joints[vertex][slot]

    panel = [i for i in range(len(rest)) if max_joint(i) in door_joints]
    assert len(panel) > 100, f"expected door-weighted vertices, got {len(panel)}"

    def displacement(a, b):
        return ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2) ** 0.5

    max_z = max(abs(rest[i][2]) for i in panel)
    hinge = [i for i in panel if abs(rest[i][2]) > 0.9 * max_z]
    assert hinge, "hinge edge not found"
    free_move = max(displacement(skinned[i], rest[i]) for i in panel)
    assert free_move > 50.0, (
        f"door free edge only travels {free_move:.1f}cm: first clip does not "
        f"open the door (expected ~140cm), test would be vacuous"
    )
    hinge_move = max(displacement(skinned[i], rest[i]) for i in hinge)
    assert hinge_move < 20.0, (
        f"door hinge travels {hinge_move:.1f}cm at full open: swing axis is "
        f"flattened, expected < 20cm (was 215cm pre-fix)"
    )
    open_z = max(abs(skinned[i][2]) for i in panel)
    assert open_z < 1.6 * max_z, (
        f"door panel z-extent grows {open_z / max_z:.2f}x: panel lies flat "
        f"instead of swinging upright (was 2.5x pre-fix)"
    )

def _grid_panel(thin: float, span_y: float, half_z: float) -> list:
    """A flat rectangular leaf: thin in X, wide in Y, tall in Z."""
    points = []
    for x in (-thin, thin):
        for y in range(-int(span_y), int(span_y) + 1, 40):
            for z in (-half_z, half_z):
                points.append((float(x), float(y), float(z)))
    return points


def _swing_clip(skeleton_size: int, bone_index: int, axis: tuple) -> list:
    """One 2-frame clip swinging the bone 90 deg around `axis` (local frame)."""
    from psa import PsaKey

    scale = 0.7071067811865476  # sin/cos of 45 deg
    if axis == (0.0, 0.0, 1.0):
        quat = (0.0, 0.0, scale, scale)
    elif axis == (0.0, 1.0, 0.0):
        quat = (0.0, scale, 0.0, scale)
    else:
        quat = (scale, 0.0, 0.0, scale)
    tracks = [None] * skeleton_size
    tracks[bone_index] = [
        PsaKey((0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0)),
        PsaKey((0.0, 0.0, 0.0), quat),
    ]
    return [{"name": "swing", "rate": 30, "num_frames": 2, "tracks": tracks}]


def test_mechanical_swing_detector_flags_a_flattened_leaf_bind() -> None:
    """A 90-deg-rotated REFSKELT that twists the swing axis off the leaf's
    principal frame is flagged (the BreakableDoors signature), so the
    endpoint zeroes it."""
    import main as m
    from psk import PskBone, PskMesh

    panel = _grid_panel(4.0, 100.0, 120.0)
    mesh = PskMesh(points=panel)
    skeleton = [
        PskBone("Root", -1, (0.0, 0.0, 0.0, 1.0), (0.0, 0.0, 0.0)),
        # 90 deg around Y: local Z maps to world X = the panel's thin axis
        PskBone("Leaf", 0, (0.0, 0.7071067811865476, 0.0, 0.7071067811865476), (0.0, 97.0, 0.0)),
    ]
    weights = {i: [(1, 1.0)] for i in range(len(panel))}
    clips = _swing_clip(2, 1, (0.0, 0.0, 1.0))
    assert m._mechanical_swing_flattened(mesh, skeleton, weights, clips) is True


def test_mechanical_swing_detector_keeps_a_meaningful_in_plane_bind() -> None:
    """Identity REFSKELT on the same leaf: the swing stays in the leaf's plane
    (the hinge is upright), so the raw orientation is kept."""
    import main as m
    from psk import PskBone, PskMesh

    panel = _grid_panel(4.0, 100.0, 120.0)
    mesh = PskMesh(points=panel)
    skeleton = [
        PskBone("Root", -1, (0.0, 0.0, 0.0, 1.0), (0.0, 0.0, 0.0)),
        PskBone("Leaf", 0, (0.0, 0.0, 0.0, 1.0), (0.0, 97.0, 0.0)),
    ]
    weights = {i: [(1, 1.0)] for i in range(len(panel))}
    clips = _swing_clip(2, 1, (0.0, 0.0, 1.0))
    assert m._mechanical_swing_flattened(mesh, skeleton, weights, clips) is False


def test_mechanical_swing_detector_keeps_an_axle_driven_hub() -> None:
    """A wheel-like hub spinning on its own axle keeps the raw bind: the swing
    axis is the axle, aligned with the geometry's thin principal axis."""
    import main as m
    from psk import PskBone, PskMesh

    points = []
    for x in range(-50, 51, 25):
        for y in range(-50, 51, 25):
            for z in (-2.0, 2.0):
                points.append((float(x), float(y), z))
    mesh = PskMesh(points=points)
    skeleton = [
        PskBone("Root", -1, (0.0, 0.0, 0.0, 1.0), (0.0, 0.0, 0.0)),
        PskBone("Hub", 0, (0.0, 0.0, 0.0, 1.0), (0.0, 0.0, 0.0)),
    ]
    weights = {i: [(1, 1.0)] for i in range(len(points))}
    clips = _swing_clip(2, 1, (0.0, 0.0, 1.0))
    assert m._mechanical_swing_flattened(mesh, skeleton, weights, clips) is False


def test_mechanical_swing_detector_real_props_flags_only_the_door() -> None:
    """Across the indexed mechanical props, only BreakableDoors trips the
    detector; a future prop that trips it needs a human decision, so this
    sweep acts as the guard rail."""
    import main as m
    from animations import align_clips_to_skeleton, load_animset_cached
    from inventory import _prop_anim_index, find_prop_mesh_dir, pick_prop_mesh
    from psk import parse_psk_file, parse_skeleton, parse_weights, skin_weights_by_point

    md = m.REPO_ROOT / "Content" / "Extracted" / "MaterialDatabase"
    index = _prop_anim_index(md)
    results = {}
    for package, rel in sorted(index.items()):
        psa_path = md / rel
        mesh_dir = find_prop_mesh_dir(psa_path)
        mesh_path = pick_prop_mesh(mesh_dir, psa_path)
        if mesh_path is None:
            continue
        mesh = parse_psk_file(mesh_path)
        skeleton = parse_skeleton(mesh_path.read_bytes())
        if len(skeleton) >= 100:
            continue  # character-rig props use the reconstruction branch
        weights = skin_weights_by_point(parse_weights(mesh_path.read_bytes()), len(mesh.points))
        animset = load_animset_cached(psa_path)
        clips = align_clips_to_skeleton(animset, [bone.name for bone in skeleton])[:16]
        results[package] = m._mechanical_swing_flattened(mesh, skeleton, weights, clips)

    assert results, "no mechanical props indexed"
    flagged = [k for k, v in results.items() if v]
    assert flagged == ["breakabledoors"], (
        f"detector flagged {flagged}, expected only ['breakabledoors']; "
        f"full map: {results}"
    )

def test_mechanical_swing_detector_keeps_a_tilted_hinge() -> None:
    """A hinge yawed 30 deg swinging a leaf around its own height axis keeps
    the raw bind: the composed bind+swing stays on the leaf's principal
    frame (Z), so the tilt is meaningful and must not be zeroed (angled
    mounts, tilted signs)."""
    import main as m
    from psk import PskBone, PskMesh

    panel = _grid_panel(4.0, 100.0, 120.0)
    mesh = PskMesh(points=panel)
    # 30 deg around local Z: the hinge is yawed but the swing stays in-plane.
    skeleton = [
        PskBone("Root", -1, (0.0, 0.0, 0.0, 1.0), (0.0, 0.0, 0.0)),
        PskBone("Leaf", 0, (0.0, 0.0, 0.25881904510252074, 0.9659258262890683), (0.0, 97.0, 0.0)),
    ]
    weights = {i: [(1, 1.0)] for i in range(len(panel))}
    clips = _swing_clip(2, 1, (0.0, 0.0, 1.0))
    assert m._mechanical_swing_flattened(mesh, skeleton, weights, clips) is False
