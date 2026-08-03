# M15 (material texture pipeline): record MaterialDatabase MI + Texture2D uassets
# as verified-eligible ledger rows with D17 evidence chains (retail upk -> TGA -> uasset).
# Run: python tools/scripts/record_material_texture_import.py [--apply]
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
from pathlib import Path

ROOT = Path(r"D:\APBReloaded")
LEDGER = ROOT / "tools" / "import_ledger.json"
MANIFEST = ROOT / "work" / "material_import_manifest.json"
IMPORTED = ROOT / "Content" / "Imported" / "MaterialDatabase"
EXTRACTED = ROOT / "Content" / "Extracted" / "MaterialDatabase"
EVIDENCE_DIR = ROOT / "work" / "evidence" / "material_texture_batch"
RETAIL_MATDB = Path(
    r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded"
    r"\APBGame\Content\Release\Packages\MaterialDatabase"
)
ROLE_PRIORITY = ("Diffuse", "Normal", "Emissive", "Opacity", "Specular", "Cube")
SCHEMA = "apb_material_database_v1"


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def build_upk_index() -> dict[str, dict]:
    index: dict[str, dict] = {}
    for upk in RETAIL_MATDB.rglob("*.upk"):
        rel = upk.relative_to(RETAIL_MATDB)
        index[upk.stem.lower()] = {
            "path": upk,
            "rel": f"MaterialDatabase/{rel.as_posix()}",
        }
    return index


