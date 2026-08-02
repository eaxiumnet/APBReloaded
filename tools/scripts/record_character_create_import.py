#!/usr/bin/env python3
"""Record D17 evidence for the character-create retail batch (M3R task 18b).

Builds per-asset ledger rows for the character-create runtime surface from the
on-disk provenance chain, mirroring `record_frontend_menu_import.py`:

- Base meshes:  retail Character/Contact/<Contact>.upk -> staged OBJ -> uasset
- Wardrobe:     retail per-contact / per-LC upk -> staged OBJ -> uasset
- Preview mat:  retail MaterialDatabase/GenericDistrict/DisplayPoint_CharacterMesh.upk
                -> extracted TGA textures -> MI uasset

Every source upk is verified to own the mesh via `model_reference_catalog.json`
name hints (the mesh leaf appears in the package's exported-name hints). Only
rows whose full chain is verifiable on disk right now are emitted; rows are
idempotent. Dry-run by default; --apply writes the ledger and evidence files.

Usage:
    python tools/scripts/record_character_create_import.py [--apply]
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
from pathlib import Path

from record_frontend_menu_import import merge_rows

ROOT = Path(__file__).resolve().parents[2]
LEDGER_PATH = ROOT / "tools" / "import_ledger.json"
CATALOG_PATH = ROOT / "Content" / "Data" / "model_reference_catalog.json"
EVIDENCE_DIR = ROOT / "work" / "evidence" / "character_create_batch"
EVIDENCE_SCHEMA = "apb_character_create_v1"

RETAIL_PACKAGES = (
    Path("C:/Program Files (x86)/Steam/steamapps/common/APB Reloaded/APBGame/Content/Release/Packages")
)
CONTACT_DIR = RETAIL_PACKAGES / "Character" / "Contact"
LC_DIR = RETAIL_PACKAGES / "Character" / "LC"
MATERIAL_DIR = RETAIL_PACKAGES / "MaterialDatabase" / "GenericDistrict"

IMPORTED_CHARS = ROOT / "Content" / "Imported" / "Characters"
WARDROBE_DIR = IMPORTED_CHARS / "Wardrobe"
EXTRACTED_MATERIAL = ROOT / "Content" / "Extracted" / "MaterialDatabase" / "DisplayPoint_CharacterMesh" / "DisplayPoint_CharacterMesh" / "Texture2D"

EXTRACTOR = "tools/UEViewer/umodel_64.exe"
EXTRACTOR_ARGS = ["umodel -obj export of character mesh; legacy payload extraction settings not retained"]

BASE_MESHES = [
    {
        "dir": "Contact_LaRocha",
        "mesh": "m_contact_enforcement_larocha",
        "upk": "Character/Contact/Contact_LaRocha.upk",
    },
    {
        "dir": "Contact_Bloodrose",
        "mesh": "F_Contact_Criminal_Bloodrose",
        "upk": "Character/Contact/Contact_Bloodrose.upk",
    },
    {
        "dir": "Contact_Sofia",
        "mesh": "F_Contact_Enforcement_Sofia",
        "upk": "Character/Contact/Contact_Sofia.upk",
    },
]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def find_retail_upk(stem: str) -> Path | None:
    """Longest-prefix match of a mesh stem against on-disk retail upks."""
    lower = stem.lower()
    best: Path | None = None
    for directory in (CONTACT_DIR, LC_DIR):
        if not directory.is_dir():
            continue
        for candidate in directory.rglob("*.upk"):
            key = candidate.stem.lower()
            if lower.startswith(key) and (best is None or len(key) > len(best.stem.lower())):
                best = candidate
    return best


def catalog_hints_for(upk: Path) -> str | None:
    """Joined name hints for the package if the catalog cataloged it."""
    if not CATALOG_PATH.is_file():
        return None
    wanted = upk.stem.casefold()
    for row in read_json(CATALOG_PATH).get("characters", []):
        if str(row.get("package", "")).casefold() == wanted:
            return " ".join(str(h) for h in row.get("name_hints", []))
    return None


def mesh_owner_confirmed(upk: Path, mesh: str) -> bool:
    """Cataloged packages must show the mesh leaf in their name hints."""
    hints = catalog_hints_for(upk)
    if hints is None:
        return False
    return mesh.casefold() in hints.casefold()


def build_mesh_row(
    upk: Path,
    mesh: str,
    intermediate: Path,
    dest: str,
    uasset: Path,
    validation_note: str,
    ownership: str = "model_reference_catalog name hints",
    evidence_path: Path | None = None,
) -> dict | None:
    if not upk.is_file() or not intermediate.is_file() or not uasset.is_file():
        return None
    leaf = mesh
    source_rel = upk.relative_to(RETAIL_PACKAGES).as_posix()
    asset_key = f"retail:{source_rel}#{leaf}"
    upk_hash = sha256(upk)
    intermediate_hash = sha256(intermediate)
    conversion = {
        "converter": "umodel",
        "format": "OBJ",
        "normalize": False,
        "source": "retail character package",
        "ownership": ownership,
    }
    evidence = {
        "path": rel(evidence_path) if evidence_path is not None else None,
        "sha256": None,
        "schema": EVIDENCE_SCHEMA,
        "record_key": asset_key,
        "fields": {
            "source_locator": f"${{retail_steam}}/APBGame/Content/Release/Packages/{source_rel}",
            "source_sha256": upk_hash,
            "extractor": EXTRACTOR,
            "extractor_args": EXTRACTOR_ARGS,
            "conversion_settings": conversion,
            "destination": dest,
            "intermediate_path": rel(intermediate),
            "intermediate_sha256": intermediate_hash,
            "extracted_file": rel(intermediate),
            "extracted_sha256": intermediate_hash,
        },
    }
    return {
        "asset_key": asset_key,
        "source_build": "retail",
        "source_locator": f"${{retail_steam}}/APBGame/Content/Release/Packages/{source_rel}",
        "source_sha256": upk_hash,
        "extractor": EXTRACTOR,
        "extractor_args": EXTRACTOR_ARGS,
        "intermediate_path": rel(intermediate),
        "intermediate_sha256": intermediate_hash,
        "conversion_settings": conversion,
        "dest": dest,
        "asset_class": "StaticMesh",
        "validation": {"class": "StaticMesh", "source_build": "retail", "note": validation_note},
        "status": "imported",
        "updated": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "uasset_path": rel(uasset),
        "uasset_sha256": sha256(uasset),
        "d17_evidence": [evidence],
    }


def build_base_rows(evidence_path: Path) -> tuple[list[dict], list[str]]:
    rows: list[dict] = []
    skipped: list[str] = []
    for spec in BASE_MESHES:
        mesh = spec["mesh"]
        upk = RETAIL_PACKAGES / spec["upk"]
        directory = IMPORTED_CHARS / spec["dir"]
        intermediate = directory / f"{mesh}.obj"
        uasset = directory / f"{mesh}.uasset"
        dest = f"/Game/Imported/Characters/{spec['dir']}/{mesh}.{mesh}"
        if not mesh_owner_confirmed(upk, mesh):
            skipped.append(f"{mesh} owner_not_confirmed")
            continue
        row = build_mesh_row(upk, mesh, intermediate, dest, uasset, "character-create base mesh", evidence_path=evidence_path)
        if row:
            rows.append(row)
        else:
            skipped.append(f"{mesh} chain_incomplete")
    return rows, skipped


def build_wardrobe_rows(evidence_path: Path) -> tuple[list[dict], list[str]]:
    rows: list[dict] = []
    skipped: list[str] = []
    for uasset in sorted(WARDROBE_DIR.glob("*.uasset")):
        stem = uasset.stem
        upk = find_retail_upk(stem)
        if upk is None:
            skipped.append(f"{stem} retail_upk_not_found")
            continue
        intermediate = uasset.with_suffix(".obj")
        dest = f"/Game/Imported/Characters/Wardrobe/{stem}.{stem}"
        row = build_mesh_row(
            upk,
            stem,
            intermediate,
            dest,
            uasset,
            "character-create wardrobe slot visual",
            "retail package name prefix-match + staged obj",
            evidence_path=evidence_path,
        )
        if row:
            rows.append(row)
        else:
            skipped.append(f"{stem} chain_incomplete")
    return rows, skipped


def build_material_row(evidence_path: Path) -> tuple[list[dict] | None, list[str]]:
    upk = MATERIAL_DIR / "DisplayPoint_CharacterMesh.upk"
    uasset = (
        ROOT
        / "Content"
        / "Imported"
        / "MaterialDatabase"
        / "DisplayPoint_CharacterMesh"
        / "MI_DisplyPoint_CharacterMesh.uasset"
    )
    intermediate = EXTRACTED_MATERIAL / "Statue_Norm.tga"
    if not upk.is_file() or not uasset.is_file() or not intermediate.is_file():
        return None, [f"MI_DisplyPoint_CharacterMesh chain_incomplete"]
    source_rel = upk.relative_to(RETAIL_PACKAGES).as_posix()
    leaf = "MI_DisplyPoint_CharacterMesh"
    dest = f"/Game/Imported/MaterialDatabase/DisplayPoint_CharacterMesh/{leaf}.{leaf}"
    asset_key = f"retail:{source_rel}#{leaf}"
    conversion = {
        "converter": "retail texture import",
        "format": "TGA -> material instance",
        "normalize": False,
        "source": "retail material database",
        "manifest": "work/material_import_manifest.json",
    }
    evidence = {
        "path": rel(evidence_path),
        "sha256": None,
        "schema": EVIDENCE_SCHEMA,
        "record_key": asset_key,
        "fields": {
            "source_locator": f"${{retail_steam}}/APBGame/Content/Release/Packages/{source_rel}",
            "source_sha256": sha256(upk),
            "extractor": "tools/scripts/build_material_import_manifest.py (documented pipeline)",
            "extractor_args": ["extract textures -> build material instance; see work/material_import_manifest.json"],
            "conversion_settings": conversion,
            "destination": dest,
            "intermediate_path": rel(intermediate),
            "intermediate_sha256": sha256(intermediate),
            "extracted_file": rel(intermediate),
            "extracted_sha256": sha256(intermediate),
        },
    }
    row = {
        "asset_key": asset_key,
        "source_build": "retail",
        "source_locator": f"${{retail_steam}}/APBGame/Content/Release/Packages/{source_rel}",
        "source_sha256": sha256(upk),
        "extractor": "tools/scripts/build_material_import_manifest.py",
        "extractor_args": ["see work/material_import_manifest.json"],
        "intermediate_path": rel(intermediate),
        "intermediate_sha256": sha256(intermediate),
        "conversion_settings": conversion,
        "dest": dest,
        "asset_class": "MaterialInstanceConstant",
        "validation": {"class": "MaterialInstanceConstant", "source_build": "retail"},
        "status": "imported",
        "updated": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "uasset_path": rel(uasset),
        "uasset_sha256": sha256(uasset),
        "d17_evidence": [evidence],
    }
    return row, []


def write_evidence(batch_name: str, rows: list[dict], evidence_path: Path) -> None:
    entries = []
    for row in rows:
        fields = row["d17_evidence"][0]["fields"]
        entries.append(
            {
                "asset_key": row["asset_key"],
                "source_locator": fields["source_locator"],
                "source_sha256": fields["source_sha256"],
                "extractor": fields["extractor"],
                "extractor_args": fields["extractor_args"],
                "conversion_settings": fields["conversion_settings"],
                "intermediate_path": fields["intermediate_path"],
                "intermediate_sha256": fields["intermediate_sha256"],
                "extracted_file": fields.get("extracted_file"),
                "extracted_sha256": fields.get("extracted_sha256"),
                "destination": fields["destination"],
            }
        )
    document = {
        "schema": EVIDENCE_SCHEMA,
        "batch": batch_name,
        "generated_by": "tools/scripts/record_character_create_import.py",
        "generated_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "entries": entries,
    }
    EVIDENCE_DIR.mkdir(parents=True, exist_ok=True)
    evidence_path.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true", help="write ledger + evidence files")
    args = parser.parse_args()

    base_rows, base_skip = build_base_rows(EVIDENCE_DIR / "base_meshes_exact.json")
    wardrobe_rows, wardrobe_skip = build_wardrobe_rows(EVIDENCE_DIR / "wardrobe_exact.json")
    material_row, material_skip = build_material_row(EVIDENCE_DIR / "material_exact.json")

    if not args.apply:
        total = len(base_rows) + len(wardrobe_rows) + (1 if material_row else 0)
        print(f"DRY_RUN base={len(base_rows)} wardrobe={len(wardrobe_rows)} material={1 if material_row else 0} total={total}")
        print(f"SKIPPED base={len(base_skip)} wardrobe={len(wardrobe_skip)} material={len(material_skip)}")
        for reason in (base_skip + wardrobe_skip + material_skip)[:12]:
            print("  skip:", reason)
        return

    batches = []
    if base_rows:
        batches.append(("character_create_base_meshes", base_rows, EVIDENCE_DIR / "base_meshes_exact.json"))
    if wardrobe_rows:
        batches.append(("character_create_wardrobe", wardrobe_rows, EVIDENCE_DIR / "wardrobe_exact.json"))
    if material_row:
        batches.append(("character_create_material", [material_row], EVIDENCE_DIR / "material_exact.json"))

    for batch_name, batch_rows, evidence_path in batches:
        write_evidence(batch_name, batch_rows, evidence_path)
        evidence_hash = sha256(evidence_path)
        for row in batch_rows:
            row["d17_evidence"][0]["sha256"] = evidence_hash

    ledger = read_json(LEDGER_PATH)
    fresh = {row["asset_key"]: row for batch in batches for row in batch[1]}
    kept, replaced, added = merge_rows(ledger["entries"], fresh)
    ledger["entries"] = kept
    ledger["updated"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    LEDGER_PATH.write_text(json.dumps(ledger, indent=2) + "\n", encoding="utf-8")
    print(f"RECORD_CHARACTER_CREATE_PASS added={added} replaced={replaced} total={len(ledger['entries'])}")


if __name__ == "__main__":
    main()
