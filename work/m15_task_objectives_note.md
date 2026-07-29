# M15/D14 — Per-stage mission briefs (TaskObjectives.INT) — handoff note

**Agent:** Qoder  •  Follows the MissionTemplates.INT title increment + template-id↔script-id matching.

## What this increment did
Recovered the **per-stage owner/dispatch mission briefings** (907 stage-brief rows across 211
mission templates) from the shipped `TaskObjectives.INT` and wired them into the Domain as a
read-only `MissionBriefCatalog` keyed by `"<template_id>_Stage<NN>"`. These are the authoritative
narrative briefings each side sees during a mission (OwnerBrief = attacker/mission owner,
DispatchBrief = dispatched opposition/defender). Advances decision **D14**
(`TaskObjectives.INT` strings parsed into `Content\Data\`).

## Source of truth
- `C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\TaskObjectives.INT`
  UTF-16LE, 1944 lines. Shipped localization **mirror of the cooked SDD table `TaskObjective`**.
- Keys: `TaskObjectives_<TemplateId>_Stage<NN>_OwnerBrief=<TEXT>` and `..._DispatchBrief=<TEXT>`.
  `TemplateId` shares the `MissionTemplate` id space **1:1** (e.g. `AE_BCS0_Ter1_B`), so briefs join
  onto `mission_templates.json` / the loaded mission scripts by id. Verified: **all 211** distinct
  brief template ids resolve in `mission_templates.json` (0 unmatched).
- Briefs carry inline retail markup like `<Col: StageText>...</Col>` — preserved **verbatim**
  (UE5 text rendering can interpret/strip it later; not our call to normalize).

## Pipeline added (reusable)
- `tools/scripts/extract_task_objectives.ps1` — parses TaskObjectives.INT → emits
  `Content/Data/task_objectives.json` (pure top-level array of
  `{id, template_id, stage, owner_brief, dispatch_brief, source}`). Mirrors the other INT→JSON
  extractors: `Clean()` collapses control/U+21B5 newline glyphs to a space, and the mandatory
  `\uXXXX` decode step (except `"`/`\`/control) so the naive Domain `JStr` reads `<`, `>`, `&`,
  apostrophes verbatim (no `u003c`/`u0027` mangling).
  - Re-run: `pwsh -NoProfile -File tools\scripts\extract_task_objectives.ps1`
- Output: **907 stage-brief rows / 211 templates**, top-level array, 0 residual `\uXXXX`, 0 U+21B5.

## Domain wiring
- New `MissionBrief` struct + `MissionBriefCatalog` (APBMission.h/.cpp): `briefs` map (keyed
  `<template_id>_Stage<NN>`), `LoadFromJsonFile/Text` (additive, merge-by-id), `Find`,
  `ForTemplate(template_id)` (returns all stages ordered ascending by stage number), `Count()`.
  Uses the existing anonymous-namespace `JStr`/`JNum`/`SplitObjects`/`ReadFile` helpers.
- `WorldService` gained a `MissionBriefCatalog mission_briefs` member, loaded in
  `InitFromDataDir` (`task_objectives.json`); INIT log gained a `mission_briefs=` token.
- Test: `TestTaskObjectivesFromRetail` in `tests/run_domain_tests.cpp` (roster ≥900, known
  brief fields, markup-preserved + anti-mangling invariant, `ForTemplate` ordering, end-to-end via
  InitFromDataDir, and join integrity — every brief template resolves to a canonical mission title).

## Verified
- `pwsh -NoProfile -File tests\build_and_run.ps1` → SCRIPT_EXIT=0; all **17** suites FAILS=0,
  0 `FAIL:`, 0 `error C`.
- `task_objectives.json`: firstchar `[`, 907 rows, 211 templates, markup literal.

## NOT done here (still open)
- **`TaskOperations.INT`** (398 lines): `TaskOperations_<OpId>_UIDescription=<short text>` — short
  per-operation UI objective labels ("Graffiti Target", etc.). Clearly-scoped next increment: a tiny
  lookup catalog like `MissionTitleCatalog` (id → UIDescription). Note many entries are empty
  (`None`, `OppositionDefault`).
- **Attaching briefs onto live `MissionRun` stages:** the catalog is a standalone per-stage lookup.
  Wiring `MissionRun` stages to their `MissionBrief` (so the runtime surfaces the owner/dispatch text
  for the active stage) is a follow-up; the join is by `<template_id>_Stage<NN>`.
- Numeric mission tuning (rewards/timers/thresholds) is cooked SDD binary, not in the INT.

## For other agents
- Treat `task_objectives.json` as generated output — regenerate via the script, don't hand-edit.
- Keep Domain-parsed data files as pure top-level arrays (the naive `SplitObjects` splits depth-0
  `{...}` only; keep object fields flat — string/number, no nested arrays/objects).
- Any new INT→JSON extractor MUST decode `\uXXXX` (except `"`/`\`) so the naive Domain parser
  reads punctuation/markup verbatim.
