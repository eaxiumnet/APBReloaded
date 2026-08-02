# M3R Task 2 — Source registry 2011 root fix (2026-08-02)

## What was wrong

`tools/check_source_registry.ps1` failed the master spine's first M3R gate:

```
SOURCE_REGISTRY_FAIL canonical_missing alias=ref_2011
  path=D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America
```

The `ref_2011` canonical root in `tools/source_registry.json` pointed at a path that no
longer exists on disk. The 2011 RTW install was relocated: the game content now lives under
`D:\APBReloaded\2011 apb\APB All Points Bulletin\Client` (the installer/launcher layout
sits above it). The 2026-08-01 frontend forensics pass already consumed the menu packages
from `Client\APBGame\Content\Interface\APBMenus_*.upk`.

## What changed

`tools/source_registry.json` (commit `572178a`, M3R task-2 delta):

- `ref_2011.path` → `D:\APBReloaded\2011 apb\APB All Points Bulletin\Client`
- added `ref_2011.packages_subpath` → `APBGame\Content`
- added a relocation note (menu packages live in `APBGame\Content\Interface`, pkg 547/31)
- `ref_2011_archive` untouched (`Client\Client1.1.0.534979.7z` verified present,
  still `quarantined_candidate`)

## Verification (same shell the spine uses: powershell 5.1)

```
SOURCE_REGISTRY_PASS roots=4 callers=3 reader=D:\APBReloaded\tools\UEViewer\umodel_64.exe
CATALOG_PROVENANCE_PASS registrations=17 allowlist=51 on_disk=68
```

The reader probe ran the patched umodel fork against a retail package (`-list`) and passed.
Evidence: `.omo/evidence/m3r-task2-20260802/` (gate-verification.json, delta.patch,
commit.json). Commit helper output: `TASK_DELTA_COMMIT_PASS commit=572178a2...`, single
owned path, pre-existing dirty worktree (14,409 entries) preserved.

## Remaining R7 blockers (unchanged, in gate order)

1. **Fidelity oracle** — `tools/fidelity_oracle_helpers.ps1` uses `ConvertFrom-Json
   -Depth` (PS7-only), so the spine's `powershell` 5.1 invocation FATALs at parse. Under
   `pwsh` it instead fails `ROW_PENDING`/`ROW_DEFERRED_REQUIRE_BINARY` because the spine
   does not pass `-AllowDeferred`. Touches task-4 (validator) and task-22 (spine) owned
   paths.
2. **0 verified rows** — ledger has 659 entries (81 imported, 572 extracted, 6
   blocked_source), none `verified`; strict provenance exits 1 with `no_verified_rows`
   and the allowlist stays empty. Promotion (task 16) needs per-row D17 evidence
   (source_locator + source_sha256 + extractor + extractor_args + intermediate_sha256 +
   conversion + destination + class_validation + d17_evidence files).
3. **Canonicalization manifest (task 6)** — `Content/Data/fidelity/
   canonicalization_manifest.json` absent.
4. **Semantic validators (tasks 11–15)** — `validate_{mesh,placement,texture,material,
   media,animation}_semantics.py` + `validate_ui_visual.py` absent; the spine's semantic
   parity step currently relies on `validate_m3r_semantic_parity.py`.
