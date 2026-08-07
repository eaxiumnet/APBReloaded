"""Project asset inventory for the APB content studio."""

from __future__ import annotations

import json
from collections import Counter
from functools import lru_cache
from pathlib import Path
from typing import Any

from assets import build_weapon_catalog
from characters import build_character_catalog
from vehicles import build_vehicle_catalog


_MEDIA_EXTS = {".webm", ".mp4", ".mkv", ".mov"}
_MEDIA_SUBDIRS = (
    "LoginAnimatedBackground",
    "LoginAnimatedBackground_ai_upscale",
    "LoginAnimatedBackground_lossless_mkv",
    "LoginAnimatedBackground_webm",
    "LoginAnimatedBackground_webm_4k",
    "LoginAnimatedBackground_webm_lossless",
    "Movies",
)


def _read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def _category(entry: dict[str, Any]) -> str:
    value = " ".join(
        str(entry.get(key) or "")
        for key in ("asset_key", "dest", "destination", "source_locator", "source_package", "consumer_domain", "asset_class")
    ).casefold()
    if "animation" in value or "animset" in value or "animsequence" in value:
        return "animation"
    if "vehicle" in value or "baked_a_" in value:
        return "vehicles"
    if "weapon" in value or "arms" in value:
        return "weapons"
    if "skeleton" in value or "skeletalmesh" in value or "skeletal" in value:
        return "skeletal"
    if "district" in value or "map" in value or "placement" in value:
        return "maps"
    if "material" in value or "texture" in value:
        return "materials"
    if "character" in value or "wardrobe" in value or "clothing" in value:
        return "characters"
    if "audio" in value or "sound" in value or "movie" in value:
        return "media"
    if "ui" in value or "menu" in value:
        return "ui"
    return "other"


def _label(value: str) -> str:
    text = value.replace("\\", "/").rstrip("/").rsplit("/", 1)[-1]
    return text or value


def _preview_maps(root: Path) -> dict[str, Any]:
    """Build the lookups that turn a ledger row into a previewable source.

    Keys are casefolded so ledger names like ``Baked_A_2DrCoupe.upk#EditorVehicle``
    can be normalized before matching (see :func:`_norm_name`).
    """
    weapons = build_weapon_catalog(root / "Content" / "Extracted" / "WeaponsBase")
    vehicles = build_vehicle_catalog(root / "Content" / "Extracted" / "VehiclesBulk")
    characters = build_character_catalog(root / "Content" / "Extracted" / "CharactersBulk")
    return {
        "weapons": {item["id"].casefold(): item["primary"] for item in weapons},
        "vehicles": {item["id"].casefold(): item["primary"] for item in vehicles},
        "characters": {item["id"].casefold(): item["id"] for item in characters},
        "textures": _texture_package_index(root / "Content" / "Extracted" / "MaterialDatabase"),
        "props": _prop_anim_index(root / "Content" / "Extracted" / "MaterialDatabase"),
        "media": _media_index(root / "Content" / "Extracted" / "2011"),
    }


def _norm_name(value: str) -> str:
    """Strip package/object suffixes from a ledger asset name.

    ``Baked_A_2DrCoupe.upk#EditorVehicle`` -> ``baked_a_2drcoupe``;
    ``Character_Select_BG_AI_compat.mp4#hdr10`` -> ``character_select_bg_ai_compat.mp4``.
    """
    text = value.rsplit("#", 1)[0].strip()
    if text.casefold().endswith(".upk"):
        text = text[:-4]
    return text.casefold()


def find_prop_mesh_dir(psa_path: Path) -> Path | None:
    """Walk up from a prop PSA to the nearest ancestor SkeletalMesh3 dir.

    The standard layout is ``MaterialDatabase/<pkg>/<pkg>/{AnimSet,
    SkeletalMesh3}`` but packages may nest differently; never assume the depth.
    """
    for parent in psa_path.parents:
        candidate = parent / "SkeletalMesh3"
        if candidate.is_dir():
            return candidate
    return None


