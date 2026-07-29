#!/usr/bin/env -S uv run
# /// script
# requires-python = ">=3.11"
# ///
# ─── How to run ───
# uv run tools/scripts/build_mesh_material_bindings.py --force
from __future__ import annotations

import json
import sys
from datetime import UTC, datetime
from pathlib import Path

from mesh_material_binding_parser import (
    BindingError,
    JsonDict,
    build_document,
    chunk_stem,
    dump_package,
    package_district,
    parse_dump,
    read_chunks,
    write_document,
)

ROOT = Path(r"D:\APBReloaded")
PACKAGES_ROOT = Path(
    r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Content\Release\Packages"
)
UMODEL = ROOT / "tools" / "UEViewer" / "umodel_64.exe"
PACKAGE_LIST = ROOT / "work" / "district_packages.txt"
MATERIAL_DATABASE = ROOT / "Content" / "Extracted" / "MaterialDatabase"
IMPORTED_DISTRICTS = ROOT / "Content" / "Imported" / "Districts"
WORK, LOGS = ROOT / "work", ROOT / "work" / "logs"
CHUNKS, DUMPS = LOGS / "binding_chunks", LOGS / "binding_dumps"
OUTPUT, LOG = WORK / "mesh_material_bindings.json", LOGS / "bindings.log"


def write_log(message: str) -> None:
    line = f"{datetime.now(UTC).isoformat()} {message}"
    with LOG.open("a", encoding="utf-8", newline="\n") as handle:
        handle.write(f"{line}\n")
    print(line, flush=True)


def imported_index() -> dict[str, list[str]]:
    index: dict[str, list[str]] = {}
    for district in ("Financial", "Waterfront"):
        for asset in (IMPORTED_DISTRICTS / district).rglob("*_LOD_0*.uasset"):
            index.setdefault(asset.stem, [])
            if district not in index[asset.stem]:
                index[asset.stem].append(district)
    return index


def material_directories() -> dict[str, str]:
    return {entry.name.lower(): entry.name for entry in MATERIAL_DATABASE.iterdir() if entry.is_dir()}


def log_summary(document: JsonDict, label: str) -> None:
    meshes = document["meshes"]
    slots = [slot for mesh in meshes for slot in mesh["slots"]]
    resolved = [slot for slot in slots if slot["resolved"]]
    unresolved = sorted({
        str(slot["material_package"])
        if slot["material_package"] is not None
        else str(slot.get("reason", "<unknown>"))
        for slot in slots if not slot["resolved"]
    })
    percentage = 0.0 if not slots else round(100 * len(resolved) / len(slots), 2)
    write_log(
        f"{label} meshes={len(meshes)} slots={len(slots)} resolved={len(resolved)} "
        f"resolved_percent={percentage} unresolved_packages={'; '.join(unresolved)} output={OUTPUT}"
    )


def main() -> int:
    force = "--force" in sys.argv
    reconcile = "--reconcile" in sys.argv
    only_package = next((value for value in sys.argv[1:] if value.lower().endswith(".upk")), "")
    for directory in (LOGS, CHUNKS, DUMPS):
        directory.mkdir(parents=True, exist_ok=True)
    imported, directories = imported_index(), material_directories()
    if reconcile:
        document = build_document(read_chunks(CHUNKS), imported, directories)
        write_document(OUTPUT, document)
        log_summary(document, "RECONCILE")
        return 0
    packages = [Path(line.strip()) for line in PACKAGE_LIST.read_text(encoding="utf-8").splitlines() if line.strip()]
    if only_package:
        packages = [path for path in packages if path.name.lower() == only_package.lower()]
    if not packages:
        raise BindingError("No listed packages selected")

    write_log(
        f"START packages={len(packages)} imported_lod0_stems={len(imported)} "
        f"material_database_dirs={len(directories)} force={force}"
    )
    for index, package_path in enumerate(packages, start=1):
        stem = chunk_stem(package_path)
        chunk_path = CHUNKS / f"{stem}.json"
        if chunk_path.exists() and not force:
            write_log(f"SKIP {index}/{len(packages)} package={package_path.stem} checkpoint={chunk_path}")
            continue
        dump_path = DUMPS / f"{stem}.txt"
        write_log(f"DUMP {index}/{len(packages)} package={package_path.stem}")
        dump_package(UMODEL, PACKAGES_ROOT, package_path, dump_path)
        meshes, loads = parse_dump(dump_path)
        chunk = {
            "package": package_path.stem, "package_path": str(package_path),
            "district": package_district(package_path), "meshes": meshes, "material_loads": loads,
        }
        with chunk_path.open("w", encoding="utf-8", newline="\n") as handle:
            json.dump(chunk, handle, indent=2)
            handle.write("\n")
        document = build_document(read_chunks(CHUNKS), imported, directories)
        write_document(OUTPUT, document)
        write_log(f"PARSED {index}/{len(packages)} package={package_path.stem} static_meshes={len(meshes)}")

    document = build_document(read_chunks(CHUNKS), imported, directories)
    write_document(OUTPUT, document)
    log_summary(document, "COMPLETE")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
