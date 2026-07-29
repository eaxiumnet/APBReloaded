# m15 — weapon description catalog (WeaponItemTypes.INT) — handoff note

**Author:** Qoder  **Status:** COMPLETE (all 17 domain suites FAILS=0)

## What landed
Extracted the retail APB **weapon description catalog** — the `id -> rich description` map the Armas
marketplace / weapon-select / inventory UI shows as a weapon's flavour + role blurb (e.g. the HVR-243
sniper, the STAR assault rifle) — into the Domain layer, following the established data-catalog recipe.
This is the missing **DESCRIPTION leg** of the weapon-info triple:

| Leg | File | Provides |
|---|---|---|
| stats | `Content/Data/weapons_catalog.json` (apbdb) | ballistics / stats |
| name | `Content/Data/weapon_display_names.json` (InventoryItemTypes) | id -> display name |
| **description** | `Content/Data/weapon_item_types.json` (**THIS**) | id -> rich description |

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_weapon_item_types.ps1` |
| Data | `Content/Data/weapon_item_types.json` (839 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBWeaponItemTypes.h` (header-only `WeaponItemTypeCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`weapon_item_types` member, load, `weapon_item_types=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestWeaponItemTypesFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\WeaponItemTypes.INT` (UTF-16LE) — mirror of the cooked SDD
  table `WeaponItemTypes`. Single `[WeaponItemTypes]` section, 936 kv lines.
- One key per weapon id: `WeaponItemTypes_<id>_Description=<player-facing text>`.
- 936 distinct ids; only **839 carry a non-empty description** — rows with an empty description are
  dropped (leaving the weapons that actually render a blurb).
- Prose kept **VERBATIM** (no stray `\u` — \uXXXX-restore): apostrophes/&/quotes round-trip; U+21B5 ->
  real `\n`; C0 control stripped. (Note: some descriptions embed a U+21B5 line-break, e.g. the STAR "-\n"
  between title and body — preserved as a newline.)

## Shape & the helpers
- 839 rows. `WeaponItemType{ id, description, order }`.
- `WeaponItemTypeCatalog` API: `Find / Description / HasDescription / ForCategory / ForClass / Count`,
  merge-by-id, order-sorted. Same private JSON helpers as the sibling catalogs (linear Find).
- **`static Category(id)`** = FIRST token (always `Weapon` for weapon ids). **`static Class(id)`** = SECOND
  token — the *discriminating* weapon class (`SniperRifle`, `AssaultRifle`, `Pistol`, ...). Use `ForClass`
  to group by weapon class, `ForCategory` to grab the whole `Weapon` family.

## Notes for other agents
- Pairs with `weapons_catalog` (stats) + `weapon_display_names` (name): when the Armas/weapon-select UI
  shows a weapon, resolve name from `weapon_display_names`, stats from `weapons_catalog`, and the blurb
  from `weapon_item_types.Description(id)`. Do NOT hardcode weapon descriptions.
- **Sibling ItemTypes description tables still to port (identical single-`_Description` schema — each is a
  clean one-increment job with this exact recipe):**
  - `VehicleItemTypes.INT` — 580 kv, **569** non-empty vehicle descriptions.
  - `ClothingItemTypes.INT` — 2058 kv, **1836** non-empty clothing descriptions (largest; core to APB
    customization).
- **Dead-end still standing:** `InventoryItemPrices.INT` (5495 rows) is a stub — every
  `CustomAdditionalItem` value is empty; real prices live in the apbdb catalogs. Do not port it.
- **Reward PAYLOAD increment still pending (bigger step):** attach a `contents` list (item ids + counts +
  lease flags) to each reward/package id from cooked SDD / apbdb.com; resolvers now include
  `inventory_item_types` + `unlock_item_types` + `weapon_item_types`. Payload data is NOT in any INT.