def pick_prop_mesh(mesh_dir: Path, psa_path: Path) -> Path | None:
    """Choose the PSK an animset drives from its sibling mesh dir.

    Prefer a candidate whose stem overlaps the package name (variants and
    LODs rarely carry it); fall back to the largest file (most verts) rather
    than blind alphabetical order.
    """
    if not mesh_dir.is_dir():
        return None
    psks = sorted(mesh_dir.glob("*.psk"))
    if not psks:
        return None
    hint = psa_path.parent.parent.name.casefold()
    for candidate in psks:
        stem = candidate.stem.casefold()
        if hint and (hint in stem or stem in hint):
            return candidate
    return max(psks, key=lambda path: path.stat().st_size)


def _prop_anim_index(md_root: Path) -> dict[str, str]:
    """Map package name -> animset PSA relpath for animated props.

    Prop packages (breakable doors, mailboxes, flag poles) extract their
    AnimSet PSA next to the SkeletalMesh3 PSK they drive. Only packages with
    both are indexed; the preview endpoint resolves the sibling mesh itself.
    """
    if not md_root.is_dir():
        return {}
    index: dict[str, str] = {}
    for package_dir in md_root.iterdir():
        if not package_dir.is_dir():
            continue
        anim_dir = package_dir / package_dir.name / "AnimSet"
        if not anim_dir.is_dir():
            continue
        for psa in sorted(anim_dir.glob("*.psa")):
            if find_prop_mesh_dir(psa) is None:
                continue
            rel = psa.relative_to(md_root).as_posix()
            index.setdefault(package_dir.name.casefold(), rel)
            break
    return index


def _texture_package_index(md_root: Path) -> dict[str, Path]:
    """Map package-name -> extracted package dir under MaterialDatabase.

    Most packages extract to ``MaterialDatabase/<pkg>/<pkg>``; some live one
    level deeper under district folders. The rglob index is built once per
    inventory build (the inventory itself is signature-cached), and parents
    win over same-named children via setdefault ordering.
    """
    if not md_root.is_dir():
        return {}
    index: dict[str, Path] = {}
    for entry in md_root.rglob("*"):
        if entry.is_dir():
            index.setdefault(entry.name.casefold(), entry)
    return index


def _find_texture(asset_name: str, texture_index: dict[str, Path]) -> str | None:
    """Locate the extracted TGA for a ``pkg.upk#obj`` ledger name.

    The returned path keeps the ledger's original casing; filesystem lookups
    stay case-insensitive (Windows) so the two never disagree. Exact hits are
    O(1) stat calls; recursive globs only run inside the (small) package dir.
    """
    fold = asset_name.casefold()
    marker = fold.find(".upk#")
    if marker < 0:
        base = texture_index.get(fold)
        if not base:
            return None
        diffuse = sorted(base.rglob("*_Diff.tga")) + sorted(base.rglob("*_DiffSpec.tga"))
        return str(diffuse[0]) if diffuse else None
    raw_pkg = asset_name[:marker]
    raw_obj = asset_name[marker + 5:]
    base = texture_index.get(raw_pkg.casefold())
    if not base:
        return None
    for texture_dir in (base / raw_pkg / "Texture2D", base / "Texture2D"):
        exact = texture_dir / f"{raw_obj}.tga"
        if exact.is_file():
            return str(exact)
    for pattern in (f"{raw_obj}.tga", f"{raw_obj}*.tga"):
        matches = sorted(base.rglob(pattern))
        if matches:
            return str(matches[0])
    if raw_obj.casefold().startswith(("mi_", "m_")):
        diffuse = sorted(base.rglob("*_Diff.tga")) + sorted(base.rglob("*_DiffSpec.tga"))
        if diffuse:
            return str(diffuse[0])
    return None


def _prop_anim_hint(asset_name: str, prop_index: dict[str, str]) -> dict[str, Any] | None:
    """Resolve an animset-ledger row to an animated prop preview."""
    norm = _norm_name(asset_name)
    psa_rel = prop_index.get(norm)
    if not psa_rel:
        return None
    return {"preview_kind": "prop_animation", "preview_path": psa_rel}


def _static_mesh_hint(asset_name: str, texture_index: dict[str, Path]) -> dict[str, Any] | None:
    """Fall back to the package's own mesh when a row has no texture.

    Covers material-instance rows (``Aerials.upk#MI_...``) and folder-level
    material entries (``2mScaffold``) whose package extracts a SkeletalMesh3.
    """
    norm = _norm_name(asset_name)
    package_dir = texture_index.get(norm)
    if not package_dir:
        return None
    mesh_dir = package_dir / package_dir.name / "SkeletalMesh3"
    if not mesh_dir.is_dir():
        return None
    psks = sorted(mesh_dir.glob("*.psk"))
    if not psks:
        return None
    return {"preview_kind": "static_mesh", "preview_path": str(psks[0])}


