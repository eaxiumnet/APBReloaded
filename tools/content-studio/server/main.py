"""APB Content Studio backend — FastAPI.

Slice 1 (viewer): serve the weapon catalog and convert extracted ActorX meshes
to glTF on demand for the three.js frontend.

Slice 2 (ColMask editor): serve ColMask region textures for character clothing items.

Run (from tools/content-studio/server):
    uvicorn main:app --port 8777 --reload
"""

from __future__ import annotations

import hashlib
import json
import math
import os
import tempfile
from pathlib import Path

from fastapi import FastAPI, HTTPException, Query
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, Response
from pydantic import BaseModel

from assets import build_weapon_catalog, resolve_mesh_path
from characters import build_character_catalog
from colmask import build_clothing_catalog, find_colmask_for_item
from compositor import (
    PROJECTED_LAYER_MATERIAL,
    add_projected_body_layer,
    composite_body_overlay,
    composite_clothing,
    composite_skin,
)
from gltf_export import mesh_to_glb, skinned_mesh_to_glb
from animations import (
    build_animation_catalog,
    resolve_animset,
    align_clips_to_skeleton,
    load_animset_cached,
    rebase_clips_to_skeleton,
)
from psa import parse_psa_file
from item_mapping import get_apbdb_id, get_apbdb_url
from inventory import find_prop_mesh_dir, get_cached_asset_inventory, pick_prop_mesh
from psk import (
    PskBone,
    bone_world_transforms,
    parse_psk_file,
    parse_skeleton,
    parse_weights,
    reconstruct_bind_skeleton,
    skin_weights_by_point,
)
from symbols import build_symbol_catalog, find_symbol_path
from texture_resolver import find_default_textures
from vehicles import (
    build_part_catalog,
    build_vehicle_catalog,
    build_wheelspin_clip,
    find_vehicle_textures,
    resolve_vehicle_mesh,
    vehicle_materials,
    vehicle_socket_transforms,
    vehicle_wheel_base,
)

# repo root = .../APBReloaded ; this file = .../tools/content-studio/server/main.py
REPO_ROOT = Path(__file__).resolve().parents[3]
WEAPONS_BASE = REPO_ROOT / "Content" / "Extracted" / "WeaponsBase"
CHARACTERS_BULK = REPO_ROOT / "Content" / "Extracted" / "CharactersBulk"
SYMBOLS_BULK = REPO_ROOT / "Content" / "Extracted" / "SymbolsBulk"
VEHICLES_BULK = REPO_ROOT / "Content" / "Extracted" / "VehiclesBulk"
VEHICLES_FULL = REPO_ROOT / "Content" / "Extracted" / "Retail" / "Vehicles"


def _clothing_paths(item_value: str) -> tuple[str, str, Path, Path]:
    parts = Path(item_value).parts
    if not parts or any(part in {"", ".", ".."} for part in parts) or len(parts) > 2:
        raise ValueError(f"invalid clothing item: {item_value}")
    name = parts[-1]
    item_id = f"{name}/{name}" if len(parts) == 1 else "/".join(parts)
    item_root = CHARACTERS_BULK.joinpath(*item_id.split("/"))
    texture_dir = item_root / "Texture2D"
    return name, item_id, item_root, texture_dir


def _resolve_clothing_preview(item_value: str) -> tuple[str, str, Path, Path, Path, bool, str]:
    name, item_id, item_root, texture_dir = _clothing_paths(item_value)
    item_mesh_dir = item_root / "SkeletalMesh3"
    item_mesh_path = next(iter(sorted(item_mesh_dir.glob("*_Xtra.psk"))), None)
    if item_mesh_path is not None:
        return name, item_id, item_root, texture_dir, item_mesh_path, False, item_mesh_path.stem

    prefix = "M" if name.startswith("M_") else "F"
    body_name = f"{prefix}_Body_Base"
    body_rel = Path(body_name) / body_name / "SkeletalMesh3" / f"{body_name}.psk"
    item_body_path = item_root / body_rel
    canonical_body_path = CHARACTERS_BULK / body_rel
    mesh_path = item_body_path if item_body_path.is_file() else canonical_body_path
    return name, item_id, item_root, texture_dir, mesh_path, True, name


def _cache_png(key: str, data: bytes) -> Path:
    temp_dir = Path(tempfile.gettempdir()) / "apb_content_studio"
    temp_dir.mkdir(exist_ok=True)
    safe_key = hashlib.sha1(key.encode("utf-8"), usedforsecurity=False).hexdigest()
    path = temp_dir / f"{safe_key}.png"
    path.write_bytes(data)
    return path

app = FastAPI(title="APB Content Studio", version="0.1.0")

# Vite dev server runs on a different port -> allow localhost origins in dev.
app.add_middleware(
    CORSMiddleware,
    allow_origins=[
        "http://localhost:5173",
        "http://127.0.0.1:5173",
    ],
    allow_methods=["GET", "POST", "OPTIONS"],
    allow_headers=["*"],
)


@app.get("/api/health")
def health() -> dict:
    return {
        "ok": True,
        "weapons_base": str(WEAPONS_BASE),
        "weapons_base_exists": WEAPONS_BASE.is_dir(),
    }


@app.get("/api/inventory/assets")
def inventory_assets(
    category: str | None = Query(None),
    status: str | None = Query(None),
    source_build: str | None = Query(None),
    query: str | None = Query(None),
    offset: int = Query(0, ge=0),
    limit: int = Query(250, ge=1, le=1000),
) -> dict:
    inventory = get_cached_asset_inventory(REPO_ROOT)
    assets = inventory["assets"]
    needle = (query or "").strip().casefold()
    if category:
        assets = [asset for asset in assets if asset["category"] == category]
    if status:
        assets = [asset for asset in assets if asset["status"] == status]
    if source_build:
        assets = [asset for asset in assets if asset["source_build"] == source_build]
    if needle:
        assets = [
            asset for asset in assets
            if needle in " ".join(str(asset.get(key) or "") for key in (
                "name", "category", "status", "source_build", "source_locator",
                "source_package", "destination", "asset_class",
            )).casefold()
        ]
    return {
        "total": inventory["total"],
        "count": len(assets),
        "offset": offset,
        "limit": limit,
        "has_more": offset + limit < len(assets),
        "categories": inventory["categories"],
        "statuses": inventory["statuses"],
        "source_builds": inventory["source_builds"],
        "assets": assets[offset:offset + limit],
    }


