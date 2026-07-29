# Track B — Slice 2: ColMask/Symbol Editor (Implementation Plan)

Date: 2026-07-22 · Agent: Cline (Sonnet) · Depends on: Slice 1 (VIEWER) complete

## Goal

Build a ColMask region-color editor + symbol decal placer with live 3D preview, per the
Track B plan §5 Slice 2.

**S2 happy**: edit a ColMask region color and drop a SymbolsBulk decal → 3D updates live.
*Proof*: before/after screenshots differ in the expected region (pixel-diff), elsewhere stable.

## Current State (2026-07-22 analysis)

### ColMask System

- **Location**: `Content/Extracted/CharactersBulk/<Item>/<Item>/Texture2D/`
- **Naming**: `<Item>_Main_ColMask_<Region>.tga` where Region = `0Base`, `1Eyes`, `2Nose`, `3Teeth`
- **Format**: TGA textures (verified: 32x32px, 24-bit, solid-color masks)
- **Variants**: `_Diff`, `_Norm`, `_Golem_BRDFMask` (per the plan §3 ground truth)
- **Purpose**: Region masks for the skin compositor — each region can be tinted independently

**Example**: `F_Armpads_Armoured_Main_ColMask_0Base.tga` (146 bytes, 32x32, solid light-yellow RGB 255,255,159)

### SymbolsBulk

- **Location**: `Content/Extracted/SymbolsBulk/<Category>/<Category>/Texture2D/`
- **Categories**: Fonts_*, Primitives_* (18 subdirs, ~1461 decals total per plan)
- **Format**: TGA textures (20-30KB each, alpha-channel decals)
- **Example**: `Primitives_Simple01/Primitives_Simple01/Texture2D/*.tga`

### Existing Code

- `server/texture_resolver.py`: handles weapon diffuse/normal/diffspec, NO ColMask support
- `server/main.py`: FastAPI app with `/api/catalog/weapons`, `/api/mesh.glb`, `/api/texture` endpoints
- `web/src/App.tsx`: weapons-only viewer (sidebar + 3D stage), no ColMask/symbol UI

## Implementation Strategy

### Phase 1: ColMask Texture Viewer (foundation)

**Backend** (`server/colmask.py` + `server/main.py`):
- New endpoint: `GET /api/colmask?item=<path>` → returns ColMask texture paths for an item
- New endpoint: `GET /api/colmask/texture?path=<path>` → serves the TGA (converted to PNG for browser)
- Reuse existing `texture_resolver.py` patterns for path resolution

**Frontend** (`web/src/ColMaskPanel.tsx`):
- New panel showing the 4 ColMask regions (Base/Eyes/Nose/Teeth) as thumbnails
- Click a region → highlight it (future: color picker)
- Integrate into App.tsx alongside the weapons sidebar

**QA**: Playwright spec asserts ColMask panel loads + thumbnails render

### Phase 2: Region Color Editor

**Backend**:
- New endpoint: `POST /api/colmask/compose` → accepts region colors, returns composed texture
- Implement ColMask compositor: load base TGA, tint regions per color map, return PNG

**Frontend**:
- Color picker per region (HTML `<input type="color">`)
- On change → call compose endpoint → update 3D material

**QA**: Playwright spec asserts color change → screenshot differs in expected region

### Phase 3: Symbol Decal Placement

**Backend**:
- New endpoint: `GET /api/symbols?category=<cat>` → lists symbols in a category
- New endpoint: `GET /api/symbol/texture?path=<path>` → serves symbol TGA as PNG
- New endpoint: `POST /api/colmask/compose-with-decal` → compose + place decal at UV coords

**Frontend**:
- Symbol browser panel (grid of thumbnails)
- Click symbol → click 3D stage → place decal at UV hit point
- Live update on placement

**QA**: Playwright spec asserts decal placement → screenshot shows decal

### Phase 4: Live 3D Compositor (integration)

**Frontend**:
- Modify `Model` component to accept composed texture as `baseColor` override
- On ColMask change or decal placement → re-compose → update material

**Backend**:
- Cache composed textures (keyed by item + region colors + decals)
- Return GLB with composed texture applied (or separate texture endpoint)

**QA**: Full S2 happy spec — edit color + drop decal → before/after screenshots differ

## Risks & Mitigations

- **ColMask format unknown**: The 146-byte TGA is tiny (32x32 solid color). May be a metadata
  stub, not the actual mask. Mitigation: inspect larger ColMask files (23KB+) to find real masks.
- **UV mapping for decals**: Placing decals at UV hit points requires mesh UV analysis.
  Mitigation: start with fixed UV coords (center of mesh), iterate to hit-point placement.
- **Performance**: Re-composing textures on every color change could be slow.
  Mitigation: debounce color picker (500ms), cache composed textures.

## Files to Create/Modify

New:
- `server/colmask.py` — ColMask resolver + compositor
- `server/symbols.py` — SymbolsBulk browser
- `web/src/ColMaskPanel.tsx` — ColMask region viewer/editor
- `web/src/SymbolBrowser.tsx` — Symbol decal browser
- `web/src/ColMaskCompositor.ts` — three.js material updater
- `server/tests/test_colmask.py` — ColMask unit tests
- `web/e2e/colmask.spec.ts` — S2 Playwright spec

Modified:
- `server/main.py` — add ColMask/symbol endpoints
- `web/src/App.tsx` — integrate ColMaskPanel + SymbolBrowser
- `web/src/styles.css` — ColMask/symbol UI styles
- `work/trackB_content_studio.md` — status log entry

## Definition of Done (Slice 2)

- ColMask panel shows 4 regions for a character clothing item
- Color picker per region → live 3D update (material changes)
- Symbol browser loads SymbolsBulk categories
- Click symbol + click stage → decal placed on mesh
- Playwright S2 spec: before/after screenshots differ in expected region
- All existing Slice 1 specs still green (no regression)

## Next Steps

1. **Investigate ColMask format**: Read a 23KB ColMask TGA, verify it's a real mask (not stub)
2. **Implement Phase 1**: ColMask texture viewer (backend + frontend)
3. **QA Phase 1**: Playwright spec asserts thumbnails load
4. **Implement Phase 2-4**: Region editor + symbol placement + live compositor
5. **QA Slice 2**: Full S2 happy spec green
