# m15 — Modification-item catalog (ModifierItemTypes.INT) — handoff note

**Author:** Qoder  **Status:** COMPLETE (all 17 domain suites FAILS=0)

## What landed
Extracted the retail APB modification-ITEM catalog (the purchasable / equippable mod items you buy
on Armas and slot on a character / vehicle / weapon) into the Domain layer, following the
established data-catalog recipe. This is the direct follow-up flagged by the ModifierEffects
increment: it binds a mod **item** to its stat **effect** tooltip.

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_modifier_item_types.ps1` |
| Data | `Content/Data/modifier_item_types.json` (284 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBModifierItemTypes.h` (header-only `ModifierItemTypeCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`modifier_item_types` member, load, `modifier_item_types=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestModifierItemTypesFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\ModifierItemTypes.INT` (UTF-16LE) — mirror of the cooked
  SDD table `ModifierItemType`.
- One key per item: `ModifierItemTypes_<id>_Description=<TypeLabel> -<U+21B5><flavour description>`.
  The value is split on the U+21B5 line-break glyph: the part before is the **type label** (usually
  ending in ` -`, stripped), the part after is the **flavour description**. ~12 items are flavour
  only (no U+21B5, e.g. `FnMod_Weapon_ExtendedBarrel`), and a few have extra U+21B5 (multi-line
  flavour, collapsed to spaces). 5 empty-value rows (`None`, MobileSupplyUnit sub-keys, Minigame
  placeholders) are dropped -> **284 real rows**. `Mod_None`/`Mod_Vacant` (DNT placeholders) are
  kept as category `Special`.
- Text is verbatim (apostrophes/punctuation preserved, no stray `\u` — \uXXXX-restore).
- `category` = id's first token after stripping the mod prefix: **Character** (53), **Vehicle**
  (69), **Weapon** (160), **Special** (2).

## Shape & the effect binding
- 284 rows. `ModifierItemType{ id, category, type_label, description, order }`.
- `ModifierItemTypeCatalog` API: `Find / TypeLabel / Description / Category / ForCategory /
  Categories / Count`, merge-by-id, order-sorted.
- **Binding to the stat effect:** `static EffectId(id)` strips the `FnMod_`/`FNMod_` prefix and a
  trailing `_Tutorial`, giving the `modifier_effects.json` id (e.g. `FnMod_Vehicle_Explosives1` ->
  `Vehicle_Explosives1`). `EffectIdFor(id)` does the same for a stored item. 138 of 284 items bind
  directly to a `modifier_effects` row; the rest are placeholders (`Mod_None`/`Mod_Vacant`),
  deployable sub-effects (`..._Radius`/`_Deploy`/`_Health`), TestMods, or renamed variants
  (item `FnMod_Character_AirControl` vs effect `Character_JumpControl`) with no 1:1 effect row.
  Callers should `Find()` the derived id and tolerate a miss.

## Notes for other agents
- This + `modifier_effects` together drive the **modification screen / item inspector**: read the
  item's `type_label` + `description` from `WorldService.modifier_item_types` for the item card, and
  render the coloured stat lines from `WorldService.modifier_effects.ParseSegments(EffectId(...))`.
  Do not hardcode either string set.
- Remaining reconciliation (not blocking): for the ~146 items whose `EffectId()` does not resolve,
  build an explicit item-id -> effect-id alias table when the mod UI is wired (the divergent names
  are a small, enumerable set). The catalog is ready for that alias map to sit on top.
- Type labels seen: Health / Utility / Activated / Chassis / Engine / Generic / Trunk / Barrel /
  Magazine / Receiver / Upper Rail / Tuning / General / Event / Adaptation Modification (weapon
  mods use the barrel/magazine/receiver/rail family).
