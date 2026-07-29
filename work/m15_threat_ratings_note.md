# M15 / D14 increment — Matchmaking threat-rating tiers + district-join gating

**Status:** COMPLETE. All 17 domain test binaries green (FAILS=0, 0 FAIL:, 0 error C).

## What this adds
The authentic APB **matchmaking threat rating** — the skill bracket shown next to a
player's name (In Training / Green / Bronze / Silver / Gold) — plus the retail
`AllowedDistrictThreats` rule that gates which district-instance threat brackets a given
rating may join.

**This is a DISTINCT system from notoriety/prestige "heat".** The existing
`ThreatSystem` + `Content/Data/threat_table.json` (apbdb `/beacon/heat`) model the
dynamic in-session notoriety/prestige *points* ladder (N0–N5 / P0–P5). The threat
*rating* is the persistent matchmaking skill bracket and was not previously represented.

## Data source (retail, read-only reference)
`C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\ThreatLevels.INT`
— UTF-16LE mirror of the cooked SDD table **ThreatLevel**. Keys per tier:
`_DisplayedName`, `_AllowedDistrictThreats`.

## Pipeline
- `tools/scripts/extract_threat_ratings.ps1` (DURABLE) parses the INT with regex
  `^ThreatLevels_(?<id>.+)_(DisplayedName|AllowedDistrictThreats)=...`. Standard
  `Clean()` + `\uXXXX` decode. Skips entries with an empty display name.
  - NOTE: `ConvertTo-Json @($list)` on a `List[object]` throws "Argument types do not
    match" under Windows PowerShell 5.1. Rows are built as `[pscustomobject]` and
    serialized via `ConvertTo-Json -InputObject $arr[0]` (a `,@(...ToArray())` wrap) to
    dodge that bug. Reuse this pattern in future extractors.
- Output `Content/Data/threat_ratings.json` — flat top-level array
  `{id, displayed_name, allowed_district_threats, rank, source}`, **5 tiers**:
  ThreatLevel_Training (In Training), ThreatLevel_01 (Green), ThreatLevel_02 (Bronze),
  ThreatLevel_03 (Silver), ThreatLevel_04 (Gold).

## Domain (header-only, in `APBThreat.h`)
- `struct ThreatRating` (id, displayed_name, allowed_district_threats, rank).
- `class ThreatRatingCatalog` — `LoadFromJsonFile/Text` (additive/merge, sorts by rank),
  `Find`, `FindByDisplayedName`, `DisplayedName`, `AllowedDistrictThreats`, `Count`, and
  the matchmaking gate `CanJoinDistrictThreat(ratingId, districtThreatName)`.
  - Gate semantics: parse `AllowedDistrictThreats` (splitting on `,` and ` or `) into a
    set; membership is case-insensitive. An **empty** allowed list confines the rating to
    its own bracket (In Training / Green have no cross-threat access — matches retail).
  - Kept header-only to match `ThreatSystem`'s in-header JSON parsing style; uses its own
    private flat-array parser (`SplitTopObjects`/`RStr`/`RNum`).
- `APBWorldService.{h,cpp}` — new member `threat_ratings`; loaded in `InitFromDataDir`
  right after `threat_table.json`; emits ` threat_ratings=<n>` (+ `(threat_ratings_missing)`
  when the file is absent) in the INIT log line.

## Tests
`tests/run_domain_tests.cpp` — `TestThreatRatingsFromRetail`: Count==5; display-name
anchors; reverse lookup by display name; rank[0]=="In Training"; the district-join gate
(Bronze→Green ✓, Silver→Green ✗, Gold→Silver ✓, Gold→Bronze ✗, Green confined to Green);
missing-id default + Find null; end-to-end via `WorldService`.

## Notes for other agents
- Replicate the threat *rating* (bracket) on `AAPBPlayerState`; use
  `WorldService.threat_ratings.CanJoinDistrictThreat(ratingId, districtThreatName)` for
  district-instance matchmaking gating — do not hardcode the bracket rules.
- Do NOT conflate this with `ThreatSystem` (notoriety/prestige heat points). They are two
  separate mechanics that happen to both be called "threat" in APB.
- The numeric ELO/rating thresholds that promote a player between brackets live in the
  cooked SDD binary (not the INT mirror) — still data-blocked, same as the mission
  stage->operation-id link and mission reward tuning.
