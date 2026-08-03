from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
import sys
import wave
from collections import Counter
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_MESH_MANIFEST = ROOT / "work" / "evidence" / "g1_payload_batch2" / "g1_payload_batch2_import_manifest.json"
DEFAULT_LOADED_VALIDATION = ROOT / "work" / "evidence" / "g1_payload_batch2" / "g1_payload_batch2_loaded_validation.json"
DEFAULT_LEDGER = ROOT / "tools" / "import_ledger.json"
DEFAULT_PLACEMENT_MANIFEST = ROOT / "Content" / "Data" / "district_placements" / "Social_Block_realv2.json"
DEFAULT_REPORT = ROOT / "work" / "evidence" / "m3r_semantic_parity_report.json"
DEFAULT_ORACLE = ROOT / "Content" / "Data" / "fidelity" / "fidelity_oracle_manifest.json"
DEFAULT_UI_LAYOUT_TEST = ROOT / "tests" / "run_layout_math_tests.cpp"
DEFAULT_UI_CAPTURE = ROOT / "work" / "logs" / "fidelity" / "frontend_login_diff.json"
EXPECTED_COLLISION = {
    "body_setup": "None",
    "use_simple_box_collision": False,
    "use_simple_line_collision": False,
    "use_simple_rigid_body_collision": False,
}
REQUIRED_ATTRIBUTES = {"POSITION", "NORMAL", "TANGENT"}


class SemanticParityError(ValueError):
    pass


