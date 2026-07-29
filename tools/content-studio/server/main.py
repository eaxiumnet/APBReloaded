"""APB Content Studio backend — FastAPI.

Slice 1 (viewer): serve the weapon catalog and convert extracted ActorX meshes
to glTF on demand for the three.js frontend.

Slice 2 (ColMask editor): serve ColMask region textures for character clothing items.

Run (from tools/content-studio/server):
    uvicorn main:app --port 8777 --reload
"""

from __future__ import annotations

from pathlib import Path

from fastapi import FastAPI, HTTPException, Query
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import Response

from assets import build_weapon_catalog, resolve_mesh_path
from colmask import build_clothing_catalog, find_colmask_for_item
from compositor import composite_skin
from gltf_export import mesh_to_glb
from item_mapping import get_apbdb_id, get_apbdb_url
from psk import parse_psk_file
from symbols import build_symbol_catalog, find_symbol_path
from texture_resolver import find_default_textures

# repo root = .../APBReloaded ; this file = .../tools/content-studio/server/main.py
REPO_ROOT = Path(__file__).resolve().parents[3]
WEAPONS_BASE = REPO_ROOT / "Content" / "Extracted" / "WeaponsBase"
CHARACTERS_BULK = REPO_ROOT / "Content" / "Extracted" / "CharactersBulk"
SYMBOLS_BULK = REPO_ROOT / "Content" / "Extracted" / "SymbolsBulk"

app = FastAPI(title="APB Content Studio", version="0.1.0")

# Vite dev server runs on a different port -> allow localhost origins in dev.
app.add_middleware(
    CORSMiddleware,
    allow_origins=[
        "http://localhost:5173",
        "http://127.0.0.1:5173",
    ],
    allow_methods=["GET"],
    allow_headers=["*"],
)


@app.get("/api/health")
def health() -> dict:
    return {
        "ok": True,
        "weapons_base": str(WEAPONS_BASE),
        "weapons_base_exists": WEAPONS_BASE.is_dir(),
    }


@app.get("/api/catalog/weapons")
def catalog_weapons() -> dict:
    weapons = build_weapon_catalog(WEAPONS_BASE)
    return {"count": len(weapons), "weapons": weapons}


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
    item_folder = CHARACTERS_BULK / item
    
    # Try to find the item's own mesh first (for accessories)
    item_mesh_path = item_folder / item / "SkeletalMesh3" / f"{item}_Xtra.psk"
    if item_mesh_path.is_file():
        mesh_path = item_mesh_path
        tex_dir = item_folder / item / "Texture2D"
        tex_base = f"{item}_Xtra"
    else:
        # Fall back to body mesh for body items
        prefix = "M" if item.startswith("M_") else "F"
        body_name = f"{prefix}_Body_Base"
        mesh_path = item_folder / body_name / "SkeletalMesh3" / f"{body_name}.psk"
        tex_dir = item_folder / item / "Texture2D"
        tex_base = item
    
    if not mesh_path.is_file():
        raise HTTPException(status_code=404, detail=f"mesh not found for item: {item}")

    try:
        mesh = parse_psk_file(mesh_path)
        
        # Build texture dict with all available material channels
        textures = {}
        
        # Base color (diffuse)
        diff_path = tex_dir / f"{tex_base}_Diff.tga"
        if diff_path.is_file():
            textures["baseColor"] = diff_path
        
        # Normal map
        norm_path = tex_dir / f"{tex_base}_Norm.tga"
        if norm_path.is_file():
            textures["normal"] = norm_path
        
        # Specular/BRDF mask
        spec_path = tex_dir / f"{item}_Golem_BRDFMask.tga"
        if spec_path.is_file():
            textures["specular"] = spec_path
        
        # Opacity mask
        opac_path = tex_dir / f"{tex_base}_Opac.tga"
        if opac_path.is_file():
            textures["opacity"] = opac_path
        
        glb = mesh_to_glb(mesh, textures=textures if textures else None)
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