@app.get("/api/catalog/weapons")
def catalog_weapons() -> dict:
    weapons = build_weapon_catalog(WEAPONS_BASE)
    return {"count": len(weapons), "weapons": weapons}


@app.get("/api/catalog/vehicles")
def catalog_vehicles() -> dict:
    vehicles = build_vehicle_catalog(VEHICLES_BULK)
    parts = build_part_catalog(VEHICLES_FULL)
    for vehicle in vehicles:
        wheel_base = vehicle_wheel_base(vehicle["id"], parts)
        if wheel_base:
            vehicle["wheel_base"] = wheel_base
    return {"count": len(vehicles), "vehicles": vehicles}


@app.get("/api/catalog/vehicle_parts")
def catalog_vehicle_parts() -> dict:
    parts = build_part_catalog(VEHICLES_FULL)
    return {"count": len(parts), "parts": parts}


@app.get("/api/vehicle.sockets")
def vehicle_sockets(path: str = Query(..., description="catalog mesh id under Retail/Vehicles")):
    try:
        abs_path = resolve_vehicle_mesh(VEHICLES_FULL, path)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    except FileNotFoundError as exc:
        raise HTTPException(status_code=404, detail=f"vehicle mesh not found: {path}") from exc
    sockets = vehicle_socket_transforms(abs_path)
    if not sockets:
        raise HTTPException(status_code=422, detail=f"vehicle has no socket data: {path}")
    return {"sockets": sockets}


@app.get("/api/vehicle.glb")
def vehicle_glb(path: str = Query(..., description="catalog mesh id under VehiclesBulk")):
    try:
        abs_path = resolve_vehicle_mesh(VEHICLES_BULK, path)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    except FileNotFoundError as exc:
        raise HTTPException(status_code=404, detail=f"vehicle mesh not found: {path}") from exc

    try:
        mesh = parse_psk_file(abs_path)
        material_textures, material_settings = vehicle_materials(abs_path)
        glb = mesh_to_glb(
            mesh,
            textures=find_vehicle_textures(abs_path),
            material_textures=material_textures,
            material_settings=material_settings,
        )
    except Exception as exc:
        raise HTTPException(status_code=422, detail=f"convert failed: {exc}") from exc
    return Response(
        content=glb,
        media_type="model/gltf-binary",
        headers={"Content-Disposition": f'inline; filename="{Path(path).stem}.glb"'},
    )


@app.get("/api/vehicle_animation.glb")
def vehicle_animation_glb(path: str = Query(..., description="catalog mesh id under VehiclesBulk")):
    """Serve a skinned vehicle GLB with a synthesized wheel-spin clip.

    Retail has no chassis animsets (the Anim_LC_Vehicle_* sets are driver
    character rigs), so the animation pipeline generates the wheel-spin clip
    from the vehicle skeleton itself: every wheel bone rotates around its
    local axle, deforming the skinned tire geometry.
    """
    try:
        abs_path = resolve_vehicle_mesh(VEHICLES_BULK, path)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    except FileNotFoundError as exc:
        raise HTTPException(status_code=404, detail=f"vehicle mesh not found: {path}") from exc

    try:
        mesh = parse_psk_file(abs_path)
        skeleton = parse_skeleton(abs_path.read_bytes())
        influences = parse_weights(abs_path.read_bytes())
        weights = skin_weights_by_point(influences, len(mesh.points))
        clips = [build_wheelspin_clip(skeleton)]
        material_textures, material_settings = vehicle_materials(abs_path)
        glb = skinned_mesh_to_glb(
            mesh,
            skeleton=skeleton,
            weights=weights,
            clips=clips,
            textures=find_vehicle_textures(abs_path),
            material_textures=material_textures,
            material_settings=material_settings,
        )
    except Exception as exc:
        raise HTTPException(status_code=422, detail=f"convert failed: {exc}") from exc
    return Response(
        content=glb,
        media_type="model/gltf-binary",
        headers={"Content-Disposition": f'inline; filename="{Path(path).stem}_wheelspin.glb"'},
    )


@app.get("/api/vehicle_part.glb")
def vehicle_part_glb(path: str = Query(..., description="part mesh id under Retail/Vehicles")):
    try:
        abs_path = resolve_vehicle_mesh(VEHICLES_FULL, path)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    except FileNotFoundError as exc:
        raise HTTPException(status_code=404, detail=f"vehicle part not found: {path}") from exc

    try:
        mesh = parse_psk_file(abs_path)
        glb = mesh_to_glb(
            mesh,
            textures=find_vehicle_textures(abs_path),
        )
    except Exception as exc:
        raise HTTPException(status_code=422, detail=f"convert failed: {exc}") from exc
    return Response(
        content=glb,
        media_type="model/gltf-binary",
        headers={"Content-Disposition": f'inline; filename="{Path(path).stem}.glb"'},
    )