def read_json(path: Path) -> dict[str, Any]:
    if not path.is_file():
        raise SemanticParityError(f"missing JSON input: {path}")
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise SemanticParityError(f"invalid JSON input: {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise SemanticParityError(f"JSON root is not an object: {path}")
    return value


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as handle:
            for block in iter(lambda: handle.read(1024 * 1024), b""):
                digest.update(block)
    except OSError as exc:
        raise SemanticParityError(f"cannot read evidence file: {path}: {exc}") from exc
    return digest.hexdigest()


def root_path(relative: str) -> Path:
    path = (ROOT / relative).resolve()
    try:
        path.relative_to(ROOT)
    except ValueError as exc:
        raise SemanticParityError(f"evidence path escapes project root: {relative}") from exc
    return path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SemanticParityError(message)


def number_vector(value: Any, length: int, name: str) -> None:
    require(
        isinstance(value, list)
        and len(value) == length
        and all(isinstance(item, (int, float)) and not isinstance(item, bool)
                and math.isfinite(float(item)) for item in value),
        f"{name} must be a finite numeric vector of length {length}",
    )


def gltf_contract(path: Path) -> dict[str, Any]:
    document = read_json(path)
    accessors = document.get("accessors")
    materials = document.get("materials", [])
    meshes = document.get("meshes")
    require(isinstance(accessors, list) and isinstance(meshes, list),
            f"glTF structural arrays are missing: {path}")
    primitives: list[dict[str, Any]] = []
    for mesh in meshes:
        require(isinstance(mesh, dict) and isinstance(mesh.get("primitives"), list),
                f"glTF mesh primitives are invalid: {path}")
        for primitive in mesh["primitives"]:
            require(isinstance(primitive, dict), f"glTF primitive is not an object: {path}")
            primitives.append(primitive)
    require(primitives, f"glTF contains no primitives: {path}")
    triangle_count = 0
    position_counts: list[int] = []
    attributes: set[str] = set()
    material_slots: list[dict[str, Any]] = []
    for section_index, primitive in enumerate(primitives):
        require(isinstance(primitive, dict), f"glTF primitive is not an object: {path}")
        primitive_attributes = primitive.get("attributes")
        require(isinstance(primitive_attributes, dict)
                and isinstance(primitive_attributes.get("POSITION"), int),
                f"glTF POSITION attribute is missing: {path}")
        attributes.update(primitive_attributes)
        position_accessor_index = primitive_attributes["POSITION"]
        require(0 <= position_accessor_index < len(accessors),
                f"glTF POSITION accessor is invalid: {path}")
        position_accessor = accessors[position_accessor_index]
        index_accessor_index = primitive.get("indices")
        require(isinstance(index_accessor_index, int)
                and 0 <= index_accessor_index < len(accessors),
                f"glTF index accessor is invalid: {path}")
        index_count = accessors[index_accessor_index].get("count")
        position_count = position_accessor.get("count")
        require(isinstance(index_count, int) and index_count > 0 and index_count % 3 == 0,
                f"glTF topology is not triangular: {path}")
        require(isinstance(position_count, int) and position_count > 0,
                f"glTF POSITION count is invalid: {path}")
        triangle_count += index_count // 3
        position_counts.append(position_count)
        material_index = primitive.get("material")
        material_name = None
        if isinstance(material_index, int):
            require(0 <= material_index < len(materials),
                    f"glTF material index is invalid: {path}")
            material_name = materials[material_index].get("name")
        material_slots.append({
            "section_index": section_index,
            "source_material_index": material_index,
            "source_material_name": material_name,
        })
    return {
        "triangle_count": triangle_count,
        "position_accessor_counts": position_counts,
        "attributes": sorted(attributes),
        "uv_sets": sorted(attribute for attribute in attributes
                          if attribute.startswith("TEXCOORD_")),
        "material_slots": material_slots,
    }


def validate_mesh_manifest(
    manifest: dict[str, Any],
    loaded: dict[str, Any],
    ledger: dict[str, Any] | None = None,
) -> dict[str, Any]:
    require(manifest.get("schema") == "apb_g1_payload_import_manifest_v1",
            "mesh manifest schema is not the accepted G1 schema")
    require(manifest.get("source_build") == "retail", "mesh source_build must be retail")
    manifest_entries = manifest.get("entries")
    require(isinstance(manifest_entries, list) and manifest_entries,
            "mesh manifest entries must be a non-empty list")
    require(manifest.get("object_count") == len(manifest_entries),
            "mesh object_count does not conserve entries")
    manifest_index: dict[str, dict[str, Any]] = {}
    source_objects: set[str] = set()
    ledger_index = {
        entry.get("asset_key"): entry
        for entry in (ledger or {}).get("entries", [])
        if isinstance(entry, dict)
    }
    for entry in manifest_entries:
        require(isinstance(entry, dict), "mesh manifest entry is not an object")
        asset_key = entry.get("asset_key")
        require(isinstance(asset_key, str) and asset_key and asset_key not in manifest_index,
                f"mesh asset_key is missing or duplicated: {asset_key!r}")
        manifest_index[asset_key] = entry
        source_object = entry.get("source_object")
        require(isinstance(source_object, str) and source_object and source_object not in source_objects,
                f"mesh source_object is missing or duplicated: {source_object!r}")
        source_objects.add(source_object)
        require(entry.get("runtime_eligible") is False,
                f"mesh row must remain runtime-ineligible: {asset_key}")
        require(isinstance(entry.get("source_locator"), str)
                and "/APBGame/Content/Release/Packages/" in entry["source_locator"],
                f"mesh source locator is not a retail package: {asset_key}")
        contract = entry.get("mesh_contract")
        require(isinstance(contract, dict), f"missing mesh contract: {asset_key}")
        attributes = contract.get("attributes")
        require(isinstance(attributes, list) and REQUIRED_ATTRIBUTES.issubset(attributes),
                f"mesh vertex attributes are incomplete: {asset_key}")
        require(contract.get("has_normals") is True and contract.get("has_tangents") is True,
                f"mesh normal/tangent contract is incomplete: {asset_key}")
        triangle_count = contract.get("triangle_count")
        require(isinstance(triangle_count, int) and triangle_count > 0,
                f"mesh triangle count is invalid: {asset_key}")
        position_counts = contract.get("position_accessor_counts")
        require(isinstance(position_counts, list) and position_counts
                and all(isinstance(item, int) and item > 0 for item in position_counts),
                f"mesh position accessor counts are invalid: {asset_key}")
        require("TEXCOORD_0" in contract.get("uv_sets", []),
                f"mesh UV contract is incomplete: {asset_key}")
        slots = contract.get("material_slots")
        require(isinstance(slots, list) and slots, f"mesh material slots are missing: {asset_key}")
        for expected_index, slot in enumerate(slots):
            require(isinstance(slot, dict), f"mesh material slot is not an object: {asset_key}")
            require(slot.get("section_index") == expected_index,
                    f"mesh material section order changed: {asset_key}")
            require(isinstance(slot.get("source_material_name"), str)
                    and slot["source_material_name"],
                    f"mesh material slot name is missing: {asset_key}")
        require(entry.get("source_collision") == EXPECTED_COLLISION,
                f"mesh collision contract changed: {asset_key}")
        gltf_record = entry.get("gltf")
        require(isinstance(gltf_record, dict) and isinstance(gltf_record.get("path"), str),
                f"mesh glTF evidence is incomplete: {asset_key}")
        gltf_path = root_path(gltf_record["path"])
        require(gltf_path.is_file() and sha256(gltf_path) == gltf_record.get("sha256"),
                f"mesh glTF hash mismatch: {asset_key}")
        try:
            parsed_contract = gltf_contract(gltf_path)
        except (IndexError, KeyError, TypeError, AttributeError) as exc:
            raise SemanticParityError(f"mesh glTF is malformed: {asset_key}: {exc}") from exc
        require(parsed_contract["triangle_count"] == triangle_count
                and parsed_contract["position_accessor_counts"] == position_counts
                and parsed_contract["material_slots"] == slots,
                f"mesh glTF contract mismatch: {asset_key}")
        require(parsed_contract["attributes"] == sorted(attributes),
                f"mesh glTF attributes changed: {asset_key}")
        for field in ("binary", "source_mesh_properties"):
            record = entry.get(field)
            require(isinstance(record, dict) and isinstance(record.get("path"), str)
                    and isinstance(record.get("sha256"), str),
                    f"mesh {field} evidence is incomplete: {asset_key}")
            evidence_path = root_path(record["path"])
            require(evidence_path.is_file(), f"mesh {field} file is missing: {asset_key}")
            require(sha256(evidence_path) == record["sha256"],
                    f"mesh {field} hash mismatch: {asset_key}")
        destination = entry.get("destination_asset")
        require(isinstance(destination, str) and destination.startswith("/Game/Imported/Districts/"),
                f"mesh destination is not an imported project path: {asset_key}")
        destination_file = entry.get("destination_file")
        require(isinstance(destination_file, str),
                f"mesh destination file is missing: {asset_key}")
        destination_path = root_path(destination_file)
        require(destination_path.is_file(), f"mesh destination file is missing: {asset_key}")
        ledger_entry = ledger_index.get(asset_key)
        require(ledger_entry is not None,
                f"mesh destination is not ledger-bound: {asset_key}")
        require(sha256(destination_path) == ledger_entry.get("uasset_sha256"),
                f"mesh destination hash mismatch: {asset_key}")

    require(loaded.get("schema") == "apb_g1_payload_loaded_validation_v1",
            "loaded mesh validation schema is not accepted")
    require(loaded.get("status") == "ok" and loaded.get("failure_count") == 0,
            "loaded mesh validation is not successful")
    require(loaded.get("object_count") == len(manifest_entries),
            "loaded mesh object count does not match import manifest")
    require(loaded.get("expected_object_count") == len(manifest_entries),
            "loaded mesh expected object count is not pinned to manifest")
    expected_section_count = sum(
        len(entry["mesh_contract"]["material_slots"]) for entry in manifest_entries
    )
    require(loaded.get("total_section_identity_count") == expected_section_count
            and loaded.get("unique_section_identity_count") == expected_section_count,
            "loaded mesh section identity totals changed")
    require(loaded.get("weed_clump_shared_slot_ok") is True,
            "loaded Weed_Clump shared-slot contract failed")
    loaded_entries = loaded.get("entries")
    require(isinstance(loaded_entries, list) and len(loaded_entries) == len(manifest_entries),
            "loaded mesh entries are incomplete")
    loaded_index: dict[str, dict[str, Any]] = {}
    for entry in loaded_entries:
        require(isinstance(entry, dict), "loaded mesh entry is not an object")
        asset_key = entry.get("asset_key")
        require(isinstance(asset_key, str) and asset_key not in loaded_index,
                f"loaded mesh asset_key is missing or duplicated: {asset_key!r}")
        loaded_index[asset_key] = entry
    require(set(loaded_index) == set(manifest_index),
            "loaded mesh asset keys do not match import manifest")

    total_sections = 0
    unique_slots: set[str] = set()
    for asset_key, manifest_entry in manifest_index.items():
        loaded_entry = loaded_index[asset_key]
        expected_names = [slot["source_material_name"]
                          for slot in manifest_entry["mesh_contract"]["material_slots"]]
        expected_metadata = {
            "APBSourceBuild": manifest_entry["source_build"] if "source_build" in manifest_entry else manifest["source_build"],
            "APBSourcePackage": manifest_entry["source_locator"],
            "APBSourceObject": manifest_entry["source_object"],
            "APBSourceSHA256": manifest_entry["source_sha256"],
            "APBIntermediateSHA256": manifest_entry["gltf"]["sha256"],
            "APBRuntimeEligible": "false",
        }
        require(loaded_entry.get("metadata_matches") is True
                and loaded_entry.get("metadata") == expected_metadata
                and loaded_entry.get("expected_metadata") == expected_metadata,
                f"loaded mesh metadata mismatch: {asset_key}")
        require(loaded_entry.get("asset_class") == "StaticMesh",
                f"loaded mesh class is not StaticMesh: {asset_key}")
        require(loaded_entry.get("destination_asset") == manifest_entry["destination_asset"]
                and loaded_entry.get("destination_file") == manifest_entry["destination_file"],
                f"loaded mesh destination identity mismatch: {asset_key}")
        require(loaded_entry.get("uasset_sha_matches_ledger") is True,
                f"loaded mesh uasset hash is not ledger-bound: {asset_key}")
        require(loaded_entry.get("material_interfaces_cleared") is True,
                f"loaded mesh material interface was not cleared: {asset_key}")
        require(loaded_entry.get("runtime_eligible") == "false",
                f"loaded mesh runtime eligibility changed: {asset_key}")
        require(loaded_entry.get("nanite_enabled") is False,
                f"loaded mesh Nanite setting changed: {asset_key}")
        require(loaded_entry.get("simple_collision_count") == 0
                and loaded_entry.get("convex_collision_count") == 0,
                f"loaded mesh collision is present: {asset_key}")
        settings = loaded_entry.get("build_settings")
        require(isinstance(settings, dict)
                and all(value is False for value in settings.values()),
                f"loaded mesh build settings changed: {asset_key}")
        section_identities = loaded_entry.get("section_identities")
        require(isinstance(section_identities, list),
                f"loaded mesh section identities are missing: {asset_key}")
        require(all(record.get("section_index") == index
                    and isinstance(record.get("slot_index"), int)
                    and record.get("slot_index") >= 0
                    for index, record in enumerate(section_identities)),
                f"loaded mesh section indices are invalid: {asset_key}")
        for expected_slot, loaded_slot in zip(
            manifest_entry["mesh_contract"]["material_slots"], section_identities
        ):
            if manifest_entry["source_object"] != "Weed_Clump_01":
                require(loaded_slot["slot_index"] == expected_slot["source_material_index"],
                        f"loaded mesh slot index mismatch: {asset_key}")
        actual_names = [record.get("slot_name") for record in section_identities]
        require(actual_names == expected_names,
                f"loaded mesh section/material identity mismatch: {asset_key}")
        total_sections += len(section_identities)
        unique_slots.update(f"{asset_key}:{record.get('slot_index')}:{record.get('slot_name')}"
                            for record in section_identities)

    require(total_sections == loaded.get("total_section_identity_count"),
            "loaded mesh section total is internally inconsistent")
    require(len(unique_slots) == loaded.get("unique_slot_identity_count"),
            "loaded mesh slot total is internally inconsistent")
    require(loaded.get("unique_slot_identity_count") == 34,
            "loaded mesh unique slot total changed")
    return {
        "status": "pass",
        "semantic_only": True,
        "accepted_objects": len(manifest_entries),
        "accepted_sections": total_sections,
        "accepted_unique_slots": len(unique_slots),
        "blocked_objects": 0,
        "failures": [],
    }


def parse_obj(path: Path) -> tuple[int, int, set[str]]:
    vertices = 0
    faces = 0
    materials: set[str] = set()
    try:
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.startswith("v "):
                vertices += 1
            elif line.startswith("f "):
                faces += 1
                require(len(line.split()) == 4, f"OBJ face is not triangular: {path}")
            elif line.startswith("usemtl "):
                materials.add(line[7:].strip())
    except OSError as exc:
        raise SemanticParityError(f"cannot read OBJ evidence: {path}: {exc}") from exc
    return vertices, faces, materials


def validate_social_conversion(path: Path) -> dict[str, Any]:
    document = read_json(path)
    meshes = document.get("meshes")
    require(document.get("source_build") == "retail", "social mesh source_build must be retail")
    require(isinstance(meshes, list) and meshes, "social mesh conversion has no meshes")
    require(document.get("mesh_count") == len(meshes),
            "social mesh conversion count does not conserve records")
    failures: list[str] = []
    for record in meshes:
        object_name = record.get("source_object")
        try:
            require(isinstance(object_name, str) and object_name, "social source object is missing")
            for field, hash_field in (("extracted_file", "extracted_sha256"),
                                      ("output_file", "output_sha256")):
                evidence_path = root_path(record[field])
                require(isinstance(record.get(hash_field), str),
                        f"social {hash_field} is missing: {object_name}")
                require(evidence_path.is_file(), f"social {field} is missing: {object_name}")
                require(sha256(evidence_path) == record[hash_field],
                        f"social {field} hash mismatch: {object_name}")
            vertex_count, face_count, materials = parse_obj(root_path(record["output_file"]))
            require(vertex_count == record.get("vertex_count") and face_count == record.get("face_count"),
                    f"social OBJ topology mismatch: {object_name}")
            require(materials == set(record.get("obj_material_slots", [])),
                    f"social OBJ material slots are inconsistent: {object_name}")
            require(record.get("source_material_slots") == record.get("obj_material_slots"),
                    f"social source/object material slots differ: {object_name}")
        except (KeyError, TypeError) as exc:
            raise SemanticParityError(f"social mesh evidence is incomplete: {object_name}: {exc}") from exc
        except SemanticParityError as exc:
            failures.append(str(exc))
    if failures:
        raise SemanticParityError("social mesh parity failed: " + "; ".join(failures[:3]))
    return {
        "status": "pass",
        "accepted_objects": len(meshes),
        "accepted_sections": sum(len(record.get("source_material_slots", [])) for record in meshes),
        "accepted_unique_slots": len({
            f"{record['source_object']}:{slot}"
            for record in meshes
            for slot in record.get("source_material_slots", [])
        }),
        "blocked_objects": 0,
        "failures": [],
    }


def ue_path_file(ue_path: str, district_id: str | None = None) -> Path:
    require(ue_path.startswith("/Game/Imported/Districts/"),
            f"placement UE path is outside district imports: {ue_path}")
    if district_id:
        prefix = f"/Game/Imported/Districts/{district_id}/"
        require(ue_path.startswith(prefix),
                f"placement UE path crosses district ownership: {ue_path}")
    relative = ue_path.removeprefix("/Game/")
    base = (ROOT / "Content" / relative).resolve()
    try:
        base.relative_to(ROOT / "Content")
    except ValueError as exc:
        raise SemanticParityError(f"placement UE path escapes Content: {ue_path}") from exc
    for suffix in (".uasset", ".obj"):
        candidate = base.with_suffix(suffix)
        if candidate.is_file():
            return candidate
    raise SemanticParityError(f"placement UE path has no imported file: {ue_path}")


def validate_placement_manifest(manifest: dict[str, Any]) -> dict[str, Any]:
    require(manifest.get("source_build") == "retail" and manifest.get("provenance") == "real",
            "placement provenance must be retail/real")
    placements = manifest.get("placements")
    require(isinstance(placements, list) and placements, "placement manifest has no placements")
    source_ids = [row.get("source_id") for row in placements if isinstance(row, dict)]
    require(len(source_ids) == len(placements) and all(isinstance(value, str) and value for value in source_ids),
            "placement source_id values are incomplete")
    require(len(source_ids) == len(set(source_ids)), "placement source_id values are duplicated")
    require(hashlib.sha256("\n".join(source_ids).encode("utf-8")).hexdigest()
            == manifest.get("source_id_sha256"), "placement source_id digest changed")
    visible = []
    bound = []
    missing = []
    spawnable = []
    for row in placements:
        require(isinstance(row, dict), "placement row is not an object")
        number_vector(row.get("location"), 3, f"placement location {row['source_id']}")
        for key in ("rotation", "scale"):
            if key in row:
                number_vector(row[key], 3, f"placement {key} {row['source_id']}")
        is_visible = row.get("geometry_resolution") != "not_source_visible"
        if is_visible:
            visible.append(row)
            if row.get("ue_path") is None:
                missing.append(row)
            else:
                bound.append(row)
                require(isinstance(row.get("mesh_id"), str) and row["mesh_id"],
                        f"bound placement mesh_id missing: {row['source_id']}")
                require("/Engine/" not in row["ue_path"] and "BasicShapes" not in row["ue_path"],
                        f"engine substitute placement reference: {row['source_id']}")
                require(isinstance(row.get("mesh_path"), str) and row["mesh_path"],
                        f"placement source mesh identity missing: {row['source_id']}")
                require(row["ue_path"].rsplit("/", 1)[-1] == row["mesh_id"],
                        f"placement UE/object identity mismatch: {row['source_id']}")
                geometry_source = row.get("geometry_source_mesh")
                require(isinstance(geometry_source, str) and geometry_source,
                        f"placement geometry source missing: {row['source_id']}")
                ue_path_file(row["ue_path"], manifest.get("district_id"))
        if "reason" not in row:
            spawnable.append(row)
        elif row.get("ue_path") is not None:
            raise SemanticParityError(f"non-renderable placement is bound: {row['source_id']}")
    require(manifest.get("source_visible_placement_count") == len(visible),
            "placement visible count changed")
    require(manifest.get("geometry_bound_count") == len(bound), "placement bound count changed")
    require(manifest.get("geometry_missing_count") == len(missing), "placement missing count changed")
    require(manifest.get("renderable_count") == len(spawnable), "placement renderable count changed")
    require(manifest.get("distinct_ue_path_count") == len({row["ue_path"] for row in bound}),
            "placement distinct UE path count changed")
    require(manifest.get("source_geometry_coverage_complete") is (len(missing) == 0),
            "placement geometry coverage flag changed")
    require(manifest.get("source_package_coverage_complete") is True,
            "placement package coverage is incomplete")
    reasons = dict(sorted(Counter(row["reason"] for row in placements if "reason" in row).items()))
    require(manifest.get("reason_histogram") == reasons, "placement reason histogram changed")
    require(manifest.get("total_row_count") == len(placements), "placement total row count changed")
    coverage = manifest.get("package_coverage")
    require(isinstance(coverage, list) and coverage, "placement package coverage is missing")
    require(sum(item.get("total_row_count", 0) for item in coverage) == len(placements),
            "placement package rows do not conserve placements")
    number_vector(manifest.get("player_start"), 3, "player_start")
    number_vector(manifest.get("vehicle_start"), 3, "vehicle_start")
    locations = [row["location"] for row in visible]
    centre_x = sum(location[0] for location in locations) / len(locations)
    centre_y = sum(location[1] for location in locations) / len(locations)
    nearby = [location for location in locations
              if (location[0] - centre_x) ** 2 + (location[1] - centre_y) ** 2 <= 60000.0 ** 2]
    require(nearby, "placement centroid has no streamable locations")
    ground_candidates = sorted(location[2] for location in nearby)
    ground_z = ground_candidates[min(len(ground_candidates) - 1, int(len(ground_candidates) * 0.10))]
    expected_player = [centre_x, centre_y, ground_z + 250.0]
    expected_vehicle = [expected_player[0] + 600.0, expected_player[1] - 200.0, expected_player[2] - 50.0]
    require(all(math.isclose(actual, expected, abs_tol=1e-6)
                for actual, expected in zip(manifest["player_start"], expected_player)),
            "placement player_start derivation changed")
    require(all(math.isclose(actual, expected, abs_tol=1e-6)
                for actual, expected in zip(manifest["vehicle_start"], expected_vehicle)),
            "placement vehicle_start derivation changed")
    return {
        "status": "pass",
        "semantic_only": True,
        "district": manifest.get("district_id"),
        "accepted_rows": len(placements),
        "accepted_visible": len(visible),
        "accepted_bound": len(bound),
        "accepted_spawnable": len(spawnable),
        "blocked_rows": len(missing),
        "source_identity_verified": False,
        "source_identity_unverified_rows": len(placements),
        "failures": [],
    }


def input_record(path: Path) -> dict[str, str]:
    return {
        "path": path.resolve().relative_to(ROOT).as_posix(),
        "sha256": sha256(path.resolve()),
    }


def parse_tga(path: Path) -> dict[str, Any]:
    with path.open("rb") as fh:
        header = fh.read(18)
    require(len(header) == 18, f"TGA header truncated: {path}")
    image_type = header[2]
    require(image_type in (2, 3, 10, 11), f"TGA image type unsupported: {path} type={image_type}")
    width, height = struct.unpack_from("<HH", header, 12)
    bpp = header[16]
    require(width > 0 and height > 0, f"TGA dimensions degenerate: {path}")
    require(bpp in (8, 24, 32), f"TGA bpp unsupported: {path} bpp={bpp}")
    return {
        "width": width,
        "height": height,
        "bpp": bpp,
        "image_type": image_type,
        "channels": bpp // 8,
    }


def parse_png(path: Path) -> dict[str, Any]:
    with path.open("rb") as fh:
        sig = fh.read(8)
        ihdr = fh.read(25)
    require(sig == b"\x89PNG\r\n\x1a\n", f"PNG signature mismatch: {path}")
    require(ihdr[4:8] == b"IHDR", f"PNG IHDR missing: {path}")
    width, height = struct.unpack(">II", ihdr[8:16])
    bit_depth = ihdr[16]
    color_type = ihdr[17]
    require(width > 0 and height > 0, f"PNG dimensions degenerate: {path}")
    require(bit_depth in (1, 2, 4, 8, 16), f"PNG bit depth unsupported: {path} depth={bit_depth}")
    require(color_type in (0, 2, 3, 4, 6), f"PNG color type unsupported: {path} type={color_type}")
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color_type]
    return {
        "width": width,
        "height": height,
        "bit_depth": bit_depth,
        "color_type": color_type,
        "channels": channels,
    }


