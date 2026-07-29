"""Tests for ColMask texture resolver and clothing catalog."""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from colmask import (
    build_clothing_catalog,
    find_colmask_for_item,
    list_colmask_regions,
)


def test_list_colmask_regions_returns_dict(tmp_path: Path) -> None:
    """list_colmask_regions returns a dict of region -> path."""
    item_dir = tmp_path / "TestItem" / "TestItem" / "Texture2D"
    item_dir.mkdir(parents=True)

    # Create sample ColMask files
    (item_dir / "TestItem_Main_ColMask_0Base.tga").touch()
    (item_dir / "TestItem_Main_ColMask_1Straps.tga").touch()
    (item_dir / "TestItem_Main_ColMask_2Buckle.tga").touch()

    regions = list_colmask_regions(item_dir.parent)

    assert len(regions) == 3
    assert "0Base" in regions
    assert "1Straps" in regions
    assert "2Buckle" in regions
    assert all(isinstance(p, Path) for p in regions.values())


def test_list_colmask_regions_empty_dir(tmp_path: Path) -> None:
    """list_colmask_regions returns empty dict for dir without ColMask files."""
    item_dir = tmp_path / "TestItem" / "TestItem" / "Texture2D"
    item_dir.mkdir(parents=True)

    regions = list_colmask_regions(item_dir.parent)

    assert regions == {}


def test_find_colmask_for_item_missing() -> None:
    """find_colmask_for_item returns None for missing item."""
    result = find_colmask_for_item("NonExistent/Item")
    assert result is None


def test_build_clothing_catalog(tmp_path: Path) -> None:
    """build_clothing_catalog returns list of items with regions."""
    characters_bulk = tmp_path / "CharactersBulk"

    # Item 1: 3 regions
    item1_dir = characters_bulk / "Item1" / "Item1" / "Texture2D"
    item1_dir.mkdir(parents=True)
    (item1_dir / "Item1_Main_ColMask_0Base.tga").touch()
    (item1_dir / "Item1_Main_ColMask_1Straps.tga").touch()
    (item1_dir / "Item1_Main_ColMask_2Buckle.tga").touch()

    # Item 2: 2 regions
    item2_dir = characters_bulk / "Item2" / "Item2" / "Texture2D"
    item2_dir.mkdir(parents=True)
    (item2_dir / "Item2_Main_ColMask_0Base.tga").touch()
    (item2_dir / "Item2_Main_ColMask_1Trim.tga").touch()

    # Item 3: no ColMask regions (should be skipped)
    item3_dir = characters_bulk / "Item3" / "Item3" / "Texture2D"
    item3_dir.mkdir(parents=True)
    (item3_dir / "Item3_Diff.tga").touch()

    catalog = build_clothing_catalog(characters_bulk)

    assert len(catalog) == 2
    assert catalog[0]["id"] == "Item1/Item1"
    assert catalog[0]["name"] == "Item1"
    assert catalog[0]["region_count"] == 3
    assert catalog[0]["regions"] == ["0Base", "1Straps", "2Buckle"]

    assert catalog[1]["id"] == "Item2/Item2"
    assert catalog[1]["name"] == "Item2"
    assert catalog[1]["region_count"] == 2
    assert catalog[1]["regions"] == ["0Base", "1Trim"]


def test_build_clothing_catalog_empty_dir(tmp_path: Path) -> None:
    """build_clothing_catalog returns empty list for empty dir."""
    catalog = build_clothing_catalog(tmp_path / "NonExistent")
    assert catalog == []
