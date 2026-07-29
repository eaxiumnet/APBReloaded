from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

import apb_level_dump as ald


PROJECT_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_MANIFEST = (
    PROJECT_ROOT / "Content" / "Data" / "district_placements" / "Financial_Block09_realv2.json"
)
DEFAULT_MAPS_DIR = ald.RETAIL_MAPS / "FinancialDistrict"

EXPECTED_BLOCK_COUNT = 44
EXPECTED_SOURCE_VISIBLE_COUNT = 1457
EXPECTED_GEOMETRY_BOUND_COUNT = 1457
EXPECTED_GEOMETRY_MISSING_COUNT = 0
EXPECTED_SPAWNABLE_COUNT = 1457
EXPECTED_TOTAL_ROW_COUNT = 3565
EXPECTED_DISTINCT_UE_PATH_COUNT = 1457
EXPECTED_BOUND_SET_SHA = "17877e612b37bbbf"
EXPECTED_MISSING_SET_SHA = "e3b0c44298fc1c14"
EXPECTED_REASON_HISTOGRAM = {
    "no_geometry_in_package_family": 611,
    "no_vertexlit_reference": 45,
    "renders_via_host": 1452,
}
EXPECTED_GEOMETRY_RESOLUTION_HISTOGRAM = {
    "linked_hidden_base_mesh": 1452,
    "vertexlit_object": 5,
}

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


def count_in_stream_radius(locations: list[list[float]], spawn: list[float]) -> int:
    radius_sq = STREAM_RADIUS_CM * STREAM_RADIUS_CM
    return sum(
        1 for location in locations
        if ((location[0] - spawn[0]) ** 2
            + (location[1] - spawn[1]) ** 2
            + (location[2] - spawn[2]) ** 2) <= radius_sq
    )


GROUND_PERCENTILE = 0.10
SPAWN_CLEARANCE_CM = 250.0
STREAM_RADIUS_CM = 60000.0
# Placements the runtime actually streams: SpawnFromManifestNearEx measures 3D DistSquared,
# so this count collapses if spawn Z inflates. 1258 is the 2D ceiling - every placement
# whose XY is in range - so equality proves Z costs the stream radius nothing.
EXPECTED_IN_RADIUS_COUNT = 1258
# The per-block max(z)+250 expression reaches only this many and strands the pawn ~10.4km
# up. Pinned so a revert to that formula fails loudly instead of silently degrading.
NAIVE_MAX_Z_IN_RADIUS_COUNT = 1232