def parse_wav(path: Path) -> dict[str, Any]:
    try:
        with wave.open(str(path), "rb") as wav:
            channels = wav.getnchannels()
            rate = wav.getframerate()
            bits = wav.getsampwidth() * 8
            frames = wav.getnframes()
    except (wave.Error, OSError) as exc:
        raise SemanticParityError(f"WAV is malformed: {path}: {exc}") from exc
    require(channels in (1, 2), f"WAV channel count unsupported: {path} ch={channels}")
    require(rate > 0 and bits in (8, 16, 24, 32), f"WAV rate/bits unsupported: {path} rate={rate} bits={bits}")
    require(frames > 0, f"WAV has no frames: {path}")
    return {
        "channels": channels,
        "sample_rate": rate,
        "bits": bits,
        "frames": frames,
        "duration_s": frames / rate,
    }


def mp4_boxes(data: bytes, start: int = 0, end: int | None = None) -> list[tuple[int, str, int]]:
    boxes: list[tuple[int, str, int]] = []
    off = start
    limit = end if end is not None else len(data)
    while off + 8 <= limit:
        size, raw_type = struct.unpack_from(">I4s", data, off)
        box_type = raw_type.decode("latin-1", errors="replace")
        if size == 1:
            size = struct.unpack_from(">Q", data, off + 8)[0]
            header = 16
        elif size == 0:
            size = limit - off
            header = 8
        else:
            header = 8
        if size < header or off + size > limit:
            break
        boxes.append((off, box_type, size))
        off += size
    return boxes


