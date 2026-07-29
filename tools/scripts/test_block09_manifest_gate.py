from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

import apb_level_dump as ald


PROJECT_ROOT = Path(__file__).resolve().parents[2]
# Block09-only fixture, NOT the loader's _realv2.json: that path now holds the merged
# 44-block district manifest, whose counts/SHAs/spawn-Z legitimately differ from the
# per-block ones pinned below. This gate keeps proving per-block extraction and the F19
# resolver fix; district-wide invariants live in test_financial_district_manifest_gate.py.
DEFAULT_MANIFEST = (
    PROJECT_ROOT / "Content" / "Data" / "district_placements" / "Financial_Block09_unit.json"
)
DEFAULT_MAPS_DIR = ald.RETAIL_MAPS / "FinancialDistrict"

# Measured from retail FinancialDistrict_Block09 by write_block09_real_manifest.py.
# Counted independently of geometry recovery so fidelity loss cannot shrink the total.
EXPECTED_SOURCE_VISIBLE_COUNT = 57
# F19: was 35/35/22. Loosened only because a resolver bug was FIXED: sanitized_stem emitted
# double underscores for archetype names with " ()", so 22 rows never matched the imported
# single-underscore assets. That geometry was in Block09 and imported all along.
EXPECTED_SPAWNABLE_COUNT = 57
EXPECTED_GEOMETRY_BOUND_COUNT = 57
EXPECTED_GEOMETRY_MISSING_COUNT = 0
# sha256[:16] of the sorted vertex-lit mesh stems in each set: pins row identity so a
# dropped shell cannot be masked by a duplicate elsewhere holding the count constant.
EXPECTED_BOUND_SET_SHA = "8bcb965a5a398ec7"
# sha256 of "" - the empty missing-set is now the asserted state, not an unset placeholder.
EXPECTED_MISSING_SET_SHA = "e3b0c44298fc1c14"
EXPECTED_SPAWN_POINTS_DERIVED = True


def set_sha(rows: list[dict]) -> str:
    identities = sorted(f"{row['source_id']}::{row['mesh_path'].rsplit('.', 1)[-1]}"
                        for row in rows)
    return hashlib.sha256("|".join(identities).encode()).hexdigest()[:16]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def is_arithmetic_ramp(values: list[float]) -> bool:
    if len(values) < 2:
        return False
    delta = (values[1] - values[0]) % 360.0
    return all(
        abs(((values[index] - values[0]) % 360.0) - ((index * delta) % 360.0)) < 1e-6
        for index in range(2, len(values))
    )


