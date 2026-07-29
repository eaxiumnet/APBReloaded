# M15 — Full retail role roster (PlayerRoles.INT) — handoff note

**Agent:** Qoder  •  **Date:** M15 increment (follows the ContactLevels.INT increment)

## What this increment did
Recovered the **complete retail role roster** with canonical display names + descriptions
from the shipped `PlayerRoles.INT` localization file and wired it into the Domain
`ProgressionCatalog`, so the recreation carries all 243 shipped roles instead of the ~20
partial apbdb-seeded entries in `roles.json`.

## Source of truth
- `C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\PlayerRoles.INT`
  (also under `D:\APBReloaded\APB Reloaded\...`). UTF-16LE, 494 lines. It is the shipped
  localization **mirror of the cooked SDD table `PlayerRoles`**.
- Keys: `PlayerRoles_<RoleId>_DisplayName=<text>` and `PlayerRoles_<RoleId>_Description=<text>`.
  `RoleId` is everything between the `PlayerRoles_` prefix and the `_DisplayName`/`_Description`
  suffix (so `..._2016_PC` stays part of the id). The literal `None` id is skipped.

## Pipeline added (reusable)
- `tools/scripts/extract_player_roles.ps1` — parses PlayerRoles.INT → emits
  `Content/Data/player_roles.json`. Mirrors `extract_contact_levels.ps1`:
  - Emits a **pure top-level JSON array** of `{id, name, description, source}` (required — the
    Domain `JsonSplitObjects` only splits depth-0 `{...}`; a wrapper object would break parsing).
  - Ordered by first appearance so the JSON mirrors the INT roster order.
  - `Clean()` collapses any run of `[\x00-\x1f\u2028\u2029\u21b5]` to a single space. NOTE: the
    SDD flattens multi-line text onto one physical line using the **U+21B5 ↵ "return" glyph**
    (NOT `\r\n`, NOT a `\x00-\x1f` control char) — that glyph MUST be in the regex or descriptions
    keep the ↵.
  - Re-run any time: `pwsh -NoProfile -File tools\scripts\extract_player_roles.ps1`
- Output: **243 roles** (244 in the INT minus `None`), incl. all **13 `Role2_*`** activity/weapon
  tracks (e.g. `Role2_CrimArson`→"Arsonist", `Role2_Crim_Hacking`→"Black-Hat").

## Domain wiring
- `RoleDef` (APBProgression.h) gained `std::string description`.
- `ProgressionCatalog::LoadRolesFromText` (APBProgression.cpp) now parses `"description"`.
- `WorldService::InitFromDataDir` loads `player_roles.json` **after** `roles.json`, so the
  retail-canonical roster merges on top of the partial apbdb seed (merge-by-id, retail wins).
  INIT log gained a `roles=` token.
- Test: `TestPlayerRolesFromRetail` in `tests/run_domain_tests.cpp` (roster ≥240, canonical
  names, non-empty description, single-line description, end-to-end via InitFromDataDir).

## Verified
- `pwsh -NoProfile -File tests\build_and_run.ps1` → SCRIPT_EXIT=0; all **17** suites FAILS=0,
  0 `FAIL:`, 0 `error C`.
- `player_roles.json`: firstchar `[`, 243 ids, 0 remaining U+21B5.

## NOT done here (still open for M15)
- Numeric **role-XP thresholds / per-level rewards** are cooked SDD binary, NOT in the INT — only
  the roster + display names/descriptions are real. XP curves remain tunable recreation defaults.
- Per-level contact **standing thresholds** (same story — cooked SDD).
- Contact/kiosk UMG.

## For other agents
- Treat `player_roles.json` as generated output — regenerate via the script, don't hand-edit.
- Keep Domain-parsed data files as pure top-level arrays.
- Descriptions are single-line by design (SDD newlines collapsed to spaces).
