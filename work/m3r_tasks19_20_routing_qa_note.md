# M3R task 19 + 20 — district routing layer + asset QA tooling (2026-08-02)

## Task 19 — verified district asset routing (commit `ff73d9e`)

`Source/APBReloaded/Systems/District/APBVerifiedDistrictAssetRouting.h/.cpp` (new):
fail-closed routing layer for district, character, vehicle, weapon, and
interactable loads. Every route resolves through `UAPBVerifiedAssetRegistry`
typed loaders with domain-tagged stable reasons (`DISTRICT_ROUTE_REJECT
domain=… reason=…`); a denied path never substitutes.

Wired consumers (all previously called the registry directly):
- `APBDistrictPlacementLoader.cpp` — `LoadPlacementMesh` now delegates to
  `RoutePlacementMesh` (BuildPlacementBinding stays the district identity gate;
  forbidden-basic-shape guard preserved; same `PLACEMENT_MESH_BINDING_FAIL`
  log markers).
- `APBFreeroamCharacter.cpp` — faction visual + wardrobe loads via
  `RouteFactionVisualMesh`/`RouteWardrobeMesh`.
- `APBFreeroamGameMode.cpp` — hero landmark via `RouteHeroLandmarkMesh`.

Verification: `APBReloadedEditor` Win64 Development `Result: Succeeded`
(twice, incl. after review fixes). `VERIFIED_ASSET_STATIC_AUDIT_PASS
loads=5 files=168` — no new dynamic-load tokens; the audit's placement-route
pattern now asserts the routing contract.

## Task 20 — static cook audit + QA orchestrator (commit `74b9f31`)

`tools/check_static_cook_assets.ps1` (new): scans `Content/Data/*.json`
catalogs for `/Game/` references and checks each against the allowlist
(exact object path, dot-split package, and package-prefix match) plus an
engine-internal whitelist (`/Engine/BasicShapes` is explicitly NOT allowed).
Emits `STATIC_COOK_ASSET_AUDIT_PASS`/`FAIL` with evidence JSON.

`tools/run_m3r_asset_qa.ps1` (new): orchestrates static audit → cook audit →
positive probe (`ALLOW_OK`/`REJECT_OK`/`NO_SUBSTITUTE_OK`) → negative probe
against a scratch allowlist copy via `-APBAllowlistOverride` →
canonical-allowlist integrity check. Emits `M3R_ASSET_QA_PASS` + evidence.

Registry (`APBVerifiedAssetRegistry.cpp/.h`): `-APBAllowlistOverride=<path>`
loads a test allowlist and skips the provenance-bound hash check. Pointing it
at the canonical allowlist is ignored (`OVERRIDE_IGNORED`, provenance stays
bound) — verified at runtime with `FPaths::IsSamePath` after the first
`NormalizeFilename` comparison misfired on relative-vs-absolute paths.

## Verification (final binary)

- `M3R_ASSET_QA_PASS static_audit=1 cook_audit=0 positive=1 negative=1 canonical_integrity=1`
- Negative probe: `RUNTIME_ALLOWLIST_ALLOW_BLOCKED reason=no_verified_entry`
  + `REJECT_OK` + `NO_SUBSTITUTE_OK` against the empty override copy.
- Override-equals-canonical control: `OVERRIDE_IGNORED`, `override=0`, ALLOW_OK.
- `STRICT_ASSET_PROVENANCE_PASS entries=659 verified=26 blocked=6` (no regression).
- Cook audit is an informative FAIL today: 15,790 catalog refs, 26 allowlisted,
  15,764 unverified — the remaining frontend/district refs wait on task 18
  (frontend routing) + further verified batches.

## Notes

- Review fixes applied: override-vs-canonical provenance bypass hardened,
  cook-audit package-prefix matching added, three dead
  `APBVerifiedAssetRegistry.h` includes removed.
- Evidence: `.omo/evidence/m3r_asset_qa/`, `.omo/evidence/spine-runs/fixed/override_control/`.
