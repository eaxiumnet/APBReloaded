# m15 — clothing description catalog (ClothingItemTypes.INT) — handoff note

**Author:** Qoder  **Status:** COMPLETE (all 17 domain suites FAILS=0)

## What landed
Extracted the retail APB **clothing description catalog** — the `id -> rich description` map the Armas
marketplace / character-customization / inventory UI shows as a clothing item's flavour + role blurb
(e.g. armoured arm pads, faction jackets, preset outfits) — into the Domain layer, following the
established data-catalog recipe. This is the **DESCRIPTION leg** of the clothing-info pairing and the
**largest** of the ItemTypes description tables — core to APB character customization. It completes the
ItemTypes description family (Weapon + Vehicle + Clothing all now ported).

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_clothing_item_types.ps1` |
| Data | `Content/Data/clothing_item_types.json` (1836 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBClothingItemTypes.h` (header-only `ClothingItemTypeCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`clothing_item_types` member, load, `clothing_item_types=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestClothingItemTypesFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\ClothingItemTypes.INT` (UTF-16LE) — mirror of the cooked SDD
  table `ClothingItemTypes`. Single `[ClothingItemTypes]` section, 2058 kv lines.
- One key per clothing id: `ClothingItemTypes_<id>_Description=<player-facing text>`.
- 2058 distinct ids; only **1836 carry a non-empty description** — rows with an empty description are
  dropped (leaving the clothing items that actually render a blurb).
- Prose kept **VERBATIM** (no stray `\u` — \uXXXX-restore): apostrophes/&/quotes round-trip; U+21B5 ->
  real `\n`; C0 control stripped.

## Shape & the helpers
- 1836 rows. `ClothingItemType{ id, description, order }`.
- `ClothingItemTypeCatalog` API: `Find / Description / HasDescription / ForCategory / ForClass / Count`,
  merge-by-id, order-sorted. Same private JSON helpers as the sibling catalogs (linear Find).
- **`static Category(id)`** = FIRST token (`Clothing`). **`static Class(id)`** = SECOND token — the
  *discriminating* class. Observed distribution across the 1836 rows: `Preset` 1013 (preset outfits),
  `M` 412 (male gender-slot items), `F` 411 (female gender-slot items). Use `ForClass` to group by
  gender/preset, `ForCategory` to grab the whole family.
- **Retail data quirk (preserved verbatim):** 2 rows carry a misspelled `Cloting_` prefix instead of
  `Clothing_`, so `Category(id)` returns `Cloting` for those two. This is a genuine source-data typo in
  ClothingItemTypes.INT, not an extraction bug — kept as-is for 1:1 fidelity.

## Notes for other agents
- Pairs with apbdb clothing catalogs (slot / faction / unlock metadata): when the Armas / customization UI
  shows a clothing item, resolve metadata from the apbdb catalogs and the blurb from
  `clothing_item_types.Description(id)`. Do NOT hardcode clothing descriptions.
- **The single-`_Description` ItemTypes family is now COMPLETE:** Weapon (839) + Vehicle (569) +
  Clothing (1836) all ported, alongside the earlier Modifier / Inventory / Unlock item-type tables. No
  more sibling `_Description` INT tables remain to port.
- **Dead-end still standing:** `InventoryItemPrices.INT` (5495 rows) is a stub — every
  `CustomAdditionalItem` value is empty; real prices live in the apbdb catalogs. Do not port it.
- **Reward PAYLOAD increment still pending (bigger step):** attach a `contents` list (item ids + counts +
  lease flags) to each reward/package id from cooked SDD / apbdb.com; description resolvers now include
  `inventory_item_types` + `unlock_item_types` + `weapon_item_types` + `vehicle_item_types` +
  `clothing_item_types`. Payload data is NOT in any INT.
