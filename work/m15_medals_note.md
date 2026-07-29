# m15 — Medal / award catalog (Medals.INT) — handoff note

**Author:** Qoder  **Status:** COMPLETE (all 17 domain suites FAILS=0)

## What landed
Extracted the retail APB medal/award catalog into the Domain layer, following the
established data-catalog recipe (extractor .ps1 -> JSON -> header-only catalog ->
WorldService wiring -> test).

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_medals.ps1` |
| Data | `Content/Data/medals.json` (82 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBMedals.h` (header-only `MedalCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`medals` member, load, `medals=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestMedalsFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\Medals.INT` (UTF-16LE) — the localized mirror
  of the cooked SDD table `Medal`. Each medal is `Medals_<id>_Title` + `Medals_<id>_Description`.
- **Titles and descriptions are verbatim from the INT** (the human-readable unlock criteria).
- `category` is **derived** from the id's first underscore token — it is NOT a separate INT
  field. Observed categories: `KillStreak`, `BigWin`, `Dishonour`, `Situational`, `TimeLimit`,
  `KillBehind`.
- The empty `None` placeholder row (blank title+desc) is skipped by the extractor.

## Shape
- 82 medals. `Medal{ id, title, description, category, order, IsDishonour() }`.
- 39 rows are negative "Demerit" dishonours (`category == "Dishonour"`): SelfKill (13),
  FriendlyKill (10), FriendlyStun (5), ArrestedKill (10), AFK (1). `MedalCatalog::Dishonours()`
  returns 39.
- `MedalCatalog` API: `Find/Title/Description/ForCategory/Categories/Count/CountForCategory/
  Dishonours`, merge-by-id, order-sorted. Same string-aware JSON helpers as `APBOrganisations.h`
  (apostrophe in "Kill 'Em All" round-trips with no `\u`).

## Notes for other agents
- This is the authoritative list for the **post-mission award popup** and the **profile
  achievements page**. Read titles/descriptions from `WorldService.medals` — do not hardcode.
- The medal *unlock logic* (what actually grants each medal at runtime) is NOT modelled here;
  this catalog only supplies the display strings + category grouping. Award-granting is a
  future increment that would live in the combat/mission-resolve Domain code.
- `RoleMilestones.INT` (718 role-title rows) and `StreetName.INT` (191 district street names)
  are the obvious next self-contained catalog gaps — probed but not yet extracted.
