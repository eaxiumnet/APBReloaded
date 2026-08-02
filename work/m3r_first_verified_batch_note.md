# M3R first verified ledger batch — 26 static meshes (2026-08-02)

## Result

- Strict gate: `STRICT_ASSET_PROVENANCE_PASS entries=659 verified=26 blocked=6`
  (was `FAIL verified=0 no_verified_rows`).
- R6 probe: `RUNTIME_ALLOWLIST_ALLOW_OK` (was `ALLOW_BLOCKED no_verified_entry`),
  plus `REJECT_OK` and `NO_SUBSTITUTE_OK`, exit 0.
- Allowlist: `VERIFIED_ASSET_ALLOWLIST_PASS entries=26` with
  `CATALOG_ALLOWLIST_HASH_SYNC hash=912773f0…` (manifest registration updated).

## Batch

26 `StaticMesh` rows from the g1 payload batch 2 (retail MaterialDatabase
packages: Enforcer_Office_Clutter, FD_Lanterns, Generic_Foliage, LitterDecals,
PortableSpotlight), destination `/Game/Imported/Districts/Financial/G1PayloadBatch2/…`.
All have full D17 evidence (`apb_mesh_payload_batch_v1` exact + gltf receipts),
on-disk intermediates hash-matching, and 26 imported uassets.

## New tooling

`tools/scripts/promote_verified_batch.py` — the atomic promotion tool the plan
mandates ("only an atomic promotion tool may set verified"). It mirrors every
verified-row check from `check_strict_asset_provenance.ps1` and the allowlist
eligibility checks from `promote_verified_assets.ps1` (destination under /Game/,
asset_class, allowed source_build, locator test). Dry-run by default;
`--apply` is idempotent and byte-stable. 12 unit/integration tests in
`tools/scripts/test_promote_verified_batch.py` (all pass).

## What stayed unverified

- 5 map extraction rows pass the strict gate but have `Content/Extracted/…`
  destinations — not allowlist-eligible; left unverified.
- 43 partial rows (intermediate hash mismatch, missing fields) rejected.
- 633 rows total rejected.

## Commit scope note

HEAD's `tools/import_ledger.json` was the old v1 format. The working tree
carried an uncommitted v2 expansion + D17 reconcile (74 evidence rows) from
prior sessions; this promotion is the top layer of that state. The promotion
commit lands the full coherent chain: v2 ledger + evidence + 26 verified rows +
generated allowlist + synced manifest + new tooling. Without it the passing
gates are not reproducible from HEAD. `catalog_provenance_manifest.json` and
`verified_asset_allowlist.json` were previously untracked and become tracked.

## Evidence

- `.omo/evidence/m3r-first-verified-batch/evidence.json`
- `.omo/evidence/m3r-first-verified-batch/probe/asset_allowlist.log`
- `.omo/evidence/m3r-first-verified-batch/ledger_promoted_temp.json`
