# Financial District Placement Promotion — Evidence

Baseline: `58754f9` on `main`. Scope: promote the Financial placement manifest from a
single-block sample to the full 44-block district, and gate it against regression.

## What Changed

| Metric | Before (Block09 sample) | After (full district) |
|---|---|---|
| Source packages | 1 | 44 |
| Total manifest rows | 137 | 3565 |
| Source-visible placements | 57 | 1457 |
| Geometry-bound | 57 | 1457 |
| Geometry-missing | 0 | 0 |
| Distinct `ue_path` | 57 | 1457 |
| Spawned in stream radius (runtime) | ~57 | 1258 |

Every one of the 1457 source-visible placements binds to its own distinct mesh path.
`geometry_missing_count` is 0, so no building silently falls back to a placeholder.

## Spawn-Z Defect Found And Fixed

The generator derived `player_start` as per-block `max(z) + 250`, which on the
district-wide set resolves to **Z = 10650** — above every rooftop. The pawn spawned
~10.4 km in the air and free-fell on district entry.

`AlignPlayerStartsAndTeleport` cannot rescue this: `FMath::Max(At.Z, 120.f)`
(`APBFreeroamGameMode.cpp:264`) only ever raises Z, never lowers it.

Fix: ground the spawn at the 10th-percentile Z of placements within the stream radius of
the district centroid, plus 250 cm clearance → **Z = 250**.

Measured consequence — the naive spawn also streams *fewer* buildings, because
`SpawnFromManifestNearEx` measures 3D distance:

| Spawn derivation | player_start Z | Placements streamed |
|---|---|---|
| `max(z) + 250` (naive) | 10650.02 | 1232 |
| 10th-percentile ground + 250 | 250.00 | **1258** |

## Gates Added

`tools/scripts/test_financial_district_manifest_gate.py` — 10 gates, all PASS:

```
GATE_DISTRICT_IDENTITY / SOURCE_ID_UNIQUE / GEOMETRY_PROVENANCE_COUNTS / ROW_IDENTITY
GATE_UE_PATH_DISTINCT / ROW_CONSERVATION / NO_RAMP / SPAWN_POINTS_DERIVED
GATE_SPAWN_GROUNDED / EXTRACTOR_PARITY
IN_RADIUS_GROUNDED=1258 IN_RADIUS_NAIVE_MAX_Z=1232
EXTRACTED_ROWS=3565 MANIFEST_ROWS=3565
```

`GATE_EXTRACTOR_PARITY` re-reads all 44 retail packages and re-derives 3565 rows, so the
manifest is proven a faithful dump rather than a hand-authored file.

Gate teeth verified by mutation — 3 tested, 0 uncaught:

| Mutation | Caught by |
|---|---|
| Spawn reverted to `max(z)+250` | `player_start must equal the grounded district derivation` |
| One renderable row dropped, all counters self-consistently adjusted | `source_visible=1456 expected=1457` |
| Two buildings collapsed onto one `ue_path`, total held at 1457 | `distinct ue_path=1456 expected=1457` |

The third mutation is caught *only* by the `len(distinct_paths) == len(bound)` assertion,
which is why that seemingly-redundant check is retained.

Manifest ownership after the split:

| File | Consumer | Rows |
|---|---|---|
| `Financial_Block09_realv2.json` | runtime loader + district gate | 3565 (1457 bound) |
| `Financial_Block09_unit.json` | `test_block09_manifest_gate.py` | 137 (57 bound) |

The Block09 gate and `test_block09_geometry_resolve.py` were repointed at
`Financial_Block09_unit.json` so they keep asserting their original 57-row constants after
promotion, and `write_block09_real_manifest.py`'s default `--output` was moved off the
production path so a re-run can no longer clobber the district manifest.

Final state — all three gates green:

```
test_block09_manifest_gate.py             exit=0  7 GATE_* PASS
test_block09_geometry_resolve.py          exit=0  FAILS=0 (per-mesh sibling-bind check)
test_financial_district_manifest_gate.py  exit=0  10 GATE_* PASS
```

## Runtime QA

`UnrealEditor.exe -game` on `Lvl_APB_Financial_Freeroam?game=/Script/APBReloaded.APBFreeroamGameMode`:

```
MAP=Lvl_APB_Financial_Freeroam DISTRICT=Financial
STREAM_SPAWN district=Financial package=FinancialDistrict_AllBlocks spawned=1258 total=1457
            radius=60000 player_start=(124665,127632,250) in_radius=1258 load_failed=0
MESH_LOAD   attempted_in_radius=1258 spawned=1258 failed=0 skipped_dup=0
PLAYER_ALIGN at=(124665,127632,250) player_starts=3
```

Runtime `in_radius=1258` matches the gate's static prediction exactly. `load_failed=0`
and `skipped_dup=0`. Player Z held at 250 across the full 306 s session with **zero**
KillZ or out-of-world events. Screenshot confirms a street-level view among buildings,
not a downward view from altitude.

## Pre-Existing Breakage — Observed, Not Fixed

Not caused by this work (which touched only `.json` and `.py` files) and left alone per
the contract on unrelated concerns and concurrent work:

| Item | Evidence it predates this work |
|---|---|
| `APBCatalog.cpp:1` IWYU error — `#include <cstdlib>` before own header | `git blame` → `a25af477`, 2026-07-19; file clean in worktree |
| `APBTicket.cpp:73` C4530 `/EHsc` error | File is ` M` from concurrent work |
| `TestTicketPayloadEscaped` failing in the "C2 RED suite" | Absent from `HEAD`, present on disk — uncommitted RED test |
| Buildings render flat red — `M_APBMaster` compile failure (`Sampler type is Normal, should be Color`) | Asset is `??` untracked, mtime 2026-07-28 15:09; manifest holds no material references |

Both C++ errors surface only because the dirty worktree flips the module into adaptive
non-unity, compiling files standalone that the unity blob normally shields. Consequence:
`APBReloadedEditor` builds clean (exit 0); the `APBReloaded` Game target fails (exit 6).
QA therefore ran through the editor's `-game` path documented in `AGENTS.md`.

Test suites: `Domain`, `Persistence`, `Fidelity` all `FAILS=0`;
`APBPlacementBindingTests` and `APBPlacementParseTests` both `FAILS=0`.