def _district_mesh_records(districts_root: Path) -> list[dict[str, Any]]:
    """Scan extracted district static meshes into inventory records.

    Retail district packages hold one ``StaticMesh3/*.psk(x)`` per building
    LOD; LOD_1/LOD_2 are dropped so each building shows as a single row
    (the preview always serves LOD_0).
    """
    if not districts_root.is_dir():
        return []
    by_base: dict[str, Path] = {}
    for mesh_path in districts_root.rglob("*"):
        if mesh_path.suffix.casefold() not in {".psk", ".pskx"}:
            continue
        stem = mesh_path.stem
        if "_LOD_" in stem:
            base, _, tail = stem.rpartition("_LOD_")
            lod = int(tail) if tail.isdigit() else 0
        else:
            base, lod = stem, 0
        previous = by_base.get(base.casefold())
        if previous is None or lod < previous[1]:
            by_base[base.casefold()] = (mesh_path, lod if lod >= 0 else 0)
    records = []
    for base, (mesh_path, _lod) in sorted(by_base.items()):
        rel = mesh_path.relative_to(districts_root.parent).as_posix()
        records.append({
            "id": f"mesh:{rel}",
            "name": mesh_path.stem,
            "category": "maps",
            "status": "extracted",
            "source_build": "retail",
            "source_locator": str(mesh_path),
            "source_sha256": None,
            "source_package": None,
            "source_object": None,
            "asset_class": "StaticMesh",
            "consumer_domain": "district",
            "destination": "/Game/Imported/Districts/" + mesh_path.relative_to(districts_root).with_suffix("").as_posix(),
            "physical": True,
            "provenance": "complete",
            "preview_kind": "static_mesh",
            "preview_path": mesh_path.relative_to(districts_root).as_posix(),
        })
    return records


def _media_index(media_root: Path) -> dict[str, Path]:
    """Map file name -> video path for the 2011 login-media subdirs only.

    The 2011 root also holds extracted map/level trees (100k+ files); walking
    it wholesale would make the inventory build slow, so only the known media
    folders are indexed. Browser-playable twins (webm/mp4) win over mkv
    sources when both share a base name.
    """
    index: dict[str, Path] = {}
    for subdir in _MEDIA_SUBDIRS:
        base = media_root / subdir
        if not base.is_dir():
            continue
        files = [path for path in base.rglob("*") if path.is_file() and path.suffix.casefold() in _MEDIA_EXTS]
        files.sort(key=lambda path: (path.suffix.casefold() == ".mkv", path.name.casefold()))
        for path in files:
            index.setdefault(path.name.casefold(), path)
    return index


def _preview_hint(raw: dict[str, Any], category: str, maps: dict[str, Any]) -> dict[str, Any]:
    key = str(raw.get("asset_key") or "")
    asset_name = _label(key)
    norm = _norm_name(asset_name)
    if category == "weapons" and norm in maps["weapons"]:
        return {"preview_kind": "weapon_mesh", "preview_path": maps["weapons"][norm]}
    if category == "vehicles" and norm in maps["vehicles"]:
        return {"preview_kind": "vehicle_mesh", "preview_path": maps["vehicles"][norm]}
    if category == "characters" and norm in maps["characters"]:
        return {"preview_kind": "character_mesh", "preview_path": maps["characters"][norm]}
    # Login-background videos are split across categories by their file name
    # (many contain "Character_Select"), so match on the file extension rather
    # than the category; otherwise half of them would stay preview-less.
    media_norm = _norm_name(asset_name)
    if media_norm.rsplit(".", 1)[-1] in {"webm", "mp4", "mkv", "mov"}:
        media_path = maps["media"].get(media_norm)
        if media_path:
            return {"preview_kind": "video", "preview_path": str(media_path)}
    texture = _find_texture(asset_name, maps["textures"])
    if texture:
        return {"preview_kind": "texture", "preview_path": texture}
    prop = _prop_anim_hint(asset_name, maps["props"])
    if prop:
        return prop
    mesh = _static_mesh_hint(asset_name, maps["textures"])
    if mesh:
        return mesh
    return {"preview_kind": "none", "preview_path": None}


