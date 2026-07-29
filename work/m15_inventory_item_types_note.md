# m15 — master inventory item-type dictionary (InventoryItemTypes.INT) — handoff note

**Author:** Qoder  **Status:** COMPLETE (all 17 domain suites FAILS=0)

## What landed
Extracted the retail APB **master inventory ITEM-TYPE dictionary** — the authoritative `id -> display
name` map for every inventory item type in APB (weapons, mods, clothing, symbols, vehicles, rewards,
equipment, consumables, ...) — into the Domain layer, following the established data-catalog recipe.
Each entry also carries the item's `CreatorName` (author credit). This is the dictionary the
inventory / Armas / rewards UI uses to render item names, and it is the foundation for the documented
reward **payload** increment (resolving reward `contents` ids to real item names).

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_inventory_item_types.ps1` |
| Data | `Content/Data/inventory_item_types.json` (12997 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBInventoryItemTypes.h` (header-only `InventoryItemTypeCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`inventory_item_types` member, load, `inventory_item_types=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestInventoryItemTypesFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\InventoryItemTypes.INT` (UTF-16LE) — mirror of the cooked
  SDD table `InventoryItemTypes`. Single `[InventoryItemTypes]` section, 26470 kv lines (largest INT).
- Two keys per item id, grouped by the extractor:
  - `InventoryItemTypes_<id>_DisplayName=<player-facing name>`
  - `InventoryItemTypes_<id>_CreatorName=<author: "Reloaded Productions" / community creator>`
- 13235 distinct ids; **238 placeholder ids (None/Vacant slots) carry an empty DisplayName and are
  dropped** -> **12997 real rows**. `CreatorName` is preserved verbatim (may be empty).
- Prose kept **VERBATIM** (no stray `\u` — \uXXXX-restore). Note ids can contain double underscores
  (e.g. `Equipment__None`), and the `_(DisplayName|CreatorName)$` suffix anchor handles that.

## Shape & the helpers
- 12997 rows. `InventoryItemType{ id, display_name, creator_name, order }`.
- `InventoryItemTypeCatalog` API: `Find / DisplayName / CreatorName / HasDisplayName / ForCategory /
  Count`, merge-by-id, order-sorted.
- **Internal `unordered_map<id,index>`** rebuilt after the order-sort backs `Find` -> O(1) lookups over
  ~13k rows (WorldService is re-initialised across ~25 tests). Public API mirrors the sibling catalogs.
- **`static Category(id)`** returns the FIRST token (`Mod_None` -> `Mod`, `Reward_GenericReward` ->
  `Reward`). Creator distribution: Reloaded Productions(4027) / (empty)(1778) / Little Orbit(1162) /
  Joker Distribution(475) / Armas Marketplace(307) / Obeya Corps. Armory(290) / ...

## Notes for other agents
- This is the id->name resolver. When any UI (inventory, Armas, mail, rewards) needs to show an item's
  name, look it up here by id; do NOT hardcode item names.
- **Reward PAYLOAD increment (next):** attach a `contents` list (item ids + counts + lease flags) to
  each reward/package id from the cooked SDD / apbdb.com item API, so milestone/mission completion can
  grant the real items. This dictionary resolves those contents ids to display names; the reward-text
  catalogs (`reward_packages` / `weighted_rewards` / `redeemable_rewards` / `reward_item_types`) supply
  the display/mail wrapper. The payload data itself is NOT in any INT — it lives in the cooked SDD.
