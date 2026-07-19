#!/usr/bin/env python3
"""ActorX PSK/PSKX reader: positions, wedges (UV), faces, materials + package textures."""
from __future__ import annotations

import struct
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class Wedge:
    point_index: int
    u: float
    v: float
    mat_index: int = 0


@dataclass
class Face:
    w0: int
    w1: int
    w2: int
    mat_index: int = 0


@dataclass
class MaterialInfo:
    name: str
    texture_path: str = ""


@dataclass
class MeshData:
    path: str
    vertices: list[tuple[float, float, float]] = field(default_factory=list)
    wedges: list[Wedge] = field(default_factory=list)
    faces: list[Face] = field(default_factory=list)
    materials: list[MaterialInfo] = field(default_factory=list)
    # Expanded render buffers (one entry per corner)
    pos: list[tuple[float, float, float]] = field(default_factory=list)
    uvs: list[tuple[float, float]] = field(default_factory=list)
    norms: list[tuple[float, float, float]] = field(default_factory=list)
    # per-material triangle index lists into pos (flat i*3)
    mat_tri_ranges: list[tuple[int, int, int]] = field(default_factory=list)  # mat_idx, start_vert, count
    name: str = ""
    diffuse_path: str = ""


def _read_chunks(data: bytes) -> list[tuple[str, int, int, bytes]]:
    chunks: list[tuple[str, int, int, bytes]] = []
    off = 0
    n = len(data)
    while off + 32 <= n:
        chunk_id = data[off : off + 20].split(b"\x00", 1)[0].decode("latin-1", errors="replace")
        _type_flag, data_size, data_count = struct.unpack_from("<iii", data, off + 20)
        off += 32
        if data_count > 0 and data_size > 0:
            payload_len = data_size * data_count
        else:
            payload_len = max(data_size, 0)
        if off + payload_len > n:
            payload_len = n - off
        payload = data[off : off + payload_len]
        off += payload_len
        chunks.append((chunk_id.strip(), data_size, data_count, payload))
        if not chunk_id.strip():
            break
    return chunks


def find_package_textures(mesh_path: Path) -> dict[str, Path]:
    """Find Texture2D/* near a SkeletalMesh3/StaticMesh3 export."""
    found: dict[str, Path] = {}
    # Walk up a few levels looking for Texture2D
    cur = mesh_path.parent
    for _ in range(5):
        tex_dir = cur / "Texture2D"
        if tex_dir.is_dir():
            for p in tex_dir.iterdir():
                if p.suffix.lower() in (".tga", ".png", ".dds", ".bmp", ".jpg", ".jpeg"):
                    found[p.stem.lower()] = p
            if found:
                break
        # sibling
        for sibling in cur.iterdir() if cur.is_dir() else []:
            if sibling.is_dir() and sibling.name.lower() == "texture2d":
                for p in sibling.iterdir():
                    if p.suffix.lower() in (".tga", ".png", ".dds", ".bmp", ".jpg", ".jpeg"):
                        found[p.stem.lower()] = p
        cur = cur.parent
    return found


def pick_diffuse(tex_map: dict[str, Path]) -> str:
    if not tex_map:
        return ""
    priority = (
        "diffuse",
        "diff",
        "albedo",
        "basecolor",
        "base_color",
        "color",
        "d",
        "main",
    )
    for key in priority:
        if key in tex_map:
            return str(tex_map[key])
    # Prefer largest non-normal/mask texture
    candidates = []
    for k, p in tex_map.items():
        if any(x in k for x in ("normal", "norm", "mask", "spec", "rough", "metal", "opac", "emiss")):
            continue
        candidates.append(p)
    if not candidates:
        candidates = list(tex_map.values())
    candidates.sort(key=lambda p: p.stat().st_size if p.is_file() else 0, reverse=True)
    return str(candidates[0]) if candidates else ""


