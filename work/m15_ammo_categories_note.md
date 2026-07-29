# m15 — Ammunition-category catalog (AmmoCategories.INT) — handoff note

**Author:** Qoder  **Status:** COMPLETE (all 17 domain suites FAILS=0)

## What landed
Extracted the retail APB ammunition-category catalog into the Domain layer, following the
established data-catalog recipe (extractor .ps1 -> JSON -> header-only catalog -> WorldService
wiring -> test).

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_ammo_categories.ps1` |
| Data | `Content/Data/ammo_categories.json` (24 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBAmmoCategories.h` (header-only `AmmoCategoryCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`ammo_categories` member, load, `ammo_categories=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestAmmoCategoriesFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\AmmoCategories.INT` (UTF-16LE) — the localized
  mirror of the cooked SDD table `AmmoCategories`. Each category has four fields:
  `Name` / `NameAbbreviated` / `QuantityText` / `Description`.
- **All four strings are verbatim from the INT.** `QuantityText` contains the literal `<Num>`
  angle-bracket token (e.g. `"<Num> bullets"`); the extractor's \uXXXX-restore keeps the angle
  brackets readable in the JSON (no stray `\u`).
- 25 ids exist in the INT; the unused `Blowtorch_Fuel` id has an empty `Name` and is dropped,
  leaving 24 real rows. The `None` id (all fields "Not Currently Available") is the no-ammo
  sentinel and is kept verbatim.

## Shape
- 24 rows. `AmmoCategory{ id, name, name_abbreviated, quantity_text, description, order }`.
- `AmmoCategoryCatalog` API: `Find/Name/Abbreviated/QuantityText/Description/Count`,
  merge-by-id, order-sorted, plus **`FormatQuantity(id, count)`** which performs the live
  `<Num>` -> count substitution the HUD ammo counter does at runtime
  (e.g. `FormatQuantity("Rifle", 30)` -> `"30 rounds"`).

## Notes for other agents
- This is the authoritative source for the **HUD ammo counter** (abbreviated label + quantity
  text) and the inventory/mod-screen ammo names. Read from `WorldService.ammo_categories` and
  use `FormatQuantity` for the live counter — do not hardcode "<Num> bullets" strings.
- The mapping from a specific weapon -> its ammo-category id lives in the weapon data
  (WeaponItemTypes / weapons_catalog), NOT here; this catalog only supplies the per-category
  display text + counter template. Wiring weapon->ammo_category is a natural follow-up increment.
- Categories cover all retail ammo pools: pistol/rifle/machinegun/shotgun/sniper/rocket, the
  full grenade family (frag/concussion/EMP/percussion/stinger/eight-ball/brick/light-frag),
  LTL (less-than-lethal) primary/secondary/grenade, 40mm grenade rounds, and the seasonal
  eggs/snowball/flare novelty ammo.
- `ModifierEffects.INT` (508 lines, character/consumable stat modifiers with `<Color>` markup
  and multi-line `_2/_3` variants) is the next high-value gameplay catalog but needs a
  markup-aware parser; left for a dedicated increment.
