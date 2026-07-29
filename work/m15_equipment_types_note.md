# m15 — Equipment-type catalog (EquipmentTypes.INT) — handoff note

**Author:** Cline  **Status:** COMPLETE (test + note added; all suites FAILS=0)

## What landed
The equipment-type extractor (`extract_equipment_types.ps1`), JSON (`equipment_types.json`),
and catalog header (`APBEquipmentTypes.h`) were created by a prior agent. This increment adds the
missing test (`TestEquipmentTypesFromRetail`) and this note, verifying the data is correct.

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_equipment_types.ps1` |
| Data | `Content/Data/equipment_types.json` (60 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBEquipmentTypes.h` (header-only `EquipmentTypeCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`equipment_types` member, load, `equipment_types=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestEquipmentTypesFromRetail` (added by Cline) |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\EquipmentTypes.INT` (UTF-16LE) — the localized
  mirror of the cooked SDD table `EquipmentType`. Each equipment has a single `_Description`.
- **All descriptions are verbatim from the INT.** DNT rows dropped (`None`, `AmmoCarrier`);
  60 real rows remain. Field names `base`/`mk` in JSON match the catalog header exactly.

## Shape
- 60 rows. `EquipmentTypeDef{ id, description, base, mk, order }`.
- `EquipmentTypeCatalog` API: `Find/Description/ForBase/FindBase/Bases/BaseCount/IsUpgrade/Count`,
  merge-by-id, order-sorted.
- **15 equipment families**, each with 4 mark tiers (base mk=0, Mk2, Mk3, Mk4) = 60 rows.

## Equipment families (15)
Battering Ram (door breaching), Brass Knuckles (mugging), Camera (crime scene investigation),
Crowbar (door/window levering), Explosives (target destruction), Hacking (electronic breach),
Handcuff Keys (free arrested criminals), Handcuffs (arrest), Paint Sprayer (Enforcer propaganda),
Petrol Can (arson), Police Badge (intimidation), Sabotage (target destruction), Slim Jim
(vehicle entry), Spray Can (graffiti), Wire Cutters (defuse/sabotage).

## Tier progression pattern
Each family follows the same escalation pattern in descriptions:
- Mk1 (base): "A [tool] for [action]."
- Mk2: "...for [action] quickly."
- Mk3: "...for [action] very quickly."
- Mk4: "...for [action] as quickly as possible."

## Notes for other agents
- This is the authoritative source for **mission toolkit item tooltips**. The inventory/equipment
  system reads the equipment id and displays the description in the item tooltip.
- The `base`/`mk` split enables the progression UI: the player unlocks Mk1 at contact level X,
  then Mk2/3/4 at higher levels. The mission-tool effectiveness scales with the mk tier.
- Faction alignment: Brass Knuckles / Spray Can / Petrol Can / Slim Jim are Criminal tools;
  Handcuffs / Handcuff Keys / Police Badge / Camera are Enforcer tools; Battering Ram / Crowbar /
  Explosives / Sabotage / Wire Cutters / Hacking / Paint Sprayer are shared.
- Entitlement/unlock gating (which contact unlocks which mk tier) comes from the SDD cooked data
  (apbdb), not the INT mirror — that wiring is a follow-up increment.
