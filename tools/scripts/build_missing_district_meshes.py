from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "model_viewer"))
from psk_reader import export_obj, load_psk, obj_material_names


ROOT = Path(__file__).resolve().parents[2]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def build_psk_index(extracted_roots: list[Path]) -> dict[str, list[tuple[Path, tuple[str, ...]]]]:
    """One scan of all ActorX sources: stem (casefolded) -> [(path, parts_lower)]."""
    index: dict[str, list[tuple[Path, tuple[str, ...]]]] = {}
    for extracted_root in extracted_roots:
        for extension in (".pskx", ".psk"):
            for candidate in extracted_root.rglob(f"*{extension}"):
                resolved = candidate.resolve()
                parts = tuple(part.lower() for part in resolved.parts)
                index.setdefault(resolved.stem.lower(), []).append((resolved, parts))
    return index


def normalize_name(name: str) -> str:
    """umodel-export sanitization: ` (MC)` -> `_MC`, other non-alnum runs -> `_`."""
    import re

    stepped = re.sub(r"\s+\((\w+)\)", r"_\1", name)
    return re.sub(r"[^A-Za-z0-9]+", "_", stepped).lower()


def source_candidates(index: dict[str, list[tuple[Path, tuple[str, ...]]]], package: str, object_name: str) -> list[Path]:
    """Find the ActorX source for a placement mesh object.

    Block placements reference the base object (e.g. `X_VertexLit_LOD`) while the
    exported meshes carry a LOD index suffix (`X_VertexLit_LOD_0.pskx`). Some
    exports additionally strip the `_VertexLit` token (`X_LOD_0.pskx`) or sanitize
    tokens such as ` (MC)` to `_MC`. Match tiers: exact object first, then
    LOD-indexed variants, then `_VertexLit`-stripped variants, then normalized
    variants. LOD_0 (highest detail) is preferred. Returns only the first tier
    with matches, so the ambiguity check stays meaningful.
    """
    package_lower = package.lower()
    base_variants = [object_name]
    if "_VertexLit" in object_name:
        base_variants.append(object_name.replace("_VertexLit", ""))
    for base in base_variants:
        for name in (f"{base}_0", base, f"{base}_1", f"{base}_2"):
            matches: list[Path] = []
            for resolved, parts in index.get(name.lower(), []):
                if package_lower in parts:
                    matches.append(resolved)
            if matches:
                return sorted(set(matches))
    for base in base_variants:
        normalized = normalize_name(base)
        for name in (f"{normalized}_0", normalized, f"{normalized}_1", f"{normalized}_2"):
            matches: list[Path] = []
            for resolved, parts in index.get(name, []):
                if package_lower in parts:
                    matches.append(resolved)
            if matches:
                return sorted(set(matches))
    return []


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--district", required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--asset-root", type=Path, required=True)
    parser.add_argument("--extracted-root", type=Path, action="append", required=True)
    parser.add_argument("--retail-packages-root", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--mesh-path", action="append", default=[])
    parser.add_argument("--mesh-list-file", type=Path, default=None)
    args = parser.parse_args()

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    obligations: dict[str, tuple[str, str]] = {}
    mesh_paths = list(args.mesh_path)
    if args.mesh_list_file is not None:
        mesh_paths = [
            line.strip()
            for line in args.mesh_list_file.read_text(encoding="utf-8").splitlines()
            if line.strip()
        ]
    if not mesh_paths:
        mesh_paths = [
            row.get("mesh_path")
            for row in manifest.get("placements", [])
            if row.get("geometry_resolution") == "retail_geometry_not_recovered"
        ]
    for mesh_path in mesh_paths:
        if not isinstance(mesh_path, str) or "." not in mesh_path:
            raise ValueError(f"missing package-qualified mesh path: {mesh_path}")
        package = mesh_path.split(".", 1)[0]
        object_name = mesh_path.rsplit(".", 1)[-1]
        obligations[mesh_path.lower()] = (package, object_name)
    if not obligations:
        raise ValueError("no mesh conversion obligations")

    records: list[dict] = []
    package_index: dict[str, list[Path]] = {}
    for package_file in args.retail_packages_root.rglob("*.upk"):
        package_index.setdefault(package_file.stem.lower(), []).append(package_file.resolve())
    psk_index = build_psk_index(args.extracted_root)
    package_hash_cache: dict[Path, str] = {}
    args.asset_root.mkdir(parents=True, exist_ok=True)
    for mesh_path, (package, object_name) in sorted(obligations.items()):
        candidates = source_candidates(psk_index, package, object_name)
        if not candidates:
            raise FileNotFoundError(f"no exact ActorX source for {package}.{object_name}")
        hashes = {sha256(candidate) for candidate in candidates}
        if len(hashes) != 1:
            raise ValueError(
                f"ambiguous ActorX sources for {package}.{object_name}: "
                + ", ".join(str(candidate) for candidate in candidates)
            )
        source = candidates[0]
        source_packages = sorted(package_index.get(package.lower(), []))
        if len(source_packages) != 1:
            raise ValueError(
                f"retail package identity is not unique for {package}: "
                + ", ".join(str(candidate) for candidate in source_packages)
            )
        source_package = source_packages[0]
        mesh = load_psk(source)
        if not mesh.vertices or not mesh.faces or not mesh.wedges:
            raise ValueError(f"empty ActorX mesh for {package}.{object_name}")
        output = (args.asset_root / f"{object_name}.obj").resolve()
        export_obj(mesh, output)
        records.append({
            "source_build": "retail",
            "source_package": package,
            "source_package_locator": (
                "${retail_steam}/APBGame/Content/Release/Packages/"
                + source_package.relative_to(args.retail_packages_root.resolve()).as_posix()
            ),
            "source_package_sha256": package_hash_cache.setdefault(
                source_package, sha256(source_package)
            ),
            "source_object": object_name,
            "extracted_file": source.relative_to(ROOT).as_posix(),
            "extracted_sha256": sha256(source),
            "converter": "tools/model_viewer/psk_reader.py",
            "output_file": output.relative_to(ROOT).as_posix(),
            "output_sha256": sha256(output),
            "vertex_count": len(mesh.vertices),
            "face_count": len(mesh.faces),
            "source_material_slots": [material.name for material in mesh.materials],
            "obj_material_slots": obj_material_names(mesh),
            "material_translation_status": "slot_identity_only",
            "texture_binding_status": "unresolved",
        })

    evidence = {
        "district": args.district,
        "source_build": "retail",
        "manifest": args.manifest.resolve().relative_to(ROOT).as_posix(),
        "mesh_count": len(records),
        "meshes": records,
    }
    args.evidence.parent.mkdir(parents=True, exist_ok=True)
    args.evidence.write_text(json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"DISTRICT_MESH_CONVERSION_PASS district={args.district} meshes={len(records)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
