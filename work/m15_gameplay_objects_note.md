# m15 — Gameplay-object catalog (GameplayObjects.INT) — handoff note

**Author:** Cline  **Status:** COMPLETE (all suites FAILS=0)

## What landed
Extracted the retail APB gameplay-object label catalog into the Domain layer, following the
established data-catalog recipe (extractor .ps1 -> JSON -> header-only catalog -> WorldService
wiring -> test).

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_gameplay_objects.ps1` |
| Data | `Content/Data/gameplay_objects.json` (44 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBGameplayObjects.h` (header-only `GameplayObjectCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`gameplay_objects` member, load, `gameplay_objects=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestGameplayObjectsFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\GameplayObjects.INT` (UTF-16LE) — the localized
  mirror of the cooked SDD table `GameplayObject`. Each object has a single `_Description`.
- **All descriptions are verbatim from the INT.** Rows with empty or DNT descriptions dropped
  (None, MinigamePickup, WeaponPickup_Box/Dropped/Mission, Pedestrian_LivingCity_MugTarget);
  44 real rows remain from 56 INT lines.

## Shape
- 44 rows. `GameplayObjectDef{ id, description, category, order }`.
- `GameplayObjectCatalog` API: `Find/Description/Category/ForCategory/Categories/Count`,
  merge-by-id, order-sorted, plus `IsProp/IsVehicle/IsAmbientVehicle/IsPlayerCharacter`.
- Category = first id token: **Prop** (16), **Vehicle** (13), **TaskItem** (7),
  **DisplayPoint** (3), **PlayerCharacter** (2), **Checkpoint** (1), **Graffiti** (1),
  **Pedestrian** (1).

## Object families
- **Props** (16): Bench, Electrical Box, Fire Hydrant, Lamppost, MailBox, Market Stall,
  News Stand, Parking Meter, Payphone, Shop Front, Trash Can, Vending Machine, Miscellaneous Prop,
  plus 3 Halloween pumpkins (Orange/Purple/Red).
- **Vehicles** (13): 9 ambient (Armoured Van, Cheap/Luxury/Mid-Range Car, Cheap/Luxury/Mid-Range
  SUV, Taxi, Truck, Van), Misc Vehicle, Basic/Advanced Player Vehicle.
- **TaskItems** (7): Event, Open World Task Item, Cash Pool, Large/Medium/Small Open World, TV.
- **DisplayPoints** (3): Graffiti, Statue, Vehicle.
- **PlayerCharacters** (2): Criminal, Enforcer.
- **Pedestrian** (1): Living City pedestrian.

## Notes for other agents
- This is the authoritative source for **context-sensitive HUD interaction labels** — the text
  shown when the player's reticle hovers over or interacts with a world object. It complements
  `TaskTargetTypeCatalog` (mission-objective labels) and `EquipmentTypeCatalog` (tool tooltips).
- The gameplay-object id is referenced by the world-placement / collision system when an object
  is interactable; this catalog resolves it to the HUD label.
- Halloween pumpkins (Prop_Halloween_Pumpkin/Purple/Red) are seasonal event objects — the event
  system toggles their visibility; the labels are always present in the catalog.
- `IsAmbientVehicle()` distinguishes ambient AI traffic from player-spawned vehicles — useful for
  the HUD to show different interaction prompts (jack vs. enter owned).
