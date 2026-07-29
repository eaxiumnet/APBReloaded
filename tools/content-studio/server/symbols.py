"""Symbol decal catalog builder for APB character customization.

SymbolsBulk contains decal textures used for character customization (logos, patterns,
fonts, etc.). Structure:
  Content/Extracted/SymbolsBulk/<Category>/<Category>/Texture2D/*.tga

Categories:
- Fonts_*: Text/font symbols (Basic, Gothic, Graffiti, Korean, Occult, Script, etc.)
- Primitives_*: Shapes, patterns, decals (AsianFontShapes, FlamesTears, Misc, Nature, etc.)
- SymbolMaterials: Material definitions (29 files)
- SymbolTexturePages: Atlas pages (86 files)

Total: 1461 TGA files across 22 categories.
"""
from __future__ import annotations

from pathlib import Path


def build_symbol_catalog(symbols_bulk: Path) -> list[dict]:
    """Build a catalog of all symbol decals in SymbolsBulk.

    Args:
        symbols_bulk: Path to Content/Extracted/SymbolsBulk/

    Returns:
        List of dicts: {category, symbol_count, symbols[]}
        Each symbol: {id, name, path}
        - id: relative path under SymbolsBulk (e.g., "Fonts_Basic/Fonts_Basic/Texture2D/0.tga")
        - name: display name (e.g., "0")
        - path: absolute path to TGA file
    """
    if not symbols_bulk.is_dir():
        return []

    categories = []
    for category_dir in sorted(symbols_bulk.iterdir()):
        if not category_dir.is_dir():
            continue

        # Expected structure: <Category>/<Category>/Texture2D/*.tga
        inner = category_dir / category_dir.name
        texture_dir = inner / "Texture2D"
        if not texture_dir.is_dir():
            continue

        symbols = []
        for tga in sorted(texture_dir.glob("*.tga")):
            rel_path = tga.relative_to(symbols_bulk)
            symbols.append({
                "id": str(rel_path).replace("\\", "/"),
                "name": tga.stem,
                "path": str(tga.resolve()),
            })

        if symbols:
            categories.append({
                "category": category_dir.name,
                "symbol_count": len(symbols),
                "symbols": symbols,
            })

    return categories


def find_symbol_path(symbols_bulk: Path, symbol_id: str) -> Path | None:
    """Resolve a symbol ID to an absolute TGA path.

    Args:
        symbols_bulk: Path to Content/Extracted/SymbolsBulk/
        symbol_id: Relative path under SymbolsBulk (e.g., "Fonts_Basic/Fonts_Basic/Texture2D/0.tga")

    Returns:
        Absolute path to TGA file, or None if not found.
    """
    symbol_path = symbols_bulk / symbol_id
    if not symbol_path.is_file():
        return None
    if symbol_path.suffix.lower() != ".tga":
        return None
    return symbol_path.resolve()