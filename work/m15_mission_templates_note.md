# M15/D14 — Canonical retail mission titles (MissionTemplates.INT) — handoff note

**Agent:** Qoder  •  **Date:** M15/D14 increment (follows the PlayerRoles.INT increment)

## What this increment did
Recovered the **canonical retail mission-title roster** (213 titles) from the shipped
`MissionTemplates.INT` and wired it into the Domain as a read-only `MissionTitleCatalog`
keyed by template id — the authoritative per-template display titles ("GANGLAND ANNEXATION",
"PIMP MY CRIB", ...). Directly advances decision **D14** ("templates parsed from retail
`MissionTemplates.INT` into `Content\Data\`").

## Source of truth
- `C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\MissionTemplates.INT`
  (also under `D:\APBReloaded\APB Reloaded\...`). UTF-16LE, 219 lines. Shipped localization
  **mirror of the cooked SDD table `MissionTemplate`**.
- Keys: `MissionTemplates_<TemplateId>_MissionTitle=<TEXT>`. `TemplateId` is everything between
  the `MissionTemplates_` prefix and the `_MissionTitle` suffix (faction/district/activity/tier/
  variant structure preserved verbatim, e.g. `DB_BCS4_Del1`, `AE_BCS0_Ter1_B`).

## Pipeline added (reusable)
- `tools/scripts/extract_mission_templates.ps1` — parses MissionTemplates.INT → emits
  `Content/Data/mission_templates.json` (pure top-level array of `{id, title, source}`).
  Mirrors `extract_player_roles.ps1` + `extract_contact_levels.ps1`.
  - **Parser-clean JSON:** Windows PowerShell `ConvertTo-Json` `\uXXXX`-escapes apostrophes,
    ampersands, and angle brackets, but the Domain's naive `JStr`/`JsonGetString` does NOT decode
    `\uXXXX` (it would keep a literal `u0027`). The extractor now decodes every `\uXXXX` back to
    its literal char EXCEPT quote (0x22) and backslash (0x5c), so titles like "YOU'RE FIRED!"
    round-trip correctly. **Sibling extractors (`extract_player_roles.ps1`,
    `extract_contact_levels.ps1`) still emit `\u0027` for apostrophes — apply the same decode step
    if their apostrophe/`&` text fidelity matters (their tests currently assert only on
    apostrophe-free values, so no test breaks, but it is a latent fidelity gap).**
    - **UPDATE (follow-up increment): sibling gap CLOSED.** The same `\uXXXX` decode step was added to
      `extract_player_roles.ps1` and `extract_contact_levels.ps1`; both JSONs regenerated (243 roles,
      73 contacts) with **0 residual `\uXXXX`** and apostrophes literal (e.g. "Derren's",
      "'Provocateur'", "'Survivor'"). `TestPlayerRolesFromRetail` gained a roster-wide invariant
      (no role name/description contains a mangled `u0027`, and at least one apostrophe round-trips
      as a literal char). All 17 suites still FAILS=0.
  - Re-run: `pwsh -NoProfile -File tools\scripts\extract_mission_templates.ps1`
- Output: **213 mission titles**, top-level array, 0 residual U+21B5.

## Domain wiring
- New `MissionTitleCatalog` (APBMission.h/.cpp): `titles` map (template id → display title),
  `LoadFromJsonFile/Text` (additive, merge-by-id), `Find`, `TitleFor(id, def)`, `Count()`.
  Uses the existing anonymous-namespace `JStr`/`SplitObjects`/`ReadFile` helpers.
- `WorldService` gained a `MissionTitleCatalog mission_titles` member, loaded in
  `InitFromDataDir` (`mission_templates.json`); INIT log gained a `mission_titles=` token.
- Test: `TestMissionTemplatesFromRetail` in `tests/run_domain_tests.cpp` (roster ≥200, known
  titles, apostrophe-decoded title, unknown-id default, end-to-end via InitFromDataDir).

## Verified
- `pwsh -NoProfile -File tests\build_and_run.ps1` → SCRIPT_EXIT=0; all **17** suites FAILS=0,
  0 `FAIL:`, 0 `error C`.
- `mission_templates.json`: firstchar `[`, 213 ids, apostrophes literal.

## NOT done here (still open)
- **Template-id ↔ mission-script-id matching — DONE (follow-up increment).** Verified the two id
  namespaces are actually IDENTICAL, not disjoint: `JG_`/`DB_`/`AE_` are contact/activity prefixes
  present in BOTH files, and **all 40** `missions.json` script ids match a `mission_templates.json`
  id exactly. `MissionTitleCatalog::ApplyTo(MissionScriptLibrary&)` now stamps the canonical retail
  title onto every loaded `MissionScriptDef` whose id resolves; called in `WorldService::InitFromDataDir`
  (after both scripts + titles load) with a new `titled_scripts=` INIT log token. The only unresolved
  ids are the two synthetic demo/test scripts prefixed `APB_Script_` (no retail MissionTemplate entry),
  which keep their hand-authored titles. `TestMissionTemplatesFromRetail` asserts: ≥40 retail-scheme
  scripts resolve, 0 retail-scheme ids left untitled, every script carries a title, `DB_BCS3_Ars1` ->
  "BOX-LOCK AND .52 BARREL", and `ApplyTo` is idempotent. (The earlier claim above that the prefixes
  "differ" was wrong — corrected here.)
- `TaskObjectives.INT` (1944 lines) / `TaskOperations.INT` objective strings — not extracted yet.
- Numeric mission tuning (rewards/timers) is cooked SDD binary, not in the INT.

## For other agents
- Treat `mission_templates.json` as generated output — regenerate via the script, don't hand-edit.
- Keep Domain-parsed data files as pure top-level arrays.
- Any new INT→JSON extractor MUST decode `\uXXXX` (except `"`/`\`) so the naive Domain parser
  reads punctuation verbatim.