def main(argv: list[str]) -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--maps-dir", type=Path, default=DEFAULT_MAPS_DIR)
    args = parser.parse_args(argv)

    manifest_path = args.manifest.resolve()
    maps_dir = args.maps_dir.resolve()
    print(f"MAPS_DIR={maps_dir}")
    print(f"MANIFEST_FILE={manifest_path}")
    require(manifest_path.is_file(), f"manifest missing: {manifest_path}")

    with manifest_path.open(encoding="utf-8") as manifest_file:
        manifest = json.load(manifest_file)
    placements = manifest["placements"]

    source_ids = [row.get("source_id") for row in placements]
    require(all(isinstance(source_id, str) and source_id for source_id in source_ids),
            "every row must have a non-empty string source_id")
    require(len(source_ids) == len(set(source_ids)), "source_id values must be unique")
    for row in placements:
        require(isinstance(row.get("location"), list) and len(row["location"]) == 3 and
                all(isinstance(value, float) for value in row["location"]),
                f"location must be a three-float list for {row['source_id']}")
        for key in ("mesh_id", "ue_path", "package", "actor", "edge", "mesh_path", "mesh_class",
                    "geometry_source_mesh", "geometry_resolution", "transform_source"):
            require(key in row, f"missing {key} for {row['source_id']}")
    print("GATE_SOURCE_ID_UNIQUE: PASS")

    extracted = ald.renderable_placements("FinancialDistrict_Block09", maps_dir)
    extracted_rotation_count = sum(record["rotation_present"] for record in extracted)
    extracted_scale_count = sum(record["scale_present"] for record in extracted)
    rotation_rows = [row for row in placements if "rotation" in row]
    scale_rows = [row for row in placements if "scale" in row]
    require(len(rotation_rows) == extracted_rotation_count,
            f"rotation keys={len(rotation_rows)} extractor_present={extracted_rotation_count}")
    require(len(scale_rows) == extracted_scale_count,
            f"scale keys={len(scale_rows)} extractor_present={extracted_scale_count}")
    require(len(rotation_rows) == 0,
            f"Block09 cStreamedBuildingActor must have no rotation keys, got {len(rotation_rows)}")
    require(len(scale_rows) == 0,
            f"Block09 cStreamedBuildingActor must have no scale keys, got {len(scale_rows)}")
    print("GATE_MISSINGNESS_PARITY: PASS")

    yaws = [row["rotation"][2] for row in rotation_rows]
    require(not rotation_rows, "Block09 must explicitly preserve rotation absence")
    require(not yaws or not all(yaw == (index * 11) % 360 for index, yaw in enumerate(yaws)),
            "legacy (i*11)%360 yaw ramp detected")
    require(not yaws or not is_arithmetic_ramp(yaws), "arithmetic yaw ramp detected")
    print("GATE_NO_RAMP: PASS")

    visible = [row for row in placements
               if row["geometry_resolution"] != "not_source_visible"]
    renderable_count = len(visible)
    bound = [row for row in visible if row["ue_path"] is not None]
    missing = [row for row in visible if row["ue_path"] is None]
    require(renderable_count == EXPECTED_SOURCE_VISIBLE_COUNT,
            f"source_visible={renderable_count} expected={EXPECTED_SOURCE_VISIBLE_COUNT}")
    require(len(bound) == EXPECTED_GEOMETRY_BOUND_COUNT,
            f"geometry_bound={len(bound)} expected={EXPECTED_GEOMETRY_BOUND_COUNT}")
    require(len(missing) == EXPECTED_GEOMETRY_MISSING_COUNT,
            f"geometry_missing={len(missing)} expected={EXPECTED_GEOMETRY_MISSING_COUNT}")
    require(len(bound) + len(missing) == renderable_count,
            "visible rows must conserve as geometry_bound + geometry_missing")
    spawnable = [row for row in placements if "reason" not in row]
    require(len(spawnable) == EXPECTED_SPAWNABLE_COUNT,
            f"spawnable={len(spawnable)} expected={EXPECTED_SPAWNABLE_COUNT}")
    for key, expected in (("source_visible_placement_count", renderable_count),
                          ("geometry_bound_count", len(bound)),
                          ("geometry_missing_count", len(missing)),
                          ("renderable_count", len(spawnable))):
        require(manifest[key] == expected, f"top-level {key}={manifest[key]} rows={expected}")
    print("GATE_GEOMETRY_PROVENANCE_COUNTS: PASS")

    require(set_sha(bound) == EXPECTED_BOUND_SET_SHA,
            f"geometry-bound row identity changed: {set_sha(bound)} != {EXPECTED_BOUND_SET_SHA}")
    require(set_sha(missing) == EXPECTED_MISSING_SET_SHA,
            f"geometry-missing row identity changed: {set_sha(missing)} != {EXPECTED_MISSING_SET_SHA}")
    for row in missing:
        require(row["geometry_resolution"] == "retail_geometry_not_recovered",
                f"unrecovered row must say so, not asset_not_imported: {row['source_id']}")
        require(row.get("reason") == "retail_geometry_not_recovered",
                f"unrecovered row needs the loader reason or it is rejected as "
                f"invalid_mesh_id: {row['source_id']}")
    print("GATE_ROW_IDENTITY: PASS")

    reason_histogram = manifest["reason_histogram"]
    non_renderable_count = sum(reason_histogram.values())
    require(manifest["total_row_count"] == len(spawnable) + non_renderable_count,
            "total_row_count must conserve spawnable and reason_histogram rows")
    require(manifest["total_row_count"] == len(placements),
            f"top-level rows={manifest['total_row_count']} placements={len(placements)}")
    require(non_renderable_count == sum("reason" in row for row in placements),
            "reason_histogram must account for every non-renderable row")
    print("GATE_ROW_CONSERVATION: PASS")

    renderable_locations = [row["location"] for row in visible]
    for key in ("player_start", "vehicle_start"):
        require(key in manifest, f"missing {key}")
        require(isinstance(manifest[key], list) and len(manifest[key]) == 3 and
                all(isinstance(value, (int, float)) and not isinstance(value, bool)
                    for value in manifest[key]),
                f"{key} must be a three-element numeric list")
    require(manifest.get("spawn_points_derived") is EXPECTED_SPAWN_POINTS_DERIVED,
            "spawn_points_derived must match the pinned derived expectation")
    require(isinstance(manifest.get("spawn_points_note"), str) and manifest["spawn_points_note"],
            "spawn_points_note must be a non-empty string")

    min_x = min(location[0] for location in renderable_locations)
    max_x = max(location[0] for location in renderable_locations)
    min_y = min(location[1] for location in renderable_locations)
    max_y = max(location[1] for location in renderable_locations)
    player_start = manifest["player_start"]
    vehicle_start = manifest["vehicle_start"]
    require(min_x <= player_start[0] <= max_x and min_y <= player_start[1] <= max_y,
            "player_start X/Y must be inside the renderable placement bounding box")
    expected_player = [
        sum(location[0] for location in renderable_locations) / len(renderable_locations),
        sum(location[1] for location in renderable_locations) / len(renderable_locations),
        max(location[2] for location in renderable_locations) + 250.0,
    ]
    expected_vehicle = [expected_player[0] + 600.0, expected_player[1] - 200.0,
                        expected_player[2] - 50.0]
    require(all(math.isclose(actual, expected) for actual, expected in zip(player_start, expected_player)),
            "player_start must derive from renderable placement centroid and height")
    require(all(math.isclose(actual, expected) for actual, expected in zip(vehicle_start, expected_vehicle)),
            "vehicle_start must derive from player_start")
    print("GATE_SPAWN_POINTS_DERIVED: PASS")


if __name__ == "__main__":
    main(sys.argv[1:])
