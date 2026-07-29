from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path

import apb_level_dump as ald


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_PACKAGE = "FinancialDistrict_Block09"
ASSET_ROOT = PROJECT_ROOT / "Content" / "Imported" / "Districts" / "Financial"
# This script emits ONE block. _realv2.json is the loader's district-wide candidate and is
# owned by build_financial_district_manifest.py, so writing there would destroy every other
# block's placements. Pass --output explicitly to target a different fixture.
OUTPUT_FILE = (
    PROJECT_ROOT / "Content" / "Data" / "district_placements" / "Financial_Block09_unit.json"
)


def sanitized_stem(stem: str) -> str:
    """Mirror the importer's asset-name sanitisation.

    F19: each of " ()" maps to "_", then RUNS collapse to one. Without the collapse,
    `Industrial Zone (LC)_0001` yields `Industrial_Zone__LC__0001`, which never matches
    the `Industrial_Zone_LC_0001` the importer actually wrote. Only archetype names
    containing spaces/parens were affected, so `Generic_NNNN` rows hid the defect.
    """
    replaced = "".join("_" if character in " ()" else character for character in stem)
    return re.sub(r"_+", "_", replaced)


def asset_index(asset_root: Path) -> dict[str, Path]:
    index: dict[str, Path] = {}
    for asset_path in asset_root.rglob("*.uasset"):
        index.setdefault(asset_path.stem.lower(), asset_path)
    return index


def resolve_asset(mesh_path: str | None, index: dict[str, Path]) -> Path | None:
    if not mesh_path:
        return None
    stem = mesh_path.rsplit(".", 1)[-1]
    candidates = (stem, f"{stem}_0", sanitized_stem(stem), f"{sanitized_stem(stem)}_0")
    for candidate in candidates:
        asset = index.get(candidate.lower())
        if asset is not None:
            return asset
    return None


def package_folder(source_package: str) -> str:
    """Map a retail package stem to its UE asset folder name.

    Generalized to support all districts, not just Financial. The folder name must
    match the directory under Content/Imported/Districts/ where the assets live.
    """
    if source_package.startswith("FinancialDistrict"):
        return "Financial"
    if source_package.startswith("WaterfrontDistrict"):
        return "Waterfront"
    if source_package.startswith("RWorldSocialDistrict"):
        return "Social"
    if source_package.startswith("PGBeaconDistrict"):
        return "Beacon"
    if source_package.startswith("PGCrateDistrict"):
        return "Crate"
    if source_package.startswith("PGAsylumDistrict"):
        return "Asylum"
    raise ValueError(f"unsupported source package: {source_package}")


def resolve_geometry(record: dict, index: dict[str, Path]) -> tuple[Path | None, str, str]:
    """Retail geometry for one visible placement, preferring the vertex-lit object itself."""
    vertexlit = resolve_asset(record.get("mesh_path"), index)
    if vertexlit is not None:
        return vertexlit, record["mesh_path"], "vertexlit_object"
    base_path = record.get("base_mesh_path")
    base = resolve_asset(base_path, index)
    if base is not None:
        return base, base_path, "linked_hidden_base_mesh"
    return None, None, "retail_geometry_not_recovered"


def placement_row(
    record: dict, asset_root: Path, index: dict[str, Path], source_package: str
) -> dict:
    source_id = record["source_id"]
    location = record.get("location")
    if not isinstance(location, list) or len(location) != 3:
        raise ValueError(f"missing three-value location for {source_id}")

    reason = record.get("reason")
    if reason is None:
        resolved_asset, geometry_source, resolution = resolve_geometry(record, index)
    else:
        resolved_asset, geometry_source, resolution = None, None, "not_source_visible"

    row = {
        "source_id": source_id,
        "location": [float(value) for value in location],
        "mesh_id": resolved_asset.stem if resolved_asset is not None else None,
        "ue_path": (
            "/Game/Imported/Districts/Financial/"
            + resolved_asset.relative_to(asset_root).with_suffix("").as_posix()
            if resolved_asset is not None else None
        ),
        "package": package_folder(source_package),
        "actor": record.get("actor"),
        "edge": "m_VertexLitComponent" if record.get("component") else None,
        "mesh_path": record.get("mesh_path"),
        "mesh_class": record.get("mesh_class"),
        "component": record.get("component"),
        "host": record.get("host"),
        "transform_source": record.get("transform_source"),
        "geometry_source_mesh": geometry_source,
        "geometry_resolution": resolution,
    }
    if resolution == "linked_hidden_base_mesh":
        row["appearance_fidelity"] = "vertex_lighting_not_recovered"
    # Not redundant: APBDistrictPlacement.h short-circuits on "reason" before its mesh_id
    # check, so without this the row is rejected as invalid_mesh_id (schema fault) instead
    # of as a named retail recovery gap. Keep in sync with that loader.
    if reason is None and resolution == "retail_geometry_not_recovered":
        reason = "retail_geometry_not_recovered"
    if reason is not None:
        row["reason"] = reason
    if record.get("rotation_present"):
        row["rotation"] = record["rotation"]
    if record.get("scale_present"):
        row["scale"] = record["scale"]
    return row


