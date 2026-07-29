# m15 — inventory item CATEGORY taxonomy (InventoryItemInfraCategories.INT) — handoff note

**Author:** Qoder  **Status:** COMPLETE (all 17 domain suites FAILS=0)

## What landed
Extracted the retail APB **inventory-item category taxonomy** — the category buckets the inventory /
Armas / store UI uses to **GROUP and LABEL** the ~13k items (Marketplace Cash, Character, Clothing:
Accessories, Clothing: Armor (Vests), Weapons, Mods, Vehicles, Symbols, ...) — into the Domain layer,
following the established data-catalog recipe. This is the **categorisation / label layer OVER** the
master item-name dictionary in `APBInventoryItemTypes.h` (InventoryItemTypes.INT) ported earlier: the UI
groups items by category, then names each item.

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_inventory_infra_categories.ps1` |
| Data | `Content/Data/inventory_infra_categories.json` (149 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBInventoryInfraCategories.h` (header-only `InventoryInfraCategoryCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`inventory_infra_categories` member, load, `inventory_infra_categories=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestInventoryInfraCategoriesFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\InventoryItemInfraCategories.INT` (UTF-16LE) — mirror of the
  cooked SDD table `InventoryItemInfraCategories`. Single `[InventoryItemInfraCategories]` section.
- **Three keys per category id:**
  - `InventoryItemInfraCategories_<id>_DisplayName=<UI header, e.g. "Clothing: Accessories (Clothing)">`
  - `InventoryItemInfraCategories_<id>_Description=<plural/long label, e.g. "Accessories (Clothing)">`
  - `InventoryItemInfraCategories_<id>_SingularName=<singular label, e.g. "Accessory (Clothing)">`
- 150 ids in the table; the all-empty placeholder `None` id is dropped -> **149 rows**.
- Labels kept **VERBATIM** (no stray `\u` — \uXXXX-restore); U+21B5 -> real `\n`; C0 control stripped.

## Shape & the helpers
- 149 rows. `InventoryInfraCategory{ id, display_name, description, singular_name, order }`.
- `InventoryInfraCategoryCatalog` API: `Find / DisplayName / Description / SingularName / Has / Count`,
  merge-by-id, order-sorted. Same private JSON helpers as the sibling catalogs (linear Find — 149 rows,
  no index needed).

## Notes for other agents
- This is the category/label layer. When the inventory/store UI groups items: take an item id from
  `inventory_item_types`, map it to a category id, then use
  `inventory_infra_categories.DisplayName(catId)` for the section header (and Description/SingularName for
  the plural/singular labels). Do NOT hardcode category strings.
- **Item -> category MAPPING is NOT in this INT.** This table only supplies the category *labels*. The
  per-item category assignment (which item belongs to which bucket) lives in the cooked SDD item rows /
  apbdb item API, alongside the reward PAYLOAD data — a follow-up increment.
- **Dead-end still standing:** `InventoryItemPrices.INT` (5495 rows) is a stub — every
  `CustomAdditionalItem` value is empty; real prices live in the apbdb catalogs. Do not port it.
- **Reward PAYLOAD increment still pending (next big step):** attach a `contents` list (item ids + counts
  + lease flags) to each reward/package id from cooked SDD / apbdb.com so completion grants real items;
  `inventory_item_types` + `unlock_item_types` resolve those ids to display text, and
  `inventory_infra_categories` supplies their grouping labels. Payload data is NOT in any INT.