@app.get("/api/mesh.glb")
def mesh_glb(
    path: str = Query(..., description="catalog mesh id (relpath under WeaponsBase)"),
    skin: str | None = Query(None, description="optional skin id (relpath under the weapon design folder)"),
):
    try:
        abs_path = resolve_mesh_path(WEAPONS_BASE, path)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    except FileNotFoundError as exc:
        raise HTTPException(status_code=404, detail=f"mesh not found: {path}") from exc

    try:
        textures = find_default_textures(abs_path, skin=skin)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    except FileNotFoundError as exc:
        raise HTTPException(status_code=404, detail=f"skin not found: {skin}") from exc

    try:
        if skin and "baseColor" in textures and "skin_colors" in textures:
            composited_png, _ = composite_skin(
                base_diffuse=textures["baseColor"],
                mask1=textures.get("mask1"),
                mask2=textures.get("mask2"),
                colors=textures["skin_colors"],
                stencil=textures.get("skin_stencil"),
                scalars=textures.get("skin_scalars"),
            )
            import tempfile
            temp_dir = Path(tempfile.gettempdir()) / "apb_content_studio"
            temp_dir.mkdir(exist_ok=True)
            safe_name = f"composited_{Path(path).stem}_{skin.replace('/', '_')}"
            temp_path = temp_dir / f"{safe_name}.png"
            with open(temp_path, "wb") as file:
                file.write(composited_png)
            textures["baseColor"] = temp_path

        mesh = parse_psk_file(abs_path)
        glb = mesh_to_glb(mesh, textures=textures)
    except Exception as exc:  # malformed/unsupported psk -> 422, not a 500 crash
        raise HTTPException(status_code=422, detail=f"convert failed: {exc}") from exc

    return Response(
        content=glb,
        media_type="model/gltf-binary",
        headers={"Content-Disposition": f'inline; filename="{Path(path).stem}.glb"'},
    )


ANIMATIONS_ROOT = REPO_ROOT / "Content" / "Extracted" / "Retail" / "Animations"


def _resolve_character_mesh(relpath: str) -> Path:
    """Resolve a body/clothing PSK relpath under CharactersBulk (traversal-safe)."""
    base = CHARACTERS_BULK.resolve()
    target = (base / relpath).resolve()
    if target != base and not str(target).startswith(str(base) + os.sep):
        raise ValueError("path escapes CharactersBulk")
    if target.suffix.casefold() not in {".psk", ".pskx"}:
        raise ValueError(f"not a mesh file: {relpath}")
    if not target.is_file():
        raise FileNotFoundError(relpath)
    return target


@app.get("/api/catalog/animations")
def catalog_animations() -> dict:
    """List every character animset with clip metadata."""
    animsets = build_animation_catalog(ANIMATIONS_ROOT)
    return {"count": len(animsets), "animsets": animsets}


@app.get("/api/catalog/characters")
def catalog_characters() -> dict:
    """List every body/clothing/crowd character mesh under CharactersBulk."""
    characters = build_character_catalog(CHARACTERS_BULK)
    return {"count": len(characters), "characters": characters}


def _animation_materials(
    name: str,
    mesh,
    tex_dir: Path,
    is_body_item: bool,
    tex_base: str,
    requested_mesh: str,
) -> tuple[dict[str, Path] | None, dict | None, dict | None]:
    """Resolve diffuse/specular/opacity textures for a skinned preview item.

    Body items get the shared skin atlas; clothing items get their own
    diffuse. Crowd/dev whole characters without a clothing mesh get no
    textures (grey preview) rather than the wrong body skin.
    """
    if is_body_item and "Body" not in Path(requested_mesh).name:
        return None, None, None
    textures: dict[str, Path] = {}
    if is_body_item:
        prefix = "M" if name.startswith("M_") else "F"
        skin_dir = CHARACTERS_BULK / f"{prefix}_Body_Skin" / f"{prefix}_Body_Skin" / "Texture2D"
        diff = next((
            candidate for candidate in (
                skin_dir / f"{prefix}_Skin_Colour_Caucasian_Diff.tga",
                skin_dir / f"{prefix}_Skin_Colour_Pale_Diff.tga",
            ) if candidate.is_file()
        ), None)
        if diff is not None:
            textures["baseColor"] = diff
    else:
        for key, stems in (
            ("baseColor", (f"{tex_base}_Main_Diff.tga", f"{tex_base}_Diff.tga",
                            f"{tex_base}_Xtra_Diff.tga", f"{name}_Main_Diff.tga")),
            ("normal", (f"{tex_base}_Main_Norm.tga", f"{tex_base}_Norm.tga",
                         f"{tex_base}_Xtra_Norm.tga")),
            ("specular", (f"{name}_Golem_BRDFMask.tga", f"{tex_base}_Golem_BRDFMask.tga")),
            ("opacity", (f"{tex_base}_Main_Opac.tga", f"{tex_base}_Opac.tga",
                          f"{tex_base}_Xtra_Opac.tga")),
        ):
            path = next((tex_dir / stem for stem in stems if (tex_dir / stem).is_file()), None)
            if path is not None:
                textures[key] = path
    if not textures:
        return None, None, None
    materials = mesh.materials or ["material_0"]
    material_textures = {material: dict(textures) for material in materials}
    material_settings: dict[str, dict] = {}
    for material in materials:
        settings = {"metallic_factor": 0.0, "roughness_factor": 0.9}
        if "hair" in material.casefold():
            settings["alpha_mode"] = "MASK"
            settings["alpha_cutoff"] = 0.333
            settings["base_color_factor"] = [0.02, 0.02, 0.02, 1.0]
        material_settings[material] = settings
    return textures, material_textures, material_settings


