# APB Artifact Correlation

Current binary:

- `C:\Users\Support\Desktop\malware_runtime_artifacts\APB.exe`
- `C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\Binaries\APB.exe`
- SHA256: `238D41CF8D0B426EA676BF94F5E68CD38269FD4489614E3A17193B98C6BDE6A2`
- Image base used by prior artifacts: `0x140000000`

## Sources

- `C:\APB\APB_SDK\APB_ScanResults.txt`
- `C:\APB\APB_SDK_Output\APB\GameDefines.hpp`
- `C:\APB\APB_SDK\CodeRed-Generator-main\Engine\APB\GameDefines.hpp`
- `C:\APB\APB_SDK\APB_Config.h`

## Static Labels To Apply

| Label | VA | RVA | Source |
| --- | ---: | ---: | --- |
| `apb_artifact_scan_none_string` | `0x142A67438` | `0x02A67438` | `APB_ScanResults.txt` verification string |
| `apb_artifact_scan_gobjects_pattern` | `0x1419391F2` | `0x019391F2` | `APB_ScanResults.txt` GObjects pattern |
| `apb_artifact_scan_gobjects_global` | `0x143CF3CE0` | `0x03CF3CE0` | `APB_ScanResults.txt` GObjects global |
| `apb_artifact_scan_gnames_pattern` | `0x14026543D` | `0x0026543D` | `APB_ScanResults.txt` GNames pattern |
| `apb_artifact_scan_gnames_global` | `0x143C73B40` | `0x03C73B40` | `APB_ScanResults.txt` GNames global |
| `apb_artifact_codered_gobjects_global` | `0x143CFFC70` | `0x03CFFC70` | `APB_SDK_Output\APB\GameDefines.hpp` |
| `apb_artifact_codered_gnames_global` | `0x143C7FAD0` | `0x03C7FAD0` | `APB_SDK_Output\APB\GameDefines.hpp` |
| `apb_artifact_engine_apb_uworld_global` | `0x143D430E8` | `0x03D430E8` | `CodeRed-Generator-main\Engine\APB\GameDefines.hpp` |
| `apb_artifact_legacy_config_gobjects_global` | `0x14399EA20` | `0x0399EA20` | `APB_Config.h` legacy candidate |
| `apb_artifact_legacy_config_gnames_global` | `0x14391E8B0` | `0x0391E8B0` | `APB_Config.h` legacy candidate |
| `apb_artifact_legacy_config_uworld_global` | `0x143A14B98` | `0x03A14B98` | `APB_Config.h` legacy candidate |

## Structure Notes From Existing Artifacts

- `TArray<T>` layout in generated CodeRed output:
  - `ArrayData` at `+0x00`
  - `ArrayCount` at `+0x08`
  - `ArrayMax` at `+0x0C`
- `FNameEntry` layout in generated CodeRed output:
  - `Flags` at `+0x00`
  - `Index` at `+0x04`
  - `HashNext` at `+0x08`
  - `Name` at `+0x10`
- APB engine fallback offsets in `CodeRed-Generator-main\Engine\APB\GameDefines.hpp`:
  - `UObject_Name = 0x20`
  - `UObject_Class = 0x8`
  - `UObject_Outer = 0x18`

## Ghidra Script

Run `ghidra_scripts\ApplyApbArtifactLabels.java` against the APB program to apply labels, plate comments, and bookmarks. The script is static only and does not read a live process.

## Verification

Headless Ghidra 12.0.4 was run against a separate project:

- Project: `C:\Users\Support\Desktop\malware_runtime_artifacts\ghidra_projects\apb_static_annotated.gpr`
- Program: `/APB.exe`
- Result: analysis succeeded and all 11 artifact labels/bookmarks were applied.
