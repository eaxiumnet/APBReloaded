"""Convert a parsed :class:`psk.PskMesh` into a binary glTF 2.0 (.glb).

glTF vertices bundle position+UV, so each ActorX *wedge* becomes one glTF vertex
(its position comes from the point it references, its UV from the wedge itself).
Faces are grouped by material index into separate primitives that share the one
POSITION/TEXCOORD_0 vertex buffer.

Coordinate space: Unreal is Z-up / left-handed, glTF is Y-up / right-handed.
We map (x, y, z) -> (x, z, -y) and reverse triangle winding to preserve facing.

Texture images are converted to embedded PNG data with Pillow.
"""

from __future__ import annotations

import json
import struct
from array import array
from io import BytesIO
from pathlib import Path

from PIL import Image

from psk import PskMesh, bone_world_transforms

_GLB_MAGIC = 0x46546C67  # 'glTF'
_CHUNK_JSON = 0x4E4F534A  # 'JSON'
_CHUNK_BIN = 0x004E4942  # 'BIN\0'

# glTF accessor component types
_FLOAT = 5126
_UINT32 = 5125
# bufferView targets
_ARRAY_BUFFER = 34962
_ELEMENT_ARRAY_BUFFER = 34963


def tga_to_png_bytes(path: Path, alpha_path: Path | None = None) -> bytes:
    with Image.open(path) as image:
        mode = "RGBA" if "A" in image.getbands() or "transparency" in image.info else "RGB"
        converted = image.convert(mode)
        if alpha_path is not None and alpha_path.is_file():
            with Image.open(alpha_path) as alpha_source:
                alpha = alpha_source.convert("L").resize(converted.size, Image.Resampling.BILINEAR)
            converted = converted.convert("RGBA")
            converted.putalpha(alpha)
        with converted, BytesIO() as output:
            converted.save(output, format="PNG")
            return output.getvalue()


def _baked_normals(mesh: PskMesh) -> array:
    sums = [(0.0, 0.0, 0.0) for _ in mesh.points]
    for w0, w1, w2, _material in mesh.faces:
        if any(w < 0 or w >= len(mesh.wedges) for w in (w0, w1, w2)):
            continue
        pidx0 = mesh.wedges[w0][0]
        pidx1 = mesh.wedges[w2][0]
        pidx2 = mesh.wedges[w1][0]
        if any(pidx < 0 or pidx >= len(mesh.points) for pidx in (pidx0, pidx1, pidx2)):
            continue
        points = (mesh.points[pidx0], mesh.points[pidx1], mesh.points[pidx2])
        p0, p1, p2 = ((x, z, -y) for x, y, z in points)
        ax, ay, az = (p1[i] - p0[i] for i in range(3))
        bx, by, bz = (p2[i] - p0[i] for i in range(3))
        face_normal = (ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx)
        for pidx in (pidx0, pidx1, pidx2):
            old = sums[pidx]
            sums[pidx] = tuple(old[i] + face_normal[i] for i in range(3))

    normals = array("f")
    for wedge in mesh.wedges:
        pidx = wedge[0]
        nx, ny, nz = sums[pidx] if 0 <= pidx < len(sums) else (0.0, 1.0, 0.0)
        length = (nx * nx + ny * ny + nz * nz) ** 0.5
        if length == 0.0:
            nx, ny, nz = 0.0, 1.0, 0.0
        else:
            nx, ny, nz = nx / length, ny / length, nz / length
        normals.extend((nx, ny, nz))
    return normals


def _pad4(buf: bytearray, fill: int = 0) -> None:
    while len(buf) % 4:
        buf.append(fill)


def _build_vertex_arrays(mesh: PskMesh):
    """One glTF vertex per wedge: positions, texcoords, baked normals, bounds."""
    positions = array("f")
    texcoords = array("f")
    normals = _baked_normals(mesh)
    minp = [float("inf")] * 3
    maxp = [float("-inf")] * 3
    for pidx, u, v, _m in mesh.wedges:
        px, py, pz = mesh.points[pidx] if pidx < len(mesh.points) else (0.0, 0.0, 0.0)
        # UE (x,y,z) -> glTF (x, z, -y)
        gx, gy, gz = px, pz, -py
        positions.extend((gx, gy, gz))
        texcoords.extend((u, v))
        for i, c in enumerate((gx, gy, gz)):
            if c < minp[i]:
                minp[i] = c
            if c > maxp[i]:
                maxp[i] = c
    vertex_count = len(mesh.wedges)
    if vertex_count == 0:
        minp = [0.0, 0.0, 0.0]
        maxp = [0.0, 0.0, 0.0]
    return positions, texcoords, normals, minp, maxp, vertex_count