@app.get("/api/animation.glb")
def animation_glb(
    mesh: str = Query(..., description="character mesh relpath under CharactersBulk (e.g. F_Body_Base/F_Body_Base/SkeletalMesh3/F_Body_Base.psk)"),
    animset: str = Query(..., description="animset PSA relpath under Retail/Animations"),
    clip: str | None = Query(None, description="single clip name; default = all clips"),
):
    """Serve a skinned + animated GLB: character mesh + selected animset clips."""
    try:
        mesh_path = _resolve_character_mesh(mesh)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    except FileNotFoundError as exc:
        raise HTTPException(status_code=404, detail=f"mesh not found: {mesh}") from exc

    try:
        animset_path = resolve_animset(ANIMATIONS_ROOT, animset)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    except FileNotFoundError as exc:
        raise HTTPException(status_code=404, detail=f"animset not found: {animset}") from exc

    try:
        mesh_obj = parse_psk_file(mesh_path)
        skeleton = parse_skeleton(mesh_path.read_bytes())
        influences = parse_weights(mesh_path.read_bytes())
        weights = skin_weights_by_point(influences, len(mesh_obj.points))
        # Character PSK REFSKELTs are the animation reference skeleton: their
        # bones sit 50-160cm away from the mesh geometry, so composing clip
        # keys through them distorts the mesh into stretched ribbons. Re-derive
        # a mesh-aligned bind skeleton from the weights (same names, parents
        # and order, so clip alignment and joint indices are unchanged).
        skeleton = reconstruct_bind_skeleton(mesh_obj.points, weights, skeleton)
        animset_obj = load_animset_cached(animset_path)
        clips = align_clips_to_skeleton(animset_obj, [bone.name for bone in skeleton])
        if clip:
            clips = [c for c in clips if c["name"] == clip]
        if not clips:
            raise HTTPException(status_code=404, detail=f"no clips matched: {clip}")
        # bound response size: locomotion animsets hold 100+ clips; a full set
        # can exceed 50MB of keyframes. The viewer fetches per-clip instead.
        clips = clips[:16]
        # Re-anchor the keys to the reconstructed bind so frame 0 rests on the
        # authored geometry and only the clip's relative motion plays.
        clips = rebase_clips_to_skeleton(clips, skeleton)
        # Best-effort textures: bodies skin-tone, clothing items their own
        # diffuse. Falls back to a grey untextured preview on any resolution
        # failure so animation playback never depends on texture lookup.
        textures = material_textures = material_settings = None
        try:
            item_name = Path(mesh).parts[0]
            _name, _item_id, _item_root, tex_dir, _m, is_body_item, tex_base = (
                _resolve_clothing_preview(item_name)
            )
            textures, material_textures, material_settings = _animation_materials(
                _name, mesh_obj, tex_dir, is_body_item, tex_base, mesh)
        except Exception:
            textures = material_textures = material_settings = None
        glb = skinned_mesh_to_glb(
            mesh_obj,
            skeleton=skeleton,
            weights=weights,
            clips=clips,
            textures=textures,
            material_textures=material_textures,
            material_settings=material_settings,
        )
    except HTTPException:
        raise
    except Exception as exc:
        raise HTTPException(status_code=422, detail=f"convert failed: {exc}") from exc

    return Response(
        content=glb,
        media_type="model/gltf-binary",
        headers={"Content-Disposition": f'inline; filename="{Path(mesh).stem}_{Path(animset).stem}.glb"'},
    )


@app.get("/api/colmask")
def colmask_regions(item: str = Query(..., description="clothing item path (e.g., 'F_Armpads_Armoured/F_Armpads_Armoured')")):
    """List ColMask regions for a character clothing item."""
    regions = find_colmask_for_item(item)
    if regions is None:
        raise HTTPException(status_code=404, detail=f"item not found: {item}")
    return {
        "item": item,
        "regions": {region: str(path) for region, path in regions.items()},
    }


@app.get("/api/colmask/texture")
def colmask_texture(path: str = Query(..., description="absolute path to ColMask TGA file")):
    """Serve a ColMask TGA as PNG (browsers can't display TGA natively)."""
    tga_path = Path(path)
    if not tga_path.is_file():
        raise HTTPException(status_code=404, detail=f"texture not found: {path}")
    if tga_path.suffix.lower() != ".tga":
        raise HTTPException(status_code=400, detail="not a TGA file")

    # Convert TGA to PNG using Pillow
    try:
        from PIL import Image
        img = Image.open(tga_path)
        import io
        buf = io.BytesIO()
        img.save(buf, format="PNG")
        buf.seek(0)
        return Response(content=buf.read(), media_type="image/png")
    except ImportError:
        raise HTTPException(status_code=500, detail="Pillow not installed (required for TGA->PNG)")
    except Exception as exc:
        raise HTTPException(status_code=500, detail=f"convert failed: {exc}")


@app.get("/api/catalog/clothing")
def catalog_clothing() -> dict:
    """List all clothing items with ColMask regions and APBDB mapping."""
    items = build_clothing_catalog(CHARACTERS_BULK)

    # Add APBDB mapping to each item
    for item in items:
        apbdb_id = get_apbdb_id(item["name"])
        if apbdb_id:
            item["apbdb_id"] = apbdb_id
            item["apbdb_url"] = get_apbdb_url(item["name"])

    return {"count": len(items), "items": items}


