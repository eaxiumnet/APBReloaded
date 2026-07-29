# APBReloaded Project Knowledge Base

> This root file is the project-wide agent contract and the primary source of truth.
> Read it first. Scoped `AGENTS.md` files add directory-specific guidance and may not
> override this contract. Ignore stale `CLAUDE.md` or `.cursorrules` content beyond
> their pointer line.

**Generated:** 2026-07-22
**Baseline:** `58754f9` on `main` (working tree may contain later uncommitted work)
**Engine:** Unreal Engine 5.8 at `D:\UE58\UE_5.8`

## Overview

UE5.8 recreation of APB Reloaded using the retail installation as the behavioral and
content reference. The project is one runtime module with an engine-free C++ domain
layer, UE-facing replicated systems, JSON catalogs, standalone domain tests, and
extraction/import tooling.

Reference roots:

- Retail: `C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded`
  Sole reference for districts, buildings, meshes, gameplay, and catalogs.
- 2011: `D:\APBReloaded\2011 apb\APB All Points Bulletin`
  **Main menu / frontend only.** Not a reference for anything else.

## Agent Contract

1. Read this file, the nearest scoped `AGENTS.md`, and `work/_active.md` before work.
2. Plans live in `work/`; completed or abandoned plans move to `work/_archive/`.
3. Keep one concern per commit and preserve unrelated dirty-worktree changes.
4. Before completion, build affected UE targets and run `tests\build_and_run.ps1` when
   Domain code or catalog behavior is involved.
5. After more than two failed attempts, stop retrying and write a debugging note in
   `work/` with evidence and the remaining blocker.
6. Never edit `D:\UE58\UE_5.8`; engine source and installed-engine files are external.

## Instruction Hierarchy

| Scope | Purpose |
|---|---|
| `Source/APBReloaded/Domain/AGENTS.md` | Pure C++ services, persistence, catalogs |
| `Source/APBReloaded/Systems/AGENTS.md` | UE bridge, replication, frontend, district, server |
| `tests/AGENTS.md` | Standalone MSVC domain test harness |
| `Content/Data/AGENTS.md` | Authoritative JSON catalogs and generated data |
| `tools/AGENTS.md` | Verification, extraction, import, and server scripts |
| `work/AGENTS.md` | Plans, evidence, logs, archives, and debug notes |

## Structure

```text
APBReloaded/
|- APBReloaded.uproject
|- Source/APBReloaded/
|  |- Domain/                 # engine-free authoritative gameplay/backend rules
|  `- Systems/
|     |- Frontend/            # boot/login/character/district UMG flow
|     |- District/            # ticket validation, freeroam, streaming, vehicles
|     `- Server/              # world GameMode and TCP control
|- Content/
|  |- Data/                   # apbdb/reference-derived JSON catalogs
|  |- Imported/               # UE assets tracked by the import ledger
|  `- Extracted/              # reference output; never package as runtime content
|- Config/                    # maps, GameModes, networking, ports
|- tests/                     # standalone C++17 suites
|- tools/                     # gates, extraction/import, and server operations
`- work/                      # active plan and evidence
```

Large local reference/generated trees (`2011 apb/`, `APB Reloaded/`, `Binaries/`,
`Intermediate/`, `Saved/`, `DerivedDataCache/`, and `Content/Extracted/`) are not
runtime source and should not be broadly searched or edited without a task-specific reason.

## Where To Look

| Task | Location | Notes |
|---|---|---|
| Active roadmap and decisions | `work/_active.md`, `work/ARCHITECTURE.md` | Check before implementation |
| Module/target settings | `Source/APBReloaded/*.Build.cs`, `Source/*.Target.cs` | V7, Unreal5_8, `bUseUnity=false` |
| Login/world/domain behavior | `Source/APBReloaded/Domain/APBWorldService.*` | Main authoritative facade |
| UE/domain bridge | `Source/APBReloaded/Systems/APBGameInstanceSubsystem.*` | Mutation and snapshot boundary |
| Replicated player state | `Source/APBReloaded/Systems/APBPlayerState.*` | Faction, threat, currency, missions |
| World authority | `Source/APBReloaded/Systems/Server/APBWorldGameMode.*` | Per-player services and tickets |
| Playable districts | `Source/APBReloaded/Systems/District/` | GameModes, streaming, pawns, vehicles |
| Frontend flow | `Source/APBReloaded/Systems/Frontend/` | C++ UMG logic and boot flow |
| Maps/network defaults | `Config/DefaultEngine.ini`, `Config/DefaultGame.ini` | Prefix routing and ports |
| Asset provenance | `tools/import_ledger.json`, `work/IMPORT_STATUS.md` | Imported/extracted/manual state |

## Code Map

| Symbol | Role |
|---|---|
| `apb::WorldService` | Domain facade for auth, characters, districts, economy, missions, persistence |
| `UAPBGameInstanceSubsystem` | UE/Blueprint entry point and Domain-to-PlayerState sync bridge |
| `AAPBPlayerState` | Replicated client-visible authoritative state and validated RPC surface |
| `AAPBWorldGameMode` | World login, character/district lists, ticket issuance, per-player services |
| `AAPBDistrictGameMode` | District ticket validation and authenticated admission |
| `AAPBFreeroamGameMode` | District resolution, placement streaming, and freeroam runtime |
| `UAPBFrontendWidget` | Primary login/character/district UI flow |

## Project Constraints

- Target UE 5.8; verify APIs rather than assuming UE4 or UE5.0-5.7 behavior.
- Port behavior from source installations; do not copy binaries or assets blindly.
- The 2011 install is a reference for the main menu / frontend only. Everything else
  (districts, buildings, meshes, gameplay, catalogs) comes from retail. Never resolve
  a non-frontend gap by reaching into the 2011 build, and never treat its packages as
  a fallback when a retail asset looks missing or stripped.
- Server-authoritative logic belongs in `Domain/`; clients never mutate Domain state.
- Replicated faction, threat, currency, inventory, and mission state belongs in
  `AAPBPlayerState`; networked behavior uses GameMode/PlayerState/Subsystem/RPC paths.
- `Content/Data/*.json` is seeded from apbdb.com; use source evidence, not guessed values.
- C++ owns gameplay and UI behavior; Blueprints are cosmetic wiring only.
- Never poll around replication, trust client-supplied character data, or use stale
  hardcoded catalog IDs.

## Commands

```powershell
# Standalone domain tests
powershell -File D:\APBReloaded\tests\build_and_run.ps1

# UE targets supported by the installed engine
& D:\UE58\UE_5.8\Engine\Build\BatchFiles\Build.bat APBReloadedEditor Win64 Development -Project=D:\APBReloaded\APBReloaded.uproject -WaitMutex
& D:\UE58\UE_5.8\Engine\Build\BatchFiles\Build.bat APBReloaded Win64 Development -Project=D:\APBReloaded\APBReloaded.uproject -WaitMutex

# Visible client/editor launch
& D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe D:\APBReloaded\APBReloaded.uproject -game -windowed -log
```

The installed binary engine cannot build `TargetType.Server`. Keep the server target for
a future source-engine build; locally run the Game target with `-WorldServer -nullrhi
-nosound -unattended`. See `work/m6_server_target_limit.md`.
