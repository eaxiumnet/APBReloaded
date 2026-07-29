# M15 / D14 increment — Faction-selection screen content (display names + lore)

**Status:** COMPLETE. All 17 domain test binaries green (FAILS=0, 0 FAIL:, 0 error C).

## What this adds
The authentic APB **faction picker** content shown at character creation: the two
faction display names (Enforcer / Criminal) plus the three info-screen bodies —
**General Info** (shared San Paro city lore), **Enforcer**, and **Criminal** — with
their full multi-paragraph descriptions preserved 1:1.

Previously the Domain only had the bare `Faction` enum (`Enforcer`/`Criminal`) +
`FactionName()`; none of the on-screen faction lore/display text was represented.

## Data source (retail, read-only reference)
`C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\Factions.INT`
— UTF-16LE mirror of the cooked SDD table **Faction**. Keys per id:
`_DisplayName`, `_FactionInfoDisplayName`, `_FactionInfoDescription`.
- The 4th id `Both` is `DNT - DO NOT TRANSLATE` (a mask, not a player faction) — skipped.
- Lore paragraph breaks are encoded as **U+21B5** (down-left arrow); a *pair* of
  U+21B5 == a blank-line break between paragraphs.

## Pipeline
- `tools/scripts/extract_factions.ps1` (DURABLE) parses the INT with regex
  `^Factions_(?<id>[^_]+)_(DisplayName|FactionInfoDisplayName|FactionInfoDescription)=...`.
  - Converts each `U+21B5` to a newline BEFORE cleaning so paragraph structure survives
    (the standard `Clean()` here strips only *other* control chars and keeps `\n`).
  - Skips empty-description and DNT rows.
  - Reuses the `[pscustomobject]` + `,@($rows.ToArray())` + `ConvertTo-Json -InputObject`
    workaround for the PS 5.1 "Argument types do not match" `List[object]` bug, and the
    standard `\uXXXX`-decode so apostrophes/&/</> round-trip.
- Output `Content/Data/factions.json` — flat top-level array
  `{id, display_name, info_title, info_description, rank, source}`, **3 rows**:
  None (General Info, rank 0), Enforcer (rank 1), Criminal (rank 2). Paragraph breaks
  are stored as escaped `\n\n`.

## Domain (header-only, NEW `APBFactionInfo.h`)
- `struct FactionInfo` (id, display_name, info_title, info_description, rank).
- `class FactionInfoCatalog` — `LoadFromJsonFile/Text` (additive/merge, sorts by rank),
  `Find`, `ForFaction(Faction)` (enum→id), `GeneralInfo()` (the None row),
  `Description`, `ParagraphCount`, `Count`.
- **Unlike the other Domain catalogs**, this one carries a PROPER JSON string unescaper
  (`\n \r \t \" \\ \/ \b \f` and `\uXXXX`→UTF-8) plus a string-/escape-aware
  `SplitTopObjects` and a `RawStr` that honours `\"`. The naive scanners used elsewhere
  collapse `\n`→`n`, which would destroy the multi-paragraph lore. Kept header-only
  (every method in-class ⇒ implicitly inline), matching `ThreatRatingCatalog` — so **no
  build-script change was needed** (pulled in transitively via `APBWorldService.h`).
- `APBWorldService.{h,cpp}` — new member `faction_info`; loaded in `InitFromDataDir`
  right after `threat_ratings.json`; emits ` factions=<n>` (+ `(factions_missing)` when
  the file is absent) in the INIT log line.

## Tests
`tests/run_domain_tests.cpp` — `TestFactionInfoFromRetail`: Count==3; display-name /
info-title anchors; `ForFaction` enum mapping; `GeneralInfo()`==None; rank ordering;
lore content anchors (Enforcer opening verbatim, "Prentiss Tigers", "G-Kings"/"Blood
Roses"); apostrophe round-trip ("It's always been here") with no leftover `\u`;
paragraph breaks preserved as real `\n\n` with no literal `\n` leaked; ParagraphCount>=3;
missing-id default + DNT 'Both' absent; end-to-end via `WorldService`.

## Notes for other agents
- Feed the faction picker / character-create UI from
  `WorldService.faction_info` — do NOT hardcode the Enforcer/Criminal blurbs. Use
  `ForFaction(Faction)` for the selected faction and `GeneralInfo()` for the shared
  San Paro intro. `info_description` already contains `\n\n` paragraph breaks ready for
  a UMG multi-line rich-text block.
- This is pure UI/reference content; it does not change gameplay state. The replicated
  faction itself still lives on `AAPBPlayerState` (the `Faction` enum).
- The parser in `APBFactionInfo.h` is the first Domain catalog with a correct JSON
  string unescaper — reuse it as the template whenever a future catalog needs to carry
  multi-line / escaped text rather than flat single-line fields.
