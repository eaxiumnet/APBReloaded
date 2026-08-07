"""Bake the content studio into a fully static, backend-free bundle.

The live studio runs a FastAPI backend that reads the extracted retail assets
(Content/Extracted) and converts PSK -> GLB / TGA -> PNG on demand. That means
every visitor needs the repo + the extracted tree + two servers. This script
does all of the conversion *offline* (where the assets live) and emits a plain
directory of JSON catalogs, GLB meshes and PNG textures. The web app built with
`--mode static` (VITE_STATIC=1) serves every /api URL from that bundle, so the
studio runs from any static host with zero downloads.

Every GLB/textureis produced by calling the same FastAPI endpoint functions from main.py, so the baked output is byte-identical in behavior to the live
backend.

Usage (from tools/content-studio/server, with the .venv active):

    python bake_static.py                              # curated showcase
    python bake_static.py --all                        # full catalogs (large)
    python bake_static.py --out ../studio-static --web-dist ../web/dist

    --web-dist copies a prebuilt `vite build --mode static` into the bundle
    (index.html + assets/), so the output is directly deployable.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
import time
from pathlib import Path

import main as backend  # noqa: F401  (importing main also wires its path constants)

from animations import build_animation_catalog
from assets import build_weapon_catalog
from characters import build_character_catalog
from colmask import build_clothing_catalog, find_colmask_for_item
from inventory import (
    _district_mesh_records,
    _prop_anim_index,
    get_cached_asset_inventory,
)
from symbols import build_symbol_catalog
from vehicles import build_part_catalog, build_vehicle_catalog, vehicle_wheel_base

# ---------------------------------------------------------------------------
# Deterministic filename slug — MUST match src/api.ts::slug so the frontend
# shim resolves the same paths the bake wrote.
# ---------------------------------------------------------------------------
_SLUG_RE = re.compile(r"[^A-Za-z0-9._-]")


def slug(value: str) -> str:
    return _SLUG_RE.sub("_", value)


# Curated showcase set (small enough for a Pages commit / fast local demo).
# Names are validated against the real catalogs at bake time; entries that do
# not exist are skipped with a warning rather than failing the bake.
CURATED = {
    "weapons": [
        "Weapon_Armas_Magnum",
        "Weapon_AssaultRifle",
        "Weapon_AssaultRifle_ATac",
        "Weapon_AssaultRifle_COBR-A",
        "Weapon_Armas_SubMachineGun_Pitbull",
        "Weapon_Armas_SniperRifle_50Cal",
    ],
    "vehicles": [
        "Baked_A_2DrCoupe",
        "Baked_A_ClassicMuscle",
        "Baked_A_ExoticMuscle",
        "Baked_A_EstateVan",
    ],
    "clothing": [
        "F_Armpads_Armoured",
        "F_Armpads_Impact",
        "F_Armwear_Bracelet_Noir_PearlRight",
        "F_Armwear_Bracelet_Urban_WristbandRightCharlotte",
        "F_Neckwear_Necklace_Enforcement_Dogtag",
        "F_Backpack_DoubleAxes",
        "M_Neckwear_Necklace_Enforcement_Dogtag",
        "M_Armpads_Armoured",
    ],
    "characters": ["F_Body_Base", "M_Body_Base"],
    # Matched by animset *display* (the group folder under Retail/Animations).
    "animsets": ["Anim_Contact_Specific_Standing", "Anim_Contact_Specific_Standing_Fem"],
    "anim_clips": 3,
    # Prop packages (MaterialDatabase) matched by package-name substring.
    "prop_hints": ["BreakableDoor", "Mailbox", "FlagPole"],
    "district_count": 3,
}

DEFAULT_OUT = Path(__file__).resolve().parents[1] / "studio-static"


def _progress(message: str) -> None:
    print(f"[bake] {message}", flush=True)


def _write(path: Path, content: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)


def _record_size(manifest: dict, key: str, path: Path) -> None:
    manifest["sizes"][key] = manifest["sizes"].get(key, 0) + path.stat().st_size


def _bake_glb(out: Path, manifest: dict, key: str, url: str, **params) -> bool:
    """Call a backend.py endpoint function and write its GLB/JSON body."""
    try:
        response = getattr(backend, url)(**params)
    except Exception as exc:  # endpoint raises HTTPException on conversion failure
        _progress(f"  ! {key} failed: {exc}")
        return False
    if response.status_code >= 400:
        _progress(f"  ! {key} -> HTTP {response.status_code}")
        return False
    target = out / "data" / key
    _write(target, response.body)
    _record_size(manifest, "glb_bytes", target)
    manifest["assets"] += 1
    return True


def _bake_png(out: Path, manifest: dict, key: str, url: str, **params) -> bool:
    """Call a backend.py TGA->PNG endpoint and write its PNG body."""
    try:
        response = getattr(backend, url)(**params)
    except Exception as exc:
        _progress(f"  ! {key} failed: {exc}")
        return False
    if response.status_code >= 400:
        return False
    target = out / "data" / key
    _write(target, response.body)
    _record_size(manifest, "texture_bytes", target)
    manifest["assets"] += 1
    return True


# ---------------------------------------------------------------------------
# Catalogs -> baked JSON (paths rewritten to data/ URLs, unbaked entries cut)
# ---------------------------------------------------------------------------
def _rewrite_catalog(out: Path, name: str, payload: dict) -> Path:
    target = out / "data" / "catalog" / name
    _write(target, json.dumps(payload, indent=1).encode("utf-8"))
    return target


def _bake_weapons(out: Path, manifest: dict, all_items: bool) -> set[str]:
    catalog = build_weapon_catalog(backend.WEAPONS_BASE)
    wanted = {folder.casefold() for folder in (CURATED["weapons"] if not all_items else [w["folder"] for w in catalog])}
    baked_parts: set[str] = set()
    kept: list[dict] = []
    for weapon in catalog:
        if weapon["folder"].casefold() not in wanted:
            continue
        parts = []
        for part in weapon["parts"]:
            # skin must be explicit: the endpoint default is a Query() object
            # (unpacked by FastAPI only when serving HTTP), not None.
            if _bake_glb(out, manifest, f"glb/weapons/{slug(part['id'])}.glb", "mesh_glb", path=part["id"], skin=None):
                parts.append(part)
                baked_parts.add(part["id"])
        if not parts:
            continue
        kept.append({
            **{key: weapon[key] for key in ("id", "folder", "display", "name_confidence", "sapbdb")},
            "primary": weapon["primary"] if weapon["primary"] in baked_parts else parts[0]["id"],
            "parts": parts,
            # Static bake does not convert skin texture variants; drop the
            # dropdown so the UI never offers a skin that cannot render.
            "skins": [],
        })
    _rewrite_catalog(out, "weapons.json", {"count": len(kept), "weapons": kept})
    _progress(f"weapons: {len(kept)} designs, {len(baked_parts)} part GLBs")
    return baked_parts


def _bake_vehicles(out: Path, manifest: dict, all_items: bool) -> tuple[set[str], set[str], set[str]]:
    catalog = build_vehicle_catalog(backend.VEHICLES_BULK)
    wanted = {v["id"].casefold() for v in catalog} if all_items else {v.casefold() for v in CURATED["vehicles"]}
    parts_catalog = build_part_catalog(backend.VEHICLES_FULL)
    wheel_variants_by_base: dict[str, list[dict]] = {}
    for part in parts_catalog:
        if part["slot"] not in ("Wheel", "Wheels"):
            continue
        wheel_variants_by_base.setdefault(part["base"], []).extend(part["variants"])

    baked_primaries: set[str] = set()
    baked_parts: set[str] = set()
    baked_variants: set[str] = set()
    baked_sockets: set[str] = set()
    kept: list[dict] = []
    for vehicle in catalog:
        if vehicle["id"].casefold() not in wanted:
            continue
        parts = []
        for part in vehicle["parts"]:
            if _bake_glb(out, manifest, f"glb/vehicles/{slug(part['id'])}.glb", "vehicle_glb", path=part["id"]):
                parts.append(part)
                baked_parts.add(part["id"])
        if not parts or vehicle["primary"] not in baked_parts:
            continue
        baked_primaries.add(vehicle["primary"])
        # Wheel-spin animated twin.
        _bake_glb(out, manifest, f"glb/vehicles/{slug(vehicle['primary'])}.wheelspin.glb", "vehicle_animation_glb", path=vehicle["primary"])
        # Socket transforms for the wheel assembler (keyed by the Retail path
        # the frontend derives: first/SkeletalMesh3/EditorVehicle.psk).
        first = vehicle["primary"].split("/")[0]
        retail = f"{first}/SkeletalMesh3/EditorVehicle.psk"
        try:
            # vehicle_sockets returns a plain dict (no FastAPI Response wrapper).
            sockets = backend.vehicle_sockets(path=retail)
            socket_path = out / "data" / "sockets" / f"{slug(retail)}.json"
            _write(socket_path, json.dumps(sockets, indent=1).encode("utf-8"))
            baked_sockets.add(retail)
        except Exception as exc:
            _progress(f"  ! sockets {retail} failed: {exc}")

        # Wheel variants for this vehicle's wheel base.
        wheel_base = vehicle_wheel_base(vehicle["id"], parts_catalog) or vehicle["id"]
        kept_variants = []
        for variant in wheel_variants_by_base.get(wheel_base, []):
            if _bake_glb(out, manifest, f"glb/vehicle_parts/{slug(variant['mesh'])}.glb", "vehicle_part_glb", path=variant["mesh"]):
                kept_variants.append(variant)
                baked_variants.add(variant["mesh"])
        kept.append({
            "id": vehicle["id"],
            "display": vehicle["display"],
            "primary": vehicle["primary"],
            "parts": parts,
            "wheel_base": wheel_base,
            "wheel_variants": kept_variants,
        })
    _rewrite_catalog(out, "vehicles.json", {"count": len(kept), "vehicles": kept})

    # Rewrite the vehicle_parts catalog to only the baked wheel variants.
    kept_parts = []
    for part in parts_catalog:
        variants = [v for v in part["variants"] if v["mesh"] in baked_variants]
        if variants:
            kept_parts.append({**part, "variants": variants})
    _rewrite_catalog(out, "vehicle_parts.json", {"count": len(kept_parts), "parts": kept_parts})
    _progress(f"vehicles: {len(kept)} families, {len(baked_parts)} part GLBs, {len(baked_variants)} wheel variants, {len(baked_sockets)} socket sets")
    return baked_primaries, baked_parts, baked_variants


def _bake_clothing(out: Path, manifest: dict, all_items: bool) -> tuple[set[str], dict[str, dict]]:
    catalog = build_clothing_catalog(backend.CHARACTERS_BULK)
    wanted = {item["name"] for item in catalog} if all_items else set(CURATED["clothing"])

    # Shared skin atlases used by the body-overlay path of the compositor.
    skins: dict[str, str] = {}
    for prefix in ("F", "M"):
        skin_dir = backend.CHARACTERS_BULK / f"{prefix}_Body_Skin" / f"{prefix}_Body_Skin" / "Texture2D"
        diff = next((
            candidate for candidate in (
                skin_dir / f"{prefix}_Skin_Colour_Caucasian_Diff.tga",
                skin_dir / f"{prefix}_Skin_Colour_Pale_Diff.tga",
            ) if candidate.is_file()
        ), None)
        if diff is None:
            continue
        rel = diff.relative_to(backend.EXTRACTED_ROOT).as_posix()
        if _bake_png(out, manifest, f"textures/clothing/__skins/{prefix}_skin.png", "texture_png", path=rel):
            skins[prefix] = f"data/textures/clothing/__skins/{prefix}_skin.png"

    baked_items: set[str] = set()
    colmask_meta: dict[str, dict] = {}
    kept: list[dict] = []
    for item in catalog:
        if item["name"] not in wanted:
            continue
        name = item["name"]
        try:
            _name, item_id, _item_root, _tex_dir, _mesh, is_body_item, tex_base = backend._resolve_clothing_preview(name)
        except Exception as exc:
            _progress(f"  ! clothing {name}: {exc}")
            continue
        if not _bake_glb(out, manifest, f"glb/clothing/{slug(name)}.glb", "clothing_mesh_glb", item=name):
            continue
        baked_items.add(item_id)

        # Diffuse + ColMask region PNGs for the client-side compositor.
        prefix = "M" if name.startswith("M_") else "F"
        diffuse = next((
            candidate for candidate in (
                _tex_dir / f"{tex_base}_Main_Diff.tga",
                _tex_dir / f"{tex_base}_Diff.tga",
                _tex_dir / f"{tex_base}_Xtra_Diff.tga",
            ) if candidate.is_file()
        ), None)
        if diffuse is not None:
            rel = diffuse.relative_to(backend.EXTRACTED_ROOT).as_posix()
            _bake_png(out, manifest, f"textures/clothing/{slug(name)}/diffuse.png", "texture_png", path=rel)

        regions: dict[str, str] = {}
        region_masks = find_colmask_for_item(item_id) or {}
        for region, mask_path in sorted(region_masks.items()):
            if _bake_png(out, manifest, f"textures/clothing/{slug(name)}/colmask/{slug(region)}.png", "colmask_texture", path=str(mask_path)):
                regions[region] = f"data/textures/clothing/{slug(name)}/colmask/{slug(region)}.png"

        colmask_meta[name] = {
            "item": item_id,
            "body": is_body_item,
            "skin": skins.get(prefix) if is_body_item else None,
            "regions": regions,
        }
        _write(out / "data" / "colmask" / f"{slug(name)}.json", json.dumps(colmask_meta[name], indent=1).encode("utf-8"))

        entry = {
            "id": item_id,
            "name": name,
            "region_count": len(regions),
            "regions": sorted(regions.keys()),
        }
        apbdb_id = backend.get_apbdb_id(name)
        if apbdb_id:
            entry["apbdb_id"] = apbdb_id
            entry["apbdb_url"] = backend.get_apbdb_url(name)
        kept.append(entry)
    _rewrite_catalog(out, "clothing.json", {"count": len(kept), "items": kept})
    _progress(f"clothing: {len(kept)} items baked")
    return baked_items, colmask_meta


def _bake_symbols(out: Path, manifest: dict) -> None:
    catalog = build_symbol_catalog(backend.SYMBOLS_BULK)
    total = 0
    for category in catalog:
        for symbol in category["symbols"]:
            target = f"textures/symbols/{slug(symbol['id'])}.png"
            if _bake_png(out, manifest, target, "symbol_texture", path=symbol["path"]):
                symbol["path"] = f"data/{target}"
                total += 1
    kept = [category for category in catalog if category["symbols"]]
    _rewrite_catalog(out, "symbols.json", {"total": total, "categories": kept})
    _progress(f"symbols: {total} decals baked")


def _bake_animations(out: Path, manifest: dict, all_items: bool, anim_animsets: int, anim_clips: int) -> None:
    catalog = build_animation_catalog(backend.ANIMATIONS_ROOT)
    if all_items:
        # --all x every animset x every body mesh is combinatorial (tens of
        # thousands of GLBs, many GB). Default to a bounded sample unless the
        # operator explicitly overrides the limits.
        selected = catalog[:anim_animsets]
        clip_limit = anim_clips
    else:
        wanted = {display.casefold() for display in CURATED["animsets"]}
        selected = [entry for entry in catalog if entry["display"].casefold() in wanted]
        clip_limit = anim_clips

    body_catalog = build_character_catalog(backend.CHARACTERS_BULK)
    body_names = {item["name"] for item in body_catalog if item["category"] == "body"}
    meshes = [item for item in body_catalog if item["category"] == "body" and item["name"] in body_names]
    if not all_items:
        meshes = [item for item in meshes if item["name"] in set(CURATED["characters"])]

    baked_mesh: set[str] = set()
    kept: list[dict] = []
    for entry in selected:
        clips = entry["clips"][:clip_limit]
        per_set = []
        for clip in clips:
            for mesh in meshes:
                key = f"glb/animations/{slug(mesh['relpath'])}__{slug(entry['relpath'])}__{slug(clip['name'])}.glb"
                if _bake_glb(out, manifest, key, "animation_glb", mesh=mesh["relpath"], animset=entry["relpath"], clip=clip["name"]):
                    baked_mesh.add(mesh["relpath"])
            per_set.append(clip)
        if per_set:
            kept.append({**entry, "clips": per_set})

    _rewrite_catalog(out, "animations.json", {"count": len(kept), "animsets": kept})
    _progress(f"animations: {len(kept)} animsets x {len(meshes)} meshes, {len(baked_mesh)} mesh combos baked")

    char_kept = [item for item in body_catalog if item["category"] == "body" and item["relpath"] in baked_mesh]
    _rewrite_catalog(out, "characters.json", {"count": len(char_kept), "characters": char_kept})


def _bake_props(out: Path, manifest: dict, all_items: bool) -> set[str]:
    md_root = backend.EXTRACTED_ROOT / "MaterialDatabase"
    index = _prop_anim_index(md_root)
    if all_items:
        selected = list(index.values())
    else:
        hints = [hint.casefold() for hint in CURATED["prop_hints"]]
        selected = [
            rel for name, rel in sorted(index.items())
            if any(hint in name for hint in hints)
        ]
        if not selected:
            # Fallback: first 3 packages so a bake never silently loses props.
            selected = list(index.values())[:3]
    baked: set[str] = set()
    for rel in selected:
        if _bake_glb(out, manifest, f"glb/props/{slug(rel)}.glb", "prop_animation_glb", path=rel):
            baked.add(rel)
    _progress(f"props: {len(baked)} animated props baked")
    return baked


def _bake_districts(out: Path, manifest: dict, all_items: bool) -> set[str]:
    districts_root = backend.EXTRACTED_ROOT / "Retail" / "Districts"
    records = _district_mesh_records(districts_root)
    if all_items:
        selected = records
    else:
        selected = records[:CURATED["district_count"]]
    baked: set[str] = set()
    for record in selected:
        rel = record["preview_path"]
        if _bake_glb(out, manifest, f"glb/districts/{slug(rel)}.glb", "static_mesh_glb", path=rel):
            baked.add(rel)
    _progress(f"districts: {len(baked)} static meshes baked")
    return baked


def _rewrite_inventory_previews(inventory: dict, baked: dict[str, set[str]]) -> None:
    """Drop previews the bake did not produce (in place).

    Asset rows keep all metadata; only preview_kind/preview_path are reset so
    the static UI shows "Preview unavailable" instead of a broken request.
    """
    for asset in inventory["assets"]:
        kind = asset.get("preview_kind")
        path = asset.get("preview_path")
        baked_kind = {
            "weapon_mesh": "weapon_parts",
            "vehicle_mesh": "vehicle_primaries",
            "character_mesh": "clothing",
            "static_mesh": "districts",
            "prop_animation": "props",
        }.get(kind)
        if baked_kind is not None and path in baked[baked_kind]:
            continue
        if kind in ("texture", "video") or baked_kind is not None:
            asset["preview_kind"] = "none"
            asset["preview_path"] = None


def _bake_inventory(out: Path, baked: dict[str, set[str]]) -> None:
    inventory = get_cached_asset_inventory(backend.REPO_ROOT)
    _rewrite_inventory_previews(inventory, baked)
    payload = {
        "total": inventory["total"],
        "count": inventory["total"],
        "offset": 0,
        "limit": inventory["total"],
        "has_more": False,
        "categories": inventory["categories"],
        "statuses": inventory["statuses"],
        "source_builds": inventory["source_builds"],
        "assets": inventory["assets"],
    }
    _rewrite_catalog(out, "inventory.json", payload)
    _progress(f"inventory: {inventory['total']} rows, previews rewritten")


def _copy_web_dist(out: Path, web_dist: Path | None) -> None:
    if web_dist is None or not web_dist.is_dir():
        _progress("no --web-dist given; skipping frontend copy")
        return
    for source in web_dist.iterdir():
        destination = out / source.name
        if source.is_dir():
            if destination.is_dir():
                shutil.rmtree(destination)
            shutil.copytree(source, destination)
        else:
            shutil.copy2(source, destination)
    _progress(f"frontend copied from {web_dist}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--web-dist", type=Path, default=None, help="prebuilt web/dist (vite build --mode static) to copy into the bundle")
    parser.add_argument("--all", action="store_true", help="bake every weapon/vehicle/clothing/animation (very large)")
    parser.add_argument("--anim-animsets", type=int, default=8, help="animsets baked in --all mode (default 8; combinatorial with meshes x clips)")
    parser.add_argument("--anim-clips", type=int, default=2, help="clips per animset (default 2 in --all, 3 in curated)")
    args = parser.parse_args()

    out = args.out.resolve()
    if out.is_dir():
        shutil.rmtree(out)
    out.mkdir(parents=True)

    start = time.time()
    if args.all:
        _progress(f"--all mode: animations limited to {args.anim_animsets} animsets x {args.anim_clips} clips; expect many GB")
    manifest = {"mode": "all" if args.all else "curated", "generated": time.strftime("%Y-%m-%dT%H:%M:%S"), "assets": 0, "sizes": {}, "failures": []}

    baked: dict[str, set[str]] = {
        "weapon_parts": set(),
        "vehicle_primaries": set(),
        "vehicle_parts": set(),
        "clothing": set(),
        "districts": set(),
        "props": set(),
    }
    baked["weapon_parts"] = _bake_weapons(out, manifest, args.all)
    baked["vehicle_primaries"], _vehicle_parts, baked["vehicle_parts"] = _bake_vehicles(out, manifest, args.all)
    baked["clothing"], _colmask = _bake_clothing(out, manifest, args.all)
    _bake_symbols(out, manifest)
    _bake_animations(out, manifest, args.all, args.anim_animsets, args.anim_clips if args.all else CURATED["anim_clips"])
    baked["props"] = _bake_props(out, manifest, args.all)
    baked["districts"] = _bake_districts(out, manifest, args.all)
    _bake_inventory(out, baked)

    _copy_web_dist(out, args.web_dist)

    total_bytes = sum(manifest["sizes"].values())
    manifest["total_bytes"] = total_bytes
    _write(out / "data" / "manifest.json", json.dumps(manifest, indent=1).encode("utf-8"))

    _progress(f"done: {manifest['assets']} assets, {total_bytes / (1024 * 1024):.1f} MB in {time.time() - start:.1f}s -> {out}")
    _progress("deploy: copy this directory to docs/studio/ and push, or serve it from any static host")
    return 0


if __name__ == "__main__":
    sys.exit(main())
