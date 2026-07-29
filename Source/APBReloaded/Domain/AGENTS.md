# Domain Scope

## Overview

Engine-free C++17 gameplay and backend rules. `apb::WorldService` is the authoritative
facade consumed by UE systems and standalone tests.

## Where To Look

| Concern | Location |
|---|---|
| Auth, characters, world/district entry | `APBWorldService.*` |
| Catalog parsing and lookup | `APBCatalog.*`, catalog-specific headers |
| JSON persistence | `APBPersistence.*` |
| Inventory/economy/auction/Armas | `APBInventory.*`, `APBAuction.*`, related services |
| Missions/threat/combat/progression | `APBMission*`, `APBThreat*`, `APBCombat*`, `APBProgression*` |
| Social/chat/group/clan/mail | `APBSocial*`, `APBChat*`, `APBGroup*`, `APBClan*` |
| District directory/relay/matchmaking | `APBDistrictDirectory.*`, `APBWorldRelay.*`, `APBMatchmaking.*` |

## Conventions

- Use standard C++17 and namespace `apb`; no UE headers, reflection macros, containers,
  strings, paths, or logging types.
- Keep `WorldService` as the facade. Focused services own their rules; UE code does not
  reach through the facade into service internals.
- Mutation is server-owned. Return explicit result/status values for boundary failures.
- Persistence is opt-in; failure or absence must preserve in-memory behavior.
- Use caller-supplied clocks/IDs where determinism matters. Avoid hidden wall-clock state.
- Catalog-driven behavior resolves stable IDs from `Content/Data`; do not embed guessed
  catalog values or depend on stale numeric IDs.
- After a successful Domain mutation, the UE owner must use the single snapshot bridge
  to synchronize `AAPBPlayerState`.

## Anti-Patterns

- Adding `CoreMinimal.h`, UObject types, Blueprint APIs, or UE filesystem access here.
- Allowing clients or UI code to mutate services directly.
- Parsing a structured catalog with ad hoc substring checks when an existing parser or
  catalog type owns the schema.
- Clearing state on an empty additive update unless the API explicitly defines replace.
- Making persistence mandatory for previously valid in-memory flows.

## Verification

Add or extend the matching `tests/run_*_tests.cpp` suite and update
`tests/build_and_run.ps1` when a new `.cpp` dependency is introduced. Run the full harness,
then build affected UE Game/Editor targets because Domain is compiled both standalone and
inside the runtime module.