@app.get("/api/clothing/mesh.glb")
def clothing_mesh_glb(
    item: str = Query(..., description="clothing item name (e.g., 'F_Armpads_Armoured' or 'F_Neckwear_Necklace_Enforcement_Dogtag')"),
):
    """Serve the mesh for a clothing item as GLB.

    Handles two types of clothing items:
    1. Body items (e.g., F_Body_Base) - use the body mesh directly
    2. Accessory items (e.g., F_Neckwear_Necklace_Enforcement_Dogtag) - use the item's own mesh
    """
    try:
        name, _item_id, item_root, tex_dir, mesh_path, is_body_item, tex_base = _resolve_clothing_preview(item)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    if not mesh_path.is_file():
        raise HTTPException(status_code=404, detail=f"preview mesh not found for item: {name}")

    try:
        mesh = parse_psk_file(mesh_path)
        projected_material = add_projected_body_layer(mesh, name) if is_body_item else None

        # Build texture dict with all available material channels
        textures = {}

        # Base color (diffuse)
        diff_path = next((
            candidate for candidate in (
                tex_dir / f"{tex_base}_Main_Diff.tga",
                tex_dir / f"{tex_base}_Diff.tga",
                tex_dir / f"{tex_base}_Xtra_Diff.tga",
            ) if candidate.is_file()
        ), None)
        if diff_path is not None:
            textures["baseColor"] = diff_path

        # Normal map
        norm_path = next((
            candidate for candidate in (
                tex_dir / f"{tex_base}_Main_Norm.tga",
                tex_dir / f"{tex_base}_Norm.tga",
                tex_dir / f"{tex_base}_Xtra_Norm.tga",
            ) if candidate.is_file()
        ), None)
        if norm_path is not None:
            textures["normal"] = norm_path

        # Specular/BRDF mask
        spec_path = next((
            candidate for candidate in (
                tex_dir / f"{name}_Golem_BRDFMask.tga",
                tex_dir / f"{tex_base}_Golem_BRDFMask.tga",
            ) if candidate.is_file()
        ), None)
        if spec_path is not None:
            textures["specular"] = spec_path

        # Opacity mask
        opac_path = next((
            candidate for candidate in (
                tex_dir / f"{tex_base}_Main_Opac.tga",
                tex_dir / f"{tex_base}_Opac.tga",
                tex_dir / f"{tex_base}_Xtra_Opac.tga",
            ) if candidate.is_file()
        ), None)
        if opac_path is not None:
            textures["opacity"] = opac_path

        prefix = "M" if name.startswith("M_") else "F"
        skin_dir = CHARACTERS_BULK / f"{prefix}_Body_Skin" / f"{prefix}_Body_Skin" / "Texture2D"
        skin_diff = next((
            candidate for candidate in (
                skin_dir / f"{prefix}_Skin_Colour_Caucasian_Diff.tga",
                skin_dir / f"{prefix}_Skin_Colour_Pale_Diff.tga",
            ) if candidate.is_file()
        ), None)
        skin_brdf = next((
            candidate for candidate in (
                skin_dir / f"{prefix}_Skin_Colour_Caucasian_Brdf_SkinGreasy.tga",
                skin_dir / f"{prefix}_Skin_Colour_Pale_Brdf_SkinGreasy.tga",
            ) if candidate.is_file()
            ), None)

        if is_body_item:
            if projected_material is not None:
                textures = {}
                if skin_diff is not None:
                    textures["baseColor"] = skin_diff
                if skin_brdf is not None:
                    textures["specular"] = skin_brdf
            else:
                textures.pop("normal", None)
                textures.pop("specular", None)
                if skin_diff is not None and diff_path is not None:
                    textures["baseColor"] = _cache_png(
                        f"body:{name}:base",
                        composite_body_overlay(skin_diff, diff_path),
                    )
                elif skin_diff is not None:
                    textures["baseColor"] = skin_diff

        face_textures = {}
        if is_body_item and skin_diff is not None:
            face_textures["baseColor"] = skin_diff
        if is_body_item and skin_brdf is not None:
            face_textures["specular"] = skin_brdf

        material_textures = {}
        for material in mesh.materials:
            material_key = material.casefold()
            if "hair" in material_key:
                material_textures[material] = {}
            elif is_body_item and material_key != "material_0":
                material_textures[material] = dict(face_textures or textures)
            else:
                material_textures[material] = dict(textures)

        if projected_material is not None and diff_path is not None:
            material_textures[projected_material] = {
                "baseColor": diff_path,
                "opacity": diff_path,
            }
            if norm_path is not None:
                material_textures[projected_material]["normal"] = norm_path
            if spec_path is not None:
                material_textures[projected_material]["specular"] = spec_path

        hair_dir = item_root / f"{prefix}_Hair" / "Texture2D"
        hair_diff = next(iter(hair_dir.glob("*_Diff.tga")), None)
        hair_opac = next(iter(hair_dir.glob("*_Opac.tga")), None)
        for material in mesh.materials:
            if "hair" not in material.casefold() or (hair_diff is None and hair_opac is None):
                continue
            material_textures[material] = {}
            if hair_diff is not None:
                material_textures[material]["baseColor"] = hair_diff
            elif hair_opac is not None:
                material_textures[material]["baseColor"] = hair_opac
            if hair_opac is not None:
                material_textures[material]["opacity"] = hair_opac
        material_settings = {
            material: {
                "alpha_mode": "MASK",
                "alpha_cutoff": 0.333,
                "base_color_factor": [0.02, 0.02, 0.02, 1.0],
            }
            for material in mesh.materials
            if "hair" in material.casefold()
        }
        if projected_material is not None:
            material_settings[PROJECTED_LAYER_MATERIAL] = {
                "alpha_mode": "MASK",
                "alpha_cutoff": 0.05,
                "double_sided": True,
            }
        # Character clothing is diffuse fabric/skin, not painted metal: keep the
        # APB metallic defaults (0.8/0.3) out of the viewer, or the BRDF mask
        # wired as metallicRoughnessTexture renders the body near-black.
        for material in mesh.materials:
            if "hair" in material.casefold():
                continue
            settings = material_settings.setdefault(material, {})
            settings["metallic_factor"] = 0.0
            settings["roughness_factor"] = 0.9
        glb = mesh_to_glb(
            mesh,
            textures=textures if textures else None,
            material_textures=material_textures,
            material_settings=material_settings,
        )
    except Exception as exc:
        raise HTTPException(status_code=422, detail=f"convert failed: {exc}") from exc

    return Response(
        content=glb,
        media_type="model/gltf-binary",
        headers={"Content-Disposition": f'inline; filename="{item}_mesh.glb"'},
    )


@app.get("/api/symbols/list")
def symbols_list() -> dict:
    """List all symbol decal categories with their symbols."""
    categories = build_symbol_catalog(SYMBOLS_BULK)
    total_symbols = sum(cat["symbol_count"] for cat in categories)
    return {"total": total_symbols, "categories": categories}


