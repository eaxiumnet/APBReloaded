# M15 / D14 increment — Notoriety/Prestige heat-level HUD descriptions

**Status:** COMPLETE. All 17 domain test binaries green (FAILS=0, 0 FAIL:, 0 error C).

## What this adds
The player-facing **heat-level descriptions** — the blurb shown at each notoriety
(Criminal N0-N5) / prestige (Enforcer P0-P5) level — now flow through the Domain and
reach the HUD bridge. Example (N0): *"At Notoriety Level Zero, you're pretty good at
keeping a low profile. Stay under the radar and Enforcers won't oppose you."*

## Key insight — no new extraction needed
`Content/Data/threat_table.json` (apbdb `/beacon/heat`, the AGENTS.md-authoritative data
source) **already carried these descriptions verbatim** — they are byte-for-byte the same
strings as the retail `HeatLevels.INT` SDD mirror (cross-checked: N0/N5 notoriety, P0/P5
prestige all match). The gap was purely on the Domain side: the `ThreatTier` struct had no
`description` field, so `ParseTierArray` discarded the text and it never reached the game.
So this increment threads the EXISTING authentic data through rather than re-extracting a
redundant file.

## Changes
- `Source/APBReloaded/Domain/APBThreat.h`
  - `struct ThreatTier` gains `std::string description;`.
  - `ParseTierArray` now reads `t.description = Str(obj, "description", Str(obj, "sDescription", ""))`
    (tolerates either apbdb key). The naive `Str` scanner is safe here: the descriptions use
    literal apostrophes (no `\uXXXX`) and contain no embedded double-quotes.
  - `CurrentTier()` returns by value, so the description flows out automatically.
- `Source/APBReloaded/Domain/APBWorldService.{h,cpp}`
  - `DomainSnapshot` gains `threat_level`, `threat_tier_name` (apbdb id e.g. "NotorietyLevel3"),
    and `threat_tier_description`.
  - `CaptureSnapshot()` populates them from `threat.CurrentTier()` — the single Domain->UI
    bridge now exposes the heat blurb for the HUD.

## Tests
`tests/run_domain_tests.cpp` — `TestHeatLevelDescriptionsFromRetail`: notoriety L0/L5 +
prestige P0 description anchors; apostrophe round-trip (no leftover `\u`); every one of the
6 notoriety + 6 prestige tiers has a non-empty description; DomainSnapshot exposes the tier
level/name/description end-to-end after a character is created.

## Notes for other agents
- HUD heat widget should read `DomainSnapshot.threat_tier_description` (+ `threat_level`) —
  do NOT hardcode the notoriety/prestige blurbs. On the UE side, map these onto
  `FAPBDomainSnapshotUE` next to the existing `threat_points`/`threat_bots` and replicate.
- `threat_tier_name` is the apbdb id ("NotorietyLevelN"/"PrestigeLevelN"), not a pretty
  label; if a short display name is wanted, derive it from `threat_level` + faction.
- HeatLevels.INT itself was NOT re-extracted — threat_table.json is the authoritative
  source and already matches it 1:1. If a future need arises to diff them, the retail file
  is `...\APBGame\Localization\INT\HeatLevels.INT` (mirror of SDD table HeatLevel).
