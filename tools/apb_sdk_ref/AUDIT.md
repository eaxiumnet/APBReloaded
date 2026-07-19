# Old project audit (2026-07-16)

## Trees checked
| Path | Size | Nature |
|------|------|--------|
| Desktop\apbsdk | ~0.18 GB | UE3 SDK gen + 2020 APBGame SDK + GNames/GObjects dumps + KsDumper |
| Desktop\malware_runtime_artifacts | ~7.2 GB | GFAC/cheat malware RE, VAD dumps, invalid GObject hunt, eaxhook payloads |
| C:\APB | ~1.5 GB | ESP/overlay/SDK dumpers, larger names dump, drivers (EchoDriv/KsDumper) |

## Useful for D:\APBReloaded freeroam recreation
YES (limited, offline reference only):
- Name vocabulary for systems (Clothing/Customisation, Threat, Mission, Vehicle, District, Armas)
- 2020 APBGame.h **class index** for domain modeling (AcMission, cCustomisationReplicator, AcAPBVehicleBase, etc.)
- Confirmation that live GObjects/GNames offsets are build-volatile and blocked by GFAC (matches live_dump conclusions)

## Not useful / do not import
- Kernel drivers (KsDumperDriver.sys, ALSysIO, EchoDriv, etc.) — do not load
- ESP/overlay/injection sources and binaries
- malware_runtime_artifacts clean_reversal_package (cheat payloads, .sys, 5GB vol dumps)
- Runtime offsets as authoritative for current Steam build (report: all legacy addresses INVALID)
- Full 40MB Objects dumps (noise; umodel+apbdb+placements already cover assets)

## Already superseded by project data
- weapons/vehicles/threat/contacts JSON from apbdb
- District mesh binds + road imports
- Domain snapshot + MP OnRep + vehicle Domain spawn

## Residual freeroam not unlocked by these trees
Modular clothing still needs UPK mesh part mapping (umodel), not ESP SDK offsets.
Dedicated server optional — not in these trees as a shippable server.

## Copied here (safe text only)
- names_freeroam_subset.txt
- apbgame_h_class_index_2020.txt
- customisation_classes_2020.txt
- enums_sample_2020.txt
- SOURCE_*.md notes