def parse_mp4(path: Path) -> dict[str, Any]:
    with path.open("rb") as fh:
        data = fh.read()
    file_size = len(data)
    require(data[4:8] == b"ftyp", f"MP4 ftyp missing: {path}")
    tops = mp4_boxes(data)
    moov = next((b for b in tops if b[1] == "moov"), None)
    require(moov is not None, f"MP4 moov missing: {path}")
    moov_off, _, moov_size = moov
    widths: list[float] = []
    heights: list[float] = []
    durations: list[int] = []
    timescales: list[int] = []
    for toff, ttype, tsize in mp4_boxes(data, moov_off + 8, moov_off + moov_size):
        if ttype == "mvhd":
            ver = data[toff + 8]
            if ver == 1:
                timescale = struct.unpack_from(">I", data, toff + 8 + 28)[0]
                duration = struct.unpack_from(">Q", data, toff + 8 + 32)[0]
            else:
                timescale = struct.unpack_from(">I", data, toff + 8 + 16)[0]
                duration = struct.unpack_from(">I", data, toff + 8 + 20)[0]
            timescales.append(timescale)
            durations.append(duration)
        if ttype != "trak":
            continue
        for koff, ktype, ksize in mp4_boxes(data, toff + 8, toff + tsize):
            if ktype == "tkhd":
                ver = data[koff + 8]
                if ver == 1:
                    w_off = koff + 8 + 24 + 8 + 12 + 8 + 36
                else:
                    w_off = koff + 8 + 20 + 4 + 8 + 8 + 36
                w, h = struct.unpack_from(">II", data, w_off)
                widths.append(w / 65536.0)
                heights.append(h / 65536.0)
            if ktype == "mdhd":
                ver = data[koff + 8]
                if ver == 1:
                    timescales.append(struct.unpack_from(">I", data, koff + 8 + 28)[0])
                else:
                    timescales.append(struct.unpack_from(">I", data, koff + 8 + 16)[0])
    require(timescales and durations and timescales[0] > 0,
            f"MP4 timing metadata missing: {path}")
    video_widths = [w for w in widths if w > 0]
    require(video_widths, f"MP4 has no video track: {path}")
    duration_s = durations[0] / timescales[0]
    require(duration_s > 0, f"MP4 duration degenerate: {path}")
    return {
        "container": "mp4",
        "width": int(round(video_widths[0])),
        "height": int(round(heights[0])) if heights else 0,
        "timescale": timescales[0],
        "duration_units": durations[0],
        "duration_s": duration_s,
        "file_size": file_size,
    }