def main(argv: list[str]) -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--maps-dir", type=Path, default=ald.RETAIL_MAPS / "FinancialDistrict")
    parser.add_argument("--asset-root", type=Path, default=ASSET_ROOT)
    parser.add_argument("--output", type=Path, default=OUTPUT_FILE)
    parser.add_argument("--source-package", default=SOURCE_PACKAGE)
    args = parser.parse_args(argv)

    source_package = args.source_package
    maps_dir = args.maps_dir.resolve()
    asset_root = args.asset_root.resolve()
    output_path = args.output.resolve()
    print(f"RETAIL_MAPS={ald.RETAIL_MAPS.resolve()}")
    print(f"MAPS_DIR={maps_dir}")
    print(f"ASSET_ROOT={asset_root}")
    print(f"OUTPUT_FILE={output_path}")

    if not maps_dir.is_dir():
        raise FileNotFoundError(f"maps directory missing: {maps_dir}")
    if not asset_root.is_dir():
        raise FileNotFoundError(f"asset directory missing: {asset_root}")

    index = asset_index(asset_root)
    records = ald.renderable_placements(source_package, maps_dir)
    placements = [
        placement_row(record, asset_root, index, source_package) for record in records
    ]
    reasons = Counter(row["reason"] for row in placements if "reason" in row)
    visible = [row for row in placements
               if row["geometry_resolution"] != "not_source_visible"]
    spawnable = [row for row in placements if "reason" not in row]
    geometry_bound = [row for row in visible if row["ue_path"] is not None]
    geometry_missing = [row for row in visible if row["ue_path"] is None]
    resolutions = Counter(row["geometry_resolution"] for row in visible)
    renderable_count = len(spawnable)
    renderable_locations = [row["location"] for row in visible]
    if not renderable_locations:
        raise ValueError("cannot derive spawn points without renderable placements")
    player_start = [
        sum(location[0] for location in renderable_locations) / len(renderable_locations),
        sum(location[1] for location in renderable_locations) / len(renderable_locations),
        max(location[2] for location in renderable_locations) + 250.0,
    ]
    vehicle_start = [player_start[0] + 600.0, player_start[1] - 200.0, player_start[2] - 50.0]
    manifest = {
        "district_id": "Financial",
        "source_package": source_package,
        "provenance": "real",
        "extractor": "apb_level_dump.renderable_placements",
        "source_build": "retail",
        "renderable_count": renderable_count,
        "source_visible_placement_count": len(visible),
        "geometry_bound_count": len(geometry_bound),
        "geometry_missing_count": len(geometry_missing),
        "geometry_resolution_histogram": dict(sorted(resolutions.items())),
        "total_row_count": len(placements),
        "reason_histogram": dict(sorted(reasons.items())),
        "player_start": player_start,
        "vehicle_start": vehicle_start,
        "spawn_points_derived": True,
        "spawn_points_note": (
            "Derived from the renderable placement centroid; not read from the source package."
        ),
        "placements": placements,
    }
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"WROTE renderable_count={renderable_count} total_row_count={len(placements)}")
    print(f"GEOMETRY_BOUND={len(geometry_bound)} GEOMETRY_MISSING={len(geometry_missing)}")
    print(f"RESOLUTION_HISTOGRAM={json.dumps(manifest['geometry_resolution_histogram'], sort_keys=True)}")
    print(f"REASON_HISTOGRAM={json.dumps(manifest['reason_histogram'], sort_keys=True)}")
    print(f"PLAYER_START={json.dumps(player_start)}")
    print(f"VEHICLE_START={json.dumps(vehicle_start)}")


if __name__ == "__main__":
    main(sys.argv[1:])
