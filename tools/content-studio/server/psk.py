"""ActorX .psk / .pskx parser (static-mesh scope).

APB base weapon/vehicle/clothing meshes were extracted by umodel to ActorX .psk
files. This module parses the chunks needed to render a static preview:

    PNTS0000  vertex positions   (FVector: 3x float32)
    VTXW0000  wedges             (point index + UV + material index)
    FACE0000  triangles          (3 wedge indices + material index)
    MATT0000  materials          (64-byte name + ints)

Skeleton (REFSKELT) and skin weights (RAWWEIGHTS) are intentionally ignored for
the viewer slice (plan B3). The classic 16-bit-index format is fully supported;
the .pskx 32-bit-index variants (VTXW3200 / FACE0032) are handled best-effort
via chunk stride so larger meshes still load.

Pure stdlib (struct) — no third-party deps.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field
from pathlib import Path

# name[20] + flags(int32) + datasize(int32) + count(int32)
_CHUNK_HEADER = struct.Struct("<20s i i i")
_HEADER_SIZE = _CHUNK_HEADER.size  # 32


@dataclass
class PskMesh:
    """Parsed static-mesh data in raw Unreal coordinate space."""

    points: list[tuple[float, float, float]] = field(default_factory=list)
    # (point_index, u, v, material_index)
    wedges: list[tuple[int, float, float, int]] = field(default_factory=list)
    # (wedge0, wedge1, wedge2, material_index)
    faces: list[tuple[int, int, int, int]] = field(default_factory=list)
    materials: list[str] = field(default_factory=list)

    @property
    def triangle_count(self) -> int:
        return len(self.faces)


def _iter_chunks(data: bytes):
    """Yield (name, datasize, count, payload) for each ActorX chunk."""
    pos = 0
    total = len(data)
    while pos + _HEADER_SIZE <= total:
        raw_name, _flags, dsize, count = _CHUNK_HEADER.unpack_from(data, pos)
        name = raw_name.split(b"\0", 1)[0].decode("ascii", "replace").strip()
        pos += _HEADER_SIZE
        span = dsize * count
        if span < 0 or pos + span > total:
            raise ValueError(
                f"chunk {name!r} declares datasize*count={span} exceeding file "
                f"(pos={pos}, size={total})"
            )
        payload = data[pos : pos + span]
        pos += span
        yield name, dsize, count, payload


@dataclass
class PskBone:
    """One skeleton bone (bind pose, parent-relative)."""

    name: str
    parent: int
    # quaternion (x, y, z, w), parent-relative orientation
    quat: tuple[float, float, float, float]
    # position (x, y, z), parent-relative
    position: tuple[float, float, float]


@dataclass
class PskWeight:
    """One raw skin influence: point_index gets `weight` from `bone_index`."""

    point_index: int
    bone_index: int
    weight: float


def parse_weights(data: bytes) -> list[PskWeight]:
    """Parse the RAWWEIGHTS chunk into per-point skin influences.

    VRawBoneInfluence (umodel ExportPsk.h): Weight(f32)@0 + PointIndex(i32)@4
    + BoneIndex(i32)@8 = 12 bytes. Bone indices index REFSKELT rows.
    """
    influences: list[PskWeight] = []
    for name, dsize, count, payload in _iter_chunks(data):
        if not name.startswith("RAWWEIGHT"):
            continue
        for i in range(count):
            off = i * dsize
            weight, point_index, bone_index = struct.unpack_from("<fii", payload, off)
            influences.append(PskWeight(point_index, bone_index, weight))
        break
    return influences


def skin_weights_by_point(
    influences: list[PskWeight], num_points: int
) -> dict[int, list[tuple[int, float]]]:
    """Collapse raw influences to per-point top-4 (bone_index, weight) lists."""
    by_point: dict[int, list[tuple[int, float]]] = {}
    for influence in influences:
        if not (0 <= influence.point_index < num_points):
            continue
        by_point.setdefault(influence.point_index, []).append(
            (influence.bone_index, influence.weight)
        )
    out: dict[int, list[tuple[int, float]]] = {}
    for point_index, entries in by_point.items():
        entries.sort(key=lambda entry: -entry[1])
        out[point_index] = entries[:4]
    return out


def _quat_mul(a: tuple[float, float, float, float], b: tuple[float, float, float, float]):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by + ay * bw + az * bx - ax * bz,
        aw * bz + az * bw + ax * by - ay * bx,
        aw * bw - ax * bx - ay * by - az * bz,
    )


def _rotate(v: tuple[float, float, float], q: tuple[float, float, float, float]):
    qx, qy, qz, qw = q
    # v' = q * v * q^-1 (rotation only); verified against the raw quat product
    ix, iy, iz = qw * v[0] + qy * v[2] - qz * v[1], \
                 qw * v[1] + qz * v[0] - qx * v[2], \
                 qw * v[2] + qx * v[1] - qy * v[0]
    iw = -qx * v[0] - qy * v[1] - qz * v[2]
    return (
        ix * qw - iw * qx - iy * qz + iz * qy,
        iy * qw - iw * qy - iz * qx + ix * qz,
        iz * qw - iw * qz - ix * qy + iy * qx,
    )


def bone_world_transforms(
    bones: list[PskBone],
) -> list[tuple[tuple[float, float, float], tuple[float, float, float, float]]]:
    """World-space (position, quat) per bone, chained from REFSKELT parents.

    Unreal component space: world = parent_world * local (rotation applied
    before translation offset). Root bones (parent < 0 or self) are world-local.
    """
    worlds: list[tuple[tuple[float, float, float], tuple[float, float, float, float]]] = []
    resolved = [False] * len(bones)
    for index, bone in enumerate(bones):
        if bone.parent < 0 or bone.parent >= len(bones) or bone.parent == index:
            worlds.append((bone.position, bone.quat))
            resolved[index] = True
            continue
        worlds.append(((0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0)))  # placeholder

    # REFSKELT is usually parent-before-child, but not guaranteed: resolve in
    # dependency order so a later-ordered parent still chains correctly.
    remaining = [i for i in range(len(bones)) if not resolved[i]]
    while remaining:
        progressed = False
        still = []
        for index in remaining:
            bone = bones[index]
            parent = bone.parent
            if not resolved[parent]:
                still.append(index)
                continue
            parent_pos, parent_quat = worlds[parent]
            rotated = _rotate(bone.position, parent_quat)
            worlds[index] = (
                (parent_pos[0] + rotated[0], parent_pos[1] + rotated[1], parent_pos[2] + rotated[2]),
                _quat_mul(parent_quat, bone.quat),
            )
            resolved[index] = True
            progressed = True
        if not progressed:
            # parent cycle: leave the remaining bones at identity rather than hang
            break
        remaining = still
    return worlds


def reconstruct_bind_skeleton(
    points: list[tuple[float, float, float]],
    weights: dict[int, list[tuple[int, float]]],
    skeleton: list[PskBone],
) -> list[PskBone]:
    """Rebuild a mesh-aligned bind skeleton from the skin weights.

    The REFSKELT inside the character PSKs is the animation *reference*
    skeleton: its bones sit 50-160cm away from the mesh geometry (median
    vertex-to-bone distance ~80cm), so composing animation keys through it
    distorts the mesh into stretched ribbons. The weights themselves define
    the real bind pose, so re-derive it:

    * origin  = joint position = centroid of the vertices the bone shares
                with its parent (the skin wraps the joint); root falls back
                to its own vertex centroid.
    * +X axis = from the joint toward the centroid of the bone's own
                vertices (the limb direction).
    * roll    = taken from the hint skeleton's world orientation so the
                animation keys (authored against the same reference) keep
                their bend convention.

    Unweighted helper bones (hair, prop sockets) ride the rig by inheriting
    the hint skeleton's local offset from their reconstructed parent.

    Returns a new skeleton with the same names/parents and mesh-aligned
    parent-relative positions/quats.
    """

    def qmul(a, b):
        ax, ay, az, aw = a
        bx, by, bz, bw = b
        return (
            aw * bx + ax * bw + ay * bz - az * by,
            aw * by + ay * bw + az * bx - ax * bz,
            aw * bz + az * bw + ax * by - ay * bx,
            aw * bw - ax * bx - ay * by - az * bz,
        )

    def qconj(q):
        return (-q[0], -q[1], -q[2], q[3])

    def qnorm(q):
        m = (q[0] ** 2 + q[1] ** 2 + q[2] ** 2 + q[3] ** 2) ** 0.5
        return q if m == 0.0 else tuple(v / m for v in q)

    def qrot(v, q):
        qx, qy, qz, qw = q
        ix = qw * v[0] + qy * v[2] - qz * v[1]
        iy = qw * v[1] + qz * v[0] - qx * v[2]
        iz = qw * v[2] + qx * v[1] - qy * v[0]
        iw = -qx * v[0] - qy * v[1] - qz * v[2]
        return (
            ix * qw - iw * qx - iy * qz + iz * qy,
            iy * qw - iw * qy - iz * qx + ix * qz,
            iz * qw - iw * qz - ix * qy + iy * qx,
        )

    def vsub(a, b):
        return (a[0] - b[0], a[1] - b[1], a[2] - b[2])

    def vlen(v):
        return (v[0] ** 2 + v[1] ** 2 + v[2] ** 2) ** 0.5

    def vnorm(v):
        l = vlen(v)
        return (0.0, 1.0, 0.0) if l == 0.0 else (v[0] / l, v[1] / l, v[2] / l)

    def vcross(a, b):
        return (
            a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0],
        )

    def vdot(a, b):
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]

    def align(a, b):
        """Unit quaternion rotating vector a onto vector b."""
        a = vnorm(a)
        b = vnorm(b)
        axis = vcross(a, b)
        dot = vdot(a, b)
        if vlen(axis) < 1e-6:
            # parallel or anti-parallel: use any perpendicular axis
            perp = (0.0, 0.0, 1.0) if abs(a[0]) < 0.9 else (1.0, 0.0, 0.0)
            axis = vcross(a, perp)
            if vlen(axis) < 1e-6:
                axis = (0.0, 1.0, 0.0)
            return qnorm((axis[0], axis[1], axis[2], -1.0 if dot < 0 else 1.0))
        axis = vnorm(axis)
        return qnorm((axis[0] * (1.0 + dot), axis[1] * (1.0 + dot), axis[2] * (1.0 + dot), 1.0))

    def centroid(ids):
        ids = list(ids)
        if not ids:
            return None
        c = [0.0, 0.0, 0.0]
        for pidx in ids:
            p = points[pidx]
            c[0] += p[0]
            c[1] += p[1]
            c[2] += p[2]
        return (c[0] / len(ids), c[1] / len(ids), c[2] / len(ids))

    n = len(skeleton)
    bone_verts: list[set] = [set() for _ in range(n)]
    for pidx, influences in weights.items():
        for bi, _w in influences[:4]:
            if 0 <= bi < n:
                bone_verts[bi].add(pidx)

    ref_worlds = bone_world_transforms(skeleton)

    # topological order (parent before child)
    order = sorted(
        range(n),
        key=lambda i: -1 if skeleton[i].parent < 0 or skeleton[i].parent >= n
        or skeleton[i].parent == i else skeleton[i].parent,
    )

    world_pos: list = [None] * n
    world_quat: list = [None] * n
    for i in order:
        bone = skeleton[i]
        parent = bone.parent if 0 <= bone.parent < n and bone.parent != i else -1
        own = bone_verts[i]
        own_c = centroid(own)
        if own_c is not None:
            if parent >= 0 and world_pos[parent] is not None:
                shared = bone_verts[i] & bone_verts[parent]
                joint = centroid(shared) or own_c
            else:
                joint = own_c
            direction = vnorm(vsub(own_c, joint))
        else:
            # unweighted helper bone: ride the parent using the hint offset
            ref_pos, ref_quat = ref_worlds[i]
            if parent >= 0 and world_pos[parent] is not None:
                joint = tuple(
                    world_pos[parent][k]
                    + qrot(ref_pos, world_quat[parent])[k] for k in range(3))
            else:
                joint = ref_pos
            direction = vnorm(qrot((1.0, 0.0, 0.0), ref_quat))
        ref_quat = ref_worlds[i][1]
        ref_x = vnorm(qrot((1.0, 0.0, 0.0), ref_quat))
        world_quat[i] = qnorm(qmul(align(ref_x, direction), ref_quat))
        world_pos[i] = joint

    # convert world -> parent-relative locals
    locals_pos: list = [None] * n
    locals_quat: list = [None] * n
    for i in order:
        bone = skeleton[i]
        parent = bone.parent if 0 <= bone.parent < n and bone.parent != i else -1
        if parent < 0:
            locals_pos[i] = world_pos[i]
            locals_quat[i] = world_quat[i]
        else:
            pq = world_quat[parent]
            locals_pos[i] = qrot(vsub(world_pos[i], world_pos[parent]), qconj(pq))
            locals_quat[i] = qnorm(qmul(qconj(pq), world_quat[i]))

    return [
        PskBone(
            name=bone.name,
            parent=bone.parent,
            position=locals_pos[i],
            quat=locals_quat[i],
        )
        for i, bone in enumerate(skeleton)
    ]


def parse_skeleton(data: bytes) -> list[PskBone]:
    """Parse the REFSKELT chunk into parent-relative :class:`PskBone` rows.

    VBone layout (umodel ExportPsk.h): Name[64] + Flags(u32) + NumChildren(i32)
    + ParentIndex(i32) + Orientation(FQuat: 4x f32) + Position(FVector: 3x f32)
    + Length(f32) + Size(FVector: 3x f32) = 120 bytes.
    """
    bones: list[PskBone] = []
    for name, dsize, count, payload in _iter_chunks(data):
        if not name.startswith("REFSKELT"):
            continue
        for i in range(count):
            off = i * dsize
            raw = payload[off : off + 64]
            bone_name = raw.split(b"\0", 1)[0].decode("ascii", "replace").strip()
            parent = struct.unpack_from("<i", payload, off + 72)[0]
            qx, qy, qz, qw = struct.unpack_from("<ffff", payload, off + 76)
            px, py, pz = struct.unpack_from("<fff", payload, off + 92)
            bones.append(PskBone(bone_name, parent, (qx, qy, qz, qw), (px, py, pz)))
        break
    return bones


def parse_skeleton_file(path: str | Path) -> list[PskBone]:
    return parse_skeleton(Path(path).read_bytes())


def parse_psk(data: bytes) -> PskMesh:
    """Parse ActorX .psk/.pskx bytes into a :class:`PskMesh`."""
    mesh = PskMesh()
    seen_header = False

    for name, dsize, count, payload in _iter_chunks(data):
        if not seen_header:
            if not name.startswith("ACTRHEAD"):
                raise ValueError(f"not an ActorX psk (first chunk was {name!r})")
            seen_header = True
            continue

        if name.startswith("PNTS"):
            for i in range(count):
                x, y, z = struct.unpack_from("<fff", payload, i * dsize)
                mesh.points.append((x, y, z))

        elif name.startswith("VTXW"):
            # classic VVertex stride 16: uint16 pointIdx @0, float U @4, float V @8, uint8 mat @12
            # pskx 32-bit variant: uint32 pointIdx @0, float U @4, float V @8, uint8 mat @16
            big = dsize >= 18
            for i in range(count):
                off = i * dsize
                if big:
                    pidx = struct.unpack_from("<I", payload, off)[0]
                    u, v = struct.unpack_from("<ff", payload, off + 4)
                    midx = payload[off + 16]
                else:
                    pidx = struct.unpack_from("<H", payload, off)[0]
                    u, v = struct.unpack_from("<ff", payload, off + 4)
                    midx = payload[off + 12]
                mesh.wedges.append((pidx, u, v, midx))

        elif name.startswith("FACE"):
            # classic VTriangle stride 12: uint16[3] @0, uint8 mat @6
            # pskx variant stride >=16: uint32[3] @0, uint8 mat @12
            big = dsize >= 16
            for i in range(count):
                off = i * dsize
                if big:
                    w0, w1, w2 = struct.unpack_from("<III", payload, off)
                    midx = payload[off + 12]
                else:
                    w0, w1, w2 = struct.unpack_from("<HHH", payload, off)
                    midx = payload[off + 6]
                mesh.faces.append((w0, w1, w2, midx))

        elif name.startswith("MATT"):
            for i in range(count):
                off = i * dsize
                raw = payload[off : off + 64]
                nm = raw.split(b"\0", 1)[0].decode("ascii", "replace").strip()
                mesh.materials.append(nm)

    if not seen_header:
        raise ValueError("empty or non-ActorX data (no ACTRHEAD chunk)")
    return mesh


def parse_psk_file(path: str | Path) -> PskMesh:
    return parse_psk(Path(path).read_bytes())
