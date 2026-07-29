from __future__ import annotations

import hashlib
import json
import re
import subprocess
from datetime import UTC, datetime
from pathlib import Path

JsonAtom = str | int | float | bool | None
JsonValue = JsonAtom | list["JsonValue"] | dict[str, "JsonValue"]
JsonDict = dict[str, JsonValue]


class BindingError(Exception):
    def __init__(self, message: str) -> None:
        super().__init__(message)


MATERIAL_LOAD = re.compile(
    r"^Loading\s+(?P<klass>Material\w*)\s+(?P<name>.+?)\s+from package\s+(?P<package>.+?)\s*$"
)
MESH_HEADER = re.compile(r"^ClassName:\s+(?P<klass>\S+)\s+ObjectName:\s+(?P<name>.+?)\s*$")
COMPACT_SECTION = re.compile(
    r"^\s*Sections\[(?P<index>\d+)\]\s*=\s*\{\s*Material\s*=\s*"
    r"(?P<klass>Material\w*)'(?P<name>[^']+)'"
)
SECTION_HEADER = re.compile(r"^\s*Sections\[(?P<index>\d+)\]\s*=\s*$")
SECTION_MATERIAL = re.compile(r"^\s*Material\s*=\s*(?P<klass>Material\w*)'(?P<name>[^']+)'\s*$")


def chunk_stem(package_path: Path) -> str:
    digest = hashlib.sha1(str(package_path).lower().encode("utf-8")).hexdigest()[:12]
    return f"{package_path.stem}_{digest}"


def package_district(package_path: Path) -> str:
    lowered = str(package_path).lower()
    if "waterfront" in lowered:
        return "Waterfront"
    if "financial" in lowered:
        return "Financial"
    raise BindingError(f"No Financial/Waterfront marker in listed package: {package_path}")


def sanitize_object_name(name: str) -> str:
    # Mirror the import tool's .uasset filename sanitization:
    # embedded spaces become underscores and parentheses are stripped, so
    # umodel ObjectName "High-street (shop) (MC)_0001_LOD_0" maps to the
    # on-disk stem "High-street_shop_MC_0001_LOD_0".
    return name.replace(" ", "_").replace("(", "").replace(")", "")


def disk_candidates(object_name: str) -> list[str]:
    # Candidate on-disk .uasset stems for a raw umodel StaticMesh3 ObjectName.
    # Placements/imports prepend "ROAD_" to procedural road-tile meshes whose
    # umodel ObjectName is bare "Road_Tile_..."; emit both so the identity gate
    # matches either form. Order is stable; first on-disk hit wins.
    base = sanitize_object_name(object_name)
    candidates = [base]
    if not base.startswith("ROAD_"):
        candidates.append(f"ROAD_{base}")
    return candidates


def parse_dump(dump_path: Path) -> tuple[list[JsonDict], dict[str, list[str]]]:
    meshes: list[JsonDict] = []
    loads: dict[str, list[str]] = {}
    current_mesh: str | None = None
    current_slots: dict[int, JsonDict] = {}
    pending_section: int | None = None

    def finish_mesh() -> None:
        nonlocal current_mesh, current_slots, pending_section
        if current_mesh is not None:
            meshes.append({"mesh": current_mesh, "raw_slots": list(current_slots.values())})
        current_mesh = None
        current_slots = {}
        pending_section = None

    with dump_path.open(encoding="utf-8", errors="replace") as handle:
        for line in handle:
            load_match = MATERIAL_LOAD.match(line)
            if load_match is not None:
                key = f"{load_match['klass'].lower()}|{load_match['name'].lower()}"
                package_name = Path(load_match["package"].strip()).stem
                packages = loads.setdefault(key, [])
                if package_name not in packages:
                    packages.append(package_name)
                continue
            mesh_match = MESH_HEADER.match(line)
            if mesh_match is not None:
                finish_mesh()
                if mesh_match["klass"] == "StaticMesh3":
                    current_mesh = mesh_match["name"]
                continue
            if current_mesh is None:
                continue
            compact_match = COMPACT_SECTION.match(line)
            if compact_match is not None:
                index = int(compact_match["index"])
                current_slots[index] = {
                    "index": index,
                    "material": compact_match["name"],
                    "material_class": compact_match["klass"],
                }
                pending_section = None
                continue
            section_match = SECTION_HEADER.match(line)
            if section_match is not None:
                pending_section = int(section_match["index"])
                continue
            material_match = SECTION_MATERIAL.match(line)
            if material_match is not None and pending_section is not None:
                current_slots[pending_section] = {
                    "index": pending_section,
                    "material": material_match["name"],
                    "material_class": material_match["klass"],
                }
                pending_section = None
    finish_mesh()
    return meshes, loads


