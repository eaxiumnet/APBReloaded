# m15 — Scoreboard-column tooltip catalog (ScoreboardDescriptions.INT) — handoff note

**Author:** Qoder  **Status:** COMPLETE (all 17 domain suites FAILS=0)

## What landed
Extracted the retail APB scoreboard-column tooltip catalog into the Domain layer, following the
established data-catalog recipe (extractor .ps1 -> JSON -> header-only catalog -> WorldService
wiring -> test).

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_scoreboard_descriptions.ps1` |
| Data | `Content/Data/scoreboard_descriptions.json` (22 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBScoreboardDescriptions.h` (header-only `ScoreboardDescriptionCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`scoreboard_descriptions` member, load, `scoreboard_descriptions=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestScoreboardDescriptionsFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\ScoreboardDescriptions.INT` (UTF-16LE) — the localized
  mirror of the cooked SDD table `ScoreboardDescription`. Every entry is a single field:
  `ScoreboardDescriptions_Column_<id>_DisplayText=<tooltip>`.
- **Both the column id and the tooltip text are verbatim from the INT.** The `<id>` is the
  scoreboard column key (`Arrests`, `Kills`, `Deaths`, `Score`, `Threat`, `PlayerName`, ...).
- 22 columns, no drops. Covers mission-scoreboard columns (Arrests/Assists/Kills/Deaths/Cash/
  Standing/Score/Targets/Side/Threat/Time/TimeAlive/MVP/Medals/PlayerName + Premium variants) and
  chaos-mode columns (Best*/Total* one-life records).

## Shape
- 22 rows. `ScoreboardDescription{ id, display_text, order }`.
- `ScoreboardDescriptionCatalog` API: `Find/DisplayText/Count`, merge-by-id, order-sorted.

## Notes for other agents
- This is the authoritative source for the **scoreboard column hover/tooltip text** on the
  post-mission and chaos-mode end-of-round screens. Read from
  `WorldService.scoreboard_descriptions` and use `DisplayText(columnId)` — do not hardcode the
  tooltip strings in the HUD.
- The catalog supplies column *descriptions* only; the numeric per-player values (arrests, kills,
  cash, standing awarded, etc.) come from the live mission-result / scoring pipeline
  (mission_result_reasons + combat resolve + progression), NOT here. Wiring the column ids to
  their live values is a natural follow-up when the scoreboard UI is built.
- The `Premium` columns (`CashPremium`, `StandingPremium`) describe the bonus cash / standing a
  Premium account receives on top of the base award — keep them as distinct columns.
- Still-open high-value INT gap: `ModifierEffects.INT` (508 lines, character/consumable stat
  modifiers with `<Color:R=..>` markup + multi-line `_2/_3` variants) needs a markup-aware parser;
  left for a dedicated increment.