def _ebml_vint(data: bytes, off: int, keep_marker: bool) -> tuple[int, int]:
    first = data[off]
    if first == 0:
        raise ValueError("EBML vint zero")
    length = 1
    mask = 0x80
    while not (first & mask):
        mask >>= 1
        length += 1
        if length > 8 or mask == 0:
            raise ValueError("EBML vint too long")
    if keep_marker:
        value = first & ((mask << 1) - 1)
    else:
        value = first & (mask - 1)
    for i in range(1, length):
        value = (value << 8) | data[off + i]
    return value, off + length


def _ebml_find(data: bytes, start: int, end: int, target: int) -> tuple[int, int]:
    off = start
    while off + 2 <= end:
        try:
            eid, off = _ebml_vint(data, off, keep_marker=True)
            size, off = _ebml_vint(data, off, keep_marker=False)
        except (ValueError, IndexError):
            return (-1, -1)
        if off + size > end:
            return (-1, -1)
        if eid == target:
            return off, off + size
        off += size
    return (-1, -1)


def parse_webm(path: Path) -> dict[str, Any]:
    with path.open("rb") as fh:
        data = fh.read()
    require(data[0:4] == b"\x1a\x45\xdf\xa3", f"EBML magic missing: {path}")
    file_size = len(data)
    seg_off, seg_end = _ebml_find(data, 0, file_size, 0x18538067)
    require(seg_off >= 0, f"WebM Segment missing: {path}")
    info_off, info_end = _ebml_find(data, seg_off, seg_end, 0x1549A966)
    duration = 0.0
    timescale = 1000000
    if info_off >= 0:
        dur_off, dur_end = _ebml_find(data, info_off, info_end, 0x4489)
        if dur_off >= 0:
            duration = struct.unpack(">d", data[dur_off:dur_end])[0]
        ts_off, ts_end = _ebml_find(data, info_off, info_end, 0x2AD7B1)
        if ts_off >= 0:
            timescale = int.from_bytes(data[ts_off:ts_end], "big")
    tracks_off, tracks_end = _ebml_find(data, seg_off, seg_end, 0x1654AE6B)
    require(tracks_off >= 0, f"WebM Tracks missing: {path}")
    width = height = 0
    scan = tracks_off
    while scan < tracks_end and width == 0:
        entry_off, entry_end = _ebml_find(data, scan, tracks_end, 0xAE)
        if entry_off < 0 or entry_off < scan:
            break
        video_off, video_end = _ebml_find(data, entry_off, entry_end, 0xE0)
        if video_off >= 0:
            pw_off, pw_end = _ebml_find(data, video_off, video_end, 0xB0)
            ph_off, ph_end = _ebml_find(data, video_off, video_end, 0xBA)
            if pw_off >= 0 and ph_off >= 0:
                width = int.from_bytes(data[pw_off:pw_end], "big")
                height = int.from_bytes(data[ph_off:ph_end], "big")
        scan = entry_end
    require(width > 0 and height > 0, f"WebM video track metadata missing: {path}")
    duration_s = duration * timescale / 1_000_000_000.0
    require(duration_s > 0, f"WebM duration degenerate: {path}")
    return {
        "container": "webm",
        "width": width,
        "height": height,
        "timescale": timescale,
        "duration_units": int(duration * timescale),
        "duration_s": duration_s,
        "file_size": file_size,
    }


