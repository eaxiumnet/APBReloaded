"""Tests for symbols.py — Symbol catalog builder."""
import pytest
from pathlib import Path
from symbols import build_symbol_catalog, find_symbol_path


def test_build_symbol_catalog_returns_categories():
    """Test that build_symbol_catalog returns a list of categories with symbols."""
    # This test requires the actual SymbolsBulk directory
    # test_symbols.py lives at tools/content-studio/server/tests/ -> parents[4] == repo root
    repo_root = Path(__file__).resolve().parents[4]
    symbols_bulk = repo_root / "Content" / "Extracted" / "SymbolsBulk"
    
    if not symbols_bulk.is_dir():
        pytest.skip("SymbolsBulk directory not found")
    
    categories = build_symbol_catalog(symbols_bulk)
    
    # Should have at least some categories
    assert isinstance(categories, list)
    assert len(categories) > 0
    
    # Each category should have the expected structure
    for cat in categories:
        assert "category" in cat
        assert "symbol_count" in cat
        assert "symbols" in cat
        assert isinstance(cat["symbols"], list)
        assert cat["symbol_count"] == len(cat["symbols"])
        
        # Each symbol should have id, name, path
        for symbol in cat["symbols"][:3]:  # Check first 3
            assert "id" in symbol
            assert "name" in symbol
            assert "path" in symbol
            assert symbol["path"].endswith(".tga")


def test_find_symbol_path_resolves_valid_symbol():
    """Test that find_symbol_path resolves a valid symbol ID to an absolute path."""
    repo_root = Path(__file__).resolve().parents[4]
    symbols_bulk = repo_root / "Content" / "Extracted" / "SymbolsBulk"
    
    if not symbols_bulk.is_dir():
        pytest.skip("SymbolsBulk directory not found")
    
    # Find a real symbol to test with
    categories = build_symbol_catalog(symbols_bulk)
    if not categories or not categories[0]["symbols"]:
        pytest.skip("No symbols found in catalog")
    
    test_symbol = categories[0]["symbols"][0]
    symbol_id = test_symbol["id"]
    
    result = find_symbol_path(symbols_bulk, symbol_id)
    
    assert result is not None
    assert result.is_file()
    assert result.suffix.lower() == ".tga"
    assert str(result).endswith(symbol_id.replace("/", "\\"))


def test_find_symbol_path_missing_symbol():
    """Test that find_symbol_path returns None for missing symbols."""
    repo_root = Path(__file__).resolve().parents[4]
    symbols_bulk = repo_root / "Content" / "Extracted" / "SymbolsBulk"
    
    if not symbols_bulk.is_dir():
        pytest.skip("SymbolsBulk directory not found")
    
    result = find_symbol_path(symbols_bulk, "NonExistent/Path/Symbol.tga")
    
    assert result is None


def test_find_symbol_path_rejects_non_tga():
    """Test that find_symbol_path rejects non-TGA files."""
    repo_root = Path(__file__).resolve().parents[4]
    symbols_bulk = repo_root / "Content" / "Extracted" / "SymbolsBulk"
    
    if not symbols_bulk.is_dir():
        pytest.skip("SymbolsBulk directory not found")
    
    # Try to resolve a non-TGA file (should return None)
    result = find_symbol_path(symbols_bulk, "Fonts_Basic/Fonts_Basic/Texture2D/0.png")
    
    assert result is None


def test_build_symbol_catalog_empty_dir():
    """Test that build_symbol_catalog handles empty/missing directories gracefully."""
    from tempfile import TemporaryDirectory
    
    with TemporaryDirectory() as tmpdir:
        result = build_symbol_catalog(Path(tmpdir))
        assert result == []
    
    # Test with non-existent directory
    result = build_symbol_catalog(Path("Z:\\NonExistent\\Path"))
    assert result == []


def test_fonts_category_has_expected_symbols():
    """Test that Fonts_Basic category has expected symbols (0-9, A-Z)."""
    repo_root = Path(__file__).resolve().parents[4]
    symbols_bulk = repo_root / "Content" / "Extracted" / "SymbolsBulk"
    
    if not symbols_bulk.is_dir():
        pytest.skip("SymbolsBulk directory not found")
    
    categories = build_symbol_catalog(symbols_bulk)
    
    # Find Fonts_Basic category
    fonts_basic = next((cat for cat in categories if cat["category"] == "Fonts_Basic"), None)
    
    if fonts_basic is None:
        pytest.skip("Fonts_Basic category not found")
    
    # Should have at least 36 symbols (0-9 + A-Z)
    assert fonts_basic["symbol_count"] >= 36
    
    # Check that we have numeric and alphabetic symbols
    symbol_names = {s["name"] for s in fonts_basic["symbols"]}
    assert "0" in symbol_names or "1" in symbol_names  # At least some numbers
    assert "A" in symbol_names or "B" in symbol_names  # At least some letters


def test_primitives_category_has_decals():
    """Test that Primitives_* categories have decal symbols."""
    repo_root = Path(__file__).resolve().parents[4]
    symbols_bulk = repo_root / "Content" / "Extracted" / "SymbolsBulk"
    
    if not symbols_bulk.is_dir():
        pytest.skip("SymbolsBulk directory not found")
    
    categories = build_symbol_catalog(symbols_bulk)
    
    # Find any Primitives category
    primitives = [cat for cat in categories if cat["category"].startswith("Primitives_")]
    
    assert len(primitives) > 0, "No Primitives categories found"
    
    # Each should have at least some symbols
    for cat in primitives:
        assert cat["symbol_count"] > 0