def _group_faces_by_material(mesh: PskMesh, mats: list[str]) -> dict[int, array]:
    """Reverse-wind faces grouped by material index (one index array per primitive)."""
    by_mat: dict[int, array] = {}
    for w0, w1, w2, midx in mesh.faces:
        m = midx if 0 <= midx < len(mats) else 0
        # reverse winding (w0,w2,w1) because we flipped an axis
        by_mat.setdefault(m, array("I")).extend((w0, w2, w1))
    if not by_mat:  # no faces -> nothing to draw, still emit a valid (empty) primitive set
        by_mat = {0: array("I")}
    return by_mat


class _GlbAssembler:
    """Shared buffer/accessor assembly for both static and skinned exporters."""

    def __init__(self) -> None:
        self.bin_buf = bytearray()
        self.buffer_views: list[dict] = []
        self.accessors: list[dict] = []
        self.images: list[dict] = []
        self.textures_json: list[dict] = []
        self.samplers: list[dict] = []
        self.texture_indices: dict[tuple[str, str], int] = {}

    def add_view(self, data: bytes, target: int | None = None) -> int:
        _pad4(self.bin_buf)
        offset = len(self.bin_buf)
        self.bin_buf.extend(data)
        view = {"buffer": 0, "byteOffset": offset, "byteLength": len(data)}
        if target is not None:
            view["target"] = target
        self.buffer_views.append(view)
        return len(self.buffer_views) - 1

    def add_accessor(self, accessor: dict) -> int:
        index = len(self.accessors)
        self.accessors.append(accessor)
        return index

    def add_float_accessor(self, data: array, count: int, type_: str, target: int | None = None,
                           min_max: tuple[list, list] | None = None) -> int:
        view = self.add_view(data.tobytes(), target)
        accessor = {"bufferView": view, "componentType": _FLOAT, "count": count, "type": type_}
        if min_max is not None:
            accessor["min"], accessor["max"] = min_max
        return self.add_accessor(accessor)

    def embed_textures(self, textures: dict[str, Path] | None,
                       material_textures: dict[str, dict[str, Path]] | None) -> None:
        if not textures and not material_textures:
            return
        texture_sets = [textures or {}]
        texture_sets.extend((material_textures or {}).values())
        for texture_set in texture_sets:
            for key in ("baseColor", "normal", "specular", "emissive"):
                path = texture_set.get(key)
                if path is None or not path.is_file() or (key, str(path)) in self.texture_indices:
                    continue
                alpha_path = texture_set.get("opacity") if key == "baseColor" else None
                png_bytes = tga_to_png_bytes(path, alpha_path=alpha_path)
                image_view = self.add_view(png_bytes)
                self.images.append({"bufferView": image_view, "mimeType": "image/png"})
                sampler_index = len(self.samplers)
                self.samplers.append({
                    "magFilter": 9729,
                    "minFilter": 9729,
                    "wrapS": 10497,
                    "wrapT": 10497,
                })
                self.texture_indices[(key, str(path))] = len(self.textures_json)
                self.textures_json.append({"sampler": sampler_index, "source": len(self.images) - 1})

    def material_texture_index(self, textures: dict[str, Path] | None,
                               material_textures: dict[str, dict[str, Path]] | None,
                               material_name: str, key: str) -> int | None:
        texture_set = (material_textures or {}).get(material_name, textures or {})
        path = texture_set.get(key)
        return self.texture_indices.get((key, str(path))) if path is not None else None

    def build_materials(self, mesh: PskMesh, mats: list[str], by_mat: dict[int, array],
                        textures: dict[str, Path] | None,
                        material_textures: dict[str, dict[str, Path]] | None,
                        material_settings: dict[str, dict] | None,
                        attributes: dict[str, int]) -> tuple[list[dict], list[dict]]:
        """Append index buffers + materials, return (primitives, materials_json)."""
        primitives: list[dict] = []
        materials_json: list[dict] = []

        def setting(material_name: str, key: str, default):
            return (material_settings or {}).get(material_name, {}).get(key, default)

        def name_for(index: int) -> str:
            return mats[index] if 0 <= index < len(mats) else f"material_{index}"

        for prim_i, (m, indices) in enumerate(sorted(by_mat.items())):
            idx_view = self.add_view(indices.tobytes(), _ELEMENT_ARRAY_BUFFER)
            idx_accessor = self.add_accessor({
                "bufferView": idx_view, "componentType": _UINT32,
                "count": len(indices), "type": "SCALAR",
            })
            material_name = name_for(m)
            base_index = self.material_texture_index(textures, material_textures, material_name, "baseColor")
            normal_index = self.material_texture_index(textures, material_textures, material_name, "normal")
            specular_index = self.material_texture_index(textures, material_textures, material_name, "specular")
            emissive_index = self.material_texture_index(textures, material_textures, material_name, "emissive")
            has_opacity = bool((material_textures or {}).get(material_name, textures or {}).get("opacity"))
            has_specular = specular_index is not None
            material = {
                "name": material_name,
                "pbrMetallicRoughness": {
                    "baseColorFactor": setting(material_name, "base_color_factor", [1.0, 1.0, 1.0, 1.0]),
                    "metallicFactor": setting(material_name, "metallic_factor", 0.8 if has_specular else 0.1),
                    "roughnessFactor": setting(material_name, "roughness_factor", 0.3 if has_specular else 0.7),
                },
                "doubleSided": setting(material_name, "double_sided", True),
            }
            if base_index is not None:
                material["pbrMetallicRoughness"]["baseColorTexture"] = {"index": base_index}
            if normal_index is not None:
                material["normalTexture"] = {"index": normal_index}
            if specular_index is not None:
                material["pbrMetallicRoughness"]["metallicRoughnessTexture"] = {"index": specular_index}
            if emissive_index is not None:
                material["emissiveTexture"] = {"index": emissive_index}
                material["emissiveFactor"] = [1.0, 1.0, 1.0]
            alpha_mode = setting(material_name, "alpha_mode", "MASK" if has_opacity else "OPAQUE")
            if alpha_mode != "OPAQUE":
                material["alphaMode"] = alpha_mode
                if alpha_mode == "MASK":
                    material["alphaCutoff"] = setting(material_name, "alpha_cutoff", 0.333)
            primitives.append({
                "attributes": dict(attributes),
                "indices": idx_accessor,
                "material": prim_i,
            })
            materials_json.append(material)
        return primitives, materials_json