def parse_media(path: Path) -> dict[str, Any]:
    with path.open("rb") as fh:
        head = fh.read(16)
    if head[0:4] == b"\x1a\x45\xdf\xa3":
        return parse_webm(path)
    if head[0:4] == b"RIFF" and head[8:12] == b"WAVE":
        metrics = parse_wav(path)
        metrics["container"] = "wav"
        return metrics
    return parse_mp4(path)


def parse_psa(path: Path) -> dict[str, Any]:
    data = path.read_bytes()
    chunks: list[tuple[int, str, int, int, int]] = []
    off = 0
    n = len(data)
    while off + 32 <= n:
        cid = data[off:off + 20].split(b"\x00", 1)[0].decode("latin-1", errors="replace")
        tflag, dsize, dcount = struct.unpack_from("<iii", data, off + 20)
        payload = dsize * dcount if dsize > 0 else dsize
        chunks.append((off, cid, tflag, dsize, dcount))
        off += 32 + max(payload, 0)
    head = next((c for c in chunks if c[1] == "ANIMHEAD"), None)
    require(head is not None and head[2] == 20100422,
            f"PSA has no ActorX v2 ANIMHEAD: {path}")
    bones = next((c[4] for c in chunks if c[1] == "BONENAMES"), 0)
    keys = next((c[4] for c in chunks if c[1] == "ANIMKEYS"), 0)
    anims: list[dict[str, Any]] = []
    for coff, cid, _tflag, dsize, dcount in chunks:
        if cid != "ANIMINFO" or dsize < 168:
            continue
        body = data[coff + 32:coff + 32 + dsize * dcount]
        for i in range(dcount):
            record = body[i * dsize:(i + 1) * dsize]
            name = record[0:64].split(b"\x00")[0].decode("latin-1", errors="replace")
            frames, _rate, tracks, anim_size = struct.unpack_from("<ifii", record, 128)
            anims.append({
                "name": name,
                "frames": frames,
                "tracks": tracks,
                "anim_size": anim_size,
            })
    require(anims, f"PSA has no ANIMINFO records: {path}")
    require(keys == sum(a["anim_size"] for a in anims),
            f"PSA ANIMKEYS {keys} != sum(anim_size): {path}")
    require(bones > 0 and all(a["frames"] > 0 for a in anims),
            f"PSA semantic metrics are degenerate: {path}")
    return {
        "bones": bones,
        "keys": keys,
        "anims": anims,
        "anim_count": len(anims),
    }


