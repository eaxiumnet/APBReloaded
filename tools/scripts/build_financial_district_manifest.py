"""Build ONE district-wide Financial placement manifest from all 44 retail blocks.

APBDistrictPlacementLoader::LoadManifestForDistrict returns on the FIRST successful
candidate, so a district cannot be assembled from per-block files. Every
`FinancialDistrict_Block<NN>` package is therefore resolved with the same accurate
pipeline proven on a single block (apb_level_dump.renderable_placements) and merged
into one manifest.

Scope: `Block<NN>` packages only. Measured on Block09, the sibling packages carry no
cStreamedBuildingActor at all (Props_=cProp/cStreamedLightingStaticMeshActor,
ArtProps_=lights/decals, _Design=checkpoints/brushes), so they are a different
extractor class and not part of the building spine.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path

import apb_level_dump as ald
import write_block09_real_manifest as block_writer

PROJECT_ROOT = Path(__file__).resolve().parents[2]
BLOCK_STEM = re.compile(r"^FinancialDistrict_Block(\d+)$")
OUTPUT_FILE = (
    PROJECT_ROOT / "Content" / "Data" / "district_placements" / "Financial_Block09_realv2.json"
)


def discover_block_stems(maps_dir: Path) -> list[str]:
    """Retail Block packages, ordered by block number."""
    stems: dict[int, str] = {}
    for entry in maps_dir.iterdir():
        match = BLOCK_STEM.match(entry.stem)
        if match is not None:
            stems[int(match.group(1))] = entry.stem
    return [stems[key] for key in sorted(stems)]


def collect_block(stem: str, maps_dir: Path, asset_root: Path, index: dict) -> list[dict]:
    records = ald.renderable_placements(stem, maps_dir)
    return [block_writer.placement_row(record, asset_root, index, stem) for record in records]


def is_visible(row: dict) -> bool:
    return row["geometry_resolution"] != "not_source_visible"


def assert_unique_source_ids(placements: list[dict]) -> None:
    """PlacementDedupKey keys on source_id; a collision would silently drop a building.

    APBDistrictPlacement.h also rejects the whole manifest on a duplicate key, so this
    must fail loudly at build time rather than at runtime.
    """
    counts = Counter(row["source_id"] for row in placements)
    collisions = {key: value for key, value in counts.items() if value > 1}
    if collisions:
        sample = list(collisions.items())[:5]
        raise ValueError(
            f"source_id collisions across blocks: {len(collisions)} ids, sample={sample}"
        )


GROUND_PERCENTILE = 0.10
SPAWN_CLEARANCE_CM = 250.0
STREAM_RADIUS_CM = 60000.0


def derive_spawn(visible: list[dict]) -> tuple[list[float], list[float]]:
    """District spawn: centroid in XY, local street level in Z.

    A single block can spawn at max(z)+250 because ~57 buildings sit in one tight
    cluster, so that is just above the block's roofline. District-wide that same
    expression selects the tallest tower in all 44 blocks: measured Z spans
    -800..10400 while p10/p50 are both 0, so max(z)+250 lifts the spawn 10.4km into
    the air. Nothing mitigates it downstream - AlignPlayerStartsAndTeleport applies
    FMath::Max(At.Z, 120.f), which only raises a low Z and never lowers a high one,
    so the pawn would free-fall. SpawnFromManifestNearEx also measures the stream
    radius with 3D DistSquared, so an inflated Z spends the 600m budget vertically:
    measured 1232 placements in radius at Z=10650 versus 1258 (the 2D ceiling) when
    grounded. Z is a low percentile near the centroid: robust to both the sunken
    (-800) and tower (10400) outliers.
    """
    locations = [row["location"] for row in visible]
    if not locations:
        raise ValueError("cannot derive spawn points without renderable placements")

    centre_x = sum(location[0] for location in locations) / len(locations)
    centre_y = sum(location[1] for location in locations) / len(locations)

    radius_sq = STREAM_RADIUS_CM * STREAM_RADIUS_CM
    nearby = [
        location for location in locations
        if (location[0] - centre_x) ** 2 + (location[1] - centre_y) ** 2 <= radius_sq
    ]
    # Centroid of a contiguous district always has neighbours; empty means the
    # placement set is not a district and the caller should see it, not a silent 0.
    if not nearby:
        raise ValueError("no placements within stream radius of the district centroid")

    ground_candidates = sorted(location[2] for location in nearby)
    ground_index = min(len(ground_candidates) - 1,
                       int(len(ground_candidates) * GROUND_PERCENTILE))
    ground_z = ground_candidates[ground_index]

    player_start = [centre_x, centre_y, ground_z + SPAWN_CLEARANCE_CM]
    vehicle_start = [player_start[0] + 600.0, player_start[1] - 200.0, player_start[2] - 50.0]
    return player_start, vehicle_start


def main(argv: list[str]) -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--maps-dir", type=Path, default=ald.RETAIL_MAPS / "FinancialDistrict")
    parser.add_argument("--asset-root", type=Path, default=block_writer.ASSET_ROOT)
    parser.add_argument("--output", type=Path, default=OUTPUT_FILE)
    parser.add_argument("--limit", type=int, default=0, help="process only the first N blocks")
    args = parser.parse_args(argv)

    maps_dir = args.maps_dir.resolve()
    asset_root = args.asset_root.resolve()
    output_path = args.output.resolve()
    if not maps_dir.is_dir():
        raise FileNotFoundError(f"maps directory missing: {maps_dir}")
    if not asset_root.is_dir():
        raise FileNotFoundError(f"asset directory missing: {asset_root}")

    stems = discover_block_stems(maps_dir)
    if args.limit > 0:
        stems = stems[: args.limit]
    print(f"MAPS_DIR={maps_dir}")
    print(f"ASSET_ROOT={asset_root}")
    print(f"OUTPUT_FILE={output_path}")
    print(f"BLOCK_PACKAGES={len(stems)}")

    index = block_writer.asset_index(asset_root)
    print(f"ASSET_INDEX_SIZE={len(index)}")

    placements: list[dict] = []
    coverage: list[dict] = []
    failures: list[dict] = []
    for stem in stems:
        try:
            rows = collect_block(stem, maps_dir, asset_root, index)
        except Exception as exc:  # noqa: BLE001 - one bad block must not hide the other 43
            failures.append({"source_package": stem, "error": f"{type(exc).__name__}: {exc}"})
            print(f"  {stem}: FAILED {type(exc).__name__}: {exc}")
            continue
        visible = [row for row in rows if is_visible(row)]
        bound = [row for row in visible if row["ue_path"] is not None]
        coverage.append({
            "source_package": stem,
            "total_row_count": len(rows),
            "renderable_count": len([row for row in rows if "reason" not in row]),
            "source_visible_placement_count": len(visible),
            "geometry_bound_count": len(bound),
            "geometry_missing_count": len(visible) - len(bound),
        })
        placements.extend(rows)
        print(f"  {stem}: rows={len(rows)} visible={len(visible)} bound={len(bound)}")

    if failures:
        raise RuntimeError(f"{len(failures)} block package(s) failed: {failures}")

    assert_unique_source_ids(placements)

    visible = [row for row in placements if is_visible(row)]
    spawnable = [row for row in placements if "reason" not in row]
    geometry_bound = [row for row in visible if row["ue_path"] is not None]
    geometry_missing = [row for row in visible if row["ue_path"] is None]
    resolutions = Counter(row["geometry_resolution"] for row in visible)
    reasons = Counter(row["reason"] for row in placements if "reason" in row)
    distinct_paths = {row["ue_path"] for row in geometry_bound}
    player_start, vehicle_start = derive_spawn(visible)

    manifest = {
        "district_id": "Financial",
        "source_package": "FinancialDistrict_AllBlocks",
        "source_packages": [entry["source_package"] for entry in coverage],
        "provenance": "real",
        "extractor": "apb_level_dump.renderable_placements",
        "source_build": "retail",
        "renderable_count": len(spawnable),
        "source_visible_placement_count": len(visible),
        "geometry_bound_count": len(geometry_bound),
        "geometry_missing_count": len(geometry_missing),
        "geometry_resolution_histogram": dict(sorted(resolutions.items())),
        "distinct_ue_path_count": len(distinct_paths),
        "total_row_count": len(placements),
        "reason_histogram": dict(sorted(reasons.items())),
        "player_start": player_start,
        "vehicle_start": vehicle_start,
        "spawn_points_derived": True,
        "spawn_points_note": (
            "Derived from the district-wide renderable placement centroid; not read from"
            " any source package."
        ),
        "block_coverage": coverage,
        "placements": placements,
    }
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print(f"BLOCKS_MERGED={len(coverage)}")
    print(f"WROTE renderable_count={len(spawnable)} total_row_count={len(placements)}")
    print(f"GEOMETRY_BOUND={len(geometry_bound)} GEOMETRY_MISSING={len(geometry_missing)}")
    print(f"DISTINCT_UE_PATHS={len(distinct_paths)} (bound={len(geometry_bound)})")
    print(f"RESOLUTION_HISTOGRAM={json.dumps(manifest['geometry_resolution_histogram'], sort_keys=True)}")
    print(f"REASON_HISTOGRAM={json.dumps(manifest['reason_histogram'], sort_keys=True)}")
    print(f"PLAYER_START={json.dumps(player_start)}")
    print(f"VEHICLE_START={json.dumps(vehicle_start)}")


if __name__ == "__main__":
    main(sys.argv[1:])