@app.get("/api/symbol/texture")
def symbol_texture(path: str = Query(..., description="absolute path to symbol TGA file")):
    """Serve a symbol TGA as PNG (browsers can't display TGA natively)."""
    tga_path = Path(path)
    if not tga_path.is_file():
        raise HTTPException(status_code=404, detail=f"symbol not found: {path}")
    if tga_path.suffix.lower() != ".tga":
        raise HTTPException(status_code=400, detail="not a TGA file")

    # Convert TGA to PNG using Pillow
    try:
        from PIL import Image
        img = Image.open(tga_path)
        import io
        buf = io.BytesIO()
        img.save(buf, format="PNG")
        buf.seek(0)
        return Response(content=buf.read(), media_type="image/png")
    except ImportError:
        raise HTTPException(status_code=500, detail="Pillow not installed (required for TGA->PNG)")
    except Exception as exc:
        raise HTTPException(status_code=500, detail=f"convert failed: {exc}")


EXTRACTED_ROOT = REPO_ROOT / "Content" / "Extracted"
DISTRICTS_ROOT = EXTRACTED_ROOT / "Retail" / "Districts"
_MEDIA_MIME = {
    ".webm": "video/webm",
    ".mp4": "video/mp4",
    ".mkv": "video/x-matroska",
    ".mov": "video/quicktime",
}


def _resolve_extracted(path: str) -> Path:
    """Resolve a path under Content/Extracted, rejecting traversal escapes."""
    base = EXTRACTED_ROOT.resolve()
    target = Path(path)
    if not target.is_absolute():
        target = base / target
    target = target.resolve()
    if target != base and not str(target).startswith(str(base) + os.sep):
        raise ValueError("path escapes Content/Extracted")
    if not target.is_file():
        raise FileNotFoundError(path)
    return target


def _district_textures(package_dir: Path, mesh_stem: str) -> dict[str, Path] | None:
    """Best-effort diffuse for a district mesh from its package Texture2D dir."""
    tex_dir = package_dir / "Texture2D"
    if not tex_dir.is_dir():
        return None
    base = mesh_stem.rsplit("_LOD_", 1)[0]
    for pattern in (f"{base}*_DiffSpec.tga", f"{base}*_Diff.tga", f"{base}*.tga"):
        matches = sorted(tex_dir.glob(pattern))
        if matches:
            return {"baseColor": matches[0]}
    return None


def _resolve_district_mesh(path: str) -> Path:
    """Resolve a district StaticMesh3 relpath under Retail/Districts."""
    base = DISTRICTS_ROOT.resolve()
    target = (base / path).resolve()
    if target != base and not str(target).startswith(str(base) + os.sep):
        raise ValueError("path escapes Retail/Districts")
    if target.suffix.casefold() not in {".psk", ".pskx"}:
        raise ValueError(f"not a mesh file: {path}")
    if not target.is_file():
        raise FileNotFoundError(path)
    return target


def _resolve_material_database(path: str) -> Path:
    """Resolve a relpath under Content/Extracted/MaterialDatabase."""
    base = (EXTRACTED_ROOT / "MaterialDatabase").resolve()
    target = (base / path).resolve()
    if target != base and not str(target).startswith(str(base) + os.sep):
        raise ValueError("path escapes MaterialDatabase")
    if not target.is_file():
        raise FileNotFoundError(path)
    return target


@app.get("/api/static_mesh.glb")
def static_mesh_glb(path: str = Query(..., description="StaticMesh3 relpath under Retail/Districts")):
    """Serve an extracted district/static mesh as GLB."""
    try:
        abs_path = _resolve_district_mesh(path)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    except FileNotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
    try:
        mesh = parse_psk_file(abs_path)
        textures = _district_textures(abs_path.parent.parent, abs_path.stem)
        glb = mesh_to_glb(mesh, textures=textures)
    except Exception as exc:
        raise HTTPException(status_code=422, detail=f"convert failed: {exc}") from exc
    return Response(
        content=glb,
        media_type="model/gltf-binary",
        headers={"Content-Disposition": f'inline; filename="{Path(path).stem}.glb"'},
    )


_MIN_SWING_DEG = 25.0
_FLAT_RATIO = 0.15
_AXIS_ALIGN = 0.7


def _qconj(q):
    return (-q[0], -q[1], -q[2], q[3])


def _qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by + ay * bw + az * bx - ax * bz,
        aw * bz + az * bw + ax * by - ay * bx,
        aw * bw - ax * bx - ay * by - az * bz,
    )


def _swing_axis(q0, q1):
    """Angle (deg) and local axis of the rotation from q0 to q1."""
    dq = _qmul(q1, _qconj(q0))
    s = math.sqrt(dq[0] ** 2 + dq[1] ** 2 + dq[2] ** 2)
    if s < 1e-9:
        return 0.0, (0.0, 0.0, 1.0)
    return 2.0 * math.atan2(s, dq[3]) * 180.0 / math.pi, (dq[0] / s, dq[1] / s, dq[2] / s)


def _quat_axis(q):
    """Unit rotation axis of a quaternion (normalized xyz part)."""
    s = math.sqrt(q[0] ** 2 + q[1] ** 2 + q[2] ** 2)
    if s < 1e-9:
        return (0.0, 0.0, 1.0)
    return (q[0] / s, q[1] / s, q[2] / s)


