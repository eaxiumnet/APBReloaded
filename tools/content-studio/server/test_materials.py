#!/usr/bin/env python3
"""Test script to verify material channel processing."""

from pathlib import Path
from texture_resolver import find_default_textures

# Test with a weapon that has all texture types
weapon_psk = Path(r"D:\APBReloaded\Content\Extracted\WeaponsBase\Weapon_TommyGun\Weapon_TommyGun\SkeletalMesh3\Weapon_TommyGun_LOD0.psk")
print("Testing Tommy Gun material channels:")
textures = find_default_textures(weapon_psk)
for channel, path in textures.items():
    print(f"  {channel}: {path.name}")

print()

# Test with a katana that has different texture types
katana_psk = Path(r"D:\APBReloaded\Content\Extracted\WeaponsBase\Weapon_Katana\Weapon_Katana\SkeletalMesh3\Weapon_Katana_LOD0.psk")
print("Testing Katana material channels:")
textures = find_default_textures(katana_psk)
for channel, path in textures.items():
    print(f"  {channel}: {path.name}")

print()

# Test with a clothing item by manually checking the texture directory
print("Testing F_Armpads_Armoured material channels:")
clothing_dir = Path(r"D:\APBReloaded\Content\Extracted\CharactersBulk\F_Armpads_Armoured\F_Armpads_Armoured\Texture2D")
if clothing_dir.exists():
    for texture_file in sorted(clothing_dir.glob("*.tga")):
        print(f"  {texture_file.name}")
else:
    print("  Texture directory not found")