# m15 — Street-name catalog (StreetName.INT) — handoff note

**Author:** Qoder  **Status:** COMPLETE (all 17 domain suites FAILS=0)

## What landed
Extracted the retail APB street-name catalog into the Domain layer, following the established
data-catalog recipe (extractor .ps1 -> JSON -> header-only catalog -> WorldService wiring -> test).

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_street_names.ps1` |
| Data | `Content/Data/street_names.json` (191 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBStreetNames.h` (header-only `StreetNameCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`street_names` member, load, `street_names=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestStreetNamesFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\StreetName.INT` (UTF-16LE) — the localized mirror
  of the cooked SDD table `StreetName`. Each row is `StreetName_<key>_DisplayedStreetName=<name>`.
- **Names are verbatim from the INT** (accents like "Malaga Drive" and "&" join labels preserved
  1:1 via the extractor's \uXXXX-restore step; the JSON has no stray `\u` escapes).
- `district` is **derived** from the key prefix (`Financial` / `Waterfront`) — NOT a separate INT
  field. `kind` is derived from the key: keys containing `_X_` are `intersection` labels,
  everything else is a single named `street`.
- **Retail typo handled:** one key is `Financia_X_BankBreakwater` (missing the trailing 'l' of
  "Financial"). The extractor still classifies it as a Financial-district intersection; its
  label ("Bank & Breakwater") is preserved verbatim. Covered by a dedicated test assertion.

## Shape
- 191 rows. `StreetName{ id, name, district, kind, order, IsIntersection() }`.
- District split: Financial 84, Waterfront 107 (0 Unknown — every row classified).
- Kind split: named `street` 78, `intersection` 113.
- `StreetNameCatalog` API: `Find/Name/ForDistrict/OfKind/Districts/Count/CountForDistrict/
  CountOfKind`, merge-by-id, order-sorted. Same string-aware JSON helpers as `APBMedals.h`.

## Notes for other agents
- This is the authoritative source for **world-map / minimap location labels** and the
  **mission-waypoint street callouts** ("meet at Shianxi Boulevard"). Read from
  `WorldService.street_names` — do not hardcode street names in HUD/map code.
- Only the two main action districts (Financial, Waterfront) have street data in the retail INT;
  social/fight-club districts have none. This matches retail behaviour.
- The mapping from a world-space position to the correct street-name key is a UE5 map-geometry
  concern (spline/volume tagging), NOT modelled here; this catalog only supplies the display
  strings + district/kind grouping.
- `RoleMilestones.INT` overlaps the existing `roles.json` / `player_roles.json` roster and is
  intentionally NOT re-extracted here to avoid duplicating the progression work.
