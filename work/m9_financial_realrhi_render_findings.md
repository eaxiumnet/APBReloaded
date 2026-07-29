# M9 Financial — Real-RHI Render Proof + Launch Findings

Date: 2026-07-27 · Author: Sisyphus · Status: evidence note (non-colliding, standalone)

> First **real-RHI** (GPU, non-`nullrhi`) boot proof for a playable district. Every prior
> district verdict in `_active.md` is a headless `-nullrhi` gate run; this closes the
> "does it actually render on a GPU" gap for `Lvl_APB_Financial_Freeroam`. Purely additive
> probe change; no gate marker moved.

## What is PROVEN (AMD RX 7900 XTX / D3D12, windowed 1280x720)

| Element | Marker (log: `%LOCALAPPDATA%\Temp\opencode\apb_capture\editor_capture3.log`) |
|---|---|
| Freeroam gamemode active | `Game class is 'APBFreeroamGameMode'` |
| Real streamed load (not void) | `Took 5.708961 seconds to LoadMap` |
| District geometry streamed | `STREAM_SPAWN n=2016 ... total_placements=2345 hit=1.00 in_radius=2016 load_failed=0` |
| Full lighting | `DISTRICT_LIGHT sun=1 sky=1 atmos=1 fog=1 ppv=1` |
| Player spawns + grounded + walkable | `PLAYABLE_SETTLED at=(140031.8,130712.4,548.4) mode=1` (Walking) |
| HUD wired | `HUDClass=AAPBFreeroamHUD` -> `AddToViewport(10)` (pixel proof = M11 `hud_capture_final.png`) |
| Rendered frame | `financial_render3.png` (374 KB) + `PLAYABLE_CAPTURE_DONE` |
| No crash | `Error/Fatal/Assertion count = 0`; process self-exited; 0 orphaned editors |

Regression floor intact same session: `gate=PASS`, 18 domain suites `FAILS=0`,
`APBReloaded.exe` + `UnrealEditor-APBReloaded.dll` build `Result: Succeeded` exit 0.

## THE GAP — content fidelity (owned by M8/M9/M10 content track, NOT a render bug)

Looking at the actual pixels: the frame is **flat-red untextured geometry**, not a textured
cityscape. The **geometry is genuinely 1:1** (2016/2016 in-radius placements bound to real
imported `UStaticMesh`, zero cubes, `load_failed=0`) but **the meshes have no real APB
materials**. Root cause, code-confirmed:

- `Systems/District/APBDistrictPlacementLoader.cpp` `LoadPlacementMesh` (L141-164) loads real
  imported meshes from `/Game/Imported/Districts/...` (skips engine cubes).
- `EnsureVisibleMeshMaterials` (L167-202) then **force-overrides every material slot** with
  `LevelColorationUnlitMaterial`. Its own comment: *"umodel imports only ship WorldGrid slots
  — under black sky + low ambient they look pure black."* The real APB materials/textures were
  never imported — this is the material-import pipeline gap (see `material_mapping_status.md`,
  `mesh_material_bindings.json`).

Because `LevelColoration` colors per-streaming-level and all placements land in one level, the
whole scene renders as a single uniform red — camera framing cannot fix this; only real
materials will. **Verdict: render+stream+light+spawn+walk+HUD proven; textured appearance is
blocked upstream on material import.**

## Launch gotchas (both cost real time — record so nobody re-derives them)

1. **Do NOT boot the standalone `APBReloaded.exe` (Game target) in this env.** It tries to
   reach a Zen storage server (`APBReloaded.44393c44/oplog/Windows` -> `maras.fibia.local:8558`,
   `10.128.234.36:8558`) and dies `Failed to initialize connection`. **Use
   `UnrealEditor.exe -game`**, which reads loose cooked/uncooked content straight from
   `Content/` on disk.
2. **Bare `-game` on the freeroam map misroutes to `APBFrontendGameMode`** (0.338s void
   LoadMap, pawn free-falls to Z=-690). A URL game option outranks both the map prefix rule and
   `WorldSettings.DefaultGameMode`, so pass it explicitly:
   `?game=/Script/APBReloaded.APBFreeroamGameMode`.

Full working launch line:

```
UnrealEditor.exe <proj> \
  "/Game/Maps/Lvl_APB_Financial_Freeroam?game=/Script/APBReloaded.APBFreeroamGameMode" \
  -game -windowed -ResX=1280 -ResY=720 \
  -APBProbe=playable -APBCapture=<png> -APBScratch=<dir> \
  -nosplash -nosound -unattended -log -abslog=<log>
```

## Probe change (additive, gated OFF by default — no gate marker affected)

`Systems/APBSessionProbeSubsystem.cpp` phase-20 capture path: new `-APBCapture=<png>` flag
(`APBCaptureWanted()` helper ~L527). When set, spawns a throwaway elevated `ACameraActor`
(focus + `(-9000,-9000,7000)` aimed at pawn), `SetViewTarget`, `FScreenshotRequest` at frame
100 & 250, `RequestExit` at frame >=300. Added includes `UnrealClient.h`,
`Camera/CameraActor.h`. **No gate script passes `-APBCapture`**, so all green markers are
byte-for-byte unchanged; this is an on-demand manual-QA tool only.

