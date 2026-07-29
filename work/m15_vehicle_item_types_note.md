# m15 — vehicle description catalog (VehicleItemTypes.INT) — handoff note

**Author:** Qoder  **Status:** COMPLETE (all 17 domain suites FAILS=0)

## What landed
Extracted the retail APB **vehicle description catalog** — the `id -> rich description` map the Armas
marketplace / vehicle-select / inventory UI shows as a vehicle's flavour + role blurb (e.g. the tuned
Classic Muscle variants, the Jericho 4x4) — into the Domain layer, following the established data-catalog
recipe. This is the **DESCRIPTION leg** of the vehicle-info pairing, exactly analogous to the weapon triple:

| Leg | File | Provides |
|---|---|---|
| stats | `Content/Data/vehicles_catalog.json` (apbdb) | handling / stats |
| **description** | `Content/Data/vehicle_item_types.json` (**THIS**) | id -> rich description |

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_vehicle_item_types.ps1` |
| Data | `Content/Data/vehicle_item_types.json` (569 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBVehicleItemTypes.h` (header-only `VehicleItemTypeCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`vehicle_item_types` member, load, `vehicle_item_types=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestVehicleItemTypesFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\VehicleItemTypes.INT` (UTF-16LE) — mirror of the cooked SDD
  table `VehicleItemTypes`. Single `[VehicleItemTypes]` section, 580 kv lines.
- One key per vehicle id: `VehicleItemTypes_<id>_Description=<player-facing text>`.
- 580 distinct ids; only **569 carry a non-empty description** — rows with an empty description are
  dropped (leaving the vehicles that actually render a blurb).
- Prose kept **VERBATIM** (no stray `\u` — \uXXXX-restore): apostrophes/&/quotes round-trip; U+21B5 ->
  real `\n`; C0 control stripped.

## Shape & the helpers
- 569 rows. `VehicleItemType{ id, description, order }`.
- `VehicleItemTypeCatalog` API: `Find / Description / HasDescription / ForCategory / ForClass / Count`,
  merge-by-id, order-sorted. Same private JSON helpers as the sibling catalogs (linear Find).
- **`static Category(id)`** = FIRST token (always `Vehicle` for vehicle ids). **`static Class(id)`** =
  SECOND token — the *discriminating* vehicle class. Observed distribution across the 569 rows:
  `Car` 338, `Truck` 153, `Van` 52, `Armas` 12, `Ambient` 6, `Rally` 6, plus 2 `Test` stubs. Use
  `ForClass` to group by vehicle class, `ForCategory` to grab the whole `Vehicle` family.

## Notes for other agents
- Pairs with `vehicles_catalog` (stats): when the Armas / vehicle-select UI shows a vehicle, resolve stats
  from `vehicles_catalog` and the blurb from `vehicle_item_types.Description(id)`. Do NOT hardcode vehicle
  descriptions.
- **Last sibling ItemTypes description table still to port (identical single-`_Description` schema — a clean
  one-increment job with this exact recipe):**
  - `ClothingItemTypes.INT` — 2058 kv, **1836** non-empty clothing descriptions (largest; core to APB
    character customization). Note `Class(id)` (second token) will be the clothing slot family
    (`M`/`F`/... gender/slot prefix) — profile the id tokens first the same way this increment did.
- **Dead-end still standing:** `InventoryItemPrices.INT` (5495 rows) is a stub — every
  `CustomAdditionalItem` value is empty; real prices live in the apbdb catalogs. Do not port it.
- **Reward PAYLOAD increment still pending (bigger step):** attach a `contents` list (item ids + counts +
  lease flags) to each reward/package id from cooked SDD / apbdb.com; description resolvers now include
  `inventory_item_types` + `unlock_item_types` + `weapon_item_types` + `vehicle_item_types`. Payload data
  is NOT in any INT.
