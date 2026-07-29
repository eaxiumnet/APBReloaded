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

from psk import PskMesh

_GLB_MAGIC = 0x46546C67  # 'glTF'
_CHUNK_JSON = 0x4E4F534A  # 'JSON'
_CHUNK_BIN = 0x004E4942  # 'BIN\0'

# glTF accessor component types
_FLOAT = 5126
_UINT32 = 5125
# bufferView targets
_ARRAY_BUFFER = 34962
_ELEMENT_ARRAY_BUFFER = 34963


def tga_to_png_bytes(path: Path) -> bytes:
    with Image.open(path) as image:
        mode = "RGBA" if "A" in image.getbands() or "transparency" in image.info else "RGB"
        with image.convert(mode) as converted, BytesIO() as output:
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


def mesh_to_glb(mesh: PskMesh, textures: dict[str, Path] | None = None) -> bytes:
    """Serialize *mesh* to glTF 2.0 binary bytes."""
    # --- build shared vertex arrays (one glTF vertex per wedge) ---
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

    # --- group faces by material -> one index array per primitive ---
    mats = mesh.materials or ["material_0"]
    by_mat: dict[int, array] = {}
    for w0, w1, w2, midx in mesh.faces:
        m = midx if 0 <= midx < len(mats) else 0
        # reverse winding (w0,w2,w1) because we flipped an axis
        by_mat.setdefault(m, array("I")).extend((w0, w2, w1))
    if not by_mat:  # no faces -> nothing to draw, still emit a valid (empty) primitive set
        by_mat = {0: array("I")}

    # --- assemble the binary buffer + bufferViews + accessors ---
    bin_buf = bytearray()
    buffer_views: list[dict] = []
    accessors: list[dict] = []

    def add_view(data: bytes, target: int | None = None) -> int:
        _pad4(bin_buf)
        offset = len(bin_buf)
        bin_buf.extend(data)
        view = {"buffer": 0, "byteOffset": offset, "byteLength": len(data)}
        if target is not None:
            view["target"] = target
        buffer_views.append(view)
        return len(buffer_views) - 1

    pos_view = add_view(positions.tobytes(), _ARRAY_BUFFER)
    uv_view = add_view(texcoords.tobytes(), _ARRAY_BUFFER)
    normal_view = add_view(normals.tobytes(), _ARRAY_BUFFER)

    pos_accessor = len(accessors)
    accessors.append({
        "bufferView": pos_view, "componentType": _FLOAT, "count": vertex_count,
        "type": "VEC3", "min": minp, "max": maxp,
    })
    uv_accessor = len(accessors)
    accessors.append({
        "bufferView": uv_view, "componentType": _FLOAT, "count": vertex_count, "type": "VEC2",
    })
    normal_accessor = len(accessors)
    accessors.append({
        "bufferView": normal_view, "componentType": _FLOAT, "count": vertex_count, "type": "VEC3",
    })

    images: list[dict] = []
    textures_json: list[dict] = []
    samplers: list[dict] = []
    texture_indices: dict[str, int] = {}
    if textures:
        for key in ("baseColor", "normal", "specular", "opacity"):
            path = textures.get(key)
            if path is None or not path.is_file():
                continue
            png_bytes = tga_to_png_bytes(path)
            image_view = add_view(png_bytes)
            images.append({"bufferView": image_view, "mimeType": "image/png"})
            sampler_index = len(samplers)
            samplers.append({
                "magFilter": 9729,
                "minFilter": 9729,
                "wrapS": 10497,
                "wrapT": 10497,
            })
            texture_indices[key] = len(textures_json)
            textures_json.append({"sampler": sampler_index, "source": len(images) - 1})

    primitives: list[dict] = []
    materials_json: list[dict] = []
    for prim_i, (m, indices) in enumerate(sorted(by_mat.items())):
        idx_view = add_view(indices.tobytes(), _ELEMENT_ARRAY_BUFFER)
        idx_accessor = len(accessors)
        accessors.append({
            "bufferView": idx_view, "componentType": _UINT32,
            "count": len(indices), "type": "SCALAR",
        })
        primitives.append({
            "attributes": {
                "POSITION": pos_accessor,
                "TEXCOORD_0": uv_accessor,
                "NORMAL": normal_accessor,
            },
            "indices": idx_accessor, "material": prim_i,
        })
        name = mats[m] if 0 <= m < len(mats) else f"material_{m}"
        
        # Calculate metallic factor from specular texture presence
        has_specular = "specular" in texture_indices
        metallic_factor = 0.8 if has_specular else 0.1
        roughness_factor = 0.3 if has_specular else 0.7
        
        material = {
            "name": name,
            "pbrMetallicRoughness": {
                "baseColorFactor": [1.0, 1.0, 1.0, 1.0],
                "metallicFactor": metallic_factor,
                "roughnessFactor": roughness_factor,
            },
            "doubleSided": True,
        }
        if "baseColor" in texture_indices:
            material["pbrMetallicRoughness"]["baseColorTexture"] = {
                "index": texture_indices["baseColor"],
            }
        if "normal" in texture_indices:
            material["normalTexture"] = {"index": texture_indices["normal"]}
        if has_specular:
            # APB specular maps are grayscale intensity maps
            # Bright specular = shiny/metallic, dark = matte
            # Use as roughness (G channel in glTF metallicRoughness)
            material["pbrMetallicRoughness"]["metallicRoughnessTexture"] = {
                "index": texture_indices["specular"],
            }
        if "opacity" in texture_indices:
            material["alphaMode"] = "MASK"
            material["alphaCutoff"] = 0.5
        if "emissive" in texture_indices:
            # Glow/emissive effects
            material["emissiveTexture"] = {"index": texture_indices["emissive"]}
            material["emissiveFactor"] = [1.0, 1.0, 1.0]
        materials_json.append(material)

    gltf = {
        "asset": {"version": "2.0", "generator": "apb-content-studio psk->glb"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0}],
        "meshes": [{"primitives": primitives}],
        "materials": materials_json,
        "accessors": accessors,
        "bufferViews": buffer_views,
        "buffers": [{"byteLength": 0}],
    }
    if images:
        gltf["images"] = images
        gltf["textures"] = textures_json
        gltf["samplers"] = samplers

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
