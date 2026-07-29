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
