# M15 / D14 increment — Mission result-reason end-screen messages

**Status:** COMPLETE. All 17 domain test binaries green (FAILS=0, 0 FAIL:, 0 error C).

## What this adds
Authentic APB mission **end-screen Win/Lose/Draw messages**, keyed by the game's
result-reason id (the reason a mission ended). This is the exact retail text the HUD
shows when a mission resolves, complementing the Domain's existing mission
win / fail / timeout / opposition resolution logic.

## Data source (retail, read-only reference)
`C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\MissionResultReasons.INT`
— UTF-16LE mirror of the cooked SDD table **MissionResultReason**. Each reason id has
`_WinMessage` / `_LoseMessage` / `_DrawMessage` keys.

## Pipeline
- `tools/scripts/extract_mission_result_reasons.ps1` (DURABLE) parses the INT with regex
  `^MissionResultReasons_(?<id>.+)_(?<kind>Win|Lose|Draw)Message=(?<val>.*)$`.
  Skips the `None` id and any reason where all three messages are empty. Applies the
  standard `Clean()` (control/U+21B5 collapse) + `\uXXXX` decode so apostrophes /
  ampersands round-trip literally (never the naive-parser-mangling `u0027`/`u0026`).
- Output `Content/Data/mission_result_reasons.json` — flat top-level array
  `{id, win_message, lose_message, draw_message, source}`, **17 reasons**:
  Abandoned, ChallengeCycled, CompletedUnopposed, Declined, Emergency, Forced,
  ObjectiveCompleted, ObjectiveFailed, OppositionDestroyedOwnerTarget,
  OwnersDestroyedOwnerTarget, Reassigned, RemovedFromSide, SideTooSmall, TimedOut,
  VipKillLimitReached, WonFinalObjective, WonMostObjectives.

## Domain
- `Source/APBReloaded/Domain/APBMission.h` — `struct MissionResultReason` +
  `class MissionResultReasonCatalog` (reasons map; LoadFromJsonFile/Text;
  Find; WinMessage/LoseMessage/DrawMessage with caller default; Count). Additive/merge
  (never clears on empty text), consistent with the other catalogs.
- `Source/APBReloaded/Domain/APBMission.cpp` — impl.
- `Source/APBReloaded/Domain/APBWorldService.{h,cpp}` — new member
  `mission_result_reasons`; loaded in `InitFromDataDir` after `task_operations.json`;
  emits ` mission_result_reasons=<n>` in the INIT log line.

## Tests
`tests/run_domain_tests.cpp` — `TestMissionResultReasonsFromRetail`:
Count>=15; anchors (TimedOut win/lose, WonFinalObjective win, CompletedUnopposed win);
apostrophe round-trip on `SideTooSmall` lose ("Your side doesn't have enough players");
missing-id returns default + Find null; anti-mangling `u0027`/`u0026` invariant;
end-to-end via `WorldService`.

## Notes for other agents
- When wiring the mission end screen UI, resolve the shown message via
  `mission_result_reasons.{Win,Lose,Draw}Message(reasonId)` — do not hardcode text.
- Numeric tuning (reward/timer values) is NOT in the INTs; it lives in cooked SDD binary.
  Still data-blocked, same as the stage->operation-id link (see m15_task_operations_note.md).