def _cloud_pca(points):
    """Principal axes of a vertex cloud via power iteration (pure stdlib).

    Returns (centroid, (l1, e1), (l2, e2), (l3, e3)) with l1 >= l2 >= l3,
    or None for a degenerate (< 3 points) cloud.
    """
    n = len(points)
    if n < 3:
        return None
    centroid = [sum(p[i] for p in points) / n for i in range(3)]
    cov = [[0.0] * 3 for _ in range(3)]
    for p in points:
        d = [p[i] - centroid[i] for i in range(3)]
        for r in range(3):
            for col in range(3):
                cov[r][col] += d[r] * d[col]
    for r in range(3):
        for col in range(3):
            cov[r][col] /= n

    def power(matrix, seed):
        v = seed
        for _ in range(40):
            out = [sum(matrix[r][k] * v[k] for k in range(3)) for r in range(3)]
            norm = math.sqrt(sum(x * x for x in out))
            if norm < 1e-12:
                return 0.0, (0.0, 0.0, 1.0)
            v = [x / norm for x in out]
        lam = sum(v[r] * sum(matrix[r][k] * v[k] for k in range(3)) for r in range(3))
        return max(lam, 0.0), tuple(v)

    def deflate(matrix, e, lam):
        return [[matrix[r][col] - lam * e[r] * e[col] for col in range(3)] for r in range(3)]

    first = power(cov, (1.0, 0.0, 0.0))
    second = power(deflate(cov, first[1], first[0]), (0.0, 1.0, 0.0))
    third = power(deflate(deflate(cov, first[1], first[0]), second[1], second[0]), (0.0, 0.0, 1.0))
    # The seeded power iterations can converge to any eigenvector (a seed on
    # one axis stays there), so sort by eigenvalue to guarantee l1 >= l2 >= l3.
    axes = sorted([first, second, third], key=lambda entry: entry[0], reverse=True)
    l1, e1 = axes[0]
    l2, e2 = axes[1]
    l3, e3 = axes[2]
    return centroid, (l1, e1), (l2, e2), (l3, e3)


def _mechanical_swing_flattened(mesh, skeleton, weights, clips) -> bool:
    """True when a mechanical prop's REFSKELT orientation flattens its swing.

    A flat leaf (door, hatch) swings around an axis *in* its plane, but some
    exports author the REFSKELT rotated ~90 deg, which maps the clip's local
    swing axis onto the leaf's thin (normal) axis. The leaf then rotates
    within its own plane around an off-center pivot: the hinge edge flies
    (BreakableDoors: 215cm). Detection is geometric and clip-driven, per
    animated joint, requiring all of:
      - the joint's weighted cloud is flat (thin principal axis),
      - the clip swings it substantially (>25 deg),
      - the raw bind maps the swing axis onto that thin axis,
      - the swing axis runs off-center (pivot at the leaf's edge, not hub).
    The check is the actual motion the rebase will play: the clip's relative
    swing composed with the bind (qb * qrel). A sane bind leaves that axis
    on the geometry's own principal frame (a door leaf swings around its
    hinge, a wheel around its axle, a pan around the housing axis); the
    pathological door bind rotates it to a diagonal, aligned with none of the
    three principal axes, which flattens the leaf (hinge flew 215cm). Any
    flat leaf whose swing lands off its principal frame is flagged and the
    caller zeroes the orientation. Angled mounts, tilted signs, wheels and
    drums keep their raw bind: their swing stays axis-aligned.
    """
    if len(mesh.points) < 3 or not skeleton:
        return False
    clouds: dict[int, list[int]] = {}
    for point_index, entries in weights.items():
        if entries and 0 <= entries[0][0] < len(skeleton):
            clouds.setdefault(entries[0][0], []).append(point_index)
    world = bone_world_transforms(skeleton)
    for bone_index, point_ids in clouds.items():
        if len(point_ids) < 8:
            continue
        _, bind_quat = world[bone_index]
        pca = _cloud_pca([mesh.points[i] for i in point_ids])
        if pca is None:
            continue
        _, (l1, e1), (_, e2), (l3, e3) = pca
        if l1 <= 0.0 or math.sqrt(l3 / l1) > _FLAT_RATIO:
            continue
        for clip in clips:
            tracks = clip.get("tracks") or []
            if bone_index >= len(tracks) or not tracks[bone_index]:
                continue
            track = tracks[bone_index]
            if len(track) < 2:
                continue
            q0 = track[0].quat
            q0c = _qconj(q0)
            key_max = max(track, key=lambda k: _swing_axis(q0, k.quat)[0])
            angle, _ = _swing_axis(q0, key_max.quat)
            if angle < _MIN_SWING_DEG:
                continue
            # The motion the rebase will actually play: bind composed with the
            # clip's relative swing (qnew = qb * qrel). A bind that twists
            # this axis off the geometry's principal frame flattens the leaf.
            qrel = _qmul(q0c, key_max.quat)
            world_axis = _quat_axis(_qmul(bind_quat, qrel))
            aligned = max(
                abs(world_axis[0] * e[0] + world_axis[1] * e[1] + world_axis[2] * e[2])
                for e in (e1, e2, e3)
            )
            if aligned < _AXIS_ALIGN:
                return True
    return False