def load_psk(path: Path) -> MeshData:
    path = Path(path)
    raw = path.read_bytes()
    chunks = _read_chunks(raw)
    mesh = MeshData(path=str(path), name=path.stem)

    for cid, data_size, data_count, payload in chunks:
        if cid.startswith("PNTS"):
            rec = 12
            for i in range(len(payload) // rec):
                x, y, z = struct.unpack_from("<fff", payload, i * rec)
                mesh.vertices.append((x, y, z))
        elif cid.startswith("VTXW"):
            rec = data_size if data_size >= 12 else 16
            n = data_count if data_count > 0 else len(payload) // rec
            for i in range(n):
                base = i * rec
                if base + rec > len(payload):
                    break
                if rec >= 16:
                    # uint16 PointIndex, uint16 pad, float U, float V [, uint32]
                    pi, _pad, u, v = struct.unpack_from("<HHff", payload, base)
                    mat = 0
                    if rec >= 17:
                        mat = payload[base + 12]
                elif rec == 12:
                    # uint16 PointIndex, float U, float V, byte mat, byte res, uint16 pad — tight
                    pi, u, v, mat, _r, _p = struct.unpack_from("<HffBBH", payload, base)
                else:
                    pi = struct.unpack_from("<H", payload, base)[0]
                    u, v = struct.unpack_from("<ff", payload, base + 4)
                    mat = 0
                mesh.wedges.append(Wedge(point_index=int(pi), u=float(u), v=float(v), mat_index=int(mat)))
        elif cid.startswith("FACE"):
            rec = data_size if data_size >= 12 else 12
            n = data_count if data_count > 0 else len(payload) // rec
            for i in range(n):
                base = i * rec
                if base + 12 > len(payload):
                    break
                w0, w1, w2, mat, _aux, _sm = struct.unpack_from("<HHHBBI", payload, base)
                mesh.faces.append(Face(int(w0), int(w1), int(w2), int(mat)))
        elif cid.startswith("MATT"):
            rec = data_size if data_size >= 64 else 88
            n = data_count if data_count > 0 else len(payload) // rec
            for i in range(n):
                base = i * rec
                chunk = payload[base : base + rec]
                name = chunk[:64].split(b"\x00", 1)[0].decode("latin-1", errors="replace")
                mesh.materials.append(MaterialInfo(name=name or f"Mat{i}"))

    tex_map = find_package_textures(path)
    mesh.diffuse_path = pick_diffuse(tex_map)
    if mesh.materials and mesh.diffuse_path:
        for m in mesh.materials:
            m.texture_path = mesh.diffuse_path
    elif mesh.diffuse_path:
        mesh.materials.append(MaterialInfo(name="Diffuse", texture_path=mesh.diffuse_path))

    _build_render_buffers(mesh)
    return mesh


def _cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _norm(v):
    l = (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) ** 0.5
    if l < 1e-12:
        return (0.0, 1.0, 0.0)
    return (v[0] / l, v[1] / l, v[2] / l)


def _build_render_buffers(mesh: MeshData) -> None:
    """Expand faces into per-corner position/uv/normal for GL_TRIANGLES."""
    if not mesh.vertices or not mesh.faces or not mesh.wedges:
        # Fallback: point cloud only — no triangles
        mesh.pos = list(mesh.vertices)
        mesh.uvs = [(0.0, 0.0)] * len(mesh.vertices)
        mesh.norms = [(0.0, 1.0, 0.0)] * len(mesh.vertices)
        return

    # Group faces by material
    by_mat: dict[int, list[Face]] = {}
    for f in mesh.faces:
        by_mat.setdefault(f.mat_index, []).append(f)

    mesh.pos.clear()
    mesh.uvs.clear()
    mesh.norms.clear()
    mesh.mat_tri_ranges.clear()

    for mat_idx in sorted(by_mat.keys()):
        start = len(mesh.pos)
        for f in by_mat[mat_idx]:
            corners = []
            ok = True
            for wi in (f.w0, f.w1, f.w2):
                if wi < 0 or wi >= len(mesh.wedges):
                    ok = False
                    break
                w = mesh.wedges[wi]
                if w.point_index < 0 or w.point_index >= len(mesh.vertices):
                    ok = False
                    break
                corners.append((mesh.vertices[w.point_index], (w.u, 1.0 - w.v)))  # flip V for GL
            if not ok:
                continue
            p0, p1, p2 = corners[0][0], corners[1][0], corners[2][0]
            n = _norm(_cross(_sub(p1, p0), _sub(p2, p0)))
            for p, uv in corners:
                mesh.pos.append(p)
                mesh.uvs.append(uv)
                mesh.norms.append(n)
        count = len(mesh.pos) - start
        if count > 0:
            mesh.mat_tri_ranges.append((mat_idx, start, count))


def normalize_mesh(mesh: MeshData) -> None:
    if not mesh.pos and mesh.vertices:
        verts = mesh.vertices
    else:
        verts = mesh.pos
    if not verts:
        return
    xs = [v[0] for v in verts]
    ys = [v[1] for v in verts]
    zs = [v[2] for v in verts]
    cx = (min(xs) + max(xs)) * 0.5
    cy = (min(ys) + max(ys)) * 0.5
    cz = (min(zs) + max(zs)) * 0.5
    scale = max(max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs), 1e-3)
    s = 1.6 / scale

    def xf(v):
        return ((v[0] - cx) * s, (v[1] - cy) * s, (v[2] - cz) * s)

    mesh.vertices = [xf(v) for v in mesh.vertices]
    mesh.pos = [xf(v) for v in mesh.pos]


def export_obj(mesh: MeshData, out: Path) -> Path:
    lines = [f"# {mesh.name}", f"# verts={len(mesh.vertices)} faces={len(mesh.faces)}"]
    if mesh.diffuse_path:
        mtl = out.with_suffix(".mtl")
        mtl.write_text(
            f"newmtl mat0\nKd 1 1 1\nmap_Kd {Path(mesh.diffuse_path).name}\n",
            encoding="utf-8",
        )
        # copy note only — path relative
        lines.append(f"mtllib {mtl.name}")
        lines.append("usemtl mat0")
    for x, y, z in mesh.vertices:
        lines.append(f"v {x:.6f} {y:.6f} {z:.6f}")
    for w in mesh.wedges:
        lines.append(f"vt {w.u:.6f} {1.0 - w.v:.6f}")
    for f in mesh.faces:
        # OBJ is 1-based; use wedge indices for vt
        lines.append(
            f"f {mesh.wedges[f.w0].point_index + 1}/{f.w0 + 1} "
            f"{mesh.wedges[f.w1].point_index + 1}/{f.w1 + 1} "
            f"{mesh.wedges[f.w2].point_index + 1}/{f.w2 + 1}"
        )
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return out


def index_models(root: Path, limit: int = 5000) -> list[Path]:
    paths: list[Path] = []
    for pat in ("*.psk", "*.pskx"):
        for p in root.rglob(pat):
            # Prefer LOD_0 / non-lod names first by sorting later
            paths.append(p)
            if len(paths) >= limit:
                break
        if len(paths) >= limit:
            break
    # Prefer models that have nearby Texture2D
    def score(p: Path) -> tuple:
        has_tex = 1 if find_package_textures(p) else 0
        lod0 = 1 if "lod_0" in p.name.lower() or "lod0" in p.name.lower() else 0
        return (-has_tex, -lod0, p.name.lower())

    paths.sort(key=score)
    return paths
