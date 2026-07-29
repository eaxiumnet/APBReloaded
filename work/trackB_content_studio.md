# ACTIVE PLAN — Track B: APB Content Studio (external dev tool)

Started: 2026-07-21 · Status: **ACTIVE** · Effort: standalone tooling, parallel to the
game roadmap in `_active.md` (which is Track A + the UE5.8 recreation).

> One plan per effort per AGENTS.md. This file governs the **external content tool** only.
> The in-game editors (Track A) remain in `_active.md` under M13 (vehicles) + M5/M17
> (symbol editor, character morphs) and are gated on reaching those milestones.

---

## 1. Goal (user-agreed, prior AskUserQuestion)

A standalone developer tool to **VIEW + EDIT + ADD NEW** APB content — clothes, skins,
weapons, vehicles, vehicle parts — with a 3D viewer, a skin/ColMask editor, and a
**ship/export path** whose artifacts the existing `tools/scripts/import_*.py` pipeline
ingests into `Content/Imported`.

"Ship" (user's definition) = **export game-ingestable artifacts**, NOT distribute an
installer. This is why we do NOT need Tauri/Electron.

## 2. Stack (LOCKED — user chose "lean" 2026-07-21)

- **Frontend**: Vite + React + TypeScript + three.js via `@react-three/fiber` + `@react-three/drei`.
  Local web app (dev: Vite server; later: static build served by the backend).
- **Backend**: Python (FastAPI + uvicorn) — matches the repo's Python tooling; exposes
  catalog, mesh-conversion, texture, and export endpoints; owns local-filesystem access.
- **Deps kept lean**: `fastapi`, `uvicorn`, `numpy` (binary packing), `Pillow` (tga→png only
  if three.js `TGALoader` proves insufficient). NO Rust, NO Tauri, NO Blender, NO trimesh/scipy.
- **QA**: Playwright (drive real browser, screenshot, assert console clean + scene graph).

**Rejected**: Tauri v2 (adds Rust+Node toolchain for a distributable we don't need);
Blender headless (not installed); pure client-only (needs local FS + export to disk).

## 3. Ground truth (verified 2026-07-21 — do not re-discover)

- Python 3.11.9 ✓ · Node 24.11 / npm 11.6 ✓ · Blender ✗ (path dead) · umodel_64.exe built ✓.
- Extracted assets under `Content/Extracted/`: `WeaponsBase` (base meshes, layout
  `WeaponsBase\<Design>\<Pkg>\SkeletalMesh3\*.psk`), `WeaponSkins` (209 tga),
  `VehiclesBulk`, `CharactersBulk`, `MorphSafeClothing`, `SymbolsBulk` (1461 decal tga),
  `ClothingMenus`, plus `UmodelExport*`.
- `.psk` = standard **ActorX** chunks, CONFIRMED on a real file
  (`Weapon_Armas_Magnum\...\Crm_Magnum_Clip_mk3_LOD0.psk`):
  `ACTRHEAD` · `PNTS0000` (verts, FVector) · `VTXW0000` (wedges: point idx + UV) ·
  `FACE0000` (tris: 3 wedge idx + mat idx) · `MATT0000` (materials) · `REFSKELT` (bones) ·
  `RAWWEIGHTS` (skin weights). Chunk header = 20-byte name + int32 flags + int32 datasize +
  int32 count. `.pskx` = extended (large vertex chunk `VTX32NRM`/`PNTS`>65535, extra chunks).
- **umodel supports `-gltf`** (`SkeletalMesh`/`StaticMesh` → glTF 2.0) — kept as fallback.
- `work/weapon_base_skin_map.json`: 792 weapons → 202 base mesh families + 590 skins; each
  family has `base`, `base_display_name`, `members[]`. Drives the viewer's catalog + the
  skin editor's base↔skin relationship.
- ColMask system on disk: `Main_ColMask_0Base/1Eyes/2Nose/3Teeth`, `_Diff`, `_Norm`,
  `_Golem_BRDFMask` — the region-mask model the skin compositor must replicate.

## 4. Decisions

- **B1** Location: `tools/content-studio/` (`server/` Python, `web/` Vite). Matches `tools/` convention.
- **B2** psk→web via a **pure-Python `.psk`/`.pskx` → glTF/GLB converter** (backend endpoint +
  reusable module), NOT re-running umodel. Works on extracted+mapped files; deterministic;
  TDD-able; reusable by the ship/export path. umodel `-gltf` is the cross-check oracle.
- **B3** Static-view first: parse PNTS+VTXW+FACE+MATT only; skeleton/weights (REFSKELT/
  RAWWEIGHTS) skipped until a rigged preview is actually needed.
- **B4** Textures: try three.js `TGALoader` client-side first; add a Pillow tga→png backend
  fallback only if the browser path fails. No premature Pillow dep.
- **B5** Skin compositor = three.js `ShaderMaterial` replicating APB ColMask (per-region base
  color from `*_ColMask_*` + tiling pattern from `WeaponSkins` + placed symbol decals from
  `SymbolsBulk` via `DecalGeometry` + `_Norm`/`_BRDFMask`). Slice 2.
- **B6** Export/ship = compose texture set + glTF + a UE5 import manifest matching what
  `tools/scripts/import_*.py` already consumes; MUST NOT mutate existing `Content/Imported`.
  Slice 3.
- **B7** Backend deps pinned in `tools/content-studio/server/requirements.txt`; frontend deps
  in `web/package.json` (exact/pinned). `.venv`, `node_modules`, build output gitignored.

## 5. Vertical slices (the contract — scenarios + proofs)

### Slice 1 — VIEWER (active)
Orbit-view any base weapon mesh in 3D with its material, from the extracted `.psk`.
- **S1 happy**: pick a base weapon → backend converts its `.psk`→GLB → three.js renders in an
  orbit camera. *Proof*: Playwright screenshot shows the mesh; browser console 0 errors;
  scene-graph assert (mesh present, >0 triangles).
- **S1b converter unit (RED first)**: parse the real Magnum clip `.psk` → assert exact
  vertex/wedge/face counts + a valid GLB (magic `glTF`, JSON+BIN chunks, accessor counts match).
- **S2 edge**: a multi-material / larger mesh (full weapon body or a `MorphSafeClothing`
  piece) renders with no missing-texture magenta and correct submesh count.
  *Proof*: screenshot + asserted material/primitive count.
- **Teardown**: kill uvicorn + Vite + Playwright browser; remove any temp GLBs written outside
  a cache dir; leave `Content/` untouched.

### Slice 2 — SKIN / ColMask EDITOR
Change a ColMask region color + place a symbol decal → live 3D update (B5).
- **S3**: edit a region color and drop a `SymbolsBulk` decal → 3D updates live.
  *Proof*: before/after screenshots differ in the expected region (pixel-diff), elsewhere stable.

### Slice 3 — NEW-CONTENT AUTHORING + SHIP/EXPORT
Author/compose new content and export game-ingestable artifacts (B6).
- **S4**: export a composed skin → valid glTF + import manifest that `import_*.py` parses;
  existing `Content/Imported` count UNCHANGED. *Proof*: glTF validates, manifest parses,
  pre/post `Content/Imported` file count identical.

## 6. Verify gates

- Converter: pytest suite green (real-file asserts + GLB structure). `python -m pytest` exit 0.
- Frontend: `npm run build` exit 0; no TS errors.
- Each slice: Playwright run — target screenshot captured, console clean, scene/DOM asserts pass.
- Ship path (Slice 3): `Content/Imported` file-count invariant proven pre/post.

## 7. Risks & mitigations

- **`.pskx` variants / large-vertex chunks** → Slice 1 targets classic `.psk` weapons first;
  add `.pskx` chunk handling when a needed mesh uses it; umodel `-gltf` is the cross-check if a
  file won't parse. AGENTS.md rule 5: >2 failed parse attempts on one file ⇒ `work/` note, move on.
- **TGA in-browser** → B4 fallback (Pillow tga→png) if `TGALoader` fails.
- **Scope is large (3 slices, whole app)** → ship Slice 1 end-to-end and QA'd before Slice 2.
- **Shared worktree** (Qoder's uncommitted M11–M16 domain work) → Track B is all-new files
  under `tools/content-studio/`; never touch `Source/` or `Content/`. No commits without user OK.

## 8. Definition of done (Track B)

Viewer renders any base weapon/vehicle/clothing mesh with skins; ColMask editor composes a
skin live; export produces artifacts `import_*.py` ingests without disturbing existing content;
converter + frontend build + Playwright gates green. Then this file → `work/_archive/`.

## 9. Status log

- 2026-07-21: Plan authored. Stack locked (lean web + Python) via user AskUserQuestion.
  Linchpin (psk→web) resolved: ActorX format confirmed on real file + umodel `-gltf` fallback.
  NEXT: scaffold `tools/content-studio/`, then Slice 1 (converter RED→GREEN → viewer → Playwright).
- 2026-07-22 (3:00): **Slice 1 VIEWER — all gates green.** Picking up a partially-built
  scaffold (server + web from prior sessions), completed the only unmet gate — the §6
  Playwright QA — under `tools/content-studio/` (no Source/Content collision with the
  concurrent Qoder M11–M16 domain edits).
  Gate evidence (all exit 0, verified live 2026-07-22 ~02:47):
    - Converter pytest: `87 passed in 6.86s` (server/.venv, real-file asserts + GLB structure).
    - Frontend build: `tsc --noEmit && vite build` → `✓ built in 4.80s` (no TS errors).
    - Playwright S1 happy: `1 passed (5.9s)` — backend + frontend auto-started via
      `playwright.config.ts webServer[]` (reuseExistingServer), Magnum mesh rendered
      (`data-status="ready"`, `data-triangles>0`), console/pageerror clean, screenshot
      artifact `web/test-results/slice1-viewer.png` (22,362 bytes).
  Files added: `web/playwright.config.ts`, `web/e2e/viewer.spec.ts`, `web/e2e/README.md`;
  `web/package.json` gained `@playwright/test@1.61.1` + `e2e`/`e2e:install` scripts;
  `.gitignore` gained the Playwright artifact dirs.
  NOTE: built browser binaries (chromium) live in `%USERPROFILE%\AppData\Local\ms-playwright`
  (machine-local, gitignored); `npm run e2e:install` reproduces them on a fresh clone.
  NEXT (Slice 2): ColMask region-color + symbol-decal live edit (B5). Pre-req: SymbolsBulk
  decal loading + the skin compositor — keep the converter/viewer slice green as the harness.
