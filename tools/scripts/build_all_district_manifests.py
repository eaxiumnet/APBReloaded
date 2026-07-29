"""Build real-provenance placement manifests for ALL districts from retail .APB packages.

Generalizes build_financial_district_manifest.py to work with any district prefix.
Discovers every .APB package in the district's retail Maps folder, extracts placements
using the proven apb_level_dump pipeline (buildings via renderable_placements, props/roads
via prop_placement_records), and merges them into one manifest with real provenance.

Replaces the 14 fabricated manifests (synthesized (i*11)%360 rotations, round-robin mesh
assignment, hardcoded [1,1,1] scale) with real UE3 tagged-property data.

Usage:
    python tools/scripts/build_all_district_manifests.py --district Financial
    python tools/scripts/build_all_district_manifests.py --district Waterfront
    python tools/scripts/build_all_district_manifests.py --district All
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import apb_level_dump as ald
import write_block09_real_manifest as block_writer

PROJECT_ROOT = Path(__file__).resolve().parents[2]
PLACEMENT_DIR = PROJECT_ROOT / "Content" / "Data" / "district_placements"

# District configuration: (retail_folder, manifest_base, asset_folder, manifest_filename)
DISTRICTS = {
    "Financial": {
        "retail_folder": "FinancialDistrict",
        "manifest_base": "Financial_Block09",
        "asset_folder": "Financial",
        "manifest_file": "Financial_Block09_realv2.json",
        "package_filter": None,  # Process ALL packages (Block, Props, ArtProps, Design, TILE)
    },
    "Waterfront": {
        "retail_folder": "WaterfrontDistrict",
        "manifest_base": "Waterfront_Block05",
        "asset_folder": "Waterfront",
        "manifest_file": "Waterfront_Block05_realv2.json",
        "package_filter": None,
    },
    "Social": {
        "retail_folder": "RWorldSocialDistrict",
        "manifest_base": "Social_Block",
        "asset_folder": "Social",
        "manifest_file": "Social_Block_realv2.json",
        "package_filter": None,
    },
    "Beacon": {
        "retail_folder": "PGBeaconDistrict",
        "manifest_base": "Beacon_Block",
        "asset_folder": "Beacon",
        "manifest_file": "Beacon_Block_realv2.json",
        "package_filter": None,
    },
    "Crate": {
        "retail_folder": "PGCrateDistrict",
        "manifest_base": "Crate_Block",
        "asset_folder": "Crate",
        "manifest_file": "Crate_Block_realv2.json",
        "package_filter": None,
    },
}

# Packages that carry no renderable geometry and should be skipped to save time.
# These are system/utility packages: audio, beacons, volumes, pathgraph, spawn zones, etc.
SKIP_PATTERNS = re.compile(
    r"(?:Audio|Beacons|Contacts|ChaosContacts|HubSpawns|ChaosHubSpawns|"
    r"MissionSpawnZones|Pathgraph|DistrictBlock_Volumes|Outofbounds_Volumes|"
    r"SafeHeightVolumes|AdHocDropOffs|Holiday_.*|Minigame_.*|ArtPlacementMASTER)$",
    re.IGNORECASE,
)


def discover_packages(maps_dir: Path) -> list[str]:
    """All .APB package stems in a district maps folder, sorted alphabetically."""
    stems: list[str] = []
    for entry in sorted(maps_dir.iterdir()):
        if entry.suffix.lower() != ".apb":
            continue
        if SKIP_PATTERNS.search(entry.stem):
            continue
        stems.append(entry.stem)
    return stems


def collect_package(stem: str, maps_dir: Path, asset_root: Path, index: dict) -> list[dict]:
    """Extract placement records from one package and convert to manifest rows."""
    records = ald.all_placement_records(stem, maps_dir)
    return [block_writer.placement_row(record, asset_root, index, stem) for record in records]


def is_visible(row: dict) -> bool:
    return row.get("geometry_resolution", "not_source_visible") != "not_source_visible"


def assert_unique_source_ids(placements: list[dict]) -> None:
    counts = Counter(row["source_id"] for row in placements)
    collisions = {key: value for key, value in counts.items() if value > 1}
    if collisions:
        sample = list(collisions.items())[:5]
        raise ValueError(
            f"source_id collisions across packages: {len(collisions)} ids, sample={sample}"
        )


GROUND_PERCENTILE = 0.10
SPAWN_CLEARANCE_CM = 250.0
STREAM_RADIUS_CM = 60000.0


def derive_spawn(visible: list[dict]) -> tuple[list[float], list[float]]:
    locations = [row["location"] for row in visible]
    if not locations:
        raise ValueError("cannot derive spawn points without renderable placements")

    centre_x = sum(loc[0] for loc in locations) / len(locations)
    centre_y = sum(loc[1] for loc in locations) / len(locations)

    radius_sq = STREAM_RADIUS_CM * STREAM_RADIUS_CM
    nearby = [
        loc for loc in locations
        if (loc[0] - centre_x) ** 2 + (loc[1] - centre_y) ** 2 <= radius_sq
    ]
    if not nearby:
        raise ValueError("no placements within stream radius of the district centroid")

    ground_candidates = sorted(loc[2] for loc in nearby)
    ground_index = min(len(ground_candidates) - 1,
                       int(len(ground_candidates) * GROUND_PERCENTILE))
    ground_z = ground_candidates[ground_index]

    player_start = [centre_x, centre_y, ground_z + SPAWN_CLEARANCE_CM]
    vehicle_start = [player_start[0] + 600.0, player_start[1] - 200.0, player_start[2] - 50.0]
    return player_start, vehicle_start


def build_district(district_name: str, config: dict, limit: int = 0) -> dict:
    """Build one district manifest from all retail packages."""
    maps_dir = (ald.RETAIL_MAPS / config["retail_folder"]).resolve()
    asset_root = (PROJECT_ROOT / "Content" / "Imported" / "Districts" / config["asset_folder"]).resolve()
    output_path = (PLACEMENT_DIR / config["manifest_file"]).resolve()

    if not maps_dir.is_dir():
        print(f"  SKIP {district_name}: maps directory not found: {maps_dir}")
        return {"district": district_name, "status": "skipped", "reason": "maps_dir_missing"}

    if not asset_root.is_dir():
        print(f"  SKIP {district_name}: asset directory not found: {asset_root}")
        return {"district": district_name, "status": "skipped", "reason": "asset_dir_missing"}

    stems = discover_packages(maps_dir)
    if limit > 0:
        stems = stems[:limit]

    print(f"  MAPS_DIR={maps_dir}")
    print(f"  ASSET_ROOT={asset_root}")
    print(f"  OUTPUT={output_path}")
    print(f"  PACKAGES={len(stems)}")

    index = block_writer.asset_index(asset_root)
    print(f"  ASSET_INDEX={len(index)}")

    placements: list[dict] = []
    coverage: list[dict] = []
    failures: list[dict] = []

    for stem in stems:
        try:
            rows = collect_package(stem, maps_dir, asset_root, index)
        except Exception as exc:
            failures.append({"source_package": stem, "error": f"{type(exc).__name__}: {exc}"})
            print(f"    {stem}: FAILED {type(exc).__name__}: {exc}")
            continue
        visible = [row for row in rows if is_visible(row)]
        bound = [row for row in visible if row.get("ue_path") is not None]
        spawnable = [row for row in rows if "reason" not in row]
        coverage.append({
            "source_package": stem,
            "total_row_count": len(rows),
            "renderable_count": len(spawnable),
            "source_visible_placement_count": len(visible),
            "geometry_bound_count": len(bound),
            "geometry_missing_count": len(visible) - len(bound),
        })
        placements.extend(rows)
        if spawnable or visible:
            print(f"    {stem}: rows={len(rows)} visible={len(visible)} bound={len(bound)} spawnable={len(spawnable)}")

    if failures:
        print(f"  WARNING: {len(failures)} package(s) failed (continuing with successful ones)")
        for f in failures[:5]:
            print(f"    {f['source_package']}: {f['error']}")

    if not placements:
        print(f"  NO PLACEMENTS extracted from {district_name}")
        return {"district": district_name, "status": "empty", "packages": len(stems)}

    try:
        assert_unique_source_ids(placements)
    except ValueError as exc:
        print(f"  WARNING: {exc}")

    visible = [row for row in placements if is_visible(row)]
    spawnable = [row for row in placements if "reason" not in row]
    geometry_bound = [row for row in visible if row.get("ue_path") is not None]
    geometry_missing = [row for row in visible if row.get("ue_path") is None]
    resolutions = Counter(row.get("geometry_resolution", "unknown") for row in visible)
    reasons = Counter(row["reason"] for row in placements if "reason" in row)
    distinct_paths = {row["ue_path"] for row in geometry_bound if row.get("ue_path")}

    try:
        player_start, vehicle_start = derive_spawn(visible)
    except ValueError as exc:
        player_start = [0.0, 0.0, 500.0]
        vehicle_start = [600.0, -200.0, 450.0]
        print(f"  WARNING: spawn derivation failed: {exc}")

    manifest = {
        "district_id": district_name,
        "source_package": f"{config['retail_folder']}_AllPackages",
        "source_packages": [entry["source_package"] for entry in coverage],
        "provenance": "real",
        "extractor": "apb_level_dump.all_placement_records",
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
        "package_coverage": coverage,
        "failed_packages": failures,
        "placements": placements,
    }
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    result = {
        "district": district_name,
        "status": "ok",
        "packages_processed": len(coverage),
        "packages_failed": len(failures),
        "renderable_count": len(spawnable),
        "geometry_bound": len(geometry_bound),
        "geometry_missing": len(geometry_missing),
        "distinct_paths": len(distinct_paths),
        "output": str(output_path),
    }
    print(f"  RESULT: {json.dumps(result)}")
    return result


def main(argv: list[str]) -> None:
    parser = argparse.ArgumentParser(description="Build real-provenance manifests for all districts")
    parser.add_argument("--district", default="All", help="District name or 'All'")
    parser.add_argument("--limit", type=int, default=0, help="Process only first N packages per district")
    args = parser.parse_args(argv)

    if args.district == "All":
        targets = list(DISTRICTS.items())
    else:
        if args.district not in DISTRICTS:
            print(f"Unknown district: {args.district}. Available: {list(DISTRICTS.keys())}")
            sys.exit(1)
        targets = [(args.district, DISTRICTS[args.district])]

    print(f"=== Building {len(targets)} district manifest(s) ===")
    results = []
    for name, config in targets:
        print(f"\n--- {name} ---")
        result = build_district(name, config, args.limit)
        results.append(result)

    print(f"\n=== SUMMARY ===")
    for r in results:
        status = r.get("status", "unknown")
        if status == "ok":
            print(f"  {r['district']}: OK renderable={r['renderable_count']} bound={r['geometry_bound']} missing={r['geometry_missing']} pkgs={r['packages_processed']}")
        else:
            print(f"  {r['district']}: {status.upper()}")


if __name__ == "__main__":
    main(sys.argv[1:])