def main(argv: list[str]) -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--maps-dir", type=Path, default=DEFAULT_MAPS_DIR)
    parser.add_argument("--skip-extractor", action="store_true",
                        help="skip the 44-block re-extraction parity pass")
    args = parser.parse_args(argv)

    manifest_path = args.manifest.resolve()
    maps_dir = args.maps_dir.resolve()
    print(f"MAPS_DIR={maps_dir}")
    print(f"MANIFEST_FILE={manifest_path}")
    require(manifest_path.is_file(), f"manifest missing: {manifest_path}")

    with manifest_path.open(encoding="utf-8") as manifest_file:
        manifest = json.load(manifest_file)
    placements = manifest["placements"]

    require(manifest["district_id"] == "Financial",
            f"district_id={manifest['district_id']!r} expected 'Financial'")
    require(manifest["provenance"] == "real",
            f"provenance={manifest['provenance']!r} must stay 'real'")
    require(manifest["source_build"] == "retail",
            f"source_build={manifest['source_build']!r} must stay 'retail'")
    require(len(manifest["source_packages"]) == EXPECTED_BLOCK_COUNT,
            f"source_packages={len(manifest['source_packages'])} "
            f"expected={EXPECTED_BLOCK_COUNT}")
    require(len(manifest["block_coverage"]) == EXPECTED_BLOCK_COUNT,
            f"block_coverage={len(manifest['block_coverage'])} "
            f"expected={EXPECTED_BLOCK_COUNT}")
    print("GATE_DISTRICT_IDENTITY: PASS")

    source_ids = [row.get("source_id") for row in placements]
    require(all(isinstance(source_id, str) and source_id for source_id in source_ids),
            "every row must have a non-empty string source_id")
    require(len(source_ids) == len(set(source_ids)),
            f"source_id values must be unique across all 44 blocks: "
            f"{len(source_ids) - len(set(source_ids))} duplicates")
    for row in placements:
        require(isinstance(row.get("location"), list) and len(row["location"]) == 3 and
                all(isinstance(value, float) for value in row["location"]),
                f"location must be a three-float list for {row['source_id']}")
        for key in ("mesh_id", "ue_path", "package", "actor", "edge", "mesh_path",
                    "mesh_class", "geometry_source_mesh", "geometry_resolution",
                    "transform_source"):
            require(key in row, f"missing {key} for {row['source_id']}")
    print("GATE_SOURCE_ID_UNIQUE: PASS")

    visible = [row for row in placements
               if row["geometry_resolution"] != "not_source_visible"]
    bound = [row for row in visible if row["ue_path"] is not None]
    missing = [row for row in visible if row["ue_path"] is None]
    spawnable = [row for row in placements if "reason" not in row]
    require(len(visible) == EXPECTED_SOURCE_VISIBLE_COUNT,
            f"source_visible={len(visible)} expected={EXPECTED_SOURCE_VISIBLE_COUNT}")
    require(len(bound) == EXPECTED_GEOMETRY_BOUND_COUNT,
            f"geometry_bound={len(bound)} expected={EXPECTED_GEOMETRY_BOUND_COUNT}")
    require(len(missing) == EXPECTED_GEOMETRY_MISSING_COUNT,
            f"geometry_missing={len(missing)} expected={EXPECTED_GEOMETRY_MISSING_COUNT}")
    require(len(spawnable) == EXPECTED_SPAWNABLE_COUNT,
            f"spawnable={len(spawnable)} expected={EXPECTED_SPAWNABLE_COUNT}")
    require(len(bound) + len(missing) == len(visible),
            "visible rows must conserve as geometry_bound + geometry_missing")
    for key, expected in (("source_visible_placement_count", len(visible)),
                          ("geometry_bound_count", len(bound)),
                          ("geometry_missing_count", len(missing)),
                          ("renderable_count", len(spawnable))):
        require(manifest[key] == expected, f"top-level {key}={manifest[key]} rows={expected}")
    print("GATE_GEOMETRY_PROVENANCE_COUNTS: PASS")

    require(set_sha(bound) == EXPECTED_BOUND_SET_SHA,
            f"district geometry-bound identity changed: {set_sha(bound)} "
            f"!= {EXPECTED_BOUND_SET_SHA}")
    require(set_sha(missing) == EXPECTED_MISSING_SET_SHA,
            f"district geometry-missing identity changed: {set_sha(missing)} "
            f"!= {EXPECTED_MISSING_SET_SHA}")
    require(manifest["reason_histogram"] == EXPECTED_REASON_HISTOGRAM,
            f"reason_histogram={manifest['reason_histogram']} "
            f"expected={EXPECTED_REASON_HISTOGRAM}")
    require(manifest["geometry_resolution_histogram"]
            == EXPECTED_GEOMETRY_RESOLUTION_HISTOGRAM,
            f"geometry_resolution_histogram={manifest['geometry_resolution_histogram']} "
            f"expected={EXPECTED_GEOMETRY_RESOLUTION_HISTOGRAM}")
    print("GATE_ROW_IDENTITY: PASS")

    # A duplicate ue_path means two placements resolved to one mesh, which the runtime
    # dedup key cannot distinguish from an authoring mistake: equality with bound is the
    # only state proving every building kept its own geometry.
    distinct_paths = {row["ue_path"] for row in bound}
    require(len(distinct_paths) == EXPECTED_DISTINCT_UE_PATH_COUNT,
            f"distinct ue_path={len(distinct_paths)} "
            f"expected={EXPECTED_DISTINCT_UE_PATH_COUNT}")
    require(manifest["distinct_ue_path_count"] == len(distinct_paths),
            f"top-level distinct_ue_path_count={manifest['distinct_ue_path_count']} "
            f"rows={len(distinct_paths)}")
    require(len(distinct_paths) == len(bound),
            "every geometry-bound row must own a distinct ue_path")
    print("GATE_UE_PATH_DISTINCT: PASS")

    non_renderable_count = sum(manifest["reason_histogram"].values())
    require(manifest["total_row_count"] == EXPECTED_TOTAL_ROW_COUNT,
            f"total_row_count={manifest['total_row_count']} "
            f"expected={EXPECTED_TOTAL_ROW_COUNT}")
    require(manifest["total_row_count"] == len(placements),
            f"top-level rows={manifest['total_row_count']} placements={len(placements)}")
    require(manifest["total_row_count"] == len(spawnable) + non_renderable_count,
            "total_row_count must conserve spawnable and reason_histogram rows")
    require(non_renderable_count == sum("reason" in row for row in placements),
            "reason_histogram must account for every non-renderable row")
    print("GATE_ROW_CONSERVATION: PASS")

    rotation_rows = [row for row in placements if "rotation" in row]
    scale_rows = [row for row in placements if "scale" in row]
    require(not rotation_rows,
            f"district is cStreamedBuildingActor-only and must preserve rotation "
            f"absence, got {len(rotation_rows)} rotation keys")
    require(not scale_rows,
            f"district must preserve scale absence, got {len(scale_rows)} scale keys")
    yaws = [row["rotation"][2] for row in rotation_rows]
    require(not yaws or not all(yaw == (index * 11) % 360 for index, yaw in enumerate(yaws)),
            "legacy (i*11)%360 yaw ramp detected")
    require(not yaws or not is_arithmetic_ramp(yaws), "arithmetic yaw ramp detected")
    print("GATE_NO_RAMP: PASS")

    for key in ("player_start", "vehicle_start"):
        require(key in manifest, f"missing {key}")
        require(isinstance(manifest[key], list) and len(manifest[key]) == 3 and
                all(isinstance(value, (int, float)) and not isinstance(value, bool)
                    for value in manifest[key]),
                f"{key} must be a three-element numeric list")
    require(manifest.get("spawn_points_derived") is True,
            "spawn_points_derived must be True")
    require(isinstance(manifest.get("spawn_points_note"), str) and manifest["spawn_points_note"],
            "spawn_points_note must be a non-empty string")

    locations = [row["location"] for row in visible]
    centre_x = sum(location[0] for location in locations) / len(locations)
    centre_y = sum(location[1] for location in locations) / len(locations)
    radius_sq = STREAM_RADIUS_CM * STREAM_RADIUS_CM
    nearby = [location for location in locations
              if (location[0] - centre_x) ** 2 + (location[1] - centre_y) ** 2 <= radius_sq]
    require(bool(nearby), "district centroid must have placements within stream radius")
    ground_candidates = sorted(location[2] for location in nearby)
    ground_index = min(len(ground_candidates) - 1,
                       int(len(ground_candidates) * GROUND_PERCENTILE))
    expected_player = [centre_x, centre_y, ground_candidates[ground_index] + SPAWN_CLEARANCE_CM]
    expected_vehicle = [expected_player[0] + 600.0, expected_player[1] - 200.0,
                        expected_player[2] - 50.0]
    player_start = manifest["player_start"]
    vehicle_start = manifest["vehicle_start"]
    require(all(abs(actual - expected) < 1e-6
                for actual, expected in zip(player_start, expected_player)),
            f"player_start={player_start} must equal the grounded district derivation "
            f"{expected_player}")
    require(all(abs(actual - expected) < 1e-6
                for actual, expected in zip(vehicle_start, expected_vehicle)),
            f"vehicle_start={vehicle_start} must derive from player_start")

    min_x = min(location[0] for location in locations)
    max_x = max(location[0] for location in locations)
    min_y = min(location[1] for location in locations)
    max_y = max(location[1] for location in locations)
    require(min_x <= player_start[0] <= max_x and min_y <= player_start[1] <= max_y,
            "player_start X/Y must be inside the renderable placement bounding box")
    print("GATE_SPAWN_POINTS_DERIVED: PASS")

    max_z = max(location[2] for location in locations)
    naive_spawn = [centre_x, centre_y, max_z + SPAWN_CLEARANCE_CM]
    require(player_start[2] < max_z,
            f"player_start Z={player_start[2]} must sit below the tallest placement "
            f"Z={max_z}; a spawn above every building means the pawn free-falls")
    in_radius = count_in_stream_radius(locations, player_start)
    naive_in_radius = count_in_stream_radius(locations, naive_spawn)
    print(f"IN_RADIUS_GROUNDED={in_radius} IN_RADIUS_NAIVE_MAX_Z={naive_in_radius}")
    require(in_radius == EXPECTED_IN_RADIUS_COUNT,
            f"placements within stream radius={in_radius} "
            f"expected={EXPECTED_IN_RADIUS_COUNT}")
    require(naive_in_radius == NAIVE_MAX_Z_IN_RADIUS_COUNT,
            f"naive max(z)+250 in-radius={naive_in_radius} "
            f"expected={NAIVE_MAX_Z_IN_RADIUS_COUNT}")
    require(in_radius > naive_in_radius,
            "grounded spawn must stream strictly more placements than max(z)+250")
    print("GATE_SPAWN_GROUNDED: PASS")

    if args.skip_extractor:
        print("GATE_EXTRACTOR_PARITY: SKIPPED")
        return

    covered = sorted(manifest["source_packages"])
    extracted_total = 0
    extracted_rotation = 0
    extracted_scale = 0
    for stem in covered:
        records = ald.renderable_placements(stem, maps_dir)
        extracted_total += len(records)
        extracted_rotation += sum(bool(record["rotation_present"]) for record in records)
        extracted_scale += sum(bool(record["scale_present"]) for record in records)
    print(f"EXTRACTED_ROWS={extracted_total} MANIFEST_ROWS={len(placements)}")
    require(extracted_total == len(placements),
            f"re-extraction yields {extracted_total} rows but manifest has "
            f"{len(placements)}: manifest is not a faithful dump of the 44 packages")
    require(extracted_rotation == len(rotation_rows),
            f"rotation keys={len(rotation_rows)} extractor_present={extracted_rotation}")
    require(extracted_scale == len(scale_rows),
            f"scale keys={len(scale_rows)} extractor_present={extracted_scale}")
    print("GATE_EXTRACTOR_PARITY: PASS")


if __name__ == "__main__":
    main(sys.argv[1:])