---

# M9 Financial — Material Binding Coverage Fix (name-normalization)

Date: 2026-07-28 · Author: Sisyphus · Status: coverage fix landed + assigned + rendered

## TL;DR

The flat-red gap above had TWO layers. The material *import* pipeline was later built
(MICs created, `M_APBMaster` parented). But the **mesh→material binding** only covered
**750 / 1711** placed Financial meshes, so most geometry still fell back to untextured.
Root cause was a **name-format mismatch**, not a dump/import gap. Fixed editor-free via
three normalization rules → **1698 / 1711 = 99.2%** placed-mesh coverage, then baked
onto assets in-editor (**ok=3002, fail=0**) and re-rendered on GPU (red reduced to a
single residual patch; newly-covered MICs proven to carry real diffuse+normal textures).

## Root cause — three-part identity-gate mismatch in `mesh_material_binding_parser.py`

`build_document()` gated each chunk's `StaticMesh3 ObjectName` by exact `(district, name)`
equality against the on-disk `.uasset` stem. Three format divergences dropped 961 meshes:

1. **Filename sanitization.** umodel ObjectName `High-street (shop) (MC)_0001_LOD_0` vs
   on-disk stem `High-street_shop_MC_0001_LOD_0`. The import tool maps space→`_` and strips
   `(` `)`. (+395)
2. **Cross-district gate.** `WD_*`-named meshes placed in Financial live in Waterfront chunks;
   the `(district, name)` key blocked a name that exists under a different chunk district.
   Match name-only across districts, emit under the on-disk district. (+206)
3. **`ROAD_` prefix.** Placement/import stem `ROAD_Road_Tile_*` vs bare chunk ObjectName
   `Road_Tile_*`. Emit both candidates; first on-disk hit wins. (+347)

Verified via placement `mesh_id` (real key) ∩ chunk ObjectNames ∩ on-disk stems:
all 1711 placed meshes exist on disk (import was complete); only the binding matcher was lossy.

## Fix (editor-free tooling)

- `tools/scripts/mesh_material_binding_parser.py`: added `sanitize_object_name()` +
  `disk_candidates()`; rewrote `build_document()` to match chunk ObjectName → on-disk stem via
  candidates and emit the real `(district, stem)` (downstream `ue_asset` path stays valid).
- `tools/scripts/build_mesh_material_bindings.py`: `imported_stems()` → `imported_index()`
  returning `dict[stem, [districts]]`, glob widened `*_LOD_0.uasset` → `*_LOD_0*.uasset`.

## Evidence (commands + markers, 2026-07-28)

| Step | Command | Result |
|---|---|---|
| Reconcile | `python tools\scripts\build_mesh_material_bindings.py --reconcile` | `RECONCILE meshes=3044 slots=11443 resolved=11436 (99.94%)`; Financial meshes 1232→2533; placed coverage **750→1698 / 1711 (99.2%)** |
| Manifest | `python tools\scripts\build_district_assignment_manifest.py` | `meshes=3044 texture_mic=11198 color_mic=243 unresolved=2` |
| Assign (gated editor) | `assign_district_materials.py` via `UnrealEditor-Cmd -run=pythonscript -nullrhi` | log `ASSIGN DONE ok=3002 fail=0 no_change=42 color_pending=243` |
| MIC content proof | `apb_mat_diag2.py` (gated) | newly-covered shells carry real textures: `MI_Conc_GreyLight`→`Concrete_01_DiffSpec`+`Concrete_003_Norm`; `MI_Stucco_M12_BlueDark`→`Stucco_DiffSpec`+`Stucco_Norm`; `MI_BandV2_Conc_Grey_1PNew`→`Concrete_01_DiffSpec`+`BandV_Norm`; all parent=`M_APBMaster` |
| Real-RHI render | freeroam launch line above, `-APBCapture=...apb_tex_proof2\financial_textured2.png` | `PLAYABLE_CAPTURE_DONE`; PNG 970 KB (vs flat-red baseline `financial_render3.png` 374 KB); flat-red reduced to one residual patch |

## Residual (13 / 1711 = 0.8%, OUT OF SCOPE for this fix)

- 5 `PGBD_B09_*` = **Beacon-district** meshes placed in Financial; Beacon packages were never
  dumped, so no chunk ObjectName exists. Needs a Beacon umodel dump.
- 8 `FD_B3x_ApSalesB0x_VertexLit_LOD_0_` (trailing underscore) = present on disk but absent
  from every dumped chunk; needs a targeted re-dump + trailing-`_` dedup.

## Honest verdict

Coverage mechanics and asset chain are **proven** (numbers + MIC texture params + red
elimination on GPU). Visible per-surface *texture detail* is only weakly assessable from the
current steep top-down capture angle, and many APB Financial shells are genuinely grey
concrete/stucco — so muted surfaces are largely correct 1:1 materials, not placeholders. A
ground-level eye-height capture pass is the remaining acceptance gate for subjective fidelity.
