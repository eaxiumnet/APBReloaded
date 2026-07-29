"""Regenerate Financial Block09 placements from REAL extracted transforms.

Replaces the fabricated pipeline (synthesized (i*11)%360 rotations, round-robin mesh
assignment). Each placement carries the genuine Location/Rotation/Scale read from the
UE3 package via extract_actor_transforms, and a mesh_id ONLY when the resolved source
mesh name maps to an asset that is actually imported under Content/Imported/Districts.
No mesh is ever fabricated: an actor whose mesh cannot be resolved to an imported asset
is recorded as unresolved rather than assigned a placeholder.

Emitted to a NEW file so the existing district manifest is preserved for comparison.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import extract_actor_transforms as ext  # noqa: E402

MAPS = Path(
    r"D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America"
    r"\APBGame\Content\FinancialDistrict\Maps"
)
IMPORTED = Path(r"D:\APBReloaded\Content\Imported\Districts\Financial")
OUT = Path(r"D:\APBReloaded\Content\Data\district_placements\Financial_Block09_real.json")
UE_FOLDER = "Financial"


def _sanitize(name: str) -> str:
    s = re.sub(r"[ ()]+", "_", name)
    return re.sub(r"_+", "_", s).strip("_")


def build_asset_index() -> set[str]:
    return {p.stem for p in IMPORTED.rglob("*.uasset")}


def resolve_asset(mesh_name: str, assets: set[str]) -> str | None:
    for cand in (mesh_name, mesh_name + "_0", _sanitize(mesh_name), _sanitize(mesh_name) + "_0"):
        if cand in assets:
            return cand
    return None


def main() -> int:
    assets = build_asset_index()
    recs = ext.extract_block("FinancialDistrict_Block09", MAPS)

    placements: list[dict] = []
    seen: set[tuple] = set()
    unresolved = 0
    for r in recs:
        asset = resolve_asset(r["mesh_name"], assets) if r["mesh_name"] else None
        if not asset:
            unresolved += 1
            continue
        key = (
            round(r["location"][0], 1),
            round(r["location"][1], 1),
            round(r["location"][2], 1),
            asset,
        )
        if key in seen:
            continue
        seen.add(key)
        placements.append({
            "mesh_id": asset,
            "ue_path": f"/Game/Imported/Districts/{UE_FOLDER}/{asset}.{asset}",
            "location": r["location"],
            "rotation": r["rotation"],
            "scale": r["scale"],
            "package": r["package"],
            "actor": r["actor"],
            "actor_class": r["actor_class"],
            "mesh_source": r["mesh_source"],
        })

    xs = [p["location"][0] for p in placements]
    ys = [p["location"][1] for p in placements]
    zs = [p["location"][2] for p in placements]
    player = [sum(xs) / len(xs), sum(ys) / len(ys), max(zs) + 250.0]

    manifest = {
        "district_id": "Financial",
        "source_package": "FinancialDistrict_Block09",
        "source_packages": ext._block_family("FinancialDistrict_Block09", MAPS),
        "layout": "real_ue3_tagged_transforms",
        "layout_note": (
            "Real Location/Rotation(URU->deg)/Scale3D from UE3 tagged properties. "
            "mesh_id assigned only where resolved mesh maps to an imported asset; "
            "props referencing un-imported prefab packages are omitted."
        ),
        "actor_count": len(placements),
        "unresolved_actor_count": unresolved,
        "player_start": player,
        "vehicle_start": [player[0] + 600, player[1] - 200, player[2] - 50],
        "placements": placements,
    }
    OUT.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(
        f"WROTE {OUT} placements={len(placements)} unresolved={unresolved} "
        f"x={min(xs):.0f}..{max(xs):.0f} y={min(ys):.0f}..{max(ys):.0f}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
