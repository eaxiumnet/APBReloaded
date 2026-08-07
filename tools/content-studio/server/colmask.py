"""ColMask texture resolver + clothing catalog for APB character items.

ColMask files are region masks used by the skin compositor. Each clothing item has
multiple ColMask TGA files (one per region), named like:
  <Item>_Main_ColMask_<Region>.tga

Regions are item-specific (e.g., "0Base", "1Straps", "2Buckle") — NOT a fixed set.
The masks are 512x1024 24-bit TGA textures where each region is a solid color.

This module:
- Resolves ColMask files for a given clothing item
- Builds a catalog of all clothing items with ColMask regions
"""
from __future__ import annotations

import re
from pathlib import Path

# ColMask naming pattern: <Item>_Main_ColMask_<Region>.tga
_COLMASK_RE = re.compile(r"^(.+)_Main_ColMask_(.+)\.tga$", re.IGNORECASE)


def _find_item_folder(psk_or_texture: Path) -> Path | None:
    """Find the clothing item folder (parent of Texture2D).

    Expected structure:
      Content/Extracted/CharactersBulk/<Item>/<Item>/Texture2D/<Item>_Main_ColMask_*.tga
    """
    # If this is a Texture2D file, go up to the item folder
    if psk_or_texture.parent.name == "Texture2D":
        return psk_or_texture.parent.parent
    # If this is already the item folder, return it
    if psk_or_texture.is_dir() and (psk_or_texture / "Texture2D").is_dir():
        return psk_or_texture
    return None


def list_colmask_regions(item_folder: Path) -> dict[str, Path]:
    """List all ColMask regions for a clothing item.

    Returns:
        Dict mapping region name (e.g., "0Base", "1Straps") to absolute TGA path.
    """
    item = item_folder.resolve()
    texture_dir = item / "Texture2D"
    if not texture_dir.is_dir():
        return {}

    regions: dict[str, Path] = {}
    for tga in texture_dir.glob("*_Main_ColMask_*.tga"):
        match = _COLMASK_RE.match(tga.name)
        if match:
            region = match.group(2)
            regions[region] = tga.resolve()

    return regions


def find_colmask_for_item(item_path: str) -> dict[str, Path] | None:
    """Resolve ColMask regions for a clothing item by path.

    Args:
        item_path: Relative path under Content/Extracted/CharactersBulk/
                   (e.g., "F_Armpads_Armoured/F_Armpads_Armoured")

    Returns:
        Dict of region -> TGA path, or None if item not found.
    """
    # colmask.py lives at tools/content-studio/server/ -> parents[3] == repo root
    repo_root = Path(__file__).resolve().parents[3]
    extract_root = repo_root / "Content" / "Extracted"
    item_folder = extract_root / "CharactersBulk" / item_path
    if not item_folder.is_dir():
        return None
    return list_colmask_regions(item_folder)


def build_clothing_catalog(characters_bulk: Path) -> list[dict]:
    """Build a catalog of clothing items with ColMask regions.

    Args:
        characters_bulk: Path to Content/Extracted/CharactersBulk/

    Returns:
        List of dicts: {id, name, region_count, regions}
        - id: relative path under CharactersBulk (e.g., "F_Armpads_Armoured/F_Armpads_Armoured")
        - name: display name (item folder name)
        - region_count: number of ColMask regions
        - regions: list of region names (e.g., ["0Base", "1Straps", "2Buckle"])
    """
    if not characters_bulk.is_dir():
        return []

    items = []
    for item_dir in sorted(characters_bulk.iterdir()):
        if not item_dir.is_dir():
            continue
        # Each item has a nested structure: <Item>/<Item>/Texture2D/
        inner = item_dir / item_dir.name
        if not inner.is_dir():
            continue

        regions = list_colmask_regions(inner)
        if not regions:
            continue  # skip items without ColMask regions

        texture_dir = inner / "Texture2D"
        has_diffuse = any(
            path.is_file()
            for pattern in ("*_Main_Diff.tga", "*_Xtra_Diff.tga", "*_Diff.tga")
            for path in texture_dir.glob(pattern)
        )
        if not has_diffuse:
            continue  # mapping-only folders do not produce a useful preview

        item_id = f"{item_dir.name}/{inner.name}"
        items.append({
            "id": item_id,
            "name": item_dir.name,
            "region_count": len(regions),
            "regions": sorted(regions.keys()),
        })

    return items