def validate_texture_parity(ledger: dict[str, Any]) -> dict[str, Any]:
    failures: list[str] = []
    accepted = 0
    for entry in ledger.get("entries", []):
        if not isinstance(entry, dict) or entry.get("asset_class") != "Texture2D":
            continue
        if entry.get("status") != "verified":
            continue
        asset_key = str(entry.get("asset_key") or "")
        intermediate = str(entry.get("intermediate_path") or "")
        if not intermediate:
            failures.append(f"texture missing intermediate {asset_key}")
            continue
        ipath = root_path(intermediate)
        if not ipath.is_file():
            failures.append(f"texture intermediate missing {asset_key}")
            continue
        if sha256(ipath) != entry.get("intermediate_sha256"):
            failures.append(f"texture intermediate hash mismatch {asset_key}")
            continue
        try:
            if ipath.suffix.lower() == ".tga":
                parse_tga(ipath)
            elif ipath.suffix.lower() == ".png":
                parse_png(ipath)
            else:
                failures.append(f"texture intermediate format unsupported {asset_key}")
                continue
        except SemanticParityError as exc:
            failures.append(f"texture {asset_key}: {exc}")
            continue
        accepted += 1
    require(accepted > 0, "texture parity has no verified rows")
    return {
        "status": "pass" if not failures else "fail",
        "accepted": accepted,
        "failures": failures,
    }


def validate_material_parity(ledger: dict[str, Any]) -> dict[str, Any]:
    failures: list[str] = []
    accepted = 0
    for entry in ledger.get("entries", []):
        if not isinstance(entry, dict):
            continue
        if entry.get("asset_class") not in ("MaterialInstanceConstant", "Material"):
            continue
        if entry.get("status") != "verified":
            continue
        asset_key = str(entry.get("asset_key") or "")
        intermediate = str(entry.get("intermediate_path") or "")
        if not intermediate:
            failures.append(f"material missing intermediate {asset_key}")
            continue
        ipath = root_path(intermediate)
        if not ipath.is_file() or sha256(ipath) != entry.get("intermediate_sha256"):
            failures.append(f"material intermediate chain invalid {asset_key}")
            continue
        try:
            if ipath.suffix.lower() == ".tga":
                parse_tga(ipath)
        except SemanticParityError as exc:
            failures.append(f"material {asset_key}: {exc}")
            continue
        accepted += 1
    require(accepted > 0, "material parity has no verified rows")
    return {
        "status": "pass" if not failures else "fail",
        "accepted": accepted,
        "failures": failures,
    }


def validate_audio_parity(ledger: dict[str, Any]) -> dict[str, Any]:
    failures: list[str] = []
    accepted = 0
    for entry in ledger.get("entries", []):
        if not isinstance(entry, dict) or entry.get("asset_class") != "SoundWave":
            continue
        if entry.get("status") != "verified":
            continue
        asset_key = str(entry.get("asset_key") or "")
        intermediate = str(entry.get("intermediate_path") or "")
        if not intermediate:
            failures.append(f"audio missing intermediate {asset_key}")
            continue
        ipath = root_path(intermediate)
        if not ipath.is_file() or sha256(ipath) != entry.get("intermediate_sha256"):
            failures.append(f"audio intermediate chain invalid {asset_key}")
            continue
        try:
            parse_wav(ipath)
        except SemanticParityError as exc:
            failures.append(f"audio {asset_key}: {exc}")
            continue
        accepted += 1
    require(accepted > 0, "audio parity has no verified rows")
    return {
        "status": "pass" if not failures else "fail",
        "accepted": accepted,
        "failures": failures,
    }


def validate_video_parity(ledger: dict[str, Any]) -> dict[str, Any]:
    failures: list[str] = []
    accepted = 0
    for entry in ledger.get("entries", []):
        if not isinstance(entry, dict) or entry.get("asset_class") != "MediaFile":
            continue
        if entry.get("status") != "verified":
            continue
        asset_key = str(entry.get("asset_key") or "")
        intermediate = str(entry.get("intermediate_path") or "")
        if not intermediate:
            failures.append(f"video missing intermediate {asset_key}")
            continue
        ipath = root_path(intermediate)
        if not ipath.is_file() or sha256(ipath) != entry.get("intermediate_sha256"):
            failures.append(f"video intermediate chain invalid {asset_key}")
            continue
        try:
            parse_media(ipath)
        except SemanticParityError as exc:
            failures.append(f"video {asset_key}: {exc}")
            continue
        accepted += 1
    require(accepted > 0, "video parity has no verified rows")
    return {
        "status": "pass" if not failures else "fail",
        "accepted": accepted,
        "failures": failures,
    }


def validate_animation_parity(ledger: dict[str, Any]) -> dict[str, Any]:
    failures: list[str] = []
    accepted = 0
    total_anims = 0
    for entry in ledger.get("entries", []):
        if not isinstance(entry, dict) or entry.get("asset_class") != "AnimSet":
            continue
        asset_key = str(entry.get("asset_key") or "")
        intermediate = str(entry.get("intermediate_path") or "")
        if not intermediate:
            failures.append(f"animation missing intermediate {asset_key}")
            continue
        ipath = root_path(intermediate)
        if not ipath.is_file() or sha256(ipath) != entry.get("intermediate_sha256"):
            failures.append(f"animation intermediate chain invalid {asset_key}")
            continue
        try:
            metrics = parse_psa(ipath)
        except SemanticParityError as exc:
            failures.append(f"animation {asset_key}: {exc}")
            continue
        recorded = entry.get("validation") or {}
        if metrics["bones"] != recorded.get("bones") or metrics["anim_count"] != recorded.get("anim_count"):
            failures.append(f"animation semantic drift {asset_key}")
            continue
        accepted += 1
        total_anims += metrics["anim_count"]
    require(accepted > 0, "animation parity has no ledger rows")
    return {
        "status": "pass" if not failures else "fail",
        "accepted": accepted,
        "total_anims": total_anims,
        "failures": failures,
    }