@app.get("/api/prop_animation.glb")
def prop_animation_glb(path: str = Query(..., description="AnimSet PSA relpath under MaterialDatabase")):
    """Serve an animated prop: the package's mesh skinned with its own AnimSet.

    Prop packages (breakable doors, mailboxes, flag poles) ship the PSA next
    to the SkeletalMesh3 PSK it drives; the sibling mesh is resolved here.
    """
    try:
        psa_path = _resolve_material_database(path)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    except FileNotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
    try:
        mesh_dir = find_prop_mesh_dir(psa_path)
        if mesh_dir is None:
            raise HTTPException(status_code=404, detail=f"no sibling SkeletalMesh3 for animset: {path}")
        mesh_path = pick_prop_mesh(mesh_dir, psa_path)
        if mesh_path is None:
            raise HTTPException(status_code=404, detail=f"no sibling mesh for animset: {path}")
        mesh = parse_psk_file(mesh_path)
        skeleton = parse_skeleton(mesh_path.read_bytes())
        influences = parse_weights(mesh_path.read_bytes())
        weights = skin_weights_by_point(influences, len(mesh.points))
        # Character-rig props (>=100 bones: crowns, corpses) carry the same
        # animation-reference REFSKELT as the character bodies, 50-160cm from
        # the geometry, so re-derive a mesh-aligned bind from the weights.
        # Mechanical props (hinges, flag poles, cameras) keep their authored
        # REFSKELT bind: their joints ARE the pivots. Bone count is the
        # discriminator because raw bone-to-vertex distance does not separate
        # the rig types (the previously-fine Mailbox/NewspaperBox/PayPhone
        # also sit 57-115cm from their geometry); reconstruction would move
        # hinge pivots to the mesh centroid and degrade their motion.
        animset = load_animset_cached(psa_path)
        clips = align_clips_to_skeleton(animset, [bone.name for bone in skeleton])[:16]
        if len(skeleton) >= 100:
            skeleton = reconstruct_bind_skeleton(mesh.points, weights, skeleton)
        elif _mechanical_swing_flattened(mesh, skeleton, weights, clips):
            # Mechanical props keep their authored joint pivots but zero the
            # orientation only when the swing is proven flattened (door-like
            # REFSKELT rotated ~90 deg mapping the clip swing onto the leaf's
            # normal, e.g. BreakableDoors). Angled mounts, tilted signs,
            # wheels and drums keep their raw bind: their swing axis is
            # meaningful and the detector leaves it alone.
            skeleton = [
                PskBone(bone.name, bone.parent, (0.0, 0.0, 0.0, 1.0), bone.position)
                for bone in skeleton
            ]
        # Prop PSA keys are authored against the animation reference rig, not
        # the mesh bind: re-anchor frame 0 to the bind so the mesh rests on
        # its authored geometry and only the clip's relative motion plays.
        # Root translation is pinned to the bind (same tradeoff as character
        # locomotion: a clip that legitimately translates its root would lose
        # that motion; none of the indexed props do).
        clips = rebase_clips_to_skeleton(clips, skeleton)
        glb = skinned_mesh_to_glb(mesh, skeleton=skeleton, weights=weights, clips=clips)
    except HTTPException:
        raise
    except Exception as exc:
        raise HTTPException(status_code=422, detail=f"convert failed: {exc}") from exc
    return Response(
        content=glb,
        media_type="model/gltf-binary",
        headers={"Content-Disposition": f'inline; filename="{Path(path).stem}_prop.glb"'},
    )


@app.get("/api/texture.png")
def texture_png(path: str = Query(..., description="TGA/PNG path under Content/Extracted")):
    """Serve any extracted texture as PNG (browsers can't render TGA natively)."""
    try:
        target = _resolve_extracted(path)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    except FileNotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
    if target.suffix.casefold() not in {".tga", ".png"}:
        raise HTTPException(status_code=400, detail="not an image file")
    if target.suffix.casefold() == ".png":
        return FileResponse(target, media_type="image/png")
    try:
        from PIL import Image
        import io
        image = Image.open(target)
        buf = io.BytesIO()
        image.save(buf, format="PNG")
        buf.seek(0)
        return Response(content=buf.read(), media_type="image/png")
    except Exception as exc:
        raise HTTPException(status_code=500, detail=f"convert failed: {exc}") from exc


@app.get("/api/media")
def media_file(path: str = Query(..., description="video path under Content/Extracted")):
    """Serve an extracted video with the right MIME so it plays in the viewer."""
    try:
        target = _resolve_extracted(path)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    except FileNotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
    media_type = _MEDIA_MIME.get(target.suffix.casefold())
    if not media_type:
        raise HTTPException(status_code=400, detail="not a playable media file")
    return FileResponse(target, media_type=media_type)


class DecalPlacement(BaseModel):
    path: str
    u: float = 0.5
    v: float = 0.5
    scale: float = 1.0
    rotation: float = 0.0

class ComposeClothingRequest(BaseModel):
    item: str
    colors: dict[str, str] = {}
    decals: list[DecalPlacement] = []

@app.post("/api/compose/clothing")
def compose_clothing_endpoint(req: ComposeClothingRequest):
    """Composite clothing texture with ColMask region tints and decals."""
    try:
        name, item_id, item_root, tex_dir, mesh_path, is_body_item, tex_base = _resolve_clothing_preview(req.item)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    diff_path = next((
        candidate for candidate in (
            tex_dir / f"{tex_base}_Main_Diff.tga",
            tex_dir / f"{tex_base}_Diff.tga",
            tex_dir / f"{tex_base}_Xtra_Diff.tga",
        ) if candidate.is_file()
    ), None)
    if diff_path is None:
        raise HTTPException(status_code=404, detail=f"diffuse texture not found for item: {name}")

    regions = find_colmask_for_item(item_id) or {}

    try:
        layer_png = composite_clothing(
            base_diffuse=diff_path,
            colmasks=regions,
            region_colors=req.colors,
            decals=[d.model_dump() if hasattr(d, "model_dump") else d.dict() for d in req.decals]
        )
        if is_body_item and "armpad" not in name.casefold():
            prefix = "M" if name.startswith("M_") else "F"
            skin_dir = CHARACTERS_BULK / f"{prefix}_Body_Skin" / f"{prefix}_Body_Skin" / "Texture2D"
            skin_diff = next((
                candidate for candidate in (
                    skin_dir / f"{prefix}_Skin_Colour_Caucasian_Diff.tga",
                    skin_dir / f"{prefix}_Skin_Colour_Pale_Diff.tga",
                ) if candidate.is_file()
            ), None)
            if skin_diff is not None:
                layer_path = _cache_png(
                    f"compose:{name}:{json.dumps(req.model_dump() if hasattr(req, 'model_dump') else req.dict(), sort_keys=True)}",
                    layer_png,
                )
                norm_path = next((
                    candidate for candidate in (
                        tex_dir / f"{tex_base}_Main_Norm.tga",
                        tex_dir / f"{tex_base}_Norm.tga",
                        tex_dir / f"{tex_base}_Xtra_Norm.tga",
                    ) if candidate.is_file()
                ), None)
                png_bytes = composite_body_overlay(skin_diff, layer_path)
            else:
                png_bytes = layer_png
        else:
            png_bytes = layer_png
        return Response(content=png_bytes, media_type="image/png")
    except Exception as exc:
        raise HTTPException(status_code=500, detail=f"compositing failed: {exc}")
