# Steam APB → UE 5.8 conversion spine

## Goal
Inventory and extract **priority** Steam content into `D:\APBReloaded` (not binary-copy of every UPK in one pass).

## Steam sources
| Class | Path |
|-------|------|
| UPK packages | `APBGame/Content/Release/Packages` (~6579) |
| Audio | `APBGame/Content/Audio` (SoundBanks, FilePackages, DefaultMusicLibrary=radio) |
| Config | `APBGame/Config`, `ScriptUserBuild` |
| Private server | `ApbPrivateServer` (C# Lobby/World/Character/File) |

## Project sinks
| Class | Path |
|-------|------|
| Meshes (umodel) | `Content/Extracted/UmodelExport` |
| Imported UE assets | `Content/Imported` |
| Audio WEM/WAV | `Content/Extracted/Audio` |
| Catalogs / placements | `Content/Data` (+ `district_placements`) |
| Opcode reuse | `Source/APBReloaded/Domain/APBPrivateServerOpcodes.h` |

## Commands
```powershell
# Inventory
python tools/convert/steam_inventory.py --scratch $env:SCRATCH --tag run1

# PrivateServer opcodes + report
python tools/convert/parse_privateserver.py --scratch $env:SCRATCH

# Tests (real Steam + on-disk data)
python tools/convert/test_convert_pipeline.py

# Audio map / dump (existing)
python tools/WwiseExtract/audio_world_map.py
python tools/WwiseExtract/dump_all_audio.py

# Models
python tools/model_viewer/view_models.py --headless
# umodel: tools/scripts/extract_with_umodel.ps1
```

## Runtime proof
- Frontend loads `LoginTheme_APB_ThemePreMaster` (StreamedSFX theme)
- Catalogs from `Content/Data` via `InitCatalogFromProjectData`
- Freeroam placements from `Content/Data/district_placements/*.json`
- Logs: `STEAM_DERIVED`, `PRIVATESERVER_MAP`
