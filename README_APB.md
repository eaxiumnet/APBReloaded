# APBReloaded — UE 5.8 Multiplayer Foundation

Engine: `D:\UE58\UE_5.8` (EngineAssociation 5.8)

## Targets
- `APBReloaded` — game client
- `APBReloadedEditor` — editor
- `APBReloadedServer` — dedicated server

## Architecture
- **WorldService** (pure C++ `Domain/`) — lobby character create, district list/join, inventory, Armas, auction, threat, missions, combat resolve
- **UAPBGameInstanceSubsystem** — UE entry calling WorldService
- **AAPBWorldGameMode / AAPBDistrictGameMode** — world routing vs district freeroam host
- **AAPBPlayerState** — replicated faction, threat, currency
- Catalog: `Content/Data/*.json` seeded from https://apbdb.com/

## Run domain tests (shipped logic)
```
powershell -File D:\APBReloaded\tests\build_and_run.ps1
```

## Open in UE 5.8
```
D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe D:\APBReloaded\APBReloaded.uproject
```
