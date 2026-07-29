#!/usr/bin/env python3
"""Comprehensive verification script for all material mapping fixes."""

from pathlib import Path
from psk import parse_psk_file
from texture_resolver import find_default_textures
from gltf_export import mesh_to_glb
from item_mapping import get_apbdb_id, get_apbdb_url, get_asset_name

def test_weapon_materials():
    """Test that weapons have all material channels."""
    print("=" * 60)
    print("TEST 1: Weapon Material Channels")
    print("=" * 60)
    
    repo_root = Path(__file__).resolve().parents[3]
    weapons_base = repo_root / "Content" / "Extracted" / "WeaponsBase"
    
    # Test Tommy Gun
    tommy_path = weapons_base / "Weapon_TommyGun" / "Weapon_TommyGun" / "SkeletalMesh3" / "Weapon_TommyGun_LOD0.psk"
    textures = find_default_textures(tommy_path)
    print(f"\nTommy Gun:")
    print(f"  Textures: {list(textures.keys())}")
    assert "baseColor" in textures, "Missing baseColor!"
    assert "normal" in textures, "Missing normal!"
    assert "specular" in textures, "Missing specular!"
    print(f"  ✅ All channels present")
    
    # Test Katana
    katana_path = weapons_base / "Weapon_Katana" / "Weapon_Katana" / "SkeletalMesh3" / "Weapon_Katana_LOD0.psk"
    textures = find_default_textures(katana_path)
    print(f"\nKatana:")
    print(f"  Textures: {list(textures.keys())}")
    assert "baseColor" in textures, "Missing baseColor!"
    assert "normal" in textures, "Missing normal!"
    assert "specular" in textures, "Missing specular!"
    print(f"  ✅ All channels present")
    
    print()

def test_clothing_accessories():
    """Test that clothing accessories render properly."""
    print("=" * 60)
    print("TEST 2: Clothing Accessories (Dog Tags)")
    print("=" * 60)
    
    repo_root = Path(__file__).resolve().parents[3]
    characters_bulk = repo_root / "Content" / "Extracted" / "CharactersBulk"
    
    # Test F_Neckwear_Necklace_Enforcement_Dogtag
    item_name = "F_Neckwear_Necklace_Enforcement_Dogtag"
    item_path = characters_bulk / item_name / item_name / "SkeletalMesh3" / f"{item_name}_Xtra.psk"
    
    print(f"\n{item_name}:")
    
    # Check mesh exists
    assert item_path.is_file(), f"Mesh not found: {item_path}"
    print(f"  ✅ Mesh found")
    
    # Parse mesh
    mesh = parse_psk_file(item_path)
    print(f"  ✅ Mesh parsed: {len(mesh.points)} vertices, {len(mesh.faces)} faces")
    
    # Check textures
    tex_dir = characters_bulk / item_name / item_name / "Texture2D"
    textures = {}
    
    diff_path = tex_dir / f"{item_name}_Xtra_Diff.tga"
    if diff_path.is_file():
        textures["baseColor"] = diff_path
    
    norm_path = tex_dir / f"{item_name}_Xtra_Norm.tga"
    if norm_path.is_file():
        textures["normal"] = norm_path
    
    print(f"  Textures: {list(textures.keys())}")
    assert "baseColor" in textures, "Missing baseColor!"
    assert "normal" in textures, "Missing normal!"
    print(f"  ✅ All channels present")
    
    # Export GLB
    glb_data = mesh_to_glb(mesh, textures=textures)
    print(f"  ✅ GLB exported: {len(glb_data)} bytes")
    
    # Test M version too
    item_name = "M_Neckwear_Necklace_Enforcement_Dogtag"
    item_path = characters_bulk / item_name / item_name / "SkeletalMesh3" / f"{item_name}_Xtra.psk"
    
    print(f"\n{item_name}:")
    assert item_path.is_file(), f"Mesh not found: {item_path}"
    print(f"  ✅ Mesh found")
    
    mesh = parse_psk_file(item_path)
    print(f"  ✅ Mesh parsed: {len(mesh.points)} vertices, {len(mesh.faces)} faces")
    
    tex_dir = characters_bulk / item_name / item_name / "Texture2D"
    textures = {}
    
    diff_path = tex_dir / f"{item_name}_Xtra_Diff.tga"
    if diff_path.is_file():
        textures["baseColor"] = diff_path
    
    norm_path = tex_dir / f"{item_name}_Xtra_Norm.tga"
    if norm_path.is_file():
        textures["normal"] = norm_path
    
    print(f"  Textures: {list(textures.keys())}")
    assert "baseColor" in textures, "Missing baseColor!"
    assert "normal" in textures, "Missing normal!"
    print(f"  ✅ All channels present")
    
    glb_data = mesh_to_glb(mesh, textures=textures)
    print(f"  ✅ GLB exported: {len(glb_data)} bytes")
    
    print()

