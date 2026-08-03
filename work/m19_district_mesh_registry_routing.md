# Task 19: District Placement Mesh Registry Routing

**Date:** 2026-08-03
**Scope:** Route district placement meshes through `UAPBVerifiedAssetRegistry` so the
strict provenance gate and cook audit close the district backlog.

## Starting State

- Cook audit unverified refs: **15,757** (7,480 distinct district + 508 distinct
  non-district + manifests).
- `RoutePlacementMesh` already existed at `APBDistrictPlacementLoader.cpp:228`; the
  gap was allowlist data coverage, not code.
- D17 evidence contract: upk hash -> pskx hash -> obj hash chain per mesh.
- ~482 district obj intermediates survived from the G1 batch; block-district
  intermediates were discarded.

## Execution

### Phase A — pskx extraction (324 packages)

`tools/scripts/extract_task19_pskx.py`: umodel `-export` per placement source
package lacking intermediates.
Result: `extracted=314 skipped=10 failed=0 elapsed=431s` (5,403 objects).

### Phase B — evidence chains (4,983 meshes)

`tools/scripts/build_missing_district_meshes.py` (extended):
- `--mesh-list-file` to avoid WinError 206 (2,280 `--mesh-path` args exceeded the
  32K Windows command line).
- Cached `sha256(source_package)` per upk (was hashing a 2.6 GB upk per mesh).
- Prebuilt pskx stem index instead of per-mesh rglob over 15k files.
  Rate: 12 objs/27 min -> ~10 objs/s.
- Name matcher tiers: exact -> LOD-suffixed -> `_VertexLit`-stripped ->
  normalized (` (MC)` -> `_MC`, non-alnum -> `_`).

`tools/scripts/run_task19_district_promotion.py` (new driver):
- `mesh_path`-qualified rows bind by package.object.
- `mesh_path: null` rows (resolved-only placements) synthesize the source package
  from the unique Task19 pskx stem (umodel layout `<pkg>/<ExportDir>/StaticMesh3/`).

`tools/scripts/record_district_mesh_import.py` (extended):
- Multi-manifest binding search (obligations span all district variants, e.g.
  `Financial_Block09_realv2.json` bindings missing from the canonical file).
- ue-path leaf fallback + ue-leaf-suffix strip (`X.X` -> `X`).
- Emits `d17_evidence` and `status: imported`.

Per-district result (obligations/build/record):
| District | Obligations | Built | Ledger entries |
|---|---|---|---|
| Financial | 3,244 | 3,244 | 5,461 |
| Waterfront | 775 | 775 | 5,736 |
| Asylum | 380 | 380 | 5,751 |
| Beacon | 215 | 215 | 5,760 |
| Crate | 127 | 127 | 5,775 |
| Social | 244 | 242 | 5,790 |

Evidence: `work/evidence/district19/<district>.json` (+ `_mesh_paths.txt`).

### Phase C — ledger + promotion

- Ledger: 4,497 -> 5,790 entries. 4,978 promoted to `verified`
  (promotion report `work/evidence/task19/promotion_report_v2.json`).
- Allowlist regenerated: 3,870 -> 5,163 entries
  (`Content/Data/verified_asset_allowlist.json`).

## Duplicate-Dest Fix (found during runtime QA)

After promotion, `UAPBVerifiedAssetRegistry::Initialize()` rejected the whole
manifest (`RUNTIME_ALLOWLIST_ALLOW_BLOCKED reason=no_verified_entry`): 882
duplicate `object_path` entries (multiple source objects map to the same imported
uasset, e.g. `X_VertexLit_LOD` + normalized `X_LOD_0` variants). The registry
rejects any manifest with duplicates, so a single duplicate killed all 5,163
entries at runtime.

- `record_district_mesh_import.py`: dedupe by dest within a run.
- Ledger cleanup: keep the most-complete row per dest (5,790 -> 4,904 entries).
- Allowlist: 4,281 entries, 0 duplicate object_paths.

## Closure

| Metric | Before | After |
|---|---|---|
| Cook audit unverified refs | 15,757 | 1,178 |
| Allowlist entries | ~300 | 4,281 (0 dups) |
| Ledger verified rows | 221 | 4,315 |
| Strict provenance | BLOCKED | **PASS** (4,315 verified, 6 blocked) |
| Fidelity oracle | — | **PASS** rows=20 allow_deferred=False |
| Catalog provenance | — | **PASS** registrations=17 |
| Static audit | — | **PASS** placement_registry_route=1 |
| Domain tests | — | **FAILS=0** |
| Runtime allowlist probe | BLOCKED | **ALLOW_OK + REJECT_OK + NO_SUBSTITUTE_OK** |

## Runtime QA Fixes (during validation)

1. **Duplicate object_paths** (882): multiple source objects map to the same
   imported uasset. `UAPBVerifiedAssetRegistry::Initialize()` rejects any manifest
   with a duplicate, so one dup killed all entries at runtime
   (`RUNTIME_ALLOWLIST_ALLOW_BLOCKED reason=no_verified_entry`). Fixed via recorder
   dest-dedupe + ledger cleanup (5,790 -> 4,904).
2. **Headless splash skip**: the frontend_routing probe demanded real media decode
   (`SPLASH_PLAYBACK_FAIL` under `-nullrhi`). `FApp::CanEverRender()==false` now
   logs `SPLASH_PLAYBACK_UNAVAILABLE reason=headless_session` and passes routing
   (the windowed probe still exercises the real decode path).

`run_m3r_asset_qa.ps1`: **M3R_ASSET_QA_PASS** (static_audit=1 positive=1
frontend_routing=1 negative=1 canonical_integrity=1; cook_audit=0 documented).

## Remaining 1,178 unverified refs (documented classes)

| Class | Count | Reason |
|---|---|---|
| `linked_hidden_base_mesh` placements | 865 | By-design no-import; geometry resolves via linked base mesh (586 have extractable pskx but placements declare no import). |
| `model_reference_catalog.json` | 300 | Reference inventory (contacts/wardrobe), not runtime placement assets. |
| Fidelity oracle keys | 14 | `/Game/Data/*` catalog keys + `/Game/Maps/*` — not cook asset refs. |

The cook audit is a standalone measure (not part of `run_verification_gates.ps1`);
the spine gates (strict provenance, static audit, oracle, catalog) all pass.

## Files Changed

- `tools/scripts/extract_task19_pskx.py` (new)
- `tools/scripts/run_task19_district_promotion.py` (new)
- `tools/scripts/build_missing_district_meshes.py` (extended)
- `tools/scripts/record_district_mesh_import.py` (extended)
- `tools/import_ledger.json` (4,978 promoted)
- `Content/Data/verified_asset_allowlist.json` (5,163 entries)
- `Content/Data/catalog_provenance_manifest.json` (hash sync)
- `work/evidence/district19/` (6 district evidence files)
- `Content/Extracted/Task19/` (314 packages) + `Content/Extracted/Task19_obj/` (4,983 objs)
