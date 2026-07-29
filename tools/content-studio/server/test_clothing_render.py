#!/usr/bin/env python3
"""Test script to verify clothing item rendering."""

from pathlib import Path
from psk import parse_psk_file
from texture_resolver import find_default_textures
from gltf_export import mesh_to_glb

def test_clothing_item(item_name):
    """Test rendering of a clothing item."""
    print(f"Testing clothing item: {item_name}")
    
    # Define paths
    repo_root = Path(__file__).resolve().parents[3]
    characters_bulk = repo_root / "Content" / "Extracted" / "CharactersBulk"
    
    # Item mesh path - accessories use their own mesh name
    item_path = characters_bulk / item_name / item_name / "SkeletalMesh3" / f"{item_name}_Xtra.psk"
    print(f"  Mesh path: {item_path}")
    
    if not item_path.is_file():
        print(f"  ERROR: Mesh file not found!")
        return False
    
    # Parse the PSK file
    try:
        mesh = parse_psk_file(item_path)
        print(f"  Mesh parsed: {len(mesh.points)} vertices, {len(mesh.faces)} faces")
    except Exception as e:
        print(f"  ERROR: Failed to parse mesh: {e}")
        return False
    
    # Build texture dict with all available material channels
    tex_dir = characters_bulk / item_name / item_name / "Texture2D"
    textures = {}
    
    # Base color (diffuse)
    diff_path = tex_dir / f"{item_name}_Xtra_Diff.tga"
    if diff_path.is_file():
        textures["baseColor"] = diff_path
        print(f"  Found baseColor: {diff_path.name}")
    
    # Normal map
    norm_path = tex_dir / f"{item_name}_Xtra_Norm.tga"
    if norm_path.is_file():
        textures["normal"] = norm_path
        print(f"  Found normal: {norm_path.name}")
    
    # Check for other potential textures
    for texture_file in tex_dir.glob("*.tga"):
        if "_ColMask_" in texture_file.name:
            print(f"  Found ColMask: {texture_file.name}")
    
    # Export to glTF
    try:
        glb_data = mesh_to_glb(mesh, textures=textures if textures else None)
        print(f"  GLB exported: {len(glb_data)} bytes")
        
        # Save to file for inspection
        output_path = Path(f"{item_name}_test.glb")
        with open(output_path, "wb") as f:
            f.write(glb_data)
        print(f"  GLB saved to: {output_path}")
        return True
    except Exception as e:
        print(f"  ERROR: Failed to export GLB: {e}")
        return False

if __name__ == "__main__":
    # Test the dogtag items
    items_to_test = [
        "F_Neckwear_Necklace_Enforcement_Dogtag",
        "M_Neckwear_Necklace_Enforcement_Dogtag"
    ]
    
    for item in items_to_test:
        success = test_clothing_item(item)
        print(f"  Result: {'SUCCESS' if success else 'FAILED'}")
        print()
    
    print("Clothing item test completed!")