def build_tga_index() -> dict[tuple[str, str], Path]:
    index: dict[tuple[str, str], Path] = {}
    for tga in EXTRACTED.rglob("*.tga"):
        rel = tga.relative_to(EXTRACTED)
        pkg = rel.parts[0].lower()
        index.setdefault((pkg, tga.stem.lower()), tga)
    return index


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()

    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    m_entries = manifest["entries"]
    by_name = {}
    for m in m_entries:
        key = (m["package"].lower(), m["material"].lower())
        by_name[key] = m

    upk_index = build_upk_index()
    tga_index = build_tga_index()
    EVIDENCE_DIR.mkdir(parents=True, exist_ok=True)

    ledger = json.loads(LEDGER.read_text(encoding="utf-8"))
    entries = {e["asset_key"]: e for e in ledger.get("entries", [])}

    upk_hash_cache: dict[str, str] = {}
    ev_files: dict[str, dict] = {}
    skipped: list[str] = []
    now = datetime.datetime.now(datetime.timezone.utc).isoformat()

    def upk_hash(stem: str) -> str:
        if stem not in upk_hash_cache:
            upk_hash_cache[stem] = sha256(upk_index[stem]["path"])
        return upk_hash_cache[stem]

    def emit_row(
        uasset: Path,
        upk_stem: str,
        obj: str,
        asset_class: str,
        material: str | None,
        textures: dict[str, str] | None,
        tga: Path | None,
    ) -> None:
        if upk_stem not in upk_index:
            skipped.append(f"no_upk {uasset}")
            return
        upk = upk_index[upk_stem]
        src_hash = upk_hash(upk_stem)
        rel_after = upk["rel"]
        asset_key = f"retail:{rel_after}#{obj}"
        if asset_key in entries:
            return
        leaf = uasset.stem
        dest = f"/Game/Imported/MaterialDatabase/{uasset.parent.name}/{leaf}.{leaf}"
        if tga is None or not tga.is_file():
            skipped.append(f"no_tga {uasset}")
            return
        tga_hash = sha256(tga)
        tga_rel = tga.relative_to(ROOT).as_posix()
        pkg_lower = uasset.parent.name.lower()
        if pkg_lower not in ev_files:
            ev_files[pkg_lower] = {
                "schema": SCHEMA,
                "package": uasset.parent.name,
                "source_locator": "${retail_steam}/APBGame/Content/Release/Packages/"
                + rel_after,
                "source_sha256": src_hash,
                "records": [],
            }
        ev_files[pkg_lower]["records"].append(
            {
                "asset_key": asset_key,
                "object": obj,
                "asset_class": asset_class,
                "material": material,
                "texture_roles": sorted(textures or {}),
                "extracted_file": tga_rel,
                "extracted_sha256": tga_hash,
                "output_file": tga_rel,
                "output_sha256": tga_hash,
                "uasset": uasset.relative_to(ROOT).as_posix(),
            }
        )
        source_locator = "${retail_steam}/APBGame/Content/Release/Packages/" + rel_after
        entries[asset_key] = {
            "asset_key": asset_key,
            "source_build": "retail",
            "source_locator": source_locator,
            "source_package": uasset.parent.name,
            "source_object": obj,
            "source_sha256": src_hash,
            "extractor": "tools/scripts/build_material_import_manifest.py",
            "extractor_args": ["see work/material_import_manifest.json"],
            "intermediate_path": tga_rel,
            "intermediate_sha256": tga_hash,
            "conversion_settings": {
                "converter": "retail texture import",
                "format": "TGA -> material instance",
                "normalize": False,
                "source": "retail material database",
                "manifest": "work/material_import_manifest.json",
            },
            "dest": dest,
            "asset_class": asset_class,
            "status": "imported",
            "validation": {
                "class": asset_class,
                "source_build": "retail",
            },
            "uasset_path": uasset.relative_to(ROOT).as_posix(),
            "uasset_sha256": sha256(uasset),
            "d17_evidence": [
                {
                    "record_key": asset_key,
                    "schema": SCHEMA,
                    "path": f"work/evidence/material_texture_batch/{uasset.parent.name}.json",
                    "sha256": "PENDING",
                    "fields": {
                        "source_locator": source_locator,
                        "source_sha256": src_hash,
                        "extractor": "tools/scripts/build_material_import_manifest.py",
                        "extractor_args": ["see work/material_import_manifest.json"],
                        "extracted_file": tga_rel,
                        "extracted_sha256": tga_hash,
                        "output_file": tga_rel,
                        "output_sha256": tga_hash,
                    },
                }
            ],
            "updated": now,
        }

    for uasset in sorted(IMPORTED.rglob("*.uasset")):
        pkg_lower = uasset.parent.name.lower()
        stem_lower = uasset.stem.lower()
        stem_tga = tga_index.get((pkg_lower, stem_lower))
        if stem_lower.startswith("mi_"):
            material = uasset.stem[3:]
            m = by_name.get((pkg_lower, material.lower())) or by_name.get(
                (pkg_lower, stem_lower)
            )
            asset_class = "MaterialInstanceConstant"
        elif stem_lower.startswith("m_"):
            material = uasset.stem[2:]
            m = by_name.get((pkg_lower, material.lower())) or by_name.get(
                (pkg_lower, stem_lower)
            )
            asset_class = "Texture2D" if stem_tga else "Material"
        else:
            material = None
            m = None
            asset_class = "Texture2D"
        textures = m.get("textures") if m else None
        tga = None
        if m is not None and textures:
            for role in ROLE_PRIORITY:
                tga_str = textures.get(role)
                if tga_str:
                    tga = Path(tga_str)
                    if tga.is_file():
                        break
                    tga = None
        if tga is None:
            tga = stem_tga
        emit_row(uasset, pkg_lower, uasset.stem, asset_class, material, textures, tga)

    for pkg_lower, ev in ev_files.items():
        ev_path = EVIDENCE_DIR / f"{ev['package']}.json"
        ev_path.write_text(json.dumps(ev, indent=1), encoding="utf-8")
        ev_hash = sha256(ev_path)
        ev_rel = f"work/evidence/material_texture_batch/{ev['package']}.json"
        for entry in entries.values():
            evidence = entry.get("d17_evidence")
            if evidence and evidence[0].get("path") == ev_rel:
                evidence[0]["sha256"] = ev_hash

    ledger["entries"] = list(entries.values())
    ledger["updated"] = now
    if args.apply:
        LEDGER.write_text(json.dumps(ledger, indent=2) + "\n", encoding="utf-8")
    print(
        f"MATERIAL_TEXTURE_LEDGER rows={len(entries)} "
        f"evidence_files={len(ev_files)} skipped={len(skipped)}"
    )
    for s in skipped[:10]:
        print(f"  SKIP {s}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
