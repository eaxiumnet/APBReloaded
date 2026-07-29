# M15/D14 — TaskOperations UI labels (Domain `MissionOperationCatalog`)

Status: **DONE** · Author: Qoder · Date: 2026-07-20 · All 17 domain suites FAILS=0.

## What this increment adds

The short per-operation objective-type **HUD labels** APB shows for a mission stage
("Checkpoint", "Graffiti Target", "Bomb Target", "Escape!", …) are now carried in the
Domain, keyed by operation id, extracted 1:1 from the shipped retail localization table.

Pipeline (mirrors the TaskObjectives / MissionTemplates increments):

1. **Extractor** — `tools/scripts/extract_task_operations.ps1`
   - Source: `APB Reloaded\APBGame\Localization\INT\TaskOperations.INT` (UTF-16LE mirror of
     the cooked SDD table `TaskOperation`).
   - Parses `TaskOperations_<OpId>_UIDescription=<short text>`.
   - Skips placeholder ops with an empty `UIDescription` (e.g. `None`, `OppositionDefault`)
     — they carry no HUD label, same "keep only real evidence" convention as ContactLevels.
   - Applies the standard `Clean()` (collapse control chars + U+21B5 SDD newline glyph) and
     the standard `\uXXXX` decode step (Windows PowerShell escapes `'`/`&`/`<`/`>`; the
     Domain's naive `JStr` does NOT decode `\uXXXX`, so we decode everything except
     `0x22`/`0x5c`/control back to literal chars).
   - Emits `Content/Data/task_operations.json` — a **pure top-level array** of flat
     `{id, ui_description, source}` objects (the only shape the Domain's `SplitObjects` +
     `JStr` can parse).
   - Result: **318 operations, 35 distinct labels** (verified 0 disallowed `\uXXXX`).

2. **Domain catalog** — `Source/APBReloaded/Domain/APBMission.{h,cpp}`
   - New `MissionOperationCatalog` (declared right after `MissionBriefCatalog`):
     `std::unordered_map<std::string,std::string> ops` (op id -> label),
     `LoadFromJsonFile/Text` (additive/merge-by-id, never clears on empty),
     `Find`, `LabelFor(id, def="")`, `Count()`.
   - Same pattern as `MissionTitleCatalog` — a tiny read-only lookup.

3. **WorldService wiring** — `Source/APBReloaded/Domain/APBWorldService.{h,cpp}`
   - New member `MissionOperationCatalog mission_ops;`.
   - `InitFromDataDir` loads `task_operations.json` and the `INIT …` log line now carries a
     `mission_ops=<count>` token.

4. **Test** — `tests/run_domain_tests.cpp` : `TestTaskOperationsFromRetail`
   - Asserts `Count() >= 300`; known retail anchors
     `AntiGraffiti10NoHoldPoints`→"Graffiti Target", `CheckpointAllAtOnce05`→"Checkpoint",
     `Escape120`→"Escape!"; missing-id returns caller default and `Find` is null;
     anti-mangling invariant (no `u0027`/`u0026` in any label); WorldService end-to-end.

## Verified

`tests\build_and_run.ps1` → **17 suites, all FAILS=0**, 0 `FAIL:`, 0 `error C`.
The new suite's 9 asserts all PASS, including `world loaded operation labels` and
`world op label resolves`.

## NOT done here (clearly-scoped follow-ups)

- **Attach labels to live mission stages.** `MissionStageDef` has a `type` string but there
  is no mapping from a stage/op to its `TaskOperation` op id yet. Wiring `LabelFor(op_id)`
  onto `MissionStageRuntime` (so the HUD/snapshot can surface the objective label) needs the
  stage→operation id link, which lives in the cooked SDD `MissionTemplate`/`Task` tables
  (not in the INT mirrors). Next data step: extract that stage→op association.
- **Cooked-SDD numeric tuning** (op point values, hold times, counts) remains binary-only;
  the INT mirrors carry only display text.
- `task_operations.json` labels are the retail (current-build) strings; a 2011-vs-retail
  diff of objective labels was not attempted this increment.

## Tooling note for other agents

`extract_task_operations.ps1` is idempotent and safe to re-run. All three INT→JSON
extractors (`extract_mission_templates`, `extract_task_objectives`, `extract_task_operations`)
now share the identical `Clean()` + `\uXXXX`-decode pattern — copy from any of them when
adding the next INT table. Domain JSON must stay a pure top-level flat-field array.
