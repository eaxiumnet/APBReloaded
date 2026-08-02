# M3R spine pass/block record — 2026-08-02

Spine: `tools/run_verification_gates.ps1` (fail-fast, powershell 5.1).
Run after: task-2 source registry fix, oracle helpers PS5.1 fix, task-17 registry
crash fix, r6 gate PS5.1 fix.

## Full run result

Spine stops at `m3r_fidelity_oracle` (fail-fast, by design):

```
financial_manifest_gate  OK
m3r_source_registry      PASS roots=4 callers=3 reader=umodel_64.exe
m3r_catalog_provenance   PASS registrations=17 allowlist=51 on_disk=68
m3r_fidelity_oracle      BLOCK (5 pending rows, no -AllowDeferred)
exit 1
```

Evidence: `work/logs/m3r_spine_*.terminal.log`, `.omo/evidence/spine-runs/`.

## Per-step pass/block table (M3R section)

| Spine step | Result | Evidence |
|---|---|---|
| financial_manifest_gate | PASS | spine run |
| m3r_source_registry | PASS | spine run |
| m3r_catalog_provenance | PASS | spine run |
| m3r_fidelity_oracle | BLOCK | 5 rows pending_manual / deferred_require_binary |
| m3r_semantic_parity | PASS | verified=0 runtime_eligible=0 |
| m3r_r6_asset_allowlist | PASS (entries=0) | `VERIFIED_ASSET_ALLOWLIST_PASS` |
| m3r_static_asset_audit | PASS | loads=5 files=166 registry_owner=1 route=1 |
| m3r_r6_editor_build | PASS | task-17 session, 58s |
| m3r_strict_asset_provenance | BLOCK | entries=659 verified=0 blocked=6 no_verified_rows |
| bind_report | PASS | financial rate 1.0, Social 763/763 |
| domain_tests_build/tests | not run | no Domain code touched this session |
| model_registry, editor_build, host_client_loop, playable, m8+ | not reached | post-M3R steps |

## Fixes landed this session

1. `tools/promote_verified_assets.ps1` — `$ProjectRoot` default crashed at
   param-bind time under PS5.1 `[CmdletBinding()]` scripts (`$PSScriptRoot`
   empty). Now resolves in body. Commit `7e5c399`.
2. `tools/check_verified_asset_static_audit.ps1` — same crash + BOM'd evidence
   JSON (broke python consumers). Now BOM-less `WriteAllText`. Commit `7e5c399`.

Both verified under the spine's exact invocation style (PS5.1, no `-ProjectRoot`).
The allowlist generator also runs clean end-to-end: `CATALOG_ALLOWLIST_HASH_SYNC`
+ `VERIFIED_ASSET_ALLOWLIST_PASS entries=0`; manifest/allowlist byte-identical
to HEAD (no hash churn — allowlist was already empty).

## Remaining blockers to full M3R pass

1. **m3r_fidelity_oracle** — 5 rows need resolution (splash movie, morph
   fallback, social streamed asset, apbdb vehicle catalog, login screenshot) or
   the spine needs `-AllowDeferred` (task-22 owned; oracle gate itself passes
   with the flag: `FIDELITY_ORACLE_PASS rows=20`).
2. **m3r_strict_asset_provenance** — 0 verified ledger rows. Promotion pipeline
   is now fully executable; next step is producing `verified` rows with D17
   evidence (source_locator + hash + destination), then re-running
   `promote_verified_assets.ps1` to populate the allowlist.

## Notes

- `promote_verified_assets.ps1` is task-16 owned; this fix is an infra commit,
  the task-16 delta (verified rows + populated allowlist) is still pending.
- Root cause of the param-default crash also exists in
  `test_validate_fidelity_oracle.ps1` / `validate_fidelity_oracle.ps1`, but
  those lack `[CmdletBinding()]` so PS5.1 binds them fine (verified working).
  Left untouched.