def _finish_glb(gltf: dict, bin_buf: bytearray) -> bytes:
    """Serialize the gltf JSON + binary buffer into a .glb container."""
    json_bytes = bytearray(json.dumps(gltf, separators=(",", ":")).encode("utf-8"))
    _pad4(json_bytes, fill=0x20)  # pad JSON chunk with spaces
    _pad4(bin_buf, fill=0x00)
    gltf["buffers"][0]["byteLength"] = len(bin_buf)
    json_bytes = bytearray(json.dumps(gltf, separators=(",", ":")).encode("utf-8"))
    _pad4(json_bytes, fill=0x20)

    total = 12 + 8 + len(json_bytes) + 8 + len(bin_buf)
    out = bytearray()
    out += struct.pack("<III", _GLB_MAGIC, 2, total)
    out += struct.pack("<II", len(json_bytes), _CHUNK_JSON)
    out += json_bytes
    out += struct.pack("<II", len(bin_buf), _CHUNK_BIN)
    out += bin_buf
    return bytes(out)


def mesh_to_glb(
    mesh: PskMesh,
    textures: dict[str, Path] | None = None,
    material_textures: dict[str, dict[str, Path]] | None = None,
    material_settings: dict[str, dict] | None = None,
) -> bytes:
    """Serialize *mesh* to glTF 2.0 binary bytes."""
    positions, texcoords, normals, minp, maxp, vertex_count = _build_vertex_arrays(mesh)
    mats = mesh.materials or ["material_0"]
    by_mat = _group_faces_by_material(mesh, mats)

    assembler = _GlbAssembler()
    assembler.embed_textures(textures, material_textures)
    pos_accessor = assembler.add_float_accessor(
        positions, vertex_count, "VEC3", _ARRAY_BUFFER, (minp, maxp))
    uv_accessor = assembler.add_float_accessor(texcoords, vertex_count, "VEC2", _ARRAY_BUFFER)
    normal_accessor = assembler.add_float_accessor(normals, vertex_count, "VEC3", _ARRAY_BUFFER)

    primitives, materials_json = assembler.build_materials(
        mesh, mats, by_mat, textures, material_textures, material_settings,
        {"POSITION": pos_accessor, "TEXCOORD_0": uv_accessor, "NORMAL": normal_accessor})

    gltf = {
        "asset": {"version": "2.0", "generator": "apb-content-studio psk->glb"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0}],
        "meshes": [{"primitives": primitives}],
        "materials": materials_json,
        "accessors": assembler.accessors,
        "bufferViews": assembler.buffer_views,
        "buffers": [{"byteLength": 0}],
    }
    if assembler.images:
        gltf["images"] = assembler.images
        gltf["textures"] = assembler.textures_json
        gltf["samplers"] = assembler.samplers
    return _finish_glb(gltf, assembler.bin_buf)


