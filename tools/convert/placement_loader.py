#!/usr/bin/env python3
"""Mirror of APBDistrictPlacementLoader::LoadManifestFromFile field shape (shipped C++)."""
from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class PlacementEntry:
    mesh_id: str
    ue_path: str
    package: str
    location: tuple[float, float, float]
    rotation: tuple[float, float, float] = (0.0, 0.0, 0.0)
    scale: tuple[float, float, float] = (1.0, 1.0, 1.0)


@dataclass
class DistrictManifest:
    district_id: str = ""
    source_package: str = ""
    player_start: tuple[float, float, float] = (0.0, 0.0, 0.0)
    vehicle_start: tuple[float, float, float] = (0.0, 0.0, 0.0)
    stream_chunk_count: int = 0
    placements: list[PlacementEntry] = field(default_factory=list)


def load_manifest_from_file(path: Path | str) -> DistrictManifest | None:
    """Same keys as Systems/APBDistrictPlacementLoader.cpp LoadManifestFromFile."""
    p = Path(path)
    if not p.is_file():
        return None
    root = json.loads(p.read_text(encoding="utf-8"))
    m = DistrictManifest()
    m.district_id = str(root.get("district_id") or "")
    m.source_package = str(root.get("source_package") or "")
    ps = root.get("player_start") or []
    if isinstance(ps, list) and len(ps) >= 3:
        m.player_start = (float(ps[0]), float(ps[1]), float(ps[2]))
    vs = root.get("vehicle_start") or []
    if isinstance(vs, list) and len(vs) >= 3:
        m.vehicle_start = (float(vs[0]), float(vs[1]), float(vs[2]))
    chunks = root.get("stream_chunks") or []
    if isinstance(chunks, list):
        m.stream_chunk_count = len(chunks)
    raw = root.get("placements")
    if not isinstance(raw, list) or not raw:
        return None
    for o in raw:
        if not isinstance(o, dict):
            continue
        loc = o.get("location") or [0, 0, 0]
        rot = o.get("rotation") or [0, 0, 0]
        scl = o.get("scale") or [1, 1, 1]
        m.placements.append(
            PlacementEntry(
                mesh_id=str(o.get("mesh_id") or ""),
                ue_path=str(o.get("ue_path") or ""),
                package=str(o.get("package") or ""),
                location=(float(loc[0]), float(loc[1]), float(loc[2])),
                rotation=(float(rot[0]), float(rot[1]), float(rot[2])),
                scale=(float(scl[0]), float(scl[1]), float(scl[2])),
            )
        )
    return m if m.placements else None


def manifest_uses_engine_cubes(m: DistrictManifest) -> bool:
    for e in m.placements:
        if "BasicShapes/Cube" in e.ue_path or "Cube" in e.mesh_id:
            return True
    return False