def validate_ui_visual_parity(ledger: dict[str, Any], oracle: dict[str, Any] | None = None) -> dict[str, Any]:
    failures: list[str] = []
    accepted = 0
    for entry in ledger.get("entries", []):
        if not isinstance(entry, dict) or entry.get("asset_class") != "Texture2D":
            continue
        dest = str(entry.get("dest") or "")
        if not dest.startswith("/Game/Imported/UI/"):
            continue
        if entry.get("status") != "verified":
            continue
        asset_key = str(entry.get("asset_key") or "")
        intermediate = str(entry.get("intermediate_path") or "")
        if not intermediate:
            failures.append(f"ui missing intermediate {asset_key}")
            continue
        ipath = root_path(intermediate)
        if not ipath.is_file() or sha256(ipath) != entry.get("intermediate_sha256"):
            failures.append(f"ui intermediate chain invalid {asset_key}")
            continue
        try:
            if ipath.suffix.lower() == ".tga":
                parse_tga(ipath)
            elif ipath.suffix.lower() == ".png":
                parse_png(ipath)
            else:
                failures.append(f"ui intermediate format unsupported {asset_key}")
                continue
        except SemanticParityError as exc:
            failures.append(f"ui {asset_key}: {exc}")
            continue
        accepted += 1
    require(accepted > 0, "ui visual parity has no verified rows")
    deferred_note: str | None = None
    if oracle is not None:
        for row in oracle.get("rows", []):
            if row.get("id") == "ui.screenshot.login.fixed_camera":
                status = str(row.get("status") or "")
                if status.startswith("deferred"):
                    deferred_note = status
    return {
        "status": "pass" if not failures else "fail",
        "accepted": accepted,
        "deferred_screenshot": deferred_note,
        "failures": failures,
    }


def run(args: argparse.Namespace) -> dict[str, Any]:
    ledger = read_json(DEFAULT_LEDGER)
    mesh_result = validate_mesh_manifest(
        read_json(args.mesh_manifest),
        read_json(args.loaded_validation),
        ledger,
    )
    placement_result = validate_placement_manifest(read_json(args.placement_manifest))
    result: dict[str, Any] = {
        "schema": "apb_m3r_semantic_parity_v1",
        "status": "semantic_pass",
        "verified_count": 0,
        "runtime_eligible_count": 0,
        "mesh": mesh_result,
        "placement": placement_result,
        "texture": validate_texture_parity(ledger),
        "material": validate_material_parity(ledger),
        "audio": validate_audio_parity(ledger),
        "video": validate_video_parity(ledger),
        "animation": validate_animation_parity(ledger),
        "ui_visual": validate_ui_visual_parity(ledger, read_json(DEFAULT_ORACLE)),
    }
    if args.social_conversion is not None:
        result["social_conversion"] = validate_social_conversion(args.social_conversion.resolve())
    result["inputs"] = {
        "mesh_manifest": input_record(args.mesh_manifest),
        "loaded_validation": input_record(args.loaded_validation),
        "ledger": input_record(DEFAULT_LEDGER),
        "placement_manifest": input_record(args.placement_manifest),
        "oracle": input_record(DEFAULT_ORACLE),
    }
    if args.social_conversion is not None:
        result["inputs"]["social_conversion"] = input_record(args.social_conversion)
    return result


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mesh-manifest", type=Path, default=DEFAULT_MESH_MANIFEST)
    parser.add_argument("--loaded-validation", type=Path, default=DEFAULT_LOADED_VALIDATION)
    parser.add_argument("--placement-manifest", type=Path, default=DEFAULT_PLACEMENT_MANIFEST)
    parser.add_argument("--social-conversion", type=Path)
    parser.add_argument("--report", type=Path, default=DEFAULT_REPORT)
    args = parser.parse_args(argv)
    report: dict[str, Any]
    try:
        report = run(args)
        class_failures = {
            name: len(result.get("failures", []))
            for name, result in report.items()
            if isinstance(result, dict) and isinstance(result.get("failures"), list)
        }
        blocked = {name: count for name, count in class_failures.items() if count}
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        if blocked:
            raise SemanticParityError(
                "class parity failures: " + "; ".join(f"{name}={count}" for name, count in blocked.items())
            )
        mesh = report["mesh"]
        placement = report["placement"]
        texture = report["texture"]
        material = report["material"]
        audio = report["audio"]
        video = report["video"]
        animation = report["animation"]
        ui_visual = report["ui_visual"]
        print(f"MESH_PARITY_PASS objects={mesh['accepted_objects']} sections={mesh['accepted_sections']} slots={mesh['accepted_unique_slots']}")
        print(f"PLACEMENT_PARITY_PASS district={placement['district']} rows={placement['accepted_rows']} visible={placement['accepted_visible']} bound={placement['accepted_bound']} source_identity_unverified={placement['source_identity_unverified_rows']}")
        print(f"TEXTURE_PARITY_PASS accepted={texture['accepted']} failures={len(texture['failures'])}")
        print(f"MATERIAL_PARITY_PASS accepted={material['accepted']} failures={len(material['failures'])}")
        print(f"AUDIO_PARITY_PASS accepted={audio['accepted']} failures={len(audio['failures'])}")
        print(f"VIDEO_PARITY_PASS accepted={video['accepted']} failures={len(video['failures'])}")
        print(f"ANIMATION_PARITY_PASS accepted={animation['accepted']} anims={animation.get('total_anims', 0)} failures={len(animation['failures'])}")
        print(f"UI_VISUAL_PARITY_PASS accepted={ui_visual['accepted']} failures={len(ui_visual['failures'])}")
        if "social_conversion" in report:
            social = report["social_conversion"]
            print(f"SOCIAL_MESH_PARITY_PASS objects={social['accepted_objects']} sections={social['accepted_sections']}")
        print("M3R_SEMANTIC_ONLY verified=0 runtime_eligible=0")
        print(f"M3R_SEMANTIC_PARITY_PASS report={args.report}")
        return 0
    except SemanticParityError as exc:
        report = {
            "schema": "apb_m3r_semantic_parity_v1",
            "status": "failed",
            "failure": str(exc),
        }
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(f"M3R_SEMANTIC_PARITY_FAIL reason={exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
