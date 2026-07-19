# APBReloaded — Agent Context

> ## THIS IS THE SINGLE SOURCE OF TRUTH
> This file (`AGENTS.md` at the repo root) is the **only** agent-instruction file for
> the APBReloaded UE5.8 port. Read **this file first**, always. Do not rely on
> memorized API shapes or file layouts from the source games — verify against
> the actual project files below.

**Project**: Convert existing retail *APB Reloaded* (2011) into a new **Unreal Engine 5.8** port, built from the existing source material.

**Engine**: `D:\UE58\UE_5.8` (EngineAssociation 5.8)

**Local Paths**:
- This repo root: `D:\APBReloaded`
- Retail APB Reloaded (reference): `C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded`
- 2011 APB (reference): `D:\APBReloaded\2011 apb` → `APB All Points Bulletin`

---

## Agent Contract (mandatory for all AI tools)

1. **Read this file first.** Ignore any stale `CLAUDE.md` / `.cursorrules` content beyond their pointer line.
2. **Plans live in one place:** `work/`. Check `work/_active.md` before starting. One plan per effort; completed/abandoned move to `work/_archive/`.
3. **One concern per commit.** Never bundle unrelated changes.
4. **Before marking a task complete:** build the project (see below) and ensure no compile errors. Domain logic has unit tests under `tests/` — run `tests\build_and_run.ps1`.
5. **If a task loops more than 2 attempts, STOP.** Write a debugging note in `work/` instead of retrying. Looping is the failure mode this contract prevents.
6. **Never edit engine source** (`D:\UE58\UE_5.8`) — it is external. Only modify this repo.

---

## Hard Constraints

- Target is **UE 5.8**. Do not use deprecated 4.x APIs or assume 5.0–5.7 behavior without checking.
- Source material is reference only: retail APB Reloaded + 2011 APB. Port *behavior*, don't copy binaries/assets blindly.
- Replicated state (faction, threat, currency) belongs in `AAPBPlayerState` — keep server-authoritative logic in **WorldService** (pure C++ `Domain/`).
- Catalog data (`Content/Data/*.json`) is seeded from `https://apbdb.com/` — treat that as the authoritative data source, not guesswork.
- All multiplayer/networked code must go through the proper UE replication path (GameMode/PlayerState/Subsystem), not polling.

---

## Key Directories

```
APBReloaded/
├── APBReloaded.uproject        # UE5.8 project file
├── Source/
│   ├── Domain/                # pure C++ WorldService: lobby, char create,
│   │                          #   district list/join, inventory, Armas, auction,
│   │                          #   threat, missions, combat resolve
│   ├── APBGameInstanceSubsystem  # UE entry → calls WorldService
│   ├── APBWorldGameMode / APBDistrictGameMode  # world routing vs district freeroam
│   └── APBPlayerState        # replicated faction, threat, currency
├── Content/Data/*.json        # catalog, seeded from apbdb.com
├── Config/                    # UE config (DefaultEngine, DefaultGame)
├── tests/                    # shipped domain logic tests (build_and_run.ps1)
├── tools/                    # build/util scripts
└── work/                     # plans live here (_active.md, _archive/)
```

## Targets
- `APBReloaded` — game client
- `APBReloadedEditor` — editor
- `APBReloadedServer` — dedicated server

## Build & Verify
```
# Domain tests (shipped logic)
powershell -File D:\APBReloaded\tests\build_and_run.ps1

# Open in UE 5.8
D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe D:\APBReloaded\APBReloaded.uproject
```
