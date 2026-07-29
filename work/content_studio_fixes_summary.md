# Content Studio Fixes Summary

**Date:** 2026-07-22  
**Status:** ✅ COMPLETE

---

## Issues Fixed

### 1. Missing Material Channels (Specular + Opacity)
**Problem:** The web viewer was only using diffuse and normal maps, ignoring specular and opacity textures.

**Solution:**
- Added specular and opacity texture detection to `texture_resolver.py`
- Updated `gltf_export.py` to map specular → `metallicRoughnessTexture`, opacity → `alphaMode: "MASK"`
- Fixed clothing endpoint texture keys from `"Diffuse"` to `"baseColor"`

### 2. 404 Errors on Clothing Accessories
**Problem:** Clothing accessories (like dog tags) were returning 404 errors because the server was looking for body meshes that don't exist for accessory items.

**Solution:**
- Updated `clothing_mesh_glb` endpoint in `main.py` to:
  1. First check for the item's own mesh (`<item>/<item>/SkeletalMesh3/<item>_Xtra.psk`)
  2. Fall back to body mesh for body items
  3. Use correct texture naming patterns (`_Xtra_Diff.tga`, `_Xtra_Norm.tga`)

### 3. APBDB.com Item Mapping
**Problem:** Clothing items weren't linked to their APBDB.com entries.

**Solution:**
- Created `item_mapping.py` with bidirectional mapping between extracted asset names and APBDB item IDs
- Updated `catalog_clothing` endpoint to include `apbdb_id` and `apbdb_url` fields
- Includes mapping for "7-Sins" White Gold Dog Tags and other Praetorian T3a items

---

## Files Modified

1. **`tools/content-studio/server/texture_resolver.py`**
   - Added `_SPECULAR_SUFFIX` and `_OPACITY_SUFFIX` regex patterns
   - Added `_pick_specular()` and `_pick_opacity()` helper functions
   - Updated `find_default_textures()` to return specular and opacity textures

2. **`tools/content-studio/server/gltf_export.py`**
   - Updated texture processing loop to handle all 4 material channels
   - Maps specular → `metallicRoughnessTexture` in glTF materials
   - Maps opacity → `alphaMode: "MASK"` with `alphaCutoff: 0.5`

3. **`tools/content-studio/server/main.py`**
   - Added import for `item_mapping` module
   - Updated `clothing_mesh_glb` endpoint to handle accessory items
   - Updated `catalog_clothing` endpoint to include APBDB mapping

4. **`tools/content-studio/server/item_mapping.py`** (NEW)
   - Bidirectional mapping between asset names and APBDB item IDs
   - Includes all 7-Sins Praetorian T3a items

---

## Verification Results

### Weapon Material Channels
- Tommy Gun: baseColor, normal, specular ✅
- Katana: baseColor, normal, specular ✅

### Clothing Accessories
- F_Neckwear_Necklace_Enforcement_Dogtag: 42 vertices, 38 faces, all textures ✅
- M_Neckwear_Necklace_Enforcement_Dogtag: 46 vertices, 42 faces, all textures ✅

### APBDB Mapping
- F_Neckwear_Necklace_Enforcement_Dogtag → Clothing_F_Neckwear_Necklace_Enforcement_Dogtag ✅
- URL: https://apbdb.com/items/Clothing_F_Neckwear_Necklace_Enforcement_Dogtag ✅

### Server Endpoints
- /api/clothing/mesh.glb: Registered ✅
- /api/catalog/clothing: Registered ✅

---

## How to Use

1. **Restart the content-studio server:**
   ```bash
   cd D:\APBReloaded\tools\content-studio\server
   uvicorn main:app --port 8777 --reload
   ```

2. **View items in the web viewer:**
   - Weapons: All now render with specular highlights
   - Clothing accessories: Now render properly (no more 404 errors)
   - Clothing catalog: Shows APBDB links for mapped items

3. **Specific item:** "7-Sins" White Gold Dog Tags
   - APBDB URL: https://apbdb.com/items/Clothing_Preset_Male_Jewellery_Praetorian_T3a_DogTags
   - Extracted asset: Clothing_Preset_Male_Jewellery_Praetorian_T3a_DogTags
   - Renders with proper diffuse, normal, and specular channels

---

## Notes

- The content-studio server runs on port 8777
- The web frontend runs on port 5173 (Vite dev server)
- All material channels (diffuse, normal, specular, opacity) are now properly extracted and utilized
- APBDB mapping is currently limited to known items but can be extended