def build_asset_inventory(repo_root: Path) -> dict[str, Any]:
    root = Path(repo_root).resolve()
    preview_maps = _preview_maps(root)
    ledger_path = root / "tools" / "import_ledger.json"
    imported_root = root / "Content" / "Imported"
    ledger = _read_json(ledger_path) if ledger_path.is_file() else {"entries": []}
    entries = ledger.get("entries") if isinstance(ledger, dict) else []
    records: list[dict[str, Any]] = []
    covered_destinations: set[str] = set()

    for index, raw in enumerate(entries if isinstance(entries, list) else []):
        if not isinstance(raw, dict):
            continue
        dest = str(raw.get("dest") or "")
        covered_destinations.add(dest.casefold())
        category = _category(raw)
        preview = _preview_hint(raw, category, preview_maps)
        records.append({
            "id": f"ledger:{index}",
            "name": _label(str(raw.get("asset_key") or dest or f"asset-{index}")),
            "category": category,
            "status": str(raw.get("status") or "unknown"),
            "source_build": str(raw.get("source_build") or "unknown"),
            "source_locator": raw.get("source_locator"),
            "source_sha256": raw.get("source_sha256"),
            "source_package": raw.get("source_package"),
            "source_object": raw.get("source_object"),
            "asset_class": raw.get("asset_class"),
            "consumer_domain": raw.get("consumer_domain"),
            "destination": dest,
            "physical": False,                "provenance": "complete" if raw.get("source_locator") and raw.get("source_sha256") else "incomplete",
            **preview,
        })

    records.extend(_district_mesh_records(root / "Content" / "Extracted" / "Retail" / "Districts"))

    if imported_root.is_dir():
        for path in imported_root.rglob("*.uasset"):
            relative = path.relative_to(root).as_posix()
            destination = "/Game/Imported/" + path.relative_to(imported_root).with_suffix("").as_posix()
            destination_key = destination.casefold()
            if any(
                destination_key == tracked or destination_key.startswith(tracked.rstrip("/") + "/")
                for tracked in covered_destinations
                if tracked
            ):
                continue
            inferred = {"destination": destination, "asset_key": relative}
            records.append({
                "id": f"file:{relative}",
                "name": path.stem,
                "category": _category(inferred),
                "status": "imported_untracked",
                "source_build": "unknown",
                "source_locator": None,
                "source_sha256": None,
                "source_package": None,
                "source_object": None,
                "asset_class": "uasset",
                "consumer_domain": None,
                "destination": destination,
                "physical": True,
                "provenance": "untracked",
                "preview_kind": "none",
                "preview_path": None,
            })

    records.sort(key=lambda item: (item["category"], item["name"].casefold(), item["id"]))
    counts = Counter(record["category"] for record in records)
    status_counts = Counter(record["status"] for record in records)
    return {
        "total": len(records),
        "categories": dict(sorted(counts.items())),
        "statuses": dict(sorted(status_counts.items())),
        "source_builds": sorted({record["source_build"] for record in records}),
        "assets": records,
    }


def _inventory_signature(root: Path) -> tuple[int, int, int, int, int, int]:
    ledger = root / "tools" / "import_ledger.json"
    imported = root / "Content" / "Imported"
    imported_files = list(imported.rglob("*.uasset")) if imported.is_dir() else []
    newest_imported = max((path.stat().st_mtime_ns for path in imported_files), default=0)
    districts = root / "Content" / "Extracted" / "Retail" / "Districts"
    district_meshes = [
        path for path in districts.rglob("*") if path.suffix.casefold() in {".psk", ".pskx"}
    ] if districts.is_dir() else []
    newest_mesh = max((path.stat().st_mtime_ns for path in district_meshes), default=0)
    return (
        ledger.stat().st_mtime_ns if ledger.exists() else 0,
        imported.stat().st_mtime_ns if imported.exists() else 0,
        newest_imported,
        len(imported_files),
        newest_mesh,
        len(district_meshes),
    )




@lru_cache(maxsize=4)
def _cached_inventory(root_text: str, signature: tuple[int, int, int, int]) -> dict[str, Any]:
    return build_asset_inventory(Path(root_text))


def get_cached_asset_inventory(repo_root: Path) -> dict[str, Any]:
    root = Path(repo_root).resolve()
    return _cached_inventory(str(root), _inventory_signature(root))
