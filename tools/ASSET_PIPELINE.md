# APB 3D Asset Extraction Pipeline

## Tools installed
| Tool | Path |
|------|------|
| **UE Viewer (umodel)** | `D:\APBReloaded\Tools\UEViewer\umodel.exe` |
| **UEViewer source** | `D:\APBReloaded\Tools\UEViewer\` |
| **3ds Max ActorX import** | `D:\APBReloaded\Tools\3dsMax_ActorX\` (`ActorXImporter.ms`, `export_fbx.ms`) |
| **Inventory script** | `D:\APBReloaded\Tools\scripts\inventory_packages.py` |
| **Extract wrapper** | `D:\APBReloaded\Tools\scripts\extract_with_umodel.ps1` |

## Source packages (Steam install)
`C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Content\Release\Packages`
- **6579** `.upk` files (UE3, FileVersion **564**, Licensee **33**)
- Major folders: `APB_Vehicles` (~2649), `APB_CharacterTool` (~1753), `Character` (~356), districts, materials, VFX

## Inventory / reference outputs
| Artifact | Purpose |
|----------|---------|
| `Content\Extracted\package_inventory.csv` | Every UPK + string-mined mesh/name hints |
| `Content\Extracted\package_inventory_summary.json` | Counts + version histogram |
| `Content\Data\model_reference_catalog.json` | Vehicle/character package → UE5 `/Game/Imported/...` hints |

## Extract with umodel
```powershell
# List (when package opens)
.\Tools\scripts\extract_with_umodel.ps1 -Package V_A_2DrCoupe_Wheels_7 -ListOnly

# Export meshes (out under Content\Extracted\UmodelExport)
.\Tools\scripts\extract_with_umodel.ps1 -Package V_A_2DrCoupe_Wheels_7 -MeshesOnly

# Always pass game tag apb (built into wrapper)
# umodel -path="...\Packages" -game=apb -export -out=... PackageName
```

### UPK 564/33 unlock (done)
Rebuilt `umodel_64.exe` from `Tools\UEViewer` with APB Reloaded summary + property-tag fixes. Stock build 1590 failed header parse; fixed binary lists/exports cooked packages. See bottom of this file and `{scratch}/asset_report.md`.

## 3D DCC path (when export works)
1. umodel → **PSK/PSA** (skeletal) or static mesh + **TGA** textures  
2. **3ds Max**: run `Tools\3dsMax_ActorX\ActorXImporter.ms`, import PSK, then `export_fbx.ms`  
3. **Blender**: PSK plugins or FBX from Max  
4. **UE 5.8**: import FBX under `/Game/Imported/Vehicles|Characters/...` matching `model_reference_catalog.json` hints  
5. Bind runtime via `apb::ModelRegistry` (loaded from catalog JSON)

## Private use
Assets come from your local Steam install; keep extracts private / offline.

## Unlock status (2026-07-16)
APB cooked **564/33** packages **load and export** with rebuilt `Tools/UEViewer/umodel_64.exe` (also copied to `umodel.exe`).

### Code fixes in UEViewer
1. `UnPackage3.cpp` — after DependsOffset, read 4×int32 pad for `GAME_APB` licensee ≥ 33
2. `GameDatabase.cpp` — detect ArVer 564 / Licensee 33 as GAME_APB
3. `UnObject.cpp` — ByteProperty EnumName + BoolProperty byte for APB lic ≥ 33

### Build
Requires gildor BuildTools (`vc32tools` + `jom`) and VS 2019+. VS 2026 supported via vc32tools patch (version 18).

```powershell
# List
D:\APBReloaded\Tools\UEViewer\umodel_64.exe -path="...\Packages" -game=apb -list Contact_LaRocha
# Export
D:\APBReloaded\Tools\UEViewer\umodel_64.exe -path="...\Packages" -game=apb -export -out=D:\APBReloaded\Content\Extracted\UmodelExport Contact_LaRocha
```

Hero meshes converted to OBJ under `Content/Imported/...` for UE5 import.

## Live dump (APB.exe running)

Script: `tools/scripts/live_dump_apb.ps1`

Artifacts: `implementer/live_dump/` and `Content/Data/live_apb_reference/`

Notes:
- EAC/SecureEngine blocks procdump minidumps (error 0x800707D1) and module/handle enumeration.
- Useful live data comes from `APBGame/Logs/Current.log` + Config INIs, not PE memory.
- Login map streams UI districts: crimescene, login01, nightclub, skatepark, beachscene, districtselect.
- District abbreviations live: CS=Asylum, F=Financial, W=Waterfront, RS=RWorldSocial, ES=EnforcerSocial.
- Net: Port 7777, Peer 7778, tick rate 30 (social 20).
- Stats/lore: prefer `tools/apbdb/sync_apbdb.py` + UPK extract over process dumps.

## Additional tools & prebuilt assets available (2026-07-22)

Five archives were dropped at repo root and staged to `tools\_incoming\`. Verified capabilities below.

### Extraction tools
| Item | Staged path | Status |
|------|-------------|--------|
| **umodel_apb** (UE Viewer fork, built 2025-11-01) | `tools\_incoming\umodel_apb\umodel_64.exe` | **Redundant.** Byte-identical export to the repo's `tools\UEViewer\umodel_64.exe` on both retail **564/33** and 2011 **547/31** packages. The repo build (2026-07-16) is newer and already covers both. Keep only as a backup binary. |
| **NinjaRipper 1.7.1** (+ importers) | `tools\_incoming\ninjaripper\` | **Available — fallback only.** Live D3D6/7/8/9/11 draw-call ripper. x64/x86 `NinjaRipper.exe` + injector DLLs + Blender/3ds Max/Noesis `.rip` importers. |

**When to reach for NinjaRipper (not umodel):**
- Packages that crash umodel (rare, pre-2012 variants).
- **Runtime-composited character-customization textures** (symbols/decals) — these exist only on-screen, never as files on disk. This is its unique value.
- `.rip` format: sig `0xDEADBEEF` v4; per-draw-call fragment, view-space, **no skeleton hierarchy**, no material graph.
- Cost: heavy manual reassembly; **BattlEye ban risk** if ripping the live client (use offline/LAN).
- NOT for audio — APB uses Wwise `.bnk/.wem`; use existing `tools\WwiseExtract`.

### Prebuilt Blender assets (community reconstructions — rip+cleanup+assembly already done)
| Asset | Location | Blender ver | Contents |
|-------|----------|-------------|----------|
| **Eevee material library** | `tools\_incoming\eevee\APB_Eevee_Materials_Whiskey_V1.1.1.blend` (extracted) | 2.80 | APB shader graphs recreated for Eevee — reference for authoring matching UE5.8 materials |
| **Social District scene** | `APB_Scene_Social_District.zip` (root, **not extracted**, 472 MB blend) | 3.x/4.x | Full assembled Social District level geometry |
| **Vehicles pack** | `APB_Data_Vehicles.rar` (root, **not extracted**, 440 MB blend) | 3.x/4.x | All APB vehicles assembled |

**Blocker:** Blender is **not installed** on this machine (checked Program Files/x86/D:). The two large scene/vehicle blends (~1.9 GB uncompressed) are inert until a **Blender 4.x** install exists; left archived to save disk. Once installed:
1. Open blend → export **glTF/FBX** → import to UE5.8 under `/Game/Imported/...`.
2. This skips raw rip+reassembly for those assets — the main acceleration these uploads provide.

> **Private use:** these community blends and all extracts stay private / offline (same rule as §Private use above).