def test_apbdb_mapping():
    """Test that APBDB mapping works correctly."""
    print("=" * 60)
    print("TEST 3: APBDB.com Item Mapping")
    print("=" * 60)
    
    # Test Dog Tags mapping
    asset_name = "F_Neckwear_Necklace_Enforcement_Dogtag"
    apbdb_id = get_apbdb_id(asset_name)
    print(f"\n{asset_name}:")
    print(f"  APBDB ID: {apbdb_id}")
    assert apbdb_id is not None, "APBDB ID not found!"
    assert apbdb_id == "Clothing_F_Neckwear_Necklace_Enforcement_Dogtag", "Wrong APBDB ID!"
    print(f"  ✅ APBDB ID correct")
    
    apbdb_url = get_apbdb_url(asset_name)
    print(f"  APBDB URL: {apbdb_url}")
    assert apbdb_url is not None, "APBDB URL not found!"
    assert "apbdb.com" in apbdb_url, "Invalid APBDB URL!"
    print(f"  ✅ APBDB URL correct")
    
    # Test reverse mapping
    reverse = get_asset_name(apbdb_id)
    print(f"  Reverse mapping: {reverse}")
    assert reverse == asset_name, "Reverse mapping incorrect!"
    print(f"  ✅ Reverse mapping correct")
    
    # Test 7-Sins Dog Tags
    asset_name = "Clothing_Preset_Male_Jewellery_Praetorian_T3a_DogTags"
    apbdb_id = get_apbdb_id(asset_name)
    print(f"\n{asset_name}:")
    print(f"  APBDB ID: {apbdb_id}")
    assert apbdb_id is not None, "APBDB ID not found!"
    print(f"  ✅ APBDB ID found")
    
    print()

def test_server_endpoints():
    """Test that server endpoints work correctly."""
    print("=" * 60)
    print("TEST 4: Server Endpoint Imports")
    print("=" * 60)
    
    try:
        from main import app
        print(f"\n  ✅ Server module loads successfully")
        
        # Check that the clothing mesh endpoint is properly defined
        routes = [route.path for route in app.routes]
        assert "/api/clothing/mesh.glb" in routes, "Clothing mesh endpoint missing!"
        print(f"  ✅ Clothing mesh endpoint registered")
        
        assert "/api/catalog/clothing" in routes, "Clothing catalog endpoint missing!"
        print(f"  ✅ Clothing catalog endpoint registered")
        
    except Exception as e:
        print(f"  ❌ Server module failed to load: {e}")
        raise
    
    print()

if __name__ == "__main__":
    print("\n" + "=" * 60)
    print("APB CONTENT STUDIO - COMPREHENSIVE VERIFICATION")
    print("=" * 60 + "\n")
    
    test_weapon_materials()
    test_clothing_accessories()
    test_apbdb_mapping()
    test_server_endpoints()
    
    print("=" * 60)
    print("ALL TESTS PASSED! ✅")
    print("=" * 60)
    print("\nSummary:")
    print("  - Weapons render with all material channels (diffuse, normal, specular)")
    print("  - Clothing accessories (dog tags) render properly")
    print("  - APBDB.com mapping works for item identification")
    print("  - Server endpoints are properly configured")
    print("\nTo see the changes in action:")
    print("  1. Restart the content-studio server")
    print("  2. Open the web viewer")
    print("  3. Browse to the clothing catalog")
    print("  4. Items will now show APBDB links and render correctly")