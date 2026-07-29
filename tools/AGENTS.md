# Tools Scope

## Overview

Local verification, extraction/import, catalog-generation, and server-operation tooling.
Large bundled/reference tools are not part of the APBReloaded runtime.

## Where To Look

| Concern | Location |
|---|---|
| Full runtime gate | `run_verification_gates.ps1` |
| M6 world-server gate | `run_m6_world_gate.ps1` |
| Asset status source | `import_ledger.json` |
| Ledger report generation | `scripts/build_import_status.py` |
| Server preflight/start | `scripts/bootstrap_server.ps1`, `start_world.ps1`, `start_district.ps1` |
| APB data extraction/conversion | `scripts/`, `convert/`, `apbdb/` |
| Third-party/reference code | `UEViewer/`, `apb_sdk_ref/`, `content-studio/` |

## Conventions

- Prefer a script's `-DryRun` mode for server/bootstrap validation before launching
  processes or changing generated state.
- Keep source roots, destination roots, and port overrides parameterized; defaults may
  reflect this workstation but scripts must report resolved paths clearly.
- `import_ledger.json` is the source of truth for extracted/imported/bound/manual asset
  status; regenerate `work/IMPORT_STATUS.md` through the existing script.
- Preserve source provenance and deterministic output names for generated catalog files.
- Gate scripts are fail-fast and must retain their terminal success markers because
  roadmap evidence depends on them.

## Anti-Patterns

- Editing generated `APBPrivateServerOpcodes.h` values by hand.
- Treating `apb_sdk_ref` offsets, kernel components, or the old SDK as runtime-authoritative.
- Claiming current retail protocol fidelity from incomplete conversion/protocol tools.
- Packaging `Content/Extracted` or tool caches into the game.
- Modifying embedded third-party repositories during unrelated port work.
- Placing server-operation scripts at `tools/`; their canonical home is `tools/scripts/`.

## Verification

Run the narrow script in dry-run or validation mode first, inspect generated JSON with a
real parser, then run the affected gate. Do not launch long-lived server/editor processes
without arranging a bounded exit condition and retaining their logs under `work/`.
