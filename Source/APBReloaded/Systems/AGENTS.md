# Systems Scope

## Overview

UE5.8-facing runtime layer. It translates engine/UI requests into authoritative Domain
operations and exposes client-visible state through normal Unreal replication.

## Structure

```text
Systems/
|- APBGameInstanceSubsystem.*   # Domain lifetime, facade calls, snapshot sync
|- APBPlayerState.*             # replicated state and validated RPCs
|- APBSessionProbeSubsystem.*   # automated runtime probes
|- Frontend/                    # startup map, UMG login/character/district flow
|- District/                    # district auth, freeroam, streaming, pawns, vehicles
`- Server/                      # world GameMode and control channel
```

## Where To Look

| Concern | Location |
|---|---|
| Domain bridge and persistence lifetime | `APBGameInstanceSubsystem.*` |
| Replicated player data and RPC results | `APBPlayerState.*` |
| Boot/login/character/district UI | `Frontend/APBFrontendWidget.*` |
| Frontend startup and camera | `Frontend/APBFrontendGameMode.*` |
| District admission | `District/APBDistrictGameMode.*` |
| Freeroam runtime and map resolution | `District/APBFreeroamGameMode.*` |
| Placement streaming | `District/APBDistrictStreamer.*`, `APBDistrictPlacementLoader.*` |
| World login/tickets | `Server/APBWorldGameMode.*` |

## Conventions

- Check `HasAuthority()`, net mode, or the owning server GameMode before Domain mutation.
- Clients observe replicated `AAPBPlayerState`; they do not own parallel gameplay state.
- Call the established Domain-to-PlayerState snapshot sync after authoritative mutations.
- Use validated server RPCs for client requests. District login must validate issued
  tickets and never trust client-supplied character state.
- Keep C++ responsible for UI/game rules. Blueprint assets wire presentation and cosmetics.
- Use map-prefix routing and GameMode configuration from `Config/DefaultEngine.ini`.
- Keep the single runtime module and existing `Frontend`, `District`, and `Server` folder
  ownership until the active roadmap explicitly schedules a module split.

## Anti-Patterns

- Polling Domain state from widgets, pawns, or tick functions to imitate replication.
- Calling Domain services directly from Blueprints or duplicating Domain rules in UE code.
- Client-side account provisioning, economy, inventory, threat, or mission mutation.
- Hardcoded district IDs, map routes, catalog values, or server ports when config/catalog
  owners already exist.
- Using `/Engine/BasicShapes/Cube` or other placeholders as final imported district assets.

## Verification

Build `APBReloadedEditor` and `APBReloaded`. Run the task-specific probe under `tools/`
when changing frontend, multiplayer, district, or world-server behavior. The installed
engine cannot build the Server target; use the documented Game-target role flags.
