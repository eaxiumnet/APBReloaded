# M3R — Fidelity oracle PS5.1 fix + promotion recipe (2026-08-02)

## Fidelity oracle gate — fixed

`tools/fidelity_oracle_helpers.ps1` used `ConvertFrom-Json -Depth 100` (a PowerShell 7
parameter). The master spine runs child gates under `powershell` 5.1, so the oracle step
FATALed at parse:

```
FIDELITY_ORACLE_FATAL ... A parameter cannot be found that matches parameter name 'Depth'
```

Removed `-Depth` from all 4 call sites (`Get-Json`, `Test-JsonCatalog`,
`Test-UiAnchorTokens`, `Test-ScreenshotSpec`). Behavior-neutral (PS5.1 parses to full
depth; PS7 defaults to 1024). Commit `029885d`. Evidence:
`.omo/evidence/m3r-oracle-ps51-fix-20260802/`.

Verified under powershell 5.1:
- no flag → the intended fail-closed list (4x `ROW_PENDING` + 1x
  `ROW_DEFERRED_REQUIRE_BINARY`), no FATAL
- `-AllowDeferred` → `FIDELITY_ORACLE_PASS rows=20 allow_deferred=True`

**Open policy decision (needs lead/plan sign-off):** the spine (`run_verification_gates.ps1`,
task-22 owned) still fails the oracle step because it does not pass `-AllowDeferred`. The 5
pending rows are: `menu.movies.splash`, `character.morph_fallback`,
`district.social.streamed_asset`, `vehicle.catalog.apbdb` (pending_manual) and
`ui.screenshot.login.fixed_camera` (deferred_require_binary). Either resolve them with real
evidence or wire `-AllowDeferred`; the spine comment says fail-closed until resolved.

## Gate state after the two fixes

| Gate | State |
|---|---|
| m3r_source_registry | PASS (task-2 fix, commit `572178a`) |
| m3r_catalog_provenance | PASS (17 registrations / 51 allowlist / 68 on disk) |
| m3r_fidelity_oracle | no FATAL; fail-closed on 5 pending rows (or PASS with -AllowDeferred) |
| m3r_semantic_parity | PASS (MESH/PLACEMENT + M3R_SEMANTIC_PARITY_PASS, verified=0) |
| m3r_r6 allowlist → strict gate | blocked: 0 verified ledger rows |

## Promotion recipe (next R7 unblock: `no_verified_rows`)

A ledger row becomes `verified` when it carries: `source_locator`, `source_sha256` (64-hex),
`extractor` + `extractor_args`, `intermediate_sha256` (+ optional `intermediate_path`),
`conversion_settings`, `destination`, `class_validation`, and `d17_evidence[]` rows whose
files exist on disk with matching SHA-256. The allowlist generator additionally requires
`dest` to start with `/Game/` (so `data:*` catalog entries are excluded).

Cheapest first batch (all evidence on disk today):
- **2011 MenuArt** — `group:2011/MenuArt` (imported, dest `/Game/Imported/UI/Menu2011`,
  class `UTexture2D`). Source upks now hashable at the fixed root
  `...\Client\APBGame\Content\Interface\APBMenus_*.upk`; 2,085 intermediate PNGs at
  `Content/Extracted/2011/MenuArt/<pkg>/Texture2D/*.png`; extractor
  `tools/scripts/export_2011_menu_art.py`. Needs group→per-asset expansion first
  (`tools/scripts/expand_import_ledger.py` writes the sidecar
  `tools/import_ledger_expanded.json`).
- **MaterialDatabase** — 572 `material/*` entries (dest `/Game/Imported/MaterialDatabase/...`,
  class `UTexture2D`/`UMaterial`/`UMaterialInstanceConstant`), 479 extracted dirs at
  `Content/Extracted/MaterialDatabase/`; source_locator already set to the extracted dir
  (allowed by `Test-SourceLocator`), but a verified row needs the retail package sha256 +
  extractor args + intermediate hash.

Prerequisites: ledger edits are task-3 owned, allowlist/task-16 owned, and the plan's
closure order requires semantic parity first (already passing via
`validate_m3r_semantic_parity.py`).