def skinned_mesh_to_glb(
    mesh: PskMesh,
    skeleton: list,
    weights: dict[int, list[tuple[int, float]]],
    clips: list[dict],
    textures: dict[str, Path] | None = None,
    material_textures: dict[str, dict[str, Path]] | None = None,
    material_settings: dict[str, dict] | None = None,
    drop_root_translation: bool = False,
) -> bytes:
    """Serialize a skinned mesh with skeleton + animation clips to glTF 2.0.

    *skeleton*: REFSKELT PskBone rows (order == RAWWEIGHTS bone indices).
    *weights*: point index -> top-4 (bone_index, weight) list.
    *clips*: [{"name", "rate", "num_frames", "tracks": per-skeleton-bone list
             (None when the PSA had no matching bone) of PsaKey(key0..N-1)}].
    """
    positions, texcoords, normals, minp, maxp, vertex_count = _build_vertex_arrays(mesh)
    mats = mesh.materials or ["material_0"]
    by_mat = _group_faces_by_material(mesh, mats)

    # --- per-wedge joints + weights (top-4, normalized) ---
    joints = array("H")
    weights_arr = array("f")
    for pidx, _u, _v, _m in mesh.wedges:
        influences = weights.get(pidx, [])
        total = sum(w for _b, w in influences) or 1.0
        for i in range(4):
            if i < len(influences):
                bone_index, weight = influences[i]
                joints.append(bone_index & 0xFFFF)
                weights_arr.append(weight / total)
            else:
                joints.append(0)
                weights_arr.append(0.0)

    assembler = _GlbAssembler()
    assembler.embed_textures(textures, material_textures)
    pos_accessor = assembler.add_float_accessor(
        positions, vertex_count, "VEC3", _ARRAY_BUFFER, (minp, maxp))
    uv_accessor = assembler.add_float_accessor(texcoords, vertex_count, "VEC2", _ARRAY_BUFFER)
    normal_accessor = assembler.add_float_accessor(normals, vertex_count, "VEC3", _ARRAY_BUFFER)
    joints_view = assembler.add_view(joints.tobytes(), _ARRAY_BUFFER)
    joints_accessor = assembler.add_accessor({
        "bufferView": joints_view, "componentType": 5123, "count": vertex_count, "type": "VEC4"})
    weights_accessor = assembler.add_float_accessor(weights_arr, vertex_count, "VEC4", _ARRAY_BUFFER)

    primitives, materials_json = assembler.build_materials(
        mesh, mats, by_mat, textures, material_textures, material_settings,
        {"POSITION": pos_accessor, "TEXCOORD_0": uv_accessor, "NORMAL": normal_accessor,
         "JOINTS_0": joints_accessor, "WEIGHTS_0": weights_accessor})

    # --- node hierarchy from REFSKELT (bind pose, parent-relative) ---
    # UE (x,y,z)->glTF (x,z,-y) for positions; quats (x,y,z,w)->(x,z,-y,w).
    nodes: list[dict] = [{"children": []}, {"mesh": 0}]  # 0 = root, 1 = mesh node
    children_of: dict[int, list[int]] = {}
    bone_node = [2 + i for i in range(len(skeleton))]
    root_children: list[int] = []
    for i, bone in enumerate(skeleton):
        nodes.append({
            "name": bone.name,
            "translation": [bone.position[0], bone.position[2], -bone.position[1]],
            "rotation": [bone.quat[0], bone.quat[2], -bone.quat[1], bone.quat[3]],
        })
        parent = bone.parent
        if parent < 0 or parent >= len(skeleton) or parent == i:
            root_children.append(bone_node[i])
        else:
            children_of.setdefault(parent, []).append(bone_node[i])
    nodes[0]["children"] = [1] + root_children
    for parent, children in children_of.items():
        nodes[bone_node[parent]].setdefault("children", []).extend(children)

    # --- skin: joints + inverse bind matrices (world at bind, inverted) ---
    def quat_to_mat(q: tuple[float, float, float, float]) -> list[float]:
        """Unit quaternion -> 4x4 column-major matrix (translation set later)."""
        qx, qy, qz, qw = q
        xx, yy, zz = qx * qx, qy * qy, qz * qz
        xy, xz, yz = qx * qy, qx * qz, qy * qz
        wx, wy, wz = qw * qx, qw * qy, qw * qz
        # glTF stores matrices column-major: m[i + 4*j] = element (row i, col j)
        # R rows: m00 m01 m02 / m10 m11 m12 / m20 m21 m22 (standard quat->R)
        m00 = 1 - 2 * (yy + zz)
        m01 = 2 * (xy - wz)
        m02 = 2 * (xz + wy)
        m10 = 2 * (xy + wz)
        m11 = 1 - 2 * (xx + zz)
        m12 = 2 * (yz - wx)
        m20 = 2 * (xz - wy)
        m21 = 2 * (yz + wx)
        m22 = 1 - 2 * (xx + yy)
        return [
            m00, m10, m20, 0,   # column 0
            m01, m11, m21, 0,   # column 1
            m02, m12, m22, 0,   # column 2
            0, 0, 0, 1,         # column 3 (translation patched in by caller)
        ]

    def invert_rigid(m: list[float]) -> list[float]:
        """Inverse of a rigid (rotation+translation) column-major 4x4."""
        # R^T in column-major storage: column j of R^T = row j of R = m[4*j .. 4*j+2]
        # translation' = -R^T * t ; (R^T t)_i = row_i(R) . t = m[4*i .. 4*i+2] . t
        tx = -(m[0] * m[12] + m[1] * m[13] + m[2] * m[14])
        ty = -(m[4] * m[12] + m[5] * m[13] + m[6] * m[14])
        tz = -(m[8] * m[12] + m[9] * m[13] + m[10] * m[14])
        return [
            m[0], m[4], m[8], 0,
            m[1], m[5], m[9], 0,
            m[2], m[6], m[10], 0,
            tx, ty, tz, 1,
        ]

    worlds = bone_world_transforms(skeleton)
    ibm = array("f")
    for wpos, wquat in worlds:
        m = quat_to_mat((wquat[0], wquat[2], -wquat[1], wquat[3]))
        m[12], m[13], m[14] = wpos[0], wpos[2], -wpos[1]
        ibm.extend(invert_rigid(m))
    ibm_accessor = assembler.add_float_accessor(ibm, len(skeleton), "MAT4")

    skin = {"joints": bone_node, "inverseBindMatrices": ibm_accessor}
    nodes[1]["skin"] = 0

    # --- animation clips: per-bone TRS channels targeting bone nodes ---
    animations: list[dict] = []
    for clip in clips:
        num_frames = clip["num_frames"]
        rate = clip["rate"] or 30.0
        times = array("f", (f / rate for f in range(num_frames)))
        time_accessor = assembler.add_float_accessor(times, num_frames, "SCALAR")
        samplers: list[dict] = []
        channels: list[dict] = []
        tracks = clip["tracks"]
        for bone_i, track in enumerate(tracks):
            if not track:
                continue
            translations = array("f")
            rotations = array("f")
            for key in track:
                translations.extend((key.position[0], key.position[2], -key.position[1]))
                rotations.extend((key.quat[0], key.quat[2], -key.quat[1], key.quat[3]))
            if drop_root_translation and _is_root_bone(skeleton, bone_i):
                translations = array("f", (0.0,) * (len(track) * 3))
            trans_accessor = assembler.add_float_accessor(translations, len(track), "VEC3", _ARRAY_BUFFER)
            rot_accessor = assembler.add_float_accessor(rotations, len(track), "VEC4", _ARRAY_BUFFER)
            trans_sampler = len(samplers)
            samplers.append({"input": time_accessor, "output": trans_accessor, "interpolation": "LINEAR"})
            rot_sampler = len(samplers)
            samplers.append({"input": time_accessor, "output": rot_accessor, "interpolation": "LINEAR"})
            channels.append({"sampler": trans_sampler, "target": {"node": bone_node[bone_i], "path": "translation"}})
            channels.append({"sampler": rot_sampler, "target": {"node": bone_node[bone_i], "path": "rotation"}})
        if channels:
            animations.append({"name": clip["name"], "samplers": samplers, "channels": channels})

    gltf = {
        "asset": {"version": "2.0", "generator": "apb-content-studio psk->glb (skinned)"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": nodes,
        "meshes": [{"primitives": primitives}],
        "skins": [skin],
        "animations": animations,
        "materials": materials_json,
        "accessors": assembler.accessors,
        "bufferViews": assembler.buffer_views,
        "buffers": [{"byteLength": 0}],
    }
    if assembler.images:
        gltf["images"] = assembler.images
        gltf["textures"] = assembler.textures_json
        gltf["samplers"] = assembler.samplers
    return _finish_glb(gltf, assembler.bin_buf)


def _is_root_bone(skeleton: list, bone_index: int) -> bool:
    bone = skeleton[bone_index]
    return bone.parent < 0 or bone.parent >= len(skeleton) or bone.parent == bone_index
