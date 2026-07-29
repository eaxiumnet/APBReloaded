#!/usr/bin/env python3
"""Test script to verify the complete material pipeline including glTF export."""

from pathlib import Path
from psk import parse_psk_file
from texture_resolver import find_default_textures
from gltf_export import mesh_to_glb

def test_weapon_pipeline(weapon_name, psk_path):
    """Test the complete pipeline for a weapon."""
    print(f"Testing {weapon_name} pipeline:")
    
    # Parse the PSK file
    mesh = parse_psk_file(Path(psk_path))
    print(f"  Mesh parsed: {len(mesh.points)} vertices, {len(mesh.faces)} faces")
    
    # Find textures
    textures = find_default_textures(Path(psk_path))
    print(f"  Textures found: {list(textures.keys())}")
    
    # Export to glTF
    glb_data = mesh_to_glb(mesh, textures=textures if textures else None)
    print(f"  GLB exported: {len(glb_data)} bytes")
    
    # Save to file for inspection
    output_path = Path(f"{weapon_name}_test.glb")
    with open(output_path, "wb") as f:
        f.write(glb_data)
    print(f"  GLB saved to: {output_path}")
    print()

def test_clothing_pipeline(item_name, psk_path, texture_dir):
    """Test the complete pipeline for a clothing item."""
    print(f"Testing {item_name} pipeline:")
    
    # Parse the PSK file
    mesh = parse_psk_file(Path(psk_path))
    print(f"  Mesh parsed: {len(mesh.points)} vertices, {len(mesh.faces)} faces")
    
    # Manually build texture dict (like the server does)
    textures = {}
    
    # Base color (diffuse)
    diff_path = Path(texture_dir) / f"{item_name}_Main_Diff.tga"
    if diff_path.is_file():
        textures["baseColor"] = diff_path
    
    # Normal map
    norm_path = Path(texture_dir) / f"{item_name}_Main_Norm.tga"
    if norm_path.is_file():
        textures["normal"] = norm_path
    
    # Specular/BRDF mask
    spec_path = Path(texture_dir) / f"{item_name}_Golem_BRDFMask.tga"
    if spec_path.is_file():
        textures["specular"] = spec_path
    
    print(f"  Textures found: {list(textures.keys())}")
    
    # Export to glTF
    glb_data = mesh_to_glb(mesh, textures=textures if textures else None)
    print(f"  GLB exported: {len(glb_data)} bytes")
    
    # Save to file for inspection
    output_path = Path(f"{item_name}_test.glb")
    with open(output_path, "wb") as f:
        f.write(glb_data)
    print(f"  GLB saved to: {output_path}")
    print()

if __name__ == "__main__":
    # Test Tommy Gun
    test_weapon_pipeline(
        "TommyGun",
        r"D:\APBReloaded\Content\Extracted\WeaponsBase\Weapon_TommyGun\Weapon_TommyGun\SkeletalMesh3\Weapon_TommyGun_LOD0.psk"
    )
    
    # Test Katana
    test_weapon_pipeline(
        "Katana",
        r"D:\APBReloaded\Content\Extracted\WeaponsBase\Weapon_Katana\Weapon_Katana\SkeletalMesh3\Weapon_Katana_LOD0.psk"
    )
    
    # Test F_Armpads_Armoured
    test_clothing_pipeline(
        "F_Armpads_Armoured",
        r"D:\APBReloaded\Content\Extracted\CharactersBulk\F_Armpads_Armoured\F_Body_Base\SkeletalMesh3\F_Body_Base.psk",
        r"D:\APBReloaded\Content\Extracted\CharactersBulk\F_Armpads_Armoured\F_Armpads_Armoured\Texture2D"
    )
    
    print("Pipeline test completed successfully!")