from __future__ import annotations

import argparse
import hashlib
import json
from datetime import date
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--ledger", type=Path, default=ROOT / "tools" / "import_ledger.json")
    args = parser.parse_args()

    evidence = json.loads(args.evidence.read_text(encoding="utf-8"))
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    ledger = json.loads(args.ledger.read_text(encoding="utf-8"))
    entries = {entry["asset_key"]: entry for entry in ledger.get("entries", [])}
    district = evidence["district"]
    evidence_path = args.evidence.resolve().relative_to(ROOT).as_posix()
    evidence_sha256 = sha256(args.evidence.resolve())

    placement_root = ROOT / "Content" / "Data" / "district_placements"
    placement_files = [
        path.resolve()
        for path in sorted(placement_root.glob(f"{district}*.json"))
    ] + [
        path.resolve()
        for path in sorted(placement_root.glob(f"{district.lower()}*.json"))
    ]
    if args.manifest.resolve() not in placement_files:
        placement_files.insert(0, args.manifest.resolve())
    all_rows: list[dict] = []
    seen: set[tuple[str, str]] = set()
    for placement_file in placement_files:
        try:
            placement_data = json.loads(placement_file.read_text(encoding="utf-8"))
        except Exception:
            continue
        for row in placement_data.get("placements", []) if isinstance(placement_data, dict) else []:
            mesh_path = str(row.get("mesh_path", "")).lower()
            if mesh_path and "." in mesh_path:
                key = (mesh_path.rsplit(".", 1)[0], mesh_path.rsplit(".", 1)[-1])
                if key in seen:
                    continue
                seen.add(key)
            else:
                ue = str(row.get("ue_path", "")).lower()
                if ue in seen:
                    continue
                seen.add(ue)
            all_rows.append(row)

    recorded_dests: set[str] = set()
    for record in evidence.get("meshes", []):
        package = record["source_package"]
        object_name = record["source_object"]
        bindings = [
            row for row in all_rows
            if str(row.get("mesh_path", "")).lower() == f"{package}.{object_name}".lower()
        ]
        if not bindings:
            bindings = [
                row for row in all_rows
                if str(row.get("mesh_path", "")).lower().split(".", 1)[0] == package.lower()
                and str(row.get("mesh_path", "")).lower().rsplit(".", 1)[-1] == object_name.lower()
            ]
        if not bindings:
            bindings = [
                row for row in all_rows
                if str(row.get("ue_path", "")).rsplit("/", 1)[-1].split(".", 1)[0].lower()
                == object_name.lower()
            ]
        if not bindings:
            raise ValueError(f"no placement binding for {package}.{object_name}")
        ue_leaf = str(bindings[0].get("ue_path", "")).rsplit("/", 1)[-1].split(".", 1)[0]
        if not ue_leaf:
            raise ValueError(f"no ue_path leaf for {package}.{object_name}")
        uasset = ROOT / "Content" / "Imported" / "Districts" / district / f"{ue_leaf}.uasset"
        if not uasset.is_file():
            raise ValueError(f"unverified district uasset for {package}.{object_name}: {ue_leaf}")
        dest = f"/Game/Imported/Districts/{district}/{ue_leaf}"
        if dest in recorded_dests:
            continue
        recorded_dests.add(dest)
        locator = record["source_package_locator"]
        package_relative = locator.split("/Packages/", 1)[-1]
        asset_key = f"retail:{package_relative}#{object_name}"
        entries[asset_key] = {
            "asset_key": asset_key,
            "source_build": "retail",
            "source_locator": locator,
            "source_package": package,
            "source_object": object_name,
            "source_sha256": record["source_package_sha256"],
            "extractor": "tools/UEViewer/umodel_64.exe",
            "extractor_args": "legacy payload extraction settings not retained",
            "extracted_path": record["extracted_file"],
            "extracted_sha256": record["extracted_sha256"],
            "conversion_settings": {
                "converter": record["converter"],
                "format": "OBJ",
                "normalize": False,
            },
            "intermediate_path": record["output_file"],
            "intermediate_sha256": record["output_sha256"],
            "dest": dest,
            "asset_class": "StaticMesh",
            "status": "imported",
            "validation": {
                "uasset_exists": True,
                "manifest_binding_count": len(bindings),
                "vertex_count": record["vertex_count"],
                "face_count": record["face_count"],
            },
            "d17_evidence": [
                {
                    "record_key": asset_key,
                    "schema": "apb_district_placement_mesh_v1",
                    "path": evidence_path,
                    "sha256": evidence_sha256,
                    "fields": {
                        "source_package": package,
                        "source_object": object_name,
                        "source_sha256": record["source_package_sha256"],
                        "extracted_file": record["extracted_file"],
                        "extracted_sha256": record["extracted_sha256"],
                        "output_file": record["output_file"],
                        "output_sha256": record["output_sha256"],
                        "vertex_count": record["vertex_count"],
                        "face_count": record["face_count"],
                    },
                }
            ],
            "updated": date.today().isoformat(),
        }

    ledger["entries"] = list(entries.values())
    ledger["updated"] = date.today().isoformat()
    args.ledger.write_text(json.dumps(ledger, indent=2) + "\n", encoding="utf-8")
    print(
        f"DISTRICT_MESH_LEDGER_PASS district={district} meshes={len(evidence.get('meshes', []))} "
        f"entries={len(ledger['entries'])}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