def dump_package(umodel: Path, packages_root: Path, package_path: Path, dump_path: Path) -> None:
    with dump_path.open("w", encoding="utf-8", newline="\n") as output:
        result = subprocess.run(
            [str(umodel), f"-path={packages_root}", "-game=apb", "-dump", package_path.name],
            cwd=umodel.parent,
            stdout=output,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
            timeout=600,
        )
    if result.returncode != 0:
        raise BindingError(f"umodel exit={result.returncode} package={package_path}")


def read_chunks(chunk_directory: Path) -> list[JsonDict]:
    chunks: list[JsonDict] = []
    for path in sorted(chunk_directory.glob("*.json")):
        with path.open(encoding="utf-8") as handle:
            chunks.append(json.load(handle))
    return chunks


def build_document(
    chunks: list[JsonDict],
    imported: dict[str, list[str]],
    directories: dict[str, str],
) -> JsonDict:
    source_meshes: dict[tuple[str, str], list[tuple[JsonDict, JsonDict]]] = {}
    for chunk in chunks:
        for mesh in chunk["meshes"]:
            matched_stem = next(
                (stem for stem in disk_candidates(str(mesh["mesh"])) if stem in imported),
                None,
            )
            if matched_stem is None:
                continue
            for district in imported[matched_stem]:
                source_meshes.setdefault((district, matched_stem), []).append((chunk, mesh))

    output_meshes: list[JsonDict] = []
    for (district, mesh_name), sources in sorted(source_meshes.items()):
        source_packages = sorted({str(chunk["package"]) for chunk, _ in sources})
        canonical_chunk, canonical_mesh = sources[0]
        slots: list[JsonDict] = []
        for raw_slot in sorted(canonical_mesh["raw_slots"], key=lambda item: int(item["index"])):
            material = str(raw_slot["material"])
            material_class = str(raw_slot["material_class"])
            loads = canonical_chunk["material_loads"]
            load_key = f"{material_class.lower()}|{material.lower()}"
            candidates = sorted(set(loads.get(load_key, [])))
            if len(candidates) == 0 and "." in material:
                suffix = material.rsplit(".", maxsplit=1)[1].lower()
                candidates = sorted({
                    package_name
                    for key, package_names in loads.items()
                    if key == f"{material_class.lower()}|{suffix}"
                    for package_name in package_names
                })
            material_package: str | None = None
            reason: str | None = None
            resolved = False
            if len(source_packages) > 1:
                reason = f"mesh name appears in multiple source packages: {', '.join(source_packages)}"
            elif len(candidates) == 0:
                reason = "no material package load record for explicit material object"
            elif len(candidates) > 1:
                reason = f"ambiguous material package load records: {', '.join(candidates)}"
            else:
                material_package = candidates[0]
                resolved_name = directories.get(material_package.lower())
                if resolved_name is None:
                    reason = "referenced package is not present under Content/Extracted/MaterialDatabase"
                else:
                    material_package, resolved = resolved_name, True
            slot: JsonDict = {
                "index": int(raw_slot["index"]), "material": material,
                "material_package": material_package, "resolved": resolved,
            }
            if reason is not None:
                slot["reason"] = reason
            slots.append(slot)
        output_meshes.append({
            "mesh": mesh_name, "package": canonical_chunk["package"],
            "district": district, "slots": slots,
        })
    return {
        "generated": datetime.now(UTC).isoformat(),
        "method": (
            "umodel -dump property tree: explicit StaticMesh3 Lods[0].Sections[index].Material names; "
            "source material packages from the same dump's Loading Material* records. Mesh identifiers "
            "preserve exact imported .uasset filename stems ending _LOD_0. Package matching is "
            "case-insensitive against immediate MaterialDatabase directory names; ambiguous or absent "
            "package evidence is unresolved."
        ),
        "meshes": output_meshes,
    }


def write_document(output: Path, document: JsonDict) -> None:
    temporary = output.with_suffix(".json.tmp")
    with temporary.open("w", encoding="utf-8", newline="\n") as handle:
        json.dump(document, handle, indent=2)
        handle.write("\n")
    temporary.replace(output)
