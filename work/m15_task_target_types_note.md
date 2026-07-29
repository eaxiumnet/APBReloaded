# m15 — Task-target-type catalog (TaskTargetTypes.INT) — handoff note

**Author:** Cline  **Status:** COMPLETE (all 17 domain suites FAILS=0)

## What landed
Extracted the retail APB mission-target-type catalog into the Domain layer, following the
established data-catalog recipe (extractor .ps1 -> JSON -> header-only catalog -> WorldService
wiring -> test).

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_task_target_types.ps1` |
| Data | `Content/Data/task_target_types.json` (118 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBTaskTargetTypes.h` (header-only `TaskTargetTypeCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`task_target_types` member, load, `task_target_types=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestTaskTargetTypesFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\TaskTargetTypes.INT` (UTF-16LE) — the localized
  mirror of the cooked SDD table `TaskTargetType`. Each target type has a single `_DisplayName`.
- **All display names are verbatim from the INT.** Rows with empty or DNT display names are
  dropped; 118 real rows remain from 127 INT lines (`A_None` has an empty name, 8 DNT rows).

## Shape
- 118 rows. `TaskTargetTypeDef{ id, display_name, order }`.
- `TaskTargetTypeCatalog` API: `Find/DisplayName/ByDisplayName/DistinctDisplayNames/Count`,
  merge-by-id, order-sorted, plus `IsNPCTarget()` and `IsCheckpoint()` sub-classification.
- Multiple ids may share one display name (e.g. 20+ Checkpoint variants all show "Checkpoint");
  the mission system resolves by id, not by display name.

## Target type families
- **Checkpoints** (20+ variants): race, territory control, dropoff (alley/hideout/lockup/shop),
  vehicle dropoff, epidemic checkpoints (T0/T1/T2 team variants), musical checkpoints.
- **Props**: ATM, bus shelter, electrical box, fire hydrant, mail box, newspaper box, news stand,
  park bench, parking meter, pay phone, trash can, vending machine, graffiti point, statue,
  market stall, fruit stall, portacabin, ticket machine, vending stand, air conditioning unit,
  garage doors, doors/doorways, commercial windows, crates, containers, invisible target area.
- **NPCs** (7): Pedestrian (5 variants: lower/standard/urban male/female), Drug Mule,
  Mr Bunny (Easter), Mr Chicken (Easter).
- **Vehicles**: Parked vehicle spawn, ambient vehicles, player vehicles.
- **Dropoffs**: Fence (criminal), Secure Lockup (enforcer).
- **Riot** (3): RIOT Device, RIOT Unit, checkpoint.
- **Seasonal**: Pumpkins (Halloween red/purple), virus barrels (epidemic event).
- **Special**: Weapon Test Checkpoint, Minigame item spawner.

## Notes for other agents
- This is the authoritative source for the **mission objective target labels** shown in the HUD
  when approaching a mission interactable ("Graffiti Point", "ATM", "Checkpoint", etc.). The
  mission stage data references target-type ids; this catalog resolves them to display names.
- The mapping from a mission stage's target to its target-type id lives in the cooked SDD mission
  tables (not the INT mirrors); that wiring is a follow-up increment.
- `IsNPCTarget()` identifies NPC-based targets (mugging, arrest) vs. prop-based ones (vandalism,
  item theft) — useful for the AI/interaction system to choose the right behavior.
- `ByDisplayName("Checkpoint")` returns all checkpoint variants — useful when the HUD just needs
  to show "Checkpoint" regardless of which specific variant the mission uses.
