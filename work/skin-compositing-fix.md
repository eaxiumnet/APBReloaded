# Skin Compositing & Glow Effects Fix

## Date: 2026-07-22

## Problem
Weapon skins were rendering incorrectly:
- Flag skins (Norway, etc.) were stretched across the entire weapon, looking misaligned
- Glow/emissive effects (SugarPlumFairy) were not rendering at all
- Gold/metallic effects were missing
- Skins appeared as a total mess instead of properly composited overlays

## Root Cause
The old code did `textures["baseColor"] = skin_texture_path` - a simple
texture replacement that:
1. Stretched small pattern textures (64x64, 128x128) to fill 256x256 weapon UVs
2. Ignored Mask1/Mask2 textures that control WHERE skins appear
3. Ignored paired _Emissive.tga textures for glow effects
4. Ignored multi-texture skin groups (BG + Text, BG + Emissive)
5. Ignored reflection cubemaps for gold/metal skins

## Investigation Findings
APB weapon skins are a 5-category composite system:
1. **patterns** (91 per weapon) - tile across weapon UV at native resolution
2. **flags** (21 per weapon) - country flag decals on mask regions
3. **backgrounds** (8 per weapon) - background layers for composite skins
4. **emissive** (1 per weapon) - glow maps paired with specific skins
5. **reflection_cubemaps** (6 per weapon) - gold/metal reflections

Multi-texture skin groups detected:
- SugarPlumFairy: BG + Emissive (glow effect)
- BadAttitude: BG + Text (text overlay)
- Aggression: Pattern02 + TilingPattern
- Asylum: 01 + 02 (variants)

## Fix Applied

### New `compositor.py` module
- `composite_skin()` returns `(diffuse_png, emissive_png)` tuple
- Tiles small patterns across weapon UV at native resolution
- Resizes flags/large textures to weapon UV space
- Composites BG + Text layers for multi-texture skins
- Uses Mask1/Mask2 R-channels to control WHERE skins appear
- Detects and extracts emissive maps for glow effects
- Detects self-illuminated skins via alpha channel

### Updated `texture_resolver.py`
- Now detects Mask1, Mask2, and Emissive textures
- Returns them in the textures dict for the compositor

### Updated `gltf_export.py`
- Added emissiveTexture support for glow effects
- Increased metallic factor (0.8) when specular map present
- Decreased roughness factor (0.3) for metallic appearance

### Updated `main.py` mesh endpoint
- Composites skin with base diffuse using masks
- Saves emissive map to temp file and passes to GLB exporter

## Verification
- All 131 existing tests pass
- Flag skins now composite onto mask-defined regions only
- SugarPlumFairy correctly detects and renders its emissive glow
- Pattern skins tile at native resolution instead of stretching
- Gold/metal skins now have proper metallic appearance via specular maps

## Files Changed
- `tools/content-studio/server/compositor.py` (new)
- `tools/content-studio/server/texture_resolver.py` (mask/emissive detection)
- `tools/content-studio/server/gltf_export.py` (emissive + metallic)
- `tools/content-studio/server/main.py` (compositing in mesh endpoint)
